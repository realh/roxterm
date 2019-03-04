/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2019 Tony Houghton <h@realh.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

#include "roxterm-launch-params.h"

RoxtermTabLaunchParams *roxterm_tab_launch_params_new(void)
{
    RoxtermTabLaunchParams *tp = g_new0(RoxtermTabLaunchParams, 1);
    return tp;
}

void roxterm_tab_launch_params_free(RoxtermTabLaunchParams *tp)
{
    g_free(tp->profile_name);
    g_free(tp->tab_title);
    g_free(tp->directory);
    g_free(tp);
}


RoxtermWindowLaunchParams *roxterm_window_launch_params_new(void)
{
    RoxtermWindowLaunchParams *wp = g_new0(RoxtermWindowLaunchParams, 1);
    wp->zoom = 1.0;
    return wp;
}

void roxterm_window_launch_params_free(RoxtermWindowLaunchParams *wp)
{
    if (wp->tabs)
    {
        g_list_free_full(wp->tabs,
                (GDestroyNotify) roxterm_tab_launch_params_free);
    }
    g_free(wp->window_title);
    g_free(wp->role);
    g_free(wp->geometry_str);
#if ENABLE_VIM
    g_free(wp->vim_cmd);
#endif
    g_free(wp);
}

RoxtermLaunchParams *roxterm_launch_params_new(void)
{
    RoxtermLaunchParams *lp = g_new0(RoxtermLaunchParams, 1);
    return lp;
}

void roxterm_launch_params_free(RoxtermLaunchParams *lp)
{
    if (lp->windows)
    {
        g_list_free_full(lp->windows,
                (GDestroyNotify) roxterm_window_launch_params_free);
    }
    if (lp->argv)
        g_strfreev(lp->argv);
    if (lp->env)
        g_strfreev(lp->env);
    g_free(lp);
}

static RoxtermWindowLaunchParams *
roxterm_launch_params_new_window(RoxtermLaunchParams *lp)
{
    RoxtermWindowLaunchParams *wp = roxterm_window_launch_params_new();
    lp->windows = g_list_prepend(lp->windows, wp);
    return wp;
}

static RoxtermWindowLaunchParams *
roxterm_launch_params_current_window(RoxtermLaunchParams *lp)
{
    RoxtermWindowLaunchParams *wp;
    if (!lp->windows)
        wp = g_list_first(lp->windows)->data;
    else
        wp = roxterm_launch_params_new_window(lp);
    return wp;
}

static RoxtermTabLaunchParams *
roxterm_window_launch_params_new_tab(RoxtermWindowLaunchParams *wp)
{
    RoxtermTabLaunchParams *tp = roxterm_tab_launch_params_new();
    wp->tabs = g_list_prepend(wp->tabs, tp);
    return tp;
}

static RoxtermTabLaunchParams *
roxterm_launch_params_current_tab(RoxtermLaunchParams *lp)
{
    RoxtermWindowLaunchParams *wp = roxterm_launch_params_current_window(lp);
    RoxtermTabLaunchParams *tp;
    if (wp->tabs)
    {
        tp = g_list_first(wp->tabs)->data;
    }
    else
    {
        tp = roxterm_window_launch_params_new_tab(wp);
    }
    return tp;
}

static gboolean roxterm_launch_value_error(const gchar *option, GError **error)
{
    if (error)
    {
        *error = g_error_new(G_OPTION_ERROR,
                G_OPTION_ERROR_BAD_VALUE,
                "Missing value for option '--%s'", option);
    }
    return FALSE;
}

static gboolean roxterm_launch_number_error(const gchar *option, GError **error)
{
    if (error)
    {
        *error = g_error_new(G_OPTION_ERROR,
                G_OPTION_ERROR_BAD_VALUE,
                "Value for option '--%s' should be a number", option);
    }
    return FALSE;
}

static gboolean roxterm_launch_bool_error(const gchar *option, GError **error)
{
    if (error)
    {
        *error = g_error_new(G_OPTION_ERROR,
                G_OPTION_ERROR_BAD_VALUE,
                "Option '--%s' should not take a value", option);
    }
    return FALSE;
}

static void roxterm_launch_params_check_dup(const gchar *option,
        gpointer value, gboolean free_old)
{
    if (value)
    {
        g_warning("--%s was specified more than once for the same context",
                option);
        if (free_old)
            g_free(value);
    }
}

static void roxterm_launch_params_set_string(const gchar *option,
        gchar **pval, const gchar *new_value)
{
    roxterm_launch_params_check_dup(option, *pval, TRUE);
    *pval = g_strdup(new_value);
}

static void roxterm_launch_params_set_boolean(const gchar *option,
        gboolean *pval)
{
    roxterm_launch_params_check_dup(option, GINT_TO_POINTER(*pval), FALSE);
    *pval = TRUE;
}

