/*
 * voice.h -- the voice service's context object, and the leaves of the voice
 * TU that this tree has reached.
 *
 * THE STRUCT'S HOME IS HERE.  `struct voice_ctx` was introduced in
 * `voicecmd.h`, modelled only as far as `voice_dle_command` could see it (a
 * 0x744-byte pad and two int flags).  The per-block handlers -- `voice_online`,
 * `voice_duplex` and `voice_tx` -- reach a dozen more fields and `voice_create`
 * settles the SIZE, so the definition moved here and `voicecmd.h` now includes
 * this file.  One type, one home (CLAUDE.md); the TAG kept its original
 * spelling because `src/service/voicecmd.c` already names it.
 *
 * `sizeof` is 0x7dc.  That is not inferred: `voice_create` (0xac210) passes
 * 0x7dc to `sysdep_malloc` at 0xac247 and to `sysdep_memset` at 0xac268.
 *
 * NEITHER IS THE SPAN LABEL EVIDENCE OF ANYTHING.  `voice_set_online`,
 * `voice_set_duplex`, `voice_online` and `voice_create` sit in a bracket
 * `tumap.py` labels `class1tx.c`, and `voice_tx` / `voice_duplex` in one
 * labelled `Fdspkrnl.c`.  Both labels come from the blob's LAYOUT.  Every one
 * of these symbols is voice: they are installed into this object's handler
 * slot by the two setters below, and the strings they print say so.
 */

#ifndef DSPLIB_VOICE_H
#define DSPLIB_VOICE_H

#include "dsplib/beepgen.h"

