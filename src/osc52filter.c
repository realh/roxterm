/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2024 Tony Houghton <h@realh.co.uk>

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

#include <dlfcn.h>

#include "glib.h"
#include "intptrmap.h"
#include "roxterm.h"
#include "vte/vte.h"
#include "osc52filter.h"

struct Osc52Filter {
    ROXTermData *roxterm;
    int pts_fd;
    size_t max_buflen;
};

static inline void osc52filter_free(Osc52Filter *oflt)
{
    g_free(oflt);
}

typedef struct {
    IntPointerMap fd_map;
} Osc52Global;

static Osc52Global osc52filter_global;

gboolean osc52filter_global_initialised = FALSE;

static Osc52Global *
osc52filter_global_init(Osc52Global *og)
{
    int_pointer_map_init(&og->fd_map);
    return og;
}

static inline void osc52filter_ensure_global_init()
{
    if (!osc52filter_global_initialised)
    {
        osc52filter_global_init(&osc52filter_global);
        osc52filter_global_initialised = TRUE;
    }
}

Osc52Filter *osc52filter_create(ROXTermData *roxterm, size_t buflen)
{
    VteTerminal *vte = roxterm_get_vte_terminal(roxterm);
    VtePty *pty = vte ? vte_terminal_get_pty(vte) : NULL;
    int fd = pty ? vte_pty_get_fd(pty) : -1;
    if (fd <= 0)
    {
        g_debug("Pty not available yet for roxterm %p", roxterm);
        return NULL;
    }
    osc52filter_ensure_global_init();
    Osc52Filter *oflt = g_new(Osc52Filter, 1);
    oflt->roxterm = roxterm;
    oflt->pts_fd = fd;
    oflt->max_buflen = buflen;
    int_pointer_map_insert(&osc52filter_global.fd_map, fd, oflt);
    g_debug("osc52: Launching roxterm %p with pty fd %d", roxterm, fd);
    return oflt;
}

void osc52filter_set_buffer_size(Osc52Filter *oflt, size_t buflen)
{
    oflt->max_buflen = buflen;
    // TODO: If the filter already contains data longer than this,
    // discard it.
}

void osc52filter_remove(Osc52Filter *oflt)
{
    int_pointer_map_remove(&osc52filter_global.fd_map, oflt->pts_fd);
    osc52filter_free(oflt);
}

// This overrides the system read. When it's called on an fd in the map of
// pts fds it pushes a chunk containing a copy of the data read.
ssize_t read(int fd, void *buf, size_t nbyte)
{
    static ssize_t (*real_read)(int, void *, size_t) = NULL;
    if (!real_read) {
        real_read = dlsym(RTLD_NEXT, "read");
    }
    osc52filter_ensure_global_init();
    ssize_t n = real_read(fd, buf, nbyte);
    if (n <= 0 || !int_pointer_map_contains(&osc52filter_global.fd_map, fd))
        return n;
    Osc52Filter *oflt =
        int_pointer_map_lookup(&osc52filter_global.fd_map, fd);
    g_return_val_if_fail(oflt != NULL, n);
    return n;
}

/* vi:set sw=4 ts=4 et cindent cino= */
