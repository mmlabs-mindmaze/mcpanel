/*
	Copyright (C) 2008-2009 Nicolas Bourdaud <nicolas.bourdaud@epfl.ch>

    This file is part of the eegpanel library

    The eegpan library is free software: you can redistribute it and/or
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "eegpanel.h"
#include "eegpanel_shared.h"
#include "eegpanel_dataproc.h"
#include <stdlib.h>
#include <string.h>
#include <common-filters.h>


#define SWAP_POINTERS(pointer1, pointer2)	do {	\
	void* temp = pointer2;				\
	pointer2 = pointer1;				\
	pointer1 = temp;				\
} while(0)


/*************************************************************************
 *                                                                       *
 *                    internal functions declarations                    *
 *                                                                       *
 *************************************************************************/

void process_eeg(EEGPanel* pan, const float* eeg, float* temp_buff, unsigned int n_samples);
void process_exg(EEGPanel* pan, const float* exg, float* temp_buff, unsigned int n_samples);
void process_tri(EEGPanel* pan, const uint32_t* tri, uint32_t* temp_buff, unsigned int n_samples);
void remove_electrode_ref(float* data, unsigned int nchann, const float* fullset, unsigned int nchann_full, unsigned int num_samples, unsigned int elec_ref);
void remove_common_avg_ref(float* data, unsigned int nchann, const float* fullset, unsigned int nchann_full, unsigned int num_samples);
void set_all_filters(EEGPanel* pan, FilterParam* options);




/*************************************************************************
 *                                                                       *
 *                    Data processing implementation                     *
 *                                                                       *
 *************************************************************************/

void process_eeg(EEGPanel* pan, const float* eeg, float* temp_buff, unsigned int n_samples)
{
	unsigned int i, j;
	unsigned int nchann = pan->neeg;
	hfilter filt;
	float* buff1, *buff2, *curr_eeg;
	unsigned int *sel = pan->eegsel.selection;
	unsigned int num_ch = pan->neeg;
	unsigned int nmax_ch = pan->nmax_eeg;
	
	buff1 = temp_buff;
	curr_eeg = pan->eeg + pan->current_sample*nchann;
	buff2 = curr_eeg;
 

	// Copy data of the selected channels
	for (i=0; i<n_samples; i++) {
		for (j=0; j<num_ch; j++)
			buff2[i*num_ch+j] = eeg[i*nmax_ch + sel[j]];
	}
	SWAP_POINTERS(buff1, buff2);

	
	filt = pan->dta.filt[EEG_OFFSET_FILTER];
	if (filt) {
		filter_f(filt, buff1, buff2, n_samples);
		// copy last samples
		for (i=0; i<num_ch; i++)
			pan->eeg_offset[i] = buff2[nchann*(n_samples-1)+i];
	}
	
	// Do referencing
	if (pan->eeg_ref_type == AVERAGE_REF)
		remove_common_avg_ref(buff1, nchann, eeg, nmax_ch, n_samples);
	else if (pan->eeg_ref_type == ELECTRODE_REF)
		remove_electrode_ref(buff1, nchann, eeg, nmax_ch, n_samples, pan->eeg_ref_elec);

	// Process decimation
	// TODO
	
	

	filt = pan->dta.filt[EEG_LOWPASS_FILTER];
	if (filt) {
		filter_f(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);		
	}

	filt = pan->dta.filt[EEG_HIGHPASS_FILTER];
	if (filt) {
		filter_f(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);
	}
	
	// copy data to the destination buffer
	if (buff1 != curr_eeg)
		memcpy(curr_eeg, buff1, n_samples*nchann*sizeof(*buff1));
}


