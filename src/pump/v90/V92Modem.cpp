/*
 * V92Modem.cpp -- `V92Modem::printTitle`, 0x13bf0, 0xac = 172 bytes.
 *
 * The original translation unit is `V92Modem.cpp`, STT_FILE #25, and this
 * file holds the one member of it that has been reconstructed.  See
 * include/dsplib/V92Modem.h for why the class declaration there is not an
 * object map, and src/pump/v90/V90Modem.cpp for the same function's V.90
 * twin -- reading either without the other loses the two differences between
 * them.
 *
 * THE SOURCE ORDER IS NOT THE DISASSEMBLY ORDER, for the same reason: GCC
 * moved the gated block to the end, so 0x13bf0 downwards reads 1, 2, 3, 7, 8,
 * 9, 4, 5, 6.  0x13c1e jumps forward to the gated block at 0x13c52 when the
 * level is high, the block jumps back to 0x13c20, and 0x13c20 is message 7.
 *
 * WHAT DIFFERS FROM `V90Modem::printTitle`, and both are easy to lose:
 *
 *   - there is no "Components: ..." line here.  Eight messages, not nine.
 *   - THE CLOSING BANNER IS UNGATED.  0x13c41-0x13c4d loads the banner into
 *     the outgoing slot and tail-jumps straight to `edprintf` with no
 *     `cmpl $0x1` in front of it, where the V.90 file tests the level first
 *     and tail-jumps to `dsplibs_debug_printf` instead.
 *
 * The banner strings are this TU's own copies at .rodata.str1.4+0x34a0,
 * +0x34dc and +0x3518, textually identical to `V90Modem.cpp`'s at 0x416c and
 * not shared with them.
 *
 * See docs/findings.md 837.
 */

#include "dsplib/V92Modem.h"

#include "dsplib/debug.h"
#include "dsplib/encode.h"

/* .rodata.str1.4+0x34a0. */
#define V92_BANNER \
	"*********************************************************\r\n"

void
V92Modem::printTitle()
{
	edprintf(V92_BANNER);
	edprintf("*******         This is a PRIVATE version         *******\r\n");
	edprintf("*******     for the use of SL DSP group only      *******\r\n");

	/*
	 * str1.1+0x0a4a.  0x13c73 loads 0xa70 ("1.1") and stores it into
	 * 0x4(%esp), 0x13c6e loads 0xa67 ("9-Apr-01") into 0x8(%esp): version
	 * first, date second, as in the V.90 file.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(V92_BANNER);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modem Version: %s  (%s)\r\n",
				     "1.1", "9-Apr-01");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(V92_BANNER);

	edprintf("V92Modem Version Description:\r\n");
	edprintf("%s\r\n", "Memory cleanups + dil descriptor crash fix");
	edprintf(V92_BANNER);
}
