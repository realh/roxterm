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

#include <glade/glade-xml.h>

#include "capplet.h"
#include "configlet.h"
#include "dlg.h"
#include "dragrcv.h"
#include "dynopts.h"
#include "options.h"
#include "profilegui.h"

#define MANAGE_PREVIEW

#define PG_GLADE_CONNECT(pg, glade, h) \
    glade_xml_signal_connect_data ((glade), #h, (GCallback) (h), (pg))

#define PG_CONNECT(pg, h) PG_GLADE_CONNECT((pg), (pg)->glade, h)

#define PG_GLADE_CONNECT_OPTS(pg, glade, h) \
    glade_xml_signal_connect_data ((glade), #h, (GCallback) (h), \
            (pg)->profile)

#define PG_CONNECT_OPTS(pg, h) PG_GLADE_CONNECT_OPTS((pg), (pg)->glade, h)

#define PG_GLADE_EC_CONNECT(pg, glade, n) \
    glade_xml_signal_connect_data ((glade), "on_"  #n "_changed", \
            (GCallback) on_editable_changed, &pg->changed. n)

#define PG_EC_CONNECT(pg, n) PG_GLADE_EC_CONNECT((pg), (pg)->glade, n)

struct ProfileGUI {
    GladeXML *glade;
    GladeXML *ssh_glade;
    GtkWidget *widget;
    char *profile_name;
    Options *profile;
    gboolean ignore_destroy;
    struct {
        char sel_by_word;
        char color_term;
        char term;
        char browser;
        char mailer;
        char command;
        char title_string;
        char ssh_address;
        char ssh_user;
        char ssh_options;
        char win_title;
    } changed;
    GtkWidget *bgimg;
#ifndef HAVE_VTE_TERMINAL_SET_CURSOR_BLINK_MODE
    GtkWidget *cursor_blinks;
#endif
    DragReceiveData *bgimg_drd;
    GtkListStore *list_store;
};

static GHashTable *profilegui_being_edited = NULL;

inline static
GtkWidget *profilegui_widget(ProfileGUI *pg, const char *name)
{
    return glade_xml_get_widget(pg->glade, name);
}

static void update_from_entry(ProfileGUI * pg, GladeXML *glade,
        const char *name)
{
    capplet_set_string(pg->profile, name, gtk_entry_get_text
        (GTK_ENTRY(glade_xml_get_widget(glade, name))));
}

inline static void
profilegui_update_from_entry(ProfileGUI *pg, const char *name)
{
    update_from_entry(pg, pg->glade, name);
}

void profilegui_check_entries_for_changes(ProfileGUI * pg)
{
    if (pg->changed.sel_by_word)
    {
        profilegui_update_from_entry(pg, "sel_by_word");
        pg->changed.sel_by_word = 0;
    }
    if (pg->changed.color_term)
    {
        profilegui_update_from_entry(pg, "color_term");
        pg->changed.color_term = 0;
    }
    if (pg->changed.term)
    {
        profilegui_update_from_entry(pg, "term");
        pg->changed.term = 0;
    }
    if (pg->changed.browser)
    {
        profilegui_update_from_entry(pg, "browser");
        pg->changed.browser = 0;
    }
    if (pg->changed.mailer)
    {
        profilegui_update_from_entry(pg, "mailer");
        pg->changed.mailer = 0;
    }
    if (pg->changed.command)
    {
        profilegui_update_from_entry(pg, "command");
        pg->changed.command = 0;
    }
    if (pg->changed.title_string)
    {
        profilegui_update_from_entry(pg, "title_string");
        pg->changed.title_string = 0;
    }
    if (pg->changed.win_title)
    {
        profilegui_update_from_entry(pg, "win_title");
        pg->changed.win_title = 0;
    }
}

static void profilegui_set_bgimg_shading(ProfileGUI *pg, gboolean sensitive)
{
    gtk_widget_set_sensitive(profilegui_widget(pg,
                "scroll_background"), sensitive);
#ifdef HAVE_GTK_FILE_CHOOSER_BUTTON_NEW
    gtk_widget_set_sensitive(pg->bgimg, sensitive);
#else
    gtk_widget_set_sensitive(profilegui_widget(pg, "bgimg_browse"), sensitive);
    gtk_widget_set_sensitive(profilegui_widget(pg, "background_img"),
            sensitive);
#endif
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
    GladeXML *glade = pg->ssh_glade;
    
    if (pg->changed.ssh_address)
    {
        update_from_entry(pg, glade, "ssh_address");
        pg->changed.ssh_address = 0;
    }
    if (pg->changed.ssh_user)
    {
        update_from_entry(pg, glade, "ssh_user");
        pg->changed.ssh_user = 0;
    }
    if (pg->changed.ssh_options)
    {
        update_from_entry(pg, glade, "ssh_options");
        pg->changed.ssh_options = 0;
    }
}

/***********************************************************/
/* Handlers */

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
    GladeXML *glade = glade_xml_new(capplet_get_glade_filename(),
        "ssh_dialog", NULL);
    GtkWidget *dialog = glade_xml_get_widget(glade, "ssh_dialog");
    
    pg->ssh_glade = glade;
    capplet_inc_windows();
    
    PG_GLADE_EC_CONNECT(pg, glade, ssh_address);
    PG_GLADE_EC_CONNECT(pg, glade, ssh_user);
    PG_GLADE_EC_CONNECT(pg, glade, ssh_options);
    PG_GLADE_CONNECT(pg, glade, on_ssh_entry_activate);
    PG_GLADE_CONNECT(pg, glade, on_ssh_host_changed);
    PG_GLADE_CONNECT_OPTS(pg, glade, on_spin_button_changed);

    capplet_set_text_entry(glade, pg->profile, "ssh_address", "localhost");
    capplet_set_text_entry(glade, pg->profile, "ssh_user", NULL);
    capplet_set_spin_button(glade, pg->profile, "ssh_port", 22);
    capplet_set_text_entry(glade, pg->profile, "ssh_options", NULL);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    
    profilegui_check_ssh_entries_for_changes(pg);
    gtk_widget_destroy(dialog);
    UNREF_LOG(g_object_unref(glade));
    pg->ssh_glade = NULL;
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

#ifndef HAVE_VTE_TERMINAL_SET_CURSOR_BLINK_MODE
static void on_cursor_blinks_toggled(GtkToggleButton *button, Options *opts)
{
    gboolean state;
    
    if (capplet_ignore_changes)
        return;
    
    state = gtk_toggle_button_get_active(button);
    capplet_set_int(opts, "cursor_blinks", state);
}
#endif

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

static void profilegui_setup_file_chooser(ProfileGUI *pg,
        GtkFileChooser *chooser)
{
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
        GtkFileChooser *chooser, const char *old_filename)
{
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

#ifdef HAVE_GTK_FILE_CHOOSER_BUTTON_NEW
static void on_bgimg_chosen(GtkFileChooser *chooser, ProfileGUI *pg)
{
    g_free(get_bgimg_filename(pg, chooser));
}
#else
static void on_bgimg_browse_clicked(GtkButton *button, ProfileGUI *pg)
{
    GtkFileChooser *chooser =
        GTK_FILE_CHOOSER(gtk_file_chooser_dialog_new(_("Background image"),
        GTK_WINDOW(pg->widget),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        _("Use as background"), GTK_RESPONSE_ACCEPT,
        NULL));
    GtkEntry *bgimg_display = GTK_ENTRY(profilegui_widget(pg,
                "background_img"));
    const char *old_filename = gtk_entry_get_text(bgimg_display);
    
    profilegui_setup_file_chooser(pg, chooser);
    profilegui_fill_in_file_chooser(pg, chooser, old_filename);
    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT)
    {
        char *new_filename = get_bgimg_filename(pg, chooser);

        if (new_filename)
        {
            gtk_entry_set_text(bgimg_display, new_filename);
            g_free(new_filename);
        }
    }
    /*
    UNREF_LOG(g_object_unref(img_filter));
    UNREF_LOG(g_object_unref(all_filter));
    */
    gtk_widget_destroy(GTK_WIDGET(chooser));
}
#endif /* HAVE_GTK_FILE_CHOOSER_BUTTON_NEW */

#define DEFAULT_BACKSPACE_BINDING 0
#define DEFAULT_DELETE_BINDING 0

static void on_reset_compat_clicked(GtkButton *button, ProfileGUI *pg)
{
    capplet_set_int(pg->profile, "backspace_binding",
            DEFAULT_BACKSPACE_BINDING);
    capplet_set_combo(pg->glade, pg->profile, "backspace_binding",
            DEFAULT_BACKSPACE_BINDING);
    capplet_set_int(pg->profile, "delete_binding",
            DEFAULT_DELETE_BINDING);
    capplet_set_combo(pg->glade, pg->profile, "delete_binding",
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
#ifdef HAVE_GTK_FILE_CHOOSER_BUTTON_NEW
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(pg->bgimg), filename);
        /* Don't need to call capplet_set_string because we'll get a signal */
#else
        gtk_entry_set_text(GTK_ENTRY(pg->bgimg), filename);
        capplet_set_string(pg->profile, "background_image", filename);
#endif
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
    /* Generic widgets */
    PG_CONNECT_OPTS(pg, on_boolean_toggled);
    PG_CONNECT_OPTS(pg, on_radio_toggled);
    PG_CONNECT_OPTS(pg, on_spin_button_changed);
    PG_CONNECT_OPTS(pg, on_float_spin_button_changed);
    PG_CONNECT_OPTS(pg, on_float_range_changed);
    PG_CONNECT_OPTS(pg, on_combo_changed);
    PG_CONNECT(pg, on_entry_activate);

    /* Editables */
    PG_EC_CONNECT(pg, sel_by_word);
    PG_EC_CONNECT(pg, color_term);
    PG_EC_CONNECT(pg, term);
    PG_EC_CONNECT(pg, command);
    PG_EC_CONNECT(pg, browser);
    PG_EC_CONNECT(pg, mailer);
    PG_EC_CONNECT(pg, title_string);
    PG_EC_CONNECT(pg, win_title);

    /* Misc */
    PG_CONNECT(pg, on_Profile_Editor_destroy);
    PG_CONNECT(pg, on_Profile_Editor_close);
    PG_CONNECT(pg, on_Profile_Editor_response);
    PG_CONNECT(pg, on_profile_notebook_switch_page);

    /* Unique widgets */
    PG_CONNECT(pg, on_font_set);
    PG_CONNECT(pg, on_bgtype_toggled);
    PG_CONNECT(pg, on_command_toggled);
    PG_CONNECT(pg, on_close_buttons_toggled);
#ifdef HAVE_GTK_FILE_CHOOSER_BUTTON_NEW
    g_signal_connect(pg->bgimg, "file-set", G_CALLBACK(on_bgimg_chosen), pg);
#else
    PG_CONNECT(pg, on_bgimg_browse_clicked);
#endif
    PG_CONNECT(pg, on_reset_compat_clicked);
#ifndef HAVE_VTE_TERMINAL_SET_CURSOR_BLINK_MODE
    g_signal_connect(pg->cursor_blinks, "toggled",
            G_CALLBACK(on_cursor_blinks_toggled), pg->profile);
#endif
    g_signal_connect(profilegui_widget(pg, "exit_action"), "changed",
            G_CALLBACK(exit_action_changed), pg);
    PG_CONNECT(pg, on_edit_ssh_clicked);
}

static void profilegui_fill_in_dialog(ProfileGUI * pg)
{
    GladeXML *glade = pg->glade;
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
    capplet_set_boolean_toggle(glade, profile, "allow_bold", TRUE);
    capplet_set_boolean_toggle(glade, profile, "audible_bell", TRUE);
    capplet_set_boolean_toggle(glade, profile, "visible_bell", FALSE);
    capplet_set_boolean_toggle(glade, profile, "bell_highlights_tab", TRUE);
#ifdef HAVE_VTE_TERMINAL_SET_CURSOR_BLINK_MODE
    {
        /* Use legacy cursor_blinks if cursor_blink_mode has not been set */
        int o = options_lookup_int(profile, "cursor_blinks") + 1;
        if (o)
            o ^= 3;
        capplet_set_radio(glade, profile, "cursor_blink_mode", o);
    }
#else
    capplet_ignore_changes = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pg->cursor_blinks),
            options_lookup_int_with_default(profile, "cursor_blinks", FALSE));
    capplet_ignore_changes = FALSE;
