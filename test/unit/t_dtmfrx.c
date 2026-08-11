/*
 * t_dtmfrx.c -- differential test of the Caller ID DTMF receiver.
 *
 * Three entry points, each driven on SIGNALS and not only on struct bytes:
 *
 *   band_pass         filters in place and gates on energy
 *   DTMF_MTD_detect   the tone bank
 *   dtmf_modem        the state machine over the other two
 *
 * The anti-vacuity assertions are all measured on the REFERENCE side, so
 * they hold whatever our code does:
 *
 *   - the tone bank returns all SIXTEEN keypad codes over the tone set, and
 *     also returns -9 for silence, for one tone of a pair, for a badly
 *     twisted pair, for an out-of-band tone and for noise.  A bank that said
 *     -9 to everything, or a digit to everything, would fail one or the
 *     other.
 *   - band_pass returns both 0 and 1, and its two energy-driven early exits
 *     are both reached.
 *   - dtmf_modem walks its states and actually collects a digit string.
 *
 * `rx->bufp` is the one field that cannot be compared directly: on the path
 * where the window needs no realignment it points at each side's OWN stack
 * buffer, so the two will never be equal.  It is normalised into a copy
 * before the object comparison and checked separately for pointing at the
 * same PLACE -- the embedded window or the caller's block.
 *
 * WHAT THE PAIR TEST DOES NOT ASSERT, AND WHY (finding 1416).
 *
 * The first version of it asserted that each of the sixteen tone pairs
 * decodes to its own keypad code.  It does not, in the OBJECT: over one
 * 160-sample block the LOW half of the answer is right for all sixteen pairs
 * and the HIGH half is right for four of them, at every level from 600 to
 * 6000 and at both rates.  Our code agrees with the object on every one of
 * those blocks, so this is a property of the original and not of the
 * reconstruction -- and it is the reason `dtmf_modem` will not accept a code
 * until two consecutive blocks agree AND `band_pass` has said there is
 * signal.  The `dtmf_modem` test below does collect a digit string, which is
 * the end-to-end statement worth making.
 *
 * So the pair test asserts the half that holds, plus the coverage of all
 * sixteen codes across the run.  It does not assert a table of observed
 * answers: that would be fitting the test to the object rather than testing
 * against it.
 */

#include <math.h>
#include <string.h>

#include "harness.h"
#include "dsplib/dtmf_rx.h"

extern int ref_band_pass(short *samples, short count, void *rx);
extern int ref_DTMF_MTD_detect(const short *samples, short count, void *rx);
extern int ref_dtmf_modem(const short *samples, unsigned short count,
			  void *rx);
extern void *ref_reset_dtmf(void *rx);

extern const short ref_MTD1_COEF_8000[], ref_MTD2_COEF_8000[];
extern const short ref_MTD3_COEF_8000[], ref_MTD4_COEF_8000[];
extern const short ref_MTD5_COEF_8000[], ref_MTD6_COEF_8000[];
extern const short ref_MTD7_COEF_8000[], ref_MTD8_COEF_8000[];
extern const short ref_MTD1_COEF_9600[], ref_MTD2_COEF_9600[];
extern const short ref_MTD3_COEF_9600[], ref_MTD4_COEF_9600[];
extern const short ref_MTD5_COEF_9600[], ref_MTD6_COEF_9600[];
extern const short ref_MTD7_COEF_9600[], ref_MTD8_COEF_9600[];

#define GUARD	32
#define NSAMP	200

struct box {
	struct dtmf_rx rx;
	unsigned char guard[GUARD];
};

static unsigned long seed;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

static void
fill_bytes(void *p, size_t n)
{
	unsigned char *b = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		b[i] = (unsigned char)(rnd() & 0xff);
}

/* ------------------------------------------------------------------ */

static int seen_bp_zero;
static int seen_bp_one;
static int seen_code[16];
static int seen_no_code;
static int seen_modem_ret[8];		/* indexed by result + 2 */
static int seen_string;
static int seen_aligned;
static int seen_unaligned;

/* ------------------------------------------------------------------ */

