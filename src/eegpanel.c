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
#include <glib.h>
#include <glib/gprintf.h>
#include <memory.h>
#include <stdlib.h>
#include "eegpanel.h"
#include "eegpanel_shared.h"


gpointer loop_thread(gpointer user_data);
char** add_default_labels(char** labels, unsigned int requested_num_labels, const char* prefix);
void set_bipole_labels(EEGPanel* pan);
void init_filter_params(EEGPanel* pan);
void set_default_values(EEGPanel* pan, const char* filename);
gint run_model_dialog(EEGPanel* pan, GtkDialog* dialog);
void get_initial_values(EEGPanel* pan);

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
	EEGPanel* pan = NULL;
	guint success = 0;


	// Allocate memory for the structures
	pan = calloc(1,sizeof(*pan));
	if (!pan)
		return NULL;
	pan->data_mutex = g_mutex_new();

	// Set callbacks
	if (cb)
		memcpy(&(pan->cb), cb, sizeof(*cb));
	if (pan->cb.user_data == NULL)
		pan->cb.user_data = pan;

	// Needed initializations
	pan->display_length = 1.0f;
	pan->decimation_factor = 1;
	init_filter_params(pan);

	// Initialize the content of the widgets
	if (settingsfilename)
		set_default_values(pan, settingsfilename);

	// Create the panel widgets according to the ui definition files
	if (!create_panel_gui(pan, uifilename)) {
		eegpanel_destroy(pan);
		return NULL;
	}
		
	get_initial_values(pan);
	eegpanel_define_input(pan, 0, 0, 16, 2048);

	return pan;
}

void eegpanel_show(EEGPanel* pan, int state)
{
	int lock_res = 0;

	//if (pan->main_loop_thread != g_thread_self()) 
	
	//////////////////////////////////////////////////////////////
	//		WARNING
	//	Possible deadlock here if called from another thread
	//	than the main loop thread
	/////////////////////////////////////////////////////////////

	if (state)
		gtk_widget_show_all(GTK_WIDGET(pan->gui.window));
	else
		gtk_widget_hide_all(GTK_WIDGET(pan->gui.window));

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

	g_mutex_free(pan->data_mutex);
	g_strfreev(pan->eeg_labels);
	g_strfreev(pan->exg_labels);
	g_strfreev(pan->bipole_labels);

	
	destroy_dataproc(pan);
	clean_selec(&(pan->eegsel));
	clean_selec(&(pan->exgsel));
	
	free(pan);
}


int eegpanel_define_input(EEGPanel* pan, unsigned int neeg,
				unsigned int nexg, unsigned int ntri,
				unsigned int fs)
{
	char tempstr[32];
	int nsamples;
	ChannelSelection chann_selec;
	
	pan->nmax_eeg = neeg;
	pan->nmax_exg = nexg;
	pan->nlines_tri = ntri;
	pan->fs = fs;
	nsamples = (pan->display_length * fs)/pan->decimation_factor;
	 

	// Add default channel labels if not available
	pan->eeg_labels = add_default_labels(pan->eeg_labels, neeg, "EEG");
	pan->exg_labels = add_default_labels(pan->exg_labels, nexg, "EXG");
	set_bipole_labels(pan);

	// Update widgets
	update_input_gui(pan);
	
	// Reset all data buffer;
	chann_selec.num_chann = 0;
	chann_selec.selection = NULL;
	chann_selec.labels = NULL;
	memcpy(&(pan->eegsel), &chann_selec, sizeof(chann_selec));
	memcpy(&(pan->exgsel), &chann_selec, sizeof(chann_selec));

	return set_data_input(pan, nsamples);
}


void eegpanel_add_samples(EEGPanel* pan, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples)
{
	g_mutex_lock(pan->data_mutex);
	add_samples(pan, eeg, exg, triggers, num_samples);
	g_mutex_unlock(pan->data_mutex);
}


void eegpanel_popup_message(EEGPanel* pan, const char* message)
{
	popup_message_gui(pan, message);
}

char* eegpanel_open_filename_dialog(EEGPanel* pan, const char* filter, const char* filtername)
{
	return open_filename_dialog_gui(pan, filter, filtername);
}
///////////////////////////////////////////////////
//
//	Internal functions
//
///////////////////////////////////////////////////
gpointer loop_thread(gpointer user_data)
{
	(void)user_data;

	gtk_main();
	return 0;
}



