/*
    roxterm - GTK+ 2.0 terminal emulator with tabs
    Copyright (C) 2004-2011 Tony Houghton <h@realh.co.uk>

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

#include <string.h>

#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "x11support.h"

inline static GdkDisplay *get_display(GdkWindow *window)
{
#ifdef HAVE_GDK_WINDOW_GET_DISPLAY
    return gdk_window_get_display(window);
#else
    return gdk_drawable_get_display(GDK_DRAWABLE(window));
#endif
}

inline static GdkScreen *get_screen(GdkWindow *window)
{
#ifdef HAVE_GDK_WINDOW_GET_SCREEN
    return gdk_window_get_screen(window);
#else
    return gdk_drawable_get_screen(GDK_DRAWABLE(window));
#endif
}

/* This started off ripped from gnome-terminal */

gboolean x11support_window_is_minimized(GdkWindow *window)
{
    GdkDisplay *display = get_display(window);
    Atom type;
    gint format;
    gulong nitems;
    gulong bytes_after;
    guchar *data;
    Atom *atoms = NULL;
    gulong i;
    gboolean minimized = FALSE;
    gint ignored;
    
    type = None;
    gdk_error_trap_push();
    XGetWindowProperty(GDK_DISPLAY_XDISPLAY(display), GDK_WINDOW_XID(window),
                        gdk_x11_get_xatom_by_name_for_display(display,
                                "_NET_WM_STATE"),
                        0, G_MAXLONG, False, XA_ATOM, &type, &format, &nitems,
                        &bytes_after, &data);
    ignored = gdk_error_trap_pop();
    (void) ignored;
    
    if (type != None)
    {
        Atom hidden_atom = gdk_x11_get_xatom_by_name_for_display(display,
                "_NET_WM_STATE_HIDDEN");
    
        atoms = (Atom *)data;
        for (i = 0; i < nitems; i++)
        {
            if (atoms[i] == hidden_atom)
            {
                minimized = TRUE;
                break;
            }
            ++i;
        }
        XFree (atoms);
    }
    return minimized;
}

gboolean x11support_get_wm_desktop(GdkWindow *window, guint32 *desktop)
{
    GdkDisplay *display = get_display(window);
    Atom type;
    int format;
    guchar *data;
    gulong n_items, bytes_after;
    gboolean result = FALSE;
    
    if (XGetWindowProperty(GDK_DISPLAY_XDISPLAY(display),
            GDK_WINDOW_XID (window),
            gdk_x11_get_xatom_by_name_for_display(display, "_NET_WM_DESKTOP"),
            0, G_MAXLONG, False, AnyPropertyType,
            &type, &format, &n_items, &bytes_after, &data) == Success &&
        type != None)
    {
        if (type == XA_CARDINAL && format == 32 && n_items == 1)
        {
            *desktop = *(gulong *) data;
            result = TRUE;
        }
        XFree(data);
    }
    return result;
}

void x11support_set_wm_desktop(GdkWindow *window, guint32 desktop)
{
    GdkScreen *screen = get_screen (window);
    GdkDisplay *display = gdk_screen_get_display (screen);
    Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);
    char *wm_selection_name;
    Atom wm_selection;
    gboolean have_wm;
    
    wm_selection_name = g_strdup_printf("WM_S%d",
            gdk_screen_get_number(screen));
    wm_selection = gdk_x11_get_xatom_by_name_for_display(display,
            wm_selection_name);
    g_free(wm_selection_name);
    XGrabServer (xdisplay);
    have_wm = XGetSelectionOwner (xdisplay, wm_selection) != None;
    
    if (have_wm)
    {
        XClientMessageEvent xclient;
    
        memset(&xclient, 0, sizeof (xclient));
        xclient.type = ClientMessage;
        xclient.serial = 0;
        xclient.send_event = True;
        xclient.window = GDK_WINDOW_XID (window);
        xclient.message_type = gdk_x11_get_xatom_by_name_for_display(display,
                "_NET_WM_DESKTOP");
        xclient.format = 32;
        xclient.data.l[0] = desktop;
        xclient.data.l[1] = 0;
        xclient.data.l[2] = 0;
        xclient.data.l[3] = 0;
        xclient.data.l[4] = 0;
        XSendEvent (xdisplay,
                GDK_WINDOW_XID(gdk_screen_get_root_window(screen)),
                False,
                SubstructureRedirectMask | SubstructureNotifyMask,
                (XEvent *) &xclient);
    }
    else
    {
        gulong long_desktop = desktop;
    
        XChangeProperty (xdisplay,
                 GDK_WINDOW_XID(window),
                 gdk_x11_get_xatom_by_name_for_display(display,
                         "_NET_WM_DESKTOP"),
                 XA_CARDINAL, 32, PropModeReplace,
                 (guchar *) &long_desktop, 1);
    }
    
    XUngrabServer (xdisplay);
    XFlush (xdisplay);
}

void x11support_clear_demands_attention(GdkWindow *window)
{
    GdkScreen *screen = get_screen(window);
    GdkDisplay *display = gdk_screen_get_display(screen);
    XClientMessageEvent xclient;
    
    memset(&xclient, 0, sizeof (xclient));
    xclient.type = ClientMessage;
    xclient.serial = 0;
    xclient.send_event = True;
    xclient.window = GDK_WINDOW_XID (window);
    xclient.message_type = gdk_x11_get_xatom_by_name_for_display(display,
            "_NET_WM_STATE");
    xclient.format = 32;
    xclient.data.l[0] = 0; /* _NET_WM_STATE_REMOVE */
    xclient.data.l[1] = gdk_x11_get_xatom_by_name_for_display(display,
            "_NET_WM_STATE_DEMANDS_ATTENTION");
    xclient.data.l[2] = 0;
    xclient.data.l[3] = 0;
    xclient.data.l[4] = 0;
    XSendEvent (GDK_DISPLAY_XDISPLAY(display),
            GDK_WINDOW_XID(gdk_screen_get_root_window (screen)),
            False,
            SubstructureRedirectMask | SubstructureNotifyMask,
            (XEvent *) &xclient);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
