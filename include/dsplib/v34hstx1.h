/*
 * v34hstx1.h -- seventeen arms of `v34handshak`'s per-sample transmit
 * dispatch.
 *
 * `v34handshak` is 61,541 bytes and is not one unit.  Table 1, the jump table
 * at `.rodata+0x2da0`, is read INSIDE the per-sample loop at 0x62950 and
 * indexed by `txstate - 5`; nineteen of its eighty-two entries have a target
 * of their own and fifty-seven are the loop bottom (finding 287).  Each arm
 * is one dispatch case, and each is reconstructed and tested on its own --
 * see docs/v34handshak.md for why the function is taken this way and
 * test/unit/t_v34hstx1.c for how one arm is compared against the blob.
 *
 * Six of them -- 65, 71, 78, 81, 85 and 86 -- are the ones that write nothing
 * below +0x234 (finding 323).  Then 60, 70, 18 and 51, the smallest of what
 * was left, at 34, 137, 152 and 285 bytes of level-0 body; those four DO write
 * below +0x234 -- 70 writes `rate_now` and `rate_want` at +0x228 -- so finding
 * 323's "six" is a statement about the first six and not about this file.  And
 * then 19, 20 and the entry 5, 54 and 74 share, at 568, 602 and 719 bytes.
 * And then 69 `EXMIT` and the entry 64 `JTXMIT` and 68 `J1TXMIT` share, at
 * 823 and 892 bytes.  And then 67 `XMITMP` at 2,024, which is the largest
 * arm of the table taken so far and the only one that clocks a MESSAGE out
 * rather than a fixed pattern -- and then 24 `TX_DPSK` at 2,220, which
 * clocks a different message out one bit at a time and carries the whole of
 * Modem-on-Hold's clear-down behind it.
 *
 * SEVENTEEN FUNCTIONS FOR TWENTY-THREE txstates, and the SHAPES ARE WORTH
 * TELLING APART because "one table entry" does not mean "one behaviour" here:
 *
 *     78 / 85          TWO entries, one body, two tail calls
 *     81 82 83 84      ONE entry, one behaviour
 *     5 / 54 / 74      ONE entry, THREE behaviours -- the shared prologue
 *                      re-reads `txstate` at 0x640f4 and branches
 *     64 / 68          ONE entry, ONE body and TWO tails -- the body is
 *                      common and the re-read at 0x636ff is reached only on
 *                      the pass where `vect_idx` wraps to zero
 *
 * so 78/85 are two functions over one static helper, and 5/54/74 and 64/68
 * are each ONE function with the state read inside it, because the read is
 * the object's.
 *
 * WHAT AN ARM IS.  The loop is
 *
 *     while (txq.count < f2aa0)
 *             switch (txstate) { ... }
 *
 * and every one of these ends by rejoining that test, so an arm is a
 * function returning void-equivalent and the caller loops.  Two of them can
 * instead leave the loop through a block that is NOT reconstructed, and they
 * say so in their return value rather than doing something plausible: the
 * eventual `v34handshak` must dispatch on it.
 *
 * WHAT IS NOT HERE, and it is a real gap rather than an omission.  Every arm
 * below that changes `txstate`, and both counter arms, guard a diagnostic on
 * `dsplibs_debug_level > 1` and print through the far blocks at 0x655f0,
 * 0x6821d, 0x68271, 0x68282, 0x6824f, 0x68260 and 0x684c6.  None of those
 * prints is reconstructed, so these functions are faithful at debug level 0
 * -- which is what the library ships (`dsplibs_debug_level` is zero) and what
 * `t_v34hstx1.c` tests at.  Finding 341.
 */

#ifndef DSPLIB_V34HSTX1_H
#define DSPLIB_V34HSTX1_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Where an arm left the dispatch.
 *
 * `V34TX1_LOOP` is "rejoined the loop bottom at 0x629e0" -- 0x629c8, 0x62d70
 * and 0x6430c all reach it, and the difference between them is a write the
 * arm that uses it performs for itself.
 *
 * The other two are transfers OUT of the arm into blocks this file does not
 * model.  They are returned rather than followed because following them
 * would mean writing code no differential test here can reach, which is
 * exactly the wrong-but-plausible thing the tree does not commit.
 */
enum v34tx1_exit {
	V34TX1_LOOP = 0,	/* back to the per-sample loop test  */
	V34TX1_MOH_WRAP,	/* 81: `vect_idx` hit 0xc0 -> 0x66d85 */
	V34TX1_TXMD_DONE	/* 86: `vect_idx` hit +0xaa78 -> 0x66fe9 */
};

