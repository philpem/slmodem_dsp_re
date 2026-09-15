/*
 * V90Phase3Modulator.cpp -- the V.90 / V.92 phase 3 downstream symbol source.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90Phase3Modulator.h` holds
 * the object map and the reasoning behind it.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x4(%esp),%eax` -- not %ecx, so these are not thiscall and
 * nothing here needs an attribute (finding F215).
 *
 * Built -fno-exceptions -fno-rtti -nostdinc++ like the rest of the C++ here;
 * see the Makefile.  No virtuals and no allocation, so the test binaries
 * still link with $(CC).
 */

#include <stddef.h>

extern "C" {
#include "dsplib/pcm.h"
#include "dsplib/debug.h"
#include "dsplib/encode.h"
}

#include "dsplib/V90Phase3Modulator.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but only parses `struct name {` out of include/dsplib and
 * compiles the result as C, so a C++ class asserts its own (finding F230).
 * This is the check that catches an object right in size and wrong by four in
 * every offset.
 *
 * Guarded on a 32-bit pointer because ten of the offsets below are pointers
 * or come after one: `make check64` compiles this file for the host purely to
 * prove the *code* does not depend on 32-bit, and the layout the blob has is
 * not something it can or should assert (src/v8/v8util.c has the same guard
 * for the same reason).
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define P3M_OFF(field, off, tag) \
	typedef char v90p3m_off_##tag[ \
	    ((int)__builtin_offsetof(V90Phase3Modulator, field) == (off)) \
	    ? 1 : -1]

P3M_OFF(sessionFlag,	0x000, sessionflag);
P3M_OFF(pcmType,	0x004, pcmtype);
P3M_OFF(timeoutBase,	0x008, timeoutbase);
P3M_OFF(codeLevel,	0x00c, codelevel);
P3M_OFF(codeLevelAlt,	0x00e, codelevelalt);
P3M_OFF(idleLevel,	0x012, idlelevel);
P3M_OFF(state,		0x014, state);
P3M_OFF(symbolCount,	0x018, symbolcount);
P3M_OFF(eventCode,	0x01c, eventcode);
P3M_OFF(scrambler,	0x020, scrambler);
P3M_OFF(polarity,	0x040, polarity);
P3M_OFF(jdBits,		0x044, jdbits);
P3M_OFF(jdV92Bits,	0x048, jdv92bits);
P3M_OFF(jdV92PhaseBits,	0x04c, jdv92phasebits);
P3M_OFF(params,		0x050, params);
P3M_OFF(dilCount,	0x054, dilcount);
P3M_OFF(seq1Length,	0x055, seq1length);
P3M_OFF(seq2Length,	0x056, seq2length);
P3M_OFF(seq1,		0x057, seq1);
P3M_OFF(seq2,		0x0d7, seq2);
P3M_OFF(segmentLength,	0x158, segmentlength);
P3M_OFF(segmentLevel,	0x178, segmentlevel);
P3M_OFF(dilLevel,	0x188, dillevel);
P3M_OFF(seq1Index,	0x388, seq1index);
P3M_OFF(seq2Index,	0x389, seq2index);
P3M_OFF(dilIndex,	0x38a, dilindex);
P3M_OFF(segmentPos,	0x38c, segmentpos);
P3M_OFF(segmentIndex,	0x390, segmentindex);
P3M_OFF(usingSegmentLevel, 0x392, usingsegmentlevel);
P3M_OFF(dilPcmCode,	0x394, dilpcmcode);
typedef char v90p3m_size[(sizeof(V90Phase3Modulator) == 0x398) ? 1 : -1];

/* The scrambler subobject's own map, asserted for the same reason. */
#define SCR_OFF(field, off, tag) \
	typedef char scrambler_off_##tag[ \
	    ((int)__builtin_offsetof(Scrambler<unsigned char, int>, field) \
	     == (off)) ? 1 : -1]

SCR_OFF(pLimit,		0x00, plimit);
SCR_OFF(pInitOut,	0x04, pinitout);
SCR_OFF(pInitTap1,	0x08, pinittap1);
SCR_OFF(pInitTap2,	0x0c, pinittap2);
SCR_OFF(pOut,		0x10, pout);
SCR_OFF(pTap1,		0x14, ptap1);
SCR_OFF(pTap2,		0x18, ptap2);
SCR_OFF(tailLength,	0x1c, taillength);
typedef char scrambler_size[
    (sizeof(Scrambler<unsigned char, int>) == 0x20) ? 1 : -1];

#endif /* 32-bit host */

/*
 * The G.711 segment boundaries, extracted from .data:0x000440 with
 * tools/tabdump.py and re-read as two rows of eight signed ints, which is
 * what both users say it is: `resetDILGenerator` and `calculateDilLength`
 * index it at `8 * pcmType + segment` with a four-byte scale and compare
 * against it with a signed branch.
 *
 * Row 0 is the mu-law endpoints, 124 + 256 * (2**k - 1); row 1 is the A-law
 * endpoints, 256 << k.  Both are the 16-bit-scale values dsplibs' own
 * `ulaw2linear` and `alaw2linear` produce (src/service/pcm.c), which is why
 * the last A-law entry is 32768 and does not fit a short.
 *
 * It is a static data member and therefore a defined data symbol in the blob,
 * renamed `ref_*` like everything else, so it has to exist here or the whole
 * suite fails to link.  Non-const to match: the blob's symbol is `D`.
 */
int V90Phase3Modulator::codeSegmentsBoundriesLookupTable[2][8] = {
	{   124,   380,   892,  1916,  3964,  8060, 16252, 32636 },
	{   256,   512,  1024,  2048,  4096,  8192, 16384, 32768 }
};

void
V90Phase3Modulator::setSessionFlag(unsigned int flag)
{
	sessionFlag = flag;
}

