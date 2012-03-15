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


#include <stdarg.h>
void
roxterm_sm_log(const char *format, ...)
{
    static FILE *fp = NULL;
    va_list ap;
    
    if (!fp)
    {
        char *n = g_build_filename(g_get_home_dir(), ".roxterm-sm-log", NULL);
        fp = fopen(n, "a");
        g_free(n);
        g_return_if_fail(fp != NULL);
        fprintf(fp, "\n*****************\nOpening new log\n");
    }
    va_start(ap, format);
    vfprintf(fp, format, ap);
    va_end(ap);
    fputc('\n', fp);
    fflush(fp);
}

typedef struct {
    SmcConn smc_conn;
    IceConn ice_conn;
    GIOChannel *ioc;
    guint tag;
    char *client_id;
    SmProp *props[5];
    gboolean init_done;
} SessionData;

static SessionData session_data;

int session_argc;
char **session_argv;

static char *session_get_filename(const char *client_id, gboolean create_dir)
{
    char *dir = g_build_filename(g_get_user_config_dir(), ROXTERM_LEAF_DIR,
            "Sessions", NULL);
    char *pathname;
    
    if (create_dir && !g_file_test(dir, G_FILE_TEST_IS_DIR))
    {
        if (g_mkdir_with_parents(dir, 0755))
        {
            g_warning(_("Unable to create Sessions directory '%s': %s"),
                    dir, strerror(errno));
            g_free(dir);
            return NULL;
        }
    }
    pathname = g_build_filename(dir, client_id, NULL);
    g_free(dir);
    return pathname;
}

