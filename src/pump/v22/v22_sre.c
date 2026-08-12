/*
 * v22_sre.c -- V.22 / V.22bis symbol-timing recovery.
 *
 * Reconstructed from dsplibs.o v22_sre.c:
 *
 *   V22_SRE_recover   .text 0x08d800, 1883 bytes
 *   V22_SRE_init      .text 0x08df60,  387 bytes
 *   V22_SRE_free      .text 0x08e0f0,   46 bytes
 *   SREv22_COFFS      .rodata 0x008760, 542 bytes
 *   SREv22_xCLOCK     .rodata 0x0089a2,  12 bytes
 *   SREv22_yCLOCK     .rodata 0x008996,  12 bytes
 *   SRE_ALPHA_AVG     .rodata 0x008984,   6 bytes
 *   SRE_BETA_AVG      .rodata 0x00897e,   6 bytes
 *   SREv22_PLL_K1     .rodata 0x008990,   6 bytes
 *   SREv22_PLL_K2     .rodata 0x00898a,   6 bytes
 *
 * See include/dsplib/v22_sre.h for what the block does and what the state
 * means.  The call sites settle both signatures:
 *
 *   V22FP_create  +0x600  V22_SRE_init(dp + 0x128, fresh)
 *   ResetRx       +0x...  V22_SRE_init(...)
 *   V22FP_delete  +0x...  V22_SRE_free(...)
 *   DemodDataV22  +0x0ff  V22_SRE_recover(dp + 0x128, ..., ..., ...)
 *
 * so the object is embedded at +0x128 in the V.22 datapump rather than
 * separately allocated.  Its three buffers are not.
 *
 * V22_SRE_recover NEEDED A PREREQUISITE.  It calls FPM_atan, which this tree
 * had not reconstructed; the test harness renames every symbol the blob
 * defines to `ref_*`, so a caller of an unreconstructed callee leaves an
 * undefined reference that breaks every test binary, not just its own.
 * FPM_atan is therefore a hard predecessor and is now src/dsp/fpm_atan.c.
 * recover's only other calls are to sysdep_memcpy, which symmap.py shares
 * between the two sides.
 *
 * ARITHMETIC NOTES, and every one of these is load-bearing:
 *
 *   - init sizes `hist` as ((short)(taps * 2)) * 2 bytes -- 108, i.e. 54
 *     shorts -- and then clears only `taps` of them.  Both halves matter:
 *     the slide in recover copies 27 shorts from hist[27] to hist[0], which
 *     is what makes 54 the real length, and the clear really does stop at
 *     27.  The 14 bytes between the two loops are -falign-loops NOPs, not a
 *     second clear; that was checked byte by byte.
 *   - the coefficient buffer is 270 entries and SREv22_COFFS is 271.  init
 *     copies the first 270, permutes them in place through a stack copy, and
 *     never touches COFFS[270].
 *   - BOTH SMOOTHERS STORE A 16-BIT TRUNCATION AND THE DECISIONS THEN READ
 *     THE TRUNCATED VALUE.  `err_avg`'s sum reaches +-34006 and `mag_avg`'s
 *     +-34768, so the truncation is not theoretical; and the gear-shift
 *     comparisons at 0x7ff and 4, and the squelch comparisons at 2 and 1,
 *     all happen after it.  Test the sum against the field, never against
 *     the int it was computed in.
 *   - the accumulator of the 27-tap dot product is 32-bit and is truncated
 *     to 16 bits exactly once, where `y` is formed.  Everything downstream
 *     sees the truncated value.
 *   - nothing saturates except the loop-filter step, which clamps to
 *     [-18432, +20480] -- and note those two bounds are not symmetric.
 */

#include "dsplib/fpm.h"
#include "dsplib/sysdep.h"
#include "dsplib/v22_sre.h"

/*
 * The prototype filter: 271 taps, Q15, symmetric about COFFS[135] = 30037.
 * A lowpass with about 0.92 of full scale at its centre tap and a first
 * sidelobe near -6300, which is the shape a windowed sinc gives at this
 * length.  Deriving the window is deferred with every other coefficient
 * derivation in this tree; the differential test proves the copy.
 *
 * Reference bytes, extracted by tools/tabdump.py.
 */
