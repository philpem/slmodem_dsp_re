/*
 * t_scrambler.cpp -- differential test of every member of every instantiation
 * of `Scrambler<T, I>` and `Descrambler<T, I>`.
 *
 * Thirty-four weak template members over five instantiations, 2,242 bytes.
 * They are `W` in the blob rather than `T`, and symmap.py renames weak symbols
 * like everything else, so all thirty-four have a `ref_` alias -- checked with
 * `nm build/dsplibs_ref.o | grep -E '_ZN9ScramblerI|_ZN11DescramblerI'`, which
 * lists exactly thirty-four -- and every one is driven DIRECTLY here.
 *
 * THE FIVE INSTANTIATIONS DO NOT HAVE THE SAME MEMBERS.  `Scrambler<h,i>` has
 * no bulk `process`; `Scrambler<i,h>` has no single-value one; `processAllOnes`
 * and `processAllZeros` exist only at `<h,h>`; `Descrambler<i,i>` has no bulk
 * form.  Driving a member an instantiation does not have would name a `ref_`
 * alias that does not exist and fail the link, so each block below names its
 * own set and `run_all` asserts the total is thirty-four -- a count of what was
 * examined, because a block that quietly did nothing reads exactly like a clean
 * one (docs/method/gates.md, rule 1).
 *
 * ---------------------------------------------------------------------------
 * THE POINTERS MUST BE VALID AND CONGRUENT.  Seven of the eight fields are
 * dereferenced; random bytes there segfault.  Each side gets its own buffer of
 * the same size with the same relative layout, so `pOut - buf` is comparable
 * even though `pOut` never is.  The raw pointers are never compared and never
 * merely checked non-null: two arrays at two addresses would pass that and
 * prove nothing (finding F224).
 *
 * The layout is 64 elements with the output cursor 40 above the limit and taps
 * 5 and 23 above the cursor, so 41 symbols reach the restart and a run of 100
 * crosses it twice.  The restart -- `resetHistoryIndexes` then
 * `copyHistoryTail` -- is a third of the class and is invisible with a bigger
 * buffer.  It is the same layout `t_v90p3mod` uses, and it is also exactly what
 * `Scrambler(5, 23, 40)` builds, which is why the constructor block can compare
 * its output against it.
 *
 * TWO EMPTY THINGS COMPARE EQUAL, so:
 *   - the history is seeded with varied values, not zeros.  An all-zero
 *     history makes `processAllZeros` and `process` indistinguishable.
 *   - `<i,h>`'s history is seeded with values ABOVE 0xff on purpose.  The
 *     intermediate is `I` = `unsigned char` there and the history is `int`, so
 *     the object stores a TRUNCATED, zero-extended byte; a `T` intermediate
 *     would store the full 32-bit XOR.  `cov_wide_tap` asserts the sweep
 *     actually produced an operand that big (finding F870).
 *   - every restart-crossing block counts its crossings and asserts the count
 *     is non-zero.
 *   - the constructor sweep asserts it contained a case with `a`, `b` and `c`
 *     pairwise distinct and all non-zero.  With `a == b` the two tap fields
 *     collapse and with `c == 0` `pInitOut == pLimit`, and any permutation of
 *     the three arguments then compares equal.
 *   - the destructor asserts the allocator's `frees` moved by exactly one per
 *     side and `bad_free` by none.  A destructor that freed nothing would
 *     otherwise pass.
 *
 * `sysdep_malloc` and `sysdep_free` are on symmap.py's deliberately-not-renamed
 * list, so both sides call the same instrumented allocator and
 * `harness_alloc.bytes` is what pins the allocation SIZE -- the only field that
 * can see `(1 + b + c) * sizeof(T)` and therefore the `shl $0x2` the `int`
 * instantiations carry.
 *
 * The `ref_` aliases are reached through asm() labels rather than by spelling
 * the alias as an identifier, which sidesteps finding F225 entirely.  The
 * convention is plain cdecl with `this` as the first stack argument
 * (finding F215).
 */

#include <string.h>

#include "harness.h"
#include "dsplib/Scrambler.h"

typedef Scrambler<unsigned char, unsigned char> ScramblerHH;
typedef Scrambler<unsigned char, int> ScramblerHI;
typedef Scrambler<int, unsigned char> ScramblerIH;
typedef Descrambler<unsigned char, int> DescramblerHI;
typedef Descrambler<int, int> DescramblerII;

/*
 * The single-value `process` returns `T`, which the header says is one byte
 * for three of the four that have it.  It is declared `int` here for the same
 * reason t_v90p3mod does: that makes the comparison cover all thirty-two bits
 * rather than the low eight, and `never returned past a byte` becomes a claim
 * the test checks rather than one it assumes.
 */
