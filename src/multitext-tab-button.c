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

#include "multitext-tab-button.h"

typedef struct 
{
    GtkImage *image;
    GdkPixbuf *pixbuf;
    gboolean showing_pixbuf;
} MultitextTabButtonPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(MultitextTabButton, multitext_tab_button,
        GTK_TYPE_BUTTON);

inline static gboolean multitext_tab_button_active_state(GtkStateFlags flags)
{
    return (flags & (GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE)) != 0;
}

inline static void
multitext_tab_button_show_close_icon_priv(MultitextTabButtonPrivate *priv)
{
    gtk_image_set_from_icon_name(priv->image, "window-close-symbolic",
            GTK_ICON_SIZE_MENU);
}

static void multitext_tab_button_state_flags_changed(GtkWidget *widget,
        GtkStateFlags flags)
{
    MultitextTabButton *self = MULTITEXT_TAB_BUTTON(widget);
    MultitextTabButtonPrivate *priv
        = multitext_tab_button_get_instance_private(self);
    gboolean show = !multitext_tab_button_active_state(flags);
    if (show && !priv->showing_pixbuf && priv->pixbuf)
        gtk_image_set_from_pixbuf(priv->image, priv->pixbuf);
    else
        multitext_tab_button_show_close_icon_priv(priv);
    priv->showing_pixbuf = show;
}

static void
multitext_tab_button_set_size (MultitextTabButton *self)
{
    int x, y;
    GtkWidget *w = GTK_WIDGET (self);

    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &x, &y);
    gtk_widget_set_size_request (w, x + 2, y + 2);
}

static void
multitext_tab_button_style_updated (GtkWidget *self)
{
    multitext_tab_button_set_size (MULTITEXT_TAB_BUTTON (self));
    GTK_WIDGET_CLASS (multitext_tab_button_parent_class)->style_updated (self);
}

static void
multitext_tab_button_constructed(GObject *obj)
{
    g_object_set(obj, "relief", GTK_RELIEF_NONE, "focus-on-click", FALSE, NULL);
    G_OBJECT_CLASS(multitext_tab_button_parent_class)->constructed(obj);
    MultitextTabButton *self = MULTITEXT_TAB_BUTTON(obj);
    MultitextTabButtonPrivate *priv
        = multitext_tab_button_get_instance_private(self);
    GtkWidget *image = gtk_image_new ();
    priv->image = GTK_IMAGE (image);
    multitext_tab_button_show_close_icon_priv(priv);
    gtk_widget_show (image);
    gtk_container_add (GTK_CONTAINER (self), image);
}

static void
multitext_tab_button_dispose(GObject *obj)
{
    MultitextTabButton *self = MULTITEXT_TAB_BUTTON(obj);
    MultitextTabButtonPrivate *priv
        = multitext_tab_button_get_instance_private(self);
    g_clear_object(&priv->pixbuf);
    G_OBJECT_CLASS(multitext_tab_button_parent_class)->dispose(obj);
}

static void
multitext_tab_button_class_init(MultitextTabButtonClass *klass)
{
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    oklass->dispose = multitext_tab_button_dispose;
    oklass->constructed = multitext_tab_button_constructed;
    GtkWidgetClass *wklass = GTK_WIDGET_CLASS(klass);
    wklass->style_updated = multitext_tab_button_style_updated;
}

static void
multitext_tab_button_init(MultitextTabButton *self)
{
    GtkWidget *w = GTK_WIDGET(self);
    gtk_widget_set_name (w, "multitext-close-button");
}

GtkWidget *
multitext_tab_button_new(void)
{
    MultitextTabButton *self = (MultitextTabButton *)
            g_object_new (MULTITEXT_TYPE_TAB_BUTTON, NULL);
    return GTK_WIDGET (self);
}

void
multitext_tab_button_set_pixbuf(MultitextTabButton *self, GdkPixbuf *pixbuf)
{
    MultitextTabButtonPrivate *priv
        = multitext_tab_button_get_instance_private(self);
    if (priv->pixbuf == pixbuf)
        return;
    if (priv->pixbuf)
        g_object_unref(priv->pixbuf);
    priv->pixbuf = pixbuf;
    g_object_ref(pixbuf);
    priv->showing_pixbuf = FALSE;
    GtkWidget *w = GTK_WIDGET(self);
    multitext_tab_button_state_flags_changed(w, gtk_widget_get_state_flags(w));
}

void multitext_tab_button_show_close_icon(MultitextTabButton *self)
{
    MultitextTabButtonPrivate *priv
        = multitext_tab_button_get_instance_private(self);
    multitext_tab_button_show_close_icon_priv(priv);
    priv->showing_pixbuf = FALSE;
    g_clear_object(&priv->pixbuf);
}
