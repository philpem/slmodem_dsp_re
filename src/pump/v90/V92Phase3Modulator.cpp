/*
 * V92Phase3Modulator.cpp -- the V.92 phase 3 upstream symbol source.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V92Phase3Modulator.h` holds
 * the object map, the 80-byte allocation it comes from and the evidence for
 * every state name.
 *
 * FOUR OF THE THIRTEEN SYMBOLS ARE HERE -- `generateSymbol` (.text+0x16600,
 * 1,437 bytes), `reset` (+0x16ba0, 351 bytes), the constructor (+0x16d00, 105
 * bytes) and the destructor (+0x16290, 22 bytes).  The other nine belong to
 * whoever owns the rest of the class; they are declared in the header and left
 * undefined, and nothing below calls one.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x20(%esp),%esi` in `reset` after three pushes and a
 * 16-byte frame -- not %ecx, so these are not thiscall (finding 215).
 *
 * THE SIX GENERATORS ARE FILE-STATIC HELPERS, NOT THE METHODS THEY MIRROR.
 * The object has `generateRu`, `generateRuNot`, `genereteSu`, `genereteSuNot`,
 * `generateJa` and `generateTRN1u` as methods of its own and then inlines
 * their bodies into `generateSymbol` rather than calling them -- which is why
 * a switch of thirteen arms is 1,437 bytes.  Each helper below was matched to
 * its method by comparing the two byte for byte, and each is a file-static
 * function rather than the method it corresponds to for the reason
 * docs/v90cpp.md gives: defining a method whose callers are not written yet
 * re-opens the link closure for the whole test suite.
 *
 * WHERE THIS DIFFERS FROM ITS V.90 SIBLING, AND IT DOES.  `V90Phase3Modulator`
 * has the same shape and is not the same code, so nothing here was carried
 * across:
 *
 *   - the scrambler is at +0x18, not +0x20, and is built (5, 23, 99);
 *   - the Ja arms emit `polarity ? -codeLevel : codeLevel`
 *     (.text+0x165af `test`/`je`/`neg`), where V.90's Jd arms emit
 *     `polarity ? codeLevel : -codeLevel`;
 *   - the TRN1u seed is `setle`, i.e. `sample <= 0`, where V.90's TRN1d seed
 *     is `sample > 0`.
 *
 * Built -fno-exceptions -fno-rtti -nostdinc++ like the rest of the C++ here.
 * No virtuals and no allocation, so the test binaries still link with $(CC).
 */

#include <stddef.h>

extern "C" {
#include "dsplib/debug.h"
#include "dsplib/encode.h"
}

#include "dsplib/V92Parameters.h"
#include "dsplib/V92Phase3Modulator.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but only parses `struct name {` out of include/dsplib, so
 * a C++ class asserts its own (finding 230).  This is the check that catches
 * an object right in size and wrong by four in every offset -- and here it is
 * also the only place the 80 bytes the allocation gives are written down as a
 * compile-time claim.
 *
 * Guarded on a 32-bit pointer because two of the offsets are pointers or come
 * after one: `make check64` compiles this file for the host purely to prove
 * the *code* does not depend on 32-bit.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V92P3M_OFF(field, off, tag) \
	typedef char v92p3m_off_##tag[ \
	    ((int)__builtin_offsetof(V92Phase3Modulator, field) == (off)) \
	    ? 1 : -1]

V92P3M_OFF(word_00,	0x00, word00);
V92P3M_OFF(codeLevel,	0x04, codelevel);
V92P3M_OFF(suLevel,	0x06, sulevel);
V92P3M_OFF(state,	0x08, state);
V92P3M_OFF(symbolCount,	0x0c, symbolcount);
V92P3M_OFF(trn1uLength,	0x10, trn1ulength);
V92P3M_OFF(eventCode,	0x14, eventcode);
V92P3M_OFF(scrambler,	0x18, scrambler);
V92P3M_OFF(polarity,	0x38, polarity);
V92P3M_OFF(jaBits,	0x3c, jabits);
V92P3M_OFF(jaBitCount,	0x40, jabitcount);
V92P3M_OFF(pad_44,	0x44, pad44);
V92P3M_OFF(params,	0x4c, params);

