/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2011 Tony Houghton <h@realh.co.uk>

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
    const MultitabColor *color;
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
multitab_label_toggle_attention (gpointer data)
{
    MultitabLabel *self = MULTITAB_LABEL (data);
    
    gtk_event_box_set_visible_window (GTK_EVENT_BOX(self),
            self->attention = !self->attention);
    return TRUE;
}

static void
multitab_label_single_width (GtkWidget *widget, gint *width,
        gint *parent_width)
{
    GtkWidget *parent;
    GtkAllocation alloc;

    for (parent = gtk_widget_get_parent (widget); parent;
            parent = gtk_widget_get_parent (parent))
    {
        if (GTK_IS_NOTEBOOK (parent))
            break;
    }
    g_return_if_fail (parent != NULL);
    gtk_widget_get_allocation (parent, &alloc);
    if (parent_width)
        *parent_width = alloc.width;
    /* About 40% of width of parent should look good */
    alloc.width = (alloc.width * 2) / 5;
    if (alloc.width > *width)
        *width = alloc.width;
}

#if GTK_CHECK_VERSION(3, 0, 0)

static void
multitab_label_destroy (GtkWidget *w)
{
    multitab_label_cancel_attention (MULTITAB_LABEL (w));
    GTK_WIDGET_CLASS (multitab_label_parent_class)->destroy (w);
}

static void
multitab_modify_width (MultitabLabel *self,
        gint *minimum_width, gint *natural_width)
{
    if (self->fixed_width)
        return;
    /*
    if (self->single && natural_width)
    {
        multitab_label_single_width ((GtkWidget *) self, natural_width);
        if (minimum_width && *minimum_width > *natural_width)
            *minimum_width = *natural_width;
    }
    */
    if (self->single && minimum_width)
    {
        int parent_width;
        
        multitab_label_single_width ((GtkWidget *) self, minimum_width,
                &parent_width);
#if MULTITAB_LABEL_GTK3_SIZE_KLUDGE
        if (self->best_width)
        {
            if (*minimum_width < *self->best_width &&
                    *minimum_width < parent_width)
            {
                *self->best_width = *minimum_width;
            }
            else if (*minimum_width > *self->best_width)
            {
                *minimum_width = *self->best_width;
            }
        }
#endif
        if (natural_width && *natural_width < *minimum_width)
            *natural_width = *minimum_width;
    }
}

static void
multitab_label_get_preferred_width (GtkWidget *widget,
        gint *minimum_width, gint *natural_width)
{
    MultitabLabel *self = MULTITAB_LABEL (widget);
    
    GTK_WIDGET_CLASS (multitab_label_parent_class)->get_preferred_width
                    (widget, minimum_width, natural_width);
    multitab_modify_width (self, minimum_width, natural_width);
}

static void
multitab_label_get_preferred_width_for_height (GtkWidget *widget,
        gint height, gint *minimum_width, gint *natural_width)
{
    MultitabLabel *self = MULTITAB_LABEL (widget);
    
    GTK_WIDGET_CLASS
            (multitab_label_parent_class)->get_preferred_width_for_height
                    (widget, height, minimum_width, natural_width);
    multitab_modify_width (self, minimum_width, natural_width);
}

#else   /* !GTK3 */

static void
multitab_label_destroy (GtkObject *o)
{
    multitab_label_cancel_attention (MULTITAB_LABEL (o));
    GTK_OBJECT_CLASS (multitab_label_parent_class)->destroy (o);
}

static void
multitab_label_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
    MultitabLabel *self = MULTITAB_LABEL (widget);
    
    GTK_WIDGET_CLASS (multitab_label_parent_class)->size_request
                    (widget, requisition);
    if (self->fixed_width)
        return;
    if (self->single && requisition)
    {
        multitab_label_single_width (widget, &requisition->width, NULL);
    }
}

#endif /* GTK2/3 */

static void
multitab_label_class_init(MultitabLabelClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);
    GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
    GParamSpec *pspec;
    
    /* Make sure theme doesn't override our colour with a gradient or image */
