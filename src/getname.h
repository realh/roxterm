#ifndef GETNAME_H
#define GETNAME_H
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


/* Opens a dialog box to get a new filename for copy or rename */

#ifndef DEFNS_H
#include "defns.h"
#endif

/* title is title to give dialog box; icon may be NULL. If old_name is non-NULL
 * it's pre-entered and selected, but if suggest_change is TRUE, the name is
 * altered by incremental numbering to avoid any existing names */
char *getname_run_dialog(GtkWindow *parent, const char *old_name,
		char const **existing, const char *title, const char *button_label,
		GtkWidget *icon, gboolean suggest_change);

#endif /* GETNAME_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
