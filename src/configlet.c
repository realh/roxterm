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


#include "capplet.h"
#include "colourgui.h"
#include "configlet.h"
#include "dlg.h"
#include "dynopts.h"
#include "encodings.h"
#include "getname.h"
#include "optsdbus.h"
#include "optsfile.h"
#include "profilegui.h"

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include <unistd.h>
#include <glib/gstdio.h>

extern gboolean g_file_set_contents(const gchar * filename,
                    const gchar * contents, gssize length, GError ** error);

static int profile_lock = 0;
static int colours_lock = 0;
static int shortcuts_lock = 0;
static int encodings_lock = 0;

typedef struct ConfigletData ConfigletData;

typedef struct {
    const char *family;
    GtkTreeView *tvwidget;
    GtkListStore *list;
    gpointer foreach_data;
    GKeyFile *encodings;
    ConfigletData *cg;
} ConfigletList;

struct ConfigletData {
    gboolean ignore_destroy;
    GladeXML *glade;
    GtkWidget *widget;
    Options *options;
    ConfigletList profile;
    ConfigletList colours;
    ConfigletList shortcuts;
    ConfigletList encodings;
};

enum {
    cfColumn_Radio,
    cfColumn_Name,

    cfColumn_NColumns
};

static gboolean ignore_changes = FALSE;

static GladeXML *configlet_glade;

static ConfigletData *configlet_data;

static void set_sensitive_for_list(ConfigletList *cl);

/* Converts "Profiles" into "profile" etc */
static char *convert_family_name(const char *family)
{
    char *optkey = g_strdup(family);

    /* Strip final 's' from "Profiles" and make first letter lower-case */
    if (optkey[0] == 'P')
        optkey[strlen(optkey) - 1] = 0;
    optkey[0] = tolower(optkey[0]);
    return optkey;
}

static const char *full_name_from_family(const char *family)
{
    if (!strcmp(family, "Profiles"))
    {
        return _("Profile");
    }
    else if (!strcmp(family, "Colours"))
    {
        return _("Colour Scheme");
    }
    else if (!strcmp(family, "Shortcuts"))
    {
        return _("Keyboard Shortcuts Scheme");
    }
    else if (!strcmp(family, "encodings"))
    {
        return _("Character Encodings");
    }
    else
    {
        g_critical(_("Full name for family '%s' not known"), family);
        return family;
    }
}

static void configlet_set_sensitive_button(const char *wbasename,
        const char *butname, gboolean sensitive)
{
    char *button_name = g_strdup_printf("%s_%s", wbasename, butname);
    GtkWidget *widget = glade_xml_get_widget(configlet_glade, button_name);

    g_free(button_name);
    g_return_if_fail(widget != NULL);
    gtk_widget_set_sensitive(widget, sensitive);
}

static void configlet_set_sensitive(const char *wbasename, gboolean sensitive)
{
    int lock;

    if (!configlet_glade)
        return;
    if (!strcmp(wbasename, "profile"))
    {
        lock = profile_lock;
    }
    else if (!strcmp(wbasename, "colours"))
    {
        lock = colours_lock;
    }
    else if (!strcmp(wbasename, "shortcuts"))
    {
        lock = shortcuts_lock;
    }
    else if (!strcmp(wbasename, "encodings"))
    {
        lock = encodings_lock;
    }
    else
    {
        g_critical(_("Bad options family basename '%s'"), wbasename);
        return;
    }
    configlet_set_sensitive_button(wbasename, "delete", !lock && sensitive);
    configlet_set_sensitive_button(wbasename, "rename", !lock && sensitive);
}

static gboolean is_in_user_dir(const char *family, const char *profile_name)
{
    if (!strcmp(family, "encodings"))
    {
        return strcmp(profile_name, "Default") != 0;
    }
    else
    {
        char *pathname = options_file_build_filename(family, profile_name,
                NULL);
        char *savename = options_file_filename_for_saving(family,
                profile_name, NULL);
        gboolean result = (pathname != NULL) && !strcmp(savename, pathname);

        g_free(savename);
        g_free(pathname);

        return result;
    }
}

static void shade_actions_for_name(ConfigletList *cl, const char *name)
{
    char *wbasename = convert_family_name(cl->family);

    configlet_set_sensitive(wbasename, is_in_user_dir(cl->family, name));
    g_free(wbasename);
}