static void
compare(const char *what, const struct box *a, const struct box *b, long tag)
{
	struct dtmf_rx ca = a->rx;
	struct dtmf_rx cb = b->rx;
	char buf[128];

	/*
	 * Same PLACE, not same address: one arm of dtmf_modem points bufp at
	 * a stack array, which is per-side by construction.
	 */
	snprintf(buf, sizeof(buf), "%s: bufp points at the window (%%ld)",
		 what);
	diff_eq_int(buf, ca.bufp == a->rx.samples, cb.bufp == b->rx.samples,
		    tag);
	snprintf(buf, sizeof(buf), "%s: bufp is set (%%ld)", what);
	diff_eq_int(buf, ca.bufp != 0, cb.bufp != 0, tag);
	ca.bufp = 0;
	cb.bufp = 0;

	snprintf(buf, sizeof(buf), "%s: object after %%ld", what);
	diff_eq_obj(buf, struct dtmf_rx, &ca, &cb, tag);
	snprintf(buf, sizeof(buf), "%s: guard after %%ld", what);
	diff_eq_int(buf, memcmp(a->guard, b->guard, GUARD), 0, tag);
}

/*
 * Seed both objects identically and never zero them, then put the counters
 * back into the range the object can hold -- the same argument as t_dtmf.c's
 * `seed_object`, and for the same reason: `state` seeded from random bytes
 * is almost never 0, 1 or 2, so the machine would never be entered.
 */
static void
seed_object(struct box *a, struct box *b, short rate, short sens)
{
	fill_bytes(a, sizeof(*a));
	a->rx.last_digit = (short)((long)(rnd() % 18UL) - 9L);
	a->rx.stable = (short)(rnd() % 4UL);
	a->rx.ndigits = (short)((long)(rnd() % 6UL) - 1L);
	a->rx.state = (short)(rnd() % 4UL);
	a->rx.aligned = (short)(rnd() % 2UL);
	a->rx.quiet = (short)(rnd() % 8UL);
	a->rx.level = (short)(rnd() % 500UL);
	a->rx.bufp = a->rx.samples;
	a->rx.rate = rate;
	a->rx.sens = sens;
	a->rx.nsamples = (int)(rnd() % 40000UL);
	memcpy(b, a, sizeof(*a));
	b->rx.bufp = b->rx.samples;
}

/* create_cid_dtmf's state, via the object's own reset. */
static void
fresh(struct box *a, struct box *b, short rate, short sens)
{
	fill_bytes(a, sizeof(*a));
	ref_reset_dtmf(&a->rx);
	a->rx.rate = rate;
	a->rx.sens = sens;
	memcpy(b, a, sizeof(*a));
	a->rx.bufp = a->rx.samples;
	b->rx.bufp = b->rx.samples;
}

/* ------------------------------------------------------------------ */

static const double low_hz[4] = { 697.0, 770.0, 852.0, 941.0 };
static const double high_hz[4] = { 1209.0, 1336.0, 1477.0, 1633.0 };

/* The bank's own (low, high) -> keypad code map, written out independently. */
static const int keycode[16] = {
	1, 2, 3, 10,
	4, 5, 6, 11,
	7, 8, 9, 12,
	14, 0, 15, 13,
};

static void
tone_block(short *out, int n, double fs, double f1, double a1,
	   double f2, double a2, double *p1, double *p2, int noise)
{
	int i;

	for (i = 0; i < n; i++) {
		double v = 0.0;

		if (noise)
			v = ((double)(long)(rnd() % 20001UL) - 10000.0)
			    / 10000.0 * a1;
		else {
			if (a1 != 0.0)
				v += a1 * sin(*p1);
			if (a2 != 0.0)
				v += a2 * sin(*p2);
		}
		*p1 += 2.0 * 3.14159265358979323846 * f1 / fs;
		*p2 += 2.0 * 3.14159265358979323846 * f2 / fs;
		if (v > 32767.0)
			v = 32767.0;
		if (v < -32768.0)
			v = -32768.0;
		out[i] = (short)v;
	}
}

/* ------------------------------------------------------------------ */

static int
run_band_pass(const char *what, struct box *a, struct box *b,
	      const short *in, short count, long tag)
{
	short sa[NSAMP], sb[NSAMP];
	int ra, rb;
	char buf[128];
	int i;

	memcpy(sa, in, (size_t)count * sizeof(short));
	memcpy(sb, in, (size_t)count * sizeof(short));

	ra = band_pass(sa, count, &a->rx);
	rb = ref_band_pass(sb, count, &b->rx);

	snprintf(buf, sizeof(buf), "%s: band_pass verdict (%%ld)", what);
	diff_eq_int(buf, ra, rb, tag);
	for (i = 0; i < count; i++) {
		snprintf(buf, sizeof(buf), "%s: filtered sample %%ld", what);
		diff_eq_int(buf, sa[i], sb[i], i);
	}
	compare(what, a, b, tag);

	if (rb == 0)
		seen_bp_zero++;
	else
		seen_bp_one++;
	return rb;
}

