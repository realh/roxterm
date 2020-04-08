/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2015-2020 Tony Houghton <h@realh.co.uk>

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

#include "dlg.h"
#include "roxterm-dirs.h"

char *roxterm_dir = NULL;

char *roxterm_bin_dir = NULL;

void roxterm_init_app_dir(int argc, char **argv)
{
    int n;

    for (n = 1; n < argc; ++n)
    {
        if (g_str_has_prefix(argv[n], "--appdir=") && strlen(argv[n]) > 9)
        {
            roxterm_app_dir = g_strdup(argv[n] + 9);
            break;
        }
    }
}

void roxterm_init_bin_dir(const char *argv0)
{
    roxterm_bin_dir = g_path_get_dirname(argv0);
    if (!strcmp(roxterm_bin_dir, "."))
    {
        /* No directory component, try to find roxterm in BIN_DIR,
         * current dir, then PATH */
        char *capplet;

        g_free(roxterm_bin_dir);
        roxterm_bin_dir = NULL;
        capplet = g_build_filename(BIN_DIR, "roxterm", NULL);
        if (g_file_test(capplet, G_FILE_TEST_IS_EXECUTABLE))
        {
            roxterm_bin_dir = g_strdup(BIN_DIR);
        }
        else if (g_file_test("roxterm", G_FILE_TEST_IS_EXECUTABLE))
        {
            /* Paranoia in case g_free() != free() */
            char *bindir = g_get_current_dir();

            roxterm_bin_dir = g_strdup(bindir);
            g_free(bindir);
        }
        else
        {
            char *full = g_find_program_in_path("roxterm");

            if (full)
            {
                roxterm_bin_dir = g_path_get_dirname(full);
                g_free(full);
            }
            else
            {
                dlg_critical(NULL, _("Can't find roxterm binary"));
            }
        }
    }
    else if (!g_path_is_absolute(roxterm_bin_dir))
    {
        /* Partial path, must be relative to current dir */
        char *cur = g_get_current_dir();
        char *full = g_build_filename(cur, roxterm_bin_dir, NULL);

        g_free(cur);
        g_free(roxterm_bin_dir);
        roxterm_bin_dir = full;
    }
    /* else full path was given, use that (already assigned) */
}
