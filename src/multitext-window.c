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

#include "multitext-window.h"

typedef struct {
    MultitextGeometryProvider *gp;
    gulong anchored_sig_tag;
    gulong destroy_sig_tag;
    MultitextNotebook *notebook;
} MultitextWindowPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(MultitextWindow, multitext_window,
        GTK_TYPE_APPLICATION_WINDOW);

enum {
    PROP_GEOM_PROV = 1,
    N_PROPS
};

static GParamSpec *multitext_window_props[N_PROPS] = {NULL};

static void multitext_window_get_property(GObject *obj, guint prop_id,
        GValue *value, GParamSpec *pspec)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(MULTITEXT_WINDOW(obj));
    g_return_if_fail(priv != NULL);
    switch (prop_id)
    {
        case PROP_GEOM_PROV:
            g_value_set_object(value, priv->gp);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

static void multitext_window_disconnect_provider_signals(MultitextWindow *self)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    g_signal_handler_disconnect(priv->gp, priv->anchored_sig_tag);
    priv->anchored_sig_tag = 0;
    g_signal_handler_disconnect(priv->gp, priv->destroy_sig_tag);
    priv->destroy_sig_tag = 0;
    priv->gp = NULL;
}

static void multitext_window_geometry_provider_anchored(
        MultitextGeometryProvider *gp, MultitextWindow *previous_top_level,
        MultitextWindow *self)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    if (gp == priv->gp && previous_top_level == self)
    {
        multitext_window_disconnect_provider_signals(self);
    }
}

static void multitext_window_geometry_provider_destroyed(
        MultitextGeometryProvider *gp, MultitextWindow *self)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    if (gp == priv->gp)
    {
        multitext_window_disconnect_provider_signals(self);
    }
}

static void multitext_window_dispose(GObject *obj)
{
    MultitextWindow *self = MULTITEXT_WINDOW(obj);
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    if (priv->gp)
        multitext_window_disconnect_provider_signals(self);
}

static void multitext_window_class_init(MultitextWindowClass *klass)
{
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    // Accessors must be set before installing properties
    oklass->get_property = multitext_window_get_property;
    multitext_window_props[PROP_GEOM_PROV] =
            g_param_spec_object("geometry-provider", "geometry_provider",
            "MultitextGeometryProvider", MULTITEXT_TYPE_GEOMETRY_PROVIDER,
            G_PARAM_READABLE);
    g_object_class_install_properties(oklass, N_PROPS, multitext_window_props);
}

static void multitext_window_init(MultitextWindow *self)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    g_return_if_fail(priv != NULL);
    priv->notebook = multitext_notebook_new();
    gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(priv->notebook));
}

MultitextGeometryProvider *
multitext_window_get_geometry_provider(MultitextWindow *self)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    g_return_val_if_fail(priv != NULL, NULL);
    return priv->gp;
}

void multitext_window_set_geometry_provider(MultitextWindow *self,
        MultitextGeometryProvider *gp)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    g_return_if_fail(priv != NULL);
    if (priv->gp)
    {
        multitext_geometry_provider_set_active(gp, FALSE);
        multitext_window_disconnect_provider_signals(self);
    }
    if (gp) 
    {
        priv->anchored_sig_tag = g_signal_connect(gp, "hierarchy-changed",
                G_CALLBACK(multitext_window_geometry_provider_anchored), self);
        priv->destroy_sig_tag = g_signal_connect(gp, "destroy",
                G_CALLBACK(multitext_window_geometry_provider_destroyed), self);
        multitext_geometry_provider_set_active(gp, TRUE);
    }
    priv->gp = gp;
}

MultitextNotebook *multitext_window_get_notebook(MultitextWindow *self)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    return priv->notebook;
}

void multitext_window_set_initial_size(MultitextWindow *self)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    GtkWidget *tlw = GTK_WIDGET(self);
    GtkWidget *nbw = GTK_WIDGET(priv->notebook);
    // Show everything except the top-level first, otherwise measured sizes
    // tend to be nonsense
    gtk_widget_realize(nbw);
    gtk_widget_show_all(nbw);
    int columns, rows;
    int cell_width, cell_height;
    int target_width, target_height;
    int current_width, current_height;
    multitext_geometry_provider_get_initial_size(priv->gp, &columns, &rows);
    multitext_geometry_provider_get_cell_size(priv->gp,
            &cell_width, &cell_height);
    target_width = cell_width * columns;
    target_height = cell_height * rows;
    // target_* hold the target size of the body of the text widget, excluding
    // padding, borders etc
    multitext_geometry_provider_get_current_size(priv->gp, &columns, &rows);
    current_width = cell_width * columns;
    current_height = cell_height * rows;
    // target_* hold the current size of the body of the text widget, excluding
    // padding, borders etc; this may differ from target_*
    GtkWidget *gpw = GTK_WIDGET(priv->gp);
    int min_w, nat_w, min_h, nat_h;
    gtk_widget_get_preferred_width(gpw, &min_w, &nat_w);
    gtk_widget_get_preferred_height(gpw, &min_h, &nat_h);
    int diff_w = nat_w - current_width;
    int diff_h = nat_h - current_height;
    // The difference between natural size and current_size should be the size
    // of padding/border the text widget has when snapped to geometry hints.
    gtk_widget_get_preferred_width(nbw, &min_w, &nat_w);
    gtk_widget_get_preferred_height(nbw, &min_h, &nat_h);
    // Now we know the minimum size for the notebook
    GtkAllocation nba, gpa;
    nba.width = min_w;
    nba.height = min_h;
    nba.x = nba.y = 0;
    gtk_widget_size_allocate(nbw, &nba);
    // Allocating the min size to the window gives it and its children valid
    // allocations with correct relative sizes
    gtk_widget_get_allocation(nbw, &nba);
    gtk_widget_get_allocation(gpw, &gpa);
    // Now we can read the difference in size between the notebook and the text
    // widget
    diff_w += nba.width - gpa.width;
    diff_h += nba.height - gpa.height;
    // This difference plus the border/padding size added to the target size
    // gives us the final size of the window content. AFAICT
    // gtk_window_set_default_size sets the size available in the window for
    // its content; we can ignore the window chrome
    gtk_window_set_default_size(GTK_WINDOW(self), diff_w + target_width,
            diff_h + target_height);
}
