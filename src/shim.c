/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2015 Tony Houghton <h@realh.co.uk>

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

#include <stdarg.h>
#define G_LOG_DOMAIN "roxterm-shim"

#include <glib.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_OF(a, b) ((a) >= (b) ? (a) : (b))

#define DEFAULT_CAPACITY 2048
#define MIN_CAPACITY 1024
#define EXCESS_CAPACITY 4096
#define MAX_CAPACITY 1024 * 1024
#define MAX_MOVE DEFAULT_CAPACITY

#define ESC_CODE 0x1b
#define OSC_CODE 0x9c
#define OSC_ESC ']'
#define ST_CODE 0x9c
#define ST_ESC '\\'
#define BEL_CODE 7

typedef struct {
    int output;
    int pipe_from_child;
    guintptr roxterm_tab_id;
} RoxtermPipeContext;

// An item queued for output.
typedef struct _OutputItem {
    struct _OutputItem *next;
    guint8 *buf;
    gsize start;
    gsize size;
} OutputItem;

inline static void output_item_free(OutputItem *item)
{
    g_free(item->buf);
    g_free(item);
}

typedef struct {
    RoxtermPipeContext *rpc;
    int fd;
    guint8 *buf;
    gsize start;    // Offset to start of current data
    gsize end;      // Offset to one after current data
    gsize capacity; // Total capacity of buf
    GMutex output_lock;
    OutputItem *output_head;
    OutputItem *output_tail;
    gssize output_result; // <= -1 error, 0 EOF, >= 1 good
    GMutex debug_lock;
    FILE *debug_out;
    const char *output_name;
} BufferedReader;

static char *log_dir = NULL;

static void br_debug(BufferedReader *br, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    g_mutex_lock(&br->debug_lock);
    vfprintf(br->debug_out, format, ap);
    va_end(ap);
    if (format[strlen(format) - 1] != '\n')
        fputc('\n', br->debug_out);
    fflush(br->debug_out);
    g_mutex_unlock(&br->debug_lock);
}

// This may leak if called with fd > 2
static const char *fd_name(int fd)
{
    switch (fd)
    {
        case 0:
            return "stdin";
        case 1:
            return "stdout";
        case 2:
            return "stderr";
        default:
            return g_strdup_printf("fd%d", fd);
    }
}

static BufferedReader *buffered_reader_new(RoxtermPipeContext *rpc)
{
    BufferedReader *br = g_new(BufferedReader, 1);
    br->rpc = rpc;
    br->fd = rpc->pipe_from_child;
    br->buf = g_new(guint8, DEFAULT_CAPACITY);
    br->start = 0;
    br->end = 0;
    br->capacity = DEFAULT_CAPACITY;
    g_mutex_init(&br->output_lock);
    br->output_head = NULL;
    br->output_tail = NULL;
    br->output_result = 1;
    g_mutex_init(&br->debug_lock);
    br->output_name = fd_name(br->rpc->output);
    char *log_leaf = g_strdup_printf("shim-log-%lx-%s.txt",
        br->rpc->roxterm_tab_id, br->output_name);
    char *log_file = g_build_filename(log_dir, log_leaf, NULL);
    br->debug_out = fopen(log_file, "w");
    g_free(log_file);
    g_free(log_leaf);
    return br;
}

// Returns number of bytes written, 0 for EOF, <0 for error
static gssize blocking_write(BufferedReader *br, const guint8 *data,
                             gsize length)
{
    gsize nwritten = 0;
    while (nwritten < length)
    {
        gssize n = write(br->rpc->output, data + nwritten, length - nwritten);
        if (n == 0)
        {
            br_debug(br, "No bytes written to %s (EOF?)\n", br->output_name);
            return 0;
        }
        else if (n < 0)
        {
            br_debug(br, "Error writing to %s: %s\n",
                    br->output_name, strerror(errno));
            return n;
        }
        else
        {
            nwritten += n;
        }
    }
    return nwritten;
}

inline static guint8 br_char(BufferedReader *br, gsize index)
{
    return br->buf[br->start + index];
}

// Returns number of bytes of data between br->start and br->end.
inline static guint8 br_len(BufferedReader *br)
{
    return br->end - br->start;
}

