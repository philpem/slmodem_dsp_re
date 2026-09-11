/*
 * fpm_sre.c -- Fixed Point Modem: generic symbol-timing recovery.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_SRE_recover  .text 0x0a9e90, 2286 bytes
 *   FPM_SRE_init     .text 0x0aa7c0,  574 bytes
 *   FPM_SRE_free     .text 0x0aa780,   57 bytes
 *
 * See include/dsplib/fpm_sre.h for the block and for what differs from
 * `v22_sre.c`, which is the author's own specialisation of it.  This file is
 * about the arithmetic, and every note below is load-bearing.
 *
 *   - THE TWO SMOOTHERS SHIFT DIFFERENTLY AND THAT IS FORCED.  `err_avg`'s
 *     15/16 decay is `sar` and `mag_avg`'s is `shr`, in otherwise identical
 *     three-instruction sequences (`shl $4`, `sub`, shift).  The compiler was
 *     not free to choose that, so the source has an unsigned intermediate in
 *     one and not the other.  It cannot change an answer -- `mag_avg` is a
 *     clamped magnitude and never goes negative -- but it is what the object
 *     encodes.
 *   - EVERY SECTION OF THE DISCRIMINANT TRUNCATES TO 16 BITS BEFORE THE NEXT
 *     ONE READS IT.  Eleven coefficients, five separate `sar` steps, and a
 *     `(short)` cast after each.  Folding two of them into one expression
 *     changes the answer; the same warning is on `v22_sre.c`'s copy.
 *   - THE FILTER ACCUMULATOR IS 32-BIT and is truncated exactly once, where
 *     `y` is formed -- and `y` is then DOUBLED, which V.22 does not do.
 *     Everything downstream, including the output sample, sees the doubled
 *     truncation.
 *   - THE DISCRIMINANT IS NOT HALVED.  V.22's is `(c_new - clk[5]) >> 1`;
 *     this one is the difference itself.
 *   - `iabs` IS TAKEN OF THE STORED 16-BIT ERROR, not of the int it was
 *     computed in, and the gear-shift thresholds are compared against that.
 *   - NOTHING SATURATES except the loop-filter step, clamped to
 *     [-18432, +20480], and `mag_avg`, clamped at 0x7fff.  Both bounds of the
 *     first are the object's and they are not symmetric.
 */

#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/sysdep.h"

