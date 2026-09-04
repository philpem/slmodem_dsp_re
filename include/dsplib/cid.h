/*
 * cid.h -- the Caller ID receiver's object, and the two DSP leaves that read
 * it.
 *
 * Caller ID arrives between the first and second rings in one of two forms:
 * as DTMF digits, which is `dtmf_rx.h`'s receiver, or as a 1200 baud FSK
 * message, which is this one.  `cid_modem` drives both off the same object
 * and this header covers the FSK half's two bottom layers:
 *
 *   Cidfsd.c   CID_FSD_demodulate   .text 0x092280   1049 bytes
 *   Cidmtd.c   CID_MTD_detect       .text 0x0926a0    259 bytes
 *
 * one translation unit each (finding F1410).  `Rxcid.c` -- reset_cid,
 * create_cid, pack_next_bit and cid_modem -- is src/service/rxcid.c, and it
 * is what named everything below +0x028 and everything from +0x086 to +0x152
 * (finding F8710).
 *
 * THE TWO RATES ARE NOT THE TWO RATES.  `rate` holds 8000 or 9600 and is the
 * rate the LINE runs at, which is what CID_MTD_detect's coefficients are
 * designed for.  The demodulator does not run at either: 8000 selects
 * AUTOCOR_COEF_7200 and six samples per bit, which is 7200 Hz at 1200 baud,
 * and 9600 selects eight, which is 9600 Hz.  The 8000 path is fed resampled
 * (finding F1509).
 *
 * WHAT THE OBJECT IS.  `create_cid` allocates 0x160 bytes and sets `rate` to
 * 8000, +0x28 to 2 and +0x2c to 9.  Every byte of it is now modelled as a
 * field; the ones still spelled `short_NNN` are cleared by reset_cid and read
 * by nothing this tree has written.  `cid_modem` passes `cid + 0x90` to
 * CID_FSD_demodulate as the bit buffer, so the demodulated bits land inside
 * the object -- but the function itself takes the buffer as an argument and
 * does not know that.
 */

#ifndef DSPLIB_CID_H
#define DSPLIB_CID_H

#include "dsplib/fpm_mrf.h"

