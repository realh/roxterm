/*
    roxterm - PROFILE/GTK terminal emulator with tabs
    Copyright (C) 2019 Tony Houghton <h@realh.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef __ROXTERM_PROFILE_H
#define __ROXTERM_PROFILE_H

#include <glib-object.h>

#include "roxterm-rgba.h"

G_BEGIN_DECLS

#define ROXTERM_TYPE_PROFILE roxterm_profile_get_type()
G_DECLARE_FINAL_TYPE(RoxtermProfile, roxterm_profile, ROXTERM, PROFILE,
        GObject);

RoxtermProfile *roxterm_profile_new(const char *name);

/**
 * roxterm_profile_get_string:
 * Returns: (transfer full):
 */
char *roxterm_profile_get_string(RoxtermProfile *self, const char *key);

void roxterm_profile_set_string(RoxtermProfile *self, const char *key,
        const char *value);

int roxterm_profile_get_int(RoxtermProfile *self, const char *key);

void roxterm_profile_set_int(RoxtermProfile *self, const char *key, int value);

gboolean roxterm_profile_get_boolean(RoxtermProfile *self, const char *key);

void roxterm_profile_set_boolean(RoxtermProfile *self, const char *key,
        gboolean value);

double roxterm_profile_get_float(RoxtermProfile *self, const char *key);

void roxterm_profile_set_float(RoxtermProfile *self, const char *key,
        double value);

RoxtermRGBA roxterm_profile_get_rgba(RoxtermProfile *self, const char *key);

void roxterm_profile_set_rgba(RoxtermProfile *self, const char *key,
        RoxtermRGBA value);

const char *roxterm_profile_get_user_directory(void);

void roxterm_profile_load(RoxtermProfile *self);

void roxterm_profile_save(RoxtermProfile *self);

gboolean roxterm_profile_has_string(RoxtermProfile *self, const char *key);
gboolean roxterm_profile_has_int(RoxtermProfile *self, const char *key);
gboolean roxterm_profile_has_boolean(RoxtermProfile *self, const char *key);
gboolean roxterm_profile_has_float(RoxtermProfile *self, const char *key);
gboolean roxterm_profile_has_rgba(RoxtermProfile *self, const char *key);

/**
 * roxterm_profile_lookup:
 *
 * @name: (nullable): NULL implies "Default"
 * Returns: (transfer full) (nullable): Callee takes a reference
 */
RoxtermProfile *roxterm_profile_lookup(const char *name);

G_END_DECLS

#endif /* __ROXTERM_PROFILE_H */
