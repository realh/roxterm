#ifndef CONFIGLET_H
#define CONFIGLET_H
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


/* "Configlet" is the main window where you can add, edit & delete profiles,
 * colour schemes and shortcut schemes */

#ifndef DEFNS_H
#include "defns.h"
#endif

typedef struct _ConfigletList ConfigletList;
typedef struct _ConfigletData ConfigletData;

gboolean configlet_open();

/* Lock when editing a profile etc and unlock when editing is complete. This is
 * done with reference counting */
void configlet_lock_profiles(void);
void configlet_unlock_profiles(void);
void configlet_lock_colour_schemes(void);
void configlet_unlock_colour_schemes(void);
void configlet_lock_shortcuts(void);
void configlet_unlock_shortcuts(void);

/* These handlers aren't referred to elsewhere in the code but are used by
 * GtkBuilder. */
void on_Configlet_destroy(GtkWidget *widget, ConfigletData *cg);
void on_Configlet_response(GtkWidget *widget, int response, ConfigletData *cg);
void on_Configlet_close(GtkWidget *widget, ConfigletData *cg);
void on_profile_edit_clicked(GtkButton *button, ConfigletData *cg);
void on_colours_edit_clicked(GtkButton *button, ConfigletData *cg);
void on_row_activated(GtkTreeView *tvwidget, GtkTreePath *path,
        GtkTreeViewColumn *column, ConfigletData *cg);
void on_profile_copy_clicked(GtkButton *button, ConfigletData *cg);
void on_colours_copy_clicked(GtkButton *button, ConfigletData *cg);
void on_shortcuts_copy_clicked(GtkButton *button, ConfigletData *cg);
void on_profile_delete_clicked(GtkButton *button, ConfigletData *cg);
void on_colours_delete_clicked(GtkButton *button, ConfigletData *cg);
void on_shortcuts_delete_clicked(GtkButton *button, ConfigletData *cg);
void on_profile_rename_clicked(GtkButton *button, ConfigletData *cg);
void on_colours_rename_clicked(GtkButton *button, ConfigletData *cg);
void on_shortcuts_rename_clicked(GtkButton *button, ConfigletData *cg);

#endif /* CONFIGLET_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
