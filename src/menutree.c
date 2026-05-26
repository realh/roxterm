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


#include "defns.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "menutree.h"
#include "gtk/gtk.h"
#include "shortcuts.h"

// menutree_labels includes mnemonic underscores so that translators don't
// have to also translate the version with underscores stripped.
static char const *menutree_labels[MENUTREE_NUM_IDS];
static gboolean filled_labels = FALSE;

static GtkWidget *menutree_append_new_item_with_mnemonic(
        GtkMenuShell *shell, const char *label)
{
    GtkWidget *item = gtk_menu_item_new_with_mnemonic(label);

    gtk_menu_shell_append(shell, item);
    return item;
}

static char *strip_underscore(const char *in)
{
    char *out = g_new(char, strlen(in) + 1);
    int n, m;

    for (n = 0, m = 0; in[n]; ++n)
    {
        if (in[n] != '_')
            out[m++] = in[n];
    }
    out[m] = 0;
    return out;
}

/* Builds a menu; va_list is a series of pairs of labels and IDs, terminated
 * by a NULL label; use "_" for a separator, and MENUTREE_NULL_ID for any item
 * you don't want stored in item_widgets */
static void menutree_build_shell_ap(MenuTree *menu_tree, GtkMenuShell *shell,
        va_list ap)
{
    char *label;
    MenuTreeID id;

    while ((label = va_arg(ap, char *)) != NULL)
    {
        GtkWidget *item = NULL;

        id = va_arg(ap, MenuTreeID);
        if (!strcmp(label, "_"))
        {
            item = gtk_separator_menu_item_new();
            gtk_menu_shell_append(shell, item);
        }
        else
        {
            if (!filled_labels &&
                    id < MENUTREE_NUM_IDS && id != MENUTREE_NULL_ID)
            {
                menutree_labels[id] = label;
            }
            label = dgettext(PACKAGE, label);
            char *stripped = NULL;
            if (menu_tree->disable_shortcuts)
            {
                label = strip_underscore(label);
                stripped = label;
            }
            item = NULL;
            switch (id)
            {
                case MENUTREE_VIEW_SHOW_MENUBAR:
                case MENUTREE_VIEW_SHOW_TAB_BAR:
                case MENUTREE_VIEW_FULLSCREEN:
                case MENUTREE_VIEW_BORDERLESS:
                    item = gtk_check_menu_item_new_with_mnemonic(label);
                    gtk_menu_shell_append(shell, item);
                    break;
                default:
                    item = menutree_append_new_item_with_mnemonic(shell, label);
                    break;
            }
            g_free(stripped);
        }
        if (id != MENUTREE_NULL_ID)
        {
            menu_tree->item_widgets[id] = item;
        }
    }
}

/* See above for description of variable args */
static void menutree_build_shell(MenuTree *menu_tree, GtkMenuShell * shell, ...)
{
    va_list ap;

    va_start(ap, shell);
    menutree_build_shell_ap(menu_tree, shell, ap);
    va_end(ap);
}

#define URI_MENU_ITEMS \
        N_("_SSH to host"), MENUTREE_SSH_HOST, \
        N_("_Open link"), MENUTREE_OPEN_IN_BROWSER, \
        N_("Send e_mail"), MENUTREE_OPEN_IN_MAILER, \
        N_("_Open file/directory in filer"), MENUTREE_OPEN_IN_FILER, \
        N_("_Call"), MENUTREE_VOIP_CALL, \
        N_("_Copy address to clipboard"), MENUTREE_COPY_URI, \
        N_("_"), MENUTREE_URI_SEPARATOR

#define MENUTREE_SEARCH_ITEM N_("_Search"), MENUTREE_SEARCH,
#define TOP_LEVEL_MENU_ITEMS \
        N_("_File"), MENUTREE_FILE, \
        N_("_Edit"), MENUTREE_EDIT, \
        N_("_View"), MENUTREE_VIEW, \
        MENUTREE_SEARCH_ITEM \
        N_("_Preferences"), MENUTREE_PREFERENCES, \
        N_("Ta_bs"), MENUTREE_TABS, \
        N_("_Help"), MENUTREE_HELP

#define COPY_PASTE_MENU_ITEMS \
        N_("Select _All"), MENUTREE_EDIT_SELECT_ALL, \
        N_("_Copy"), MENUTREE_EDIT_COPY, \
        N_("_Paste"), MENUTREE_EDIT_PASTE, \
        N_("Cop_y & Paste"), MENUTREE_EDIT_COPY_AND_PASTE, \
        "_", MENUTREE_NULL_ID

