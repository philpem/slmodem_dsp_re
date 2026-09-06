/*
 * t_jdmpleaves.cpp -- differential test of seventeen leaf setters claimed by
 * the VPcmV34Main leaf pass:
 *
 *     V90Jd::setMaxLookahead / setConstelSize / setRatesMask / resetCrc
 *     V92Jd::setJdPhase / setMaxLookahead / setRatesMask / setConstelSize
 *            / resetCrc
 *     V90MP::resetCRC / resetDetector / calcSequenceLength / PrintBase2
 *     ModulusEncoder::ModulusEncoder(j*7)  (C1 and C2)
 *     ModulusDecoder::ModulusDecoder(j*7)  (C1 and C2)
 *
 * All exported API with no caller anywhere in the object -- `readelf -r`
 * finds no relocation naming any of them -- so the `ref_` aliases are the
 * only way to drive the blob's copies, exactly as t_v90p4seq argues for its
 * three (finding F7000).
 *
 * THE FIXTURE IS t_v90jd's: the objects are SEEDED, never zeroed, identically
 * on both sides per trial, compared whole with a guard past the end, and
 * every member's writes are anti-vacuity-checked so a body that stopped
 * writing could not pass on seed agreeing with seed (findings F223, F224).
 *
 * `PrintBase2` never reads `this` -- no instruction in its 81 bytes touches
 * 0x4(%esp)'s pointee -- and the fixture CHECKS that rather than assuming it:
 * the object is compared after every call like everyone else's.  Its output
 * strings are compared byte for byte, including the terminator position, over
 * widths 0 (minimal form, leading-zero suppression) through 16 and 32.
 *
 * `calcSequenceLength` divides by `groupSize` with a plain `div`, so zero is
 * out of the grid: the object faults there and a faulting trial is not a
 * differential trial.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/ModulusCoder.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V90MP.h"
#include "dsplib/V92Jd.h"

extern "C" {
void ref_v90jd_setmax(void *self, unsigned char v)
	asm("ref__ZN5V90Jd15setMaxLookaheadEh");
void ref_v90jd_setconstel(void *self, unsigned char a, unsigned char b)
	asm("ref__ZN5V90Jd14setConstelSizeEhh");
void ref_v90jd_setrates(void *self, int mask)
	asm("ref__ZN5V90Jd12setRatesMaskEi");
void ref_v90jd_resetcrc(void *self) asm("ref__ZN5V90Jd8resetCrcEv");

void ref_v92jd_setphase(void *self, float f)
	asm("ref__ZN5V92Jd10setJdPhaseEf");
void ref_v92jd_setmax(void *self, unsigned char v)
	asm("ref__ZN5V92Jd15setMaxLookaheadEh");
void ref_v92jd_setconstel(void *self, unsigned char a, unsigned char b)
	asm("ref__ZN5V92Jd14setConstelSizeEhh");
void ref_v92jd_setrates(void *self, int mask)
	asm("ref__ZN5V92Jd12setRatesMaskEi");
void ref_v92jd_resetcrc(void *self) asm("ref__ZN5V92Jd8resetCrcEv");

void ref_v90mp_resetcrc(void *self) asm("ref__ZN5V90MP8resetCRCEv");
void ref_v90mp_resetdet(void *self) asm("ref__ZN5V90MP13resetDetectorEv");
void ref_v90mp_calcseq(void *self)
	asm("ref__ZN5V90MP18calcSequenceLengthEv");
void ref_v90mp_printbase2(void *self, char *out, unsigned long v,
			  unsigned short nbits)
	asm("ref__ZN5V90MP10PrintBase2EPcmt");

void our_enc7_c1(void *self, unsigned a, unsigned b, unsigned c, unsigned d,
		 unsigned e, unsigned f, unsigned g)
	asm("_ZN14ModulusEncoderC1Ejjjjjjj");
void our_enc7_c2(void *self, unsigned a, unsigned b, unsigned c, unsigned d,
		 unsigned e, unsigned f, unsigned g)
	asm("_ZN14ModulusEncoderC2Ejjjjjjj");
void our_dec7_c1(void *self, unsigned a, unsigned b, unsigned c, unsigned d,
		 unsigned e, unsigned f, unsigned g)
	asm("_ZN14ModulusDecoderC1Ejjjjjjj");
void our_dec7_c2(void *self, unsigned a, unsigned b, unsigned c, unsigned d,
		 unsigned e, unsigned f, unsigned g)
	asm("_ZN14ModulusDecoderC2Ejjjjjjj");
void ref_enc7_c1(void *self, unsigned a, unsigned b, unsigned c, unsigned d,
		 unsigned e, unsigned f, unsigned g)
	asm("ref__ZN14ModulusEncoderC1Ejjjjjjj");
void ref_enc7_c2(void *self, unsigned a, unsigned b, unsigned c, unsigned d,
		 unsigned e, unsigned f, unsigned g)
	asm("ref__ZN14ModulusEncoderC2Ejjjjjjj");
void ref_dec7_c1(void *self, unsigned a, unsigned b, unsigned c, unsigned d,
		 unsigned e, unsigned f, unsigned g)
	asm("ref__ZN14ModulusDecoderC1Ejjjjjjj");
void ref_dec7_c2(void *self, unsigned a, unsigned b, unsigned c, unsigned d,
		 unsigned e, unsigned f, unsigned g)
	asm("ref__ZN14ModulusDecoderC2Ejjjjjjj");
}

/* ------------------------------------------------------------- storage */

