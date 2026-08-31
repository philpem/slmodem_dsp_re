/*
 * t_cidprog.c -- differential test of `cid_progress`, the Caller ID service's
 * state machine and the last function of `src/service/cid.c`.
 *
 * WHAT IS HARD ABOUT TESTING IT is that almost nothing it does is visible in
 * its return value.  It buffers samples into `ctx->samples`, dispatches on
 * `ctx->mode`, WRITES `ctx->mode` from two of its arms, and carries three
 * separate result variables from one turn of its loop to the next.  So the
 * comparison is over the whole of `struct cid_modem` and both receivers, not
 * over the answer, and the buffer the caller supplied is compared too -- the
 * object never writes through `in` and a test that did not look could not say
 * so.
 *
 * THE ORACLE FOR THE BUFFERING IS ARITHMETIC, NOT THE OTHER SIDE.  There is
 * exactly one exit from the outer loop -- `f3f8 != len` -- so a call always
 * drains its input, and the fill level afterwards is
 * `(f3f8_before + n) % len` with `(f3f8_before + n) / len` blocks processed.
 * That is asserted against the REFERENCE on every call, so "the block length
 * is 160 at 8000 and 192 at 9600" is pinned by an independent statement of
 * the same fact rather than by our own copy of it.
 *
 * THE ARMS ARE PLANTED, NOT HOPED FOR.  Real audio drives the file -- a Bell
 * 202 mark tone and bit stream for the FSK side, DTMF tone pairs for the
 * other -- but the arms that need a receiver to answer 3 or -1 are reached by
 * seeding the receiver, which is what `t_rxcid` does for the same reason.  The
 * seeds are read off the receivers' own source: `cid_modem` returns 3 or -1
 * from the checksum once `mark_conf` is at least `m*18/256`, which is 18 at
 * both of this function's block lengths, and `dtmf_modem` returns 2 or -1 from
 * `ndigits > 2` and 3 from a pending 'C' committed by a silent block.
 *
 * ANTI-VACUITY, ALL COUNTED ON THE REFERENCE SIDE (finding F134): every
 * return value, every arm of the mode dispatch, both block lengths, calls that
 * process no block, one block and several, the `*count` clear, and each of the
 * two mode transitions the function performs.  A counter taken off our own
 * side would measure our coverage and not the object's.
 *
 * SIZED FROM THE BOUNDS, NOT FROM COMFORT (D956, D972, D973).  `cid_modem`
 * copies `count` samples into a 206-short local and checksums `data[1]+2`
 * bytes with no bound of its own, so every object sits in a box with a 4 KB
 * guard behind it and the guards are compared on BOTH sides -- an over-run
 * that both sides perform identically would otherwise pass unseen.
 * `cid_progress` itself only ever passes 160 or 192, which is inside both
 * receivers' limits, and this test asserts that by watching the guards.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "harness.h"
#include "dsplib/cid.h"
#include "dsplib/cid_modem.h"
#include "dsplib/dtmf_rx.h"

extern short ref_cid_progress(void *ctx, short *in, int what, short *count);
extern void *ref_cid_create(void *ctx, int cid_val, int mode);

#define GUARD		4096
#define MAXIN		1024

/*
 * `m*18 >> 8` with m = 40960/160 = 49152/192 = 256, which is what `cid_modem`
 * computes at either of `cid_progress`'s two block lengths.  Spelled out here
 * rather than imported so the two derivations stay independent.
 */
#define MARK_CONF_FULL	18

struct box {
	struct cid_modem ctx;
	unsigned char g1[GUARD];
	struct dtmf_rx dtmf;
	unsigned char g2[GUARD];
	struct cid fsk;
	unsigned char g3[GUARD];
};

static unsigned long seed = 20260831UL;

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
/* Anti-vacuity, every one of them counted on the REFERENCE side.      */

static int seen_ret[4];		/* returns of 0, 1, 2 and 3           */
static int seen_ret_other;	/* anything else -- must stay zero    */
static int seen_arm_fsk;	/* mode 0                             */
static int seen_arm_dtmf;	/* mode 1                             */
static int seen_arm_auto;	/* mode 5                             */
static int seen_arm_message;	/* mode 3                             */
static int seen_arm_none;	/* mode > 1, not 5, not 3             */
static int seen_len_160;
static int seen_len_192;
static int seen_blocks_0;
static int seen_blocks_1;
static int seen_blocks_many;
static int seen_count_cleared;
static int seen_mode_to_2;
static int seen_mode_to_3;
static int seen_fsk_reset;
static int seen_full_at_entry;	/* a block ran on no new samples      */

