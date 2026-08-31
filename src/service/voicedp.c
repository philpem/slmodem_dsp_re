/*
 * voicedp.c -- the voice service's per-block handlers and the four setters
 * that install them.
 *
 * In the object's emission order, with the span `tumap.py` labels each:
 *
 *     0xabef0  voice_set_online    47    class1tx.c +94
 *     0xabf20  voice_set_duplex    45    class1tx.c +94
 *     0xabf50  voice_online       508    class1tx.c +94
 *     0xaf190  voice_set_rx       305    Fdspkrnl.c +13
 *     0xaf2d0  voice_rx           949    Fdspkrnl.c +13
 *     0xafcf0  voice_set_tx       110    Fdspkrnl.c +13
 *     0xafd60  voice_tx          1150    Fdspkrnl.c +13
 *     0xb01e0  voice_duplex       251    Fdspkrnl.c +13
 *
 * A SPAN NAME IS NOT A MODULE NAME (CLAUDE.md).  Three of these sit in a
 * bracket labelled `class1tx.c` and they are not fax: the label comes from
 * the blob's LAYOUT, the bracket is shared with `voice.c#260`, every symbol
 * is `voice_*`, and `voice_create` -- in the same bracket -- installs
 * `voice_online` into the context this file's handlers all take.  It is the
 * same correction `voicecmd.h` records for `voice_dle_command`, two functions
 * earlier in the same bracket.
 *
 * THE FOUR HANDLERS SHARE A SIGNATURE AND NOT A JOB.  All five arguments
 * after the context are the datapump's block buffers, in `FDSP_DP_Run`'s
 * order (voice.h's `voice_handler_fn`), and each handler uses a different
 * subset:
 *
 *   voice_online   plays the beep queue into `rx_flt` or `tx_lin`, zero-fills
 *                  the rest of the block, and reads NEITHER `rx_lin` NOR
 *                  `tx_flt`.  Returns 1 on the block that retires the beep,
 *                  2 on a mode mismatch.
 *   voice_tx       un-escapes DLE from `rx_lin`'s BYTE stream into the FIFO,
 *                  then converts a mean-removed block out of the FIFO into
 *                  `rx_flt`/`tx_lin`.  Returns 13 when the FIFO is short,
 *                  3 on a mode mismatch.
 *   voice_rx       removes the block's DC, scales it, converts it to escaped
 *                  u-law in `tx_lin` and lets the silence detector append to
 *                  that.  Returns 8 when the stream closes, 7 on an over-long
 *                  block, 4 on a mode mismatch.
 *   voice_duplex   runs `FDSP_DP_Run` over the whole block and then overlays
 *                  the beep on the receive side.  Returns 1, 5 as above.
 *
 * `countp` and `hostcount` are two different counts, both in/out.  `countp`
 * is the block's SAMPLE count and every handler leaves it 0.  `hostcount` is
 * the HOST BYTE count: `voice_tx` reads it as how many escaped bytes are in
 * `rx_lin` and writes back how much room the FIFO now has; `voice_online`
 * writes it and never reads it.
 *
 * All eight are GLOBAL in the blob, so all eight are plain cdecl -- there is
 * no regparm question here.  Finding F8770's hazard is about LOCAL symbols and
 * the prologues confirm it: each of these reads its arguments off the stack.
 */

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
 * Go online: mode 2, the beep-only handler, and the detector enabled with
 * whatever mask the context carries.
 */
void
voice_set_online(struct voice_ctx *v)
{
	v->mode = 2;
	v->handler = voice_online;
	detector_set_enable(v->detector, (short)v->detector_enable);
}

/*
 * Go duplex: mode 3, the full-duplex handler, and the detector enabled with a
 * CONSTANT 0x24 rather than the context's mask.  That difference is the
 * object's and is not explained by anything reconstructed here.
 */
void
voice_set_duplex(struct voice_ctx *v)
{
	v->mode = 3;
	v->handler = voice_duplex;
	detector_set_enable(v->detector, 0x24);
}

/*
 * The beep-only block handler.  Nothing arrives from the line here: the block
 * is filled entirely from the beep generator, and once the queue runs dry the
 * remainder of the block -- and every later block -- is silence.
 *
 * `rx_lin` and `tx_flt` are never read.  `hostcount` is written twice, and
 * the first of those stores is dead on every path: deviation D997.
 */
