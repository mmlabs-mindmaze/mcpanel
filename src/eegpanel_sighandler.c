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
#include "eegpanel_sighandler.h"
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkspinbutton.h>
#include <stdlib.h>
#include <string.h>

/*************************************************************************
 *                                                                       *
 *                         Signal handlers                               *
 *                                                                       *
 *************************************************************************/
#define GET_PANEL_FROM(widget)  ((EEGPanel*)(g_object_get_data(G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(widget))), "eeg_panel")))

gboolean startacquisition_button_clicked_cb(GtkButton* button, gpointer data)
{
	EEGPanel* pan = GET_PANEL_FROM(button);

	(void)data;

	if (pan->cb.system_connection) {
		if (pan->cb.system_connection(pan->connected ? 0 : 1, pan->cb.user_data)) {
			pan->connected = !pan->connected;
			updategui_toggle_connection(pan, pan->connected);
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
		if (!retcode)
			return FALSE;
		pan->recording = !pan->recording;
		updategui_toggle_recording(pan, pan->recording);
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
		if (pan->cb.setup_recording) 
			if (pan->cb.setup_recording(&eeg_sel, &exg_sel, pan->cb.user_data)) 
				updategui_toggle_rec_openclose(pan, 1);
		
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

			if (pan->cb.stop_recording(pan->cb.user_data)) 
				updategui_toggle_rec_openclose(pan, 0);
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
		if (type == EEG) {
			copy_selec(&(pan->eegsel), &select);
			update_filter(pan, EEG_LOWPASS_FILTER);
			update_filter(pan, EEG_HIGHPASS_FILTER);
		} else {
			copy_selec(&(pan->exgsel), &select);
			update_filter(pan, EXG_LOWPASS_FILTER);
			update_filter(pan, EXG_HIGHPASS_FILTER);
		}
		set_data_input(pan, -1);
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
	set_data_input(pan, (pan->display_length * pan->fs)/pan->decimation_factor);
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
	set_data_input(pan, num_samples);
	set_scopes_xticks(pan);

	g_mutex_unlock(pan->data_mutex);
}

int guess_filter(EEGPanel* pan, GObject* widg)
{
	int type = -1;

	if ((widg == pan->gui.widgets[EEG_LOWPASS_SPIN]) || 
	    (widg == pan->gui.widgets[EEG_LOWPASS_CHECK])) {
		type = EEG_LOWPASS_FILTER;
	}
	if ((widg == pan->gui.widgets[EEG_HIGHPASS_SPIN]) || 
	    (widg == pan->gui.widgets[EEG_HIGHPASS_CHECK])) {
		type = EEG_HIGHPASS_FILTER;
	}
	if ((widg == pan->gui.widgets[EXG_LOWPASS_SPIN]) || 
	    (widg == pan->gui.widgets[EXG_LOWPASS_CHECK])) {
		type = EXG_LOWPASS_FILTER;
	}
	if ((widg == pan->gui.widgets[EXG_HIGHPASS_SPIN]) || 
	    (widg == pan->gui.widgets[EXG_HIGHPASS_CHECK])) {
		type = EXG_HIGHPASS_FILTER;
	}
	return type;
}

void filter_button_changed_cb(GtkButton* button, gpointer user_data)
{
	FilterParam options[NUM_FILTERS];
	float eeg_low_fc, eeg_high_fc, exg_low_fc, exg_high_fc;
	int eeg_low_state, eeg_high_state, exg_low_state, exg_high_state;
	float fs;
	(void)user_data;
	int type, reset = 0;

	EEGPanel* pan = GET_PANEL_FROM(button);

	type = guess_filter(pan, G_OBJECT(button));
	if (type == -1)
		return;

	g_mutex_lock(pan->data_mutex);
	if (GTK_IS_SPIN_BUTTON(button)) {
		pan->filter_param[type].freq = gtk_spin_button_get_value(GTK_SPIN_BUTTON(button));
		if (pan->filter_param[type].state)
			reset = 1;
	}
	if (GTK_IS_TOGGLE_BUTTON(button)) {
		pan->filter_param[type].state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
		reset = 1;
	}

	update_filter(pan, type);
	g_mutex_unlock(pan->data_mutex);
}


void connect_panel_signals(EEGPanel* pan)
{
	GtkTreeView* treeview;

	g_signal_connect(pan->gui.widgets[STARTACQUISITION_BUTTON], "clicked", (GCallback)startacquisition_button_clicked_cb, NULL);
	g_signal_connect(pan->gui.widgets[START_RECORDING_BUTTON], "clicked", (GCallback)start_recording_button_clicked_cb, NULL);
	g_signal_connect(pan->gui.widgets[PAUSE_RECORDING_BUTTON], "clicked", (GCallback)pause_recording_button_clicked_cb, NULL);

	g_signal_connect_after(pan->gui.widgets[DECIMATION_COMBO], "changed", (GCallback)decimation_combo_changed_cb, NULL);

	treeview = GTK_TREE_VIEW(pan->gui.widgets[EEG_TREEVIEW]);
	g_signal_connect_after(gtk_tree_view_get_selection(treeview), "changed", (GCallback)channel_selection_changed_cb, (gpointer)EEG);
	treeview = GTK_TREE_VIEW(pan->gui.widgets[EXG_TREEVIEW]);
	g_signal_connect_after(gtk_tree_view_get_selection(treeview), "changed", (GCallback)channel_selection_changed_cb, (gpointer)EXG);

	g_signal_connect(pan->gui.widgets[EEG_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)ELEC_TYPE);
	g_signal_connect(pan->gui.widgets[OFFSET_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)OFFSET_TYPE);
	g_signal_connect(pan->gui.widgets[EXG_SCALE_COMBO], "changed", (GCallback)scale_combo_changed_cb, (gpointer)BIPOLE_TYPE);

	g_signal_connect_after(pan->gui.widgets[TIME_WINDOW_COMBO], "changed", (GCallback)time_window_combo_changed_cb, (gpointer)ELEC_TYPE);

	g_signal_connect_after(pan->gui.widgets[EEG_LOWPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EEG_HIGHPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EXG_LOWPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EXG_HIGHPASS_CHECK], "toggled",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EEG_LOWPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EEG_HIGHPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EXG_LOWPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);
	g_signal_connect_after(pan->gui.widgets[EXG_HIGHPASS_SPIN], "value-changed",(GCallback)filter_button_changed_cb, NULL);

	g_signal_connect(pan->gui.widgets[REFTYPE_COMBO], "changed", (GCallback)reftype_combo_changed_cb, NULL);
	g_signal_connect(pan->gui.widgets[ELECREF_COMBO], "changed", (GCallback)refelec_combo_changed_cb, NULL);

	g_signal_connect(pan->gui.widgets[ELECSET_COMBO], "changed", (GCallback)elecset_combo_changed_cb, NULL);
}


