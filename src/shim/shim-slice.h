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

#include <cstdint>
#include <memory>
#include <string>

#include "shim-buffer.h"
#include "shim-queue.h"

namespace shim {

using std::uint8_t;

class SliceWriter {
public:
    virtual ~SliceWriter() = default;

    // Blocks until all bytes have been written.
    bool write_to_fd(int fd) const;

    virtual const uint8_t &operator[](int index) const;

    virtual int size() const;

    std::string content_as_string() const
    {
        const char *begin = (const char *) &(*this)[0];
        const char *end = begin + size();
        return std::string(begin, end);
    }
};

class IndependentSlice: public SliceWriter {
private:
    std::vector<uint8_t> vec;
public:
    IndependentSlice(std::vector<uint8_t> &&src_vec) : vec(src_vec) {}

    IndependentSlice(const std::vector<uint8_t> &src_vec) : vec(src_vec) {}

    const uint8_t &operator[](int index) const;

    int size() const;
};

class ShimSlice: public SliceWriter {
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

    // Makes a new slice with a subset of the other slice's data. offset is
    // relative to the other slice's offset.
    ShimSlice(const ShimSlice &slice, int offset, int length) :
        buf(slice.buf), offset(slice.offset + offset), length(length) {}

    const uint8_t &operator[](int index) const;

    int size() const;

    // This reads as many bytes as it can and automatically calls
    // buf->bytes_added.
    bool read_from_fd(int fd);

    // Changes the range of data this slice refers to. offset is relative to
    // the slice's old offset, as opposed to an absolute offset into buf.
    void change(int offset, int length)
    {
        this->offset += offset;
        this->length = length;
    }
};

using SliceWriterQueue = ShimQueue<SliceWriter, MaxSliceQueueLength>;
using ShimSliceQueue = ShimQueue<ShimSlice, MaxSliceQueueLength>;

}
