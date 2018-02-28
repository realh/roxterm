#ifndef ROXTERM_REGEX_H
#define ROXTERM_REGEX_H
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


#ifndef DEFNS_H
#include "defns.h"
#endif

typedef enum {
    ROXTerm_Match_Invalid,
    ROXTerm_Match_FullURI,
    ROXTerm_Match_URINoScheme,
    ROXTerm_Match_SSH_Host,
    ROXTerm_Match_MailTo,
    ROXTerm_Match_VOIP,
    ROXTerm_Match_File,
} ROXTerm_MatchType;

typedef struct {
    const char *regex;
    ROXTerm_MatchType match_type;
} ROXTerm_RegexAndType;

extern ROXTerm_RegexAndType roxterm_regexes[];

#endif /* ROXTERM_REGEX_H */

/* vi:set sw=4 ts=4 noet cindent cino= */
