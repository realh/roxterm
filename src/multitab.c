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

#include <ctype.h>
#include <errno.h>

#include "dlg.h"
#include "globalopts.h"
#include "menutree.h"
#include "multitab.h"
#include "multitab-close-button.h"
#include "multitab-label.h"
#include "session-file.h"
#include "shortcuts.h"

#define HORIZ_TAB_WIDTH_CHARS 16

struct MultiTab {
    MultiWin *parent;
    GtkWidget *widget;            /* Top-level widget in notebook */
    char *window_title;
    char *window_title_template;
    GtkWidget *popup_menu_item, *menu_bar_item;
    GtkWidget *active_widget;
    gpointer user_data;
    GtkWidget *label;
    GtkAdjustment *adjustment;
    double scroll_step;
    GtkWidget *close_button;
    GtkWidget *label_box;
    const char *status_icon_name;
    GtkWidget *rename_dialog;
    gboolean postponed_free;
    gboolean old_win_destroyed;
    gboolean title_template_locked;
    int middle_click_action;
    gboolean restore_pending;
    int restore_rows, restore_columns;
};

struct MultiWin {
    GtkWidget *gtkwin;        /* Top-level window */
    GtkWidget *vbox;          /* Container for menu bar, tabs and vte widget */
    GtkWidget *notebook;
    MenuTree *menu_bar;
    MenuTree *popup_menu;
    MenuTree *short_popup;
    guint ntabs;
    GList *tabs;
    MultiTab *current_tab;
    GtkPositionType tab_pos;
    gboolean always_show_tabs;
    gpointer user_data_template;
    gulong destroy_handler;
    gboolean ignore_tab_selections;
    gboolean ignore_toggles;
    gboolean ignore_tabs_moving;
    gboolean show_menu_bar;
    Options *shortcuts;
    GtkAccelGroup *accel_group;
    MultiTabSelectionHandler tab_selection_handler;
    gboolean menu_bar_set;        /* Menu bar can be configured either from
                                   profile when opening a window, or when user
                                   clicks on menu. Former case doesn't resize
                                   window and should only be done once; this
                                   flag ensures attempts for subsequent tabs
                                   are ignored */
    MultiWinScrollBar_Position scroll_bar_pos;
    gboolean wrap_switch_tab;
    int zoom_index;
    gboolean borderless;
    gboolean fullscreen;
    char *title_template;
    char *child_title;
    gboolean composite;
    gboolean title_template_locked;
    int best_tab_width;
    GtkWidget *add_tab_button;
    gboolean show_add_tab_button;
    GdkGeometry geom_hints;
    GdkWindowHints geom_hint_mask;
};

static double multi_win_zoom_factors[] = {
    PANGO_SCALE_XX_SMALL / (1.2 *1.2 * 1.2 * 1.2),
    PANGO_SCALE_XX_SMALL / (1.2 * 1.2 * 1.2),
    PANGO_SCALE_XX_SMALL / (1.2 * 1.2),
    PANGO_SCALE_XX_SMALL / 1.2,
    PANGO_SCALE_XX_SMALL,
    PANGO_SCALE_X_SMALL,
    PANGO_SCALE_SMALL,
    PANGO_SCALE_MEDIUM,
    PANGO_SCALE_LARGE,
    PANGO_SCALE_X_LARGE,
    PANGO_SCALE_XX_LARGE,
    PANGO_SCALE_XX_LARGE * 1.2,
    PANGO_SCALE_XX_LARGE * 1.2 * 1.2,
    PANGO_SCALE_XX_LARGE * 1.2 * 1.2 * 1.2,
    PANGO_SCALE_XX_LARGE * 1.2 * 1.2 * 1.2 * 1.2
};
#define MULTI_WIN_NORMAL_ZOOM_INDEX 7
#define MULTI_WIN_N_ZOOM_FACTORS 15

static char *multi_win_role_prefix = NULL;
static int multi_win_role_index = 0;

static MultiTabFiller multi_tab_filler;
static MultiTabDestructor multi_tab_destructor;
static MultiWinMenuSignalConnector multi_win_menu_signal_connector;
static MultiWinGeometryFunc multi_win_geometry_func;
static MultiWinSizeFunc multi_win_size_func;
static MultiWinDefaultSizeFunc multi_win_default_size_func;
static MultiTabToNewWindowHandler multi_tab_to_new_window_handler;
static MultiWinZoomHandler multi_win_zoom_handler;
MultiWinGetDisableMenuShortcuts multi_win_get_disable_menu_shortcuts;
static MultiWinGetTabPos multi_win_get_config_tab_pos;
static MultiWinDeleteHandler multi_win_delete_handler;
static MultiTabGetShowCloseButton multi_tab_get_show_close_button;
static MultiTabGetNewTabAdjacent multi_tab_get_new_tab_adjacent;
static MultiTabConnectMiscSignals multi_tab_connect_misc_signals;

static gboolean multi_win_notify_tab_removed(MultiWin *, MultiTab *);

static void multi_win_add_tab(MultiWin *, MultiTab *, int position,
        gboolean notify_only);

static void multi_tab_add_menutree_items(MultiWin * win, MultiTab * tab,
        int position);

static void multi_win_select_tab_action(GtkCheckMenuItem *widget,
        MultiTab * tab);

static void multi_win_destructor(MultiWin *win, gboolean destroy_widgets);

static void multi_win_close_tab_clicked(GtkWidget *widget, MultiTab *tab);

static gboolean multi_win_process_geometry(MultiWin *win,
        MultiTab *tab, int columns, int rows, int *width, int *height);

static char *multi_tab_get_full_window_title(MultiTab * tab);

static gboolean multi_tab_do_restore_size(MultiTab *tab)
{
    int width, height;

    multi_win_process_geometry(tab->parent, tab,
            tab->restore_columns, tab->restore_rows, &width, &height);
    gtk_window_resize(GTK_WINDOW(tab->parent->gtkwin), width, height);
    return FALSE;
}

static void multi_tab_size_allocate(GtkWidget *widget, GdkRectangle *alloc,
        MultiTab *tab)
{
    (void) widget;
    (void) alloc;

    if (tab->restore_pending)
    {
        tab->restore_pending = FALSE;
        /* Calling gtk_window_resize from a size-allocate handler doesn't seem
         * to work, which is probably quite a sensible precaution by GTK, but it
         * means we have to make another deferment.
         */
        g_idle_add((GSourceFunc) multi_tab_do_restore_size, tab);
    }
}

/* After adding/removing menu bars etc we want to resize the window to preserve
 * the terminal's geometry. But when the packing change unwantedly resizes the
 * terminal widget the only way to reliably read its new size is during/after a
 * size-allocate. This function caches the current size of the terminal's
 * viewport (which we want to keep). The size-allocate handler then works out
 * the difference between the window size and the new viewport size, then adds
 * this to the old viewport size to get the new window size.
 */
static void multi_win_request_size_restore(MultiWin *win)
{
    MultiTab *tab = win->current_tab;

    if (!tab || !tab->active_widget ||
            !gtk_widget_get_realized(tab->active_widget))
    {
        return;
    }
    if (multi_win_is_maximised(win) || multi_win_is_fullscreen(win))
        return;
    tab->restore_rows = -1;
    tab->restore_columns = -1;
    multi_win_size_func(tab->user_data, FALSE,
            &tab->restore_columns, &tab->restore_rows);
    tab->restore_pending = TRUE;
}

MultiTab *multi_tab_get_from_widget(GtkWidget *widget)
{
    return g_object_get_data(G_OBJECT(widget), "roxterm_tab");
}

void multi_tab_popup_menu_at_pointer(MultiTab * tab)
{
    GtkMenu *menu = GTK_MENU(menutree_get_top_level_widget
            (tab->parent->show_menu_bar ? tab->parent->short_popup :
            tab->parent->popup_menu));

    gtk_menu_set_screen(menu, gtk_widget_get_screen(tab->widget));
    gtk_menu_popup_at_pointer(menu, NULL);
}

/*
void multi_tab_popup_menu_over_widget(MultiTab * tab)
{
    GtkMenu *menu = GTK_MENU(menutree_get_top_level_widget
            (tab->parent->show_menu_bar ? tab->parent->short_popup :
            tab->parent->popup_menu));

    gtk_menu_set_screen(menu, gtk_widget_get_screen(tab->widget));
    gtk_menu_popup_at_widget(menu, tab->active_widget,
            GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
}
*/

void multi_tab_init(MultiTabFiller filler, MultiTabDestructor destructor,
    MultiWinMenuSignalConnector menu_signal_connector,
    MultiWinGeometryFunc geometry_func, MultiWinSizeFunc size_func,
    MultiWinDefaultSizeFunc default_size_func,
    MultiTabToNewWindowHandler tab_to_new_window_handler,
    MultiWinZoomHandler zoom_handler,
    MultiWinGetDisableMenuShortcuts get_disable_menu_shortcuts,
    MultiWinGetTabPos tab_pos,
    MultiWinDeleteHandler delete_handler,
    MultiTabGetShowCloseButton get_show_close_button,
    MultiTabGetNewTabAdjacent get_new_tab_adjacent,
    MultiTabConnectMiscSignals connect_misc_signals
    )
{
    multi_tab_filler = filler;
    multi_tab_destructor = destructor;
    multi_win_menu_signal_connector = menu_signal_connector;
    multi_win_geometry_func = geometry_func;
    multi_win_size_func = size_func;
    multi_win_default_size_func = default_size_func;
    multi_tab_to_new_window_handler = tab_to_new_window_handler;
    multi_win_zoom_handler = zoom_handler;
    multi_win_get_disable_menu_shortcuts = get_disable_menu_shortcuts;
    multi_win_get_config_tab_pos = tab_pos;
    multi_win_delete_handler = delete_handler;
    multi_tab_get_show_close_button = get_show_close_button;
    multi_tab_get_new_tab_adjacent = get_new_tab_adjacent;
    multi_tab_connect_misc_signals = connect_misc_signals;
}

// Doesn't connect client's signal handlers
MultiTab *multi_tab_new_defer_connect(MultiWin * parent,
        gpointer user_data_template)
{
    MultiTab *tab = g_new0(MultiTab, 1);
    guint pos;

    tab->parent = parent;
    tab->status_icon_name = NULL;
    tab->widget = multi_tab_filler(parent, tab, user_data_template,
        &tab->user_data, &tab->active_widget, &tab->adjustment);
    g_signal_connect(tab->widget, "size-allocate",
            G_CALLBACK(multi_tab_size_allocate), tab);
    g_object_set_data(G_OBJECT(tab->widget), "roxterm_tab", tab);

    if (parent->current_tab &&
            multi_tab_get_new_tab_adjacent(user_data_template))
    {
        pos = multi_tab_get_page_num(parent->current_tab) + 1;
        if (pos == parent->ntabs)
            pos = -1;
    }
    else
    {
        pos = -1;
    }
    multi_win_add_tab(parent, tab, pos, FALSE);
    multi_win_select_tab(parent, tab);

    //g_debug("call roxterm_force_resize_now from line %d", __LINE__);
    //roxterm_force_resize_now(parent);

    return tab;
}

MultiTab *multi_tab_new(MultiWin * parent, gpointer user_data_template)
{
    MultiTab *tab = multi_tab_new_defer_connect(parent, user_data_template);
    multi_tab_connect_misc_signals(tab->user_data);
    return tab;
}

