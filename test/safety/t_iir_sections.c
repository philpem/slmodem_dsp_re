/* Fixed-build contracts; the blob has no defined zero-section oracle. */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "dsplib/fpm_iir.h"

#ifdef DSPLIB_REPRODUCE_BUGS
#error This test requires the fixed build
#endif

static unsigned checks, failed;

static void
check(int got, int expected)
{
	checks++;
	if (got != expected) {
		failed++;
		fprintf(stderr, "check %u: got %d, expected %d\n",
			checks, got, expected);
	}
}

int
main(void)
{
	static const short sections[] = { 0, -1, -2, SHRT_MIN, -32767 };
	static const short values[] = { SHRT_MIN, -1234, -1, 0, 1, SHRT_MAX };
	static const short lengths[] = { 0, 1, 6 };
	short samples[6];
	unsigned s, v, n;
	short coeff[5] = { 0, 0, 0, 0, 16384 };
	short state[2] = { 12, 34 };

	/* No coefficients or history are required when no section runs. */
	for (s = 0; s < sizeof(sections) / sizeof(sections[0]); s++) {
		for (v = 0; v < sizeof(values) / sizeof(values[0]); v++)
			check(FPM_iir_filt(values[v], NULL, NULL, sections[s]),
			      values[v]);
		for (n = 0; n < sizeof(lengths) / sizeof(lengths[0]); n++) {
			memcpy(samples, values, sizeof(samples));
			FPM_iir_filt_block(samples, NULL, NULL, sections[s], lengths[n]);
			for (v = 0; v < sizeof(values) / sizeof(values[0]); v++)
				check(samples[v], values[v]);
		}
	}

	/* A positive section still processes its history. */
	check(FPM_iir_filt(1234, coeff, state, 1), 1234);
	check(state[0], 34);
	check(state[1], 1234);
	printf("fixed IIR section guards: %u checks, %u failed\n", checks, failed);
	return failed != 0;
}
