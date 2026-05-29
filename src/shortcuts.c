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


#include "config.h"
#include "defns.h"

#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "dlg.h"
#include "dynopts.h"
#include "glib.h"
#include "menu-labels.h"
#include "optsdbus.h"
#include "optsfile.h"
#include "shortcuts.h"

#define SHORTCUTS_GROUP "roxterm shortcuts scheme"

#define SHORTCUTS_SUBDIR "Shortcuts"

char *shortcuts_strip_underscores(const char *in)
{
    char *out = g_new(char, strlen(in) + 1);
    int n, m;

    for (n = 0, m = 0; in[n]; ++n)
    {
        if (in[n] != '_')
            out[m++] = in[n];
    }
    out[m] = 0;
    return out;
}

#ifndef ROXTERM_CAPPLET

#include "roxterm.h"

typedef struct {
    guint key;
    GdkModifierType modifiers;
    char *path;     /* leaf */
} ShortcutsItem;

typedef struct {
    char *index_str;
    GArray *items;
} ShortcutsData;

static DynamicOptions *shortcuts_dynopts = NULL;

static guint32 shortcuts_counter = 0;

static Options **shortcuts_indexed_names = NULL;

static guint32 shortcuts_index_size = 0;

static char *make_full_path(const char *index_str, const char *path_leaf)
{
    char *s = g_strjoin("/", ACCEL_PATH, index_str, path_leaf, NULL);
    size_t l = strlen(s);
    if (l >= 4 && !strcmp(s + l - 3, "..."))
    {
        s[l - 3] = 0;
    }
    // else if (g_str_has_suffix(path_leaf, "..."))
    // {
    //     g_critical("GAP: Missed stripping ... from '%s'", s);
    // }
    // else if (strstr(s, "Find"))
    // {
    //     g_debug("GAP: Not stripping ... from '%s'", s);
    // }
    return s;
}

static char *full_path_for_tab(const char *index_str, int tab)
{
    char *leaf = g_strdup_printf("Tabs/Select_Tab_%d", tab);
    char *path = make_full_path(index_str, leaf);

    g_free(leaf);
    return path;
}

/* Set up defaults of Alt+1 - Alt+9, Alt+0 for selecting first 10 tabs, but
 * only if user hasn't configured something else for each one */
static void shortcuts_check_change_tabs(Options *shortcuts,
        const char *index_str)
{
    int n;

    for (n = 0; n < 10; ++n)
    {
        char *path = full_path_for_tab(index_str, n);
        char *s = options_lookup_string(shortcuts, path);

        if (!s)
        {
            guint a_key;
            GdkModifierType a_mods;
            char *accel = g_strdup_printf("<Alt>%d", n < 9 ? n + 1 : 0);

            gtk_accelerator_parse(accel, &a_key, &a_mods);
            gtk_accel_map_add_entry(path, a_key, a_mods);
            g_free(accel);
        }
        g_free(s);
        g_free(path);
    }
}

static void shortcuts_change_item(GArray *array, const char *path,
        guint key, GdkModifierType modifiers)
{
    guint n;

    for (n = 0; n < array->len; ++n)
    {
        ShortcutsItem *item = &g_array_index(array, ShortcutsItem, n);

        if (!strcmp(item->path, path))
        {
            item->key = key;
            item->modifiers = modifiers;
            return;
        }
    }
}

gboolean shortcuts_key_is_shortcut(Options *shortcuts,
        guint key, GdkModifierType modifiers)
{
    ShortcutsData *data = options_get_data(shortcuts);
    guint n;

    for (n = 0; n < data->items->len; ++n)
    {
        ShortcutsItem *item = &g_array_index(data->items, ShortcutsItem, n);

        if (item->key == key && item->modifiers == modifiers)
            return TRUE;
    }
    return FALSE;
}

