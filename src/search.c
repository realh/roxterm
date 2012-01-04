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

#include <errno.h>

#include "dlg.h"
#include "search.h"

/* Too many completions may be distracting */
#define SEARCH_MAX_COMPLETIONS 32

static GtkWidget *search_dialog = NULL;

struct {
    GtkEntry *entry;
    GtkEntryCompletion *completion;
    GtkTreeModel *model;
    GtkToggleButton *match_case, *entire_word, *as_regex,
            *backwards, *wrap;
    ROXTermData *roxterm;
    VteTerminal *vte;
    MultiWin *win;
} search_data;

static char *search_get_filename(gboolean create_dir)
{
    char *dir = g_build_filename(g_get_user_config_dir(), ROXTERM_LEAF_DIR,
            NULL);
    char *pathname;
    
    if (create_dir && !g_file_test(dir, G_FILE_TEST_IS_DIR))
    {
        if (g_mkdir_with_parents(dir, 0755))
        {
            g_warning(_("Unable to create config directory '%s': %s"),
                    dir, strerror(errno));
            g_free(dir);
            return NULL;
        }
    }
    pathname = g_build_filename(dir, "Searches", NULL);
    g_free(dir);
    return pathname;
}

/* Creates a model for the completion, populating it with strings loaded
 * from file if present.
 */
static GtkTreeModel *search_create_model(void)
{
    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
    char *filename = search_get_filename(FALSE);
    
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
    {
        GError *error = NULL;
        GIOChannel *ioc = g_io_channel_new_file(filename, "r", &error);
        
        if (!ioc)
        {
            g_warning(_("Unable to read search history file '%s': %s"),
                    filename, error->message);
            g_error_free(error);
        }
        while (ioc)
        {
            char *line = NULL;
            GtkTreeIter iter;
            gsize len = 0;
            
            switch (g_io_channel_read_line(ioc, &line, &len, NULL, &error))
            {
                case G_IO_STATUS_NORMAL:
                    if (len && line[len - 1] == '\n')
                    {
                        if (len > 1 && line[len - 2] == '\r')
                            line[len - 2] = 0;
                        else
                            line[len - 1] = 0;
                    }
                    gtk_list_store_append(store, &iter);
                    gtk_list_store_set(store, &iter, 0, line, -1);
                    g_free(line);
                    break;
                case G_IO_STATUS_AGAIN:
                case G_IO_STATUS_ERROR:
                    g_warning(_("Error reading search history file: %s"),
                            error ? error->message : _("Already in use"));
                    if (error)
                    {
                        g_error_free(error);
                        error = NULL;
                    }
                case G_IO_STATUS_EOF:
                    g_io_channel_shutdown(ioc, FALSE, NULL);
                    g_io_channel_unref(ioc);
                    ioc = NULL;
                    break;
            }
        }
    }
    g_free(filename);
    return GTK_TREE_MODEL(store);
}

static void search_save_completion(void)
{
    char *filename = search_get_filename(TRUE);
    GError *error = NULL;
    GIOChannel *ioc = g_io_channel_new_file(filename, "w", &error);
    GtkTreeIter iter;
    gboolean iterable;
    
    if (!ioc)
    {
        g_warning(_("Unable to write search history file '%s': %s"),
                filename, error->message);
        g_error_free(error);
        g_free(filename);
        return;
    }
    iterable = gtk_tree_model_get_iter_first(search_data.model, &iter);
    
    while (iterable)
    {
        char *pattern;
        char *line;
        
        gtk_tree_model_get(search_data.model, &iter, 0, &pattern);
        line = g_strdup_printf("%s\n", pattern);
        switch (g_io_channel_write_chars(ioc, line, -1, NULL, &error))
        {
            case G_IO_STATUS_NORMAL:
                iterable = gtk_tree_model_iter_next(search_data.model, &iter);
                break;
            default:
                g_warning(_("Error writing search history file: %s"),
                        error ? error->message : _("Already in use"));
                if (error)
                {
                    g_error_free(error);
                    error = NULL;
                }
                break;
        }
        g_free(line);
        g_free(pattern);
    }
    g_io_channel_shutdown(ioc, TRUE, NULL);
    g_io_channel_unref(ioc);
    g_free(filename);
}

static void search_setup_completion(void)
{
    search_data.model = search_create_model();
    search_data.completion = gtk_entry_completion_new();
    gtk_entry_completion_set_model(search_data.completion, search_data.model);
    gtk_entry_completion_set_text_column(search_data.completion, 0);
    gtk_entry_completion_set_inline_completion(search_data.completion, FALSE);
    gtk_entry_completion_set_inline_selection(search_data.completion, TRUE);
    gtk_entry_completion_set_popup_completion(search_data.completion, TRUE);
    gtk_entry_completion_set_popup_single_match(search_data.completion, TRUE);
}

