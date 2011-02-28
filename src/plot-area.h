/*
    Copyright (C) 2008-2009,2011 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

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
#ifndef _PLOT_AREA
#define _PLOT_AREA

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TYPE_PLOT_AREA plot_area_get_type()

#define PLOT_AREA(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PLOT_AREA, PlotArea))

#define PLOT_AREA_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_PLOT_AREA, PlotAreaClass))

#define IS_PLOT_AREA(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PLOT_AREA))

#define IS_PLOT_AREA_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_PLOT_AREA))

#define PLOT_AREA_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_PLOT_AREA, PlotAreaClass))

typedef struct {
	GtkDrawingArea parent;

	double* xticks;
	double* yticks;
	guint num_xticks;
	guint num_yticks;
	GdkColor* colors;
	guint nColors;
	GdkColor grid_color;
} PlotArea;

typedef struct {
	GtkDrawingAreaClass parent_class;
} PlotAreaClass;

GType plot_area_get_type (void);

PlotArea* plot_area_new (void);
void plot_area_set_ticks(PlotArea* self, guint num_xticks, guint num_yticks);
G_END_DECLS

#endif /* _PLOT_AREA */

