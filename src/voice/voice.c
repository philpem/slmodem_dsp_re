/*
 * voice.c -- the voice modem service: DLE command, the three mode setters,
 * and the create/delete/command/modem core.
 *
 * Recovered as ONE translation unit, the object's `voice.c`#261
 * (0x0abe20..0x0ac95f).  Four of our files were over-splits of it:
 * `voicecmd.c` (voice_dle_command), `voicedp.c` (voice_set_online,
 * voice_set_duplex, voice_online), `voicesvc.c` (voice_delete, voice_create,
 * voice_command, voice_modem) and this file's `_handle_status`.  The
 * functions are in the object's own emission order because emission order is
 * a register-allocation carrier (F7796) and because three of them are inlined
 * into their callers in the object -- they must share a TU for GCC 3.4.2 to
 * inline them the same way (F8815/F8823).
 *
 * The other `voice.c` in the object (the early record, blob ordinal 4) is
 * src/service/voice.c and holds VOICE_create/delete/command/process and the
 * vce_* hooks; the object has two distinct `voice.c` FILE records.
 *
 * Bodies are moved VERBATIM from the four files; no source text changed
 * except each function's placement.  See F11410.
 */

#include <stddef.h>

#include "dsplib/beepgen.h"
#include "dsplib/debug.h"
#include "dsplib/detector.h"
#include "dsplib/fdspkrnl.h"
#include "dsplib/fifo8.h"
#include "dsplib/pcm.h"
#include "dsplib/silence.h"
#include "dsplib/sysdep.h"
#include "dsplib/vce.h"
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
 * Three arms, and the two that do something are symmetrical: print, set one
 * flag, return.  The default arm sets nothing at all -- so an unknown command
 * byte on a voice connection is silently ignored unless the debug level is
 * above 1, and the caller cannot tell it apart from <DLE><ETX> by the return
 * value alone.
 *
 * The command byte is compared after a sign extension, so 0x83 is -125 here
 * and not 131; it can only ever reach the default arm, and what the object
 * prints for it with `%2x` is the sign-extended word.
 */
