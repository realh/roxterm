#ifndef ENCODINGS_H
#define ENCODINGS_H
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


#ifndef DEFNS_H
#include "defns.h"
#endif

typedef GPtrArray Encodings;

/* Loads the encodings file or creates a default */
Encodings *encodings_load(void);

void encodings_save(Encodings *);

/* Returns a NULL-terminated array of strings; free the list with g_free, not
 * g_strfreev; the first item is always "Default".
 */
char const **encodings_list(Encodings *);

inline static const char *encodings_lookup(Encodings *enc, int n)
{
    return g_ptr_array_index(enc, n);
}

inline static int encodings_count(Encodings *enc)
{
    return enc->len;
}

void encodings_add(Encodings *enc, const char *v);

inline static void encodings_remove(Encodings *enc, int n)
{
    g_ptr_array_remove_index(enc, n);
}

void encodings_change(Encodings *, int n, const char *);


#endif /* ENCODINGS_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
