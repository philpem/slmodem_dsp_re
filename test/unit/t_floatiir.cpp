/*
 * t_floatiir -- FloatIIR against the blob, member by member.
 *
 * The concrete all-pole class, not the GenericIIR instantiation that shares
 * its translation unit and its file name; t_genericiir.cpp covers that one.
 *
 * `this` is an ordinary first stack argument in this object -- plain cdecl,
 * no thiscall -- so each member is declared here as a free function taking
 * `void *self`, with an asm() label binding it to the mangled `ref_` symbol.
 * The extern "C" is what stops the compiler mangling the declaration a second
 * time (finding F225); the asm label is what lets it have a readable name.
 *
 * WHY THE OBJECT IS NOT COMPARED WHOLE.  m_hist holds a heap pointer, and the
 * two sides get two different addresses from the allocator, so a byte compare
 * of the 20-byte object always differs at +4.  The four other fields are
 * compared directly and the buffer is compared through the pointers.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/FloatIIR.h"

extern "C" {
void ref_ctor(void *self, unsigned ncoeff, float *coeff, unsigned block)
	asm("ref__ZN8FloatIIRC1EjPfj");
void ref_dtor(void *self) asm("ref__ZN8FloatIIRD1Ev");
/*
 * The base-object variants.  For a class with no bases these do the same
 * work as C1/D1 -- verified byte-identical in the object, not assumed -- but
 * GCC emitted them as separate copies rather than aliases, so they are
 * separate symbols and go untested unless something calls them.
 */
void ref_ctor2(void *self, unsigned ncoeff, float *coeff, unsigned block)
	asm("ref__ZN8FloatIIRC2EjPfj");
void ref_dtor2(void *self) asm("ref__ZN8FloatIIRD2Ev");
void ref_reset(void *self) asm("ref__ZN8FloatIIR5resetEv");
int ref_setcoef(void *self, float *coeff, unsigned ncoeff)
	asm("ref__ZN8FloatIIR15setCoefficientsEPfj");
void ref_process(void *self, const float *in, float *out, unsigned n)
	asm("ref__ZN8FloatIIR7processEPKfPfj");
}

/*
 * The blob's object, byte for byte, so its fields can be read without
 * declaring a second copy of the class and hoping the two agree.
 */
struct ref_layout {
	float *coeff;
	float *hist;
	unsigned ncoeff;
	unsigned len;
	int pos;
};

static float coef_a[16] = {
	0.5f, -0.25f, 0.125f, -0.0625f, 0.03125f, -0.015625f,
	0.0078125f, -0.00390625f, 0.5f, 0.25f, -0.75f, 0.125f,
	-0.5f, 0.375f, -0.125f, 0.0625f
};
static float coef_b[16] = {
	-0.1f, 0.2f, -0.3f, 0.4f, -0.05f, 0.15f, -0.25f, 0.35f,
	0.11f, -0.22f, 0.33f, -0.44f, 0.55f, -0.66f, 0.77f, -0.88f
};

/*
 * Fields, and the history buffer through the two pointers.  `tag` identifies
 * the call site; every check carries it, because a differential failure that
 * cannot say which case produced it costs a re-derivation (finding F220).
 */
static void
compare(const char *what, FloatIIR *a, const ref_layout *b, long tag)
{
	const ref_layout *la = (const ref_layout *)a;

	diff_eq_int("%s: ncoeff", la->ncoeff, b->ncoeff, tag);
	diff_eq_int("%s: len", la->len, b->len, tag);
	diff_eq_int("%s: pos", la->pos, b->pos, tag);
	diff_eq_int("%s: coeff points at the same array",
		    la->coeff == b->coeff, 1, tag);
	(void)what;

	diff_eq_int("both allocated", (la->hist != 0) == (b->hist != 0), 1, tag);
	if (la->hist == 0 || b->hist == 0)
		return;
	/*
	 * As raw bits, not as floats: a NaN would compare unequal to itself
	 * and an untouched slot holds the harness fill, which is not a number.
	 */
	for (unsigned i = 0; i < b->len; i++)
		diff_eq_int("history word",
			    ((const unsigned *)la->hist)[i],
			    ((const unsigned *)b->hist)[i],
			    tag * 1000 + (long)i);
}

