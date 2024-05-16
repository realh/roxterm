/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2024 Tony Houghton <h@realh.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "send-to-pipe.h"

#define BUFFER_SIZE 16384
#define BUFFER_MIN_AVAILABLE 1024
#define POOL_CAPACITY 64

#define MAX_OF(a, b) ((a) >= (b) ? (a) : (b))
#define MIN_OF(a, b) ((a) <= (b) ? (a) : (b))

#define ESC_CODE 0x1b
#define OSC_CODE 0x9d
#define OSC_ESC ']'
#define ST_CODE 0x9c
#define ST_ESC '\\'
#define BEL_CODE 7

// Primitive array of pointers like a C++ vector without type safety.
typedef struct {
    void **array;
    int start;
    int end;
    int capacity;
} Vector;

static Vector *vector_new(int capacity)
{
    Vector *vec = malloc(sizeof(Vector));
    vec->array = malloc(capacity * sizeof(void *));
    vec->capacity = 0;
    vec->start = 0;
    vec->end = 0;
    return vec;
}

static int vector_length(Vector *vec)
{
    if (vec->start >= vec->end)
        return vec->capacity - vec->start + vec->end;
    return vec->end - vec->start;
}

static inline void *vector_get(Vector *vec, int index)
{
    index = (index + vec->start) % vec->capacity;
    return vec->array[index];
}

static inline void vector_set(Vector *vec, int index, void *p)
{
    index = (index + vec->start) % vec->capacity;
    vec->array[index] = p;
}

static void vector_append(Vector *vec, void *p)
{
    if (vector_length(vec) >= vec->capacity)
    {
        int new_capacity = vec->capacity * 2;
        if (vec->start == 0)
        {
            vec->array = realloc(vec->array, new_capacity);
        }
        else
        {
            void **new_array = malloc(vec->capacity);
            int n1 = vec->capacity - vec->start;
            memcpy(new_array, vec->array + vec->start, n1);
            if (vec->start)
                memcpy(new_array + n1, vec->array, vec->end);
            vec->end = vec->capacity;
            vec->start = 0;
            free(vec->array);
            vec->array = new_array;
        }
        vec->capacity = new_capacity;
    }
    vec->array[vec->end++] = p;
}

static void *vector_pop_end(Vector *vec)
{
    if (vec->end == 0)
        return NULL;
    void *p = vec->array[--vec->end];
    if (vec->end == vec->start)
        vec->start = vec->end = 0;
    else if (vec->end == 0)
        vec->end = vec->capacity;
    return p;
}

static void *vector_pop_start(Vector *vec)
{
    if (vec->end == 0)
        return NULL;
    void *p = vec->array[vec->start++];
    if (vec->end == vec->start)
        vec->start = vec->end = 0;
    else if (vec->start == vec->capacity)
        vec->start = 0;
    return p;
}

//////////////////////////////////////////////

static Vector *buffer_pool = NULL;
static pthread_mutex_t buffer_pool_lock = PTHREAD_MUTEX_INITIALIZER;

static Vector *chunk_pool = NULL;
static pthread_mutex_t chunk_pool_lock = PTHREAD_MUTEX_INITIALIZER;

static void *pool_get(Vector **pool, pthread_mutex_t *lock)
{
    void *p;
    pthread_mutex_lock(lock);
    if (*pool && vector_length(*pool))
        p = vector_pop_end(*pool);
    else
        p = NULL;
    pthread_mutex_unlock(lock);
    return p;
}

// If this returns p (ie non-NULL) you should free it with the appropriate free
// function. This means the pool is full.
static void *pool_return(Vector **pool, pthread_mutex_t *lock, void *p)
{
    pthread_mutex_lock(lock);
    if (!*pool)
        *pool = vector_new(POOL_CAPACITY);
    if (vector_length(*pool) < POOL_CAPACITY)
    {
        vector_append(*pool, p);
        p = NULL;
    }
    pthread_mutex_unlock(lock);
    return p;
}

//////////////////////////////////////////////