#define RESET_MENU_ITEMS \
        N_("_Reset"), MENUTREE_EDIT_RESET, \
        N_("Reset And C_lear"), MENUTREE_EDIT_RESET_AND_CLEAR

#define SHOW_MENU_BAR_ITEM \
        N_("Show Menu_bar"), MENUTREE_VIEW_SHOW_MENUBAR

#define PREFS_ITEMS1 \
        N_("Select _Profile"), MENUTREE_PREFERENCES_SELECT_PROFILE, \
        N_("Select _Colour Scheme"), MENUTREE_PREFERENCES_SELECT_COLOUR_SCHEME, \
        N_("Select _Shortcuts Scheme"), MENUTREE_PREFERENCES_SELECT_SHORTCUTS, \
        "_", MENUTREE_NULL_ID, \
        N_("_Edit Current Profile"), MENUTREE_PREFERENCES_EDIT_CURRENT_PROFILE, \
        N_("E_dit Current Colour Scheme"), \
            MENUTREE_PREFERENCES_EDIT_CURRENT_COLOUR_SCHEME

#define PREFS_ITEMS2 \
        "_", MENUTREE_NULL_ID, \
        N_("Configuration _Manager"), MENUTREE_PREFERENCES_CONFIG_MANAGER, \
        "_", MENUTREE_NULL_ID, \
        NULL

GtkMenu *menutree_submenu_from_id(MenuTree *mtree, MenuTreeID id)
{
    GtkWidget *item = menutree_get_widget_for_id(mtree, id);
    return item ?
            GTK_MENU(gtk_menu_item_get_submenu(GTK_MENU_ITEM(item))) : NULL;
}

static char *get_accel_path(Options *shortcuts, const char *branch_name)
{
    char *s = g_strjoin("/", ACCEL_PATH,
            shortcuts_get_index_str(shortcuts), branch_name, NULL);
    // size_t l = strlen(s);
    // if (l >= 4 && !strcmp(s + l - 3, "..."))
    // {
    //     g_debug("GAP: Stripping ... from '%s'", s);
    //     s[l - 3] = 0;
    //     g_debug("GAP: Stripped:          '%s'", s);
    // }
    // else if (g_str_has_suffix(branch_name, "..."))
    // {
    //     g_critical("GAP: Missed stripping ... from '%s'", s);
    // }
    // else if (strstr(s, "Find"))
    // {
    //     g_debug("GAP: Not stripping ... from '%s'", s);
    // }
    return s;
}

static void
menutree_set_accel_path_for_item(MenuTree * tree, MenuTreeID id,
    const char *path_leaf)
{
    GtkWidget *item = tree->item_widgets[id];
    char *full_path;

    if (!item)
        return;
    full_path = get_accel_path(tree->shortcuts, path_leaf);
    gtk_menu_item_set_accel_path(GTK_MENU_ITEM(item), full_path);
    g_free(full_path);
}

static void
menutree_set_accel_path_for_item_range(MenuTree *mtree, MenuTreeID first_item,
        MenuTreeID last_item, const char *menu_branch)
{
    for (MenuTreeID item_id = first_item; item_id <= last_item; ++item_id)
    {
        // Skip items with submenus that aren't at the start or end of a range
        if (item_id >= MENUTREE_FILE_NEW_WINDOW_WITH_PROFILE &&
                item_id <= MENUTREE_FILE_NEW_TAB_WITH_PROFILE_HEADER)
        {
            continue;
        }
        char *leaf = strip_underscore(menutree_labels[item_id]);
        char *path;
        if (leaf[0] != 0 && menu_branch[0] != 0)
        {
            path = g_strjoin("/", menu_branch, leaf, NULL);
        }
        else
        {
            path = g_strdup_printf("%s%s", menu_branch, leaf);
        }
        menutree_set_accel_path_for_item(mtree, item_id, path);
        g_free(path);
        g_free(leaf);
    }
}

