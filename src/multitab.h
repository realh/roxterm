#ifndef MULTITAB_H
#define MULTITAB_H
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

/* Hierarchy of multiple windows, each containing multiple tabs */

#include "menutree.h"
#include "options.h"

#define HAVE_COMPOSITE GTK_CHECK_VERSION(2, 10, 0)

typedef struct MultiTab MultiTab;
typedef struct MultiWin MultiWin;

/* This type of function is called whenever a new tab/window is created; it
 * should return a widget to be the main content of the tab/window.
 * user_data_template is either the data belonging to the tab active when the
 * user created a new tab, or in the case of the window created by
 * multi_tab_init, the data passed to multi_tab_init; it's const because it's
 * intended to be a template for creating new data, which is passed back via
 * new_user_data; title for tab should also be passed back.
 * Returned widget is the top-most widget to add to the GtkNotebook, while
 * active_widget is used to return a widget to attach signal/event handlers to.
 * If the function exits with adjustment non-NULL it is used for handling
 * scroll menu actions.
 */
typedef GtkWidget *(*MultiTabFiller) (MultiWin * win, MultiTab * tab,
    gpointer user_data_template, gpointer * new_user_data,
    GtkWidget ** active_widget, GtkAdjustment **adjustment);

/* Called when a tab is destroyed, with the data passed as new_user_data above
 */
typedef void (*MultiTabDestructor) (gpointer user_data);

/* Called to connect menu action signal handlers to a window */
typedef void (*MultiWinMenuSignalConnector) (MultiWin * win);

/* Called when a window wants to know what geometry hints to use for a child
 * widget; the user_data is the data returned by MultiTabFiller's new_user_data.
 */
typedef void (*MultiWinGeometryFunc) (gpointer user_data, GdkGeometry * geom,
    GdkWindowHints * hints);

/* If pixels is FALSE: Return grid size in *pwidth and *pheight.
 * If pixels is TRUE: Given a grid size in *pwidth and *pheight, update them to
 * the total size of the widget required for it. If a size in is -1 use the
 * widget's current grid size instead.
 */
typedef void (*MultiWinSizeFunc) (gpointer user_data, gboolean pixels,
    int *pwidth, int *pheight);

/* Return the profile's default size in char cells */
typedef void (*MultiWinDefaultSizeFunc)(gpointer user_data,
        int *width, int *height);

/* Called when a tab is selected */
typedef void (*MultiTabSelectionHandler) (gpointer user_data, MultiTab * tab);

/* Called when a tab is dragged to a new window; should be used to try to make
 * sure page is same size as other tabs in new window and to update references.
 * old_win is NULL if the tab's previous window was destroyed.
 */
typedef void (*MultiTabToNewWindowHandler)(MultiWin *win, MultiTab *tab,
        MultiWin *old_win);

/* Called when the zoom factor is changed */
typedef void (*MultiWinZoomHandler)(gpointer user_data, double zoom_factor,
        int zoom_index);

/* Called to find out whether to use menu shortcuts */
typedef void (*MultiWinGetDisableMenuShortcuts)(gpointer user_data,
    gboolean *general, gboolean *tabs);

/* As number of tabs and tab position can be set by user we need another
 * function to get these values */
typedef void (*MultiWinInitialTabs)(gpointer user_data,
        GtkPositionType *p_pos, int *p_num);

/* A handler for delete-event */
typedef void (*MultiWinDeleteHandler)(GtkWidget *, GdkEvent *, MultiWin *win);

/* Return whether to show close buttons in tabs */
typedef gboolean (*MultiTabGetShowCloseButton)(gpointer user_data);

/* Needed externally */
MultiWinGetDisableMenuShortcuts multi_win_get_disable_menu_shortcuts;

/* Whether to open a new tab adjacent to current one */
typedef gboolean (*MultiTabGetNewTabAdjacent)(gpointer user_data);

/* Call to set up function hooks. See MultiTabFiller etc above.
 * menu_signal_connector is called each time a new window is created to give
 * the client a chance to connect its signal handlers; each handler will
 * probably need to call multi_win_get_user_data_for_current_tab () */
