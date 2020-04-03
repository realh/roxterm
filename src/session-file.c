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

#include "multitab.h"
#include "roxterm.h"
#include "session-file.h"

/*
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
*/

char *session_get_filename(const char *leafname, const char *subdir,
        gboolean create_dir)
{
    char *dir = g_build_filename(g_get_user_config_dir(), ROXTERM_LEAF_DIR,
            subdir, NULL);
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
    pathname = g_build_filename(dir, leafname, NULL);
    g_free(dir);
    return pathname;
}

static void save_tab_to_fp(MultiTab *tab, gpointer handle)
{
    FILE *fp = handle;
    ROXTermData *roxterm = multi_tab_get_user_data(tab);
    char const * const *commandv = roxterm_get_actual_commandv(roxterm);
    const char *name = multi_tab_get_window_title_template(tab);
    const char *title = multi_tab_get_window_title(tab);
    char *cwd = roxterm_get_cwd(roxterm);
    char *s = g_markup_printf_escaped("<tab profile='%s'\n"
            "        colour_scheme='%s' cwd='%s'\n"
            "        title_template='%s' window_title='%s'\n"
            "        title_template_locked='%d'",
            roxterm_get_profile_name(roxterm),
            roxterm_get_colour_scheme_name(roxterm),
            cwd ? cwd : (cwd = g_get_current_dir()),
            name ? name : "", title ? title : "",
            multi_tab_get_title_template_locked(tab));

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

static gboolean save_session_to_fp(FILE *fp, const char *session_id)
{
    GList *wlink;

    SLOG("Saving session with id %s", sd->client_id);
    if (fprintf(fp, "<roxterm_session id='%s'>\n", session_id) < 0)
        return FALSE;
    for (wlink = multi_win_all; wlink; wlink = g_list_next(wlink))
    {
        MultiWin *win = wlink->data;
        GtkWindow *gwin = GTK_WINDOW(multi_win_get_widget(win));
        int w, h;
        int x, y;
        int result;
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
        s = g_markup_printf_escaped("  <window geometry='%dx%d+%d+%d'\n"
                "      title_template='%s' font='%s'\n"
                "      title_template_locked='%d'\n"
                "      title='%s' role='%s'\n"
                "      shortcut_scheme='%s' show_menubar='%d'\n"
                "      always_show_tabs='%d' tab_pos='%d'\n"
                "      show_add_tab_btn='%d'\n"
                "      disable_menu_shortcuts='%d' disable_tab_shortcuts='%d'\n"
                "      maximised='%d' fullscreen='%d' borderless='%d' zoom='%f'>\n",
                w, h, x, y,
                tt ? tt : "", font_name,
                multi_win_get_title_template_locked(win),
                title, gtk_window_get_role(gwin),
                multi_win_get_shortcuts_scheme_name(win),
                multi_win_get_show_menu_bar(win),
                multi_win_get_always_show_tabs(win),
                multi_win_get_tab_pos(win),
                multi_win_get_show_add_tab_button(win),
                disable_menu_shortcuts, disable_tab_shortcuts,
                multi_win_is_maximised(win),
                multi_win_is_fullscreen(win),
                multi_win_is_borderless(win),
                roxterm_get_zoom_factor(user_data));
        result = fputs(s, fp);
        g_free(s);
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

gboolean save_session_to_file(const char *filename, const char *id)
{
    gboolean result;
    FILE *fp = fopen(filename, "w");

    if (!fp)
    {
        SLOG("Failed to open '%s' to save session: %s",
                filename, strerror(errno));
        return FALSE;
    }
    result = save_session_to_fp(fp, id);
    if (!result)
    {
        SLOG("Failed to save session to '%s': %s", filename, strerror(errno));
    }
    fclose(fp);
    return result;
}

gboolean load_session_from_file(const char *filename, const char *client_id)
{
    GError *err = NULL;
    char *buf;
    gsize buflen;
    gboolean result;

    SLOG("Loading session from '%s'", filename);
    if (!g_file_get_contents(filename, &buf, &buflen, &err))
    {
        g_warning(_("Unable to load session from '%s': %s"), filename,
                err->message);
        SLOG("Unable to load session from '%s': %s", filename, err->message);
        g_error_free(err);
        return FALSE;
    }
    result = roxterm_load_session(buf, buflen, client_id);
    g_free(buf);
    return result;
}