int
voice_online(struct voice_ctx *v, short *rx_lin, float *rx_flt, float *tx_flt,
	     short *tx_lin, unsigned short *hostcount, unsigned short *countp)
{
	unsigned short i = 0;
	int r = 0;
	int ret = 0;

	(void)rx_lin;
	(void)tx_flt;

	if (v->beep_done) {
		if (VOICE_OUT_IS_LINEAR(v)) {
			for (i = 0; i < *countp; i++)
				tx_lin[i] = 0;
		} else {
			for (i = 0; i < *countp; i++)
				rx_flt[i] = 0.0f;
		}
	} else if (VOICE_OUT_IS_LINEAR(v)) {
		while (i < *countp && r != 1) {
			float s;

			r = beepgen_sample(v->beepgen, &s);
			tx_lin[i] = (short)(s * VOICE_BEEP_FULL_SCALE);
			i++;
		}
		while (i < *countp)
			tx_lin[i++] = 0;
	} else {
		while (i < *countp && r != 1) {
			r = beepgen_sample(v->beepgen, &rx_flt[i]);
			i++;
		}
		while (i < *countp)
			rx_flt[i++] = 0.0f;
	}

	if (r == 1) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("beepgend end, send ok\n");
		v->beep_done = 1;
		FDSP_Kernel_SetInternalBeepInProgress(0);
		ret = 1;
	}

	/* D997: overwritten on every path by the store two lines down. */
	*hostcount = *countp;
	*countp = 0;
	*hostcount = 0;

	if (v->int_0014 != v->mode && v->int_0014 != 0)
		ret = VOICE_ONLINE_MODE_STATUS;
	return ret;
}

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
 * `cfg.fn_04` IS THE HOST'S SETTINGS CALLBACK, and this function is what
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
	    (unsigned int (*)(void *, int))v->cfg.fn_04;

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
	v->gain_fmt1 = query(v->cfg.modem, VOICE_PARAM_RX_GAIN_FMT1)
		       * VOICE_RX_PARAM_SCALE;
	v->gain_fmt3 = query(v->cfg.modem, VOICE_PARAM_RX_GAIN_FMT3)
		       * VOICE_RX_PARAM_SCALE;
	v->gain_other = query(v->cfg.modem, VOICE_PARAM_RX_GAIN_OTHER)
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
	short n = *countp;
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
		dc = sum / have;
		if (v->dc_init)
			v->dc_init = 0;
		else
			dc = dc * VOICE_RX_DC_FOLD + v->dc * VOICE_RX_DC_KEEP;
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
				v->flt[i] = rx_lin[i] * gain;
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

/*
 * Go into transmit: mode 1, the transmit handler, the transmit detector mask
 * and the 8 kHz 8-bit format.
 *
 * IT STARTS THE UNDERRUN LATCH UP, not down.  A path that has just been armed
 * has an empty FIFO by definition, so the first block would report "not
 * enough data" for a condition the host has had no chance to fix; the latch
 * suppresses exactly that one report and `voice_tx` clears it on the first
 * block that has a whole block's worth.
 *
 * The two halves of the format are stored bits-first here and rate-first in
 * `voice_set_rx`, which is scheduling and not source (finding F617's rule:
 * store order needs the full-text test, and neither function has had it).
 * Each is written in its own object's order.
 */
void
voice_set_tx(struct voice_ctx *v)
{
	v->mode = 1;
	v->handler = voice_tx;
	detector_set_enable(v->detector, (short)v->detector_enable_tx);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("PCM 8 bit.\n");

	v->rate_bits.s.bits = 8;
	v->rate_bits.s.rate = 8000;
	v->underrun = 1;
}

/*
 * The transmit block handler: host bytes in, one block of samples out.
 *
 * Two halves with the FIFO between them.  The first un-escapes `*hostcount`
 * bytes of `rx_lin` -- a DLE doubled is one literal DLE, a DLE followed by
 * anything else is a command that goes to `voice_dle_command` and is NOT
 * copied -- and writes what survives to the FIFO.  The second takes a whole
 * block back out if one is there, removes its mean, and converts.
 *
 * `rx_lin` is a BYTE stream here, whatever the handler signature calls it,
 * and the scan reads one past `*hostcount` when the last byte is a DLE:
 * deviation D998.  `tx_flt` is never read.
 */
