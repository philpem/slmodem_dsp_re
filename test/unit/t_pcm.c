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

/*
 * ITU-T V.90 (09/98), Table 1: 128 Ucode values and their corresponding
 * G.711 codewords and linear representations.  These 128 literal rows
 * consume the table's 512 published result cells (mu-law code/linear and
 * A-law code/linear); the Ucode is the array index.  Keep this as a
 * transcription, rather than deriving it with the G.711 formulae, so it is
 * an oracle independent of both the reconstruction and dsplibs.o.
 */
struct v90_table1_row {
	unsigned char mu_code;
	int mu_linear;
	unsigned char a_code;
	int a_linear;
};

static const struct v90_table1_row v90_table1[128] = {
	{ 0xff,     0, 0xd5,     8 }, { 0xfe,     8, 0xd4,    24 },
	{ 0xfd,    16, 0xd7,    40 }, { 0xfc,    24, 0xd6,    56 },
	{ 0xfb,    32, 0xd1,    72 }, { 0xfa,    40, 0xd0,    88 },
	{ 0xf9,    48, 0xd3,   104 }, { 0xf8,    56, 0xd2,   120 },
	{ 0xf7,    64, 0xdd,   136 }, { 0xf6,    72, 0xdc,   152 },
	{ 0xf5,    80, 0xdf,   168 }, { 0xf4,    88, 0xde,   184 },
	{ 0xf3,    96, 0xd9,   200 }, { 0xf2,   104, 0xd8,   216 },
	{ 0xf1,   112, 0xdb,   232 }, { 0xf0,   120, 0xda,   248 },
	{ 0xef,   132, 0xc5,   264 }, { 0xee,   148, 0xc4,   280 },
	{ 0xed,   164, 0xc7,   296 }, { 0xec,   180, 0xc6,   312 },
	{ 0xeb,   196, 0xc1,   328 }, { 0xea,   212, 0xc0,   344 },
	{ 0xe9,   228, 0xc3,   360 }, { 0xe8,   244, 0xc2,   376 },
	{ 0xe7,   260, 0xcd,   392 }, { 0xe6,   276, 0xcc,   408 },
	{ 0xe5,   292, 0xcf,   424 }, { 0xe4,   308, 0xce,   440 },
	{ 0xe3,   324, 0xc9,   456 }, { 0xe2,   340, 0xc8,   472 },
	{ 0xe1,   356, 0xcb,   488 }, { 0xe0,   372, 0xca,   504 },
	{ 0xdf,   396, 0xf5,   528 }, { 0xde,   428, 0xf4,   560 },
	{ 0xdd,   460, 0xf7,   592 }, { 0xdc,   492, 0xf6,   624 },
	{ 0xdb,   524, 0xf1,   656 }, { 0xda,   556, 0xf0,   688 },
	{ 0xd9,   588, 0xf3,   720 }, { 0xd8,   620, 0xf2,   752 },
	{ 0xd7,   652, 0xfd,   784 }, { 0xd6,   684, 0xfc,   816 },
	{ 0xd5,   716, 0xff,   848 }, { 0xd4,   748, 0xfe,   880 },
	{ 0xd3,   780, 0xf9,   912 }, { 0xd2,   812, 0xf8,   944 },
	{ 0xd1,   844, 0xfb,   976 }, { 0xd0,   876, 0xfa,  1008 },
	{ 0xcf,   924, 0xe5,  1056 }, { 0xce,   988, 0xe4,  1120 },
	{ 0xcd,  1052, 0xe7,  1184 }, { 0xcc,  1116, 0xe6,  1248 },
	{ 0xcb,  1180, 0xe1,  1312 }, { 0xca,  1244, 0xe0,  1376 },
	{ 0xc9,  1308, 0xe3,  1440 }, { 0xc8,  1372, 0xe2,  1504 },
	{ 0xc7,  1436, 0xed,  1568 }, { 0xc6,  1500, 0xec,  1632 },
	{ 0xc5,  1564, 0xef,  1696 }, { 0xc4,  1628, 0xee,  1760 },
	{ 0xc3,  1692, 0xe9,  1824 }, { 0xc2,  1756, 0xe8,  1888 },
	{ 0xc1,  1820, 0xeb,  1952 }, { 0xc0,  1884, 0xea,  2016 },
	{ 0xbf,  1980, 0x95,  2112 }, { 0xbe,  2108, 0x94,  2240 },
	{ 0xbd,  2236, 0x97,  2368 }, { 0xbc,  2364, 0x96,  2496 },
	{ 0xbb,  2492, 0x91,  2624 }, { 0xba,  2620, 0x90,  2752 },
	{ 0xb9,  2748, 0x93,  2880 }, { 0xb8,  2876, 0x92,  3008 },
	{ 0xb7,  3004, 0x9d,  3136 }, { 0xb6,  3132, 0x9c,  3264 },
	{ 0xb5,  3260, 0x9f,  3392 }, { 0xb4,  3388, 0x9e,  3520 },
	{ 0xb3,  3516, 0x99,  3648 }, { 0xb2,  3644, 0x98,  3776 },
	{ 0xb1,  3772, 0x9b,  3904 }, { 0xb0,  3900, 0x9a,  4032 },
	{ 0xaf,  4092, 0x85,  4224 }, { 0xae,  4348, 0x84,  4480 },
	{ 0xad,  4604, 0x87,  4736 }, { 0xac,  4860, 0x86,  4992 },
	{ 0xab,  5116, 0x81,  5248 }, { 0xaa,  5372, 0x80,  5504 },
	{ 0xa9,  5628, 0x83,  5760 }, { 0xa8,  5884, 0x82,  6016 },
	{ 0xa7,  6140, 0x8d,  6272 }, { 0xa6,  6396, 0x8c,  6528 },
	{ 0xa5,  6652, 0x8f,  6784 }, { 0xa4,  6908, 0x8e,  7040 },
	{ 0xa3,  7164, 0x89,  7296 }, { 0xa2,  7420, 0x88,  7552 },
	{ 0xa1,  7676, 0x8b,  7808 }, { 0xa0,  7932, 0x8a,  8064 },
	{ 0x9f,  8316, 0xb5,  8448 }, { 0x9e,  8828, 0xb4,  8960 },
	{ 0x9d,  9340, 0xb7,  9472 }, { 0x9c,  9852, 0xb6,  9984 },
	{ 0x9b, 10364, 0xb1, 10496 }, { 0x9a, 10876, 0xb0, 11008 },
	{ 0x99, 11388, 0xb3, 11520 }, { 0x98, 11900, 0xb2, 12032 },
	{ 0x97, 12412, 0xbd, 12544 }, { 0x96, 12924, 0xbc, 13056 },
	{ 0x95, 13436, 0xbf, 13568 }, { 0x94, 13948, 0xbe, 14080 },
	{ 0x93, 14460, 0xb9, 14592 }, { 0x92, 14972, 0xb8, 15104 },
	{ 0x91, 15484, 0xbb, 15616 }, { 0x90, 15996, 0xba, 16128 },
	{ 0x8f, 16764, 0xa5, 16896 }, { 0x8e, 17788, 0xa4, 17920 },
	{ 0x8d, 18812, 0xa7, 18944 }, { 0x8c, 19836, 0xa6, 19968 },
	{ 0x8b, 20860, 0xa1, 20992 }, { 0x8a, 21884, 0xa0, 22016 },
	{ 0x89, 22908, 0xa3, 23040 }, { 0x88, 23932, 0xa2, 24064 },
	{ 0x87, 24956, 0xad, 25088 }, { 0x86, 25980, 0xac, 26112 },
	{ 0x85, 27004, 0xaf, 27136 }, { 0x84, 28028, 0xae, 28160 },
	{ 0x83, 29052, 0xa9, 29184 }, { 0x82, 30076, 0xa8, 30208 },
	{ 0x81, 31100, 0xab, 31232 }, { 0x80, 32124, 0xaa, 32256 },
};

int
main(void)
{
	int rc = 0;
	int i;

	/*
	 * Do not merge these two blocks into a differential comparison: each is
	 * deliberately reported against the Recommendation, so an identical
	 * shared departure by candidate and blob remains visible.
	 */
	diff_begin("V.90 Table 1/reconstruction");
	for (i = 0; i < 128; i++) {
		diff_eq_int("Ucode %ld mu linear",
			    ulaw2linear(v90_table1[i].mu_code),
			    v90_table1[i].mu_linear, i);
		diff_eq_int("Ucode %ld A linear",
			    alaw2linear(v90_table1[i].a_code),
			    v90_table1[i].a_linear, i);
	}
	rc |= diff_end();

	diff_begin("V.90 Table 1/blob");
	for (i = 0; i < 128; i++) {
		diff_eq_int("Ucode %ld mu linear",
			    ref_ulaw2linear(v90_table1[i].mu_code),
			    v90_table1[i].mu_linear, i);
		diff_eq_int("Ucode %ld A linear",
			    ref_alaw2linear(v90_table1[i].a_code),
			    v90_table1[i].a_linear, i);
	}
	rc |= diff_end();

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
