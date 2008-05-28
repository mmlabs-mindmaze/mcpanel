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


#define GET_PANEL_FROM(widget)  ((EEGPanel*)(g_object_get_data(G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))), "eeg_panel")))

typedef enum {
	TOP_WINDOW,
	EEG_SCOPE,
	EXG_SCOPE,
	TRI_SCOPE,
	EEG_OFFSET_BAR1,
	EEG_OFFSET_BAR2,
	EXG_OFFSET_BAR1,
	EXG_OFFSET_BAR2,
	EEG_AXES,
	EXG_AXES,
	TRI_AXES,
	EEG_OFFSET_AXES1,
	EEG_OFFSET_AXES2,
	EXG_OFFSET_AXES1,
	EXG_OFFSET_AXES2,
	EEG_SCALE_COMBO,
	EEG_LOWPASS_CHECK,
	EEG_LOWPASS_SPIN,
	EEG_HIGHPASS_CHECK,
	EEG_HIGHPASS_SPIN,
	REFTYPE_COMBO,
	ELECREF_COMBO,
	ELECSET_COMBO,
	EEG_TREEVIEW,
	OFFSET_SCALE_COMBO,
	EXG_SCALE_COMBO,
	EXG_LOWPASS_CHECK,
	EXG_LOWPASS_SPIN,
	EXG_HIGHPASS_CHECK,
	EXG_HIGHPASS_SPIN,
	EXG_TREEVIEW,
	STARTACQUISITION_BUTTON,
	NATIVE_FREQ_LABEL,
	DECIMATION_COMBO,
	DISPLAYED_FREQ_LABEL,
	TIME_WINDOW_COMBO,
	RECORDING_LIMIT_ENTRY,
	START_RECORDING_BUTTON,
	PAUSE_RECORDING_BUTTON,
	FILE_LENGTH_LABEL,
	NUM_PANEL_WIDGETS_DEFINED
} PanelWidgetEnum;

typedef enum {
	ELEC_TYPE,
	BIPOLE_TYPE,
	OFFSET_TYPE
} ScopeType;

typedef enum {
	NONE_REF,
	AVERAGE_REF,
	ELECTRODE_REF
} RefType;

typedef enum {
	EEG_LOWPASS_FILTER,
	EEG_HIGHPASS_FILTER,
	EEG_DECIMATION_FILTER,
	EXG_LOWPASS_FILTER,
	EXG_HIGHPASS_FILTER,
	EXG_DECIMATION_FILTER,
	EEG_OFFSET_FILTER,
	EXG_OFFSET_FILTER,
	NUM_FILTERS
} EnumFilter;


typedef struct _LinkWidgetName {
	PanelWidgetEnum id;
	const char* name;
	const char* type;
} LinkWidgetName;

const LinkWidgetName widget_name_table[] = {
	{TOP_WINDOW, "topwindow", "GtkWindow"},
	{EEG_SCOPE, "eeg_scope", "Scope"},
	{EXG_SCOPE, "exg_scope", "Scope"},
	{TRI_SCOPE, "tri_scope", "BinaryScope"},
	{EEG_OFFSET_BAR1, "eeg_offset_bar1", "Bargraph"},
	{EEG_OFFSET_BAR2, "eeg_offset_bar2", "Bargraph"},
//	{EXG_OFFSET_BAR1, "exg_offset_bar1", "Bargraph"},
//	{EXG_OFFSET_BAR2, "exg_offset_bar2", "Bargraph"},
	{EEG_AXES, "eeg_axes", "LabelizedPlot"},
	{EXG_AXES, "exg_axes", "LabelizedPlot"},
	{TRI_AXES, "tri_axes", "LabelizedPlot"},
	{EEG_OFFSET_AXES1, "eeg_offset_axes1", "LabelizedPlot"},
	{EEG_OFFSET_AXES2, "eeg_offset_axes2", "LabelizedPlot"},
//	{EXG_OFFSET_AXES1, "exg_offset_axes1", "LabelizedPlot"},
//	{EXG_OFFSET_AXES2, "exg_offset_axes2", "LabelizedPlot"},
	{EEG_SCALE_COMBO, "eeg_scale_combo", "GtkComboBox"},
	{EEG_LOWPASS_CHECK, "eeg_lowpass_check", "GtkCheckButton"},
	{EEG_LOWPASS_SPIN, "eeg_lowpass_spin", "GtkSpinButton"},
	{EEG_HIGHPASS_CHECK, "eeg_highpass_check", "GtkCheckButton"},
	{EEG_HIGHPASS_SPIN, "eeg_highpass_spin", "GtkSpinButton"},
	{REFTYPE_COMBO, "reftype_combo", "GtkComboBox"},
	{ELECREF_COMBO, "elecref_combo", "GtkComboBox"},
	{ELECSET_COMBO, "elecset_combo", "GtkComboBox"},
	{EEG_TREEVIEW, "eeg_treeview", "GtkTreeView"},
	{OFFSET_SCALE_COMBO, "offset_scale_combo", "GtkComboBox"},
	{EXG_SCALE_COMBO, "exg_scale_combo", "GtkComboBox"},
	{EXG_LOWPASS_CHECK, "exg_lowpass_check", "GtkCheckButton"},
	{EXG_LOWPASS_SPIN, "exg_lowpass_spin", "GtkSpinButton"},
	{EXG_HIGHPASS_CHECK, "exg_highpass_check", "GtkCheckButton"},
	{EXG_HIGHPASS_SPIN, "exg_highpass_spin", "GtkSpinButton"},
	{EXG_TREEVIEW, "exg_treeview", "GtkTreeView"},
	{STARTACQUISITION_BUTTON, "startacquisition_button", "GtkToggleButton"},  
	{NATIVE_FREQ_LABEL, "native_freq_label", "GtkLabel"},
	{DECIMATION_COMBO, "decimation_combo", "GtkComboBox"},
	{DISPLAYED_FREQ_LABEL, "displayed_freq_label", "GtkLabel"},
	{TIME_WINDOW_COMBO, "time_window_combo", "GtkComboBox"},
	{RECORDING_LIMIT_ENTRY, "recording_limit_entry", "GtkEntry"},
	{START_RECORDING_BUTTON, "start_recording_button", "GtkButton"},
	{PAUSE_RECORDING_BUTTON, "pause_recording_button", "GtkToggleButton"},
	{FILE_LENGTH_LABEL, "file_length_label", "GtkLabel"}
};

