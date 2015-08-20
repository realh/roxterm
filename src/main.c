/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2011 Tony Houghton <h@realh.co.uk>

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

#include "defns.h"

#include <errno.h>
#include <stdio.h>
#include <locale.h>
#include <unistd.h>

#include <gdk/gdk.h>

#include "dlg.h"
#include "globalopts.h"
#include "multitab.h"
#include "roxterm.h"
#include "rtdbus.h"
#if ENABLE_SM
#include "session.h"
#endif
#include "session-file.h"
#include "x11support.h"

#define ROXTERM_DBUS_NAME RTDBUS_NAME ".term"
#define ROXTERM_DBUS_OBJECT_PATH RTDBUS_OBJECT_PATH "/term"
#define ROXTERM_DBUS_INTERFACE RTDBUS_INTERFACE
#define ROXTERM_DBUS_METHOD_NAME "NewTerminal"

extern char **environ;

static DBusMessage *create_dbus_message(int argc, char **argv,
        const char *display)
{
    DBusMessage *message;
    int n;

    message = rtdbus_method_new(ROXTERM_DBUS_NAME, ROXTERM_DBUS_OBJECT_PATH,
            ROXTERM_DBUS_INTERFACE,
            ROXTERM_DBUS_METHOD_NAME, DBUS_TYPE_INVALID);
    for (n = 0; environ[n]; ++n);
    message = rtdbus_append_args(message,
            DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &environ, n,
            DBUS_TYPE_INVALID);
    if (!message)
        return NULL;

    for (n = 0; message && n < argc; ++n)
    {
        char *arg;
        char *tmp = NULL;

        if (!strcmp(argv[n], "--tab"))
        {
            if (global_options_workspace == WORKSPACE_UNDEFINED)
            {
                guint32 ws;

                if (x11support_get_current_desktop(
                        gdk_get_default_root_window(), &ws))
                {
                    global_options_workspace = ws;
                }
                else
                {
                    g_warning(_("Unable to read current workspace"));
                }
            }
            tmp = g_strdup_printf("--tab=%u", global_options_workspace);
            arg = tmp;
        }
        else
        {
            arg = argv[n];
        }
        message = rtdbus_append_args(message,
                DBUS_TYPE_STRING, &arg,
                DBUS_TYPE_INVALID);
        if (tmp)
            g_free(tmp);
    }
    if (display)
    {
        message = rtdbus_append_args(message,
                DBUS_TYPE_STRING, &display,
                DBUS_TYPE_INVALID);
    }
    return message;
}

/* Returns 0 for OK, -1 if DBUS fails, +1 if reply is an error */
static int run_via_dbus(DBusMessage *message)
{
    int result = 0;

    if (!message)
        return -1;

    /* New roxterm command may have been run in a different directory
     * from original instance */
    if (!result && !global_options_directory)
    {
        char *cwd = GET_CURRENT_DIR();
        const char *d = "-d";

        message = rtdbus_append_args(message,
                DBUS_TYPE_STRING, &d,
                DBUS_TYPE_STRING, &cwd,
                DBUS_TYPE_INVALID);
        if (!message)
            result = -1;
        FREE_CURRENT_DIR(cwd);
    }
    if (!result && global_options_commandv)
    {
        int n;
        const char *e = "-e";

        message = rtdbus_append_args(message,
                DBUS_TYPE_STRING, &e, DBUS_TYPE_INVALID);
        if (!message)
            return -1;
        for (n = 0; global_options_commandv[n]; ++n)
        {
            message = rtdbus_append_args(message,
                    DBUS_TYPE_STRING, &global_options_commandv[n],
                    DBUS_TYPE_INVALID);
            if (!message)
                return -1;
        }
    }
    if (!result)
    {
        /* The reply to the NewTerminal message can't include whether it
         * launched successfully because we don't know that until we fork
         * the command after an idle, and D-BUS won't let us defer the reply to
         * after an idle.
         *
         * Removing the reply prevents the error message reported in
         * <http://p.sf.net/roxterm/gi5kdI9QrPZ>
         * at the expense of regressing
         * <http://p.sf.net/roxterm/EfNoUkwW>
         */
        result = rtdbus_send_message(message) ? 0 : -1;
    }
    return result;
}

