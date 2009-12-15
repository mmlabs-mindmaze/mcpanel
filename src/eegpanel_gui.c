/*
	Copyright (C) 2008-2009 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This file is part of the eegpanel library

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
#include "eegpanel.h"
#include "eegpanel_shared.h"
#include "eegpanel_sighandler.h"
#include "eegpanel_gui.h"
#include "default-ui.h"
#include <stdlib.h>
#include <string.h>

#define REFRESH_INTERVAL	30


const LinkWidgetName widget_name_table[] = {
	{TOP_WINDOW, "topwindow", "GtkWindow"},
	{EEG_SCOPE, "eeg_scope", "Scope"},
	{EXG_SCOPE, "exg_scope", "Scope"},
	{TRI_SCOPE, "tri_scope", "BinaryScope"},
	{EEG_OFFSET_BAR1, "eeg_offset_bar1", "Bargraph"},
	{EEG_OFFSET_BAR2, "eeg_offset_bar2", "Bargraph"},
	{EEG_AXES, "eeg_axes", "LabelizedPlot"},
	{EXG_AXES, "exg_axes", "LabelizedPlot"},
	{TRI_AXES, "tri_axes", "LabelizedPlot"},
	{EEG_OFFSET_AXES1, "eeg_offset_axes1", "LabelizedPlot"},
	{EEG_OFFSET_AXES2, "eeg_offset_axes2", "LabelizedPlot"},
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

int poll_widgets(EEGPanel* pan, GtkBuilder* builder);
int initialize_widgets(EEGPanel* pan);
GtkListStore* initialize_list_treeview(GtkTreeView* treeview, const gchar* attr_name);
void initialize_combo(GtkComboBox* combo);
int RegisterCustomDefinition(void);
gboolean blocking_funcall_cb(gpointer data);


gboolean check_redraw_scopes_cb(gpointer user_data)
{
	EEGPanel* pan = user_data;
	unsigned int curr, prev;
	GObject** widg = pan->gui.widgets;

	// Redraw all the scopes
	g_mutex_lock(pan->data_mutex);

	curr = pan->current_sample;
	prev = pan->last_drawn_sample;

	if (curr != prev) {
		scope_update_data(SCOPE(widg[EEG_SCOPE]), curr);
		scope_update_data(SCOPE(widg[EXG_SCOPE]), curr);
		binary_scope_update_data(BINARY_SCOPE(widg[TRI_SCOPE]), curr);
		bargraph_update_data(BARGRAPH(widg[EEG_OFFSET_BAR1]), 0);
		bargraph_update_data(BARGRAPH(widg[EEG_OFFSET_BAR2]), 0);

		g_object_set(pan->gui.widgets[CMS_LED], "state", (gboolean)(pan->flags.cms_in_range), NULL);
		g_object_set(pan->gui.widgets[BATTERY_LED], "state", (gboolean)(pan->flags.low_battery), NULL);

		pan->last_drawn_sample = curr;
	}

	g_mutex_unlock(pan->data_mutex);

	// Run modal dialog
	if (pan->dialog) {
		pan->dlg_retval = gtk_dialog_run(pan->dialog);
		g_mutex_unlock(pan->dlg_completion_mutex);
	}

	return TRUE;
}

int popup_message_dialog(struct DialogParam* dlgprm)
{
	GtkWidget* dialog;
	const char* message = dlgprm->str_in;

	dialog = gtk_message_dialog_new(dlgprm->gui->window,
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_CLOSE,
					"%s", message);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	return 0;
}

int open_filename_dialog(struct DialogParam* dlgprm)
{
	GtkWidget* dialog;
	char string[64];
	const char* currstr = dlgprm->str_in;

	dialog = gtk_file_chooser_dialog_new("Choose a filename",
					     dlgprm->gui->window,
					     GTK_FILE_CHOOSER_ACTION_SAVE,
					     GTK_STOCK_OK,GTK_RESPONSE_ACCEPT,
					     GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
					     NULL);
	
	while (sscanf(currstr, "%[^|]", string)) {
		GtkFileFilter* ffilter = gtk_file_filter_new();
		gtk_file_filter_set_name(ffilter, string);
		while (currstr = strchr(currstr, '|')) {
			if (currstr[1] == '|')
				break;	
			sscanf(++currstr, "%[^|]", string);
			gtk_file_filter_add_pattern(ffilter, string);
		}
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ffilter);
		if (!currstr)
			break;
		currstr += 2;
	}

	if (gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_ACCEPT) {
		gchar* retfile = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (retfile) {
			dlgprm->str_out = malloc(strlen(retfile)+1);
			strcpy(dlgprm->str_out, retfile);
			g_free(retfile);
		}
	}
	gtk_widget_destroy(dialog);

	return 0;
}

gboolean blocking_funcall_cb(gpointer data)
{
	struct BlockingCallParam* bcprm = data;

	// Run the function
	gdk_threads_enter();
	bcprm->retcode = bcprm->func(bcprm->data);
	gdk_threads_leave();

	// Signal that it is done
	g_mutex_lock(bcprm->mtx);
	bcprm->done = 1;
	g_cond_signal(bcprm->cond);
	g_mutex_unlock(bcprm->mtx);
	
	return FALSE;
}


int run_func_in_guithread(EEGPanel* pan, BCProc func, void* data)
{
	if (pan->main_loop_thread != g_thread_self()) {
		// Init synchronization objects
		// Warning: Assume that the creation will not fail
		struct BlockingCallParam bcprm = {
			.mtx = g_mutex_new(),
			.cond = g_cond_new(),
			.done = 0,
			.data = data,
			.func = func
		};

		// queue job into GTK main loop
		// and wait for completion
		g_mutex_lock(bcprm.mtx);
		g_idle_add(blocking_funcall_cb, &bcprm);
		while (!bcprm.done)
			g_cond_wait(bcprm.cond, bcprm.mtx);
		g_mutex_unlock(bcprm.mtx);

		// free sync objects
		g_mutex_free(bcprm.mtx);
		g_cond_free(bcprm.cond);

		return bcprm.retcode;
	}
	else
		return func(data);
}


/*************************************************************************
 *                                                                       *
 *                         GUI functions                                 *
 *                                                                       *
 *************************************************************************/
