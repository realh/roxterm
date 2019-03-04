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
#ifndef __MULTITEXT_WINDOW_H
#define __MULTITEXT_WINDOW_H

#include "multitext-geometry-provider.h"

G_BEGIN_DECLS

#define MULTITEXT_TYPE_WINDOW multitext_window_get_type()
G_DECLARE_DERIVABLE_TYPE(MultitextWindow, multitext_window,
        MULTITEXT, WINDOW, GtkApplicationWindow);

/**
 * MultitextWindow:
 *
 * A GtkApplicationWindow whose size is determined by a widget containing a grid
 * of text in a fixed-width font. It overrides gtk_container_add etc to
 * determine whether a child widget is a MultitextGeometryProvider
 */
struct _MultitextWindowClass {
    GtkApplicationWindowClass parent_class;
};

/**
 * multitext_window_get_geometry_provider:
 *
 * Returns: (transfer none) (nullable): The child widget which is the geometry
 *          provider
 */
MultitextGeometryProvider *
multitext_window_get_geometry_provider(MultitextWindow *self);

G_END_DECLS

#endif /* __MULTITEXT_WINDOW_H */
