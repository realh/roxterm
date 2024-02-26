
/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2024 Tony Houghton <h@realh.co.uk>

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

#include <glib.h>

#if !GLIB_CHECK_VERSION(2, 32, 0)
inline static GThread *g_thread_try_new(const gchar *name, GThreadFunc func,
                                        gpointer data, GError **error)
{
    return g_thread_create(func, data, TRUE, error);
}

inline static GThread *g_thread_new(const gchar *name, GThreadFunc func,
                                    gpointer data)
{
    GError *error = NULL;
    GThread *thread = g_thread_create(func, data, TRUE, &error);
    if (!thread)
    {
        g_error("Failed to launch thread '%s': %s", name,
            (error && error->message) ? error->message : "unknown error");
    }
}
#endif
