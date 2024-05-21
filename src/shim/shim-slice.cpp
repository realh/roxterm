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

#include <algorithm>
#include <cstring>
#include <climits>

#include "../send-to-pipe.h"
#include "shim-log.h"
#include "shim-slice.h"

namespace unistd {
#include <unistd.h>
}

namespace shim {

bool ShimSlice::read_from_fd(int fd)
{
    auto addr = buf->address(offset);
    ssize_t nread = std::min(length, PIPE_BUF);
    memset(addr, 0, nread);
    shimlog << "read_from_fd: attempting to read up to " << nread
            << " bytes from fd " << fd << " to offset " << offset
            << ", addr " << (void *) addr << endlog;
    nread = unistd::read(fd, addr, nread) > 0;
    shimlog << "read " << nread << " bytes " << (char *) addr << endlog;
    if (nread > 0)
    {
        length = nread;
        buf->bytes_added(length);
        return true;
    }
    length = 0;
    return false;
}

bool ShimSlice::write_to_fd(int fd) const
{
    return blocking_write(fd, buf->address(offset), length) > 0;
}

}
