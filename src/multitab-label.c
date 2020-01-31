/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2015 Tony Houghton <h@realh.co.uk>

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

#include "multitab-label.h"

G_DEFINE_TYPE (MultitabLabel, multitab_label, GTK_TYPE_EVENT_BOX);

#define PROP_TEXT 1
#define PROP_ATTENTION_COLOR 2

static void
multitab_label_set_property (GObject *object, guint prop,
        const GValue *value, GParamSpec *pspec)
{
    const GdkRGBA *color;
    MultitabLabel *self = MULTITAB_LABEL (object);

    switch (prop)
    {
        case PROP_TEXT:
            multitab_label_set_text (self, g_value_get_string (value));
            break;
        case PROP_ATTENTION_COLOR:
            color = g_value_get_pointer (value);
            multitab_label_set_attention_color (self, color);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop, pspec);
            break;
    }
}

static void
multitab_label_get_property (GObject *object, guint prop,
        GValue *value, GParamSpec *pspec)
{
    MultitabLabel *self = MULTITAB_LABEL (object);

    switch (prop)
    {
        case PROP_TEXT:
            g_value_set_string (value, multitab_label_get_text (self));
            break;
        case PROP_ATTENTION_COLOR:
            g_value_set_pointer (value, &self->attention_color);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop, pspec);
            break;
    }
}

static gboolean
multitab_label_draw (GtkWidget *widget, cairo_t *cr)
{
    MultitabLabel *self = MULTITAB_LABEL (widget);
    if (self->attention)
    {
        cairo_set_source_rgb(cr, self->attention_color.red,
                self->attention_color.green,
                self->attention_color.blue);
        cairo_paint(cr);
    }
    return GTK_WIDGET_CLASS (multitab_label_parent_class)->draw (widget, cr);
}

static gboolean
multitab_label_toggle_attention (gpointer data)
{
    MultitabLabel *self = MULTITAB_LABEL (data);

    self->attention = !self->attention;
    gtk_widget_queue_draw (GTK_WIDGET (data));
    return TRUE;
}

static void
multitab_label_destroy (GtkWidget *w)
{
    multitab_label_cancel_attention (MULTITAB_LABEL (w));
    GTK_WIDGET_CLASS (multitab_label_parent_class)->destroy (w);
}


#define MULTITAB_LABEL_FIXED_WIDTH 200

static void
multitab_label_modify_width(MultitabLabel *self,
        gint *minimum_width, gint *natural_width)
{
    if (self->single)
    {
        if (*minimum_width < MULTITAB_LABEL_FIXED_WIDTH)
            *minimum_width = MULTITAB_LABEL_FIXED_WIDTH;
        *natural_width = MAX(*natural_width, MULTITAB_LABEL_FIXED_WIDTH);
    }
}


static void
multitab_label_get_preferred_width (GtkWidget *widget,
        gint *minimum_width, gint *natural_width)
{
    MultitabLabel *self = MULTITAB_LABEL (widget);

    GTK_WIDGET_CLASS (multitab_label_parent_class)->get_preferred_width
                    (widget, minimum_width, natural_width);
    multitab_label_modify_width (self, minimum_width, natural_width);
}

static void
multitab_label_get_preferred_width_for_height (GtkWidget *widget,
        gint height, gint *minimum_width, gint *natural_width)
{
    MultitabLabel *self = MULTITAB_LABEL (widget);

    GTK_WIDGET_CLASS
            (multitab_label_parent_class)->get_preferred_width_for_height
                    (widget, height, minimum_width, natural_width);
    multitab_label_modify_width (self, minimum_width, natural_width);
}

