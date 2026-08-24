/*
 * V92Modem.cpp -- the top of the V.92 half of the V.PCM construction chain.
 *
 *     V92Modem::V92Modem(V92ModemSide, _tagModemParameters *, unsigned int,
 *                        tagV90DILdescriptor *, V92ComputationalMode)
 *                                       .text+0x13d30 (C1), +0x13ec0 (C2)
 *     V92Modem::~V92Modem()             .text+0x13a80 (D1), +0x13990 (D2)
 *     V92Modem::progress(int *, unsigned int &, float *, unsigned int)
 *                                       .text+0x13b70, 0x79 = 121 bytes
 *     V92Modem::printTitle()            .text+0x13bf0, 0xac = 172 bytes
 *     V92Modem::reset()                 .text+0x13ca0, 0x8a = 138 bytes
 *
 * ALL SIX SYMBOLS OF THE CLASS.  `reset` and `progress` are the class's whole
 * run-time surface and both are three-arm switches on `modemSide` that forward
 * to the modulator; everything else the object does at run time it does one
 * level down.
 *
 * The original translation unit is `V92Modem.cpp`, STT_FILE #25.
 * include/dsplib/V92Modem.h carries the object map, the 0xaac the two
 * embedding displacements in `VPcmFloModem` give, and what proved each field.
 *
 * THE CONSTRUCTOR IS ALMOST ENTIRELY WIRING, which is why it earns a file
 * comment out of proportion to its 393 bytes: it allocates five objects and
 * hands seven arguments to the last of them, and an argument in the wrong
 * position there is invisible in every one of those classes' own tests.
 *
 * C1 AND C2 ARE BYTE-IDENTICAL, all 393 of them; D1 and D2 differ in exactly
 * two bytes, both the scratch register the epilogue pops into.  Neither is a
 * fact about the source.
 *
 * WHY THE SUB-OBJECTS ARE BUILT THROUGH asm() LABELS: the reason
 * src/pump/v90/V92Modulator.cpp and V92Precoder.cpp give in full --
 * `sysdep_malloc(n)` then the constructor with NO null test between them is
 * `new` over an inline `operator new`; this build is -nostdinc++ with no
 * <new>, and a user-declared placement form makes GCC emit the null test the
 * blob does not have.
 *
 * THE FIVE `sysdep_malloc(sizeof(X))` IMMEDIATES ARE THE ORIGINAL COMPILER'S
 * OWN `sizeof`s (finding 1246), and this is the richest single source of them
 * in the V.92 chain: 0xdc, 0x2c, 0x918, 0xb4 and 0x90.  Every one was already
 * pinned by its own class's allocation site and every one agrees, so this
 * file confirms five sizes and invents none.  The assertions below are what
 * makes each `sizeof` a transcription of the blob's literal rather than a
 * hope.
 *
 * THE ILLEGAL-SIDE ARM STORES NOTHING.  Both functions have a third arm for a
 * `modemSide` that is neither 0 nor 1, and in the constructor that arm prints
 * and returns -- it does NOT null the modulator pointer, which therefore
 * keeps whatever the storage held.  Finding 1323, and the test drives it.
 *
 * ONE GUARD IN THE DESTRUCTOR IS UNREACHABLE.  `if (mappingParams)` at
 * .text+0x139da is preceded, two calls earlier, by
 * `V92deleteConstellations(mappingParams)`, which dereferences the same
 * pointer with no null test of its own.  So a null +0xaa0 faults before the
 * guard is read and the guard's false branch cannot be driven by any fixture
 * that does not also fault the blob.  Reproduced as found; finding 1322.
 *
 * WHAT DIFFERS FROM `V90Modem::printTitle`, and both are easy to lose:
 *
 *   - there is no "Components: ..." line here.  Eight messages, not nine.
 *   - THE CLOSING BANNER IS UNGATED.  0x13c41-0x13c4d loads the banner into
 *     the outgoing slot and tail-jumps straight to `edprintf` with no
 *     `cmpl $0x1` in front of it, where the V.90 file tests the level first
 *     and tail-jumps to `dsplibs_debug_printf` instead.
 *
 * THE SOURCE ORDER IS NOT THE DISASSEMBLY ORDER in `printTitle`: GCC moved
 * the gated block to the end, so 0x13bf0 downwards reads 1, 2, 3, 7, 8, 9, 4,
 * 5, 6.  0x13c1e jumps forward to the gated block at 0x13c52 when the level
 * is high, the block jumps back to 0x13c20, and 0x13c20 is message 7.
 *
 * The banner strings are this TU's own copies at .rodata.str1.4+0x34a0,
 * +0x34dc and +0x3518, textually identical to `V90Modem.cpp`'s at 0x416c.
 * Two separate copies in the OBJECT, one after the final link -- see the
 * matching note in V90Modem.cpp for why that is not a contradiction.
 *
 * See docs/findings.md 837 and 1320-1324.
 */

