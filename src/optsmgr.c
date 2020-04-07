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

#include "dynopts.h"
#include "optsmgr.h"

enum {
    SIG_DELETE_OPTS,
    SIG_ADD_OPTS,
    SIG_RENAME_OPTS,
    SIG_OPT_CHANGED,

    NUM_SIGS
};

struct _RoxtermOptsManager {
    GObject parent_instance;
};

guint roxterm_opts_manager_signals[NUM_SIGS];

G_DEFINE_TYPE(RoxtermOptsManager, roxterm_opts_manager, G_TYPE_OBJECT);

void roxterm_opts_manager_class_init(UNUSED RoxtermOptsManagerClass *klass)
{
    roxterm_opts_manager_signals[SIG_DELETE_OPTS] =
        g_signal_new("delete-options",
            ROXTERM_TYPE_OPTS_MANAGER,
            G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            0, NULL, NULL, NULL,
            G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
    roxterm_opts_manager_signals[SIG_ADD_OPTS] =
        g_signal_new("add-options",
            ROXTERM_TYPE_OPTS_MANAGER,
            G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            0, NULL, NULL, NULL,
            G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
    roxterm_opts_manager_signals[SIG_RENAME_OPTS] =
        g_signal_new("rename-options",
            ROXTERM_TYPE_OPTS_MANAGER,
            G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            0, NULL, NULL, NULL,
            G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    roxterm_opts_manager_signals[SIG_RENAME_OPTS] =
        g_signal_new("option-changed",
            ROXTERM_TYPE_OPTS_MANAGER,
            G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            0, NULL, NULL, NULL,
            G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
}

void roxterm_opts_manager_init(UNUSED RoxtermOptsManager *mgr)
{
}

static RoxtermOptsManager *roxterm_opts_manager = NULL;

RoxtermOptsManager *roxterm_opts_manager_get(void)
{
    if (!roxterm_opts_manager)
        roxterm_opts_manager = g_object_new(ROXTERM_TYPE_OPTS_MANAGER, NULL);
    return roxterm_opts_manager;
}

/**
 * roxterm_opts_manager_delete_options: (method): Deletes a profile/scheme
 * @name: Name of profile/scheme
 */
void roxterm_opts_manager_delete_options(RoxtermOptsManager *mgr,
        const char *family, const char *name)
{
    g_signal_emit(mgr, roxterm_opts_manager_signals[SIG_DELETE_OPTS], 0,
            family, name);
    DynamicOptions *dynopts = dynamic_options_get(family);
    dynamic_options_unref(dynopts, name);
}

void roxterm_opts_manager_add_options(RoxtermOptsManager *mgr,
        const char *family, const char *name)
{
    DynamicOptions *dynopts = dynamic_options_get(family);
    (void) dynamic_options_lookup_and_ref(dynopts, name, family);
    g_signal_emit(mgr, roxterm_opts_manager_signals[SIG_ADD_OPTS], 0,
            family, name);
}

void roxterm_opts_manager_rename_options(RoxtermOptsManager *mgr,
        const char *family, const char *old_name, const char *new_name)
{
    DynamicOptions *dynopts = dynamic_options_get(family);
    dynamic_options_rename(dynopts, old_name, new_name);
    g_signal_emit(mgr, roxterm_opts_manager_signals[SIG_RENAME_OPTS], 0,
            family, old_name, new_name);
}

void roxterm_opts_manager_option_changed(RoxtermOptsManager *mgr,
        const char *family, const char *name, const char *key)
{
    g_signal_emit(mgr, roxterm_opts_manager_signals[SIG_OPT_CHANGED], 0,
            family, name, key);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
