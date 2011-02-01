#ifndef GLOBALOPTS_H
#define GLOBALOPTS_H
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


#include "options.h"

/* Call before gtk_init parses argv in case there's an execute option which may
 * get corrupted by gtk_init; returns FALSE if there's a -e/--execute with no
 * subsequent args; use shallow_copy if you want original strings after -e
 * to be freed later; argc is reduced to exclude any args after and including
 * -e/--execute, but argv isn't actually changed */
gboolean global_options_preparse_argv_for_execute(int *argc, char **argv,
		gboolean shallow_copy);

/* This will be non-NULL after above call if argv contained -e/--execute */
extern char **global_options_commandv;

/* This will be non-NULL after call below if argv contained --appdir */
extern char *global_options_appdir;

/* Similar to global_options_appdir but derived from argv[0] */
extern char *global_options_bindir;

/* Directory to run terminal in */
extern char *global_options_directory;

/* Whether to open next window fullscreen */
extern gboolean global_options_fullscreen;

/* Whether to open next window maximised */
extern gboolean global_options_maximise;

/* Whether to try to open new terminal in an existing window */
extern gboolean global_options_tab;

/* For session management */
extern gboolean global_options_disable_sm;
extern char *global_options_restart_session_id;
extern char *global_options_clone_session_id;

/* Call after argv has been processed by gtk_init; may be called more than once
 * but repeat invocations have no effect on appdir/bindir and argv/argc are
 * altered. Bear in mind that --help/--usage args will cause exit.
 * If report is FALSE, parsing errors are ignored (because it gets called
 * twice with same args if doing a dbus launch).
 */
void global_options_init(int *argc, char ***argv, gboolean report);

/* Only access via following functions */
extern Options *global_options;

inline static char *global_options_lookup_string_with_default(const char
	*key, const char *default_value)
{
	return options_lookup_string_with_default(global_options, key,
		default_value);
}

inline static char *global_options_lookup_string(const char *key)
{
	return options_lookup_string_with_default(global_options, key, NULL);
}

inline static int global_options_lookup_int_with_default(const char *key, int d)
{
	return options_lookup_int_with_default(global_options, key, d);
}

inline static int global_options_lookup_int(const char *key)
{
	return options_lookup_int(global_options, key);
}

inline static double global_options_lookup_double(const char *key)
{
	return options_lookup_double(global_options, key);
}

/* Reset a string option which should only be "one-shot" */
inline static void global_options_reset_string(const char *key)
{
    options_set_string(global_options, key, NULL);
}

/* Checks CLI args for --appdir without altering argv */
void global_options_init_appdir(int argc, char **argv);

/* Deep copy a NULL-terminated array of NULL-terminated strings */
char **global_options_copy_strv(char **ps);

#if HAVE_GET_CURRENT_DIR_NAME
#include <stdlib.h>
#include <unistd.h>
#define GET_CURRENT_DIR get_current_dir_name
#define FREE_CURRENT_DIR free
#else
#define GET_CURRENT_DIR g_get_current_dir
#define FREE_CURRENT_DIR g_free
#endif

#endif /* GLOBALOPTS_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
