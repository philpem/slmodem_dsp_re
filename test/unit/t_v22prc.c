/*
 * t_v22prc.c -- differential test of V.22's small state and status helpers.
 *
 * Nine leaves, and the reason they are worth a careful test rather than a
 * cursory one is that every one of them has an edge the obvious rewrite gets
 * wrong:
 *
 *   - TxNOP and RxClampV22 count down through a 16-bit register and test
 *     BEFORE the decrement, so they write n+1 entries, not n.  The test
 *     checks the entry one past the end is untouched.
 *   - RxTrained1200 calls an EMPTY array trained.
 *   - RxTrained2400 wants strictly more than seven trailing 15s, scans
 *     backwards, and stops on running out as well as on a mismatch.  Both
 *     exits land on the same test, so an array that is entirely 15s and
 *     shorter than eight entries still passes if it runs out with a count
 *     above seven -- which it cannot, and that is worth pinning.
 *   - GetSignalQuality subtracts from -32768 in 32 bits and then truncates
 *     unsigned, so it wraps.  Swept over the whole 16-bit domain.
 *   - TxClockSync multiplies a short by three and stores a short, so it
 *     overflows for |baud| > 10922.  Also swept whole.
 *
 * The datapump object is not modelled (see include/dsplib/v22prc.h), so the
 * fixtures here are byte buffers with the known offsets poked directly.  Both
 * sides are handed the same buffer contents in separate buffers.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v22prc.h"

extern int ref_ReadGTimer(void *modem);
extern void ref_TxNOP(void *modem, void *arg1, short *out, short *count);
extern void ref_RxClampV22(void *modem, void *arg1, short *out, short *count);
extern int ref_RxTrained1200(const short *symbols, const unsigned short *count);
extern int ref_RxTrained2400(const short *symbols, const unsigned short *count);
extern int ref_CarrierDetect(void *modem);
extern int ref_SignalDetect(void *modem);
extern unsigned short ref_GetSignalQuality(void *modem);
extern void ref_TxClockSync(void *modem);

/* Comfortably past V22FP_QUALITY at 0x186. */
#define FP_SIZE		0x200
#define OBJ_SIZE	0x60

/* A value no field under test can legitimately hold. */
#define MARK		0x5ead

struct fixture {
	unsigned char obj[OBJ_SIZE];
	unsigned char fp[FP_SIZE];
	int timer;
};

static void
fixture_init(struct fixture *f)
{
	memset(f, 0, sizeof(*f));
	*(void **)(void *)(f->obj + V22_OBJ_FP) = f->fp;
	*(void **)(void *)(f->obj + V22_OBJ_GTIMER) = &f->timer;
}

static void
put_short(unsigned char *base, int off, short v)
{
	*(short *)(void *)(base + off) = v;
}

static void
put_int(unsigned char *base, int off, int v)
{
	*(int *)(void *)(base + off) = v;
}

static short
get_short(const unsigned char *base, int off)
{
	return *(const short *)(const void *)(base + off);
}

/* --------------------------------------------------------------------- */

static int
run_gtimer(void)
{
	static const int starts[] = {
		0, 1, -1, -20, 100000, 0x7fffffff - 40, -0x7fffffff
	};
	struct fixture a, b;
	int rc, s, i;

	diff_begin("ReadGTimer");
	for (s = 0; s < (int)(sizeof(starts) / sizeof(starts[0])); s++) {
		fixture_init(&a);
		fixture_init(&b);
		a.timer = b.timer = starts[s];

		for (i = 0; i < 5; i++) {
			int ra = ref_ReadGTimer(&a);
			int rb = ReadGTimer(&b);

			diff_eq_int("start %ld: return", rb, ra, starts[s]);
			diff_eq_int("start %ld: timer", b.timer, a.timer,
				    starts[s]);
		}
	}
	rc = diff_end();
	return rc;
}

/*
 * TxNOP and RxClampV22 share a shape, so they share a driver.  `written` is
 * how many entries the object actually fills; the buffer is longer, and the
 * tail must survive.
 */
