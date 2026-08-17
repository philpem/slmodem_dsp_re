/*
 * V92Phase4Modulator.cpp -- the V.92 phase 4 upstream symbol source.
 *
 * Reconstructed from dsplibs.o.  Twenty-eight symbols, 3,133 bytes: the four
 * below, plus the twenty-four members at the bottom of this file that generate
 * the phase 4 upstream signals, take the tag-driven state transitions, and
 * reset the object between segments.  `include/dsplib/V92Phase4Modulator.h`
 * carries the object map, the state codes four format strings name, and the
 * eight members still outstanding -- all of them behind `V92CP::infoToBits`.
 *
 * The original four:
 *
 *     V92Phase4Modulator::V92Phase4Modulator(V92Parameters *,
 *         V92BitsToSymbol *, V92CP *, V92MappingParams *)
 *                                       .text+0x17970 (C1), +0x17a20 (C2)
 *     V92Phase4Modulator::~V92Phase4Modulator()
 *                                       .text+0x16de0 (D2), +0x16e40 (D1)
 *
 * Each pair differs only in which registers hold two of the argument setups,
 * which is the register allocator's choice and not the source's; GCC emits
 * both from one definition.
 *
 * Plain cdecl, `this` first on the stack -- `mov 0x20(%esp),%ebx` after a
 * 0x1c-byte frame with three saves in it -- finding 215.
 */

#include <stddef.h>

#include "dsplib/V92Phase4Modulator.h"
#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92CP.h"
#include "dsplib/V92Mapper.h"
#include "dsplib/V92ModulusEncoder.h"
#include "dsplib/V92Transmitter.h"
#include "dsplib/debug.h"
#include "dsplib/encode.h"

extern "C" {
void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);

/*
 * V92Mapper's complete-object constructor, by the name the relocation at
 * .text+0x179be carries.  Called through an asm() label rather than through
 * `new` for the reason src/pump/v90/V92Precoder.cpp sets out in full: the
 * blob's `sysdep_malloc(n); ctor(p)` with no null test between them is what
 * `new` emits over an inline `operator new`, and this build has no <new>.
 */
void v92p4m_mapper_ctor(void *self) asm("_ZN9V92MapperC1Ev");
}

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V92P4M_OFF(field, off, tag) \
	typedef char v92p4m_off_##tag[ \
	    ((int)__builtin_offsetof(V92Phase4Modulator, field) == (off)) \
	    ? 1 : -1]

V92P4M_OFF(state,		0x000, state);
V92P4M_OFF(symbolCount,		0x004, symbolcount);
V92P4M_OFF(patternIndex,	0x008, patternindex);
V92P4M_OFF(pad_0c,		0x00c, pad0c);
V92P4M_OFF(word_18,		0x018, word18);
V92P4M_OFF(byte_1c,		0x01c, byte1c);
V92P4M_OFF(pad_1d,		0x01d, pad1d);
V92P4M_OFF(flag_20,		0x020, flag20);
V92P4M_OFF(pad_24,		0x024, pad24);
V92P4M_OFF(word_28,		0x028, word28);
V92P4M_OFF(word_2c,		0x02c, word2c);
V92P4M_OFF(word_30,		0x030, word30);
V92P4M_OFF(word_34,		0x034, word34);
V92P4M_OFF(word_38,		0x038, word38);
V92P4M_OFF(flag_3c,		0x03c, flag3c);
V92P4M_OFF(amplitude,		0x040, amplitude);
V92P4M_OFF(bitsPerSymbol,	0x043, bitspersymbol);
V92P4M_OFF(pad_44,		0x044, pad44);
V92P4M_OFF(mappingParams,	0x048, mappingparams);
V92P4M_OFF(scrambler,		0x04c, scrambler);
V92P4M_OFF(bitsToSymbol,	0x06c, bitstosymbol);
V92P4M_OFF(mapper,		0x070, mapper);
V92P4M_OFF(cp,			0x074, cp);
V92P4M_OFF(prevBit,		0x078, prevbit);
V92P4M_OFF(bits,		0x07c, bits);
V92P4M_OFF(pattern,		0x1a8, pattern);
V92P4M_OFF(patternLength,	0x1ac, patternlength);
V92P4M_OFF(word_1b0,		0x1b0, word1b0);
V92P4M_OFF(word_1b8,		0x1b8, word1b8);
V92P4M_OFF(e2uExtended,		0x1bc, e2uextended);
V92P4M_OFF(word_1c0,		0x1c0, word1c0);
V92P4M_OFF(word_1c4,		0x1c4, word1c4);
V92P4M_OFF(params,		0x1c8, params);

