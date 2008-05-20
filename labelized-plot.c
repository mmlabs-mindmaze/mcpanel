#include <gtk/gtk.h>
#include "labelized-plot.h"
#include "plot-area.h"

enum {
	DUMMY_PROP,
	XTICK_LABELS,
	XTICK_LABELV,
	YTICK_LABELS,
	YTICK_LABELV,
};

static gboolean labelized_plot_expose_event_callback(LabelizedPlot *self, GdkEventExpose *event, gpointer data);

G_DEFINE_TYPE (LabelizedPlot, labelized_plot, GTK_TYPE_ALIGNMENT)
#define GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), TYPE_LABELIZED_PLOT, ScopePrivate))



static void
labelized_plot_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
labelized_plot_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
	LabelizedPlot* self = LABELIZED_PLOT(object);

	switch (property_id) {
	case XTICK_LABELS:
		g_strfreev(self->xtick_labels);
		self->xtick_labels = g_strsplit(g_value_get_string(value), ";", 0);
		break;
		
	case XTICK_LABELV:
		g_strfreev(self->xtick_labels);
		self->xtick_labels = g_strdupv(g_value_get_boxed(value));
		break;
	
	case YTICK_LABELS:
		g_strfreev(self->ytick_labels);
		self->ytick_labels = g_strsplit(g_value_get_string(value), ";", 0);
		break;
		
	case YTICK_LABELV:
		g_strfreev(self->ytick_labels);
		self->ytick_labels = g_strdupv(g_value_get_boxed(value));
		break;
	
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}

	if (GTK_WIDGET_DRAWABLE(self))
		gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void
labelized_plot_finalize (GObject *object)
{
	LabelizedPlot* self = LABELIZED_PLOT(object);
	g_strfreev(self->xtick_labels);
	g_strfreev(self->ytick_labels);
	pango_font_description_free(self->tick_font_desc);
	pango_font_description_free(self->label_font_desc);
	
	// Call parent finalize function
	if (G_OBJECT_CLASS(labelized_plot_parent_class)->finalize)
		G_OBJECT_CLASS(labelized_plot_parent_class)->finalize(object);
}

static void
labelized_plot_class_init (LabelizedPlotClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = labelized_plot_get_property;
	object_class->set_property = labelized_plot_set_property;
	object_class->finalize = labelized_plot_finalize;


	g_object_class_install_property(G_OBJECT_CLASS(klass),
					XTICK_LABELS,
					g_param_spec_string("xtick-labels",
							"Labels of the horizontal ticks",
							"The labels that should be displayed near each horizontal tick (separated by ';')",
							NULL,
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(G_OBJECT_CLASS(klass),
					XTICK_LABELV,
					g_param_spec_boxed("xtick-labelv",
							"Labels of the horizontal ticks",
							"The labels that should be displayed near each horizontal tick",
							G_TYPE_STRV,
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(G_OBJECT_CLASS(klass),
					YTICK_LABELS,
					g_param_spec_string("ytick-labels",
							"Labels of the vertical ticks",
							"The labels that should be displayed near each vertical tick (separated by ';')",
							NULL,
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(G_OBJECT_CLASS(klass),
					YTICK_LABELV,
					g_param_spec_boxed("ytick-labelv",
							"Labels of the horizontal ticks",
							"The labels that should be displayed near each vertical tick",
							G_TYPE_STRV,
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));
}


static void
labelized_plot_init (LabelizedPlot* self)
{
	PangoContext* p_context;

	self->xtick_labels = NULL;
	self->ytick_labels = NULL;

	gtk_alignment_set_padding(GTK_ALIGNMENT(self),1,20,30,1);
	
	/* Connect the handled signal*/
	g_signal_connect (G_OBJECT (self), "expose_event",  
                      G_CALLBACK(labelized_plot_expose_event_callback), NULL);

	// Set font description
	p_context =  gtk_widget_get_pango_context( GTK_WIDGET(self) );
	self->tick_font_desc = pango_font_description_copy_static(pango_context_get_font_description(p_context));
	self->label_font_desc = pango_font_description_copy_static(pango_context_get_font_description(p_context));
	pango_font_description_set_size(self->tick_font_desc, 6*PANGO_SCALE);
	pango_font_description_set_size(self->label_font_desc, 8*PANGO_SCALE);
}


LabelizedPlot*
labelized_plot_new (void)
{
	return g_object_new(TYPE_LABELIZED_PLOT, NULL);
}


/* Specific methods */


static gboolean
labelized_plot_expose_event_callback (LabelizedPlot *self,
		                              GdkEventExpose *event,
        		                      gpointer data)
{
	PangoLayout* layout;
	PangoContext* context;
	guint num_ticks, num_labels, i, ivalue, jvalue;
	gint width, height;
	gchar** labels;
	const gint* offsets;
	GdkGC* gc = GTK_WIDGET(self)->style->fg_gc[GTK_WIDGET_STATE(self)];
	GdkWindow* window = GTK_WIDGET(self)->window;
	GtkAllocation child_alloc;

	PlotArea* child = PLOT_AREA(gtk_bin_get_child(GTK_BIN(self)));
	if (child == NULL)
		return TRUE;

	child_alloc = GTK_WIDGET(child)->allocation;
	context = gtk_widget_get_pango_context(GTK_WIDGET(self));
	layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, self->tick_font_desc);

	// Draw vertical axis
	offsets = child->yticks;
	labels = self->ytick_labels;
	num_labels = labels ? g_strv_length(labels) : 0;
	num_ticks = child->num_yticks;
	ivalue = child_alloc.x;
	for (i=0; i<num_ticks; i++) {
		jvalue = child_alloc.y + offsets[i] ;

		// Draw tick
		gdk_draw_line(window, gc, ivalue-5, jvalue, ivalue, jvalue);

		// Draw label of the channel
		if (i<num_labels) {
			pango_layout_set_text(layout, labels[i], -1);
			pango_layout_get_pixel_size(layout, &width, &height);
			gdk_draw_layout(window, gc, ivalue-8-width, jvalue-height/2, layout);
		}
	}

	// Draw horizontal axis
	offsets = child->xticks;
	labels = self->xtick_labels;
	num_labels = labels ? g_strv_length(labels) : 0;
	jvalue = child_alloc.y + child_alloc.height;
	num_ticks = child->num_xticks;
	for (i=0; i<num_ticks; i++) {
		ivalue = child_alloc.x + offsets[i];

		// Draw tick
		gdk_draw_line(window, gc, ivalue, jvalue, ivalue, jvalue+5);

		/* Draw label of the channel */
		if (i<num_labels) {
			pango_layout_set_text (layout, labels[i] , -1);
			pango_layout_get_pixel_size(layout, &width, &height);
			gdk_draw_layout(window, gc, ivalue-width/2, jvalue+8, layout);
		}
	}

	g_object_unref(layout);

	gtk_paint_shadow (GTK_WIDGET(self)->style,
					  window,
					  GTK_WIDGET_STATE(self),
					  GTK_SHADOW_ETCHED_IN,
					  NULL,
					  GTK_WIDGET(self),
					  NULL,
					  child_alloc.x-1,
					  child_alloc.y-1,
					  child_alloc.width+2,
					  child_alloc.height+2);

	return TRUE;
}



