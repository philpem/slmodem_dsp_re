/*
 * v8int.h -- static helpers shared between V.8's translation units.
 *
 * The object carries no symbol for any of V.8's sequence helpers: every use
 * is inlined, so a `static` helper that two units both call can only have
 * reached both through a header.  `ext_word` is the one such helper --
 * V8SetMessage (V8Interface.c) calls it directly and initTxSequence (V8.c)
 * reaches it through emit_extension -- so it is defined `static` here and
 * each including unit gets its own copy, exactly as the object shows.  The
 * helpers used by a single unit (emit_extension, selected_sequence,
 * rx_sequence, ext_expected, match_extension, in_list,
 * emit_extension_words) stay in that unit's own file.
 */

#ifndef V8INT_H
#define V8INT_H

#include "dsplib/v8.h"

/*
 * One extension character.  The octet is reversed -- V.8 goes least
 * significant bit first -- shifted up one and given a low bit, which is the
 * framing the fixed constants already carry.
 */
static short
ext_word(unsigned char c)
{
	return (short)((charFlip(c) << 1) | 1);
}

#endif