static int
run_blockfill(const char *label,
	      void (*ours)(void *, void *, short *, short *),
	      void (*ref)(void *, void *, short *, short *),
	      int written)
{
	short ba[V22_TX_BLOCK + 8], bb[V22_TX_BLOCK + 8];
	short ca, cb;
	struct fixture fa, fb;
	int rc, i;

	fixture_init(&fa);
	fixture_init(&fb);

	for (i = 0; i < V22_TX_BLOCK + 8; i++)
		ba[i] = bb[i] = (short)MARK;
	ca = cb = (short)MARK;

	diff_begin(label);
	ref(&fa, 0, ba, &ca);
	ours(&fb, 0, bb, &cb);

	diff_eq_int("count (%ld)", cb, ca, 0);
	diff_eq_int("count is the block length (%ld)", ca, written, 0);
	for (i = 0; i < V22_TX_BLOCK + 8; i++)
		diff_eq_int("out[%ld]", bb[i], ba[i], i);

	/*
	 * Non-vacuity: the entry one PAST what a naive `for (i = 0; i < n)`
	 * would write must have been written too, and the one after that must
	 * not.  Without this the off-by-one the loop idiom encodes is
	 * untested, because both readings agree everywhere else.
	 */
	diff_eq_int("entry %ld was written", ba[written - 1] != (short)MARK,
		    1, written - 1);
	diff_eq_int("entry %ld was NOT written", ba[written] == (short)MARK,
		    1, written);
	rc = diff_end();
	return rc;
}

/* --------------------------------------------------------------------- */

static int trained12_true, trained12_false;
static int trained24_true, trained24_false;

static int
check_trained(const short *sym, int n)
{
	unsigned short na = (unsigned short)n, nb = (unsigned short)n;
	int ra, rb, bad = 0;

	ra = ref_RxTrained1200(sym, &na);
	rb = RxTrained1200(sym, &nb);
	diff_eq_int("RxTrained1200 n=%ld", rb, ra, n);
	if (ra != rb)
		bad = 1;
	if (ra)
		trained12_true++;
	else
		trained12_false++;

	ra = ref_RxTrained2400(sym, &na);
	rb = RxTrained2400(sym, &nb);
	diff_eq_int("RxTrained2400 n=%ld", rb, ra, n);
	if (ra != rb)
		bad = 1;
	if (ra)
		trained24_true++;
	else
		trained24_false++;

	return bad;
}

/*
 * Exhaustive over an alphabet of the two symbols that matter plus one that
 * matters to neither, for every length up to eight.  3^8 * 9 arrays is
 * cheap and leaves nothing to choose.
 */
static int
run_trained_exhaustive(void)
{
	static const short alpha[3] = {
		V22_TRAINED_1200_SYMBOL, V22_TRAINED_2400_SYMBOL, 0
	};
	short sym[8];
	int rc, n, k, code;

	diff_begin("RxTrained exhaustive, n = 0..8");
	for (n = 0; n <= 8; n++) {
		int combos = 1;

		for (k = 0; k < n; k++)
			combos *= 3;

		for (code = 0; code < combos; code++) {
			int c = code;

			for (k = 0; k < n; k++) {
				sym[k] = alpha[c % 3];
				c /= 3;
			}
			(void)check_trained(sym, n);
		}
	}
	rc = diff_end();
	return rc;
}

/*
 * The lengths the exhaustive sweep cannot reach: a run of exactly seven and
 * exactly eight trailing 15s, which is where RxTrained2400's threshold sits,
 * and a long all-3 array for the 1200 side.
 */
static int
run_trained_targeted(void)
{
	short sym[64];
	int rc, i, run;

	diff_begin("RxTrained targeted runs");
	for (run = 0; run <= 12; run++) {
		for (i = 0; i < 20; i++)
			sym[i] = 0;
		for (i = 0; i < run; i++)
			sym[20 - 1 - i] = V22_TRAINED_2400_SYMBOL;
		(void)check_trained(sym, 20);
	}

	for (i = 0; i < 64; i++)
		sym[i] = V22_TRAINED_1200_SYMBOL;
	(void)check_trained(sym, 64);
	(void)check_trained(sym, 1);

	for (i = 0; i < 64; i++)
		sym[i] = V22_TRAINED_2400_SYMBOL;
	(void)check_trained(sym, 64);
	(void)check_trained(sym, 8);
	(void)check_trained(sym, 7);

	/* A 3 buried in the middle of an otherwise-trained 1200 array. */
	for (i = 0; i < 16; i++)
		sym[i] = V22_TRAINED_1200_SYMBOL;
	sym[9] = 4;
	(void)check_trained(sym, 16);

	/* And a 15 buried in the middle of an otherwise-trained 2400 tail. */
	for (i = 0; i < 16; i++)
		sym[i] = V22_TRAINED_2400_SYMBOL;
	sym[4] = 0;
	(void)check_trained(sym, 16);
	rc = diff_end();
	return rc;
}

/* --------------------------------------------------------------------- */

