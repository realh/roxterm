#ifndef SHORTCUTS_H
#define SHORTCUTS_H
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


#include "options.h"

/* Call once to connect signal to save shortcuts when changed by user; or you
 * can just leave it because it will be called by shortcuts_open */
void shortcuts_init(void);

/* Loads named keyboard shortcuts scheme, adding accelerators to the global
 * GtkAccelMap. If file not found or NULL, tries to load Default */
Options *shortcuts_open(const char *scheme_name);

void shortcuts_unref(Options *scheme);

void shortcuts_enable_signal_handler(gboolean enable);

#endif /* SHORTCUTS_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