/*
 * One symbol of the six-symbol Sd pattern, chosen by `(symbolCount - 1) % 6`
 * through a jump table.  The six entries are the ones at .rodata:0x9f4;
 * `generateSd`'s own table at 0x984 holds the identical sequence, which is
 * what names the state.  The modulus is unsigned -- the object divides by six
 * with the 0xaaaaaaab reciprocal and an unsigned shift.
 */
static short
sdSymbol(const V90Phase3Modulator *m)
{
	switch ((m->symbolCount - 1u) % 6u) {
	case 0:
	case 2:
		return m->codeLevelAlt;
	case 1:
		return m->idleLevel;
	case 3:
	case 5:
		return (short)-m->codeLevelAlt;
	case 4:
		return (short)-m->idleLevel;
	}
	return 0;		/* unreachable: a remainder mod 6 is < 6 */
}

int
V90Phase3Modulator::generateSd()
{
	return sdSymbol(this);
}

/*
 * Take a DIL descriptor and expand it into the generator's own state: the two
 * byte sequences copied verbatim, the eight segment lengths as 6 * size + 6,
 * and every PCM code turned into a linear level by the companding law in
 * force.
 *
 * The code-to-level conversion is the blob's, not G.711's plain inverse: the
 * descriptor holds a seven-bit magnitude, and the sign/company bits are
 * supplied here -- `(code & 0x7f) ^ 0xd5` for A-law and `~(code & 0x7f)` for
 * mu-law, which is `(code & 0x7f) ^ 0xff`.  Both produce a code in 0x80..0xff,
 * so every level is on the same side of zero.
 *
 * A null descriptor clears `dilCount` and returns, which is the whole of the
 * function's error handling.
 */
void
V90Phase3Modulator::resetDILGenerator(const tagV90DILdescriptor *d)
{
	unsigned int i;
	unsigned int level;

	if (d == NULL) {
		dilCount = 0;
		return;
	}

	dilCount = d->dilCount;
	seq1Length = d->seq1Length;
	seq2Length = d->seq2Length;

	/*
	 * The two lengths are bytes and the two destinations are 128 apart, so
	 * a descriptor claiming more than 128 would run one sequence into the
	 * next.  That is what the object does -- the loop bound is the byte,
	 * with no clamp -- and it is preserved rather than corrected.  The
	 * copies read their bound from the descriptor, as the object does;
	 * the DIL loop below reads its own field back, as the object also
	 * does.
	 */
	for (i = 0; i < (unsigned int)d->seq1Length; i++)
		seq1[i] = d->seq1[i];
	for (i = 0; i < (unsigned int)d->seq2Length; i++)
		seq2[i] = d->seq2[i];

	for (i = 0; i <= 7; i++) {
		segmentLength[i] = 6u * (unsigned int)d->segmentSize[i] + 6u;
		if (pcmType != PCM_TYPE_MU_LAW)
			segmentLevel[i] = (short)alaw2linear(
			    (unsigned char)((d->segmentCode[i] & 0x7f) ^ 0xd5));
		else
			segmentLevel[i] = (short)ulaw2linear(
			    (unsigned char)((d->segmentCode[i] & 0x7f) ^ 0xff));
	}

	for (i = 0; i < (unsigned int)dilCount; i++) {
		if (pcmType != PCM_TYPE_MU_LAW)
			dilLevel[i] = (short)alaw2linear(
			    (unsigned char)((d->dilCode[i] & 0x7f) ^ 0xd5));
		else
			dilLevel[i] = (short)ulaw2linear(
			    (unsigned char)((d->dilCode[i] & 0x7f) ^ 0xff));
	}

	seq1Index = 0;
	seq2Index = 0;
	dilIndex = 0;
	segmentPos = 0;

	/*
	 * Which G.711 segment the first DIL level falls in.  The level is read
	 * back out of `dilLevel[0]` as UNSIGNED sixteen bits -- the blob's
	 * `movzwl` -- and compared against the row for the law in force; the
	 * index runs one past the end of the row when no boundary matches,
	 * which is what the object does and is preserved here.
	 */
	level = (unsigned int)(unsigned short)dilLevel[0];
	for (i = 0; i <= 7; i++) {
		if ((int)level <=
		    codeSegmentsBoundriesLookupTable[(int)pcmType][i])
			break;
	}
	segmentIndex = (unsigned char)i;
}

/*
 * ===========================================================================
 * The pieces `generateV90Symbol` and `generateV92Symbol` inline.
 *
 * The object has every one of these as a method of its own -- `generateSd`,
 * `generateSdNot`, `generateTRN1d`, `generateJd`, `generateJdNot`,
 * `generateV92Jd`, `generateJdPhase`, `generateDIL`,
 * `updateCodeSegmentPointer` -- and then inlines the bodies into the two
 * symbol generators rather than calling them, which is why those two are 1790
 * and 2044 bytes of otherwise repetitive code.  They are file-static
 * functions here rather than the methods they correspond to for the reason
 * docs/v90cpp.md gives: defining a method whose callees are not written yet
 * re-opens the link closure for the whole test suite.  Nothing outside this
 * file needs them.
 *
 * `generateDIL` IS NO LONGER ONE OF THEM.  It is a real method, defined
 * below, because its three relocations -- `linear2alaw`, `linear2ulaw` and
 * `codeSegmentsBoundriesLookupTable` -- all exist, so defining it CLOSES the
 * link rather than re-opening it.  `updateCodeSegmentPointer` stays a static
 * (`updateCodeSegment`) because the blob inlines it into `generateDIL` and
 * claiming its own symbol is a separate piece of work.
 * ===========================================================================
 */

