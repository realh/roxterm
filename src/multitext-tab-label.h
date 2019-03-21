/*
    roxterm - VTE/GTK terminal emulator with tabs
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

#ifndef __MULTITEXT_TAB_LABEL_H
#define __MULTITEXT_TAB_LABEL_H

#include "multitext-tab-button.h"

#define MULTITEXT_TYPE_TAB_LABEL multitext_tab_label_get_type()
G_DECLARE_DERIVABLE_TYPE(MultitextTabLabel, multitext_tab_label,
        MULTITEXT, TAB_LABEL, GtkBox);

struct _MultitextTabLabelClass
{
    GtkEventBoxClass parent_class;
};

MultitextTabLabel *
multitext_tab_label_new(MultitextTabButton *button, gboolean single);

void
multitext_tab_label_set_text(MultitextTabLabel *self, const char *text);

const char *
multitext_tab_label_get_text(MultitextTabLabel *self);

/* Whether the tab is the only one in the window */
void
multitext_tab_label_set_single(MultitextTabLabel *self, gboolean single);

MultitextTabButton *multitext_tab_label_get_button(MultitextTabLabel *self);

#endif /* __MULTITEXT_TAB_LABEL_H */