static void configlet_delete(ConfigletData *cg)
{
    if (cg->widget)
    {
        cg->ignore_destroy = TRUE;
        gtk_widget_destroy(cg->widget);
        cg->widget = NULL;
    }
    if (cg->glade)
    {
        UNREF_LOG(g_object_unref(cg->glade));
        cg->glade = NULL;
        configlet_glade = NULL;
    }
    g_free(cg);
    configlet_data = NULL;
    capplet_dec_windows();
}

static const char *family_name_to_opt_key(const char *family)
{
    if (!strcmp(family, "Profiles"))
        return "profile";
    else if (!strcmp(family, "Colours"))
        return "colour_scheme";
    else if (!strcmp(family, "Shortcuts"))
        return "shortcut_scheme";
    else if (!strcmp(family, "encodings"))
        return "encoding";
    return NULL;
}

static char *configlet_get_configured_name(ConfigletList *cl)
{
    const char *optkey = family_name_to_opt_key(cl->family);

    g_return_val_if_fail(optkey, NULL);
    return options_lookup_string_with_default(cl->cg->options, optkey,
            strcmp(cl->family, "Colours") ? "Default" : "GTK");
}

static void configlet_list_build(ConfigletList *cl)
{
    char **item_list;
    char **pitem;
    char *selected_name = configlet_get_configured_name(cl);

    if (cl->encodings)
    {
        item_list = encodings_list(cl->encodings);
    }
    else
    {
        item_list = dynamic_options_list(dynamic_options_get(cl->family));
    }

    if (cl->list)
    {
        gtk_list_store_clear(cl->list);
    }
    else
    {
        cl->list = gtk_list_store_new(cfColumn_NColumns,
                G_TYPE_BOOLEAN, G_TYPE_STRING);
    }

    for (pitem = item_list; *pitem; ++pitem)
    {
        GtkTreeIter iter;

        gtk_list_store_append(cl->list, &iter);
        gtk_list_store_set(cl->list, &iter,
                cfColumn_Radio, !strcmp(selected_name, *pitem),
                cfColumn_Name, *pitem,
                -1);
    }
    g_strfreev(item_list);
    g_free(selected_name);
}

/* Selects matching name and clears cl->foreach_data if it finds a match */
static gboolean foreach_find_name(GtkTreeModel *model, GtkTreePath *path,
        GtkTreeIter *iter, gpointer data)
{
    char *name = NULL;
    ConfigletList *cl = data;

    gtk_tree_model_get(model, iter, cfColumn_Name, &name, -1);
    if (name && cl->foreach_data && !strcmp(name, cl->foreach_data))
    {
        gtk_tree_selection_select_iter(
                gtk_tree_view_get_selection(cl->tvwidget), iter);
        cl->foreach_data = NULL;
        return TRUE;
    }
    g_free(name);
    return FALSE;
}

/* If name isn't in list, first item is selected */
static void configlet_select_name(ConfigletList *cl, const char *name)
{
    GtkTreeIter iter;

    ignore_changes = TRUE;
    /* foreach_data is set to NULL if our function finds name */
    cl->foreach_data = (gpointer) name;
    if (name)
        gtk_tree_model_foreach(GTK_TREE_MODEL(cl->list), foreach_find_name, cl);
    if (cl->foreach_data || !name)
    {
        /* No match so select first item */
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(cl->list), &iter))
        {
            gtk_tree_selection_select_iter(
                    gtk_tree_view_get_selection(cl->tvwidget), &iter);
        }
    }
    ignore_changes = FALSE;
}

/* Sets the radio of whichever item matches string pointed to by data */
gboolean update_radios(GtkTreeModel *model,
        GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    char *name = NULL;

    gtk_tree_model_get(model, iter, cfColumn_Name, &name, -1);
    gtk_list_store_set(GTK_LIST_STORE(model), iter, cfColumn_Radio,
            name && !strcmp(name, data), -1);
    return FALSE;
}

/* When we receive this event, the tree model's radio is in its prior state so
 * if it isn't already selected we select it, otherwise do nothing. */
