/*
 * t_fuzzcov.cpp -- COVERAGE-GUIDED differential fuzz target (libFuzzer).
 *
 * This is the frontier-handoff section 5 candidate B brought to the point of
 * an actual fuzzer.  The blob `ref_*` aliases (the oracle) are linked in from
 * `build/dsplibs_ref.o`; OUR reconstruction is instrumented directly because
 * every target here is a header-instantiated template from `include/dsplib/`
 * compiled INTO this translation unit.  The fuzzer drives BOTH sides from the
 * SAME input bytes and treats ANY divergence as a bug: it aborts, and
 * libFuzzer records the failing input and the coverage that led to it.
 *
 * IT IS APPARATUS (`test/fuzz/`), LIKE `test/unit/t_fuzz.cpp`, SO THE
 * DIFFERENTIAL RULE DOES NOT APPLY -- but the denominator and firing rules do.
 * Every family increments a global comparison counter (`g_checks`) that is
 * printed at exit, and the negative control is a planted `src/`/header edit
 * (a `^ 1u` in the parallel coder) that the fuzzer MUST catch before a clean
 * run is believed.
 *
 * WHY COVERAGE-GUIDED AND NOT A FIXED-SEED LFSR.  `t_fuzz.cpp` explores
 * pseudo-random breadth but cannot ADAPT: it never learns which state reached
 * a deep branch.  libFuzzer gets edge/feature coverage on OUR instrumented
 * code and mutates the input deterministically toward unseen states, so the
 * STATE-DEPENDENT `process` methods -- the V.90/V.92 chain members -- are
 * genuinely explored across long construction+drain sequences rather than
 * sampled.  This is the first pass at driving those deeper states; the
 * families are the same near-exact/non-exact integer kernels `t_fuzz` built,
 * chosen because a divergence there is a real behavioural defect, not an x87
 * artefact (period-exact, integer-only, no `gccdiverge` entry).
 *
 * THE INPUT MODEL.  Byte 0 selects the family (0..NFAM-1); the remaining
 * bytes are that family's parameter/state/stream.  Each family is bounded in
 * work per input so ASan (slow, 32-bit) does not make exploration pointless.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dsplib/DiffCoder.h"
#include "dsplib/Scrambler.h"
#include "dsplib/V90SignBitsExtractor.h"
#include "dsplib/V92Mapper.h"
#include "dsplib/sysdep.h"

extern "C" {
/* Parallel encoder / decoder (near-exact BYTES; F11527). */
void ref_pe_ctor(void *s, unsigned n) asm("ref__ZN27ParallelDifferentialEncoderIhEC1Ej");
void ref_pe_dtor(void *s) asm("ref__ZN27ParallelDifferentialEncoderIhED1Ev");
int  ref_pe_reset(void *s, unsigned n, unsigned char init)
	asm("ref__ZN27ParallelDifferentialEncoderIhE5resetEjh");
void ref_pe_process(void *s, unsigned char *in, unsigned char *out)
	asm("ref__ZN27ParallelDifferentialEncoderIhE7processEPhS1_");

void ref_pd_ctor(void *s, unsigned n) asm("ref__ZN27ParallelDifferentialDecoderIhEC1Ej");
void ref_pd_dtor(void *s) asm("ref__ZN27ParallelDifferentialDecoderIhED1Ev");
int  ref_pd_reset(void *s, unsigned n, unsigned char init)
	asm("ref__ZN27ParallelDifferentialDecoderIhE5resetEjh");
void ref_pd_process(void *s, unsigned char *in, unsigned char *out)
	asm("ref__ZN27ParallelDifferentialDecoderIhE7processEPhS1_");

/* Scrambler<h,h>/<h,i> scalar + bulk; Descrambler<h,i>/<i,i> scalar. */
int  ref_shh_proc1(void *s, unsigned char in)
	asm("ref__ZN9ScramblerIhhE7processEh");
int  ref_shi_proc1(void *s, unsigned char in)
	asm("ref__ZN9ScramblerIhiE7processEh");
int  ref_dhi_proc1(void *s, unsigned char in)
	asm("ref__ZN11DescramblerIhiE7processEh");
int  ref_dii_proc1(void *s, int in)
	asm("ref__ZN11DescramblerIiiE7processEi");
void ref_shh_procn(void *s, const unsigned char *in, unsigned char *out, unsigned n)
	asm("ref__ZN9ScramblerIhhE7processEPKhPhj");
