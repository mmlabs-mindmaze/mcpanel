/*
	Copyright (C) 2008-2010 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

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
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <memory.h>
#include "scope.h"

enum {
	DUMMY_PROP,
	SCALE_PROP
};

static void scope_calculate_drawparameters(Scope* self);
static gboolean scope_expose_event_callback(Scope *self, GdkEventExpose *event, gpointer data);
static gboolean scope_configure_event_callback(Scope *self, GdkEventConfigure *event, gpointer data);

LOCAL_FN GType scope_get_type(void);
G_DEFINE_TYPE (Scope, scope, TYPE_PLOT_AREA)
#define GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), TYPE_SCOPE, ScopePrivate))


/*
static void
scope_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}
*/

static void
scope_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
	Scope* self = SCOPE(object);

	switch (property_id) {
	case SCALE_PROP:
		self->phys_scale = (data_t)g_value_get_double(value);
		scope_calculate_drawparameters(self);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}

}

static void
scope_finalize (GObject *object)
{
	Scope* self = SCOPE(object);
	
	// Free allocted structures
	g_free(self->ticks);
	g_free(self->xpos);
	g_free(self->path);
	if (self->surface != NULL)
		cairo_surface_destroy(self->surface);
	
	// Call parent finalize function
	if (G_OBJECT_CLASS (scope_parent_class)->finalize)
		G_OBJECT_CLASS (scope_parent_class)->finalize (object);
}


static void
scope_class_init (ScopeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

//	object_class->get_property = scope_get_property;
	object_class->set_property = scope_set_property;
	object_class->finalize = scope_finalize;


	g_object_class_install_property(G_OBJECT_CLASS(klass),
					SCALE_PROP,
					g_param_spec_double("scale",
							"Scale",
							"The scale of the divisions",
							0.0,
							1.7E308,
							1.0,
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));
}

static void
scope_init (Scope *self)
{
	// Initialize members
	self->num_channels = 0;
	self->num_ticks = 0;
	self->num_points = 0;
	self->phys_scale = (data_t)1;
	self->current_pointer = 0; 
	self->data = NULL;
	self->ticks = NULL;
	self->xpos = NULL;
	self->path = NULL;
	self->surface = NULL;

	plot_area_set_ticks(PLOT_AREA(self), self->num_ticks, self->num_channels);

	// Connect the handled signal
	g_signal_connect_after(G_OBJECT(self), "configure_event",  
	                  G_CALLBACK(scope_configure_event_callback), NULL);
	g_signal_connect(G_OBJECT(self), "expose_event",  
	                 G_CALLBACK(scope_expose_event_callback), NULL);
}


LOCAL_FN
Scope* scope_new (void)
{
	return g_object_new(TYPE_SCOPE, NULL);
}


static 
void scope_draw_samples(const Scope* restrict self, cairo_t* cr,
                   int first, int last)
{
	int ich, is, nch, nColors, value, width, height;
	const double* restrict offsets = PLOT_AREA(self)->yticks;
	data_t scale = self->scale;
	const data_t* restrict data = self->data;
	const GdkColor * restrict colors = PLOT_AREA(self)->colors;
	cairo_path_t cpath;

	nColors = PLOT_AREA(self)->nColors;
	width = GTK_WIDGET(self)->allocation.width;
	height = GTK_WIDGET(self)->allocation.height;
	nch = self->num_channels;	

	cpath.data = self->path +2*first;
	cpath.num_data = 2*(last - first + 1);
	cpath.status = CAIRO_STATUS_SUCCESS;
	self->path[2*first].header.type = CAIRO_PATH_MOVE_TO;

	// Draw the channels data
	for (ich=0; ich<nch; ich++) {
		// Convert data_t values into y coordinate
		// (positive y xpos to bottom in the window basis) 
		for (is=first; is<=last; is++) {
			value = offsets[ich] - (scale*data[is*nch+ich]);
			if (value < 0)
				value = 0;
			if (value > height)
				value = height;
				
			self->path[2*is+1].point.y = value;
		}

		// Draw calculated lines
		cairo_append_path(cr, &cpath);
		gdk_cairo_set_source_color(cr, colors + (ich%nColors));
		cairo_stroke(cr);
	}
	self->path[2*first].header.type = CAIRO_PATH_LINE_TO;
}


