#ifndef PROFILEGUI_H
#define PROFILEGUI_H
/*
    roxterm - GTK+ 2.0 terminal emulator with tabs
    Copyright (C) 2004 Tony Houghton <h@realh.co.uk>

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


#ifndef DEFNS_H
#include "defns.h"
#endif

/* Editing of a Profile with a GUI (using GtkBuilder) */

typedef struct _ProfileGUI ProfileGUI;

/* Loads a Profile and creates a working dialog box for it.
 * Creates new profile if it doesn't already exist */
ProfileGUI *profilegui_open(const char *profile_name, GdkScreen *scrn);

void profilegui_delete(ProfileGUI *);

#endif /* PROFILEGUI_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