/* ------------------------------------------------------------------ */

/*
 * Build the pair.  The object itself is filled with pseudorandom bytes so
 * that any field `cid_progress` must not touch has a value a spurious store
 * would change; the two receivers are zeroed before they are constructed,
 * because `reset_cid` treats a non-null `mrf.history` as a buffer it already
 * owns and the random fill would hand it a wild pointer.
 *
 * The reference side is built by `ref_cid_create` and ours by `cid_create`,
 * so each object's receivers come from its own side, exactly as in `t_cidsvc`.
 */
static void
build(struct box *a, struct box *b, int mode, short rate)
{
	fill_bytes(&a->ctx, sizeof(a->ctx));
	memset(&a->dtmf, 0, sizeof(a->dtmf));
	memset(&a->fsk, 0, sizeof(a->fsk));
	memset(a->g1, 0xa5, GUARD);
	memset(a->g2, 0xa5, GUARD);
	memset(a->g3, 0xa5, GUARD);
	memcpy(b, a, sizeof(*a));

	a->ctx.dtmf = &a->dtmf;
	a->ctx.fsk = &a->fsk;
	a->ctx.mode = CID_MODE_AUTOMATIC;
	b->ctx.dtmf = &b->dtmf;
	b->ctx.fsk = &b->fsk;
	b->ctx.mode = CID_MODE_AUTOMATIC;

	ref_cid_create(&a->ctx, 0, 0);
	cid_create(&b->ctx, 0, 0);

	a->dtmf.rate = b->dtmf.rate = rate;
	a->fsk.rate = b->fsk.rate = rate;
	a->ctx.mode = b->ctx.mode = mode;
	a->ctx.f3f8 = b->ctx.f3f8 = 0;
}

/* The block length the object will compute for this pair. */
static int
blocklen(const struct box *o)
{
	short rate = o->ctx.mode != CID_MODE_DTMF
		     ? o->fsk.rate : o->dtmf.rate;

	return rate != CID_RATE_8000 ? CID_BLOCK_9600 : CID_BLOCK_8000;
}

/* ------------------------------------------------------------------ */

static void
cmp_fsk(const char *what, struct cid *ours, struct cid *ref, long tag)
{
	struct cid fa = *ref, fb = *ours;
	char label[192];

	snprintf(label, sizeof(label),
		 "%s: mrf history both set or both clear (%%ld)", what);
	diff_eq_int(label, (fa.mrf.history != 0) == (fb.mrf.history != 0), 1,
		    tag);
	if (fa.mrf.history && fb.mrf.history) {
		int n = fa.mrf.history_len < fb.mrf.history_len
			? fa.mrf.history_len : fb.mrf.history_len;

		snprintf(label, sizeof(label), "%s: mrf history contents %%ld",
			 what);
		diff_eq_int(label, memcmp(fa.mrf.history, fb.mrf.history,
					  (size_t)n * sizeof(short)), 0, tag);
	}
	snprintf(label, sizeof(label),
		 "%s: mrf coeff both set or both clear (%%ld)", what);
	diff_eq_int(label, (fa.mrf.cfg.coeff != 0) == (fb.mrf.cfg.coeff != 0),
		    1, tag);
	if (fa.mrf.cfg.coeff && fb.mrf.cfg.coeff) {
		int n = fa.mrf.cfg.taps < fb.mrf.cfg.taps
			? fa.mrf.cfg.taps : fb.mrf.cfg.taps;

		snprintf(label, sizeof(label), "%s: mrf coefficients %%ld",
			 what);
		diff_eq_int(label, memcmp(fa.mrf.cfg.coeff, fb.mrf.cfg.coeff,
					  (size_t)n * sizeof(short)), 0, tag);
	}

	fa.mrf.history = fb.mrf.history = 0;
	fa.mrf.cfg.coeff = fb.mrf.cfg.coeff = 0;
	snprintf(label, sizeof(label), "%s: FSK receiver %%ld", what);
	diff_eq_obj(label, struct cid, &fb, &fa, tag);
}

/*
 * One call on each side over the same samples, and everything either could
 * have touched compared afterwards.
 */
