/*
    roxterm4 - VTE/GTK terminal emulator with tabs
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

#ifndef __MULTITEXT_GEOMETRY_PROVIDER_H
#define __MULTITEXT_GEOMETRY_PROVIDER_H

#include "multitext-tab-label.h"

G_BEGIN_DECLS

#define MULTITEXT_TYPE_GEOMETRY_PROVIDER multitext_geometry_provider_get_type()
G_DECLARE_INTERFACE (MultitextGeometryProvider, multitext_geometry_provider,
        MULTITEXT, GEOMETRY_PROVIDER, GtkWidget)

/**
 * MultitextGeometryProvider:
 *
 * A widget containing text with fixed-width characters that determines the
 * size of the window, or a container for such a widget.
 */
struct _MultitextGeometryProviderInterface
{
    GTypeInterface parent_iface;
    /**
     * MultitextGeometryProviderInterface::get_initial_size:
     *
     * @columns: (out):
     * @rows: (out):
     */
    void (*get_initial_size)(MultitextGeometryProvider *self,
            int *columns, int *rows);
    /**
     * MultitextGeometryProviderInterface::get_current_size:
     *
     * @columns: (out):
     * @rows: (out):
     */
    void (*get_current_size)(MultitextGeometryProvider *self,
            int *columns, int *rows);
    /**
     * MultitextGeometryProviderInterface::get_cell_size:
     *
     * @width: (out):
     * @height: (out):
     */
    void (*get_cell_size)(MultitextGeometryProvider *self,
            int *width, int *height);
    /**
     * MultitextGeometryProviderInterface::set_active:
     */
    void (*set_active)(MultitextGeometryProvider *self,
            gboolean active);
    /**
     * MultitextGeometryProviderInterface::confirm_close:
     */
    gboolean (*confirm_close)(MultitextGeometryProvider *self);
    /**
     * MultitextGeometryProviderInterface::get_tab_label:
     *
     * Returns: (nullable) (transfer none):
     */
    MultitextTabLabel *(*get_tab_label)(MultitextGeometryProvider *self);
    /**
     * MultitextGeometryProviderInterface::set_tab_label:
     *
     * @label: (nullable) (transfer none):
     */
    void (*set_tab_label)(MultitextGeometryProvider *self,
            MultitextTabLabel *label);
};

/**
 * multitext_geometry_provider_get_initial_size:
 *
 * Gets the size, in characters, the widget wants to be eg when opening a new
 * window
 *
 * @columns: (out):
 * @rows: (out):
 */
void
multitext_geometry_provider_get_initial_size(MultitextGeometryProvider *self,
        int *columns, int *rows);

/**
 * multitext_geometry_provider_get_current_size:
 *
 * Gets the size, in characters, the widget is at the moment
 *
 * @columns: (out):
 * @rows: (out):
 */
void
multitext_geometry_provider_get_current_size(MultitextGeometryProvider *self,
        int *columns, int *rows);

/**
 * multitext_geometry_provider_get_cell_size:
 *
 * Get the size of each text cell in pixels
 *
 * @width: (out):
 * @height: (out):
 */
void
multitext_geometry_provider_get_cell_size(MultitextGeometryProvider *self,
        int *width, int *height);

/**
 * multitext_geometry_provider_set_active:
 *
 * When a GeometryProvider is active it means it's the widget that controls
 * the window's geometry
 */
void multitext_geometry_provider_set_active(MultitextGeometryProvider *self,
        gboolean active);

/**
 * multitext_geometry_provider_confirm_close:
 *
 * Returns: TRUE if the user should be asked for confirmation before
 *          closing this page
 */
gboolean
multitext_geometry_provider_confirm_close(MultitextGeometryProvider *self);

/**
 * multitext_geometry_get_tab_label:
 *
 * Returns: (nullable) (transfer none):
 */
MultitextTabLabel *
multitext_geometry_provider_get_tab_label(MultitextGeometryProvider *self);

/**
 * multitext_geometry_set_tab_label:
 *
 * @label: (nullable) (transfer none):
 */
void multitext_geometry_provider_set_tab_label(MultitextGeometryProvider *self,
        MultitextTabLabel *label);

#endif /* __MULTITEXT_GEOMETRY_PROVIDER_H */
