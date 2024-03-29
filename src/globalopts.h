#ifndef GLOBALOPTS_H
#define GLOBALOPTS_H
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

/* Whether to open next window borderless */
extern gboolean global_options_borderless;

/* Whether to try to open new terminal in an existing window */
extern gboolean global_options_tab;

/* Fork first instance */
extern gboolean global_options_fork;

/* What to do on command exit */
extern gint global_options_atexit;

extern char *global_options_user_session_id;

/* Key for dark theme preference in GSettings */
extern const char *global_options_color_scheme_key;

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

/* Checks CLI args for --appdir and --fork without altering argv */
void global_options_init_appdir(int argc, char **argv);

/* Detects bindir from argv[0] */
void global_options_init_bindir(const char *argv0);

/* Deep copy a NULL-terminated array of NULL-terminated strings */
char **global_options_copy_strv(char **ps);

/* Returns whether GNOME's dark theme preference is available. */
gboolean global_options_has_gnome_dark_theme_setting(void);

/* Returns whether GTK supports dark theme. */
gboolean global_options_has_gtk_dark_theme_setting();

/* Gets the prefer-dark setting from GNOME's gsettings or roxterm's legacy
 * option.
 */
gboolean global_options_system_theme_is_dark(void);

/* Applies the dark theme setting now and whenever the Gsetting changes */
void global_options_apply_dark_theme(void);

typedef void (*GlobalOptionsDarkThemeChangeHandler)(gboolean prefer_dark,
	gpointer handle);

/* Only works on GSettings, legacy option should be monitored separately. */
void global_options_register_dark_theme_change_handler(
	GlobalOptionsDarkThemeChangeHandler handler, gpointer handle);

#endif /* GLOBALOPTS_H */

/* vi:set sw=4 ts=4 et cindent cino= */