const short SREv22_COFFS[V22_SRE_COFFS_LEN] = {
	102, 90, 69, 39, 3, -36, -77, -115,
	-146, -166, -174, -167, -145, -108, -58, 0,
	63, 126, 183, 228, 256, 265, 251, 214,
	155, 79, -8, -101, -193, -273, -336, -373,
	-381, -356, -299, -212, -102, 23, 155, 282,
	392, 476, 523, 528, 489, 404, 281, 126,
	-48, -229, -401, -548, -657, -715, -716, -655,
	-535, -362, -149, 88, 331, 560, 753, 893,
	964, 956, 867, 699, 461, 172, -147, -472,
	-775, -1028, -1207, -1294, -1274, -1145, -911, -586,
	-193, 238, 674, 1077, 1412, 1645, 1752, 1715,
	1530, 1203, 753, 212, -381, -978, -1530, -1986,
	-2302, -2442, -2381, -2113, -1645, -1002, -227, 624,
	1487, 2288, 2956, 3423, 3634, 3549, 3150, 2440,
	1451, 238, -1119, -2526, -3870, -5032, -5896, -6353,
	-6311, -5701, -4484, -2655, -245, 2678, 6016, 9640,
	13397, 17121, 20641, 23791, 26419, 28395, 29621, 30037,
	29621, 28395, 26419, 23791, 20641, 17121, 13397, 9640,
	6016, 2678, -245, -2655, -4484, -5701, -6311, -6353,
	-5896, -5032, -3870, -2526, -1119, 238, 1451, 2440,
	3150, 3549, 3634, 3423, 2956, 2288, 1487, 624,
	-227, -1002, -1645, -2113, -2381, -2442, -2302, -1986,
	-1530, -978, -381, 212, 753, 1203, 1530, 1715,
	1752, 1645, 1412, 1077, 674, 238, -193, -586,
	-911, -1145, -1274, -1294, -1207, -1028, -775, -472,
	-147, 172, 461, 699, 867, 956, 964, 893,
	753, 560, 331, 88, -149, -362, -535, -655,
	-716, -715, -657, -548, -401, -229, -48, 126,
	281, 404, 489, 528, 523, 476, 392, 282,
	155, 23, -102, -212, -299, -356, -381, -373,
	-336, -273, -193, -101, -8, 79, 155, 214,
	251, 265, 256, 228, 183, 126, 63, 0,
	-58, -108, -145, -167, -174, -166, -146, -115,
	-77, -36, 3, 39, 69, 90, 102,
};

/*
 * cos and sin of k * 60 degrees at Q14: 16384 * cos and 16384 * sin, both
 * exact to the unit.  One cycle over six samples is 600 Hz at 3600 Hz, and
 * 600 baud is V.22's symbol rate -- so this pair is the reference against
 * which one symbol's worth of timing discriminant is correlated.
 */
const short SREv22_xCLOCK[V22_SRE_CLOCK] = {
	16384, 8192, -8192, -16384, -8192, 8192,
};

const short SREv22_yCLOCK[V22_SRE_CLOCK] = {
	0, 14189, 14189, 0, -14189, -14189,
};

/*
 * Smoother coefficients, Q15, shared with the other SRE variants -- the
 * symbols carry no `v22` in their names and the V.17 and V.32 blocks index
 * the same two arrays.  Each pair sums to 32768 exactly, so the smoother has
 * unity DC gain: 29720 + 3048 and 31720 + 1048.
 */
const short SRE_ALPHA_AVG[V22_SRE_MODES] = {
	29720, 29720, 31720,
};

const short SRE_BETA_AVG[V22_SRE_MODES] = {
	3048, 3048, 1048,
};

/*
 * Loop gains, Q12.  K2[0] IS ZERO -- mode 0 has no integrator at all and is
 * a first-order loop; modes 1 and 2 are second order with the same pair of
 * gains and differ only in how often they are applied.
 */
const short SREv22_PLL_K1[V22_SRE_MODES] = {
	758, 762, 762,
};

const short SREv22_PLL_K2[V22_SRE_MODES] = {
	0, 4, 4,
};