static void configlet_cell_toggled(GtkCellRendererToggle *cell,
        gchar *path_string, ConfigletList *cl)
{
    GtkTreeIter iter;
    char *name;
    gboolean active;

    g_return_if_fail(
            gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(cl->list),
                &iter, path_string));
    gtk_tree_model_get(GTK_TREE_MODEL(cl->list), &iter,
            cfColumn_Radio, &active,
            cfColumn_Name, &name, -1);
    if (!active)
    {
        gtk_tree_model_foreach(GTK_TREE_MODEL(cl->list), update_radios, name);
        options_set_string(cl->cg->options,
                family_name_to_opt_key(cl->family), name);
        capplet_save_file(cl->cg->options);
    }
    g_free(name);
}

static void configlet_list_init(ConfigletList *cl, GtkWidget *widget,
        const char *family)
{
    GtkTreeViewColumn *rcolumn, *tcolumn;
    GtkCellRenderer *radio;
    char *cname;

    cl->tvwidget = GTK_TREE_VIEW(widget);
    cl->family = family;
    cl->list = gtk_list_store_new(cfColumn_NColumns,
            G_TYPE_BOOLEAN, G_TYPE_STRING);
    gtk_tree_view_set_model(cl->tvwidget, GTK_TREE_MODEL(cl->list));

    radio = gtk_cell_renderer_toggle_new();
    gtk_cell_renderer_toggle_set_radio(GTK_CELL_RENDERER_TOGGLE(radio),
            TRUE);
    g_object_set(radio, "activatable", TRUE, NULL);
    g_signal_connect(radio, "toggled",
            G_CALLBACK(configlet_cell_toggled), cl);
    rcolumn = gtk_tree_view_column_new_with_attributes(
                NULL,
                radio,
                "active",
                cfColumn_Radio,
                NULL);
    gtk_tree_view_append_column(cl->tvwidget, rcolumn);

    tcolumn = gtk_tree_view_column_new_with_attributes(
                NULL,
                gtk_cell_renderer_text_new(),
                "text",
                cfColumn_Name,
                NULL);
    gtk_tree_view_append_column(cl->tvwidget, tcolumn);
    gtk_tree_view_column_set_visible(rcolumn, TRUE);

    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(cl->tvwidget),
            GTK_SELECTION_BROWSE);
    configlet_list_build(cl);
    configlet_select_name(cl, cname = configlet_get_configured_name(cl));
    g_free(cname);
    gtk_widget_realize(widget);
    gtk_tree_view_columns_autosize(cl->tvwidget);
}

/********************************************************************/
/* Generic handlers */

static void on_Configlet_destroy(GtkWidget * widget, ConfigletData * cg)
{
    if (cg->ignore_destroy)
    {
        return;
    }
    else
    {
        cg->ignore_destroy = TRUE;
        configlet_delete(cg);
    }
}

static void on_Configlet_response(GtkWidget * widget, int response,
        ConfigletData * cg)
{
    configlet_delete(cg);
}

static void on_Configlet_close(GtkWidget * widget, ConfigletData * cg)
{
    configlet_delete(cg);
}


/********************************************************************/
/* Family-specific handlers and helpers */

static gboolean get_selected_iter(ConfigletList *cl,
        GtkTreeModel **pmodel, GtkTreeIter *piter)
{
    return gtk_tree_selection_get_selected(
                gtk_tree_view_get_selection(cl->tvwidget), pmodel, piter);
}

static int get_selected_index(ConfigletList *cl)
{
    /* FIXME: Is there a better way of doing this? */
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(cl->tvwidget);
    GtkTreeSelection *selection;
    int n = 0;

    selection = gtk_tree_view_get_selection(cl->tvwidget);
    if (!gtk_tree_model_get_iter_first(model, &iter))
        return -1;
    while (!gtk_tree_selection_iter_is_selected(selection, &iter))
    {
        if (!gtk_tree_model_iter_next(model, &iter))
            return -1;
        ++n;
    }
    return n;
}

static char *get_selected_name(ConfigletList *cl)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    char *name = NULL;

    if (get_selected_iter(cl, &model, &iter))
    {
        gtk_tree_model_get(model, &iter, cfColumn_Name, &name, -1);
    }
    return name;
}

static gboolean get_selected_state(ConfigletList *cl)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    gboolean state = FALSE;

    if (get_selected_iter(cl, &model, &iter))
    {
        gtk_tree_model_get(model, &iter, cfColumn_Radio, &state, -1);
    }
    return state;
}

