#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include "eegpanel.h"
#include "plot-area.h"
#include "scope.h"
#include "bargraph.h"
#include "binary-scope.h"
#include "labelized-plot.h"
#include "gtk-led.h"
#include "filter.h"
#include <memory.h>

#define GET_PANEL_FROM(widget)  (EEGPanel*)(g_object_get_data(G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))), "eeg_panel"))


struct _EEGPanelPrivateData {
	GtkWindow* window;
	Scope* eeg_scope;
	Scope* exg_scope;
	BinaryScope* tri_scope;
	Bargraph* eeg_bargraph;
	LabelizedPlot *eeg_axes, *exg_axes, *tri_axes, *bar_axes;

	GtkTreeView* eeg_treeview;
	GtkComboBox *reftype_combo, *elecref_combo, *electrodesets_combo;

	GThread* main_loop_thread;

	//
	unsigned int sampling_rate, nmax_eeg, nmax_exg, nlines_tri;
	unsigned int num_eeg_channels, num_exg_channels;
	unsigned int num_samples;
	unsigned int display_length;
	unsigned int current_sample;

	// Labels
	char** eeg_labels;
	char** exg_labels;

	// data
	unsigned int *selected_eeg, *selected_exg;
	float *eeg, *exg;
	uint32_t *triggers;
};

int set_data_input(EEGPanel* panel, int num_samples, ChannelSelection* eeg_selec, ChannelSelection* exg_selec);
int fill_selec_from_treeselec(ChannelSelection* selection, GtkTreeSelection* tree_sel, char** labels);
int poll_widgets(EEGPanel* panel, GtkBuilder* builder);
int initialize_widgets(EEGPanel* panel, GtkBuilder* builder);
GtkListStore* initialize_list_treeview(GtkTreeView* treeview, const gchar* attr_name);
void initialize_combo(GtkComboBox* combo, const char* labels);
void fill_treeview(GtkTreeView* treeview, unsigned int num_labels, const char** labels);
void fill_combo(GtkComboBox* combo, const char* labels);
char** add_default_labels(char** labels, unsigned int requested_num_labels, const char* prefix);

gpointer loop_thread(gpointer user_data)
{
	gtk_main();
	return 0;
}
///////////////////////////////////////////////////
//
//	Signal handlers
//
///////////////////////////////////////////////////
extern gboolean startacquisition_button_toggled_cb(GtkButton* button, gpointer data)
{
	EEGPanel* panel = GET_PANEL_FROM(button);
	int state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) ? 1 : 0;

	if (panel->system_connection) {
		if (panel->system_connection(state, panel->user_data)) {
			
		}
	}
		

	return TRUE;
}

extern void reftype_combo_changed_cb(GtkComboBox* combo, gpointer data)
{
	EEGPanel* panel = GET_PANEL_FROM(combo);
	int selec = gtk_combo_box_get_active(combo);

	gtk_widget_set_sensitive(GTK_WIDGET(panel->priv->elecref_combo), (selec==2)? TRUE : FALSE);
}

extern void channel_selection_changed_cb(GtkTreeSelection* selection, gpointer user_data)
{
	ChannelSelection select;
	ChannelType type = (ChannelType)user_data;
	EEGPanel* panel = GET_PANEL_FROM(gtk_tree_selection_get_tree_view(selection));
	char** labels = (type == EEG) ? panel->priv->eeg_labels : panel->priv->exg_labels;
	
	// Prepare the channel selection structure to be passed
	fill_selec_from_treeselec(&select, selection, labels);

	
	if (panel->process_selection) {
		if (panel->process_selection(&select, type, panel->user_data) > 0) {
			if (type == EEG)
				set_data_input(panel, -1, &select, NULL);
			else
				set_data_input(panel, -1, NULL, &select);
		}
	}

	// free everything holded by the selection struct
	g_free(select.selection);
	g_free(select.labels);
}



/*extern void destroy( GtkWidget *widget,
                     gpointer   data )
{
    printf("Salut!\n");
    gtk_main_quit ();
}*/


int RegisterCustomDefinition(void)
{
	volatile GType type;

	type = TYPE_PLOT_AREA;
	type = TYPE_SCOPE;
	type = TYPE_BINARY_SCOPE;
	type = TYPE_BARGRAPH;
	type = TYPE_LABELIZED_PLOT;
	type = GTK_TYPE_LED;

	return type;
}

