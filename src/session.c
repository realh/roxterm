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

#ifndef DEFNS_H
#include "defns.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

#include <X11/SM/SMlib.h>

#include <glib/gstdio.h>

#include "multitab.h"
#include "roxterm.h"
#include "session.h"
#include "session-file.h"

typedef struct {
    SmcConn smc_conn;
    IceConn ice_conn;
    GIOChannel *ioc;
    guint tag;
    char *client_id;
    SmProp *props[5];
    gboolean init_done;
} SessionData;

int session_argc;
char **session_argv;

static SessionData session_data;

static gboolean ioc_watch_callback(GIOChannel *source,
        GIOCondition cond, gpointer handle)
{
    SessionData *sd = handle;
    (void) source;
    (void) cond;

    if (IceProcessMessages(sd->ice_conn, NULL, NULL) ==
            IceProcessMessagesIOError)
    {
        g_warning(_("Error processing ICE (session management) messages"));
        SmcCloseConnection(sd->smc_conn, 0, NULL);
        sd->tag = 0;
        return FALSE;
    }
    return TRUE;
}

static void ice_watch_callback(IceConn conn, IcePointer handle,
        Bool opening, IcePointer *watch_data)
{
    SessionData *sd = handle;
    (void) watch_data;

    if (opening)
    {
        int fd = IceConnectionNumber(conn);

        SLOG("Opening ice watch callback on fd %d", fd);
        sd->ice_conn = conn;
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        sd->ioc = g_io_channel_unix_new(fd);
        sd->tag = g_io_add_watch(sd->ioc, G_IO_IN, ioc_watch_callback, sd);
    }
    else
    {
        SLOG("Closing ice watch callback");
        if (sd->tag)
        {
            g_source_remove(sd->tag);
            sd->tag = 0;
        }
        if (sd->ioc)
        {
            g_io_channel_unref(sd->ioc);
            sd->ioc = NULL;
        }
    }
}

/* g_strdup uses g_malloc/g_free which are not guaranteed to be compatible with
 * malloc/free so we need strdup or equivalent. Providing our own implementation
 * of strdup is probably easier than messing around with feature_test_macros
 * and/or automake tests.
 */
static char *h_strdup(const char *s)
{
    char *s2 = malloc(strlen(s) + 1);

    strcpy(s2, s);
    return s2;
}

inline static void fill_in_prop_val(SmPropValue *val, const char *s)
{
    val->length = strlen(s);
    val->value = (SmPointer) h_strdup(s);
}

static SmProp *make_string_list_propv_full(int num_vals, const char *name,
        char const * const *values, const char *prop_type)
{
    int n;
    SmProp *prop = malloc(sizeof(SmProp));

    prop->name = h_strdup(name);
    prop->type = h_strdup(prop_type);
    prop->num_vals = num_vals;
    prop->vals = malloc(sizeof(SmPropValue) * num_vals);
    for (n = 0; n < num_vals; ++n)
    {
        fill_in_prop_val(prop->vals + n, values[n]);
    }
    return prop;
}

inline static SmProp *make_string_list_propv(int num_vals, const char *name,
        char const * const *values)
{
    return make_string_list_propv_full(num_vals, name, values, SmLISTofARRAY8);
}

static SmProp *make_string_list_prop(int num_vals, const char *name,
        const char *s1, const char *s2, const char *s3)
{
    char const *values[3];

    values[0] = s1;
    if (num_vals >= 2)
        values[1] = s2;
    if (num_vals >= 3)
        values[2] = s3;
    return make_string_list_propv_full(num_vals, name, values, SmLISTofARRAY8);
}

inline static SmProp *make_string_list_prop1(const char *name, const char *s)
{
    return make_string_list_propv_full(1, name, &s, SmARRAY8);
}

/* If resclone is non-NULL add --restart/clone-session-id... */
static SmProp *make_start_command_prop(const char *name, const char *resclone,
        const char *client_id)
{
    SmProp *prop;

    if (resclone)
    {
        char const **argv;
        char *idarg = g_strdup_printf("--%s-session-id=%s",
                resclone, client_id);
        int n, m;

        argv = g_new(const char *, session_argc + 2);
        /* FIXME: Can be + 1 instead of + 2 if not logging */
        for (n = m = 0; n < session_argc; ++n)
        {
            if (!g_str_has_prefix(session_argv[n], "--restart-session-id=") &&
                !g_str_has_prefix(session_argv[n], "--clone-session-id="))
            {
                argv[m++] = session_argv[n];
            }
        }
        argv[m++] = idarg;
        argv[m] = NULL;
        char *cmd = g_strjoinv(" ", (char **) argv);
        SLOG("Generated start command for '%s': %s", name, cmd);
        g_free(cmd);
        prop = make_string_list_propv(m, name, argv);
        g_free(idarg);
    }
    else
    {
        char *cmd = g_strjoinv(" ", session_argv);
        SLOG("Generated start command for '%s': %s", name, cmd);
        g_free(cmd);
        prop = make_string_list_propv(session_argc, name,
                (char const * const *) session_argv);
    }
    return prop;
}