static void multi_tab_delete_without_notifying_parent(MultiTab * tab,
        gboolean destroy_widgets)
{
    if (tab->rename_dialog)
    {
        gtk_widget_destroy(tab->rename_dialog);
        tab->postponed_free = TRUE;
    }
    else
    {
        tab->postponed_free = FALSE;
    }
    (*multi_tab_destructor) (tab->user_data);
    tab->user_data = NULL;

    g_free(tab->window_title);
    tab->window_title = NULL;
    g_free(tab->window_title_template);
    tab->window_title_template = NULL;
    if (destroy_widgets && tab->widget)
    {
        gtk_widget_destroy(tab->widget);
        tab->widget = NULL;
    }
    /* if !destroy_widgets menutrees are being destroyed anyway,
     * so no need to update */
    if (destroy_widgets && tab->popup_menu_item)
    {
        menutree_remove_tab(tab->parent->popup_menu, tab->popup_menu_item);
        tab->popup_menu_item = NULL;
    }
    if (destroy_widgets && tab->menu_bar_item)
    {
        menutree_remove_tab(tab->parent->menu_bar, tab->menu_bar_item);
        tab->menu_bar_item = NULL;
    }
    if (!tab->postponed_free)
        g_free(tab);
}

void multi_tab_delete(MultiTab * tab)
{
    MultiWin *win = tab->parent;

    win->ignore_tabs_moving = TRUE;
    multi_tab_delete_without_notifying_parent(tab, TRUE);
    if (!multi_win_notify_tab_removed(win, tab))
    {
        win->ignore_tabs_moving = FALSE;
    }
}

GtkWidget *multi_tab_get_widget(MultiTab * tab)
{
    return tab->widget;
}

/*
static void multi_win_set_geometry_hints_for_tab(MultiWin * win, MultiTab * tab)
{
    GdkGeometry geom;
    GdkWindowHints hints;

    if (multi_win_geometry_func)
    {
        (*multi_win_geometry_func) (tab->user_data, &geom, &hints);
        multi_win_set_geometry_hints(win, tab->active_widget, &geom, hints);
    }
}
*/

static char *make_title(const char *template, const char *title,
        int tab_num, int tab_count)
{
    GString *subbed;
    size_t n, l;
    if (!template || !template[0])
        return g_new0(char, 1);
    l = strlen(template);
    subbed = g_string_sized_new(strlen(template));
    for (n = 0; n < l; ++n)
    {
        if (template[n] == '%')
        {
            switch (template[++n])
            {
                case 's':
                    if (title)
                        g_string_append(subbed, title);
                    break;
                case 't':
                    g_string_append_printf(subbed, "%d", tab_num);
                    break;
                case 'n':
                    g_string_append_printf(subbed, "%d", tab_count);
                    break;
                case 0:
                    --n;    /* Make sure next iteration sees terminator */
                    // Fall-through
                case '%':
                    g_string_append_c(subbed, '%');
                    break;
                default:
                    g_string_append_c(subbed, '%');
                    g_string_append_c(subbed, template[n]);
                    break;
            }
        }
        else
        {
            g_string_append_c(subbed, template[n]);
        }
    }
    return g_string_free(subbed, FALSE);
}


static void multi_tab_set_full_window_title(MultiTab * tab)
{
    MultiWin *win = tab->parent;
    char *tab_label;
    
    tab_label = multi_tab_get_full_window_title(tab);
    if (tab->label)
    {
        multitab_label_set_text(MULTITAB_LABEL(tab->label), tab_label);
    }
    if (win)
    {
        win->ignore_toggles = TRUE;
        if (win->current_tab == tab)
            multi_win_set_title(win, tab->window_title);
        if (tab->popup_menu_item)
        {
            tab->popup_menu_item = menutree_change_tab_title
                (win->popup_menu, tab->popup_menu_item, tab_label);
            g_signal_connect(tab->popup_menu_item, "toggled",
                G_CALLBACK(multi_win_select_tab_action), tab);
        }
        if (tab->menu_bar_item)
        {
            tab->menu_bar_item = menutree_change_tab_title
                (win->menu_bar, tab->menu_bar_item, tab_label);
            g_signal_connect(tab->menu_bar_item, "toggled",
                G_CALLBACK(multi_win_select_tab_action), tab);
        }
        win->ignore_toggles = FALSE;
    }
    g_free(tab_label);
}

void multi_tab_set_window_title(MultiTab * tab, const char *title)
{
    g_free(tab->window_title);
    tab->window_title = title ? g_strdup(title) : NULL;
    multi_tab_set_full_window_title(tab);
}

void multi_tab_set_window_title_template(MultiTab * tab, const char *template)
{
    if (tab->title_template_locked)
        return;
    g_free(tab->window_title_template);
    tab->window_title_template = template ? g_strdup(template) : NULL;
    multi_tab_set_full_window_title(tab);
}

gboolean multi_tab_get_title_template_locked(MultiTab *tab)
{
    return tab->title_template_locked;
}

void multi_tab_set_title_template_locked(MultiTab *tab, gboolean locked)
{
    tab->title_template_locked = locked;
}

const char *multi_tab_get_window_title(MultiTab * tab)
{
    return tab->window_title;
}

const char *multi_tab_get_window_title_template(MultiTab * tab)
{
    return tab->window_title_template;
}

static char *multi_tab_get_full_window_title(MultiTab * tab)
{
    int num = tab->parent->ntabs;
    int pos = multi_tab_get_page_num(tab) + 1;
    return make_title(tab->window_title_template, tab->window_title, pos, num);
}

gpointer multi_tab_get_user_data(MultiTab * tab)
{
    return tab->user_data;
}

void multi_tab_connect_tab_selection_handler(MultiWin * win,
    MultiTabSelectionHandler handler)
{
    win->tab_selection_handler = handler;
}

int multi_tab_get_page_num(MultiTab *tab)
{
    return gtk_notebook_page_num(GTK_NOTEBOOK(tab->parent->notebook),
            tab->widget);
}

MultiWin *multi_tab_get_parent(MultiTab *tab)
{
    return tab->parent;
}

inline static void
multi_win_shade_item_in_both_menus(MultiWin * win, MenuTreeID id,
    gboolean shade)
{
    gtk_widget_set_sensitive(menutree_get_widget_for_id(win->menu_bar, id),
        !shade);
    gtk_widget_set_sensitive(menutree_get_widget_for_id(win->popup_menu, id),
        !shade);
}

inline static gboolean multi_win_at_first_tab(MultiWin *win)
{
    return g_list_previous(g_list_find(win->tabs, win->current_tab)) == NULL;
}

inline static gboolean multi_win_at_last_tab(MultiWin *win)
{
    return g_list_next(g_list_find(win->tabs, win->current_tab)) == NULL;
}

/* Returns TRUE if menu items should be shaded based on wrap_switch_tab flag
 * and on whether there's only one tab */
inline static gboolean multi_win_override_shade_next_prev_tab(MultiWin *win)
{
    return win->ntabs == 1 || !win->wrap_switch_tab;
}

static void multi_win_shade_for_previous_tab(MultiWin *win)
{
    gboolean shade = multi_win_override_shade_next_prev_tab(win) &&
            multi_win_at_first_tab(win);

    multi_win_shade_item_in_both_menus(win, MENUTREE_TABS_PREVIOUS_TAB,
            shade);
    multi_win_shade_item_in_both_menus(win, MENUTREE_TABS_MOVE_TAB_LEFT,
            shade);
}

static void multi_win_shade_for_next_tab(MultiWin * win)
{
    gboolean shade = multi_win_override_shade_next_prev_tab(win) &&
            multi_win_at_last_tab(win);

    multi_win_shade_item_in_both_menus(win, MENUTREE_TABS_NEXT_TAB,
            shade);
    multi_win_shade_item_in_both_menus(win, MENUTREE_TABS_MOVE_TAB_RIGHT,
            shade);
}

inline static void multi_win_shade_for_next_and_previous_tab(MultiWin * win)
{
    multi_win_shade_for_previous_tab(win);
    multi_win_shade_for_next_tab(win);
}

static void multi_win_shade_for_single_tab(MultiWin * win)
{
    multi_win_shade_item_in_both_menus(win, MENUTREE_TABS_CLOSE_OTHER_TABS,
            win->ntabs <= 1);
    multi_win_shade_item_in_both_menus(win, MENUTREE_TABS_DETACH_TAB,
            win->ntabs <= 1);
}

static void multi_win_shade_menus_for_tabs(MultiWin * win)
{
    multi_win_shade_for_next_and_previous_tab(win);
    multi_win_shade_for_single_tab(win);
}

static void multi_tab_remove_menutree_items(MultiWin * win, MultiTab * tab)
{
    if (tab->menu_bar_item)
    {
        menutree_remove_tab(win->menu_bar, tab->menu_bar_item);
        tab->menu_bar_item = NULL;
    }
    if (tab->popup_menu_item)
    {
        menutree_remove_tab(win->popup_menu, tab->popup_menu_item);
        tab->popup_menu_item = NULL;
    }
}

static void renumber_tabs(MultiWin *win)
{
    GList *link;
    int n = 0;

    for (link = win->tabs; link; link = g_list_next(link))
    {
        MultiTab *tab = link->data;

        multi_tab_set_full_window_title(tab);
        ++n;
    }
}

void multi_tab_move_to_position(MultiTab *tab, int position, gboolean reorder)
{
    MultiWin *win = tab->parent;

    if (reorder)
    {
        gtk_notebook_reorder_child(GTK_NOTEBOOK(win->notebook),
                tab->widget, position);
    }
    win->tabs = g_list_remove(win->tabs, tab);
    win->tabs = g_list_insert(win->tabs, tab, position);
    renumber_tabs(win);
    multi_win_shade_menus_for_tabs(win);
    multi_tab_remove_menutree_items(win, tab);
    multi_tab_add_menutree_items(win, tab, position);
    multi_tab_set_full_window_title(tab);
}

gboolean multi_tab_remove_from_parent(MultiTab *tab, gboolean notify_only)
{
    MultiWin *win = tab->parent;
    gboolean destroyed;

    g_return_val_if_fail(win, TRUE);
    win->ignore_tabs_moving = TRUE;
    if (!notify_only)
    {
        g_object_ref(tab->widget);
        tab->label = NULL;
        gtk_notebook_detach_tab(GTK_NOTEBOOK(win->notebook), tab->widget);
        tab->close_button = NULL;
    }
    multi_tab_remove_menutree_items(win, tab);
    destroyed = multi_win_notify_tab_removed(win, tab);
    if (!destroyed)
    {
        win->ignore_tabs_moving = FALSE;
    }
    tab->parent = NULL;
    return destroyed;
}

void multi_tab_move_to_new_window(MultiWin *win, MultiTab *tab, int position)
{
    MultiWin *old_win = tab->parent;
    gboolean old_win_destroyed;

    multi_win_set_always_show_tabs(win, old_win->always_show_tabs);
    multi_win_set_show_menu_bar(win, old_win->show_menu_bar);
    if (multi_win_is_borderless(old_win))
        multi_win_set_borderless(win, TRUE);
    if (multi_win_is_fullscreen(old_win))
        multi_win_set_fullscreen(win, TRUE);
    else if (multi_win_is_maximised(old_win))
        gtk_window_maximize(GTK_WINDOW(win->gtkwin));
    if (!win->tabs)
    {
        win->title_template = old_win->title_template ?
                g_strdup(old_win->title_template) : NULL;
    }
    old_win_destroyed = old_win && old_win->ntabs <= 1;
    multi_tab_remove_from_parent(tab, FALSE);
    multi_win_add_tab(win, tab, position, FALSE);
    /* multi_win_set_geometry_hints_for_tab(win, tab); */
    if (multi_tab_to_new_window_handler)
    {
        multi_tab_to_new_window_handler(win, tab,
                old_win_destroyed ? NULL : old_win);
    }
    /* Remove extra references added by multi_tab_remove_from_parent */
    g_object_unref(tab->widget);
}

