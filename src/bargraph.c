/*
	Copyright (C) 2008-2009,2011 Nicolas Bourdaud
	<nicolas.bourdaud@epfl.ch>

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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gtk/gtk.h>
#include <memory.h>
#include "bargraph.h"

enum {
	DUMMY_PROP,
	BAR_RATIO,
	MIN_VALUE,
	MAX_VALUE
};

static void bargraph_calculate_drawparameters(Bargraph* self);
static void bargraph_draw_samples(const Bargraph* self, cairo_t* cr);
static gboolean bargraph_expose_event_callback(Bargraph *self, GdkEventExpose *event, gpointer data);
static gboolean bargraph_configure_event_callback(Bargraph *self, GdkEventConfigure *event, gpointer data);

LOCAL_FN GType bargraph_get_type(void);
G_DEFINE_TYPE (Bargraph, bargraph, TYPE_PLOT_AREA)
#define GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), TYPE_BARGRAPH, BargraphPrivate))


/*
static void
bargraph_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
	
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}
*/

static
void bargraph_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
	Bargraph* self = BARGRAPH(object);

	switch (property_id) {
	case BAR_RATIO:
		self->bar_ratio = g_value_get_float(value);
		break;

	case MIN_VALUE:
		self->min = (data_t)g_value_get_double(value);
		bargraph_calculate_drawparameters(self);
		break;

	case MAX_VALUE:
		self->max = (data_t)g_value_get_double(value);
		bargraph_calculate_drawparameters(self);
		break;
	
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static
void bargraph_finalize (GObject *object)
{
	Bargraph* self = BARGRAPH(object);
	
	// Free allocated structures
	g_free(self->tick_values);
	g_free(self->grid_data);
	
	// Call parent finalize function
	if (G_OBJECT_CLASS (bargraph_parent_class)->finalize)
		G_OBJECT_CLASS (bargraph_parent_class)->finalize (object);
}


static
void bargraph_class_init (BargraphClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	//object_class->get_property = bargraph_get_property;
	object_class->set_property = bargraph_set_property;
	object_class->finalize = bargraph_finalize;


	
	g_object_class_install_property(G_OBJECT_CLASS(klass),
					BAR_RATIO,
					g_param_spec_float("bar-ratio",
							"Ratio",
							"The color used to plot the grid",
							0.0f,
							1.0f,
							0.66f,
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(G_OBJECT_CLASS(klass),
					MIN_VALUE,
					g_param_spec_double("min-value",
							"Ratio",
							"The color used to plot the grid",
							-1.7E308,
							1.7E308,
							-1.0,
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(G_OBJECT_CLASS(klass),
					MAX_VALUE,
					g_param_spec_double("max-value",
							"Ratio",
							"The color used to plot the grid",
							-1.7E308,
							1.7E308,
							1.0,
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));
}

static void
bargraph_init (Bargraph *self)
{
	// Initialize members
	self->num_channels = 0;
	self->num_ticks = 0;
	self->scale = (data_t)1;
	self->current_pointer = 0; 
	self->data = NULL;
	self->tick_values = NULL;
	self->offset = 0;
	self->min = -1;
	self->max = 1;
	self->bar_ratio = 0.666f;
	
	plot_area_set_ticks(PLOT_AREA(self), self->num_channels, self->num_ticks);

	// Connect the handled signal
	g_signal_connect_after(G_OBJECT(self), "configure_event",  
				G_CALLBACK(bargraph_configure_event_callback), NULL);
	g_signal_connect(G_OBJECT(self), "expose_event",  
				G_CALLBACK(bargraph_expose_event_callback), NULL);
}


LOCAL_FN
Bargraph* bargraph_new (void)
{
	return g_object_new(TYPE_BARGRAPH, NULL);
}


static
gboolean bargraph_configure_event_callback(Bargraph *self,
                                           GdkEventConfigure *event,
                                           gpointer data)
{
	(void)data;
	(void)event;
	bargraph_calculate_drawparameters (self);
	return TRUE;
}


static
gboolean bargraph_expose_event_callback(Bargraph *self,
                                        GdkEventExpose *event,
                                        gpointer data)
{
	(void)data;
	(void)event;

	cairo_t* cr;

	cr = gdk_cairo_create(gtk_widget_get_window(GTK_WIDGET(self)));

	// Set up plot style
	cairo_set_line_width(cr, 1.0);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);

	/* Redraw the region */
	bargraph_draw_samples(self, cr);
	
	cairo_destroy(cr);

	return TRUE;
}


static
void bargraph_draw_samples(const Bargraph* self, cairo_t* cr)
{
	guint ich, ncols;
	int halfwidth;
	const double* chann_pos = PLOT_AREA(self)->xticks;
	const GdkColor* colors = PLOT_AREA(self)->colors;
	const unsigned int width = GTK_WIDGET(self)->allocation.width;
	const data_t* values = self->data + self->current_pointer*self->num_channels;
	data_t val;
	ncols = PLOT_AREA(self)->nColors;

	// draw grid
	gdk_cairo_set_source_color(cr, &(PLOT_AREA(self)->grid_color));
	cairo_append_path(cr, &self->grid_path);
	cairo_stroke(cr);
	
	// Draw the channels data
	if (self->num_channels) {
		halfwidth = 0.5f*self->bar_ratio*width / (float)self->num_channels;
		for (ich=0; ich<self->num_channels; ich++) {
			gdk_cairo_set_source_color(cr, colors + ich%ncols);

			// val to the limits
			val = values[ich];
			if (val > self->max)
				val = self->max;
			if (val < self->min)
				val = self->min;
				
			// positive y points to bottom in the window basis 
			cairo_rectangle(cr,
			                chann_pos[ich] - halfwidth,
			                self->offset,
					2*halfwidth,
					val*self->scale);
			cairo_fill(cr);
		}
	}
}


static
void bargraph_calculate_drawparameters(Bargraph* self)
{
	guint height, width, i;
	double *xticks, *yticks;

	width = GTK_WIDGET(self)->allocation.width;
	height = GTK_WIDGET(self)->allocation.height;
	xticks = PLOT_AREA(self)->xticks;
	yticks = PLOT_AREA(self)->yticks;

	// Calculate scaling parameters
	self->scale = ((gfloat)height) / (self->max - self->min);
	self->offset = height + (gint)(self->min * self->scale); 
	
	// Calculate x ticks
	for (i=0; i<self->num_channels; i++)
		xticks[i] = (gint)((gfloat)(width * (2*i+1)) / (gfloat)(2*self->num_channels));
	
	// Calculate ticks values and path data
	// (positive y points to bottom in the window basis) 
	for (i=0; i<self->num_ticks; i++) {
		yticks[i] = self->offset - self->tick_values[i]*self->scale;
		self->grid_data[4*i].header.length = 2;
		self->grid_data[4*i].header.type = CAIRO_PATH_MOVE_TO;
		self->grid_data[4*i+1].point.x = 0.5;
		self->grid_data[4*i+1].point.y = yticks[i] + 0.5;
		self->grid_data[4*i+2].header.length = 2;
		self->grid_data[4*i+2].header.type = CAIRO_PATH_LINE_TO;
		self->grid_data[4*i+3].point.x = width - 0.5;
		self->grid_data[4*i+3].point.y = yticks[i] + 0.5;
	}
}


LOCAL_FN
void bargraph_update_data(Bargraph* self, guint pointer)
{
	if (!self)
		return;

	self->current_pointer = pointer;

	if (self->data == NULL)
		return;

	if (GTK_WIDGET_DRAWABLE(self)) 
		gtk_widget_queue_draw(GTK_WIDGET(self));
}


LOCAL_FN
void bargraph_set_data(Bargraph* self, data_t* data, guint num_ch)
{
	self->data = data;

	if (num_ch != self->num_channels) {
		self->num_channels = num_ch;
		plot_area_set_ticks(PLOT_AREA(self), self->num_channels, self->num_ticks);
		bargraph_calculate_drawparameters(self);
	}
	
	if (GTK_WIDGET_DRAWABLE(self))
		gtk_widget_queue_draw(GTK_WIDGET(self));
}


LOCAL_FN
void bargraph_set_ticks(Bargraph* self, guint nticks, data_t* tvalues)
{
	if (nticks != self->num_ticks) {
		g_free(self->grid_data);
		self->grid_data = g_malloc(4*nticks*sizeof(*(self->grid_data)));
		self->tick_values = g_malloc(nticks*sizeof(*(tvalues)));
		self->num_ticks = nticks;
		plot_area_set_ticks(PLOT_AREA(self), 
		                    self->num_channels, self->num_ticks);
		self->grid_path.data = self->grid_data;
		self->grid_path.num_data = 4*nticks;
	}

	memcpy(self->tick_values, tvalues, nticks*sizeof(*(tvalues)));
	bargraph_calculate_drawparameters(self);
}
