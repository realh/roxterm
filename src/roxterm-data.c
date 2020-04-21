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

#include "colourscheme.h"
#include "dlg.h"
#include "dynopts.h"
#include "multitab.h"
#include "roxterm-app.h"
#include "roxterm-data.h"
#include "roxterm-data-priv.h"

void roxterm_data_init(ROXTermData *roxterm,
        double zoom_factor, const char *directory,
        Options *profile, gboolean maximise,
        const char *colour_scheme_name,
        char **geom, gboolean *size_on_cli, RoxtermStrvRef *env)
{
    int width, height, x, y;

    roxterm->target_zoom_factor = zoom_factor;
    if (!roxterm->target_zoom_factor)
    {
        roxterm->target_zoom_factor = 1.0;
    }
    roxterm->directory = directory ? g_strdup(directory) : g_get_current_dir();
    roxterm->profile = profile;
    if (size_on_cli)
        *size_on_cli = FALSE;
    if (*geom)
    {
        if (multi_win_parse_geometry(*geom, &width, &height, &x, &y, NULL))
        {
            roxterm->columns = width;
            roxterm->rows = height;
            if (size_on_cli)
                *size_on_cli = TRUE;
        }
        else
        {
            dlg_warning(NULL, _("Invalid geometry specification"));
            *geom = NULL;
        }
    }
    roxterm->maximise = maximise;
    roxterm->borderless = options_lookup_int_with_default(profile,
                                        "borderless", 0);
    if (colour_scheme_name)
    {
        roxterm->colour_scheme = colour_scheme_lookup_and_ref
            (colour_scheme_name);
    }
    roxterm->env = env;
    if (env)
        ++env->count;
}

ROXTermData *roxterm_data_new(double zoom_factor, const char *directory,
        Options *profile, gboolean maximise,
        const char *colour_scheme_name,
        char **geom, gboolean *size_on_cli, RoxtermStrvRef *env)
{
    ROXTermData *roxterm = g_new0(ROXTermData, 1);
    roxterm_data_init(roxterm, zoom_factor, directory,
        profile, maximise, colour_scheme_name,
        geom, size_on_cli, env);
    return roxterm;
}

void roxterm_data_delete_static(ROXTermData *roxterm)
{
    g_return_if_fail(roxterm);

    if (roxterm->colour_scheme)
    {
        UNREF_LOG(colour_scheme_unref(roxterm->colour_scheme));
        roxterm->colour_scheme = NULL;
    }
    if (roxterm->profile)
    {
        UNREF_LOG(dynamic_options_unref(roxterm_get_profiles(),
                options_get_leafname(roxterm->profile)));
        roxterm->profile = NULL;
    }
    if (roxterm->actual_commandv && (!roxterm->commandv ||
                roxterm->actual_commandv != roxterm->commandv->strv))
    {
        g_strfreev(roxterm->actual_commandv);
        roxterm->actual_commandv = NULL;
    }
    if (roxterm->commandv)
    {
        roxterm_strv_ref_unref(roxterm->commandv);
        roxterm->commandv = NULL;
    }
    g_free(roxterm->directory);
    if (roxterm->env)
    {
        roxterm_strv_ref_unref(roxterm->env);
        roxterm->env = NULL;
    }
}

void roxterm_data_delete(ROXTermData *roxterm)
{
    roxterm_data_delete_static(roxterm);
    g_free(roxterm);
}

void roxterm_data_init_from_partner(ROXTermData *roxterm, ROXTermData *partner)
{
    roxterm->dont_lookup_dimensions = TRUE;
    roxterm->target_zoom_factor = partner->target_zoom_factor;
    roxterm->maximise = partner->maximise;
    roxterm->borderless = partner->borderless;
}

/* vi:set sw=4 ts=4 noet cindent cino= */
