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

extern "C" {
#include "send-to-pipe.h"
}

#include <array>
#include <cerrno>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

extern "C" {
#include <unistd.h>
}

using namespace std;

constexpr int BufferSize = 16384;
constexpr int BufferMinChunkSize = 256;
constexpr int MaxBufferSizeForTopUp = BufferSize - BufferMinChunkSize;
constexpr int MaxSliceQueueLength = 16;
constexpr int MaxBackChannelQueueLength = 8;

constexpr int EscCode = 0x1b;
constexpr int OscCode = 0x9d;
constexpr int OscEsc  = ']';
constexpr int StCode  = 0x9c;
constexpr int StEsc   = '\\';
constexpr int BelCode = 7;

/*************************************************************/

class ShimBuffer : public enable_shared_from_this<ShimBuffer> {
private:
    array<uint8_t, BufferSize> buf{};
    int n_filled{0};
    mutex mut{};
public:
    // Gets an empty slice if the buffer isn't full
    optional<class ShimSlice> next_fillable_slice();

    // Marks that the last slice was filled with n bytes, which need not be
    // the slice's capacity. Returns true if the buffer is now full.
    bool filled(int n);

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

/*************************************************************/

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

    int size() const
    {
        return length;
    }

    uint8_t operator[](int index) const
    {
        return (*buf)[index];
    }

    bool read_from_fd(int fd);

    bool write_to_fd(int fd) const;

    ShimSlice sub_slice(int offset, int length) const
    {
        return ShimSlice(buf, this->offset + offset, length);
    }
};

/*************************************************************/

class BackChannelMessage {
private:
    string prefix;
protected:
    virtual vector<ShimSlice> get_data() const;
public:
    BackChannelMessage(const string &prefix) : prefix(prefix) {}

    BackChannelMessage(string &&prefix) : prefix(prefix) {}

    virtual ~BackChannelMessage() = default;

    bool send_to_pipe(int fd) const;

    // Excludes the extra 4 bytes representing this size that are sent up the
    // pipe.
    int total_length() const;
};

/*************************************************************/

template<class T, int size_limit> class ShimQueue {
private:
    queue<T> q{};
    mutex mut{};
    condition_variable cond{};
public:
    void push(T &&item)
    {
        unique_lock lock{mut};
        bool was_empty = !q.size();
        while (q.size() >= size_limit)
            cond.wait(lock);
        q.push(item);
        if (was_empty)
            cond.notify_one();
    }

    T pop()
    {
        unique_lock lock{mut};
        while (q.size() == 0)
            cond.wait(lock);
        auto item = q.front();
        q.pop();
        if (q.size() == MaxSliceQueueLength - 1)
            cond.notify_one();
        return item;
    }
};

using ShimSliceQueue = ShimQueue<ShimSlice, MaxSliceQueueLength>;

using BackChannelQueue =
    ShimQueue<BackChannelMessage, MaxBackChannelQueueLength>;

/*************************************************************/

class ShimStreamProcessor {
private:

};

/*************************************************************/
/*************************************************************/

optional<class ShimSlice> ShimBuffer::next_fillable_slice()
{
    scoped_lock lock{mut};
    if (is_full())
        return nullopt;
    return ShimSlice(shared_from_this(), n_filled, BufferSize - n_filled);
}

bool ShimBuffer::filled(int n)
{
    scoped_lock lock{mut};
    n_filled += n;
    return is_full();
}

/*************************************************************/

bool ShimSlice::read_from_fd(int fd)
{
    int nread = read(fd, buf->address(offset), length) > 0;
    length = nread;
    if (length > 0)
    {
        buf->filled(length);
        return true;
    }
    return false;
}

bool ShimSlice::write_to_fd(int fd) const
{
    return blocking_write(fd, buf->address(offset), length) > 0;
}

/*************************************************************/

int BackChannelMessage::total_length() const
{
    auto length = prefix.size();
    for (const auto &slice: get_data())
    {
        length += slice.size();
    }
    return length;
}

vector<ShimSlice> BackChannelMessage::get_data() const
{
    return {};
}

bool BackChannelMessage::send_to_pipe(int fd) const
{
    auto length = total_length();
    if (blocking_write(fd, &length, sizeof length) <= 0)
        return false;
    if (prefix.length() &&
        blocking_write(fd, prefix.c_str(), prefix.length()) <= 0)
    {
        return false;
    }
    for (const auto &slice: get_data())
    {
        if (!slice.write_to_fd(fd))
            return false;
    }
    return true;
}

/*************************************************************/

int main(int argc, char **argv)
{
    // pipe is argv[1]
    // Copy argv from element 2 onwards to make sure the vector is
    // null-terminated.
    vector<char *> args{argv + 2, argv + argc};
    args.push_back(nullptr);

    int exec_result = execvp(args[0], &args[0]);
    if (exec_result)
    {
        cerr << "Failed to exec";
        for (auto arg : args)
        {
            if (arg)
                cerr << ' ' << arg;
        }
        cerr << endl;
        cerr << strerror(errno) << endl;
        return exec_result;
    }

    return 0;
}