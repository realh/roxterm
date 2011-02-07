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

#include "dlg.h"
#include "display.h"
#include "globalopts.h"
#include "menutree.h"
#include "multitab.h"
#include "shortcuts.h"

#define HAVE_COMPOSITE GTK_CHECK_VERSION(2, 10, 0)

#define HORIZ_TAB_WIDTH 120

struct MultiTab {
    MultiWin *parent;
    GtkWidget *widget;            /* Top-level widget in notebook */
    char *window_title;
    char *window_title_template;
    char *icon_title;
    GtkWidget *popup_menu_item, *menu_bar_item;
    GtkWidget *active_widget;
    gpointer user_data;
    GtkWidget *label;
    GtkWidget *label_bg;
    GtkWidget *menu_label;
    GtkAdjustment *adjustment;
    double scroll_step;
    gboolean attention;
    guint attention_tag;
    GtkWidget *close_button;
    GtkWidget *label_box;
    void (*label_packer)(GtkBox *, GtkWidget *, gboolean, gboolean, guint);
    const char *status_stock;
    GtkWidget *image;
    GtkWidget *rename_dialog;
    gboolean postponed_free;
    gboolean old_win_destroyed;
    gboolean title_template_locked;
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
    gboolean fullscreen;
    char *title_template;
    char *default_title_template;
    char *child_title;
    char *child_icon;
#if HAVE_COMPOSITE
    gboolean composite;
#endif
    char *display_name;
    gboolean title_template_locked;
    char *xclass, *xname;
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
static MultiWinInitialTabs multi_win_initial_tabs;
static MultiWinDeleteHandler multi_win_delete_handler;
static MultiTabGetShowCloseButton multi_tab_get_show_close_button;

static gboolean multi_win_notify_tab_removed(MultiWin *, MultiTab *);

static void multi_win_add_tab(MultiWin *, MultiTab *, int position,
        gboolean notify_only);

static void multi_tab_add_menutree_items(MultiWin * win, MultiTab * tab,
        int position);

static void multi_win_select_tab_action(GtkCheckMenuItem *widget,
        MultiTab * tab);

static void multi_win_destructor(MultiWin *win, gboolean destroy_widgets);

static void multi_win_set_icon_title(MultiWin *win, const char *title);

static void multi_win_restore_size(MultiWin *win)
{
    MultiTab *tab = win->current_tab;
    int w = -1;
    int h = -1;
    GtkRequisition win_rq, term_rq;
    
    if (!tab || !tab->active_widget || !GTK_WIDGET_REALIZED(tab->active_widget))
        return;
    if (multi_win_is_maximised(win) || multi_win_is_fullscreen(win))
        return;
    /* Find new difference between window and term widget (padding) */
    gtk_widget_size_request(win->gtkwin, &win_rq);
    gtk_widget_size_request(tab->active_widget, &term_rq);
    /*
    g_debug("Window request %dx%d, term %dx%d",
            win_rq.width, win_rq.height, term_rq.width, term_rq.height);
    */
    /* What size is the terminal supposed to be? */
    multi_win_size_func(tab->user_data, TRUE, &w, &h);
    /* Resize window to terminal's previous size + new padding */
    gtk_window_resize(GTK_WINDOW(win->gtkwin),
            w + win_rq.width - term_rq.width,
            h + win_rq.height - term_rq.height);
}

MultiTab *multi_tab_get_from_widget(GtkWidget *widget)
{
    return g_object_get_data(G_OBJECT(widget), "roxterm_tab");
}


void multi_tab_popup_menu(MultiTab * tab, guint button, guint32 event_time)
{
    GtkMenu *menu = GTK_MENU(menutree_get_top_level_widget
            (tab->parent->show_menu_bar ? tab->parent->short_popup :
            tab->parent->popup_menu));
    
    gtk_menu_set_screen(menu, gtk_widget_get_screen(tab->widget));
    gtk_menu_popup(menu, NULL, NULL, NULL, NULL, button, event_time);
}


void multi_tab_init(MultiTabFiller filler, MultiTabDestructor destructor,
    MultiWinMenuSignalConnector menu_signal_connector,
    MultiWinGeometryFunc geometry_func, MultiWinSizeFunc size_func,
    MultiWinDefaultSizeFunc default_size_func,
    MultiTabToNewWindowHandler tab_to_new_window_handler,
    MultiWinZoomHandler zoom_handler,
    MultiWinGetDisableMenuShortcuts get_disable_menu_shortcuts,
    MultiWinInitialTabs initial_tabs,
    MultiWinDeleteHandler delete_handler,
    MultiTabGetShowCloseButton get_show_close_button)
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
    multi_win_initial_tabs = initial_tabs;
    multi_win_delete_handler = delete_handler;
    multi_tab_get_show_close_button = get_show_close_button;
}

