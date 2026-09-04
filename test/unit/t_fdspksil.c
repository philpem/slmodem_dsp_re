/*
 * t_fdspksil.c -- differential tests for src/service/silence.c: the voice
 * path's silence detector and `_status` (blob 0xb02e0..0xb068f).
 *
 * `_status` is driven with guard bytes either side of its two: it writes
 * exactly two and adds exactly two to the length, and an off-by-one either
 * way lands on a guard.  The code is a `char`, so the sweep includes values
 * that are negative in that type -- the object sign-extends it for the debug
 * line and stores the low byte, and the two would agree over 0..127 alone.
 *
 * silence_is_more_then is a float truncation feeding a signed comparison, so
 * the sweep is placed AROUND the boundary rather than across a range.
 *
 * silence_progress asks its owner for two settings through a stored callback.
 * BOTH SIDES SHARE ONE CALLBACK AND ONE CONTEXT, which is safe because the
 * context is read-only: the only thing the callback records is a global call
 * count, and the test compares the number of calls each side made rather
 * than reading it as state.  That keeps the two `struct silence` objects bit
 * identical, so the whole struct goes through diff_eq_obj -- function
 * pointer, context pointer and all.
 *
 * THE SETTINGS SWEEP STOPS AT 0x7fffffff ON PURPOSE.  The object converts
 * the TIME answer through the x87 as an unsigned doubleword and truncates it
 * back to `int`, so an answer of 2^31 or more is an out-of-range double-to-int
 * conversion -- undefined in C, `0x80000000` on the x87, and something GCC 14
 * folds away entirely (finding F8755).  Driving it would be measuring the two
 * compilers' treatment of undefined behaviour, not the reconstruction.
 *
 * silence_create's out-of-memory arm is NOT driven: the harness allocator
 * does not fail on request, so `if (s == 0) return 0` after the malloc is
 * unreached here.  It is read from the disassembly (`xor %eax,%eax; test
 * %edx,%edx; jne`) and recorded rather than tested.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/silence.h"
#include "dsplib/debug.h"
#include "dsplib/sysdep.h"

extern unsigned int ref_dsplibs_debug_level;

extern void ref__status(unsigned char *out, unsigned short *len, char code);
extern int ref_silence_is_more_then(struct silence *s, float t);
extern struct silence *ref_silence_create(struct silence *s, void *obj,
					  unsigned int (*q)(void *, int));
extern void ref_silence_delete(struct silence *s);
extern void ref_silence_progress(struct silence *s, float *buf, short n,
				 unsigned char *out, unsigned short *len);

/* Per-side debug transcripts; see test/harness/runtime.c. */
extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
unsigned dsplib_debug_capture_lines(int side);
const char *dsplib_debug_capture_text(int side);

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/*
 * The owner the detector queries.  Read-only, so one instance serves both
 * sides; `q_calls` is apparatus, not state the detector can see.
 */
struct owner {
	unsigned int level;	/* answer to SILENCE_PARAM_LEVEL */
	unsigned int time;	/* answer to SILENCE_PARAM_TIME  */
	unsigned int bad;	/* answer to anything else       */
};

static int q_calls;
static int q_other;		/* queries for neither known parameter */

static unsigned int
owner_query(void *obj, int what)
{
	struct owner *o = (struct owner *)obj;

	q_calls++;
	if (what == SILENCE_PARAM_LEVEL)
		return o->level;
	if (what == SILENCE_PARAM_TIME)
		return o->time;
	q_other++;
	return o->bad;
}

static unsigned int lcg_state = 0x51c0u;
static unsigned int
lcg(void)
{
	lcg_state = lcg_state * 1664525u + 1013904223u;
	return lcg_state;
}

#define NSAMP 1700
static float bufa[NSAMP], bufb[NSAMP];

