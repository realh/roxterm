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

#include "intptrmap.h"
#include "osc52filter.h"

typedef struct {
    IntPointerMap fd_map;
    GMutex map_mutex;
    GMutex launch_mutex;
    Osc52Filter *being_launched;
} Osc52Global;

static Osc52Global osc52filter_global;

gboolean osc52filter_global_initialised = FALSE;

static Osc52Global *
osc52filter_global_init(Osc52Global *og)
{
    int_pointer_map_init(&og->fd_map);
    g_mutex_init(&og->launch_mutex);
    og->being_launched = NULL;
}

Osc52Filter *osc52filter_create(ROXTermData *roxterm)
{
    if (!osc52filter_global_initialised)
    {
        osc52filter_global_init(&osc52filter_global);
        osc52filter_global_initialised = TRUE;
    }
    Osc52Filter *og = g_new(Osc52Filter, 1);
    og->roxterm = roxterm;
    g_debug("osc52: Launching roxterm %p", roxterm);
    g_mutex_lock(&osc52filter_global.launch_mutex);
    osc52filter_global.being_launched = og;
}

void osc52filter_remove(Osc52Filter *oflt)
{
    g_mutex_lock(&osc52filter_global.map_mutex);
    int_pointer_map_remove(&osc52filter_global.fd_map, oflt->pts_fd);
    g_mutex_unlock(&osc52filter_global.map_mutex);
    g_free(oflt);
}

extern int __real_posix_openpt(int oflag);
int __wrap_posix_openpt(int oflag)
{
    Osc52Filter *og = osc52filter_global.being_launched;
    g_mutex_unlock(&osc52filter_global.launch_mutex);
    int fd = __real_posix_openpt(oflag);
    g_debug("osc52: posix_openpt opened fd %d for roxterm %p", fd, og->roxterm);
    g_mutex_lock(&osc52filter_global.map_mutex);
    int_pointer_map_insert(&osc52filter_global.fd_map, fd, og);
    g_mutex_unlock(&osc52filter_global.map_mutex);
}

 extern ssize_t __real_read(int fd, void *buf, size_t nbyte);
 ssize_t __wrap_read(int fd, void *buf, size_t nbyte)
{
    ssize_t n = __real_read(fd, buf, nbyte);
    if (n <= 0 || !int_pointer_map_contains(&osc52filter_global.fd_map, fd))
        return n;
    Osc52Filter *og =
        int_pointer_map_lookup(&osc52filter_global.fd_map, fd);
    g_return_val_if_fail(og != NULL, n);
    g_debug("osc52: read %ld bytes from fd %d for roxterm %p",
            n, fd, og->roxterm);
    return n;
}

/* vi:set sw=4 ts=4 et cindent cino= */