void multi_tab_draw_attention(MultiTab *tab)
{
    multitab_label_draw_attention(MULTITAB_LABEL(tab->label));
}

void multi_tab_cancel_attention(MultiTab *tab)
{
    multitab_label_cancel_attention(MULTITAB_LABEL(tab->label));
}

static void multi_tab_pack_for_horizontal(MultiTab *tab, GtkContainer *nb)
{
    gtk_container_child_set(nb, tab->widget,
            "tab-expand", FALSE, "tab-fill", TRUE, NULL);
    multitab_label_set_single(MULTITAB_LABEL(tab->label), TRUE);
}

static void multi_tab_pack_for_single(MultiTab *tab, GtkContainer *nb)
{
    gtk_container_child_set(nb, tab->widget,
            "tab-expand", FALSE, "tab-fill", FALSE, NULL);
    multitab_label_set_single(MULTITAB_LABEL(tab->label), TRUE);
}

static void multi_win_pack_for_single_tab(MultiWin *win)
{
    MultiTab *tab = win->tabs->data;

    multi_tab_pack_for_single(tab, GTK_CONTAINER(win->notebook));
}

static void multi_tab_pack_for_multiple(MultiTab *tab, GtkContainer *nb)
{
    gtk_container_child_set(nb, tab->widget,
            "tab-expand", TRUE, "tab-fill", TRUE, NULL);
    multitab_label_set_single(MULTITAB_LABEL(tab->label), FALSE);
}

static void multi_win_pack_for_multiple_tabs(MultiWin *win)
{
    GList *link;
    GtkContainer *nb = GTK_CONTAINER(win->notebook);

    for (link = win->tabs; link; link = g_list_next(link))
    {
        MultiTab *tab = link->data;

        multi_tab_pack_for_multiple(tab, nb);
    }
}

static void multi_win_set_full_title(MultiWin *win)
{
    if (win->gtkwin)
    {
        int pos = win && win->current_tab ?
                  multi_tab_get_page_num(win->current_tab) + 1 : 1;
        char *title0 = make_title(win->title_template, win->child_title,
                pos, win->ntabs);

        gtk_window_set_title(GTK_WINDOW(win->gtkwin), title0);
        g_free(title0);
    }
}

void multi_win_set_title(MultiWin *win, const char *title)
{
    if (win->child_title != title)
    {
        g_free(win->child_title);
        win->child_title = title ? g_strdup(title) : NULL;
    }
    multi_win_set_full_title(win);
}

static void multi_win_highlight_selected_tab(MultiWin *win)
{
    GList *link;
    for (link = win->tabs; link; link = g_list_next(link))
    {
        MultiTab *tab = link->data;
        /* If this is called while a tab is being moved to another window its
         * label may be NULL
         */
        if (tab->label)
        {
            multitab_label_set_current(MULTITAB_LABEL(tab->label),
                    tab == win->current_tab);
        }
    }
}

void multi_win_select_tab(MultiWin * win, MultiTab * tab)
{
    /* This is called by anything that causes a change of tab to keep things
     * consistent; doing this will cause extra tab change related events to be
     * generated so we have to ignore them for the duration */
    if (win->ignore_tab_selections || tab == win->current_tab)
    {
        return;
    }
    win->ignore_tab_selections = TRUE;
    win->ignore_toggles = TRUE;
    win->current_tab = tab;
    if (tab)
    {
        char *title = multi_tab_get_full_window_title(tab);

        if (tab->label)
            multitab_label_cancel_attention(MULTITAB_LABEL(tab->label));
        multi_tab_set_status_icon_name(tab, NULL);
        win->user_data_template = tab->user_data;
        gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook),
            multi_tab_get_page_num(tab));
        gtk_widget_grab_focus(tab->active_widget);
        multi_win_set_title(win, tab->window_title);
        g_free(title);
        menutree_select_tab(win->popup_menu, tab->popup_menu_item);
        menutree_select_tab(win->menu_bar, tab->menu_bar_item);
        if (gtk_widget_get_realized(tab->active_widget))
        {
            if (win->tab_selection_handler)
                win->tab_selection_handler(tab->user_data, tab);
        }
    }
    multi_win_shade_for_next_and_previous_tab(win);
    multi_win_highlight_selected_tab(win);
    /* FIXME: Ideally we should shade scroll up/down menu items if tab doesn't
     * have an adjustment, but in this implementation they always do
     */
    win->ignore_tab_selections = FALSE;
    win->ignore_toggles = FALSE;
}

/* The top-level needs to be transparent as a whole to support the transparency
 * option, but the menu bar is also transparent, so we need to manually fill in
 * its background.
 */
static gboolean
multi_win_draw_chrome_bg(GtkWidget *widget, cairo_t *cr, MultiWin *win)
{
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    GtkAllocation alloc;

    gtk_widget_get_allocation(win->vbox, &alloc);
    gtk_render_background(context, cr,
            alloc.x, alloc.y, alloc.width, alloc.height);
    return FALSE;
}

static void add_menu_bar(MultiWin *win)
{
    GtkWidget *menu_bar;

    if (win->show_menu_bar)
        return;
    menu_bar = menutree_get_top_level_widget(win->menu_bar);
    gtk_box_pack_start(GTK_BOX(win->vbox), menu_bar, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(win->vbox), menu_bar, 0);
    gtk_widget_show(menu_bar);
    win->show_menu_bar = TRUE;
}

static void remove_menu_bar(MultiWin *win)
{
    GtkWidget *menu_bar;

    if (!win->show_menu_bar)
        return;
    menu_bar = menutree_get_top_level_widget(win->menu_bar);
    gtk_widget_hide(menu_bar);
    gtk_container_remove(GTK_CONTAINER(win->vbox), menu_bar);
    win->show_menu_bar = FALSE;
}

void multi_win_set_show_menu_bar(MultiWin * win, gboolean show)
{
    if (win->menu_bar_set == show)
        return;
    multi_win_request_size_restore(win);
    win->menu_bar_set = show;
    if (show)
        add_menu_bar(win);
    else
        remove_menu_bar(win);
    menutree_set_show_menu_bar_active(win->menu_bar, show);
    menutree_set_show_menu_bar_active(win->popup_menu, show);
    menutree_set_show_menu_bar_active(win->short_popup, show);
}

gboolean multi_win_get_show_menu_bar(MultiWin * win)
{
    return win->show_menu_bar;
}

void multi_win_set_borderless(MultiWin *win, gboolean borderless)
{
    if (borderless != win->borderless)
    {
        gtk_window_set_decorated(GTK_WINDOW(win->gtkwin), !borderless);
        win->borderless = borderless;
        menutree_set_borderless_active(win->menu_bar, borderless);
        menutree_set_borderless_active(win->popup_menu, borderless);
    }
}

void multi_win_set_fullscreen(MultiWin *win, gboolean fullscreen)
{
    if (fullscreen == win->fullscreen)
        return;
    if (fullscreen)
    {
        gtk_window_fullscreen(GTK_WINDOW(win->gtkwin));
    }
    else
    {
        gtk_window_unfullscreen(GTK_WINDOW(win->gtkwin));
    }
    win->fullscreen = fullscreen;
}

#if GTK_CHECK_VERSION(3,22,23)
#define WIN_STATE_TILED \
            (GDK_WINDOW_STATE_TOP_TILED | GDK_WINDOW_STATE_BOTTOM_TILED | \
            GDK_WINDOW_STATE_LEFT_TILED | GDK_WINDOW_STATE_RIGHT_TILED)
#else
#define WIN_STATE_TILED GDK_WINDOW_STATE_TILED
#endif

#define WIN_STATE_SNAPPED (GDK_WINDOW_STATE_MAXIMIZED | \
        GDK_WINDOW_STATE_FULLSCREEN | WIN_STATE_TILED)

inline static void multi_win_apply_geometry_hints(MultiWin *win)
{
    gtk_window_set_geometry_hints(GTK_WINDOW(win->gtkwin), NULL,
            &win->geom_hints, win->geom_hint_mask);
}

static gboolean multi_win_state_event_handler(GtkWidget *widget,
        GdkEventWindowState *event, MultiWin *win)
{
    /* Having geometry hints set causes window to shrink by one
     * character width and height when unmaximizing.
     * <https://gitlab.gnome.org/GNOME/gtk/issues/34>
     * This is a workaround.
     */
    if (event->changed_mask & WIN_STATE_SNAPPED)
    {
        if (event->new_window_state & WIN_STATE_SNAPPED)
        {
            gtk_window_set_geometry_hints(GTK_WINDOW(widget), NULL, NULL, 0);
        }
        else
        {
            multi_win_apply_geometry_hints(win);
        }
    }

    if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
        win->fullscreen =
                (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
        menutree_set_fullscreen_active(win->menu_bar, win->fullscreen);
        menutree_set_fullscreen_active(win->popup_menu, win->fullscreen);
    }
    return FALSE;
}

static void multi_win_realize_handler(GtkWidget *win, gpointer data)
{
    GdkWindow *w = gtk_widget_get_window(win);

    gdk_window_set_group(w, w);
    (void) data;
}

static void multi_win_destroy_handler(GObject * obj, MultiWin * win)
{
    (void) obj;

    multi_win_destructor(win, FALSE);
}

static void multi_win_menutree_deleted_handler(MenuTree * tree, gpointer data)
{
    MultiWin *win = data;

    if (win->menu_bar == tree)
        win->menu_bar = NULL;
    else if (win->popup_menu == tree)
        win->popup_menu = NULL;
    else if (win->short_popup == tree)
        win->short_popup = NULL;
    else
    {
        g_warning("Unknown MenuTree deleted");
    }
}

static void
multi_win_page_switched(GtkNotebook * notebook, GtkWidget *page,
    guint page_num, MultiWin * win)
{
    MultiTab *tab = multi_tab_get_from_widget(
            gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), page_num));
    (void) notebook;
    (void) page;

    if (win->current_tab == tab)
        return;
    multi_win_select_tab(win, tab);
}

static void tab_set_name_from_clipboard_callback(GtkClipboard *clp,
        const gchar *text, gpointer data)
{
    MultiTab *tab = (MultiTab *) data;
    (void) clp;

    if (text == NULL || tab == NULL)
        return;
    multi_tab_set_window_title_template(tab, text);
}

static gboolean multi_win_focus_in(GtkWidget *widget, GdkEventFocus *event,
        MultiWin *win)
{
    (void) widget;
    (void) event;
    multi_win_all = g_list_remove(multi_win_all, win);
    multi_win_all = g_list_prepend(multi_win_all, win);
    gtk_window_set_urgency_hint(GTK_WINDOW(win->gtkwin), FALSE);
    return FALSE;
}

static void popup_tabs_menu(MultiWin *win, GdkEvent *event)
{
    GtkMenuItem *mparent;

    g_return_if_fail(win->popup_menu != NULL);
    mparent = GTK_MENU_ITEM(
            menutree_get_widget_for_id(win->popup_menu, MENUTREE_TABS));
    gtk_menu_popup_at_pointer(GTK_MENU(gtk_menu_item_get_submenu(mparent)),
            event);
}

