/*
 * v22.h -- V.22 / V.22bis / Bell 212: the datapump's core-facing object.
 *
 * The modem core sees a `struct dp`; this module sees the block that begins
 * with one.  Same shape as `struct b103_dp` in b103.h, from the same author,
 * and `v22_delete` casts between them the same way.
 *
 * Reconstructed from dsplibs.o:
 *
 *   v22_create   .text 0x004fb0  300 bytes, LOCAL
 *   v22_delete   .text 0x0050e0   72 bytes, LOCAL
 *   v22_process  .text 0x005130  557 bytes, LOCAL
 *   dp_v22_init  .text 0x005360   72 bytes
 *   dp_v22_exit  .text 0x0053b0   70 bytes
 *
 * ---------------------------------------------------------------------------
 * THE LAYOUT IS COMPLETE, AND THE ARITHMETIC IS WHAT SAYS SO
 *
 * `v22_create` (0x4fb0) allocates 0x344 bytes, zeroes them, and stores five
 * things: the datapump id at +0x00, the modem at +0x04, the operations table
 * at +0x0c, the wrapper at BOTH +0x10 and +0x20, and the V22FP object at
 * +0x1c.  `v22_process` (0x5130) supplies the rest: it passes +0x14 to
 * `modem_get_bits` and `modem_put_bits` as their bit width, tests and writes
 * +0x18, and indexes two INT arrays with a scale of 4, at +0x24 and +0x1b4.
 *
 * The two arrays close the struct exactly.  0x1b4 + 100 * 4 = 0x344, which is
 * the allocation size -- so the second array is a hundred entries and there is
 * nothing after it.  That is not an assumption about how big a bit buffer
 * should be; it is the only length that makes the object's own malloc size
 * come out right, and `struct b103_dp` independently has two hundred-entry
 * int arrays in the same position for the same reason.
 *
 * The one difference from B103 is that its gap holds THREE ints -- `caller`,
 * `tx_bits_wanted` and `last_status` -- where this holds two, which is why its
 * arrays sit four bytes higher at +0x28 and +0x1b8 and its object is 840 bytes
 * to this one's 836.
 *
 * ---------------------------------------------------------------------------
 * WHAT `v22_create` SETTLES ABOUT THE CONFIGURATION IT BUILDS
 *
 * Read while modelling the layout, and recorded here because it corroborates
 * `v22fp.h`'s reading of `struct v22fp_cfg` from the other end:
 *
 *   cfg.mode = (caller == 0)      so mode 1 -- v22_answer, by F8529's table
 *                                 -- is the NOT-caller, and mode 0 is
 *                                 v22_originate.  The two agree.
 *   cfg.rate = 2 for id 212, 1 for id 22, 0 for anything else (id 122).
 *                                 v22fp.h has rate 0 giving 2400 and 1 and 2
 *                                 both giving 1200, so V.22bis gets 2400 and
 *                                 V.22 and Bell 212 get 1200.
 *   cfg.f08 = 60000, f0c = 0, f10 = 700, f14 = 0, f18 = 1
 *                                 exactly the constants v22fp.h names as
 *                                 coming from this caller.
 *
 * `dp_wrapper_create` is handed the object, `v22_process`, a fragment of 160
 * and a datapump rate of 8000 -- the same geometry B103 uses.
 */

#ifndef DSPLIB_V22_H
#define DSPLIB_V22_H

#include "dsplib/dp.h"

struct dp_wrapper;
struct v22fp;

/*
 * Entries in each bit buffer.  NOT a round number chosen for comfort: it is
 * what closes the object, 0x1b4 + 100 * 4 = 0x344.  `v22_process` also clamps
 * a received count to it -- `cmp $0x64` -- which is the same number reached a
 * second way.
 */
#define V22_BIT_BUFFER	100

/*
 * 836 bytes, of which the first 20 are the `struct dp` the modem core sees --
 * so a `struct dp *` from this module can be cast back to one of these, which
 * is what `v22_delete` does.
 */
struct v22_dp {
	struct dp dp;			/* +0x000 .. +0x013                */
	/*
	 * +0x14 is the bit width `modem_get_bits` and `modem_put_bits` are
	 * called with, and it is also the shift `v22_process` uses to build
	 * the mask `(1 << width) - 1` it applies to every received word.
	 */
	int bits_per_word;		/* +0x014                          */
	/*
	 * +0x18 is how many transmit bits to fetch per block, and it doubles
	 * as the "carry data" flag: zero takes the branch that transmits
	 * nothing.  `b103_dp` has the same field with the same double duty.
	 */
	int tx_bits_wanted;		/* +0x018                          */
	struct v22fp *fp;		/* +0x01c the modulation           */
	struct dp_wrapper *wrapper;	/* +0x020                          */
	/*
	 * The bit buffers.  Both are indexed as INTS by v22_process, and the
	 * second one's hundred entries are what make the object come out at
	 * exactly 0x344 bytes.  The transmit buffer is also read back as
	 * BYTES through the same address -- `modem_get_bits` fills it as
	 * bytes and the loop then widens them in place, backwards, so one
	 * buffer serves both widths, exactly as `b103_process` does.
	 */
	int tx_bits[V22_BIT_BUFFER];	/* +0x024                          */
	int rx_bits[V22_BIT_BUFFER];	/* +0x1b4                          */
};

/* The datapump runs at 8 kHz in 160-sample fragments, as B103 does. */
#define V22_DP_SRATE	8000
#define V22_DP_FRAG	160

/*
 * The three ids `dp_v22_init` registers this datapump under -- 122, 22 and
 * 212, in that order -- and the two `v22_create` tests for when it picks a
 * rate.  slmodemd's `modem_defs.h` spells the first `DP_V22BIS = 122`.
 */
#define V22_DP_ID_V22BIS	122
#define V22_DP_ID_V22		22
#define V22_DP_ID_BELL212	212

/*
 * The number of data words fetched, and handed to the modulation, per block.
 * A literal 12 in the object at three sites: the "carry no data" arm of
 * `v22_process` passes it straight through without fetching anything, and
 * both connect arms install it as `tx_bits_wanted`.  Twelve words in a 20 ms
 * block is 600 baud, which is V.22's symbol rate.
 */
#define V22_WORDS_PER_BLOCK	12

/*
 * Bits per data word, and the line rate reported to the core, for each of the
 * two connect codes.  Both pairs are literals in `v22_process`.
 */
#define V22_BITS_PER_WORD_1200	2
#define V22_BITS_PER_WORD_2400	4
#define V22_LINE_RATE_1200	1200
#define V22_LINE_RATE_2400	2400

/*
 * `V22FP_modem`'s status byte for the 1200 connect.  `v22conn.h` names 3 from
 * the author's own "V22_MSG_CONNECT_2400 In NODE_2400C" strings; 4 has no
 * string, and what names it is `v22_process`'s own arm -- two bits per word
 * and 1200 bit/s each way, beside the 2400 arm for 3.  That is evidence class
 * 2, a caller that types it, and not usage inference.  Finding F8538.
 */
#define V22_MSG_CONNECT_1200	4

/*
 * Register and deregister the three ids above.  The only two symbols this
 * module exports: `v22_create`, `v22_delete` and `v22_process` are all
 * file-static in the object and are reached through what these register.
 */
int dp_v22_init(void);
void dp_v22_exit(void);

#endif /* DSPLIB_V22_H */
