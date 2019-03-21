/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2015 Tony Houghton <h@realh.co.uk>

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

#include "config.h"
#include "roxterm-tab-button.h"

enum {
    ROXTERM_TAB_ICON_BUSY,
    ROXTERM_TAB_ICON_OUTPUT,
    ROXTERM_TAB_ICON_EXITED,
    ROXTERM_TAB_ICON_BELL,

    ROXTERM_TAB_N_ICONS
};

static GdkPixbuf *roxterm_tab_icon_pixbufs[ROXTERM_TAB_N_ICONS]
        = { NULL, NULL, NULL, NULL };

static const char *roxterm_tab_icon_names[ROXTERM_TAB_N_ICONS] = {
    "busy", "output", "exited", "attention",
};

struct _RoxtermTabButton
{
    guint tab_state;
};

G_DEFINE_TYPE(RoxtermTabButton, roxterm_tab_button, MULTITEXT_TYPE_TAB_BUTTON);

static void
roxterm_tab_button_show_icon(RoxtermTabButton *self, guint state_index)
{
    MultitextTabButton *mb = MULTITEXT_TAB_BUTTON(self);
    if (!state_index)
    {
        multitext_tab_button_show_close_icon(mb);
    }
    else
    {
        --state_index;
        if (!roxterm_tab_icon_pixbufs[state_index])
        {
            GError *error = NULL;
            char *path = g_strdup_printf(ROXTERM_RESOURCE_PATH "/icons/%s.svg",
                    roxterm_tab_icon_names[state_index]);
            int w, h;
            gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
            roxterm_tab_icon_pixbufs[state_index] =
                gdk_pixbuf_new_from_resource_at_scale(path, w, h,
                        TRUE, &error);
            if (!roxterm_tab_icon_pixbufs[state_index])
            {
                multitext_tab_button_show_close_icon(mb);
                g_critical("Unable to load SVG resource '%s'", path);
                g_free(path);
                g_error_free(error);
                return;
            }
        }
        multitext_tab_button_set_pixbuf(mb,
                roxterm_tab_icon_pixbufs[state_index]);
    }
}

static void
roxterm_tab_button_show_state(RoxtermTabButton *self)
{
    for (guint n = 4; n > 0; --n)
    {
        if (self->tab_state & (1 << (n - 1)))
        {
            roxterm_tab_button_show_icon(self, n);
            return;
        }
    }
    roxterm_tab_button_show_icon(self, 0);
}

static void
roxterm_tab_button_class_init(UNUSED RoxtermTabButtonClass *klass)
{
}

static void
roxterm_tab_button_init(UNUSED RoxtermTabButton *self)
{
}

GtkWidget *
roxterm_tab_button_new(void)
{
    RoxtermTabButton *self = (RoxtermTabButton *)
            g_object_new (ROXTERM_TYPE_TAB_BUTTON, NULL);
    return GTK_WIDGET (self);
}

void
roxterm_tab_button_set_state(RoxtermTabButton *self, RoxtermTabState bits)
{
    self->tab_state |= bits;
    roxterm_tab_button_show_state(self);
}

void
roxterm_tab_button_clear_state(RoxtermTabButton *self, RoxtermTabState bits)
{
    self->tab_state &= ~bits;
    roxterm_tab_button_show_state(self);
}
