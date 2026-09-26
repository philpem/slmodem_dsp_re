/*
 * V29rxdec.c -- ITU-T V.29 (fax): the receiver's decision tables.
 *
 * The blob names `V29rxdec.c` between `V29rx.c` and `V29rxtab.c`.  Its only
 * candidates are the five `V29RX_DEC_*` arrays below: they sit at .data
 * 0x007e20..0x007eaf, the slot between `V29rx.c`'s own configuration and
 * `V29rxtab.c`'s `V29RX_FSE_*` (0x007ec0..), exactly where a `V29rxdec.c`
 * input falls in link order.  They are `V29RX_decision`'s only readers.  The
 * bodies are the text that was in `V29rxtab.c`, moved verbatim.
 */

#include "dsplib/v29cfg.h"

/*
 * ------------------------------------------------------------------------
 * THE DECISION TABLES.  `V29RX_decision` (0x9b8f0, 260 bytes) is their only
 * reader and it references nothing else that is unwritten, so these five are
 * the whole of its data blocker.
 *
 * ALL FIVE ARE LOADED WITH `movzwl TABLE(%reg,%reg,1)`, which is a stride of
 * two -- the index is doubled by the addressing mode, not scaled by a size --
 * so the element type is sixteen bits and the counts below are `st_size / 2`
 * confirmed by the loop's own bound.
 *
 * THE BOUND IS 8 OR 16 AND THE OBJECT COMPUTES IT WITHOUT A BRANCH:
 *
 *      cmp  $0x1,%ebp          ebp = rx->f10, the rate selector
 *      sbb  %esi,%esi          esi = -1 when f10 == 0, else 0
 *      and  $0xfffffff8,%esi   esi = -8 or 0
 *      lea  0x10(%esi),%ebp    ebp = 8 or 16
 *
 * so the slicer searches the first EIGHT points at the low rate and all
 * SIXTEEN at the high one. Eight points is three bits a symbol at 2400 baud,
 * which is V.29's 7200 bit/s fallback; sixteen is four bits, which is 9600.
 * That is why the tables are sixteen entries with the eight-point
 * constellation FIRST, and it is measured from the code rather than inferred
 * from the layout.
 *
 * AND THE CONSTELLATION IS THE RECOMMENDATION'S OWN, TO THE UNIT.  V.29's
 * Table 1 gives amplitudes 3 and 5 on the axes and sqrt(2) and 3*sqrt(2) on
 * the diagonals. Every value in `V29RX_DEC_IMAP`, `_QMAP` and `_MAG` is one of
 * those four multiplied by 2048:
 *
 *      3 * 2048        = 6144         sqrt(2) * 2048  = 2896
 *      5 * 2048        = 10240      3*sqrt(2) * 2048  = 8689
 *
 * Four independent numbers agreeing with a published table is what makes this
 * a derivation rather than a resemblance, and it is the evidence that the
 * element type is a signed sixteen-bit amplitude. `t_v29cfg.c` asserts all
 * sixteen points against `2048 * A * {cos,sin}(k * 45 degrees)` computed from
 * the amplitudes alone.
 *
 * `V29RX_DEC_ANGLE` is the same eight phases twice, 4096 per 45 degrees in a
 * 32768-count turn, so the second ring repeats the first ring's angles -- as
 * it must, both rings being at the same eight phases.
 *
 * `V29RX_DEC_PMAP` is indexed by `(phase - prev_phase) & 7` and is the
 * differential decoder's Gray map. Eight entries, and it is a PERMUTATION of
 * 0..7 -- asserted as one, because a table of eight small integers is exactly
 * where a transcription slip hides.
 *
 * THE LOADS ARE `movzwl` AND THE VALUES ARE NEGATIVE, WHICH IS NOT A
 * CONTRADICTION.  Each load is followed by a subtraction and then a
 * `movswl %dx,%edx` that discards the upper half, so the extension is 614's
 * free case: `unsigned short` and `short` compile to the same bytes and behave
 * identically here. `short` is chosen because `V29RX_DEC_IMAP` holds -6144 and
 * the amplitudes above are signed. Recorded as a choice, not as a reading.
 */

short V29RX_DEC_QMAP[16] = {
	     0,   2048,   6144,   2048,      0,  -2048,  -6144,  -2048,
	     0,   6144,  10240,   6144,      0,  -6144, -10240,  -6144
};

short V29RX_DEC_IMAP[16] = {
	  6144,   2048,      0,  -2048,  -6144,  -2048,      0,   2048,
	 10240,   6144,      0,  -6144, -10240,  -6144,      0,   6144
};

short V29RX_DEC_ANGLE[16] = {
	     0,   4096,   8192,  12288,  16384,  20480,  24576,  28672,
	     0,   4096,   8192,  12288,  16384,  20480,  24576,  28672
};

short V29RX_DEC_MAG[16] = {
	  6144,   2896,   6144,   2896,   6144,   2896,   6144,   2896,
	 10240,   8689,  10240,   8689,  10240,   8689,  10240,   8689
};

short V29RX_DEC_PMAP[8] = { 1, 0, 2, 3, 7, 6, 4, 5 };
