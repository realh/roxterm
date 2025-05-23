#ifndef OSC52FILTER_H
#define OSC52FILTER_H
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

#include "roxterm.h"

typedef struct Osc52Filter Osc52Filter;

Osc52Filter *osc52filter_create(ROXTermData *roxterm, size_t buflen);

void osc52filter_remove(Osc52Filter *oflt);

void osc52filter_set_buffer_size(Osc52Filter *oflt, size_t buflen);

#endif /* OSC52FILTER_H */

/* vi:set sw=4 ts=4 et cindent cino= */
