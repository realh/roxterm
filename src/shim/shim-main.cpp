#include "../send-to-pipe.h"
#include "shim-log.h"
#include "shim-pipe.h"
#include "shim-stream-processor.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <thread>
#include <unistd.h>

namespace shim {

void pipe_reader(int fd)
{
    bool ok = true;
    char buf[1024];
    while (ok)
    {
        int nread = read(fd, buf, 1023);
        if (nread <= 0)
        {
            shimlog << "read returned " << nread << ", error "
                << strerror(errno) << endlog;
            ok = false;
        }
        else
        {
            buf[nread] = 0;
            shimlog << "read " << nread << " bytes: " << buf << endlog;
        }
    }
}

void send_test_message_to_pipe(int fd)
{
    const char *test_msg = "This is a test message.\n" 
        "Why are we reading garbage from the pipe?\n";
    auto l = strlen(test_msg);
    int n = blocking_write(fd, test_msg, l);
    if (n != (int) l)
    {
        std::cerr << "Result of sending test message is " << n
            << ", should be " << l << std::endl;
    }
}

void shim_main()
{
    shim::Pipe pipe;

    ShimStreamProcessor sproc{"stdin", pipe.r, 1};
    sproc.start();
    
    std::cerr << "Sending test message" << std::endl;
    send_test_message_to_pipe(pipe.w);
    std::cerr << "OK, napping" << std::endl;
    sleep(2);
    std::cerr << "OK, closing pipe" << std::endl;
    close(pipe.w);
    std::cerr << "Waiting for thread" << std::endl;
    sproc.join();
}

}

int main()
{
    shim::shim_main();
    return 0;
}