#include "filter.h"
#include <memory.h>
#include <stdlib.h>
#include <math.h>


void apply_window(float* fir, unsigned int length, KernelWindow window)
{
	int i;
	float M = length-1;

	switch (window) {
	case HAMMING_WINDOW:
		for (i=0; i<length; i++)
			fir[i] *= 0.54 + 0.46*cos(2.0*M_PI*((double)i/M - 0.5));
		break;

	case BLACKMAN_WINDOW:
		for (i=0; i<length; i++)
			fir[i] *= 0.42 + 0.5*cos(2.0*M_PI*((double)i/M - 0.5)) + 0.08*cos(4.0*M_PI*((double)i/M - 0.5));
		break;

	case RECT_WINDOW:
		break;
	}
}


void normalize_fir(float* fir, unsigned int length)
{
	int i;
	double sum = 0.0;

	for (i=0; i<length; i++)
		sum += fir[i];

	for (i=0; i<length; i++)
		fir[i] /= sum;
}

void compute_convolution(float* product, float* sig1, unsigned int len1, float* sig2, unsigned int len2)
{
	int i,j;

	memset(product, 0, (len1+len2-1)*sizeof(*product));
	
	for (i=0; i<len1; i++)
		for (j=0; j<len2; j++)
			product[i+j] += sig1[i]*sig2[j];
}


void compute_fir_lowpass(float* fir, unsigned int length, float fc)
{
	int i;
	float half_len = (double)((unsigned int)(length/2));

	for (i=0; i<length; i++)
		if (i != length/2)
			fir[i] = sin(2.0*M_PI*(double)fc*((double)i-half_len)) / ((double)i-half_len);
		else
			fir[i] = 2.0*M_PI*fc;
}

void reverse_fir(float* fir, unsigned int length)
{
	int i;

	// compute delay minus lowpass fir
	for (i=0; i<length; i++)
		fir[i] = -1.0 * fir[i];
	fir[length-1] += 1.0;
}


dfilter* create_fir_filter(unsigned int fir_length, unsigned int nchann, float** fir_out)
{
	dfilter* filt = NULL;
	float* fir = NULL;
	float* off = NULL;

	filt = malloc(sizeof(*filt));
	fir = malloc(fir_length*sizeof(*fir));
	off = malloc((fir_length-1)*nchann*sizeof(*off));

	// handle memory allocation problem
	if (!filt || !fir || !off) {
		free(filt);
		free(fir);
		free(off);
		return NULL;
	}

	memset(filt, 0, sizeof(*filt));
	memset(off, 0, (fir_length-1)*nchann*sizeof(*off)); 
	
	// prepare the filt struct
	filt->a = fir;
	filt->num_chann = nchann;
	filt->a_len = fir_length;
	filt->xoff = off;
	
	if (fir_out)
		*fir_out = fir;

	return filt;
}

void destroy_filter(dfilter* filt)
{
	free((void*)(filt->a));
	free((void*)(filt->b));
	free(filt->xoff);
	free(filt->yoff);
	free(filt);
}


