/*
 * V92Modulator.cpp -- construction and destruction of the V.92 upstream
 * modulator, the object that owns the whole transmit graph.
 *
 * Reconstructed from dsplibs.o.  Four symbols, 2,474 bytes:
 *
 *     V92Modulator::V92Modulator(unsigned, V92Phase2Info *, V92Ja *,
 *         tagV90DILdescriptor *, V92CP *, V92MappingParams *,
 *         V92Parameters *)          .text+0x15120 (C1), +0x15400 (C2)
 *     V92Modulator::~V92Modulator() .text+0x14050 (D2), +0x14250 (D1)
 *
 * Each pair is byte-identical bar register allocation; GCC emits both from
 * one definition.  `include/dsplib/V92Modulator.h` carries the object map,
 * the 0x90 the allocation gives, the eleven owned pieces and the two words
 * nobody writes.
 *
 * THE CONSTRUCTOR ENDS BY INLINING `reset()`, and that is measured rather
 * than assumed: `V92Modulator::reset` is its own 186-byte symbol at
 * .text+0x15060, and the constructor's last 152 bytes are its body statement
 * for statement -- the same `Scrambler<int,unsigned char>::reset(this+0x54,
 * 0)`, the same five clears, the same `Queue<float>::reset`, the same
 * `params->MODULATOR_QUEUE_LENGTH >> 1` and priming loop, and the same
 * closing `FloatFIR::reset` -- down to the "V92Modulator reset" the two both
 * print.  `reset` itself is NOT written here: it belongs to whoever takes the
 * other sixteen members, and this file carries the body as a file-static
 * helper so that the duplication is visible rather than hidden.  That is the
 * spelling V92Phase3Modulator.cpp uses for the six generators it inlines.
 *
 * WHY THE SUB-OBJECTS ARE BUILT THROUGH asm() LABELS: the reason
 * src/pump/v90/V92Precoder.cpp gives in full -- `sysdep_malloc(n); ctor(p)`
 * with no null test between them is `new` over an inline `operator new`, this
 * build is -nostdinc++ with no <new>, and a placement form would add the null
 * test the blob does not have.
 *
 * AND WHY ONE OF THE SIX IS `delete` AFTER ALL.  `ResamplerTimingOffset` has
 * a virtual destructor and inherits `Resampler`'s member `operator delete`,
 * so the blob releases it with `mov (%edx),%eax; call *0x4(%eax)` -- vtable
 * slot one, the deleting destructor, and no `sysdep_free` of its own.  The
 * only C++ that emits that is `delete p`, and it costs no libstdc++ because
 * the `operator delete` it reaches is the member one.  The other five have no
 * `operator delete` of their own, so `delete` on them would reference the
 * global form and break every test binary's link.
 *
 * Plain cdecl, `this` first on the stack -- `mov 0x50(%esp),%esi` after four
 * pushes and a 0x3c-byte frame -- finding 215.
 */

#include <stddef.h>

extern "C" {
#include "dsplib/debug.h"
#include "dsplib/vpcm_tables.h"
}

#include "dsplib/V92Modulator.h"
#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92Parameters.h"
#include "dsplib/V92Phase3Modulator.h"
#include "dsplib/V92Phase4Modulator.h"
#include "dsplib/ResamplerTimingOffset.h"
#include "dsplib/Queue.h"
#include "dsplib/FloatFIR.h"

extern "C" {
void *sysdep_malloc(unsigned int size);
void sysdep_free(void *mem);

/* The six complete-object constructors, by the names the relocations carry. */
void v92mod_bts_ctor(void *self, unsigned int n, void *params)
	asm("_ZN15V92BitsToSymbolC1EjP13V92Parameters");
void v92mod_rto_ctor(void *self, unsigned int phases, float ppmScale,
		     unsigned int taps, float cutoff, float ppm,
		     unsigned int minHistory)
	asm("_ZN21ResamplerTimingOffsetC1Ejfjffj");
void v92mod_p3m_ctor(void *self, void *params)
	asm("_ZN18V92Phase3ModulatorC1EP13V92Parameters");
void v92mod_p4m_ctor(void *self, void *params, void *bitsToSymbol, void *cp,
		     void *mappingParams)
	asm("_ZN18V92Phase4ModulatorC1EP13V92ParametersP15V92BitsToSymbolP5V92C"
	    "PP16V92MappingParams");
void v92mod_queue_ctor(void *self, unsigned int n) asm("_ZN5QueueIfEC1Ej");
void v92mod_fir_ctor(void *self, unsigned int nTaps, float *coef,
		     unsigned int blockSize) asm("_ZN8FloatFIRC1EjPfj");
}

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V92MOD_OFF(field, off, tag) \
	typedef char v92mod_off_##tag[ \
	    ((int)__builtin_offsetof(V92Modulator, field) == (off)) ? 1 : -1]