static void
menutree_set_accel_path_for_submenu(MenuTree *mtree, MenuTreeID id,
        const char *menu_branch)
{
    GtkMenu *menu = menutree_submenu_from_id(mtree, id);
    if (!menu)
    {
        g_critical("No submenu has ID %d for branch %s", id, menu_branch);
        return;
    }
    gtk_menu_set_accel_group(menu, mtree->accel_group);

    // This is rather hacky, depends on the menu layout not changing much.
    MenuTreeID first_item, last_item;
    switch (id)
    {
        case MENUTREE_FILE:
            // Some items need to be skipped, but they're in the middle of the
            // menu. See below.
            first_item = MENUTREE_FILE_NEW_WINDOW;
            last_item = MENUTREE_FILE_SAVE_SESSION;
            break;
        case MENUTREE_EDIT:
            first_item = MENUTREE_EDIT_SELECT_ALL;
            last_item = MENUTREE_EDIT_RESPAWN;
            break;
        case MENUTREE_VIEW:
            first_item = MENUTREE_VIEW_SHOW_MENUBAR;
            last_item = MENUTREE_VIEW_SCROLL_TO_BOTTOM;
            break;
        case MENUTREE_SEARCH:
            first_item = MENUTREE_SEARCH_FIND;
            last_item = MENUTREE_SEARCH_FIND_PREVIOUS;
            break;
        case MENUTREE_PREFERENCES:
            // SELECT_PROFILE etc need to be skipped, but luckily they're at
            // the top.
            first_item = MENUTREE_PREFERENCES_EDIT_CURRENT_PROFILE;
            last_item = MENUTREE_PREFERENCES_CONFIG_MANAGER;
            break;
        case MENUTREE_TABS:
            first_item = MENUTREE_TABS_DETACH_TAB;
            last_item = MENUTREE_TABS_MOVE_TAB_RIGHT;
            break;
        case MENUTREE_HELP:
            first_item = MENUTREE_SEARCH_FIND;
            last_item = MENUTREE_SEARCH_FIND_PREVIOUS;
            break;
        default:
            g_critical("Invalid submenu ID %d for branch %s", id, menu_branch);
            return;
    }
    menutree_set_accel_path_for_item_range(mtree, first_item, last_item,
            menu_branch);
    if (id == MENUTREE_FILE)
    {
        // These are only present in the long popup menu, but they'll be
        // ignored if they aren't present.
        menutree_set_accel_path_for_item_range(mtree,
                MENUTREE_SSH_HOST, MENUTREE_COPY_URI, "");
    }
}

static void
menutree_set_accel_path_for_tab(MenuTree * tree, int tab)
{
    char *leaf = NULL;
    char *full_path = NULL;

    if (!tree->tabs || tab >= 10)
        return;
    if (!tree->disable_tab_shortcuts && tree->ntabs != 1)
    {
        leaf = g_strdup_printf("Tabs/Select_Tab_%d", tab);
        full_path = get_accel_path(tree->shortcuts, leaf);
    }
    shortcuts_enable_signal_handler(FALSE);
    gtk_menu_item_set_accel_path(
            GTK_MENU_ITEM(g_list_nth_data(tree->tabs, tab)),
            full_path);
    g_free(full_path);
    g_free(leaf);
    shortcuts_enable_signal_handler(TRUE);
}

static void menutree_apply_tab_shortcuts(MenuTree *mtree)
{
    int n;

    for (n = 0; n < mtree->ntabs; ++n)
        menutree_set_accel_path_for_tab(mtree, n);
}

