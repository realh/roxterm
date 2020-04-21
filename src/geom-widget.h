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

#ifndef GEOMETRY_WIDGET_H
#define GEOMETRY_WIDGET_H

#include "defns.h"

G_BEGIN_DECLS

#define GEOMETRY_TYPE_WIDGET geometry_widget_get_type()
G_DECLARE_INTERFACE(GeometryWidget, geometry_widget, GEOMETRY, WIDGET,
        GtkWidget);

/**
 * GeometryWidget:
 *
 * A widget containing text with fixed-width characters that determines the
 * size of the window, or a container for such a widget.
 */
struct _GeometryWidgetInterface
{
    GTypeInterface parent_iface;
    /**
     * GeometryWidgetInterface::get_target_size:
     * @columns: (out):
     * @rows: (out):
     */
    void (*get_target_size)(GeometryWidget *gw,
            int *columns, int *rows);
    /**
     * GeometryWidgetInterface::get_current_size:
     * @columns: (out):
     * @rows: (out):
     */
    void (*get_current_size)(GeometryWidget *gw,
            int *columns, int *rows);
    /**
     * GeometryWidgetInterface::get_cell_size:
     * @width: (out):
     * @height: (out):
     */
    void (*get_cell_size)(GeometryWidget *gw,
            int *width, int *height);
    /**
     * GeometryWidgetInterface::set_active:
     */
    void (*set_active)(GeometryWidget *gw, gboolean active);
    /**
     * GeometryWidgetInterface::get_active:
     * Returns: (transfer none):
     */
    GeometryWidget *(*get_active)(GeometryWidget *gw);
    /**
     * GeometryWidgetInterface::is_active:
     */
    gboolean (*is_active)(GeometryWidget *gw);
    /**
     * GeometryWidgetInterface::confirm_close:
     */
    gboolean (*confirm_close)(GeometryWidget *gw);
    /**
     * GeometryWidgetInterface::set_alloc_for_measurement:
     *
     * @afm: Any size-allocate events received while this is TRUE should
     *       be treated for the purpose of measurement only, they should not
     *       influence the widget's target size
     */
    void (*set_alloc_for_measurement)(GeometryWidget *gw,
            gboolean afm);
    guint changed_signal;
};

/**
 * geometry_widget_get_target_size: (slot get_target_size)
 *
 * Gets the size, in characters, the widget wants to be eg when opening a new
 * window
 *
 * @columns: (out):
 * @rows: (out):
 */
void
geometry_widget_get_target_size(GeometryWidget *gw,
        int *columns, int *rows);

/**
 * geometry_widget_get_current_size: (slot get_current_size)
 *
 * Gets the size, in characters, the widget is at the moment
 *
 * @columns: (out):
 * @rows: (out):
 */
void
geometry_widget_get_current_size(GeometryWidget *gw,
        int *columns, int *rows);

/**
 * geometry_widget_get_cell_size: (slot get_cell_size)
 *
 * Get the size of each text cell in pixels
 *
 * @width: (out):
 * @height: (out):
 */
void
geometry_widget_get_cell_size(GeometryWidget *gw,
        int *width, int *height);

/**
 * geometry_widget_set_active: (slot set_active)
 *
 * When a GeometryWidget is active it means it's the widget that controls
 * the window's geometry. The active widget may change when they're the child
 * of a GtkNotebook.
 */
void geometry_widget_set_active(GeometryWidget *gw,
        gboolean active);

/**
 * geometry_widget_get_active: (slot get_active)
 * Returns: (transfer none):
 * 
 * Returns gw for child widgets, the active child for containers.
 */
GeometryWidget *geometry_widget_get_active(GeometryWidget *gw);

/**
 * geometry_widget_is_active: (slot is_active)
 * Returns: Whether gw was set active by geometry_widget_set_active().
 */
gboolean geometry_widget_is_active(GeometryWidget *gw);

/**
 * geometry_widget_confirm_close: (slot confirm_close):
 * Returns: TRUE if the user should be asked to confirm before detsroying
 *          this widget
 */
gboolean geometry_widget_confirm_close(GeometryWidget *gw);

/**
 * geometry_widget_set_alloc_for_measurement: (slot set_alloc_for_measurement)
 *
 * @afm: Any size-allocate events received while this is TRUE should
 *       be treated for the purpose of measurement only, they should not
 *       influence the widget's target size
 */
void geometry_widget_set_alloc_for_measurement(
        GeometryWidget *gw, gboolean afm);

/**
 * geometry_widget_geometry_changed: (method):
 * Raises a "geometry-changed" signal with no arguments, but only if gw is
 * currently active.
 */
void geometry_widget_geometry_changed(GeometryWidget *gw);

G_END_DECLS

#endif /* GEOMETRY_WIDGET_H */
