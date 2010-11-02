#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <eegpanel.h>
#include <stdlib.h>
#include <time.h>
#include <gtk/gtk.h>
#if !HAVE_DECL_CLOCK_NANOSLEEP
#include "../lib/clock_nanosleep.h"
#endif

#define EEGSET	AB
#define NEEG	64
#define EXGSET	STD
#define NEXG	8
#define NSAMPLES	32
#define SAMPLING_RATE	2048

GThread* thread_id;
volatile int run_eeg = 0;
volatile int recording = 0;
volatile int recsamples = 0;

#define increase_timespec(timeout, delay)	do {			\
	unsigned int nsec_duration = (delay) + (timeout).tv_nsec;	\
	(timeout).tv_sec += nsec_duration/1000000000;			\
	(timeout).tv_nsec = nsec_duration%1000000000;			\
} while (0)

const char* eeg_labels[64] = {"Fp1","AF7","AF3","F1","F3","F5","F7","FT7",
			"FC5","FC3","FC1","C1","C3","C5","T7","TP7",
			"CP5","CP3","CP1","P1","P3","P5","P7","P9",
			"PO7","PO3","O1","Iz","Oz","POz","Pz","CPz",
			"Fpz","Fp2","AF8","AF4","AFz","Fz","F2","F4",
			"F6","F8","FT8","FC6","FC4","FC2","FCz","Cz",
			"C2","C4","C6","T8","TP8","CP6","CP4","CP2",
			"P2","P4","P6","P8","P10","PO8","PO4","O2"
};

void set_signals(float* eeg, float* exg, uint32_t* tri, int nsamples)
{
	int i, j;
	static uint32_t triggers;
	static int isample = 0;

	for (i=0; i<nsamples; i++) {
		for (j=0; j<NEEG; j++)
			eeg[i*NEEG+j] = i;
		for (j=0; j<NEXG; j++)
			exg[i*NEXG+j] = j*i;
		tri[i] = triggers;
		if ((isample/2048) % 2)
			tri[i] |= 0x100000;
		if (!((isample/2048) % 3))
			tri[i] |= 0x400000;
		isample++;
	}

	triggers++;
}

#define UPDATE_DELAY	((NSAMPLES*1000)/SAMPLING_RATE)
gpointer reading_thread(gpointer arg)
{
	float *eeg, *exg;
	uint32_t *tri;
	EEGPanel* panel = arg;
	struct timespec curr;
	unsigned int isample = 0;

	eeg = calloc(NEEG*NSAMPLES, sizeof(*eeg));
	exg = calloc(NEXG*NSAMPLES, sizeof(*exg));
	tri = calloc(NSAMPLES, sizeof(*tri));


	curr.tv_sec = 0;
	curr.tv_nsec = UPDATE_DELAY*1000000;

	while(run_eeg) {
		clock_nanosleep(CLOCK_REALTIME, 0, &curr, NULL);
		set_signals(eeg, exg, tri, NSAMPLES);
		eegpanel_add_samples(panel, eeg, exg, tri, NSAMPLES);
		isample += NSAMPLES;

		if (recording) {
			recsamples += NSAMPLES;
			if (recsamples > 201*NSAMPLES) {
				recording = 0;
				eegpanel_notify(panel, REC_PAUSED);
			}
		}

		if ((isample<201*NSAMPLES)&&(isample >= 200*NSAMPLES)) {
			eegpanel_popup_message(panel, "Hello!");
			isample = 201*NSAMPLES;
		}
	}

	free(eeg);
	free(exg);
	free(tri);

	return 0;
}

int SetupRecording(const ChannelSelection* eeg_sel, const ChannelSelection* exg_sel, void* user_data)
{
	(void)eeg_sel;
	(void)exg_sel;
	EEGPanel* panel = user_data;
	char* filename;
	int retval = 0;

	if ((filename = eegpanel_open_filename_dialog(panel, "BDF files|*.bdf||Any files|*"))) {
		fprintf(stderr,"filename %s\n", filename);
		free(filename);
		retval = 1;
		recsamples = 0;
	}
	return retval;
}

int Connect(EEGPanel* panel)
{
	run_eeg = 1;

	eegpanel_define_input(panel, NEEG, NEXG, 16, SAMPLING_RATE);

	thread_id = g_thread_create(reading_thread, panel, TRUE, NULL);

	return 0;
}

int Disconnect(EEGPanel* panel)
{
	(void)panel;
	run_eeg = 0;
	g_thread_join(thread_id);

	return 0;
}


int SystemConnection(int start, void* user_data)
{
	EEGPanel* panel = user_data;
	int retval;

	if (start) 
		retval = Connect(panel);
	else 
		retval = Disconnect(panel);

	return (retval < 0) ? 0 : 1;
}

int StopRecording(void* user_data)
{
	(void)user_data;
	return 1;
}

int ToggleRecording(int start, void* user_data)
{
	(void)user_data;
	if (start)
		recording = 1;
	return 1;
}


int ClosePanel(void* user_data)
{
	EEGPanel* panel = user_data;
	if (run_eeg) {
		Disconnect(panel);
		eegpanel_popup_message(panel, "Acquisition disconnected by close func");
	}
	
	return 1;
}


int main(int argc, char* argv[])
{
	EEGPanel* panel;
	struct PanelCb cb;
	struct PanelSettings settings = {
		.filt[EEG_LOWPASS_FLT] = { .state = 1, .freq = 100.0f },
		.filt[SENSOR_LOWPASS_FLT] = { .state = 1, .freq = 100.0f },
		.filt[EEG_HIGHPASS_FLT] = { .state = 1, .freq = 1.0f },
		.filt[SENSOR_HIGHPASS_FLT] = { .state = 1, .freq = 1.0f },
		.eeglabels = eeg_labels,
		.num_eeg = 64
	};

	cb.user_data = NULL;
	cb.process_selection = NULL;
	cb.close_panel = ClosePanel;
	cb.system_connection = SystemConnection;
	cb.setup_recording = SetupRecording;
	cb.stop_recording = StopRecording;
	cb.toggle_recording = ToggleRecording;
	
	init_eegpanel_lib(&argc, &argv);

	panel = eegpanel_create(&settings, &cb);
	if (!panel) {
		fprintf(stderr,"error at the creation of the panel\n");
		return 1;
	}

	eegpanel_show(panel, 1);
	eegpanel_run(panel, 0);

	eegpanel_destroy(panel);

	return 0;
}

