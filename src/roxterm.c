/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2018 Tony Houghton <h@realh.co.uk>

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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vte/vte.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "about.h"
#include "colourscheme.h"
#include "dlg.h"
#include "dragrcv.h"
#include "dynopts.h"
#include "globalopts.h"
#include "optsfile.h"
#include "optsdbus.h"
#include "roxterm.h"
#include "multitab.h"
#include "roxterm-regex.h"
#include "search.h"
#include "session-file.h"
#include "shortcuts.h"
#include "uri.h"
#include "resources.h"

#if VTE_CHECK_VERSION(0,50,0)
/* EK says hyperlinks can cause segfaults in 0.50.0 */
inline static gboolean roxterm_enable_hyperlinks()
{
    return vte_get_major_version() > 0 || vte_get_minor_version() > 50 ||
        vte_get_micro_version() >= 1;
    /* Note no need to check minor_version == 50 in final clause because this
     * has already been checked by #if.
     */
}
#endif

typedef enum {
    Roxterm_ChildExitClose,
    Roxterm_ChildExitHold,
    Roxterm_ChildExitRespawn,
    Roxterm_ChildExitAsk,

    Roxterm_ChildExitNotOverridden
} RoxtermChildExitAction;


typedef struct {
    int tag;
    ROXTerm_MatchType type;
} ROXTerm_MatchMap;

struct ROXTermData {
    /* We do not own references to tab or widget */
    MultiTab *tab;
    GtkWidget *widget;            /* VteTerminal */
    pid_t pid;
    /* We own a reference to colour_scheme */
    Options *colour_scheme;
    char *matched_url;
    ROXTerm_MatchType match_type;
    Options *profile;
    char *special_command;      /* Next window/tab opened from this one should
                                   run this command instead of default */
    char **commandv;            /* Copied from --execute args */
    char **actual_commandv;     /* The actual command used */
    char *directory;            /* Copied from global_options_directory */
    GArray *match_map;
    gboolean no_respawn;
    gboolean running;
    DragReceiveData *drd;

    gboolean hold_over_uri;    /* Whether we've just received a button press
                               over a URI which may turn into a drag or click
                                */
    double hold_x, hold_y;     /* Position of event above */
    gulong hold_handler_id;    /* Signal handler id for the above event */
    double target_zoom_factor, current_zoom_factor;
    int zoom_index;
    gulong post_exit_tag;
    const char *status_icon_name;
    gboolean borderless;
    gboolean maximise;
    gulong win_state_changed_tag;
    GtkWidget *replace_task_dialog;
    gboolean postponed_free;
    PangoFontDescription *pango_desc;
    gboolean dont_lookup_dimensions;
    char *reply;
    int columns, rows;
    char **env;
    char *search_pattern;
    guint search_flags;
    /*int file_match_tag[2];*/
    gboolean from_session;
    int padding_w, padding_h;
    gboolean is_shell;
    gulong child_exited_tag;
    RoxtermChildExitAction exit_action;
    gboolean override_exit_action;
};

#define PROFILE_NAME_KEY "roxterm_profile_name"

static GList *roxterm_terms = NULL;

static DynamicOptions *roxterm_profiles = NULL;

static void roxterm_apply_profile(ROXTermData * roxterm, VteTerminal * vte,
        gboolean update_geometry);

inline static MultiWin *roxterm_get_win(ROXTermData *roxterm)
{
    return roxterm->tab ? multi_tab_get_parent(roxterm->tab) : NULL;
}

/*********************** URI handling ***********************/

static int roxterm_match_add(ROXTermData *roxterm, VteTerminal *vte,
        const char *match, ROXTerm_MatchType type)
{
    ROXTerm_MatchMap map;
    VteRegex *regex;
    GError *err = NULL;

    regex = vte_regex_new_for_match(match, -1, PCRE2_MULTILINE, &err);
    if (!regex || err)
    {
        g_warning("Failed to compile regex '%s': %s",
                match, err ? err->message : "");
        return -1;
    }
    map.type = type;
    map.tag = vte_terminal_match_add_regex(vte, regex, 0);
    vte_terminal_match_set_cursor_name(vte, map.tag, "pointer");
    g_array_append_val(roxterm->match_map, map);
    return map.tag;
}

/*
static void roxterm_match_remove(ROXTermData *roxterm, VteTerminal *vte,
        int tag)
{
    guint n;

    vte_terminal_match_remove(vte, tag);
    for (n = 0; n < roxterm->match_map->len; ++n)
    {
        if (g_array_index(roxterm->match_map, ROXTerm_MatchMap, n).tag == tag)
        {
            g_array_remove_index_fast(roxterm->match_map, n);
            break;
        }
    }
}
*/

static void roxterm_add_matches(ROXTermData *roxterm, VteTerminal *vte)
{
    int n;

#if VTE_CHECK_VERSION(0,50,0)
    vte_terminal_set_allow_hyperlink(vte, roxterm_enable_hyperlinks());
#endif

    for (n = 0; roxterm_regexes[n].regex; ++n)
    {
        roxterm_match_add(roxterm, vte, roxterm_regexes[n].regex,
                roxterm_regexes[n].match_type);
    }
}

/*
static void roxterm_add_file_matches(ROXTermData *roxterm, VteTerminal *vte)
{
    int n;

    for (n = 0; n < 2; ++n)
    {
        roxterm->file_match_tag[n] =
            roxterm_match_add(roxterm, vte, file_urls[n], ROXTerm_Match_File);
    }
}

static void roxterm_remove_file_matches(ROXTermData *roxterm, VteTerminal *vte)
{
    int n;

    for (n = 0; n < 2; ++n)
    {
        roxterm_match_remove(roxterm, vte, roxterm->file_match_tag[n]);
        roxterm->file_match_tag[n] = -1;
    }
}
*/

static ROXTerm_MatchType roxterm_get_match_type(ROXTermData *roxterm, int tag)
{
    guint n;

    for (n = 0; n < roxterm->match_map->len; ++n)
    {
        ROXTerm_MatchMap *m = &g_array_index(roxterm->match_map,
                ROXTerm_MatchMap, n);

        if (m->tag == tag)
            return m->type;
    }
    return ROXTerm_Match_Invalid;
}

/********************* End URI handling *********************/

inline static DynamicOptions *roxterm_get_profiles(void)
{
    if (!roxterm_profiles)
        roxterm_profiles = dynamic_options_get("Profiles");
    return roxterm_profiles;
}

/* Foreground (and palette?) colour change doesn't cause a redraw so force it */
inline static void roxterm_force_redraw(ROXTermData *roxterm)
{
    if (roxterm->widget)
        gtk_widget_queue_draw(roxterm->widget);
}

char *roxterm_get_cwd(ROXTermData *roxterm)
{
    char *pidfile = NULL;
    char *target = NULL;
    GError *error = NULL;

    if (roxterm->pid < 0)
        return NULL;

    pidfile = g_strdup_printf("/proc/%d/cwd", roxterm->pid);
    target = g_file_read_link(pidfile, &error);
    /* According to a comment in gnome-terminal readlink()
     * returns a NULL/empty string in Solaris but we can still use the
     * /proc link if it exists */
    if (!target || !target[0])
    {
        g_free(target);
        target = NULL;
        if (!error && g_file_test(pidfile, G_FILE_TEST_EXISTS))
            target = pidfile;
        else
            g_free(pidfile);
    }
    else
    {
        g_free(pidfile);
    }
    if (error)
        g_error_free(error);
    return target;
}

static char **roxterm_strv_copy(char **src)
{
    int n;
    char **dest;

    for (n = 0; src[n]; ++n);
    dest = g_new(char *, n + 1);
    for (n = 0; src[n]; ++n)
        dest[n] = g_strdup(src[n]);
    dest[n] = 0;
    return dest;
}

static ROXTermData *roxterm_data_clone(ROXTermData *old_gt)
{
    ROXTermData *new_gt = g_new(ROXTermData, 1);

    *new_gt = *old_gt;
    new_gt->status_icon_name = NULL;
    new_gt->widget = NULL;
    new_gt->tab = NULL;
    new_gt->running = FALSE;
    new_gt->postponed_free = FALSE;
    new_gt->dont_lookup_dimensions = FALSE;
    new_gt->actual_commandv = NULL;
    new_gt->env = roxterm_strv_copy(old_gt->env);
    new_gt->child_exited_tag = 0;
    new_gt->post_exit_tag = 0;
    new_gt->win_state_changed_tag = 0;

    if (old_gt->colour_scheme)
    {
        new_gt->colour_scheme = colour_scheme_lookup_and_ref
            (options_get_leafname(old_gt->colour_scheme));
    }
    if (old_gt->profile)
    {
        new_gt->profile = dynamic_options_lookup_and_ref(roxterm_profiles,
            options_get_leafname(old_gt->profile), "roxterm profile");
    }
    /* special_command should be transferred to new data and removed from old
     * one */
    old_gt->special_command = NULL;

    /* Only inherit commandv from templates, not from running terminals */
    if (!old_gt->running && old_gt->commandv)
    {
        new_gt->commandv = global_options_copy_strv(old_gt->commandv);
    }
    else
    {
        new_gt->commandv = NULL;
    }
    new_gt->directory = roxterm_get_cwd(old_gt);
    if (!new_gt->directory && old_gt->directory)
    {
        new_gt->directory = g_strdup(old_gt->directory);
    }

    new_gt->match_map = g_array_new(FALSE, FALSE, sizeof(ROXTerm_MatchMap));
    new_gt->matched_url = NULL;

    if (old_gt->pango_desc)
    {
        new_gt->pango_desc = pango_font_description_copy(old_gt->pango_desc);
    }
    if (old_gt->widget && gtk_widget_get_realized(old_gt->widget))
    {
        VteTerminal *vte = VTE_TERMINAL(old_gt->widget);

        new_gt->columns = vte_terminal_get_column_count(vte);
        new_gt->rows = vte_terminal_get_row_count(vte);
    }
    /*new_gt->file_match_tag[0] = new_gt->file_match_tag[1] = -1;*/
    if (new_gt->override_exit_action)
        new_gt->override_exit_action = FALSE;
    else
        new_gt->exit_action = Roxterm_ChildExitNotOverridden;

    return new_gt;
}

static GHashTable *roxterm_hash_env(char **env)
{
    int n;
    GHashTable *env_table = g_hash_table_new_full(g_str_hash, g_str_equal,
            g_free, g_free);

    for (n = 0; env[n]; ++n)
    {
        const char *eq = strchr(env[n], '=');
        
        g_hash_table_replace(env_table, eq ?
                g_strndup(env[n], eq - env[n]) : g_strdup(env[n]),
                eq ? g_strdup(eq + 1) : NULL);
    }
    return env_table;
}

static char **roxterm_envv_from_hash(GHashTable *hash)
{
    GHashTableIter it;
    gsize len = g_hash_table_size(hash);
    char **env = g_new(char *, len + 1);
    char *key, *val;
    int n = 0;

    g_hash_table_iter_init(&it, hash);
    while (g_hash_table_iter_next(&it, (gpointer *) &key, (gpointer *) &val))
    {
        env[n++] = g_strdup_printf("%s=%s", key, val);
    }
    env[n] = NULL;
    return env;
}

static char **roxterm_get_environment(ROXTermData *roxterm, const char *term)
{
    GHashTable *env = roxterm_hash_env(roxterm->env);
    char **envv;

    if (term)
        g_hash_table_replace(env, g_strdup("TERM"), g_strdup(term));
    else
        g_hash_table_remove(env, "TERM");
    g_hash_table_replace(env, g_strdup("ROXTERM_ID"),
            g_strdup_printf("%p", roxterm));
    g_hash_table_replace(env, g_strdup("ROXTERM_NUM"),
            g_strdup_printf("%d", g_list_length(roxterm_terms)));
    g_hash_table_replace(env, g_strdup("ROXTERM_PID"),
            g_strdup_printf("%d", (int) getpid()));

#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default()))
    {
        MultiWin *mwin = roxterm_get_win(roxterm);
        if (mwin)
        {
            GtkWidget *widget = multi_win_get_widget(mwin);
            if (widget && gtk_widget_is_toplevel(widget))
            {
                Window xid = gdk_x11_window_get_xid(
                                gtk_widget_get_window(widget));
                g_hash_table_replace(env, g_strdup("WINDOWID"),
                        g_strdup_printf("%ld", xid));
            }

        }
    }
#endif

    g_hash_table_remove(env, "COLORTERM");
    g_hash_table_remove(env, "COLUMNS");
    g_hash_table_remove(env, "LINES");
    g_hash_table_remove(env, "VTE_VERSION");
    /* gnome-terminal also removes GNOME_DESKTOP_ICON, probably best not to do
     * the same without knowing why. */
    /* g_hash_table_remove(env, "GNOME_DESKTOP_ICON"); */

    envv = roxterm_envv_from_hash(env);
    g_hash_table_unref(env);
    return envv;
}

static GtkWindow *roxterm_get_toplevel(ROXTermData *roxterm)
{
    MultiWin *w;

    if (roxterm && (w = roxterm_get_win(roxterm)) != NULL)
    {
        GtkWidget *tl = multi_win_get_widget(w);
        if (tl && gtk_widget_is_toplevel(tl))
            return GTK_WINDOW(tl);
    }
    return NULL;
}

static char *get_default_command(ROXTermData *roxterm)
{
    char *command = NULL;
    struct passwd *pinfo = getpwuid(getuid());

    if (pinfo)
    {
        command = g_strdup(pinfo->pw_shell);
    }
    else
    {
        dlg_warning(roxterm_get_toplevel(roxterm),
                _("Unable to look up login details: %s"), strerror(errno));
    }
    if (!command || !command[0])
        command = g_strdup("/bin/sh");
    return command;
}

static void roxterm_show_status(ROXTermData *roxterm, const char *name)
{
    if (name && !strcmp(name, "dialog-information") &&
            roxterm->status_icon_name &&
            (!strcmp(roxterm->status_icon_name, "dialog-warning") ||
            !strcmp(roxterm->status_icon_name, "dialog_error")))

    {
        return;
    }
    roxterm->status_icon_name = name;
    if (roxterm->tab && options_lookup_int_with_default(roxterm->profile,
            "show_tab_status", FALSE))
    {
        multi_tab_set_status_icon_name(roxterm->tab, name);
    }
}

static gboolean roxterm_command_failed(ROXTermData *roxterm)
{
    MultiWin *win = roxterm_get_win(roxterm);
    GtkWidget *w = win ? multi_win_get_widget(win) : NULL;

    dlg_critical(w ? GTK_WINDOW(w) : NULL, "%s", roxterm->reply);
    g_free(roxterm->reply);
    multi_tab_delete(roxterm->tab);
    return FALSE;
}

/* Returns NULL if cwd is apparently inaccessible */
static const char *check_cwd(const char *cwd)
{
    return (cwd && g_file_test(cwd, G_FILE_TEST_IS_DIR) &&
            g_file_test(cwd, G_FILE_TEST_IS_EXECUTABLE)) ?
            cwd : NULL;
}

/* Returns home dir if cwd is inaccessible. If home is also inaccessible
 * returns NULL. */
static const char *roxterm_check_cwd(const char *cwd)
{
    return check_cwd(cwd) ? cwd : check_cwd(g_get_home_dir());
}

static void roxterm_report_launch_error_async(ROXTermData *roxterm,
        const char *msg, GError *error)
{
    roxterm->reply = g_strdup_printf("%s: %s",
            msg, error ? error->message : "?");
    g_idle_add((GSourceFunc) roxterm_command_failed, roxterm);
}

/* Mustn't free this error: https://bugzilla.gnome.org/show_bug.cgi?id=793675 */
static void roxterm_fork_callback(VteTerminal *vte,
        GPid pid, GError *error, gpointer user_data)
{
    ROXTermData *roxterm = user_data;
    (void) vte;

    roxterm->pid = pid;
    if (!vte)
        roxterm->widget = NULL;
    if (pid == -1)
    {
        roxterm_report_launch_error_async(roxterm,
                _("Failed to run command"), error);
    }
}

static void roxterm_fork_command(ROXTermData *roxterm, VteTerminal *vte,
        char **argv, char **envv,
        const char *working_directory,
        gboolean login)
{
    char *filename = argv[0];
    char **new_argv = NULL;

    working_directory = roxterm_check_cwd(working_directory);
    if (login)
    {
        /* If login_shell, make sure argv[0] is the
         * shell base name (== leaf name) prepended by "-" */
        guint old_len;
        char *leaf = strrchr(filename, G_DIR_SEPARATOR);

        if (leaf)
            ++leaf;
        else
            leaf = argv[0];

        /* The NULL terminator is not considered by g_strv_length() */
        old_len = g_strv_length(argv);
        new_argv = g_new(char *, old_len + 2);

        new_argv[0] = filename;
        new_argv[1] = g_strconcat("-", leaf, NULL);
        memcpy(new_argv + 2, argv + 1, sizeof(char *) * old_len);
        argv = new_argv;
    }

    /* VTE_SPAWN_NO_PARENT_ENVV is undocumented; looking at VTE's source it
     * appears to prevent our process' env from being merged into the envv we're
     * passing in here, which is behaviour we want.
     */
    vte_terminal_spawn_async(vte, VTE_PTY_DEFAULT,
            working_directory, argv, envv, 
            G_SPAWN_DO_NOT_REAP_CHILD |
            (login ? G_SPAWN_FILE_AND_ARGV_ZERO : G_SPAWN_SEARCH_PATH) |
            VTE_SPAWN_NO_PARENT_ENVV,
            NULL, NULL, NULL,
            -1, NULL,
            roxterm_fork_callback, roxterm);
    if (new_argv)
    {
        g_free(new_argv[1]);
        g_free(new_argv);
    }
}

static void roxterm_run_command(ROXTermData *roxterm, VteTerminal *vte)
{
    char **commandv = NULL;
    char *command = NULL;
    gboolean special = FALSE; /* special_command and -e override login_shell */
    gboolean login = FALSE;
    const char *term = options_lookup_string(roxterm->profile, "term");
    char **env;

    if (term && !term[0])
        term = NULL;
    env = roxterm_get_environment(roxterm, term);
    roxterm->running = TRUE;
    roxterm_show_status(roxterm, "window-close");
    roxterm->is_shell = FALSE;
    if (roxterm->actual_commandv)
    {
        special = TRUE;
    }
    else if (roxterm->special_command)
    {
        special = TRUE;
        /* Use special_command, currently single string */
        command = roxterm->special_command;
        special = TRUE;
    }
    else if (roxterm->commandv)
    {
        /* Either restoring a session or -e was given */
        if (!roxterm->from_session)
        {
            special = TRUE;
            /* If commandv is a single string it might be a command + args
             * separated by spaces, so parse it. See
             * <http://sourceforge.net/p/roxterm/bugs/87/>.
             */
            if (!roxterm->commandv[1])
            {
                command = roxterm->commandv[0];
                roxterm->commandv[0] = NULL;
                g_strfreev(roxterm->commandv);
                roxterm->commandv = NULL;
            }
        }
    }
    else
    {
        /* Either use custom command from option or default (as single string)
         */
        if (options_lookup_int_with_default(roxterm->profile,
                    "use_ssh", FALSE))
        {
            const char *host = options_lookup_string_with_default(
                    roxterm->profile, "ssh_address", "localhost");
            const char *user = options_lookup_string(roxterm->profile,
                    "ssh_user");
            const char *ssh_opts = options_lookup_string(roxterm->profile,
                    "ssh_options");
            int port = options_lookup_int_with_default(roxterm->profile,
                    "ssh_port", 22);

            command = g_strdup_printf("ssh%s%s %s -p %d %s",
                    (user && user[0]) ? " -l " : "",
                    (user && user[0]) ? user : "",
                    (ssh_opts && ssh_opts[0]) ? ssh_opts : "",
                    port, host);
        }
        else if (options_lookup_int_with_default(roxterm->profile,
                    "use_custom_command", FALSE))
        {
            command = options_lookup_string(roxterm->profile, "command");
        }
        if (!command || !command[0])
        {
            command = get_default_command(roxterm);
            roxterm->is_shell = TRUE;
        }
        else
        {
            /* Using custom command disables login_shell */
            special = TRUE;
        }
    }
    if (roxterm->actual_commandv)
    {
        commandv = roxterm->actual_commandv;
    }
    if (roxterm->commandv && !roxterm->special_command)
    {
        /* We're using roxterm->commandv, point commandv to it */
        commandv = roxterm->commandv;
    }
    else if (!roxterm->actual_commandv)
    {
        /* Parse our own commandv from single string command */
        int argc;
        GError *error = NULL;

        commandv = NULL;
        if (!g_shell_parse_argv(command, &argc, &commandv, &error))
        {
            char *msg = g_strdup_printf(_("Unable to parse command '%s'"),
                    command);
            roxterm_report_launch_error_async(roxterm, msg, error);
            g_free(msg);
            if (error)
                g_error_free(error);
            if (commandv)
                g_strfreev(commandv);
            commandv = NULL;
            /*
            commandv = g_new(char *, 2);
            commandv[0] = get_default_command(roxterm);
            commandv[1] = NULL;
            */
        }
    }
    if (!special && options_lookup_int_with_default(roxterm->profile,
            "login_shell", 0))
    {
        login = TRUE;
    }

    if (commandv && commandv[0])
    {
        roxterm_fork_command(roxterm, vte, commandv, env,
                roxterm->directory, login);
    }

    roxterm->special_command = NULL;
    /* If special_command was set, command points to the same string */
    g_free(command);
    roxterm->actual_commandv = commandv;
    g_strfreev(env);
}