#define GUARD	64

#define JD90_SLOT	((unsigned)sizeof(V90Jd) + GUARD)
#define JD92_SLOT	((unsigned)sizeof(V92Jd) + GUARD)
#define MP_SLOT		((unsigned)sizeof(V90MP) + GUARD)
#define MC_SLOT		((unsigned)sizeof(ModulusEncoder) + GUARD)

static unsigned char jd90[2][JD90_SLOT] __attribute__((aligned(8)));
static unsigned char jd92[2][JD92_SLOT] __attribute__((aligned(8)));
static unsigned char mp[2][MP_SLOT] __attribute__((aligned(8)));
static unsigned char mc[2][MC_SLOT] __attribute__((aligned(8)));

#define JD90(s)	((V90Jd *)(void *)jd90[s])
#define JD92(s)	((V92Jd *)(void *)jd92[s])
#define MP_(s)	((V90MP *)(void *)mp[s])

/* ------------------------------------------------------------- seeding */

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)((lfsr >> 3) | 1u);	/* never zero; F230 */
}

static void
fill_pair(unsigned char *a, unsigned char *b, unsigned n, long trial)
{
	unsigned i;

	lfsr = 0x1b71u ^ (unsigned)trial * 0x9e37u;
	for (i = 0; i < n; i++)
		a[i] = nextb();
	memcpy(b, a, n);
}

#define NTRIAL	24

/* ------------------------------------------------------------- V90Jd */

static int
run_v90jd(void)
{
	int trial, moved = 0;

	diff_begin("V90Jd::{setMaxLookahead,setConstelSize,setRatesMask,"
		   "resetCrc}");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[JD90_SLOT];
		unsigned char v = (unsigned char)(trial * 37 + 1);
		unsigned char c1 = (unsigned char)(trial * 11 + 3);
		unsigned char c2 = (unsigned char)(trial * 29 + 5);
		/* Bit 27 and above set on some trials; negative on some. */
		int mask = (int)(0x91a2b3c4u * (unsigned)(trial + 1));

		fill_pair(jd90[0], jd90[1], JD90_SLOT, trial);
		memcpy(before, jd90[0], JD90_SLOT);

		JD90(0)->setMaxLookahead(v);
		ref_v90jd_setmax(jd90[1], v);
		JD90(0)->setConstelSize(c1, c2);
		ref_v90jd_setconstel(jd90[1], c1, c2);
		JD90(0)->setRatesMask(mask);
		ref_v90jd_setrates(jd90[1], mask);
		JD90(0)->resetCrc();
		ref_v90jd_resetcrc(jd90[1]);

		diff_eq_obj("after the four setters", V90Jd, JD90(0), JD90(1),
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    memcmp(jd90[0] + sizeof(V90Jd),
				   before + sizeof(V90Jd),
				   JD90_SLOT - sizeof(V90Jd)) == 0, 1, trial);

		/* Anti-vacuity: the seed can never satisfy all of these. */
		diff_eq_int("the lookahead pair landed (trial %ld)",
			    JD90(0)->bits[49] == (v & 1)
			    && JD90(0)->bits[50] == ((v >> 1) & 1), 1, trial);
		diff_eq_int("the constellation pair landed (trial %ld)",
			    JD90(0)->bits[47] == c1 && JD90(0)->bits[48] == c2,
			    1, trial);
		diff_eq_int("the crc is all ones (trial %ld)",
			    JD90(0)->crc[0] == 1 && JD90(0)->crc[15] == 1, 1,
			    trial);
		if (memcmp(before, jd90[0], JD90_SLOT) != 0)
			moved = 1;
	}

	diff_eq_int("the setters changed the object", moved, 1, 0);

	return diff_end();
}

/* ------------------------------------------------------------- V92Jd */

