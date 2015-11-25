#ifndef X11SUPPORT_H
#define X11SUPPORT_H
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


#ifndef DEFNS_H
#include "defns.h"
#endif

/* Get whether a window is minimized.
 * GTK+ implementation can give bogus results.
 * See http://bugzilla.gnome.org/show_bug.cgi?id=586664
 */
gboolean x11support_window_is_minimized(GdkWindow *window);

/* This would probably work for any uint32 property,
 * but we only use it for desktops.
 */
gboolean x11support_get_desktop_prop(GdkWindow *window,
        const char *prop_name, guint32 *desktop);

/* Allow us to make sure a window stays on the same desktop when re-realizing */
inline static gboolean x11support_get_wm_desktop(GdkWindow *window,
        guint32 *desktop)
{
    return x11support_get_desktop_prop(window, "_NET_WM_DESKTOP", desktop);
}

/* _NET_WM_DESKTOP doesn't always seem to be present on root */
inline static gboolean x11support_get_current_desktop(GdkWindow *window,
        guint32 *desktop)
{
    return x11support_get_desktop_prop(window, "_NET_CURRENT_DESKTOP", desktop);
}

void x11support_set_wm_desktop(GdkWindow *window, guint32 desktop);

void x11support_clear_demands_attention(GdkWindow *window);

#endif /* X11SUPPORT_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
