/*
 * t_pcm.c -- exhaustive differential test of the G.711 companding routines.
 *
 * These functions are pure and have small input domains, so the test is
 * exhaustive rather than sampled: every one of the 65536 linear values and
 * all 256 code values are compared against the original object.  That makes
 * this the strongest possible equivalence result -- not "agrees on the cases
 * we thought of" but "agrees everywhere".
 *
 * It doubles as the proof that the Tier-1 rig itself works: symbol renaming,
 * the shared/stateful import split, and 32-bit linkage against dsplibs.o.
 */

#include "harness.h"
#include "dsplib/pcm.h"

/* The originals, reachable under their renamed identities. */
extern unsigned char ref_linear2alaw(int pcm_val);
extern int ref_alaw2linear(unsigned char a_val);
extern unsigned char ref_linear2ulaw(int pcm_val);
extern int ref_ulaw2linear(unsigned char u_val);
extern unsigned char ref_alaw2ulaw(unsigned char a_val);
extern unsigned char ref_ulaw2alaw(unsigned char u_val);

int
main(void)
{
	int rc = 0;
	int i;

	diff_begin("alaw2linear");
	for (i = 0; i < 256; i++)
		diff_eq_int("alaw2linear(0x%02lx)",
			    alaw2linear((unsigned char)i),
			    ref_alaw2linear((unsigned char)i), i);
	rc |= diff_end();

	diff_begin("ulaw2linear");
	for (i = 0; i < 256; i++)
		diff_eq_int("ulaw2linear(0x%02lx)",
			    ulaw2linear((unsigned char)i),
			    ref_ulaw2linear((unsigned char)i), i);
	rc |= diff_end();

	diff_begin("alaw2ulaw");
	for (i = 0; i < 256; i++)
		diff_eq_int("alaw2ulaw(0x%02lx)",
			    alaw2ulaw((unsigned char)i),
			    ref_alaw2ulaw((unsigned char)i), i);
	rc |= diff_end();

	diff_begin("ulaw2alaw");
	for (i = 0; i < 256; i++)
		diff_eq_int("ulaw2alaw(0x%02lx)",
			    ulaw2alaw((unsigned char)i),
			    ref_ulaw2alaw((unsigned char)i), i);
	rc |= diff_end();

	/*
	 * Full 16-bit signed sweep.  The encoders take `int`, and the blob's
	 * segment search saturates beyond +-32767, so the interesting domain
	 * is exactly the 16-bit range plus a little either side to confirm
	 * the saturation branch agrees too.
	 */
	diff_begin("linear2alaw");
	for (i = -32768; i <= 32767; i++)
		diff_eq_int("linear2alaw(%ld)",
			    linear2alaw(i), ref_linear2alaw(i), i);
	rc |= diff_end();

	diff_begin("linear2ulaw");
	for (i = -32768; i <= 32767; i++)
		diff_eq_int("linear2ulaw(%ld)",
			    linear2ulaw(i), ref_linear2ulaw(i), i);
	rc |= diff_end();

	diff_begin("linear2alaw/saturate");
	for (i = 32768; i <= 40000; i++) {
		diff_eq_int("linear2alaw(%ld)",
			    linear2alaw(i), ref_linear2alaw(i), i);
		diff_eq_int("linear2alaw(%ld)",
			    linear2alaw(-i), ref_linear2alaw(-i), -i);
	}
	rc |= diff_end();

	diff_begin("linear2ulaw/saturate");
	for (i = 32768; i <= 40000; i++) {
		diff_eq_int("linear2ulaw(%ld)",
			    linear2ulaw(i), ref_linear2ulaw(i), i);
		diff_eq_int("linear2ulaw(%ld)",
			    linear2ulaw(-i), ref_linear2ulaw(-i), -i);
	}
	rc |= diff_end();

	return rc;
}
