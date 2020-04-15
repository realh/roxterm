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

#include "geom-widget.h"

G_DEFINE_INTERFACE(GeometryWidget, multitext_geometry_widget,
        GTK_TYPE_WIDGET);

void
multitext_geometry_widget_default_init(
        UNUSED GeometryWidgetInterface *iface)
{
}

void
multitext_geometry_widget_get_target_size(GeometryWidget *gw,
        int *columns, int *rows)
{
    g_return_if_fail(GEOMETRY_IS_WIDGET(gw));
    GeometryWidgetInterface *iface
            = GEOMETRY_WIDGET_GET_IFACE(gw);
    g_return_if_fail(iface->get_target_size != NULL);
    iface->get_target_size(gw, columns, rows);
}

void
multitext_geometry_widget_get_current_size(GeometryWidget *gw,
        int *columns, int *rows)
{
    g_return_if_fail(GEOMETRY_IS_WIDGET(gw));
    GeometryWidgetInterface *iface
            = GEOMETRY_WIDGET_GET_IFACE(gw);
    g_return_if_fail(iface->get_current_size != NULL);
    iface->get_current_size(gw, columns, rows);
}

void
multitext_geometry_widget_get_cell_size(GeometryWidget *gw,
        int *width, int *height)
{
    g_return_if_fail(GEOMETRY_IS_WIDGET(gw));
    GeometryWidgetInterface *iface
            = GEOMETRY_WIDGET_GET_IFACE(gw);
    g_return_if_fail(iface->get_cell_size != NULL);
    iface->get_cell_size(gw, width, height);
}

void multitext_geometry_widget_set_active(GeometryWidget *gw,
        gboolean active)
{
    g_return_if_fail(GEOMETRY_IS_WIDGET(gw));
    GeometryWidgetInterface *iface
            = GEOMETRY_WIDGET_GET_IFACE(gw);
    g_return_if_fail(iface->set_active != NULL);
    iface->set_active(gw, active);
}

gboolean
multitext_geometry_widget_confirm_close(GeometryWidget *gw)
{
    g_return_val_if_fail(GEOMETRY_IS_WIDGET(gw), FALSE);
    GeometryWidgetInterface *iface
            = GEOMETRY_WIDGET_GET_IFACE(gw);
    g_return_val_if_fail(iface->confirm_close != NULL, FALSE);
    return iface->confirm_close(gw);
}

void multitext_geometry_widget_set_alloc_for_measurement(
        GeometryWidget *gw, gboolean afm)
{
    g_return_if_fail(GEOMETRY_IS_WIDGET(gw));
    GeometryWidgetInterface *iface
            = GEOMETRY_WIDGET_GET_IFACE(gw);
    g_return_if_fail(iface->set_alloc_for_measurement != NULL);
    iface->set_alloc_for_measurement(gw, afm);
}
