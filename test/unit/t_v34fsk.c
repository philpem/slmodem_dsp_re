/*
 * t_v34fsk.c -- differential test of the V.34 FSK discriminator and slicer.
 *
 * This is the first V.34 module whose arguments reach deep into the
 * enclosing V.34 object -- +0x402, +0x80c4, +0xaad0, +0xaae6 and +0xab00 --
 * and the object is not reconstructed.  So both sides are handed a
 * `struct v34_object`, which is mostly padding with those five fields named,
 * and the whole 43 KB of it is compared after every call.  That is a stronger
 * check than comparing the named fields would be: if the reconstruction wrote
 * one short outside them the padding would catch it, and the padding is where
 * a wrong offset would land.
 *
 * The delay line hangs off a pointer, so each side gets its own -- a shared
 * one would have the two demodulators feeding each other.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34det.h"	/* costbl, to build the test signal */

extern void ref_fskdetect(void *obj, const short *in, short *out,
			  const void *cfg);
extern void ref_fskdemodulate(void *obj, const short *in, void *st);
extern void ref_V34InitializeImplementationSpecific(void *obj);
extern const short ref_intcoef1[V34_FSK_TAPS];
extern const short ref_intcoef2[V34_FSK_TAPS];
extern const short ref_intcoef3[V34_FSK_TAPS];

#define SRATE		9600		/* the V.34 host rate            */
#define NSAMP		2048

/* Coverage. */
static int saw_bit;		/* the slicer shifted a bit in            */
static int saw_reset;		/* the bit clock was restarted            */
static int saw_inhibit;		/* the switched-off path ran              */
static int saw_wrap;		/* the phase counter reached its wrap     */

static short sig[NSAMP];

static struct v34_object obj_a, obj_b;
static struct v34_fskdelay dly_a, dly_b;

/*
 * Compare the whole V.34 object, padding included.
 *
 * Skips only the one pointer field, because the two sides legitimately hold
 * different addresses for their own delay lines.  Everything else -- named or
 * not -- must agree.
 */
static void
compare_all(const char *what)
{
	const unsigned char *a = (const unsigned char *)&obj_a;
	const unsigned char *b = (const unsigned char *)&obj_b;
	unsigned i;

	for (i = 0; i < sizeof(obj_a); i++) {
		if (i >= __builtin_offsetof(struct v34_object, echo0)
			 + __builtin_offsetof(struct v34_echo, coeff_frac)
		    && i < __builtin_offsetof(struct v34_object, echo0)
			   + __builtin_offsetof(struct v34_echo, coeff_frac)
			   + sizeof(void *))
			continue;
		diff_eq_int(what, a[i], b[i], i);
	}

	for (i = 0; i < sizeof(dly_a); i++)
		diff_eq_int("the delay line", ((unsigned char *)&dly_a)[i],
			    ((unsigned char *)&dly_b)[i], i);
}

/*
 * Both objects to a known state.  0xa5 first, so a field neither function
 * writes shows up as the fill on both sides rather than as an accidental
 * zero, then the five named fields to what a caller would have set.
 */
static void
setup(const struct v34_fsk *cfg)
{
	memset(&obj_a, HARNESS_MALLOC_FILL, sizeof(obj_a));
	memset(&obj_b, HARNESS_MALLOC_FILL, sizeof(obj_b));
	memset(&dly_a, HARNESS_MALLOC_FILL, sizeof(dly_a));
	memset(&dly_b, HARNESS_MALLOC_FILL, sizeof(dly_b));

	memset(obj_a.fsk_interp, 0, sizeof(obj_a.fsk_interp));
	memset(obj_b.fsk_interp, 0, sizeof(obj_b.fsk_interp));
	memset(obj_a.fsk_lpf, 0, sizeof(obj_a.fsk_lpf));
	memset(obj_b.fsk_lpf, 0, sizeof(obj_b.fsk_lpf));
	memset(dly_a.line, 0, sizeof(dly_a.line));
	memset(dly_b.line, 0, sizeof(dly_b.line));

	/*
	 * Point each side's canceller at its own scratch array rather than at
	 * its own `echo0_frac`, so the two sides cannot share storage and the
	 * delay line stays comparable on its own.  The object does point it
	 * at echo0_frac (finding 100); that aliasing is asserted separately
	 * once V34InitializeImplementationSpecific is driven.
	 */
	obj_a.echo0.coeff_frac = (short *)&dly_a;
	obj_b.echo0.coeff_frac = (short *)&dly_b;
	obj_a.fsk_inhibit = obj_b.fsk_inhibit = 0;
	obj_a.fsk = *cfg;
	obj_b.fsk = *cfg;
}

