/*
 * t_fdspkrnl.c -- differential tests for the `Fdspkrnl.c` span: the TONE_*
 * helpers, the float memset, and the echo-canceller kernel.
 *
 * The kernel objects are built BY HAND, field by field, because nothing
 * reconstructed allocates them.  Channel structs carry a heap pointer
 * (`coef`), so the comparison is field-by-field rather than one diff_eq_obj
 * over the struct.
 *
 * FDSP_Kernel_InitObj is reconstructed now, so an InitObj triggered mid-test
 * -- the saturation countdown expiring inside bValidateEnergyValue -- runs
 * OUR code on our side and the blob's on the reference's, and section 8
 * drives it directly on top of that.
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
#include "dsplib/sysdep.h"

extern unsigned int ref_dsplibs_debug_level;
extern int ref_bInternalBeepInProgress;

/*
 * EchoCanceler and bInternalBeepInProgress are FILE-LOCAL in the object, so
 * Fdspkrnl.c defines them `static` and fdspkrnl.h no longer declares them.
 * The test tier links a globalized copy of the reconstructed objects
 * (tools/testvisible.py), so these plain declarations resolve; the partial
 * link keeps the LOCAL binding.
 *
 * EchoCanceler takes ten arguments and has no taken address, so each
 * compiler gives it its static calling convention -- and the two compilers
 * this tree builds with DISAGREE on how many arguments that covers.  GCC
 * 3.4.2 caps it at regparm(2), which is the object's own; GCC 4 and later
 * use regparm(3).  Declaring the convention the building compiler actually
 * chose is what lets both tiers call their own copy correctly.
 */
#if __GNUC__ >= 4
extern void EchoCanceler(float *hist, int offset, float *coef,
			 unsigned int ntaps, float *in, float *out,
			 float *out2, int *verdict, float mu, int update)
	__attribute__((regparm(3)));
#else
extern void EchoCanceler(float *hist, int offset, float *coef,
			 unsigned int ntaps, float *in, float *out,
			 float *out2, int *verdict, float mu, int update)
	__attribute__((regparm(2)));
#endif
extern int bInternalBeepInProgress;

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
extern void ref_FDSP_Kernel_InitObj(struct fdsp_kernel *k);
extern void ref_FDSP_Kernel_SetInternalBeepInProgress(int on);
extern void ref_TONE_delete(struct fdsp_tone *t);
extern int ref_TONE_detect(struct fdsp_tone *t, float *buf, short n);

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
 * floats, InitObj's clearing size) and the buffer area k->buffers points at
 * (InitObj clears 0x271c bytes of it).
 */
struct kernel_side {
	struct fdsp_kernel k;
	struct fdsp_channel a, b;
	float coef_a[240], coef_b[240];
	struct fdsp_buffers shorts;
};

