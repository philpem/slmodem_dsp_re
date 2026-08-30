/*
 * t_v22det.c -- differential test of V.22's pattern generator and its three
 * pattern detectors.
 *
 * All four are pure functions of an array and a count read through a pointer,
 * so unlike the wrappers in t_v22data.c there is nothing to get wrong about
 * WHICH object is touched and everything to get wrong about the arithmetic.
 * What this test is built around, therefore, is the sixteen-bit wrap: the
 * three accumulators inside every detector are `short` and truncate on every
 * iteration, and an `int` accumulator agrees with them over every short input
 * and disagrees over every long one.  A sweep that never ran past 145 symbols
 * at 2400 bit/s would pass against a wrong reconstruction.
 *
 * So the sweeps go to 8,192 symbols -- Detect_Retrain correlates against 3
 * and needs about 7,282 before ITS accumulators wrap, an order of magnitude
 * more than the other two -- and the coverage guards at the bottom assert
 * that each accumulator was actually seen to wrap, that each detector
 * answered both ways, and that both rate arms were taken.
 *
 * THE INPUTS ARE NOT UNIFORM RANDOM, and that is the point.  A correlator
 * fires on a stream that is CLOSE to its ideal, and uniform noise over
 * 0..0xffff is never close to anything -- a sweep made of it would exercise
 * only the "no match" arm and the guards below would catch that, but the
 * cheaper fix is to generate streams that are the ideal pattern with a
 * controlled number of symbols corrupted.  Four generators do that, one per
 * pattern the object looks for, plus pure noise for the negative case.
 *
 * MakeTxData is compared byte for byte over a buffer that is poisoned first,
 * so a generator writing one symbol too few or too many is separated.  Its
 * two pathological inputs are handled as follows:
 *
 *   - a NEGATIVE count runs 65535 writes rather than none.  Tested, with a
 *     buffer big enough to hold them, because it is the whole difference
 *     between the object's countdown and the `for (i = 0; i < n; i++)`
 *     anybody would write instead.
 *   - an ODD count on pattern 0 never terminates (see v22det.h).  NOT
 *     tested, and it cannot be: neither side returns.
 */

#include "harness.h"

#include "dsplib/v22det.h"
#include "dsplib/v22prc.h"

extern void ref_MakeTxData(short *out, const short *count, short pattern);
extern int ref_Detect_1s(const unsigned short *sym,
			 const unsigned short *count, short bps, short thresh);
extern int ref_Detect_Rmloop2_ACK(const unsigned short *sym,
				  const unsigned short *count, short bps);
extern int ref_Detect_Retrain(const unsigned short *sym,
			      const unsigned short *count);

/* ------------------------------------------------------------------------ */

static unsigned long rng_state;

static void
rng_seed(unsigned long s)
{
	rng_state = s | 1UL;
}

static unsigned
rng_next(void)
{
	rng_state = rng_state * 1103515245UL + 12345UL;
	return (unsigned)((rng_state >> 16) & 0xffffU);
}

/* ------------------------------------------------------------------------ */

#define MAXSYM		8192
#define POISON_SLACK	8

/*
 * Coverage counters.  Every one of these is asserted non-zero at the end: a
 * detector that only ever answered one way, or a sum that never left sixteen
 * bits, would agree with a wrong reconstruction over the whole sweep.
 */
static long fired_1s, quiet_1s;
static long fired_rm, quiet_rm;
static long fired_rt, quiet_rt;
static long rate_1200, rate_2400;
static long saw_wrap_energy;	/* n * ideal^2 past 32767                */
static long saw_wrap_sumsq;	/* likewise for the received energy      */
static long saw_long_run;	/* Detect_Retrain's period-2 run past 2  */
static long saw_wrap_retrain;	/* its own energy past 32767             */

/*
 * The three sums the detectors keep, computed here in 32 bits purely so the
 * guards can say whether the object's 16-bit versions were pushed past the
 * point where the two readings diverge.  Nothing in the comparison uses them.
 */