MultiTab *multi_tab_new(MultiWin * parent, gpointer user_data_template)
{
    MultiTab *tab = g_new0(MultiTab, 1);

    tab->status_stock = NULL;
    tab->widget = multi_tab_filler(parent, tab, user_data_template,
        &tab->user_data, &tab->active_widget, &tab->adjustment);
    g_object_set_data(G_OBJECT(tab->widget), "roxterm_tab", tab);

    multi_win_add_tab(parent, tab, -1, FALSE);
    multi_win_select_tab(parent, tab);

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

const char *multi_tab_get_display_name(MultiTab *tab)
{
    return tab->parent ? tab->parent->display_name : NULL;
}

void multi_win_set_geometry_hints(MultiWin *win, GtkWidget *child,
    GdkGeometry *geometry, GdkWindowHints geom_mask)
{
    gtk_window_set_geometry_hints(GTK_WINDOW(win->gtkwin), child,
        geometry, geom_mask);
}

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

static char *make_title(const char *template, const char *title)
{
    char *title0 = NULL;
    
    if (template)
    {
        if (strstr(template, "%s"))
        {
            title0 = g_strdup_printf(template, title ? title: "");
        }
        else
        {
            title0 = g_strdup(template);
        }
    }
    return title0;
}

void multi_tab_set_icon_title(MultiTab * tab, const char *title)
{
    MultiWin *win = tab->parent;

    g_free(tab->icon_title);
    tab->icon_title = title ? g_strdup(title) : NULL;
    if (win->current_tab == tab)
    {
        multi_win_set_icon_title(win, tab->icon_title);
    }
}

static gboolean check_title_template(const char *tt)
{
    const char *c;
    gboolean lwpc = FALSE;
    gboolean got_pcs = FALSE;
    
    c = tt;
    do
    {
        if (*c == '%')
        {
            /* Check for illegal trailing % */
            if (!lwpc && *(c + 1) == 0)
                return FALSE;
            /* lwpc is FALSE after even # of %s, TRUE after odd # */
            lwpc = !lwpc;
        }
        else
        {
            if (lwpc)
            {
                if (*c == 's')
                {
                    if (got_pcs)
                        return FALSE;
                    got_pcs = TRUE;
                }
                else
                {
                    return FALSE;
                }
            }
            lwpc = FALSE;
        }
    }
    while (*(c++));
    return TRUE;
}

static gboolean validate_title_template(GtkWindow *parent, const char *tt)
{
    if (tt && !check_title_template(tt))
    {
        static char *bad_template = NULL;
    
        if (!bad_template || strcmp(bad_template, tt))
        {
            dlg_warning(parent,
              _("'%s' contains invalid %% sequences for a title template"),
              tt);
            g_free(bad_template);
            bad_template = g_strdup(tt);
        }
        return FALSE;
    }
    return TRUE;
}

static void multi_tab_set_full_window_title(MultiTab * tab,
        const char *template, const char *title)
{
    MultiWin *win = tab->parent;
    char *actual_title = g_strdup_printf(template ? template : "%s",
            title ? title : "");

    if (tab->label)
    {
        char *menu_text = g_strdup_printf("%d. %s",
                gtk_notebook_page_num(GTK_NOTEBOOK(win->notebook),
                        tab->widget) + 1,
                actual_title);
        
        gtk_label_set_text(GTK_LABEL(tab->label), actual_title);
        gtk_label_set_text(GTK_LABEL(tab->menu_label), menu_text);
        g_free(menu_text);
    }
    if (win)
    {
        win->ignore_toggles = TRUE;
        if (win->current_tab == tab)
            multi_win_set_title(win, actual_title);
        if (tab->popup_menu_item)
        {
            tab->popup_menu_item = menutree_change_tab_title
                (win->popup_menu, tab->popup_menu_item, actual_title);
            g_signal_connect(tab->popup_menu_item, "toggled",
                G_CALLBACK(multi_win_select_tab_action), tab);
        }
        if (tab->menu_bar_item)
        {
            tab->menu_bar_item = menutree_change_tab_title
                (win->menu_bar, tab->menu_bar_item, actual_title);
            g_signal_connect(tab->menu_bar_item, "toggled",
                G_CALLBACK(multi_win_select_tab_action), tab);
        }
        win->ignore_toggles = FALSE;
    }
    g_free(actual_title);
}

void multi_tab_set_window_title(MultiTab * tab, const char *title)
{
    g_free(tab->window_title);
    tab->window_title = title ? g_strdup(title) : NULL;
    multi_tab_set_full_window_title(tab, tab->window_title_template, title);
}

void multi_tab_set_window_title_template(MultiTab * tab, const char *template)
{
    GtkWidget *gwin = tab->parent ? tab->parent->gtkwin : NULL;
    
    if (!validate_title_template(gwin ? GTK_WINDOW(gwin) : NULL,
            template))
    {
        return;
    }
    if (tab->title_template_locked)
        return;
    g_free(tab->window_title_template);
    tab->window_title_template = template ? g_strdup(template) : NULL;
    multi_tab_set_full_window_title(tab, template, tab->window_title);
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
    return make_title(tab->window_title_template, tab->window_title);
}

const char *multi_tab_get_icon_title(MultiTab * tab)
{
    return tab->icon_title;
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

MultiTab *multi_tab_get_tab_under_pointer(int x, int y)
{
    MultiWin *win = multi_win_get_win_under_pointer();
    GList *tab_node;
    GtkPositionType tab_pos;
    gboolean vert_tabs;

    if (!win)
        return NULL;
    if (win->ntabs == 1)
        return win->tabs->data;
    tab_pos = win->tab_pos;
    vert_tabs = (tab_pos == GTK_POS_LEFT || tab_pos == GTK_POS_RIGHT);
    for (tab_node = win->tabs; tab_node; tab_node = g_list_next(tab_node))
    {
        int x0, y0, x1, y1;
        MultiTab *tab = tab_node->data;
        GtkWidget *label = gtk_notebook_get_tab_label(
                GTK_NOTEBOOK(win->notebook), tab->widget);

        if (!GTK_WIDGET_MAPPED(label))
            continue;
        gdk_window_get_origin(label->window, &x0, &y0);
        x0 += label->allocation.x;
        y0 += label->allocation.y;
        x1 = x0 + label->allocation.width;
        y1 = y0 + label->allocation.height;
        if (vert_tabs && y >= y0 && y < y1)
            return tab;
        else if (!vert_tabs && x >= x0 && x < x1)
            return tab;
    }
    return NULL;
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

inline static void multi_win_shade_menus_for_tabs(MultiWin * win)
{
    multi_win_shade_for_next_and_previous_tab(win);
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
    multi_win_shade_menus_for_tabs(win);
    multi_tab_remove_menutree_items(win, tab);
    multi_tab_add_menutree_items(win, tab, position);
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
        gtk_notebook_remove_page(GTK_NOTEBOOK(win->notebook),
                multi_tab_get_page_num(tab));
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
    if (multi_win_is_fullscreen(old_win))
        multi_win_set_fullscreen(win, TRUE);
    else if (multi_win_is_maximised(old_win))
        gtk_window_maximize(GTK_WINDOW(win->gtkwin));
    if (!win->tabs)
    {
        win->default_title_template = old_win->default_title_template ?
                g_strdup(old_win->default_title_template) : NULL;
        win->title_template = old_win->default_title_template ?
                g_strdup(old_win->default_title_template) : NULL;
    }
    old_win_destroyed = old_win && old_win->ntabs <= 1;
    multi_tab_remove_from_parent(tab, FALSE);
    multi_win_add_tab(win, tab, position, FALSE);
    multi_win_set_geometry_hints_for_tab(win, tab);
    if (multi_tab_to_new_window_handler)
        multi_tab_to_new_window_handler(win, tab, old_win_destroyed);
    /* Remove extra references added by multi_tab_remove_from_parent */
    g_object_unref(tab->widget);
}

static gboolean multi_tab_toggle_attention(gpointer data)
{
    MultiTab *tab = data;
    
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(tab->label_bg),
            tab->attention = !tab->attention);
    return TRUE;
}

void multi_tab_draw_attention(MultiTab *tab)
{
    multi_tab_cancel_attention(tab);
    multi_tab_toggle_attention(tab);
    tab->attention_tag = g_timeout_add(500, multi_tab_toggle_attention, tab);
}

void multi_tab_cancel_attention(MultiTab *tab)
{
    if (tab->attention_tag)
    {
        g_source_remove(tab->attention_tag);
        tab->attention_tag = 0;
    }
    else
    {
        return;
    }
    if (tab->attention)
    {
        multi_tab_toggle_attention(tab);
        tab->attention = FALSE;
    }
}

#if 0
/* Force tab width to something "nice". Unfortunately we can't
 * easily find the window's size when we're adding the first tab. */
static void multi_tab_set_single_size(MultiTab *tab)
{
    g_object_set(G_OBJECT(tab->parent->notebook), "homogeneous", FALSE, NULL);
    gtk_container_child_set(GTK_CONTAINER(tab->parent->notebook), tab->widget,
            "tab-expand", FALSE, "tab-fill", FALSE, NULL);
    gtk_widget_set_size_request(tab->label, 240, -1);
}
#endif

static void multi_win_set_full_title(MultiWin *win,
        const char *template, const char *title)
{
    if (win->gtkwin)
    {
        char *title0 = make_title(template, title);
        
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
    multi_win_set_full_title(win, win->title_template, title);
}

static void multi_win_set_icon_title(MultiWin *win, const char *title)
{
    if (win->gtkwin->window)
        gdk_window_set_icon_name(win->gtkwin->window, title);
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
        
        multi_tab_cancel_attention(tab);
        multi_tab_set_status_stock(tab, NULL);
        win->user_data_template = tab->user_data;
        gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook),
            multi_tab_get_page_num(tab));
        gtk_widget_grab_focus(tab->active_widget);
        multi_win_set_title(win, title);
        g_free(title);
        multi_win_set_icon_title(win, tab->icon_title);
        menutree_select_tab(win->popup_menu, tab->popup_menu_item);
        menutree_select_tab(win->menu_bar, tab->menu_bar_item);
        if (GTK_WIDGET_REALIZED(tab->active_widget))
        {
            if (win->tab_selection_handler)
                win->tab_selection_handler(tab->user_data, tab);
            multi_win_set_geometry_hints_for_tab(win, tab);
        }
    }
    multi_win_shade_for_next_and_previous_tab(win);
    /* FIXME: Ideally we should shade scroll up/down menu items if tab doesn't
     * have an adjustment, but in this implementation they always do
     */
    win->ignore_tab_selections = FALSE;
    win->ignore_toggles = FALSE;
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
    win->menu_bar_set = show;
    if (show)
        add_menu_bar(win);
    else
        remove_menu_bar(win);
    menutree_set_show_menu_bar_active(win->menu_bar, show);
    menutree_set_show_menu_bar_active(win->popup_menu, show);
    menutree_set_show_menu_bar_active(win->short_popup, show);
    multi_win_restore_size(win);
}

