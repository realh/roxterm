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


#include "dlg.h"
#include "globalopts.h"
#include "optsdbus.h"

#ifdef ROXTERM_CAPPLET
#include "colourgui.h"
#include "configlet.h"
#include "profilegui.h"
#endif

#include <string.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus-glib-lowlevel.h>

#define OPTSDBUS_NAME RTDBUS_NAME ".Options"
#define OPTSDBUS_OBJECT_PATH RTDBUS_OBJECT_PATH "/Options"
#define OPTSDBUS_INTERFACE RTDBUS_INTERFACE ".Options"
#define OPTSDBUS_ERROR RTDBUS_ERROR ".OptionsError"

#define OPTSDBUS_STRING_SIGNAL "StringOption"
#define OPTSDBUS_INT_SIGNAL "IntOption"
#define OPTSDBUS_FLOAT_SIGNAL "FloatOption"

#define OPTSDBUS_SET_PROFILE "SetProfile"
#define OPTSDBUS_SET_COLOUR_SCHEME "SetColourScheme"
#define OPTSDBUS_SET_SHORTCUT_SCHEME "SetShortcutScheme"

#ifdef ROXTERM_CAPPLET

static gboolean optsdbus_read_args(DBusMessage *message, char const **arg,
        DBusError *perror, DBusMessage **preply_error)
{
    gboolean result = arg ? dbus_message_get_args(message, perror,
                DBUS_TYPE_STRING, arg,
                DBUS_TYPE_INVALID) :
                dbus_message_get_args(message, perror,
                DBUS_TYPE_INVALID);
    if (!result)
    {
        rtdbus_whinge(perror, _("Unable to get argument from D-BUS message"));
        if (!dbus_message_get_no_reply(message))
        {
            *preply_error = dbus_message_new_error(message, OPTSDBUS_ERROR,
                    _("Unable to get argument"));
        }
        return FALSE;
    }
    return TRUE;
}