static gboolean tab_clicked_handler(GtkWidget *widget,
        GdkEventButton *event, MultiTab *tab)
{
    (void) widget;
    if (event->type != GDK_BUTTON_PRESS)
        return FALSE;

    switch (event->button)
    {
        case 2:
            switch (tab->middle_click_action)
            {
                case 1:
                    multi_win_close_tab_clicked(NULL, tab);
                    break;
                case 2:
                    gtk_clipboard_request_text(
                            gtk_clipboard_get(GDK_SELECTION_PRIMARY),
                            tab_set_name_from_clipboard_callback, tab);
                    break;
                default:
                    break;
            }
            return TRUE;
        case 3:
            multi_win_select_tab(tab->parent, tab);
            popup_tabs_menu(tab->parent, (GdkEvent *) event);
            return TRUE;
        default:
            break;
    }
    return FALSE;
}

/* Creates the label widget for a tab. tab->label is the GtkLabel containing
 * the text; the return value is the top-level container. */
static GtkWidget *make_tab_label(MultiTab *tab, GtkPositionType tab_pos)
{
    (void) tab_pos;
    tab->label = multitab_label_new(tab->parent->notebook, NULL,
            &tab->parent->best_tab_width);
    multi_tab_set_full_window_title(tab);
    tab->label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_set_homogeneous(GTK_BOX(tab->label_box), FALSE);
    gtk_box_pack_start(GTK_BOX(tab->label_box), tab->label, TRUE, TRUE, 0);
    g_signal_connect(tab->label, "button-press-event",
            G_CALLBACK(tab_clicked_handler), tab);
    if (multi_tab_get_show_close_button(tab->user_data))
    {
        multi_tab_add_close_button(tab);
    }
    gtk_widget_show_all(tab->label_box);

    return tab->label_box;
}

static void page_reordered_callback(GtkNotebook *notebook, GtkWidget *child,
        guint page_num, MultiWin *win)
{
    MultiTab *tab = multi_tab_get_from_widget(child);
    (void) notebook;
    (void) win;

    g_return_if_fail(tab);
    multi_tab_move_to_position(tab, page_num, FALSE);
}

static void page_added_callback(GtkNotebook *notebook, GtkWidget *child,
        guint page_num, MultiWin *win)
{
    MultiTab *tab = multi_tab_get_from_widget(child);
    gboolean old_win_destroyed;
    MultiWin *old_win = tab->parent;
    gboolean vert_tabs;

    g_return_if_fail(tab);
    if (win->ignore_tabs_moving)
        return;
    if (tab->parent == win)
    {
        /* Dragged in from same window so do nothing */
        return;
    }
    vert_tabs = win->tab_pos == GTK_POS_LEFT || win->tab_pos == GTK_POS_RIGHT;
    if (tab->parent)
    {
        old_win_destroyed = multi_tab_remove_from_parent(tab, TRUE);
    }
    else
    {
        old_win_destroyed = TRUE;
        g_warning("Page added had no previous parent");
    }
    multi_win_add_tab(win, tab, page_num, TRUE);
    multi_tab_to_new_window_handler(win, tab,
            old_win_destroyed ? NULL : old_win);
    if (vert_tabs)
    {
        multi_tab_pack_for_horizontal(tab, GTK_CONTAINER(notebook));
    }
    else
    {
        if (win->ntabs == 1)
        {
            multi_win_pack_for_single_tab(win);
            gtk_widget_queue_resize(win->notebook);
            multi_win_show(win);
        }
        else if (win->ntabs == 2)
        {
            multi_win_pack_for_multiple_tabs(win);
        }
        else
        {
            multi_tab_pack_for_multiple(tab, GTK_CONTAINER(notebook));
        }
#if 0
        if (old_win_destroyed && !vert_tabs)
        {
            /* The dragged tab's label will now be stupidly small,
             * but there's absolutely nothing we can do about it.
             */
        }
#endif
    }
}

static GtkNotebook *multi_win_notebook_creation_hook(GtkNotebook *source,
        GtkWidget *page, gint x, gint y, gpointer data)
{
    (void) source;
    (void) data;
    MultiTab *tab = multi_tab_get_from_widget(page);
    MultiWin *win;

    g_return_val_if_fail(tab, NULL);
    /* Need to set !expand in advance */
    multi_tab_pack_for_single(tab, GTK_CONTAINER(tab->parent->notebook));
    win = multi_win_new_for_tab(x, y, tab);
    /* Defer showing till page_added_callback because we can't set size
     * correctly until the tab has been added to the notebook */
    return GTK_NOTEBOOK(win->notebook);
}

MultiWin *multi_win_new_for_tab(int x, int y, MultiTab *tab)
{
    gboolean disable_menu_shortcuts, disable_tab_shortcuts;
    MultiWin *win = tab->parent;
    int w, h;
    GtkWindow *gwin = GTK_WINDOW(win->gtkwin);
    const char *title_template = win->title_template;
    gboolean show_menubar = win->show_menu_bar;
    gboolean borderless = win->borderless;

    gtk_window_get_size(gwin, &w, &h);
    multi_win_get_disable_menu_shortcuts(tab->user_data,
            &disable_menu_shortcuts, &disable_tab_shortcuts);
    win = multi_win_new_blank(multi_win_get_shortcut_scheme(win),
            win->zoom_index, disable_menu_shortcuts, disable_tab_shortcuts,
                win->tab_pos, win->always_show_tabs,
                win->show_add_tab_button);
    multi_win_set_show_menu_bar(win, show_menubar);
    multi_win_set_title_template(win, title_template);
    multi_win_set_borderless(win, borderless);
    gwin = GTK_WINDOW(win->gtkwin);
    gtk_window_set_default_size(gwin, w, h);
    if (x != -1 && y != -1)
        gtk_window_move(gwin, MAX(x - 20, 0), MAX(y - 8, 0));
    gtk_widget_show_all(win->notebook);
    return win;
}

static void multi_win_close_tab_clicked(GtkWidget *widget, MultiTab *tab)
{
    (void) widget;
    if (!multi_win_delete_handler ||
            !multi_win_delete_handler(tab->parent->gtkwin, NULL,
                    tab->user_data))
    {
        multi_tab_delete(tab);
    }
}

MultiWin *multi_win_clone(MultiWin *old,
        gpointer user_data_template, gboolean always_show_tabs)
{
    char *geom;
    int width, height;
    MultiWin *result;
    GtkPositionType tab_pos = multi_win_get_config_tab_pos(user_data_template);
    gboolean borderless = multi_win_is_borderless(old);

    multi_win_default_size_func(multi_tab_get_user_data(old->current_tab),
            &width, &height);
    geom = g_strdup_printf("%dx%d", width, height);
    result = multi_win_new_with_geom(old->shortcuts,
            old->zoom_index, user_data_template, geom,
            tab_pos, borderless, always_show_tabs, old->show_add_tab_button);
    g_free(geom);
    return result;
}

static void multi_win_new_window_action(MultiWin * win)
{
    multi_win_clone(win,
            win->current_tab ?
                    win->current_tab->user_data : win->user_data_template,
            win->always_show_tabs);
}

static void multi_win_new_tab_action(MultiWin * win)
{
    multi_tab_new(win, win->current_tab ?
            win->current_tab->user_data : win->user_data_template);
}

static void multi_win_detach_tab_action(MultiWin * win)
{
    MultiTab *tab = win->current_tab;

    win = multi_win_new_for_tab(-1, -1, tab);
    multi_tab_move_to_new_window(win, tab, -1);
    gtk_widget_show_all(win->gtkwin);
}

static void multi_win_close_tab_action(MultiWin * win)
{
    multi_win_close_tab_clicked(NULL, win->current_tab);
}

static void multi_win_close_other_tabs_action(MultiWin * win)
{
    GList *link;

    while ((link = win->tabs) != NULL)
    {
        if (link->data == win->current_tab)
            link = g_list_next(link);
        if (link)
            multi_tab_delete(link->data);
        else
            break;
    }
}

static void multi_win_name_tab_action(MultiWin * win)
{
    MultiTab *tab = win->current_tab;
    g_return_if_fail(tab);
    GtkWidget *dialog_w = gtk_dialog_new_with_buttons(_("Name Tab"),
            GTK_WINDOW(win->gtkwin),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            _("_Cancel"), GTK_RESPONSE_CANCEL,
            _("_Apply"), GTK_RESPONSE_APPLY,
            NULL);
    GtkDialog *dialog = GTK_DIALOG(dialog_w);
    GtkWidget *name_w = gtk_entry_new();
    GtkEntry *name_e = GTK_ENTRY(name_w);
    const char *name = tab->window_title_template;
    int response;

    tab->rename_dialog = dialog_w;
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(dialog)),
            name_w, TRUE, TRUE, 8);
    gtk_entry_set_activates_default(name_e, TRUE);
    gtk_entry_set_text(name_e, name ? name : "");
    gtk_widget_show_all(dialog_w);
    response = gtk_dialog_run(dialog);
    switch (response)
    {
        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
            /* Don't destroy */
            break;
        case GTK_RESPONSE_APPLY:
            name = gtk_entry_get_text(name_e);
            tab->title_template_locked = FALSE;
            if (name && !name[0])
                name = NULL;
            multi_tab_set_window_title_template(tab, name);
            tab->title_template_locked = (name != NULL);
            /* Fall through to destroy */
        default:
            gtk_widget_destroy(dialog_w);
            break;
    }
    tab->rename_dialog = NULL;
    if (tab->postponed_free)
        g_free(tab);
}
static void multi_win_save_session_action(MultiWin * win)
{
    GtkWidget *dialog_w = gtk_dialog_new_with_buttons(_("Save Session"),
            GTK_WINDOW(win->gtkwin),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            _("_Cancel"), GTK_RESPONSE_CANCEL,
            _("_Save"), GTK_RESPONSE_OK,
            NULL);
    GtkDialog *dialog = GTK_DIALOG(dialog_w);
    GtkWidget *title_w = gtk_entry_new();
    GtkEntry *title_e = GTK_ENTRY(title_w);
    GtkWidget *img = gtk_image_new_from_icon_name("dialog-information",
            GTK_ICON_SIZE_DIALOG);
    GtkWidget *tip_label = gtk_label_new(_("The current session will be saved "
                "in $XDG_CONFIG_HOME/roxterm.sourceforge.net/UserSessions/ "
                "with the given leafname (XDG_CONFIG_HOME defaults to "
                "~/.config). A session saves the state of current windows "
                "and tabs but not the terminals' text content. A session "
                "can be restored with roxterm's --session command-line "
                "option, or will be restored automatically if named "
                "'Default'. Leaving the field blank is equivalent to "
                "'Default'."));
    int response;
    const char *name;
    char *filename;
    GtkWidget *content_area = gtk_dialog_get_content_area(dialog);
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);
    gtk_box_pack_start(GTK_BOX(content_area), title_w, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 8);
    gtk_label_set_line_wrap(GTK_LABEL(tip_label), TRUE);
    gtk_label_set_width_chars(GTK_LABEL(tip_label), 30);
    gtk_label_set_max_width_chars(GTK_LABEL(tip_label), 50);
    gtk_box_pack_start(GTK_BOX(hbox), tip_label, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(content_area), hbox, FALSE, FALSE, 8);
    gtk_entry_set_activates_default(title_e, TRUE);
    gtk_entry_set_text(title_e, "Default");
    gtk_widget_show_all(dialog_w);
    response = gtk_dialog_run(dialog);
    switch (response)
    {
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_NONE:
            /* Don't destroy */
            break;
        case GTK_RESPONSE_OK:
            name = gtk_entry_get_text(title_e);
            if (!name || !name[0])
                name = "Default";
            filename = session_get_filename(name, "UserSessions", TRUE);
            if (!save_session_to_file(filename, name))
            {
                dlg_critical(GTK_WINDOW(win->gtkwin),
                        _("Unable to save session to file '%s': %s"),
                        filename, strerror(errno));
            }
            /* Fall through to destroy */
        default:
            gtk_widget_destroy(dialog_w);
            break;
    }
}

