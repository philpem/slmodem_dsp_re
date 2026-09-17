/*
 * V90Mapper.cpp -- V90Mapper's constructor, destructor and two resets.
 *
 * Reconstructed from dsplibs.o.  `include/dsplib/V90Mapper.h` carries the
 * object map and the argument for the 0x704 size; this file is the four
 * functions and the assertions that hold the compiler to that map.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x10(%esp),%ebx` after one push and a 8-byte frame -- not
 * %ecx, so these are not thiscall and nothing here needs an attribute
 * (finding F215).
 *
 * THE TWO SUBOBJECTS ARE BUILT BY THE MEM-INITIALIZER LIST AND NOT BY HAND.
 * `ModulusEncoder` and `V90SpectralShaper` both have a user-declared default
 * constructor, so declaring them as members is what emits
 *
 *     lea 0x670(%ebx),%edx ; call ModulusEncoder::C1
 *     lea 0x68c(%ebx),%eax ; call V90SpectralShaper::C1
 *
 * in declaration order, before the body, which is the blob's order.  The
 * destructor's single `V90SpectralShaper::~V90SpectralShaper` call is emitted
 * the same way, after the body, because `ModulusEncoder` has no destructor.
 *
 * THE SIX-WORD CLEAR AT +0x658 IS A LOOP IN THE BLOB, NOT SIX STORES.  It is
 * `cmp $0x5,%eax ; jbe` over an unsigned counter incremented after the store,
 * so it runs for 0..5 inclusive and covers 0x658..0x66f, which is exactly the
 * gap up to the modulus encoder.  Written as six unrolled stores the
 * behaviour is identical and the code is not, so it is written as the loop.
 *
 * ---------------------------------------------------------------------------
 * THE TWO RESETS ARE ONE FUNCTION WITH TWO ENDS, and reading them side by side
 * is how the shared part was settled.  `.text+0x30070` (`reset`, 517 bytes)
 * and `.text+0x30280` (`resetNoSpectral`, 404 bytes) both run
 *
 *     bitsPerFrame = mp->word_0
 *     word_08      = bitsPerFrame - signBitsPerFrame
 *     the six-constellation fill
 *     the seven-word modulusEncoder fill
 *     signEncoder.prev_ = 0
 *     word_700 = 0
 *
 * byte for byte the same, and differ only in what surrounds it: `reset`
 * COMPUTES `signBitsPerFrame` (and `signBitGroups`, `signBitGroupSize`,
 * `bitsBuffered`) where `resetNoSpectral` reads the first and leaves the rest
 * alone, and the two call different members of the spectral shaper.  That
 * "+0x0c is read and never written here" is exactly the split
 * `V90Demapper::resetNoSpectral` has against `V90Demapper::reset`, and it is
 * only visible over storage that was never zeroed -- findings F223 and F224, and
 * `t_v90modchain.cpp` pokes a value that is not any `6 - shaperSR` into it
 * before the call for that reason.
 *
 * THE ORDER OF THE STORES IS NOT THE OBJECT'S AND CANNOT BE.  GCC schedules
 * this block heavily -- in `reset` the `word_700` store lands between two
 * halves of the modulusEncoder fill -- so what is reproduced is the set of
 * stores and their values, which is what the differential test compares.
 * Finding F617's full-text acceptance test is not claimed for either function.
 */

#include <stddef.h>

/*
 * `pcm.h` is a C header with no linkage block of its own, so it is wrapped
 * here exactly as `V90AutoDigitalImpDetector.cpp` wraps it -- without this the
 * two conversions are looked up under their mangled C++ names and nothing in
 * `src/service/pcm.c` matches.
 */
extern "C" {
#include "dsplib/pcm.h"
}

#include "dsplib/sysdep.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Mapper.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but only parses `struct name {` out of include/dsplib, so
 * a C++ class has to assert its own -- and it is exactly the check that
 * catches an object right in size and wrong by four in every offset.
 */
#if __SIZEOF_POINTER__ == 4
#define V90MAPPER_OFF(field, off, tag) \
	typedef char v90mapper_off_##tag[ \
	    ((int)__builtin_offsetof(V90Mapper, field) == (off)) ? 1 : -1]