void
multi_tab_init(MultiTabFiller filler, MultiTabDestructor destructor,
    MultiWinMenuSignalConnector menu_signal_connector,
    MultiWinGeometryFunc, MultiWinSizeFunc, MultiWinDefaultSizeFunc,
    MultiTabToNewWindowHandler,
    MultiWinZoomHandler, MultiWinGetDisableMenuShortcuts, MultiWinInitialTabs,
    MultiWinDeleteHandler, MultiTabGetShowCloseButton,
    MultiTabGetNewTabAdjacent
    );

/* Register a MultiTabSelectionHandler (see above) */
void
multi_tab_connect_tab_selection_handler(MultiWin *, MultiTabSelectionHandler);

MultiTab *multi_tab_new(MultiWin * parent, gpointer user_data_template);

/* When all tabs are destroyed the window is destroyed too */
void multi_tab_delete(MultiTab *);

GtkWidget *multi_tab_get_widget(MultiTab *);

const char *multi_tab_get_display_name(MultiTab *tab);

void multi_tab_set_icon_title(MultiTab *, const char *);

/* Sets the title which is used to build the actual title from title_template.
 */
void multi_tab_set_window_title(MultiTab *, const char *);

/* See multi_win_set-title */
void multi_tab_set_window_title_template(MultiTab *, const char *);

/* Whether to show number in tab title */
void multi_tab_set_show_number(MultiTab *tab, gboolean show);

const char *multi_tab_get_window_title_template(MultiTab *);

gboolean multi_tab_get_title_template_locked(MultiTab *);

void multi_tab_set_title_template_locked(MultiTab *, gboolean);

/* Not the full title */
const char *multi_tab_get_window_title(MultiTab *);

const char *multi_tab_get_icon_title(MultiTab *);

void multi_tab_popup_menu(MultiTab * tab, guint button, guint32 event_time);

gpointer multi_tab_get_user_data(MultiTab *);

/* Returns the notebook page number */
int multi_tab_get_page_num(MultiTab *);

/* Declared before other multi_win_* funcs because needed by inline function */
GtkNotebook *multi_win_get_notebook(MultiWin *win);

MultiWin *multi_tab_get_parent(MultiTab *tab);

inline static GtkNotebook *multi_tab_get_notebook(MultiTab *tab)
{
    return multi_win_get_notebook(multi_tab_get_parent(tab));
}

/* Adds reference to widget but label and menutree widgets are lost;
 * if notify_only this function assumes the tab has already been moved by
 * GTK and it's just being called for notification.
 * Returns TRUE if win has no tabs left and is destroyed.
 */
gboolean multi_tab_remove_from_parent(MultiTab *tab, gboolean notify_only);

/* Moves a tab to the given position in the notebook; if reorder is TRUE
 * this function does the reordering otherwise it assumes the reordering has
 * already been done and it just updates data structs */
void multi_tab_move_to_position(MultiTab *tab, int position, gboolean reorder);

/* Move tab to a different window */
void multi_tab_move_to_new_window(MultiWin *win, MultiTab *tab, int position);

/* Returns NULL if pointer not over one of our tabs. x and y are absolute
 * coordinates from mouse event. Note that when there are multiple tabs this
 * returns tab whose label spans pointer position, not necessarily the one
 * whose main page widget is under the pointer */
MultiTab *multi_tab_get_tab_under_pointer(int x, int y);

/* widget is the top-level child of the notebook, as would be passed in a drag
 * received event for a GtkNotebook dragged tab.
 */
MultiTab *multi_tab_get_from_widget(GtkWidget *widget);

/* Make the tab flash orange or something */
void multi_tab_draw_attention(MultiTab *tab);
void multi_tab_cancel_attention(MultiTab *tab);

void multi_tab_add_close_button(MultiTab *tab);
void multi_tab_remove_close_button(MultiTab *tab);
void multi_tab_set_status_stock(MultiTab *tab, const char *stock);

void multi_tab_set_middle_click_tab_action(MultiTab *tab, int action);

/* In following functions a zoom_index of -1 means default */

typedef enum {
    MULTI_WIN_DEFAULT_SIZE,
    MULTI_WIN_FULL_SCREEN,
    MULTI_WIN_MAXIMISED
} MultiWinSizing;

/* If this is set all windows will be given a unique role based on it */
void multi_win_set_role_prefix(const char *role_prefix);

