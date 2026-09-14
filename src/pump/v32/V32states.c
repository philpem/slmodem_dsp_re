/*
 * v32state.c -- ITU-T V.32/V.32bis: the handshake state-name accessor.
 *
 * Reconstructed from dsplibs.o:
 *   V32StateName  .text   0x084510  (22 bytes)
 *   statenames    .rodata 0x007e80  (140 bytes -- 35 pointers)
 *
 * The whole function is six instructions:
 *
 *   84510: mov 0x4(%esp),%edx
 *   84514: mov $0x3952,%eax          <== .rodata.str1.1  ("INVALID!")
 *   84519: cmp $0x22,%edx
 *   8451c: ja  84525
 *   8451e: mov 0x7e80(,%edx,4),%eax  <== .rodata         (statenames)
 *   84525: ret
 *
 * Two things are read out of that and neither is a guess.  The bound is
 * UNSIGNED -- `ja`, not `jg` -- so a negative index takes the fallback rather
 * than reading backwards off the table, and 0x22 = 34 is exactly the last
 * index, which confirms the table length from the code rather than from the
 * symbol size.  The fallback string is loaded FIRST and overwritten on the
 * in-range path, which is how GCC 3.4.2 spells `x = default; if (ok) x =
 * table[i]; return x;`.
 *
 * The fallback is "INVALID!".  `include/dsplib/v32state.h` said
 * "STATE_UNKNOWN" before this file existed; that was a plausible guess
 * written when only the table had been read, and it is corrected there.
 *
 * `statenames` is file-local in the object (`objcopy --globalize-symbols` is
 * what gives it a `ref_` alias at all), so it is `static` here.  The
 * differential test compares the strings the two accessors return, which is
 * what the object's only consumer -- the debug printer -- can observe.
 */

#include "dsplib/v32state.h"

static const char *const statenames[V32_STATE_COUNT] = {
	"STATE_A",		/* 0  */
	"STATE_B",		/* 1  */
	"STATE_B2",		/* 2  */
	"STATE_C",		/* 3  */
	"STATE_D",		/* 4  */
	"STATE_D2",		/* 5  */
	"STATE_E",		/* 6  */
	"STATE_F",		/* 7  */
	"STATE_G",		/* 8  */
	"STATE_H",		/* 9  */
	"STATE_I",		/* 10 */
	"STATE_J",		/* 11 */
	"STATE_K",		/* 12 */
	"STATE_L",		/* 13 */
	"STATE_M",		/* 14 */
	"STATE_N",		/* 15 */
	"STATE_O",		/* 16 */
	"STATE_P",		/* 17 */
	"STATE_Q",		/* 18 */
	"STATE_R",		/* 19 */
	"STATE_S",		/* 20 */
	"STATE_T",		/* 21 */
	"STATE_U",		/* 22 */
	"STATE_V",		/* 23 */
	"STATE_W",		/* 24 */
	"STATE_X",		/* 25 */
	"STATE_Y",		/* 26 */
	"STATE_Z",		/* 27 */
	"STATE_END",		/* 28 */
	"STATE_F2",		/* 29 */
	"STATE_X2",		/* 30 */
	"STATE_CLEARDOWN",	/* 31 */
	"STATE_DONE",		/* 32 */
	"STATE_ERROR",		/* 33 */
	"STATE_DONT_CARE"	/* 34 */
};

const char *
V32StateName(int state)
{
	const char *name = "INVALID!";

	if ((unsigned int)state <= (unsigned int)(V32_STATE_COUNT - 1))
		name = statenames[state];

	return name;
}