void filter(const dfilter* filt, const float* in, float* out, int nsamples)
{
	int i, k, ichann, io, ii, num;
	const float* x;
	const float* y;
	unsigned int a_len = filt->a_len;
	const float* a = filt->a;
	unsigned int b_len = filt->b_len;
	const float* b = filt->b;
	unsigned int nchann = filt->num_chann;
	const float* xprev = filt->xoff + (a_len-1)*nchann;
	const float* yprev = filt->yoff + (b_len-1)*nchann;

	memset(out, 0, nchann*nsamples*sizeof(*out));

	// compute the product of convolution of the input with the finite
	// impulse response (fir)
	for (i=0; i<nsamples; i++) {
		io = i*nchann;
		for (k=0; k<a_len; k++) {
			ii = (i-k)*nchann;

			// If the convolution must be done with samples not
			// provided, use the stored ones
			x = (i-k >= 0) ? in : xprev;
			
			for (ichann=0; ichann<nchann; ichann++)
				out[io+ichann] += a[k]*x[ii+ichann];
		}

		// compute the convolution in the denominator
		for (k=0; k<b_len; k++) {
			ii = (i-k-1)*nchann;

			// If the convolution must be done with samples not
			// provided, use the stored ones
			y = (i-k >= 0) ? out : yprev;
			
			for (ichann=0; ichann<nchann; ichann++)
				out[io+ichann] += b[k]*y[ii+ichann];
		}
	}

	// store the last samples
	if (a_len) {
		num = a_len-1 - nsamples;
		if (num > 0)
			memmove(filt->xoff, filt->xoff + nsamples*nchann, num*nchann*sizeof(*out));
		else
			num = 0;
		memcpy(filt->xoff + num*nchann, in+(nsamples-a_len+1+num)*nchann, (a_len-1-num)*nchann*sizeof(*out));
	}

	// store the last output
	if (b_len) {
		num = b_len-1 - nsamples;
		if (num > 0)
			memmove(filt->yoff, filt->yoff + nsamples*nchann, num*nchann*sizeof(*out));
		else
			num = 0;
		memcpy(filt->yoff + num*nchann, in+(nsamples-b_len+1+num)*nchann, (b_len-1-num)*nchann*sizeof(*out));
	}
}


///////////////////////////////////////////////////////////////////////////////
//
//			Create particular filters
//
///////////////////////////////////////////////////////////////////////////////
dfilter* create_fir_mean(unsigned int fir_length, unsigned int nchann)
{
	int i;
	float* fir = NULL;
	dfilter* filt;

	filt = create_fir_filter(fir_length, nchann, &fir);
	if (!filt)
		return NULL;

	// prepare the finite impulse response
	for (i=0; i<fir_length; i++)
		fir[i] = 1.0f/(float)fir_length;

	return filt;
}

dfilter* create_fir_filter_lowpass(float fc, unsigned int half_length, unsigned int nchann, KernelWindow window)
{
	float* fir = NULL;
	dfilter* filt;
	unsigned int fir_length = 2*half_length + 1;

	filt = create_fir_filter(fir_length, nchann, &fir);
	if (!filt)
		return NULL;

	// prepare the finite impulse response
	compute_fir_lowpass(fir, fir_length, fc);
	apply_window(fir, fir_length, window);
	normalize_fir(fir, fir_length);

	return filt;
}


dfilter* create_fir_filter_highpass(float fc, unsigned int half_length, unsigned int nchann, KernelWindow window)
{
	float* fir = NULL;
	dfilter* filt;
	unsigned int fir_length = 2*half_length + 1;

	filt = create_fir_filter(fir_length, nchann, &fir);
	if (!filt)
		return NULL;

	// prepare the finite impulse response
	compute_fir_lowpass(fir, fir_length, fc);
	apply_window(fir, fir_length, window);
	normalize_fir(fir, fir_length);
	reverse_fir(fir, fir_length);

	return filt;
}


dfilter* create_fir_filter_bandpass(float fc_low, float fc_high, unsigned int half_length, unsigned int nchann, KernelWindow window)
{
	unsigned int len = 2*(half_length/2)+1;
	float fir_low[len], fir_high[len];
	float* fir = NULL;
	dfilter* filt;
	unsigned int fir_length = 2*half_length + 1;
	

	filt = create_fir_filter(fir_length, nchann, &fir);
	if (!filt)
		return NULL;

	// Create the lowpass finite impulse response
	compute_fir_lowpass(fir_low, len, fc_low);
	apply_window(fir_low, len, window);
	normalize_fir(fir_low, len);

	// Create the highpass finite impulse response
	compute_fir_lowpass(fir_high, len, fc_high);
	apply_window(fir_high, len, window);
	normalize_fir(fir_high, len);
	reverse_fir(fir_high, len);

	// compute the convolution product of the two FIR
	compute_convolution(fir, fir_low, len, fir_high, len);

	return filt;
}