#include "dsplib/V92Modem.h"

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/sysdep.h"

#include "dsplib/V92CP.h"
#include "dsplib/V92DILdescriptorPacker.h"
#include "dsplib/V92Modulator.h"
#include "dsplib/V92ParamsInfo.h"
#include "dsplib/V92Parameters.h"
#include "dsplib/V92Phase2Info.h"

extern "C" {
/*
 * The three complete-object constructors, by the names the relocations at
 * .text+0x13da5, +0x13dcb, +0x13de4 and +0x13eb0 carry.  `void *` throughout
 * for the reason V92Modulator.cpp gives: the call is a relocation against a
 * mangled name and nothing here needs the argument types to be checked twice.
 */
void v92modem_params_ctor(void *self, void *modemParams)
	asm("_ZN13V92ParametersC1EP19_tagModemParameters");
void v92modem_p2i_ctor(void *self, void *params)
	asm("_ZN13V92Phase2InfoC1EP13V92Parameters");
void v92modem_cp_ctor(void *self) asm("_ZN5V92CPC1Ev");
void v92modem_mod_ctor(void *self, unsigned int nSamples, void *phase2Info,
		       void *ja, void *dil, void *cp, void *mappingParams,
		       void *params)
	asm("_ZN12V92ModulatorC1EjP13V92Phase2InfoP5V92JaP19tagV90DILdescriptor"
	    "P5V92CPP16V92MappingParamsP13V92Parameters");
}

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V92MODEM_OFF(field, off, tag) \
	typedef char v92modem_off_##tag[ \
	    ((int)__builtin_offsetof(V92Modem, field) == (off)) ? 1 : -1]

V92MODEM_OFF(modulator,		0x000, modulator);
V92MODEM_OFF(parameters,	0x004, parameters);
V92MODEM_OFF(phase2Info,	0x008, phase2info);
V92MODEM_OFF(ja,		0x00c, ja);
V92MODEM_OFF(dil,		0xa9c, dil);
V92MODEM_OFF(mappingParams,	0xaa0, mappingparams);
V92MODEM_OFF(cp,		0xaa4, cp);
V92MODEM_OFF(modemSide,		0xaa8, modemside);

/*
 * 0x6bd0 - 0x6124 from above and +0xaa8 from below; see the header.  This is
 * the assertion the two bounds exist for.
 */
typedef char v92modem_size[(sizeof(V92Modem) == 0xaac) ? 1 : -1];

/*
 * THE ALLOCATION ORACLE, asserted rather than commented.  Each of these five
 * is a `movl $imm,(%esp)` immediately before a `call sysdep_malloc` whose
 * result is immediately constructed, so the immediate is what the ORIGINAL
 * compiler computed for `sizeof`.  Turning them into compile-time assertions
 * is what stops a later edit to one of those five classes from silently
 * changing what this constructor asks for.
 */
typedef char v92modem_szparams[(sizeof(V92Parameters) == 0xdc) ? 1 : -1];
typedef char v92modem_szp2i[(sizeof(V92Phase2Info) == 0x2c) ? 1 : -1];
typedef char v92modem_szcp[(sizeof(V92CP) == 0x918) ? 1 : -1];
typedef char v92modem_szmp[(sizeof(struct V92ParamsInfo) == 0xb4) ? 1 : -1];
typedef char v92modem_szmod[(sizeof(V92Modulator) == 0x90) ? 1 : -1];

#endif /* __SIZEOF_POINTER__ == 4 */

/* .rodata.str1.4+0x34a0. */
#define V92_BANNER \
	"*********************************************************\r\n"

/*
 * ===========================================================================
 * V92Modem::V92Modem (.text+0x13d30 C1, +0x13ec0 C2, 393 bytes each)
 *
 * THE FIFTH ARGUMENT IS NEVER READ.  Nothing in either copy touches
 * 0x54(%esp), which is where `V92ComputationalMode` lands after the
 * `sub $0x3c,%esp`.  It is named and unused here for the same reason.
 *
 * THE STATEMENT ORDER IS THE OBJECT'S, and for the `dil` store that is
 * measured rather than tidy: `mov %ebp,0xa9c(%esi)` at .text+0x13dad sits
 * BETWEEN the V92Parameters constructor's return and the V92Phase2Info
 * allocation, and GCC cannot move a store to `*this` across either call
 * because either call might alias it.  So the store is where the source put
 * it.
 *
 * THE SWITCH IS ON THE MEMBER, not on the parameter: .text+0x13e17 reloads
 * `0xaa8(%esi)` although the parameter is still live in %ebx.
 * ===========================================================================
 */
