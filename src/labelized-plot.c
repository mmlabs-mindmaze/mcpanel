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

LOCAL_FN GType labelized_plot_get_type(void);

G_DEFINE_TYPE (LabelizedPlot, labelized_plot, GTK_TYPE_ALIGNMENT)
#define GET_PRIVATE(o) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), TYPE_LABELIZED_PLOT, ScopePrivate))



static void
labelized_plot_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
	(void)value;

	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
labelized_plot_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
	char** labels = NULL;
	LabelizedPlot* self = LABELIZED_PLOT(object);

	switch (property_id) {
	case XTICK_LABELS:
		g_strfreev(self->xtick_labels);
		self->xtick_labels = g_strsplit(g_value_get_string(value), ";", 0);
		break;
		
	case XTICK_LABELV:
		g_strfreev(self->xtick_labels);
		labels = g_value_get_boxed(value);
		self->xtick_labels = g_strdupv(labels);
		break;
	
	case YTICK_LABELS:
		g_strfreev(self->ytick_labels);
		self->ytick_labels = g_strsplit(g_value_get_string(value), ";", 0);
		break;
		
	case YTICK_LABELV:
		g_strfreev(self->ytick_labels);
		labels = g_value_get_boxed(value);
		self->ytick_labels = g_strdupv(labels);
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
	self->xtick_labels = NULL;
	self->ytick_labels = NULL;

	gtk_alignment_set_padding(GTK_ALIGNMENT(self),3,20,30,3);
	
	/* Connect the handled signal*/
	g_signal_connect (G_OBJECT (self), "expose_event",  
                      G_CALLBACK(labelized_plot_expose_event_callback), NULL);

}


LOCAL_FN
LabelizedPlot* labelized_plot_new (void)
{
	return g_object_new(TYPE_LABELIZED_PLOT, NULL);
}


/* Specific methods */


static
gboolean labelized_plot_expose_event_callback(LabelizedPlot *self,
		                              GdkEventExpose *event,
        		                      gpointer data)
{
	(void)event;
	(void)data;

	cairo_t* cr;
	PangoLayout* layout;
	PangoContext* context;
	PangoFontDescription* desc;
	guint num_ticks, num_labels, i, ivalue, jvalue;
	gint width, height;
	gchar** labels;
	const double* offsets;
	GdkWindow* window = GTK_WIDGET(self)->window;
	GtkAllocation child_alloc;

	PlotArea* child = PLOT_AREA(gtk_bin_get_child(GTK_BIN(self)));
	if (child == NULL)
		return TRUE;
	child_alloc = GTK_WIDGET(child)->allocation;

	cr = gdk_cairo_create(window);
	cairo_set_line_width(cr, 1.0);
	cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	context = pango_cairo_create_context(cr);
	desc = pango_font_description_copy_static(pango_context_get_font_description(context));
	layout = pango_layout_new(context);
	pango_font_description_set_size(desc, 6*PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);

	// Draw vertical axis
	offsets = child->yticks;
	labels = self->ytick_labels;
	num_labels = labels ? g_strv_length(labels) : 0;
	num_ticks = child->num_yticks;
	ivalue = child_alloc.x;
	for (i=0; i<num_ticks; i++) {
		jvalue = child_alloc.y + offsets[i] ;

		// Draw tick
		cairo_move_to(cr, 0.5 + ivalue-5, 0.5 + jvalue);
		cairo_line_to(cr, 0.5 + ivalue, 0.5 + jvalue);
		cairo_stroke(cr);

		// Draw label of the channel
		if (i<num_labels) {
			pango_layout_set_text(layout, labels[i], -1);
			pango_layout_get_pixel_size(layout, &width, &height);
			cairo_move_to(cr, 0.5 + ivalue-8-width, 0.5 + jvalue-height/2);
			pango_cairo_show_layout(cr, layout);
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
		cairo_move_to(cr, 0.5 + ivalue, 0.5 + jvalue);
		cairo_line_to(cr, 0.5 + ivalue, 0.5 + jvalue+5);
		cairo_stroke(cr);

		/* Draw label of the channel */
		if (i<num_labels) {
			pango_layout_set_text (layout, labels[i] , -1);
			pango_layout_get_pixel_size(layout, &width, &height);
			cairo_move_to(cr, 0.5 + ivalue-width/2, 0.5 + jvalue+8);
			pango_cairo_show_layout(cr, layout);
		}
	}

	g_object_unref(layout);
	pango_font_description_free(desc);
	cairo_destroy(cr);

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