void
FPM_SRE_init(struct fpm_sre *sre, const struct fpm_sre_cfg *cfg, int fresh)
{
	short i;

	/*
	 * The reuse path, and it is the opposite way round from FPM_FSE_init:
	 * a re-init whose existing buffers are already big enough keeps them.
	 * `fresh` is then set so the single allocation block below covers both
	 * "never initialised" and "just released".
	 */
	if (!fresh && sre->cfg.coeffs < cfg->coeffs) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Reallocating FPM_SRE buffers");
		sysdep_free(sre->clk);
		sysdep_free(sre->hist);
		sysdep_free(sre->coeff);
		sysdep_free(sre->rms_buf);
		fresh = 1;
	}

	sre->cfg = *cfg;

	sre->active = 0;
	sre->acquiring = 1;
	sre->mode = 0;
	sre->pll_acc = 0;
	sre->err_avg = 0;
	sre->mag_avg = 0;
	sre->adapt = 1;
	sre->taps = (short)(sre->cfg.coeffs / FPM_SRE_BRANCHES);
	sre->fill = 0;
	sre->acc_x = 0;
	sre->acc_y = 0;
	sre->frac = 0;
	sre->branch = 0;
	sre->groups = sre->cfg.groups_acq;
	sre->settle = 0;
	sre->group = 0;
	sre->tick = 0;
	sre->need = 1;
	sre->rms_on = 1;
	sre->rms_idx = 0;

	if (fresh) {
		/*
		 * The object narrows the coefficient and RMS byte counts before
		 * allocation.  Keep that arithmetic for differential reconstruction;
		 * the normal build must allocate the full configured ranges.
		 */
#ifdef DSPLIB_REPRODUCE_BUGS
		sre->coeff = sysdep_malloc((short)(2 * sre->cfg.coeffs));
		sre->hist = sysdep_malloc((short)(2 * sre->taps));
		sre->clk = sysdep_malloc(FPM_SRE_CLOCK * 2);
		sre->rms_buf = sysdep_malloc((short)(2 * sre->cfg.rms_len));
#else
		unsigned coeffs = sre->cfg.coeffs;
		unsigned taps = sre->taps;
		unsigned rms_len = sre->cfg.rms_len;

		sre->coeff = sysdep_malloc(2U * coeffs);
		/* `taps` is bounded by signed `coeffs / FPM_SRE_BRANCHES`. */
		sre->hist = sysdep_malloc(2U * taps);
		sre->clk = sysdep_malloc(FPM_SRE_CLOCK * 2);
		sre->rms_buf = sysdep_malloc(2U * rms_len);
#endif
	}

	/*
	 * The prototype goes in unpermuted: branch selection is a stride of
	 * FPM_SRE_BRANCHES in the dot product, not a layout.
	 */
	for (i = 0; i < sre->cfg.coeffs; i++)
		sre->coeff[i] = sre->cfg.proto[i];

	for (i = 0; i < sre->taps; i++)
		sre->hist[i] = 0;

	for (i = 0; i <= FPM_SRE_CLOCK - 1; i++)
		sre->clk[i] = 0;

	for (i = 0; i < sre->cfg.rms_len; i++)
		sre->rms_buf[i] = 0;

	sre->ppm_count = 0;
	sre->ppm_acc = 0;
	sre->ppm_offset = 0;
	sre->ppm_n = 1;
	sre->ppm_slip = 0;
	sre->ppm_first = 1;
}

/*
 * Release the four buffers, in the SAME ORDER init's realloc path releases
 * them -- clk, hist, coeff, rms_buf, which is neither the order they are
 * allocated in nor the order they are declared in.  It is reproduced because
 * the object encodes it and not because anything can see it: no allocation
 * follows, so a permutation of these four is unobservable.  Recorded as a
 * surviving mutation with that derivation.
 *
 * The pointers are NOT cleared afterwards, so a second call is a double free
 * and a re-init with `fresh` zero reads four dangling pointers.  The object
 * leaves both to the caller.
 */
void
FPM_SRE_free(struct fpm_sre *sre)
{
	sysdep_free(sre->clk);
	sysdep_free(sre->hist);
	sysdep_free(sre->coeff);
	sysdep_free(sre->rms_buf);
}

static int
iabs(int v)
{
	return v < 0 ? -v : v;
}

/*
 * Take `n` samples into the circular history, and into the level gate's ring
 * as well while that is running.
 *
 * The object holds this twice -- once on the path that has enough input for
 * another output and once on the path that has not -- with identical
 * structure, which is what an inlined static helper gives.  It is written
 * once here for that reason and not to tidy it up.
 *
 * The gate's copy RE-READS the sample from `in[-1]` rather than reusing the
 * value just stored, because the store through `hist` may alias the input;
 * the object reloads it and so does this.
 */
static short
sre_ingest(struct fpm_sre *sre, short *hist, short taps, short fill,
	   const short *in, short n)
{
	short i;

	for (i = (short)(n - 1); i != -1; i = (short)(i - 1)) {
		fill = (short)(fill + 1 < taps ? fill + 1 : 0);
		hist[fill] = *in++;

		if (sre->rms_on != 0) {
			short k = sre->rms_idx;

			sre->rms_idx = (short)(k + 1);
			sre->rms_buf[k] = in[-1];
			if (sre->rms_idx >= sre->cfg.rms_len)
				sre->rms_idx = 0;
		}
	}

	return fill;
}

