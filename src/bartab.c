#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <string.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include "bargraph.h"
#include "signaltab.h"


enum bartab_widgets {
	TAB_ROOT,
	TAB_BAR1,
	TAB_BAR2,
	AXES1,
	AXES2,
	SCALE_COMBO,
	ELEC_TREEVIEW,
	NUM_BARTAB_WIDGETS
};


struct widget_name_entry {
	const char* name;
	const char* type;
};

static
const struct widget_name_entry bartab_widgets_table[] = {
	[TAB_ROOT] = {"bartab_template", "GtkWidget"},
	[TAB_BAR1] = {"bartab_bar1", "Bargraph"},
	[TAB_BAR2] = {"bartab_bar2", "Bargraph"},
	[AXES1] = {"bartab_axes1", "LabelizedPlot"},
	[AXES2] = {"bartab_axes2", "LabelizedPlot"},
	[SCALE_COMBO] = {"bartab_scale_combo", "GtkComboBox"},
	[ELEC_TREEVIEW] = {"bartab_treeview", "GtkTreeView"}
};

static
char* object_list[] = {
	"bartab_template",
	"channel_model",
	"scale_model",
	NULL
};


struct bartab {
	struct signaltab tab;
	float *data;
	unsigned int nselch, nch1;
	unsigned int* selch;
	char** labels;

	Bargraph *bar1, *bar2;
	GObject* widgets[NUM_BARTAB_WIDGETS];
};

#define get_bartab(p) \
	((struct bartab*)(((char*)(p))-offsetof(struct bartab, tab)))

static void bartab_yticks(struct bartab*, float, const char*);
/**************************************************************************
 *                                                                        *
 *                          Signal processing                             *
 *                                                                        *
 **************************************************************************/
static
void init_buffers(struct bartab* brtab)
{
	unsigned int nsel = brtab->nselch;
	unsigned int nch1 = brtab->nch1;
	g_free(brtab->data);
	brtab->data = g_malloc0(nsel*sizeof(*(brtab->data)));
	bargraph_set_data(brtab->bar1, brtab->data, nch1);
	bargraph_set_data(brtab->bar2, brtab->data+nch1, nsel-nch1);
}


/**************************************************************************
 *                                                                        *
 *                        Signal handlers                                 *
 *                                                                        *
 **************************************************************************/
static
void update_selected_label(struct bartab* brtab)
{
	unsigned int nch1 = brtab->nch1;
	unsigned int nch2 = brtab->nselch - nch1;
	char *labels1[nch1+1];
	char *labels2[nch2+1];
	unsigned int i;

	labels2[nch2] = labels1[nch1] = NULL;

	for (i=0; i<nch1; i++) 
		labels1[i] = brtab->labels[brtab->selch[i]];
	for (i=0; i<nch2; i++) 
		labels2[i] = brtab->labels[brtab->selch[i+nch1]];
	
	g_object_set(brtab->widgets[AXES1], "xtick-labelv", labels1, NULL);
	g_object_set(brtab->widgets[AXES2], "xtick-labelv", labels2, NULL);
}


static
void bartab_selch_cb(GtkTreeSelection* selec, gpointer user_data)
{
	GList *list, *elem;
	unsigned int i, j;
	struct bartab* brtab = user_data;
	unsigned int num = gtk_tree_selection_count_selected_rows(selec);
	
	g_mutex_lock(brtab->tab.datlock);

	// Prepare the channel selection structure to be passed
	if (num != brtab->nselch) {
		g_free(brtab->selch);
		brtab->selch = g_malloc(num*sizeof(*brtab->selch));
		brtab->nselch = num;
		brtab->nch1 = num/2;
		init_buffers(brtab);
	}


	// Copy the selection
	elem = list = gtk_tree_selection_get_selected_rows(selec, NULL);
	for(i=0; i<num; i++) {
		j = *gtk_tree_path_get_indices((GtkTreePath*)(elem->data));
		brtab->selch[i] = j;
		elem = g_list_next(elem);
	}
	g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(list);

	g_mutex_unlock(brtab->tab.datlock);

	update_selected_label(brtab);
}


