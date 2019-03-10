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
#include "roxterm-vte.h"

struct _RoxtermVte {
    VteTerminal parent_instance;
    RoxtermStrvRef *env;
    char *directory;
    GCancellable *launch_cancellable;
    VteTerminalSpawnAsyncCallback launch_callback;
    int zoom;
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

static void roxterm_vte_class_init(RoxtermVteClass *klass)
{
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    oklass->dispose = roxterm_vte_dispose;
}

static void roxterm_vte_init(RoxtermVte *self)
{
    (void) self;
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
    self->env = roxterm_strv_ref(lp->env);
    if (wp->geometry_str)
    {
        vte_terminal_set_size(vte, wp->columns, wp->rows);
    }
    if (tp->directory)
        self->directory = g_strdup(tp->directory);
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
    char *argv[2] = { NULL, NULL };
    argv[0] = vte_get_user_shell();
    if (!argv[0])
        argv[0] = g_strdup("/bin/sh");
    // Using a cancellable causes an operation not supported error
    //self->launch_cancellable = g_cancellable_new();
    self->launch_callback = callback;
    vte_terminal_spawn_async(VTE_TERMINAL(self), VTE_PTY_DEFAULT,
            self->directory, argv, self->env ? self->env->strv : NULL,
            G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, self->launch_cancellable,
            roxterm_vte_spawn_callback, handle);
}

inline static void roxterm_vte_apply_pango_zoom(PangoFontDescription *p, int zoom)
{
    pango_font_description_set_size(p,
            pango_font_description_get_size(p) * zoom / 100);
}

static void roxterm_vte_update_font(RoxtermVte *self, const char *font,
        int zoom)
{
    const PangoFontDescription *orig_pango = 
                vte_terminal_get_font(&self->parent_instance);
    PangoFontDescription *pango = font
        ? pango_font_description_from_string(font)
        : pango_font_description_copy_static(orig_pango);
    if (!font)
        zoom = zoom * 100 / self->zoom;
    roxterm_vte_apply_pango_zoom(pango, zoom);
    if (!pango_font_description_equal(pango, orig_pango))
    {
        vte_terminal_set_font(&self->parent_instance, pango);
        // TODO: Recalculate geometry
    }
    self->zoom = zoom;
    pango_font_description_free(pango);
}

void roxterm_vte_set_font_name(RoxtermVte *self, const char *font)
{
    roxterm_vte_update_font(self, font, self->zoom);
}

char *roxterm_vte_get_font_name(RoxtermVte *self)
{
    const PangoFontDescription *orig_pango = 
                vte_terminal_get_font(&self->parent_instance);
    char *result;
    if (self->zoom != 100)
    {
        PangoFontDescription *pango
            = pango_font_description_copy_static(orig_pango);
        roxterm_vte_apply_pango_zoom(pango, 10000 / self->zoom);
        result = pango_font_description_to_string(pango);
        pango_font_description_free(pango);
    }
    else
    {
        result = pango_font_description_to_string(orig_pango);
    }
    return result;
}

void roxterm_vte_set_zoom(RoxtermVte *self, int zoom)
{
    roxterm_vte_update_font(self, NULL, zoom);
}

int roxterm_vte_get_zoom(RoxtermVte *self)
{
    return self->zoom;
}
