#ifndef FILTER_H
#define FILTER_H

typedef enum {
	BLACKMAN_WINDOW,
	HAMMING_WINDOW,
	RECT_WINDOW
} KernelWindow;


typedef struct _dfilter
{
	unsigned int num_chann;
	unsigned int a_len;
	const float* a;
	unsigned int b_len;
	const float* b;
	float* xoff;
	float* yoff;
} dfilter;

void filter(const dfilter* filt, const float* x, float* y, int num_samples);
dfilter* create_fir_filter_mean(unsigned int nsamples, unsigned int nchann);
dfilter* create_fir_filter_lowpass(float fc, unsigned int half_length, unsigned int nchann, KernelWindow window);
dfilter* create_fir_filter_highpass(float fc, unsigned int half_length, unsigned int nchann, KernelWindow window);
dfilter* create_fir_filter_bandpass(float fc_low, float fc_high, unsigned int half_length, unsigned int nchann, KernelWindow window);
void destroy_filter(dfilter* filt);

#endif //FILTER_H
