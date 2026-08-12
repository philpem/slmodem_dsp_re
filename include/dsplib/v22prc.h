/*
 * v22prc.h -- V.22 / V.22bis: the small state and status helpers.
 *
 * Nine leaf functions from two of the author's translation units, grouped
 * here because they share one property: none of them calls anything, so all
 * nine could be written and tested before the datapump object they read is
 * modelled.
 *
 * THE OBJECT IS NOT MODELLED, DELIBERATELY.  Four of these reach fields of
 * the V.22 datapump instance and the receiver hanging off it, and
 * `V22FP_create` -- the 2,449-byte function that lays that instance out -- is
 * not reconstructed.  Naming a struct now would mean guessing at fields whose
 * meaning has not been established, and `src/dsp/fpm_tone.c` already settled
 * how this tree handles that: a wrong layout is worse than none.  So the
 * parameters are `void *` and the offsets are named constants with the
 * evidence beside each.  When `V22FP_create` lands these get retyped and the
 * constants go away.
 *
 * The offsets are not guesses; each is a load or a store in the object, and
 * the function's own name is what licenses the reading of it.
 */

#ifndef DSPLIB_V22PRC_H
#define DSPLIB_V22PRC_H

/*
 * Offsets in the datapump instance -- the pointer these functions are handed.
 */
#define V22_OBJ_GTIMER		0x50	/* int *, a shared millisecond clock  */
#define V22_OBJ_FP		0x54	/* the V22FP receiver/transmitter     */

/*
 * Offsets in the object at V22_OBJ_FP.  Named after the function that reads
 * each, which is the only evidence there is for what they mean.
 */
#define V22FP_TX_CLOCK		0x78	/* short, written by TxClockSync      */
#define V22FP_SIGNAL		0xec	/* int,   returned by SignalDetect    */
#define V22FP_BAUD		0x12a	/* short, tripled into TX_CLOCK       */
#define V22FP_CARRIER		0x130	/* int,   returned by CarrierDetect   */
#define V22FP_QUALITY		0x186	/* unsigned short, GetSignalQuality   */

/* Written only by SetAdaptEqV22; see the note on its three modes. */
#define V22FP_EQ_ADAPT		0x10	/* int                                */
#define V22FP_EQ_MODE		0x16c	/* short                              */
#define V22FP_EQ_EXTRA		0x180	/* int, mode 3 only                   */

/*
 * The datapump's block, in samples.  TxNOP emits exactly this many and then
 * reports the count, so the constant is in the code and not a parameter --
 * the same 160 that v22_iir.c's filter loop runs.
 */
#define V22_TX_BLOCK		160

/* RxClampV22's block, which is a different and much shorter one. */
#define V22_CLAMP_BLOCK		12
#define V22_CLAMP_VALUE		15

/*
 * The two "have we finished training" predicates, and the patterns they look
 * for.  1200 requires EVERY entry to be 3; 2400 requires a run of more than
 * seven 15s at the END of the array.  The asymmetry is the original's.
 */
#define V22_TRAINED_1200_SYMBOL	3
#define V22_TRAINED_2400_SYMBOL	15
#define V22_TRAINED_2400_RUN	7	/* strictly more than this */

/*
 * Advance the shared timer by one block and return its new value.  The step
 * is 20, which is 160 samples at 8 kHz in milliseconds.
 */
int ReadGTimer(void *modem);

/*
 * Emit a block of silence: V22_TX_BLOCK zero samples, and the count.  The
 * first two arguments are never read.  Their types are undetermined -- `void *`
 * is chosen for the stack slot, not from evidence -- and the shape is that of
 * a transmit-state handler, which is what the seven-entry V22_PROTOCOL table
 * holds.
 */
void TxNOP(void *modem, void *arg1, short *out, short *count);

/* The same shape, but V22_CLAMP_BLOCK entries of V22_CLAMP_VALUE. */
void RxClampV22(void *modem, void *arg1, short *out, short *count);

/*
 * Training predicates.  `count` is read through a pointer, not passed by
 * value, and is unsigned.
 */
int RxTrained1200(const short *symbols, const unsigned short *count);
int RxTrained2400(const short *symbols, const unsigned short *count);

/* Status accessors. */
int CarrierDetect(void *modem);
int SignalDetect(void *modem);
unsigned short GetSignalQuality(void *modem);

/*
 * Derive the transmit clock from the baud field: three times it, stored back
 * as a short.
 *
 * DECLARED void, AND THE OBJECT DOES NOT SETTLE THAT.  It leaves the product
 * in `eax`, which is what a `short`-returning function would also do, and
 * there is no extension either way to tell them apart.  The store is the same
 * under both readings, so nothing observable turns on it; recorded here so
 * that a caller found later to use the value is recognised as evidence rather
 * than as a contradiction.
 */
void TxClockSync(void *modem);

/*
 * Equaliser adaptation control.  Three live modes and a silent default:
 *
 *   1  stop adapting              EQ_ADAPT = 0
 *   2  adapt                      EQ_ADAPT = 1, EQ_MODE = 0
 *   3  adapt, second mode         EQ_ADAPT = 1, EQ_MODE = 1, EQ_EXTRA = 1
 *   anything else                 nothing at all, silently
 *
 * `mode` is loaded with `movzwl`, so it is sixteen bits wide and unsigned;
 * a caller passing 0x10002 selects nothing, not mode 2.  Note also that mode
 * 3 sets EQ_EXTRA and mode 2 does not CLEAR it, so the two are not
 * symmetrical and the order the caller uses them in matters.
 */
void SetAdaptEqV22(void *modem, unsigned short mode);

#endif /* DSPLIB_V22PRC_H */
