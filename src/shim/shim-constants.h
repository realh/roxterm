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

namespace shim {

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

};