static void
note_wrap(const unsigned short *sym, unsigned short n, int ideal)
{
	long sumsq = 0;
	unsigned i;

	for (i = 0; i < n; i++)
		sumsq += (long)sym[i] * sym[i];
	if (sumsq > 32767L || sumsq < -32768L)
		saw_wrap_sumsq++;
	if ((long)n * ideal * ideal > 32767L)
		saw_wrap_energy++;
}

/* ------------------------------------------------------------------------ */

/*
 * Stream generators.  `noise` is how many of the n symbols are replaced by a
 * random value, so noise = 0 is the ideal pattern and noise = n is junk.
 */
static void
gen_constant(unsigned short *sym, unsigned n, unsigned noise, unsigned short v)
{
	unsigned i;

	for (i = 0; i < n; i++)
		sym[i] = v;
	for (i = 0; i < noise; i++)
		sym[rng_next() % (n ? n : 1)] = (unsigned short)(rng_next() & 15);
}

/* The period-2 stream Detect_Retrain's run counter is looking for. */
static void
gen_period2(unsigned short *sym, unsigned n, unsigned noise,
	    unsigned short even, unsigned short odd)
{
	unsigned i;

	for (i = 0; i < n; i++)
		sym[i] = (i & 1) ? odd : even;
	for (i = 0; i < noise; i++)
		sym[rng_next() % (n ? n : 1)] = (unsigned short)(rng_next() & 15);
}

static void
gen_noise(unsigned short *sym, unsigned n, unsigned mask)
{
	unsigned i;

	for (i = 0; i < n; i++)
		sym[i] = (unsigned short)(rng_next() & mask);
}

/* ------------------------------------------------------------------------ */

static void
check_1s(const unsigned short *sym, unsigned short n, short bps, short thresh,
	 long tag)
{
	int a = ref_Detect_1s(sym, &n, bps, thresh);
	int b = Detect_1s(sym, &n, bps, thresh);

	diff_eq_int("Detect_1s case %ld", b, a, tag);
	if (a)
		fired_1s++;
	else
		quiet_1s++;
	note_wrap(sym, n, bps == V22_DET_BPS_2400 ? V22_TRAINED_2400_SYMBOL
						  : V22_TRAINED_1200_SYMBOL);
}

static void
check_rm(const unsigned short *sym, unsigned short n, short bps, long tag)
{
	int a = ref_Detect_Rmloop2_ACK(sym, &n, bps);
	int b = Detect_Rmloop2_ACK(sym, &n, bps);

	diff_eq_int("Detect_Rmloop2_ACK case %ld", b, a, tag);
	if (a)
		fired_rm++;
	else
		quiet_rm++;
}

static void
check_rt(const unsigned short *sym, unsigned short n, long tag)
{
	int a = ref_Detect_Retrain(sym, &n);
	int b = Detect_Retrain(sym, &n);
	unsigned i;
	long run = 0;

	diff_eq_int("Detect_Retrain case %ld", b, a, tag);
	if (a)
		fired_rt++;
	else
		quiet_rt++;

	if ((long)((n + 1) / 2) * 9 > 32767L)
		saw_wrap_retrain++;

	for (i = 2; i + 1 < n; i += 2) {
		if (sym[i] == sym[i - 2] && sym[i + 1] == sym[i - 1])
			run++;
		else
			run = 0;
		if (run > V22_DET_RETRAIN_RUN)
			saw_long_run++;
	}
}

/* ------------------------------------------------------------------------ */

static const short bps_values[] = { 1200, 2400, 0, 600, 4800, -1 };

