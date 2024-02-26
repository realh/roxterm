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

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <glib-unix.h>
#include <unistd.h>

#include "roxterm.h"
#include "send-to-pipe.h"

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

/* Reads from fd into buffer, blocking until it's received length bytes. */
static gssize blocking_read(int fd, void *buffer, guint32 length)
{
    guint32 nread = 0;
    while (nread < length)
    {
        gssize n = read(fd, buffer + nread, length - nread);
        if (n <= 0)
            return n;
        else
            nread += n;
    }
    return nread;
}

/*
 * Counterpart to send_to_pipe. Result should be freed. p_nread will be updated
 * to -errno on error.
 */
static char *receive_from_pipe(int fd, gint32 *p_nread)
{
    guint32 len;
    gssize nread = blocking_read(fd, &len, sizeof len);
    if (nread < 0)
    {
        *p_nread = -errno;
        return NULL;
    }
    if (nread == 0)
    {
        *p_nread = 0;
        return NULL;
    }
    if (len > OSC52_SIZE_LIMIT)
    {
        *p_nread = -EMSGSIZE;
        return NULL;
    }
    char *buffer = g_malloc(len);
    if (!buffer)
    {
        *p_nread = -ENOMEM;
        return NULL;
    }
    nread = blocking_read(fd, buffer, len);
    if (nread <= 0)
        g_free(buffer);
    if (nread < 0)
    {
        *p_nread = -errno;
        return NULL;
    }
    if (nread == 0)
    {
        *p_nread = 0;
        return NULL;
    }
    *p_nread = nread;
    return buffer;
}

/*
 * This runs in a background thread of the original process. First it waits
 * for OK <pid> or ERR <error details> from a forked pid and sends it to the
 * callback via idle. Then it waits for OSC52 messages until it reads END.
 */
static gpointer main_context_listener(RoxtermMainContext *ctx)
{
    gboolean started = FALSE;
    
    while (TRUE)
    {
        gint32 msg_len;
        char *msg = receive_from_pipe(ctx->pipe_from_fork, &msg_len);

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
        else if (msg_len < 0)
        {
            ctx->error = g_error_new(G_IO_ERROR,
                g_io_error_from_errno(-msg_len),
                "Error reading message from child");
        }
        if (!msg || ctx->error)
        {
            g_clear_pointer(&msg, g_free);
            msg = NULL;
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
                return NULL;
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
        {
            g_clear_pointer(&msg, g_free);
            break;
        }

        /* TODO: Check for OSC52 */

        g_clear_pointer(&msg, g_free);
    }
    roxterm_main_context_free(ctx);
    return NULL;
}

/*
 * This exits after the child has been launched (or not) and the appropriate
 * message sent to the parent.
 */
static void launch_child(VtePty *pty, const char *working_directory,
                         char **argv, char **envv, int pipe_to_main)
{
    gboolean login = argv[0] != NULL && argv[1] != NULL && argv[1][0] == '-';
    GError *error = NULL;
    GPid pid;
    int child_stdout, child_stderr;

    vte_pty_child_setup(pty);
    // TODO: Instead of g_spawn_async_with_pipes, close all fds this process
    // doesn't need (use /proc/self/fd or /dev/fd) and call execvpe.
    if (!g_spawn_async_with_pipes(
            working_directory, argv, envv,
            G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_CHILD_INHERITS_STDIN |
                (login ? G_SPAWN_FILE_AND_ARGV_ZERO : G_SPAWN_SEARCH_PATH),
            NULL, NULL, &pid, NULL,
            &child_stdout, &child_stderr, &error) &&
        !error)
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
    int nwrit = send_to_pipe(pipe_to_main, msg, -1);
    if (nwrit <= 0)
    {
        g_critical("Failed to send '%s' message to main: %s",
            msg, nwrit ? strerror(-nwrit) : "EOF");
    }
    g_free(msg);
    exit((error || nwrit <= 0) ? 1 : 0);
}

void roxterm_spawn_child(ROXTermData *roxterm, VteTerminal *vte,
                         const char *working_directory,
                         char **argv, char **envv,
                         VteTerminalSpawnAsyncCallback callback,
                         gboolean free_argv2)
{
    GError *error = NULL;
    VtePty *pty = vte_terminal_pty_new_sync(vte, VTE_PTY_DEFAULT,
                                            NULL, &error);
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
                context->pipe_from_fork = pipe_pair[0];
                context->callback = callback;
                context->pid = -1;
                context->error = NULL;
                g_thread_create((GThreadFunc) main_context_listener,
                                context, FALSE, &error);
                
                /* Not sure whether it's better to make vte wait for this pid
                 * or the child it's about to spawn, but this is easier. */
                vte_terminal_watch_child(vte, pid);

                /* We can go ahead and free argv now because the fork's child
                 * has a copy.
                 */
                if (free_argv2)
                    g_free(argv[2]);
                g_free(argv);
            }
            else    /* Child */
            {
                close(pipe_pair[0]);
                launch_child(pty, working_directory, argv, envv,
                                         pipe_pair[1]);
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
