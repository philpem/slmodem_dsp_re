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
	 * +0x000  The sixteen-byte host block, copied in whole and handed on
	 * to `beepgen_create`; that call is what types it (beepgen.h).
	 */
	struct beepgen_config	cfg;

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
	void			*silence;	/* +0x028 silence_create   */
	unsigned char		pad_002c[0x034 - 0x02c];
	int			*dp;		/* +0x034 FDSP_DP_Create,
						 *        FDSP_DP_Run's
						 *        first argument   */

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
	 * +0x74c  Output format selector.  Every reconstructed use is the
	 * same test, `(unsigned)(out_format - 1) <= 1`: 1 and 2 send 16-bit
	 * linear to `tx_lin`, anything else sends float.  `voice_create`
	 * starts it at 0.  What 1 and 2 mean apart from each other is not
	 * established.
	 */
	int			out_format;

	unsigned char		pad_0750[0x758 - 0x750];
	union voice_rate_bits	rate_bits;	/* +0x758                  */

	/*
	 * +0x75c  Latches the "not enough data" report so `voice_tx` makes it
	 * once: set when the report is made, cleared on the first block that
	 * has enough.  The report itself is suppressed after <DLE><ETX>.
	 */
	short			underrun;

	short			short_075e;	/* +0x75e create writes 0x3f */
	short			short_0760;	/* +0x760 create writes 0x3f */

	/*
	 * +0x762  The word `voice_set_online` hands to `detector_set_enable`;
	 * that callee is what types it.  `voice_create` starts it at 0x3f.
	 * `voice_set_duplex` passes 0x24 instead and does not read this.
	 */
	unsigned short		detector_enable;

	short			short_0764;	/* +0x764 create writes 0    */
	unsigned char		pad_0766[0x7dc - 0x766];
};

/*
 * The DLE byte `voice_tx` un-escapes on.  Doubled it is one literal DLE;
 * followed by anything else it introduces a command byte, which goes to
 * `voice_dle_command` and is not copied through.
 */
#define VOICE_DLE	0x10

/*
 * Map a DLE event code to a status code: 1 -> 10, 2 -> 11, 4 -> 12, and
 * ANY other code answers the first argument back unchanged -- the caller's
 * running status, passed in so the non-event is a no-op.
 */
int _handle_status(int status, int code);

/*
 * The three per-block handlers, and the two setters that install two of them.
 * See voicedp.c.
 */
int voice_online(struct voice_ctx *v, short *rx_lin, float *rx_flt,
		 float *tx_flt, short *tx_lin, unsigned short *hostcount,
		 unsigned short *countp);
int voice_tx(struct voice_ctx *v, short *rx_lin, float *rx_flt,
	     float *tx_flt, short *tx_lin, unsigned short *hostcount,
	     unsigned short *countp);
int voice_duplex(struct voice_ctx *v, short *rx_lin, float *rx_flt,
		 float *tx_flt, short *tx_lin, unsigned short *hostcount,
		 unsigned short *countp);

void voice_set_online(struct voice_ctx *v);
void voice_set_duplex(struct voice_ctx *v);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_VOICE_H */