void get_initial_values(EEGPanel* pan)
{
	GtkTreeModel* model;
	GtkTreeIter iter;
	GValue value;
	GtkComboBox* combo;

	// Get the display length
	combo = GTK_COMBO_BOX(pan->gui.widgets[TIME_WINDOW_COMBO]);
	memset(&value, 0, sizeof(value));
	model = gtk_combo_box_get_model(combo);
	gtk_combo_box_get_active_iter(combo, &iter);
	gtk_tree_model_get_value(model, &iter, 1, &value);
	pan->display_length = g_value_get_float(&value);
}

int poll_widgets(EEGPanel* pan, GtkBuilder* builder)
{
	unsigned int i;
	struct PanelGUI* gui = &(pan->gui);

	// Get the list of mandatory widgets and check their type;
	for (i=0; i<NUM_PANEL_WIDGETS_REGISTERED; i++) {
		int id = widget_name_table[i].id;
		const char* name = widget_name_table[i].name;
		GType type = g_type_from_name(widget_name_table[i].type);

		gui->widgets[id] = gtk_builder_get_object(builder, name);
		if (!gui->widgets[id] || !g_type_is_a(G_OBJECT_TYPE(gui->widgets[id]), type)) {
			fprintf(stderr, "Widget \"%s\" not found or is not a derived type of %s\n", name, widget_name_table[i].type);
			return 0;
		}
	}

	// Get the root window and pointer to the pan as internal
	// data to retrieve it later easily in the callbacks
	gui->window = GTK_WINDOW(gui->widgets[TOP_WINDOW]);
	g_object_set_data(G_OBJECT(gui->window), "eeg_panel", pan);
	
	// Get the plot widgets
	gui->eeg_scope = SCOPE(gtk_builder_get_object(builder, "eeg_scope"));
	gui->exg_scope = SCOPE(gtk_builder_get_object(builder, "exg_scope"));
	gui->eeg_offset_bar1 = BARGRAPH(gtk_builder_get_object(builder, "eeg_offset_bar1"));
	gui->eeg_offset_bar2 = BARGRAPH(gtk_builder_get_object(builder, "eeg_offset_bar2"));
	gui->tri_scope = BINARY_SCOPE(gtk_builder_get_object(builder, "tri_scope"));

	return 1;	
}

