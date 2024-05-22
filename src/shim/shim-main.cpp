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
#include "shim-pipe.h"
#include "shim-stream-processor.h"

namespace shim {

int launch_child(const std::vector<char *> &args, int back_channel_pipe)
{
    //fcntl(back_channel_pipe, F_SETFD, FD_CLOEXEC);
    shimlog << "launch_child closing back_channel_pipe " <<
        back_channel_pipe << endlog;
    close(back_channel_pipe);
    shimlog << "launch_child launching" << endlog;
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
    shimlog << '\n' << std::strerror(errno) << endlog;
    return exec_result;
}

int run_stream_processors(pid_t pid,
    int back_channel_pipe, Pipe &stderr_pipe)//, Pipes &stdout_pipes)
{
    BackChannelProcessor bcp{back_channel_pipe};
    std::stringstream ss;
    ss << "OK " << pid;
    bcp.push_message(BackChannelMessage (ss.str()));
    shimlog << "Sent '" << ss.str() << "' to back channel" << endlog;

    shimlog << "stderr StreamProcessor piping from " << stderr_pipe.r <<
        " to " << stderr_pipe.w << endlog;

    Osc52StreamProcessor stderr_proc("stderr", stderr_pipe.r,
        stderr_pipe.w, bcp);

    stderr_proc.start();

    shimlog << "Started stream processors, waiting for child" << endlog;

    int exit_status;
    waitpid(pid, &exit_status, 0);
    shimlog << "Child exited" << endlog;

    bcp.push_message(BackChannelMessage("END"));
    shimlog << "Sent 'END' to back channel" << endlog;
    bcp.stop();
    shimlog << "Stopped back channel" << endlog;
    bcp.join();

    shimlog << "Joined back channel thread" << endlog;
    stderr_proc.join();
    shimlog << "Joined stderr processor thread" << endlog;
    //stdout_proc.join();
    shimlog << "Joined stream processor threads" << endlog;
    if (WIFEXITED(exit_status))
        return WEXITSTATUS(exit_status);
    else
        return 1;
}

Pipe open_and_check_pipe(const char *name)
{
    Pipe pipe;
    if (pipe.err_code)
    {
        shim::shimlog << "Error opening shim " << name << " pipe: " <<
            std::strerror(errno) << endlog;
    }
    return pipe;
}

}

int main(int argc, char **argv)
{
    int back_channel_pipe = std::atoi(argv[1]);

    // Copy argv from element 2 onwards to make sure the std::vector is
    // null-terminated.
    std::vector<char *> args{argv + 2, argv + argc};
    args.push_back(nullptr);

    shim::Pipe stderr_pipe = shim::open_and_check_pipe("stderr");
    if (stderr_pipe.err_code)
        return stderr_pipe.err_code;
    // shim::Pipes stdout_pipes = shim::open_and_check_pipes("stdout");
    // if (stdout_pipes.err_code)
    //     return stdout_pipes.err_code;
    // shim::Pipes stdin_pipes = shim::open_and_check_pipes("stdin");
    // if (stdin_pipes.err_code)
    //     return stdin_pipes.err_code;

    auto pid = fork();

    if (!pid)
    {
        stderr_pipe.remap_child(shim::FdStderr);
        //stdout_pipes.remap_child(shim::FdStdout);
        return shim::launch_child(args, back_channel_pipe);
    }
    else
    {
        stderr_pipe.remap_parent(shim::FdStderr);
        //stdout_pipes.remap_parent(shim::FdStdout);
        return run_stream_processors(pid, back_channel_pipe,
            stderr_pipe); //, stdout_pipes);
    }

    return 0;
}