static void
run(const char *what, struct box *a, struct box *b, const short *in, int nin,
    long tag)
{
	static short ina[MAXIN], inb[MAXIN];
	unsigned char clean[GUARD];
	char label[192];
	short na, nb, ra, rb;
	int len = blocklen(a);
	int fill_before = a->ctx.f3f8;
	int mode_before = a->ctx.mode;
	short pack_before = a->fsk.pack_len;
	short conf_before = a->fsk.mark_conf;
	int total, blocks;

	memset(clean, 0xa5, sizeof(clean));
	memset(ina, 0, sizeof(ina));
	memset(inb, 0, sizeof(inb));
	if (nin > 0)
		memcpy(ina, in, (size_t)nin * sizeof(short));
	memcpy(inb, ina, sizeof(ina));

	na = (short)nin;
	nb = (short)nin;
	ra = ref_cid_progress(&a->ctx, ina, 0, &na);
	rb = cid_progress(&b->ctx, inb, 0, &nb);

	snprintf(label, sizeof(label), "%s: return value (%%ld)", what);
	diff_eq_int(label, rb, ra, tag);
	snprintf(label, sizeof(label), "%s: *count after (%%ld)", what);
	diff_eq_int(label, nb, na, tag);
	snprintf(label, sizeof(label), "%s: reference cleared *count (%%ld)",
		 what);
	diff_eq_int(label, na, 0, tag);
	snprintf(label, sizeof(label), "%s: input buffer untouched (%%ld)",
		 what);
	diff_eq_int(label, memcmp(ina, inb, sizeof(ina)), 0, tag);

	{
		struct cid_modem ca = a->ctx, cb = b->ctx;

		ca.dtmf = cb.dtmf = 0;
		ca.fsk = cb.fsk = 0;
		snprintf(label, sizeof(label), "%s: service object %%ld", what);
		diff_eq_obj(label, struct cid_modem, &cb, &ca, tag);
	}
	{
		struct dtmf_rx da = a->dtmf, db = b->dtmf;

		snprintf(label, sizeof(label),
			 "%s: DTMF bufp both set or both clear (%%ld)", what);
		diff_eq_int(label, (da.bufp != 0) == (db.bufp != 0), 1, tag);
		da.bufp = db.bufp = 0;
		snprintf(label, sizeof(label), "%s: DTMF receiver %%ld", what);
		diff_eq_obj(label, struct dtmf_rx, &db, &da, tag);
	}
	cmp_fsk(what, &b->fsk, &a->fsk, tag);

	snprintf(label, sizeof(label), "%s: reference guards (%%ld)", what);
	diff_eq_int(label, memcmp(a->g1, clean, GUARD)
			   | memcmp(a->g2, clean, GUARD)
			   | memcmp(a->g3, clean, GUARD), 0, tag);
	snprintf(label, sizeof(label), "%s: our guards (%%ld)", what);
	diff_eq_int(label, memcmp(b->g1, clean, GUARD)
			   | memcmp(b->g2, clean, GUARD)
			   | memcmp(b->g3, clean, GUARD), 0, tag);

	/*
	 * The arithmetic oracle.  It holds only where the fill level started
	 * inside the block, which every case here does; a fill level ABOVE the
	 * block length has its own case below and its own assertion.
	 */
	total = fill_before + (nin > 0 ? nin : 0);
	blocks = fill_before <= len ? total / len : 0;
	if (fill_before <= len) {
		snprintf(label, sizeof(label),
			 "%s: reference fill level is (before + n) %% len (%%ld)",
			 what);
		diff_eq_int(label, a->ctx.f3f8, total % len, tag);
	} else {
		snprintf(label, sizeof(label),
			 "%s: an over-full buffer is left alone (%%ld)", what);
		diff_eq_int(label, a->ctx.f3f8, fill_before, tag);
	}

	/* ---- what the OBJECT did, counted on the reference side ---- */
	if (ra >= 0 && ra <= 3)
		seen_ret[ra]++;
	else
		seen_ret_other++;

	if (mode_before == CID_MODE_FSK)
		seen_arm_fsk += blocks > 0;
	else if (mode_before == CID_MODE_DTMF)
		seen_arm_dtmf += blocks > 0;
	else if (mode_before == CID_MODE_AUTOMATIC)
		seen_arm_auto += blocks > 0;
	else if (mode_before == CID_MODE_CID_MESSAGE)
		seen_arm_message += blocks > 0;
	else
		seen_arm_none += blocks > 0;

	if (len == CID_BLOCK_8000)
		seen_len_160++;
	else
		seen_len_192++;

	if (blocks == 0)
		seen_blocks_0++;
	else if (blocks == 1)
		seen_blocks_1++;
	else
		seen_blocks_many++;

	if (blocks > 0 && nin <= 0)
		seen_full_at_entry++;
	if (nin != 0 && na == 0)
		seen_count_cleared++;
	if (a->ctx.mode != mode_before) {
		if (a->ctx.mode == CID_MODE_FSK_DONE)
			seen_mode_to_2++;
		if (a->ctx.mode == CID_MODE_CID_MESSAGE)
			seen_mode_to_3++;
	}
	if (mode_before == CID_MODE_AUTOMATIC && pack_before > 0
	    && conf_before > 0 && a->fsk.pack_len == 0
	    && a->fsk.mark_conf == 0)
		seen_fsk_reset++;
}

