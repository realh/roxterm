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
#ifndef __ROXTERM_APPLICATION_H
#define __ROXTERM_APPLICATION_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "roxterm-window.h"

G_BEGIN_DECLS

#define ROXTERM_TYPE_APPLICATION roxterm_application_get_type()
G_DECLARE_FINAL_TYPE(RoxtermApplication, roxterm_application,
        ROXTERM, APPLICATION, GtkApplication);

RoxtermApplication *roxterm_application_new(void);

RoxtermWindow *roxterm_application_new_window(RoxtermApplication *app,
        RoxtermLaunchParams *lp, RoxtermWindowLaunchParams *wp);

/**
 * roxterm_application_get_builder:
 *
 * Returns: (transfer none) (nullable):
 */
GtkBuilder *roxterm_application_get_builder(RoxtermApplication *self);

G_END_DECLS

#endif /* __ROXTERM_APPLICATION_H */