static char *roxterm_lookup_uri_handler(ROXTermData *roxterm, const char *tag)
{
    char *filename = options_lookup_string(roxterm->profile, tag);

    if (g_file_test(filename, G_FILE_TEST_IS_DIR))
    {
        char *apprun = g_build_filename(filename, "AppRun", NULL);

        if (g_file_test(apprun, G_FILE_TEST_IS_EXECUTABLE))
        {
            g_free(filename);
            filename = apprun;
        }
        else
        {
            g_free(apprun);
        }
    }
    return filename;
}

/* Converts quotes to % sequences to fix debian bug #696917 */
char *preparse_url(const char *url)
{
    GString *s = g_string_new("");
    size_t n;
    char c;
    for (n = 0; (c = url[n]) != 0; ++n)
    {
        switch (c)
        {
            case '"':
                s = g_string_append(s, "%22");
                break;
            case '\'':
                s = g_string_append(s, "%27");
                break;
            default:
                s = g_string_append_c(s, c);
                break;
        }
    }
    return g_string_free(s, FALSE);
}

static void roxterm_launch_ssh(ROXTermData *roxterm, const char *uri)
{
    char *ssh_o = roxterm_lookup_uri_handler(roxterm, "ssh");
    char *ssh = uri_get_ssh_command(uri, ssh_o);

    if (ssh_o)
        g_free(ssh_o);
    if (ssh)
    {
        roxterm_spawn(roxterm, ssh, options_lookup_int_with_default
            (roxterm->profile, "ssh_spawn_type", ROXTerm_SpawnNewTab));
        g_free(ssh);
    }
}

static char *roxterm_get_modified_uri(ROXTermData *roxterm)
{
    const char *m = roxterm->matched_url;

    switch (roxterm->match_type)
    {
        case ROXTerm_Match_MailTo:
            if (!g_str_has_prefix(m, "mailto:"))
                return g_strdup_printf("mailto:%s", m);
            break;
        case ROXTerm_Match_URINoScheme:
            if (g_str_has_prefix(m, "ftp"))
                return g_strdup_printf("ftp://%s", m);
            else
                return g_strdup_printf("http://%s", m);
        case ROXTerm_Match_SSH_Host:
            if (!g_str_has_prefix(m, "ssh:"))
                return g_strdup_printf("ssh://%s", m);
            break;
        default:
            break;
    }
    return NULL;
}

static void roxterm_launch_uri(ROXTermData *roxterm,
        const char *uri, guint32 timestamp)
{
    GError *error = NULL;
    gboolean result;

#if GTK_CHECK_VERSION(3,22,0)
    result = gtk_show_uri_on_window(roxterm_get_toplevel(roxterm),
            uri, timestamp, &error);
#else
    result = gtk_show_uri(gtk_window_get_screen(roxterm_get_toplevel(roxterm)),
            uri, timestamp, &error);
#endif
    if (!result)
    {
        dlg_warning(roxterm_get_toplevel(roxterm),
                _("Unable to open URI %s: %s"), uri, error->message);
        g_error_free(error);
    }
}

static void roxterm_launch_matched_uri(ROXTermData *roxterm, guint32 timestamp)
{
    const char *m = roxterm->matched_url;
    char *mod = NULL;

    if (roxterm->match_type == ROXTerm_Match_SSH_Host)
    {
        roxterm_launch_ssh(roxterm, roxterm->matched_url);
        return;
    }
    mod = roxterm_get_modified_uri(roxterm);
    roxterm_launch_uri(roxterm, mod ? mod : m, timestamp);
    g_free(mod);
}

static void roxterm_data_delete(ROXTermData *roxterm)
{
    /* This doesn't delete widgets because they're deleted when removed from
     * the parent */
    GtkWindow *gwin;

    g_return_if_fail(roxterm);

    /* Fix for https://github.com/realh/roxterm/issues/197 */
    if (roxterm->widget && roxterm->child_exited_tag)
        g_signal_handler_disconnect(roxterm->widget, roxterm->child_exited_tag);
    gwin = roxterm_get_toplevel(roxterm);
    if (gwin && roxterm->win_state_changed_tag)
    {
        g_signal_handler_disconnect(gwin, roxterm->win_state_changed_tag);
    }
    if (roxterm->post_exit_tag)
        g_source_remove(roxterm->post_exit_tag);
    if (roxterm->colour_scheme)
    {
        UNREF_LOG(colour_scheme_unref(roxterm->colour_scheme));
    }
    if (roxterm->profile)
    {
        UNREF_LOG(dynamic_options_unref(roxterm_profiles,
                options_get_leafname(roxterm->profile)));
    }
    drag_receive_data_delete(roxterm->drd);
    if (roxterm->actual_commandv != roxterm->commandv)
        g_strfreev(roxterm->actual_commandv);
    if (roxterm->commandv)
        g_strfreev(roxterm->commandv);
    g_free(roxterm->directory);
    g_strfreev(roxterm->env);
    if (roxterm->pango_desc)
        pango_font_description_free(roxterm->pango_desc);
    if (roxterm->replace_task_dialog)
    {
        roxterm->postponed_free = TRUE;
        gtk_widget_destroy(roxterm->replace_task_dialog);
    }
    else
    {
        g_free(roxterm);
    }
}

typedef enum {
    ROXTerm_DontShowURIMenuItems,
    ROXTerm_ShowWebURIMenuItems,
    ROXTerm_ShowMailURIMenuItems,
    ROXTerm_ShowFileURIMenuItems,
    ROXTerm_ShowSSHHostMenuItems,
    ROXTerm_ShowVOIPURIMenuItems,
} ROXTerm_URIMenuItemsShowType;

static void
set_show_uri_menu_items(MenuTree *tree, ROXTerm_URIMenuItemsShowType show_type)
{
    g_return_if_fail(tree);
    menutree_set_show_item(tree, MENUTREE_OPEN_IN_BROWSER,
        show_type == ROXTerm_ShowWebURIMenuItems);
    menutree_set_show_item(tree, MENUTREE_OPEN_IN_MAILER,
        show_type == ROXTerm_ShowMailURIMenuItems);
    menutree_set_show_item(tree, MENUTREE_OPEN_IN_FILER,
        show_type == ROXTerm_ShowFileURIMenuItems);
    menutree_set_show_item(tree, MENUTREE_COPY_URI,
        show_type != ROXTerm_DontShowURIMenuItems);
    menutree_set_show_item(tree, MENUTREE_SSH_HOST,
        show_type == ROXTerm_ShowSSHHostMenuItems);
    menutree_set_show_item(tree, MENUTREE_VOIP_CALL,
        show_type == ROXTerm_ShowVOIPURIMenuItems);
    menutree_set_show_item(tree, MENUTREE_URI_SEPARATOR,
        show_type != ROXTerm_DontShowURIMenuItems);
}

static void
roxterm_set_show_uri_menu_items(ROXTermData * roxterm,
    ROXTerm_URIMenuItemsShowType show_type)
{
    MultiWin *win = roxterm_get_win(roxterm);

    set_show_uri_menu_items(multi_win_get_popup_menu(win), show_type);
    set_show_uri_menu_items(multi_win_get_short_popup_menu(win), show_type);
}

static double roxterm_get_config_saturation(ROXTermData *roxterm)
{
    double saturation = options_lookup_double_with_default(roxterm->profile,
            "saturation", 1.0);

    if (saturation < 0 || saturation > 1)
    {
        g_warning(_("Saturation option %f out of range"), saturation);
        saturation = CLAMP(saturation, 0, 1);
    }
    return saturation;
}

static void
roxterm_update_cursor_colour(ROXTermData * roxterm, VteTerminal * vte)
{
    vte_terminal_set_color_cursor(vte,
            colour_scheme_get_cursor_colour(roxterm->colour_scheme, TRUE));
}

static void
roxterm_update_cursorfg_colour(ROXTermData * roxterm, VteTerminal * vte)
{
    vte_terminal_set_color_cursor_foreground(vte,
            colour_scheme_get_cursorfg_colour(roxterm->colour_scheme, TRUE));
}

static void
roxterm_update_bold_colour(ROXTermData * roxterm, VteTerminal * vte)
{
    vte_terminal_set_color_bold(vte,
            colour_scheme_get_bold_colour(roxterm->colour_scheme, TRUE));
}

guint16 extrapolate_chroma(guint16 bg, guint16 fg, double factor)
{
    gint32 ext = (gint32) bg +
        (gint32) ((double) ((gint32) fg - (gint32) bg) * factor);

    return (guint16) CLAMP(ext, 0, 0xffff);
}

static const GdkRGBA *extrapolate_colours(const GdkRGBA *bg,
        const GdkRGBA *fg, double factor)
{
    static GdkRGBA ext;

#define EXTRAPOLATE(c) ext. c = \
    extrapolate_chroma(bg ? bg->  c : 0, fg ? fg->  c : 0xffff, factor)
    EXTRAPOLATE(red);
    EXTRAPOLATE(green);
    EXTRAPOLATE(blue);
    return &ext;
}

static const GdkRGBA *roxterm_get_background_colour_with_transparency(
        ROXTermData * roxterm)
{
    const GdkRGBA *bgro =
            colour_scheme_get_background_colour(roxterm->colour_scheme, TRUE);
    static GdkRGBA background;
    float alpha = roxterm_get_config_saturation(roxterm);
    
    if (bgro)
    {
        background = *bgro;
    }
    else if (alpha < 1.0f)
    {
        bgro = colour_scheme_get_background_colour(roxterm->colour_scheme,
                FALSE);
        background = *bgro;
        bgro = NULL;
    }
    background.alpha = alpha;
    return (alpha < 1.0f) ? &background : bgro;
}

static void
roxterm_apply_colour_scheme(ROXTermData *roxterm, VteTerminal *vte)
{
    gboolean bold = FALSE;
    const GdkRGBA *bd = NULL;
    int ncolours = 0;
    const GdkRGBA *palette = NULL;
    const GdkRGBA *foreground =
            colour_scheme_get_foreground_colour(roxterm->colour_scheme, TRUE);
    const GdkRGBA *background =
            roxterm_get_background_colour_with_transparency(roxterm);
    
    vte_terminal_set_default_colors(vte);
    ncolours = colour_scheme_get_palette_size(roxterm->colour_scheme);
    if (ncolours)
        palette = colour_scheme_get_palette(roxterm->colour_scheme);
    vte_terminal_set_colors(vte, foreground, background,
            palette, ncolours);
    if (!ncolours && foreground && background)
    {
        vte_terminal_set_color_bold(vte,
                extrapolate_colours(background, foreground, 1.2));
        bold = TRUE;
    }
    bd = colour_scheme_get_bold_colour(roxterm->colour_scheme, TRUE);
    if (bd || !bold)
        vte_terminal_set_color_bold(vte, bd);
    roxterm_update_cursor_colour(roxterm, vte);
    roxterm_update_cursorfg_colour(roxterm, vte);
    roxterm_force_redraw(roxterm);
}

static gboolean roxterm_is_parented(ROXTermData *roxterm)
{
    GtkWindow *top = roxterm_get_toplevel(roxterm);
    return top && gtk_widget_get_realized(GTK_WIDGET(top)) &&
        gtk_widget_get_realized(roxterm->widget);
}

static void
roxterm_set_vte_size(ROXTermData *roxterm, VteTerminal *vte,
        int columns, int rows)
{
    MultiWin *win = roxterm_get_win(roxterm);

    vte_terminal_set_size(vte, columns, rows);
    if (roxterm_is_parented(roxterm) &&
            multi_win_get_current_tab(win) == roxterm->tab)
    {
        multi_win_apply_new_geometry(win, columns, rows, roxterm->tab);
    }
}

static void
roxterm_get_padding(ROXTermData *roxterm, int *width, int *height)
{
    GtkBorder padding;

    gtk_style_context_get_padding(gtk_widget_get_style_context(roxterm->widget),
            gtk_widget_get_state_flags(roxterm->widget), &padding);
    /* The +2 is a fudge otherwise the window shrinks by one cell each time
     * it's maximized and then unmaximized.
     */
    *width = padding.left + padding.right + 2;
    *height = padding.top + padding.bottom + 2;
}

static void roxterm_geometry_func(ROXTermData *roxterm,
        GdkGeometry *geom, GdkWindowHints *hints)
{
    VteTerminal *vte = VTE_TERMINAL(roxterm->widget);

    roxterm_get_padding(roxterm, &geom->base_width, &geom->base_height);
    /* The following results include spacing (cell-*-scale) */
    geom->width_inc = vte_terminal_get_char_width(vte);
    geom->height_inc = vte_terminal_get_char_height(vte);
    geom->min_width = geom->base_width + 4 * geom->width_inc;
    geom->min_height = geom->base_height + 4 * geom->height_inc;
    if (hints)
        *hints = GDK_HINT_RESIZE_INC | GDK_HINT_BASE_SIZE | GDK_HINT_MIN_SIZE;
}

static void roxterm_size_func(ROXTermData *roxterm, gboolean pixels,
        int *pwidth, int *pheight)
{
    VteTerminal *vte = VTE_TERMINAL(roxterm->widget);

    if (!pixels || *pwidth == -1)
        *pwidth = vte_terminal_get_column_count(vte);
    if (!pixels || *pheight == -1)
        *pheight = vte_terminal_get_row_count(vte);
    if (pixels)
    {
        int px, py;

        roxterm_get_padding(roxterm, &px, &py);
        /* The following results include spacing (cell-*-scale) */
        *pwidth = *pwidth * vte_terminal_get_char_width(vte) + px;
        *pheight = *pheight * vte_terminal_get_char_height(vte) + py;
    }
}

static void roxterm_default_size_func(ROXTermData *roxterm,
        int *pwidth, int *pheight)
{
    *pwidth = options_lookup_int_with_default(roxterm->profile, "width", 80);
    *pheight = options_lookup_int_with_default(roxterm->profile, "height", 24);
}

static void roxterm_update_geometry(ROXTermData * roxterm, VteTerminal * vte)
{
    int columns, rows;
    (void) vte;

    columns = vte_terminal_get_column_count(vte);
    rows = vte_terminal_get_row_count(vte);
    roxterm_set_vte_size(roxterm, vte, columns, rows);
}

static void roxterm_update_size(ROXTermData * roxterm, VteTerminal * vte)
{
    int w, h;

    roxterm_default_size_func(roxterm, &w, &h);
    roxterm_set_vte_size(roxterm, vte, w, h);
    roxterm_update_geometry(roxterm, vte);
}

static void resize_pango_for_zoom(PangoFontDescription *pango_desc,
        double zf)
{
    int size;
    gboolean abs = FALSE;

    if (zf == 1.0)
        return;
    size = pango_font_description_get_size(pango_desc);
    if (!size)
        size = 10 * PANGO_SCALE;
    else
        abs = pango_font_description_get_size_is_absolute(pango_desc);
    size = (int) ((double) size * zf);
    if (abs)
        pango_font_description_set_absolute_size(pango_desc, size);
    else
        pango_font_description_set_size(pango_desc, size);
}

/* The spacing apply functions don't need to explicitly resize the window
 * because VTE triggers the apropriate signal.
 */
static void
roxterm_apply_vspacing(ROXTermData *roxterm, VteTerminal *vte)
{
    double spacing = (double) options_lookup_int_with_default(roxterm->profile,
            "vspacing", 0) / 100.0;

    vte_terminal_set_cell_height_scale(vte, CLAMP(spacing, 0.0, 1.0) + 1.0);
}

static void
roxterm_apply_hspacing(ROXTermData *roxterm, VteTerminal *vte)
{
    double spacing = (double) options_lookup_int_with_default(roxterm->profile,
            "hspacing", 0) / 100.0;

    vte_terminal_set_cell_width_scale(vte, CLAMP(spacing, 0.0, 1.0) + 1.0);
}

static void
roxterm_apply_profile_font(ROXTermData *roxterm, VteTerminal *vte,
    gboolean update_geometry)
{
    char *fdesc;
    PangoFontDescription *pango_desc = NULL;
    int w = vte_terminal_get_column_count(vte);
    int h = vte_terminal_get_row_count(vte);

    if (roxterm->pango_desc)
    {
        pango_font_description_free(roxterm->pango_desc);
        roxterm->pango_desc = NULL;
    }
    fdesc = options_lookup_string(roxterm->profile, "font");
    double zf = roxterm->target_zoom_factor;

    if (fdesc && fdesc[0])
    {
        pango_desc = pango_font_description_from_string(fdesc);
        if (!pango_desc)
            g_warning(_("Couldn't create a font from '%s'"), fdesc);
    }
    if (!pango_desc)
    {
        if (zf == roxterm->current_zoom_factor)
        {
            if (fdesc)
                g_free(fdesc);
            return;
        }
        pango_desc = pango_font_description_copy(
                vte_terminal_get_font(vte));
        zf /= roxterm->current_zoom_factor;
    }
    if (!pango_desc)
    {
        g_free(fdesc);
        g_return_if_fail(pango_desc != NULL);
    }
    resize_pango_for_zoom(pango_desc, zf);
    vte_terminal_set_font(vte, pango_desc);
    roxterm->pango_desc = pango_desc;
    g_free(fdesc);
    roxterm->current_zoom_factor = roxterm->target_zoom_factor;
    roxterm_apply_vspacing(roxterm, vte);
    roxterm_apply_hspacing(roxterm, vte);
    if (update_geometry)
    {
        roxterm_set_vte_size(roxterm, vte, w, h);
    }
}

static void
roxterm_update_font(ROXTermData *roxterm, VteTerminal *vte,
    gboolean update_geometry)
{
    PangoFontDescription *pango_desc = NULL;
    double zf;
    int w, h;

    if (!roxterm->pango_desc)
    {
        roxterm_apply_profile_font(roxterm, vte, update_geometry);
        return;
    }
    w = vte_terminal_get_column_count(vte);
    h = vte_terminal_get_row_count(vte);
    zf = roxterm->target_zoom_factor /
            (roxterm->current_zoom_factor ?
                    roxterm->current_zoom_factor : 1);
    pango_desc = roxterm->pango_desc;
    resize_pango_for_zoom(pango_desc, zf);
    vte_terminal_set_font(vte, pango_desc);
    roxterm->current_zoom_factor = roxterm->target_zoom_factor;
    roxterm_apply_vspacing(roxterm, vte);
    roxterm_apply_hspacing(roxterm, vte);
    if (update_geometry)
    {
        roxterm_set_vte_size(roxterm, vte, w, h);
    }
}

