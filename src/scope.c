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
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <memory.h>
#include <stdio.h>
#include "mcpanel.h"
#include "scope.h"

enum {
	DUMMY_PROP,
	SCALE_PROP
};

struct scope_event {
	int pos;
	uint32_t type;
	int drawn_once;
};

static void scope_calculate_drawparameters(Scope* self);
static void scope_draw_samples(Scope* self, unsigned int first, unsigned int last);
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
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}

	if (GTK_WIDGET_DRAWABLE(self)) {
		scope_calculate_drawparameters(self);
		gtk_widget_queue_draw(GTK_WIDGET(self));
	}
}

static void
scope_finalize (GObject *object)
{
	Scope* self = SCOPE(object);

	g_mutex_clear(&self->mtx);
	
	// Free allocted structures
	g_free(self->ticks);
	g_free(self->points);
	g_free(self->events);

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
	self->points = NULL;
	self->nevent = 0;
	self->nevent_max = 0;
	self->events = NULL;
	g_mutex_init(&self->mtx);

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
gboolean scope_configure_event_callback (Scope *self,
         	                         GdkEventConfigure *event,
                                         gpointer data)
{
	(void)event;
	(void)data;

	scope_calculate_drawparameters (self);
	return TRUE;
}


static gboolean
scope_expose_event_callback (Scope *self,
                             GdkEventExpose *event,
                             gpointer data)
{
	(void)data;
	unsigned int first, last, num_points;
	int xmin, xmax, i, nrect;
	GdkPoint* points = self->points;
	GdkRectangle* rect;
	
	num_points = self->num_points;
	if (num_points == 0)
		return TRUE;

	gdk_region_get_rectangles(event->region, &rect, &nrect);

	for (i=0; i<nrect; i++) {
		/* Determine which samples should redrawn */
		xmin = rect[i].x;
		xmax = rect[i].x + rect[i].width;
		first=0;
		while ((first<num_points-1) && (points[first+1].x < xmin))
			first++;
        
		last=num_points-1;
		while ((last>0) && (points[last-1].x > xmax))
			last--;
	
		/* Redraw the region */
		scope_draw_samples(self, first, last);
	}
	g_free(rect);

	return TRUE;
}


static
void scope_draw_events(Scope* self)
{
	int i, x, slen, evwidth, evheight;
	struct scope_event* events;
	PangoLayout* layout;
	PangoContext* context;
	PangoFontDescription* desc;
	char eventcode[11];
	GdkPoint* points = self->points;
	GdkWindow* window = GTK_WIDGET(self)->window;
	GdkGC* plotgc = PLOT_AREA(self)->plotgc;
	const GdkColor *colors = PLOT_AREA(self)->colors;
	int num_colors = PLOT_AREA(self)->nColors;
	int height = GTK_WIDGET(self)->allocation.height;
	GdkRectangle rect = {0, 0, GTK_WIDGET(self)->allocation.width, GTK_WIDGET(self)->allocation.height};

	g_mutex_lock(&self->mtx);

	events = self->events;
	if (!self->num_displayed_event)
		goto exit;

	// Setup pango context for small fonts
	layout = gtk_widget_create_pango_layout(GTK_WIDGET(self), NULL);
	context = gtk_widget_get_pango_context(GTK_WIDGET(self));
	desc = pango_font_description_copy_static(pango_context_get_font_description(context));
	pango_font_description_set_size(desc, 6 * PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);
	gdk_gc_set_clip_rectangle(plotgc, &rect);

	for (i = 0; i < self->num_displayed_event; i++) {
		// Event code text
		slen = sprintf(eventcode, "0x%x", events[i].type);
		pango_layout_set_text(layout, eventcode, slen);
		pango_layout_get_pixel_size(layout, &evwidth, &evheight);

		x = points[(events[i].pos - self->ns_offset) % self->num_points].x;
		gdk_gc_set_foreground(plotgc, colors + events[i].type % num_colors);
		gdk_draw_line(window, plotgc, x, 0, x, height - evheight);
		gdk_draw_layout(window, plotgc, x - evwidth / 2, height - evheight - 2, layout);
	}

	g_object_unref(layout);
	pango_font_description_free(desc);

exit:
	g_mutex_unlock(&self->mtx);
}


static void
scope_draw_samples(Scope* self, unsigned int first, unsigned int last)
{
	unsigned int iChannel, iSample, iColor, num_channels, nColors;
	GdkPoint* points = self->points;
	const gint* offsets = PLOT_AREA(self)->yticks;
	const gint* xticks = PLOT_AREA(self)->xticks;
	gint xmin, xmax, value, height;
	data_t scale = self->scale;
	const data_t* data = self->data;
	unsigned int i;
	const GdkColor *grid_color, *colors;
	GdkWindow* window = GTK_WIDGET(self)->window;
	GdkGC* plotgc = PLOT_AREA(self)->plotgc;

	nColors = PLOT_AREA(self)->nColors;
	colors = PLOT_AREA(self)->colors;
	grid_color = &(PLOT_AREA(self)->grid_color);
	height = GTK_WIDGET(self)->allocation.height;
	num_channels = self->num_channels;	

	// Find the position of the first sample
	xmin = points[first].x;
	while ((first>0) && (xmin == points[first].x))
		first--;

	xmin = points[first].x;
	xmax = points[last].x;

	// draw grid
	gdk_gc_set_foreground ( plotgc, grid_color );
	for (i=0; i<num_channels; i++) {
		gdk_draw_line(window,
				plotgc,
				xmin,
				offsets[i],
				xmax,
				offsets[i]);
	}
	for (i=0; i<self->num_ticks; i++) {
		if ((xticks[i]>=xmin) && (xticks[i]<=xmax))
			gdk_draw_line(window,
					plotgc,
					xticks[i],
					0,
					xticks[i],
					height);
	}

	if (data == NULL)
		return;


	// Draw the channels data
	for (iChannel=0; iChannel<num_channels; iChannel++) {
		// Convert data_t values into y coordinate
		// (positive y points to bottom in the window basis) 
		for (iSample=first; iSample<=last; iSample++) {
			value = offsets[iChannel] - 
				(gint)(scale*data[iSample*num_channels+iChannel]);
			if (value < 0)
				value = 0;
			if (value > height)
				value = height;
				
			points[iSample].y = value;
		}

		// Draw calculated lines
		iColor = iChannel % nColors;
		gdk_gc_set_foreground ( plotgc,	colors + iColor );
		gdk_draw_lines (window,
		                plotgc,
				points + first,
				last - first + 1);
	}

	// Draw the scanline
	gdk_draw_line(window,
			GTK_WIDGET(self)->style->fg_gc[gtk_widget_get_state(GTK_WIDGET(self))],
			points[self->current_pointer].x,
			0,
			points[self->current_pointer].x,
			height - 1);

	scope_draw_events(self);
}


static
void scope_calculate_evt_label_width(Scope* self)
{
	int evwidth, evheight;
	PangoLayout* layout;
	PangoContext* context;
	PangoFontDescription* desc;

	// Setup pango context for small fonts
	layout = gtk_widget_create_pango_layout(GTK_WIDGET(self), NULL);
	context = gtk_widget_get_pango_context(GTK_WIDGET(self));
	desc = pango_font_description_copy_static(pango_context_get_font_description(context));
	pango_font_description_set_size(desc, 6 * PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);

	pango_layout_set_text(layout, "0xFFFFFFFF", -1);
	pango_layout_get_pixel_size(layout, &evwidth, &evheight);

	g_object_unref(layout);
	pango_font_description_free(desc);

	self->evt_label_width = evwidth;
}


static
void scope_calculate_drawparameters(Scope* self)
{
	guint height, width;
	unsigned int num_ch, i, num_points;
	gint* offsets = PLOT_AREA(self)->yticks;
	gint* xticks = PLOT_AREA(self)->xticks;

	num_ch = self->num_channels;
	num_points = self->num_points;

	width = GTK_WIDGET(self)->allocation.width;
	height = GTK_WIDGET(self)->allocation.height;

	// calculate scaling_factor;
	self->scale = num_ch ? ((data_t)height)/(self->phys_scale * (data_t)num_ch) : 1;

	/* Calculate y offsets */
	for (i=0; i<num_ch; i++)
		offsets[i] = (gint)((float)(height*(2*i+1)) / (float)(2*num_ch));

	/* Calculate x coordinates*/
	for (i=0; i<num_points; i++)
		self->points[i].x = (gint)( ((float)(i*width))/(float)(num_points-1) );

	// Set the ticks position
	for (i=0; i<self->num_ticks; i++)
		xticks[i] = (num_points > (unsigned int) self->ticks[i]) ? self->points[self->ticks[i]].x : -1;

	scope_calculate_evt_label_width(self);
}


static
void scope_queue_event_draw(Scope* self, const struct scope_event* evt)
{
	GdkRectangle rect;
	int x;

	if (!GTK_WIDGET_DRAWABLE(self))
		return;

	x = self->points[(evt->pos - self->ns_offset) % self->num_points].x;

	rect.x = x - self->evt_label_width/2;
	rect.y = 0;
	rect.width = self->evt_label_width;
	rect.height = GTK_WIDGET(self)->allocation.height;

	gdk_window_invalidate_rect(gtk_widget_get_window(GTK_WIDGET(self)),
	                           &rect, FALSE);
}


static
void scope_update_events(Scope* self, int ns_total)
{
	int i, first, discard_lim;
	struct scope_event* events = self->events;

	g_mutex_lock(&self->mtx);
	self->ns_total = ns_total;
	self->ns_offset = (ns_total - self->current_pointer) % self->num_points;

	// Find the index of first event not outdated
	discard_lim = ns_total - self->num_points;
	for (first = 0; first < self->nevent; first++) {
		if (events[first].pos > discard_lim) {
			break;
		}
	}

	// Remove events before first
	if (first > 0) {
		self->nevent -= first;
		memmove(events, events + first, self->nevent * sizeof(*events));
	}

	// Find the number of event that can be displayed
	for (i = 0; i < self->nevent; i++) {
		if (events[i].pos > ns_total)
			break;

		// queue a draw if it is new
		if (!events[i].drawn_once) {
			scope_queue_event_draw(self, &events[i]);
			events[i].drawn_once = 1;
		}
	}
	self->num_displayed_event = i;

	g_mutex_unlock(&self->mtx);
}


LOCAL_FN
void scope_update_data(Scope* self, guint pointer, int ns_total)
{
	int first, last;
	GdkRectangle rect[2];
	int combine = 0;
	//GdkWindow* window;
	GdkPoint* points;
	GdkRegion* region;

	if (!self || !self->num_points)
		return;

	if (GTK_WIDGET_DRAWABLE(self)) {
		points  = self->points;

		// Set the region that should be redrawn 
		first = self->current_pointer ? self->current_pointer -1 : 0;
		last = pointer;
		if (pointer < self->current_pointer) {
			rect[1].x = points[first].x-1;
			rect[1].width = points[self->num_points-1].x - rect[1].x+1;
			first = 0;
			combine++;
		}
		rect[0].y = rect[1].y = 0;
		rect[0].height = rect[1].height = GTK_WIDGET(self)->allocation.height;
		rect[0].x = points[first].x-1;
		rect[0].width = points[last].x - rect[0].x+1;

		// Repaint
		region = gdk_region_rectangle(&rect[0]);
		if (combine)
			gdk_region_union_with_rect(region, &rect[1]);
		gdk_window_invalidate_region(gtk_widget_get_window(GTK_WIDGET(self)),
					     region, FALSE);
		gdk_region_destroy(region);
	}

	self->current_pointer = pointer;
	scope_update_events(self, ns_total);
}


LOCAL_FN
void scope_set_data(Scope* self, data_t* data, guint num_points, guint num_ch)
{
	int has_changed = 0;
	if (self==NULL)
		return;

	self->data = data;
	self->current_pointer = 0;

	if (num_points != self->num_points) {
		g_free(self->points);
		self->points = g_malloc0(num_points*sizeof(GdkPoint));
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
void scope_add_events(Scope* self, int nevent, const struct mcp_event* added_events)
{
	int i, insert_at;
	struct scope_event evt;
	struct scope_event* events;

	if (self == NULL)
		return;

	g_mutex_lock(&self->mtx);

	// Realloc if necessary
	if (nevent + self->nevent > self->nevent_max) {
		self->nevent_max = nevent + self->nevent;
		self->events = g_realloc(self->events, self->nevent_max*sizeof(*events));
	}

	// Add event while respecting chronological order
	events = self->events;
	for (i = 0; i < nevent; i++) {
		evt.pos = added_events[i].pos;
		evt.type = added_events[i].type;
		evt.drawn_once = 0;

		// Discard event that are already outdated
		if (evt.pos < self->ns_total - (int)self->num_points)
			continue;

		for (insert_at = 0; insert_at < self->nevent; insert_at++) {
			if (events[insert_at].pos > evt.pos)
				break;
		}

		// Insert evt at the position found
		memmove(events+insert_at+1, events+insert_at,
		        (self->nevent - insert_at)*sizeof(*events));
		events[insert_at] = evt;
		self->nevent++;
		if (evt.pos < self->ns_total)
			self->num_displayed_event++;
	}

	g_mutex_unlock(&self->mtx);
}


LOCAL_FN
void scope_reset_events(Scope* self)
{
	if (self == NULL)
		return;

	g_mutex_lock(&self->mtx);
	self->nevent = 0;
	self->ns_total = 0;
	g_mutex_unlock(&self->mtx);
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
