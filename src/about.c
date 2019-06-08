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


#include "defns.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "about.h"
#include "globalopts.h"
#include "resources.h"
#include "version.h"

static GtkWidget *about_dialog_create(void)
{
    GtkWidget *about = gtk_about_dialog_new();
    GtkAboutDialog *ad = GTK_ABOUT_DIALOG(about);
    char const *authors[] = { _("Tony Houghton <h@realh.co.uk>"),
        _("Thanks to the developers of VTE"),
        NULL };

    gtk_about_dialog_set_program_name(ad, "ROXTerm");
    gtk_about_dialog_set_version(ad, VERSION);
    gtk_about_dialog_set_copyright(ad, _("(c) 2005-2018 Tony Houghton"));
    gtk_about_dialog_set_website(ad, "https://realh.github.io/roxterm");
    gtk_about_dialog_set_authors(ad, authors);
    resources_access_icon();
    gtk_about_dialog_set_logo_icon_name(ad, "roxterm");
    return about;
}

void about_dialog_show(GtkWindow *parent,
        gboolean (*uri_handler)(GtkAboutDialog *, char *, gpointer),
        gpointer hook_data
)
{
    GtkWidget *ad;

    ad = about_dialog_create();
    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(ad), parent);
    g_signal_connect(ad, "activate-link", G_CALLBACK(uri_handler), hook_data);
    gtk_dialog_run(GTK_DIALOG(ad));
    gtk_widget_destroy(ad);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
