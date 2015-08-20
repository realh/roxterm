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


#include "defns.h"

#include <stdarg.h>
#include <string.h>

#include "dlg.h"
#include "rtdbus.h"

DBusConnection  *rtdbus_connection;
DBusGConnection *rtdbus_g_connection;

gboolean rtdbus_ok = FALSE;

void rtdbus_whinge(DBusError * pderror, const char *s)
{
    dlg_critical(NULL, "%s: %s", s,
        dbus_error_is_set(pderror) ? pderror->message : _("<unknown>"));
    if (dbus_error_is_set(pderror))
        dbus_error_free(pderror);
}

void rtdbus_warn(DBusError * pderror, const char *s)
{
    g_warning("%s: %s", s,
        dbus_error_is_set(pderror) ? pderror->message : _("<unknown>"));
    if (dbus_error_is_set(pderror))
        dbus_error_free(pderror);
}

static void rtdbus_shutdown(void)
{
    if (rtdbus_connection)
    {
        UNREF_LOG(dbus_connection_unref(rtdbus_connection));
        rtdbus_connection = NULL;
        rtdbus_g_connection = NULL;
    }
}

gboolean rtdbus_send_message(DBusMessage *message)
{
    dbus_uint32_t serial = 0;

    if (!dbus_connection_send(rtdbus_connection, message, &serial))
    {
        g_warning(_("Unable to send D-BUS message"));
        UNREF_LOG(dbus_message_unref(message));
        return FALSE;
    }
    UNREF_LOG(dbus_message_unref(message));
    return TRUE;
}

gboolean rtdbus_send_message_with_reply(DBusMessage *message)
{
    DBusError derror;
    DBusMessage *reply;
    gboolean result = TRUE;
    const char *reply_msg = NULL;

    dbus_error_init(&derror);
    reply = dbus_connection_send_with_reply_and_block(rtdbus_connection,
            message, -1, &derror);
    UNREF_LOG(dbus_message_unref(message));
    if (!reply)
    {
        rtdbus_whinge(&derror, _("Unable to send/get reply to D-BUS message"));
        return FALSE;
    }
    if (!dbus_message_get_args(reply, &derror, DBUS_TYPE_STRING, &reply_msg,
            DBUS_TYPE_INVALID))
    {
        rtdbus_whinge(&derror, _("Unable to get D-BUS reply message"));
        result = FALSE;
    }
    if (strcmp(reply_msg, "OK"))
    {
        result = FALSE;
        g_warning("%s", reply_msg);
    }
    UNREF_LOG(dbus_message_unref(reply));
    return result;
}

gboolean rtdbus_start_service(const char *name, const char *object_path,
        DBusObjectPathMessageFunction method_handler, gboolean replace)
{
    DBusError derror;
    DBusObjectPathVTable vtable = {
        NULL, method_handler, NULL, NULL, NULL, NULL
    };
    guint flags;
    int result;

    dbus_error_init(&derror);
    flags = DBUS_NAME_FLAG_ALLOW_REPLACEMENT | DBUS_NAME_FLAG_DO_NOT_QUEUE;
    if (replace)
        flags |= DBUS_NAME_FLAG_REPLACE_EXISTING;
    result = dbus_bus_request_name(rtdbus_connection, name, flags, &derror);
    switch (result)
    {
        case -1:
            rtdbus_whinge(&derror, _("Unable to start D-BUS service"));
            rtdbus_shutdown();
            break;
        case DBUS_REQUEST_NAME_REPLY_EXISTS:
            return TRUE;
        default:
            if (!dbus_connection_register_object_path(rtdbus_connection, 
                    object_path, &vtable, NULL))
            {
                /* Currently complaining about handler already being registered
                 * and returning FALSE without setting derror, but still seems
                 * to work OK, so ignore FALSE if derror not set. */
                if (dbus_error_is_set(&derror))
                {
                    rtdbus_whinge(&derror,
                            _("Unable to listen for D-BUS method calls"));
                    rtdbus_shutdown();
                }
            }
            break;
    }
    return FALSE;
}

gboolean rtdbus_init(void)
{
    static gboolean already = FALSE;
    static gboolean status = FALSE;
    GError *err = NULL;

    if (already)
        return status;
    already = TRUE;
    rtdbus_g_connection = dbus_g_bus_get(DBUS_BUS_SESSION, &err);
    if (err)
    {
        dlg_critical(NULL, _("Error connecting to dbus: %s"), err->message);
        g_error_free(err);
        return status = FALSE;
    }
    rtdbus_connection = dbus_g_connection_get_connection(rtdbus_g_connection);

    /* We don't want to die if dbus dies... */
    dbus_connection_set_exit_on_disconnect(rtdbus_connection, FALSE);

    return status = TRUE;
}

