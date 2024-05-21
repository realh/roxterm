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

#include <condition_variable>
#include <mutex>
#include <queue>

#include "shim-constants.h"

namespace shim {

// A queue item T must have a size() method. If it returns 0 it's allowed to
// override size_limit.
template<class T, int size_limit> class ShimQueue {
private:
    std::queue<T> q{};
    std::mutex mut{};
    std::condition_variable cond{};
public:
    void push(const T &item)
    {
        std::unique_lock lock{mut};
        bool was_empty = !q.size();
        int item_size = item.size();
        while (item_size && q.size() >= size_limit)
            cond.wait(lock);
        q.push(item);
        if (was_empty)
            cond.notify_one();
    }

    void push(T &&item)
    {
        std::unique_lock lock{mut};
        bool was_empty = !q.size();
        int item_size = item.size();
        while (item_size && q.size() >= size_limit)
            cond.wait(lock);
        q.emplace(item);
        if (was_empty)
            cond.notify_one();
    }

    T pop()
    {
        std::unique_lock lock{mut};
        while (q.size() == 0)
            cond.wait(lock);
        auto item = q.front();
        q.pop();
        if (q.size() == MaxSliceQueueLength - 1)
            cond.notify_one();
        return std::move(item);
    }
};

}
