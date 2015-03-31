/*
    roxterm - VTE/GTK terminal emulator with tabs
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

#include <ctype.h>
#include <string.h>

static char *get_screen_dot(const char *display_name)
{
    char *n, *dot;

    dot = strrchr(display_name, '.');
    if (dot && *(dot + 1))
    {
        for (n = dot + 1; *n; ++n)
        {
            if (!isdigit(*n))
            {
                dot = NULL;
                break;
            }
        }
    }
    else
    {
        dot = NULL;
    }
    return dot;
}

static GdkScreen *get_matching_screen(const char *display_name,
        GdkDisplay *dpy, gboolean name_includes_screen)
{
    GdkScreen *screen = NULL;
    char *scrn_name = NULL;

    if (!name_includes_screen)
    {
        char *dot;

        screen = gdk_display_get_default_screen(dpy);
        scrn_name = gdk_screen_make_display_name(screen);
        dot = get_screen_dot(scrn_name);
        if (dot)
            *dot = 0;
        if (strcmp(display_name, scrn_name))
            screen = NULL;
    }
    else
    {
        screen = gdk_display_get_screen(dpy, 0);
        scrn_name = gdk_screen_make_display_name(screen);
        if (strcmp(display_name, scrn_name))
            screen = NULL;
    }
    g_free(scrn_name);
    return screen;
}

GdkScreen *display_get_screen_for_name(const char *display_name)
{
    static GdkDisplayManager *dpy_mgr = NULL;
    GSList *node, *dpy_list;
    gboolean name_inc_scrn;
    GdkDisplay *dpy = NULL;
    GdkScreen *screen = NULL;

    if (!display_name)
        return gdk_screen_get_default();
    name_inc_scrn = get_screen_dot(display_name) != NULL;;
    if (!dpy_mgr)
        dpy_mgr = gdk_display_manager_get();
    dpy_list = gdk_display_manager_list_displays(dpy_mgr);
    for (node = dpy_list; node; node = g_slist_next(node))
    {
        dpy = node->data;
        screen = get_matching_screen(display_name, dpy, name_inc_scrn);
        if (screen)
            break;
    }
    g_slist_free(dpy_list);
    if (screen)
        return screen;
    dpy = gdk_display_open(display_name);
    if (!dpy)
        return NULL;
    screen = get_matching_screen(display_name, dpy, name_inc_scrn);
    if (!screen)
        g_object_unref(dpy);
    return screen;
}
