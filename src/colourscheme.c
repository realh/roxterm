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


#include "colourscheme.h"
#include "dlg.h"
#include "dynopts.h"

#define COLOURSCHEME_GROUP "roxterm colour scheme"

#if GTK_CHECK_VERSION(3,0,0)
#define HAVE_COLORMAP 0
#else
#define HAVE_COLORMAP 1
#endif

typedef struct {
    GdkColor *foreground, *background, *cursor;
    GdkColor *palette;
    int palette_size;
#if HAVE_COLORMAP
    GdkColormap *colourmap;
#endif
} ColourScheme;

static DynamicOptions *colour_scheme_dynopts = NULL;

static void delete_scheme(ColourScheme *scheme)
{
    g_free(scheme->foreground);
    g_free(scheme->background);
    g_free(scheme->cursor);
    g_free(scheme->palette);
    g_free(scheme);
}

void colour_scheme_reset_cached_data(Options *opts)
{
    ColourScheme *scheme = options_get_data(opts);

    if (scheme)
        delete_scheme(scheme);
    scheme = g_new0(ColourScheme, 1);
#if HAVE_COLORMAP
    scheme->colourmap = gdk_colormap_get_system();
#endif
    options_associate_data(opts, scheme);
}

Options *colour_scheme_lookup_and_ref(const char *scheme_name)
{
    Options *opts;

    if (!colour_scheme_dynopts)
        colour_scheme_dynopts = dynamic_options_get("Colours");
    opts = dynamic_options_lookup_and_ref(colour_scheme_dynopts,
            scheme_name, COLOURSCHEME_GROUP);
    colour_scheme_reset_cached_data(opts);
    return opts;
}

gboolean colour_scheme_unref(Options * opts)
{
    ColourScheme *scheme;

    g_return_val_if_fail(opts, FALSE);
    scheme = options_get_data(opts);
    g_return_val_if_fail(scheme, FALSE);

    if (dynamic_options_unref(colour_scheme_dynopts,
                options_get_leafname(opts)))
    {
        delete_scheme(scheme);
        return TRUE;
    }
    return FALSE;
}

static const char *colour_scheme_choose_default(int palette_entry)
{
    static const char *default_colours[24] = {
        "#2c2c2c", "#c00000", "#00c000", "#c0c000",
            "#5555ff", "#aa00aa", "#00aaaa", "#e8e8e8",
        "#000000", "#ff0000", "#00ff00", "#ffff00",
            "#2666ff", "#ff00ff", "#00ffff", "#ffffff",
        "#4c4c4c", "#a83030", "#208820", "#a88800",
            "#555598", "#883088", "#308888", "#d8d8d8"
    };

    return default_colours[palette_entry];
}

static gboolean
colour_scheme_parse(ColourScheme * scheme, GdkColor *colour,
        const char *colour_name)
{
    if (gdk_color_parse(colour_name, colour))
    {
#if HAVE_COLORMAP
        return gdk_colormap_alloc_color(scheme->colourmap, colour, TRUE, TRUE);
#else
        return TRUE;
#endif
    }
    return FALSE;
}

static gboolean colour_scheme_lookup_and_parse(Options * opts,
        ColourScheme * scheme, GdkColor * colour, const char *key,
        const char *default_colour, gboolean warn)
{
    char *name = options_lookup_string(opts, key);
    gboolean result = TRUE;

    if (!name || !colour_scheme_parse(scheme, colour, name))
    {
        if (warn && name)
        {
            dlg_warning(NULL,
                    _("Unable to parse colour '%s' for %s in scheme %s"),
                    name, key, options_get_leafname(opts));
        }
        if (default_colour)
            result = colour_scheme_parse(scheme, colour, default_colour);
        else
            result = FALSE;
    }
    if (name)
        g_free(name);
    return result;
}

static void
colour_scheme_parse_palette_range(Options * opts, ColourScheme * scheme,
    int start, int end)
{
    int n;

    for (n = start; n < 24; ++n)
    {
        char key[8];
        sprintf(key, "%d", n);

        colour_scheme_lookup_and_parse(opts, scheme, &scheme->palette[n], key,
                    colour_scheme_choose_default(n), n < end);
    }
}

static void colour_scheme_parse_palette(Options * opts, ColourScheme * scheme)
{
    if (!scheme->palette)
        scheme->palette = g_new0(GdkColor, 24);
    scheme->palette_size = options_lookup_int(opts, "palette_size");
    switch (scheme->palette_size)
    {
        case 8:
        case 16:
        case 24:
            /* No problem */
            break;
        case -1:
            /* Not given, probably fine */
            scheme->palette_size = 0;
            break;
        case 0:
            break;
        default:
            dlg_warning(NULL,
                _("Invalid palette size %d in colour scheme %s"),
                scheme->palette_size, options_get_leafname(opts));
            scheme->palette_size = 0;
            break;
    }
    colour_scheme_parse_palette_range(opts, scheme, 0, scheme->palette_size);
}