/*
 * One output sample: `taps` terms, newest first, over the circular history
 * against every tenth coefficient starting at `branch`.
 *
 * Two runs rather than a modulo -- hist[fill] down to hist[0], then
 * hist[taps-1] down to hist[fill+1] -- and the coefficient pointer walks
 * straight through both, so the split is in the history's indexing only.
 */
static short
sre_filter(const struct fpm_sre *sre, short taps, short fill)
{
	const short *h = sre->hist + fill;
	const short *c = sre->coeff + sre->branch;
	int acc = 0;
	short i;

	for (i = fill; i >= 0; i--) {
		acc += *h * *c;
		h--;
		c += FPM_SRE_BRANCHES;
	}

	h = sre->hist + taps - 1;
	for (i = (short)(taps - 1); i > fill; i--) {
		acc += *h * *c;
		h--;
		c += FPM_SRE_BRANCHES;
	}

	return (short)(2 * (short)(acc >> 15));
}

/*
 * The timing discriminant: three cascaded second-order sections over `clk`,
 * driven by the filter output.  Eleven coefficients, all from the caller's
 * table, where V.22's specialisation has eleven literals -- four of which are
 * zero there, which is why its first section subtracts one term and this one
 * subtracts two.
 */
static short
sre_discriminant(const short *k, short *clk, short y)
{
	int drive = (int)y << 9;
	short a_new, b_new, c_new;
	short u, p, q, r;
	short d;

	a_new = (short)((drive - k[0] * clk[0] - k[1] * clk[1]) >> 14);
	b_new = (short)((drive - k[2] * clk[2] - k[3] * clk[3]) >> 14);

	u = (short)(a_new - ((k[4] * clk[0]) >> 14));
	p = (short)((k[5] * clk[0]) >> 14);
	q = (short)(b_new - ((k[6] * clk[2]) >> 14));
	r = (short)((q * u + (short)((k[7] * clk[2]) >> 14) * p) >> 11);

	c_new = (short)((k[8] * r + k[9] * clk[4] - k[10] * clk[5]) >> 14);

	/*
	 * The slope is measured against clk[5]'s OLD value and clk[5] then
	 * takes clk[4]'s, so the order of these lines matters.
	 */
	d = (short)(c_new - clk[5]);
	clk[1] = clk[0];
	clk[0] = a_new;
	clk[3] = clk[2];
	clk[2] = b_new;
	clk[5] = clk[4];
	clk[4] = c_new;

	return d;
}

/*
 * Rebuild every coefficient from the prototype, interpolating between
 * adjacent taps.  Mode 2 only.
 *
 * A single flat walk, because the branches are a stride rather than a layout
 * -- and it is what makes `cfg.proto` need one entry more than `cfg.coeffs`.
 *
 * THE FIRST STORE IS DEAD AND IS REPRODUCED.  The object stores the w0 term
 * to co[i] and then stores the sum over it; the second term's operand is the
 * UNTRUNCATED first, so the two stores are what an `int` temp with two
 * assignments to the same slot gives, and the reload GCC would otherwise need
 * is suppressed only because `co` and `proto` may alias.  `v22_sre.c` met the
 * same pair and declined to reproduce it; the values agree either way.
 */
static void
sre_interpolate(struct fpm_sre *sre, short w0, short w1)
{
	const short *proto = sre->cfg.proto;
	short *co = sre->coeff;
	short i;

	for (i = 0; i < sre->cfg.coeffs; i++) {
		int v = (proto[i] * w0) >> 15;

		co[i] = (short)v;
		v += (proto[i + 1] * w1) >> 15;
		co[i] = (short)v;
	}
}

/*
 * One PLL update: turn the accumulated correlation vector into a phase error,
 * smooth it, decide whether to change gear, and move (branch, frac).
 */
