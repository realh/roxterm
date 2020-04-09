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


#include "defns.h"

#include "resources.h"
#include "globalopts.h"

void resources_access_icon()
{
    static gboolean added = FALSE;
    if (!added)
    {
        gtk_icon_theme_add_resource_path(gtk_icon_theme_get_default(),
                ROXTERM_RESOURCE_ICONS_PATH);
        added = TRUE;
#if 0
        GError *error = NULL;
        gsize size;
        guint32 flags;
        if (!g_resources_get_info(ROXTERM_RESOURCE_LOGO, 0, &size, &flags,
                    &error))
        {
            g_critical("%s not found: %s", ROXTERM_RESOURCE_LOGO,
                    error->message);
            g_error_free(error);
        }
        else
        {
            g_debug("%s has flags %x and size %ld", ROXTERM_RESOURCE_LOGO,
                    flags, size);
        }
#endif
    }
}

/* vi:set sw=4 ts=4 noet cindent cino= */
