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
#include "multitext-window.h"

#define MULTITEXT_WINDOW_STATE_SNAPPED (GDK_WINDOW_STATE_MAXIMIZED | \
        GDK_WINDOW_STATE_FULLSCREEN | WIN_STATE_TILED)

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

MultitextGeometryProvider *
multitext_window_find_geometry_provider(GtkWidget *widget)
{
    if (MULTITEXT_IS_GEOMETRY_PROVIDER(widget))
        return MULTITEXT_GEOMETRY_PROVIDER(widget);
    if (!GTK_IS_CONTAINER(widget))
        return NULL;
    GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
    for (GList *link = children; link; link = g_list_next(link))
    {
        MultitextGeometryProvider *gp =
            multitext_window_find_geometry_provider(link->data);
        if (gp)
            return gp;
    }
    return NULL;
}


static gboolean multitext_window_delete_event(GtkWidget *widget,
        GdkEventAny *event)
{
    if (CHAIN_UP_BOOL(GTK_WIDGET_CLASS, multitext_window_parent_class,
                delete_event)(widget, event))
    {
        return TRUE;
    }
    MultitextWindow *self = MULTITEXT_WINDOW(widget);
    GtkContainer *nb = GTK_CONTAINER(multitext_window_get_notebook(self));
    GList *pages = gtk_container_get_children(nb);
    for (GList *link = pages; link; link = g_list_next(link))
    {
        MultitextGeometryProvider *gp
            = multitext_window_find_geometry_provider(link->data);
        if (gp && multitext_geometry_provider_confirm_close(gp))
        {
            // TODO; Run confirmation dialog
            return TRUE;
        }
    }
    //gtk_widget_destroy(widget);
    //g_debug("Window should be closed, ref count %d",
    //      G_OBJECT(widget)->ref_count);
    return FALSE;
}

#if GTK_CHECK_VERSION(3,22,23)
#define MULTITEXT_WINDOW_STATE_TILED_MASK \
        (GDK_WINDOW_STATE_TOP_TILED | GDK_WINDOW_STATE_BOTTOM_TILED | \
        GDK_WINDOW_STATE_LEFT_TILED | GDK_WINDOW_STATE_RIGHT_TILED)
#else
#define MULTITEXT_WINDOW_STATE_TILED_MASK GDK_WINDOW_STATE_TILED
#endif
#define MULTITEXT_WINDOW_STATE_SNAPPED_MASK (GDK_WINDOW_STATE_MAXIMIZED | \
        GDK_WINDOW_STATE_FULLSCREEN | MULTITEXT_WINDOW_STATE_TILED_MASK)

inline static gboolean multitext_window_state_is_snapped(GdkWindowState state)
{
    return state & MULTITEXT_WINDOW_STATE_SNAPPED_MASK;
}

inline static gboolean multitext_window_is_snapped(gpointer window)
{
    return multitext_window_state_is_snapped(
            gdk_window_get_state(gtk_widget_get_window(window)));
}

static gboolean multitext_window_state_event(GtkWidget *widget,
        GdkEventWindowState *event)
{
    MultitextWindow *self = MULTITEXT_WINDOW(widget);
    if (multitext_window_state_is_snapped(event->changed_mask))
    {
        if (multitext_window_state_is_snapped(event->new_window_state))
        {
            gtk_window_set_geometry_hints(GTK_WINDOW(widget), NULL, NULL, 0);
        }
        else
        {
            multitext_window_update_geometry(self, NULL, NULL, FALSE);
        }
    }
    if (CHAIN_UP_BOOL(GTK_WIDGET_CLASS, multitext_window_parent_class,
                window_state_event)(widget, event))
    {
        return TRUE;
    }
    return FALSE;
}

static void multitext_window_child_geometry_changed(
        UNUSED MultitextGeometryProvider *gp, MultitextWindow *self)
{
    int width, height;
    multitext_window_update_geometry(self, &width, &height, FALSE);
    gtk_window_resize(GTK_WINDOW(self), width, height);
}

static void multitext_window_page_removed(UNUSED GtkNotebook *gnb,
        GtkWidget *child, UNUSED guint page_num, MultitextWindow *self)
{
    g_signal_handlers_disconnect_by_func(child,
            G_CALLBACK(multitext_window_child_geometry_changed), self);
}

