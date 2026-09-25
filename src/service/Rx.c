/*
 * Rx.c -- the voice receive handler and the setter that installs it.
 *
 * Recovered TU boundary (issue #6/#20/#67).  The blob's FILE records place
 * `Rx.c` (record 268) between `Notch.c` and `TONE.c`, and `ld -r` concatenates
 * `.text` in FILE order: `Notch.c`'s last function is `notch` at 0x0af150 and
 * `TONE.c`'s first is `TONE_create` at 0x0af690, so [0x0af150, 0x0af690) is
 * `Rx.c`'s.  The only two globals there are `voice_set_rx` (0x0af190, 305
 * bytes) and `voice_rx` (0x0af2d0, 949).  Neither is a LOCAL anchor, so the
 * bracket, the `_rx` naming and the empty competing set are the proof.
 *
 * The bodies and the constants they read moved VERBATIM out of
 * src/service/voicedp.c; that file was a layer over three object TUs.  The
 * compile-time constants are repeated here because they were file-local to
 * the layer and the object's own TU carries its own copies.  No body
 * rewritten and no flag changed.  Finding F11386.
 */

#include "dsplib/voice.h"


#include "dsplib/beepgen.h"
#include "dsplib/debug.h"
#include "dsplib/detector.h"
#include "dsplib/fdspkrnl.h"
#include "dsplib/fifo8.h"
#include "dsplib/pcm.h"
#include "dsplib/silence.h"
#include "dsplib/voice.h"
#include "dsplib/voicecmd.h"

/*
 * The state's layout is a byte count from a build where pointers are four
 * bytes, so these are compiled only under that ABI.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define VOICE_ASSERT_OFF(field, off) \
	typedef char voice_ctx_off_##field[ \
		((int)__builtin_offsetof(struct voice_ctx, field) == (off)) \
			? 1 : -1]

VOICE_ASSERT_OFF(mode, 0x010);
VOICE_ASSERT_OFF(int_0014, 0x014);
VOICE_ASSERT_OFF(beepgen, 0x018);
VOICE_ASSERT_OFF(detector, 0x01c);
VOICE_ASSERT_OFF(handler, 0x020);
VOICE_ASSERT_OFF(fifo, 0x024);
VOICE_ASSERT_OFF(dp, 0x034);
VOICE_ASSERT_OFF(stage, 0x038);
VOICE_ASSERT_OFF(flt, 0x100);
VOICE_ASSERT_OFF(beep_done, 0x740);
VOICE_ASSERT_OFF(dle_etx, 0x744);
VOICE_ASSERT_OFF(dle_can, 0x748);
VOICE_ASSERT_OFF(out_format, 0x74c);
VOICE_ASSERT_OFF(rx_armed, 0x756);
VOICE_ASSERT_OFF(rate_bits, 0x758);
VOICE_ASSERT_OFF(underrun, 0x75c);
VOICE_ASSERT_OFF(detector_enable_tx, 0x75e);
VOICE_ASSERT_OFF(detector_enable_rx, 0x760);
VOICE_ASSERT_OFF(detector_enable, 0x762);
VOICE_ASSERT_OFF(marker_period, 0x764);
VOICE_ASSERT_OFF(marker_countdown, 0x766);
VOICE_ASSERT_OFF(dc_init, 0x7c8);
VOICE_ASSERT_OFF(dc, 0x7cc);
VOICE_ASSERT_OFF(gain_fmt1, 0x7d0);
VOICE_ASSERT_OFF(gain_other, 0x7d4);
VOICE_ASSERT_OFF(gain_fmt3, 0x7d8);

typedef char voice_ctx_size[(sizeof(struct voice_ctx) == 0x7dc) ? 1 : -1];

#endif

/*
 * 1 and 2 mean 16-bit linear out; anything else means float.  The object
 * spells the test `(unsigned)(x - 1) <= 1` at all five sites that make it,
 * which is one subtraction and one unsigned compare, so it is kept as one.
 */
#define VOICE_OUT_IS_LINEAR(v)	((unsigned int)((v)->out_format - 1) <= 1u)

/*
 * The beep's full scale.  0x46ea6000 in `.rodata.cst4`, and NOT the 32000.0f
 * `FDSP_DP_Run` uses two functions away.
 */