#if GTK_CHECK_VERSION(3, 0, 0)
    static const char *style =
            "* {\n"
              "background-image : none;\n"
            "}";
    
    klass->style_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (klass->style_provider,
            style, -1, NULL);
    GTK_WIDGET_CLASS (klass)->destroy = multitab_label_destroy;
#else
    gtk_rc_parse_string ("style \"multitab-label-style\"\n"
           "{\n"
              "bg_pixmap[NORMAL] = \"<none>\"\n"
              "bg_pixmap[ACTIVE] = \"<none>\"\n"
           "}\n"
           "widget \"*.multitab-label\" "
           "style \"multitab-label-style\"");
    GTK_OBJECT_CLASS (klass)->destroy = multitab_label_destroy;
#endif
    
    oclass->set_property = multitab_label_set_property;
    oclass->get_property = multitab_label_get_property;
    
    pspec = g_param_spec_pointer ("attention-color",
            "Attention Color", "Color to flash to draw attention",
            G_PARAM_READWRITE);
    g_object_class_install_property (oclass, PROP_ATTENTION_COLOR, pspec);
    
#if GTK_CHECK_VERSION(3, 0, 0)
    wclass->get_preferred_width = multitab_label_get_preferred_width;
    wclass->get_preferred_width_for_height =
            multitab_label_get_preferred_width_for_height;
#else
    wclass->size_request = multitab_label_size_request;
#endif
}

static void
multitab_label_init(MultitabLabel *self)
{
    GtkWidget *label = gtk_label_new ("");
    GtkWidget *w = GTK_WIDGET(self);
    static MultitabColor amber;
    static gboolean parsed_amber = FALSE;
    
    self->single = FALSE;
#if MULTITAB_LABEL_GTK3_SIZE_KLUDGE
    self->best_width = NULL;
#endif
#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_style_context_add_provider (gtk_widget_get_style_context (w),
            GTK_STYLE_PROVIDER (MULTITAB_LABEL_GET_CLASS
                    (self)->style_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#endif
    gtk_widget_set_name (w, "multitab-label");
    if (!parsed_amber)
    {
#if GTK_CHECK_VERSION(3, 0, 0)
        gdk_rgba_parse (&amber, "#ffc450");
#else
        gdk_color_parse ("#ffc450", &amber);
#endif
        parsed_amber = TRUE;
    }
    gtk_event_box_set_visible_window (GTK_EVENT_BOX (self), FALSE);
    multitab_label_set_attention_color (self, &amber);
    self->label = GTK_LABEL (label);
    self->fixed_width = FALSE;
    gtk_label_set_ellipsize (self->label, PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_show (label);
    gtk_container_add (GTK_CONTAINER (self), label);
}

GtkWidget *
multitab_label_new (const char *text
#if MULTITAB_LABEL_GTK3_SIZE_KLUDGE
    , int *best_width
#endif
)
{
    MultitabLabel *self = (MultitabLabel *)
            g_object_new (MULTITAB_TYPE_LABEL, NULL);
    
    if (text)
        multitab_label_set_text (self, text);
#if MULTITAB_LABEL_GTK3_SIZE_KLUDGE
    self->best_width = best_width;
#endif
    return GTK_WIDGET (self);
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
        self->attention = FALSE;
    }
}

void
multitab_label_set_attention_color (MultitabLabel *self,
        const MultitabColor *color)
{
    GtkWidget *w = GTK_WIDGET (self);
    
    self->attention_color = *color;
#if GTK_CHECK_VERSION(3, 0, 0)
    gtk_widget_override_background_color(w, GTK_STATE_FLAG_NORMAL, color);
    gtk_widget_override_background_color(w, GTK_STATE_FLAG_ACTIVE, color);
#else
    gtk_widget_modify_bg(w, GTK_STATE_NORMAL, color);
    gtk_widget_modify_bg(w, GTK_STATE_ACTIVE, color);
#endif
}

const MultitabColor *
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
        gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

void
multitab_label_set_fixed_width (MultitabLabel *self, int width)
{
    if (width == -1)
    {
        self->fixed_width = FALSE;
    }
    else
    {
        self->fixed_width = TRUE;
        gtk_label_set_width_chars (self->label, width);
    }
}
