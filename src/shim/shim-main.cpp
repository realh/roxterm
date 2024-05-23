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
#include <ostream>
#include <sstream>

#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../send-to-pipe.h"

#include "shim-back-channel.h"
#include "shim-constants.h"
#include "shim-log.h"
#include "shim-stream-processor.h"

namespace shim {

void report_errno_to_shimlog_and_stderr(const char *message)
{
    std::stringstream ss;
    ss << message << ": " << std::strerror(errno);
    auto s = ss.str();
    std::cerr << s << std::endl;
    shimlog << s;
}

bool remap_fd(int orig, int target)
{
    if (orig == target)
        return true;
    if (dup2(orig, target) < 0)
    {
        std::stringstream ss;
        ss << "Unable to map pts " << orig << " to fd " << target;
        report_errno_to_shimlog_and_stderr(ss.str().c_str());
        return false;
    }
    return true;
}

int launch_child(const std::vector<char *> &args, int pts)
{
    if (!remap_fd(pts, FdStdin))
        return 1;
    if (!remap_fd(pts, FdStdout))
        return 1;
    if (!remap_fd(pts, FdStderr))
        return 1;
    if (pts > 2)
        close(pts);

    shimlog << "launch_child launching" << endlog;
    int exec_result = execvp(args[0], &args[0]);

    // This is never reached if execvp is successful
    std::stringstream ss;
    ss << "Failed to exec";
    for (auto arg : args)
    {
        if (arg)
        {
            ss << ' ' << arg;
        }
    }
    report_errno_to_shimlog_and_stderr(ss.str().c_str());
    return exec_result;
}

int run_stream_processors(pid_t pid, int back_channel_pipe, int pty_master)
{
    BackChannelProcessor bcp{back_channel_pipe};
    std::stringstream ss;
    ss << "OK " << pid;
    bcp.push_message(BackChannelMessage (ss.str()));
    shimlog << "Sent '" << ss.str() << "' to back channel" << endlog;

    // stdout and stderr are both bound to the parent's pts
    shimlog << "stdout/stderr StreamProcessor piping from " << pty_master
            << " to stdout " << endlog;
    Osc52StreamProcessor out_proc("stdout", pty_master, 1, bcp);
    out_proc.start();

    shimlog << "stdin StreamProcessor piping from stdin "
            << " to " << pty_master << endlog;
    Osc52StreamProcessor in_proc("stdin", 1, pty_master, bcp);
    out_proc.start();

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
    out_proc.join();
    shimlog << "Joined output processor thread" << endlog;
    in_proc.join();
    shimlog << "Joined input processor thread" << endlog;
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

    int pty_master = ::posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (pty_master < 0)
    {
        shim::report_errno_to_shimlog_and_stderr("Unable to open shim pty");
        return 1;
    }

    const char *slave_dev = ptsname(pty_master);
    if (!slave_dev)
    {
        shim::report_errno_to_shimlog_and_stderr(
            "Unable to get device name of shim pty slave");
        return 1;
    }
    int pts = ::open(slave_dev, O_RDWR);
    if (pts < 0)
    {
        shim::report_errno_to_shimlog_and_stderr(
            "Unable to open shim pty slave");
        return 1;
    }
    

    auto pid = fork();

    if (!pid)
    {
        close(back_channel_pipe);
        return shim::launch_child(args, pts);
    }
    else
    {
        return shim::run_stream_processors(pid, back_channel_pipe, pty_master);
    }

    return 0;
}