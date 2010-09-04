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

#include "defns.h"

#include <gdk/gdk.h>

#include "multitab.h"
#include "tabdrag.h"

struct TabDrag {
    GtkWidget *widget;
    MultiTab *tab;
    int start_x, start_y;
    guint button;
    gulong motion_handler, release_handler,
           grab_notify_handler, grab_broken_handler;
    GtkWidget *toplevel;
    gboolean grabbed;
};

static gboolean tab_drag_start(TabDrag *td, guint32 time);

static gboolean tab_drag_start_release_handler(GtkWidget *widget,
        GdkEventButton *event, TabDrag *td)
{
    if (event->button == td->button)
    {
        tab_drag_release(td);
    }
    return FALSE;
}

static gboolean tab_drag_end_release_handler(GtkWidget *widget,
        GdkEventButton *event, TabDrag *td)
{
    if (event->button == td->button)
    {
        MultiWin *orig_parent = multi_tab_get_parent(td->tab);
        
        tab_drag_release(td);
        if (!multi_win_get_win_under_pointer())
        {
            /* Dropped on backdrop or foreign window, make new window, or
             * simply move old window if it only contained one tab.
             */
            if (multi_win_get_ntabs(orig_parent) == 1)
            {
                gtk_window_move(GTK_WINDOW(multi_win_get_widget(orig_parent)),
                        MAX((int) event->x_root - 20, 0),
                        MAX((int) event->y_root - 8, 0));
            }
            else
            {
                char *display_name = gdk_screen_make_display_name(
                        gdk_drawable_get_screen(GDK_DRAWABLE(event->window)));
                MultiWin *win = multi_win_new_for_tab(display_name,
                        (int) event->x_root, (int) event->y_root, td->tab);
                        
                g_free(display_name);
                multi_tab_move_to_new_window(win, td->tab, -1); 
                multi_win_show(win);
                multi_win_select_tab(win, td->tab);
            }
        }
        else
        {
            multi_win_restore_focus(orig_parent);
        }
    }
    return FALSE;
}

static void tab_drag_grab_notify_handler(GtkWidget *widget,
        gboolean was_grabbed, TabDrag *td)
{
    tab_drag_release(td);
}

static gboolean tab_drag_grab_broken_handler(GtkWidget *widget,
        GdkEvent *event, TabDrag *td)
{
    tab_drag_release(td);
    return FALSE;
}

static gboolean tab_drag_motion_handler(GtkWidget *widget,
        GdkEventMotion *event, TabDrag *td)
{
    MultiTab *tab;
    MultiWin *old_win, *new_win;
    int page_num;
    gboolean current;

    tab = multi_tab_get_tab_under_pointer(event->x_root, event->y_root);
    if (!tab || tab == td->tab)
        return FALSE;
    page_num = multi_tab_get_page_num(tab);
    old_win = multi_tab_get_parent(td->tab);
    new_win = multi_tab_get_parent(tab);
    current = (multi_win_get_current_tab(old_win) == td->tab);
    if (new_win == old_win)
    {
        multi_tab_move_to_position(td->tab, page_num, TRUE);
    }
    else
    {
        multi_tab_move_to_new_window(new_win, td->tab, page_num); 
        if (current)
            multi_win_select_tab(new_win, td->tab);
    }

    return FALSE;
}

static gboolean tab_drag_start(TabDrag *td, guint32 time)
{
    static GdkCursor *cursor = NULL;

    td->toplevel = gtk_widget_get_toplevel(td->widget);
    gtk_grab_add(td->toplevel);
    
    if (!cursor) cursor = gdk_cursor_new(GDK_FLEUR);

    if (gdk_pointer_grab(td->toplevel->window, FALSE,
                GDK_BUTTON1_MOTION_MASK | GDK_BUTTON3_MOTION_MASK
                    | GDK_BUTTON_RELEASE_MASK,
                NULL, cursor, time) != GDK_GRAB_SUCCESS)
    {
        tab_drag_release(td);
        return FALSE;
    }
    td->grabbed = TRUE;
    td->release_handler = g_signal_connect(td->toplevel, "button-release-event",
            G_CALLBACK(tab_drag_end_release_handler), td);
    td->grab_notify_handler = g_signal_connect(td->toplevel, "grab-notify",
            G_CALLBACK(tab_drag_grab_notify_handler), td);
    if (gtk_check_version(2, 8, 0))
    {
        td->grab_broken_handler = g_signal_connect(td->toplevel,
                "grab-broken-event", G_CALLBACK(tab_drag_grab_broken_handler),
                td);
    }
    td->motion_handler = g_signal_connect(td->toplevel, "motion-notify-event",
            G_CALLBACK(tab_drag_motion_handler), td);
    return TRUE;
}

static gboolean tab_drag_start_motion_handler(GtkWidget *widget,
        GdkEventMotion *event, TabDrag *td)
{
    gboolean result = FALSE;

    if (gtk_drag_check_threshold(widget, td->start_x, td->start_y,
                event->x, event->y))
    {
        tab_drag_release(td);
        result = tab_drag_start(td, event->time);
    }
    return result;
}

TabDrag *tab_drag_new(GtkWidget *widget, MultiTab *tab)
{
    TabDrag *td = g_new0(TabDrag, 1);

    td->widget = widget;
    td->tab = tab;
    return td;
}

void tab_drag_release(TabDrag *td)
{
    GtkWidget *widget;

    g_return_if_fail(td);
    if (td->grabbed && gdk_pointer_is_grabbed())
        gdk_pointer_ungrab(GDK_CURRENT_TIME);
    td->grabbed = FALSE;
    if (td->toplevel)
    {
        widget = td->toplevel;
        gtk_grab_remove(widget);
        td->toplevel = NULL;
    }
    else
    {
        widget = td->widget;
    }
    if (td->motion_handler)
    {
        g_signal_handler_disconnect(widget, td->motion_handler);
        td->motion_handler = 0;
    }
    if (td->release_handler)
    {
        g_signal_handler_disconnect(widget, td->release_handler);
        td->release_handler = 0;
    }
    if (td->grab_broken_handler)
    {
        g_signal_handler_disconnect(widget, td->grab_broken_handler);
        td->grab_broken_handler = 0;
    }
    if (td->grab_notify_handler)
    {
        g_signal_handler_disconnect(widget, td->grab_notify_handler);
        td->grab_notify_handler = 0;
    }
}

void tab_drag_delete(TabDrag *td)
{
    if (!td)
        return;
    tab_drag_release(td);
    g_free(td);
}

void tab_drag_press(TabDrag *td, guint button, int event_x, int event_y)
{
    td->button = button;
    td->start_x = event_x;
    td->start_y = event_y;
    td->motion_handler = g_signal_connect(td->widget, "motion-notify-event",
            G_CALLBACK(tab_drag_start_motion_handler), td);
    td->release_handler = g_signal_connect(td->widget, "button-release-event",
            G_CALLBACK(tab_drag_start_release_handler), td);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
