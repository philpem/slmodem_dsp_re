/*
 * t_rxcid.c -- differential test of Rxcid.c's other three: reset_cid,
 * create_cid and cid_modem.  (pack_next_bit has its own coverage in
 * t_cidleaves; it is exercised here again through cid_modem, which is where
 * the original inlines it.)
 *
 * FOUR THINGS HAVE TO BE SHOWN, and two of them are the reason this file is
 * longer than the functions:
 *
 *   - the object comes out identical.  `struct cid` holds TWO pointers -- the
 *     resampler's coefficient table and its history buffer -- which hold two
 *     different addresses on the two sides and always will, so they are
 *     compared by what they POINT AT and blanked before diff_eq_obj sees the
 *     rest.  Blanking them without comparing their targets would silently
 *     stop testing the 180-byte filter, which is the only differential check
 *     V23_MRF_FILT can ever get: it is file-static on both sides and cannot be
 *     named from here.
 *
 *   - cid_modem's FOUR return values all appear, from the REFERENCE side.  A
 *     driver exercised only on silence returns 1 for ever and passes with
 *     every stage after the tone detector deleted, so the counters at the
 *     bottom require 1, 2, 3 and -1 to have been seen, plus the branches that
 *     do not show in a return value: the resampler running, the AGC
 *     re-adapting `gain`, and the mark-tone confidence both rising and being
 *     cleared.  Finding F134's argument, and the counts are printed.
 *
 *   - THE BUFFERS ARE SIZED FROM THE INPUT, not from a comfortable constant.
 *     cid_modem copies `count` samples into a 206-short frame buffer and
 *     CID_FSD_demodulate writes into `cid->bits` -- 36 shorts -- with no bound
 *     of its own (deviation D973, and D956 is the same shape in V22FP_modem).
 *     Every block here is at most 160 samples, and the guard band after each
 *     object is checked after every call.
 *
 *   - reset_cid does NOT allocate a second resampler buffer.  It asks
 *     FPM_MRF_init to allocate only when `mrf.history` is NULL, so the pointer
 *     is required to be unchanged across a second reset.  Nothing else in the
 *     suite would notice a leak there.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/cid.h"
#include "dsplib/fpm_mrf.h"

extern void ref_reset_cid(struct cid *cid);
extern struct cid *ref_create_cid(struct cid *cid);
extern int ref_cid_modem(const short *samples, unsigned short count,
			 struct cid *cid);

#define GUARD	48
#define NTAPS	90

struct box {
	struct cid cid;
	unsigned char guard[GUARD];
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

/* Anti-vacuity, all measured on the REFERENCE side. */
static int calls;
static int seen_ret1, seen_ret2, seen_ret3, seen_retm1;
static int seen_conf_up, seen_conf_clear;
static int seen_resample, seen_gain_move;
static int seen_bytes;

/* ------------------------------------------------------------------ */

/*
 * Compare two objects that each own their own resampler buffer.  The two
 * pointer fields are compared by target and then blanked; everything else goes
 * through diff_eq_obj so a difference is reported as a field.
 */
