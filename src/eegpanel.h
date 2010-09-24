/*
	Copyright (C) 2008-2009 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This file is part of the eegpanel library

    The eegpanel library is free software: you can redistribute it and/or
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _EEGPANEL_H_
#define _EEGPANEL_H_

#include <stdint.h>


typedef struct _EEGPanel EEGPanel;

typedef enum
{
	EEG,
	EXG,
	TRIGGERS
} ChannelType;

typedef struct _ChannelSelection {
	unsigned int *selection;
	unsigned int num_chann;
	const char** labels;
} ChannelSelection;

typedef int (*ProcessSelectionFunc)(const ChannelSelection* selection, ChannelType type, void* user_data);
typedef int (*SystemConnectionFunc)(int start, void* user_data);
typedef int (*SetupRecordingFunc)(const ChannelSelection* eeg_sel, const ChannelSelection* exg_sel, void* user_data);
typedef int (*StopRecordingFunc)(void* user_data);
typedef int (*ToggleRecordingFunc)(int start, void* user_data);
typedef int (*ClosePanelFunc)(void* user_data);


struct PanelCb {
	/* function supplied by the user */
	ProcessSelectionFunc process_selection;
	SystemConnectionFunc system_connection;
	SetupRecordingFunc setup_recording;
	StopRecordingFunc stop_recording;
	ToggleRecordingFunc toggle_recording; 
	ClosePanelFunc close_panel;

	/* pointer used to pass data to the user functions */
	void* user_data;
};

struct FilterSettings {
	int state;
	float freq;
};

enum FilterNames {
	EEG_LOWPASS_FLT = 0,
	EEG_HIGHPASS_FLT,
	SENSOR_LOWPASS_FLT,
	SENSOR_HIGHPASS_FLT,
	NUMMAX_FLT
};

enum notification {
	DISCONNECTED = 0,
	CONNECTED,
	REC_OPENED,
	REC_CLOSED,
	REC_ON,
	REC_PAUSED
};

struct PanelSettings {
	const char* uifilename;
	struct FilterSettings filt[NUMMAX_FLT];
	unsigned int num_eeg, num_sensor;
	const char** eeglabels;
	const char** sensorlabels;
};

void init_eegpanel_lib(int *argc, char ***argv);
EEGPanel* eegpanel_create(const struct PanelSettings* settings, 
			  const struct PanelCb* cb);
void eegpanel_destroy(EEGPanel* panel);
void eegpanel_show(EEGPanel* panel, int state);
void eegpanel_popup_message(EEGPanel* panel, const char* message);
char* eegpanel_open_filename_dialog(EEGPanel* panel, const char* filefilters);
void eegpanel_run(EEGPanel* panel, int nonblocking);
void eegpanel_add_samples(EEGPanel* panel, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples);
int eegpanel_define_input(EEGPanel* panel, unsigned int num_eeg_channels,
				unsigned int num_exg_channels, unsigned int num_tri_lines,
				unsigned int sampling_rate);
int eegpanel_notify(EEGPanel* panel, enum notification event);

#endif /*_EEGPANEL_H_*/
