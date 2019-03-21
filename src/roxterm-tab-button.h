/*
    roxterm4 - VTE/GTK terminal emulator with tabs
    Copyright (C) 2019 Tony Houghton <h@realh.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __ROXTERM_TAB_BUTTON_H
#define __ROXTERM_TAB_BUTTON_H

#include "multitext-tab-button.h"

/**
 * RoxtermTabState:
 *
 * The state of the tab, reflected in coloured circles replacing the close icon
 */
typedef enum {
    ROXTERM_TAB_STATE_DEFAULT = 0,  // Close icon
    ROXTERM_TAB_STATE_BUSY = 1,     // Red
    ROXTERM_TAB_STATE_OUTPUT = 2,   // Blue
    ROXTERM_TAB_STATE_EXITED = 4,   // Purple
    ROXTERM_TAB_STATE_BELL = 8,     // Amber
} RoxtermTabState;

#define ROXTERM_TYPE_TAB_BUTTON roxterm_tab_button_get_type()
G_DECLARE_FINAL_TYPE(RoxtermTabButton, roxterm_tab_button,
        ROXTERM, TAB_BUTTON, MultitextTabButton);

struct _RoxtermTabButtonClass
{
    MultitextTabButtonClass parent_class;
};

GtkWidget *
roxterm_tab_button_new(void);

void
roxterm_tab_button_set_state(RoxtermTabButton *btn, RoxtermTabState bits);

void
roxterm_tab_button_clear_state(RoxtermTabButton *btn, RoxtermTabState bits);

#endif /* __ROXTERM_TAB_BUTTON_H */