void process_exg(EEGPanel* pan, const float* exg, float* temp_buff, unsigned int n_samples)
{
	unsigned int i, j, k;
	unsigned int nchann = pan->nexg;
	hfilter filt;
	float* buff1, *buff2, *curr_exg;
	unsigned int *sel = pan->exgsel.selection;
	unsigned int num_ch = pan->nexg;
	unsigned int nmax_ch = pan->nmax_exg;
	
	buff1 = temp_buff;
	curr_exg = pan->exg + pan->current_sample*nchann;
	buff2 = curr_exg;
 

	// Copy data of the selected channels
	for (i=0; i<n_samples; i++) {
		for (j=0; j<num_ch; j++) {
			k = (sel[j]+1)%nmax_ch;
			buff2[i*num_ch+j] = exg[i*nmax_ch + sel[j]] - exg[i*nmax_ch + k];
		}
	}
	SWAP_POINTERS(buff1, buff2);

	
	filt = pan->dta.filt[EXG_OFFSET_FILTER];
	if (filt) {
		filter_f(filt, buff1, buff2, n_samples);
		// copy last samples
		for (i=0; i<num_ch; i++)
			pan->exg_offset[i] = buff2[nchann*(n_samples-1)+i];
	}

	// Process decimation
	// TODO

	filt = pan->dta.filt[EXG_LOWPASS_FILTER];
	if (filt) {
		filter_f(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);		
	}

	filt = pan->dta.filt[EXG_HIGHPASS_FILTER];
	if (filt) {
		filter_f(filt, buff1, buff2, n_samples);
		SWAP_POINTERS(buff1, buff2);
	}
	
	// copy data to the destination buffer
	if (buff1 != curr_exg)
		memcpy(curr_exg, buff1, n_samples*nchann*sizeof(*buff1));
}

#define CMS_IN_RANGE	0x100000
#define LOW_BATTERY	0x400000
void process_tri(EEGPanel* pan, const uint32_t* tri, uint32_t* temp_buff, unsigned int n_samples)
{
	unsigned int i;
	uint32_t* buff1, *buff2, *curr_tri;
	
	buff1 = temp_buff;
	curr_tri = pan->triggers + pan->current_sample;
	buff2 = curr_tri;

	pan->flags.cms_in_range = 1;
	pan->flags.low_battery = 0;

	// Copy data and set the states of the system
	for (i=0; i<n_samples; i++) {
		buff2[i] = tri[i];
		if ((CMS_IN_RANGE & tri[i]) == 0)
			pan->flags.cms_in_range = 0;
		if (LOW_BATTERY & tri[i])
			pan->flags.low_battery = 1;
	}
}

void remove_common_avg_ref(float* data, unsigned int nchann, const float* fullset, unsigned int nchann_full, unsigned int num_samples)
{
	unsigned int i, j;
	float sum;

	for (i=0; i<num_samples; i++) {
		// calculate the sum
		sum = 0;
		for (j=0; j<nchann_full; j++)
			sum += fullset[i*nchann_full+j];
		sum /= (float)nchann_full;

		// reference the data
		for (j=0; j<nchann; j++)
			data[i*nchann+j] -= sum;
	}
}

void remove_electrode_ref(float* data, unsigned int nchann, const float* fullset, unsigned int nchann_full, unsigned int num_samples, unsigned int elec_ref)
{
	unsigned int i, j;

	for (i=0; i<num_samples; i++) {
		// reference the data
		for (j=0; j<nchann; j++)
			data[i*nchann+j] -= fullset[i*nchann_full+elec_ref];
	}
}