/* An FSK signal: `hz0` for a 0 bit, `hz1` for a 1, `spb` samples per bit. */
static void
fill_fsk(int hz0, int hz1, int spb, int amp, unsigned pattern)
{
	unsigned phase = 0;
	int i;

	for (i = 0; i < NSAMP; i++) {
		int bit = (pattern >> ((i / spb) & 31)) & 1;
		int hz = bit ? hz1 : hz0;

		phase = (phase + (unsigned)(16384L * hz / SRATE)) & 0x3fff;
		sig[i] = (short)((costbl[phase >> 6] * amp) >> 14);
	}
}

/*
 * Drive both sides a block at a time through `fskdetect` alone, so the
 * discriminator is compared without the slicer's state on top of it.
 */
static void
run_detect(const char *what, const struct v34_fsk *cfg)
{
	int i;

	setup(cfg);

	for (i = 0; i + V34_FSK_BLOCK <= NSAMP; i += V34_FSK_BLOCK) {
		short out_a[V34_FSK_PHASES], out_b[V34_FSK_PHASES];
		int k;

		memset(out_a, 0x5a, sizeof(out_a));
		memset(out_b, 0x5a, sizeof(out_b));

		fskdetect(&obj_a, sig + i, out_a, &obj_a.fsk);
		ref_fskdetect(&obj_b, sig + i, out_b, &obj_b.fsk);

		for (k = 0; k < V34_FSK_PHASES; k++)
			diff_eq_int(what, out_a[k], out_b[k], i + k);
		compare_all(what);
	}
}

/* And the whole thing, slicer included.  Returns the bits recovered. */
static int
run_demod(const char *what, const struct v34_fsk *cfg, int inhibit,
	  int quiet)
{
	int i;

	setup(cfg);
	obj_a.fsk_inhibit = obj_b.fsk_inhibit = (short)inhibit;

	for (i = 0; i + V34_FSK_BLOCK <= NSAMP; i += V34_FSK_BLOCK) {
		short before_phase = obj_a.fsk.phase;
		short before_bits = obj_a.fsk.nbits;

		fskdemodulate(&obj_a, sig + i, &obj_a.fsk);
		ref_fskdemodulate(&obj_b, sig + i, &obj_b.fsk);
		compare_all(what);

		if (obj_a.fsk.nbits != before_bits)
			saw_bit++;
		/*
		 * Three interpolated samples go by per call and each steps
		 * the phase by one, so a call that did not advance it by
		 * exactly three restarted the clock -- on a sign change, or
		 * on the counter's wrap.  Sampling `phase == 0` between calls
		 * would miss almost all of them, because the reset is
		 * followed by up to two more increments before the call
		 * returns.
		 */
		if (obj_a.fsk.phase != (short)(before_phase + V34_FSK_PHASES))
			saw_reset++;
		/* Silence never changes sign, so there a decrease is a wrap. */
		if (quiet && obj_a.fsk.phase < before_phase)
			saw_wrap++;
		if (inhibit)
			saw_inhibit++;
	}

	return obj_a.fsk.nbits;
}