void menutree_apply_shortcuts(MenuTree *tree, Options *shortcuts)
{
    GtkMenu *submenu;
    char *accel_path;

    tree->shortcuts = shortcuts;
    shortcuts_enable_signal_handler(FALSE);
    if (GTK_IS_MENU(tree->top_level))
    {
        gtk_menu_set_accel_group(GTK_MENU(tree->top_level), tree->accel_group);
    }
    menutree_set_accel_path_for_submenu(tree, MENUTREE_FILE, "File");
    menutree_set_accel_path_for_submenu(tree, MENUTREE_EDIT, "Edit");
    menutree_set_accel_path_for_submenu(tree, MENUTREE_VIEW, "View");
    menutree_set_accel_path_for_submenu(tree, MENUTREE_SEARCH, "Search");
    menutree_set_accel_path_for_submenu(tree, MENUTREE_PREFERENCES,
            "Preferences");
    menutree_set_accel_path_for_submenu(tree, MENUTREE_HELP, "Help");

    submenu = GTK_MENU(tree->new_win_profiles_menu);
    if (submenu)
    {
        gtk_menu_set_accel_group(submenu, tree->accel_group);
        accel_path = get_accel_path(tree->shortcuts,
                "File/New Window With Profile");
        gtk_menu_set_accel_path(submenu, accel_path);
        g_free(accel_path);
    }
    submenu = GTK_MENU(tree->new_tab_profiles_menu);
    if (submenu)
    {
        gtk_menu_set_accel_group(submenu, tree->accel_group);
        accel_path = get_accel_path(tree->shortcuts,
                "File/New Tab With Profile");
        gtk_menu_set_accel_path(submenu, accel_path);
        g_free(accel_path);
    }

    /* Tabs have shortcuts set dynamically so set paths
     * for fixed items individually */
    // submenu = menutree_submenu_from_id(tree, MENUTREE_TABS);
    // if (submenu)
    //     gtk_menu_set_accel_group(submenu, tree->accel_group);
    menutree_set_accel_path_for_item(tree,
            MENUTREE_FILE_NEW_WINDOW_WITH_PROFILE_HEADER,
            "File/New Window With Profile/Profiles");
    menutree_set_accel_path_for_item(tree,
            MENUTREE_FILE_NEW_TAB_WITH_PROFILE_HEADER,
            "File/New Tab With Profile/Profiles");
    // menutree_set_accel_path_for_item(tree, MENUTREE_TABS_DETACH_TAB,
    //         "Tabs/Detach Tab");
    // menutree_set_accel_path_for_item(tree, MENUTREE_TABS_CLOSE_TAB,
    //         "Tabs/Close Tab");
    // menutree_set_accel_path_for_item(tree, MENUTREE_TABS_CLOSE_OTHER_TABS,
    //         "Tabs/Close Other Tabs");
    // menutree_set_accel_path_for_item(tree, MENUTREE_TABS_NAME_TAB,
    //         "Tabs/Name Tab...");
    // menutree_set_accel_path_for_item(tree, MENUTREE_TABS_NEXT_TAB,
    //         "Tabs/Next Tab");
    // menutree_set_accel_path_for_item(tree, MENUTREE_TABS_PREVIOUS_TAB,
    //         "Tabs/Previous Tab");
    // menutree_set_accel_path_for_item(tree, MENUTREE_TABS_MOVE_TAB_LEFT,
    //         "Tabs/Move Tab Left");
    // menutree_set_accel_path_for_item(tree, MENUTREE_TABS_MOVE_TAB_RIGHT,
    //         "Tabs/Move Tab Right");
    menutree_set_accel_path_for_submenu(tree, MENUTREE_HELP, "Help");
    menutree_apply_tab_shortcuts(tree);
    shortcuts_enable_signal_handler(TRUE);
}

/*
static void submenu_destroy_handler(GtkWidget *menu, gpointer handle)
{
    g_debug("Destroying submenu %p", menu);
}
*/

