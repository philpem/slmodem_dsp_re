/*
 * VPcmXfCreate.cpp -- `VPCMXF_Create` (0xfcf0, 0x1ef = 495 bytes) and
 * `VPCMXF_Delete` (0xf6c0, 0x6d = 109 bytes): the V.PCM interface's
 * constructor and destructor for a `VPcmFloModem`.
 *
 * Both are `extern "C"` in the object -- their relocations carry no mangling
 * -- and `vpcm_create` and `vpcm_delete`, which are C, call them across that
 * boundary.  See CLAUDE.md's note on the link line for the three conditions
 * that makes legal; these two meet all three (extern "C", free functions,
 * and `$(CXXOBJ64)` is on the interop link line).
 *
 * ===========================================================================
 * FIVE ARGUMENTS AND ONLY THE FIRST TWO ARE OBVIOUS
 * ===========================================================================
 *
 *   digitalSide  Tested for zero three times and never stored.  It picks the
 *                message's "%s", and it is INVERTED on its way into the
 *                modem: `sete %al` at 0xfd07 puts `(digitalSide == 0)` into
 *                the V90ModemSide slot.  So a zero here is the ANALOG side,
 *                which is the one `vpcm_create` always asks for -- it passes
 *                a literal 0 (findings F701, F702).
 *   v34Object    Passed straight through to the modem's first argument.
 *   dpRuntime    Likewise, to its third.
 *   durationMs   A DURATION IN MILLISECONDS, converted to a sample count
 *                below; see the next block.
 *   mode         A two-bit mask, unpacked into the two computational modes.
 *
 * ===========================================================================
 * WHY THE CONVERSION IS TWO DIFFERENT MULTIPLIES
 * ===========================================================================
 *
 * The object has two paths into one tail:
 *
 *     digitalSide != 0   fmuls .rodata.cst4+0x54    (8.0)
 *     digitalSide == 0   fmull .rodata.cst8+0x10    (9.6)
 *     both               fadds .rodata.cst4+0x58    (0.5), then a truncating
 *                        fistpl with the rounding control forced to 11
 *
 * 8.0 samples per millisecond is 8000 Hz and 9.6 is 9600 Hz, which are the
 * codec rate and the V.PCM rate; `vpcm_create` requires its `srate` to be
 * exactly 9600 and passes 0 here, so the shipped path is the second one and
 * a `max_frag` of 48 comes back out as 48.  The `+ 0.5` before a TRUNCATION
 * is a round-to-nearest for a non-negative value.
 *
 * THE TWO CONSTANTS ARE WRITTEN `8.0` AND `9.6`, both `double`, and the
 * narrowing to `fmuls` is the compiler's: GCC loads a `double` constant that
 * is exactly representable as a `float` with the four-byte form, which is
 * also why the shared `+ 0.5` is an `fadds` and not an `faddl`.  Writing
 * `8.0f` would say something about the source that the object does not.
 *
 * `fildll` with the high word ZEROED is what types the argument: the value is
 * widened to 64 bits with no sign extension, which is an `unsigned int`
 * conversion and not an `int` one.
 *
 * ===========================================================================
 * THE NULL TEST IS AFTER THE CONSTRUCTION, NOT BEFORE IT
 * ===========================================================================
 *
 * 0xfd9f allocates, 0xfdcd constructs into whatever came back, and 0xfdd2
 * `test %ebx,%ebx` is the first look at the pointer.  So a failed allocation
 * is constructed into before it is noticed -- the object faults and then
 * prints.  Reproduced exactly; the guard is not moved to where it would have
 * done some good, because moving it would be a different function and the
 * blob is the specification.  Recorded as D236.
 *
 * ===========================================================================
 * TWENTY-ONE STORES THE CALLER MAKES INTO THE OBJECT IT JUST BUILT
 * ===========================================================================
 *
 * And five of them CONTRADICT the constructor, which is the interesting part:
 * `nofBitsPerSymbol` is 0 from the constructor and 2 from here,
 * `minNofTransmitSequences` is 0 and then 1, and `v34BaudAllow` is
 * {1,1,1,1,1,1} and then {1,0,1,1,1,0}.  A constructed-and-returned
 * `VPcmFloModem` therefore never has the constructor's values for those, and
 * a test that drove only the constructor would be measuring a state the
 * program never sees.  Both are written as found.
 *
 * The pattern here is `VPcmFloModem::externalReset`'s -- the same six flags,
 * the same five cleared bytes, the same three CP fields (VPcmFloModem.h).  It
 * is NOT a call to it: `externalReset` also re-initialises both parameter
 * blocks and prints, and does neither here.  The duplication is the
 * original's.  This file exists partly because of it: finding F1264, one
 * source file is one mutation suite's namespace, and putting a near-copy of
 * `externalReset`'s tail into `VPcmFloModem.cpp` would make anchors in both
 * match twice -- which `tools/mutate.py` calls UNUSABLE, and unusable does
 * not fail a run.
 */