/* ------------------------------------------------------------------ */
/* Signal generators, the same shapes t_rxcid and t_dtmfrx use.        */

static const double PI = 3.14159265358979323846;

static void
tone(short *out, int n, double fs, double f, double amp, double *ph)
{
	int i;

	for (i = 0; i < n; i++) {
		double v = amp * sin(*ph);

		*ph += 2.0 * PI * f / fs;
		if (v > 32767.0)
			v = 32767.0;
		if (v < -32768.0)
			v = -32768.0;
		out[i] = (short)v;
	}
}

/* Bell 202: 1200 Hz is a mark, 2200 Hz a space, 1200 baud, continuous phase. */
static void
fsk(short *out, int n, double fs, const unsigned char *bits, int nbits,
    int *bitpos, double *ph, double *frac)
{
	double spb = fs / 1200.0;
	int i;

	for (i = 0; i < n; i++) {
		double f = bits[*bitpos % nbits] ? 1200.0 : 2200.0;

		out[i] = (short)(6000.0 * sin(*ph));
		*ph += 2.0 * PI * f / fs;
		*frac += 1.0;
		if (*frac >= spb) {
			*frac -= spb;
			(*bitpos)++;
		}
	}
}

static const double low_hz[4] = { 697.0, 770.0, 852.0, 941.0 };
static const double high_hz[4] = { 1209.0, 1336.0, 1477.0, 1633.0 };

static void
pair(short *out, int n, double fs, double f1, double f2, double amp,
     double *p1, double *p2)
{
	int i;

	for (i = 0; i < n; i++) {
		double v = 0.0;

		if (amp != 0.0)
			v = amp * sin(*p1) + amp * sin(*p2);
		*p1 += 2.0 * PI * f1 / fs;
		*p2 += 2.0 * PI * f2 / fs;
		if (v > 32767.0)
			v = 32767.0;
		if (v < -32768.0)
			v = -32768.0;
		out[i] = (short)v;
	}
}

/* ------------------------------------------------------------------ */
/*
 * The planted seeds.  Each is applied to BOTH sides, and each is read off the
 * receiver's own source rather than guessed -- the comments name the test in
 * `cid_modem` or `dtmf_modem` that the seed satisfies.
 */

/* `cid_modem`: pack_len >= msglen and the checksum agrees -> 3. */
static void
seed_fsk_message(struct box *o, int good)
{
	unsigned char s = 0;
	int j;

	o->fsk.mark_conf = MARK_CONF_FULL;
	o->fsk.pack_len = 10;
	o->fsk.data[0] = 0x80;
	o->fsk.data[1] = 5;
	for (j = 2; j < 7; j++)
		o->fsk.data[j] = (unsigned char)(0x30 + j);
	for (j = 0; j < 7; j++)
		s = (unsigned char)(s + o->fsk.data[j]);
	o->fsk.data[7] = (unsigned char)(good ? -s : ~s);
}

/* `cid_modem`: a declared body length of zero -> msglen 3 -> -1. */
static void
seed_fsk_giveup(struct box *o)
{
	o->fsk.mark_conf = MARK_CONF_FULL;
	o->fsk.pack_len = 4;
	o->fsk.data[1] = 0;
}

/* `dtmf_modem`: ndigits > 2 -> result 2, and past 3*rate -> -1. */
static void
seed_dtmf_collecting(struct box *o, int timeout)
{
	o->dtmf.ndigits = 3;
	o->dtmf.nsamples = timeout ? 4 * (int)(unsigned short)o->dtmf.rate : 0;
	o->dtmf.state = 0;
}

/*
 * `dtmf_modem`: state 2 with a pending 'C' (code 12) and a stable count above
 * zero, committed by a block the band pass calls silent -> result 3.
 */