static void
cmp_cid(const char *what, struct box *ours, struct box *ref, long tag)
{
	struct cid ca, cb;
	char buf[160];

	snprintf(buf, sizeof(buf), "%s: history_len (%%ld)", what);
	diff_eq_int(buf, ours->cid.mrf.history_len, ref->cid.mrf.history_len,
		    tag);

	if (ours->cid.mrf.cfg.coeff != 0 && ref->cid.mrf.cfg.coeff != 0) {
		snprintf(buf, sizeof(buf),
			 "%s: V23_MRF_FILT against the blob's (%%ld)", what);
		diff_eq_int(buf, memcmp(ours->cid.mrf.cfg.coeff,
					ref->cid.mrf.cfg.coeff,
					NTAPS * sizeof(short)), 0, tag);
	} else {
		snprintf(buf, sizeof(buf), "%s: coeff both NULL (%%ld)", what);
		diff_eq_int(buf, ours->cid.mrf.cfg.coeff == 0,
			    ref->cid.mrf.cfg.coeff == 0, tag);
	}

	if (ours->cid.mrf.history != 0 && ref->cid.mrf.history != 0 &&
	    ours->cid.mrf.history_len == ref->cid.mrf.history_len &&
	    ours->cid.mrf.history_len > 0) {
		snprintf(buf, sizeof(buf), "%s: resampler history (%%ld)",
			 what);
		diff_eq_int(buf, memcmp(ours->cid.mrf.history,
					ref->cid.mrf.history,
					(size_t)ours->cid.mrf.history_len *
					sizeof(short)), 0, tag);
	}

	ca = ours->cid;
	cb = ref->cid;
	ca.mrf.cfg.coeff = cb.mrf.cfg.coeff = 0;
	ca.mrf.history = cb.mrf.history = 0;

	snprintf(buf, sizeof(buf), "%s: object after %%ld", what);
	diff_eq_obj(buf, struct cid, &ca, &cb, tag);

	snprintf(buf, sizeof(buf), "%s: guard after %%ld", what);
	diff_eq_int(buf, memcmp(ours->guard, ref->guard, GUARD), 0, tag);
}

/*
 * A pair of constructed objects, each with its own resampler buffer.  Built
 * with create_cid so the pointer arrangement is the object's own; the two
 * boxes are byte-identical afterwards apart from those pointers.
 */
static void
build(struct box *ours, struct box *ref, short rate)
{
	memset(ours, 0, sizeof(*ours));
	memset(ref, 0, sizeof(*ref));
	memset(ours->guard, 0x5a, GUARD);
	memset(ref->guard, 0x5a, GUARD);

	create_cid(&ours->cid);
	ref_create_cid(&ref->cid);

	ours->cid.rate = rate;
	ref->cid.rate = rate;
}

static void
run(const char *what, struct box *ours, struct box *ref, const short *in,
    int n, long tag)
{
	char buf[160];
	short conf_before, len_before;
	int gain_before;
	int ra, rb;

	conf_before = ref->cid.mark_conf;
	gain_before = ref->cid.gain;
	len_before = ref->cid.pack_len;

	ra = ref_cid_modem(in, (unsigned short)n, &ref->cid);
	rb = cid_modem(in, (unsigned short)n, &ours->cid);

	snprintf(buf, sizeof(buf), "%s: return (%%ld)", what);
	diff_eq_int(buf, rb, ra, tag);
	cmp_cid(what, ours, ref, tag);

	calls++;
	if (ra == 1)
		seen_ret1++;
	else if (ra == 2)
		seen_ret2++;
	else if (ra == 3)
		seen_ret3++;
	else if (ra == -1)
		seen_retm1++;

	if (ref->cid.mark_conf > conf_before)
		seen_conf_up++;
	if (ref->cid.mark_conf == 0 && conf_before != 0)
		seen_conf_clear++;
	if (ref->cid.gain != gain_before)
		seen_gain_move++;
	if (ref->cid.pack_len != len_before)
		seen_bytes++;
}

/* ------------------------------------------------------------------ */

static void
tone(short *out, int n, double fs, double f, double amp, double *ph)
{
	int i;

	for (i = 0; i < n; i++) {
		double v = amp * sin(*ph);

		*ph += 2.0 * 3.14159265358979323846 * f / fs;
		if (v > 32767.0)
			v = 32767.0;
		if (v < -32768.0)
			v = -32768.0;
		out[i] = (short)v;
	}
}

static void
noise(short *out, int n, double amp)
{
	int i;

	for (i = 0; i < n; i++)
		out[i] = (short)(((double)(long)(rnd() % 20001UL) - 10000.0)
				 / 10000.0 * amp);
}

/*
 * Bell 202 as Caller ID uses it: 1200 Hz is a mark (1), 2200 Hz a space (0),
 * 1200 baud, continuous phase.  `bits` is written LSB-first by the caller.
 */
