/*
 * t_fuzz.cpp -- equivalence fuzzing: many seeded pseudo-random inputs driven
 * through BOTH the blob's reference and this tree's reconstruction, compared
 * EXACTLY at every step.  This is non-whole-tree mutation: no rebuild, no
 * planted alternative -- just input-domain breadth against the blob-as-oracle,
 * the cheapest equivalence evidence the tree can produce.
 *
 * THIS IS IN `test/`, SO IT IS APPARATUS, NOT RECONSTRUCTION.  The differential
 * rule does not apply; the denominator rule does.  Every target group prints
 * its own "PASS ... N checks" through diff_begin/diff_end, so a group that
 * quietly ran nothing is distinguishable from a group that passed (F134,
 * F2400).  The FIXED SEED is stated below and every case index is derived from
 * the seed alone, so a run is reproducible bit for bit.
 *
 * THE TARGETS ARE DELIBERATELY NOT THE EXACT SET.  The value of this fixture
 * is breadth over functions the byte-identity tier has NOT certified, where a
 * finite differential fixture is the only evidence.  `ParallelDifferential
 * Encoder<h>::process` and its decoder are near-exact (byteident BYTES,
 * 4 of 54 -- same size, four differing bytes) and integer-only, so a
 * divergence detected here is a genuine behavioural difference, not an x87
 * rounding artefact.  The scrambler/descrambler members are byte-exact and
 * serve as the positive control for the harness itself: if seeding them with
 * random taps, history and streams did NOT come out identical, the harness
 * would be broken.
 *
 * WHY INTEGER.  Fuzzing a float path legitimately crosses the x32->x64 /
 * x87 boundary only on the MODERN tier; on the PERIOD tier the comparison
 * must be exact and these integer kernels are exact under that compiler with
 * no declared divergence.  There is no tolerance here, and none is wanted.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/DiffCoder.h"
#include "dsplib/Scrambler.h"
#include "dsplib/sysdep.h"

extern "C" {
/* Parallel encoder / decoder (near-exact). */
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

/*
 * OUR side.  The ref_* block above is the BLOB; the un-prefixed entry points
 * are this tree's reconstruction.  The parallel coders are header-inlined, so
 * these name the weak symbols this tree's own instantiation emits -- which is
 * exactly the side the byte-identity set has NOT certified byte-for-byte
 * (byteident BYTES, 4 of 54), and therefore the side the fuzz must exercise
 * against the blob.  (The first iteration of this fixture compared ref_ to
 * ref_ and always passed; the planted-bug negative control caught it.)
 */
void our_pe_ctor(void *s, unsigned n) asm("_ZN27ParallelDifferentialEncoderIhEC1Ej");
void our_pe_dtor(void *s) asm("_ZN27ParallelDifferentialEncoderIhED1Ev");
int  our_pe_reset(void *s, unsigned n, unsigned char init)
	asm("_ZN27ParallelDifferentialEncoderIhE5resetEjh");
void our_pe_process(void *s, unsigned char *in, unsigned char *out)
	asm("_ZN27ParallelDifferentialEncoderIhE7processEPhS1_");

void our_pd_ctor(void *s, unsigned n) asm("_ZN27ParallelDifferentialDecoderIhEC1Ej");
void our_pd_dtor(void *s) asm("_ZN27ParallelDifferentialDecoderIhED1Ev");
int  our_pd_reset(void *s, unsigned n, unsigned char init)
	asm("_ZN27ParallelDifferentialDecoderIhE5resetEjh");
void our_pd_process(void *s, unsigned char *in, unsigned char *out)
	asm("_ZN27ParallelDifferentialDecoderIhE7processEPhS1_");

/* Scrambler<h,h> -- bulk and single-value process (byte-exact control). */
void ref_shh_proc1(void *s, unsigned char in)
	asm("ref__ZN9ScramblerIhhE7processEh");
void ref_shh_procn(void *s, const unsigned char *in, unsigned char *out, unsigned n)
	asm("ref__ZN9ScramblerIhhE7processEPKhPhj");
void ref_shh_ones(void *s, unsigned char *out, unsigned n)
	asm("ref__ZN9ScramblerIhhE14processAllOnesEPhj");

/* Descrambler<h,i> -- bulk process across the width family. */
void ref_dhi_procn(void *s, const unsigned char *in, int *out, unsigned n)
	asm("ref__ZN11DescramblerIhiE7processEPKhPij");
}

/*
 * ---------------------------------------------------------------------------
 * DETERMINISTIC PSEUDO-RANDOM SOURCE.  The seed is a single constant; every
 * (case, arm) draws from a fresh LFSR chain so a run is reproducible and never
 * depends on malloc layout or the phase of the moon.
 * ---------------------------------------------------------------------------
 */
#define FUZZ_SEED 0x5eed1234u

static unsigned
next_word(void)
{
	static unsigned s = FUZZ_SEED;
	s = (s >> 1) ^ (-(int)(s & 1u) & 0xb400u);
	return s;
}

