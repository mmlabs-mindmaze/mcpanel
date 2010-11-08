#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <eegpanel.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <gtk/gtk.h>

#define EEGSET	AB
#define NEEG	64
#define EXGSET	STD
#define NEXG	8
#define NSAMPLES	32
#define SAMPLING_RATE	2048

volatile int run_eeg = 0;
volatile int recording = 0;
volatile int recsamples = 0;
unsigned int isample = 0;

float *geeg=NULL, *gexg=NULL;
uint32_t *gtri = NULL;

#define increase_timespec(timeout, delay)	do {			\
	unsigned int nsec_duration = (delay) + (timeout).tv_nsec;	\
	(timeout).tv_sec += nsec_duration/1000000000;			\
	(timeout).tv_nsec = nsec_duration%1000000000;			\
} while (0)

const char* eeg_lab[NEEG] = {"Fp1","AF7","AF3","F1","F3","F5","F7","FT7",
			"FC5","FC3","FC1","C1","C3","C5","T7","TP7",
			"CP5","CP3","CP1","P1","P3","P5","P7","P9",
			"PO7","PO3","O1","Iz","Oz","POz","Pz","CPz",
			"Fpz","Fp2","AF8","AF4","AFz","Fz","F2","F4",
			"F6","F8","FT8","FC6","FC4","FC2","FCz","Cz",
			"C2","C4","C6","T8","TP8","CP6","CP4","CP2",
			"P2","P4","P6","P8","P10","PO8","PO4","O2"
};
const char* exg_lab[NEXG] = {
	"EXG1","EXG2", "EXG3", "EXG4", "EXG5", "EXG6", "EXG7", "EXG8"
};

#define BAR_NSCALES 2
const char* bar_sclabels[BAR_NSCALES] = {"25 mV", "50 mV"};
const float bar_scales[BAR_NSCALES] = {25.0e3, 50.0e3};

struct panel_tabconf tabconf[] = {
	[0] = {.type = TABTYPE_SCOPE, .name = "EEG"},
	[1] = {.type = TABTYPE_BARGRAPH, .name = "EEG offsets",
	       .nscales = BAR_NSCALES, .sclabels = bar_sclabels,
	       .scales = bar_scales},
	[2] = {.type = TABTYPE_SCOPE, .name = "Sensors"},
};
#define NTAB	(sizeof(tabconf)/sizeof(tabconf[0]))

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
gboolean iteration_func(gpointer data)
{
	EEGPanel *panel = data;

	if (!run_eeg)
		return FALSE;

	set_signals(geeg, gexg, gtri, NSAMPLES);
	eegpanel_add_samples(panel, 0, NSAMPLES, geeg);
	eegpanel_add_samples(panel, 1, NSAMPLES, geeg);
	eegpanel_add_samples(panel, 2, NSAMPLES, gexg);
	eegpanel_add_triggers(panel, NSAMPLES, gtri);
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
	return TRUE;
}

int SetupRecording(void* user_data)
{
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

	eegpanel_define_triggers(panel, 16, SAMPLING_RATE);
	eegpanel_define_tab_input(panel, 0, NEEG, SAMPLING_RATE, eeg_lab);
	eegpanel_define_tab_input(panel, 1, NEEG, SAMPLING_RATE, eeg_lab);
	eegpanel_define_tab_input(panel, 2, NEXG, SAMPLING_RATE, exg_lab);

	geeg = calloc(NEEG*NSAMPLES, sizeof(*geeg));
	gexg = calloc(NEXG*NSAMPLES, sizeof(*gexg));
	gtri = calloc(NSAMPLES, sizeof(*gtri));
	g_timeout_add(UPDATE_DELAY, iteration_func, panel);

	return 0;
}

int Disconnect(EEGPanel* panel)
{
	run_eeg = 0;

	g_idle_remove_by_data(panel);
	free(geeg);
	free(gexg);
	free(gtri);

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
	struct PanelCb cb = {
		.user_data = NULL,
		.close_panel = ClosePanel,
		.system_connection = SystemConnection,
		.setup_recording = SetupRecording,
		.stop_recording = StopRecording,
		.toggle_recording = ToggleRecording
	};
	
	init_eegpanel_lib(&argc, &argv);

	panel = eegpanel_create(NULL, &cb, NTAB, tabconf);
	if (!panel) {
		fprintf(stderr,"error at the creation of the panel\n");
		return 1;
	}

	eegpanel_show(panel, 1);
	eegpanel_run(panel, 0);

	eegpanel_destroy(panel);

	return 0;
}