static void add_name_to_list(ConfigletList *cl, const char *new_name)
{
    GtkTreeIter iter;
    char *cname = configlet_get_configured_name(cl);

    gtk_list_store_append(cl->list, &iter);
    gtk_list_store_set(cl->list, &iter,
            cfColumn_Radio,
            cname && !strcmp(new_name, configlet_get_configured_name(cl)),
            cfColumn_Name, new_name,
            -1);
    g_free(cname);
    set_sensitive_for_list(cl);
}

/*This checks whether the name is still available in dynopts family - which
 * can happen if the removed name was in ~/.config overriding a system
 * profile/scheme with the same name - and leaves it if so, returning FALSE.
 */
static gboolean remove_name_from_list(ConfigletList *cl, const char *old_name)
{
    gboolean remove = TRUE;

    if (!cl->encodings)
    {
        char **name_list =
            dynamic_options_list(dynamic_options_get(cl->family));
        char **pname;

        for (pname = name_list; *pname; ++pname)
        {
            if (!strcmp(old_name, *pname))
            {
                remove = FALSE;
                dlg_message(GTK_WINDOW(cl->cg->widget),
                        _("'%s' will now refer to a %s provided by the system"),
                        old_name, full_name_from_family(cl->family));
            }
        }
        g_strfreev(name_list);
    }
    if (remove)
    {
        GtkTreeIter iter;

        gtk_tree_selection_get_selected(
                    gtk_tree_view_get_selection(cl->tvwidget), NULL, &iter);
        gtk_list_store_remove(cl->list, &iter);
    }
    set_sensitive_for_list(cl);
    return remove;
}

/* This is Add for Encodings */
static void configlet_copy(ConfigletList *cl,
        const char *old_leaf, const char *new_leaf)
{
    GError *error = NULL;
    gboolean success = FALSE;

    if (cl->encodings)
    {
        encodings_add(cl->encodings, new_leaf);
        encodings_save(cl->encodings);
        success = TRUE;
    }
    else
    {
        char *old_path = options_file_build_filename(cl->family, old_leaf,
                NULL);
        char *new_path = options_file_filename_for_saving(cl->family,
            new_leaf, NULL);
        char *dir = options_file_filename_for_saving(cl->family, NULL);
        char *buffer = NULL;
        gsize size;

        if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
        {
            if (g_mkdir_with_parents(dir, 0755) == -1)
            {
                dlg_warning(GTK_WINDOW(cl->cg->widget),
                        _("Unable to create directory '%s'"), dir);
                return;
            }
        }
        if (g_file_test(old_path, G_FILE_TEST_IS_REGULAR))
        {
            if (g_file_get_contents(old_path, &buffer, &size, &error))
                success = g_file_set_contents(new_path, buffer, size, &error);
        }
        else
        {
            success = g_file_set_contents(new_path, "", -1, &error);
        }
        g_free(buffer);
        g_free(new_path);
        g_free(old_path);
    }
    if (success)
    {
        add_name_to_list(cl, new_leaf);
        optsdbus_send_stuff_changed_signal(OPTSDBUS_ADDED, cl->family,
                new_leaf, NULL);
    }
    else
    {
        dlg_warning(GTK_WINDOW(cl->cg->widget),
                _("Unable to copy profile/scheme: %s"),
                error && error->message ? error->message : _("unknown reason"));
    }
    if (error)
        g_error_free(error);
}

/* This is Edit for Encodings */
static void configlet_rename(ConfigletList *cl,
        const char *old_leaf, const char *new_leaf)
{
    gboolean success = FALSE;

    if (cl->encodings)
    {
        int n = get_selected_index(cl);

        encodings_change(cl->encodings, n, new_leaf);
        encodings_save(cl->encodings);
        success = TRUE;
    }
    else
    {
        char *old_path = options_file_build_filename(cl->family, old_leaf,
                NULL);
        char *new_path = options_file_filename_for_saving(cl->family,
            new_leaf, NULL);

        success = (g_rename(old_path, new_path) == 0);
        g_free(new_path);
        g_free(old_path);
    }
    if (success)
    {
        GtkTreeModel *model;
        GtkTreeIter iter;
        gboolean state;

        get_selected_iter(cl, &model, &iter);
        gtk_tree_model_get(model, &iter, cfColumn_Radio, &state, -1);
        gtk_list_store_set(cl->list, &iter,
                cfColumn_Radio, state, cfColumn_Name, new_leaf, -1);
        /*
        g_debug("Sending d-bus message: %s, %s, %s, %s",
                OPTSDBUS_RENAMED, cl->family, old_leaf, new_leaf);
        */
        optsdbus_send_stuff_changed_signal(OPTSDBUS_RENAMED, cl->family,
                old_leaf, new_leaf);
    }
    else
    {
        dlg_warning(GTK_WINDOW(cl->cg->widget),
                _("Unable to rename profile/scheme"));
    }
}