static void multi_win_set_window_title_action(MultiWin * win)
{
    MultiTab *tab = win->current_tab;
    g_return_if_fail(tab);
    GtkWidget *dialog_w = gtk_dialog_new_with_buttons(_("Set Window Title"),
            GTK_WINDOW(win->gtkwin),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            _("_Cancel"), GTK_RESPONSE_CANCEL,
            _("_Apply"), GTK_RESPONSE_APPLY,
            NULL);
    GtkDialog *dialog = GTK_DIALOG(dialog_w);
    GtkWidget *title_w = gtk_entry_new();
    GtkEntry *title_e = GTK_ENTRY(title_w);
    GtkWidget *img = gtk_image_new_from_icon_name("dialog-information",
            GTK_ICON_SIZE_DIALOG);
    GtkWidget *tip_label = gtk_label_new(_("The title string may include '%s' "
           "which is substituted with the active tab's name/window title, "
           "'%t' for the active tab's number, '%n' for the number of tabs "
           "and or '%%' for a literal '%'. Other '%' sequences are passed "
           "through. Apply an empty string here to use the profile's title "
           "string."));
    int response;
    const char *title;
    GtkWidget *content_area = gtk_dialog_get_content_area(dialog);
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);
    gtk_box_pack_start(GTK_BOX(content_area), title_w, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 8);
    gtk_label_set_line_wrap(GTK_LABEL(tip_label), TRUE);
    gtk_label_set_width_chars(GTK_LABEL(tip_label), 30);
    gtk_label_set_max_width_chars(GTK_LABEL(tip_label), 50);
    gtk_box_pack_start(GTK_BOX(hbox), tip_label, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(content_area), hbox, FALSE, FALSE, 8);
    gtk_entry_set_activates_default(title_e, TRUE);
    gtk_entry_set_text(title_e, win->title_template ? win->title_template : "");
    gtk_widget_show_all(dialog_w);
    response = gtk_dialog_run(dialog);
    switch (response)
    {
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_NONE:
            /* Don't destroy */
            break;
        case GTK_RESPONSE_APPLY:
            title = gtk_entry_get_text(title_e);
            if (title && !title[0])
                title = NULL;
            win->title_template_locked = FALSE;
            multi_win_set_title_template(win, title);
            win->title_template_locked = (title != NULL);
            /* Fall through to destroy */
        default:
            gtk_widget_destroy(dialog_w);
            break;
    }
}

static void multi_win_next_tab_action(MultiWin * win)
{
    if (win->wrap_switch_tab && multi_win_at_last_tab(win))
        gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), 0);
    else
        gtk_notebook_next_page(GTK_NOTEBOOK(win->notebook));
}

static void multi_win_previous_tab_action(MultiWin * win)
{
    if (win->wrap_switch_tab && multi_win_at_first_tab(win))
    {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook),
                win->ntabs - 1);
    }
    else
    {
        gtk_notebook_prev_page(GTK_NOTEBOOK(win->notebook));
    }
}

static void multi_win_select_tab_action(GtkCheckMenuItem *widget,
        MultiTab * tab)
{
    MultiWin *win = tab->parent;

    if (win->ignore_toggles)
        return;
    if (gtk_check_menu_item_get_active(widget))
        multi_win_select_tab(win, tab);
}

static void
multi_win_toggle_show_tab_bar_action(GtkCheckMenuItem * item, MultiWin * win)
{
    gboolean show = gtk_check_menu_item_get_active(item);

    if (show != win->always_show_tabs)
        multi_win_set_always_show_tabs(win, show);
}

static void
multi_win_toggle_show_menubar_action(GtkCheckMenuItem * item, MultiWin * win)
{
    gboolean show = gtk_check_menu_item_get_active(item);

    if (show != win->show_menu_bar)
        multi_win_set_show_menu_bar(win, show);
}

static void
multi_win_toggle_fullscreen_action(GtkCheckMenuItem * item, MultiWin * win)
{
    gboolean fs = gtk_check_menu_item_get_active(item);

    if (fs != win->fullscreen)
        multi_win_set_fullscreen(win, fs);
}

static void
multi_win_toggle_borderless_action(GtkCheckMenuItem * item, MultiWin * win)
{
    gboolean fs = gtk_check_menu_item_get_active(item);

    if (fs != win->borderless)
        multi_win_set_borderless(win, fs);
}

static void multi_win_zoom_changed(MultiWin *win)
{
    double zf = multi_win_zoom_factors[win->zoom_index];
    GList *link;

    if (!multi_win_zoom_handler)
        return;
    for (link = win->tabs; link; link = g_list_next(link))
    {
        multi_win_zoom_handler(((MultiTab *) link->data)->user_data, zf,
                win->zoom_index);
    }
}

static void multi_win_zoom_in_action(MultiWin *win)
{
    if (win->zoom_index < MULTI_WIN_N_ZOOM_FACTORS - 1)
    {
        ++win->zoom_index;
        multi_win_zoom_changed(win);
    }
}

static void multi_win_zoom_out_action(MultiWin *win)
{
    if (win->zoom_index > 0)
    {
        --win->zoom_index;
        multi_win_zoom_changed(win);
    }
}

static void multi_win_zoom_norm_action(MultiWin *win)
{
    if (win->zoom_index != MULTI_WIN_NORMAL_ZOOM_INDEX)
    {
        win->zoom_index = MULTI_WIN_NORMAL_ZOOM_INDEX;
        multi_win_zoom_changed(win);
    }
}

typedef enum {
    MULTI_TAB_SCROLL_STEP_LINE,
    MULTI_TAB_SCROLL_STEP_PAGE,
    MULTI_TAB_SCROLL_STEP_END
} MultiTabScrollStep;

static void multi_win_vscroll_by(MultiWin *win, double multiplier,
        MultiTabScrollStep step_type)
{
    GtkAdjustment *adj;
    double newval;
    double bottom;

    adj = win->current_tab->adjustment;
    if (!adj)
        return;
    bottom = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
    if (step_type == MULTI_TAB_SCROLL_STEP_END)
    {
        if (multiplier < 0)
            newval = 0;
        else
            newval = bottom;
    }
    else
    {
        double step;

        if (step_type == MULTI_TAB_SCROLL_STEP_PAGE)
            step = gtk_adjustment_get_page_increment(adj);
        else
            step = gtk_adjustment_get_step_increment(adj);
        newval = gtk_adjustment_get_value(adj) + multiplier * step;
        newval = CLAMP(newval, 0, bottom);
    }
    gtk_adjustment_set_value(adj, newval);
}

static void multi_win_scroll_up_action(MultiWin *win)
{
    g_return_if_fail(win->current_tab != NULL);
    multi_win_vscroll_by(win, -1, MULTI_TAB_SCROLL_STEP_LINE);
}

static void multi_win_scroll_down_action(MultiWin *win)
{
    g_return_if_fail(win->current_tab != NULL);
    multi_win_vscroll_by(win, 1, MULTI_TAB_SCROLL_STEP_LINE);
}

static void multi_win_scroll_page_up_action(MultiWin *win)
{
    g_return_if_fail(win->current_tab != NULL);
    multi_win_vscroll_by(win, -1, MULTI_TAB_SCROLL_STEP_PAGE);
}

static void multi_win_scroll_page_down_action(MultiWin *win)
{
    g_return_if_fail(win->current_tab != NULL);
    multi_win_vscroll_by(win, 1, MULTI_TAB_SCROLL_STEP_PAGE);
}

static void multi_win_scroll_half_page_up_action(MultiWin *win)
{
    g_return_if_fail(win->current_tab != NULL);
    multi_win_vscroll_by(win, -0.5, MULTI_TAB_SCROLL_STEP_PAGE);
}

static void multi_win_scroll_half_page_down_action(MultiWin *win)
{
    g_return_if_fail(win->current_tab != NULL);
    multi_win_vscroll_by(win, 0.5, MULTI_TAB_SCROLL_STEP_PAGE);
}

static void multi_win_scroll_to_top_action(MultiWin *win)
{
    g_return_if_fail(win->current_tab != NULL);
    multi_win_vscroll_by(win, -1, MULTI_TAB_SCROLL_STEP_END);
}

static void multi_win_scroll_to_bottom_action(MultiWin *win)
{
    g_return_if_fail(win->current_tab != NULL);
    multi_win_vscroll_by(win, 1, MULTI_TAB_SCROLL_STEP_END);
}

static void multi_win_move_tab_by_one(MultiWin *win, int dir)
{
    MultiTab *tab = win->current_tab;
    int pos;

    g_return_if_fail(win->current_tab);
    pos = multi_tab_get_page_num(tab) + dir;
    if (win->wrap_switch_tab)
    {
        if (pos < 0)
            pos = win->ntabs - 1;
        else if (pos >= (int) win->ntabs)
            pos = 0;
    }
    g_return_if_fail(pos >= 0 && pos < (int) win->ntabs);
    multi_tab_move_to_position(tab, pos, TRUE);
}

static void multi_win_move_tab_left_action(MultiWin *win)
{
    multi_win_move_tab_by_one(win, -1);
}

static void multi_win_move_tab_right_action(MultiWin *win)
{
    multi_win_move_tab_by_one(win, 1);
}

static void multi_win_close_window_action(MultiWin *win)
{
    if (!multi_win_delete_handler ||
            !multi_win_delete_handler(win->gtkwin, (GdkEvent *) win, win))
    {
        multi_win_destructor(win, TRUE);
    }
}

static void multi_win_popup_new_term_with_profile(GtkMenu *menu)
{
    gtk_menu_popup_at_pointer(menu, NULL);
}

