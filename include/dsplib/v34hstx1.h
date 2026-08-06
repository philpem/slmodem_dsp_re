/*
 * v34hstx1.h -- ten arms of `v34handshak`'s per-sample transmit dispatch.
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
 * below +0x234 (finding 323).  The four added since are the smallest of what
 * was left: 60, 70, 18 and 51, at 34, 137, 152 and 285 bytes of level-0 body.
 * Those four DO write below +0x234 -- 70 writes `rate_now` and `rate_want` at
 * +0x228 -- so finding 323's "six" is a statement about the first six and not
 * about this file.
 *
 * WHAT AN ARM IS.  The loop is
 *
 *     while (txq.count < f2aa0)
 *             switch (txstate) { ... }
 *
 * and every one of these ten ends by rejoining that test, so an arm is a
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
 * NONE OF THE FOUR ADDED SINCE NEEDS A NEW EXIT.  60 rejoins at 0x62d70, 18
 * at 0x6409a on both of its paths, 70 at 0x6431f and 0x64326 and 51 at
 * 0x63941, 0x6409a and 0x62d70 -- every one of them a block that reloads the
 * object and re-tests the loop condition.  So the oracle past them is the
 * blob's own tail and nothing is given up; the two values below remain the
 * only places this file stops short.
 */

/* 18 SSEG         0x64048, continuing at 0x66d11 */
int v34tx1_sseg(void *obj);
/* 51 TX_L1        0x62c69, with the second copy of its loop at 0x65290 */
int v34tx1_tx_l1(void *obj);
/* 60 TONE_AB      0x62d3d */
int v34tx1_tone_ab(void *obj);
/* 65 XMIT0        0x62d83 */
int v34tx1_xmit0(void *obj);
/* 70 DATAXMIT     0x63ca8 */
int v34tx1_dataxmit(void *obj);
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