#define VOICE_BEEP_FULL_SCALE	30000.0f

/*
 * `voice_tx`'s two output scales, both from `.rodata.cst4`: 260.0f when the
 * host stream is 8 bits per sample and 30000.0f otherwise.  Why 260 -- the
 * staging bytes are already mean-removed at that point, so the two are not
 * the same units -- is not established.
 */
#define VOICE_TX_SCALE_8BIT	260.0f
#define VOICE_TX_SCALE_OTHER	30000.0f

/*
 * The staging bytes are scaled by 1/64 on the way into the mean, and the
 * float output either divides by 64 or multiplies the mean back up by it.
 */
#define VOICE_TX_BYTE_SCALE	(1.0f / 64.0f)

/*
 * How much room `voice_tx` reports back to the host: 170 bytes less whatever
 * the FIFO holds, floored at zero.  170 is the object's literal and has no
 * relation to the ring's own length that this tree can show.
 */
#define VOICE_TX_HOST_ROOM	170

/*
 * The fixed number of floats `voice_tx` zeroes when there is not enough data
 * and the output is float.  The linear arm of the same branch zeroes
 * `*countp` samples instead; the asymmetry is the object's.
 */
#define VOICE_TX_FLT_FILL	160

/*
 * How many bytes `voice_tx` asks the FIFO for, by rate.  160 at 8000 Hz is
 * 20 ms; 144 at 7200 is the same; 222 at 11025 is 20.1 ms and is the odd one
 * out (20 ms would be 220.5).  The object computes the last two branchlessly
 * from a single `setne`, which is `-O3` if-converting a conditional.
 */
#define VOICE_TX_READ_8000	160
#define VOICE_TX_READ_7200	144
#define VOICE_TX_READ_OTHER	222

/*
 * The six (bits, rate) pairs `voice_tx` knows, and the FIFO fill each one
 * demands before a block can be produced.  Each threshold is one less than
 * the byte count of 20 ms at that format, so the test `fill > threshold` is
 * "a whole block is present".
 */
#define VOICE_FMT_8BIT_8000	VOICE_RATE_BITS(8, 8000)
#define VOICE_FMT_8BIT_7200	VOICE_RATE_BITS(8, 7200)
#define VOICE_FMT_8BIT_11025	VOICE_RATE_BITS(8, 11025)
#define VOICE_FMT_4BIT_8000	VOICE_RATE_BITS(4, 8000)
#define VOICE_FMT_4BIT_7200	VOICE_RATE_BITS(4, 7200)
#define VOICE_FMT_4BIT_11025	VOICE_RATE_BITS(4, 11025)

/*
 * What `voice_tx` returns when the FIFO is short of a block and the stream
 * has not been closed with <DLE><ETX>.  The object has no name for it.
 */
#define VOICE_TX_UNDERRUN_STATUS	13

/* The nonzero code each handler returns when `int_0014` differs from `mode`. */
#define VOICE_ONLINE_MODE_STATUS	2
#define VOICE_TX_MODE_STATUS		3
#define VOICE_RX_MODE_STATUS		4
#define VOICE_DUPLEX_MODE_STATUS	5

/*
 * `voice_rx`'s two other answers: 8 when the block ends the stream with
 * <DLE><ETX>, and 7 when the caller asked for more samples than the working
 * buffer's fixed 200.  The object has names for neither.
 */
#define VOICE_RX_ETX_STATUS		8
#define VOICE_RX_TOO_MANY_STATUS	7

/*
 * The most samples `voice_rx` will convert through the context's own float
 * buffer.  It is a hard immediate (`cmp $0xc8,%dx`), and only the two 16-bit
 * arms test it -- the float arm converts in the caller's buffer and is not
 * bounded at all.
 */
#define VOICE_RX_MAX_SAMPLES		200

/* Samples per unit of `marker_period` -- 100 ms at 8 kHz. */
#define VOICE_RX_MARKER_UNIT		800

/*
 * Receive scaling, both halves of it.  The host's answer is divided by 128
 * once, in `voice_set_rx`; the two 16-bit arms then divide by 32767 as well,
 * on every block.  `0x38000100` is the float nearest 1/32767, which is what
 * fixes the second: the object MULTIPLIES, so the reciprocal is a constant
 * expression in the source rather than a division the compiler folded --
 * GCC will not turn `/ 32767.0f` into a multiply without fast math.
 */
