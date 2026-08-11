/*
 * V92Modulator.h -- the V.92 upstream modulator: the object that owns the
 * whole transmit graph.
 *
 * Reconstructed from dsplibs.o.  The class has eighteen symbols; TWO of them
 * are written in src/pump/v90/V92Modulator.cpp -- the constructor (C1 at
 * .text+0x15120 and C2 at +0x15400, 734 bytes each) and the destructor (D2 at
 * +0x14050 and D1 at +0x14250, 503 bytes each).  The other sixteen are
 * declared here with the signatures the mangling gives and deliberately left
 * undefined; `progress` alone is 1,075 bytes and none of them has been read.
 *
 * SO THE MAP BELOW IS THE CONSTRUCTOR'S, THE DESTRUCTOR'S AND `reset`'s, and
 * it happens to be almost the whole object: only +0x24 and +0x3c are unnamed,
 * and they are unnamed because NOTHING WRITTEN HERE WRITES THEM.  That is a
 * claim rather than a gap -- see the note below.
 *
 * THE OBJECT IS 0x90 BYTES AND THAT IS THE ALLOCATION.  `V92Modem::V92Modem`
 * builds it:
 *
 *     13e6a:  c7 04 24 90 00 00 00   movl $0x90,(%esp)
 *     13e71:  e8 ..                  call sysdep_malloc
 *     13eb0:  e8 ..                  call V92Modulator::V92Modulator
 *
 * the original compiler's own `sizeof` (finding 1249's oracle).  The furthest
 * field is the four bytes at +0x8c and 0x8c + 4 == 0x90 exactly.
 *
 * ---------------------------------------------------------------------------
 * TWO WORDS ARE LEFT UNINITIALISED, AND THAT IS THE CONSTRUCTOR'S
 *
 * +0x24 and +0x3c are written by neither the constructor nor the `reset` it
 * inlines, so a freshly built V92Modulator carries whatever `sysdep_malloc`
 * left in them until some member not written here fills them.  Finding 1240's
 * shape in a third class, and t_v92mod.cpp holds it: both sides are seeded
 * with the same bytes and never zeroed, so the two words compare equal
 * BECAUSE nobody wrote them, and a reconstruction that helpfully cleared
 * either would fail.
 *
 * ---------------------------------------------------------------------------
 * THE SCRAMBLER IS A MEMBER AT +0x54 AND ITS INTERMEDIATE TYPE IS THE THIRD
 * ONE IN THIS FAMILY
 *
 * `Scrambler<int, unsigned char>::Scrambler(this + 0x54, 5, 0x17, 0x63)` opens
 * the constructor, `::reset(this + 0x54, 0)` opens the inlined `reset`, and
 * `::~Scrambler(this + 0x54)` closes the destructor unconditionally -- the D1
 * variant, which is a member's and not a non-virtual base's.  0x20 bytes, so
 * +0x54..+0x73, and the next field at +0x74 meets it exactly.
 *
 * The taps (5, 23) and the slack 99 are the same three numbers
 * `V92Phase3Modulator` and `V92Phase4Modulator` use.  The three
 * instantiations differ only in their type arguments -- `<i,h>` here,
 * `<h,i>` there, `<h,h>` in phase 4 -- and every one of those is the
 * mangling's rather than a choice.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE OBJECT OWNS: eleven allocations, and six of them are objects
 *
 *     +0x44  0x50    V92Phase3Modulator(params)
 *     +0x48  0x1cc   V92Phase4Modulator(params, bitsToSymbol, cp, mapParams)
 *     +0x4c  0x20    V92BitsToSymbol(3 * blockSize, params)
 *     +0x50  0x4c    ResamplerTimingOffset(120, 100.0f, 16, 0.98f, 0.0f, 0)
 *     +0x74  0x14    Queue<float>(params->MODULATOR_QUEUE_LENGTH)
 *     +0x78  0x14    FloatFIR(36, v92TxPreFilter, 99)
 *     +0x7c  (blockSize + 10) * 2
 *     +0x80  (blockSize + 10) * 4
 *     +0x88  blockSize * 8
 *     +0x84  (nSamples + 10) * 4
 *     +0x8c  (nSamples + 10) * 4
 *
 * `nSamples` is the constructor's first argument and `blockSize` is derived
 * from it; the two are NOT interchangeable and the last three sizes are what
 * says so.  The destructor releases all eleven, each behind a null test, in
 * the order +0x44, +0x48, +0x4c, +0x50, +0x74, +0x78, +0x7c, +0x80, +0x88,
 * +0x84, +0x8c.  **The five raw buffers come back in exactly the order they
 * were taken in, and the six OBJECTS do not**: the constructor builds them
 * +0x4c, +0x50, +0x44, +0x48, +0x74, +0x78 and the destructor releases them
 * +0x44, +0x48, +0x4c, +0x50, +0x74, +0x78.  The transmitter has the same
 * asymmetry one level down (finding 1281), and both are reproduced as found.
 *
 * THE RESAMPLER IS RELEASED THROUGH ITS VTABLE and the other five objects are
 * not.  `mov (%edx),%eax; call *0x4(%eax)` with no `sysdep_free` after it is
 * `delete p` over a class with a virtual destructor and a member `operator
 * delete`; the other five are `if (p) { p->~T(); sysdep_free(p); }`, which is
 * the same expression over a class that has neither.  Both spellings are in
 * the .cpp for that reason and the difference is the object's.
 *
 * Data member names are invented and descriptive (finding 226).
 */

