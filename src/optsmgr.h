#ifndef OPTSMGR_H
#define OPTSMGR_H
/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2020 Tony Houghton <h@realh.co.uk>

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

#include <glib-object.h>

/**
 * RoxtermOptsManager: A singleton to propagate changes of options between
 *                     config UI and terminals
 * 
 * String arguments etc named "family" have the possible values "Profiles",
 * "Colours" or "Shortcuts".
 */
#define ROXTERM_TYPE_OPTS_MANAGER roxterm_opts_manager_get_type()
G_DECLARE_FINAL_TYPE(RoxtermOptsManager, roxterm_opts_manager,
        ROXTERM, OPTS_MANAGER, GObject);

/**
 * roxterm_opts_manager_get:
 * Returns: The singleton
 */
RoxtermOptsManager *roxterm_opts_manager_get(void);

/**
 * roxterm_opts_manager_delete_options: (method): Deletes a profile/scheme
 * @name: Name of profile/scheme
 */
void roxterm_opts_manager_delete_options(RoxtermOptsManager *mgr,
        const char *family, const char *name);

/**
 * RoxtermOptsManager::delete-options:
 * @name: Name of profile/scheme
 *
 * This signal is raised before the deletion.
 */

/**
 * roxterm_opts_manager_add_options: (method): Adds a profile/scheme
 * @name: Name of profile/scheme
 */
void roxterm_opts_manager_add_options(RoxtermOptsManager *mgr,
        const char *family, const char *name);

/**
 * RoxtermOptsManager::add-options:
 * @name: Name of profile/scheme
 *
 * This signal is raised after the addition.
 */

/**
 * roxterm_opts_manager_rename_options: (method): Renames a profile/scheme
 * @old_name: Old name of profile/scheme
 * @new_name: Old name of profile/scheme
 */
void roxterm_opts_manager_rename_options(RoxtermOptsManager *mgr,
        const char *family, const char *old_name, const char *new_name);

/**
 * RoxtermOptsManager::rename-options:
 * @old_name: Old name of profile/scheme
 * @new_name: Old name of profile/scheme
 *
 * This signal is raised after the rename.
 */

/**
 * roxterm_opts_manager_option_changed: (method): Signal that an option's
 *      value has changed
 * @name: Name of profile/scheme
 * @key: Option key
 */
void roxterm_opts_manager_option_changed(RoxtermOptsManager *mgr,
        const char *family, const char *name, const char *key);

/**
 * RoxtermOptsManager::option-changed:
 *      value has changed
 * @name: Name of profile/scheme
 * @key: Option key
 */

#endif /* OPTSMGR_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
