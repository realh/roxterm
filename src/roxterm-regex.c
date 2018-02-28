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

#include "roxterm-regex.h"

/* Keeping this as a separate file unchanged from gnome-terminal will make it
 * easier to keep track of any upstream changes. Only include it here though,
 * it contains several macros with generic names.
 */
#include "terminal-regex.h"

#define REGEX_SSH DEFS "(?i:ssh:)" USERPASS URL_HOST PORT

ROXTerm_RegexAndType roxterm_regexes[] = {
    { REGEX_URL_AS_IS, ROXTerm_Match_FullURI },
    { REGEX_URL_HTTP, ROXTerm_Match_URINoScheme },
    { REGEX_NEWS_MAN, ROXTerm_Match_FullURI },
    { REGEX_SSH, ROXTerm_Match_SSH_Host },
    { REGEX_EMAIL, ROXTerm_Match_SSH_Host },
    { REGEX_URL_VOIP, ROXTerm_Match_VOIP },
    { REGEX_URL_FILE, ROXTerm_Match_File },
    { NULL, ROXTerm_Match_Invalid }
};

/* vi:set sw=4 ts=4 noet cindent cino= */
