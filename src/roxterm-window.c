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
#include "roxterm-header-bar.h"
#include "roxterm-window.h"

struct _RoxtermWindow {
    GtkApplicationWindow parent_instance;
    RoxtermLaunchParams *lp;
    RoxtermWindowLaunchParams *wp;
};

G_DEFINE_TYPE(RoxtermWindow, roxterm_window, MULTITEXT_TYPE_WINDOW);

static void roxterm_window_dispose(GObject *obj)
{
    RoxtermWindow *self = ROXTERM_WINDOW(obj);
    if (self->wp)
    {
        roxterm_window_launch_params_unref(self->wp);
        self->wp = NULL;
    }
    if (self->lp)
    {
        roxterm_launch_params_unref(self->lp);
        self->lp = NULL;
    }
}

static void roxterm_window_class_init(UNUSED RoxtermWindowClass *klass)
{
}

static void roxterm_window_init(RoxtermWindow *self)
{
    GtkWindow *gwin = GTK_WINDOW(self);
    RoxtermHeaderBar *header = roxterm_header_bar_new();
    gtk_window_set_titlebar(gwin, GTK_WIDGET(header));
}

RoxtermWindow *roxterm_window_new(RoxtermApplication *app)
{
    GObject *obj = g_object_new(ROXTERM_TYPE_WINDOW,
            "application", app,
            NULL);
    RoxtermWindow *self = ROXTERM_WINDOW(obj);
    return self;
}

void roxterm_window_apply_launch_params(RoxtermWindow *self,
        RoxtermLaunchParams *lp, RoxtermWindowLaunchParams *wp)
{
    self->lp = lp ? roxterm_launch_params_ref(lp) : NULL;
    self->wp = wp ? roxterm_window_launch_params_ref(wp) : NULL;
}

static void roxterm_window_spawn_callback(VteTerminal *vte,
        GPid pid, GError *error, gpointer handle)
{
    //RoxtermWindow *self = handle;
    MultitextWindow *mwin = handle;
    if (pid == -1)
    {
        // TODO: GUI dialog
        g_critical("Child command failed to run: %s", error->message);
        // error is implicitly transfer-none according to vte's gir file
        gtk_container_remove(
                GTK_CONTAINER(multitext_window_get_notebook(mwin)),
                GTK_WIDGET(vte));
        // TODO: Verify that this also destroys the tab's label
    }
}

RoxtermVte *roxterm_window_new_tab(RoxtermWindow *self,
        RoxtermTabLaunchParams *tp, int index)
{
    RoxtermVte *rvt = roxterm_vte_new();
    roxterm_vte_apply_launch_params(rvt, self->lp, self->wp, tp);
    MultitextWindow *mwin = MULTITEXT_WINDOW(self);
    GtkNotebook *gnb = GTK_NOTEBOOK(multitext_window_get_notebook(mwin));
    gtk_notebook_insert_page(gnb, GTK_WIDGET(rvt), NULL, index);
    roxterm_vte_spawn(rvt, roxterm_window_spawn_callback, self);
    return rvt;
}