/* Creates top-level menubar or popup menu with submenus */
static void menutree_build(MenuTree *menu_tree, Options *shortcuts,
        GType menu_type)
{
    GtkWidget *submenu;

    menu_tree->top_level = menu_type == GTK_TYPE_MENU_BAR ?
        gtk_menu_bar_new() : gtk_menu_new();
    if (menu_type == GTK_TYPE_MENU_BAR)
    {
        menutree_build_shell(menu_tree, GTK_MENU_SHELL(menu_tree->top_level),
                TOP_LEVEL_MENU_ITEMS, NULL);
    }
    else
    {
        menutree_build_shell(menu_tree, GTK_MENU_SHELL(menu_tree->top_level),
            URI_MENU_ITEMS, TOP_LEVEL_MENU_ITEMS, NULL);
    }

    submenu = gtk_menu_new();
    /*
    g_debug("Creating File submenu %p", submenu);
    g_signal_connect(submenu, "destroy",
            G_CALLBACK(submenu_destroy_handler), NULL);
    */
    menutree_build_shell(menu_tree, GTK_MENU_SHELL(submenu),
        N_("_New Window"), MENUTREE_FILE_NEW_WINDOW,
        N_("New _Tab"), MENUTREE_FILE_NEW_TAB,
        N_("New _Window With Profile"), MENUTREE_FILE_NEW_WINDOW_WITH_PROFILE,
        N_("New Tab With _Profile"), MENUTREE_FILE_NEW_TAB_WITH_PROFILE,
        N_("C_lose Tab"), MENUTREE_FILE_CLOSE_TAB,
        N_("_Close Window"), MENUTREE_FILE_CLOSE_WINDOW,
        "_", MENUTREE_NULL_ID,
        N_("Save Buffer _As..."), MENUTREE_FILE_SAVE_BUFFER_AS,
        N_("Save _Buffer"), MENUTREE_FILE_SAVE_BUFFER,
        "_", MENUTREE_NULL_ID,
        N_("_Save Session..."), MENUTREE_FILE_SAVE_SESSION,
        NULL);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_tree->item_widgets
            [MENUTREE_FILE]), submenu);

    menu_tree->new_win_profiles_menu = gtk_menu_new();
    menutree_build_shell(menu_tree,
            GTK_MENU_SHELL(menu_tree->new_win_profiles_menu),
            N_("Profiles"), MENUTREE_FILE_NEW_WINDOW_WITH_PROFILE_HEADER,
            "_", MENUTREE_NULL_ID,
            NULL);
    gtk_widget_set_sensitive(menutree_get_widget_for_id(menu_tree,
                MENUTREE_FILE_NEW_WINDOW_WITH_PROFILE_HEADER), FALSE);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_tree->item_widgets
            [MENUTREE_FILE_NEW_WINDOW_WITH_PROFILE]),
            menu_tree->new_win_profiles_menu);
    menu_tree->new_tab_profiles_menu = gtk_menu_new();
    menutree_build_shell(menu_tree,
            GTK_MENU_SHELL(menu_tree->new_tab_profiles_menu),
            N_("Profiles"), MENUTREE_FILE_NEW_TAB_WITH_PROFILE_HEADER,
            "_", MENUTREE_NULL_ID,
            NULL);
    gtk_widget_set_sensitive(menutree_get_widget_for_id(menu_tree,
                MENUTREE_FILE_NEW_TAB_WITH_PROFILE_HEADER), FALSE);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_tree->item_widgets
            [MENUTREE_FILE_NEW_TAB_WITH_PROFILE]),
            menu_tree->new_tab_profiles_menu);

    submenu = gtk_menu_new();
    menutree_build_shell(menu_tree, GTK_MENU_SHELL(submenu),
        COPY_PASTE_MENU_ITEMS,
        N_("_Set Window Title..."), MENUTREE_EDIT_SET_WINDOW_TITLE,
        RESET_MENU_ITEMS,
        "_", MENUTREE_NULL_ID,
        N_("Res_tart command"), MENUTREE_EDIT_RESPAWN,
        NULL);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_tree->item_widgets
            [MENUTREE_EDIT]), submenu);

    submenu = gtk_menu_new();
    menutree_build_shell(menu_tree, GTK_MENU_SHELL(submenu),
        SHOW_MENU_BAR_ITEM,
        N_("_Always Show Tab Bar"), MENUTREE_VIEW_SHOW_TAB_BAR,
        N_("_Full Screen"), MENUTREE_VIEW_FULLSCREEN,
        N_("_Borderless"), MENUTREE_VIEW_BORDERLESS,
        "_", MENUTREE_NULL_ID,
        N_("Zoom _In"), MENUTREE_VIEW_ZOOM_IN,
        N_("Zoom _Out"), MENUTREE_VIEW_ZOOM_OUT,
        N_("_Normal Size"), MENUTREE_VIEW_ZOOM_NORM,
        "_", MENUTREE_NULL_ID,
        N_("Scroll _Up One Line"), MENUTREE_VIEW_SCROLL_UP,
        N_("Scroll _Down One Line"), MENUTREE_VIEW_SCROLL_DOWN,
        N_("Scroll Up One _Page"), MENUTREE_VIEW_SCROLL_PAGE_UP,
        N_("Scroll Do_wn One Page"), MENUTREE_VIEW_SCROLL_PAGE_DOWN,
        N_("Scroll Up One _Half Page"), MENUTREE_VIEW_SCROLL_HALF_PAGE_UP,
        N_("Scroll Down One Hal_f Page"), MENUTREE_VIEW_SCROLL_HALF_PAGE_DOWN,
        N_("Scroll To _Top"), MENUTREE_VIEW_SCROLL_TO_TOP,
        N_("Scroll To _Bottom"), MENUTREE_VIEW_SCROLL_TO_BOTTOM,
        NULL);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_tree->item_widgets
            [MENUTREE_VIEW]), submenu);

    submenu = gtk_menu_new();
    menutree_build_shell(menu_tree, GTK_MENU_SHELL(submenu),
        N_("_Find..."), MENUTREE_SEARCH_FIND,
        N_("Find _Next"), MENUTREE_SEARCH_FIND_NEXT,
        N_("Find _Previous"), MENUTREE_SEARCH_FIND_PREVIOUS,
        NULL);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_tree->item_widgets
            [MENUTREE_SEARCH]), submenu);

    submenu = gtk_menu_new();
    menutree_build_shell(menu_tree, GTK_MENU_SHELL(submenu),
        PREFS_ITEMS1,
        N_("Edi_t Current Shortcuts Scheme"),
            MENUTREE_PREFERENCES_EDIT_CURRENT_SHORTCUTS_SCHEME,
        PREFS_ITEMS2);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_tree->item_widgets
            [MENUTREE_PREFERENCES]), submenu);

    submenu = gtk_menu_new();
    menutree_build_shell(menu_tree, GTK_MENU_SHELL(submenu),
        N_("_Detach Tab"), MENUTREE_TABS_DETACH_TAB,
        N_("_Close Tab"), MENUTREE_TABS_CLOSE_TAB,
        N_("Close _Other Tabs"), MENUTREE_TABS_CLOSE_OTHER_TABS,
        N_("Na_me Tab..."), MENUTREE_TABS_NAME_TAB,
        "_", MENUTREE_NULL_ID,
        N_("_Previous Tab"), MENUTREE_TABS_PREVIOUS_TAB,
        N_("_Next Tab"), MENUTREE_TABS_NEXT_TAB,
        "_", MENUTREE_NULL_ID,
        N_("Move Tab _Left"), MENUTREE_TABS_MOVE_TAB_LEFT,
        N_("Move Tab _Right"), MENUTREE_TABS_MOVE_TAB_RIGHT,
        "_", MENUTREE_NULL_ID, NULL);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_tree->item_widgets
            [MENUTREE_TABS]), submenu);

    submenu = gtk_menu_new();
    menutree_build_shell(menu_tree, GTK_MENU_SHELL(submenu),
        N_("Show _Manual"), MENUTREE_HELP_SHOW_MANUAL,
        N_("_About ROXTerm"), MENUTREE_HELP_ABOUT, NULL);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_tree->item_widgets
            [MENUTREE_HELP]), submenu);
    menutree_apply_shortcuts(menu_tree, shortcuts);
}

