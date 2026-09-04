/*
 * V92Phase4Modulator.cpp -- the V.92 phase 4 upstream symbol source.
 *
 * Reconstructed from dsplibs.o.  ALL 34 MEMBERS: the four below, the members
 * that generate the phase 4 upstream signals, take the tag-driven state
 * transitions and reset the object between segments, `generateSymbol` -- the
 * state machine the generators are the arms of -- and `reset`, at the bottom,
 * which is the writer every one of the others reads its state out of.
 * `include/dsplib/V92Phase4Modulator.h` carries the object map, the state codes
 * the format strings name, and the enum `reset`'s mangling requires.
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
 * 0x1c-byte frame with three saves in it -- finding F215.
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
V92P4M_OFF(word_0c,		0x00c, word0c);
V92P4M_OFF(pad_10,		0x010, pad10);
V92P4M_OFF(word_18,		0x018, word18);
V92P4M_OFF(byte_1c,		0x01c, byte1c);
V92P4M_OFF(flag_20,		0x020, flag20);
V92P4M_OFF(word_24,		0x024, word24);
V92P4M_OFF(word_28,		0x028, word28);
V92P4M_OFF(word_2c,		0x02c, word2c);
V92P4M_OFF(word_30,		0x030, word30);
V92P4M_OFF(word_34,		0x034, word34);
V92P4M_OFF(word_38,		0x038, word38);
V92P4M_OFF(flag_3c,		0x03c, flag3c);
V92P4M_OFF(amplitude,		0x040, amplitude);
V92P4M_OFF(byte_42,		0x042, byte42);
V92P4M_OFF(bitsPerSymbol,	0x043, bitspersymbol);
V92P4M_OFF(word_44,		0x044, word44);
V92P4M_OFF(mappingParams,	0x048, mappingparams);
V92P4M_OFF(scrambler,		0x04c, scrambler);
V92P4M_OFF(bitsToSymbol,	0x06c, bitstosymbol);
V92P4M_OFF(mapper,		0x070, mapper);
V92P4M_OFF(cp,			0x074, cp);
V92P4M_OFF(prevBit,		0x078, prevbit);
V92P4M_OFF(bitsExt,		0x078, bitsext);

/*
 * AND WHERE `bits[0]` LANDS INSIDE IT, which is the assertion the union
 * replaced `V92P4M_OFF(bits, 0x07c, bits)` with.  If the bias and the array
 * ever drift apart, every fold moves by the difference and no differential
 * trial below `bitsPerSymbol == 1` could tell.  D561.
 */
typedef char v92p4m_off_bits[
    ((int)__builtin_offsetof(V92Phase4Modulator, bitsExt)
     + V92P4M_BITS_BELOW == 0x07c) ? 1 : -1];

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

/* The words of the caller's V92CP this file writes. */
typedef char v92p4m_cp_byte04[
	((int)__builtin_offsetof(V92CP, byte_04) == 0x04) ? 1 : -1];
typedef char v92p4m_cp_suv[
	((int)__builtin_offsetof(V92CP, suv) == 0x108) ? 1 : -1];
typedef char v92p4m_cp_bps[
	((int)__builtin_offsetof(V92CP, bitsPerSymbol) == 0x128) ? 1 : -1];

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
 * THE ORDER OF THE DEFINITIONS BELOW IS LOAD-BEARING.  DO NOT REGROUP THEM.
 *
 * GCC 3.4.2 emits leaf functions in source-definition order, and where a
 * function is emitted CHANGES THE CODE IT EMITS -- not just its address.
 * Reordering this file's definitions to the blob's emission order took seven
 * grade-1 near-misses to byte-exact and moved nothing the wrong way.
 *
 * So the definitions are in the BLOB'S EMISSION ORDER, which is the original
 * author's source order and is NOT grouped by role.  `nm -n` on the blob is
 * the authority; the `.text+0x...` addresses in the per-function comments run
 * in increasing order down the file and are the cheap check that they still
 * do.  Tidying two related handlers back together will silently un-match
 * whatever sits between them.  Finding F7782.
 *
 * A macro or a file-scope `static` must therefore live ABOVE the definitions,
 * not beside its first user: a later re-ordering will move a user above it.
 * ===========================================================================
 */

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
 * a statement -- the same reading finding F1256 makes of V92Phase3Modulator's
 * 22-byte destructor, and the same reason its source is an empty body.
 *
 * The mapper pointer is NOT nulled after the free, so a second destruction
 * double-frees it.  Reproduced; see docs/deviations.md.
 * ===========================================================================
 */
/*
 * THE REPLACEMENT `operator delete`, exactly as `V92Precoder.cpp` carries it
 * and for the same reason: the blob's global `operator delete` IS
 * `sysdep_free` (refinement.md lever 7), and a delete-expression over an
 * inline wrapper is the only spelling that emits what the object emits.
 *
 * ITS POSITION IS DELIBERATE AND IS NOT A TIDY-UP WAITING TO HAPPEN.  An
 * inline function's place in the translation unit is a lever-3 carrier, and
 * consolidating the array form into `sysdep.h` once cost eight destructors
 * their byte identity (finding F7815).  One copy per file, here, below its
 * `sysdep_free` declaration and above its only user.
 *
 * No sized form: the Makefile passes `-fno-sized-deallocation`, so the modern
 * build resolves `delete` to this unsized operator the way 3.4.2 does
 * (finding F7900).
 */
inline void operator delete(void *p) { sysdep_free(p); }

