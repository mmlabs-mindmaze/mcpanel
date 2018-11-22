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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <float.h>
#include <gtk/gtk.h>
#include <string.h>
#include "plotgraph.h"

enum {
	DUMMY_PROP,
	XMIN_VALUE,
	XMAX_VALUE,
	YMIN_VALUE,
	YMAX_VALUE,
	PLOTGRAPH_NUM_PROP
};


LOCAL_FN GType plotgraph_get_type(void);
G_DEFINE_TYPE (Plotgraph, plotgraph, TYPE_PLOT_AREA)
#define GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), TYPE_PLOTGRAPH, PlotgraphPrivate))

#define PLOT_MARGIN 1

/**
 * plotgraph_calculate_drawparameters() - compute params needed for rendering
 * @self:       pointer to initialize plot graph
 *
 * This function compute the parameters that are needed for drawing plot but
 * that are constant across draw calls. This function need only to be recalled
 * when the number of points change or when the widget allocation size change.
 */
static
void plotgraph_calculate_drawparameters(Plotgraph* self)
{
	int height, width, i, num_points, ifirst, ilast;
	float point_x_inc, xv;
	int* xticks = PLOT_AREA(self)->xticks;
	int* yticks = PLOT_AREA(self)->yticks;
	GdkPoint* points = self->points;

	num_points = self->num_points;
	width = GTK_WIDGET(self)->allocation.width;
	height = GTK_WIDGET(self)->allocation.height;

	// Do not update if limits are invalid
	if ( (self->xmax - self->xmin) < FLT_MIN
	  || (self->ymax - self->ymin) < FLT_MIN
	  || (self->points_xmax - self->points_xmin) < FLT_MIN ) {
		self->xscale = self->yscale = 0.0f;
		self->xoffset = self->yoffset = 0.0f;
		self->disp_start_idx = 0;
		self->disp_num_points = 0;
		return;
	}

	// Compute parameters of the data -> pixel mapping (0 is at top of
	// region and increasing value goes to bottom). Accomodate a
	// margin to avoid cropping data and ticks set at the limit
	self->yscale = -(height-2*PLOT_MARGIN) / (self->ymax - self->ymin);
	self->yoffset = (height-PLOT_MARGIN) - self->yscale * self->ymin;

	// Compute parameters of the abscisses -> pixel mapping (0 is at left of
	// region and increasing value goes to right). Accomodate a
	// margin to avoid cropping data and ticks set at the limit
	self->xscale = (width-2*PLOT_MARGIN) / (self->xmax - self->xmin);
	self->xoffset = PLOT_MARGIN - self->xscale * self->xmin;

	// Compute point increment on x-axis and the indices of the
	// visible points
	point_x_inc = (self->points_xmax - self->points_xmin)/ (num_points-1);
	ifirst = (self->xmin > self->points_xmin) ? (self->xmin - self->points_xmin) / point_x_inc : 0;
	ilast = (int)((self->xmax - self->points_xmin) / point_x_inc) + 1;
	if (ilast > num_points-1)
		ilast = num_points-1;

	// Setup x coordinates
	for (i = ifirst; i <= ilast; i++) {
		xv = i * point_x_inc + self->points_xmin;
		points[i].x = self->xscale * xv + self->xoffset;
	}
	self->disp_start_idx = ifirst;
	self->disp_num_points = ilast - ifirst + 1;

	// Set the xticks position
	for (i = 0; i < self->num_xticks; i++)
		xticks[i] = self->xscale * self->xtick_values[i] + self->xoffset;

	// Set the yticks position
	for (i = 0; i < self->num_yticks; i++)
		yticks[i] = self->yscale * self->ytick_values[i] + self->yoffset;
}


