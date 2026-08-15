/*
 * dtmf_rx.c -- reconstructed from dsplibs.o Dtmf_Rx.c.  All four of it.
 *
 *   reset_dtmf       .text 0x090a90   275 bytes
 *   create_cid_dtmf  .text 0x090bb0   370 bytes
 *   band_pass        .text 0x090d30  1187 bytes
 *   dtmf_modem       .text 0x0911e0  1729 bytes
 *
 * The first two are not in `dtmf_modem`'s closure -- `cid_reset` and
 * `cid_create` reach them, nothing here does -- and they are what pins the
 * object's size at 0x38c and what include/dsplib/dtmf_rx.h's field comments
 * are checked against.  Findings 1700, 1701 and 1702.
 *
 * See finding 1410 for why these are Dtmf_Rx.c and not, as
 * docs/attribution.md guessed, `Data.c` or `Dtmf.c`.
 */

#include "dsplib/dtmf_rx.h"
#include "dsplib/debug.h"
#include "dsplib/fpm_iir.h"
#include "dsplib/sysdep.h"

/*
 * `create_cid_dtmf` allocates 0x38c bytes for this object (blob 0x090d0f).
 * `make offsets` checks every field's offset and nothing checks the total,
 * so the size is asserted here: `diff_eq_obj` compares `sizeof(type)` bytes,
 * and a size read wrongly off one disassembly would silently narrow every
 * comparison in t_dtmfrx.
 */
#if defined(__i386__)
/*
 * 32-bit only, and the reason is `bufp`: the object has a POINTER in it, so
 * its size is ABI-dependent and 0x38c is a claim about the target the blob
 * was built for.  `make check64` compiles this file as 64-bit to catch
 * pointer-size assumptions, and an unguarded assertion here fires there --
 * which is the check doing its job, not a mistake to suppress.
 */
typedef char dtmf_rx_size_check[sizeof(struct dtmf_rx) == 0x38c ? 1 : -1];
#endif

/*
 * Put the receiver back to the state a new one is in.
 *
 * Everything the state machine accumulates -- the digit string, the tone
 * bank's eight resonator states, band_pass's biquad and its energy memory,
 * the sample counter -- and nothing the CALLER chose: `rate` and `sens`
 * survive, which is what makes this usable as `cid_reset`'s reset rather than
 * only as part of construction.
 *
 * Four things it leaves alone are worth naming, because each is a fact about
 * the original rather than an omission here:
 *
 *   digits[16..19]  the clearing loop stops at 15 and the array is 20 (D307)
 *   pre_low         the low group's pre-notch, where pre_high is cleared
 *                   (D251)
 *   aligned         harmless: `state` comes out as 1, and state 1 writes
 *                   `aligned` on both of the paths that can reach state 2
 *   samples, hold   400 words of window that the machine refills before it
 *                   reads them
 */
void
reset_dtmf(struct dtmf_rx *rx)
{
	short i;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DTMF CID Reset !\n");

	rx->f000 = 0;
	rx->f004 = 0;
	rx->f008 = 0;
	rx->nsamples = 0;

	rx->last_digit = -1;
	rx->stable = 0;
	rx->ndigits = -1;		/* -1, not 0: "no string started" */
	rx->quiet = 0;
	rx->level = 1;			/* the energy memory's floor      */
	rx->bufp = rx->samples;

	/* The four biquad states, in pairs, in the object's own order. */
	rx->f354[0] = 0;
	rx->f354[1] = 0;
	rx->bp_state[0] = 0;
	rx->bp_state[1] = 0;
	rx->f35c[0] = 0;
	rx->f35c[1] = 0;
	rx->pre_high[0] = 0;
	rx->pre_high[1] = 0;

	/* One state pair per tone: all eight of the bank's resonators. */
	for (i = 0; i <= 7; i++) {
		rx->tone_state[i][0] = 0;
		rx->tone_state[i][1] = 0;
	}

	/* Sixteen, not twenty.  D307. */
	for (i = 0; i <= 15; i++)
		rx->digits[i] = 0;

	rx->state = 1;			/* armed, hunting for an edge */
}

/*
 * Construct one.  A NULL argument allocates; anything else is the caller's
 * storage and is used in place.  The object is returned either way, so
 * `cid_create` can write the result back over the pointer it passed.
 *
 * `sysdep_malloc`'s result is used without being checked -- D303, and the
 * house style of this object: LowPassFIR and GenericToneDetector do the same.
 *
 * The two configuration fields are set BEFORE the reset, which is visible
 * rather than incidental: the trace below reads `rate` back out of the
 * object, and `reset_dtmf` does not touch either field.
 */
