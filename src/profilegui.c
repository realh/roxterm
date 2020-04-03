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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "capplet.h"
#include "colourgui.h"
#include "configlet.h"
#include "dlg.h"
#include "dragrcv.h"
#include "dynopts.h"
#include "options.h"
#include "profilegui.h"
#include "resources.h"

#define MANAGE_PREVIEW

struct _ProfileGUI {
    CappletData capp;
    GtkWidget *dialog;
    GtkWidget *ssh_dialog;
    char *profile_name;
    gboolean ignore_destroy;
    struct {
        guint word_chars : 1;
        guint term : 1;
        guint browser : 1;
        guint mailer : 1;
        guint ssh : 1;
        guint filer : 1;
        guint dir_filer : 1;
        guint command : 1;
        guint title_string : 1;
        guint ssh_address : 1;
        guint ssh_user : 1;
        guint ssh_options : 1;
        guint win_title : 1;
    } changed;
    DragReceiveData *bgimg_drd;
    GtkListStore *list_store;
};

static GHashTable *profilegui_being_edited = NULL;

#if 0
inline static
GtkWidget *profilegui_widget(ProfileGUI *pg, const char *name)
{
    /* Result will often be cast to a specialised widget so use static
     * cast here to avoid double GObject cast.
     */
    return (GtkWidget *) gtk_builder_get_object(pg->capp.builder, name);
}
#endif
static
GtkWidget *profilegui_widget(ProfileGUI *pg, const char *name)
{
    GtkWidget *widget =
            (GtkWidget *)gtk_builder_get_object(pg->capp.builder, name);

    if (!widget)
        g_critical(_("Profile widget '%s' not found"), name);
    return widget;
}

static void profilegui_update_from_entry(ProfileGUI * pg, const char *name)
{
    capplet_set_string(pg->capp.options, name,
            gtk_entry_get_text(
                    GTK_ENTRY(gtk_builder_get_object(pg->capp.builder, name))));
}

#define PG_MEMBER(a, b) a . b

#define PG_UPDATE_IF(s) if (PG_MEMBER(pg->changed, s)) \
    { \
        profilegui_update_from_entry(pg, # s); \
        PG_MEMBER(pg->changed, s) = 0; \
    }

void profilegui_check_entries_for_changes(ProfileGUI * pg)
{
    PG_UPDATE_IF(word_chars)
    PG_UPDATE_IF(term)
    PG_UPDATE_IF(browser)
    PG_UPDATE_IF(mailer)
    PG_UPDATE_IF(ssh)
    PG_UPDATE_IF(filer)
    PG_UPDATE_IF(dir_filer)
    PG_UPDATE_IF(command)
    PG_UPDATE_IF(title_string)
    PG_UPDATE_IF(win_title)
}

static void profilegui_set_command_shading(ProfileGUI *pg)
{
    gboolean use_custom = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(profilegui_widget(pg, "use_custom_command")));
    gboolean use_ssh = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(profilegui_widget(pg, "use_ssh")));

    gtk_widget_set_sensitive(profilegui_widget(pg, "command"), use_custom);
    gtk_widget_set_sensitive(profilegui_widget(pg, "ssh_host"), use_ssh);
    gtk_widget_set_sensitive(profilegui_widget(pg, "edit_ssh"), use_ssh);
    gtk_widget_set_sensitive(profilegui_widget(pg, "login_shell"),
            !use_custom && !use_ssh);
}

static void profilegui_set_close_buttons_shading(ProfileGUI *pg)
{
    gtk_widget_set_sensitive(profilegui_widget(pg, "show_tab_status"),
            gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(profilegui_widget(pg, "tab_close_btn"))));
}

static void profilegui_set_scrollbar_shading(ProfileGUI *pg)
{
    gtk_widget_set_sensitive(profilegui_widget(pg, "overlay_scrollbar"),
            !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
                    profilegui_widget(pg, "scrollbar_pos0"))));
}

static void profilegui_check_ssh_entries_for_changes(ProfileGUI * pg)
{
    PG_UPDATE_IF(ssh_address)
    PG_UPDATE_IF(ssh_user)
    PG_UPDATE_IF(ssh_options)
}