static void menutree_build_short_popup(MenuTree *menu_tree, Options *shortcuts,
        GType menu_type)
{
    (void) menu_type;
    menu_tree->top_level = gtk_menu_new();
    menu_tree->shortcuts = shortcuts;
    menutree_build_shell(menu_tree, GTK_MENU_SHELL(menu_tree->top_level),
        URI_MENU_ITEMS,
        COPY_PASTE_MENU_ITEMS,
        RESET_MENU_ITEMS,
        SHOW_MENU_BAR_ITEM,
        NULL);
    gtk_menu_set_accel_group(GTK_MENU(menu_tree->top_level),
            menu_tree->accel_group);
    menutree_set_accel_path_for_item_range(menu_tree,
            MENUTREE_SSH_HOST, MENUTREE_COPY_URI, "");
    menutree_set_accel_path_for_item_range(menu_tree,
            MENUTREE_EDIT_SELECT_ALL, MENUTREE_EDIT_COPY_AND_PASTE, "Edit");
    menutree_set_accel_path_for_item_range(menu_tree,
            MENUTREE_EDIT_RESET, MENUTREE_EDIT_RESET_AND_CLEAR, "Edit");
    menutree_set_accel_path_for_item_range(menu_tree,
        MENUTREE_VIEW_SHOW_MENUBAR, MENUTREE_VIEW_SHOW_MENUBAR, "View");
}

static void menutree_destroy(MenuTree * tree)
{
    //g_debug("Destroying menu tree %p", tree);
    if (tree->deleted_handler)
        tree->deleted_handler(tree, tree->deleted_data);
    if (tree->tabs)
        g_list_free(tree->tabs);
    g_free(tree);
}

static void menutree_destroy_widget_handler(GObject * obj, MenuTree * tree)
{
    (void) obj;

    //g_debug("Menu %p being destroyed", obj);
    tree->top_level = NULL;
    /* tree is no good any more so destroy it */
    menutree_destroy(tree);
}

static MenuTree *menutree_new_common(Options *shortcuts,
        GtkAccelGroup *accel_group,
        GType menu_type,
        void (*builder)(MenuTree *menu_tree, Options *shortcuts,
                GType menu_type),
        gboolean disable_shortcuts,
        gboolean disable_tab_shortcuts, gpointer user_data)
{
    MenuTree *tree = g_new0(MenuTree, 1);
    int n;

    for (n = 0; n < MENUTREE_NUM_IDS; ++n)
    {
        tree->item_widgets[n] = NULL;
    }

    tree->user_data = user_data;
    tree->accel_group = accel_group;
    tree->disable_shortcuts = disable_shortcuts;
    tree->disable_tab_shortcuts = disable_tab_shortcuts;

    builder(tree, shortcuts, menu_type);

    g_signal_connect(tree->top_level,
            "destroy", G_CALLBACK(menutree_destroy_widget_handler), tree);

    gtk_widget_show_all(tree->top_level);
    return tree;
}

