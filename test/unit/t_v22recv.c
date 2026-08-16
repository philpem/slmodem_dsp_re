/*
 * t_v22recv.c -- differential test of `V22_FSE_receive`, the V.22 equaliser's
 * per-block receive loop.
 *
 * The whole state object is compared after EVERY call, together with all seven
 * heap buffers entry by entry, the symbols written through `out`, and the
 * quadrant the slicer keeps outside the block.  A difference anywhere in the
 * chain -- one tap, one phase unit -- reaches `out_i` on the next symbol and
 * never goes away, so the comparison is the real gate and everything below is
 * about making sure the run actually SEPARATES this reading from the plausible
 * wrong ones.
 *
 * FINDING 3052 IS THE TRAP, and this function has more places to fall into it
 * than most: it is 1,885 bytes of fixed-point arithmetic in which most wrong
 * readings differ from the right one by one bit on some inputs and by nothing
 * at all on the rest.  Two have already bitten this file -- `V22_FSE_init`
 * reverses a symmetric prototype, and the slicers accumulate a squared
 * distance in sixteen bits -- so every counter below names the wrong reading
 * it kills, is computed from the REFERENCE side's own data, and is asserted
 * non-zero in `main`.  A run in which one of them is zero has not tested what
 * it claims to have tested, whatever the comparisons said.
 *
 * WHY THE COUNTERS CAN BE COMPUTED AT ALL.  One run drives exactly one symbol
 * per call -- 1 sample, then 6 at a time -- which makes every per-symbol
 * intermediate a difference between two observable states of the reference
 * object.  The two smoothing counters go further and ask the question the
 * other way round: given the reference's `err_avg` before and after, is there
 * ANY input value under which the single-shift reading would have produced
 * what the reference produced?  Where there is none, that reading is dead.
 *
 * WHAT IS NOT TESTED, deliberately.  `out_i` and `out_q` are fourteen entries
 * and nothing in the function bounds `n_out` against them, so a call asking
 * for more than fourteen symbols overruns both heap buffers on both sides and
 * would be measuring two allocators.  Every chunk here is at most 40 samples,
 * which is at most 7 symbols, and `n_out <= 14` is asserted on every call.
 */

#include <limits.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22tab.h"

extern void ref_V22_FSE_init(struct v22_fse *state,
			     const struct v22_fse_cfg *cfg, int fresh);
extern void ref_V22_FSE_free(struct v22_fse *state);
extern unsigned short ref_V22_FSE_receive(struct v22_fse *state,
					  const short *in, unsigned short *out,
					  short count);
extern unsigned short ref_FSEv22_decision12(struct v22_fse *state,
					    short *angle, short *mag);
extern unsigned short ref_FSEv22_decision24(struct v22_fse *state,
					    short *angle, short *mag);

/* A marker no coefficient, sample or symbol of this object can be. */
#define MARK 0x5ead

#define NIN	2400		/* 400 symbols at six samples each        */
#define NOUT	64		/* far more than any one call may produce */

/* The smoother's weights and the FIR's gain, spelled again here so the
 * counters are not computed from the same #defines the source uses. */
#define W_OLD	0x799a
#define W_NEW	0x666

/* The slow component's amplitude; see make_input and sep_fir_trunc. */
#define SLOW_AMP	4800

static short input[NIN];
static short drive_i[V22_FSE_TAPS];
static short drive_q[V22_FSE_TAPS];
static struct v22_fse_cfg drive_cfg;
static unsigned short out_a[NOUT], out_b[NOUT];

/*
 * The counters, every one of them from the reference side.  `main` refuses to
 * report a pass while any is zero -- except `sep_acc_overflow`, which must be
 * zero: the test must not be resting on a wrapped accumulator, because that is
 * undefined behaviour in C and the two sides agreeing on it would be luck.
 */