EEGPanel* eegpanel_create(void)
{
	GtkBuilder* builder;
	EEGPanel* panel = NULL;
	EEGPanelPrivateData* priv = NULL;
	GError* error;

	RegisterCustomDefinition();

	// Allocate memory for the structures
	panel = g_malloc0(sizeof(*panel));
	priv = g_malloc0(sizeof(*priv));
	panel->priv = priv;


	// Create the panel widgets according to the ui definition files
	builder = gtk_builder_new();
	gtk_builder_add_from_file(builder, "eegpanel.ui", &error);
	gtk_builder_connect_signals(builder, panel);

	// Get the pointers of the control widgets
	poll_widgets(panel, builder);

	
	// Needed initializations
	priv->display_length = 1;

	// Initialize the content of the widgets
	initialize_widgets(panel, builder);
	eegpanel_define_input(panel, 128, 8, 16, 2048);

	g_object_unref(builder);

	return panel;
}

void eegpanel_show(EEGPanel* panel, int state)
{
	EEGPanelPrivateData* priv = panel->priv;
	int lock_res = 0;

	if ((priv->main_loop_thread) && (priv->main_loop_thread != g_thread_self()))
		lock_res = 1;
		
	if (lock_res)
		gdk_threads_enter();

	if (state)
		gtk_widget_show_all(GTK_WIDGET(priv->window));
	else
		gtk_widget_hide_all(GTK_WIDGET(priv->window));

	if (lock_res)
		gdk_threads_leave();
}



void eegpanel_run(EEGPanel* panel, int nonblocking)
{
	EEGPanelPrivateData* priv = panel->priv;

	if (!nonblocking) {
		priv->main_loop_thread = g_thread_self();
		gtk_main();
		return;
	}
	
	priv->main_loop_thread = g_thread_create(loop_thread, NULL, TRUE, NULL);
	return;
}


void eegpanel_destroy(EEGPanel* panel)
{
	EEGPanelPrivateData* priv = panel->priv;

	// If called from another thread than the one of the main loop
	// wait for the termination of the main loop
	if ((priv->main_loop_thread) && (priv->main_loop_thread != g_thread_self()))
		g_thread_join(priv->main_loop_thread);

	g_strfreev(priv->eeg_labels);
	g_strfreev(priv->exg_labels);
	
	g_free(priv->eeg);
	g_free(priv->exg);
	g_free(priv->selected_eeg);
	g_free(priv->selected_exg);
	g_free(priv->triggers);
	
	g_free(priv);
	g_free(panel);
}


int poll_widgets(EEGPanel* panel, GtkBuilder* builder)
{
	EEGPanelPrivateData* priv = panel->priv;

	// Get the root window
	priv->window = GTK_WINDOW(gtk_builder_get_object(builder, "topwindow"));
	g_object_set_data(G_OBJECT(priv->window), "eeg_panel", panel);
	
	// Get the plot widgets
	priv->eeg_scope = SCOPE(gtk_builder_get_object(builder, "eeg_scope"));
	priv->exg_scope = SCOPE(gtk_builder_get_object(builder, "exg_scope"));
	priv->eeg_bargraph = BARGRAPH(gtk_builder_get_object(builder, "eeg_bargraph"));
	priv->tri_scope = BINARY_SCOPE(gtk_builder_get_object(builder, "tri_scope"));
	priv->eeg_axes = LABELIZED_PLOT(gtk_builder_get_object(builder, "eeg_axes"));
	priv->exg_axes = LABELIZED_PLOT(gtk_builder_get_object(builder, "exg_axes"));
	priv->tri_axes = LABELIZED_PLOT(gtk_builder_get_object(builder, "tri_axes"));
	priv->bar_axes = LABELIZED_PLOT(gtk_builder_get_object(builder, "bar_axes"));

	// Get the control widgets
	priv->eeg_treeview = GTK_TREE_VIEW(gtk_builder_get_object(builder, "eeg_elecselec_treeview"));
	priv->reftype_combo = GTK_COMBO_BOX(gtk_builder_get_object(builder, "reftype_combo"));
	priv->elecref_combo = GTK_COMBO_BOX(gtk_builder_get_object(builder, "elecref_combo"));
	priv->electrodesets_combo = GTK_COMBO_BOX(gtk_builder_get_object(builder, "electrodesets_combo"));

	return 1;	
}