// If spare_capacity is 1, allows buf to exceed MAX_CAPACITY to avoid
// frustratingly giving up when all we need is one more byte to check for ST.
static void ensure_spare_capacity(BufferedReader *br, gsize spare_capacity)
{
    if (br->capacity - br->end >= spare_capacity)
        return;
    if (br->start)
    {
        gsize old_size = br->end - br->start;
        // Only shift data to the start if it's a small amount
        if (old_size <= MAX_MOVE)
        {
            if (old_size)
                memmove(br->buf, br->buf + br->start, old_size);
            br->start = 0;
            br->end = old_size;
        }
    }
    if (br->capacity - br->end >= spare_capacity)
        return;
    gsize capacity = br->capacity * 2;
    if (capacity > MAX_CAPACITY)
        capacity = MAX_CAPACITY;
    if (capacity <= br->capacity)
    {
        if (spare_capacity == 1 && capacity < MAX_CAPACITY + 1)
        {
            capacity++;
        }
        else
        {
            br_debug(br, "GMD: Buffer capacity %ld is at/beyond max\n",
                br->capacity);
            return;
        }
    }
    assert(capacity >= br->capacity);
    if (capacity > br->capacity)
    {
        br->buf = g_realloc(br->buf, capacity * sizeof(guint8));
        br->capacity = capacity;
    }
}

// Reads more data into buf at end, extending buf's capacity if necessary.
// If buf is already at MAX_CAPACITY, no data will be read, and end will
// remain unchanged. Returns result of the read operation.
static gssize get_more_data(BufferedReader *br, gsize min_capacity)
{
    ensure_spare_capacity(br, min_capacity);
    gsize spare_capacity = br->capacity - br->end;
    gsize nread = MAX_OF(spare_capacity, DEFAULT_CAPACITY);
    gsize offset = br->end;
    if (nread < 0)
        br_debug(br, "GMD: Read error %s\n", strerror(errno));
    else if (nread == 0)
        br_debug(br, "GMD: Read EOF\n");
    nread = read(br->fd, br->buf + offset, nread);
    if (nread > 0)
        br->end += nread;
    return nread;
}

// Call while the mutex is locked. Returns the result of the previous write.
static gssize queue_item_for_writing(BufferedReader *br, OutputItem *item)
{
    if (br->output_result <= 0)
    {
        output_item_free(item);
        return br->output_result;
    }
    if (br->output_tail)
        br->output_tail->next = item;
    else
        br->output_head = item;
    br->output_tail = item;
}

// Writes nwrite bytes starting from from br->start and updates
// start to point to one past the written data.
static gssize flush_up_to(BufferedReader *br, gsize nwrite)
{
    if (nwrite > 0)
    {
        // Prepare the OutputItem before locking to check whether it can be
        // flushed, because if it can't be flushed, the time wasted by this
        // is irrelevant, and the lock should be held for as short a time as
        // possible.
        OutputItem *item = g_new(OutputItem, 1);
        item->next = NULL;
        // If the data to be written is smaller than the remainder of the
        // main buffer, copy the former to a new buffer, otherwise use br's
        // buf in the item and replace it with a copy of the remainder.
        gsize surplus = br_len(br) - nwrite;
        if (nwrite < surplus)
        {
            item->buf = g_new(guint8, nwrite);
            memcpy(item->buf, br->buf + br->start, nwrite);
            item->start = 0;
            item->size = nwrite;
            br->start += nwrite;
        }
        else {
            item->buf = br->buf;
            item->start = br->start;
            item->size = nwrite;
            br->buf = g_new(guint8, surplus);
            memcpy(br->buf, item->buf + item->start + nwrite, surplus);
            br->start = 0;
            br->end = surplus;
        }

        g_mutex_lock(&br->output_lock);
        nwrite = queue_item_for_writing(br, item);
        g_mutex_unlock(&br->output_lock);
        if (nwrite <= 0)
        {
            br_debug(br, "FUT: Previous output result %ld\n", nwrite);
            return nwrite;
        }

        // nwrite = blocking_write(br->rpc->output, br->buf + br->start, nwrite,
        //                         fd_name(br->rpc->output));
    }
    if (br->start == br->end && br->start != 0)
    {
        br->start = 0;
        br->end = 0;
        if (br->capacity > DEFAULT_CAPACITY)
        {
            // Don't use realloc because the data is invalid so copying it
            // is wasteful
            g_free(br->buf);
            br->buf = g_new(guint8, DEFAULT_CAPACITY);
            br->capacity = DEFAULT_CAPACITY;
        }
    }
    return nwrite;
}

