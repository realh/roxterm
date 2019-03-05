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

#include "roxterm-vte.h"

struct _RoxtermVte {
    VteTerminal parent_instance;
};

G_DEFINE_TYPE(RoxtermVte, roxterm_vte, VTE_TYPE_TERMINAL);

static void roxterm_vte_class_init(RoxtermVteClass *klass)
{
    (void) klass;
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
