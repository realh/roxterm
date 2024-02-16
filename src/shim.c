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

#define G_LOG_DOMAIN "roxterm-shim"

#include <glib.h>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
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
    int input_fd;
    int output_fd;
    guintptr roxterm_tab_id;
    guint8 *buf;
    gsize start;    // Offset to start of current data
    gsize end;      // Offset to one after current data
    gsize capacity; // Total capacity of buf
    GMutex output_lock;
    GCond output_cond;
    OutputItem *output_head;
    OutputItem *output_tail;
    gssize output_result; // <= -1 error, 0 EOF, >= 1 good
    GMutex debug_lock;
    FILE *debug_out;
    const char *output_name;
    gboolean stop_output;
} Osc52FilterContext;

static char *log_dir = NULL;

static void ofc_debug(Osc52FilterContext *ofc, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    g_mutex_lock(&ofc->debug_lock);
    vfprintf(ofc->debug_out, format, ap);
    va_end(ap);
    if (format[strlen(format) - 1] != '\n')
        fputc('\n', ofc->debug_out);
    fflush(ofc->debug_out);
    g_mutex_unlock(&ofc->debug_lock);
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

static Osc52FilterContext *osc52_filter_context_new(guintptr roxterm_tab_id,
                                             int input_fd, int output_fd)
{
    Osc52FilterContext *ofc = g_new(Osc52FilterContext, 1);
    ofc->roxterm_tab_id = roxterm_tab_id;
    ofc->output_fd = output_fd;
    ofc->input_fd = input_fd;
    ofc->buf = g_new(guint8, DEFAULT_CAPACITY);
    ofc->start = 0;
    ofc->end = 0;
    ofc->capacity = DEFAULT_CAPACITY;
    g_mutex_init(&ofc->output_lock);
    g_cond_init(&ofc->output_cond);
    ofc->output_head = NULL;
    ofc->output_tail = NULL;
    ofc->output_result = 1;
    ofc->stop_output = FALSE;
    g_mutex_init(&ofc->debug_lock);
    ofc->output_name = fd_name(ofc->output_fd);
    char *log_leaf = g_strdup_printf("osc52-log-%lx-%s.txt",
        ofc->roxterm_tab_id, ofc->output_name);
    char *log_file = g_build_filename(log_dir, log_leaf, NULL);
    ofc->debug_out = fopen(log_file, "w");
    g_free(log_file);
    g_free(log_leaf);
    return ofc;
}

// Returns number of bytes written, 0 for EOF, <0 for error
static gssize write_output(Osc52FilterContext *ofc, const guint8 *data,
                             gsize length)
{
    gsize nwritten = 0;
    while (nwritten < length)
    {
        gssize n = write(ofc->output_fd, data + nwritten, length - nwritten);
        if (n == 0)
        {
            ofc_debug(ofc, "No bytes written to %s (EOF?)\n", ofc->output_name);
            return 0;
        }
        else if (n < 0)
        {
            ofc_debug(ofc, "Error writing to %s: %s\n",
                    ofc->output_name, strerror(errno));
            return n;
        }
        else
        {
            nwritten += n;
        }
    }
    return nwritten;
}

inline static guint8 ofc_char(Osc52FilterContext *ofc, gsize index)
{
    return ofc->buf[ofc->start + index];
}

// Returns number of bytes of data between ofc->start and ofc->end.
inline static guint8 ofc_len(Osc52FilterContext *ofc)
{
    return ofc->end - ofc->start;
}

// If spare_capacity is 1, allows buf to exceed MAX_CAPACITY to avoid
// frustratingly giving up when all we need is one more byte to check for ST.
static void ensure_spare_capacity(Osc52FilterContext *ofc, gsize spare_capacity)
{
    if (ofc->capacity - ofc->end >= spare_capacity)
        return;
    if (ofc->start)
    {
        gsize old_size = ofc->end - ofc->start;
        // Only shift data to the start if it's a small amount
        if (old_size <= MAX_MOVE)
        {
            if (old_size)
                memmove(ofc->buf, ofc->buf + ofc->start, old_size);
            ofc->start = 0;
            ofc->end = old_size;
        }
    }
    if (ofc->capacity - ofc->end >= spare_capacity)
        return;
    gsize capacity = ofc->capacity * 2;
    if (capacity > MAX_CAPACITY)
        capacity = MAX_CAPACITY;
    if (capacity <= ofc->capacity)
    {
        if (spare_capacity == 1 && capacity < MAX_CAPACITY + 1)
        {
            capacity++;
        }
        else
        {
            ofc_debug(ofc, "GMD: Buffer capacity %ld is at/beyond max\n",
                ofc->capacity);
            return;
        }
    }
    assert(capacity >= ofc->capacity);
    if (capacity > ofc->capacity)
    {
        ofc->buf = g_realloc(ofc->buf, capacity * sizeof(guint8));
        ofc->capacity = capacity;
    }
}

// Reads more data into buf at end, extending buf's capacity if necessary.
// If buf is already at MAX_CAPACITY, no data will be read, and end will
// remain unchanged. Returns result of the read operation.
static gssize get_more_data(Osc52FilterContext *ofc, gsize min_capacity)
{
    ensure_spare_capacity(ofc, min_capacity);
    gsize spare_capacity = ofc->capacity - ofc->end;
    gsize nread = MAX_OF(spare_capacity, DEFAULT_CAPACITY);
    gsize offset = ofc->end;
    if (nread < 0)
        ofc_debug(ofc, "GMD: Read error %s\n", strerror(errno));
    else if (nread == 0)
        ofc_debug(ofc, "GMD: Read EOF\n");
    nread = read(ofc->input_fd, ofc->buf + offset, nread);
    if (nread > 0)
        ofc->end += nread;
    return nread;
}

// Call while the mutex is locked. Returns the result of the previous write.
static gssize queue_item_for_writing(Osc52FilterContext *ofc, OutputItem *item)
{
    gssize last_result = ofc->output_result;
    if (last_result <= 0)
    {
        output_item_free(item);
        return last_result;
    }
    if (ofc->output_tail)
        ofc->output_tail->next = item;
    else
        ofc->output_head = item;
    ofc->output_tail = item;
    g_cond_signal(&ofc->output_cond);
    return last_result;
}

// Writes nwrite bytes starting from from ofc->start and updates
// start to point to one past the written data.
static gssize flush_up_to(Osc52FilterContext *ofc, gsize nwrite)
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
        // main buffer, copy the former to a new buffer, otherwise use ofc's
        // buf in the item and replace it with a copy of the remainder.
        gsize surplus = ofc_len(ofc) - nwrite;
        if (nwrite < surplus)
        {
            item->buf = g_new(guint8, nwrite);
            memcpy(item->buf, ofc->buf + ofc->start, nwrite);
            item->start = 0;
            item->size = nwrite;
            ofc->start += nwrite;
        }
        else {
            item->buf = ofc->buf;
            item->start = ofc->start;
            item->size = nwrite;
            ofc->buf = g_new(guint8, surplus);
            memcpy(ofc->buf, item->buf + item->start + nwrite, surplus);
            ofc->start = 0;
            ofc->end = surplus;
        }

        g_mutex_lock(&ofc->output_lock);
        nwrite = queue_item_for_writing(ofc, item);
        g_mutex_unlock(&ofc->output_lock);
        if (nwrite <= 0)
        {
            ofc_debug(ofc, "FUT: Previous output result %ld\n", nwrite);
            return nwrite;
        }

        // nwrite = blocking_write(ofc->output, ofc->buf + ofc->start, nwrite,
        //                         fd_name(ofc->output));
    }
    if (ofc->start == ofc->end && ofc->start != 0)
    {
        ofc->start = 0;
        ofc->end = 0;
        if (ofc->capacity > DEFAULT_CAPACITY)
        {
            // Don't use realloc because the data is invalid so copying it
            // is wasteful
            g_free(ofc->buf);
            ofc->buf = g_new(guint8, DEFAULT_CAPACITY);
            ofc->capacity = DEFAULT_CAPACITY;
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

// offset is relative to ofc->start. If the buffer contains 52; but not a valid
// *write* request, returns IsNotOsc52.
static OscStatus buf_contains_52(Osc52FilterContext *ofc, gsize offset)
{
    gsize buflen = ofc->end - ofc->start;
    if (offset < buflen - 1 && ofc_char(ofc, offset) != '5')
        return IsNotOsc52;
    if (offset < buflen - 2 && ofc_char(ofc, offset + 1) != '2')
        return IsNotOsc52;
    if (offset < buflen - 3 && ofc_char(ofc, offset + 2) != ';')
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
        guint8 c = ofc_char(ofc, offset);
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

static OscStatus get_osc_status_at_offset(Osc52FilterContext *ofc, gsize offset)
{
    gsize buflen = ofc_len(ofc);
    guint8 c = ofc_char(ofc, offset);
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
        else if (ofc_char(ofc, offset + 1) == OSC_ESC)
            osc_status = IsOsc;
        else
            osc_status = IsNotOsc;
    }
    if (osc_status == IsOsc)
        osc_status = buf_contains_52(ofc, offset + esc_size);
    if (osc_status != IsNotOsc)
    {
        ofc_debug(ofc, "FOS: osc_status %s at offset %ld/%ld\n",
            osc_status_names[osc_status], offset, ofc_len(ofc));
    }
    return osc_status;
}

// If data between ofc->start and ofc->end contains the start of an OSC 52,
// returns the offset, relative to ofc->start, of the data following "52;".
// Otherwise returns 0 for no match, -1 for error/EOF. If there is a match
// the data before it is flushed ie it's written to the output, and ofc->start
// then points to the start of the ESC sequence.
static gssize find_osc_start(Osc52FilterContext *ofc)
{
    gsize offset;   // Relative to ofc->start
    for (offset = 0; offset < ofc_len(ofc); ++offset)
    {
        OscStatus osc_status = get_osc_status_at_offset(ofc, offset);

        switch (osc_status)
        {
            // In these 2 cases flush the data before the ESC, then get
            // more data and check the status again.
            case Unknown:
            case IsOsc:
                flush_up_to(ofc, offset);
                // flush changes start to offset, so now offset is 0
                offset = 0;
                if (get_more_data(ofc, MIN_CAPACITY) <= 0)
                    return -1;
                osc_status = get_osc_status_at_offset(ofc, offset);
                break;
            default:
                break;
        }

        switch (osc_status)
        {
        case Unknown:
        case IsOsc:
            ofc_debug(ofc, "FOS: OscStatus %s, fail!\n",
                osc_status_names[osc_status]);
            continue;
        case IsNotOsc:
        case IsNotOsc52:
            continue;
        case IsOsc52:
            flush_up_to(ofc, offset);
            offset = 0;
            return (ofc_char(ofc, offset) == ESC_CODE) ? 5 : 4;
        }
    }

    return 0;
}

static gpointer input_filter(Osc52FilterContext *ofc)
{
    while (TRUE)
    {
        gssize nread = get_more_data(ofc, MIN_CAPACITY);
        if (nread == 0)
        {
            ofc_debug(ofc, "F: Read EOF\n");
            return NULL;
        }
        else if (nread < 0)
        { 
            ofc_debug(ofc, "F: Read error %s\n", strerror(errno));
            return NULL;
        }

        gssize osc52_offset = find_osc_start(ofc);
        if (osc52_offset == -1)
        {
            return NULL;
        }
        if (TRUE) // (osc52_offset == 0)
        {
            if (flush_up_to(ofc, ofc_len(ofc)) <= 0)
                return NULL;
        }
    }
    return NULL;
}

static void stop_output_writer(Osc52FilterContext *ofc)
{
    g_mutex_lock(&ofc->output_lock);
    ofc->stop_output = TRUE;
    g_cond_signal(&ofc->output_cond);
    g_mutex_unlock(&ofc->output_lock);
}

static gpointer output_writer(Osc52FilterContext *ofc)
{
    g_mutex_lock(&ofc->output_lock);
    while (!ofc->stop_output || ofc->output_head)
    {
        if (!ofc->output_head)
            g_cond_wait(&ofc->output_cond, &ofc->output_lock);
        OutputItem *item = ofc->output_head;
        if (!item)
            continue;
        if (item == ofc->output_tail)
            ofc->output_tail = NULL;
        ofc->output_head = item->next;
        g_mutex_unlock(&ofc->output_lock);
        gssize nwritten = write_output(ofc,
            item->buf + item->start, item->size);
        output_item_free(item);
        g_mutex_lock(&ofc->output_lock);
        if (nwritten <= 0)
            break;
    }
    g_mutex_unlock(&ofc->output_lock);
    return NULL;
}

static GThread *run_osc52_thread(gpointer (*thread_func)(Osc52FilterContext *),
                                 Osc52FilterContext *ofc,
                                 const char *name_prefix)
{
    char *thread_name = g_strdup_printf("%s-%s",
        name_prefix, ofc->output_name);
    ofc_debug(ofc, "Launching %s thread copying from %d to %d\n",
            thread_name, ofc->input_fd, ofc->output_fd);
    GThread *thread = g_thread_new(thread_name,
                        (GThreadFunc) thread_func, ofc);
    if (!thread)
    {
        ofc_debug(ofc, "Failed to launch %s thread\n", thread_name);
        exit(1);
    }
    return thread;
}

inline static GThread *run_input_filter(Osc52FilterContext *ofc)
{
    return run_osc52_thread(input_filter, ofc, "input");
}

inline static GThread *run_output_writer(Osc52FilterContext *ofc)
{
    return run_osc52_thread(output_writer, ofc, "output");
}

int main(int argc, char **argv)
{
    log_dir = g_build_filename(g_get_user_cache_dir(), "roxterm", NULL);
    g_mkdir_with_parents(log_dir, 0755);

    guintptr roxterm_tab_id = strtoul(argv[1], NULL, 16);

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

    int stdout_pipe = -1;
    int stderr_pipe = -1;

    GError *error = NULL;
    if (g_spawn_async_with_pipes(
        NULL, argv2, NULL,
        G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_CHILD_INHERITS_STDIN |
        (login ? G_SPAWN_FILE_AND_ARGV_ZERO : G_SPAWN_SEARCH_PATH),
        NULL, NULL,
        &pid, NULL,
        &stdout_pipe, &stderr_pipe,
        &error))
    {
        // TODO: stdout
        Osc52FilterContext *ofc_stderr = osc52_filter_context_new(
            roxterm_tab_id, stderr_pipe, 2);

        g_free(log_dir);

        GThread *stderr_input_thread = run_input_filter(ofc_stderr);
        GThread *stderr_output_thread = run_output_writer(ofc_stderr);

        int status = 0;
        waitpid(pid, &status, 0);
        ofc_debug(ofc_stderr, "Child exited: %d, status %d\n",
                WIFEXITED(status), WEXITSTATUS(status));
        stop_output_writer(ofc_stderr);
        g_thread_join(stderr_input_thread);
        g_thread_join(stderr_output_thread);
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
