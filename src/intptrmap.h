#ifndef INTPTRMAP_H
#define INTPTRMAP_H
/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2024 Tony Houghton <h@realh.co.uk>

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

#include <glib.h>

/* Wrapper for a GHashTable of pointers keyed by ints, adding a set of bits to
 * provide a shortcut for the contains method for keys up to 255.
 */
typedef struct {
    GHashTable *hash;
    uint64_t bits[4];
} IntPointerMap;

#define ONE64 ((uint64_t) 1)

static inline IntPointerMap *int_pointer_map_init(IntPointerMap *ipm)
{
    ipm->bits[0] = 0;
    ipm->bits[1] = 0;
    ipm->bits[2] = 0;
    ipm->bits[3] = 0;
    ipm->hash = g_hash_table_new(g_direct_hash, g_direct_equal);
    return ipm;
}

static inline gboolean int_pointer_map_contains(IntPointerMap *ipm, int key)
{
    if (key < 64)
        return (ipm->bits[0] & (ONE64 << key)) != 0;
    if (key < 256)
        return (ipm->bits[key / 64] & (ONE64 << (key % 64))) != 0;
    return g_hash_table_contains(ipm->hash, GINT_TO_POINTER(key));
}

static inline gboolean int_pointer_map_insert(IntPointerMap *ipm, int key,
                                              gpointer value)
{
    if (key < 256)
        ipm->bits[key / 64] |= (ONE64 << (key % 64));
    return g_hash_table_insert(ipm->hash, GINT_TO_POINTER(key), value);
}

static inline gpointer int_pointer_map_lookup(IntPointerMap *ipm, int key)
{
    return g_hash_table_lookup(ipm->hash, GINT_TO_POINTER(key));
}

static inline gboolean int_pointer_map_remove(IntPointerMap *ipm, int key)
{
    if (key < 256)
        ipm->bits[key / 64] &= ~(ONE64 << (key % 64));
    return g_hash_table_remove(ipm->hash, GINT_TO_POINTER(key));
}

#endif /* INTPTRMAP_H */

/* vi:set sw=4 ts=4 et cindent cino= */
