/*
 * t_fdspksil.c -- differential tests for src/service/silence.c: the silence
 * object's create/delete/query, and `_status` (blob 0xb02e0..0xb0415).
 *
 * `_status` is the only one with an observable output buffer, and it is
 * driven with guard bytes either side: it writes exactly two bytes and adds
 * exactly two to the length, and an off-by-one either way lands on a guard.
 * The code is a `char`, so the sweep includes values that are negative in
 * that type -- the object sign-extends it for the debug line and stores the
 * low byte, and the two would agree over 0..127 alone.
 *
 * silence_is_more_then is a float truncation feeding a signed comparison, so
 * the sweep is placed AROUND the boundary rather than across a range: for
 * each count, the ms values that put 10*ms just under, exactly on and just
 * over it.
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
extern int ref_silence_is_more_then(struct silence *s, float ms);
extern struct silence *ref_silence_create(struct silence *s, int a, int b);
extern void ref_silence_delete(struct silence *s);

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
					     0x41 };
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
	 * SECTION 2 -- silence_is_more_then.  For each count, the ms values
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
			float mss[12];
			unsigned int nms = 0;
			float base = (float)counts[ci] * 0.1f;

			mss[nms++] = base;
			mss[nms++] = base - 0.05f;
			mss[nms++] = base + 0.05f;
			for (i = 0; i < sizeof(extra) / sizeof(extra[0]); i++)
				mss[nms++] = extra[i];

			for (vi = 0; vi < nms; vi++) {
				struct silence sa, sb;
				int ra, rb;
				long tag = (long)(ci * 100 + vi);

				memset(&sa, 0x5c, sizeof(sa));
				sa.count = (unsigned short)counts[ci];
				memcpy(&sb, &sa, sizeof(sa));
				ra = ref_silence_is_more_then(&sa, mss[vi]);
				rb = silence_is_more_then(&sb, mss[vi]);
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
		static const int pairs[][2] = {
			{ 0, 0 }, { 1, -1 }, { 0x7fffffff, -0x7fffffff },
			{ 12345, 678 }
		};
		static const unsigned int npair = sizeof(pairs)
						  / sizeof(pairs[0]);

		for (ci = 0; ci < npair; ci++) {
			struct silence sa, sb;
			struct silence *ra, *rb;

			memset(&sa, 0x3c, sizeof(sa));
			memcpy(&sb, &sa, sizeof(sa));
			ra = ref_silence_create(&sa, pairs[ci][0],
						pairs[ci][1]);
			rb = silence_create(&sb, pairs[ci][0], pairs[ci][1]);
			diff_eq_int("create returns arg %ld",
				    rb == &sb, ra == &sa, (long)ci);
			diff_eq_obj("silence_create", struct silence,
				    &sb, &sa, (long)ci);
			diff_eq_int("create int_00 %ld", sb.int_00,
				    pairs[ci][0], (long)ci);
			diff_eq_int("create int_04 %ld", sb.int_04,
				    pairs[ci][1], (long)ci);
			diff_eq_int("create count %ld", sb.count, 0,
				    (long)ci);
			diff_eq_int("create int_10 %ld", sb.int_10, 0,
				    (long)ci);
			/* the hole at +0x0e keeps the fill on both sides */
			diff_eq_int("create pad kept %ld", sb.pad_0e[0], 0x3c,
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
		sa = ref_silence_create(0, 11, 22);
		la = harness_alloc;
		copy_a = *sa;
		ref_silence_delete(sa);
		lad = harness_alloc;

		harness_alloc_reset();
		sb = silence_create(0, 11, 22);
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

	return failed;
}
