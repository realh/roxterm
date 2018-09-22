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


#include "colourscheme.h"
#include "dlg.h"
#include "dynopts.h"

#define COLOURSCHEME_GROUP "roxterm colour scheme"

typedef struct {
    GdkRGBA *foreground, *background, *cursor, *cursorfg, *bold, *dim;
    GdkRGBA *palette;
    int palette_size;
} ColourScheme;

static DynamicOptions *colour_scheme_dynopts = NULL;

static void delete_scheme(ColourScheme *scheme)
{
    g_free(scheme->foreground);
    g_free(scheme->background);
    g_free(scheme->cursor);
    g_free(scheme->bold);
    g_free(scheme->dim);
    g_free(scheme->palette);
    g_free(scheme);
}

void colour_scheme_reset_cached_data(Options *opts)
{
    ColourScheme *scheme = options_get_data(opts);

    if (scheme)
        delete_scheme(scheme);
    scheme = g_new0(ColourScheme, 1);
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
    static const char *default_colours[16] = {
        "#2c2c2c", "#c00000", "#00c000", "#c0c000",
            "#5555ff", "#aa00aa", "#00aaaa", "#e8e8e8",
        "#000000", "#ff0000", "#00ff00", "#ffff00",
            "#2666ff", "#ff00ff", "#00ffff", "#ffffff"
    };

    return default_colours[palette_entry];
}

static gboolean
colour_scheme_parse(ColourScheme * scheme, GdkRGBA *colour,
        const char *colour_name)
{
    (void) scheme;
    if (gdk_rgba_parse(colour, colour_name))
    {
        return TRUE;
    }
    return FALSE;
}

static gboolean colour_scheme_lookup_and_parse(Options * opts,
        ColourScheme * scheme, GdkRGBA * colour, const char *key,
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

    for (n = start; n < 16; ++n)
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
        scheme->palette = g_new0(GdkRGBA, 16);
    scheme->palette_size = options_lookup_int(opts, "palette_size");
    switch (scheme->palette_size)
    {
        case 8:
        case 16:
            /* No problem */
            break;
        case 24:
            /* No longer supported, use 16 */
            scheme->palette_size = 16;
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

GdkRGBA *colour_scheme_get_palette(Options * opts)
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

static GdkRGBA *colour_scheme_get_named_colour(Options *opts,
        const char *name, const char *dflt,
        size_t member_offset, gboolean allow_null)
{
    ColourScheme *scheme;
    GdkRGBA **member;

    g_return_val_if_fail(opts, NULL);
    scheme = options_get_data(opts);
    g_return_val_if_fail(scheme, NULL);

    member = (GdkRGBA **) (((char *) scheme) + member_offset);
    if (!*member)
    {
        *member = g_new0(GdkRGBA, 1);
        if (!colour_scheme_lookup_and_parse(opts, scheme, *member,
                    name, allow_null ? NULL : dflt, TRUE))
        {
            g_free(*member);
            *member = NULL;
        }
    }
    return *member;
}

GdkRGBA *colour_scheme_get_cursor_colour(Options *opts,
        gboolean allow_null)
{
    return colour_scheme_get_named_colour(opts, "cursor", "#ccc",
            offsetof(ColourScheme, cursor), allow_null);
}

GdkRGBA *colour_scheme_get_cursorfg_colour(Options * opts,
        gboolean allow_null)
{
    return colour_scheme_get_named_colour(opts, "cursorfg", "#000",
            offsetof(ColourScheme, cursorfg), allow_null);
}

GdkRGBA *colour_scheme_get_foreground_colour(Options * opts,
            gboolean allow_null)
{
    return colour_scheme_get_named_colour(opts, "foreground", "#ccc",
            offsetof(ColourScheme, foreground), allow_null);
}

GdkRGBA *colour_scheme_get_background_colour(Options * opts,
        gboolean allow_null)
{
    return colour_scheme_get_named_colour(opts, "background", "#000",
            offsetof(ColourScheme, background), allow_null);
}

GdkRGBA *colour_scheme_get_bold_colour(Options * opts,
        gboolean allow_null)
{
    return colour_scheme_get_named_colour(opts, "bold", "#fff",
            offsetof(ColourScheme, bold), allow_null);
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
        GdkRGBA **colour, const char *key, const char *colour_name)
{
    if (!*colour)
        *colour = g_new(GdkRGBA, 1);
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
    GdkRGBA *colour;

    g_return_if_fail(opts);
    g_return_if_fail(colour_name);
    g_return_if_fail(index >= 0 && index < 16);
    scheme = options_get_data(opts);
    g_return_if_fail(scheme);
    colour = &scheme->palette[index];
    sprintf(key, "%d", index);
    colour_scheme_set_colour(opts, scheme, &colour, key, colour_name);
}

static void colour_scheme_set_named_colour(Options *opts,
        const char *field_name,
        const char *colour_name, size_t member_offset)
{
    ColourScheme *scheme;
    GdkRGBA **member;

    g_return_if_fail(opts);
    scheme = options_get_data(opts);
    g_return_if_fail(scheme);
    member = (GdkRGBA **) (((char *) scheme) + member_offset);
    colour_scheme_set_colour(opts, scheme, member,
            field_name, colour_name);
}

void colour_scheme_set_cursor_colour(Options * opts, const char *colour_name)
{
    colour_scheme_set_named_colour(opts, "cursor", colour_name,
            offsetof(ColourScheme, cursor));
}

void colour_scheme_set_cursorfg_colour(Options * opts, const char *colour_name)
{
    colour_scheme_set_named_colour(opts, "cursorfg", colour_name,
            offsetof(ColourScheme, cursorfg));
}

void colour_scheme_set_foreground_colour(Options * opts,
        const char *colour_name)
{
    colour_scheme_set_named_colour(opts, "foreground", colour_name,
            offsetof(ColourScheme, foreground));
}

void colour_scheme_set_background_colour(Options * opts,
        const char *colour_name)
{
    colour_scheme_set_named_colour(opts, "background", colour_name,
            offsetof(ColourScheme, background));
}

void colour_scheme_set_bold_colour(Options * opts,
        const char *colour_name)
{
    colour_scheme_set_named_colour(opts, "bold", colour_name,
            offsetof(ColourScheme, bold));
}

/* vi:set sw=4 ts=4 noet cindent cino= */
