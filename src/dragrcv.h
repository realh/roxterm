#ifndef DRAGRCV_H
#define DRAGRCV_H
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

/* Handles drag & drop of text or URI onto a widget */


#ifndef DEFNS_H
#include "defns.h"
#endif

enum
{
    ROXTERM_DRAG_TARGET_URI_LIST,
    ROXTERM_DRAG_TARGET_UTF8_STRING,
    ROXTERM_DRAG_TARGET_TEXT,
    ROXTERM_DRAG_TARGET_COMPOUND_TEXT,
    ROXTERM_DRAG_TARGET_STRING,
    ROXTERM_DRAG_TARGET_TEXT_PLAIN,
    ROXTERM_DRAG_TARGET_MOZ_URL,
    ROXTERM_DRAG_TARGET_TEXT_UNICODE,
    ROXTERM_DRAG_TARGET_TAB
    /*
    ROXTERM_DRAG_TARGET_COLOR,
    ROXTERM_DRAG_TARGET_BGIMAGE,
    ROXTERM_DRAG_TARGET_RESET_BG,
    */
};

typedef struct DragReceiveData DragReceiveData;

/* Function called when something's been dragged in and converted to text.
 * It should return FALSE if the text is invalid in some way. If we receive a
 * URI list, each URI is separated by \r\n, but any leading "file://"
 * is stripped. text is NULL-terminated.
 */
typedef gboolean (*DragReceiveHandler)(GtkWidget *, const char *text,
        gulong length, gpointer data);

/* Called when a tab is dropped on a target widget. widget is  the dragged
 * widget, data is as passed to drag_receive_setup_dest_widget. */
typedef gboolean (*DragReceiveTabHandler)(GtkWidget *widget, gpointer data);

/* Sets up widget as a destination for the types of drag we're interested in.
 * It returns a pointer to an opaque data structure which should be deleted
 * when the widget is destroyed. The tab handler may be NULL. */
DragReceiveData *drag_receive_setup_dest_widget(GtkWidget *widget,
        DragReceiveHandler, DragReceiveTabHandler, gpointer data);

inline static void drag_receive_data_delete(DragReceiveData *drd)
{
    g_free(drd);
}

#endif /* DRAGRCV_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
