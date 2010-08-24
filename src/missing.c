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

#include <errno.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <glib/gstdio.h>

#ifndef HAVE_G_FILE_SET_CONTENTS

/* This section is lifted from glib (and reindented) */

#ifndef O_BINARY
#define O_BINARY 0
#endif

static gboolean
rename_file(const char *old_name, const char *new_name, GError ** err)
{
	errno = 0;
	if (g_rename(old_name, new_name) == -1)
	{
		int save_errno = errno;
		gchar *display_old_name = g_filename_display_name(old_name);
		gchar *display_new_name = g_filename_display_name(new_name);

		g_set_error(err,
					G_FILE_ERROR,
					g_file_error_from_errno(save_errno),
					_("Failed to rename file '%s' to '%s': "
					  "g_rename() failed: %s"),
					display_old_name, display_new_name,
					g_strerror(save_errno));

		g_free(display_old_name);
		g_free(display_new_name);

		return FALSE;
	}

	return TRUE;
}

static gint create_temp_file(gchar * tmpl, int permissions)
{
	int len;
	char *XXXXXX;
	int count, fd;
	static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static const int NLETTERS = sizeof(letters) - 1;
	glong value;
	GTimeVal tv;
	static int counter = 0;

	len = strlen(tmpl);
	if (len < 6 || strcmp(&tmpl[len - 6], "XXXXXX"))
	{
		errno = EINVAL;
		return -1;
	}

	/* This is where the Xs start.  */
	XXXXXX = &tmpl[len - 6];

	/* Get some more or less random data.  */
	g_get_current_time(&tv);
	value = (tv.tv_usec ^ tv.tv_sec) + counter++;

	for (count = 0; count < 100; value += 7777, ++count)
	{
		glong v = value;

		/* Fill in the random bits.  */
		XXXXXX[0] = letters[v % NLETTERS];
		v /= NLETTERS;
		XXXXXX[1] = letters[v % NLETTERS];
		v /= NLETTERS;
		XXXXXX[2] = letters[v % NLETTERS];
		v /= NLETTERS;
		XXXXXX[3] = letters[v % NLETTERS];
		v /= NLETTERS;
		XXXXXX[4] = letters[v % NLETTERS];
		v /= NLETTERS;
		XXXXXX[5] = letters[v % NLETTERS];

		/* tmpl is in UTF-8 on Windows, thus use g_open() */
		fd = g_open(tmpl, O_RDWR | O_CREAT | O_EXCL | O_BINARY, permissions);

		if (fd >= 0)
			return fd;
		else if (errno != EEXIST)
			/* Any other error will apply also to other names we might
			 *  try, and there are 2^32 or so of them, so give up now.
			 */
			return -1;
	}

	/* We got out of the loop because we ran out of combinations to try.  */
	errno = EEXIST;
	return -1;
}


static gchar *write_to_temp_file(const gchar * contents,
								 gssize length, const gchar * template,
								 GError ** err)
{
	gchar *tmp_name;
	gchar *display_name;
	gchar *retval;
	FILE *file;
	gint fd;
	int save_errno;

	retval = NULL;

	tmp_name = g_strdup_printf("%s.XXXXXX", template);

	errno = 0;
	fd = create_temp_file(tmp_name, 0666);
	display_name = g_filename_display_name(tmp_name);

	if (fd == -1)
	{
		save_errno = errno;
		g_set_error(err,
					G_FILE_ERROR,
					g_file_error_from_errno(save_errno),
					_("Failed to create file '%s': %s"),
					display_name, g_strerror(save_errno));

		goto out;
	}

	errno = 0;
	file = fdopen(fd, "wb");
	if (!file)
	{
		save_errno = errno;
		g_set_error(err,
					G_FILE_ERROR,
					g_file_error_from_errno(save_errno),
					_("Failed to open file '%s' for writing: "
					  "fdopen() failed: %s"),
					display_name, g_strerror(save_errno));

		close(fd);
		g_unlink(tmp_name);

		goto out;
	}

	if (length > 0)
	{
		size_t n_written;

		errno = 0;

		n_written = fwrite(contents, 1, length, file);

		if (n_written < length)
		{
			save_errno = errno;

			g_set_error(err,
						G_FILE_ERROR,
						g_file_error_from_errno(save_errno),
						_
						("Failed to write file '%s': fwrite() failed: %s"),
						display_name, g_strerror(save_errno));

			fclose(file);
			g_unlink(tmp_name);

			goto out;
		}
	}

	errno = 0;
	if (fclose(file) == EOF)
	{
		save_errno = 0;

		g_set_error(err,
					G_FILE_ERROR,
					g_file_error_from_errno(save_errno),
					_("Failed to close file '%s': fclose() failed: %s"),
					display_name, g_strerror(save_errno));

		g_unlink(tmp_name);

		goto out;
	}

	retval = g_strdup(tmp_name);

  out:
	g_free(tmp_name);
	g_free(display_name);
	return retval;
}

gboolean g_file_set_contents(const gchar * filename,
					const gchar * contents, gssize length, GError ** error)
{
	gchar *tmp_filename;
	gboolean retval;
	GError *rename_error = NULL;

	g_return_val_if_fail(filename != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail(contents != NULL || length == 0, FALSE);
	g_return_val_if_fail(length >= -1, FALSE);

	if (length == -1)
		length = strlen(contents);

	tmp_filename = write_to_temp_file(contents, length, filename, error);

	if (!tmp_filename)
	{
		retval = FALSE;
		goto out;
	}

	if (!rename_file(tmp_filename, filename, &rename_error))
	{
#ifndef G_OS_WIN32

		g_unlink(tmp_filename);
		g_propagate_error(error, rename_error);
		retval = FALSE;
		goto out;

#else /* G_OS_WIN32 */

		/* Renaming failed, but on Windows this may just mean
		 * the file already exists. So if the target file
		 * exists, try deleting it and do the rename again.
		 */
		if (!g_file_test(filename, G_FILE_TEST_EXISTS))
		{
			g_unlink(tmp_filename);
			g_propagate_error(error, rename_error);
			retval = FALSE;
			goto out;
		}

		g_error_free(rename_error);

		if (g_unlink(filename) == -1)
		{
			gchar *display_filename = g_filename_display_name(filename);

			int save_errno = errno;

			g_set_error(error,
						G_FILE_ERROR,
						g_file_error_from_errno(save_errno),
						_("Existing file '%s' could not be removed: "
						  "g_unlink() failed: %s"),
						display_filename, g_strerror(save_errno));

			g_free(display_filename);
			g_unlink(tmp_filename);
			retval = FALSE;
			goto out;
		}

		if (!rename_file(tmp_filename, filename, error))
		{
			g_unlink(tmp_filename);
			retval = FALSE;
			goto out;
		}

#endif
	}

	retval = TRUE;

  out:
	g_free(tmp_filename);
	return retval;
}

#endif /* ! HAVE_G_FILE_SET_CONTENTS */