typedef enum {
    Unknown,    // No more data after ESC
    IsNotOsc,
    IsOsc,      // OSC, number unknown
    IsNotOsc52, // OSC but confirmed not 52
    IsOsc52,
} OscStatus;

static char const *osc_status_names[] = {
    "Unknown",
    "IsNotOsc",
    "IsOsc",
    "IsNotOsc52",
    "IsOsc52",
};

// offset is relative to br->start. If the buffer contains 52; but not a valid
// *write* request, returns IsNotOsc52.
static OscStatus buf_contains_52(BufferedReader *br, gsize offset)
{
    gsize buflen = br->end - br->start;
    if (offset < buflen - 1 && br_char(br, offset) != '5')
        return IsNotOsc52;
    if (offset < buflen - 2 && br_char(br, offset + 1) != '2')
        return IsNotOsc52;
    if (offset < buflen - 3 && br_char(br, offset + 2) != ';')
        return IsNotOsc52;
    if (offset + 3 >= buflen)
        return IsOsc;
    // Don't allow more than one each of 'c' and 'p'
    // Other selection characters are allowed, but will be ignored in the
    // absence of c or p.
    gboolean have_c = FALSE;
    gboolean have_p = FALSE;
    gboolean have_semi = FALSE;
    for (offset = offset + 4; offset < buflen; ++offset)
    {
        guint8 c = br_char(br, offset);
        if (have_semi)
        {
            return c == '?' ? IsNotOsc52 : IsOsc52;
        }
        if (c == ';')
        {
            if (have_c || have_p)
                have_semi = TRUE;
            else
                return IsNotOsc52;
        }
        else if (c == 'c' && !have_c)
            have_c = TRUE;
        else if (c == 'p' && !have_p)
            have_p = TRUE;
        else if (!strchr("qs01234567", (char) c))
            return IsNotOsc52;
    }
    return IsNotOsc52;
}

static OscStatus get_osc_status_at_offset(BufferedReader *br, gsize offset)
{
    gsize buflen = br_len(br);
    guint8 c = br_char(br, offset);
    gsize esc_size = 0;
    if (c == OSC_CODE)
        esc_size = 1;
    else if (c == ESC_CODE)
        esc_size = 2;
    OscStatus osc_status = IsNotOsc;
    if (esc_size == 1)
    {
        osc_status = IsOsc;
    }
    else if (esc_size == 2)
    {
        if (offset + 1 >= buflen)
            osc_status = Unknown;
        else if (br_char(br, offset + 1) == OSC_ESC)
            osc_status = IsOsc;
        else
            osc_status = IsNotOsc;
    }
    if (osc_status == IsOsc)
        osc_status = buf_contains_52(br, offset + esc_size);
    if (osc_status != IsNotOsc)
    {
        br_debug(br, "FOS: osc_status %s at offset %ld/%ld\n",
            osc_status_names[osc_status], offset, br_len(br));
    }
    return osc_status;
}

// If data between br->start and br->end contains the start of an OSC 52,
// returns the offset, relative to br->start, of the data following "52;".
// Otherwise returns 0 for no match, -1 for error/EOF. If there is a match
// the data before it is flushed ie it's written to the output, and br->start
// then points to the start of the ESC sequence.
static gssize find_osc_start(BufferedReader *br)
{
    gsize offset;   // Relative to br->start
    for (offset = 0; offset < br_len(br); ++offset)
    {
        OscStatus osc_status = get_osc_status_at_offset(br, offset);

        switch (osc_status)
        {
            // In these 2 cases flush the data before the ESC, then get
            // more data and check the status again.
            case Unknown:
            case IsOsc:
                flush_up_to(br, offset);
                // flush changes start to offset, so now offset is 0
                offset = 0;
                if (get_more_data(br, MIN_CAPACITY) <= 0)
                    return -1;
                osc_status = get_osc_status_at_offset(br, offset);
                break;
            default:
                break;
        }

        switch (osc_status)
        {
        case Unknown:
        case IsOsc:
            br_debug(br, "FOS: OscStatus %s, fail!\n",
                osc_status_names[osc_status]);
            continue;
        case IsNotOsc:
        case IsNotOsc52:
            continue;
        case IsOsc52:
            flush_up_to(br, offset);
            offset = 0;
            return (br_char(br, offset) == ESC_CODE) ? 5 : 4;
        }
    }

    return 0;
}