/* Its inversion, .rodata:0xa0c, matching `generateSdNot`'s table at 0x99c. */
static short
sdNotSymbol(const V90Phase3Modulator *m)
{
	switch ((m->symbolCount - 1u) % 6u) {
	case 0:
	case 2:
		return (short)-m->codeLevelAlt;
	case 1:
		return (short)-m->idleLevel;
	case 3:
	case 5:
		return m->codeLevelAlt;
	case 4:
		return m->idleLevel;
	}
	return 0;		/* unreachable, as above */
}

/*
 * One scrambled, differentially encoded symbol -- the body `generateJd`,
 * `generateJdNot`, `generateV92Jd` and `generateJdPhase` all share, differing
 * only in the bit they feed the scrambler.  The scrambler's output is one
 * byte, so the exclusive-or with `polarity` is the object's full 32-bit `xor`
 * either way.
 */
static short
scrambledSymbol(V90Phase3Modulator *m, unsigned char in)
{
	m->polarity ^= m->scrambler.process(in);
	return m->polarity ? m->codeLevel : (short)-m->codeLevel;
}

/*
 * The bit of a 72-entry vector this symbol carries.  `symbolCount` has
 * already been incremented, so the first symbol of a repetition takes entry
 * zero.  Unsigned again: the object divides by 72 with 0x38e38e39 and `shr`.
 */
static unsigned char
vectorBit(const unsigned char *bits, unsigned int symbolCount)
{
	return bits[(symbolCount - 1u) % 72u];
}

/*
 * `updateCodeSegmentPointer`, inlined: which G.711 segment the current DIL
 * level falls in.  Identical to the search at the end of `resetDILGenerator`
 * -- the level is read back as UNSIGNED sixteen bits and the index runs one
 * past the end of the row when no boundary matches.
 */
static void
updateCodeSegment(V90Phase3Modulator *m)
{
	unsigned int level =
	    (unsigned int)(unsigned short)m->dilLevel[m->dilIndex];
	unsigned int i;

	for (i = 0; i <= 7; i++) {
		if ((int)level <=
		    V90Phase3Modulator::codeSegmentsBoundriesLookupTable
			[(int)m->pcmType][i])
			break;
	}
	m->segmentIndex = (unsigned char)i;
}

/*
 * ===========================================================================
 * THE EIGHT LEAF METHODS, claimed by the VPcmV34Main leaf pass.
 *
 * The object emits every one of these as a `T` symbol of its own AND inlines
 * the identical body into the two symbol pumps; the file-static helpers
 * above are those bodies, so each method below is one call the compiler
 * inlines straight back.  Blob addresses, in emission order: `generateSd`
 * 0x2ac60, `generateSdNot` 0x2acf0, `updateCodeSegmentPointer` 0x2ae90 --
 * then, after `generateDIL` (0x2b070): `generateJdNot` 0x2b220, `generateJd`
 * 0x2b260, `generateTRN1d` 0x2b2b0, `generateJdPhase` 0x2b9f0 and
 * `generateV92Jd` 0x2ba40, which sit further down this file for the same
 * emission-order reason (F7796).
 *
 * THE RETURN TYPE IS `int` FOR THE SEVEN GENERATORS, read off the standalone
 * bodies: each ends `movswl %dx,%eax` (or `mov %ebx,%eax` of a `movswl`'d
 * value), the CALLEE widening a short -- where a `short` return leaves the
 * widening to the caller, as `V90Phase4Modulator`'s sequence readers do.
 * The header used to spell them `void` with the usual not-measured caveat;
 * the standalone bodies are the measurement.
 *
 * Their unreachable default arms differ from the helpers': past the `%6u`
 * the standalone bodies return whatever is in a callee-saved register, the
 * helpers return 0, and no input reaches either.
 * ===========================================================================
 */
int
V90Phase3Modulator::generateSdNot()
{
	return sdNotSymbol(this);
}

/* 0x2ae90, 59 bytes: the helper above, standalone. */
void
V90Phase3Modulator::updateCodeSegmentPointer()
{
	updateCodeSegment(this);
}