static long sep_window_clamp;	/* symbols with fewer than 49 history entries */
static long sep_window_slide;	/* symbols with a full window                 */
static long sep_hist_shift;	/* calls that shifted the history down        */
static long sep_shift_edge;	/* calls where hist_n + need == 98 exactly    */
static long sep_fir_trunc;	/* symbols where (short)(acc >> 15) truncates */
static long sep_acc_overflow;	/* symbols where the accumulator wrapped      */
static long sep_smooth_err;	/* symbols separating err_avg's split shift   */
static long sep_smooth_mse;	/* the same for mse                           */
static long sep_iq_differ;	/* symbols where out_i != out_q               */
static long sep_coeff_iq;	/* taps where the I and Q prototypes differ   */
static long sep_clk_seen[V22_CRR_CLK_STEPS];
static long sep_pll_band[3];
static long sep_train_edge;	/* the symbol that ends training              */
static long sep_train_held;	/* symbols after sym_count stopped            */
static long sep_mse_open;	/* symbols where the LMS gate was open        */
static long sep_mse_shut;	/* symbols where it was shut                  */
static long sep_short_block;	/* calls that stashed a partial interval      */
static long sep_pll_ran, sep_pll_skipped;
static long sep_lms_ran, sep_lms_skipped;
static long sep_tail_checked;	/* calls whose out_i tail was proved intact   */

/*
 * The driving prototype.  Asymmetric, and the Q array is not the I array:
 * both matter here and not only in `V22_FSE_init`, because the FIR pairs tap
 * k with window entry k and a reversed reading of EITHER would agree with the
 * right one on a symmetric prototype.  Large and mostly one sign, so the
 * 49-tap accumulator reaches past a short and the truncation before the output
 * gain is exercised -- see `sep_fir_trunc`.
 */
static void
make_drive(void)
{
	int i;

	for (i = 0; i < V22_FSE_TAPS; i++) {
		drive_i[i] = (short)(9000 + 220 * i + ((i * 2357) & 511));
		drive_q[i] = (short)(19560 - 220 * i + ((i * 1013) & 511));
		if (drive_i[i] != drive_q[i])
			sep_coeff_iq++;
	}

	drive_cfg.icoff = drive_i;
	drive_cfg.qcoff = drive_q;
}

/*
 * The input.  Two components whose periods are coprime with each other and
 * with the six-sample symbol interval, so no two symbols see the same window,
 * plus a small pseudo-random term.  Integer only: a float generator would be
 * two different signals under the two compilers this tree builds with.
 */
static void
make_input(void)
{
	static const short w11[11] = {
		0, 4000, 6500, 7000, 5000, 1500, -2500, -5800,
		-7000, -6000, -3000,
	};
	static const short w7[7] = { 0, 2600, 3000, 900, -1800, -3000, -1700 };
	unsigned lfsr = 0x2C1D5Bu;
	int i;

	for (i = 0; i < NIN; i++) {
		/*
		 * A square wave two windows long, which is what drives the
		 * 49-tap accumulator past a short: the other two components sum
		 * to nothing over a window (49 is seven periods of the second
		 * and the first is very nearly zero-mean), so this one decides
		 * the magnitude.  See `sep_fir_trunc`.
		 */
		short slow = (short)((i % 98) < 49 ? SLOW_AMP : -SLOW_AMP);

		lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xB400u);
		input[i] = (short)(slow + w11[i % 11] + w7[i % 7]
				   + (short)((lfsr & 0x3ff) - 512));
	}
}

struct v22recv_pair {
	struct v22_fse a;	/* the object's                             */
	struct v22_fse b;	/* ours                                     */
	short quad_a;
	short quad_b;
};

static void
setup_pair(struct v22recv_pair *p, int use24)
{
	memset(&p->a, MARK & 0xff, sizeof p->a);
	memset(&p->b, MARK & 0xff, sizeof p->b);

	harness_alloc_reset();
	ref_V22_FSE_init(&p->a, &drive_cfg, 1);
	V22_FSE_init(&p->b, &drive_cfg, 1);

	/*
	 * The two things init does not supply.  Each side gets its OWN slicer
	 * -- the object's for the object's loop, ours for ours -- so this run
	 * would notice a slicer that had drifted as well as a receive loop that
	 * had; and its own quadrant, which the slicers read and write.
	 */
	p->quad_a = 0;
	p->quad_b = 0;
	p->a.prev_quad = &p->quad_a;
	p->b.prev_quad = &p->quad_b;
	p->a.decision = use24 ? ref_FSEv22_decision24 : ref_FSEv22_decision12;
	p->b.decision = use24 ? FSEv22_decision24 : FSEv22_decision12;
}