static int
run_detectors(void)
{
	static unsigned short sym[MAXSYM];
	static const unsigned lens[] = {
		0, 1, 2, 3, 4, 7, 8, 15, 16, 63, 64, 100, 144, 145, 146,
		200, 255, 256, 300, 399, 400,
		/*
		 * Long enough for Detect_Retrain's own accumulators to wrap:
		 * it correlates against 3, so its energy grows by 9 per EVEN
		 * symbol and needs about 7,282 symbols to pass 32767.  The
		 * other two reach the same point in a tenth of that, which is
		 * why the short lengths above suffice for them.
		 */
		7280, 7282, 8192
	};
	long tag = 0;
	unsigned li, b, noise, rep;

	diff_begin("Detect_1s / Detect_Rmloop2_ACK / Detect_Retrain");
	rng_seed(0x51ed5eedUL);

	for (li = 0; li < sizeof(lens) / sizeof(lens[0]); li++) {
		unsigned n = lens[li];

		for (b = 0; b < sizeof(bps_values) / sizeof(bps_values[0]); b++) {
			short bps = bps_values[b];

			if (bps == V22_DET_BPS_2400)
				rate_2400++;
			else
				rate_1200++;

			for (noise = 0; noise <= 4; noise++) {
				short th;

				/* Continuous ones, at each rate's symbol. */
				gen_constant(sym, n, noise,
					     bps == V22_DET_BPS_2400
					     ? V22_TRAINED_2400_SYMBOL
					     : V22_TRAINED_1200_SYMBOL);
				for (th = 0; th < 4; th++) {
					static const short ths[] = {
						0, 0x4000, 0x7eb8, 0x7fff
					};
					check_1s(sym, (unsigned short)n, bps,
						 ths[th], tag++);
				}
				check_1s(sym, (unsigned short)n, bps,
					 (short)-0x4000, tag++);
				check_rm(sym, (unsigned short)n, bps, tag++);
				check_rt(sym, (unsigned short)n, tag++);

				/* The RMLOOP2 acknowledgement's own symbol. */
				gen_constant(sym, n, noise,
					     bps == V22_DET_BPS_2400
					     ? V22_DET_RMLOOP2_2400
					     : V22_DET_RMLOOP2_1200);
				check_1s(sym, (unsigned short)n, bps,
					 0x4000, tag++);
				check_rm(sym, (unsigned short)n, bps, tag++);
				check_rt(sym, (unsigned short)n, tag++);

				/*
				 * THE STREAM DETECT_RETRAIN ACTUALLY FIRES ON,
				 * and it is not the obvious one.  Its energy
				 * runs over EVERY symbol while its correlation
				 * runs over the even ones only, so a stream
				 * whose quadrant is 3 throughout scores
				 * 2*corr - sumsq = 0 and never matches.  What
				 * matches is quadrant 3 on the even symbols
				 * and quadrant 0 on the odd ones, which makes
				 * the two sums equal and the difference exactly
				 * the ideal energy.
				 */
				gen_period2(sym, n, noise, 0x0c, 0x00);
				check_rt(sym, (unsigned short)n, tag++);
				check_rm(sym, (unsigned short)n, bps, tag++);

				gen_period2(sym, n, noise, 0x0f, 0x02);
				check_rt(sym, (unsigned short)n, tag++);

				/* Quadrant 3 on both halves: must NOT fire. */
				gen_period2(sym, n, noise, 0x0c, 0x0d);
				check_rt(sym, (unsigned short)n, tag++);

				/* Not period two at all. */
				gen_constant(sym, n, noise, 0x0c);
				check_rt(sym, (unsigned short)n, tag++);
			}

			/* Pure noise, at three widths. */
			for (rep = 0; rep < 3; rep++) {
				static const unsigned masks[] = {
					0xf, 0xff, 0xffff
				};
				gen_noise(sym, n, masks[rep]);
				check_1s(sym, (unsigned short)n, bps,
					 0x4000, tag++);
				check_rm(sym, (unsigned short)n, bps, tag++);
				check_rt(sym, (unsigned short)n, tag++);
			}
		}
	}
	return diff_end();
}

/* ------------------------------------------------------------------------ */

