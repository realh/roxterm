#ifndef RTDBUS_H
#define RTDBUS_H
/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2011 Tony Houghton <h@realh.co.uk>

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


/* D-BUS functions common to all parts of ROXTerm */

#ifndef DEFNS_H
#include "defns.h"
#endif

#include <dbus/dbus-glib-lowlevel.h>

/* These are just stubs; they should have a specific suffix appended */
#define RTDBUS_NAME "net.sf.roxterm"
#define RTDBUS_OBJECT_PATH "/net/sf/roxterm"
#define RTDBUS_INTERFACE RTDBUS_NAME
#define RTDBUS_ERROR RTDBUS_NAME

extern DBusConnection  *rtdbus_connection;
extern DBusGConnection *rtdbus_g_connection;

extern gboolean rtdbus_ok;

/* Report a D-BUS error in a dialog, prepending message 's',
 * then free the error data */
void rtdbus_whinge(DBusError *pderror, const char *s);

/* As above but print it on the console with g_warning */
void rtdbus_warn(DBusError *pderror, const char *s);

/* Unrefs message after sending */
gboolean rtdbus_send_message(DBusMessage *message);

/* As above but waits for a reply and checks it for success */
gboolean rtdbus_send_message_with_reply(DBusMessage *message);

/* Returns TRUE if another instance already has the name */
gboolean rtdbus_start_service(const char *name, const char *object_path,
		DBusObjectPathMessageFunction, gboolean replace);

/* Call before any other D-BUS functions. May be called more than once */
gboolean rtdbus_init(void);

/* Adds a match rule and filter */
gboolean rtdbus_add_rule_and_filter(const char *match_rule,
        DBusHandleMessageFunction filter_fn, void *user_data);

/* Constructs a match rule for the given signal and adds a filter for it */
gboolean rtdbus_add_signal_rule_and_filter(
		const char *path, const char *interface,
		DBusHandleMessageFunction);

/* Creates a signal message with the given arguments passed as pairs of
 * DBUS_TYPE_*, &(*) terminated by DBUS_TYPE_INVALID */
DBusMessage *rtdbus_signal_new(const char *object_path, const char *interface,
		const char *signal_name, int first_arg_type, ...);

/* As above but creates a method call */
DBusMessage *rtdbus_method_new(const char *bus_name, const char *object_path,
        const char *interface, const char *method_name,
        int first_arg_type, ...);

/* Appends args (see above) to message and returns the modified message. On
 * failure (unlikely) the message is freed, an error is printed and NULL is
 * returned */
DBusMessage *rtdbus_append_args(DBusMessage *message, 
		int first_arg_type, ...);

/* See above */
DBusMessage *rtdbus_append_args_valist(DBusMessage *message, 
		int first_arg_type, va_list ap);

/* Returns NULL-terminated array of strings ("strv") corresponding to a
 * message's args. Non-string message args are counted as errors and skipped.
 * Result may be freed with g_strfreev if it isn't NULL (no args).
 * iter must be initialised before calling.
 */
char **rtdbus_get_message_args_as_strings(DBusMessageIter *iter);

/* Returns a "strv" from a single message arg, advancing iter */
char **rtdbus_get_message_arg_string_array(DBusMessageIter *iter);

#endif /* RTDBUS_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