static void
free_pair(struct v22recv_pair *p)
{
	ref_V22_FSE_free(&p->a);
	V22_FSE_free(&p->b);
}

/*
 * The seven heap addresses, the slicer and the quadrant differ between the two
 * sides and always will; blanking them in a copy keeps diff_eq_obj's field
 * report and its run coalescing.  Everything else in the object is compared,
 * including the fields nothing is expected to touch.
 */
static void
blank_pointers(struct v22_fse *dst, const struct v22_fse *src)
{
	*dst = *src;
	dst->out_i = (short *)0;
	dst->out_q = (short *)0;
	dst->icoeff = (short *)0;
	dst->qcoeff = (short *)0;
	dst->hist = (short *)0;
	dst->r44 = (short *)0;
	dst->r48 = (short *)0;
	dst->prev_quad = (short *)0;
	dst->decision = (v22_fse_decision)0;
}

static void
compare_pair(struct v22recv_pair *p, long where)
{
	struct v22_fse ma, mb;
	int i;

	blank_pointers(&ma, &p->a);
	blank_pointers(&mb, &p->b);
	diff_eq_obj("state", struct v22_fse, &mb, &ma, where);

	for (i = 0; i < V22_FSE_TAPS; i++) {
		diff_eq_int("icoeff[%ld]", p->b.icoeff[i], p->a.icoeff[i], i);
		diff_eq_int("qcoeff[%ld]", p->b.qcoeff[i], p->a.qcoeff[i], i);
	}
	for (i = 0; i < V22_FSE_HIST; i++)
		diff_eq_int("hist[%ld]", p->b.hist[i], p->a.hist[i], i);
	for (i = 0; i < V22_FSE_OUT; i++) {
		diff_eq_int("out_i[%ld]", p->b.out_i[i], p->a.out_i[i], i);
		diff_eq_int("out_q[%ld]", p->b.out_q[i], p->a.out_q[i], i);
	}
	/* Neither buffer is read or written by anything reconstructed. */
	for (i = 0; i < V22_FSE_AUX; i++) {
		diff_eq_int("r44[%ld]", p->b.r44[i], p->a.r44[i], i);
		diff_eq_int("r48[%ld]", p->b.r48[i], p->a.r48[i], i);
	}
	diff_eq_int("prev_quad (%ld)", p->quad_b, p->quad_a, where);
}

/* Fill both sides' visible outputs with MARK, so an untouched entry shows. */
static void
mark_outputs(struct v22recv_pair *p)
{
	int i;

	for (i = 0; i < NOUT; i++) {
		out_a[i] = MARK;
		out_b[i] = MARK;
	}
	for (i = 0; i < V22_FSE_OUT; i++) {
		p->a.out_i[i] = MARK;
		p->a.out_q[i] = MARK;
		p->b.out_i[i] = MARK;
		p->b.out_q[i] = MARK;
	}
}

/*
 * One call on both sides, with everything a call can be asked about checked.
 * Returns the reference's symbol count.
 */