int initialize_widgets(EEGPanel* panel, GtkBuilder* builder)
{
	EEGPanelPrivateData* priv = panel->priv;
	
	initialize_list_treeview(priv->eeg_treeview, "channels");
	g_signal_connect_after(gtk_tree_view_get_selection(priv->eeg_treeview),
				"changed",
				(GCallback)channel_selection_changed_cb,
				EEG);
	//initialize_combo(priv->reftype_combo, "None\nAverage\nElectrode");
	//initialize_combo(priv->elecref_combo, "EXG1\nAF1");
	//initialize_combo(priv->electrodesets_combo, "set A\nsets AB\nsets A-D\nall");

	return 1;
}

int eegpanel_define_input(EEGPanel* panel, unsigned int num_eeg_channels,
				unsigned int num_exg_channels, unsigned int num_tri_lines,
				unsigned int sampling_rate)
{
	EEGPanelPrivateData* priv = panel->priv;
	ChannelSelection chann_selec;
	
	priv->nmax_eeg = num_eeg_channels;
	priv->nmax_exg = num_exg_channels;
	priv->nlines_tri = num_tri_lines;
	priv->sampling_rate = sampling_rate;

	// 

	// Add default channel labels if not available
	priv->eeg_labels = add_default_labels(priv->eeg_labels, num_eeg_channels, "EEG");
	priv->exg_labels = add_default_labels(priv->exg_labels, num_eeg_channels, "EXG");

	fill_treeview(priv->eeg_treeview, priv->nmax_eeg, (const char**)priv->eeg_labels);

	// Reset all data buffer;
	chann_selec.num_chann = 0;
	chann_selec.selection = NULL;
	chann_selec.labels = NULL;
	return set_data_input(panel, sampling_rate * priv->display_length, &chann_selec, &chann_selec);
}

GtkListStore* initialize_list_treeview(GtkTreeView* treeview, const gchar* attr_name)
{
	GtkListStore* list;
	GtkCellRenderer* cell_renderer;
	GtkTreeViewColumn* view_column;
	
	list = gtk_list_store_new(1, G_TYPE_STRING);
	cell_renderer = gtk_cell_renderer_text_new();
	view_column = gtk_tree_view_column_new_with_attributes(attr_name,
								cell_renderer,
								"text", 0,
								NULL);
	gtk_tree_view_append_column( treeview, view_column );
	gtk_tree_selection_set_mode( gtk_tree_view_get_selection(treeview), GTK_SELECTION_MULTIPLE );
	
	gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(list));
	g_object_unref(list);

	return list;
}


void initialize_combo(GtkComboBox* combo, const char* labels)
{
	GtkListStore* list;
	GtkCellRenderer *renderer;

	list = gtk_list_store_new(1, G_TYPE_STRING);
	gtk_combo_box_set_model(combo, GTK_TREE_MODEL(list));
	g_object_unref(list);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT(combo), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT(combo), renderer,
					"text", 0, NULL);

	if (labels)
		fill_combo(combo, labels);
}


void fill_treeview(GtkTreeView* treeview, unsigned int num_labels, const char** labels)
{
	GtkListStore* list;
	int i = 0;
	GtkTreeIter iter;

	list = GTK_LIST_STORE(gtk_tree_view_get_model(treeview));
	gtk_list_store_clear(list);

	if (!labels)
		return;

	while ((i<num_labels) && labels[i]) {
		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0, labels[i++], -1);
	}
}

void fill_combo(GtkComboBox* combo, const char* labels)
{
	GtkListStore* list;
	int num_labels, i;
	GtkTreeIter iter;
	gchar** labelv = NULL;

	list = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
	gtk_list_store_clear(list);

	labelv = g_strsplit(labels, "\n", 0);
	num_labels = labelv ? g_strv_length(labelv) : 0;
	for (i=0; i<num_labels; i++) {
		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0, labelv[i], -1);
	}
	g_strfreev(labelv);

	gtk_combo_box_set_active (combo, 0);
}