struct dtmf_rx *
create_cid_dtmf(struct dtmf_rx *rx)
{
	if (rx == NULL)
		rx = (struct dtmf_rx *)sysdep_malloc(sizeof(*rx));

	rx->rate = DTMF_RX_RATE_8000;
	rx->sens = 0;

	/*
	 * The object loads `rate` back with `movzwl`, so it is read unsigned
	 * here as it is in dtmf_modem.  Unobservable -- the value it has just
	 * stored is 8000 -- but it is the encoding the field's type forces.
	 *
	 * The second `%d` is a constant 0 in the object, materialised rather
	 * than loaded, so which of `rx->sens`, a local or a literal the author
	 * wrote is not recoverable.  Written the plain way.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DTMF Cid Creating   Fs = %d   "
				     "Threshold = %d !\n",
				     (unsigned short)rx->rate, 0);

	/*
	 * Inlined at this site in the object, which is why the second debug
	 * gate below it re-loads `dsplibs_debug_level`: the printf above could
	 * have changed it.  A call reproduces that exactly, and the modern
	 * build cannot spell the gnu89 `inline` that would reproduce the
	 * codegen without losing the out-of-line definition the test needs.
	 */
	reset_dtmf(rx);
	return rx;
}

/*
 * band_pass's four coefficient sets, all FPM_iir_filt sections in the order
 * { a2, b2, a1, b1, b0 }.
 *
 * TWO OF THEM ARE NEVER READ.  The high-pass pair is initialised on every
 * call and nothing uses it; only the band-pass pair reaches the filter.  It
 * is reproduced because it is what the object does -- a modern compiler will
 * delete the stores again, which changes nothing observable -- and D253
 * records it.
 *
 * The band-pass is one biquad with zeros at DC and Nyquist (b1 = 0, b2 =
 * -b0), poles at radius 0.64 (8000) / 0.70 (9600) and a centre near 870 Hz
 * at both rates.  Very broad: it is a shaping filter for an energy decision,
 * not a channel filter.  The high-pass sets are a double zero at DC with
 * poles at 244 Hz, at both rates.
 */
#define BP_HP_8000	{ -12971, 12917, 28620, -25834, 12917 }
#define BP_HP_9600	{ -13484, 13194, 29347, -26388, 13194 }
#define BP_BP_8000	{ -6786, -4799, 16287, 0, 4799 }
#define BP_BP_9600	{ -8079, -4152, 19423, 0, 4152 }

/*
 * Filter a block in place and say whether it carries signal.
 *
 * Four stages, in this order and not another:
 *
 *   1. LIMIT.  More than count/128 samples above 20000 divides the block by
 *      16; failing that, more than count/128 above 10000 divides it by 8.
 *   2. DC BLOCK.  Subtract the block's own mean.
 *   3. COARSE GAIN.  Count the samples above 150, 300 and 600 and multiply
 *      the whole block by 6, 3, 2 or 1 accordingly -- the fewer loud
 *      samples, the more gain.
 *   4. FILTER, accumulating the energy before and after it.
 *
 * The verdict compares the filtered energy against the raw energy scaled by
 * a threshold that itself steps down as the raw energy rises, so a loud
 * block is judged on a looser ratio than a quiet one.
 */
