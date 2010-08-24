/*
    roxterm - GTK+ 2.0 terminal emulator with tabs
    Copyright (C) 2004 Tony Houghton <h@realh.co.uk>

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

#include <locale.h>

#include <gdk/gdk.h>

#include "dlg.h"
#include "globalopts.h"
#include "multitab.h"
#include "roxterm.h"
#include "rtdbus.h"
#if ENABLE_SM
#include "session.h"
#endif

/* List of all terminal windows managed by this app */
static GList *main_all_windows = NULL;

static gboolean main_quit_handler(gpointer data)
{
    (void) data;

    /* FIXME: Destroy any windows left? */

    return FALSE;
}

#define ROXTERM_DBUS_NAME RTDBUS_NAME ".term"
#define ROXTERM_DBUS_OBJECT_PATH RTDBUS_OBJECT_PATH "/term"
#define ROXTERM_DBUS_INTERFACE RTDBUS_INTERFACE
#define ROXTERM_DBUS_METHOD_NAME "NewTerminal"

static DBusMessage *create_dbus_message(int argc, char **argv,
        const char *display)
{
    DBusMessage *message;
    int n;

    message = rtdbus_method_new(ROXTERM_DBUS_NAME, ROXTERM_DBUS_OBJECT_PATH,
            ROXTERM_DBUS_INTERFACE,
            ROXTERM_DBUS_METHOD_NAME, DBUS_TYPE_INVALID);
    for (n = 0; message && n < argc; ++n)
    {
        message = rtdbus_append_args(message,
                DBUS_TYPE_STRING, RTDBUS_ARG(argv[n]),
                DBUS_TYPE_INVALID);
    }
    if (display)
    {
        message = rtdbus_append_args(message,
                DBUS_TYPE_STRING, RTDBUS_ARG(display),
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
    if (!global_options_directory)
    {
        char *cwd = GET_CURRENT_DIR();
        const char *d = "-d";

        message = rtdbus_append_args(message,
                DBUS_TYPE_STRING, RTDBUS_ARG(d), 
                DBUS_TYPE_STRING, RTDBUS_ARG(cwd),
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
                DBUS_TYPE_STRING, RTDBUS_ARG(e), DBUS_TYPE_INVALID);
        if (!message)
            return -1;
        for (n = 0; global_options_commandv[n]; ++n)
        {
            message = rtdbus_append_args(message,
                    DBUS_TYPE_STRING, RTDBUS_ARG(global_options_commandv[n]),
                    DBUS_TYPE_INVALID);
            if (!message)
                return -1;
        }
    }
    if (!result)
        result = rtdbus_send_message_with_reply(message) ? 0 : 1;
    return result;
}

static DBusHandlerResult new_term_listener(DBusConnection *connection,
        DBusMessage *message, void *user_data)
{
    DBusError derror;
    char **argv;
    int argc;
    int n;
    const char *display = NULL;

    dbus_error_init(&derror);

    if (!dbus_message_is_method_call(message, ROXTERM_DBUS_INTERFACE,
                ROXTERM_DBUS_METHOD_NAME))
    {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;    
    }
    argv = rtdbus_get_message_args_as_strings(message);
    for (argc = 0; argv && argv[argc]; ++argc)
    {
        if (g_str_has_prefix(argv[argc], "--display="))
            display = argv[argc] + 10;
    }
    if (argc)
    {
        global_options_preparse_argv_for_execute(&argc, argv, TRUE);
        global_options_init(&argc, &argv, FALSE);
    }
    roxterm_launch(display, message);

    for (n = 0; n < argc; ++n)
        g_free(argv[n]);
    g_free(argv);
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;    
}

gboolean listen_for_new_term(void)
{
    return rtdbus_start_service(ROXTERM_DBUS_NAME,
            ROXTERM_DBUS_OBJECT_PATH,
            new_term_listener, global_options_lookup_int("replace") > 0);
}

static const char *abs_bin(const char *filename)
{
    if (g_path_is_absolute(filename))
    {
        return filename;
    }
    return g_build_filename(global_options_bindir, "roxterm", NULL);
}

int main(int argc, char **argv)
{
    gboolean preparse_ok;
    DBusMessage *message = NULL;
    char *display = NULL;
    const char *dpy_name;
    int n;
    gboolean launched = FALSE;
    gboolean dbus_ok;

    g_set_application_name(PACKAGE);
    preparse_ok = global_options_preparse_argv_for_execute(&argc, argv, FALSE);
    
#if ENABLE_NLS
    global_options_init_appdir(argc, argv);
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, global_options_appdir ?
            g_strdup_printf("%s/locale", global_options_appdir) : LOCALEDIR);
    textdomain(PACKAGE);
#endif

    /* Get --display option before GTK removes it */
    for (n = 1; n < argc; ++n)
    {
        if (strlen(argv[n]) > 10 && g_str_has_prefix(argv[n], "--display="))
        {
            display = g_strdup(argv[n]); 
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
    
    dpy_name = gdk_display_get_name(gdk_display_get_default());
    if (!display)
        display = g_strdup_printf("--display=%s", dpy_name);
    
    /* Have to create message with args from argv before parsing them */
    dbus_ok = rtdbus_ok = rtdbus_init();
    if (dbus_ok)
    {
#if ENABLE_SM
        if (global_options_disable_sm ||
                (!global_options_restart_session_id &&
                !global_options_clone_session_id))
        {
            message = create_dbus_message(argc, argv, display);
        }
#else
        message = create_dbus_message(argc, argv, display);
#endif
    }
    g_free(display);
    
    global_options_init(&argc, &argv, TRUE);

    if (dbus_ok)
    {
        dbus_ok = listen_for_new_term();
        /* Only TRUE if another roxterm is providing the service */
    }
    
    dbus_ok = global_options_lookup_int("separate") <= 0 && dbus_ok
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
                return 0;
            case 1:
                return 1;
            case -1:
                /* DBUS failure, run as if --separate */
                break;
        }
    }
    else if (message)
    {
        dbus_message_unref(message);
    }

    gtk_quit_add(0, main_quit_handler, main_all_windows);

    roxterm_init();
#if ENABLE_SM
    if (!global_options_disable_sm)
    {
        if (global_options_restart_session_id)
            launched = session_load(global_options_restart_session_id);
        if (!launched && global_options_clone_session_id)
            launched = session_load(global_options_clone_session_id);
    }
#endif
    multi_win_set_role_prefix("roxterm");
    if (!launched)
    {
        roxterm_launch(dpy_name, NULL);
    }

#if ENABLE_SM
    if (!global_options_disable_sm)
    {
        if (global_options_appdir)
        {
            session_arg0 = g_build_filename(global_options_appdir,
                    "AppRun", NULL);
            if (!g_file_test(session_arg0, G_FILE_TEST_IS_EXECUTABLE))
            {
                g_free((char *) session_arg0);
                session_arg0 = abs_bin(argv[0]);
            }
        }
        else
        {
            session_arg0 = abs_bin(argv[0]);
        }
        session_init(global_options_restart_session_id);
    }
#endif

    gtk_main();

    return 0;
}

/* vi:set sw=4 ts=4 et cindent cino= */