/*
 * NO ARM ADDED SINCE NEEDS A NEW EXIT, and that is a result rather than the
 * absence of one.  60 rejoins at 0x62d70, 18 at 0x6409a on both of its paths,
 * 70 at 0x6431f and 0x64326, 51 at 0x63941, 0x6409a and 0x62d70; 19 at 0x629c8
 * and 0x63948, 20 at 0x6431f and 0x63da2, and 5/54/74 at 0x6409a, 0x629cf,
 * 0x640a1, 0x63da2 and 0x63941; 69 at 0x63941 and 0x6409a, and 64/68 at
 * 0x629c8, 0x62d32, 0x63941, 0x63da2, 0x6409a, 0x640a1, 0x6431f and 0x64326;
 * 67 at 0x62d70 and 0x640a1, and 24 at 0x629c8, 0x629cf, 0x63948, 0x6431f,
 * 0x64326 and 0x640a1 -- every one of them a block that reloads the object
 * and re-tests the loop condition.  So the oracle past them is the blob's own
 * tail and nothing is given up; the two values above remain the only places
 * this file stops short.
 *
 * 0x62d32 IS PART OF 64/68 AND NOT OF ITS REJOIN, for the reason 0x6430c is
 * part of 20: it clears `vect_idx` before falling into 0x62d70, and only one
 * of 64/68's five exits passes through it.  51 reaches the same block.
 *
 * 0x6430c IS PART OF 20 AND NOT OF ITS REJOIN.  20's two counting paths pass
 * through it and bump f25c0; the path that ends the segment leaves through
 * 0x63da2, which does not.  86 rejoins through 0x6430c as well (finding 340).
 */

/*
 * 5 SILENCE, 54 SILENCEINFO, 74 SILENCERETRAIN  0x640b4.
 *
 * ONE TABLE ENTRY FOR THREE txstates, and unlike 81/82/83/84 it is not one
 * behaviour: the shared prologue re-reads +0x3596 at 0x640f4 and gives each
 * of the three a different tail.  One function, with the read inside it,
 * because the read is the object's.
 */
int v34tx1_silence(void *obj);
/*
 * 64 JTXMIT and 68 J1TXMIT  0x635cc.
 *
 * ONE TABLE ENTRY FOR TWO txstates, ONE BODY AND TWO TAILS.  The body -- the
 * countdown at +0xaa78, two bits out of +0x25d6, the differential quadrant
 * and one `vect4` point -- is the same for both, and only the pass on which
 * `vect_idx` wraps to zero re-reads +0x3596 (0x636ff) and separates them: 68
 * ends the segment at 0x65653 and 64 counts +0x25d8 up to 0x80 first and
 * then becomes 68.  One function, with the read inside it.
 */
int v34tx1_jtxmit(void *obj);
/*
 * 24 TX_DPSK      0x62b96, with the second reader at 0x67c4e, the message
 * dispatch at 0x64e91 and the hold tail at 0x64fec.
 *
 * One bit of the Modem-on-Hold message per pass, XORed into +0x358c and sent
 * as `vect4[0]` or `vect4[2]` -- 60 `TONE_AB`'s two points and 60's scaling,
 * which is why the two AGREE COLD and are still not one arm.  When the
 * message runs out the arm either hands the transmit machine to 60 or, on
 * hold, re-arms the reader and works through the clear-down.
 */
int v34tx1_tx_dpsk(void *obj);
/* 18 SSEG         0x64048, continuing at 0x66d11 */
int v34tx1_sseg(void *obj);
/* 19 SBARSEG      0x6296d, with the segment's end at 0x671f0 */
int v34tx1_sbarseg(void *obj);
/* 20 PPSEG        0x642bf, continuing at 0x66d57 and 0x680ac */
int v34tx1_ppseg(void *obj);
/* 51 TX_L1        0x62c69, with the second copy of its loop at 0x65290 */
int v34tx1_tx_l1(void *obj);
/* 60 TONE_AB      0x62d3d */
int v34tx1_tone_ab(void *obj);
/* 65 XMIT0        0x62d83 */
int v34tx1_xmit0(void *obj);
/*
 * 69 EXMIT        0x63858, with the sixteen-point half at 0x67031 and the
 * segment's end at 0x66e01.  One halfword of the receiver -- +0x11e against
 * 0x89b0 -- chooses between two symbols per pass out of `vect4` and four out
 * of `vect16`, and the two halves advance `vect_idx` by two and by four.
 */
int v34tx1_exmit(void *obj);
/* 70 DATAXMIT     0x63ca8 */
int v34tx1_dataxmit(void *obj);
/*
 * 67 XMITMP       0x6399b, with the sixteen-point mapper at 0x63b62, the
 * four-point one at 0x64929 and the sequence's end at 0x645d0.
 *
 * Two bits per symbol or four, out of the message reader at +0xaa6c -- the
 * whole of 0x639db..0x63aae is `getbit` INLINED and one arm of it, the
 * restart, is a real call at 0x6484c.  The bits are collected into `f25c8`
 * lowest first and then mapped through `vect4` or `vect16`, chosen by the
 * same receiver halfword against the same constant that 69 uses.
 */
int v34tx1_xmitmp(void *obj);
/* 71 TXLEVEL      0x641d1 */
int v34tx1_txlevel(void *obj);
/* 78 JaTXMIT      0x64139 */
int v34tx1_jatxmit(void *obj);
/* 81 MOH_SILENCE  0x63d58, shared by txstates 81, 82, 83 and 84 */
int v34tx1_moh_silence(void *obj);
/* 85 K56JaTXMIT   0x63fb0 */
int v34tx1_k56jatxmit(void *obj);
/* 86 TXMD         0x63dae */
int v34tx1_txmd(void *obj);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34HSTX1_H */