typedef char v92p4m_size[(sizeof(V92Phase4Modulator) == 0x1cc) ? 1 : -1];
typedef char v92p4m_mapper_size[(sizeof(V92Mapper) == 0x2c) ? 1 : -1];

/*
 * The three fields this file reaches through, each at the offset the
 * instruction that reaches it uses:  `mov (%ecx),%edx; mov 0x48(%edx),%ebx;
 * movl $0x1,0x50(%ebx)` at .text+0x178fd, in generateDataSymbolBeforeFPE.
 */
typedef char v92p4m_bts_tx[
	((int)__builtin_offsetof(V92BitsToSymbol, transmitter) == 0x00) ? 1 : -1];
typedef char v92p4m_tx_modenc[
	((int)__builtin_offsetof(V92Transmitter, modulusEncoder) == 0x48) ? 1 : -1];
typedef char v92p4m_modenc_f50[
	((int)__builtin_offsetof(V92ModulusEncoder, field_50) == 0x50) ? 1 : -1];

/* The two words of the caller's V92CP this file writes. */
typedef char v92p4m_cp_byte04[
	((int)__builtin_offsetof(V92CP, byte_04) == 0x04) ? 1 : -1];

/*
 * The scrambler is a member and its size is what makes +0x6c meet it, so the
 * map is only self-consistent if this holds.
 */
typedef char v92p4m_scram_size[
	(sizeof(Scrambler<unsigned char, unsigned char>) == 0x20) ? 1 : -1];

/* The word this constructor reaches into somebody else's object to clear. */
typedef char v92p4m_cp_word110[
	((int)__builtin_offsetof(V92CP, word_110) == 0x110) ? 1 : -1];

#endif /* 32-bit */

/*
 * ===========================================================================
 * V92Phase4Modulator::V92Phase4Modulator (.text+0x17970 / +0x17a20, 164 B)
 *
 * The whole body, in the object's own order:
 *
 *     Scrambler<unsigned char,unsigned char>(this + 0x4c, 5, 0x17, 0x63)
 *     malloc(0x2c) -> V92Mapper() -> +0x70
 *     +0x1c0 = 0
 *     +0x48  = mappingParams          (the FOURTH argument)
 *     +0x1c8 = params                 (the FIRST)
 *     +0x1c4 = 0
 *     +0x74  = cp                     (the THIRD)
 *     cp->word_110 = 0
 *     +0x6c  = bitsToSymbol           (the SECOND)
 *     +0x18  = 0
 *     +0x1c  = 0                      (a BYTE store)
 *
 * THE ARGUMENTS ARE NOT STORED IN ORDER, and that is worth stating because a
 * reconstruction that stores them in declaration order produces an object
 * that is wrong in four fields and passes nothing.  The order above is
 * `mov 0x30(%esp)`, `mov 0x24(%esp)`, `mov 0x2c(%esp)`, `mov 0x28(%esp)` --
 * fourth, first, third, second -- read off the frame after a 0x1c-byte
 * subtraction with three register saves inside it.
 *
 * The store into the caller's `V92CP` is the constructor's, not a side
 * effect: %esi is zeroed at .text+0x179e5 for the sole purpose of it.
 * ===========================================================================
 */
V92Phase4Modulator::V92Phase4Modulator(V92Parameters *p, V92BitsToSymbol *bts,
				       V92CP *c, V92MappingParams *mp)
	: scrambler(V92P4M_SCRAM_TAP1, V92P4M_SCRAM_TAP2, V92P4M_SCRAM_SLACK)
{
	void *m;

	m = sysdep_malloc(sizeof(V92Mapper));
	v92p4m_mapper_ctor(m);
	mapper = (V92Mapper *)m;

	word_1c0 = 0;
	mappingParams = mp;
	params = p;
	word_1c4 = 0;
	cp = c;
	cp->word_110 = 0;
	bitsToSymbol = bts;
	word_18 = 0;
	byte_1c = 0;
}

/*
 * ===========================================================================
 * V92Phase4Modulator::~V92Phase4Modulator (.text+0x16de0 / +0x16e40, 87 B)
 *
 * The body is the mapper's release and nothing else; the unconditional
 * `Scrambler<unsigned char,unsigned char>::~Scrambler(this + 0x4c)` that
 * follows it on both paths is the COMPILER'S implicit member destruction, not
 * a statement -- the same reading finding 1256 makes of V92Phase3Modulator's
 * 22-byte destructor, and the same reason its source is an empty body.
 *
 * The mapper pointer is NOT nulled after the free, so a second destruction
 * double-frees it.  Reproduced; see docs/deviations.md.
 * ===========================================================================
 */
