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


#include "defns.h"

#include <string.h>

#include "capplet.h"
#include "configlet.h"
#include "dlg.h"
#include "dragrcv.h"
#include "dynopts.h"
#include "options.h"
#include "profilegui.h"

#define MANAGE_PREVIEW

struct ProfileGUI {
    CappletData capp;
    GtkWidget *widget;
    GtkWidget *ssh_widget;
    char *profile_name;
    gboolean ignore_destroy;
    struct {
        guint sel_by_word : 1;
        guint color_term : 1;
        guint term : 1;
        guint browser : 1;
        guint mailer : 1;
        guint command : 1;
        guint title_string : 1;
        guint ssh_address : 1;
        guint ssh_user : 1;
        guint ssh_options : 1;
        guint win_title : 1;
    } changed;
    GtkWidget *bgimg;
    DragReceiveData *bgimg_drd;
    GtkListStore *list_store;
};

static GHashTable *profilegui_being_edited = NULL;

inline static
GtkWidget *profilegui_widget(ProfileGUI *pg, const char *name)
{
    /* Result will often be cast to a specialised widget so use static
     * cast here to avoid double GObject cast.
     */
    return (GtkWidget *) gtk_builder_get_object(pg->capp.builder, name);
}

static void profilegui_update_from_entry(ProfileGUI * pg, const char *name)
{
    capplet_set_string(pg->capp.options, name,
            gtk_entry_get_text(
                    GTK_ENTRY(gtk_builder_get_object(pg->capp.builder, name))));
}

#define UPDATE_IF(s) if (pg->changed ## s) \
    { \
        profilegui_update_from_entry(pg, # s); \
        pg->changed ## s = 0; \
    }

void profilegui_check_entries_for_changes(ProfileGUI * pg)
{
    UPDATE_IF(sel_by_word)
    UPDATE_IF(color_term)
    UPDATE_IF(term)
    UPDATE_IF(browser)
    UPDATE_IF(mailer)
    UPDATE_IF(command)
    UPDATE_IF(title_string)
    UPDATE_IF(win_title)
}

static void profilegui_set_bgimg_shading(ProfileGUI *pg, gboolean sensitive)
{
    gtk_widget_set_sensitive(profilegui_widget(pg,
                "scroll_background"), sensitive);
    gtk_widget_set_sensitive(pg->bgimg, sensitive);
    gtk_widget_set_sensitive(profilegui_widget(pg, "img_label"),
            sensitive);
}

static void profilegui_set_transparency_shading(ProfileGUI *pg,
        gboolean sensitive)
{
    gtk_widget_set_sensitive(profilegui_widget(pg, "saturation"), sensitive);
    gtk_widget_set_sensitive(profilegui_widget(pg, "transp_l1"), sensitive);
    gtk_widget_set_sensitive(profilegui_widget(pg, "transp_l2"), sensitive);
    gtk_widget_set_sensitive(profilegui_widget(pg, "transp_l3"), sensitive);
}

static void profilegui_set_background_shading(ProfileGUI *pg)
{
    profilegui_set_bgimg_shading(pg, gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(profilegui_widget(pg, "background_type1"))));
    profilegui_set_transparency_shading(pg, !gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(profilegui_widget(pg, "background_type0"))));
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

static void profilegui_check_ssh_entries_for_changes(ProfileGUI * pg)
{
    GtkBuilder *builder = pg->capp.builder;
    
    UPDATE_IF(ssh_address)
    UPDATE_IF(ssh_user)
    UPDATE_IF(ssh_options)
}

/***********************************************************/
/* Handlers */

void on_editable_changed(GtkEditable * editable, ProfileGUI *pg)
{
    const char *n = gtk_buildable_get_name(GTK_BUILDABLE(editable));
#define IF_CHANGED(s) if (!strcmp(n, #s)) pg->changed. ## s = 1

    IF_CHANGED(sel_by_word);
    else IF_CHANGED(color_term);
    else IF_CHANGED(term);
    else IF_CHANGED(browser);
    else IF_CHANGED(mailer);
    else IF_CHANGED(command);
    else IF_CHANGED(title_string);
    else IF_CHANGED(ssh_address);
    else IF_CHANGED(ssh_user);
    else IF_CHANGED(ssh_options);
    else IF_CHANGED(win_title);
}

