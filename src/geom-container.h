/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2020 Tony Houghton <h@realh.co.uk>

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

#ifndef GEOMETRY_CONTAINER_H
#define GEOMETRY_CONTAINER_H

#include "geom-widget.h"

G_BEGIN_DECLS

#define GEOMETRY_TYPE_CONTAINER geometry_container_get_type()
G_DECLARE_INTERFACE(GeometryContainer, geometry_container, GEOMETRY, CONTAINER,
        GtkContainer);

/**
 * GeometryContainer:
 *
 * A mix-in interface which implements GeometryWidget by forwarding its methods
 * to the active child of a container. The implementation of get_active()
 * assumes the container is a GtkBin.
 */
struct _GeometryContainerInterface
{
    GTypeInterface parent_iface;
    /**
     * GeometryContainerInterface::get_current:
     * Returns: (transfer none):
     */
    GeometryWidget *(*get_current)(GeometryContainer *gc);
};

/**
 * geometry_container_get_current: (slot get_current):
 * Returns: (transfer none):
 *
 * This returns the current immediate child, whereas
 * geometry_widget_get_active() recurses down the hierarchy
 */
GeometryWidget *geometry_container_get_current(GeometryContainer *gc);

/**
 * geometry_container_add_signal_hooks: (method):
 * Implementations need to call this during their construction to connect to
 * GtkContainer::add and GtkContainer::remove so that they can forward
 * "geometry-changed" signals.
 */
void geometry_container_add_signal_hooks(GeometryContainer *gc);

/**
 * geometry_container_dispose: (method):
 * Implementations need to call this from their dispose methods to remove any
 * outstanding signal connections. It doesn't chain up.
 */
void geometry_container_remove_signal_hooks(GeometryContainer *gc);

G_END_DECLS

#endif /* GEOMETRY_CONTAINER_H */
