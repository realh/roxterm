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

#include <locale.h>

#include "capplet.h"
#include "colourgui.h"
#include "configlet.h"
#include "dynopts.h"
#include "globalopts.h"
#include "logo.h"
#include "optsdbus.h"
#include "optsfile.h"
#include "profilegui.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
    capplet_open_windows = 0;

gboolean capplet_ignore_changes = FALSE;

GtkWidget *capplet_lookup_combo(GtkBuilder *builder, const char *name)
{
    char *box_name = g_strdup_printf("%s_hbox", name);
    GObject *box = gtk_builder_get_object(builder, box_name);
    GList *children;
    GList *link;
    GtkWidget *combo = NULL;
    
    g_free(box_name);
    g_return_val_if_fail(box != NULL, NULL);
    children = gtk_container_get_children(GTK_CONTAINER(box));
    for (link = children; link; link = g_list_next(link))
    {
        if (GTK_IS_COMBO_BOX(link->data) &&
                !strcmp(gtk_buildable_get_name(link->data), name))
        {
            combo = link->data;
            break;
        }
    }
    g_list_free(children);
    if (!combo)
        g_critical(_("Combo '%s' not found"), name);
    return combo;
}

void capplet_save_file(Options * options)
{
    options_file_save(options->kf, options->name);
    options->kf_dirty = FALSE;
}

void capplet_set_int(Options * options, const char *name, int value)
{
    options_set_int(options, name, value);
    capplet_save_file(options);
    optsdbus_send_int_opt_signal(options->name, name, value);
}

void capplet_set_string(Options * options, const char *name,
        const char *value)
{
    options_set_string(options, name, value);
    capplet_save_file(options);
    optsdbus_send_string_opt_signal(options->name, name, value);
}

void capplet_set_float(Options * options, const char *name, double value)
{
    options_set_double(options, name, value);
    capplet_save_file(options);
    optsdbus_send_float_opt_signal(options->name, name, value);
}

void capplet_set_boolean_toggle(CappletData *capp,
    const char *name, gboolean dflt)
{
    capplet_ignore_changes = TRUE;
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(gtk_builder_get_object(capp->builder, name)),
            options_lookup_int_with_default(capp->options, name, dflt));
    capplet_ignore_changes = FALSE;
}

void capplet_set_text_entry(CappletData *capp,
    const char *name, const char *dflt)
{
    char *value = options_lookup_string_with_default(capp->options, name,
            dflt ? dflt : "");

    capplet_ignore_changes = TRUE;
    gtk_entry_set_text(GTK_ENTRY(gtk_builder_get_object(capp->builder, name)),
            value);
    capplet_ignore_changes = FALSE;
    g_free(value);
}

void capplet_set_radio(CappletData *capp,
    const char *name, int dflt)
{
    capplet_set_radio_by_index(capp->builder, name, 
            options_lookup_int_with_default(capp->options, name, dflt));
}

static void capplet_set_spin_button_value(CappletData *capp,
        const char *name, gdouble value)
{
    GtkSpinButton *spin_button = 
        GTK_SPIN_BUTTON(gtk_builder_get_object(capp->builder, name));
    
    capplet_ignore_changes = TRUE;
    gtk_spin_button_set_value(spin_button, value);
    gtk_adjustment_set_value(gtk_spin_button_get_adjustment(spin_button),
            value);
    capplet_ignore_changes = FALSE;
}

void capplet_set_spin_button(CappletData *capp,
    const char *name, int dflt)
{
    capplet_set_spin_button_value(capp, name,
        (gdouble) options_lookup_int_with_default(capp->options, name, dflt));
        
}

void capplet_set_spin_button_float(CappletData *capp,
    const char *name)
{
    capplet_set_spin_button_value(capp, name,
            options_lookup_double(capp->options, name));
}

void capplet_set_float_range(CappletData *capp,
    const char *name, double dflt)
{
    double value = options_lookup_double(capp->options, name);

    if (value == -1)
        value = dflt;
    capplet_ignore_changes = TRUE;
    gtk_range_set_value(GTK_RANGE(gtk_builder_get_object(capp->builder, name)),
            value);
    capplet_ignore_changes = FALSE;
}

void capplet_set_combo(CappletData *capp,
    const char *name, int dflt)
{
    capplet_ignore_changes = TRUE;
    gtk_combo_box_set_active(
            GTK_COMBO_BOX(capplet_lookup_combo(capp->builder, name)),
            options_lookup_int_with_default(capp->options, name, dflt));
    capplet_ignore_changes = FALSE;
}

/**********************************************/

/* Helpers for radio handler */

int capplet_which_radio_is_selected(GtkWidget *widget)
{
    GSList *list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(widget));
    int n = -1;

    g_return_val_if_fail(list, -1);
    for (;list; list = g_slist_next(list))
    {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(list->data)))
        {
            const char *name =
                    gtk_buildable_get_name(GTK_BUILDABLE(list->data));

            /* Only works for single digits */
            n = atoi(name + strlen(name) - 1);
            break;
        }
    }
    if (!list)
        g_critical(_("No radio button selected in queried group"));
    return n;
}

void capplet_set_radio_by_index(GtkBuilder *builder,
        const char *basename, int index)
{
    char *widget_name = g_strdup_printf("%s%d", basename, index);

    capplet_ignore_changes = TRUE;
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, widget_name)),
            TRUE);
    capplet_ignore_changes = FALSE;
}

/* Generic handlers */