void eegpanel_add_selected_samples(EEGPanel* panel, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples)
{
	unsigned int num_eeg_ch, num_exg_ch, nmax_eeg, nmax_exg, i,j;
	unsigned int num_samples_written = 0;
	EEGPanelPrivateData* priv = panel->priv;
	unsigned int pointer = priv->current_sample;
	unsigned int *eeg_sel, *exg_sel; 
	unsigned int lock_res = 0;

	num_eeg_ch = priv->num_eeg_channels;
	num_exg_ch = priv->num_exg_channels;
	nmax_eeg = priv->nmax_eeg;
	nmax_exg = priv->nmax_exg;
	eeg_sel = priv->selected_eeg;
	exg_sel = priv->selected_exg;


	// if we need to wrap, first add the tail
	if (num_samples+pointer > priv->num_samples) {
		num_samples_written = num_samples+pointer - priv->num_samples;
		
		eegpanel_add_selected_samples(panel, eeg, exg, triggers, num_samples_written);

		eeg += nmax_eeg*num_samples_written;
		exg += nmax_exg*num_samples_written;
		triggers += num_samples_written;
		num_samples -= num_samples_written;
		pointer = 0;
	}

	// copy data
	if (eeg) {
		for (i=0; i<num_samples; i++)
			for (j=0; j<num_eeg_ch; j++)
				priv->eeg[(pointer+i)*num_eeg_ch+j] = eeg[i*nmax_eeg+eeg_sel[j]];
	}
	if (exg) {
		for (i=0; i<num_samples; i++)
			for (j=0; j<num_exg_ch; j++)
				priv->exg[(pointer+i)*num_exg_ch+j] = exg[i*nmax_exg+exg_sel[j]];
	}
	if (triggers)
		memcpy(priv->triggers + pointer, triggers, num_samples*sizeof(*triggers));

	// Update current pointer
	pointer += num_samples;
	if (pointer >= priv->num_samples)
		pointer -= priv->num_samples;
	priv->current_sample = pointer;

	if ((priv->main_loop_thread) && (priv->main_loop_thread != g_thread_self()))
		lock_res = 1;

	
	// Update the content of the scopes
	if (lock_res)
		gdk_threads_enter();
	scope_update_data(priv->eeg_scope, pointer);
	scope_update_data(priv->exg_scope, pointer);
	binary_scope_update_data(priv->tri_scope, pointer);
	if (lock_res)
		gdk_threads_leave();
}

void eegpanel_add_samples(EEGPanel* panel, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples)
{
	unsigned int num_eeg_ch, num_exg_ch;
	unsigned int num_samples_written = 0;
	EEGPanelPrivateData* priv = panel->priv;
	unsigned int pointer = priv->current_sample;
	unsigned int lock_res = 0;

	num_eeg_ch = priv->num_eeg_channels;
	num_exg_ch = priv->num_exg_channels;

	// if we need to wrap, first add the tail
	if (num_samples+pointer > priv->num_samples) {
		num_samples_written = num_samples+pointer - priv->num_samples;
		
		eegpanel_add_samples(panel, eeg, exg, triggers, num_samples_written);

		eeg += num_eeg_ch*num_samples_written;
		exg += num_exg_ch*num_samples_written;
		triggers += num_samples_written;
		num_samples -= num_samples_written;
		pointer = 0;
	}

	// copy data
	if (eeg)
		memcpy(priv->eeg + pointer*num_eeg_ch, eeg, num_samples*num_eeg_ch*sizeof(*eeg));
	if (exg)
		memcpy(priv->exg + pointer*num_exg_ch, exg, num_samples*num_exg_ch*sizeof(*exg));
	if (triggers)
		memcpy(priv->triggers + pointer, triggers, num_samples*sizeof(*triggers));

	// Update current pointer
	pointer += num_samples;
	if (pointer >= priv->num_samples)
		pointer -= priv->num_samples;
	priv->current_sample = pointer;

	if ((priv->main_loop_thread) && (priv->main_loop_thread != g_thread_self()))
		lock_res = 1;

	
	// Update the content of the scopes
	if (lock_res)
		gdk_threads_enter();
	// Update the content of the scopes
	scope_update_data(priv->eeg_scope, pointer);
	scope_update_data(priv->exg_scope, pointer);
	binary_scope_update_data(priv->tri_scope, pointer);
	if (lock_res)
		gdk_threads_leave();
}