static void roxterm_set_zoom_factor(ROXTermData *roxterm, double factor,
        int index)
{
    roxterm->zoom_index = index;
    roxterm->target_zoom_factor = factor;
    roxterm_update_font(roxterm, VTE_TERMINAL(roxterm->widget), TRUE);
}

/* Change a terminal's font etc so it matches size with another */
static void roxterm_match_text_size(ROXTermData *roxterm, ROXTermData *other)
{
    VteTerminal *vte;
    VteTerminal *other_vte;
    const PangoFontDescription *fd;
    int width, height;

    if (roxterm == other)
        return;
    vte = VTE_TERMINAL(roxterm->widget);
    other_vte = VTE_TERMINAL(other->widget);
    width = vte_terminal_get_column_count(other_vte);
    height = vte_terminal_get_row_count(other_vte);
    fd = vte_terminal_get_font(VTE_TERMINAL(other->widget));
    roxterm->target_zoom_factor = other->current_zoom_factor;
    roxterm->current_zoom_factor = other->current_zoom_factor;
    roxterm->zoom_index = other->zoom_index;
    if (!pango_font_description_equal(
            vte_terminal_get_font(VTE_TERMINAL(roxterm->widget)), fd))
    {
        vte_terminal_set_font(vte, fd);
    }
    roxterm_set_vte_size(roxterm, VTE_TERMINAL(roxterm->widget),
            width, height);
}

static gboolean roxterm_about_uri_hook(GtkAboutDialog *about,
        gchar *link, gpointer data)
{
    (void) about;
    roxterm_launch_uri(data, link, gtk_get_current_event_time());
    return TRUE;
}

static gboolean roxterm_popup_handler(GtkWidget * widget, ROXTermData * roxterm)
{
    (void) widget;
    roxterm_set_show_uri_menu_items(roxterm, ROXTerm_DontShowURIMenuItems);
    multi_tab_popup_menu_at_pointer(roxterm->tab);
    return TRUE;
}

/* Checks whether a button event position is over a matched expression; if so
 * roxterm->matched_url is set and the result is TRUE */
static gboolean roxterm_check_match(ROXTermData *roxterm, VteTerminal *vte,
        GdkEvent *event)
{
    int tag;
#if VTE_CHECK_VERSION(0,50,0)
    char *hyper = roxterm_enable_hyperlinks() ?
        vte_terminal_hyperlink_check_event(vte, event) : NULL;
#endif

    g_free(roxterm->matched_url);
    /* If we're over a hyperlink we should also have a pattern match; be lazy
     * and use that to work out the type of link */
    roxterm->matched_url = vte_terminal_match_check_event(vte, event, &tag);
    if (roxterm->matched_url)
    {
        roxterm->match_type = roxterm_get_match_type(roxterm, tag);
    }
#if VTE_CHECK_VERSION(0,50,0)
    else if (hyper)
    {
        roxterm->match_type = ROXTerm_Match_FullURI;
    }
    if (hyper)
    {
        g_free(roxterm->matched_url);
        roxterm->matched_url = hyper;
    }
#endif
    return roxterm->matched_url != NULL;
}

static void roxterm_clear_hold_over_uri(ROXTermData *roxterm)
{
    if (roxterm->hold_over_uri)
    {
        g_signal_handler_disconnect(roxterm->widget, roxterm->hold_handler_id);
        roxterm->hold_over_uri = FALSE;
    }
}

inline static void roxterm_clear_drag_url(ROXTermData *roxterm)
{
    roxterm_clear_hold_over_uri(roxterm);
}

static GtkTargetList *roxterm_get_uri_drag_target_list(void)
{
    static const GtkTargetEntry target_table[] = {
        { (char *) "text/uri-list", 0, ROXTERM_DRAG_TARGET_URI_LIST },
        { (char *) "UTF8_STRING", 0, ROXTERM_DRAG_TARGET_UTF8_STRING }
    };

    return gtk_target_list_new(target_table, G_N_ELEMENTS(target_table));
}

static void roxterm_uri_drag_ended(GtkWidget *widget,
        GdkDragContext *drag_context, ROXTermData *roxterm)
{
    (void) widget;
    (void) drag_context;
    roxterm_clear_drag_url(roxterm);
}

static void roxterm_uri_drag_data_get(GtkWidget *widget,
        GdkDragContext *drag_context, GtkSelectionData *selection,
        guint info, guint time, ROXTermData *roxterm)
{
    char *uri_list[2];
    (void) widget;
    (void) drag_context;
    (void) time;

    g_return_if_fail(roxterm->matched_url);

    if (info == ROXTERM_DRAG_TARGET_URI_LIST)
    {
        uri_list[0] = roxterm_get_modified_uri(roxterm);
        if (!uri_list[0])
            uri_list[0] = g_strdup(roxterm->matched_url);
        uri_list[1] = NULL;
        gtk_selection_data_set_uris(selection, uri_list);
        g_free(uri_list[0]);
    }
    else
    {
        gtk_selection_data_set_text(selection, roxterm->matched_url, -1);
    }
}

/* Starts drag & drop process if user tries to drag a URL */
static gboolean roxterm_motion_handler(GtkWidget *widget, GdkEventButton *event,
        ROXTermData *roxterm)
{
    GtkTargetList *target_list;

    if (!gtk_drag_check_threshold(widget, roxterm->hold_x,
                roxterm->hold_y, event->x, event->y)
            || !roxterm->hold_over_uri)
    {
        return FALSE;
    }

    roxterm_clear_hold_over_uri(roxterm);
    g_return_val_if_fail(roxterm->matched_url, FALSE);


    target_list = roxterm_get_uri_drag_target_list();
    gtk_drag_begin_with_coordinates(roxterm->widget, target_list,
            GDK_ACTION_COPY, 1, (GdkEvent *) event, event->x, event->y);

    return TRUE;
}

static gboolean roxterm_click_handler(GtkWidget *widget,
        GdkEventButton *event, ROXTermData *roxterm)
{
    VteTerminal *vte = VTE_TERMINAL(roxterm->widget);

    (void) widget;
    
    roxterm_clear_drag_url(roxterm);
    roxterm_check_match(roxterm, vte, (GdkEvent *) event);
    if (event->button == 3)
    {
        ROXTerm_URIMenuItemsShowType show_type = ROXTerm_DontShowURIMenuItems;

        /* If user right-clicks with no modifiers give the child app a chance
         * to handle the event first.
         */
        if (!(event->state & (GDK_SHIFT_MASK |
                GDK_CONTROL_MASK | GDK_MOD1_MASK)))
        {
            if (GTK_WIDGET_CLASS(VTE_TERMINAL_GET_CLASS(widget))->
                    button_press_event(widget, event))
            {
                return TRUE;
            }
        }

        if (roxterm->matched_url)
        {
            switch (roxterm->match_type)
            {
                case ROXTerm_Match_SSH_Host:
                    show_type = ROXTerm_ShowSSHHostMenuItems;
                    break;
                case ROXTerm_Match_MailTo:
                    show_type = ROXTerm_ShowMailURIMenuItems;
                    break;
                case ROXTerm_Match_VOIP:
                    show_type = ROXTerm_ShowVOIPURIMenuItems;
                    break;
                case ROXTerm_Match_File:
                    show_type = ROXTerm_ShowFileURIMenuItems;
                    break;
                default:
                    show_type = ROXTerm_ShowWebURIMenuItems;
                    break;
                }
        }
        roxterm_set_show_uri_menu_items(roxterm, show_type);
        multi_tab_popup_menu_at_pointer(roxterm->tab);
        return TRUE;
    }
    else if ((event->state & GDK_CONTROL_MASK) && event->button == 1
             && event->type == GDK_BUTTON_PRESS && roxterm->matched_url)
    {
        roxterm->hold_over_uri = TRUE;
        roxterm->hold_x = event->x;
        roxterm->hold_y = event->y;
        roxterm->hold_handler_id = g_signal_connect(roxterm->widget,
                "motion-notify-event",
                G_CALLBACK(roxterm_motion_handler), roxterm);
        return TRUE;
    }
    return FALSE;
}

static gboolean roxterm_release_handler(GtkWidget *widget,
        GdkEventButton *event, ROXTermData *roxterm)
{
    gboolean result = FALSE;
    (void) widget;

    if ((event->state & GDK_CONTROL_MASK) && event->button == 1
            && roxterm->hold_over_uri && roxterm->matched_url)
    {
        roxterm_launch_matched_uri(roxterm, event->time);
        result = TRUE;
    }
    roxterm_clear_hold_over_uri(roxterm);
    return result;
}

static void roxterm_window_title_handler(VteTerminal *vte,
        ROXTermData * roxterm)
{
    const char *t = vte_terminal_get_window_title(vte);

    multi_tab_set_window_title(roxterm->tab, t ? t : _("ROXTerm"));
}

/* data is cast to char const **pname - indirect pointer to name to check for -
 * if a match is found, *pname is set to NULL */
static void check_if_name_matches_property(GtkWidget *widget, gpointer data)
{
    const char *profile_name = g_object_get_data(G_OBJECT(widget),
            PROFILE_NAME_KEY);
    char const **pname = data;
    gboolean check = FALSE;

    g_return_if_fail(profile_name);
    /* pname may have been NULLed by an earlier iteration */
    if (!*pname)
        return;
    if (!strcmp(profile_name, *pname))
    {
        check = TRUE;
        *pname = NULL;
    }
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), check);
}

static void check_preferences_submenu(MenuTree *tree, MenuTreeID id,
        const char *current_name)
{
    GtkMenu *submenu = menutree_submenu_from_id(tree, id);
    const char *name = current_name;

    gtk_container_foreach(GTK_CONTAINER(submenu),
            check_if_name_matches_property, &name);
    if (name)
    {
        /* name hasn't been NULLed, no match, try "ghost" */
        char *ghost_name = g_strdup_printf("(%s)", current_name);

        name = ghost_name;
        gtk_container_foreach(GTK_CONTAINER(submenu),
                check_if_name_matches_property, &name);
        g_free(ghost_name);
        if (name)
        {
            g_critical(_("No menu item matches profile/scheme '%s'"),
                    current_name);
        }
    }
}

static void check_preferences_submenu_pair(ROXTermData *roxterm, MenuTreeID id,
        const char *current_name)
{
    MultiWin *win = roxterm_get_win(roxterm);
    MenuTree *tree = multi_win_get_popup_menu(win);

    multi_win_set_ignore_toggles(win, TRUE);
    if (tree)
    {
        check_preferences_submenu(tree, id, current_name);
    }
    tree = multi_win_get_menu_bar(win);
    if (tree)
    {
        check_preferences_submenu(tree, id, current_name);
    }
    multi_win_set_ignore_toggles(win, FALSE);
}

inline static void roxterm_shade_mtree_search_items(MenuTree *mtree,
        gboolean shade)
{
    if (mtree)
    {
        menutree_shade(mtree, MENUTREE_SEARCH_FIND_NEXT, shade);
        menutree_shade(mtree, MENUTREE_SEARCH_FIND_PREVIOUS, shade);
    }
}

static void roxterm_shade_search_menu_items(ROXTermData *roxterm)
{
    MultiWin *win = roxterm_get_win(roxterm);
    gboolean shade = vte_terminal_search_get_regex(
            VTE_TERMINAL(roxterm->widget)) == NULL;

    roxterm_shade_mtree_search_items(multi_win_get_menu_bar(win), shade);
    roxterm_shade_mtree_search_items(multi_win_get_popup_menu(win), shade);
}

static void roxterm_tab_selection_handler(ROXTermData * roxterm, MultiTab * tab)
{
    MultiWin *win = roxterm_get_win(roxterm);
    (void) tab;

    roxterm->status_icon_name = NULL;
    check_preferences_submenu_pair(roxterm,
            MENUTREE_PREFERENCES_SELECT_PROFILE,
            options_get_leafname(roxterm->profile));
    check_preferences_submenu_pair(roxterm,
            MENUTREE_PREFERENCES_SELECT_COLOUR_SCHEME,
            options_get_leafname(roxterm->colour_scheme));
    check_preferences_submenu_pair(roxterm,
            MENUTREE_PREFERENCES_SELECT_SHORTCUTS,
            options_get_leafname(multi_win_get_shortcut_scheme(win)));
    roxterm_shade_search_menu_items(roxterm);

    multi_win_set_ignore_toggles(win, TRUE);
    multi_win_set_ignore_toggles(win, FALSE);
}

static gboolean run_child_when_idle(ROXTermData *roxterm)
{
    if (!roxterm->running)
        roxterm_run_command(roxterm, VTE_TERMINAL(roxterm->widget));
    return FALSE;
}

static void roxterm_launch_uri_action(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    if (roxterm->matched_url)
        roxterm_launch_matched_uri(roxterm, gtk_get_current_event_time());
}

static void roxterm_copy_url_action(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    if (roxterm->matched_url)
    {
        gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY),
            roxterm->matched_url, -1);
        gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
            roxterm->matched_url, -1);
    }
}

static void roxterm_select_all_action(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    vte_terminal_select_all(VTE_TERMINAL(roxterm->widget));
}

static void roxterm_copy_clipboard_action(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
#if VTE_CHECK_VERSION(0,50,0)
    vte_terminal_copy_clipboard_format(VTE_TERMINAL(roxterm->widget),
            VTE_FORMAT_TEXT);
#else
    vte_terminal_copy_clipboard(VTE_TERMINAL(roxterm->widget));
#endif
}

static void roxterm_paste_clipboard_action(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    vte_terminal_paste_clipboard(VTE_TERMINAL(roxterm->widget));
}

static void roxterm_copy_and_paste_action(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
#if VTE_CHECK_VERSION(0,50,0)
    vte_terminal_copy_clipboard_format(VTE_TERMINAL(roxterm->widget),
            VTE_FORMAT_TEXT);
#else
    vte_terminal_copy_clipboard(VTE_TERMINAL(roxterm->widget));
#endif
    vte_terminal_paste_clipboard(VTE_TERMINAL(roxterm->widget));
}

static void roxterm_reset_action(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    vte_terminal_reset(VTE_TERMINAL(roxterm->widget), TRUE, FALSE);
}

static void roxterm_reset_and_clear_action(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    vte_terminal_reset(VTE_TERMINAL(roxterm->widget), TRUE, TRUE);
}

static void roxterm_respawn_action(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);
    int response;
    GtkWidget *w = NULL;

    if (roxterm->post_exit_tag)
    {
        g_source_remove(roxterm->post_exit_tag);
    }
    if (roxterm->running)
    {
        w = roxterm->replace_task_dialog =
                dlg_ok_cancel(GTK_WINDOW(multi_win_get_widget(win)),
                    _("Kill current task?"),
                    _("The current task is still running; "
                    "to restart it the current instance must be killed. "
                    "Do you want to kill the current task to "
                    "replace it with a new instance?"));
        response = gtk_dialog_run(GTK_DIALOG(w));
    }
    else
    {
        response = GTK_RESPONSE_OK;
    }
    switch (response)
    {
        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
            /* Don't destroy */
            break;
        case GTK_RESPONSE_OK:
            roxterm_run_command(roxterm, VTE_TERMINAL(roxterm->widget));
            /* Fall through */
        default:
            if (w)
                gtk_widget_destroy(w);
    }
    if (roxterm->postponed_free)
        g_free(roxterm);
    else
        roxterm->replace_task_dialog = NULL;
}

static void roxterm_edit_profile_action(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    optsdbus_send_edit_profile_message(options_get_leafname(roxterm->profile));
}

static void roxterm_edit_colour_scheme_action(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    optsdbus_send_edit_colour_scheme_message(
            options_get_leafname(roxterm->colour_scheme));
}

static void roxterm_edit_shortcuts_scheme_action(MultiWin * win)
{
    shortcuts_edit(GTK_WINDOW(multi_win_get_widget(win)),
            multi_win_get_shortcuts_scheme_name(win));
}


static void roxterm_open_search_action(MultiWin *win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    search_open_dialog(roxterm);
}

static void roxterm_find_next_action(MultiWin *win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    vte_terminal_search_find_next(VTE_TERMINAL(roxterm->widget));
}

static void roxterm_find_prev_action(MultiWin *win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    vte_terminal_search_find_previous(VTE_TERMINAL(roxterm->widget));
}

static void roxterm_show_about(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);

    g_return_if_fail(roxterm);
    about_dialog_show(GTK_WINDOW(multi_win_get_widget(win)),
            roxterm_about_uri_hook,
            roxterm);
}

static char *roxterm_get_help_filename(const char *base_dir, const char *lang)
{
    char *filename = g_build_filename(base_dir, lang, "guide.html", NULL);

    if (!g_file_test(filename, G_FILE_TEST_EXISTS))
    {
        g_free(filename);
        filename = NULL;
    }
    return filename;
}

static char *roxterm_get_help_uri(const char *base_dir, const char *lang)
{
    char *short_lang = NULL;
    char *sep;
    char *filename;
    char *uri;

    filename = roxterm_get_help_filename(base_dir, lang);
    if (!filename)
    {
        short_lang = g_strdup(lang);
        sep = strchr(short_lang, '.');
        if (sep)
        {
            *sep = 0;
            filename = roxterm_get_help_filename(base_dir, short_lang);
        }
        if (!filename)
        {
            sep = strchr(short_lang, '_');
            filename = roxterm_get_help_filename(base_dir, short_lang);
        }
        g_free(short_lang);
        if (!filename)
        {
            filename = roxterm_get_help_filename(base_dir, "en");
        }
    }
    uri = g_strconcat("file://", filename, NULL);
    g_free(filename);
    return uri;
}

static void roxterm_show_manual(MultiWin * win)
{
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);
    char *uri;
    char *dir = NULL;
    const char *lang;

    g_return_if_fail(roxterm);

    if (global_options_appdir)
    {
        dir = g_build_filename(global_options_appdir, "Help", NULL);
    }
    else
    {
        dir = g_build_filename(HTML_DIR, NULL);
    }

    lang = g_getenv("LANG");
    if (!lang || !lang[0])
        lang = "en";
    uri = roxterm_get_help_uri(dir, lang);

    roxterm_launch_uri(roxterm, uri, gtk_get_current_event_time());
    g_free(uri);
    g_free(dir);
}

static void roxterm_open_config_manager(void *ignored)
{
    (void) ignored;
    optsdbus_send_edit_opts_message("Configlet", NULL);
}

/* The "style-updated" signal gets heavily spammed, including when GTK is
 * playing silly buggers with size allocation, so we don't have much choice
 * but to ignore it altogether.
 */
/*
static void
roxterm_style_change_handler(GtkWidget * widget, ROXTermData * roxterm)
{
    (void) widget;
    if (gtk_widget_get_allocated_width(roxterm->widget) > 1 &&
        gtk_widget_get_allocated_height(roxterm->widget) > 1)
    {
        roxterm_update_geometry(roxterm, VTE_TERMINAL(roxterm->widget));
    }
}
*/

static void
roxterm_char_size_changed(GtkSettings * settings, guint arg1, guint arg2,
    ROXTermData * roxterm)
{
    (void) settings;
    (void) arg1;
    (void) arg2;
    roxterm_update_geometry(roxterm, VTE_TERMINAL(roxterm->widget));
}

static void roxterm_hide_menutree(GtkMenuItem *item, gpointer handle)
{
    GtkWidget *submenu = gtk_menu_item_get_submenu(item);
    (void) handle;

    if (submenu)
    {
        gtk_widget_hide(submenu);
        gtk_container_foreach(GTK_CONTAINER(submenu),
                (GtkCallback) roxterm_hide_menutree, NULL);
    }
}

static RoxtermChildExitAction
roxterm_get_child_exit_action(ROXTermData *roxterm)
{
    RoxtermChildExitAction action = roxterm->exit_action;
    if (action == Roxterm_ChildExitNotOverridden)
    {
        action = options_lookup_int_with_default(roxterm->profile,
                "exit_action", Roxterm_ChildExitClose);
    }
    return action;
}

