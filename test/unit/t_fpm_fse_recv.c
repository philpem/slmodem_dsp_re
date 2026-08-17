/*
 * t_fpm_fse_recv.c -- differential test of `FPM_FSE_receive` and of the
 * function-scope static `avg_err_show.0` that gates its one debug line.
 *
 * THE OBSERVABLE IS NOT THE RETURN VALUE.  One call leaves its answer in four
 * places -- the returned symbol count, the caller's `out[]`, the two heap
 * arrays `out_i`/`out_q`, and about forty fields of the state including the
 * adapted coefficients -- and a defect in the tilt filter or in one PLL band
 * moves only the last of those.  So every trial compares the WHOLE state as
 * an object, with the five heap pointers blanked because they are five
 * different addresses on the two sides and always will be, and then compares
 * the five buffers by content.
 *
 * THE SLICER IS OURS, AND THAT IS DELIBERATE.  `cfg.decision` is a callback
 * the datapump supplies, and every slicer in the object dereferences
 * `cfg.owner` -- a V.32 decoder state this test does not have -- so driving
 * this with a real one would be testing V.32's constellation code and would
 * segfault before it tested anything.  The slicers here are pure functions of
 * their two in/out arguments, with no state of their own, so the equaliser's
 * own arithmetic is the only thing that varies between two trials.  That the
 * arguments are IN as well as out is itself one of the claims: the object
 * writes the measured angle and magnitude into them before the call, where
 * V.22's copy of this block passes both uninitialised.
 *
 * HOW THE BOUNDARY TRIALS STEER.  Three of the object's reductions are `>=`
 * where V.22's are `>`, and two readings of a `>=` differ on exactly ONE
 * input value.  A trial that never lands on it cannot tell them apart, so the
 * sweep in `sweep_derot_edge` walks `tilt_out` through all 65536 values with
 * everything else held still -- the equaliser's history is one tap, the clock
 * increment is zero and the PLL and the LMS are off, so `tilt_out` is the
 * only thing that moves and the derotation angle is a straight function of
 * it.  The counters record what the BLOB returned, not what we predicted:
 * `sep_derot_zero` counts sweep positions where the symbol came back zero,
 * and the count is 3 for the object's reading and 2 for a `>` one, because
 * one of the three positions IS the boundary.
 *
 * WHAT IS NOT OBSERVABLE, and is recorded rather than tested.  The
 * re-rotation angle at the end of a symbol reaches nothing but
 * `FPM_phasor`, and `phasor_split` reduces its argument modulo the half-turn
 * -- 0x1000 and 0x9000 produce the same index, the same fraction and the same
 * quadrant sign.  So the `>=` in THAT reduction cannot be separated from a
 * `>` by any input, and `test/mutations/fpmfserecv.json` carries it as an
 * expected survivor rather than as a hole.  The derotation `>=` earlier in
 * the symbol escapes the same fate only because the raw angle is handed to
 * the slicer, which is why the sweep's slicer returns it.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_phasor.h"

extern unsigned int ref_dsplibs_debug_level;
extern void ref_FPM_FSE_init(struct fpm_fse *state,
			     const struct fpm_fse_cfg *cfg, int fresh);
extern void ref_FPM_FSE_free(struct fpm_fse *state);
extern unsigned short ref_FPM_FSE_receive(struct fpm_fse *state,
					  const short *in, unsigned short *out,
					  unsigned short count);
extern void ref_FPM_atan(short y, short x, short *angle);

/* 20 KB apiece, so not on the stack. */
static struct fpm_fse ours, theirs;
static struct fpm_fse blank_a, blank_b;

/* Set by the tests below; every one is checked in main(). */
static int sep_derot_zero;	/* sweep positions whose symbol came back 0  */
static int sep_derot_neg;	/* sweep positions in the negative-fix band  */
static int sep_perr_wrap;	/* trials whose out[] moved when perr wrapped */
static int sep_band_hi;		/* trials whose pll_sel moved at err_hi      */
static int sep_band_lo;		/* trials whose pll_sel moved at err_lo      */
static int sep_train_edge;	/* trials whose pll_sel moved at train_sym   */
static int sep_diag_wrap;	/* trials whose diag_n moved at the wrap     */
static int sep_mse_clamp;	/* trials the saturation actually changed    */
static int sep_lms_force;	/* trials the force flag actually adapted    */
static int sep_tilt;		/* trials the tilt filter actually moved     */
static int sep_tail;		/* trials that stashed a short tail          */
static int sep_show_lines;	/* `Decoder Error` lines the blob printed    */
static int sep_rerot_reach;	/* sweep positions AT the re-rotation edge   */

/*
 * The slicer's three knobs.  All file-scope so the two sides see the same
 * function with the same behaviour; nothing here is per-side state.
 */
static short slice_perr;	/* how far the slicer moves the angle       */
static short slice_mag;		/* the ideal magnitude it reports back      */

/*
 * A slicer whose phase error is EXACTLY `slice_perr`: it subtracts the knob
 * from the angle it was handed, and `FPM_FSE_receive` forms the error as
 * `derot - angle`.  That is what makes the PLL band edges and the error
 * wrap reachable by choosing a number rather than by searching for one.
 *
 * The symbol it returns is the RAW angle it was handed, which is the only
 * path by which the derotation reduction's low bit escapes: everything else
 * that angle reaches is taken modulo the half turn.
 */
static unsigned short
fixed_slicer(struct fpm_fse *state, short *angle, short *mag)
{
	short a = *angle;

	(void)state;
	*angle = (short)(a - slice_perr);
	*mag = slice_mag;
	return (unsigned short)a;
}

