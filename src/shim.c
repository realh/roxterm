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

#include <sys/types.h>
#define G_LOG_DOMAIN "roxterm-shim"

#include <glib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static FILE *debug_out = NULL;

typedef struct {
    int fd;
    char *buf;
    gssize capacity;
    gsize offset_to_next_str;
    gsize offset_to_end;
} BufferedReader;

static inline BufferedReader *buffered_reader_new(int fd)
{
    BufferedReader *reader = g_new0(BufferedReader, 1);
    reader->fd = fd;
    return reader;
}

/**
 * Attempts to read from fd up to and including a character matching c.
 * If c is -1 just read as many bytes as are available, up to the buffer's
 * capacity. The result should not be freed and will be overwritten on the
 * next call. end_out is the offset of the byte following c if c != -1.
 * Returns NULL without setting errmsg if no bytes are read (assumes EOF).
 */
static const char *read_until_c(BufferedReader *reader,
    gsize *end_out, int c, char const **errmsg)
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
        /* See if we can return the current buffered content. */
        if (c != -1)
        {
            for (gsize i = chunk_offset; i < reader->offset_to_end; ++i)
            {
                if (reader->buf[i] == c)
                {
                    *end_out = i + 1;
                    reader->offset_to_next_str = i + 1;
                    return reader->buf;
                }
            }
        } else if (reader->offset_to_end) {
            *end_out = reader->offset_to_end;
            reader->offset_to_next_str = 0;
            reader->offset_to_end = 0;
            return reader->buf;
        }

        /* Buf was empty, or didn't contain c, read more data. */
        chunk_offset = reader->offset_to_end;
        gsize spare_capacity = reader->capacity - chunk_offset;
        gsize new_capacity = 0;

        /* Make sure we have enough capacity for a decent read, but if we
         * have excess, shrink the buffer to make sure memory is freed after
         * a huge paste.
         */
        if (spare_capacity < 256)
            new_capacity = reader->capacity + 1024;
        else if (spare_capacity > 4096)
            new_capacity = chunk_offset + 1024;
        if (new_capacity)
        {
            reader->buf = g_realloc(reader->buf, new_capacity);
            reader->capacity = new_capacity;
            spare_capacity = new_capacity - chunk_offset;
        }

        ssize_t nread = read(reader->fd, reader->buf, spare_capacity);
        if (nread == 0)
        {
            return NULL;
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

/* If len is -1, data is written until and including its 0 terminator. */
static gboolean blocking_write(int fd, const char *data, gssize length,
                               char const **errmsg)
{
    if (length < 0) length = strlen(data) + 1;
    gssize nwritten = 0;
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

static gpointer osc52_pipe_filter(RoxtermPipeContext *rpc)
{
    BufferedReader *reader = buffered_reader_new(rpc->pipe_from_child);
    const char *outname = rpc->output == 1 ? "stdout" : "stderr";
    const char *errmsg = NULL;
    while (TRUE)
    {
        const char *buf;
        gsize buflen;
        buf = read_until_c(reader, &buflen, -1, &errmsg);
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
    char *log_path = g_build_filename(
        g_get_home_dir(), "Code", "roxterm", "log.txt", NULL);
    debug_out = fopen(log_path, "w");
    g_free(log_path);

    RoxtermPipeContext *rpc_stdout = g_new(RoxtermPipeContext, 1);
    rpc_stdout->roxterm_tab_id = strtoul(argv[1], NULL, 16);
    rpc_stdout->output = 1;

    RoxtermPipeContext *rpc_stderr = g_new(RoxtermPipeContext, 1);
    rpc_stderr->roxterm_tab_id = rpc_stdout->roxterm_tab_id;
    rpc_stderr->output = 2;

    GPid pid;
    argv += 2;
    char *cmd_leaf = g_path_get_basename(argv[0]);
    gboolean login = argv[1] != NULL && argv[1][0] == '-' &&
        !strcmp(argv[1] + 1, cmd_leaf);
    g_free(cmd_leaf);

    GError *error = NULL;
    if (g_spawn_async_with_pipes(
        NULL, argv, NULL,
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