static gboolean roxterm_post_child_exit(ROXTermData *roxterm)
{
    MultiWin *win = roxterm_get_win(roxterm);
    RoxtermChildExitAction action = roxterm_get_child_exit_action(roxterm);
    if (action == Roxterm_ChildExitNotOverridden)
    {
        action = options_lookup_int_with_default(roxterm->profile,
                "exit_action", Roxterm_ChildExitClose);
    }
    if (action == Roxterm_ChildExitAsk)
    {
        GtkWidget *dialog = gtk_message_dialog_new(
                roxterm_get_toplevel(roxterm),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                _("The command in this terminal has terminated. "
                "What do you want to do with the terminal now?"));
        GtkWidget *respawn;

        gtk_dialog_add_button(GTK_DIALOG(dialog),
                _("Close"), Roxterm_ChildExitClose);
        gtk_dialog_add_button(GTK_DIALOG(dialog),
                _("Leave open"), Roxterm_ChildExitHold);
        respawn = gtk_dialog_add_button(GTK_DIALOG(dialog),
                _("Rerun command"), Roxterm_ChildExitRespawn);
        if (roxterm->no_respawn)
            gtk_widget_set_sensitive(respawn, FALSE);
        gtk_widget_show_all(dialog);
        action = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
    roxterm->post_exit_tag = 0;
    if (action == Roxterm_ChildExitRespawn && roxterm->no_respawn)
        action = Roxterm_ChildExitClose;
    switch (action)
    {
        case Roxterm_ChildExitClose:
            gtk_container_foreach(
                    GTK_CONTAINER(
                            multi_win_get_menu_bar(win)->top_level),
                    (GtkCallback) roxterm_hide_menutree, NULL);
            gtk_widget_hide(multi_win_get_popup_menu(win)->top_level);
            gtk_widget_hide(multi_win_get_short_popup_menu(win)->top_level);
            multi_tab_delete(roxterm->tab);
            break;
        case Roxterm_ChildExitHold:
            roxterm_show_status(roxterm, "dialog-error");
            break;
        case Roxterm_ChildExitRespawn:
            roxterm_run_command(roxterm, VTE_TERMINAL(roxterm->widget));
            break;
        default:
            break;
    }
    return FALSE;
}

static void roxterm_child_exited(VteTerminal *vte, int status,
        ROXTermData *roxterm)
{
    double delay = 0;

    (void) status;

    roxterm->running = FALSE;
    roxterm_show_status(roxterm, "dialog-error");
    RoxtermChildExitAction action = roxterm_get_child_exit_action(roxterm);
    if (action != Roxterm_ChildExitAsk &&
        ((delay = options_lookup_double(roxterm->profile, "exit_pause")) != 0))
    {
        vte_terminal_feed(vte, _("\n.\n"), -1);
        roxterm->post_exit_tag = g_timeout_add((guint) (delay * 1000.0),
                (GSourceFunc) roxterm_post_child_exit, roxterm);
    }
    else
    {
        roxterm_post_child_exit(roxterm);
    }
}

inline static ROXTermData *roxterm_from_menutree(MenuTree *mtree)
{
    return (ROXTermData *) multi_win_get_user_data_for_current_tab(
                     (MultiWin *) mtree->user_data);

}

static void roxterm_change_profile(ROXTermData *roxterm, Options *profile)
{
    if (roxterm->profile != profile)
    {
        if (roxterm->profile)
        {
            dynamic_options_unref(roxterm_profiles,
                    options_get_leafname(roxterm->profile));
        }
        roxterm->profile = profile;
        options_ref(roxterm->profile);
        /* Force profile's font */
        if (roxterm->pango_desc)
        {
            pango_font_description_free(roxterm->pango_desc);
            roxterm->pango_desc = NULL;
        }
        roxterm_apply_profile(roxterm, VTE_TERMINAL(roxterm->widget), FALSE);
        roxterm_update_size(roxterm, VTE_TERMINAL(roxterm->widget));
    }
}

static void roxterm_change_colour_scheme(ROXTermData *roxterm,
        Options *colour_scheme)
{
    if (roxterm->colour_scheme != colour_scheme)
    {
        if (roxterm->colour_scheme)
            colour_scheme_unref(roxterm->colour_scheme);
        roxterm->colour_scheme = colour_scheme;
        options_ref(colour_scheme);
        roxterm_apply_colour_scheme(roxterm, VTE_TERMINAL(roxterm->widget));
    }
}

static void roxterm_change_colour_scheme_by_name(ROXTermData *roxterm,
        const char *name)
{
    Options *scheme = colour_scheme_lookup_and_ref(name);
    if (scheme)
    {
        roxterm_change_colour_scheme(roxterm, scheme);
        colour_scheme_unref(scheme);
    }
    else
    {
        dlg_warning(roxterm_get_toplevel(roxterm),
                _("Unknown colour scheme '%s'"), name);
    }
}

static void match_text_size_foreach_tab(MultiTab *tab, void *data)
{
    ROXTermData *roxterm = multi_tab_get_user_data(tab);

    if (roxterm != data)
        roxterm_match_text_size(roxterm, data);
}

static void roxterm_new_term_with_profile(GtkMenuItem *mitem,
        MenuTree *mtree, gboolean just_tab)
{
    ROXTermData *roxterm = roxterm_from_menutree(mtree);
    MultiWin *win;
    const char *profile_name;
    Options *old_profile;
    Options *new_profile;

    if (!roxterm)
        return;

    win = roxterm_get_win(roxterm);
    profile_name = g_object_get_data(G_OBJECT(mitem), PROFILE_NAME_KEY);
    if (!profile_name)
    {
        g_critical(_("Menu item has no '%s' data"), PROFILE_NAME_KEY);
        return;
    }
    old_profile = roxterm->profile;
    new_profile = dynamic_options_lookup_and_ref(roxterm_get_profiles(),
            profile_name, "roxterm profile");
    if (!new_profile)
    {
        dlg_warning(roxterm_get_toplevel(roxterm),
                _("Profile '%s' not found"), profile_name);
        return;
    }
    roxterm->profile = new_profile;
    if (just_tab)
    {
        multi_tab_new(win, roxterm);
    }
    else
    {
        multi_win_clone(win, roxterm,
                options_lookup_int_with_default(new_profile,
                        "always_show_tabs", TRUE));
    }
    dynamic_options_unref(roxterm_profiles, profile_name);
    roxterm->profile = old_profile;
    /* All tabs in the same window must have same size */
    /*
    if (just_tab && strcmp(profile_name, options_get_leafname(old_profile)))
    {
        multi_win_foreach_tab(win, match_text_size_foreach_tab,
                multi_tab_get_user_data(tab));
    }
    */
}

static void roxterm_new_window_with_profile(GtkMenuItem *mitem, MenuTree *mtree)
{
    roxterm_new_term_with_profile(mitem, mtree, FALSE);
}

static void roxterm_new_tab_with_profile(GtkMenuItem *mitem, MenuTree *mtree)
{
    roxterm_new_term_with_profile(mitem, mtree, TRUE);
}

static void roxterm_profile_selected(GtkCheckMenuItem *mitem, MenuTree *mtree)
{
    ROXTermData *roxterm = roxterm_from_menutree(mtree);
    const char *profile_name;
    MultiWin *win = roxterm ? roxterm_get_win(roxterm) : NULL;

    if (!roxterm || multi_win_get_ignore_toggles(win))
        return;

    profile_name = g_object_get_data(G_OBJECT(mitem), PROFILE_NAME_KEY);
    if (!profile_name)
    {
        g_critical(_("Menu item has no '%s' data"), PROFILE_NAME_KEY);
        return;
    }
    if (!gtk_check_menu_item_get_active(mitem))
    {
        check_preferences_submenu_pair(roxterm,
                MENUTREE_PREFERENCES_SELECT_PROFILE, profile_name);
    }

    if (strcmp(profile_name, options_get_leafname(roxterm->profile)))
    {
        Options *profile = dynamic_options_lookup_and_ref(roxterm_profiles,
            profile_name, "roxterm profile");

        if (profile)
        {
            roxterm_change_profile(roxterm, profile);
            /* All tabs in the same window must have same size */
            multi_win_foreach_tab(win, match_text_size_foreach_tab,
                    roxterm);
            /* Have one more ref than we need, so decrease it */
            options_unref(profile);
        }
        else
        {
            dlg_warning(roxterm_get_toplevel(roxterm),
                    _("Profile '%s' not found"), profile_name);
        }
    }
}

static void roxterm_colour_scheme_selected(GtkCheckMenuItem *mitem,
        MenuTree *mtree)
{
    ROXTermData *roxterm = roxterm_from_menutree(mtree);
    const char *scheme_name;

    if (!roxterm || multi_win_get_ignore_toggles(roxterm_get_win(roxterm)))
        return;

    scheme_name = g_object_get_data(G_OBJECT(mitem), PROFILE_NAME_KEY);
    if (!scheme_name)
    {
        g_critical(_("Menu item has no '%s' data"), PROFILE_NAME_KEY);
        return;
    }
    if (!gtk_check_menu_item_get_active(mitem))
    {
        check_preferences_submenu_pair(roxterm,
                MENUTREE_PREFERENCES_SELECT_COLOUR_SCHEME, scheme_name);
    }

    if (strcmp(scheme_name, options_get_leafname(roxterm->colour_scheme)))
    {
        Options *colour_scheme = colour_scheme_lookup_and_ref(scheme_name);

        if (colour_scheme)
        {
            roxterm_change_colour_scheme(roxterm, colour_scheme);
            options_unref(colour_scheme);
        }
        else
        {
            dlg_warning(roxterm_get_toplevel(roxterm),
                    _("Colour scheme '%s' not found"), scheme_name);
        }
    }
}

static void roxterm_shortcuts_selected(GtkCheckMenuItem *mitem,
        MenuTree *mtree)
{
    ROXTermData *roxterm = roxterm_from_menutree(mtree);
    MultiWin *win = roxterm ? roxterm_get_win(roxterm) : NULL;
    char *scheme_name;
    Options *shortcuts;

    if (!roxterm || multi_win_get_ignore_toggles(win))
        return;

    scheme_name = g_object_get_data(G_OBJECT(mitem), PROFILE_NAME_KEY);
    if (!scheme_name)
    {
        g_critical(_("Menu item has no '%s' data"), PROFILE_NAME_KEY);
        return;
    }
    if (!gtk_check_menu_item_get_active(mitem))
    {
        check_preferences_submenu_pair(roxterm,
                MENUTREE_PREFERENCES_SELECT_SHORTCUTS, scheme_name);
    }
    shortcuts = shortcuts_open(scheme_name, TRUE);
    multi_win_set_shortcut_scheme(win, shortcuts);
    shortcuts_unref(shortcuts);
}

static void roxterm_text_changed_handler(VteTerminal *vte, ROXTermData *roxterm)
{
    (void) vte;
    if (roxterm->tab != multi_win_get_current_tab(roxterm_get_win(roxterm)))
    {
        roxterm_show_status(roxterm, "dialog-information");
    }
}

static void roxterm_bell_handler(VteTerminal *vte, ROXTermData *roxterm)
{
    MultiWin *win = roxterm_get_win(roxterm);
    (void) vte;

    if (roxterm->tab != multi_win_get_current_tab(win))
    {
        roxterm_show_status(roxterm, "dialog-warning");
    }
    if (options_lookup_int_with_default(roxterm->profile,
            "bell_highlights_tab", TRUE))
    {
        GtkWindow *gwin = GTK_WINDOW(multi_win_get_widget(win));

        if (roxterm->tab != multi_win_get_current_tab(win))
            multi_tab_draw_attention(roxterm->tab);
        if (!gtk_window_is_active(gwin))
            gtk_window_set_urgency_hint(gwin, TRUE);
    }
}

/* Ignore keys which are shortcuts, otherwise they get sent to terminal
 * when menu item is shaded.
 */
static gboolean roxterm_key_press_handler(GtkWidget *widget,
        GdkEventKey *event, ROXTermData *roxterm)
{
    Options *shortcuts = multi_win_get_shortcut_scheme(
            roxterm_get_win(roxterm));
    (void) widget;

    if (!event->is_modifier &&
            shortcuts_key_is_shortcut(shortcuts, event->keyval,
                    event->state & GDK_MODIFIER_MASK))
    {
        return TRUE;
    }
    return FALSE;
}

static void roxterm_resize_window_handler(VteTerminal *vte,
        guint columns, guint rows, ROXTermData *roxterm)
{
    MultiWin *win = roxterm_get_win(roxterm);

    /* Can't compute size if not realized */
    if (!gtk_widget_get_realized(roxterm->widget))
        return;
    /* Ignore if maximised or full screen */
    if (win && (multi_win_is_maximised(win) || multi_win_is_fullscreen(win)))
        return;

    /* May already be desired size */
    // The following code was broken, needs to convert to pixels
    //GtkAllocation alloc;
    //gtk_widget_get_allocation(roxterm->widget, &alloc);
    //if (alloc.width == (int) width && alloc.height == (int) height)
    //    return;

    roxterm_set_vte_size(roxterm, vte, columns, rows);
}

static GtkWidget *create_radio_menu_item(MenuTree *mtree,
        const char *name, GSList **group, GCallback handler)
{
    GtkWidget *mitem;

    mitem = gtk_radio_menu_item_new_with_label(*group, name);
    gtk_widget_show(mitem);
    *group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(mitem));
    if (handler)
    {
        g_object_set_data_full(G_OBJECT(mitem), PROFILE_NAME_KEY,
                g_strdup(name), g_free);
        g_signal_connect(mitem, "activate", handler, mtree);
    }
    return mitem;
}

static GtkMenu *radio_menu_from_strv(char **items,
        GCallback handler, MenuTree *mtree)
{
    int n;
    GtkMenu *menu;
    GSList *group = NULL;

    if (!items || !items[0])
        return NULL;

    menu = GTK_MENU(gtk_menu_new());

    for (n = 0; items[n]; ++n)
    {
        GtkWidget *mitem = create_radio_menu_item(mtree, items[n],
                &group, handler);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mitem);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mitem),
                FALSE);
    }
    return menu;
}

static void roxterm_add_pair_of_pref_submenus(MultiWin *win,
        DynamicOptions *family, MenuTreeID id, GCallback handler)
{
    char **items = dynamic_options_list_sorted(family);
    GtkMenu *submenu;
    MenuTree *menutree;

    g_return_if_fail(items);
    multi_win_set_ignore_toggles(win, TRUE);
    menutree = multi_win_get_menu_bar(win);
    if (menutree)
    {
        submenu = radio_menu_from_strv(items, handler, menutree);
        gtk_menu_item_set_submenu(
                GTK_MENU_ITEM(menutree_get_widget_for_id(menutree, id)),
                GTK_WIDGET(submenu));
    }
    menutree = multi_win_get_popup_menu(win);
    if (menutree)
    {
        submenu = radio_menu_from_strv(items, handler, menutree);
        gtk_menu_item_set_submenu(
                GTK_MENU_ITEM(menutree_get_widget_for_id(menutree, id)),
                GTK_WIDGET(submenu));
    }
    multi_win_set_ignore_toggles(win, FALSE);
    g_strfreev(items);
}

static void build_new_term_with_profile_submenu(MenuTree *mtree,
        GCallback callback, GtkMenuShell *mshell, char **items)
{
    int n;

    for (n = 0; items[n]; ++n)
    {
        GtkWidget *mitem = gtk_menu_item_new_with_label(items[n]);

        gtk_widget_show(mitem);
        if (callback)
        {
            g_object_set_data_full(G_OBJECT(mitem), PROFILE_NAME_KEY,
                    g_strdup(items[n]), g_free);
            g_signal_connect(mitem, "activate", callback, mtree);
        }
        gtk_menu_shell_append(mshell, mitem);
    }
}

static void rebuild_new_term_with_profile_submenu(MenuTree *mtree,
        GCallback callback, GtkMenuShell *mshell, char **items)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(mshell));
    GList *child;
    int n;

    for (n = 0, child = children; child; child = g_list_next(child), ++n)
    {
        /* First two items are header and separator */
        if (n >= 2)
        {
            gtk_container_remove(GTK_CONTAINER(mshell),
                    GTK_WIDGET(child->data));
        }
    }
    build_new_term_with_profile_submenu(mtree, callback, mshell, items);
}

static void roxterm_build_pair_of_profile_submenus(MultiWin *win)
{
    char **items = dynamic_options_list_sorted(dynamic_options_get("Profiles"));
    MenuTree *menutree;

    g_return_if_fail(items);
    multi_win_set_ignore_toggles(win, TRUE);
    menutree = multi_win_get_menu_bar(win);
    if (menutree)
    {
        build_new_term_with_profile_submenu(menutree,
            G_CALLBACK(roxterm_new_window_with_profile),
            GTK_MENU_SHELL(menutree->new_win_profiles_menu), items);
        build_new_term_with_profile_submenu(menutree,
            G_CALLBACK(roxterm_new_tab_with_profile),
            GTK_MENU_SHELL(menutree->new_tab_profiles_menu), items);
    }
    menutree = multi_win_get_popup_menu(win);
    if (menutree)
    {
        build_new_term_with_profile_submenu(menutree,
            G_CALLBACK(roxterm_new_window_with_profile),
            GTK_MENU_SHELL(menutree->new_win_profiles_menu), items);
        build_new_term_with_profile_submenu(menutree,
            G_CALLBACK(roxterm_new_tab_with_profile),
            GTK_MENU_SHELL(menutree->new_tab_profiles_menu), items);
    }
    multi_win_set_ignore_toggles(win, FALSE);
    g_strfreev(items);
}

static void roxterm_add_all_pref_submenus(MultiWin *win)
{
    roxterm_add_pair_of_pref_submenus(win, dynamic_options_get("Profiles"),
            MENUTREE_PREFERENCES_SELECT_PROFILE,
            G_CALLBACK(roxterm_profile_selected));
    roxterm_build_pair_of_profile_submenus(win);
    roxterm_add_pair_of_pref_submenus(win, dynamic_options_get("Colours"),
            MENUTREE_PREFERENCES_SELECT_COLOUR_SCHEME,
            G_CALLBACK(roxterm_colour_scheme_selected));
    roxterm_add_pair_of_pref_submenus(win, dynamic_options_get("Shortcuts"),
            MENUTREE_PREFERENCES_SELECT_SHORTCUTS,
            G_CALLBACK(roxterm_shortcuts_selected));
}

