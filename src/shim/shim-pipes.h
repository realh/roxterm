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

}
