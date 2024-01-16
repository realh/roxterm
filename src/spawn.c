/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2018 Tony Houghton <h@realh.co.uk>

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

#include "spawn.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <glib-unix.h>
#include <unistd.h>
#include <sys/wait.h>

#define ROXTERM_SPAWN_ERROR \
    g_quark_from_static_string("roxterm-spawn-error-quark")    

enum {
    ROXTermSpawnPtyError,
    ROXTermSpawnError,
    ROXTermSpawnPipeError,
    ROXTermSpawnGarbledMessageError,
    ROXTermSpawnUnexpectedMsgError,
    ROXTermSpawnBadPidError,
};

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
 * Returns NULL without setting an error if no bytes are read (assumes EOF).
 */
static const char *read_until_c(BufferedReader *reader,
    gsize *end_out, int c, GError **error)
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
            if (error)
            {
                *error = g_error_new(G_FILE_ERROR,
                                    g_io_error_from_errno(errno),
                                    "Error reading from pipe: %s",
                                    strerror(errno));
            }
            return NULL;
        }
        reader->offset_to_end += nread;
    }
    return NULL;
}

/* If len is -1, data is written until and including its 0 terminator. */
static gboolean blocking_write(int fd, const char *data, gssize length,
    GError **error)
{
    if (length < 0) length = strlen(data) + 1;
    gssize nwritten = 0;
    while (nwritten < length)
    {
        gssize n = write(fd, data + nwritten, length - nwritten);
        if (n = 0)
        {
            if (error)
            {
                *error = g_error_new(ROXTERM_SPAWN_ERROR, ROXTermSpawnPipeError,
                    "Nothing was written to pipe");
            }
            return FALSE;
        }
        else if (n < 0)
        {
            if (error)
            {
                *error = g_io_error_from_errno(errno);
            }
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
    ROXTermData *roxterm;
    VteTerminal *vte;
    int pipe_from_fork;
    VteTerminalSpawnAsyncCallback callback;
    GPid pid;
    GError *error;
} RoxtermMainContext;

typedef struct {
    int output;
    int pipe_to_main;
    int pipe_from_child;
} RoxtermPipeContext;

static void roxterm_main_context_free(RoxtermMainContext *context)
{
    if (context->error)
        g_error_free(context->error);
    if (context->pipe_from_fork > 0)
        close(context->pipe_from_fork);
    g_free(context);
}

GError *garbled_message_error(const char *s)
{
    return g_error_new(ROXTERM_SPAWN_ERROR, ROXTermSpawnGarbledMessageError,
        "Garbled message from filter: %s", s);
}

GError *parse_error(const char *s)
{
    char *endptr = NULL;
    long domain = strtol(s, &endptr, 10);
    if (endptr == s || *endptr != ',')
        return garbled_message_error(s);
    s = endptr + 1;
    long code = strtol(s, &endptr, 10);
    if (endptr == s || *endptr != ',')
        return garbled_message_error(s);
    return g_error_new((GQuark) domain, (int) code, "%s", endptr + 1);
}

/*
 * If context->error is set, the context is freed after calling its callback.
 */
static int main_callback_on_idle(RoxtermMainContext *context)
{
    context->callback(context->vte, context->pid, context->error,
        context->roxterm);
    if (context->error)
    {
        roxterm_main_context_free(context);
    }
    return G_SOURCE_REMOVE;
}

/*
 * This runs in a background thread of the original process. First it waits
 * for OK <pid> or ERR <error details> from a forked pid and sends it to the
 * callback via idle. Then it waits for OSC52 messages until it reads END.
 */
static gpointer main_context_listener(RoxtermMainContext *ctx)
{
    gboolean started = FALSE;
    BufferedReader *reader = buffered_reader_new(ctx->pipe_from_fork);
    while (TRUE)
    {
        const char *msg;
        gsize msg_len;
        msg = read_until_c(reader, &msg_len, 0, &ctx->error);
        if (msg)
        {
            if (g_str_has_prefix(msg, "ERR"))
            {
                ctx->error = parse_error(msg + 4);
            }
            else if (g_str_has_prefix(msg, "OK"))
            {
                if (started)
                {
                    g_critical("main_context_listener received spurious OK");
                }
                else
                {
                    long pid = strtol(msg + 3, NULL, 10);
                    if (pid == 0)
                    {
                        ctx->error = g_error_new(ROXTERM_SPAWN_ERROR,
                            ROXTermSpawnBadPidError,
                            "Can't read pid from '%s'", msg);
                    }
                    else
                    {
                        g_debug("main_context_listener: %s", msg);
                        ctx->pid = pid;
                        ctx->error = NULL;
                        g_idle_add((GSourceFunc) main_callback_on_idle, ctx);
                        started = TRUE;
                    }
                }
            }
        }
        if (!msg || ctx->error)
        {
            if (!started)
            {
                if (!ctx->error)
                {
                    ctx->error = g_error_new(ROXTERM_SPAWN_ERROR,
                        ROXTermSpawnPipeError,
                        "IPC pipe was closed prematurely");
                }
                /* This will free the context later. */
                g_idle_add((GSourceFunc) main_callback_on_idle, ctx);
                return;
            }
            else if (ctx->error)
            {
                g_critical("main_context_listener error %s",
                    ctx->error->message);
            }
            else
            {
                g_warning("main_context_listener: pipe was closed");
            }
            break;
        }

        if (!strcmp(msg, "END"))
            break;
        /* TODO: Check for OSC52 */
    }
    roxterm_main_context_free(ctx);
    return NULL;
}

static gpointer osc52_pipe_filter(RoxtermPipeContext *rpc)
{
    GError *error = NULL;
    BufferedReader *reader = buffered_reader_new(rpc->pipe_from_child);
    const char *outname = rpc->output == 1 ? "stdout" : "stderr";
    while (TRUE)
    {
        const char *buf;
        gsize buflen;
        buf = read_until_c(reader, &buflen, -1, &error);
        if (error)
        {
            g_critical("Error reading from child's %s: %s",
                outname, error->message);
            break;
        }
        else if (!buf || !buflen)
        {
            g_critical("No data from child's %s", outname, error->message);
            break;
        }
        gboolean wrote = blocking_write(rpc->output, buf, buflen, &error);
        if (error)
        {
            g_critical("Error writing to pty's %s: %s",
                outname, error->message);
            break;
        }
        else if (!wrote)
        {
            g_critical("No data written to pty's %s", outname);
            break;
        }
    }
    return NULL;
}

static void start_pipe_filter_thread(int output,
                                     int pipe_to_main, int pipe_from_child,
                                     GError **error)
{
    RoxtermPipeContext *rpc = g_new(RoxtermPipeContext, 1);
    rpc->output = output;
    rpc->pipe_to_main = pipe_to_main;
    rpc->pipe_from_child = pipe_from_child;
    g_thread_create(osc52_pipe_filter, rpc, FALSE, error);
}

/*
 * This blocks until the child dies, then exits itself (it never returns).
 */
static void launch_and_monitor_child(VtePty *pty,
                                     const char *working_directory,
                                     char **argv, char **envv,
                                     int pipe_to_main,
                                     gboolean free_argv1_and_argv)
{
    gboolean login = argv[0] != NULL && argv[1] != NULL && argv[1][0] == '-';
    GError *error = NULL;
    GPid pid;
    int child_stdout, child_stderr;

    vte_pty_child_setup(pty);
    if (g_spawn_async_with_pipes(
            working_directory, argv, envv,
            G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_CHILD_INHERITS_STDIN |
                (login ? G_SPAWN_FILE_AND_ARGV_ZERO : G_SPAWN_SEARCH_PATH),
            NULL, NULL,
            &pid, NULL, &child_stdout, &child_stderr, &error))
    {
        start_pipe_filter_thread(1, pipe_to_main, child_stdout, &error);
        if (!error)
        {
            start_pipe_filter_thread(2, pipe_to_main, child_stdout, &error);
        }
        if (error)
        {
            g_critical("Error staring pipe filter thread: %s", error->message);
            kill(pid, SIGTERM);
        }
    }
    else if (!error)
    {
        error = g_error_new(ROXTERM_SPAWN_ERROR, ROXTermSpawnError,
                            _("Unable to spawn child process"));
    }
    char *msg;
    if (error)
    {
        msg = g_strdup_printf("ERR %u,%u,%s", error->domain, error->code,
            error->message);
    }
    else
    {
        msg = g_strdup_printf("OK %u", pid);
    }
    GError *e2 = NULL;
    if (!blocking_write(pipe_to_main, msg, -1, &e2))
    {
        g_critical("Failed to send message to main: %s", msg);
        if (e2)
        {
            g_critical("Pipe write error: %s", e2->message);
            g_error_free(e2);
        }
    }
    g_free(msg);
    if (!error && !e2)
    {
        wait(NULL);
    }
    close(pipe_to_main);
    if (error)
        g_error_free(error);
    exit((error || e2) ? 1 : 0);
}

void roxterm_spawn_child(ROXTermData *roxterm, VteTerminal *vte,
                         const char *working_directory,
                         char **argv, char **envv,
                         VteTerminalSpawnAsyncCallback callback,
                         gboolean free_argv1_and_argv)
{
    GError *error = NULL;
    VtePty *pty = vte_terminal_pty_new_sync(vte, VTE_PTY_DEFAULT, NULL, &error);
    GPid pid = -1;

    if (pty)
    {
        /*
         * This pipe is used to send messages from the child of the fork to
         * the parent (calling/main context).
         * index 0 = read, 1 = write.
         */
        int pipe_pair[2];
        vte_terminal_set_pty(vte, pty);
        if (g_unix_open_pipe(pipe_pair, 0, &error))
        {
            pid = fork();
            if (pid != 0)   /* Parent */
            {
                /*
                 * Note this pid is not the one we need to pass back to the
                 * callback; we need the child of that pid. It gets sent down
                 * result_pipe.
                 */
                g_debug("roxterm_spawn_child: main context (parent) waiting "
                    "for pid of %d's child", pid);
                close(pipe_pair[1]);
                RoxtermMainContext *context = g_new(RoxtermMainContext, 1);
                context->roxterm = roxterm;
                context->vte = vte;
                context->pipe_from_fork = pipe_pair;
                context->callback = callback;
                context->pid = -1;
                context->error = NULL;
                g_thread_create(main_context_listener, context, FALSE, &error);
                
                /* Not sure whether it's better to make vte wait for this pid
                 * or the child it's about to spawn, but this is easier. */
                vte_terminal_watch_child(vte, pid);

                /* We can go ahead and free argv now because the fork's child
                 * has a copy.
                 */
                if (free_argv1_and_argv)
                {
                    g_free(argv[1]);
                    g_free(argv);
                }
            }
            else    /* Child */
            {
                close(pipe_pair[0]);
                launch_and_monitor_child(pty, working_directory, argv, envv,
                                         pipe_pair[1], free_argv1_and_argv);
            }
        }
        else if (!error)
        {
            error = g_error_new(ROXTERM_SPAWN_ERROR, ROXTermSpawnPipeError,
                                _("Unable to open pipe to monitor child"));
        }
    }
    else if (!error)
    {
        error = g_error_new(ROXTERM_SPAWN_ERROR, ROXTermSpawnPtyError,
                            _("Unable to create PTY"));
    }
    if (error)
    {
        pid = -1;
        callback(vte, pid, error, roxterm);
        g_error_free(error);
    }
}