Options *shortcuts_open(const char *scheme, gboolean reload)
{
    Options *shortcuts;
    ShortcutsData *data;

    shortcuts_init();

    /* Check if it's already been loaded */
    shortcuts = dynamic_options_lookup(shortcuts_dynopts, scheme);
    if (shortcuts)
    {
        options_ref(shortcuts);
        if (reload)
        {
            options_reload_keyfile(shortcuts);
        }
        else
        {
            return shortcuts;
        }
    }
    else
    {
        shortcuts = dynamic_options_lookup_and_ref(shortcuts_dynopts, scheme,
                SHORTCUTS_GROUP);
        reload = FALSE;
    }

    shortcuts_enable_signal_handler(FALSE);

    if (reload)
    {
        data = options_get_data(shortcuts);
    }
    else
    {
        if (!shortcuts_index_size)
        {
            shortcuts_indexed_names = g_new(Options *,
                    shortcuts_index_size = 4);
        }
        else if (shortcuts_counter + 1 >= shortcuts_index_size)
        {
            shortcuts_indexed_names = g_renew(Options *,
                    shortcuts_indexed_names, shortcuts_index_size *= 2);
        }
        data = g_new(ShortcutsData, 1);
        data->index_str = g_strdup_printf("%08x", shortcuts_counter);
        data->items = g_array_new(FALSE, FALSE, sizeof(ShortcutsItem));
        options_associate_data(shortcuts, data);
        shortcuts_indexed_names[shortcuts_counter] = shortcuts;
        ++shortcuts_counter;
    }

    if (shortcuts->kf)
    {
        GError *err = NULL;
        char **all_keys = g_key_file_get_keys(shortcuts->kf, SHORTCUTS_GROUP,
                NULL, &err);
        char **pkey;

        if (!all_keys || err)
        {
            dlg_critical(NULL,
                    _("Unable to read keys from shortcuts file %s: %s"),
                    shortcuts->name,
                    (err && !STR_EMPTY(err->message)) ? err->message :
                    _("unknown reason"));
            if (all_keys)
                g_strfreev(all_keys);
            if (err)
                g_error_free(err);
            return shortcuts;
        }

        for (pkey = all_keys; *pkey; ++pkey)
        {
            char *path = *pkey;
            char *accel = options_lookup_string(shortcuts, path);
            char *full_path;
            ShortcutsItem item;

            if (!accel)
            {
                /* Not an error, user may have deleted shortcut */
                continue;
            }
            gtk_accelerator_parse(accel, &item.key, &item.modifiers);
            if (item.key)
            {
                full_path = make_full_path(data->index_str, path);
                if (gtk_accel_map_lookup_entry(full_path, NULL))
                {
                    gtk_accel_map_change_entry(full_path,
                            item.key, item.modifiers, TRUE);
                    shortcuts_change_item(data->items, path,
                            item.key, item.modifiers);
                }
                else
                {
                    item.path = g_strdup(path);
                    g_array_append_val(data->items, item);
                    gtk_accel_map_add_entry(full_path,
                            item.key, item.modifiers);
                }
                g_free(full_path);
            }
            else
            {
                dlg_warning(NULL, _("Shortcut '%s' in '%s' has invalid value"),
                        path, shortcuts->name);
            }
            g_free(accel);
        }
        g_strfreev(all_keys);
    }
    shortcuts_check_change_tabs(shortcuts, data->index_str);
    shortcuts_enable_signal_handler(TRUE);
    return shortcuts;
}

void shortcuts_unref(Options *shortcuts)
{
    ShortcutsData *data = options_get_data(shortcuts);
    gboolean ref0;
    guint index = G_MAXUINT;

    if (sscanf(data->index_str, "%x", &index) != 1 ||
            index >= shortcuts_counter)
    {
        g_critical("Unrefing shortcuts group with bad index string %s",
                data->index_str);
        index = G_MAXUINT;
    }
    if (shortcuts->deleted)
    {
        ref0 = options_unref(shortcuts);
    }
    else
    {
        ref0 = dynamic_options_unref(shortcuts_dynopts,
                options_get_leafname(shortcuts));
    }
    if (ref0)
    {
        guint n;

        for (n = 0; n < data->items->len; ++n)
        {
            g_free(g_array_index(data->items, ShortcutsItem, n).path);
        }
        g_free(data->index_str);
        g_array_free(data->items, TRUE);
        g_free(data);
        if (index != G_MAXUINT)
            shortcuts_indexed_names[index] = NULL;
    }
}

