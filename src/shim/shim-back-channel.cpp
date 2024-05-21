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

#include <sstream>

#include "../send-to-pipe.h"
#include "shim-back-channel.h"
#include "shim-log.h"

namespace shim {

int BackChannelMessage::size() const
{
    auto length = prefix.size();
    for (const auto &slice: get_data())
    {
        length += slice.size();
    }
    return length;
}

std::vector<ShimSlice> BackChannelMessage::get_data() const
{
    return {};
}

bool BackChannelMessage::send_to_pipe(int fd) const
{
    auto length = size();
    shimlog << "Sending '" << std::string(*this) << "' to pipe, length " <<
        length << std::endl;
    int write_result = blocking_write(fd, &length, sizeof length);
    if (write_result <= 0)
    {
        shimlog << "Writing length failed: " << write_result << std::endl;
        return false;
    }
    if (prefix.length() &&
        (write_result = blocking_write(fd, prefix.c_str(),
            prefix.length())) <= 0)
    {
        shimlog << "Writing prefix failed: " << write_result << std::endl;
        return false;
    }
    auto data = get_data();
    shimlog << data.size() << " data elements" << std::endl;
    for (const auto &slice: data)
    {
        if (!slice.write_to_fd(fd))
        {
            shimlog << "Writing slice failed" << std::endl;
            return false;
        }
    }
    return true;
}

BackChannelMessage::operator std::string() const
{
    std::stringstream ss;
    ss << prefix;
    for (const auto &slice: get_data())
    {
        ss << slice.content_as_string();
    }
    return ss.str();
}

/*************************************************************/

void BackChannelProcessor::execute()
{
    while (true)
    {
        auto message = q.pop();
        if (message.is_empty())
        {
            shimlog << "BackChannelProcessor: empty message";
            return;
        }
        shimlog << "BackChannelProcessor sending '" <<
            std::string(message) << '\'' << std::endl;
        message.send_to_pipe(fd);
    }
}

}
