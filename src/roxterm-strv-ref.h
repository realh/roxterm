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
#ifndef __ROXTERM_STRV_REF_H
#define __ROXTERM_STRV_REF_H

#include <glib.h>

/**
 * RoxtermStrvRef:
 *
 * Provides reference counting for a strv
 *
 * @count: The reference count
 * @strv: The actual strv
 */
typedef struct {
    char **strv;
    grefcount count;
} RoxtermStrvRef;

inline static RoxtermStrvRef *roxterm_strv_ref_new(char const * const *src)
{
    RoxtermStrvRef *rsr = g_new(RoxtermStrvRef, 1);
    rsr->strv = g_strdupv((char **) src);
    rsr->count = 1;
    return rsr;
}

inline static RoxtermStrvRef *roxterm_strv_ref(RoxtermStrvRef *rsr)
{
    ++rsr->count;
    return rsr;
}

inline static int roxterm_strv_unref(RoxtermStrvRef *rsr)
{
    int a = --rsr->count;
    if (!a)
    {
        g_strfreev(rsr->strv);
        g_free(rsr);
    }
    return a;
}

#endif /* __ROXTERM_STRV_REF_H */
