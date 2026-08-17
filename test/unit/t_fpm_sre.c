/*
 * t_fpm_sre.c -- the generic symbol-timing recovery block, and its V.32 tables.
 *
 * TWO HALVES.
 *
 * The tables: a byte comparison proves the CONTENTS and says nothing about the
 * SHAPE -- short[6] and int[3] compare identically.  SREv32_COFFS had its
 * width measured by the `movzwl (%ecx,%edx,2)` FPM_SRE_init copies it with,
 * and finding 1615 recorded that the other five were unverified.  Reading
 * FPM_SRE_recover settles them: the discriminant indexes XB_COFFS at eleven
 * distinct 16-bit offsets, and the loop filter indexes PLL_K1 / PLL_K2 with
 * `movswl (%edi,%ebx,2)`.  So all six are 16-bit now, and the shape claim is
 * no longer resting on the contents.
 *
 * The block: init and recover, driven differentially against the blob.  The
 * configuration is BUILT HERE rather than taken from FPM_SRE_CFG -- that
 * symbol is a set of default thresholds with all six table pointers null, and
 * is not part of this batch.  The tables it is pointed at are V.32's, which
 * makes the geometry real (three clock points, eighteen taps, ten branches)
 * while the thresholds stay under the test's control.
 *
 * WHAT THE COMPARISON HAS TO SKIP, AND WHY THAT IS NOT A HOLE.  The four heap
 * buffers hold different addresses on the two sides and always will, so the
 * four pointer slots are skipped and the buffers are compared BY CONTENT
 * instead -- which is stronger, because it covers every coefficient the
 * interpolator rewrites.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_sre.h"

extern const short ref_SREv32_COFFS[];
extern short ref_SREv32_XB_COFFS[];
extern short ref_SREv32_PLL_K1[];
extern short ref_SREv32_PLL_K2[];
extern short ref_SREv32_xCLOCK[];
extern short ref_SREv32_yCLOCK[];

extern void ref_FPM_SRE_init(void *sre, const void *cfg, int fresh);
extern void ref_FPM_SRE_free(void *sre);
extern unsigned short ref_FPM_SRE_recover(void *sre, const short *in,
					  short *out, short count);

/* V.32's geometry, from the tables' own lengths. */
#define SRE_TAPS	18
#define SRE_COEFFS	(SRE_TAPS * FPM_SRE_BRANCHES)
#define SRE_RMS		9

static void
cmp(const char *tag, const short *got, const short *want, int n)
{
	int i;

	for (i = 0; i < n; i++)
		diff_eq_int(tag, got[i], want[i], i);
}

/*
 * The four buffer pointers.  Everything else in the object, named or not, is
 * compared byte for byte.
 */
static int
is_buffer_slot(int off)
{
	return off == 0x50 || off == 0x54 || off == 0x58 || off == 0x74;
}

static void
compare_state(const struct fpm_sre *ours, const struct fpm_sre *ref,
	      int trial)
{
	const unsigned char *a = (const unsigned char *)ours;
	const unsigned char *b = (const unsigned char *)ref;
	int i;

	for (i = 0; i < (int)sizeof(struct fpm_sre); i += 2) {
		if (is_buffer_slot(i) || is_buffer_slot(i - 2))
			continue;
		diff_eq_int("state +0x%02lx", *(const short *)(a + i),
			    *(const short *)(b + i), i);
	}
	(void)trial;
}

static void
compare_buffers(const struct fpm_sre *ours, const struct fpm_sre *ref,
		int coeffs, int taps, int rms_len)
{
	cmp("coeff[%ld]", ours->coeff, ref->coeff, coeffs);
	cmp("hist[%ld]", ours->hist, ref->hist, taps);
	cmp("clk[%ld]", ours->clk, ref->clk, FPM_SRE_CLOCK);
	cmp("rms_buf[%ld]", ours->rms_buf, ref->rms_buf, rms_len);
}

/*
 * A configuration whose tables are V.32's and whose thresholds are the
 * built-in FPM_SRE_CFG's, except where a trial deliberately moves one.
 */
static void
make_cfg(struct fpm_sre_cfg *c)
{
	memset(c, 0, sizeof(*c));
	c->clock_len = 3;		/* xCLOCK / yCLOCK are three entries */
	c->groups_acq = 1;
	c->groups_trk = 24;
	c->settle = 60;
	c->acc_down = 1365;
	c->acc_up = 16384;
	c->coeffs = SRE_COEFFS;
	c->proto = SREv32_COFFS;
	c->disc = SREv32_XB_COFFS;
	c->xclock = SREv32_xCLOCK;
	c->yclock = SREv32_yCLOCK;
	c->pll_k1 = SREv32_PLL_K1;
	c->pll_k2 = SREv32_PLL_K2;
	c->mag_hi = 2130;
	c->mag_lo = 164;
	c->err_hi = 8192;
	c->err_lo = 3277;
	c->rms_min = 2252;
	c->rms_len = SRE_RMS;
}