#include <stddef.h>

#include "dsplib/VPcmFloModem.h"

#include "dsplib/debug.h"
#include "dsplib/sysdep.h"

/*
 * No header carries these two prototypes as `VPcmFloModem *`: the only
 * callers in the object are `vpcm_create` and `vpcm_delete`, which are C and
 * see them through `include/dsplib/vpcm.h` as `void *`.  They are declared
 * here so that the definitions below are not their first declaration --
 * src/pump/v90/VPcmXfTerm.cpp's rule, and the same shape.
 */
extern "C" VPcmFloModem *VPCMXF_Create(int digitalSide, void *v34Object,
				       _tagModemParameters *dpRuntime,
				       unsigned int durationMs, int mode);
extern "C" void VPCMXF_Delete(VPcmFloModem *self);

/*
 * The complete-object constructor, by the name the relocation at 0xfdcd
 * carries.  NOT placement `new`: the blob allocates and then constructs with
 * NOTHING between the two instructions, and a user-declared placement
 * `operator new` -- which is what this build would need, being `-nostdinc++`
 * with no <new> -- makes GCC emit a null test in front of the constructor
 * call.  The object's null test is AFTER it, which is a different function.
 * src/pump/v90/V92Modem.cpp and V92Modulator.cpp give the same reason at
 * length; this is the one site in the chain where the difference would have
 * been visible in the control flow rather than only in the instruction count.
 */
void vpcmxf_modem_ctor(void *self, void *v34Object,
				    V90ModemSide side, void *dpRuntime,
				    unsigned int nSamples,
				    V90ComputationalMode v90Mode,
				    V92ComputationalMode v92Mode)
	asm("_ZN12VPcmFloModemC1EPv12V90ModemSideP19_tagModemParametersj"
	    "20V90ComputationalMode20V92ComputationalMode");

extern "C" VPcmFloModem *
VPCMXF_Create(int digitalSide, void *v34Object,
	      _tagModemParameters *dpRuntime, unsigned int durationMs,
	      int mode)
{
	VPcmFloModem *self;
	V90ModemSide side;
	V90ComputationalMode v90Mode;
	V92ComputationalMode v92Mode;
	int maxDataBuffer;

	side = (V90ModemSide)(digitalSide == 0);

	if (digitalSide != 0)
		maxDataBuffer = (int)(durationMs * 8.0 + 0.5);
	else
		maxDataBuffer = (int)(durationMs * 9.6 + 0.5);

	/*
	 * A two-bit mask and a four-arm switch: bit 0 is the V.90 mode and
	 * bit 1 the V.92 one.  The object writes it as four arms and not as
	 * two shifts -- `cmp $0x2` / `jg` / `cmp $0x3` / `dec` / `je` -- so
	 * the source is a switch, and the `jg` is SIGNED, which is what makes
	 * `mode` an `int`.  Anything outside 1..3, including a negative, is
	 * the default and gives both modes zero.
	 */
	switch (mode) {
	case 1:
		v90Mode = (V90ComputationalMode)1;
		v92Mode = (V92ComputationalMode)0;
		break;
	case 2:
		v90Mode = (V90ComputationalMode)0;
		v92Mode = (V92ComputationalMode)1;
		break;
	case 3:
		v90Mode = (V90ComputationalMode)1;
		v92Mode = (V92ComputationalMode)1;
		break;
	default:
		v90Mode = (V90ComputationalMode)0;
		v92Mode = (V92ComputationalMode)0;
		break;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "VPCMXF_Create: side is %s, maxDataBuffer - %d\r\n",
		    digitalSide != 0 ? "Digital" : "Analog", maxDataBuffer);

	self = (VPcmFloModem *)sysdep_malloc(sizeof(VPcmFloModem));
	vpcmxf_modem_ctor(self, v34Object, side, dpRuntime,
			  (unsigned int)maxDataBuffer, v90Mode, v92Mode);

	/* See the file comment: the object tests AFTER it constructs. */
	if (self == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "VPCMXF_Create: new VPcmFloModem() failed.\n");
		return 0;
	}

	self->flags_173a[0] = 0;
	self->flags_173a[1] = 0;
	self->flags_173a[2] = 0;
	self->flag_173d = 0;
	self->flag_173e = 0;

	self->v34BaudAllow[0] = 1;
	self->v34BaudAllow[1] = 0;
	self->v34BaudAllow[2] = 1;
	self->v34BaudAllow[3] = 1;
	self->v34BaudAllow[4] = 1;
	self->v34BaudAllow[5] = 0;

	self->nofBits = 0;
	self->cpNofBits = 0;
	/*
	 * `bitPointer = 0;` BELONGS BELOW THE BAUD TABLE AND NOT BESIDE
	 * `flags_173a[0]`, AND THE POSITION IS DECODED RATHER THAN CHOSEN --
	 * but only to within a run of six slots, so read the paragraph before
	 * moving it back.
	 *
	 * It used to be the second statement of this block, which is where
	 * the object's own store order puts it, and the compiler then hoisted
	 * the `cpNofBits` store past it: 28 of 495 bytes differed and
	 * `byteident --why` rejected at row 68, `%ax,0x1738(%ebx)` against
	 * `%ax,0x7dcc(%ebx)`.  The three 16-bit zero stores -- `bitPointer`
	 * (+0x1738), `nofBits` (+0x1736) and `cpNofBits` (+0x7dcc) -- came out
	 * in a rotation of the source order, each taking its zero register in
	 * emission order.
	 *
	 * Seventy cells were compiled with the period compiler: every position
	 * of each of the three word stores in this 21-statement block (21 each)
	 * and all 3! orders of the three among their own slots.  **Six reach
	 * zero differing bytes and they are consecutive** -- `bitPointer`
	 * anywhere after `cpNofBits` and before `nofTransmitSequences`.  The
	 * two neighbours of that run are 4 bytes out and nothing else is below
	 * 5, so the run's EDGES are sharp and its interior is not resolvable.
	 *
	 * So this decodes a FACT and not an order (refinement.md's rule 0):
	 * the author wrote this store after the CP bit counters, not with the
	 * flag byte at +0x173a.  The slot inside the run is ours; grouping it
	 * with the other two 16-bit counters is the only reason it is here
	 * rather than three lines lower.
	 */
	self->bitPointer = 0;
	self->terminateJa = 0;
	self->terminateCp = 0;
	self->terminateCpNot = 0;
	self->cpNotLoaded = 0;
	self->nofBitsPerSymbol = 2;
	self->nofTransmitSequences = 0;
	self->minNofTransmitSequences = 1;

	return self;
}

