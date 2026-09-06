/*
 * v21cfg.h -- Class 1 fax, V.21 channel: the nine tables `V21RX_create`
 *             (0x098e70) references directly, and the configuration type it
 *             copies onto its stack -- plus `V21TX_create`'s own default,
 *             `V21TX_CFG`, added by F9500 for the same reason.
 *
 *   AGCv21_CFG           .rodata 0x00a0e4    24   struct fpm_agc_cfg
 *   AGC_DEF_ALPHA_v21    .rodata 0x00a100     4   short[2]
 *   AGC_DEF_BETA_v21     .rodata 0x00a0fc     4   short[2]
 *   V21RX_IIR_LPF        .rodata 0x00a088    30   short[15]
 *   V21RX_CHAN2_INTRP    .rodata 0x00a0a6    30   short[15]
 *   V21RX_CHAN1_INTRP    .rodata 0x00a0c4    30   short[15]
 *   V21_MRF_FILT         .rodata 0x00c000   720   short[360]
 *   V21_CHAN1_MTD_COEFF  .data   0x007a74    20   short[10]
 *   V21RX_CFG            .data   0x007ab4    24   struct v21rx_cfg
 *
 * `V21_CHAN2_MTD_COEFF` is the tenth member of this bank and is NOT declared
 * here: all three fax receiver constructors reference it, so it lives in
 * `faxcfg.h` beside the tables they share.  Finding F9140 wrote it.
 *
 * EVERY ELEMENT COUNT IS WRITTEN DOWN TWICE IN THE OBJECT, which is F9140's
 * method and the reason a byte count alone is not enough.  `V21RX_create`
 * copies each library built-in configuration onto its stack and patches the
 * tables AND the lengths in, so each count appears once as `st_size` and once
 * as the length field stored beside the pointer.  Both readings agree for all
 * of them:
 *
 *   table                 st_size   the count V21RX_create writes
 *   V21RX_CHAN1/2_INTRP     30      `fsd.fir_taps` = 0x0f = 15   (0x098ffc)
 *   V21RX_IIR_LPF           30      `fsd.iir_len`  = 3, x5/section (0x09900d)
 *   V21_MRF_FILT           720      `mrf.taps`     = 0x168 = 360 (0x098f50)
 *   V21_CHAN1_MTD_COEFF     20      `mtd.tones`    = 2, x5/section (0x099060)
 *   AGC_DEF_ALPHA/BETA_v21   4      see the DC-gain note below
 *
 * THE TWO AGC COEFFICIENT ARRAYS ARE GLOBAL AND UNIQUELY NAMED, WHICH IS THE
 * OPPOSITE OF EVERY OTHER MODULATION'S.  F9144 measured `AGC_DEF_ALPHA` and
 * `AGC_DEF_BETA` as defined SIX times each in the blob, five of them local,
 * so F9058 denies them a `ref_` alias and V.17's, V.27's and V.29's can only
 * be compared through their consumer.  V.21's are spelled `AGC_DEF_ALPHA_v21`
 * and `AGC_DEF_BETA_v21`, appear exactly once, and are `GLOBAL` -- so they DO
 * get a `ref_` alias, are compared by name in `t_v21cfg.c`, and must be
 * written as globals rather than as the file statics the family pattern would
 * suggest.  Reading the pattern instead of the symbol table would have got
 * this wrong in the safe-looking direction.  F9147 flagged it; this header is
 * where it is acted on.
 *
 * THE COUNT OF 2 COMES FROM THE VALUES, not from `st_size` alone.  A
 * first-order smoother `y += alpha*y + beta*x` has unity DC gain when
 * alpha + beta == 32768 in Q15, and BOTH elements satisfy it exactly:
 *
 *      [0]   16384 + 16384 = 32768
 *      [1]   32604 +   164 = 32768
 *
 * Two pairs, both exact, so the array is two elements of `short` and not one
 * element of `int`.  `b103_agc_cfg.c` records that three of the object's six
 * copies get element 1 wrong (32604 + 1638 = 34242, a DC gain of 1.045, D6);
 * V.21's is one of the two that are RIGHT, and it is byte-identical to the
 * `.data:0x77bc/0x77b8` local pair rather than to Bell 103's own.
 */

#ifndef DSPLIB_V21CFG_H
#define DSPLIB_V21CFG_H

struct fpm_agc_cfg;

