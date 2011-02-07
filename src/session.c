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

typedef struct {
    SmcConn smc_conn;
    IceConn ice_conn;
    GIOChannel *ioc;
    guint tag;
    char *client_id;
    SmProp *props[5];
} SessionData;

static SessionData session_data;

const char *session_arg0;

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
        
        sd->ice_conn = conn;
        fcntl(fd, F_SETFD, FD_CLOEXEC);
        sd->ioc = g_io_channel_unix_new(fd);
        sd->tag = g_io_add_watch(sd->ioc, G_IO_IN, ioc_watch_callback, sd);
    }
    else
    {
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
            cwd ? cwd = g_strdup(cwd) : (cwd = g_get_current_dir()),
            name ? name : "", title ? title : "", icon_title ? icon_title : "",
            multi_tab_get_title_template_locked(tab),
            vte_terminal_get_encoding(roxterm_get_vte_terminal(roxterm)));
            
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
        const char *xname, *xclass;
        
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
        multi_win_get_class_name(win, &xclass, &xname);
        s = g_markup_printf_escaped("  <window disp='%s'\n"
                "      geometry='%dx%d+%d+%d'\n"
                "      title_template='%s' font='%s'\n"
                "      title_template_locked='%d'\n"
                "      title='%s' role='%s'\n"
                "      xclass='%s' xname='%s'\n"
                "      shortcut_scheme='%s' show_menubar='%d'\n"
                "      always_show_tabs='%d' tab_pos='%d'\n"
                "      disable_menu_shortcuts='%d' disable_tab_shortcuts='%d'\n"
                "      maximised='%d' fullscreen='%d' zoom='%f'>\n",
                disp, w, h, x, y,
                tt ? tt : "", font_name,
                multi_win_get_title_template_locked(win),
                title, gtk_window_get_role(gwin),
                xclass ? xclass : "", xname ? xname : "",
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
        if (result < 0)
            return FALSE;
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

static SmProp *make_string_list_prop(int num_vals, const char *name,
        const char *s1, const char *s2, const char *s3)
{
    SmProp *prop = malloc(sizeof(SmProp));
    
    prop->name = h_strdup(name);
    if (num_vals == 1)
        prop->type = h_strdup(SmARRAY8);
    else
        prop->type = h_strdup(SmLISTofARRAY8);
    prop->num_vals = num_vals;
    prop->vals = malloc(sizeof(SmPropValue) * num_vals);
    fill_in_prop_val(prop->vals, s1);
    if (num_vals >= 2)
        fill_in_prop_val(prop->vals + 1, s2);
    if (num_vals >= 3)
        fill_in_prop_val(prop->vals + 2, s3);
    return prop;
}

inline static SmProp *make_start_command_prop(const char *name,
        const char *opt, const char *client_id)
{
    char *idarg = g_strdup_printf("--%s-session-id=%s", opt, client_id);
    SmProp *prop = make_string_list_prop(2, name, session_arg0, idarg, NULL);
    
    g_free(idarg);
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
    props[0] = make_string_list_prop(1, SmCloneCommand,
            session_arg0, NULL, NULL);
    props[1] = make_string_list_prop(3, SmDiscardCommand,
            "/bin/rm", "-f", filename);
    props[2] = make_string_list_prop(1, SmProgram,
            session_arg0, NULL, NULL);
    props[3] = make_start_command_prop(SmRestartCommand,
            "restart", sd->client_id);
    props[4] = make_string_list_prop(1, SmUserID,
            g_get_user_name(), NULL, NULL);
    SmcSetProperties(sd->smc_conn, 5, props);
}

static void session_save_yourself_callback(SmcConn smc_conn, SmPointer handle,
        int save_type, Bool shutdown, int interact_style, Bool fast)
{
    SessionData *sd = handle;
    char *filename = session_get_filename(sd->client_id, TRUE);
    gboolean success = FALSE;
    
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
    SmcSaveYourselfDone(smc_conn, success);
}

static void die_callback(SmcConn smc_conn, SmPointer handle)
{
    SmcCloseConnection(smc_conn, 0, NULL);
}

static void save_complete_callback(SmcConn smc_conn, SmPointer handle)
{
}

static void shutdown_cancelled_callback(SmcConn smc_conn, SmPointer handle)
{
}

void session_init(const char *client_id)
{
    SmcCallbacks callbacks = { {session_save_yourself_callback, &session_data},
            { die_callback, NULL },
            { save_complete_callback, NULL },
            { shutdown_cancelled_callback, NULL } };
    char error_s[256];
            
    error_s[0] = 0;
    session_data.client_id = NULL;
    session_data.ioc = NULL;
    session_data.tag = 0;
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
}

gboolean session_load(const char *client_id)
{
    GError *err = NULL;
    char *filename = session_get_filename(client_id, FALSE);
    char *buf;
    gsize buflen;
    gboolean result;
    
    if (!g_file_get_contents(filename, &buf, &buflen, &err))
    {
        g_warning(_("Unable to load session '%s': %s"), client_id,
                err->message);
        g_error_free(err);
        return FALSE;
    }
    result = roxterm_load_session(buf, buflen, client_id);
    g_free(buf);
    return result;
}