#ifndef DSPLIB_V92MODULATOR_H
#define DSPLIB_V92MODULATOR_H

#include "dsplib/Scrambler.h"

class FloatFIR;
class ResamplerTimingOffset;
class V92BitsToSymbol;
class V92CP;
class V92Ja;
class V92MappingParams;
class V92Parameters;
class V92Phase2Info;
class V92Phase3Modulator;
class V92Phase4Modulator;
struct tagV90DILdescriptor;
template <class T> class Queue;

/*
 * The scrambler's three arguments: the near tap, the far tap and the slack
 * below the restart point.  `mov $0x5`, `mov $0x17`, `mov $0x63` at
 * .text+0x15121..+0x1512c.
 */
#define V92MOD_SCRAM_TAP1	5
#define V92MOD_SCRAM_TAP2	23
#define V92MOD_SCRAM_SLACK	99

/*
 * `blockSize = (unsigned)(nSamples * 0.83333331f + 0.5f)`.  The multiplier is
 * `.rodata.cst4+0xc8` = 0x3f555555, which is five sixths rounded to a float,
 * and the addend is `+0xcc` = 0.5f.  Both are loaded with `fmuls`/`fadds`, so
 * both are SINGLE precision and the `f` suffixes are load-bearing; the
 * conversion back is `fistpll` with the control word forced to truncate and
 * only the low half kept, which is what GCC emits for float -> unsigned.
 */
#define V92MOD_RATE_NUM		5.0f
#define V92MOD_RATE_DEN		6.0f

/* ResamplerTimingOffset(120, 100.0f, 16, 0.98f, 0.0f, 0): six literals, from
 * .text+0x1525f..+0x1528d.  0x42c80000 is 100.0f and 0x3f7ae148 is 0.98f. */
#define V92MOD_RS_PHASES	120
#define V92MOD_RS_PPMSCALE	100.0f
#define V92MOD_RS_TAPS		16
#define V92MOD_RS_CUTOFF	0.98f
#define V92MOD_RS_PPM		0.0f
#define V92MOD_RS_MINHISTORY	0

/* FloatFIR(36, v92TxPreFilter, 99) -- the tap count is the table's own, which
 * include/dsplib/vpcm_tables.h carries, and 99 is the block slack. */
#define V92MOD_FIR_BLOCK	99

/* The slack every derived buffer but one carries: `add $0xa` and
 * `lea 0x28(,%ebx,4)` are the same ten elements spelled two ways. */
#define V92MOD_BUF_SLACK	10

class V92Modulator {
public:
	V92Modulator(unsigned int nSamples, V92Phase2Info *phase2Info,
		     V92Ja *ja, tagV90DILdescriptor *dil, V92CP *cp,
		     V92MappingParams *mappingParams, V92Parameters *params);
	~V92Modulator();

	/*
	 * Declared, not defined.  Argument types are the mangling's and
	 * exact; return types are not mangled, and `void` here means "not
	 * established" rather than "measured" -- except `getV92TxFilterDelay`,
	 * which is 21 bytes ending in a value in %eax.
	 */
	void reset();
	void enterPhase3();
	void enterPhase4();
	void enterDataPhase();
	void exitJa();
	void exitSilence();
	void exitSuSecond();
	void exitTRN1uSecond();
	void exitCPt();
	void initiateRRN();
	void initiateFPE();
	void mkResampledSignal(unsigned int &n);
	void progress(int *a, unsigned int &b, float *c, unsigned int d);
	int getV92TxFilterDelay() const;

	/* Public for `offsetof`; one access section, as everywhere here. */

	/*
	 * +0x00  `(unsigned)(nSamples * 5/6 + 0.5f)`.  Read back out of the
	 * object three times by the constructor itself to size buffers, and
	 * copied to +0x08 by `reset`.
	 */
	unsigned int blockSize;