extern "C" {
/* Scrambler<unsigned char, unsigned char> -- nine. */
void ref_shh_ctor(void *, unsigned, unsigned, unsigned)
	asm("ref__ZN9ScramblerIhhEC1Ejjj");
void ref_shh_dtor(void *) asm("ref__ZN9ScramblerIhhED1Ev");
void ref_shh_rhi(void *) asm("ref__ZN9ScramblerIhhE19resetHistoryIndexesEv");
void ref_shh_cht(void *) asm("ref__ZN9ScramblerIhhE15copyHistoryTailEv");
void ref_shh_reset(void *, unsigned char) asm("ref__ZN9ScramblerIhhE5resetEh");
int ref_shh_proc1(void *, unsigned char) asm("ref__ZN9ScramblerIhhE7processEh");
void ref_shh_procn(void *, const unsigned char *, unsigned char *, unsigned)
	asm("ref__ZN9ScramblerIhhE7processEPKhPhj");
void ref_shh_ones(void *, unsigned char *, unsigned)
	asm("ref__ZN9ScramblerIhhE14processAllOnesEPhj");
void ref_shh_zeros(void *, unsigned char *, unsigned)
	asm("ref__ZN9ScramblerIhhE15processAllZerosEPhj");

/* Scrambler<unsigned char, int> -- six, no bulk form. */
void ref_shi_ctor(void *, unsigned, unsigned, unsigned)
	asm("ref__ZN9ScramblerIhiEC1Ejjj");
void ref_shi_dtor(void *) asm("ref__ZN9ScramblerIhiED1Ev");
void ref_shi_rhi(void *) asm("ref__ZN9ScramblerIhiE19resetHistoryIndexesEv");
void ref_shi_cht(void *) asm("ref__ZN9ScramblerIhiE15copyHistoryTailEv");
void ref_shi_reset(void *, unsigned char) asm("ref__ZN9ScramblerIhiE5resetEh");
int ref_shi_proc1(void *, unsigned char) asm("ref__ZN9ScramblerIhiE7processEh");

/* Scrambler<int, unsigned char> -- six, bulk form only. */
void ref_sih_ctor(void *, unsigned, unsigned, unsigned)
	asm("ref__ZN9ScramblerIihEC1Ejjj");
void ref_sih_dtor(void *) asm("ref__ZN9ScramblerIihED1Ev");
void ref_sih_rhi(void *) asm("ref__ZN9ScramblerIihE19resetHistoryIndexesEv");
void ref_sih_cht(void *) asm("ref__ZN9ScramblerIihE15copyHistoryTailEv");
void ref_sih_reset(void *, int) asm("ref__ZN9ScramblerIihE5resetEi");
void ref_sih_procn(void *, const int *, unsigned char *, unsigned)
	asm("ref__ZN9ScramblerIihE7processEPKiPhj");

/* Descrambler<unsigned char, int> -- seven. */
void ref_dhi_ctor(void *, unsigned, unsigned, unsigned)
	asm("ref__ZN11DescramblerIhiEC1Ejjj");
void ref_dhi_dtor(void *) asm("ref__ZN11DescramblerIhiED1Ev");
void ref_dhi_rhi(void *) asm("ref__ZN11DescramblerIhiE19resetHistoryIndexesEv");
void ref_dhi_cht(void *) asm("ref__ZN11DescramblerIhiE15copyHistoryTailEv");
void ref_dhi_reset(void *, unsigned char)
	asm("ref__ZN11DescramblerIhiE5resetEh");
int ref_dhi_proc1(void *, unsigned char)
	asm("ref__ZN11DescramblerIhiE7processEh");
void ref_dhi_procn(void *, const unsigned char *, int *, unsigned)
	asm("ref__ZN11DescramblerIhiE7processEPKhPij");

/* Descrambler<int, int> -- six, no bulk form. */
void ref_dii_ctor(void *, unsigned, unsigned, unsigned)
	asm("ref__ZN11DescramblerIiiEC1Ejjj");
void ref_dii_dtor(void *) asm("ref__ZN11DescramblerIiiED1Ev");
void ref_dii_rhi(void *) asm("ref__ZN11DescramblerIiiE19resetHistoryIndexesEv");
void ref_dii_cht(void *) asm("ref__ZN11DescramblerIiiE15copyHistoryTailEv");
void ref_dii_reset(void *, int) asm("ref__ZN11DescramblerIiiE5resetEi");
int ref_dii_proc1(void *, int) asm("ref__ZN11DescramblerIiiE7processEi");
}

/* ------------------------------------------------------------------------ */

#define BUF	64u		/* history elements, both sides		*/
#define OUT	40u		/* pInitOut - pLimit			*/
#define TAP1	45u		/* pInitTap1 - pLimit			*/
#define TAP2	63u		/* pInitTap2 - pLimit			*/
#define TAIL	23u		/* tailLength				*/

#define CTOR_A	(TAP1 - OUT)	/* 5   -- the near tap's distance	*/
#define CTOR_B	(TAP2 - OUT)	/* 23  -- the far tap's, and tailLength	*/
#define CTOR_C	OUT		/* 40  -- pInitOut above pLimit		*/

#define NBULK	100u		/* bulk elements per call, > 41		*/

/* How many members each block drove; asserted against 34 at the end. */
static int members_driven;

static unsigned lfsr_state;

static unsigned
next_word(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return lfsr_state;
}

/*
 * `mode` picks how varied the fill is, and the modes matter:
 *   0  pseudorandom over the whole width of T
 *   1  all zeros -- the fill that makes process and processAllZeros agree
 *   2  all ones
 *   3  one bit per element, which is what `reset` leaves and what the V.90
 *      users actually put there
 * Mode 0 at `T = int` is what puts values above 0xff under the taps.
 */