typedef char v92p3m_size[(sizeof(V92Phase3Modulator) == 0x50) ? 1 : -1];

#endif /* 32-bit */

/*
 * ===========================================================================
 * The six pieces `generateSymbol` inlines.
 *
 * `(symbolCount - 1) % 6` and `symbolCount % 12` are UNSIGNED throughout: the
 * object divides with the 0xaaaaaaab reciprocal, a plain `mul` and a logical
 * shift, and there is no sign fixup anywhere in the function.  A signed
 * `symbolCount` would need one, so the width and the signedness are both
 * forced rather than chosen.
 * ===========================================================================
 */

/*
 * Ru: three symbols at +codeLevel then three at -codeLevel, chosen by a RANGE
 * test -- `cmp $0x2; jbe` then `cmp $0x5; ja` at .text+0x1669b, which is what
 * GCC lowers three consecutive cases to.  `generateRu` (+0x163a0) is the same
 * eleven instructions.
 *
 * The negation is taken as a SHORT before it is widened: `movzwl 0x4(%esi);
 * neg; movswl %di,%ebx`, so -32768 stays -32768 rather than becoming 32768.
 */
static short
ruSymbol(const V92Phase3Modulator *m)
{
	switch ((m->symbolCount - 1u) % 6u) {
	case 0:
	case 1:
	case 2:
		return m->codeLevel;
	case 3:
	case 4:
	case 5:
		return (short)-m->codeLevel;
	}
	return 0;
}

/* Its inversion; `generateRuNot` (+0x16400) with the two arms exchanged. */
static short
ruNotSymbol(const V92Phase3Modulator *m)
{
	switch ((m->symbolCount - 1u) % 6u) {
	case 0:
	case 1:
	case 2:
		return (short)-m->codeLevel;
	case 3:
	case 4:
	case 5:
		return m->codeLevel;
	}
	return 0;
}

/*
 * Su: a six-symbol cycle at the OTHER amplitude, +0 -0 - with the two zeros
 * one apart from each other rather than adjacent.  The grouping is not three
 * consecutive runs, so GCC lowers it as a BIT TEST instead of a range test --
 * `mov $1,%eax; shl %cl,%eax; test $0x05,%al; test $0x12,%al; test $0x28,%al`
 * at .text+0x16705 -- and the three masks are exactly {0,2}, {1,4} and {3,5}.
 * `genereteSu` (+0x16460, the misspelling is the original's) is the same.
 */
static short
suSymbol(const V92Phase3Modulator *m)
{
	switch ((m->symbolCount - 1u) % 6u) {
	case 0:
	case 2:
		return m->suLevel;
	case 1:
	case 4:
		return 0;
	case 3:
	case 5:
		return (short)-m->suLevel;
	}
	return 0;
}

/* Its inversion; `genereteSuNot` (+0x164e0).  The zeros stay put. */
static short
suNotSymbol(const V92Phase3Modulator *m)
{
	switch ((m->symbolCount - 1u) % 6u) {
	case 0:
	case 2:
		return (short)-m->suLevel;
	case 1:
	case 4:
		return 0;
	case 3:
	case 5:
		return m->suLevel;
	}
	return 0;
}

/*
 * Ja: twenty-five symbols of a constant 1 bit, then the V92Ja vector read
 * cyclically.  `cmp $0x18,%eax; jbe` keeps the constant for symbolCount 1
 * through 24 -- the count has already been incremented -- and `sub $0x19;
 * divl 0x40(%ebx)` indexes the vector from the 25th.
 *
 * THE DIVISOR IS NOT CHECKED.  A null `jaBitCount` faults here exactly as it
 * does in the object; `reset` sets both fields to zero together when it is
 * given no V92Ja, so the caller is what keeps this state off that path.
 *
 * `polarity ^= process(bit)` is the object's full 32-bit `xor` of the byte the
 * scrambler returned, and the sense is `polarity ? -codeLevel : codeLevel` --
 * `test %eax,%eax; je; neg %edx` at .text+0x165af.  That is the opposite of
 * V90Phase3Modulator's Jd arms and was read here rather than assumed.
 */