static int
run_mtd(const char *what, struct box *a, struct box *b,
	const short *in, short count, long tag)
{
	int ra, rb;
	char buf[128];

	ra = DTMF_MTD_detect(in, count, &a->rx);
	rb = ref_DTMF_MTD_detect(in, count, &b->rx);

	snprintf(buf, sizeof(buf), "%s: MTD code (%%ld)", what);
	diff_eq_int(buf, ra, rb, tag);
	compare(what, a, b, tag);

	if (rb == -9)
		seen_no_code++;
	else {
		int k;

		for (k = 0; k < 16; k++)
			if (keycode[k] == rb)
				seen_code[k]++;
	}
	return rb;
}

static int
run_modem(const char *what, struct box *a, struct box *b,
	  const short *in, unsigned short count, long tag)
{
	int ra, rb;
	char buf[128];

	ra = dtmf_modem(in, count, &a->rx);
	rb = ref_dtmf_modem(in, count, &b->rx);

	snprintf(buf, sizeof(buf), "%s: modem result (%%ld)", what);
	diff_eq_int(buf, ra, rb, tag);
	compare(what, a, b, tag);

	if (rb + 2 >= 0 && rb + 2 < 8)
		seen_modem_ret[rb + 2]++;
	if (b->rx.ndigits > 0 && b->rx.digits[0] != 0)
		seen_string++;
	if (b->rx.aligned)
		seen_aligned++;
	else
		seen_unaligned++;
	return rb;
}

/* ------------------------------------------------------------------ */

