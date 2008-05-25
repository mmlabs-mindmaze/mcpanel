#include <gtk/gtk.h>
#include <memory.h>
#include "scope.h"

enum {
	DUMMY_PROP,
	SCALE_PROP
};

static void scope_calculate_drawparameters(Scope* self);
static void scope_draw_samples(const Scope* self, unsigned int first, unsigned int last);
static gboolean scope_expose_event_callback(Scope *self, GdkEventExpose *event, gpointer data);
static gboolean scope_configure_event_callback(Scope *self, GdkEventConfigure *event, gpointer data);

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
	
	// Free allocted structures
	g_free(self->ticks);
	g_free(self->points);
	
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

	plot_area_set_ticks(PLOT_AREA(self), self->num_ticks, self->num_channels);

	// Connect the handled signal
	g_signal_connect_after(G_OBJECT(self), "configure_event",  
				G_CALLBACK(scope_configure_event_callback), NULL);
	g_signal_connect(G_OBJECT(self), "expose_event",  
				G_CALLBACK(scope_expose_event_callback), NULL);
}

Scope*
scope_new (void)
{
	return g_object_new(TYPE_SCOPE, NULL);
}

static gboolean
scope_configure_event_callback (Scope *self,
                                GdkEventConfigure *event,
                                gpointer data)
{
	scope_calculate_drawparameters (self);
	return TRUE;
}


static gboolean
scope_expose_event_callback (Scope *self,
                             GdkEventExpose *event,
                             gpointer data)
{
	unsigned int first, last, xmin, xmax, num_points;
	GdkPoint* points = self->points;
	
	num_points = self->num_points;
	xmin = event->area.x;
	xmax = event->area.x + event->area.width;

	if (num_points == 0)
		return TRUE;

	/* Determine which samples should redrawn */
	first=0;
	while ((first<num_points-1) && (points[first+1].x < xmin))
		first++;

	last=num_points-1;
	while ((last>0) && (points[last-1].x > xmax))
		last--;

	/* Redraw the region */
	scope_draw_samples(self, first, last);

	return TRUE;
}



static void
scope_draw_samples(const Scope* self, unsigned int first, unsigned int last)
{
	unsigned int iChannel, iSample, iColor, width, height, num_channels, nColors;
	GdkPoint* points = self->points;
	const gint* offsets = PLOT_AREA(self)->yticks;
	const gint* xticks = PLOT_AREA(self)->xticks;
	gint xmin, xmax;
	data_t scale = self->scale;
	const data_t* data = self->data;
	int i;
	const GdkColor *grid_color, *colors;
	GdkWindow* window = GTK_WIDGET(self)->window;
	GdkGC* plotgc = PLOT_AREA(self)->plotgc;

	nColors = PLOT_AREA(self)->nColors;
	colors = PLOT_AREA(self)->colors;
	grid_color = &(PLOT_AREA(self)->grid_color);
	width = GTK_WIDGET(self)->allocation.width;
	height = GTK_WIDGET(self)->allocation.height;
	num_channels = self->num_channels;	

	// Find the position of the first sample
	i = points[first].x;
	while ((first>0) && (i == points[first].x))
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
		for (iSample=first; iSample<=last; iSample++)
			points[iSample].y = offsets[iChannel] - 
				(gint)(scale*data[iSample*num_channels+iChannel]);

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
			GTK_WIDGET(self)->style->fg_gc[GTK_WIDGET_STATE(self)],
			points[self->current_pointer].x,
			0,
			points[self->current_pointer].x,
			height - 1);
}


static void
scope_calculate_drawparameters(Scope* self)
{
	guint height, width;
	unsigned int i, num_ch, num_points;
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
		xticks[i] = (num_points > self->ticks[i]) ? self->points[self->ticks[i]].x : -1;
}


void
scope_update_data(Scope* self, guint pointer)
{
	int first, last;
	GdkRectangle rect;
	//GdkWindow* window;
	GdkPoint* points;

	if (!self)
		return;

	if (pointer < self->current_pointer) {
		scope_update_data(self, self->num_points-1);
		self->current_pointer = 0;
	}

	//window = GTK_WIDGET(self)->window;
	points  = self->points;

	first = self->current_pointer;
	last = pointer;
	self->current_pointer = pointer;

	if (points == NULL)
		return;

	if (GTK_WIDGET_DRAWABLE(self)) {
		// Set the region that should be redrawn 
		rect.y = 0;
		rect.height = GTK_WIDGET(self)->allocation.height;
		rect.x = points[first].x-1;
		rect.width = points[last].x - rect.x+1;

		// Repaint
		gtk_widget_queue_draw_area(GTK_WIDGET(self),
						rect.x,
						rect.y,
						rect.width,
						rect.height);
		//gdk_window_begin_paint_rect(window, &rect);
		//scope_draw_samples(self, first, last);
		//gdk_window_end_paint(window);
	}
}

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