static short
jaSymbol(V92Phase3Modulator *m)
{
	unsigned char bit = 1;
	short sample;

	if (m->symbolCount > 24u)
		bit = m->jaBits[(m->symbolCount - 25u) % m->jaBitCount];

	m->polarity ^= m->scrambler.process(bit);

	sample = m->codeLevel;
	if (m->polarity != 0u)
		sample = (short)-sample;
	return sample;
}

/*
 * TRN1u: the scrambler driven with a constant 1 bit, the output choosing the
 * sign.  `generateTRN1u` (+0x165c0) is six instructions and the whole of it.
 */
static short
trn1uSymbol(V92Phase3Modulator *m)
{
	if (m->scrambler.process(1) != 0)
		return (short)-m->codeLevel;
	return m->codeLevel;
}

/*
 * ===========================================================================
 * generateSymbol -- .text+0x16600, 1,437 bytes
 *
 * The count is incremented BEFORE the dispatch (`incl 0xc(%esi)` at +0x16616),
 * so every arm's test sees the count including the symbol it is producing.
 *
 * The state bound is `cmp $0xf,%eax; ja` -- UNSIGNED -- over a sixteen-entry
 * jump table at .rodata:0x5e4, and the cast below is what holds the compiler
 * to that: a `switch` on the enum itself would let it assume the value is one
 * of the sixteen and skip the bound.
 *
 * STATE 2 IS NOT A CASE.  Its table entry is the DEFAULT label, +0x16625,
 * while 6, 14 and 15 share the silent block at +0x1666e.  Both write zero to
 * `eventCode` and return zero; the only difference between them is the
 * message, so it is the message that separates the two groups and nothing
 * else can.
 * ===========================================================================
 */