/*
 * ===========================================================================
 * `V90Phase3Modulator::generateDIL` -- .text+0x2b070, 0x1aa = 426 bytes.
 *
 * One symbol of the digital impairment learning sequence, and the four
 * cursors it steps.
 *
 * IT IS A METHOD HERE AND WAS THE FILE-STATIC `dilSymbol` BEFORE, and the
 * difference is the whole point of claiming it.  The blob carries
 * `generateDIL` as a `T` symbol AND inlines the identical body into
 * `generateV90Symbol` and `generateV92Symbol` rather than calling it -- the
 * same treatment `generateSd`, `generateJd` and the rest get.  Defining the
 * method in this translation unit and calling it from the two generators is
 * what reproduces both halves of that: GCC 3.4.2 at -O3 inlines it at all
 * four call sites, and still emits the out-of-line copy because an external
 * symbol has to exist whether or not anything reaches it.  Measured rather
 * than assumed, with tools/instrcount.py over the period toolchain -- the
 * generators are 560 and 696 instructions before this change and 560 and 696
 * after it, unchanged to the instruction, and the standalone copy appears
 * beside them at 98 against the blob's 117.
 *
 * NOTHING CALLS IT, IN THE BLOB OR IN THE PERIOD BUILD, AND NOTHING MAY BE
 * ADDED THAT DOES.  `readelf -rW` over the 1.2 MB object finds ZERO
 * relocations of any kind naming `_ZN18V90Phase3Modulator11generateDILEv`,
 * against two naming `generateV90Symbol`, which is what shows the
 * measurement can fire at all; the period object is zero as well.  A source
 * call the compiler declined to inline would appear as exactly such a
 * relocation, so that one measurement is both the callerless proof and the
 * proof that the inlining shape is the object's.
 *
 * THE MODERN BUILD MAKES THE OTHER CHOICE AND THAT IS NOT A DEFECT.  GCC 13
 * at -O2 declines to inline it and leaves four `call` relocations in
 * build/repro; the differential tier is green either way, because the
 * inlining decision changes no behaviour.  The compiler that decides the
 * SHAPE is the period one, and it agrees with the blob.
 *
 * THE 19-INSTRUCTION GAP IS ACCOUNTED FOR AND IS NOT A MISSING CALL --
 * instrcount.py's standing reading, and worth writing down because that is
 * the failure it exists to catch.  Each side has exactly TWO `call`s and
 * THREE relocations, so nothing is missing.
 *
 * THIRTEEN of the nineteen are alignment padding counted inside `st_size`:
 * the blob carries seventeen filler instructions (three `lea 0x0(%e_,%eiz,1)`
 * forms, one `lea 0x0(%esi),%esi` and thirteen `nop`s, most of them ahead of
 * the search loop at 0x2b1f0) against four in ours.  Real instructions are
 * 100 against 94.
 *
 * THE REMAINING SIX are one shape, seen three times: the blob re-reads what
 * ours keeps.  It evaluates `seq2[seq2Index] == 0` twice, once for the branch
 * at 0x2b08c and once for the `sete` at 0x2b0af, where ours computes the flag
 * once; it re-reads `seq1Index` and `seq2Index` out of the object after the
 * companding call (0x2b0dd, 0x2b0fc) where ours spills the two locals to the
 * stack; and its ternary branches out to a tail at 0x2b180 that repeats four
 * instructions ours reaches by falling through.
 *
 * The re-reads are a FORCED difference and a real observation about the
 * original's source: a memory read cannot be moved across an opaque call in
 * either direction, so the author's code reads those members after the
 * companding call rather than caching them in locals as `i1` and `i2` do
 * here.  It is left as it stands because this body is the one the
 * differential tier has already proved, and rewriting it would move the two
 * generators' counts and with them the evidence above.  Worth a pass of its
 * own; it is a codegen-tier gain and no test can see it.
 *
 * THE RETURN TYPE IS `int`, AND ONLY THE INSTRUCTIONS SAY SO.  A mangled name
 * carries the parameter types and not the return type, so `...generateDILEv`
 * settles nothing.  The two loads that build the value are `movswl` --
 * 0x188(%ebx,%esi,2) for `dilLevel[dilIndex]` and 0x178(%ebx,%edx,2) for
 * `segmentLevel[segmentIndex]` -- and those are NOT evidence: the upper half
 * of a load feeding a `short` is free either way, and finding F614 is the
 * standing ruling against reading anything into it.  What IS forced is the
 * re-extension after the negation, `neg %ecx ; movswl %cx,%edi` at 0x2b10f.
 * Truncating the negated value to sixteen bits is demanded by `short level`
 * however the function returns; widening it back to thirty-two is demanded
 * only if a 32-bit consumer wants it, and its only consumer is the
 * `mov %edi,%eax` at both `ret`s.  Under a `short` return the upper half of
 * %eax is dead and that extension would not be emitted.  It is the same
 * argument on the same evidence that the header already records for
 * `generateV90Symbol` and `generateV92Symbol`, whose every path ends
 * `movswl %si,%esi ; mov %esi,%eax`.
 *
 * `seq2` chooses the level -- a zero there means this symbol carries the
 * segment's own boundary level rather than the DIL entry's -- and `seq1`
 * chooses its sign.  The PCM CODE stored in `dilPcmCode` is the DIL entry's
 * either way; it does not follow the level.  Both index bytes wrap at their
 * sequence's length rather than at 256, and a length of zero therefore never
 * wraps them, which is the object's behaviour and is preserved.
 *
 * The two companding arms are two of the symbol's three relocations: mu-law
 * goes through `linear2ulaw` and is complemented (`not %al`), anything else
 * goes through `linear2alaw` and is exclusive-ored with 0xd5.  The third is
 * `codeSegmentsBoundriesLookupTable`, reached because
 * `updateCodeSegmentPointer` is inlined into the restart arm -- its own
 * symbol is not among the relocations, which is what says it is inlined and
 * not called.
 *
 * Reaching `segmentLength[segmentIndex]` restarts the segment: all three
 * cursors go to zero, `dilIndex` advances modulo `dilCount`, and the segment
 * index is recomputed from the new level.  `segmentIndex` can be 8 -- one
 * past the end of both eight-element arrays -- which is what the object's own
 * search produces when no boundary matches; the reads that follow land inside
 * the object either way and are reproduced rather than corrected.
 * ===========================================================================
 */
int
V90Phase3Modulator::generateDIL()
{
	unsigned char i1 = seq1Index;
	unsigned char i2 = seq2Index;
	int fromSegment = (seq2[i2] == 0);
	unsigned short code = (unsigned short)dilLevel[dilIndex];
	unsigned char next1, next2;
	unsigned int pos;
	short level;

	level = fromSegment ? segmentLevel[segmentIndex]
			    : dilLevel[dilIndex];
	usingSegmentLevel = (short)fromSegment;

	if (pcmType != PCM_TYPE_MU_LAW)
		dilPcmCode = (unsigned char)(linear2alaw((int)code) ^ 0xd5);
	else
		dilPcmCode = (unsigned char)~linear2ulaw((int)code);

	next2 = (unsigned char)(i2 + 1);
	if (next2 == seq2Length)
		next2 = 0;

	if (seq1[i1] == 0)
		level = (short)-level;

	next1 = (unsigned char)(i1 + 1);
	if (next1 == seq1Length)
		next1 = 0;

	pos = segmentPos + 1u;
	if (pos == segmentLength[segmentIndex]) {
		segmentPos = 0;
		seq2Index = 0;
		seq1Index = 0;
		dilIndex = (unsigned char)(dilIndex + 1);
		if (dilIndex == dilCount)
			dilIndex = 0;
		updateCodeSegment(this);
	} else {
		seq1Index = next1;
		segmentPos = pos;
		seq2Index = next2;
	}

	return level;
}

