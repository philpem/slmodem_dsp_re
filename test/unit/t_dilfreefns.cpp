/*
 * t_dilfreefns.cpp -- differential test of six free functions claimed by the
 * VPcmV34Main leaf pass:
 *
 *     getSegmentPointer(PcmType, int)           0x31da0  115 B
 *     calculateDilLength(DilType, PcmType)      0x31e20  241 B
 *     float2Bits(float, unsigned char *, int)   0x4ec00  125 B
 *     setConstellationMask(...)                 0x33570  128 B
 *     setCodecConstellationMask(...)            0x335f0  128 B
 *     setDataBitRate(...)                       0x33690   28 B
 *
 * All callerless exported API (F7000's terms).  The first two are pure
 * functions of their arguments (and, for the DIL length, of the file-static
 * tables both sides carry their own transcription of -- comparing the two
 * IS comparing the transcriptions); `float2Bits` writes one byte per bit
 * into a caller buffer; the three mask setters write a `V90MappingParams`.
 *
 * `calculateDilLength(DilType, PcmType)`'s whole input space is four cells
 * plus the early return, so the sweep is EXHAUSTIVE rather than sampled, and
 * the answers are additionally cross-checked against the DESCRIPTOR overload
 * run over a descriptor built by `setDilDescriptor` from the same type --
 * the two must agree because they read the same tables.
 *
 * `getSegmentPointer` is swept over every boundary value and both laws:
 * the level grid brackets each of the sixteen boundaries one below, at, and
 * one above, plus INT_MIN/INT_MAX for the signed compare.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90DilDescriptorSettings.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Phase3Modulator.h"	/* PcmType */
#include "dsplib/V92CP.h"		/* float2Bits(float, uchar *, int) */

extern "C" {
unsigned int ref_getsegptr(int pcm, int level)
	asm("ref__Z17getSegmentPointer7PcmTypei");
unsigned int ref_calcdil(int type, int pcm)
	asm("ref__Z18calculateDilLength7DilType7PcmType");
unsigned int ref_calcdil_desc(void *d, int pcm)
	asm("ref__Z18calculateDilLengthP19tagV90DILdescriptor7PcmType");
void ref_f2b(float f, unsigned char *bits, int mode)
	asm("ref__Z10float2BitsfPhi");
void ref_setmask(void *params, int which, const short *mask)
	asm("ref_setConstellationMask");
void ref_setcodecmask(void *params, int which, const short *mask)
	asm("ref_setCodecConstellationMask");
void ref_setrate(void *params, int islong, int rate)
	asm("ref_setDataBitRate");
}

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)((lfsr >> 3) | 1u);
}

static void
fill_pair(unsigned char *a, unsigned char *b, unsigned n)
{
	unsigned i;

	for (i = 0; i < n; i++)
		a[i] = nextb();
	if (b)
		memcpy(b, a, n);
}

/* -------------------------------------------------- getSegmentPointer */

static int
run_getsegptr(void)
{
	static const int bounds[] = { 0x0f, 0x1f, 0x2f, 0x3f, 0x4f, 0x5f,
				      0x6f, 0x7f, 0x00 };
	long tag = 0;
	int pcm, bi, d;

	diff_begin("getSegmentPointer");

	/*
	 * PcmType 2 is OUT OF THE GRID: the row select is `pcm * 8` with no
	 * clamp in either implementation, so a third law reads past the
	 * sixteen copied boundaries into each side's own stack -- reproduced
	 * indeterminacy, not a differential trial (D561's rule; the mu-law
	 * row length already exercises the `pcm != 1` test).
	 */
	for (pcm = 0; pcm < 2; pcm++)
		for (bi = 0; bi < (int)(sizeof bounds / sizeof bounds[0]);
		     bi++)
			for (d = -1; d <= 1; d++) {
				int level = bounds[bi] + d;
				unsigned int r0, r1;

				r0 = getSegmentPointer((PcmType)pcm, level);
				r1 = ref_getsegptr(pcm, level);
				diff_eq_int("segment index (%ld)", (long)r0,
					    (long)r1, tag);
				tag++;
			}

	/* The signed compare's far corners. */
	diff_eq_int("INT_MIN (%ld)",
		    (long)getSegmentPointer(PCM_TYPE_MU_LAW, -0x7fffffff - 1),
		    (long)ref_getsegptr(0, -0x7fffffff - 1), tag);
	diff_eq_int("INT_MAX (%ld)",
		    (long)getSegmentPointer(PCM_TYPE_A_LAW, 0x7fffffff),
		    (long)ref_getsegptr(1, 0x7fffffff), tag + 1);

	return diff_end();
}

/* -------------------------------------------------- calculateDilLength */

static unsigned char descbuf[0x213 + 64];