static void edit_thing_by_name(ConfigletList *cl, const char *name)
{
    GdkScreen *scrn = gtk_widget_get_screen(cl->cg->widget);
    
    if (!strcmp(cl->family, "Profiles"))
    {
        profilegui_open(name, scrn);
    }
    else if (!strcmp(cl->family, "Colours"))
    {
        colourgui_open(name, scrn);
    }
    else if (!strcmp(cl->family, "Shortcuts"))
    {
        dlg_message(GTK_WINDOW(cl->cg->widget),
                _("Shortcuts can currently only be edited by hovering "
                    "over the menu entries and pressing keys"));
    }
    else if (cl->encodings)
    {
        dlg_message(GTK_WINDOW(cl->cg->widget),
                _("Encoding names have no properties"));
    }
    else
    {
        g_critical(_("Option family name '%s' not recognised for editing"),
                cl->family);
    }
}

static void edit_thing_by_iter(ConfigletList *cl, GtkTreeIter *iter)
{
    char *name;

    gtk_tree_model_get(GTK_TREE_MODEL(cl->list), iter,
            cfColumn_Name, &name, -1);
    g_return_if_fail(name != NULL);
    edit_thing_by_name(cl, name);
    g_free(name);
}

static gboolean edit_selected_thing(ConfigletList *cl)
{
    char *name = get_selected_name(cl);

    if (!name)
    {
        g_warning(_("No selection to edit"));
        return FALSE;
    }

    edit_thing_by_name(cl, name);
    g_free(name);
    return TRUE;
}

static void on_edit_clicked(GtkButton *button, ConfigletList *cl)
{
    edit_selected_thing(cl);
}

static void on_row_activated(GtkTreeView *tvwidget, GtkTreePath *path,
        GtkTreeViewColumn *column, ConfigletList *cl)
{
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(cl->list), &iter, path))
        edit_thing_by_iter(cl, &iter);
    else
        g_warning(_("Can't get iterator for activated item"));
}

static void on_tree_selection_changed(GtkTreeSelection *selection,
        ConfigletList *cl)
{
    char *name = get_selected_name(cl);

    if (name)
    {
        shade_actions_for_name(cl, name);
        g_free(name);
    }
}

/* This is Add for Encodings */
static void on_copy_clicked(GtkButton *button, ConfigletList *cl)
{
    char *old_name = get_selected_name(cl);
    char *title = NULL;
    const char *button_label = NULL;
    char **existing = NULL;

    if (cl->encodings)
    {
        title = g_strdup_printf(_("Add Encoding"));
        button_label = GTK_STOCK_ADD;
        existing = encodings_list(cl->encodings);
    }
    else
    {
        DynamicOptions *dynopts = dynamic_options_get(cl->family);

        title = g_strdup_printf(_("Copy %s"),
            full_name_from_family(cl->family));
        button_label = GTK_STOCK_COPY;
        existing = dynamic_options_list(dynopts);
    }
    if (old_name || cl->encodings)
    {
        char *new_name = getname_run_dialog(GTK_WINDOW(cl->cg->widget),
                old_name, existing, title, button_label, NULL,
                cl->encodings == NULL);

        if (new_name)
        {
            configlet_copy(cl, old_name, new_name);
            g_free(new_name);
        }
        g_free(old_name);
    }
    else
    {
        g_warning(_("No selection to copy"));
    }
    g_free(title);
}