template <class T>
static void
fill(T *a, T *b, unsigned int n, int trial, int mode)
{
	unsigned int i;

	lfsr_state = 0x7c1bu + 0x2545u * (unsigned)trial + (unsigned)mode;
	for (i = 0; i < n; i++) {
		T v;

		switch (mode) {
		case 0:  v = (T)(next_word() * 0x9e3779b9u);	break;
		case 1:  v = (T)0;				break;
		case 2:  v = (T)~(T)0;				break;
		default: v = (T)(next_word() & 1u);		break;
		}
		a[i] = b[i] = v;
	}
}

template <class S, class T>
static void
place(S *s, T *buf, unsigned int out)
{
	s->pLimit = buf;
	s->pInitOut = buf + OUT;
	s->pInitTap1 = buf + TAP1;
	s->pInitTap2 = buf + TAP2;
	s->pOut = buf + out;
	s->pTap1 = buf + out + (TAP1 - OUT);
	s->pTap2 = buf + out + (TAP2 - OUT);
	s->tailLength = TAIL;
}

/*
 * Every field of both objects, as each side's own offset into its own buffer,
 * plus the buffers themselves.
 */
template <class S, class T>
static void
cmp(const S *a, const S *b, const T *ba, const T *bb, unsigned int n, long tag)
{
	diff_eq_int("pLimit offset (case %ld)",
		    a->pLimit - ba, b->pLimit - bb, tag);
	diff_eq_int("pInitOut offset (case %ld)",
		    a->pInitOut - ba, b->pInitOut - bb, tag);
	diff_eq_int("pInitTap1 offset (case %ld)",
		    a->pInitTap1 - ba, b->pInitTap1 - bb, tag);
	diff_eq_int("pInitTap2 offset (case %ld)",
		    a->pInitTap2 - ba, b->pInitTap2 - bb, tag);
	diff_eq_int("pOut offset (case %ld)",
		    a->pOut - ba, b->pOut - bb, tag);
	diff_eq_int("pTap1 offset (case %ld)",
		    a->pTap1 - ba, b->pTap1 - bb, tag);
	diff_eq_int("pTap2 offset (case %ld)",
		    a->pTap2 - ba, b->pTap2 - bb, tag);
	diff_eq_int("tailLength (case %ld)", a->tailLength, b->tailLength, tag);
	diff_eq_int("the history (case %ld)",
		    memcmp(ba, bb, n * sizeof(T)) == 0, 1, tag);
}

/*
 * A slot big enough for the object, holding STORAGE and ALIGNMENT only.  It is
 * what lets the destructor be called on an object this file laid out by hand
 * -- see dsplib/Scrambler.h and finding F871.
 *
 * `o` IS A REFERENCE, and that is the whole trick.  This was a union with an
 * `S o` member and a user-provided empty constructor and destructor, which is
 * C++11: C++98 forbids a union member whose type has a non-trivial
 * constructor or destructor outright, and the author's compiler was C++98
 * (V7 in docs/method/compilers.md).  Binding a reference to the raw bytes
 * instead keeps every `x.o.member()` at the use sites reading exactly as
 * before -- 613 of them across the five files this affected -- while the
 * storage is a plain array that nothing constructs or destroys.
 */
template <class S>
struct slot {
	union {
		unsigned char raw[sizeof(S)];
		double align_;		/* alignment only; trivial */
	};
	S &o;

	slot() : o(*(S *)raw) {}
};

/* ------------------------------------------------------------------------ */
/* The five members every instantiation has.                                */

/*
 * `resetHistoryIndexes`, on its own so it cannot hide inside `reset`.
 */
template <class S, class T>
static void
drive_rhi(void (*rref)(void *), T *ba, T *bb)
{
	slot<S> sa, sb;
	int trial;

	for (trial = 0; trial < 16; trial++) {
		fill(ba, bb, BUF, trial, trial % 4);
		place(&sa.o, ba, (unsigned)trial % 41u);
		place(&sb.o, bb, (unsigned)trial % 41u);

		sa.o.resetHistoryIndexes();
		rref(&sb.o);
		cmp(&sa.o, &sb.o, ba, bb, BUF, trial);
	}
	members_driven++;
}

/*
 * `copyHistoryTail`, likewise -- and with `tailLength` swept from 0 up, since
 * the loop is `tailLength` iterations spelled as a decrement to -1 and zero is
 * the value that separates that from a pre-test loop.
 */
template <class S, class T>
static void
drive_cht(void (*rref)(void *), T *ba, T *bb, int *moved)
{
	slot<S> sa, sb;
	int trial;

	for (trial = 0; trial < 24; trial++) {
		unsigned char before[BUF * sizeof(T)];

		fill(ba, bb, BUF, trial, trial % 4);
		place(&sa.o, ba, (unsigned)trial % 41u);
		place(&sb.o, bb, (unsigned)trial % 41u);
		sa.o.tailLength = sb.o.tailLength = (unsigned)trial;
		memcpy(before, ba, sizeof(before));

		sa.o.copyHistoryTail();
		rref(&sb.o);
		cmp(&sa.o, &sb.o, ba, bb, BUF, trial);
		if (memcmp(before, ba, sizeof(before)) != 0)
			*moved = 1;
	}
	members_driven++;
}

/*
 * `reset`, whose masking of everything but bit 0 is the whole claim.  Every
 * value of the low byte is driven, and at `T = int` the sweep also carries
 * values whose low bit is set and whose other bits are not, and the reverse.
 */
