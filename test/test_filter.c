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