static
void bartab_scale_changed_cb(GtkComboBox *combo, gpointer user_data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value;
	double scale;
	struct bartab* brtab = user_data;
	
	// Get selected item
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);

	// Get associated cale value
	memset(&value, 0, sizeof(value));
	gtk_tree_model_get_value(model, &iter, 1, &value);
	scale = g_value_get_double(&value);
	g_value_unset(&value);

	// Get associated label and set the ticks accordingly
	gtk_tree_model_get_value(model, &iter, 0, &value);
	bartab_yticks(brtab, scale, g_value_get_string(&value));
	g_value_unset(&value);

	// Set the scale
	g_object_set(brtab->widgets[TAB_BAR1], "min-value", -scale,
	                                       "max-value", scale, NULL);
	g_object_set(brtab->widgets[TAB_BAR2], "min-value", -scale,
	                                       "max-value", scale, NULL);
}


/**************************************************************************
 *                                                                        *
 *                      Internal helper functions                         *
 *                                                                        *
 **************************************************************************/
static
void initialize_widgets(struct bartab* brtab)
{
	GObject** widg = brtab->widgets;

	// Initialize scale combo
	gtk_combo_box_set_active(GTK_COMBO_BOX(widg[SCALE_COMBO]), 0);
	bartab_scale_changed_cb(GTK_COMBO_BOX(widg[SCALE_COMBO]), brtab);
}


static
void connect_widgets_signals(struct bartab* brtab)
{
	GtkTreeView* treeview;
	GtkTreeSelection* treeselec;
	GObject** widgets = (GObject**) brtab->widgets;

	treeview = GTK_TREE_VIEW(widgets[ELEC_TREEVIEW]);
	treeselec = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_set_mode(treeselec, GTK_SELECTION_MULTIPLE );
	g_signal_connect_after(treeselec, "changed",
	                       G_CALLBACK(bartab_selch_cb), brtab);
	g_signal_connect(widgets[SCALE_COMBO], "changed",
	                 G_CALLBACK(bartab_scale_changed_cb), brtab);
}


static
int find_widgets(struct bartab* brtab, GtkBuilder* builder)
{
	int id;
	const char* name;
	GType type;
	GObject** widgets = (GObject**) brtab->widgets;

	// Get the list of mandatory widgets and check their type;
	for (id=0; id< NUM_BARTAB_WIDGETS; id++) {
		name = bartab_widgets_table[id].name;
		type = g_type_from_name(bartab_widgets_table[id].type);

		widgets[id] = gtk_builder_get_object(builder, name);
		if (widgets[id] == NULL
		  || !g_type_is_a(G_OBJECT_TYPE(widgets[id]), type)) {
			fprintf(stderr, 
			        "Widget \"%s\" not found or "
				"is not a derived type of %s\n",
				name, bartab_widgets_table[id].type);
			return -1;
		}
	}

	brtab->bar1 = BARGRAPH(brtab->widgets[TAB_BAR1]);
	brtab->bar2 = BARGRAPH(brtab->widgets[TAB_BAR2]);
	brtab->tab.widget = GTK_WIDGET(brtab->widgets[TAB_ROOT]);
	brtab->tab.scale_combo = GTK_COMBO_BOX(brtab->widgets[SCALE_COMBO]);
	return 0;
}


static
void fill_treeview(GtkTreeView* treeview, const char** labels)
{
	GtkListStore* list;
	unsigned int i = 0;
	GtkTreeIter iter;
	GtkTreeSelection* selec;

	list = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
	gtk_list_store_clear(list);

	while (labels[i] != NULL) {
		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0, labels[i++], -1);
	}

	// Select initially all items
	selec = gtk_tree_view_get_selection(treeview);
	gtk_tree_selection_select_all(selec);
}


