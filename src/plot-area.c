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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <memory.h>
#include "plot-area.h"

enum {
	PROP_0,
	GRID_COLOR,
	CHANNEL_COLOR,
	BACKGROUND_COLOR
};

static void 	plot_area_realize_callback(PlotArea* self);
static void 	plot_area_unrealize_callback(PlotArea* self);
static void plot_area_set_color(PlotArea* self, const gchar* colorstr, GdkColor* color);
static void plot_area_set_channel_colors(PlotArea* self, const gchar* colorstr);


LOCAL_FN GType plot_area_get_type(void);
G_DEFINE_TYPE (PlotArea, plot_area, GTK_TYPE_DRAWING_AREA)


static
void plot_area_get_property(GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
	(void)value;

	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}


static
void plot_area_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
	GdkColor color;
	PlotArea* self = PLOT_AREA(object); 

	switch (property_id) {
	case GRID_COLOR:
		plot_area_set_color(self, g_value_get_string(value), &(self->grid_color));
		break;

	case BACKGROUND_COLOR:
		plot_area_set_color(self, g_value_get_string(value), &color);
		gtk_widget_modify_bg ( GTK_WIDGET(self), GTK_STATE_NORMAL, &color);
		break;

	case CHANNEL_COLOR:
		plot_area_set_channel_colors(self, g_value_get_string(value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}


static
void plot_area_finalize (GObject *object)
{
	PlotArea* self = PLOT_AREA(object);

	g_free(self->colors);
	g_free(self->xticks);
	g_free(self->yticks);
	self->xticks = self->yticks = NULL;

	if (G_OBJECT_CLASS (plot_area_parent_class)->finalize)
		G_OBJECT_CLASS (plot_area_parent_class)->finalize (object);
}


static
void plot_area_class_init (PlotAreaClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = plot_area_get_property;
	object_class->set_property = plot_area_set_property;
	object_class->finalize = plot_area_finalize;

	g_object_class_install_property(G_OBJECT_CLASS(klass),
					GRID_COLOR,
					g_param_spec_string("grid-color",
							"grid color",
							"The color used to plot the grid",
							"grey",
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(G_OBJECT_CLASS(klass),
					BACKGROUND_COLOR,
					g_param_spec_string("background",
							"background color",
							"The background color",
							"#FFFFFF",
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));
					
	g_object_class_install_property(G_OBJECT_CLASS(klass),
					CHANNEL_COLOR,
					g_param_spec_string("channel-colors",
							"Channel colors",
							"The colors used to display each channel data separated by ';' "
									"(wrap on colors if fewer than channels)",
							"blue",
							G_PARAM_WRITABLE |
							G_PARAM_STATIC_STRINGS));

/*	g_object_class_install_property(G_OBJECT_CLASS(klass),
					CHANNEL_COLOR,
					g_param_spec_value_array("channel-colors",
							"Channel colors",
							"The colors used to display each channel data (wrap on colors if fewer than channels)",
							g_param_spec_boxed("single-channel-color",
									NULL,NULL,
									GDK_TYPE_COLOR,
									G_PARAM_WRITABLE |
									G_PARAM_STATIC_STRINGS),
							G_PARAM_WRITABLE | 
							G_PARAM_STATIC_STRINGS));
*/
}


static
void plot_area_init (PlotArea *self)
{
	unsigned int i;
	self->nColors = 1;
	self->colors = g_malloc(self->nColors*sizeof(GdkColor));
	self->num_xticks = self->num_yticks = 0;
	self->xticks = self->yticks = NULL;

	// Assign default colors
	gdk_color_parse("gray67", &(self->grid_color));
	for (i=0; i<self->nColors; i++)
		gdk_color_parse("blue1", self->colors + i);
	
	self->plotgc = NULL;
	
	/* Connect the handled signal*/
	g_signal_connect_after (G_OBJECT (self), "realize",  
	                        G_CALLBACK (plot_area_realize_callback), NULL);
	g_signal_connect (G_OBJECT (self), "unrealize",  
	                        G_CALLBACK (plot_area_unrealize_callback), NULL);
}


static
void plot_area_realize_callback(PlotArea* self)
{
	unsigned int i;
	GdkColormap* colormap = gtk_widget_get_colormap(GTK_WIDGET(self));

	/* Allocate et intialize the graphical context used for the plot */
	self->plotgc = gdk_gc_new(GTK_WIDGET(self)->window);
	gdk_gc_copy(self->plotgc, GTK_WIDGET(self)->style->fg_gc[GTK_STATE_NORMAL]);

	/* allocate plot color */
	gdk_colormap_alloc_color( colormap, &(self->grid_color), FALSE, TRUE);
	for(i=0; i<self->nColors; i++)
		gdk_colormap_alloc_color( colormap, self->colors+i, FALSE, TRUE);
}


static
void plot_area_unrealize_callback(PlotArea* self)
{
	GdkColormap* colormap = gtk_widget_get_colormap(GTK_WIDGET(self));

	gdk_colormap_free_colors( colormap, self->colors, self->nColors);
	gdk_colormap_free_colors( colormap, &(self->grid_color), 1);

	g_object_unref( G_OBJECT(self->plotgc) );
}


LOCAL_FN
PlotArea* plot_area_new (void)
{
	return g_object_new (TYPE_PLOT_AREA, NULL);
}


static
void plot_area_set_color(PlotArea* self, const gchar* colorstr, GdkColor* color)
{
	GdkColormap* colormap = NULL;
	if (GTK_WIDGET_REALIZED(self)) {
		colormap = gtk_widget_get_colormap(GTK_WIDGET(self));
		gdk_colormap_free_colors(colormap, color, 1);
	}

	gdk_color_parse(colorstr, color);

	if (GTK_WIDGET_REALIZED(self))
		gdk_colormap_alloc_color(colormap, color, FALSE, TRUE);
}


static
void plot_area_set_channel_colors(PlotArea* self, const gchar* colorstr)
{
	GdkColormap* colormap = NULL;
	gchar** splitstr;
	unsigned int i;
	GdkColor default_color = {0, 0, 0, 0xFFFF}; // blue

	if (GTK_WIDGET_REALIZED(self)) {
		colormap = gtk_widget_get_colormap(GTK_WIDGET(self));
		gdk_colormap_free_colors(colormap, self->colors, self->nColors);
	}
	g_free(self->colors);

	splitstr = g_strsplit(colorstr, ";", 0);
	self->nColors = g_strv_length(splitstr);
	self->colors = g_malloc0(self->nColors*sizeof(*(self->colors)));
	for (i=0; i<self->nColors; i++) {
		if (!gdk_color_parse(splitstr[i], self->colors+i))
			self->colors[i] = default_color;
	}
	g_strfreev(splitstr);


	if (GTK_WIDGET_REALIZED(self)) {
		for (i=0; i<self->nColors; i++)
			gdk_colormap_alloc_color(colormap, self->colors+i, FALSE, TRUE);
	}	
}


LOCAL_FN
void plot_area_set_ticks(PlotArea* self, guint num_xticks, guint num_yticks)
{
	if (num_xticks != self->num_xticks) {
		g_free(self->xticks);
		self->num_xticks = num_xticks;
		self->xticks = g_malloc(self->num_xticks * sizeof(*(self->xticks)));
	}

	if (num_yticks != self->num_yticks) {
		g_free(self->yticks);
		self->num_yticks = num_yticks;
		self->yticks = g_malloc(self->num_yticks * sizeof(*(self->yticks)));
	}
}