/* Create a new window with default settings, adding it to main list */
MultiWin *multi_win_new_full(const char *display_name,
        Options *shortcuts, int zoom_index,
        gpointer user_data_template, const char *geom,
        MultiWinSizing sizing, int numtabs, GtkPositionType tab_pos,
        gboolean always_show_tabs);

inline static MultiWin *multi_win_new_with_geom(const char *display_name,
        Options *shortcuts,
        int zoom_index, gpointer user_data_template,
        const char *geom, int numtabs, GtkPositionType tab_pos,
        gboolean always_show_tabs)
{
    return multi_win_new_full(display_name, shortcuts, zoom_index,
                  user_data_template, geom, MULTI_WIN_DEFAULT_SIZE,
                  numtabs, tab_pos, always_show_tabs);
}

inline static MultiWin *multi_win_new(const char *display_name,
        Options *shortcuts, int zoom_index,
        gpointer user_data_template, int numtabs, GtkPositionType tab_pos,
        gboolean always_show_tabs)
{
    return multi_win_new_full(display_name, shortcuts, zoom_index,
                  user_data_template, NULL, MULTI_WIN_DEFAULT_SIZE,
                  numtabs, tab_pos, always_show_tabs);
}

/* Creates a new window that's the same as the old one except with a different
 * profile etc
 */
MultiWin *multi_win_clone(MultiWin *old,
        gpointer user_data_template, gboolean always_show_tabs);

inline static MultiWin *multi_win_new_fullscreen(const char *display_name,
        Options *shortcuts,
        int zoom_index, gpointer user_data_template,
        int numtabs, GtkPositionType tab_pos,
        gboolean always_show_tabs)
{
    return multi_win_new_full(display_name, shortcuts, zoom_index,
                  user_data_template, NULL, MULTI_WIN_FULL_SCREEN,
                  numtabs, tab_pos, always_show_tabs);
}

inline static MultiWin *multi_win_new_maximised(const char *display_name,
        Options *shortcuts,
        int zoom_index, gpointer user_data_template,
        int numtabs, GtkPositionType tab_pos,
        gboolean always_show_tabs)
{
    return multi_win_new_full(display_name, shortcuts, zoom_index,
                  user_data_template, NULL, MULTI_WIN_MAXIMISED,
                  numtabs, tab_pos, always_show_tabs);
}

/* Create a new window without a new child tab, ready to have an existing
 * tab moved to it. Window isn't shown yet because we don't know how big we
 * want it to be until it contains a tab. */
MultiWin *multi_win_new_blank(const char *display_name,
        Options *shortcuts, int zoom_index,
        gboolean disable_menu_shortcuts, gboolean disable_tab_shortcuts,
        GtkPositionType tab_pos, gboolean always_show_tabs);

/* Creates a new "blank" window ready for a dragged tab at given coords */
MultiWin *multi_win_new_for_tab(const char *display_name, int x, int y,
        MultiTab *tab);

void multi_win_show(MultiWin *win);

/* Deletes a window, clearing up its data and removing it from main list, also
 * destroying the GtkWindow if it hasn't been destroyed already. When all
 * windows are destroyed it calls gtk_main_quit() */
void multi_win_delete(MultiWin *);

void
multi_win_set_geometry_hints(MultiWin * win, GtkWidget * child,
    GdkGeometry * geometry, GdkWindowHints geom_mask);

void multi_win_set_fullscreen(MultiWin *win, gboolean fullscreen);

GtkWidget *multi_win_get_widget(MultiWin * win);

gboolean multi_win_is_fullscreen(MultiWin *win);

gboolean multi_win_is_maximised(MultiWin *win);

int multi_win_get_zoom_index(MultiWin *win);

double multi_win_get_zoom_factor(MultiWin *win);

int multi_win_get_nearest_index_for_zoom(double factor);

/* Adds signal handlers for "activate" to an item in both menus;
 * popup_id and bar_id are for returning the signal handler ids returned by
 * g_signal_connect; they can be NULL if you don't need to know them */
void
multi_win_menu_connect_data(MultiWin *win, MenuTreeID id,
    GCallback handler, gpointer user_data, GConnectFlags flags,
    gulong *popup_id, gulong *bar_id, gulong *short_pupup_id);