static void
sre_update(struct fpm_sre *sre, short yq, short xq)
{
	short angle, e, m, step;
	int q, rem, frac, branch;

	FPM_atan(yq, xq, &angle);

	/* Fold the upper half turn down, so the error is signed about zero. */
	if (angle > 0x4000)
		angle = (short)(angle - 0x8000);

	/*
	 * The loop gain is the clock length.  See the note on `clock_len` in
	 * the header: it really is the same field the tick comparison reads.
	 */
	e = (short)((sre->cfg.clock_len * (int)angle) >> 3);

	sre->err_avg = (short)(((sre->err_avg * 15) >> 4) + (e >> 4));

	/* The caller can freeze the phase while still letting it be measured. */
	if (sre->adapt == 0)
		return;

	m = sre->mode;
	if (sre->settle > sre->cfg.settle) {
		/*
		 * Gear shift.  The magnitude is of the STORED error, i.e. of
		 * the 16-bit truncation, and both thresholds are compared
		 * after a further `>> 3`.
		 */
		int a = iabs((int)sre->err_avg);

		if ((sre->cfg.err_hi >> 3) <= a && m == 2) {
			sre->mode = 1;
			sre->groups = sre->cfg.groups_acq;
			sre->pll_acc = (short)
				((sre->cfg.acc_down * sre->pll_acc) >> 15);
			m = 1;
		}
		if ((sre->cfg.err_lo >> 3) >= a && m == 1) {
			sre->mode = 2;
			sre->groups = sre->cfg.groups_trk;
			sre->acquiring = 0;
			sre->pll_acc = (short)
				((sre->cfg.acc_up * sre->pll_acc) >> 12);
			m = 2;
		}
	} else {
		/*
		 * Settling.  The integrator is held at zero throughout, so the
		 * loop is first order here whatever pll_k2[0] holds.
		 */
		short settle = sre->settle;

		sre->pll_acc = 0;
		sre->settle = (short)(settle + 1);
		m = (short)(settle < sre->cfg.settle ? 0 : 1);
		sre->mode = m;
	}

	{
		short p;
		int acc = sre->pll_acc;

		p = (short)(((sre->cfg.pll_k1[m] * e) >> 12) + acc);
		sre->pll_acc = (short)
			(((sre->cfg.pll_k2[m] * e + 0x800) >> 12) + acc);

		/*
		 * Ten sub-branch steps per input sample, and the sign is
		 * inverted: a positive error retards the sampling instant.
		 */
		step = (short)((-(FPM_SRE_BRANCHES * p)) >> 4);
	}

	/* Deliberately asymmetric, exactly as in V.22. */
	if (step > 0x5000)
		step = 0x5000;
	else if (step < -18432)
		step = -18432;

	/*
	 * Split the step into whole branches and a remainder.  For a negative
	 * step the quotient is the floor plus one, so the remainder is
	 * negative and the borrow below carries it.
	 */
	q = step >> 11;
	if (step < 0)
		q = q + 1;

	branch = sre->branch + q;
	rem = step - (q << 11);
	frac = sre->frac + rem;

	if ((short)frac < 0) {
		branch = branch - 1;
		frac = frac + FPM_SRE_FRAC_ONE;
	}
	sre->frac = (short)frac;
	sre->branch = (short)branch;

	if (sre->frac > FPM_SRE_FRAC_ONE - 1) {
		sre->frac = (short)(sre->frac - FPM_SRE_FRAC_ONE);
		sre->branch = (short)(sre->branch + 1);
	}

	{
		short w1 = (short)(sre->frac << 4);
		short w0 = (short)(0x7fff - w1);

		/* Only tracking mode interpolates between prototype taps. */
		if (m == 2)
			sre_interpolate(sre, w0, w1);
	}
}

/*
 * Close a timing-offset interval: fold this interval's slip into the running
 * total, republish the mean, and start the next one.
 *
 * Reached only when a call drains its input exactly -- a call that ends short
 * takes the early return and the meter does not tick.
 */