void shortcuts_init(void)
{
    if (!shortcuts_dynopts)
    {
        shortcuts_dynopts = dynamic_options_get(SHORTCUTS_SUBDIR);
        shortcuts_enable_signal_handler(TRUE);
    }
}

const char *shortcuts_get_index_str(Options *shortcuts)
{
    ShortcutsData *data = options_get_data(shortcuts);

    return data ? data->index_str : NULL;
}

#endif /* ROXTERM_CAPPLET */

static const char *shortcuts_find_text_editor(void)
{
    static char *editor = NULL;
    static char const *subs[] = {"vi", "emacs", "gedit", "kate", NULL};
    static char const *editors[] = {"neovide", "gedit", "kate", "gvim",
        "emacs", NULL};
    char *env;
    int n;

    if (editor)
        return editor;
    env = getenv("EDITOR");
    if (env)
    {
        for (n = 0; subs[n] && !editor; ++n)
        {
            if (strstr(env, subs[n]))
            {
                editor = g_find_program_in_path(
                        strcmp(subs[n], "vi") ? subs[n] : "gvim");
            }
        }
    }
    if (!editor)
    {
        for (n = 0; editors[n] && !editor; ++n)
            editor = g_find_program_in_path(editors[n]);
    }
    return editor;
}

typedef struct {
    GFileMonitor *monitor;
    GFile *file;
    char *name;
} ShortcutsMonitorDetails_;

static gboolean remove_shortcuts_monitor(ShortcutsMonitorDetails_ *md)
{
    g_object_unref(md->monitor);
    /* Trying to unref file gives an error, is it destroyed with monitor? */
    //g_object_unref(md->file);
    g_free(md->name);
    g_free(md);
    return FALSE;
}

static void shortcuts_file_modified(GFileMonitor *monitor,
        GFile *file, GFile *other_file, GFileMonitorEvent event_type,
        char *name)
{
    (void) monitor;
    (void) other_file;

    /* FIXME: Send dbus signal */
    g_debug("Shortcuts '%s' were modified, event %x", name, event_type);
    if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    {
#ifdef ROXTERM_CAPPLET
        optsdbus_send_stuff_changed_signal(OPTSDBUS_CHANGED,
                "Shortcuts", name, NULL);
#else
        roxterm_stuff_changed_handler(OPTSDBUS_CHANGED,
                "Shortcuts", name, NULL);
#endif
        g_file_monitor_cancel(monitor);
        g_signal_handlers_disconnect_by_func(monitor, shortcuts_file_modified,
                name);
        ShortcutsMonitorDetails_ *md = g_new(ShortcutsMonitorDetails_, 1);
        md->monitor = monitor;
        md->file = file;
        md->name = name;
        g_idle_add((GSourceFunc) remove_shortcuts_monitor, md);
    }
}

static MenuTreeID shortcuts_items_without_accel[] = {
    MENUTREE_FILE_NEW_WINDOW_WITH_PROFILE,
    MENUTREE_FILE_NEW_TAB_WITH_PROFILE,
    MENUTREE_PREFERENCES_SELECT_PROFILE,
    MENUTREE_PREFERENCES_SELECT_COLOUR_SCHEME,
    MENUTREE_PREFERENCES_SELECT_SHORTCUTS,
    MENUTREE_NULL_ID,
};

