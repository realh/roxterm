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

#include <fcntl.h>
#include <unistd.h>

#include "shim-constants.h"
#include "shim-log.h"
#include "shim-pipes.h"

namespace shim {

Pipes::Pipes()
{
    if (pipe2(&parent_r, O_CLOEXEC))
    {
        err_code = errno;
        return;
    }
    if (pipe2(&child_r, O_CLOEXEC))
    {
        err_code = errno;
        return;
    }
    shimlog << "Created Pipes: parent_r " << parent_r <<
        ", parent_w " << parent_w <<
        ", child_r " << child_r <<
        ", child_w " << child_w << std::endl;
}

int Pipes::remap_parent(int target_fd)
{
    if (err_code)
        return err_code;
    int *remap_fd, *close_fd;
    if (target_fd == FdStdin)
    {
        remap_fd = &parent_w;
        close_fd = &child_r;
        shimlog << "remap_parent remapping parent_w " << parent_w <<
            " to " << target_fd << ", closing child_r " << child_r << std::endl;
    }
    else
    {
        remap_fd = &parent_r;
        close_fd = &child_w;
        shimlog << "remap_parent remapping parent_r " << parent_r <<
            " to " << target_fd << ", closing child_w " << child_w << std::endl;
    }
    if (dup2(*remap_fd, target_fd))
    {
        err_code = errno;
        return err_code;
    }
    *remap_fd = target_fd;
    shimlog << "remap_parent closing " << *close_fd << std::endl;
    close(*close_fd);
    *close_fd = -1; 
    return 0;
}

int Pipes::remap_child(int target_fd)
{
    int *remap_fd = (target_fd == FdStdin) ? &child_r : &child_w;
    auto remapped_name =
        (target_fd == FdStdin) ? "child_r " : "child_w ";
    shimlog << "remap_child remapping " << remapped_name << *remap_fd <<
        " to " << target_fd << std::endl;
    if (dup2(*remap_fd, target_fd))
    {
        err_code = errno;
        return err_code;
    }
    *remap_fd = target_fd;
    return 0;
}

}