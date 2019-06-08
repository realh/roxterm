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

#include <string.h>

#include "capplet.h"
#include "colourgui.h"
#include "colourscheme.h"
#include "configlet.h"
#include "dlg.h"
#include "dynopts.h"
#include "resources.h"

#define DEFAULT_PALETTE_SIZE 16

struct _ColourGUI {
    CappletData capp;
    GtkWidget *widget;
    char *scheme_name;
    Options *orig_scheme;
    gboolean ignore_destroy;
};

static GHashTable *colourgui_being_edited = NULL;

static void colourgui_set_colour_widget(GtkBuilder *builder,
        const char *widget_name, GdkRGBA *colour, gboolean ignore)
{
    GtkWidget *widget;

    if (!colour)
        return;
    widget = GTK_WIDGET(gtk_builder_get_object(builder, widget_name));
    /* Setting a colour doesn't seem to cause a change signal so
     * explicitly raise one if necessary */
    capplet_ignore_changes = TRUE;
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(widget), colour);
    capplet_ignore_changes = FALSE;
    if (!ignore)
        g_signal_emit_by_name(widget, "color-set");
}

static void colourgui_set_fgbg_colour_widgets(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore)
{
    colourgui_set_colour_widget(builder, "foreground_colour",
            colour_scheme_get_foreground_colour(colour_scheme, FALSE), ignore);
    colourgui_set_colour_widget(builder, "background_colour",
            colour_scheme_get_background_colour(colour_scheme, FALSE), ignore);
}

static void colourgui_set_cursor_colour_widget(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore)
{
    colourgui_set_colour_widget(builder, "cursor_colour",
            colour_scheme_get_cursor_colour(colour_scheme, FALSE), ignore);
}

static void colourgui_set_cursorfg_colour_widget(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore)
{
    colourgui_set_colour_widget(builder, "cursorfg_colour",
            colour_scheme_get_cursorfg_colour(colour_scheme, FALSE), ignore);
}

static void colourgui_set_bold_colour_widget(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore)
{
    colourgui_set_colour_widget(builder, "bold_colour",
            colour_scheme_get_bold_colour(colour_scheme, FALSE), ignore);
}

static void colourgui_set_all_palette_widgets(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore)
{
    int n;
    GdkRGBA *palette = colour_scheme_get_palette(colour_scheme);

    g_return_if_fail(palette);
    for (n = 0; n < 16; ++n)
    {
        char widget_name[20];

        sprintf(widget_name, "palette%d_%d", n / 8, n % 8);
        colourgui_set_colour_widget(builder, widget_name, &palette[n], ignore);
    }
}

static void colourgui_shade_row(GtkBuilder *builder,
        int row, gboolean sensitive)
{
    char widget_name[20];
    int column;

    for (column = 0; column < 8; ++column)
    {
        sprintf(widget_name, "palette%d_%d", row, column);
        gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder,
                widget_name)), sensitive);
    }
}

static void colourgui_set_palette_shading(GtkBuilder *builder)
{
    int size_radio = -1;
    gboolean use_custom = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder,
                        "use_custom_colours")));
    int row;
    char widget_name[20];

    if (use_custom)
    {
        size_radio = capplet_which_radio_is_selected(
                GTK_WIDGET(gtk_builder_get_object(builder, "palette_size0")));
    }
    colourgui_shade_row(builder, 1, size_radio >= 1);
    colourgui_shade_row(builder, 0, size_radio >= 0);
    for (row = 0; row < 2; ++row)
    {
        sprintf(widget_name, "palette_size%d", row);
        gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder,
                widget_name)), use_custom);
    }
}

static void colourgui_set_fgbg_shading(GtkBuilder *builder)
{
    gboolean sensitive = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "set_fgbg")))
        && !gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder,
                    "fgbg_track_palette")));

    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder,
                "foreground_colour")), sensitive);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder,
                "text_colour_label")), sensitive);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder,
                "background_colour")), sensitive);
    gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(builder,
                "background_colour_label")), sensitive);
}

static void colourgui_set_fgbg_track_shading(GtkBuilder *builder)
{
    gtk_widget_set_sensitive(
            GTK_WIDGET(gtk_builder_get_object(builder, "fgbg_track_palette")),
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
                    gtk_builder_get_object(builder, "set_fgbg")))
            && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
                    gtk_builder_get_object(builder, "use_custom_colours"))));
}

static void colourgui_set_cursor_shading(GtkBuilder *builder)
{
    gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
                    gtk_builder_get_object(builder, "set_cursor_colour")));
    gtk_widget_set_sensitive(
            GTK_WIDGET(gtk_builder_get_object(builder, "cursor_colour")),
            state);
    gtk_widget_set_sensitive(
            GTK_WIDGET(gtk_builder_get_object(builder, "cursorfg_colour")),
            state);
}

