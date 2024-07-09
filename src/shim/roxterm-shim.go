package main

import (
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"sync"
	"time"
)

const (
	BufferSize            = 16384
	BufferMinChunkSize    = 256
	MaxBufferSizeForTopUp = BufferSize - BufferMinChunkSize
	MaxChunkQueueLength   = 16
	Osc52SizeLimit        = 1024 * 1024

	EscCode = 0x1b
	OscCode = 0x9d
	OscEsc  = ']'
	StCode  = 0x9c
	StEsc   = '\\'
	BelCode = 7
)

const (
	// Copying from input to output, but checking for start of control sequence
	// of interest.
	Filtering = iota

	// Got the start of a control sequence, but we don't know whether it's of
	// interest yet.
	PotentialMatch

	// Collecting data in an OSC52 write sequence.
	CollectingOsc52

	// Sequence was of interest, but it's too long, so the rest of it will be
	// discarded until we get a terminator.
	Discarding

	// Copying until we get to an Esc terminator.
	CopyingIgnoredEsc
)

// ShimBuffer holds data as it's read from the input until it's ready to be
// sent downstream.
type ShimBuffer struct {
	buf     []byte
	nFilled int // Number of bytes stored in the buffer
}

// NewShimBuffer allocates an empty ShimBuffer
func NewShimBuffer() *ShimBuffer {
	return &ShimBuffer{
		buf: make([]byte, BufferSize),
	}
}

// NextFillableSlice gets a slice from the ShimBuffer that can be written to.
// If the buffer is near capacity it returns nil and you should allocate a new
// ShimBuffer.
func (buf *ShimBuffer) NextFillableSlice() []byte {
	if buf.nFilled > MaxBufferSizeForTopUp {
		return nil
	} else {
		return buf.buf[buf.nFilled:]
	}
}

// Filled marks that the slice has been filled with nFilled bytes. Call this
// as soon as any data has been saved to the slice, don't wait until the slice
// is full.
func (buf *ShimBuffer) Filled(nFilled int) {
	buf.nFilled += nFilled
}

func (buf *ShimBuffer) IsEmpty() bool {
	return buf.nFilled == 0
}

func composeOsc52Log(chunks [][]byte) string {
	var s []byte
	for _, chunk := range chunks {
		s = append(s, chunk...)
	}
	return string(s)
}

type StreamProcessor struct {
	input                io.Reader
	output               io.Writer
	state                int
	unprocessedChunks    chan []byte
	processedChunks      chan []byte
	osc52Chan            chan [][]byte
	wg                   *sync.WaitGroup
	name                 string
	escStart             []byte
	escEnd               bool
	collectedOsc52Chunks [][]byte
	collectedOsc52Size   int
}

func NewStreamProcessor(input io.Reader, output io.Writer, wg *sync.WaitGroup,
	osc52Chan chan [][]byte, name string,
) *StreamProcessor {
	sp := &StreamProcessor{
		input:             input,
		output:            output,
		state:             Filtering,
		unprocessedChunks: make(chan []byte, MaxChunkQueueLength),
		processedChunks:   make(chan []byte, MaxChunkQueueLength),
		osc52Chan:         osc52Chan,
		wg:                wg,
		name:              name,
	}
	return sp
}

func (sp *StreamProcessor) closeUnprocessedChunksChan() {
	ch := sp.unprocessedChunks
	sp.unprocessedChunks = nil
	if ch != nil {
		close(ch)
	}
}

func (sp *StreamProcessor) closeProcessedChunksChan() {
	ch := sp.processedChunks
	sp.processedChunks = nil
	if ch != nil {
		close(ch)
	}
}

// inputReaderThread reads data from the input stream and feeds it to the
// unprocessed chunks queue.
func (sp *StreamProcessor) inputReaderThread() {
	running := true
	for running {
		buf := NewShimBuffer()
		if buf == nil {
			log.Printf("%s CPT: NewShimBuffer returned nil", sp.name)
			running = false
			break
		}
		for {
			chunk := buf.NextFillableSlice()
			if chunk == nil {
				break
			}
			reader := sp.input
			if reader == nil {
				running = false
				break
			}
			nFilled, err := reader.Read(chunk)
			if err != nil {
				log.Printf("%s IRT: Error reading stream: %v", sp.name, err)
				sp.closeUnprocessedChunksChan()
				running = false
				break
			}
			if nFilled == 0 {
				sp.closeUnprocessedChunksChan()
				running = false
				break
			}
			buf.Filled(nFilled)
			chunk = chunk[:nFilled]
			log.Printf("%s IRT: Read chunk from stdin: '%s'", sp.name, chunk)
			ch := sp.unprocessedChunks
			if ch != nil {
				ch <- chunk
			} else {
				running = false
				break
			}
		}
	}
	log.Printf("%s IRT: Leaving thread", sp.name)
	sp.wg.Done()
}