static int
run_calcdil(void)
{
	long tag = 100;
	int type, pcm;
	int nonzero = 0;

	diff_begin("calculateDilLength(DilType, PcmType)");

	for (type = 0; type < 3; type++)	/* 2 takes the early return */
		for (pcm = 0; pcm < 2; pcm++) {
			unsigned int r0, r1;

			r0 = calculateDilLength((DilType)type, (PcmType)pcm);
			r1 = ref_calcdil(type, pcm);
			diff_eq_int("length (%ld)", (long)r0, (long)r1, tag);
			if (type > 1) {
				diff_eq_int("out of range answers 0 (%ld)",
					    (long)r0, 0, tag);
			} else {
				/*
				 * Cross-check against the descriptor overload
				 * over `setDilDescriptor`'s own expansion of
				 * the same type: same tables, same search,
				 * so the same number -- on both sides.
				 */
				memset(descbuf, 0, sizeof descbuf);
				setDilDescriptor(
				    (tagV90DILdescriptor *)(void *)descbuf,
				    (DilType)type);
				diff_eq_int("agrees with the descriptor"
					    " overload (%ld)", (long)r0,
					    (long)calculateDilLength(
					      (tagV90DILdescriptor *)(void *)
						  descbuf, (PcmType)pcm),
					    tag);
				diff_eq_int("and the blob agrees with its own"
					    " (%ld)", (long)r1,
					    (long)ref_calcdil_desc(descbuf,
								   pcm), tag);
				if (r0 > 0)
					nonzero = 1;
			}
			tag++;
		}

	diff_eq_int("a real length was produced", nonzero, 1, 0);

	return diff_end();
}

/* --------------------------------------------------------- float2Bits */

static int
run_f2b(void)
{
	static const float values[] = {
		0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 3.99993896484375f,
		-3.99993896484375f, 1.984375f, 0.0001220703125f,
		-0.0001220703125f, 2.5f, -1.75f, 0.33333333f, 7.0f, -7.0f
	};
	long tag = 200;
	int vi, mode, wrote = 0;

	diff_begin("float2Bits(float, unsigned char *, int)");

	for (mode = 0; mode < 3; mode++)	/* 2 must write nothing */
		for (vi = 0; vi < (int)(sizeof values / sizeof values[0]);
		     vi++) {
			unsigned char ba[64], bb[64], before[64];
			unsigned i;

			lfsr = 0x51a2u + (unsigned)tag;
			for (i = 0; i < 64; i++)
				ba[i] = nextb();
			memcpy(bb, ba, 64);
			memcpy(before, ba, 64);

			float2Bits(values[vi], ba + 8, mode);
			ref_f2b(values[vi], bb + 8, mode);

			diff_eq_int("the buffers agree, guards included"
				    " (%ld)", memcmp(ba, bb, 64) == 0, 1,
				    tag);
			if (mode == 2)
				diff_eq_int("mode 2 wrote nothing (%ld)",
					    memcmp(ba, before, 64) == 0, 1,
					    tag);
			else if (memcmp(ba, before, 64) != 0)
				wrote = 1;
			tag++;
		}

	diff_eq_int("the expansion wrote bits", wrote, 1, 0);

	return diff_end();
}

/* ------------------------------------------------- the three setters */

static unsigned char mpp[2][sizeof(V90MappingParams) + 64]
	__attribute__((aligned(8)));

#define MPP(s)	((V90MappingParams *)(void *)mpp[s])

static int
run_masks(void)
{
	long tag = 400;
	int trial, filled = 0;

	diff_begin("setConstellationMask / setCodecConstellationMask / "
		   "setDataBitRate");

	for (trial = 0; trial < 16; trial++) {
		short mask[8];
		/* 6 and 7 drive the `which < 6` clamp; -1 would index the
		 * table at [-1] in BOTH implementations and is left out --
		 * a fault is not a differential trial. */
		int which = trial % 8;
		int islong = trial & 1;
		int rate = (trial * 37) - 20;
		int i;

		lfsr = 0x77aau + (unsigned)trial * 0x9e37u;
		fill_pair(mpp[0], mpp[1], (unsigned)sizeof mpp[0]);
		for (i = 0; i < 8; i++)
			mask[i] = (short)(0x8421u >> (i & 3)
					  ^ (unsigned)(trial * 4099 + i));

		setConstellationMask(MPP(0), which, mask);
		ref_setmask(mpp[1], which, mask);
		setCodecConstellationMask(MPP(0), which, mask);
		ref_setcodecmask(mpp[1], which, mask);
		setDataBitRate(MPP(0), islong, rate);
		ref_setrate(mpp[1], islong, rate);

		diff_eq_int("the blocks agree, guard included (%ld)",
			    memcmp(mpp[0], mpp[1], sizeof mpp[0]) == 0, 1,
			    tag);
		diff_eq_int("the rate landed (%ld)", (long)MPP(0)->word_0,
			    (long)(unsigned)(rate + (islong ? 0x14 : 8)),
			    tag);
		if (MPP(0)->constellationSize[which < 6 ? which : 0] > 0)
			filled = 1;
		tag++;
	}

	diff_eq_int("some constellation was filled", filled, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_getsegptr();
	rc |= run_calcdil();
	rc |= run_f2b();
	rc |= run_masks();

	return rc;
}