static void roxterm_connect_menu_signals(MultiWin * win)
{
    multi_win_menu_connect_swapped(win, MENUTREE_EDIT_SELECT_ALL,
        G_CALLBACK(roxterm_select_all_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_EDIT_COPY,
        G_CALLBACK(roxterm_copy_clipboard_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_EDIT_PASTE,
        G_CALLBACK(roxterm_paste_clipboard_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_EDIT_COPY_AND_PASTE,
        G_CALLBACK(roxterm_copy_and_paste_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_EDIT_RESET,
        G_CALLBACK(roxterm_reset_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_EDIT_RESET_AND_CLEAR,
        G_CALLBACK(roxterm_reset_and_clear_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_EDIT_RESPAWN,
        G_CALLBACK(roxterm_respawn_action), win, NULL, NULL, NULL);

    multi_win_menu_connect_swapped(win,
            MENUTREE_PREFERENCES_EDIT_CURRENT_PROFILE,
        G_CALLBACK(roxterm_edit_profile_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win,
            MENUTREE_PREFERENCES_EDIT_CURRENT_COLOUR_SCHEME,
        G_CALLBACK(roxterm_edit_colour_scheme_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win,
            MENUTREE_PREFERENCES_EDIT_CURRENT_SHORTCUTS_SCHEME,
        G_CALLBACK(roxterm_edit_shortcuts_scheme_action),
        win, NULL, NULL, NULL);

    multi_win_menu_connect_swapped(win, MENUTREE_HELP_ABOUT,
        G_CALLBACK(roxterm_show_about), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_HELP_SHOW_MANUAL,
        G_CALLBACK(roxterm_show_manual), win, NULL, NULL, NULL);

    multi_win_menu_connect(win, MENUTREE_PREFERENCES_CONFIG_MANAGER,
        G_CALLBACK(roxterm_open_config_manager), NULL, NULL, NULL, NULL);

    multi_win_menu_connect_swapped(win, MENUTREE_OPEN_IN_BROWSER,
        G_CALLBACK(roxterm_launch_uri_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_OPEN_IN_MAILER,
        G_CALLBACK(roxterm_launch_uri_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_OPEN_IN_FILER,
        G_CALLBACK(roxterm_launch_uri_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_COPY_URI,
        G_CALLBACK(roxterm_copy_url_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_SSH_HOST,
        G_CALLBACK(roxterm_launch_uri_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_SEARCH_FIND,
        G_CALLBACK(roxterm_open_search_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_SEARCH_FIND_NEXT,
        G_CALLBACK(roxterm_find_next_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_SEARCH_FIND_PREVIOUS,
        G_CALLBACK(roxterm_find_prev_action), win, NULL, NULL, NULL);

    roxterm_add_all_pref_submenus(win);
}

static void roxterm_composited_changed_handler(VteTerminal *vte,
        ROXTermData *roxterm)
{
    roxterm_apply_colour_scheme(roxterm, vte);
}

static void roxterm_connect_misc_signals(ROXTermData * roxterm)
{
    roxterm->child_exited_tag = g_signal_connect(roxterm->widget,
            "child-exited", G_CALLBACK(roxterm_child_exited), roxterm);
    g_signal_connect(roxterm->widget, "popup-menu",
            G_CALLBACK(roxterm_popup_handler), roxterm);
    g_signal_connect(roxterm->widget, "button-press-event",
            G_CALLBACK (roxterm_click_handler), roxterm);
    g_signal_connect(roxterm->widget, "button-release-event",
            G_CALLBACK (roxterm_release_handler), roxterm);
    g_signal_connect(roxterm->widget, "window-title-changed",
        G_CALLBACK(roxterm_window_title_handler), roxterm);
    //g_signal_connect(roxterm->widget, "style-updated",
        //G_CALLBACK(roxterm_style_change_handler), roxterm);
    g_signal_connect(roxterm->widget, "char-size-changed",
        G_CALLBACK(roxterm_char_size_changed), roxterm);
    g_signal_connect(roxterm->widget, "drag-end",
        G_CALLBACK(roxterm_uri_drag_ended), roxterm);
    g_signal_connect(roxterm->widget, "drag-data-get",
        G_CALLBACK(roxterm_uri_drag_data_get), roxterm);
    g_signal_connect(roxterm->widget, "bell",
            G_CALLBACK(roxterm_bell_handler), roxterm);
    /* None of these seem to get raised on text output */
    /*
    g_signal_connect(roxterm->widget, "text-modified",
            G_CALLBACK(roxterm_text_changed_handler), roxterm);
    g_signal_connect(roxterm->widget, "text-inserted",
            G_CALLBACK(roxterm_text_changed_handler), roxterm);
    g_signal_connect(roxterm->widget, "text-deleted",
            G_CALLBACK(roxterm_text_changed_handler), roxterm);
    */
    g_signal_connect(roxterm->widget, "cursor-moved",
            G_CALLBACK(roxterm_text_changed_handler), roxterm);
    g_signal_connect(roxterm->widget, "resize-window",
            G_CALLBACK(roxterm_resize_window_handler), roxterm);
    g_signal_connect(roxterm->widget, "composited-changed",
            G_CALLBACK(roxterm_composited_changed_handler), roxterm);
    g_signal_connect(roxterm->widget, "key-press-event",
            G_CALLBACK(roxterm_key_press_handler), roxterm);
}

inline static void
roxterm_set_word_chars(ROXTermData * roxterm, VteTerminal * vte)
{
    char *wchars = options_lookup_string_with_default
        (roxterm->profile, "word_chars", "-,./?%&#:_=+@~");

    vte_terminal_set_word_char_exceptions(vte, wchars);
    if (wchars)
        g_free(wchars);
}

inline static void
roxterm_update_audible_bell(ROXTermData * roxterm, VteTerminal * vte)
{
    vte_terminal_set_audible_bell(vte, options_lookup_int
        (roxterm->profile, "audible_bell") != 0);
}

static void
roxterm_update_cursor_blink_mode(ROXTermData * roxterm, VteTerminal * vte)
{
    int o = options_lookup_int(roxterm->profile, "cursor_blink_mode");

    if (o == -1)
    {
        o = options_lookup_int(roxterm->profile, "cursor_blinks") + 1;
        if (o)
            o ^= 3;
    }
    vte_terminal_set_cursor_blink_mode(vte, o);
}

inline static void
roxterm_update_cursor_shape(ROXTermData * roxterm, VteTerminal * vte)
{
    vte_terminal_set_cursor_shape(vte, options_lookup_int_with_default
        (roxterm->profile, "cursor_shape", 0));
}

inline static void
roxterm_update_mouse_autohide(ROXTermData * roxterm, VteTerminal * vte)
{
    vte_terminal_set_mouse_autohide(vte, options_lookup_int(roxterm->profile,
            "mouse_autohide") != 0);
}

static void roxterm_set_scrollback_lines(ROXTermData * roxterm,
        VteTerminal * vte)
{
    int lines = options_lookup_int_with_default(roxterm->profile,
                "limit_scrollback", 0) ?
            options_lookup_int_with_default(roxterm->profile,
                    "scrollback_lines", 1000) :
            -1;
    vte_terminal_set_scrollback_lines(vte, lines);
}

static void roxterm_set_scroll_on_output(ROXTermData * roxterm,
        VteTerminal * vte)
{
    vte_terminal_set_scroll_on_output(vte, options_lookup_int_with_default
        (roxterm->profile, "scroll_on_output", 0));
}

static void roxterm_set_scroll_on_keystroke(ROXTermData * roxterm,
        VteTerminal * vte)
{
    vte_terminal_set_scroll_on_keystroke(vte, options_lookup_int_with_default
        (roxterm->profile, "scroll_on_keystroke", 0));
}

static void roxterm_set_backspace_binding(ROXTermData * roxterm,
        VteTerminal * vte)
{
    vte_terminal_set_backspace_binding(vte, (VteEraseBinding)
        options_lookup_int_with_default(roxterm->profile,
            "backspace_binding", VTE_ERASE_AUTO));
}

static void roxterm_set_delete_binding(ROXTermData * roxterm,
        VteTerminal * vte)
{
    vte_terminal_set_delete_binding(vte, (VteEraseBinding)
        options_lookup_int_with_default(roxterm->profile,
            "delete_binding", VTE_ERASE_AUTO));
}

inline static void roxterm_apply_wrap_switch_tab(ROXTermData *roxterm)
{
    multi_win_set_wrap_switch_tab(roxterm_get_win(roxterm),
        options_lookup_int_with_default(roxterm->profile,
            "wrap_switch_tab", FALSE));
}

inline static void roxterm_apply_always_show_tabs(ROXTermData *roxterm)
{
    multi_win_set_always_show_tabs(roxterm_get_win(roxterm),
        options_lookup_int_with_default(roxterm->profile,
            "always_show_tabs", TRUE));
}

static void roxterm_apply_disable_menu_access(ROXTermData *roxterm)
{
    static char *orig_menu_access = NULL;
    static gboolean disabled = FALSE;
    gboolean disable = options_lookup_int_with_default(roxterm->profile,
            "disable_menu_access", FALSE);
    GtkSettings *settings;
    GtkBindingSet *binding_set;

    if (disable == disabled)
        return;
    settings = gtk_settings_get_default();
    if (!orig_menu_access)
    {
        g_object_get(settings, "gtk-menu-bar-accel", &orig_menu_access, NULL);
    }
    disabled = disable;
    g_type_class_unref(g_type_class_ref(GTK_TYPE_MENU_BAR));
    g_object_set(settings, "gtk-menu-bar-accel",
            disable ? NULL: orig_menu_access,
            NULL);
    binding_set = gtk_binding_set_by_class(
            VTE_TERMINAL_GET_CLASS(roxterm->widget));
    if (disable)
    {
        gtk_binding_entry_skip(binding_set, GDK_KEY_F10, GDK_SHIFT_MASK);
    }
    else
    {
        gtk_binding_entry_remove(binding_set, GDK_KEY_F10, GDK_SHIFT_MASK);
    }
}

static void roxterm_apply_title_template(ROXTermData *roxterm)
{
    char *win_title = global_options_lookup_string("title");
    gboolean custom_win_title;
    MultiWin *win = roxterm_get_win(roxterm);

    if (win_title)
    {
        custom_win_title = TRUE;
    }
    else
    {
        custom_win_title = FALSE;
        win_title = options_lookup_string_with_default(roxterm->profile,
                    "win_title", "%s");
    }
    multi_win_set_title_template(win, win_title);
    if (custom_win_title)
    {
        global_options_reset_string("title");
        multi_win_set_title_template_locked(win, TRUE);
    }
    g_free(win_title);
}

static void roxterm_apply_show_tab_status(ROXTermData *roxterm)
{
    if (roxterm->tab)
    {
        const char *name;

        if (options_lookup_int_with_default(roxterm->profile,
                "show_tab_status", FALSE))
        {
            name = roxterm->status_icon_name;
        }
        else
        {
            name = NULL;
        }
        multi_tab_set_status_icon_name(roxterm->tab, name);
    }
}

/*
static void roxterm_apply_match_files(ROXTermData *roxterm, VteTerminal *vte)
{
    if (options_lookup_int_with_default(roxterm->profile,
            "match_plain_files", FALSE))
    {
        if (roxterm->file_match_tag[0] == -1)
            roxterm_add_file_matches(roxterm, vte);
    }
    else
    {
        if (roxterm->file_match_tag[0] != -1)
            roxterm_remove_file_matches(roxterm, vte);
    }

}
*/

inline static void roxterm_apply_middle_click_tab(ROXTermData *roxterm)
{
    multi_tab_set_middle_click_tab_action(roxterm->tab,
            options_lookup_int_with_default(roxterm->profile,
                    "middle_click_tab", 0));
}

static void roxterm_apply_colour_scheme_from_profile(ROXTermData *roxterm)
{
    char *scheme = options_lookup_string(roxterm->profile, "colour_scheme");
    if (scheme && scheme[0])
    {
        roxterm_change_colour_scheme_by_name(roxterm, scheme);
    }
    g_free(scheme);
}

static void roxterm_apply_show_add_tab_btn(ROXTermData *roxterm)
{
    MultiWin *win = roxterm_get_win(roxterm);
    if (win)
    {
        multi_win_set_show_add_tab_button(win,
                options_lookup_int_with_default(roxterm->profile,
                        "show_add_tab_btn", 1));
    }
}

static void
roxterm_apply_bold_is_bright(ROXTermData *roxterm, VteTerminal *vte)
{
    vte_terminal_set_bold_is_bright(vte,
            options_lookup_int_with_default(roxterm->profile, "bold_is_bright",
                FALSE));
}

static void
roxterm_apply_text_blink_mode(ROXTermData *roxterm, VteTerminal *vte)
{
    static VteTextBlinkMode modes[] = { VTE_TEXT_BLINK_NEVER,
        VTE_TEXT_BLINK_FOCUSED, VTE_TEXT_BLINK_UNFOCUSED, 
        VTE_TEXT_BLINK_ALWAYS };
    int i = options_lookup_int_with_default(roxterm->profile,
            "text_blink_mode", 0);
    if (i < 0 || i >= (int) G_N_ELEMENTS(modes))
    {
        g_warning("Value %d out of range for 'text_blink_mode' option", i);
        i = 0;
    }
    vte_terminal_set_text_blink_mode(vte, modes[i]);
}

static void roxterm_apply_profile(ROXTermData *roxterm, VteTerminal *vte,
        gboolean update_geometry)
{
    roxterm_set_word_chars(roxterm, vte);
    roxterm_update_audible_bell(roxterm, vte);
    roxterm_update_cursor_blink_mode(roxterm, vte);
    roxterm_update_cursor_shape(roxterm, vte);

    roxterm_apply_colour_scheme(roxterm, vte);

    roxterm_update_font(roxterm, vte, update_geometry);

    roxterm_apply_bold_is_bright(roxterm, vte);
    roxterm_apply_text_blink_mode(roxterm, vte);

    roxterm_set_scrollback_lines(roxterm, vte);
    roxterm_set_scroll_on_output(roxterm, vte);
    roxterm_set_scroll_on_keystroke(roxterm, vte);

    roxterm_set_backspace_binding(roxterm, vte);
    roxterm_set_delete_binding(roxterm, vte);

    roxterm_update_mouse_autohide(roxterm, vte);

    roxterm_apply_wrap_switch_tab(roxterm);

    roxterm_apply_disable_menu_access(roxterm);

    roxterm_apply_title_template(roxterm);
    roxterm_apply_show_tab_status(roxterm);
    /*roxterm_apply_match_files(roxterm, vte);*/
    roxterm_apply_middle_click_tab(roxterm);

    roxterm_apply_colour_scheme_from_profile(roxterm);

    roxterm_apply_show_add_tab_btn(roxterm);
}

static gboolean
roxterm_drag_data_received(GtkWidget *widget,
        const char *text, gulong len, gpointer data)
{
    char *sep = strstr(text, "\r\n");
    char *rejoined = NULL;
    (void) data;

    if (sep)
    {
        char **uris = g_strsplit(text, "\r\n", 0);
        int n;
        char *tmp;

        for (n = 0; uris[n]; ++n)
        {
            /* Last uri is usually a blank line so skip */
            if (!uris[n][0])
            {
                continue;
            }
            /* escape ' chars */
            if (strchr(uris[n], '\''))
            {
                char **split = g_strsplit(uris[n], "'", 0);

                tmp = g_strjoinv("'\\''", split);
                g_free(uris[n]);
                uris[n] = tmp;
            }
            tmp = g_strdup_printf("%s%s%s", rejoined ? rejoined : "",
                    rejoined ? "' '" : "'", uris[n]);
            g_free(rejoined);
            rejoined = tmp;
        }
        if (rejoined && rejoined[0])
        {
            tmp = g_strdup_printf("%s' ", rejoined);
            g_free(rejoined);
            rejoined = tmp;
        }
        text = rejoined;
        if (text)
            len = strlen(text);
        g_strfreev(uris);
    }
    if (text)
        vte_terminal_feed_child(VTE_TERMINAL(widget), text, len);
    if (rejoined)
        g_free(rejoined);
    return TRUE;
}

static gboolean roxterm_window_state_changed(GtkWidget *win,
        GdkEventWindowState *event, ROXTermData *roxterm)
{
    (void) win;
    roxterm->maximise =
            (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
    return FALSE;
}

inline static void
roxterm_attach_state_changed_handler(ROXTermData *roxterm)
{
    roxterm->win_state_changed_tag =
            g_signal_connect(roxterm_get_toplevel(roxterm),
            "window-state-event",
            G_CALLBACK(roxterm_window_state_changed), roxterm);
}

static void
roxterm_tab_received(GtkWidget *rcvd_widget, ROXTermData *roxterm)
{
    MultiTab *tab = multi_tab_get_from_widget(rcvd_widget);

    if (tab != roxterm->tab)
    {
        MultiWin *win = roxterm_get_win(roxterm);
        int page_num = multi_tab_get_page_num(roxterm->tab);

        if (multi_tab_get_parent(tab) == win)
            multi_tab_move_to_position(tab, page_num, TRUE);
        else
            multi_tab_move_to_new_window(win, tab, page_num);
    }
}

inline static GtkAdjustment *roxterm_get_vte_hadjustment(VteTerminal *vte)
{
    return gtk_scrollable_get_hadjustment(GTK_SCROLLABLE(vte));
}

inline static GtkAdjustment *roxterm_get_vte_vadjustment(VteTerminal *vte)
{
    return gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(vte));
}

static GtkWidget *roxterm_multi_tab_filler(MultiWin * win, MultiTab * tab,
    ROXTermData * roxterm_template, ROXTermData ** roxterm_out,
    GtkWidget ** vte_widget, GtkAdjustment **adjustment)
{
    VteTerminal *vte;
    const char *title_orig;
    ROXTermData *roxterm = roxterm_data_clone(roxterm_template);
    int hide_menu_bar;
    MultiWinScrollBar_Position scrollbar_pos;
    char *tab_name;
    gboolean custom_tab_name = FALSE;
    MultiWin *template_win = roxterm_get_win(roxterm_template);
    GtkWidget *viewport = NULL;

    roxterm_terms = g_list_append(roxterm_terms, roxterm);

    if (template_win)
    {
        hide_menu_bar = !multi_win_get_show_menu_bar(template_win);
    }
    else
    {
        hide_menu_bar = global_options_lookup_int("hide_menubar");
        if (hide_menu_bar == -1)
        {
            hide_menu_bar = options_lookup_int(roxterm_template->profile,
                "hide_menubar") == 1;
        }
    }
    multi_win_set_show_menu_bar(win, !hide_menu_bar);

    roxterm->tab = tab;
    *roxterm_out = roxterm;

    roxterm->widget = vte_terminal_new();
    vte_terminal_set_size(VTE_TERMINAL(roxterm->widget),
            roxterm->columns, roxterm->rows);
    gtk_widget_grab_focus(roxterm->widget);
    vte = VTE_TERMINAL(roxterm->widget);
    if (vte_widget)
        *vte_widget = roxterm->widget;
    if (adjustment)
        *adjustment = roxterm_get_vte_vadjustment(vte);

    scrollbar_pos = multi_win_set_scroll_bar_position(win,
        options_lookup_int_with_default(roxterm_template->profile,
            "scrollbar_pos", MultiWinScrollBar_Right));
    if (scrollbar_pos)
    {
        /* gnome-terminal packs a separate scrollbar and vte into an HBox, but
         * doing the same here causes warnings about the scrollbar getting 
         * allocated a size without calling get_preferred_*. Hopefully using
         * a GtkScrolledWindow will fix that, and also has the advantage of
         * enabling the use of overlay scrollbars.
         */
        viewport = gtk_scrolled_window_new(roxterm_get_vte_hadjustment(vte),
                        roxterm_get_vte_vadjustment(vte));
        GtkScrolledWindow *sw = GTK_SCROLLED_WINDOW(viewport);
        gtk_scrolled_window_set_policy(sw,
                GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
        gtk_scrolled_window_set_overlay_scrolling(sw,
            options_lookup_int_with_default(roxterm_template->profile,
                "overlay_scrollbar", TRUE));
        gtk_scrolled_window_set_placement(sw,
                (scrollbar_pos == MultiWinScrollBar_Left) ?
                GTK_CORNER_TOP_RIGHT : GTK_CORNER_TOP_LEFT);
        gtk_container_add(GTK_CONTAINER(viewport), roxterm->widget);
        gtk_widget_show_all(viewport);
    }
    else
    {
        gtk_widget_show(roxterm->widget);
    }

    roxterm_add_matches(roxterm, vte);

    roxterm_apply_profile(roxterm, vte, FALSE);
    tab_name = global_options_lookup_string("tab-name");
    if (tab_name)
    {
        custom_tab_name = TRUE;
    }
    else
    {
        custom_tab_name = FALSE;
        tab_name = options_lookup_string_with_default(roxterm->profile,
                "title_string", "%t. %s");
    }
    multi_tab_set_window_title_template(tab, tab_name);
    multi_tab_set_title_template_locked(tab, custom_tab_name);
    if (custom_tab_name)
    {
        global_options_reset_string("tab-name");
    }
    g_free(tab_name);
    title_orig = vte_terminal_get_window_title(vte);
    multi_tab_set_window_title(tab, title_orig ? title_orig : _("ROXTerm"));

    //g_debug("call roxterm_set_vte_size from line %d", __LINE__);
    //roxterm_set_vte_size(roxterm, vte, roxterm->columns, roxterm->rows);

    multi_tab_connect_tab_selection_handler(win,
        (MultiTabSelectionHandler) roxterm_tab_selection_handler);
    roxterm->drd = drag_receive_setup_dest_widget(roxterm->widget,
            roxterm_drag_data_received,
            (DragReceiveTabHandler) roxterm_tab_received,
            roxterm);

    roxterm_attach_state_changed_handler(roxterm);

    g_idle_add((GSourceFunc) run_child_when_idle, roxterm);

    return viewport ? viewport : roxterm->widget;
}

VteTerminal *roxterm_get_current_vte_ptr(MultiWin *win) {
    ROXTermData *roxterm = multi_win_get_user_data_for_current_tab(win);
    VteTerminal *vte = VTE_TERMINAL(roxterm->widget);
    return vte;
}

static void roxterm_multi_tab_destructor(ROXTermData * roxterm)
{
    roxterm_terms = g_list_remove(roxterm_terms, roxterm);
    roxterm_data_delete(roxterm);
}

/* Returns FALSE if the value hasn't really changed */
static gboolean
roxterm_update_option(Options * opts, const char *key,
    OptsDBusOptType opt_type, OptsDBusValue val)
{
    gboolean result = TRUE;
    char *oldval;

    switch (opt_type)
    {
        case OptsDBus_StringOpt:
            oldval = options_lookup_string(opts, key);
            if (g_strcmp0(oldval, val.s))
                options_set_string(opts, key, val.s);
            else
                result = FALSE;
            if (oldval)
                g_free(oldval);
            return result;
        case OptsDBus_IntOpt:
            if (options_lookup_int(opts, key) == val.i)
                return FALSE;
            options_set_int(opts, key, val.i);
            return TRUE;
        case OptsDBus_FloatOpt:
            if (options_lookup_double(opts, key) == val.f)
                return FALSE;
            options_set_double(opts, key, val.f);
            return TRUE;
        default:
            g_critical(_("Unknown option type (%d)"), opt_type);
    }
    return FALSE;
}

static gboolean roxterm_get_show_tab_close_button(ROXTermData *roxterm)
{
    return options_lookup_int_with_default(roxterm->profile,
            "tab_close_btn", TRUE);
}

static gboolean roxterm_get_new_tab_adjacent(ROXTermData *roxterm)
{
    return options_lookup_int_with_default(roxterm->profile,
            "new_tabs_adjacent", FALSE);
}

static void roxterm_reflect_profile_change(Options * profile, const char *key)
{
    GList *link;

    for (link = roxterm_terms; link; link = g_list_next(link))
    {
        ROXTermData *roxterm = link->data;
        VteTerminal *vte;
        MultiWin *win = roxterm_get_win(roxterm);
        gboolean apply_to_win = FALSE;

        if (roxterm->profile != profile || roxterm->profile->deleted)
            continue;

        vte = VTE_TERMINAL(roxterm->widget);
        if (!strcmp(key, "font"))
        {
            roxterm_apply_profile_font(roxterm, vte, TRUE);
            apply_to_win = TRUE;
        }
        else if (!strcmp(key, "vspacing"))
        {
            roxterm_apply_vspacing(roxterm, vte);
            apply_to_win = TRUE;
        }
        else if (!strcmp(key, "hspacing"))
        {
            roxterm_apply_hspacing(roxterm, vte);
            apply_to_win = TRUE;
        }
        else if (!strcmp(key, "bold_is_bright"))
        {
            roxterm_apply_bold_is_bright(roxterm, vte);
        }
        else if (!strcmp(key, "text_blink_mode"))
        {
            roxterm_apply_text_blink_mode(roxterm, vte);
        }
        else if (!strcmp(key, "hide_menubar") &&
            multi_win_get_current_tab(win) == roxterm->tab)
        {
            multi_win_set_show_menu_bar(win,
                !options_lookup_int(roxterm->profile, "hide_menubar"));
        }
        else if (!strcmp(key, "audible_bell"))
        {
            roxterm_update_audible_bell(roxterm, vte);
        }
        else if (!strcmp(key, "cursor_blink_mode"))
        {
            roxterm_update_cursor_blink_mode(roxterm, vte);
        }
        else if (!strcmp(key, "cursor_shape"))
        {
            roxterm_update_cursor_shape(roxterm, vte);
        }
        else if (!strcmp(key, "mouse_autohide"))
        {
            roxterm_update_mouse_autohide(roxterm, vte);
        }
        else if (!strcmp(key, "word_chars"))
        {
            roxterm_set_word_chars(roxterm, vte);
        }
        else if (!strcmp(key, "width") || !strcmp(key, "height"))
        {
            roxterm_update_size(roxterm, vte);
            apply_to_win = TRUE;
        }
        else if (!strcmp(key, "maximise"))
        {
            roxterm->maximise = options_lookup_int(roxterm->profile,
                    "maximise");
            if (roxterm->maximise)
            {
                gtk_window_maximize(roxterm_get_toplevel(roxterm));
            }
            else
            {
                gtk_window_unmaximize(roxterm_get_toplevel(roxterm));
            }
            apply_to_win = TRUE;
        }
        else if (!strcmp(key, "full_screen"))
        {
            int fs = options_lookup_int(roxterm->profile, "full_screen");
            multi_win_set_fullscreen(win, fs);
            apply_to_win = TRUE;
        }
        else if (!strcmp(key, "borderless"))
        {
            int fs = options_lookup_int(roxterm->profile, "borderless");
            multi_win_set_borderless(win, fs);
            apply_to_win = TRUE;
        }
        else if (!strcmp(key, "saturation"))
        {
            roxterm_apply_colour_scheme(roxterm, vte);
        }
        else if (!strcmp(key, "scrollback_lines") ||
                !strcmp(key, "limit_scrollback"))
        {
            roxterm_set_scrollback_lines(roxterm, vte);
        }
        else if (!strcmp(key, "scroll_on_output"))
        {
            roxterm_set_scroll_on_output(roxterm, vte);
        }
        else if (!strcmp(key, "scroll_on_keystroke"))
        {
            roxterm_set_scroll_on_keystroke(roxterm, vte);
        }
        else if (!strcmp(key, "backspace_binding"))
        {
            roxterm_set_backspace_binding(roxterm, vte);
        }
        else if (!strcmp(key, "delete_binding"))
        {
            roxterm_set_delete_binding(roxterm, vte);
        }
        else if (!strcmp(key, "wrap_switch_tab"))
        {
            roxterm_apply_wrap_switch_tab(roxterm);
        }
        else if (!strcmp(key, "always_show_tabs"))
        {
            roxterm_apply_always_show_tabs(roxterm);
        }
        else if (!strcmp(key, "show_add_tab_btn"))
        {
            roxterm_apply_show_add_tab_btn(roxterm);
        }
        else if (!strcmp(key, "disable_menu_access"))
        {
            roxterm_apply_disable_menu_access(roxterm);
        }
        else if (!strcmp(key, "disable_menu_shortcuts"))
        {
            gboolean disable = options_lookup_int(roxterm->profile,
                    "disable_menu_shortcuts");
            MenuTree *mtree = multi_win_get_menu_bar(win);

            menutree_disable_shortcuts(mtree, disable);
        }
        else if (!strcmp(key, "disable_tab_menu_shortcuts"))
        {
            gboolean disable = options_lookup_int(roxterm->profile,
                    "disable_tab_menu_shortcuts");
            MenuTree *mtree = multi_win_get_popup_menu(win);

            menutree_disable_tab_shortcuts(mtree, disable);
            mtree = multi_win_get_menu_bar(win);
            menutree_disable_tab_shortcuts(mtree, disable);
        }
        else if (!strcmp(key, "title_string"))
        {
            multi_tab_set_window_title_template(roxterm->tab,
                    options_lookup_string_with_default(roxterm->profile,
                            "title_string", "%t. %s"));
        }
        else if (!strcmp(key, "win_title"))
        {
            multi_win_set_title_template(win,
                    options_lookup_string_with_default(roxterm->profile,
                            "win_title", "%s"));
        }
        else if (!strcmp(key, "tab_close_btn"))
        {
            if (roxterm_get_show_tab_close_button(roxterm))
                multi_tab_add_close_button(roxterm->tab);
            else
                multi_tab_remove_close_button(roxterm->tab);
        }
        else if (!strcmp(key, "show_tab_status"))
        {
            roxterm_apply_show_tab_status(roxterm);
        }
        /*
        else if (!strcmp(key, "match_plain_files"))
        {
            roxterm_apply_match_files(roxterm, vte);
        }
        */
        else if (!strcmp(key, "middle_click_tab"))
        {
            roxterm_apply_middle_click_tab(roxterm);
        }
        else if (!strcmp(key, "colour_scheme"))
        {
            roxterm_apply_colour_scheme_from_profile(roxterm);
        }
        if (apply_to_win)
        {
            multi_win_foreach_tab(win, match_text_size_foreach_tab, roxterm);
        }
    }
}

static gboolean roxterm_update_colour_option(Options *scheme, const char *key,
        const char *value)
{
    void (*setter)(Options *, const char *) = NULL;
    GdkRGBA *old_colour;
    GdkRGBA *pnew_colour = NULL;
    GdkRGBA  new_colour;

    if (value)
    {
        g_return_val_if_fail(gdk_rgba_parse(&new_colour, value), FALSE);
        pnew_colour = &new_colour;
    }
    if (!strcmp(key, "foreground"))
    {
        old_colour = colour_scheme_get_foreground_colour(scheme, TRUE);
        setter = colour_scheme_set_foreground_colour;
    }
    else if (!strcmp(key, "background"))
    {
        old_colour = colour_scheme_get_background_colour(scheme, TRUE);
        setter = colour_scheme_set_background_colour;
    }
    else if (!strcmp(key, "cursor"))
    {
        old_colour = colour_scheme_get_cursor_colour(scheme, TRUE);
        setter = colour_scheme_set_cursor_colour;
    }
    else if (!strcmp(key, "cursorfg"))
    {
        old_colour = colour_scheme_get_cursorfg_colour(scheme, TRUE);
        setter = colour_scheme_set_cursorfg_colour;
    }
    else if (!strcmp(key, "bold"))
    {
        old_colour = colour_scheme_get_bold_colour(scheme, TRUE);
        setter = colour_scheme_set_bold_colour;
    }
    else
    {
        old_colour = colour_scheme_get_palette(scheme) + atoi(key);
    }
    if (!old_colour && !pnew_colour)
        return FALSE;
    if (old_colour && pnew_colour && gdk_rgba_equal(old_colour, pnew_colour))
        return FALSE;
    if (setter)
        setter(scheme, value);
    else
        colour_scheme_set_palette_entry(scheme, atoi(key), value);
    return TRUE;
}

static gboolean roxterm_update_palette_size(Options *scheme, int size)
{
    if (colour_scheme_get_palette_size(scheme) == size)
        return FALSE;
    colour_scheme_set_palette_size(scheme, size);
    return TRUE;
}

static void roxterm_reflect_colour_change(Options *scheme, const char *key)
{
    GList *link;

    for (link = roxterm_terms; link; link = g_list_next(link))
    {
        ROXTermData *roxterm = link->data;
        VteTerminal *vte;

        if (roxterm->colour_scheme != scheme || roxterm->colour_scheme->deleted)
            continue;

        vte = VTE_TERMINAL(roxterm->widget);

        if (!strcmp(key, "cursor"))
            roxterm_update_cursor_colour(roxterm, vte);
        else if (!strcmp(key, "cursorfg"))
            roxterm_update_cursorfg_colour(roxterm, vte);
        else if (!strcmp(key, "bold"))
            roxterm_update_bold_colour(roxterm, vte);
        else
            roxterm_apply_colour_scheme(roxterm, vte);
    }
}

static void
roxterm_opt_signal_handler(const char *profile_name, const char *key,
    OptsDBusOptType opt_type, OptsDBusValue val)
{
    if (!strncmp(profile_name, "Profiles/", 9))
    {
        const char *short_profile_name = profile_name + 9;
        Options *profile = dynamic_options_lookup_and_ref(roxterm_profiles,
            short_profile_name, "roxterm profile");

        if (roxterm_update_option(profile, key, opt_type, val))
            roxterm_reflect_profile_change(profile, key);
        dynamic_options_unref(roxterm_profiles, short_profile_name);
    }
    else if (!strncmp(profile_name, "Colours/", 8))
    {
        const char *scheme_name = profile_name + 8;
        Options *scheme = colour_scheme_lookup_and_ref(scheme_name);
        gboolean changed = FALSE;

        if (!strcmp(key, "palette_size"))
            changed = roxterm_update_palette_size(scheme, val.i);
        else
            changed = roxterm_update_colour_option(scheme, key, val.s);
        if (changed)
            roxterm_reflect_colour_change(scheme, key);
        colour_scheme_unref(scheme);
    }
    else if (!strcmp(profile_name, "Global") &&
            (!strcmp(key, "warn_close") ||
            !strcmp(key, "only_warn_running") ||
            !strcmp(key, "prefer_dark_theme")))
    {
        options_set_int(global_options, key, val.i);
        if (!strcmp(key, "prefer_dark_theme"))
            global_options_apply_dark_theme();
    }
    else
    {
        g_critical(_("Profile/key '%s/%s' not understood"), profile_name, key);
    }
}

/* data is cast to char const **pname; if the deleted item is currently
 * selected *pname is changed to NULL */
static void delete_name_from_menu(GtkWidget *widget, gpointer data)
{
    const char *profile_name = g_object_get_data(G_OBJECT(widget),
            PROFILE_NAME_KEY);
    char const **pname = data;

    g_return_if_fail(profile_name);
    /* pname may have been NULLed by an earlier call */
    if (!*pname)
        return;
    if (!strcmp(profile_name, *pname))
    {
        if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
            *pname = NULL;
        gtk_widget_destroy(widget);
    }
}

/* Casts data to GtkWidget ** and sets target to found widget if it finds an
 * entry with no PROFILE_NAME_KEY */
static void find_unhandled_menu_entry(GtkWidget *widget, gpointer data)
{
    if (!g_object_get_data(G_OBJECT(widget), PROFILE_NAME_KEY))
        *((GtkWidget **) data) = widget;
}

static GtkWidget *append_name_to_menu(MenuTree *mtree, GtkMenu *menu,
        const char *name, GCallback handler)
{
    GSList *group = NULL;
    GtkWidget *mitem;

    gtk_container_foreach(GTK_CONTAINER(menu),
            menutree_find_radio_group, &group);
    mitem = create_radio_menu_item(mtree, name, &group, handler);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mitem);
    return mitem;
}

/* Sets *pname to NULL if it's currently selected */
inline static void remove_name_from_menu(GtkMenu *menu, char const **pname)
{
    gtk_container_foreach(GTK_CONTAINER(menu),
            delete_name_from_menu, (gpointer) pname);
}

static void stuff_changed_do_menu(MenuTree *mtree,
        const char *what_happened, const char *family_name,
        const char *current_name, const char *new_name)
{
    MenuTreeID id;
    GtkMenu *menu;
    GCallback handler = NULL;

    if (!strcmp(family_name, "Profiles"))
    {
        char **items = dynamic_options_list_sorted(
                dynamic_options_get("Profiles"));

        char *s = (char *) "Current profiles:";
        for (char **it = items; *it; ++it)
        {
            s = g_strdup_printf("%s %s", s, *it);
        }

        id = MENUTREE_PREFERENCES_SELECT_PROFILE;
        handler = G_CALLBACK(roxterm_profile_selected);

        menu = menutree_submenu_from_id(mtree,
                MENUTREE_FILE_NEW_WINDOW_WITH_PROFILE);
        rebuild_new_term_with_profile_submenu(mtree,
            G_CALLBACK(roxterm_new_window_with_profile),
            GTK_MENU_SHELL(mtree->new_win_profiles_menu), items);
        rebuild_new_term_with_profile_submenu(mtree,
            G_CALLBACK(roxterm_new_tab_with_profile),
            GTK_MENU_SHELL(mtree->new_tab_profiles_menu), items);
        g_strfreev(items);
    }
    else if (!strcmp(family_name, "Colours"))
    {
        id = MENUTREE_PREFERENCES_SELECT_COLOUR_SCHEME;
        handler = G_CALLBACK(roxterm_colour_scheme_selected);
    }
    else if (!strcmp(family_name, "Shortcuts"))
    {
        id = MENUTREE_PREFERENCES_SELECT_SHORTCUTS;
        handler = G_CALLBACK(roxterm_shortcuts_selected);
    }
    else
    {
        g_warning(_("Changed options family '%s' not known"), family_name);
        return;
    }
    g_return_if_fail((menu = menutree_submenu_from_id(mtree, id)) != NULL);

    if (!strcmp(what_happened, OPTSDBUS_DELETED))
    {
        GtkWidget *unhandled_mitem = NULL;

        remove_name_from_menu(menu, &current_name);
        gtk_container_foreach(GTK_CONTAINER(menu),
                find_unhandled_menu_entry, &unhandled_mitem);
        if (!unhandled_mitem)
        {
            unhandled_mitem = append_name_to_menu(mtree, menu,
                    _("[Deleted]"), NULL);
            gtk_widget_set_sensitive(unhandled_mitem, FALSE);
        }
        /* If deleted item was selected, select unhandled item in its place
         */
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(unhandled_mitem),
                current_name == NULL);
    }
    else if (!strcmp(what_happened, OPTSDBUS_ADDED))
    {
        append_name_to_menu(mtree, menu, current_name, handler);
    }
    else if (!strcmp(what_happened, OPTSDBUS_RENAMED))
    {
        GtkWidget *mitem;

        remove_name_from_menu(menu, &current_name);
        mitem = append_name_to_menu(mtree, menu, new_name, handler);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mitem),
                current_name == NULL);
    }
    else
    {
        g_warning(_("Options family change action '%s' not known"),
                what_happened);
    }
}

void roxterm_stuff_changed_handler(const char *what_happened,
        const char *family_name, const char *current_name,
        const char *new_name)
{
    GList *link;
    DynamicOptions *dynopts = NULL;
    Options *options = NULL;

    if (!strcmp(what_happened, OPTSDBUS_CHANGED) &&
            strcmp(family_name, "Shortcuts"))
    {
        for (link = roxterm_terms; link; link = g_list_next(link))
        {
            ROXTermData *roxterm = link->data;

            if (!strcmp(family_name, "Profiles"))
            {
                if (!strcmp(options_get_leafname(roxterm->profile),
                        current_name))
                {
                    roxterm_apply_profile(roxterm,
                            VTE_TERMINAL(roxterm->widget), TRUE);
                }
            }
            else if (!strcmp(family_name, "Colours"))
            {
                if (!strcmp(options_get_leafname(roxterm->colour_scheme),
                        current_name))
                {
                    roxterm_apply_colour_scheme(roxterm,
                            VTE_TERMINAL(roxterm->widget));
                }
            }
        }
        return;
    }

    if (!strcmp(what_happened, OPTSDBUS_DELETED))
    {
        if (options)
        {
            dynamic_options_forget(dynopts, current_name);
            options->deleted = TRUE;
        }
    }
    else if (!strcmp(what_happened, OPTSDBUS_RENAMED))
    {
        if (options)
        {
            options_change_leafname(options, new_name);
            dynamic_options_rename(dynopts, current_name, new_name);
        }
    }

    for (link = multi_win_all; link; link = g_list_next(link))
    {
        MultiWin *win = (MultiWin *) link->data;

        if (strcmp(what_happened, OPTSDBUS_CHANGED))
        {
            MenuTree *mtree = multi_win_get_menu_bar(win);

            multi_win_set_ignore_toggles(win, TRUE);
            if (mtree)
            {
                stuff_changed_do_menu(mtree, what_happened, family_name,
                        current_name, new_name);
            }
            mtree = multi_win_get_popup_menu(win);
            if (mtree)
            {
                stuff_changed_do_menu(mtree, what_happened, family_name,
                        current_name, new_name);
            }
            multi_win_set_ignore_toggles(win, FALSE);
        }
        else if (!strcmp(family_name, "Shortcuts"))
        {
            Options *shortcuts = shortcuts_open(current_name, TRUE);
            multi_win_set_shortcut_scheme(win, shortcuts);
            shortcuts_unref(shortcuts);
        }
    }
}

static gboolean roxterm_verify_id(ROXTermData *roxterm)
{
    GList *link;

    for (link = roxterm_terms; link; link = g_list_next(link))
    {
        if (link->data == roxterm)
            return TRUE;
    }
    g_warning(_("Invalid ROXTERM_ID %p in D-Bus message "
            "(this is expected if you used roxterm's --separate option)"),
            roxterm);
    return FALSE;
}

static void roxterm_set_profile_handler(ROXTermData *roxterm, const char *name)
{
    if (!roxterm_verify_id(roxterm))
        return;

    Options *profile = dynamic_options_lookup_and_ref(roxterm_profiles, name,
            "roxterm profile");

    if (profile)
    {
        roxterm_change_profile(roxterm, profile);
        multi_win_foreach_tab(roxterm_get_win(roxterm),
                match_text_size_foreach_tab, roxterm);
        options_unref(profile);
    }
    else
    {
        dlg_warning(roxterm_get_toplevel(roxterm),
                _("Unknown profile '%s' in D-Bus signal"), name);
    }
}

static void roxterm_set_colour_scheme_handler(ROXTermData *roxterm,
        const char *name)
{
    if (!roxterm_verify_id(roxterm))
        return;
    roxterm_change_colour_scheme_by_name(roxterm, name);
}

static void roxterm_set_shortcut_scheme_handler(ROXTermData *roxterm,
        const char *name)
{
    if (!roxterm_verify_id(roxterm))
        return;

    Options *shortcuts = shortcuts_open(name, TRUE);
    multi_win_set_shortcut_scheme(roxterm_get_win(roxterm), shortcuts);
    shortcuts_unref(shortcuts);
}

static GtkPositionType get_profile_tab_pos(Options *profile)
{
    switch (options_lookup_int_with_default(profile, "tab_pos", 0))
    {
        case 0:
            return GTK_POS_TOP;
        case 1:
            return GTK_POS_BOTTOM;
        case 2:
            return GTK_POS_LEFT;
        case 3:
            return GTK_POS_RIGHT;
        case 4:
            return -1;
        default:
            return GTK_POS_TOP;
    }
}

inline static GtkPositionType roxterm_get_tab_pos(const ROXTermData *roxterm)
{
    return get_profile_tab_pos(roxterm->profile);
}

static gboolean roxterm_get_always_show_tabs(const ROXTermData *roxterm)
{
    return (gboolean) options_lookup_int_with_default(roxterm->profile,
            "always_show_tabs", TRUE);
}

/* Takes over ownership of profile and non-const strings;
 * on exit size_on_cli indicates whether dimensions were given in *geom;
 * geom is freed and set to NULL if it's invalid
 */
static ROXTermData *roxterm_data_new(double zoom_factor, const char *directory,
        char *profile_name, Options *profile, gboolean maximise,
        const char *colour_scheme_name,
        char **geom, gboolean *size_on_cli, char **env)
{
    ROXTermData *roxterm = g_new0(ROXTermData, 1);
    int width, height, x, y;
    (void) profile_name;

    roxterm->status_icon_name = NULL;
    roxterm->target_zoom_factor = zoom_factor;
    roxterm->current_zoom_factor = 1.0;
    if (!roxterm->target_zoom_factor)
    {
        roxterm->target_zoom_factor = 1.0;
        roxterm->zoom_index = -1;
    }
    else
    {
        roxterm->zoom_index = multi_win_get_nearest_index_for_zoom(
                roxterm->target_zoom_factor);
    }
    roxterm->directory = directory ? g_strdup(directory) : g_get_current_dir();
    roxterm->profile = profile;
    if (size_on_cli)
        *size_on_cli = FALSE;
    if (*geom)
    {
        if (multi_win_parse_geometry(*geom, &width, &height, &x, &y, NULL))
        {
            roxterm->columns = width;
            roxterm->rows = height;
            if (size_on_cli)
                *size_on_cli = TRUE;
        }
        else
        {
            dlg_warning(NULL, _("Invalid geometry specification"));
            *geom = NULL;
        }
    }
    roxterm->maximise = maximise;
    roxterm->borderless = options_lookup_int_with_default(profile,
                                        "borderless", 0);
    if (colour_scheme_name)
    {
        roxterm->colour_scheme = colour_scheme_lookup_and_ref
            (colour_scheme_name);
    }
    roxterm->pid = -1;
    roxterm->env = global_options_copy_strv(env);
    /*roxterm->file_match_tag[0] = roxterm->file_match_tag[1] = -1;*/
    roxterm->exit_action = Roxterm_ChildExitNotOverridden;
    return roxterm;
}

void roxterm_launch(char **env)
{
    GtkPositionType tab_pos;
    gboolean always_show_tabs;
    char *shortcut_scheme;
    Options *shortcuts;
    char *geom = options_lookup_string(global_options, "geometry");
    gboolean size_on_cli = FALSE;
    char *profile_name =
            global_options_lookup_string_with_default("profile", "Default");
    char *colour_scheme_name =
            global_options_lookup_string_with_default("colour_scheme", "GTK");
    Options *profile = dynamic_options_lookup_and_ref(roxterm_get_profiles(),
            profile_name, "roxterm profile");
    MultiWin *win = NULL;
    ROXTermData *roxterm = roxterm_data_new(
            global_options_lookup_double("zoom"),
            global_options_directory,
            profile_name, profile,
            global_options_maximise ||
                    options_lookup_int_with_default(profile,
                            "maximise", 0),
            colour_scheme_name,
            &geom, &size_on_cli, env);
    int show_add_tab_btn;

    if (!size_on_cli)
    {
        roxterm_default_size_func(roxterm, &roxterm->columns, &roxterm->rows);
    }
    if (global_options_commandv)
    {
        roxterm->commandv = global_options_copy_strv(global_options_commandv);
    }
    tab_pos = roxterm_get_tab_pos(roxterm);
    always_show_tabs = roxterm_get_always_show_tabs(roxterm);
    shortcut_scheme = global_options_lookup_string_with_default
        ("shortcut_scheme", "Default");
    shortcuts = shortcuts_open(shortcut_scheme, FALSE);
    if (global_options_tab)
    {
        ROXTermData *partner;
        char *wtitle = global_options_lookup_string("title");
        MultiWin *best_inactive = NULL;
        GList *link;

        win = NULL;
        for (link = multi_win_all; link; link = g_list_next(link))
        {
            GtkWidget *w;
            gboolean title_ok;

            win = link->data;
            w = multi_win_get_widget(win);
            title_ok = !wtitle ||
                    !g_strcmp0(multi_win_get_title_template(win), wtitle);
            if (gtk_window_is_active(GTK_WINDOW(w)) && title_ok)
            {
                break;
            }
            if (title_ok && !best_inactive)
                best_inactive = win;
            win = NULL;
        }
        if (!win)
        {
            if (best_inactive)
                win = best_inactive;
        }
        partner = win ? multi_win_get_user_data_for_current_tab(win) : NULL;
        if (partner)
        {
            GtkWidget *gwin = multi_win_get_widget(win);

            roxterm->dont_lookup_dimensions = TRUE;
            roxterm->target_zoom_factor = partner->target_zoom_factor;
            roxterm->zoom_index = partner->zoom_index;
            if (partner->pango_desc)
            {
                roxterm->pango_desc =
                        pango_font_description_copy(partner->pango_desc);
                roxterm->current_zoom_factor = partner->current_zoom_factor;
            }
            else
            {
                roxterm->current_zoom_factor = 1.0;
            }
            /* roxterm_data_clone needs to see a widget to get geometry
             * correct */
            roxterm->widget = partner->widget;
            roxterm->tab = partner->tab;
            multi_tab_new(win, roxterm);
            if (gtk_widget_get_visible(gwin))
                gtk_window_present(GTK_WINDOW(gwin));
        }
        else
        {
            global_options_tab = FALSE;
        }
    }
    if (0 <= global_options_atexit && global_options_atexit <= 3)
    {
        roxterm->exit_action = global_options_atexit;
        roxterm->override_exit_action = TRUE;
        global_options_atexit = -1;
    }

    gboolean borderless = roxterm->borderless | global_options_borderless;

    show_add_tab_btn = options_lookup_int_with_default(roxterm->profile,
            "show_add_tab_btn", 1);
    if (global_options_tab)
    {
        global_options_tab = FALSE;
    }
    else if (global_options_fullscreen ||
            options_lookup_int_with_default(roxterm->profile, "full_screen", 0))
    {
        global_options_fullscreen = FALSE;
        win = multi_win_new_fullscreen(shortcuts,
                roxterm->zoom_index, roxterm,
                tab_pos, borderless, always_show_tabs, show_add_tab_btn);
    }
    else if (roxterm->maximise)
    {
        global_options_maximise = FALSE;
        win = multi_win_new_maximised(shortcuts,
                roxterm->zoom_index, roxterm,
                tab_pos, borderless, always_show_tabs, show_add_tab_btn);
    }
    else
    {
        if (!size_on_cli)
        {
            if (geom)
            {
                char *old_geom = geom;

                geom = g_strdup_printf("%dx%d%s",
                        roxterm->columns, roxterm->rows, old_geom);
                g_free(old_geom);
            }
            else
            {
                geom = g_strdup_printf("%dx%d",
                        roxterm->columns, roxterm->rows);
            }
        }
        win = multi_win_new_with_geom(shortcuts,
                roxterm->zoom_index, roxterm, geom,
                tab_pos, borderless, always_show_tabs, show_add_tab_btn);
    }
    g_free(geom);

    roxterm_data_delete(roxterm);
    g_free(shortcut_scheme);
    g_free(colour_scheme_name);
    g_free(profile_name);

    //g_debug("call roxterm_force_resize_now from %s:%d", __FILE__, __LINE__);
    //roxterm_force_resize_now(win);
}

static void roxterm_tab_to_new_window(MultiWin *win, MultiTab *tab,
        MultiWin *old_win)
{
    ROXTermData *roxterm = multi_tab_get_user_data(tab);
    ROXTermData *match_roxterm;
    MultiTab *match_tab = multi_win_get_current_tab(win);
    GtkWindow *old_gwin = old_win ?
            GTK_WINDOW(multi_win_get_widget(old_win)) : NULL;

    if (old_gwin)
    {
        g_signal_handler_disconnect(old_gwin, roxterm->win_state_changed_tag);
    }
    roxterm_attach_state_changed_handler(roxterm);
    if (!match_tab)
        return;
    match_roxterm = multi_tab_get_user_data(match_tab);
    if (roxterm == match_roxterm)
    {
        return;
    }
    roxterm_match_text_size(roxterm, match_roxterm);
}

static void roxterm_get_disable_menu_shortcuts(ROXTermData *roxterm,
    gboolean *general, gboolean *tabs)
{
    if (general)
    {
        *general = options_lookup_int_with_default(roxterm->profile,
            "disable_menu_shortcuts", FALSE);
    }
    if (tabs)
    {
        *tabs = options_lookup_int_with_default(roxterm->profile,
            "disable_tab_menu_shortcuts", FALSE);
    }
}

typedef struct {
    guint ntabs;
    int warn;
    gboolean only_running;
} DontShowData;

static void dont_show_again_toggled(GtkToggleButton *button, DontShowData *d)
{
    int val = 0;

    if (gtk_toggle_button_get_active(button))
    {
        if (!d->ntabs)
            val = 2;
        else if (d->ntabs > 1)
            val = 0;
        else
            val = 1;
    }
    else
    {
        val = d->warn;
    }
    options_set_int(global_options, "warn_close", val);
    options_file_save(global_options->kf, "Global");
}

static void only_running_toggled(GtkToggleButton *button, DontShowData *d)
{
    (void) d;
    options_set_int(global_options, "only_warn_running",
            gtk_toggle_button_get_active(button));
    options_file_save(global_options->kf, "Global");
}

static gboolean check_pid_has_children(pid_t pid)
{
    int fd = 0;
    ssize_t count = -1;
    char *buf = g_strdup_printf("/proc/%d/task/%d/children", pid, pid);
    size_t l = strlen(buf);

    fd = open(buf, O_RDONLY);
    if (fd == -1)
    {
        g_warning("Failed to open %s\n", buf);
        g_free(buf);
    }
    else
    {
        memset(buf, 0, l);
        count = read(fd, buf, l);
        if (count == -1)
            g_warning("Failed to read %s\n", buf);
        close(fd);
    }
    return count > 0;
}

static gboolean roxterm_check_is_running(ROXTermData *roxterm)
{
    if (roxterm && roxterm->running)
    {
        return roxterm->is_shell ? check_pid_has_children(roxterm->pid) : TRUE;
    }
    return FALSE;
}

static void check_each_tab_running(MultiTab *tab, void *data)
{
    if (roxterm_check_is_running((ROXTermData *) multi_tab_get_user_data(tab)))
        *((gboolean *) data) = TRUE;
}

static gboolean roxterm_delete_handler(GtkWindow *gtkwin, GdkEvent *event,
        gpointer data)
{
    GtkWidget *dialog;
    GtkWidget *noshow;
    GtkWidget *only_running;
    gboolean response;
    DontShowData d;
    const char *msg;
    MultiWin *win = event ? data : NULL;
    ROXTermData *roxterm = event ? NULL : data;
    GtkWidget *ca_box;
    gboolean running = FALSE;


    d.warn = global_options_lookup_int_with_default("warn_close", 3);
    d.only_running = global_options_lookup_int_with_default("only_warn_running",
            FALSE);
    d.ntabs = win ? multi_win_get_ntabs(win) : 0;
    if ((!win && d.warn < 3) || !d.warn || (d.warn == 1 && d.ntabs <= 1))
        return FALSE;
    if (d.only_running)
    {
        if (win)
        {
            multi_win_foreach_tab(win, check_each_tab_running, &running);
            if (!running)
                return FALSE;
        }
        else
        {
            if (!roxterm_check_is_running(roxterm))
                return FALSE;
        }
    }

    switch (d.ntabs)
    {
        case 0:
            msg = _("Closing this ROXTerm tab may cause loss of data. "
                    "Are you sure you want to continue?");
            break;
        case 1:
            msg = _("You are about to close a ROXTerm window; this may cause "
                    "loss of data. Are you sure you want to continue?");
            break;
        default:
            msg = _("You are about to close a window containing multiple "
                    "ROXTerm tabs; this may cause loss of data. Are you sure "
                    "you want to continue?");
            break;
    }
    dialog = gtk_message_dialog_new(gtkwin,
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO,
            "%s", msg);
    gtk_window_set_title(GTK_WINDOW(dialog), _("ROXTerm: Confirm close"));
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_NO);

    ca_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    noshow = gtk_check_button_new_with_mnemonic(_("_Don't show this again"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(noshow), FALSE);
    g_signal_connect(noshow, "toggled",
            G_CALLBACK(dont_show_again_toggled), &d);
    gtk_box_pack_start(GTK_BOX(ca_box), noshow, FALSE, FALSE, 0);
    gtk_widget_show(noshow);

    only_running = gtk_check_button_new_with_mnemonic(
            _("Only warn if tasks are still _running"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(only_running),
            d.only_running);
    g_signal_connect(only_running, "toggled",
            G_CALLBACK(only_running_toggled), &d);
    gtk_box_pack_start(GTK_BOX(ca_box), only_running, FALSE, FALSE, 0);
    gtk_widget_show(only_running);

    response = gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_YES;
    gtk_widget_destroy(dialog);
    return response;
}

void roxterm_init(void)
{
    resources_access_icon();
    gtk_window_set_default_icon_name("roxterm");

    optsdbus_listen_for_opt_signals(roxterm_opt_signal_handler);
    optsdbus_listen_for_stuff_changed_signals(roxterm_stuff_changed_handler);
    optsdbus_listen_for_set_profile_signals(
            (OptsDBusSetProfileHandler) roxterm_set_profile_handler);
    optsdbus_listen_for_set_colour_scheme_signals(
            (OptsDBusSetProfileHandler) roxterm_set_colour_scheme_handler);
    optsdbus_listen_for_set_shortcut_scheme_signals(
            (OptsDBusSetProfileHandler) roxterm_set_shortcut_scheme_handler);

    multi_tab_init((MultiTabFiller) roxterm_multi_tab_filler,
        (MultiTabDestructor) roxterm_multi_tab_destructor,
        roxterm_connect_menu_signals,
        (MultiWinGeometryFunc) roxterm_geometry_func,
        (MultiWinSizeFunc) roxterm_size_func,
        (MultiWinDefaultSizeFunc) roxterm_default_size_func,
        roxterm_tab_to_new_window,
        (MultiWinZoomHandler) roxterm_set_zoom_factor,
        (MultiWinGetDisableMenuShortcuts) roxterm_get_disable_menu_shortcuts,
        (MultiWinGetTabPos) roxterm_get_tab_pos,
        (MultiWinDeleteHandler) roxterm_delete_handler,
        (MultiTabGetShowCloseButton) roxterm_get_show_tab_close_button,
        (MultiTabGetNewTabAdjacent) roxterm_get_new_tab_adjacent,
        (MultiTabConnectMiscSignals) roxterm_connect_misc_signals
        );
}

gboolean roxterm_spawn_command_line(const gchar *command_line,
        const char *cwd, char **env, GError **error)
{
    char **env2 = NULL;
    int argc;
    char **argv = NULL;
    gboolean result;

    cwd = roxterm_check_cwd(cwd);
    result = g_shell_parse_argv(command_line, &argc, &argv, error);
    if (result)
    {
        result = g_spawn_async(cwd, argv, env, G_SPAWN_SEARCH_PATH,
                NULL, NULL, NULL, error);
    }
    if (env2)
        g_strfreev(env2);
    if (argv)
        g_strfreev(argv);
    return result;
}

void roxterm_spawn(ROXTermData *roxterm, const char *command,
    ROXTerm_SpawnType spawn_type)
{
    GError *error = NULL;
    GtkPositionType tab_pos;
    char *cwd;
    MultiWin *win = roxterm_get_win(roxterm);
    MultiTab *tab;

    tab_pos = roxterm_get_tab_pos(roxterm);
    switch (spawn_type)
    {
        case ROXTerm_SpawnNewWindow:
            roxterm->special_command = g_strdup(command);
            roxterm->no_respawn = TRUE;
            multi_win_new(multi_win_get_shortcut_scheme(win),
                    roxterm->zoom_index, roxterm, tab_pos,
                    roxterm->borderless,
                    multi_win_get_always_show_tabs(win),
                    options_lookup_int_with_default(roxterm->profile,
                            "show_add_tab_btn", 1));
            break;
        case ROXTerm_SpawnNewTab:
            roxterm->special_command = g_strdup(command);
            roxterm->no_respawn = TRUE;
            tab = multi_tab_new(win, roxterm);
            break;
        default:
            cwd = roxterm_get_cwd(roxterm);
            roxterm_spawn_command_line(command, cwd, roxterm->env, &error);
            if (error)
            {
                dlg_warning(roxterm_get_toplevel(roxterm),
                        _("Unable to spawn command %s: %s"),
                    command, error->message);
            }
            g_free(cwd);
            break;
    }
}

VteTerminal *roxterm_get_vte_terminal(ROXTermData *roxterm)
{
    return VTE_TERMINAL(roxterm->widget);
}

static const char *get_options_leafname(Options *opts)
{
    char *slash = strrchr(opts->name, G_DIR_SEPARATOR);

    return slash ? slash + 1 : opts->name;
}

const char *roxterm_get_profile_name(ROXTermData *roxterm)
{
    return get_options_leafname(roxterm->profile);
}

const char *roxterm_get_colour_scheme_name(ROXTermData *roxterm)
{
    return get_options_leafname(roxterm->colour_scheme);
}

const char *roxterm_get_shortcuts_scheme_name(ROXTermData *roxterm)
{
    return get_options_leafname(roxterm->profile);
}

void roxterm_get_nonfs_dimensions(ROXTermData *roxterm, int *cols, int *rows)
{
    /* FIXME: This gets current dimensions whether we're full-screen/maximised
     * or not, but this is for session management so presumably manager will
     * remember size anyway.
     */
    VteTerminal *vte = VTE_TERMINAL(roxterm->widget);

    *cols = vte_terminal_get_column_count(vte);
    *rows = vte_terminal_get_row_count(vte);
}

double roxterm_get_zoom_factor(ROXTermData *roxterm)
{
    return roxterm->current_zoom_factor;
}

char const * const *roxterm_get_actual_commandv(ROXTermData *roxterm)
{
    return (char const * const *) roxterm->actual_commandv;
}

typedef struct {
    const char *client_id;
    gboolean session_tag_open;
    MultiWin *win;
    MultiTab *tab;
    ROXTermData *roxterm;
    gboolean borderless;
    gboolean maximised;
    gboolean fullscreen;
    char *geom;
    int width, height;
    PangoFontDescription *fdesc;
    char *window_title;
    char *window_title_template;
    double zoom_factor;
    char *title;
    char *role;
    char **commandv;
    int argc;
    int argn;
    char *tab_name;
    char *tab_title_template;
    char *tab_title;
    gboolean win_title_template_locked;
    gboolean tab_title_template_locked;
    gboolean current;
    MultiTab *active_tab;
} _ROXTermParseContext;

static void parse_open_win(_ROXTermParseContext *rctx,
        const char **attribute_names, const char **attribute_values,
        GError **error)
{
    int n;
    const char *geom = NULL;
    const char *role = NULL;
    const char *font = NULL;
    const char *shortcuts_name = NULL;
    const char *title_template = NULL;
    const char *title = NULL;
    gboolean show_mbar = TRUE;
    gboolean show_tabs = FALSE;
    gboolean show_add_tab_btn = TRUE;
    gboolean disable_menu_shortcuts = FALSE;
    gboolean disable_tab_shortcuts = FALSE;
    GtkPositionType tab_pos = GTK_POS_TOP;
    MultiWin *win;
    GtkWindow *gwin;
    Options *shortcuts;

    rctx->fullscreen = FALSE;
    rctx->maximised = FALSE;
    rctx->borderless = FALSE;
    rctx->zoom_factor = 1.0;
    rctx->active_tab = NULL;
    rctx->win_title_template_locked = FALSE;
    for (n = 0; attribute_names[n]; ++n)
    {
        const char *a = attribute_names[n];
        const char *v = attribute_values[n];

        if (!strcmp(a, "geometry"))
        {
            rctx->geom = g_strdup(v);
            sscanf(v, "%dx%d+", &rctx->width, &rctx->height);
        }
        else if (!strcmp(a, "title_template"))
            title_template = v;
        else if (!strcmp(a, "font"))
            font = v;
        else if (!strcmp(a, "title"))
            title = v;
        else if (!strcmp(a, "role"))
            role = v;
        else if (!strcmp(a, "shortcut_scheme"))
            shortcuts_name = v;
        else if (!strcmp(a, "show_menubar"))
            show_mbar = (strcmp(v, "0") != 0);
        else if (!strcmp(a, "always_show_tabs"))
            show_tabs = strcmp(v, "0");
        else if (!strcmp(a, "tab_pos"))
            tab_pos = atoi(v);
        else if (!strcmp(a, "show_add_tab_btn"))
            show_add_tab_btn = strcmp(v, "0");
        else if (!strcmp(a, "disable_menu_shortcuts"))
            disable_menu_shortcuts = (strcmp(v, "0") != 0);
        else if (!strcmp(a, "disable_tab_shortcuts"))
            disable_tab_shortcuts = (strcmp(v, "0") != 0);
        else if (!strcmp(a, "maximised"))
            rctx->maximised = (strcmp(v, "0") != 0);
        else if (!strcmp(a, "fullscreen"))
            rctx->fullscreen = (strcmp(v, "0") != 0);
        else if (!strcmp(a, "borderless"))
            rctx->borderless = (strcmp(v, "0") != 0);
        else if (!strcmp(a, "zoom"))
            rctx->zoom_factor = atof(v);
        else if (!strcmp(a, "title_template_locked"))
            rctx->win_title_template_locked = atoi(v);
        else
        {
            *error = g_error_new(G_MARKUP_ERROR,
                    G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                    _("Unknown <window> attribute '%s'"), a);
            return;
        }
    }
    SLOG("Opening window with title %s", title);

    shortcuts = shortcuts_open(shortcuts_name, FALSE);
    rctx->win = win = multi_win_new_blank(shortcuts,
            multi_win_get_nearest_index_for_zoom(rctx->zoom_factor),
            disable_menu_shortcuts, disable_tab_shortcuts, tab_pos, show_tabs,
            show_add_tab_btn);
    shortcuts_unref(shortcuts);
    /* Set role and title before and after adding tabs because docs are quite
     * vague about how these are used for restoring the session.
     */
    gwin = GTK_WINDOW(multi_win_get_widget(rctx->win));
    if (role && role[0])
    {
        rctx->role = g_strdup(role);
        gtk_window_set_role(gwin, role);
    }
    if (title_template && title_template[0])
    {
        rctx->window_title_template = g_strdup(title_template);
        multi_win_set_title_template(win, title_template);
    }
    if (rctx->window_title)
    {
        multi_win_set_title(win, rctx->window_title);
    }
    if (geom && geom[0])
    {
        rctx->geom = g_strdup(geom);
    }
    if (font && font[0])
    {
        rctx->fdesc = pango_font_description_from_string(font);
        resize_pango_for_zoom(rctx->fdesc, rctx->zoom_factor);
    }
    multi_win_set_borderless(win, rctx->borderless);
    multi_win_set_show_menu_bar(win, show_mbar);
    multi_win_set_always_show_tabs(win, show_tabs);
    if (title && title[0])
        rctx->window_title = g_strdup(title);
}

static void roxterm_connect_each_tabs_signals(MultiTab *tab, void *handle)
{
    roxterm_connect_misc_signals(multi_tab_get_user_data(tab));
    (void) handle;
}

static void close_win_tag(_ROXTermParseContext *rctx)
{
    if (!multi_win_get_num_tabs(rctx->win))
    {
        g_warning(_("Session contains window with no valid tabs"));
        multi_win_delete(rctx->win);
    }
    else
    {
        GtkWindow *gwin = GTK_WINDOW(multi_win_get_widget(rctx->win));

        if (rctx->borderless)
        {
            multi_win_set_borderless(rctx->win, TRUE);
        }
        if (rctx->geom)
        {
            /* Need to show children before parsing geom */
            gtk_widget_realize(gtk_bin_get_child(GTK_BIN(gwin)));
            gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(gwin)));
            multi_win_set_initial_geometry(rctx->win, rctx->geom,
                    rctx->active_tab);
        }
        if (rctx->fullscreen)
        {
            multi_win_set_fullscreen(rctx->win, TRUE);
        }
        else if (rctx->maximised)
        {
            gtk_window_maximize(gwin);
        }
        if (rctx->window_title_template)
        {
            multi_win_set_title_template(rctx->win,
                    rctx->window_title_template);
        }
        if (rctx->role)
            gtk_window_set_role(gwin, rctx->role);
        if (rctx->window_title)
            multi_win_set_title(rctx->win, rctx->window_title);
        multi_win_set_title_template_locked(rctx->win,
                rctx->win_title_template_locked);
        multi_win_show(rctx->win);
        multi_win_foreach_tab(rctx->win, roxterm_connect_each_tabs_signals,
                NULL);
        if (rctx->active_tab)
        {
            multi_win_select_tab(rctx->win, rctx->active_tab);
            /*
            roxterm_tab_selection_handler(
                    multi_tab_get_user_data(rctx->active_tab),
                    rctx->active_tab);
            */
        }
    }
    rctx->win = NULL;
    g_free(rctx->role);
    rctx->role = NULL;
    g_free(rctx->window_title);
    rctx->window_title = NULL;
    g_free(rctx->window_title_template);
    rctx->window_title_template = NULL;
    if (rctx->fdesc)
    {
        pango_font_description_free(rctx->fdesc);
        rctx->fdesc = NULL;
    }
    g_free(rctx->geom);
    rctx->geom = NULL;
}

extern char **environ;

static void parse_open_tab(_ROXTermParseContext *rctx,
        const char **attribute_names, const char **attribute_values)
{
    const char *profile_name = "Default";
    const char *colours_name = "GTK";
    const char *cwd = NULL;
    int n;
    ROXTermData *roxterm;
    Options *profile;

    rctx->current = FALSE;
    rctx->tab_title_template_locked = FALSE;
    for (n = 0; attribute_names[n]; ++n)
    {
        const char *a = attribute_names[n];
        const char *v = attribute_values[n];

        if (!strcmp(a, "profile"))
            profile_name = v;
        else if (!strcmp(a, "colour_scheme"))
            colours_name = v;
        else if (!strcmp(a, "cwd"))
            cwd = v;
        else if (!strcmp(a, "title_template"))
            rctx->tab_title_template = g_strdup(v);
        else if (!strcmp(a, "window_title"))
            rctx->tab_title = g_strdup(v);
        else if (!strcmp(a, "current"))
            rctx->current = (strcmp(v, "0") != 0);
        else if (!strcmp(a, "title_template_locked"))
            rctx->tab_title_template_locked = atoi(v);
        /* Ignore unknown tags, probably caused by deprecated settings
        else
        {
            *error = g_error_new(G_MARKUP_ERROR,
                    G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                    _("Unknown <tab> attribute '%s'"), a);
            return;
        }
        */
    }

    profile = dynamic_options_lookup_and_ref(roxterm_get_profiles(),
            profile_name, "roxterm profile");
    roxterm = roxterm_data_new(rctx->zoom_factor, cwd,
            g_strdup(profile_name), profile,
            rctx->maximised, colours_name,
            &rctx->geom, NULL, environ);
    roxterm->from_session = TRUE;
    roxterm->dont_lookup_dimensions = TRUE;
    if (rctx->fdesc)
        roxterm->pango_desc = pango_font_description_copy(rctx->fdesc);
    rctx->roxterm = roxterm;
}

static void close_tab_tag(_ROXTermParseContext *rctx)
{
    ROXTermData *roxterm = rctx->roxterm;

    if (rctx->commandv)
    {
        if (rctx->commandv[0])
        {
            roxterm->commandv = rctx->commandv;
        }
        else
        {
            g_warning(_("<command> with no arguments"));
        }
    }
    rctx->commandv = NULL;
    rctx->tab = multi_tab_new(rctx->win, roxterm);
    if (rctx->current)
        rctx->active_tab = rctx->tab;
    if (rctx->tab_title_template && rctx->tab_title_template[0])
    {
        multi_tab_set_window_title_template(rctx->tab,
                rctx->tab_title_template);
    }
    if (rctx->tab_title && rctx->tab_title[0])
        multi_tab_set_window_title(rctx->tab, rctx->tab_title);
    multi_tab_set_title_template_locked(rctx->tab,
            rctx->tab_title_template_locked);
    g_free(rctx->tab_title_template);
    rctx->tab_title_template = NULL;
    g_free(rctx->tab_title);
    rctx->tab_title = NULL;
    roxterm_data_delete(roxterm);
    /*
    roxterm = multi_tab_get_user_data(rctx->tab);
    vte = VTE_TERMINAL(roxterm->widget);
    roxterm_set_vte_size(roxterm, vte, rctx->width, rctx->height);
    */
}

static void parse_open_command(_ROXTermParseContext *rctx,
        const char **attribute_names, const char **attribute_values,
        GError **error)
{
    int n;

    rctx->argn = 0;
    for (n = 0; attribute_names[n]; ++n)
    {
        const char *a = attribute_names[n];
        const char *v = attribute_values[n];

        if (!strcmp(a, "argc"))
            rctx->argc = atoi(v);
        else
        {
            *error = g_error_new(G_MARKUP_ERROR,
                    G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                    _("Unknown <command> attribute '%s'"), a);
            return;
        }
    }
    if (rctx->argc)
    {
        rctx->commandv = g_new0(char *, rctx->argc + 1);
    }
    else
    {
        *error = g_error_new(G_MARKUP_ERROR,
                G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                _("<command> missing argc attribute"));
    }
}

static void parse_open_arg(_ROXTermParseContext *rctx,
        const char **attribute_names, const char **attribute_values,
        GError **error)
{
    int n;
    const char *arg = NULL;

    if (rctx->argn >= rctx->argc)
    {
        *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                _("Too many <arg> elements for <command argc='%d>"),
                rctx->argc);
        return;
    }
    for (n = 0; attribute_names[n]; ++n)
    {
        const char *a = attribute_names[n];
        const char *v = attribute_values[n];

        if (!strcmp(a, "s"))
        {
            arg = v;
        }
        else
        {
            *error = g_error_new(G_MARKUP_ERROR,
                    G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                    _("Unknown <arg> attribute '%s'"), a);
            return;
        }
    }
    if (arg)
    {
        rctx->commandv[(rctx->argn)++] = g_strdup(arg);
    }
    else
    {
        *error = g_error_new(G_MARKUP_ERROR,
                G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                _("<arg> missing s attribute"));
    }
}

static void parse_roxterm_session_tag(_ROXTermParseContext *rctx,
        const char **attribute_names, const char **attribute_values,
        GError **error)
{
    int n;
    (void) rctx;
    (void) attribute_values;

    for (n = 0; attribute_names[n]; ++n)
    {
        const char *a = attribute_names[n];

        if (strcmp(a, "id"))
        {
            *error = g_error_new(G_MARKUP_ERROR,
                    G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                    _("Unknown <roxterm_session> attribute '%s'"), a);
            return;
        }
    }
}

static void parse_start_element(GMarkupParseContext *context,
        const char *element_name,
        const char **attribute_names, const char **attribute_values,
        gpointer handle, GError **error)
{
    _ROXTermParseContext *rctx = handle;
    (void) context;

    if (!strcmp(element_name, "roxterm_session"))
    {
        if (rctx->win)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("<roxterm_session> tag nested within <window>"));
        }
        else
        {
            rctx->session_tag_open = TRUE;
            parse_roxterm_session_tag(rctx,
                    attribute_names, attribute_values, error);
        }
    }
    else if (!strcmp(element_name, "window"))
    {
        if (!rctx->session_tag_open)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("<window> tag not within <roxterm_session>"));
        }
        else if (rctx->win)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("<window> tag nested within another <window>"));
        }
        else
        {
            parse_open_win(rctx, attribute_names, attribute_values, error);
        }
    }
    else if (!strcmp(element_name, "tab"))
    {
        if (!rctx->win)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("<tab> tag not within <window>"));
        }
        else if (rctx->roxterm)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("<tab> tag nested within another <tab>"));
        }
        else
        {
            parse_open_tab(rctx, attribute_names, attribute_values);
        }
    }
    else if (!strcmp(element_name, "command"))
    {
        if (!rctx->roxterm)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("<command> tag not within <tab>"));
        }
        else if (rctx->commandv)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("<command> tag nested within another <command>"));
        }
        else
        {
            parse_open_command(rctx, attribute_names, attribute_values, error);
        }
    }
    else if (!strcmp(element_name, "arg"))
    {
        if (!rctx->commandv)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("<arg> tag not within <command>"));
        }
        else
        {
            parse_open_arg(rctx, attribute_names, attribute_values, error);
        }
    }
    else
    {
        *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                _("Unknown element <%s>"), element_name);
    }
}