V92MOD_OFF(blockSize,		0x00, blocksize);
V92MOD_OFF(queuePrime,		0x04, queueprime);
V92MOD_OFF(blockRemaining,	0x08, blockremaining);
V92MOD_OFF(byte_0c,		0x0c, byte0c);
V92MOD_OFF(byte_0d,		0x0d, byte0d);
V92MOD_OFF(phase2Info,		0x10, phase2info);
V92MOD_OFF(ja,			0x14, ja);
V92MOD_OFF(dil,			0x18, dil);
V92MOD_OFF(mappingParams,	0x1c, mappingparams);
V92MOD_OFF(cp,			0x20, cp);
V92MOD_OFF(pad_24,		0x24, pad24);
V92MOD_OFF(float_28,		0x28, float28);
V92MOD_OFF(word_2c,		0x2c, word2c);
V92MOD_OFF(word_30,		0x30, word30);
V92MOD_OFF(word_34,		0x34, word34);
V92MOD_OFF(word_38,		0x38, word38);
V92MOD_OFF(pad_3c,		0x3c, pad3c);
V92MOD_OFF(params,		0x40, params);
V92MOD_OFF(phase3Modulator,	0x44, phase3);
V92MOD_OFF(phase4Modulator,	0x48, phase4);
V92MOD_OFF(bitsToSymbol,	0x4c, bitstosymbol);
V92MOD_OFF(resampler,		0x50, resampler);
V92MOD_OFF(scrambler,		0x54, scrambler);
V92MOD_OFF(queue,		0x74, queue);
V92MOD_OFF(txFilter,		0x78, txfilter);
V92MOD_OFF(buf_7c,		0x7c, buf7c);
V92MOD_OFF(buf_80,		0x80, buf80);
V92MOD_OFF(buf_84,		0x84, buf84);
V92MOD_OFF(buf_88,		0x88, buf88);
V92MOD_OFF(buf_8c,		0x8c, buf8c);

typedef char v92mod_size[(sizeof(V92Modulator) == 0x90) ? 1 : -1];

/*
 * The six `sizeof`s the constructor allocates with are the six classes' own,
 * every one already pinned by its own header from its own allocation site.
 * Asserting them here is what makes each `sysdep_malloc(sizeof(X))` below a
 * transcription of the blob's literal rather than a hope.
 */
typedef char v92mod_bts_size[(sizeof(V92BitsToSymbol) == 0x20) ? 1 : -1];
typedef char v92mod_rto_size[
	(sizeof(ResamplerTimingOffset) == 0x4c) ? 1 : -1];
typedef char v92mod_p3m_size[(sizeof(V92Phase3Modulator) == 0x50) ? 1 : -1];
typedef char v92mod_p4m_size[(sizeof(V92Phase4Modulator) == 0x1cc) ? 1 : -1];
typedef char v92mod_queue_size[(sizeof(Queue<float>) == 0x14) ? 1 : -1];
typedef char v92mod_fir_size[(sizeof(FloatFIR) == 0x14) ? 1 : -1];
typedef char v92mod_scram_size[
	(sizeof(Scrambler<int, unsigned char>) == 0x20) ? 1 : -1];

#endif /* 32-bit */

