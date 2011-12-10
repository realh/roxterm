#ifndef MULTITAB_LABEL_H
#define MULTITAB_LABEL_H
/*
    roxterm - VTE/GTK terminal emulator with tabs
    Copyright (C) 2004-2011 Tony Houghton <h@realh.co.uk>

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

#define MULTITAB_TYPE_LABEL \
        (multitab_label_get_type ())
#define MULTITAB_LABEL(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
        MULTITAB_TYPE_LABEL, MultitabLabel))
#define MULTITAB_IS_LABEL(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MULTITAB_TYPE_LABEL))
#define MULTITAB_LABEL_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST ((klass), \
        MULTITAB_TYPE_LABEL, MultitabLabelClass))
#define MULTITAB_IS_LABEL_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE ((klass), MULTITAB_TYPE_LABEL))
#define MULTITAB_LABEL_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), \
        MULTITAB_TYPE_LABEL, MultitabLabelClass))

typedef struct _MultitabLabel        MultitabLabel;
typedef struct _MultitabLabelClass   MultitabLabelClass;

#define MULTITAB_LABEL_USE_PARENT_SALLOC !GTK_CHECK_VERSION(3, 0, 0)

#if GTK_CHECK_VERSION(3, 0, 0)
typedef GdkRGBA MultitabColor;
#else
typedef GdkColor MultitabColor;
#endif

struct _MultitabLabel
{
    GtkEventBox parent_instance;
    MultitabColor attention_color;
    GtkLabel *label;
    gboolean attention;
    guint timeout_tag;
    gboolean single;
    gboolean fixed_width;
    GtkWidget *parent;
#if MULTITAB_LABEL_USE_PARENT_SALLOC
    gulong parent_salloc_tag;
#endif
    int *best_width;
};

struct _MultitabLabelClass
{
    GtkEventBoxClass parent_class;
#if GTK_CHECK_VERSION(3, 0, 0)
    GtkCssProvider *style_provider;
#endif
};

GType multitab_label_get_type (void);


/* best_width may not be used, depending on GTK version etc.
 * parent should be the GtkNotebook containing the tab.
 */
GtkWidget *
multitab_label_new (GtkWidget *parent, const char *text, int *best_width);

void
multitab_label_set_parent (MultitabLabel *label,
        GtkWidget *parent, int *best_width);

void
multitab_label_set_text (MultitabLabel *label, const char *text);

const char *
multitab_label_get_text (MultitabLabel *label);

void
multitab_label_draw_attention (MultitabLabel *label);

void
multitab_label_cancel_attention (MultitabLabel *label);

void
multitab_label_set_attention_color (MultitabLabel *label,
        const MultitabColor *color);

const MultitabColor *
multitab_label_get_attention_color (MultitabLabel *label);

/* Whether the tab is the only one in the window */
void
multitab_label_set_single (MultitabLabel *label, gboolean single);

/* width is in chars; -1 to disable */
void
multitab_label_set_fixed_width (MultitabLabel *label, int width);

#endif /* MULTITAB_LABEL_H */