V92Phase4Modulator::~V92Phase4Modulator()
{
	if (mapper != 0) {
		mapper->~V92Mapper();
		sysdep_free(mapper);
	}
}

/*
 * ===========================================================================
 * THE GENERATORS
 *
 * Every one of them returns a symbol, and every one of them ends with a
 * 16-bit value sign-extended into %eax -- `cwtl` where the value is already in
 * %ax, `movswl` where it is in memory.  That is why the return type is `int`
 * and the local holding the symbol is `short`.
 *
 * Six share one shape: fill `bits` with `bitsPerSymbol` scrambled bits,
 * exclusive-OR `prevBit` into the LAST of them, store the result back both
 * into that bit and into `prevBit`, and hand the block to `mapper`.  Three of
 * those six take a second path when `flag_3c` is set, in which the bits come
 * from `bitsToSymbol` instead and the mapper is not used at all.
 * ===========================================================================
 */

/*
 * generateCPt (.text+0x17ff0, 94 B).
 *
 * Below 25 symbols the emitted bit simply alternates; from 25 on it is the
 * scrambled `pattern`, reduced from a base of 25 rather than of zero
 * (`sub $0x19,%eax` at +0x18020).  Both arms converge on the same store to
 * `prevBit` and the same +/- selection, which is what the `jmp 18006` says.
 */
int V92Phase4Modulator::generateCPt()
{
	unsigned int bit;
	short sym;

	if (symbolCount <= 24)
		bit = prevBit ^ 1;
	else
		bit = (unsigned char)scrambler.process(
			      pattern[(symbolCount - 25) % patternLength])
		      ^ prevBit;
	prevBit = bit;

	sym = amplitude;
	if (bit != 0)
		sym = -sym;
	return sym;
}

/*
 * generateE1u (.text+0x17fb0, 54 B).  One scrambled zero, differentially
 * encoded, and the same +/- selection generateCPt ends with.
 */
int V92Phase4Modulator::generateE1u()
{
	unsigned int bit;
	short sym;

	bit = (unsigned char)scrambler.process(0) ^ prevBit;
	prevBit = bit;

	sym = amplitude;
	if (bit != 0)
		sym = -sym;
	return sym;
}

/*
 * generateCPu (.text+0x17d50, 304 B) and generateSUVu (+0x17e80, 304 B).
 *
 * THE TWO BODIES ARE THE SAME INSTRUCTIONS IN THE SAME ORDER, differing only
 * in which of %esi and %edi holds the loop counter -- the register allocator's
 * choice, which finding 614 puts in the free column.  So the two source
 * bodies are identical and what makes CPu a CP and SUVu an SUV is what
 * `pattern` holds, not what these do with it.  Written out twice rather than
 * factored: a shared helper would be one symbol where the object has two.
 *
 * The loop re-reads `bitsPerSymbol` on every iteration (`movzbl 0x43(%ebx)` at
 * +0x17ddc, inside the loop) because `Scrambler::process` is a call the
 * compiler cannot see through -- so the bound is the field, not a copy of it.
 */
int V92Phase4Modulator::generateCPu()
{
	unsigned int i;
	unsigned int n;
	unsigned int last;
	short sym;

	if (flag_3c == 0) {
		for (i = 0; i < bitsPerSymbol; i++) {
			bits[i] = scrambler.process(pattern[patternIndex]);
			patternIndex = (patternIndex + 1) % patternLength;
		}
		last = bits[bitsPerSymbol - 1] ^ prevBit;
		bits[bitsPerSymbol - 1] = (unsigned char)last;
		prevBit = last;
		sym = mapper->process(bits);
		return sym;
	}

	n = bitsToSymbol->nofBitsForNextTime();
	if (n != 0) {
		for (i = 0; i < n; i++) {
			bits[i] = scrambler.process(pattern[patternIndex]);
			patternIndex = (patternIndex + 1) % patternLength;
		}
		bitsToSymbol->process(bits, n);
	}
	bitsToSymbol->process(n, &sym);
	return sym;
}

