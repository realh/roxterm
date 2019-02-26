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
#include "roxterm-window.h"

struct _RoxtermWindow {
    GtkWindow parent_instance;
    RoxtermApplication *app;
};

G_DEFINE_TYPE(RoxtermWindow, roxterm_window, GTK_TYPE_APPLICATION_WINDOW);

enum {
    PROP_APPLICATION = 1,
    N_PROPS
};

static GParamSpec *roxterm_window_props[N_PROPS] = {NULL};

static void roxterm_window_set_property(GObject *obj, guint prop_id,
        const GValue *value, GParamSpec *pspec)
{
    RoxtermWindow *self = ROXTERM_WINDOW(obj);
    switch (prop_id)
    {
        case PROP_APPLICATION:
            // No point in holding a reference
            self->app = g_value_get_object(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

static void roxterm_window_get_property(GObject *obj, guint prop_id,
        GValue *value, GParamSpec *pspec)
{
    RoxtermWindow *self = ROXTERM_WINDOW(obj);
    switch (prop_id)
    {
        case PROP_APPLICATION:
            // No point in holding a reference
            g_value_set_object(value, self->app);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

static void roxterm_window_class_init(RoxtermWindowClass *klass)
{
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    roxterm_window_props[PROP_APPLICATION] =
            g_param_spec_object("application", "application",
            "RoxtermApplication", ROXTERM_TYPE_APPLICATION,
            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
    g_object_class_install_properties(oklass, N_PROPS, roxterm_window_props);
    oklass->set_property = roxterm_window_set_property;
    oklass->get_property = roxterm_window_get_property;
}

static void roxterm_window_init(RoxtermWindow *self)
{
    (void) self;
}

RoxtermWindow *roxterm_window_new(RoxtermApplication *app)
{
    GObject *obj = g_object_new(ROXTERM_TYPE_WINDOW,
            "application", app,
            NULL);
    return ROXTERM_WINDOW(obj);
}

RoxtermApplication *roxterm_window_get_application(RoxtermWindow *win)
{
    return win->app;
}