int
V92Phase3Modulator::generateSymbol()
{
	short sample = 0;

	symbolCount++;

	switch ((unsigned int)state) {
	case V92P3M_STATE_RU:
		eventCode = 0;
		sample = ruSymbol(this);
		if (symbolCount == 384u) {
			state = V92P3M_STATE_RU_NOT;
			symbolCount = 0;
		}
		break;

	case V92P3M_STATE_RU_NOT:
		eventCode = 0;
		sample = ruNotSymbol(this);
		if (symbolCount == 24u) {
			state = V92P3M_STATE_TRN1U;
			symbolCount = 0;
			scrambler.reset(0);
			eventCode = 2;
		}
		break;

	/*
	 * The only arm whose limit is a field rather than a literal, and the
	 * only place `trn1uLength` is read.  The polarity seed is `setle`, so
	 * a zero sample seeds 1.
	 */
	case V92P3M_STATE_TRN1U:
		eventCode = 0;
		sample = trn1uSymbol(this);
		if (symbolCount == trn1uLength) {
			state = V92P3M_STATE_JA;
			symbolCount = 0;
			eventCode = 3;
			polarity = (sample <= 0);
		}
		break;

	case V92P3M_STATE_JA:
		eventCode = 0;
		sample = jaSymbol(this);
		break;

	case V92P3M_STATE_JA_END:
		eventCode = 0;
		sample = jaSymbol(this);
		if ((symbolCount % 12u) == 0u) {
			state = V92P3M_STATE_SILENCE;
			symbolCount = 0;
		}
		break;

	/*
	 * Silence, and the two states past the end of the sequence.  One
	 * block in the object, entered from three table slots.
	 */
	case V92P3M_STATE_SILENCE:
	case V92P3M_STATE_END:
	case V92P3M_STATE_SILENT_15:
		eventCode = 0;
		sample = 0;
		break;

	case V92P3M_STATE_SU:
		eventCode = 0;
		sample = suSymbol(this);
		if (symbolCount == 144u) {
			state = V92P3M_STATE_SU_NOT;
			symbolCount = 0;
		}
		break;

	case V92P3M_STATE_SU_NOT:
		eventCode = 0;
		sample = suNotSymbol(this);
		if (symbolCount == 24u) {
			state = V92P3M_STATE_SU_SECOND;
			symbolCount = 0;
			eventCode = 5;
		}
		break;

	case V92P3M_STATE_SU_SECOND:
		eventCode = 0;
		sample = suSymbol(this);
		break;

	case V92P3M_STATE_SU_SECOND_END:
		eventCode = 0;
		sample = suSymbol(this);
		if ((symbolCount % 12u) == 0u) {
			state = V92P3M_STATE_SU_SECOND_NOT;
			symbolCount = 0;
		}
		break;

	case V92P3M_STATE_SU_SECOND_NOT:
		eventCode = 0;
		sample = suNotSymbol(this);
		if (symbolCount == 24u) {
			state = V92P3M_STATE_TRN1U_SECOND;
			symbolCount = 0;
			scrambler.reset(0);
			eventCode = 7;
		}
		break;

	case V92P3M_STATE_TRN1U_SECOND:
		eventCode = 0;
		sample = trn1uSymbol(this);
		break;

	/*
	 * `cmp $0x7f7,%ecx; jbe` at .text+0x16aa0 -- the literal in the object
	 * is 2039, so the guard is `> 2039` and the boundary test that follows
	 * only ever fires at 2040 or beyond.
	 */
	case V92P3M_STATE_TRN1U_SECOND_END:
		eventCode = 0;
		sample = trn1uSymbol(this);
		if (symbolCount > 2039u && (symbolCount % 12u) == 0u) {
			state = V92P3M_STATE_END;
			symbolCount = 0;
			eventCode = 8;
			polarity = (sample <= 0);
		}
		break;

	default:
		eventCode = 0;
		sample = 0;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V92Phase3Modulator: Illegal state\r\n");
		break;
	}

	return sample;
}

/*
 * ===========================================================================
 * reset -- .text+0x16ba0, 351 bytes
 *
 * THE FIRST PARAMETER IS DEAD.  Its slot, `0x24(%esp)`, is never read; the
 * amplitude is the literal `movw $0xfa0,0x4(%esi)` that both tail-duplicated
 * blocks store.  See the header.
 *
 * The TRN1u length is `12 * ((a + b + 12) / 12)` floored at 8160, where a and
 * b are the two V92Parameters echo-update durations.  THE DIVISION IS SIGNED
 * AND THE FLOOR IS UNSIGNED, and both are forced: .text+0x16bf2 multiplies by
 * 0x2aaaaaab with `imul` and then applies the `sar $0x1f` / `sub` fixup that
 * only a signed divide needs, while +0x16c07 compares the product with `ja`.
 * The two readings part company as soon as `a + b + 12` is negative, which is
 * what the test drives.
 * ===========================================================================
 */