static void colourgui_set_bold_shading(GtkBuilder *builder)
{
    gtk_widget_set_sensitive(
            GTK_WIDGET(gtk_builder_get_object(builder, "bold_colour")),
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
                    gtk_builder_get_object(builder, "set_bold_colour"))));
}

static void colourgui_set_fgbg_toggle(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore)
{
    char *fg = options_lookup_string(colour_scheme, "foreground");
    char *bg = options_lookup_string(colour_scheme, "background");

    capplet_ignore_changes = ignore;
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "set_fgbg")),
            fg || bg);
    capplet_ignore_changes = FALSE;
    if (bg)
        g_free(bg);
    if (fg)
        g_free(fg);
}

static void colourgui_set_fgbg_track_toggle(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore)
{
    gboolean track = FALSE;

    if (gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder,
                    "set_fgbg")))
            && colour_scheme_get_palette_size(colour_scheme))
    {
        GdkRGBA *palette = colour_scheme_get_palette(colour_scheme);
        GdkRGBA *fg = colour_scheme_get_foreground_colour(colour_scheme, TRUE);
        GdkRGBA *bg = colour_scheme_get_background_colour(colour_scheme, TRUE);

        if (fg && bg
                && gdk_rgba_equal(&palette[0], fg)
                && gdk_rgba_equal(&palette[7], bg))
        {
            track = TRUE;
        }
    }
    capplet_ignore_changes = ignore;
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder,
                "fgbg_track_palette")),
        track);
    capplet_ignore_changes = FALSE;
}

static void colourgui_set_palette_toggles(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore)
{
    int ps = colour_scheme_get_palette_size(colour_scheme);
    GtkToggleButton *ucp =
        GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder,
                "use_custom_colours"));

    capplet_ignore_changes = ignore;
    switch (ps)
    {
        case 0:
            gtk_toggle_button_set_active(ucp, FALSE);
            /* Leave radios in whatever state they were in before */
            break;
        case 8:
        case 16:
            gtk_toggle_button_set_active(ucp, TRUE);
            capplet_set_radio_by_index(builder, "palette_size", ps / 8 - 1);
            /* This will clear ignore_changes, but doesn't matter */
            break;
        default:
            g_critical(_("Illegal palette size %d"),
                        colour_scheme_get_palette_size(colour_scheme));
            gtk_toggle_button_set_active(ucp, FALSE);
            break;
    }
    capplet_ignore_changes = FALSE;
}

static void colourgui_set_named_toggle(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore,
        const char *opt_name, const char *widget_name)
{
    char *ov = options_lookup_string(colour_scheme, opt_name);

    capplet_ignore_changes = ignore;
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder,
                    widget_name)),
            ov != NULL);
    capplet_ignore_changes = FALSE;
    g_free(ov);
}

inline static void colourgui_set_cursor_toggle(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore)
{
    colourgui_set_named_toggle(builder, colour_scheme, ignore,
            "cursor", "set_cursor_colour");
}

inline static void colourgui_set_bold_toggle(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore)
{
    colourgui_set_named_toggle(builder, colour_scheme, ignore,
            "bold", "set_bold_colour");
}

static void colourgui_set_all_toggles(GtkBuilder *builder,
        Options *colour_scheme, gboolean ignore)
{
    colourgui_set_fgbg_toggle(builder, colour_scheme, ignore);
    colourgui_set_fgbg_track_toggle(builder, colour_scheme, ignore);
    colourgui_set_palette_toggles(builder, colour_scheme, ignore);
    colourgui_set_cursor_toggle(builder, colour_scheme, ignore);
    colourgui_set_bold_toggle(builder, colour_scheme, ignore);
}

static void colourgui_set_all_shading(GtkBuilder *builder)
{
    colourgui_set_palette_shading(builder);
    colourgui_set_fgbg_shading(builder);
    colourgui_set_fgbg_track_shading(builder);
    colourgui_set_cursor_shading(builder);
    colourgui_set_bold_shading(builder);
}

static void colourgui_fill_in_dialog(ColourGUI * cg, gboolean ignore)
{
    GtkBuilder *builder = cg->capp.builder;
    Options *colour_scheme = cg->capp.options;

    colourgui_set_all_toggles(builder, colour_scheme, ignore);
    colourgui_set_all_shading(builder);
    colourgui_set_all_palette_widgets(builder, colour_scheme, ignore);
    colourgui_set_fgbg_colour_widgets(builder, colour_scheme, ignore);
    colourgui_set_cursor_colour_widget(builder, colour_scheme, ignore);
    colourgui_set_cursorfg_colour_widget(builder, colour_scheme, ignore);
    colourgui_set_bold_colour_widget(builder, colour_scheme, ignore);
}

