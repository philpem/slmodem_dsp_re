/*
 * fpm_rms.c -- scaled RMS of a sample block.
 *
 * Reconstructed from dsplibs.o fpm_rms.c, .text 0x0a99d0.
 *
 * Accumulates (x * 910 >> 15) * x per sample and takes the square root.
 * 910/32768 = 0.027771, which is 1/36 to four figures, so the result is
 * sqrt(sum(x^2) / 36) -- the scaling is headroom, chosen so the accumulator
 * stays inside 32 bits for up to 36 full-scale samples.
 *
 * Beyond that it does not.  The per-sample maximum is 29,785,203, so the sum
 * passes 0x7fffffff at 73 full-scale samples and wraps negative; it is then
 * handed to FPM_sqrt_dp as unsigned.  Every caller passes a runtime sample
 * count and b103's fragments are 160 samples, so this is not unreachable --
 * only unlikely, since an AGC-controlled signal sits well below full scale.
 *
 * Reproduced as-is under the project's rule: guard only where the contract
 * makes the input impossible, otherwise match the blob.  See D2 in
 * docs/deviations.md.
 */

#include "dsplib/fpm.h"

/* 910/32768 = 1/36, applied per sample to keep the accumulator in range. */
#define FPM_RMS_SCALE 0x38e

short
FPM_rms(const short *samples, unsigned short count)
{
	int sum = 0;
	unsigned i;

	for (i = 0; i < count; i++) {
		int x = samples[i];

		/*
		 * Scale first, then square: (x/36) * x rather than (x*x)/36.
		 * Doing it the other way would overflow a single term before
		 * the division could bring it back.
		 */
		sum += ((x * FPM_RMS_SCALE) >> 15) * x;
	}

	return (short)FPM_sqrt_dp((unsigned int)sum);
}