#define NUM_PANEL_WIDGETS_REGISTERED (sizeof(widget_name_table)/sizeof(widget_name_table[0]))

struct _EEGPanelPrivateData {
	GtkWindow* window;
	Scope* eeg_scope;
	Scope* exg_scope;
	BinaryScope* tri_scope;
	Bargraph *eeg_offset_bar1, *eeg_offset_bar2;

	GObject* widgets[NUM_PANEL_WIDGETS_DEFINED];

	GThread* main_loop_thread;

	//
	unsigned int sampling_rate;
	unsigned int decimation_factor;
	unsigned int decimation_offset;
	unsigned int nmax_eeg, nmax_exg, nlines_tri;
	unsigned int num_eeg_channels, num_exg_channels;
	unsigned int num_samples;
	unsigned int display_length;
	unsigned int current_sample;

	// Labels
	char** eeg_labels;
	char** exg_labels;
	char** bipole_labels;

	// data
	unsigned int *selected_eeg, *selected_exg;
	float *eeg, *exg;
	float *eeg_offset, *exg_offset;
	uint32_t *triggers;
	RefType eeg_ref_type;
	int eeg_ref_elec;

	// filters
	dfilter *filt[NUM_FILTERS];
	float filter_fc[NUM_FILTERS];
};

void eegpanel_destroy(EEGPanel* panel);
int set_data_input(EEGPanel* panel, int num_samples, ChannelSelection* eeg_selec, ChannelSelection* exg_selec);
int fill_selec_from_treeselec(ChannelSelection* selection, GtkTreeSelection* tree_sel, char** labels);
int poll_widgets(EEGPanel* panel, GtkBuilder* builder);
int initialize_widgets(EEGPanel* panel, GtkBuilder* builder);
GtkListStore* initialize_list_treeview(GtkTreeView* treeview, const gchar* attr_name);
void initialize_combo(GtkComboBox* combo);
void fill_treeview(GtkTreeView* treeview, unsigned int num_labels, const char** labels);
void fill_combo(GtkComboBox* combo, char** labels, int num_labels);
char** add_default_labels(char** labels, unsigned int requested_num_labels, const char* prefix);
void set_bipole_labels(EEGPanelPrivateData* priv);
void set_scopes_xticks(EEGPanelPrivateData* priv);
void set_bargraphs_yticks(EEGPanelPrivateData* priv, float max);
void set_all_filters(EEGPanelPrivateData* priv);
void process_eeg(EEGPanelPrivateData* priv, const float* eeg, float* temp_buff, unsigned int n_samples);
void process_exg(EEGPanelPrivateData* priv, const float* exg, float* temp_buff, unsigned int n_samples);
void process_tri(EEGPanelPrivateData* priv, const uint32_t* tri, uint32_t* temp_buff, unsigned int n_samples);
void remove_electrode_ref(float* data, unsigned int nchann, const float* fullset, unsigned int nchann_full, unsigned int num_samples, unsigned int elec_ref);
void remove_common_avg_ref(float* data, unsigned int nchann, const float* fullset, unsigned int nchann_full, unsigned int num_samples);

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