gboolean multi_win_get_show_menu_bar(MultiWin * win)
{
    return win->show_menu_bar;
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

static gboolean multi_win_state_event_handler(GtkWidget *widget,
        GdkEventWindowState *event, MultiWin *win)
{
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
    GdkWindow *w = win->window;

    gdk_window_set_group(w, w);
    (void) data;
}

static void multi_win_destroy_handler(GtkObject * obj, MultiWin * win)
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
multi_win_page_switched(GtkNotebook * notebook, GtkNotebookPage * page,
    guint page_num, MultiWin * win)
{
    MultiTab *tab = multi_tab_get_from_widget(
            gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), page_num));

    if (win->current_tab == tab)
        return;
    multi_win_select_tab(win, tab);
}

static void tab_set_name_from_clipboard_callback(GtkClipboard *clp,
        const gchar *text, gpointer data)
{
    MultiTab *tab = (MultiTab *) data;
    
    if (text == NULL || tab == NULL)
        return;
    multi_tab_set_window_title_template(tab, text);
}

static gboolean multi_win_focus_in(GtkWidget *widget, GdkEventFocus *event,
        MultiWin *win)
{
    multi_win_all = g_list_remove(multi_win_all, win);
    multi_win_all = g_list_prepend(multi_win_all, win);
    gtk_window_set_urgency_hint(GTK_WINDOW(win->gtkwin), FALSE);
    return FALSE;
}

