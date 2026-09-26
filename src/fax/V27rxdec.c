/*
 * V27rxdec.c -- ITU-T V.27ter (fax): the receiver's decision and
 *              constellation tables, as `V27RX_create` seeds them.
 *
 * The blob names `V27rxdec.c` between `V27rx.c` and `V27rxtab.c`.  Its only
 * candidates are the seven `V27RX_DEC_*` arrays below: they sit at .data
 * 0x007bc0..0x007c03, the slot between `V27rx.c`'s own configuration and
 * `V27rxtab.c`'s `V27RX_FSE_*`, which is exactly where a `V27rxdec.c` input
 * falls in link order.  Every one is referenced only from `V27RX_create`.
 * The bodies are the text that was in `V27rxtab.c`, moved verbatim.
 */

#include "dsplib/v27cfg.h"

/*
 * The decoder's constellation: the phase angle of each index, and the bits
 * each phase STEP carries.  Four entries at 2400 bit/s and eight at 4800,
 * which is `V27RX_DEC_PHS_MASK` + 1 and is what `V27RX_decision` selects on
 * `dec->eight_phase`.  The angles are `i * 0x8000 / n` exactly -- one full
 * revolution divided into four or eight -- in the units `FPM_atan` produces.
 */
short V27RX_DEC_LAST_PHASE_2400[4] = {
	     0,   8192,  16384,  24576,
};


short V27RX_DEC_LAST_PHASE_4800[8] = {
	     0,   4096,   8192,  12288,  16384,  20480,  24576,  28672,
};


/*
 * The selector, and here the pointer array is `D` -- writable.  Eleven of the
 * fourteen selectors are, and the other three are `R`; the split is the
 * object's and is reproduced rather than tidied.
 */
short *V27RX_DEC_LAST_PHASE[2] = {
	V27RX_DEC_LAST_PHASE_2400, V27RX_DEC_LAST_PHASE_4800
};

short V27RX_DEC_PMAP_2400[4] = {
	     0,      1,      3,      2,
};


short V27RX_DEC_PMAP_4800[8] = {
	     1,      0,      2,      3,      7,      6,      4,      5,
};


short *V27RX_DEC_PMAP[2] = {
	V27RX_DEC_PMAP_2400, V27RX_DEC_PMAP_4800
};

/*
 * The phase index mask, stored into the decoder block's +0x14 at 99c57.
 * 3 and 7, which is four phases and eight.
 */
short V27RX_DEC_PHS_MASK[2] = {
	     3,      7,
};