#endif
#ifdef HAVE_VTE_TERMINAL_SET_CURSOR_SHAPE
    capplet_set_radio(glade, profile, "cursor_shape", 0);
#endif
    capplet_set_boolean_toggle(glade, profile, "hide_mouse", TRUE);
    capplet_set_text_entry(glade, profile, "sel_by_word", "-A-Za-z0-9,./?%&#_");
    capplet_set_text_entry(glade, profile, "color_term", NULL);
    capplet_set_text_entry(glade, profile, "term", NULL);
    capplet_set_combo(glade, profile, "tab_pos", 0);
    capplet_set_spin_button(glade, profile, "init_tabs", 1);
    capplet_set_boolean_toggle(glade, profile, "wrap_switch_tab", FALSE);
    capplet_set_boolean_toggle(glade, profile, "tab_close_btn", TRUE);
    capplet_set_boolean_toggle(glade, profile, "show_tab_status", FALSE);
    capplet_set_boolean_toggle(glade, profile, "always_show_tabs", TRUE);
    profilegui_set_close_buttons_shading(pg);
    capplet_set_text_entry(glade, profile, "browser", NULL);
    capplet_set_radio(glade, profile, "browser_spawn_type", 0);
    capplet_set_text_entry(glade, profile, "mailer", NULL);
    capplet_set_radio(glade, profile, "mailer_spawn_type", 0);
    capplet_set_spin_button(glade, profile, "width", 80);
    capplet_set_spin_button(glade, profile, "height", 25);
    capplet_set_radio(glade, profile, "background_type", 0);
    profilegui_set_background_shading(pg);
    capplet_set_float_range(glade, profile, "saturation", 1.0);
    capplet_set_boolean_toggle(glade, profile, "scroll_background", TRUE);