static gboolean roxterm_launch_params_parse_tab_option(const gchar *option,
        const gchar *value, gpointer data, GError **error)
{
    RoxtermLaunchParams *lp = data;
    if (!lp)
        return TRUE;
    if (!value)
        return roxterm_launch_value_error(option, error);
    RoxtermTabLaunchParams *tp = roxterm_launch_params_current_tab(lp);
    if (!strcmp(option, "profile"))
        roxterm_launch_params_set_string(option, &tp->profile_name, value);
    else if (!strcmp(option, "tab-name"))
        roxterm_launch_params_set_string(option, &tp->tab_title, value);
    else if (!strcmp(option, "directory"))
        roxterm_launch_params_set_string(option, &tp->directory, value);
#if ENABLE_VIM
    else if (!strcmp(option, "vim"))
        roxterm_launch_params_set_boolean(option, &tp->vim);
#endif
    return TRUE;
}

static gboolean roxterm_launch_params_parse_geometry(const char *geometry,
        int *columns, int *rows, GError **error)
{
    gboolean result = FALSE;
    const char *x = strchr(geometry, 'x');
    if (!x)
        x = strchr(geometry, 'X');
    if (x)
    {
        char *g2 = g_strdup(geometry);
        g2[x - geometry] = 0;
        const char *r = g2 + (x - geometry + 1);
        if (sscanf(g2, "%d", columns) == 1)
            result = (sscanf(r, "%d", rows) == 1);
        g_free(g2);
    }
    if (!result && error)
    {
        *error = g_error_new(G_OPTION_ERROR,
                G_OPTION_ERROR_BAD_VALUE,
                "Invalid geometry");
    }
    return result;
}

static gboolean
roxterm_launch_params_parse_window_str_option(const gchar *option,
        const gchar *value, gpointer data, GError **error)
{
    if (!value)
        return roxterm_launch_value_error(option, error);
    RoxtermLaunchParams *lp = data;
    if (!lp)
        return TRUE;
    RoxtermWindowLaunchParams *wp = roxterm_launch_params_current_window(lp);
    if (!strcmp(option, "window-title"))
        roxterm_launch_params_set_string(option, &wp->window_title, value);
    else if (!strcmp(option, "role"))
        roxterm_launch_params_set_string(option, &wp->role, value);
    else if (!strcmp(option, "geometry"))
    {
        roxterm_launch_params_set_string(option, &wp->geometry_str, value);
        if (!roxterm_launch_params_parse_geometry(wp->geometry_str,
                &wp->columns, &wp->rows, error))
        {
            return FALSE;
        }
    }
    else if (!strcmp(option, "zoom"))
    {
        roxterm_launch_params_set_string(option, &wp->zoom_str, value);
        if (sscanf(wp->zoom_str, "%lf", &wp->zoom) < 1)
            return roxterm_launch_number_error(option, error);
    }
#if ENABLE_VIM
    else if (!strcmp(option, "vim-cmd"))
        roxterm_launch_params_set_string(option, &wp->vim_cmd, value);
#endif
    return TRUE;
}

static gboolean
roxterm_launch_params_parse_window_bool_option(const gchar *option,
        const gchar *value, gpointer data, GError **error)
{
    if (value)
        return roxterm_launch_bool_error(option, error);
    RoxtermLaunchParams *lp = data;
    if (!lp)
        return TRUE;
    RoxtermWindowLaunchParams *wp = roxterm_launch_params_current_window(lp);
    if (!strcmp(option, "maximize") || !strcmp(option, "maximise"))
        roxterm_launch_params_set_boolean(option, &wp->maximized);
    else if (!strcmp(option, "fullscreen"))
        roxterm_launch_params_set_boolean(option, &wp->fullscreen);
    return TRUE;
}

static gboolean
roxterm_launch_params_new_tab_or_window(const gchar *option,
        const gchar *value, gpointer data, GError **error)
{
    if (value)
        return roxterm_launch_bool_error(option, error);
    RoxtermLaunchParams *lp = data;
    if (!lp)
        return TRUE;
    if (!strcmp(option, "tab"))
    {
        gboolean implicit = lp->windows == NULL;
        RoxtermWindowLaunchParams *wp
                = roxterm_launch_params_current_window(lp);
        wp->implicit = implicit;
        roxterm_window_launch_params_new_tab(wp);
    }
    else
    {
        roxterm_launch_params_new_window(lp);
    }
    return TRUE;
}