int
voice_dle_command(struct voice_ctx *v, signed char cmd)
{
	switch (cmd) {
	case VOICE_DLE_ETX:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("voice dle command: ETX\n");
		v->dle_etx = 1;
		return 0;
	case VOICE_DLE_CAN:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("voice <CAN> command\n");
		v->dle_can = 1;
		return VOICE_DLE_CAN_STATUS;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Unknown command - %2x\n", cmd);
	return 0;
}


/*
 * Go online: mode 2, the beep-only handler, and the detector enabled with
 * whatever mask the context carries.
 */
void
voice_set_online(struct voice_ctx *v)
{
	v->mode = 2;
	v->handler = voice_online;
	detector_set_enable(v->detector, v->detector_enable);
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
 * Tear down whatever was built.
 *
 * Every pointer is guarded and the LAST call is a tail call -- 0xac18c and
 * 0xac200 are both `jmp sysdep_free` off a restored stack, which is what a
 * `return`-less trailing call compiles to.  The order is beepgen, detector,
 * fifo, silence, dp, self; `voice_create`'s own failure path frees the same
 * five in the same order (0xac3e1) and is a second witness to it.
 *
 * The debug line prints the context POINTER with `%lX`, which is the
 * object's format string and not a transcription -- `.rodata.str1.1` 0x4f7f.
 */
void
voice_delete(struct voice_ctx *v)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("voice delete %lX\n", (unsigned long)v);

	if (v->beepgen != 0)
		beepgen_delete(v->beepgen);
	if (v->detector != 0)
		detector_delete(v->detector);
	if (v->fifo != 0)
		FIFO8_delete(v->fifo);
	if (v->silence != 0)
		silence_delete(v->silence);
	if (v->dp != 0)
		FDSP_DP_Delete(v->dp);
	sysdep_free(v);
}

/*
 * Build the voice service.
 *
 * THE LOCAL `beepgen_config` IS NOT DEAD CODE, and the object settles that
 * without ambiguity: 0xac29f is `lea 0x20(%esp),%esi` and %esi becomes
 * `beepgen_create`'s second argument, so the sixteen bytes written at
 * 0xac230-0xac24e are an addressable object that is passed on.  What makes
 * them look dead at first reading is that they are a ROTATION of `cfg`
 * rather than a copy of it -- see `struct voice_config` in voice.h and
 * finding F8813 for the mapping and for what it proves about +0x04.
 *
 * The four loads happen BEFORE `sysdep_malloc`, and that is source order and
 * not scheduling: GCC cannot hoist a load of `*cfg` across an opaque call, so
 * the local is built where the object builds it.
 *
 * The two echo delays come back through `STRM_VCE_GetFDSPEnvironmentalParams`
 * and are read back SIGNED (`movswl` at 0xac2a8 and 0xac2ad) into
 * `FDSP_DP_Create`, whose own parameters are `short`.
 *
 * THE FAILURE ARMS ARE UNREACHABLE FROM ANY FIXTURE, because the only way any
 * of the five constructors returns 0 is a failed `sysdep_malloc` and the
 * harness allocator cannot be made to fail.  They are transcribed from
 * 0xac3e1 and 0xac420 and are NOT covered by t_voicesvc; finding F8822 says
 * so rather than leaving the gap to be discovered.
 */
struct voice_ctx *
voice_create(const struct voice_config *cfg)
{
	struct beepgen_config bcfg;
	struct voice_ctx *v;
	short far_delay, near_delay;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("voice_create 11.8.98 12:41\n");

	if (cfg == 0)
		return 0;

	/*
 * The rotation.  `get_sreg` is the S-register getter and it lands in
 * beepgen's `get_sreg` slot, which beepgen calls as `f(modem, 24)` --
	 * SREG_FLASH_TIMER.  The cast is the one place the two readings of
	 * that callback's RESULT meet: `detector_create` and `silence_create`
	 * take it `unsigned int` (F8803's `shr`), `beepgen_config` takes it
	 * `int`, and `vce_get_sreg` itself is declared `int`.  No single
	 * spelling satisfies all three; the field keeps the two-of-three one
	 * and the odd site casts.  Finding F8814.
	 */
	bcfg.modem = cfg->modem;
	bcfg.hook_on = cfg->hook_on;
	bcfg.hook_off = cfg->hook_off;
	bcfg.get_sreg = (int (*)(void *, int))cfg->get_sreg;

	v = sysdep_malloc(sizeof(*v));
	if (v == 0)
		return 0;
	sysdep_memset(v, 0, sizeof(*v));

	v->cfg = *cfg;

	STRM_VCE_GetFDSPEnvironmentalParams(&far_delay, &near_delay);
	v->dp = FDSP_DP_Create(0, far_delay, near_delay);

	v->beepgen = beepgen_create(0, &bcfg);
	if (v->beepgen == 0)
		goto fail;

	v->detector = detector_create(0, v->cfg.modem, v->cfg.get_sreg);
	if (v->detector == 0)
		goto fail;

	v->fifo = FIFO8_create(v->fifo, 0);
	if (v->fifo == 0)
		goto fail;

	v->silence = silence_create(v->silence, v->cfg.modem, v->cfg.get_sreg);
	if (v->silence == 0)
		goto fail;

	/*
	 * The initialisation block, 0xac347-0xac3c0.  `sysdep_memset` has
	 * already zeroed all of it, so the four stores of 0 are redundant in
	 * the object too and are transcribed rather than elided; the order is
	 * the object's store order, which the scheduler interleaved but did
	 * not reorder across.
	 */
	v->mode = VOICE_MODE_ONLINE;
	v->dle_etx = 0;
	v->detector_enable_tx = DETECTOR_ENABLE_ALL;
	v->detector_enable_rx = DETECTOR_ENABLE_ALL;
	v->detector_enable = DETECTOR_ENABLE_ALL;
	v->dle_can = 0;
	v->marker_period = 0;
	v->rate_bits.s.rate = 8000;
	v->int_0014 = 4;
	v->handler = voice_online;
	v->out_format = 0;
	v->rate_bits.s.bits = 8;
	v->beep_done = 1;

	return v;

fail:
	/*
	 * The teardown.  The object has TWO entries into it -- 0xac3e1 when
	 * `beepgen_create` failed and 0xac420 for the three constructors
	 * after it -- and they differ only in that the first skips the
	 * beepgen test, which the compiler knows is NULL on that edge.  One
	 * label with the guard says the same thing.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("voice delete %lX\n", (unsigned long)v);
	if (v->beepgen != 0)
		beepgen_delete(v->beepgen);
	if (v->detector != 0)
		detector_delete(v->detector);
	if (v->fifo != 0)
		FIFO8_delete(v->fifo);
	if (v->silence != 0)
		silence_delete(v->silence);
	if (v->dp != 0)
		FDSP_DP_Delete(v->dp);
	sysdep_free(v);
	return 0;
}

/*
 * The host command interface: eleven opcodes, a jump table at .rodata 0xed80,
 * and 7 for anything else.
 *
 * `arg` is an array of ints and only VOICE_BEEP_COMMAND reads three of them.
 * The debug line is printed BEFORE the range check, so an out-of-range opcode
 * still names itself in the log.
 *
 * TWO ARMS HAVE A MODE GATE and each carries its OWN copy of it (0xac4d6 for
 * DTMF, 0xac66e for beep), sharing only the refusal at 0xac4e5.
 *
 * THE OPCODE THAT DOES NOTHING IS REAL.  VOICE_OUTPUT_TRANSMIT_LEVEL_COMMAND
 * prints its argument and returns 0 without storing it anywhere -- there is
 * no store on that path at all (0xac5b4-0xac5d3), and at the shipping debug
 * level it does not even print.  Deviation D1010.
 *
 * THE OPCODES 7 AND 8 ARE EASY TO CROSS, and the jump table at .rodata 0xed80
 * is the authority: 7 goes to 0xac61e, which prints
 * "VOICE_TIME_MARK_COMMAND", and 8 to 0xac59a, which prints
 * "VOICE_VLS_COMMAND".  Finding F8816.
 */
int
voice_command(struct voice_ctx *v, int cmd, int *arg)
{
	int ret = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("voice_command # %d\n", cmd);

	switch (cmd) {
	case VOICE_BEEP_COMMAND:
		if (v->mode != VOICE_MODE_ONLINE &&
		    v->mode != VOICE_MODE_DUPLEX) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "modem not in online or duplex\n");
			ret = 7;
			break;
		}
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("call beep gen with: %d %d %d\n",
					     arg[0], arg[1], arg[2]);
		FDSP_Kernel_SetInternalBeepInProgress(1);
		beepgen_start_beep(v->beepgen, arg[0], arg[1], arg[2]);
		v->beep_done = 0;
		break;

	case VOICE_DTMF_COMMAND:
		/*
		 * The same gate written a second time, and NO debug line of
		 * its own -- 0xac718 goes straight to the kernel flag.
		 */
		if (v->mode != VOICE_MODE_ONLINE &&
		    v->mode != VOICE_MODE_DUPLEX) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "modem not in online or duplex\n");
			ret = 7;
			break;
		}
		FDSP_Kernel_SetInternalBeepInProgress(1);
		beepgen_start_dtmf(v->beepgen, arg[0], arg[1]);
		v->beep_done = 0;
		break;

	case VOICE_SET_MODE_COMMAND:
		/*
		 * Four setters, and `beep_done` is raised afterwards on EVERY
		 * path including the one that recognised nothing: 0xac65e is
		 * the join of all five edges, the default among them.
		 */
		switch (arg[0]) {
		case VOICE_MODE_RX:
			voice_set_rx(v);
			break;
		case VOICE_MODE_TX:
			voice_set_tx(v);
			break;
		case VOICE_MODE_ONLINE:
			voice_set_online(v);
			break;
		case VOICE_MODE_DUPLEX:
			voice_set_duplex(v);
			break;
		}
		v->beep_done = 1;
		break;

	case VOICE_OUTPUT_TRANSMIT_LEVEL_COMMAND:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "VOICE_OUTPUT_TRANSMIT_LEVEL_COMMAND %d\n", arg[0]);
		break;

	case VOICE_DETECTOR_ENABLE_COMMAND:
		/*
		 * One word carrying three byte-wide masks, and the object's
		 * encoding of the third is what fixes the spelling: the low
		 * two are `movzbl %cl` and `movzbl %ch`, the top one is
		 * `sar $0x10` FOLLOWED BY `and $0xff` (0xac602).  A signed
		 * shift then a mask is `(w >> 16) & 0xff` on a signed int;
		 * an unsigned `w` would have given `shr` and no mask.
		 * No debug line on this arm.
		 */
		v->detector_enable_tx = arg[0] & 0xff;
		v->detector_enable_rx = (arg[0] >> 8) & 0xff;
		v->detector_enable = (arg[0] >> 16) & 0xff;
		break;

	case VOICE_PLAYBACK_VOLUME_COMMAND:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "VOICE_PLAYBACK_VOLUME_COMMAND %d\n", arg[0]);
		v->playback_volume = (short)arg[0];
		break;

	case VOICE_TIME_MARK_COMMAND:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VOICE_TIME_MARK_COMMAND %d\n",
					     arg[0]);
		v->marker_period = arg[0];
		break;

	case VOICE_VLS_COMMAND:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VOICE_VLS_COMMAND %d\n", arg[0]);
		v->out_format = arg[0];
		break;

	case VOICE_ABORT_COMMAND:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VOICE_ABORT_COMMAND\n");
		v->dle_can = 1;
		break;

	case VOICE_RESET_DUPLEX_COMMAND: {
		short far_delay, near_delay;

		/*
		 * The one arm whose debug line comes AFTER its work: 0xac563
		 * tests the level, 0xac56a stores the new kernel, and only
		 * then does 0xac56f print.
		 */
		STRM_VCE_GetFDSPEnvironmentalParams(&far_delay, &near_delay);
		v->dp = FDSP_DP_Create(v->dp, far_delay, near_delay);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VOICE_RESET_DUPLEX_COMMAND\n");
		break;
	}

	default:
		ret = 7;
		break;
	}

	return ret;
}

