/*
 * t_fpm_ecc.c -- differential test of the echo canceller's setup.
 *
 * The eight heap pointers necessarily differ between the two builds, so the
 * states are copied, the pointer slots blanked in both copies, and the rest
 * compared as one object -- which keeps the five padding holes (0x06, 0x12,
 * 0x1a, 0x26, 0x36) inside the comparison, where a field-by-field check would
 * have skipped exactly the bytes an unmodelled store would land in.  The
 * buffers themselves are then compared by content.
 *
 * ECC_CFG is compared against the blob's own copy directly, and again
 * indirectly: FPM_ECC_init(state, NULL, 1) takes it as the configuration, so
 * every field of it reaches the state comparison.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_ecc.h"

extern void ref_FPM_ECC_init(void *state, const void *cfg, int fresh);
extern void ref_FPM_ECC_free(void *state);
extern struct fpm_ecc_cfg ref_ECC_CFG;
extern short ref_FPM_circ_dotp2(const short *coeff, const short *hist,
				short widx, short taps, short stride,
				short shift);

/*
 * Six constellation maps of our own.  The real ones are SMCv32_IMAP16 and
 * friends; init only copies the two pointers, and the cancel test needs maps
 * wide enough to accept any low byte, so these are 256 entries each.
 */
static short map_i[6][256];
static short map_q[6][256];
static const short *const test_imap[6] = {
	map_i[0], map_i[1], map_i[2], map_i[3], map_i[4], map_i[5]
};
static const short *const test_qmap[6] = {
	map_q[0], map_q[1], map_q[2], map_q[3], map_q[4], map_q[5]
};

static void
cmp_array(const char *tag, const short *got, const short *want, int n,
	  long input)
{
	int i;
	int bad = -1;

	for (i = 0; i < n; i++) {
		if (got[i] != want[i]) {
			bad = i;
			break;
		}
	}
	diff_eq_int(tag, bad, -1, input);
	if (bad >= 0)
		diff_eq_int("  first differing element", got[bad], want[bad],
			    bad);
}

static void
cmp_state(const char *tag, const struct fpm_ecc *got,
	  const struct fpm_ecc *want, long input)
{
	struct fpm_ecc a, b;
	int j;

	memcpy(&a, got, sizeof(a));
	memcpy(&b, want, sizeof(b));
	a.line = b.line = 0;
	a.near_i = b.near_i = 0;
	a.near_q = b.near_q = 0;
	a.far_i = b.far_i = 0;
	a.far_q = b.far_q = 0;
	for (j = 0; j < 3; j++)
		a.coef[j] = b.coef[j] = 0;
	diff_eq_obj(tag, struct fpm_ecc, &a, &b, input);

	diff_eq_int("line allocated", got->line != 0, 1, input);
	cmp_array("line contents", got->line, want->line, want->line_len,
		  input);
	cmp_array("near_i contents", got->near_i, want->near_i,
		  want->near_len, input);
	cmp_array("near_q contents", got->near_q, want->near_q,
		  want->near_len, input);
	cmp_array("far_i contents", got->far_i, want->far_i, want->far_len,
		  input);
	cmp_array("far_q contents", got->far_q, want->far_q, want->far_len,
		  input);
	for (j = 0; j < 3; j++)
		cmp_array("coef contents", got->coef[j], want->coef[j],
			  2 * (want->cfg.near_taps + want->cfg.far_taps), j);
}

/*
 * One configuration, through the three paths: fresh allocation, re-init in
 * place, and re-init after the caller has scribbled on the state.
 */
static void
run(const struct fpm_ecc_cfg *cfg, short near_delay, short far_delay,
    long input)
{
	struct fpm_ecc a, b;
	short *keep;
	int i;

	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	a.near_delay = b.near_delay = near_delay;
	a.far_delay = b.far_delay = far_delay;

	ref_FPM_ECC_init(&a, cfg, 1);
	FPM_ECC_init(&b, cfg, 1);
	cmp_state("fresh", &b, &a, input);

	/*
	 * Re-init in place.  Nothing is freed and nothing is allocated, so
	 * the buffers must be the same ones, cleared again.
	 */
	keep = b.line;
	for (i = 0; i < a.line_len; i++) {
		a.line[i] = (short)(i * 7);
		b.line[i] = (short)(i * 7);
	}
	a.mu = b.mu = 0x1234;
	a.phase = b.phase = 2;
	ref_FPM_ECC_init(&a, cfg, 0);
	FPM_ECC_init(&b, cfg, 0);
	cmp_state("re-init", &b, &a, input);
	diff_eq_int("re-init kept the buffer", b.line == keep, 1, input);

	ref_FPM_ECC_free(&a);
	FPM_ECC_free(&b);
}