static DBusHandlerResult optsdbus_method_handler(DBusConnection *connection,
        DBusMessage *message, void *user_data)
{
    DBusMessage *reply = NULL;
    DBusError derror;
    enum {
        Unknown,
        EditProfile,
        EditColourScheme,
        OpenConfiglet
    } action = Unknown;
    const char *profile_name = NULL;
    char const **parg = NULL;
    (void) connection;
    (void) user_data;

    dbus_error_init(&derror);

    if (dbus_message_is_method_call(message, OPTSDBUS_INTERFACE, "EditProfile"))
    {
        action = EditProfile;
    }
    else if (dbus_message_is_method_call(message, OPTSDBUS_INTERFACE,
                "EditColourScheme"))
    {
        action = EditColourScheme;
    }
    else if (dbus_message_is_method_call(message, OPTSDBUS_INTERFACE,
                "Configlet"))
    {
        action = OpenConfiglet;
    }
    if (action == EditProfile || action == EditColourScheme)
        parg = &profile_name;
    switch (action)
    {
        case EditProfile:
        case EditColourScheme:
        case OpenConfiglet:
            if (!optsdbus_read_args(message, parg, &derror, &reply))
            {
                break;
            }
            /* Fall-through */
            switch (action)
            {
                case EditProfile:
                    profilegui_open(profile_name);
                    break;
                case EditColourScheme:
                    colourgui_open(profile_name);
                    break;
                case OpenConfiglet:
                    configlet_open();
                    break;
                default:
                    break;
            }
            if (!dbus_message_get_no_reply(message))
                reply = dbus_message_new_method_return(message);
#if ROXTERM_DBUS_OLD_ARGS_SEMANTICS
            if (profile_name)
                dbus_free(profile_name);
#endif
            break;
        default:
            g_warning(_("Don't know how to handle method %s.%s"),
                        dbus_message_get_interface(message),
                        dbus_message_get_member(message));
            break;
    }

    if (reply)
        rtdbus_send_message(reply);

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusMessage *optsdbus_signal_new(const char *signal_name,
        const char *profile_name, const char *key)
{
    return rtdbus_signal_new(OPTSDBUS_OBJECT_PATH, OPTSDBUS_INTERFACE,
            signal_name,
            DBUS_TYPE_STRING, &profile_name,
            DBUS_TYPE_STRING, &key,
            DBUS_TYPE_INVALID);
}

gboolean optsdbus_send_string_opt_signal(const char *profile_name,
    const char *key, const char *value)
{
    DBusMessage *message = optsdbus_signal_new(OPTSDBUS_STRING_SIGNAL,
        profile_name, key);

    if (message)
    {
        DBusError derror;

        dbus_error_init(&derror);
        if (!value)
            value = "";
        if (!rtdbus_append_args(message,
                DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID))
        {
            return FALSE;
        }
        else
        {
            return rtdbus_send_message(message);
        }
    }
    return FALSE;
}

gboolean optsdbus_send_int_opt_signal(const char *profile_name,
    const char *key, int value)
{
    DBusMessage *message;

    message = optsdbus_signal_new(OPTSDBUS_INT_SIGNAL,
        profile_name, key);

    if (message)
    {
        DBusError derror;

        dbus_error_init(&derror);
        if (!dbus_message_append_args(message,
                DBUS_TYPE_INT32, &value, DBUS_TYPE_INVALID))
        {
            return FALSE;
        }
        else
        {
            return rtdbus_send_message(message);
        }
    }
    return FALSE;
}

gboolean optsdbus_send_float_opt_signal(const char *profile_name,
    const char *key, double value)
{
    DBusMessage *message;

    message = optsdbus_signal_new(OPTSDBUS_FLOAT_SIGNAL,
        profile_name, key);

    if (message)
    {
        DBusError derror;

        dbus_error_init(&derror);
        if (!dbus_message_append_args(message,
                DBUS_TYPE_DOUBLE, &value, DBUS_TYPE_INVALID))
        {
            return FALSE;
        }
        else
        {
            return rtdbus_send_message(message);
        }
    }
    return FALSE;
}

gboolean optsdbus_send_stuff_changed_signal(const char *what_happened,
        const char *family_name, const char *old_name, const char *new_name)
{
    /* We can cheat and use optsdbus_signal_new because it just happens
     * to build a message with 2 string args; just what we want */
    DBusMessage *message = optsdbus_signal_new(what_happened,
        family_name, old_name);

    if (message)
    {
        DBusError derror;

        dbus_error_init(&derror);
        if (new_name)
        {
            if (!dbus_message_append_args(message,
                    DBUS_TYPE_STRING, &new_name, DBUS_TYPE_INVALID))
            {
                return FALSE;
            }
        }
        return rtdbus_send_message(message);
    }
    return FALSE;
}

gboolean optsdbus_send_edit_opts_message(const char *method, const char *arg)
{
    DBusMessage * message;

    message = rtdbus_method_new(OPTSDBUS_NAME,
            OPTSDBUS_OBJECT_PATH, OPTSDBUS_INTERFACE, method,
            DBUS_TYPE_STRING, &arg,
            DBUS_TYPE_INVALID);

    if (!message)
        return FALSE;
    return rtdbus_send_message(message);
}

gboolean optsdbus_init(void)
{
    return rtdbus_start_service(OPTSDBUS_NAME, OPTSDBUS_OBJECT_PATH,
            optsdbus_method_handler, FALSE);
}

#else /* !ROXTERM_CAPPLET */

static OptsDBusOptionHandler optsdbus_option_handler = NULL;
static OptsDBusStuffChangedHandler optsdbus_stuff_changed_handler = NULL;
static OptsDBusSetProfileHandler optsdbus_set_profile_handler = NULL;
static OptsDBusSetProfileHandler optsdbus_set_colour_scheme_handler = NULL;
static OptsDBusSetProfileHandler optsdbus_set_shortcut_scheme_handler = NULL;

static gboolean optsdbus_set_profile_callback(OptsDBusSetProfileHandler handler,
        DBusMessage *message, DBusError *pderror)
{
    const char *id_str = NULL;
    const char *profile_name = NULL;

    gboolean result = dbus_message_get_args(message, pderror,
            DBUS_TYPE_STRING, &id_str,
            DBUS_TYPE_STRING, &profile_name,
            DBUS_TYPE_INVALID);
    if (result)
    {
        void *id = NULL;

        if (sscanf(id_str, "%p", &id) == 1)
        {
            handler(id, profile_name);
        }
        else
        {
            dlg_warning(NULL,
                    _("Unrecognised ROXTERM_ID '%s' in D-Bus message"), id_str);
        }
    }
#if ROXTERM_DBUS_OLD_ARGS_SEMANTICS
    if (profile_name)
        dbus_free(profile_name);
    if (id_str)
        dbus_free(id_str);
#endif
    return result;
}

static DBusHandlerResult
optsdbus_message_filter(DBusConnection * connection, DBusMessage * message,
    void *user_data)
{
    const char *profile_name = NULL;
    const char *key = NULL;
    OptsDBusValue val;
    char *dbus_str = NULL;
    const char *signal_name;
    gboolean args_result = TRUE;
    DBusError derror;
    OptsDBusOptType opt_type = OptsDBus_InvalidOpt;
    const char *what_happened = NULL;
    const char *family_name = NULL;
    const char *current_name = NULL;
    const char *new_name = NULL;

    (void) connection;
    (void) user_data;

    if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL ||
        strcmp(dbus_message_get_path(message), OPTSDBUS_OBJECT_PATH))
    {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    signal_name = dbus_message_get_member(message);
    if (strcmp(dbus_message_get_interface(message), OPTSDBUS_INTERFACE))
    {
        goto bad_signal;
    }

    dbus_error_init(&derror);
    if (!strcmp(signal_name, OPTSDBUS_STRING_SIGNAL))
    {
        args_result = dbus_message_get_args(message, &derror,
            DBUS_TYPE_STRING, &profile_name,
            DBUS_TYPE_STRING, &key,
            DBUS_TYPE_STRING, &dbus_str, DBUS_TYPE_INVALID);
        if (args_result)
        {
            opt_type = OptsDBus_StringOpt;
            val.s = dbus_str && dbus_str[0] ? dbus_str : NULL;
        }
    }
    else if (!strcmp(signal_name, OPTSDBUS_INT_SIGNAL))
    {
        args_result = dbus_message_get_args(message, &derror,
            DBUS_TYPE_STRING, &profile_name,
            DBUS_TYPE_STRING, &key,
            DBUS_TYPE_INT32, &val.i, DBUS_TYPE_INVALID);
        if (args_result)
            opt_type = OptsDBus_IntOpt;
    }
    else if (!strcmp(signal_name, OPTSDBUS_FLOAT_SIGNAL))
    {
        args_result = dbus_message_get_args(message, &derror,
            DBUS_TYPE_STRING, &profile_name,
            DBUS_TYPE_STRING, &key,
            DBUS_TYPE_DOUBLE, &val.f, DBUS_TYPE_INVALID);
        if (args_result)
            opt_type = OptsDBus_FloatOpt;
    }
    else if (!strcmp(signal_name, OPTSDBUS_DELETED)
             || !strcmp(signal_name, OPTSDBUS_ADDED)
             || !strcmp(signal_name, OPTSDBUS_RENAMED)
             || !strcmp(signal_name, OPTSDBUS_CHANGED))
    {
        what_happened = signal_name;
        if (strcmp(signal_name, OPTSDBUS_RENAMED))
        {
            args_result = dbus_message_get_args(message, &derror,
                DBUS_TYPE_STRING, &family_name,
                DBUS_TYPE_STRING, &current_name,
                DBUS_TYPE_INVALID);
        }
        else
        {
            args_result = dbus_message_get_args(message, &derror,
                DBUS_TYPE_STRING, &family_name,
                DBUS_TYPE_STRING, &current_name,
                DBUS_TYPE_STRING, &new_name,
                DBUS_TYPE_INVALID);
        }
    }
    else if (!strcmp(signal_name, OPTSDBUS_SET_PROFILE))
    {
        if (optsdbus_set_profile_handler)
        {
            args_result = optsdbus_set_profile_callback(
                    optsdbus_set_profile_handler, message, &derror);
        }
    }
    else if (!strcmp(signal_name, OPTSDBUS_SET_COLOUR_SCHEME))
    {
        if (optsdbus_set_colour_scheme_handler)
        {
            args_result = optsdbus_set_profile_callback(
                    optsdbus_set_colour_scheme_handler, message, &derror);
        }
    }
    else if (!strcmp(signal_name, OPTSDBUS_SET_SHORTCUT_SCHEME))
    {
        if (optsdbus_set_shortcut_scheme_handler)
        {
            args_result = optsdbus_set_profile_callback(
                    optsdbus_set_shortcut_scheme_handler, message, &derror);
        }
    }
    else
    {
        goto bad_signal;
    }
    if (!args_result)
    {
        rtdbus_whinge(&derror, _("Unable to read D-BUS signal arguments"));
    }
    else
    {
        if (profile_name && optsdbus_option_handler)
        {
            (*optsdbus_option_handler) (profile_name, key, opt_type, val);
        }
        if (family_name && optsdbus_stuff_changed_handler)
        {
            (*optsdbus_stuff_changed_handler) (what_happened,
                    family_name, current_name, new_name);
        }
    }

#if ROXTERM_DBUS_OLD_ARGS_SEMANTICS
    if (profile_name)
        dbus_free(profile_name);
    if (key)
        dbus_free(key);
    if (dbus_str)
        dbus_free(dbus_str);
    if (family_name)
        dbus_free(family_name);
    if (current_name)
        dbus_free(current_name);
    if (new_name)
        dbus_free(new_name);
#endif
    return DBUS_HANDLER_RESULT_HANDLED;
bad_signal:
    g_warning(_("Unrecognised D-BUS signal %s.%s"),
        dbus_message_get_interface(message), signal_name);
    /* Consider it handled, because nothing else should be handling
     * this object */
    return DBUS_HANDLER_RESULT_HANDLED;
}

static void optsdbus_enable_filter(void)
{
    static gboolean enabled = FALSE;

    if (enabled)
        return;
    enabled = TRUE;
    rtdbus_add_signal_rule_and_filter(
            OPTSDBUS_OBJECT_PATH, OPTSDBUS_INTERFACE,
            optsdbus_message_filter);
}

void optsdbus_listen_for_opt_signals(OptsDBusOptionHandler handler)
{
    g_return_if_fail(rtdbus_ok);
    optsdbus_enable_filter();
    optsdbus_option_handler = handler;
}

void optsdbus_listen_for_stuff_changed_signals(
        OptsDBusStuffChangedHandler handler)
{
    g_return_if_fail(rtdbus_ok);
    optsdbus_enable_filter();
    optsdbus_stuff_changed_handler = handler;
}

void optsdbus_listen_for_set_profile_signals(OptsDBusSetProfileHandler handler)
{
    g_return_if_fail(rtdbus_ok);
    optsdbus_enable_filter();
    optsdbus_set_profile_handler = handler;
}

void optsdbus_listen_for_set_colour_scheme_signals(
        OptsDBusSetProfileHandler handler)
{
    g_return_if_fail(rtdbus_ok);
    optsdbus_enable_filter();
    optsdbus_set_colour_scheme_handler = handler;
}

void optsdbus_listen_for_set_shortcut_scheme_signals(
        OptsDBusSetProfileHandler handler)
{
    g_return_if_fail(rtdbus_ok);
    optsdbus_enable_filter();
    optsdbus_set_shortcut_scheme_handler = handler;
}

gboolean optsdbus_send_edit_opts_message(const char *method, const char *arg)
{
    GError *error = NULL;
    char *appdir = global_options_appdir && global_options_appdir[0] ?
        g_strdup_printf("'--appdir=%s'", global_options_appdir) : NULL;
    char *bindir = global_options_bindir ?
        g_strdup_printf("%s%c", global_options_bindir, G_DIR_SEPARATOR) :
        NULL;
    char *param = arg ? g_strdup_printf("--%s=%s", method, arg) :
            g_strdup(method);
    char *command =  g_strdup_printf("%sroxterm-config %s '%s'",
                bindir ? bindir : "", appdir ? appdir : "", param);

    g_free(bindir);
    g_free(appdir);
    g_free(param);
    if (!g_spawn_command_line_async(command, &error))
    {
        dlg_critical(NULL, _("Error launching capplet: %s"), error->message);
        g_error_free(error);
        g_free(command);
        return FALSE;
    }
    g_free(command);
    return TRUE;
}

#endif /* !ROXTERM_CAPPLET */

/* vi:set sw=4 ts=4 noet cindent cino= */
