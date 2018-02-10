#ifndef COLOURGUI_H
#define COLOURGUI_H
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

/* Editing of a Colour with a GUI */

typedef struct _ColourGUI ColourGUI;

/* Loads a Colour and creates a working dialog box for it.
 * Creates new colour scheme if it doesn't already exist */
ColourGUI *colourgui_open(const char *colour_scheme_name);

void colourgui_delete(ColourGUI *);

/* Handlers declared extern for GtkBuilder */
void on_Colour_Editor_destroy(GtkWidget *widget, ColourGUI * cg);
void on_Colour_Editor_response(GtkWidget *widget, gint response, ColourGUI *cg);
void on_Colour_Editor_close(GtkWidget *widget, ColourGUI * cg);
void on_color_set(GtkColorButton *button, ColourGUI * cg);
void on_set_fgbg_toggled(GtkToggleButton *button, ColourGUI * cg);
void on_fgbg_track_palette_toggled(GtkToggleButton *button, ColourGUI * cg);
void on_set_cursor_colour_toggled(GtkToggleButton *button, ColourGUI *cg);
void on_palette_size_toggled(GtkToggleButton *button, ColourGUI *cg);
void on_use_custom_colours_toggled(GtkToggleButton *button, ColourGUI *cg);

#endif /* COLOURGUI_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
