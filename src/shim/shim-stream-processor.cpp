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
#include <cstdint>
#include <cstring>

#include "shim-constants.h"
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
                    << endlog;
        }
        if (ok)
        {
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
            switch (state)
            {
            case Filtering:
                ok = process_slice_in_filtering_state(slice);
                break;
            case PotentialMatch:
                ok = process_slice_in_potential_match_state(slice);
                break;
            case ProcessingMatchedSequence:
                ok = process_slice_in_processing_matched_sequence_state(slice);
                break;
            case Discarding:
                ok = process_slice_in_discard_state(slice);
                break;
            case CollectingOsc52:
                ok = process_slice_in_collect_osc52_state(slice);
                break;
            case CopyingIgnoredEsc:
                ok = process_slice_in_copying_esc_state(slice);
                break;
            }
        }
    }
}

bool ShimStreamProcessor::process_slice_in_filtering_state(ShimSlice &slice)
{
    for (int i = 0; i < slice.size(); ++i)
    {
        auto c = slice[i];
        if (c == EscCode || c == OscCode)
        {
            shimlog << "Got potential OSC 52 start " << int(c) << endlog;
            esc_start.push_back(c);
            auto pre_esc = ShimSlice(slice, 0, i);
            ++i;
            auto remainder = ShimSlice(slice, i, slice.size() - i);
            processed_slices.push(pre_esc);
            state = PotentialMatch;
            return process_slice_in_potential_match_state(remainder);
        }
    }
    processed_slices.push(slice);
    return true;
}

bool ShimStreamProcessor::process_slice_in_potential_match_state(
    ShimSlice &slice)
{
    for (int i = 0; i < slice.size(); ++i)
    {
        auto c = slice[i];
        auto ess = esc_start.size();
        uint8_t expect = 0;
        switch (ess)
        {
            case 1:
                if (esc_start[0] == OscCode)
                {
                    expect = '5';
                }
                else if (c == OscEsc)
                {
                    esc_start.push_back(c);
                    continue;
                }
                else
                {
                    shimlog << "Unexpected character " << int(c) << " after ESC"
                        << endlog;
                    return process_rest_of_slice_in_copying_esc_state(slice, i);
                }
                break;
            case 2:
                if (esc_start[1] == OscEsc)
                    expect = '5';
                break;
            default:
                switch (esc_start.back())
                {
                    case '5':
                        expect = '2';
                        break;
                    case '2':
                        expect = ';';
                        break;
                    case ';':
                        if (ess < 6)
                            expect = 'c';
                        else
                            expect = '?';
                }
        }
        if (expect)
        {
            if (c == expect || (expect == 'c' && c == 'p'))
            {
                esc_start.push_back(c);
                continue;
            }
            else if (expect == '?' && c == expect)
            {
                // If expect is '?', it means we want anything except '?',
                // because '?' means the child wants to read the clipboard
                // which we don't allow.
                state = CollectingOsc52;
                auto remainder = ShimSlice(slice, i,
                    slice.size() - i);
                return process_slice_in_collect_osc52_state(remainder);
            }
            // Again, some OSC other than 52
            return process_rest_of_slice_in_copying_esc_state(slice, i);
        }
    }
    processed_slices.push(slice);
    return true;
}

bool ShimStreamProcessor::process_rest_of_slice_in_copying_esc_state(
    ShimSlice &slice, int offset)
{
    processed_slices.push(IndependentSlice(esc_start));
    auto remainder = ShimSlice(slice, offset, slice.size() - offset);
    esc_start.resize(0);
    state = CopyingIgnoredEsc;
    return process_slice_in_copying_esc_state(remainder);
}

bool ShimStreamProcessor::process_slice_in_processing_matched_sequence_state(
    ShimSlice &slice)
{
    // TODO
    processed_slices.push(slice);
    return true;
}

bool ShimStreamProcessor::process_slice_in_collect_osc52_state(ShimSlice &slice)
{
    // TODO
    processed_slices.push(slice);
    return true;
}

bool ShimStreamProcessor::process_slice_in_discard_state(ShimSlice &slice)
{
    // TODO
    processed_slices.push(slice);
    return true;
}

bool ShimStreamProcessor::process_slice_in_copying_esc_state(ShimSlice &slice)
{
    // TODO
    processed_slices.push(slice);
    return true;
}

}