static
void scope_draw_grid(Scope* restrict self, cairo_t* cr,
                     const GdkRectangle* restrict rect)
{
	unsigned int i;
	double xmin, xmax, ymin, ymax;
	const double* restrict offsets = PLOT_AREA(self)->yticks;
	const double* restrict xticks = PLOT_AREA(self)->xticks;

	xmin = rect->x + 0.5;
	xmax = (rect->x + rect->width) - 0.5;
	gdk_cairo_set_source_color(cr, &(PLOT_AREA(self)->grid_color));
	for (i=0; i<self->num_channels; i++) {
		cairo_move_to(cr, xmin, offsets[i]+0.5);
		cairo_line_to(cr, xmax, offsets[i]+0.5);
	}

	ymin = rect->y + 0.5;
	ymax = (rect->y + rect->height) - 0.5;
	for (i=0; i<self->num_ticks; i++) {
		if ((xticks[i]>=xmin) && (xticks[i]<=xmax)) {
			cairo_move_to(cr, xticks[i]+0.5, ymin);
			cairo_line_to(cr, xticks[i]+0.5, ymax);
		}
	}
	cairo_stroke(cr);
}


static
void scope_rectangle_draw(Scope* restrict self, cairo_t* cr,
                          int first, int last,
			  const GdkRectangle* restrict rect)
{
	int cp = self->current_pointer;
	cairo_save(cr);

	// Set up the clip region
	gdk_cairo_rectangle(cr, rect);
	cairo_clip(cr);

	// Redraw background
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_paint(cr);

	// draw grid
	scope_draw_grid(self, cr, rect);

	// Draw signals
	if (first != last)
		scope_draw_samples(self, cr, first, last);

	// Draw the scanline
	if (self->num_points) {
		cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
		cairo_move_to(cr, self->xpos[cp]+0.5, 0);
		cairo_line_to(cr, self->xpos[cp]+0.5, rect[0].height);
		cairo_stroke(cr);
	}
	
	cairo_restore(cr);
}


static 
int scope_get_update_extent(Scope* restrict self, int full,
                            int first[2], int last[2], GdkRectangle rect[2])
{
	int nrect = 1;
	int ref, *xpos = self->xpos;

	rect[0].y = rect[1].y = 0;
	rect[0].height = GTK_WIDGET(self)->allocation.height;
	rect[1].height = rect[0].height;

	if (!full) {
		first[0] = self->last_draw_pointer;
		last[0] = self->current_pointer;
		
		// redraw a little bit before to smooth the transition
		ref = xpos[first[0]];
		while (first[0] > 0 && xpos[first[0]] == ref)
			first[0]--;

		if (first[0] > last[0]) {
			nrect++;
			first[1] = first[0];
			first[0] = 0;
			last[1] = self->num_points - 1;
			rect[1].x = xpos[first[1]];
			rect[1].width = GTK_WIDGET(self)->allocation.width
			                - xpos[first[1]];
		}
		rect[0].x = xpos[first[0]];
		rect[0].width = xpos[last[0]] - xpos[first[0]] + 1;

	} else {
		first[0] = 0;
		last[0] = self->num_points ? self->num_points-1 : 0;
		rect[0].x = 0;
		rect[0].width = GTK_WIDGET(self)->allocation.width;
	}

	return nrect;
}


static
void scope_update_draw(Scope* restrict self, int full)
{
	GdkRectangle rect[2];
	GdkRegion* reg;
	int r, nrect;
	int first[2], last[2];
	cairo_t* cr;

	if (!full && self->last_draw_pointer == self->current_pointer)
		return;

	nrect = scope_get_update_extent(self, full, first, last, rect);
	reg = gdk_region_new();
	
	// Prepare cairo context to draw on the offscreen surface
	cr = cairo_create(self->surface);
	cairo_set_line_width (cr, 1.0);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	
	// Redraw by part
	for (r=0; r<nrect; r++) {
		scope_rectangle_draw(self, cr, first[r], last[r], rect + r);
		gdk_region_union_with_rect(reg, rect + r);
	}

	// The offscreen is now updated
	cairo_destroy(cr);
	
	// Request a redraw of the updated region
	gdk_window_invalidate_region(GTK_WIDGET(self)->window, reg, FALSE);
	gdk_region_destroy(reg);

	self->last_draw_pointer = self->current_pointer;
}


