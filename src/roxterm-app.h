/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2015-2020 Tony Houghton <h@realh.co.uk>

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
#ifndef ROXTERM_APP_H
#define ROXTERM_APP_H

#include "launch-params.h"
#include "multitab.h"

G_BEGIN_DECLS

#define ROXTERM_APP_ID "uk.co.realh.roxterm"

#define ROXTERM_TYPE_APP roxterm_app_get_type()
G_DECLARE_FINAL_TYPE(RoxtermApp, roxterm_app,
        ROXTERM, APP, GtkApplication);

RoxtermApp *roxterm_app_new(void);

MultiWin *roxterm_app_new_window(RoxtermApp *app,
        RoxtermLaunchParams *lp, RoxtermWindowLaunchParams *wp);

G_END_DECLS

#endif /* ROXTERM_APP_H */
