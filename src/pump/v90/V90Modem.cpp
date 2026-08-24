/*
 * V90Modem.cpp -- `V90Modem::printTitle`, 0x19400, 0xd2 = 210 bytes.
 *
 * The original translation unit is `V90Modem.cpp`, STT_FILE #31, and this
 * file holds the one member of it that has been reconstructed.  Everything
 * else in that TU -- the constructor at 0x194e0, the destructors, `progress`,
 * `reset` -- is still missing, and include/dsplib/V90Modem.h says at length
 * why the class declaration there is not an object map.
 *
 * THE SOURCE ORDER IS NOT THE DISASSEMBLY ORDER.  GCC split the body on the
 * first `dsplibs_debug_level` test and put the gated block at the END of the
 * function, so reading 0x19400 downwards gives messages 1, 2, 3, 7, 8, 9,
 * 10, 4, 5, 6.  The order below is the one the branches actually take:
 * 0x1942e jumps FORWARD to the gated block at 0x19481 when the level is high
 * and falls THROUGH to message 7 at 0x19430 when it is not, and the gated
 * block ends by jumping back to 0x19430.  Reading the layout instead of the
 * edges would put the version line after the description, which is the wrong
 * transcript.
 *
 * NINE CALLS THROUGH `edprintf` AND FOUR GATES.  Every gate is the object's
 * usual `cmpl $0x1` -- `DSPLIB_DEBUG_ON()` -- and it is re-read at each one
 * (0x19427, 0x1945d, 0x1948d, 0x194b4) rather than tested once, which is what
 * a macro per call site compiles to and what the reconstruction must emit if
 * the branch structure is to match.
 *
 * `printTitle` NEVER TOUCHES `this`, so nothing here reads a member and the
 * member-ness of the function is not observable.  0x19475 writes a string
 * pointer into the incoming argument slot before the tail jump, which is only
 * correct because the value there is already dead.
 *
 * The banner strings are this TU's own copies in `.rodata.str1.4` at 0x416c,
 * 0x41a8 and 0x41e4 -- `V92Modem.cpp` has an identical set at 0x34a0, 0x34dc
 * and 0x3518, which is what two translation units each spelling out the same
 * literal looks like after `ld -r`.  Do not factor them into a shared header:
 * the reason is which TU the original put the literal in, and NOT how many
 * copies survive, because `.rodata.str1.4` is `SHF_MERGE|SHF_STRINGS` and the
 * final link folds every copy -- ours and the blob's -- into one.  Nothing in
 * the transcript tier can see it either way.
 *
 * See docs/findings.md 837.
 */

#include "dsplib/V90Modem.h"

#include "dsplib/debug.h"
#include "dsplib/encode.h"

/*
 * `progress` DEREFERENCES BOTH SIDE POINTERS, so this translation unit is one
 * of the ones V90Modem.h's forward-declaration note has in mind: it needs both
 * complete types and includes them itself rather than putting them in the
 * header, which is included by VPcmFloModem.h and would decide the question
 * for every user of that (findings 1112 and 1325).
 */
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90Modulator.h"

/*
 * .rodata.str1.4+0x416c.  Fifty-seven asterisks and a CRLF; counted from the
 * hex dump rather than typed until it looked right.
 */
#define V90_BANNER \
	"*********************************************************\r\n"

void
V90Modem::printTitle()
{
	edprintf(V90_BANNER);
	edprintf("*******         This is a PRIVATE version         *******\r\n");
	edprintf("*******     for the use of SL DSP group only      *******\r\n");

	/*
	 * str1.1+0x105d, with the arguments in the order the object pushes
	 * them: 0x1084 ("2.98") into 0x4(%esp) and 0x107a ("25-Mar-04") into
	 * 0x8(%esp), so the VERSION is first and the DATE second.  They are
	 * two separate string literals and the disassembly loads them into
	 * two registers before storing both, which is the only reason the
	 * order is unambiguous.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(V90_BANNER);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modem Version: %s  (%s)\r\n",
				     "2.98", "25-Mar-04");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(V90_BANNER);

	edprintf("V90Modem Version Description:\r\n");
	edprintf("%s\r\n",
		 "Modified Quick Connect without Memory + Train Time + "
		 "Constel Power");
	edprintf("Components: Floreat, ADI, ACD, New BLL\r\n");

	/*
	 * AND THIS ONE IS GATED, where `V92Modem::printTitle`'s closing banner
	 * is not.  0x19464 tests the level and 0x1947c tail-jumps to
	 * `dsplibs_debug_printf`; the V.92 file's 0x13c4d tail-jumps to
	 * `edprintf` with no test at all.  The two functions are otherwise the
	 * same shape, so this is exactly the kind of difference a
	 * copy-and-edit reconstruction loses.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(V90_BANNER);
}

/*
 * ===========================================================================
 * `V90Modem::progress` -- .text+0x19ad0, 188 bytes
 *
 * THE SIDE SWITCH AND NOTHING ELSE.  188 bytes of which 150 are the three
 * arms' argument shuffles: the frame is set up, `side` is loaded once from
 * +0x49bc, and each arm writes the four incoming words back into the same
 * stack slots it found them in before tail-JUMPING to its callee.  That is
 * what GCC emits for a sibling call whose argument list is the caller's own,
 * and it is why all three exits are `jmp` and none is `call`.
 *
 * THE SWITCH IS THE CONSTRUCTOR'S, INSTRUCTION FOR INSTRUCTION.  `test %edx,
 * %edx ; je` then `dec %edx ; je` then fall through, over an UNSIGNED
 * `V90ModemSide` -- the same three-way shape V90ModemCtor.cpp has and the
 * same one the destructor's `cmpl $0x1 ; jbe` range test agrees with.
 *
 * `V90Modem` DOES NOT RESET ITS SIDE OBJECT.  Neither arm touches anything
 * but the pointer it forwards through, so the state every one of these calls
 * depends on -- `V90Modulator::state`, `symbolCount`, `eventCode` -- is
 * whatever the last `reset` left, and this class has no member that calls
 * one.  On the digital arm that reset is `vPcmResetPhase3Modem`'s, which is
 * outside this translation unit and is not written yet.  Finding 7514.
 * ===========================================================================
 */
void
V90Modem::progress(int *bits, unsigned int &nofBits, float *samples,
		   unsigned int nofSymbols)
{
	switch (side) {
	case V90_MODEM_SIDE_DIGITAL:
		modulator->progress(bits, nofBits, samples, nofSymbols);
		break;

	case V90_MODEM_SIDE_ANALOG:
		demodulator->progress(bits, nofBits, samples, nofSymbols);
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Modem progress: Illegal "
					     "modemSide\r\n");
		break;
	}
}