MenuTree *menutree_new(Options *shortcuts, GtkAccelGroup *accel_group,
    GType menu_type, gboolean disable_shortcuts,
    gboolean disable_tab_shortcuts, gpointer user_data)
{
    MenuTree *tree = NULL;

    if (!filled_labels)
    {
        int n;

        for (n = 0; n < MENUTREE_NUM_IDS; ++n)
            menutree_labels[n] = NULL;
    }
    tree = menutree_new_common(shortcuts, accel_group, menu_type,
        menutree_build, disable_shortcuts, disable_tab_shortcuts, user_data);
    /*
    g_debug("Created menu %p of type %s", tree->top_level,
            menu_type == GTK_TYPE_MENU_BAR ? "bar" : "popup");
    */
    filled_labels = TRUE;
    return tree;
}

MenuTree *menutree_new_short_popup(Options *shortcuts,
        GtkAccelGroup *accel_group, gboolean disable_shortcuts,
        gpointer user_data)
{
    MenuTree *tree = menutree_new_common(shortcuts, accel_group, GTK_TYPE_MENU,
        menutree_build_short_popup, disable_shortcuts, FALSE, user_data);
    //g_debug("Created short popup menu %p", tree->top_level);
    return tree;
}

void menutree_delete(MenuTree * tree)
{
    g_return_if_fail(tree != NULL);

    if (tree->top_level)
    {
        gtk_widget_destroy(tree->top_level);
    }
    else
    {
        menutree_destroy(tree);
    }
}

void
menutree_connect_destroyed(MenuTree * tree,
    GCallback callback, gpointer user_data)
{
    tree->deleted_handler = (void (*)(MenuTree *, gpointer)) callback;
    tree->deleted_data = user_data;
}

void menutree_find_radio_group(GtkWidget *widget, gpointer data)
{
    GSList **pgroup = data;

    if (g_type_is_a(G_OBJECT_TYPE(G_OBJECT(widget)), GTK_TYPE_RADIO_MENU_ITEM))
        *pgroup = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(widget));
}

inline static GtkMenuShell *menutree_get_submenu_at_id(MenuTree *tree, int id)
{
    return GTK_MENU_SHELL(gtk_menu_item_get_submenu(
                GTK_MENU_ITEM(tree->item_widgets[id])));
}

static void menutree_remove_tab_without_fixing_accels(MenuTree *tree,
        GtkWidget *menu_item)
{
    gtk_widget_destroy(menu_item);
    tree->tabs = g_list_remove(tree->tabs, menu_item);
    --tree->ntabs;
}

void menutree_remove_tab(MenuTree *tree, GtkWidget *menu_item)
{
    menutree_remove_tab_without_fixing_accels(tree, menu_item);
    menutree_apply_tab_shortcuts(tree);
}

static GtkWidget *menutree_tab_menu_item_new(GtkMenuShell *menu,
        const char *title)
{
    /* GtkCheckMenuItem is actually easier to manage than GtkRadioMenuItem due
     * to difficulty of keeping track of group when deleting radio menu items
     */
    GtkWidget *menu_item;
    GSList *group = NULL;

    gtk_container_foreach(GTK_CONTAINER(menu), menutree_find_radio_group,
            &group);
    menu_item = gtk_radio_menu_item_new_with_label(group, title);
    gtk_widget_show(menu_item);
    return menu_item;
}

GtkWidget *menutree_add_tab_at_position(MenuTree * tree, const char *title,
        int position)
{
    GtkMenuShell *submenu = menutree_get_submenu_at_id(tree, MENUTREE_TABS);
    GtkWidget *menu_item = menutree_tab_menu_item_new(submenu, title);

    if (position == -1)
    {
        gtk_menu_shell_append(submenu, menu_item);
        tree->tabs = g_list_append(tree->tabs, menu_item);
        position = tree->ntabs;
    }
    else
    {
        gtk_menu_shell_insert(submenu, menu_item,
                position + MENUTREE_TABS_FIRST_DYNAMIC);
        tree->tabs = g_list_insert(tree->tabs, menu_item, position);
    }
    ++tree->ntabs;
    menutree_apply_tab_shortcuts(tree);
    return menu_item;
}