/* 0x2b220, 51 bytes: the shared scrambled-symbol body with a constant 0. */
int
V90Phase3Modulator::generateJdNot()
{
	return scrambledSymbol(this, 0);
}

/* 0x2b260, 80 bytes: the 72-bit Jd vector through the scrambler. */
int
V90Phase3Modulator::generateJd()
{
	return scrambledSymbol(this, vectorBit(jdBits, symbolCount));
}

/*
 * 0x2b2b0, 60 bytes.  NOT the shared body: the scrambler is driven with a
 * constant 1 and its raw output picks the sign directly -- `polarity` is
 * neither read nor written, which is what separates TRN1d from JdNot.
 * A conditional expression reproduces the object's separate narrow load and
 * sign extension on both return paths. The remaining two differing bytes
 * only select the register holding the constant input bit (F10212).
 */
int
V90Phase3Modulator::generateTRN1d()
{
	return scrambler.process(1) ? codeLevel : (short)-codeLevel;
}

/* 0x2b9f0 and 0x2ba40, 80 bytes each: the two V.92 vectors, same body. */
int
V90Phase3Modulator::generateJdPhase()
{
	return scrambledSymbol(this, vectorBit(jdV92PhaseBits, symbolCount));
}

int
V90Phase3Modulator::generateV92Jd()
{
	return scrambledSymbol(this, vectorBit(jdV92Bits, symbolCount));
}

/*
 * THE Jd TIMEOUT IS COMPUTED IN THE x87, AND THE DIL TIMEOUT IS NOT.
 *
 * `symbolCount == timeoutBase + 40000` is a plain 32-bit integer compare in
 * the object, wraparound and all.  This one is not: both counts go through
 * `fildll` -- the unsigned-to-floating conversion, high word zeroed -- the
 * constant arrives as `fadds` from a four-byte 24804.0, and the comparison is
 * `fcompp`.  With -mfpmath=387 and no rounding between, the sum is exact in
 * the register's 64-bit mantissa, so the test is `symbolCount == timeoutBase +
 * 24804` over the INTEGERS and not modulo 2**32.  The two disagree only when
 * the sum crosses 2**32, and the differential test drives exactly that case.
 *
 * Written as the object writes it rather than widened to 64-bit integers,
 * because the object's arithmetic is the specification; GCC 13 at -O2 with
 * -mfpmath=387 emits fildq/faddp/fucomip and keeps the excess precision.
 */
static int
jdTimeoutReached(unsigned int symbolCount, unsigned int timeoutBase)
{
	return (float)symbolCount == (float)timeoutBase + 24804.0f;
}

/*
 * One downstream symbol, V.90.
 *
 * `symbolCount` is incremented before anything else, so every count compared
 * below is the count including this symbol.  The dispatch is a sixteen-entry
 * jump table with an unsigned bound -- `cmp $0xf,%eax; ja` -- so a state
 * outside 0..15 takes the illegal-state arm exactly as 4, 5, 6 and 13 do.
 * Those four are the states V.92 owns; `generateV92Symbol` returns the
 * compliment by treating 7 as illegal.
 */
int
V90Phase3Modulator::generateV90Symbol()
{
	short sample = 0;

	symbolCount++;

	switch ((unsigned int)state) {
	case P3M_STATE_SD:
		eventCode = 0;
		sample = sdSymbol(this);
		if (symbolCount == 0x180) {
			state = P3M_STATE_SD_NOT;
			symbolCount = 0;
		}
		break;

	case P3M_STATE_SD_NOT:
		sample = sdNotSymbol(this);
		if (symbolCount == 0x30) {
			state = P3M_STATE_TRN1D;
			eventCode = 1;
			symbolCount = 0;
			scrambler.reset(0);
		} else {
			eventCode = 0;
		}
		break;

	case P3M_STATE_TRN1D:
		eventCode = 0;
		sample = scrambler.process(1) ? codeLevel
					      : (short)-codeLevel;
		if (symbolCount == 0x3e7c) {
			if (jdBits != NULL) {
				state = P3M_STATE_JD;
				eventCode = 2;
				polarity = (sample > 0);
			} else {
				state = P3M_STATE_ERROR;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Phase3Modulator: ERROR: Null " "JdBits @ end of TRN1d\r\n");
			}
			symbolCount = 0;
		}
		break;

	case P3M_STATE_JD:
		eventCode = 0;
		sample = scrambledSymbol(this, vectorBit(jdBits, symbolCount));
		if (jdTimeoutReached(symbolCount, timeoutBase)) {
			state = P3M_STATE_JD_TIMEOUT;
			symbolCount = 0;
			edprintf("V90Phase3Modulator: Jd TimeOut\r\n");
		}
		break;

	case P3M_STATE_JD_END:
		eventCode = 0;
		sample = scrambledSymbol(this, vectorBit(jdBits, symbolCount));
		if (symbolCount % 72u == 0) {
			state = P3M_STATE_JD_NOT;
			symbolCount = 0;
		}
		break;

	case P3M_STATE_JD_NOT:
		eventCode = 0;
		sample = scrambledSymbol(this, 0);
		if (symbolCount == 12) {
			if (dilCount != 0) {
				state = P3M_STATE_DIL;
			} else {
				state = P3M_STATE_ERROR;
				edprintf("V90Phase3Modulator: ERROR: Null DIL " "@ end of JdNOT\r\n");
			}
			symbolCount = 0;
		}
		break;

	case P3M_STATE_DIL:
		eventCode = 0;
		sample = generateDIL();
		if (symbolCount == timeoutBase + 40000u) {
			state = P3M_STATE_DIL_TIMEOUT;
			symbolCount = 0;
			edprintf("V90Phase3Modulator: DIL TimeOut\r\n");
		}
		break;

	case P3M_STATE_DIL_END:
		eventCode = 0;
		sample = generateDIL();
		if (segmentPos == 0) {
			edprintf("V90Phase3Modulator: Phase3 Terminated "
				 "@ %d\r\n", (int)symbolCount);
			state = P3M_STATE_TERMINATED;
			symbolCount = 0;
			eventCode = 6;
		}
		break;

	case P3M_STATE_TERMINATED:
	case P3M_STATE_JD_TIMEOUT:
	case P3M_STATE_DIL_TIMEOUT:
	case P3M_STATE_ERROR:
		eventCode = 0;
		sample = 0;
		break;

	default:
		eventCode = 0;
		sample = 0;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90Phase3Modulator: Illegal state\r\n");
		break;
	}

	return sample;
}