/* ------------------------------------------------------------------------ */
/* The near-exact integer kernel: ParallelDifferentialEncoder<h>/<h>, decoder. */

struct ref_parallel {
	unsigned char *state;
	unsigned	capacity;
	unsigned	size;
};

struct coder {
	void (*ctor)(void *, unsigned);
	void (*dtor)(void *);
	int  (*reset)(void *, unsigned, unsigned char);
	void (*process)(void *, unsigned char *, unsigned char *);
};

/*
 * Each family has an OUR coder and a REF coder; a fuzz case runs the OUR one
 * on one object and the REF one on the other, and compares.  A single coder
 * would run both sides on the SAME implementation and always pass -- the
 * planted-bug negative control caught exactly that.
 */
static const coder enc_our = { our_pe_ctor, our_pe_dtor, our_pe_reset, our_pe_process };
static const coder enc_ref = { ref_pe_ctor, ref_pe_dtor, ref_pe_reset, ref_pe_process };
static const coder dec_our = { our_pd_ctor, our_pd_dtor, our_pd_reset, our_pd_process };
static const coder dec_ref = { ref_pd_ctor, ref_pd_dtor, ref_pd_reset, ref_pd_process };

#define PBUF 256u

static void
parallel_cmp(const void *a, const void *b, unsigned cap, long tag)
{
	const ref_parallel *x = (const ref_parallel *)a;
	const ref_parallel *y = (const ref_parallel *)b;
	unsigned i;

	diff_eq_int("capacity", x->capacity, y->capacity, tag);
	diff_eq_int("size", x->size, y->size, tag);
	diff_eq_int("state allocated", x->state != 0, y->state != 0, tag);
	if (!x->state || !y->state)
		return;
	for (i = 0; i < cap; i++)
		diff_eq_int("state", x->state[i], y->state[i], tag * 10000 + i);
	/* Nothing may be written past the 12-byte object. */
	for (i = 0; i < 32 - sizeof(ref_parallel); i++)
		diff_eq_int("wrote past object",
			    ((const unsigned char *)a)[sizeof(ref_parallel) + i],
			    ((const unsigned char *)b)[sizeof(ref_parallel) + i],
			    tag * 10000 + 500 + i);
}

/*
 * One fuzz case.  `cap` comes from a tiny legal set, `width` is drawn up to
 * cap+3 so the refuse path (width > capacity) is hit frequently, and the
 * input stream is 100% pseudorandom.  Outputs are compared over the WHOLE
 * PBUF, prefilled identically, so a write beyond the active width is a failure
 * rather than silence.  In-place processing (in == out) is also driven,
 * because it pins the read/write order inside the loop.
 */
static void
parallel_case(const coder *ours, const coder *refs, unsigned cap,
	      unsigned width, unsigned char init, int in_place, long tag)
{
	static unsigned char oa[32], ob[32];
	static unsigned char ain[PBUF], bout[PBUF], aout[PBUF];
	unsigned i;

	memset(oa, 0x5a, sizeof(oa));
	memset(ob, 0x5a, sizeof(ob));
	ours->ctor(oa, cap);
	refs->ctor(ob, cap);

	memset(ain, 0xa5, sizeof(ain));
	for (i = 0; i < PBUF; i++)
		ain[i] = (unsigned char)(next_word() >> 3);

	{
		int ra = ours->reset(oa, width, init);
		int rb = refs->reset(ob, width, init);
		diff_eq_int("reset return", ra, rb, tag);
	}
	parallel_cmp(oa, ob, cap, tag);

	memset(aout, 0xa5, sizeof(aout));
	memset(bout, 0xa5, sizeof(bout));
	if (in_place) {
		memcpy(aout, ain, PBUF);
		memcpy(bout, ain, PBUF);
		ours->process(oa, aout, aout);
		refs->process(ob, bout, bout);
	} else {
		ours->process(oa, ain, aout);
		refs->process(ob, ain, bout);
	}
	for (i = 0; i < PBUF; i++)
		diff_eq_int("out", aout[i], bout[i], tag * 10000 + 600 + i);
	parallel_cmp(oa, ob, cap, tag);

	ours->dtor(oa);
	refs->dtor(ob);
	diff_eq_int("state after dtor",
		    ((const ref_parallel *)oa)->state != 0,
		    ((const ref_parallel *)ob)->state != 0, tag * 10000 + 900);
}

#define NPARALLEL 40000u

static int
fuzz_parallel(const coder *ours, const coder *refs, const char *name)
{
	static const unsigned caps[] = { 1, 2, 4, 8, 16, 32, 64 };
	unsigned i;

	diff_begin(name);
	for (i = 0; i < NPARALLEL; i++) {
		unsigned cap = caps[next_word() % (sizeof(caps) / sizeof(caps[0]))];
		unsigned width = next_word() % (cap + 4u);	/* cap .. cap+3 refuse */
		unsigned char init = (unsigned char)(next_word() >> 3);
		int in_place = (next_word() & 1u) != 0;
		parallel_case(ours, refs, cap, width, init, in_place,
			      (long)i * 10 + (in_place ? 1 : 0));
	}
	return diff_end();
}

