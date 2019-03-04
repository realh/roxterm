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
} MultitextWindowPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(MultitextWindow, multitext_window,
        GTK_TYPE_APPLICATION_WINDOW);

static void multitext_window_add(GtkContainer *self, GtkWidget *child)
{
    GTK_CONTAINER_CLASS(multitext_window_parent_class)->add(self, child);
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(MULTITEXT_WINDOW(self));
    g_return_if_fail(priv != NULL);
    if (MULTITEXT_IS_GEOMETRY_PROVIDER(child))
    {
        if (priv->gp)
        {
            g_warning("MultitextWindow already contains a"
                    " MultitextGeometryProvider");
        }
        priv->gp = MULTITEXT_GEOMETRY_PROVIDER(child);
    }
}

static void multitext_window_remove(GtkContainer *self, GtkWidget *child)
{
    GTK_CONTAINER_CLASS(multitext_window_parent_class)->add(self, child);
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(MULTITEXT_WINDOW(self));
    g_return_if_fail(priv != NULL);
    if (child == GTK_WIDGET(priv->gp))
        priv->gp = NULL;
}

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
    GtkContainerClass *cklass = GTK_CONTAINER_CLASS(klass);
    cklass->add = multitext_window_add;
    cklass->remove = multitext_window_remove;
}

static void multitext_window_init(MultitextWindow *self)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    g_return_if_fail(priv != NULL);
    priv->gp = NULL;
}

MultitextGeometryProvider *
multitext_window_get_geometry_provider(MultitextWindow *self)
{
    MultitextWindowPrivate *priv
        = multitext_window_get_instance_private(self);
    g_return_val_if_fail(priv != NULL, NULL);
    return priv->gp;
}
