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

	EscCode = 0x1b
	OscCode = 0x9d
	OscEsc  = ']'
	StCode  = 0x9c
	StEsc   = '\\'
	BelCode = 7
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

type StreamProcessor struct {
	input             io.Reader
	output            io.Writer
	unprocessedChunks chan []byte
	processedChunks   chan []byte
	osc52Chan         chan []byte
	wg                *sync.WaitGroup
	name              string
}

func NewStreamProcessor(input io.Reader, output io.Writer, wg *sync.WaitGroup,
	osc52Chan chan []byte, name string,
) *StreamProcessor {
	sp := &StreamProcessor{
		input:             input,
		output:            output,
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
			log.Printf("%s IRT: NewShimBuffer returned nil", sp.name)
			running = false
			break
		}
		for {
			chunk := buf.NextFillableSlice()
			if chunk == nil {
				log.Printf("%s IRT: Buffer was full", sp.name)
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
			ch := sp.unprocessedChunks
			if ch != nil {
				log.Printf("%s IRT: Read chunk '%s'", sp.name, chunk)
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
// Later there will be two versions, one to filter out OSC 52 and send clipboard
// write requests to the parent out of band, another to intercept the parent's
// response to XTGETTCAP to indicate OSC 52 is supported.
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
		for {
			chunk := <-chin
			if chunk == nil {
				log.Printf(
					"%s CPT: channel closed (nil chunk), closing output",
					sp.name)
				sp.closeProcessedChunksChan()
				break
			}
			log.Printf("%s CPT: Processing '%s'", sp.name, chunk)
			chout <- chunk
		}
	}
	log.Printf("%s CPT: Leaving thread", sp.name)
	sp.wg.Done()
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
			// TODO: Can't realistically log this unless we use a file
			log.Printf("%s CWT: Error writing stream: %v", sp.name, err)
			break
		}
	}
	sp.wg.Done()
	log.Printf("%s CWT: Leaving thread", sp.name)
}

// osc52Thread reads data from osc52Chan and sends it to osc52Pipe. Each chunk
// of data has its length prepended, encoded as uint32 little-endian.
func osc52Thread(osc52Chan chan []byte, osc52Pipe io.Writer, wg *sync.WaitGroup,
) {
	dataLen := make([]byte, 4)
	var err error = nil
	for err == nil {
		chunk := <-osc52Chan
		if chunk == nil {
			log.Println("OFT: channel closed (nil chunk)")
			break
		}
		l := len(chunk)
		dataLen[0] = byte(l & 0xff)
		dataLen[1] = byte((l >> 8) & 0xff)
		dataLen[2] = byte((l >> 16) & 0xff)
		dataLen[3] = byte((l >> 24) & 0xff)
		log.Printf("OFT: Sending '%s' (len %d)", chunk, l)
		_, err := osc52Pipe.Write(dataLen)
		if err == nil {
			_, err = osc52Pipe.Write(chunk)
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

func runStreamProcessor(r io.Reader, w io.Writer,
	osc52Chan chan []byte, wg *sync.WaitGroup, name string,
) *StreamProcessor {
	sp := NewStreamProcessor(r, w, wg, osc52Chan, name)
	sp.Start()
	return sp
}

func run() (wg *sync.WaitGroup, err error) {
	var stdinReader, stdoutReader, stderrReader io.Reader
	var stdinWriter, stdoutWriter, stderrWriter io.Writer
	stdinReader, stdinWriter, err =
		openPipe("stdin")
	if err != nil {
		return
	}
	stdoutReader, stdoutWriter, err =
		openPipe("stdout")
	if err != nil {
		return
	}
	stderrReader, stderrWriter, err =
		openPipe("stdout")
	if err != nil {
		return
	}

	path := "/home/tony/bin/ptywrap"
	args := append([]string{path, path}, os.Args[2:]...)
	joinedArgs := strings.Join(args, " ")
	log.Printf("Starting command %s %s", path, joinedArgs)
	cmd := exec.Cmd{
		Path:   path,
		Args:   args,
		Stdin:  stdinReader,
		Stdout: stdoutWriter,
		Stderr: stderrWriter,
	}
	err = cmd.Start()
	if err != nil {
		err = fmt.Errorf("failed to run shell/command %s %s: %v",
			path, joinedArgs, err)
		return
	} else {
		log.Printf("Started command %s %s", path, joinedArgs)
	}

	wg = &sync.WaitGroup{}
	pipeNum, _ := strconv.Atoi(os.Args[1])
	osc52Pipe := os.NewFile(uintptr(pipeNum), "OSC52Pipe")
	osc52Chan := make(chan []byte, 2)
	wg.Add(1)
	go osc52Thread(osc52Chan, osc52Pipe, wg)

	// Use a separate WaitGroup for the stream processors so we can wait for
	// them to finish before closing osc52Chan
	spWg := &sync.WaitGroup{}
	spIn := runStreamProcessor(os.Stdin, stdinWriter, osc52Chan, spWg,
		"stdin")
	spOut := runStreamProcessor(stdoutReader, os.Stdout, osc52Chan, spWg,
		"stdout")
	spErr := runStreamProcessor(stderrReader, os.Stderr, osc52Chan, spWg,
		"stderr")

	go func() {
		log.Println("Waiting for child command to exit")
		err := cmd.Wait()
		exitCode := cmd.ProcessState.ExitCode()
		log.Printf("Child exited with code %d error %s", exitCode, err.Error())
		spIn.closeProcessedChunksChan()
		spIn.closeUnprocessedChunksChan()
		spOut.closeUnprocessedChunksChan()
		spErr.closeUnprocessedChunksChan()

		// Make sure we exit after a few seconds in case the pipes are blocked.
		waiter := make(chan bool)
		go func() {
			log.Println("Waiting for stream processors to finish")
			spWg.Wait()
			log.Println("Sending END to roxterm over back-channel")
			osc52Chan <- []byte("END")
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
	osc52Chan <- []byte(ok)

	return
}

func main() {
	cacheDir, _ := os.UserCacheDir()
	logOutput, err := os.Create(cacheDir + "/roxterm-shim.log")
	if err == nil {
		log.SetOutput(logOutput)
	}
	wg, err := run()
	if err != nil {
		log.Printf("Error from run(): %v", err)
	}
	if wg != nil {
		wg.Wait()
	}
}