static void parse_end_element(GMarkupParseContext *context,
        const char *element_name,
        gpointer handle, GError **error)
{
    _ROXTermParseContext *rctx = handle;
    (void) context;

    if (!strcmp(element_name, "roxterm_session"))
    {
        if (!rctx->session_tag_open)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("Unmatched </roxterm_session>"));
        }
        else if (rctx->win)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("Attempt to close <roxterm_session> without "
                    "closing <window>"));
        }
        else
        {
            rctx->session_tag_open = FALSE;
        }
    }
    else if (!strcmp(element_name, "window"))
    {
        if (!rctx->win)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("Unmatched </window>"));
        }
        else if (rctx->roxterm)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("Attempt to close <window> without closing <tab>"));
        }
        else
        {
            close_win_tag(rctx);
        }
    }
    else if (!strcmp(element_name, "tab"))
    {
        if (!rctx->roxterm)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("Unmatched </tab>"));
        }
        else
        {
            close_tab_tag(rctx);
            rctx->tab = NULL;
            rctx->roxterm = NULL;
        }
    }
    else if (!strcmp(element_name, "command"))
    {
        if (!rctx->commandv)
        {
            *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                    _("Unmatched </command>"));
        }
    }
    else if (strcmp(element_name, "arg"))
    {
        *error = g_error_new(G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                _("Unknown closing element </%s>"), element_name);
    }
}