int set_data_input(EEGPanel* panel, int num_samples, ChannelSelection* eeg_selec, ChannelSelection* exg_selec)
{
	unsigned int num_eeg_points, num_exg_points, num_tri_points, num_eeg, num_exg;
	float *eeg, *exg;
	uint32_t *triggers;
	EEGPanelPrivateData* priv = panel->priv;

	// Use the previous values if unspecified
	num_samples = (num_samples>=0) ? num_samples : priv->num_samples;
	num_eeg = eeg_selec ? eeg_selec->num_chann : priv->num_eeg_channels;
	num_exg = exg_selec ? exg_selec->num_chann : priv->num_exg_channels;

	num_eeg_points = num_samples*num_eeg;
	num_exg_points = num_samples*num_exg;
	num_tri_points = num_samples;

	if (num_eeg_points != priv->num_eeg_channels*priv->num_samples) {
		eeg = g_malloc0(num_eeg_points * sizeof(*eeg));
		g_free(priv->eeg);
		priv->eeg = eeg;
	}
	scope_set_data(priv->eeg_scope, priv->eeg, num_samples, num_eeg);

	if (num_exg_points != priv->num_exg_channels*priv->num_samples) {
		exg = g_malloc0(num_exg_points * sizeof(*exg));
		g_free(priv->exg);
		priv->exg = exg;
	}
	scope_set_data(priv->exg_scope, priv->exg, num_samples, num_exg);

	if (num_tri_points != priv->num_samples) {
		triggers = g_malloc0(num_tri_points * sizeof(*triggers));
		g_free(priv->triggers);
		priv->triggers = triggers;
	}
	binary_scope_set_data(priv->tri_scope, priv->triggers, num_samples, priv->nlines_tri);

	// Store the eeg selection and update the labels
	if (eeg_selec) {
		if (num_eeg != priv->num_eeg_channels) {
			g_free(priv->selected_eeg);
			priv->selected_eeg = g_malloc(num_eeg*sizeof(*(priv->selected_eeg)));
			priv->num_eeg_channels = num_eeg;
		}
		memcpy(priv->selected_eeg, eeg_selec->selection, num_eeg*sizeof(*(priv->selected_eeg)));

		if (eeg_selec->labels)	
			g_object_set(G_OBJECT(priv->eeg_axes), "ytick-labelv", eeg_selec->labels, NULL);
	}


	// Store the exg selection and update the labels
	if (exg_selec) {
		if (num_exg != priv->num_exg_channels) {
			g_free(priv->selected_exg);
			priv->selected_exg = g_malloc(num_exg*sizeof(*(priv->selected_exg)));
			priv->num_exg_channels = num_exg;
		}
		memcpy(priv->selected_exg, exg_selec->selection, num_exg*sizeof(*(priv->selected_exg)));

		if (exg_selec->labels)	
			g_object_set(G_OBJECT(priv->exg_axes), "ytick-labelv", exg_selec->labels, NULL);
	}

	priv->current_sample = 0;
	priv->num_samples = num_samples;
	return 1;
}

int fill_selec_from_treeselec(ChannelSelection* selection, GtkTreeSelection* tree_sel, char** labels)
{
	int num = gtk_tree_selection_count_selected_rows(tree_sel);
	GList* list;
	int i;
	unsigned int* chann_select = NULL;
	const char** labelv = NULL;

	// Allocation
	chann_select = g_malloc(num*sizeof(*chann_select));

	// Fill the array of the selected channels
	list = gtk_tree_selection_get_selected_rows(tree_sel, NULL);
	for(i=0; i<num; i++) {
		chann_select[i] = *gtk_tree_path_get_indices((GtkTreePath*)(list->data));
		list = g_list_next(list);
	}
	g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(list);

	// Add labels name if provided
	if (labels) {
		labelv = g_malloc0((num+1)*sizeof(*labelv));
		for (i=0; i<num; i++)
			 labelv[i] = labels[chann_select[i]];
	}

	selection->selection = chann_select;
	selection->num_chann = num;
	selection->labels = labelv;

	return 0;
}

char** add_default_labels(char** labels, unsigned int requested_num_labels, const char* prefix)
{
	guint prev_length, alloc_length, i;

	if(labels)
		prev_length = g_strv_length(labels);
	else
		prev_length = 0;

	if(prev_length < requested_num_labels) {
		/* Calculate the size of the prefix to know the size that should be allocated to the strings */
		alloc_length = 0;
		while(prefix[alloc_length])
			alloc_length++;

		alloc_length += 5;

		/* Append the new labels to the previous ones */
		labels = g_realloc(labels, (requested_num_labels+1)*sizeof(gchar*));
		for(i=prev_length; i<requested_num_labels; i++) {
			labels[i] = g_malloc(alloc_length);
			g_sprintf( labels[i], "%s%u", prefix, i+1 );
		}
		labels[requested_num_labels] = NULL;
	}

	return labels;
}


