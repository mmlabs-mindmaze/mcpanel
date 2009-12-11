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
static int apply_filter(struct InternalFilterState* filst, float** buff, float** temp, unsigned int ns);



/*************************************************************************
 *                                                                       *
 *                    Data processing implementation                     *
 *                                                                       *
 *************************************************************************/

void process_eeg(EEGPanel* pan, const float* eeg, float* tmpbuf, unsigned int ns)
{
	unsigned int i, j;
	struct InternalFilterState* filprm = pan->dta.filstate;
	float* data, *cureeg;
	unsigned int *sel = pan->eegsel.selection;
	unsigned int nc = pan->neeg;
	unsigned int nmax_ch = pan->nmax_eeg;

	cureeg = data = pan->eeg + pan->current_sample*nc;

	// Copy data of the selected channels
	for (i=0; i<ns; i++) 
		for (j=0; j<nc; j++) 
			data[i*nc+j] = eeg[i*nmax_ch + sel[j]];

	if (apply_filter(&(filprm[EEG_OFFSET_FILTER]),&data, &tmpbuf, ns)) {
		// copy last samples
		for (i=0; i<nc; i++)
			pan->eeg_offset[i] = data[nc*(ns-1)+i];
		// We restart the processing from the previous data
		SWAP_POINTERS(data, tmpbuf);
	}
	
	// Process decimation
	// TODO

	// Do referencing
	if (pan->eeg_ref_type == AVERAGE_REF)
		remove_common_avg_ref(data, nc, eeg, nmax_ch, ns);
	else if (pan->eeg_ref_type == ELECTRODE_REF)
		remove_electrode_ref(data, nc, eeg, nmax_ch, ns, pan->eeg_ref_elec);

	apply_filter(&(filprm[EEG_LOWPASS_FILTER]),&data, &tmpbuf, ns);
	apply_filter(&(filprm[EEG_HIGHPASS_FILTER]),&data, &tmpbuf, ns);
	
	// copy data to the destination buffer
	if (data != cureeg)
		memcpy(cureeg, data, ns*nc*sizeof(*data));
}


void process_exg(EEGPanel* pan, const float* exg, float* tmpbuf, unsigned int ns)
{
	unsigned int i, j, k;
	unsigned int nc = pan->nexg;
	struct InternalFilterState* filprm = pan->dta.filstate;
	float *data, *curexg;
	unsigned int *sel = pan->exgsel.selection;
	unsigned int nmax_ch = pan->nmax_exg;
	
	curexg = data = pan->exg + pan->current_sample*nc;

	// Copy data of the selected channels
	for (i=0; i<ns; i++) {
		for (j=0; j<nc; j++) {
			k = (sel[j]+1)%nmax_ch;
			data[i*nc+j] = exg[i*nmax_ch + sel[j]] - exg[i*nmax_ch + k];
		}
	}

	if (apply_filter(&(filprm[EXG_OFFSET_FILTER]),&data, &tmpbuf, ns)) {
		// copy last samples
		for (i=0; i<nc; i++)
			pan->exg_offset[i] = data[nc*(ns-1)+i];
		// We restart the processing from the previous data
		SWAP_POINTERS(data, tmpbuf);
	}
	
	// Process decimation
	// TODO

	apply_filter(&(filprm[EXG_LOWPASS_FILTER]),&data, &tmpbuf, ns);
	apply_filter(&(filprm[EXG_HIGHPASS_FILTER]),&data, &tmpbuf, ns);
	
	// copy data to the destination buffer
	if (data != curexg)
		memcpy(curexg, data, ns*nc*sizeof(*data));
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
	int reset_filters = 1;

	pan->decimation_offset = 0;


	// A positive values indicates that we wanted to change
	// specifically the number of samples displayed
	if (num_samples >= 0)
		reset_filters = 0;
	else
		num_samples = pan->num_samples;

	// Use the previous values if unspecified
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
	if (reset_filters)
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


void update_filter(EEGPanel* pan, EnumFilter type)
{
	float fs = pan->fs;
	FilterParam* param =  &(pan->filter_param[type]);
	struct InternalFilterState* iprm = &(pan->dta.filstate[type]);

	
	if (param->state) {
		if (!iprm->filt || ((param->freq/fs!=iprm->fc) || (iprm->numch != param->numch))) {
			destroy_filter(iprm->filt);
			iprm->fc = param->freq/fs;
			iprm->numch = param->numch;
			iprm->filt = create_butterworth_filter(iprm->numch,
			                                 DATATYPE_FLOAT,
			                                 iprm->fc, 2,
							 param->highpass);
		}
	}
	else {
		destroy_filter(iprm->filt);
		iprm->filt = NULL;
	}

	iprm->reinit = 1;
}

void destroy_dataproc(EEGPanel* pan)
{
	int i;

	for (i=0; i<NUM_FILTERS; i++)
		destroy_filter(pan->dta.filstate[i].filt);
	
	free(pan->eeg);
	free(pan->exg);
	free(pan->triggers);
	free(pan->eeg_offset);
	free(pan->exg_offset);
}

static int apply_filter(struct InternalFilterState* filprm, float** inout, float** tmpbuff, unsigned int ns)
{
	if (!filprm->filt)
		return 0;

	// Reset filter with incoming values if it has changed previously
	if (filprm->reinit) {
		init_filter(filprm->filt, *inout);
		filprm->reinit = 0;
	}
		
	// Filter and swap so that inout still hold the data
	filter_f(filprm->filt, *inout, *tmpbuff, ns);
	SWAP_POINTERS(*tmpbuff, *inout);

	return 1;
}