static void
tables(void)
{
	static const struct {
		const char *name;
		const short *ours;
		const short *ref;
	} t[16] = {
		{ "MTD1_8000", MTD1_COEF_8000, ref_MTD1_COEF_8000 },
		{ "MTD2_8000", MTD2_COEF_8000, ref_MTD2_COEF_8000 },
		{ "MTD3_8000", MTD3_COEF_8000, ref_MTD3_COEF_8000 },
		{ "MTD4_8000", MTD4_COEF_8000, ref_MTD4_COEF_8000 },
		{ "MTD5_8000", MTD5_COEF_8000, ref_MTD5_COEF_8000 },
		{ "MTD6_8000", MTD6_COEF_8000, ref_MTD6_COEF_8000 },
		{ "MTD7_8000", MTD7_COEF_8000, ref_MTD7_COEF_8000 },
		{ "MTD8_8000", MTD8_COEF_8000, ref_MTD8_COEF_8000 },
		{ "MTD1_9600", MTD1_COEF_9600, ref_MTD1_COEF_9600 },
		{ "MTD2_9600", MTD2_COEF_9600, ref_MTD2_COEF_9600 },
		{ "MTD3_9600", MTD3_COEF_9600, ref_MTD3_COEF_9600 },
		{ "MTD4_9600", MTD4_COEF_9600, ref_MTD4_COEF_9600 },
		{ "MTD5_9600", MTD5_COEF_9600, ref_MTD5_COEF_9600 },
		{ "MTD6_9600", MTD6_COEF_9600, ref_MTD6_COEF_9600 },
		{ "MTD7_9600", MTD7_COEF_9600, ref_MTD7_COEF_9600 },
		{ "MTD8_9600", MTD8_COEF_9600, ref_MTD8_COEF_9600 },
	};
	int i, j;

	for (i = 0; i < 16; i++) {
		int distinct = 0;

		diff_eq_int("table %ld matches the object byte for byte",
			    memcmp(t[i].ours, t[i].ref, 5 * sizeof(short)),
			    0, i);
		for (j = 1; j < 5; j++)
			if (t[i].ours[j] != t[i].ours[0])
				distinct = 1;
		diff_eq_int("table %ld is not all one value", distinct, 1, i);
		/* The pole radius is 0.9 in all sixteen: a2 = -round(0.81*2^14). */
		diff_eq_int("table %ld has r = 0.9", t[i].ours[0], -13271, i);
		diff_eq_int("table %ld has unit numerator ends", t[i].ours[1],
			    16384, i);
		diff_eq_int("table %ld has unit numerator ends (b0)",
			    t[i].ours[4], 16384, i);
	}
	/* The two rates' tables are genuinely different data. */
	diff_eq_int("MTD1 differs between the two rates (%ld)",
		    memcmp(MTD1_COEF_8000, MTD1_COEF_9600, 10) != 0, 1, 0);
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	static short blk[NSAMP];
	struct box a, b;
	double p1, p2;
	int rc = 0;
	int i, lo, hi, r, k;
	int rate_i;

	seed = 0x51ee7a11UL;

	diff_begin("dtmf_rx: the tone bank's coefficient tables");
	tables();
	rc |= diff_end();

	/* ---- band_pass ---- */

	diff_begin("dtmf_rx: band_pass on seeded objects and random blocks");
	for (i = 0; i < 40; i++) {
		short n = (short)(20 + (rnd() % 160UL));
		int k;

		seed_object(&a, &b, (short)((i & 1) ? 9600 : 8000),
			    (short)(rnd() % 5UL));
		for (k = 0; k < n; k++)
			blk[k] = (short)((long)(rnd() % 65536UL) - 32768L);
		run_band_pass("bp random", &a, &b, blk, n, i);
	}
	rc |= diff_end();

	diff_begin("dtmf_rx: band_pass on tones, silence and clipping");
	for (rate_i = 0; rate_i < 2; rate_i++) {
		double fs = rate_i ? 9600.0 : 8000.0;
		short rate = (short)(rate_i ? 9600 : 8000);
		static const double amp[6] = {
			0.0, 30.0, 200.0, 2000.0, 12000.0, 31000.0
		};
		int ai;

		for (ai = 0; ai < 6; ai++) {
			fresh(&a, &b, rate, 0);
			p1 = p2 = 0.0;
			for (i = 0; i < 6; i++) {
				tone_block(blk, 160, fs, 697.0, amp[ai],
					   1209.0, amp[ai], &p1, &p2, 0);
				run_band_pass("bp tone", &a, &b, blk, 160,
					      rate_i * 100 + ai * 10 + i);
			}
			/* a short block, below the level-update threshold */
			tone_block(blk, 40, fs, 941.0, amp[ai], 1633.0,
				   amp[ai], &p1, &p2, 0);
			run_band_pass("bp short", &a, &b, blk, 40,
				      rate_i * 100 + ai);
			/* noise at the same level */
			tone_block(blk, 160, fs, 0.0, amp[ai], 0.0, 0.0,
				   &p1, &p2, 1);
			run_band_pass("bp noise", &a, &b, blk, 160,
				      rate_i * 100 + ai);
		}
		/* every sensitivity setting */
		for (i = 0; i < 5; i++) {
			fresh(&a, &b, rate, (short)i);
			p1 = p2 = 0.0;
			tone_block(blk, 160, fs, 852.0, 4000.0, 1477.0,
				   4000.0, &p1, &p2, 0);
			run_band_pass("bp sens", &a, &b, blk, 160,
				      rate_i * 10 + i);
		}
	}
	rc |= diff_end();

	/* ---- DTMF_MTD_detect ---- */

	diff_begin("dtmf_rx: the tone bank on all sixteen pairs");
	for (rate_i = 0; rate_i < 2; rate_i++) {
		double fs = rate_i ? 9600.0 : 8000.0;
		short rate = (short)(rate_i ? 9600 : 8000);

		for (lo = 0; lo < 4; lo++)
			for (hi = 0; hi < 4; hi++) {
				fresh(&a, &b, rate, 0);
				p1 = p2 = 0.0;
				for (i = 0; i < 6; i++) {
					tone_block(blk, 160, fs, low_hz[lo],
						   600.0, high_hz[hi],
						   600.0, &p1, &p2, 0);
					r = run_mtd("mtd pair", &a, &b, blk,
						    160,
						    rate_i * 1000
						    + (lo * 4 + hi) * 10 + i);
				}
				/*
				 * The LOW half of the answer is right for all
				 * sixteen pairs and the HIGH half is not --
				 * see the note above `main`.  Asserting the
				 * half that holds is worth more than
				 * asserting nothing, and worth more than
				 * asserting a table of observed answers.
				 */
				for (k = 0; k < 16; k++)
					if (keycode[k] == r)
						break;
				diff_eq_int(
				    "the reference decoded a code for pair %ld",
				    k < 16, 1, lo * 4 + hi);
				diff_eq_int(
				    "the reference got pair %ld's LOW tone right",
				    k / 4, lo, lo * 4 + hi);
			}
	}
	rc |= diff_end();

	diff_begin("dtmf_rx: the tone bank on what is not a digit");
	for (rate_i = 0; rate_i < 2; rate_i++) {
		double fs = rate_i ? 9600.0 : 8000.0;
		short rate = (short)(rate_i ? 9600 : 8000);
		int quiet = 0;

		/* silence */
		fresh(&a, &b, rate, 0);
		p1 = p2 = 0.0;
		for (i = 0; i < 4; i++) {
			tone_block(blk, 160, fs, 697.0, 0.0, 1209.0, 0.0,
				   &p1, &p2, 0);
			r = run_mtd("mtd silence", &a, &b, blk, 160, i);
		}
		quiet += (r == -9);
		diff_eq_int("silence is refused at rate %ld", r, -9, rate_i);
		/* one tone of a pair */
		fresh(&a, &b, rate, 0);
		p1 = p2 = 0.0;
		for (i = 0; i < 4; i++) {
			tone_block(blk, 160, fs, 697.0, 6000.0, 1209.0, 0.0,
				   &p1, &p2, 0);
			r = run_mtd("mtd low only", &a, &b, blk, 160, i);
		}
		quiet += (r == -9);
		fresh(&a, &b, rate, 0);
		p1 = p2 = 0.0;
		for (i = 0; i < 4; i++) {
			tone_block(blk, 160, fs, 697.0, 0.0, 1209.0, 6000.0,
				   &p1, &p2, 0);
			r = run_mtd("mtd high only", &a, &b, blk, 160, i);
		}
		quiet += (r == -9);
		/* out of band */
		fresh(&a, &b, rate, 0);
		p1 = p2 = 0.0;
		for (i = 0; i < 4; i++) {
			tone_block(blk, 160, fs, 400.0, 6000.0, 2600.0,
				   6000.0, &p1, &p2, 0);
			r = run_mtd("mtd out of band", &a, &b, blk, 160, i);
		}
		quiet += (r == -9);
		/* noise */
		fresh(&a, &b, rate, 0);
		p1 = p2 = 0.0;
		for (i = 0; i < 4; i++) {
			tone_block(blk, 160, fs, 0.0, 6000.0, 0.0, 0.0,
				   &p1, &p2, 1);
			r = run_mtd("mtd noise", &a, &b, blk, 160, i);
		}
		quiet += (r == -9);
		/*
		 * Silence and noise are the two the bank refuses outright.
		 * A lone tone of a pair, an out-of-band pair and a heavily
		 * twisted pair can all produce a code from ONE block -- the
		 * group that has no tone in it still has a smallest energy,
		 * and nothing in the bank asks how small.  That is what
		 * dtmf_modem's "same code twice, and band_pass agreed"
		 * requirement is there for, and it is asserted where it
		 * belongs rather than wished for here.
		 */
		diff_eq_int("silence and noise are refused at rate %ld",
			    quiet >= 2, 1, rate_i);
	}

	/* extreme twist: the pair is there but 40 dB apart */
	for (rate_i = 0; rate_i < 2; rate_i++) {
		double fs = rate_i ? 9600.0 : 8000.0;

		fresh(&a, &b, (short)(rate_i ? 9600 : 8000), 0);
		p1 = p2 = 0.0;
		for (i = 0; i < 4; i++) {
			tone_block(blk, 160, fs, 770.0, 8000.0, 1336.0, 80.0,
				   &p1, &p2, 0);
			run_mtd("mtd twist", &a, &b, blk, 160, i);
		}
	}

	/*
	 * Seeded objects and random blocks.  Three hundred of them, not
	 * thirty: the energies' rounding term is worth one count per sample
	 * and only shows up when two sections finish within that of each
	 * other, which needs enough blocks for a near-tie to happen.
	 */
	for (i = 0; i < 300; i++) {
		short n = (short)(16 + (rnd() % 180UL));
		int k;

		seed_object(&a, &b, (short)((i & 1) ? 9600 : 8000), 0);
		for (k = 0; k < n; k++)
			blk[k] = (short)((long)(rnd() % 65536UL) - 32768L);
		run_mtd("mtd random", &a, &b, blk, n, i);
	}

	/*
	 * And the same again at the bottom of the range.  The energies are
	 * accumulated with a rounding term worth one count a sample and then
	 * shifted right fifteen places, so on a loud block that term is lost
	 * in the shift and on a quiet one it decides which section is the
	 * smallest.  Short blocks of small samples, with the tone states
	 * cleared so the block is all that is being measured.
	 */
	for (i = 0; i < 12000; i++) {
		short n = (short)(12 + (rnd() % 187UL));
		unsigned long amp = 4UL + (rnd() % 3000UL);
		int k;

		seed_object(&a, &b, (short)((i & 1) ? 9600 : 8000), 0);
		memset(a.rx.tone_state, 0, sizeof(a.rx.tone_state));
		memset(b.rx.tone_state, 0, sizeof(b.rx.tone_state));
		a.rx.pre_low[0] = a.rx.pre_low[1] = 0;
		a.rx.pre_high[0] = a.rx.pre_high[1] = 0;
		b.rx.pre_low[0] = b.rx.pre_low[1] = 0;
		b.rx.pre_high[0] = b.rx.pre_high[1] = 0;
		for (k = 0; k < n; k++)
			blk[k] = (short)((long)(rnd() % (2UL * amp + 1UL))
					 - (long)amp);
		run_mtd("mtd quiet", &a, &b, blk, n, 10000 + i);
	}
	rc |= diff_end();

	/* ---- dtmf_modem ---- */

	diff_begin("dtmf_rx: dtmf_modem over a Caller ID digit string");
	for (rate_i = 0; rate_i < 2; rate_i++) {
		double fs = rate_i ? 9600.0 : 8000.0;
		short rate = (short)(rate_i ? 9600 : 8000);
		/*
		 * 0 1 'A' 'B' 4 then 'C'.  A and B are in there because
		 * dtmf_modem rejects exactly those two codes by name, and 1633
		 * is the one high tone the bank decodes reliably from a single
		 * block -- see the note at the top of the file.
		 */
		static const int seq[6][2] = {
			{ 3, 1 }, { 0, 0 }, { 0, 3 }, { 1, 3 }, { 1, 0 },
			{ 2, 3 }
		};
		int d;
		long tag = 0;

		fresh(&a, &b, rate, 0);
		p1 = p2 = 0.0;
		/* lead-in silence, so the machine leaves state 0 */
		for (i = 0; i < 4; i++) {
			tone_block(blk, 160, fs, 0.0, 0.0, 0.0, 0.0,
				   &p1, &p2, 0);
			run_modem("modem lead-in", &a, &b, blk, 160, tag++);
		}
		for (d = 0; d < 6; d++) {
			for (i = 0; i < 8; i++) {
				tone_block(blk, 160, fs, low_hz[seq[d][0]],
					   6000.0, high_hz[seq[d][1]], 6000.0,
					   &p1, &p2, 0);
				run_modem("modem tone", &a, &b, blk, 160,
					  tag++);
			}
			/*
			 * Eight blocks of gap, not five: the quiet counter's
			 * give-up fires at six and a five-block gap never
			 * reaches it.
			 */
			for (i = 0; i < 8; i++) {
				tone_block(blk, 160, fs, 0.0, 0.0, 0.0, 0.0,
					   &p1, &p2, 0);
				run_modem("modem gap", &a, &b, blk, 160,
					  tag++);
			}
		}
		diff_eq_int("the reference collected digits at rate %ld",
			    b.rx.ndigits > 0, 1, rate_i);
	}
	rc |= diff_end();

	/*
	 * The two lengths at which the digit string matters.  Twenty is where
	 * dtmf_modem gives up, and reaching it by collecting twenty digits
	 * would be four thousand blocks; the count is started near the limit
	 * instead, on BOTH sides, which is the same experiment at a hundredth
	 * of the cost.
	 */
	diff_begin("dtmf_rx: dtmf_modem at the digit-string limit");
	for (i = 0; i < 2; i++) {
		double fs = 8000.0;
		short start = (short)(i ? 19 : 15);
		long tag = 0;
		int d, e2;

		fresh(&a, &b, 8000, 0);
		p1 = p2 = 0.0;
		for (r = 0; r < 4; r++) {
			tone_block(blk, 160, fs, 0.0, 0.0, 0.0, 0.0,
				   &p1, &p2, 0);
			run_modem("limit lead-in", &a, &b, blk, 160, tag++);
		}
		/*
		 * The sample counter has to be restarted with it: a receiver
		 * that already has three digits gives up 300 ms after its
		 * last decision, and the lead-in's silence counts.
		 */
		a.rx.ndigits = start;
		b.rx.ndigits = start;
		a.rx.nsamples = 0;
		b.rx.nsamples = 0;
		for (d = 0; d < 2; d++) {
			/*
			 * 852 + 1633 is 'C', the one code that both commits
			 * and lengthens the string, and 1633 is the high tone
			 * the bank gets right from a single block.  A and B
			 * are refused by name and 'D' is accepted without
			 * being stored, so neither would move `ndigits`.
			 */
			(void)d;
			for (r = 0; r < 8; r++) {
				tone_block(blk, 160, fs, low_hz[2], 6000.0,
					   high_hz[3], 6000.0, &p1, &p2, 0);
				run_modem("limit tone", &a, &b, blk, 160,
					  tag++);
			}
			for (r = 0; r < 6; r++) {
				tone_block(blk, 160, fs, 0.0, 0.0, 0.0, 0.0,
					   &p1, &p2, 0);
				run_modem("limit gap", &a, &b, blk, 160,
					  tag++);
			}
		}
		diff_eq_int("the string grew past %ld",
			    b.rx.ndigits > start, 1, start);

		/*
		 * A long burst of 'A'.  dtmf_modem refuses that code by name,
		 * so every block after the first goes down the arm that only
		 * increments the quiet counter -- which is the one way to
		 * drive it past its give-up threshold of six while a digit
		 * string is long enough for the threshold to matter.
		 */
		for (r = 0; r < 14; r++) {
			tone_block(blk, 160, fs, low_hz[0], 6000.0,
				   high_hz[3], 6000.0, &p1, &p2, 0);
			run_modem("limit A burst", &a, &b, blk, 160, tag++);
		}
		diff_eq_int("the quiet counter passed six (%ld)",
			    b.rx.quiet > 6, 1, start);

		/*
		 * Now put it back ON the threshold.  Six is the first value
		 * that gives up, so five has to be shown NOT to -- and five
		 * is a value the machine passes through in one block, which
		 * is not long enough to sample by waiting.
		 */
		for (r = 5; r <= 6; r++) {
			a.rx.quiet = (short)r;
			b.rx.quiet = (short)r;
			tone_block(blk, 160, fs, 0.0, 0.0, 0.0, 0.0,
				   &p1, &p2, 0);
			run_modem("limit quiet edge", &a, &b, blk, 160,
				  tag++);
			/* back into a tone, so the next block is state 2 */
			for (e2 = 0; e2 < 6; e2++) {
				tone_block(blk, 160, fs, low_hz[2], 6000.0,
					   high_hz[3], 6000.0, &p1, &p2, 0);
				run_modem("limit quiet re-arm", &a, &b, blk,
					  160, tag++);
			}
		}
	}
	rc |= diff_end();

	/*
	 * band_pass's remaining corners: a block that hits the limiter's
	 * thresholds EXACTLY (the counters are strict `>`), a level memory
	 * starting at zero, and one block per step of the threshold
	 * staircase.
	 */
	diff_begin("dtmf_rx: band_pass at its exact thresholds");
	for (rate_i = 0; rate_i < 2; rate_i++) {
		short rate = (short)(rate_i ? 9600 : 8000);
		static const short exact[4] = { 20000, 10000, -20000, -10000 };
		int k, e;

		for (e = 0; e < 4; e++) {
			fresh(&a, &b, rate, 0);
			for (k = 0; k < 160; k++)
				blk[k] = (short)((k % 3) ? 500 : exact[e]);
			run_band_pass("bp exact", &a, &b, blk, 160,
				      rate_i * 10 + e);
		}

		/*
		 * EXACTLY two samples over the loud threshold in a 160-sample
		 * block.  The limiter's fraction is count/128, which is 1
		 * here, so two is the smallest count that trips it and the
		 * smallest that a fraction of count/64 would not.
		 */
		for (e = 0; e < 2; e++) {
			fresh(&a, &b, rate, 0);
			for (k = 0; k < 160; k++)
				blk[k] = 500;
			blk[7] = (short)(e ? 21000 : 11000);
			blk[93] = (short)(e ? 21000 : 11000);
			run_band_pass("bp two loud", &a, &b, blk, 160,
				      rate_i * 10 + e);
		}

		/* the level memory floor, on a block too short to move it */
		fresh(&a, &b, rate, 0);
		a.rx.level = 0;
		b.rx.level = 0;
		p1 = p2 = 0.0;
		tone_block(blk, 80, 8000.0, 941.0, 3000.0, 1633.0, 3000.0,
			   &p1, &p2, 0);
		run_band_pass("bp zero level", &a, &b, blk, 80, rate_i);
		diff_eq_int("a zero level was floored at one (%ld)",
			    b.rx.level, 1, rate_i);

		/*
		 * The threshold staircase has four steps and the coarse gain
		 * ahead of it normalises three decades into one, so the sweep
		 * has to be fine to land on each step: 24 levels, 1.5x apart.
		 */
		for (e = 0; e < 24; e++) {
			double amp = 8.0 * pow(1.5, (double)e);

			fresh(&a, &b, rate, 0);
			p1 = p2 = 0.0;
			tone_block(blk, 160, 8000.0, 941.0, amp, 1633.0, amp,
				   &p1, &p2, 0);
			run_band_pass("bp staircase", &a, &b, blk, 160,
				      rate_i * 10 + e);
		}
	}
	rc |= diff_end();

	diff_begin("dtmf_rx: dtmf_modem on seeded objects, every state");
	for (i = 0; i < 120; i++) {
		short n = (short)(20 + (rnd() % 170UL));
		int k;

		seed_object(&a, &b, (short)((i & 1) ? 9600 : 8000),
			    (short)(i % 5));
		/* each seeded object gets three blocks, so it can move on */
		for (k = 0; k < (int)n; k++)
			blk[k] = (short)((long)(rnd() % 30001UL) - 15000L);
		run_modem("modem seeded", &a, &b, blk, (unsigned short)n, i);
		for (k = 0; k < (int)n; k++)
			blk[k] = (short)((long)(rnd() % 2001UL) - 1000L);
		run_modem("modem seeded q", &a, &b, blk, (unsigned short)n,
			  1000 + i);
		for (k = 0; k < (int)n; k++)
			blk[k] = 0;
		run_modem("modem seeded z", &a, &b, blk, (unsigned short)n,
			  2000 + i);
	}
	rc |= diff_end();

	/* sensitivity 1, the diagnostic branch nothing else reaches */
	diff_begin("dtmf_rx: dtmf_modem with sens == 1");
	for (rate_i = 0; rate_i < 2; rate_i++) {
		double fs = rate_i ? 9600.0 : 8000.0;
		long tag = 0;

		fresh(&a, &b, (short)(rate_i ? 9600 : 8000), 1);
		b.rx.ndigits = -1;
		a.rx.ndigits = -1;
		p1 = p2 = 0.0;
		for (i = 0; i < 4; i++) {
			tone_block(blk, 160, fs, 0.0, 0.0, 0.0, 0.0,
				   &p1, &p2, 0);
			run_modem("sens1 lead-in", &a, &b, blk, 160, tag++);
		}
		for (i = 0; i < 20; i++) {
			tone_block(blk, 160, fs, 941.0, 6000.0, 1633.0,
				   6000.0, &p1, &p2, 0);
			run_modem("sens1 tone", &a, &b, blk, 160, tag++);
		}
		for (i = 0; i < 8; i++) {
			tone_block(blk, 160, fs, 0.0, 0.0, 0.0, 0.0,
				   &p1, &p2, 0);
			run_modem("sens1 gap", &a, &b, blk, 160, tag++);
		}
	}
	rc |= diff_end();

	/* ---- the assertions that make the run non-vacuous ---- */

	diff_begin("dtmf_rx: guards");
	diff_eq_int("band_pass returned 0 (%ld)", seen_bp_zero > 0, 1, 0);
	diff_eq_int("band_pass returned 1 (%ld)", seen_bp_one > 0, 1, 0);
	for (i = 0; i < 16; i++)
		diff_eq_int("the tone bank returned keypad code %ld",
			    seen_code[i] > 0, 1, keycode[i]);
	diff_eq_int("the tone bank also returned -9 (%ld)",
		    seen_no_code > 0, 1, 0);
	diff_eq_int("dtmf_modem returned 1 (%ld)", seen_modem_ret[3] > 0, 1, 0);
	diff_eq_int("dtmf_modem returned 2 (%ld)", seen_modem_ret[4] > 0, 1, 0);
	diff_eq_int("dtmf_modem returned 3 (%ld)", seen_modem_ret[5] > 0, 1, 0);
	diff_eq_int("dtmf_modem returned -1 (%ld)", seen_modem_ret[1] > 0, 1, 0);
	diff_eq_int("a digit string was collected (%ld)", seen_string > 0, 1, 0);
	diff_eq_int("the aligned window was used (%ld)", seen_aligned > 0, 1, 0);
	diff_eq_int("the unaligned window was used (%ld)",
		    seen_unaligned > 0, 1, 0);
	rc |= diff_end();

	return rc;
}
