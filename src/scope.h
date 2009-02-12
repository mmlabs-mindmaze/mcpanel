/*
	Copyright (C) 2008-2009 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This file is part of the eegpanel library

    The eegpanel library is free software: you can redistribute it and/or
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _SCOPE
#define _SCOPE

#include <glib-object.h>
#include "plot-area.h"
#include "plottk-types.h"


G_BEGIN_DECLS

#define TYPE_SCOPE scope_get_type()

#define SCOPE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SCOPE, Scope))

#define SCOPE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SCOPE, ScopeClass))

#define IS_SCOPE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SCOPE))

#define IS_SCOPE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_SCOPE))

#define SCOPE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_SCOPE, ScopeClass))

typedef struct {
	PlotArea parent;

	guint num_channels;
	guint num_ticks;
	gint* ticks;
	data_t scale;
	data_t phys_scale;
	guint current_pointer;
	guint num_points;
	GdkPoint* points;
	const data_t* data;
} Scope;

typedef struct {
	PlotAreaClass parent_class;
} ScopeClass;

GType scope_get_type (void);

Scope* scope_new (void);
void scope_update_data(Scope* self, guint pointer);
void scope_set_data(Scope* self, data_t* data, guint num_points, guint num_ch);
void scope_set_ticks(Scope* self, guint num_ticks, guint* ticks);

G_END_DECLS

#endif /* _SCOPE */