#ifdef HAVE_GTK_FILE_CHOOSER_BUTTON_NEW
    val = options_lookup_string_with_default(profile, "background_img", "");
    profilegui_fill_in_file_chooser(pg, GTK_FILE_CHOOSER(pg->bgimg), val);
    g_free(val);
#else
    capplet_set_text_entry(glade, profile, "background_img", NULL);
#endif
    capplet_set_boolean_toggle(glade, profile, "true_translucence", TRUE);
    capplet_set_radio(glade, profile, "scrollbar_pos", 1);
    capplet_set_spin_button(glade, profile, "scrollback_lines", 1000);
    capplet_set_boolean_toggle(glade, profile, "scroll_on_output", FALSE);
    capplet_set_boolean_toggle(glade, profile, "scroll_on_keystroke", FALSE);
    capplet_set_combo(glade, profile, "backspace_binding",
            DEFAULT_BACKSPACE_BINDING);
    capplet_set_combo(glade, profile, "delete_binding",
            DEFAULT_DELETE_BINDING);
    capplet_set_boolean_toggle(glade, profile, "disable_menu_access", FALSE);
    capplet_set_boolean_toggle(glade, profile, "disable_menu_shortcuts", FALSE);
    capplet_set_boolean_toggle(glade, profile, "disable_tab_menu_shortcuts",
            FALSE);
    capplet_ignore_changes = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(glade,
                "use_default_shell")), TRUE);
    gtk_entry_set_text(GTK_ENTRY(glade_xml_get_widget(glade, "ssh_host")),
            options_lookup_string_with_default(profile, "ssh_address",
                    "localhost"));
    capplet_ignore_changes = FALSE;
    capplet_set_boolean_toggle(glade, profile, "use_ssh", FALSE);
    capplet_set_boolean_toggle(glade, profile, "login_shell", FALSE);
    capplet_set_boolean_toggle(glade, profile, "update_records", TRUE);
    capplet_set_boolean_toggle(glade, profile, "use_custom_command", FALSE);
    capplet_set_text_entry(glade, profile, "command", NULL);
    capplet_set_combo(glade, profile, "exit_action", 0);
    capplet_set_spin_button_float(glade, profile, "exit_pause");
    capplet_set_text_entry(glade, profile, "title_string", "%s");
    capplet_set_text_entry(glade, profile, "win_title", "%s");
    exit_action_changed(GTK_COMBO_BOX(profilegui_widget(pg, "exit_action")),
            pg);
    profilegui_set_command_shading(pg);
}