void reftype_combo_changed_cb(GtkComboBox* combo, gpointer data)
{
	GtkTreeIter iter;
	GValue value = {0};
	GtkTreeModel* model;
	RefType type;
	EEGPanelPrivateData* priv = GET_PANEL_FROM(combo)->priv;
	
	// Get the value set
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	type = g_value_get_uint(&value);

	gtk_widget_set_sensitive(GTK_WIDGET(priv->widgets[ELECREF_COMBO]), (type==ELECTRODE_REF)? TRUE : FALSE);

	priv->eeg_ref_type = type;
}

void refelec_combo_changed_cb(GtkComboBox* combo, gpointer data)
{
	EEGPanelPrivateData* priv = GET_PANEL_FROM(combo)->priv;
	priv->eeg_ref_elec = gtk_combo_box_get_active(combo);
}

void elecset_combo_changed_cb(GtkComboBox* combo, gpointer data)
{
	unsigned int selec, nmax;
	int first, last;
	GtkTreeSelection* tree_selection;
	GtkTreePath *path1, *path2;
	EEGPanelPrivateData* priv = GET_PANEL_FROM(combo)->priv;

	tree_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->widgets[EEG_TREEVIEW]));

	selec = gtk_combo_box_get_active(combo);

	if (selec==0) {
		gtk_tree_selection_select_all(tree_selection);
	}
	else {
		nmax = priv->nmax_eeg;

		first = 0;
		last = selec*32 - 1;
		first = (first < nmax) ? first : nmax-1;
		last = (last < nmax) ? last : nmax-1;

		path1 = gtk_tree_path_new_from_indices(first, -1);
		path2 = gtk_tree_path_new_from_indices(last, -1);
		gtk_tree_selection_unselect_all(tree_selection);
		gtk_tree_selection_select_range(tree_selection, path1, path2);
		gtk_tree_path_free(path1);
		gtk_tree_path_free(path2);
	}
}

extern void channel_selection_changed_cb(GtkTreeSelection* selection, gpointer user_data)
{
	ChannelSelection select;
	ChannelType type = (ChannelType)user_data;
	EEGPanel* panel = GET_PANEL_FROM(gtk_tree_selection_get_tree_view(selection));
	char** labels = (type == EEG) ? panel->priv->eeg_labels : panel->priv->bipole_labels;
	
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

void decimation_combo_changed_cb(GtkComboBox* combo, gpointer data)
{
	char tempstr[32];
	GtkTreeIter iter;
	GValue value = {0};
	GtkTreeModel* model;
	EEGPanel* panel = GET_PANEL_FROM(combo);
	EEGPanelPrivateData* priv = panel->priv;

	// Get the value set
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);

	// Reset the internals
	priv->decimation_factor = g_value_get_uint(&value);
	set_data_input(panel, (priv->sampling_rate*priv->display_length)/priv->decimation_factor, NULL, NULL);
	
	// update the display
	sprintf(tempstr,"%u Hz",priv->sampling_rate/priv->decimation_factor); 
	gtk_label_set_text(GTK_LABEL(priv->widgets[DISPLAYED_FREQ_LABEL]), tempstr);

}

void scale_combo_changed_cb(GtkComboBox* combo, gpointer user_data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value = {0};
	double scale;
	ScopeType type = (ChannelType)user_data;
	EEGPanelPrivateData* priv = GET_PANEL_FROM(combo)->priv;
	
	
	// Get the value set
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	scale = g_value_get_double(&value);

	switch (type) {
	case ELEC_TYPE:
		g_object_set(priv->widgets[EEG_SCOPE], "scale", scale, NULL);
		break;

	case BIPOLE_TYPE:
		g_object_set(priv->widgets[EXG_SCOPE], "scale", scale, NULL);
		break;

	case OFFSET_TYPE:
		g_object_set(priv->widgets[EEG_OFFSET_BAR1], "min-value", -scale, "max-value", scale, NULL);
		g_object_set(priv->widgets[EEG_OFFSET_BAR2], "min-value", -scale, "max-value", scale, NULL);
		set_bargraphs_yticks(priv, scale);
		break;
	}
}

void time_window_combo_changed_cb(GtkComboBox* combo, gpointer user_data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value = {0};
	unsigned int time_length, num_samples;
	EEGPanel* panel = GET_PANEL_FROM(combo);
	EEGPanelPrivateData* priv = panel->priv;
	
	
	// Get the value set
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	time_length = g_value_get_uint(&value);

	priv->display_length = time_length;
	num_samples = time_length * (priv->sampling_rate / priv->decimation_factor);
	set_data_input(panel, num_samples, NULL, NULL);
	set_scopes_xticks(priv);
}