static int
step(struct v22recv_pair *p, int pos, short n, long where)
{
	unsigned short ra, rb;
	short hist_n0 = p->a.hist_n;
	short need0 = p->a.need;
	int i;

	mark_outputs(p);

	ra = ref_V22_FSE_receive(&p->a, input + pos, out_a, n);
	rb = V22_FSE_receive(&p->b, input + pos, out_b, n);

	diff_eq_int("at %ld: return", (long)rb, (long)ra, where);
	diff_eq_int("at %ld: return is n_out", (long)ra, p->a.n_out, where);
	/*
	 * The bound the function does not check.  If this ever fires the run
	 * has walked off two heap buffers and nothing below it means anything.
	 */
	diff_eq_int("at %ld: n_out within V22_FSE_OUT", ra <= V22_FSE_OUT, 1,
		    where);
	diff_eq_int("at %ld: n_in is count", p->a.n_in, n, where);

	for (i = 0; i < NOUT; i++)
		diff_eq_int("symbol[%ld]", out_b[i], out_a[i], i);
	/* Nothing beyond the symbols produced: kills "always write the lot". */
	for (i = (int)ra; i < NOUT; i++)
		diff_eq_int("symbol[%ld] untouched", out_a[i], MARK, i);
	if (ra < V22_FSE_OUT) {
		for (i = (int)ra; i < V22_FSE_OUT; i++) {
			diff_eq_int("out_i[%ld] untouched", p->a.out_i[i], MARK,
				    i);
			diff_eq_int("out_q[%ld] untouched", p->a.out_q[i], MARK,
				    i);
		}
		sep_tail_checked++;
	}

	/*
	 * Which entry of `CRRv22_CLK` the reference reached.  Recorded here and
	 * not in the per-symbol analysis because a drive made of whole symbol
	 * intervals only ever reaches ONE of them -- the step is twelve and the
	 * modulus six -- so it takes the ragged runs to move it.
	 */
	if (p->a.clk_phase >= 0 && p->a.clk_phase < V22_CRR_CLK_STEPS)
		sep_clk_seen[p->a.clk_phase]++;

	/*
	 * The history shift, and its boundary.  `hist_n + need > 98` is the
	 * object's test and `>=` would shift one call early, so the input that
	 * separates them is one landing on 98 exactly.  Both are counted here
	 * rather than per symbol so the ragged runs -- which are what put
	 * `hist_n` off the lattice a whole-symbol drive walks -- feed them; on a
	 * call producing several symbols they see only its first interval, so
	 * both are lower bounds and neither can over-count.
	 */
	if (p->a.hist_n < hist_n0)
		sep_hist_shift++;
	if (hist_n0 + need0 == V22_FSE_HIST)
		sep_shift_edge++;

	compare_pair(p, where);
	return (int)ra;
}

/* ------------------------------------------------------------------ *
 * The counters
 * ------------------------------------------------------------------ */

/*
 * A 49-tap sum of products with signed overflow detected rather than risked.
 * The products cannot overflow -- two shorts -- but the sum can, and a test
 * that depended on the wrap would be resting on undefined behaviour.
 */
static int
fir_of(const short *coeff, const short *win, int *overflowed)
{
	int acc = 0;
	int k;

	for (k = 0; k < V22_FSE_TAPS; k++) {
		int term = coeff[k] * win[k];

		if ((term > 0 && acc > INT_MAX - term)
		    || (term < 0 && acc < INT_MIN - term)) {
			*overflowed = 1;
			return acc;
		}
		acc += term;
	}
	return acc;
}

/*
 * Is the reference's transition from `old` to `got` impossible under the
 * SINGLE-SHIFT reading of the smoother?
 *
 * The right reading shifts each term down fifteen and adds; the wrong one adds
 * and shifts once.  They differ by at most one and agree on most inputs, which
 * is finding 3052's shape exactly.  The input the reference smoothed is not
 * observable, so this asks whether any input at all could have produced `got`
 * under the wrong reading: where none could, that reading is dead on this
 * symbol whatever the input was.
 */
static int
smooth_separated(short old, short got)
{
	int base = ((int)old * W_OLD) >> 15;
	int admissible = 0;
	int p;

	for (p = -32768; p <= 32767; p++) {
		short split = (short)(base + ((p * W_NEW) >> 15));
		short single;

		if (split != got)
			continue;
		admissible = 1;
		single = (short)((((int)old * W_OLD) + (p * W_NEW)) >> 15);
		if (single == got)
			return 0;
	}
	return admissible;
}

/*
 * Everything one symbol of the reference's own run can be made to say.  Called
 * only where the call produced exactly one symbol, so `hist_n`, `hist` and the
 * two smoothed values after the call are that symbol's.
 */
