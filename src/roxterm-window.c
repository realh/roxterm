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

// application is not a property of the parent class, so we need to implement
// it as one to be able to set it during construction
enum {
    PROP_APP = 1,
    N_PROPS
};

static GParamSpec *roxterm_window_props[N_PROPS] = {NULL};

static void roxterm_window_set_property(GObject *obj, guint prop_id,
        const GValue *value, GParamSpec *pspec)
{
    GtkWindow *win = GTK_WINDOW(obj);
    switch (prop_id)
    {
        case PROP_APP:
            gtk_window_set_application(win, g_value_get_object(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

static void roxterm_window_get_property(GObject *obj, guint prop_id,
        GValue *value, GParamSpec *pspec)
{
    GtkWindow *win = GTK_WINDOW(obj);
    switch (prop_id)
    {
        case PROP_APP:
            g_value_set_object(value, gtk_window_get_application(win));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

static void on_win_new_tab(UNUSED GSimpleAction *action, UNUSED GVariant *param,
        gpointer win)
{
    roxterm_window_new_tab(win, NULL, -1);
}

static void on_win_new_vim_tab(UNUSED GSimpleAction *action,
        UNUSED GVariant *param, UNUSED gpointer app)
{
}

static GActionEntry roxterm_win_actions[] = {
    { "new-tab", on_win_new_tab, NULL, NULL, NULL },
    { "new-vim-tab", on_win_new_vim_tab, NULL, NULL, NULL },
};

// This has to be called at end of construction because it reads the app
// which is set as a property
static void roxterm_window_add_tab_bar_buttons(RoxtermWindow *self)
{
    RoxtermApplication *app
        = ROXTERM_APPLICATION(gtk_window_get_application(GTK_WINDOW(self)));
    GtkBuilder *builder = roxterm_application_get_builder(app);
    GtkWidget *menu_btn_w = gtk_menu_button_new();
    GtkMenuButton *menu_btn = GTK_MENU_BUTTON(menu_btn_w);
    GMenuModel *menu
        = G_MENU_MODEL(gtk_builder_get_object(builder, "tab-bar-menu"));
    gtk_menu_button_set_menu_model(menu_btn, menu);
    GtkWidget *nt_btn_w = gtk_button_new();
    GtkButton *nt_btn = GTK_BUTTON(nt_btn_w);
    GtkActionable *nt_btn_a = GTK_ACTIONABLE(nt_btn);
    GtkWidget *img = gtk_image_new_from_icon_name("tab-new-symbolic",
                GTK_ICON_SIZE_MENU);
    gtk_button_set_image(nt_btn, img);
    gtk_actionable_set_action_name(nt_btn_a, "win.new-tab");
    GtkWidget *box_w = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkBox *box = GTK_BOX(box_w);
    gtk_box_pack_start(box, nt_btn_w, FALSE, FALSE, 0);
    gtk_box_pack_start(box, menu_btn_w, FALSE, FALSE, 0);
    GtkNotebook *nb
        = GTK_NOTEBOOK(multitext_window_get_notebook(MULTITEXT_WINDOW(self)));
    gtk_widget_show_all(box_w);
    gtk_notebook_set_action_widget(nb, box_w, GTK_PACK_END);
}

static void roxterm_window_constructed(GObject *obj)
{
    RoxtermWindow *self = ROXTERM_WINDOW(obj);
    roxterm_window_add_tab_bar_buttons(self);
}

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

static void roxterm_window_class_init(RoxtermWindowClass *klass)
{
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    oklass->constructed = roxterm_window_constructed;
    oklass->dispose = roxterm_window_dispose;
    oklass->set_property = roxterm_window_set_property;
    oklass->get_property = roxterm_window_get_property;
    roxterm_window_props[PROP_APP] =
            g_param_spec_object("application", "application",
            "RoxtermApplication", ROXTERM_TYPE_APPLICATION,
            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
    g_object_class_install_properties(oklass, N_PROPS, roxterm_window_props);
}

static void roxterm_window_init(RoxtermWindow *self)
{
    g_action_map_add_action_entries(G_ACTION_MAP(self),
            roxterm_win_actions, G_N_ELEMENTS(roxterm_win_actions), self);
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
    GtkScrollable *scrollable = GTK_SCROLLABLE(rvt);
    GtkWidget *vpw = gtk_scrolled_window_new(
            gtk_scrollable_get_hadjustment(scrollable), 
            gtk_scrollable_get_vadjustment(scrollable));
    GtkScrolledWindow *viewport = GTK_SCROLLED_WINDOW(vpw);
    gtk_scrolled_window_set_policy(viewport, GTK_POLICY_NEVER,
            GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_overlay_scrolling(viewport, TRUE);
    gtk_container_add(GTK_CONTAINER(vpw), GTK_WIDGET(rvt));
    MultitextWindow *mwin = MULTITEXT_WINDOW(self);
    GtkNotebook *gnb = GTK_NOTEBOOK(multitext_window_get_notebook(mwin));
    gtk_notebook_insert_page(gnb, vpw, NULL, index);
    multitext_window_set_geometry_provider(MULTITEXT_WINDOW(self),
            MULTITEXT_GEOMETRY_PROVIDER(rvt));
    roxterm_vte_spawn(rvt, roxterm_window_spawn_callback, self);
    return rvt;
}