/*
 * `delete mapper`, AND THE OPEN-CODED FORM IS NOT EQUIVALENT.  This body read
 *
 *     if (mapper != 0) { mapper->~V92Mapper(); sysdep_free(mapper); }
 *
 * which reloads `mapper` from +0x70 after the destructor call, because the
 * call may have written it.  The blob loads it ONCE into `%ebx` and uses the
 * same register for the free, which costs it a second callee-saved register
 * and the stack slots to spill both -- 87 bytes against our 67.
 *
 * A delete-expression evaluates its operand once, and it is the ONLY spelling
 * that does it the object's way.  Eight were compiled (finding F8082): the
 * open-coded form, three local-copy forms, a comma operator, a local copy
 * followed by `delete`, and the delete-expression with and without a
 * redundant null guard.  Only the last two reach the object, and they reach
 * it exactly; every local-copy form lands at 61 bytes, which is FURTHER from
 * the blob than the shape they replaced.  So what is decoded is that the
 * operand is the MEMBER and the expression is a `delete`, and not merely that
 * the pointer is read once.
 *
 * The guarded spelling `if (mapper != 0) delete mapper;` is byte-identical --
 * a delete-expression already tests for null, so GCC folds the guard away --
 * and is therefore not distinguished by the object.  The unguarded one is
 * written because it is the smaller claim.
 */
