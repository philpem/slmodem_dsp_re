/*
 * V8Detector.c -- the tone and phase-reversal detector.
 *
 * The object's V8Detector.c, recovered from the address bracket its own
 * locals bound: `a` and `b` are .rodata+0x5724 and +0x572a, and the blob's
 * only functions between V8.c's last (`v8handshak`, ending 0x783a3) and
 * V8Dftc.c's first (`v8_dftupdate`, the earliest user of `v8_costbl`, at
 * 0x78b00) are exactly the six below.  `b` is reached from notch_filter and
 * v8_tone_detect and from nothing else.
 *
 * `a` and `b` are the two coefficient sections the tone filter runs over,
 * reconstructed here as `tone_in_a`/`tone_in_b`.  They were previously
 * split across v8sig.c (the filter functions) and v8util.c (the two init
 * functions); this file is the translation unit they belong to.
 */

#include "dsplib/debug.h"
#include "dsplib/v8.h"

static const short tone_in_a[2] = { -8057, 14787 };

/*
 * One biquad, direct form I, with the histories kept as four shorts: x1, x2
 * then y1, y2.  The original writes them back in a fixed order that matters,
 * because x2 takes the old x1 and y2 the old y1.
 */
static int
biquad(short *x, short *y, const short *b, const short *a, int in)
{
	int acc = in;

	acc += v8_mpyint(x[0], b[0]);
	acc += v8_mpyint(x[1], b[1]);
	acc -= v8_mpyint(y[0], a[0]);
	acc -= v8_mpyint(y[1], a[1]);

	x[1] = x[0];
	y[1] = y[0];
	x[0] = (short)in;
	y[0] = (short)acc;

	return acc;
}

/*
 * The fixed input biquad every tone detector shares, in Q14.  The originals
 * are called `a` and `b` -- local symbols of V8Detector.c, at .rodata+0x5724
 * and +0x572a -- and both are three entries: `a[0]` is 0x4000, the implicit
 * 1.0, and every reader skips it.  Kept two entries here because that is
 * what the code uses.
 */
static const short tone_in_b[3] = { 15565, -8057, 15565 };

/*
 * The same two sections again, standing on their own.
 *
 * Nothing in the object calls either of them.  The compiler inlined copies
 * into `v8_tone_detect` -- the same coefficients at .rodata+0x5726, the same
 * histories -- and left the out-of-line originals behind, so these are what
 * that code was written from.  Reconstructed because they are in the
 * translation unit, not because anything reaches them.
 *
 * They are not quite the inlined code, either: each product is truncated to
 * a short before it is accumulated here, and `v8_tone_detect` accumulates
 * the full result.  That is visible in the object as a `cwtl` after every
 * call, and it is why these cannot just call the helpers above.
 */
short
notch_filter(const short *in, struct v8_detector *d)
{
	int acc = 0;
	int i;

	d->acc_c[0] = *in;
	for (i = 0; i < 3; i++)
		acc += (short)v8_mpyint(d->acc_c[i], tone_in_b[i]);
	for (i = 0; i < 2; i++)
		acc -= (short)v8_mpyint(d->acc_d[i], tone_in_a[i]);

	d->acc_c[2] = d->acc_c[1];
	d->acc_d[2] = d->acc_d[1];
	d->acc_c[1] = d->acc_c[0];
	d->acc_d[1] = d->acc_d[0];
	d->acc_d[0] = (short)acc;
	return (short)acc;
}