static void parse_error(GMarkupParseContext *context, GError *error,
        gpointer handle)
{
    _ROXTermParseContext *rctx = handle;
    (void) context;

    g_warning(_("Unable to parse data for session '%s': %s"),
            rctx->client_id, error->message);
}

gboolean roxterm_load_session(const char *xml, gssize len,
        const char *client_id)
{
    GMarkupParser pvtbl = { parse_start_element, parse_end_element,
            NULL, NULL, parse_error };
    _ROXTermParseContext *rctx = g_new0(_ROXTermParseContext, 1);
    GMarkupParseContext *pctx = g_markup_parse_context_new(&pvtbl,
            G_MARKUP_PREFIX_ERROR_POSITION, rctx, NULL);
    gboolean result;
    GError *error = NULL;

    rctx->client_id = client_id;
    result = g_markup_parse_context_parse(pctx, xml, len, &error);
    if (!error)
        result = g_markup_parse_context_end_parse(pctx, &error) & result;
    if (error)
    {
        g_error_free(error);
        error = NULL;
    }
    SLOG("Session loaded, result %d", result);
    return result;
}

MultiWin *roxterm_get_multi_win(ROXTermData *roxterm)
{
    return roxterm_get_win(roxterm);
}

VteTerminal *roxterm_get_vte(ROXTermData *roxterm)
{
    return VTE_TERMINAL(roxterm->widget);
}