/*
 * The same, but the reported magnitude is derived from the measured one, so
 * the decision error the LMS adapts on is a function of the equaliser's own
 * output rather than a constant.  Still stateless.
 */
static unsigned short
scaled_slicer(struct fpm_fse *state, short *angle, short *mag)
{
	short a = *angle;
	short m = *mag;

	(void)state;
	*angle = (short)(a - slice_perr);
	*mag = (short)(m - (m >> 2) + slice_mag);
	return (unsigned short)(a ^ m);
}

/*
 * A slicer that reports HALF the magnitude it measured, offset by the knob.
 * Half is where the decision error vanishes -- the re-rotation is at fourteen
 * fractional bits where the derotation was at thirteen -- so sweeping the
 * offset walks the error through zero in small steps, which is the only way
 * to reach a smoothed error of exactly zero and so the only way to see the
 * LMS gate refuse.
 */
static unsigned short
near_slicer(struct fpm_fse *state, short *angle, short *mag)
{
	short a = *angle;
	short m = *mag;

	(void)state;
	*angle = (short)(a - slice_perr);
	*mag = (short)((m >> 1) + slice_mag);
	return (unsigned short)a;
}

/*
 * A slicer that reads the pair the equaliser has just written, the way the
 * object's own slicers do: `out_i`/`out_q` indexed by `n_out`, which has NOT
 * been advanced yet at the moment of the call.  If that ordering were wrong
 * this reads the previous symbol and the returned value moves.
 */
static unsigned short
peeking_slicer(struct fpm_fse *state, short *angle, short *mag)
{
	short a = *angle;
	short i = state->out_i[state->n_out];
	short q = state->out_q[state->n_out];

	*angle = (short)(a - slice_perr);
	*mag = (short)(i - q);
	return (unsigned short)((i ^ (q << 1)) + (unsigned short)a);
}

/* ------------------------------------------------------------------ */

#define MAX_TAPS	16
#define MAX_IN		2048
#define MAX_OUT		512

static short drive_i[MAX_TAPS];
static short drive_q[MAX_TAPS];
static short drive_clk[8];
static short drive_k1[3];
static short drive_k2[3];
static short in_buf[MAX_IN];
static unsigned short out_a[MAX_OUT];
static unsigned short out_b[MAX_OUT];

/*
 * The five heap pointers are the sanctioned skip.  Blanking them in a copy
 * keeps diff_eq_obj's field-level report and its run coalescing, which an
 * open-coded byte loop would throw away.
 */
static void
compare_state(const char *tag, long id)
{
	blank_a = ours;
	blank_b = theirs;

	blank_a.out_i = blank_b.out_i = (short *)0;
	blank_a.out_q = blank_b.out_q = (short *)0;
	blank_a.icoeff = blank_b.icoeff = (short *)0;
	blank_a.qcoeff = blank_b.qcoeff = (short *)0;
	blank_a.hist = blank_b.hist = (short *)0;

	diff_eq_obj(tag, struct fpm_fse, &blank_a, &blank_b, id);
}

/*
 * And the buffers by content.  `out_i`/`out_q` are compared over the symbols
 * the call claims to have produced plus the two spare entries init allocates,
 * so a write past `n_out` is caught rather than skipped.
 */
static void
compare_buffers(long id)
{
	int taps = ours.cfg.taps;
	int n = ours.n_out;
	int i;

	for (i = 0; i < taps; i++) {
		diff_eq_int("icoeff[%ld]", ours.icoeff[i], theirs.icoeff[i], i);
		diff_eq_int("qcoeff[%ld]", ours.qcoeff[i], theirs.qcoeff[i], i);
		diff_eq_int("hist[%ld]", ours.hist[i], theirs.hist[i], i);
	}
	for (i = 0; i < n + 2; i++) {
		diff_eq_int("out_i[%ld]", ours.out_i[i], theirs.out_i[i], i);
		diff_eq_int("out_q[%ld]", ours.out_q[i], theirs.out_q[i], i);
	}
}

static void
fill_cfg(struct fpm_fse_cfg *cfg, short block, short interp, short taps,
	 short clk_mod, short clk_inc)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->block = block;
	cfg->interp = interp;
	cfg->icoff = drive_i;
	cfg->qcoff = drive_q;
	cfg->taps = taps;
	cfg->mu[0] = 2620;
	cfg->mu[1] = 393;
	cfg->mu[2] = 97;
	cfg->clk = drive_clk;
	cfg->clk_mod = clk_mod;
	cfg->clk_inc = clk_inc;
	cfg->train_sym = 4;
	cfg->err_hi = 6536;
	cfg->err_lo = 1638;
	cfg->pll_k1 = drive_k1;
	cfg->pll_k2 = drive_k2;
	cfg->decision = fixed_slicer;
}

static void
make_drive(void)
{
	int i;

	/*
	 * Asymmetric, different from each other, and none of them small: a
	 * transposed I/Q or a reversed walk has to change the answer.
	 */
	for (i = 0; i < MAX_TAPS; i++) {
		int v = ((i * 7919 + 1301) & 0x3fff) - 8192;

		drive_i[i] = (short)(v | 1);
		drive_q[i] = (short)(-3 * v + 5 * i + 7);
	}
	for (i = 0; i < 8; i++)
		drive_clk[i] = (short)(i * 4096 + 137);
	drive_k1[0] = 602;
	drive_k1[1] = 3050;
	drive_k1[2] = 766;
	drive_k2[0] = 0;
	drive_k2[1] = 18;
	drive_k2[2] = 1;
	for (i = 0; i < MAX_IN; i++)
		in_buf[i] = (short)(((i * 2377 + 991) & 0x7fff) - 16384);
}