void ref_dhi_procn(void *s, const unsigned char *in, int *out, unsigned n)
	asm("ref__ZN11DescramblerIhiE7processEPKhPij");
void ref_sih_procn(void *s, const int *in, unsigned char *out, unsigned n)
	asm("ref__ZN9ScramblerIihE7processEPKiPhj");
void ref_shh_reset(void *s, unsigned char value)
	asm("ref__ZN9ScramblerIhhE5resetEh");
void ref_shi_reset(void *s, unsigned char value)
	asm("ref__ZN9ScramblerIhiE5resetEh");
void ref_dhi_reset(void *s, unsigned char value)
	asm("ref__ZN11DescramblerIhiE5resetEh");
void ref_dii_reset(void *s, int value)
	asm("ref__ZN11DescramblerIiiE5resetEi");

/* V90SignBitsExtractor (SIZE 1/417) and V92Mapper (SIZE 2/107), both stateful. */
void ref_sbe_ctor(void *s) asm("ref__ZN20V90SignBitsExtractorC1Ev");
void ref_sbe_dtor(void *s) asm("ref__ZN20V90SignBitsExtractorD1Ev");
void ref_sbe_reset(void *s, unsigned spacing, unsigned state)
	asm("ref__ZN20V90SignBitsExtractor5resetEjj");
void ref_sbe_process(void *s, unsigned char *in, unsigned char *out)
	asm("ref__ZN20V90SignBitsExtractor7processEPhS0_");

void ref_v92m_ctor(void *s) asm("ref__ZN9V92MapperC1Ev");
void ref_v92m_dtor(void *s) asm("ref__ZN9V92MapperD1Ev");
void ref_v92m_reset(void *s, short scale, unsigned char mode)
	asm("ref__ZN9V92Mapper5resetEsh");
int  ref_v92m_process(void *s, unsigned char *bits)
	asm("ref__ZN9V92Mapper7processEPh");
}

/* ------------------------------------------------------------------------ */

static volatile unsigned long long g_checks;

static void
g_report(void)
{
	fprintf(stderr, "FUZZCOV total comparisons: %llu\n", g_checks);
}

struct _report_reg {
	_report_reg() { atexit(g_report); }
};
static _report_reg _g_report_reg;

static void
ck(int okay, const char *what, long long got, long long ref, int fam, int step)
{
	g_checks++;
	if (!okay) {
		fprintf(stderr,
			"FUZZCOV DIVERGENCE fam=%d step=%d %s: got %ld, reference %ld\n",
			fam, step, what, got, ref);
		__builtin_trap();
	}
}

/* Deterministic, in-bounds cursor over the fuzzer input. */
struct cur {
	const uint8_t *p;
	size_t n, i;
};

static unsigned
cu8(struct cur *c)
{
	if (c->i >= c->n)
		return 0;
	return c->p[c->i++];
}

static uint32_t
cu32(struct cur *c)
{
	uint32_t v = 0;
	int k;
	for (k = 0; k < 4; k++)
		v = (v << 8) | cu8(c);
	return v;
}

/* Scratch buffers.  Bounded so ASan stays cheap. */
#define PBUF 256u
#define SBUF 64u
#define SBUZZ 40u

/* ------------------------------------------------------------------------ */
/* fam 0/1: parallel encoder/decoder, construction + drain. */
static const unsigned caps[7] = { 1, 2, 4, 8, 16, 32, 64 };

/* The object's layout: state ptr / capacity / size (12 bytes, found F871). */
struct pobj {
	unsigned char *state;
	unsigned cap;
	unsigned size;
};

