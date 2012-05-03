#ifndef BOXCOMPAT_H
#define BOXCOMPAT_H
/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2011 Tony Houghton <h@realh.co.uk>

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

/* Makes it easy to migrate from GtkBox to GtkGrid */

#ifndef DEFNS_H
#include "defns.h"
#endif

#if GTK_CHECK_VERSION(3, 0, 0)
inline static void box_compat_pack(GtkWidget *box, GtkWidget *child,
        gboolean hexpand, gboolean vexpand, int hspacing, int vspacing)
{
    gtk_container_add(GTK_CONTAINER(box), child);
    g_object_set(child, "hexpand", hexpand, "vexpand", vexpand,
            "margin-left", hspacing, "margin-right", hspacing,
            "margin-top", vspacing, "margin-bottom", vspacing,
            NULL);
}
#endif

inline static void box_compat_packh(GtkWidget *box, GtkWidget *child,
        gboolean expand, int spacing)
{
#if GTK_CHECK_VERSION(3, 0, 0)
    box_compat_pack(box, child, expand, TRUE, spacing / 2, 0);
#else
    gtk_box_pack_start(GTK_BOX(box), child, expand, expand, spacing);
#endif
}

inline static void box_compat_packv(GtkWidget *box, GtkWidget *child,
        gboolean expand, int spacing)
{
#if GTK_CHECK_VERSION(3, 0, 0)
    box_compat_pack(box, child, TRUE, expand, 0, spacing / 2);
#else
    gtk_box_pack_start(GTK_BOX(box), child, expand, expand, spacing);
#endif
}

#endif /* BOXCOMPAT_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
