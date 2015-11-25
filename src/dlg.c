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

#include "dlg.h"

static void run_msg_dialog(GtkWindow *parent, const char *title,
        GtkMessageType mtype, const char *msg)
{
    GtkWidget *dialog;
    char *full_title = g_strdup_printf(_("%s from ROXTerm"), title);

    dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT,
            mtype, GTK_BUTTONS_OK, "%s", msg);
    gtk_window_set_title(GTK_WINDOW(dialog), full_title);
    g_free(full_title);
    gtk_dialog_run(GTK_DIALOG (dialog));
    gtk_widget_destroy(dialog);
}

#define ROXTERM_DEFINE_REPORT_FN(fn, title, log_level, diag_type) \
void fn(GtkWindow *parent, const char *s, ...) \
{ \
    va_list ap; \
    char *msg; \
    va_start(ap, s); \
    msg = g_strdup_vprintf(s, ap); \
    g_log(G_LOG_DOMAIN, log_level, "%s", msg); \
    run_msg_dialog(parent, title, diag_type, msg); \
    va_end(ap); \
    g_free(msg); \
}

ROXTERM_DEFINE_REPORT_FN(dlg_message, _("Message"), G_LOG_LEVEL_MESSAGE,
        GTK_MESSAGE_INFO)

ROXTERM_DEFINE_REPORT_FN(dlg_warning, _("Warning"), G_LOG_LEVEL_WARNING,
        GTK_MESSAGE_WARNING)

ROXTERM_DEFINE_REPORT_FN(dlg_critical, _("Error"), G_LOG_LEVEL_CRITICAL,
        GTK_MESSAGE_ERROR)

GtkWidget *dlg_ok_cancel(GtkWindow *parent, const char *title,
        const char *format, ...)
{
    va_list ap;
    char *msg;
    GtkWidget *w;
    
    va_start(ap, format);
    msg = g_strdup_vprintf(format, ap);
    va_end(ap);
    w = gtk_message_dialog_new(parent,
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
            "%s", msg);
    g_free(msg);
    gtk_window_set_title(GTK_WINDOW(w), title);
    return w;
}

/* vi:set sw=4 ts=4 et cindent cino= */
