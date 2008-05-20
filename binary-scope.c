#include <gtk/gtk.h>
#include "binary-scope.h"
#include <memory.h>

static void binary_scope_calculate_drawparameters(const BinaryScope* self);
static void binary_scope_draw_samples(const BinaryScope* self, unsigned int first, unsigned int last);
static gboolean binary_scope_expose_event_callback(BinaryScope *self, GdkEventExpose *event, gpointer data);
static gboolean binary_scope_configure_event_callback(BinaryScope *self, GdkEventConfigure *event, gpointer data);

G_DEFINE_TYPE (BinaryScope, binary_scope, TYPE_PLOT_AREA)
#define GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), TYPE_BINARY_SCOPE, BinaryScopePrivate))



static void
binary_scope_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
binary_scope_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
binary_scope_finalize (GObject *object)
{
	BinaryScope* self = BINARY_SCOPE(object);
	
	// Free allocted structures
	g_free(self->ticks);
	g_free(self->offsets);
	g_free(self->xcoords);
	
	// Call parent finalize function
	if (G_OBJECT_CLASS(binary_scope_parent_class)->finalize)
		G_OBJECT_CLASS(binary_scope_parent_class)->finalize(object);
}


static void
binary_scope_class_init (BinaryScopeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = binary_scope_get_property;
	object_class->set_property = binary_scope_set_property;
	object_class->finalize = binary_scope_finalize;
}

static void
binary_scope_init (BinaryScope *self)
{
	// Initialize members
	self->num_channels = 0;
	self->num_ticks = 0;
	self->current_pointer = 0; 
	self->data = NULL;
	self->ticks = NULL;
	self->offsets = g_malloc((self->num_channels+1)*sizeof(gint));
	self->xcoords = NULL;

	plot_area_set_ticks(PLOT_AREA(self), self->num_ticks, 0);

	// Connect the handled signal
	g_signal_connect_after(G_OBJECT(self), "configure_event",  
				G_CALLBACK(binary_scope_configure_event_callback), NULL);
	g_signal_connect(G_OBJECT(self), "expose_event",  
				G_CALLBACK(binary_scope_expose_event_callback), NULL);
}

BinaryScope*
binary_scope_new (void)
{
	return g_object_new(TYPE_BINARY_SCOPE, NULL);
}

static gboolean
binary_scope_configure_event_callback (BinaryScope *self,
                                GdkEventConfigure *event,
                                gpointer data)
{
	binary_scope_calculate_drawparameters (self);
	return TRUE;
}


static gboolean
binary_scope_expose_event_callback (BinaryScope *self,
                             GdkEventExpose *event,
                             gpointer data)
{
	unsigned int first, last, xmin, xmax, num_points;
	gint* xcoords = self->xcoords;
	
	num_points = self->num_points;
	xmin = event->area.x;
	xmax = event->area.x + event->area.width;

	if (num_points == 0)
		return TRUE;

	/* Determine which samples should redrawn */
	first=0;
	while ((first<num_points-1) && (xcoords[first+1] < xmin))
		first++;

	last=num_points-1;
	while ((last>0) && (xcoords[last-1] > xmax))
		last--;

	/* Redraw the region */
	binary_scope_draw_samples(self, first, last);

	return TRUE;
}



static void
binary_scope_draw_samples(const BinaryScope* self, unsigned int first, unsigned int last)
{
	unsigned int i, iChannel, iSample, iColor;
	gint xmin, xmax;
	guint32 channelMask;
	int bScanning;
	GdkGC* plotgc = PLOT_AREA(self)->plotgc;
	GdkGC* stategc = GTK_WIDGET(self)->style->fg_gc[GTK_WIDGET_STATE (self)];
	GdkWindow* window = GTK_WIDGET(self)->window;

	const GdkColor* grid_color = &(PLOT_AREA(self)->grid_color);
	const GdkColor* colors = PLOT_AREA(self)->colors;
	const gint* offsets = self->offsets; 
	const guint32* values = self->data; 
	const gint* xcoords = self->xcoords;
	const guint nColors = PLOT_AREA(self)->nColors;
	const gint* xticks = PLOT_AREA(self)->xticks;
	const guint height = GTK_WIDGET(self)->allocation.height;

	xmin = xcoords[first];
	xmax = xcoords[last];
	
	// draw grid
	gdk_gc_set_foreground(plotgc, grid_color);
	for (i=0; i<self->num_ticks; i++) {
		if ((xticks[i]>=xmin) && (xticks[i]<=xmax))
			gdk_draw_line(window,
					plotgc,
					xticks[i],
					0,
					xticks[i],
					height);
	}


	// Draw the channels data
	for (iChannel=0; iChannel<self->num_channels; iChannel++) {
		channelMask = 0x00000001 << iChannel;
		iColor = iChannel % nColors;
		gdk_gc_set_foreground(plotgc, colors+iColor);

		bScanning = (values[i] & channelMask) ? 1 : 0;
		iSample = first;
		for (i=first; i<=last; i++) {
			if ((values[i] & channelMask) && !bScanning) {
				iSample = i;
				bScanning = 1;
			}
			if ((!(values[i] & channelMask) || (i==last)) && bScanning) {
				bScanning = FALSE;
				gdk_draw_rectangle (window,
		    		            	plotgc,
						TRUE,
						xcoords[iSample],
						offsets[iChannel],
						xcoords[i]-xcoords[iSample],
						offsets[iChannel+1]-offsets[iChannel]);
			}
		}
	}

	// Draw the scanline
	gdk_draw_line(window,
	               stategc,
	               xcoords[self->current_pointer],
	               0,
	               xcoords[self->current_pointer],
	               height - 1);
}


