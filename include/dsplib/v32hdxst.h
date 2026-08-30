/*
 * v32hdxst.h -- ITU-T V.32 / V.32bis: the twenty half-duplex STATES and the
 *               five next-state dispatchers they transition through.
 *
 * `include/dsplib/v32hdx.h` is the CONTRACT -- what the two drivers
 * (`V32TxHdxModem`, `V32RxHdxModem`) require of a state.  This header is the
 * roster: the twenty states themselves, the five `V32*NextState` functions,
 * and the four `.data` tables that tie them together.
 *
 * ---------------------------------------------------------------------------
 * WHY THIS IS ONE HEADER AND NOT SIX
 *
 * The twenty-five functions are ONE strongly-connected component in the
 * object's relocation graph, and that is measured rather than assumed:
 *
 *     python3 tools/closure.py RxHdxNull --missing
 *
 * reports twenty-two of them from a 103-byte leaf, and adding the three the
 * walk does not reach from there (`RxHdxTone`, `TxHdxNull`, `TxHdxTone`)
 * closes it at twenty-five call symbols and six data symbols.  A state
 * installs its successor by STORING that successor's address into the
 * context, and a `movl $handler, field` is a relocation exactly as a `call`
 * is -- so no proper subset of the twenty-five can be linked (findings F8492,
 * F8493).  They land together or not at all, and one header is the honest
 * shape of that.
 *
 * ---------------------------------------------------------------------------
 * THE OBJECT'S OWN FILE NAMES
 *
 * The blob's `FILE` symbols name the original translation units, and the
 * `.text` addresses group cleanly against them:
 *
 *     V32TXHDX.c   0x07fce0..0x080324   V32TxHdxModem + the eight TxHdx*
 *     V32rxhdx.c   0x0838f0..0x084509   V32RxHdxModem + the twelve RxHdx*
 *                                       (with CalcTurnAroundDelay local)
 *     V32org.c     0x082be0             V32OrgNextState
 *     V32ans.c     0x085ab0             V32AnsNextState
 *     V32RNG.c     0x084c80, 0x0852d0   V32RngInitNextState, V32RngRespNextState
 *     V32loop.c    0x0864a0             V32LocLoopNextState
 *
 * This tree does not mirror the blob's translation units -- `v32fpctl.c`
 * already carries functions from four of these spans -- so the reconstruction
 * splits by role instead.  The mapping is recorded here because it is the
 * only place the object's own grouping is written down, and because a tier-3
 * pass that wants emission order will need it.
 *
 * ---------------------------------------------------------------------------
 * THE MODE TABLE, AND A CORRECTION TO v32hdx.h
 *
 * A state transitions with `call *V32NextState[hdx->mode]` -- `TxHdxTone` at
 * 7fd9b is the worked example.  `v32hdx.h`'s prose lists that table's six
 * slots as
 *
 *     0 Org   1 Ans   2 LocLoop   3 LocLoop   4 RngInit   5 (no relocation)
 *
 * and THAT IS WRONG in two slots.  `tools/tabdump.py --sym V32NextState`
 * prints all six relocations:
 *
 *     0  V32OrgNextState        3  V32LocLoopNextState
 *     1  V32AnsNextState        4  V32RngInitNextState
 *     2  V32LocLoopNextState    5  V32RngRespNextState
 *
 * so slot 5 is relocated after all, and the pair of `V32LocLoopNextState`
 * entries is at 2 and 3 rather than 2 and 3-with-a-hole.  Finding F8560.
 */

#ifndef DSPLIB_V32HDXST_H
#define DSPLIB_V32HDXST_H

#include "dsplib/v32hdx.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The mode, which is hdx + V32HDX_MODE (0x76) and indexes V32NextState.
 * Named from the table's own relocations; see the correction above.
 *
 * THERE ARE TWO UNRELATED THINGS IN THIS TREE CALLED A V.32 "MODE" AND THEY
 * SHARE A PREFIX BY ACCIDENT OF THE OBJECT'S OWN VOCABULARY.  This one is the
 * HANDSHAKE mode -- which of the five state machines drives the half-duplex
 * exchange, six slots, held at hdx + 0x76.  `include/dsplib/v32fpctl.h`'s
 * `V32_MODE_ABS4` .. `V32_MODE_128T` are the CONSTELLATION mode -- which
 * signal set the modulator uses, seven values, a different field entirely.
 * No macro name collides; the concepts do.  Check which field a use reads
 * before reaching for either set.
 */
