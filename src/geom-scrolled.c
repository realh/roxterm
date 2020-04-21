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

#include "geom-scrolled.h"
#include "geom-widget.h"

typedef struct
{
    gboolean active;
} GeometryScrolledWindowPrivate;

static void geometry_scrolled_window_implement_geometry_container(
        UNUSED GeometryContainerInterface *iface)
{
    // Just inherit the default
}

G_DEFINE_TYPE_WITH_CODE(GeometryScrolledWindow, geometry_scrolled_window,
        GTK_TYPE_SCROLLED_WINDOW,
        G_ADD_PRIVATE(GeometryScrolledWindow)
        G_IMPLEMENT_INTERFACE(GEOMETRY_TYPE_CONTAINER,
            geometry_scrolled_window_implement_geometry_container)
        G_IMPLEMENT_INTERFACE(GEOMETRY_TYPE_WIDGET,
            geometry_container_implement_geometry_widget)
        );

static gboolean geometry_container_is_active(GeometryWidget *gw)
{
    GeometryScrolledWindowPrivate *priv =
        geometry_scrolled_window_get_instance_private(
                GEOMETRY_SCROLLED_WINDOW(gw));
    return priv ? priv->active : FALSE;
}

GeometryScrolledWindow *geometry_scrolled_window_new(void)
{
    return (GeometryScrolledWindow *)
        g_object_new(GEOMETRY_TYPE_SCROLLED_WINDOW, NULL);
}

static void geometry_scrolled_window_dispose(GObject *obj)
{
    geometry_container_remove_signal_hooks((GeometryContainer *) obj);
    G_OBJECT_CLASS(geometry_scrolled_window_parent_class)->dispose(obj);
}

static void
geometry_scrolled_window_class_init(GeometryScrolledWindowClass *klass)
{
    GObjectClass *gklass = G_OBJECT_CLASS(klass);
    gklass->dispose = geometry_scrolled_window_dispose;
}

static void geometry_scrolled_window_init(GeometryScrolledWindow *gn)
{
    geometry_container_add_signal_hooks((GeometryContainer *) gn);
}