static void menu_item_change_label(GtkMenuItem *item, const char *label)
{
    GtkWidget *inner_item = gtk_bin_get_child(GTK_BIN(item));

    if (GTK_IS_LABEL(inner_item))
    {
        gtk_label_set_text_with_mnemonic(GTK_LABEL(inner_item), label);
    }
    else if (GTK_IS_CONTAINER(inner_item))
    {
        GList *link;

        for (link = gtk_container_get_children(GTK_CONTAINER(inner_item));
                link; link = g_list_next(link))
        {
            GtkWidget *child = link->data;

            if (GTK_IS_LABEL(child))
            {
                gtk_label_set_text_with_mnemonic(GTK_LABEL(child), label);
                return;
            }
        }
    }
}

inline static void menutree_change_label(MenuTree *tree, MenuTreeID id,
        const char *label)
{
    menu_item_change_label(GTK_MENU_ITEM(menutree_get_widget_for_id(tree, id)),
            label);
}

GtkWidget *menutree_change_tab_title(MenuTree * tree,
    GtkWidget * menu_item, const char *title)
{
    (void) tree;
    menu_item_change_label(GTK_MENU_ITEM(menu_item), title);
    return menu_item;
}

void menutree_change_move_tab_labels(MenuTree *tree)
{
    menutree_change_label(tree, MENUTREE_TABS_MOVE_TAB_LEFT,
            N_("Move Tab _Up"));
    menutree_change_label(tree, MENUTREE_TABS_MOVE_TAB_RIGHT,
            N_("Move Tab _Down"));
}

static void menutree_set_toggle(MenuTree *tree, MenuTreeID id, gboolean active)
{
    GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM
        (menutree_get_widget_for_id(tree, id));

    if (active != gtk_check_menu_item_get_active(item))
    {
        gtk_check_menu_item_set_active(item, active);
    }
}

void menutree_set_show_menu_bar_active(MenuTree *tree, gboolean active)
{
    menutree_set_toggle(tree, MENUTREE_VIEW_SHOW_MENUBAR, active);
}

void menutree_set_show_tab_bar_active(MenuTree *tree, gboolean active)
{
    menutree_set_toggle(tree, MENUTREE_VIEW_SHOW_TAB_BAR, active);
}

void menutree_set_fullscreen_active(MenuTree *tree, gboolean active)
{
    menutree_set_toggle(tree, MENUTREE_VIEW_FULLSCREEN, active);
}

void menutree_set_borderless_active(MenuTree *tree, gboolean active)
{
    menutree_set_toggle(tree, MENUTREE_VIEW_BORDERLESS, active);
}

extern  void multi_win_new_tab_action(gpointer win);

gulong menutree_named_signal_connect_data(MenuTree * tree, MenuTreeID id,
        const char *signame, GCallback handler, gpointer user_data,
        GConnectFlags flags)
{
    GtkWidget *menu_item;

    g_return_val_if_fail(tree, 0);
    menu_item = menutree_get_widget_for_id(tree, id);
    if (!menu_item)
        return 0;
    return g_signal_connect_data(menu_item, signame, handler, user_data,
        NULL, flags);
}

void menutree_set_show_item(MenuTree * tree, MenuTreeID id, gboolean show)
{
    GtkWidget *item = menutree_get_widget_for_id(tree, id);

    if (show)
        gtk_widget_show(item);
    else
        gtk_widget_hide(item);
}

void menutree_disable_shortcuts(MenuTree *tree, gboolean disable)
{
    int n;

    if (tree->disable_shortcuts == disable)
        return;
    tree->disable_shortcuts = disable;
    for (n = 0; n < MENUTREE_NUM_IDS; ++n)
    {
        if (tree->item_widgets[n] && menutree_labels[n])
        {
            GtkWidget *child;
            child = gtk_bin_get_child(GTK_BIN(tree->item_widgets[n]));
            if (child && GTK_IS_LABEL(child))
            {
                if (disable)
                {
                    char *stripped = strip_underscore(
                            dgettext(PACKAGE, menutree_labels[n]));

                    gtk_label_set_text(GTK_LABEL(child), stripped);
                    g_free(stripped);
                }
                else
                {
                    gtk_label_set_text_with_mnemonic(GTK_LABEL(child),
                            dgettext(PACKAGE, menutree_labels[n]));
                }
            }
        }
    }
}

void menutree_disable_tab_shortcuts(MenuTree *tree, gboolean disable)
{
    if (tree->disable_tab_shortcuts == disable)
        return;
    tree->disable_tab_shortcuts = disable;
    menutree_apply_tab_shortcuts(tree);
}

/* vi:set sw=4 ts=4 et cindent cino= */
