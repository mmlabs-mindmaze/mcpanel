/*
	Copyright (C) 2008-2009 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This file is part of the eegpan library

    The eegpan library is free software: you can redistribute it and/or
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <memory.h>
#include <stdlib.h>
#include "eegpanel.h"
#include "plot-area.h"
#include "scope.h"
#include "bargraph.h"
#include "binary-scope.h"
#include "labelized-plot.h"
#include "gtk-led.h"
#include "filter.h"
#include "default-ui.h"

#define REFRESH_INTERVAL	30

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
	CONNECT_LED,
	CMS_LED,
	BATTERY_LED,
	NATIVE_FREQ_LABEL,
	DECIMATION_COMBO,
	DISPLAYED_FREQ_LABEL,
	TIME_WINDOW_COMBO,
	RECORDING_LIMIT_ENTRY,
	RECORDING_LED,
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

typedef struct _FilterParam {
	float fc;
	float freq;
	int state;
} FilterParam;

typedef struct _Indicators {
	unsigned int cms_in_range	: 1;
	unsigned int low_battery	: 1;
} Indicators;

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
	{CONNECT_LED, "connect_led", "GtkLed"},  
	{CMS_LED, "cms_led", "GtkLed"},  
	{BATTERY_LED, "battery_led", "GtkLed"},  
	{STARTACQUISITION_BUTTON, "startacquisition_button", "GtkButton"},  
	{NATIVE_FREQ_LABEL, "native_freq_label", "GtkLabel"},
	{DECIMATION_COMBO, "decimation_combo", "GtkComboBox"},
	{DISPLAYED_FREQ_LABEL, "displayed_freq_label", "GtkLabel"},
	{TIME_WINDOW_COMBO, "time_window_combo", "GtkComboBox"},
	{RECORDING_LIMIT_ENTRY, "recording_limit_entry", "GtkEntry"},
	{RECORDING_LED, "recording_led", "GtkLed"},  
	{START_RECORDING_BUTTON, "start_recording_button", "GtkButton"},
	{PAUSE_RECORDING_BUTTON, "pause_recording_button", "GtkButton"},
	{FILE_LENGTH_LABEL, "file_length_label", "GtkLabel"}
};

#define NUM_PANEL_WIDGETS_REGISTERED (sizeof(widget_name_table)/sizeof(widget_name_table[0]))

struct _EEGPanel {
	GtkWindow* window;
	Scope* eeg_scope;
	Scope* exg_scope;
	BinaryScope* tri_scope;
	Bargraph *eeg_offset_bar1, *eeg_offset_bar2;

	GObject* widgets[NUM_PANEL_WIDGETS_DEFINED];

	GThread* main_loop_thread;
	GMutex* data_mutex;

	// modal dialog stuff
	GMutex* dialog_mutex;
	GMutex* dlg_completion_mutex;
	GtkDialog* dialog;
	gint dlg_retval;

	// states
	gboolean connected;
	gboolean fileopened;
	gboolean recording;

	//
	unsigned int sampling_rate;
	unsigned int decimation_factor;
	unsigned int decimation_offset;
	unsigned int nmax_eeg, nmax_exg, nlines_tri;
	unsigned int num_eeg_channels, num_exg_channels;
	unsigned int num_samples;
	float display_length;
	unsigned int current_sample;
	unsigned int last_drawn_sample;

	// Labels
	char** eeg_labels;
	char** exg_labels;
	char** bipole_labels;

	// data
	unsigned int *selected_eeg, *selected_exg;
	Indicators flags;
	float *eeg, *exg;
	float *eeg_offset, *exg_offset;
	uint32_t *triggers;
	RefType eeg_ref_type;
	int eeg_ref_elec;

	// filters
	dfilter *filt[NUM_FILTERS];
	FilterParam filter_param[NUM_FILTERS];

	// callbacks
	struct PanelCb cb;
};

gpointer loop_thread(gpointer user_data);
int set_data_input(EEGPanel* pan, int num_samples, ChannelSelection* eeg_selec, ChannelSelection* exg_selec);
int fill_selec_from_treeselec(ChannelSelection* selection, GtkTreeSelection* tree_sel, char** labels);
void clean_selec(ChannelSelection* selection);
int poll_widgets(EEGPanel* pan, GtkBuilder* builder);
int initialize_widgets(EEGPanel* pan, GtkBuilder* builder);
GtkListStore* initialize_list_treeview(GtkTreeView* treeview, const gchar* attr_name);
void initialize_combo(GtkComboBox* combo);
void fill_treeview(GtkTreeView* treeview, unsigned int num_labels, const char** labels);
void fill_combo(GtkComboBox* combo, char** labels, int num_labels);
char** add_default_labels(char** labels, unsigned int requested_num_labels, const char* prefix);
void set_bipole_labels(EEGPanel* pan);
void set_scopes_xticks(EEGPanel* pan);
void set_bargraphs_yticks(EEGPanel* pan, float max);
void set_all_filters(EEGPanel* pan, FilterParam* options);
void initialize_all_filters(EEGPanel* pan);
void process_eeg(EEGPanel* pan, const float* eeg, float* temp_buff, unsigned int n_samples);
void process_exg(EEGPanel* pan, const float* exg, float* temp_buff, unsigned int n_samples);
void process_tri(EEGPanel* pan, const uint32_t* tri, uint32_t* temp_buff, unsigned int n_samples);
void remove_electrode_ref(float* data, unsigned int nchann, const float* fullset, unsigned int nchann_full, unsigned int num_samples, unsigned int elec_ref);
void remove_common_avg_ref(float* data, unsigned int nchann, const float* fullset, unsigned int nchann_full, unsigned int num_samples);
void add_samples(EEGPanel* pan, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples);
int RegisterCustomDefinition(void);
gboolean check_redraw_scopes(gpointer user_data);
void set_default_values(EEGPanel* pan, const char* filename);
gint run_model_dialog(EEGPanel* pan, GtkDialog* dialog);
void get_initial_values(EEGPanel* pan);

///////////////////////////////////////////////////
//
//	Signal handlers
//
///////////////////////////////////////////////////
gboolean pause_recording_button_clicked_cb(GtkButton* button, gpointer data);

gboolean startacquisition_button_clicked_cb(GtkButton* button, gpointer data)
{
	EEGPanel* pan = GET_PANEL_FROM(button);

	(void)data;

	if (pan->cb.system_connection) {
		if (pan->cb.system_connection(pan->connected ? 0 : 1, pan->cb.user_data)) {
			pan->connected = !pan->connected;
			gtk_led_set_state(GTK_LED(pan->widgets[CONNECT_LED]), pan->connected);
			gtk_button_set_label(button, pan->connected ? "Disconnect" : "Connect");
		}
	}
	

	return TRUE;
}


gboolean start_recording_button_clicked_cb(GtkButton* button, gpointer data)
{
	ChannelSelection eeg_sel, exg_sel;
	GtkTreeSelection* selection;
	EEGPanel* pan = GET_PANEL_FROM(button);

	(void)data;


	if (!pan->fileopened) {
		// setup recording
		
		// Prepare selections
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pan->widgets[EEG_TREEVIEW]));
		fill_selec_from_treeselec(&eeg_sel, selection, pan->eeg_labels);
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pan->widgets[EXG_TREEVIEW]));
		fill_selec_from_treeselec(&exg_sel, selection, pan->exg_labels);

		// Send the setup event through the callback
		if (pan->cb.setup_recording) {
			if (pan->cb.setup_recording(&eeg_sel, &exg_sel, pan->cb.user_data)) {
				pan->fileopened = TRUE;
				gtk_button_set_label(GTK_BUTTON(pan->widgets[PAUSE_RECORDING_BUTTON]),"Record");
				gtk_button_set_label(button, "Stop");
				gtk_widget_set_sensitive(GTK_WIDGET(pan->widgets[PAUSE_RECORDING_BUTTON]),TRUE);
			}
		}
		
		// free selection
		g_free(eeg_sel.selection);
		g_free(eeg_sel.labels);
		g_free(exg_sel.selection);
		g_free(exg_sel.labels);
	} 
	else {
		// Stop recording
		if (pan->cb.stop_recording) {
			// Pause recording before
			if (pan->recording) 
				pause_recording_button_clicked_cb(GTK_BUTTON(pan->widgets[PAUSE_RECORDING_BUTTON]),NULL);

			if (pan->cb.stop_recording(pan->cb.user_data)) {
				pan->fileopened = FALSE;
				gtk_button_set_label(button, "Setup");
				gtk_widget_set_sensitive(GTK_WIDGET(pan->widgets[PAUSE_RECORDING_BUTTON]),FALSE);
			}
		}
	}



	return TRUE;
}


gboolean pause_recording_button_clicked_cb(GtkButton* button, gpointer data)
{
	int retcode = 0;
	EEGPanel* pan = GET_PANEL_FROM(button);

	(void)data;


	if (pan->cb.toggle_recording) {
		retcode = pan->cb.toggle_recording(pan->recording ? 0 : 1, pan->cb.user_data);
		pan->recording = !pan->recording;
	}
	
	if (retcode) {
		gtk_led_set_state(GTK_LED(pan->widgets[RECORDING_LED]), pan->recording);
		gtk_button_set_label(button,pan->recording ? "Pause": "Record");
	}

	return retcode ? TRUE : FALSE;
}

void reftype_combo_changed_cb(GtkComboBox* combo, gpointer data)
{
	GtkTreeIter iter;
	GValue value;
	GtkTreeModel* model;
	RefType type;
	EEGPanel* pan = GET_PANEL_FROM(combo);

	(void)data;
	
	// Get the value set
	memset(&value, 0, sizeof(value));
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	type = g_value_get_uint(&value);

	gtk_widget_set_sensitive(GTK_WIDGET(pan->widgets[ELECREF_COMBO]), (type==ELECTRODE_REF)? TRUE : FALSE);

	pan->eeg_ref_type = type;
}


void refelec_combo_changed_cb(GtkComboBox* combo, gpointer data)
{
	EEGPanel* pan = GET_PANEL_FROM(combo);
	(void)data;

	pan->eeg_ref_elec = gtk_combo_box_get_active(combo);
}

void elecset_combo_changed_cb(GtkComboBox* combo, gpointer data)
{
	int selec, nmax, first, last;
	GtkTreeSelection* tree_selection;
	GtkTreePath *path1, *path2;
	EEGPanel* pan = GET_PANEL_FROM(combo);
	(void)data;

	nmax = pan->nmax_eeg;
	if (!nmax)
		return;

	tree_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pan->widgets[EEG_TREEVIEW]));

	selec = gtk_combo_box_get_active(combo);

	if (selec==0) 
		gtk_tree_selection_select_all(tree_selection);
	else {
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
	char** labels;
	ChannelSelection select;
	ChannelType type = (ChannelType)user_data;
	EEGPanel* pan = GET_PANEL_FROM(gtk_tree_selection_get_tree_view(selection));

	labels = (type == EEG) ? pan->eeg_labels : pan->bipole_labels;
	
	// Prepare the channel selection structure to be passed
	fill_selec_from_treeselec(&select, selection, labels);

	g_mutex_lock(pan->data_mutex);
	if (!pan->cb.process_selection || (pan->cb.process_selection(&select, type, pan->cb.user_data) > 0)) {
		if (type == EEG)
			set_data_input(pan, -1, &select, NULL);
		else
			set_data_input(pan, -1, NULL, &select);
	}
	g_mutex_unlock(pan->data_mutex);

	// free everything holded by the selection struct
	g_free(select.selection);
	g_free(select.labels);
}

void decimation_combo_changed_cb(GtkComboBox* combo, gpointer data)
{
	char tempstr[32];
	GtkTreeIter iter;
	GValue value;
	GtkTreeModel* model;
	EEGPanel* pan = GET_PANEL_FROM(combo);
	(void)data;

	// Get the value set
	memset(&value, 0, sizeof(value));
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);


	// Reset the internals
	g_mutex_lock(pan->data_mutex);
	pan->decimation_factor = g_value_get_uint(&value);
	set_data_input(pan, (pan->display_length * pan->sampling_rate)/pan->decimation_factor, NULL, NULL);
	g_mutex_unlock(pan->data_mutex);
	
	// update the display
	sprintf(tempstr,"%u Hz",pan->sampling_rate/pan->decimation_factor); 
	gtk_label_set_text(GTK_LABEL(pan->widgets[DISPLAYED_FREQ_LABEL]), tempstr);


}

void scale_combo_changed_cb(GtkComboBox* combo, gpointer user_data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value;
	double scale;
	ScopeType type = (ChannelType)user_data;
	EEGPanel* pan = GET_PANEL_FROM(combo);
	
	// Get the value set
	memset(&value, 0, sizeof(value));
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	scale = g_value_get_double(&value);

	switch (type) {
	case ELEC_TYPE:
		g_object_set(pan->widgets[EEG_SCOPE], "scale", scale, NULL);
		break;

	case BIPOLE_TYPE:
		g_object_set(pan->widgets[EXG_SCOPE], "scale", scale, NULL);
		break;

	case OFFSET_TYPE:
		g_object_set(pan->widgets[EEG_OFFSET_BAR1], "min-value", -scale, "max-value", scale, NULL);
		g_object_set(pan->widgets[EEG_OFFSET_BAR2], "min-value", -scale, "max-value", scale, NULL);
		set_bargraphs_yticks(pan, scale);
		break;
	}
}

void time_window_combo_changed_cb(GtkComboBox* combo, gpointer user_data)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value;
	unsigned int num_samples;
	float time_length;
	EEGPanel* pan = GET_PANEL_FROM(combo);
	(void)user_data;

	g_mutex_lock(pan->data_mutex);
	
	// Get the value set
	memset(&value, 0, sizeof(value));
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	time_length = g_value_get_float(&value);

	pan->display_length = time_length;
	num_samples = (unsigned int)(time_length * (float)(pan->sampling_rate / pan->decimation_factor));
	set_data_input(pan, num_samples, NULL, NULL);
	set_scopes_xticks(pan);

	g_mutex_unlock(pan->data_mutex);
}

void filter_button_changed_cb(GtkButton* button, gpointer user_data)
{
	FilterParam options[NUM_FILTERS];
	float eeg_low_fc, eeg_high_fc, exg_low_fc, exg_high_fc;
	int eeg_low_state, eeg_high_state, exg_low_state, exg_high_state;
	float fs;
	(void)user_data;

	EEGPanel* pan = GET_PANEL_FROM(button);

	// Get the cut-off frequencies specified by the spin buttons
	eeg_low_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(pan->widgets[EEG_LOWPASS_SPIN]));
	eeg_high_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(pan->widgets[EEG_HIGHPASS_SPIN]));
	exg_low_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(pan->widgets[EXG_LOWPASS_SPIN]));
	exg_high_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(pan->widgets[EXG_HIGHPASS_SPIN]));

	// Retrieve the state of the check boxes
	eeg_low_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pan->widgets[EEG_LOWPASS_CHECK]));
	eeg_high_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pan->widgets[EEG_HIGHPASS_CHECK]));
	exg_low_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pan->widgets[EXG_LOWPASS_CHECK]));
	exg_high_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pan->widgets[EXG_HIGHPASS_CHECK]));


	g_mutex_lock(pan->data_mutex);
	fs = pan->sampling_rate / pan->decimation_factor;
	memcpy(options, pan->filter_param, sizeof(options));
	
	// Set the cutoff frequencies of every filters
	options[EEG_LOWPASS_FILTER].freq = eeg_low_fc;
	options[EEG_LOWPASS_FILTER].state = eeg_low_state;
	options[EEG_HIGHPASS_FILTER].freq = eeg_high_fc;
	options[EEG_HIGHPASS_FILTER].state = eeg_high_state;
	options[EXG_LOWPASS_FILTER].freq = exg_low_fc;
	options[EXG_LOWPASS_FILTER].state = exg_low_state;
	options[EXG_HIGHPASS_FILTER].freq = exg_high_fc;
	options[EXG_HIGHPASS_FILTER].state = exg_high_state;
	
	
	memcpy(pan->filter_param, options, sizeof(options));
	set_data_input(pan, -1, NULL, NULL);
//	set_all_filters(pan, options);
	g_mutex_unlock(pan->data_mutex);
}


gboolean check_redraw_scopes(gpointer user_data)
{
	EEGPanel* pan = user_data;
	unsigned int curr_sample, last_sample;

	// Redraw all the scopes
	g_mutex_lock(pan->data_mutex);

	curr_sample = pan->current_sample;
	last_sample = pan->last_drawn_sample;

	if (curr_sample != last_sample) {
		scope_update_data(pan->eeg_scope, curr_sample);
		scope_update_data(pan->exg_scope, curr_sample);
		binary_scope_update_data(pan->tri_scope, curr_sample);
		bargraph_update_data(pan->eeg_offset_bar1, 0);
		bargraph_update_data(pan->eeg_offset_bar2, 0);

		g_object_set(pan->widgets[CMS_LED], "state", (gboolean)(pan->flags.cms_in_range), NULL);
		g_object_set(pan->widgets[BATTERY_LED], "state", (gboolean)(pan->flags.low_battery), NULL);

		pan->last_drawn_sample = curr_sample;
	}

	g_mutex_unlock(pan->data_mutex);

	// Run modal dialog
	if (pan->dialog) {
		pan->dlg_retval = gtk_dialog_run(pan->dialog);
		g_mutex_unlock(pan->dlg_completion_mutex);
	}

	return TRUE;
}


///////////////////////////////////////////////////
//
//	API functions
//
///////////////////////////////////////////////////
void init_eegpanel_lib(int *argc, char ***argv)
{
	g_thread_init(NULL);
	gdk_threads_init();
	gtk_init(argc, argv);
}

EEGPanel* eegpanel_create(const char* uifilename, const char* settingsfilename, const struct PanelCb* cb)
{
	GtkBuilder* builder;
	EEGPanel* pan = NULL;
	GError* error = NULL;
	guint success = 0;

	RegisterCustomDefinition();

	// Allocate memory for the structures
	pan = g_malloc0(sizeof(*pan));
	pan->data_mutex = g_mutex_new();
	pan->dialog_mutex = g_mutex_new();
	pan->dlg_completion_mutex = g_mutex_new();
	g_mutex_lock(pan->dlg_completion_mutex);

	// Set callbacks
	if (cb)
		memcpy(&(pan->cb), cb, sizeof(*cb));
	if (pan->cb.user_data == NULL)
		pan->cb.user_data = pan;

	// Create the pan widgets according to the ui definition files
	builder = gtk_builder_new();
	if (uifilename)
		success = gtk_builder_add_from_file(builder, uifilename, &error);
	else
		success = gtk_builder_add_from_string(builder, str_default_ui_, -1, &error);
	
	if (success == 0) {
		fprintf(stderr,"%s\n",error->message);
		eegpanel_destroy(pan);
		return NULL;
	}
		
	gtk_builder_connect_signals(builder, pan);

	// Get the pointers of the control widgets
	if (poll_widgets(pan, builder)) {
		// Needed initializations
		pan->display_length = 1.0f;
		pan->decimation_factor = 1;
		initialize_all_filters(pan);
	
		// Initialize the content of the widgets
		if (settingsfilename)
			set_default_values(pan, settingsfilename);
		initialize_widgets(pan, builder);
		get_initial_values(pan);
		eegpanel_define_input(pan, 0, 0, 16, 2048);
		set_scopes_xticks(pan);

//		g_idle_add(check_redraw_scopes, pan);
		g_timeout_add(REFRESH_INTERVAL, check_redraw_scopes, pan);
	} else {
		eegpanel_destroy(pan);
		pan = NULL;
	}

	g_object_unref(builder);

	return pan;
}

void eegpanel_show(EEGPanel* pan, int state)
{
	int lock_res = 0;

	if ((pan->main_loop_thread) && (pan->main_loop_thread != g_thread_self()))
		lock_res = 1;
	
	//////////////////////////////////////////////////////////////
	//		WARNING
	//	Possible deadlock here if called from another thread
	//	than the main loop thread
	/////////////////////////////////////////////////////////////
	if (lock_res)
		gdk_threads_enter();

	if (state)
		gtk_widget_show_all(GTK_WIDGET(pan->window));
	else
		gtk_widget_hide_all(GTK_WIDGET(pan->window));

	if (lock_res)
		gdk_threads_leave();
}



void eegpanel_run(EEGPanel* pan, int nonblocking)
{

	if (!nonblocking) {
		pan->main_loop_thread = g_thread_self();
		gtk_main();
		return;
	}
	
	pan->main_loop_thread = g_thread_create(loop_thread, NULL, TRUE, NULL);
	return;
}


void eegpanel_destroy(EEGPanel* pan)
{
	// Stop refreshing the scopes content
	g_source_remove_by_user_data(pan);

	// Stop recording
	if (pan->recording && pan->cb.toggle_recording)
		pan->cb.toggle_recording(0, pan->cb.user_data);
	if (pan->fileopened && pan->cb.stop_recording)
		pan->cb.stop_recording(pan->cb.user_data);

	// Disconnect the system if applicable
	if (pan->connected && pan->cb.system_connection)
		pan->cb.system_connection(FALSE, pan->cb.user_data);

	// If called from another thread than the one of the main loop
	// wait for the termination of the main loop
	if ((pan->main_loop_thread) && (pan->main_loop_thread != g_thread_self()))
		g_thread_join(pan->main_loop_thread);

	g_mutex_free(pan->dialog_mutex);
	g_mutex_unlock(pan->dlg_completion_mutex);
	g_mutex_free(pan->dlg_completion_mutex);

	g_mutex_free(pan->data_mutex);
	g_strfreev(pan->eeg_labels);
	g_strfreev(pan->exg_labels);
	g_strfreev(pan->bipole_labels);

	
	g_free(pan->eeg);
	g_free(pan->exg);
	g_free(pan->selected_eeg);
	g_free(pan->selected_exg);
	g_free(pan->triggers);
	
	g_free(pan);
}


int eegpanel_define_input(EEGPanel* pan, unsigned int num_eeg_channels,
				unsigned int num_exg_channels, unsigned int num_tri_lines,
				unsigned int sampling_rate)
{
	char tempstr[32];
	int num_samples;
	ChannelSelection chann_selec;
	
	pan->nmax_eeg = num_eeg_channels;
	pan->nmax_exg = num_exg_channels;
	pan->nlines_tri = num_tri_lines;
	pan->sampling_rate = sampling_rate;
	num_samples = (pan->display_length * sampling_rate)/pan->decimation_factor;
	 

	// Add default channel labels if not available
	pan->eeg_labels = add_default_labels(pan->eeg_labels, num_eeg_channels, "EEG");
	pan->exg_labels = add_default_labels(pan->exg_labels, num_exg_channels, "EXG");
	set_bipole_labels(pan);

	// Update widgets
	fill_treeview(GTK_TREE_VIEW(pan->widgets[EEG_TREEVIEW]), pan->nmax_eeg, (const char**)pan->eeg_labels);
	fill_combo(GTK_COMBO_BOX(pan->widgets[ELECREF_COMBO]), pan->eeg_labels, pan->nmax_eeg);
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->widgets[ELECREF_COMBO]), 0);
	fill_treeview(GTK_TREE_VIEW(pan->widgets[EXG_TREEVIEW]), pan->nmax_exg, (const char**)pan->bipole_labels);
	sprintf(tempstr,"%u Hz",sampling_rate);
	gtk_label_set_text(GTK_LABEL(pan->widgets[NATIVE_FREQ_LABEL]), tempstr);
	sprintf(tempstr,"%u Hz",sampling_rate/pan->decimation_factor); 
	gtk_label_set_text(GTK_LABEL(pan->widgets[DISPLAYED_FREQ_LABEL]), tempstr);

	// Reset all data buffer;
	chann_selec.num_chann = 0;
	chann_selec.selection = NULL;
	chann_selec.labels = NULL;
	return set_data_input(pan, num_samples, &chann_selec, &chann_selec);
}


void eegpanel_add_samples(EEGPanel* pan, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples)
{
	g_mutex_lock(pan->data_mutex);
	add_samples(pan, eeg, exg, triggers, num_samples);
	g_mutex_unlock(pan->data_mutex);
}


void eegpanel_popup_message(EEGPanel* pan, const char* message)
{
	GtkWidget* dialog;

	dialog = gtk_message_dialog_new(pan->window,
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_CLOSE,
					"%s", message);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

char* eegpanel_open_filename_dialog(EEGPanel* pan, const char* filter, const char* filtername)
{
	GtkWidget* dialog;
	char* filename = NULL;
	GtkFileFilter* ffilter;

	dialog = gtk_file_chooser_dialog_new("Choose a filename",
					     pan->window,
					     GTK_FILE_CHOOSER_ACTION_SAVE,
					     GTK_STOCK_OK,GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
					     NULL);
	if (filter && filtername) {
		ffilter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(ffilter, filter);
		gtk_file_filter_set_name(ffilter, filtername);
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ffilter);
	}
	ffilter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(ffilter, "*");
	gtk_file_filter_set_name(ffilter, "any files");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ffilter);


	if (gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT) {
		gchar* retfile = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (retfile) {
			filename = malloc(strlen(retfile));
			strcpy(filename, retfile);
			g_free(retfile);
		}
	}
	gtk_widget_destroy(dialog);

	return filename;
}
///////////////////////////////////////////////////
//
//	Internal functions
//
///////////////////////////////////////////////////

void get_initial_values(EEGPanel* pan)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value;
	GtkComboBox* combo;

	// Get the display length
	combo = GTK_COMBO_BOX(pan->widgets[TIME_WINDOW_COMBO]);
	memset(&value, 0, sizeof(value));
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	pan->display_length = g_value_get_float(&value);
}

gpointer loop_thread(gpointer user_data)
{
	(void)user_data;

	gtk_main();
	return 0;
}

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



int poll_widgets(EEGPanel* pan, GtkBuilder* builder)
{
	unsigned int i;

	// Get the list of mandatory widgets and check their type;
	for (i=0; i<NUM_PANEL_WIDGETS_REGISTERED; i++) {
		int id = widget_name_table[i].id;
		const char* name = widget_name_table[i].name;
		GType type = g_type_from_name(widget_name_table[i].type);

		pan->widgets[id] = gtk_builder_get_object(builder, name);
		if (!pan->widgets[id] || !g_type_is_a(G_OBJECT_TYPE(pan->widgets[id]), type)) {
			fprintf(stderr, "Widget \"%s\" not found or is not a derived type of %s\n", name, widget_name_table[i].type);
			return 0;
		}
	}

	// Get the root window and pointer to the pan as internal
	// data to retrieve it later easily in the callbacks
	pan->window = GTK_WINDOW(pan->widgets[TOP_WINDOW]);
	g_object_set_data(G_OBJECT(pan->window), "eeg_panel", pan);
	
	// Get the plot widgets
	pan->eeg_scope = SCOPE(gtk_builder_get_object(builder, "eeg_scope"));
	pan->exg_scope = SCOPE(gtk_builder_get_object(builder, "exg_scope"));
	pan->eeg_offset_bar1 = BARGRAPH(gtk_builder_get_object(builder, "eeg_offset_bar1"));
	pan->eeg_offset_bar2 = BARGRAPH(gtk_builder_get_object(builder, "eeg_offset_bar2"));
	pan->tri_scope = BINARY_SCOPE(gtk_builder_get_object(builder, "tri_scope"));

	return 1;	
}

int initialize_widgets(EEGPanel* pan, GtkBuilder* builder)
{
	GtkTreeView* treeview;
	(void)builder;
	
	g_signal_connect(pan->widgets[STARTACQUISITION_BUTTON], "clicked", (GCallback)startacquisition_button_clicked_cb, NULL);
	g_signal_connect(pan->widgets[START_RECORDING_BUTTON], "clicked", (GCallback)start_recording_button_clicked_cb, NULL);
	g_signal_connect(pan->widgets[PAUSE_RECORDING_BUTTON], "clicked", (GCallback)pause_recording_button_clicked_cb, NULL);
	gtk_widget_set_sensitive(GTK_WIDGET(pan->widgets[PAUSE_RECORDING_BUTTON]),FALSE);

	treeview = GTK_TREE_VIEW(pan->widgets[EEG_TREEVIEW]);
	initialize_list_treeview(treeview, "channels");
	g_signal_connect_after(gtk_tree_view_get_selection(treeview), "changed", (GCallback)channel_selection_changed_cb, (gpointer)EEG);

	treeview = GTK_TREE_VIEW(pan->widgets[EXG_TREEVIEW]);
	initialize_list_treeview(treeview, "channels");
	g_signal_connect_after(gtk_tree_view_get_selection(treeview), "changed", (GCallback)channel_selection_changed_cb, (gpointer)EXG);

	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->widgets[DECIMATION_COMBO]), 0);
	g_signal_connect_after(pan->widgets[DECIMATION_COMBO],	"changed", (GCallback)decimation_combo_changed_cb, NULL);
	
	// Scale combos
	g_signal_connect(pan->widgets[EEG_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)ELEC_TYPE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->widgets[EEG_SCALE_COMBO]), 0);
	g_signal_connect(pan->widgets[OFFSET_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)OFFSET_TYPE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->widgets[OFFSET_SCALE_COMBO]), 0);
	g_signal_connect(pan->widgets[EXG_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)BIPOLE_TYPE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->widgets[EXG_SCALE_COMBO]), 0);

	// Time window combo
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->widgets[TIME_WINDOW_COMBO]), 0);
	g_signal_connect_after(pan->widgets[TIME_WINDOW_COMBO], "changed", (GCallback)time_window_combo_changed_cb, (gpointer)ELEC_TYPE);

	// filter buttons
	g_object_set(pan->widgets[EEG_LOWPASS_CHECK], "active", pan->filter_param[EEG_LOWPASS_FILTER].state, NULL);
	g_object_set(pan->widgets[EEG_LOWPASS_SPIN], "value", pan->filter_param[EEG_LOWPASS_FILTER].freq, NULL);
	g_object_set(pan->widgets[EEG_HIGHPASS_CHECK], "active", pan->filter_param[EEG_HIGHPASS_FILTER].state, NULL);
	g_object_set(pan->widgets[EEG_HIGHPASS_SPIN], "value", pan->filter_param[EEG_HIGHPASS_FILTER].freq, NULL);
	g_object_set(pan->widgets[EXG_LOWPASS_CHECK], "active", pan->filter_param[EXG_LOWPASS_FILTER].state, NULL);
	g_object_set(pan->widgets[EXG_LOWPASS_SPIN], "value", pan->filter_param[EXG_LOWPASS_FILTER].freq, NULL);
	g_object_set(pan->widgets[EXG_HIGHPASS_CHECK], "active", pan->filter_param[EXG_HIGHPASS_FILTER].state, NULL);
	g_object_set(pan->widgets[EXG_HIGHPASS_SPIN], "value", pan->filter_param[EXG_HIGHPASS_FILTER].freq, NULL);
	g_signal_connect_after(pan->widgets[EEG_LOWPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->widgets[EEG_HIGHPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->widgets[EXG_LOWPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->widgets[EXG_HIGHPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->widgets[EEG_LOWPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->widgets[EEG_HIGHPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->widgets[EXG_LOWPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->widgets[EXG_HIGHPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);

	// reference combos
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->widgets[REFTYPE_COMBO]), 0);
	g_signal_connect(pan->widgets[REFTYPE_COMBO], "changed", (GCallback)reftype_combo_changed_cb, NULL);
	g_signal_connect(pan->widgets[ELECREF_COMBO], "changed", (GCallback)refelec_combo_changed_cb, NULL);

	g_signal_connect(pan->widgets[ELECSET_COMBO], "changed", (GCallback)elecset_combo_changed_cb, NULL);
	return 1;
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
	unsigned int i = 0;
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

void add_samples(EEGPanel* pan, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples)
{
	unsigned int num_eeg_ch, num_exg_ch, nmax_eeg, nmax_exg;
	unsigned int num_samples_written = 0;
	unsigned int pointer = pan->current_sample;
	unsigned int *eeg_sel, *exg_sel; 
	void* buff = NULL;
	unsigned int buff_len;

	num_eeg_ch = pan->num_eeg_channels;
	num_exg_ch = pan->num_exg_channels;
	nmax_eeg = pan->nmax_eeg;
	nmax_exg = pan->nmax_exg;
	eeg_sel = pan->selected_eeg;
	exg_sel = pan->selected_exg;

	

	// if we need to wrap, first add the tail
	if (num_samples+pointer > pan->num_samples) {
		num_samples_written = pan->num_samples - pointer;
		
		add_samples(pan, eeg, exg, triggers, num_samples_written);

		eeg += nmax_eeg*num_samples_written;
		exg += nmax_exg*num_samples_written;
		triggers += num_samples_written;
		num_samples -= num_samples_written;
		pointer = pan->current_sample;
	}
	
	// Create a buffer big enough to hold incoming eeg or exg data
	buff_len = (num_eeg_ch > num_exg_ch) ? num_eeg_ch : num_exg_ch;
	buff_len *= num_samples;
	buff = g_malloc(buff_len*sizeof(*eeg));

	// copy data
	if (eeg) 
		process_eeg(pan, eeg, buff, num_samples);
	if (exg) 
		process_exg(pan, exg, buff, num_samples);
	if (triggers)
		process_tri(pan, triggers, buff, num_samples);

	g_free(buff);

	// Update current pointer
	pointer += num_samples;
	if (pointer >= pan->num_samples)
		pointer -= pan->num_samples;
	pan->current_sample = pointer;
}


int set_data_input(EEGPanel* pan, int num_samples, ChannelSelection* eeg_selec, ChannelSelection* exg_selec)
{
	unsigned int num_eeg_points, num_exg_points, num_tri_points, num_eeg, num_exg;
	unsigned int num_elec_bar1, num_elec_bar2;
	float *eeg, *exg;
	uint32_t *triggers;

	pan->decimation_offset = 0;

	// Use the previous values if unspecified
	num_samples = (num_samples>=0) ? num_samples : (int)(pan->num_samples);
	num_eeg = eeg_selec ? eeg_selec->num_chann : pan->num_eeg_channels;
	num_exg = exg_selec ? exg_selec->num_chann : pan->num_exg_channels;

	num_eeg_points = num_samples*num_eeg;
	num_exg_points = num_samples*num_exg;
	num_tri_points = num_samples;
	num_elec_bar2 = num_eeg/2;
	num_elec_bar1 = num_eeg - num_elec_bar2;

	if (num_eeg_points != pan->num_eeg_channels*pan->num_samples) {
		eeg = g_malloc0(num_eeg_points * sizeof(*eeg));
		g_free(pan->eeg);
		pan->eeg = eeg;
	}
	memset(pan->eeg, 0, num_eeg_points*sizeof(*eeg));
	scope_set_data(pan->eeg_scope, pan->eeg, num_samples, num_eeg);

	if (num_exg_points != pan->num_exg_channels*pan->num_samples) {
		exg = g_malloc0(num_exg_points * sizeof(*exg));
		g_free(pan->exg);
		pan->exg = exg;
	}
	scope_set_data(pan->exg_scope, pan->exg, num_samples, num_exg);

	if (num_tri_points != pan->num_samples) {
		triggers = g_malloc0(num_tri_points * sizeof(*triggers));
		g_free(pan->triggers);
		pan->triggers = triggers;
	}
	binary_scope_set_data(pan->tri_scope, pan->triggers, num_samples, pan->nlines_tri);

	// Store the eeg selection and update the labels
	if (eeg_selec) {
		if (num_eeg != pan->num_eeg_channels) {
			g_free(pan->selected_eeg);
			g_free(pan->eeg_offset);
			pan->selected_eeg = g_malloc(num_eeg*sizeof(*(pan->selected_eeg)));
			pan->eeg_offset = g_malloc0(num_eeg*sizeof(*(pan->eeg_offset)));
			pan->num_eeg_channels = num_eeg;
		}
		memcpy(pan->selected_eeg, eeg_selec->selection, num_eeg*sizeof(*(pan->selected_eeg)));
	}


	// Store the exg selection
	if (exg_selec) {
		if (num_exg != pan->num_exg_channels) {
			g_free(pan->selected_exg);
			g_free(pan->exg_offset);
			pan->selected_exg = g_malloc(num_exg*sizeof(*(pan->selected_exg)));
			pan->exg_offset = g_malloc0(num_exg*sizeof(*(pan->exg_offset)));
			pan->num_exg_channels = num_exg;
		}
		memcpy(pan->selected_exg, exg_selec->selection, num_exg*sizeof(*(pan->selected_exg)));
	}

	// Set bargraphs data
	bargraph_set_data(pan->eeg_offset_bar1, pan->eeg_offset, num_elec_bar1);
	bargraph_set_data(pan->eeg_offset_bar2, pan->eeg_offset+num_elec_bar1, num_elec_bar2);

	// Update the filters
	set_all_filters(pan, NULL);
	
	// Update the labels
	if ((eeg_selec) && (eeg_selec->labels)) {	
		g_object_set(pan->widgets[EEG_AXES], "ytick-labelv", eeg_selec->labels, NULL);
		g_object_set(pan->widgets[EEG_OFFSET_AXES1], "xtick-labelv", eeg_selec->labels, NULL);	
		g_object_set(pan->widgets[EEG_OFFSET_AXES2], "xtick-labelv", eeg_selec->labels+num_elec_bar1, NULL);	
	}
	if ((exg_selec) && (exg_selec->labels))	
			g_object_set(pan->widgets[EXG_AXES], "ytick-labelv", exg_selec->labels, NULL);


	pan->last_drawn_sample = pan->current_sample = 0;
	pan->num_samples = num_samples;
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

void clean_selec(ChannelSelection* selection)
{
	if (!selection)
		return;

	g_free(selection->selection);
	g_free(selection->labels);
	selection->selection = NULL;
	selection->labels = NULL;
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

void set_bipole_labels(EEGPanel* pan)
{
	unsigned int i, j;
	char** bip_labels = NULL;
	unsigned int num_labels = pan->nmax_exg;

	g_strfreev(pan->bipole_labels);
	bip_labels = g_malloc0((num_labels+1)*sizeof(char*));

	for (i=0; i<num_labels;i++) {
		j = (i+1)%num_labels;
		bip_labels[i] = g_strconcat(pan->exg_labels[i],
					    "-",
					    pan->exg_labels[j],
					    NULL);
	}

	pan->bipole_labels = bip_labels;
}


void set_scopes_xticks(EEGPanel* pan)
{
	unsigned int i, value;
	unsigned int* ticks;
	char** labels;
	unsigned int inc, num_ticks;
	float disp_len = pan->display_length;
	unsigned int sampling_rate = pan->sampling_rate / pan->decimation_factor;

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
		labels[i] = g_strdup_printf("%us",value);
	}

	// Set the ticks to the scope widgets
	scope_set_ticks(pan->eeg_scope, num_ticks, ticks);
	g_object_set(pan->widgets[EEG_AXES], "xtick-labelv", labels, NULL);
	binary_scope_set_ticks(pan->tri_scope, num_ticks, ticks);
	g_object_set(pan->widgets[TRI_AXES], "xtick-labelv", labels, NULL);
	scope_set_ticks(pan->exg_scope, num_ticks, ticks);
	g_object_set(pan->widgets[EXG_AXES], "xtick-labelv", labels, NULL);


	g_free(ticks);
	g_strfreev(labels);
}

void set_bargraphs_yticks(EEGPanel* pan, float max)
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
	bargraph_set_ticks(pan->eeg_offset_bar1, num_ticks, ticks);
	bargraph_set_ticks(pan->eeg_offset_bar2, num_ticks, ticks);
	g_object_set(pan->widgets[EEG_OFFSET_AXES1], "ytick-labelv", labels, NULL);
	g_object_set(pan->widgets[EEG_OFFSET_AXES2], "ytick-labelv", labels, NULL);


	g_free(ticks);
	g_strfreev(labels);
}


void set_one_filter(EEGPanel* pan, EnumFilter type, FilterParam* options, unsigned int nchann, int highpass)
{
	float fs = pan->sampling_rate;
	dfilter* filt = pan->filt[type];
	FilterParam* curr_param =  &(pan->filter_param[type]);
	FilterParam* param = options + type;

	
	if (param->state) {
		if (!filt || ((param->freq/fs!=curr_param->fc) || (filt->num_chann != nchann))) {
			destroy_filter(filt);
			param->fc = param->freq/fs;
			filt = create_butterworth_filter(param->fc, 2, nchann, highpass);
			pan->filter_param[type] = *param;
		}
		else
			reset_filter(filt);
	}
	else {
		destroy_filter(filt);
		filt = NULL;
	}

	pan->filt[type] = filt;
}


void set_all_filters(EEGPanel* pan, FilterParam* options)
{
	unsigned int num_eeg = pan->num_eeg_channels;
	unsigned int num_exg = pan->num_exg_channels;
	unsigned int dec_state = (pan->decimation_factor != 1) ? 1 : 0;

	if (options==NULL)
		options = pan->filter_param;

	options[EEG_DECIMATION_FILTER].state = dec_state;
	options[EXG_DECIMATION_FILTER].state = dec_state;

	// Set the filters
	set_one_filter(pan, EEG_LOWPASS_FILTER, options, num_eeg, 0);
	set_one_filter(pan, EEG_HIGHPASS_FILTER, options, num_eeg, 1);
	set_one_filter(pan, EXG_LOWPASS_FILTER, options, num_exg, 0);
	set_one_filter(pan, EXG_HIGHPASS_FILTER, options, num_exg, 1);

	set_one_filter(pan, EEG_DECIMATION_FILTER, options, num_eeg, 0);
	set_one_filter(pan, EXG_DECIMATION_FILTER, options, num_exg, 0);

	set_one_filter(pan, EEG_OFFSET_FILTER, options, num_eeg, 0);
	set_one_filter(pan, EXG_OFFSET_FILTER, options, num_exg, 0);
}


void initialize_all_filters(EEGPanel* pan)
{
	FilterParam* options = pan->filter_param;
	float fs = pan->sampling_rate;
	int dec_state = (pan->decimation_factor!=1) ? 1 : 0;
	float dec_fc = 0.4*fs/pan->decimation_factor;
	
	
	memset(options, 0, sizeof(options));

	options[EEG_OFFSET_FILTER].state = 1;
	options[EEG_OFFSET_FILTER].freq = 1.0;
	options[EXG_OFFSET_FILTER].state = 1;
	options[EXG_OFFSET_FILTER].freq = 1.0;
	options[EEG_DECIMATION_FILTER].freq = dec_fc;
	options[EEG_DECIMATION_FILTER].state = dec_state;
	options[EXG_DECIMATION_FILTER].freq = dec_fc;
	options[EXG_DECIMATION_FILTER].state = dec_state;
	options[EEG_LOWPASS_FILTER].state = 0;
	options[EEG_LOWPASS_FILTER].freq = 0.1;
	options[EEG_HIGHPASS_FILTER].state = 0;
	options[EEG_HIGHPASS_FILTER].freq = 0.4*fs;
	options[EEG_LOWPASS_FILTER].state = 0;
	options[EEG_LOWPASS_FILTER].freq = 0.1;
	options[EEG_HIGHPASS_FILTER].state = 0;
	options[EEG_HIGHPASS_FILTER].freq = 0.4*fs;
}


#define SWAP_POINTERS(pointer1, pointer2)	do {	\
	void* temp = pointer2;				\
	pointer2 = pointer1;				\
	pointer1 = temp;				\
} while(0)


void process_eeg(EEGPanel* pan, const float* eeg, float* temp_buff, unsigned int n_samples)
{
	unsigned int i, j;
	unsigned int nchann = pan->num_eeg_channels;
	dfilter* filt;
	float* buff1, *buff2, *curr_eeg;
	unsigned int *sel = pan->selected_eeg;
	unsigned int num_ch = pan->num_eeg_channels;
	unsigned int nmax_ch = pan->nmax_eeg;
	
	buff1 = temp_buff;
	curr_eeg = pan->eeg + pan->current_sample*nchann;
	buff2 = curr_eeg;
 

	// Copy data of the selected channels
	for (i=0; i<n_samples; i++) {
		for (j=0; j<num_ch; j++)
			buff2[i*num_ch+j] = eeg[i*nmax_ch + sel[j]];
	}
	SWAP_POINTERS(buff1, buff2);

	
	filt = pan->filt[EEG_OFFSET_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		// copy last samples
		for (i=0; i<num_ch; i++)
			pan->eeg_offset[i] = buff2[nchann*(n_samples-1)+i];
	}
	
	// Do referencing
	if (pan->eeg_ref_type == AVERAGE_REF)
		remove_common_avg_ref(buff1, nchann, eeg, nmax_ch, n_samples);
	else if (pan->eeg_ref_type == ELECTRODE_REF)
		remove_electrode_ref(buff1, nchann, eeg, nmax_ch, n_samples, pan->eeg_ref_elec);

	// Process decimation
	// TODO
	
	

	filt = pan->filt[EEG_LOWPASS_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);		
	}

	filt = pan->filt[EEG_HIGHPASS_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);
	}
	
	// copy data to the destination buffer
	if (buff1 != curr_eeg)
		memcpy(curr_eeg, buff1, n_samples*nchann*sizeof(*buff1));
}


void process_exg(EEGPanel* pan, const float* exg, float* temp_buff, unsigned int n_samples)
{
	unsigned int i, j, k;
	unsigned int nchann = pan->num_exg_channels;
	dfilter* filt;
	float* buff1, *buff2, *curr_exg;
	unsigned int *sel = pan->selected_exg;
	unsigned int num_ch = pan->num_exg_channels;
	unsigned int nmax_ch = pan->nmax_exg;
	
	buff1 = temp_buff;
	curr_exg = pan->exg + pan->current_sample*nchann;
	buff2 = curr_exg;
 

	// Copy data of the selected channels
	for (i=0; i<n_samples; i++) {
		for (j=0; j<num_ch; j++) {
			k = (sel[j]+1)%nmax_ch;
			buff2[i*num_ch+j] = exg[i*nmax_ch + sel[j]] - exg[i*nmax_ch + k];
		}
	}
	SWAP_POINTERS(buff1, buff2);

	
	filt = pan->filt[EXG_OFFSET_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		// copy last samples
		for (i=0; i<num_ch; i++)
			pan->exg_offset[i] = buff2[nchann*(n_samples-1)+i];
	}

	// Process decimation
	// TODO

	filt = pan->filt[EXG_LOWPASS_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);		
	}

	filt = pan->filt[EXG_HIGHPASS_FILTER];
	if (filt) {
		filter(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);
	}
	
	// copy data to the destination buffer
	if (buff1 != curr_exg)
		memcpy(curr_exg, buff1, n_samples*nchann*sizeof(*buff1));
}

#define CMS_IN_RANGE	0x100000
#define LOW_BATTERY	0x400000
void process_tri(EEGPanel* pan, const uint32_t* tri, uint32_t* temp_buff, unsigned int n_samples)
{
	unsigned int i;
	uint32_t* buff1, *buff2, *curr_tri;
	
	buff1 = temp_buff;
	curr_tri = pan->triggers + pan->current_sample;
	buff2 = curr_tri;

	pan->flags.cms_in_range = 1;
	pan->flags.low_battery = 0;

	// Copy data and set the states of the system
	for (i=0; i<n_samples; i++) {
		buff2[i] = tri[i];
		if ((CMS_IN_RANGE & tri[i]) == 0)
			pan->flags.cms_in_range = 0;
		if (LOW_BATTERY & tri[i])
			pan->flags.low_battery = 1;
	}
}

void remove_common_avg_ref(float* data, unsigned int nchann, const float* fullset, unsigned int nchann_full, unsigned int num_samples)
{
	unsigned int i, j;
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
	unsigned int i, j;

	for (i=0; i<num_samples; i++) {
		// reference the data
		for (j=0; j<nchann; j++)
			data[i*nchann+j] -= fullset[i*nchann_full+elec_ref];
	}
}

void get_default_channel_labels(GKeyFile* key_file, char*** labels, const char* group_name, const char* prefix)
{
	char** keys;
	unsigned int max_index=0, i=0, index;

	// Get the max index
	keys = g_key_file_get_keys(key_file, group_name, NULL, NULL);

	if (!keys)
		return;

	while (keys[i]) {
		if ((sscanf(keys[i],"channel%u",&index) == 1) && (index>max_index))
			max_index = index;
		i++;
	}

	*labels = add_default_labels(*labels, max_index, prefix);

	// Get all the names
	i = 0;
	while (keys[i]) {
		if (sscanf(keys[i],"channel%u",&index) == 1) {
			index--;	// starting index is 1
			g_free((*labels)[index]);
			(*labels)[index] = g_key_file_get_string(key_file, group_name, keys[i], NULL);
		}
		i++;
	}
	g_strfreev(keys);
}

void set_default_values(EEGPanel* pan, const char* filename)
{
	GKeyFile* key_file;

	key_file = g_key_file_new();
	if (!g_key_file_load_from_file(key_file, "settings.ini", G_KEY_FILE_NONE, NULL)) {
		g_key_file_free(key_file);
		return;
	}

	// Set filter default parameters
	pan->filter_param[EEG_LOWPASS_FILTER].state = g_key_file_get_boolean(key_file, "filtering", "EEGLowpassState", NULL);
	pan->filter_param[EEG_LOWPASS_FILTER].freq = g_key_file_get_double(key_file, "filtering", "EEGLowpassFreq", NULL);
	pan->filter_param[EEG_HIGHPASS_FILTER].state = g_key_file_get_boolean(key_file, "filtering", "EEGHighpassState", NULL);
	pan->filter_param[EEG_HIGHPASS_FILTER].freq = g_key_file_get_double(key_file, "filtering", "EEGHighpassFreq", NULL);
	pan->filter_param[EXG_LOWPASS_FILTER].state = g_key_file_get_boolean(key_file, "filtering", "EXGLowpassState", NULL);
	pan->filter_param[EXG_LOWPASS_FILTER].freq = g_key_file_get_double(key_file, "filtering", "EXGLowpassFreq", NULL);
	pan->filter_param[EXG_HIGHPASS_FILTER].state = g_key_file_get_boolean(key_file, "filtering", "EXGHighpassState", NULL);
	pan->filter_param[EXG_HIGHPASS_FILTER].freq = g_key_file_get_double(key_file, "filtering", "EXGHighpassFreq", NULL);

	// Get Channels name
	get_default_channel_labels(key_file, &(pan->eeg_labels), "eeg_channels", "EEG");
	get_default_channel_labels(key_file, &(pan->exg_labels), "exg_channels", "EXG");

	g_key_file_free(key_file);
}

gint run_model_dialog(EEGPanel* pan, GtkDialog* dialog)
{
	gint retval;

	if ((pan->main_loop_thread) && (pan->main_loop_thread != g_thread_self()))
		retval = gtk_dialog_run(dialog);
	else {
		g_mutex_lock(pan->dialog_mutex);		
		
		pan->dialog = dialog;

		g_mutex_lock(pan->dlg_completion_mutex);
		retval = pan->dlg_retval;

		g_mutex_unlock(pan->dialog_mutex);		
	}

	return retval;
}