struct cid {
	/*
	 * reset_cid's and cid_modem's territory.  `gain` is the AGC
	 * multiplier cid_modem adapts and applies to the whole block; `dc` is
	 * the block-mean estimate it subtracts, updated as
	 * 0.8 * this block's mean + 0.2 * the last value.  Both are usage
	 * inference from cid_modem and nothing else -- neither appears in a
	 * format string.  Finding F8710.
	 */
	int gain;			/* +0x000 cid_modem's AGC multiplier*/
	short short_004;		/* +0x004 cleared by reset_cid only */
	short dc;			/* +0x006 running DC estimate       */
	short short_008;		/* +0x008 cleared by reset_cid only */
	/*
	 * +0x00a was `pad_00a` (a bare `short`) -- REMOVED (finding
	 * F10145): already correctly described as alignment, `short_008`
	 * ending at +0x00a leaves exactly 2 bytes ahead of `mrf`, whose
	 * first member `struct fpm_mrf_cfg cfg` holds a pointer and needs
	 * 4-byte alignment.  `dis.py` over `cid_modem`/`create_cid`/
	 * `reset_cid`/`pack_next_bit` finds no access to offset 0x00a/0x00b.
	 */
	/*
	 * The 9:10 resampler that makes the 8000 Hz line into the 7200 Hz the
	 * demodulator wants; reset_cid configures it from V23_MRF_FILT, which
	 * is Rxcid.c's own static.  Findings F1509, F1510.  Naming it also
	 * exposes the pointer at +0x024, which is `mrf.history` and NOT a
	 * field of `struct cid`: create_cid nulls it and reset_cid passes
	 * `mrf.history == NULL` as FPM_MRF_init's `fresh`, which is what
	 * stops a second reset leaking the buffer.
	 */
	struct fpm_mrf mrf;		/* +0x00c 9:10, 8000 Hz -> 7200 Hz  */
	/*
	 * NAMED FROM THE AUTHOR'S OWN WORDS, evidence rule 1: `create_cid`
	 * prints "FSK CID  Setting  Fs = %d   Threshold = %d !" with this
	 * field's seed as the second value (.rodata.str1.4 + 0x116a8).  It
	 * doubles as `pack_next_bit`'s channel-seizure run length, which is
	 * the object's own reuse and not two fields.  Finding F8710.
	 */
	short threshold;		/* +0x028 create_cid puts 2 here    */
	short rate;			/* +0x02a 8000 or 9600, the LINE    */
	short f02c;			/* +0x02c create_cid puts 9 here;
					 *        cid_modem's confidence step*/
	short lpf_idx;			/* +0x02e write index, 0..16        */
	short lpf_hist[17];		/* +0x030 fix_LPF's circular buffer */
	short ac_idx;			/* +0x052 write index, 0..4         */
	short ac_hist[5];		/* +0x054 the delay line            */
	short last_bit;			/* +0x05e the bit last emitted      */
	short high_count;		/* +0x060 samples in `high_sum`     */
	short low_count;		/* +0x062 samples in `low_sum`      */
	int thresh;			/* +0x064 the slicing level         */
	int high_level;			/* +0x068 mean of the first 128     */
	int low_sum;			/* +0x06c running negative total    */
	int high_sum;			/* +0x070 first 128 positives       */
	short short_074;		/* +0x074 cleared by reset_cid only */
	short short_076;		/* +0x076 cleared by reset_cid only */
	short run;			/* +0x078 samples agreeing          */
	short opp;			/* +0x07a samples disagreeing       */
	short dead;			/* +0x07c samples in the dead zone  */
	short mtd1_state[2];		/* +0x07e MTD_COEF_1's biquad       */
	short mtd2_state[2];		/* +0x082 MTD_COEF_2's biquad       */
	short short_086;		/* +0x086 cleared by reset_cid only */
	short short_088;		/* +0x088 cleared by reset_cid only */
	short short_08a;		/* +0x08a cleared by reset_cid only */
	short short_08c;		/* +0x08c cleared by reset_cid only */
	/*
	 * The mark-tone confidence.  cid_modem adds `f02c` to it for every
	 * block CID_MTD_detect answers 0 for and zeroes it otherwise, then
	 * compares it against two thresholds derived from the block length --
	 * about 26.7 ms of tone to start resampling and about 40 ms to start
	 * demodulating.  Finding F8712.
	 */
	short mark_conf;		/* +0x08e mark-tone confidence      */
	/*
	 * Where cid_modem has CID_FSD_demodulate put the demodulated bits.
	 * The length is the space to `data`, not a bound the object checks:
	 * CID_FSD_demodulate takes the buffer as an argument and writes one
	 * short per bit with no limit, so more than 216 samples reaching the
	 * demodulator would run past it.  cid_modem's own 206-short frame
	 * buffer is the tighter limit and fails first.  Deviation D973.
	 */
	short bits[36];			/* +0x090 cid_modem's bit buffer    */
	/*
	 * `pack_next_bit`'s territory (Rxcid.c): the async framer that turns
	 * the demodulated bit stream into message bytes.  The six framer names
	 * are usage inference from that one function -- see src/service/rxcid.c
	 * for the derivation.  The LENGTH of `data` is reset_cid's: it clears
	 * exactly 120 bytes from +0x0d8 and then clears +0x150 and +0x152 as
	 * two separate shorts alongside +0x154 and +0x156.
	 */
	unsigned char data[120];	/* +0x0d8 assembled message bytes   */
	short short_150;		/* +0x150 cleared by reset_cid only */
	short short_152;		/* +0x152 cleared by reset_cid only */
	short mark_bal;			/* +0x154 mark/space balance while
					 *        hunting carrier (state 0) */
	short pack_state;		/* +0x156 0 hunt, 3 wait-start-run,
					 *        1 wait start bit, 2 shift */
	short pack_acc;			/* +0x158 the byte being assembled  */
	short pack_pos;			/* +0x15a bit position / zero-run   */
	short pack_len;			/* +0x15c bytes stored into `data`  */
	/*
	 * +0x15e was `pad_15e[2]`, the struct's LAST member -- REMOVED
	 * (finding F10145).  Trailing padding: `pack_len` ends at +0x15e
	 * and the struct's own alignment (forced to 4 by its several `int`
	 * members and `mrf`'s pointer) rounds `sizeof` up to +0x160 on its
	 * own.  `src/service/cid_mtd.c`'s existing `cid_size_check[sizeof
	 * (struct cid) == 0x160 ? 1 : -1]` is a hard compile-time proof,
	 * and 0x160 is also `create_cid`'s literal allocation size, not
	 * adjacency alone.  `dis.py` over `cid_modem`/`create_cid`/
	 * `reset_cid`/`pack_next_bit` finds no access to 0x15e/0x15f.
	 */
};

/* The line rate, and the only value either leaf tests for. */
#define CID_RATE_9600	9600

/* What create_cid puts there.  8000 is the rate by being the other one. */
#define CID_RATE_8000	8000

/*
 * Demodulate `count` line samples into `bits`, one short per bit, and return
 * how many bits that was.  A delay-line discriminator followed by a 17-tap
 * low pass, then a slicer with its own adaptive threshold; every piece of
 * state it needs is in `cid` and persists across calls.
 *
 * There is no bound on `bits`: the caller sizes it from `count`.
 */