void
V22_SRE_init(struct v22_sre *sre, int fresh)
{
	/*
	 * The permutation is not in place: the original builds the reordered
	 * filter on the stack and copies it back, which is why the frame is
	 * 0x23c bytes for a function with no other locals.
	 */
	short scratch[V22_SRE_COEFFS];
	short i, j, k;

	sre->active = 0;
	sre->acquiring = 1;
	sre->adapt = 1;
	sre->mode = 0;
	sre->pll_acc = 0;
	sre->err_avg = 0;
	sre->mag_avg = 0;
	sre->taps = V22_SRE_TAPS;
	sre->fill = 0;
	sre->acc_x = 0;
	sre->acc_y = 0;
	sre->frac = 0;
	sre->branch = 0;
	sre->groups = 3;
	sre->settle = 0;
	sre->group = 0;
	sre->tick = 0;
	sre->need = 1;

	if (fresh) {
		/*
		 * No inspection and no free of what may already be there --
		 * the same contract, and the same leak on a second `fresh`
		 * call, as FPM_MRF_init.
		 */
		sre->coeff = (short *)sysdep_malloc(V22_SRE_COEFFS
						    * sizeof(short));
		sre->hist = (short *)sysdep_malloc((short)(sre->taps * 2)
						   * sizeof(short));
		sre->clk = (short *)sysdep_malloc(V22_SRE_CLOCK
						  * sizeof(short));
	}

	/* The prototype, in design order, minus its final tap. */
	for (i = 0; i <= V22_SRE_COEFFS - 1; i++)
		sre->coeff[i] = SREv22_COFFS[i];

	/*
	 * Polyphase de-interleave.  Branch b holds the taps whose prototype
	 * index is congruent to 260 + b modulo 10, walked DOWNWARDS -- so
	 * each branch is time-reversed, which is what lets recover's dot
	 * product read the history forwards.
	 */
	k = 0;
	for (i = V22_SRE_COEFFS - V22_SRE_BRANCHES;
	     i <= V22_SRE_COEFFS - 1; i++)
		for (j = i; j >= 0; j = (short)(j - V22_SRE_BRANCHES))
			scratch[k++] = sre->coeff[j];

	for (i = 0; i <= V22_SRE_COEFFS - 1; i++)
		sre->coeff[i] = scratch[i];

	/*
	 * `taps` entries, not the 2 * taps that were allocated.  See the
	 * header: the upper half is written by recover before it is read.
	 */
	for (i = 0; i < sre->taps; i++)
		sre->hist[i] = 0;

	for (i = 0; i <= V22_SRE_CLOCK - 1; i++)
		sre->clk[i] = 0;
}

void
V22_SRE_free(struct v22_sre *sre)
{
	/* Reverse allocation order, and the pointers are left dangling. */
	sysdep_free(sre->clk);
	sysdep_free(sre->hist);
	sysdep_free(sre->coeff);
}

static int
iabs(int v)
{
	return v < 0 ? -v : v;
}

/*
 * One output sample, from the newest V22_SRE_TAPS entries of the history
 * against polyphase branch `branch`.
 *
 * When fewer than V22_SRE_TAPS samples have ever arrived the window starts at
 * hist[0] instead of hist[fill - taps] -- the original clamps the negative
 * index rather than testing it, which is also why init only has to clear the
 * lower half of the buffer: nothing above `fill` is ever read.
 */
static short
sre_filter(const struct v22_sre *sre, short fill)
{
	const short *c = sre->coeff + sre->branch * sre->taps;
	const short *h = (fill < sre->taps) ? sre->hist
					    : sre->hist + (fill - sre->taps);
	int acc = 0;
	short i;

	for (i = V22_SRE_TAPS - 1; i >= 0; i--)
		acc += *c++ * *h++;

	return (short)(acc >> 15);
}

/*
 * The timing discriminant: three cascaded second-order sections over `clk`,
 * driven by the filter output, whose third stage's one-sample slope is the
 * thing the PLL steers on.
 *
 * The multipliers are the original's literals.  Every section shifts its own
 * accumulator down separately and truncates to 16 bits before the next one
 * reads it, so combining them into one expression changes the answer.
 */
static int
sre_discriminant(short *clk, short y)
{
	int drive = (int)y << 9;
	short a_new, b_new, c_new;
	short u, v, p, q, r;
	int d;

	a_new = (short)((drive - 16128 * clk[1]) >> 14);
	b_new = (short)((drive - 28156 * clk[2] - 16128 * clk[3]) >> 14);

	p = (short)((-16256 * clk[0]) >> 13);
	q = (short)((-8127 * clk[2]) >> 13);
	u = (short)(2 * a_new);
	v = (short)(2 * (b_new - ((-14077 * clk[2]) >> 14)));
	r = (short)((u * v + q * p) >> 13);

	c_new = (short)((992 * r + 15361 * clk[4] - 14399 * clk[5]) >> 14);

	/*
	 * clk[5] takes clk[4]'s OLD value and the slope is measured against
	 * that same old value, so the order of these three lines matters.
	 */
	d = (c_new - clk[5]) >> 1;
	clk[5] = clk[4];
	clk[4] = c_new;

	clk[1] = clk[0];
	clk[0] = a_new;
	clk[3] = clk[2];
	clk[2] = b_new;

	return d;
}

/*
 * Rebuild the ten polyphase branches from the prototype, interpolating
 * between adjacent taps by `frac`.  Mode 2 only: in the faster modes the
 * phase moves in whole branches and the branches init built are used as they
 * are.
 *
 * This is the same walk init does -- start at 260, step down by 10 -- and it
 * is what makes SREv22_COFFS's 271st entry necessary.
 */