static gboolean tab_clicked_handler(GtkWidget *widget,
        GdkEventButton *event, MultiTab *tab)
{
    if (event->type != GDK_BUTTON_PRESS)
        return FALSE;
    
    switch (event->button)
    {
        case 2:
            gtk_clipboard_request_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY),
                                       tab_set_name_from_clipboard_callback,
                                       tab);
            break;
        default:
            break;
    }
    return FALSE;
}

static void page_reordered_callback(GtkNotebook *notebook, GtkWidget *child,
        guint page_num, MultiWin *win)
{
    MultiTab *tab = multi_tab_get_from_widget(child);

    g_return_if_fail(tab);
    multi_tab_move_to_position(tab, page_num, FALSE);
}

static void page_added_callback(GtkNotebook *notebook, GtkWidget *child,
        guint page_num, MultiWin *win)
{
    MultiTab *tab = multi_tab_get_from_widget(child);
    gboolean old_win_destroyed;

    g_return_if_fail(tab);
    if (!win->ignore_tabs_moving)
    {
        if (tab->parent == win)
        {
            /* Dragged in from same window so do nothing */
            return;
        }
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
        multi_tab_to_new_window_handler(win, tab, old_win_destroyed);
        if (win->ntabs == 1)
        {
            /* multi_tab_set_single_size(tab); */
            multi_win_show(win);
        }
    }
}

static GtkNotebook *multi_win_notebook_creation_hook(GtkNotebook *source,
        GtkWidget *page, gint x, gint y, gpointer data)
{
    MultiTab *tab = multi_tab_get_from_widget(page);
    MultiWin *win;

    g_return_val_if_fail(tab, NULL);
    win = multi_win_new_for_tab(tab->parent->display_name, x, y, tab);
    /* Defer showing till page_added_callback because we can't set size
     * correctly until the tab has been added to the notebook */
    return GTK_NOTEBOOK(win->notebook);
}

MultiWin *multi_win_new_for_tab(const char *display_name, int x, int y,
        MultiTab *tab)
{
    gboolean disable_menu_shortcuts, disable_tab_shortcuts;
    MultiWin *win = tab->parent;
    int w, h;
    GtkWindow *gwin = GTK_WINDOW(win->gtkwin);

    gtk_window_get_size(gwin, &w, &h);
    multi_win_get_disable_menu_shortcuts(tab->user_data,
            &disable_menu_shortcuts, &disable_tab_shortcuts);
    win = multi_win_new_blank(display_name,
                multi_win_get_shortcut_scheme(win), win->zoom_index,
                disable_menu_shortcuts, disable_tab_shortcuts,
                win->tab_pos, win->always_show_tabs);
    multi_win_set_geometry_hints_for_tab(win, tab);
    gwin = GTK_WINDOW(win->gtkwin);
    gtk_window_set_default_size(gwin, w, h);
    gtk_window_move(gwin, MAX(x - 20, 0), MAX(y - 8, 0));
    gtk_widget_show_all(win->notebook);
    return win;
}

static void multi_win_close_tab_clicked(GtkWidget *widget, MultiTab *tab)
{
    multi_tab_delete(tab);
}

MultiWin *multi_win_clone(MultiWin *old,
        gpointer user_data_template, gboolean always_show_tabs)
{
    GtkPositionType tab_pos = GTK_POS_TOP;
    int num_tabs = 1;
    char geom[16];
    int width, height;

    multi_win_initial_tabs(user_data_template, &tab_pos, &num_tabs);
    multi_win_default_size_func(multi_tab_get_user_data(old->current_tab),
            &width, &height);
    sprintf(geom, "%dx%d", width, height);
    return multi_win_new_with_geom(old->display_name, old->shortcuts,
            old->zoom_index, user_data_template, geom,
            num_tabs, tab_pos, always_show_tabs);
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
    multi_tab_new(win, win->current_tab ? win->current_tab->user_data :
        win->user_data_template);
}

static void multi_win_close_tab_action(MultiWin * win)
{
    multi_tab_delete(win->current_tab);
}

