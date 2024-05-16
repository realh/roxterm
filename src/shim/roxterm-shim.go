package main

import (
	"io"
	"log"
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

// ShimBuffer holds data as it's read from the child's stdin/stderr until it's
// ready to be sent to the parent.
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
	childReader       io.Reader
	parentWriter      io.Writer
	unprocessedChunks chan []byte
	processedChunks   chan []byte
}

func NewStreamProcessor(childReader io.Reader, parentWriter io.Writer,
) *StreamProcessor {
	sp := &StreamProcessor{
		childReader:       childReader,
		parentWriter:      parentWriter,
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

// childReaderThread reads data from the child stream and feeds it to the
// unprocessed chunks queue
func (sp *StreamProcessor) childReaderThread() {
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
			reader := sp.childReader
			if reader == nil {
				return
			}
			nFilled, err := reader.Read(chunk)
			if err != nil {
				log.Printf("Error reading from child process: %v", err)
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