// Makes an array of strings from one of the macros in menu-labels.h. The
// macros include menutree ids so they act as the single source of truth; here
// we only want the strings, but the ids are useful for filtering.
static char const **build_label_list(MenuTreeID ignored, ...)
{
    char const **vec = NULL;
    int vec_cap = 0;
    int i = 0;
    va_list ap;
    va_start(ap, ignored);
    const char *label = NULL;;
    do
    {
        label = va_arg(ap, char *);
        if (label)
        {
            ignored = va_arg(ap, MenuTreeID);
            for (int j = 0;
                    shortcuts_items_without_accel[j] != MENUTREE_NULL_ID; ++j)
            {
                if (shortcuts_items_without_accel[j] == ignored)
                {
                    ignored = MENUTREE_NULL_ID;
                    break;
                }
            }
            if (ignored == MENUTREE_NULL_ID)
            {
                continue;
            }
        }
        else if (i == 0)
        {
            return vec;
        }
        if (i >= vec_cap)
        {
            if (vec_cap)
            {
                vec_cap *= 2;
                vec = g_realloc_n(vec, vec_cap, sizeof(char const *));
            }
            else
            {
                vec_cap = 4;
                vec = g_new(char const *, vec_cap);
            }
        }
        vec[i++] = label;
    }
    while (label != NULL);
    return vec;
}

static char *shortcuts_get_locale()
{
    // 1. Check LANGUAGE first (highest priority for gettext)
    char *lang = getenv("LANGUAGE");

    if (lang && strlen(lang) > 0) {
        // GNU LANGUAGE can be a colon-separated list (e.g., "hu:de:en")
        // We take the first preference
        char *colon = strchr(lang, ':');
        if (colon)
        {
            lang = g_strndup(lang, colon - lang);
        }
        else
        {
            lang = g_strdup(lang);
        }
    } else {
        // 2. Fall back to standard setlocale if LANGUAGE isn't set
        lang = setlocale(LC_MESSAGES, NULL);
        if (lang) {
            lang = g_strdup(lang);
        }
    }

    // 3. Strip any encoding suffixes (.UTF-8 or @modifiers)
    char *suffix = strpbrk(lang, ".@");
    if (suffix) {
        *suffix = '\0'; 
    }
    return lang;
}