// chunkProcessorThread reads from the unprocessed chunks queue, and passes
// the chunks on to the processed chunk queue.
func (sp *StreamProcessor) chunkProcessorThread() {
	chin := sp.unprocessedChunks
	var chout chan []byte
	if chin == nil {
		log.Printf("%s CPT: Input nil, closing output", sp.name)
		sp.closeProcessedChunksChan()
	} else if chout = sp.processedChunks; chout == nil {
		log.Printf("%s CPT: Output nil, closing input", sp.name)
		sp.closeUnprocessedChunksChan()
	} else {
		ok := true
		for ok {
			chunk := <-chin
			if chunk == nil {
				log.Printf(
					"%s CPT: channel closed (nil chunk), closing output",
					sp.name)
				ok = false
				break
			}
			switch sp.state {
			case Filtering:
				ok = sp.processChunkInFilteringState(chunk)
			case PotentialMatch:
				ok = sp.processChunkInPotentialMatchState(chunk)
			case Discarding:
				ok = sp.processChunkUntilST(chunk)
			case CopyingIgnoredEsc:
				ok = sp.processChunkUntilST(chunk)
			case CollectingOsc52:
				ok = sp.processChunkUntilST(chunk)
			default:
				log.Printf("%s CPT: Invalid state %d", sp.name, sp.state)
				ok = false
			}
		}
		sp.closeProcessedChunksChan()
	}
	log.Printf("%s CPT: Leaving thread", sp.name)
	sp.wg.Done()
}

func (sp *StreamProcessor) processChunkInFilteringState(chunk []byte,
) bool {
	for i, c := range chunk {
		if c == EscCode || c == OscCode {
			log.Printf("%s processor got potential OSC 52 start %d", sp.name, c)
			sp.escStart = []byte{c}
			if i != 0 {
				sp.processedChunks <- chunk[i:]
			}
			remainder := chunk[i+1:]
			sp.state = PotentialMatch
			if len(remainder) != 0 {
				return sp.processChunkInPotentialMatchState(remainder)
			} else {
				return true
			}
		}
	}
	sp.processedChunks <- chunk
	return true
}

func (sp *StreamProcessor) processChunkInPotentialMatchState(chunk []byte,
) bool {
	for i, c := range chunk {
		ess := len(sp.escStart)
		var expect byte
		switch ess {
		case 1:
			if sp.escStart[0] == OscCode {
				log.Printf("%s just had 0x9d, expecting 5", sp.name)
				expect = '5'
			} else if sp.escStart[0] == EscCode && c == OscEsc {
				log.Printf("%s got ESC-] ...", sp.name)
				sp.escStart = append(sp.escStart, c)
				continue
			} else {
				log.Printf("%s: Unexpected character %d after ESC",
					sp.name, c)
				return sp.processRestOfChunkInCopyingEscState(chunk, i)
			}
		case 2:
			if sp.escStart[1] == OscEsc {
				log.Printf("%s just had ESC-], expecting 5", sp.name)
				expect = '5'
			} else {
				sp.escStart = append(sp.escStart, c)
				log.Printf("%s ESC sequence started with %d %d, then %d",
					sp.name, sp.escStart[0], sp.escStart[1], c)
			}
		default:
			switch sp.escStart[ess-1] {
			case '5':
				log.Printf("%s: had 5, expecting 2", sp.name)
				expect = '2'
			case '2':
				log.Printf("%s: had 2, expecting ;", sp.name)
				expect = ';'
			case ';':
				if ess < 6 {
					log.Printf("%s: had ;, expecting c or p", sp.name)
					expect = 'c'
				} else {
					log.Printf("%s: had ;, expecting (not) ?", sp.name)
					expect = '?'
				}
			case 'c', 'p':
				log.Printf("%s: had c/p, expecting ;", sp.name)
				expect = ';'
			}
		}
		if expect != 0 {
			if expect == '?' && c != expect {
				// If expect is '?', it means we want anything except '?',
				// because '?' means the child wants to read the clipboard
				// which we don't allow.
				log.Printf("%s: found start of OSC 52 write", sp.name)
				sp.state = CollectingOsc52
				sp.escEnd = false
				// Include the c/p and the semicolon after
				sp.escStart = sp.escStart[len(sp.escStart)-2:]
				sp.collectedOsc52Chunks = [][]byte{sp.escStart}
				sp.collectedOsc52Size = 0
				remainder := chunk[i:]
				return sp.processChunkUntilST(remainder)
			} else if c == expect || (expect == 'c' && c == 'p') {
				log.Printf("%s got expected char %c", sp.name, c)
				sp.escStart = append(sp.escStart, c)
				continue
			}
			// Again, some OSC other than 52
			return sp.processRestOfChunkInCopyingEscState(chunk, i)
		}
	}
	sp.processedChunks <- chunk
	return true
}

