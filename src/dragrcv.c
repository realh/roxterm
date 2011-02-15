/*
    roxterm - GTK+ 2.0 terminal emulator with tabs
    Copyright (C) 2004 Tony Houghton <h@realh.co.uk>

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


#include "dragrcv.h"

#include <string.h>

/*
typedef gboolean (*DragReceiveHandler)(GtkWidget *, const char *text,
        gulong length, char const **uris, gpointer data);
*/

struct DragReceiveData {
    DragReceiveHandler handler;
    DragReceiveTabHandler tab_handler;
    gpointer data;
};

static void drag_data_received(GtkWidget *widget, GdkDragContext *context, 
        gint x, gint y, GtkSelectionData *selection_data, guint info,
        guint time, gpointer data)
{
    DragReceiveData *drd = data;
    char *str;
    GString *gstr;
    int i;
    const guint16 *char_data;
    int char_len;
    char *filename;
    char *uri_list;
    char **uris;

#if 0
    GList *tmp;

    tmp = context->targets;
    while (tmp != NULL)
    {
        GdkAtom atom = GPOINTER_TO_UINT(tmp->data);

        g_debug("Target: %s", gdk_atom_name(atom));

        tmp = tmp->next;
    }
    g_debug("info: %d", info);
#endif

    switch (info)
    {
        case ROXTERM_DRAG_TARGET_TEXT_PLAIN:
            if (selection_data->format != 8
                || selection_data->length == 0)
            {
                g_critical(_("text/plain dropped on widget had wrong "
                            "format (%d) or length (%d)"),
                            selection_data->format, selection_data->length);
                return;
            }
            /* Fall-through */
        case ROXTERM_DRAG_TARGET_STRING:
        case ROXTERM_DRAG_TARGET_UTF8_STRING:
        case ROXTERM_DRAG_TARGET_COMPOUND_TEXT:
        case ROXTERM_DRAG_TARGET_TEXT:
            str = (char *) gtk_selection_data_get_text(selection_data);
            if (str && *str)
                drd->handler(widget, str, strlen(str), drd->data);
            g_free(str);
            break;
        case ROXTERM_DRAG_TARGET_MOZ_URL:
            /* MOZ_URL is in UCS-2 but in format 8. BROKEN!
             *
             * The data contains the URL, a \n, then the
             * title of the web page.
             */
            if (selection_data->format != 8 || selection_data->length == 0
                || (selection_data->length % 2) != 0)
            {
                g_critical(_("Mozilla url dropped on widget had wrong "
                            "format (%d) or length (%d)"),
                            selection_data->format,
                            selection_data->length);
                return;
            }

            gstr = g_string_new(NULL);
            char_len = selection_data->length / 2;
            char_data = (const guint16 *) selection_data->data;
            i = 0;
            while (i < char_len)
            {
                if (char_data[i] == '\n')
                    break;
                g_string_append_unichar(gstr, (gunichar) char_data[i]);
                ++i;
            }

            filename = g_filename_from_uri(gstr->str, NULL, NULL);
            if (filename)
                drd->handler(widget, filename, strlen(filename), drd->data);
            else
                drd->handler(widget, gstr->str, gstr->len, drd->data);
            g_free(filename);
            g_string_free(gstr, TRUE);
            break;
        case ROXTERM_DRAG_TARGET_URI_LIST:
            if (selection_data->format != 8
                || selection_data->length == 0)
            {
                g_critical(_("URI list dropped on widget had wrong "
                            "format (%d) or length (%d)"),
                            selection_data->format, selection_data->length);
                return;
            }
            /* Don't use gtk_selection_data_get_text because it returns UTF-8
             * while g_filename_from_uri wants ASCII in */
            uri_list = g_strndup((const char *) selection_data->data,
                    selection_data->length);
            uris = g_strsplit(uri_list, "\r\n", 0);
            for (i = 0; uris && uris[i]; ++i)
            {
                char *old;

                old = uris[i];
                uris[i] = g_filename_from_uri(old, NULL, NULL);
                if (uris[i] == NULL)
                    uris[i] = old;
                else
                    g_free(old);
                if ((g_str_has_prefix(uris[i], "mailto:")
                            || g_str_has_prefix(uris[i], "MAILTO:"))
                        && !strchr(uris[i], '?'))
                {
                    old = uris[i];
                    uris[i] = g_strdup(uris[i] + 7);
                    g_free(old);
                }
            }
            if (uris)
            {
                char *flat = g_strjoinv("\r\n", uris);

                drd->handler(widget, flat, strlen(flat), drd->data);
                g_free (flat);
            }
            g_strfreev(uris);
            g_free(uri_list);
            break;
        case ROXTERM_DRAG_TARGET_TAB:
            if (drd->tab_handler)
            {
                drd->tab_handler(*(GtkWidget**) selection_data->data,
                        drd->data);
            }
    }
}

DragReceiveData *drag_receive_setup_dest_widget(GtkWidget *widget,
        DragReceiveHandler handler, DragReceiveTabHandler tab_handler,
        gpointer data)
{
    static GtkTargetEntry target_table[] = {
        /*
        { "application/x-color", 0, ROXTERM_DRAG_TARGET_COLOR },
        { "property/bgimage",    0, ROXTERM_DRAG_TARGET_BGIMAGE },
        { "x-special/gnome-reset-background", 0, ROXTERM_DRAG_TARGET_RESET_BG },
        */
        { (char *) "text/uri-list",  0, ROXTERM_DRAG_TARGET_URI_LIST },
        { (char *) "text/x-moz-url",  0, ROXTERM_DRAG_TARGET_MOZ_URL },
        { (char *) "UTF8_STRING", 0, ROXTERM_DRAG_TARGET_UTF8_STRING },
        { (char *) "COMPOUND_TEXT", 0, ROXTERM_DRAG_TARGET_COMPOUND_TEXT },
        { (char *) "TEXT", 0, ROXTERM_DRAG_TARGET_TEXT },
        { (char *) "STRING",     0, ROXTERM_DRAG_TARGET_STRING },
        { (char *) "text/plain", 0, ROXTERM_DRAG_TARGET_TEXT_PLAIN }
        , { (char *) "GTK_NOTEBOOK_TAB",
                GTK_TARGET_SAME_APP, ROXTERM_DRAG_TARGET_TAB }
        /*{ (char *) "text/unicode", 0, ROXTERM_DRAG_TARGET_TEXT_UNICODE }*/
    };
    DragReceiveData *drd = g_new(DragReceiveData, 1);
    
    drd->handler = handler;
    drd->tab_handler = tab_handler;
    drd->data = data;
    g_signal_connect(widget, "drag_data_received",
            G_CALLBACK(drag_data_received), drd);
    
    gtk_drag_dest_set(widget,
                       GTK_DEST_DEFAULT_MOTION |
                       GTK_DEST_DEFAULT_HIGHLIGHT |
                       GTK_DEST_DEFAULT_DROP,
                       target_table, G_N_ELEMENTS(target_table),
                       GDK_ACTION_COPY | GDK_ACTION_MOVE);
    return drd;
}

/* vi:set sw=4 ts=4 noet cindent cino= */