void filter_button_changed_cb(GtkButton* button, gpointer user_data)
{
	EEGPanelPrivateData* priv = GET_PANEL_FROM(button)->priv;
	set_all_filters(priv);
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
	if (poll_widgets(panel, builder)) {

	
		// Needed initializations
		priv->display_length = 1;
		priv->decimation_factor = 1;
	
		// Initialize the content of the widgets
		initialize_widgets(panel, builder);
		eegpanel_define_input(panel, 128, 8, 16, 2048);
		set_scopes_xticks(priv);
	} else {
		eegpanel_destroy(panel);
		panel = NULL;
	}

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
	g_strfreev(priv->bipole_labels);
	
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
	int i;
	EEGPanelPrivateData* priv = panel->priv;

	// Get the list of mandatory widgets and check their type;
	for (i=0; i<NUM_PANEL_WIDGETS_REGISTERED; i++) {
		int id = widget_name_table[i].id;
		const char* name = widget_name_table[i].name;
		GType type = g_type_from_name(widget_name_table[i].type);

		priv->widgets[id] = gtk_builder_get_object(builder, name);
		if (!priv->widgets[id] || !g_type_is_a(G_OBJECT_TYPE(priv->widgets[id]), type)) {
			fprintf(stderr, "Widget \"%s\" not found or is not a derived type of %s\n", name, widget_name_table[i].type);
			return 0;
		}
	}

	// Get the root window and pointer to the panel as internal
	// data to retrieve it later easily in the callbacks
	priv->window = GTK_WINDOW(priv->widgets[TOP_WINDOW]);
	g_object_set_data(G_OBJECT(priv->window), "eeg_panel", panel);
	
	// Get the plot widgets
	priv->eeg_scope = SCOPE(gtk_builder_get_object(builder, "eeg_scope"));
	priv->exg_scope = SCOPE(gtk_builder_get_object(builder, "exg_scope"));
	priv->eeg_offset_bar1 = BARGRAPH(gtk_builder_get_object(builder, "eeg_offset_bar1"));
	priv->eeg_offset_bar2 = BARGRAPH(gtk_builder_get_object(builder, "eeg_offset_bar2"));
	priv->tri_scope = BINARY_SCOPE(gtk_builder_get_object(builder, "tri_scope"));

	return 1;	
}