gboolean rtdbus_add_rule_and_filter(const char *match_rule,
        DBusHandleMessageFunction filter_fn, void *user_data)
{
    DBusError derror;

    dbus_error_init(&derror);
    dbus_bus_add_match(rtdbus_connection, match_rule, &derror);
    if (dbus_error_is_set(&derror))
    {
        rtdbus_whinge(&derror, _("Unable to add D-BUS match rule"));
        return FALSE;
    }
    if (!dbus_connection_add_filter(rtdbus_connection,
            filter_fn, user_data, NULL))
    {
        rtdbus_whinge(&derror, _("Unable to install D-BUS message filter"));
        return FALSE;
    }
    return TRUE;
}

gboolean rtdbus_add_signal_rule_and_filter(
        const char *path, const char *interface,
        DBusHandleMessageFunction filter_fn)
{
    char *match_rule = g_strdup_printf(
            "type='signal',path='%s',interface='%s'",
            path, interface);
    gboolean result = rtdbus_add_rule_and_filter(match_rule, filter_fn, NULL);
    g_free(match_rule);
    return result;
}

DBusMessage *rtdbus_append_args(DBusMessage *message, int first_arg_type, ...)
{
    va_list ap;

    if (first_arg_type == DBUS_TYPE_INVALID)
        return message;
    va_start(ap, first_arg_type);
    message = rtdbus_append_args_valist(message, first_arg_type, ap);
    va_end(ap);
    return message;
}

DBusMessage *rtdbus_append_args_valist(DBusMessage *message, 
        int first_arg_type, va_list ap)
{
    if (first_arg_type == DBUS_TYPE_INVALID)
        return message;
    if (!dbus_message_append_args_valist(message, first_arg_type, ap))
    {
        UNREF_LOG(dbus_message_unref(message));
        message = NULL;
        g_critical(_("Unable to append arguments to D-BUS message"));
    }
    return message;
}

DBusMessage *rtdbus_signal_new(const char *object_path, const char *interface,
        const char *signal_name, int first_arg_type, ...)
{
    DBusMessage *message;
    DBusError derror;

    /* No point in doing anything if D-BUS has broken */
    if (!rtdbus_connection)
        return NULL;
    dbus_error_init(&derror);
    message = dbus_message_new_signal(object_path, interface, signal_name);
    if (message)
    {
        va_list ap;

        va_start(ap, first_arg_type);
        message = rtdbus_append_args_valist(message, first_arg_type, ap);
        va_end(ap);
    }
    else
    {
        rtdbus_whinge(&derror, _("Unable to create D-BUS signal message"));
    }
    return message;
}

DBusMessage *rtdbus_method_new(
        const char *bus_name, const char *object_path, const char *interface,
        const char *method_name, int first_arg_type, ...)
{
    DBusMessage *message;
    DBusError derror;

    /* No point in doing anything if D-BUS has broken */
    if (!rtdbus_connection)
        return NULL;
    dbus_error_init(&derror);
    message = dbus_message_new_method_call(bus_name, object_path, interface,
            method_name);
    if (message)
    {
        va_list ap;

        va_start(ap, first_arg_type);
        message = rtdbus_append_args_valist(message, first_arg_type, ap);
        va_end(ap);
    }
    else
    {
        rtdbus_whinge(&derror, _("Unable to create D-BUS method call message"));
    }
    return message;
}

char **rtdbus_get_message_args_as_strings(DBusMessageIter *iter)
{
    char **argv = NULL;
    int len = 0;
    int argc = 0;
    int argtype;

    for (; (argtype = dbus_message_iter_get_arg_type(iter)) 
            != DBUS_TYPE_INVALID;
            dbus_message_iter_next(iter))
    {
        const char *arg;

        if (argtype != DBUS_TYPE_STRING)
        {
            g_critical(_("Invalid argument type ('%c') in "
                        "NewTerminal D-BUS method"), argtype);
            continue;
        }
        if (argc + 1 >= len)
            argv = g_renew(char *, argv, len = len ? len * 2 : 2);
        dbus_message_iter_get_basic(iter, &arg);
        /* Check in case arg may be NULL string (probably not) */
        if (!arg)
        {
            argv[argc] = g_strdup("");
        }
        else
        {
            /* Always g_strdup in case dbus' memory management was incompatible
             * with glib's */
            argv[argc] = g_strdup(arg);
        }
        argv[++argc] = NULL;
    }
    return argv;
}

char **rtdbus_get_message_arg_string_array(DBusMessageIter *iter)
{
    int t;
    DBusMessageIter sub_iter;
    
    if ((t = dbus_message_iter_get_arg_type(iter)) != DBUS_TYPE_ARRAY)
    {
        g_critical(_("Invalid first argument type ('%c') in "
                    "NewTerminal D-BUS method; expected array"), t);
        return NULL;
    }
    if ((t = dbus_message_iter_get_element_type(iter)) != DBUS_TYPE_STRING)
    {
        g_critical(_("Invalid array element type ('%c') in "
                    "NewTerminal D-BUS method; expected strings"), t);
        return NULL;
    }
    dbus_message_iter_recurse(iter, &sub_iter);
    dbus_message_iter_next(iter);
    return rtdbus_get_message_args_as_strings(&sub_iter);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