static void
analyse_symbol(struct v22recv_pair *p, const short *ic0, const short *qc0,
	       short err0, short mse0, short sym0)
{
	const short *win;
	int overflow = 0;
	int acc;
	short hist_n = p->a.hist_n;

	/*
	 * The window, and the wrong reading it kills: `&hist[hist_n - 49]`
	 * without the clamp, which is a negative index for the first eight
	 * symbols of every connection.
	 */
	if (hist_n > V22_FSE_TAPS - 1) {
		win = &p->a.hist[hist_n - V22_FSE_TAPS];
		sep_window_slide++;
	} else {
		win = p->a.hist;
		sep_window_clamp++;
	}

	/*
	 * Kills "keep the accumulator in 32 bits through the output gain".
	 * The object truncates to a short between the >> 15 and the * 0x8908,
	 * so the two readings agree on every sum that fits and diverge wildly
	 * on the ones that do not.
	 */
	acc = fir_of(ic0, win, &overflow);
	if ((acc >> 15) > 32767 || (acc >> 15) < -32768)
		sep_fir_trunc++;
	acc = fir_of(qc0, win, &overflow);
	if ((acc >> 15) > 32767 || (acc >> 15) < -32768)
		sep_fir_trunc++;
	if (overflow)
		sep_acc_overflow++;

	if (smooth_separated(err0, p->a.err_avg))
		sep_smooth_err++;
	if (smooth_separated(mse0, p->a.mse))
		sep_smooth_mse++;

	/* Kills an I/Q or a cos/sin swap in the output pair. */
	if (p->a.out_i[0] != p->a.out_q[0])
		sep_iq_differ++;

	/* The gain band, and the LMS gate, both straight off the reference. */
	if (p->a.pll_sel >= 0 && p->a.pll_sel <= 2)
		sep_pll_band[p->a.pll_sel]++;
	if (p->a.mse > 0)
		sep_mse_open++;
	else
		sep_mse_shut++;

	/*
	 * The training counter's edge: `sym_count` stops one past
	 * V22_FSE_TRAIN, and the symbol that takes it there is the one that
	 * leaves `pll_sel` at 1.  Kills an off-by-one in either comparison.
	 */
	if (p->a.sym_count == V22_FSE_TRAIN + 1 && p->a.pll_sel == 1)
		sep_train_edge++;
	if (sym0 == V22_FSE_TRAIN + 1 && p->a.sym_count == V22_FSE_TRAIN + 1)
		sep_train_held++;

}

/* ------------------------------------------------------------------ *
 * The runs
 * ------------------------------------------------------------------ */

/*
 * One symbol per call: 1 sample for the first -- `need` is 1 out of init --
 * and 6 for the rest.  This is the run the counters come from, because every
 * per-symbol quantity is a difference between two observable states here.
 */
static int
run_persymbol(const char *label, int use24)
{
	struct v22recv_pair p;
	short ic0[V22_FSE_TAPS], qc0[V22_FSE_TAPS];
	int pos = 0, first = 1, symbols = 0, rc;

	setup_pair(&p, use24);
	diff_begin(label);

	while (pos + V22_FSE_INTERP <= NIN) {
		short n = (short)(first ? 1 : V22_FSE_INTERP);
		short err0 = p.a.err_avg;
		short mse0 = p.a.mse;
		short sym0 = p.a.sym_count;

		memcpy(ic0, p.a.icoeff, sizeof ic0);
		memcpy(qc0, p.a.qcoeff, sizeof qc0);

		if (step(&p, pos, n, pos) == 1) {
			analyse_symbol(&p, ic0, qc0, err0, mse0, sym0);
			symbols++;
		}
		pos += n;
		first = 0;
	}

	diff_eq_int("symbols produced (%ld)", symbols > 300, 1, symbols);
	rc = diff_end();
	free_pair(&p);
	return rc;
}

/*
 * Ragged blocks.  These are what move `clk_phase` off the one entry a
 * whole-symbol drive ever reaches, what exercise the partial-interval stash,
 * and what let `hist_n + need` land on 98 exactly.
 */
static int
run_blocks(const char *label, const int *chunks, int nchunks, int use24)
{
	struct v22recv_pair p;
	int pos = 0, ci = 0, symbols = 0, rc;

	setup_pair(&p, use24);
	diff_begin(label);

	/* An empty call is not a no-op: it still rewrites n_in and n_out. */
	diff_eq_int("empty call returns %ld",
		    (long)V22_FSE_receive(&p.b, input, out_b, 0),
		    (long)ref_V22_FSE_receive(&p.a, input, out_a, 0), 0);
	compare_pair(&p, -1);

	while (pos < NIN) {
		int n = chunks[ci++ % nchunks];
		short need0 = p.a.need;

		if (pos + n > NIN)
			n = NIN - pos;
		if (n < need0)
			sep_short_block++;
		symbols += step(&p, pos, (short)n, pos);
		pos += n;
	}

	diff_eq_int("symbols produced (%ld)", symbols > 300, 1, symbols);
	rc = diff_end();
	free_pair(&p);
	return rc;
}