V92Modem::V92Modem(V92ModemSide side, _tagModemParameters *modemParams,
		   unsigned int nSamples, tagV90DILdescriptor *dilDescriptor,
		   V92ComputationalMode mode)
{
	void *p;

	(void)mode;

	/*
	 * .rodata.str1.4+0x35c4, and the `%s` is chosen at .text+0x13d64 by
	 * `test %ebx,%ebx` on the PARAMETER -- this runs before +0xaa8 is
	 * written.  Zero selects str1.1+0xa85 "Digital", anything else
	 * str1.1+0xa8d "Analog".
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modem Construction (as %s Modem)\r\n",
				     side == V92_MODEM_SIDE_DIGITAL
				     ? "Digital" : "Analog");

	printTitle();

	modemSide = side;

	p = sysdep_malloc(sizeof(V92Parameters));
	v92modem_params_ctor(p, modemParams);
	parameters = (V92Parameters *)p;

	dil = dilDescriptor;

	/*
	 * The argument is read back out of +0x004 rather than kept in a
	 * register: `mov 0x4(%esi),%ecx` at .text+0x13dbf.
	 */
	p = sysdep_malloc(sizeof(V92Phase2Info));
	v92modem_p2i_ctor(p, parameters);
	phase2Info = (V92Phase2Info *)p;

	p = sysdep_malloc(sizeof(V92CP));
	v92modem_cp_ctor(p);
	cp = (V92CP *)p;

	/*
	 * NO CONSTRUCTOR ON THIS ONE -- a bare allocation and then two C
	 * functions, which is what identifies the block as a struct and not a
	 * class.  The member is stored FIRST (.text+0x13dfb precedes the call
	 * at +0x13e04) and the second call re-reads it, so this is a member
	 * assignment and not a local handed on twice.
	 *
	 * The cast is the `V92MappingParams` / `struct V92ParamsInfo`
	 * identification of finding 1321: one 180-byte block reached from two
	 * directions, the mangling's sixth parameter type and the four C
	 * functions' own.
	 */
	mappingParams = (V92MappingParams *)sysdep_malloc(
	    sizeof(struct V92ParamsInfo));
	V92createConstellations((struct V92ParamsInfo *)mappingParams);
	V92createFilterCoefficients((struct V92ParamsInfo *)mappingParams);

	switch (modemSide) {
	case V92_MODEM_SIDE_DIGITAL:
		modulator = 0;
		break;

	case V92_MODEM_SIDE_ANALOG:
		/*
		 * Seven arguments, and six of the seven are read back out of
		 * the object rather than out of a register: +0x008, `lea
		 * 0xc(%esi)`, +0xaa4, +0xaa0 and +0x004 at .text+0x13e78
		 * onwards.  Only `dil` reaches the call in the register the
		 * parameter arrived in, which is the compiler's choice
		 * between two spellings of the same value.
		 */
		p = sysdep_malloc(sizeof(V92Modulator));
		v92modem_mod_ctor(p, nSamples, phase2Info, ja, dil, cp,
				  mappingParams, parameters);
		modulator = (V92Modulator *)p;
		break;

	default:
		/*
		 * .rodata.str1.4+0x35ec, and the arm is a TAIL CALL: 0x13e3a
		 * stores the string into the slot `this` arrived in and
		 * 0x13e49 jumps.  +0x000 is not written on this path.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V92Modem Constructor: Illegal modemSide\r\n");
		break;
	}
}

/*
 * ===========================================================================
 * V92Modem::~V92Modem (.text+0x13a80 D1, +0x13990 D2, 229 bytes each)
 *
 * FIVE RELEASES AND ONE NULLING.  The four owned objects come back in the
 * order mapping parameters, modulator, CP, parameters, phase 2 info -- which
 * is neither the construction order nor its reverse -- and only the LAST of
 * the five has its pointer written back.  `movl $0x0,0x8(%esi)` at
 * .text+0x13a08 is the destructor's only store, and with -fno-lifetime-dse in
 * CXXFLAGS it survives into our object as it does into the blob's (finding
 * 1272 for why the flag is there).  A reconstruction that nulled all five, or
 * none, disagrees here.
 *
 * THE PHASE 2 INFO IS FREED WITHOUT A DESTRUCTOR CALL, and that is the blob's
 * own statement rather than an omission: it carries no `_ZN13V92Phase2InfoD*`
 * symbol at all.
 *
 * THE RANGE TEST IS UNSIGNED.  `cmpl $0x1,0xaa8(%esi); jbe` at +0x13a92,
 * where a signed `> 1` would have been `jle`; see the header for what that
 * says about the enum's underlying type.
 * ===========================================================================
 */
V92Modem::~V92Modem()
{
	/* .rodata.str1.1+0xa2e. */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modem Destruction\r\n");

	/* .rodata.str1.4+0x344c.  Two separate loads of the debug level, one
	 * per gate, and the side test between them. */
	if (modemSide > V92_MODEM_SIDE_ANALOG) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V92Modem Destructor: Illegal modemSide\r\n");
	}

	/*
	 * UNGUARDED, both of them, and the guard on the free below is
	 * therefore unreachable -- see the file comment and finding 1322.
	 */
	V92deleteConstellations((struct V92ParamsInfo *)mappingParams);
	V92deleteFilterCoefficients((struct V92ParamsInfo *)mappingParams);
	if (mappingParams != 0)
		sysdep_free(mappingParams);

	if (modulator != 0) {
		modulator->~V92Modulator();
		sysdep_free(modulator);
	}
	if (cp != 0) {
		cp->~V92CP();
		sysdep_free(cp);
	}
	if (parameters != 0) {
		parameters->~V92Parameters();
		sysdep_free(parameters);
	}
	if (phase2Info != 0) {
		sysdep_free(phase2Info);
		phase2Info = 0;
	}
}

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

/*
 * ===========================================================================
 * V92Modem::reset (.text+0x13ca0, 138 bytes)
 *
 * The same three-arm switch on `modemSide` the constructor and the destructor
 * carry, with the banner reprinted in front of it.  `printTitle` runs on EVERY
 * side, including the illegal one -- it is above the switch, not inside the
 * analog arm.
 *
 * THE ANALOG ARM IS THREE STATEMENTS AND THE LAST IS A TAIL CALL:
 *
 *     V92DILdescriptorPacker(dil, ja + 4, (int *)ja)
 *     modulator->reset()
 *     modulator->enterPhase3()
 *
 * and `modulator` is RE-READ from +0x000 before each of the last two
 * (`mov (%ebx),%edx` at +0x13d11, `mov (%ebx),%eax` at +0x13d1b), which is
 * what a member access through `this` compiles to across a call that might
 * alias it.
 *
 * THE TWO POINTERS INTO `ja` ARE +0x010 AND +0x00c, in that argument order:
 * `lea 0x10(%ebx),%eax` goes to the packer's `unsigned char *` and
 * `lea 0xc(%ebx),%edx` to its `int *`.  That is V92Ja's `bitCount` at its own
 * +0x000 and its byte vector at +0x004, and it is what pins the `V92Ja` to
 * +0x00c from the second direction -- see the header.
 *
 * THE DIGITAL ARM DOES NOTHING AT ALL, which on the shipped configuration is
 * the whole point: the digital side has no `V92Modulator` to reset because it
 * never built one.
 * ===========================================================================
 */
void
V92Modem::reset()
{
	/* .rodata.str1.1+0xa74. */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modem Reset\r\n");

	printTitle();

	switch (modemSide) {
	case V92_MODEM_SIDE_DIGITAL:
		break;

	case V92_MODEM_SIDE_ANALOG:
		V92DILdescriptorPacker(dil, ja + 4, (int *)ja);
		modulator->reset();
		modulator->enterPhase3();
		break;

	default:
		/* .rodata.str1.4+0x35a0, and a TAIL CALL as the constructor's
		 * illegal arm is. */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V92Modem Reset: Illegal modemSide\r\n");
		break;
	}
}

/*
 * ===========================================================================
 * V92Modem::progress (.text+0x13b70, 121 bytes)
 *
 * 121 bytes of which 15 do anything: the switch, and a forward of all four
 * arguments to the modulator.  There is no banner here and no `printTitle`.
 *
 * THE ANALOG ARM IS A TAIL CALL WITH THE ARGUMENTS LEFT WHERE THEY ARE --
 * `mov (%eax),%eax; mov %eax,0x10(%esp); jmp V92Modulator::progress` at
 * +0x13bd0 -- so only `this` is rewritten and the other four stay in the
 * caller's own slots.  That is what says the two signatures are identical and
 * in the same order, and it is also what says both return `void`.
 *
 * THE DIGITAL ARM RETURNS WITHOUT SO MUCH AS A MESSAGE, which is not an
 * oversight: `vpcm_create` passes NULL to `VPCMXF_Create` and that selects the
 * DEMODULATOR, so it is the V.90 side that is dead in the shipped object.  V.92
 * upstream PCM has the ANALOGUE client transmitting, so this arm is the live
 * one here and the empty digital arm is the counterpart of the V.90 file's
 * empty analogue one.
 * ===========================================================================
 */
void
V92Modem::progress(int *bits, unsigned int &nbits, float *out,
		   unsigned int nSamples)
{
	switch (modemSide) {
	case V92_MODEM_SIDE_DIGITAL:
		break;

	case V92_MODEM_SIDE_ANALOG:
		modulator->progress(bits, nbits, out, nSamples);
		break;

	default:
		/* .rodata.str1.4+0x3478. */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V92Modem progress: Illegal modemSide\r\n");
		break;
	}
}
