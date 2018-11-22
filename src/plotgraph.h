/*
    Copyright (C) 2018  MindMaze
    Nicolas Bourdaud <nicolas.bourdaud@gmail.com>

    This program is free software: you can redistribute it and/or modify
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef PLOTGRAPH_H
#define PLOTGRAPH_H

#include <glib-object.h>
#include "plot-area.h"

G_BEGIN_DECLS

#define TYPE_PLOTGRAPH plotgraph_get_type()

#define PLOTGRAPH(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PLOTGRAPH, Plotgraph))

#define PLOTGRAPH_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_PLOTGRAPH, PlotgraphClass))

#define IS_PLOTGRAPH(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PLOTGRAPH))

#define IS_PLOTGRAPH_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_PLOTGRAPH))

#define PLOTGRAPH_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_PLOTGRAPH, PlotgraphClass))

typedef struct plotgraph {
	PlotArea parent;

	int num_xticks;
	float* xtick_values;
	int num_yticks;
	float* ytick_values;
	float yscale, yoffset;
	float xscale, xoffset;
	float ymin, ymax, xmin, xmax;
	float points_xmin;
	float points_xmax;
	int disp_start_idx;
	int disp_num_points;
	int num_points;
	GdkPoint* points;
} Plotgraph;

typedef struct {
	PlotAreaClass parent_class;
} PlotgraphClass;

GType plotgraph_get_type (void);

struct plotgraph* plotgraph_new (void);
void plotgraph_update_data(Plotgraph* self, const float* data);
void plotgraph_set_datalen(Plotgraph* self, int len, float xmin, float xmax);
void plotgraph_set_xticks(Plotgraph* self, int num_ticks, const float* xtick_values);
void plotgraph_set_yticks(Plotgraph* self, int num_ticks, const float* ytick_values);

G_END_DECLS

#endif /* PLOTGRAPH_H */