static void multi_win_name_tab_action(MultiWin * win)
{
    MultiTab *tab = win->current_tab;
    g_return_if_fail(tab);
    GtkWidget *dialog_w = gtk_dialog_new_with_buttons(_("Name Tab"),
            GTK_WINDOW(win->gtkwin),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
            NULL);
    GtkDialog *dialog = GTK_DIALOG(dialog_w);
    GtkWidget *name_w = gtk_entry_new();
    GtkEntry *name_e = GTK_ENTRY(name_w);
    const char *name = tab->window_title_template;
    int response;

    tab->rename_dialog = dialog_w;
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), name_w, TRUE, TRUE, 8);
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

static void multi_win_set_window_title_action(MultiWin * win)
{
    MultiTab *tab = win->current_tab;
    g_return_if_fail(tab);
    GtkWidget *dialog_w = gtk_dialog_new_with_buttons(_("Set Window Title"),
            GTK_WINDOW(win->gtkwin),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
            NULL);
    GtkDialog *dialog = GTK_DIALOG(dialog_w);
    GtkWidget *title_w = gtk_entry_new();
    GtkEntry *title_e = GTK_ENTRY(title_w);
    GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
    GtkWidget *img = gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO,
            GTK_ICON_SIZE_DIALOG);
    GtkWidget *tip_label = gtk_label_new(_("The title string may include '%s' "
            "which is substituted with the title set by the child command "
            "(usually the current directory for shells). No other % "
            "characters or sequences are permitted except '%%' which is "
            "displayed as a single %. Apply an empty string here to use the "
            "profile's title string."));
    int response;
    const char *title;

    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_APPLY);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), title_w, FALSE, TRUE, 8);
    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, TRUE, 0);
    gtk_label_set_line_wrap(GTK_LABEL(tip_label), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), tip_label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), hbox, TRUE, TRUE, 8);
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
            win->title_template_locked = (title == NULL);
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

static void multi_win_vscroll_by(MultiWin *win, double multiplier)
{
    GtkAdjustment *adj;
    double newval;
    
    adj = win->current_tab->adjustment;
    if (!adj)
        return;
    newval = adj->value + multiplier * adj->step_increment;
    newval = CLAMP(newval, 0, adj->upper - adj->page_size);
    gtk_adjustment_set_value(adj, newval);
}

static void multi_win_scroll_up_action(MultiWin *win)
{
    g_return_if_fail(win->current_tab != NULL);
    multi_win_vscroll_by(win, -1);
}

static void multi_win_scroll_down_action(MultiWin *win)
{
    g_return_if_fail(win->current_tab != NULL);
    multi_win_vscroll_by(win, 1);
}