gboolean roxterm_set_search(ROXTermData *roxterm,
        const char *pattern, guint flags, GError **error)
{
    VteRegex *regex = NULL;
    char *cooked_pattern = NULL;

    roxterm->search_flags = flags;

    if (roxterm->search_pattern)
    {
        g_free(roxterm->search_pattern);
        roxterm->search_pattern = NULL;
    }

    if (pattern && pattern[0])
    {
        roxterm->search_pattern = g_strdup(pattern);
        if (!(flags & ROXTERM_SEARCH_AS_REGEX))
        {
            cooked_pattern = g_regex_escape_string(pattern, -1);
            pattern = cooked_pattern;
        }
        if (flags & ROXTERM_SEARCH_ENTIRE_WORD)
        {
            char *tmp = cooked_pattern;

            cooked_pattern = g_strdup_printf("\\<%s\\>", pattern);
            pattern = cooked_pattern;
            g_free(tmp);
        }

        regex = vte_regex_new_for_search(pattern, -1,
                ((flags & ROXTERM_SEARCH_MATCH_CASE) ? 0 : PCRE2_CASELESS) |
                PCRE2_NOTEMPTY,
                error);
        g_free(cooked_pattern);
        if (!regex)
            return FALSE;
    }

    vte_terminal_search_set_regex(VTE_TERMINAL(roxterm->widget), regex, 0);
    vte_terminal_search_set_wrap_around(VTE_TERMINAL(roxterm->widget),
            flags & ROXTERM_SEARCH_WRAP);
    roxterm_shade_search_menu_items(roxterm);
    return TRUE;
}

const char *roxterm_get_search_pattern(ROXTermData *roxterm)
{
    return roxterm->search_pattern;
}

guint roxterm_get_search_flags(ROXTermData *roxterm)
{
    return roxterm->search_flags;
}

/* vi:set sw=4 ts=4 et cindent cino= */
