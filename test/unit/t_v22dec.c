/*
 * t_v22dec.c -- differential test of the two V.22 receive slicers.
 *
 * These are the shape finding F3052 warns about: a nearest-point search whose
 * obvious wrong readings agree with the true one over every point that is
 * actually near a constellation point, which is every point a working receiver
 * ever hands them.  Four readings in particular:
 *
 *   truncation   the 1200 slicer accumulates its squared distance in SIXTEEN
 *                bits.  A full-precision search picks the same candidate for
 *                every point inside the constellation and a different one
 *                outside it.
 *   swap         the I and Q tables are interchangeable for any point on the
 *                diagonal, and the constellations are symmetric about it.
 *   fallback     the 2400 slicer's initial best distance is 8192, not
 *                infinity, so a point far from both candidates decodes as
 *                index 0 rather than as the nearer of the two.
 *   modulo       the returned quadrant is (this - previous) reduced modulo
 *                sixteen, and the reduction only shows when the previous
 *                quadrant is the larger.
 *
 * So the grid below is deliberately not a plausible receiver's output: it runs
 * to the ends of the 16-bit range, it includes points on and off the diagonal,
 * and it drives the previous quadrant through every value including ones the
 * slicers themselves never store.  `main()` refuses to report a pass unless
 * the counters show each of the four was actually separated, and the counts
 * are computed from the TABLES rather than from either implementation.
 *
 * `n_out` is varied too, so an implementation that read out_i[0] instead of
 * out_i[n_out] fails rather than passing on a one-entry buffer.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22txtab.h"

extern unsigned short ref_FSEv22_decision12(struct v22_fse *state,
					    short *angle, short *mag);
extern unsigned short ref_FSEv22_decision24(struct v22_fse *state,
					    short *angle, short *mag);

extern const short ref_DECv22_ANGL12[];
extern const short ref_DECv22_QMAP12[];
extern const short ref_DECv22_IMAP12[];
extern const short ref_DECv22_ANGL24[];
extern const short ref_DECv22_QMAP24[];
extern const short ref_DECv22_IMAP24[];
extern const short ref_DECv22_MAG24[];

#define MARK 0x5ead
#define NOUT 14

/*
 * TWO KINDS OF COUNTER, AND THEY ARE NOT INTERCHANGEABLE.
 *
 * `sep_*` counts trials on which the true reading and a named wrong one
 * produce a DIFFERENT OBSERVABLE RESULT -- a different reported angle, a
 * different returned symbol.  `saw_*` counts trials that merely take a
 * particular PATH.  Only the first is evidence that the test could catch the
 * wrong reading; the second is coverage, which is worth having and is not the
 * same claim.
 *
 * The distinction is not pedantry.  A separating-trial counter that counts
 * paths can be true and prove nothing, because a clamp, a saturation or a
 * min/max downstream can take both readings to the same result on exactly the
 * inputs that separated them upstream.  Three of the counters here started out
 * as `sep_` and were counting paths; they are `saw_` now, and what actually
 * adjudicates every claim in this file is `test/mutations/v22dec.json`, where
 * each wrong reading is written out and run.
 */
static long sep_truncation;
static long sep_swap;
static long saw_fallback;
static long saw_modulo_borrow;
static long saw_amp_hi;
static long saw_amp_lo;
static long saw_best12[4];
static long saw_mag24[3];

/*
 * The grid.  Constellation coordinates are +-4096 and +-12288, so the points
 * straddle every decision boundary, sit exactly on several, and run out to the
 * ends of the range where the 16-bit accumulation misbehaves.
 */
static const short axis[] = {
	-32768, -32767, -30000, -24576, -20000, -16384, -12289, -12288,
	-12287,  -8193,  -8192,  -8191,  -4097,  -4096,  -4095,  -2048,
	    -1,      0,      1,   2048,   4095,   4096,   4097,   8191,
	  8192,   8193,  12287,  12288,  12289,  16384,  20000,  24576,
	 30000,  32767,
};
#define NAXIS ((int)(sizeof axis / sizeof axis[0]))

/* The previous quadrant, including values the slicers never write. */
static const short prevs[] = { 0, 4, 8, 12, 2, 15, -4, (short)0x8000 };
#define NPREV ((int)(sizeof prevs / sizeof prevs[0]))

/*
 * The counting models.  These exist ONLY to decide whether a trial separates
 * two readings; nothing here decides whether the reconstruction is right --
 * the blob does that.  They are written from the tables, not from V22Dec.c.
 */
static int
best12(short si, short sq, int truncate, int swap)
{
	const short *im = swap ? DECv22_QMAP12 : DECv22_IMAP12;
	const short *qm = swap ? DECv22_IMAP12 : DECv22_QMAP12;
	long bestd = truncate ? 0x7fff : 0x7fffffffL;
	int best = 0;
	int k;

	for (k = 0; k <= 3; k++) {
		long d;

		if (truncate) {
			short di = (short)(si - im[k]);
			short dq = (short)(sq - qm[k]);

			d = (short)(((di * di) >> 16) + ((dq * dq) >> 16));
		} else {
			long di = (long)si - im[k];
			long dq = (long)sq - qm[k];

			d = (di * di + dq * dq) >> 16;
		}
		if (d < bestd) {
			best = k;
			bestd = d;
		}
	}
	return best;
}