/*
 * The V.21 receiver's modem configuration table, and the fourth type in
 * `faxcfg.h`'s family rather than a reuse of `struct v29rx_cfg`.
 *
 * It is 24 bytes, exactly `V29RX_CFG`'s size, and its dwords read [1, 300,
 * 60000, 0, 0, 0] against V.29's [1, 9600, 60000, 0, 0, 0] -- so on size and
 * on values alone it looks like the same type with a different bit rate, and
 * `faxcfg.h`'s own argument for three types (their sizes are 40, 28 and 24)
 * does not separate this case.
 *
 * WHAT SEPARATES IT IS A FORCED ENCODING.  `V21RX_create` tests the field at
 * +0x00 twice, with `cmpw $0x0,(%esi)` at 0x098fc7 and again at 0x099043, and
 * a 16-bit compare is not something the compiler may narrow an `int` into --
 * an `int` holding 0x10000 is non-zero while its low half is not, so `cmpl`
 * or `testl` is forced for an `int` and `cmpw` is forced for a `short`.
 * `V29RX_CFG`'s +0x00 is `int_0000`.  Two different widths at the same offset
 * is two types, on CLAUDE.md's "act on what the compiler was FORCED to
 * encode".  Finding F9351.
 *
 * `chan2` IS NAMED FROM WHAT IT SELECTS and from nothing else.  Both tests
 * pick channel 2's tables when it is non-zero and channel 1's when it is
 * zero: `V21RX_CHAN2_INTRP` against `V21RX_CHAN1_INTRP` for the discriminator
 * FIR at 0x098fd8/0x0990fb, and `V21_CHAN2_MTD_COEFF` against
 * `V21_CHAN1_MTD_COEFF` for the tone detector at 0x099026/0x099049.  Both
 * arms also set `fsd.delay`, to 5 for channel 1 and 3 for channel 2.  The
 * table ships 1, so the built-in default is the ANSWERING side -- V.21
 * channel 2, 1650/1850 Hz -- which is what a fax receiver wants.
 *
 * THE SIGN IS NOT ESTABLISHED, exactly as for `faxcfg.h`'s `bit_rate`: every
 * comparison the object makes on `chan2` is against zero, which carries no
 * sign, and the only value it holds is 1.
 *
 * CONFIRMED-EXHAUSTED, WAVE 7: `short_0002`/`short_0006`/`int_0008`/
 * `int_000c`/`int_0010` were re-checked against this struct's own three
 * `faxcfg.h` siblings (`v17rx_cfg`/`v27rx_cfg`/`v29rx_cfg`), which carry the
 * identical shape at the identical offsets and stay unnamed there too for
 * the same reason (`int_0008` is 60000 in all four and read back by
 * nothing; the rest are zero and untouched) -- a negative twin-class
 * result, not an unexamined one.
 */
struct v21rx_cfg {
	short		chan2;		/* +0x00  1: use channel 2's tables  */
	short		short_0002;	/* +0x02  0                          */
	short		bit_rate;	/* +0x04  300                        */
	short		short_0006;	/* +0x06  0                          */
	int		int_0008;	/* +0x08  60000, as in all four      */
	int		int_000c;	/* +0x0c  0                          */
	int		int_0010;	/* +0x10  0                          */
	void	       *aux;		/* +0x14  0; V21RX_create forwards
					 *        it to BOTH `fpm_mrf_cfg`'s
					 *        `aux` and the fsd config's
					 *        matching slot            */
};

/*
 * `R` in the object -- global and in `.rodata` -- for everything but
 * `V21_CHAN1_MTD_COEFF` and `V21RX_CFG`, which are `D`.  The storage classes
 * are the object's own `nm` types and are asserted by `t_v21cfg.c`.
 */
extern const struct fpm_agc_cfg AGCv21_CFG;
extern const short AGC_DEF_ALPHA_v21[2];
extern const short AGC_DEF_BETA_v21[2];

extern const short V21RX_IIR_LPF[15];
extern const short V21RX_CHAN2_INTRP[15];
extern const short V21RX_CHAN1_INTRP[15];
extern const short V21_MRF_FILT[360];

extern short V21_CHAN1_MTD_COEFF[10];
extern struct v21rx_cfg V21RX_CFG;

