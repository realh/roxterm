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

#include <ctype.h>
#include <string.h>

#include "uri.h"

char *uri_get_ssh_command(const char *uri, const char *ssh)
{
    const char *at;
    char *result;
    char *hostname;
    const char *port;
    char *user;

    if (!ssh)
        ssh = "ssh";
    if (g_str_has_prefix(uri, "ssh://"))
        uri += 6;

    at = strchr(uri, '@');
    if (at)
    {
        char *colon;

        user = g_strdup_printf("%.*s", (int) (at - uri), uri);
        colon = strchr(user, ':');
        /* Just strip password, ssh doesn't support it on CLI */
        if (colon)
            *colon = 0;
        uri = at + 1;
    }
    else
    {
        user = NULL;
    }

    port = strchr(uri, ':');
    if (port)
    {
        hostname = g_strdup_printf("%.*s", (int) (port - uri), uri);
        ++port;
        uri = hostname;
    }
    else
    {
        hostname = NULL;
    }

    result = g_strdup_printf("%s%s%s%s%s %s", ssh,
            user ? " -l " : "", user ? user : "",
            port ? " -p " : "", port ? port : "",
            uri);
    g_free(hostname);
    g_free(user);
    return result;
}

/* vi:set sw=4 ts=4 noet cindent cino= */
