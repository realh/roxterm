/*
    roxterm - GTK+ 2.0 terminal emulator with tabs
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

#if ! GTK_CHECK_VERSION(3, 0, 0)
static void
multitab_close_button_style_set (GtkWidget *self, GtkStyle *previous_style)
{
    int w, h;

    gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (self),
                       GTK_ICON_SIZE_MENU, &w, &h);
    gtk_widget_set_size_request (self, w + 2, h + 2);
    GTK_WIDGET_CLASS (multitab_close_button_parent_class)->style_set (self,
            previous_style);
}

static void
multitab_close_button_class_init(MultitabCloseButtonClass *klass)
{
    GTK_WIDGET_CLASS (klass)->style_set = multitab_close_button_style_set;
}
#endif

static void
multitab_close_button_init(MultitabCloseButton *self)
{
    GtkButton *parent = GTK_BUTTON(self);
    GtkWidget *image = gtk_image_new ();
    
    gtk_button_set_relief(parent, GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click(parent, FALSE);
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
}