gboolean roxterm_launch_params_preparse_argv_execute(RoxtermLaunchParams *lp,
        int *argc, char ***pargv, GError **error)
{
    char **argv = *pargv;
    for (int n = 1; n < *argc; ++n)
    {
        if (!strcmp(argv[n], "--execute") || !strcmp(argv[n], "-e"))
        {
            if (n == *argc - 1)
            {
                if (error)
                {
                    *error = g_error_new(G_OPTION_ERROR,
                            G_OPTION_ERROR_FAILED,
                            "No arguments following %s", argv[n]);
                    if (lp)
                        roxterm_launch_params_free(lp);
                    return FALSE;
                }
            }
            argv[n] = NULL;
            int orig_argc = *argc;
            *argc = n;
            if (lp)
            {
                lp->argv = g_new(gchar *, orig_argc - n);
                int m = 0;
                ++n;
                for (; n < orig_argc; ++n)
                {
                    // Steal from original argv, lp->argv takes ownership
                    lp->argv[m++] = argv[n];
                    argv[n] = NULL;
                }
                lp->argv[m] = NULL;
                lp->argc = m;
            }
            else
            {
                for (; n < orig_argc; ++n)
                {
                    g_free(argv[n]);
                    argv[n] = NULL;
                }
            }
            break;
        }
    }
    return TRUE;
}

#define PADDING "                                "

static GOptionEntry roxterm_launch_params_cli_options[] = {
    /*
    { "usage", 'u', G_OPTION_FLAG_IN_MAIN | G_OPTION_FLAG_NO_ARG,
        G_OPTION_ARG_CALLBACK, global_options_show_usage,
        N_("Show brief usage message"), NULL },
    */
    { "directory", 'd', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_tab_option,
        N_("Set the terminal's working directory"), N_("DIRECTORY") },
    { "geometry", 0, G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_window_str_option,
        N_("Set size of terminal"),
        N_("COLUMNSxROWS") },
    { "profile", 'p', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_tab_option,
        N_("Use the named profile"), N_("PROFILE") },
    { "vim", 'v', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_tab_option,
        N_("This tab is a vim view"), NULL },
    { "maximise", 'm', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_window_bool_option,
        N_("Maximise the window, overriding profile"), NULL },
    { "maximize", 0, G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_window_bool_option,
        N_("Synonym for --maximise"), NULL },
    { "fullscreen", 'f', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_window_bool_option,
        N_("Take up the whole screen with no\n"
        PADDING "window furniture"),
        NULL },
    { "zoom", 'z', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_window_str_option,
        N_("Scale factor for terminal's font\n"
        PADDING "(1.0 is normal)"),
        N_("ZOOM") },
    { "window-title", 'T', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_window_str_option,
        N_("Set window title"), N_("TITLE") },
    { "tab-name", 'n', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_tab_option,
        N_("Set tab name"), N_("NAME") },
    { "tab", 't', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_new_tab_or_window,
        N_("Add a new tab to the current window"),
        NULL },
    { "window", 'w', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_new_tab_or_window,
        N_("Open an additional window"),
        NULL },
    { "role", 0, G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_window_str_option,
        N_("Set X window system 'role' hint"), N_("NAME") },
    { "vim-cmd", 0, G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_CALLBACK, roxterm_launch_params_parse_window_str_option,
        N_("Command for window's vim instance"), N_("'VIM_COMMAND'") },
    { "execute", 'e', G_OPTION_FLAG_IN_MAIN,
        G_OPTION_ARG_NONE, NULL,
        N_("Execute remainder of command line inside the\n"
        PADDING "terminal. "
        "Must be the final option."),
        NULL },
    { NULL, 0, 0, 0, NULL, NULL, NULL }
};

GOptionContext *roxterm_launch_params_get_option_context(gpointer handle)
{
    GOptionContext *octx = g_option_context_new(NULL);
    GOptionGroup *ogroup = gtk_get_option_group(FALSE);

    g_option_context_add_group(octx, ogroup);
    ogroup = g_option_group_new("RoxTerm",
                    N_("RoxTerm options"),
                    N_("RoxTerm options"),
                    handle, NULL);
    g_option_context_set_main_group(octx, ogroup);
    g_option_context_add_main_entries(octx, roxterm_launch_params_cli_options,
            NULL);
    return octx;
}

RoxtermLaunchParams *
roxterm_launch_params_new_from_command_line(GApplicationCommandLine *cmd,
        GError **error)
{
    RoxtermLaunchParams *lp = roxterm_launch_params_new();
    int argc;
    gchar **argv = g_application_command_line_get_arguments(cmd, &argc);
    roxterm_launch_params_preparse_argv_execute(lp, &argc, &argv, error);
    GOptionContext *octx = roxterm_launch_params_get_option_context(lp);
    if (!g_option_context_parse(octx, &argc, &argv, error))
    {
        roxterm_launch_params_free(lp);
        lp = NULL;
    }
    else
    {
        const char *dir = g_application_command_line_get_cwd(cmd);
        lp->env = g_strdupv(
                (char **) g_application_command_line_get_environ(cmd));
        for (GList *wlink = lp->windows; wlink; wlink = g_list_next(wlink))
        {
            RoxtermWindowLaunchParams *wp = wlink->data;
            for (GList *tlink = wp->tabs; tlink; tlink = g_list_next(tlink))
            {
                RoxtermTabLaunchParams *tp = tlink->data;
                if (!tp->directory)
                    tp->directory = g_strdup(dir);
            }
        }
    }
    g_strfreev(argv);
    return lp;
}
