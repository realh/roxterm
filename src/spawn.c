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

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib-unix.h>
#include <unistd.h>

#include "glib/gstdio.h"
#include "glibconfig.h"
#include "roxterm.h"
#include "send-to-pipe.h"
#include "thread-compat.h"

typedef struct {
    ROXTermData *roxterm;
    VteTerminal *vte;
    int pipe_from_fork;
    VteTerminalSpawnAsyncCallback callback;
    GPid pid;
    GError *error;
    GThread *thread;
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
    if (context->thread)
        g_thread_unref(context->thread);
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
    /* With the go shim it's easier to hardwire LE, but with the C shim it's
       easier to use native byte order, so this bit of code needs altering if
       we use C/C++ for the shim. */
    len = GUINT32_FROM_LE(len);
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
 * This runs in a background thread of the original process, waiting for OSC 52
 * messages. It should stop automatically when the pipe gets closed by the shim.
 */
static gpointer main_context_listener(RoxtermMainContext *ctx)
{
    while (TRUE)
    {
        gint32 msg_len;
        char *msg = receive_from_pipe(ctx->pipe_from_fork, &msg_len);

        if (msg)
        {
            g_debug("main_context_listener: received %u '%s'",
                msg_len, msg);
        }
        else
        {
            g_debug("main_context_listener: received null");
        }

        if (msg_len < 0)
        {
            ctx->error = g_error_new(G_IO_ERROR,
                g_io_error_from_errno(-msg_len),
                "Error reading message from child");
        }
        if (!msg || ctx->error)
        {
            g_clear_pointer(&msg, g_free);
            if (ctx->error)
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

        if (g_str_has_prefix(msg, "OK "))
        {
            GPid pid = strtol(msg + 3, NULL, 10);
            g_debug("Pid of shim's child is %d", pid);
            roxterm_update_pid(ctx->roxterm, pid);
        }
        else
        {
            // Anything else is OSC52
            char *semi = strchr(msg, ';');
            if (semi != NULL)
            {
                *semi = 0;
                roxterm_set_clipboard_from_osc52(ctx->roxterm, msg, semi + 1);
            }
            else
            {
                g_critical("Badly formed OSC52 message piped from shim");
            }
        }

        g_clear_pointer(&msg, g_free);
    }
    roxterm_main_context_free(ctx);
    return NULL;
}

void roxterm_spawn_child(ROXTermData *roxterm, VteTerminal *vte,
                         const char *working_directory,
                         char **argv, char **envv,
                         VteTerminalSpawnAsyncCallback callback)
{
    GError *error = NULL;
    GPid pid = -1;

    /*
        * This pipe is used to send messages from the child of the fork to
        * the parent (calling/main context).
        * index 0 = read, 1 = write.
        */
    int pipe_pair[2];
    if (g_unix_open_pipe(pipe_pair, 0, &error))
    {
        g_debug("roxterm_spawn_child: Opened pipes %d (r) and %d (w)",
                pipe_pair[0], pipe_pair[1]);
        argv[1] = g_strdup_printf("%d", pipe_pair[1]);

        fcntl(pipe_pair[1], F_SETFD, FD_CLOEXEC);
        
        RoxtermMainContext *context = g_new(RoxtermMainContext, 1);
        context->roxterm = roxterm;
        context->vte = vte;
        context->pipe_from_fork = pipe_pair[0];
        context->callback = callback;
        context->pid = -1;
        context->error = NULL;
        context->thread = g_thread_try_new(
            "osc52-listener", (GThreadFunc) main_context_listener,
            context, &error);

        vte_terminal_spawn_with_fds_async(
            vte, VTE_PTY_DEFAULT,
            working_directory, (char const * const *) argv,
            (char const * const *) envv,
            pipe_pair + 1, 1, pipe_pair + 1, 1,
            VTE_SPAWN_NO_PARENT_ENVV,
            NULL, NULL, NULL, -1, NULL,
            callback, roxterm);
    }
    else if (!error)
    {
        error = g_error_new(ROXTERM_SPAWN_ERROR, ROXTermSpawnPipeError,
                            _("Unable to open pipe to monitor child"));
    }

    if (error)
    {
        pid = -1;
        callback(vte, pid, error, roxterm);
        g_error_free(error);
    }
}