/* Both sides from a zeroed state and the same configuration. */
static void
init_pair(struct fpm_fse_cfg *cfg)
{
	memset(&ours, 0, sizeof(ours));
	memset(&theirs, 0, sizeof(theirs));
	ref_FPM_FSE_init(&theirs, cfg, 1);
	FPM_FSE_init(&ours, cfg, 1);
}

static void
free_pair(void)
{
	ref_FPM_FSE_free(&theirs);
	FPM_FSE_free(&ours);
}

/*
 * One mirrored call.  The two sides are always driven with the same count in
 * the same order, which is what keeps their two private copies of
 * `avg_err_show.0` in step -- an asymmetric sequence would desynchronise them
 * silently, and no comparison in this file could see it.
 */
static void
call_pair(const short *in, unsigned short count, long id)
{
	unsigned short ra, rb;

	memset(out_a, 0xa5, sizeof(out_a));
	memset(out_b, 0xa5, sizeof(out_b));

	rb = ref_FPM_FSE_receive(&theirs, in, out_b, count);
	ra = FPM_FSE_receive(&ours, in, out_a, count);

	diff_eq_int("returned symbols (%ld)", ra, rb, id);
	if (memcmp(out_a, out_b, sizeof(out_a)) != 0) {
		unsigned i;

		for (i = 0; i < MAX_OUT; i++)
			diff_eq_int("out[%ld]", out_a[i], out_b[i], (long)i);
	}
}

/* ------------------------------------------------------------------ */

/*
 * The general trial: a real-sized filter, several calls in a row, and every
 * observable compared after each one.  `poke` is applied to both states after
 * init, so a trial can put the block into a state init never produces.
 */
static void
run_seq(const char *label, struct fpm_fse_cfg *cfg, const unsigned short *counts,
	int ncalls, void (*poke)(struct fpm_fse *))
{
	int c;

	diff_begin(label);
	init_pair(cfg);
	if (poke != 0) {
		poke(&ours);
		poke(&theirs);
	}

	for (c = 0; c < ncalls; c++) {
		unsigned short n = counts[c];
		int off = (c * 37) % (MAX_IN - 64);

		call_pair(&in_buf[off], n, c);
		compare_state("state after call", c);
		compare_buffers(c);
	}

	free_pair();
}

/* ------------------------------------------------------------------ */

/*
 * The one-tap configuration the boundary sweeps use.  With `clk_inc` zero the
 * clock phase never moves, with `taps` one the write index never moves, and
 * with the PLL and the LMS off nothing that feeds the derotation changes from
 * one call to the next -- so `tilt_out`, which the caller sets before each
 * call, is the only variable.
 */
static void
fill_one_tap(struct fpm_fse_cfg *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->block = 64;
	cfg->interp = 1;
	cfg->icoff = drive_i;
	cfg->qcoff = drive_q;
	cfg->taps = 1;
	cfg->mu[0] = 2620;
	cfg->clk = drive_clk;
	cfg->clk_mod = 1;
	cfg->clk_inc = 0;
	cfg->train_sym = 4;
	cfg->err_hi = 6536;
	cfg->err_lo = 1638;
	cfg->pll_k1 = drive_k1;
	cfg->pll_k2 = drive_k2;
	cfg->decision = fixed_slicer;
}

/*
 * Walk `tilt_out` through every value a short can hold and record what came
 * back.  `fixed_slicer` returns the raw angle, so `out[0]` IS the derotation
 * angle for that position, and the shape of the returned sequence is the
 * observable: the object's `>=` folds exactly one position onto zero that a
 * `>` would leave at 0x8000, and its `< 0` fix folds the whole negative half
 * onto values a missing fix would leave above 0x8000.
 *
 * Comparing all 65536 positions between the two sides is also the densest
 * single check in this file.
 */
