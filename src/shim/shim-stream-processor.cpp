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
#include <mutex>

#include "shim-log.h"
#include "shim-stream-processor.h"

namespace shim {

void ShimStreamProcessor::start()
{
    input_thread =
        new std::thread(&ShimStreamProcessor::get_slices_from_input, this);
    process_thread =
        new std::thread(&ShimStreamProcessor::process_slices, this);
    output_thread =
        new std::thread(&ShimStreamProcessor::write_slices_to_output, this);
}

void ShimStreamProcessor::get_slices_from_input()
{
    while (!is_stopping())
    {
        auto buf = std::make_shared<ShimBuffer>();
        while (!buf->is_full())
        {
            auto slice = ShimSlice(buf);
            if (slice.size() == 0)
                break;
            bool ok = slice.read_from_fd(input_fd);
            if (!ok)
            {
                shimlog << name << " stream processor read empty or error"
                        << std::endl;
            }
            // If ok is false the slice has length 0 so pushing it will cause
            // the reader to stop.
            unprocessed_slices.push(slice);
            if (!ok)
                return;
        }
    }
}

void ShimStreamProcessor::write_slices_to_output()
{
    bool ok = true;
    while (ok)
    {
        auto slice = processed_slices.pop();
        ok = slice.size() != 0;
        if (!ok)
        {
            shimlog << name << " stream processor output writer stopping"
                    << std::endl;
        }
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

void ShimStreamProcessor::join_one_thread(std::thread **p_thread)
{
    if (*p_thread)
    {
        (*p_thread)->join();
        delete *p_thread;
        *p_thread = nullptr;
    }
}

void ShimStreamProcessor::join()
{
    stop();
    join_one_thread(&input_thread);
    join_one_thread(&process_thread);
    join_one_thread(&output_thread);
}

void ShimStreamProcessor::process_slices()
{
    bool ok = true;
    while (ok)
    {
        auto slice = unprocessed_slices.pop();
        ok = slice.size() != 0;
        if (ok)
        {
            current_slice.reset(&slice);
        }
        else
        {
            shimlog << name << " stream processor slice processor stopping"
                    << std::endl;
        }
        processed_slices.push(slice);
    }
}

void Osc52StreamProcessor::process_slices()
{
    bool ok = true;
    while (ok)
    {
        auto slice = unprocessed_slices.pop();
        ok = slice.size() != 0;
        if (ok)
            current_slice.reset(&slice);
        processed_slices.push(slice);
    }
}

}