// Loads existing `name` shortcuts file if one exists and builds the filename
// for the user-writabe version. It writes all possible options to the writable
// one, commenting out the ones which weren't set in the previous version of
// the file. If the current locale isn't English, each option is accompanied
// by its translation. Returns the pathname of the new file.
// See https://github.com/realh/roxterm/pull/284#issuecomment-4470503786
static char *make_editable_shortcuts_file(const char *name)
{
    char *twig_name = g_strdup_printf("%s/%s", SHORTCUTS_SUBDIR, name);
    GKeyFile *existing_kf = options_file_open(twig_name, SHORTCUTS_GROUP);
    g_free(twig_name);

    // We're going to replace these strings with ones that need to be freed
    char **top_labels = (char **) build_label_list(0,
            TOP_LEVEL_MENU_ITEMS, MENUTREE_URI_LABEL, MENUTREE_NUM_IDS, NULL);

    char *lang = shortcuts_get_locale();
    char **trans_top_labels = NULL;
    gboolean translate = lang && !(lang[0] == 'C' && !lang[1]) &&
        strcmp(lang, "POSIX") && !g_str_has_prefix(lang, "en");
    g_debug("lang %s, translate %d", lang, translate);
    if (translate)
    {
        int l = 0;
        for (; top_labels[l]; ++l);
        trans_top_labels = g_new(char *, l + 1);
        for (int i = 0; i < l; ++i)
        {
            trans_top_labels[i] = shortcuts_strip_underscores(
                    dgettext(PACKAGE, top_labels[i]));
        }
        trans_top_labels[l] = NULL;
    }
    for (int i = 0; top_labels[i]; ++i)
    {
        top_labels[i] = shortcuts_strip_underscores(top_labels[i]);
    }

    char *filename = options_file_filename_for_saving("Shortcuts", name, NULL);
    char *dirname = g_path_get_dirname(filename);
    g_mkdir_with_parents(dirname, 0755);
    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        g_critical("Unable to open '%s' for writing: %s",
                filename, strerror(errno));
        goto exit_make_shortcuts_file;
    }
    fputs("[" SHORTCUTS_GROUP "]\n", fp);

    for (int i = 0; i < 8; ++i)
    {
        char const **item_labels;
        switch (i)
        {
            case 0:
                item_labels = build_label_list(0, FILE_MENU_ITEMS, NULL);
                break;
            case 1:
                item_labels = build_label_list(0, EDIT_MENU_ITEMS, NULL);
                break;
            case 2:
                item_labels = build_label_list(0, VIEW_MENU_ITEMS, NULL);
                break;
            case 3:
                item_labels = build_label_list(0, SEARCH_MENU_ITEMS, NULL);
                break;
            case 4:
                item_labels = build_label_list(0, PREFERENCES_MENU_ITEMS, NULL);
                break;
            case 5:
                item_labels = build_label_list(0, TABS_MENU_ITEMS, NULL);
                break;
            case 6:
                item_labels = build_label_list(0, TABS_MENU_ITEMS, NULL);
                break;
            case 7:
                item_labels = build_label_list(0, URI_MENU_ITEMS, NULL);
                break;
        }
        for (int j = 0; item_labels[j]; ++j)
        {
            fputc('\n', fp);
            char *path = g_strjoin("/", top_labels[i],
                    shortcuts_strip_underscores(item_labels[j]), NULL);
            if (translate)
            {
                char *tpath = g_strjoin("/", trans_top_labels[i],
                        shortcuts_strip_underscores(
                            dgettext(PACKAGE, item_labels[j])), NULL);
                if (strcmp(path, tpath))
                {
                    fprintf(fp, "# # [%s] %s\n", lang, tpath);
                }
                g_free(tpath);
            }
            char *accel = g_key_file_get_string(existing_kf,
                    SHORTCUTS_GROUP, path, NULL);
            if (accel)
            {
                fprintf(fp, "%s=%s\n", path, accel);
                g_free(accel);
            }
            else
            {
                fprintf(fp, "# %s=\n", path);
            }
            g_free(path);
        }
        g_free(item_labels);
    }

exit_make_shortcuts_file:
    if (fp)
    {
        fclose(fp);
    }
    if (top_labels)
    {
        g_strfreev(top_labels);
    }
    if (trans_top_labels)
    {
        g_strfreev(trans_top_labels);
    }
    if (existing_kf)
    {
        g_key_file_unref(existing_kf);
    }
    return filename;
}

void shortcuts_edit(GtkWindow *window, const char *name)
{
    char *filename;
    const char *editor = shortcuts_find_text_editor();
    char const *cmdv[3];
    GError *error = NULL;
    GPid pid;

    if (!editor)
    {
        dlg_critical(window,
                _("Unable to find a text editor. Please install "
                "gedit, gvim, kate or emacs."));
        return;
    }
    filename = make_editable_shortcuts_file(name);
    if (!filename)
        return;
    cmdv[0] = editor;
    cmdv[1] = filename;
    cmdv[2] = NULL;
    if (g_spawn_async(NULL, (char **) cmdv, NULL, 0, NULL, NULL, &pid, &error))
    {
        GFile *f = g_file_new_for_path(filename);
        GFileMonitor *monitor = g_file_monitor_file(f, G_FILE_MONITOR_NONE,
                NULL, &error);
        if (monitor)
        {
            g_signal_connect(monitor, "changed",
                    G_CALLBACK(shortcuts_file_modified), g_strdup(name));
        }
    }
    if (error)
    {
        dlg_critical(window,
                _("Error trying to edit or monitor Shortcuts file '%s': %s"),
                        filename, error->message);
        g_error_free(error);
    }
    g_free(filename);
}

/* vi:set sw=4 ts=4 et cindent cino= */
