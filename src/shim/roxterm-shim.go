package main

import (
	"io"
	"log"
	"sync"
)

const (
	BufferSize            = 16384
	BufferMinChunkSize    = 256
	MaxBufferSizeForTopUp = BufferSize - BufferMinChunkSize
	MaxBufferQueueLength  = 16
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
	nRead   int // Number of bytes read from the buffer
	mutex   sync.Mutex
	cond    sync.Cond
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
func (buf *ShimBuffer) NextFillableSlice() (chunk []byte) {
	buf.mutex.Lock()
	if buf.nFilled > MaxBufferSizeForTopUp {
		chunk = nil
	} else {
		chunk = buf.buf[buf.nFilled:]
	}
	buf.mutex.Unlock()
	return
}

// Filled marks that the slice has been filled with nFilled bytes. Call this
// as soon as any data has been saved to the slice, don't wait until the slice
// is full.
func (buf *ShimBuffer) Filled(nFilled int) {
	buf.mutex.Lock()
	buf.nFilled += nFilled
	if nFilled > 0 {
		buf.cond.Signal()
	}
	buf.mutex.Unlock()
}

func (buf *ShimBuffer) IsEmpty() bool {
	return buf.nFilled == 0
}

// NextFilledSlice gets the next slice full of data that has been stored in the
// buffer. If the buffer is full and all the data has been read already it
// returns nil. If the buffer has spare capacity but all the valid data has
// already been read, this blocks until data is made available by the Filled
// method.
func (buf *ShimBuffer) NextFilledSlice() (chunk []byte) {
	buf.mutex.Lock()
	for buf.nFilled <= buf.nRead {
		if buf.nFilled > MaxBufferSizeForTopUp {
			chunk = nil
			break
		}
		buf.cond.Wait()
	}
	chunk = buf.buf[buf.nRead:buf.nFilled]
	buf.nRead = buf.nFilled
	buf.mutex.Unlock()
	return
}

type StreamProcessor struct {
	childReader        io.Reader
	parentWriter       io.Writer
	filledBuffers      chan *ShimBuffer
	filledBufferWaiter chan bool
	unprocessedChunks  chan []byte
	processedChunks    chan []byte
}

func NewStreamProcessor(childReader io.Reader, parentWriter io.Writer,
) *StreamProcessor {
	sp := &StreamProcessor{
		childReader:       childReader,
		parentWriter:      parentWriter,
		filledBuffers:     make(chan *ShimBuffer, MaxBufferQueueLength),
		unprocessedChunks: make(chan []byte, MaxChunkQueueLength),
		processedChunks:   make(chan []byte, MaxChunkQueueLength),
	}
	return sp
}

// NextEmptyBuffer gets a buffer ready to be filled with data from downstream.
// If the queue of filled buffers is long this blocks until a buffer is popped.
// It returns nil if the processor is shutting down.
func (sp *StreamProcessor) NextEmptyBuffer() (buf *ShimBuffer) {
	sp.filledBufferWaiter = make(chan bool)
	waiter := sp.filledBufferWaiter
	ch := sp.filledBuffers
	if ch == nil {
		buf = nil
	} else if len(ch) > MaxBufferQueueLength {
		if <-waiter {
			buf = NewShimBuffer()
		} else {
			buf = nil
		}
	} else {
		buf = NewShimBuffer()
	}
	sp.filledBufferWaiter = nil
	return
}

// PushFilledBuffer adds a buffer to the queue to be processed
func (sp *StreamProcessor) PushFilledBuffer(buf *ShimBuffer) {
	ch := sp.filledBuffers
	if ch != nil {
		ch <- buf
	}
}

// NextfilledBuffer gets the next (partially) filled buffer from the head of
// the queue.
func (sp *StreamProcessor) NextFilledBuffer() *ShimBuffer {
	ch := sp.filledBuffers
	if ch == nil {
		return nil
	}
	buf := <-sp.filledBuffers
	waiter := sp.filledBufferWaiter
	if waiter != nil {
		waiter <- buf != nil
	}
	return buf
}

func (sp *StreamProcessor) childReaderThread() {
	for {
		buf := sp.NextEmptyBuffer()
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
				return
			}
			if nFilled == 0 {
				return
			}
			firstFill := buf.IsEmpty()
			buf.Filled(nFilled)
			if firstFill {
				sp.PushFilledBuffer(buf)
			}
		}
	}
}
