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

#include "multitext-geometry-provider.h"

G_DEFINE_INTERFACE(MultitextGeometryProvider, multitext_geometry_provider,
        GTK_TYPE_WIDGET);

void
multitext_geometry_provider_default_init(MultitextGeometryProviderInterface
        *iface)
{
    (void) iface;
}

void
multitext_geometry_provider_get_initial_size(MultitextGeometryProvider *self,
        int *columns, int *rows)
{
    g_return_if_fail(MULTITEXT_IS_GEOMETRY_PROVIDER(self));
    MultitextGeometryProviderInterface *iface
            = MULTITEXT_GEOMETRY_PROVIDER_GET_IFACE(self);
    g_return_if_fail(iface->get_initial_size != NULL);
    iface->get_initial_size(self, columns, rows);
}

void
multitext_geometry_provider_get_current_size(MultitextGeometryProvider *self,
        int *columns, int *rows)
{
    g_return_if_fail(MULTITEXT_IS_GEOMETRY_PROVIDER(self));
    MultitextGeometryProviderInterface *iface
            = MULTITEXT_GEOMETRY_PROVIDER_GET_IFACE(self);
    g_return_if_fail(iface->get_current_size != NULL);
    iface->get_current_size(self, columns, rows);
}

void
multitext_geometry_provider_get_cell_size(MultitextGeometryProvider *self,
        int *width, int *height)
{
    g_return_if_fail(MULTITEXT_IS_GEOMETRY_PROVIDER(self));
    MultitextGeometryProviderInterface *iface
            = MULTITEXT_GEOMETRY_PROVIDER_GET_IFACE(self);
    g_return_if_fail(iface->get_cell_size != NULL);
    iface->get_cell_size(self, width, height);
}