static void
fsk(short *out, int n, double fs, const unsigned char *bits, int nbits,
    int *bitpos, double *ph, double *frac)
{
	double spb = fs / 1200.0;
	int i;

	for (i = 0; i < n; i++) {
		double f = bits[*bitpos % nbits] ? 1200.0 : 2200.0;

		out[i] = (short)(6000.0 * sin(*ph));
		*ph += 2.0 * 3.14159265358979323846 * f / fs;
		*frac += 1.0;
		if (*frac >= spb) {
			*frac -= spb;
			(*bitpos)++;
		}
	}
}

/* ------------------------------------------------------------------ */

static int
run_reset(void)
{
	struct box a, b;
	short *hist_ours, *hist_ref;
	int rc;

	diff_begin("reset_cid and create_cid");

	/*
	 * A fresh object.  create_cid is what nulls `mrf.history`, so this is
	 * also the only path on which reset_cid may allocate.
	 */
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	memset(a.guard, 0x5a, GUARD);
	memset(b.guard, 0x5a, GUARD);
	diff_eq_int("create_cid returns its argument (%ld)",
		    create_cid(&a.cid) == &a.cid, 1, 0);
	diff_eq_int("ref create_cid returns its argument (%ld)",
		    ref_create_cid(&b.cid) == &b.cid, 1, 0);
	cmp_cid("create_cid on zeroed storage", &a, &b, 0);

	diff_eq_int("create_cid seeds rate (%ld)", a.cid.rate, 8000, 0);
	diff_eq_int("create_cid seeds threshold (%ld)", a.cid.threshold, 2, 0);
	diff_eq_int("create_cid seeds mark_conf_step (%ld)",
		    a.cid.mark_conf_step, 9, 0);
	diff_eq_int("reset_cid sizes the history (%ld)",
		    a.cid.mrf.history_len, 10, 0);
	diff_eq_int("reset_cid sets nine branches (%ld)",
		    a.cid.mrf.cfg.branches, 9, 0);
	diff_eq_int("reset_cid sets ten decimate (%ld)",
		    a.cid.mrf.cfg.decimate, 10, 0);
	diff_eq_int("reset_cid sets ninety taps (%ld)", a.cid.mrf.cfg.taps,
		    NTAPS, 0);

	/*
	 * A SECOND RESET MUST NOT REALLOCATE.  `mrf.history` is not NULL any
	 * more, so FPM_MRF_init is asked to reuse it -- which is what stops
	 * every reset leaking twenty bytes, and nothing else here would see
	 * it.
	 */
	hist_ours = a.cid.mrf.history;
	hist_ref = b.cid.mrf.history;
	reset_cid(&a.cid);
	ref_reset_cid(&b.cid);
	diff_eq_int("second reset keeps our buffer (%ld)",
		    a.cid.mrf.history == hist_ours, 1, 0);
	diff_eq_int("second reset keeps the blob's buffer (%ld)",
		    b.cid.mrf.history == hist_ref, 1, 0);
	cmp_cid("second reset_cid", &a, &b, 1);

	/*
	 * A DIRTY object: every field the reset is supposed to clear starts
	 * non-zero, so a store this reconstruction omits shows up rather than
	 * agreeing with a zero that was already there.  The three settings and
	 * the resampler pointer are put back by hand, because reset_cid keeps
	 * the first three and dereferences the fourth.
	 */
	fill_bytes(&a.cid, sizeof(a.cid));
	memcpy(&b.cid, &a.cid, sizeof(a.cid));
	a.cid.mrf.history = hist_ours;
	b.cid.mrf.history = hist_ref;
	a.cid.mrf.history_len = 10;
	b.cid.mrf.history_len = 10;
	reset_cid(&a.cid);
	ref_reset_cid(&b.cid);
	cmp_cid("reset_cid over a dirty object", &a, &b, 2);
	diff_eq_int("reset_cid keeps rate (%ld)", a.cid.rate, b.cid.rate, 2);
	diff_eq_int("reset_cid keeps threshold (%ld)", a.cid.threshold,
		    b.cid.threshold, 2);
	diff_eq_int("reset_cid keeps mark_conf_step (%ld)",
		    a.cid.mark_conf_step, b.cid.mark_conf_step, 2);
	diff_eq_int("reset_cid cleared pack_len (%ld)", a.cid.pack_len, 0, 2);
	diff_eq_int("reset_cid cleared data[119] (%ld)", a.cid.data[119], 0,
		    2);
	diff_eq_int("reset_cid cleared short_152 (%ld)", a.cid.short_152, 0,
		    2);

	/*
	 * create_cid over dirty storage: it does NOT clear the pointer field
	 * by accident, it clears it deliberately, and the reset that follows
	 * therefore allocates afresh.
	 */
	fill_bytes(&a.cid, sizeof(a.cid));
	memcpy(&b.cid, &a.cid, sizeof(a.cid));
	create_cid(&a.cid);
	ref_create_cid(&b.cid);
	cmp_cid("create_cid over a dirty object", &a, &b, 3);

	rc = diff_end();
	return rc;
}