static int
run_v92jd(void)
{
	/*
	 * Phases whose Q16 forms exercise every bit of the low sixteen and
	 * both signs; nothing unordered -- a NaN's conversion is undefined in
	 * the reconstruction as much as in the object (D561's rule).
	 */
	static const float phases[] = {
		0.0f, 0.5f, -0.5f, 0.9999847f, -0.9999847f, 1.0f / 3.0f,
		123.456f, -77.125f, 0.0000152587890625f
	};
	int trial, moved = 0, distinct = 0;
	unsigned char firstphase[16];

	diff_begin("V92Jd::{setJdPhase,setMaxLookahead,setRatesMask,"
		   "setConstelSize,resetCrc}");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[JD92_SLOT];
		float ph = phases[trial % (int)(sizeof phases
						/ sizeof phases[0])];
		unsigned char v = (unsigned char)(trial * 41 + 2);
		unsigned char c1 = (unsigned char)(trial * 13 + 7);
		unsigned char c2 = (unsigned char)(trial * 17 + 9);
		int mask = (int)(0x5a5a1234u + 0x01030507u * (unsigned)trial);

		fill_pair(jd92[0], jd92[1], JD92_SLOT, 1000 + trial);
		memcpy(before, jd92[0], JD92_SLOT);

		JD92(0)->setJdPhase(ph);
		ref_v92jd_setphase(jd92[1], ph);
		JD92(0)->setMaxLookahead(v);
		ref_v92jd_setmax(jd92[1], v);
		JD92(0)->setRatesMask(mask);
		ref_v92jd_setrates(jd92[1], mask);
		JD92(0)->setConstelSize(c1, c2);
		ref_v92jd_setconstel(jd92[1], c1, c2);
		JD92(0)->resetCrc();
		ref_v92jd_resetcrc(jd92[1]);

		diff_eq_obj("after the five setters", V92Jd, JD92(0), JD92(1),
			    trial);
		diff_eq_int("no store past the object (trial %ld)",
			    memcmp(jd92[0] + sizeof(V92Jd),
				   before + sizeof(V92Jd),
				   JD92_SLOT - sizeof(V92Jd)) == 0, 1, trial);

		diff_eq_int("the constellation pair landed in phaseBits"
			    " (trial %ld)",
			    JD92(0)->phaseBits[48] == c1
			    && JD92(0)->phaseBits[49] == c2, 1, trial);
		diff_eq_int("the crc is all ones (trial %ld)",
			    JD92(0)->crc[0] == 1 && JD92(0)->crc[15] == 1, 1,
			    trial);
		if (memcmp(before, jd92[0], JD92_SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(firstphase, &JD92(0)->phaseBits[18], 16);
		else if (memcmp(firstphase, &JD92(0)->phaseBits[18], 16) != 0)
			distinct = 1;
	}

	diff_eq_int("the setters changed the object", moved, 1, 0);
	diff_eq_int("the phase bits are not the same on every trial",
		    distinct, 1, 0);

	return diff_end();
}

/* -------------------------------------------------------------- V90MP */

static int
run_v90mp(void)
{
	int trial, moved = 0, rounded = 0, exact = 0;

	diff_begin("V90MP::{resetCRC,resetDetector,calcSequenceLength}");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[MP_SLOT];
		unsigned int group = 1u + (unsigned)(trial % 7);

		fill_pair(mp[0], mp[1], MP_SLOT, 2000 + trial);

		/* The divisor is poked non-zero on both sides; see the head. */
		MP_(0)->groupSize = MP_(1)->groupSize = group;
		MP_(0)->bodyLength = MP_(1)->bodyLength =
		    (unsigned char)(trial * 31 + 5);
		memcpy(before, mp[0], MP_SLOT);

		MP_(0)->resetCRC();
		ref_v90mp_resetcrc(mp[1]);
		MP_(0)->resetDetector();
		ref_v90mp_resetdet(mp[1]);
		MP_(0)->calcSequenceLength();
		ref_v90mp_calcseq(mp[1]);

		diff_eq_obj("after the three", V90MP, MP_(0), MP_(1), trial);
		diff_eq_int("no store past the object (trial %ld)",
			    memcmp(mp[0] + sizeof(V90MP),
				   before + sizeof(V90MP),
				   MP_SLOT - sizeof(V90MP)) == 0, 1, trial);

		diff_eq_int("the crc is all ones (trial %ld)",
			    MP_(0)->crc[0] == 1 && MP_(0)->crc[15] == 1, 1,
			    trial);
		diff_eq_int("the detector was reset (trial %ld)",
			    MP_(0)->bitIndex == 18 && MP_(0)->rxState == 0
			    && MP_(0)->onesRun == 0 && MP_(0)->zerosRun == 0,
			    1, trial);

		/* Both arms of calcSequenceLength, counted. */
		if (((unsigned)MP_(0)->bodyLength + 1u) % group == 0u)
			exact++;
		else
			rounded++;
		if (memcmp(before, mp[0], MP_SLOT) != 0)
			moved = 1;
	}

	diff_eq_int("the three changed the object", moved, 1, 0);
	diff_eq_int("the rounding arm was reached", rounded > 0, 1, 0);
	diff_eq_int("the exact arm was reached", exact > 0, 1, 0);

	return diff_end();
}

