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
#include "eegpanel_gui.h"
#include "default-ui.h"
#include <stdlib.h>
#include <string.h>

#define REFRESH_INTERVAL	30
#define GET_PANEL_FROM(widget)  ((EEGPanel*)(g_object_get_data(G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))), "eeg_panel")))

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

int fill_selec_from_treeselec(ChannelSelection* selection, GtkTreeSelection* tree_sel, char** labels);
void clean_selec(ChannelSelection* selection);
int poll_widgets(EEGPanel* pan, GtkBuilder* builder);
int initialize_widgets(EEGPanel* pan);
GtkListStore* initialize_list_treeview(GtkTreeView* treeview, const gchar* attr_name);
void initialize_combo(GtkComboBox* combo);
void fill_treeview(GtkTreeView* treeview, unsigned int num_labels, const char** labels);
void fill_combo(GtkComboBox* combo, char** labels, int num_labels);
void set_scopes_xticks(EEGPanel* pan);
void set_bargraphs_yticks(EEGPanel* pan, float max);
int RegisterCustomDefinition(void);

/*************************************************************************
 *                                                                       *
 *                         Signal handlers                               *
 *                                                                       *
 *************************************************************************/
gboolean startacquisition_button_clicked_cb(GtkButton* button, gpointer data)
{
	EEGPanel* pan = GET_PANEL_FROM(button);

	(void)data;

	if (pan->cb.system_connection) {
		if (pan->cb.system_connection(pan->connected ? 0 : 1, pan->cb.user_data)) {
			pan->connected = !pan->connected;
			gtk_led_set_state(GTK_LED(pan->gui.widgets[CONNECT_LED]), pan->connected);
			gtk_button_set_label(button, pan->connected ? "Disconnect" : "Connect");
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
		gtk_led_set_state(GTK_LED(pan->gui.widgets[RECORDING_LED]), pan->recording);
		gtk_button_set_label(button,pan->recording ? "Pause": "Record");
	}

	return retcode ? TRUE : FALSE;
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
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pan->gui.widgets[EEG_TREEVIEW]));
		fill_selec_from_treeselec(&eeg_sel, selection, pan->eeg_labels);
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pan->gui.widgets[EXG_TREEVIEW]));
		fill_selec_from_treeselec(&exg_sel, selection, pan->exg_labels);

		// Send the setup event through the callback
		if (pan->cb.setup_recording) {
			if (pan->cb.setup_recording(&eeg_sel, &exg_sel, pan->cb.user_data)) {
				pan->fileopened = TRUE;
				gtk_button_set_label(GTK_BUTTON(pan->gui.widgets[PAUSE_RECORDING_BUTTON]),"Record");
				gtk_button_set_label(button, "Stop");
				gtk_widget_set_sensitive(GTK_WIDGET(pan->gui.widgets[PAUSE_RECORDING_BUTTON]),TRUE);
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
				pause_recording_button_clicked_cb(GTK_BUTTON(pan->gui.widgets[PAUSE_RECORDING_BUTTON]),NULL);

			if (pan->cb.stop_recording(pan->cb.user_data)) {
				pan->fileopened = FALSE;
				gtk_button_set_label(button, "Setup");
				gtk_widget_set_sensitive(GTK_WIDGET(pan->gui.widgets[PAUSE_RECORDING_BUTTON]),FALSE);
			}
		}
	}



	return TRUE;
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

	gtk_widget_set_sensitive(GTK_WIDGET(pan->gui.widgets[ELECREF_COMBO]), (type==ELECTRODE_REF)? TRUE : FALSE);

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

	tree_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pan->gui.widgets[EEG_TREEVIEW]));

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
	set_data_input(pan, (pan->display_length * pan->fs)/pan->decimation_factor, NULL, NULL);
	g_mutex_unlock(pan->data_mutex);
	
	// update the display
	sprintf(tempstr,"%u Hz",pan->fs/pan->decimation_factor); 
	gtk_label_set_text(GTK_LABEL(pan->gui.widgets[DISPLAYED_FREQ_LABEL]), tempstr);


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
		g_object_set(pan->gui.widgets[EEG_SCOPE], "scale", scale, NULL);
		break;

	case BIPOLE_TYPE:
		g_object_set(pan->gui.widgets[EXG_SCOPE], "scale", scale, NULL);
		break;

	case OFFSET_TYPE:
		g_object_set(pan->gui.widgets[EEG_OFFSET_BAR1], "min-value", -scale, "max-value", scale, NULL);
		g_object_set(pan->gui.widgets[EEG_OFFSET_BAR2], "min-value", -scale, "max-value", scale, NULL);
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
	num_samples = (unsigned int)(time_length * (float)(pan->fs / pan->decimation_factor));
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
	eeg_low_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(pan->gui.widgets[EEG_LOWPASS_SPIN]));
	eeg_high_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(pan->gui.widgets[EEG_HIGHPASS_SPIN]));
	exg_low_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(pan->gui.widgets[EXG_LOWPASS_SPIN]));
	exg_high_fc = gtk_spin_button_get_value(GTK_SPIN_BUTTON(pan->gui.widgets[EXG_HIGHPASS_SPIN]));

	// Retrieve the state of the check boxes
	eeg_low_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pan->gui.widgets[EEG_LOWPASS_CHECK]));
	eeg_high_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pan->gui.widgets[EEG_HIGHPASS_CHECK]));
	exg_low_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pan->gui.widgets[EXG_LOWPASS_CHECK]));
	exg_high_state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pan->gui.widgets[EXG_HIGHPASS_CHECK]));


	g_mutex_lock(pan->data_mutex);
	fs = pan->fs / pan->decimation_factor;
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


