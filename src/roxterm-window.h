/*
    roxterm4 - VTE/GTK terminal emulator with tabs
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

#include "multitext-window.h"
#include "roxterm-launch-params.h"
#include "roxterm-vte.h"

G_BEGIN_DECLS

#define ROXTERM_TYPE_WINDOW roxterm_window_get_type()
G_DECLARE_FINAL_TYPE(RoxtermWindow, roxterm_window,
        ROXTERM, WINDOW, MultitextWindow);

/**
 * roxterm_window_new:
 *
 * Creates a new window with an empty notebook
 *
 * @builder: (transfer none): Provides menus, a ref is temporarily added until
 *          window is fully constructed
 * Returns: (transfer full):
 */
RoxtermWindow *roxterm_window_new(GtkBuilder *builder);

/**
 * roxterm_window_apply_launch_params:
 *
 * @lp: (transfer none):
 * @wp: (transfer none):
 */
void roxterm_window_apply_launch_params(RoxtermWindow *self,
        RoxtermLaunchParams *lp, RoxtermWindowLaunchParams *wp);

/**
 * roxterm_window_new_tab:
 *
 * Creates a new tab in this window
 *
 * @tp: (transfer none):
 * @index: tab position or -1 to append
 *
 * Returns: (transfer full):
 */
RoxtermVte *roxterm_window_new_tab(RoxtermWindow *win,
        RoxtermTabLaunchParams *tp, int index);


G_END_DECLS

#endif /* __ROXTERM_WINDOW_H */
