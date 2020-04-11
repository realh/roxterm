/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2015-2020 Tony Houghton <h@realh.co.uk>

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
#ifndef LAUNCH_PARAMS_H
#define LAUNCH_PARAMS_H

#include "roxterm.h"
#include "strv-ref.h"

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct {
    int refcount;
    char *profile_name;
    char *colour_scheme_name;
    char *shortcuts_name;
    char *tab_title;
    char *directory;
    RoxtermChildExitAction exit_action;
    gboolean custom_command;
} RoxtermTabLaunchParams;

RoxtermTabLaunchParams *roxterm_tab_launch_params_new(void);

inline static RoxtermTabLaunchParams *
roxterm_tab_launch_params_ref(RoxtermTabLaunchParams *tp)
{
    ++tp->refcount;
    return tp;
}

void roxterm_tab_launch_params_unref(RoxtermTabLaunchParams *tp);

typedef struct {
    int refcount;
    GList *tabs;        // element-type: RoxtermTabLaunchParams 
    char *window_title;
    char *role;
    char *geometry_str;
    char *zoom_str;
    gdouble zoom; 
    int columns, rows;
    gboolean maximized;
    gboolean fullscreen;
    gboolean borderless;
    gboolean implicit;  // means --tab was specified and tabs should be added
                        // to an existing window if possible
} RoxtermWindowLaunchParams;

RoxtermWindowLaunchParams *roxterm_window_launch_params_new(void);

inline static RoxtermWindowLaunchParams *
roxterm_window_launch_params_ref(RoxtermWindowLaunchParams *wp)
{
    ++wp->refcount;
    return wp;
}

void roxterm_window_launch_params_unref(RoxtermWindowLaunchParams *wp);


typedef struct {
    int refcount;
    GList *windows;     // element-type: RoxtermWindowLaunchParams 
    int argc;
    char **argv;
    RoxtermStrvRef *env;
    char *user_session_id;
} RoxtermLaunchParams;

RoxtermLaunchParams *roxterm_launch_params_new(void);

RoxtermLaunchParams *
roxterm_launch_params_new_from_command_line(GApplicationCommandLine *cmd,
        GError **error);

inline static RoxtermLaunchParams *
roxterm_launch_params_ref(RoxtermLaunchParams *lp)
{
    ++lp->refcount;
    return lp;
}

void roxterm_launch_params_unref(RoxtermLaunchParams *lp);

// Separate args pre- and post- --execute; lp can be NULL, in which case
// everything from --execute is discarded
gboolean roxterm_launch_params_preparse_argv_execute(RoxtermLaunchParams *lp,
        int *argc, char ***pargv, GError **error);

GOptionContext *roxterm_launch_params_get_option_context(gpointer handle);

G_END_DECLS

#endif /* LAUNCH_PARAMS_H */
