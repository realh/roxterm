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

#include "geom-notebook.h"
#include "geom-widget.h"

typedef struct
{
    gboolean active;
} GeometryNotebookPrivate;

static void geometry_notebook_implement_geometry_container(
        GeometryContainerInterface *iface);

static void
geometry_notebook_implement_geometry_widget(GeometryWidgetInterface *iface);

G_DEFINE_TYPE_WITH_CODE(GeometryNotebook, geometry_notebook, GTK_TYPE_NOTEBOOK,
        G_ADD_PRIVATE(GeometryNotebook)
        G_IMPLEMENT_INTERFACE(GEOMETRY_TYPE_CONTAINER,
            geometry_notebook_implement_geometry_container)
        G_IMPLEMENT_INTERFACE(GEOMETRY_TYPE_WIDGET,
            geometry_notebook_implement_geometry_widget)
        );

static GeometryWidget *geometry_notebook_get_current(GeometryContainer *gc)
{
    GtkNotebook *gn = GTK_NOTEBOOK(gc);
    g_return_val_if_fail(gn, NULL);
    int pn = gtk_notebook_get_current_page(gn);
    GtkWidget *w = gtk_notebook_get_nth_page(gn, pn);
    GeometryWidget *gw = GEOMETRY_WIDGET(w);
    g_return_val_if_fail(gw, NULL);
    return gw;
}

static void geometry_notebook_implement_geometry_container(
        GeometryContainerInterface *iface)
{
    iface->get_current = geometry_notebook_get_current;
}

static void geometry_container_set_active(GeometryWidget *gw, gboolean active)
{
    GeometryNotebookPrivate *priv =
        geometry_notebook_get_instance_private(GEOMETRY_NOTEBOOK(gw));
    if (priv)
        priv->active = active;
    GtkNotebook *gn = GTK_NOTEBOOK(gw);
    int pn = active ? gtk_notebook_get_current_page(gn) : -1;
    int count = gtk_notebook_get_n_pages(gn);
    for (int n = 0; n < count; ++n)
    {
        GtkWidget *child = gtk_notebook_get_nth_page(gn, n);
        geometry_widget_set_active(GEOMETRY_WIDGET(child),
                n == pn ? active : FALSE);
    }
}

static gboolean geometry_container_is_active(GeometryWidget *gw)
{
    GeometryNotebookPrivate *priv =
        geometry_notebook_get_instance_private(GEOMETRY_NOTEBOOK(gw));
    return priv ? priv->active : FALSE;
}

static void
geometry_notebook_implement_geometry_widget(GeometryWidgetInterface *iface)
{
    geometry_container_implement_geometry_widget(iface);
    iface->set_active = geometry_container_set_active;
}

GeometryNotebook *geometry_notebook_new(void)
{
    return (GeometryNotebook *) g_object_new(GEOMETRY_TYPE_NOTEBOOK, NULL);
}

static void geometry_notebook_dispose(GObject *obj)
{
    geometry_container_remove_signal_hooks((GeometryContainer *) obj);
    G_OBJECT_CLASS(geometry_notebook_parent_class)->dispose(obj);
}

static void geometry_notebook_class_init(GeometryNotebookClass *klass)
{
    GObjectClass *gklass = G_OBJECT_CLASS(klass);
    gklass->dispose = geometry_notebook_dispose;
}

static void geometry_notebook_init(GeometryNotebook *gn)
{
    geometry_container_add_signal_hooks((GeometryContainer *) gn);
}
