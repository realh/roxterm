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

#include <stdio.h>
#include <string.h>

#include "dlg.h"
#include "dynopts.h"
#include "optsfile.h"
#include "shortcuts.h"

#define SHORTCUTS_GROUP "roxterm shortcuts scheme"

#define SHORTCUTS_SUBDIR "Shortcuts"

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

inline static char *make_full_path(const char *index_str, const char *path_leaf)
{
    return g_strjoin("/", ACCEL_PATH, index_str, path_leaf, NULL);
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

void
shortcuts_changed_handler(GtkAccelMap * map, const char *accel_path,
    guint accel_key, GdkModifierType accel_mods, gpointer user_data)
{
    const char *path_without_root = accel_path + sizeof(ACCEL_PATH);
    char *index_str = strchr(path_without_root, '/');
    const char *path_leaf = accel_path;
    Options *shortcuts;
    char *accel_key_name = NULL;
    guint index = G_MAXUINT;

    accel_key_name = gtk_accelerator_name(accel_key, accel_mods);

    if (index_str)
    {
        path_leaf = index_str + 1;
        index_str = g_strndup(path_without_root, index_str -
            path_without_root);
    }
    else
    {
        path_leaf = NULL;
        index_str = g_strdup(path_without_root);
    }
    if (sscanf(index_str, "%x", &index) != 1 || index >= shortcuts_counter ||
        (shortcuts = shortcuts_indexed_names[index]) == NULL)
    {
        g_critical("Keyboard shortcut changed with bad index string %s",
                index_str);
    }
    else if (!shortcuts->deleted)
    {
        ShortcutsData *data = options_get_data(shortcuts);

        options_set_string(shortcuts, path_leaf, accel_key_name);
        shortcuts_change_item(data->items, path_leaf, accel_key, accel_mods);
        options_file_save(shortcuts->kf, shortcuts->name);
    }

    g_free(index_str);
    g_free(accel_key_name);

    (void) map;
    (void) user_data;
}

void shortcuts_init(void)
{
    if (!shortcuts_dynopts)
    {
        shortcuts_dynopts = dynamic_options_get(SHORTCUTS_SUBDIR);
        shortcuts_enable_signal_handler(TRUE);
    }
}

void shortcuts_enable_signal_handler(gboolean enable)
{
    static gulong handler_id = 0;

    if (enable)
    {
        if (!handler_id)
        {
            handler_id = g_signal_connect(gtk_accel_map_get(), "changed",
                G_CALLBACK(shortcuts_changed_handler), NULL);
        }
        else
        {
            g_warning("Shortcuts signal handler already connected");
        }
    }
    else
    {
        if (handler_id)
        {
            g_signal_handler_disconnect(gtk_accel_map_get(), handler_id);
            handler_id = 0;
        }
        else
        {
            g_warning("Shortcuts signal handler already disconnected");
        }
    }
}

const char *shortcuts_get_index_str(Options *shortcuts)
{
    ShortcutsData *data = options_get_data(shortcuts);

    return data ? data->index_str : NULL;
}

/* vi:set sw=4 ts=4 noet cindent cino= */
