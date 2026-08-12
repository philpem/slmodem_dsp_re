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
 * one translation unit each (finding 1410).
 *
 * THE TWO RATES ARE NOT THE TWO RATES.  `rate` holds 8000 or 9600 and is the
 * rate the LINE runs at, which is what CID_MTD_detect's coefficients are
 * designed for.  The demodulator does not run at either: 8000 selects
 * AUTOCOR_COEF_7200 and six samples per bit, which is 7200 Hz at 1200 baud,
 * and 9600 selects eight, which is 9600 Hz.  The 8000 path is fed resampled
 * (finding 1509).
 *
 * WHAT THE OBJECT IS.  `create_cid` allocates 0x160 bytes and sets `rate` to
 * 8000, +0x28 to 2 and +0x2c to 9.  Only the span this batch reads is named
 * below; the rest is reset_cid's and cid_progress's, and is padding until a
 * later batch writes them.  `cid_modem` passes `cid + 0x90` to
 * CID_FSD_demodulate as the bit buffer, so the demodulated bits land inside
 * the object -- but the function itself takes the buffer as an argument and
 * does not know that.
 */

#ifndef DSPLIB_CID_H
#define DSPLIB_CID_H

struct cid {
	/*
	 * +0x00c is a `struct fpm_mrf`, the 9:10 resampler that makes the
	 * 8000 Hz line into the 7200 Hz the demodulator wants -- so this pad
	 * ends at a real boundary, not a convenient one.  Findings 1509,
	 * 1510; a later batch writing `reset_cid` will name the rest.
	 */
	unsigned char pad_000[40];	/* +0x000 reset_cid's territory     */
	short f028;			/* +0x028 create_cid puts 2 here    */
	short rate;			/* +0x02a 8000 or 9600, the LINE    */
	short f02c;			/* +0x02c create_cid puts 9 here    */
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
	unsigned char pad_074[4];	/* +0x074 not read by either leaf   */
	short run;			/* +0x078 samples agreeing          */
	short opp;			/* +0x07a samples disagreeing       */
	short dead;			/* +0x07c samples in the dead zone  */
	short mtd1_state[2];		/* +0x07e MTD_COEF_1's biquad       */
	short mtd2_state[2];		/* +0x082 MTD_COEF_2's biquad       */
	unsigned char pad_086[218];	/* +0x086 to 0x160, the allocation  */
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

#endif /* DSPLIB_CID_H */
