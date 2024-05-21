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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../send-to-pipe.h"

#include "shim-back-channel.h"
#include "shim-constants.h"
#include "shim-log.h"
#include "shim-pipes.h"
#include "shim-stream-processor.h"

namespace shim {

int launch_child(const std::vector<char *> &args, int back_channel_pipe)
{
    //fcntl(back_channel_pipe, F_SETFD, FD_CLOEXEC);
    shimlog << "launch_child closing back_channel_pipe " <<
        back_channel_pipe << std::endl;
    close(back_channel_pipe);
    shimlog << "launch_child launching" << std::endl;
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

    shimlog << "stderr StreamProcessor piping from " << stderr_pipes.child_r <<
        " to " << stderr_pipes.parent_w << std::endl;
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

}

int main(int argc, char **argv)
{
    int back_channel_pipe = std::atoi(argv[1]);

    // Copy argv from element 2 onwards to make sure the std::vector is
    // null-terminated.
    std::vector<char *> args{argv + 2, argv + argc};
    args.push_back(nullptr);

    shim::Pipes stderr_pipes;
    if (stderr_pipes.err_code)
    {
        std::cerr << "Error opening shim pipes: " <<
            std::strerror(errno) << std::endl;
        shim::shimlog << "Error opening shim pipes: " <<
            std::strerror(errno) << std::endl;
        return stderr_pipes.err_code;
    }

    auto pid = fork();

    if (!pid)
    {
        stderr_pipes.remap_child(shim::FdStderr);
        shim::shimlog << "Remapped stderr child pipes" << std::endl;
        return shim::launch_child(args, back_channel_pipe);
    }
    else
    {
        stderr_pipes.remap_parent(shim::FdStderr);
        shim::shimlog << "Remapped stderr parent pipes" << std::endl;
        return run_stream_processors(pid, back_channel_pipe, stderr_pipes);
    }

    return 0;
}