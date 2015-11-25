#ifndef OPTIONS_H
#define OPTIONS_H
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


/* Handles a group of options from one file */

#ifndef DEFNS_H
#include "defns.h"
#endif

#include <string.h>

#include "optsfile.h"

typedef struct {
    GKeyFile *kf;
    int ref;
    gpointer user_data;
    gboolean kf_dirty;
    const char *group_name; /* For GKeyFile */
    char *name;             /* leafname for saving; includes "Profiles"
                               or "Shortcuts" or whatever */
    gboolean deleted;       /* Has been deleted by configlet while still
                               in use */
} Options;


Options *options_open(const char *leafname, const char *group_name);

gboolean options_copy_keyfile(Options *dest, const Options *src);

Options *options_copy(const Options *);

void options_delete_keyfile(Options *options);

void options_reload_keyfile(Options *options);

/* Options start off with one reference when opened; this adds a reference */
inline static void options_ref(Options *options)
{
    ++options->ref;
}

/* Usually use options_unref instead except in special cases */
void options_delete(Options *options);

/* Deletes options and returns true if ref reaches zero */
gboolean options_unref(Options * options);

char *options_lookup_string_with_default(Options * options,
    const char *key, const char *default_value);

inline static char *options_lookup_string(Options * options, const char *key)
{
    return options_lookup_string_with_default(options, key, NULL);
}

int options_lookup_int_with_default(Options * options, const char *key, int d);

inline static int options_lookup_int(Options * options, const char *key)
{
    return options_lookup_int_with_default(options, key, -1);
}

double options_lookup_double_with_default(Options * options, const char *key,
        double d);

inline static double options_lookup_double(Options * options, const char *key)
{
    return options_lookup_double_with_default(options, key, 0.0);
}

void options_set_string(Options * options, const char *key, const char *value);

void options_set_int(Options * options, const char *key, int value);

void options_set_double(Options * options, const char *key, double value);

/* Associates some data with a set of options; useful eg for storing a set of
 * GdkRGBAs along with palette options */
inline static void options_associate_data(Options * options, gpointer user_data)
{
    options->user_data = user_data;
}

/* Returns the data associated above */
inline static gpointer options_get_data(Options * options)
{
    return options->user_data;
}

/* Returns the last path element of the name member */
const char *options_get_leafname(Options *options);

void options_change_leafname(Options *options, const char *new_leaf);

#endif /* OPTIONS_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