static void
binary_scope_calculate_drawparameters(const BinaryScope* self)
{
	guint height, width;
	unsigned int i, num_ch, num_points;
	gint* xticks = PLOT_AREA(self)->xticks;

	num_ch = self->num_channels;
	num_points = self->num_points;

	width = GTK_WIDGET(self)->allocation.width;
	height = GTK_WIDGET(self)->allocation.height;
	
	// Calculate y offsets
	for (i=0; i<=num_ch; i++)
		self->offsets[i] = (gint)((float)(height*i) / (float)(num_ch));

	// Calculate x coordinates
	for (i=0; i<num_points; i++)
		self->xcoords[i] = (gint)( ((float)(i*width))/(float)(num_points-1) );
	
	// Set the ticks position
	for (i=0; i<self->num_ticks; i++)
		xticks[i] = (num_points > self->ticks[i]) ? self->xcoords[self->ticks[i]] : -1;
}


void
binary_scope_update_data(BinaryScope* self, guint pointer)
{
	int first, last;
	GdkRectangle rect;
//	GdkWindow* window;
//	GdkPoint* points;

	if (!self)
		return;

	if (pointer < self->current_pointer) {
		binary_scope_update_data(self, self->num_points-1);
		self->current_pointer = 0;
	}

//	window = GTK_WIDGET(self)->window;
	gint* xcoords  = self->xcoords;

	first = self->current_pointer;
	last = pointer;
	self->current_pointer = pointer;

//	if (points == NULL)
//		return;

	if (GTK_WIDGET_DRAWABLE(self)) {
		// Set the region that should be redrawn 
		rect.y = 0;
		rect.height = GTK_WIDGET(self)->allocation.height;
		rect.x = xcoords[first];
		rect.width = xcoords[last] - rect.x + 1;

		// Repaint
		gtk_widget_queue_draw_area(GTK_WIDGET(self),
						rect.x,
						rect.y,
						rect.width,
						rect.height);
		//gdk_window_begin_paint_rect(window, &rect);
		//binary_scope_draw_samples(self, first, last);
		//gdk_window_end_paint(window);
	}
}

void binary_scope_set_data(BinaryScope* self, guint32* data, guint num_points, guint num_ch)
{
	int has_changed = 0;

	if (!self)
		return;

	self->data = data;
	self->current_pointer = 0;
	
	
	if (num_points != self->num_points) {
		g_free(self->xcoords);
		self->xcoords = g_malloc0(num_points*sizeof(gint));
		self->num_points = num_points;
		has_changed = 1;
	}

	if (num_ch != self->num_channels) {
		g_free(self->offsets);
		self->offsets = g_malloc((num_ch+1)*sizeof(gint));
		self->num_channels = num_ch;
		has_changed = 1;
	}

	if (has_changed)
		binary_scope_calculate_drawparameters(self);

	if (GTK_WIDGET_DRAWABLE(self))
		gtk_widget_queue_draw(GTK_WIDGET(self));
}


void binary_scope_set_ticks(BinaryScope* self, guint num_ticks, guint* ticks)
{
	if (num_ticks != self->num_ticks) {
		g_free(self->ticks);
		self->ticks = g_malloc(num_ticks*sizeof(*(self->ticks)));
		self->num_ticks = num_ticks;
		plot_area_set_ticks(PLOT_AREA(self), self->num_ticks, self->num_channels);
	}

	memcpy(self->ticks, ticks, num_ticks*sizeof(*(self->ticks)));	
	binary_scope_calculate_drawparameters(self);
}
