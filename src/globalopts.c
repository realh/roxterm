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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dlg.h"
#include "globalopts.h"
#include "version.h"

Options *global_options = NULL;

char **global_options_commandv = NULL;
char *global_options_appdir = NULL;
char *global_options_bindir = NULL;
char *global_options_directory = NULL;
char *global_options_restart_session_id = NULL;
char *global_options_clone_session_id = NULL;
gboolean global_options_disable_sm = FALSE;
gboolean global_options_replace = FALSE;
gboolean global_options_fullscreen = FALSE;
gboolean global_options_maximise = FALSE;
gboolean global_options_tab = FALSE;
gboolean global_options_fork = FALSE;

static void correct_scheme(const char *bad_name, const char *good_name)
{
    char *val = global_options_lookup_string(bad_name);

    if (!val)
        return;
    if (global_options_lookup_string(good_name))
        return;
    /*g_print("Converting %s to %s (%s)\n", bad_name, good_name, val);*/
    options_set_string(global_options, good_name, val);
}

inline static void correct_schemes(void)
{
    correct_scheme("colour-scheme", "colour_scheme");
    correct_scheme("shortcut-scheme", "shortcut_scheme");
}

static void global_options_reset(void)
{
    g_free(global_options_directory);
    global_options_directory = NULL;
    global_options_replace = FALSE;
    global_options_fullscreen = FALSE;
    global_options_tab = FALSE;
    global_options_disable_sm = FALSE;
    options_reload_keyfile(global_options);
    correct_schemes();
}

static const char *process_option_name(const char *name);

static gboolean global_options_show_usage(const gchar *option_name,
        const gchar *value, gpointer data, GError **error)
{
    puts("roxterm [-?|--help] [--usage] [--geometry=GEOMETRY] [--appdir=DIR]\n"
      "    [--profile=PROFILE|-p PROFILE] \n"
      "    [--colour-scheme=SCHEME|--color-scheme=SCHEME|-c SCHEME]\n"
      "    [--shortcut-scheme=SCHEME|-s SCHEME]\n"
      "    [--fullscreen|-f] [--maximise|--maximize|-m] [--zoom=ZOOM|-z ZOOM]\n"
      "    [--title=TITLE|-T TITLE] [--tab-name=NAME|-n NAME]\n"
      "    [--separate] [--replace] [--tab]\n"
      "    [--directory=DIRECTORY|-d DIRECTORY] [--disable-sm]\n"
      "    [--show-menubar] [--hide-menubar]\n"
      "    [--fork]\n"
      "    [--role=ROLE]\n"
      "    [--no-geometry]\n"
      "    [-e|--execute COMMAND]\n");
    exit(0);
    return TRUE;
}

static gboolean global_options_swallow_execute(const gchar *option_name,
        const gchar *value, gpointer data, GError **error)
{
    return TRUE;
}

static gboolean global_options_set_string(const gchar *option_name,
        const gchar *value, gpointer data, GError **error)
{
    option_name = process_option_name(option_name);
    if (!strcmp(option_name, "colour-scheme")
        || !strcmp(option_name, "color-scheme")
        || !strcmp(option_name, "color_scheme"))
    {
        option_name = "colour_scheme";
    }
    else if (!strcmp(option_name, "shortcut-scheme"))
    {
        option_name = "shortcut_scheme";
    }
    options_set_string(global_options, option_name, value);
    return TRUE;
}

static gboolean global_options_set_directory(const gchar *option_name,
        const gchar *value, gpointer data, GError **error)
{
    if (g_path_is_absolute(value))
    {
        global_options_directory = g_strdup(value);
    }
    else
    {
        char *cwd = GET_CURRENT_DIR();

        global_options_directory = g_build_filename(cwd, value, NULL);
        FREE_CURRENT_DIR(cwd);
    }
    if (!g_file_test(global_options_directory, G_FILE_TEST_IS_DIR))
    {
        g_set_error(error, ROXTERM_ARG_ERROR, ROXTermArgError,
            _("Invalid directory '%s'"), global_options_directory);
        g_free(global_options_directory);
        global_options_directory = NULL;
        return FALSE;
    }
    return TRUE;
}

static gboolean global_options_set_bool(const gchar *option_name,
        const gchar *value, gpointer data, GError **error)
{
    gboolean val = TRUE;
    
    option_name = process_option_name(option_name);
    if (!strcmp(option_name, "show-menubar"))
    {
        option_name = "hide_menubar";
        val = FALSE;
    }
    else if (!strcmp(option_name, "hide-menubar"))
    {
        option_name = "hide_menubar";
    }
    options_set_int(global_options, option_name, val);
    return TRUE;
}

static gboolean global_options_set_double(const gchar *option_name,
        const gchar *value, gpointer data, GError **error)
{
    option_name = process_option_name(option_name);
    options_set_double(global_options, option_name, atof(value));
    return TRUE;
}