inline static void
multi_win_menu_connect(MultiWin * win, MenuTreeID id,
        GCallback handler, gpointer user_data,
        gulong *popup_id, gulong *bar_id, gulong *short_popup_id)
{
    multi_win_menu_connect_data(win, id, handler, user_data, 0,
            popup_id, bar_id, short_popup_id);
}

inline static void
multi_win_menu_connect_swapped(MultiWin * win, MenuTreeID id,
        GCallback handler, gpointer user_data,
        gulong *popup_id, gulong *bar_id, gulong *short_popup_id)
{
    multi_win_menu_connect_data(win, id, handler, user_data,
        G_CONNECT_SWAPPED, popup_id, bar_id, short_popup_id);
}

MultiTab *multi_win_get_current_tab(MultiWin * win);

guint multi_win_get_ntabs(MultiWin * win);

gpointer multi_win_get_user_data_for_current_tab(MultiWin * win);

MenuTree *multi_win_get_menu_bar(MultiWin * win);

MenuTree *multi_win_get_popup_menu(MultiWin * win);

MenuTree *multi_win_get_short_popup_menu(MultiWin * win);

Options *multi_win_get_shortcut_scheme(MultiWin * win);

/* reapply is for forcing an update of the current scheme */
void multi_win_set_shortcut_scheme(MultiWin * win, Options *, gboolean reapply);

GtkAccelGroup *multi_win_get_accel_group(MultiWin * win);

void
multi_win_set_show_menu_bar(MultiWin * win, gboolean show);

gboolean multi_win_get_show_menu_bar(MultiWin * win);

typedef enum {
    MultiWinScrollBar_None,
    MultiWinScrollBar_Right,
    MultiWinScrollBar_Left,
    MultiWinScrollBar_Query        /* No change, just query current value */
} MultiWinScrollBar_Position;

/* Sets scrollbar position (will be ignored if already set), and returns the
 * current value in case the new value had to be ignored */
MultiWinScrollBar_Position multi_win_set_scroll_bar_position(MultiWin * win,
    MultiWinScrollBar_Position new_pos);

/* Returns the window under the pointer or NULL if the window isn't a terminal
 * window owned by this app */
MultiWin *multi_win_get_win_under_pointer(void);

/* Make sure this win's current tab claims focus for this win */
void multi_win_restore_focus(MultiWin *win);

/* Ignore toggle signals on menu items because they're being manipulated by
 * code, not user */
void multi_win_set_ignore_toggles(MultiWin *win, gboolean ignore);

gboolean multi_win_get_ignore_toggles(MultiWin *win);

void multi_win_select_tab(MultiWin *win, MultiTab *tab);

void multi_win_set_wrap_switch_tab(MultiWin *win, gboolean wrap);

/* Apply a function to all tabs in one window */
typedef void (*MultiWinForEachTabFunc)(MultiTab *tab, void *user_data);
void multi_win_foreach_tab(MultiWin *, MultiWinForEachTabFunc,
        gpointer user_data);

GtkPositionType multi_win_get_tab_pos(MultiWin *win);

gboolean multi_win_get_always_show_tabs(MultiWin *win);

void multi_win_set_always_show_tabs(MultiWin *win, gboolean show);

/* List of all known windows - treat as read-only */
extern GList *multi_win_all;

/* Sets a title template string. %s is substituted with the current tab's title.
 * There may only be one %s and no other % characters except %%.
 * NULL is equivalent to "%s".
 */
void multi_win_set_title_template(MultiWin *win, const char *tt);

const char *multi_win_get_title_template(MultiWin *win);

const char *multi_win_get_title(MultiWin *win);

void multi_win_set_title(MultiWin *win, const char *);

void multi_win_set_title_template_locked(MultiWin *win, gboolean locked);

gboolean multi_win_get_title_template_locked(MultiWin *win);

/* Whether window is managed by a compositor */
gboolean multi_win_composite(MultiWin *win);

const char *multi_win_get_display_name(MultiWin *win);

const char *multi_win_get_shortcuts_scheme_name(MultiWin *win);

guint multi_win_get_num_tabs(MultiWin *win);

#endif /* MULTITAB_H */

/* vi:set sw=4 ts=4 et cindent cino= */
