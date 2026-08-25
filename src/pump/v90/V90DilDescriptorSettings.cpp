/*
 * V90DilDescriptorSettings.cpp -- `setDilDescriptor` and `calculateDilLength`.
 *
 * THE FILE WAS `V90Dil.cpp`, an invented name, and the blob's own is
 * `V90DilDescriptorSettings.cpp`: `STT_FILE` #244, corroborated by the two
 * `edprintf` messages `setDilDescriptor` ends with, which begin
 * `"V90DilDescriptorSettings: "` where every neighbouring diagnostic names a
 * class and a member.  See the header for the whole argument, and finding
 * F7600.
 *
 * Free functions with mangled names, which the linker treats exactly like
 * members: `_Z18calculateDilLengthP19tagV90DILdescriptor7PcmType` and
 * `_Z16setDilDescriptorP19tagV90DILdescriptor7DilType`.  Nothing here is a
 * method, so there is no `this` and no calling-convention question; every
 * argument is an ordinary stack argument.
 *
 * THE SEGMENT BOUNDARIES ARE A LOCAL ARRAY, NOT THE MODULATOR'S STATIC ONE.
 * `V90Phase3Modulator::codeSegmentsBoundriesLookupTable` is a defined data
 * symbol at .data:0x440 holding LINEAR levels -- 124, 380, 892, ... -- and
 * `resetDILGenerator` indexes it.  This function does not touch it.  It
 * copies sixteen ints out of .rodata:0xb80 onto its own stack frame with
 * `rep movsl` once per DIL entry, and those sixteen are CODE boundaries:
 * 0x0f, 0x1f, 0x2f ... 0x7f and 0x1f, 0x2f ... 0x7f, 0. An anonymous constant
 * pool entry rather than a symbol, so it closes no link requirement of its
 * own; being re-copied inside the loop is what says it is a local declared in
 * the loop body rather than a file-scope or `static` one.
 *
 * The row is chosen by `8 * pcmType`, and the row LENGTH by `7 + (pcmType !=
 * 1)`: eight boundaries under mu-law, seven under A-law.  That is why the
 * A-law row's eighth entry is a zero that is never read.
 */

#include <stddef.h>

#include "dsplib/V90DilDescriptorSettings.h"

#include "dsplib/debug.h"
#include "dsplib/encode.h"

/*
 * ===========================================================================
 * THE EIGHT TABLES, AND SEVEN OF THE EIGHT NAMES ARE THE RECOMMENDATION'S
 * ===========================================================================
 *
 * `N`, `SP`, `TP`, `H`, `REF`, `Lsp` and `Ltp` are Recommendation V.90's own
 * notation for the DIL descriptor's fields, not the author's invention:
 * Table 12/V.90 in §8.3.1 names the bit fields `N` (18:25), `LSP - 1`
 * (35:41), `LTP - 1` (43:49), `SP` (52:67), `TP` (52+alpha:67+alpha), `H1`
 * through `H8` and `REF1` through `REF8`, and §8.4.1 defines every one of
 * them again in prose.  `TO` is NOT the spec's -- §8.3.1 spells that field
 * out as "The Ucode of the training symbol used for the 1st DIL segment" and
 * §8.4.1 as "A set of N Ucodes", with no symbol at all -- so `TO` is the
 * author's own name for the ucode set.  Finding F7601.
 *
 * WHAT IS IN THEM IS NOT THE SPEC'S.  The Recommendation gives the ranges and
 * the semantics and no values: 0 <= N <= 255, 1 <= LSP <= 128, 1 <= LTP <=
 * 128, seven bits each for Hc, REFc and every ucode, and Lc = (Hc + 1) * 6
 * symbols for the length of a DIL-segment.  Every number below satisfies all
 * of that and none of it is determined by it -- which DIL to request is the
 * analogue modem's choice, and these two rows are this vendor's.  So this is
 * NOT the situation docs/method/conformance-plan.md records for Table 1/V.90,
 * whose 512 published numbers our `ulaw2linear`/`alaw2linear` reproduce with
 * zero mismatches; there is no published table to check these against.
 *
 * THE ROW IS THE `DilType`, and the strides are forced rather than chosen:
 * `N`, `Lsp` and `Ltp` are indexed by `%ecx` itself, `SP` and `TP` by
 * `%ecx << 7`, `TO` by `%ecx << 8` and `H` and `REF` by `lea 0x0(,%ecx,8)`.
 * Two rows, and the symbol sizes agree exactly: 2, 2, 2, 256, 256, 16, 16,
 * 512 bytes.
 *
 * THEY ARE NOT `const`, AND THAT IS MEASURED.  All eight are `d` (LOCAL
 * OBJECT) in `.data`, section 143; a `static const unsigned char` array goes
 * to `.rodata` under GCC 3.4.2 at -O3, so the author wrote them without the
 * qualifier.  Nothing writes them.
 *
 * WHAT THE TWO ROWS DIFFER IN.  `TO`'s two rows are byte-for-byte identical,
 * so `DilType` does not change WHICH training symbols are sent, only how long
 * each segment is (`H`, 39 against 19 in the middle) and how long the two
 * patterns are (120 against 60).  `REF` is identical too.  ADI_QC is
 * therefore the same sweep at roughly half the dwell -- which is what a
 * quick-connect variant of an impairment-learning sequence should be, and
 * the one cross-check on the arm-to-enumerator mapping that does not depend
 * on reading the two `edprintf` strings.
 *
 * `N` is 144 in both rows, and 144 is exactly the number of non-zero entries
 * at the head of each `TO` row; `Lsp` and `Ltp` are 120 and 60, and each
 * `SP`/`TP` row is zero from that index on.  Three independent agreements
 * between a count and the table it counts.
 */
