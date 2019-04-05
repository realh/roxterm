/*
    roxterm4 - VTE/GTK terminal emulator with tabs
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

#include "multitext-geometry-provider.h"
#include "roxterm-profile.h"
#include "roxterm-tab-button.h"
#include "roxterm-vte.h"

struct _RoxtermVte {
    VteTerminal parent_instance;
    RoxtermStrvRef *env;
    char *directory;
    RoxtermProfile *profile;
    GCancellable *launch_cancellable;
    VteTerminalSpawnAsyncCallback launch_callback;
    MultitextTabLabel *label;
    char *tab_title;
    int target_columns, target_rows;
    gboolean active;
    gboolean login_shell;
    gboolean hold_open_on_child_exit;
    gboolean alloc_for_measurement;
};

enum {
    ROXTERM_VTE_SIGNAL_GEOMETRY_CHANGED,

    ROXTERM_VTE_N_SIGNALS
};

static guint roxterm_vte_signals[ROXTERM_VTE_N_SIGNALS];

static void roxterm_vte_get_current_size(MultitextGeometryProvider *self,
        int *columns, int *rows)
{
    VteTerminal *vte = VTE_TERMINAL(self);
    if (columns)
        *columns = vte_terminal_get_column_count(vte);
    if (rows)
        *rows = vte_terminal_get_row_count(vte);
}

static void roxterm_vte_get_target_size(MultitextGeometryProvider *gp,
        int *columns, int *rows)
{
    RoxtermVte *self = ROXTERM_VTE(gp);
    *columns = self->target_columns;
    *rows = self->target_rows;
}

static void roxterm_vte_get_cell_size(MultitextGeometryProvider *self,
        int *width, int *height)
{
    VteTerminal *vte = VTE_TERMINAL(self);
    if (width)
        *width = vte_terminal_get_char_width(vte);
    if (height)
        *height = vte_terminal_get_char_height(vte);
}

static void roxterm_vte_set_active(MultitextGeometryProvider *gp,
        gboolean active)
{
    RoxtermVte *self = ROXTERM_VTE(gp);
    self->active = active;
}

static gboolean roxterm_vte_confirm_close(UNUSED MultitextGeometryProvider *gp)
{
    return FALSE;
}

static MultitextTabLabel *
roxterm_vte_get_tab_label(MultitextGeometryProvider *gp)
{
    RoxtermVte *self = ROXTERM_VTE(gp);
    return self->label;
}

static void
roxterm_vte_set_tab_label(MultitextGeometryProvider *gp,
        MultitextTabLabel *label)
{
    RoxtermVte *self = ROXTERM_VTE(gp);
    self->label = label;
}

void roxterm_vte_set_alloc_for_measurement(
        MultitextGeometryProvider *gp, gboolean afm)
{
    RoxtermVte *self = ROXTERM_VTE(gp);
    self->alloc_for_measurement = afm;
}

static void roxterm_vte_child_exited(VteTerminal *vte, int status)
{
    RoxtermVte *self = ROXTERM_VTE(vte);
    if (self->hold_open_on_child_exit && !status)
    {
        MultitextTabLabel *label = multitext_geometry_provider_get_tab_label(
                MULTITEXT_GEOMETRY_PROVIDER(self));
        RoxtermTabButton *button
            = ROXTERM_TAB_BUTTON(multitext_tab_label_get_button(label));
        roxterm_tab_button_set_state(button, ROXTERM_TAB_STATE_EXITED);
    }
    else
    {
        gtk_widget_destroy(gtk_widget_get_parent(GTK_WIDGET(self)));
    }
}

static void roxterm_vte_geometry_provider_init(
        MultitextGeometryProviderInterface *iface)
{
    iface->get_target_size = roxterm_vte_get_target_size;
    iface->get_current_size = roxterm_vte_get_current_size;
    iface->get_cell_size = roxterm_vte_get_cell_size;
    iface->set_active = roxterm_vte_set_active;
    iface->confirm_close = roxterm_vte_confirm_close;
    iface->get_tab_label = roxterm_vte_get_tab_label;
    iface->set_tab_label = roxterm_vte_set_tab_label;
    iface->set_alloc_for_measurement = roxterm_vte_set_alloc_for_measurement;
}

G_DEFINE_TYPE_WITH_CODE(RoxtermVte, roxterm_vte, VTE_TYPE_TERMINAL,
        G_IMPLEMENT_INTERFACE(MULTITEXT_TYPE_GEOMETRY_PROVIDER,
            roxterm_vte_geometry_provider_init));

static void roxterm_vte_dispose(GObject *obj)
{
    RoxtermVte *self = ROXTERM_VTE(obj);
    if (self->env)
    {
        roxterm_strv_unref(self->env);
        self->env = NULL;
    }
    g_free(self->directory);
    self->directory = NULL;
    g_clear_object(&self->launch_cancellable);
    g_free(self->tab_title);
    self->tab_title = NULL;
    G_OBJECT_CLASS(roxterm_vte_parent_class)->dispose(obj);
}

enum {
    PROP_PROFILE = 1,
    PROP_FONT,
    PROP_LOGIN_SHELL,
    PROP_HOLD_ON_CHILD_EXIT,
    N_PROPS
};

static GParamSpec *roxterm_vte_props[N_PROPS] = {NULL};

static void roxterm_vte_set_property(GObject *obj, guint prop_id,
        const GValue *value, GParamSpec *pspec)
{
    RoxtermVte *self = ROXTERM_VTE(obj);
    switch (prop_id)
    {
        case PROP_PROFILE:
            roxterm_vte_set_profile(self, g_value_get_string(value));
            break;
        case PROP_FONT:
            roxterm_vte_set_font_name(self, g_value_get_string(value));
            break;
        case PROP_LOGIN_SHELL:
            self->login_shell = g_value_get_boolean(value);
            break;
        case PROP_HOLD_ON_CHILD_EXIT:
            self->hold_open_on_child_exit = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

static void roxterm_vte_get_property(GObject *obj, guint prop_id,
        GValue *value, GParamSpec *pspec)
{
    RoxtermVte *self = ROXTERM_VTE(obj);
    switch (prop_id)
    {
        case PROP_PROFILE:
            g_value_set_string(value, roxterm_vte_get_profile(self));
            break;
        case PROP_FONT:
            g_value_set_string(value, roxterm_vte_get_font_name(self));
            break;
        case PROP_LOGIN_SHELL:
            g_value_set_boolean(value, self->login_shell);
            break;
        case PROP_HOLD_ON_CHILD_EXIT:
            g_value_set_boolean(value, self->hold_open_on_child_exit);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

static void roxterm_vte_char_size_changed(VteTerminal *vte, guint w, guint h)
{
    CHAIN_UP(VTE_TERMINAL_CLASS, roxterm_vte_parent_class, char_size_changed)
        (vte, w, h);
    RoxtermVte *self = ROXTERM_VTE(vte);
    roxterm_vte_geometry_changed_if_active(self);
}

static void roxterm_vte_size_allocate(GtkWidget *widget, GtkAllocation *alloc)
{
    CHAIN_UP(GTK_WIDGET_CLASS, roxterm_vte_parent_class, size_allocate)
        (widget, alloc);
    RoxtermVte *self = ROXTERM_VTE(widget);
    VteTerminal *vte = VTE_TERMINAL(widget);
    if (!self->alloc_for_measurement)
    {
        self->target_columns = vte_terminal_get_column_count(vte);
        self->target_rows = vte_terminal_get_row_count(vte);
    }
    g_debug("vte size-allocate: %ld x %ld (afm %d)",
            vte_terminal_get_column_count(vte),
            vte_terminal_get_row_count(vte),
            self->alloc_for_measurement);
}

static void roxterm_vte_class_init(RoxtermVteClass *klass)
{
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    oklass->dispose = roxterm_vte_dispose;
    // Accessors must be set before installing properties
    oklass->set_property = roxterm_vte_set_property;
    oklass->get_property = roxterm_vte_get_property;
    roxterm_vte_props[PROP_PROFILE] =
            g_param_spec_string("profile", "profile-name",
            "Profile name", "Default",
            G_PARAM_READWRITE);
    roxterm_vte_props[PROP_FONT] =
            g_param_spec_string("font-name", "font-description",
            "Pango font description string", NULL,
            G_PARAM_READWRITE);
    roxterm_vte_props[PROP_LOGIN_SHELL] =
            g_param_spec_boolean("login-shell", "login-shell",
            "Whether shell is a login shell", TRUE,
            G_PARAM_READWRITE);
    roxterm_vte_props[PROP_HOLD_ON_CHILD_EXIT] =
            g_param_spec_boolean("hold-open-on-child-exit",
            "hold-open-on-child-exit",
            "Whether to keep the terminal open after its command has completed",
            FALSE, G_PARAM_READWRITE);
    g_object_class_install_properties(oklass, N_PROPS, roxterm_vte_props);
    roxterm_vte_signals[ROXTERM_VTE_SIGNAL_GEOMETRY_CHANGED]
        = g_signal_newv("geometry-changed", ROXTERM_TYPE_VTE,
                G_SIGNAL_RUN_LAST, NULL, NULL, NULL, NULL,
                G_TYPE_NONE, 0, NULL);
    VteTerminalClass *vklass = VTE_TERMINAL_CLASS(klass);
    vklass->child_exited = roxterm_vte_child_exited;
    vklass->char_size_changed = roxterm_vte_char_size_changed;
    GtkWidgetClass *wklass = GTK_WIDGET_CLASS(klass);
    wklass->size_allocate = roxterm_vte_size_allocate;
}

static void roxterm_vte_init(RoxtermVte *self)
{
    self->login_shell = TRUE;
}

RoxtermVte *roxterm_vte_new(void)
{
    GObject *obj = g_object_new(ROXTERM_TYPE_VTE,
            NULL);
    return ROXTERM_VTE(obj);
}

void roxterm_vte_apply_launch_params(RoxtermVte *self, RoxtermLaunchParams *lp,
        RoxtermWindowLaunchParams *wp, RoxtermTabLaunchParams *tp)
{
    VteTerminal *vte = VTE_TERMINAL(self);
    self->env = lp ? roxterm_strv_ref(lp->env) : NULL;
    if (wp && wp->geometry_str)
    {
        vte_terminal_set_size(vte, wp->columns, wp->rows);
        self->target_columns = wp->columns;
        self->target_rows = wp->rows;
    }
    else
    {
        self->target_columns = vte_terminal_get_column_count(vte);
        self->target_rows = vte_terminal_get_row_count(vte);
    }
    g_debug("At construction: %d x %d", self->target_columns, self->target_rows);
    if (tp)
    {
        if (tp->directory)
            self->directory = g_strdup(tp->directory);
        if (tp->tab_title)
            self->tab_title = g_strdup(tp->tab_title);
    }
    roxterm_vte_set_profile(self, tp ? tp->profile_name : "Default");
}

static void roxterm_vte_spawn_callback(VteTerminal *vte,
        GPid pid, GError *error, gpointer handle)
{
    RoxtermVte *self = ROXTERM_VTE(vte);
    g_clear_object(&self->launch_cancellable);
    if (self->launch_callback)
    {
        self->launch_callback(vte, pid, error, handle);
    }
}

void roxterm_vte_spawn(RoxtermVte *self,
        VteTerminalSpawnAsyncCallback callback, gpointer handle)
{
    char *argv[3] = { NULL, NULL, NULL };
    argv[0] = vte_get_user_shell();
    if (!argv[0])
        argv[0] = g_strdup("/bin/sh");
    if (self->login_shell)
        argv[1] = "-l";
    // Using a cancellable causes an operation not supported error
    //self->launch_cancellable = g_cancellable_new();
    self->launch_callback = callback;
    vte_terminal_spawn_async(VTE_TERMINAL(self), VTE_PTY_DEFAULT,
            self->directory, argv, self->env ? self->env->strv : NULL,
            G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, self->launch_cancellable,
            roxterm_vte_spawn_callback, handle);
    g_free(argv[0]);
}

void roxterm_vte_set_font_name(RoxtermVte *self, const char *font)
{
    PangoFontDescription *pango = font
        ? pango_font_description_from_string(font) : NULL;
    vte_terminal_set_font(&self->parent_instance, pango);
    // TODO: Recalculate geometry
    if (pango)
        pango_font_description_free(pango);
}

char *roxterm_vte_get_font_name(RoxtermVte *self)
{
    const PangoFontDescription *pango = 
                vte_terminal_get_font(&self->parent_instance);
    return pango ? pango_font_description_to_string(pango) : NULL;
}

void roxterm_vte_set_profile(RoxtermVte *self, const char *profile_name)
{
    GObject *obj = G_OBJECT(self);
    if (self->profile)
    {
        if (!strcmp(roxterm_profile_get_name(self->profile), profile_name))
            return;
        roxterm_profile_disconnect_property_listener(self->profile, obj);
        g_object_unref(self->profile);
    }
    self->profile = roxterm_profile_lookup(profile_name);
    roxterm_profile_apply_as_properties(self->profile, obj, NULL);
    roxterm_profile_connect_property_listener(self->profile, obj, NULL);
}

const char *roxterm_vte_get_profile(RoxtermVte *self)
{
    return self->profile ? roxterm_profile_get_name(self->profile) : NULL;
}

RoxtermTabLaunchParams *roxterm_vte_get_launch_params(RoxtermVte *self)
{
    RoxtermTabLaunchParams *tp = roxterm_tab_launch_params_new();
    tp->profile_name = g_strdup(roxterm_profile_get_name(self->profile));
    tp->tab_title = g_strdup(self->tab_title);
    tp->directory = g_strdup(self->directory);
    // TODO: vim field
    return tp;
}

void roxterm_vte_geometry_changed_if_active(RoxtermVte *self)
{
    if (self->active && !self->alloc_for_measurement)
    {
        g_signal_emit(self,
                roxterm_vte_signals[ROXTERM_VTE_SIGNAL_GEOMETRY_CHANGED], 0);
    }
}
