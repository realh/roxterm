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
#ifndef __ROXTERM_WINDOW_H
#define __ROXTERM_WINDOW_H

#include <glib-object.h>
#include <gtk/gtk.h>

#define ROXTERM_TYPE_WINDOW roxterm_window_get_type()
G_DECLARE_FINAL_TYPE(RoxtermWindow, roxterm_window,
        ROXTERM, WINDOW, GtkApplicationWindow);

struct _RoxtermApplication;

RoxtermWindow *roxterm_window_new(struct _RoxtermApplication *app);

#endif /* __ROXTERM_WINDOW_H */