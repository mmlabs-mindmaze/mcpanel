/*
	Copyright (C) 2008-2009 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This file is part of the mcpanel library

    The mcpanel library is free software: you can redistribute it and/or
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _BINARY_SCOPE
#define _BINARY_SCOPE

#include <glib-object.h>
#include "plot-area.h"


G_BEGIN_DECLS

#define TYPE_BINARY_SCOPE binary_scope_get_type()

#define BINARY_SCOPE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_BINARY_SCOPE, BinaryScope))

#define BINARY_SCOPE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_BINARY_SCOPE, BinaryScopeClass))

#define IS_BINARY_SCOPE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_BINARY_SCOPE))

#define IS_BINARY_SCOPE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_BINARY_SCOPE))

#define BINARY_SCOPE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_BINARY_SCOPE, BinaryScopeClass))

typedef struct {
	PlotArea parent;

	guint num_channels;
	guint num_ticks;
	guint* ticks;
	gint* offsets;
	guint current_pointer;
	guint num_points;
	gint* xcoords;
	const guint32* data;
} BinaryScope;

typedef struct {
	PlotAreaClass parent_class;
} BinaryScopeClass;

GType binary_scope_get_type (void);

BinaryScope* binary_scope_new (void);
void binary_scope_update_data(BinaryScope* self, guint pointer);
void binary_scope_set_data(BinaryScope* self, guint32* data, guint num_points, guint num_ch);
void binary_scope_set_ticks(BinaryScope* self, guint num_ticks, guint* ticks);

G_END_DECLS

#endif /* _BINARY_SCOPE */