#define VOICE_RX_PARAM_SCALE		(1.0f / 128.0f)
#define VOICE_RX_LINEAR_SCALE		(1.0f / 32767.0f)

/* The full scale `voice_rx` converts back through on the way to u-law. */
#define VOICE_RX_FULL_SCALE		32767.0f

/*
 * The DC estimate's smoothing, and the argument `voice_rx` hands
 * `silence_is_more_then` -- 0.8 seconds (silence.h settles the unit).  The
 * first two are `double` literals in the object's arithmetic; the third is a
 * float in `.rodata.cst4`.
 *
 * WHICH WEIGHT GOES ON WHICH TERM IS `.rodata.cst8`'s TO SAY, and it is easy
 * to get backwards -- this reconstruction did, and the test caught it at the
 * second block of every sequence.  0x1b0 is 0.99 and 0x1b8 is 0.01, the
 * object loads 0x1b8 FIRST (so it ends up on the block mean) and 0x1b0 second
 * (so it ends up on the stored estimate).  99% old, 1% new.  Finding F8790.
 */
#define VOICE_RX_DC_KEEP		0.99
#define VOICE_RX_DC_FOLD		0.01
#define VOICE_RX_SILENCE_SECONDS	0.8f

/*
 * Go into receive: mode 0, the receive handler, the receive detector mask, a
 * re-created silence detector, a fresh marker countdown and three gains read
 * from the host.
 *
 * `silence_create`'s RESULT IS DISCARDED.  The object calls it with the
 * pointer `voice_create` already stored and never stores what comes back --
 * harmless while that pointer is non-NULL, since the callee initialises in
 * place and returns its argument, and a leak if it ever is.  Deviation D988.
 *
 * `cfg.get_sreg` IS THE HOST'S SETTINGS CALLBACK, and this function is what
 * shows it.  `beepgen.h` types that slot `void (*)(void *modem)` from the one
 * place `beepgen_start_beep` calls it; here it is called with TWO arguments
 * and its unsigned answer converted to float, and it is also what
 * `silence_create` is handed as its `query`.  Finding F8788; the cast below
 * is that finding and not a convenience.
 */
void
voice_set_rx(struct voice_ctx *v)
{
	unsigned int (*query)(void *obj, int what) =
	    (unsigned int (*)(void *, int))v->cfg.get_sreg;

	v->rx_armed = 1;
	v->mode = 0;
	v->handler = voice_rx;
	detector_set_enable(v->detector, (short)v->detector_enable_rx);
	silence_create(v->silence, v->cfg.modem, query);
	v->marker_countdown = (unsigned short)(v->marker_period
					       * VOICE_RX_MARKER_UNIT);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Sample rate 8000\n");

	v->rate_bits.s.rate = 8000;
	v->rate_bits.s.bits = 8;
	v->dc = 0.0f;
	v->dc_init = 1;
	/*
	 * `query()`'s `unsigned int` is what the multiply by
	 * VOICE_RX_PARAM_SCALE already implicitly converts to `float` (the
	 * usual arithmetic conversions, `query() * float` promotes the
	 * integer operand) -- the cast below only makes that existing
	 * conversion explicit, at the point it already happens.  The blob
	 * loads it with `fildll` and rounds once, in the `fstps` that stores
	 * the product; making the cast explicit here does not move that
	 * store or add a second rounding.
	 */
	v->gain_fmt1 = (float)query(v->cfg.modem, VOICE_PARAM_RX_GAIN_FMT1)
		       * VOICE_RX_PARAM_SCALE;
	v->gain_fmt3 = (float)query(v->cfg.modem, VOICE_PARAM_RX_GAIN_FMT3)
		       * VOICE_RX_PARAM_SCALE;
	v->gain_other = (float)query(v->cfg.modem, VOICE_PARAM_RX_GAIN_OTHER)
			* VOICE_RX_PARAM_SCALE;
}

