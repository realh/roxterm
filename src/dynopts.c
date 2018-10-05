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


#include "dynopts.h"
#include "optsfile.h"

#include <string.h>

struct DynamicOptions {
    char *family;
    GHashTable *profiles;
};


DynamicOptions *dynamic_options_get(const char *family)
{
    static GHashTable *all_dynopts = NULL;
    DynamicOptions *dynopts;

    if (!all_dynopts)
        all_dynopts = g_hash_table_new(g_str_hash, g_str_equal);

    dynopts = g_hash_table_lookup(all_dynopts, family);
    if (!dynopts)
    {
        dynopts = g_new(DynamicOptions, 1);
        dynopts->family = g_strdup(family);
        dynopts->profiles = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_insert(all_dynopts, (gpointer) family, dynopts);
    }
    return dynopts;
}

Options *dynamic_options_lookup(DynamicOptions * dynopts,
    const char *profile_name)
{
    return g_hash_table_lookup(dynopts->profiles, profile_name);
}

Options *dynamic_options_lookup_and_ref(DynamicOptions * dynopts,
    const char *profile_name, const char *group_name)
{
    Options *options = g_hash_table_lookup(dynopts->profiles, profile_name);

    if (!options)
    {
        char *leafname = g_build_filename(dynopts->family, profile_name,
            NULL);

        options = options_open(leafname, group_name);
        g_hash_table_insert(dynopts->profiles, g_strdup(profile_name), options);
    }
    else
    {
        options_ref(options);
    }
    return options;
}

void
dynamic_options_forget(DynamicOptions *dynopts, const char *profile_name)
{
    g_hash_table_remove(dynopts->profiles, profile_name);
}

gboolean
dynamic_options_unref(DynamicOptions * dynopts, const char *profile_name)
{
    /* Use generic pointers for these to avoid breaking strict aliasing (see
     * man gcc) */
    gpointer options;
    gpointer key;
    gboolean lookup_ok = g_hash_table_lookup_extended(dynopts->profiles,
        profile_name, &key, &options);

    if (!lookup_ok)
        return FALSE;

    /* Have to check ref and remove from hash first in case options_unref
     * frees profile_name */
    if (((Options *) options)->ref == 1)
    {
        g_hash_table_remove(dynopts->profiles, profile_name);
        g_free(key);
    }
    return options_unref(options);
}

static GList *dynopts_add_path_contents_to_list(GList *list, const char *path,
                const char *family, gboolean sorted)
{
    GError *err = NULL;
    char *dirname = g_build_filename(path, family, NULL);

    if (g_file_test(dirname, G_FILE_TEST_IS_DIR))
    {
        GDir *dir = g_dir_open(dirname, 0, &err);

        if (!dir || err)
        {
            g_warning("%s", err->message);
            g_error_free(err);
        }
        else
        {
            const char *filename;

            while ((filename = g_dir_read_name(dir)) != NULL)
            {
                GList *link;

                for (link = list; link; link = g_list_next(link))
                {
                    if (!strcmp(link->data, filename))
                        break;
                }
                if (!link)
                {
                    char *pathname = g_build_filename(dirname, filename, NULL);

                    if (!g_file_test(pathname, G_FILE_TEST_IS_DIR))
                        list = g_list_append(list, g_strdup(filename));
                    g_free(pathname);
                }
                /* else duplicate */
            }
            g_dir_close(dir);
        }
    }
    g_free(dirname);
    if (sorted)
    {
        list = g_list_sort(list, (GCompareFunc) dynamic_options_strcmp);
    }
    return list;
}

char **dynamic_options_list_full(DynamicOptions *dynopts, gboolean sorted)
{
    int i;
    const char * const *paths = options_file_get_pathv();
    char **strv;
    guint nmemb;
    guint n;
    GList *list = g_list_append(NULL, g_strdup("Default"));
    GList *link;

    for (i = 0; paths[i]; ++i)
    {
        list = dynopts_add_path_contents_to_list(list, paths[i],
                dynopts->family, sorted);
    }

    nmemb = g_list_length(list);
    if (!nmemb)
        return NULL;
    strv = g_new(char *, nmemb + 1);
    for (link = list, n = 0; link && n < nmemb; link = g_list_next(link), ++n)
        strv[n] = link->data;
    strv[n] = NULL;
    g_list_free(list);
    
    return strv;
}

void dynamic_options_rename(DynamicOptions *dynopts,
        const char *old_name, const char *new_name)
{
    Options *opts = dynamic_options_lookup(dynopts, old_name);

    g_return_if_fail(opts);
    dynamic_options_forget(dynopts, old_name);
    g_hash_table_insert(dynopts->profiles, g_strdup(new_name), opts);
}

int dynamic_options_strcmp(const char *s1, const char *s2)
{
    char *u1, *u2;
    int result;
    
    if (!g_strcmp0(s1, "Default"))
        return g_strcmp0(s2, "Default") ? -1 : 0;
    else if (!g_strcmp0(s2, "Default"))
        return 1;
    u1 = s1 ? g_utf8_casefold(s1, -1) : g_strdup("");
    u2 = s2 ? g_utf8_casefold(s2, -1) : g_strdup("");
    result = g_utf8_collate(u1, u2);
    g_free(u2);
    g_free(u1);
    return result;
}

/* vi:set sw=4 ts=4 noet cindent cino= */