static void multitext_window_constructed(GObject *obj)
{
    G_OBJECT_CLASS(multitext_window_parent_class)->constructed(obj);
    MultitextWindow *self = MULTITEXT_WINDOW(obj);
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    g_return_if_fail(priv != NULL);
    priv->notebook = multitext_notebook_new();
    gtk_container_add(GTK_CONTAINER(self), GTK_WIDGET(priv->notebook));
    g_signal_connect(priv->notebook, "page-removed",
            G_CALLBACK(multitext_window_page_removed), self);
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
    GtkWidgetClass *wklass = GTK_WIDGET_CLASS(klass);
    wklass->delete_event = multitext_window_delete_event;
    wklass->window_state_event = multitext_window_state_event;
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

static void multitext_window_apply_geometry_hints(GtkWindow *window,
        int base_width, int base_height, int width_inc, int height_inc)
{
    if (multitext_window_is_snapped(window))
        return;
    g_debug("Applying geometry hints %d+%dx, %d+%dy",
            base_width, width_inc,
            base_height, height_inc);
    GdkGeometry geom;
    geom.base_width = base_width;
    geom.base_height = base_height;
    geom.width_inc = width_inc;
    geom.height_inc = height_inc;
    geom.min_width = MIN(300, 10 * width_inc);
    geom.min_height = MIN(200, 5 * height_inc);
    gtk_window_set_geometry_hints(window, NULL, &geom,
            GDK_HINT_RESIZE_INC);
}

void multitext_window_update_geometry(MultitextWindow *self,
        int *window_width, int *window_height, gboolean initial)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    int columns, rows;
    int cell_width, cell_height;
    int target_width, target_height;
    int current_width, current_height;
    multitext_geometry_provider_get_target_size(priv->gp, &columns, &rows);
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
    GtkAllocation wa, tlca, gpa;
    GtkWidget *gw = GTK_WIDGET(self);
    if (initial)
    {
        gtk_widget_get_preferred_width(gw, &min_w, &nat_w);
        gtk_widget_get_preferred_height(gw, &min_h, &nat_h);
        // Now we know the minimum size for the window
        wa.width = min_w;
        wa.height = min_h;
        wa.x = wa.y = 0;
        multitext_geometry_provider_set_alloc_for_measurement(priv->gp, TRUE);
        gtk_widget_size_allocate(gw, &wa);
        multitext_geometry_provider_set_alloc_for_measurement(priv->gp, FALSE);
        // Allocating the min size to the window gives it and its children
        // valid allocations with correct relative sizes
    }
    GtkWidget *tlc = gtk_bin_get_child(GTK_BIN(self));  // top level child
    gtk_widget_get_allocation(gw, &wa);
    gtk_widget_get_allocation(tlc, &tlca);
    gtk_widget_get_allocation(gpw, &gpa);
    // Now we can read the differences in size between the window, the tlc
    // and the geometry widget
    diff_w += wa.width - gpa.width;
    diff_h += wa.height - gpa.height;
    GtkWindow *gwin = GTK_WINDOW(self);
    multitext_window_apply_geometry_hints(gwin, diff_w + wa.width - gpa.width,
            diff_h + wa.height - gpa.height, cell_width, cell_height);
    // The difference between the window's allocation and gp's allocation added
    // to gp's padding (diff_w/h) gives us the "base" dimensions for the
    // geometry hints, but the size we're going to pass to
    // gtk_window_set_default_size() or  gtk_window_resize() should exclude
    // window chrome
    if (window_width)
        *window_width = diff_w + target_width + tlca.width - gpa.width;
    if (window_height)
        *window_height = diff_h + target_height + tlca.height - gpa.height;
}

void multitext_window_set_initial_size(MultitextWindow *self)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    GtkWidget *nbw = GTK_WIDGET(priv->notebook);
    // Show everything except the top-level first, otherwise measured sizes
    // tend to be nonsense
    gtk_widget_realize(nbw);
    gtk_widget_show_all(nbw);
    int width, height;
    multitext_window_update_geometry(self, &width, &height, TRUE);
    gtk_window_set_default_size(GTK_WINDOW(self), width, height);
}

void multitext_window_insert_page(MultitextWindow *self,
        GtkWidget *child, MultitextGeometryProvider *gp,
        MultitextTabLabel *label, int index)
{
    MultitextNotebook *mnb = multitext_window_get_notebook(self);
    GtkNotebook *gnb = GTK_NOTEBOOK(mnb);
    GtkWidget *label_w = label ? GTK_WIDGET(label) : NULL;
    int n_pages = gtk_notebook_get_n_pages(gnb);
    if (label)
    {
        gtk_widget_show_all(label_w);
        multitext_geometry_provider_set_tab_label(gp, label);
    }
    gtk_notebook_insert_page(gnb, child, label_w, index);
    multitext_notebook_set_tab_label_homogeneous(mnb, child, n_pages > 0);
    if (n_pages == 1)
    {
        // Switch from single tab label size to homogeneous
        GtkWidget *old_page = gtk_notebook_get_nth_page(gnb, 0);
        GtkWidget *old_label = gtk_notebook_get_tab_label(gnb, old_page);
        multitext_tab_label_set_single(MULTITEXT_TAB_LABEL(old_label), FALSE);
        multitext_notebook_set_tab_label_homogeneous(mnb, old_page, TRUE);
    }
    gtk_widget_show_all(child);
    multitext_window_set_geometry_provider(MULTITEXT_WINDOW(self), gp);
    gtk_notebook_set_current_page(gnb, index == -1 ? n_pages : index);
    g_signal_connect(gp, "geometry-changed",
            G_CALLBACK(multitext_window_child_geometry_changed), self);
}

