/*
 * V90Modem.cpp -- `V90Modem::printTitle`, 0x19400, 0xd2 = 210 bytes, and
 * `V90Modem::progress`, 0x19ad0, 0xbc = 188 bytes.
 *
 * ...and `V90Modem::reset`, 0x199a0, 0xdd = 221 bytes.
 *
 * The original translation unit is `V90Modem.cpp`, STT_FILE #31.  The
 * constructor and the destructors live in src/pump/v90/V90ModemCtor.cpp.
 * THE TU IS NOW COMPLETE: `reset` was the last member missing, and it is the
 * object's ONLY caller of `V90Modulator::reset`, so until it was written
 * nothing this tree built could put the modulator's `state` into a defined
 * condition.
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
 * for every user of that (findings F1112 and F1325).
 */
#include "dsplib/V90Demodulator.h"
#include "dsplib/V90Modulator.h"

/*
 * `reset` names a `DilType` and dereferences `params`, so it needs both of
 * these where `printTitle` and `progress` needed neither.  The DIL header
 * pulls in `V90Phase3Modulator.h` for `tagV90DILdescriptor`, which
 * V90Modem.h only forward-declares.
 */
#include "dsplib/V90DilDescriptorSettings.h"
#include "dsplib/V90Parameters.h"

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
		dsplibs_debug_printf("V90Modem Version: %s  (%s)\r\n", "2.98", "25-Mar-04");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(V90_BANNER);

	edprintf("V90Modem Version Description:\r\n");
	edprintf("%s\r\n", "Modified Quick Connect without Memory + Train Time + "
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
 * `V90Modem::reset` -- .text+0x199a0, 221 bytes
 *
 * PLACED HERE BECAUSE THE OBJECT PLACES IT HERE: 0x19400 `printTitle`,
 * 0x199a0 `reset`, 0x19a80 `setSessionFlag`, 0x19ad0 `progress`.
 * `setSessionFlag` is in V90SessionFlag.cpp for its own reasons and the rest
 * of this file follows .text.
 *
 * THE SHAPE IS `progress`'s, WITH ONE ARM DOING WORK.  Same gated banner,
 * same three-way `side` test over an unsigned `V90ModemSide` (`test`/`je`,
 * `dec`/`je`, fall through), same tail jumps out of both live arms and out of
 * the default's `dsplibs_debug_printf`.
 *
 * THE ANALOGUE ARM'S TWO STATEMENTS ARE ORDERED AND THE ORDER IS FORCED.
 * `PROBING_MODE` is read at 0x19a12, before anything else in the arm, and its
 * non-zero exit at 0x19a6d zeroes `%esi` -- the register holding `qcFlag` --
 * and then rejoins the common path at 0x19a28, which is the `xor %eax,%eax`
 * that makes the `DilType` argument zero.  So the mask is applied to the
 * VARIABLE and both later readers see it: a reconstruction that passed the
 * original argument to `V90Demodulator::reset` would agree with the object on
 * the descriptor and differ on the demodulator, and only a fixture that
 * drives `PROBING_MODE` non-zero can tell.
 *
 * THE `DilType` SELECT IS AN `if`/`else` AND NOT A TERNARY, AND THAT IS
 * MEASURED RATHER THAN PREFERRED.  Both spellings mean the same thing and
 * GCC 3.4.2 compiles them differently, so the object can tell them apart:
 *
 *   ternary    xor %edx,%edx ; test %esi,%esi ; setne %dl      3 instructions
 *   if/else    test %esi,%esi ; mov $0x1,%eax ; jne ; xor %eax,%eax
 *
 * and the object's 0x19a1f..0x19a28 is the second, byte for byte.  The
 * ternary version came out at 59 instructions against the blob's 60 and the
 * if/else at 61; the count alone would have preferred the wrong one, and what
 * decides is that the four instructions match.  CLAUDE.md's forced column.
 *
 * WHAT THE REMAINING +1 IS, and it is the compiler's free choice.  The masked
 * path ends `xor %eax,%eax ; jmp` in ours and plain `jmp` in the object,
 * because the object CROSS-JUMPS it into the `else` arm's own `xor` at
 * 0x19a28 where ours materialises a second copy.  Both paths reach
 * `mov %eax,0x4(%esp)` with `%eax` zero and every instruction either side is
 * identical; it is basic-block placement, which is 617's free column, and it
 * is not chased.
 *
 * WHY EITHER OF THEM CAN FOLD THE MASKED PATH AT ALL: `qcFlag = 0` is a real
 * assignment to the variable, so on that path the compiler has already proved
 * the select's condition false.  That is a consequence of the source below
 * and not an extra statement in it.
 *
 * `printTitle()` RUNS ON EVERY SIDE, including the illegal one, because it is
 * called at 0x199bf before `side` is loaded at 0x199c4.
 * ===========================================================================
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define SF_OFF_MODEM(cls, field, off, tag) \
	typedef char sf_off_modem_##tag[ \
	    ((int)__builtin_offsetof(cls, field) == (off)) ? 1 : -1]
