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

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "about.h"
#include "globalopts.h"
#include "version.h"

#define LOGO_SIZE 64

static GdkPixbuf *about_load_logo(void)
{
    static GdkPixbuf *result = NULL;
    static gboolean tried = FALSE;

    if (!tried)
    {
        GError *error = NULL;
        char *filename;

        if (global_options_appdir)
        {
            filename = g_build_filename(global_options_appdir,
                    ".DirIcon", NULL);
        }
        else
        {
            filename = g_build_filename(ICON_DIR, "roxterm.svg", NULL);
        }
        
        result = gdk_pixbuf_new_from_file_at_size(filename,
                LOGO_SIZE, LOGO_SIZE, &error);
        if (error)
        {
            g_warning(_("Unable to load logo for about dialog: %s"),
                    error->message);
            g_error_free(error);
        }
        tried = TRUE;
        g_free(filename);
    }
    return result;
}

static GtkWidget *about_dialog_create(void)
{
    GtkWidget *about = gtk_about_dialog_new();
    GtkAboutDialog *ad = GTK_ABOUT_DIALOG(about);
    char const *authors[] = { _("Tony Houghton <h@realh.co.uk>"),
        _("Thanks to the developers of VTE"),
        NULL };
    GdkPixbuf *logo = about_load_logo();

    gtk_about_dialog_set_program_name(ad, "ROXTerm");
    gtk_about_dialog_set_version(ad, VERSION);
    gtk_about_dialog_set_copyright(ad, _("(c) 2005-2012 Tony Houghton"));
    gtk_about_dialog_set_website(ad, "http://roxterm.sourceforge.net");
    gtk_about_dialog_set_authors(ad, authors);
    if (logo)
        gtk_about_dialog_set_logo(ad, logo);
    return about;
}

void about_dialog_show(GtkWindow *parent,
#if USE_ACTIVATE_LINK
        gboolean (*uri_handler)(GtkAboutDialog *, char *, gpointer),
#else
        GtkAboutDialogActivateLinkFunc www_hook,
        GtkAboutDialogActivateLinkFunc email_hook,
#endif
        gpointer hook_data
)
{
    GtkWidget *ad;

#if !USE_ACTIVATE_LINK
    gtk_about_dialog_set_url_hook(www_hook, hook_data, NULL);
    gtk_about_dialog_set_email_hook(email_hook, hook_data, NULL);
#endif
    ad = about_dialog_create();
    if (parent)
        gtk_window_set_transient_for(GTK_WINDOW(ad), parent);
#if USE_ACTIVATE_LINK
    g_signal_connect(ad, "activate-link", G_CALLBACK(uri_handler), hook_data);
#endif
    gtk_dialog_run(GTK_DIALOG(ad));
    gtk_widget_destroy(ad);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