#define V32_MODE_ORIGINATE	0
#define V32_MODE_ANSWER		1
#define V32_MODE_LOCLOOP_2	2
#define V32_MODE_LOCLOOP_3	3
#define V32_MODE_RING_INIT	4
#define V32_MODE_RING_RESP	5
#define V32_NEXTSTATE_COUNT	6

/*
 * A next-state dispatcher.  One argument, the modem instance, and no value:
 * every call site is `push modem; call *V32NextState[mode]` with %eax dead
 * afterwards (7fd9b in `TxHdxTone` is the shortest).
 */
typedef void (*v32_nextstate_fn)(void *modem);

/*
 * `.data`, GLOBAL, and therefore NOT const -- section 143 in the object for
 * all four.  `V32_RX_MODE` and `V32_TX_MODE` are seven entries each, indexed
 * by the V.32 rate 0..6 of `v32seq.h`, and `V32_CONNECT` is the handshake
 * state each rate connects in.
 */
extern v32_nextstate_fn V32NextState[V32_NEXTSTATE_COUNT];
extern short V32_CONNECT[7];
extern short V32_RX_MODE[7];
extern short V32_TX_MODE[7];

/*
 * The S-tone detector's coefficient bank -- .rodata 0x006d60, 30 bytes,
 * GLOBAL, and therefore const.  It is `struct fpm_mtd_cfg::coeff`: fifteen
 * shorts, three biquad sections of five, and the type is settled by the
 * callee rather than by the table (`include/dsplib/fpm_mtd.h`, "+0x00 is a
 * POINTER to the coefficient bank, not a scalar").
 *
 * `V32OrgNextState` (830b8) and `V32AnsNextState` (860c3) store its address
 * into the MTD at hdx + V32_HDX_MTD before calling `FPM_MTD_create`, and
 * `RxHdxSTone` (842b3) tests `mtd->cfg.coeff == V32_S_DATA_COEF` to tell the
 * DATA-mode S tone's detector from the other bank the same field can hold.
 *
 * That other bank is `V32_S_COEF`, .rodata 0x006d7e, also fifteen shorts and
 * immediately after this one.  It is NOT defined here: nothing written or in
 * this batch's closure references it, and a table with no consumer cannot be
 * differentially tested.  Finding F8563.
 */
extern const short V32_S_DATA_COEF[15];

/* ---------------------------------------------------------------- transmit */

short TxHdxTone(void *modem, short *data, short *out, unsigned short *left);
short TxHdxCarrierState(void *modem, short *data, short *out,
			unsigned short *left);
short TxHdxScrSequence(void *modem, short *data, short *out,
		       unsigned short *left);
short TxHdxTRN(void *modem, short *data, short *out, unsigned short *left);
short TxHdxData(void *modem, short *data, short *out, unsigned short *left);
short TxHdxNoCarrier(void *modem, short *data, short *out,
		     unsigned short *left);
short TxHdxFinishFrame(void *modem, short *data, short *out,
		       unsigned short *left);
short TxHdxNull(void *modem, short *data, short *out, unsigned short *left);

/* ----------------------------------------------------------------- receive */

void RxHdxTone(void *modem, short *in, unsigned short *out,
	       unsigned short *count);
void RxHdxNoSignal(void *modem, short *in, unsigned short *out,
		   unsigned short *count);
void RxHdxPhsReversal(void *modem, short *in, unsigned short *out,
		      unsigned short *count);
void RxHdxRateSequence(void *modem, short *in, unsigned short *out,
		       unsigned short *count);
void RxHdxSequence(void *modem, short *in, unsigned short *out,
		   unsigned short *count);
void RxHdxSequenceE(void *modem, short *in, unsigned short *out,
		    unsigned short *count);
void RxHdxData(void *modem, short *in, unsigned short *out,
	       unsigned short *count);
void RxHdxToneData(void *modem, short *in, unsigned short *out,
		   unsigned short *count);
void RxHdxSTone(void *modem, short *in, unsigned short *out,
		unsigned short *count);
void RxHdxEpoch(void *modem, short *in, unsigned short *out,
		unsigned short *count);
void RxHdxError(void *modem, short *in, unsigned short *out,
		unsigned short *count);
void RxHdxNull(void *modem, short *in, unsigned short *out,
	       unsigned short *count);

/* --------------------------------------------------------- the dispatchers */

void V32OrgNextState(void *modem);
void V32AnsNextState(void *modem);
void V32RngInitNextState(void *modem);
void V32RngRespNextState(void *modem);
void V32LocLoopNextState(void *modem);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32HDXST_H */
