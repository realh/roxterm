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

#include "shim-log.h"
#include "shim-stream-processor.h"

namespace shim {

void ShimStreamProcessor::get_slices_from_input()
{
    while (true)
    {
        auto buf = std::make_shared<ShimBuffer>();
        while (!buf->is_full())
        {
            auto slice = ShimSlice(buf);
            if (slice.size() == 0)
                break;
            bool ok = slice.read_from_fd(input_fd);
            // If ok is false the slice has length 0 so pushing it will cause
            // the reader to stop.
            unprocessed_slices.push(slice);
            if (!ok) return;
        }
    }
}

void ShimStreamProcessor::process_slices()
{
    bool ok = true;
    while (ok)
    {
        auto slice = unprocessed_slices.pop();
        ok = slice.size() != 0;
        processed_slices.push(slice);
    }
}

void ShimStreamProcessor::write_slices_to_output()
{
    bool ok = true;
    while (ok)
    {
        auto slice = processed_slices.pop();
        ok = slice.size() != 0;
        if (ok)
        {
            ok = slice.write_to_fd(output_fd);
            if (!ok)
            {
                shimlog << "Failed to write slice to output: " <<
                    strerror(errno) << std::endl;
            }
        }
    }
}

void ShimStreamProcessor::join()
{
    input_thread.join();
    process_thread.join();
    output_thread.join();
}

}