static void
sre_interpolate(struct v22_sre *sre, short w0, short w1)
{
	short *co = sre->coeff;
	short i, k, j;

	j = 0;
	for (i = V22_SRE_COEFFS - V22_SRE_BRANCHES;
	     i <= V22_SRE_COEFFS - 1; i++)
		for (k = i; k >= 0; k = (short)(k - V22_SRE_BRANCHES)) {
			/*
			 * The original stores the first term before adding the
			 * second, to the same slot; that store is dead and is
			 * not reproduced.
			 */
			int v = (SREv22_COFFS[k] * w0) >> 15;

			v += (SREv22_COFFS[k + 1] * w1) >> 15;
			co[j++] = (short)v;
		}
}

/*
 * One PLL update: turn the accumulated correlation vector into a phase error,
 * smooth it, decide whether to change gear, and move (branch, frac).
 *
 * Called once every `groups` groups of six samples, so once per 18 samples in
 * modes 0 and 1 and once per 72 in mode 2.
 */
static void
sre_update(struct v22_sre *sre, short yq, short xq)
{
	short angle, e, m, step;
	int err, q, rem, frac, branch;

	FPM_atan(yq, xq, &angle);

	/*
	 * Fold the upper half turn down, so the error is signed about zero
	 * rather than wrapping at 0x8000.
	 */
	if (angle > 0x4000)
		angle = (short)(angle - 0x8000);

	/* 1.5 * angle.  The gain lives here rather than in K1. */
	e = (short)((3 * (int)angle) >> 1);

	m = sre->mode;
	err = ((sre->err_avg * SRE_ALPHA_AVG[m]) >> 15)
	      + ((e * SRE_BETA_AVG[m]) >> 15);
	sre->err_avg = (short)err;

	/* The caller can freeze the phase while still letting it be measured. */
	if (sre->adapt == 0)
		return;

	if (sre->settle > 0x40) {
		/*
		 * Gear shift.  Note the magnitude is taken of the STORED
		 * error, i.e. of the 16-bit truncation, and `err` reaches
		 * +-34006 so that is a real distinction.
		 */
		int a = iabs((int)(short)err);

		if (a > 0x7ff && m == 2) {
			sre->mode = 1;
			sre->groups = 3;
			sre->pll_acc = (short)(sre->pll_acc >> 2);
			m = 1;
		}
		if (a <= 4 && m == 1) {
			short acc = sre->pll_acc;

			sre->mode = 2;
			sre->groups = 12;
			sre->acquiring = 0;
			sre->pll_acc = (short)(acc << 2);
			m = 2;
		}
	} else {
		/*
		 * The first 65 updates.  The integrator is held at zero, so
		 * the loop really is first order here -- and SREv22_PLL_K2[0]
		 * is zero as well, which makes it doubly so.
		 */
		short settle = sre->settle;

		sre->pll_acc = 0;
		sre->settle = (short)(settle + 1);
		m = (settle <= 0x3f) ? 0 : 1;
		sre->mode = m;
	}

	{
		short p;
		int acc = sre->pll_acc;

		p = (short)(((SREv22_PLL_K1[m] * e) >> 12) + acc);
		sre->pll_acc = (short)(((SREv22_PLL_K2[m] * e + 0x800) >> 12)
				       + acc);

		/*
		 * Ten sub-branch steps per input sample, and the sign is
		 * inverted: a positive error retards the sampling instant.
		 * The clamp is deliberately asymmetric.
		 */
		step = (short)((-(10 * p)) >> 4);
	}

	if (step > 0x5000)
		step = 0x5000;
	else if (step < -18432)
		step = -18432;

	/*
	 * Split the step into whole branches and a remainder.  For a negative
	 * step the quotient is the floor plus one, so the remainder is
	 * negative and the borrow below is what carries it.
	 */
	q = step >> 11;
	if (step < 0)
		q = (short)(q + 1);

	branch = sre->branch + q;
	rem = step - (q << 11);
	frac = sre->frac + rem;

	if ((short)frac < 0) {
		branch = branch - 1;
		frac = frac + V22_SRE_FRAC_ONE;
	}
	sre->frac = (short)frac;
	sre->branch = (short)branch;

	if (sre->frac > V22_SRE_FRAC_ONE - 1) {
		sre->frac = (short)(sre->frac - V22_SRE_FRAC_ONE);
		sre->branch = (short)(sre->branch + 1);
	}

	/* Only tracking mode interpolates between prototype taps. */
	if (m == 2) {
		short w1 = (short)(sre->frac << 4);
		short w0 = (short)(0x7fff - w1);

		sre_interpolate(sre, w0, w1);
	}
}