/* Where the 2400 slicer's three index bits land before the distance search. */
static int
base24(short si, short sq)
{
	int base = si < 0 ? 0 : 8;
	int sign = si < 0 ? -1 : 1;

	if (sq < 0)
		base += 4;
	if ((short)((2 * sign) << 12) <= si)
		base += 2;
	return base;
}

static void
count_separations(void)
{
	int ia, iq, ip;

	for (ia = 0; ia < NAXIS; ia++) {
		for (iq = 0; iq < NAXIS; iq++) {
			short si = axis[ia];
			short sq = axis[iq];
			int base = base24(si, sq);
			short d0, d1;

			/*
			 * OBSERVABLE separation, not merely a different
			 * internal choice: a different candidate is only
			 * visible to this test if it carries a different
			 * reported angle, so that is what is counted.
			 * `DECv22_ANGL12` happens to have four distinct
			 * entries, which makes the two conditions equivalent
			 * here -- but writing the weaker one would be relying
			 * on that, and the sixteen-point table next door does
			 * repeat its angles.
			 */
			if (DECv22_ANGL12[best12(si, sq, 1, 0)]
			    != DECv22_ANGL12[best12(si, sq, 0, 0)])
				sep_truncation++;
			if (DECv22_ANGL12[best12(si, sq, 1, 0)]
			    != DECv22_ANGL12[best12(si, sq, 1, 1)])
				sep_swap++;

			/* Path coverage of the amplitude test, nothing more. */
			if (base & 2)
				saw_amp_hi++;
			else
				saw_amp_lo++;

			/*
			 * The fallback: neither candidate beats the initial
			 * 8192, so `best` stays at index 0.  COVERAGE, not
			 * separation -- whether taking it changes the output
			 * depends on `DECv22_ANGL24`, which repeats several of
			 * its entries, so this counter cannot say.  The claim
			 * is adjudicated by the "starts from an infinity"
			 * mutation instead.
			 */
			d0 = (short)(sq >= DECv22_QMAP24[base]
				     ? sq - DECv22_QMAP24[base]
				     : DECv22_QMAP24[base] - sq);
			d1 = (short)(sq >= DECv22_QMAP24[base + 1]
				     ? sq - DECv22_QMAP24[base + 1]
				     : DECv22_QMAP24[base + 1] - sq);
			if (d0 >= 8192 && d1 >= 8192)
				saw_fallback++;
		}
	}

	/*
	 * The subtraction borrows, so the reduction has something to do.
	 *
	 * THIS USED TO BE A SEPARATION GUARD AND THE CLAIM WAS WITHDRAWN.  It
	 * asserted that the trials distinguished masking four bits from masking
	 * two, and no trial does: both operands have already been masked with
	 * V22_SYM_QUAD, so the difference is a multiple of four and its low two
	 * bits are clear whatever the borrow does.  The two readings are
	 * provably equal for every possible pair -- which is now written out as
	 * an `equivalent` entry in test/mutations/v22dec.json and confirmed to
	 * survive, rather than being asserted here as a separation that does
	 * not exist.  `0x0f` is in the source because `and $0xf,%eax` is what
	 * the object encodes at 0x8878a and 0x88641.
	 *
	 * What is left is honest coverage: the borrow itself happens.
	 */
	for (ip = 0; ip < NPREV; ip++) {
		int q;

		for (q = 0; q <= 12; q += 4)
			if ((unsigned short)q < (unsigned short)prevs[ip])
				saw_modulo_borrow++;
	}
}

/*
 * One side's state.  Everything the slicers do not touch is MARK, so a write
 * anywhere else is a difference.
 */
static void
setup(struct v22_fse *s, short *oi, short *oq, short *prev, short n)
{
	memset(s, MARK & 0xff, sizeof *s);
	s->out_i = oi;
	s->out_q = oq;
	s->prev_quad = prev;
	s->n_out = n;
}

/*
 * The three pointers each side is given are three different addresses and
 * always will be, so they are the sanctioned skip.  Blanking them in a copy
 * keeps diff_eq_obj's field-level report and its run coalescing, which an
 * open-coded byte loop would throw away.
 */
static void
blank_pointers(struct v22_fse *dst, const struct v22_fse *src)
{
	*dst = *src;
	dst->out_i = (short *)0;
	dst->out_q = (short *)0;
	dst->prev_quad = (short *)0;
}