/***********************************************************/
/* Handlers */

void on_editable_changed(GtkEditable * editable, ProfileGUI *pg)
{
    const char *n = gtk_buildable_get_name(GTK_BUILDABLE(editable));
#define PG_IF_CHANGED(s) if (!strcmp(n, #s)) PG_MEMBER(pg->changed, s) = 1

    PG_IF_CHANGED(word_chars);
    else PG_IF_CHANGED(term);
    else PG_IF_CHANGED(browser);
    else PG_IF_CHANGED(mailer);
    else PG_IF_CHANGED(ssh);
    else PG_IF_CHANGED(dir_filer);
    else PG_IF_CHANGED(filer);
    else PG_IF_CHANGED(command);
    else PG_IF_CHANGED(title_string);
    else PG_IF_CHANGED(ssh_address);
    else PG_IF_CHANGED(ssh_user);
    else PG_IF_CHANGED(ssh_options);
    else PG_IF_CHANGED(win_title);
    else if (!strcmp(n, "ssh_port"))
    {
        GtkAdjustment *adj = gtk_spin_button_get_adjustment(
                GTK_SPIN_BUTTON(editable));
        gtk_adjustment_set_value(adj,
                atoi(gtk_entry_get_text(GTK_ENTRY(editable))));
    }
}

void on_ssh_entry_activate(GtkEntry * entry, ProfileGUI * pg)
{
    (void) entry;
    profilegui_check_ssh_entries_for_changes(pg);
}

void on_ssh_host_changed(GtkWidget * widget, ProfileGUI *pg)
{
    gtk_entry_set_text(GTK_ENTRY(profilegui_widget(pg, "ssh_host")),
            gtk_entry_get_text(GTK_ENTRY(widget)));
}

void on_edit_ssh_clicked(GtkWidget * widget, ProfileGUI *pg)
{
    (void) widget;
    capplet_inc_windows();

    capplet_set_text_entry(&pg->capp, "ssh_address", "localhost");
    capplet_set_text_entry(&pg->capp, "ssh_user", NULL);
    capplet_set_spin_button(&pg->capp, "ssh_port", 22);
    capplet_set_text_entry(&pg->capp, "ssh_options", NULL);
    gtk_dialog_run(GTK_DIALOG(pg->ssh_dialog));
    profilegui_check_ssh_entries_for_changes(pg);
    gtk_widget_hide(pg->ssh_dialog);
    capplet_dec_windows();
}

void on_Profile_Editor_destroy(GtkWidget * widget, ProfileGUI * pg)
{
    (void) widget;
    if (pg->ignore_destroy)
    {
        return;
    }
    else
    {
        pg->ignore_destroy = TRUE;
        profilegui_delete(pg);
    }
}

void on_Profile_Editor_response(GtkWidget *widget, int response, ProfileGUI *pg)
{
    (void) widget;
    (void) response;
    profilegui_delete(pg);
}

void on_Profile_Editor_close(GtkWidget * widget, ProfileGUI * pg)
{
    (void) widget;
    profilegui_delete(pg);
}

void on_font_set(GtkFontButton * fontbutton, ProfileGUI * pg)
{
    capplet_set_string(pg->capp.options,
        "font", gtk_font_chooser_get_font(GTK_FONT_CHOOSER(fontbutton)));
}

void on_entry_activate(GtkEntry * entry, ProfileGUI * pg)
{
    (void) entry;
    profilegui_check_entries_for_changes(pg);
}

void on_profile_notebook_switch_page(GtkNotebook * notebook, GtkWidget *page,
        guint page_num, ProfileGUI * pg)
{
    (void) notebook;
    (void) page;
    (void) page_num;
    profilegui_check_entries_for_changes(pg);
}

void on_command_toggled(GtkToggleButton *button, ProfileGUI *pg)
{
    (void) button;
    profilegui_set_command_shading(pg);
}

void on_close_buttons_toggled(GtkToggleButton *button, ProfileGUI *pg)
{
    (void) button;
    profilegui_set_close_buttons_shading(pg);
}