static void colourgui_revert(ColourGUI *cg)
{
    options_copy_keyfile(cg->capp.options, cg->orig_scheme);
    colour_scheme_reset_cached_data(cg->capp.options);
    colourgui_fill_in_dialog(cg, FALSE);
}

/***********************************************************/
/* Handlers */

void on_Colour_Editor_destroy(GtkWidget *widget, ColourGUI * cg)
{
    (void) widget;
    if (cg->ignore_destroy)
    {
        return;
    }
    else
    {
        cg->ignore_destroy = TRUE;
        colourgui_delete(cg);
    }
}

void on_Colour_Editor_response(GtkWidget *widget, gint response, ColourGUI * cg)
{
    (void) widget;
    if (response == 1)
        colourgui_revert(cg);
    else
        colourgui_delete(cg);
}

void on_Colour_Editor_close(GtkWidget *widget, ColourGUI * cg)
{
    (void) widget;
    colourgui_delete(cg);
}

static const char *colourgui_name_colour(const GdkRGBA *colour)
{
    static char colour_name[16];

    sprintf(colour_name, "#%04hx%04hx%04hx", \
        (guint16) (colour->red * 65535), \
        (guint16) (colour->green * 65535), \
        (guint16) (colour->blue * 65535));
    return colour_name;
}

static const char *colourgui_read_colour_widget(ColourGUI *cg,
        const char *widget_name)
{
    GtkColorButton *button =
            GTK_COLOR_BUTTON(gtk_builder_get_object(cg->capp.builder,
                    widget_name));
    GdkRGBA colour;

    g_return_val_if_fail(button, NULL);
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &colour);
    return colourgui_name_colour(&colour);
}

static void colourgui_set_colour_option(Options *colour_scheme,
        const char *opt_name, const char *colour_name,
        void (*set_fn)(Options *, const char *))
{
    set_fn(colour_scheme, colour_name);
    capplet_set_string(colour_scheme, opt_name, colour_name);
}

#define COLOURGUI_SET_COLOUR_OPTION(cs, opt_name, colour_name) \
        colourgui_set_colour_option(cs, \
        #opt_name, colour_name, colour_scheme_set_ ## opt_name ## _colour)

static void colourgui_set_colour_option_from_widget(ColourGUI *cg,
        const char *widget_name, const char *opt_name,
        void (*set_fn)(Options *, const char *))
{
    colourgui_set_colour_option(cg->capp.options, opt_name,
            colourgui_read_colour_widget(cg, widget_name), set_fn);
}

#define COLOURGUI_SET_COLOUR_OPTION_FROM_WIDGET(cg, name) \
        colourgui_set_colour_option_from_widget(cg, #name "_colour", \
        #name, colour_scheme_set_ ## name ## _colour)

void on_color_set(GtkColorButton *button, ColourGUI * cg)
{
    const char *widget_name;
    GdkRGBA colour;
    const char *colour_name;
    const char *option_name = NULL;
    char dyn_opt_nm[8];
    Options *opts = cg->capp.options;

    if (capplet_ignore_changes)
    {
        return;
    }
    widget_name = gtk_buildable_get_name(GTK_BUILDABLE(button));
    g_return_if_fail(widget_name);
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &colour);
    colour_name = colourgui_name_colour(&colour);
    if (!strcmp(widget_name, "foreground_colour"))
    {
        option_name = "foreground";
        colour_scheme_set_foreground_colour(opts, colour_name);
    }
    else if (!strcmp(widget_name, "background_colour"))
    {
        option_name = "background";
        colour_scheme_set_background_colour(opts, colour_name);
    }
    else if (!strcmp(widget_name, "cursor_colour"))
    {
        option_name = "cursor";
        colour_scheme_set_cursor_colour(opts, colour_name);
    }
    else if (!strcmp(widget_name, "cursorfg_colour"))
    {
        option_name = "cursorfg";
        colour_scheme_set_cursorfg_colour(opts, colour_name);
    }
    else if (!strcmp(widget_name, "bold_colour"))
    {
        option_name = "bold";
        colour_scheme_set_bold_colour(opts, colour_name);
    }
    else if (!strncmp(widget_name, "palette", 7) && strlen(widget_name) == 10)
    {
        int row = widget_name[7] - '0';
        int column = widget_name[9] - '0';

        colour_scheme_set_palette_entry(opts, row * 8 + column, colour_name);
        sprintf(dyn_opt_nm, "%d", row * 8 + column);
        option_name = dyn_opt_nm;
        if (row == 0 && gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(gtk_builder_get_object(cg->capp.builder,
                        "fgbg_track_palette"))))
        {
            if (column == 7)
            {
                colourgui_set_colour_widget(cg->capp.builder,
                        "background_colour", &colour, TRUE);
                capplet_set_string(cg->capp.options,
                        "background", colour_name);
            }
            if (column == 0)
            {
                colourgui_set_colour_widget(cg->capp.builder,
                        "foreground_colour", &colour, TRUE);
                capplet_set_string(cg->capp.options,
                        "foreground", colour_name);
            }
        }
    }
    if (option_name)
    {
        capplet_set_string(cg->capp.options, option_name, colour_name);
    }
    else
    {
        g_critical(_("Unable to get colour option name from widget name '%s'"),
                widget_name);
    }
}

