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


#include "defns.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "dlg.h"
#include "options.h"
#include "optsfile.h"

void options_reload_keyfile(Options *options)
{
	if (options->kf)
		options_delete_keyfile(options);
	options->kf = options_file_open(options->name, options->group_name);
	options->kf_dirty = FALSE;
}

Options *options_open(const char *leafname, const char *group_name)
{
	Options *options = g_new0(Options, 1);

	options->group_name = group_name;
	options->name = g_strdup(leafname);
	options->ref = 1;
	options_reload_keyfile(options);
	return options;
}

gboolean options_copy_keyfile(Options *dest, const Options *src)
{
	gsize l = 0;
	GError *err = NULL;
	char *kf_data = g_key_file_to_data(src->kf, &l, &err);
	GKeyFile *old_kf = NULL;
	gboolean result = TRUE;
	
	if (dest->kf)
	{
		old_kf = dest->kf;
		dest->kf = NULL;
	}
	if (err)
		goto copy_err;
	dest->kf = g_key_file_new();
	if (!l || g_key_file_load_from_data(dest->kf, kf_data, l,
		G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &err))
	{
		goto copy_done;
	}

copy_err:
	result = FALSE;
	if (err && err->message)
		g_critical(_("Unable to copy options keyfile data: %s"), err->message);
	if (dest->kf)
		g_key_file_free(dest->kf);
	dest->kf = old_kf;
	old_kf = NULL;
	
copy_done:
	if (old_kf)
		g_key_file_free(old_kf);
	g_free(kf_data);
	return result;
}

Options *options_copy(const Options *old_opts)
{
	Options *new_opts = g_new(Options, 1);
	
	*new_opts = *old_opts;
	new_opts->kf = NULL;
	new_opts->user_data = NULL;
	if (options_copy_keyfile(new_opts, old_opts))
	{
		new_opts->name = g_strdup(old_opts->name);
	}
	else
	{
		g_free(new_opts);
		new_opts = NULL;
	}
	return new_opts;
}

void options_delete_keyfile(Options * options)
{
	options_file_delete(options->kf);
	options->kf = NULL;
}

void options_delete(Options *options)
{
	if (options->kf)
		options_delete_keyfile(options);
	g_free(options->name);
	g_free(options);
}

/* Deletes options and returns true if ref reaches zero */
gboolean options_unref(Options * options)
{
	if (!--options->ref)
	{
		options_delete(options);
		return TRUE;
	}
	return FALSE;
}

char *options_lookup_string_with_default(Options * options,
	const char *key, const char *default_value)
{
	return options_file_lookup_string_with_default(
			options->kf, options->group_name,
			key, default_value);
}

int options_lookup_int_with_default(Options * options,
	const char *key, int default_value)
{
	return options_file_lookup_int_with_default(
			options->kf, options->group_name,
			key, default_value);
}

double options_lookup_double_with_default(Options *options, const char *key,
        double d)
{
	char *str_val = options_lookup_string(options, key);
	double result = d;

	if (str_val)
	{
		char *endptr;

		errno = 0;
		result = strtod(str_val, &endptr);
		if (endptr == str_val)
		{
			dlg_warning(NULL,
			  _("Unable to convert value '%s' for key '%s' in '%s' to number"),
				str_val, key, options->name);
            result = d;
		}
		else if (errno)
		{
			dlg_warning(NULL,
				_("Unable to convert value '%s' for key '%s' in '%s' "
						"to number: %s"),
				str_val, key, options->name, strerror(errno));
            result = d;
		}
	}
	return result;
}


void options_set_string(Options * options, const char *key, const char *value)
{
	if (!options->kf)
		options->kf = g_key_file_new();
	g_key_file_set_string(options->kf, options->group_name, key,
			value ? value : "");
}

void options_set_int(Options * options, const char *key, int value)
{
	if (!options->kf)
		options->kf = g_key_file_new();
	g_key_file_set_integer(options->kf, options->group_name, key, value);
}

void options_set_double(Options * options, const char *key, double value)
{
	char *str_val = g_strdup_printf("%f", value);

	options_set_string(options, key, str_val);
	g_free(str_val);
}

const char *options_get_leafname(Options *options)
{
	const char *leafname = strrchr(options->name, G_DIR_SEPARATOR);

	return leafname ? leafname + 1 : options->name;
}

void options_change_leafname(Options *options, const char *new_leaf)
{
	char *old_name = options->name;
	char *old_leaf = strrchr(options->name, G_DIR_SEPARATOR);

	if (old_leaf)
	{
		*old_leaf = 0;
		options->name = g_build_filename(old_name, new_leaf, NULL);
	}
	else
	{
		options->name = g_strdup(new_leaf);
	}
	g_free(old_name);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
