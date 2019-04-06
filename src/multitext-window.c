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

#include <vte/vte.h>

#define MULTITEXT_WINDOW_STATE_SNAPPED (GDK_WINDOW_STATE_MAXIMIZED | \
        GDK_WINDOW_STATE_FULLSCREEN | WIN_STATE_TILED)

typedef struct {
    MultitextGeometryProvider *gp;
    gulong anchored_sig_tag;
    gulong destroy_sig_tag;
    MultitextNotebook *notebook;
    GdkGeometry geom;
    gboolean have_geom;
    gboolean geom_pending;
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

static void multitext_window_get_target_size(MultitextWindow *self,
        int *window_width, int *window_height, gboolean initial)
{
    GtkWidget *tlw = GTK_WIDGET(self);
    if (multitext_window_is_snapped(tlw))
        return;
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    GdkGeometry *geom = &priv->geom;
    GtkWidget *tlc = gtk_bin_get_child(GTK_BIN(self));  // top level child
    // Before size negotiation the geometry provider's natural preferred size
    // corresponds to the total size the widget should be to show the
    // configured number of columns and rows.
    GtkWidget *gp_w = GTK_WIDGET(priv->gp);
    int gp_nat_w, gp_nat_h;
    multitext_geometry_provider_set_alloc_for_measurement(priv->gp, TRUE);
    gtk_widget_get_preferred_width(gp_w, &geom->min_width, &gp_nat_w);
    gtk_widget_get_preferred_height(gp_w, &geom->min_height, &gp_nat_h);
    // At this point we can also determine how much padding there is in the
    // gp widget
    int cols, rows;
    multitext_geometry_provider_get_current_size(priv->gp, &cols, &rows);
    multitext_geometry_provider_get_cell_size(priv->gp,
            &geom->width_inc, &geom->height_inc);
    geom->base_width = gp_nat_w - geom->width_inc * cols;
    geom->base_height = gp_nat_h - geom->height_inc * rows;
    // However, if there hasn't yet been a size allocation we can't reliably
    // measure how much padding etc is added by the notebook and any other
    // widgets in the hierarchy, so do a sort of test allocation. Probably
    // best to do it on the window's immediate child rather than the window
    // itself in case we break gtk_window_set_default_size
    GtkAllocation tlc_alloc;
    if (initial)
    {
        // Might as well make some attempt to get the right size in the first
        // place, but we can't trust that this will result in gp being
        // allocated a size that will result in it snapping to correct base_*
        // dimensions
        int min_tlc_w, min_tlc_h;
        gtk_widget_get_preferred_width(tlc, &min_tlc_w, &tlc_alloc.width);
        gtk_widget_get_preferred_height(tlc, &min_tlc_h, &tlc_alloc.height);
        tlc_alloc.x = tlc_alloc.y = 0;
        //tlc_alloc.width = gp_nat_w + MAX(min_tlc_w - geom->min_width, 0);
        //tlc_alloc.height = gp_nat_h + MAX(min_tlc_h - geom->min_height, 0);
        gtk_widget_size_allocate(tlc, &tlc_alloc);
    }
    multitext_geometry_provider_set_alloc_for_measurement(priv->gp, FALSE);
    // Now we can read how much padding there should be between tlc and gp and
    // add it to gp's internal padding. That plus its target size gives us the
    // target window size, which does not include its "chrome"
    GtkAllocation gp_alloc;
    gtk_widget_get_allocation(gp_w, &gp_alloc);
    // Get tlc's actual allocation in case initial was FALSE or initial
    // allocation request was unable to be satisifed
    gtk_widget_get_allocation(tlc, &tlc_alloc);
    int target_w, target_h;
    multitext_geometry_provider_get_target_size(priv->gp, &target_w, &target_h);
    int pad_w = tlc_alloc.width - gp_alloc.width;
    int pad_h = tlc_alloc.height - gp_alloc.height;
    geom->base_width += pad_w;
    geom->base_height += pad_h;
    geom->min_width += pad_w;
    geom->min_height += pad_h;
    if (window_width)
        *window_width = target_w * geom->width_inc + geom->base_width;
    if (window_height)
        *window_height = target_h * geom->height_inc + geom->base_height;
}

static void multitext_window_update_geometry(MultitextWindow *self);

// Always returns FALSE so it can be used directly as an idle callback
static gboolean multitext_window_apply_geometry(MultitextWindow *self)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    if (!priv->have_geom)
    {
        multitext_window_get_target_size(self, NULL, NULL, FALSE);
        multitext_window_update_geometry(self);
    }
    else
    {
        GtkWindow *win = GTK_WINDOW(self);
        gtk_window_set_geometry_hints(win, NULL, &priv->geom,
                GDK_HINT_RESIZE_INC | GDK_HINT_BASE_SIZE | GDK_HINT_MIN_SIZE);
    }
    return FALSE;
}