#define DIL_TYPES	2

static unsigned char N[DIL_TYPES] = { 144, 144 };
static unsigned char Lsp[DIL_TYPES] = { 120, 60 };
static unsigned char Ltp[DIL_TYPES] = { 120, 60 };

static unsigned char SP[DIL_TYPES][128] = {
	{
		  1,   0,   0,   0,   1,   1,   1,   1,   1,   0,   1,   0,   0,   0,   0,   0,
		  0,   0,   1,   0,   1,   1,   0,   0,   0,   1,   1,   1,   1,   1,   0,   1,
		  0,   1,   0,   1,   1,   0,   0,   1,   1,   0,   0,   0,   1,   0,   1,   0,
		  0,   1,   1,   0,   0,   1,   1,   0,   1,   1,   1,   0,   1,   1,   1,   0,
		  0,   1,   0,   1,   0,   0,   0,   1,   1,   0,   0,   1,   0,   0,   0,   0,
		  0,   0,   1,   1,   1,   1,   0,   1,   1,   1,   0,   1,   1,   1,   0,   0,
		  1,   0,   1,   0,   1,   1,   0,   1,   0,   1,   0,   0,   0,   0,   0,   1,
		  1,   0,   1,   1,   1,   0,   0,   1,   0,   0,   0,   0,   0,   0,   0,   0,
	},
	{
		  1,   1,   0,   1,   1,   1,   0,   0,   1,   0,   0,   0,   0,   0,   0,   1,
		  1,   1,   1,   0,   1,   0,   0,   1,   1,   1,   0,   1,   1,   0,   0,   1,
		  1,   0,   0,   0,   1,   0,   0,   1,   1,   1,   0,   1,   1,   0,   1,   0,
		  1,   1,   0,   1,   0,   1,   0,   0,   1,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	},
};

static unsigned char TP[DIL_TYPES][128] = {
	{
		  0,   1,   1,   0,   1,   0,   1,   1,   0,   1,   0,   1,   0,   0,   0,   0,
		  0,   0,   1,   1,   1,   1,   1,   1,   1,   0,   0,   0,   0,   0,   0,   1,
		  0,   1,   0,   1,   0,   0,   1,   0,   1,   0,   1,   0,   0,   0,   0,   0,
		  0,   1,   1,   1,   1,   1,   0,   1,   1,   0,   1,   1,   1,   1,   0,   0,
		  0,   0,   1,   0,   1,   0,   1,   1,   0,   1,   0,   1,   0,   0,   1,   0,
		  0,   1,   1,   0,   0,   0,   1,   1,   0,   1,   0,   1,   1,   0,   0,   1,
		  1,   0,   0,   1,   1,   0,   1,   0,   1,   0,   1,   1,   0,   1,   0,   1,
		  0,   0,   1,   0,   1,   1,   1,   1,   0,   0,   0,   0,   0,   0,   0,   0,
	},
	{
		  0,   1,   1,   0,   1,   0,   1,   1,   0,   1,   0,   1,   0,   0,   0,   1,
		  0,   0,   1,   1,   1,   1,   1,   1,   1,   0,   0,   0,   0,   0,   0,   1,
		  0,   1,   0,   1,   0,   0,   1,   0,   1,   0,   1,   0,   0,   0,   0,   0,
		  0,   1,   1,   1,   1,   1,   1,   0,   1,   0,   1,   1,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	},
};

