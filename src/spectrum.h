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
#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <complex.h>

struct spectrum {
	int num_point;
	int curr;
	int wlen;
	float dampling;
	float* input_ringbuffer;
	complex float* dft;
	complex float* w;
};

void spectrum_init(struct spectrum* sp, int num_point);
void spectrum_reset(struct spectrum* sp, int num_point);
void spectrum_deinit(struct spectrum* sp);
void spectrum_update(struct spectrum* sp, int ns, const float* data);
void spectrum_get(struct spectrum* sp, int nfreq, float* amplitude);

#endif