/*
 * ===========================================================================
 * The body of `V92Modulator::reset` (.text+0x15060), which the constructor
 * inlines and this file therefore carries as a helper.
 *
 * The whole thing, with nothing elided and in the object's order:
 *
 *     if (dsplibs_debug_level > 1) printf("V92Modulator reset\r\n")
 *     scrambler.reset(0)
 *     +0x2c = 0 ; +0x30 = 0 ; +0x08 = +0x00 ; +0x34 = 0 ; +0x38 = 0
 *     queue->reset()
 *     +0x0c = 0 ; +0x0d = 0                     (both BYTE stores)
 *     +0x28 = 0.0f
 *     +0x04 = params->MODULATOR_QUEUE_LENGTH >> 1
 *     for (i = 0; i < +0x04; i++) queue->write(0.0f)
 *     txFilter->reset()
 *
 * THE SHIFT IS ARITHMETIC AND THE LOOP'S COMPARISON IS UNSIGNED.  `sar $1`
 * with no sign fixup is what GCC emits for `>> 1` over a SIGNED int, and the
 * parameter really is one -- `MODULATOR_QUEUE_LENGTH` is `int` in
 * V92Parameters.h, from a block of 54 four-byte slots.  The loop that follows
 * closes with `cmp %ebx,0x4(%esi); ja`, which is UNSIGNED, so the field it
 * lands in is unsigned and a negative parameter would prime the queue about
 * two billion times.  Finding 1284; the fixture keeps the parameter small and
 * positive for exactly that reason.
 *
 * The bound and the queue pointer are both re-read from the object on every
 * iteration, which is what the object does and what a member access through
 * `this` compiles to when the call between them might alias.
 * ===========================================================================
 */
static void
v92mod_reset(V92Modulator *m)
{
	unsigned int i;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator reset\r\n");

	m->scrambler.reset(0);
	m->word_2c = 0;
	m->word_30 = 0;
	m->blockRemaining = m->blockSize;
	m->word_34 = 0;
	m->word_38 = 0;
	m->queue->reset();
	m->byte_0c = 0;
	m->byte_0d = 0;
	m->float_28 = 0.0f;
	m->queuePrime = (unsigned int)(m->params->MODULATOR_QUEUE_LENGTH >> 1);

	for (i = 0; i < m->queuePrime; i++)
		m->queue->write(0.0f);

	m->txFilter->reset();
}

/*
 * ===========================================================================
 * V92Modulator::V92Modulator (.text+0x15120 / +0x15400, 734 bytes)
 *
 * The scrambler in the member-initialiser list, then a debug line, then the
 * seven arguments filed away, then eleven allocations, then the inlined
 * `reset`.
 *
 * `blockSize` IS COMPUTED IN FLOATING POINT AND TRUNCATED.  The object does
 * `fildll` over a zero-extended 64-bit push of the argument -- the unsigned
 * to float conversion -- then `fmuls 0x3f555555`, `fadds 0.5f`, and a
 * `fistpll` with the x87 control word forced to round-toward-zero whose low
 * half is kept.  That last pair is GCC's float-to-UNSIGNED sequence, so both
 * ends of the conversion are unsigned and neither is a guess.  There is no
 * intermediate store, so the multiply and the add happen in the x87's
 * extended precision and the `f` suffixes on the two constants control only
 * which values are loaded.
 *
 * `3 * blockSize` for the bit-to-symbol stage is `lea (%eax,%eax,2)` over the
 * value RE-READ from +0x00, and the two buffers at +0x84 and +0x8c are sized
 * from the ARGUMENT rather than from `blockSize`.  Getting those two the same
 * way round is the difference between a graph that is right and one that is
 * plausible.
 *
 * NOT ONE OF THE ELEVEN ALLOCATIONS IS CHECKED, the blob's included.
 * ===========================================================================
 */