#ifdef HAVE_GTK_FILE_CHOOSER_BUTTON_NEW
/* For backwards compatibility with older GTKs the glade file uses a text entry
 * and a button for choosing a background image file. This function replaces
 * them with a GtkFileChooserButton. */
static void profilegui_replace_file_chooser_widgets(ProfileGUI *pg)
{
    GtkWidget *hbox = profilegui_widget(pg, "background_img_box");
    GtkWidget *entry = profilegui_widget(pg, "background_img");
    GtkWidget *button = profilegui_widget(pg, "bgimg_browse");

    gtk_widget_destroy(entry);
    gtk_widget_destroy(button);
    pg->bgimg = gtk_file_chooser_button_new(_("Background image"),
            GTK_FILE_CHOOSER_ACTION_OPEN);
    profilegui_setup_file_chooser(pg, GTK_FILE_CHOOSER(pg->bgimg));
    gtk_widget_show(pg->bgimg);
    gtk_box_pack_start(GTK_BOX(hbox), pg->bgimg, TRUE, TRUE, 0);
}
#endif

#if !defined HAVE_VTE_TERMINAL_SET_CURSOR_BLINK_MODE || \
        !defined HAVE_VTE_TERMINAL_SET_CURSOR_SHAPE
static void profilegui_destroy_radio_group_and_label(ProfileGUI *pg,
        const char *group_name)
{
    int n;
    char *name = g_strdup_printf("%s_label", group_name);
    
    gtk_widget_destroy(profilegui_widget(pg, name));
    g_free(name);
    for (n = 0; n < 3; ++n)
    {
        name = g_strdup_printf("%s%d", group_name, n);
        gtk_widget_destroy(profilegui_widget(pg, name));
        g_free(name);
    }
}
#endif

