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

#include <stdint.h>

/* Writes data to a pipe (or any file descriptor), blocking until all the
 * data is sent. The result is length on success, 0 for EOF, < 0 for error
 * (result of write()).
 */
int blocking_write(int fd, const void *data, uint32_t length);

/* Read counterpart to blocking_write. */
int blocking_read(int fd, void *data, uint32_t length);

/* As blocking_write, but the data sent to the pipe is preceded by length encoded
 * in binary. If length < 0, the length of the string, including terminator,
 * is used. The result on success excludes the extra 4 bytes used * to encode it.
 */
int send_to_pipe(int fd, const char *data, uint32_t length);