/*
 * ---------------------------------------------------------------- cancel --
 *
 * An echo canceller is adaptive, so a single call proves almost nothing: the
 * coefficients have to be driven far enough from zero that an arithmetic
 * disagreement anywhere in the update law shows up.  Each case below runs
 * many blocks and compares the entire state, both sample buffers and all
 * eight arrays after every one of them, and then asserts that the
 * coefficients actually moved -- an excitation that left them at zero would
 * pass every comparison while testing nothing.
 *
 * The excitation is an echo of the very symbols in the delay line plus a
 * near-end signal, which is what the block sees in service.
 */

#define MAXBLK 64

static unsigned long rng_state;

static int
rng_next(void)
{
	rng_state = rng_state * 1103515245UL + 12345UL;
	return (int)((rng_state >> 16) & 0x7fff);
}

/* A repeating symbol sequence: map 0..5 in the high byte, point in the low. */
static short
symbol_at(int k)
{
	return (short)((((k * 7) % 6) << 8) | ((k * 37) & 0xff));
}

extern short ref_FPM_ECC_cancel(void *state, short *buf, unsigned short count);

static int
cancel_case(const char *tag, const struct fpm_ecc_cfg *cfg, short near_delay,
	    short far_delay, short hold, short freeze, int adapt_near,
	    int adapt_far, short mu, short shrink, int blocks, int blocksize)
{
	struct fpm_ecc a, b;
	short bufa[MAXBLK], bufb[MAXBLK];
	long input;
	int blk, i, j;
	int moved = 0;
	int base = 0;

	diff_begin(tag);

	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	a.near_delay = b.near_delay = near_delay;
	a.far_delay = b.far_delay = far_delay;
	ref_FPM_ECC_init(&a, cfg, 1);
	FPM_ECC_init(&b, cfg, 1);

	for (i = 0; i < a.line_len; i++) {
		a.line[i] = symbol_at(i);
		b.line[i] = symbol_at(i);
	}
	a.hold_power = b.hold_power = hold;
	a.freeze = b.freeze = freeze;
	a.adapt_near = b.adapt_near = adapt_near;
	a.adapt_far = b.adapt_far = adapt_far;
	a.mu = b.mu = mu;
	/*
	 * The coefficient block stride comes from cfg.near_taps / cfg.far_taps
	 * and the loop counts from near_len / far_len.  init makes them equal,
	 * so shrinking the lengths is the only way to tell which the code used
	 * for which.
	 */
	if (shrink != 0) {
		if (a.near_len > shrink)
			a.near_len = b.near_len = (short)(a.near_len - shrink);
		if (a.far_len > shrink)
			a.far_len = b.far_len = (short)(a.far_len - shrink);
	}

	rng_state = 12345UL + (unsigned long)blocksize;
	for (blk = 0; blk < blocks; blk++) {
		short ra, rb;

		input = blk;
		for (i = 0; i < blocksize; i++) {
			int s = symbol_at((base + i) / 3);
			int echo = map_i[(s >> 8) & 0xff][s & 0xff];

			bufa[i] = bufb[i] = (short)(echo / 2 +
						    (rng_next() % 2001) - 1000);
		}
		base += blocksize;

		ra = ref_FPM_ECC_cancel(&a, bufa, (unsigned short)blocksize);
		rb = FPM_ECC_cancel(&b, bufb, (unsigned short)blocksize);

		diff_eq_int("symbols consumed (%ld)", rb, ra, input);
		cmp_array("residual samples", bufb, bufa, blocksize, input);
		cmp_state("state", &b, &a, input);
	}

	for (j = 0; j < 3; j++)
		for (i = 0; i < 2 * (a.cfg.near_taps + a.cfg.far_taps); i++)
			if (a.coef[j][i] != 0)
				moved = 1;
	/*
	 * Nothing can move until a symbol has been pushed into the histories,
	 * which takes three samples, and until the residual is large enough to
	 * survive the update's 17-bit shift.
	 */
	diff_eq_int("the coefficients moved (%ld)", moved,
		    (base >= 12 && freeze == 0 && mu != 0 &&
		     (adapt_near == 1 || adapt_far == 1)) ? 1 : 0, 0);

	ref_FPM_ECC_free(&a);
	FPM_ECC_free(&b);
	return diff_end();
}

