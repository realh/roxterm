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

#include <cerrno>

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "shim-constants.h"
#include "shim-log.h"
#include "shim-pipe.h"

namespace shim {

Pipe::Pipe()
{
    if (pipe2(&r, O_CLOEXEC))
    {
        err_code = errno;
        return;
    }
    shimlog << "Created Pipe: r " << r << " w " << w << endlog;
}

int Pipe::remap_parent(int target_fd)
{
    if (err_code)
        return err_code;
    int *replace_fd;
    const char *replace_name;
    if (target_fd == FdStdin)
    {
        replace_fd = &r;
        replace_name = " r ";
    }
    else
    {
        replace_fd = &w;
        replace_name = " w ";
    }
    shimlog << "remap_parent replacing " << replace_name << *replace_fd <<
        " with " << target_fd << endlog;
    close(*replace_fd);
    *replace_fd = target_fd;
    return 0;
}

int Pipe::remap_child(int target_fd)
{
    if (err_code)
        return err_code;
    int *close_fd, *remap_fd;
    const char *close_name, *remap_name;
    if (target_fd == FdStdin)
    {
        close_fd = &w;
        close_name = " w ";
        remap_fd = &r;
        remap_name = " r ";
    }
    else
    {
        close_fd = &r;
        close_name = " r ";
        remap_fd = &w;
        remap_name = " w ";
    }
    shimlog << "remap_child closing " << close_name << *close_fd << endlog;
    shimlog << "remap_child remapping " << remap_name << *remap_fd
        << " to " << target_fd << endlog;
    close(*close_fd);
    *close_fd = -1;
    int new_fd = dup2(*remap_fd, target_fd);
    if (new_fd != target_fd)
    {
        err_code = errno;
        shimlog << "remap_child dup2 failed: result " << new_fd
            << ", error " << strerror(err_code) << endlog;
        if (err_code == 0)
            err_code = EINVAL;
    }
    return err_code;
}

}