static void
sweep_derot_edge(const char *label, short carrier, int count_them)
{
	struct fpm_fse_cfg cfg;
	long t;
	int zero_hits = 0;
	int high_bit = 0;
	int rerot_edge_hits = 0;

	diff_begin(label);
	drive_clk[0] = carrier;
	fill_one_tap(&cfg);
	init_pair(&cfg);

	/* Nothing but `tilt_out` may move between positions. */
	ours.pll_on = theirs.pll_on = 0;
	ours.tilt_on = theirs.tilt_on = 0;
	ours.lms_on = theirs.lms_on = 0;
	ours.lms_force = theirs.lms_force = 0;
	slice_perr = 300;
	slice_mag = 1234;
	in_buf[0] = 20011;

	for (t = 0; t < 65536; t += 1) {
		unsigned short ra, rb;

		ours.tilt_out = theirs.tilt_out = (short)t;

		rb = ref_FPM_FSE_receive(&theirs, in_buf, out_b, 1);
		ra = FPM_FSE_receive(&ours, in_buf, out_a, 1);

		if (ra != rb || out_a[0] != out_b[0])
			diff_eq_int("swept tilt_out=%ld: symbol",
				    out_a[0], out_b[0], t);
		/*
		 * AND BOTH OUTPUTS, AT EVERY POSITION RATHER THAN ONLY AT THE
		 * END.  A quarter of the sweep leaves the derotation angle
		 * negative and `FPM_phasor` indexes its sign tables with an
		 * unmasked quadrant, so this used to be uncomparable; then
		 * `out_i` alone became comparable, because the COSINE's
		 * out-of-domain sign came from the same translation unit while
		 * the SINE's came from the previous one's tail, which the
		 * instrumented build displaces.
		 *
		 * `out_q` is compared here now.  The phasor no longer reads any
		 * address it does not own: both windows are leading entries of
		 * `FPM_cos_sign_ext` and `FPM_sin_sign_ext`, so neither half
		 * depends on a link any more.  16384 of these 65536 positions
		 * are outside the phasor's designed domain and every one of
		 * them is checked, on both observables.  D392, findings 3623,
		 * 3624 and 3700.
		 */
		diff_eq_int("swept tilt_out=%ld: out_i",
			    ours.out_i[0], theirs.out_i[0], t);
		diff_eq_int("swept tilt_out=%ld: out_q",
			    ours.out_q[0], theirs.out_q[0], t);
		if (out_b[0] == 0)
			zero_hits++;
		if (out_b[0] >= 0x8000)
			high_bit++;
		/*
		 * REACHABILITY, not separation.  `fixed_slicer` returns the
		 * angle it was handed and subtracts a known constant from it,
		 * and with the PLL off the carrier is `clk[0]` exactly, so the
		 * re-rotation's own half-angle is recomputable here.  It is
		 * counted only to tell "the suite never lands on that value"
		 * apart from "it lands on it and nothing moves", which is the
		 * difference between an untested claim and a recorded one.
		 */
		if ((short)((((short)(out_b[0] - slice_perr)) + carrier) >> 1)
		    == (short)(FPM_PHASOR_CYCLE / 2))
			rerot_edge_hits++;
	}

	/*
	 * THREE positions come back zero, not one and not two.  `half` walks
	 * every value a short can hold as the sweep runs, and three of them
	 * leave the doubled angle at zero: `half` already zero, `half` at
	 * -0x4000 which the negative fix lifts to zero, and `half` at 0x4000
	 * which the upper fold takes to zero.  It is the last of those that a
	 * `>` reading leaves at 0x8000, so a `>` scores two.
	 */
	if (count_them) {
		diff_eq_int("positions returning symbol 0 (%ld)", zero_hits, 3,
			    0);
		sep_derot_zero = zero_hits;
	}

	/*
	 * EXACTLY A QUARTER of the sweep still comes back negative, and that
	 * is the object's arithmetic rather than a defect in it: the two
	 * adjustments are not a loop, so `half` below -0x4000 is lifted once
	 * and left below zero.  The count is the sharp check on both -- drop
	 * the negative fix and it is a half, run the pair in a loop and it is
	 * nothing.
	 */
	if (count_them) {
		diff_eq_int("positions returning a negative angle (%ld)",
			    high_bit, 16384, 0);
		sep_derot_neg = high_bit;
	}

	diff_eq_int("re-rotation half-angles landing on 0x4000 (%ld)",
		    rerot_edge_hits > 0, 1, 0);
	sep_rerot_reach += rerot_edge_hits;

	/*
	 * WHERE THE SWEEP ENDS, KEPT AS A RECORD RATHER THAN AS A GATE.
	 *
	 * This pair used to be load-bearing: `compare_state` and
	 * `compare_buffers` below read `out_q` and the scatter log's `.q`,
	 * which are SINE-derived, and the sine was comparable only inside
	 * 0 .. 0x7fff, so the walk had to have ended in domain for the object
	 * comparison to mean anything.  D392 is closed on both halves now --
	 * `out_q` is compared at all 65536 positions above -- so nothing here
	 * depends on the final position any more.  They stay because they are
	 * measurements of the sweep that other claims in this file are read
	 * against: exactly a quarter of it is driven out of domain, and it
	 * comes back in.  Findings 3588, 3623, 3624 and 3700.
	 */
	diff_eq_int("the sweep ends in the phasor's domain (%ld)",
		    out_b[0] < 0x8000, 1, 0);
	diff_eq_int("positions driven outside it, both outputs compared (%ld)",
		    high_bit, 16384, 0);

	compare_state("state after the sweep", 0);
	compare_buffers(0);
	drive_clk[0] = 137;
	free_pair();
}

/*
 * The phase-error wrap.  `slice_perr` IS the error, so the two edges are
 * reachable by naming them: 0x4000 is left alone and 0x4001 is pulled a whole
 * turn down, and the same on the negative side.  The observable is the
 * smoothed error the state carries, which is a compared byte of the object.
 */
static void
trial_perr_wrap(void)
{
	struct fpm_fse_cfg cfg;
	static const short edges[6] = {
		0x4000, 0x4001, 0x3fff, -16384, -16385, -16383
	};
	int e;
	short seen[6];

	diff_begin("FPM_FSE_receive: the phase-error wrap at its two edges");
	for (e = 0; e < 6; e++) {
		fill_one_tap(&cfg);
		init_pair(&cfg);
		ours.tilt_on = theirs.tilt_on = 0;
		ours.lms_on = theirs.lms_on = 0;
		slice_perr = edges[e];
		slice_mag = 1234;
		call_pair(in_buf, 1, e);
		compare_state("state after one symbol", e);
		compare_buffers(e);
		seen[e] = theirs.err_avg;
		free_pair();
	}

	/*
	 * 0x4000 and 0x4001 are one apart before the wrap and a whole turn
	 * apart after it, so the smoothed error they leave behind cannot be
	 * adjacent unless the wrap ran.  Same on the negative edge.
	 */
	if (seen[0] > 0 && seen[1] < 0)
		sep_perr_wrap++;
	if (seen[3] < 0 && seen[4] > 0)
		sep_perr_wrap++;
	diff_eq_int("the positive edge wraps (%ld)",
		    seen[0] > 0 && seen[1] < 0, 1, 0);
	diff_eq_int("the negative edge wraps (%ld)",
		    seen[3] < 0 && seen[4] > 0, 1, 0);
}