func (sp *StreamProcessor) resetEscState() {
	sp.escStart = nil
	sp.escEnd = false
}

func (sp *StreamProcessor) processChunkUntilST(chunk []byte,
) bool {
	for i, c := range chunk {
		gotTerm := false
		badEsc := false
		if sp.escEnd {
			if c == StEsc {
				gotTerm = true
			} else {
				badEsc = true
			}
		}
		if c == EscCode {
			sp.escEnd = true
			continue
		} else if c == StCode || c == BelCode {
			gotTerm = true
		}
		if gotTerm {
			switch sp.state {
			case CollectingOsc52:
				boundary := i
				if sp.escEnd {
					boundary -= 1
				}
				sp.collectedOsc52Chunks = append(sp.collectedOsc52Chunks,
					chunk[:boundary])
				sp.collectedOsc52Size += i
				log.Printf("%s: Sending '%s' to osc52Chan", sp.name,
					composeOsc52Log(sp.collectedOsc52Chunks))
				sp.osc52Chan <- sp.collectedOsc52Chunks
				sp.collectedOsc52Chunks = nil
				sp.collectedOsc52Size = 0
			case CopyingIgnoredEsc:
				sp.processedChunks <- chunk[:i+1]
			}
			sp.resetEscState()
			sp.state = Filtering
			return sp.processChunkInFilteringState(chunk[i+1:])
		}
		if badEsc {
			log.Printf("%s: Bad ESC terminator %d in state %d",
				sp.name, c, sp.state)
			switch sp.state {
			case CollectingOsc52:
				sp.collectedOsc52Chunks = nil
				sp.collectedOsc52Size = 0
			case CopyingIgnoredEsc:
				sp.processedChunks <- chunk[:i+1]
			}
			sp.resetEscState()
			sp.state = Filtering
			return sp.processChunkInFilteringState(chunk[:i+1])
		}
	}
	// End of chunk with no terminator
	switch sp.state {
	case CollectingOsc52:
		sp.collectedOsc52Size += len(chunk)
		if sp.collectedOsc52Size < Osc52SizeLimit {
			sp.collectedOsc52Chunks = append(sp.collectedOsc52Chunks, chunk)
		} else {
			sp.collectedOsc52Chunks = nil
			sp.collectedOsc52Size = 0
			log.Printf("%s: OSC 52 sequence too long", sp.name)
			sp.state = Discarding
			sp.resetEscState()
		}
	case CopyingIgnoredEsc:
		sp.processedChunks <- chunk
	}
	return true
}

func (sp *StreamProcessor) processRestOfChunkInCopyingEscState(chunk []byte,
	offset int,
) bool {
	log.Printf("%s: passing through remainder of Esc sequence", sp.name)
	sp.processedChunks <- sp.escStart
	remainder := chunk[:offset]
	sp.resetEscState()
	sp.state = CopyingIgnoredEsc
	return sp.processChunkUntilST(remainder)
}

// chunkWriterThread reads from the processed chunks queue, and writes them to
// the output stream.
func (sp *StreamProcessor) chunkWriterThread() {
	ch := sp.processedChunks
	for ch != nil {
		chunk := <-ch
		if chunk == nil {
			log.Printf("%s CWT: channel closed (nil chunk)", sp.name)
			break
		}
		log.Printf("%s CWT: Read chunk '%s'", sp.name, chunk)
		// Write is guaranteed to write the entire slice or return an error
		_, err := sp.output.Write(chunk)
		if err != nil {
			log.Printf("%s CWT: Error writing stream: %v", sp.name, err)
			break
		}
	}
	sp.wg.Done()
	log.Printf("%s CWT: Leaving thread", sp.name)
}

// osc52Thread reads data from osc52Chan and sends it to osc52Pipe. Each chunk
// of data has its length prepended, encoded as uint32 little-endian.
func osc52Thread(osc52Chan chan [][]byte, osc52Pipe io.Writer,
	wg *sync.WaitGroup,
) {
	dataLen := make([]byte, 4)
	var err error = nil
	for err == nil {
		chunks := <-osc52Chan
		if chunks == nil {
			log.Println("OFT: channel closed (nil chunk)")
			break
		}
		var l = 0
		for _, chunk := range chunks {
			l += len(chunk)
		}
		dataLen[0] = byte(l & 0xff)
		dataLen[1] = byte((l >> 8) & 0xff)
		dataLen[2] = byte((l >> 16) & 0xff)
		dataLen[3] = byte((l >> 24) & 0xff)
		log.Printf("OFT: Sending %d bytes in total", l)
		log.Printf("OFT: Sending '%s'", composeOsc52Log(chunks))
		_, err = osc52Pipe.Write(dataLen)
		if err != nil {
			break
		}
		for _, chunk := range chunks {
			_, err = osc52Pipe.Write(chunk)
			if err != nil {
				break
			}
		}
	}
	log.Printf("OFT: Leaving thread, error %v", err)
	wg.Done()
}