V90MAPPER_OFF(params,		0x000, params);
V90MAPPER_OFF(bitsPerFrame,	0x004, c004);
V90MAPPER_OFF(word_08,		0x008, c008);
V90MAPPER_OFF(signBitsPerFrame,	0x00c, c00c);
V90MAPPER_OFF(signBitGroups,	0x010, c010);
V90MAPPER_OFF(signBitGroupSize,	0x014, c014);
V90MAPPER_OFF(buf,		0x018, buf);
V90MAPPER_OFF(bitsBuffered,	0x01c, c01c);
V90MAPPER_OFF(codes,		0x020, codes);
V90MAPPER_OFF(levels,		0x038, levels);
V90MAPPER_OFF(signs,		0x044, signs);
V90MAPPER_OFF(samples,		0x04a, samples);
V90MAPPER_OFF(constellation,	0x056, cons);
V90MAPPER_OFF(constellationSize,0x658, c658);
V90MAPPER_OFF(modulusEncoder,	0x670, modenc);
V90MAPPER_OFF(spectralShaper,	0x68c, shaper);
V90MAPPER_OFF(primeFrames,		0x6f8, c6f8);
V90MAPPER_OFF(signEncoder,	0x6fc, c6fc);
V90MAPPER_OFF(word_700,		0x700, c700);
typedef char v90mapper_size[(sizeof(V90Mapper) == 0x704) ? 1 : -1];
#endif

/*
 * THE ASSERTIONS ABOVE ARE NOT WHAT PROTECTS THE NEW OFFSETS, and saying so
 * is the point of this note.  `__SIZEOF_POINTER__` is a GCC 4.6+ predefine,
 * so under the period compiler the guard reads `#if 0` and every one of them
 * vanishes while the file still compiles -- CLAUDE.md's silent variance, and
 * `docs/method/compilers.md` is its register.  What actually holds
 * `constellation`'s base and stride is `t_v90modchain`'s byte-for-byte
 * comparison of the whole 0x704 against the blob's: the tail-fill writes
 * every row out to index 127 on every call, so a base or a stride that is
 * wrong by one element differs somewhere in 1,536 bytes and is reported.
 */

/*
 * ===========================================================================
 * V90Mapper::V90Mapper -- .text+0x2ff50 (C1) and +0x2ffe0 (C2), 138 bytes.
 *
 * The 0x50-byte allocation is NOT null-checked, exactly as in the blob: the
 * pointer is stored and the first user of it stores through whatever came
 * back.  Every other store is a constant.
 * ===========================================================================
 */
V90Mapper::V90Mapper(V90Parameters *p)
{
	unsigned int i;

	signEncoder.prev_ = 0;
	buf = sysdep_malloc(0x50);
	bitsBuffered = 0;
	primeFrames = 0;
	signBitGroupSize = 0;
	signBitGroups = 0;
	signBitsPerFrame = 0;
	word_08 = 0;
	bitsPerFrame = 0;
	for (i = 0; i <= 5; i++)
		constellationSize[i] = 0;
	params = p;
}

/*
 * ===========================================================================
 * V90Mapper::~V90Mapper -- .text+0x2fed0 (D2) and +0x2ff10 (D1), 61 bytes.
 *
 * `buf` is NOT nulled after the free, so a second destruction double-frees;
 * that is the blob's behaviour and is left alone.  The spectral shaper's
 * destructor runs after this body whichever way the branch went, which is
 * why the blob has two copies of the call.
 * ===========================================================================
 */
V90Mapper::~V90Mapper()
{
	if (buf)
		sysdep_free(buf);
}

/*
 * ===========================================================================
 * The part both resets share: build the six constellations out of the
 * companded code tables, then hand the six sizes and `word_08` to the
 * embedded ModulusEncoder and clear the sign encoder.
 *
 * It is written out TWICE below rather than factored into a helper, because
 * the blob has no helper: both functions carry their own copy of these loops
 * and of the seven stores, and a static helper here would put bytes against
 * neither side of `compare.py`'s per-function count (finding F605).
 * ===========================================================================
 */

