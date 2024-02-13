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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MIN_CAPACITY 1024
#define EXCESS_CAPACITY 2048
#define MAX_CAPACITY 1024 * 1024

static FILE *debug_out = NULL;

typedef struct {
    int fd;
    guint8 *buf;
    gssize capacity;
    gsize offset_to_next_str;
    gsize offset_to_end;
    gboolean eof;
} BufferedReader;

static inline BufferedReader *buffered_reader_new(int fd)
{
    BufferedReader *reader = g_new0(BufferedReader, 1);
    reader->fd = fd;
    return reader;
}

/**
 * Attempts to read from fd up to and including an escape character. If the
 * escape is a double byte (ESC something), it will include the second byte.
 * If force_esc is TRUE it will enlarge the buffer (up to a point) to
 * accommodate a long clipboard, otherwise it may return before encountering an
 * ESC. The result should not be freed and will be overwritten on
 * the next call. end_out is the offset of the byte following the last byte read
 * or one past the escape character. Returns NULL without setting errmsg if no
 * bytes are read (assumes EOF).
 */
static const guint8 *read_until_esc(BufferedReader *reader,
    gsize *end_out, gboolean force_esc, char const **errmsg)
{
    /* If there's stuff left over from the previous read, move it to the start
     * of the buffer.
     */
    if (reader->offset_to_next_str)
    {
        memcpy(reader->buf, reader->buf + reader->offset_to_next_str,
            reader->offset_to_end - reader->offset_to_next_str);
        reader->offset_to_end -= reader->offset_to_next_str;
        reader->offset_to_next_str = 0;
    }
    gsize chunk_offset = 0;

    /* Start reading */
    while (TRUE)
    {
        gboolean want_esc_code = FALSE;
        /* See if we can return the current buffered content. */
        for (gsize i = chunk_offset; i < reader->offset_to_end; ++i)
        {
            switch (reader->buf[i])
            {
                case 0x1b:      // ESC
                    if (i + 1 < reader->offset_to_end)
                    {
                        ++i;
                        // fall through
                    }
                    else
                    {
                        want_esc_code = TRUE;
                        break;
                    }
                /* Not interested in all of these, but they introduce string
                 * sequences, except for BEL, which is a non-standard
                 * alternative to ST.
                 */
                case 0x90:      // DCS
                case 0x98:      // SOS
                case 0x9b:      // CSI
                case 0x9c:      // ST
                case 0x9d:      // OSC
                case 0x9e:      // PM
                case 0x9f:      // APC
                case 0x07:      // BEL
                    *end_out = i + 1;
                    reader->offset_to_next_str = i + 1;
                    return reader->buf;
            }
        }

        /* Any surplus from previous read should be sent to pty asap */
        if (!force_esc && !want_esc_code && reader->offset_to_end) {
            *end_out = reader->offset_to_end;
            reader->offset_to_next_str = 0;
            reader->offset_to_end = 0;
            return reader->buf;
        }

        if (reader->eof)
            return NULL;

        /* Buf was empty, or didn't contain c, read more data. */
        chunk_offset = reader->offset_to_end;
        gsize spare_capacity = reader->capacity - chunk_offset;
        gsize new_capacity = 0;

        /* Make sure we have enough capacity for a decent read, but if we
         * have excess, shrink the buffer to make sure memory is freed after
         * a huge paste and that we're not reading big chunks which might
         * increase latency.
         */
        if (spare_capacity < MIN_CAPACITY)
            new_capacity = reader->capacity + MIN_CAPACITY;
        else if (spare_capacity > EXCESS_CAPACITY)
            new_capacity = chunk_offset + MIN_CAPACITY;
        if (new_capacity > MAX_CAPACITY && !want_esc_code)
        {
            force_esc = FALSE;
            continue;
        }
        if (new_capacity)
        {
            reader->buf = g_realloc(reader->buf, new_capacity);
            reader->capacity = new_capacity;
            spare_capacity = new_capacity - chunk_offset;
        }
        if (want_esc_code)
            spare_capacity = 1;

        ssize_t nread = read(reader->fd, reader->buf + reader->offset_to_end,
                             spare_capacity);
        if (nread == 0)
        {
            reader->eof = TRUE;
        }
        else if (nread < 0)
        { 
            *errmsg = strerror(errno);
            return NULL;
        }
        reader->offset_to_end += nread;
    }
    return NULL;
}