static void on_delete_clicked(GtkButton *button, ConfigletList *cl)
{
    char *name;

    if (get_selected_state(cl))
    {
        dlg_warning(GTK_WINDOW(cl->cg->widget),
                _("You may not delete the selected default %s"),
                full_name_from_family(cl->family));
        return;
    }
    name = get_selected_name(cl);
    if (!name)
    {
        g_warning(_("No selection to delete"));
        return;
    }
    if (!is_in_user_dir(cl->family, name))
    {
        dlg_warning(GTK_WINDOW(cl->cg->widget),
                _("'%s' is a system %s and can not be deleted"),
                name, full_name_from_family(cl->family));
    }
    else
    {
        gboolean remove = TRUE;

        if (cl->encodings)
        {
            int n = get_selected_index(cl);

            g_return_if_fail(n != -1);
            encodings_remove(cl->encodings, n);
            encodings_save(cl->encodings);
        }
        else
        {
            char *filename = options_file_build_filename(cl->family,
                    name, NULL);

            if (g_unlink(filename))
            {
                dlg_warning(GTK_WINDOW(cl->cg->widget),
                        _("Unable to delete '%s'"), filename);
                remove = FALSE;
            }
            g_free(filename);
        }
        if (remove)
        {
            if (remove_name_from_list(cl, name))
            {
                optsdbus_send_stuff_changed_signal(OPTSDBUS_DELETED,
                        cl->family, name, NULL);
            }
        }
    }
    g_free(name);
}

/* This is Edit for Encodings */
static void on_rename_clicked(GtkButton *button, ConfigletList *cl)
{
    char *title = NULL;
    char **existing = NULL;
    char *old_name = get_selected_name(cl);

    if (cl->encodings)
    {
        title = g_strdup_printf(_("Edit Encoding"));
        existing = encodings_list(cl->encodings);
    }
    else
    {
        DynamicOptions *dynopts = dynamic_options_get(cl->family);

        title = g_strdup_printf(_("Rename %s"),
            full_name_from_family(cl->family));
        existing = dynamic_options_list(dynopts);
    }
    if (old_name || cl->encodings)
    {
        char *new_name = getname_run_dialog(GTK_WINDOW(cl->cg->widget),
                old_name, existing, title, GTK_STOCK_APPLY, NULL,
                cl->encodings == NULL);

        if (new_name)
        {
            configlet_rename(cl, old_name, new_name);
            g_free(new_name);
        }
        g_free(old_name);
    }
    else
    {
        g_warning(_("No selection to copy"));
    }
    g_free(title);
}

/********************************************************************/