static void
build_side(struct kernel_side *s)
{
	unsigned int i;

	memset(s, 0, sizeof(*s));
	s->k.status = 2;
	s->k.saturation = 0;
	s->k.ntaps_a = 80;
	s->k.ntaps_b = 40;
	s->k.buffers = &s->shorts;
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
	dst->k.buffers = &dst->shorts;
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

	diff_eq_int("k.status %ld", got->k.status, want->k.status, tag);
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
	if (memcmp(&got->shorts, &want->shorts, sizeof(got->shorts)))
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

	/*
	 * SECTION 8 -- FDSP_Kernel_InitObj, called directly.
	 *
	 * Every region it clears starts NON-ZERO on both sides, so a loop
	 * that stopped short leaves the fill behind rather than a zero that
	 * was already there; the scalar defaults start wrong for the same
	 * reason.  compare_sides() then covers the whole object including
	 * the 0x271c-byte buffer area.
	 */
	diff_begin("FDSP_Kernel_InitObj direct");
	{
		static struct kernel_side ka, kb;
		unsigned char *pa, *pb;

		build_side(&ka);
		/* garbage everywhere InitObj is supposed to write */
		for (i = 0; i < 240; i++) {
			ka.coef_a[i] = smallf() * 7.0f + 1.0f;
			ka.coef_b[i] = smallf() * 7.0f + 2.0f;
		}
		for (i = 0; i < FDSP_DLY; i++) {
			ka.a.dly[i] = smallf() * 9.0f + 3.0f;
			ka.b.dly[i] = smallf() * 9.0f + 4.0f;
		}
		for (i = 0; i < 4; i++) {
			ka.a.energy[i] = (int)(lcg() & 0xffff) + 1;
			ka.b.energy[i] = (int)(lcg() & 0xffff) + 1;
		}
		ka.a.energy_idx = 3;
		ka.b.energy_idx = 2;
		ka.a.mu = 5.5f;
		ka.b.mu = 6.5f;
		ka.a.short_1690 = 0x1234;
		ka.b.short_1690 = 0x5678;
		ka.k.status = 99;
		ka.k.saturation = 77;
		ka.k.ntaps_a = 11;
		ka.k.ntaps_b = 22;
		pa = (unsigned char *)&ka.shorts;
		for (i = 0; i < sizeof(ka.shorts); i++)
			pa[i] = (unsigned char)(0xa5 + i);
		clone_side(&kb, &ka);
		pb = (unsigned char *)&kb.shorts;
		diff_eq_int("fill planted", memcmp(pa, pb,
			    sizeof(ka.shorts)), 0, 0);
		/* the fill must be non-zero, or the clearing test is vacuous */
		diff_eq_int("fill is not already zero", pa[0] != 0, 1, 0);

		ref_FDSP_Kernel_InitObj(&ka.k);
		FDSP_Kernel_InitObj(&kb.k);
		compare_sides(&kb, &ka, 200);
		/* and it really did clear: not a no-op on both sides */
		diff_eq_int("dly cleared", kb.a.dly[7] == 0.0f, 1, 0);
		diff_eq_int("buffer area cleared", pb[0] == 0, 1, 0);
		diff_eq_int("last buffer int cleared",
			    kb.shorts.int_2718 == 0, 1, 0);

		/* twice over, from the already-clean state */
		ref_FDSP_Kernel_InitObj(&ka.k);
		FDSP_Kernel_InitObj(&kb.k);
		compare_sides(&kb, &ka, 201);
	}
	failed |= diff_end();

	/*
	 * SECTION 9 -- FDSP_Kernel_SetInternalBeepInProgress.
	 *
	 * The flag is written whatever the level is; only the message is
	 * gated, and its "ON"/"OFF" arm follows the argument.  Both non-zero
	 * arguments and zero are driven, and the level is swept 0..3 because
	 * a gate at the wrong threshold is invisible at any single level
	 * (the argument in debug.h).
	 */
	diff_begin("FDSP_Kernel_SetInternalBeepInProgress");
	{
		static const int args[] = { 1, 0, 7, 0, -3 };
		unsigned int lvl, ai;

		for (lvl = 0; lvl <= 3; lvl++)
		for (ai = 0; ai < sizeof(args) / sizeof(args[0]); ai++) {
			long tag = (long)(lvl * 10 + ai);

			set_level(lvl);
			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			ref_FDSP_Kernel_SetInternalBeepInProgress(args[ai]);
			FDSP_Kernel_SetInternalBeepInProgress(args[ai]);
			dsplib_debug_capture_on = 0;
			diff_eq_int("beep flag %ld", bInternalBeepInProgress,
				    ref_bInternalBeepInProgress, tag);
			diff_eq_int("beep lines %ld",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), tag);
			diff_eq_int("beep text %ld",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)),
				    0, tag);
			/* the gate really is > 1, and the arm really moves */
			diff_eq_int("beep gated %ld",
				    (int)dsplib_debug_capture_lines(0),
				    lvl > 1 ? 1 : 0, tag);
			if (lvl > 1)
				diff_eq_int("beep arm %ld",
					    strstr(dsplib_debug_capture_text(0),
						   args[ai] ? "ON" : "OFF")
					    != NULL, 1, tag);
		}
		set_level(0);
		bInternalBeepInProgress = 0;
		ref_bInternalBeepInProgress = 0;
	}
	failed |= diff_end();

	/*
	 * SECTION 10 -- TONE_delete.
	 *
	 * There is nothing to compare but the allocator's books, so that is
	 * what is compared: each side gets its own five blocks, the counters
	 * are read after each run, and `bad_free` catches a pointer freed
	 * that was never handed out -- which is what a wrong field offset
	 * would produce.  Both arms of the `fir_len > 0` test are driven,
	 * and the zero arm must leave the four hanging blocks LIVE.
	 */
	diff_begin("TONE_delete");
	{
		static const short lens[] = { 4, 0, -1 };
		unsigned int li;

		for (li = 0; li < 3; li++) {
			struct alloc_log la, lb;
			struct fdsp_tone *ta, *tb;

			harness_alloc_reset();
			ta = (struct fdsp_tone *)sysdep_malloc(sizeof(*ta));
			memset(ta, 0, sizeof(*ta));
			ta->fir_len = lens[li];
			ta->fir_coef = (float *)sysdep_malloc(16);
			ta->fir_dly = (float *)sysdep_malloc(16);
			ta->ptr_01b4 = (float *)sysdep_malloc(16);
			ta->ptr_01b8 = (float *)sysdep_malloc(16);
			ref_TONE_delete(ta);
			la = harness_alloc;

			harness_alloc_reset();
			tb = (struct fdsp_tone *)sysdep_malloc(sizeof(*tb));
			memset(tb, 0, sizeof(*tb));
			tb->fir_len = lens[li];
			tb->fir_coef = (float *)sysdep_malloc(16);
			tb->fir_dly = (float *)sysdep_malloc(16);
			tb->ptr_01b4 = (float *)sysdep_malloc(16);
			tb->ptr_01b8 = (float *)sysdep_malloc(16);
			TONE_delete(tb);
			lb = harness_alloc;

			diff_eq_int("delete frees %ld", lb.frees, la.frees,
				    (long)li);
			diff_eq_int("delete live %ld", lb.live, la.live,
				    (long)li);
			diff_eq_int("delete bad_free %ld", lb.bad_free,
				    la.bad_free, (long)li);
			diff_eq_int("delete free_null %ld", lb.free_null,
				    la.free_null, (long)li);
			diff_eq_int("delete bytes %ld", (int)lb.bytes,
				    (int)la.bytes, (long)li);
			/* the arms are distinguishable, not both "5 frees" */
			diff_eq_int("delete arm %ld", lb.frees,
				    lens[li] > 0 ? 5 : 1, (long)li);
			diff_eq_int("delete nothing wild %ld", lb.bad_free, 0,
				    (long)li);
		}
		harness_alloc_reset();
	}
	failed |= diff_end();

	/*
	 * SECTION 11 -- TONE_detect.
	 *
	 * The FIR half is TONE_filter's, so the axes that matter here are
	 * the biquad and the two smoothed powers: the ring index is swept
	 * over every slot of three lengths, and the starting powers and
	 * thresholds are chosen so that all three verdicts appear -- below
	 * the floor (2), above it and inside the ratio (1), above it and
	 * outside (0).  A zero block length is driven too, because the tail
	 * still runs for it.
	 */
	diff_begin("TONE_detect");
	{
		static const short lens[] = { 1, 5, 16 };
		unsigned int li, idx, vi;
		int seen[3];

		seen[0] = seen[1] = seen[2] = 0;
		for (li = 0; li < 3; li++)
		for (idx = 0; idx <= (unsigned int)lens[li]; idx++)
		for (vi = 0; vi < 4; vi++) {
			struct fdsp_tone ta, tb;
			float coefa[16], dlya[16], dlyb[16];
			float ba[40], bb[40];
			short bn = (short)(vi == 3 ? 0 : 40);
			long tag = (long)((li * 100 + idx) * 10 + vi);
			int ra, rb;

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
			ta.det_coef[0] = smallf();
			ta.det_coef[1] = smallf();
			ta.det_coef[2] = smallf();
			ta.det_z1 = smallf() * 10.0f;
			ta.det_z2 = smallf() * 10.0f;
			/*
			 * vi picks the verdict arm: a floor above anything
			 * the block can reach (2), then a ratio that admits
			 * (1) and one that refuses (0).
			 */
			ta.float_0014 = vi == 0 ? 1.0e9f : 1.0e-6f;
			ta.float_000c = vi == 1 ? 1.0e6f : 1.0e-6f;
			ta.float_005c = 250.0f;
			ta.float_0060 = 700.0f;
			memcpy(&tb, &ta, sizeof(ta));
			tb.fir_dly = dlyb;   /* own delay line, same bits */

			for (i = 0; i < 40; i++)
				ba[i] = smallf() * 50.0f;
			memcpy(bb, ba, sizeof(ba));

			ra = ref_TONE_detect(&ta, ba, bn);
			rb = TONE_detect(&tb, bb, bn);
			diff_eq_int("detect ret %ld", rb, ra, tag);
			if (ra >= 0 && ra <= 2)
				seen[ra]++;
			diff_eq_int("detect fir_idx %ld", tb.fir_idx,
				    ta.fir_idx, tag);
			diff_eq_float("detect z1 %ld", tb.det_z1, ta.det_z1,
				      tag);
			diff_eq_float("detect z2 %ld", tb.det_z2, ta.det_z2,
				      tag);
			diff_eq_float("detect e_res %ld", tb.float_005c,
				      ta.float_005c, tag);
			diff_eq_float("detect e_tot %ld", tb.float_0060,
				      ta.float_0060, tag);
			for (i = 0; i < 16; i++)
				diff_eq_float("detect dly[%ld]", dlyb[i],
					      dlya[i], tag * 100 + i);
			/* the input block must not be written */
			for (i = 0; i < 40; i++)
				diff_eq_float("detect in[%ld]", bb[i], ba[i],
					      tag * 100 + i);
		}
		/*
		 * F134: a verdict arm nothing entered proves nothing, so the
		 * coverage is asserted FROM THE RUN.
		 */
		diff_eq_int("verdict 0 seen", seen[0] > 0, 1, 0);
		diff_eq_int("verdict 1 seen", seen[1] > 0, 1, 0);
		diff_eq_int("verdict 2 seen", seen[2] > 0, 1, 0);
	}
	failed |= diff_end();

	return failed;
}
