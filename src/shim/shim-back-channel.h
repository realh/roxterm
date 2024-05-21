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

#include <string>
#include <thread>
#include <vector>

#include "shim-queue.h"
#include "shim-slice.h"

namespace shim {

class BackChannelMessage {
private:
    std::string prefix;
protected:
    virtual std::vector<ShimSlice> get_data() const;
public:
    BackChannelMessage(const std::string &prefix) : prefix(prefix) {}

    BackChannelMessage(std::string &&prefix) : prefix(prefix) {}

    virtual ~BackChannelMessage() = default;

    bool send_to_pipe(int fd) const;

    // Excludes the extra 4 bytes representing this size that are sent up the
    // pipe.
    int size() const;

    bool is_empty() const
    {
        return size() == 0;
    }

    operator std::string() const;
};

using BackChannelQueue =
    ShimQueue<BackChannelMessage, MaxBackChannelQueueLength>;

class BackChannelProcessor {
private:
    int fd;
    BackChannelQueue q{};
    std::mutex mut{};
    std::condition_variable cond{};
    std::thread thread;

    void execute();
public:
    BackChannelProcessor(int fd) :
        fd(fd), thread(&BackChannelProcessor::execute, this)
    {}

    void push_message(const BackChannelMessage &message)
    {
        q.push(message);
    }

    void push_message(BackChannelMessage &&message)
    {
        q.push(message);
    }

    void stop()
    {
        q.push(BackChannelMessage(""));
    }

    void join()
    {
        thread.join();
    }
};

}