short
V22_SRE_recover(struct v22_sre *sre, const short *in, short *out, short count)
{
	short *hist = sre->hist;
	short taps = sre->taps;
	short fill = sre->fill;
	short need = sre->need;
	short produced = 0;
	short remaining = count;

	while (remaining != 0) {
		short y, tick, mode;
		int d, acc_x, acc_y;

		/*
		 * Not enough input left for another output.  Bank what there
		 * is, carry the shortfall in `need`, and return -- this is
		 * what lets the stream be fed in arbitrary fragments.
		 */
		if (remaining < need) {
			need = (short)(need - remaining);
			if (fill + remaining > V22_SRE_HIST) {
				sysdep_memcpy(hist, hist + taps,
					      taps * sizeof(short));
				fill = (short)(fill - taps);
			}
			sysdep_memcpy(hist + fill, in,
				      remaining * sizeof(short));
			fill = (short)(fill + remaining);
			break;
		}

		/* Slide the window down a whole tap set if the next run of
		 * inputs would not fit above it. */
		if (fill + need > V22_SRE_HIST) {
			sysdep_memcpy(hist, hist + taps,
				      taps * sizeof(short));
			fill = (short)(fill - taps);
		}

		sysdep_memcpy(hist + fill, in, need * sizeof(short));
		in += need;
		fill = (short)(fill + need);
		remaining = (short)(remaining - need);

		y = sre_filter(sre, fill);
		d = sre_discriminant(sre->clk, y);

		/*
		 * Correlate the discriminant against one symbol's worth of
		 * clock reference.  Note the y term SUBTRACTS.
		 */
		tick = sre->tick;
		sre->tick = (short)(tick + 1);
		acc_x = sre->acc_x + ((d * SREv22_xCLOCK[tick]) >> 3);
		acc_y = sre->acc_y - ((d * SREv22_yCLOCK[tick]) >> 3);

		/* The envelope, and the only rounded smoother of the two. */
		mode = sre->mode;
		sre->mag_avg = (short)
			(((sre->mag_avg * SRE_ALPHA_AVG[mode]) >> 15)
			 + ((iabs(d) * SRE_BETA_AVG[mode] + 0x4000) >> 15));

		if ((short)(tick + 1) <= V22_SRE_CLOCK - 1) {
			sre->acc_x = acc_x;
			sre->acc_y = acc_y;
		} else {
			short group = sre->group;

			sre->tick = 0;
			if (sre->groups > (short)(group + 1)) {
				sre->group = (short)(group + 1);
				sre->acc_x = acc_x;
				sre->acc_y = acc_y;
			} else {
				int update = 0;
				int xq = 0, yq = 0;

				/*
				 * The squelch, with hysteresis: above 2 the
				 * PLL runs, at or below 1 the whole loop is
				 * reset to its acquisition state.
				 */
				if (sre->mag_avg > 2)
					sre->active = 1;
				if (sre->mag_avg <= 1) {
					sre->active = 0;
					sre->acquiring = 1;
					sre->err_avg = 0;
					sre->groups = 3;
					sre->settle = 0;
				}

				if (sre->active != 0) {
					sre->acc_x = acc_x;
					sre->acc_y = acc_y;
					xq = acc_x >> 16;
					yq = acc_y >> 16;
					/*
					 * In tracking mode a nearly in-phase
					 * vector carries no usable timing
					 * information, so the update is
					 * skipped rather than fed noise.
					 */
					update = mode != 2
						|| (unsigned short)(xq + 0x31)
						   > 0x62;
				}

				if (update)
					sre_update(sre, (short)yq, (short)xq);

				/*
				 * The original also stores group + 1 before
				 * the update; every path from there reaches
				 * this line, so that store is dead.
				 */
				sre->group = 0;
				sre->acc_x = 0;
				sre->acc_y = 0;
			}
		}

		/*
		 * Advance one symbol.  Ten branch-steps is one input sample
		 * at the nominal rate, so every wrap past branch 9 is another
		 * input the next output will need.
		 */
		sre->branch = (short)(sre->branch + V22_SRE_BRANCHES);
		need = 0;
		while (sre->branch > V22_SRE_BRANCHES - 1) {
			sre->branch = (short)(sre->branch
					      - V22_SRE_BRANCHES);
			need = (short)(need + 1);
		}

		out[produced++] = y;
	}

	sre->need = need;
	sre->fill = fill;
	return produced;
}