static unsigned char H[DIL_TYPES][8] = {
	{  19,  39,  39,  39,  39,  39,  39,  19 },
	{   9,  19,  19,  19,  19,  19,  19,   9 },
};

static unsigned char REF[DIL_TYPES][8] = {
	{  78,  78,  78,  78,  78,  78,  78,  25 },
	{  78,  78,  78,  78,  78,  78,  78,  25 },
};

static unsigned char TO[DIL_TYPES][256] = {
	{
		 79,  78,  77,  76,  75,  74,  73,  72,  71,  70,  69,  68,  67,  66,  65,  64,
		 63,  61,  59,  57,  55,  53,  51,  49,  47,  45,  43,  41,  39,  37,  35,  33,
		 31,  29,  27,  25,  23,  21,  19,  17,  15,  13,  11,   9,   7,   5,   3,   2,
		  4,   6,   8,  10,  12,  14,  16,  18,  20,  22,  24,  26,  28,  30,  32,  34,
		 36,  38,  40,  42,  44,  46,  48,  50,  52,  54,  56,  58,  60,  62,  64,  65,
		 66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,
		 82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,
		 98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
		114, 115, 116, 100,  90,  80,  79,  78,  77,  76,  75,  74,  73,  72,  71,  70,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	},
	{
		 79,  78,  77,  76,  75,  74,  73,  72,  71,  70,  69,  68,  67,  66,  65,  64,
		 63,  61,  59,  57,  55,  53,  51,  49,  47,  45,  43,  41,  39,  37,  35,  33,
		 31,  29,  27,  25,  23,  21,  19,  17,  15,  13,  11,   9,   7,   5,   3,   2,
		  4,   6,   8,  10,  12,  14,  16,  18,  20,  22,  24,  26,  28,  30,  32,  34,
		 36,  38,  40,  42,  44,  46,  48,  50,  52,  54,  56,  58,  60,  62,  64,  65,
		 66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,
		 82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,
		 98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
		114, 115, 116, 100,  90,  80,  79,  78,  77,  76,  75,  74,  73,  72,  71,  70,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	},
};

/*
 * ===========================================================================
 * `setDilDescriptor` -- .text+0x31c60, 0x131 = 305 bytes
 * ===========================================================================
 *
 * Five copies and three scalars, and the interesting part is what it does NOT
 * write.  `seq1` is filled to `seq1Length`, `seq2` to `seq2Length` and
 * `dilCode` to `dilCount`; the rest of all three arrays keeps whatever it
 * held.  Only `segmentSize` and `segmentCode` are filled unconditionally, and
 * their loops are `cmp $0x7,%edx; jbe` -- a plain `i < 8` with no guard,
 * where the other three are `cmp %edx,%eax; ja` against a RELOADED count.
 *
 * THE THREE COUNTS ARE RE-READ ON EVERY ITERATION and the loops are written
 * that way deliberately.  `movzbl 0x8c2(%ecx),%eax` sits INSIDE the `seq1`
 * loop at 0x31cad, `Ltp` at 0x31ce0 and `N` at 0x31d10.  The store
 * `d->seq1[i] = ...` may alias the table -- both are `unsigned char` -- so
 * GCC cannot hoist the bound, and hoisting it into a local here would emit a
 * different loop.  Finding F2302 is the same shape the other way round.
 *
 * THE THREE ZERO-TRIP GUARDS ARE THE COMPILER'S, not an `if` in the source.
 * `cmpb $0x0,0x8c2(%ecx); je` before the `seq1` loop is what GCC 3.4.2 emits
 * for a `for` whose bound it must assume can be zero; the `H`/`REF` loops
 * have no such test because 8 is a literal.
 *
 * IT IS `void`.  The `DIL_TYPE_ADI` arm tail-JUMPS to `edprintf` (0x31d8c)
 * and the `DIL_TYPE_ADI_QC` arm CALLS it and returns (0x31d77), so the two
 * exits do not agree on `%eax` and no caller can be reading one -- the
 * argument that settled `V90Phase4Modulator::setMappingParams` (7431).
 * `V90Modem::reset`, the object's only caller, discards it.
 *
 * The switch has no `default`: a `DilType` outside {0, 1} falls off the end
 * silently -- `test %ecx,%ecx; je` then `dec %ecx; je` then `ret`, the same
 * three-way shape `V90Modem` uses for `side`.
 *
 * AND SUCH A VALUE IS NOT TESTABLE AND MUST NOT BE TESTED.  Every index above
 * is `type` scaled by the row width with no bound check, so a `type` of 2
 * reads PAST the end of all eight tables, and what lies past them is
 * `.data`'s own layout -- which is the blob's for the blob's copy and GCC's
 * choice for ours.  The two would legitimately differ, so a fixture that
 * drove an out-of-range `DilType` would be measuring section placement.  The
 * object's only constructor of a `DilType` is `V90Modem::reset`'s
 * `qcFlag ? 1 : 0`, so no in-object path can produce one.
 */