/* One construct/run/destroy cycle, compared at every step. */
static void
run(unsigned ncoeff, unsigned block, float *coef, long tag)
{
	static unsigned char refobj[64];
	FloatIIR ours(ncoeff, coef, block);
	ref_ctor(refobj, ncoeff, coef, block);
	ref_layout *r = (ref_layout *)refobj;

	compare("after construct", &ours, r, tag);

	/*
	 * Enough samples to drive m_pos below zero several times, so the
	 * history compaction runs and is compared rather than assumed: block
	 * is the number of samples between compactions.
	 */
	static float in[512], outa[512], outb[512];
	for (int i = 0; i < 512; i++)
		in[i] = (float)((i * 37 % 101) - 50) / 64.0f;

	unsigned done = 0;
	int step = 0;
	while (done < 512) {
		unsigned n = (unsigned)(1 + (step % 7));
		if (done + n > 512)
			n = 512 - done;
		ours.process(in + done, outa + done, n);
		ref_process(refobj, in + done, outb + done, n);
		compare("after process", &ours, r, tag * 100 + step);
		done += n;
		step++;
	}
	for (int i = 0; i < 512; i++)
		diff_eq_int("output word", ((unsigned *)outa)[i],
			    ((unsigned *)outb)[i], tag * 10000 + i);

	ours.reset();
	ref_reset(refobj);
	compare("after reset", &ours, r, tag + 500000);

	/* Destructors free the two buffers; the allocator log proves it. */
	ref_dtor(refobj);
}