/*
 * ===========================================================================
 * V90Mapper::resetNoSpectral -- .text+0x30280, 404 bytes.
 *
 * `this` at 0x30(%esp), `mp` at 0x34, `pcm` at 0x38; plain cdecl as
 * everywhere here (finding F215).
 *
 * THE TWO CODE CONVERSIONS ARE G.711 WRITTEN OUT LONGHAND, and both are in
 * the object as arithmetic rather than as a table:
 *
 *     30300:  24 7f     and  $0x7f,%al     mu-law: complement the low seven
 *     30302:  f6 d0     not  %al
 *     303f0:  24 7f     and  $0x7f,%al     A-law:  toggle them against 0xd5
 *     303f2:  34 d5     xor  $0xd5,%al
 *
 * The `not` is on the BYTE and the result is then zero-extended
 * (`movzbl %al,%ecx`), so the argument that reaches `ulaw2linear` is
 * `(unsigned char)~(b & 0x7f)` and not `~(b & 0x7f)`, which would be
 * 0xffffff80 or above.  `V90Phase3Demodulator.h` records the same pair the
 * other way round, as `(ucode & 0x7f) ^ 0xff`, which is the same eight bits.
 *
 * THE ENTRY COUNT IS RE-READ FROM THE OBJECT ON EVERY ITERATION -- the loop
 * guard is `mov 0x658(%ebp,%edi,4),%eax ; cmp %esi,%eax ; ja` at 0x30314,
 * INSIDE the loop and after the call -- because `ulaw2linear` is an external
 * function that may alias `this`.  So the member is read in the condition and
 * not hoisted into a local; the demapper's `reset` caches its copy and its
 * `resetNoSpectral` does not, and here neither does.
 *
 * THE TAIL-FILL RUNS TO 127 UNCONDITIONALLY, so a short constellation leaves
 * the rest of its row ZEROED rather than stale.  That is invisible to a test
 * over zeroed storage and is one of the things `t_v90modchain.cpp` seeds
 * against.
 * ===========================================================================
 */
void
V90Mapper::reset(V90MappingParams *mp, PcmType pcm)
{
	unsigned int i, j;

	bitsPerFrame = mp->word_0;
	signBitGroups = mp->shaperSR;

	if (mp->shaperSR != 0)
		signBitGroupSize = V90MAPPER_FRAME / mp->shaperSR;
	else
		signBitGroupSize = 0;

	signBitsPerFrame = V90MAPPER_FRAME - mp->shaperSR;
	word_08 = bitsPerFrame - signBitsPerFrame;

	for (i = 0; i < V90MAPPER_CONSTELLATIONS; i++) {
		constellationSize[i] = mp->constellationSize[i];

		for (j = 0; j < constellationSize[i]; j++) {
			unsigned char b = mp->constellation[i][j];

			if (pcm)
				constellation[i][j] = (short)alaw2linear(
				    (unsigned char)((b & 0x7f) ^ 0xd5));
			else
				constellation[i][j] = (short)ulaw2linear(
				    (unsigned char)~(b & 0x7f));
		}

		for (j = constellationSize[i]; j < V90MAPPER_LEVELS; j++)
			constellation[i][j] = 0;
	}

	if (signBitGroups != 0) {
		spectralShaper.reset(mp->shaperId, (unsigned int)mp->shaperSR,
				     mp->shaperA1, mp->shaperA2,
				     mp->shaperB1, mp->shaperB2);
		primeFrames = mp->shaperId;
	} else {
		primeFrames = 0;
	}

	modulusEncoder.field_00 = constellationSize[0];
	modulusEncoder.field_04 = constellationSize[1];
	modulusEncoder.field_08 = constellationSize[2];
	modulusEncoder.field_0c = constellationSize[3];
	modulusEncoder.field_10 = constellationSize[4];
	modulusEncoder.field_14 = constellationSize[5];
	modulusEncoder.field_18 = word_08;
	signEncoder.prev_ = 0;

	word_700 = 0;
	bitsBuffered = 0;
}

void
V90Mapper::resetNoSpectral(V90MappingParams *mp, PcmType pcm)
{
	unsigned int i, j;

	bitsPerFrame = mp->word_0;
	word_08 = bitsPerFrame - signBitsPerFrame;

	for (i = 0; i < V90MAPPER_CONSTELLATIONS; i++) {
		constellationSize[i] = mp->constellationSize[i];

		for (j = 0; j < constellationSize[i]; j++) {
			unsigned char b = mp->constellation[i][j];

			if (pcm)
				constellation[i][j] = (short)alaw2linear(
				    (unsigned char)((b & 0x7f) ^ 0xd5));
			else
				constellation[i][j] = (short)ulaw2linear(
				    (unsigned char)~(b & 0x7f));
		}

		for (j = constellationSize[i]; j < V90MAPPER_LEVELS; j++)
			constellation[i][j] = 0;
	}

	modulusEncoder.field_00 = constellationSize[0];
	modulusEncoder.field_04 = constellationSize[1];
	modulusEncoder.field_08 = constellationSize[2];
	modulusEncoder.field_0c = constellationSize[3];
	modulusEncoder.field_10 = constellationSize[4];
	modulusEncoder.field_14 = constellationSize[5];
	modulusEncoder.field_18 = word_08;
	signEncoder.prev_ = 0;

	spectralShaper.resetSSFilter(mp->shaperA1, mp->shaperA2,
				     mp->shaperB1, mp->shaperB2);

	word_700 = 0;
}