template <class S, class T>
static void
drive_reset(void (*rref)(void *, T), T *ba, T *bb, int *saw0, int *saw1)
{
	slot<S> sa, sb;
	int trial, mode;

	for (mode = 0; mode < 4; mode++) {
		for (trial = 0; trial < 256; trial++) {
			T v = (T)(trial ^ (int)((unsigned)trial << 8));

			fill(ba, bb, BUF, trial, mode);
			place(&sa.o, ba, (unsigned)trial % 41u);
			place(&sb.o, bb, (unsigned)trial % 41u);

			sa.o.reset(v);
			rref(&sb.o, v);
			cmp(&sa.o, &sb.o, ba, bb, BUF,
			    (long)(mode * 1000 + trial));

			if ((trial & 1) == 0)
				*saw0 = 1;
			else
				*saw1 = 1;
		}
	}
	members_driven++;
}

/*
 * The constructor and the destructor together, because the destructor needs
 * something the constructor allocated and because `frees` is only meaningful
 * against the `allocs` that preceded it.
 *
 * `ours` is a real automatic object: there is no placement new here
 * (-nostdinc++, and the object used none either), so the only way to run a
 * constructor is to declare one.  `theirs` is a hand-laid slot the alias is
 * called on.  The inner scope is what makes our destructor's effect on the
 * allocator measurable separately from theirs.
 */
template <class S, class T>
static void
drive_ctor_dtor(void (*rctor)(void *, unsigned, unsigned, unsigned),
		void (*rdtor)(void *), unsigned a, unsigned b, unsigned c,
		long tag, int *distinct)
{
	slot<S> theirs;
	unsigned int words = 1u + b + c;
	int al0 = harness_alloc.allocs;
	unsigned by0 = harness_alloc.bytes;
	int fr0, bad0, fr_theirs, fr_ours;

	{
		S ours(a, b, c);

		rctor(&theirs.o, a, b, c);

		diff_eq_int("the constructors allocated once each (case %ld)",
			    harness_alloc.allocs - al0, 2, tag);
		diff_eq_int("(1 + b + c) * sizeof(T) bytes each (case %ld)",
			    harness_alloc.bytes - by0,
			    2u * words * sizeof(T), tag);

		cmp(&ours, &theirs.o, ours.pLimit, theirs.o.pLimit, words, tag);
		diff_eq_int("pInitOut is c above pLimit (case %ld)",
			    ours.pInitOut - ours.pLimit, (long)c, tag);
		diff_eq_int("pInitTap1 is a above pInitOut (case %ld)",
			    ours.pInitTap1 - ours.pInitOut, (long)a, tag);
		diff_eq_int("pInitTap2 is b above pInitOut (case %ld)",
			    ours.pInitTap2 - ours.pInitOut, (long)b, tag);
		diff_eq_int("tailLength is b (case %ld)",
			    ours.tailLength, (long)b, tag);

		bad0 = harness_alloc.bad_free;
		fr0 = harness_alloc.frees;
		rdtor(&theirs.o);
		fr_theirs = harness_alloc.frees - fr0;
		fr0 = harness_alloc.frees;
	}
	fr_ours = harness_alloc.frees - fr0;

	diff_eq_int("their destructor freed exactly once (case %ld)",
		    fr_theirs, 1, tag);
	diff_eq_int("our destructor freed exactly once (case %ld)",
		    fr_ours, 1, tag);
	diff_eq_int("neither freed something unknown (case %ld)",
		    harness_alloc.bad_free - bad0, 0, tag);

	if (a != b && b != c && a != c && a && b && c)
		*distinct = 1;
}

/*
 * The destructor's null arm, which the sweep above cannot reach: the allocator
 * never returns null, so `pLimit` is never zero after a construction.  Both
 * sides get a hand-laid object with `pLimit` cleared and are destroyed
 * explicitly.
 */
template <class S>
static void
drive_dtor_null(void (*rdtor)(void *))
{
	typedef S Self;
	slot<S> sa, sb;
	int null0 = harness_alloc.free_null;
	int fr0 = harness_alloc.frees;
	int bad0 = harness_alloc.bad_free;

	memset(sa.raw, 0, sizeof(sa.raw));
	memset(sb.raw, 0, sizeof(sb.raw));

	sa.o.~Self();
	rdtor(&sb.o);

	diff_eq_int("a null pLimit frees nothing", harness_alloc.frees - fr0,
		    0, 0);
	diff_eq_int("a null pLimit is not passed to the allocator",
		    harness_alloc.free_null - null0, 0, 0);
	diff_eq_int("a null pLimit is not a bad free",
		    harness_alloc.bad_free - bad0, 0, 0);
	members_driven += 2;			/* the constructor and this */
}

/* ------------------------------------------------------------------------ */
/* The process family.                                                      */

/*
 * The single-value `process`, run long enough to cross the restart twice.
 *
 * `alias` puts the near tap ON the output cursor.  It does NOT separate the
 * descrambler's `*pOut = in; r = *pOut ^ ...` from `r = in ^ ...` -- nothing
 * can, and the two are recorded `equivalent` in the mutation set with the
 * argument.  What it is: a distinct input configuration in which the store and
 * the tap read touch the same element, so an implementation that read the taps
 * before storing gives a different answer.  It costs one flag and it runs the
 * whole 100-symbol sweep again.
 */
