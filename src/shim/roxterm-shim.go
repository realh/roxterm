package main

import (
	"io"
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
}

func NewStreamProcessor(input io.Reader, output io.Writer,
) *StreamProcessor {
	sp := &StreamProcessor{
		input:             input,
		output:            output,
		unprocessedChunks: make(chan []byte, MaxChunkQueueLength),
		processedChunks:   make(chan []byte, MaxChunkQueueLength),
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
	for {
		buf := NewShimBuffer()
		if buf == nil {
			return
		}
		for {
			chunk := buf.NextFillableSlice()
			if chunk == nil {
				break
			}
			reader := sp.input
			if reader == nil {
				return
			}
			nFilled, err := reader.Read(chunk)
			if err != nil {
				//log.Printf("Error reading stream: %v", err)
				sp.closeUnprocessedChunksChan()
				return
			}
			if nFilled == 0 {
				sp.closeUnprocessedChunksChan()
				return
			}
			buf.Filled(nFilled)
			chunk = chunk[:nFilled]
			ch := sp.unprocessedChunks
			if ch != nil {
				ch <- chunk
			} else {
				return
			}
		}
	}
}

// chunkProcessorThread reads from the unprocessed chunks queue, and passes
// the chunks on to the processed chunk queue.
// Later there will be two versions, one to filter out OSC 52 and send clipboard
// write requests to the parent out of band, another to intercept the parent's
// response to XTGETTCAP to indicate OSC 52 is supported.
func (sp *StreamProcessor) chunkProcessorThread() {
	chin := sp.unprocessedChunks
	if chin == nil {
		sp.closeProcessedChunksChan()
		return
	}
	chout := sp.processedChunks
	if chout == nil {
		sp.closeUnprocessedChunksChan()
		return
	}
	for {
		chunk := <-chin
		if chunk == nil {
			sp.closeProcessedChunksChan()
			return
		}
		chout <- chunk
	}
}

// chunkWriterThread reads from the processed chunks queue, and writes them to
// the output stream.
func (sp *StreamProcessor) chunkWriterThread() {
	ch := sp.processedChunks
	if ch == nil {
		return
	}
	for {
		chunk := <-ch
		if chunk == nil {
			return
		}
		for len(chunk) > 0 {
			nWritten, err := sp.output.Write(chunk)
			if err != nil {
				// TODO: Can't realistically log this unless we use a file
				return
			}
			chunk = chunk[nWritten:]
		}
	}
}