/*
 * The two PLL band edges, landed on exactly.  With the state fresh the
 * smoothed error after one symbol is `(perr * 0x666) >> 15` and nothing else,
 * so the threshold that makes the comparison an equality is computable --
 * and the test does not have to trust that arithmetic, because it sets
 * `err_hi` one either side as well and requires the three results to differ.
 */
static void
trial_band_edges(void)
{
	struct fpm_fse_cfg cfg;
	short e_at;
	int k;
	short sel[3];

	diff_begin("FPM_FSE_receive: the PLL band edges");

	/* What one symbol's smoothing leaves behind, measured not assumed. */
	fill_one_tap(&cfg);
	init_pair(&cfg);
	ours.tilt_on = theirs.tilt_on = 0;
	ours.lms_on = theirs.lms_on = 0;
	ours.cfg.train_sym = theirs.cfg.train_sym = -1;
	slice_perr = 9000;
	slice_mag = 1234;
	call_pair(in_buf, 1, 0);
	compare_state("measuring the smoothed error", 0);
	e_at = theirs.err_avg;
	if (e_at < 0)
		e_at = (short)-e_at;
	free_pair();
	diff_eq_int("the smoothed error is positive (%ld)", e_at > 0, 1, 0);

	/* err_hi at the value, one below and one above. */
	for (k = 0; k < 3; k++) {
		fill_one_tap(&cfg);
		init_pair(&cfg);
		ours.tilt_on = theirs.tilt_on = 0;
		ours.lms_on = theirs.lms_on = 0;
		ours.cfg.train_sym = theirs.cfg.train_sym = -1;
		ours.cfg.err_hi = theirs.cfg.err_hi = (short)(e_at - 1 + k);
		ours.cfg.err_lo = theirs.cfg.err_lo = -32768;
		slice_perr = 9000;
		call_pair(in_buf, 1, k);
		compare_state("err_hi edge", k);
		compare_buffers(k);
		sel[k] = theirs.pll_sel;
		free_pair();
	}
	/*
	 * At the value and below it the band is 1; one above it the band is
	 * still 0.  That the middle one is 1 is the whole of the `>=`: a `>`
	 * reading leaves it 0 and this comparison moves.
	 */
	diff_eq_int("err_hi below  -> band 1 (%ld)", sel[0], 1, 0);
	diff_eq_int("err_hi equal  -> band 1 (%ld)", sel[1], 1, 0);
	diff_eq_int("err_hi above  -> band 0 (%ld)", sel[2], 0, 0);
	if (sel[1] != sel[2])
		sep_band_hi++;

	/* And err_lo, whose edge is inclusive on the other side. */
	for (k = 0; k < 3; k++) {
		fill_one_tap(&cfg);
		init_pair(&cfg);
		ours.tilt_on = theirs.tilt_on = 0;
		ours.lms_on = theirs.lms_on = 0;
		ours.cfg.train_sym = theirs.cfg.train_sym = -1;
		ours.cfg.err_hi = theirs.cfg.err_hi = 32767;
		ours.cfg.err_lo = theirs.cfg.err_lo = (short)(e_at - 1 + k);
		slice_perr = 9000;
		call_pair(in_buf, 1, k);
		compare_state("err_lo edge", k + 3);
		compare_buffers(k + 3);
		sel[k] = theirs.pll_sel;
		free_pair();
	}
	diff_eq_int("err_lo below  -> band 0 (%ld)", sel[0], 0, 0);
	diff_eq_int("err_lo equal  -> band 2 (%ld)", sel[1], 2, 0);
	diff_eq_int("err_lo above  -> band 2 (%ld)", sel[2], 2, 0);
	if (sel[0] != sel[1])
		sep_band_lo++;
}

/*
 * Training.  `sym_count` is compared against `cfg.train_sym` twice with two
 * different relations, and the interesting value is the one where they are
 * equal: the outer test sends it down the training branch and the inner one
 * leaves the band at 1 rather than 0.
 */
static void
trial_train_edge(void)
{
	struct fpm_fse_cfg cfg;
	int k;
	short sel[3];
	short cnt[3];

	diff_begin("FPM_FSE_receive: the training boundary");
	for (k = 0; k < 3; k++) {
		fill_one_tap(&cfg);
		init_pair(&cfg);
		ours.tilt_on = theirs.tilt_on = 0;
		ours.lms_on = theirs.lms_on = 0;
		ours.cfg.train_sym = theirs.cfg.train_sym = 5;
		ours.sym_count = theirs.sym_count = (short)(4 + k);
		ours.pll_sel = theirs.pll_sel = 2;
		/*
		 * A non-zero integrator on the way in.  V.22's copy of this
		 * block zeroes it while training and this one does not, and
		 * with nothing in it to lose the two readings agree.
		 */
		ours.freq = theirs.freq = 1234567;
		slice_perr = 700;
		slice_mag = 1234;
		call_pair(in_buf, 1, k);
		compare_state("training boundary", k);
		compare_buffers(k);
		sel[k] = theirs.pll_sel;
		cnt[k] = theirs.sym_count;
		free_pair();
	}
	/*
	 * 4 < 5 leaves the band 0 and counts up; 5 == 5 is still the training
	 * branch, counts up and leaves the band 1; 6 > 5 is the tracking
	 * branch, does NOT count up, and re-selects the band from the error.
	 */
	diff_eq_int("below  -> band 0 (%ld)", sel[0], 0, 0);
	diff_eq_int("equal  -> band 1 (%ld)", sel[1], 1, 0);
	diff_eq_int("below  counts up (%ld)", cnt[0], 5, 0);
	diff_eq_int("equal  counts up (%ld)", cnt[1], 6, 0);
	diff_eq_int("above  holds     (%ld)", cnt[2], 6, 0);
	if (sel[0] != sel[1])
		sep_train_edge++;
	/*
	 * Both end at 6, so the observable is the ADVANCE and not the value:
	 * the equal case moved by one and the above case by nothing.
	 */
	if ((cnt[1] - 5) != (cnt[2] - 6))
		sep_train_edge++;
}