static void on_ssh_entry_activate(GtkEntry * entry, ProfileGUI * pg)
{
    profilegui_check_ssh_entries_for_changes(pg);
}

static void on_ssh_host_changed(GtkWidget * widget, ProfileGUI *pg)
{
    gtk_entry_set_text(GTK_ENTRY(profilegui_widget(pg, "ssh_host")),
            gtk_entry_get_text(GTK_ENTRY(widget)));
}

static void on_edit_ssh_clicked(GtkWidget * widget, ProfileGUI *pg)
{
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

static void on_Profile_Editor_destroy(GtkWidget * widget, ProfileGUI * pg)
{
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

static void on_Profile_Editor_response(GtkWidget * widget, int response,
        ProfileGUI * pg)
{
    profilegui_delete(pg);
}

static void on_Profile_Editor_close(GtkWidget * widget, ProfileGUI * pg)
{
    profilegui_delete(pg);
}

static void on_font_set(GtkFontButton * fontbutton, ProfileGUI * pg)
{
    capplet_set_string(pg->profile,
        "font", gtk_font_button_get_font_name(fontbutton));
}

static void on_entry_activate(GtkEntry * entry, ProfileGUI * pg)
{
    profilegui_check_entries_for_changes(pg);
}

static void
on_profile_notebook_switch_page(GtkNotebook * notebook, GtkNotebookPage * page,
    guint page_num, ProfileGUI * pg)
{
    profilegui_check_entries_for_changes(pg);
}

static void on_bgtype_toggled(GtkToggleButton *button, ProfileGUI *pg)
{
    if (gtk_toggle_button_get_active(button))
        profilegui_set_background_shading(pg);
    on_radio_toggled(button, pg->profile);
}

static void on_command_toggled(GtkToggleButton *button, ProfileGUI *pg)
{
    profilegui_set_command_shading(pg);
}

static void on_close_buttons_toggled(GtkToggleButton *button, ProfileGUI *pg)
{
    profilegui_set_close_buttons_shading(pg);
}

static void on_cursor_blinks_toggled(GtkToggleButton *button, ProfileGUI *pg)
{
    gboolean state;
    
    if (capplet_ignore_changes)
        return;
    
    state = gtk_toggle_button_get_active(button);
    capplet_set_int(&pg->capp, "cursor_blinks", state);
}

#define PREVIEW_SIZE 160

inline static gboolean pg_image_reportable(ProfileGUI *pg)
{
    return (capplet_which_radio_is_selected(
                profilegui_widget(pg, "background_type1")) == 1);
}

#ifdef MANAGE_PREVIEW
static void
pg_load_preview(GtkFileChooser *chooser, GtkWidget *preview,
        const char *filename, ProfileGUI *pg)
{
    int width, height;
    GdkPixbuf *pixbuf = NULL;
    GError *error = NULL;

    if (filename && gdk_pixbuf_get_file_info(filename, &width, &height))
    {
        if (width <= PREVIEW_SIZE && height <= PREVIEW_SIZE)
        {
            pixbuf = gdk_pixbuf_new_from_file(filename, &error);
        }
        else
        {
            pixbuf = gdk_pixbuf_new_from_file_at_scale(filename,
                    PREVIEW_SIZE, PREVIEW_SIZE, TRUE, &error);
        }
    }
    else if (filename)
    {
        /* Get an appropriate error */
        pixbuf = gdk_pixbuf_new_from_file_at_scale(filename,
                PREVIEW_SIZE, PREVIEW_SIZE, TRUE, &error);
    }
    gtk_image_set_from_pixbuf(GTK_IMAGE(preview), pixbuf);
    
    /* Let's not report file not found any more */
#if 0
    if (!pixbuf && filename && filename[0] &&
            !g_file_test(filename, G_FILE_TEST_IS_DIR))
    {
        if (pg_image_reportable(pg))
        {
            GtkWidget *tl = gtk_widget_get_toplevel(preview);
            if (tl && !GTK_WIDGET_TOPLEVEL(tl))
                tl = NULL;
            dlg_warning(GTK_WINDOW(tl),
                    _("Unable to load background image file '%s': %s"),
                    filename, error ? error->message : _("unknown reason"));
        }
        else
        {
            g_warning(_("Unable to load background image file '%s': %s"),
                    filename, error ? error->message : _("unknown reason"));
        }
    }
#endif

    if (error)
        g_error_free(error);
    gtk_file_chooser_set_preview_widget_active(chooser, pixbuf != NULL);
}

static void on_update_preview(GtkFileChooser *chooser, ProfileGUI *pg)
{
    GtkWidget *preview = gtk_file_chooser_get_preview_widget(chooser);
    char *filename = gtk_file_chooser_get_preview_filename(chooser);
    GdkPixbuf *old_pixbuf = NULL;
    
    if (gtk_image_get_storage_type(GTK_IMAGE(preview)) == GTK_IMAGE_PIXBUF)
        old_pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(preview));
    if (old_pixbuf)
    {
        UNREF_LOG(g_object_unref(old_pixbuf));
    }
    pg_load_preview(chooser, preview, filename, pg);
    g_free(filename);
}
#endif

static void profilegui_setup_file_chooser(ProfileGUI *pg)
{
    GtkFileChooser *chooser =
            GTK_FILE_CHOOSER(gtk_builder_get_object(pg->capp.builder,
                    "background_img"));
                    
    GtkFileFilter *img_filter = gtk_file_filter_new();
    GtkFileFilter *all_filter = gtk_file_filter_new();
#ifdef MANAGE_PREVIEW
    GtkWidget *preview = gtk_image_new();
#endif

    gtk_file_filter_set_name(img_filter, _("Image files"));
    gtk_file_filter_set_name(all_filter, "All filetypes");
    gtk_file_filter_add_pixbuf_formats(img_filter);
    gtk_file_filter_add_pattern (all_filter, "*");
    gtk_file_chooser_add_filter(chooser, img_filter);
    gtk_file_chooser_add_filter(chooser, all_filter);

    gtk_file_chooser_set_use_preview_label(chooser, FALSE);
#ifdef MANAGE_PREVIEW
    gtk_file_chooser_set_preview_widget(chooser, preview);
    g_signal_connect(chooser, "update-preview",
            G_CALLBACK(on_update_preview), pg);
#endif
}

static void profilegui_fill_in_file_chooser(ProfileGUI *pg,
        const char *old_filename)
{
    GtkFileChooser *chooser =
            GTK_FILE_CHOOSER(gtk_builder_get_object(pg->capp.builder,
                    "background_img"));
                    
#ifdef MANAGE_PREVIEW
    GtkWidget *preview = gtk_file_chooser_get_preview_widget(chooser);
#endif

    gtk_file_chooser_set_filename(chooser, old_filename);
#ifdef MANAGE_PREVIEW
    pg_load_preview(chooser, preview, old_filename, pg);
#endif
}

static char *get_bgimg_filename(ProfileGUI *pg, GtkFileChooser *chooser)
{
    char *new_filename = gtk_file_chooser_get_filename(chooser);

    if (!g_file_test(new_filename, G_FILE_TEST_IS_REGULAR))
    {
        g_free(new_filename);
        return NULL;
    }
    capplet_set_string(pg->profile, "background_img", new_filename);
    return new_filename;
}

static void on_bgimg_chosen(GtkFileChooser *chooser, ProfileGUI *pg)
{
    g_free(get_bgimg_filename(pg, chooser));
}

#define DEFAULT_BACKSPACE_BINDING 0
#define DEFAULT_DELETE_BINDING 0

static void on_reset_compat_clicked(GtkButton *button, ProfileGUI *pg)
{
    capplet_set_int(pg->profile, "backspace_binding",
            DEFAULT_BACKSPACE_BINDING);
    capplet_set_combo(&pg->capp, "backspace_binding",
            DEFAULT_BACKSPACE_BINDING);
    capplet_set_int(pg->profile, "delete_binding",
            DEFAULT_DELETE_BINDING);
    capplet_set_combo(&pg->capp, "delete_binding",
            DEFAULT_DELETE_BINDING);
}

static char *pg_get_dragged_in_filename(const char *text, gulong length)
{
    char *first_file = first_file = strstr(text, "\r\n");

    if (first_file)
    {
        return g_strndup(text, first_file - text);
    }
    return g_strndup(text, length);
}

static gboolean bgimg_drag_data_received(GtkWidget *widget,
        const char *text, gulong length, gpointer data)
{
    ProfileGUI *pg = data;
    gboolean result = FALSE;
    char *filename = pg_get_dragged_in_filename(text, length);

    if (gdk_pixbuf_get_file_info(filename, NULL, NULL))
    {
        GtkFileChooser *chooser =
                GTK_FILE_CHOOSER(gtk_builder_get_object(pg->capp.builder,
                        "background_img"));
                    
        gtk_file_chooser_set_filename(chooser, filename);
        /* Don't need to call capplet_set_string because we'll get a signal */
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(profilegui_widget(pg, "background_type1")),
                TRUE);
        result = TRUE;
    }
    g_free(filename);
    return result;
}

