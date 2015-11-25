#ifndef OPTSFILE_H
#define OPTSFILE_H
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


/* Keyfile component of options */

#ifndef DEFNS_H
#include "defns.h"
#endif

/* Loads options from a named file, searching for it in Choices path; if no
 * file found it returns NULL */
GKeyFile *options_file_open(const char *leafname, const char *group_name);

void options_file_save(GKeyFile *, const char *leafname);

inline static void options_file_delete(GKeyFile *kf)
{
	g_key_file_free(kf);
}

/* These 2 functions construct a partial pathname from filename and remaining
 * args (NULL-terminated). */

/* Finds first existing file with this name in roxterm's config path */
char *options_file_build_filename(const char *filename, ...);

/* Finds best full filename to save a file. Creates filename's parent dirs if
 * necessary, but not elements of filename */
char *options_file_filename_for_saving(const char *filename, ...);

/* Makes directory, recursively making parents if necessary */
gboolean options_file_mkdir_with_parents(const char *dirname);

/* Gets all paths where options may be stored, NULL terminated */
const char * const *options_file_get_pathv(void);

char *options_file_lookup_string_with_default(
		GKeyFile *kf, const char *group_name,
		const char *key, const char *default_value);

int options_file_lookup_int_with_default(
		GKeyFile *kf, const char *group_name,
		const char *key, int default_value);

gboolean options_file_copy_to_user_dir(GtkWindow *window,
        const char *src_path, const char *family, const char *new_leaf);

char *options_file_make_editable(GtkWindow *window,
        const char *family_name, const char *name);

#endif /* OPTSFILE_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
