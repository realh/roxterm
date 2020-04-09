/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2015-2020 Tony Houghton <h@realh.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "configlet.h"
#include "colourgui.h"
#include "dlg.h"
#include "dynopts.h"
#include "globalopts.h"
#include "launch-params.h"
#include "profilegui.h"
#include "dlg.h"
#include "roxterm.h"
#include "roxterm-app.h"
#include "roxterm-dirs.h"
#include "shortcuts.h"

#include <string.h>

static DynamicOptions *roxterm_profiles = NULL;

DynamicOptions *roxterm_get_profiles(void)
{
    if (!roxterm_profiles)
        roxterm_profiles = dynamic_options_get("Profiles");
    return roxterm_profiles;
}


struct _RoxtermApp {
    GtkApplication parent_instance;
};

G_DEFINE_TYPE(RoxtermApp, roxterm_app, GTK_TYPE_APPLICATION);

#if 0
static void on_app_about(UNUSED GSimpleAction *action, UNUSED GVariant *param,
        UNUSED gpointer app)
{
}

static void on_app_prefs(UNUSED GSimpleAction *action, UNUSED GVariant *param,
        UNUSED gpointer app)
{
}

static void on_app_new_win(UNUSED GSimpleAction *action, UNUSED GVariant *param,
        gpointer app)
{
    roxterm_app_new_window(app, NULL, NULL);
}

static void on_app_quit(UNUSED GSimpleAction *action, UNUSED GVariant *param,
        gpointer app)
{
    g_application_quit(G_APPLICATION(app));
}

#define GACTIONENTRY_PADDING {0, 0, 0}

static GActionEntry roxterm_app_actions[] = {
    { "about", on_app_about, NULL, NULL, NULL, GACTIONENTRY_PADDING },
    { "prefs", on_app_prefs, NULL, NULL, NULL, GACTIONENTRY_PADDING  },
    { "new-win", on_app_new_win, NULL, NULL, NULL, GACTIONENTRY_PADDING },
    { "quit", on_app_quit, NULL, NULL, NULL, GACTIONENTRY_PADDING },
};
#endif

static void roxterm_app_startup(GApplication *gapp)
{
    CHAIN_UP(G_APPLICATION_CLASS, roxterm_app_parent_class, startup)(gapp);
    global_options_init();
    roxterm_init();
    /*
    GtkApplication *gtkapp = GTK_APPLICATION(gapp);
    RoxtermApp *self = ROXTERM_APP(gapp);
    self->builder = gtk_builder_new_from_resource(ROXTERM_RESOURCE_PATH
             "menus.ui");
    GMenuModel *app_menu
        = G_MENU_MODEL(gtk_builder_get_object(self->builder, "app-menu"));
    gtk_app_set_app_menu(gtkapp, app_menu);
    g_action_map_add_action_entries(G_ACTION_MAP(gapp),
            roxterm_app_actions, G_N_ELEMENTS(roxterm_app_actions), gapp);
     */
}

