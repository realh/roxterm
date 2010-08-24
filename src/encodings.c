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

#include "defns.h"

#include <string.h>

#include "encodings.h"
#include "optsfile.h"

static const char *encodings_group_name = "roxterm encodings";

static const char *encodings_get_key(int n)
{
	static char key[16];

	sprintf(key, "e%d", n);
	return key;
}

static GKeyFile *encodings_build_default(void)
{
	GKeyFile *kf = g_key_file_new();

	g_key_file_set_integer(kf, encodings_group_name, "n", 2);
	g_key_file_set_string(kf, encodings_group_name, "e0", "UTF-8");
	g_key_file_set_string(kf, encodings_group_name, "e1", "ISO-8859-1");
	return kf;
}

GKeyFile *encodings_load(void)
{
	char *filename = options_file_build_filename("Encodings", NULL);

	if (filename)
	{
		GKeyFile *result = options_file_open("Encodings", encodings_group_name);

		g_free(filename);
		return result;
	}
	else
	{
		return encodings_build_default();
	}
}

void encodings_save(GKeyFile *kf)
{
	options_file_save(kf, "Encodings");
}

char **encodings_list(GKeyFile *kf)
{
	int n = encodings_count(kf);
	int m;
	char **list;

	list = g_new(char *, n + 2);
	list[0] = g_strdup("Default");
	for (m = 0; m < n; ++m)
	{
		list[m + 1] = encodings_lookup(kf, m);
	}
	list[n + 1] = NULL;
	return list;
}

char *encodings_lookup(GKeyFile *kf, int n)
{
	return options_file_lookup_string_with_default(kf,
		encodings_group_name, encodings_get_key(n), "UTF-8");
}

int encodings_count(GKeyFile *kf)
{
	return options_file_lookup_int_with_default(kf,
			encodings_group_name, "n", 0);
}

void encodings_add(GKeyFile *kf, const char *enc)
{
	int n = encodings_count(kf);
	
	encodings_change(kf, n, enc);
	g_key_file_set_integer(kf, encodings_group_name, "n", n + 1);
}

void encodings_remove(GKeyFile *kf, int n)
{
	int count = encodings_count(kf);

	for (; n < count - 1; ++n)
	{
		char *enc = encodings_lookup(kf, n + 1);

		encodings_change(kf, n, enc);
		g_free(enc);
	}
	g_key_file_set_integer(kf, encodings_group_name, "n", count - 1);
}

void encodings_change(GKeyFile *kf, int n, const char *enc)
{
	g_key_file_set_string(kf, encodings_group_name,
			encodings_get_key(n), enc);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
