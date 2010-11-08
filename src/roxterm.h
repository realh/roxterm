#ifndef ROXTERM_H
#define ROXTERM_H
/*
    roxterm - GTK+ 2.0 terminal emulator with tabs
    Copyright (C) 2004 Tony Houghton <h@realh.co.uk>

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


#ifndef DEFNS_H
#include "defns.h"
#endif

#include <vte/vte.h>

typedef struct ROXTermData ROXTermData;

/* Make sure global options have been parsed before calling this, but call it
 * before anything else like roxterm_launch. Sets up important global
 * variables etc.
 */
void roxterm_init(void);

/* Launch a new terminal in response to a D-BUS message (launcher) or for first
 * time (launcher = NULL); display_name may be NULL for default display.
 * env is copied without being altered.
 */
void roxterm_launch(const char *display_name, char **env);

/* Ways of spawning a command */
typedef enum {
	ROXTerm_SpawnExternal,		/* Run independently of ROXterm,
								   eg because it has its own window */
	ROXTerm_SpawnNewWindow,		/* Run command in a new ROXterm window */
	ROXTerm_SpawnNewTab			/* Run command in a new ROXterm tab */
} ROXTerm_SpawnType;

/* NB ROXTermData is unused if spawn type is External */
void roxterm_spawn(ROXTermData *, const char *command, ROXTerm_SpawnType);

gboolean roxterm_spawn_command_line(const gchar *command_line,
        const char *display_name, const char *cwd, char **env, GError **error);

VteTerminal *roxterm_get_vte_terminal(ROXTermData *roxterm);

const char *roxterm_get_profile_name(ROXTermData *roxterm);

const char *roxterm_get_colour_scheme_name(ROXTermData *roxterm);
        
char *roxterm_get_cwd(ROXTermData *roxterm);

/* Returns non-full-screen dimensions */
void roxterm_get_nonfs_dimensions(ROXTermData *roxterm, int *cols, int *rows);

double roxterm_get_zoom_factor(ROXTermData *roxterm);

char const * const *roxterm_get_actual_commandv(ROXTermData *roxterm);

#if ENABLE_SM
gboolean roxterm_load_session(const char *xml, gssize len,
        const char *client_id);
#endif

#endif /* ROXTERM_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