	/*
	 * +0x04  `params->MODULATOR_QUEUE_LENGTH >> 1`, and the bound of the
	 * loop that primes the queue.  The SHIFT is arithmetic (`sar $1`) and
	 * the loop's comparison is unsigned (`ja`) -- the field is unsigned
	 * and the parameter it comes from is signed, which is finding 1284.
	 */
	unsigned int queuePrime;

	/* +0x08  Set to `blockSize` by `reset`; a running copy of it. */
	unsigned int blockRemaining;

	/* +0x0c  Cleared by `reset`.  One byte. */
	unsigned char byte_0c;

	/* +0x0d  Cleared by `reset`.  One byte. */
	unsigned char byte_0d;

	/* +0x0e  Two bytes of alignment; nothing writes them. */
	unsigned char pad_0e[2];

	/* +0x10  The constructor's SECOND argument, stored and not owned. */
	V92Phase2Info *phase2Info;

	/* +0x14  The constructor's THIRD argument, stored and not owned. */
	V92Ja *ja;

	/* +0x18  The constructor's FOURTH argument, stored and not owned. */
	tagV90DILdescriptor *dil;

	/* +0x1c  The constructor's SIXTH argument; handed on to the phase 4
	 * modulator and not owned. */
	V92MappingParams *mappingParams;

	/* +0x20  The constructor's FIFTH argument; handed on to the phase 4
	 * modulator and not owned. */
	V92CP *cp;

	/*
	 * +0x24  NOT WRITTEN by the constructor or by `reset`.  See the note
	 * at the top of this file: the hole is the claim.
	 */
	unsigned char pad_24[4];

	/* +0x28  Cleared by `reset` -- a 32-bit zero, and the same zero is
	 * passed to `Queue<float>::write`, so it is a float. */
	float float_28;

	/* +0x2c  Cleared by `reset`. */
	unsigned int word_2c;

	/* +0x30  Cleared by `reset`. */
	unsigned int word_30;

	/* +0x34  Cleared by `reset`. */
	unsigned int word_34;

	/* +0x38  Cleared by `reset`. */
	unsigned int word_38;

	/* +0x3c  NOT WRITTEN.  The second of the two holes. */
	unsigned char pad_3c[4];

	/* +0x40  The constructor's SEVENTH argument.  Read back by the
	 * constructor itself for the phase 3 and phase 4 modulators and for
	 * the queue's length, and by `reset` for the queue again. */
	V92Parameters *params;

	/* +0x44  Owned: `V92Phase3Modulator(params)`. */
	V92Phase3Modulator *phase3Modulator;

	/* +0x48  Owned: `V92Phase4Modulator(params, bitsToSymbol, cp,
	 * mappingParams)`. */
	V92Phase4Modulator *phase4Modulator;

	/* +0x4c  Owned: `V92BitsToSymbol(3 * blockSize, params)`. */
	V92BitsToSymbol *bitsToSymbol;

	/* +0x50  Owned, and the only one released through a vtable. */
	ResamplerTimingOffset *resampler;

	/* +0x54  The upstream scrambler, built (5, 23, 99).  A member. */
	Scrambler<int, unsigned char> scrambler;

	/* +0x74  Owned: `Queue<float>(params->MODULATOR_QUEUE_LENGTH)`. */
	Queue<float> *queue;

	/* +0x78  Owned: `FloatFIR(36, v92TxPreFilter, 99)`. */
	FloatFIR *txFilter;

	/*
	 * +0x7c  `(blockSize + 10) * 2` bytes.  The element width is the
	 * multiplier -- `lea 0x14(%ebp,%ebp,1)`, which is `(n + 10) * 2` --
	 * and `short` is the narrowest type that gives it.
	 */
	short *buf_7c;

	/* +0x80  `(blockSize + 10) * 4` bytes: `add $0xa; shl $2`. */
	int *buf_80;

	/*
	 * +0x84  `(nSamples + 10) * 4` bytes, from the CONSTRUCTOR ARGUMENT
	 * and not from `blockSize`: `lea 0x28(,%ebx,4)` with %ebx holding the
	 * argument, computed once and used for this and for +0x8c.
	 */
	float *buf_84;

	/*
	 * +0x88  `blockSize * 8` bytes: `shl $0x3` and no slack.  The element
	 * width is NOT established -- eight bytes each, or two four-byte ones
	 * per block element, are the same instruction -- so this stays a
	 * `void *` and the .cpp allocates the byte count the object computes.
	 */
	void *buf_88;

	/* +0x8c  `(nSamples + 10) * 4` bytes, the same expression as +0x84
	 * and the same register; the last four bytes of the object. */
	float *buf_8c;
};

#endif /* DSPLIB_V92MODULATOR_H */
