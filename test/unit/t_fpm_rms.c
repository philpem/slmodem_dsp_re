/*
 * t_fpm_rms.c -- differential test of the scaled block RMS.
 *
 * Covers the ordinary range, then deliberately pushes past the point where the
 * accumulator wraps (73 full-scale samples), because that region is reachable
 * from a 160-sample fragment and the reconstruction reproduces the wrap rather
 * than guarding it.
 */

#include "harness.h"
#include "dsplib/fpm.h"

extern short ref_FPM_rms(const short *samples, unsigned short count);

#define NMAX 512
static short buf[NMAX];

static int
sweep(const char *label, int count, int fill)
{
	unsigned lfsr = 0x2C0FFEEu;
	int i;

	diff_begin(label);
	for (i = 0; i < NMAX; i++) {
		switch (fill) {
		case 0:				/* full scale, worst case  */
			buf[i] = 32767;
			break;
		case 1:				/* alternating full scale  */
			buf[i] = (i & 1) ? -32768 : 32767;
			break;
		case 2:				/* small signal            */
			buf[i] = (short)((i % 97) - 48);
			break;
		default:			/* pseudorandom            */
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xD0000001u);
			buf[i] = (short)(lfsr & 0xffff);
			break;
		}
	}

	for (i = 0; i <= count; i++)
		diff_eq_int("count %ld", FPM_rms(buf, (unsigned short)i),
			    ref_FPM_rms(buf, (unsigned short)i), i);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	/* Below the overflow point: 73 full-scale samples. */
	rc |= sweep("rms small signal", NMAX - 1, 2);
	rc |= sweep("rms pseudorandom", NMAX - 1, 3);

	/* Straight through the overflow point and well past it. */
	rc |= sweep("rms full scale (wraps)", NMAX - 1, 0);
	rc |= sweep("rms alternating (wraps)", NMAX - 1, 1);

	/* Zero count must return zero without touching the buffer. */
	diff_begin("rms degenerate");
	diff_eq_int("count 0 (%ld)", FPM_rms(buf, 0), ref_FPM_rms(buf, 0), 0);
	rc |= diff_end();

	return rc;
}