static void roxterm_app_launch_window(UNUSED RoxtermApp *app,
        RoxtermLaunchParams *lp, RoxtermWindowLaunchParams *wlp)
{
    MultiWin *win = NULL;
    ROXTermData *partner = NULL;
    gboolean free_partner = FALSE;
    gboolean old_win = FALSE;
    char *shortcut_scheme =
        global_options_lookup_string_with_default("shortcut_scheme",
                "Default");
    Options *shortcuts = shortcuts_open(shortcut_scheme, FALSE);
    if (wlp->implicit)
    {
        char *wtitle = wlp->window_title;
        MultiWin *best_inactive = NULL;
        GList *link;
        // TODO: Eventually we should use gtk_application_get_windows()
        // instead of multi_win_all
        for (link = multi_win_all; link; link = g_list_next(link))
        {
            GtkWidget *w;
            gboolean title_ok;

            win = link->data;
            w = multi_win_get_widget(win);
            title_ok = !wtitle ||
                    !g_strcmp0(multi_win_get_title_template(win), wtitle);
            if (gtk_window_is_active(GTK_WINDOW(w)) && title_ok)
            {
                break;
            }
            if (title_ok && !best_inactive)
                best_inactive = win;
            win = NULL;
        }
        if (!win)
        {
            if (best_inactive)
                win = best_inactive;
        }
        partner = win ? multi_win_get_user_data_for_current_tab(win) : NULL;
        if (win)
            old_win = TRUE;
    }
    gboolean size_on_cli = FALSE;
    for (GList *link = wlp->tabs; link; link = g_list_next(link))
    {
        RoxtermTabLaunchParams *tlp = link->data;
        const char *profile_name = tlp->profile_name ?
            tlp->profile_name : "Default";
        const char *colour_scheme_name = tlp->colour_scheme_name ?
            tlp->colour_scheme_name : "Default";
        Options *profile = dynamic_options_lookup_and_ref(
                roxterm_get_profiles(), profile_name);
        char *geom = wlp->geometry_str;
        gboolean maximise = wlp->maximized ||
            options_lookup_int_with_default(profile, "maximise", 0);
        ROXTermData *roxterm = roxterm_data_new(
                global_options_lookup_double("zoom"),
                tlp->directory, profile_name, profile,
                maximise, colour_scheme_name,
                &geom, &size_on_cli, lp->env);
        if (partner)
        {
            roxterm_data_init_from_partner(roxterm, partner);
            GtkWidget *gwin = multi_win_get_widget(win);
            multi_tab_new(win, roxterm);
            if (gtk_widget_get_visible(gwin))
                gtk_window_present(GTK_WINDOW(gwin));
            // multi_* create functions only use ROXTermData as a template
            // so we need to free it
            roxterm_data_delete(roxterm);
        }
        else
        {
            gboolean borderless = wlp->borderless ||
                options_lookup_int_with_default(profile, "borderless", 0);
            gboolean show_add_tab_btn = options_lookup_int_with_default(profile,
                "show_add_tab_btn", 1);
            gboolean always_show_tabs = roxterm_get_always_show_tabs(roxterm);
            GtkPositionType tab_pos = roxterm_get_tab_pos(roxterm);
            int zoom_index = roxterm_data_get_zoom_index(roxterm);
            if (wlp->fullscreen || options_lookup_int_with_default(profile,
                        "full_screen", 0))
            {
                win = multi_win_new_fullscreen(shortcuts, zoom_index, roxterm,
                    tab_pos, borderless, always_show_tabs, show_add_tab_btn);
            }
            else if (maximise)
            {
                win = multi_win_new_maximised(shortcuts, zoom_index, roxterm,
                    tab_pos, borderless, always_show_tabs, show_add_tab_btn);
            }
            else
            {
                if (!size_on_cli)
                {
                    int columns, rows;
                    roxterm_init_default_size(roxterm, &columns, &rows);
                    geom = g_strdup_printf("%dx%d", columns, rows);
                }
                win = multi_win_new_with_geom(shortcuts,
                        zoom_index, roxterm, geom, tab_pos, borderless,
                        always_show_tabs, show_add_tab_btn);
                if (!size_on_cli)
                    g_free(geom);
            }
            if (!partner)
            {
                partner = roxterm;
                free_partner = TRUE;
            }
            else
            {
                // multi_* create functions only use ROXTermData as a template
                // so we need to free it
                roxterm_data_delete(roxterm);
            }
        }
    }
    if (free_partner)
        roxterm_data_delete(partner);
    g_free(shortcut_scheme);
}

static void roxterm_app_launch(RoxtermApp *app, RoxtermLaunchParams *lp)
{
    // TODO: session
    for (GList *link = lp->windows; link; link = g_list_next(link))
    {
        roxterm_app_launch_window(app, lp, link->data);
    }
}

static gint roxterm_app_command_line(GApplication *gapp,
        GApplicationCommandLine *cmd_line)
{
    CHAIN_UP(G_APPLICATION_CLASS, roxterm_app_parent_class,
            command_line)(gapp, cmd_line);
    RoxtermApp *self = ROXTERM_APP(gapp);
    GError *error = NULL;
    RoxtermLaunchParams *lp
            = roxterm_launch_params_new_from_command_line(cmd_line, &error);
    if (!lp)
    {
        g_critical("Error parsing command-line options: %s", error->message);
        g_error_free(error);
        return 1;
    }
    roxterm_app_launch(self, lp);
    roxterm_launch_params_unref(lp);
    return 0;
}

