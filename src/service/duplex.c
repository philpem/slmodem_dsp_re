/*
 * duplex.c -- the voice full-duplex block handler.
 *
 * Recovered TU boundary (issue #6/#20/#67).  The blob's FILE records place
 * `duplex.c` (record 271) between `Tx.c` (270) and `silence.c` (272).  `Tx.c`'s
 * last function is `voice_tx`, ending at 0x0b01de, and `silence.c`'s first is
 * `silence_is_more_then` at 0x0b0360, so [0x0b01de, 0x0b0360) is `duplex.c`'s
 * and holds exactly one global, `voice_duplex` at 0x0b01e0 (251 bytes).
 *
 * The body moved VERBATIM out of src/service/voicedp.c.  Finding F11386.
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