// Start starts all of a StreamProcessor's goroutines.
func (sp *StreamProcessor) Start() {
	log.Printf("Starting %s processor", sp.name)
	sp.wg.Add(3)
	go sp.inputReaderThread()
	go sp.chunkProcessorThread()
	go sp.chunkWriterThread()
}

func reportErrorOnStderr(message string) {
	os.Stderr.WriteString(message)
	os.Stderr.WriteString("\n")
}

func openPipe(name string) (r *os.File, w *os.File, err error) {
	r, w, err = os.Pipe()
	if err != nil {
		err = fmt.Errorf("unable to open pipe for %s: %v", name, err)
	}
	return
}

func sendStringToOsc52Chan(ch chan [][]byte, s string) {
	ch <- [][]byte{[]byte(s)}
}

func runStreamProcessor(r io.Reader, w io.Writer,
	osc52Chan chan [][]byte, wg *sync.WaitGroup, name string,
) *StreamProcessor {
	sp := NewStreamProcessor(r, w, wg, osc52Chan, name)
	sp.Start()
	return sp
}

func run() (wg *sync.WaitGroup, err error) {
	stdinReader := os.NewFile(0, "stdin")
	stdoutWriter := os.NewFile(0, "stdout")
	stderrReader, stderrWriter, err := openPipe("stderr")
	if err != nil {
		return
	}

	cmd := exec.Cmd{
		Path:   os.Args[2],
		Args:   os.Args[3:],
		Stdin:  stdinReader,
		Stdout: stdoutWriter,
		Stderr: stderrWriter,
	}
	err = cmd.Start()
	if err != nil {
		err = fmt.Errorf("failed to run shell/command: %v", err)
		return
	} else {
		log.Printf("Started command %s %s", os.Args[2],
			strings.Join(os.Args[3:], " "))
	}

	wg = &sync.WaitGroup{}
	pipeNum, _ := strconv.Atoi(os.Args[1])
	osc52Pipe := os.NewFile(uintptr(pipeNum), "OSC52Pipe")
	osc52Chan := make(chan [][]byte, 2)
	wg.Add(1)
	go osc52Thread(osc52Chan, osc52Pipe, wg)

	// Use a separate WaitGroup for the stream processor so we can wait for
	// it to finish before closing osc52Chan
	spWg := &sync.WaitGroup{}
	spErr := runStreamProcessor(stderrReader, os.Stderr, osc52Chan, spWg,
		"stderr")

	go func() {
		log.Println("Waiting for child command to exit")
		err := cmd.Wait()
		exitCode := cmd.ProcessState.ExitCode()
		log.Printf("Child exited with code %d error %s", exitCode, err.Error())
		spErr.closeUnprocessedChunksChan()

		// Make sure we exit after a few seconds in case the pipes are blocked.
		waiter := make(chan bool)
		go func() {
			log.Println("Waiting for stream processor to finish")
			spWg.Wait()
			log.Println("Sending END to roxterm over back-channel")
			sendStringToOsc52Chan(osc52Chan, "END")
			close(osc52Chan)
			log.Println("Waiting for back-channel thread")
			wg.Wait()
			close(waiter)
		}()
		select {
		case <-waiter:
		case <-time.After(time.Second * 5):
			log.Println("Shutdown timed out")
			wg.Done()
		}
		log.Println("Shutdown cleanly")
		os.Exit(exitCode)
	}()

	ok := fmt.Sprintf("OK %d", cmd.Process.Pid)
	log.Printf("Sending pid message '%s' to roxterm back-channel", ok)
	sendStringToOsc52Chan(osc52Chan, ok)

	return
}

func main() {
	cacheDir, _ := os.UserCacheDir()
	cacheDir += "/roxterm"
	os.MkdirAll(cacheDir, 0755)
	timeStamp := time.Now().UTC().Format(time.RFC3339)
	timeStamp = strings.Replace(timeStamp, ":", "-", -1)
	logName := fmt.Sprintf("%s/goshim-%s-%d.log",
		cacheDir, timeStamp, os.Getpid())
	logOutput, err := os.Create(logName)
	if err == nil {
		log.SetOutput(logOutput)
	}
	wg, err := run()
	if err != nil {
		reportErrorOnStderr(err.Error())
	}
	if wg != nil {
		wg.Wait()
	}
}