static int
run_status(void)
{
	static const int vals[] = { 0, 1, -1, 0x7fffffff, -0x7fffffff - 1,
				    0x1234, MARK };
	struct fixture a, b;
	int rc, i;

	diff_begin("CarrierDetect and SignalDetect");
	for (i = 0; i < (int)(sizeof(vals) / sizeof(vals[0])); i++) {
		fixture_init(&a);
		fixture_init(&b);
		put_int(a.fp, V22FP_CARRIER, vals[i]);
		put_int(b.fp, V22FP_CARRIER, vals[i]);
		put_int(a.fp, V22FP_SIGNAL, vals[i] ^ 0x55);
		put_int(b.fp, V22FP_SIGNAL, vals[i] ^ 0x55);

		diff_eq_int("CarrierDetect(%ld)", CarrierDetect(&b),
			    ref_CarrierDetect(&a), vals[i]);
		diff_eq_int("SignalDetect(%ld)", SignalDetect(&b),
			    ref_SignalDetect(&a), vals[i]);
	}
	rc = diff_end();
	return rc;
}

/* Whether the subtraction ever left sixteen bits; see the guard in main. */
static int quality_wrapped;

static int
run_quality(void)
{
	struct fixture a, b;
	int rc, v;

	fixture_init(&a);
	fixture_init(&b);

	diff_begin("GetSignalQuality, whole 16-bit domain");
	for (v = 0; v < 0x10000; v++) {
		unsigned short ra, rb;

		*(unsigned short *)(void *)(a.fp + V22FP_QUALITY) =
			(unsigned short)v;
		*(unsigned short *)(void *)(b.fp + V22FP_QUALITY) =
			(unsigned short)v;

		ra = ref_GetSignalQuality(&a);
		rb = GetSignalQuality(&b);
		diff_eq_int("quality field %ld", rb, ra, v);

		if (-32768 - v < 0 || -32768 - v > 0xffff)
			quality_wrapped++;
	}
	rc = diff_end();
	return rc;
}

/* Whether baud * 3 ever left a short; see the guard in main. */
static int clock_overflowed;

static int
run_clocksync(void)
{
	struct fixture a, b;
	int rc, v;

	diff_begin("TxClockSync, whole 16-bit domain");
	for (v = -32768; v <= 32767; v++) {
		fixture_init(&a);
		fixture_init(&b);
		put_short(a.fp, V22FP_BAUD, (short)v);
		put_short(b.fp, V22FP_BAUD, (short)v);
		put_short(a.fp, V22FP_TX_CLOCK, (short)MARK);
		put_short(b.fp, V22FP_TX_CLOCK, (short)MARK);

		ref_TxClockSync(&a);
		TxClockSync(&b);

		diff_eq_int("baud %ld: tx clock",
			    get_short(b.fp, V22FP_TX_CLOCK),
			    get_short(a.fp, V22FP_TX_CLOCK), v);
		diff_eq_int("baud %ld: baud unchanged",
			    get_short(b.fp, V22FP_BAUD),
			    get_short(a.fp, V22FP_BAUD), v);

		if (v * 3 > 32767 || v * 3 < -32768)
			clock_overflowed++;
	}
	rc = diff_end();
	return rc;
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	rc |= run_gtimer();
	rc |= run_blockfill("TxNOP", TxNOP, ref_TxNOP, V22_TX_BLOCK);
	rc |= run_blockfill("RxClampV22", RxClampV22, ref_RxClampV22,
			    V22_CLAMP_BLOCK);
	rc |= run_trained_exhaustive();
	rc |= run_trained_targeted();
	rc |= run_status();
	rc |= run_quality();
	rc |= run_clocksync();

	/*
	 * Guards, so none of the above can go vacuous.  A predicate that only
	 * ever answered one way would agree with a stub; an arithmetic sweep
	 * that never left sixteen bits would not have tested the wrap that is
	 * the only interesting thing about either function.
	 */
	diff_begin("v22prc coverage guards");
	diff_eq_int("RxTrained1200 answered true (%ld)", trained12_true > 0,
		    1, 0);
	diff_eq_int("RxTrained1200 answered false (%ld)", trained12_false > 0,
		    1, 0);
	diff_eq_int("RxTrained2400 answered true (%ld)", trained24_true > 0,
		    1, 0);
	diff_eq_int("RxTrained2400 answered false (%ld)", trained24_false > 0,
		    1, 0);
	diff_eq_int("GetSignalQuality wrapped (%ld)", quality_wrapped > 0, 1, 0);
	diff_eq_int("TxClockSync overflowed (%ld)", clock_overflowed > 0, 1, 0);
	rc |= diff_end();

	return rc;
}
