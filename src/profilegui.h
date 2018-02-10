#ifndef PROFILEGUI_H
#define PROFILEGUI_H
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

/* Editing of a Profile with a GUI (using GtkBuilder) */

typedef struct _ProfileGUI ProfileGUI;

/* Loads a Profile and creates a working dialog box for it.
 * Creates new profile if it doesn't already exist */
ProfileGUI *profilegui_open(const char *profile_name);

void profilegui_delete(ProfileGUI *);

/* Handlers declared extern for GtkBuilder */
void on_ssh_entry_activate(GtkEntry * entry, ProfileGUI * pg);
void on_ssh_host_changed(GtkWidget * widget, ProfileGUI *pg);
void on_edit_ssh_clicked(GtkWidget * widget, ProfileGUI *pg);
void on_Profile_Editor_destroy(GtkWidget * widget, ProfileGUI * pg);
void on_Profile_Editor_response(GtkWidget *widget, int response,
        ProfileGUI *pg);
void on_Profile_Editor_close(GtkWidget * widget, ProfileGUI * pg);
void on_font_set(GtkFontButton * fontbutton, ProfileGUI * pg);
void on_entry_activate(GtkEntry * entry, ProfileGUI * pg);
void on_profile_notebook_switch_page(GtkNotebook * notebook, GtkWidget *page,
        guint page_num, ProfileGUI * pg);
void on_bgtype_toggled(GtkToggleButton *button, ProfileGUI *pg);
void on_command_toggled(GtkToggleButton *button, ProfileGUI *pg);
void on_close_buttons_toggled(GtkToggleButton *button, ProfileGUI *pg);
void on_cursor_blinks_toggled(GtkToggleButton *button, ProfileGUI *pg);
void on_bgimg_chosen(GtkFileChooser *chooser, ProfileGUI *pg);
void on_reset_compat_clicked(GtkButton *button, ProfileGUI *pg);
void on_cell_size_toggled(GtkToggleButton * button, ProfileGUI *pg);

#endif /* PROFILEGUI_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
