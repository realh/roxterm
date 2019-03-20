/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2019 Tony Houghton <h@realh.co.uk>

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

#include "config.h"
#include "roxterm-application.h"
#include "roxterm-launch-params.h"

#include <string.h>

struct _RoxtermApplication {
    GtkApplication parent_instance;
    GtkBuilder *builder;
};

G_DEFINE_TYPE(RoxtermApplication, roxterm_application, GTK_TYPE_APPLICATION);

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
    roxterm_application_new_window(app, NULL, NULL);
}

static void on_app_new_vim(UNUSED GSimpleAction *action, UNUSED GVariant *param,
        UNUSED gpointer app)
{
}

static void on_app_quit(UNUSED GSimpleAction *action, UNUSED GVariant *param,
        gpointer app)
{
    g_application_quit(G_APPLICATION(app));
}

static GActionEntry roxterm_app_actions[] = {
    { "about", on_app_about, NULL, NULL, NULL },
    { "prefs", on_app_prefs, NULL, NULL, NULL },
    { "new-win", on_app_new_win, NULL, NULL, NULL },
    { "new-vim", on_app_new_vim, NULL, NULL, NULL },
    { "quit", on_app_quit, NULL, NULL, NULL },
};

static void roxterm_application_window_removed(GtkApplication *app,
        GtkWindow *win)
{
    GTK_APPLICATION_CLASS(roxterm_application_parent_class)
            ->window_removed(app, win);
    if (!gtk_application_get_windows(app))
        g_application_quit(G_APPLICATION(app));
}

static void roxterm_application_startup(GApplication *gapp)
{
    G_APPLICATION_CLASS(roxterm_application_parent_class)->startup(gapp);
    GtkApplication *gtkapp = GTK_APPLICATION(gapp);
    RoxtermApplication *self = ROXTERM_APPLICATION(gapp);
    self->builder = gtk_builder_new_from_resource(ROXTERM_RESOURCE_PATH
             "menus.ui");
    GMenuModel *app_menu
        = G_MENU_MODEL(gtk_builder_get_object(self->builder, "app-menu"));
    gtk_application_set_app_menu(gtkapp, app_menu);
    g_action_map_add_action_entries(G_ACTION_MAP(gapp),
            roxterm_app_actions, G_N_ELEMENTS(roxterm_app_actions), gapp);
}

static gint roxterm_application_command_line(GApplication *gapp,
        GApplicationCommandLine *cmd_line)
{
    G_APPLICATION_CLASS(roxterm_application_parent_class)->command_line(gapp,
            cmd_line);
    RoxtermApplication *self = ROXTERM_APPLICATION(gapp);
    GError *error = NULL;
    RoxtermLaunchParams *lp
            = roxterm_launch_params_new_from_command_line(cmd_line, &error);
    if (!lp)
    {
        g_critical("Error parsing command-line options: %s", error->message);
        g_error_free(error);
        return 1;
    }
    for (GList *link = lp->windows; link; link = g_list_next(link))
    {
        roxterm_application_new_window(self, lp, link->data);
    }
    return 0;
}

static void roxterm_application_class_init(RoxtermApplicationClass *klass)
{
    GtkApplicationClass *gtkapp_klass = GTK_APPLICATION_CLASS(klass);
    gtkapp_klass->window_removed = roxterm_application_window_removed;
    GApplicationClass *gapp_klass = G_APPLICATION_CLASS(klass);
    gapp_klass->startup = roxterm_application_startup;
    gapp_klass->command_line = roxterm_application_command_line;
}

static void roxterm_application_init(UNUSED RoxtermApplication *self)
{
}

RoxtermApplication *roxterm_application_new(void)
{
    GObject *obj = g_object_new(ROXTERM_TYPE_APPLICATION,
            "application-id", ROXTERM_APPLICATION_ID,
            "flags", G_APPLICATION_HANDLES_COMMAND_LINE
                | G_APPLICATION_SEND_ENVIRONMENT,
            NULL);
    return ROXTERM_APPLICATION(obj);
}

RoxtermWindow *roxterm_application_new_window(RoxtermApplication *self,
        RoxtermLaunchParams *lp, RoxtermWindowLaunchParams *wp)
{
    GtkApplication *gapp = GTK_APPLICATION(self);
    RoxtermWindow *win = roxterm_window_new(self->builder);
    GtkWindow *gwin = GTK_WINDOW(win);
    roxterm_window_apply_launch_params(win, lp, wp);
    if (wp)
    {
        for (GList *link = wp->tabs; link; link = g_list_next(link))
        {
            roxterm_window_new_tab(win, link->data, -1);
        }
    }
    else
    {
        roxterm_window_new_tab(win, NULL, -1);
    }
    gtk_application_add_window(gapp, gwin);
    multitext_window_set_initial_size(MULTITEXT_WINDOW(win));
    gtk_widget_show_all(GTK_WIDGET(win));
    return win;
}

static int roxterm_application_parse_options_early(int argc, char **argv)
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

GtkBuilder *roxterm_application_get_builder(RoxtermApplication *self)
{
    return self->builder;
}

int main(int argc, char **argv)
{
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
            return roxterm_application_parse_options_early(argc, argv);
        }
        else if (!strcmp(s, "--execute") || !strcmp(s, "-e"))
        {
            break;
        }
    }
    RoxtermApplication *app = roxterm_application_new();
    int result = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return result;
}