int V92Phase4Modulator::generateSUVu()
{
	unsigned int i;
	unsigned int n;
	unsigned int last;
	short sym;

	if (flag_3c == 0) {
		for (i = 0; i < bitsPerSymbol; i++) {
			bits[i] = scrambler.process(pattern[patternIndex]);
			patternIndex = (patternIndex + 1) % patternLength;
		}
		last = bits[bitsPerSymbol - 1] ^ prevBit;
		bits[bitsPerSymbol - 1] = (unsigned char)last;
		prevBit = last;
		sym = mapper->process(bits);
		return sym;
	}

	n = bitsToSymbol->nofBitsForNextTime();
	if (n != 0) {
		for (i = 0; i < n; i++) {
			bits[i] = scrambler.process(pattern[patternIndex]);
			patternIndex = (patternIndex + 1) % patternLength;
		}
		bitsToSymbol->process(bits, n);
	}
	bitsToSymbol->process(n, &sym);
	return sym;
}

/*
 * generateE2u (.text+0x17c60, 239 B).  generateCPu's shape with the pattern
 * loop replaced by one bulk `processAllZeros` -- E2u is an all-zeros segment.
 */
int V92Phase4Modulator::generateE2u()
{
	unsigned int n;
	unsigned int last;
	short sym;

	if (flag_3c == 0) {
		scrambler.processAllZeros(bits, bitsPerSymbol);
		last = bits[bitsPerSymbol - 1] ^ prevBit;
		prevBit = last;
		bits[bitsPerSymbol - 1] = (unsigned char)last;
		sym = mapper->process(bits);
		return sym;
	}

	n = bitsToSymbol->nofBitsForNextTime();
	if (n != 0) {
		scrambler.processAllZeros(bits, n);
		bitsToSymbol->process(bits, n);
	}
	bitsToSymbol->process(n, &sym);
	return sym;
}

/*
 * generateTRN2u (.text+0x17c10, 78 B).  generateE2u's first arm with
 * `processAllOnes`, and no `flag_3c` test at all.
 */
int V92Phase4Modulator::generateTRN2u()
{
	unsigned int last;
	short sym;

	scrambler.processAllOnes(bits, bitsPerSymbol);
	last = bits[bitsPerSymbol - 1] ^ prevBit;
	prevBit = last;
	bits[bitsPerSymbol - 1] = (unsigned char)last;
	sym = mapper->process(bits);
	return sym;
}

/*
 * generateRm (.text+0x17ad0, 149 B) and generateB1u (+0x17b70, 149 B).
 *
 * THE TWO BODIES ARE IDENTICAL, instruction for instruction, register for
 * register -- the same relationship generateCPu and generateSUVu have.  Both
 * go through `bitsToSymbol` unconditionally and neither touches `mapper`.
 */
int V92Phase4Modulator::generateRm()
{
	unsigned int n;
	short sym;

	n = bitsToSymbol->nofBitsForNextTime();
	if (n != 0) {
		scrambler.processAllOnes(bits, n);
		bitsToSymbol->process(bits, n);
	}
	bitsToSymbol->process(n, &sym);
	return sym;
}

int V92Phase4Modulator::generateB1u()
{
	unsigned int n;
	short sym;

	n = bitsToSymbol->nofBitsForNextTime();
	if (n != 0) {
		scrambler.processAllOnes(bits, n);
		bitsToSymbol->process(bits, n);
	}
	bitsToSymbol->process(n, &sym);
	return sym;
}

/*
 * generateRu (.text+0x16fb0, 88 B) and generateRuNot (+0x17010, 88 B).
 *
 * A six-symbol pattern: three of one sign then three of the other, indexed by
 * `(symbolCount - 1) % 6` -- the `mul $0xaaaaaaab; shr $2` at +0x16fc9 is
 * GCC's division by six, and the `lea (%edx,%edx,2); add %edx,%edx; sub` that
 * follows it is the multiply-back.  `RuNot` is `Ru` with the two signs
 * exchanged.
 *
 * THE SWITCH HAS NO DEFAULT AND `sym` IS LEFT UNINITIALISED ON A PATH THAT
 * CANNOT BE TAKEN.  `x % 6` is never above 5, but GCC does not know that, so
 * it emits a `ja` to a return that reads the register `sym` lives in before
 * anything has written it (+0x16fdf -> +0x16ffb, and +0x1703f -> +0x1705b).
 * That is the object's own code and it is reproduced rather than tidied: a
 * `default:` arm here would add an instruction the blob does not have.
 */
int V92Phase4Modulator::generateRu()
{
	short sym;

	switch ((symbolCount - 1) % 6) {
	case 0:
	case 1:
	case 2:
		sym = amplitude;
		break;
	case 3:
	case 4:
	case 5:
		sym = -amplitude;
		break;
	}
	return sym;
}

