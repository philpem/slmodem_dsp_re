/*
 * V90Phase3Modulator.cpp -- the V.90 / V.92 phase 3 downstream symbol source.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90Phase3Modulator.h` holds
 * the object map and the reasoning behind it.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x4(%esp),%eax` -- not %ecx, so these are not thiscall and
 * nothing here needs an attribute (finding 215).
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
 * compiles the result as C, so a C++ class asserts its own (finding 230).
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
 * docs/v90cpp.md gives: defining a method whose callers are not written yet
 * re-opens the link closure for the whole test suite, and only the five
 * methods of batch 2 may be defined.  Nothing outside this file needs them.
 * ===========================================================================
 */

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
 * `generateDIL`, inlined: one symbol of the digital impairment learning
 * sequence, and the four cursors it steps.
 *
 * `seq2` chooses the level -- a zero there means this symbol carries the
 * segment's own boundary level rather than the DIL entry's -- and `seq1`
 * chooses its sign.  The PCM CODE stored in `dilPcmCode` is the DIL entry's
 * either way; it does not follow the level.  Both index bytes wrap at their
 * sequence's length rather than at 256, and a length of zero therefore never
 * wraps them, which is the object's behaviour and is preserved.
 *
 * Reaching `segmentLength[segmentIndex]` restarts the segment: all three
 * cursors go to zero, `dilIndex` advances modulo `dilCount`, and the segment
 * index is recomputed from the new level.  `segmentIndex` can be 8 -- one
 * past the end of both eight-element arrays -- which is what the object's own
 * search produces when no boundary matches; the reads that follow land inside
 * the object either way and are reproduced rather than corrected.
 */
static short
dilSymbol(V90Phase3Modulator *m)
{
	unsigned char i1 = m->seq1Index;
	unsigned char i2 = m->seq2Index;
	int fromSegment = (m->seq2[i2] == 0);
	unsigned short code = (unsigned short)m->dilLevel[m->dilIndex];
	unsigned char next1, next2;
	unsigned int pos;
	short level;

	level = fromSegment ? m->segmentLevel[m->segmentIndex]
			    : m->dilLevel[m->dilIndex];
	m->usingSegmentLevel = (short)fromSegment;

	if (m->pcmType != PCM_TYPE_MU_LAW)
		m->dilPcmCode = (unsigned char)(linear2alaw((int)code) ^ 0xd5);
	else
		m->dilPcmCode = (unsigned char)~linear2ulaw((int)code);

	next2 = (unsigned char)(i2 + 1);
	if (next2 == m->seq2Length)
		next2 = 0;

	if (m->seq1[i1] == 0)
		level = (short)-level;

	next1 = (unsigned char)(i1 + 1);
	if (next1 == m->seq1Length)
		next1 = 0;

	pos = m->segmentPos + 1u;
	if (pos == m->segmentLength[m->segmentIndex]) {
		m->segmentPos = 0;
		m->seq2Index = 0;
		m->seq1Index = 0;
		m->dilIndex = (unsigned char)(m->dilIndex + 1);
		if (m->dilIndex == m->dilCount)
			m->dilIndex = 0;
		updateCodeSegment(m);
	} else {
		m->seq1Index = next1;
		m->segmentPos = pos;
		m->seq2Index = next2;
	}

	return level;
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
					    "V90Phase3Modulator: ERROR: Null "
					    "JdBits @ end of TRN1d\r\n");
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
				edprintf("V90Phase3Modulator: ERROR: Null DIL "
					 "@ end of JdNOT\r\n");
			}
			symbolCount = 0;
		}
		break;

	case P3M_STATE_DIL:
		eventCode = 0;
		sample = dilSymbol(this);
		if (symbolCount == timeoutBase + 40000u) {
			state = P3M_STATE_DIL_TIMEOUT;
			symbolCount = 0;
			edprintf("V90Phase3Modulator: DIL TimeOut\r\n");
		}
		break;

	case P3M_STATE_DIL_END:
		eventCode = 0;
		sample = dilSymbol(this);
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
					    "V90Phase3Modulator: ERROR: Null "
					    "v92JdBits @ end of TRN1d\r\n");
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
				edprintf("V90Phase3Modulator: ERROR: Null DIL "
					 "@ end of JdNOT\r\n");
			}
			symbolCount = 0;
		}
		break;

	case P3M_STATE_DIL:
		eventCode = 0;
		sample = dilSymbol(this);
		if (symbolCount == timeoutBase + 40000u) {
			state = P3M_STATE_DIL_TIMEOUT;
			symbolCount = 0;
			edprintf("V90Phase3Modulator: DIL TimeOut\r\n");
		}
		break;

	case P3M_STATE_DIL_END:
		eventCode = 0;
		sample = dilSymbol(this);
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
