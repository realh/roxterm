/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2015 Tony Houghton <h@realh.co.uk>

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
    options_reload_keyfile(global_options);
    correct_schemes();
}

static gboolean global_options_set_string(const gchar *option_name,
        const gchar *value, gpointer data, GError **error)
{
    (void) error;
    (void) data;
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

static gboolean global_options_set_bool(const gchar *option_name,
        const gchar *value, gpointer data, GError **error)
{
    (void) error;
    (void) data;
    (void) value;
    gboolean val = TRUE;

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
    (void) error;
    (void) data;
    options_set_double(global_options, option_name, atof(value));
    return TRUE;
}

void global_options_init()
{
    static gboolean already = FALSE;

    if (already)
    {
        global_options_reset();
    }
    else
    {
        global_options = options_open("Global", NULL, "roxterm options");
        correct_schemes();
    }
    already = TRUE;
}

void global_options_apply_dark_theme(void)
{
    GtkSettings *gtk_settings;

    gtk_settings = gtk_settings_get_default();
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(gtk_settings),
            "gtk-application-prefer-dark-theme"))
    {
        g_object_set(gtk_settings, "gtk-application-prefer-dark-theme",
                global_options_lookup_int_with_default("prefer_dark_theme",
                        FALSE),
                NULL);
    }
}

/* vi:set sw=4 ts=4 noet cindent cino= */