#define CONFIGLET_CONNECT(glade, data, h) \
    glade_xml_signal_connect_data(glade, #h, (GCallback) h, (data))

#define CG_CONNECT(cg, h) \
    CONFIGLET_CONNECT((cg)->glade, cg, h)


static void connect_generic_handlers(ConfigletData *cg)
{
    CG_CONNECT(cg, on_Configlet_destroy);
    CG_CONNECT(cg, on_Configlet_close);
    CG_CONNECT(cg, on_Configlet_response);
}

static void connect_a_list_handler(ConfigletList *cl, GladeXML *glade,
        const char *wbasename, const char *signame_stub, GCallback handler)
{
    char *signame;

    signame = g_strdup_printf("on_%s_%s_clicked", wbasename, signame_stub);
    glade_xml_signal_connect_data(glade, signame, handler, cl);
    g_free(signame);
}

static void connect_list_handlers(ConfigletList *cl, GladeXML *glade)
{
    char *wbasename = convert_family_name(cl->family);

    if (!cl->encodings && strcmp(cl->family, "Shortcuts"))
    {
        connect_a_list_handler(cl, glade, wbasename, "edit",
                G_CALLBACK(on_edit_clicked));
        g_signal_connect(cl->tvwidget, "row-activated",
                G_CALLBACK(on_row_activated), cl);
    }
    connect_a_list_handler(cl, glade, wbasename, "delete",
            G_CALLBACK(on_delete_clicked));
    connect_a_list_handler(cl, glade, wbasename, "copy",
            G_CALLBACK(on_copy_clicked));
    connect_a_list_handler(cl, glade, wbasename, "rename",
            G_CALLBACK(on_rename_clicked));
    g_free(wbasename);

    g_signal_connect(gtk_tree_view_get_selection(cl->tvwidget), "changed",
                  G_CALLBACK(on_tree_selection_changed), cl);
}

static void configlet_setup_family(ConfigletData *cg, ConfigletList *cl,
        const char *family)
{
    char *wbasename = convert_family_name(family);
    char *widget_name = g_strdup_printf("%s_treeview", wbasename);
    GtkWidget *widget = glade_xml_get_widget(cg->glade, widget_name);
    char *cname;

    g_free(widget_name);
    g_return_if_fail(widget != NULL);
    cl->cg = cg;
    configlet_list_init(cl, widget, family);
    shade_actions_for_name(cl, cname = configlet_get_configured_name(cl));
    g_free(cname);
    connect_list_handlers(cl, cg->glade);
    g_free(wbasename);
}

gboolean configlet_open(GdkScreen *scrn)
{
    
    if (configlet_data)
    {
        if (scrn)
            gtk_window_set_screen(GTK_WINDOW(configlet_data->widget), scrn);
        gtk_window_present(GTK_WINDOW(configlet_data->widget));
    }
    else
    {
        ConfigletData *cg = configlet_data = g_new0(ConfigletData, 1);

        cg->glade = glade_xml_new(capplet_get_glade_filename(),
                "Configlet", NULL);
        if (cg->glade)
            cg->widget = glade_xml_get_widget(cg->glade, "Configlet");
        if (!cg->widget)
        {
            configlet_delete(cg);
            return FALSE;
        }
        if (scrn)
            gtk_window_set_screen(GTK_WINDOW(cg->widget), scrn);
        configlet_glade = cg->glade;

        cg->options = options_open("Global", "roxterm options");

        cg->encodings.encodings = encodings_load();

        connect_generic_handlers(cg);
        CONFIGLET_CONNECT(cg->glade, cg->options, on_boolean_toggled);
        CONFIGLET_CONNECT(cg->glade, cg->options, on_radio_toggled);

        configlet_setup_family(cg, &cg->profile, "Profiles");
        configlet_setup_family(cg, &cg->colours, "Colours");
        configlet_setup_family(cg, &cg->shortcuts, "Shortcuts");
        configlet_setup_family(cg, &cg->encodings, "encodings");
        
        capplet_set_radio(cg->glade, cg->options, "warn_close", 2);
        capplet_set_boolean_toggle(cg->glade, cg->options,
                "edit_shortcuts", FALSE);

        capplet_inc_windows();
        gtk_widget_show(cg->widget);
    }
    return TRUE;
}

static void set_sensitive_for_list(ConfigletList *cl)
{
    on_tree_selection_changed(gtk_tree_view_get_selection(cl->tvwidget), cl);
}

void configlet_lock_profiles(void)
{
    ++profile_lock;
    if (configlet_data)
        set_sensitive_for_list(&configlet_data->profile);
}

void configlet_unlock_profiles(void)
{
    if (--profile_lock < 0)
    {
        g_critical(_("Trying to decrease profile_lock below 0"));
        profile_lock = 0;
    }
    if (configlet_data)
        set_sensitive_for_list(&configlet_data->profile);
}

void configlet_lock_colour_schemes(void)
{
    ++colours_lock;
    if (configlet_data)
        set_sensitive_for_list(&configlet_data->colours);
}

void configlet_unlock_colour_schemes(void)
{
    if (--colours_lock < 0)
    {
        g_critical(_("Trying to decrease colours_lock below 0"));
        colours_lock = 0;
    }
    if (configlet_data)
        set_sensitive_for_list(&configlet_data->colours);
}

void configlet_lock_shortcuts(void)
{
    ++shortcuts_lock;
    if (configlet_data)
        set_sensitive_for_list(&configlet_data->shortcuts);
}

void configlet_unlock_shortcuts(void)
{
    if (--shortcuts_lock < 0)
    {
        g_critical(_("Trying to decrease shortcuts_lock below 0"));
        shortcuts_lock = 0;
    }
    if (configlet_data)
        set_sensitive_for_list(&configlet_data->shortcuts);
}

void configlet_lock_encodings(void)
{
    ++encodings_lock;
    if (configlet_data)
        set_sensitive_for_list(&configlet_data->encodings);
}

void configlet_unlock_encodings(void)
{
    if (--encodings_lock < 0)
    {
        g_critical(_("Trying to decrease encodings_lock below 0"));
        encodings_lock = 0;
    }
    if (configlet_data)
        set_sensitive_for_list(&configlet_data->encodings);
}

/* vi:set sw=4 ts=4 noet cindent cino= */
