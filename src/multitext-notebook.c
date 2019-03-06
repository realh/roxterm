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

#include "multitext-notebook.h"

struct _MultitextNotebook {
    GtkNotebook parent_instance;
} ;

G_DEFINE_TYPE(MultitextNotebook, multitext_notebook, GTK_TYPE_NOTEBOOK);

static void multitext_notebook_class_init(MultitextNotebookClass *klass)
{
    (void) klass;
}

static void multitext_notebook_init(MultitextNotebook *self)
{
    (void) self;
}

MultitextNotebook *multitext_notebook_new(void)
{
    GObject *obj = g_object_new(MULTITEXT_TYPE_NOTEBOOK, NULL);
    return MULTITEXT_NOTEBOOK(obj);
}