SF_OFF_MODEM(V90Modem, modulator, 0x0000, mdm_mod);
SF_OFF_MODEM(V90Modem, demodulator, 0x0004, mdm_dem);
SF_OFF_MODEM(V90Modem, phase2Info, 0x0008, mdm_p2i);
SF_OFF_MODEM(V90Modem, params, 0x49b4, mdm_49b4);
SF_OFF_MODEM(V90Modem, sessionFlag, 0x49b8, mdm_flag);
SF_OFF_MODEM(V90Modem, side, 0x49bc, mdm_side);
#undef SF_OFF_MODEM
#endif

void
V90Modem::reset(unsigned int qcFlag)
{
	DilType dilType;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modem Reset, qcFlag = %d\r\n",
				     qcFlag);

	printTitle();

	switch (side) {
	case V90_MODEM_SIDE_DIGITAL:
		modulator->reset();
		break;

	case V90_MODEM_SIDE_ANALOG:
		if (params->PROBING_MODE) {
			edprintf("due to probe mode quick connect is " "masked !!!\r\n");
			qcFlag = 0;
		}

		if (qcFlag)
			dilType = DIL_TYPE_ADI_QC;
		else
			dilType = DIL_TYPE_ADI;

		setDilDescriptor(dil, dilType);
		demodulator->reset(qcFlag);
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Modem Reset: Illegal " "modemSide\r\n");
		break;
	}
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
 * WHICH IS WHY THE ARMS ARE AN `if`/`else if` CHAIN AND NOT A `switch`.  This
 * was a `switch` and it emitted two `jmp`s and a `call`: a `break` out of a
 * `switch` puts the `dsplibs_debug_printf` in the default arm out of tail
 * position under GCC 3.4.2, and the frame grows from the object's 0xc to 0x2c
 * to hold the outgoing argument.  The chain below emits all three `jmp`s and
 * the 0xc frame.  It is still 172 bytes against the object's 188 -- the four
 * `mov`s at 0x19af1..0x19b04 that write the incoming words back into their own
 * slots are ones GCC elides for us, and that residual is not understood -- but
 * the exits are now the object's.  Findings F7982 and F7983.
 *
 * THE SWITCH IS THE CONSTRUCTOR'S, INSTRUCTION FOR INSTRUCTION.  `test %edx,
 * %edx ; je` then `dec %edx ; je` then fall through, over an UNSIGNED
 * `V90ModemSide` -- the same three-way shape V90ModemCtor.cpp has and the
 * same one the destructor's `cmpl $0x1 ; jbe` range test agrees with.
 *
 * `V90Modem::progress` DOES NOT RESET ITS SIDE OBJECT.  Neither arm touches
 * anything but the pointer it forwards through, so the state every one of
 * these calls depends on -- `V90Modulator::state`, `symbolCount`, `eventCode`
 * -- is whatever the last `reset` and the last phase edge left.  The caller
 * of `V90Modulator::reset` is `V90Modem::reset`, in this same translation
 * unit and not written; the member that first sets `state` to 1 is
 * `V90Modulator::enterPhase3`, also not written.  Finding F7520.
 * ===========================================================================
 */
/*
 * `mov 0x49bc(%eax),%edx` is read BEFORE the store to +0x49b8, which matters
 * only if the two could alias and they cannot -- they are distinct members of
 * one object.  Written in the order the object reads them anyway.
 *
 * The flag is stored on every path, including the one that calls nothing.
 */
void
V90Modem::setSessionFlag(unsigned int flag)
{
	int which = side;

	sessionFlag = flag;

	if (which == 0)
		modulator->setSessionFlag(flag);
	else if (which == 1)
		demodulator->setSessionFlag(flag);
}

void
V90Modem::progress(int *bits, unsigned int &nofBits, float *samples,
		   unsigned int nofSymbols)
{
	if (side == V90_MODEM_SIDE_DIGITAL) {
		modulator->progress(bits, nofBits, samples, nofSymbols);
	} else if (side == V90_MODEM_SIDE_ANALOG) {
		demodulator->progress(bits, nofBits, samples, nofSymbols);
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90Modem progress: Illegal " "modemSide\r\n");
	}
}