static void
fam_parallel(int is_dec, const uint8_t *d, size_t n)
{
	struct cur c = { d, n, 0 };
	unsigned cap = caps[cu8(&c) % 7u];
	unsigned init = cu8(&c);
	unsigned steps = 1u + cu8(&c) % 12u;
	int inplace = cu8(&c) & 1u;
	union {
		unsigned char raw[sizeof(ParallelDifferentialEncoder<unsigned char>)];
		double align_;
	} oa, ob_;
	ParallelDifferentialEncoder<unsigned char> &eoa = *(ParallelDifferentialEncoder<unsigned char> *)&oa;
	ParallelDifferentialEncoder<unsigned char> &eob = *(ParallelDifferentialEncoder<unsigned char> *)&ob_;
	ParallelDifferentialDecoder<unsigned char> &doa = *(ParallelDifferentialDecoder<unsigned char> *)&oa;
	unsigned s;

	if (is_dec)
		::new((void *)&oa) ParallelDifferentialDecoder<unsigned char>(cap);
	else
		::new((void *)&oa) ParallelDifferentialEncoder<unsigned char>(cap);
	if (is_dec)
		ref_pd_ctor(&ob_, cap);
	else
		ref_pe_ctor(&ob_, cap);

	for (s = 0; s < steps; s++) {
		unsigned char ain[PBUF], aout[PBUF], bout[PBUF];
		unsigned width = cu8(&c) % (cap + 4u);
		int ra, rb, i, j;

		ra = is_dec ? doa.reset(width, init) : eoa.reset(width, init);
		rb = is_dec ? ref_pd_reset(&ob_, width, init) : ref_pe_reset(&ob_, width, init);
		ck(ra == rb, (is_dec ? "d" : "e") /*reset ret*/, ra, rb, is_dec, s);

		for (i = 0; i < (int)PBUF; i++)
			ain[i] = (unsigned char)cu8(&c);
		for (i = 0; i < (int)PBUF; i++)
			aout[i] = bout[i] = 0xa5;

		if (is_dec) {
			if (inplace) { memcpy(aout, ain, PBUF); memcpy(bout, ain, PBUF);
				doa.process(aout, aout); ref_pd_process(&ob_, bout, bout); }
			else { doa.process(ain, aout); ref_pd_process(&ob_, ain, bout); }
		} else {
			if (inplace) { memcpy(aout, ain, PBUF); memcpy(bout, ain, PBUF);
				eoa.process(aout, aout); ref_pe_process(&ob_, bout, bout); }
			else { eoa.process(ain, aout); ref_pe_process(&ob_, ain, bout); }
		}

		for (i = 0; i < (int)PBUF; i++)
			ck(aout[i] == bout[i], "out", aout[i], bout[i], is_dec, s);

		/* Compare the internal state buffer contents via the layout. */
		{
			const pobj *pa = (const pobj *)&oa;
			const pobj *pb = (const pobj *)&ob_;
			ck(pa->cap == pb->cap, "state cap", pa->cap, pb->cap, is_dec, s);
			ck(pa->size == pb->size, "state size", pa->size, pb->size, is_dec, s);
			for (j = 0; j < (int)cap; j++) {
				unsigned char va = pa->state ? pa->state[j] : 0;
				unsigned char vb = pb->state ? pb->state[j] : 0;
				ck(va == vb, "state", va, vb, is_dec, s);
			}
		}
	}

	if (is_dec) { doa.~ParallelDifferentialDecoder<unsigned char>(); ref_pd_dtor(&ob_); }
	else { eoa.~ParallelDifferentialEncoder<unsigned char>(); ref_pe_dtor(&ob_); }
}

/* ------------------------------------------------------------------------ */
/* fam 2: Scrambler<h,h> bulk (byte-exact positive control).  The object is
 * hair-packed into a SEPARATE slot and the history buffer is a SEPARATE array
 * -- the object's pointer fields must not share memory with the tap data,
 * or the taps read the pointer bytes (which encode the object's own address
 * and differ between the two sides).  Mirrors test/unit/t_fuzz.cpp `place`.
 */
template <class S>
struct slot {
	union {
		unsigned char raw[sizeof(S)];
		double align_;
	};
	S &o;
	slot() : o(*(S *)raw) {}
};

static void
place_scram(Scrambler<unsigned char, unsigned char> *s, unsigned char *buf,
	    unsigned out)
{
	s->pLimit = buf;
	s->pInitOut = buf + 40;
	s->pInitTap1 = buf + 45;
	s->pInitTap2 = buf + 63;
	s->pOut = buf + out;
	s->pTap1 = buf + out + 5;
	s->pTap2 = buf + out + 23;
	s->tailLength = 23;
}

static void
fam_scram_hh(const uint8_t *d, size_t n)
{
	struct cur c = { d, n, 0 };
	unsigned rounds = 1u + cu8(&c) % 40u;
	unsigned r;

	for (r = 0; r < rounds; r++) {
		slot<Scrambler<unsigned char, unsigned char> > sa, sb;
		unsigned char ba[256], bb[256];
		unsigned char ain[SBUZZ], aout[SBUZZ], bout[SBUZZ];
		unsigned out = cu8(&c) & 7u;
		unsigned nao = 1u + cu8(&c) % SBUZZ;
		unsigned k;

		for (k = 0; k < SBUF; k++)
			ba[k] = bb[k] = (unsigned char)cu8(&c);
		place_scram(&sa.o, ba, out);
		place_scram(&sb.o, bb, out);

		for (k = 0; k < nao; k++)
			ain[k] = (unsigned char)cu8(&c);
		sa.o.process(ain, aout, nao);
		ref_shh_procn(&sb.o, ain, bout, nao);
		for (k = 0; k < nao; k++)
			ck(aout[k] == bout[k], "scram out", aout[k], bout[k], 2, r);
	}
}