template <class S, class T>
static void
drive_proc1(int (*rref)(void *, T), T *ba, T *bb, int *restarts, int *differed,
	    int *wide, int alias)
{
	slot<S> sa, sb;
	int trial, mode, i;

	for (mode = 0; mode < 4; mode++) {
		for (trial = 0; trial < 8; trial++) {
			unsigned int start = (unsigned)trial * 5u;

			fill(ba, bb, BUF, trial, mode);
			place(&sa.o, ba, start);
			place(&sb.o, bb, start);
			if (alias) {
				sa.o.pTap1 = sa.o.pOut;
				sb.o.pTap1 = sb.o.pOut;
			}

			for (i = 0; i < 100; i++) {
				T in = (T)(next_word() * 0x9e3779b9u);
				const T *before = sa.o.pOut;
				long tag = (long)(mode * 10000 + trial * 100
						  + i);
				int ra, rb;

				ra = (int)sa.o.process(in);
				rb = rref(&sb.o, in);

				diff_eq_int("process returned (case %ld)",
					    ra, rb, tag);
				cmp(&sa.o, &sb.o, ba, bb, BUF, tag);

				if (before == ba && sa.o.pOut == sa.o.pInitOut)
					(*restarts)++;
				if (ra != 0)
					*differed = 1;
				if (((unsigned)rb & ~0xffu) != 0)
					*wide = 1;
			}
		}
	}
	if (!alias)
		members_driven++;
}

/*
 * The bulk `process`.  It is driven twice over: as `NBULK` separate one-element
 * calls, which is the only way to see each restart as it happens, and as one
 * `NBULK`-element call, which is the only thing that checks the loop composes.
 *
 * `wide` records that an operand above 0xff reached the XOR.  At
 * `Scrambler<int, unsigned char>` that is the case which tells a `T`
 * intermediate from the `I` one the object has (finding F870); at the other
 * instantiations `T` is a byte and it never fires, which is why the assertion
 * is made by the caller and not here.
 */
template <class S, class T, class I>
static void
drive_procn(void (*rref)(void *, const T *, I *, unsigned), T *ba, T *bb,
	    T *in, I *oa, I *ob, int *restarts, int *wide)
{
	slot<S> sa, sb;
	int trial, mode, i, k, alias;

	for (mode = 0; mode < 4; mode++) {
	    for (alias = 0; alias < 2; alias++) {
		for (trial = 0; trial < 8; trial++) {
			unsigned int start = (unsigned)trial * 5u;
			long base = (long)(mode * 100000 + alias * 10000
					   + trial * 100);

			/* one element at a time */
			fill(ba, bb, BUF, trial, mode);
			fill(in, in, NBULK, trial + 64, mode);
			memset(oa, 0x5a, NBULK * sizeof(I));
			memset(ob, 0x5a, NBULK * sizeof(I));
			place(&sa.o, ba, start);
			place(&sb.o, bb, start);
			/*
			 * The near tap ON the cursor: the store and one tap
			 * read touch the same element.  See drive_proc1 for
			 * what this does and does not separate.  A valid
			 * comparison either way, because both sides are laid
			 * out identically.
			 */
			if (alias) {
				sa.o.pTap1 = sa.o.pOut;
				sb.o.pTap1 = sb.o.pOut;
			}

			for (i = 0; i < (int)NBULK; i++) {
				const T *before = sa.o.pOut;
				T t1 = *sa.o.pTap1, t2 = *sa.o.pTap2;
				T src = in[i];

				sa.o.process(in + i, oa + i, 1u);
				rref(&sb.o, in + i, ob + i, 1u);

				cmp(&sa.o, &sb.o, ba, bb, BUF, base + i);
				diff_eq_int("the destination (case %ld)",
					    memcmp(oa, ob, NBULK * sizeof(I))
					    == 0, 1, base + i);

				if (before == ba && sa.o.pOut == sa.o.pInitOut)
					(*restarts)++;
				if ((((unsigned)(src ^ t1 ^ t2)) & ~0xffu) != 0)
					*wide = 1;
			}

			/* and the whole run in one call */
			for (k = 0; k < 2; k++) {
				unsigned int n = k ? NBULK : 0u;

				fill(ba, bb, BUF, trial, mode);
				fill(in, in, NBULK, trial + 64, mode);
				memset(oa, 0x5a, NBULK * sizeof(I));
				memset(ob, 0x5a, NBULK * sizeof(I));
				place(&sa.o, ba, start);
				place(&sb.o, bb, start);
				if (alias) {
					sa.o.pTap1 = sa.o.pOut;
					sb.o.pTap1 = sb.o.pOut;
				}

				sa.o.process(in, oa, n);
				rref(&sb.o, in, ob, n);

				cmp(&sa.o, &sb.o, ba, bb, BUF,
				    base + 900 + k);
				diff_eq_int("the destination, bulk (case %ld)",
					    memcmp(oa, ob, NBULK * sizeof(I))
					    == 0, 1, base + 900 + k);
			}
		    }
		}
	}
	members_driven++;
}

/* ------------------------------------------------------------------------ */
/* The five blocks.  Each names exactly its own members.                    */

static unsigned char hh_a[BUF], hh_b[BUF], hh_in[NBULK];
static unsigned char hh_oa[NBULK], hh_ob[NBULK];

