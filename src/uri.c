/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2011 Tony Houghton <h@realh.co.uk>

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

#include <ctype.h>
#include <string.h>

#include "dlg.h"
#include "uri.h"

static char *uri_find_first_listed_in_path(char const *const *programs)
{
    int i;

    for (i = 0; programs[i]; ++i)
    {
        char *prog_name = g_strdup(programs[i]);
        char *args = strchr(prog_name, ' ');
        char *program;

        if (args)
            *args++ = 0;
        program = g_find_program_in_path(prog_name);

        g_free(prog_name);
        if (program)
        {
            if (args)
            {
                prog_name = program;
                program = g_strdup_printf("%s %s", prog_name, args);
                g_free(prog_name);
            }
            return program;
        }
    }
    return NULL;
}

/* Tries to find a browser the user likes. May include stuff like %s after the
 * command name so bear that in mind */
static char *uri_get_preferred_browser(void)
{
    char const *browsers[] = { "x-www-browser", "firefox", "iceweasel",
        "chromium", "google-chrome", "chrome", "opera", "gnome-www-browser",
        "epiphany", "konqueror", "mozilla", "netscape", NULL
    };
    int i;
    char *default_choices_path = NULL;
    char **choices_branches;
    char *browser = NULL;
    const char *env = g_getenv("BROWSER");

    if (env)
    {
        return g_strdup(env);
    }

    /* Try to find text-html handler in ROX options */
    env = g_getenv("CHOICESPATH");
    if (!env)
    {
        const char *home = g_get_home_dir();

        default_choices_path = g_strconcat(home, "/.config:",
            home, "/Choices:/usr/local/share/Choices:/usr/share/Choices", NULL);
        env = default_choices_path;
    }
    choices_branches = g_strsplit(env, ":", 0);
    if (default_choices_path)
        g_free(default_choices_path);
    for (i = 0; choices_branches[i]; ++i)
    {
        char *mime_handler = g_strconcat(choices_branches[i],
            "/MIME-types/text_html", NULL);

        /* Check for ROX appdir first */
        if (g_file_test(mime_handler, G_FILE_TEST_IS_DIR))
        {
            char *tmp = mime_handler;

            mime_handler = g_strconcat(mime_handler, "/AppRun", NULL);
            g_free(tmp);
        }
        if (g_file_test(mime_handler, G_FILE_TEST_IS_EXECUTABLE))
        {
            browser = mime_handler;
            break;
        }
        g_free(mime_handler);
    }
    g_strfreev(choices_branches);
    if (browser)
        return browser;

    browser = uri_find_first_listed_in_path(browsers);

    if (!browser)
    {
        dlg_warning(NULL, _("Unable to find a web browser"));
    }

    return browser;
}

/* cfged is what user has configured. If candidates is NULL,
 * use specialised browser finder.
 */
static char *uri_get_command(const char *uri, const char *cfged,
        char const **candidates)
{
    char *preferred = NULL;
    char *cmd = NULL;

    if (!cfged || !cfged[0])
    {
        cfged = preferred = candidates ?
                uri_find_first_listed_in_path(candidates) :
                uri_get_preferred_browser();
    }
    if (!cfged)
        return NULL;

    if (strstr(cfged, "%s"))
    {
        cmd = g_strdup_printf(cfged, uri);
    }
    else
    {
        cmd = candidates ?
                g_strdup_printf("%s %s", cfged, uri) :
                g_strdup_printf("%s '%s'", cfged, uri);
    }
    if (preferred)
        g_free(preferred);
    return cmd;
}

char *uri_get_browser_command(const char *url, const char *browser)
{
    return uri_get_command(url, browser, NULL);
}

char *uri_get_mailer_command(const char *address, const char *mailer)
{
    const char *mailers[] = { "claws-mail", "thunderbird", "icedove", "balsa",
        "evolution", "mutt", "pine", "elm", "mozilla", "mail", NULL
    };

    return uri_get_command(address, mailer, mailers);
}

char *uri_get_directory_command(const char *dirname, const char *filer)
{
    const char *filers[] = { "rox", "thunar", "nautilus -n --no-desktop",
        "dolphin", "konqueror", NULL
    };

    return uri_get_command(dirname, filer, filers);
}

char *uri_get_file_command(const char *filename, const char *filer)
{
    const char *filers[] = { "xdg-open", "rox", NULL };

    return uri_get_command(filename, filer, filers);
}

char *uri_get_ssh_command(const char *hostname, const char *ssh)
{
    const char *ssh_candidates[] = { "ssh", "rox", NULL };

    return uri_get_command(hostname, ssh, ssh_candidates);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