V92Phase4Modulator::~V92Phase4Modulator()
{
	delete mapper;
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
 * WHY THE SIX GENERATORS BELOW SAY `bitsExt[V92P4M_BITS_BELOW + i]` WHERE THE
 * OBJECT SAYS `bits[i]`.
 *
 * The bit block and `prevBit` are ONE array object here, because the object
 * addresses one byte below the block: the differential fold writes
 * `bits[bitsPerSymbol - 1]`, which at a count of zero is `prevBit`'s top byte,
 * and the blob's `0x7b(%count,%this,1)` is that address.  The header carries
 * the four instruction pairs that say so and D561 carries the ruling.  Written
 * as two members the subscript was out of bounds, the two overlapping stores
 * were the compiler's to order, and GCC 3.4.2 and GCC 13 ordered them
 * differently (finding F4705).
 *
 * Spelling every block access through the wider array rather than through a
 * local pointer is DELIBERATE and was measured: the bias rides in the
 * addressing mode exactly as finding F3701 predicted, and `compare.py` comes
 * out at 410 identical / 78 same size / 606 different size / 401355 bytes,
 * unchanged in every figure.  A local `unsigned char *bits = &bitsExt[...]`
 * reads better and does not: it hoists the address, moves six functions by 27
 * bytes in total, and is a source change this deviation is not entitled to
 * make.
 */

/*
 * generateCPu (.text+0x17d50, 304 B) and generateSUVu (+0x17e80, 304 B).
 *
 * THE TWO BODIES ARE THE SAME INSTRUCTIONS IN THE SAME ORDER, differing only
 * in which of %esi and %edi holds the loop counter -- the register allocator's
 * choice, which finding F614 puts in the free column.  So the two source
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
			bitsExt[V92P4M_BITS_BELOW + i] =
				scrambler.process(pattern[patternIndex]);
			patternIndex = (patternIndex + 1) % patternLength;
		}
		last = bitsExt[V92P4M_BITS_BELOW + bitsPerSymbol - 1] ^ prevBit;
		bitsExt[V92P4M_BITS_BELOW + bitsPerSymbol - 1] =
			(unsigned char)last;
		prevBit = last;
		sym = mapper->process(&bitsExt[V92P4M_BITS_BELOW]);
		return sym;
	}

	n = bitsToSymbol->nofBitsForNextTime();
	if (n != 0) {
		for (i = 0; i < n; i++) {
			bitsExt[V92P4M_BITS_BELOW + i] =
				scrambler.process(pattern[patternIndex]);
			patternIndex = (patternIndex + 1) % patternLength;
		}
		bitsToSymbol->process(&bitsExt[V92P4M_BITS_BELOW], n);
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
			bitsExt[V92P4M_BITS_BELOW + i] =
				scrambler.process(pattern[patternIndex]);
			patternIndex = (patternIndex + 1) % patternLength;
		}
		last = bitsExt[V92P4M_BITS_BELOW + bitsPerSymbol - 1] ^ prevBit;
		bitsExt[V92P4M_BITS_BELOW + bitsPerSymbol - 1] =
			(unsigned char)last;
		prevBit = last;
		sym = mapper->process(&bitsExt[V92P4M_BITS_BELOW]);
		return sym;
	}

	n = bitsToSymbol->nofBitsForNextTime();
	if (n != 0) {
		for (i = 0; i < n; i++) {
			bitsExt[V92P4M_BITS_BELOW + i] =
				scrambler.process(pattern[patternIndex]);
			patternIndex = (patternIndex + 1) % patternLength;
		}
		bitsToSymbol->process(&bitsExt[V92P4M_BITS_BELOW], n);
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
		scrambler.processAllZeros(&bitsExt[V92P4M_BITS_BELOW],
					  bitsPerSymbol);
		last = bitsExt[V92P4M_BITS_BELOW + bitsPerSymbol - 1] ^ prevBit;
		prevBit = last;
		bitsExt[V92P4M_BITS_BELOW + bitsPerSymbol - 1] =
			(unsigned char)last;
		sym = mapper->process(&bitsExt[V92P4M_BITS_BELOW]);
		return sym;
	}

	n = bitsToSymbol->nofBitsForNextTime();
	if (n != 0) {
		scrambler.processAllZeros(&bitsExt[V92P4M_BITS_BELOW], n);
		bitsToSymbol->process(&bitsExt[V92P4M_BITS_BELOW], n);
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

	scrambler.processAllOnes(&bitsExt[V92P4M_BITS_BELOW], bitsPerSymbol);
	last = bitsExt[V92P4M_BITS_BELOW + bitsPerSymbol - 1] ^ prevBit;
	prevBit = last;
	bitsExt[V92P4M_BITS_BELOW + bitsPerSymbol - 1] = (unsigned char)last;
	sym = mapper->process(&bitsExt[V92P4M_BITS_BELOW]);
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
		scrambler.processAllOnes(&bitsExt[V92P4M_BITS_BELOW], n);
		bitsToSymbol->process(&bitsExt[V92P4M_BITS_BELOW], n);
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
		scrambler.processAllOnes(&bitsExt[V92P4M_BITS_BELOW], n);
		bitsToSymbol->process(&bitsExt[V92P4M_BITS_BELOW], n);
	}
	bitsToSymbol->process(n, &sym);
	return sym;
}

/*
 * resetBeforRRN (.text+0x16ea0, 81 B).
 *
 * THE FIRST TWO STORES ARE WRITTEN IN THE ORDER THE COMPILER REVERSES, NOT THE
 * ORDER IT EMITS.  The object emits `0x1c4` then `0x1c0`; GCC 3.4.2 at these
 * flags sinks the load of `cp` between the two and swaps them, so ascending
 * source order gives descending emission.  Both orders were compiled: the
 * source below is EXACT and the other spelling misses by exactly those two
 * bytes, which makes the map on this pair a bijection and the object's order
 * decodable.  Finding F7770.
 */
void V92Phase4Modulator::resetBeforRRN()
{
	word_1c0 = 0;
	word_1c4 = 0;
	symbolCount = 0;
	cp->word_110 = 0;
	word_28 = 1;
	word_2c = 0;
	word_30 = 0;
	word_34 = 0;
	word_38 = 0;
	flag_20 = 0;
}

/* resetBeforFPE (.text+0x16f00, 19 B). */
void V92Phase4Modulator::resetBeforFPE()
{
	symbolCount = 0;
	flag_3c = 1;
}


/*
 * enterRepeatedCP (.text+0x16f20, 139 B).  Not a `recived` handler and not
 * guarded by anything: it clears the two trace fields, announces the state and
 * rebuilds the message unconditionally.  The message goes through
 * `dsplibs_debug_printf` under `dsplibs_debug_level > 1` rather than through
 * `edprintf`, exactly as `recivedSUVtag`'s does.
 */
void V92Phase4Modulator::enterRepeatedCP()
{
	byte_1c = 0;
	word_18 = 0;

	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("V92Phase4Modulator: enter repeatedCPu "
				     "@ %d\r\n", symbolCount);

	state = V92P4M_STATE_REPEATED_CPU;
	cp->byte_00 = 0;
	cp->infoToBits();
	pattern = cp->getBitVector(patternLength);
	word_1b0 = patternLength / cp->bitsPerSymbol;
	symbolCount = 0;
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
 * ===========================================================================
 * THE SEGMENT BOUNDARIES
 *
 * The three resets belong with these and are no longer beside them: the
 * definitions in this file are ordered to the BLOB'S EMISSION ORDER, which is
 * not grouped by role.  See the note above the first definition.
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

/*
 * ===========================================================================
 * THE SIX HANDLERS THAT REBUILD THE CP MESSAGE
 *
 * `recivedSUV`, `recivedPartTwoSilenceRrnSUV`,
 * `recivedPartOneSilenceRrnSUVtag`, `recivedCPtag`, `recivedRt` and
 * `enterRepeatedCP`.  They are NOT contiguous and this banner does not head a
 * block: the definitions in this file are in the blob's emission order, which
 * is not grouped by role.  See the note above the first definition.  Each of
 * the six ends with the same four statements:
 *
 *     cp->byte_00 = k;
 *     cp->infoToBits();
 *     pattern = cp->getBitVector(patternLength);
 *     word_1b0 = patternLength / cp->bitsPerSymbol;
 *
 * -- pack the message, take the vector and its padded length, and turn that
 * length into a count in SYMBOLS.  The blob holds that block once per
 * function, with no call and no helper symbol anywhere in .text+0x16f20 ..
 * +0x1783a, so it is written out at each site rather than factored into a
 * helper the object does not have.  Finding F4754.
 *
 * `recivedSUV` and `recivedPartTwoSilenceRrnSUV` are 177 bytes each and the
 * same 177 bytes -- MEASURED, not asserted: the blob's two bodies compare
 * byte for byte equal.  Same guard, same modulus test, same
 * `.rodata.str1.4:0x3c40` string, same tail.  Two ordinary GLOBAL symbols, not
 * linkonce and not an alias, so the original spelled the body twice --
 * finding F1237's ruling for `reset` against the constructor, one class over.
 *
 * **THE FOUR-STATEMENT BLOCK IS CONTIGUOUS AND NOTHING BELONGS INSIDE IT.**
 * `word_1c4 = 1` in the two SUV handlers and `symbolCount = 0` in
 * `enterRepeatedCP` used to sit between `getBitVector` and the division,
 * because the object EMITS their stores there; GCC 3.4.2 at these flags sinks
 * an independent store into the division's schedule, so writing them after the
 * block is what produces the object's emission.  Three functions became
 * byte-identical when they were moved out.  Finding F7770 -- do not "tidy" them
 * back in.
 * ===========================================================================
 */

/*
 * recivedSUV (.text+0x17130, 177 B).  Acts once -- `word_1c4` is the latch --
 * and only out of SUV.  Off a period boundary it goes to 6 instead and leaves
 * the latch alone, so it can try again on the next symbol.
 */
void V92Phase4Modulator::recivedSUV()
{
	if (word_1c4 != 0 || state != V92P4M_STATE_SUV)
		return;

	if (symbolCount % word_1b0 != 0) {
		state = 6;
		return;
	}

	edprintf("V92Phase4Modulator: on recivedSUV enter CPu @ %d\r\n",
		 symbolCount);
	state = V92P4M_STATE_CPU;
	symbolCount = 0;
	cp->byte_00 = 0;
	cp->infoToBits();
	pattern = cp->getBitVector(patternLength);
	word_1b0 = patternLength / cp->bitsPerSymbol;
	word_1c4 = 1;
}

/* recivedPartOneSilenceRrnSUV (.text+0x171f0, 12 B). */
void V92Phase4Modulator::recivedPartOneSilenceRrnSUV()
{
	cp->byte_04 = 1;
}

/* recivedPartTwoSilenceRrnSUV (.text+0x17200, 177 B).  The same body again;
 * see the banner above for why it is repeated rather than shared. */
void V92Phase4Modulator::recivedPartTwoSilenceRrnSUV()
{
	if (word_1c4 != 0 || state != V92P4M_STATE_SUV)
		return;

	if (symbolCount % word_1b0 != 0) {
		state = 6;
		return;
	}

	edprintf("V92Phase4Modulator: on recivedSUV enter CPu @ %d\r\n",
		 symbolCount);
	state = V92P4M_STATE_CPU;
	symbolCount = 0;
	cp->byte_00 = 0;
	cp->infoToBits();
	pattern = cp->getBitVector(patternLength);
	word_1b0 = patternLength / cp->bitsPerSymbol;
	word_1c4 = 1;
}

/* recivedCP (.text+0x172c0, 34 B).  Two separate loads of `cp`, which is two
 * statements through the pointer and not one. */
void V92Phase4Modulator::recivedCP()
{
	word_1c0 = 1;
	cp->byte_04 = 1;
	cp->word_110 = 0;
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
 * recivedPartOneSilenceRrnSUVtag (.text+0x173c0, 294 B).
 *
 * `cp->byte_04` is BOTH the branch selector and the last store on every path,
 * including the one that returns because `flag_20` was already set: the object
 * reloads `cp` at +0x173ff and writes the byte again after the two arms have
 * each written it.  Reproduced as the trailing statement it is.
 */
void V92Phase4Modulator::recivedPartOneSilenceRrnSUVtag()
{
	if (flag_20 == 0) {
		if (cp->byte_04 == 1) {
			if (symbolCount % word_1b0 != 0) {
				state = 9;
			} else {
				edprintf("V92Phase4Modulator: enter E2u First "
					 "at RRN @ %d\r\n", symbolCount);
				state = V92P4M_STATE_E2U;
				symbolCount = 0;
				word_1b8 = (e2uExtended != 0) ? 13 : 12;
			}
		} else {
			cp->byte_04 = 1;

			if (symbolCount % word_1b0 != 0) {
				state = 8;
			} else {
				state = 10;
				symbolCount = 0;
				cp->byte_00 = 1;
				cp->infoToBits();
				pattern = cp->getBitVector(patternLength);
				word_1b0 = patternLength / cp->bitsPerSymbol;
			}
		}
		flag_20 = 1;
	}

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
 * recivedCPtag (.text+0x17500, 296 B).
 *
 * Two halves that share only their tail.  The FIRST CP tag -- `word_1c0`
 * clear -- raises the two CP flags and rebuilds the message; every one after
 * it takes `recivedSUVtag`'s transition instead, with the modulus test on the
 * outside and the state switch in its false arm, which is `recivedEd`'s
 * shape rather than `recivedSUVtag`'s.
 */
void V92Phase4Modulator::recivedCPtag()
{
	byte_1c = 0;
	word_18 = 0;

	if (word_1c0 == 0) {
		if (flag_20 != 0)
			return;

		word_1c0 = 1;
		cp->byte_04 = 1;
		cp->byte_00 = 1;

		if (symbolCount % word_1b0 != 0) {
			state = 8;
		} else {
			state = 10;
			symbolCount = 0;
			cp->infoToBits();
			pattern = cp->getBitVector(patternLength);
			word_1b0 = patternLength / cp->bitsPerSymbol;
		}

		flag_20 = 1;
		return;
	}

	if (flag_20 != 0)
		return;

	if (symbolCount % word_1b0 != 0) {
		switch (state) {
		case V92P4M_STATE_SUV:
			state = 9;
			break;
		case V92P4M_STATE_CPU:
		case V92P4M_STATE_REPEATED_CPU:
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
 * recivedRt (.text+0x17740, 251 B).
 *
 * The only member that WRITES `cp->bitsPerSymbol`, and the reason that field
 * has a name rather than an offset: `movzbl 0x43(%ebx),%eax; mov
 * %al,0x128(%edx)` at +0x177e6 copies this class's own `bitsPerSymbol` into
 * it.  It also clears the CP's `suv`.
 *
 * The gate is `symbolCount > 2399 && symbolCount % 12 == 0`; 2399 is the
 * object's literal (`cmp $0x95f; jbe`) and `< 2400` compiles to the same
 * branch, so which of the two the author wrote is not established.
 *
 * A NULL `cp` is a real arm here and not a defensive one: it sets state 29
 * and prints an ERROR, so the object expects to be able to reach it.
 */
void V92Phase4Modulator::recivedRt()
{
	if (state != 23 || word_38 == 0)
		return;

	if (symbolCount < 2400 || symbolCount % 12 != 0) {
		state = 24;
		return;
	}

	if (cp == 0) {
		state = 29;
		symbolCount = 0;

		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("V92Phase4Modulator: ERROR: Null "
					     "CP at RRN @ end of TRN2d\r\n");
		return;
	}

	edprintf("V92Phase4Modulator: on recivedRt enter SUV @ %d\r\n",
		 symbolCount);
	symbolCount = 0;
	patternIndex = 0;
	state = V92P4M_STATE_SUV;
	cp->bitsPerSymbol = bitsPerSymbol;
	cp->byte_00 = 1;
	cp->suv = 0;
	cp->infoToBits();
	pattern = cp->getBitVector(patternLength);
	word_1b0 = patternLength / bitsPerSymbol;
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
 * generateSymbol (.text+0x18050, 4,055 B) -- THE STATE MACHINE ITSELF
 *
 * One symbol per call.  Three statements happen whatever the state -- the
 * count advances, the report word is cleared, and the state selects an arm --
 * and then twenty-six arms each generate their segment's symbol and, where
 * the segment can end here, take the transition out of it.  `V92Modulator::
 * progress` calls it once per symbol of the block it is filling.
 *
 * ---------------------------------------------------------------------------
 * THE SWITCH IS DENSE 0..29 AND FOUR SLOTS ARE HOLES
 *
 *     1806f:  83 f8 1d      cmp  $0x1d,%eax
 *     18072:  77 0c         ja   <default>
 *     18074:  ff 24 85 ..   jmp  *0x624(,%eax,4)      <== R_386_32 .rodata
 *
 * -- a thirty-entry table at `.rodata+0x624`, of which slots 7, 14, 21 and 22
 * hold the default label.  GCC fills a dense table's gaps that way, so those
 * four are NOT written as arms: whether the source listed them is not
 * recoverable, and an empty `case 7:` would be a claim the object cannot
 * support.  The `ja` on a signed `state` is the same unsigned bound test GCC
 * emits for any switch, and every negative value lands in the default.
 *
 * ---------------------------------------------------------------------------
 * THE ARMS ARE CALLS, AND THAT IS MEASURED RATHER THAN ASSUMED
 *
 * `generateCPu` and `generateSUVu` survive as real calls with relocations on
 * them.  Every other generator's body appears inline -- there is no call to
 * `generateCPt`, `generateE1u`, `generateE2u`, `generateTRN2u`, `generateRu`,
 * `generateRuNot`, `generateDataSymbolBefore{FPE,RRN}`, `setMappingParams`,
 * `resetRRNSecondSection` or `enterRepeatedCP` anywhere in the function --
 * and they are written here as calls anyway, for three reasons:
 *
 *   1. Each inlined body brings its OWN stack slots.  The frame holds eight
 *      distinct `short` slots (+0x30, 0x32, 0x34, 0x36, 0x38, 0x3a, 0x3c,
 *      0x3e) and eight distinct `unsigned` ones, one pair per arm that needs
 *      them, where a single function-scope pair written out per arm would
 *      have been reused.  That is what inlining looks like and hand-written
 *      bodies do not.
 *   2. Inlining is the compiler's choice at `-O3` and not the source's
 *      (CLAUDE.md's "act on what the compiler was FORCED to encode").
 *   3. Nine of the ten bodies are UNIQUE to one member, so which member was
 *      called is not a guess.  The tenth is not, and it is called out below.
 *
 * THE ONE THING THAT IS NOT RECOVERABLE.  `generateRm` and `generateB1u` are
 * the same 149 bytes as each other, instruction for instruction (see their
 * banner above), so the five arms that inline that body -- states 16, 17, 26,
 * 27 and 28 -- name one of two indistinguishable members.  Neither the
 * differential tier nor the codegen tier can see the difference, and no
 * message fires on it.  The choice below follows the state each arm serves --
 * `generateB1u` where the state is B1u or its neighbours, `generateRm` where
 * it is Rm's -- and it is a CHOICE, not a reading.  Finding F4820.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE MESSAGES ESTABLISH
 *
 * Thirteen `.rodata.str1.4` strings, and six of them name a state code on the
 * instruction after the store -- 3, 10, 16, 17, 23 and 28, all now `#define`d
 * in the header.  The split between `edprintf` and `dsplibs_debug_printf` is
 * the object's and is reproduced per site: nine arms take the encoded channel
 * unconditionally and eight go through `dsplibs_debug_printf` under
 * `dsplibs_debug_level > 1`.
 *
 * ---------------------------------------------------------------------------
 * THE DIVISORS
 *
 * Six arms reduce `symbolCount` modulo `word_1b0`, two reduce it modulo
 * `patternLength`, three divide `patternLength` by `bitsPerSymbol` and four
 * by `cp->bitsPerSymbol`, and one takes `symbolCount % 12` through GCC's
 * `mul $0xaaaaaaab; shr $3` reciprocal.  Every one of them is an unsigned
 * `div` with no zero test in front of it, in the object as much as here --
 * see docs/deviations.md D700, which is also where D571's "can this be
 * reached" question is answered.
 * ===========================================================================
 */
int V92Phase4Modulator::generateSymbol()
{
	short sym;

	symbolCount++;
	word_0c = 0;

	switch (state) {
	case 0:
		sym = generateCPt();
		break;

	/* CPt again, and the boundary `exitCPt` misses: a whole number of
	 * PATTERN lengths past 24, not of `word_1b0`. */
	case 1:
		sym = generateCPt();
		if ((symbolCount - 24) % patternLength == 0) {
			edprintf("V92Phase4Modulator: enter E1u @ %d\r\n",
				 symbolCount);
			state = V92P4M_STATE_E1U;
			symbolCount = 0;
			word_1b8 = 12;
		}
		break;

	case V92P4M_STATE_E1U:
		sym = generateE1u();
		if (symbolCount == word_1b8) {
			state = V92P4M_STATE_TRN2U_MOD;
			symbolCount = 0;
			scrambler.reset(0);
		}
		break;

	case V92P4M_STATE_TRN2U_MOD:
		sym = generateTRN2u();
		break;

	/* TRN2u after the exit tag -- `exitTRN2u` is what takes 3 to 4 -- and
	 * the segment ends on a twelve-symbol boundary past 12599. */
	case 4:
		sym = generateTRN2u();
		if (symbolCount > 12599 && symbolCount % 12 == 0) {
			if (cp == 0) {
				state = 29;
				symbolCount = 0;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "V92Phase4Modulator: ERROR: Null CP"
					    " @ end of TRN2d\r\n");
			} else {
				edprintf("V92Phase4Modulator: on"
					 " TRN2uModulationExit enter SUV"
					 " @ %d\r\n", symbolCount);
				state = V92P4M_STATE_SUV;
				symbolCount = 0;
				patternIndex = 0;
				cp->bitsPerSymbol = bitsPerSymbol;
				cp->byte_00 = 1;
				cp->setSUV(word_2c);
				cp->infoToBits();
				pattern = cp->getBitVector(patternLength);
				word_1b0 = patternLength / bitsPerSymbol;
			}
		}
		break;

	/*
	 * SUV.  The message is repacked on every period boundary, and once
	 * the SUV has run `word_44 + 800` symbols past the point `byte_1c`
	 * started the count, the repeated CP takes over.
	 */
	case V92P4M_STATE_SUV:
		sym = generateSUVu();
		if (byte_1c != 0)
			word_18++;
		if (symbolCount % word_1b0 == 0 && symbolCount != 0) {
			cp->infoToBits();
			pattern = cp->getBitVector(patternLength);
			word_1b0 = patternLength / bitsPerSymbol;
			if (word_18 > word_44 + 800)
				enterRepeatedCP();
		}
		break;

	case 6:
		sym = generateSUVu();
		if (symbolCount % word_1b0 == 0) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "V92Phase4Modulator: on SUVuToCPuModulation"
				    " enter CPu @ %d\r\n", symbolCount);
			state = V92P4M_STATE_CPU;
			symbolCount = 0;
			cp->byte_00 = 0;
			cp->infoToBits();
			pattern = cp->getBitVector(patternLength);
			word_1c4 = 1;
			word_1b0 = patternLength / cp->bitsPerSymbol;
		}
		break;

	case 8:
		sym = generateSUVu();
		if (symbolCount % word_1b0 == 0) {
			edprintf("V92Phase4Modulator: enter FinalSUVu"
				 " @ %d\r\n", symbolCount);
			state = V92P4M_STATE_FINAL_SUVU;
			symbolCount = 0;
			cp->byte_00 = 1;
			cp->infoToBits();
			pattern = cp->getBitVector(patternLength);
			word_1b0 = patternLength / cp->bitsPerSymbol;
		}
		break;

	case 9:
		sym = generateSUVu();
		if (symbolCount % word_1b0 == 0) {
			edprintf("V92Phase4Modulator: enter E2u @ %d\r\n",
				 symbolCount);
			state = V92P4M_STATE_E2U;
			symbolCount = 0;
			word_1b8 = (e2uExtended != 0) ? 13 : 12;
		}
		break;

	/* 10 and 12 test EQUALITY with `word_1b0` where 9 and 11 test the
	 * remainder; the object's `cmp 0x1b0(%esi),%eax` against its
	 * `divl 0x1b0(%esi)` is the whole difference. */
	case V92P4M_STATE_FINAL_SUVU:
		sym = generateSUVu();
		if (symbolCount == word_1b0) {
			edprintf("V92Phase4Modulator: enter E2u @ %d\r\n",
				 symbolCount);
			state = V92P4M_STATE_E2U;
			symbolCount = 0;
			word_1b8 = (e2uExtended != 0) ? 13 : 12;
		}
		break;

	case 11:
		sym = generateCPu();
		if (symbolCount % word_1b0 == 0) {
			edprintf("V92Phase4Modulator: enter E2u @ %d\r\n",
				 symbolCount);
			state = V92P4M_STATE_E2U;
			symbolCount = 0;
			word_1b8 = (e2uExtended != 0) ? 13 : 12;
		}
		break;

	case V92P4M_STATE_CPU:
		sym = generateCPu();
		if (symbolCount == word_1b0) {
			edprintf("V92Phase4Modulator: CPu Terminated"
				 " @ %d\r\n", symbolCount);
			word_18 = 0;
			byte_1c = 1;
			state = V92P4M_STATE_SUV;
			symbolCount = 0;
			cp->byte_00 = 1;
			cp->infoToBits();
			pattern = cp->getBitVector(patternLength);
			word_1b0 = patternLength / cp->bitsPerSymbol;
		}
		break;

	/* The repeated CP repacks and stays where it is. */
	case V92P4M_STATE_REPEATED_CPU:
		sym = generateCPu();
		if (symbolCount % word_1b0 == 0 && symbolCount != 0) {
			cp->infoToBits();
			pattern = cp->getBitVector(patternLength);
			word_1b0 = patternLength / bitsPerSymbol;
		}
		break;

	/*
	 * E2u, and the busiest boundary in the function: four ways out, in
	 * the object's own test order.
	 */
	case V92P4M_STATE_E2U:
		sym = generateE2u();
		if (symbolCount == word_1b8) {
			if (word_28 != 0 && word_30 != 0 && word_34 == 0) {
				edprintf("V92Phase4Modulator: enter TRN2u"
					 " Second at RRN @ %d\r\n",
					 symbolCount);
				symbolCount = 0;
				state = V92P4M_STATE_TRN2U_SECOND;
				word_24 = (word_38 != 0) ? 4000 : 8004;
			} else if (mappingParams == 0) {
				state = 29;
				symbolCount = 0;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "V92Phase4Modulator: ERROR: Null"
					    " dataPhaseMappingParams @ end of"
					    " Ed\r\n");
			} else if (flag_3c == 0) {
				edprintf("V92Phase4Modulator: enter B1u"
					 " @ %d\r\n", symbolCount);
				state = V92P4M_STATE_B1U;
				setMappingParams(mappingParams);
				symbolCount = 0;
				scrambler.reset(0);
			} else {
				edprintf("V92Phase4Modulator: enter FB1u"
					 " @ %d\r\n", symbolCount);
				state = V92P4M_STATE_FB1U;
				symbolCount = 0;
				scrambler.reset(0);
			}
		}
		break;

	case V92P4M_STATE_B1U:
		sym = generateB1u();
		if (symbolCount == 576) {
			edprintf("V92Phase4Modulator: Phase4 Terminated"
				 " @ %d\r\n", symbolCount);
			state = V92P4M_STATE_TERMINATED;
			symbolCount = 0;
			word_0c = 9;
		}
		break;

	case V92P4M_STATE_FB1U:
		sym = generateB1u();
		if (symbolCount == 576) {
			edprintf("V92Phase4Modulator: enter B1u @ %d\r\n",
				 symbolCount);
			setMappingParams(mappingParams);
			state = V92P4M_STATE_B1U;
			symbolCount = 0;
		}
		break;

	case 18:
		sym = generateDataSymbolBeforeRRN();
		break;

	case V92P4M_STATE_RU:
		sym = generateRu();
		if (symbolCount == 384) {
			state = 20;
			symbolCount = 0;
		}
		break;

	case 20:
		sym = generateRuNot();
		if (symbolCount == 24) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "V92Phase4Modulator: RRN: enter"
				    " TRN2uModulation @ %d\r\n", symbolCount);
			state = V92P4M_STATE_TRN2U_MOD;
			symbolCount = 0;
			scrambler.reset(0);
			prevBit = 0;
		}
		break;

	/*
	 * The second TRN2u, whose length is `word_24` rather than a literal,
	 * and its post-tag twin.  The two arms differ in the message, in the
	 * bound, and in whether `cp->bitsPerSymbol` is refreshed.
	 */
	case V92P4M_STATE_TRN2U_SECOND:
		sym = generateTRN2u();
		if (symbolCount >= word_24 && symbolCount % 12 == 0) {
			if (cp == 0) {
				state = 29;
				symbolCount = 0;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "V92Phase4Modulator: ERROR: Null CP"
					    " @ RRN @ end of TRN2u\r\n");
			} else {
				edprintf("V92Phase4Modulator: RRN: Terminate"
					 " TRN2uSecond @ %d\r\n", symbolCount);
				resetRRNSecondSection();
				state = V92P4M_STATE_SUV;
				symbolCount = 0;
				patternIndex = 0;
				cp->byte_00 = 1;
				cp->suv = 0;
				cp->infoToBits();
				pattern = cp->getBitVector(patternLength);
				word_1b0 = patternLength / bitsPerSymbol;
			}
		}
		break;

	case 24:
		sym = generateTRN2u();
		if (symbolCount > 2399 && symbolCount % 12 == 0) {
			if (cp == 0) {
				state = 29;
				symbolCount = 0;
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "V92Phase4Modulator: ERROR: Null CP"
					    " @ RRN @ end of TRN2u\r\n");
			} else {
				edprintf("V92Phase4Modulator: on"
					 " TRN2uRrnSecondExit enter SUV"
					 " @ %d\r\n", symbolCount);
				resetRRNSecondSection();
				state = V92P4M_STATE_SUV;
				symbolCount = 0;
				patternIndex = 0;
				cp->bitsPerSymbol = bitsPerSymbol;
				cp->byte_00 = 1;
				cp->suv = 0;
				cp->infoToBits();
				pattern = cp->getBitVector(patternLength);
				word_1b0 = patternLength / bitsPerSymbol;
			}
		}
		break;

	case 25:
		sym = generateDataSymbolBeforeFPE();
		break;

	case V92P4M_STATE_RM:
		sym = generateRm();
		if (symbolCount == 384) {
			state = 27;
			symbolCount = 0;
			bitsToSymbol->transmitter->modulusEncoder->field_50 = 2;
		}
		break;

	case 27:
		sym = generateRm();
		if (symbolCount == 24) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "V92Phase4Modulator: FPE: enter SUVu"
				    " @ %d\r\n", symbolCount);
			state = V92P4M_STATE_SUV;
			symbolCount = 0;
			cp->bitsPerSymbol = bitsPerSymbol;
			cp->byte_00 = 1;
			cp->setSUV(word_2c);
			cp->infoToBits();
			pattern = cp->getBitVector(patternLength);
			word_1b0 = patternLength / bitsPerSymbol;
			setMappingParams(mappingParams);
		}
		break;

	case V92P4M_STATE_TERMINATED:
		sym = generateB1u();
		break;

	/* The one arm that generates nothing at all. */
	case 29:
		sym = 0;
		break;

	default:
		sym = 0;
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("V92Phase4Modulator: Illegal"
					     " state\r\n");
		break;
	}

	return sym;
}

