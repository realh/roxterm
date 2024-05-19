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

#include <string.h>
#include <unistd.h>

#include "send-to-pipe.h"

int blocking_write(int fd, const void *data, uint32_t length)
{
    int nwritten = 0;
    while (nwritten < (ssize_t) length)
    {
        ssize_t n = write(fd, data + nwritten, length - nwritten);
        if (n <= 0)
            return (int) n;
        else
            nwritten += n;
    }
    return nwritten;
}

int blocking_read(int fd, void *data, uint32_t length)
{
    int nread = 0;
    while (nread < (ssize_t) length)
    {
        ssize_t n = read(fd, data + nread, length - nread);
        if (n <= 0)
            return (int) n;
        else
            nread += n;
    }
    return nread;
}

// Sends the data length encoded as uint32_t immediately before the data
int send_to_pipe(int fd, const char *data, uint32_t length)
{
    if (length < 0) length = strlen(data) + 1;
    int nwritten = blocking_write(fd, &length, sizeof length);
    if (nwritten <= 0)
        return nwritten;
    return blocking_write(fd, data, length);
}
