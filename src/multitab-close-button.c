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

#include "multitab-close-button.h"

G_DEFINE_TYPE (MultitabCloseButton, multitab_close_button, GTK_TYPE_BUTTON);

static void
multitab_close_button_set_size (MultitabCloseButton *self)
{
    int x, y;
    GtkWidget *w = GTK_WIDGET (self);

    gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (w),
                       GTK_ICON_SIZE_MENU, &x, &y);
    gtk_widget_set_size_request (w, x + 2, y + 2);
}

#if GTK_CHECK_VERSION(3, 0, 0)
static void
multitab_close_button_style_updated (GtkWidget *self)
{
    multitab_close_button_set_size (MULTITAB_CLOSE_BUTTON (self));
    GTK_WIDGET_CLASS (multitab_close_button_parent_class)->style_updated (self);
}
#else
static void
multitab_close_button_style_set (GtkWidget *self, GtkStyle *previous_style)
{
    multitab_close_button_set_size (MULTITAB_CLOSE_BUTTON (self));
    GTK_WIDGET_CLASS (multitab_close_button_parent_class)->style_set (self,
            previous_style);
}
#endif

static void
multitab_close_button_class_init(MultitabCloseButtonClass *klass)
{
#if GTK_CHECK_VERSION(3, 0, 0)
    static const char *button_style =
            "* {\n"
              "-GtkButton-default-border : 0;\n"
              "-GtkButton-default-outside-border : 0;\n"
              "-GtkButton-inner-border: 0;\n"
              "-GtkWidget-focus-line-width : 0;\n"
              "-GtkWidget-focus-padding : 0;\n"
              "padding: 0;\n"
            "}";
    
    klass->style_provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_data (klass->style_provider,
            button_style, -1, NULL);
    GTK_WIDGET_CLASS (klass)->style_updated =
            multitab_close_button_style_updated;
#else
    gtk_rc_parse_string ("style \"multitab-close-button-style\"\n"
           "{\n"
              "GtkWidget::focus-padding = 0\n"
              "GtkWidget::focus-line-width = 0\n"
              "xthickness = 0\n"
              "ythickness = 0\n"
           "}\n"
           "widget \"*.multitab-close-button\" "
           "style \"multitab-close-button-style\"");
    GTK_WIDGET_CLASS (klass)->style_set = multitab_close_button_style_set;
#endif
}

static void
multitab_close_button_init(MultitabCloseButton *self)
{
    GtkButton *parent = GTK_BUTTON(self);
    GtkWidget *image = gtk_image_new ();
    GtkWidget *w = GTK_WIDGET(self);
    
    gtk_button_set_relief(parent, GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click(parent, FALSE);
#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_style_context_add_provider (gtk_widget_get_style_context (w),
            GTK_STYLE_PROVIDER (MULTITAB_CLOSE_BUTTON_GET_CLASS
                    (self)->style_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#endif
    gtk_widget_set_name (w, "multitab-close-button");
    self->image = GTK_IMAGE (image);
    gtk_widget_show (image);
    gtk_container_add (GTK_CONTAINER (self), image);
}

GtkWidget *
multitab_close_button_new(const char *image_name)
{
    MultitabCloseButton *self = (MultitabCloseButton *)
            g_object_new (MULTITAB_TYPE_CLOSE_BUTTON, NULL);
    
    multitab_close_button_set_image (self, image_name);
    return GTK_WIDGET (self);
}

void
multitab_close_button_set_image(MultitabCloseButton *self,
        const char *image_name)
{
    gtk_image_set_from_stock (self->image,
            image_name ? image_name : GTK_STOCK_CLOSE,
            GTK_ICON_SIZE_MENU);
    multitab_close_button_set_size (self);
}
