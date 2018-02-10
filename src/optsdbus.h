#ifndef OPTSDBUS_H
#define OPTSDBUS_H
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


/* ROXTerm's interface to dbus */

#include "rtdbus.h"

/* A ROXTerm D-BUS option consists of:
 * Profile name (including "Profiles/", "Colours/" etc)
 * Option key
 * Option value (string or int)
 */

/* Possible values for what_happened in optsdbus_send_stuff_changed_signal */
#define OPTSDBUS_DELETED "OptionsDeleted"
#define OPTSDBUS_ADDED "OptionsAdded"
#define OPTSDBUS_RENAMED "OptionsRenamed"
#define OPTSDBUS_CHANGED "OptionsChanged"

#ifdef ROXTERM_CAPPLET

gboolean optsdbus_init(void);

gboolean optsdbus_send_string_opt_signal(const char *profile_name,
	const char *key, const char *value);

gboolean optsdbus_send_int_opt_signal(const char *profile_name,
	const char *key, int value);

gboolean optsdbus_send_float_opt_signal(const char *profile_name,
	const char *key, double value);

/* new_name may be NULL if not a rename operation */
gboolean optsdbus_send_stuff_changed_signal(const char *what_happened,
		const char *family_name, const char *current_name,
		const char *new_name);

#else /* !ROXTERM_CAPPLET */

typedef enum {
	OptsDBus_InvalidOpt,
	OptsDBus_StringOpt,
	OptsDBus_IntOpt,
	OptsDBus_FloatOpt
} OptsDBusOptType;

typedef union {
	const char *s;
	int i;
	double f;
} OptsDBusValue;

/* profile_name includes "Profiles/", "Colours/" etc.
 * Only one of str_val or int_val will be valid at a time. */
typedef void (*OptsDBusOptionHandler) (const char *profile_name,
	const char *key, OptsDBusOptType, OptsDBusValue);

void optsdbus_listen_for_opt_signals(OptsDBusOptionHandler handler);

/* new_name is NULL if what_happened isn't rename */
typedef void (*OptsDBusStuffChangedHandler)(const char *what_happened,
		const char *family_name, const char *current_name,
		const char *new_name);

void optsdbus_listen_for_stuff_changed_signals(
		OptsDBusStuffChangedHandler handler);

typedef void (*OptsDBusSetProfileHandler)(void *roxterm_id,
        const char *profile_name);

void optsdbus_listen_for_set_profile_signals(OptsDBusSetProfileHandler);
void optsdbus_listen_for_set_colour_scheme_signals(OptsDBusSetProfileHandler);
void optsdbus_listen_for_set_shortcut_scheme_signals(OptsDBusSetProfileHandler);

#endif /* !ROXTERM_CAPPLET */

/* In capplet this sends a D-BUS message to another instance; in terminal
 * it starts a new instance of the capplet which will either forward the
 * message to a previous instance or handle it itself. */
gboolean optsdbus_send_edit_opts_message(const char *method, const char *arg);

inline static gboolean
optsdbus_send_edit_profile_message(const char *profile_name)
{
	return optsdbus_send_edit_opts_message("EditProfile", profile_name);
}

inline static gboolean
optsdbus_send_edit_colour_scheme_message(const char *profile_name)
{
	return optsdbus_send_edit_opts_message("EditColourScheme", profile_name);
}

#endif /* OPTSDBUS_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