int
main(void)
{
	int rc = 0;

	diff_begin("FloatIIR: construct, run and reset");
	harness_alloc_reset();
	/*
	 * ncoeff is rounded down to a multiple of four, so 6 must behave as
	 * 4 and 3 as 0.  The zero-tap case is NOT driven here: with no taps
	 * the compaction loop's counter starts at -1 and runs 2^32 times.
	 * That is the original's behaviour and it is recorded as D57, not
	 * something to discover by hanging the suite.
	 */
	run(4, 8, coef_a, 1);
	run(8, 16, coef_a, 2);
	run(6, 8, coef_a, 3);		/* rounds to 4 */
	run(16, 64, coef_b, 4);
	run(4, 1, coef_a, 5);		/* compaction on every sample */
	run(12, 3, coef_b, 6);
	rc |= diff_end();

	/*
	 * A STIFF, LONG RUN -- which does NOT distinguish the accumulator
	 * split, and is kept for what it does cover.
	 *
	 * This case was written to catch a single-accumulator build of the dot
	 * product, on the reasoning that a different association order must
	 * give a different answer.  It does not catch it, and neither do the
	 * six cases above: that mutant passes all 289,028 checks in this file.
	 * Sixteen products of similar magnitude sum inside the x87 significand
	 * without rounding, so both orders are exact.  See the note in
	 * FloatIIR::process and D58.
	 *
	 * Kept because 262,144 samples through poles near the unit circle,
	 * with the history compacting every 64 samples and inputs spanning two
	 * decades, is the strongest exercise of `process` in this file even
	 * though it is not the one it was written to be.
	 */
	diff_begin("FloatIIR: a long run with marginally stable poles");
	{
		static float stiff[16];
		for (int i = 0; i < 16; i++)
			stiff[i] = (i % 2 ? -1.0f : 1.0f) *
				   (0.9f / 16.0f) * (1.0f + i * 0.013f);

		static unsigned char refobj[64];
		FloatIIR ours(16, stiff, 64);
		ref_ctor(refobj, 16, stiff, 64);

		static float in[4096], outa[4096], outb[4096];
		for (int block = 0; block < 64; block++) {
			for (int i = 0; i < 4096; i++) {
				int t = block * 4096 + i;
				in[i] = (float)((t * 2654435761u) >> 8 &
						0xffff) / 8192.0f - 4.0f;
				in[i] *= (float)(1 + (t % 17));
			}
			ours.process(in, outa, 4096);
			ref_process(refobj, in, outb, 4096);
			for (int i = 0; i < 4096; i++)
				diff_eq_int("stiff output word",
					    ((unsigned *)outa)[i],
					    ((unsigned *)outb)[i],
					    (long)block * 4096 + i);
		}
		compare("after the stiff run", &ours,
			(ref_layout *)refobj, 30);
		ref_dtor(refobj);
	}
	rc |= diff_end();

	diff_begin("FloatIIR: setCoefficients");
	{
		static unsigned char refobj[64];
		FloatIIR ours(8, coef_a, 16);
		ref_ctor(refobj, 8, coef_a, 16);
		ref_layout *r = (ref_layout *)refobj;

		/* Same tap count: pointer swaps, geometry untouched. */
		diff_eq_int("same count returns", ours.setCoefficients(coef_b, 8),
			    ref_setcoef(refobj, coef_b, 8), 1);
		compare("same count", &ours, r, 10);

		/* Grow, which clamps the position. */
		diff_eq_int("grow returns", ours.setCoefficients(coef_a, 20),
			    ref_setcoef(refobj, coef_a, 20), 2);
		compare("grow", &ours, r, 11);

		/* Refused: the rounded count is not below m_len. */
		diff_eq_int("too many returns", ours.setCoefficients(coef_b, 64),
			    ref_setcoef(refobj, coef_b, 64), 3);
		compare("refused leaves everything alone", &ours, r, 12);

		/* Shrink again, which must not move a position already low. */
		diff_eq_int("shrink returns", ours.setCoefficients(coef_b, 4),
			    ref_setcoef(refobj, coef_b, 4), 4);
		compare("shrink", &ours, r, 13);

		static float in[64], outa[64], outb[64];
		for (int i = 0; i < 64; i++)
			in[i] = (float)(i % 13) / 8.0f - 0.75f;
		ours.process(in, outa, 64);
		ref_process(refobj, in, outb, 64);
		for (int i = 0; i < 64; i++)
			diff_eq_int("output after reconfigure",
				    ((unsigned *)outa)[i],
				    ((unsigned *)outb)[i], 100 + i);
		compare("after reconfigured run", &ours, r, 14);
		ref_dtor(refobj);
	}
	rc |= diff_end();

	diff_begin("FloatIIR: the base-object constructor and destructor");
	{
		/*
		 * C2/D2 are byte-identical to C1/D1 here, so this proves
		 * nothing new about the behaviour -- it drives the symbols, so
		 * that "identical" is a statement this suite has checked
		 * rather than one it inherited from a reading of the object.
		 */
		static unsigned char via1[64], via2[64];
		ref_ctor(via1, 8, coef_a, 16);
		ref_ctor2(via2, 8, coef_a, 16);
		ref_layout *r1 = (ref_layout *)via1, *r2 = (ref_layout *)via2;
		diff_eq_int("C2 ncoeff", r2->ncoeff, r1->ncoeff, 0);
		diff_eq_int("C2 len", r2->len, r1->len, 0);
		diff_eq_int("C2 pos", r2->pos, r1->pos, 0);
		diff_eq_int("C2 coeff", r2->coeff == r1->coeff, 1, 0);
		for (unsigned i = 0; i < r1->len; i++)
			diff_eq_int("C2 history word",
				    ((unsigned *)r2->hist)[i],
				    ((unsigned *)r1->hist)[i], (long)i);
		ref_dtor2(via2);
		ref_dtor(via1);
	}
	rc |= diff_end();

	diff_begin("FloatIIR: zero-length process is a no-op");
	{
		static unsigned char refobj[64];
		FloatIIR ours(4, coef_a, 8);
		ref_ctor(refobj, 4, coef_a, 8);
		float in = 1.0f, out = 2.0f;
		ours.process(&in, &out, 0);
		ref_process(refobj, &in, &out, 0);
		compare("count zero", &ours, (ref_layout *)refobj, 20);
		diff_eq_int("output untouched", ((unsigned *)&out)[0],
			    ((unsigned *)&out)[0], 0);
		ref_dtor(refobj);
	}
	rc |= diff_end();

	return rc;
}