/*
 * ===========================================================================
 * V92Phase4Modulator::reset (.text+0x19030, 290 bytes)
 *
 * The whole object back to a known state, the CP message repacked, and then
 * `nSymbols` symbols generated before returning.  The last of the class's 34
 * members, and the only one that writes `amplitude`, `byte_42`,
 * `bitsPerSymbol` or `word_44`.
 *
 * IN THE OBJECT'S ORDER, with nothing elided:
 *
 *     word_0c = 0                 word_44 = suvLimit
 *     amplitude = amplitudeArg    byte_42 = bitsArg
 *     state = stateArg            bitsPerSymbol = bitsArg + 2
 *     symbolCount = 0
 *     mapper->reset(amplitude, bitsArg)
 *     scrambler.reset(0)
 *     prevBit = 0
 *     word_1c0 = 0 ; cp->word_110 = 0 ; word_1c4 = 0
 *     word_28 = 0 ; flag_3c = 0 ; word_2c = 0 ; word_30 = 0 ; word_34 = 0
 *     word_18 = 0 ; byte_1c = 0 ; flag_20 = 0
 *     cp->bitsPerSymbol = 1
 *     cp->byte_00 = 0
 *     cp->infoToBits()
 *     pattern = cp->getBitVector(patternLength)
 *     e2uExtended = 0
 *     for (i = 0; i < nSymbols; i++) generateSymbol()
 *
 * THAT LIST IS THE OBJECT'S EMITTED ORDER AND IT IS NOT THE AUTHOR'S SOURCE
 * ORDER -- 617's ruling, and here it is measured rather than inherited.  Our
 * source is written in exactly that order and GCC hoists `byte_42` two slots,
 * so the map is not the identity.  The whole single-statement family was then
 * enumerated: every one of the seven positions of `byte_42 = bitsArg`, crossed
 * with both orders of `word_44`/`amplitude`, fourteen compiles.  **NONE of the
 * fourteen emits `0x44` before `0x40`, which the object does**, so the
 * remaining 46 bytes are not a permutation of these statements at all and no
 * spelling in the family can close them.  Closest was 27 of 290, in a spelling
 * that separates `byte_42` from `bitsPerSymbol`, and it was declined: closer
 * bytes are not a grade.  Finding F7771 -- do not re-run the search.
 *
 * THE AMPLITUDE HANDED TO THE MAPPER IS RE-READ FROM THE FIELD, not passed
 * through from the argument: `movswl 0x40(%esi),%eax` at .text+0x1907b, where
 * the argument's own sign-extension is two instructions earlier in %ebx and
 * has been overwritten.  Same value, different memory operand, and the operand
 * is forced -- so the source says `amplitude`, not `amplitudeArg`.
 *
 * WHAT IT DOES NOT WRITE, and each absence is checkable: `word_24`, `word_38`,
 * `word_1b0`, `word_1b8`, `patternLength` other than through `getBitVector`,
 * `mappingParams`, `bitsToSymbol`, `mapper`, `cp` and `params`.  `word_1b0` in
 * particular stays at whatever the constructor left, which is D700's first
 * divisor.
 *
 * ---------------------------------------------------------------------------
 * `bitsPerSymbol = bitsArg + 2` IS COMPUTED IN ONE BYTE AND WRAPS.  D571/D700.
 *
 *     19062:  88 56 42     mov %dl,0x42(%esi)      byte_42 = bitsArg
 *     19065:  80 c2 02     add $0x2,%dl            <- eight bits wide
 *     1906a:  88 56 43     mov %dl,0x43(%esi)      bitsPerSymbol = ...
 *
 * `add $0x2,%dl` is an eight-bit add on the eight-bit argument, which is what
 * C's integral promotion followed by truncation into an `unsigned char` field
 * is FORCED to produce, so the wrap is the declaration's and not a choice.  A
 * `bitsArg` of 254 leaves `bitsPerSymbol` at 0 and 255 leaves it at 1; nothing
 * in the 290 bytes tests either, and the constructor does not initialise the
 * field at all.
 *
 * **RESET ITSELF SURVIVES IT.**  The one division reset can reach is inside
 * `V92CP::infoToBits`, and the store two instructions above the call is
 * `movb $0x1,0x128(%ebx)` -- reset forces the CP's OWN `bitsPerSymbol` to 1
 * before packing, so `12 * bitsPerSymbol` is 12 whatever was passed here.  The
 * fault fires later and elsewhere: `recivedRt` copies THIS field into
 * `cp->bitsPerSymbol` (`movzbl 0x43(%ebx),%eax; mov %al,0x128(%edx)` at
 * .text+0x177e6), and the next `infoToBits` then divides by zero.
 *
 * So reset is the writer that CREATES the state D700 records and is not the
 * reader that trips over it.  Reproduced with no clamp and no test; a trial at
 * 254 is in t_v92p4reset.cpp and compares the state reset leaves, which is
 * observable on both sides, rather than driving the divide, which would raise
 * #DE identically on both and measure the CPU (D571's argument).
 *
 * ---------------------------------------------------------------------------
 * THE GENERATION LOOP is `for (i = 0; i < nSymbols; i++) generateSymbol();`
 * and the object's `test %edi,%edi; je` guard with a `dec`/`jne` body is what
 * GCC 3.4.2 at -O3 emits for it.  The return value is discarded at every
 * iteration, which is the object: nothing stores %eax between calls.
 * ===========================================================================
 */
void
V92Phase4Modulator::reset(short amplitudeArg, unsigned char bitsArg,
			  V92Phase4ModulatorState stateArg,
			  unsigned int nSymbols, unsigned int suvLimit)
{
	unsigned int i;

	word_0c = 0;
	word_44 = suvLimit;
	amplitude = amplitudeArg;
	byte_42 = bitsArg;
	state = stateArg;
	bitsPerSymbol = (unsigned char)(bitsArg + 2);
	symbolCount = 0;

	mapper->reset(amplitude, bitsArg);
	scrambler.reset(0);

	prevBit = 0;
	word_1c0 = 0;
	cp->word_110 = 0;
	word_1c4 = 0;
	word_28 = 0;
	flag_3c = 0;
	word_2c = 0;
	word_30 = 0;
	word_34 = 0;
	word_18 = 0;
	byte_1c = 0;
	flag_20 = 0;

	cp->bitsPerSymbol = 1;
	cp->byte_00 = 0;
	cp->infoToBits();

	pattern = cp->getBitVector(patternLength);
	e2uExtended = 0;

	for (i = 0; i < nSymbols; i++)
		generateSymbol();
}