/* Adds a new pattern or moves an existing one to the top of the list */
static void search_update_completion(const char *pattern)
{
    GtkTreeIter iter, last;
    gboolean iterable = gtk_tree_model_get_iter_first(search_data.model, &iter);
    int index = 0;
    
    while (iterable)
    {
        char *line;
        
        gtk_tree_model_get(search_data.model, &iter, 0, &line, -1);
        if (!strcmp(line, pattern))
        {
            GtkTreeIter top;
            
            g_free(line);
            if (!index)
            {
                /* This string is already at top, do nothing */
                return;
            }
            /* Move to top of list */
            gtk_tree_model_get_iter_first(search_data.model, &top);
            gtk_list_store_move_before(GTK_LIST_STORE(search_data.model),
                    &iter, &top);
            search_save_completion();
            return;
        }
        ++index;
        last = iter;
        iterable = gtk_tree_model_iter_next(search_data.model, &iter);
    }
    if (index >= SEARCH_MAX_COMPLETIONS)
    {
        gtk_list_store_remove(GTK_LIST_STORE(search_data.model), &last);
    }
    gtk_list_store_append(GTK_LIST_STORE(search_data.model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(search_data.model), &iter,
            0, pattern, -1);
    search_save_completion();
}

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
        if (search_dialog)
            gtk_widget_hide(search_dialog);
    }
}

static void search_response_cb(GtkWidget *widget,
        guint response, void *handle)
{
    if (response == GTK_RESPONSE_ACCEPT)
    {
        GError *error = NULL;
        const char *pattern = gtk_entry_get_text(search_data.entry);
        gboolean backwards =
                gtk_toggle_button_get_active(search_data.backwards);
        guint flags =
                (gtk_toggle_button_get_active(search_data.match_case) ?
                        0 : ROXTERM_SEARCH_MATCH_CASE) |
                (gtk_toggle_button_get_active(search_data.as_regex) ?
                        ROXTERM_SEARCH_AS_REGEX : 0) |
                (gtk_toggle_button_get_active(search_data.entire_word) ?
                        ROXTERM_SEARCH_ENTIRE_WORD : 0) |
                (backwards ? 
                        ROXTERM_SEARCH_BACKWARDS : 0) |
                (gtk_toggle_button_get_active(search_data.wrap) ?
                        ROXTERM_SEARCH_WRAP : 0);
        
        if (pattern && pattern[0])
            search_update_completion(pattern);
        if (roxterm_set_search(search_data.roxterm, pattern, flags, &error))
        {
            if (pattern && pattern[0])
            {
                if (backwards)
                    vte_terminal_search_find_previous(search_data.vte);
                else
                    vte_terminal_search_find_next(search_data.vte);
            }
        }
        else
        {
            dlg_warning(GTK_WINDOW(search_dialog),
                    _("Invalid search expression: %s"), error->message);
            g_error_free(error);
            /* Keep dialog open if there was an error */
            return;
        }
    }
    gtk_widget_hide(search_dialog);
}

void search_open_dialog(ROXTermData *roxterm)
{
    const char *pattern = roxterm_get_search_pattern(roxterm);
    guint flags = roxterm_get_search_flags(roxterm);
    
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
        gtk_dialog_set_default_response(GTK_DIALOG(search_dialog),
                GTK_RESPONSE_ACCEPT);
                 
        vbox = GTK_BOX(gtk_dialog_get_content_area(
                GTK_DIALOG(search_dialog)));

        search_data.entry = GTK_ENTRY(entry);
        search_setup_completion();
        gtk_entry_set_width_chars(search_data.entry, 40);
        gtk_entry_set_activates_default(search_data.entry, TRUE);
        gtk_widget_set_tooltip_text(entry, _("A search string or "
                "perl-compatible regular expression. An empty string "
                "clears any previous search."));
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
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        w = gtk_check_button_new_with_mnemonic(_("_Wrap Around"));
        gtk_widget_set_tooltip_text(w, _("Whether to wrap the search to the "
                "opposite end of the buffer when the beginning or end is "
                "reached."));
        search_data.wrap = GTK_TOGGLE_BUTTON(w);
        gtk_box_pack_start(vbox, w, FALSE, FALSE, DLG_SPACING);
        
        g_signal_connect(search_dialog, "response",
                G_CALLBACK(search_response_cb), NULL);
        g_signal_connect(search_dialog, "destroy",
                G_CALLBACK(search_destroy_cb), NULL);
        
    }
    
    if (pattern)
    {
        gtk_entry_set_text(search_data.entry, pattern);
        gtk_editable_select_region(GTK_EDITABLE(search_data.entry),
                0, g_utf8_strlen(pattern, -1));
    }
    else
    {
        gtk_entry_set_text(search_data.entry, "");
        flags = ROXTERM_SEARCH_BACKWARDS | ROXTERM_SEARCH_WRAP;
    }
    gtk_toggle_button_set_active(search_data.match_case,
            flags & ROXTERM_SEARCH_MATCH_CASE);
    gtk_toggle_button_set_active(search_data.entire_word,
            flags & ROXTERM_SEARCH_ENTIRE_WORD);
    gtk_toggle_button_set_active(search_data.as_regex,
            flags & ROXTERM_SEARCH_AS_REGEX);
    gtk_toggle_button_set_active(search_data.backwards,
            flags & ROXTERM_SEARCH_BACKWARDS);
    gtk_toggle_button_set_active(search_data.wrap,
            flags & ROXTERM_SEARCH_WRAP);
    
    if (gtk_widget_get_visible(search_dialog))
    {
        gtk_window_present(GTK_WINDOW(search_dialog));
    }
    else
    {
        gtk_widget_show_all(search_dialog);
    }
    gtk_widget_grab_focus(GTK_WIDGET(search_data.entry));
}

/* vi:set sw=4 ts=4 et cindent cino= */