V92Modulator::V92Modulator(unsigned int nSamples, V92Phase2Info *p2,
			   V92Ja *j, tagV90DILdescriptor *d, V92CP *c,
			   V92MappingParams *mp, V92Parameters *pp)
	: scrambler(V92MOD_SCRAM_TAP1, V92MOD_SCRAM_TAP2, V92MOD_SCRAM_SLACK)
{
	void *p;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator constraction\r\n");

	phase2Info = p2;
	blockSize = (unsigned int)(nSamples
				   * (V92MOD_RATE_NUM / V92MOD_RATE_DEN)
				   + 0.5f);
	ja = j;
	cp = c;
	dil = d;
	params = pp;
	mappingParams = mp;

	buf_7c = (short *)sysdep_malloc((blockSize + V92MOD_BUF_SLACK)
					* sizeof(short));
	buf_80 = (int *)sysdep_malloc((blockSize + V92MOD_BUF_SLACK)
				      * sizeof(int));
	buf_88 = sysdep_malloc(blockSize * 8);
	buf_84 = (float *)sysdep_malloc((nSamples + V92MOD_BUF_SLACK)
					* sizeof(float));
	buf_8c = (float *)sysdep_malloc((nSamples + V92MOD_BUF_SLACK)
					* sizeof(float));

	p = sysdep_malloc(sizeof(V92BitsToSymbol));
	v92mod_bts_ctor(p, 3 * blockSize, params);
	bitsToSymbol = (V92BitsToSymbol *)p;

	p = sysdep_malloc(sizeof(ResamplerTimingOffset));
	v92mod_rto_ctor(p, V92MOD_RS_PHASES, V92MOD_RS_PPMSCALE,
			V92MOD_RS_TAPS, V92MOD_RS_CUTOFF, V92MOD_RS_PPM,
			V92MOD_RS_MINHISTORY);
	resampler = (ResamplerTimingOffset *)p;

	p = sysdep_malloc(sizeof(V92Phase3Modulator));
	v92mod_p3m_ctor(p, params);
	phase3Modulator = (V92Phase3Modulator *)p;

	p = sysdep_malloc(sizeof(V92Phase4Modulator));
	v92mod_p4m_ctor(p, params, bitsToSymbol, cp, mappingParams);
	phase4Modulator = (V92Phase4Modulator *)p;

	p = sysdep_malloc(sizeof(Queue<float>));
	v92mod_queue_ctor(p, (unsigned int)params->MODULATOR_QUEUE_LENGTH);
	queue = (Queue<float> *)p;

	p = sysdep_malloc(sizeof(FloatFIR));
	v92mod_fir_ctor(p, V92_TXPREFILTER_TAPS, v92TxPreFilter,
			V92MOD_FIR_BLOCK);
	txFilter = (FloatFIR *)p;

	v92mod_reset(this);
}

/*
 * ===========================================================================
 * V92Modulator::~V92Modulator (.text+0x14050 / +0x14250, 503 bytes)
 *
 * Eleven null-guarded releases and then the scrambler, which is the
 * compiler's implicit member destruction and not a statement -- finding
 * 1256's reading, and the same one V92Phase4Modulator's destructor gets.
 *
 * THE SIX OBJECTS COME BACK IN A DIFFERENT ORDER FROM THE ONE THEY WERE BUILT
 * IN, AND THE FIVE RAW BUFFERS DO NOT.  The constructor allocates the buffers
 * +0x7c, +0x80, +0x88, +0x84, +0x8c and the destructor frees them +0x7c,
 * +0x80, +0x88, +0x84, +0x8c -- the same, +0x88 before +0x84 in both.  The
 * objects are built bit-to-symbol, resampler, phase 3, phase 4, queue, filter
 * and released phase 3, phase 4, bit-to-symbol, resampler, queue, filter.
 * Reproduced as found; nothing in the test tier sequences a free, so this is
 * the disassembly's word and not a measurement.
 *
 * NOTHING IS NULLED after its free, so a second destruction double-frees all
 * eleven; see docs/deviations.md D212.
 * ===========================================================================
 */
V92Modulator::~V92Modulator()
{
	if (phase3Modulator != 0) {
		phase3Modulator->~V92Phase3Modulator();
		sysdep_free(phase3Modulator);
	}
	if (phase4Modulator != 0) {
		phase4Modulator->~V92Phase4Modulator();
		sysdep_free(phase4Modulator);
	}
	if (bitsToSymbol != 0) {
		bitsToSymbol->~V92BitsToSymbol();
		sysdep_free(bitsToSymbol);
	}
	if (resampler != 0)
		delete resampler;
	if (queue != 0) {
		queue->~Queue();
		sysdep_free(queue);
	}
	if (txFilter != 0) {
		txFilter->~FloatFIR();
		sysdep_free(txFilter);
	}
	if (buf_7c != 0)
		sysdep_free(buf_7c);
	if (buf_80 != 0)
		sysdep_free(buf_80);
	if (buf_88 != 0)
		sysdep_free(buf_88);
	if (buf_84 != 0)
		sysdep_free(buf_84);
	if (buf_8c != 0)
		sysdep_free(buf_8c);
}
