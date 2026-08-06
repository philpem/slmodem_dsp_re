/*
 * t_lowpassfir -- LowPassFIR<float> against the blob.
 *
 * EVERY FLOAT ARGUMENT IS PASSED AS BITS.  Both `design` overloads and the
 * constructor take `cutoff` and `gain` by value, and which instruction fills a
 * by-value float's outgoing stack slot is the caller's choice -- GCC has been
 * seen making opposite choices for the two sides of one differential pair, so
 * the two callees would receive different numbers (finding 340).  With a NaN
 * cutoff among the inputs, and the NaN path being one of the behaviours under
 * test, that is not a hypothetical.  Declaring the same entry points a second
 * time with `unsigned` parameters puts a byte-identical stack image in front of
 * both sides; on i386 cdecl the two ABIs are the same.
 *
 * The two sides allocate from separate arenas, so `coefficients` is compared
 * for nullness and, where the window was adopted, for whether it is the buffer
 * THAT side was given -- never as an address.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/LowPassFIR.h"

extern "C" {
void *sysdep_malloc(unsigned int size);

/*
 * The four entry points, on both sides, with the floats declared as `unsigned`.
 * `this` is a plain first stack argument: the object is cdecl, not thiscall.
 * The `WindowType` argument is an `int` here for the same reason -- an enum is
 * passed as one, and the test wants to drive values outside the enumeration.
 */
void ref_lpf_ctor(void *self, unsigned n, unsigned cutoff, int type,
		  unsigned gain)  asm("ref__ZN10LowPassFIRIfEC1Ejf10WindowTypef");
void our_lpf_ctor(void *self, unsigned n, unsigned cutoff, int type,
		  unsigned gain)  asm("_ZN10LowPassFIRIfEC1Ejf10WindowTypef");
void ref_lpf_dtor(void *self) asm("ref__ZN10LowPassFIRIfED1Ev");
void our_lpf_dtor(void *self) asm("_ZN10LowPassFIRIfED1Ev");
int  ref_lpf_design4(void *self, unsigned n, unsigned cutoff, int type,
		     unsigned gain)
	asm("ref__ZN10LowPassFIRIfE6designEjf10WindowTypef");
int  our_lpf_design4(void *self, unsigned n, unsigned cutoff, int type,
		     unsigned gain)
	asm("_ZN10LowPassFIRIfE6designEjf10WindowTypef");
int  ref_lpf_design5(void *self, unsigned n, unsigned cutoff, unsigned gain,
		     const float *window, int adopt)
	asm("ref__ZN10LowPassFIRIfE6designEjffPKfi");
int  our_lpf_design5(void *self, unsigned n, unsigned cutoff, unsigned gain,
		     const float *window, int adopt)
	asm("_ZN10LowPassFIRIfE6designEjffPKfi");
}

/* The blob's object, so its two words can be read without a second class. */
struct ref_lpf {
	float		*coef;
	unsigned int	 taps;
};

static unsigned fbits(float f)
{
	union { float f; unsigned u; } u;

	u.f = f;
	return u.u;
}

static long cbits(float f)
{
	union { float f; unsigned u; } u;

	u.f = f;
	return (long)u.u;
}

/*
 * Compare the two objects and, where there is one, the whole coefficient array.
 *
 * HOW MANY COEFFICIENTS TO COMPARE IS TAKEN FROM THE REFERENCE, because a
 * rejected design leaves `taps` uninitialised -- the constructor writes only
 * `coefficients` -- and trusting our own word would mean indexing off whatever
 * the poison left there.  Our `taps` is compared against the blob's first, so a
 * disagreement is reported before it can be used.
 */
static void cmp(const void *a, const void *b, long tag, int coefs = 1)
{
	const ref_lpf *x = (const ref_lpf *)a;
	const ref_lpf *y = (const ref_lpf *)b;
	unsigned i;

	diff_eq_int("taps", (long)x->taps, (long)y->taps, tag);
	diff_eq_int("coefficients allocated", x->coef != 0, y->coef != 0, tag);
	for (i = 8; i < 32; i++)
		diff_eq_int("wrote past the object",
			    (long)((const unsigned char *)a)[i],
			    (long)((const unsigned char *)b)[i],
			    tag * 100 + (long)i);
	/*
	 * `coefs` is 0 after the destructor.  Both sides have freed the array
	 * by then, and what an allocator leaves in freed memory is its own
	 * business rather than the object's -- comparing it compares the two
	 * arenas.  The stale POINTER is still compared above, because the
	 * destructor deliberately does not null it.
	 */
	if (!coefs || !x->coef || !y->coef || x->taps != y->taps ||
	    y->taps > 4096)
		return;
	for (i = 0; i < y->taps; i++)
		diff_eq_int("coefficient", cbits(x->coef[i]), cbits(y->coef[i]),
			    tag * 10000 + i);
}