/*
 * A count of zero and a count below zero, which are not the same thing: zero
 * never enters the loop, a negative one enters and takes the short-block path.
 * Neither may produce a symbol or move anything but `n_in` and `n_out`.
 */
static int
run_degenerate(const char *label, int use24)
{
	struct v22recv_pair p;
	struct v22_fse before_a;
	int rc;

	setup_pair(&p, use24);
	diff_begin(label);

	/* Get some state in first, so "unchanged" is a real statement. */
	step(&p, 0, 40, 0);
	before_a = p.a;

	mark_outputs(&p);
	diff_eq_int("count 0 returns %ld",
		    (long)V22_FSE_receive(&p.b, input + 40, out_b, 0),
		    (long)ref_V22_FSE_receive(&p.a, input + 40, out_a, 0), 0);
	diff_eq_int("count 0 produced nothing (%ld)", p.a.n_out, 0, 0);
	diff_eq_int("count 0 recorded n_in (%ld)", p.a.n_in, 0, 0);
	diff_eq_int("count 0 left need (%ld)", p.a.need, before_a.need, 0);
	diff_eq_int("count 0 left hist_n (%ld)", p.a.hist_n, before_a.hist_n,
		    0);
	diff_eq_int("count 0 left clk_phase (%ld)", p.a.clk_phase,
		    before_a.clk_phase, 0);
	diff_eq_int("count 0 left phase (%ld)", p.a.phase, before_a.phase, 0);
	compare_pair(&p, 0);

	mark_outputs(&p);
	diff_eq_int("count -3 returns %ld",
		    (long)V22_FSE_receive(&p.b, input + 40, out_b, -3),
		    (long)ref_V22_FSE_receive(&p.a, input + 40, out_a, -3), 0);
	diff_eq_int("count -3 produced nothing (%ld)", p.a.n_out, 0, 0);
	diff_eq_int("count -3 recorded n_in (%ld)", p.a.n_in, -3, 0);
	diff_eq_int("count -3 left need (%ld)", p.a.need, before_a.need, 0);
	diff_eq_int("count -3 left hist_n (%ld)", p.a.hist_n, before_a.hist_n,
		    0);
	compare_pair(&p, 1);

	rc = diff_end();
	free_pair(&p);
	return rc;
}

/*
 * The two gates, each driven both ways.  This is what NAMES `pll_on` and
 * `lms_on`: a name taken from a gate that was only ever run one way is not
 * forced, so each run asserts both that the block ran when the flag was set
 * and that nothing it writes moved when it was clear.
 *
 * `seed_mse` is the third gate.  The smoothed squared error cannot go
 * non-positive under any signal -- it is a sum of squares through a smoother
 * that cannot cross zero from above -- so the only way to see the LMS update
 * skipped on that test is to seed the field, which a datapump could do and
 * this does.
 */
static int
run_gates(const char *label, int use24, int pll_on, int lms_on, short seed_mse)
{
	struct v22recv_pair p;
	short ic0[V22_FSE_TAPS], qc0[V22_FSE_TAPS];
	int pos = 0, coeff_moved = 0, pll_moved = 0, rc;

	setup_pair(&p, use24);
	p.a.pll_on = pll_on;
	p.b.pll_on = pll_on;
	p.a.lms_on = lms_on;
	p.b.lms_on = lms_on;
	p.a.mse = seed_mse;
	p.b.mse = seed_mse;

	memcpy(ic0, p.a.icoeff, sizeof ic0);
	memcpy(qc0, p.a.qcoeff, sizeof qc0);

	diff_begin(label);

	while (pos + 30 <= NIN) {
		step(&p, pos, 30, pos);
		if (memcmp(ic0, p.a.icoeff, sizeof ic0) != 0
		    || memcmp(qc0, p.a.qcoeff, sizeof qc0) != 0)
			coeff_moved = 1;
		if (p.a.phase != 0 || p.a.freq != 0 || p.a.sym_count != 0)
			pll_moved = 1;
		pos += 30;
	}

	if (pll_on) {
		diff_eq_int("pll_on: carrier recovery ran (%ld)", pll_moved, 1,
			    0);
		diff_eq_int("pll_on: sym_count counted (%ld)",
			    p.a.sym_count > 0, 1, p.a.sym_count);
		sep_pll_ran++;
	} else {
		/*
		 * Every field the PLL block writes, and nothing else writes,
		 * still holds what init left.  That is what makes +0x14 the
		 * carrier-recovery gate rather than a field that happens to be
		 * 1.
		 */
		diff_eq_int("pll_on clear: phase held (%ld)", p.a.phase, 0, 0);
		diff_eq_int("pll_on clear: freq held (%ld)", p.a.freq, 0, 0);
		diff_eq_int("pll_on clear: pll_sel held (%ld)", p.a.pll_sel, 0,
			    0);
		diff_eq_int("pll_on clear: sym_count held (%ld)", p.a.sym_count,
			    0, 0);
		sep_pll_skipped++;
	}

	if (lms_on && seed_mse >= 0) {
		diff_eq_int("lms_on: coefficients adapted (%ld)", coeff_moved,
			    1, 0);
		sep_lms_ran++;
	} else {
		diff_eq_int("lms gate shut: coefficients held (%ld)",
			    coeff_moved, 0, 0);
		sep_lms_skipped++;
	}

	rc = diff_end();
	free_pair(&p);
	return rc;
}