/* ------------------------------------------------------------------------ */
/* The byte-exact positive control: Scrambler<h,h> and Descrambler<h,i>.

 * These are exact, so they are the control that proves the harness sees no
 * false divergence.  Taps, history and stream are all fuzzed.  The object is
 * placement-built into a raw slot (finding F871); the layout offsets are the
 * header's, and the pointers are set so the restarted run crosses several
 * times.
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

#define SBUF	64u		/* history elements			  */
#define SOUT	40u		/* pInitOut distance			  */
#define STAP1	45u
#define STAP2	63u
#define STAIL	23u
#define SFUZZB	200u		/* bytes per bulk call, >> SBUF to restart */

template <class S, class T>
static void
place(S *s, T *buf, unsigned out)
{
	s->pLimit = buf;
	s->pInitOut = buf + SOUT;
	s->pInitTap1 = buf + STAP1;
	s->pInitTap2 = buf + STAP2;
	s->pOut = buf + out;
	s->pTap1 = buf + out + (STAP1 - SOUT);
	s->pTap2 = buf + out + (STAP2 - SOUT);
	s->tailLength = STAIL;
}

/*
 * Bulk process with pseudorandom history and stream.  No ctor/reset: the
 * object is placement-built and the history is the seeded fill.  The pointer
 * `out` is drawn small so a 200-element run restarts several times, which is
 * the widely-separated-in-time-but-behaviourally-relevant branch of the class
 * (F869-871).  `Descrambler<h,i>` is included specifically because its output
 * width is `I` = int while the history is `T` = byte: the intermediate-width
 * path (F870) is only observable at values that do not fit a byte.
 */
static int
fuzz_scrambler_hh(void)
{
	unsigned i;

	diff_begin("Scrambler<h,h>::process, fuzz");
	for (i = 0; i < 30000u; i++) {
		slot<Scrambler<unsigned char, unsigned char> > sa, sb;
		unsigned char ba[SBUF], bb[SBUF];
		unsigned char ain[SFUZZB], aout[SFUZZB], bout[SFUZZB];
		unsigned out = next_word() & 7u;
		unsigned k;
		long tag = (long)i;

		for (k = 0; k < SBUF; k++)
			ba[k] = bb[k] = (unsigned char)(next_word() >> 3);
		place(&sa.o, ba, out);
		place(&sb.o, bb, out);
		for (k = 0; k < SFUZZB; k++)
			ain[k] = (unsigned char)(next_word() >> 3);

		sa.o.process(ain, aout, SFUZZB);
		ref_shh_procn(&sb.o, ain, bout, SFUZZB);

		for (k = 0; k < SFUZZB; k++)
			diff_eq_int("scram out", aout[k], bout[k], tag * 1000 + k);
		for (k = 0; k < SBUF; k++)
			diff_eq_int("scram history", ba[k], bb[k], tag * 1000 + 700 + k);
	}
	return diff_end();
}

static int
fuzz_descrambler_hi(void)
{
	unsigned i;

	diff_begin("Descrambler<h,i>::process, fuzz");
	for (i = 0; i < 20000u; i++) {
		slot<Descrambler<unsigned char, int> > sa, sb;
		unsigned char ba[SBUF], bb[SBUF];
		unsigned char ain[SFUZZB];
		int aout[SFUZZB], bout[SFUZZB];
		unsigned out = next_word() & 7u;
		unsigned k;
		long tag = (long)i;

		for (k = 0; k < SBUF; k++)
			ba[k] = bb[k] = (unsigned char)(next_word() >> 3);
		place(&sa.o, ba, out);
		place(&sb.o, bb, out);
		for (k = 0; k < SFUZZB; k++)
			ain[k] = (unsigned char)(next_word() >> 3);

		sa.o.process(ain, aout, SFUZZB);
		ref_dhi_procn(&sb.o, ain, bout, SFUZZB);

		for (k = 0; k < SFUZZB; k++)
			diff_eq_int("descram out", aout[k], bout[k], tag * 1000 + k);
		for (k = 0; k < SBUF; k++)
			diff_eq_int("descram history", ba[k], bb[k], tag * 1000 + 700 + k);
	}
	return diff_end();
}

/* ------------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;

	rc |= fuzz_parallel(&enc_our, &enc_ref,
			    "ParallelDifferentialEncoder<h>::process, fuzz");
	rc |= fuzz_parallel(&dec_our, &dec_ref,
			    "ParallelDifferentialDecoder<h>::process, fuzz");
	rc |= fuzz_scrambler_hh();
	rc |= fuzz_descrambler_hi();
	return rc;
}
