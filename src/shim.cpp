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

#include <cstdlib>
#include <fstream>
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
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

using std::uint8_t;

constexpr int BufferSize = 16384;
constexpr int BufferMinChunkSize = 256;
constexpr int MaxBufferSizeForTopUp = BufferSize - BufferMinChunkSize;
constexpr int MaxSliceQueueLength = 16;
constexpr int MaxBackChannelQueueLength = 8;

constexpr int FdStdin = 0;
constexpr int FdStdout = 1;
constexpr int FdStderr = 2;

constexpr int EscCode = 0x1b;
constexpr int OscCode = 0x9d;
constexpr int OscEsc  = ']';
constexpr int StCode  = 0x9c;
constexpr int StEsc   = '\\';
constexpr int BelCode = 7;

auto shimlog = std::ofstream("/home/tony/.cache/roxterm/shimlog");

/*************************************************************/

class ShimBuffer : public std::enable_shared_from_this<ShimBuffer> {
private:
    std::array<uint8_t, BufferSize> buf{};
    int n_filled{0};
    std::mutex mut{};

    ShimBuffer() = default;
public:
    // Gets an empty slice if the buffer isn't full. If it is full, the slice
    // will have zero length.
    class ShimSlice next_fillable_slice();

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

    static std::shared_ptr<ShimBuffer> make_new()
    {
        return (new ShimBuffer())->shared_from_this();
    }
};

/*************************************************************/

class ShimSlice {
private:
    mutable std::shared_ptr<ShimBuffer> buf;
    int offset;
    int length;
public:
    ShimSlice(std::shared_ptr<ShimBuffer> buf, int offset, int length) :
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

    std::string content_as_string() const
    {
        const char *begin = (const char *) buf->address(offset);
        const char *end = begin + length;
        return std::string(begin, end);
    }
};

/*************************************************************/

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

/*************************************************************/

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

using ShimSliceQueue = ShimQueue<ShimSlice, MaxSliceQueueLength>;

using BackChannelQueue =
    ShimQueue<BackChannelMessage, MaxBackChannelQueueLength>;

/*************************************************************/

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

/*************************************************************/

class ShimStreamProcessor {
private:
	int input_fd;
	int output_fd;
	ShimSliceQueue unprocessed_slices{};
	ShimSliceQueue processed_slices{};
	BackChannelProcessor &back_channel;
    std::thread input_thread;
    std::thread process_thread;
    std::thread output_thread;

    void get_slices_from_input();

    void process_slices();

    void write_slices_to_output();
public:
    ShimStreamProcessor(int input_fd, int output_fd,
        BackChannelProcessor &back_channel) :
        input_fd(input_fd),
        output_fd(output_fd),
        back_channel(back_channel),
        input_thread(&ShimStreamProcessor::get_slices_from_input, this),
        process_thread(&ShimStreamProcessor::process_slices, this),
        output_thread(&ShimStreamProcessor::write_slices_to_output, this)
    {}

    void join();
};

/*************************************************************/
/*************************************************************/

ShimSlice ShimBuffer::next_fillable_slice()
{
    std::scoped_lock lock{mut};
    if (is_full())
        return ShimSlice(shared_from_this(), 0, 0);
    return ShimSlice(shared_from_this(), n_filled, BufferSize - n_filled);
}

bool ShimBuffer::filled(int n)
{
    std::scoped_lock lock{mut};
    n_filled += n;
    return is_full();
}

/*************************************************************/

bool ShimSlice::read_from_fd(int fd)
{
    int nread = read(fd, buf->address(offset), length) > 0;
    if (nread > 0)
    {
        length = nread;
        buf->filled(length);
        return true;
    }
    length = 0;
    return false;
}

bool ShimSlice::write_to_fd(int fd) const
{
    return blocking_write(fd, buf->address(offset), length) > 0;
}

/*************************************************************/

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

/*************************************************************/