static gboolean ioc_watch_callback(GIOChannel *source,
        GIOCondition cond, gpointer handle)
{
    SessionData *sd = handle;
    
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

static void save_tab_to_fp(MultiTab *tab, gpointer handle)
{
    FILE *fp = handle;
    ROXTermData *roxterm = multi_tab_get_user_data(tab);
    char const * const *commandv = roxterm_get_actual_commandv(roxterm);
    const char *name = multi_tab_get_window_title_template(tab);
    const char *title = multi_tab_get_window_title(tab);
    const char *icon_title = multi_tab_get_icon_title(tab);
    char *cwd = roxterm_get_cwd(roxterm);
    char *s = g_markup_printf_escaped("<tab profile='%s'\n"
            "        colour_scheme='%s' cwd='%s'\n"
            "        title_template='%s' window_title='%s' icon_title='%s'\n"
            "        title_template_locked='%d'\n"
            "        encoding='%s'",
            roxterm_get_profile_name(roxterm),
            roxterm_get_colour_scheme_name(roxterm),
            cwd ? cwd : (cwd = g_get_current_dir()),
            name ? name : "", title ? title : "", icon_title ? icon_title : "",
            multi_tab_get_title_template_locked(tab),
            vte_terminal_get_encoding(roxterm_get_vte_terminal(roxterm)));
            
    SLOG("Saving tab with window_title '%s', cwd %s", title, cwd);
    fprintf(fp, "    %s current='%d'%s>\n", s,
            tab == multi_win_get_current_tab(multi_tab_get_parent(tab)),
            commandv ? "" : " /");
    g_free(cwd);
    g_free(s);
    if (commandv)
    {
        int n;
        
        for (n = 0; commandv[n]; ++n);
        fprintf(fp, "      <command argc='%d'>\n", n);
        for (n = 0; commandv[n]; ++n)
        {
            s = g_markup_printf_escaped("        <arg s='%s' />\n",
                    commandv[n]);
            fputs(s, fp);
            g_free(s);
        }
        fputs("      </command>\n", fp);
        fputs("    </tab>\n", fp);
    }
}

static gboolean save_session_to_fp(SessionData *sd, FILE *fp)
{
    GList *wlink;
    
    SLOG("Saving session with id %s", sd->client_id);
    if (fprintf(fp, "<roxterm_session id='%s'>\n", sd->client_id) < 0)
        return FALSE;
    for (wlink = multi_win_all; wlink; wlink = g_list_next(wlink))
    {
        MultiWin *win = wlink->data;
        GtkWindow *gwin = GTK_WINDOW(multi_win_get_widget(win));
        int w, h;
        int x, y;
        int result;
        char *disp = gdk_screen_make_display_name(gtk_window_get_screen(gwin));
        const char *tt = multi_win_get_title_template(win);
        const char *title = multi_win_get_title(win);
        gpointer user_data = multi_win_get_user_data_for_current_tab(win);
        VteTerminal *vte;
        char *font_name;
        gboolean disable_menu_shortcuts, disable_tab_shortcuts;
        char *s;
        
        SLOG("Saving window with title '%s'", title);
        if (!user_data)
        {
            g_warning(_("Window with no user data"));
            continue;
        }
        vte = roxterm_get_vte_terminal(user_data);
        font_name = pango_font_description_to_string(
                vte_terminal_get_font(vte));
        multi_win_get_disable_menu_shortcuts(user_data,
                &disable_menu_shortcuts, &disable_tab_shortcuts);
        roxterm_get_nonfs_dimensions(user_data, &w, &h);
        gtk_window_get_position(gwin, &x, &y);
        s = g_markup_printf_escaped("  <window disp='%s'\n"
                "      geometry='%dx%d+%d+%d'\n"
                "      title_template='%s' font='%s'\n"
                "      title_template_locked='%d'\n"
                "      title='%s' role='%s'\n"
                "      shortcut_scheme='%s' show_menubar='%d'\n"
                "      always_show_tabs='%d' tab_pos='%d'\n"
                "      disable_menu_shortcuts='%d' disable_tab_shortcuts='%d'\n"
                "      maximised='%d' fullscreen='%d' zoom='%f'>\n",
                disp, w, h, x, y,
                tt ? tt : "", font_name,
                multi_win_get_title_template_locked(win),
                title, gtk_window_get_role(gwin),
                multi_win_get_shortcuts_scheme_name(win),
                multi_win_get_show_menu_bar(win),
                multi_win_get_always_show_tabs(win),
                multi_win_get_tab_pos(win),
                disable_menu_shortcuts, disable_tab_shortcuts,
                multi_win_is_maximised(win),
                multi_win_is_fullscreen(win),
                roxterm_get_zoom_factor(user_data));
        result = fputs(s, fp);
        g_free(s);
        g_free(disp);
        g_free(font_name);
        SLOG("Saved the window");
        if (result < 0)
        {
            SLOG("But it failed!");
            return FALSE;
        }
        multi_win_foreach_tab(win, save_tab_to_fp, fp);
        if (fprintf(fp, "  </window>\n") < 0)
            return FALSE;
    }
    return fprintf(fp, "</roxterm_session>\n") > 0;
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
        char *filename = session_get_filename(sd->client_id, TRUE);
        
        SLOG("Implementing SaveYourself for %s", sd->client_id);
        if (filename)
        {
            FILE *fp = fopen(filename, "w");
            
            if (fp)
            {
                success = save_session_to_fp(sd, fp);
                fclose(fp);
                if (!success)
                    g_unlink(filename);
            }
            if (success)
            {
                session_set_props(sd, filename);
            }
            else
            {
                g_warning(_("Failed to save session state to '%s': %s"),
                        filename, strerror(errno));
            }
            g_free(filename);
        }
    }
    SmcSaveYourselfDone(smc_conn, success);
}

static void die_callback(SmcConn smc_conn, SmPointer handle)
{
    SLOG("die_callback");
    SmcCloseConnection(smc_conn, 0, NULL);
    /* FIXME: Should we try to reconnect if not in shutdown? */
}

static void save_complete_callback(SmcConn smc_conn, SmPointer handle)
{
    SLOG("save_complete_callback");
}

static void shutdown_cancelled_callback(SmcConn smc_conn, SmPointer handle)
{
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
    GError *err = NULL;
    char *filename = session_get_filename(client_id, FALSE);
    char *buf;
    gsize buflen;
    gboolean result;
    
    SLOG("Loading session %s", client_id);
    if (!g_file_get_contents(filename, &buf, &buflen, &err))
    {
        g_warning(_("Unable to load session '%s': %s"), client_id,
                err->message);
        SLOG("Unable to load session '%s': %s", client_id, err->message);
        g_error_free(err);
        return FALSE;
    }
    result = roxterm_load_session(buf, buflen, client_id);
    g_free(buf);
    return result;
}