static int
run_hh(void)
{
	int restarts = 0, differed = 0, wide = 0, moved = 0, distinct = 0;
	int ones_moved = 0, zeros_moved = 0, saw0 = 0, saw1 = 0;
	int trial, mode, i;
	static const unsigned ctor_args[8][3] = {
		{ CTOR_A, CTOR_B, CTOR_C }, { 0x12, 0x17, 0x63 },
		{ 1, 2, 3 }, { 0, 7, 11 }, { 5, 0, 9 }, { 3, 8, 0 },
		{ 6, 6, 6 }, { 31, 7, 2 }
	};

	diff_begin("Scrambler<unsigned char, unsigned char>");

	for (i = 0; i < 8; i++)
		drive_ctor_dtor<ScramblerHH, unsigned char>(
		    ref_shh_ctor, ref_shh_dtor, ctor_args[i][0],
		    ctor_args[i][1], ctor_args[i][2], i, &distinct);
	drive_dtor_null<ScramblerHH>(ref_shh_dtor);
	drive_rhi<ScramblerHH>(ref_shh_rhi, hh_a, hh_b);
	drive_cht<ScramblerHH>(ref_shh_cht, hh_a, hh_b, &moved);
	drive_reset<ScramblerHH>(ref_shh_reset, hh_a, hh_b, &saw0, &saw1);
	drive_proc1<ScramblerHH>(ref_shh_proc1, hh_a, hh_b, &restarts,
				 &differed, &wide, 0);
	drive_procn<ScramblerHH>(ref_shh_procn, hh_a, hh_b, hh_in, hh_oa,
				 hh_ob, &restarts, &wide);

	/*
	 * processAllOnes and processAllZeros.  They are `process` with the
	 * input replaced by a constant, so the thing that separates them from
	 * each other and from `process` is a history that is NOT all zeros --
	 * mode 1 below is the vacuous fill, kept deliberately and outvoted by
	 * the other three.
	 */
	for (mode = 0; mode < 4; mode++) {
		for (trial = 0; trial < 8; trial++) {
			slot<ScramblerHH> sa, sb;
			unsigned int start = (unsigned)trial * 5u;
			long base = (long)(mode * 10000 + trial * 100);
			int k;

			for (k = 0; k < 2; k++) {
				unsigned int n = k ? NBULK : 1u;

				fill(hh_a, hh_b, BUF, trial, mode);
				memset(hh_oa, 0x5a, NBULK);
				memset(hh_ob, 0x5a, NBULK);
				place(&sa.o, hh_a, start);
				place(&sb.o, hh_b, start);

				sa.o.processAllOnes(hh_oa, n);
				ref_shh_ones(&sb.o, hh_ob, n);

				cmp(&sa.o, &sb.o, hh_a, hh_b, BUF, base + k);
				diff_eq_int("allOnes destination (case %ld)",
					    memcmp(hh_oa, hh_ob, NBULK) == 0,
					    1, base + k);
				if (memcmp(hh_oa, hh_ob, NBULK) == 0 &&
				    hh_oa[0] != 0x5a)
					ones_moved = 1;

				fill(hh_a, hh_b, BUF, trial, mode);
				memset(hh_oa, 0x5a, NBULK);
				memset(hh_ob, 0x5a, NBULK);
				place(&sa.o, hh_a, start);
				place(&sb.o, hh_b, start);

				sa.o.processAllZeros(hh_oa, n);
				ref_shh_zeros(&sb.o, hh_ob, n);

				cmp(&sa.o, &sb.o, hh_a, hh_b, BUF,
				    base + 50 + k);
				diff_eq_int("allZeros destination (case %ld)",
					    memcmp(hh_oa, hh_ob, NBULK) == 0,
					    1, base + 50 + k);
				if (memcmp(hh_oa, hh_ob, NBULK) == 0 &&
				    hh_oa[0] != 0x5a)
					zeros_moved = 1;
			}
		}
	}
	members_driven += 2;

	/*
	 * The two constants really are different.  From the SAME history,
	 * `processAllOnes` and `processAllZeros` must disagree in their first
	 * output -- they differ by exactly the XOR of 1 -- and both must
	 * disagree with `process` of anything whose low bit is the other one.
	 * Without this, two functions that both computed `*pTap1 ^ *pTap2`
	 * would pass every comparison above.
	 */
	{
		slot<ScramblerHH> s1, s0, sp;
		unsigned char o1[4], o0[4];
		int r;

		fill(hh_a, hh_b, BUF, 3, 0);
		place(&s1.o, hh_a, OUT);
		place(&s0.o, hh_a, OUT);
		place(&sp.o, hh_a, OUT);
		s1.o.processAllOnes(o1, 1u);
		fill(hh_a, hh_b, BUF, 3, 0);
		s0.o.processAllZeros(o0, 1u);
		fill(hh_a, hh_b, BUF, 3, 0);
		r = sp.o.process((unsigned char)0);

		diff_eq_int("allOnes and allZeros differ by one",
			    o1[0] ^ o0[0], 1, 0);
		diff_eq_int("allZeros is process(0)", o0[0], r, 0);
	}

	diff_eq_int("copyHistoryTail moved something", moved, 1, 0);
	diff_eq_int("reset saw an even argument", saw0, 1, 0);
	diff_eq_int("reset saw an odd argument", saw1, 1, 0);
	diff_eq_int("process crossed the restart", restarts > 0, 1, 0);
	diff_eq_int("process returned something nonzero", differed, 1, 0);
	diff_eq_int("the reference process never returned past a byte",
		    wide, 0, 0);
	diff_eq_int("processAllOnes wrote its destination", ones_moved, 1, 0);
	diff_eq_int("processAllZeros wrote its destination", zeros_moved, 1, 0);
	diff_eq_int("the constructor sweep had a distinct non-zero case",
		    distinct, 1, 0);

	return diff_end();
}