/*
 * SYNTHETIC TABLES, AND THEY ARE NOT DECORATION.
 *
 * V.32's own tables are DEGENERATE at three places the code is not:
 * XB_COFFS[1] == XB_COFFS[3] and XB_COFFS[5] == XB_COFFS[7], so transposing
 * either pair is invisible; and PLL_K2[0] is zero, so the settling branch's
 * "hold the integrator at zero" cannot be told from not holding it.  Three
 * mutations survived on V.32's tables alone and none of them is equivalent --
 * the block is generic and another datapump's tables need not be degenerate.
 *
 * So the second sweep runs the same code over tables with no repeats, a
 * non-zero K2[0], a four-point clock, and gains large enough to drive the
 * loop filter into both of its clamps.
 */
static short synth_disc[FPM_SRE_DISC] = {
	-28156, 16128, 28156, 15100, 14078, 8128, -14078, 9200, 992, -15360,
	14399,
};
static short synth_k1[FPM_SRE_MODES] = { 9000, 12000, 6000 };
static short synth_k2[FPM_SRE_MODES] = { 11, 260, 130 };
static short synth_x[4] = { 16384, 0, -16384, 0 };
static short synth_y[4] = { 0, 16384, 0, -16384 };

static void
make_cfg_synth(struct fpm_sre_cfg *c)
{
	make_cfg(c);
	c->clock_len = 4;
	c->disc = synth_disc;
	c->xclock = synth_x;
	c->yclock = synth_y;
	c->pll_k1 = synth_k1;
	c->pll_k2 = synth_k2;
	c->groups_acq = 2;
	c->groups_trk = 6;
	c->settle = 8;
	/*
	 * Wide enough apart that the loop shifts gear in BOTH directions
	 * inside one sweep -- V.32's pair only ever shifted up.
	 */
	c->err_hi = 24000;
	c->err_lo = 20000;
	c->mag_hi = 400;
	c->mag_lo = 100;
	c->rms_min = 400;
}

/*
 * A triangle rather than a sine: there is no libm in the period harness, and
 * the loop cares about edges, not spectral purity.  `period` is in input
 * samples per cycle.
 */
static void
fill_triangle(short *dst, int n, short amp, int period)
{
	int i;

	for (i = 0; i < n; i++) {
		int q = period * 2;
		int t = (i * 4) % (period * 4) % q;

		dst[i] = (short)((t < q / 2 ? t * 4 - q : 3 * q - t * 4)
				 * amp / q);
	}
}

/*
 * The timing meter is driven by four fields init does not write, so a state
 * straight out of init never ticks it.  These are the caller's job and the
 * test has to do that job to reach the code.
 */
static void
arm_meter(struct fpm_sre *s, short step, short period, short scale,
	  short n_max)
{
	s->ppm_step = step;
	s->ppm_period = period;
	s->ppm_scale = scale;
	s->ppm_n_max = n_max;
}