int
band_pass(short *samples, short count, struct dtmf_rx *rx)
{
	short hp_8000[5] = BP_HP_8000;
	short hp_9600[5] = BP_HP_9600;
	short bp_8000[5] = BP_BP_8000;
	short bp_9600[5] = BP_BP_9600;
	const short *coef;
	int thresh = 14500;
	int raw_acc = 0;
	int tone_acc = 0;
	unsigned raw, tone;
	unsigned short level;
	short n_20k = 0, n_10k = 0;
	short n150 = 0, n300 = 0, n600 = 0;
	short lim;
	short i;
	int sum;

	(void)hp_8000;		/* see the comment above: initialised, unread */
	(void)hp_9600;

	coef = (rx->rate == DTMF_RX_RATE_9600) ? bp_9600 : bp_8000;

	/* 1. limit */
	for (i = 0; i < count; i++) {
		if (samples[i] > 20000)
			n_20k++;
		if (samples[i] > 10000)
			n_10k++;
	}
	lim = (short)(count >> 7);
	if (lim < n_20k) {
		for (i = 0; i < count; i++)
			samples[i] = (short)((samples[i] + 8) >> 4);
	} else if (lim < n_10k) {
		for (i = 0; i < count; i++)
			samples[i] = (short)((samples[i] + 4) >> 3);
	}

	/* 2. DC block.  The divide is inside the loop in the object too. */
	sum = 0;
	for (i = 0; i < count; i++)
		sum += samples[i];
	for (i = 0; i < count; i++)
		samples[i] = (short)((unsigned short)samples[i] - sum / count);

	/* 3. coarse gain */
	for (i = 0; i < count; i++) {
		int a = samples[i] < 0 ? -(int)samples[i] : (int)samples[i];

		if (a > 600) {
			n150++;
			n300++;
			n600++;
		} else if (a > 300) {
			n150++;
			n300++;
		} else if (a > 150) {
			n150++;
		}
	}
	if (n150 <= 2) {
		for (i = 0; i < count; i++)
			samples[i] = (short)(samples[i] * 6);
	} else if (n300 > 2) {
		if (n600 <= 2)
			for (i = 0; i < count; i++)
				samples[i] = (short)(samples[i] * 2);
	} else {
		for (i = 0; i < count; i++)
			samples[i] = (short)(samples[i] * 3);
	}

	/* 4. filter, with the energy taken on both sides of it */
	for (i = 0; i < count; i++) {
		int x = samples[i];
		int y;

		raw_acc += x * x;
		samples[i] = FPM_iir_filt((short)x, coef, rx->bp_state, 1);
		y = samples[i];
		tone_acc += (y * y + 4) >> 3;
	}

	/*
	 * Both accumulators are read as UNSIGNED from here down -- the object
	 * shifts them with `shr` and compares them with `jae`, so a block loud
	 * enough to wrap `raw_acc` is judged as an enormous energy rather than
	 * a negative one.  Preserved, not corrected.
	 */
	raw = ((unsigned)raw_acc + 0x20000) >> 18;
	tone = ((unsigned)tone_acc + 0x4000) >> 15;

	if (raw > 1375)
		thresh = 2719;
	else if (raw > 687)
		thresh = 4984;
	else if (raw > 343)
		thresh = 8156;

	/* `sens` trims the threshold down: 2 -> 7/8, 3 -> 3/4, 4 -> 1/2. */
	if (rx->sens == 2)
		thresh = (short)((thresh * 8 - thresh + 4) >> 3);
	else if (rx->sens == 3)
		thresh = (short)((thresh * 3 + 2) >> 2);
	else if (rx->sens == 4)
		thresh = (short)((thresh + 1) >> 1);

	if ((((unsigned)(thresh * (int)raw)) + 0x4000) >> 15 >= tone)
		return 0;

	/*
	 * The second gate is against the level this object last saw, so a
	 * block quieter than the running level is silence however good its
	 * tone-to-total ratio was.
	 */
	level = (unsigned short)rx->level;
	if ((unsigned)(int)(short)level >= raw)
		return 0;

	/* Only a long enough block is allowed to move the running level. */
	if (count > 100) {
		level = (unsigned short)(raw >> 3);
		rx->level = (short)level;
	}
	if ((short)level == 0)
		rx->level = 1;
	return 1;
}

/*
 * One block of line samples through the receiver's state machine.
 *
 * State 0 idle       : both halves must go quiet before hunting starts.
 * State 1 hunting    : a quiet half followed by a loud one is a tone edge;
 *                      the window is realigned to it and `hold` starts
 *                      carrying half a block over.
 * State 2 aligned    : whole blocks go to the tone bank.  A silent block
 *                      commits whatever digit was pending and goes back to
 *                      hunting.
 * State 3            : written by the timeout below; nothing dispatches on
 *                      it, so it falls through to "return result".
 */