/*
 * The scatter log's wrap.  Full means RESET and DROP: the symbol that would
 * have gone in at 480 is not recorded and the counter goes back to zero, so
 * the entry at index 479 is the last one ever written in a pass.
 */
static void
trial_diag_wrap(void)
{
	struct fpm_fse_cfg cfg;
	int k;
	static const int seeds[4] = { 0, 478, 479, 480 };
	int after[4];

	diff_begin("FPM_FSE_receive: the scatter log's wrap");
	for (k = 0; k < 4; k++) {
		fill_one_tap(&cfg);
		init_pair(&cfg);
		ours.tilt_on = theirs.tilt_on = 0;
		ours.lms_on = theirs.lms_on = 0;
		ours.diag_n = theirs.diag_n = seeds[k];
		slice_perr = 700;
		slice_mag = 1234;
		call_pair(in_buf, 1, k);
		compare_state("diag wrap", k);
		compare_buffers(k);
		after[k] = theirs.diag_n;
		free_pair();
	}
	diff_eq_int("from 0   -> 1   (%ld)", after[0], 1, 0);
	diff_eq_int("from 478 -> 479 (%ld)", after[1], 479, 0);
	diff_eq_int("from 479 -> 480 (%ld)", after[2], 480, 0);
	diff_eq_int("from 480 -> 0   (%ld)", after[3], 0, 0);
	if (after[2] != after[3])
		sep_diag_wrap++;
}

/*
 * The mean-square error's saturation, which is an UNSIGNED test: the sum of
 * two squares is truncated to sixteen bits before it is weighted, so it can
 * come out negative, and a negative smoothed error saturates HIGH rather than
 * being clamped at zero.  Which reported magnitudes do that is not predicted
 * here -- the magnitude is swept and the two outcomes are COUNTED, so the
 * trial proves it saw both rather than asserting where the edge is.
 *
 * Note what the saturation implies downstream: the stored error is never
 * negative, so the LMS gate below can only fail on an error of exactly zero.
 */
static void
trial_mse_clamp(void)
{
	struct fpm_fse_cfg cfg;
	int k;
	int saturated = 0;
	int plain = 0;
	int zero = 0;

	diff_begin("FPM_FSE_receive: the mean-square error's saturation");
	for (k = 0; k < 64; k++) {
		fill_one_tap(&cfg);
		init_pair(&cfg);
		ours.tilt_on = theirs.tilt_on = 0;
		ours.lms_on = theirs.lms_on = 0;
		slice_perr = 700;
		slice_mag = (short)(k * 512);
		call_pair(in_buf, 1, k);
		compare_state("mse clamp", k);
		compare_buffers(k);
		if (theirs.mse == 0x7fff)
			saturated++;
		else if (theirs.mse == 0)
			zero++;
		else
			plain++;
		diff_eq_int("mse is never negative (%ld)", theirs.mse >= 0, 1,
			    k);
		free_pair();
	}
	diff_eq_int("some magnitudes saturate (%ld)", saturated > 0, 1, 0);
	diff_eq_int("some do not (%ld)", plain + zero > 0, 1, 0);
	if (saturated > 0 && plain + zero > 0)
		sep_mse_clamp++;
}

/*
 * The LMS gate is an OR, not an AND.  The saturation above means the stored
 * error is never negative, so `mse > 0` can only fail on a zero -- reachable
 * when the decision error is small enough that its square vanishes in the
 * `>> 11` and again in the 0.05 weighting, but not so small that the step the
 * update is handed rounds to nothing.
 *
 * Which magnitude does that is SEARCHED FOR rather than predicted: each is
 * run twice, once with the override clear and once with it set, and the
 * counter records the magnitudes where the two runs left DIFFERENT
 * coefficients.  That is an observed difference in a compared buffer, and it
 * is zero for a reading in which the override is an AND or is absent.
 */
static void
trial_lms_force(void)
{
	struct fpm_fse_cfg cfg;
	int k;
	int gated = 0;
	int moved_when_free = 0;

	diff_begin("FPM_FSE_receive: the LMS gate and its override");
	for (k = -200; k <= 200; k++) {
		short off_c0, on_c0;
		short off_mse;
		int f;

		off_c0 = on_c0 = 0;
		off_mse = 0;
		for (f = 0; f < 2; f++) {
			fill_one_tap(&cfg);
			cfg.decision = near_slicer;
			init_pair(&cfg);
			ours.tilt_on = theirs.tilt_on = 0;
			ours.lms_on = theirs.lms_on = 1;
			ours.lms_force = theirs.lms_force = f;
			slice_perr = 0;
			slice_mag = (short)k;
			call_pair(in_buf, 1, k * 2 + f);
			compare_state("lms gate", k * 2 + f);
			compare_buffers(k * 2 + f);
			if (f == 0) {
				off_c0 = theirs.icoeff[0];
				off_mse = theirs.mse;
			} else {
				on_c0 = theirs.icoeff[0];
			}
			free_pair();
		}
		if (off_mse == 0 && off_c0 != on_c0)
			gated++;
		if (off_mse > 0 && off_c0 != drive_i[0])
			moved_when_free++;
	}
	diff_eq_int("the override adapted where the gate refused (%ld)",
		    gated > 0, 1, 0);
	diff_eq_int("and a positive error adapts without it (%ld)",
		    moved_when_free > 0, 1, 0);
	if (gated > 0 && moved_when_free > 0)
		sep_lms_force++;
}