int initialize_widgets(EEGPanel* pan)
{
	GtkTreeView* treeview;
	
	gtk_widget_set_sensitive(GTK_WIDGET(pan->gui.widgets[PAUSE_RECORDING_BUTTON]),FALSE);

	treeview = GTK_TREE_VIEW(pan->gui.widgets[EEG_TREEVIEW]);
	initialize_list_treeview(treeview, "channels");

	treeview = GTK_TREE_VIEW(pan->gui.widgets[EXG_TREEVIEW]);
	initialize_list_treeview(treeview, "channels");

	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[DECIMATION_COMBO]), 0);
	
	// Scale combos
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[EEG_SCALE_COMBO]), 0);
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[OFFSET_SCALE_COMBO]), 0);
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[EXG_SCALE_COMBO]), 0);

	// Time window combo
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[TIME_WINDOW_COMBO]), 0);

	// filter buttons
	g_object_set(pan->gui.widgets[EEG_LOWPASS_CHECK], "active", pan->filter_param[EEG_LOWPASS_FILTER].state, NULL);
	g_object_set(pan->gui.widgets[EEG_LOWPASS_SPIN], "value", pan->filter_param[EEG_LOWPASS_FILTER].freq, NULL);
	g_object_set(pan->gui.widgets[EEG_HIGHPASS_CHECK], "active", pan->filter_param[EEG_HIGHPASS_FILTER].state, NULL);
	g_object_set(pan->gui.widgets[EEG_HIGHPASS_SPIN], "value", pan->filter_param[EEG_HIGHPASS_FILTER].freq, NULL);
	g_object_set(pan->gui.widgets[EXG_LOWPASS_CHECK], "active", pan->filter_param[EXG_LOWPASS_FILTER].state, NULL);
	g_object_set(pan->gui.widgets[EXG_LOWPASS_SPIN], "value", pan->filter_param[EXG_LOWPASS_FILTER].freq, NULL);
	g_object_set(pan->gui.widgets[EXG_HIGHPASS_CHECK], "active", pan->filter_param[EXG_HIGHPASS_FILTER].state, NULL);
	g_object_set(pan->gui.widgets[EXG_HIGHPASS_SPIN], "value", pan->filter_param[EXG_HIGHPASS_FILTER].freq, NULL);

	// reference combos
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[REFTYPE_COMBO]), 0);

	connect_panel_signals(pan);

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

int fill_selec_from_treeselec(ChannelSelection* sel, GtkTreeSelection* tree_sel, char** labels)
{
	int num = gtk_tree_selection_count_selected_rows(tree_sel);
	GList *list, *elem;
	int i;
	unsigned int* chann_select = NULL;
	const char** labelv = NULL;

	// Allocation
	chann_select = malloc(num*sizeof(*chann_select));

	// Fill the array of the selected channels
	elem = list = gtk_tree_selection_get_selected_rows(tree_sel, NULL);
	for(i=0; i<num; i++) {
		chann_select[i] = *gtk_tree_path_get_indices((GtkTreePath*)(elem->data));
		elem = g_list_next(elem);
	}
	g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(list);

	// Add labels name if provided
	if (labels) {
		labelv = malloc((num+1)*sizeof(*labelv));
		labelv[num] = NULL;
		for (i=0; i<num; i++)
			 labelv[i] = labels[chann_select[i]];
	}

	sel->selection = chann_select;
	sel->num_chann = num;
	sel->labels = labelv;

	return 0;
}


void set_scopes_xticks(EEGPanel* pan)
{
	unsigned int i, value;
	unsigned int* ticks;
	char** labels;
	unsigned int inc, num_ticks;
	float disp_len = pan->display_length;
	unsigned int fs = pan->fs / pan->decimation_factor;
	GObject** widg = pan->gui.widgets;

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
		ticks[i] = value*fs -1;
		labels[i] = g_strdup_printf("%us",value);
	}

	// Set the ticks to the scope widgets
	scope_set_ticks(SCOPE(widg[EEG_SCOPE]), num_ticks, ticks);
	g_object_set(widg[EEG_AXES], "xtick-labelv", labels, NULL);
	binary_scope_set_ticks(BINARY_SCOPE(widg[TRI_SCOPE]), num_ticks, ticks);
	g_object_set(widg[TRI_AXES], "xtick-labelv", labels, NULL);
	scope_set_ticks(SCOPE(widg[EXG_SCOPE]), num_ticks, ticks);
	g_object_set(widg[EXG_AXES], "xtick-labelv", labels, NULL);


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
	GObject** widg = pan->gui.widgets;
	
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
	bargraph_set_ticks(BARGRAPH(widg[EEG_OFFSET_BAR1]), num_ticks, ticks);
	bargraph_set_ticks(BARGRAPH(widg[EEG_OFFSET_BAR2]), num_ticks, ticks);
	g_object_set(widg[EEG_OFFSET_AXES1], "ytick-labelv", labels, NULL);
	g_object_set(widg[EEG_OFFSET_AXES2], "ytick-labelv", labels, NULL);


	g_free(ticks);
	g_strfreev(labels);
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

