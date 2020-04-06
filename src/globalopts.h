#ifndef GLOBALOPTS_H
#define GLOBALOPTS_H
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


#include "options.h"

void global_options_init(void);

/* Only access via following functions */
extern Options *global_options;

inline static char *global_options_lookup_string_with_default(const char
	*key, const char *default_value)
{
	return options_lookup_string_with_default(global_options, key,
		default_value);
}

inline static char *global_options_lookup_string(const char *key)
{
	return options_lookup_string_with_default(global_options, key, NULL);
}

inline static int global_options_lookup_int_with_default(const char *key, int d)
{
	return options_lookup_int_with_default(global_options, key, d);
}

inline static int global_options_lookup_int(const char *key)
{
	return options_lookup_int(global_options, key);
}

inline static double global_options_lookup_double(const char *key)
{
	return options_lookup_double(global_options, key);
}

/* Reset a string option which should only be "one-shot" */
inline static void global_options_reset_string(const char *key)
{
    options_set_string(global_options, key, NULL);
}

void global_options_apply_dark_theme(void);

#endif /* GLOBALOPTS_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
