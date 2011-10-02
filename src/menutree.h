#ifndef MENUTREE_H
#define MENUTREE_H
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


/* Handles the menu tree */

#include "options.h"

/* These are upper case because they could be replaced with string macros if
 * we were to build menus from some external definition */

typedef enum {
    MENUTREE_NULL_ID = -1,

    MENUTREE_OPEN_IN_BROWSER,
    MENUTREE_OPEN_IN_MAILER,
    MENUTREE_OPEN_IN_FILER,
    MENUTREE_COPY_URI,
    MENUTREE_URI_SEPARATOR,

    MENUTREE_FILE,
    MENUTREE_EDIT,
    MENUTREE_VIEW,
#ifdef HAVE_VTE_TERMINAL_SEARCH_SET_GREGEX
    MENUTREE_SEARCH,
#endif
    MENUTREE_PREFERENCES,
    MENUTREE_TABS,
    MENUTREE_HELP,

    MENUTREE_FILE_NEW_WINDOW,
    MENUTREE_FILE_NEW_TAB,
    MENUTREE_FILE_NEW_WINDOW_WITH_PROFILE,
    MENUTREE_FILE_NEW_WINDOW_WITH_PROFILE_HEADER,
    MENUTREE_FILE_NEW_TAB_WITH_PROFILE,
    MENUTREE_FILE_NEW_TAB_WITH_PROFILE_HEADER,
    MENUTREE_FILE_CLOSE_TAB,
    MENUTREE_FILE_CLOSE_WINDOW,

    MENUTREE_EDIT_COPY,
    MENUTREE_EDIT_PASTE,
    MENUTREE_EDIT_COPY_AND_PASTE,
    MENUTREE_EDIT_SET_WINDOW_TITLE,
    MENUTREE_EDIT_RESET,
    MENUTREE_EDIT_RESET_AND_CLEAR,
    MENUTREE_EDIT_RESPAWN,

    MENUTREE_VIEW_SHOW_MENUBAR,
    MENUTREE_VIEW_SHOW_TAB_BAR,
    MENUTREE_VIEW_FULLSCREEN,
    MENUTREE_VIEW_ZOOM_IN,
    MENUTREE_VIEW_ZOOM_OUT,
    MENUTREE_VIEW_ZOOM_NORM,
    MENUTREE_VIEW_SCROLL_UP,
    MENUTREE_VIEW_SCROLL_DOWN,
    MENUTREE_VIEW_SCROLL_PAGE_UP,
    MENUTREE_VIEW_SCROLL_PAGE_DOWN,
    MENUTREE_VIEW_SCROLL_TO_TOP,
    MENUTREE_VIEW_SCROLL_TO_BOTTOM,

#ifdef HAVE_VTE_TERMINAL_SEARCH_SET_GREGEX
    MENUTREE_SEARCH_FIND,
    MENUTREE_SEARCH_FIND_NEXT,
    MENUTREE_SEARCH_FIND_PREVIOUS,
#endif

    MENUTREE_PREFERENCES_PROFILES,
    MENUTREE_PREFERENCES_SELECT_PROFILE,
    MENUTREE_PREFERENCES_SELECT_COLOUR_SCHEME,
    MENUTREE_PREFERENCES_SELECT_SHORTCUTS,
    MENUTREE_PREFERENCES_EDIT_CURRENT_PROFILE,
    MENUTREE_PREFERENCES_EDIT_CURRENT_COLOUR_SCHEME,
    MENUTREE_PREFERENCES_CONFIG_MANAGER,
    MENUTREE_PREFERENCES_CHARACTER_ENCODING,
    MENUTREE_PREFERENCES_INPUT_METHODS,

    MENUTREE_TABS_DETACH_TAB,
    MENUTREE_TABS_CLOSE_TAB,
    MENUTREE_TABS_CLOSE_OTHER_TABS,
    MENUTREE_TABS_NAME_TAB,
    MENUTREE_TABS_PREVIOUS_TAB,
    MENUTREE_TABS_NEXT_TAB,
    MENUTREE_TABS_MOVE_TAB_LEFT,
    MENUTREE_TABS_MOVE_TAB_RIGHT,

    MENUTREE_HELP_SHOW_MANUAL,
    MENUTREE_HELP_ABOUT,


    MENUTREE_NUM_IDS,

    MENUTREE_TABS_FIRST_FIXED = MENUTREE_TABS_DETACH_TAB,
    MENUTREE_TABS_LAST_FIXED = MENUTREE_TABS_MOVE_TAB_RIGHT + 1,
                                /* +1 for separator */
    MENUTREE_TABS_FIRST_DYNAMIC =
    MENUTREE_TABS_LAST_FIXED - MENUTREE_TABS_FIRST_FIXED + 3
    /* +3 because there are 3 separators in total */
} MenuTreeID;

typedef struct MenuTree MenuTree;
struct MenuTree {
    GtkWidget *top_level;
    gboolean being_deleted;
    void (*deleted_handler) (MenuTree *, gpointer);
    gpointer deleted_data;
    GtkWidget *item_widgets[MENUTREE_NUM_IDS];
    gpointer user_data;
    GtkAccelGroup *accel_group;
    GList *tabs;    /* List of GtkMenuItem widgets */
    int ntabs;
    Options *shortcuts;
    GtkWidget *encodings;    /* Encodings submenu */
    GSList *encodings_group;
    int n_encodings;
    gboolean disable_shortcuts;
    gboolean disable_tab_shortcuts;
    GtkWidget *new_win_profiles_menu;
    GtkWidget *new_tab_profiles_menu;
};

/* Builds a menu tree. The GType should either be GTK_TYPE_MENU_BAR or
 * GTK_TYPE_MENU.
 */