/*
 * Status mapping leaf: 1->10, 2->11, 4->12, otherwise the handler result.
 */
int
_handle_status(int status, int code)
{
	if (code == 1)
		return 10;
	if (code == 2)
		return 11;
	if (code == 4)
		return 12;
	return status;
}

/*
 * One block through the voice service: the detector first, then whichever
 * handler `mode` currently selects.
 *
 * THE DETECTOR'S OUTPUT MODE IS THE OPPOSITE WAY ROUND FROM THE OBVIOUS
 * READING.  0xac81a is `cmpl $0x3,0x10(%ebx)` and 0xac826 is `je 0xac928`,
 * and 0xac928 is the `detector_set_output_status` call -- so DUPLEX takes the
 * STATUS arm and every other mode takes IN_STREAM.  Written the other way
 * round it passes any single-block fixture whose detector never fires.
 *
 * THE BUFFER ARITHMETIC IS IN BYTES.  `tx_lin` is the handler signature's
 * 16-bit block, but the detector appends DLE-escaped BYTES to it and reports
 * how many, and 0xac877 advances the handler's copy of the pointer with a
 * plain `add` rather than an `lea (,%edx,2)`.  So the offset is a byte offset
 * and the source has to say so.
 *
 * AND THE SEVENTH ARGUMENT THE HANDLER GETS IS A LOCAL, not the caller's
 * `countp` (0xac86b, `lea 0x28(%esp)`).  The handler sees the count the block
 * arrived with, writes back through the local, and `voice_modem` then adds
 * the detector's byte count to whatever the handler left there before storing
 * it through the caller's pointer.
 *
 * THE STATUS CHAIN IS `_handle_status` AND ONLY MAPS THREE OF SIX CODES.
 * `detector_progress` answers 1 and 2 for the two cadences and 3, 4, 5, 6 for
 * the four tones; 1, 2 and 4 become 10, 11 and 12 here and 3, 5 and 6 are
 * discarded in favour of the handler's own return.  Deviation D1011.
 */
int
voice_modem(struct voice_ctx *v, short *rx_lin, float *rx_flt, float *tx_flt,
	    short *tx_lin, unsigned short *hostcount, unsigned short *countp)
{
	unsigned char *out = (unsigned char *)tx_lin;
	unsigned short det_len = 0;
	unsigned short saved = *countp;
	int st, r;

	if (v->mode == VOICE_MODE_DUPLEX)
		detector_set_output_status(v->detector);
	else
		detector_set_output_in_stream(v->detector);

	st = detector_progress(v->detector, tx_flt, (short)*countp, out, &det_len);

	r = v->handler(v, rx_lin, rx_flt, tx_flt, (short *)(out + det_len),
		       hostcount, &saved);

	*countp = saved + det_len;
	v->int_0014 = v->mode;

	if (v->dle_etx != 0 || v->dle_can != 0) {
		voice_set_online(v);
		v->dle_etx = 0;
		v->dle_can = 0;
	}

	return _handle_status(r, st);
}
