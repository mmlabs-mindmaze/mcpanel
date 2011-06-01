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
#ifndef __LABELIZED_PLOT_H__
#define __LABELIZED_PLOT_H__


#include <glib.h>
#include <glib-object.h>
#include <gtk/gtkalignment.h>


G_BEGIN_DECLS

#define TYPE_LABELIZED_PLOT		(labelized_plot_get_type ())

#define LABELIZED_PLOT(obj)\
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_LABELIZED_PLOT, LabelizedPlot))

#define LABELIZED_PLOT_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_LABELIZED_PLOT, LabelizedPlotClass))

#define IS_LABELIZED_PLOT(obj)\
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_LABELIZED_PLOT))

#define IS_LABELIZED_PLOT_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_LABELIZED_PLOT))


typedef struct _LabelizedPlot       LabelizedPlot;
typedef struct _LabelizedPlotClass  LabelizedPlotClass;

struct _LabelizedPlot
{
	GtkAlignment parent;
	gchar** xtick_labels;
	gchar** ytick_labels;
	PangoFontDescription* tick_font_desc;
	PangoFontDescription* label_font_desc;
};

struct _LabelizedPlotClass
{
	GtkAlignmentClass parent_class;
};

GType		labelized_plot_get_type(void);
LabelizedPlot*	labelized_plot_new(void);

G_END_DECLS


#endif /* __LABELIZED_PLOT_H__ */