void on_set_fgbg_toggled(GtkToggleButton *button, ColourGUI * cg)
{
    gboolean state;

    if (capplet_ignore_changes)
        return;

    colourgui_set_fgbg_shading(cg->capp.builder);
    colourgui_set_fgbg_track_shading(cg->capp.builder);
    colourgui_set_fgbg_track_toggle(cg->capp.builder, cg->capp.options, FALSE);
    state = gtk_toggle_button_get_active(button);
    if (state)
    {
        COLOURGUI_SET_COLOUR_OPTION_FROM_WIDGET(cg, foreground);
        COLOURGUI_SET_COLOUR_OPTION_FROM_WIDGET(cg, background);
    }
    else
    {
        COLOURGUI_SET_COLOUR_OPTION(cg->capp.options, foreground, NULL);
        COLOURGUI_SET_COLOUR_OPTION(cg->capp.options, background, NULL);
    }
}

static void mirror_colour(ColourGUI *cg, const char *source_widget_name,
        const char *target_widget, const char *target_option)
{
    GdkRGBA colour;
    const char *colour_name;

    gtk_color_chooser_get_rgba(
            GTK_COLOR_CHOOSER(gtk_builder_get_object(cg->capp.builder,
                source_widget_name)),
        &colour);
    colour_name = colourgui_name_colour(&colour);
    colourgui_set_colour_widget(cg->capp.builder, target_widget, &colour, TRUE);
    capplet_set_string(cg->capp.options, target_option, colour_name);
}

void on_fgbg_track_palette_toggled(GtkToggleButton *button, ColourGUI * cg)
{
    gboolean state;

    if (capplet_ignore_changes)
        return;

    colourgui_set_fgbg_shading(cg->capp.builder);
    state = gtk_toggle_button_get_active(button);
    if (state)
    {
        mirror_colour(cg, "palette0_7", "background_colour", "background");
        mirror_colour(cg, "palette0_0", "foreground_colour", "foreground");
    }
}

void on_use_custom_colours_toggled(GtkToggleButton *button, ColourGUI *cg)
{
    (void) button;
    gboolean state;
    int old_size;
    int palette_size;

    if (capplet_ignore_changes)
    {
        return;
    }

    state = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(gtk_builder_get_object(cg->capp.builder,
                    "use_custom_colours")));
    old_size = colour_scheme_get_palette_size(cg->capp.options);
    if (state)
    {
        palette_size = capplet_which_radio_is_selected(
                GTK_WIDGET(gtk_builder_get_object(cg->capp.builder,
                        "palette_size0"))) * 8 + 8;
    }
    else
    {
        palette_size = 0;
    }
    if (palette_size != old_size)
    {
        int n;

        colour_scheme_set_palette_size(cg->capp.options, palette_size);
        if (!palette_size)
        {
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(gtk_builder_get_object(cg->capp.builder,
                            "fgbg_track_palette")), FALSE);
        }
        colourgui_set_fgbg_track_shading(cg->capp.builder);
        colourgui_set_palette_shading(cg->capp.builder);
        for (n = old_size; n < palette_size; ++n)
        {
            char key[8];
            char *val;

            sprintf(key, "%d", n);
            val = options_lookup_string(cg->capp.options, key);
            if (!val)
            {
                char widget_name[24];
                const char *colour_name;

                sprintf(widget_name, "palette%d_%d", n / 8, n % 8);
                colour_name = colourgui_read_colour_widget(cg, widget_name);
                colour_scheme_set_palette_entry(cg->capp.options, n,
                        colour_name);
                capplet_set_string(cg->capp.options, key, colour_name);
            }
            else
            {
                g_free(val);
            }
        }
        capplet_set_int(cg->capp.options, "palette_size", palette_size);
    }
}

