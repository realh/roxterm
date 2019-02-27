/*
    roxterm - VTE/GTK terminal emulator with tabs
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
#ifndef __ROXTERM_LAUNCH_PARAMS_H
#define __ROXTERM_LAUNCH_PARAMS_H

#include <glib.h>

typedef struct {
    char *profile_name;
    char *tab_title;
    char *directory;
    gboolean vim;
} RoxtermTabLaunchParams;

RoxtermTabLaunchParams *roxterm_tab_launch_params_new(void);

void roxterm_tab_launch_params_free(RoxtermTabLaunchParams *tp);

typedef struct {
    GList *tabs;        // element-type: RoxtermTabLaunchParams 
    char *window_title;
    char *role;
    char *geometry;
    char *zoom_str;
    gdouble zoom; 
    gboolean maximized;
    gboolean fullscreen;
    gboolean implicit;  // means --tab was specified and tabs should be added
                        // to an existing window if possible
} RoxtermWindowLaunchParams;

RoxtermWindowLaunchParams *roxterm_window_launch_params_new(void);

void roxterm_window_launch_params_free(RoxtermWindowLaunchParams *wp);

typedef struct {
    GList *windows;     // element-type: RoxtermWindowLaunchParams 
    int argc;
    char **argv;
    const gchar * const *env;
} RoxtermLaunchParams;

RoxtermLaunchParams *roxterm_launch_params_new(void);

RoxtermLaunchParams *
roxterm_launch_params_new_from_command_line(GApplicationCommandLine *cmd,
        GError **error);

void roxterm_launch_params_free(RoxtermLaunchParams *lp);

#endif /* __ROXTERM_LAUNCH_PARAMS_H */