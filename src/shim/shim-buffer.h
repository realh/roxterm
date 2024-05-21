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

#include <array>
#include <cstdint>

#include "shim-constants.h"

namespace shim {

class ShimBuffer {
private:
    std::array<std::uint8_t, BufferSize> buf{};
    int n_filled{0};
public:
    // Gets the number of bytes filled in the buffer so far.
    int get_filled() const
    {
        return n_filled;
    }

    // Marks that the last slice was filled with n bytes, which need not be
    // the slice's capacity. Returns true if the buffer is now full.
    bool bytes_added(int n)
    {
        n_filled += n;
        return is_full();
    }

    bool is_full() const
    {
        return n_filled >= MaxBufferSizeForTopUp;
    }

    uint8_t operator[](int index) const
    {
        return buf[index];
    }

    uint8_t *address(int offset = 0)
    {
        return &buf[offset];
    }
};

}