void on_cell_size_toggled(GtkToggleButton *button, ProfileGUI *pg)
{
    gboolean state = gtk_toggle_button_get_active(button);

    gtk_widget_set_sensitive(profilegui_widget(pg, "width"), state);
    gtk_widget_set_sensitive(profilegui_widget(pg, "height"), state);
}

void on_scrollbar_pos_toggled(GtkRadioButton *button, ProfileGUI *pg)
{
    (void) button;
    profilegui_set_scrollbar_shading(pg);
}

#define DEFAULT_BACKSPACE_BINDING 0
#define DEFAULT_DELETE_BINDING 0

void on_reset_compat_clicked(GtkButton *button, ProfileGUI *pg)
{
    (void) button;
    capplet_set_int(pg->capp.options, "backspace_binding",
            DEFAULT_BACKSPACE_BINDING);
    capplet_set_combo(&pg->capp, "backspace_binding",
            DEFAULT_BACKSPACE_BINDING);
    capplet_set_int(pg->capp.options, "delete_binding",
            DEFAULT_DELETE_BINDING);
    capplet_set_combo(&pg->capp, "delete_binding",
            DEFAULT_DELETE_BINDING);
}

void on_edit_colour_scheme_clicked(GtkButton *button, ProfileGUI *pg)
{
    (void) button;
    GtkWidget *combo = capplet_lookup_widget(&pg->capp, "colour_scheme");
    char *name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    colourgui_open(name);
}

static void exit_action_changed(GtkComboBox *combo, ProfileGUI *pg)
{
    gtk_widget_set_sensitive(profilegui_widget(pg, "exit_pause"),
            gtk_combo_box_get_active(combo) != 3);
}

/***********************************************************/

/* Hopefully all this combo support stuff can go away when everything
 * supports GtkComboBoxText.
 */

inline static void profilegui_add_combo_item(GtkWidget *combo, const char *item)
{
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), item);
}

/* GtkBuilder signal handlers can't be static */
void on_colour_scheme_combo_changed(GtkComboBox *combo,
        CappletData *capp)
{
    char *value;

    if (capplet_ignore_changes)
        return;

    gboolean unset = gtk_combo_box_get_active(combo) == 0;
    gtk_widget_set_sensitive(
            GTK_WIDGET(gtk_builder_get_object(capp->builder,
                    "edit_colour_scheme")), !unset);
    if (unset)
    {
        value = NULL;
    }
    else
    {
        value = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    }
    capplet_set_string(capp->options, "colour_scheme", value);
    g_free(value);
}

static void profilegui_init_colour_scheme_combo(ProfileGUI *pg)
{
    GtkWidget *combo = GTK_WIDGET(gtk_builder_get_object(pg->capp.builder,
                "colour_scheme"));
    char **schemes = dynamic_options_list_sorted(
            dynamic_options_get("Colours"));
    int n;
    profilegui_add_combo_item(combo, _("(Don't set)"));
    if (!schemes[0] || strcmp(schemes[0], _("Default")))
        profilegui_add_combo_item(combo, _("Default"));
    for (n = 0; schemes[n]; ++n)
        profilegui_add_combo_item(combo, schemes[n]);
}

