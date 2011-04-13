#ifndef ENCODINGS_H
#define ENCODINGS_H
/*
    roxterm - GTK+ 2.0 terminal emulator with tabs
    Copyright (C) 2004 Tony Houghton <h@realh.co.uk>

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

/* Loads the encodings keyfile or creates a default */
GKeyFile *encodings_load(void);

void encodings_save(GKeyFile *kf);

/* Returns a NULL-terminated array of strings which can (and should) be freed
 * with g_strfreev; the first item is always "Default". */
char **encodings_list_full(GKeyFile *, gboolean sorted);

inline static char **encodings_list(GKeyFile *kf)
{
    return encodings_list_full(kf, FALSE);
}

inline static char **encodings_list_sorted(GKeyFile *kf)
{
    return encodings_list_full(kf, TRUE);
}

char *encodings_lookup(GKeyFile *, int n);

int encodings_count(GKeyFile *);

void encodings_add(GKeyFile *, const char *);

void encodings_remove(GKeyFile *, int n);

void encodings_change(GKeyFile *, int n, const char *);


#endif /* ENCODINGS_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