MenuTree *menutree_new(Options *shortcuts, GtkAccelGroup * accel_group,
    GType menu_type, gboolean disable_shortcuts,
    gboolean disable_tab_shortcuts, gpointer user_data);

/* Creates the short version of the popup menu, used as the context menu when
 * the menu bar is enabled.
 * Must not be called before menutree_new has been called for first time.
 */
MenuTree *menutree_new_short_popup(Options *shortcuts,
        GtkAccelGroup *accel_group, gboolean disable_shortcuts,
        gpointer user_data);

/* A "raw" handler for "toggled" signals on encodings items. The user_data
 * argument will be as passed to menutree_new. The client should work out
 * whether the widget is being toggled on or off and use
 * menutree_encoding_from_widget to get the encoding name. */
typedef void (*MenuTreeToggledHandler)(GtkCheckMenuItem *, gpointer);

void menutree_build_encodings_menu(MenuTree *, char const **encodings,
        MenuTreeToggledHandler);

const char *menutree_encoding_from_widget(GtkCheckMenuItem *);

void menutree_add_encoding(MenuTree *, const char *encoding,
        MenuTreeToggledHandler);

void menutree_remove_encoding(MenuTree *, const char *encoding);

void menutree_change_encoding(MenuTree *, const char *old_encoding,
        const char *new_encoding, MenuTreeToggledHandler);

void menutree_select_encoding(MenuTree *, const char *encoding);


void menutree_delete(MenuTree *);

/* Gets the widget at the top-level (menu bar or popup menu) */
inline static GtkWidget *menutree_get_top_level_widget(MenuTree * tree)
{
    return tree->top_level;
}

inline static GtkWidget *menutree_get_widget_for_id(MenuTree *tree,
    MenuTreeID id)
{
    return tree->item_widgets[id];
}

GtkMenu *menutree_submenu_from_id(MenuTree *mtree, MenuTreeID id);

inline static void
menutree_shade(MenuTree * tree, MenuTreeID id, gboolean shade)
{
    gtk_widget_set_sensitive(tree->item_widgets[id], !shade);
}

/* Supply a function to be called when a menutree is destroyed */
void
menutree_connect_destroyed(MenuTree * tree,
    GCallback callback, gpointer user_data);

/* Adds a tab to the tabs menu. Returns the GtkMenuItem widget. Caller's
 * responsibility to connect a signal handler to it; position starts at 0
 * for first tab, not actual position in menu (due to Next/Previous Tab and
 * separator), or -1 to append. */
GtkWidget *menutree_add_tab_at_position(MenuTree * tree, const char *title,
        int position);

inline static GtkWidget *menutree_add_tab(MenuTree * tree, const char *title)
{
    return menutree_add_tab_at_position(tree, title, -1);
}

GtkWidget *menutree_change_tab_title(MenuTree * tree,
    GtkWidget *widget, const char *title);


void menutree_set_show_menu_bar_active(MenuTree * tree, gboolean active);
void menutree_set_show_tab_bar_active(MenuTree *tree, gboolean active);
void menutree_set_fullscreen_active(MenuTree *tree, gboolean active);

gulong menutree_named_signal_connect_data(MenuTree * tree, MenuTreeID id,
    const char *signame, GCallback handler, gpointer user_data,
    GConnectFlags flags);

inline static gulong menutree_named_signal_connect(MenuTree *tree,
    MenuTreeID id, const char *signame, GCallback handler, gpointer user_data)
{
    return menutree_named_signal_connect_data(tree, id, signame, handler,
            user_data, 0);
}

inline static gulong menutree_named_signal_connect_swapped(MenuTree *tree,
    MenuTreeID id, const char *signame, GCallback handler, gpointer user_data)
{
    return menutree_named_signal_connect_data(tree, id, signame, handler,
            user_data, G_CONNECT_SWAPPED);
}

/* Connects a handler for an item's "activate" signal */
inline static gulong menutree_signal_connect_data(MenuTree *tree, MenuTreeID id,
    GCallback handler, gpointer user_data, GConnectFlags flags)
{
    return menutree_named_signal_connect_data(tree, id, "activate", handler,
            user_data, flags);
}

inline static gulong
menutree_signal_connect(MenuTree * tree, MenuTreeID id,
    GCallback handler, gpointer user_data)
{
    return menutree_signal_connect_data(tree, id, handler, user_data, 0);
}

inline static gulong
menutree_signal_connect_swapped(MenuTree * tree, MenuTreeID id,
    GCallback handler, gpointer user_data)
{
    return menutree_signal_connect_data(tree, id, handler, user_data,
        G_CONNECT_SWAPPED);
}

void menutree_set_show_item(MenuTree * tree, MenuTreeID id, gboolean show);

/* Attach an Input Methods submenu for the current tab */
void menutree_attach_im_submenu(MenuTree *tree, GtkWidget *submenu);

void menutree_apply_shortcuts(MenuTree *tree, Options *shortcuts);

/* Use with gtk_container_foreach to find the group of the last radio menu
 * item in a menu. data must point to a GSList * where the group is stored */
void menutree_find_radio_group(GtkWidget *widget, gpointer data);

/* Remove an item from the tabs menu; menu_item is the widget returned by
 * menutree_add_tab */
void menutree_remove_tab(MenuTree * tree, GtkWidget * menu_item);

inline static void menutree_select_tab(MenuTree * tree, GtkWidget * menu_item)
{
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
}

void menutree_disable_shortcuts(MenuTree *tree, gboolean disable);

void menutree_disable_tab_shortcuts(MenuTree *tree, gboolean disable);

/* Change left/right to up/down */
void menutree_change_move_tab_labels(MenuTree *tree);

#endif /* MENUTREE_H */

/* vi:set sw=4 ts=4 et cindent cino= */