static void profilegui_set_colour_scheme_combo(CappletData *capp)
{
    GtkWidget *combo = capplet_lookup_widget(capp, "colour_scheme");
    char *value = options_lookup_string(capp->options, "colour_scheme");
    int active = 0;
    if (value && value[0])
    {
        GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
        GtkTreeIter iter;
        gboolean valid;
        /* We skip the initial "(Don't set)" item */
        active = 1;
        for (valid = (gtk_tree_model_get_iter_first(model, &iter) &&
                gtk_tree_model_iter_next(model, &iter));
                valid;
                valid = gtk_tree_model_iter_next(model, &iter), ++active)
        {
            char *item;
            gtk_tree_model_get(model, &iter, 0, &item, -1);
            if (!strcmp(item, value))
            {
                g_free(item);
                break;
            }
            g_free(item);
        }
        if (!valid)
            profilegui_add_combo_item(combo, value);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
    gtk_widget_set_sensitive(
            GTK_WIDGET(gtk_builder_get_object(capp->builder,
                    "edit_colour_scheme")), active != 0);
    g_free(value);
}

static void profilegui_connect_handlers(ProfileGUI * pg)
{
    gtk_builder_connect_signals(pg->capp.builder, pg);
}

static gboolean profilegui_font_filter(const PangoFontFamily *family,
        const PangoFontFace *face, gpointer data)
{
    (void) face;
    (void) data;

    return pango_font_family_is_monospace((PangoFontFamily *) family);
}

static void profilegui_fill_in_dialog(ProfileGUI * pg)
{
    Options *profile = pg->capp.options;
    char *val;
    GtkFontChooser *font_chooser;

    capplet_set_spin_button(&pg->capp, "vspacing", 0);
    capplet_set_spin_button(&pg->capp, "hspacing", 0);
    capplet_set_toggle(&pg->capp, "bold_is_bright", FALSE);
    capplet_set_radio(&pg->capp, "text_blink_mode", 0);

    if (options_lookup_int_with_default(profile, "full_screen", 0))
    {
        capplet_set_toggle(&pg->capp, "full_screen", TRUE);
    }
    else if (options_lookup_int_with_default(profile, "maximise", 0))
    {
        capplet_set_toggle(&pg->capp, "maximise", TRUE);
    }
    else
    {
        capplet_set_toggle(&pg->capp, "cell_size", TRUE);
    }
    if (options_lookup_int_with_default(profile, "borderless", 0))
    {
        capplet_set_toggle(&pg->capp, "borderless", TRUE);
    }
    font_chooser = GTK_FONT_CHOOSER(profilegui_widget(pg, "font_button"));
    gtk_font_chooser_set_filter_func(font_chooser, profilegui_font_filter,
            NULL, NULL);
    gtk_font_chooser_set_font( font_chooser,
            val = options_lookup_string_with_default(profile,
                "font", "Monospace 10"));
    g_free(val);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(profilegui_widget(pg,
                "show_menubar")), !options_lookup_int_with_default(profile,
            "hide_menubar", FALSE));
    capplet_set_boolean_toggle(&pg->capp, "audible_bell", TRUE);
    capplet_set_boolean_toggle(&pg->capp, "bell_highlights_tab", TRUE);
    {
        /* Use legacy cursor_blinks if cursor_blink_mode has not been set */
        int o = options_lookup_int(profile, "cursor_blinks") + 1;
        if (o)
            o ^= 3;
        capplet_set_radio(&pg->capp, "cursor_blink_mode", o);
    }
    capplet_set_radio(&pg->capp, "cursor_shape", 0);
    capplet_set_boolean_toggle(&pg->capp, "hide_mouse", TRUE);
    capplet_set_text_entry(&pg->capp, "word_chars", "-,./?%&#_");
    capplet_set_text_entry(&pg->capp, "term", NULL);
    capplet_set_float_range(&pg->capp, "saturation", 1.0);
    capplet_set_combo(&pg->capp, "tab_pos", 0);
    capplet_set_boolean_toggle(&pg->capp, "wrap_switch_tab", FALSE);
    capplet_set_boolean_toggle(&pg->capp, "tab_close_btn", TRUE);
    capplet_set_boolean_toggle(&pg->capp, "show_tab_status", FALSE);
    capplet_set_boolean_toggle(&pg->capp, "always_show_tabs", TRUE);
    capplet_set_boolean_toggle(&pg->capp, "show_add_tab_btn", TRUE);
    capplet_set_boolean_toggle(&pg->capp, "new_tabs_adjacent", FALSE);
    capplet_set_radio(&pg->capp, "middle_click_tab", 0);
    profilegui_set_close_buttons_shading(pg);
    capplet_set_text_entry(&pg->capp, "ssh", NULL);
    /*capplet_set_boolean_toggle(&pg->capp, "match_plain_files", FALSE);*/
    capplet_set_spin_button(&pg->capp, "width", 80);
    capplet_set_spin_button(&pg->capp, "height", 24);
    on_cell_size_toggled(GTK_TOGGLE_BUTTON(profilegui_widget(pg, "cell_size")),
            pg);
    capplet_set_boolean_toggle(&pg->capp, "overlay_scrollbar", TRUE);
    capplet_set_radio(&pg->capp, "scrollbar_pos", 1);
    profilegui_set_scrollbar_shading(pg);
    capplet_set_boolean_toggle(&pg->capp, "limit_scrollback", FALSE);
    capplet_set_spin_button(&pg->capp, "scrollback_lines", 1000);
    capplet_set_boolean_toggle(&pg->capp, "scroll_on_output", FALSE);
    capplet_set_boolean_toggle(&pg->capp, "scroll_on_keystroke", FALSE);
    capplet_set_combo(&pg->capp, "backspace_binding",
            DEFAULT_BACKSPACE_BINDING);
    capplet_set_combo(&pg->capp, "delete_binding",
            DEFAULT_DELETE_BINDING);
    capplet_set_boolean_toggle(&pg->capp, "disable_menu_access", FALSE);
    capplet_set_boolean_toggle(&pg->capp, "disable_menu_shortcuts", FALSE);
    capplet_set_boolean_toggle(&pg->capp, "disable_tab_menu_shortcuts",
            FALSE);
    capplet_ignore_changes = TRUE;
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(profilegui_widget(pg, "use_default_shell")),
            TRUE);
    gtk_entry_set_text(GTK_ENTRY(profilegui_widget(pg, "ssh_host")),
            options_lookup_string_with_default(profile, "ssh_address",
                    "localhost"));
    capplet_ignore_changes = FALSE;
    capplet_set_boolean_toggle(&pg->capp, "use_ssh", FALSE);
    capplet_set_boolean_toggle(&pg->capp, "login_shell", FALSE);
    capplet_set_boolean_toggle(&pg->capp, "use_custom_command", FALSE);
    capplet_set_text_entry(&pg->capp, "command", NULL);
    capplet_set_combo(&pg->capp, "exit_action", 0);
    profilegui_set_colour_scheme_combo(&pg->capp);
    capplet_set_spin_button_float(&pg->capp, "exit_pause");
    capplet_set_text_entry(&pg->capp, "title_string", "%t. %s");
    capplet_set_text_entry(&pg->capp, "win_title", "%s");
    exit_action_changed(
        GTK_COMBO_BOX(capplet_lookup_widget(&pg->capp, "exit_action")),
        pg);
    profilegui_set_command_shading(pg);
}