static void
sre_meter(struct fpm_sre *sre)
{
	int sum = sre->ppm_slip * sre->ppm_scale + sre->ppm_acc;

	sre->ppm_offset = (short)((short)sum / sre->ppm_n);

	sre->ppm_count = 0;
	sre->ppm_slip = 0;

	if (sre->ppm_n < sre->ppm_n_max) {
		if (sre->ppm_first != 0) {
			sre->ppm_acc = 0;
			sre->ppm_n = 1;
			sre->ppm_first = 0;
		} else {
			sre->ppm_n = (short)(sre->ppm_n + 1);
			sre->ppm_acc = (short)sum;
		}
	} else {
		/*
		 * The window is full.  Restart the average FROM ITS OWN MEAN
		 * rather than from zero, so a long-running measurement decays
		 * instead of freezing.
		 */
		sre->ppm_n = 1;
		if (sre->ppm_first != 0) {
			sre->ppm_acc = 0;
			sre->ppm_n = 1;
			sre->ppm_first = 0;
		} else {
			sre->ppm_n = 2;
			sre->ppm_acc = sre->ppm_offset;
		}
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("TimingVxx: Timing Offset [ppm] = %d\n",
				     (int)sre->ppm_offset);
}

unsigned short
FPM_SRE_recover(struct fpm_sre *sre, const short *in, short *out, short count)
{
	short *hist = sre->hist;
	short taps = sre->taps;
	short fill = sre->fill;
	short need = sre->need;
	short produced = 0;
	short remaining = count;

	while (remaining != 0) {
		short y, d, tick, mode, extra;
		int acc_x, acc_y;

		/*
		 * Not enough input left for another output.  Bank what there
		 * is, carry the shortfall in `need`, and return -- and note
		 * that this path does NOT tick the timing meter.
		 */
		if (remaining < need) {
			need = (short)(need - remaining);
			fill = sre_ingest(sre, hist, taps, fill, in, remaining);
			sre->fill = fill;
			sre->need = need;
			return (unsigned short)produced;
		}

		fill = sre_ingest(sre, hist, taps, fill, in, need);
		in += need;
		remaining = (short)(remaining - need);

		y = sre_filter(sre, taps, fill);
		d = sre_discriminant(sre->cfg.disc, sre->clk, y);

		/*
		 * The level gate.  Until the collected block is loud enough
		 * the discriminant is discarded and the loop runs on zero;
		 * the first block that clears the threshold turns the gate
		 * off for good.
		 */
		if (sre->rms_on != 0) {
			if (sre->cfg.rms_min < FPM_rms(sre->rms_buf,
						       sre->cfg.rms_len))
				sre->rms_on = 0;
			else
				d = 0;
		}

		/*
		 * Correlate against one group's worth of clock reference.
		 * Note the y term SUBTRACTS.
		 */
		tick = sre->tick;
		sre->tick = (short)(tick + 1);
		acc_x = sre->acc_x + ((d * sre->cfg.xclock[tick]) >> 3);
		acc_y = sre->acc_y - ((d * sre->cfg.yclock[tick]) >> 3);

		/*
		 * The envelope.  15/16 with no rounding, and the decay's
		 * shift is LOGICAL where err_avg's is arithmetic -- see the
		 * note at the top of this file.  It cannot change an answer
		 * and it is what the object encodes.
		 */
		mode = sre->mode;
		{
			unsigned int decay = (unsigned int)(sre->mag_avg * 15)
					     >> 4;
			int mag = (short)decay + (iabs(d) >> 1);

			if (mag > 0x7fff)
				mag = 0x7fff;
			sre->mag_avg = (short)mag;
		}

		if (sre->cfg.clock_len > (short)(tick + 1)) {
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

				/* The squelch, with hysteresis. */
				if (sre->mag_avg >= sre->cfg.mag_hi)
					sre->active = 1;
				if (sre->mag_avg <= sre->cfg.mag_lo)
					sre->active = 0;

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

				/*
				 * The original also stores group + 1 before
				 * the update; every path from there reaches
				 * the line below, so that store is dead.
				 */
				if (update)
					sre_update(sre, (short)yq, (short)xq);

				sre->group = 0;
				sre->acc_x = 0;
				sre->acc_y = 0;
			}
		}

		/*
		 * Advance one symbol.  Ten branch-steps is one input sample at
		 * the nominal rate, so every wrap past branch 9 is another
		 * input the next output will need -- and the count of them is
		 * both `need` and what the timing meter measures.
		 */
		extra = 0;
		sre->branch = (short)(sre->branch + FPM_SRE_BRANCHES);
		while (sre->branch > FPM_SRE_BRANCHES - 1) {
			sre->branch = (short)(sre->branch - FPM_SRE_BRANCHES);
			extra = (short)(extra + 1);
		}
		need = extra;

		out[produced++] = y;

		if (extra == 2)
			sre->ppm_slip = (short)(sre->ppm_slip + 1);
		else if (extra == 0)
			sre->ppm_slip = (short)(sre->ppm_slip - 1);
	}

	sre->ppm_count = (short)(sre->ppm_count + sre->ppm_step);
	if (sre->ppm_count == sre->ppm_period)
		sre_meter(sre);

	sre->fill = fill;
	sre->need = need;
	return (unsigned short)produced;
}

