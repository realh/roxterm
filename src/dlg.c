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

#include "dlg.h"
#ifdef HAVE_VTE_TERMINAL_SEARCH_SET_GREGEX
#include "roxterm.h"
#endif

#define DLG_SPACING 8

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

#ifdef HAVE_VTE_TERMINAL_SEARCH_SET_GREGEX

static GtkWidget *dlg_search_dialog = NULL;

struct {
    GtkEntry *entry;
    GtkToggleButton *match_case, *entire_word, *as_regex,
            *backwards, *wrap;
    ROXTermData *roxterm;
    VteTerminal *vte;
    MultiWin *win;
} dlg_search_data;

static void dlg_search_destroy_cb(GtkWidget *widget, void *handle)
{
    dlg_search_data->vte = NULL;
    dlg_search_dialog = NULL;
}

static void dlg_search_vte_destroyed_cb(GtkWidget *widget, void *handle)
{
    if (dlg_search_data->vte == widget)
    {
        dlg_search_data->vte = NULL;
        gtk_widget_hide(dlg_search_dialog);
    }
}

static void dlg_search_response_cb(GtkWidget *widget,
        guint response, void *handle)
{
    if (response == GTK_RESPONSE_ACCEPT)
    {
        const char *entered = gtk_entry_get_text(dlg_search_data->entry);
        GRegex *regex = NULL;
        GError *error = NULL;
        char *pattern;
        
        if (!entered)
        {
            dlg_warning(_("Nothing to search for"));
            return;
        }
        pattern = gtk_toggle_button_get_state(dlg_search_data->as_regex) ?
                g_strdup(entered) : g_regex_escape_string(entered);
        if (gtk_toggle_button_get_state(dlg_search_data->entire_word))
        {
            char *tmp = pattern;
            
            pattern = g_strdup_printf("\\<%s\\>", tmp);
            g_free(tmp);
        }
        regex = g_regex_new(pattern,
                (gtk_toggle_button_get_state(dlg_search_data->match_case) ?
                        0 : G_REGEX_CASELESS) |
                G_REGEX_OPTIMIZE,
                G_REGEX_MATCH_NOTEMPTY,
                &error);
        g_free(pattern);
        if (!regex)
        {
            dlg_warning(_("Invalid search expression: %s"), error->message);
            g_error_free(error);
            return;
        }
        vte_terminal_search_set_gregex(dlg_search_data->vte, regex);
        vte_terminal_search_set_wrap_around(
                gtk_toggle_button_get_state(dlg_search_data->wrap));
        roxterm_shade_search_menu_items(dlg_search_data->roxterm);
        if (gtk_toggle_button_get_state(dlg_search_data->backwards))
            vte_terminal_search_find_previous(dlg_search_data->vte);
        else
            vte_terminal_search_find_next(dlg_search_data->vte);
    }
    gtk_widget_hide(dlg_search_dialog);
}

void dlg_open_search(ROXTerm *roxterm)
{
    dlg_search_data->roxterm = roxterm;
    dlg_search_data->win = roxterm_get_multi_win(roxterm);
    dlg_search_data->vte = roxterm_get_vte(roxterm);
    g_signal_connect(vte, "destroy", dlg_search_vte_destroyed_cb, NULL);
    
    if (!dlg_search_dialog)
    {
        GtkBox *vbox;
        GtkWidget *hbox = gtk_hbox_new(FALSE, DLG_SPACING);
        GtkWidget *w = gtk_label_new_with_mnemonic(_("_Search for:"));
        GtkWidget *entry = gtk_entry_new();
        
        dlg_search_dialog = gtk_dialog_new_with_buttons(_("Find"), parent,
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                GTK_STOCK_FIND, GTK_RESPONSE_ACCEPT,
                NULL);
                 
        vbox = GTK_BOX(gtk_dialog_get_content_area(
                GTK_DIALOG(dlg_search_dialog)));

        dlg_search_data->entry = GTK_ENTRY(entry);
        gtk_widget_set_tooltip_text(entry, _("A search string or "
                "perl-compatible regular expression."));
        gtk_label_set_mnemonic_widget(GTK_LABEL(w), entry);
        gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
        gtk_box_pack_start(vbox, hbox, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("Match _Case"));
        gtk_widget_set_tooltip_text(w,
                _("Whether the search is case sensitive"));
        dlg_search_data->match_case = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("Match _Entire Word"));
        gtk_widget_set_tooltip_text(w, _("If set the pattern will only match "
                "when it forms a word on its own."));
        dlg_search_data->entire_word = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(
                _("Match As _Regular Expression"));
        gtk_widget_set_tooltip_text(w, _("If set the pattern is a "
                "perl-compatible regular expression."));
        dlg_search_data->as_regex = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("Search _Backwards"));
        gtk_widget_set_tooltip_text(w, _("Whether to search backwards when "
                "the Find button is clicked. This does not affect the "
                "Find Next and Find Previous menu items."));
        dlg_search_data->backwards = GTK_TOGGLE_BUTTON(w);
        gtk_toggle_button_set_active(dlg_search_data->backwards, TRUE);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("_Wrap Around"));
        gtk_widget_set_tooltip_text(w, _("Whether to wrap the search to the "
                "opposite end of the buffer when the beginning or end is "
                "reached."))
        dlg_search_data->wrap = GTK_TOGGLE_BUTTON(w);
        gtk_toggle_button_set_active(dlg_search_data->wrap, TRUE);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        g_signal_connect(dlg_search_dialog, "response",
                G_CALLBACK(dlg_search_response_cb), NULL);
        g_signal_connect(dlg_search_dialog, "destroy",
                G_CALLBACK(dlg_search_destroy_cb), NULL);
        
    }
    if (gtk_widget_get_visible(dlg_search_dialog))
    {
        gtk_window_present(GTK_WINDOW(dlg_search_dialog));
    }
    else
    {
        gtk_widget_show_all(dlg_search_dialog);
    }
}

#endif

/* vi:set sw=4 ts=4 et cindent cino= */