/*
 * The receive block handler: line samples in, an escaped u-law byte stream
 * out.
 *
 * Four stages.  (1) The mean of the incoming float block folds into a running
 * DC estimate at one part in a hundred, and that estimate is subtracted from
 * every sample.  (2) The block is scaled by the gain its output format
 * selects -- formats 1 and 3 convert from `rx_lin` into the context's own
 * float buffer, everything else scales the caller's `tx_flt` in place.
 * (3) Each sample becomes a u-law byte in `tx_lin`, with a literal DLE
 * doubled and a periodic <DLE>'T' marker inserted.  (4) `silence_progress`
 * appends its own escapes after them, and a pending <DLE><CAN> closes the
 * stream with <DLE><ETX>.
 *
 * `rx_flt` is never read.  `tx_lin` is a BYTE stream here, whatever the
 * handler signature calls it, and `tx_flt` is both the input block and, in
 * the float arm, the working buffer.
 *
 * NOTE THE ASYMMETRY OF THE 200-SAMPLE BOUND: formats 1 and 3 refuse a
 * longer block outright (answer 7), and the float arm neither tests nor
 * needs it, because it works in the caller's buffer.
 */
int
voice_rx(struct voice_ctx *v, short *rx_lin, float *rx_flt, float *tx_flt,
	 short *tx_lin, unsigned short *hostcount, unsigned short *countp)
{
	unsigned char *out = (unsigned char *)tx_lin;
	float *flt = v->flt;
	float sum = 0.0f;
	short n = (short)*countp;
	unsigned short have = (unsigned short)n;
	unsigned short i;
	unsigned short j = 0;
	int k;
	int ret = 0;

	(void)rx_flt;

	if (have != 0) {
		float dc;

		for (k = 0; k < have; k++)
			sum += tx_flt[k];
		dc = sum / (float)have;
		if (v->dc_init)
			v->dc_init = 0;
		else
			dc = (float)(dc * VOICE_RX_DC_FOLD + v->dc * VOICE_RX_DC_KEEP);
		v->dc = dc;
		for (k = 0; k < have; k++)
			tx_flt[k] -= v->dc;
	}

	/*
	 * D989: the answer is thrown away.  `silence_is_more_then` reads the
	 * detector and returns a verdict; the object calls it here -- on both
	 * the empty-block and the non-empty path -- and uses neither result.
	 */
	(void)silence_is_more_then(v->silence, VOICE_RX_SILENCE_SECONDS);

	if (v->rx_armed != 1) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("RX WAIT ABORT\n");
		*countp = 0;
	} else {
		float gain;

		if (v->out_format == 3 || v->out_format == 1) {
			gain = VOICE_RX_LINEAR_SCALE
			       * (v->out_format == 1 ? v->gain_fmt1
						     : v->gain_fmt3);
			if (*countp > VOICE_RX_MAX_SAMPLES) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					  "rx buffer greater than internal\n");
				return VOICE_RX_TOO_MANY_STATUS;
			}
			for (i = 0; i < *countp; i++)
				v->flt[i] = (float)rx_lin[i] * gain;
		} else {
			gain = v->gain_other;
			flt = tx_flt;
			for (i = 0; i < *countp; i++)
				tx_flt[i] *= gain;
		}

		for (i = 0; i < have; i++) {
			unsigned char b;

			b = linear2ulaw((short)(flt[i] * VOICE_RX_FULL_SCALE)
					>> 2);
			out[j] = b;
			if (b == VOICE_DLE) {
				j++;
				out[j] = VOICE_DLE;
			}
			j++;
			if (v->marker_period != 0) {
				v->marker_countdown--;
				if (v->marker_countdown == 0) {
					out[j] = VOICE_DLE;
					out[j + 1] = VOICE_DLE_MARK;
					j = (unsigned short)(j + 2);
					v->marker_countdown =
					    (unsigned short)
					    (v->marker_period
					     * VOICE_RX_MARKER_UNIT);
				}
			}
		}

		*countp = j;
		silence_progress(v->silence, flt, n, out + j, countp);

		if (v->dle_can) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "****** Send DLE ETX\n");
			_status(out + *countp, countp, 3);
			v->rx_armed = 0;
			detector_set_enable(v->detector, 0);
			ret = VOICE_RX_ETX_STATUS;
		}
	}

	*hostcount = 0;
	if (v->int_0014 != v->mode)
		ret = VOICE_RX_MODE_STATUS;
	return ret;
}