static void multi_win_move_tab_by_one(MultiWin *win, int dir)
{
    MultiTab *tab = win->current_tab;
    int pos;
    
    g_return_if_fail(win->current_tab);
    pos = multi_tab_get_page_num(tab) + dir;
    if (win->wrap_switch_tab)
    {
        if (pos >= win->ntabs)
            pos = 0;
        else if (pos < 0)
            pos = win->ntabs - 1;
    }
    g_return_if_fail(pos >= 0 && pos < win->ntabs);
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

static void multi_win_connect_actions(MultiWin * win)
{
    multi_win_menu_connect_swapped(win, MENUTREE_FILE_NEW_WINDOW, G_CALLBACK
        (multi_win_new_window_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_FILE_CLOSE_WINDOW, G_CALLBACK
        (multi_win_delete), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_FILE_NEW_TAB, G_CALLBACK
        (multi_win_new_tab_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_FILE_CLOSE_TAB, G_CALLBACK
        (multi_win_close_tab_action), win, NULL, NULL, NULL);
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
    multi_win_menu_connect_swapped(win, MENUTREE_EDIT_SET_WINDOW_TITLE,
        G_CALLBACK(multi_win_set_window_title_action), win, NULL, NULL, NULL);
    multi_win_menu_connect(win, MENUTREE_VIEW_SHOW_TAB_BAR, G_CALLBACK
        (multi_win_toggle_show_tab_bar_action), win, NULL, NULL, NULL);
    multi_win_menu_connect(win, MENUTREE_VIEW_SHOW_MENUBAR, G_CALLBACK
        (multi_win_toggle_show_menubar_action), win, NULL, NULL, NULL);
    multi_win_menu_connect(win, MENUTREE_VIEW_FULLSCREEN, G_CALLBACK
        (multi_win_toggle_fullscreen_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_ZOOM_IN, G_CALLBACK
        (multi_win_zoom_in_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_ZOOM_OUT, G_CALLBACK
        (multi_win_zoom_out_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_ZOOM_NORM, G_CALLBACK
        (multi_win_zoom_norm_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_SCROLL_UP, G_CALLBACK
        (multi_win_scroll_up_action), win, NULL, NULL, NULL);
    multi_win_menu_connect_swapped(win, MENUTREE_VIEW_SCROLL_DOWN, G_CALLBACK
        (multi_win_scroll_down_action), win, NULL, NULL, NULL);
}

static void multi_win_set_show_tabs_menu_items(MultiWin *win, gboolean active)
{
    menutree_set_show_tab_bar_active(win->menu_bar, active);
    menutree_set_show_tab_bar_active(win->popup_menu, active);
}

static void multi_win_show_tabs(MultiWin * win)
{
    if (win->tab_pos != -1)
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
    g_object_set_data(G_OBJECT(win->gtkwin->window), "ROXTermWin", win);
}

MultiWin *multi_win_new_blank(const char *display_name, Options *shortcuts,
        int zoom_index,
        gboolean disable_menu_shortcuts, gboolean disable_tab_shortcuts,
        GtkPositionType tab_pos, gboolean always_show_tabs)
{
    MultiWin *win = g_new0(MultiWin, 1);
    static gboolean set_nwc_hook = FALSE;
    GtkNotebook *notebook;
    const char *xclass, *xname;

    if (!set_nwc_hook)
    {
        gtk_notebook_set_window_creation_hook(multi_win_notebook_creation_hook,
                win, NULL);
        set_nwc_hook = TRUE;
    }

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
    
    xclass = global_options_lookup_string("xclass");
    xname = global_options_lookup_string("xname");
    if (xclass && !xname)
        xname = "Roxterm";
    if (xname && !xclass)
        xclass = "roxterm";
    if (xclass)
    {
        win->xclass = g_strdup(xclass);
        win->xname = g_strdup(xname);
        gtk_window_set_wmclass(GTK_WINDOW(win->gtkwin), xname, xclass);
        global_options_reset_string("xclass");
        global_options_reset_string("xname");
    }
    
    if (display_name)
    {
        GdkScreen *scr;
        
        win->display_name = g_strdup(display_name);
        scr = display_get_screen_for_name(display_name);
        if (scr)
            gtk_window_set_screen(GTK_WINDOW(win->gtkwin), scr);
    }
    if (multi_win_role_prefix)
    {
        char *role = g_strdup_printf("%s-%d-%x-%d", multi_win_role_prefix,
                getpid(), g_random_int(), ++multi_win_role_index);
        
        gtk_window_set_role(GTK_WINDOW(win->gtkwin), role);
        g_free(role);
    }
    g_signal_connect(win->gtkwin, "realize",
            G_CALLBACK(multi_win_realize_handler), win);
    win->destroy_handler = g_signal_connect(win->gtkwin, "destroy",
            G_CALLBACK(multi_win_destroy_handler), win);
    if (multi_win_delete_handler)
    {
        g_signal_connect(GTK_OBJECT(win->gtkwin), "delete-event",
                G_CALLBACK(multi_win_delete_handler), win);
    }
    
#if HAVE_COMPOSITE
    {
        GdkScreen *screen = gtk_widget_get_screen(win->gtkwin);
        GdkColormap *colormap = gdk_screen_get_rgba_colormap(screen);
        if (colormap && gdk_screen_is_composited(screen))
        {
            gtk_widget_set_colormap(win->gtkwin, colormap);
            win->composite = TRUE;
        }
        else
        {
            win->composite = FALSE;
        }
    }
#endif

    win->vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(win->gtkwin), win->vbox);

    options_ref(shortcuts);
    win->shortcuts = shortcuts;
    win->accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(win->gtkwin), win->accel_group);

    win->popup_menu = menutree_new(shortcuts, win->accel_group,
        GTK_TYPE_MENU, disable_menu_shortcuts, disable_tab_shortcuts, win);
    menutree_connect_destroyed(win->popup_menu,
        G_CALLBACK(multi_win_menutree_deleted_handler), win);

    win->short_popup = menutree_new_short_popup(shortcuts, win->accel_group,
        disable_menu_shortcuts, win);
    menutree_connect_destroyed(win->short_popup,
        G_CALLBACK(multi_win_menutree_deleted_handler), win);

    win->menu_bar = menutree_new(shortcuts, win->accel_group,
        GTK_TYPE_MENU_BAR, disable_menu_shortcuts, disable_tab_shortcuts,
        win);
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
    gtk_notebook_set_scrollable(notebook, TRUE);
    if (win->tab_pos == GTK_POS_LEFT || win->tab_pos == GTK_POS_RIGHT)
    {
        g_object_set(notebook, "tab-hborder", 0, NULL);
    }
    else
    {
        g_object_set(notebook, "tab-vborder", 0, NULL);
    }
    gtk_notebook_popup_enable(notebook);
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
    gtk_box_pack_start_defaults(GTK_BOX(win->vbox), win->notebook);
    g_signal_connect(win->notebook, "switch-page",
        G_CALLBACK(multi_win_page_switched), win);
    g_signal_connect(win->gtkwin, "focus-in-event",
        G_CALLBACK(multi_win_focus_in), win);
    gtk_notebook_set_group(notebook, &multi_win_all);
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

MultiWin *multi_win_new_full(const char *display_name, Options *shortcuts,
        int zoom_index, gpointer user_data_template, const char *geom,
        MultiWinSizing sizing, int numtabs, GtkPositionType tab_pos,
        gboolean always_show_tabs)
{
    gboolean disable_menu_shortcuts, disable_tab_shortcuts; 
    MultiWin *win;
    MultiTab *tab;
    int n;
    
    multi_win_get_disable_menu_shortcuts(user_data_template,
            &disable_menu_shortcuts, &disable_tab_shortcuts);
    win = multi_win_new_blank(display_name, shortcuts, zoom_index,
            disable_menu_shortcuts, disable_tab_shortcuts,
            tab_pos, always_show_tabs);
    win->user_data_template = user_data_template;
    win->tab_pos = tab_pos;
    for (n = 0; n < numtabs; ++n)
        multi_tab_new(win, user_data_template);

    if (tab_pos == -1)
    {
        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(win->notebook),
                always_show_tabs);
        gtk_notebook_set_show_border(GTK_NOTEBOOK(win->notebook),
                always_show_tabs);
    }
    else
    {
        gtk_notebook_set_tab_pos(GTK_NOTEBOOK(win->notebook), tab_pos);
    }
    multi_win_shade_menus_for_tabs(win);

    if (geom)
    {
        gtk_window_parse_geometry(GTK_WINDOW(win->gtkwin), geom);
    }
    else if (sizing == MULTI_WIN_FULL_SCREEN)
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
    for (n = numtabs - 1; n >= 0; --n)
        gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), n);
    tab = win->tabs->data;
    win->tab_selection_handler(tab->user_data, tab);

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
        /* Remember we added an extra reference to this when we created window
         */
        UNREF_LOG(g_object_ref(menutree_get_top_level_widget(win->menu_bar)));
    }
    if (destroy_widgets && win->gtkwin)
    {
        gtk_widget_destroy(win->gtkwin);
        win->gtkwin = NULL;
    }
    UNREF_LOG(options_unref(win->shortcuts));
    if (win->display_name)
    {
        g_free(win->display_name);
    }
    g_free(win->xclass);
    g_free(win->xname);
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
        if (win->ntabs == 1)
        {
            tab = win->tabs->data;
            /*multi_tab_set_single_size(tab);*/
            if (!win->always_show_tabs)
            {
                multi_win_hide_tabs(win);
                multi_win_restore_size(win);
            }
        }
    }
    multi_win_shade_menus_for_tabs(win);
    return FALSE;
}

void multi_tab_add_close_button(MultiTab *tab)
{
    GtkRcStyle *rcstyle;
    GtkSettings *settings;
    int w, h;
    MultiWin *win;

    if (tab->close_button)
        return;
    
    win = tab->parent;
    tab->close_button = gtk_button_new();
    tab->image = gtk_image_new_from_stock(
            tab->status_stock ? tab->status_stock : GTK_STOCK_CLOSE,
            GTK_ICON_SIZE_MENU);
    rcstyle = gtk_rc_style_new();
    gtk_container_add(GTK_CONTAINER(tab->close_button), tab->image);
    gtk_button_set_relief(GTK_BUTTON(tab->close_button), GTK_RELIEF_NONE);
    gtk_button_set_focus_on_click(GTK_BUTTON(tab->close_button), FALSE);
    settings = gtk_widget_get_settings(tab->label);
    gtk_icon_size_lookup_for_settings(settings, GTK_ICON_SIZE_MENU, &w, &h);
    /* gtk_widget_set_size_request(tab->close_button, w + 2, h + 2); */
    rcstyle->xthickness = rcstyle->ythickness = 0;
    gtk_widget_modify_style(tab->close_button, rcstyle);
    gtk_rc_style_unref(rcstyle);
    tab->label_packer(GTK_BOX(tab->label_box), tab->close_button,
            FALSE, FALSE, 0);
    g_signal_connect(tab->close_button, "clicked",
            G_CALLBACK(multi_win_close_tab_clicked), tab);
    gtk_widget_show_all(tab->close_button);
    if (win)
    {
        multi_win_restore_size(win);
    }
}

void multi_tab_remove_close_button(MultiTab *tab)
{
    if (tab->close_button)
    {
        MultiWin *win;
    
        win = tab->parent;
        gtk_widget_destroy(tab->close_button);
        tab->close_button = NULL;
        if (win)
        {
            multi_win_restore_size(win);
        }
    }
    tab->image = NULL;
}

void multi_tab_set_status_stock(MultiTab *tab, const char *stock)
{
    if (!strcmp(tab->status_stock ? tab->status_stock : GTK_STOCK_CLOSE,
            stock ? stock : GTK_STOCK_CLOSE))
    {
        return;
    }
    tab->status_stock = stock;
    if (tab->image)
    {
        gtk_image_set_from_stock(GTK_IMAGE(tab->image),
                stock ? stock : GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
    }
}

/* Creates the label widget for a tab. tab->label is the GtkLabel containing
 * the text; the return value is the top-level container. */
static GtkWidget *make_tab_label(MultiTab *tab, GtkPositionType tab_pos)
{
    static GdkColor amber;
    static gboolean parsed_amber = FALSE;
    GtkLabel *label;

    tab->label = gtk_label_new(NULL);
    tab->menu_label = gtk_label_new(NULL);
    label = GTK_LABEL(tab->label);
    gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_MIDDLE);
    multi_tab_set_full_window_title(tab, tab->window_title_template,
            tab->window_title);
    switch (tab_pos)
    {
        case GTK_POS_LEFT:
        case GTK_POS_RIGHT:
            tab->label_box = gtk_vbox_new(FALSE, 4);
            tab->label_packer = gtk_box_pack_end;
            gtk_widget_set_size_request(tab->label, HORIZ_TAB_WIDTH, -1);
            break;
        default:
            tab->label_box = gtk_hbox_new(FALSE, 4);
            tab->label_packer = gtk_box_pack_start;
            break;
    }
    
    tab->label_bg = gtk_event_box_new();
    if (!parsed_amber)
    {
        gdk_color_parse("#ffc450", &amber);
        parsed_amber = TRUE;
    }
    gtk_widget_modify_bg(tab->label_bg, GTK_STATE_NORMAL, &amber);
    gtk_widget_modify_bg(tab->label_bg, GTK_STATE_ACTIVE, &amber);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(tab->label_bg), FALSE);
    tab->label_packer(GTK_BOX(tab->label_box), tab->label_bg, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(tab->label_bg), tab->label);
    g_signal_connect(tab->label_bg, "button-press-event",
            G_CALLBACK(tab_clicked_handler), tab);
    if (multi_tab_get_show_close_button(tab->user_data))
    {
        multi_tab_add_close_button(tab);
    }
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(tab->parent->notebook),
        tab->widget, tab->label_box);
    gtk_widget_show_all(tab->label_box);

    return tab->label_box;
}