static unsigned char hi_a[BUF], hi_b[BUF];

static int
run_hi(void)
{
	int restarts = 0, differed = 0, wide = 0, moved = 0, distinct = 0;
	int saw0 = 0, saw1 = 0, i;
	static const unsigned ctor_args[6][3] = {
		{ CTOR_A, CTOR_B, CTOR_C }, { 0x12, 0x17, 0x63 },
		{ 1, 2, 3 }, { 0, 5, 8 }, { 4, 0, 6 }, { 2, 9, 0 }
	};

	diff_begin("Scrambler<unsigned char, int>");

	for (i = 0; i < 6; i++)
		drive_ctor_dtor<ScramblerHI, unsigned char>(
		    ref_shi_ctor, ref_shi_dtor, ctor_args[i][0],
		    ctor_args[i][1], ctor_args[i][2], i, &distinct);
	drive_dtor_null<ScramblerHI>(ref_shi_dtor);
	drive_rhi<ScramblerHI>(ref_shi_rhi, hi_a, hi_b);
	drive_cht<ScramblerHI>(ref_shi_cht, hi_a, hi_b, &moved);
	drive_reset<ScramblerHI>(ref_shi_reset, hi_a, hi_b, &saw0, &saw1);
	drive_proc1<ScramblerHI>(ref_shi_proc1, hi_a, hi_b, &restarts,
				 &differed, &wide, 0);

	diff_eq_int("copyHistoryTail moved something", moved, 1, 0);
	diff_eq_int("reset saw an even argument", saw0, 1, 0);
	diff_eq_int("reset saw an odd argument", saw1, 1, 0);
	diff_eq_int("process crossed the restart", restarts > 0, 1, 0);
	diff_eq_int("process returned something nonzero", differed, 1, 0);
	diff_eq_int("the reference process never returned past a byte",
		    wide, 0, 0);
	diff_eq_int("the constructor sweep had a distinct non-zero case",
		    distinct, 1, 0);

	return diff_end();
}

static int ih_a[BUF], ih_b[BUF], ih_in[NBULK];
static unsigned char ih_oa[NBULK], ih_ob[NBULK];

static int
run_ih(void)
{
	int restarts = 0, wide = 0, moved = 0, distinct = 0;
	int saw0 = 0, saw1 = 0, i;
	static const unsigned ctor_args[6][3] = {
		{ CTOR_A, CTOR_B, CTOR_C }, { 0x12, 0x17, 0x63 },
		{ 1, 2, 3 }, { 0, 5, 8 }, { 4, 0, 6 }, { 2, 9, 0 }
	};

	diff_begin("Scrambler<int, unsigned char>");

	for (i = 0; i < 6; i++)
		drive_ctor_dtor<ScramblerIH, int>(
		    ref_sih_ctor, ref_sih_dtor, ctor_args[i][0],
		    ctor_args[i][1], ctor_args[i][2], i, &distinct);
	drive_dtor_null<ScramblerIH>(ref_sih_dtor);
	drive_rhi<ScramblerIH>(ref_sih_rhi, ih_a, ih_b);
	drive_cht<ScramblerIH>(ref_sih_cht, ih_a, ih_b, &moved);
	drive_reset<ScramblerIH>(ref_sih_reset, ih_a, ih_b, &saw0, &saw1);
	drive_procn<ScramblerIH>(ref_sih_procn, ih_a, ih_b, ih_in, ih_oa,
				 ih_ob, &restarts, &wide);

	diff_eq_int("copyHistoryTail moved something", moved, 1, 0);
	diff_eq_int("reset saw an even argument", saw0, 1, 0);
	diff_eq_int("reset saw an odd argument", saw1, 1, 0);
	diff_eq_int("process crossed the restart", restarts > 0, 1, 0);
	diff_eq_int("the constructor sweep had a distinct non-zero case",
		    distinct, 1, 0);

	/*
	 * THE ASSERTION THIS WHOLE INSTANTIATION IS HERE FOR.  `I` is
	 * `unsigned char` and `T` is `int`, so the XOR is computed in eight
	 * bits and the history gets it zero-extended.  If the sweep never put
	 * a value above 0xff under a tap, a `T` intermediate would pass every
	 * comparison above and finding F870 would be untested.
	 */
	diff_eq_int("an operand above 0xff reached the XOR", wide, 1, 0);

	return diff_end();
}

static unsigned char dhi_a[BUF], dhi_b[BUF], dhi_in[NBULK];
static int dhi_oa[NBULK], dhi_ob[NBULK];

