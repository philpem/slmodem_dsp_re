/* One field home for the measured TX-control owner and its flat view.
 * Comments use complete v34_object offsets. Intentionally no include guard. */

#include "dsplib/v34rx.h"

	struct v34_queue txq;				/* +0x221c */
	int txq_ring_tail[V34_TXQ_RING - 1];		/* to +0x25c0 */
	short seg_symcount;					/* +0x25c0 */
	/*
	 * The transmit-side flag word `v34hstx1.cpp` and `v34shell.c` both
	 * test directly, bit by bit, rather than through a name for the
	 * whole field -- eight bits with an established meaning and two with
	 * none yet:
	 *
	 *   bit 0   V34_TXFLAG_CALLER       set for the calling station's
	 *           scrambler generator, clear for the answering one --
	 *           `txmitdibit`/`txmitquadbit` pin this one.
	 *   bit 2   V34_EC_FROZEN (v34rx.h) both echo cancellers frozen; the
	 *           echo-adapt start/stop reports clear and set it.
	 *   bit 4   PROG_TXBIT_DATA (v34pcmmain.cpp) set by `modulatevector`
	 *           once the training-to-data symbol count (`train_symcount`
	 *           against `span`) expires; gates the shell mapper's
	 *           non-idle input.
	 *   bit 8   set once the segment counter passes a threshold during echo
	 *           adaptation (`v34hstx1.cpp`); no established name.
	 *   bit 9   V34_EC_FEED (v34rx.h) gates the echo feed.
	 *   bit 10  set on one baud-ratio branch of the echo-adapt ladder,
	 *           alongside clearing bit 15; no established name.
	 *   bit 13  V34_TXFLAG_SEG4A        raised entering TRNSEG4A/SSEG.
	 *   bit 14  V34_TXFLAG_NLENCODE     `initdigital`'s own comment names
	 *           it: "Bit 13 of the same word is modulatevector's
	 *           non-linear encoder" (bit 13 of the INFO word, stored here
	 *           at bit 14); selects `V34nlencoder` over a plain point copy.
	 *   bit 15  V34_TXFLAG_PPSEG        raised for PPSEG, read as the
	 *           field's sign.
	 *
	 * Bits 8 and 10 are usage inference only -- a single write site each,
	 * no format string, no reader that types them -- and are left as bare
	 * hex rather than guessed into a name.
	 */
	short tx_flags;					/* +0x25c2 */
	unsigned char unmapped_25c4[0x25c6 - 0x25c4];
	/*
	 * The differentially-encoded quadrant, carried from one symbol to the
	 * next: prev_quadrant is the previous one and cur_quadrant the current.  In the
	 * dibit case the two end up equal; in the quadbit case cur_quadrant is set
	 * first and prev_quadrant only catches up at the end, because the second
	 * dibit indexes off the first one's quadrant.
	 */
	short prev_quadrant;					/* +0x25c6 */
	short cur_quadrant;					/* +0x25c8 */
	unsigned char unmapped_25ca[0x25cc - 0x25ca];
	/* The transmit scrambler's shift register. */
	int tx_scr_sr;					/* +0x25cc */
	/*
	 * The point being transmitted: two shorts, real then imaginary.
	 * Every arm that sends a constellation point writes both halves with
	 * one 32-bit store (txmitdibit(), txmitquadbit(), modulatevector()),
	 * which is why `vect4` and `vect16` hold ints at all -- but the
	 * halves are also written and read separately as shorts: the silent
	 * symbol zeroes each on its own, and `txmit` reassembles them as
	 * `(im << 16) | (unsigned short)re` before handing the result to
	 * `V34ModulatorProcess`. That reader is the only thing in the object
	 * that reads this pair, which is what settles it as the point going
	 * out rather than one coming in.
	 *
	 * The union spells the aliasing out rather than casting a pointer,
	 * which -O2 is entitled to reorder (the same remedy as
	 * `v34_receiver::energy`). `c` is an array rather than two named
	 * fields because `V34nlencoder` is handed `txpoint.c` and writes
	 * `out[0]` and `out[1]` through it -- two named shorts would make
	 * that second store its own out-of-bounds defect.
	 *
	 * A 4-byte struct assignment would emit the same single `movl` and
	 * is an equally consistent reading of the original; nothing in the
	 * object separates the two, so this does not claim to have settled
	 * which the author wrote (finding F5303).
	 */
	union {
		int word;				/* +0x25d0 both at once */
		short c[2];				/* [0] real, [1] imag  */
	} txpoint;
	short tx_scale;					/* +0x25d4 tx scale  */
	short short_25d6;				/* +0x25d6 */
	short short_25d8;				/* +0x25d8 */
	short short_25da;				/* +0x25da */
	/*
	 * The transmit power reduction in whole dB, which `settxlevel`
	 * assembles from the far end's MP message and then applies to
	 * `tx_scale` above -- so this is the request and that is the result.
	 * Its own diagnostic names it: "power reduction requested by remote
	 * modem is %d dB". It can be negative: with a V.90 receiver running,
	 * a `GetVPcmMinimalTxPowerReduction` below zero is added to the
	 * request rather than compared with it, and the result drives
	 * `settxlevel`'s other loop -- the one that raises the scale.
	 */
	short tx_pwr_reduction;					/* +0x25dc */