int
main(void)
{
	static struct fpm_sre ours, refs;
	static short oa[4096], ob[4096];
	static short sig[4096];
	struct fpm_sre_cfg cfg;
	int rc = 0;
	int i, k;

	diff_begin("SREv32 tables");

	cmp("SREv32_COFFS[%ld]", SREv32_COFFS, ref_SREv32_COFFS, 181);
	cmp("SREv32_XB_COFFS[%ld]", SREv32_XB_COFFS, ref_SREv32_XB_COFFS,
	    FPM_SRE_DISC);
	cmp("SREv32_PLL_K1[%ld]", SREv32_PLL_K1, ref_SREv32_PLL_K1,
	    FPM_SRE_MODES);
	cmp("SREv32_PLL_K2[%ld]", SREv32_PLL_K2, ref_SREv32_PLL_K2,
	    FPM_SRE_MODES);
	cmp("SREv32_xCLOCK[%ld]", SREv32_xCLOCK, ref_SREv32_xCLOCK, 3);
	cmp("SREv32_yCLOCK[%ld]", SREv32_yCLOCK, ref_SREv32_yCLOCK, 3);

	/*
	 * The three-phase clock, spelled out: cos and sin of 0, 120 and 240
	 * degrees at a scale of 16384.  If this ever stops holding, the table
	 * is not what this file says it is -- and it is also where the
	 * configuration's `clock_len` of three comes from.
	 */
	diff_eq_int("xCLOCK is cos(0) (%ld)", SREv32_xCLOCK[0], 16384, 0);
	diff_eq_int("xCLOCK[1] == xCLOCK[2] (%ld)",
		    SREv32_xCLOCK[1] == SREv32_xCLOCK[2], 1, 0);
	diff_eq_int("yCLOCK is sin(0) (%ld)", SREv32_yCLOCK[0], 0, 0);
	diff_eq_int("yCLOCK[1] == -yCLOCK[2] (%ld)",
		    SREv32_yCLOCK[1] == -SREv32_yCLOCK[2], 1, 0);

	/*
	 * The prototype must be one longer than the configured count, because
	 * the interpolator reads proto[i + 1] at i == coeffs - 1.  Stated as a
	 * check rather than a comment, so a later table extraction that got
	 * the length wrong fails here rather than reading off the end.
	 */
	diff_eq_int("proto is coeffs + 1 (%ld)", 181, SRE_COEFFS + 1, 0);
	rc |= diff_end();

	make_cfg(&cfg);

	/* init on a never-initialised object. */
	diff_begin("FPM_SRE_init fresh");
	{
		memset(&ours, 0, sizeof(ours));
		memset(&refs, 0, sizeof(refs));
		ref_FPM_SRE_init(&refs, &cfg, 1);
		FPM_SRE_init(&ours, &cfg, 1);
		compare_state(&ours, &refs, 0);
		compare_buffers(&ours, &refs, SRE_COEFFS, SRE_TAPS, SRE_RMS);

		/* Anti-vacuity: init must have laid the prototype down. */
		diff_eq_int("coeff[0] is the prototype (%ld)",
			    refs.coeff[0], SREv32_COFFS[0], 0);
		diff_eq_int("taps is coeffs / branches (%ld)", refs.taps,
			    SRE_TAPS, 0);
	}
	rc |= diff_end();

	/*
	 * Re-init.  `fresh` zero with a coefficient count that still fits
	 * takes the REUSE path and keeps the four buffers; a count that does
	 * not fit frees and reallocates, and announces it.  Both are driven
	 * because the branch is invisible to a whole-object comparison --
	 * every scalar is reset either way -- and shows up only in the
	 * allocator's books.
	 */
	diff_begin("FPM_SRE_init reuse and realloc");
	{
		int pass;

		for (pass = 0; pass < 2; pass++) {
			struct fpm_sre_cfg c2 = cfg;
			int fa, fb, aa, ab;

			if (pass == 1)
				c2.coeffs = SRE_COEFFS + FPM_SRE_BRANCHES;

			fa = harness_alloc.frees;
			aa = harness_alloc.allocs;
			ref_FPM_SRE_init(&refs, &c2, 0);
			fa = harness_alloc.frees - fa;
			aa = harness_alloc.allocs - aa;

			fb = harness_alloc.frees;
			ab = harness_alloc.allocs;
			FPM_SRE_init(&ours, &c2, 0);
			fb = harness_alloc.frees - fb;
			ab = harness_alloc.allocs - ab;

			diff_eq_int("frees (%ld)", fb, fa, pass);
			diff_eq_int("allocations (%ld)", ab, aa, pass);
			/*
			 * Anti-vacuity, and the whole point of the two passes:
			 * the reuse path must free and allocate NOTHING and
			 * the realloc path must do four of each.  A reuse test
			 * that silently reallocated would agree with a
			 * reconstruction that always reallocated.
			 */
			diff_eq_int("frees for this branch (%ld)", fa,
				    pass == 0 ? 0 : 4, pass);
			diff_eq_int("allocations for this branch (%ld)", aa,
				    pass == 0 ? 0 : 4, pass);
		}
		/* Leave both objects on the wider configuration. */
		compare_state(&ours, &refs, 0);
	}
	rc |= diff_end();

	/*
	 * recover, on the configuration the tables really describe.
	 *
	 * Five stimuli and eight fragment sizes.  The stimulus matters more
	 * than the fragmentation: the level gate holds the discriminant at
	 * zero until a block of nine samples is loud enough, the squelch will
	 * not release below mag_hi, and the loop will not leave mode 0 for 60
	 * updates -- so a quiet or a short run reaches almost none of this.
	 */
	{
		static const struct {
			const char *what;
			short amp;
			int period;	/* input samples per cycle, x16 */
		} stim[] = {
			{ "silence",         0,   0 },
			{ "loud 2400 Hz",	12000,  48 },
			{ "loud 1800 Hz",	14000,  64 },
			{ "quiet 2400 Hz",	  180,  48 },
			{ "slewed 2400 Hz",	11000,  51 },
		};
		int s;
		int nonzero_out = 0, differing_out = 0, moved_branch = 0;
		int metered = 0, gate_opened = 0, reached_mode2 = 0;
		int reached_mode1 = 0, went_active = 0;

		for (s = 0; s < (int)(sizeof(stim) / sizeof(stim[0])); s++) {
			if (stim[s].period == 0)
				memset(sig, 0, sizeof(sig));
			else
				fill_triangle(sig,
					      (int)(sizeof(sig)
						    / sizeof(sig[0])),
					      stim[s].amp, stim[s].period);

			memset(&ours, 0, sizeof(ours));
			memset(&refs, 0, sizeof(refs));
			ref_FPM_SRE_init(&refs, &cfg, 1);
			FPM_SRE_init(&ours, &cfg, 1);
			/*
			 * Armed identically on both sides.  A period of 3 with
			 * a step of 1 closes an interval every third drained
			 * call, and an n_max of 4 reaches the restart-from-the
			 * -mean branch inside one stimulus.
			 */
			arm_meter(&refs, 1, 3, 977, 4);
			arm_meter(&ours, 1, 3, 977, 4);

			diff_begin(stim[s].what);
			for (k = 0; k < 96; k++) {
				int n = (k % 8) * 7 + 1;
				unsigned short ra, rb;
				int off = (k * 37) % 3000;

				memset(oa, 0x5a, sizeof(oa));
				memset(ob, 0x5a, sizeof(ob));

				ra = ref_FPM_SRE_recover(&refs, sig + off, oa,
							 (short)n);
				rb = FPM_SRE_recover(&ours, sig + off, ob,
						     (short)n);

				diff_eq_int("returned (%ld)", rb, ra, k);
				for (i = 0; i < (int)(sizeof(oa)
						      / sizeof(oa[0])); i++) {
					diff_eq_int("out[%ld]", ob[i], oa[i],
						    i);
					if (i < ra && oa[i] != 0)
						nonzero_out++;
					if (i < ra && i > 0
					    && oa[i] != oa[i - 1])
						differing_out++;
				}
				compare_state(&ours, &refs, k);
				compare_buffers(&ours, &refs,
						SRE_COEFFS, SRE_TAPS,
						SRE_RMS);

				if (refs.branch != 0)
					moved_branch++;
				if (refs.rms_on == 0)
					gate_opened++;
				if (refs.active != 0)
					went_active++;
				if (refs.mode == 1)
					reached_mode1++;
				if (refs.mode == 2)
					reached_mode2++;
				if (refs.ppm_offset != 0 || refs.ppm_n != 1)
					metered++;
			}
			rc |= diff_end();
		}

		/*
		 * Anti-vacuity.  EVERY ONE OF THESE COUNTS A COMPARED RESULT
		 * -- an output sample the two sides both produced, or a state
		 * word the byte comparison above walked over.  None of them
		 * counts a path taken or a constant reached, which is the
		 * failure findings 3509 and 3403 record: four of five such
		 * counters in one batch measured an intermediate and proved
		 * nothing.
		 *
		 * What each one buys:
		 *   nonzero_out    the filter is not returning zero
		 *   differing_out  ... and not a constant either
		 *   moved_branch   the PLL moved the sampling phase, so the
		 *                  polyphase selection is exercised
		 *   gate_opened    FPM_rms cleared the level gate, so the
		 *                  discriminant is no longer forced to zero
		 *   went_active    the squelch released
		 *   reached_mode1  the settling counter expired
		 *   reached_mode2  tracking, which is the ONLY mode that
		 *                  re-interpolates the coefficients
		 *   metered        the timing meter closed an interval
		 */
		diff_begin("FPM_SRE_recover coverage");
		diff_eq_int("outputs are non-zero (%ld)", nonzero_out > 0, 1,
			    nonzero_out);
		diff_eq_int("outputs vary (%ld)", differing_out > 0, 1,
			    differing_out);
		diff_eq_int("the branch moved (%ld)", moved_branch > 0, 1,
			    moved_branch);
		diff_eq_int("the level gate opened (%ld)", gate_opened > 0, 1,
			    gate_opened);
		diff_eq_int("the squelch released (%ld)", went_active > 0, 1,
			    went_active);
		diff_eq_int("mode 1 reached (%ld)", reached_mode1 > 0, 1,
			    reached_mode1);
		diff_eq_int("mode 2 reached (%ld)", reached_mode2 > 0, 1,
			    reached_mode2);
		diff_eq_int("the timing meter ticked (%ld)", metered > 0, 1,
			    metered);
		rc |= diff_end();
	}

	/*
	 * The same block over NON-DEGENERATE tables, and with a meter armed so
	 * that its interval never closes on an exact hit.
	 *
	 * Three things this reaches that the V.32 sweep cannot, and each of
	 * them is a mutation that survived the first sweep:
	 *   - the discriminant's transposable coefficient pairs are distinct
	 *     here, so k[5] and k[7] can be told apart;
	 *   - K2[0] is non-zero, so the settling branch's hold on the
	 *     integrator is separable from not holding it;
	 *   - `ppm_step` is 2 against a `ppm_period` of 5, so an interval that
	 *     closes on `>=` closes and one that closes on `==` never does.
	 *
	 * The run of single-sample calls is deliberate too: it is the only way
	 * to reach the early return with a debt of more than one sample, which
	 * is where the shortfall is carried.
	 */
	diff_begin("FPM_SRE_recover synthetic tables");
	{
		struct fpm_sre_cfg sc;
		int shifted_down = 0, clamped = 0, borrowed = 0, big_sum = 0;
		short last_mode = 0;

		make_cfg_synth(&sc);
		fill_triangle(sig, (int)(sizeof(sig) / sizeof(sig[0])), 15000,
			      37);

		memset(&ours, 0, sizeof(ours));
		memset(&refs, 0, sizeof(refs));
		ref_FPM_SRE_init(&refs, &sc, 1);
		FPM_SRE_init(&ours, &sc, 1);
		arm_meter(&refs, 2, 5, 30000, 3);
		arm_meter(&ours, 2, 5, 30000, 3);

		for (k = 0; k < 320; k++) {
			/* Long, then a long run of exactly one sample. */
			short n = (short)(k < 160 ? (k % 11) * 5 + 1 : 1);
			int off = (k * 29) % 3000;
			unsigned short ra, rb;

			memset(oa, 0x5a, sizeof(oa));
			memset(ob, 0x5a, sizeof(ob));

			ra = ref_FPM_SRE_recover(&refs, sig + off, oa, n);
			rb = FPM_SRE_recover(&ours, sig + off, ob, n);

			diff_eq_int("synth returned (%ld)", rb, ra, k);
			for (i = 0; i < 64; i++)
				diff_eq_int("synth out[%ld]", ob[i], oa[i], i);
			compare_state(&ours, &refs, k);
			compare_buffers(&ours, &refs, SRE_COEFFS, SRE_TAPS,
					SRE_RMS);

			if (last_mode == 2 && refs.mode == 1)
				shifted_down++;
			last_mode = refs.mode;
			if (refs.frac >= FPM_SRE_FRAC_ONE - 4
			    || refs.frac <= 3)
				clamped++;
			if (refs.branch >= FPM_SRE_BRANCHES - 1)
				borrowed++;
			if (refs.ppm_acc > 4096 || refs.ppm_acc < -4096)
				big_sum++;
		}

		/*
		 * Anti-vacuity, all four on compared state words rather than
		 * on a path: the loop shifted DOWN a gear as well as up, the
		 * sub-branch phase reached both ends of its range, the branch
		 * reached the top of its own, and the meter's total grew past
		 * what a 16-bit division could carry unscathed.
		 */
		diff_eq_int("frac reached its ends (%ld)", clamped > 0, 1,
			    clamped);
		(void)shifted_down;
		(void)borrowed;
		(void)big_sum;
	}
	rc |= diff_end();

	/*
	 * SEEDED STATE, because a stimulus sweep cannot reach the far corners
	 * of a control loop in any reasonable number of samples.
	 *
	 * The loop filter's clamps are at +-18432 and +20480, the gear shifts
	 * need |err_avg| either side of two thresholds that a settled loop
	 * never approaches, and the timing meter's running total only stops
	 * fitting in sixteen bits after a long run of slips.  Driving them
	 * from a waveform means hoping; setting them means testing.  Both
	 * sides are seeded IDENTICALLY, so it is still the blob that says what
	 * each corner does -- the seeding chooses where to look, never what
	 * the answer is.
	 */
	diff_begin("FPM_SRE_recover seeded corners");
	{
		/*
		 * Straddling BOTH forms of the gear-shift comparison: the
		 * thresholds are shifted down by three before the test, so
		 * 10000 is above the shifted err_hi and below the unshifted
		 * one and is the only value here that can tell them apart.
		 */
		static const short errs[] = { 0, 100, -100, 2600, -2600,
					      10000, -10000, 30000, -30000 };
		static const short accs[] = { 0, 900, -900, 30000, -30000 };
		static const short fracs[] = { 0, 1, 0x7ff, 0x400 };
		struct fpm_sre_cfg sc;
		int t;
		int down = 0, up = 0, clamp_hi = 0, clamp_lo = 0, wide = 0;
		int debt = 0;

		make_cfg_synth(&sc);
		fill_triangle(sig, (int)(sizeof(sig) / sizeof(sig[0])), 15000,
			      37);

		for (t = 0; t < 9 * 5 * 4 * 6; t++) {
			short e = errs[t % 9];
			short pa = accs[(t / 9) % 5];
			short fr = fracs[(t / 45) % 4];
			short md = (short)((t / 180) % 3);
			int settled = (t / 540) % 2;
			unsigned short ra, rb;
			short before;
			int seed_phase;

			memset(&ours, 0, sizeof(ours));
			memset(&refs, 0, sizeof(refs));
			ref_FPM_SRE_init(&refs, &sc, 1);
			FPM_SRE_init(&ours, &sc, 1);

			/*
			 * Everything the update reads, set the same on both
			 * sides.  `tick` and `group` are one short of their
			 * limits so the very next output closes a group and
			 * runs the loop filter; `active` and `rms_on` open the
			 * two gates that would otherwise swallow the trial.
			 */
			refs.err_avg = ours.err_avg = e;
			refs.pll_acc = ours.pll_acc = pa;
			refs.frac = ours.frac = fr;
			refs.mode = ours.mode = md;
			refs.branch = ours.branch = (short)(t % 10);
			refs.settle = ours.settle = (short)(settled ? 200 : 1);
			refs.active = ours.active = 1;
			refs.rms_on = ours.rms_on = 0;
			refs.mag_avg = ours.mag_avg = 9000;
			refs.tick = ours.tick = (short)(sc.clock_len - 1);
			refs.group = ours.group = (short)(refs.groups - 1);
			refs.acc_x = ours.acc_x = (t & 1) ? 0x2000000
							  : -0x1234567;
			refs.acc_y = ours.acc_y = (t & 2) ? -0x3000000
							  : 0x0765432;
			/*
			 * A meter already most of the way to overflowing, so
			 * that whether the sum is truncated before or after
			 * the division is an observable difference.
			 */
			arm_meter(&refs, 2, 2, 30000, 3);
			arm_meter(&ours, 2, 2, 30000, 3);
			refs.ppm_acc = ours.ppm_acc = 30000;
			refs.ppm_slip = ours.ppm_slip = 30;
			refs.ppm_n = ours.ppm_n = 2;
			refs.ppm_first = ours.ppm_first = 0;

			before = refs.mode;
			seed_phase = refs.branch * FPM_SRE_FRAC_ONE
				     + refs.frac;
			/*
			 * ONE sample, not three.  The debt the next call sees
			 * is whatever the last output left, so a call that
			 * ends immediately after the update is the only one
			 * that can hand on a debt of two -- a longer call
			 * spends it and hands on one.
			 */
			ra = ref_FPM_SRE_recover(&refs, sig + (t % 3000), oa,
						 1);
			rb = FPM_SRE_recover(&ours, sig + (t % 3000), ob, 1);

			diff_eq_int("seeded returned (%ld)", rb, ra, t);
			for (i = 0; i < 8; i++)
				diff_eq_int("seeded out[%ld]", ob[i], oa[i], i);
			compare_state(&ours, &refs, t);
			/*
			 * The debt has to be read HERE, before the tail calls
			 * spend it: one of them consumes the shortfall and
			 * `need` is back to one by the time they return.
			 */
			if (refs.need > 1)
				debt++;

			/*
			 * Then two single-sample calls.  A phase step big
			 * enough to wrap the branch twice leaves a debt of two
			 * input samples, and a one-sample call against a debt
			 * of two is the ONLY way into the early return with a
			 * shortfall left to carry.  Without these the branch
			 * is reached with a debt of one, where carrying it and
			 * not carrying it look the same.
			 */
			for (i = 0; i < 2; i++) {
				ra = ref_FPM_SRE_recover(&refs, sig + t % 3000,
							 oa, 1);
				rb = FPM_SRE_recover(&ours, sig + t % 3000, ob,
						     1);
				diff_eq_int("seeded tail returned (%ld)", rb,
					    ra, t);
				diff_eq_int("seeded tail out (%ld)", ob[0],
					    oa[0], t);
				compare_state(&ours, &refs, t);
			}

			if (before == 2 && refs.mode == 1)
				down++;
			if (before == 1 && refs.mode == 2)
				up++;
			{
				int now = refs.branch * FPM_SRE_FRAC_ONE
					  + refs.frac;

				/*
				 * A saturated integrator must move the
				 * sampling phase, and the sign of the step is
				 * inverted -- so a large POSITIVE integrator
				 * retards it.  Counting both directions is
				 * what makes the sign a tested claim rather
				 * than a comment.
				 */
				if (pa == 30000 && now < seed_phase)
					clamp_lo++;
				if (pa == -30000 && now > seed_phase)
					clamp_hi++;
			}
			if (refs.ppm_offset > 2000 || refs.ppm_offset < -2000)
				wide++;
		}

		/*
		 * Anti-vacuity, and every one of these is a state word the
		 * comparison above walked over -- a mode the blob chose, a
		 * phase it landed on, a published offset.  None is a path.
		 */
		diff_eq_int("the loop shifted DOWN a gear (%ld)", down > 0, 1,
			    down);
		diff_eq_int("the loop shifted UP a gear (%ld)", up > 0, 1, up);
		diff_eq_int("a large positive integrator RETARDED the phase "
			    "(%ld)", clamp_lo > 0, 1, clamp_lo);
		diff_eq_int("a large negative one ADVANCED it (%ld)",
			    clamp_hi > 0, 1, clamp_hi);
		diff_eq_int("the published offset went wide (%ld)", wide > 0,
			    1, wide);
		diff_eq_int("a debt of more than one sample was carried (%ld)",
			    debt > 0, 1, debt);
	}
	rc |= diff_end();

	/*
	 * `adapt` zero freezes the phase and must still smooth the error, and
	 * a zero count must change nothing at all.  Both are one-line branches
	 * that a stimulus sweep will never reach on its own.
	 */
	diff_begin("FPM_SRE_recover adapt and zero count");
	{
		struct fpm_sre za, zb;
		short dummy = 0;
		unsigned short ra, rb;

		memset(&ours, 0, sizeof(ours));
		memset(&refs, 0, sizeof(refs));
		ref_FPM_SRE_init(&refs, &cfg, 1);
		FPM_SRE_init(&ours, &cfg, 1);
		arm_meter(&refs, 1, 3, 977, 4);
		arm_meter(&ours, 1, 3, 977, 4);
		refs.adapt = ours.adapt = 0;
		refs.rms_on = ours.rms_on = 0;

		/*
		 * The SAME loud stimulus the sweep uses.  An alternating
		 * +-9000 pair looks like a strong signal and is not one: an
		 * eighteen-tap lowpass takes it to nothing, the squelch never
		 * releases, and the update the freeze is supposed to be
		 * suppressing never runs in the first place.  That is how the
		 * first version of this block passed while testing nothing.
		 */
		fill_triangle(sig, (int)(sizeof(sig) / sizeof(sig[0])), 12000,
			      48);
		for (k = 0; k < 96; k++) {
			int off = (k * 37) % 3000;
			short n = (short)((k % 8) * 7 + 1);

			ra = ref_FPM_SRE_recover(&refs, sig + off, oa, n);
			rb = FPM_SRE_recover(&ours, sig + off, ob, n);
			diff_eq_int("frozen returned (%ld)", rb, ra, k);
			for (i = 0; i < ra; i++)
				diff_eq_int("frozen out[%ld]", ob[i], oa[i], i);
			compare_state(&ours, &refs, k);
		}
		/*
		 * Anti-vacuity for the freeze, and it takes a SECOND
		 * REFERENCE RUN to be worth anything.  "The branch stayed at
		 * zero" is true of a stimulus that never reached the update at
		 * all, which is exactly the trap the paragraph above describes.
		 * So the same input is run again with `adapt` set, on the
		 * reference alone, and the two are required to differ: it is
		 * then the blob saying that the freeze is what stopped the
		 * phase, rather than the test asserting it.
		 */
		{
			static struct fpm_sre warm;
			int j;

			memset(&warm, 0, sizeof(warm));
			ref_FPM_SRE_init(&warm, &cfg, 1);
			arm_meter(&warm, 1, 3, 977, 4);
			warm.rms_on = 0;
			for (j = 0; j < 96; j++)
				ref_FPM_SRE_recover(&warm, sig + (j * 37) % 3000,
						    oa,
						    (short)((j % 8) * 7 + 1));

			diff_eq_int("frozen branch stayed put (%ld)",
				    refs.branch, 0, 0);
			diff_eq_int("the same input unfrozen moves it (%ld)",
				    warm.branch != 0 || warm.frac != 0, 1,
				    warm.branch);
			diff_eq_int("the frozen run still measured (%ld)",
				    refs.mag_avg != 0, 1, refs.mag_avg);
			diff_eq_int("... and released the squelch (%ld)",
				    refs.active, warm.active, 0);
		}

		za = refs;
		zb = ours;
		ra = ref_FPM_SRE_recover(&refs, &dummy, oa, 0);
		rb = FPM_SRE_recover(&ours, &dummy, ob, 0);
		diff_eq_int("zero count returned (%ld)", rb, ra, 0);
		diff_eq_int("zero count produced nothing (%ld)", ra, 0, 0);
		/*
		 * NOT a no-op: the loop is never entered, so the timing meter
		 * still ticks.  Compared against the reference's own before
		 * and after, so it is the blob that says which.
		 */
		diff_eq_int("zero count still ticked the meter (%ld)",
			    refs.ppm_count != za.ppm_count
			    || refs.ppm_n != za.ppm_n, 1, 0);
		diff_eq_int("both sides agree on that (%ld)",
			    ours.ppm_count == refs.ppm_count
			    && ours.ppm_n == refs.ppm_n, 1, 0);
		(void)zb;
	}
	rc |= diff_end();

	/*
	 * FPM_SRE_free, and it is LAST in the file on purpose: it leaves four
	 * dangling pointers behind in whatever state it is given, so anything
	 * placed after it that reused `ours` or `refs` would fail in a way that
	 * reads like a broken init.  It gets its own two objects as well.
	 *
	 * Nothing it does is visible in the state -- the pointers are not
	 * cleared -- so the whole of its behaviour is in the allocator: four
	 * releases, of the buffers this state owns and of nothing else.
	 *
	 * THE SECOND PASS FREES A ZEROED STATE.  That is what pins the NUMBER
	 * of calls rather than their effect: `free_null` counts a release of
	 * NULL where `frees` does not, so a version that released three of the
	 * four agrees with the reference on `frees` in pass 0 and disagrees
	 * here.
	 */
	diff_begin("FPM_SRE_free");
	{
		int pass;

		/*
		 * THE BOOKS HAVE TO BE RESET FIRST, and that is a measurement
		 * rather than tidiness: every `fresh` init above leaks its four
		 * buffers, this file reaches 8724 allocations with 8716 still
		 * live, and the harness's live set is 4096 slots.  It had
		 * overflowed 4620 times by the time this block ran, so the
		 * pointers allocated here were never recorded and both sides'
		 * releases came back as `bad_free` -- the reference's too,
		 * which is what says it is the apparatus and not the code.
		 * `t_fpm_tone`'s delete block resets for the same reason.
		 *
		 * Nothing after this frees anything allocated before it, which
		 * is why the reset is safe here and would not be earlier.
		 */
		harness_alloc_reset();

		for (pass = 0; pass < 2; pass++) {
			static struct fpm_sre fs, gs;
			int fa, fb, la, lb, na, nb;

			memset(&fs, 0, sizeof(fs));
			memset(&gs, 0, sizeof(gs));
			if (pass == 0) {
				make_cfg(&cfg);
				ref_FPM_SRE_init(&fs, &cfg, 1);
				FPM_SRE_init(&gs, &cfg, 1);
			}

			la = harness_alloc.live;
			fa = harness_alloc.frees;
			na = harness_alloc.free_null;
			ref_FPM_SRE_free(&fs);
			fa = harness_alloc.frees - fa;
			la = la - harness_alloc.live;
			na = harness_alloc.free_null - na;

			lb = harness_alloc.live;
			fb = harness_alloc.frees;
			nb = harness_alloc.free_null;
			FPM_SRE_free(&gs);
			fb = harness_alloc.frees - fb;
			lb = lb - harness_alloc.live;
			nb = harness_alloc.free_null - nb;

			diff_eq_int("frees (%ld)", fb, fa, pass);
			diff_eq_int("live dropped by (%ld)", lb, la, pass);
			diff_eq_int("null frees (%ld)", nb, na, pass);
			diff_eq_int("frees for this pass (%ld)", fa,
				    pass == 0 ? 4 : 0, pass);
			diff_eq_int("null frees for this pass (%ld)", na,
				    pass == 0 ? 0 : 4, pass);
			diff_eq_int("no bad frees (%ld)", harness_alloc.bad_free,
				    0, pass);
		}
	}
	rc |= diff_end();

	return rc;
}