int
dtmf_modem(const short *samples, unsigned short count, struct dtmf_rx *rx)
{
	short buf[198];
	short flag = 1;
	int result = 1;
	short ndigits_in;
	short state;
	short half;
	short first, second;
	short digit;
	short n;
	short stable;
	int rate;
	short i;
	int j;

	for (i = 0; i < (int)count; i++)
		buf[i] = samples[i];

	rx->nsamples += count;
	ndigits_in = rx->ndigits;
	rate = (unsigned short)rx->rate;

	if (ndigits_in > 2) {
		result = 2;
		if (rx->nsamples > 3 * rate)
			result = -1;
		/* 300 ms of samples without a decision arms the timeout. */
		if (rx->nsamples > rate * 300 / 1000)
			rx->state = 3;
	}

	state = (short)(unsigned short)rx->state;

	if (state == 1) {
		if (rx->nsamples > 3 * rate && ndigits_in > 0)
			result = -1;

		half = (short)((unsigned short)count >> 1);
		first = (short)band_pass(buf, half, rx);
		second = (short)band_pass(buf + (unsigned short)half, half, rx);

		if (first == 0 && second == 1) {
			/*
			 * The edge.  The second half is the start of a tone,
			 * so keep it and pair it with the first half of the
			 * next block -- that is the realignment.
			 */
			flag = 0;
			rx->state = 2;
			for (j = 0; j < (unsigned short)half; j++)
				rx->hold[j] = buf[(unsigned short)half + j];
			rx->aligned = 1;
		}
		if (first == 1 && second == 1) {
			/* Already inside a tone: no realignment needed. */
			rx->aligned = 0;
			flag = 1;
			rx->state = 2;
			state = 2;
		} else {
			state = (short)(unsigned short)rx->state;
		}
	}

	if (state == 2 && flag != 0) {
		if (band_pass(buf, (short)count, rx) == 0) {
			/* ---- silence: commit whatever was pending ---- */
			if (rx->stable <= 0) {
				rx->level = 1;
				rx->quiet++;
			} else if ((unsigned short)rx->last_digit == 12) {
				n = rx->ndigits;
				rx->digits[n] = 0;
				rx->ndigits = (short)(n + 1);
				result = 3;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "CID DTMF Detected 'C' !\n");
			} else if ((unsigned short)rx->last_digit <= 9) {
				n = rx->ndigits;
				rx->digits[n] =
				    (char)((unsigned short)rx->last_digit + 0x30);
				rx->ndigits = (short)(n + 1);
			}

			if (rx->quiet > 5 && rx->ndigits > 2)
				result = -1;
			rx->state = 1;
			rx->stable = 0;
			rx->last_digit = -1;
			return result;
		}

		/* ---- signal: hand a whole window to the tone bank ---- */
		if (rx->aligned != 0) {
			half = (short)((unsigned short)count >> 1);
			for (j = 0; j < (unsigned short)half; j++) {
				rx->samples[j] = rx->hold[j];
				rx->samples[(unsigned short)half + j] = buf[j];
				rx->hold[j] = buf[(unsigned short)half + j];
			}
			rx->bufp = rx->samples;
		} else {
			rx->bufp = buf;
		}

		digit = (short)DTMF_MTD_detect(rx->bufp, (short)count, rx);
		stable = (short)((unsigned short)rx->stable + 1);
		rx->nsamples = 0;

		if (rx->sens == 1) {
			/*
			 * A diagnostic mode, and it reads like one: with no
			 * digit string started yet the reported digit is
			 * REPLACED by the stable count plus 20, which is not a
			 * keypad code at all.  D254; reproduced as found.
			 */
			int fresh = (rx->ndigits == -1);

			if (fresh && digit != 13)
				digit = (short)(stable + 20);
			if (fresh && digit == 13) {
				rx->quiet = 0;
				rx->state = 0;
				rx->last_digit = -1;
				rx->ndigits = 0;
				stable = 0;
			}
		} else if (rx->ndigits == -1) {
			rx->ndigits = 0;
		}

		if (rx->last_digit != digit) {
			rx->last_digit = digit;
			rx->stable = (short)(rx->ndigits > 0);
			return result;
		}

		/* The same code twice running is what commits it. */
		if (digit != -9 && digit <= 13 && digit != 10 && digit != 11) {
			rx->quiet = 0;
			rx->stable = 0;
			rx->last_digit = -1;
			rx->state = 0;

			if (digit == 12) {
				n = rx->ndigits;
				rx->digits[n] = 0;
				rx->ndigits = (short)(n + 1);
				result = 3;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "CID DTMF Detected 'C' !\n");
			} else if (digit <= 9) {
				n = rx->ndigits;
				rx->digits[n] = (char)(digit + 0x30);
				rx->ndigits = (short)(n + 1);
			}
			if (rx->ndigits == 20)
				result = -1;
			return result;
		}

		rx->stable = stable;
		rx->level = 1;
		rx->quiet = (short)((unsigned short)rx->quiet + 1);
		if (rx->quiet > 5 && rx->ndigits > 2)
			result = -1;
		return result;
	}

	if (state == 0) {
		half = (short)((unsigned short)count >> 1);
		first = (short)band_pass(buf, half, rx);
		second = (short)band_pass(buf + (unsigned short)half, half, rx);
		/* Both halves quiet -- or either of them -- starts the hunt. */
		if (first == 0 || second == 0)
			rx->state = 1;
	}

	return result;
}