/*
 * One downstream symbol, V.92.
 *
 * The same machine with the V.92 message sequence in place of the V.90 one.
 * Sd, SdNot, TRN1d, JdNot and both DIL states are identical; states 3 and 4
 * carry `jdV92Bits` where V.90's 3 and 7 carry `jdBits`, and states 5 and 6
 * carry `jdV92PhaseBits`, which V.90 has no state for at all.  Note that
 * state 2's null check is on `jdV92Bits` here, and that nothing ever checks
 * `jdV92PhaseBits` -- states 5 and 6 dereference it unconditionally.
 *
 * The JdPhase timeout is the one asymmetry worth naming: it is an ABSOLUTE
 * 24804 symbols, `flds` then `fildll` then `fcompp`, with no `timeoutBase`
 * added, where the Jd timeout two states earlier adds it.
 */
int
V90Phase3Modulator::generateV92Symbol()
{
	short sample = 0;

	symbolCount++;

	switch ((unsigned int)state) {
	case P3M_STATE_SD:
		eventCode = 0;
		sample = sdSymbol(this);
		if (symbolCount == 0x180) {
			state = P3M_STATE_SD_NOT;
			symbolCount = 0;
		}
		break;

	case P3M_STATE_SD_NOT:
		sample = sdNotSymbol(this);
		if (symbolCount == 0x30) {
			state = P3M_STATE_TRN1D;
			eventCode = 1;
			symbolCount = 0;
			scrambler.reset(0);
		} else {
			eventCode = 0;
		}
		break;

	case P3M_STATE_TRN1D:
		eventCode = 0;
		sample = scrambler.process(1) ? codeLevel
					      : (short)-codeLevel;
		if (symbolCount == 0x3e7c) {
			if (jdV92Bits != NULL) {
				state = P3M_STATE_JD;
				eventCode = 2;
				polarity = (sample > 0);
			} else {
				state = P3M_STATE_ERROR;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V90Phase3Modulator: ERROR: Null " "v92JdBits @ end of TRN1d\r\n");
			}
			symbolCount = 0;
		}
		break;

	case P3M_STATE_JD:
		eventCode = 0;
		sample = scrambledSymbol(this,
		    vectorBit(jdV92Bits, symbolCount));
		if (jdTimeoutReached(symbolCount, timeoutBase)) {
			state = P3M_STATE_JD_TIMEOUT;
			symbolCount = 0;
			edprintf("V90Phase3Modulator: V92Jd TimeOut\r\n");
		}
		break;

	case P3M_STATE_V92JD_END:
		eventCode = 0;
		sample = scrambledSymbol(this,
		    vectorBit(jdV92Bits, symbolCount));
		if (symbolCount % 72u == 0) {
			state = P3M_STATE_JD_PHASE;
			symbolCount = 0;
			eventCode = 3;
		}
		break;

	case P3M_STATE_JD_PHASE:
		eventCode = 0;
		sample = scrambledSymbol(this,
		    vectorBit(jdV92PhaseBits, symbolCount));
		if ((float)symbolCount == 24804.0f) {
			state = P3M_STATE_JD_PHASE_TIMEOUT;
			symbolCount = 0;
			edprintf("V90Phase3Modulator: V92JdPhase TimeOut\r\n");
		}
		break;

	case P3M_STATE_JD_PHASE_END:
		eventCode = 0;
		sample = scrambledSymbol(this,
		    vectorBit(jdV92PhaseBits, symbolCount));
		if (symbolCount % 72u == 0) {
			state = P3M_STATE_JD_NOT;
			symbolCount = 0;
		}
		break;

	case P3M_STATE_JD_NOT:
		eventCode = 0;
		sample = scrambledSymbol(this, 0);
		if (symbolCount == 12) {
			if (dilCount != 0) {
				state = P3M_STATE_DIL;
			} else {
				state = P3M_STATE_ERROR;
				edprintf("V90Phase3Modulator: ERROR: Null DIL " "@ end of JdNOT\r\n");
			}
			symbolCount = 0;
		}
		break;

	case P3M_STATE_DIL:
		eventCode = 0;
		sample = generateDIL();
		if (symbolCount == timeoutBase + 40000u) {
			state = P3M_STATE_DIL_TIMEOUT;
			symbolCount = 0;
			edprintf("V90Phase3Modulator: DIL TimeOut\r\n");
		}
		break;

	case P3M_STATE_DIL_END:
		eventCode = 0;
		sample = generateDIL();
		if (segmentPos == 0) {
			edprintf("V90Phase3Modulator: Phase3 Terminated "
				 "@ %d\r\n", (int)symbolCount);
			state = P3M_STATE_TERMINATED;
			symbolCount = 0;
			eventCode = 6;
		}
		break;

	case P3M_STATE_TERMINATED:
	case P3M_STATE_JD_TIMEOUT:
	case P3M_STATE_JD_PHASE_TIMEOUT:
	case P3M_STATE_DIL_TIMEOUT:
	case P3M_STATE_ERROR:
		eventCode = 0;
		sample = 0;
		break;

	default:
		eventCode = 0;
		sample = 0;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90Phase3Modulator: Illegal state\r\n");
		break;
	}

	return sample;
}

