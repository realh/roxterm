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

#include "roxterm-application.h"

#define ROXTERM_APPLICATION_ID "uk.co.realh.roxterm4"

struct _RoxtermApplication {
    GtkApplication parent_instance;
};

G_DEFINE_TYPE(RoxtermApplication, roxterm_application, GTK_TYPE_APPLICATION);

static void roxterm_application_window_removed(GtkApplication *app,
        GtkWindow *win)
{
    GTK_APPLICATION_CLASS(roxterm_application_parent_class)
            ->window_removed(app, win);
    if (!gtk_application_get_windows(app))
        g_application_quit(G_APPLICATION(app));
}

static gint roxterm_application_command_line(GApplication *gapp,
        GApplicationCommandLine *cmd_line)
{
    (void) cmd_line;
    RoxtermApplication *self = ROXTERM_APPLICATION(gapp);
    roxterm_application_new_window(self);
    return 0;
}

static void roxterm_application_class_init(RoxtermApplicationClass *klass)
{
    GtkApplicationClass *gtkapp_klass = GTK_APPLICATION_CLASS(klass);
    gtkapp_klass->window_removed = roxterm_application_window_removed;
    GApplicationClass *gapp_klass = G_APPLICATION_CLASS(klass);
    gapp_klass->command_line = roxterm_application_command_line;
}

static void roxterm_application_init(RoxtermApplication *self)
{
    (void) self;
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

RoxtermWindow *roxterm_application_new_window(RoxtermApplication *app)
{
    RoxtermWindow *win = roxterm_window_new(app);
    GtkWindow *gwin = GTK_WINDOW(win);
    gtk_application_add_window(GTK_APPLICATION(app), gwin);
    gtk_widget_show_all(GTK_WIDGET(win));
    gtk_window_present(gwin);
    return win;
}

int main(int argc, char **argv)
{
    RoxtermApplication *app = roxterm_application_new();
    int result = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return result;
}