/*
 * The mse gate on its own: seeded far enough negative that the smoother takes
 * a while to climb back, so the run sees the update skipped and then resumed
 * with everything else running normally.
 */
static int
run_mse_gate(const char *label, int use24)
{
	struct v22recv_pair p;
	short ic0[V22_FSE_TAPS];
	int pos = 0, first = 1, shut = 0, opened = 0, rc;

	setup_pair(&p, use24);
	p.a.mse = -30000;
	p.b.mse = -30000;

	diff_begin(label);

	while (pos + V22_FSE_INTERP <= 600) {
		short n = (short)(first ? 1 : V22_FSE_INTERP);

		memcpy(ic0, p.a.icoeff, sizeof ic0);
		if (step(&p, pos, n, pos) == 1) {
			int moved = memcmp(ic0, p.a.icoeff, sizeof ic0) != 0;

			if (p.a.mse <= 0) {
				diff_eq_int("at %ld: shut gate held the taps",
					    moved, 0, pos);
				sep_mse_shut++;
				shut++;
			} else {
				sep_mse_open++;
				if (moved)
					opened++;
			}
		}
		pos += n;
		first = 0;
	}

	diff_eq_int("gate was shut for a stretch (%ld)", shut > 0, 1, shut);
	diff_eq_int("and opened again (%ld)", opened > 0, 1, opened);

	rc = diff_end();
	free_pair(&p);
	return rc;
}

