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

// extern "C" {
// #include "config.h"
// }

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

using namespace std;

constexpr int BufferSize            = 16384;
constexpr int BufferMinChunkSize    = 256;
constexpr int MaxBufferSizeForTopUp = BufferSize - BufferMinChunkSize;
constexpr int MaxSliceQueueLength   = 16;

constexpr int EscCode = 0x1b;
constexpr int OscCode = 0x9d;
constexpr int OscEsc  = ']';
constexpr int StCode  = 0x9c;
constexpr int StEsc   = '\\';
constexpr int BelCode = 7;

using ShimArray = array<uint8_t, BufferSize>;

class ShimBuffer {
private:
    ShimArray buf{ array<uint8_t, BufferSize>() };
    int n_filled{0};
public:
    // Gets an empty slice if the buffer isn't full
    optional<class ShimSlice> next_fillable_slice();

    // Marks that the last slice was filled with n bytes, which need not be
    // the slice's capacity.
    bool filled(int n);

    bool is_full()
    {
        return n_filled >= MaxBufferSizeForTopUp;
    }

    uint8_t operator[](int index)
    {
        return buf[index];
    }
};

class ShimSlice {
private:
    mutable shared_ptr<ShimBuffer> buf;
    int offset;
    int length;
public:
    ShimSlice(shared_ptr<ShimBuffer> buf, int offset, int length) :
        buf(buf), offset(offset), length(length) {}
    
    ShimSlice(const ShimSlice &) = default;

    ShimSlice(ShimSlice &&) = default;

    uint8_t operator[](int index) const
    {
        return (*buf)[index];
    }
};