#ifndef HAVE_VTE_TERMINAL_SET_CURSOR_BLINK_MODE
static void profilegui_replace_blink_mode_widgets(ProfileGUI *pg)
{
    GtkWidget *table = profilegui_widget(pg, "general_table");
    
    profilegui_destroy_radio_group_and_label(pg, "cursor_blink_mode");
    pg->cursor_blinks = gtk_check_button_new_with_mnemonic(_("Cursor blin_ks"));
    gtk_widget_show(pg->cursor_blinks);
    gtk_table_attach_defaults(GTK_TABLE(table), pg->cursor_blinks, 2, 3, 2, 3);
}
#endif

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

    if (!profilegui_being_edited)
    {
        profilegui_being_edited = g_hash_table_new_full(g_str_hash, g_str_equal,
                g_free, NULL);
    }
    pg = g_hash_table_lookup(profilegui_being_edited, profile_name);
    if (pg)
    {
        if (scrn)
            gtk_window_set_screen(GTK_WINDOW(pg->widget), scrn);
        gtk_window_present(GTK_WINDOW(pg->widget));
        return pg;
    }
    
    if (!profiles)
        profiles = dynamic_options_get("Profiles");

    pg = g_new0(ProfileGUI, 1);
    pg->profile_name = g_strdup(profile_name);
    pg->profile = dynamic_options_lookup_and_ref(profiles, profile_name,
        "roxterm profile");

    pg->glade = glade_xml_new(capplet_get_glade_filename(),
        "Profile_Editor", NULL);
    g_assert(pg->glade);
    pg->widget = profilegui_widget(pg, "Profile_Editor");
    if (scrn)
        gtk_window_set_screen(GTK_WINDOW(pg->widget), scrn);

    title = g_strdup_printf(_("ROXTerm Profile \"%s\""), profile_name);
    gtk_window_set_title(GTK_WINDOW(pg->widget), title);
    g_free(title);
    
    profilegui_setup_list_store(pg);

    g_hash_table_insert(profilegui_being_edited, g_strdup(profile_name), pg);

#ifndef HAVE_VTE_TERMINAL_SET_CURSOR_BLINK_MODE
    profilegui_replace_blink_mode_widgets(pg);
#endif
#ifndef HAVE_VTE_TERMINAL_SET_CURSOR_SHAPE
    profilegui_destroy_radio_group_and_label(pg, "cursor_shape");
#endif
#ifdef HAVE_GTK_FILE_CHOOSER_BUTTON_NEW
    profilegui_replace_file_chooser_widgets(pg);
#else
    pg->bgimg = profilegui_widget(pg, "background_img");
#endif
    pg->bgimg_drd = drag_receive_setup_dest_widget(
            profilegui_widget(pg, "bgimg_drag_target"),
            bgimg_drag_data_received, NULL, pg);
    profilegui_fill_in_dialog(pg);
    profilegui_connect_handlers(pg);

    gtk_widget_show(pg->widget);

    capplet_inc_windows();
    configlet_lock_profiles();

    return pg;
}

void profilegui_delete(ProfileGUI * pg)
{
    static DynamicOptions *dynopts = NULL;

    if (!dynopts)
        dynopts = dynamic_options_get("Profiles");

    profilegui_check_entries_for_changes(pg);
    if (!g_hash_table_remove(profilegui_being_edited, pg->profile_name))
    {
        g_critical(_("Deleting edited profile that wasn't in list of profiles "
                    "being edited"));
    }
    if (!pg->ignore_destroy)
    {
        pg->ignore_destroy = TRUE;
        gtk_widget_destroy(profilegui_widget(pg, "Profile_Editor"));
    }
    UNREF_LOG(g_object_unref(pg->list_store));
    UNREF_LOG(g_object_unref(pg->glade));
    dynamic_options_unref(dynopts, pg->profile_name);
    g_free(pg->profile_name);
    drag_receive_data_delete(pg->bgimg_drd);
    g_free(pg);
    capplet_dec_windows();
    configlet_unlock_profiles();
}

/* vi:set sw=4 ts=4 noet cindent cino= */