static void multi_tab_add_menutree_items(MultiWin * win, MultiTab * tab,
        int position)
{
    char *title = multi_tab_get_full_window_title(tab);
    
    tab->popup_menu_item = menutree_add_tab_at_position(win->popup_menu,
            title, position);
    tab->menu_bar_item = menutree_add_tab_at_position(win->menu_bar,
            title, position);
    g_free(title);
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
        gtk_notebook_append_page_menu(notebook, tab->widget, label,
                tab->menu_label);
    }
    else
    {
        gtk_notebook_insert_page_menu(notebook,
                tab->widget, label, tab->menu_label, position);
    }
    gtk_notebook_set_tab_reorderable(notebook, tab->widget, TRUE);
    gtk_notebook_set_tab_detachable(notebook, tab->widget, TRUE);
    g_object_set(gtk_widget_get_parent(label), "can-focus", FALSE, NULL);
    /* Note at this point ntabs is how many tabs there are about to be,
     * not how many there were before adding.
     */
    /*
    if (win->ntabs == 1)
    {
        multi_tab_set_single_size(tab);
    }
    else
    {
        GList *link;
        
        g_object_set(G_OBJECT(win->notebook), "homogeneous", TRUE, NULL);
        for (link = win->tabs; link; link = g_list_next(link))
        {
            MultiTab *tab = link->data;
            
            gtk_container_child_set(GTK_CONTAINER(notebook), tab->widget,
                    "tab-expand", TRUE, "tab-fill", TRUE, NULL);
        }
    }
    */
    gtk_container_child_set(GTK_CONTAINER(notebook), tab->widget,
            "tab-expand", TRUE, "tab-fill", TRUE, NULL);
}

