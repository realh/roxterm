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
#pragma once

#include <memory>
#include <string>

#include "shim-buffer.h"
#include "shim-queue.h"

namespace shim {

class ShimSlice {
private:
    mutable std::shared_ptr<ShimBuffer> buf;
    int offset;
    int length;
public:
    ShimSlice(std::shared_ptr<ShimBuffer> buf, int offset, int length) :
        buf(buf), offset(offset), length(length) {}

    // This creates a slice from buf's current filled position to its end
    ShimSlice(std::shared_ptr<ShimBuffer> buf) :
        buf(buf), offset(buf->get_filled()), length(BufferSize - offset) {}
    
    ShimSlice(const ShimSlice &) = default;

    ShimSlice(ShimSlice &&) = default;

    int size() const
    {
        return length;
    }

    uint8_t operator[](int index) const
    {
        return (*buf)[index];
    }

    // This reads as many bytes as it can and automatically calls
    // buf->bytes_added.
    bool read_from_fd(int fd);

    // Blocks until all bytes have been written.
    bool write_to_fd(int fd) const;

    ShimSlice sub_slice(int offset, int length) const
    {
        return ShimSlice(buf, this->offset + offset, length);
    }

    std::string content_as_string() const
    {
        const char *begin = (const char *) buf->address(offset);
        const char *end = begin + length;
        return std::string(begin, end);
    }
};

using ShimSliceQueue = ShimQueue<ShimSlice, MaxSliceQueueLength>;

}
