/*
 * t_agc -- Agc<float> against the blob.
 *
 * EVERY FLOAT REACHES THE OBJECT AS A 32-BIT WORD, never through a `float`
 * lvalue.  The fields are poked with `memcpy` and the input buffers are filled
 * the same way, because an x87 load-store quietens a signalling NaN and this
 * test drives signalling NaNs on purpose -- into `alpha`, whose comparison
 * against 1.0 is the branch that decides whether the AGC adapts at all
 * (finding F600).  None of the four entry points takes a float by value, so the
 * bits-taking declarations t_sinewave needs are not required here.
 *
 * The object is placement-built into a POISONED oversized slot and all 32
 * bytes are compared after every call, along with the whole output buffer and
 * the input buffer -- `process` is called in place in some cases, and with
 * `out` pointing INTO the object in others, so nothing may be assumed
 * untouched.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/Agc.h"

extern "C" {
void ref_agc_ctor(void *self)		asm("ref__ZN3AgcIfEC1Ev");
void ref_agc_reset(void *self)		asm("ref__ZN3AgcIfE5resetEv");
void ref_agc_freeze(void *self)		asm("ref__ZN3AgcIfE6freezeEv");
void ref_agc_process(void *self, const float *in, float *out, unsigned n)
					asm("ref__ZN3AgcIfE7processEPKfPfj");

void our_agc_ctor(void *self)		asm("_ZN3AgcIfEC1Ev");
void our_agc_reset(void *self)		asm("_ZN3AgcIfE5resetEv");
void our_agc_freeze(void *self)		asm("_ZN3AgcIfE6freezeEv");
void our_agc_process(void *self, const float *in, float *out, unsigned n)
					asm("_ZN3AgcIfE7processEPKfPfj");
}

#define SLOT	96		/* 32 for the object, the rest poisoned */
#define NBUF	72

static unsigned char oa[SLOT], ob[SLOT];
static float ina[NBUF], inb[NBUF], outa[NBUF], outb[NBUF];

static void poke(void *obj, unsigned off, unsigned bits)
{
	memcpy((unsigned char *)obj + off, &bits, 4);
}

/* The whole slot and both buffers, as words. */
static void cmp_all(long tag)
{
	unsigned i;

	for (i = 0; i < SLOT; i += 4) {
		unsigned x, y;

		memcpy(&x, oa + i, 4);
		memcpy(&y, ob + i, 4);
		diff_eq_int("object word", (long)x, (long)y, tag * 100 + i);
	}
	for (i = 0; i < NBUF; i++) {
		unsigned x, y;

		memcpy(&x, &outa[i], 4);
		memcpy(&y, &outb[i], 4);
		diff_eq_int("out", (long)x, (long)y, tag * 100 + i);
		memcpy(&x, &ina[i], 4);
		memcpy(&y, &inb[i], 4);
		diff_eq_int("in untouched", (long)x, (long)y, tag * 100 + i);
	}
}

static void fill(unsigned pattern, unsigned seedbits)
{
	unsigned i;

	for (i = 0; i < NBUF; i++) {
		unsigned b;

		switch (pattern) {
		case 0: b = 0x3f800000u; break;			/* 1.0        */
		case 1: b = 0x00000000u; break;			/* silence    */
		case 2: b = 0x3f800000u + i * 0x00010000u; break;
		case 3: b = (i & 1) ? 0xbf800000u : 0x3f800000u; break;
		case 4: b = 0x00000001u; break;			/* denormal   */
		case 5: b = 0x7f7fffffu; break;			/* huge       */
		case 6: b = (i & 3) ? 0x3f000000u : 0x7fc00000u; break; /* NaN */
		default: b = seedbits + i * 0x00040001u; break;
		}
		memcpy(&ina[i], &b, 4);
		memcpy(&inb[i], &b, 4);
	}
}

/* One configuration, driven through a sequence of calls. */
static void run(unsigned alpha, unsigned ref, unsigned blockLen,
		unsigned count, unsigned pattern, unsigned n, long tag)
{
	memset(oa, 0x5a, sizeof(oa));
	memset(ob, 0x5a, sizeof(ob));
	our_agc_ctor(oa);
	ref_agc_ctor(ob);
	cmp_all(tag);

	/* Fields poked as words on both sides, so an sNaN alpha stays one. */
	poke(oa, 0x00, alpha);   poke(ob, 0x00, alpha);
	poke(oa, 0x0c, ref);     poke(ob, 0x0c, ref);
	poke(oa, 0x18, blockLen); poke(ob, 0x18, blockLen);
	poke(oa, 0x1c, count);   poke(ob, 0x1c, count);

	fill(pattern, 0x3e000000u + tag);
	memset(outa, 0xa5, sizeof(outa));
	memset(outb, 0xa5, sizeof(outb));
	our_agc_process(oa, ina, outa, n);
	ref_agc_process(ob, inb, outb, n);
	cmp_all(tag + 1);

	/* A second call, so the carried `count` and `gain` are exercised. */
	memset(outa, 0xa5, sizeof(outa));
	memset(outb, 0xa5, sizeof(outb));
	our_agc_process(oa, ina, outa, n);
	ref_agc_process(ob, inb, outb, n);
	cmp_all(tag + 2);
}

