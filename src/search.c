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
#include "search.h"

static GtkWidget *search_dialog = NULL;

struct {
    GtkEntry *entry;
    GtkToggleButton *match_case, *entire_word, *as_regex,
            *backwards, *wrap;
    ROXTermData *roxterm;
    VteTerminal *vte;
    MultiWin *win;
} search_data;

static void search_destroy_cb(GtkWidget *widget, void *handle)
{
    search_data.vte = NULL;
    search_dialog = NULL;
}

static void search_vte_destroyed_cb(VteTerminal *widget, void *handle)
{
    if (search_data.vte == widget)
    {
        search_data.vte = NULL;
        gtk_widget_hide(search_dialog);
    }
}

static void search_response_cb(GtkWidget *widget,
        guint response, void *handle)
{
    if (response == GTK_RESPONSE_ACCEPT)
    {
        const char *entered = gtk_entry_get_text(search_data.entry);
        GRegex *regex = NULL;
        GError *error = NULL;
        char *pattern;
        
        if (!entered || !entered[0])
        {
            dlg_warning(GTK_WINDOW(search_dialog), _("Nothing to search for"));
            return;
        }
        pattern = gtk_toggle_button_get_active(search_data.as_regex) ?
                g_strdup(entered) : g_regex_escape_string(entered, -1);
        if (gtk_toggle_button_get_active(search_data.entire_word))
        {
            char *tmp = pattern;
            
            pattern = g_strdup_printf("\\<%s\\>", tmp);
            g_free(tmp);
        }
        regex = g_regex_new(pattern,
                (gtk_toggle_button_get_active(search_data.match_case) ?
                        0 : G_REGEX_CASELESS) |
                G_REGEX_OPTIMIZE,
                G_REGEX_MATCH_NOTEMPTY,
                &error);
        g_free(pattern);
        if (!regex)
        {
            dlg_warning(GTK_WINDOW(search_dialog),
                    _("Invalid search expression: %s"), error->message);
            g_error_free(error);
            return;
        }
        vte_terminal_search_set_gregex(search_data.vte, regex);
        vte_terminal_search_set_wrap_around(search_data.vte,
                gtk_toggle_button_get_active(search_data.wrap));
        roxterm_shade_search_menu_items(search_data.roxterm);
        if (gtk_toggle_button_get_active(search_data.backwards))
            vte_terminal_search_find_previous(search_data.vte);
        else
            vte_terminal_search_find_next(search_data.vte);
    }
    gtk_widget_hide(search_dialog);
}

void search_open_dialog(ROXTermData *roxterm)
{
    search_data.roxterm = roxterm;
    search_data.win = roxterm_get_multi_win(roxterm);
    search_data.vte = roxterm_get_vte(roxterm);
    g_signal_connect(search_data.vte, "destroy",
            G_CALLBACK(search_vte_destroyed_cb), NULL);
    
    if (!search_dialog)
    {
        GtkBox *vbox;
        GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
        GtkWidget *w = gtk_label_new_with_mnemonic(_("_Search for:"));
        GtkWidget *entry = gtk_entry_new();
        
        search_dialog = gtk_dialog_new_with_buttons(_("Find"),
                GTK_WINDOW(multi_win_get_widget(search_data.win)),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                GTK_STOCK_FIND, GTK_RESPONSE_ACCEPT,
                NULL);
                 
        vbox = GTK_BOX(gtk_dialog_get_content_area(
                GTK_DIALOG(search_dialog)));

        search_data.entry = GTK_ENTRY(entry);
        gtk_entry_set_width_chars(search_data.entry, 40);
        gtk_widget_set_tooltip_text(entry, _("A search string or "
                "perl-compatible regular expression."));
        gtk_label_set_mnemonic_widget(GTK_LABEL(w), entry);
        gtk_box_pack_start(GTK_BOX(hbox), w, FALSE, FALSE, DLG_SPACING);
        gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, DLG_SPACING);
        gtk_box_pack_start(vbox, hbox, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("Match _Case"));
        gtk_widget_set_tooltip_text(w,
                _("Whether the search is case sensitive"));
        search_data.match_case = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("Match _Entire Word"));
        gtk_widget_set_tooltip_text(w, _("If set the pattern will only match "
                "when it forms a word on its own."));
        search_data.entire_word = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(
                _("Match As _Regular Expression"));
        gtk_widget_set_tooltip_text(w, _("If set the pattern is a "
                "perl-compatible regular expression."));
        search_data.as_regex = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("Search _Backwards"));
        gtk_widget_set_tooltip_text(w, _("Whether to search backwards when "
                "the Find button is clicked. This does not affect the "
                "Find Next and Find Previous menu items."));
        search_data.backwards = GTK_TOGGLE_BUTTON(w);
        gtk_toggle_button_set_active(search_data.backwards, TRUE);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("_Wrap Around"));
        gtk_widget_set_tooltip_text(w, _("Whether to wrap the search to the "
                "opposite end of the buffer when the beginning or end is "
                "reached."));
        search_data.wrap = GTK_TOGGLE_BUTTON(w);
        gtk_toggle_button_set_active(search_data.wrap, TRUE);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        g_signal_connect(search_dialog, "response",
                G_CALLBACK(search_response_cb), NULL);
        g_signal_connect(search_dialog, "destroy",
                G_CALLBACK(search_destroy_cb), NULL);
        
    }
    if (gtk_widget_get_visible(search_dialog))
    {
        gtk_window_present(GTK_WINDOW(search_dialog));
    }
    else
    {
        gtk_widget_show_all(search_dialog);
    }
}

/* vi:set sw=4 ts=4 et cindent cino= */
