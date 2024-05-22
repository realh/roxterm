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

#include <memory>
#include <mutex>

#include "shim-back-channel.h"
#include "shim-slice.h"

namespace shim {

enum StreamProcessorState {
    // Copying from input to output, but checking for start of control sequence
    // of interest.
    Copying,

    // Got the start of a control sequence, but we don't know whether it's of
    // interest yet.
    PotentialMatch,

    // Processing a control sequence of interest.
    ProcessingMatchedSequence,

    // Sequence was of interest, but it's too long, so the rest of it will be
    // discarded until we get a terminator.
    Discarding,

};

class ShimStreamProcessor {
protected:
    const char *name;
	int input_fd;
	int output_fd;
	ShimSliceQueue unprocessed_slices{};
	ShimSliceQueue processed_slices{};
    std::thread *input_thread;
    std::thread *process_thread;
    std::thread *output_thread;

    void get_slices_from_input();

    void write_slices_to_output();

    std::unique_ptr<ShimSlice> current_slice{nullptr};

    virtual void process_slices();

    void join_one_thread(std::thread **p_thread);
public:
    ShimStreamProcessor(const char *name, int input_fd, int output_fd) :
        name(name),
        input_fd(input_fd),
        output_fd(output_fd)
    {}

    // We can't start from the constructor because the sub-class won't be
    // set up yet.
    void start();

    // This also calls stop.
    void join();
};

class Osc52StreamProcessor: public ShimStreamProcessor {
private:
	BackChannelProcessor &back_channel;
protected:
    virtual void process_slices();
public:
    Osc52StreamProcessor(const char *name, int input_fd, int output_fd,
        BackChannelProcessor &back_channel) :
        ShimStreamProcessor(name, input_fd, output_fd),
        back_channel(back_channel)
    {}
};

}