int
main(void)
{
	/*
	 * A configuration in the shape the handshake uses: a discriminator
	 * lag of 8 taps at the 3x interpolated rate, no offset, and a bit
	 * length that makes the slicer sample near the middle of each bit.
	 */
	static const struct v34_fsk cfg = {
		.delay = 8, .offset = 0,
		.bit_lo = 0, .bit_hi = 1,
		.bit_len = 24, .resync_next = 12,
		.phase = 0, .next = 12, .nbits = 0, .sr = 0, .prev = 0
	};
	struct v34_fsk c;
	int rc = 0;
	int bits, bits_inhibited;
	int i;

	diff_begin("v34 fsk: the interpolator coefficients");
	for (i = 0; i < V34_FSK_TAPS; i++) {
		diff_eq_int("intcoef1[%ld]", intcoef1[i], ref_intcoef1[i], i);
		diff_eq_int("intcoef2[%ld]", intcoef2[i], ref_intcoef2[i], i);
		diff_eq_int("intcoef3[%ld]", intcoef3[i], ref_intcoef3[i], i);
	}
	/*
	 * Not differential: the structure the three tables have.  A
	 * transcription that swapped 1 and 3, or reversed one of them, would
	 * compare equal above only if it were wrong on BOTH sides -- which it
	 * cannot be -- but this states the property explicitly so a future
	 * regeneration has something to fail against.
	 */
	for (i = 0; i < V34_FSK_TAPS; i++) {
		diff_eq_int("phase 3 is phase 1 reversed at %ld", intcoef3[i],
			    intcoef1[V34_FSK_TAPS - 1 - i], i);
		diff_eq_int("phase 2 is symmetric at %ld", intcoef2[i],
			    intcoef2[V34_FSK_TAPS - 1 - i], i);
	}
	rc |= diff_end();

	diff_begin("v34 fskdetect: the discriminator alone");

	fill_fsk(1180, 980, 24, 10000, 0xb2c5a63d);
	run_detect("V.21-rate FSK[%ld]", &cfg);

	/* A single tone: the discriminator output should sit still. */
	fill_fsk(1180, 1180, 24, 10000, 0);
	run_detect("one tone[%ld]", &cfg);

	memset(sig, 0, sizeof(sig));
	run_detect("silence[%ld]", &cfg);

	/*
	 * Full scale, which is where the accumulators wrap.  Twelve taps of a
	 * Q14 coefficient against a full-scale sample leave 32 bits, and the
	 * original carries round rather than saturating.
	 */
	fill_fsk(1180, 980, 24, 32767, 0xb2c5a63d);
	run_detect("full scale[%ld]", &cfg);

	/* A delay of zero squares the sample instead of correlating it. */
	c = cfg;
	c.delay = 0;
	run_detect("zero discriminator delay[%ld]", &c);

	/* And the far end of the line. */
	c = cfg;
	c.delay = V34_FSK_DELAY_LINE - 1;
	run_detect("maximum discriminator delay[%ld]", &c);

	/* A non-zero offset moves the slicing point. */
	c = cfg;
	c.offset = 3000;
	run_detect("with an offset[%ld]", &c);

	rc |= diff_end();

	diff_begin("v34 fskdemodulate: the slicer");

	fill_fsk(1180, 980, 24, 10000, 0xb2c5a63d);
	bits = run_demod("V.21-rate FSK[%ld]", &cfg, 0, 0);

	/* Switched off: nothing must move, including the detector's state. */
	bits_inhibited = run_demod("inhibited[%ld]", &cfg, 1, 0);

	/*
	 * A bit length short enough that the phase counter's wrap -- at
	 * bit_len * 256 -- is reachable inside this buffer.  With bit_len 2
	 * the wrap is at 512 and the interpolated stream is 3 per input
	 * sample, so it is crossed several times.
	 */
	c = cfg;
	c.bit_len = 2;
	c.resync_next = 1;
	c.next = 1;
	fill_fsk(1180, 980, 24, 10000, 0xb2c5a63d);
	run_demod("a short bit and a wrapping counter[%ld]", &c, 0, 0);

	/*
	 * ...which, on a live signal, never actually wraps: an FSK stream
	 * crosses zero constantly and every crossing resets the phase long
	 * before it reaches bit_len * 256.  Silence is the only input that
	 * lets the counter run, because a stream of zeroes never changes sign.
	 * That is worth a test of its own -- the wrap is the module's only
	 * protection against a clock left running with no signal, and nothing
	 * else here reaches it.
	 */
	memset(sig, 0, sizeof(sig));
	run_demod("silence, so the counter can reach its wrap[%ld]", &c, 0, 1);

	/* Inverted bit sense, which only the slicer sees. */
	c = cfg;
	c.bit_lo = 1;
	c.bit_hi = 0;
	fill_fsk(1180, 980, 24, 10000, 0xb2c5a63d);
	run_demod("inverted bit sense[%ld]", &c, 0, 0);

	rc |= diff_end();

	diff_begin("v34 fsk: it behaves like a demodulator");
	printf("  bits recovered: %d, inhibited %d\n", bits, bits_inhibited);
	diff_eq_int("a live signal produces bits", bits > 0, 1, bits);
	diff_eq_int("an inhibited one produces none", bits_inhibited, 0,
		    bits_inhibited);
	/*
	 * 2048 samples at 24 samples per bit is about 85 bits at the input
	 * rate.  A slicer that sampled at the interpolated rate instead would
	 * produce three times that, and one that never resynchronised would
	 * drift; a wide band catches both without pinning an exact count that
	 * start-up transients would make brittle.
	 */
	diff_eq_int("and about the right number of them",
		    bits > 40 && bits < 200, 1, bits);
	rc |= diff_end();

	diff_begin("v34 InitializeImplementationSpecific");
	{
		static struct v34_object ia, ib;

		memset(&ia, HARNESS_MALLOC_FILL, sizeof(ia));
		memset(&ib, HARNESS_MALLOC_FILL, sizeof(ib));
		V34InitializeImplementationSpecific(&ia);
		ref_V34InitializeImplementationSpecific(&ib);

		/*
		 * Pointers cannot be compared across the two objects, so
		 * every installed one is checked as an OFFSET from its own
		 * base -- which is stronger than comparing values, because
		 * the offsets are what the layout actually asserts.
		 */
#define OFF(o, p)  ((long)((char *)(p) - (char *)&(o)))
		diff_eq_int("p_2074", OFF(ia, ia.p_2074), 0x146c, 0);
		diff_eq_int("echo0.dline", OFF(ia, ia.echo0.dline), 0x81f8, 0);
		diff_eq_int("echo0.coeff", OFF(ia, ia.echo0.coeff), 0x9018, 0);
		diff_eq_int("echo0.coeff_frac", OFF(ia, ia.echo0.coeff_frac),
			    0x80d8, 0);
		diff_eq_int("echo0.hist", OFF(ia, ia.echo0.hist), 0x8ee8, 0);
		diff_eq_int("echo1.dline", OFF(ia, ia.echo1.dline), 0x9278, 0);
		diff_eq_int("echo1.coeff", OFF(ia, ia.echo1.coeff), 0xa098, 0);
		diff_eq_int("echo1.coeff_frac", OFF(ia, ia.echo1.coeff_frac),
			    0x9158, 0);
		diff_eq_int("echo1.hist", OFF(ia, ia.echo1.hist), 0x9f68, 0);
		diff_eq_int("echo0.dlen", (long)ia.echo0.dlen, V34_ECHO_DLEN,
			    0);
		diff_eq_int("echo0.taps", (long)ia.echo0.taps, V34_ECHO_TAPS,
			    0);
		diff_eq_int("echo1.dlen", (long)ia.echo1.dlen, V34_ECHO_DLEN,
			    0);
		diff_eq_int("echo1.taps", (long)ia.echo1.taps, V34_ECHO_TAPS,
			    0);
		/* And the reference installed exactly the same offsets. */
		diff_eq_int("ref echo0.dline", OFF(ib, ib.echo0.dline),
			    OFF(ia, ia.echo0.dline), 0);
		diff_eq_int("ref echo1.coeff", OFF(ib, ib.echo1.coeff),
			    OFF(ia, ia.echo1.coeff), 0);
		diff_eq_int("ref p_2074", OFF(ib, ib.p_2074),
			    OFF(ia, ia.p_2074), 0);

		/*
		 * D30: `cursor` is loaded with the OLD contents of `dline`,
		 * not with the new base.  Under the fill that is the fill
		 * pattern, and both sides must agree on it -- so this asserts
		 * the defect rather than the tidy behaviour.
		 */
		diff_eq_int("cursor holds the old dline, not the new",
			    (long)(unsigned)(unsigned long)ia.echo0.cursor,
			    (long)(unsigned)0xa5a5a5a5u, 0);
		diff_eq_int("and the reference agrees",
			    (long)(unsigned)(unsigned long)ib.echo0.cursor,
			    (long)(unsigned)0xa5a5a5a5u, 0);

		/* Finding 100, asserted: the FSK delay line IS echo0_frac. */
		diff_eq_int("the FSK delay line is echo0's fractional array",
			    OFF(ia, ia.echo0.coeff_frac),
			    OFF(ia, ia.echo0_frac), 0);
#undef OFF
	}
	rc |= diff_end();

	diff_begin("v34 fsk: coverage");
	diff_eq_int("the slicer shifted a bit in", saw_bit > 0, 1, 0);
	diff_eq_int("the bit clock was restarted", saw_reset > 0, 1, 0);
	diff_eq_int("the phase counter reached its wrap", saw_wrap > 0, 1, 0);
	diff_eq_int("the switched-off path ran", saw_inhibit > 0, 1, 0);
	rc |= diff_end();

	return rc;
}
