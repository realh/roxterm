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

#include "config.h"
#include "multitext-notebook.h"

struct _MultitextNotebook {
    GtkNotebook parent_instance;
};

static void multitext_notebook_page_removed(GtkNotebook *gnb,
        UNUSED GtkWidget *child, UNUSED guint page_num)
{
    MultitextNotebook *self = MULTITEXT_NOTEBOOK(gnb);
    GtkContainer *cnb = GTK_CONTAINER(self);
    int n_pages = gtk_notebook_get_n_pages(gnb);
    if (!n_pages)
    {
        gtk_widget_destroy(
                GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(self))));
    }
    if (n_pages == 1)
    {
        GtkWidget *child = gtk_container_get_children(cnb)->data;
        MultitextTabLabel *label
            = MULTITEXT_TAB_LABEL(gtk_notebook_get_tab_label(gnb, child));
        multitext_notebook_set_tab_label_homogeneous(self, child, FALSE);
        multitext_tab_label_set_single(label, TRUE);
    }
}

G_DEFINE_TYPE(MultitextNotebook, multitext_notebook, GTK_TYPE_NOTEBOOK);

static void multitext_notebook_constructed(GObject *obj)
{
    G_OBJECT_CLASS(multitext_notebook_parent_class)->constructed(obj);
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(obj), TRUE);
}

static void multitext_notebook_class_init(MultitextNotebookClass *klass)
{
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    oklass->constructed = multitext_notebook_constructed;
    GtkNotebookClass *gnklass = GTK_NOTEBOOK_CLASS(klass);
    gnklass->page_removed = multitext_notebook_page_removed;
}

static void multitext_notebook_init(UNUSED MultitextNotebook *self)
{
}

MultitextNotebook *multitext_notebook_new(void)
{
    GObject *obj = g_object_new(MULTITEXT_TYPE_NOTEBOOK, NULL);
    return MULTITEXT_NOTEBOOK(obj);
}

void multitext_notebook_set_tab_label_homogeneous(MultitextNotebook *self,
        GtkWidget *page, gboolean homogeneous)
{
    gtk_container_child_set(GTK_CONTAINER(self), page,
            "tab-expand", homogeneous,
            "tab-fill", TRUE,
            NULL);
}

void multitext_notebook_remove_page(MultitextNotebook *self, GtkWidget *page)
{
    GtkContainer *cnb = GTK_CONTAINER(self);
    gtk_container_remove(cnb, page);
}
