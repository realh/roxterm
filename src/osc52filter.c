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

#include <dlfcn.h>

#include "glib.h"
#include "intptrmap.h"
#include "roxterm.h"
#include "vte/vte.h"
#include "osc52filter.h"

struct Osc52Filter {
    ROXTermData *roxterm;
    int pts_fd;
    size_t max_buflen;
};

static inline void osc52filter_free(Osc52Filter *oflt)
{
    g_free(oflt);
}

typedef struct Osc52DataChunk Osc52DataChunk;

struct Osc52DataChunk {
    Osc52DataChunk *next;
    Osc52Filter *filter;
    size_t size;
    guint8 data[];
};

static inline void osc52_data_chunk_free(Osc52DataChunk *chunk)
{
    g_free(chunk);
}

typedef struct {
    IntPointerMap fd_map;
    GMutex map_mutex;
    GMutex chunk_mutex;
    GCond chunk_cond;
    Osc52DataChunk *first_chunk;
    Osc52DataChunk *last_chunk;
    GThread *thread;
} Osc52Global;

static Osc52Global osc52filter_global;

gboolean osc52filter_global_initialised = FALSE;

static gpointer osc52filter_thread(gpointer handle);

static Osc52Global *
osc52filter_global_init(Osc52Global *og)
{
    int_pointer_map_init(&og->fd_map);
    g_mutex_init(&og->map_mutex);
    g_mutex_init(&og->chunk_mutex);
    g_cond_init(&og->chunk_cond);
    og->first_chunk = NULL;
    og->last_chunk = NULL;
    og->thread = g_thread_new("Osc52Filter", osc52filter_thread, og);
    return og;
}

// Creates a chunk containing a copy of data and pushes it on to the
// list. If size is zero it causes the filter to be freed when the thread
// pops this chunk.
static void osc52filter_global_add_chunk(Osc52Global *og,
                                         Osc52Filter *filter,
                                         guint8 const *data, size_t size)
{
    Osc52DataChunk *chunk = g_malloc(sizeof(Osc52DataChunk) + size);
    chunk->next = NULL;
    chunk->filter = filter;
    chunk->size = size;
    if (size)
    {
        memcpy(chunk->data, data, size);
    }
    g_mutex_lock(&og->chunk_mutex);
    if (og->last_chunk)
    {
        og->last_chunk->next = chunk;
    }
    else
    {
        og->first_chunk = chunk;
        og->last_chunk = chunk;
    }
    g_cond_signal(&og->chunk_cond);
    g_mutex_unlock(&og->chunk_mutex);
}

static inline void osc52filter_add_chunk(Osc52Filter *filter,
                                  guint8 const *data, size_t size)
{
    osc52filter_global_add_chunk(&osc52filter_global, filter, data, size);
}

// This may block, waiting for the cond to be signalled by add_chunk
static Osc52DataChunk *osc52filter_global_get_next_chunk(Osc52Global *og)
{
    g_mutex_lock(&og->chunk_mutex);
    Osc52DataChunk *chunk = NULL;
    while (!chunk)
    {
        chunk = og->first_chunk;
        if (!chunk)
        {
            g_cond_wait(&og->chunk_cond,
                        &og->chunk_mutex);
        }
    }
    og->first_chunk = chunk->next;
    if (!og->first_chunk)
        og->last_chunk = NULL;
    g_mutex_unlock(&og->chunk_mutex);
    return chunk;
}

static inline Osc52DataChunk *osc52filter_get_next_chunk()
{
    return osc52filter_global_get_next_chunk(&osc52filter_global);
}

static inline void osc52filter_ensure_global_init()
{
    if (!osc52filter_global_initialised)
    {
        osc52filter_global_init(&osc52filter_global);
        osc52filter_global_initialised = TRUE;
    }
}

Osc52Filter *osc52filter_create(ROXTermData *roxterm, size_t buflen)
{
    VteTerminal *vte = roxterm_get_vte_terminal(roxterm);
    VtePty *pty = vte ? vte_terminal_get_pty(vte) : NULL;
    int fd = pty ? vte_pty_get_fd(pty) : -1;
    if (fd <= 0)
    {
        g_debug("Pty not available yet for roxterm %p", roxterm);
        return NULL;
    }
    osc52filter_ensure_global_init();
    Osc52Filter *oflt = g_new(Osc52Filter, 1);
    oflt->roxterm = roxterm;
    oflt->pts_fd = fd;
    oflt->max_buflen = buflen;
    g_mutex_lock(&osc52filter_global.map_mutex);
    int_pointer_map_insert(&osc52filter_global.fd_map, fd, oflt);
    g_mutex_unlock(&osc52filter_global.map_mutex);
    g_debug("osc52: Launching roxterm %p with pty fd %d", roxterm, fd);
    return oflt;
}

void osc52filter_set_buffer_size(Osc52Filter *oflt, size_t buflen)
{
    oflt->max_buflen = buflen;
    // TODO: If the filter already contains data longer than this,
    // discard it.
}

void osc52filter_remove(Osc52Filter *oflt)
{
    g_mutex_lock(&osc52filter_global.map_mutex);
    // roxterm may be destroyed before the thread receives the empty chunk,
    // so the thread should lock the map_mutex and read roxterm from the
    // filter again every time it wants to use it.
    oflt->roxterm = NULL;
    int_pointer_map_remove(&osc52filter_global.fd_map, oflt->pts_fd);
    g_mutex_unlock(&osc52filter_global.map_mutex);
    osc52filter_add_chunk(oflt, NULL, 0);
}

// This overrides the system read. When it's called on an fd in the map of
// pts fds it pushes a chunk containing a copy of the data read.
ssize_t read(int fd, void *buf, size_t nbyte)
{
    static ssize_t (*real_read)(int, void *, size_t) = NULL;
    if (!real_read) {
        real_read = dlsym(RTLD_NEXT, "read");
    }
    osc52filter_ensure_global_init();
    ssize_t n = real_read(fd, buf, nbyte);
    if (n <= 0 || !int_pointer_map_contains(&osc52filter_global.fd_map, fd))
        return n;
    Osc52Filter *oflt =
        int_pointer_map_lookup(&osc52filter_global.fd_map, fd);
    g_return_val_if_fail(oflt != NULL, n);
    osc52filter_add_chunk(oflt, buf, n);
    return n;
}

static void osc52filter_process_chunk(Osc52Global *og, Osc52DataChunk *chunk)
{
    (void) og;
    Osc52Filter *oflt = chunk->filter;
    g_debug("osc52: read %ld bytes from fd %d for roxterm %p",
            chunk->size, oflt->pts_fd, oflt->roxterm);
}

static gpointer osc52filter_thread(gpointer handle)
{
    Osc52Global *og = handle;
    while (TRUE)
    {
        Osc52DataChunk *chunk = osc52filter_get_next_chunk();
        g_mutex_lock(&og->map_mutex);
        ROXTermData *roxterm = chunk->filter->roxterm;
        g_mutex_unlock(&og->map_mutex);
        if (roxterm && chunk->size)
        {
            osc52filter_process_chunk(og, chunk);
        }
        if (!chunk->size)
        {
            osc52filter_free(chunk->filter);
        }
        osc52_data_chunk_free(chunk);
    }
    return handle;
}


/* vi:set sw=4 ts=4 et cindent cino= */