static gboolean page_selected(GtkTreeSelection *selection,
        GtkTreeModel *model, GtkTreePath *path,
        gboolean path_currently_selected, gpointer pg)
{
    GtkTreeIter iter;
    int page;
    (void) path_currently_selected;
    (void) selection;

    if (!gtk_tree_model_get_iter(model, &iter, path))
        return FALSE;
    gtk_tree_model_get(model, &iter, 1, &page, -1);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(profilegui_widget(pg,
            "profile_notebook")), page);
    return TRUE;
}

static void profilegui_setup_list_store(ProfileGUI *pg)
{
    static char const *labels[] = {
            N_("Text"), N_("Appearance"), N_("Command"), N_("General"),
            N_("Scrolling"), N_("Keyboard"), N_("Tabs")
    };
    GtkTreeIter iter;
    guint n;
    GtkTreeViewColumn *column;
    GtkWidget *tvw;
    GtkTreeView *tv;
    GtkTreeSelection *sel;

    pg->list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    for (n = 0; n < G_N_ELEMENTS(labels); ++n)
    {
        gtk_list_store_append(pg->list_store, &iter);
        gtk_list_store_set(pg->list_store, &iter,
                0, gettext(labels[n]), 1, n, -1);
    }
    tvw = gtk_tree_view_new_with_model(GTK_TREE_MODEL(pg->list_store));
    tv = GTK_TREE_VIEW(tvw);
    gtk_tree_view_set_headers_visible(tv, FALSE);
    sel = gtk_tree_view_get_selection(tv);
    column = gtk_tree_view_column_new_with_attributes("Page",
            gtk_cell_renderer_text_new(), "text", 0, NULL);
    gtk_tree_view_append_column(tv, column);
    gtk_tree_selection_set_select_function(sel, page_selected, pg, NULL);
    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(pg->list_store), &iter);
    gtk_tree_selection_select_iter(sel, &iter);
    gtk_container_add(GTK_CONTAINER(
            profilegui_widget(pg, "page_selector_container")), tvw);
    gtk_widget_show(tvw);
}

