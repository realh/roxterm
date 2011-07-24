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

#include <string.h>

#include "dynopts.h"
#include "encodings.h"
#include "optsfile.h"

static const char *encodings_group_name = "roxterm encodings";

static const char *encodings_get_key(int n)
{
    static char key[16];

    sprintf(key, "e%d", n);
    return key;
}

static GPtrArray *encodings_build_default(void)
{
    GPtrArray *enc = g_ptr_array_sized_new(2);

    g_ptr_array_add(enc, (char *) "ISO-8859-1");
    g_ptr_array_add(enc, (char *) "UTF-8");
    return enc;
}

static int encodings_compare(const char const **penc1,
        const char const **penc2)
{
    return dynamic_options_strcmp(*penc1, *penc2);
    
}

Encodings *encodings_load(void)
{
    char *filename = options_file_build_filename("Encodings", NULL);

    if (filename)
    {
        GKeyFile *kf = options_file_open("Encodings", encodings_group_name);
        int count = options_file_lookup_int_with_default(kf,
            encodings_group_name, "n", 0);
        GPtrArray *enc = g_ptr_array_sized_new(count);
        int n;

        for (n = 0; n < count; ++n)
        {
            char *v = options_file_lookup_string_with_default(kf,
                    encodings_group_name, encodings_get_key(n), NULL);
            
            g_ptr_array_add(enc, v);
        }
        g_free(filename);
        g_key_file_free(kf);
        g_ptr_array_sort(enc, (GCompareFunc) encodings_compare);
        return enc;
    }
    else
    {
        return encodings_build_default();
    }
}

void encodings_save(Encodings *enc)
{
    GKeyFile *kf = g_key_file_new();
    int n;
    
    g_key_file_set_integer(kf, encodings_group_name, "n", enc->len);
    for (n = 0; n < enc->len; ++n)
    {
        g_key_file_set_string(kf, encodings_group_name,
                encodings_get_key(n), g_ptr_array_index(enc, n));
    }
    options_file_save(kf, "Encodings");
    g_key_file_free(kf);
}

char const **encodings_list(Encodings *enc)
{
    int n = encodings_count(enc);
    int m;
    char const **list;

    list = g_new(char const *, n + 2);
    list[0] = g_strdup("Default");
    for (m = 0; m < n; ++m)
    {
        list[m + 1] = encodings_lookup(enc, m);
    }
    list[n + 1] = NULL;
    return list;
}

void encodings_add(Encodings *enc, const char *v)
{
    g_ptr_array_add(enc, g_strdup(v));
    g_ptr_array_sort(enc, (GCompareFunc) encodings_compare);
}

void encodings_change(Encodings *enc, int n, const char *v)
{
    encodings_remove(enc, n);
    encodings_add(enc, v);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