/*
 * `V21TX_CFG`, `D` at .data 0x07af8, 28 bytes -- `V21TX_create`'s own
 * default, copied onto the transmit handle's first 28 bytes exactly as
 * `V21RX_CFG` is onto the receiver's first 24.  See v21fax.h for what
 * `V21TX_create` (0x0992f0) establishes about the handle; NOTHING
 * reconstructed READS any field of this table back out of the handle, so it
 * is spelled from the object's bytes and forced widths alone, not from a
 * consumer.
 *
 * NOT `struct v21rx_cfg`'s layout, and F9356 already showed why: the
 * receiver's +0x00 is tested `cmpw` (16-bit forced) while this table is 28
 * bytes to the receiver's 24, with a different value at +0x0c than the
 * receiver's +0x08.  `V21TX_create` never tests any field of its own copy
 * with a width-forcing compare (the six-plus-one dword copy at 0x099328 is a
 * bulk `mov` sequence, width-blind), so the SHORT/INT split below is the
 * receiver's own precedent, not something this table's own instructions
 * force -- keep that distinction in mind before trusting it further than
 * that.
 *
 * The bytes: `01 00 2c 01 00 00 00 00 60 ea 00 00 80 0c 00 00` then four
 * zero dwords.  Read as short,short,short,short,int,int,int,int,int:
 * {1, 300, 0, 0, 60000, 3200, 0, 0, 0}.  300 sits where V.21's own bit rate
 * sits in every sibling table in this file; 60000 is the same literal
 * `V21RX_CFG.int_0008` and every one of `faxcfg.h`'s siblings carry at their
 * own +0x08. 3200 has no parallel elsewhere in this file and is not named.
 *
 * CONFIRMED-EXHAUSTED, WAVE 7: every field re-checked against `V21TX_
 * create`'s own reads (none past a bulk copy -- see the header comment) and
 * against the sibling `v17tx_cfg`/`v27tx_cfg`/`v29tx_cfg` (F10183, this
 * wave) at the same offsets; none of those siblings' own established
 * fields (`fifo_size_factor`/`scale_mul`/`flags`, all further along the
 * struct) land inside this table's own +0x00..+0x18, since V.21's
 * transmit config is a different, shorter shape (write-only, no FIFO
 * size factor of its own -- `V21TX_create`'s FIFO is a fixed 6 elements,
 * not derived from any field here). `short_0000`'s own three-arm behaviour
 * is already fully derived above; it is not `chan2` under another name,
 * since arms 0 and 1 are observably IDENTICAL (both take the same
 * unconditionally-overwritten tone pair) where `chan2` genuinely selects
 * between two different table sets.
 */
struct v21tx_cfg {
	short	short_0000;	/* +0x00  1                                 */
	short	bit_rate;	/* +0x02  300, V.21's only rate              */
	short	short_0004;	/* +0x04  0                                  */
	short	short_0006;	/* +0x06  0                                  */
	int	int_0008;	/* +0x08  60000, as in every sibling table   */
	int	int_000c;	/* +0x0c  3200                               */
	int	int_0010;	/* +0x10  0                                  */
	int	int_0014;	/* +0x14  0                                  */
	int	int_0018;	/* +0x18  0                                  */
};

extern struct v21tx_cfg V21TX_CFG;

/*
 * The two tone-detector banks are RESONATORS AT V.21'S OWN FOUR FREQUENCIES,
 * and that is what fixes their element type independently of `st_size`.
 *
 * Each is two sections of five shorts, which is what `tones = 2` over 20
 * bytes gives.  Every section has the form
 *
 *      { -r^2, 1.0, 2*r*cos(w), -2*cos(w), 1.0 }   in Q14
 *
 * with r = 0.9 and w = 2*pi*f/8000, and rounding that expression to integers
 * reproduces ALL TWENTY entries of both banks exactly from four published
 * numbers:
 *
 *      V21_CHAN1_MTD_COEFF   980 Hz, 1180 Hz     channel 1, the caller
 *      V21_CHAN2_MTD_COEFF  1650 Hz, 1850 Hz     channel 2, the answerer
 *
 * Those are V.21's mark and space tones as the Recommendation defines them.
 * A reading that made these anything but shorts in Q14, five to a section,
 * would not produce them.  `t_v21cfg.c` asserts the identity over all twenty
 * entries in integer arithmetic.  Finding F9350, and it is the same kind of
 * evidence F9141 recorded for `V29RX_CRR_TABLE`.
 *
 * Recorded as EVIDENCE FOR A TYPE, not as a generator: `docs/fastpass.md`
 * defers coefficient derivations to the 8 kHz retarget, and a byte-exact copy
 * is byte-exact.  What a byte copy cannot give is the stride, and this is
 * where the stride came from.
 */
#define V21_CHAN1_MARK_HZ	980
#define V21_CHAN1_SPACE_HZ	1180
#define V21_CHAN2_MARK_HZ	1650
#define V21_CHAN2_SPACE_HZ	1850

/* The rate the resonators are designed at; see the derivation above. */
#define V21_MTD_SAMPLE_RATE	8000

#endif /* DSPLIB_V21CFG_H */
