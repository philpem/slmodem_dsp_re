/*
 * t_fdspkrnl.c -- differential tests for the `Fdspkrnl.c` span: the TONE_*
 * helpers, the float memset, and the echo-canceller kernel.
 *
 * The kernel objects are built BY HAND, field by field, because nothing
 * reconstructed allocates them; FDSP_Kernel_InitObj is still the blob's and
 * both sides call that same code on their own objects, so an InitObj
 * triggered mid-test (the saturation countdown expiring) initialises both
 * sides identically.  Channel structs carry a heap pointer (`coef`), so the
 * comparison is field-by-field rather than one diff_eq_obj over the struct.
 *
 * EchoCanceler and bValidateEnergyValue are LOCAL in the blob: their ref_
 * aliases take GCC 3.4's regparm(2) static convention and are declared so
 * here; our copies are external with the ordinary convention.
 *
 * MTK_phasor is also still the blob's, so TONE_generate's oscillator is the
 * same code on both sides and the comparison is about everything AROUND it:
 * the state handed in, the scaling, the millisecond clock and the phase
 * inversion at expiry.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/beepgen.h"
#include "dsplib/fdspkrnl.h"

extern unsigned int ref_dsplibs_debug_level;
extern int ref_bInternalBeepInProgress;

/* Per-side debug transcripts; see test/harness/runtime.c. */
extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
unsigned dsplib_debug_capture_lines(int side);
const char *dsplib_debug_capture_text(int side);

extern void ref_zFLTUTL_FloatMemSet(float v, float *buf, unsigned int n);
extern void ref_TONE_generate(struct fdsp_tone *t, float *buf, short n);
extern void ref_TONE_filter(struct fdsp_tone *t, float *buf, short n);
extern void ref_TONE_kill(struct fdsp_tone *t, float *buf, short n);
extern void ref_EchoCanceler(float *hist, int offset, float *coef,
			     unsigned int ntaps, float *in, float *out,
			     float *out2, int *verdict, float mu, int update)
	__attribute__((regparm(2)));
extern int ref_bValidateEnergyValue(float *buf, unsigned int n, int *hist,
				    unsigned int *idxp, unsigned int histlen,
				    struct fdsp_kernel *k)
	__attribute__((regparm(2)));
extern int ref_FDSP_Kernel_Loop(struct fdsp_kernel *k, float *in_a,
				float *out_b, float *in_b, float *out_a);

static unsigned int lcg_state = 0xbeef101u;
static unsigned int
lcg(void)
{
	lcg_state = lcg_state * 1664525u + 1013904223u;
	return lcg_state;
}