/* Loads a Profile and creates a working dialog box for it */
ProfileGUI *profilegui_open(const char *profile_name)
{
    static DynamicOptions *profiles = NULL;
    ProfileGUI *pg;
    char *title;
    static const char *adj_names[] = {
            "width_adjustment", "height_adjustment",
            "exit_pause_adjustment", "scrollback_lines_adjustment",
            "saturation_adjustment", "ssh_port_adjustment",
            "hspacing_adjustment", "vspacing_adjustment",
            NULL };
    static const char *obj_names[] = {
            "Profile_Editor", "ssh_dialog",
            "general_entries_size_group", "general_entry_labels_size_group",
            NULL };
    GError *error = NULL;

    if (!profilegui_being_edited)
    {
        profilegui_being_edited = g_hash_table_new_full(g_str_hash, g_str_equal,
                g_free, NULL);
    }
    pg = g_hash_table_lookup(profilegui_being_edited, profile_name);
    if (pg)
    {
        gtk_window_present(GTK_WINDOW(pg->dialog));
        return pg;
    }

    if (!profiles)
        profiles = dynamic_options_get("Profiles");

    pg = g_new0(ProfileGUI, 1);
    pg->profile_name = g_strdup(profile_name);
    pg->capp.options = dynamic_options_lookup_and_ref(profiles, profile_name,
        "roxterm profile");

    pg->capp.builder = gtk_builder_new();
    if (!gtk_builder_add_objects_from_resource(pg->capp.builder,
            ROXTERM_RESOURCE_UI, (char **) adj_names, &error) ||
            !gtk_builder_add_objects_from_resource(pg->capp.builder,
            ROXTERM_RESOURCE_UI, (char **) obj_names, &error))
    {
        g_error(_("Unable to load GTK UI definitions: %s"), error->message);
    }
    pg->dialog = profilegui_widget(pg, "Profile_Editor");
    pg->ssh_dialog = profilegui_widget(pg, "ssh_dialog");

    title = g_strdup_printf(_("ROXTerm Profile \"%s\""), profile_name);
    gtk_window_set_title(GTK_WINDOW(pg->dialog), title);
    g_free(title);

    profilegui_init_colour_scheme_combo(pg);

    profilegui_setup_list_store(pg);

    g_hash_table_insert(profilegui_being_edited, g_strdup(profile_name), pg);

    profilegui_fill_in_dialog(pg);
    profilegui_connect_handlers(pg);

    gtk_widget_show(pg->dialog);

    capplet_inc_windows();
    configlet_lock_profiles();

    return pg;
}

void profilegui_delete(ProfileGUI * pg)
{
    static DynamicOptions *dynopts = NULL;

    if (!dynopts)
        dynopts = dynamic_options_get("Profiles");

    profilegui_check_ssh_entries_for_changes(pg);
    profilegui_check_entries_for_changes(pg);
    if (!g_hash_table_remove(profilegui_being_edited, pg->profile_name))
    {
        g_critical(_("Deleting edited profile that wasn't in list of profiles "
                    "being edited"));
    }
    if (!pg->ignore_destroy)
    {
        pg->ignore_destroy = TRUE;
        gtk_widget_destroy(pg->dialog);
        gtk_widget_destroy(pg->ssh_dialog);
    }
    UNREF_LOG(g_object_unref(pg->list_store));
    UNREF_LOG(g_object_unref(pg->capp.builder));
    dynamic_options_unref(dynopts, pg->profile_name);
    g_free(pg->profile_name);
    drag_receive_data_delete(pg->bgimg_drd);
    g_free(pg);
    capplet_dec_windows();
    configlet_unlock_profiles();
}

/* vi:set sw=4 ts=4 noet cindent cino= */