/* ------------------------------------------------------------------ */

static int
run_modem(short rate, double fs)
{
	struct box a, b;
	short in[160];
	char label[80];
	double ph = 0.0, frac = 0.0;
	int bitpos = 0;
	int i, rc;

	snprintf(label, sizeof(label), "cid_modem at %d", (int)rate);
	diff_begin(label);

	/* Silence, and the shortest block the divide allows. */
	build(&a, &b, rate);
	memset(in, 0, sizeof(in));
	run("silence", &a, &b, in, 80, 0);
	run("count 1", &a, &b, in, 1, 0);
	run("count 160", &a, &b, in, 160, 0);

	/* Noise: loud, and nothing like 1200 Hz. */
	build(&a, &b, rate);
	for (i = 0; i < 8; i++) {
		noise(in, 80, 9000.0);
		run("noise", &a, &b, in, 80, i);
	}

	/* An out-of-band tone, which must not build confidence. */
	build(&a, &b, rate);
	ph = 0.0;
	for (i = 0; i < 8; i++) {
		tone(in, 80, fs, 2200.0, 8000.0, &ph);
		run("2200 Hz", &a, &b, in, 80, i);
	}

	/*
	 * The mark tone, long enough to cross both thresholds -- four blocks
	 * of 80 at 8000 is 40 ms -- and then well past them, so the resampler,
	 * the AGC and the demodulator all run.
	 */
	build(&a, &b, rate);
	ph = 0.0;
	for (i = 0; i < 24; i++) {
		tone(in, 80, fs, 1200.0, 8000.0, &ph);
		run("1200 Hz mark", &a, &b, in, 80, i);
	}

	/* And then it stops, which must clear the confidence again. */
	for (i = 0; i < 6; i++) {
		memset(in, 0, sizeof(in));
		run("mark then silence", &a, &b, in, 80, i);
	}

	/*
	 * The real thing: mark tone into a Bell 202 bit stream, driven long
	 * enough for the framer to leave state 0, take a seizure run and
	 * assemble bytes.
	 */
	{
		static const unsigned char msg[] = {
			/* seizure, then a mark run, then framed bytes */
			0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			0, 0, 1, 0, 0, 0, 0, 0, 1, 1,
			0, 1, 0, 0, 0, 0, 0, 0, 1, 1,
			0, 1, 1, 0, 0, 0, 0, 0, 1, 1
		};

		build(&a, &b, rate);
		ph = 0.0;
		for (i = 0; i < 16; i++) {
			tone(in, 80, fs, 1200.0, 8000.0, &ph);
			run("preamble", &a, &b, in, 80, i);
		}
		bitpos = 0;
		frac = 0.0;
		for (i = 0; i < 60; i++) {
			fsk(in, 80, fs, msg, (int)sizeof(msg), &bitpos, &ph,
			    &frac);
			run("fsk data", &a, &b, in, 80, i);
		}
	}

	/*
	 * The message-length and checksum arithmetic, reached by seeding the
	 * framer rather than by hoping the demodulator lands on it.  The
	 * confidence is planted at exactly the 40 ms threshold so stage 2 is
	 * skipped and the length test is what decides.
	 *
	 * m = (rate == 9600 ? 49152 : 40960) / 80, so the threshold is
	 * m * 18 >> 8: 36 at 8000 and 43 at 9600.
	 */
	{
		short thresh = (short)(((rate == CID_RATE_9600 ? 49152 : 40960)
					/ 80) * 18 >> 8);
		int k;

		/* body length 5 -> msglen 8, and a checksum that agrees */
		build(&a, &b, rate);
		memset(in, 0, sizeof(in));
		for (k = 0; k < 2; k++) {
			struct box *o = k ? &b : &a;
			unsigned char s = 0;
			int j;

			o->cid.mark_conf = thresh;
			o->cid.pack_len = 10;
			o->cid.data[0] = 0x80;
			o->cid.data[1] = 5;
			for (j = 2; j < 7; j++)
				o->cid.data[j] = (unsigned char)(0x30 + j);
			for (j = 0; j < 7; j++)
				s = (unsigned char)(s + o->cid.data[j]);
			o->cid.data[7] = (unsigned char)-s;
		}
		run("checksum agrees", &a, &b, in, 80, 5);

		/* the same message with the checksum byte wrong */
		build(&a, &b, rate);
		for (k = 0; k < 2; k++) {
			struct box *o = k ? &b : &a;
			int j;

			o->cid.mark_conf = thresh;
			o->cid.pack_len = 10;
			o->cid.data[0] = 0x80;
			o->cid.data[1] = 5;
			for (j = 2; j < 8; j++)
				o->cid.data[j] = (unsigned char)(0x30 + j);
		}
		run("checksum disagrees", &a, &b, in, 80, 6);

		/* a declared length of zero: msglen 3, rejected outright */
		build(&a, &b, rate);
		for (k = 0; k < 2; k++) {
			struct box *o = k ? &b : &a;

			o->cid.mark_conf = thresh;
			o->cid.pack_len = 4;
			o->cid.data[1] = 0;
		}
		run("declared length zero", &a, &b, in, 80, 7);

		/* a declared length past the clamp: msglen pinned at 115 */
		build(&a, &b, rate);
		for (k = 0; k < 2; k++) {
			struct box *o = k ? &b : &a;

			o->cid.mark_conf = thresh;
			o->cid.pack_len = 4;
			o->cid.data[1] = 250;
		}
		run("declared length clamped", &a, &b, in, 80, 8);

		/* pack_len 2 exactly: the length is still the 115 default */
		build(&a, &b, rate);
		for (k = 0; k < 2; k++) {
			struct box *o = k ? &b : &a;

			o->cid.mark_conf = thresh;
			o->cid.pack_len = 2;
			o->cid.data[1] = 5;
		}
		run("pack_len at the boundary", &a, &b, in, 80, 9);

		/*
		 * pack_len already past the clamp, so the checksum runs at the
		 * clamped length of 115.  data[1] is 113 and not 250: at 250
		 * the object sums data[0..251] and reads two bytes PAST its own
		 * allocation, which is its shape but not something to fire
		 * deliberately at a stack object (D973).
		 */
		build(&a, &b, rate);
		for (k = 0; k < 2; k++) {
			struct box *o = k ? &b : &a;
			int j;

			o->cid.mark_conf = thresh;
			o->cid.pack_len = 118;
			for (j = 0; j < 116; j++)
				o->cid.data[j] = (unsigned char)(j * 7 + 1);
			o->cid.data[1] = 113;
		}
		run("pack_len past the clamp", &a, &b, in, 80, 10);
	}

	/*
	 * THE TWO THRESHOLDS, SWEPT INTEGER BY INTEGER.  Both are derived from
	 * the block length rather than stored, so a wrong multiplier moves
	 * them by one or two and every input that steps the confidence by
	 * `mark_conf_step` steps straight over the gap: an 18 written as 17
	 * survives the whole rest of this file.  `mark_conf` is planted at every value
	 * around both thresholds instead, once against a block the detector
	 * rejects (which pins the upper gate, since crossing it is what stops
	 * the detector running and clearing the count) and once against a
	 * block it accepts (which pins the lower one, since the confidence
	 * lands between the two).
	 */
	{
		int p;

		for (p = 0; p <= 56; p++) {
			build(&a, &b, rate);
			a.cid.mark_conf = (short)p;
			b.cid.mark_conf = (short)p;
			memset(in, 0, sizeof(in));
			run("threshold sweep, silence", &a, &b, in, 80, p);

			build(&a, &b, rate);
			a.cid.mark_conf = (short)p;
			b.cid.mark_conf = (short)p;
			ph = 0.0;
			tone(in, 80, fs, 1200.0, 8000.0, &ph);
			run("threshold sweep, mark tone", &a, &b, in, 80, p);
		}
	}

	/*
	 * THE GAIN FLOOR.  A full-scale block makes FPM_div_32's reciprocal
	 * shift the whole product away, so with `gain` still at the zero
	 * reset_cid left it the new gain computes as zero and is floored to
	 * one.  Nothing else here reaches that, and without it the floor is
	 * dead code no test can see.
	 */
	{
		int amp;

		for (amp = 0; amp < 4; amp++) {
			static const double amps[4] = { 32767.0, 20000.0,
							300.0, 5.0 };
			build(&a, &b, rate);
			/* inside the adaptation window: below the upper gate,
			 * and one detected block puts it above the lower one */
			a.cid.mark_conf = (short)(((rate == CID_RATE_9600
						    ? 49152 : 40960) / 80)
						  * 12 >> 8);
			b.cid.mark_conf = a.cid.mark_conf;
			ph = 0.0;
			for (i = 0; i < 4; i++) {
				tone(in, 80, fs, 1200.0, amps[amp], &ph);
				run("gain floor", &a, &b, in, 80,
				    (long)amps[amp]);
			}
		}
	}

	/*
	 * THE CLAMP ITSELF, at the one value that can see it: a declared body
	 * length of 113 makes msglen 116, so 115 and 116 differ in whether a
	 * pack_len of exactly 115 is "enough".  112, 113 and 114 are swept
	 * against pack_len 114, 115 and 116 for the same reason.
	 */
	{
		short thresh2 = (short)(((rate == CID_RATE_9600 ? 49152 : 40960)
					 / 80) * 18 >> 8);
		int len, pl, k;

		memset(in, 0, sizeof(in));

		/*
		 * And the OTHER end of the same test: `pack_len > 2` is what
		 * decides whether the declared length is believed at all, and
		 * it only separates from `> 1` at a pack_len of exactly 2 with
		 * a declared length of zero -- where believing it returns -1
		 * and not believing it demodulates.
		 */
		for (pl = 0; pl <= 5; pl++) {
			for (len = 0; len <= 2; len++) {
				build(&a, &b, rate);
				for (k = 0; k < 2; k++) {
					struct box *o = k ? &b : &a;

					o->cid.mark_conf = thresh2;
					o->cid.pack_len = (short)pl;
					o->cid.data[1] = (unsigned char)len;
				}
				run("short pack_len", &a, &b, in, 80,
				    pl * 100 + len);
			}
		}

		for (len = 111; len <= 115; len++) {
			for (pl = 113; pl <= 117; pl++) {
				build(&a, &b, rate);
				for (k = 0; k < 2; k++) {
					struct box *o = k ? &b : &a;
					int j;

					o->cid.mark_conf = thresh2;
					o->cid.pack_len = (short)pl;
					for (j = 0; j < 120; j++)
						o->cid.data[j] =
						    (unsigned char)(j * 3 + 5);
					o->cid.data[1] = (unsigned char)len;
				}
				run("msglen clamp", &a, &b, in, 80,
				    len * 1000 + pl);
			}
		}
	}

	/*
	 * Seeded objects: every field the driver reads starts at a value it
	 * did not choose, so a field this reconstruction reads at the wrong
	 * offset shows up.  `rate` and the resampler pointers are put back
	 * because the driver dereferences one and switches on the other.
	 */
	{
		struct box sa, sb;
		int k;

		build(&sa, &sb, rate);
		for (k = 0; k < 12; k++) {
			short *ho = sa.cid.mrf.history;
			short *hr = sb.cid.mrf.history;
			struct fpm_mrf_cfg co = sa.cid.mrf.cfg;
			struct fpm_mrf_cfg cr = sb.cid.mrf.cfg;

			fill_bytes(&sa.cid, sizeof(sa.cid));
			memcpy(&sb.cid, &sa.cid, sizeof(sa.cid));
			sa.cid.mrf.cfg = co;
			sb.cid.mrf.cfg = cr;
			sa.cid.mrf.history = ho;
			sb.cid.mrf.history = hr;
			sa.cid.mrf.history_len = 10;
			sb.cid.mrf.history_len = 10;
			sa.cid.mrf.need = 1;
			sb.cid.mrf.need = 1;
			sa.cid.mrf.phase = 0;
			sb.cid.mrf.phase = 0;
			sa.cid.mrf.widx = 0;
			sb.cid.mrf.widx = 0;
			sa.cid.rate = rate;
			sb.cid.rate = rate;
			/*
			 * EVERY FIELD A CALLEE USES AS A SUBSCRIPT, not only
			 * every field it dereferences -- and a blob-against-
			 * blob dry run cannot catch an unplanted one, because
			 * both sides read the same wild index into the same
			 * array and agree (D955, F8587).  CID_FSD_demodulate
			 * pre-increments `ac_idx` and `lpf_idx` and wraps only
			 * on the HIGH side, so a negative seed indexes off the
			 * front of `ac_hist` and `lpf_hist`; `pack_len`
			 * subscripts `data` with no bound at all.
			 */
			sa.cid.ac_idx = (short)(k % 5);
			sb.cid.ac_idx = sa.cid.ac_idx;
			sa.cid.lpf_idx = (short)(k % 17);
			sb.cid.lpf_idx = sa.cid.lpf_idx;
			sa.cid.pack_len = (short)(k * 7);
			sb.cid.pack_len = sa.cid.pack_len;
			sa.cid.pack_pos = (short)(k % 8);
			sb.cid.pack_pos = sa.cid.pack_pos;
			sa.cid.pack_state = (short)(k % 4);
			sb.cid.pack_state = sa.cid.pack_state;
			sa.cid.threshold = (short)(k % 5);
			sb.cid.threshold = sa.cid.threshold;
			sa.cid.mark_conf_step = 9;
			sb.cid.mark_conf_step = 9;

			noise(in, 96, 7000.0);
			run("seeded", &sa, &sb, in, 96, k);
		}
	}

	rc = diff_end();
	return rc;
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;

	rc |= run_reset();
	rc |= run_modem(CID_RATE_8000, 8000.0);
	rc |= run_modem(CID_RATE_9600, 9600.0);

	diff_begin("cid_modem coverage");
	printf("    cid_modem calls compared: %d "
	       "(ret 1/2/3/-1: %d/%d/%d/%d; conf up %d, cleared %d; "
	       "gain moved %d; bytes framed %d)\n",
	       calls, seen_ret1, seen_ret2, seen_ret3, seen_retm1,
	       seen_conf_up, seen_conf_clear, seen_gain_move, seen_bytes);
	diff_eq_int("calls compared (%ld)", calls > 100, 1, calls);
	diff_eq_int("blocks that returned 1, still hunting (%ld)",
		    seen_ret1 > 0, 1, seen_ret1);
	diff_eq_int("blocks that returned 2, collecting (%ld)",
		    seen_ret2 > 0, 1, seen_ret2);
	diff_eq_int("blocks that returned 3, checksum agreed (%ld)",
		    seen_ret3 > 0, 1, seen_ret3);
	diff_eq_int("blocks that returned -1, rejected (%ld)",
		    seen_retm1 > 0, 1, seen_retm1);
	diff_eq_int("blocks that raised the confidence (%ld)",
		    seen_conf_up > 0, 1, seen_conf_up);
	diff_eq_int("blocks that cleared it (%ld)", seen_conf_clear > 0, 1,
		    seen_conf_clear);
	diff_eq_int("blocks that moved the AGC gain (%ld)",
		    seen_gain_move > 0, 1, seen_gain_move);
	diff_eq_int("blocks that framed a byte (%ld)", seen_bytes > 0, 1,
		    seen_bytes);
	rc |= diff_end();

	(void)seen_resample;
	return rc;
}