static void
seed_dtmf_c(struct box *o)
{
	o->dtmf.state = 2;
	o->dtmf.aligned = 0;
	o->dtmf.stable = 1;
	o->dtmf.last_digit = 12;
	o->dtmf.ndigits = 0;
	o->dtmf.quiet = 0;
	o->dtmf.nsamples = 0;
	o->dtmf.level = 1;
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	static struct box a, b;
	static short in[MAXIN];
	static const short rates[2] = { CID_RATE_8000, CID_RATE_9600 };
	int rc;
	int r, i, k;
	char what[96];

	diff_begin("cid_progress: the object's shape");
	diff_eq_int("struct cid_modem is 0x3fc bytes (%ld)",
		    (int)sizeof(struct cid_modem), CID_MODEM_BYTES, 0);
	diff_eq_int("the block buffer runs to f3f8 (%ld)",
		    (int)(sizeof(a.ctx.samples) / sizeof(short)), 200, 0);
	rc = diff_end();

	/* ---------------------------------------------------------------- */
	/*
	 * Buffering, over every mode and both rates, with input lengths that
	 * bracket the block: nothing, part of a block, exactly a block, a
	 * block and a bit, and several blocks.  The lengths are deliberately
	 * not multiples of either 160 or 192, so the carried remainder is
	 * exercised from one call to the next.
	 */
	diff_begin("cid_progress: buffering and the block boundary");
	{
		static const int modes[] = { 0, 1, 2, 3, 4, 5 };
		static const int lens[] = { 0, 1, 37, 160, 161, 192, 193,
					    200, 400, 700, -5 };
		unsigned m, ri, li;

		for (ri = 0; ri < 2; ri++)
			for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
				build(&a, &b, modes[m], rates[ri]);
				for (li = 0;
				     li < sizeof(lens) / sizeof(lens[0]); li++) {
					int n = lens[li];

					for (i = 0; i < MAXIN; i++)
						in[i] = (short)((long)(rnd()
							 % 4001UL) - 2000L);
					snprintf(what, sizeof(what),
						 "mode %d rate %d n %d",
						 modes[m], (int)rates[ri], n);
					run(what, &a, &b, in, n,
					    (long)(li + 100 * m + 1000 * ri));
				}
			}
	}
	rc |= diff_end();

	/* ---------------------------------------------------------------- */
	/*
	 * THE TWO RATE READS ARE NOT THE SAME READ.  The object computes the
	 * block length twice -- once from the DTMF receiver under `mode != 0`
	 * and once from the FSK receiver under `mode != 1` -- and the second
	 * OVERWRITES the first, so in modes 3 and 5 the FSK receiver's rate
	 * decides and the DTMF receiver's does not.  With both receivers at the
	 * same rate that is invisible, and the injection ritual's "the length
	 * comes from the DTMF receiver in both ifs" survived the whole of the
	 * rest of this file until this section existed.
	 */
	diff_begin("cid_progress: the two receivers' rates disagree");
	{
		static const int mmodes[] = { 0, 1, 3, 5 };
		unsigned m;
		int swap;

		for (swap = 0; swap < 2; swap++)
			for (m = 0;
			     m < sizeof(mmodes) / sizeof(mmodes[0]); m++) {
				build(&a, &b, mmodes[m], CID_RATE_8000);
				a.dtmf.rate = b.dtmf.rate = (short)(swap
					? CID_RATE_9600 : CID_RATE_8000);
				a.fsk.rate = b.fsk.rate = (short)(swap
					? CID_RATE_8000 : CID_RATE_9600);
				for (i = 0; i < MAXIN; i++)
					in[i] = (short)((long)(rnd() % 4001UL)
							- 2000L);
				for (k = 0; k < 5; k++) {
					snprintf(what, sizeof(what),
						 "mode %d dtmf %d fsk %d",
						 mmodes[m], (int)a.dtmf.rate,
						 (int)a.fsk.rate);
					run(what, &a, &b, in, 100, (long)k);
				}
			}
	}
	rc |= diff_end();

	/* ---------------------------------------------------------------- */
	/*
	 * A buffer that is already full at entry, and one that is over-full.
	 * The first must run a block on no new samples at all -- the object's
	 * only exit is `f3f8 != len`, so the fill test is what decides and not
	 * the sample count -- and the second must do nothing.
	 */
	diff_begin("cid_progress: the fill level decides, not the sample count");
	for (r = 0; r < 2; r++) {
		int len;

		build(&a, &b, CID_MODE_FSK, rates[r]);
		len = blocklen(&a);
		a.ctx.f3f8 = b.ctx.f3f8 = len;
		snprintf(what, sizeof(what), "full buffer, no samples, rate %d",
			 (int)rates[r]);
		run(what, &a, &b, in, 0, (long)r);

		build(&a, &b, CID_MODE_DTMF, rates[r]);
		len = blocklen(&a);
		a.ctx.f3f8 = b.ctx.f3f8 = len;
		snprintf(what, sizeof(what),
			 "full buffer, no samples, DTMF, rate %d",
			 (int)rates[r]);
		run(what, &a, &b, in, 0, (long)r);

		build(&a, &b, CID_MODE_FSK, rates[r]);
		a.ctx.f3f8 = b.ctx.f3f8 = 199;
		snprintf(what, sizeof(what), "over-full buffer, rate %d",
			 (int)rates[r]);
		run(what, &a, &b, in, 64, (long)r);
	}
	rc |= diff_end();

	/* ---------------------------------------------------------------- */
	/*
	 * REAL AUDIO through the FSK arm: a 1200 Hz mark tone long enough to
	 * cross both of `cid_modem`'s confidence thresholds, then a Bell 202
	 * bit stream, fed in chunks that do not align with the block so the
	 * buffering and the receiver are exercised together.
	 */
	diff_begin("cid_progress: mode 0 over a Bell 202 carrier");
	for (r = 0; r < 2; r++) {
		double fs = rates[r] == CID_RATE_9600 ? 9600.0 : 8000.0;
		double ph = 0.0, frac = 0.0;
		int bitpos = 0;
		static const unsigned char msg[] = {
			0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			0, 0, 1, 0, 0, 0, 0, 0, 1, 1,
			0, 1, 0, 0, 0, 0, 0, 0, 1, 1,
			0, 1, 1, 0, 0, 0, 0, 0, 1, 1
		};

		build(&a, &b, CID_MODE_FSK, rates[r]);
		for (i = 0; i < 24; i++) {
			tone(in, 100, fs, 1200.0, 8000.0, &ph);
			snprintf(what, sizeof(what), "mark tone rate %d",
				 (int)rates[r]);
			run(what, &a, &b, in, 100, (long)i);
		}
		for (i = 0; i < 60; i++) {
			fsk(in, 100, fs, msg, (int)sizeof(msg), &bitpos, &ph,
			    &frac);
			snprintf(what, sizeof(what), "fsk data rate %d",
				 (int)rates[r]);
			run(what, &a, &b, in, 100, (long)i);
		}
		diff_eq_int("the reference collected message bytes (%ld)",
			    a.fsk.pack_len > 0, 1, (long)r);
	}
	rc |= diff_end();

	/* ---------------------------------------------------------------- */
	/*
	 * REAL AUDIO through the DTMF arm, and through the automatic one.
	 * '0', '1' and 'C' as (low, high) index pairs; 'C' is (852, 1633),
	 * which is the one high tone the bank decodes from a single block.
	 */
	diff_begin("cid_progress: modes 1 and 5 over DTMF tone pairs");
	{
		static const int seq[3][2] = { { 3, 1 }, { 0, 0 }, { 2, 3 } };
		static const int dmodes[2] = { CID_MODE_DTMF,
					       CID_MODE_AUTOMATIC };
		int mi, d;

		for (mi = 0; mi < 2; mi++)
			for (r = 0; r < 2; r++) {
				double fs = rates[r] == CID_RATE_9600
					    ? 9600.0 : 8000.0;
				double p1 = 0.0, p2 = 0.0;
				long tag = 0;

				build(&a, &b, dmodes[mi], rates[r]);
				for (i = 0; i < 4; i++) {
					pair(in, 200, fs, 0.0, 0.0, 0.0, &p1,
					     &p2);
					snprintf(what, sizeof(what),
						 "mode %d lead-in rate %d",
						 dmodes[mi], (int)rates[r]);
					run(what, &a, &b, in, 200, tag++);
				}
				for (d = 0; d < 3; d++) {
					for (i = 0; i < 8; i++) {
						pair(in, 200, fs,
						     low_hz[seq[d][0]],
						     high_hz[seq[d][1]], 6000.0,
						     &p1, &p2);
						snprintf(what, sizeof(what),
							 "mode %d tone rate %d",
							 dmodes[mi],
							 (int)rates[r]);
						run(what, &a, &b, in, 200,
						    tag++);
					}
					for (i = 0; i < 8; i++) {
						pair(in, 200, fs, 0.0, 0.0, 0.0,
						     &p1, &p2);
						snprintf(what, sizeof(what),
							 "mode %d gap rate %d",
							 dmodes[mi],
							 (int)rates[r]);
						run(what, &a, &b, in, 200,
						    tag++);
					}
				}
			}
	}
	rc |= diff_end();

	/* ---------------------------------------------------------------- */
	/*
	 * THE PLANTED ARMS.  Each seed is applied identically to both sides
	 * and then one block is run, so exactly one dispatch arm and one
	 * verdict test fires per case.  These are what make the return values
	 * 1, 2 and 3 and both mode transitions reachable.
	 */
	diff_begin("cid_progress: every verdict the receivers can return");
	for (r = 0; r < 2; r++) {
		int len;
		long tag = (long)r;

		for (i = 0; i < MAXIN; i++)
			in[i] = 0;

		/* mode 0, a good message: res == 3 -> return 1 */
		build(&a, &b, CID_MODE_FSK, rates[r]);
		for (k = 0; k < 2; k++)
			seed_fsk_message(k ? &b : &a, 1);
		len = blocklen(&a);
		run("mode 0, FSK checksum agrees", &a, &b, in, len, tag);

		/* mode 0, a bad message: res == -1 -> return 2, and a string */
		build(&a, &b, CID_MODE_FSK, rates[r]);
		for (k = 0; k < 2; k++)
			seed_fsk_message(k ? &b : &a, 0);
		run("mode 0, FSK checksum disagrees", &a, &b, in, len, tag);

		/* mode 0, a declared length of zero: res == -1 the other way */
		build(&a, &b, CID_MODE_FSK, rates[r]);
		for (k = 0; k < 2; k++)
			seed_fsk_giveup(k ? &b : &a);
		run("mode 0, FSK declared length zero", &a, &b, in, len, tag);

		/* mode 1, digits arriving: res == 2 and mode 1 -> return 3 */
		build(&a, &b, CID_MODE_DTMF, rates[r]);
		for (k = 0; k < 2; k++)
			seed_dtmf_collecting(k ? &b : &a, 0);
		len = blocklen(&a);
		run("mode 1, DTMF collecting", &a, &b, in, len, tag);

		/* mode 1, the DTMF timeout: res == -1 -> return 2 */
		build(&a, &b, CID_MODE_DTMF, rates[r]);
		for (k = 0; k < 2; k++)
			seed_dtmf_collecting(k ? &b : &a, 1);
		run("mode 1, DTMF timed out", &a, &b, in, len, tag);

		/* mode 1, a committed 'C': res == 3 -> return 1 */
		build(&a, &b, CID_MODE_DTMF, rates[r]);
		for (k = 0; k < 2; k++)
			seed_dtmf_c(k ? &b : &a);
		run("mode 1, DTMF terminated by C", &a, &b, in, len, tag);

		/* mode 3, the same seeds down the CID_MESSAGE arm */
		build(&a, &b, CID_MODE_CID_MESSAGE, rates[r]);
		for (k = 0; k < 2; k++)
			seed_dtmf_collecting(k ? &b : &a, 0);
		len = blocklen(&a);
		run("mode 3, DTMF collecting", &a, &b, in, len, tag);

		build(&a, &b, CID_MODE_CID_MESSAGE, rates[r]);
		for (k = 0; k < 2; k++)
			seed_dtmf_collecting(k ? &b : &a, 1);
		run("mode 3, DTMF timed out", &a, &b, in, len, tag);

		build(&a, &b, CID_MODE_CID_MESSAGE, rates[r]);
		for (k = 0; k < 2; k++)
			seed_dtmf_c(k ? &b : &a);
		run("mode 3, DTMF terminated by C", &a, &b, in, len, tag);

		/*
		 * mode 5, the automatic arm's own four verdicts.  The FSK
		 * seeds move the mode to 2 and the DTMF ones to 3, so each
		 * case also pins one of the two transitions.
		 */
		build(&a, &b, CID_MODE_AUTOMATIC, rates[r]);
		for (k = 0; k < 2; k++)
			seed_fsk_message(k ? &b : &a, 1);
		len = blocklen(&a);
		run("mode 5, FSK message arrives", &a, &b, in, len, tag);
		diff_eq_int("mode 5 with an FSK message becomes mode 2 (%ld)",
			    a.ctx.mode, CID_MODE_FSK_DONE, tag);

		build(&a, &b, CID_MODE_AUTOMATIC, rates[r]);
		for (k = 0; k < 2; k++)
			seed_fsk_message(k ? &b : &a, 0);
		run("mode 5, FSK gives up", &a, &b, in, len, tag);

		build(&a, &b, CID_MODE_AUTOMATIC, rates[r]);
		for (k = 0; k < 2; k++)
			seed_dtmf_collecting(k ? &b : &a, 0);
		run("mode 5, DTMF collecting", &a, &b, in, len, tag);
		diff_eq_int("mode 5 with DTMF digits becomes mode 3 (%ld)",
			    a.ctx.mode, CID_MODE_CID_MESSAGE, tag);

		build(&a, &b, CID_MODE_AUTOMATIC, rates[r]);
		for (k = 0; k < 2; k++)
			seed_dtmf_collecting(k ? &b : &a, 1);
		run("mode 5, DTMF times out", &a, &b, in, len, tag);

		build(&a, &b, CID_MODE_AUTOMATIC, rates[r]);
		for (k = 0; k < 2; k++)
			seed_dtmf_c(k ? &b : &a);
		run("mode 5, DTMF terminated by C", &a, &b, in, len, tag);

		/*
		 * Both receivers seeded at once, which is the only way the
		 * automatic arm's two verdict tests both fire in one block --
		 * and the order matters, since the DTMF tests run first and
		 * the FSK ones can overwrite the mode they set.
		 */
		build(&a, &b, CID_MODE_AUTOMATIC, rates[r]);
		for (k = 0; k < 2; k++) {
			seed_fsk_message(k ? &b : &a, 1);
			seed_dtmf_collecting(k ? &b : &a, 0);
		}
		run("mode 5, both receivers answer", &a, &b, in, len, tag);

		/*
		 * Two blocks in one call with a seed that fires on the first:
		 * the result variables survive into the second turn of the
		 * loop, which nothing else here shows.
		 */
		build(&a, &b, CID_MODE_AUTOMATIC, rates[r]);
		for (k = 0; k < 2; k++)
			seed_dtmf_collecting(k ? &b : &a, 0);
		run("mode 5, two blocks in one call", &a, &b, in, 2 * len, tag);
	}
	rc |= diff_end();

	/* ---------------------------------------------------------------- */
	diff_begin("cid_progress: every arm of the object fired");
	diff_eq_int("calls that returned 0 (%ld)", seen_ret[0] > 0, 1,
		    seen_ret[0]);
	diff_eq_int("calls that returned 1 (%ld)", seen_ret[1] > 0, 1,
		    seen_ret[1]);
	diff_eq_int("calls that returned 2 (%ld)", seen_ret[2] > 0, 1,
		    seen_ret[2]);
	diff_eq_int("calls that returned 3 (%ld)", seen_ret[3] > 0, 1,
		    seen_ret[3]);
	diff_eq_int("calls that returned anything else (%ld)", seen_ret_other,
		    0, seen_ret_other);
	diff_eq_int("blocks run down the mode 0 arm (%ld)", seen_arm_fsk > 0, 1,
		    seen_arm_fsk);
	diff_eq_int("blocks run down the mode 1 arm (%ld)", seen_arm_dtmf > 0,
		    1, seen_arm_dtmf);
	diff_eq_int("blocks run down the mode 5 arm (%ld)", seen_arm_auto > 0,
		    1, seen_arm_auto);
	diff_eq_int("blocks run down the mode 3 arm (%ld)", seen_arm_message > 0,
		    1, seen_arm_message);
	diff_eq_int("blocks run down the no-receiver arm (%ld)",
		    seen_arm_none > 0, 1, seen_arm_none);
	diff_eq_int("calls with a 160-sample block (%ld)", seen_len_160 > 0, 1,
		    seen_len_160);
	diff_eq_int("calls with a 192-sample block (%ld)", seen_len_192 > 0, 1,
		    seen_len_192);
	diff_eq_int("calls that ran no block (%ld)", seen_blocks_0 > 0, 1,
		    seen_blocks_0);
	diff_eq_int("calls that ran one block (%ld)", seen_blocks_1 > 0, 1,
		    seen_blocks_1);
	diff_eq_int("calls that ran several blocks (%ld)", seen_blocks_many > 0,
		    1, seen_blocks_many);
	diff_eq_int("calls that ran a block on no new samples (%ld)",
		    seen_full_at_entry > 0, 1, seen_full_at_entry);
	diff_eq_int("calls whose *count was cleared from non-zero (%ld)",
		    seen_count_cleared > 0, 1, seen_count_cleared);
	diff_eq_int("mode transitions to 2 (%ld)", seen_mode_to_2 > 0, 1,
		    seen_mode_to_2);
	diff_eq_int("mode transitions to 3 (%ld)", seen_mode_to_3 > 0, 1,
		    seen_mode_to_3);
	diff_eq_int("mode 5 blocks that reset the FSK receiver (%ld)",
		    seen_fsk_reset > 0, 1, seen_fsk_reset);
	rc |= diff_end();

	return rc;
}