/*
 * The tilt filter.  Init leaves its coefficients zero, so it is inert until a
 * datapump writes them; writing them here is the only way to see the 4-tap
 * walk at all, and the delay line has to have shifted for the fourth symbol
 * to differ from the first.
 */
static void
trial_tilt(void)
{
	struct fpm_fse_cfg cfg;
	unsigned short counts[1];
	int k;
	short got[2];

	diff_begin("FPM_FSE_receive: the tilt filter");
	for (k = 0; k < 2; k++) {
		fill_one_tap(&cfg);
		init_pair(&cfg);
		ours.lms_on = theirs.lms_on = 0;
		ours.tilt_on = theirs.tilt_on = k;
		/*
		 * SMALL INTEGERS, and the object is what says so: the 4-tap
		 * sum is stored without any shift, so a coefficient of 512
		 * multiplies a phase error of 700 into 30720 and drives the
		 * derotation angle out of `FPM_phasor`'s domain within two
		 * symbols.  These four are asymmetric, of two magnitudes and
		 * of mixed sign, and the recursion through `tilt_out` settles
		 * rather than growing.
		 *
		 * A LARGE-COEFFICIENT ARM WAS TRIED HERE AND TAKEN BACK OUT.
		 * It drove the angle out of domain deliberately, and the
		 * reason it could not stay was that this trial's whole content
		 * is `compare_state` and `compare_buffers`, both of which read
		 * `out_q` and the scatter log's `.q` -- SINE-derived, and the
		 * half D392 then left open.  THAT OBSTACLE IS GONE (finding
		 * 3700) and such an arm would be admissible now; it has not
		 * been re-added, because the sweep above already drives both
		 * observables out of domain at 16384 positions and this trial
		 * is about the recursion through `tilt_out`.  Findings 3623,
		 * 3624 and 3700.
		 */
		ours.tilt_coeff[0] = theirs.tilt_coeff[0] = 2;
		ours.tilt_coeff[1] = theirs.tilt_coeff[1] = -1;
		ours.tilt_coeff[2] = theirs.tilt_coeff[2] = -1;
		ours.tilt_coeff[3] = theirs.tilt_coeff[3] = 1;
		slice_perr = 700;
		slice_mag = 1234;
		/*
		 * ONE SAMPLE PER CALL, eight times, and compared after each:
		 * the filter is recursive through its own output and its delay
		 * line is four deep, so a defect in it does not show until the
		 * fourth symbol and the run that finds it should say which
		 * symbol rather than which call.
		 */
		for (counts[0] = 0; counts[0] < 8; counts[0]++) {
			call_pair(&in_buf[counts[0]], 1, k * 8 + counts[0]);
			compare_state("tilt", k * 8 + counts[0]);
			compare_buffers(k * 8 + counts[0]);
			/*
			 * IN DOMAIN, and asserted rather than assumed.
			 * `fixed_slicer` returns the derotation angle, and
			 * this used to be a NECESSITY: past 0x7fff the phasor
			 * indexes its sign table with a negative quadrant and
			 * the sine's out-of-range word came from the previous
			 * translation unit's tail, which the instrumented
			 * build displaces, so `compare_buffers`' `out_q` was
			 * not comparable there.  It is comparable everywhere
			 * now (finding 3700).  The check stays as a statement
			 * about THIS trial: the coefficients above are chosen
			 * to keep the bias small enough that the angle stays
			 * in domain, which is what makes the recursion settle
			 * rather than run away.  D392, findings 3588, 3623,
			 * 3624 and 3700.
			 */
			diff_eq_int("derotation angle in phasor domain (%ld)",
				    out_b[0] < 0x8000, 1,
				    k * 8 + counts[0]);
		}
		got[k] = theirs.tilt_out;
		free_pair();
	}
	diff_eq_int("off leaves the output alone (%ld)", got[0], 0, 0);
	diff_eq_int("on moves it (%ld)", got[1] != 0, 1, 0);
	if (got[0] != got[1])
		sep_tilt++;
}

/*
 * The short tail, the empty call and the negative one.  `count` is unsigned
 * and the loop runs on a signed copy, so 0x8000 and above arrive as a
 * negative length: the object charges nothing against `need`, stashes
 * nothing, and returns immediately.
 */
static void
trial_tails(void)
{
	struct fpm_fse_cfg cfg;
	static const unsigned short counts[8] = {
		0, 1, 2, 7, 0x8000, 0xffff, 3, 12
	};
	int k;
	short need_before, need_after;

	diff_begin("FPM_FSE_receive: short, empty and negative counts");
	fill_cfg(&cfg, 64, 5, 8, 4, 3);
	init_pair(&cfg);
	slice_perr = 700;
	slice_mag = 1234;

	for (k = 0; k < 8; k++) {
		need_before = theirs.need;
		call_pair(in_buf, counts[k], k);
		compare_state("tail call", k);
		compare_buffers(k);
		need_after = theirs.need;
		/*
		 * A call shorter than the symbol interval leaves `need`
		 * strictly smaller than it found it and produces nothing.
		 */
		if (counts[k] != 0 && counts[k] < 0x8000
		    && theirs.n_out == 0 && need_after < need_before)
			sep_tail++;
	}
	free_pair();
}

/*
 * `avg_err_show.0`.  It is not a field of anything and no object comparison
 * can see it; the only observable is the transcript, so the transcript is
 * what this compares.  The counter is charged in SAMPLES once per call and
 * tested once per symbol, so a call long enough to cross 0x1c1f prints on its
 * first symbol and resets -- which is why three times the threshold produces
 * a bounded number of lines rather than one per symbol.
 */