void on_radio_toggled(GtkToggleButton * button, CappletData *capp)
{
    const char *name;

    /* We'll get two events: one for the button going off and one for the
     * button going on; we can ignore the former */
    if (capplet_ignore_changes || !gtk_toggle_button_get_active(button))
    {
        return;
    }
    name = gtk_buildable_get_name(GTK_BUILDABLE(button));
    if (name)
    {
        char *opt_name = g_strdup(name);
        int l = strlen(name);
        int n = atoi(name + l - 1);

        g_return_if_fail(isdigit(name[l - 1]));
        opt_name[l - 1] = 0;
        capplet_set_int(capp->options, opt_name, n);
        g_free(opt_name);
    }
}

void on_boolean_toggled(GtkToggleButton * button, CappletData *capp)
{
    const char *name;

    if (capplet_ignore_changes)
        return;
    name = gtk_buildable_get_name(GTK_BUILDABLE(button));
    if (name)
    {
        gboolean state = gtk_toggle_button_get_active(button);

        if (!strcmp(name, "show_menubar"))
        {
            name = "hide_menubar";
            state = !state;
        }
        capplet_set_int(capp->options, name, state);
    }
}

void on_float_spin_button_changed(GtkSpinButton *button, CappletData *capp)
{
    const char *name;

    if (capplet_ignore_changes)
        return;
    name = gtk_buildable_get_name(GTK_BUILDABLE(button));
    if (name)
    {
        capplet_set_float(capp->options, name,
                gtk_spin_button_get_value(button));
    }
}

void on_spin_button_changed(GtkSpinButton * button, CappletData *capp)
{
    const char *name;

    if (capplet_ignore_changes)
        return;
    name = gtk_buildable_get_name(GTK_BUILDABLE(button));
    if (name)
    {
        capplet_set_int(capp->options, name,
                gtk_spin_button_get_value_as_int(button));
    }
}

void on_float_range_changed(GtkRange * range, CappletData *capp)
{
    const char *name;

    if (capplet_ignore_changes)
        return;
    name = gtk_buildable_get_name(GTK_BUILDABLE(range));
    if (name)
    {
        capplet_set_float(capp->options, name, gtk_range_get_value(range));
    }
}

void on_combo_changed(GtkComboBox * combo, CappletData *capp)
{
    const char *name;

    if (capplet_ignore_changes)
        return;
    name = gtk_buildable_get_name(GTK_BUILDABLE(combo));
    if (name)
    {
        capplet_set_int(capp->options, name,
                gtk_combo_box_get_active(combo));
    }
}

/**********************************************/


int main(int argc, char **argv)
{
    gboolean listen = FALSE;
    int n;
    const char *profile = NULL;
    const char *colour_scheme = NULL;
    char *logo_filename;
    gboolean persist = FALSE;
    gboolean open_configlet = FALSE;
    gboolean dbus_ok;
    GdkScreen *scrn;
    char *disp_name;

    g_set_application_name(_("roxterm-config"));

    global_options_init_appdir(argc, argv);

#if ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, global_options_appdir ?
            g_strdup_printf("%s/locale", global_options_appdir) : LOCALEDIR);
    textdomain(PACKAGE);
#endif    

    gtk_init(&argc, &argv);
    scrn = gdk_screen_get_default();
    disp_name = gdk_screen_make_display_name(scrn);

    logo_filename = logo_find();
    gtk_window_set_default_icon_from_file(logo_filename, NULL);
    g_free(logo_filename);
    
    for (n = 1; n < argc; ++n)
    {
        if (!strcmp(argv[n], "--listen"))
            listen = TRUE;
        else if (!strncmp(argv[n], "--EditProfile=", 14))
            profile = argv[n] + 14;
        else if (!strncmp(argv[n], "--EditColourScheme=", 19))
            colour_scheme = argv[n] + 19;
        else if (!strcmp(argv[n], "--Configlet"))
            open_configlet = TRUE;
    }
    
    if (!open_configlet)
        open_configlet = !profile && !colour_scheme && !listen;

    dbus_ok = rtdbus_init();

    if (dbus_ok && optsdbus_init())
    {
        /* Defer to another instance */
        if (profile)
        {
            if (optsdbus_send_edit_profile_message(profile, disp_name))
                profile = NULL;
        }
        if (colour_scheme)
        {
            if (optsdbus_send_edit_colour_scheme_message(colour_scheme,
                    disp_name))
            {
                colour_scheme = NULL;
            }
        }
        if (open_configlet)
        {
            if (optsdbus_send_edit_opts_message("Configlet", "", disp_name))
            {
                open_configlet = FALSE;
            }
        }
    }

    if (profile)
    {
        profilegui_open(profile, scrn);
        persist = TRUE;
    }
    if (colour_scheme)
    {
        colourgui_open(colour_scheme, scrn);
        persist = TRUE;
    }
    if (open_configlet)
    {
        if (configlet_open(scrn))
            persist = TRUE;
    }

    if (dbus_ok && listen)
        persist = TRUE;

    g_free(disp_name);
    
    if (persist)
        gtk_main();

    return 0;
}

const char *capplet_get_ui_filename(void)
{
    static char *filename = NULL;

    if (!filename)
    {
        if (global_options_appdir)
        {
            filename = g_build_filename(global_options_appdir, "src",
                "roxterm-config.ui", NULL);
        }
        else
        {
            filename = g_build_filename(PKG_DATA_DIR,
                "roxterm-config.ui", NULL);
        }
    }
    return filename;
}

void capplet_inc_windows(void)
{
    ++capplet_open_windows;
}

void capplet_dec_windows(void)
{
    if (!--capplet_open_windows)
        gtk_main_quit();
}

/* vi:set sw=4 ts=4 noet cindent cino= */