typedef struct {
    uint8_t *buf;
    int filled;       // Number of bytes written to buf
    int nread;      // Number of bytes read from buf
    pthread_mutex_t lock;
    pthread_cond_t cond;
} Buffer;

// A Chunk is part of a Buffer that can be read without needing to lock the
// Buffer.
typedef struct {
    uint8_t *buf;
    int size;       // Number of bytes available in buf
    int nread;      // Number of bytes read from buf
} Chunk;

static Chunk *chunk_allocate()
{
    Chunk *chunk = malloc(sizeof(Chunk));
    chunk->buf = NULL;
    chunk->size = 0;
    chunk->nread = 0;
    return chunk;
}

Chunk *get_new_chunk()
{
    Chunk *chunk = pool_get(&chunk_pool, &chunk_pool_lock);
    if (!chunk)
        chunk = chunk_allocate();
    return chunk;
}

inline static void chunk_free(Chunk *chunk)
{
    free(chunk);
}

inline static void chunk_reset(Chunk *chunk)
{
    chunk->buf = NULL;
    chunk->size = 0;
    chunk->nread = 0;
}

static Buffer *buffer_allocate()
{
    Buffer *buffer = malloc(sizeof(Buffer));
    buffer->buf = malloc(BUFFER_SIZE);
    buffer->filled = 0;
    buffer->nread = 0;
    pthread_mutex_init(&buffer->lock, NULL);
    pthread_cond_init(&buffer->cond, NULL);
    return buffer;
}

Buffer *get_new_buffer()
{
    Buffer *buffer = pool_get(&buffer_pool, &buffer_pool_lock);
    if (!buffer)
        buffer = buffer_allocate();
}

inline static void buffer_free(Buffer *buf)
{
    free(buf->buf);
    free(buf);
}

inline static void buffer_reset(Buffer *buf)
{
    buf->filled = 0;
    buf->nread = 0;
}

// If there is sufficient capacity, this returns the address to start writing
// at and the number of bytes that can be written. If the buffer is (nearly)
// full, returns NULL.
static uint8_t *buffer_get_block_for_writing(Buffer *buffer, int *nwritable)
{
    uint8_t *buf;
    pthread_mutex_lock(&buffer->lock);
    if (buffer->filled > BUFFER_SIZE - BUFFER_MIN_AVAILABLE)
    {
        buf = NULL;
        *nwritable = 0;
    }
    buf = buffer->buf + buffer->filled;
    *nwritable = BUFFER_SIZE - buffer->filled;
    pthread_mutex_unlock(&buffer->lock);
    return buf;
}

// Complement to buffer_lock_for_writing; call after data has been written
// to the buffer. This signals the cond if a new Chunk is now available.
static void buffer_filled(Buffer *buffer, int nfilled)
{
    pthread_mutex_lock(&buffer->lock);
    buffer->filled += nfilled;
    if (nfilled > 0)
        pthread_cond_signal(&buffer->cond);
    pthread_mutex_unlock(&buffer->lock);
}

// Gets the next Chunk available from Buffer. Returns NULL if the buffer is
// filled to (near) capacity and the data has all been read already. If the
// buffer has spare capacity but everything so far has been read, this blocks
// until signalled by buffer_filled.
static Chunk *buffer_get_next_chunk(Buffer *buffer)
{
    pthread_mutex_lock(&buffer->lock);
    while (buffer->filled == buffer->nread)
    {
        if (buffer->filled  > BUFFER_SIZE - BUFFER_MIN_AVAILABLE)
        {
            pthread_mutex_unlock(&buffer->lock);
            return NULL;
        }
        pthread_cond_wait(&buffer->cond, &buffer->lock);
    }
    Chunk *chunk = get_new_chunk();
    chunk->buf = buffer->buf + buffer->nread;
    chunk->size = buffer->filled - buffer->nread;
    pthread_mutex_unlock(&buffer->lock);
    chunk->nread = 0;
    buffer->nread = buffer->filled;
    return chunk;
}