static gpointer osc52_pipe_filter(BufferedReader *br)
{
    while (TRUE)
    {
        gssize nread = get_more_data(br, MIN_CAPACITY);
        if (nread == 0)
        {
            br_debug(br, "F: Read EOF\n");
            return NULL;
        }
        else if (nread < 0)
        { 
            br_debug(br, "F: Read error %s\n", strerror(errno));
            return NULL;
        }

        gsize osc52_offset = find_osc_start(br);
        if (osc52_offset == -1)
        {
            return NULL;
        }
        if (TRUE) // (osc52_offset == 0)
        {
            if (flush_up_to(br, br_len(br)) <= 0)
                return NULL;
        }
    }
    return NULL;
}

static BufferedReader *run_filter(RoxtermPipeContext *rpc)
{
    BufferedReader *br = buffered_reader_new(rpc);
    br_debug(br, "%s filter copying from %d to %d\n",
            br->output_name, rpc->pipe_from_child,
            rpc->output);
    char *thread_name = g_strdup_printf("filter-%s", br->output_name);
    if (!g_thread_new(thread_name,
                        (GThreadFunc) osc52_pipe_filter, br))
    {
        br_debug(br, "Failed to launch %s thread\n", thread_name);
        exit(1);
    }
    return br;
}

int main(int argc, char **argv)
{
    log_dir = g_build_filename(g_get_user_cache_dir(), "roxterm", NULL);
    g_mkdir_with_parents(log_dir, 0755);

    RoxtermPipeContext *rpc_stdout = g_new(RoxtermPipeContext, 1);
    rpc_stdout->roxterm_tab_id = strtoul(argv[1], NULL, 16);
    rpc_stdout->output = 1;

    RoxtermPipeContext *rpc_stderr = g_new(RoxtermPipeContext, 1);
    rpc_stderr->roxterm_tab_id = rpc_stdout->roxterm_tab_id;
    rpc_stderr->output = 2;

    GPid pid;
    argv += 2;
    argc -= 2;
    char *cmd_leaf = g_path_get_basename(argv[0]);
    gboolean login = argv[1] != NULL && argv[1][0] == '-' &&
        !strcmp(argv[1] + 1, cmd_leaf);
    g_free(cmd_leaf);

    // g_spawn_async_with_pipes requires that argv is null-terminated and
    // there's no guarantee that the main argument is.
    char **argv2 = g_new(char *, argc + 1);
    memcpy(argv2, argv, argc * sizeof(char *));
    argv2[argc] = NULL;

    GError *error = NULL;
    if (g_spawn_async_with_pipes(
        NULL, argv2, NULL,
        G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_CHILD_INHERITS_STDIN |
        (login ? G_SPAWN_FILE_AND_ARGV_ZERO : G_SPAWN_SEARCH_PATH),
        NULL, NULL,
        &pid, NULL,
//        &rpc_stdout->pipe_from_child,
        NULL,
        &rpc_stderr->pipe_from_child, &error))
    {
        BufferedReader *br_stderr = run_filter(rpc_stderr);

        g_free(log_dir);

        int status = 0;
        waitpid(pid, &status, 0);
        br_debug(br_stderr, "Child exited: %d, status %d\n",
                WIFEXITED(status), WEXITSTATUS(status));
        exit(status);
    }
    else
    {
        fprintf(stderr, "Failed to run command: %s\n",
                error ? error->message : "unknown error");
        exit(1);
    }
    
    return 0;
}

/* vi:set sw=4 ts=4 et cindent cino= */
