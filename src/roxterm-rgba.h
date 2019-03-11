/*
    roxterm - PROFILE/GTK terminal emulator with tabs
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
#ifndef __ROXTERM_RGBA_H
#define __ROXTERM_RGBA_H

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef guint32 RoxtermRGBAInt;

typedef guint8 RoxtermRGBAChannel;

typedef union {
    RoxtermRGBAInt v;
    struct {
        RoxtermRGBAChannel r, g, b, a;
    };
} RoxtermRGBA;

RoxtermRGBA roxterm_rgba_from_gdk(const GdkRGBA *grgba);

RoxtermRGBA roxterm_rgba_parse(const char *s);

/**
 * roxterm_rgba_to_gdk:
 *
 * @grgba: (out) (caller-allocates):
 */
void roxterm_rgba_to_gdk(RoxtermRGBA rrgba, GdkRGBA *grgba);

/**
 * roxterm_rgba_to_string:
 *
 * Returns: (transfer full):
 */
char *roxterm_rgba_to_string(RoxtermRGBA rrgba);

G_END_DECLS

#endif /* __ROXTERM_RGBA_H */