static void roxterm_app_class_init(RoxtermAppClass *klass)
{
    GApplicationClass *gapp_klass = G_APPLICATION_CLASS(klass);
    gapp_klass->startup = roxterm_app_startup;
    gapp_klass->command_line = roxterm_app_command_line;
}

static void roxterm_app_init(UNUSED RoxtermApp *self)
{
}

RoxtermApp *roxterm_app_get(void)
{
    static RoxtermApp *app = NULL;
    if (!app)
    {
        GObject *obj = g_object_new(ROXTERM_TYPE_APP,
                "application-id", ROXTERM_APP_ID,
                "flags", G_APPLICATION_HANDLES_COMMAND_LINE
                    | G_APPLICATION_SEND_ENVIRONMENT,
                NULL);
        app = ROXTERM_APP(obj);
    }
    return app;
}

static int roxterm_app_parse_options_early(int argc, char **argv)
{
    // Not sure if it's safe to manipulate argv as passed to main, so make
    // copy to be on the safe side
    char **orig_argv = argv;
    argv = g_new(char *, argc + 1);
    for (int n = 0; n < argc; ++n)
        argv[n] = g_strdup(orig_argv[n]);
    argv[argc] = NULL;
    GError *error = NULL;
    if (!roxterm_launch_params_preparse_argv_execute(NULL,
            &argc, &argv, &error))
    {
        g_error("%s", error->message);
        g_error_free(error);
        g_strfreev(argv);
        return 1;
    }
    GOptionContext *octx = roxterm_launch_params_get_option_context(NULL);
    gboolean result = g_option_context_parse(octx, &argc, &argv, &error);
    if (!result)
    {
        g_error("Option parsing error: %s", error->message);
        g_error_free(error);
    }
    g_option_group_unref(g_option_context_get_main_group(octx));
    g_option_context_free(octx);
    g_strfreev(argv);
    return result;
}

static void roxterm_app_add_window_if_new(RoxtermApp *app, GtkWindow *window)
{
    GtkApplication *gapp = GTK_APPLICATION(app);
    GList *windows = gtk_application_get_windows(gapp);
    for (GList *link = windows; link; link = g_list_next(link))
    {
        if (window == link->data)
            return;
    }
    gtk_application_add_window(gapp, window);
}

void roxterm_app_open_configlet(RoxtermApp *app)
{
    ConfigletData *cg = configlet_open();
    if (cg)
        roxterm_app_add_window_if_new(app, configlet_get_dialog(cg));
}

void roxterm_app_edit_profile(RoxtermApp *app, const char *profile_name)
{
    ProfileGUI *pg = profilegui_open(profile_name);
    if (pg)
        roxterm_app_add_window_if_new(app, profile_gui_get_dialog(pg));
}

void roxterm_app_edit_colours(RoxtermApp *app, const char *scheme_name)
{
    ColourGUI *cg = colourgui_open(scheme_name);
    if (cg)
        roxterm_app_add_window_if_new(app, colourgui_get_dialog(cg));
}

void roxterm_app_edit_shortcuts(UNUSED RoxtermApp *app, const char *scheme_name,
        GtkWindow *parent_window)
{
    shortcuts_edit(parent_window, scheme_name);
}

int main(int argc, char **argv)
{
    roxterm_init_app_dir(argc, argv);
    roxterm_init_bin_dir(argv[0]);
    // First check whether args contain --help or similar and, if so, process
    // them now and exit, so we respond to --help etc correctly in the local
    // instance. Otherwise all options are passed to primary instance, because
    // we now have repeatable options which is too sophisticated for
    // GApplication to handle the conventional way.
    for (int n = 1; n < argc; ++n)
    {
        const char *s = argv[n];
        if (!strcmp(s, "-h") || !strcmp(s, "-?")
                || g_str_has_prefix(s, "--help"))
        {
            return roxterm_app_parse_options_early(argc, argv);
        }
        else if (!strcmp(s, "--execute") || !strcmp(s, "-e"))
        {
            // if --help comes after --execute it should be passed on to
            // child command, not handled by us
            break;
        }
    }
    RoxtermApp *app = roxterm_app_get();
    int result = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return result;
}
