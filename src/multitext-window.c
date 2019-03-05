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
    priv->gp = NULL;
    priv->anchored_sig_tag = 0;
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
        multitext_window_disconnect_provider_signals(self);
    }
    if (gp) 
    {
        priv->anchored_sig_tag = g_signal_connect(gp, "hierarchy-changed",
                G_CALLBACK(multitext_window_geometry_provider_anchored), self);
        priv->destroy_sig_tag = g_signal_connect(gp, "destroy",
                G_CALLBACK(multitext_window_geometry_provider_destroyed), self);
    }
    priv->gp = gp;
}
