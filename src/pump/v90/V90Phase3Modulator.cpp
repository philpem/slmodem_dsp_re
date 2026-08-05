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
P3M_OFF(word_08,	0x008, word08);
P3M_OFF(codeLevel,	0x00c, codelevel);
P3M_OFF(codeLevelAlt,	0x00e, codelevelalt);
P3M_OFF(idleLevel,	0x012, idlelevel);
P3M_OFF(state,		0x014, state);
P3M_OFF(symbolCount,	0x018, symbolcount);
P3M_OFF(word_1c,	0x01c, word1c);
P3M_OFF(scrambler,	0x020, scrambler);
P3M_OFF(word_40,	0x040, word40);
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
P3M_OFF(byte_388,	0x388, byte388);
P3M_OFF(byte_389,	0x389, byte389);
P3M_OFF(byte_38a,	0x38a, byte38a);
P3M_OFF(word_38c,	0x38c, word38c);
P3M_OFF(segmentIndex,	0x390, segmentindex);
P3M_OFF(short_392,	0x392, short392);
P3M_OFF(byte_394,	0x394, byte394);
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

	byte_388 = 0;
	byte_389 = 0;
	byte_38a = 0;
	word_38c = 0;

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
