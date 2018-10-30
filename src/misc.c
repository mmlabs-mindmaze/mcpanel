/*
    Copyright (C) 2011 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This program is free software: you can redistribute it and/or
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include "misc.h"

/*************************************************************************
 *                                                                       *
 *                         Key file helper                               *
 *                                                                       *
 *************************************************************************/
LOCAL_FN
void mcpi_key_get_dval(GKeyFile* keyfile, const char* group, const char* key, gdouble* val)
{
	gdouble res;
	GError* error = NULL;
	
	if (!keyfile)
		return;

	res = g_key_file_get_double(keyfile, group, key, &error);
	if (error == NULL)
		*val = res;
}

LOCAL_FN
void mcpi_key_get_bval(GKeyFile* keyfile, const char* group, const char* key, gboolean* val)
{
	gdouble res;
	GError* error = NULL;
	
	if (!keyfile)
		return;

	res = g_key_file_get_boolean(keyfile, group, key, &error);
	if (error == NULL)
		*val = res;
}


 
LOCAL_FN
void mcpi_key_set_combo(GKeyFile* keyfile, const char* group, const char* key, GtkComboBox* combo)
{
	const char* val;
	GError* error = NULL;
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value;
	int found = 0;
	
	if (!keyfile)
		return;

	val = g_key_file_get_string(keyfile, group, key, &error);
	if (!val)
		return;

	model = gtk_combo_box_get_model(combo);
	memset(&value, 0, sizeof(value));
	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	do {
		gtk_tree_model_get_value(model, &iter, 0, &value);
		found = strcmp(g_value_get_string(&value), val) ? 0 : 1;
		g_value_unset(&value);
	} while (!found && gtk_tree_model_iter_next(model, &iter));
	
	if (found)
		gtk_combo_box_set_active_iter(combo, &iter);
}


/*************************************************************************
 *                                                                       *
 *                         widget model setup                            *
 *                                                                       *
 *************************************************************************/
LOCAL_FN
void fill_treeview(GtkTreeView* treeview, const char** labels)
{
	GtkListStore* list;
	unsigned int i = 0;
	GtkTreeIter iter;
	GtkTreeSelection* selec;

	list = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
	gtk_list_store_clear(list);

	while (labels[i] != NULL) {
		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0, labels[i++], -1);
	}

	// Select initially all items
	selec = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_select_all(selec);
}


LOCAL_FN
void fill_combo(GtkComboBox* combo, const char** labels)
{
	GtkListStore* list;
	int i = 0;
	GtkTreeIter iter;

	list = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
	gtk_list_store_clear(list);

	while (labels[i] != NULL) {
		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0, labels[i++], -1);
	}

	gtk_combo_box_set_active (combo, 0);
}
