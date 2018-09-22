#ifndef COLOURSCHEME_H
#define COLOURSCHEME_H
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


/* Colours are represented by strings as understood by gdk_color/rgba_parse */

#include "options.h"

Options *colour_scheme_lookup_and_ref(const char *scheme);

gboolean colour_scheme_unref(Options * opts);

int colour_scheme_get_palette_size(Options * opts);

/* A palette always has 16 valid entries even if logical palette size is
 * smaller */
GdkRGBA *colour_scheme_get_palette(Options * opts);

/* If allow_null is FALSE a default colour is used */
GdkRGBA *colour_scheme_get_cursor_colour(Options * opts,
        gboolean allow_null);

GdkRGBA *colour_scheme_get_cursorfg_colour(Options * opts,
        gboolean allow_null);

GdkRGBA *colour_scheme_get_bold_colour(Options * opts,
        gboolean allow_null);

GdkRGBA *colour_scheme_get_foreground_colour(Options * opts,
        gboolean allow_null);

GdkRGBA *colour_scheme_get_background_colour(Options * opts,
        gboolean allow_null);

void colour_scheme_set_palette_size(Options * opts, int size);

void colour_scheme_set_palette_entry(Options * opts, int index,
		const char *colour_name);

void colour_scheme_set_cursor_colour(Options * opts, const char *colour_name);

void
colour_scheme_set_cursorfg_colour(Options * opts, const char *colour_name);

void colour_scheme_set_bold_colour(Options * opts, const char *colour_name);

void colour_scheme_set_foreground_colour(Options * opts,
		const char *colour_name);

void colour_scheme_set_background_colour(Options * opts,
		const char *colour_name);

void colour_scheme_reset_cached_data(Options *opts);

#endif /* COLOURSCHEME_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
