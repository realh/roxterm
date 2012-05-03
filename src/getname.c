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
#include <stdlib.h>
#include <string.h>

#include "boxcompat.h"
#include "getname.h"

static gboolean name_clashes(const char *new_name, char const **existing)
{
	while (*existing)
	{
		if (!strcmp(new_name, *existing))
		{
			return TRUE;
		}
		++existing;
	}
	return FALSE;
}

static char *suggest_name(const char *orig, char const **existing)
{
	/* Does orig end in a number? */
	size_t len = strlen(orig);
	size_t numoff = len;
	int num = 0;
	char *suggest;

	while (isdigit(orig[numoff - 1]))
		--numoff;
	if (numoff != len)
		num = atoi(orig + numoff);
	for (;;)
	{
		++num;
		suggest = g_strdup_printf("%.*s%02d", (int) numoff, orig, num);
		if (!name_clashes(suggest, existing))
			break;
		g_free(suggest);
	}
	return suggest;
}

char *getname_run_dialog(GtkWindow *parent, const char *old_name,
		char const **existing, const char *title, const char *button_label,
		GtkWidget *icon, gboolean suggest_change)
{
	GtkWidget *dialog;
	GtkWidget *name_field;
	char *new_name = NULL;
	
	dialog = gtk_dialog_new_with_buttons(title, parent,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			button_label, GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	name_field = gtk_entry_new();
	box_compat_packv(gtk_dialog_get_content_area(GTK_DIALOG(dialog)),
	        name_field, FALSE, 16);
	gtk_entry_set_activates_default(GTK_ENTRY(name_field), TRUE);
	gtk_widget_show(name_field);

	if (old_name)
	{
		if (suggest_change)
		{
			char *suggest = suggest_name(old_name, existing);

			gtk_entry_set_text(GTK_ENTRY(name_field), suggest);
			g_free(suggest);
		}
		else
		{
			gtk_entry_set_text(GTK_ENTRY(name_field), old_name);
		}
	}

	while (!new_name)
	{
		int response;

		gtk_editable_select_region(GTK_EDITABLE(name_field), 0, -1);
		response = gtk_dialog_run(GTK_DIALOG(dialog));
		if (response == GTK_RESPONSE_OK)
		{
			const char *read_name = gtk_entry_get_text(GTK_ENTRY(name_field));

			if (name_clashes(read_name, existing))
			{
				GtkWidget *error_box = gtk_message_dialog_new(
						GTK_WINDOW(dialog),
						GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						_("An item with that name already exists"));
					
				gtk_dialog_run(GTK_DIALOG(error_box));
				gtk_widget_destroy(error_box);
				new_name = NULL;
			}
			else
			{
				new_name = g_strdup(read_name);
			}
		}
		else
		{
			break;
		}
	}

	gtk_widget_destroy(dialog);
	return new_name;
}

/* vi:set sw=4 ts=4 noet cindent cino= */