/*
 * ===========================================================================
 * V90Mapper::reset -- .text+0x30070, 517 bytes.
 *
 * `this` at 0x40(%esp), `mp` at 0x44, `pcm` at 0x48.
 *
 * `signBitGroupSize` IS STORED ZERO WHEN THERE IS NO SHAPER, which is where
 * this function and `V90Demapper::reset` part company.  The demapper wraps
 * only the division in its `if` and leaves the field stale; here the zero arm
 * is an explicit `movl $0x0,0x14(%ebp)` at 0x30206, jumping back into the
 * common path.  Both are reproduced as the object has them.
 *
 * THE DIVISION IS UNSIGNED -- `f7 f1  div %ecx` at 0x3009c and not `idiv`,
 * with no signed fixup anywhere near it -- so the numerator is `6u` and not
 * `6`.  `V90MappingParams::shaperSR` is declared `int` and stays so for the
 * reason that header gives at length; what forces the instruction is
 * `V90MAPPER_FRAME`'s type on the left.  This is a codegen difference the
 * differential tier cannot see: the two spellings agree over every divisor a
 * caller can produce and separate only for a negative one.
 *
 * `signBitsPerFrame` IS COMPUTED BEFORE `word_08` READS IT.  The object does
 * `mov $0x6,%edx ; sub %ecx,%edx ; mov %edx,0xc(%ebp) ; sub %edx,%ebx ; mov
 * %ebx,0x8(%ebp)` at 0x300a1..0x300b1 -- one value, stored and then used --
 * which is what makes `resetNoSpectral`'s bare read of the same field the
 * same statement with the definition removed.
 *
 * THE SHAPER'S ARGUMENT ORDER IS `(shaperId, shaperSR, ...)` AND THE OBJECT
 * SAYS SO, which is worth writing down because the two are adjacent words of
 * the same block and swapping them is silent whenever they are equal:
 *
 *     30244:  mov 0x620(%edx),%edi ; mov %edi,0x8(%esp)   <- 2nd param, sr
 *     3024e:  mov 0x624(%edx),%esi ; mov %esi,0x4(%esp)   <- 1st param, id
 *
 * and `V90SpectralShaper::reset(unsigned id, unsigned sr, ...)` is the
 * mangling's `Ejjffff` with the shaper's own body agreeing.
 * ===========================================================================
 */
