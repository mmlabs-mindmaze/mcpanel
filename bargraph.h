#ifndef _BARGRAPH
#define _BARGRAPH

#include <glib-object.h>
#include "plot-area.h"
#include "plottk-types.h"

G_BEGIN_DECLS

#define TYPE_BARGRAPH bargraph_get_type()

#define BARGRAPH(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_BARGRAPH, Bargraph))

#define BARGRAPH_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_BARGRAPH, BargraphClass))

#define IS_BARGRAPH(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_BARGRAPH))

#define IS_BARGRAPH_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_BARGRAPH))

#define BARGRAPH_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_BARGRAPH, BargraphClass))

typedef struct {
	PlotArea parent;

	guint num_channels;
	guint num_ticks;
	data_t* tick_values;
	gint offset;
	data_t scale;
	data_t min;
	data_t max;
	gfloat bar_ratio;
	guint current_pointer;
	const data_t* data;
} Bargraph;

typedef struct {
	PlotAreaClass parent_class;
} BargraphClass;

GType bargraph_get_type (void);

Bargraph* bargraph_new (void);
void bargraph_update_data(Bargraph* self, guint pointer);
void bargraph_set_data(Bargraph* self, data_t* data, guint num_ch);
void bargraph_set_ticks(Bargraph* self, guint num_ticks, data_t* tick_values);

G_END_DECLS

#endif /* _BARGRAPH */