int initialize_widgets(EEGPanel* panel, GtkBuilder* builder)
{
	GtkTreeView* treeview;
	EEGPanelPrivateData* priv = panel->priv;
	
	treeview = GTK_TREE_VIEW(priv->widgets[EEG_TREEVIEW]);
	initialize_list_treeview(treeview, "channels");
	g_signal_connect_after(gtk_tree_view_get_selection(treeview), "changed", (GCallback)channel_selection_changed_cb, (gpointer)EEG);

	treeview = GTK_TREE_VIEW(priv->widgets[EXG_TREEVIEW]);
	initialize_list_treeview(treeview, "channels");
	g_signal_connect_after(gtk_tree_view_get_selection(treeview), "changed", (GCallback)channel_selection_changed_cb, (gpointer)EXG);

	gtk_combo_box_set_active(GTK_COMBO_BOX(priv->widgets[DECIMATION_COMBO]), 0);
	g_signal_connect_after(priv->widgets[DECIMATION_COMBO],	"changed", (GCallback)decimation_combo_changed_cb, NULL);
	
	// Scale combos
	g_signal_connect(priv->widgets[EEG_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)ELEC_TYPE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(priv->widgets[EEG_SCALE_COMBO]), 0);
	g_signal_connect(priv->widgets[OFFSET_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)OFFSET_TYPE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(priv->widgets[OFFSET_SCALE_COMBO]), 0);
	g_signal_connect(priv->widgets[EXG_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)BIPOLE_TYPE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(priv->widgets[EXG_SCALE_COMBO]), 0);

	// Time window combo
	gtk_combo_box_set_active(GTK_COMBO_BOX(priv->widgets[TIME_WINDOW_COMBO]), 0);
	g_signal_connect_after(priv->widgets[TIME_WINDOW_COMBO], "changed", (GCallback)time_window_combo_changed_cb, (gpointer)ELEC_TYPE);

	// filter buttons
	g_signal_connect_after(priv->widgets[EEG_LOWPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(priv->widgets[EEG_HIGHPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(priv->widgets[EXG_LOWPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(priv->widgets[EXG_HIGHPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(priv->widgets[EEG_LOWPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(priv->widgets[EEG_HIGHPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(priv->widgets[EXG_LOWPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(priv->widgets[EXG_HIGHPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);

	// reference combos
	gtk_combo_box_set_active(GTK_COMBO_BOX(priv->widgets[REFTYPE_COMBO]), 0);
	g_signal_connect(priv->widgets[REFTYPE_COMBO], "changed", (GCallback)reftype_combo_changed_cb, NULL);
	g_signal_connect(priv->widgets[ELECREF_COMBO], "changed", (GCallback)refelec_combo_changed_cb, NULL);

	g_signal_connect(priv->widgets[ELECSET_COMBO], "changed", (GCallback)elecset_combo_changed_cb, NULL);
	return 1;
}

int eegpanel_define_input(EEGPanel* panel, unsigned int num_eeg_channels,
				unsigned int num_exg_channels, unsigned int num_tri_lines,
				unsigned int sampling_rate)
{
	char tempstr[32];
	int num_samples;
	EEGPanelPrivateData* priv = panel->priv;
	ChannelSelection chann_selec;
	
	priv->nmax_eeg = num_eeg_channels;
	priv->nmax_exg = num_exg_channels;
	priv->nlines_tri = num_tri_lines;
	priv->sampling_rate = sampling_rate;
	num_samples = (sampling_rate*priv->display_length)/priv->decimation_factor;
	// 

	// Add default channel labels if not available
	priv->eeg_labels = add_default_labels(priv->eeg_labels, num_eeg_channels, "EEG");
	priv->exg_labels = add_default_labels(priv->exg_labels, num_exg_channels, "EXG");
	set_bipole_labels(priv);

	// Update widgets
	fill_treeview(GTK_TREE_VIEW(priv->widgets[EEG_TREEVIEW]), priv->nmax_eeg, (const char**)priv->eeg_labels);
	fill_combo(GTK_COMBO_BOX(priv->widgets[ELECREF_COMBO]), priv->eeg_labels, priv->nmax_eeg);
	gtk_combo_box_set_active(GTK_COMBO_BOX(priv->widgets[ELECREF_COMBO]), 0);
	fill_treeview(GTK_TREE_VIEW(priv->widgets[EXG_TREEVIEW]), priv->nmax_exg, (const char**)priv->bipole_labels);
	sprintf(tempstr,"%u Hz",sampling_rate);
	gtk_label_set_text(GTK_LABEL(priv->widgets[NATIVE_FREQ_LABEL]), tempstr);
	sprintf(tempstr,"%u Hz",sampling_rate/priv->decimation_factor); 
	gtk_label_set_text(GTK_LABEL(priv->widgets[DISPLAYED_FREQ_LABEL]), tempstr);

	// Reset all data buffer;
	chann_selec.num_chann = 0;
	chann_selec.selection = NULL;
	chann_selec.labels = NULL;
	return set_data_input(panel, num_samples, &chann_selec, &chann_selec);
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


void initialize_combo(GtkComboBox* combo)
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

void fill_combo(GtkComboBox* combo, char** labels, int num_labels)
{
	GtkListStore* list;
	int i;
	GtkTreeIter iter;

	list = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
	gtk_list_store_clear(list);

	if (num_labels < -1)
		num_labels = labels ? g_strv_length(labels) : 0;

	for (i=0; i<num_labels; i++) {
		gtk_list_store_append(list, &iter);
		gtk_list_store_set(list, &iter, 0, labels[i], -1);
	}

	gtk_combo_box_set_active (combo, 0);
}


void eegpanel_add_samples(EEGPanel* panel, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples)
{
	unsigned int num_eeg_ch, num_exg_ch, nmax_eeg, nmax_exg;
	unsigned int num_samples_written = 0;
	EEGPanelPrivateData* priv = panel->priv;
	unsigned int pointer = priv->current_sample;
	unsigned int *eeg_sel, *exg_sel; 
	unsigned int lock_res = 0;
	void* buff = NULL;
	unsigned int buff_len;

	num_eeg_ch = priv->num_eeg_channels;
	num_exg_ch = priv->num_exg_channels;
	nmax_eeg = priv->nmax_eeg;
	nmax_exg = priv->nmax_exg;
	eeg_sel = priv->selected_eeg;
	exg_sel = priv->selected_exg;

	

	// if we need to wrap, first add the tail
	if (num_samples+pointer > priv->num_samples) {
		num_samples_written = priv->num_samples - pointer;
		
		eegpanel_add_samples(panel, eeg, exg, triggers, num_samples_written);

		eeg += nmax_eeg*num_samples_written;
		exg += nmax_exg*num_samples_written;
		triggers += num_samples_written;
		num_samples -= num_samples_written;
		pointer = priv->current_sample;
	}
	
	// Create a buffer big enough to hold incoming eeg or exg data
	buff_len = (num_eeg_ch > num_exg_ch) ? num_eeg_ch : num_exg_ch;
	buff_len *= num_samples;
	buff = g_malloc(buff_len*sizeof(*eeg));

	// copy data
	if (eeg) 
		process_eeg(priv, eeg, buff, num_samples);
	if (exg) 
		process_exg(priv, exg, buff, num_samples);
	if (triggers)
		process_tri(priv, triggers, buff, num_samples);

	g_free(buff);

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
	bargraph_update_data(priv->eeg_offset_bar1, 0);
	bargraph_update_data(priv->eeg_offset_bar2, 0);
	if (lock_res)
		gdk_threads_leave();
}

/*void eegpanel_add_selected_samples(EEGPanel* panel, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples)
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
		
		eegpanel_add_selected_samples(panel, eeg, exg, triggers, num_samples_written);

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
}*/


int set_data_input(EEGPanel* panel, int num_samples, ChannelSelection* eeg_selec, ChannelSelection* exg_selec)
{
	unsigned int num_eeg_points, num_exg_points, num_tri_points, num_eeg, num_exg;
	unsigned int num_elec_bar1, num_elec_bar2;
	float *eeg, *exg;
	uint32_t *triggers;
	EEGPanelPrivateData* priv = panel->priv;

	priv->decimation_offset = 0;

	// Use the previous values if unspecified
	num_samples = (num_samples>=0) ? num_samples : priv->num_samples;
	num_eeg = eeg_selec ? eeg_selec->num_chann : priv->num_eeg_channels;
	num_exg = exg_selec ? exg_selec->num_chann : priv->num_exg_channels;

	num_eeg_points = num_samples*num_eeg;
	num_exg_points = num_samples*num_exg;
	num_tri_points = num_samples;
	num_elec_bar2 = num_eeg/2;
	num_elec_bar1 = num_eeg - num_elec_bar2;

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
			g_free(priv->eeg_offset);
			priv->selected_eeg = g_malloc(num_eeg*sizeof(*(priv->selected_eeg)));
			priv->eeg_offset = g_malloc0(num_eeg*sizeof(*(priv->eeg_offset)));
			priv->num_eeg_channels = num_eeg;
		}
		memcpy(priv->selected_eeg, eeg_selec->selection, num_eeg*sizeof(*(priv->selected_eeg)));

		if (eeg_selec->labels) {	
			g_object_set(priv->widgets[EEG_AXES], "ytick-labelv", eeg_selec->labels, NULL);
			g_object_set(priv->widgets[EEG_OFFSET_AXES1], "xtick-labelv", eeg_selec->labels, NULL);	
			g_object_set(priv->widgets[EEG_OFFSET_AXES2], "xtick-labelv", eeg_selec->labels+num_elec_bar1, NULL);	
		}
	}


	// Store the exg selection and update the labels
	if (exg_selec) {
		if (num_exg != priv->num_exg_channels) {
			g_free(priv->selected_exg);
			g_free(priv->exg_offset);
			priv->selected_exg = g_malloc(num_exg*sizeof(*(priv->selected_exg)));
			priv->exg_offset = g_malloc0(num_exg*sizeof(*(priv->exg_offset)));
			priv->num_exg_channels = num_exg;
		}
		memcpy(priv->selected_exg, exg_selec->selection, num_exg*sizeof(*(priv->selected_exg)));

		if (exg_selec->labels)	
			g_object_set(priv->widgets[EXG_AXES], "ytick-labelv", exg_selec->labels, NULL);
	}

	// Set bargraphs data
	bargraph_set_data(priv->eeg_offset_bar1, priv->eeg_offset, num_elec_bar1);
	bargraph_set_data(priv->eeg_offset_bar2, priv->eeg_offset+num_elec_bar1, num_elec_bar2);

	set_all_filters(priv);

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

void set_bipole_labels(EEGPanelPrivateData* priv)
{
	unsigned int i, j;
	char** bip_labels = NULL;
	unsigned int num_labels = priv->nmax_exg;

	g_strfreev(priv->bipole_labels);
	bip_labels = g_malloc0((num_labels+1)*sizeof(char*));

	for (i=0; i<num_labels;i++) {
		j = (i+1)%num_labels;
		bip_labels[i] = g_strconcat(priv->exg_labels[i],
					    "-",
					    priv->exg_labels[j],
					    NULL);
	}

	priv->bipole_labels = bip_labels;
}


void set_scopes_xticks(EEGPanelPrivateData* priv)
{
	int i, value;
	unsigned int* ticks;
	char** labels;
	unsigned int inc, num_ticks;
	unsigned int disp_len = priv->display_length;
	unsigned int sampling_rate = priv->sampling_rate / priv->decimation_factor;

	inc = 1;
	if (disp_len > 5)
		inc = 2;
	if (disp_len > 10)
		inc = 5;
	if (disp_len > 30)
		inc = 10;
	
	num_ticks = disp_len / inc;
	ticks = g_malloc(num_ticks*sizeof(*ticks));
	labels = g_malloc0((num_ticks+1)*sizeof(*labels));

	// set the ticks and ticks labels
	for (i=0; i<num_ticks; i++) {
		value = (i+1)*inc;
		ticks[i] = value*sampling_rate -1;
		labels[i] = g_strdup_printf("%is",value);
	}

	// Set the ticks to the scope widgets
	scope_set_ticks(priv->eeg_scope, num_ticks, ticks);
	g_object_set(priv->widgets[EEG_AXES], "xtick-labelv", labels, NULL);
	binary_scope_set_ticks(priv->tri_scope, num_ticks, ticks);
	g_object_set(priv->widgets[TRI_AXES], "xtick-labelv", labels, NULL);
	scope_set_ticks(priv->exg_scope, num_ticks, ticks);
	g_object_set(priv->widgets[EXG_AXES], "xtick-labelv", labels, NULL);


	g_free(ticks);
	g_strfreev(labels);
}

void set_bargraphs_yticks(EEGPanelPrivateData* priv, float max)
{
	char* unit = "uV";
	int i, inc;
	float value;
	float* ticks;
	char** labels;
	int num_ticks;
	unsigned int max_value;
	
	max_value = max;
	if (max > 1000.0) {
		max_value = max/1000.0;
		unit = "mV";
	}

	
	inc = 1;
	if (max_value > 5)
		inc = 2;
	if (max_value > 10)
		inc = 5;
	if (max_value > 25)
		inc = 10;
	if (max_value > 50)
		inc = 25;
	if (max_value > 100)
		inc = 50;
	if (max_value > 200)
		inc = 100;
	if (max_value > 500)
		inc = 250;
	
	num_ticks = 2*(max_value / inc)+1;
	ticks = g_malloc(num_ticks*sizeof(*ticks));
	labels = g_malloc0((num_ticks+1)*sizeof(*labels));

	// set the ticks and ticks labels
	for (i=0; i<num_ticks; i++) {
		value = (i-(num_ticks-1)/2)*inc;
		ticks[i] = (max >1000.0) ? value*1000 : value;
		labels[i] = g_strdup_printf("%.0f%s",value,unit);
	}

	// Set the ticks to the scope widgets
	bargraph_set_ticks(priv->eeg_offset_bar1, num_ticks, ticks);
	bargraph_set_ticks(priv->eeg_offset_bar2, num_ticks, ticks);
	g_object_set(priv->widgets[EEG_OFFSET_AXES1], "ytick-labelv", labels, NULL);
	g_object_set(priv->widgets[EEG_OFFSET_AXES2], "ytick-labelv", labels, NULL);


	g_free(ticks);
	g_strfreev(labels);
}


void set_one_filter(EEGPanelPrivateData* priv, EnumFilter type, int enable, float fc, int nchann, int highpass)
{
	dfilter* filt = priv->filt[type];
	float current_fc = priv->filter_fc[type];

	if (enable) {
		if (!filt || (fc!=current_fc) || (filt->num_chann != nchann)) {
			destroy_filter(filt);
			filt = create_butterworth_filter(fc, 2, nchann, highpass);
			priv->filter_fc[type] = fc;
		}
	} else {
		destroy_filter(filt);
		filt = NULL;
	}

	priv->filt[type] = filt;
}

void set_all_filters(EEGPanelPrivateData* priv)
{
	float eeg_lowpass_fc, eeg_highpass_fc;
	float exg_lowpass_fc, exg_highpass_fc;
	float dec_fc, offset_fc;
	unsigned int eeg_low_state, eeg_high_state, exg_low_state, exg_high_state;

	unsigned int num_eeg = priv->num_eeg_channels;
	unsigned int num_exg = priv->num_exg_channels;
	float fs = priv->sampling_rate;
	unsigned int dec_state = (priv->decimation_factor != 1) ? 1 : 0;
	float dec_factor = priv->decimation_factor;

	// Set the cutoff frequencies of every filters
	dec_fc = fs*0.4/dec_factor;
	offset_fc = 1.0 / fs;
	fs /= dec_factor;
	eeg_lowpass_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(priv->widgets[EEG_LOWPASS_SPIN]))/fs;
	eeg_highpass_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(priv->widgets[EEG_HIGHPASS_SPIN]))/fs;
	exg_lowpass_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(priv->widgets[EXG_LOWPASS_SPIN]))/fs;
	exg_highpass_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(priv->widgets[EXG_HIGHPASS_SPIN]))/fs;

	// Retrieve the state of the check boxes
	eeg_low_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->widgets[EEG_LOWPASS_CHECK]));
	eeg_high_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->widgets[EEG_HIGHPASS_CHECK]));
	exg_low_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->widgets[EXG_LOWPASS_CHECK]));
	exg_high_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->widgets[EXG_HIGHPASS_CHECK]));

	// Set the filters
	set_one_filter(priv, EEG_LOWPASS_FILTER, eeg_low_state, eeg_lowpass_fc, num_eeg, 0);
	set_one_filter(priv, EEG_HIGHPASS_FILTER, eeg_high_state, eeg_highpass_fc, num_eeg, 1);
	set_one_filter(priv, EXG_LOWPASS_FILTER, exg_low_state, exg_lowpass_fc, num_exg, 0);
	set_one_filter(priv, EXG_HIGHPASS_FILTER, exg_high_state, exg_highpass_fc, num_exg, 1);

	set_one_filter(priv, EEG_DECIMATION_FILTER, dec_state, dec_fc, num_eeg, 0);
	set_one_filter(priv, EXG_DECIMATION_FILTER, dec_state, dec_fc, num_exg, 0);

	set_one_filter(priv, EEG_OFFSET_FILTER, 1, offset_fc, num_eeg, 0);
	set_one_filter(priv, EXG_OFFSET_FILTER, 1, offset_fc, num_exg, 0);
}

#define SWAP_POINTERS(pointer1, pointer2)	do {	\
	void* temp = pointer2;				\
	pointer2 = pointer1;				\
	pointer1 = temp;				\
} while(0)


void process_eeg(EEGPanelPrivateData* priv, const float* eeg, float* temp_buff, unsigned int n_samples)
{
	int i, j;
	unsigned int nchann = priv->num_eeg_channels;
	dfilter* filt;
	float* buff1, *buff2, *curr_eeg;
	unsigned int *sel = priv->selected_eeg;
	unsigned int num_ch = priv->num_eeg_channels;
	unsigned int nmax_ch = priv->nmax_eeg;
	
	buff1 = temp_buff;
	curr_eeg = priv->eeg + priv->current_sample*nchann;
	buff2 = curr_eeg;
 

	// Copy data of the selected channels
	for (i=0; i<n_samples; i++) {
		for (j=0; j<num_ch; j++)
			buff2[i*num_ch+j] = eeg[i*nmax_ch + sel[j]];
	}
	SWAP_POINTERS(buff1, buff2);

	
	filt = priv->filt[EEG_OFFSET_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		// copy last samples
		for (i=0; i<num_ch; i++)
			priv->eeg_offset[i] = buff2[nchann*(n_samples-1)+i];
	}
	
	// Do referencing
	if (priv->eeg_ref_type == AVERAGE_REF)
		remove_common_avg_ref(buff1, nchann, eeg, nmax_ch, n_samples);
	else if (priv->eeg_ref_type == ELECTRODE_REF)
		remove_electrode_ref(buff1, nchann, eeg, nmax_ch, n_samples, priv->eeg_ref_elec);

	// Process decimation
	// TODO
	
	

	filt = priv->filt[EEG_LOWPASS_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);		
	}

	filt = priv->filt[EEG_HIGHPASS_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);
	}
	
	// copy data to the destination buffer
	if (buff1 != curr_eeg)
		memcpy(curr_eeg, buff1, n_samples*nchann*sizeof(*buff1));
}


