/*
 * v22det.h -- V.22 / V.22bis: the transmit pattern generator and the three
 * received-pattern detectors.
 *
 * Reconstructed from dsplibs.o:
 *
 *   MakeTxData          .text 0x08bd50  195 bytes
 *   Detect_Retrain      .text 0x08bef0  288 bytes
 *   Detect_Rmloop2_ACK  .text 0x08c010  209 bytes
 *   Detect_1s           .text 0x08c0f0  199 bytes
 *
 * These four sit immediately below `Detect_v22` (0x08c1c0, src/pump/v22/
 * v22data.c) in the 0x8bd50..0x8c5a0 block, and none of them calls anything
 * or touches the datapump instance: they work on a symbol array and a count
 * passed by pointer.  That is why they can be written before the seven
 * `V22_PROTOCOL` handlers that use them.
 *
 * ---------------------------------------------------------------------------
 * ALL THREE DETECTORS ARE ONE CORRELATOR, WRITTEN OUT THREE TIMES
 *
 * Each accumulates three sums over the received symbols
 *
 *     sumsq  = sum s[i] * s[i]
 *     corr   = sum s[i] * ideal
 *     energy = sum ideal * ideal
 *
 * and declares a match when |2*corr - sumsq| clears a fraction of `energy`.
 * Expanding the left-hand side gives `energy - sum (s[i] - ideal)^2`, so the
 * test is "the symbols are close to `ideal`" written without a subtraction
 * inside the loop.
 *
 * EVERY ONE OF THE THREE ACCUMULATORS IS SIXTEEN BITS AND WRAPS.  The object
 * truncates each to a `short` on every iteration -- `cwtl`, or `movswl %ax`,
 * after each add -- and the comparison at the end is a 16-bit `cmp` on both
 * operands.  `energy` in particular is just `n * ideal^2` and wraps for
 * n * 225 above 32767, i.e. after 145 symbols at 2400 bit/s.  That is not a
 * defect being reproduced for its own sake: it is what decides the verdict on
 * any block long enough to reach it, and an `int` accumulator disagrees.
 *
 * ---------------------------------------------------------------------------
 * WHAT A DETECTOR RETURNS
 *
 * Zero for no match, and otherwise `*count` scaled by 27307/16384 -- 1.66668,
 * which is one 600-baud symbol interval in milliseconds (1000/600 = 1.66667).
 * The scale factor is a literal `imul $0x6aab / sar $0xe` in all three, and
 * the millisecond reading is INFERENCE from that arithmetic and from the
 * 600-baud symbol rate `v22_fse.h` establishes; nothing in these four
 * functions names it.  It is zero-extended into the return, so the value is
 * sixteen bits wide.
 */

#ifndef DSPLIB_V22DET_H
#define DSPLIB_V22DET_H

/*
 * The bit rate the caller is running at.  Passed as a `short` and compared
 * against 2400 exactly; every other value -- 1200 included -- selects the
 * other arm.
 */
#define V22_DET_BPS_2400	2400

/*
 * The symbol each detector correlates against.  `Detect_1s` uses the same
 * two values `v22prc.h`'s RxTrained1200 and RxTrained2400 look for, which is
 * what licenses reading it as the scrambled-ones pattern; the pair
 * `Detect_Rmloop2_ACK` uses has no such corroboration and is named for its
 * value alone.
 */
#define V22_DET_RMLOOP2_1200	2
#define V22_DET_RMLOOP2_2400	10

/*
 * `Detect_Retrain` correlates the QUADRANT of each symbol, not the symbol:
 * it takes bits 3:2, which `fpm_smc.h` establishes as where V.22's quadrant
 * lives, and looks for a constant 3.
 */
#define V22_DET_QUAD_SHIFT	2
#define V22_DET_QUAD_MASK	3
#define V22_DET_RETRAIN_QUAD	3

/*
 * The match threshold, Q15, and a literal in both `Detect_Retrain` and
 * `Detect_Rmloop2_ACK`.  0.98999.  `Detect_1s` takes its own as an argument
 * instead.
 */