int
main(void)
{
	static const unsigned alphas[] = {
		0x3f800000u,		/* 1.0 -- frozen                     */
		0x3f000000u,		/* 0.5                               */
		0x3f7ff972u,		/* 0.9999                            */
		0x00000000u,		/* 0.0 -- gain jumps straight there  */
		0xbf800000u,		/* -1.0                              */
		0x40000000u,		/* 2.0 -- the cancellation amplifier */
		0x7fc00000u,		/* quiet NaN -- must read as frozen  */
		0x7fa00000u,		/* SIGNALLING NaN -- the sNaN case   */
		0x7f800000u		/* +inf                              */
	};
	static const unsigned refs[] = {
		0x3f800000u, 0x00000000u, 0x40800000u, 0xbf800000u,
		0x7f7fffffu, 0x00000001u
	};
	static const unsigned blens[] = { 1, 2, 3, 4, 8, 500, 0xffffffffu };
	static const unsigned lens[] = { 0, 1, 2, 3, 4, 8, 9, 64 };
	int rc = 0;
	unsigned a, r, b, p, n;
	long tag = 0;

	diff_begin("agc: construct, reset and the frozen default");
	{
		unsigned i;

		for (i = 0; i < 8; i++) {
			memset(oa, 0x5a, sizeof(oa));
			memset(ob, 0x5a, sizeof(ob));
			our_agc_ctor(oa);
			ref_agc_ctor(ob);
			cmp_all(tag++);
			/*
			 * `reset` after the block length has been changed is
			 * where `count` and `blockLen` can disagree -- the
			 * caller defect in finding F608 is exactly this.
			 */
			poke(oa, 0x18, blens[i % 7]);
			poke(ob, 0x18, blens[i % 7]);
			our_agc_reset(oa);
			ref_agc_reset(ob);
			cmp_all(tag++);
		}
	}
	rc |= diff_end();

	/*
	 * FREEZE IN EVERY ORDER, with a signalling NaN among the poles.  The
	 * save reads `alpha` before overwriting it, and both the order and the
	 * integer copy are what this block pins.
	 */
	diff_begin("agc: freeze, including a signalling NaN pole");
	for (a = 0; a < sizeof(alphas) / sizeof(alphas[0]); a++) {
		int k;

		memset(oa, 0x5a, sizeof(oa));
		memset(ob, 0x5a, sizeof(ob));
		our_agc_ctor(oa);
		ref_agc_ctor(ob);
		for (k = 0; k < 3; k++) {
			poke(oa, 0x00, alphas[a]);
			poke(ob, 0x00, alphas[a]);
			our_agc_freeze(oa);
			ref_agc_freeze(ob);
			cmp_all(tag++);
			if (k == 1) {
				our_agc_reset(oa);
				ref_agc_reset(ob);
				cmp_all(tag++);
			}
		}
	}
	rc |= diff_end();

	diff_begin("agc: process over pole x reference x block x length");
	for (a = 0; a < sizeof(alphas) / sizeof(alphas[0]); a++)
		for (r = 0; r < sizeof(refs) / sizeof(refs[0]); r++)
			for (b = 0; b < sizeof(blens) / sizeof(blens[0]); b++)
				for (n = 0; n < sizeof(lens) / sizeof(lens[0]); n++)
					run(alphas[a], refs[r], blens[b],
					    blens[b], 2, lens[n], tag++);
	rc |= diff_end();

	diff_begin("agc: every input shape, including silence and NaN");
	for (p = 0; p < 8; p++)
		for (b = 0; b < sizeof(blens) / sizeof(blens[0]); b++)
			for (n = 0; n < sizeof(lens) / sizeof(lens[0]); n++)
				run(0x3f000000u, 0x3f800000u, blens[b],
				    blens[b], p, lens[n], tag++);
	rc |= diff_end();

	/*
	 * THE BLOCK BOUNDARY, walked one sample at a time.  The update runs
	 * before the length is retested, so a call ending exactly on a boundary
	 * still performs it -- and a `count` seeded part-way through a block is
	 * how the boundary is reached at every offset.
	 */
	diff_begin("agc: the block boundary at every offset");
	for (b = 0; b < 5; b++) {
		unsigned bl = blens[b] > 16 ? 16 : blens[b];

		for (a = 1; a <= bl; a++)
			for (n = 0; n < sizeof(lens) / sizeof(lens[0]); n++)
				run(0x3f000000u, 0x3f800000u, bl, a, 2,
				    lens[n], tag++);
	}
	rc |= diff_end();

	/*
	 * IN PLACE, and with the output pointing into the object itself.  The
	 * loop re-reads `in[0]` and `gain` after storing `acc` precisely
	 * because that store can alias them, and nothing else in the test can
	 * tell a cached read from a re-read.
	 */
	diff_begin("agc: in place, and writing over the object");
	for (b = 0; b < 5; b++)
		for (n = 1; n < sizeof(lens) / sizeof(lens[0]); n++) {
			unsigned i;

			memset(oa, 0x5a, sizeof(oa));
			memset(ob, 0x5a, sizeof(ob));
			our_agc_ctor(oa);
			ref_agc_ctor(ob);
			poke(oa, 0x00, 0x3f000000u);
			poke(ob, 0x00, 0x3f000000u);
			poke(oa, 0x18, blens[b]); poke(ob, 0x18, blens[b]);
			poke(oa, 0x1c, blens[b]); poke(ob, 0x1c, blens[b]);
			fill(2, 0);

			/* in == out */
			our_agc_process(oa, ina, ina, lens[n]);
			ref_agc_process(ob, inb, inb, lens[n]);
			for (i = 0; i < NBUF; i++) {
				unsigned x, y;

				memcpy(&x, &ina[i], 4);
				memcpy(&y, &inb[i], 4);
				diff_eq_int("in place", (long)x, (long)y,
					    tag * 100 + i);
			}
			for (i = 0; i < SLOT; i += 4) {
				unsigned x, y;

				memcpy(&x, oa + i, 4);
				memcpy(&y, ob + i, 4);
				diff_eq_int("object after in place",
					    (long)x, (long)y, tag * 100 + i);
			}
			tag++;

			/* out aimed at the object's own tail */
			fill(2, 0);
			our_agc_process(oa, ina, (float *)(oa + 32), 4);
			ref_agc_process(ob, inb, (float *)(ob + 32), 4);
			for (i = 0; i < SLOT; i += 4) {
				unsigned x, y;

				memcpy(&x, oa + i, 4);
				memcpy(&y, ob + i, 4);
				diff_eq_int("object written through",
					    (long)x, (long)y, tag * 100 + i);
			}
			tag++;
		}
	rc |= diff_end();

	/*
	 * THE TWO CASES THAT SEPARATE A RECIPROCAL FROM A DIVIDE.  Both were at
	 * zero kills until built for deliberately (finding F608): the `>` guard
	 * needs `lvl` to land exactly on 1e-10f, and `acc/blockLen` needs a
	 * denormal `acc` whose quotient sits on a float rounding midpoint, so
	 * that a difference of about 2^-64 decides the tie.
	 */
	diff_begin("agc: the ties that separate a reciprocal from a divide");
	{
		/*
		 * THE FOUR DENORMAL `acc` VALUES ARE WITNESSES, found by
		 * exhaustive search over the denormal range: with blockLen 500,
		 * `(1/500)*acc` and `acc/500` round to DIFFERENT floats
		 * (0x00000001 against 0x00000002, and so on).  Nothing else
		 * separates the two readings, and a block length that is a
		 * power of two never can -- the division is exact.  Without
		 * these the reciprocal is taken on the object's bytes alone.
		 *
		 * 0x2edbe6ff and its neighbours are the other tie: they put
		 * `lvl` either side of 1e-10f exactly, which is what separates
		 * the `>` guard from a `>=`.
		 */
		static const unsigned accs[] = {
			0x000002eeu, 0x000006d6u, 0x00000abeu, 0x00000ea6u,
			0x2edbe6ffu, 0x2edbe700u, 0x2edbe701u,
			0x00000001u, 0x00000002u, 0x000003e8u, 0x0000ffffu
		};
		unsigned i;

		for (i = 0; i < sizeof(accs) / sizeof(accs[0]); i++)
			for (b = 0; b < sizeof(blens) / sizeof(blens[0]); b++) {
				memset(oa, 0x5a, sizeof(oa));
				memset(ob, 0x5a, sizeof(ob));
				our_agc_ctor(oa);
				ref_agc_ctor(ob);
				poke(oa, 0x00, 0x3f000000u);
				poke(ob, 0x00, 0x3f000000u);
				poke(oa, 0x14, accs[i]); poke(ob, 0x14, accs[i]);
				poke(oa, 0x18, blens[b]); poke(ob, 0x18, blens[b]);
				poke(oa, 0x1c, 1);       poke(ob, 0x1c, 1);
				fill(1, 0);		/* silence */
				memset(outa, 0xa5, sizeof(outa));
				memset(outb, 0xa5, sizeof(outb));
				our_agc_process(oa, ina, outa, 1);
				ref_agc_process(ob, inb, outb, 1);
				cmp_all(tag++);
			}
	}
	rc |= diff_end();

	return rc;
}