int create_panel_gui(EEGPanel* pan, const char* uifilename)
{
	GtkBuilder* builder;
	unsigned int res;
	GError* error = NULL;

	RegisterCustomDefinition();

	// Create the panel widgets according to the ui definition files
	builder = gtk_builder_new();
	if (uifilename)
		res = gtk_builder_add_from_file(builder, uifilename, &error);
	else
		res = gtk_builder_add_from_string(builder, str_default_ui_, -1, &error);

	if (!res) {
		fprintf(stderr, "%s\n", error->message);
		goto out;
	}

	gtk_builder_connect_signals(builder, pan);
	
	// Get the pointers of the control widgets
	poll_widgets(pan, builder);
	g_timeout_add(REFRESH_INTERVAL, check_redraw_scopes_cb, pan);
	initialize_widgets(pan);
	set_scopes_xticks(pan);
	
out:
	g_object_unref(builder);
	return res;
}

void update_input_gui(EEGPanel* pan)
{
	char tempstr[32];
	struct PanelGUI* gui = &(pan->gui);
	
	// Update channels
	fill_treeview(GTK_TREE_VIEW(gui->widgets[EEG_TREEVIEW]), pan->nmax_eeg, (const char**)pan->eeg_labels);
	fill_combo(GTK_COMBO_BOX(gui->widgets[ELECREF_COMBO]), pan->eeg_labels, pan->nmax_eeg);
	gtk_combo_box_set_active(GTK_COMBO_BOX(gui->widgets[ELECREF_COMBO]), 0);
	fill_treeview(GTK_TREE_VIEW(gui->widgets[EXG_TREEVIEW]), pan->nmax_exg, (const char**)pan->bipole_labels);

	// Update FS
	sprintf(tempstr,"%u Hz",pan->fs);
	gtk_label_set_text(GTK_LABEL(gui->widgets[NATIVE_FREQ_LABEL]), tempstr);
	sprintf(tempstr,"%u Hz",pan->fs/pan->decimation_factor); 
	gtk_label_set_text(GTK_LABEL(gui->widgets[DISPLAYED_FREQ_LABEL]), tempstr);
}

void set_databuff_gui(EEGPanel* pan)
{
	struct PanelGUI* gui = &(pan->gui);
	unsigned int num_elec_bar1, num_elec_bar2;
	

	scope_set_data(gui->eeg_scope, pan->eeg, pan->num_samples, pan->neeg);
	scope_set_data(gui->exg_scope, pan->exg, pan->num_samples, pan->nexg);
	binary_scope_set_data(gui->tri_scope, pan->triggers, pan->num_samples, pan->nlines_tri);

	// Set bargraphs data
	num_elec_bar2 = pan->neeg/2;
	num_elec_bar1 = pan->neeg - num_elec_bar2;
	bargraph_set_data(gui->eeg_offset_bar1, pan->eeg_offset, num_elec_bar1);
	bargraph_set_data(gui->eeg_offset_bar2, pan->eeg_offset+num_elec_bar1, num_elec_bar2);

	// Update the labels
	g_object_set(pan->gui.widgets[EEG_AXES], "ytick-labelv", pan->eegsel.labels, NULL);
	g_object_set(pan->gui.widgets[EEG_OFFSET_AXES1], "xtick-labelv", pan->eegsel.labels, NULL);	
	g_object_set(pan->gui.widgets[EEG_OFFSET_AXES2], "xtick-labelv", pan->eegsel.labels+num_elec_bar1, NULL);	
	g_object_set(pan->gui.widgets[EXG_AXES], "ytick-labelv", pan->exgsel.labels, NULL);
}

void updategui_toggle_recording(EEGPanel* pan, int state)
{
	gtk_led_set_state(GTK_LED(pan->gui.widgets[RECORDING_LED]), state);
	gtk_button_set_label(GTK_BUTTON(pan->gui.widgets[PAUSE_RECORDING_BUTTON]), state ? "Pause": "Record");
}

void updategui_toggle_connection(EEGPanel* pan, int state)
{
	gtk_led_set_state(GTK_LED(pan->gui.widgets[CONNECT_LED]), state);
	gtk_button_set_label(GTK_BUTTON(pan->gui.widgets[STARTACQUISITION_BUTTON]), state ? "Disconnect" : "Connect");
}

void updategui_toggle_rec_openclose(EEGPanel* pan, int state)
{
	gtk_button_set_label(GTK_BUTTON(pan->gui.widgets[START_RECORDING_BUTTON]), state ? "Stop" : "Setup");
	gtk_widget_set_sensitive(GTK_WIDGET(pan->gui.widgets[PAUSE_RECORDING_BUTTON]),state);
}
