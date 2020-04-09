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

#define ROXTERM_TYPE_DYNAMIC_OPTIONS roxterm_dynamic_options_get_type()
G_DECLARE_FINAL_TYPE(RoxtermDynamicOptions, roxterm_dynamic_options,
        ROXTERM, DYNAMIC_OPTIONS, GObject);

typedef RoxtermDynamicOptions DynamicOptions;

/**
 * roxterm_dynamic_options_get:
 * Returns: (transfer none):
 */
RoxtermDynamicOptions *roxterm_dynamic_options_get(const char *family);

/**
 * roxterm_dynamic_options_lookup: (method):
 * Returns: (transfer none) (nullable):
 */
Options *roxterm_dynamic_options_lookup(RoxtermDynamicOptions *dynopts,
	const char *profile_name);

/**
 * roxterm_dynamic_options_get_family_name: (method):
 */
const char *
roxterm_dynamic_options_get_family_name(RoxtermDynamicOptions *dynopts);

/**
 * roxterm_dynamic_options_lookup: (method):
 * Returns: (transfer none) (not nullable):
 */
Options *roxterm_dynamic_options_lookup_and_ref(RoxtermDynamicOptions *dynopts,
	const char *profile_name);

/**
 * roxterm_dynamic_options_unref: (method): Unrefs the profile/scheme, not the
 *      dynopts
 * Returns: TRUE if the Options unref'd to 0
 */
gboolean roxterm_dynamic_options_unref(RoxtermDynamicOptions *dynopts,
        const char *profile_name);

/**
 * roxterm_dynamic_options_rename: (method):
 */
void roxterm_dynamic_options_rename(RoxtermDynamicOptions *dynopts,
		const char *old_name, const char *new_name);

/**
 * roxterm_dynamic_options_list_full: (method):
 * Returns: (transfer full):
 */
char **roxterm_dynamic_options_list_full(RoxtermDynamicOptions *dynopts,
        gboolean sorted);

/**
 * roxterm_opts_manager_option_changed: (method): Signal that an option's
 *      value has changed
 * @name: Name of profile/scheme
 * @key: Option key
 */
void roxterm_dynamic_options_option_changed(RoxtermDynamicOptions *dynopts,
        const char *name, const char *key);

// Signals

/**
 * RoxtermDynamicOptions::options-deleted:
 * @name: Name of profile/scheme
 *
 * This signal is raised before the deletion.
 */

/**
 * RoxtermDynamicOptions::options-added:
 * @name: Name of profile/scheme
 *
 * This signal is raised after the addition.
 */

/**
 * RoxtermDynamicOptions::options-renamed:
 * @old_name: Old name of profile/scheme
 * @new_name: New name of profile/scheme
 *
 * This signal is raised after the rename.
 */

/**
 * RoxtermDynamicOptions::option-changed:
 * @name: Name of profile/scheme
 * @key: Option key
 * Raised after an option value has changed
 */

// Backwards compatibility with older code

/* Like g_strcmp0 but "Default" comes first */
int dynamic_options_strcmp(const char *s1, const char *s2);

/* family is subdir in roxterm: Profiles, Colours, Shortcuts. This
 * returns a reference to existing DynamicOptions if one has already been
 * created for the given family */
inline DynamicOptions *dynamic_options_get(const char *family)
{
    return roxterm_dynamic_options_get(family);
}

/* profile_name may actually be a colour scheme or keyboard shortcut scheme;
 * returns NULL if it hasn't already been opened with
 * dynamic_options_lookup_and_ref */
inline Options *dynamic_options_lookup(DynamicOptions *dynopts,
	const char *profile_name)
{
    return roxterm_dynamic_options_lookup(dynopts, profile_name);
}

/* Creates Options if it hasn't already been opened */
inline Options *dynamic_options_lookup_and_ref(DynamicOptions * dynopts,
	const char *profile_name)
{
    return roxterm_dynamic_options_lookup_and_ref(dynopts, profile_name);
}

/* Calls options_unref on looked up profile and removes it from dynopts */
inline gboolean dynamic_options_unref(DynamicOptions *dynopts,
        const char *profile_name)
{
    return roxterm_dynamic_options_unref(dynopts, profile_name);
}

/* Renames a profile */
inline void dynamic_options_rename(DynamicOptions *dynopts,
		const char *old_name, const char *new_name)
{
    roxterm_dynamic_options_rename(dynopts, old_name, new_name);
}

inline char **dynamic_options_list_full(DynamicOptions *dynopts,
        gboolean sorted)
{
    return roxterm_dynamic_options_list_full(dynopts, sorted);
}

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

/**
 * roxterm_dynamic_options_changed: (method): Raise corresponding signal
 *      when a shortcuts scheme file has been changed
 */
void roxterm_dynamic_options_scheme_changed(DynamicOptions *dynopts,
        const char *scheme_name);

/**
 * RoxtermDynamicOptions::scheme-changed:
 */

#endif /* DYNOPTS_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