static void session_set_props(SessionData *sd, const char *filename)
{
    int n;
    SmProp **props = sd->props;

    for (n = 0; n < 5; ++n)
    {
        if (props[n])
            SmFreeProperty(props[n]);
    }
    props[0] = make_start_command_prop(SmCloneCommand,
            "clone", sd->client_id);
    props[1] = make_string_list_prop(3, SmDiscardCommand,
            "/bin/rm", "-f", filename);
    props[2] = make_string_list_prop1(SmProgram,
            session_argv[0]);
    props[3] = make_start_command_prop(SmRestartCommand,
            "restart", sd->client_id);
    props[4] = make_string_list_prop1(SmUserID,
            g_get_user_name());
    SmcSetProperties(sd->smc_conn, 5, props);
}

static void session_save_yourself_callback(SmcConn smc_conn, SmPointer handle,
        int save_type, Bool shutdown, int interact_style, Bool fast)
{
    SessionData *sd = handle;
    gboolean success = FALSE;
    (void) shutdown;
    (void) interact_style;
    (void) fast;

    if (!sd->init_done)
    {
        SLOG("Ignoring SaveYourself because we're initialising");
        /* An extra message is sent in response to initialising the connection
         * so we don't actually want to save state at that point.
         */
        sd->init_done = TRUE;
        success = TRUE;
    }
    else if (save_type == SmSaveGlobal)
    {
        SLOG("Ignoring Global SaveYourself");
        /* Only save state for SmSaveLocal/Both */
        success = TRUE;
    }
    else
    {
        char *filename = session_get_filename(sd->client_id, "Sessions", TRUE);

        SLOG("Implementing SaveYourself for %s", sd->client_id);
        if (filename)
        {
            success = save_session_to_file(filename, sd->client_id);
            if (success)
            {
                session_set_props(sd, filename);
            }
            else
            {
                g_warning(_("Failed to save session state to '%s': %s"),
                        filename, strerror(errno));
                g_unlink(filename);
            }
            g_free(filename);
        }
    }
    SmcSaveYourselfDone(smc_conn, success);
}

static void die_callback(SmcConn smc_conn, SmPointer handle)
{
    (void) handle;
    SLOG("die_callback");
    SmcCloseConnection(smc_conn, 0, NULL);
    /* FIXME: Should we try to reconnect if not in shutdown? */
}

static void save_complete_callback(SmcConn smc_conn, SmPointer handle)
{
    (void) smc_conn;
    (void) handle;
    SLOG("save_complete_callback");
}

static void shutdown_cancelled_callback(SmcConn smc_conn, SmPointer handle)
{
    (void) smc_conn;
    (void) handle;
    SLOG("shutdown_cancelled_callback");
}

void session_init(const char *client_id)
{
    SmcCallbacks callbacks = { {session_save_yourself_callback, &session_data},
            { die_callback, NULL },
            { save_complete_callback, NULL },
            { shutdown_cancelled_callback, NULL } };
    char error_s[256];

    SLOG("session_init(%s)", client_id);
    error_s[0] = 0;
    session_data.client_id = NULL;
    session_data.ioc = NULL;
    session_data.tag = 0;
    session_data.init_done = client_id != NULL;
    memset(session_data.props, 0, sizeof(SmProp *) * 5);

    if (!IceAddConnectionWatch(ice_watch_callback, &session_data))
    {
        g_warning(_("Unable to initialise ICE for session management"));
        return;
    }

    session_data.smc_conn = SmcOpenConnection(NULL, NULL, 1, 0,
            SmcSaveYourselfProcMask | SmcDieProcMask |
            SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask,
            &callbacks,
            (char *) client_id, &session_data.client_id,
            sizeof(error_s), error_s);
    if (!session_data.smc_conn)
    {
        g_warning(_("Failed to connect to session manager: %s"),
                error_s[0] ? error_s : _("unknown reason"));
        free(session_data.client_id);
        session_data.client_id = NULL;
    }
    SLOG("client_id is now %s", session_data.client_id);
}

gboolean session_load(const char *client_id)
{
    return load_session_from_file(
            session_get_filename(client_id, "Sessions", FALSE),
            client_id);
}
