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

#include "multitext-tab-label.h"

typedef struct 
{
    GtkEventBox *ebox;
    GtkLabel *label;
    MultitextTabButton *button;
    gboolean single;
} MultitextTabLabelPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(MultitextTabLabel, multitext_tab_label,
        GTK_TYPE_BOX);

enum {
    PROP_TEXT = 1,
    PROP_BUTTON,
    PROP_SINGLE,

    N_PROPS
};

static GParamSpec *multitext_tab_label_props[N_PROPS] = { NULL };

static void
multitext_tab_label_set_property (GObject *object, guint prop,
        const GValue *value, GParamSpec *pspec)
{
    MultitextTabLabel *self = MULTITEXT_TAB_LABEL (object);
    MultitextTabLabelPrivate *priv
        = multitext_tab_label_get_instance_private(self);

    switch (prop)
    {
        case PROP_TEXT:
            multitext_tab_label_set_text(self, g_value_get_string (value));
            break;
        case PROP_BUTTON:
            priv->button = g_value_get_object(value);
            break;
        case PROP_SINGLE:
            multitext_tab_label_set_single(self, g_value_get_boolean(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop, pspec);
            break;
    }
}

static void
multitext_tab_label_get_property(GObject *object, guint prop,
        GValue *value, GParamSpec *pspec)
{
    MultitextTabLabel *self = MULTITEXT_TAB_LABEL (object);
    MultitextTabLabelPrivate *priv
        = multitext_tab_label_get_instance_private(self);

    switch (prop)
    {
        case PROP_TEXT:
            g_value_set_string(value, multitext_tab_label_get_text(self));
            break;
        case PROP_BUTTON:
            g_value_set_object(value, priv->button);
            break;
        case PROP_SINGLE:
            g_value_set_boolean(value, priv->single);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop, pspec);
            break;
    }
}

#define MULTITEXT_TAB_LABEL_FIXED_WIDTH 300

static void
multitext_tab_label_modify_width(MultitextTabLabel *self,
        gint *minimum_width, gint *natural_width)
{
    MultitextTabLabelPrivate *priv
        = multitext_tab_label_get_instance_private(self);
    if (priv->single)
    {
        if (*minimum_width < MULTITEXT_TAB_LABEL_FIXED_WIDTH)
            *minimum_width = MULTITEXT_TAB_LABEL_FIXED_WIDTH;
        *natural_width = MAX(*minimum_width, MULTITEXT_TAB_LABEL_FIXED_WIDTH);
    }
}

static void
multitext_tab_label_get_preferred_width (GtkWidget *widget,
        gint *minimum_width, gint *natural_width)
{
    MultitextTabLabel *self = MULTITEXT_TAB_LABEL (widget);
    GTK_WIDGET_CLASS(multitext_tab_label_parent_class)->get_preferred_width
                    (widget, minimum_width, natural_width);
    multitext_tab_label_modify_width(self, minimum_width, natural_width);
}

static void
multitext_tab_label_get_preferred_width_for_height (GtkWidget *widget,
        gint height, gint *minimum_width, gint *natural_width)
{
    MultitextTabLabel *self = MULTITEXT_TAB_LABEL (widget);
    GTK_WIDGET_CLASS
            (multitext_tab_label_parent_class)->get_preferred_width_for_height
                    (widget, height, minimum_width, natural_width);
    multitext_tab_label_modify_width(self, minimum_width, natural_width);
}

#if __APPLE__
#define MULTITEXT_PACK gtk_box_pack_end
#else
#define MULTITEXT_PACK gtk_box_pack_start
#endif

#define MULTITEXT_TAB_LABEL_PADDING 4

static void multitext_tab_label_constructed(GObject *obj)
{
    MultitextTabLabel *self = MULTITEXT_TAB_LABEL(obj);
    GtkBox *box = GTK_BOX(obj);
    MultitextTabLabelPrivate *priv
        = multitext_tab_label_get_instance_private(self);
    G_OBJECT_CLASS(multitext_tab_label_parent_class)->constructed(obj);
    gtk_container_add_with_properties(GTK_CONTAINER(priv->ebox),
            GTK_WIDGET(priv->label), "hexpand", TRUE, NULL);
    MULTITEXT_PACK(box, GTK_WIDGET(priv->ebox),
            TRUE, TRUE, MULTITEXT_TAB_LABEL_PADDING);
    if (priv->button)
    {
        MULTITEXT_PACK(box, GTK_WIDGET(priv->button),
                FALSE, FALSE, MULTITEXT_TAB_LABEL_PADDING);
    }
}

static void
multitext_tab_label_class_init(MultitextTabLabelClass *klass)
{
    GObjectClass *oklass = G_OBJECT_CLASS (klass);

    oklass->constructed = multitext_tab_label_constructed;
    oklass->set_property = multitext_tab_label_set_property;
    oklass->get_property = multitext_tab_label_get_property;
    multitext_tab_label_props[PROP_TEXT] =
            g_param_spec_string("text", "text",
            "Label text", "Roxterm tab",
            G_PARAM_READWRITE);
    multitext_tab_label_props[PROP_BUTTON] =
            g_param_spec_object("button", "button",
            "Close/status button", MULTITEXT_TYPE_TAB_BUTTON,
            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
    multitext_tab_label_props[PROP_SINGLE] =
            g_param_spec_boolean("single", "single",
            "Only tab in its window", TRUE,
            G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
    g_object_class_install_properties(oklass, N_PROPS,
            multitext_tab_label_props);

    GtkWidgetClass *wklass = GTK_WIDGET_CLASS (klass);
    wklass->get_preferred_width = multitext_tab_label_get_preferred_width;
    wklass->get_preferred_width_for_height =
            multitext_tab_label_get_preferred_width_for_height;
}

static void
multitext_tab_label_init(MultitextTabLabel *self)
{
    MultitextTabLabelPrivate *priv
        = multitext_tab_label_get_instance_private(self);
    priv->ebox = GTK_EVENT_BOX(gtk_event_box_new());
    priv->label = GTK_LABEL(gtk_label_new(""));
}

MultitextTabLabel *
multitext_tab_label_new(MultitextTabButton *button, gboolean single)
{
    GObject *obj = g_object_new(MULTITEXT_TYPE_TAB_LABEL,
            "button", button,
            "single", single,
            NULL);
    return MULTITEXT_TAB_LABEL(obj);
}

void
multitext_tab_label_set_text(MultitextTabLabel *self, const char *text)
{
    MultitextTabLabelPrivate *priv
        = multitext_tab_label_get_instance_private(self);
    gtk_label_set_text(priv->label, text);
}

const char *
multitext_tab_label_get_text (MultitextTabLabel *self)
{
    MultitextTabLabelPrivate *priv
        = multitext_tab_label_get_instance_private(self);
    return gtk_label_get_text(priv->label);
}

void
multitext_tab_label_set_single (MultitextTabLabel *self, gboolean single)
{
    MultitextTabLabelPrivate *priv
        = multitext_tab_label_get_instance_private(self);
    priv->single = single;
    gtk_widget_queue_resize(GTK_WIDGET(self));
}

MultitextTabButton *multitext_tab_label_get_button(MultitextTabLabel *self)
{
    MultitextTabLabelPrivate *priv
        = multitext_tab_label_get_instance_private(self);
    return priv->button;
}
