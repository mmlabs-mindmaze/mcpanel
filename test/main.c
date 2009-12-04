#include <stdio.h>
#include <eegpanel.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
//#include <gtk/gtk.h>

#define EEGSET	AB
#define NEEG	64
#define EXGSET	STD
#define NEXG	8
#define NSAMPLES	32
#define SAMPLING_RATE	2048

pthread_t thread_id = 0;
volatile int run_eeg = 0;

float *geeg=NULL, *gexg=NULL;
uint32_t *gtri = NULL;

#define increase_timespec(timeout, delay)	do {			\
	unsigned int nsec_duration = (delay) + (timeout).tv_nsec;	\
	(timeout).tv_sec += nsec_duration/1000000000;			\
	(timeout).tv_nsec = nsec_duration%1000000000;			\
} while (0)

void set_signals(float* eeg, float* exg, uint32_t* tri, int nsamples)
{
	int i, j;
	static uint32_t triggers;
	static int isample = 0;

	for (i=0; i<nsamples; i++) {
		for (j=0; j<NEEG; j++)
			eeg[i*NEEG+j] = i;
		for (j=0; j<NEXG; j++)
			exg[i*NEXG+j] = i;
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
void* reading_thread(void* arg)
{
	float *eeg, *exg;
	uint32_t *tri;
	int32_t *raweeg, *rawexg;
	EEGPanel* panel = arg;
	struct timespec curr, prev;
	int interval;
	unsigned int isample = 0;

	eeg = calloc(NEEG*NSAMPLES, sizeof(*eeg));
	exg = calloc(NEXG*NSAMPLES, sizeof(*exg));
	tri = calloc(NSAMPLES, sizeof(*tri));
	raweeg = (int32_t*)eeg;
	rawexg = (int32_t*)exg;

	curr.tv_sec = 0;
	curr.tv_nsec = UPDATE_DELAY*1000000;

//	clock_gettime(CLOCK_REALTIME, &prev);
	while(run_eeg) {
//		clock_gettime(CLOCK_REALTIME, &curr);
//		interval = (curr.tv_sec - prev.tv_sec)*1000 + (curr.tv_nsec - prev.tv_nsec)/1000000;

//		if (interval > UPDATE_DELAY) {
			nanosleep(&curr, NULL);
			set_signals(eeg, exg, tri, NSAMPLES);
			eegpanel_add_samples(panel, eeg, exg, tri, NSAMPLES);
			isample += NSAMPLES;

			if ((isample<201*NSAMPLES)&&(isample >= 200*NSAMPLES)) {
				eegpanel_popup_message(panel, "Hello!");
				isample = 201*NSAMPLES;
			}
//			prev = curr;
//		}
	}

	free(eeg);
	free(exg);
	free(tri);

	return 0;
}

/*
gboolean iteration_func(gpointer data)
{
	EEGPanel *panel = data;

	if (!run_eeg)
		return FALSE;

	set_signals(geeg, gexg, gtri, NSAMPLES);
	eegpanel_add_samples(panel, geeg, gexg, gtri, NSAMPLES);
	return TRUE;
}*/

int SetupRecording(const ChannelSelection* eeg_sel, const ChannelSelection* exg_sel, void* user_data)
{
	EEGPanel* panel = user_data;
	char* filename;
	int retval = 0;

	if (filename = eegpanel_open_filename_dialog(panel, "*.bdf", "BDF files (*.bdf)")) {
		fprintf(stderr,"filename %s\n", filename);
		free(filename);
		retval = 1;
	}
	return retval;
}

int Connect(EEGPanel* panel)
{
	run_eeg = 1;

	eegpanel_define_input(panel, NEEG, NEXG, 16, SAMPLING_RATE);

	pthread_create(&thread_id, NULL, reading_thread, panel);

/*	geeg = calloc(NEEG*NSAMPLES, sizeof(*geeg));
	gexg = calloc(NEXG*NSAMPLES, sizeof(*gexg));
	gtri = calloc(NSAMPLES, sizeof(*gtri));
	g_timeout_add(UPDATE_DELAY, iteration_func, panel);
*/
	return 0;
}

int Disconnect(EEGPanel* panel)
{
	run_eeg = 0;
	pthread_join(thread_id, NULL);

	/*free(geeg);
	free(gexg);
	free(gtri);
	*/

	return 0;
}


int SystemConnection(int start, void* user_data)
{
	EEGPanel* panel = user_data;
	int retval;

	if (start) {
		retval = Connect(panel);
	}
	else {
		retval = Disconnect(panel);
	}

	return (retval < 0) ? 0 : 1;
}

int StopRecording(void* user_data)
{
	return 1;
}

int ToggleRecording(int start, void* user_data)
{
	return 1;
}


int main(int argc, char* argv[])
{
	EEGPanel* panel;
	struct PanelCb cb;
	char settingfile[128];

	cb.user_data = NULL;
	cb.process_selection = NULL;
	cb.system_connection = SystemConnection;
	cb.setup_recording = SetupRecording;
	cb.stop_recording = StopRecording;
	cb.toggle_recording = ToggleRecording;
	
	init_eegpanel_lib(&argc, &argv);

	sprintf(settingfile, "%s/settings.ini", getenv("srcdir"));
	panel = eegpanel_create(NULL, settingfile, &cb);
	if (!panel) {
		fprintf(stderr,"error at the creation of the panel\n");
		return 1;
	}
	


	eegpanel_show(panel, 1);
	eegpanel_run(panel, 0);

	eegpanel_destroy(panel);

	return 0;
}