static
void bartab_yticks(struct bartab* brtab, float scale, const char* label)
{
	float dscale, incscale, inc, ticks[16];
	char unit[16], labels[16][32], *ticklabels[17] = {NULL};
	int i, nticks;
	GObject** widg = brtab->widgets;

	sscanf(label, "%f %s", &dscale, unit);
	if (dscale <= 0.0)
		return;
	incscale = dscale;

	// Align ticks to a rounded value
	while (incscale < 2.5)
		incscale *= 10.0;
	while (incscale > 25.0)
		incscale /= 10.0;
	inc = 1.0;
	if (incscale > 5.0)
		inc = 2.0;
	if (incscale > 10.0)
		inc = 5.0;
	inc *= dscale/incscale;
	
	// set the ticks and ticks labels
	nticks = 2*(int)(dscale / inc)+1;
	for (i=0; i<nticks; i++) {
		ticks[i] =  (i-(nticks-1)/2)*inc;
		ticklabels[i] = labels[i];
		sprintf(ticklabels[i], "%.1f%s", ticks[i], unit);
		ticks[i] *= scale / dscale;
	}

	// Set the ticks to the scope widgets
	bargraph_set_ticks(brtab->bar1, nticks, ticks);
	bargraph_set_ticks(brtab->bar2, nticks, ticks);
	g_object_set(widg[AXES1], "ytick-labelv", ticklabels, NULL);
	g_object_set(widg[AXES2], "ytick-labelv", ticklabels, NULL);
}


/**************************************************************************
 *                                                                        *
 *                                                                        *
 *                                                                        *
 **************************************************************************/
static
void bartab_destroy(struct signaltab* tab)
{
	struct bartab* brtab = get_bartab(tab);

	g_strfreev(brtab->labels);
	g_free(brtab->data);
	g_free(brtab);
}


static
void bartab_define_input(struct signaltab* tab, const char** labels)
{
	struct bartab* brtab = get_bartab(tab);
	
	g_strfreev(brtab->labels);
	brtab->labels = g_strdupv((char**)labels);

	g_mutex_unlock(brtab->tab.datlock);
	fill_treeview(GTK_TREE_VIEW(brtab->widgets[ELEC_TREEVIEW]), labels);
	g_mutex_lock(brtab->tab.datlock);

	init_buffers(brtab);
}


static
void bartab_process_data(struct signaltab* tab, unsigned int ns,
                           const float* in)
{
	struct bartab* brtab = get_bartab(tab);
	unsigned int j;
	unsigned int *sel = brtab->selch;
	unsigned int nch = brtab->nselch;
	unsigned int nmax_ch = brtab->tab.nch;

	// Copy data of the selected channels
	for (j=0; j<nch; j++) 
		brtab->data[j] = in[(ns-1)*nmax_ch + sel[j]];
}


static
void bartab_update_plot(struct signaltab* tab)
{
	struct bartab* brtab = get_bartab(tab);

	bargraph_update_data(brtab->bar1, 0);
	bargraph_update_data(brtab->bar2, 0);
}


LOCAL_FN 
struct signaltab* create_tab_bargraph(const char* uidef,
		    int nscales, const char** sclabels, const float* scales)
{
	struct bartab* brtab = NULL;
	GtkBuilder* builder;
	unsigned int res;
	GError* error = NULL;

	brtab = g_malloc0(sizeof(*brtab));

	// Build the tab widget according to the ui definition files
	builder = gtk_builder_new();
	res = gtk_builder_add_objects_from_string(builder, uidef, -1,
	                                          object_list, &error);
	if (!res) {
		fprintf(stderr, "%s\n", error->message);
		goto error;
	}
	brtab->tab.scale = 1;

	// Initialize the struture with the builded widget
	if (find_widgets(brtab, builder))
		goto error;
	initialize_signaltab(&(brtab->tab), nscales, sclabels, scales);
	initialize_widgets(brtab);
	connect_widgets_signals(brtab);

	// Destroy the builder
	g_object_ref(brtab->tab.widget);
	g_object_unref(builder);
	
	// Initialilize the parent class
	brtab->tab.destroy = bartab_destroy;
	brtab->tab.define_input = bartab_define_input;
	brtab->tab.process_data = bartab_process_data;
	brtab->tab.update_plot = bartab_update_plot;
	brtab->tab.set_wndlen = NULL;
	return &(brtab->tab);

error:
	free(brtab);
	g_object_unref(builder);
	return NULL;
}


