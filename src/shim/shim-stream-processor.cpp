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
#include "shim-slice.h"
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
    bool is_stdin = !strcmp(name, "stdin");
    bool ok = true;
    while (ok)
    {
        // shimlog << name << " stream processor making a new buffer"
        //                 << endlog;
        auto buf = std::make_shared<ShimBuffer>();
        while (!buf->is_full())
        {
            auto slice = ShimSlice(buf);
            if (slice.size() == 0)
            {
                shimlog << name << " processor got empty slice from buf"
                        << endlog;
                break;
            }
            // else
            // {
            //     shimlog << name << " processor reading up to " << slice.size()
            //             << " bytes from input fd" << endlog;
            // }

            ok = slice.read_from_fd(input_fd);
            if (!ok)
            {
                shimlog << name << " processor read EOF or error"
                        << endlog;
            }
            else if (is_stdin)
            {
                shimlog << name << " processor read " << slice.size()
                        << " bytes from fd\n****\n"
                        << slice.content_as_string()
                        << "\n****" << endlog;
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
    bool is_stdin = !strcmp(name, "stdin");
    bool ok = true;
    while (ok)
    {
        auto slice = processed_slices.pop();
        ok = slice.size() != 0;
        if (!ok)
        {
            shimlog << name << " stream processor output writer stopping"
                    << endlog;
        }
        if (ok)
        {
            if (is_stdin)
            {
                shimlog << name << " processor writing " << slice.size()
                        << " bytes to fd\n****\n"
                        << slice.content_as_string()
                        << "\n****" << endlog;
            }
            ok = slice.write_to_fd(output_fd);
            if (!ok)
            {
                shimlog << "Failed to write slice to output: " <<
                    strerror(errno) << endlog;
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
    join_one_thread(&input_thread);
    shimlog << name << " processor joined input thread" << endlog;
    join_one_thread(&process_thread);
    shimlog << name << " processor joined process thread" << endlog;
    join_one_thread(&output_thread);
    shimlog << name << " processor joined output thread" << endlog;
}

void ShimStreamProcessor::process_slices()
{
    bool ok = true;
    while (ok)
    {
        auto slice = unprocessed_slices.pop();
        ok = slice.size() != 0;
        if (!ok)
        {
            shimlog << name << " stream processor slice processor stopping"
                    << endlog;
        }
        else
        {
            shimlog << name << " processing\n****" << slice.content_as_string()
                << "\n****" << endlog;
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
        // if (ok)
        //     current_slice.reset(new ShimSlice(slice));
        processed_slices.push(slice);
    }
}

}
