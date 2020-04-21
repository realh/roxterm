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

#ifndef GEOMETRY_NOTEBOOK_H
#define GEOMETRY_NOTEBOOK_H

#include "geom-container.h"

G_BEGIN_DECLS

#define GEOMETRY_TYPE_NOTEBOOK geometry_notebook_get_type()
G_DECLARE_DERIVABLE_TYPE(GeometryNotebook, geometry_notebook,
        GEOMETRY, NOTEBOOK, GtkNotebook);

/**
 * GeometryNotebook: A GtkNotebook which implements GeometryContainer
 */
struct _GeometryNotebookClass
{
    GtkNotebookClass parent_class;
};

/**
 * geometry_notebook_new: (constructor):
 */
GeometryNotebook *geometry_notebook_new(void);

G_END_DECLS

#endif /* GEOMETRY_NOTEBOOK_H */
