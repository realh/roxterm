#ifndef DLG_H
#define DLG_H
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

/* Miscellaneous dialog boxes */

#ifndef DEFNS_H
#include "defns.h"
#endif

#define DLG_SPACING 4

/* Show messages in dialog boxes and log them using g_log */
void dlg_message(GtkWindow *parent, const char *, ...);
void dlg_warning(GtkWindow *parent, const char *, ...);
void dlg_critical(GtkWindow *parent, const char *, ...);

GtkWidget *dlg_ok_cancel(GtkWindow *parent, const char *title,
        const char *, ...);

#endif /* DLG_H */

/* vi:set sw=4 ts=4 et cindent cino= */