#ifdef __cplusplus
extern "C" {
#endif

struct voice_ctx;
struct detector;
struct fifo8;
struct silence;

/*
 * `struct voice_config` -- the sixteen-byte host block `voice_create` is
 * handed, and which lives at `voice_ctx` +0x000.
 *
 * IT IS NOT A `struct beepgen_config`, and this header used to say it was.
 * `voice_create` builds a SEPARATE 16-byte local for `beepgen_create` and
 * ROTATES three of the four words on the way in (0xac230-0xac24e against
 * 0xac29f):
 *
 *	voice_config		beepgen_config		struct beepgen
 *	+0x00 modem	  ->	+0x00 modem	  ->	+0x000 modem
 *	+0x08 fn_08	  ->	+0x04 fn_04	  ->	+0x11c fn_011c
 *	+0x0c fn_0c	  ->	+0x08 fn_08	  ->	+0x120 hook_on_proc
 *	+0x04 fn_04	  ->	+0x0c fn_0c	  ->	+0x124 fn_0124
 *
 * so the two blocks are different types that happen to share a size.  That
 * rotation is what SETTLES the +0x04 slot: it is the S-register getter --
 * `detector_create` (0xac2f8) and `silence_create` (0xac337) both take it as
 * their third argument, `voicedp.c` reads the receive gains through it, and
 * `beepgen`'s `fn_0124` -- which is the slot it lands in -- is called as
 * `f(modem, 24)`, i.e. `vce_get_sreg(modem, SREG_FLASH_TIMER)`.  Findings
 * F8813 and F8814.
 *
 * The three names stay as `voicedp.c` already spells them; only the TYPES and
 * the derivations are new.  `fn_04` is spelled the way `detector.h`'s
 * `detector_sreg_fn` and `silence_create`'s third parameter are spelled --
 * `unsigned int` result, from the `shr` at 0xad531 (F8803).
 */
struct voice_config {
	void		*modem;			/* +0x00 host handle       */
	unsigned int	(*fn_04)(void *modem, int num);
						/* +0x04 vce_get_sreg      */
	void		(*fn_08)(void *modem);	/* +0x08 -> beepgen fn_011c */
	void		(*fn_0c)(void *modem);	/* +0x0c -> hook_on_proc   */
};

/*
 * What goes in `voice_ctx.handler` at +0x20.  The shape is the one
 * `FDSP_DP_Run` has (see beepgen.h), which is what `voice_duplex` proves: it
 * forwards arguments 1..6 of its own signature to that function unchanged,
 * slot for slot.
 *
 * Only three of the seven are read by all three handlers -- the context, the
 * float buffer and `countp`.  `rx_lin` is a 16-bit linear block to
 * `FDSP_DP_Run` and an escaped BYTE stream to `voice_tx`, which casts it;
 * `voice_online` reads neither it nor `tx_flt` at all.
 *
 * `hostcount` and `countp` are both in/out counts and they are NOT the same
 * count: `countp` is the block's sample count and `hostcount` the host's byte
 * count.  See voicedp.c's banner for how each handler uses them, and finding
 * F8786 for how this typed `FDSP_DP_Run`'s sixth argument, which that
 * function itself never loads.
 */
typedef int (*voice_handler_fn)(struct voice_ctx *v, short *rx_lin,
				float *rx_flt, float *tx_flt, short *tx_lin,
				unsigned short *hostcount,
				unsigned short *countp);

/*
 * The sample format, as one 32-bit word at +0x758.
 *
 * The two halves are written separately by `voice_create` (`mov %ax,0x758`,
 * `mov %si,0x75a` -- 8000 and 8) and read separately by `voice_tx` (`movzwl
 * 0x758(%edi)`, `cmpw $0x8,0x75a(%edi)`), so they are two 16-bit fields.  But
 * `voice_tx` also loads all four bytes at once (`mov 0x758(%edi),%eax`) and
 * dispatches on the pair against six 32-bit constants; GCC 3.4 does not fuse
 * two adjacent 16-bit compares into one 32-bit compare, so the combined read
 * is in the source and a union is what spells all three accesses without a
 * cast.
 */
union voice_rate_bits {
	struct {
		unsigned short	rate;	/* +0x00 8000, 7200 or 11025      */
		unsigned short	bits;	/* +0x02 8 or 4                   */
	} s;
	unsigned int	both;		/* (bits << 16) | rate            */
};

/* The six (bits, rate) pairs `voice_tx` knows, as that combined word. */
#define VOICE_RATE_BITS(bits, rate) \
	((((unsigned int)(bits)) << 16) | ((unsigned int)(rate)))

/*
 * The voice service's context.
 *
 * Names are graded as CLAUDE.md asks.  `cfg`, `beepgen`, `detector`, `fifo`,
 * `silence` and `dp` are typed by the callee `voice_create` hands each to.
 * `dle_etx` / `dle_can` are `voice_dle_command`'s, unchanged.  `mode` is named
 * from the two setters, which write 2 and 3 into it while installing the
 * matching handler.  `beep_done`, `out_format`, `underrun` and
 * `detector_enable` are USAGE INFERENCE -- the weakest grade -- and their
 * comments say what was actually observed.  Everything left as `pad_*` or
 * `short_NNNN` is space no reconstructed function touches.
 */
struct voice_ctx {
	/*
	 * +0x000  The sixteen-byte host block, copied in whole by
	 * `voice_create` (0xac27a-0xac295) and NOT what `beepgen_create` is
	 * handed -- see `struct voice_config` above for the rotation.
	 */
	struct voice_config	cfg;

	/*
	 * +0x010  The service mode.  `voice_set_online` writes 2 and
	 * `voice_set_duplex` writes 3, each beside the handler it installs,
	 * and `voice_create` starts it at 2.
	 *
	 * +0x014  Compared against `mode` by all three handlers and by
	 * nothing else reconstructed here; a mismatch is what makes a handler
	 * return its nonzero code (2, 5 and 3 respectively).  `voice_create`
	 * starts it at 4, so it differs from `mode` from the outset.  What
	 * sets it is unwritten, so it keeps a neutral name.
	 */
	int			mode;
	int			int_0014;

	struct beepgen		*beepgen;	/* +0x018 beepgen_create   */
	struct detector		*detector;	/* +0x01c detector_create  */
	voice_handler_fn	handler;	/* +0x020 voice_online at
						 *        creation         */
	struct fifo8		*fifo;		/* +0x024 FIFO8_create     */
	struct silence		*silence;	/* +0x028 silence_create   */
	unsigned char		pad_002c[0x034 - 0x02c];

	/*
	 * +0x034  The datapump kernel.  `voice_create` fills it from
	 * `FDSP_DP_Create` (0xac2c1) and `voice_delete` hands it to
	 * `FDSP_DP_Delete` (0xac1f3), so its TRUE type is
	 * `struct fdsp_kernel *`.
	 *
	 * IT IS SPELLED `void *` BECAUSE ONE DECLARATION IN ANOTHER FILE IS
	 * STILL BEHIND.  `FDSP_DP_Run` (beepgen.h, defined in
	 * src/service/beepgen.c) declares the same object `int *status`,
	 * because that function's only use of it is to store 2 into the
	 * kernel's `status` (named from this very cross-reference, finding
	 * F10143), and `voicedp.c` passes this field straight into it.
	 * `void *` is the one spelling both call sites accept without a
	 * cast; when `FDSP_DP_Run`'s parameter is retyped this becomes
	 * `struct fdsp_kernel *`.  Finding F8817.
	 */
	void			*dp;		/* +0x034 struct fdsp_kernel * */

	/*
	 * +0x038  The byte staging area between the host and the FIFO.
	 * `voice_tx` un-escapes into it, hands it to `FIFO8_write`, and later
	 * reads a block back into it with `FIFO8_read`.
	 *
	 * ITS LENGTH IS BOUNDED ONLY BY THE NEXT KNOWN FIELD, and 200 bytes
	 * is not enough for what the object puts in it: at 11025 Hz
	 * `voice_tx` asks `FIFO8_read` for 222.  That overrun is the
	 * object's, is reproduced rather than repaired, and is deviation
	 * D996.
	 */
	unsigned char		stage[0x100 - 0x038];

	/*
	 * +0x100  The float working buffer.  `voice_tx` uses it in place of
	 * the caller's `rx_flt` whenever the output is 16-bit linear or the
	 * rate is not 8000.  The LENGTH here fills the space up to the next
	 * field this tree has read and is not itself established.
	 */
	float			flt[(0x740 - 0x100) / 4];

	/*
	 * +0x740  Non-zero once the queued beep has been played out.  All
	 * three handlers skip `beepgen_sample` while it is up, and the one
	 * that retires the last tone sets it, prints "beepgend end, send ok"
	 * and calls FDSP_Kernel_SetInternalBeepInProgress(0).
	 * `voice_create` starts it at 1, so a fresh object is not beeping.
	 */
	int			beep_done;

	int			dle_etx;	/* +0x744 <DLE><ETX> seen  */
	int			dle_can;	/* +0x748 <DLE><CAN> seen  */

	/*
	 * +0x74c  Output format selector.  Every reconstructed READ is the
	 * same test, `(unsigned)(out_format - 1) <= 1`: 1 and 2 send 16-bit
	 * linear to `tx_lin`, anything else sends float.  `voice_create`
	 * starts it at 0.  What 1 and 2 mean apart from each other is still
	 * not established.
	 *
	 * WHAT IS NOW ESTABLISHED IS WHO WRITES IT, and it is stronger
	 * evidence than the reads were: the only writer in the object is
	 * `voice_command`'s case 8, which prints "VOICE_VLS_COMMAND %d"
	 * (.rodata.str1.1 0x500e) and stores that word here whole
	 * (0xac5a7-0xac5af).  So this field is the argument of the host's
	 * VLS command and the name stays `out_format` only because the
	 * READS are what say what it selects; the object does not say what
	 * "VLS" expands to and neither does this header.  Finding F8819.
	 */
	int			out_format;

	unsigned char		pad_0750[0x754 - 0x750];

	/*
	 * +0x754  The playback volume, from the author's own format string:
	 * `voice_command`'s case 6 prints "VOICE_PLAYBACK_VOLUME_COMMAND %d"
	 * and then stores the low sixteen bits of the same word here
	 * (0xac5e5-0xac5ee).  That is evidence class 1 for the NAME.
	 *
	 * NOTHING IN THE 1.2 MB READS IT BACK, so the signedness is not
	 * established and neither is the unit; `short` is the store's width
	 * and no more than that.
	 */
	short			playback_volume;

	/*
	 * +0x756  The receive path's arm.  `voice_set_rx` sets it to 1 beside
	 * the handler it installs; `voice_rx` does nothing at all unless it
	 * reads exactly 1 (it prints "RX WAIT ABORT" and gives up), and
	 * clears it on the block that sends <DLE><ETX>.
	 */
	short			rx_armed;

	union voice_rate_bits	rate_bits;	/* +0x758                  */

	/*
	 * +0x75c  Latches the "not enough data" report so `voice_tx` makes it
	 * once: set when the report is made, cleared on the first block that
	 * has enough.  The report itself is suppressed after <DLE><ETX>.
	 */
	short			underrun;

	/*
	 * +0x75e  The transmit path's detector mask, typed the same way as
	 * the two below it: `voice_set_tx` hands it to `detector_set_enable`.
	 * `voice_create` starts it at 0x3f.
	 */
	unsigned short		detector_enable_tx;

	/*
	 * +0x760  The receive path's detector mask, typed by the callee
	 * `voice_set_rx` hands it to, exactly as `detector_enable` below is
	 * typed by `voice_set_online`.  `voice_create` starts it at 0x3f.
	 */
	unsigned short		detector_enable_rx;

	/*
	 * +0x762  The word `voice_set_online` hands to `detector_set_enable`;
	 * that callee is what types it.  `voice_create` starts it at 0x3f.
	 * `voice_set_duplex` passes 0x24 instead and does not read this.
	 */
	unsigned short		detector_enable;

	/*
	 * +0x764  How often `voice_rx` emits its <DLE>'T' marker, in units of
	 * 800 samples -- 100 ms at this object's 8 kHz.  Both `voice_set_rx`
	 * and `voice_rx` reload +0x766 with `marker_period * 800` and count
	 * it down one per output sample; a zero here disables the marker
	 * outright.  `voice_create` starts it at 0, so the marker is off
	 * until something sets it.
	 *
	 * +0x766  That countdown.
	 */
	unsigned short		marker_period;
	unsigned short		marker_countdown;

	unsigned char		pad_0768[0x7c8 - 0x768];

	/*
	 * +0x7c8  Seed the DC estimate rather than smooth it.  `voice_set_rx`
	 * sets it to 1 and `voice_rx` clears it on the first block it sees,
	 * taking that block's mean whole; every later block folds in at one
	 * part in a hundred.
	 *
	 * +0x7cc  The DC estimate itself, subtracted from every receive
	 * sample.
	 */
	int			dc_init;
	float			dc;

	/*
	 * +0x7d0 / +0x7d8 / +0x7d4  The receive gain, one per output format,
	 * each read from the host through `cfg.fn_04` at +0x48, +0x8a and
	 * +0x8b and scaled by 1/128.  `voice_rx` picks `fmt1` for format 1,
	 * `fmt3` for format 3 and `other` for everything else, which is what
	 * names them -- nothing in the object names the three parameters.
	 *
	 * The two 16-bit formats additionally divide by 32767 on the way in;
	 * the `other` arm does not.  That asymmetry is the object's.
	 */
	float			gain_fmt1;
	float			gain_other;
	float			gain_fmt3;
	/* 0x7dc bytes in total -- voice_create's allocation size. */
};

/*
 * The DLE byte `voice_tx` un-escapes on.  Doubled it is one literal DLE;
 * followed by anything else it introduces a command byte, which goes to
 * `voice_dle_command` and is not copied through.
 */
#define VOICE_DLE	0x10

/*
 * The byte `voice_rx` shields with a DLE every `marker_period` blocks of 800
 * samples.  Nothing in the object says what the host makes of it; the letter
 * is the object's own immediate (`movb $0x54,...` at 0xaf47d).
 */
#define VOICE_DLE_MARK	'T'

/*
 * The three settings `voice_set_rx` reads through `cfg.fn_04`.  They are not
 * in `modem_params.h`, which stops at 63 plus one outlier -- the same
 * position `silence.h` is in with its own 0x52 and 0x53, and the same answer:
 * the numbers are the object's and the names say what the answers are USED
 * for, which is all the object establishes.
 */
#define VOICE_PARAM_RX_GAIN_FMT1	0x48
#define VOICE_PARAM_RX_GAIN_FMT3	0x8a
#define VOICE_PARAM_RX_GAIN_OTHER	0x8b

/**
 * @brief Map a detector event code to a host status code.
 *
 * @param status The caller's running status, returned unchanged for any
 *               @p code not listed below.
 * @param code   A `detector_progress` event code.
 * @return 10 for code 1, 11 for code 2, 12 for code 4; otherwise @p status.
 */
int _handle_status(int status, int code);

/**
 * @brief The beep-only per-block handler (`mode` == ::VOICE_MODE_ONLINE).
 *
 * Nothing arrives from the line: the block is filled entirely from the beep
 * generator, and once its queue runs dry the rest of this block and every
 * later one is silence. @p rx_lin and @p tx_flt are never read.
 *
 * @param v          The voice context.
 * @param rx_lin     Unused.
 * @param rx_flt     Unused.
 * @param tx_flt     Unused.
 * @param tx_lin     Out: the block of samples to send to the line.
 * @param hostcount  In/out: host byte count (unaffected by this handler).
 * @param countp     In/out: block sample count.
 * @return 0 in the paths this tree has reached.
 */
int voice_online(struct voice_ctx *v, short *rx_lin, float *rx_flt,
		 float *tx_flt, short *tx_lin, unsigned short *hostcount,
		 unsigned short *countp);

/**
 * @brief The transmit per-block handler (`mode` == ::VOICE_MODE_TX).
 *
 * Two halves with the FIFO between them: un-escape `*hostcount` bytes of
 * @p rx_lin (a doubled DLE is one literal byte, a DLE followed by anything
 * else is a command routed to voice_dle_command() and not copied) into the
 * FIFO, then take a whole block back out if one is ready, remove its mean,
 * and convert it into @p tx_lin. @p rx_lin is a byte stream here despite its
 * declared type, and @p tx_flt is never read.
 *
 * @param v          The voice context.
 * @param rx_lin     In: escaped host bytes, cast to `const unsigned char *`.
 * @param rx_flt     Unused.
 * @param tx_flt     Unused.
 * @param tx_lin     Out: the converted block of line samples.
 * @param hostcount  In: bytes of @p rx_lin available to un-escape.
 * @param countp     Out: block sample count produced.
 * @return 0 in the paths this tree has reached.
 */
int voice_tx(struct voice_ctx *v, short *rx_lin, float *rx_flt,
	     float *tx_flt, short *tx_lin, unsigned short *hostcount,
	     unsigned short *countp);

/**
 * @brief The full-duplex per-block handler (`mode` == ::VOICE_MODE_DUPLEX).
 *
 * Runs `FDSP_DP_Run` for the datapump kernel's own transmit/receive path,
 * then overlays a queued beep onto @p rx_flt sample by sample until it
 * finishes.
 *
 * @param v          The voice context.
 * @param rx_lin     Passed through to `FDSP_DP_Run`.
 * @param rx_flt     In/out: receive float block; the beep is mixed into it.
 * @param tx_flt     Passed through to `FDSP_DP_Run`.
 * @param tx_lin     Passed through to `FDSP_DP_Run`.
 * @param hostcount  Passed through to `FDSP_DP_Run`.
 * @param countp     Passed through to `FDSP_DP_Run`.
 * @return `VOICE_DUPLEX_MODE_STATUS` if the mode changed underneath this
 *         call, 1 if the beep just finished, 0 otherwise.
 */
int voice_duplex(struct voice_ctx *v, short *rx_lin, float *rx_flt,
		 float *tx_flt, short *tx_lin, unsigned short *hostcount,
		 unsigned short *countp);

/**
 * @brief The receive per-block handler (`mode` == ::VOICE_MODE_RX).
 *
 * Four stages: fold the block's mean into a running DC estimate and remove
 * it; scale by the gain the output format selects; convert each sample to
 * an escaped u-law byte in @p tx_lin, doubling a literal DLE and inserting a
 * periodic `<DLE>'T'` marker; then let `silence_progress` append its own
 * escapes, closing the stream with `<DLE><ETX>` if a `<DLE><CAN>` is
 * pending. @p rx_flt is never read.
 *
 * @param v          The voice context.
 * @param rx_lin     In: line samples.
 * @param rx_flt     Unused.
 * @param tx_flt     In (formats 1/3) or scratch: the float working buffer.
 * @param tx_lin     Out: the escaped byte stream, cast to `unsigned char *`.
 * @param hostcount  Out: bytes written to @p tx_lin.
 * @param countp     In: samples in @p rx_lin.
 * @return 7 if formats 1 or 3 are asked for a block over 200 samples;
 *         0 otherwise.
 */
int voice_rx(struct voice_ctx *v, short *rx_lin, float *rx_flt,
	     float *tx_flt, short *tx_lin, unsigned short *hostcount,
	     unsigned short *countp);

/** @brief Go online: mode ::VOICE_MODE_ONLINE, install voice_online(), and
 *         enable the detector with the context's own mask. */
void voice_set_online(struct voice_ctx *v);

/** @brief Go duplex: mode ::VOICE_MODE_DUPLEX, install voice_duplex(), and
 *         enable the detector with a fixed mask (0x24), not the context's. */
void voice_set_duplex(struct voice_ctx *v);

/**
 * @brief Arm the receive path: mode ::VOICE_MODE_RX, install voice_rx(),
 * enable the detector with the receive mask, create the silence detector,
 * reset the DC estimate to be seeded fresh, fix the rate/format to 8 bit /
 * 8000 Hz, and read the three receive gains back from the host.
 */
void voice_set_rx(struct voice_ctx *v);

/** @brief Arm the transmit path: mode ::VOICE_MODE_TX, install voice_tx(),
 *         enable the detector with the transmit mask, fix the rate/format
 *         to 8 bit / 8000 Hz, and latch the one-shot underrun report. */
void voice_set_tx(struct voice_ctx *v);

/*
 * The four values of `mode`.
 *
 * ONLINE and DUPLEX are the author's own words: `voice_command` refuses the
 * beep and DTMF commands outside `(unsigned)(mode - 2) <= 1` and prints
 * "modem not in online or duplex" (.rodata.str1.4 0x12e48) when it does.
 * That is evidence class 1, and it also fixes which of the two is which --
 * `voice_set_online` writes 2 and installs `voice_online`,
 * `voice_set_duplex` writes 3 and installs `voice_duplex`.
 *
 * 0 and 1 are `voice_set_rx`'s and `voice_set_tx`'s, from the same pairing in
 * voicedp.c; nothing in the object names those two, so they are usage.
 */
#define VOICE_MODE_RX		0
#define VOICE_MODE_TX		1
#define VOICE_MODE_ONLINE	2
#define VOICE_MODE_DUPLEX	3

/*
 * `voice_command`'s opcodes.  The switch is dense 0..10 with a jump table at
 * .rodata 0xed80, and ANYTHING ELSE -- including 4, whose table slot is the
 * default label -- returns 7.
 *
 * SIX OF THE ELEVEN WEAR THE AUTHOR'S OWN NAME.  Their arms print a format
 * string that is the constant's identifier followed by its argument, so the
 * spelling below is transcribed, not invented (evidence class 1):
 *
 *	 3  .rodata.str1.4 0x12e68  "VOICE_OUTPUT_TRANSMIT_LEVEL_COMMAND %d"
 *	 6  .rodata.str1.4 0x12e90  "VOICE_PLAYBACK_VOLUME_COMMAND %d"
 *	 7  .rodata.str1.1 0x04fdd  "VOICE_TIME_MARK_COMMAND %d"
 *	 8  .rodata.str1.1 0x0500e  "VOICE_VLS_COMMAND %d"
 *	 9  .rodata.str1.1 0x04ff9  "VOICE_ABORT_COMMAND"
 *	10  .rodata.str1.1 0x04fc1  "VOICE_RESET_DUPLEX_COMMAND"
 *
 * The other five arms print nothing that names them, so 0, 1, 2 and 5 are
 * named from what they DO -- `beepgen_start_beep`, `beepgen_start_dtmf`, the
 * four mode setters and the three `detector_enable*` masks -- which is usage
 * inference, the weakest grade.  4 gets no name at all: the object has no arm
 * for it, only a table slot pointing at the default.
 */
#define VOICE_BEEP_COMMAND			0	/* usage        */
#define VOICE_DTMF_COMMAND			1	/* usage        */
#define VOICE_SET_MODE_COMMAND			2	/* usage        */
#define VOICE_OUTPUT_TRANSMIT_LEVEL_COMMAND	3
/*      (4 is not an opcode -- its table slot is the default label)    */
#define VOICE_DETECTOR_ENABLE_COMMAND		5	/* usage        */
#define VOICE_PLAYBACK_VOLUME_COMMAND		6
#define VOICE_TIME_MARK_COMMAND			7
#define VOICE_VLS_COMMAND			8
#define VOICE_ABORT_COMMAND			9
#define VOICE_RESET_DUPLEX_COMMAND		10

/* The largest opcode the switch accepts; `cmp $0xa,%ebx; ja` at 0xac4ca. */
#define VOICE_COMMAND_MAX			10

/*
 * The voice service itself.  See src/service/voicesvc.c.
 *
 * `voice_command`'s `arg` is read as three 32-bit words, at (%esi), 0x4(%esi)
 * and 0x8(%esi); only VOICE_BEEP_COMMAND reads all three, most read one, and
 * VOICE_RESET_DUPLEX_COMMAND reads none.  It returns 0 when the command was
 * acted on and 7 when it was not -- an unknown opcode, or the beep/DTMF mode
 * gate refusing.  Neither number is named by anything in the object, so both
 * stay literal, exactly as the handlers' 1..13 do.
 */
/**
 * @brief Build the voice service core: beep generator, cadence/tone
 * detector, FIFO, silence detector and datapump kernel.
 *
 * Builds a separate, rotated `beepgen_config` local from @p cfg (see
 * `struct voice_config` above) before allocating, and reads the two FDSP
 * echo delays back through `STRM_VCE_GetFDSPEnvironmentalParams` for
 * `FDSP_DP_Create`. The failure arms (a sub-constructor returning NULL) are
 * unreachable from any fixture, since the only way one fails is a
 * `sysdep_malloc` the harness allocator cannot be made to fail.
 *
 * @param cfg The host's sixteen-byte configuration block.
 * @return The new context, or NULL if @p cfg is NULL.
 */
struct voice_ctx *voice_create(const struct voice_config *cfg);

/**
 * @brief Tear the voice service down: beep generator, detector, FIFO,
 * silence detector, datapump kernel (each if non-NULL) and the context
 * itself.
 */
void voice_delete(struct voice_ctx *v);

/**
 * @brief The voice service's own command dispatch, eleven opcodes wide.
 *
 * `VOICE_BEEP_COMMAND` and `VOICE_DTMF_COMMAND` are refused unless `mode` is
 * online or duplex. `arg` is read as up to three 32-bit words depending on
 * the opcode; `VOICE_RESET_DUPLEX_COMMAND` reads none.
 *
 * @param v   The voice context.
 * @param cmd One of the `VOICE_*_COMMAND` opcodes.
 * @param arg The opcode's argument words.
 * @return 0 if the command was acted on, 7 if it was refused or unknown.
 */
int voice_command(struct voice_ctx *v, int cmd, int *arg);

/**
 * @brief Run one block through the voice service: dispatch to the current
 * per-block handler, then run the cadence/tone detector over the transmit
 * float block and fold its status into the return value.
 *
 * `*countp` is in/out and changes meaning across the call: in, the block's
 * sample count for the handler; out, the handler's sample count plus
 * whatever bytes the detector appended. A pending `<DLE><ETX>` or
 * `<DLE><CAN>` from voice_dle_command() switches the context back online
 * before this call returns.
 *
 * @param v          The voice context.
 * @param rx_lin     Passed to the current handler.
 * @param rx_flt     Passed to the current handler and to the detector.
 * @param tx_flt     Passed to the current handler.
 * @param tx_lin     Passed to the current handler; the detector's own bytes
 *                   are appended after whatever the handler wrote.
 * @param hostcount  Passed to the current handler.
 * @param countp     In/out block sample count; see above.
 * @return One of the `VOICE_*` message codes (0-13), or whatever the
 *         handler or the detector status mapping produced.
 */
int voice_modem(struct voice_ctx *v, short *rx_lin, float *rx_flt,
		float *tx_flt, short *tx_lin, unsigned short *hostcount,
		unsigned short *countp);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_VOICE_H */