int set_data_input(EEGPanel* pan, int num_samples)
{
	unsigned int num_eeg_points, num_exg_points, num_tri_points, num_eeg, num_exg;
	float *eeg, *exg;
	uint32_t *triggers;

	pan->decimation_offset = 0;

	// Use the previous values if unspecified
	num_samples = (num_samples>=0) ? num_samples : (int)(pan->num_samples);
	num_eeg = pan->eegsel.num_chann;
	num_exg = pan->exgsel.num_chann;

	num_eeg_points = num_samples*num_eeg;
	num_exg_points = num_samples*num_exg;
	num_tri_points = num_samples;

	if (num_eeg_points != pan->neeg*pan->num_samples) {
		free(pan->eeg);
		pan->eeg = malloc(num_eeg_points*sizeof(*eeg));
	}
	memset(pan->eeg, 0, num_eeg_points*sizeof(*eeg));

	if (num_exg_points != pan->nexg*pan->num_samples) {
		free(pan->exg);
		pan->exg = malloc(num_exg_points * sizeof(*exg));
	}
	memset(pan->exg, 0, num_exg_points*sizeof(*eeg));

	if (num_tri_points != pan->num_samples) {
		free(pan->triggers);
		pan->triggers = malloc(num_tri_points * sizeof(*triggers));
	}
	memset(pan->triggers, 0, num_tri_points*sizeof(*triggers));

	if (num_eeg != pan->neeg) {
		free(pan->eeg_offset);
		pan->eeg_offset = malloc(num_eeg*sizeof(*(pan->eeg_offset)));
		pan->neeg = num_eeg;
	}


	if (num_exg != pan->nexg) {
		free(pan->exg_offset);
		pan->exg_offset = malloc(num_exg*sizeof(*(pan->exg_offset)));
		pan->nexg = num_exg;
	}

	// Update the filters
	set_all_filters(pan, NULL);
	
	pan->last_drawn_sample = pan->current_sample = 0;
	pan->num_samples = num_samples;

	set_databuff_gui(pan);

	return 1;
}


void add_samples(EEGPanel* pan, const float* eeg, const float* exg, const uint32_t* triggers, unsigned int num_samples)
{
	unsigned int num_eeg_ch, num_exg_ch, nmax_eeg, nmax_exg;
	unsigned int ns_w = 0;
	unsigned int pointer = pan->current_sample;
	unsigned int *eeg_sel, *exg_sel; 
	void* buff = NULL;
	unsigned int buff_len;

	num_eeg_ch = pan->neeg;
	num_exg_ch = pan->nexg;
	nmax_eeg = pan->nmax_eeg;
	nmax_exg = pan->nmax_exg;
	eeg_sel = pan->eegsel.selection;
	exg_sel = pan->exgsel.selection;

	

	// if we need to wrap, first add the tail
	if (num_samples+pointer > pan->num_samples) {
		ns_w = pan->num_samples - pointer;
		
		add_samples(pan, eeg, exg, triggers, ns_w);

		eeg += nmax_eeg*ns_w;
		exg += nmax_exg*ns_w;
		triggers += ns_w;
		num_samples -= ns_w;
		pointer = pan->current_sample;
	}
	
	// Create a buffer big enough to hold incoming eeg or exg data
	buff_len = (num_eeg_ch > num_exg_ch) ? num_eeg_ch : num_exg_ch;
	buff_len *= num_samples;
	buff = malloc(buff_len*sizeof(*eeg));

	// copy data
	if (eeg) 
		process_eeg(pan, eeg, buff, num_samples);
	if (exg) 
		process_exg(pan, exg, buff, num_samples);
	if (triggers)
		process_tri(pan, triggers, buff, num_samples);

	free(buff);

	// Update current pointer
	pointer += num_samples;
	if (pointer >= pan->num_samples)
		pointer -= pan->num_samples;
	pan->current_sample = pointer;
}


void set_one_filter(EEGPanel* pan, EnumFilter type, FilterParam* options, unsigned int nchann, int highpass)
{
	float fs = pan->fs;
	hfilter filt = pan->dta.filt[type];
	FilterParam* curr_param =  &(pan->filter_param[type]);
	FilterParam* param = options + type;

	
	if (param->state) {
		if (!filt || ((param->freq/fs!=curr_param->fc) || (pan->dta.numch[type] != nchann))) {
			destroy_filter(filt);
			param->fc = param->freq/fs;
			filt = create_butterworth_filter(param->fc, 2, nchann, highpass, DATATYPE_FLOAT);
			pan->filter_param[type] = *param;
			pan->dta.numch[type] = nchann;
		}
		else
			reset_filter(filt);
	}
	else {
		destroy_filter(filt);
		filt = NULL;
	}

	pan->dta.filt[type] = filt;
}

void destroy_dataproc(EEGPanel* pan)
{
	int i;

	for (i=0; i<NUM_FILTERS; i++)
		destroy_filter(pan->dta.filt[i]);
	
	free(pan->eeg);
	free(pan->exg);
	free(pan->triggers);
	free(pan->eeg_offset);
	free(pan->exg_offset);
}
