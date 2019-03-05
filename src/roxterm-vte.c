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
#include "roxterm-vte.h"

struct _RoxtermVte {
    VteTerminal parent_instance;
};

static void roxterm_vte_get_current_size(MultitextGeometryProvider *self,
        int *columns, int *rows)
{
    VteTerminal *vte = VTE_TERMINAL(self);
    if (columns)
        *columns = vte_terminal_get_column_count(vte);
    if (rows)
        *rows = vte_terminal_get_row_count(vte);
}

static void roxterm_vte_get_initial_size(MultitextGeometryProvider *self,
        int *columns, int *rows)
{
    // FIXME: If this is a newly created terminal it should read the size from
    // the profile, not the current size
    roxterm_vte_get_current_size(self, columns, rows);
}

static void roxterm_vte_get_cell_size(MultitextGeometryProvider *self,
        int *width, int *height)
{
    VteTerminal *vte = VTE_TERMINAL(self);
    if (width)
        *width = vte_terminal_get_char_width(vte);
    if (height)
        *height = vte_terminal_get_char_height(vte);
}

static void roxterm_vte_geometry_provider_init(
        MultitextGeometryProviderInterface *iface)
{
    iface->get_initial_size = roxterm_vte_get_initial_size;
    iface->get_current_size = roxterm_vte_get_current_size;
    iface->get_cell_size = roxterm_vte_get_cell_size;
}

G_DEFINE_TYPE_WITH_CODE(RoxtermVte, roxterm_vte, VTE_TYPE_TERMINAL,
        G_IMPLEMENT_INTERFACE(MULTITEXT_TYPE_GEOMETRY_PROVIDER,
            roxterm_vte_geometry_provider_init));

static void roxterm_vte_class_init(RoxtermVteClass *klass)
{
    (void) klass;
}

static void roxterm_vte_init(RoxtermVte *self)
{
    (void) self;
}

RoxtermVte *roxterm_vte_new(void)
{
    GObject *obj = g_object_new(ROXTERM_TYPE_VTE,
            NULL);
    return ROXTERM_VTE(obj);
}