#define V22_DET_THRESH_Q15	0x7eb8

/*
 * One symbol interval in milliseconds, Q14: 27307/16384 = 1.66668, and
 * 1000 ms / 600 baud = 1.66667.  See the note above -- the constant is the
 * object's, the unit is inference.
 */
#define V22_DET_SYMBOL_MS_Q14	0x6aab

/*
 * `Detect_Retrain` also requires the symbol stream to be PERIOD-2: it walks
 * the even indices and counts how many in a row satisfy
 * s[i] == s[i-2] && s[i+1] == s[i-1], resetting to zero on any that does not.
 * More than this many consecutive pairs, AND the correlation, is a retrain.
 */
#define V22_DET_RETRAIN_RUN	2	/* strictly more than this */

/*
 * The five transmit patterns `MakeTxData` can lay down, selected by its third
 * argument.  Anything outside 0..4 writes nothing at all, silently, and the
 * range test is UNSIGNED -- a negative selector is a no-op, not case 0.
 *
 * Two of the five are corroborated: pattern 1 emits exactly what
 * `RxTrained1200` accepts and pattern 2 exactly what `RxTrained2400` accepts
 * (v22prc.h).  Pattern 0's alternating 0, 3 is the shape of V.22bis's S1
 * double dibit but nothing here says so, and patterns 3 and 4 are named for
 * the symbol they emit because that is all the object settles.
 */
#define V22_TXDATA_S1		0	/* 0, 3 alternating, in pairs       */
#define V22_TXDATA_ONES_1200	1	/* every symbol 3                   */
#define V22_TXDATA_ONES_2400	2	/* every symbol 15                  */
#define V22_TXDATA_SYMBOL_2	3	/* every symbol 2                   */
#define V22_TXDATA_SYMBOL_10	4	/* every symbol 10                  */

/* The two symbols pattern 0 alternates between. */
#define V22_TXDATA_S1_EVEN	0
#define V22_TXDATA_S1_ODD	3

/*
 * Fill `out` with `*count` symbols of the selected pattern.
 *
 * `count` is read through a pointer with `movswl`, but only the low sixteen
 * bits of the result are ever used, so the extension is FREE in finding
 * F614's sense and `short` is chosen because it is what the object encodes
 * rather than because anything forces it.
 *
 * THE LOOPS COUNT DOWN TO ZERO AND DO NOT TEST FOR POSITIVE.  A negative
 * count therefore runs until the sixteen-bit counter wraps through -32768 to
 * zero -- 65535 writes for -1 -- rather than none.
 *
 * PATTERN 0 DOES NOT TERMINATE ON AN ODD COUNT.  It writes two symbols per
 * step and decrements by two, and the odd sixteen-bit values form a cycle
 * under -2 that never contains zero, so the loop runs for ever.  That is the
 * object's behaviour and it is reproduced; it is recorded here because it is
 * the one input a test cannot take.
 */
void MakeTxData(short *out, const short *count, short pattern);

/*
 * Is the receiver seeing continuous ones?  `thresh` is Q15 and multiplies the
 * ideal energy; `bps` selects which symbol counts as a one.
 *
 * The symbols are read UNSIGNED (`movzwl`), which matters only above 0x8000
 * and cannot arise from a four-bit constellation index; it is what the object
 * encodes and is kept.
 */
int Detect_1s(const unsigned short *sym, const unsigned short *count,
	      short bps, short thresh);

/* The RMLOOP2 acknowledgement pattern.  Its threshold is built in. */
int Detect_Rmloop2_ACK(const unsigned short *sym, const unsigned short *count,
		       short bps);

/*
 * The retrain request: a constant quadrant AND a period-2 symbol stream.
 * Takes no rate -- it works on quadrants, which are the same at both rates.
 */
int Detect_Retrain(const unsigned short *sym, const unsigned short *count);

#endif /* DSPLIB_V22DET_H */