/*
 * Start a phase 3 transmission, and optionally run it forward.
 *
 * The order is the object's: the scrambler first, then the fields that do not
 * depend on the law, then the three levels, then the bit vectors, then the
 * DIL generator, and only then the symbols.  `resetDILGenerator` is called on
 * both paths and with whatever descriptor came in, including null.
 *
 * THE TWO Jd PATHS ARE NOT SYMMETRIC.  `sessionFlag` decides between them,
 * and each writes only its own pointers: the V.90 path writes `jdBits` and
 * leaves the two V.92 pointers alone, and the V.92 path writes both V.92
 * pointers and leaves `jdBits` alone.  Only the V.92 path has an else -- a
 * null `V90Jd*` gives `jdBits` a null through the same store, while a null
 * `V92Jd*` takes a separate arm that nulls both.
 *
 * The three levels are the same companding the DIL generator does: the code
 * carries a seven-bit magnitude and the sign/company bits are supplied here,
 * `(code & 0x7f) ^ 0xd5` for A-law and `(code & 0x7f) ^ 0xff` for mu-law.
 * `codeLevelAlt` is the same for the code sixteen higher, added as a byte
 * -- which the mask makes moot, but it is what the object does -- and
 * `idleLevel` is the level of the code that is all sign bits, 0xd5 or 0xff.
 *
 * The law is read back out of `pcmType` for the second and third conversions
 * rather than from the argument, which is why the object tests it three
 * times for what is one decision.
 *
 * The trailing loop is the caller's warm-up: `nSymbols` symbols generated
 * immediately, through whichever generator `sessionFlag` selects, and the
 * flag is re-read on every iteration.
 */
void
V90Phase3Modulator::reset(PcmType law, unsigned char code,
			  Phase3ModulatorState st, unsigned int nSymbols,
			  V90Jd *jd, V92Jd *jd92,
			  const tagV90DILdescriptor *d, unsigned int base)
{
	unsigned int i;

	scrambler.reset(0);

	pcmType = law;
	symbolCount = 0;
	eventCode = 0;
	timeoutBase = base;
	state = st;

	if (law != PCM_TYPE_MU_LAW)
		codeLevel = (short)alaw2linear(
		    (unsigned char)((code & 0x7f) ^ 0xd5));
	else
		codeLevel = (short)ulaw2linear(
		    (unsigned char)((code & 0x7f) ^ 0xff));

	code = (unsigned char)(code + 0x10);

	if (pcmType != PCM_TYPE_MU_LAW)
		codeLevelAlt = (short)alaw2linear(
		    (unsigned char)((code & 0x7f) ^ 0xd5));
	else
		codeLevelAlt = (short)ulaw2linear(
		    (unsigned char)((code & 0x7f) ^ 0xff));

	if (pcmType != PCM_TYPE_MU_LAW)
		idleLevel = (short)alaw2linear(0xd5);
	else
		idleLevel = (short)ulaw2linear(0xff);

	polarity = 0;

	if (sessionFlag == 0) {
		jdBits = jd != NULL ? jd->getBitVector() : NULL;
	} else if (jd92 != NULL) {
		jdV92Bits = jd92->getJdBitVector();
		jdV92PhaseBits = jd92->getJdPhaseBitVector();
	} else {
		jdV92Bits = NULL;
		jdV92PhaseBits = NULL;
	}

	resetDILGenerator(d);

	for (i = nSymbols; i != 0; i--) {
		if (sessionFlag != 0)
			generateV92Symbol();
		else
			generateV90Symbol();
	}
}

/*
 * ===========================================================================
 * V90Phase3Modulator::V90Phase3Modulator (.text+0x2c4a0, 123 bytes)
 *
 * Three statements, no branch, and the whole of the disassembly is:
 *
 *     lea    0x20(%ebx),%edx                  <- the scrambler subobject
 *     Scrambler<unsigned char,int>::Scrambler(0x12, 0x17, 0x63)
 *     mov    0x34(%esp),%eax ; mov %eax,0x50(%ebx)     params
 *     mov    0x38(%esp),%ecx ; mov %ecx,(%ebx)         sessionFlag
 *     reset(0, 0x40, 0, 0, NULL, NULL, NULL, 0)
 *
 * THE MEMBER-INITIALISER IS NOT A CHOICE.  `Scrambler` has no default
 * constructor, so `scrambler(18, 23, 99)` is the only legal spelling and it
 * happens to give the blob's ordering -- subobject first, body second -- for
 * free.  (18, 23) are V.90's scrambler taps, the same pair
 * `V90Phase3Demodulator` gives its descrambler; 99 is the distance the
 * initial output position sits above the buffer's floor, so the buffer is
 * 1 + 23 + 99 = 123 bytes.
 *
 * THE STORE ORDER IS BEHAVIOUR.  `sessionFlag` must be written before `reset`
 * runs, because `reset` reads it to choose between writing `jdBits` and
 * writing the two V.92 vectors.  It is asserted by t_v90p3mod's constructor
 * block and by a mutation.
 *
 * `0x40` is the argument `reset` companded into `codeLevel`, and the two
 * enum arguments are both zero -- mu-law and the Sd state.  The trailing
 * count is 0, so `reset`'s warm-up loop does not run; the descriptor is NULL,
 * so `resetDILGenerator` clears `dilCount` and returns without touching one
 * of the 800-odd DIL bytes, which is why the whole DIL half of the object is
 * still whatever the allocation left there.
 * ===========================================================================
 */
V90Phase3Modulator::V90Phase3Modulator(V90Parameters *p, unsigned int flag)
	: scrambler(18, 23, 99)
{
	params = p;
	sessionFlag = flag;
	reset(PCM_TYPE_MU_LAW, 0x40, P3M_STATE_SD, 0, NULL, NULL, NULL, 0);
}