int
main(void)
{
	/*
	 * The interesting cutoffs are the boundaries.  1.0 is Nyquist and is
	 * accepted; the next float above it is refused; -0.0 compares equal to
	 * zero and is accepted; a NaN is refused by the FIRST test, because the
	 * object uses `fcoms`/`jb` and an unordered compare fails it.  Zero is
	 * accepted and produces a whole array of x87 indefinites, which is the
	 * blob's behaviour and is compared as bits like everything else.
	 */
	static const float cutoffs[] = {
		0.0f, -0.0f, 0.25f, 0.5f, 1.0f, 0.001f, 0.9999f, 0.75f,
		1.0000001f, -0.001f, 2.0f
	};
	static const float gains[] = { 1.0f, 0.0f, -2.5f, 100.0f, 1e-20f };
	static const unsigned taps[] = {
		0, 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 32, 33, 64, 65
	};
	static const int types[] = { 0, 1, 2, 3, 4, -1 };
	static unsigned char oa[32], ob[32];
	static float win[128];
	unsigned nan_bits = 0x7fc00000u;
	unsigned inf_bits = 0x7f800000u;
	unsigned snan_bits = 0x7f800001u;
	int rc = 0;
	unsigned c, g, n, i;
	int t;
	long tag = 0;

	/*
	 * The constructor, over the cross product.  It is a null store and a
	 * tail call into the four-argument `design`, so this exercises both --
	 * including the window allocation, `designWindow`, and the adopt.
	 */
	diff_begin("lowpassfir: construct over taps x cutoff x window x gain");
	for (n = 0; n < sizeof(taps) / sizeof(taps[0]); n++)
		for (c = 0; c < sizeof(cutoffs) / sizeof(cutoffs[0]); c++)
			for (t = 0; t < (int)(sizeof(types) / sizeof(types[0])); t++)
				for (g = 0; g < sizeof(gains) / sizeof(gains[0]); g++) {
					memset(oa, 0x5a, sizeof(oa));
					memset(ob, 0x5a, sizeof(ob));
					our_lpf_ctor(oa, taps[n], fbits(cutoffs[c]),
						     types[t], fbits(gains[g]));
					ref_lpf_ctor(ob, taps[n], fbits(cutoffs[c]),
						     types[t], fbits(gains[g]));
					cmp(oa, ob, tag++);
					our_lpf_dtor(oa);
					ref_lpf_dtor(ob);
					/* The pointer is not nulled. */
					cmp(oa, ob, tag++, 0);
				}
	rc |= diff_end();

	/*
	 * The cutoffs that are not numbers.  Split out because they are the
	 * reason the first test is written `!(cutoff >= 0)` rather than
	 * `cutoff < 0`, and because passing them as bits is what makes the
	 * comparison honest -- a signalling NaN routed through an x87 register
	 * would arrive quiet at one side and not the other.
	 */
	diff_begin("lowpassfir: NaN, signalling NaN and infinity cutoffs");
	{
		unsigned pats[3];
		unsigned k;

		pats[0] = nan_bits;
		pats[1] = snan_bits;
		pats[2] = inf_bits;
		for (k = 0; k < 3; k++)
			for (n = 0; n < sizeof(taps) / sizeof(taps[0]); n++)
				for (g = 0; g < sizeof(gains) / sizeof(gains[0]); g++) {
					memset(oa, 0x5a, sizeof(oa));
					memset(ob, 0x5a, sizeof(ob));
					our_lpf_ctor(oa, taps[n], pats[k], 1,
						     fbits(gains[g]));
					ref_lpf_ctor(ob, taps[n], pats[k], 1,
						     fbits(gains[g]));
					cmp(oa, ob, tag++);
					our_lpf_dtor(oa);
					ref_lpf_dtor(ob);
				}

		/* And a NaN gain, which is not rejected and reaches the divide. */
		for (n = 0; n < sizeof(taps) / sizeof(taps[0]); n++) {
			memset(oa, 0x5a, sizeof(oa));
			memset(ob, 0x5a, sizeof(ob));
			our_lpf_ctor(oa, taps[n], fbits(0.5f), 2, nan_bits);
			ref_lpf_ctor(ob, taps[n], fbits(0.5f), 2, nan_bits);
			cmp(oa, ob, tag++);
			our_lpf_dtor(oa);
			ref_lpf_dtor(ob);
		}
	}
	rc |= diff_end();

	/*
	 * THE PRIMITIVE, DIRECTLY, over its three window paths.
	 *
	 *   window == 0   build a Hamming window in place -- and it calls
	 *                 `hamming` itself, not `designWindow`
	 *   adopt == 0    allocate and COPY the caller's window
	 *   adopt != 0    take the caller's buffer, write through it, and free
	 *                 it from the destructor
	 *
	 * The adopt cases hand over a `sysdep_malloc`ed buffer per side,
	 * because the object really does own it afterwards.  `adopt` is driven
	 * with a negative value as well as 1: the object tests it against zero,
	 * not against 1.
	 */
	diff_begin("lowpassfir: the primitive's three window paths");
	{
		static const int adopts[] = { 0, 1, -7, 2 };
		unsigned a;

		for (i = 0; i < 128; i++)
			win[i] = 0.25f + (float)i * 0.01f;

		for (n = 0; n < sizeof(taps) / sizeof(taps[0]); n++)
			for (c = 0; c < sizeof(cutoffs) / sizeof(cutoffs[0]); c++) {
				/* window == 0: the Hamming path. */
				memset(oa, 0x5a, sizeof(oa));
				memset(ob, 0x5a, sizeof(ob));
				((ref_lpf *)oa)->coef = 0;
				((ref_lpf *)ob)->coef = 0;
				diff_eq_int("design5 null window",
					    our_lpf_design5(oa, taps[n],
							    fbits(cutoffs[c]),
							    fbits(1.0f), 0, 0),
					    ref_lpf_design5(ob, taps[n],
							    fbits(cutoffs[c]),
							    fbits(1.0f), 0, 0),
					    tag);
				cmp(oa, ob, tag++);
				our_lpf_dtor(oa);
				ref_lpf_dtor(ob);

				for (a = 0; a < sizeof(adopts) / sizeof(adopts[0]); a++) {
					float *wa, *wb;
					unsigned nt = taps[n] ? taps[n] : 1;

					wa = (float *)sysdep_malloc(nt * sizeof(float));
					wb = (float *)sysdep_malloc(nt * sizeof(float));
					for (i = 0; i < nt; i++)
						wa[i] = wb[i] = win[i];

					memset(oa, 0x5a, sizeof(oa));
					memset(ob, 0x5a, sizeof(ob));
					((ref_lpf *)oa)->coef = 0;
					((ref_lpf *)ob)->coef = 0;
					diff_eq_int("design5 return",
						    our_lpf_design5(oa, taps[n],
								    fbits(cutoffs[c]),
								    fbits(2.0f),
								    wa, adopts[a]),
						    ref_lpf_design5(ob, taps[n],
								    fbits(cutoffs[c]),
								    fbits(2.0f),
								    wb, adopts[a]),
						    tag);
					/*
					 * Was the buffer adopted or copied?
					 * Compared as "is it THIS side's own
					 * window", which is the only form of
					 * the question both runs can answer.
					 */
					diff_eq_int("window adopted",
						    ((ref_lpf *)oa)->coef == wa,
						    ((ref_lpf *)ob)->coef == wb,
						    tag);
					cmp(oa, ob, tag++);
					our_lpf_dtor(oa);
					ref_lpf_dtor(ob);
				}
			}
	}
	rc |= diff_end();

	/*
	 * REDESIGN CHAINS.  `design` frees the previous array before deciding
	 * on the new one, so running several designs over one object is the
	 * only way to reach that `sysdep_free` -- a single design always has a
	 * null pointer there.  The chain deliberately mixes accepted and
	 * rejected argument sets, so a rejection lands between two successes
	 * and the state it leaves behind is carried into the next call.
	 */
	diff_begin("lowpassfir: redesign over one object");
	{
		static const unsigned chain_n[] = { 16, 0, 33, 1, 8, 64, 2, 17 };
		static const float chain_c[] = {
			0.5f, 0.25f, 2.0f, 0.3f, 0.0f, 1.0f, -0.5f, 0.75f
		};
		unsigned k;

		for (t = 0; t < (int)(sizeof(types) / sizeof(types[0])); t++) {
			memset(oa, 0x5a, sizeof(oa));
			memset(ob, 0x5a, sizeof(ob));
			our_lpf_ctor(oa, 8, fbits(0.4f), types[t], fbits(1.0f));
			ref_lpf_ctor(ob, 8, fbits(0.4f), types[t], fbits(1.0f));
			cmp(oa, ob, tag++);

			for (k = 0; k < sizeof(chain_n) / sizeof(chain_n[0]); k++) {
				diff_eq_int("redesign return",
					    our_lpf_design4(oa, chain_n[k],
							    fbits(chain_c[k]),
							    types[t], fbits(1.5f)),
					    ref_lpf_design4(ob, chain_n[k],
							    fbits(chain_c[k]),
							    types[t], fbits(1.5f)),
					    tag);
				cmp(oa, ob, tag++);
			}
			our_lpf_dtor(oa);
			ref_lpf_dtor(ob);
		}
	}
	rc |= diff_end();

	return rc;
}