static void
multitab_label_class_init(MultitabLabelClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
    GParamSpec *pspec;

    /* Make sure theme doesn't override our colour with a gradient or image */
    static const char *style =
            "MultitabLabel {\n"
              "background-image : none;\n"
            "}";

    klass->style_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (klass->style_provider,
            style, -1, NULL);

    wclass->destroy = multitab_label_destroy;
    wclass->draw = multitab_label_draw;
    wclass->get_preferred_width = multitab_label_get_preferred_width;
    wclass->get_preferred_width_for_height =
            multitab_label_get_preferred_width_for_height;
    gtk_widget_class_set_css_name (wclass, "multitab-label");

    oclass->set_property = multitab_label_set_property;
    oclass->get_property = multitab_label_get_property;

    pspec = g_param_spec_string ("text",
            "Text", "Text displayed in the label",
            "",
            G_PARAM_READWRITE);
    g_object_class_install_property (oclass, PROP_TEXT, pspec);

    pspec = g_param_spec_pointer ("attention-color",
            "Attention Color", "Color to flash to draw attention",
            G_PARAM_READWRITE);
    g_object_class_install_property (oclass, PROP_ATTENTION_COLOR, pspec);

}

static void
multitab_label_init(MultitabLabel *self)
{
    GtkWidget *label = gtk_label_new ("");
    GtkWidget *w = GTK_WIDGET(self);
    static GdkRGBA amber;
    static gboolean parsed_amber = FALSE;

    self->single = FALSE;
    self->parent = NULL;
#if MULTITAB_LABEL_GTK3_SIZE_KLUDGE
    self->best_width = NULL;
#endif
    gtk_style_context_add_provider (gtk_widget_get_style_context (w),
            GTK_STYLE_PROVIDER (MULTITAB_LABEL_GET_CLASS
                    (self)->style_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    if (!parsed_amber)
    {
        gdk_rgba_parse(&amber, "#ffc450");
        parsed_amber = TRUE;
    }
    gtk_event_box_set_visible_window (GTK_EVENT_BOX (self), FALSE);
    multitab_label_set_attention_color (self, &amber);
    self->label = GTK_LABEL (label);
    gtk_label_set_ellipsize (self->label, PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_show (label);
    gtk_container_add (GTK_CONTAINER (self), label);
}

GtkWidget *
multitab_label_new (GtkWidget *parent, const char *text, int *best_width)
{
    MultitabLabel *self = (MultitabLabel *)
            g_object_new (MULTITAB_TYPE_LABEL, NULL);

    self->parent = parent;
    if (text)
        multitab_label_set_text (self, text);
    self->best_width = best_width;
    return GTK_WIDGET (self);
}

void
multitab_label_set_parent (MultitabLabel *self,
        GtkWidget *parent, int *best_width)
{
    self->parent = parent;
    self->best_width = best_width;
}

void
multitab_label_set_text (MultitabLabel *self, const char *text)
{
    gtk_label_set_text (self->label, text);
}

const char *
multitab_label_get_text (MultitabLabel *self)
{
    return gtk_label_get_text (self->label);
}

void
multitab_label_draw_attention (MultitabLabel *self)
{
    multitab_label_cancel_attention (self);
    multitab_label_toggle_attention (self);
    self->timeout_tag = g_timeout_add (500,
            multitab_label_toggle_attention, self);
}

void
multitab_label_cancel_attention (MultitabLabel *self)
{
    if (self->timeout_tag)
    {
        g_source_remove (self->timeout_tag);
        self->timeout_tag = 0;
    }
    if (self->attention)
    {
        multitab_label_toggle_attention (self);
    }
}

void
multitab_label_set_attention_color (MultitabLabel *self,
        const GdkRGBA *color)
{
    self->attention_color = *color;
}

const GdkRGBA *
multitab_label_get_attention_color (MultitabLabel *self)
{
    return &self->attention_color;
}

void
multitab_label_set_single (MultitabLabel *self, gboolean single)
{

    if (single != self->single)
    {
        self->single = single;
#if GTK_CHECK_VERSION(3, 0, 0)
        if (self->label)
            g_object_set(self->label, "hexpand", !single, NULL);
#endif
        gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

void
multitab_label_set_current(MultitabLabel *label, gboolean current)
{
    gtk_widget_set_sensitive(GTK_WIDGET(label->label), current);
}
