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
#include "dsplib/sysdep.h"
#include "dsplib/modem_params.h"
#include "dsplib/V90CodecType.h"
#include "dsplib/V90Jd.h"
#include "dsplib/V90Phase2Info.h"
#include "dsplib/V92Jd.h"

/* F11621: typed owned deletion retains one pointer across both calls. */
inline void operator delete(void *p) { sysdep_free(p); }

/*
 * .rodata.str1.4+0x416c.  Fifty-seven asterisks and a CRLF; counted from the
 * hex dump rather than typed until it looked right.
 */
#define V90_BANNER \
	"*********************************************************\r\n"

/*
 * The destructor. F11621: all five typed owners use ordinary scalar delete;
 * phase2Info stays a bare host free, and CP/MP destruction is generated.
 *
 * SIX POINTERS IN FORWARD ORDER, then the two embedded members in reverse.
 * The forward order is the giveaway that the six are statements in the body
 * and not member destruction -- a compiler-generated one would run them
 * backwards, as it does for `cp` and `mp` below, which are not written here.
 *
 * `phase2Info` IS FREED WITHOUT A DESTRUCTOR CALL: 0x192ed tests it and
 * 0x193a0 goes straight to `sysdep_free` with no `_ZN13V90Phase2InfoD` in
 * between.  So the class is trivially destructible, which is what
 * V90Phase2Info.h describes, and a `delete`-shaped statement here would emit
 * exactly that.
 */
V90Modem::~V90Modem()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modem Destruction\r\n");

	/*
	 * `cmpl $0x1,0x49bc(%esi); jbe` -- UNSIGNED, which is why
	 * `V90ModemSide` has an `unsigned int` base.  See V90Modem.h.
	 */
	if (side > V90_MODEM_SIDE_ANALOG) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90Modem Destructor: Illegal modemSide\r\n");
	}

	delete modulator;
	delete demodulator;
	if (phase2Info != 0)
		sysdep_free(phase2Info);
	delete jd;
	delete jd92;
	delete params;
}

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

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char v90m_dem_size[(sizeof(V90Demodulator) == 0x298) ? 1 : -1];
#endif

/*
 * The six nested constructors used to be reached by their mangled names,
 * `void *` throughout, on the belief (finding F1340) that a user-declared
 * placement `operator new` -- needed here since this build is `-nostdinc++`
 * with no `<new>` -- would force GCC to emit a null test the blob does not
 * have between `sysdep_malloc` and the constructor call.  Finding F10155
 * retracts that: the null check is tied to the placement `operator new`
 * being declared `throw()`, which is not this project's, and
 * `include/dsplib/sysdep.h` declares the shared non-throw pair every such
 * site needs.  Genuine placement `new` and explicit destructor calls are
 * used below instead; see finding F10157 for the site that proved this
 * mechanism end-to-end.
 */

/*
 * Hold the compiler to the map in the header.  Guarded on a 32-bit pointer
 * because eight of the offsets are pointers and `make check64` lays them out
 * differently -- the layout the blob has is a 32-bit layout, and asserting it
 * on a host that cannot have it is asserting the wrong thing.
 *
 * `sizeof(V90Modem)` is NOT asserted here: src/pump/v90/VpcmFloModem.cpp
 * already asserts it, and it settled the number before any of these fields
 * existed.  A second copy would look like a second measurement.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90M_OFF(field, off, tag) \
	typedef char v90m_off_##tag[ \
		((int)__builtin_offsetof(V90Modem, field) == (off)) ? 1 : -1]

V90M_OFF(modulator,		0x0000, modulator);
V90M_OFF(demodulator,		0x0004, demodulator);
V90M_OFF(phase2Info,		0x0008, phase2info);
V90M_OFF(jd,			0x000c, jd);
V90M_OFF(jd92,			0x0010, jd92);
V90M_OFF(dil,			0x0014, dil);
V90M_OFF(mappingParams,		0x0018, mapparams);
V90M_OFF(mappingParamsAlt,	0x0668, mapparamsalt);
V90M_OFF(additionalCPinfo,	0x0cb8, cpinfo);
V90M_OFF(mp,			0x0cd0, mp);
V90M_OFF(cp,			0x0df4, cp);
V90M_OFF(params,		0x49b4, ptr49b4);
V90M_OFF(sessionFlag,		0x49b8, sessionflag);
V90M_OFF(side,			0x49bc, side);

/*
 * The four sizes this file's allocations depend on, asserted where they are
 * used rather than only in the classes' own translation units.  A batch that
 * moved a field of any of them would otherwise change what this constructor
 * allocates without changing a line of it.
 */