static int
run_dhi(void)
{
	int restarts = 0, differed = 0, wide = 0, moved = 0, distinct = 0;
	int saw0 = 0, saw1 = 0, i;
	int alias_restarts = 0, alias_differed = 0, alias_wide = 0;
	static const unsigned ctor_args[6][3] = {
		{ CTOR_A, CTOR_B, CTOR_C }, { 0x12, 0x17, 0x63 },
		{ 1, 2, 3 }, { 0, 5, 8 }, { 4, 0, 6 }, { 2, 9, 0 }
	};

	diff_begin("Descrambler<unsigned char, int>");

	for (i = 0; i < 6; i++)
		drive_ctor_dtor<DescramblerHI, unsigned char>(
		    ref_dhi_ctor, ref_dhi_dtor, ctor_args[i][0],
		    ctor_args[i][1], ctor_args[i][2], i, &distinct);
	drive_dtor_null<DescramblerHI>(ref_dhi_dtor);
	drive_rhi<DescramblerHI>(ref_dhi_rhi, dhi_a, dhi_b);
	drive_cht<DescramblerHI>(ref_dhi_cht, dhi_a, dhi_b, &moved);
	drive_reset<DescramblerHI>(ref_dhi_reset, dhi_a, dhi_b, &saw0, &saw1);
	drive_proc1<DescramblerHI>(ref_dhi_proc1, dhi_a, dhi_b, &restarts,
				   &differed, &wide, 0);
	/* ... and again with the near tap ON the cursor. */
	drive_proc1<DescramblerHI>(ref_dhi_proc1, dhi_a, dhi_b,
				   &alias_restarts, &alias_differed,
				   &alias_wide, 1);
	drive_procn<DescramblerHI>(ref_dhi_procn, dhi_a, dhi_b, dhi_in, dhi_oa,
				   dhi_ob, &restarts, &wide);

	diff_eq_int("copyHistoryTail moved something", moved, 1, 0);
	diff_eq_int("reset saw an even argument", saw0, 1, 0);
	diff_eq_int("reset saw an odd argument", saw1, 1, 0);
	diff_eq_int("process crossed the restart", restarts > 0, 1, 0);
	diff_eq_int("process returned something nonzero", differed, 1, 0);
	diff_eq_int("the aliased run crossed the restart too",
		    alias_restarts > 0, 1, 0);
	diff_eq_int("the reference process never returned past a byte",
		    wide, 0, 0);
	diff_eq_int("the constructor sweep had a distinct non-zero case",
		    distinct, 1, 0);

	return diff_end();
}

static int dii_a[BUF], dii_b[BUF];

static int
run_dii(void)
{
	int restarts = 0, differed = 0, wide = 0, moved = 0, distinct = 0;
	int saw0 = 0, saw1 = 0, i;
	int alias_restarts = 0, alias_differed = 0, alias_wide = 0;
	static const unsigned ctor_args[6][3] = {
		{ CTOR_A, CTOR_B, CTOR_C }, { 0x12, 0x17, 0x63 },
		{ 1, 2, 3 }, { 0, 5, 8 }, { 4, 0, 6 }, { 2, 9, 0 }
	};

	diff_begin("Descrambler<int, int>");

	for (i = 0; i < 6; i++)
		drive_ctor_dtor<DescramblerII, int>(
		    ref_dii_ctor, ref_dii_dtor, ctor_args[i][0],
		    ctor_args[i][1], ctor_args[i][2], i, &distinct);
	drive_dtor_null<DescramblerII>(ref_dii_dtor);
	drive_rhi<DescramblerII>(ref_dii_rhi, dii_a, dii_b);
	drive_cht<DescramblerII>(ref_dii_cht, dii_a, dii_b, &moved);
	drive_reset<DescramblerII>(ref_dii_reset, dii_a, dii_b, &saw0, &saw1);
	drive_proc1<DescramblerII>(ref_dii_proc1, dii_a, dii_b, &restarts,
				   &differed, &wide, 0);
	drive_proc1<DescramblerII>(ref_dii_proc1, dii_a, dii_b,
				   &alias_restarts, &alias_differed,
				   &alias_wide, 1);

	diff_eq_int("copyHistoryTail moved something", moved, 1, 0);
	diff_eq_int("reset saw an even argument", saw0, 1, 0);
	diff_eq_int("reset saw an odd argument", saw1, 1, 0);
	diff_eq_int("process crossed the restart", restarts > 0, 1, 0);
	diff_eq_int("process returned something nonzero", differed, 1, 0);
	diff_eq_int("the aliased run crossed the restart too",
		    alias_restarts > 0, 1, 0);

	/*
	 * `T` is `int` here, so an operand above 0xff DOES reach the XOR and
	 * the result is a full word.  That is the mirror of `<i,h>`'s
	 * assertion: this one must see a wide return, that one must not.
	 */
	diff_eq_int("the reference process returned past a byte", wide, 1, 0);
	diff_eq_int("the constructor sweep had a distinct non-zero case",
		    distinct, 1, 0);

	return diff_end();
}

static int
run_count(void)
{
	diff_begin("every member of every instantiation was driven");
	diff_eq_int("members driven", members_driven, 34, 0);
	return diff_end();
}

int
main(void)
{
	int bad = 0;

	harness_alloc_reset();

	bad |= run_hh();
	bad |= run_hi();
	bad |= run_ih();
	bad |= run_dhi();
	bad |= run_dii();
	bad |= run_count();

	return bad;
}