void
setDilDescriptor(tagV90DILdescriptor *d, DilType type)
{
	unsigned int i;

	d->dilCount = N[type];
	d->seq1Length = Lsp[type];
	d->seq2Length = Ltp[type];

	for (i = 0; i < Lsp[type]; i++)
		d->seq1[i] = SP[type][i];

	for (i = 0; i < Ltp[type]; i++)
		d->seq2[i] = TP[type][i];

	for (i = 0; i < N[type]; i++)
		d->dilCode[i] = TO[type][i];

	for (i = 0; i < 8; i++)
		d->segmentSize[i] = H[type][i];

	for (i = 0; i < 8; i++)
		d->segmentCode[i] = REF[type][i];

	switch (type) {
	case DIL_TYPE_ADI:
		edprintf("V90DilDescriptorSettings: DIL descriptor set to "
			 "option ADI.\r\n");
		break;

	case DIL_TYPE_ADI_QC:
		edprintf("V90DilDescriptorSettings: DIL descriptor set to "
			 "option ADI_QC.\r\n");
		break;
	}
}

/*
 * The object reads the length code one past `segmentSize` when the search
 * falls off the end, so the read is spelled through a byte pointer here: see
 * the comment on `seg` below.
 */
#define DIL_SEGMENT_SIZE_OFF \
	((unsigned int)__builtin_offsetof(tagV90DILdescriptor, segmentSize))

unsigned int
calculateDilLength(tagV90DILdescriptor *dil, PcmType pcmType)
{
	unsigned int length = 0;
	unsigned int count;
	unsigned int i;

	if (dil == 0)
		return length;

	count = dil->dilCount;
	if (count == 0)
		return length;

	for (i = 0; i < count; i++) {
		/*
		 * Re-initialised on every iteration, which is what the object
		 * does; see the file comment.
		 */
		int codeSegmentsBoundries[2][8] = {
			{ 0x0f, 0x1f, 0x2f, 0x3f, 0x4f, 0x5f, 0x6f, 0x7f },
			{ 0x1f, 0x2f, 0x3f, 0x4f, 0x5f, 0x6f, 0x7f, 0x00 }
		};
		unsigned int rowLength = pcmType == PCM_TYPE_A_LAW ? 7u : 8u;
		unsigned int code = dil->dilCode[i];
		unsigned int seg;

		for (seg = 0; seg < rowLength; seg++)
			if (codeSegmentsBoundries[pcmType][seg] >= (int)code)
				break;

		/*
		 * THE OBJECT READS ONE PAST `segmentSize` AND THIS REPRODUCES
		 * IT.  Under mu-law the search runs eight boundaries, the
		 * largest of which is 0x7f, so a DIL code of 0x80 or more
		 * matches none of them and leaves `seg` at 8 -- and the byte
		 * taken is then +0x103 + 8, which is `segmentCode[0]`, the
		 * member after the one indexed.  Under A-law the row is seven
		 * long, so `seg` stops at 7 and the read is in range.
		 *
		 * Spelled through a byte pointer rather than as
		 * `segmentSize[seg]` so that the out-of-range index is
		 * explicit and cannot be assumed away by a compiler that
		 * believes the declared bound.
		 */
		length += 6u * ((const unsigned char *)dil)[
		    DIL_SEGMENT_SIZE_OFF + seg] + 6u;
	}

	return length;
}
