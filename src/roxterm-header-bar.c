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

#include "config.h"
#include "roxterm-header-bar.h"

struct _RoxtermHeaderBar {
    GtkHeaderBar parent_instance;
    GtkWidget *burger;
};

G_DEFINE_TYPE(RoxtermHeaderBar, roxterm_header_bar, GTK_TYPE_HEADER_BAR);

static void roxterm_header_bar_class_init(UNUSED RoxtermHeaderBarClass *klass)
{
}

static void roxterm_header_bar_init(RoxtermHeaderBar *self)
{
    GtkHeaderBar *ghdr = GTK_HEADER_BAR(self);
    gtk_header_bar_set_show_close_button(ghdr, TRUE);
    gtk_header_bar_set_has_subtitle(ghdr, FALSE);
    self->burger = gtk_menu_button_new();
    gtk_menu_button_set_direction(GTK_MENU_BUTTON(self->burger),
            GTK_ARROW_NONE);
    gtk_header_bar_pack_end(ghdr, self->burger);
}

RoxtermHeaderBar *roxterm_header_bar_new(void)
{
    GObject *obj = g_object_new(ROXTERM_TYPE_HEADER_BAR,
            NULL);
    RoxtermHeaderBar *self = ROXTERM_HEADER_BAR(obj);
    return self;
}