static void exit_action_changed(GtkComboBox *combo, ProfileGUI *pg)
{
    gtk_widget_set_sensitive(profilegui_widget(pg, "exit_pause"),
            gtk_combo_box_get_active(combo) != 3);
}

/***********************************************************/


static void profilegui_connect_handlers(ProfileGUI * pg)
{
    gtk_builder_connect_signals(pg->capp.builder, pg);
}

static void profilegui_fill_in_dialog(ProfileGUI * pg)
{
    Options *profile = pg->profile;
    char *val;

    gtk_font_button_set_font_name(
            GTK_FONT_BUTTON(profilegui_widget(pg, "font_button")),
            val = options_lookup_string_with_default(profile,
                "font", "Monospace 10"));
    g_free(val);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(profilegui_widget(pg,
                "show_menubar")), !options_lookup_int_with_default(profile,
            "hide_menubar", FALSE));
    capplet_set_boolean_toggle(&pg->capp, "allow_bold", TRUE);
    capplet_set_boolean_toggle(&pg->capp, "audible_bell", TRUE);
    capplet_set_boolean_toggle(&pg->capp, "visible_bell", FALSE);
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
    capplet_set_text_entry(&pg->capp, "sel_by_word", "-A-Za-z0-9,./?%&#_");
    capplet_set_text_entry(&pg->capp, "color_term", NULL);
    capplet_set_text_entry(&pg->capp, "term", NULL);
    capplet_set_combo(&pg->capp, "tab_pos", 0);
    capplet_set_spin_button(&pg->capp, "init_tabs", 1);
    capplet_set_boolean_toggle(&pg->capp, "wrap_switch_tab", FALSE);
    capplet_set_boolean_toggle(&pg->capp, "tab_close_btn", TRUE);
    capplet_set_boolean_toggle(&pg->capp, "show_tab_status", FALSE);
    capplet_set_boolean_toggle(&pg->capp, "always_show_tabs", TRUE);
    profilegui_set_close_buttons_shading(pg);
    capplet_set_text_entry(&pg->capp, "browser", NULL);
    capplet_set_radio(&pg->capp, "browser_spawn_type", 0);
    capplet_set_text_entry(&pg->capp, "mailer", NULL);
    capplet_set_radio(&pg->capp, "mailer_spawn_type", 0);
    capplet_set_spin_button(&pg->capp, "width", 80);
    capplet_set_spin_button(&pg->capp, "height", 25);
    capplet_set_radio(&pg->capp, "background_type", 0);
    profilegui_set_background_shading(pg);
    capplet_set_float_range(&pg->capp, "saturation", 1.0);
    capplet_set_boolean_toggle(g&pg->capp, scroll_background", TRUE);
    val = options_lookup_string_with_default(profile, "background_img", "");
    profilegui_fill_in_file_chooser(pg, val);
    g_free(val);
    capplet_set_boolean_toggle(&pg->capp, "true_translucence", TRUE);
    capplet_set_radio(&pg->capp, "scrollbar_pos", 1);
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
    capplet_set_boolean_toggle(&pg->capp, "update_records", TRUE);
    capplet_set_boolean_toggle(&pg->capp, "use_custom_command", FALSE);
    capplet_set_text_entry(&pg->capp, "command", NULL);
    capplet_set_combo(&pg->capp, "exit_action", 0);
    capplet_set_spin_button_float(&pg->capp, "exit_pause");
    capplet_set_text_entry(&pg->capp, "title_string", "%s");
    capplet_set_text_entry(&pg->capp, "win_title", "%s");
    exit_action_changed(GTK_COMBO_BOX(profilegui_widget(pg, "exit_action")),
            pg);
    profilegui_set_command_shading(pg);
}

