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
#ifndef MISC_H
#define MISC_H

#include <glib.h>

LOCAL_FN void mcpi_key_get_dval(GKeyFile* keyfile, const char* group, const char* key, gdouble* val);
LOCAL_FN void mcpi_key_get_ival(GKeyFile* keyfile, const char* group, const char* key, gint* val);
LOCAL_FN void mcpi_key_get_bval(GKeyFile* keyfile, const char* group, const char* key, gboolean* val);
LOCAL_FN void mcpi_key_set_combo(GKeyFile* keyfile, const char* group, const char* key, GtkComboBox* combo);

LOCAL_FN void fill_treeview(GtkTreeView* treeview, const char** labels);
LOCAL_FN void fill_combo(GtkComboBox* combo, const char** labels);
LOCAL_FN void select_channels(GtkTreeView* treeview, int nch, int const * indices);

#endif /*MISC_H*/