static
void plotgraph_set_property(GObject *object, guint property_id,
                            const GValue *value, GParamSpec *pspec)
{
	Plotgraph* self = PLOTGRAPH(object);

	switch (property_id) {
	case XMIN_VALUE:
		self->xmin = g_value_get_float(value);
		break;

	case XMAX_VALUE:
		self->xmax = g_value_get_float(value);
		break;

	case YMIN_VALUE:
		self->ymin = g_value_get_float(value);
		break;

	case YMAX_VALUE:
		self->ymax = g_value_get_float(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}

	plotgraph_calculate_drawparameters(self);
}


static
gboolean plotgraph_configure_event_callback(Plotgraph *self,
                                            GdkEventConfigure *event,
                                            gpointer data)
{
	(void)data;
	(void)event;

	plotgraph_calculate_drawparameters(self);
	return TRUE;
}


static
gboolean plotgraph_expose_event_callback(Plotgraph *self,
                                         GdkEventExpose *event,
                                         gpointer data)
{
	(void)data;

	int i, xmin, xmax, ymin, ymax;
	const GdkRectangle* rect = &event->area;
	GdkGC* plotgc = PLOT_AREA(self)->plotgc;
	GdkWindow* wnd = GTK_WIDGET(self)->window;
	const int* yticks = PLOT_AREA(self)->yticks;

	xmin = rect->x;
	xmax = rect->x + rect->width;
	ymin = rect->y;
	ymax = rect->y + rect->height;

	// draw grid
	gdk_gc_set_foreground(plotgc, &PLOT_AREA(self)->grid_color);
	for (i = 0; i < self->num_yticks; i++) {
		if (!(ymin <= yticks[i] && yticks[i] <= ymax))
			continue;

		gdk_draw_line(wnd, plotgc, xmin, yticks[i], xmax, yticks[i]);
	}

	// Draw plot
	gdk_gc_set_foreground(plotgc, PLOT_AREA(self)->colors);
	gdk_draw_lines(wnd, plotgc,
	               self->points + self->disp_start_idx,
	               self->disp_num_points);

	return TRUE;
}



static
void plotgraph_finalize(GObject *object)
{
	Plotgraph* self = PLOTGRAPH(object);

	// Free allocated structures
	g_free(self->points);
	g_free(self->xtick_values);
	g_free(self->ytick_values);

	// Call parent finalize function
	if (G_OBJECT_CLASS (plotgraph_parent_class)->finalize)
		G_OBJECT_CLASS (plotgraph_parent_class)->finalize (object);
}


static
void plotgraph_class_init(PlotgraphClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = plotgraph_set_property;
	object_class->finalize = plotgraph_finalize;

	g_object_class_install_property(G_OBJECT_CLASS(klass), YMIN_VALUE,
	                                g_param_spec_float("ymin-value", "yMin",
	                                                    "The maximum displayable value on y-axis",
	                                                    -FLT_MAX, FLT_MAX, -1.0,
	                                                    G_PARAM_WRITABLE |
	                                                    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(G_OBJECT_CLASS(klass), YMAX_VALUE,
	                                g_param_spec_float("ymax-value", "yMax",
	                                                    "The maximum displayable value on y-axis",
	                                                    -FLT_MAX, FLT_MAX, 1.0,
	                                                    G_PARAM_WRITABLE |
	                                                    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(G_OBJECT_CLASS(klass), XMIN_VALUE,
	                                g_param_spec_float("xmin-value", "xMin",
	                                                    "The maximum displayable value on x-axis",
	                                                    -FLT_MAX, FLT_MAX, -1.0,
	                                                    G_PARAM_WRITABLE |
	                                                    G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(G_OBJECT_CLASS(klass), XMAX_VALUE,
	                                g_param_spec_float("xmax-value", "xMax",
	                                                    "The maximum displayable value on x-axis",
	                                                    -FLT_MAX, FLT_MAX, 1.0,
	                                                    G_PARAM_WRITABLE |
	                                                    G_PARAM_STATIC_STRINGS));
}


static
void plotgraph_init(Plotgraph *self)
{
	// Initialize members
	self->num_xticks = 0;
	self->num_yticks = 0;
	self->xtick_values = NULL;
	self->ytick_values = NULL;
	self->num_points = 0;
	self->points = NULL;
	self->xmin = -1.0f;
	self->xmax = 1.0f;
	self->ymin = -1.0f;
	self->ymax = 1.0f;

	plot_area_set_ticks(PLOT_AREA(self), self->num_xticks, self->num_yticks);

	// Connect the handled signal
	g_signal_connect_after(G_OBJECT(self), "configure_event",
				G_CALLBACK(plotgraph_configure_event_callback), NULL);
	g_signal_connect(G_OBJECT(self), "expose_event",
				G_CALLBACK(plotgraph_expose_event_callback), NULL);
}


LOCAL_FN
Plotgraph* plotgraph_new(void)
{
	return g_object_new(TYPE_PLOTGRAPH, NULL);
}


/**
 * plotgraph_set_datalen() - change the number of point in the plot
 * @self:       pointer to initialized plotgraph
 * @len:        new length of the data array of the plot
 * @xmin:       the x-coordinate of the first point
 * @xmax:       the x-coordinate of the last point
 *
 * This function specify a new number of points @len used to form the plot
 * and define the size of the array of data expected in next calls to
 * plotgraph_update_data().
 *
 * This function set the limits of the plot along the x-axis. The @xmin and
 * @xmax unit are the same as the one used in the "xmin-value" and
 * "xmax-value" properties of the Plotgraph widget.
 */
LOCAL_FN
void plotgraph_set_datalen(Plotgraph* self, int len, float xmin, float xmax)
{
	gsize mem_size;

	// There must be at least 2 points... If len is invalid, behave as
	// if there is no data points
	if (len < 2)
		len = 0;

	self->points_xmin = xmin;
	self->points_xmax = xmax;

	// Resize points array only if changed
	if (len != self->num_points) {
		mem_size = len * sizeof(*self->points);
		self->num_points = len;
		self->points = g_realloc(self->points, mem_size);
		memset(self->points, 0, mem_size);
	}

	plotgraph_calculate_drawparameters(self);
}


/**
 * plotgraph_update_data() - update signal data in the plotgraph
 * @self:       pointer to initialized plotgraph
 * @data:       array of data of length @self->num_points
 *
 * Updates the data to be displayed, ie the plot data. This does not redraw
 * directly the data. Instead a request to redraw the widget is queued which
 * will be coalesced with other requests if several data update occured in
 * a recent time. Note that if the widget is not visible, no redraw will be
 * queued.
 */
LOCAL_FN
void plotgraph_update_data(Plotgraph* self, const float* data)
{
	int i;
	float sc, off;

	if (!self)
		return;

	// Transform data into array of points
	sc = self->yscale;
	off = self->yoffset;
	for (i = 0; i < self->num_points; i++)
		self->points[i].y = sc * data[i] + off;

	// Queue redraw if the plotgraph is visible
	if (GTK_WIDGET_DRAWABLE(self))
		gtk_widget_queue_draw(GTK_WIDGET(self));
}


LOCAL_FN
void plotgraph_set_xticks(Plotgraph* self, int num_ticks, const float* xtick_values)
{
	gsize sz = num_ticks * sizeof(*self->xtick_values);

	if (num_ticks != self->num_xticks) {
		self->xtick_values = g_realloc(self->xtick_values, sz);
		self->num_xticks = num_ticks;
		plot_area_set_ticks(PLOT_AREA(self), self->num_xticks, self->num_yticks);
	}
	memcpy(self->xtick_values, xtick_values, sz);

	plotgraph_calculate_drawparameters(self);
}


LOCAL_FN
void plotgraph_set_yticks(Plotgraph* self, int num_ticks, const float* ytick_values)
{
	gsize sz = num_ticks * sizeof(*self->ytick_values);

	if (num_ticks != self->num_yticks) {
		self->ytick_values = g_realloc(self->ytick_values, sz);
		self->num_yticks = num_ticks;
		plot_area_set_ticks(PLOT_AREA(self), self->num_xticks, self->num_yticks);
	}
	memcpy(self->ytick_values, ytick_values, sz);

	plotgraph_calculate_drawparameters(self);
}
