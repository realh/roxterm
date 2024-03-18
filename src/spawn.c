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
            ctx->callback(ctx->vte, pid, NULL, ctx->roxterm);
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

/*
 * Closes all files open in the current process except for 0, 1, 2, keep_pipe
 * and keep_pty.
 */
static void close_fds(int keep_pipe, int keep_pty)
{
    // /dev/fd should exist on most systems, but isn't guaranteed on Linux, so
    // check /dev/fd first, and if it doesn't exist try /proc/self/fd.
    const char *fd_dir = "/dev/fd";
    if (!g_file_test(fd_dir, G_FILE_TEST_EXISTS))
        fd_dir = "/proc/self/fd";
    if (!g_file_test(fd_dir, G_FILE_TEST_EXISTS))
    {
        g_warning("Unable to locate /dev/fd or /proc/self/fd");
        return;
    }

    GError *error = NULL;
    GDir *dir = g_dir_open(fd_dir, 0, &error);
    if (!dir)
    {
        g_critical("Unable to read contents of %s: %s", fd_dir,
            (error && error->message) ? error->message : "unknown error");
        return;
    }

    const char *entry_name;
    while ((entry_name = g_dir_read_name(dir)) != NULL)
    {
        // POSIX should skip '.' and '..', but play it safe 
        if (!isdigit(entry_name[0]))
            continue;
        int fd = strtol(entry_name, NULL, 10);
        if (fd > 2 && fd != keep_pipe && fd != keep_pty)
        {
            g_debug("close_fds closing %d", fd);
            close(fd);
        }
        else
        {
            g_debug("close_fds skipping %d", fd);
        }
    }
    g_dir_close(dir);
}

/*
 * If this fails it sends an error message down pipe_to_main.
 */
static void launch_child(VtePty *pty, const char *working_directory,
                         char **argv, char **envv, int pipe_to_main)
{
    int keep_pty = vte_pty_get_fd(pty);
    g_debug("launch_child: keeping pipe %d and pty %d", pipe_to_main, keep_pty);
    close_fds(pipe_to_main, keep_pty);

    if (working_directory && working_directory[0])
    {
        g_debug("launch_child: cd '%s'", working_directory);
        g_chdir(working_directory);
    }
    else
    {
        g_debug("launch_child: no working_directory");
    }

    char *cmd = g_strjoinv(" ", argv);
    g_debug("launch_child: cmd %s", cmd);
    g_free(cmd);

    g_debug("launch_child: calling vte_pty_child_setup");
    vte_pty_child_setup(pty);
    
    // After this we don't want to call g_debug because it will come out
    // in the terminal we just launched.

    // This doesn't return if it succeeds
    execve(argv[0], argv, envv);
    //g_debug("launch_child: execve failed");
    char *msg = g_strdup_printf("ERR %u,%u,%s: %s", ROXTERM_SPAWN_ERROR,
        ROXTermSpawnError, _("Unable to spawn child process"),
        strerror(errno));
    int nwrit = send_to_pipe(pipe_to_main, msg, -1);
    if (nwrit <= 0)
    {
        g_critical("Failed to send '%s' message to main: %s",
            msg, nwrit ? strerror(-nwrit) : "EOF");
    }
    g_free(msg);
    exit(1);
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
            g_debug("roxterm_spawn_child: Opened pipes %d (r) and %d (w)",
                    pipe_pair[0], pipe_pair[1]);
            argv[1] = g_strdup_printf("%d", pipe_pair[1]);
            pid = fork();
            if (pid != 0)   /* Parent */
            {
                g_debug("roxterm_spawn_child: main context (parent) waiting "
                    "for pid of %d's child", pid);
                g_debug("r_s_c parent: closing pipe/w %d", pipe_pair[1]);
                close(pipe_pair[1]);
                g_debug("r_s_c parent: closed pipe/w %d", pipe_pair[1]);
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
                g_debug("r_s_c child: closing pipe/w %d", pipe_pair[0]);
                close(pipe_pair[0]);
                g_debug("r_s_c child: closed pipe/w %d", pipe_pair[0]);
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