void
V92Phase3Modulator::reset(short levelArg, V92Phase3ModulatorState stateArg,
			  unsigned int nSymbols, V92Ja *jaArg,
			  const tagV90DILdescriptor *dilArg,
			  unsigned int lastArg)
{
	unsigned int length;
	unsigned int i;
	float scaled;

	/*
	 * `levelArg` is named so a mutation can try to use it and be caught.
	 * The object never reads its slot; see the block comment above.
	 */
	(void)levelArg;

	scrambler.reset(0);
	symbolCount = 0;
	word_00 = lastArg;
	state = stateArg;

	length = (unsigned int)(12 *
	    ((params->V92_ECHO_FAST_UPDATE_DURATION +
	      params->V92_ECHO_SLOW_UPDATE_DURATION + 12) / 12));
	if (length < 8160u)
		length = 8160u;
	trn1uLength = length;
	edprintf("V92Phase3Modulator: TRN1u state length set to %d\r\n",
	    trn1uLength);

	eventCode = 0;
	codeLevel = 4000;

	if (jaArg != NULL) {
		jaBits = jaArg->bits;
		jaBitCount = jaArg->bitCount;
	} else {
		/*
		 * The author's own words, and the only thing the DIL
		 * descriptor is used for: `reset` never dereferences it.
		 */
		if (dilArg != NULL && DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("BUG BUG BUG BUG BUG - "
			    "V92Phase3Modulator: no Ja Object, "
			    "DIL descriptor available !\r\n");
		jaBitCount = 0;
		jaBits = NULL;
	}

	/*
	 * sqrt(3/2) to sixteen digits, as an EIGHT-byte constant, with the
	 * product rounded through a four-byte temporary before a four-byte
	 * 0.5 is added -- see the header on `suLevel`.  The cast truncates
	 * toward zero, which is what the `or $0xc00` on the saved control word
	 * at .text+0x16c5e sets up.
	 */
	scaled = codeLevel * 1.224744871391589;
	suLevel = (short)(scaled + 0.5f);

	for (i = 0; i < nSymbols; i++)
		generateSymbol();
}

/*
 * ===========================================================================
 * V92Phase3Modulator::V92Phase3Modulator (.text+0x16d00, 105 bytes)
 *
 * The whole body, with nothing elided:
 *
 *     lea    0x18(%ebx),%edx                  <- the scrambler subobject
 *     Scrambler<unsigned char,int>::Scrambler(0x5, 0x17, 0x63)
 *     mov    0x34(%esp),%eax ; mov %eax,0x4c(%ebx)     params
 *     reset(0xfa0, (V92Phase3ModulatorState)0, 0, NULL, NULL, 0)
 *
 * THE TAPS ARE (5, 23), NOT V.90's (18, 23).  This is the upstream scrambler
 * and it is a different polynomial; the third argument, 99, and therefore the
 * 1 + 23 + 99 = 123-byte history are the same in both.  Reading it across
 * from `V90Phase3Modulator` would have been wrong, which is the file
 * comment's point about these two classes.
 *
 * THE `params` STORE MUST PRECEDE THE CALL, and unlike a bare store order
 * that is a behavioural claim: `reset` computes `trn1uLength` out of
 * `params->V92_ECHO_FAST_UPDATE_DURATION` and
 * `params->V92_ECHO_SLOW_UPDATE_DURATION`, so ordering it the other way round
 * dereferences whatever +0x4c held before construction.  Both orderings are
 * mutations and both are caught.
 *
 * The 4000 is `reset`'s dead first parameter -- the header says why the
 * amplitude is `reset`'s own literal and the argument cannot reach it -- and
 * it is passed anyway because that is what the object passes.  `nSymbols` is
 * 0, so nothing is generated, and `ja` is NULL, so `jaBits` and `jaBitCount`
 * come out NULL and 0.  The NULL descriptor is what keeps the "no Ja Object,
 * DIL descriptor available" complaint quiet.
 * ===========================================================================
 */
V92Phase3Modulator::V92Phase3Modulator(V92Parameters *p)
	: scrambler(5, 23, 99)
{
	params = p;
	reset(4000, V92P3M_STATE_RU, 0, NULL, NULL, 0);
}

/*
 * .text+0x16290, 22 bytes, and 22 bytes is not an empty function: the body is
 * a single call to `Scrambler<unsigned char, int>::~Scrambler` on
 * `this + 0x18`, unguarded.  That is what GCC emits for an empty destructor
 * over a class whose one non-trivially-destructible member is the scrambler,
 * so the source is the empty body and the free is the compiler's implicit
 * member destruction.
 */
V92Phase3Modulator::~V92Phase3Modulator()
{
}
