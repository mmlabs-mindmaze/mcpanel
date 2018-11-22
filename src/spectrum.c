/*
    Copyright (C) 2018  MindMaze
    Nicolas Bourdaud <nicolas.bourdaud@gmail.com>

    This program is free software: you can redistribute it and/or modify
    modify it under the terms of the version 3 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <complex.h>
#include <math.h>

#include "spectrum.h"


/**
 * DOC: Online estimation of spectrum
 *
 * This spectrum estimator is implemented with a sliding discrete Fourier
 * transform (DFT) with a dampling factor to make the estimation numerically
 * stable.
 *
 * Estimation of spectrum is a quite well know problem whose literature about
 * it is rather extensive. This implement takes as reference an article from
 * Eric Jacobsen and Richard Lyons published in "DSP tips & tricks" part of
 * "IEEE signal processing magazine" of 2003 (1053-5888/03). This can be
 * accessed at:
 * https://pdfs.semanticscholar.org/525f/b581f9afe17b6ec21d6cb58ed42d1100943f.pdf
 */

#define DAMPLING_POW_N  0.95f

/**
 * spectrum_init() - initialize a spectrum estimator
 * @sp:         pointer to uninitialized spectrum estimator struct
 * @num_point:  number of point to estimate the DFT
 *
 * Use this function to initialize a fresh spectrum estimator. Note that the
 * internals of @sp are overwritten no matter what is they value, thus leading
 * to memory leak if they were previously initialized.
 */
LOCAL_FN
void spectrum_init(struct spectrum* sp, int num_point)
{
	int k, wlen;
	float dampling;

	// Amplitude of DFT is symetric, so computation will be only done of
	// half of the number of point used to estimate DFT (we round up since
	// number of point could be odd)
	wlen = (num_point + 1)/2;

	sp->input_ringbuffer = calloc(num_point, sizeof(*sp->input_ringbuffer));
	sp->dft = calloc(wlen, sizeof(*sp->dft));
	sp->w = malloc(wlen * sizeof(*sp->w));
	if (!sp->input_ringbuffer || !sp->dft || !sp->w)
		abort();

	dampling = powf(DAMPLING_POW_N, 1.0f/num_point);
	for (k = 0; k < wlen; k++)
		sp->w[k] = dampling*cexpf(I*(2*M_PI*k)/num_point);

	sp->num_point = num_point;
	sp->wlen = wlen;
	sp->curr = 0;
	sp->dampling = dampling;
}


/**
 * spectrum_deinit() - cleanup a spectrum estimator
 * @sp:         pointer to initialized spectrum estimator struct
 */
LOCAL_FN
void spectrum_deinit(struct spectrum* sp)
{
	free(sp->input_ringbuffer);
	free(sp->dft);
	free(sp->w);

	*sp = (struct spectrum) {0};
}


/**
 * spectrum_reset() - reset an initialized spectrum estimator
 * @sp:         pointer to initialized spectrum estimator struct
 * @num_point:  number of point to estimate the DFT
 *
 * Use this function to reset the internals of the initialized spectrum
 * estimator pointed by @sp.
 */
LOCAL_FN
void spectrum_reset(struct spectrum* sp, int num_point)
{
	spectrum_deinit(sp);
	spectrum_init(sp, num_point);
}


/**
 * spectrum_update() - update DFT of a spectrum estimator with new data
 * @sp:         pointer initialized spectrum estimator struct
 * @ns:         number of added sample
 * @data:       array of sample added to update the DFT (must be of length @ns)
 *
 * Call this function to update the estimation of the DFT for the estimator
 * pointed to by @sp with @ns new points of the signal.
 */
LOCAL_FN
void spectrum_update(struct spectrum* sp, int ns, const float* data)
{
	float input_diff;
	int i, k;
	int curr = sp->curr;

	for (i = 0; i < ns; i++) {
		input_diff = data[i] - DAMPLING_POW_N*sp->input_ringbuffer[curr];
		sp->input_ringbuffer[curr] = data[i];

		// Update the DFT of all frequency iteratively
		for (k = 0; k < sp->wlen; k++)
			sp->dft[k] = sp->dft[k]*sp->w[k] + input_diff;

		if (++curr >= sp->num_point)
			curr = 0;
	}

	sp->curr = curr;
}


/**
 * spectrum_get() - get the amplitude of current DFT
 * @sp:         pointer initialized spectrum estimator struct
 * @nfreq:      number of frequency component whose amplitude must be
 *              computed. It must be less or equal to the number point used
 *              to estimate the DFT divided by 2 (rounded upward).
 * @amplitude:  output array that will receive the amplitude (length @nfreq)
 *
 * This gets the amplitude of the @nfreq first frequency component of the DFT
 * currently estimated by @sp. The output is written in the array pointed to by
 * @amplitude.
 */
LOCAL_FN
void spectrum_get(struct spectrum* sp, int nfreq, float* amplitude)
{
	int k;
	float scale = 1.0f / sp->num_point;

	if (nfreq > sp->wlen)
		abort();

	for (k = 0; k < nfreq; k++)
		amplitude[k] = scale * cabsf(sp->dft[k]);
}
