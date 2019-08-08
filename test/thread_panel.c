#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <mcpanel.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>
#if !HAVE_DECL_CLOCK_NANOSLEEP
#include "../lib/clock_nanosleep.h"
#endif

#define EEGSET	AB
#define NEEG	64
#define EXGSET	STD
#define NEXG	8
#define SAMPLING_RATE	2048
#define NSAMPLES	((unsigned int)(SAMPLING_RATE*0.03))
#define NTRI    3

GThread* thread_id;
volatile int run_eeg = 0;
volatile int recording = 0;
volatile int recsamples = 0;

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
	{.type = TABTYPE_SCOPE, .name = "EEG"},
	{.type = TABTYPE_BARGRAPH, .name = "EEG offsets",
	 .nscales = BAR_NSCALES, .sclabels = bar_sclabels,
	 .scales = bar_scales},
	{.type = TABTYPE_SPECTRUM, .name = "EEG Spectrum"},
	{.type = TABTYPE_SCOPE, .name = "Sensors"},
};
#define NTAB	(sizeof(tabconf)/sizeof(tabconf[0]))

static
int rand_lim(int min, int max)
{
	float width = max - min;

	return ((int)((width/RAND_MAX) * rand())) + min;
}

static
void set_signals(float* eeg, float* exg, uint32_t* tri, int nsamples)
{
	int i, j;
	static uint32_t triggers;
	static int isample = 0;

	for (i=0; i<nsamples; i++) {
		for (j=0; j<NEEG; j++)
			eeg[i*NEEG+j] = i + (j%2)*rand_lim(-20, 20);
		for (j=0; j<NEXG; j++)
			exg[i*NEXG+j] = j*i;
		for (j=0; j<NTRI; j++)
			tri[i*NTRI+j] = triggers << j;

		if ((isample/SAMPLING_RATE) % 2)
			tri[i*NTRI] |= 0x100000;
		if (!((isample/SAMPLING_RATE) % 3))
			tri[i*NTRI] |= 0x400000;
		isample++;
	}

	triggers++;
}

#define UPDATE_DELAY	((NSAMPLES*1000)/SAMPLING_RATE)
static
gpointer reading_thread(gpointer arg)
{
	float *eeg, *exg;
	uint32_t *tri;
	mcpanel* panel = arg;
	struct timespec curr;
	unsigned int isample = 0;
	struct mcp_event evt = {0};
	unsigned int next_event = 30*NSAMPLES;

	eeg = calloc(NEEG*NSAMPLES, sizeof(*eeg));
	exg = calloc(NEXG*NSAMPLES, sizeof(*exg));
	tri = calloc(NTRI*NSAMPLES, sizeof(*tri));


	curr.tv_sec = 0;
	curr.tv_nsec = UPDATE_DELAY*1000000;

	while(run_eeg) {
		clock_nanosleep(CLOCK_REALTIME, 0, &curr, NULL);
		set_signals(eeg, exg, tri, NSAMPLES);
		mcp_add_samples(panel, 0, NSAMPLES, eeg);
		mcp_add_samples(panel, 1, NSAMPLES, eeg);
		mcp_add_samples(panel, 2, NSAMPLES, eeg);
		mcp_add_samples(panel, 3, NSAMPLES, exg);
		mcp_add_triggers(panel, NSAMPLES, tri);
		isample += NSAMPLES;

		if (isample >= next_event) {
			evt.type = (evt.type+1) % 8;
			evt.pos = next_event + rand_lim(-SAMPLING_RATE, SAMPLING_RATE);
			mcp_add_events(panel, 0, 1, &evt);
			next_event += 2*SAMPLING_RATE;
		}

		if (recording) {
			recsamples += NSAMPLES;
			if (recsamples > 201*NSAMPLES) {
				recording = 0;
				mcp_notify(panel, REC_PAUSED);
			}
		}

		if ((isample<201*NSAMPLES)&&(isample >= 200*NSAMPLES)) {
			mcp_popup_message(panel, "Hello!");
			isample = 201*NSAMPLES;
		}
	}

	free(eeg);
	free(exg);
	free(tri);

	return 0;
}

static
int SetupRecording(void* user_data)
{
	mcpanel* panel = user_data;
	char* filename;
	int retval = 0;

	if ((filename = mcp_open_filename_dialog(panel, "BDF files|*.bdf||Any files|*"))) {
		fprintf(stderr,"filename %s\n", filename);
		free(filename);
		retval = 1;
		recsamples = 0;
	}
	return retval;
}

static
int Connect(mcpanel* panel)
{
	run_eeg = 1;
	const char* tri_lab[] = {"tri1", "tri2", "trigger:3"};

	mcp_define_trigg_input(panel, 16, NTRI, SAMPLING_RATE, tri_lab);
	mcp_define_tab_input(panel, 0, NEEG, SAMPLING_RATE, eeg_lab);
	mcp_define_tab_input(panel, 1, NEEG, SAMPLING_RATE, eeg_lab);
	mcp_define_tab_input(panel, 2, NEEG, SAMPLING_RATE, eeg_lab);
	mcp_define_tab_input(panel, 3, NEXG, SAMPLING_RATE, exg_lab);

	thread_id = g_thread_new(NULL, reading_thread, panel);

	return 0;
}

static
int Disconnect(mcpanel* panel)
{
	(void)panel;
	run_eeg = 0;
	g_thread_join(thread_id);

	return 0;
}

static
int SystemConnection(int start, void* user_data)
{
	mcpanel* panel = user_data;
	int retval;

	if (start) 
		retval = Connect(panel);
	else 
		retval = Disconnect(panel);

	return (retval < 0) ? 0 : 1;
}

static
int StopRecording(void* user_data)
{
	(void)user_data;
	return 1;
}

static
int ToggleRecording(int start, void* user_data)
{
	(void)user_data;
	if (start)
		recording = 1;
	return 1;
}

static
int ClosePanel(void* user_data)
{
	mcpanel* panel = user_data;
	if (run_eeg) {
		Disconnect(panel);
		mcp_popup_message(panel, "Acquisition disconnected by close func");
	}
	
	return 1;
}

static
void on_device_info(int id, void* data)
{
	(void)id;
	mcpanel* panel = data;
	mcp_popup_message(panel, "Device info clicked");
}


int main(int argc, char* argv[])
{
	mcpanel* panel;
	struct panel_button cust_buttons = {
		.label = "device info",
		.id = 0,
		.callback = on_device_info
	};
	struct PanelCb cb = {
		.user_data = NULL,
		.close_panel = ClosePanel,
		.system_connection = SystemConnection,
		.setup_recording = SetupRecording,
		.stop_recording = StopRecording,
		.toggle_recording = ToggleRecording,
		.nbutton = 1,
		.custom_button = &cust_buttons,
		.confname = "test"
	};
	
	mcp_init_lib(&argc, &argv);

	panel = mcp_create(NULL, &cb, NTAB, tabconf);
	if (!panel) {
		fprintf(stderr,"error at the creation of the panel\n");
		return 1;
	}

	mcp_show(panel, 1);
	mcp_run(panel, 0);

	mcp_destroy(panel);

	return 0;
}

