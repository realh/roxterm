#ifndef CAPPLET_H
#define CAPPLET_H
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


/* Main part of capplet program and tools for other parts */

#ifndef DEFNS_H
#include "defns.h"
#endif

#include "options.h"

#include <glade/glade-xml.h>

const char *capplet_get_glade_filename(void);

/* This is set when one of the widgets is being changed by the program, so we
 * should ignore the resultant signal */
extern gboolean capplet_ignore_changes;

void capplet_save_file(Options * options);

/* Set a value, save file and send DBus message */
void capplet_set_int(Options * options, const char *name, int value);

void capplet_set_string(Options * options, const char *name, const char *value);

void capplet_set_float(Options * options, const char *name, double value);

/* Returns -1 if there's an error */
int capplet_which_radio_is_selected(GtkWidget *widget);

void capplet_set_radio_by_index(GladeXML *glade,
		const char *basename, int index);

/**********************************************/
/* Generic handlers */

void on_editable_changed(GtkEditable * editable, char *pflag);

void on_boolean_toggled(GtkToggleButton * button, Options * options);

void on_radio_toggled(GtkToggleButton * button, Options * options);

void on_spin_button_changed(GtkSpinButton * button, Options * options);

void on_float_spin_button_changed(GtkSpinButton * button, Options * options);

void on_float_range_changed(GtkRange * range, Options * options);

void on_combo_changed(GtkComboBox * combo, Options * options);

/*void on_file_chosen(GtkFileChooser * chooser, Options * options);*/

/**********************************************/

/* Convenience functions to lookup an option and set the widget with the same
 * name */
void capplet_set_boolean_toggle(GladeXML * glade, Options * options,
	const char *name, gboolean dflt);

void capplet_set_text_entry(GladeXML * glade, Options * options,
	const char *name, const char *dflt);

void capplet_set_radio(GladeXML * glade, Options * options,
	const char *name, int dflt);

void capplet_set_spin_button(GladeXML * glade, Options * options,
	const char *name, int dflt);

void capplet_set_spin_button_float(GladeXML * glade, Options * options,
	const char *name);

void capplet_set_float_range(GladeXML * glade, Options * options,
	const char *name, double dflt);

void capplet_set_combo(GladeXML * glade, Options * options,
	const char *name, int dflt);

/*void capplet_set_filename(GladeXML * glade, Options * options,
	const char *name);*/

/* Keep track of number of open windows and quit when it reaches 0 */
void capplet_inc_windows(void);
void capplet_dec_windows(void);

#endif /* CAPPLET_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
