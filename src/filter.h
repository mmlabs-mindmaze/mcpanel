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
	unsigned int curr_sample;
	unsigned int a_len;
	const float* a;
	unsigned int b_len;
	const float* b;
	float* xoff;
	float* yoff;
} dfilter;

void filter(dfilter* filt, const float* x, float* y, int num_samples);
void reset_filter(dfilter* filt);
dfilter* create_fir_filter_mean(unsigned int nsamples, unsigned int nchann);
dfilter* create_fir_filter_lowpass(float fc, unsigned int half_length, unsigned int nchann, KernelWindow window);
dfilter* create_fir_filter_highpass(float fc, unsigned int half_length, unsigned int nchann, KernelWindow window);
dfilter* create_fir_filter_bandpass(float fc_low, float fc_high, unsigned int half_length, unsigned int nchann, KernelWindow window);
dfilter* create_butterworth_filter(float fc, unsigned int num_pole, unsigned int num_chann, int highpass);
dfilter* create_chebychev_filter(float fc, unsigned int num_pole, unsigned int nchann, int highpass, float ripple);
dfilter* create_integrate_filter(unsigned int nchann);
dfilter* create_adhoc_filter(unsigned int nchann);
void destroy_filter(dfilter* filt);

#endif //FILTER_H