static DBusHandlerResult new_term_listener(DBusConnection *connection,
        DBusMessage *message, void *user_data)
{
    DBusError derror;
    char **env;
    char **argv;
    int argc;
    const char *display = NULL;
    DBusMessageIter iter;

    (void) connection;
    (void) user_data;

    dbus_error_init(&derror);

    if (!dbus_message_is_method_call(message, ROXTERM_DBUS_INTERFACE,
                ROXTERM_DBUS_METHOD_NAME))
    {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    dbus_message_iter_init(message, &iter);
    env = rtdbus_get_message_arg_string_array(&iter);
    argv = rtdbus_get_message_args_as_strings(&iter);
    for (argc = 0; argv && argv[argc]; ++argc)
    {
        if (g_str_has_prefix(argv[argc], "--display="))
            display = argv[argc] + 10;
        else if (!strcmp(argv[argc], "--display") && argv[argc + 1])
            display = argv[argc + 1];
    }
    if (argc)
    {
        global_options_preparse_argv_for_execute(&argc, argv, TRUE);
        global_options_init(&argc, &argv, FALSE);
    }
    roxterm_launch(display, env);

    g_strfreev(argv);
    g_strfreev(env);

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

gboolean listen_for_new_term(void)
{
    return rtdbus_start_service(ROXTERM_DBUS_NAME,
            ROXTERM_DBUS_OBJECT_PATH,
            new_term_listener, global_options_lookup_int("replace") > 0);
}

static char *abs_bin(const char *filename)
{
    if (g_path_is_absolute(filename))
    {
        return g_strdup(filename);
    }
    return g_build_filename(global_options_bindir, "roxterm", NULL);
}

static int wait_for_child(int pipe_r)
{
    char result;

    if (read(pipe_r, &result, 1) != 1)
    {
        g_error(_("Parent failed to read signal from child for --fork: %s"),
                strerror(errno));
        result = 1;
    }
    close(pipe_r);
    return (int) result;
}

/* Only single-byte exit codes are supported */
static int roxterm_exit(int pipe_w, char exit_code)
{
    if (global_options_fork)
    {
        if (write(pipe_w, &exit_code, 1) != 1)
        {
            g_error(_("Child failed to signal parent for --fork: %s"),
                    strerror(errno));
            exit_code = 1;
        }
        close(pipe_w);
    }
    return exit_code;
}

static gboolean roxterm_idle_ok(gpointer ppipe)
{
    roxterm_exit(*(int *) ppipe, 0);
    return FALSE;
}

static DBusHandlerResult
roxterm_dbus_service_ready(DBusConnection * connection,
        DBusMessage * message, void *user_data)
{
    const char *service_name = NULL;
    DBusError derror;

    (void) connection;

    dbus_error_init(&derror);
    dbus_message_get_args(message, &derror,
            DBUS_TYPE_STRING, &service_name,
            DBUS_TYPE_INVALID);
    if (!service_name)
    {
        rtdbus_warn(&derror, "NULL service name in service_ready handler");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    else if (!strcmp(service_name, ROXTERM_DBUS_NAME))
    {
        //g_debug("We acquired service name '%s'", service_name);
        //g_debug("dbus service ready, sending OK down pipe
        roxterm_idle_ok(user_data);
        dbus_connection_remove_filter(connection, roxterm_dbus_service_ready,
                user_data);
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

int main(int argc, char **argv)
{
    gboolean preparse_ok;
    DBusMessage *message = NULL;
    char *display = NULL;
    const char *dpy_name = NULL;
    int n;
    gboolean launched = FALSE;
    gboolean dbus_ok;
    pid_t fork_result = 0;
    static int fork_pipe[2] = { -1, -1};
    gboolean defer_pipe = FALSE;
    const char *session_leafname;
    char *session_filename;

    global_options_init_appdir(argc, argv);
    global_options_init_bindir(argv[0]);
    if (global_options_fork)
    {
        if (pipe(fork_pipe))
        {
            g_error(_("Unable to open pipe to implement --fork: %s"),
                    strerror(errno));
        }
        if ((fork_result = fork()) > 0)
        {
            /* parent doesn't write */
            close(fork_pipe[1]);
            return wait_for_child(fork_pipe[0]);
        }
        else if (!fork_result)
        {
            /* child doesn't read */
            close(fork_pipe[0]);
        }
        else
        {
            g_error(_("Unable to fork to implement --fork: %s"),
                    strerror(errno));
        }
    }
#if ENABLE_SM
    if (!global_options_disable_sm)
    {
        session_argc = argc;
        session_argv = g_new(char *, argc + 1);
        if (global_options_appdir)
        {
            session_argv[0] = g_build_filename(global_options_appdir,
                    "AppRun", NULL);
            if (!g_file_test(session_argv[0], G_FILE_TEST_IS_EXECUTABLE))
            {
                g_free(session_argv[0]);
                session_argv[0] = abs_bin(argv[0]);
            }
        }
        else
        {
            session_argv[0] = abs_bin(argv[0]);
        }
        for (n = 1; n < argc; ++n)
        {
            session_argv[n] = g_strdup(argv[n]);
        }
        /* FIXME: NULL terminator (and argc + 1 above) only necessary if
         * debugging
         */
        session_argv[n] = NULL;
    }
#endif
    g_set_application_name(PACKAGE);
    preparse_ok = global_options_preparse_argv_for_execute(&argc, argv, FALSE);

#if ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, global_options_appdir ?
            g_strdup_printf("%s/build/locale", global_options_appdir) :
            LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
    textdomain(PACKAGE);
#endif

    /* Get --display option before GTK removes it */
    for (n = 1; n < argc; ++n)
    {
        if (strlen(argv[n]) > 10 && g_str_has_prefix(argv[n], "--display="))
        {
            display = g_strdup(argv[n]);
            dpy_name = display + 10;
            break;
        }
        else if (n < argc - 1 && !strcmp(argv[n], "--display"))
        {
            display = g_strdup_printf("--display=%s", argv[n + 1]);
            dpy_name = display + 10;
            break;
        }
    }

    gtk_init(&argc, &argv);
    if (!preparse_ok)
    {
        /* Only one possible reason for failure */
        dlg_critical(NULL, _("Missing command after -e/--execute option"));
        return 1;
    }

    if (!display)
    {
        dpy_name = gdk_display_get_name(gdk_display_get_default());
        display = g_strdup_printf("--display=%s", dpy_name);
    }

    /* Have to create message with args from argv before parsing them */
    dbus_ok = rtdbus_ok = rtdbus_init();
    if (dbus_ok)
    {
        if (global_options_fork)
        {
            rtdbus_add_rule_and_filter("type='signal',"
                    "interface='org.freedesktop.DBus',"
                    "member='NameAcquired',"
                    "path='/org/freedesktop/DBus',"
                    "sender='org.freedesktop.DBus'",
                roxterm_dbus_service_ready,
                &fork_pipe[1]);
            defer_pipe = TRUE;
        }
#if ENABLE_SM
        if (!global_options_user_session_id && (global_options_disable_sm ||
                (!global_options_restart_session_id &&
                !global_options_clone_session_id)))
#else
        if (!global_options_user_session_id)
#endif
        {
            message = create_dbus_message(argc, argv, display);
        }
    }
    g_free(display);

    global_options_init(&argc, &argv, TRUE);
    global_options_apply_dark_theme();

    if (dbus_ok)
    {
        dbus_ok = listen_for_new_term();
        /* Only TRUE if another roxterm is providing the service */
    }

    dbus_ok = global_options_lookup_int("separate") <= 0 && dbus_ok
            && !global_options_user_session_id
#if ENABLE_SM
            && (global_options_disable_sm ||
                (!global_options_restart_session_id
                && !global_options_clone_session_id))
#endif
            ;

    if (dbus_ok)
    {
        switch (run_via_dbus(message))
        {
            case 0:
                return roxterm_exit(fork_pipe[1], 0);
            case 1:
                return roxterm_exit(fork_pipe[1], 1);
            case -1:
                /* DBUS failure, run as if --separate */
                break;
        }
    }
    else if (message)
    {
        dbus_message_unref(message);
    }


    roxterm_init();
    multi_win_set_role_prefix("roxterm");
#if ENABLE_SM
    if (!global_options_disable_sm)
    {
        if (global_options_restart_session_id)
            launched = session_load(global_options_restart_session_id);
        if (!launched && global_options_clone_session_id)
            launched = session_load(global_options_clone_session_id);
    }
#endif

    session_leafname = global_options_user_session_id ?
            global_options_user_session_id : "Default";
    session_filename = session_get_filename(session_leafname,
            "UserSessions", FALSE);
    if (g_file_test(session_filename, G_FILE_TEST_IS_REGULAR))
    {
        launched = load_session_from_file(session_filename, session_leafname);
    }
    else if (global_options_user_session_id)
    {
        g_critical("Session file '%s' not found", session_filename);
    }
    if (!launched)
    {
        roxterm_launch(dpy_name, environ);
    }

#if ENABLE_SM
    if (!global_options_disable_sm)
    {
        session_init(global_options_restart_session_id);
    }
#endif

    /* Usually this should be deferred to NameAcquired signal handler */
    if (!defer_pipe)
    {
        g_idle_add(roxterm_idle_ok, &fork_pipe[1]);
    }

    SLOG("Entering main loop with %d windows", g_list_length(multi_win_all));
    gtk_main();

    SLOG("Exiting normally");

    return 0;
}

/* vi:set sw=4 ts=4 et cindent cino= */
