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
#ifndef __ROXTERM_HEADER_BAR_H
#define __ROXTERM_HEADER_BAR_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define ROXTERM_TYPE_HEADER_BAR roxterm_header_bar_get_type()
G_DECLARE_FINAL_TYPE(RoxtermHeaderBar, roxterm_header_bar,
        ROXTERM, HEADER_BAR, GtkHeaderBar);

RoxtermHeaderBar *roxterm_header_bar_new(void);

G_END_DECLS

#endif /* __ROXTERM_HEADER_BAR_H */