static void multi_win_connect_actions(MultiWin * win)
{
    multi_win_menu_connect_swapped(win, MENUTREE_FILE_NEW_WINDOW, G_CALLBACK
        (multi_win_new_window_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_FILE_CLOSE_WINDOW, G_CALLBACK
        (multi_win_close_window_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_FILE_NEW_TAB, G_CALLBACK
        (multi_win_new_tab_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_FILE_CLOSE_TAB, G_CALLBACK
        (multi_win_close_tab_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_FILE_SAVE_SESSION, G_CALLBACK
        (multi_win_save_session_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_TABS_DETACH_TAB, G_CALLBACK
        (multi_win_detach_tab_action), win, NULL, NULL, NULL);
    menutree_signal_connect_swapped(win->popup_menu,
            MENUTREE_FILE_NEW_WINDOW_WITH_PROFILE_HEADER,
            G_CALLBACK(multi_win_popup_new_term_with_profile),
            win->popup_menu->new_win_profiles_menu);
    menutree_signal_connect_swapped(win->popup_menu,
            MENUTREE_FILE_NEW_TAB_WITH_PROFILE_HEADER,
            G_CALLBACK(multi_win_popup_new_term_with_profile),
            win->popup_menu->new_tab_profiles_menu);
    multi_win_menu_connect_swapped(win, MENUTREE_TABS_PREVIOUS_TAB, G_CALLBACK
        (multi_win_previous_tab_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_TABS_NEXT_TAB, G_CALLBACK
        (multi_win_next_tab_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_TABS_MOVE_TAB_LEFT, G_CALLBACK
        (multi_win_move_tab_left_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_TABS_MOVE_TAB_RIGHT, G_CALLBACK
        (multi_win_move_tab_right_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_TABS_NAME_TAB, G_CALLBACK
        (multi_win_name_tab_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_TABS_CLOSE_TAB, G_CALLBACK
        (multi_win_close_tab_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_TABS_CLOSE_OTHER_TABS,
        G_CALLBACK(multi_win_close_other_tabs_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_EDIT_SET_WINDOW_TITLE,
        G_CALLBACK(multi_win_set_window_title_action), win, NULL, NULL, NULL);
    multi_win_menu_connect(win, MENUTREE_VIEW_SHOW_TAB_BAR, G_CALLBACK
        (multi_win_toggle_show_tab_bar_action), win, NULL, NULL, NULL);
    multi_win_menu_connect(win, MENUTREE_VIEW_SHOW_MENUBAR, G_CALLBACK
        (multi_win_toggle_show_menubar_action), win, NULL, NULL, NULL);
    multi_win_menu_connect(win, MENUTREE_VIEW_FULLSCREEN, G_CALLBACK
        (multi_win_toggle_fullscreen_action), win, NULL, NULL, NULL);
    multi_win_menu_connect(win, MENUTREE_VIEW_BORDERLESS, G_CALLBACK
        (multi_win_toggle_borderless_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_ZOOM_IN, G_CALLBACK
        (multi_win_zoom_in_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_ZOOM_OUT, G_CALLBACK
        (multi_win_zoom_out_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_ZOOM_NORM, G_CALLBACK
        (multi_win_zoom_norm_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_SCROLL_UP,
        G_CALLBACK(multi_win_scroll_up_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_SCROLL_DOWN,
        G_CALLBACK(multi_win_scroll_down_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_SCROLL_PAGE_UP,
        G_CALLBACK(multi_win_scroll_page_up_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_SCROLL_PAGE_DOWN,
        G_CALLBACK(multi_win_scroll_page_down_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_SCROLL_HALF_PAGE_UP,
        G_CALLBACK(multi_win_scroll_half_page_up_action), win,
        NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_SCROLL_HALF_PAGE_DOWN,
        G_CALLBACK(multi_win_scroll_half_page_down_action), win,
        NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_SCROLL_TO_TOP,
        G_CALLBACK(multi_win_scroll_to_top_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_SCROLL_TO_BOTTOM,
        G_CALLBACK(multi_win_scroll_to_bottom_action), win, NULL, NULL, NULL);
}

static void multi_win_set_show_tabs_menu_items(MultiWin *win, gboolean active)
{
    menutree_set_show_tab_bar_active(win->menu_bar, active);
    menutree_set_show_tab_bar_active(win->popup_menu, active);
}

static void multi_win_show_tabs(MultiWin * win)
{
    if ((int) win->tab_pos != -1)
    {
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook), TRUE);
        gtk_notebook_set_show_border(GTK_NOTEBOOK(win->notebook), TRUE);
    }
}

static void multi_win_hide_tabs(MultiWin * win)
{
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(win->notebook), FALSE);
}

void multi_win_show(MultiWin *win)
{
    gtk_widget_show(win->notebook);
    gtk_widget_show(win->vbox);
    gtk_widget_show(win->gtkwin);
    g_object_set_data(G_OBJECT(gtk_widget_get_window(win->gtkwin)),
            "ROXTermWin", win);

    //g_debug("call roxterm_force_resize_now from line %d", __LINE__);
    //roxterm_force_resize_now(win);
}

void multi_win_set_colormap(MultiWin *win)
{
    GdkScreen *screen = gtk_widget_get_screen(win->gtkwin);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);

    if (gdk_screen_is_composited(screen) && visual)
    {
        gtk_widget_set_visual(win->gtkwin, visual);
        win->composite = TRUE;
    }
    else
    {
        gtk_widget_set_visual(win->gtkwin,
                gdk_screen_get_system_visual(screen));
        win->composite = FALSE;
    }
}

/* Close window explicitly instead of allowing system to close it by returning
 * FALSE from delete-event handler. This cures
 * https://sourceforge.net/p/roxterm/bugs/89/
 * but I don't really know why.
 */
static gboolean multi_win_delete_event_cb(GtkWidget *widget, GdkEvent *event,
        MultiWin *win)
{
    if (!multi_win_delete_handler(widget, event, win))
        multi_win_destructor(win, TRUE);
    return TRUE;
}

MultiWin *multi_win_new_blank(Options *shortcuts,
        int zoom_index,
        gboolean disable_menu_shortcuts, gboolean disable_tab_shortcuts,
        GtkPositionType tab_pos, gboolean always_show_tabs,
        gboolean add_tab_button)
{
    MultiWin *win = g_new0(MultiWin, 1);
    GtkNotebook *notebook;
    char *role = NULL;

    win->best_tab_width = G_MAXINT;
    win->tab_pos = tab_pos;
    win->always_show_tabs = always_show_tabs;
    win->scroll_bar_pos = MultiWinScrollBar_Query;
    if (zoom_index < 0)
        win->zoom_index = MULTI_WIN_NORMAL_ZOOM_INDEX;
    else if (zoom_index >= MULTI_WIN_N_ZOOM_FACTORS)
        win->zoom_index = multi_win_zoom_factors[MULTI_WIN_N_ZOOM_FACTORS - 1];
    else
        win->zoom_index = zoom_index;

    win->gtkwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    role = global_options_lookup_string("role");
    if (role)
    {
        global_options_reset_string("role");
    }
    else if (multi_win_role_prefix)
    {
        role = g_strdup_printf("%s-%d-%x-%d", multi_win_role_prefix,
                getpid(), g_random_int(), ++multi_win_role_index);
    }
    if (role)
    {
        gtk_window_set_role(GTK_WINDOW(win->gtkwin), role);
        g_free(role);
    }

    g_signal_connect(win->gtkwin, "realize",
            G_CALLBACK(multi_win_realize_handler), win);
    win->destroy_handler = g_signal_connect(win->gtkwin, "destroy",
            G_CALLBACK(multi_win_destroy_handler), win);
    if (multi_win_delete_handler)
    {
        g_signal_connect(win->gtkwin, "delete-event",
                G_CALLBACK(multi_win_delete_event_cb), win);
    }
    g_signal_connect(win->gtkwin, "draw",
            G_CALLBACK(multi_win_draw_chrome_bg), win);

    multi_win_set_colormap(win);

    /* This on its own seems to allow transparency to work correctly in all of
     * Classic, Xorg and Wayland GNOME variants. The "draw" signal handler
     * broke the shadows outside the window in Wayland.
     */
    gtk_widget_set_app_paintable(win->gtkwin, TRUE);

    win->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(win->gtkwin), win->vbox);

    options_ref(shortcuts);
    win->shortcuts = shortcuts;
    win->accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(win->gtkwin), win->accel_group);

    win->popup_menu = menutree_new(shortcuts, win->accel_group,
        GTK_TYPE_MENU, TRUE, disable_tab_shortcuts, win);
    //g_debug("Created popup menu %p", win->popup_menu);
    menutree_connect_destroyed(win->popup_menu,
        G_CALLBACK(multi_win_menutree_deleted_handler), win);

    win->short_popup = menutree_new_short_popup(shortcuts, win->accel_group,
        TRUE, win);
    menutree_connect_destroyed(win->short_popup,
        G_CALLBACK(multi_win_menutree_deleted_handler), win);
    //g_debug("Created short popup menu %p", win->short_popup);

    win->menu_bar = menutree_new(shortcuts, win->accel_group,
        GTK_TYPE_MENU_BAR, disable_menu_shortcuts, disable_tab_shortcuts,
        win);
    //g_debug("Created menu bar %p", win->menu_bar);
    /* Make sure menu widgets aren't destroyed when removed from window */
    g_object_ref(menutree_get_top_level_widget(win->menu_bar));
    menutree_connect_destroyed(win->menu_bar,
        G_CALLBACK(multi_win_menutree_deleted_handler), win);
    win->show_menu_bar = FALSE;

    if (win->tab_pos == GTK_POS_LEFT || win->tab_pos == GTK_POS_RIGHT)
    {
        menutree_change_move_tab_labels(win->popup_menu);
        menutree_change_move_tab_labels(win->menu_bar);
    }

    multi_win_connect_actions(win);
    (*multi_win_menu_signal_connector) (win);
    g_signal_connect(win->gtkwin, "window-state-event",
        G_CALLBACK(multi_win_state_event_handler), win);

    win->notebook = gtk_notebook_new();
    notebook = GTK_NOTEBOOK(win->notebook);
    g_signal_connect(notebook, "create-window",
            G_CALLBACK(multi_win_notebook_creation_hook), win);
    gtk_notebook_set_scrollable(notebook, TRUE);
    if ((int) tab_pos == -1)
    {
        gtk_notebook_set_show_tabs(notebook, always_show_tabs);
        gtk_notebook_set_show_border(notebook, always_show_tabs);
    }
    else
    {
        gtk_notebook_set_tab_pos(notebook, tab_pos);
    }
    gtk_notebook_popup_disable(notebook);
    if (always_show_tabs)
    {
        multi_win_set_show_tabs_menu_items(win, TRUE);
        multi_win_show_tabs(win);
    }
    else
    {
        multi_win_set_show_tabs_menu_items(win, FALSE);
        multi_win_hide_tabs(win);
    }
    win->add_tab_button = multitab_close_button_new("tab-new-symbolic");
    g_signal_connect_swapped(win->add_tab_button, "clicked",
            G_CALLBACK(multi_win_new_tab_action), win);
    gtk_notebook_set_action_widget(notebook, win->add_tab_button, GTK_PACK_END);
    if (add_tab_button)
        gtk_widget_show_all(win->add_tab_button);
    win->show_add_tab_button = add_tab_button;
    gtk_box_pack_start(GTK_BOX(win->vbox), win->notebook, TRUE, TRUE, 0);
    g_signal_connect(win->notebook, "switch-page",
        G_CALLBACK(multi_win_page_switched), win);
    g_signal_connect(win->gtkwin, "focus-in-event",
        G_CALLBACK(multi_win_focus_in), win);
    gtk_notebook_set_group_name(notebook, "ROXTerm");
    g_signal_connect(win->notebook, "page-reordered",
        G_CALLBACK(page_reordered_callback), win);
    g_signal_connect(win->notebook, "page-added",
        G_CALLBACK(page_added_callback), win);
    /* VTE widgets handle these events, so we only get them if they occur over
     * the actual sticky out bit */

    multi_win_all = g_list_append(multi_win_all, win);

    return win;
}

void multi_win_set_role_prefix(const char *role_prefix)
{
    g_free(multi_win_role_prefix);
    multi_win_role_prefix = g_strdup(role_prefix);
}

MultiWin *multi_win_new_full(Options *shortcuts,
        int zoom_index, gpointer user_data_template, const char *geom,
        MultiWinSizing sizing, GtkPositionType tab_pos, gboolean borderless,
        gboolean always_show_tabs, gboolean add_tab_button)
{
    gboolean disable_menu_shortcuts, disable_tab_shortcuts;
    MultiWin *win;
    MultiTab *tab;

    multi_win_get_disable_menu_shortcuts(user_data_template,
            &disable_menu_shortcuts, &disable_tab_shortcuts);
    win = multi_win_new_blank(shortcuts, zoom_index,
            disable_menu_shortcuts, disable_tab_shortcuts,
            tab_pos, always_show_tabs, add_tab_button);
    win->user_data_template = user_data_template;
    win->tab_pos = tab_pos;
    tab = multi_tab_new_defer_connect(win, user_data_template);

    multi_win_shade_menus_for_tabs(win);
    multi_win_set_borderless(win, borderless);

    /* Showing everything except top-level, then realizing top-level, seems to
     * be the key to getting some sensible size allocations so we can work out
     * how much size the "chrome" needs.
     */
    gtk_widget_show_all(win->vbox);
    gtk_widget_realize(win->gtkwin);
    if (geom)
    {
        multi_win_set_initial_geometry(win, geom, tab);
    }
    if (sizing == MULTI_WIN_FULL_SCREEN)
    {
        multi_win_set_fullscreen(win, TRUE);
    }
    else if (sizing == MULTI_WIN_MAXIMISED)
    {
        gtk_window_maximize(GTK_WINDOW(win->gtkwin));
    }
    multi_win_show(win);

    /* Need to do this after multi_tab_new's initial call of
     * multi_win_select_tab to ensure child widgets are realized
     * and tab selection handler is activated */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), 0);
    tab = win->tabs->data;
    win->tab_selection_handler(tab->user_data, tab);
    multi_tab_connect_misc_signals(tab->user_data);
    return win;
}

static void multi_win_destructor(MultiWin *win, gboolean destroy_widgets)
{
    GList *link;

    g_return_if_fail(win);

    win->ignore_tab_selections = TRUE;
    if (win->accel_group)
    {
        UNREF_LOG(g_object_unref(win->accel_group));
        win->accel_group = NULL;
    }
    if (destroy_widgets && win->gtkwin)
    {
        g_signal_handler_disconnect(win->gtkwin, win->destroy_handler);
    }

    if (!destroy_widgets)
    {
        win->gtkwin = NULL;
    }
    for (link = win->tabs; link; link = g_list_next(link))
    {
        multi_tab_delete_without_notifying_parent(link->data, destroy_widgets);
    }
    if (win->menu_bar)
    {
        /* Remember we added an extra reference to this when we created window,
         * so it will now be removed from window but not destroyed. Have to
         * destroy it explicitly.
         */
        UNREF_LOG(menutree_delete(win->menu_bar));
        win->menu_bar = NULL;
    }
    if (win->popup_menu)
    {
        UNREF_LOG(menutree_delete(win->popup_menu));
        win->popup_menu = NULL;
    }
    if (win->short_popup)
    {
        UNREF_LOG(menutree_delete(win->short_popup));
        win->short_popup = NULL;
    }
    if (destroy_widgets && win->gtkwin)
    {
        gtk_widget_destroy(win->gtkwin);
        win->gtkwin = NULL;
    }
    UNREF_LOG(options_unref(win->shortcuts));
    g_free(win->title_template);
    g_free(win->child_title);
    g_free(win);
    multi_win_all = g_list_remove(multi_win_all, win);
    if (!multi_win_all)
    {
        gtk_main_quit();
    }
}

void multi_win_delete(MultiWin *win)
{
    multi_win_destructor(win, TRUE);
}

/* Returns TRUE if window destroyed */
static gboolean multi_win_notify_tab_removed(MultiWin * win, MultiTab * tab)
{
    GList *link = g_list_find(win->tabs, tab);

    g_return_val_if_fail(link, FALSE);
    /* GtkNotebook event will have dealt with new tab selection for us but we
     * need to ensure we don't respond to spurious events from deleting menu
     * items */
    if (win->current_tab == tab)
    {
        win->current_tab = NULL;
    }
    /*win->ignore_tab_selections = TRUE;*/
    win->tabs = g_list_delete_link(win->tabs, link);
    /*win->ignore_tab_selections = FALSE;*/
    if (!--win->ntabs)
    {
        multi_win_delete(win);
        return TRUE;
    }
    else
    {
        renumber_tabs(win);
        if (win->ntabs == 1)
        {
            tab = win->tabs->data;
            if (win->tab_pos == GTK_POS_TOP || win->tab_pos == GTK_POS_BOTTOM)
                multi_win_pack_for_single_tab(win);
            if (!win->always_show_tabs)
            {
                multi_win_request_size_restore(win);
                multi_win_hide_tabs(win);
            }
        }
    }
    multi_win_shade_menus_for_tabs(win);
    return FALSE;
}

/* Adding or removeing close buttons could cause unwanted resizes, but might
 * not, safer to not try to do anything about it.
 */

void multi_tab_add_close_button(MultiTab *tab)
{
    if (tab->close_button)
        return;

    tab->close_button = multitab_close_button_new(tab->status_icon_name);
    gtk_box_pack_start(GTK_BOX(tab->label_box), tab->close_button,
            FALSE, FALSE, 0);
    g_signal_connect(tab->close_button, "clicked",
            G_CALLBACK(multi_win_close_tab_clicked), tab);
    gtk_widget_show(tab->close_button);
}

void multi_tab_remove_close_button(MultiTab *tab)
{
    if (tab->close_button)
    {
        gtk_widget_destroy(tab->close_button);
        tab->close_button = NULL;
    }
}

void multi_tab_set_status_icon_name(MultiTab *tab, const char *name)
{
    if (!strcmp(tab->status_icon_name ? tab->status_icon_name : "window-close",
            name ? name : "window-close"))
    {
        return;
    }
    tab->status_icon_name = name;
    if (tab->close_button)
    {
        multitab_close_button_set_image(
                MULTITAB_CLOSE_BUTTON(tab->close_button), name);
    }
}

void multi_tab_set_middle_click_tab_action(MultiTab *tab, int action)
{
    tab->middle_click_action = action;
}

static void multi_tab_add_menutree_items(MultiWin * win, MultiTab * tab,
        int position)
{
    char *title = multi_tab_get_full_window_title(tab);
    gboolean has_num = g_str_has_prefix(tab->window_title_template, "%t. ");
    char *n_and_title = has_num ?
            g_strdup_printf("%d. %s", multi_tab_get_page_num(tab), title) :
            title;

    if (has_num)
        g_free(title);
    tab->popup_menu_item = menutree_add_tab_at_position(win->popup_menu,
            n_and_title, position);
    tab->menu_bar_item = menutree_add_tab_at_position(win->menu_bar,
            n_and_title, position);
    g_free(n_and_title);
    g_signal_connect(tab->popup_menu_item, "toggled",
        G_CALLBACK(multi_win_select_tab_action), tab);
    g_signal_connect(tab->menu_bar_item, "toggled",
        G_CALLBACK(multi_win_select_tab_action), tab);
    if (win->current_tab == tab)
    {
        menutree_select_tab(win->menu_bar, tab->menu_bar_item);
        menutree_select_tab(win->popup_menu, tab->popup_menu_item);
    }
}

static void multi_win_add_tab_to_notebook(MultiWin * win, MultiTab * tab,
        int position)
{
    GtkWidget *label = make_tab_label(tab, win->tab_pos);
    GtkNotebook *notebook = GTK_NOTEBOOK(win->notebook);

    if (position == -1)
    {
        gtk_notebook_append_page(notebook, tab->widget, label);
    }
    else
    {
        gtk_notebook_insert_page(notebook, tab->widget, label, position);
    }
    gtk_notebook_set_tab_reorderable(notebook, tab->widget, TRUE);
    gtk_notebook_set_tab_detachable(notebook, tab->widget, TRUE);
    g_object_set(gtk_widget_get_parent(label), "can-focus", FALSE, NULL);
    /* Note at this point ntabs is how many tabs there are about to be,
     * not how many there were before adding.
     */
    if (win->tab_pos == GTK_POS_TOP || win->tab_pos == GTK_POS_BOTTOM)
    {
        if (win->ntabs == 1)
            multi_win_pack_for_single_tab(win);
        else if (win->ntabs == 2)
            multi_win_pack_for_multiple_tabs(win);
        else
            multi_tab_pack_for_multiple(tab, GTK_CONTAINER(notebook));
    }
    else
    {
        multi_tab_pack_for_horizontal(tab, GTK_CONTAINER(notebook));
    }
}

/* Keep track of a tab after it's been created; if notify_only is set, it's
 * assumed the tab has already been added by GTK and this is being called
 * just for notification */
static void multi_win_add_tab(MultiWin * win, MultiTab * tab, int position,
        gboolean notify_only)
{
    tab->parent = win;
    if (tab->label)
    {
        multitab_label_set_parent(MULTITAB_LABEL(tab->label),
                tab->parent->notebook, &tab->parent->best_tab_width);
        multi_tab_set_full_window_title(tab);
    }
    win->ignore_tabs_moving = TRUE;
    if (position == -1)
        win->tabs = g_list_append(win->tabs, tab);
    else
        win->tabs = g_list_insert(win->tabs, tab, position);
    ++win->ntabs;
    if (win->ntabs == 2 && !win->always_show_tabs)
    {
        multi_win_request_size_restore(win);
        multi_win_show_tabs(win);
    }
    multi_tab_add_menutree_items(win, tab, position);
    if (!notify_only)
    {
        multi_win_add_tab_to_notebook(win, tab, position);
        gtk_widget_show(tab->widget);
        if (win->ntabs > 1 && gtk_widget_get_visible(win->gtkwin))
            gtk_window_present(GTK_WINDOW(win->gtkwin));
    }
    renumber_tabs(win);
    multi_win_shade_menus_for_tabs(win);
    multi_win_select_tab(win, tab);
    win->ignore_tabs_moving = FALSE;
}

GtkWidget *multi_win_get_widget(MultiWin * win)
{
    return win->gtkwin;
}

gboolean multi_win_is_fullscreen(MultiWin *win)
{
    return win->fullscreen;
}

gboolean multi_win_is_maximised(MultiWin *win)
{
    GdkWindow *w = gtk_widget_get_window(win->gtkwin);

    return (w && (gdk_window_get_state(w) & GDK_WINDOW_STATE_MAXIMIZED) != 0);
}

gboolean multi_win_is_borderless(MultiWin *win)
{
    return win->borderless;
}

int multi_win_get_zoom_index(MultiWin *win)
{
    return win->zoom_index;
}

double multi_win_get_zoom_factor(MultiWin *win)
{
    int i = win->zoom_index;

    if (i == -1)
        i = MULTI_WIN_NORMAL_ZOOM_INDEX;
    return multi_win_zoom_factors[i];
}

int multi_win_get_nearest_index_for_zoom(double factor)
{
    int i;

    for (i = 0; i < MULTI_WIN_N_ZOOM_FACTORS; ++i)
    {
        if (multi_win_zoom_factors[i] >= factor)
            return i;
    }
    return i - 1;
}

void
multi_win_menu_connect_data(MultiWin *win, MenuTreeID id,
    GCallback handler, gpointer user_data, GConnectFlags flags,
    gulong *popup_id, gulong *bar_id, gulong *short_popup_id)
{
    int handler_id;

    g_return_if_fail(win);
    handler_id = menutree_signal_connect_data(win->popup_menu, id, handler,
        user_data, flags);
    if (popup_id)
        *popup_id = handler_id;
    handler_id = menutree_signal_connect_data(win->menu_bar, id, handler,
        user_data, flags);
    if (bar_id)
        *bar_id = handler_id;
    handler_id = menutree_signal_connect_data(win->short_popup, id, handler,
        user_data, flags);
    if (short_popup_id)
        *short_popup_id = handler_id;
}

MultiTab *multi_win_get_current_tab(MultiWin * win)
{
    return win->current_tab;
}

guint multi_win_get_ntabs(MultiWin * win)
{
    return win->ntabs;
}

gpointer multi_win_get_user_data_for_current_tab(MultiWin * win)
{
    if (!win->current_tab)
        return NULL;
    return win->current_tab->user_data;
}

MenuTree *multi_win_get_menu_bar(MultiWin * win)
{
    return win->menu_bar;
}

MenuTree *multi_win_get_popup_menu(MultiWin * win)
{
    return win->popup_menu;
}

MenuTree *multi_win_get_short_popup_menu(MultiWin * win)
{
    return win->short_popup;
}

Options *multi_win_get_shortcut_scheme(MultiWin * win)
{
    return win->shortcuts;
}

void multi_win_set_shortcut_scheme(MultiWin *win, Options *shortcuts)
{
    if (win->shortcuts && win->shortcuts != shortcuts)
    {
        UNREF_LOG(shortcuts_unref(win->shortcuts));
    }
    if (win->shortcuts != shortcuts)
    {
        options_ref(shortcuts);
        win->shortcuts = shortcuts;
    }
    if (win->menu_bar)
        menutree_apply_shortcuts(win->menu_bar, shortcuts);
    if (win->popup_menu)
        menutree_apply_shortcuts(win->popup_menu, shortcuts);
    if (win->short_popup)
        menutree_apply_shortcuts(win->short_popup, shortcuts);
}

GtkAccelGroup *multi_win_get_accel_group(MultiWin * win)
{
    return win->accel_group;
}

MultiWinScrollBar_Position multi_win_set_scroll_bar_position(MultiWin * win,
    MultiWinScrollBar_Position new_pos)
{
    if (win->scroll_bar_pos == MultiWinScrollBar_Query)
        win->scroll_bar_pos = new_pos;
    return win->scroll_bar_pos;
}

GtkNotebook *multi_win_get_notebook(MultiWin *win)
{
    return GTK_NOTEBOOK(win->notebook);
}

void multi_win_restore_focus(MultiWin *win)
{
    if (win->current_tab)
        gtk_widget_grab_focus(win->current_tab->active_widget);
}

void multi_win_set_ignore_toggles(MultiWin *win, gboolean ignore)
{
    win->ignore_toggles = ignore;
}

gboolean multi_win_get_ignore_toggles(MultiWin *win)
{
    return win->ignore_toggles;
}

void multi_win_set_wrap_switch_tab(MultiWin *win, gboolean wrap)
{
    win->wrap_switch_tab = wrap;
    multi_win_shade_for_next_and_previous_tab(win);
}

void multi_win_foreach_tab(MultiWin *win, MultiWinForEachTabFunc func,
        gpointer user_data)
{
    GList *link;

    for (link = win->tabs; link; link = g_list_next(link))
    {
        (*func)(link->data, user_data);
    }
}

GtkPositionType multi_win_get_tab_pos(MultiWin *win)
{
    return win->tab_pos;
}

gboolean multi_win_get_always_show_tabs(MultiWin *win)
{
    return win->always_show_tabs;
}

void multi_win_set_always_show_tabs(MultiWin *win, gboolean show)
{
    gboolean old_show = win->always_show_tabs;

    win->always_show_tabs = show;
    multi_win_set_show_tabs_menu_items(win, show);
    if (win->ntabs == 1 && old_show != show)
    {
        MultiTab *tab = win->current_tab;

        g_return_if_fail(tab != NULL);
        multi_win_request_size_restore(win);
        if (show)
            multi_win_show_tabs(win);
        else
            multi_win_hide_tabs(win);
    }
}

gboolean multi_win_get_show_add_tab_button(MultiWin *win)
{
    return win->show_add_tab_button;
}

void multi_win_set_show_add_tab_button(MultiWin *win, gboolean show)
{
    win->show_add_tab_button = show;
    if (show)
        gtk_widget_show_all(win->add_tab_button);
    else
        gtk_widget_hide(win->add_tab_button);
}

void multi_win_set_title_template(MultiWin *win, const char *tt)
{
    if (win->title_template_locked)
        return;
    g_free(win->title_template);
    win->title_template = tt ? g_strdup(tt) : NULL;
    multi_win_set_full_title(win);
}

void multi_win_set_title_template_locked(MultiWin *win, gboolean locked)
{
    win->title_template_locked = locked;
}

gboolean multi_win_get_title_template_locked(MultiWin *win)
{
    return win->title_template_locked;
}

const char *multi_win_get_title_template(MultiWin *win)
{
    return win->title_template;
}

const char *multi_win_get_title(MultiWin *win)
{
    return win->child_title;
}

gboolean multi_win_composite(MultiWin *win)
{
    return gdk_screen_is_composited(gtk_widget_get_screen(win->gtkwin));
}

const char *multi_win_get_shortcuts_scheme_name(MultiWin *win)
{
    char *slash = strrchr(win->shortcuts->name, G_DIR_SEPARATOR);

    return slash ? slash + 1 : win->shortcuts->name;
}

guint multi_win_get_num_tabs(MultiWin *win)
{
    return g_list_length(win->tabs);
}

/* Parse a sequence of digits from a geometry string.
 * Return pointer to first non-digit or NULL if invalid. */
static const char *parse_digits(const char *geom, int *num)
{
    unsigned long ul = 0;
    size_t len = geom ? strspn(geom, "0123456789") : 0;
    if (0 < len && 1 == sscanf(geom, "%lu", &ul) && ul <= G_MAXINT)
    {
        *num = (int) ul;
        return geom + len;
    }
    return NULL;
}

/* Parse a signed integer, which might be preceded by a plus or minus.
 * Return NULL if invalid, or a pointer to where the parsing ended. */
static const char *parse_signed(const char *geom, int *num)
{
    char c = *geom;
    if (c == '+' || c == '-')
        geom++;
    geom = parse_digits(geom, num);
    if (geom && c == '-')
    {
        *num = (0 - *num);
    }
    return geom;
}

/* Parse a pair of signed integers, each must be preceded by a plus-sign.
 * Return NULL if invalid, or a pointer to where the parsing ended. */
static const char *parse_geom_offsets(const char *g, int *x, int *y)
{
    if (g && *g == '+')
    {
        g = parse_signed(g + 1, x);
        if (g && *g == '+')
        {
            return parse_signed(g + 1, y);
        }
    }
    return NULL;
}

/* Parse a geometry string WxH[+X+Y]. If pxy is non-null, then also
 * parse a position +X+Y. Return TRUE for WxH and set pxy for +X+Y. */
gboolean multi_win_parse_geometry(const char *geom,
        int *width, int *height, int *x, int *y, gboolean *pxy)
{
    geom = parse_digits(geom, width);
    if (geom && strchr("xX", *geom))
    {
        geom = parse_digits(geom + 1, height);
        if (geom && (!*geom || strchr("+-", *geom)))
        {
            if (pxy)
            {
                *pxy = (parse_geom_offsets(geom, x, y) != NULL);
            }
            return TRUE;
        }
    }
    return FALSE;
}

/* Returns TRUE if the geometry hints have changed. */
static gboolean multi_win_process_geometry(MultiWin *win,
        MultiTab *tab, int columns, int rows, int *width, int *height)
{
    GdkGeometry geom;
    GdkWindowHints hint_mask;
    int bw, bh;
    int gw, gh;
    int ww, wh;

    //g_debug("Processing geometry %dx%d", columns, rows);
    if (!tab)
        tab = win->current_tab;
    multi_win_geometry_func(tab->user_data, &geom, &hint_mask);
    /*
    g_debug("VTE cell size %dx%d, padding %dx%d",
            geom.width_inc, geom.height_inc,
            geom.base_width, geom.base_height);
    */

    /* Get difference in size between toplevel window and the geometry
     * widget.
     */
    gw = gtk_widget_get_allocated_width(tab->active_widget);
    gh = gtk_widget_get_allocated_height(tab->active_widget);
    /* Setting the size of a window excludes its decorations, so get the
     * "chrome" size from the vbox.
     */
    bw = gtk_widget_get_allocated_width(win->vbox);
    bh = gtk_widget_get_allocated_height(win->vbox);
    //g_debug("Terminal allocation %dx%d, undecorated window allocation %dx%d",
    //        gw, gh, bw, bh);
    if (gw <= 1 || gh <= 1)
    {
        return FALSE;
    }
    /*
    g_debug("Want to allocate %dx%d to terminal", 
            columns * geom.width_inc + geom.base_width,
            rows * geom.height_inc + geom.base_height);
    */

    geom.base_width += bw - gw;
    geom.base_height += bh - gh;
    //g_debug("Additional padding %dx%d, total %dx%d", bw - gw, bh - gh,
    //        geom.base_width, geom.base_height);
    *width = columns * geom.width_inc + geom.base_width;
    *height = rows * geom.height_inc + geom.base_height;
    //g_debug("Desired size including chrome but not CSD: %dx%d",
    //        *width, *height);
    /* From gnome-terminal's code I deduced that gtk_window_set_default_size
     * etc should exclude the window decorations, but the geometry hints should
     * include them. This seems to fix roxterm's sizing :).
     */
    ww = gtk_widget_get_allocated_width(win->gtkwin);
    wh = gtk_widget_get_allocated_height(win->gtkwin);
    //g_debug("Window allocation %dx%d, CSD %dx%d", ww, wh, ww - bw, wh - bh);
    geom.min_width += ww - gw;
    geom.min_height += wh - gh;
    geom.base_width += ww - bw;
    geom.base_height += wh - bh;
    if (geom.min_width != win->geom_hints.min_width ||
        geom.min_height != win->geom_hints.min_height ||
        geom.base_width != win->geom_hints.base_width ||
        geom.base_height != win->geom_hints.base_height ||
        geom.width_inc != win->geom_hints.width_inc ||
        geom.height_inc != win->geom_hints.height_inc)
    {
        //g_debug("Updating geometry");
        win->geom_hints = geom;
        win->geom_hint_mask = hint_mask;
        return TRUE;
    }
    //g_debug("Geometry unchanged");
    return FALSE;
}

void multi_win_set_initial_geometry(MultiWin *win, const char *geom,
        MultiTab *tab)
{
    int columns, rows, x, y, width, height;
    gboolean xy;

    if (multi_win_parse_geometry(geom, &columns, &rows, &x, &y, &xy))
    {
        multi_win_process_geometry(win, tab, columns, rows, &width, &height);
        multi_win_apply_geometry_hints(win);
        gtk_window_set_default_size(GTK_WINDOW(win->gtkwin), width, height);
        /* Honor initial position on launch: it is indispensable and ubiquitous */
        if (xy)
        {
            gtk_window_move(GTK_WINDOW(win->gtkwin), x, y);
        }
    }
}

void multi_win_apply_new_geometry(MultiWin *win, int columns, int rows,
        MultiTab *tab)
{
    int width, height;
    int old_width, old_height;
    GdkWindowState state = gdk_window_get_state(
            gtk_widget_get_window(win->gtkwin));

    if (state & WIN_STATE_SNAPPED)
    {
        //g_debug("Ignoring geometry change for maximized or similar state");
        return;
    }

    //g_debug("Processing new geometry for %dx%d columns and rows",
    //        columns, rows);
    multi_win_process_geometry(win, tab, columns, rows, &width, &height);
    gtk_window_get_size(GTK_WINDOW(win->gtkwin), &old_width, &old_height);
    //g_debug("Desired window size %dx%d, currently %dx%d",
    //        width, height, old_width, old_height);
    if (width != old_width || height != old_height)
    {
        gtk_window_set_geometry_hints(GTK_WINDOW(win->gtkwin), NULL, NULL, 0);
        gtk_window_resize(GTK_WINDOW(win->gtkwin), width, height);
        multi_win_apply_geometry_hints(win);
    }
}

/* This is maintained in order of most recently focused */
GList *multi_win_all = NULL;

/* vi:set sw=4 ts=4 et cindent cino= */
