/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2020 Tony Houghton <h@realh.co.uk>

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

#include "geom-container.h"
#include "geom-widget.h"

static void
geometry_container_get_target_size(GeometryWidget *gw,
        int *columns, int *rows)
{
    g_return_if_fail(GEOMETRY_IS_CONTAINER(gw));
    GeometryWidget *child = geometry_widget_get_active((GeometryWidget *) gw);
    geometry_widget_get_target_size(child, columns, rows);
}

static void
geometry_container_get_current_size(GeometryWidget *gw,
        int *columns, int *rows)
{
    g_return_if_fail(GEOMETRY_IS_CONTAINER(gw));
    GeometryWidget *child = geometry_widget_get_active((GeometryWidget *) gw);
    geometry_widget_get_current_size(child, columns, rows);
}

static void
geometry_container_get_cell_size(GeometryWidget *gw,
        int *width, int *height)
{
    g_return_if_fail(GEOMETRY_IS_CONTAINER(gw));
    GeometryWidget *child = geometry_widget_get_active((GeometryWidget *) gw);
    geometry_widget_get_cell_size(child, width, height);
}

static void geometry_container_set_active(GeometryWidget *gw,
        gboolean active)
{
    g_return_if_fail(GEOMETRY_IS_CONTAINER(gw));
    GeometryWidget *child = geometry_widget_get_active((GeometryWidget *) gw);
    if (child)
        geometry_widget_set_active(child, active);
}

static GeometryWidget *geometry_container_get_active(GeometryWidget *gw)
{
    return geometry_widget_get_active(
            geometry_container_get_current((GeometryContainer *) gw));
}

static gboolean geometry_container_is_active(GeometryWidget *gw)
{
    g_return_val_if_fail(GEOMETRY_IS_CONTAINER(gw), FALSE);
    g_return_val_if_fail(GTK_IS_BIN(gw), FALSE);
    GtkWidget *child = gtk_bin_get_child((GtkBin *) gw);
    return child ? geometry_widget_is_active((GeometryWidget *) child) : FALSE;
}

gboolean
geometry_container_confirm_close(GeometryWidget *gw)
{
    g_return_val_if_fail(GEOMETRY_IS_CONTAINER(gw), FALSE);
    GeometryWidget *child = geometry_widget_get_active((GeometryWidget *) gw);
    return child ? geometry_widget_confirm_close(child) : FALSE;
}

void geometry_container_set_alloc_for_measurement(
        GeometryWidget *gw, gboolean afm)
{
    g_return_if_fail(GEOMETRY_IS_CONTAINER(gw));
    GeometryWidget *child = geometry_widget_get_active((GeometryWidget *) gw);
    geometry_widget_set_alloc_for_measurement(child, afm);
}

static void geometry_container_add(GeometryContainer *gc, GtkWidget *child,
        UNUSED gpointer handle)
{
    if (GEOMETRY_IS_WIDGET(child))
        g_signal_connect_swapped(child, "change-geometry",
                G_CALLBACK(geometry_widget_geometry_changed), gc);
}

static void geometry_container_remove(GeometryContainer *gc, GtkWidget *child,
        UNUSED gpointer handle)
{
    if (GEOMETRY_IS_WIDGET(child))
        g_signal_handlers_disconnect_by_data(child, gc);
}

void geometry_container_add_signal_hooks(GeometryContainer *gc)
{
    g_signal_connect(gc, "add", G_CALLBACK(geometry_container_add), NULL);
    g_signal_connect(gc, "remove",
            G_CALLBACK(geometry_container_remove), NULL);
}

static void geometry_container_implement_geometry_widget(
        GeometryWidgetInterface *iface)
{
    iface->get_target_size = geometry_container_get_target_size;
    iface->get_current_size = geometry_container_get_current_size;
    iface->get_cell_size = geometry_container_get_cell_size;
    iface->set_active = geometry_container_set_active;
    iface->get_active = geometry_container_get_active;
    iface->is_active = geometry_container_is_active;
    iface->confirm_close = geometry_container_confirm_close;
    iface->set_alloc_for_measurement =
        geometry_container_set_alloc_for_measurement;
}

static GeometryWidget *
geometry_container_default_get_current(GeometryContainer *gc)
{
    g_return_val_if_fail(GEOMETRY_IS_CONTAINER(gc), NULL);
    g_return_val_if_fail(GTK_IS_BIN(gc), NULL);
    return GEOMETRY_WIDGET(gtk_bin_get_child((GtkBin *) gc));
}

G_DEFINE_INTERFACE(GeometryContainer, geometry_container,
        GTK_TYPE_WIDGET);

void geometry_container_default_init(UNUSED GeometryContainerInterface *iface)
{
    iface->get_current = geometry_container_default_get_current;
}

GeometryWidget *geometry_container_get_current(GeometryContainer *gc)
{
    g_return_val_if_fail(GEOMETRY_IS_CONTAINER(gc), NULL);
    GeometryContainerInterface *iface = GEOMETRY_CONTAINER_GET_IFACE(gc);
    g_return_val_if_fail(iface->get_current != NULL, NULL);
    return iface->get_current(gc);
}

static void geometry_container_disconnect_each_child(GtkWidget *child,
        gpointer gc)
{
    if (GEOMETRY_IS_WIDGET(child))
        g_signal_handlers_disconnect_by_data(child, gc);
}

void geometry_container_remove_signal_hooks(GeometryContainer *gc)
{
    gtk_container_foreach((GtkContainer *) gc,
            geometry_container_disconnect_each_child, gc);
}
