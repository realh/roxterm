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

#include "defns.h"
#include "roxterm.h"

/*
 * This is rather like vte_terminal_spawn_async but uses a shim
 * (passed in argv[0]) to handle OSC 52 clipboard writes. argv gets
 * (shallow) freed by this If free_argv2 is true, it also
 * frees argv[2], which should be "-sh" where 'sh' is the shell.
 * env is not freed, that should be done in the callback.
 */
void roxterm_spawn_child(ROXTermData *roxterm, VteTerminal *vte,
                         const char *working_directory,
                         char **argv, char **envv,
                         VteTerminalSpawnAsyncCallback callback,
                         gboolean free_argv2);
