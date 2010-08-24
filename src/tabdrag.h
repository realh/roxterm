#ifndef TABDRAG_H
#define TABDRAG_H
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


/* Handles dragging of tabs */

#ifndef DEFNS_H
#include "defns.h"
#endif

typedef struct TabDrag TabDrag;

/* widget is the tab label widget the drag starts from */
TabDrag *tab_drag_new(GtkWidget *widget, struct MultiTab *tab);

/* Releases any handlers etc used by the TabDrag, but doesn't free the data */
void tab_drag_release(TabDrag *);

/* As above but also frees the TabDrag struct */
void tab_drag_delete(TabDrag *);

/* Call when you get a button press that might turn into an event;
 * button and event_x/_y are from the GdkEventButton event */
void tab_drag_press(TabDrag *, guint button, int event_x, int event_y);

#endif /* TABDRAG_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
