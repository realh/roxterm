package main

import (
	"fmt"
	"io"
	"os"
	"os/exec"
	"strconv"
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
}

func NewStreamProcessor(input io.Reader, output io.Writer, wg *sync.WaitGroup,
	osc52Chan chan []byte,
) *StreamProcessor {
	sp := &StreamProcessor{
		input:             input,
		output:            output,
		unprocessedChunks: make(chan []byte, MaxChunkQueueLength),
		processedChunks:   make(chan []byte, MaxChunkQueueLength),
		osc52Chan:         osc52Chan,
		wg:                wg,
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
				//log.Printf("Error reading stream: %v", err)
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
				ch <- chunk
			} else {
				running = false
				break
			}
		}
	}
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
		sp.closeProcessedChunksChan()
	} else if chout = sp.processedChunks; chout == nil {
		sp.closeUnprocessedChunksChan()
	} else {
		for {
			chunk := <-chin
			if chunk == nil {
				sp.closeProcessedChunksChan()
				break
			}
			chout <- chunk
		}
	}
	sp.wg.Done()
}

// chunkWriterThread reads from the processed chunks queue, and writes them to
// the output stream.
func (sp *StreamProcessor) chunkWriterThread() {
	ch := sp.processedChunks
	for ch != nil {
		chunk := <-ch
		if chunk == nil {
			break
		}
		// Write is guaranteed to write the entire slice or return an error
		_, err := sp.output.Write(chunk)
		if err != nil {
			// TODO: Can't realistically log this unless we use a file
			break
		}
	}
	sp.wg.Done()
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
			break
		}
		l := len(chunk)
		dataLen[0] = byte(l & 0xff)
		dataLen[1] = byte((l >> 8) & 0xff)
		dataLen[2] = byte((l >> 16) & 0xff)
		dataLen[3] = byte((l >> 24) & 0xff)
		_, err := osc52Pipe.Write(dataLen)
		if err == nil {
			_, err = osc52Pipe.Write(chunk)
		}
	}
	wg.Done()
}

// Start starts all of a StreamProcessor's goroutines.
func (sp *StreamProcessor) Start() {
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
	osc52Chan chan []byte, wg *sync.WaitGroup,
) *StreamProcessor {
	sp := NewStreamProcessor(r, w, wg, osc52Chan)
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
	spIn := runStreamProcessor(os.Stdin, stdinWriter, osc52Chan, spWg)
	spOut := runStreamProcessor(stdoutReader, os.Stdout, osc52Chan, spWg)
	spErr := runStreamProcessor(stderrReader, os.Stderr, osc52Chan, spWg)

	go func() {
		cmd.Wait()
		spIn.closeProcessedChunksChan()
		spIn.closeUnprocessedChunksChan()
		spOut.closeUnprocessedChunksChan()
		spErr.closeUnprocessedChunksChan()

		// Make sure we exit after a few seconds in case the pipes are blocked.
		waiter := make(chan bool)
		go func() {
			spWg.Wait()
			osc52Chan <- []byte("END")
			close(osc52Chan)
			wg.Wait()
			close(waiter)
		}()
		select {
		case <-waiter:
		case <-time.After(time.Second * 5):
			wg.Done()
		}
	}()

	osc52Chan <- []byte(fmt.Sprintf("OK %d", cmd.Process.Pid))

	return
}

func main() {
	wg, err := run()
	if err != nil {
		reportErrorOnStderr(err.Error())
	}
	if wg != nil {
		wg.Wait()
	}
}