static gboolean blocking_write(int fd, const guint8 *data, gsize length,
                               char const **errmsg)
{
    if (length < 0) length = strlen((char *) data) + 1;
    gsize nwritten = 0;
    while (nwritten < length)
    {
        gssize n = write(fd, data + nwritten, length - nwritten);
        if (n == 0)
        {
            return FALSE;
        }
        else if (n < 0)
        {
            *errmsg = strerror(errno);
            return FALSE;
        }
        else
        {
            nwritten += n;
        }
    }
    return TRUE;
}

typedef struct {
    int output;
    int pipe_from_child;
    guintptr roxterm_tab_id;
} RoxtermPipeContext;

typedef enum {
    NormalMode,
    StartedEsc,
    ReceivingClipBoard,
    ClipboardTooLarge,
} PipeFilterState;

static gpointer osc52_pipe_filter(RoxtermPipeContext *rpc)
{
    BufferedReader *reader = buffered_reader_new(rpc->pipe_from_child);
    const char *outname = rpc->output == 1 ? "stdout" : "stderr";
    const char *errmsg = NULL;
    PipeFilterState pf_state = NormalMode;

    while (TRUE)
    {
        const guint8 *buf;
        gsize buflen;
        gboolean force_esc = pf_state == ReceivingClipBoard;

        buf = read_until_esc(reader, &buflen, force_esc, &errmsg);
        if (errmsg)
        {
            fprintf(debug_out, "Error reading from %lx child's %s: %s\n",
                    rpc->roxterm_tab_id, outname, errmsg);
            exit(1);
        }
        else if (!buf || !buflen)
        {
            fprintf(debug_out, "No data read from %lx child's %s (EOF?)\n",
                    rpc->roxterm_tab_id, outname);
            exit(0);
        }
        gboolean wrote = blocking_write(rpc->output, buf, buflen, &errmsg);
        if (errmsg)
        {
            fprintf(debug_out, "Error writing to %lx pty's %s: %s\n",
                    rpc->roxterm_tab_id, outname, errmsg);
            exit(1);
        }
        else if (!wrote)
        {
            fprintf(debug_out, "No data written to %lx pty's %s (EOF?)\n",
                    rpc->roxterm_tab_id, outname);
            exit(0);
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    char *log_dir = g_build_filename(g_get_user_cache_dir(), "roxterm", NULL);
    g_mkdir_with_parents(log_dir, 0755);
    char *log_leaf = g_strdup_printf("shim-log-%s.txt", argv[1]);
    char *log_file = g_build_filename(log_dir, log_leaf, NULL);
    debug_out = fopen(log_file, "a");
    g_free(log_file);
    g_free(log_leaf);
    g_free(log_dir);

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
        &rpc_stdout->pipe_from_child,
        &rpc_stderr->pipe_from_child, &error))
    {
        if (!g_thread_new("stdout",
                          (GThreadFunc) osc52_pipe_filter, rpc_stdout))
        {
            fprintf(debug_out, "Failed to launch thread for stdout\n");
            exit(1);
        }
        if (!g_thread_new("stderr",
                          (GThreadFunc) osc52_pipe_filter, rpc_stderr))
        {
            fprintf(debug_out, "Failed to launch thread for stderr\n");
            exit(1);
        }
        int status = 0;
        waitpid(pid, &status, 0);
        fprintf(debug_out, "Child exited: %d, status %d\n",
                WIFEXITED(status), WEXITSTATUS(status));
        exit(status);
    }
    else
    {
        fprintf(debug_out, "Failed to run command: %s\n",
                error ? error->message : "unknown error");
        exit(1);
    }
    
    return 0;
}

/* vi:set sw=4 ts=4 et cindent cino= */
