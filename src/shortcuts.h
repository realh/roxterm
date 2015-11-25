#ifndef SHORTCUTS_H
#define SHORTCUTS_H
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

#include "defns.h"

#ifndef ROXTERM_CAPPLET
#include "options.h"

/* Call once to connect signal to save shortcuts when changed by user; or you
 * can just leave it because it will be called by shortcuts_open */
void shortcuts_init(void);

/* Loads named keyboard shortcuts scheme, adding accelerators to the global
 * GtkAccelMap. If file not found or NULL, tries to load Default. If reload
 * is TRUE it forces the shortcuts to be reloaded from the file even if a
 * scheme of the same name is already loaded.
 */
Options *shortcuts_open(const char *scheme_name, gboolean reload);

void shortcuts_unref(Options *scheme);

#if GTK_CHECK_VERSION(3, 10, 0)
inline static void shortcuts_enable_signal_handler(gboolean enable)
{
    (void) enable;
}
#else
void shortcuts_enable_signal_handler(gboolean enable);
#endif

gboolean shortcuts_key_is_shortcut(Options *shortcuts,
        guint key, GdkModifierType modifiers);

const char *shortcuts_get_index_str(Options *shortcuts);

#endif /* ROXTERM_CAPPLET */

/* window = parent window for dialogs in case of error etc */
void shortcuts_edit(GtkWindow *window, const char *name);

#endif /* SHORTCUTS_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