short
biquad_filter(short in, struct v8_detector *d, const short *coeff)
{
	int acc = in >> 4;
	int stage1;
	/*
	 * `unsigned short`, AND THE OBJECT IS WHAT DECIDES IT.  Both history
	 * heads are read at four sites whose 32-bit result is discarded by a
	 * 16-bit store, so the extension is F614's dead one -- and lever 8
	 * measured that a dead extension follows the DECLARED TYPE OF THE LOCAL
	 * being loaded into, not the field's, not the store's and not a cast's.
	 * The blob loads all four `movzwl`; `short` here gave `movswl`.
	 *
	 * Ten cells, two distinct emissions: {short, unsigned short, int,
	 * unsigned int} x {`x0, y0`, `y0, x0`, one declaration each}, and
	 * `unsigned short` is the ONLY one of the four that emits the object's
	 * encoding -- an exhausted domain with a unique preimage on the type.
	 * It is value-preserving: both are read from `short` fields and stored
	 * straight back to `short` fields, so every store truncates.
	 *
	 * It does not close the symbol and is not expected to: 222 bytes
	 * against the blob's 224 either way, and the residual two are one
	 * `lea (%eax,%ebx,1),%ebx` where we emit `add %eax,%ebx`.  This is
	 * 7803's shape -- a named declaration property recovered on its own
	 * axis, which makes what is left legible.
	 */
	unsigned short x0, y0;
	int i;

	for (i = 0; i < 2; i++) {
		acc += (short)v8_mpyint(d->acc_a[i], coeff[i]);
		acc -= (short)v8_mpyint(d->acc_b[i], coeff[4 + i]);
	}

	/* Old before new, both histories. */
	y0 = d->acc_b[0];
	x0 = d->acc_a[0];
	d->acc_b[0] = (short)acc;
	d->acc_a[0] = (short)(in >> 4);
	d->acc_b[1] = (short)y0;
	d->acc_a[1] = (short)x0;

	stage1 = (short)(acc >> 4);
	acc = stage1;
	for (i = 0; i < 2; i++) {
		acc += (short)v8_mpyint(d->acc_a[2 + i], coeff[2 + i]);
		acc -= (short)v8_mpyint(d->acc_b[2 + i], coeff[6 + i]);
	}

	y0 = d->acc_b[2];
	x0 = d->acc_a[2];
	d->acc_b[2] = (short)acc;
	d->acc_a[2] = (short)stage1;
	d->acc_b[3] = (short)y0;
	d->acc_a[3] = (short)x0;
	return (short)acc;
}

/*
 * Arm the tone detector.
 *
 * The original has an empty inner loop here -- three iterations that do
 * nothing -- left over from whatever the accumulators used to be.  It has no
 * effect and is not reproduced; everything that touches memory is.
 */
void
v8_detectorinit(struct v8 *v, struct v8_detector *d, const short *table,
		short a3, short a4, short a5, short a6, short a7)
{
	int i;

	for (i = 0; i < 4; i++) {
		d->acc_a[i] = 0;
		d->acc_b[i] = 0;
	}
	for (i = 0; i < 3; i++) {
		d->acc_c[i] = 0;
		d->acc_d[i] = 0;
	}

	d->lo_rule = a3;
	d->counter = (short)-a5;
	d->f0c = 1;
	d->table = table;
	d->armed = 0;
	d->count_limit = a4;
	d->hi_thresh = a6;
	d->lo_thresh = a7;
	d->integrator = 0;
	d->warmup = 0;

	v->rx.flags |= V8_RX_DETECTOR_ARMED;
}

int
v8_tone_detect(struct v8 *v, struct v8_detector *d, short *in)
{
	while (in < v->rx.buf) {
		int acc;
		int stage1;
		int stage2;
		int i;

		/* The fixed input section, over its own three-deep history. */
		d->acc_c[0] = *in;
		acc = 0;
		for (i = 0; i < 3; i++)
			acc += v8_mpyint(d->acc_c[i], tone_in_b[i]);
		for (i = 0; i < 2; i++)
			acc -= v8_mpyint(d->acc_d[i], tone_in_a[i]);

		d->acc_c[2] = d->acc_c[1];
		d->acc_d[2] = d->acc_d[1];
		d->acc_c[1] = d->acc_c[0];
		d->acc_d[1] = d->acc_d[0];
		d->acc_d[0] = (short)acc;

		*in++ = (short)acc;

		/* Then the detector's own two sections. */
		stage1 = biquad(&d->acc_a[0], &d->acc_b[0], &d->table[0],
				&d->table[4], (short)acc >> 4);
		stage2 = biquad(&d->acc_a[2], &d->acc_b[2], &d->table[2],
				&d->table[6], (short)stage1 >> 4);

		d->integrator = (short)(v8_absfn((short)((short)stage2 >> 4))
				 + v8_mpyint(d->integrator, 0x3ccd));
	}

	if (d->lo_rule != 0) {
		if ((short)d->integrator < d->lo_thresh)
			d->counter = (short)(d->counter + 1);
		if ((short)d->integrator > d->hi_thresh)
			d->counter = 0;
		return d->counter > d->count_limit;
	}

	if (d->armed != 0) {
		if ((short)d->integrator > d->hi_thresh)
			d->counter = (short)(d->counter + 1);
		else
			d->counter = 0;
		return d->counter > d->count_limit;
	}

	/*
	 * Not armed yet: wait for the level to stay up for 0x33 blocks, then
	 * switch to the second rule and take the detector out of the
	 * receiver's flag word.
	 */
	if ((short)d->integrator <= 0x30) {
		d->warmup = 0;
		return 0;
	}
	d->warmup = (short)(d->warmup + 1);
	if (d->warmup == 0x33) {
		d->armed = 1;
		v->rx.flags &= (unsigned short)~V8_RX_DETECTOR_ARMED;
		d->counter = 0;
	}
	return 0;
}