int V92Phase4Modulator::generateRuNot()
{
	short sym;

	switch ((symbolCount - 1) % 6) {
	case 0:
	case 1:
	case 2:
		sym = -amplitude;
		break;
	case 3:
	case 4:
	case 5:
		sym = amplitude;
		break;
	}
	return sym;
}

/*
 * generateDataSymbolBeforeFPE (.text+0x178a0, 110 B).
 *
 * `nbits` is passed to `process` UNINITIALISED and comes back as the number of
 * bits that were left over; a non-zero one means the data being sent has run
 * out mid-block, which is when Rm starts.  Its last act reaches three levels
 * down -- `bitsToSymbol->transmitter->modulusEncoder->field_50 = 1` -- and
 * that word is what `V92ModulusEncoder::progress` switches on.
 */
int V92Phase4Modulator::generateDataSymbolBeforeFPE()
{
	unsigned int nbits;
	short sym;

	bitsToSymbol->process(nbits, &sym);
	if (nbits != 0) {
		edprintf("V92Phase4Modulator: enter Rm @ %d\r\n", symbolCount);
		state = V92P4M_STATE_RM;
		symbolCount = 0;
		bitsToSymbol->transmitter->modulusEncoder->field_50 = 1;
	}
	return sym;
}

/* generateDataSymbolBeforeRRN (.text+0x17910, 95 B).  The same, entering Ru
 * and without the reach into the modulus encoder. */
int V92Phase4Modulator::generateDataSymbolBeforeRRN()
{
	unsigned int nbits;
	short sym;

	bitsToSymbol->process(nbits, &sym);
	if (nbits != 0) {
		edprintf("V92Phase4Modulator: enter Ru @ %d\r\n", symbolCount);
		state = V92P4M_STATE_RU;
		symbolCount = 0;
	}
	return sym;
}

/*
 * ===========================================================================
 * THE TAG HANDLERS
 *
 * Three of them share one block: on a symbol count that is a whole number of
 * `word_1b0` periods, announce E2u, enter it, restart the count and record
 * whether the following state is 12 or 13.  The blob holds that block once per
 * function -- cross-jumped within each, never shared between them -- so it is
 * written out at each site rather than factored into a helper the object does
 * not have.
 * ===========================================================================
 */

/* recivedCP (.text+0x172c0, 34 B).  Two separate loads of `cp`, which is two
 * statements through the pointer and not one. */
void V92Phase4Modulator::recivedCP()
{
	word_1c0 = 1;
	cp->byte_04 = 1;
	cp->word_110 = 0;
}

/* recivedPartOneSilenceRrnSUV (.text+0x171f0, 12 B). */
void V92Phase4Modulator::recivedPartOneSilenceRrnSUV()
{
	cp->byte_04 = 1;
}

/*
 * recivedPartTwoSilenceRrnSUVtag (.text+0x174f0, 5 B).  A single `jmp` with a
 * relocation on it -- a sibling call, so a distinct function whose body is one
 * call, not an alias.
 */
void V92Phase4Modulator::recivedPartTwoSilenceRrnSUVtag()
{
	recivedSUVtag();
}

/*
 * recivedEd (.text+0x17630, 141 B).  The modulus test is the OUTER one here
 * and the state switch is inside its false arm; `recivedSUVtag` has the two
 * the other way round, which is a difference in the two sources and not in
 * what the compiler did with one.
 */
void V92Phase4Modulator::recivedEd()
{
	if (flag_20 != 0)
		return;

	if (symbolCount % word_1b0 != 0) {
		switch (state) {
		case 5:
			state = 9;
			break;
		case 12:
		case 13:
			state = 11;
			break;
		default:
			return;
		}
	} else {
		edprintf("V92Phase4Modulator: enter E2u @ %d\r\n", symbolCount);
		state = V92P4M_STATE_E2U;
		symbolCount = 0;
		word_1b8 = (e2uExtended != 0) ? 13 : 12;
	}
	flag_20 = 1;
}

/*
 * recivedFirstRrnEd (.text+0x176c0, 114 B).  E2u again, but the state after it
 * is 12 whatever `e2uExtended` says -- and if it says the segment was
 * extended, that is an error worth a second message.
 */