gboolean check_redraw_scopes_cp(gpointer user_data)
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

gboolean popup_dialog_cb(gpointer data)
{
	GtkWidget* dialog;
	struct DialogParam* dlgprm = data;


	dialog = gtk_message_dialog_new(dlgprm->gui->window,
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_INFO,
					GTK_BUTTONS_CLOSE,
					"%s", dlgprm->message);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	free(dlgprm->message);
	free(dlgprm);

	return FALSE;
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
	
	g_signal_connect(pan->gui.widgets[STARTACQUISITION_BUTTON], "clicked", (GCallback)startacquisition_button_clicked_cb, NULL);
	g_signal_connect(pan->gui.widgets[START_RECORDING_BUTTON], "clicked", (GCallback)start_recording_button_clicked_cb, NULL);
	g_signal_connect(pan->gui.widgets[PAUSE_RECORDING_BUTTON], "clicked", (GCallback)pause_recording_button_clicked_cb, NULL);
	gtk_widget_set_sensitive(GTK_WIDGET(pan->gui.widgets[PAUSE_RECORDING_BUTTON]),FALSE);

	treeview = GTK_TREE_VIEW(pan->gui.widgets[EEG_TREEVIEW]);
	initialize_list_treeview(treeview, "channels");
	g_signal_connect_after(gtk_tree_view_get_selection(treeview), "changed", (GCallback)channel_selection_changed_cb, (gpointer)EEG);

	treeview = GTK_TREE_VIEW(pan->gui.widgets[EXG_TREEVIEW]);
	initialize_list_treeview(treeview, "channels");
	g_signal_connect_after(gtk_tree_view_get_selection(treeview), "changed", (GCallback)channel_selection_changed_cb, (gpointer)EXG);

	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[DECIMATION_COMBO]), 0);
	g_signal_connect_after(pan->gui.widgets[DECIMATION_COMBO],	"changed", (GCallback)decimation_combo_changed_cb, NULL);
	
	// Scale combos
	g_signal_connect(pan->gui.widgets[EEG_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)ELEC_TYPE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[EEG_SCALE_COMBO]), 0);
	g_signal_connect(pan->gui.widgets[OFFSET_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)OFFSET_TYPE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[OFFSET_SCALE_COMBO]), 0);
	g_signal_connect(pan->gui.widgets[EXG_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)BIPOLE_TYPE);
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[EXG_SCALE_COMBO]), 0);

	// Time window combo
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[TIME_WINDOW_COMBO]), 0);
	g_signal_connect_after(pan->gui.widgets[TIME_WINDOW_COMBO], "changed", (GCallback)time_window_combo_changed_cb, (gpointer)ELEC_TYPE);

	// filter buttons
	g_object_set(pan->gui.widgets[EEG_LOWPASS_CHECK], "active", pan->filter_param[EEG_LOWPASS_FILTER].state, NULL);
	g_object_set(pan->gui.widgets[EEG_LOWPASS_SPIN], "value", pan->filter_param[EEG_LOWPASS_FILTER].freq, NULL);
	g_object_set(pan->gui.widgets[EEG_HIGHPASS_CHECK], "active", pan->filter_param[EEG_HIGHPASS_FILTER].state, NULL);
	g_object_set(pan->gui.widgets[EEG_HIGHPASS_SPIN], "value", pan->filter_param[EEG_HIGHPASS_FILTER].freq, NULL);
	g_object_set(pan->gui.widgets[EXG_LOWPASS_CHECK], "active", pan->filter_param[EXG_LOWPASS_FILTER].state, NULL);
	g_object_set(pan->gui.widgets[EXG_LOWPASS_SPIN], "value", pan->filter_param[EXG_LOWPASS_FILTER].freq, NULL);
	g_object_set(pan->gui.widgets[EXG_HIGHPASS_CHECK], "active", pan->filter_param[EXG_HIGHPASS_FILTER].state, NULL);
	g_object_set(pan->gui.widgets[EXG_HIGHPASS_SPIN], "value", pan->filter_param[EXG_HIGHPASS_FILTER].freq, NULL);
	g_signal_connect_after(pan->gui.widgets[EEG_LOWPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EEG_HIGHPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EXG_LOWPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EXG_HIGHPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EEG_LOWPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EEG_HIGHPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EXG_LOWPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EXG_HIGHPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);

	// reference combos
	gtk_combo_box_set_active(GTK_COMBO_BOX(pan->gui.widgets[REFTYPE_COMBO]), 0);
	g_signal_connect(pan->gui.widgets[REFTYPE_COMBO], "changed", (GCallback)reftype_combo_changed_cb, NULL);
	g_signal_connect(pan->gui.widgets[ELECREF_COMBO], "changed", (GCallback)refelec_combo_changed_cb, NULL);

	g_signal_connect(pan->gui.widgets[ELECSET_COMBO], "changed", (GCallback)elecset_combo_changed_cb, NULL);
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
		fprintf(stderr,"%s\n",error->message);
		goto out;
	}

	gtk_builder_connect_signals(builder, pan);
	
	// Get the pointers of the control widgets
	poll_widgets(pan, builder);
	g_timeout_add(REFRESH_INTERVAL, check_redraw_scopes_cp, pan);
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
	if ((eeg_selec) && (eeg_selec->labels)) {	
		g_object_set(pan->gui.widgets[EEG_AXES], "ytick-labelv", eeg_selec->labels, NULL);
		g_object_set(pan->gui.widgets[EEG_OFFSET_AXES1], "xtick-labelv", eeg_selec->labels, NULL);	
		g_object_set(pan->gui.widgets[EEG_OFFSET_AXES2], "xtick-labelv", eeg_selec->labels+num_elec_bar1, NULL);	
	}
	if ((exg_selec) && (exg_selec->labels))	
		g_object_set(pan->gui.widgets[EXG_AXES], "ytick-labelv", exg_selec->labels, NULL);
}