/*
 * Arm the ANSam phase-reversal detector.  The window is cleared and the
 * countdown at +0x0e set to 32 -- half the window, which is how long it
 * waits before its first verdict.
 */
void
v8_phase_rev_init(struct v8_phase_rev *pr)
{
	int i;

	pr->detected = 0;
	pr->corr = 0;
	pr->energy = 0;
	pr->smoothed = 0;
	pr->run = 0;
	pr->reversals = 0;
	pr->half = 0x20;
	pr->widx = 0;
	for (i = 0; i < 64; i++)
		pr->window[i] = 0;
}

/*
 * Look for ANSam's phase reversals.
 *
 * Each new sample is correlated against the one half a window back.  While
 * the carrier's phase is steady that product stays positive; when it inverts
 * the product goes sharply negative, and the run of samples since the last
 * such event is what gets measured.  The comparison is against a smoothed
 * energy rather than a fixed threshold, so it holds at whatever level the
 * AGC settles on.
 */
void
v8_phase_rev_detect(struct v8_phase_rev *pr, const short *in, short count)
{
	int corr = pr->corr;
	int energy = pr->energy;
	int smoothed = pr->smoothed;
	short widx = pr->widx;
	int half = pr->half;
	int full = half * 2;
	int spacing = 0;
	short i;

	for (i = 0; i < count; i++) {
		int back;
		int x, old, diff;
		int level;

		/* Advance the write cursor, wrapping at the window's end. */
		widx = (short)(widx + 1);
		if (widx >= full)
			widx = 0;

		back = widx - half;
		if ((short)back < 0)
			back = (short)(back + full);

		x = in[i];
		old = pr->window[widx];
		diff = x - old;

		/* The window's energy, one sample in and one sample out. */
		energy += (x * x + 0x8000) >> 16;
		energy -= (old * old + 0x8000) >> 16;

		corr += (pr->window[back] * diff + 0x8000) >> 16;

		level = smoothed * 0x7fe2 + (energy * 15) * 2;
		smoothed = level >> 15;

		if (corr * 2 < (level >> 17)) {
			/* A reversal, if the run since the last one is long
			 * enough to be one rather than noise. */
			if ((short)pr->run > (short)(pr->half * 4)) {
				spacing = ((short)pr->run * 0xd55) >> 15;
				pr->run = 0;
				pr->reversals = (short)(pr->reversals + 1);
			} else {
				pr->run = (short)(pr->run + 1);
			}
		} else {
			pr->run = (short)(pr->run + 1);
		}

		if ((unsigned)(spacing - V8_PHASE_REV_MIN) <= V8_PHASE_REV_SPAN
		    && (short)pr->reversals > 1) {
			/*
			 * The author's word for the spacing is "delay", and
			 * this is the only place it is named.
			 */
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "ANSAM phase reversals detected " "delay = %d\n", spacing);
			pr->detected = 1;
		}

		pr->window[widx] = (short)x;
	}

	pr->widx = widx;
	pr->corr = corr;
	pr->energy = energy;
	pr->smoothed = smoothed;
}