static
void scope_calculate_drawparameters(Scope* self)
{
	guint height, width;
	unsigned int num_ch, i, num_points;
	double* offsets = PLOT_AREA(self)->yticks;
	double* xticks = PLOT_AREA(self)->xticks;

	num_ch = self->num_channels;
	num_points = self->num_points;

	width = GTK_WIDGET(self)->allocation.width;
	height = GTK_WIDGET(self)->allocation.height;

	// calculate scaling_factor;
	self->scale = num_ch ? ((data_t)height)/(self->phys_scale * (data_t)num_ch) : 1;

	/* Calculate y offsets */
	for (i=0; i<num_ch; i++)
		offsets[i] = ((double)(height*(2*i+1)) / (2*num_ch));

	/* Calculate x coordinates*/
	for (i=0; i<num_points; i++) {
		self->xpos[i] = (gint)( ((float)(i*width))/(float)(num_points-1) );
		self->path[2*i+1].point.x = self->xpos[i];
	}

	// Set the ticks position
	for (i=0; i<self->num_ticks; i++)
		xticks[i] = (num_points > self->ticks[i]) ?
		                  self->xpos[self->ticks[i]] : -1;
}

static
gboolean scope_configure_event_callback (Scope *self,
         	                         GdkEventConfigure *event,
                                         gpointer data)
{
	(void)event;
	(void)data;
	GtkWidget* widg = GTK_WIDGET(self);

	if (self->surface != NULL)
		cairo_surface_destroy(self->surface);
	
	// Create a offscreen surface similar to the window
#if HAVE_GDK_WINDOW_CREATE_SIMILAR_SURFACE
	self->surface = gdk_window_create_similar_surface(widg->window,
	                                           CAIRO_CONTENT_COLOR,
						   widg->allocation.width,
						   widg->allocation.height);
#else
	cairo_t* cr = gdk_cairo_create(widg->window);
	cairo_surface_t* surf = cairo_get_target(cr);
	self->surface = cairo_surface_create_similar(surf,
	                                           CAIRO_CONTENT_COLOR,
						   widg->allocation.width,
						   widg->allocation.height);
	cairo_destroy(cr);
#endif
	

	scope_calculate_drawparameters (self);
	scope_update_draw(self, 1);

	return TRUE;
}


static gboolean
scope_expose_event_callback (Scope *self,
                             GdkEventExpose *event,
                             gpointer data)
{
	(void)data;
	cairo_t* cr;

	cairo_surface_flush(self->surface);
	
	cr = gdk_cairo_create(GTK_WIDGET(self)->window);
	cairo_set_source_surface(cr, self->surface, 0.0, 0.0);
	gdk_cairo_region(cr, event->region);
	cairo_fill(cr);
	cairo_destroy(cr);

	return TRUE;
}




LOCAL_FN
void scope_update_data(Scope* self, guint pointer)
{
	if (!self || !self->num_points)
		return;

	self->current_pointer = pointer;
	if (GTK_WIDGET_DRAWABLE(self)) 
		scope_update_draw(self, 0);
}


LOCAL_FN
void scope_set_data(Scope* self, data_t* data, guint num_points, guint num_ch)
{
	int i, has_changed = 0;
	if (self==NULL)
		return;

	self->data = data;
	self->current_pointer = self->last_draw_pointer = 0;

	if (num_points != self->num_points) {
		g_free(self->xpos);
		self->xpos = g_malloc(num_points*sizeof(*(self->xpos)));
		self->path = g_malloc0(2*num_points*sizeof(*(self->path)));
		for (i=0; i<num_points; i++) {
			self->path[2*i].header.type = CAIRO_PATH_LINE_TO; 
			self->path[2*i].header.length = 2; 
		}
		self->num_points = num_points;
		has_changed = 1;
	}

	if (self->num_channels != num_ch) {
		self->num_channels = num_ch;
		plot_area_set_ticks(PLOT_AREA(self), self->num_ticks, self->num_channels);
		has_changed = 1;
	}

	if (has_changed) {
		scope_calculate_drawparameters(self);
	}
	if (GTK_WIDGET_DRAWABLE(self))
		gtk_widget_queue_draw(GTK_WIDGET(self));
}


LOCAL_FN
void scope_set_ticks(Scope* self, guint num_ticks, guint* ticks)
{
	if (num_ticks != self->num_ticks) {
		g_free(self->ticks);
		self->ticks = g_malloc(num_ticks*sizeof(*(self->ticks)));
		self->num_ticks = num_ticks;
		plot_area_set_ticks(PLOT_AREA(self), self->num_ticks, self->num_channels);
	}

	memcpy(self->ticks, ticks, num_ticks*sizeof(*(self->ticks)));	
	scope_calculate_drawparameters(self);
}