/*
 * The state's layout is a byte count from a build where pointers are four
 * bytes, so these are compiled only under that ABI.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define SRE_ASSERT_OFF(field, off) \
	typedef char fpm_sre_off_##field[ \
		((int)__builtin_offsetof(struct fpm_sre, field) == (off)) \
			? 1 : -1]

SRE_ASSERT_OFF(mode, 0x38);
SRE_ASSERT_OFF(pll_acc, 0x3a);
SRE_ASSERT_OFF(err_avg, 0x3c);
SRE_ASSERT_OFF(mag_avg, 0x3e);
SRE_ASSERT_OFF(active, 0x40);
SRE_ASSERT_OFF(acquiring, 0x44);
SRE_ASSERT_OFF(adapt, 0x48);
SRE_ASSERT_OFF(taps, 0x4c);
SRE_ASSERT_OFF(fill, 0x4e);
SRE_ASSERT_OFF(coeff, 0x50);
SRE_ASSERT_OFF(hist, 0x54);
SRE_ASSERT_OFF(clk, 0x58);
SRE_ASSERT_OFF(acc_x, 0x5c);
SRE_ASSERT_OFF(acc_y, 0x60);
SRE_ASSERT_OFF(frac, 0x64);
SRE_ASSERT_OFF(branch, 0x66);
SRE_ASSERT_OFF(groups, 0x68);
SRE_ASSERT_OFF(settle, 0x6a);
SRE_ASSERT_OFF(group, 0x6c);
SRE_ASSERT_OFF(tick, 0x6e);
SRE_ASSERT_OFF(need, 0x70);
SRE_ASSERT_OFF(rms_buf, 0x74);
SRE_ASSERT_OFF(rms_on, 0x78);
SRE_ASSERT_OFF(rms_idx, 0x7a);
SRE_ASSERT_OFF(ppm_step, 0x7c);
SRE_ASSERT_OFF(ppm_count, 0x7e);
SRE_ASSERT_OFF(ppm_acc, 0x80);
SRE_ASSERT_OFF(ppm_offset, 0x82);
SRE_ASSERT_OFF(ppm_n, 0x84);
SRE_ASSERT_OFF(ppm_slip, 0x86);
SRE_ASSERT_OFF(ppm_scale, 0x88);
SRE_ASSERT_OFF(ppm_period, 0x8a);
SRE_ASSERT_OFF(ppm_n_max, 0x8c);
SRE_ASSERT_OFF(ppm_first, 0x8e);

typedef char fpm_sre_cfg_size[(sizeof(struct fpm_sre_cfg) == 0x38) ? 1 : -1];
typedef char fpm_sre_size[(sizeof(struct fpm_sre) == 0x90) ? 1 : -1];

#endif /* 32-bit */
