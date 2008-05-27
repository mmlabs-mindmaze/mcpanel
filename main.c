#include <gtk/gtk.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include "eegpanel.h"
#include "filter.h"
#include <math.h>

#define NUM_CH 		64	
#define NUM_EXG_CH 	8	
#define SAMPLING_RATE	2048
#define UPDATE_PERIOD	30
#define NUM_POINTS	((UPDATE_PERIOD*SAMPLING_RATE)/1000)

float eeg_data[NUM_CH*NUM_POINTS], filtered_eeg_data[NUM_CH*NUM_POINTS];
float exg_data[NUM_EXG_CH*NUM_POINTS], filtered_exg_data[NUM_EXG_CH*NUM_POINTS];
uint32_t trigg[NUM_POINTS];
static int addsamp = 0;
dfilter* filt = NULL;
dfilter* exg_filt = NULL;


static gboolean timerfunc(gpointer user_data)
{
	EEGPanel* panel = user_data;

	static gfloat t=0;
	static guint32 triggers=0;
	static int isample = 0;
	gint i, j;
	for (i=0; i<NUM_POINTS; i++) {
		for (j=0; j<NUM_CH; j++)
			eeg_data[NUM_CH*i +j] = cos(2.0*M_PI*t*(j+1));
			//eeg_data[NUM_CH*i +j] = ((isample/SAMPLING_RATE)%2) ? 0 : 1;
			//eeg_data[NUM_CH*i +j] = (isample%SAMPLING_RATE) ? 0 : 1;
		for (j=0; j<NUM_EXG_CH; j++)
			exg_data[NUM_EXG_CH*i +j] = cos(2.0*M_PI*t*(j+1));
		trigg[i] = triggers;
		t += 1.0 / (float)SAMPLING_RATE;
		isample++;
	}
	triggers++;

	filter(filt, eeg_data, filtered_eeg_data, NUM_POINTS); 
//	filter(exg_filt, exg_data, filtered_exg_data, NUM_POINTS); 
	eegpanel_add_samples(panel, filtered_eeg_data, filtered_exg_data, trigg, NUM_POINTS);

	return TRUE;
}

int ProcessSelection(const ChannelSelection* selection, ChannelType type, void* user_data)
{
/*	int i;
	int num_ch = selection->num_chann;
	unsigned int* selected = selection->selection;

	addsamp = 0;
	// check that the selected channels are consecutive
	for (i=0; i<num_ch-1; i++) {
		if (selected[i+1] != selected[i]+1)
			return 0;
	}
	if (num_ch != NUM_CH)
		return 0;
*/	
	addsamp = 1;
	return 1;
}

int main(int argc, char* argv[])
{
	float fc, fc2;
	EEGPanel* panel;
	//int i = 20;

	//filt = create_filter_mean(120, NUM_CH);
	fc = 20.0 / (float) SAMPLING_RATE;
	fc2 = 30.0 / (float) SAMPLING_RATE;
	//filt = create_fir_filter_bandpass(fc, fc2, 20, NUM_CH, HAMMING_WINDOW);
	//filt = create_fir_filter_lowpass(fc, 100, NUM_CH, RECT_WINDOW);
	//exg_filt = create_fir_filter_lowpass(fc, 100, NUM_EXG_CH, RECT_WINDOW);
	filt = create_butterworth_filter(fc, 2, NUM_CH, 1);
	//filt = create_adhoc_filter(NUM_CH);
	//filt = create_integrate_filter(NUM_CH);
	//filt = create_fir_filter_mean(2,NUM_CH);
	exg_filt = create_chebychev_filter(0.1/*fc*/, 4, NUM_EXG_CH, 1, 0.1);

	g_thread_init(NULL);
	gdk_threads_init();
	gtk_init(&argc, &argv);
	
	
	panel = eegpanel_create();
	if (!panel)
		return 0;

	eegpanel_define_input(panel, NUM_CH, 8, 16, SAMPLING_RATE);
	panel->process_selection = ProcessSelection;


	eegpanel_show(panel, 1);
	g_timeout_add(UPDATE_PERIOD, timerfunc, panel);
	
	eegpanel_run(panel, 0);

/*	while(i--) {
		timerfunc(panel);
		sleep(3);
	}*/

	eegpanel_destroy(panel);
	destroy_filter(filt);

	return 0;
}