void process_exg(EEGPanelPrivateData* priv, const float* exg, float* temp_buff, unsigned int n_samples)
{
	int i, j, k;
	unsigned int nchann = priv->num_exg_channels;
	dfilter* filt;
	float* buff1, *buff2, *curr_exg;
	unsigned int *sel = priv->selected_exg;
	unsigned int num_ch = priv->num_exg_channels;
	unsigned int nmax_ch = priv->nmax_exg;
	
	buff1 = temp_buff;
	curr_exg = priv->exg + priv->current_sample*nchann;
	buff2 = curr_exg;
 

	// Copy data of the selected channels
	for (i=0; i<n_samples; i++) {
		for (j=0; j<num_ch; j++) {
			k = (sel[j]+1)%nmax_ch;
			buff2[i*num_ch+j] = exg[i*nmax_ch + sel[j]] - exg[i*nmax_ch + k];
		}
	}
	SWAP_POINTERS(buff1, buff2);

	
	/*filt = priv->filt[EXG_OFFSET_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		// copy last samples
		for (i=0; i<num_ch; i++)
			priv->exg_offset[i] = buff2[nchann*(n_samples-1)+i];
	}*/

	// Process decimation
	// TODO

	filt = priv->filt[EXG_LOWPASS_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);		
	}

	filt = priv->filt[EXG_HIGHPASS_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);
	}
	
	// copy data to the destination buffer
	if (buff1 != curr_exg)
		memcpy(curr_exg, buff1, n_samples*nchann*sizeof(*buff1));
}