int
main(void)
{
	static const int bulk[] = { 30 };
	static const int ragged[] = { 1, 7, 13, 3, 6, 2, 19, 5, 11, 4, 40, 9 };
	static const int tiny[] = { 1 };
	static const int edge[] = { 5, 6, 6, 6, 6, 6, 6, 6, 7, 6, 6, 6 };
	int rc = 0;

	make_drive();
	make_input();

	rc |= run_persymbol("v22 receive one symbol per call, 1200", 0);
	rc |= run_persymbol("v22 receive one symbol per call, 2400", 1);
	rc |= run_blocks("v22 receive bulk, 1200", bulk, 1, 0);
	rc |= run_blocks("v22 receive bulk, 2400", bulk, 1, 1);
	rc |= run_blocks("v22 receive ragged, 1200", ragged,
			 (int)(sizeof ragged / sizeof ragged[0]), 0);
	rc |= run_blocks("v22 receive ragged, 2400", ragged,
			 (int)(sizeof ragged / sizeof ragged[0]), 1);
	rc |= run_blocks("v22 receive one sample per call, 1200", tiny, 1, 0);
	rc |= run_blocks("v22 receive shift boundary, 2400", edge,
			 (int)(sizeof edge / sizeof edge[0]), 1);
	rc |= run_degenerate("v22 receive empty and negative", 0);
	rc |= run_gates("v22 receive gates on", 0, 1, 1, 0);
	rc |= run_gates("v22 receive pll_on clear", 0, 0, 1, 0);
	rc |= run_gates("v22 receive lms_on clear", 1, 1, 0, 0);
	rc |= run_mse_gate("v22 receive mse gate, 1200", 0);

	/*
	 * The guards.  Each names the reading it kills; a zero here means the
	 * run never separated that reading from this one, and a pass would be
	 * finding 3052 rather than a result.
	 */
	diff_begin("v22 receive separation");
	diff_eq_int("window clamp exercised (%ld)", sep_window_clamp > 0, 1,
		    sep_window_clamp);
	diff_eq_int("full window exercised (%ld)", sep_window_slide > 0, 1,
		    sep_window_slide);
	diff_eq_int("history shift exercised (%ld)", sep_hist_shift > 0, 1,
		    sep_hist_shift);
	diff_eq_int("shift boundary hit exactly (%ld)", sep_shift_edge > 0, 1,
		    sep_shift_edge);
	diff_eq_int("FIR accumulator truncation exercised (%ld)",
		    sep_fir_trunc > 0, 1, sep_fir_trunc);
	diff_eq_int("no accumulator overflowed (%ld)", sep_acc_overflow, 0,
		    sep_acc_overflow);
	diff_eq_int("err_avg split shift separated (%ld)", sep_smooth_err > 0,
		    1, sep_smooth_err);
	diff_eq_int("mse split shift separated (%ld)", sep_smooth_mse > 0, 1,
		    sep_smooth_mse);
	diff_eq_int("I and Q outputs separated (%ld)", sep_iq_differ > 0, 1,
		    sep_iq_differ);
	diff_eq_int("I and Q prototypes separated (%ld)", sep_coeff_iq > 0, 1,
		    sep_coeff_iq);
	diff_eq_int("clk_phase 0 reached (%ld)", sep_clk_seen[0] > 0, 1,
		    sep_clk_seen[0]);
	diff_eq_int("clk_phase 2 reached (%ld)", sep_clk_seen[2] > 0, 1,
		    sep_clk_seen[2]);
	diff_eq_int("clk_phase 4 reached (%ld)", sep_clk_seen[4] > 0, 1,
		    sep_clk_seen[4]);
	/*
	 * The odd entries of CRRv22_CLK are NOT reachable from a zero init:
	 * the step is two per sample and the reduction subtracts six.  Stated
	 * rather than asserted the other way round, because it is a property
	 * of the reading and a future change that made them reachable would be
	 * a defect.
	 */
	diff_eq_int("clk_phase 1 unreachable (%ld)", sep_clk_seen[1], 0,
		    sep_clk_seen[1]);
	diff_eq_int("clk_phase 3 unreachable (%ld)", sep_clk_seen[3], 0,
		    sep_clk_seen[3]);
	diff_eq_int("clk_phase 5 unreachable (%ld)", sep_clk_seen[5], 0,
		    sep_clk_seen[5]);
	diff_eq_int("pll band 0 reached (%ld)", sep_pll_band[0] > 0, 1,
		    sep_pll_band[0]);
	diff_eq_int("pll band 1 reached (%ld)", sep_pll_band[1] > 0, 1,
		    sep_pll_band[1]);
	diff_eq_int("pll band 2 reached (%ld)", sep_pll_band[2] > 0, 1,
		    sep_pll_band[2]);
	diff_eq_int("training edge seen (%ld)", sep_train_edge > 0, 1,
		    sep_train_edge);
	diff_eq_int("sym_count held past training (%ld)", sep_train_held > 0, 1,
		    sep_train_held);
	diff_eq_int("lms gate open seen (%ld)", sep_mse_open > 0, 1,
		    sep_mse_open);
	diff_eq_int("lms gate shut seen (%ld)", sep_mse_shut > 0, 1,
		    sep_mse_shut);
	diff_eq_int("partial interval stashed (%ld)", sep_short_block > 0, 1,
		    sep_short_block);
	diff_eq_int("pll_on driven set (%ld)", sep_pll_ran > 0, 1, sep_pll_ran);
	diff_eq_int("pll_on driven clear (%ld)", sep_pll_skipped > 0, 1,
		    sep_pll_skipped);
	diff_eq_int("lms_on driven set (%ld)", sep_lms_ran > 0, 1, sep_lms_ran);
	diff_eq_int("lms_on driven clear (%ld)", sep_lms_skipped > 0, 1,
		    sep_lms_skipped);
	diff_eq_int("output tail proved intact (%ld)", sep_tail_checked > 0, 1,
		    sep_tail_checked);
	rc |= diff_end();

	return rc;
}