typedef char v90m_parm_size[(sizeof(V90Parameters) == 0x558) ? 1 : -1];
typedef char v90m_ph2_size[(sizeof(V90Phase2Info) == 0x24) ? 1 : -1];
typedef char v90m_jd_size[(sizeof(V90Jd) == 0x90) ? 1 : -1];
typedef char v90m_jd92_size[(sizeof(V92Jd) == 0xdc) ? 1 : -1];
typedef char v90m_mod_size[(sizeof(V90Modulator) == 0x70) ? 1 : -1];
#endif

/*
 * The constructor.
 *
 * TWO MEMBER CONSTRUCTIONS COME FIRST AND THEY ARE NOT STATEMENTS: `V90MP`
 * at +0xcd0 and `V90CP` at +0xdf4 are called before the first diagnostic, in
 * DECLARATION order, which is what a member with a default constructor and no
 * mem-initialiser gives.  The destructor runs them in the opposite order,
 * `~V90CP` then `~V90MP`, which is the other half of the same evidence.
 * Neither appears in the body below because neither is written there.
 *
 * THE SWITCH IS ON THE MEMBER, NOT THE PARAMETER.  0x195de reloads
 * `0x49bc(%esi)` rather than reusing the register it stored from, so the
 * source reads `this->side` back.  V92Modem's constructor does the same
 * (V92Modem.h), and it is the kind of thing that survives only if it is
 * written the way the object has it.
 *
 * ELEVEN BYTES CAME OFF THIS FUNCTION AS TWO INDEPENDENT DECODINGS, and the
 * enumeration was run jointly over both because nothing before the compile
 * said they were independent.  Twenty-four cells: four spellings of the
 * diagnostic's ternary crossed with all 3! orders of `side`, `sessionFlag`
 * and `dil`.  Differing bytes of 597:
 *
 *                        side dil flag  side flag dil  dil side flag
 *     side ? A : D            11             1              11
 *     side == 0 ? D : A       10             0  <--         10
 *     !side ? D : A           10             0  <--         10
 *     side != 0 ? A : D       11             1              11
 *
 *                        dil flag side  flag side dil  flag dil side
 *     side ? A : D            11             1              1
 *     side == 0 ? D : A       10             0  <--         0  <--
 *     !side ? D : A           10             0  <--         0  <--
 *     side != 0 ? A : D       11             1              1
 *
 * The table SEPARATES, which is itself the finding: the ternary axis moves
 * every cell by exactly one byte and the store axis by ten, so the two
 * differences are independent and neither is a consequence of the other.
 *
 * **SIX CELLS REACH ZERO, so neither axis has a unique preimage** and 7771's
 * rule applies to both.  What each axis DOES decode is sharp, and it is the
 * negative half that carries it:
 *
 *   - the condition tests for ZERO, with "Digital" as the true arm.  Every
 *     cell testing for non-zero is off by that byte.  `== 0` and `!` are the
 *     same expression to GCC 3.4.2 and the object cannot separate them.
 *   - `dil = dilDescriptor;` is stored LAST.  Every cell with `dil` ahead of
 *     `sessionFlag` costs ten bytes; the relative order of `side` and
 *     `sessionFlag` is not observable and all three cells that put `dil`
 *     last are exact.
 *
 * `side` is left first and `== 0` chosen over `!` because those are the
 * smaller edits, not because the object prefers them.
 */