/*
 * ===========================================================================
 * V90Mapper::process -- .text+0x30420, 530 bytes
 *
 * ONE V.90 FRAME AT A TIME, OUT OF A BIT STREAM THAT NEED NOT BE A WHOLE
 * NUMBER OF THEM.  Each input byte is one payload bit; they go into `buf` at
 * `bitsBuffered` and nothing else happens until `bitsPerFrame` of them are
 * there.  Then the frame is built, `bitsPerFrame` is taken back OFF
 * `bitsBuffered` rather than the count being cleared, and whatever arrived
 * after the frame boundary stays buffered for the next call.  The blob's
 * epilogue does not set `%eax`, so nothing is returned; `nofSymbols` is the
 * only answer.
 *
 * `buf` IS `void *` AND THAT IS WHAT THE RELOADS ARE.  The store at 0x30466
 * goes through it as an `unsigned char`, which may alias anything, so
 * `bitsBuffered` is re-read at 0x30469 and `buf` itself at 0x3045f on every
 * pass.  A `unsigned char *` member would not have needed either.
 *
 * THE FRAME IS THREE STAGES AND THE MIDDLE ONE HAS TWO ARMS:
 *
 *   - `ModulusEncoder::progress` turns the `word_08` bits above the sign bits
 *     into six mixed-radix digits, and each digit picks a level out of its own
 *     constellation: `levels[k] = constellation[k][codes[k]]`, which the
 *     object addresses as one flat `(k << 7) + codes[k]` with no bound on the
 *     digit at all.
 *   - With `signBitGroups` nonzero the sign bits go through the spectral
 *     shaper, `signBitGroups` calls of `signBitGroupSize` samples each.  The
 *     bit pointer advances by `signBitGroupSize - 1` per group and the sample
 *     pointers by `signBitGroupSize`, because position 0 of every shaper frame
 *     is the trellis's and not a payload bit (see V90SpectralShaper.h).  All
 *     three members are RE-READ inside the loop, at 0x304d0, 0x304de and
 *     0x304f2, which is what a member as a loop expression compiles to across
 *     a call that could change it.
 *   - With `signBitGroups` zero there is no shaper: the six bits go through
 *     the serial differential encoder one at a time and a ZERO NEGATES, which
 *     is the same polarity `V90SpectralShaper::process` uses for its own sign
 *     bits.  `signs` is written and never read back by this class.
 *
 * THE TAIL IS THE PRIMING COUNTDOWN AND IT HAS THREE ARMS, not two.  +0x6f8
 * starts at `mp->shaperId` and the shaper does not emit anything real until it
 * has been fed that many frames, so:
 *
 *      +0x6f8 == 0                 all six samples go out
 *      +0x6f8 <  signBitGroups     the last `6 - +0x6f8 * signBitGroupSize`
 *                                  of them do, and the countdown ends
 *      otherwise                   none do, and +0x6f8 loses signBitGroups
 *
 * -- which totals `shaperId * signBitGroupSize` suppressed samples over the
 * whole countdown.  Where `shaperSR` divides six, and it does for every value
 * V.90 uses, that is exactly `V90BitsToSymbol::reset`'s `extraSymbols`,
 * `6 * shaperId / shaperSR`, computed by a different function in a different
 * class out of the same two parameters -- so the three arms are confirmed from
 * outside themselves.  The two part company for a `shaperSR` that does not
 * divide six, and finding F7422 carries the algebra.
 *
 * `nofOut += V90MAPPER_FRAME - start` IS OUTSIDE THE LOOP AND UNCONDITIONAL,
 * and the object is what says so: the guard at 0x305e7 skips the copy when
 * `start` exceeds 5 and lands on 0x30611, which does the update anyway.  With
 * `start` counted up inside the loop instead, a skipped loop would leave
 * `nofOut` alone.  No state `reset` can produce reaches that -- `start` is at
 * most `6 - signBitGroupSize` on this arm -- so the fixture pokes the three
 * fields by hand to drive it.  Findings F7423 and F3120.
 * ===========================================================================
 */
void
V90Mapper::process(unsigned char *bits, unsigned int nofBits, short *symbols,
		   unsigned int &nofSymbols)
{
	unsigned int i;
	unsigned int nofOut = 0;

	for (i = 0; i < nofBits; i++) {
		unsigned int k;

		unsigned char *out = (unsigned char *)buf;
		out[bitsBuffered] = bits[i];
		bitsBuffered++;
		if (bitsBuffered >= bitsPerFrame) {
			modulusEncoder.progress((unsigned char *)buf
						    + signBitsPerFrame, codes);

			for (k = 0; k < V90MAPPER_FRAME; k++)
				levels[k] = constellation[k][codes[k]];

			if (signBitGroups != 0) {
				for (k = 0; k < signBitGroups; k++)
					spectralShaper.process(
					    &levels[k * signBitGroupSize],
					    (unsigned char *)buf
						+ k * (signBitGroupSize - 1),
					    &samples[k * signBitGroupSize]);
			} else {
				for (k = 0; k < V90MAPPER_FRAME; k++) {
					signs[k] = signEncoder.process(
					    ((unsigned char *)buf)[k]);
					samples[k] = signs[k] ? levels[k]
							      : (short)-levels[k];
				}
			}

			if (primeFrames != 0) {
				if (primeFrames >= signBitGroups) {
					primeFrames -= signBitGroups;
				} else {
					unsigned int start = primeFrames * signBitGroupSize;

					for (k = start; k < V90MAPPER_FRAME; k++)
						symbols[nofOut + k - start] = samples[k];
					nofOut += V90MAPPER_FRAME
					    - primeFrames * signBitGroupSize;
					primeFrames = 0;
				}
			} else {
				for (k = 0; k < V90MAPPER_FRAME; k++)
					symbols[nofOut + k] = samples[k];
				nofOut += V90MAPPER_FRAME;
			}

			bitsBuffered -= bitsPerFrame;
		}
	}

	nofSymbols = nofOut;
}