/* small deterministic float, roughly -1..1 with a fractional tail */
static float
smallf(void)
{
	return ((int)(lcg() % 20001) - 10000) * 0.0001f;
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/*
 * One side's kernel: the struct, two channels, their coef arrays (240
 * floats, InitObj's clearing size) and the short area k->ptr_10 points at
 * (InitObj clears 0x271c bytes of it).
 */
struct kernel_side {
	struct fdsp_kernel k;
	struct fdsp_channel a, b;
	float coef_a[240], coef_b[240];
	unsigned char shorts[0x2720];
};

static void
build_side(struct kernel_side *s)
{
	unsigned int i;

	memset(s, 0, sizeof(*s));
	s->k.int_00 = 2;
	s->k.saturation = 0;
	s->k.ntaps_a = 80;
	s->k.ntaps_b = 40;
	s->k.ptr_10 = s->shorts;
	s->k.chan_a = &s->a;
	s->k.chan_b = &s->b;
	s->a.coef = s->coef_a;
	s->b.coef = s->coef_b;
	s->a.mu = 0.032f;
	s->b.mu = 0.032f;
	s->a.offset = 3;
	s->b.offset = 5;
	for (i = 0; i < FDSP_DLY; i++) {
		s->a.dly[i] = smallf();
		s->b.dly[i] = s->a.dly[i] * 0.5f;
	}
	/* the second side must copy, not re-roll -- see clone_side() */
}

/* b becomes a bit-identical copy of a, with its OWN pointers re-aimed. */
static void
clone_side(struct kernel_side *dst, const struct kernel_side *src)
{
	memcpy(dst, src, sizeof(*src));
	dst->k.ptr_10 = dst->shorts;
	dst->k.chan_a = &dst->a;
	dst->k.chan_b = &dst->b;
	dst->a.coef = dst->coef_a;
	dst->b.coef = dst->coef_b;
}

/* Everything in one channel except the coef POINTER, whose target array is
 * compared by the caller. */
static void
compare_channel(const char *who, struct fdsp_channel *got,
		struct fdsp_channel *want, long tag)
{
	unsigned int i;

	for (i = 0; i < FDSP_DLY; i++)
		diff_eq_float("dly[%ld]", got->dly[i], want->dly[i],
			      tag * 10000 + i);
	for (i = 0; i < FDSP_BLOCK; i++)
		diff_eq_float("cross[%ld]", got->cross[i], want->cross[i],
			      tag * 10000 + i);
	diff_eq_int("verdict %ld", got->verdict, want->verdict, tag);
	diff_eq_int("offset %ld", got->offset, want->offset, tag);
	diff_eq_float("mu %ld", got->mu, want->mu, tag);
	for (i = 0; i < 4; i++)
		diff_eq_int("energy[%ld]", got->energy[i], want->energy[i],
			    tag * 10 + i);
	diff_eq_int("energy_idx %ld", got->energy_idx, want->energy_idx,
		    tag);
	(void)who;
}

static void
compare_sides(struct kernel_side *got, struct kernel_side *want, long tag)
{
	unsigned int i;

	diff_eq_int("k.int_00 %ld", got->k.int_00, want->k.int_00, tag);
	diff_eq_int("k.saturation %ld", got->k.saturation,
		    want->k.saturation, tag);
	diff_eq_int("k.ntaps_a %ld", got->k.ntaps_a, want->k.ntaps_a, tag);
	diff_eq_int("k.ntaps_b %ld", got->k.ntaps_b, want->k.ntaps_b, tag);
	compare_channel("a", &got->a, &want->a, tag * 2);
	compare_channel("b", &got->b, &want->b, tag * 2 + 1);
	for (i = 0; i < 240; i++) {
		diff_eq_float("coef_a[%ld]", got->coef_a[i], want->coef_a[i],
			      tag * 1000 + i);
		diff_eq_float("coef_b[%ld]", got->coef_b[i], want->coef_b[i],
			      tag * 1000 + i);
	}
	if (memcmp(got->shorts, want->shorts, sizeof(got->shorts)))
		diff_eq_int("shorts area differs", 1, 0, tag);
}

int
main(void)
{
	int failed = 0;
	unsigned int i, n, v;

	set_level(0);

	/* SECTION 1 -- zFLTUTL_FloatMemSet across sizes including 0. */
	diff_begin("zFLTUTL_FloatMemSet");
	for (n = 0; n <= 33; n += 3) {
		float ba[40], bb[40];
		float val = smallf() * 1000.0f;

		memset(ba, 0x5c, sizeof(ba));
		memcpy(bb, ba, sizeof(ba));
		ref_zFLTUTL_FloatMemSet(val, ba, n);
		zFLTUTL_FloatMemSet(val, bb, n);
		for (i = 0; i < 40; i++)
			diff_eq_float("set[%ld]", bb[i], ba[i],
				      (long)(n * 100 + i));
	}
	failed |= diff_end();

	/*
	 * SECTION 2 -- TONE_generate.  The four timer arms: still running,
	 * endless (duration <= 0), expired with the phase wrap, expired
	 * without it.  n includes 0 (no samples, timer only).
	 */
	diff_begin("TONE_generate");
	{
		static const struct {
			float dur, elapsed, phase, step;
			short n;
		} cases[] = {
			{ 1000.0f, 0.0f, 0.1f, 0.05f, 160 },  /* running   */
			{ 0.0f, 5.0f, 1.0f, 0.2f, 8 },        /* endless   */
			{ -3.0f, 90.0f, 2.0f, 0.7f, 16 },     /* endless   */
			{ 10.0f, 9.0f, 0.3f, 0.11f, 160 },    /* expires   */
			{ 10.0f, 9.9f, 3.5f, 0.3f, 8 },       /* wraps     */
			{ 2.0f, 1.9f, 0.0f, 0.0f, 1 },        /* edge      */
			{ 1.0f, 0.5f, 0.2f, 0.1f, 0 },        /* n == 0    */
			{ 0.125f, 0.0f, 6.2f, 0.9f, 1 },      /* boundary  */
		};
		unsigned int c;

		for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
			struct fdsp_tone ta, tb;
			float ba[176], bb[176];

			memset(&ta, 0, sizeof(ta));
			ta.amp = 0.25f;
			ta.duration = cases[c].dur;
			ta.elapsed = cases[c].elapsed;
			ta.phase = cases[c].phase;
			ta.step = cases[c].step;
			memcpy(&tb, &ta, sizeof(ta));
			memset(ba, 0x2d, sizeof(ba));
			memcpy(bb, ba, sizeof(ba));

			ref_TONE_generate(&ta, ba, cases[c].n);
			TONE_generate(&tb, bb, cases[c].n);
			for (i = 0; i < 176; i++)
				diff_eq_float("gen[%ld]", bb[i], ba[i],
					      (long)(c * 1000 + i));
			diff_eq_obj("tone after generate", struct fdsp_tone,
				    &tb, &ta, c);
		}
	}
	failed |= diff_end();

	/*
	 * SECTION 3 -- TONE_filter: ring lengths 1, 4, 16; every starting
	 * index including the wrap; block lengths incl. 0.
	 */
	diff_begin("TONE_filter");
	{
		static const short lens[] = { 1, 4, 16 };
		unsigned int li, idx;

		for (li = 0; li < 3; li++)
		for (idx = 0; idx <= (unsigned int)lens[li]; idx++) {
			struct fdsp_tone ta, tb;
			float coefa[16], dlya[16], dlyb[16];
			float ba[48], bb[48];
			short bn = (short)(idx % 3 == 0 ? 0 : 48);

			for (i = 0; i < 16; i++) {
				coefa[i] = smallf();
				dlya[i] = smallf() * 100.0f;
			}
			memcpy(dlyb, dlya, sizeof(dlya));
			memset(&ta, 0, sizeof(ta));
			ta.fir_len = lens[li];
			ta.fir_idx = (short)idx;
			ta.fir_coef = coefa;
			ta.fir_dly = dlya;
			memcpy(&tb, &ta, sizeof(ta));
			tb.fir_dly = dlyb;   /* own delay line, same bits */

			for (i = 0; i < 48; i++)
				ba[i] = smallf() * 50.0f;
			memcpy(bb, ba, sizeof(ba));
			ref_TONE_filter(&ta, ba, bn);
			TONE_filter(&tb, bb, bn);
			for (i = 0; i < 48; i++)
				diff_eq_float("fir[%ld]", bb[i], ba[i],
					      (long)(li * 1000 + idx * 100
						     + i));
			diff_eq_int("fir_idx %ld", tb.fir_idx, ta.fir_idx,
				    (long)(li * 100 + idx));
			for (i = 0; i < 16; i++)
				diff_eq_float("fir dly[%ld]", dlyb[i],
					      dlya[i],
					      (long)(li * 1000 + idx * 100
						     + i));
		}
	}
	failed |= diff_end();

	/* SECTION 4 -- TONE_kill: biquad state and in-place block. */
	diff_begin("TONE_kill");
	for (v = 0; v < 6; v++) {
		struct fdsp_tone ta, tb;
		float coefa[3];
		float ba[64], bb[64];
		short bn = (short)(v == 0 ? 0 : v * 12);

		for (i = 0; i < 3; i++)
			coefa[i] = smallf();
		memset(&ta, 0, sizeof(ta));
		ta.iir_coef = coefa;
		ta.iir_z1 = smallf() * 10.0f;
		ta.iir_z2 = smallf() * 10.0f;
		memcpy(&tb, &ta, sizeof(ta));
		for (i = 0; i < 64; i++)
			ba[i] = smallf() * 100.0f;
		memcpy(bb, ba, sizeof(ba));
		ref_TONE_kill(&ta, ba, bn);
		TONE_kill(&tb, bb, bn);
		for (i = 0; i < 64; i++)
			diff_eq_float("kill[%ld]", bb[i], ba[i],
				      (long)(v * 100 + i));
		diff_eq_float("z1 %ld", tb.iir_z1, ta.iir_z1, v);
		diff_eq_float("z2 %ld", tb.iir_z2, ta.iir_z2, v);
	}
	failed |= diff_end();

	/*
	 * SECTION 5 -- EchoCanceler, called directly.  mu = 0 (no model, no
	 * adaptation), mu != 0 with update off and on; a near signal quiet
	 * enough to adapt and loud enough not to, so both sides of the
	 * half-peak test and both verdicts appear.
	 */
	diff_begin("EchoCanceler direct");
	{
		static float hist[FDSP_DLY];
		static const float mus[] = { 0.0f, 0.032f };
		unsigned int mi, up, loud;

		for (i = 0; i < FDSP_DLY; i++)
			hist[i] = smallf() * 1000.0f;
		for (mi = 0; mi < 2; mi++)
		for (up = 0; up < 2; up++)
		for (loud = 0; loud < 2; loud++) {
			float coefa[80], coefb[80];
			float in[FDSP_BLOCK];
			float outa[FDSP_BLOCK], outb[FDSP_BLOCK];
			float o2a[FDSP_BLOCK], o2b[FDSP_BLOCK];
			int va = -5, vb = -6;
			long tag = (long)(mi * 100 + up * 10 + loud);

			for (i = 0; i < 80; i++)
				coefa[i] = smallf() * 0.1f;
			memcpy(coefb, coefa, sizeof(coefa));
			for (i = 0; i < FDSP_BLOCK; i++)
				in[i] = smallf() * (loud ? 2000.0f : 1.0f);
			memset(outa, 0x3e, sizeof(outa));
			memcpy(outb, outa, sizeof(outa));
			memset(o2a, 0x3f, sizeof(o2a));
			memcpy(o2b, o2a, sizeof(o2a));

			ref_EchoCanceler(hist, 7, coefa, 80, in, outa, o2a,
					 &va, mus[mi], (int)up);
			EchoCanceler(hist, 7, coefb, 80, in, outb, o2b,
				     &vb, mus[mi], (int)up);
			diff_eq_int("verdict %ld", vb, va, tag);
			for (i = 0; i < FDSP_BLOCK; i++) {
				diff_eq_float("out[%ld]", outb[i], outa[i],
					      tag * 1000 + i);
				diff_eq_float("out2[%ld]", o2b[i], o2a[i],
					      tag * 1000 + i);
			}
			for (i = 0; i < 80; i++)
				diff_eq_float("coef[%ld]", coefb[i],
					      coefa[i], tag * 1000 + i);
		}
	}
	failed |= diff_end();

	/*
	 * SECTION 6 -- bValidateEnergyValue, called directly on hand-built
	 * kernels.  The beep suppressor, the quiet arm, both loud arms, and
	 * the countdown all the way to the delayed InitObj -- which is the
	 * blob's code on both sides' objects, so the comparison afterwards
	 * checks the whole re-initialised kernel.
	 */
	diff_begin("bValidateEnergyValue direct");
	{
		static struct kernel_side ka, kb;
		float quiet[FDSP_BLOCK], loud[FDSP_BLOCK];
		int hista[4], histb[4];
		unsigned int idxa, idxb;
		int ra, rb, round;

		/*
		 * The gate compares the ring AVERAGE of variance * 32000
		 * against 2200: +/-0.3 sits well under it (~900), +/-30
		 * well over (~9.6e6) without overflowing the int cast.
		 */
		for (i = 0; i < FDSP_BLOCK; i++) {
			quiet[i] = smallf() * 0.3f;
			loud[i] = smallf() * 30.0f;
		}
		build_side(&ka);
		clone_side(&kb, &ka);

		/* beep in progress: 0 out, nothing touched */
		memset(hista, 0, sizeof(hista));
		memset(histb, 0, sizeof(histb));
		idxa = idxb = 0;
		bInternalBeepInProgress = 1;
		ref_bInternalBeepInProgress = 1;
		ra = ref_bValidateEnergyValue(loud, FDSP_BLOCK, hista, &idxa,
					      4, &ka.k);
		rb = bValidateEnergyValue(loud, FDSP_BLOCK, histb, &idxb,
					  4, &kb.k);
		diff_eq_int("beep ret", rb, ra, 0);
		bInternalBeepInProgress = 0;
		ref_bInternalBeepInProgress = 0;

		/* quiet, no countdown: the licensed arm */
		ra = ref_bValidateEnergyValue(quiet, FDSP_BLOCK, hista,
					      &idxa, 4, &ka.k);
		rb = bValidateEnergyValue(quiet, FDSP_BLOCK, histb, &idxb,
					  4, &kb.k);
		diff_eq_int("quiet ret", rb, ra, 1);
		diff_eq_int("quiet idx", idxb, idxa, 1);
		for (i = 0; i < 4; i++)
			diff_eq_int("hist[%ld]", histb[i], hista[i], (long)i);

		/*
		 * loud until the countdown starts, then quiet until it
		 * expires: 16 rounds is enough for saturation = 1280/160*2
		 * = 16 to reach the InitObj arm.  Debug level 2 for one
		 * round so both printf gates run, with the transcripts
		 * captured -- the strstr assertions afterwards are the
		 * proof the two saturation arms actually FIRED, not just
		 * that nothing differed.
		 */
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		for (round = 0; round < 24; round++) {
			float *blk = round < 6 ? loud : quiet;

			/*
			 * Level 2 exactly where the two printf gates sit.
			 * Round 0 starts the countdown at 1280/160*2 = 16
			 * ("Identify High energy").  Rounds 1..5 are loud
			 * decrements; the ring average stays above 2200
			 * until three quiet blocks displace the loud ones,
			 * so rounds 6..8 also decrement on the loud arm,
			 * 9..15 on the quiet arm, and round 16 reaches
			 * zero and re-initialises ("Delayed ...").
			 */
			set_level(round == 0 || round == 16 ? 2 : 0);
			ra = ref_bValidateEnergyValue(blk, FDSP_BLOCK, hista,
						      &idxa, 4, &ka.k);
			rb = bValidateEnergyValue(blk, FDSP_BLOCK, histb,
						  &idxb, 4, &kb.k);
			diff_eq_int("round %ld ret", rb, ra, (long)round);
			diff_eq_int("round %ld sat", kb.k.saturation,
				    ka.k.saturation, (long)round);
			diff_eq_int("round %ld idx", idxb, idxa,
				    (long)round);
			for (i = 0; i < 4; i++)
				diff_eq_int("round hist[%ld]", histb[i],
					    hista[i],
					    (long)(round * 10 + i));
		}
		set_level(0);
		dsplib_debug_capture_on = 0;
		diff_eq_int("saturation transcript lines ours",
			    dsplib_debug_capture_lines(0), 2, 0);
		diff_eq_int("saturation transcript lines ref",
			    dsplib_debug_capture_lines(1), 2, 0);
		diff_eq_int("saturation transcript text equal",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)), 0, 0);
		diff_eq_int("'Identify High energy' fired",
			    strstr(dsplib_debug_capture_text(0),
				   "Identify High energy") != NULL, 1, 0);
		diff_eq_int("'Delayed' InitObj arm fired",
			    strstr(dsplib_debug_capture_text(0),
				   "Delayed FDSP_Kernel_InitObj") != NULL,
			    1, 0);
		compare_sides(&kb, &ka, 60);
	}
	failed |= diff_end();

	/*
	 * SECTION 7 -- FDSP_Kernel_Loop end to end: several blocks of
	 * far/near traffic through both directions, everything compared
	 * after every block.  Inputs are quiet enough that the energy gate
	 * licenses adaptation (the saturation arm was exercised above).
	 */
	diff_begin("FDSP_Kernel_Loop blocks");
	{
		static struct kernel_side ka, kb;
		float ina[FDSP_BLOCK], inb[FDSP_BLOCK];
		float outa_a[FDSP_BLOCK], outa_b[FDSP_BLOCK];
		float outb_a[FDSP_BLOCK], outb_b[FDSP_BLOCK];
		int block, ra, rb;

		build_side(&ka);
		clone_side(&kb, &ka);
		for (i = 0; i < 240; i++) {
			ka.coef_a[i] = smallf() * 0.05f;
			ka.coef_b[i] = smallf() * 0.05f;
			kb.coef_a[i] = ka.coef_a[i];
			kb.coef_b[i] = ka.coef_b[i];
		}

		for (block = 0; block < 6; block++) {
			/* quiet enough that the energy gate licenses
			 * adaptation -- the saturation arm ran above */
			for (i = 0; i < FDSP_BLOCK; i++) {
				ina[i] = smallf() * 0.2f;
				inb[i] = smallf() * 0.2f;
			}
			memset(outa_a, 0x41, sizeof(outa_a));
			memcpy(outa_b, outa_a, sizeof(outa_a));
			memset(outb_a, 0x42, sizeof(outb_a));
			memcpy(outb_b, outb_a, sizeof(outb_a));

			ra = ref_FDSP_Kernel_Loop(&ka.k, ina, outb_a, inb,
						  outa_a);
			rb = FDSP_Kernel_Loop(&kb.k, ina, outb_b, inb,
					      outa_b);
			diff_eq_int("loop ret %ld", rb, ra, (long)block);
			for (i = 0; i < FDSP_BLOCK; i++) {
				diff_eq_float("out_a[%ld]", outa_b[i],
					      outa_a[i],
					      (long)(block * 1000 + i));
				diff_eq_float("out_b[%ld]", outb_b[i],
					      outb_a[i],
					      (long)(block * 1000 + i));
			}
			compare_sides(&kb, &ka, 100 + block);
		}
	}
	failed |= diff_end();

	return failed;
}
