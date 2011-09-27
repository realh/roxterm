#ifndef DEFNS_H
#define DEFNS_H
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


/* Macros, declarations and includes that are used by most other files.
 * Always include this first.
 */

#include "rtconfig.h"

#include <gtk/gtk.h>

#if ENABLE_NLS
#include <libintl.h>
#define _(s) ((const char *) gettext(s))
#else
#define _(s) s
#define gettext(s) s
#endif
#define N_(s) s

/*
#include "gettext.h"
#ifndef _
#define _(String) gettext (String)
#endif
#ifndef N_
#define N_(String) gettext_noop (String)
#endif
*/

#define ACCEL_PATH "<roxterm>"

#define STR_EMPTY(s) ((s) ? (s) : "")

/*
#define UNREF_LOG(a) g_debug("Calling ` %s ' from %s", #a, __func__); \
        a ; \
        g_debug("unref complete");
*/
#define UNREF_LOG(a) a

#define ROXTERM_ARG_ERROR g_quark_from_static_string("roxterm-arg-error-quark")

enum {
    ROXTermArgError
};

#define ROXTERM_LEAF_DIR "roxterm.sourceforge.net"

#ifndef HAVE_GTK_WIDGET_GET_REALIZED
#define gtk_widget_get_realized GTK_WIDGET_REALIZED
#endif

#ifndef HAVE_GTK_WIDGET_GET_MAPPED
#define gtk_widget_get_mapped GTK_WIDGET_MAPPED
#endif

/* Whether to try to prevent tabs from being too wide to unmaximize correctly.
 * <http://mail.gnome.org/archives/gtk-devel-list/2011-September/msg00214.html
 */
#define MULTITAB_LABEL_GTK3_SIZE_KLUDGE GTK_CHECK_VERSION(3, 0, 0)

#endif /* DEFNS_H */

/* vi:set sw=4 ts=4 et cindent cino= */