V90Modem::V90Modem(V90ModemSide modemSide, _tagModemParameters *modemParams,
		   tagV90DILdescriptor *dilDescriptor, unsigned int nofSymbols,
		   V90ComputationalMode compMode, unsigned int flag)
{
	void *p;

	/*
	 * "Digital" when the argument is zero and "Analog" otherwise -- and
	 * the test is on the ARGUMENT here, before anything is stored, where
	 * the switch below is on the member.  0x19514 tests `%ebx`, which is
	 * still `0x54(%esp)`.
	 *
	 * THE CONDITION IS WRITTEN AS A TEST FOR ZERO, WITH "Digital" AS THE
	 * TRUE ARM, and that is decoded rather than transcribed -- see the
	 * table above the constructor.  Written `modemSide ? "Analog" :
	 * "Digital"` the object's `je` comes out as `jne`, one byte.
	 *
	 * AND THE .rodata LAYOUT IS AN INDEPENDENT WITNESS, from a different
	 * observable than the branch byte: the blob's two strings are at
	 * `.rodata.str1.1+0x1089` and `+0x1091`, eight bytes apart, so
	 * "Digital" is the EARLIER of the two -- and GCC 3.4.2 emits string
	 * literals in the order the source mentions them.  The spelling that
	 * fixes the branch is the same one that puts "Digital" first.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90Modem Construction (as %s Modem)\r\n",
				     modemSide == 0 ? "Digital" : "Analog");

	printTitle();

	side = modemSide;
	sessionFlag = flag;
	dil = dilDescriptor;

	p = sysdep_malloc(sizeof(V90Parameters));
	new (p) V90Parameters(modemParams);
	params = (V90Parameters *)p;

	/*
	 * The V90Parameters pointer is read back OUT OF THE OBJECT for each
	 * of the next three, not kept in a register: `mov 0x49b4(%esi),%ecx`
	 * at 0x19581, `%edx` at 0x195a4 and `%eax` at 0x195c9.  Three
	 * reloads, three statements.
	 */
	p = sysdep_malloc(sizeof(V90Phase2Info));
	new (p) V90Phase2Info(params);
	phase2Info = (V90Phase2Info *)p;

	p = sysdep_malloc(sizeof(V90Jd));
	new (p) V90Jd(params);
	jd = (V90Jd *)p;

	p = sysdep_malloc(sizeof(V92Jd));
	new (p) V92Jd(params);
	jd92 = (V92Jd *)p;

	/*
	 * NEITHER ARM IS THE DEFAULT AND THE DEFAULT WRITES NOTHING.  On any
	 * value but 0 and 1 this returns with `modulator` and `demodulator`
	 * holding whatever the storage held -- see V90Modem.h, and finding
	 * F1323 for V92Modem's identical shape.
	 *
	 * THE TWO ARMS WRITE THEIR NULL AT OPPOSITE ENDS, and that is the
	 * object's order rather than a tidy-up: on the modulator arm
	 * `movl $0x0,0x4(%esi)` at 0x19677 comes AFTER the construction, and
	 * on the demodulator arm `movl $0x0,(%esi)` at 0x196a0 comes BEFORE
	 * the allocation.  Neither store can be moved across the constructor
	 * call, which might alias `*this`, so both are where the source put
	 * them.
	 */
	switch (side) {
	case V90_MODEM_SIDE_DIGITAL:
		p = sysdep_malloc(sizeof(V90Modulator));
		new (p) V90Modulator(nofSymbols, phase2Info, jd, jd92, dil,
				     &mappingParams, &mappingParamsAlt,
				     &additionalCPinfo, &cp, &mp, params,
				     sessionFlag);
		modulator = (V90Modulator *)p;
		demodulator = 0;
		break;

	case V90_MODEM_SIDE_ANALOG:
		modulator = 0;
		p = sysdep_malloc(sizeof(V90Demodulator));
		new (p) V90Demodulator(nofSymbols, phase2Info, jd, jd92, dil,
				       &mappingParams, &mappingParamsAlt,
				       &additionalCPinfo, &cp, &mp,
				       (__tHardwareCodecTypes__)modemParams->codecType,
				       params, compMode, sessionFlag);
		demodulator = (V90Demodulator *)p;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90Modem Constructor: Illegal modemSide\r\n");
		break;
	}
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