/* ------------------------------------------------------------------------ */
/* fam 3: V90SignBitsExtractor process, stateful construction+drain. */

static void
fam_sbe(const uint8_t *d, size_t n)
{
	struct cur c = { d, n, 0 };
	static const unsigned spacings[4] = { 1, 2, 3, 6 };
	unsigned spacing = spacings[cu8(&c) % 4u];
	unsigned state = cu8(&c) & 1u;	/* only 0/1: D386 recorded deviation */
	unsigned width = V90SBE_DECODER_SIZE / spacing;
	unsigned iters = 1u + cu8(&c) % 30u;
	unsigned k;
	union {
		unsigned char raw[sizeof(V90SignBitsExtractor)];
		double align_;
	} sa, sb;
	V90SignBitsExtractor *pa = (V90SignBitsExtractor *)&sa;
	V90SignBitsExtractor *pb = (V90SignBitsExtractor *)&sb;

	::new((void *)&sa) V90SignBitsExtractor();
	ref_sbe_ctor(&sb);
	pa->reset(spacing, state);
	ref_sbe_reset(&sb, spacing, state);

	for (k = 0; k < iters; k++) {
		unsigned char ain[V90SBE_DECODER_SIZE];
		unsigned char aout[V90SBE_DECODER_SIZE], bout[V90SBE_DECODER_SIZE];
		unsigned j;
		for (j = 0; j < width; j++)
			ain[j] = (unsigned char)cu8(&c);
		for (j = 0; j < V90SBE_DECODER_SIZE; j++)
			aout[j] = bout[j] = 0xa5;
		pa->process(ain, aout);
		ref_sbe_process(&sb, ain, bout);
		for (j = 0; j < V90SBE_DECODER_SIZE; j++)
			ck(aout[j] == bout[j], "sbe out", aout[j], bout[j], 3, k);
		ck(pa->state == pb->state, "sbe state", pa->state, pb->state, 3, k);
	}
	pa->~V90SignBitsExtractor();
	ref_sbe_dtor(&sb);
}

/* ------------------------------------------------------------------------ */
/* fam 4: V92Mapper process, stateful construction+drain (bit-per-byte). */

static void
fam_v92m(const uint8_t *d, size_t n)
{
	struct cur c = { d, n, 0 };
	short scale = (short)cu32(&c);
	unsigned char mode = (unsigned char)(cu8(&c) & 1u);
	unsigned nbits = (mode == 0) ? 2u : 3u;
	unsigned iters = 1u + cu8(&c) % 40u;
	unsigned k;
	union {
		unsigned char raw[sizeof(V92Mapper)];
		double align_;
	} va, vb;
	V92Mapper *pa = (V92Mapper *)&va;
	V92Mapper *pb = (V92Mapper *)&vb;

	::new((void *)&va) V92Mapper();
	ref_v92m_ctor(&vb);
	pa->reset(scale, mode);
	ref_v92m_reset(&vb, scale, mode);

	for (k = 0; k < iters; k++) {
		unsigned char bits[8];
		unsigned j;
		for (j = 0; j < nbits; j++)
			bits[j] = (unsigned char)(cu8(&c) & 1u);
		{
			int ra = pa->process(bits);
			int rb = ref_v92m_process(&vb, bits);
			ck(ra == rb, "v92m out", ra, rb, 4, k);
		}
	}
	pa->~V92Mapper();
	ref_v92m_dtor(&vb);
}

/* ------------------------------------------------------------------------ */

extern "C" int
LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
	if (Size == 0)
		return 0;
	switch (Data[0] % 5u) {
	case 0: fam_parallel(0, Data + 1, Size - 1); break;
	case 1: fam_parallel(1, Data + 1, Size - 1); break;
	case 2: fam_scram_hh(Data + 1, Size - 1); break;
	case 3: fam_sbe(Data + 1, Size - 1); break;
	case 4: fam_v92m(Data + 1, Size - 1); break;
	default: break;
	}
	return 0;
}
