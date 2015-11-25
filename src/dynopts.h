#ifndef DYNOPTS_H
#define DYNOPTS_H
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


/* A sort of base class for "dynamic options" ie profiles, colour schemes and
 * keyboard shortcuts, which can be looked up by name */

#include "options.h"

typedef struct DynamicOptions DynamicOptions;

/* family is subdir in roxterm: Profiles, Colours, Shortcuts. This
 * returns a reference to existing DynamicOptions if one has already been
 * created for the given family */
DynamicOptions *dynamic_options_get(const char *family);

/* profile_name may actually be a colour scheme or keyboard shortcut scheme;
 * returns NULL if it hasn't already been opened with
 * dynamic_options_lookup_and_ref */
Options *dynamic_options_lookup(DynamicOptions * dynopts,
	const char *profile_name);

/* Creates Options if it hasn't already been opened */
Options *dynamic_options_lookup_and_ref(DynamicOptions * dynopts,
	const char *profile_name, const char *group_name);

/* Removes profile from dynopts without unrefing profile */
void
dynamic_options_forget(DynamicOptions *dynopts, const char *profile_name);

/* Calls options_unref on looked up profile and removes it from dynopts */
gboolean
dynamic_options_unref(DynamicOptions * dynopts, const char *profile_name);

/* Renames a profile */
void dynamic_options_rename(DynamicOptions *dynopts,
		const char *old_name, const char *new_name);

char **dynamic_options_list_full(DynamicOptions *, gboolean sorted);

/* Returns a list of names (g_strfreev them) of files within family's subdir;
 * the first item will always be "Default" even if no such file exists */
inline static char **dynamic_options_list(DynamicOptions *dynopts)
{
    return dynamic_options_list_full(dynopts, FALSE);
}

/* As above but the list is sorted (but Default comes first) */
inline static char **dynamic_options_list_sorted(DynamicOptions *dynopts)
{
    return dynamic_options_list_full(dynopts, TRUE);
}

/* Like g_strcmp0 but "Default" comes first */
int dynamic_options_strcmp(const char *s1, const char *s2);

#endif /* DYNOPTS_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
