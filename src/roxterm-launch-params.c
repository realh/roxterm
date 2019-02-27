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
    g_free(wp->geometry);
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
    if (!value)
        return roxterm_launch_value_error(option, error);
    RoxtermTabLaunchParams *tp = roxterm_launch_params_current_tab(lp);
    if (!strcmp(option, "profile"))
        roxterm_launch_params_set_string(option, &tp->profile_name, value);
    else if (!strcmp(option, "tab-name"))
        roxterm_launch_params_set_string(option, &tp->tab_title, value);
    else if (!strcmp(option, "directory"))
        roxterm_launch_params_set_string(option, &tp->directory, value);
    return TRUE;
}

static gboolean
roxterm_launch_params_parse_window_str_option(const gchar *option,
        const gchar *value, gpointer data, GError **error)
{
    if (!value)
        return roxterm_launch_value_error(option, error);
    RoxtermLaunchParams *lp = data;
    RoxtermWindowLaunchParams *wp = roxterm_launch_params_current_window(lp);
    if (!strcmp(option, "window-title"))
        roxterm_launch_params_set_string(option, &wp->window_title, value);
    else if (!strcmp(option, "role"))
        roxterm_launch_params_set_string(option, &wp->role, value);
    else if (!strcmp(option, "geometry"))
        roxterm_launch_params_set_string(option, &wp->geometry, value);
    else if (!strcmp(option, "zoom"))
    {
        roxterm_launch_params_set_string(option, &wp->zoom_str, value);
        if (sscanf(wp->zoom_str, "%ld", &wp->zoom))
            return roxterm_launch_number_error(option, error);
    }
    return TRUE;
}

static gboolean
roxterm_launch_params_parse_window_bool_option(const gchar *option,
        const gchar *value, gpointer data, GError **error)
{
    if (value)
        return roxterm_launch_bool_error(option, error);
    RoxtermLaunchParams *lp = data;
    RoxtermWindowLaunchParams *wp = roxterm_launch_params_current_window(lp);
    if (!strcmp(option, "maximized"))
        roxterm_launch_params_set_boolean(option, &wp->maximized);
    else if (!strcmp(option, "fullscreen"))
        roxterm_launch_params_set_boolean(option, &wp->fullscreen);
    return TRUE;
}

RoxtermLaunchParams *
roxterm_launch_params_new_from_command_line(GApplicationCommandLine *cmd,
        GError **error)
{
    RoxtermLaunchParams *lp = roxterm_launch_params_new();
    // Separate args pre- and post- --execute 
    int argc;
    gchar **argv = g_application_command_line_get_arguments(cmd, &argc);
    for (int n = 0; n < argc; ++n)
    {
        if (!strcmp(argv[n], "--execute") || !strcmp(argv[n], "-e"))
        {
            if (n == argc - 1)
            {
                if (error)
                {
                    *error = g_error_new(G_OPTION_ERROR,
                            G_OPTION_ERROR_FAILED,
                            "No arguments following %s", argv[n]);
                    roxterm_launch_params_free(lp);
                    return NULL;
                }
            }
            argv[n] = NULL;
            lp->argv = g_new(gchar *, argc - n);
            int m = 0;
            ++n;
            for (; n < argc; ++n)
            {
                // Steal from original argv, lp->argv takes ownership
                lp->argv[m++] = argv[n];
                argv[n] = NULL;
            }
            lp->argv[m] = NULL;
            break;
        }
    }
    g_strfreev(argv);
}