/*
 * ---------------------------------------------------------------------------
 * FPM_circ_dotp2 -- the library form of this file's own `ecc_filter`.
 *
 * Three things distinguish it from a plain dot product, and a lazy input set
 * checks none of them:
 *
 *   - the walk is CIRCULAR and split in two halves at `widx`, so a `widx` in
 *     the middle is the only shape that exercises both;
 *   - the coefficients advance by `stride`, so a stride of 1 would leave the
 *     whole generalisation untested and is not the only case driven here;
 *   - `>> 3` is applied per PRODUCT and `shift - 3` to the SUM, which is not
 *     the same as one `>> shift` at the end -- each product loses its low
 *     three bits separately.  `dotp_trunc_seen` counts the products where
 *     that per-term truncation actually discarded something.
 *
 * `shift < 3` is not driven: the object computes the residual into `%cl` and
 * x86 masks a shift count to five bits, so `shift == 2` shifts right by 31.
 * Nothing calls this function, so there is no behaviour there to preserve.
 */
#define DTAPS	64

static short d_hist[DTAPS];
static short d_coeff[DTAPS * 8];
static int dotp_trunc_seen, dotp_nonzero_seen;

static unsigned dseed;

static int
drnd(int range)
{
	dseed = dseed * 1103515245u + 12345u;
	return (int)((dseed >> 13) % (unsigned)range);
}

static int
dotp_case(short widx, short taps, short stride, short shift)
{
	short got, want;
	int i;

	for (i = 0; i < DTAPS; i++)
		d_hist[i] = (short)(drnd(65535) - 32768);
	for (i = 0; i < DTAPS * 8; i++)
		d_coeff[i] = (short)(drnd(65535) - 32768);

	{
		const short *c = d_coeff;

		for (i = widx; i >= 0; i--) {
			if ((d_hist[i] * *c) & 7)
				dotp_trunc_seen++;
			c += stride;
		}
		for (i = taps - 1; i > widx; i--) {
			if ((d_hist[i] * *c) & 7)
				dotp_trunc_seen++;
			c += stride;
		}
	}

	want = ref_FPM_circ_dotp2(d_coeff, d_hist, widx, taps, stride, shift);
	got = FPM_circ_dotp2(d_coeff, d_hist, widx, taps, stride, shift);

	if (want != 0)
		dotp_nonzero_seen++;

	diff_eq_int("widx %ld", got, want, widx);
	return 0;
}