static GOptionEntry global_g_options[] = {
    { "usage", 'u', G_OPTION_FLAG_IN_MAIN | G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, global_options_show_usage,
        N_("Show brief usage message"), NULL },
    { "directory", 'd', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, global_options_set_directory,
        N_("Set the terminal's working directory"), N_("DIRECTORY") },
    { "geometry", 0, G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, global_options_set_string,
        N_("Set size and/or position of terminal\n"
        "                                   according to X geometry "
        "specification"),
        N_("GEOMETRY") },
    { "appdir", 0,  G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, global_options_set_string,
        N_("Application directory when run as a ROX\n"
        "                                   application"),
        N_("DIRECTORY") },
    { "show-menubar", 0, G_OPTION_FLAG_IN_MAIN | G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, global_options_set_bool,
        N_("Show the menu bar, overriding profile"), NULL },
    { "hide-menubar", 0, G_OPTION_FLAG_IN_MAIN | G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, global_options_set_bool,
        N_("Hide the menu bar, overriding profile"), NULL },
    { "profile", 'p', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, global_options_set_string,
        N_("Use the named profile"), N_("PROFILE") },
    { "colour-scheme", 'c', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, global_options_set_string,
        N_("Use the named colour scheme"),
        N_("SCHEME") },
    { "color-scheme", 0, G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, global_options_set_string,
        N_("Use the named colour scheme\n"
        "                                   (same as --colour-scheme)"),
        N_("SCHEME") },
    { "shortcut-scheme", 's', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, global_options_set_string,
        N_("Use the named keyboard shortcut scheme"),
        N_("SCHEME") },
    { "colour_scheme", 'c', G_OPTION_FLAG_HIDDEN,
        G_OPTION_ARG_CALLBACK, global_options_set_string, NULL, NULL },
    { "shortcut_scheme", 'c', G_OPTION_FLAG_HIDDEN,
        G_OPTION_ARG_CALLBACK, global_options_set_string, NULL, NULL },
    { "maximise", 'm', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_NONE, &global_options_maximise,
        N_("Maximise the window, overriding profile"), NULL },
    { "maximize", 0, G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_NONE, &global_options_maximise,
        N_("Synonym for --maximise"), NULL },
    { "fullscreen", 'f', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_NONE, &global_options_fullscreen,
        N_("Make the initial terminal take up the whole\n"
        "                                   screen with no window furniture"),
        NULL },
    { "zoom", 'z', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, global_options_set_double,
        N_("Scale factor for terminal's fonts\n"
        "                                   (1.0 is normal)"),
        N_("ZOOM") },
    { "separate", 0, G_OPTION_FLAG_IN_MAIN | G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, global_options_set_bool,
        N_("Use a separate process to run this terminal"), NULL },
    { "replace", 0, G_OPTION_FLAG_IN_MAIN | G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, global_options_set_bool,
        N_("Replace any existing process as ROXTerm's\n"
        "                                   D-BUS service provider"),
        NULL },
    { "title", 'T', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, global_options_set_string,
        N_("Set window title"), N_("TITLE") },
    { "tab-name", 'n', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, global_options_set_string,
        N_("Set tab name"), N_("NAME") },
    { "tab", 0, G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_NONE, &global_options_tab,
        N_("Open a tab in an existing window instead of\n"
        "                                   a new window if possible"),
        NULL },
    { "fork", 0, G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_NONE, &global_options_fork,
        N_("Fork into the background even if this is the first instance"),
        NULL },
    { "disable-sm", 0, G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_NONE, &global_options_disable_sm,
        N_("Disable session management"), NULL },
    { "role", 0, G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, global_options_set_string,
        N_("Set X window system 'role' hint"), N_("NAME") },
    { "restart-session-id", 0, G_OPTION_FLAG_HIDDEN,
        G_OPTION_ARG_STRING, &global_options_restart_session_id, NULL, NULL },
    { "clone-session-id", 0, G_OPTION_FLAG_HIDDEN,
        G_OPTION_ARG_STRING, &global_options_clone_session_id, NULL, NULL },
#if GTK_CHECK_VERSION(3, 0, 0)
    { "no-geometry", 0, G_OPTION_FLAG_IN_MAIN | G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, global_options_set_bool,
        N_("Don't set window geometry hints. This is a workaround for "
        "<https://bugzilla.gnome.org/show_bug.cgi?id=649680>"), NULL },
#endif
    { "execute", 'e', G_OPTION_FLAG_IN_MAIN | G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, global_options_swallow_execute,
        N_("Execute remainder of command line inside the\n"
        "                                   terminal. "
        "Must be the final option."),
        NULL },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
};