/*
 * ===========================================================================
 * The destructor -- D2 at 0xd030 and D1 at 0xd0a0, 97 bytes each.
 * ===========================================================================
 *
 * USER-DECLARED NOW, AND THE BLOB IS WHY.  This tree used to leave it
 * implicit, and the comment below the definition explained `VPCMXF_Delete`
 * on that basis -- but an implicit destructor is implicitly INLINE, and GCC
 * 3.4 emits NO out-of-line copy of one (t_vpcmctor.cpp measured exactly
 * that: no `_ZN12VPcmFloModemD` anywhere in our build).  The blob HAS both
 * symbols, 97 bytes each, so the original DECLARED its destructor; the
 * VPcmV34Main leaf pass claims the pair.
 *
 * The body is empty; the 97 bytes are the six member destructions the
 * compiler generates, in reverse declaration order -- `entFilt`
 * (GenericIIR<float, double> at +0x7f28), `sineWave` (+0x6f9c), `ansam`
 * (+0x6f5c), `echoCanceller` (+0x6bd0), `v92modem` (+0x6124), `modem`
 * (+0x1758) -- which is EXACTLY the blob's six calls in the blob's order.
 * That agreement is the header's member modelling paying off: the six
 * embedded objects are declared with their real types, so `{}` IS the
 * original's destructor, whatever its body said.
 *
 * IT IS DEFINED IN THIS FILE AND NOT IN VPcmFloModem.cpp, because the TU is
 * a codegen carrier: the blob's `VPCMXF_Delete` INLINES the destructor (six
 * member-destructor relocations at 0xf6d5..0xf71b, no `D1` among them),
 * which GCC only does for a same-TU definition -- exactly the relationship
 * the original had, with both in VPcmV34Main.cpp.  Defined elsewhere,
 * `VPCMXF_Delete` becomes one `call _ZN12VPcmFloModemD1Ev`: identical
 * behaviour, the wrong six instructions.
 */
VPcmFloModem::~VPcmFloModem()
{
}

/*
 * `VPCMXF_Delete` is `if (p) { p->~VPcmFloModem(); sysdep_free(p); }`, and
 * `_ZN12VPcmFloModemD1Ev` at 0xd0a0 is the SAME six calls in the same order,
 * 0x61 bytes of it -- the destructor defined above, inlined here and emitted
 * out of line, one source statement producing both.  A hand-written sequence
 * here would give us a function with no `~VPcmFloModem` behind it.
 */
extern "C" void
VPCMXF_Delete(VPcmFloModem *self)
{
	if (self != 0) {
		self->~VPcmFloModem();
		sysdep_free(self);
	}
}