short CID_FSD_demodulate(const short *samples, short *bits, short count,
			 struct cid *cid);

/*
 * The mark-tone detector, and it answers backwards: **0 means the tone is
 * there**, 1 means it is not.  `cid_modem` reads it that way -- a zero adds
 * cid->f02c to its confidence counter and anything else clears it.
 *
 * Two cascaded notches take out 1200 Hz and 1300 Hz; what is left is compared
 * with the input's own energy.  See src/service/cid_mtd.c.
 */
short CID_MTD_detect(const short *samples, short count, struct cid *cid);

/*
 * Feed the framer one demodulated bit.  Argument order is the object's:
 * the BIT first, the object second.
 *
 * State 0 hunts carrier on a mark/space balance counter; more than 15 net
 * marks arms state 3, which waits for threshold + 1 consecutive spaces...
 * except that the object then backdates the run to threshold and enters state 2
 * directly, so the byte collector starts at bit position threshold rather than
 * 0.  State 1 waits for a start bit (a 0); state 2 shifts eight bits
 * LSB-first into pack_acc and appends the byte to `data`.  Nothing bounds
 * pack_len against sizeof(data) -- that is the object's own shape, and
 * cid_modem is what must keep the message short.
 *
 * `threshold` doubles as the framer's channel-seizure run length here and as
 * the slicer threshold `cid_threshold` stores; create_cid seeds it with 2.
 * The name is the author's -- see the field's comment above.
 */
void pack_next_bit(short bit, struct cid *cid);

/*
 * Put the receiver back to the state a new one is in, and configure the 9:10
 * resampler from Rxcid.c's own static filter.  `rate`, `threshold` and `f02c` are
 * the caller's and survive; everything else is cleared.
 *
 * FPM_MRF_init is asked to allocate only when `mrf.history` is still NULL, so
 * calling this repeatedly reuses the buffer rather than leaking it.
 */
void reset_cid(struct cid *cid);

/*
 * Construct one.  NULL allocates 0x160 bytes; anything else is the caller's
 * storage.  Returns the object either way.  Seeds `rate` with 8000, `threshold`
 * with 2 and `f02c` with 9, all AFTER the reset.
 */
struct cid *create_cid(struct cid *cid);

/*
 * One block of line samples through the FSK receiver, and the top of this
 * file's stack.  Four stages, each gated on how much mark tone has been seen:
 *
 *   1. copy to a local buffer and subtract the tracked DC.
 *   2. below ~40 ms of confidence, ask CID_MTD_detect whether 1200 Hz is
 *      there; a yes adds `f02c` to `mark_conf`, a no clears it.
 *   3. above ~26.7 ms, resample 8000 -> 7200 (9600 is already right) and,
 *      while the confidence is still between the two thresholds, re-adapt
 *      `gain` from the block's mean absolute value; then apply `gain`.
 *   4. above ~40 ms, demodulate into `bits` and push each bit through
 *      pack_next_bit.  Once `data[1] + 3` bytes have arrived, checksum them.
 *
 * Returns 1 while still hunting, 2 while collecting, 3 for a message whose
 * checksum agrees, and -1 for one that does not.
 *
 * `count` MUST NOT exceed 206: the local sample buffer is that long in the
 * object and nothing checks.  Deviation D973.
 */
int cid_modem(const short *samples, unsigned short count, struct cid *cid);

/*
 * The demodulator's coefficients, all three GLOBAL in the object and all
 * three in `.data` rather than `.rodata` -- so `const` here is this tree's
 * reading of the intent and not the original's storage class.
 */
extern const short fix_LPF[17];
extern const short AUTOCOR_COEF_7200[5];
extern const short AUTOCOR_COEF_9600[5];

/*
 * CID_MTD_detect's four are file-static in the object, so they cannot be
 * named from a test.  This is the same accessor arrangement `fpm_div.c` uses
 * for FPM_div_table: `which` is 1 or 2, `rate` is 8000 or 9600, and anything
 * else returns NULL.
 */
const short *CID_MTD_coeff(int which, int rate);

/*
 * The TOP of the stack: slmodemd's own three entry points, defined in
 * src/service/cid.c and declared by the host verbatim
 * (ref/slmodemd/modem.c:87-89) -- the signatures are quoted from there.
 * `in` is `void *` in the host's extern and is a buffer of line samples
 * (shorts); `CID_process` returns 0 while a message is still arriving, 1
 * once one has been delivered to the TTY, -1 when the receiver gives up.
 */
void *CID_create(void *m, unsigned rate, unsigned cid_val);
void CID_delete(void *cid);
int CID_process(void *cid, void *in, int count);

#endif /* DSPLIB_CID_H */