static void
trial_show(void)
{
	struct fpm_fse_cfg cfg;
	int k;
	unsigned lines_ours, lines_ref;

	diff_begin("FPM_FSE_receive: the Decoder Error report");
	fill_cfg(&cfg, 4096, 3, 8, 4, 1);
	cfg.decision = scaled_slicer;
	init_pair(&cfg);
	slice_perr = 700;
	slice_mag = 4000;

	dsplibs_debug_level = 2;
	ref_dsplibs_debug_level = 2;
	dsplib_debug_capture_reset();
	dsplib_debug_capture_on = 1;

	/* 12 x 900 = 10800 samples, so the threshold is crossed at least once. */
	for (k = 0; k < 12; k++) {
		call_pair(in_buf, 900, k);
		compare_state("while reporting", k);
	}

	dsplib_debug_capture_on = 0;
	lines_ours = dsplib_debug_capture_lines(0);
	lines_ref = dsplib_debug_capture_lines(1);
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	diff_eq_int("the blob printed something (%ld)", lines_ref > 0, 1, 0);
	diff_eq_int("line count (%ld)", lines_ours, lines_ref, 0);
	diff_eq_int("transcript (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, 0);
	sep_show_lines = (int)lines_ref;
	free_pair();
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	struct fpm_fse_cfg cfg;
	static const unsigned short seq[10] = {
		3, 5, 1, 16, 4, 0, 9, 2, 30, 7
	};
	int rc = 0;

	make_drive();
	slice_perr = 700;
	slice_mag = 1234;

	/* The bulk trial: a real-sized filter driven for a while. */
	fill_cfg(&cfg, 144, 3, 8, 4, 1);
	run_seq("FPM_FSE_receive: 8 taps, 3 samples a symbol", &cfg, seq, 10,
		0);
	rc |= diff_end();

	/* The same, with the slicer that reads the pair just written. */
	fill_cfg(&cfg, 144, 3, 12, 4, 1);
	cfg.decision = peeking_slicer;
	run_seq("FPM_FSE_receive: 12 taps, the slicer reads out_i/out_q", &cfg,
		seq, 10, 0);
	rc |= diff_end();

	/* And one whose clock walks a longer table with a bigger step. */
	fill_cfg(&cfg, 144, 2, 16, 8, 5);
	cfg.decision = scaled_slicer;
	slice_perr = -2200;
	run_seq("FPM_FSE_receive: 16 taps, clk_mod 8, clk_inc 5", &cfg, seq,
		10, 0);
	rc |= diff_end();
	slice_perr = 700;

	/*
	 * Twice.  The first pass counts, and its small carrier keeps the
	 * RE-rotation's own reduction below its fold; the second raises the
	 * carrier until `(angle + carrier) >> 1` passes 0x4000, which is the
	 * only way that second fold is reached at all.  Without it the
	 * mutation that makes it exclusive survives for want of an input
	 * rather than for want of an observable, and the two look the same
	 * from the report.
	 */
	sweep_derot_edge("FPM_FSE_receive: the derotation reduction, swept",
			 137, 1);
	rc |= diff_end();

	sweep_derot_edge("FPM_FSE_receive: the same sweep over a high carrier",
			 20000, 0);
	rc |= diff_end();

	trial_perr_wrap();
	rc |= diff_end();

	trial_band_edges();
	rc |= diff_end();

	trial_train_edge();
	rc |= diff_end();

	trial_diag_wrap();
	rc |= diff_end();

	trial_mse_clamp();
	rc |= diff_end();

	trial_lms_force();
	rc |= diff_end();

	trial_tilt();
	rc |= diff_end();

	trial_tails();
	rc |= diff_end();

	trial_show();
	rc |= diff_end();

	/*
	 * Every counter above is an OBSERVED difference in something the test
	 * compares -- a returned symbol, a state byte, a transcript line --
	 * and not a branch taken or a loop bound.  A trial set that stopped
	 * reaching one of these would still pass every comparison above and
	 * would prove nothing, so the run fails here instead.
	 */
	diff_begin("FPM_FSE_receive: what the trials separated");
	diff_eq_int("derotation: sweep positions returning 0 (%ld)",
		    sep_derot_zero, 3, 0);
	diff_eq_int("derotation: positions left negative (%ld)",
		    sep_derot_neg, 16384, 0);
	diff_eq_int("phase-error wrap edges seen (%ld)", sep_perr_wrap, 2, 0);
	diff_eq_int("err_hi edge moved the band (%ld)", sep_band_hi, 1, 0);
	diff_eq_int("err_lo edge moved the band (%ld)", sep_band_lo, 1, 0);
	diff_eq_int("training edge separations (%ld)", sep_train_edge, 2, 0);
	diff_eq_int("scatter log wrap separated (%ld)", sep_diag_wrap, 1, 0);
	diff_eq_int("saturation separated (%ld)", sep_mse_clamp, 1, 0);
	diff_eq_int("lms_force separated (%ld)", sep_lms_force, 1, 0);
	diff_eq_int("tilt filter separated (%ld)", sep_tilt, 1, 0);
	diff_eq_int("short tails stashed (%ld)", sep_tail > 0, 1, 0);
	diff_eq_int("Decoder Error lines seen (%ld)", sep_show_lines > 0, 1,
		    0);
	/*
	 * Reachability rather than separation, and the reason the survivor in
	 * `test/mutations/fpmfserecv.json` is recorded as one: the suite DOES
	 * land on the re-rotation's boundary, and nothing it compares moves.
	 */
	diff_eq_int("re-rotation boundary reached (%ld)", sep_rerot_reach > 0,
		    1, 0);
	rc |= diff_end();

	return rc;
}