int
main(void)
{
	struct fpm_ecc_cfg cfg;
	int i, j;
	int rc;

	for (i = 0; i < 6; i++)
		for (j = 0; j < 256; j++) {
			map_i[i][j] = (short)(1000 * i + j);
			map_q[i][j] = (short)(-500 * i - j);
		}

	diff_begin("ECC_CFG");
	diff_eq_obj("the library default", struct fpm_ecc_cfg, &ECC_CFG,
		    &ref_ECC_CFG, 0);
	if (diff_end() != 0)
		return 1;

	diff_begin("FPM_ECC_init");

	/* The V.32 shape: 480 symbols of far lag, 40 taps a side. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.far_lag = 480;
	cfg.near_taps = 40;
	cfg.far_taps = 40;
	cfg.imap = test_imap;
	cfg.qmap = test_qmap;
	cfg.fill = 16;
	run(&cfg, 24, 8, 1);
	run(&cfg, 1, 1, 2);
	run(&cfg, 0, 0, 3);		/* both taps at the top of the line */
	run(&cfg, 200, 300, 4);

	/* A near section only, and a far section only. */
	cfg.near_taps = 0;
	run(&cfg, 4, 4, 5);
	cfg.near_taps = 40;
	cfg.far_taps = 0;
	run(&cfg, 4, 4, 6);

	/* Short lines, so the read taps sit near their wrap. */
	cfg.far_lag = 8;
	cfg.near_taps = 6;
	cfg.far_taps = 5;
	cfg.fill = 0x0103;
	run(&cfg, 3, 2, 7);
	run(&cfg, 0, 1, 8);
	cfg.far_lag = 1;
	run(&cfg, 0, 0, 9);		/* line_len 1: the modulo's edge */

	/* No configuration at all: the library default is taken. */
	run(0, 12, 6, 10);
	run(0, 0, 0, 11);
	rc = diff_end();

	/* The V.32 shape again, and every combination of the three switches. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.far_lag = 480;
	cfg.near_taps = 40;
	cfg.far_taps = 40;
	cfg.imap = test_imap;
	cfg.qmap = test_qmap;
	cfg.fill = 16;

	rc |= cancel_case("cancel: V.32, both sections adapting", &cfg,
			  24, 8, 0, 0, 1, 1, 0x29, 0, 40, 24);
	/*
	 * The near update off with near_taps still set: the blob then runs the
	 * FAR update over the NEAR coefficient blocks, because the pointer it
	 * uses is only advanced by the near update it just skipped.  A tidier
	 * rewrite computes coef + 2*near_taps here and fails this case.
	 */
	rc |= cancel_case("cancel: far adapting, near frozen (the aliasing)",
			  &cfg, 24, 8, 0, 0, 0, 1, 0x29, 0, 40, 24);
	rc |= cancel_case("cancel: near adapting, far frozen", &cfg,
			  24, 8, 0, 0, 1, 0, 0x29, 0, 40, 24);
	rc |= cancel_case("cancel: frozen entirely", &cfg,
			  24, 8, 0, 1, 1, 1, 0x29, 0, 20, 24);
	rc |= cancel_case("cancel: power meters held", &cfg,
			  24, 8, 1, 0, 1, 1, 0x29, 0, 20, 24);
	rc |= cancel_case("cancel: adapt flags that are not 1", &cfg,
			  24, 8, 0, 0, 2, -1, 0x29, 0, 20, 24);

	/* A much larger step, so the coefficients run away and wrap. */
	rc |= cancel_case("cancel: mu 0x400", &cfg, 24, 8, 0, 0, 1, 1,
			  0x400, 0, 40, 24);
	rc |= cancel_case("cancel: mu 0", &cfg, 24, 8, 0, 0,
			  1, 1, 0, 0, 8, 24);
	rc |= cancel_case("cancel: mu -0x29", &cfg, 24, 8, 0, 0, 1, 1,
			  -0x29, 0, 20, 24);

	/*
	 * Lengths shorter than the configured tap counts: the stride between
	 * coefficient blocks must still come from the config.
	 */
	rc |= cancel_case("cancel: near_len and far_len shrunk", &cfg,
			  24, 8, 0, 0, 1, 1, 0x29, 7, 40, 24);

	/* Block sizes around the three-samples-per-symbol boundary. */
	rc |= cancel_case("cancel: one sample a block", &cfg, 24, 8, 0, 0,
			  1, 1, 0x29, 0, 40, 1);
	rc |= cancel_case("cancel: two", &cfg, 24, 8, 0, 0, 1, 1, 0x29, 0,
			  30, 2);
	rc |= cancel_case("cancel: three", &cfg, 24, 8, 0, 0, 1, 1, 0x29, 0,
			  30, 3);
	rc |= cancel_case("cancel: five", &cfg, 24, 8, 0, 0, 1, 1, 0x29, 0,
			  30, 5);
	rc |= cancel_case("cancel: no samples at all", &cfg, 24, 8, 0, 0,
			  1, 1, 0x29, 0, 4, 0);
	rc |= cancel_case("cancel: a full block", &cfg, 24, 8, 0, 0, 1, 1,
			  0x29, 0, 30, MAXBLK);

	/* No near section: the far update starts at the top of the set. */
	cfg.near_taps = 0;
	rc |= cancel_case("cancel: far section only", &cfg, 24, 8, 0, 0,
			  1, 1, 0x29, 0, 40, 24);
	/*
	 * `far_taps == 0` is NOT tested here and cannot be: the far blocks sit
	 * at `2 * near_taps` inside a coefficient set that is only
	 * `2 * (near_taps + far_taps)` long, so with no far taps the far
	 * filter reads and the far update writes one entry PAST the
	 * allocation -- and the unguarded symbol push stores into a
	 * zero-length far history besides.  Both builds do it, into different
	 * heap blocks, so the two disagree on garbage.  A single far tap is
	 * the smallest configuration that stays inside its own memory.
	 */
	cfg.near_taps = 40;
	cfg.far_taps = 1;
	rc |= cancel_case("cancel: a single far tap", &cfg, 24, 8, 0, 0,
			  1, 1, 0x29, 0, 40, 24);

	/*
	 * A short line, so both read taps wrap many times over -- with the
	 * real 480-symbol lag that needs more than 1,440 samples a case.
	 */
	cfg.far_lag = 8;
	cfg.near_taps = 6;
	cfg.far_taps = 5;
	cfg.fill = 0x0103;
	rc |= cancel_case("cancel: an eight-symbol lag, taps wrapping", &cfg,
			  3, 2, 0, 0, 1, 1, 0x29, 0, 60, 24);
	rc |= cancel_case("cancel: the same, one sample a block", &cfg,
			  3, 2, 0, 0, 1, 1, 0x29, 0, 60, 1);
	cfg.far_lag = 2;
	rc |= cancel_case("cancel: a two-symbol lag", &cfg, 1, 1, 0, 0,
			  1, 1, 0x29, 0, 60, 7);

	/* The library default as the configuration, maps patched in. */
	memcpy(&cfg, &ECC_CFG, sizeof(cfg));
	cfg.imap = test_imap;
	cfg.qmap = test_qmap;
	rc |= cancel_case("cancel: ECC_CFG's own shape", &cfg, 12, 6, 0, 0,
			  1, 1, 0x29, 0, 40, 24);


	/* --- FPM_circ_dotp2 --------------------------------------------- */

	dseed = 9137u;
	diff_begin("FPM_circ_dotp2");

	/* Stride 1 and shift 17: exactly what `ecc_filter` plus its caller
	 * does, which is the one configuration with a known intended use. */
	for (i = 0; i < DTAPS; i += 3)
		dotp_case((short)i, DTAPS, 1, 17);

	/* Both ends of the walk, where one of the two halves does nothing. */
	dotp_case(0, DTAPS, 1, 17);
	dotp_case(DTAPS - 1, DTAPS, 1, 17);
	dotp_case(-1, DTAPS, 1, 17);

	/* Degenerate lengths. */
	dotp_case(0, 1, 1, 17);
	dotp_case(0, 2, 1, 17);
	dotp_case(1, 2, 1, 17);
	dotp_case(-1, 0, 1, 17);

	/* The stride generalisation, which stride 1 cannot reach. */
	for (i = 1; i <= 8; i++)
		dotp_case(20, DTAPS, (short)i, 17);
	for (i = 1; i <= 8; i++)
		dotp_case(0, DTAPS, (short)i, 17);

	/*
	 * A negative stride is NOT driven.  It is arithmetically fine on both
	 * sides -- the object scales the stride by two and adds -- but it
	 * walks below `d_coeff`, and this harness does not read outside its
	 * own arrays without a backing region to read into.  Compare
	 * `t_fpm_lmsupd`'s `FPM_block_update` case, where the out-of-contract
	 * index IS driven because `hist` was placed in the middle of a padded
	 * array for exactly that purpose.
	 */

	/* The shift, over its whole usable range.  `shift == 3` leaves the
	 * sum unshifted, which is where a residual applied in the wrong
	 * direction would show. */
	for (i = 3; i <= 20; i++)
		dotp_case(20, DTAPS, 1, (short)i);

	/* A sweep of lengths, so no tap count is special. */
	for (i = 1; i <= DTAPS; i += 7)
		for (j = -1; j < i; j += 5)
			dotp_case((short)j, (short)i, 1, 17);

	rc |= diff_end();

	/*
	 * Non-vacuity.  A run in which every product's low three bits were
	 * already zero would not distinguish per-product truncation from one
	 * shift at the end, and a run whose every answer was zero would not
	 * distinguish anything at all.
	 */
	diff_begin("FPM_circ_dotp2 reached its edge cases");
	diff_eq_int("per-product truncation fired (%ld times)",
		    dotp_trunc_seen > 0, 1, dotp_trunc_seen);
	diff_eq_int("non-zero results seen (%ld)", dotp_nonzero_seen > 0, 1,
		    dotp_nonzero_seen);
	rc |= diff_end();

	return rc;
}