/*
 * .text+0x2ac30, 22 bytes -- and 22 bytes is not an empty function.  The body
 * is one call, `Scrambler<unsigned char, int>::~Scrambler` on `this + 0x20`,
 * with no test of anything first: that is exactly what GCC emits for a
 * destructor whose own body is empty over a class with one member that has a
 * non-trivial destructor.  So the source is an empty body, and what frees the
 * scrambler's buffer is the implicit member destruction the compiler appends.
 * Nothing else here owns memory.
 */
V90Phase3Modulator::~V90Phase3Modulator()
{
}

/*
 * THE DISPATCHER: one test of `sessionFlag` and a tail call to whichever of
 * the two symbol generators the session is running.  It is the entry point
 * the modulator's caller uses, and it is why `sessionFlag` is at +0x00 --
 * the field a dispatcher tests first is the one that costs no displacement
 * byte to reach.
 *
 * The object sign-extends the callee's value again before returning it; the
 * header says why that instruction is not in our build and why it cannot
 * change the number.
 */
int
V90Phase3Modulator::generateSymbol()
{
	if (sessionFlag != 0)
		return generateV92Symbol();

	return generateV90Symbol();
}

/*
 * LEAVE Jd -- .text+0x2ad80, 88 bytes -- and LEAVE JdPhase, +0x2ade0, 70.
 * The two exits `V90Modulator::exitJd` and `::exitJdPhase` call, and the same
 * shape as `exitDIL` above: guard on the state, guard on nothing having been
 * sent yet, and then a boundary test that chooses between running the current
 * repetition out and handing straight on.
 *
 * THE BOUNDARY IS THE 72-SYMBOL REPETITION, which is `generateV90Symbol`'s
 * and `generateV92Symbol`'s own -- both already spell it `symbolCount % 72u
 * == 0`, and the object divides by 72 the same way here (0x38e38e39, `mul`,
 * `shr $4`, then times 9 times 8).  Off a boundary the state goes to the
 * matching "_END" state, which keeps emitting the same thing until the count
 * reaches one; on a boundary the repetition is already whole and the next
 * state starts at once.
 *
 * `exitJd` FORKS ON `sessionFlag` AND `exitJdPhase` DOES NOT, because
 * JdPhase is a V.92 state and has only one successor:
 *
 *              off a boundary            on a boundary
 *   exitJd     V.90 -> JD_END (7)        V.90 -> JD_NOT (8)
 *              V.92 -> V92JD_END (4)     V.92 -> JD_PHASE (5)
 *   exitJdPhase       JD_PHASE_END (6)          JD_NOT (8)
 *
 * so under V.90 the boundary case skips JD_END entirely and starts JdNot, and
 * under V.92 it skips V92JD_END and starts JdPhase.  The object's
 * `cmp $0x1 ; sbb ; and $0x3 ; add $imm` is GCC's if-conversion of the
 * conditional, not something the source spells.
 *
 * THE EMITTED STORE ORDER DIFFERS FROM THE SOURCE ORDER. `exitJd` writes
 * the count first (0x2adad) and the state second (0x2adc1), but GCC 3.4.2
 * reproduces all 88 bytes only when the state assignment precedes the count
 * clear in source. The 24-cell guard/branch/store-order domain in F10212
 * confirms this. `exitJdPhase` writes the state first in both representations.
 *
 * NEITHER TOUCHES `eventCode`.  `exitDIL` is the only one of the four exits
 * that raises an event, and it raises it only when phase 3 is over.
 */
void
V90Phase3Modulator::exitJd()
{
	if (state != P3M_STATE_JD)
		return;
	if (symbolCount == 0)
		return;

	if (symbolCount % 72u == 0) {
		state = sessionFlag ? P3M_STATE_JD_PHASE : P3M_STATE_JD_NOT;
		symbolCount = 0;
	} else {
		state = sessionFlag ? P3M_STATE_V92JD_END : P3M_STATE_JD_END;
	}
}

void
V90Phase3Modulator::exitJdPhase()
{
	if (state != P3M_STATE_JD_PHASE)
		return;
	if (symbolCount == 0)
		return;

	if (symbolCount % 72u == 0) {
		state = P3M_STATE_JD_NOT;
		symbolCount = 0;
	} else {
		state = P3M_STATE_JD_PHASE_END;
	}
}

/*
 * LEAVE THE DIL STATE, and only from the DIL state: three guards before
 * anything is written, in this order, and the object tests them one at a
 * time rather than as a conjunction.
 *
 *   state must be P3M_STATE_DIL      -- 9, `cmpl $0x9,0x14`
 *   symbolCount must be non-zero     -- nothing has been sent yet otherwise
 *   segmentPos decides which exit    -- mid-segment goes to DIL_END, which
 *                                       runs the segment out; on a segment
 *                                       boundary the sequence is over
 *
 * THE TERMINATION ARM IS THE ONE `generateV90Symbol` ALREADY CONTAINS, and
 * identically: the same message with the same symbol count, then
 * TERMINATED, then the count cleared and event 6 raised.  Both are in the
 * object, in full, at 0x2ae6b and 0x2b79b; the two are not a shared helper
 * and this reconstruction does not make them one.
 *
 * `eventCode` is written ONLY on that arm.  The DIL_END arm changes the
 * state and leaves the event alone, so a caller polling the event sees
 * nothing until the sequence really ends.
 */
void
V90Phase3Modulator::exitDIL()
{
	if (state != P3M_STATE_DIL)
		return;
	if (symbolCount == 0)
		return;

	if (segmentPos != 0) {
		state = P3M_STATE_DIL_END;
		return;
	}

	edprintf("V90Phase3Modulator: Phase3 Terminated @ %d\r\n",
		 (int)symbolCount);
	state = P3M_STATE_TERMINATED;
	symbolCount = 0;
	eventCode = 6;
}