gboolean global_options_preparse_argv_for_execute(int *argc, char **argv,
        gboolean shallow_copy)
{
    int n;

    if (global_options_commandv)
    {
        g_strfreev(global_options_commandv);
        global_options_commandv = NULL;
    }
    for (n = 0; n < *argc; ++n)
    {
        if (!strcmp(argv[n], "--execute") || !strcmp(argv[n], "-e"))
        {
            int m;
            int old_argc = *argc;

            if (n == *argc - 1)
                return FALSE;

            if (shallow_copy)
                g_free(argv[n]);
            
            global_options_commandv = g_new(char *, *argc - n);

            *argc = n;
            argv[n++] = NULL;
            for (m = 0; n < old_argc; ++m, ++n)
            {
                global_options_commandv[m]
                    = shallow_copy ? argv[n] : g_strdup(argv[n]);
            }
            global_options_commandv[m] = NULL;
            break;
        }
    }
    return TRUE;
}

void global_options_init_appdir(int argc, char **argv)
{
    int n;

    for (n = 1; n < argc; ++n)
    {
        if (g_str_has_prefix(argv[n], "--appdir=") && strlen(argv[n]) > 9)
        {
            global_options_appdir = g_strdup(argv[n] + 9);
        }
        else if (!strcmp(argv[n], "--fork"))
        {
            global_options_fork = TRUE;
        }
    }
}

static void global_options_parse_argv(int *argc, char ***argv, gboolean report)
{
    GOptionContext *octx = g_option_context_new(NULL);
    GOptionGroup *ogroup = gtk_get_option_group(FALSE);
    GError *err = NULL;

#if ENABLE_NLS
    int n;
    GOptionEntry *o;
    
    for (n = 0; o = &global_g_options[n], o->long_name; ++n)
    {
        if (o->description)
            o->description = gettext(o->description);
        if (o->arg_description)
            o->arg_description = gettext(o->arg_description);
    }
#endif

    g_option_context_set_help_enabled(octx, TRUE);
    g_option_context_add_group(octx, ogroup);
    g_option_context_add_main_entries(octx, global_g_options, NULL);
    g_option_context_parse(octx, argc, argv, &err);
    if (err)
    {
        if (report)
        {
            dlg_warning(NULL, _("Error parsing command line options: %s"),
                    err->message);
        }
        g_error_free(err);
    }
}

static const char *process_option_name(const char *name)
{
    while (*name == '-')
        ++name;
    if (strlen(name) == 1)
    {
        int n;
        GOptionEntry *e;
    
        for (n = 0; e = &global_g_options[n], e->long_name; ++n)
        {
            if (*name == e->short_name)
                return e->long_name;
        }
    }
    return name;
}

void global_options_init_bindir(const char *argv0)
{
    global_options_bindir = g_path_get_dirname(argv0);
    if (!strcmp(global_options_bindir, "."))
    {
        /* No directory component, try to find roxterm-config in BIN_DIR,
         * current dir, then PATH */
        char *capplet;

        g_free(global_options_bindir);
        global_options_bindir = NULL;
        capplet = g_build_filename(BIN_DIR, "roxterm-config", NULL);
        if (g_file_test(capplet, G_FILE_TEST_IS_EXECUTABLE))
        {
            global_options_bindir = g_strdup(BIN_DIR);
        }
        else if (g_file_test("roxterm-config", G_FILE_TEST_IS_EXECUTABLE))
        {
            /* Paranoia in case g_free() != free() */
            char *bindir = GET_CURRENT_DIR();
            
            global_options_bindir = g_strdup(bindir);
            FREE_CURRENT_DIR(bindir);
        }
        else
        {
            char *full = g_find_program_in_path("roxterm-config");
            
            if (full)
            {
                global_options_bindir = g_path_get_dirname(full);
                g_free(full);
            }
            else
            {
                dlg_critical(NULL, _("Can't find roxterm-config"));
            }
        }
    }
    else if (!g_path_is_absolute(global_options_bindir))
    {
        /* Partial path, must be relative to current dir */
        char *cur = GET_CURRENT_DIR();
        char *full = g_build_filename(cur, global_options_bindir, NULL);

        FREE_CURRENT_DIR(cur);
        g_free(global_options_bindir);
        global_options_bindir = full;
    }
    /* else full path was given, use that */
}

void global_options_init(int *argc, char ***argv, gboolean report)
{
    static gboolean already = FALSE;

    if (already)
    {
        global_options_reset();
    }
    else
    {
        /* Need to get appdir before trying to load any options files */
        if (!global_options_appdir)
            global_options_init_appdir(*argc, *argv);
        global_options = options_open("Global", "roxterm options");
        correct_schemes();
    }
    global_options_parse_argv(argc, argv, report);
    if (!global_options_bindir)
    {
        global_options_init_bindir((*argv)[0]);
    }
    already = TRUE;
}

char **global_options_copy_strv(char **ps)
{
    char **ps2;
    int n;

    for (n = 0; ps[n]; ++n);
    ps2 = g_new(char *, n + 1);
    for (; n >= 0; --n)
        ps2[n] = ps[n] ? g_strdup(ps[n]) : NULL;
    return ps2;
}

/* vi:set sw=4 ts=4 noet cindent cino= */