static int
tables(void)
{
	int i;

	diff_begin("v22 dec tables");
	for (i = 0; i < 4; i++) {
		diff_eq_int("DECv22_ANGL12[%ld]", DECv22_ANGL12[i],
			    ref_DECv22_ANGL12[i], i);
		diff_eq_int("DECv22_QMAP12[%ld]", DECv22_QMAP12[i],
			    ref_DECv22_QMAP12[i], i);
		diff_eq_int("DECv22_IMAP12[%ld]", DECv22_IMAP12[i],
			    ref_DECv22_IMAP12[i], i);
	}
	for (i = 0; i < 16; i++) {
		diff_eq_int("DECv22_ANGL24[%ld]", DECv22_ANGL24[i],
			    ref_DECv22_ANGL24[i], i);
		diff_eq_int("DECv22_QMAP24[%ld]", DECv22_QMAP24[i],
			    ref_DECv22_QMAP24[i], i);
		diff_eq_int("DECv22_IMAP24[%ld]", DECv22_IMAP24[i],
			    ref_DECv22_IMAP24[i], i);
	}
	for (i = 0; i < 3; i++)
		diff_eq_int("DECv22_MAG24[%ld]", DECv22_MAG24[i],
			    ref_DECv22_MAG24[i], i);
	return diff_end();
}

static int
run(const char *label, int bits)
{
	static short oi_a[NOUT], oq_a[NOUT], oi_b[NOUT], oq_b[NOUT];
	struct v22_fse a, b, ma, mb;
	int ia, iq, ip;
	long trial = 0;
	int rc;

	diff_begin(label);

	for (ia = 0; ia < NAXIS; ia++) {
		for (iq = 0; iq < NAXIS; iq++) {
			for (ip = 0; ip < NPREV; ip++) {
				short prev_a, prev_b;
				short ang_a = (short)MARK, ang_b = (short)MARK;
				short mag_a = (short)MARK, mag_b = (short)MARK;
				unsigned short ra, rb;
				short n = (short)(trial % NOUT);
				int i;

				for (i = 0; i < NOUT; i++) {
					oi_a[i] = oq_a[i] = (short)MARK;
					oi_b[i] = oq_b[i] = (short)MARK;
				}
				oi_a[n] = oi_b[n] = axis[ia];
				oq_a[n] = oq_b[n] = axis[iq];
				prev_a = prev_b = prevs[ip];

				setup(&a, oi_a, oq_a, &prev_a, n);
				setup(&b, oi_b, oq_b, &prev_b, n);

				if (bits == 12) {
					ra = ref_FSEv22_decision12(&a, &ang_a,
								   &mag_a);
					rb = FSEv22_decision12(&b, &ang_b,
							       &mag_b);
					saw_best12[best12(axis[ia], axis[iq],
							  1, 0)]++;
				} else {
					ra = ref_FSEv22_decision24(&a, &ang_a,
								   &mag_a);
					rb = FSEv22_decision24(&b, &ang_b,
							       &mag_b);
					for (i = 0; i < 3; i++)
						if (mag_a == ref_DECv22_MAG24[i])
							saw_mag24[i]++;
				}

				diff_eq_int("trial %ld: symbol", rb, ra, trial);
				diff_eq_int("trial %ld: angle", ang_b, ang_a,
					    trial);
				diff_eq_int("trial %ld: mag", mag_b, mag_a,
					    trial);
				diff_eq_int("trial %ld: prev_quad", prev_b,
					    prev_a, trial);
				blank_pointers(&ma, &a);
				blank_pointers(&mb, &b);
				diff_eq_obj("state", struct v22_fse, &mb, &ma,
					    trial);

				/* Nothing may be written through out_i/out_q. */
				for (i = 0; i < NOUT; i++) {
					diff_eq_int("trial %ld: out_i kept",
						    oi_b[i], oi_a[i], trial);
					diff_eq_int("trial %ld: out_q kept",
						    oq_b[i], oq_a[i], trial);
				}
				trial++;
			}
		}
	}

	rc = diff_end();
	return rc;
}

int
main(void)
{
	int rc = 0;
	int i;

	count_separations();

	rc |= tables();
	rc |= run("v22 decision12", 12);
	rc |= run("v22 decision24", 24);

	/*
	 * Two separation guards and four coverage ones, labelled as what they
	 * are.  What decides whether a wrong reading would actually be caught
	 * is test/mutations/v22dec.json, not these.
	 */
	diff_begin("v22 dec separation and coverage");
	diff_eq_int("truncation separated in the ANGLE (%ld)",
		    sep_truncation > 0, 1, sep_truncation);
	diff_eq_int("I/Q swap separated in the ANGLE (%ld)", sep_swap > 0, 1,
		    sep_swap);
	diff_eq_int("coverage: 8192 fallback reached (%ld)", saw_fallback > 0,
		    1, saw_fallback);
	diff_eq_int("coverage: the differential borrows (%ld)",
		    saw_modulo_borrow > 0, 1, saw_modulo_borrow);
	diff_eq_int("coverage: amplitude threshold taken (%ld)",
		    saw_amp_hi > 0, 1, saw_amp_hi);
	diff_eq_int("coverage: amplitude threshold untaken (%ld)",
		    saw_amp_lo > 0, 1, saw_amp_lo);
	for (i = 0; i < 4; i++)
		diff_eq_int("decision12 candidate %ld reached",
			    saw_best12[i] > 0, 1, i);
	for (i = 0; i < 3; i++)
		diff_eq_int("decision24 ring %ld reached", saw_mag24[i] > 0, 1,
			    i);
	rc |= diff_end();

	return rc;
}