void ShimStreamProcessor::get_slices_from_input()
{
    while (true)
    {
        auto buf = ShimBuffer::make_new();
        while (!buf->is_full())
        {
            auto slice = buf->next_fillable_slice();
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
        processed_slices.push(slice);
    }
}

void ShimStreamProcessor::join()
{
    input_thread.join();
    process_thread.join();
    output_thread.join();
}

/*************************************************************/

struct Pipes {
    int parent_r;
    int parent_w;
    int child_r;
    int child_w;

    int err_code{0};

    Pipes();
    
    // Call in the parent after forking. If target_fd is stdin, parent_w is
    // remapped, child_r is closed, and the StreamProcessor will read from
    // parent_r and write to child_w. Otherwise parent_r is remapped, child_w
    // is closed, and the StreamProcessor will read from child_r and write to
    // parent_w.
    int remap_parent(int target_fd);

    // Call in the child after forking. If target_fd is stdin, child_r is
    // remapped, otherwise child_w is remapped. The other pipe ends will be
    // closed automatically if exec succeeds, due to the use of CLOEXEC.
    int remap_child(int target_fd);
};

Pipes::Pipes()
{
    if (pipe2(&parent_r, O_CLOEXEC))
    {
        err_code = errno;
        return;
    }
    if (pipe2(&child_r, O_CLOEXEC))
        err_code = errno;
}

int Pipes::remap_parent(int target_fd)
{
    if (err_code)
        return err_code;
    int *remap_fd, *close_fd;
    if (target_fd == FdStdin)
    {
        remap_fd = &parent_w;
        close_fd = &child_r;
    }
    else
    {
        remap_fd = &parent_r;
        close_fd = &child_w;
    }
    if (dup2(*remap_fd, target_fd))
    {
        err_code = errno;
        return err_code;
    }
    *remap_fd = target_fd;
    close(*close_fd);
    *close_fd = -1; 
    return 0;
}

int Pipes::remap_child(int target_fd)
{
    int *remap_fd = (target_fd == FdStdin) ? &child_r : &child_w;
    if (dup2(*remap_fd, target_fd))
    {
        err_code = errno;
        return err_code;
    }
    *remap_fd = target_fd;
    return 0;
}

int launch_child(const std::vector<char *> &args, int back_channel_pipe)
{
    //fcntl(back_channel_pipe, F_SETFD, FD_CLOEXEC);
    close(back_channel_pipe);
    int exec_result = execvp(args[0], &args[0]);

    // This is never reached if execvp is successful
    std::cerr << "Failed to exec";
    shimlog << "Failed to exec";
    for (auto arg : args)
    {
        if (arg)
        {
            std::cerr << ' ' << arg;
            shimlog << ' ' << arg;
        }
    }
    std::cerr << std::endl;
    shimlog << std::endl;
    std::cerr << std::strerror(errno) << std::endl;
    shimlog << std::strerror(errno) << std::endl;
    return exec_result;
}

int run_stream_processors(pid_t pid,
    int back_channel_pipe, Pipes &stderr_pipes)
{
    BackChannelProcessor bcp{back_channel_pipe};
    std::stringstream ss;
    ss << "OK " << pid;
    bcp.push_message(BackChannelMessage (ss.str()));
    shimlog << "Sent '" << ss.str() << "' to back channel" << std::endl;

    ShimStreamProcessor stderr_proc(stderr_pipes.child_r,
        stderr_pipes.parent_w, bcp);

    shimlog << "Started stderr stream processor, waiting for child" << std::endl;

    int exit_status;
    waitpid(pid, &exit_status, 0);
    shimlog << "Child exited" << std::endl;
    bcp.push_message(BackChannelMessage("END"));
    shimlog << "Sent 'END' to back channel" << std::endl;
    bcp.stop();
    shimlog << "Stopped back channel" << std::endl;
    bcp.join();
    shimlog << "Joined back channel thread" << std::endl;
    stderr_proc.join();
    shimlog << "Joined stderr thread" << std::endl;
    if (WIFEXITED(exit_status))
        return WEXITSTATUS(exit_status);
    else
        return 1;
}

int main(int argc, char **argv)
{
    int back_channel_pipe = std::atoi(argv[1]);

    // Copy argv from element 2 onwards to make sure the std::vector is
    // null-terminated.
    std::vector<char *> args{argv + 2, argv + argc};
    args.push_back(nullptr);

    Pipes stderr_pipes;
    if (stderr_pipes.err_code)
    {
        std::cerr << "Error opening shim pipes: " <<
            std::strerror(errno) << std::endl;
        shimlog << "Error opening shim pipes: " <<
            std::strerror(errno) << std::endl;
        return stderr_pipes.err_code;
    }

    auto pid = fork();

    if (!pid)
    {
        stderr_pipes.remap_child(FdStderr);
        shimlog << "Remapped stderr child pipes" << std::endl;
        return launch_child(args, back_channel_pipe);
    }
    else
    {
        stderr_pipes.remap_parent(FdStderr);
        shimlog << "Remapped stderr parent pipes" << std::endl;
        return run_stream_processors(pid, back_channel_pipe, stderr_pipes);
    }

    return 0;
}