/*
 * PrintBase2, over its own buffers.  36 characters is the most any input
 * here can produce (32 digits and slack); the buffers are seeded and the
 * TAIL past the terminator is compared too, so a body that wrote further
 * than the blob's fails on the seed it disturbed.
 */
#define STR_SLOT	64

static int
run_printbase2(void)
{
	static const unsigned long values[] = {
		0ul, 1ul, 0x2aaaul, 0x8000ul, 0xdeadbeeful, 0x80000000ul,
		0xfffffffful, 0x1234ul
	};
	static const unsigned short widths[] = { 0, 1, 5, 14, 16, 32 };
	int vi, wi, minimal = 0, padded = 0;
	long tag = 0;

	diff_begin("V90MP::PrintBase2");

	for (vi = 0; vi < (int)(sizeof values / sizeof values[0]); vi++)
		for (wi = 0; wi < (int)(sizeof widths / sizeof widths[0]);
		     wi++) {
			char sa[STR_SLOT], sb[STR_SLOT];
			unsigned char before[MP_SLOT];
			int i;

			fill_pair(mp[0], mp[1], MP_SLOT, 3000 + tag);
			memcpy(before, mp[0], MP_SLOT);
			for (i = 0; i < STR_SLOT; i++)
				sa[i] = sb[i] = (char)(0x41 + (i & 15));

			MP_(0)->PrintBase2(sa, values[vi], widths[wi]);
			ref_v90mp_printbase2(mp[1], sb, values[vi],
					     widths[wi]);

			diff_eq_int("the output agrees, tail included (%ld)",
				    memcmp(sa, sb, STR_SLOT) == 0, 1, tag);
			diff_eq_obj("this was not touched", V90MP, MP_(0),
				    MP_(1), tag);
			diff_eq_int("and ours really was untouched (%ld)",
				    memcmp(mp[0], before, MP_SLOT) == 0, 1,
				    tag);

			if (widths[wi] == 0 && values[vi] != 0)
				minimal++;
			if (widths[wi] >= 5)
				padded++;
			tag++;
		}

	diff_eq_int("the minimal form was driven", minimal > 0, 1, 0);
	diff_eq_int("the padded form was driven", padded > 0, 1, 0);

	return diff_end();
}

/* --------------------------------------------------- the modulus ctors */

typedef void (*ctor7)(void *, unsigned, unsigned, unsigned, unsigned,
		      unsigned, unsigned, unsigned);

static int
run_ctor7(const char *name, ctor7 ours, ctor7 theirs)
{
	int trial, moved = 0;

	diff_begin(name);

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[MC_SLOT];
		unsigned a[7];
		int i;

		fill_pair(mc[0], mc[1], MC_SLOT, 4000 + trial);
		memcpy(before, mc[0], MC_SLOT);
		for (i = 0; i < 7; i++)
			a[i] = 0x01010101u * (unsigned)(i + 1)
			       + 0x00010007u * (unsigned)trial;

		ours(mc[0], a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
		theirs(mc[1], a[0], a[1], a[2], a[3], a[4], a[5], a[6]);

		diff_eq_obj("after the ctor", ModulusEncoder,
			    (ModulusEncoder *)(void *)mc[0],
			    (ModulusEncoder *)(void *)mc[1], trial);
		diff_eq_int("no store past the object (trial %ld)",
			    memcmp(mc[0] + sizeof(ModulusEncoder),
				   before + sizeof(ModulusEncoder),
				   MC_SLOT - sizeof(ModulusEncoder)) == 0, 1,
			    trial);
		diff_eq_int("all seven landed in order (trial %ld)",
			    ((ModulusEncoder *)(void *)mc[0])->field_00 == a[0]
			    && ((ModulusEncoder *)(void *)mc[0])->field_18
			       == a[6], 1, trial);
		if (memcmp(before, mc[0], MC_SLOT) != 0)
			moved = 1;
	}

	diff_eq_int("the ctor changed the object", moved, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_v90jd();
	rc |= run_v92jd();
	rc |= run_v90mp();
	rc |= run_printbase2();
	rc |= run_ctor7("ModulusEncoder::ModulusEncoder(j*7) (C1)",
			our_enc7_c1, ref_enc7_c1);
	rc |= run_ctor7("ModulusEncoder::ModulusEncoder(j*7) (C2)",
			our_enc7_c2, ref_enc7_c2);
	rc |= run_ctor7("ModulusDecoder::ModulusDecoder(j*7) (C1)",
			our_dec7_c1, ref_dec7_c1);
	rc |= run_ctor7("ModulusDecoder::ModulusDecoder(j*7) (C2)",
			our_dec7_c2, ref_dec7_c2);

	return rc;
}