void clean_selec(ChannelSelection* selection)
{
	if (!selection)
		return;

	free(selection->selection);
	free(selection->labels);
	selection->selection = NULL;
	selection->labels = NULL;
}

void copy_selec(ChannelSelection* dst, ChannelSelection* src)
{
	clean_selec(dst);
	
	dst->num_chann = src->num_chann;

	dst->selection = malloc(src->num_chann*sizeof(*(src->selection)));
	memcpy(dst->selection, src->selection, (src->num_chann)*sizeof(*(src->selection)));

	if (src->labels) {
		dst->labels = malloc((src->num_chann+1)*sizeof(*(src->labels)));
		memcpy(dst->labels, src->labels, (src->num_chann+1)*sizeof(*(src->labels)));
	}
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


void set_all_filters(EEGPanel* pan, FilterParam* options)
{
	unsigned int dec_state = (pan->decimation_factor != 1) ? 1 : 0;

	if (options==NULL)
		options = pan->filter_param;

	options[EEG_DECIMATION_FILTER].state = dec_state;
	options[EXG_DECIMATION_FILTER].state = dec_state;
	options[EEG_LOWPASS_FILTER].numch = pan->neeg;
	options[EXG_LOWPASS_FILTER].numch = pan->nexg;
	options[EEG_HIGHPASS_FILTER].numch = pan->neeg;
	options[EXG_HIGHPASS_FILTER].numch = pan->nexg;
	options[EEG_DECIMATION_FILTER].numch = pan->neeg;
	options[EXG_DECIMATION_FILTER].numch = pan->nexg;
	options[EEG_OFFSET_FILTER].numch = pan->neeg;
	options[EXG_OFFSET_FILTER].numch = pan->nexg;

	// Set the filters
	set_one_filter(pan, EEG_LOWPASS_FILTER);
	set_one_filter(pan, EEG_HIGHPASS_FILTER);
	set_one_filter(pan, EXG_LOWPASS_FILTER);
	set_one_filter(pan, EXG_HIGHPASS_FILTER);

	set_one_filter(pan, EEG_DECIMATION_FILTER);
	set_one_filter(pan, EXG_DECIMATION_FILTER);

	set_one_filter(pan, EEG_OFFSET_FILTER);
	set_one_filter(pan, EXG_OFFSET_FILTER);
}


void init_filter_params(EEGPanel* pan)
{
	FilterParam* options = pan->filter_param;
	float fs = pan->fs;
	int dec_state = (pan->decimation_factor!=1) ? 1 : 0;
	float dec_fc = 0.4*fs/pan->decimation_factor;
	
	
	memset(options, 0, sizeof(options));

	options[EEG_OFFSET_FILTER].state = 1;
	options[EEG_OFFSET_FILTER].freq = 1.0;
	options[EEG_OFFSET_FILTER].highpass = 0;
	options[EXG_OFFSET_FILTER].state = 1;
	options[EXG_OFFSET_FILTER].freq = 1.0;
	options[EXG_OFFSET_FILTER].highpass = 0;
	options[EEG_DECIMATION_FILTER].freq = dec_fc;
	options[EEG_DECIMATION_FILTER].state = dec_state;
	options[EEG_DECIMATION_FILTER].highpass = 0;
	options[EXG_DECIMATION_FILTER].freq = dec_fc;
	options[EXG_DECIMATION_FILTER].state = dec_state;
	options[EXG_DECIMATION_FILTER].highpass = 0;
	options[EEG_LOWPASS_FILTER].state = 0;
	options[EEG_LOWPASS_FILTER].freq = 0.1;
	options[EEG_LOWPASS_FILTER].highpass = 0;
	options[EEG_HIGHPASS_FILTER].state = 0;
	options[EEG_HIGHPASS_FILTER].freq = 0.4*fs;
	options[EEG_HIGHPASS_FILTER].highpass = 1;
	options[EXG_LOWPASS_FILTER].state = 0;
	options[EXG_LOWPASS_FILTER].freq = 0.1;
	options[EXG_LOWPASS_FILTER].highpass = 0;
	options[EXG_HIGHPASS_FILTER].state = 0;
	options[EXG_HIGHPASS_FILTER].freq = 0.4*fs;
	options[EXG_HIGHPASS_FILTER].highpass = 1;
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
	if (!g_key_file_load_from_file(key_file, filename, G_KEY_FILE_NONE, NULL)) {
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