void V92Phase4Modulator::recivedFirstRrnEd()
{
	if (flag_20 != 0)
		return;

	if (symbolCount % word_1b0 != 0) {
		state = 9;
	} else {
		edprintf("V92Phase4Modulator: enter E2u First at RRN @ %d\r\n",
			 symbolCount);
		state = V92P4M_STATE_E2U;
		symbolCount = 0;
		word_1b8 = 12;
		if (e2uExtended != 0)
			edprintf("V92Phase4Modulator: ERROR: E2u is extended"
				 " in RRN !!!\r\n");
	}
	flag_20 = 1;
}

/*
 * recivedSUVtag (.text+0x172f0, 207 B).
 *
 * The two clears at the top and the trace happen whatever else does; the
 * transition needs `word_1c0`, the CP's own `word_110`, and `flag_20` clear.
 * The message goes through `dsplibs_debug_printf` rather than `edprintf`, so
 * it is neither encoded nor part of edprintf's key stream -- `cmpl $0x1,
 * dsplibs_debug_level; ja` at +0x172fc.
 */
void V92Phase4Modulator::recivedSUVtag()
{
	byte_1c = 0;
	word_18 = 0;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("recivedSUVtag called\r\n");

	if (word_1c0 == 0 || cp->word_110 == 0 || flag_20 != 0)
		return;

	switch (state) {
	case 5:
		if (symbolCount % word_1b0 != 0) {
			state = 9;
		} else {
			edprintf("V92Phase4Modulator: enter E2u @ %d\r\n",
				 symbolCount);
			state = V92P4M_STATE_E2U;
			symbolCount = 0;
			word_1b8 = (e2uExtended != 0) ? 13 : 12;
		}
		flag_20 = 1;
		break;
	case 12:
	case 13:
		if (symbolCount % word_1b0 != 0) {
			state = 11;
		} else {
			edprintf("V92Phase4Modulator: enter E2u @ %d\r\n",
				 symbolCount);
			state = V92P4M_STATE_E2U;
			symbolCount = 0;
			word_1b8 = (e2uExtended != 0) ? 13 : 12;
		}
		flag_20 = 1;
		break;
	}
}

/*
 * ===========================================================================
 * THE SEGMENT BOUNDARIES AND THE RESETS
 * ===========================================================================
 */

/*
 * exitCPt (.text+0x170b0, 87 B).  CPt ends on a whole number of periods
 * counted from 24; anything else means it has to be extended, which is state 1.
 */
void V92Phase4Modulator::exitCPt()
{
	if (state != 0 || symbolCount == 0)
		return;

	if ((symbolCount - 24) % word_1b0 != 0) {
		state = 1;
		return;
	}
	edprintf("V92Phase4Modulator: enter E1u @ %d\r\n", symbolCount);
	state = V92P4M_STATE_E1U;
	symbolCount = 0;
}

/* exitTRN2u (.text+0x17110, 30 B). */
void V92Phase4Modulator::exitTRN2u()
{
	if (state == 3 && symbolCount != 0)
		state = 4;
}

/* resetBeforFPE (.text+0x16f00, 19 B). */
void V92Phase4Modulator::resetBeforFPE()
{
	symbolCount = 0;
	flag_3c = 1;
}

/* resetBeforRRN (.text+0x16ea0, 81 B). */
void V92Phase4Modulator::resetBeforRRN()
{
	word_1c4 = 0;
	word_1c0 = 0;
	symbolCount = 0;
	cp->word_110 = 0;
	word_28 = 1;
	word_2c = 0;
	word_30 = 0;
	word_34 = 0;
	word_38 = 0;
	flag_20 = 0;
}

/* resetRRNSecondSection (.text+0x17070, 61 B). */
void V92Phase4Modulator::resetRRNSecondSection()
{
	word_1c4 = 0;
	word_34 = 1;
	flag_20 = 0;
	byte_1c = 0;
	word_18 = 0;
	word_1c0 = 0;
	cp->word_110 = 0;
	cp->byte_04 = 0;
}

/*
 * setMappingParams (.text+0x17840, 88 B).
 *
 * It does NOT store its argument into `this->mappingParams`; it hands it
 * straight to `bitsToSymbol` and forces that object's symbol block back to
 * one.  A null argument is a diagnostic and nothing else.  Both exits are
 * sibling calls in the object, which is what an ignored return value from a
 * tail position gives.
 */
void V92Phase4Modulator::setMappingParams(V92MappingParams *mp)
{
	if (mp == 0) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("V92Phase4Modulator: ERROR: Null"
					     " mappingParams @"
					     " setMappingParams\r\n");
		return;
	}
	bitsToSymbol->reset(mp);
	bitsToSymbol->setSymbolsBlockSize(1);
}
