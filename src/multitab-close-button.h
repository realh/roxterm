#ifndef MULTITAB_CLOSE_BUTTON_H
#define MULTITAB_CLOSE_BUTTON_H
/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2015 Tony Houghton <h@realh.co.uk>

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


#ifndef DEFNS_H
#include "defns.h"
#endif

#include <glib-object.h>

#define MULTITAB_TYPE_CLOSE_BUTTON \
        (multitab_close_button_get_type ())
#define MULTITAB_CLOSE_BUTTON(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        MULTITAB_TYPE_CLOSE_BUTTON, MultitabCloseButton))
#define MULTITAB_IS_CLOSE_BUTTON(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MULTITAB_TYPE_CLOSE_BUTTON))
#define MULTITAB_CLOSE_BUTTON_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST ((klass), \
        MULTITAB_TYPE_CLOSE_BUTTON, MultitabCloseButtonClass))
#define MULTITAB_IS_CLOSE_BUTTON_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), MULTITAB_TYPE_CLOSE_BUTTON))
#define MULTITAB_CLOSE_BUTTON_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), \
        MULTITAB_TYPE_CLOSE_BUTTON, MultitabCloseButtonClass))

typedef struct _MultitabCloseButton        MultitabCloseButton;
typedef struct _MultitabCloseButtonClass   MultitabCloseButtonClass;

struct _MultitabCloseButton
{
    GtkButton parent_instance;
    GtkImage *image;
};

struct _MultitabCloseButtonClass
{
    GtkButtonClass parent_class;
};

GType multitab_close_button_get_type (void);


/* NULL image_name for default GTK_STOCK_CLOSE */
GtkWidget *
multitab_close_button_new(const char *image_name);

void
multitab_close_button_set_image(MultitabCloseButton *btn,
        const char *image_name);


#endif /* MULTITAB_CLOSE_BUTTON_H */
