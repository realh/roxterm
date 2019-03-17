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

#include "multitext-geometry-provider.h"
#include "roxterm-profile.h"
#include "roxterm-vte.h"

struct _RoxtermVte {
    VteTerminal parent_instance;
    RoxtermStrvRef *env;
    char *directory;
    RoxtermProfile *profile;
    GCancellable *launch_cancellable;
    VteTerminalSpawnAsyncCallback launch_callback;
    gboolean login_shell;
};

static void roxterm_vte_get_current_size(MultitextGeometryProvider *self,
        int *columns, int *rows)
{
    VteTerminal *vte = VTE_TERMINAL(self);
    if (columns)
        *columns = vte_terminal_get_column_count(vte);
    if (rows)
        *rows = vte_terminal_get_row_count(vte);
}

static void roxterm_vte_get_initial_size(MultitextGeometryProvider *self,
        int *columns, int *rows)
{
    // FIXME: If this is a newly created terminal it should read the size from
    // the profile, not the current size
    roxterm_vte_get_current_size(self, columns, rows);
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

static void roxterm_vte_geometry_provider_init(
        MultitextGeometryProviderInterface *iface)
{
    iface->get_initial_size = roxterm_vte_get_initial_size;
    iface->get_current_size = roxterm_vte_get_current_size;
    iface->get_cell_size = roxterm_vte_get_cell_size;
}

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
}

G_DEFINE_TYPE_WITH_CODE(RoxtermVte, roxterm_vte, VTE_TYPE_TERMINAL,
        G_IMPLEMENT_INTERFACE(MULTITEXT_TYPE_GEOMETRY_PROVIDER,
            roxterm_vte_geometry_provider_init));

enum {
    PROP_PROFILE = 1,
    PROP_FONT,
    PROP_LOGIN_SHELL,
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
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
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
    g_object_class_install_properties(oklass, N_PROPS, roxterm_vte_props);
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
    }
    if (tp && tp->directory)
        self->directory = g_strdup(tp->directory);
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
