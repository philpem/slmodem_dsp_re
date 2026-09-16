/*
 * voicesvc.c -- the voice service's core: construct, destroy, command, run.
 *
 * Reconstructed from dsplibs.o, in the object's own emission order:
 *
 *   voice_delete   .text 0x0ac150   181 bytes
 *   voice_create   .text 0x0ac210   642 bytes
 *   voice_command  .text 0x0ac4a0   802 bytes
 *   voice_modem    .text 0x0ac800   338 bytes
 *
 * THE SPAN IS `class1tx.c +94` AND THAT IS NOT A MODULE NAME.  All four sit
 * inside the bracket `tumap.py` labels `class1tx.c`, and none of them is fax:
 * the label comes from the blob's LAYOUT, the same bracket already holds
 * `voice_set_online`, `voice_set_duplex`, `voice_online` and
 * `voice_dle_command`, every symbol here is `voice_*`, and the strings they
 * print are the voice service's own ("voice_create 11.8.98 12:41",
 * "VOICE_ABORT_COMMAND", "modem not in online or duplex").  `voicedp.c` and
 * `voicecmd.h` record the identical correction for their own share of the
 * bracket; this file is a third instance of it, not a new claim.
 *
 * WHAT ELSE IS IN THE ORIGINAL TRANSLATION UNIT.  `_handle_status` (0xac7d0)
 * sits between `voice_command` and `voice_modem` in the blob and this tree
 * keeps it in src/voice/voice.c; past 0xac960 the same TU runs on into
 * `GetGain`, the `beepgen_*` set and the `detector_*` set, which are split
 * across src/service/Beepgen.c, src/service/detector.c and
 * src/service/Fdspkrnl.c.  That split is existing practice and is not undone
 * here; the ORDER of the four definitions below is the object's, because
 * emission order is a register-allocation carrier (F7796, F7800).
 *
 * THREE CALLS IN HERE ARE INLINED IN THE OBJECT AND ARE CALLS IN OURS.
 * `voice_set_online` (47 bytes), `voice_set_duplex` (45) and `_handle_status`
 * (44) are all small enough for -O3's -finline-functions and all three live
 * in the same TU there.  Ours are in src/service/voicedp.c and
 * src/voice/voice.c, so the period compiler cannot see their bodies and emits
 * a call.  The source below is the author's either way -- the inlined blocks
 * at 0xac780, 0xac7a9 and 0xac8e0 are `voice_set_online`'s and
 * `voice_set_duplex`'s instruction for instruction (compare 0xabef0 and
 * 0xabf20).  Findings F8815 and F8823.
 */

#include "dsplib/beepgen.h"
#include "dsplib/debug.h"
#include "dsplib/detector.h"
#include "dsplib/fdspkrnl.h"
#include "dsplib/fifo8.h"
#include "dsplib/silence.h"
#include "dsplib/sysdep.h"
#include "dsplib/vce.h"
#include "dsplib/voice.h"

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