void on_set_cursor_colour_toggled(GtkToggleButton *button, ColourGUI *cg)
{
    int state;

    if (capplet_ignore_changes)
        return;

    colourgui_set_cursor_shading(cg->capp.builder);
    state = gtk_toggle_button_get_active(button);
    if (state)
    {
        COLOURGUI_SET_COLOUR_OPTION_FROM_WIDGET(cg, cursor);
        COLOURGUI_SET_COLOUR_OPTION_FROM_WIDGET(cg, cursorfg);
    }
    else
    {
        COLOURGUI_SET_COLOUR_OPTION(cg->capp.options, cursor, NULL);
        COLOURGUI_SET_COLOUR_OPTION(cg->capp.options, cursorfg, NULL);
    }
}

void on_set_bold_colour_toggled(GtkToggleButton *button, ColourGUI *cg)
{
    int state;

    if (capplet_ignore_changes)
        return;

    colourgui_set_bold_shading(cg->capp.builder);
    state = gtk_toggle_button_get_active(button);
    if (state)
        COLOURGUI_SET_COLOUR_OPTION_FROM_WIDGET(cg, bold);
    else
        COLOURGUI_SET_COLOUR_OPTION(cg->capp.options, bold, NULL);
}

void on_palette_size_toggled(GtkToggleButton *button, ColourGUI *cg)
{
    /* We can ignore a radio button being switched off because we'll get
     * another signal for the one being switched on */
    if (gtk_toggle_button_get_active(button))
        on_use_custom_colours_toggled(button, cg);
}


/***********************************************************/


ColourGUI *colourgui_open(const char *scheme_name)
{
    ColourGUI *cg;
    char *title;
    static char const *build_objs[] = { "Colour_Scheme_Editor", NULL };
    GError *error = NULL;

    if (!colourgui_being_edited)
    {
        colourgui_being_edited = g_hash_table_new_full(g_str_hash, g_str_equal,
                g_free, NULL);
    }

    cg = g_hash_table_lookup(colourgui_being_edited, scheme_name);
    if (cg)
    {
        gtk_window_present(GTK_WINDOW(cg->widget));
        return cg;
    }

    cg = g_new0(ColourGUI, 1);
    cg->scheme_name = g_strdup(scheme_name);
    cg->capp.options = colour_scheme_lookup_and_ref(scheme_name);
    cg->orig_scheme = options_copy(cg->capp.options);

    cg->capp.builder = gtk_builder_new();
    if (!gtk_builder_add_objects_from_resource(cg->capp.builder,
            ROXTERM_RESOURCE_UI, (char **) build_objs, &error))
    {
        g_error(_("Unable to load 'Colour_Scheme_Editor' from UI defs: %s"),
                error->message);
    }
    cg->widget = GTK_WIDGET(gtk_builder_get_object(cg->capp.builder,
            "Colour_Scheme_Editor"));
    title = g_strdup_printf(_("ROXTerm Colour Scheme \"%s\""), scheme_name);
    gtk_window_set_title(GTK_WINDOW(cg->widget), title);
    g_free(title);

    if (!cg->orig_scheme)
    {
        dlg_warning(GTK_WINDOW(cg->widget),
                _("Unable to back up colour scheme to revert"));
        gtk_widget_set_sensitive(
                GTK_WIDGET(gtk_builder_get_object(cg->capp.builder, "revert")),
                FALSE);
    }

    g_hash_table_insert(colourgui_being_edited, g_strdup(scheme_name), cg);

    colourgui_fill_in_dialog(cg, TRUE);
    gtk_builder_connect_signals(cg->capp.builder, cg);

    gtk_widget_show(cg->widget);

    capplet_inc_windows();

    configlet_lock_colour_schemes();

    return cg;
}

void colourgui_delete(ColourGUI * cg)
{
    if (!g_hash_table_remove(colourgui_being_edited, cg->scheme_name))
    {
        g_critical(_("Deleting edited colour scheme that wasn't in list of "
                    "schemes being edited"));
    }
    if (!cg->ignore_destroy)
    {
        cg->ignore_destroy = TRUE;
        gtk_widget_destroy(cg->widget);
    }
    UNREF_LOG(g_object_unref(cg->capp.builder));
    g_free(cg->scheme_name);
    if (cg->orig_scheme)
        options_delete(cg->orig_scheme);
    UNREF_LOG(colour_scheme_unref(cg->capp.options));
    g_free(cg);
    capplet_dec_windows();
    configlet_unlock_colour_schemes();
}

/* vi:set sw=4 ts=4 noet cindent cino= */
