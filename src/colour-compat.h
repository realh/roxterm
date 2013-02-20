#ifndef COLOUR_COMPAT_H
#define COLOUR_COMPAT_H
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

/* Migration from GdkColor to GdkRGBA */

#ifndef DEFNS_H
#include "defns.h"
#endif

#if GTK_CHECK_VERSION(3, 0, 0)

#define COLOUR_T GdkRGBA
#define COLOUR_EQUAL gdk_rgba_equal
#define COLOUR_PARSE gdk_rgba_parse
#define COLOUR_BUTTON_SET(w, c) \
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(w), c)
#define COLOUR_BUTTON_GET(w, p) \
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(w), p)
#define COLOUR_SET_VTE(f) vte_terminal_set_color##f##_rgba
#define COLOUR_SPRINTF(s, c) sprintf(s, "#%04hx%04hx%04hx", \
        (guint16) (c->red * 65535), \
        (guint16) (c->green * 65535), \
        (guint16) (c->blue * 65535))

#else

#define COLOUR_T GdkColor
#define COLOUR_EQUAL gdk_color_equal
#define COLOUR_PARSE gdk_color_parse
#define COLOUR_BUTTON_SET(w, c) \
        gtk_color_button_set_color(GTK_COLOR_BUTTON(w), c)
#define COLOUR_BUTTON_GET(w, p) \
        gtk_color_button_get_color(GTK_COLOR_BUTTON(w), p)
#define COLOUR_SET_VTE(f) vte_terminal_set_color##f
#define COLOUR_SPRINTF(s, c) sprintf(c, "#%04hx%04hx%04hx", \
        c->red, c->green, c->blue)

#endif /* GTK_CHECK_VERSION */

#ifdef HAVE_VTE_TERMINAL_SET_BACKGROUND_TINT_RGBA
#define COLOUR_SET_VTE_TINT vte_terminal_set_background_tint_rgba
#else
#define COLOUR_SET_VTE_TINT vte_terminal_set_background_tint_color
#endif

#endif /* COLOUR_COMPAT_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