// After getting hints with multitext_window_get_target_size and then
// allocating a size to the winodw, this adds the window chrome to the hints
// before applying them
static void multitext_window_update_geometry(MultitextWindow *self)
{
    GtkWindow *win = GTK_WINDOW(self);
    if (multitext_window_is_snapped(win))
        return;
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    GtkWidget *tlw = GTK_WIDGET(win);
    GtkWidget *tlc = gtk_bin_get_child(GTK_BIN(tlw));  // top level child
    // geom comes from multitext_window_get_target_size so we need to add
    // top_level's chrome to it
    GtkAllocation win_alloc, tlc_alloc;
    gtk_widget_get_allocation(tlw, &win_alloc);
    gtk_widget_get_allocation(tlc, &tlc_alloc);
    int pad_w = win_alloc.width - tlc_alloc.width;
    int pad_h = win_alloc.height - tlc_alloc.height;
    GdkGeometry *geom = &priv->geom;
    geom->base_width += pad_w;
    geom->base_height += pad_h;
    geom->min_width += pad_w;
    geom->min_height += pad_h;
    priv->have_geom = TRUE;
    multitext_window_apply_geometry(self);
}

static void multitext_window_size_allocate(GtkWidget *widget,
        GtkAllocation *alloc)
{
    CHAIN_UP(GTK_WIDGET_CLASS, multitext_window_parent_class, size_allocate)
        (widget, alloc);
    MultitextWindow *self = MULTITEXT_WINDOW(widget);
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    if (priv->geom_pending)
    {
        multitext_window_update_geometry(self);
        priv->geom_pending = FALSE;
    }
}

static gboolean multitext_window_state_event(GtkWidget *widget,
        GdkEventWindowState *event)
{
    // Disable geometry hints early, restore them late
    MultitextWindow *self = MULTITEXT_WINDOW(widget);
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    gboolean changed = multitext_window_state_is_snapped(event->changed_mask);
    gboolean snapped =
        multitext_window_state_is_snapped(event->new_window_state);
    if (changed && snapped)
    {
        gtk_window_set_geometry_hints(GTK_WINDOW(widget), NULL, NULL, 0);
    }
    gboolean result = CHAIN_UP_BOOL(GTK_WIDGET_CLASS,
            multitext_window_parent_class, window_state_event)(widget, event);
    if (changed && !snapped)
    {
        //priv->have_geom = FALSE;
        //g_idle_add((GSourceFunc) multitext_window_apply_geometry, self);
        multitext_window_apply_geometry(self);
    }
    return result;
}

static void multitext_window_child_geometry_changed(
        UNUSED MultitextGeometryProvider *gp, MultitextWindow *self)
{
    int width, height;
    multitext_window_get_target_size(self, &width, &height, FALSE);
    gtk_window_resize(GTK_WINDOW(self), width, height);
    multitext_window_update_geometry(self);
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
    wklass->size_allocate = multitext_window_size_allocate;
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
    GtkWidget *tlc = gtk_bin_get_child(GTK_BIN(self));
    // Realize the top-level and show everything except the top-level first,
    // otherwise measured sizes tend to be nonsense
    gtk_widget_realize(GTK_WIDGET(self));
    gtk_widget_show_all(tlc);
    int width, height;
    multitext_window_get_target_size(self, &width, &height, TRUE);
    gtk_window_set_default_size(GTK_WINDOW(self), width, height);
    // Can't update geometry yet, wait until we get a size allocation
    priv->geom_pending = TRUE;
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