GdkColor *colour_scheme_get_palette(Options * opts)
{
    ColourScheme *scheme;

    g_return_val_if_fail(opts, NULL);
    scheme = options_get_data(opts);
    g_return_val_if_fail(scheme, NULL);

    colour_scheme_parse_palette(opts, scheme);
    return scheme->palette;
}

int colour_scheme_get_palette_size(Options * opts)
{
    ColourScheme *scheme;

    g_return_val_if_fail(opts, 0);
    scheme = options_get_data(opts);
    g_return_val_if_fail(scheme, 0);

    colour_scheme_parse_palette(opts, scheme);
    return scheme->palette_size;
}

GdkColor *colour_scheme_get_cursor_colour(Options *opts,
        gboolean allow_null)
{
    ColourScheme *scheme;

    g_return_val_if_fail(opts, NULL);
    scheme = options_get_data(opts);
    g_return_val_if_fail(scheme, NULL);

    if (!scheme->cursor)
    {
        scheme->cursor = g_new0(GdkColor, 1);
        if (!colour_scheme_lookup_and_parse(opts, scheme, scheme->cursor,
                    "cursor", allow_null ? NULL : "#c0c0c0", TRUE))
        {
            g_free(scheme->cursor);
            scheme->cursor = NULL;
        }
    }
    return scheme->cursor;
}

GdkColor *colour_scheme_get_foreground_colour(Options * opts,
            gboolean allow_null)
{
    ColourScheme *scheme;

    g_return_val_if_fail(opts, NULL);
    scheme = options_get_data(opts);
    g_return_val_if_fail(scheme, NULL);

    if (!scheme->foreground)
    {
        scheme->foreground = g_new0(GdkColor, 1);
        if (!colour_scheme_lookup_and_parse(opts, scheme, scheme->foreground,
                "foreground", allow_null ? NULL : "#c0c0c0", FALSE))
        {
            g_free(scheme->foreground);
            scheme->foreground = NULL;
        }
    }
    return scheme->foreground;
}

GdkColor *colour_scheme_get_background_colour(Options * opts,
        gboolean allow_null)
{
    ColourScheme *scheme;

    g_return_val_if_fail(opts, NULL);
    scheme = options_get_data(opts);
    g_return_val_if_fail(scheme, NULL);

    if (!scheme->background)
    {
        scheme->background = g_new0(GdkColor, 1);
        if (!colour_scheme_lookup_and_parse(opts, scheme, scheme->background,
                "background", allow_null ? NULL : "#000", FALSE))
        {
            g_free(scheme->background);
            scheme->background = NULL;
        }
    }
    return scheme->background;
}

void colour_scheme_set_palette_size(Options * opts, int size)
{
    ColourScheme *scheme;

    g_return_if_fail(opts);
    scheme = options_get_data(opts);
    g_return_if_fail(scheme);
    scheme->palette_size = size;
}

static void colour_scheme_set_colour(Options *opts, ColourScheme *scheme,
        GdkColor **colour, const char *key, const char *colour_name)
{
    if (!*colour)
        *colour = g_new(GdkColor, 1);
    if (!colour_name || colour_scheme_parse(scheme, *colour, colour_name))
        options_set_string(opts, key, colour_name);
    if (!colour_name)
    {
        g_free(*colour);
        *colour = NULL;
    }
}

void colour_scheme_set_palette_entry(Options * opts, int index,
        const char *colour_name)
{
    ColourScheme *scheme;
    char key[8];
    GdkColor *colour;

    g_return_if_fail(opts);
    g_return_if_fail(colour_name);
    g_return_if_fail(index >= 0 && index < 24);
    scheme = options_get_data(opts);
    g_return_if_fail(scheme);
    colour = &scheme->palette[index];
    sprintf(key, "%d", index);
    colour_scheme_set_colour(opts, scheme, &colour, key, colour_name);
}

void colour_scheme_set_cursor_colour(Options * opts, const char *colour_name)
{
    ColourScheme *scheme;

    g_return_if_fail(opts);
    scheme = options_get_data(opts);
    g_return_if_fail(scheme);
    colour_scheme_set_colour(opts, scheme, &scheme->cursor,
            "cursor", colour_name);
}

void colour_scheme_set_foreground_colour(Options * opts,
        const char *colour_name)
{
    ColourScheme *scheme;

    g_return_if_fail(opts);
    scheme = options_get_data(opts);
    g_return_if_fail(scheme);
    colour_scheme_set_colour(opts, scheme, &scheme->foreground,
            "foreground", colour_name);
}

void colour_scheme_set_background_colour(Options * opts,
        const char *colour_name)
{
    ColourScheme *scheme;

    g_return_if_fail(opts);
    scheme = options_get_data(opts);
    g_return_if_fail(scheme);
    colour_scheme_set_colour(opts, scheme, &scheme->background,
            "background", colour_name);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