int
main(void)
{
	int failed = 0;
	unsigned int i, ci, li, vi;

	/*
	 * SECTION 1 -- _status.  Two bytes at *out, two on *len, one debug
	 * line above level 1.  The buffer is 16 bytes of 0xbb and only two
	 * of them may move.
	 */
	diff_begin("_status");
	{
		static const int codes[] = { 0, 1, 2, 0x10, 0x7f, -1, -128,
					     0x41, 0x71, 0x73 };
		static const unsigned short lens[] = { 0, 1, 7, 0xfffe };
		static const unsigned int ncode = sizeof(codes)
						  / sizeof(codes[0]);
		static const unsigned int nlen = sizeof(lens)
						 / sizeof(lens[0]);
		unsigned int lx;
		int lines_seen_off = 0, lines_seen_on = 0;

		for (li = 0; li <= 3; li++)
		for (ci = 0; ci < ncode; ci++)
		for (lx = 0; lx < nlen; lx++) {
			unsigned char oa[16], ob[16];
			unsigned short la, lb;
			long tag = (long)((li * 100 + ci) * 10 + lx);

			memset(oa, 0xbb, sizeof(oa));
			memcpy(ob, oa, sizeof(oa));
			la = lens[lx];
			lb = la;
			set_level(li);
			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			ref__status(oa + 4, &la, (char)codes[ci]);
			_status(ob + 4, &lb, (char)codes[ci]);
			dsplib_debug_capture_on = 0;

			diff_eq_int("_status len %ld", lb, la, tag);
			for (i = 0; i < 16; i++)
				diff_eq_int("_status out[%ld]", ob[i], oa[i],
					    tag * 100 + i);
			diff_eq_int("_status lines %ld",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), tag);
			diff_eq_int("_status text %ld",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)),
				    0, tag);
			/* it wrote two bytes and only two */
			diff_eq_int("_status dle %ld", ob[4], 0x10, tag);
			diff_eq_int("_status code %ld", ob[5],
				    (unsigned char)codes[ci], tag);
			diff_eq_int("_status guard lo %ld", ob[3], 0xbb, tag);
			diff_eq_int("_status guard hi %ld", ob[6], 0xbb, tag);
			diff_eq_int("_status advance %ld",
				    (unsigned short)(lb - lens[lx]), 2, tag);
			if (dsplib_debug_capture_lines(0) == 0)
				lines_seen_off = 1;
			else
				lines_seen_on = 1;
		}
		set_level(0);
		/* F134: both sides of the debug gate were actually taken */
		diff_eq_int("_status quiet arm ran", lines_seen_off, 1, 0);
		diff_eq_int("_status printing arm ran", lines_seen_on, 1, 0);
	}
	failed |= diff_end();

	/*
	 * SECTION 2 -- silence_is_more_then.  For each count, the values
	 * placed either side of and exactly on the boundary, plus negatives,
	 * zero and a magnitude far past anything the count can reach.
	 */
	diff_begin("silence_is_more_then");
	{
		static const int counts[] = { 0, 1, 2, 9, 10, 11, 100, 999,
					      32767, 65535 };
		static const unsigned int ncount = sizeof(counts)
						   / sizeof(counts[0]);
		int seen_true = 0, seen_false = 0;

		for (ci = 0; ci < ncount; ci++) {
			static const float extra[] = { -1.0f, -0.05f, 0.0f,
						       0.049f, 1e9f, -1e9f };
			float ts[12];
			unsigned int nts = 0;
			float base = (float)counts[ci] * 0.1f;

			ts[nts++] = base;
			ts[nts++] = base - 0.05f;
			ts[nts++] = base + 0.05f;
			for (i = 0; i < sizeof(extra) / sizeof(extra[0]); i++)
				ts[nts++] = extra[i];

			for (vi = 0; vi < nts; vi++) {
				struct silence sa, sb;
				int ra, rb;
				long tag = (long)(ci * 100 + vi);

				memset(&sa, 0x5c, sizeof(sa));
				sa.count = (unsigned short)counts[ci];
				memcpy(&sb, &sa, sizeof(sa));
				ra = ref_silence_is_more_then(&sa, ts[vi]);
				rb = silence_is_more_then(&sb, ts[vi]);
				diff_eq_int("more_then %ld", rb, ra, tag);
				diff_eq_obj("more_then reads only",
					    struct silence, &sb, &sa, tag);
				if (ra)
					seen_true = 1;
				else
					seen_false = 1;
			}
		}
		diff_eq_int("more_then true seen", seen_true, 1, 0);
		diff_eq_int("more_then false seen", seen_false, 1, 0);
	}
	failed |= diff_end();

	/*
	 * SECTION 3 -- silence_create into caller storage.  The object
	 * starts full of a fill, so every field it is supposed to write is
	 * visibly written and the two bytes at +0x0e are visibly NOT.
	 */
	diff_begin("silence_create supplied");
	{
		static struct owner owners[3];

		for (ci = 0; ci < 3; ci++) {
			struct silence sa, sb;
			struct silence *ra, *rb;
			void *obj = ci == 2 ? (void *)0 : &owners[ci];

			memset(&sa, 0x3c, sizeof(sa));
			memcpy(&sb, &sa, sizeof(sa));
			ra = ref_silence_create(&sa, obj, owner_query);
			rb = silence_create(&sb, obj, owner_query);
			diff_eq_int("create returns arg %ld",
				    rb == &sb, ra == &sa, (long)ci);
			diff_eq_obj("silence_create", struct silence,
				    &sb, &sa, (long)ci);
			diff_eq_int("create obj %ld", sb.obj == obj, 1,
				    (long)ci);
			diff_eq_int("create query %ld",
				    sb.query == owner_query, 1, (long)ci);
			diff_eq_int("create count %ld", sb.count, 0,
				    (long)ci);
			diff_eq_int("create nsamp %ld", sb.nsamp, 0,
				    (long)ci);
			diff_eq_int("create saw_signal %ld", sb.saw_signal, 0,
				    (long)ci);
			diff_eq_int("create energy %ld", sb.energy == 0.0f, 1,
				    (long)ci);
			/*
			 * the hole at +0x0e keeps the fill on both sides.
			 * `pad_0e` was removed from struct silence as
			 * compiler-inserted alignment (finding F10145), so
			 * this reads the same physical byte by offset.
			 */
			diff_eq_int("create pad kept %ld",
				    ((unsigned char *)&sb)[0x0e], 0x3c,
				    (long)ci);
		}
	}
	failed |= diff_end();

	/*
	 * SECTION 4 -- silence_create(NULL) and silence_delete, compared on
	 * the allocator's books.
	 */
	diff_begin("silence_create allocating");
	{
		static struct owner owner;
		struct alloc_log la, lb, lad, lbd;
		struct silence *sa, *sb;
		struct silence copy_a;

		/*
		 * The books are read after create and again after delete
		 * WITHOUT a reset between them: resetting drops the slot the
		 * allocator recorded, so the free that follows would be
		 * counted as a wild pointer on both sides and agree while
		 * measuring nothing.
		 */
		harness_alloc_reset();
		sa = ref_silence_create(0, &owner, owner_query);
		la = harness_alloc;
		copy_a = *sa;
		ref_silence_delete(sa);
		lad = harness_alloc;

		harness_alloc_reset();
		sb = silence_create(0, &owner, owner_query);
		lb = harness_alloc;
		diff_eq_int("alloc allocs", lb.allocs, la.allocs, 0);
		diff_eq_int("alloc bytes", (int)lb.bytes, (int)la.bytes, 0);
		diff_eq_int("alloc one block", lb.allocs, 1, 0);
		diff_eq_int("alloc sizeof", (int)lb.bytes,
			    (int)sizeof(struct silence), 0);
		diff_eq_int("alloc not null", sb != 0, sa != 0, 0);
		diff_eq_obj("silence_create allocating", struct silence,
			    sb, &copy_a, 0);
		silence_delete(sb);
		lbd = harness_alloc;

		diff_eq_int("delete frees", lbd.frees, lad.frees, 0);
		diff_eq_int("delete bad_free", lbd.bad_free, lad.bad_free, 0);
		diff_eq_int("delete free_null", lbd.free_null, lad.free_null,
			    0);
		diff_eq_int("delete live", lbd.live, lad.live, 0);
		diff_eq_int("delete freed one", lbd.frees, 1, 0);
		diff_eq_int("delete balanced", lbd.live, 0, 0);

		/* NULL through delete: counted, not crashed, on both sides */
		harness_alloc_reset();
		ref_silence_delete(0);
		la = harness_alloc;
		harness_alloc_reset();
		silence_delete(0);
		lb = harness_alloc;
		diff_eq_int("delete null", lb.free_null, la.free_null, 0);
		diff_eq_int("delete null counted", lb.free_null, 1, 0);
		harness_alloc_reset();
	}
	failed |= diff_end();

	/*
	 * SECTION 5 -- silence_progress.
	 *
	 * The block is 800 samples, so a call of 1700 completes two of them
	 * and leaves 100 in the accumulator; calls are chained so the run
	 * counter, the accumulator and the block boundary all carry across.
	 * Amplitudes bracket the three thresholds (whose per-sample energies
	 * are 2.51e-6, 6.99e-6 and 2.26e-5), and the LEVEL setting is swept
	 * over the disabling values as well as the three live ones -- 0x10001
	 * checks that only the low 16 bits of the answer are read.
	 */
	diff_begin("silence_progress");
	{
		static const unsigned int levels[] = { 0, 1, 2, 3, 4, 0x10001,
						       0xffff0001u };
		static const unsigned int times[] = { 0, 1, 2, 7, 0x7fffffffu };
		static const float amps[] = { 0.0f, 0.0005f, 0.002f, 0.004f,
					      0.02f };
		static const short ns[] = { 1700, 800, 900, 0, -3 };
		static struct owner owner;
		unsigned int lx, tx, an;
		int seen_q = 0, seen_s = 0, seen_loud = 0, seen_disabled = 0;
		int seen_escape = 0;

		for (lx = 0; lx < sizeof(levels) / sizeof(levels[0]); lx++)
		for (tx = 0; tx < sizeof(times) / sizeof(times[0]); tx++)
		for (li = 0; li <= 3; li += 3) {
			struct silence sa, sb;
			unsigned int round;

			owner.level = levels[lx];
			owner.time = times[tx];
			owner.bad = 0xdeadbeef;
			memset(&sa, 0x77, sizeof(sa));
			memcpy(&sb, &sa, sizeof(sa));
			ref_silence_create(&sa, &owner, owner_query);
			silence_create(&sb, &owner, owner_query);
			set_level(li);

			for (round = 0; round < 8; round++) {
				unsigned char oa[32], ob[32];
				unsigned short la, lb;
				short bn = ns[round % 5];
				int ca, cb, qbase;
				long tag = (long)(((lx * 10 + tx) * 10
						   + li) * 10 + round);

				an = (round + lx) % 5;
				for (i = 0; i < NSAMP; i++) {
					float j = (float)(int)(lcg() % 2001)
						  - 1000.0f;
					bufa[i] = amps[an] * j * 0.001f;
				}
				memcpy(bufb, bufa, sizeof(bufa));
				memset(oa, 0xcd, sizeof(oa));
				memcpy(ob, oa, sizeof(oa));
				la = (unsigned short)(round * 3);
				lb = la;

				dsplib_debug_capture_reset();
				dsplib_debug_capture_on = 1;
				qbase = q_calls;
				ref_silence_progress(&sa, bufa, bn, oa, &la);
				ca = q_calls - qbase;
				qbase = q_calls;
				silence_progress(&sb, bufb, bn, ob, &lb);
				cb = q_calls - qbase;
				dsplib_debug_capture_on = 0;

				diff_eq_int("progress queries %ld", cb, ca,
					    tag);
				diff_eq_int("progress len %ld", lb, la, tag);
				for (i = 0; i < 32; i++)
					diff_eq_int("progress out[%ld]",
						    ob[i], oa[i],
						    tag * 100 + i);
				for (i = 0; i < NSAMP; i++)
					diff_eq_float("progress in[%ld]",
						      bufb[i], bufa[i],
						      tag * 10000 + i);
				diff_eq_obj("progress state", struct silence,
					    &sb, &sa, tag);
				diff_eq_int("progress lines %ld",
					    (int)dsplib_debug_capture_lines(0),
					    (int)dsplib_debug_capture_lines(1),
					    tag);
				diff_eq_int("progress text %ld",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1)),
					    0, tag);

				if (lb != la)
					continue;
				if (lb != (unsigned short)(round * 3)) {
					seen_escape = 1;
					if (ob[0] == 0x10 && ob[1] == 0x71)
						seen_q = 1;
					if (ob[0] == 0x10 && ob[1] == 0x73)
						seen_s = 1;
				}
				if (sb.saw_signal)
					seen_loud = 1;
				if (levels[lx] == 0 || (levels[lx] & 0xffff) > 3)
					seen_disabled = 1;
			}
		}
		set_level(0);
		/*
		 * F134: every arm the sweep is meant to reach is asserted
		 * FROM THE RUN, not from the shape of the loops.
		 */
		diff_eq_int("an escape was emitted", seen_escape, 1, 0);
		diff_eq_int("the q arm fired", seen_q, 1, 0);
		diff_eq_int("the s arm fired", seen_s, 1, 0);
		diff_eq_int("a loud block was seen", seen_loud, 1, 0);
		diff_eq_int("the disabled arm ran", seen_disabled, 1, 0);
		diff_eq_int("no query for an unknown parameter", q_other, 0,
			    0);
	}
	failed |= diff_end();

	return failed;
}