int
voice_tx(struct voice_ctx *v, short *rx_lin, float *rx_flt, float *tx_flt,
	 short *tx_lin, unsigned short *hostcount, unsigned short *countp)
{
	const unsigned char *in = (const unsigned char *)rx_lin;
	float *flt = rx_flt;
	int nread = VOICE_TX_READ_8000;
	unsigned short i = 0;
	unsigned short j = 0;
	unsigned short k;
	int ret = 0;
	int enough = 0;
	int fill;
	int room;

	(void)tx_flt;

	if (v->rate_bits.s.rate != 8000) {
		flt = v->flt;
		nread = (v->rate_bits.s.rate == 7200) ? VOICE_TX_READ_7200
						      : VOICE_TX_READ_OTHER;
	}
	if (VOICE_OUT_IS_LINEAR(v))
		flt = v->flt;

	while (i < *hostcount) {
		unsigned char c = in[i];

		if (c == VOICE_DLE) {
			i++;			/* D998: may pass the end */
			c = in[i];
			if (c != VOICE_DLE) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "TX: shel comando dle\n");
				ret = voice_dle_command(v, (signed char)c);
				i++;
				continue;
			}
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("TX: double dle\n");
		}
		v->stage[j] = c;
		i++;
		j++;
	}

	FIFO8_write(v->fifo, v->stage, j);

	fill = v->fifo->count;
	switch (v->rate_bits.both) {
	case VOICE_FMT_8BIT_8000:
		enough = fill > 0x9f;
		break;
	case VOICE_FMT_8BIT_7200:
		enough = fill > 0x8f;
		break;
	case VOICE_FMT_8BIT_11025:
		enough = fill > 0xdb;
		break;
	case VOICE_FMT_4BIT_8000:
		enough = fill > 0x4f;
		break;
	case VOICE_FMT_4BIT_7200:
		enough = fill > 0x47;
		break;
	case VOICE_FMT_4BIT_11025:
		enough = fill > 0x6d;
		break;
	default:
		break;
	}

	if (enough) {
		unsigned short n;
		float sum = 0.0f;
		float mean;

		v->underrun = 0;
		/* D996: at 11025 this asks for 222 bytes of a 200-byte area. */
		n = (unsigned short)FIFO8_read(v->fifo, v->stage,
					       (unsigned short)nread);

		for (k = 0; k < n; k++)
			sum += v->stage[k] * VOICE_TX_BYTE_SCALE;
		mean = sum / (int)n;

		if (VOICE_OUT_IS_LINEAR(v)) {
			float offset = mean * 64.0f;

			for (k = 0; k < n; k++)
				flt[k] = v->stage[k] - offset;
		} else {
			for (k = 0; k < n; k++)
				flt[k] = v->stage[k] * VOICE_TX_BYTE_SCALE
					 - mean;
		}

		if (VOICE_OUT_IS_LINEAR(v)) {
			float scale = (v->rate_bits.s.bits == 8)
					  ? VOICE_TX_SCALE_8BIT
					  : VOICE_TX_SCALE_OTHER;

			for (k = 0; k < *countp; k++)
				tx_lin[k] = (short)(int)(flt[k] * scale);
		}
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\n ** NO enouch data  ** \n");
		if (v->underrun == 0 && v->dle_etx == 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "VOICE TX: not enough data to tx\n");
			ret = VOICE_TX_UNDERRUN_STATUS;
			v->underrun = 1;
		}
		/*
		 * D999: the linear arm fills `*countp` samples and the float
		 * arm a fixed 160, which agree only at 8 kHz.
		 */
		if (VOICE_OUT_IS_LINEAR(v)) {
			for (k = 0; k < *countp; k++)
				tx_lin[k] = 0;
		} else {
			for (k = 0; k < VOICE_TX_FLT_FILL; k++)
				rx_flt[k] = 0.0f;
		}
	}

	room = (short)(VOICE_TX_HOST_ROOM - (int)v->fifo->count);
	if (room < 0)
		room = 0;
	*hostcount = (unsigned short)room;
	*countp = 0;

	if (v->int_0014 != v->mode)
		ret = VOICE_TX_MODE_STATUS;
	return ret;
}

/*
 * The full-duplex block handler: the datapump does the whole block, then any
 * queued beep is laid over the RECEIVE side of it.  The beep therefore
 * overwrites what `FDSP_DP_Run` just converted, sample for sample, for as long
 * as the queue lasts -- that is the object's order and not an oversight here.
 *
 * `FDSP_DP_Run`'s return is discarded, and nothing zero-fills: the datapump
 * has already written the whole block.
 */
int
voice_duplex(struct voice_ctx *v, short *rx_lin, float *rx_flt, float *tx_flt,
	     short *tx_lin, unsigned short *hostcount, unsigned short *countp)
{
	int ret = 0;

	FDSP_DP_Run(v->dp, rx_lin, rx_flt, tx_flt, tx_lin, hostcount, countp);

	if (!v->beep_done) {
		unsigned short i = 0;
		int r = 0;

		while (i < *countp && r != 1) {
			r = beepgen_sample(v->beepgen, &rx_flt[i]);
			i++;
		}
		if (r == 1) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("beepgend end, send ok\n");
			v->beep_done = 1;
			FDSP_Kernel_SetInternalBeepInProgress(0);
			ret = 1;
		}
	}

	if (v->int_0014 != v->mode)
		ret = VOICE_DUPLEX_MODE_STATUS;
	return ret;
}