/*
 * MakeTxData, compared byte for byte.  Both buffers are poisoned with a value
 * no pattern emits, so a generator that stops one symbol early or runs one
 * late is separated -- which comparing only the first `n` entries would not
 * do.
 */
#define TXBUF	65600

static short txa[TXBUF];
static short txb[TXBUF];

static long tx_wrote, tx_silent;

static int
check_tx(short count, short pattern, unsigned long window, long tag)
{
	unsigned long i;
	int first = -1;

	for (i = 0; i < window; i++)
		txa[i] = txb[i] = (short)0x5a5a;

	ref_MakeTxData(txa, &count, pattern);
	MakeTxData(txb, &count, pattern);

	for (i = 0; i < window; i++)
		if (txa[i] != txb[i]) {
			first = (int)i;
			break;
		}
	diff_eq_int("MakeTxData case %ld: first differing symbol", first, -1,
		    tag);

	if (txa[0] != (short)0x5a5a)
		tx_wrote++;
	else
		tx_silent++;
	return first;
}

static int
run_maketxdata(void)
{
	static const short counts[] = {
		0, 1, 2, 3, 4, 5, 8, 16, 17, 100, 101, 160, 161, 255, 256
	};
	long tag = 0;
	unsigned ci;
	short p;

	diff_begin("MakeTxData");

	for (ci = 0; ci < sizeof(counts) / sizeof(counts[0]); ci++)
		for (p = -2; p <= 6; p++) {
			short n = counts[ci];

			/*
			 * Pattern 0 does not terminate on an odd count; see
			 * v22det.h.  Every other selector is safe.
			 */
			if (p == V22_TXDATA_S1 && (n & 1))
				continue;
			check_tx(n, p, (unsigned long)n + POISON_SLACK,
				 tag++);
		}

	/*
	 * The countdown, which is the whole difference between this and a
	 * `for (i = 0; i < n; i++)`: -1 writes 65535 symbols, not none.
	 */
	check_tx((short)-1, V22_TXDATA_ONES_1200, 65535 + POISON_SLACK, tag++);
	check_tx((short)-2, V22_TXDATA_S1, 65534 + POISON_SLACK, tag++);

	return diff_end();
}

/* ------------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;

	rc |= run_detectors();
	rc |= run_maketxdata();

	diff_begin("v22det coverage guards");
	diff_eq_int("Detect_1s matched (%ld)", fired_1s > 0, 1, 0);
	diff_eq_int("Detect_1s declined (%ld)", quiet_1s > 0, 1, 0);
	diff_eq_int("Detect_Rmloop2_ACK matched (%ld)", fired_rm > 0, 1, 0);
	diff_eq_int("Detect_Rmloop2_ACK declined (%ld)", quiet_rm > 0, 1, 0);
	diff_eq_int("Detect_Retrain matched (%ld)", fired_rt > 0, 1, 0);
	diff_eq_int("Detect_Retrain declined (%ld)", quiet_rt > 0, 1, 0);
	diff_eq_int("2400 arm taken (%ld)", rate_2400 > 0, 1, 0);
	diff_eq_int("1200 arm taken (%ld)", rate_1200 > 0, 1, 0);
	/*
	 * The two that matter most: without them an `int` accumulator passes
	 * everything above.
	 */
	diff_eq_int("ideal energy wrapped 16 bits (%ld)", saw_wrap_energy > 0,
		    1, 0);
	diff_eq_int("received energy wrapped 16 bits (%ld)", saw_wrap_sumsq > 0,
		    1, 0);
	diff_eq_int("period-2 run passed the threshold (%ld)", saw_long_run > 0,
		    1, 0);
	diff_eq_int("Detect_Retrain's own energy wrapped (%ld)",
		    saw_wrap_retrain > 0, 1, 0);
	diff_eq_int("MakeTxData wrote something (%ld)", tx_wrote > 0, 1, 0);
	diff_eq_int("MakeTxData declined a selector (%ld)", tx_silent > 0, 1, 0);
	rc |= diff_end();

	return rc;
}
