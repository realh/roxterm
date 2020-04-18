#ifndef ROXTERM_DATA_PRIV_H
#define ROXTERM_DATA_PRIV_H
/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2018 Tony Houghton <h@realh.co.uk>

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

struct ROXTermData {
    /* We own references to colour_scheme and profile */
    Options *colour_scheme;
    Options *profile;
    char *special_command;      /* Next window/tab opened from this one should
                                   run this command instead of default */
    RoxtermStrvRef *commandv;   /* From --execute args */
    char **actual_commandv;     /* The actual command used */
    char *directory;            /* Copied from launch params */
    double target_zoom_factor;
    gboolean borderless;
    gboolean maximise;
    gboolean dont_lookup_dimensions;
    int columns, rows;
    RoxtermStrvRef *env;
    gboolean from_session;
};

G_END_DECLS

#endif //ROXTERM_DATA_PRIV_H