void process_tri(EEGPanelPrivateData* priv, const uint32_t* tri, uint32_t* temp_buff, unsigned int n_samples)
{
	int i;
	uint32_t* buff1, *buff2, *curr_tri;
	
	buff1 = temp_buff;
	curr_tri = priv->triggers + priv->current_sample;
	buff2 = curr_tri;
 

	// Copy data
	for (i=0; i<n_samples; i++) {
		buff2[i] = tri[i];
	}
}

void remove_common_avg_ref(float* data, unsigned int nchann, const float* fullset, unsigned int nchann_full, unsigned int num_samples)
{
	int i, j;
	float sum;

	for (i=0; i<num_samples; i++) {
		// calculate the sum
		sum = 0;
		for (j=0; j<nchann_full; j++)
			sum += fullset[i*nchann_full+j];
		sum /= (float)nchann_full;

		// reference the data
		for (j=0; j<nchann; j++)
			data[i*nchann+j] -= sum;
	}
}

void remove_electrode_ref(float* data, unsigned int nchann, const float* fullset, unsigned int nchann_full, unsigned int num_samples, unsigned int elec_ref)
{
	int i, j;

	for (i=0; i<num_samples; i++) {
		// reference the data
		for (j=0; j<nchann; j++)
			data[i*nchann+j] -= fullset[i*nchann_full+elec_ref];
	}
}
