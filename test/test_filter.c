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
#include "filter.h"

#define NCHANN	1
#define NSAMPLE	1
#define NITER 10000


int main(int argc, char* argv[])
{
	int i, j, k;
	dfilter* filt;
	float buff1[NCHANN*NSAMPLE], buff2[NCHANN*NSAMPLE];
	volatile float dummy;

	// set signals
	for (i=0; i<NCHANN; i++)
		for (j=0; j<NSAMPLE; j++)
			buff1[j*NCHANN+i] = j;


	filt = create_butterworth_filter(0.02, 4, NCHANN, 0);
	for (k=0; k<NITER; k++) {
		filter(filt, buff1, buff2, NSAMPLE);
		for (i=0; i<NCHANN*NSAMPLE; i++) {
			buff1[i] = dummy = buff2[i];
		}
	}


	return 0;
}
