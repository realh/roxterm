#ifndef ROXTERM_DATA_H
#define ROXTERM_DATA_H
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

#include "options.h"
#include "strv-ref.h"

G_BEGIN_DECLS

typedef struct ROXTermData ROXTermData;

/**
 * roxterm_data_new: (constructor):
 * @profile_name: (transfer full):
 * @profile: (transfer full):
 * @geom: (in-out) (transfer none) (nullable): Set to NULL if invalid
 * @size_on_cli: (out): Set to TRUE if geom was specified
 * @env: (transfer none):
 */
ROXTermData *roxterm_data_new(double zoom_factor, const char *directory,
        const char *profile_name, Options *profile, gboolean maximise,
        const char *colour_scheme_name,
        char **geom, gboolean *size_on_cli, RoxtermStrvRef *env);

/**
 * roxterm_data_delete_static: Frees all data used, but not the outer struct
 * @roxterm: (transfer none):
 */
void roxterm_data_delete_static(ROXTermData *roxterm);

/**
 * roxterm_data_delete: Frees all data and the outer struct
 * @roxterm: (transfer full):
 */
void roxterm_data_delete(ROXTermData *roxterm);

/**
 * roxterm_data_init_from_partner: (method): Copy certain settings from @partner
 *      to @roxterm when the latter is a new tab to share @partner's window
 * @roxterm: (transfer none):
 * @partner: (transfer none):
 */
void roxterm_data_init_from_partner(ROXTermData *roxterm, ROXTermData *partner);

G_END_DECLS

#endif /* ROXTERM_DATA_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