static gboolean page_selected(GtkTreeSelection *selection,
        GtkTreeModel *model, GtkTreePath *path,
        gboolean path_currently_selected, gpointer pg)
{
    GtkTreeIter iter;
    int page;
    
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
            N_("Appearance"), N_("General"), N_("Command"),
            N_("URI handling"), N_("Background"), N_("Scrolling"),
            N_("Keyboard"), N_("Tabs")
    };
    GtkTreeIter iter;
    int n;
    GtkTreeViewColumn *column;
    GtkWidget *tvw;
    GtkTreeView *tv;
    GtkTreeSelection *sel;
    
    pg->list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    for (n = 0; n < 8; ++n)
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
ProfileGUI *profilegui_open(const char *profile_name, GdkScreen *scrn)
{
    static DynamicOptions *profiles = NULL;
    ProfileGUI *pg;
    char *title;
    static const char *obj_names = { "Profile_Editor", "ssh_dialog", NULL };

    if (!profilegui_being_edited)
    {
        profilegui_being_edited = g_hash_table_new_full(g_str_hash, g_str_equal,
                g_free, NULL);
    }
    pg = g_hash_table_lookup(profilegui_being_edited, profile_name);
    if (pg)
    {
        if (scrn)
            gtk_window_set_screen(GTK_WINDOW(pg->dialog), scrn);
        gtk_window_present(GTK_WINDOW(pg->dialog));
        return pg;
    }
    
    if (!profiles)
        profiles = dynamic_options_get("Profiles");

    pg = g_new0(ProfileGUI, 1);
    pg->profile_name = g_strdup(profile_name);
    pg->profile = dynamic_options_lookup_and_ref(profiles, profile_name,
        "roxterm profile");

    pg->capp.builder = gtk_builder_new();
    if (!gtk_builder_add_objects_from_file(pg->capp.builder, 
            capplet_get_glade_filename(), (char **) obj_names, &error))
    {
        g_error(_("Unable to load GTK UI definitions: %s"), error->message);
    }
    pg->dialog = profilegui_widget(pg, "Profile_Editor");
    pg->ssh_dialog = profilegui_widget(pg, "ssh_dialog");
    
    if (scrn)
        gtk_window_set_screen(GTK_WINDOW(pg->dialog), scrn);

    title = g_strdup_printf(_("ROXTerm Profile \"%s\""), profile_name);
    gtk_window_set_title(GTK_WINDOW(pg->dialog), title);
    g_free(title);
    
    profilegui_setup_list_store(pg);

    g_hash_table_insert(profilegui_being_edited, g_strdup(profile_name), pg);

    profilegui_setup_file_chooser(pg);
    pg->bgimg_drd = drag_receive_setup_dest_widget(
            profilegui_widget(pg, "bgimg_drag_target"),
            bgimg_drag_data_received, NULL, pg);
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
    UNREF_LOG(g_object_unref(pg->builder));
    dynamic_options_unref(dynopts, pg->profile_name);
    g_free(pg->profile_name);
    drag_receive_data_delete(pg->bgimg_drd);
    g_free(pg);
    capplet_dec_windows();
    configlet_unlock_profiles();
}

/* vi:set sw=4 ts=4 noet cindent cino= */
