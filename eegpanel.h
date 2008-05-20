#ifndef _EEGPANEL_H_
#define _EEGPANEL_H_

#include <stdint.h>


typedef struct _EEGPanelPrivateData EEGPanelPrivateData;
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
typedef int (*ToggleRecordingFunc)(int start, void* user_data);


struct _EEGPanel {
	// function supplied by the user
	ProcessSelectionFunc process_selection;
	SystemConnectionFunc system_connection;
	SetupRecordingFunc setup_recording;
	ToggleRecordingFunc toggle_recording; 

	// pointer used to pass data to the user functions
	void* user_data;

	// private data, don't touch
	EEGPanelPrivateData* priv;
};

EEGPanel* eegpanel_create(void);
void eegpanel_destroy(EEGPanel* panel);
void eegpanel_show(EEGPanel* panel, int state);
void eegpanel_run(EEGPanel* panel, int nonblocking);
void eegpanel_add_selected_samples(EEGPanel* panel, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples);
void eegpanel_add_samples(EEGPanel* panel, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples);
int eegpanel_define_input(EEGPanel* panel, unsigned int num_eeg_channels,
				unsigned int num_exg_channels, unsigned int num_tri_lines,
				unsigned int sampling_rate);

#endif //_EEGPANEL_H_
