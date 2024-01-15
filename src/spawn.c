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
#include <string.h>
#include <glib-unix.h>
#include <unistd.h>

#define ROXTERM_SPAWN_ERROR g_quark_from_static_string("roxterm-spawn-error-quark")    

enum {
    ROXTermSpawnPtyError,
    ROXTermSpawnError,
    ROXTermSpawnPipeError,
};

/*
 * Attempts to read up to 4096 bytes from fd into buf, setting a GError
 * on failure.
 */
static gssize read_fd(int fd, char *buf, GError **error)
{
    ssize_t nread = read(fd, buf, 4096);
    if (nread < 0 && error)
    {
        *error = g_error_new(G_FILE_ERROR,
                             g_file_error_from_errno(errno),
                             "Error reading from pipe: %s",
                             strerror(errno));
    }
    return nread;
}

static void wait_for_spawn_result(ROXTermData *roxterm, VteTerminal *vte,
                                  int result_pipe,
                                  VteTerminalSpawnAsyncCallback callback)
{
    /* This needs to be done in a GSourceFunc polling on result_pipe */
    GError *error = NULL;
    char buf[4096];
    GPid pid = -1;
    if (read_fd(result_pipe, buf, &error) > 0)
    {
    }
    close(result_pipe);
    errno = 0;
    if (error)
        pid = -1;
    callback(vte, pid, error, roxterm);
    if (error)
        g_error_free(error);
}

void roxterm_spawn_child(ROXTermData *roxterm, VteTerminal *vte,
                         const char *working_directory,
                         char **argv, char **envv,
                         VteTerminalSpawnAsyncCallback callback,
                         gboolean free_argv1_and_argv)
{
    gboolean login = argv[0] != NULL && argv[1] != NULL && argv[1][0] == '-';
    GError *error = NULL;
    VtePty *pty = vte_terminal_pty_new_sync(vte, VTE_PTY_DEFAULT, NULL, &error);
    GPid pid = -1;

    if (pty)
    {
        /* This pipe is used to wait for the result of g_spawn_async.
         * index 0 = read, 1 = write */
        int result_pipe[2];
        vte_terminal_set_pty(vte, pty);
        if (g_unix_open_pipe(result_pipe, 0, &error))
        {
            pid = fork();
            if (pid != 0)
            {
                /*
                 * Note this pid is not the one we need to pass back to the
                 * callback; we need the child of that pid. It gets sent down
                 * result_pipe.
                 */
                close(result_pipe[1]);
                wait_for_spawn_result(roxterm, vte,
                                      result_pipe[0],
                                      callback);
            }
            if (g_spawn_async(working_directory, argv, envv,
                    G_SPAWN_DO_NOT_REAP_CHILD |
                        (login ? G_SPAWN_FILE_AND_ARGV_ZERO :
                              G_SPAWN_SEARCH_PATH),
                    (GSpawnChildSetupFunc) vte_pty_child_setup, pty,
                    &pid, &error))
            {
                vte_terminal_watch_child(vte, pid);
            }
            /*
            else if (!error)
            {
                error = g_error_new(ROXTERM_SPAWN_ERROR, ROXTermSpawnError,
                                    _("Unable to spawn child process"));
            }
            */
        }
        else if (!error)
        {
            error = g_error_new(ROXTERM_SPAWN_ERROR, ROXTermSpawnPipeError,
                                _("Unable to open pipe for spawn result"));
        }
    }
    else if (!error)
    {
        error = g_error_new(ROXTERM_SPAWN_ERROR, ROXTermSpawnPtyError,
                            _("Unable to create PTY"));
    }
    if (error)
        pid = -1;
    callback(vte, pid, error, roxterm);
    if (error)
        g_error_free(error);
    if (free_argv1_and_argv)
    {
        g_free(argv[1]);
        g_free(argv);
    }
}