/* Keep track of a tab after it's been created; if notify_only is set, it's
 * assumed the tab has already been added by GTK and this is being called 
 * just for notification */
static void multi_win_add_tab(MultiWin * win, MultiTab * tab, int position,
        gboolean notify_only)
{
    tab->parent = win;
    win->ignore_tabs_moving = TRUE;
    if (position == -1)
        win->tabs = g_list_append(win->tabs, tab);
    else
        win->tabs = g_list_insert(win->tabs, tab, position);
    ++win->ntabs;
    if (win->ntabs == 2 && !win->always_show_tabs)
    {
        multi_win_show_tabs(win);
        multi_win_restore_size(win);
    }
    multi_tab_add_menutree_items(win, tab, position);
    if (!notify_only)
    {
        multi_win_add_tab_to_notebook(win, tab, position);
        gtk_widget_show(tab->widget);
    }
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
    return (win->gtkwin->window &&
            (gdk_window_get_state(win->gtkwin->window) &
                GDK_WINDOW_STATE_MAXIMIZED) != 0);
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
    if (win->shortcuts != shortcuts)
    {
        if (win->shortcuts)
        {
            UNREF_LOG(shortcuts_unref(win->shortcuts));
        }
        options_ref(shortcuts);
        win->shortcuts = shortcuts;
        if (win->menu_bar)
            menutree_apply_shortcuts(win->menu_bar, shortcuts);
        if (win->popup_menu)
            menutree_apply_shortcuts(win->popup_menu, shortcuts);
        if (win->short_popup)
            menutree_apply_shortcuts(win->short_popup, shortcuts);
    }
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

MultiWin *multi_win_get_win_under_pointer(void)
{
    int win_x, win_y;
    GdkWindow *gdkwin = gdk_window_at_pointer(&win_x, &win_y);

    if (!gdkwin)
        return NULL;
    return g_object_get_data(G_OBJECT(gdk_window_get_toplevel(gdkwin)),
            "ROXTermWin");
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
        if (show)
            multi_win_show_tabs(win);
        else
            multi_win_hide_tabs(win);
        multi_win_restore_size(win);
    }
}

void multi_win_set_title_template(MultiWin *win, const char *tt)
{
    if (!validate_title_template(GTK_WINDOW(win->gtkwin), tt))
        return;
    if (win->title_template_locked)
        return;
    g_free(win->title_template);
    win->title_template = tt ? g_strdup(tt) : NULL;
    multi_win_set_full_title(win, tt, win->child_title);
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
#if HAVE_COMPOSITE
    return win->composite;
#else
    return FALSE;
#endif
}

const char *multi_win_get_display_name(MultiWin *win)
{
    return win->display_name;
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

void
multi_win_get_class_name(MultiWin *win, char const **xclass, char const **xname)
{
    *xclass = win->xclass;
    *xname = win->xname;
}

/* This is maintained in order of most recently focused */
GList *multi_win_all = NULL;

/* vi:set sw=4 ts=4 et cindent cino= */
