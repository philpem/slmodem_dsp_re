/*
 * t_v34pcmif.c -- differential test of the V.90/K56Flex hooks into V.34.
 *
 * Every function here is a handful of stores into a 44 KB object, so the
 * comparison is the whole object byte for byte on both sides.  That is the
 * only way to catch the failure these functions can actually have: a store
 * that lands in the right place on one side and in a neighbouring pad on the
 * other.  Checking just the field the function is *about* would pass.
 *
 * Both objects are pre-filled with HARNESS_MALLOC_FILL rather than zeroed,
 * for the reason harness.h gives: zero is the one value that makes a field
 * nobody wrote look deliberate.
 *
 * THE INPUTS ARE SWEPT, NOT SAMPLED.  `V34XF_IndicateTrn2dReceived` is a
 * four-rung ratchet whose rungs are the boundaries 9/10, 14, 17 -- so the
 * sweep contains every boundary and both sides of it, and a reconstruction
 * that used `<` for `<=` anywhere would be caught rather than merely
 * probably caught.  `V34XF_GetRTD` gets the same treatment around its 16-bit
 * wrap.
 *
 * THE DEBUG TRANSCRIPTS ARE COMPARED TOO.  Four of these six functions print,
 * and finding 134 is the reason that matters: a dropped call site is
 * invisible to every test in this tree unless the transcript itself is the
 * thing being compared.  So each sweep runs a second time with both debug
 * levels raised and the two transcripts diffed.
 *
 * AND THREE ENTRY POINTS THAT REACH OUT OF THE OBJECT.  `VPcmV34InitiateHangUp`
 * and `VPcmV34InitiateRateRenegotiation` fork on `status`, and one arm walks
 * `p3548 -> +0x175c -> +0x20c -> +0x8c` into memory this object does not own.
 * Each side gets its OWN three-link chain, prefilled with a pattern, and the
 * chains are compared to each other afterwards -- so "wrote the right code
 * into the leaf" and "wrote nothing at all" are different results.  Comparing
 * the leaf alone would not do: `VPcmV34InitiateHangUp` also sets a flag byte
 * at `p3548 + 0x173e` and the renegotiation does not, and that byte is the
 * only difference between the two functions' PCM arms.
 *
 * THE STATE WORDS ARE SEEDED IN RANGE ON EVERY CASE THAT REACHES THE
 * HANDSHAKE.  `v34handshakinit` mode 2 runs three transitions, and each
 * indexes `StateName` with the value it finds -- nothing bounds the index
 * (D42), so a fill pattern there is an out-of-bounds read as soon as the
 * debug level is up.  All three are seeded to DIFFERENT in-range values for
 * the reason t_v34hshak.c gives: equal words make the three machines
 * indistinguishable.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34scram.h"
#include "dsplib/v34shell.h"

extern unsigned int ref_dsplibs_debug_level;

extern void ref_VPcmV34LogTimingOffset(void *obj, short offset);
extern void ref_VPcmV34SetTxScale(void *obj);
extern void ref_VPcmV34ReportStartOfEchoAdapt(void *obj);
extern void ref_VPcmV34ReportMiddleOfEchoAdapt(void *obj);
extern double *ref_V34XF_GetProbeResultsPtr(void *obj);
extern int *ref_V34XF_GetInfo0BitsPtr(void *obj);
extern short ref_V34XF_GetRTD(void *obj);
extern void ref_V34XF_IndicateJdReceived(void *obj, unsigned char constel,
					 unsigned char silence_scr);
extern void ref_V34XF_IndicateDilReceived(void *obj, unsigned char constel);
extern void ref_V34XF_IndicateTrn2dReceived(void *obj);
extern void ref_V34XF_IndicateK56FlexRateDetermined(void *obj);

extern void ref_VPcmV34InitiateHangUp(void *obj);
extern void ref_VPcmV34InitiateRateRenegotiation(void *obj, int req);
extern void ref_VPcmV34SetV90RateReneg(void *obj, short rrn_type,
				       unsigned char constel_size);

extern short ref_scrambleGPC(void *obj, short n);
extern short ref_scrambleGPA(void *obj, short n);
extern int ref_descrambleGPC(void *obj, unsigned short b, unsigned short n);
extern int ref_descrambleGPA(void *obj, unsigned short b, unsigned short n);
extern const short ref_Convolve16[64];

/*
 * Ours is the mapped struct, the blob's is raw bytes of the same length.
 * Statics, not locals: 44 KB each and two of them would be a large stack
 * frame on a 32-bit target.
 */
static struct v34_object oa;
static unsigned char ob[sizeof(struct v34_object)];

/*
 * The pointer-sized fields the two Initiate entry points and
 * `VPcmV34SetV90RateReneg` leave holding different addresses on the two
 * sides: the session pointer this file seeds per side, and the four
 * `preinitdigital` installs in the two shell contexts.  Each is a hole in the
 * byte comparison, so each is checked by what it selects instead --
 * `check_session_chain` and `check_shell_ptrs` below.
 *
 * `compare()` IS SHARED WITH THE SIX SWEEPS ABOVE, which predate this list
 * and lost no coverage to it: none of the four indications, the two handouts,
 * `GetRTD`, `SetTxScale` or `LogTimingOffset` writes any of these five
 * offsets, so the holes are holes in bytes those functions never touch.
 */
static const unsigned ptr_skip[] = {
	0x3548,					/* the session object       */
	0x0a28, 0x0e48,				/* receive shell context    */
	0x0a28 + V34_SHELL_TX, 0x0e48 + V34_SHELL_TX	/* transmit         */
};
#define NPTR (sizeof(ptr_skip) / sizeof(ptr_skip[0]))

static int saw_ptr_written[NPTR];

static int
skipped(unsigned off)
{
	unsigned k;

	for (k = 0; k < NPTR; k++)
		if (off >= ptr_skip[k] && off < ptr_skip[k] + 4)
			return 1;
	return 0;
}

/*
 * The per-side dummies every skipped pointer starts out holding.  Two reasons,
 * and the second is the one that was learned the hard way: it makes "this arm
 * left the field alone" visible, AND it means the content checks below read a
 * buffer rather than a fill pattern.  A fixture that dereferences an unseeded
 * pointer CRASHES on the first case that diverges instead of reporting it,
 * which is a defect in the test and not a diagnosis -- `run_setupreceiver` had
 * exactly this and it hid a real one.
 *
 * Long enough for the 64-short convolution table the checks read through them.
 */
#define DUMMY_LEN 128
static short dummy_a[DUMMY_LEN], dummy_b[DUMMY_LEN];

static void
setup(void)
{
	void *pa = (void *)dummy_a;
	void *pb = (void *)dummy_b;
	unsigned k;

	memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
	memset(ob, HARNESS_MALLOC_FILL, sizeof(ob));
	for (k = 0; k < DUMMY_LEN; k++)
		dummy_a[k] = dummy_b[k] = (short)(0x4b00 + k);
	for (k = 0; k < NPTR; k++) {
		memcpy((unsigned char *)&oa + ptr_skip[k], &pa, sizeof(pa));
		memcpy(ob + ptr_skip[k], &pb, sizeof(pb));
	}
}

/*
 * Compare the two objects in full.  `tag` identifies the case; the byte
 * index is folded in so a failure report names the offset that differs,
 * which for a struct that is mostly padding is the whole diagnosis.
 */
static void
compare(const char *what, long tag)
{
	const unsigned char *p = (const unsigned char *)&oa;
	void *seed = (void *)dummy_a;
	unsigned i, k;
	int bad = 0;

	/*
	 * A skipped field has to earn its hole, and with the seeding above the
	 * old test -- "the two sides differ" -- is true by construction and
	 * proves nothing.  The one that still means something is that
	 * SOMETHING wrote it: it no longer holds the dummy.
	 */
	for (k = 0; k < NPTR; k++)
		if (memcmp(p + ptr_skip[k], &seed, 4) != 0)
			saw_ptr_written[k] = 1;

	for (i = 0; i < sizeof(oa); i++) {
		if (p[i] == ob[i] || skipped(i))
			continue;
		bad++;
		if (bad <= 8)
			diff_eq_int(what, p[i], ob[i],
				    (long)i * 1000 + tag);
	}

	/*
	 * The loop above only reports differences, so an all-equal run
	 * records no check at all.  Count one, or a comparison that silently
	 * stopped comparing would look like a pass.
	 */
	diff_eq_int(what, bad, 0, tag);
}

/* Set the same field on both sides without going through a named accessor. */
static void
poke_int(unsigned off, int v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

static void
poke_short(unsigned off, short v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

/*
 * The v90_receiver values the TRN2d ratchet is swept over: every rung, every
 * boundary, and both extremes of the signed range because the object's
 * comparisons are signed and a reconstruction using unsigned ones would
 * differ only there.
 */
static const int trn2d_in[] = {
	(-0x7fffffff - 1), -1000, -1, 0, 1, 8, 9, 10, 11, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 100, 0x7fffffff
};

/* And the RTD values, chosen around the +480 wrap. */
static const unsigned short rtd_in[] = {
	0x0000, 0x0001, 0x000f, 0x7fff, 0x8000, 0xfe1f, 0xfe20, 0xfe21,
	0xff00, 0xfffe, 0xffff
};

static void
sweep(int with_debug)
{
	unsigned i;
	int c, s;
	long base = with_debug ? 500000L : 0L;

	/* --- Jd: two carried bits, four combinations, all of them --- */
	for (c = 0; c <= 1; c++)
	for (s = 0; s <= 1; s++) {
		setup();
		if (with_debug)
			dsplib_debug_capture_reset();
		V34XF_IndicateJdReceived(&oa, (unsigned char)c,
					 (unsigned char)s);
		ref_V34XF_IndicateJdReceived(ob, (unsigned char)c,
					     (unsigned char)s);
		compare("IndicateJdReceived", base + 100 + c * 2 + s);
		if (with_debug)
			diff_eq_int("IndicateJdReceived transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, base + 100 + c * 2 + s);
	}

	/*
	 * A non-boolean `constel`, which the object tests with `cmpb $0` and
	 * so treats as any-non-zero.  0x80 in particular: a reconstruction
	 * that took the parameter as `signed char` and compared `> 0` would
	 * pass every case above and fail this one.
	 */
	for (i = 0; i < 4; i++) {
		static const unsigned char odd[] = { 2, 0x7f, 0x80, 0xff };

		setup();
		if (with_debug)
			dsplib_debug_capture_reset();
		V34XF_IndicateJdReceived(&oa, odd[i], 0);
		ref_V34XF_IndicateJdReceived(ob, odd[i], 0);
		compare("IndicateJdReceived, odd constel", base + 120 + i);
		if (with_debug)
			diff_eq_int("IndicateJdReceived odd transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, base + 120 + i);

		setup();
		if (with_debug)
			dsplib_debug_capture_reset();
		V34XF_IndicateJdReceived(&oa, 0, odd[i]);
		ref_V34XF_IndicateJdReceived(ob, 0, odd[i]);
		compare("IndicateJdReceived, odd silence", base + 130 + i);
		if (with_debug)
			diff_eq_int("IndicateJdReceived odd silence "
				    "transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, base + 130 + i);
	}

	/* --- DIL --- */
	for (i = 0; i < 6; i++) {
		static const unsigned char dil[] = { 0, 1, 2, 0x7f, 0x80,
						     0xff };

		setup();
		if (with_debug)
			dsplib_debug_capture_reset();
		V34XF_IndicateDilReceived(&oa, dil[i]);
		ref_V34XF_IndicateDilReceived(ob, dil[i]);
		compare("IndicateDilReceived", base + 200 + i);
		if (with_debug)
			diff_eq_int("IndicateDilReceived transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, base + 200 + i);
	}

	/* --- TRN2d, the ratchet --- */
	for (i = 0; i < sizeof(trn2d_in) / sizeof(trn2d_in[0]); i++) {
		setup();
		poke_int(0x24c, trn2d_in[i]);
		if (with_debug)
			dsplib_debug_capture_reset();
		V34XF_IndicateTrn2dReceived(&oa);
		ref_V34XF_IndicateTrn2dReceived(ob);
		compare("IndicateTrn2dReceived", base + 300 + i);
		if (with_debug)
			diff_eq_int("IndicateTrn2dReceived transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, base + 300 + i);
	}

	/*
	 * Applied twice: the ratchet's whole point is that a second call from
	 * the value the first produced must not move it, and calling once
	 * cannot show that.
	 */
	for (i = 0; i < sizeof(trn2d_in) / sizeof(trn2d_in[0]); i++) {
		setup();
		poke_int(0x24c, trn2d_in[i]);
		V34XF_IndicateTrn2dReceived(&oa);
		ref_V34XF_IndicateTrn2dReceived(ob);
		V34XF_IndicateTrn2dReceived(&oa);
		ref_V34XF_IndicateTrn2dReceived(ob);
		compare("IndicateTrn2dReceived twice", base + 350 + i);
	}

	/* --- K56Flex --- */
	setup();
	if (with_debug)
		dsplib_debug_capture_reset();
	V34XF_IndicateK56FlexRateDetermined(&oa);
	ref_V34XF_IndicateK56FlexRateDetermined(ob);
	compare("IndicateK56FlexRateDetermined", base + 400);
	if (with_debug)
		diff_eq_int("IndicateK56FlexRateDetermined transcript",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0,
			    1, base + 400);
}

/* --- the three request entry points --------------------------------------- */

/*
 * The session chain, per side.  Lengths are one page past the deepest offset
 * each link is indexed at, so an out-of-bounds store lands inside the buffer
 * and shows up in the comparison rather than corrupting something else.
 */
#define SESS_LEN	0x1800		/* indexed at +0x173e and +0x175c */
#define DEMOD_LEN	0x0240		/* indexed at +0x20c              */
#define LEAF_LEN	0x00c0		/* indexed at +0x8c               */

#define SESS_PTR	0x175c
#define SESS_FLAG	0x173e
#define DEMOD_PTR	0x020c
#define LEAF_REQ	0x008c

#define CHAIN_FILL	0x3c

static unsigned char sess_a[SESS_LEN], sess_b[SESS_LEN];
static unsigned char demod_a[DEMOD_LEN], demod_b[DEMOD_LEN];
static unsigned char leaf_a[LEAF_LEN], leaf_b[LEAF_LEN];

static void
put_ptr(void *base, unsigned off, void *v)
{
	memcpy((unsigned char *)base + off, &v, sizeof(v));
}

static void
poke_ptr(unsigned off, void *pa, void *pb)
{
	memcpy((unsigned char *)&oa + off, &pa, sizeof(pa));
	memcpy(ob + off, &pb, sizeof(pb));
}

static void
seed_chain(void)
{
	memset(sess_a, CHAIN_FILL, sizeof(sess_a));
	memset(sess_b, CHAIN_FILL, sizeof(sess_b));
	memset(demod_a, CHAIN_FILL, sizeof(demod_a));
	memset(demod_b, CHAIN_FILL, sizeof(demod_b));
	memset(leaf_a, CHAIN_FILL, sizeof(leaf_a));
	memset(leaf_b, CHAIN_FILL, sizeof(leaf_b));

	put_ptr(sess_a, SESS_PTR, demod_a);
	put_ptr(sess_b, SESS_PTR, demod_b);
	put_ptr(demod_a, DEMOD_PTR, leaf_a);
	put_ptr(demod_b, DEMOD_PTR, leaf_b);

	poke_ptr(0x3548, sess_a, sess_b);
}

/*
 * Compare two buffers with one pointer-sized hole in each -- the links are
 * necessarily different addresses.  Everything else, including the flag byte
 * and the request word, is compared.
 */
static void
compare_link(const char *what, const unsigned char *a, const unsigned char *b,
	     unsigned len, unsigned hole, long tag)
{
	unsigned i;
	int bad = 0;

	for (i = 0; i < len; i++) {
		if (a[i] == b[i] || (hole != (unsigned)-1
				     && i >= hole && i < hole + 4))
			continue;
		bad++;
		if (bad <= 4)
			diff_eq_int(what, a[i], b[i], (long)i * 1000 + tag);
	}
	diff_eq_int(what, bad, 0, tag);
}

/*
 * The whole chain, plus the two things it is FOR.  `want_req` is the code the
 * leaf should be carrying and `want_flag` whether the session flag was set;
 * both are -1 when this arm was not taken, and then the chain must be exactly
 * as it was seeded -- which is the half that catches a fork gone the wrong
 * way.
 */
static int saw_chain_walked;

static void
check_session_chain(const char *what, int taken, int want_req, int want_flag,
		    long tag)
{
	compare_link(what, sess_a, sess_b, SESS_LEN, SESS_PTR, tag);
	compare_link(what, demod_a, demod_b, DEMOD_LEN, DEMOD_PTR, tag);
	compare_link(what, leaf_a, leaf_b, LEAF_LEN, (unsigned)-1, tag);

	if (taken) {
		int got;

		saw_chain_walked = 1;
		memcpy(&got, leaf_a + LEAF_REQ, sizeof(got));
		diff_eq_int(what, got, want_req, tag);
		diff_eq_int(what, sess_a[SESS_FLAG],
			    want_flag ? 1 : CHAIN_FILL, tag);
	} else {
		unsigned i;
		int touched = 0;

		for (i = 0; i < SESS_LEN; i++)
			if (sess_a[i] != CHAIN_FILL
			    && (i < SESS_PTR || i >= SESS_PTR + 4))
				touched = 1;
		for (i = 0; i < LEAF_LEN; i++)
			if (leaf_a[i] != CHAIN_FILL)
				touched = 1;
		diff_eq_int(what, touched, 0, tag);
	}
}

static void *
ptr_at(const void *base, unsigned off)
{
	void *p;

	memcpy(&p, (const unsigned char *)base + off, sizeof(p));
	return p;
}

/*
 * The four pointers `preinitdigital` installs, checked the way t_v34digital.c
 * checks them: by which function or table each side selected, which is the
 * thing `f359c` decides and the thing an address comparison cannot see.
 */
static void
check_shell_ptrs(short f359c, long tag)
{
	int orig = (f359c == 0x65);
	const short *ca = ptr_at(&oa, 0x0a28);
	const short *cb = ptr_at(ob, 0x0a28);
	int k, diffs = 0;

	diff_eq_int("tx scrambler",
		    ptr_at(&oa, 0x0e48 + V34_SHELL_TX)
		    == (void *)(orig ? scrambleGPC : scrambleGPA), 1, tag);
	diff_eq_int("ref tx scrambler",
		    ptr_at(ob, 0x0e48 + V34_SHELL_TX)
		    == (void *)(orig ? ref_scrambleGPC : ref_scrambleGPA),
		    1, tag);
	diff_eq_int("rx descrambler",
		    ptr_at(&oa, 0x0e48)
		    == (void *)(orig ? descrambleGPA : descrambleGPC), 1, tag);
	diff_eq_int("ref rx descrambler",
		    ptr_at(ob, 0x0e48)
		    == (void *)(orig ? ref_descrambleGPA : ref_descrambleGPC),
		    1, tag);

	for (k = 0; k < 64; k++)
		if (ca[k] != cb[k])
			diffs++;
	diff_eq_int("convolve table", diffs, 0, tag);
	diff_eq_int("and it is Convolve16",
		    ca == Convolve16 && cb == ref_Convolve16, 1, tag);
	diff_eq_int("and the transmit context has it too",
		    ptr_at(&oa, 0x0a28 + V34_SHELL_TX) == (void *)Convolve16
		    && ptr_at(ob, 0x0a28 + V34_SHELL_TX)
		       == (void *)ref_Convolve16, 1, tag);
}

/*
 * Every input the three functions read, one field per member.  Two inputs
 * driven from one variable cannot be told apart -- findings 116b, 123 and 171
 * -- so `rate_now`, `rate_min` and `rate_max` are separate even though the
 * interesting cases are relations between them, and the three state words are
 * separate from each other and from the two trace counters.
 */
struct req_case {
	int		status;
	int		rate_min;
	int		rate_max;
	int		rate_now;
	int		rate_want;
	short		rrn_local;
	short		rrn_remote;
	short		f359c;
	short		mst;		/* +0x3592 */
	short		rxst;		/* +0x3594 */
	short		txst;		/* +0x3596 */
	short		trace1;		/* +0x2aa2 */
	short		trace2;		/* +0xaa78 */
	unsigned short	txflags;	/* +0x25c2 */
	unsigned short	rxflags;	/* receiver +0x122 */
	int		v90_receiver;	/* +0x24c */
	short		f382;
};

/*
 * The default case: a V.34 connection, three DIFFERENT state values, two
 * DIFFERENT trace counters, and a rate index in the middle of its range so
 * that a step either way stores something.
 */
static const struct req_case req_base = {
	3,			/* status: not 1 or 2, so the V.34 arm      */
	4, 12, 8, 7,		/* rate min, max, now, want                 */
	0x0033, 0x0044,		/* rrn local, remote                        */
	0x65,			/* f359c                                    */
	V34HS_PHASE1,		/* mst  = 33                                */
	V34HS_PHASE2,		/* rxst = 34                                */
	V34HS_TONE_AB,		/* txst = 60                                */
	0x1111, 0x2222,		/* [1], [2]                                 */
	0x0000, 0x0000,		/* txflags, rxflags                         */
	6,			/* v90_receiver                             */
	0x1234			/* f382                                     */
};

static void
seed_object(const struct req_case *c)
{
	setup();
	seed_chain();

	poke_int(0x0000, c->status);
	poke_int(0x0220, c->rate_min);
	poke_int(0x0224, c->rate_max);
	poke_int(0x0228, c->rate_now);
	poke_int(0x022c, c->rate_want);
	poke_int(0x024c, c->v90_receiver);
	poke_short(0xac0e, c->rrn_local);
	poke_short(0xac10, c->rrn_remote);
	poke_short(0x359c, c->f359c);
	poke_short(0x3592, c->mst);
	poke_short(0x3594, c->rxst);
	poke_short(0x3596, c->txst);
	poke_short(0x2aa2, c->trace1);
	poke_short(0xaa78, c->trace2);
	poke_short(0x25c2, (short)c->txflags);
	poke_short(0x264 + 0x122, (short)c->rxflags);
	poke_short(0x0382, c->f382);
}

/* Did this case take the PCM arm? */
static int
pcm_arm(const struct req_case *c)
{
	return (unsigned)(c->status - 1) <= 1;
}

/*
 * What the V.34 arm should leave in `rate_want`, given the request code.
 *
 * The step is spelled unsigned here for the same reason it is in the source,
 * and the two extreme rows of `rate_in` are why.  With a plain `+ 1` this
 * function reported 41 where BOTH implementations produced INT_MAX: the
 * optimiser folded `rate_now + 1 <= rate_max` into `rate_now < rate_max` on
 * the strength of the overflow being undefined.
 *
 * So the failure those rows produced was the FIXTURE's, not the code's --
 * an expectation computed by different rules from the thing it was checking.
 * Worth stating, because an expectation helper that duplicates the logic it
 * is checking is only useful while the duplication is exact.
 */
static int
want_after(const struct req_case *c, int req)
{
	if (req == 0 || req == 2 || req == 5) {
		int down = (int)((unsigned)c->rate_now - 1u);

		return (down >= c->rate_min) ? down : c->rate_want;
	}
	if (req == 3) {
		int up = (int)((unsigned)c->rate_now + 1u);

		return (up <= c->rate_max) ? up : c->rate_want;
	}
	return -1;
}

static void
run_hangup(const struct req_case *c, long tag)
{
	seed_object(c);

	VPcmV34InitiateHangUp(&oa);
	ref_VPcmV34InitiateHangUp(ob);

	compare("InitiateHangUp", tag);
	check_session_chain("InitiateHangUp chain", pcm_arm(c), 2, 1, tag);
	if (!pcm_arm(c)) {
		check_shell_ptrs(c->f359c, tag);
		/*
		 * The three clears happen on both arms, and `rate_now` must
		 * survive -- seeded non-zero so that a reconstruction which
		 * cleared four words instead of three is visible.
		 */
		diff_eq_int("InitiateHangUp kept rate_now",
			    oa.rate_now, c->rate_now, tag);
		diff_eq_int("InitiateHangUp cleared rate_want",
			    oa.rate_want, 0, tag);
	}
}

static void
run_reneg(const struct req_case *c, int req, long tag)
{
	seed_object(c);

	VPcmV34InitiateRateRenegotiation(&oa, req);
	ref_VPcmV34InitiateRateRenegotiation(ob, req);

	compare("InitiateRateRenegotiation", tag);
	check_session_chain("InitiateRateRenegotiation chain", pcm_arm(c),
			    req, 0, tag);
	if (!pcm_arm(c)) {
		check_shell_ptrs(c->f359c, tag);
		diff_eq_int("InitiateRateRenegotiation rate_want",
			    oa.rate_want, want_after(c, req), tag);
		diff_eq_int("InitiateRateRenegotiation counted",
			    (int)oa.rrn_local,
			    (int)(short)(c->rrn_local + 1), tag);
		diff_eq_int("and left the remote counter",
			    (int)oa.rrn_remote, (int)c->rrn_remote, tag);
	}
}

static void
run_setv90(const struct req_case *c, short rrn_type, unsigned char constel,
	   long tag)
{
	seed_object(c);

	VPcmV34SetV90RateReneg(&oa, rrn_type, constel);
	ref_VPcmV34SetV90RateReneg(ob, rrn_type, constel);

	compare("SetV90RateReneg", tag);
	check_shell_ptrs(c->f359c, tag);
	/*
	 * The two polarities a plausible-but-wrong reconstruction gets
	 * backwards, asserted against the value rather than only against the
	 * blob: `rrn_type` is tested for ZERO and not for sign, and
	 * `constel_size` is read unsigned.
	 */
	diff_eq_int("SetV90RateReneg v90_receiver",
		    oa.v90_receiver, rrn_type != 0 ? 15 : 11, tag);
	diff_eq_int("SetV90RateReneg f382",
		    (int)(unsigned short)oa.f382,
		    constel != 0 ? 0x89b0 : 0x8990, tag);
	/* And it goes nowhere near the session object. */
	check_session_chain("SetV90RateReneg chain", 0, 0, 0, tag);
}

/*
 * One case with the transcripts captured and compared.  A mismatch otherwise
 * reports as "got 0, reference 1" and nothing else, which over a sweep this
 * size is not a diagnosis; `PCMIF_DUMP=1` prints both sides.
 */
static void
traced(void (*fn)(const struct req_case *, int, long),
       const struct req_case *c, int arg, long tag, const char *what)
{
	dsplib_debug_capture_reset();
	fn(c, arg, tag);
	if (getenv("PCMIF_DUMP")
	    && strcmp(dsplib_debug_capture_text(0),
		      dsplib_debug_capture_text(1)) != 0)
		fprintf(stderr, "--- %s case %ld\n=== ours\n%s=== ref\n%s",
			what, tag, dsplib_debug_capture_text(0),
			dsplib_debug_capture_text(1));
	diff_eq_int("transcript", strcmp(dsplib_debug_capture_text(0),
					 dsplib_debug_capture_text(1)) == 0,
		    1, tag);
	diff_eq_int("transcript non-empty",
		    dsplib_debug_capture_text(1)[0] != 0, 1, tag);
}

/* Adapters so the three can share `traced`. */
static void
hangup_thunk(const struct req_case *c, int unused, long tag)
{
	(void)unused;
	run_hangup(c, tag);
}

static void
reneg_thunk(const struct req_case *c, int req, long tag)
{
	run_reneg(c, req, tag);
}

/*
 * The status values.  0 and everything below it are the ones that separate
 * `(unsigned)(status - 1) <= 1` from a reconstruction written as `status <= 2`
 * -- both agree on 1, 2 and 3, and differ on 0 and on negatives.
 */
static const int status_in[] = {
	(-0x7fffffff - 1), -1, 0, 1, 2, 3, 10, 0x7fffffff
};

/*
 * The request codes.  1 and 4 are there because they fall to the `-1` default
 * between the values that do not, which is the easy thing to get wrong; 5 is
 * there because it shares a body with 0 and 2 and a reconstruction can carry
 * two of the three.
 */
static const int req_in[] = { -0x7fffffff - 1, -2, -1, 0, 1, 2, 3, 4, 5, 6 };

/*
 * The rate configurations, each a (min, max, now) triple placed at a boundary
 * of one of the two clamps.  The clamps are `jl` and `jg`, so it is the
 * boundary and its two neighbours that separate `<` from `<=`.
 */
static const int rate_in[][3] = {
	{  4, 12,  8 },		/* mid range: both steps store              */
	{  4, 12,  4 },		/* at the floor: down stores nothing        */
	{  4, 12,  5 },		/* one above it: down stores exactly min    */
	{  4, 12, 12 },		/* at the ceiling: up stores nothing        */
	{  4, 12, 11 },		/* one below it: up stores exactly max      */
	{  7,  7,  7 },		/* a range of one: neither step stores      */
	{  4, 12,  0 },		/* below the floor: down stores nothing     */
	{  4, 12, 20 },		/* above the ceiling: up stores nothing     */
	{ 12,  4,  8 },		/* inverted bounds: neither step stores     */
	{  0,  0,  0 },		/* all zero, where -1 and 0 are adjacent    */
	/*
	 * And the two ends of the signed range.  The object steps with `dec`
	 * and `inc`, which WRAP; C's `- 1` and `+ 1` on a signed int overflow
	 * instead, and an optimiser is entitled to assume they cannot -- so
	 * these two rows are the only place the reconstruction can legally
	 * diverge from the instruction, and without them it does so silently.
	 */
	{ (-0x7fffffff - 1), 0x7fffffff, 0x7fffffff },
	{ (-0x7fffffff - 1), 0x7fffffff, (-0x7fffffff - 1) }
};

int
main(void)
{
	unsigned i;
	int rc = 0;

	diff_begin("v34 pcm interface: the four phase-3 indications");
	sweep(0);
	rc |= diff_end();

	diff_begin("v34 pcm interface: the same, with the debug sites live");
	{
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		sweep(1);

		/*
		 * And something must actually have been printed -- otherwise
		 * every transcript comparison above is "" == "", which is
		 * exactly what a dropped call site looks like.
		 */
		diff_eq_int("transcript non-empty",
			    dsplib_debug_capture_text(1)[0] != 0, 1, 0);
		diff_eq_int("ours printed too",
			    dsplib_debug_capture_text(0)[0] != 0, 1, 0);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	diff_begin("v34 pcm interface: the two pointer handouts");
	{
		setup();
		diff_eq_int("GetProbeResultsPtr offset",
			    (char *)V34XF_GetProbeResultsPtr(&oa)
			    - (char *)&oa,
			    (char *)ref_V34XF_GetProbeResultsPtr(ob)
			    - (char *)ob, 0);
		diff_eq_int("GetInfo0BitsPtr offset",
			    (char *)V34XF_GetInfo0BitsPtr(&oa) - (char *)&oa,
			    (char *)ref_V34XF_GetInfo0BitsPtr(ob)
			    - (char *)ob, 0);
		/*
		 * Neither may write anything.  They are `return &field`, and
		 * a reconstruction that initialised the array on the way out
		 * would still hand back the right address.
		 */
		compare("the handouts store nothing", 0);
	}
	rc |= diff_end();

	diff_begin("v34 pcm interface: GetRTD and its 16-bit wrap");
	{
		int saw_negative = 0;

		for (i = 0; i < sizeof(rtd_in) / sizeof(rtd_in[0]); i++) {
			short got, want;

			setup();
			poke_short(0xaa7e, (short)rtd_in[i]);
			got = V34XF_GetRTD(&oa);
			want = ref_V34XF_GetRTD(ob);
			diff_eq_int("GetRTD", got, want, rtd_in[i]);
			compare("GetRTD stores nothing", 600 + i);
			if (want < 0)
				saw_negative = 1;
		}

		/*
		 * The sweep has to contain a case that wraps, or it is not
		 * testing the thing the note in v34pcmif.c is about.
		 */
		diff_eq_int("the sweep reached the wrap", saw_negative, 1, 0);
	}
	rc |= diff_end();

	diff_begin("v34 pcm interface: SetTxScale");
	{
		/*
		 * Its diagnostic goes through `edprintf`, which gates itself
		 * -- so unlike everything else here the call site is NOT
		 * behind a level test, and the transcript has to be compared
		 * with the level raised to see it at all.
		 */
		setup();
		VPcmV34SetTxScale(&oa);
		ref_VPcmV34SetTxScale(ob);
		compare("SetTxScale", 800);

		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		setup();
		VPcmV34SetTxScale(&oa);
		ref_VPcmV34SetTxScale(ob);
		compare("SetTxScale, logging", 801);
		diff_eq_int("SetTxScale transcript",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, 801);
		diff_eq_int("and it said something",
			    dsplib_debug_capture_text(1)[0] != 0, 1, 801);
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	/*
	 * The two echo-adapt reports.
	 *
	 * Nothing but the transcript: neither touches the object, so the byte
	 * compare passes on an empty body and says nothing on its own.  All
	 * three levels, and 1 is the one that matters -- every gate in the
	 * object is `> 1`, so 1 is the single value at which it differs from
	 * the `>= 1` a reader would write, and at 0 both spellings are
	 * silent.
	 *
	 * The two strings differ in one word, so comparing each function's
	 * transcript against the reference's catches a transposition.  The
	 * two `saw_` flags catch the case that comparison cannot: both sides
	 * silent, and the strcmp passing on two empty strings.
	 */
	diff_begin("v34 pcm interface: the two echo-adapt reports");
	{
		unsigned lvl;
		int saw_start = 0, saw_middle = 0;

		dsplib_debug_capture_on = 1;
		for (lvl = 0; lvl <= 2; lvl++) {
			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;

			setup();
			dsplib_debug_capture_reset();
			VPcmV34ReportStartOfEchoAdapt(&oa);
			ref_VPcmV34ReportStartOfEchoAdapt(ob);
			compare("ReportStartOfEchoAdapt", 850 + (long)lvl);
			diff_eq_int("start transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, 850 + (long)lvl);
			if (lvl < 2) {
				diff_eq_int("below the threshold, ours said "
					    "nothing",
					    dsplib_debug_capture_lines(0), 0,
					    850 + (long)lvl);
				diff_eq_int("below the threshold, nor did the "
					    "reference",
					    dsplib_debug_capture_lines(1), 0,
					    850 + (long)lvl);
			} else if (dsplib_debug_capture_lines(1) > 0) {
				saw_start = 1;
			}

			setup();
			dsplib_debug_capture_reset();
			VPcmV34ReportMiddleOfEchoAdapt(&oa);
			ref_VPcmV34ReportMiddleOfEchoAdapt(ob);
			compare("ReportMiddleOfEchoAdapt", 860 + (long)lvl);
			diff_eq_int("middle transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, 860 + (long)lvl);
			if (lvl < 2) {
				diff_eq_int("below the threshold, ours said "
					    "nothing",
					    dsplib_debug_capture_lines(0), 0,
					    860 + (long)lvl);
				diff_eq_int("below the threshold, nor did the "
					    "reference",
					    dsplib_debug_capture_lines(1), 0,
					    860 + (long)lvl);
			} else if (dsplib_debug_capture_lines(1) > 0) {
				saw_middle = 1;
			}
		}
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;

		diff_eq_int("the start report said something", saw_start, 1, 0);
		diff_eq_int("and so did the middle one", saw_middle, 1, 0);
	}
	rc |= diff_end();

	diff_begin("v34 pcm interface: LogTimingOffset");
	{
		static const short off[] = { 0, 1, -1, 0x7fff,
					     (short)0x8000, 1234 };

		for (i = 0; i < sizeof(off) / sizeof(off[0]); i++) {
			setup();
			VPcmV34LogTimingOffset(&oa, off[i]);
			ref_VPcmV34LogTimingOffset(ob, off[i]);
			compare("LogTimingOffset", 700 + i);
		}
	}
	rc |= diff_end();

	/* --- InitiateHangUp --------------------------------------------- */

	diff_begin("v34 pcm interface: InitiateHangUp, both arms of the fork");
	{
		unsigned s, r;

		for (s = 0; s < sizeof(status_in) / sizeof(status_in[0]); s++)
		for (r = 0; r < sizeof(rate_in) / sizeof(rate_in[0]); r++) {
			struct req_case c = req_base;

			c.status = status_in[s];
			c.rate_min = rate_in[r][0];
			c.rate_max = rate_in[r][1];
			c.rate_now = rate_in[r][2];
			run_hangup(&c, 1000 + (long)s * 100 + r);
		}
	}
	rc |= diff_end();

	/* --- InitiateRateRenegotiation ---------------------------------- */

	diff_begin("v34 pcm interface: InitiateRateRenegotiation, "
		   "every code against every boundary");
	{
		unsigned s, q, r;

		for (s = 0; s < sizeof(status_in) / sizeof(status_in[0]); s++)
		for (q = 0; q < sizeof(req_in) / sizeof(req_in[0]); q++)
		for (r = 0; r < sizeof(rate_in) / sizeof(rate_in[0]); r++) {
			struct req_case c = req_base;

			c.status = status_in[s];
			c.rate_min = rate_in[r][0];
			c.rate_max = rate_in[r][1];
			c.rate_now = rate_in[r][2];
			/*
			 * A DIFFERENT prior request each time, so the cases
			 * where the step stores nothing are distinguishable
			 * from the cases where it stores this value.
			 */
			c.rate_want = 30 + (int)r;
			run_reneg(&c, req_in[q],
				  2000 + (long)s * 1000 + (long)q * 100 + r);
		}
	}
	rc |= diff_end();

	/*
	 * And the counter's 16-bit wrap, which no rate configuration reaches:
	 * `rrn_local` is a short and the increment is done in 16 bits.
	 */
	diff_begin("v34 pcm interface: the local RRN counter wraps");
	{
		static const short pre[] = { 0, 1, -1, 0x7ffe, 0x7fff,
					     (short)0x8000, (short)0xffff };

		for (i = 0; i < sizeof(pre) / sizeof(pre[0]); i++) {
			struct req_case c = req_base;

			c.rrn_local = pre[i];
			run_reneg(&c, 3, 3000 + i);
		}
	}
	rc |= diff_end();

	/* --- SetV90RateReneg -------------------------------------------- */

	diff_begin("v34 pcm interface: SetV90RateReneg, both zero tests");
	{
		static const short rrn[] = { 0, 1, -1, 2, 0x7fff,
					     (short)0x8000 };
		static const unsigned char cst[] = { 0, 1, 2, 0x7f, 0x80,
						     0xff };
		static const short which[] = { 0x65, 0x66, 0, 0x64 };
		unsigned a, b, w;

		for (w = 0; w < sizeof(which) / sizeof(which[0]); w++)
		for (a = 0; a < sizeof(rrn) / sizeof(rrn[0]); a++)
		for (b = 0; b < sizeof(cst) / sizeof(cst[0]); b++) {
			struct req_case c = req_base;

			c.f359c = which[w];
			/*
			 * A different starting `txflags` per case: the mask
			 * is `& ~0x4018 | 0x2000`, and a case whose flags
			 * start at zero cannot show a bit being cleared.
			 */
			c.txflags = (unsigned short)(0xffff
						     ^ (unsigned short)
						       (a * 0x111 + b));
			c.v90_receiver = 3 + (int)a;
			run_setv90(&c, rrn[a], cst[b],
				   4000 + (long)w * 100 + (long)a * 10 + b);
		}
	}
	rc |= diff_end();

	/* --- the same three, with the diagnostics live ------------------- */

	diff_begin("v34 pcm interface: the three requests, transcripts too");
	{
		unsigned s, q;

		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		for (s = 0; s < sizeof(status_in) / sizeof(status_in[0]); s++) {
			struct req_case c = req_base;

			c.status = status_in[s];
			traced(hangup_thunk, &c, 0, 5000 + (long)s,
			       "InitiateHangUp");
		}

		/*
		 * The renegotiation prints nothing of its own; what it has to
		 * say comes from `v34handshakinit`'s three transitions, and
		 * those index `StateName`.  So the state words are driven a
		 * third of the table apart here as well as together -- the
		 * two machines' argument slots are otherwise interchangeable.
		 */
		for (q = 0; q < sizeof(req_in) / sizeof(req_in[0]); q++) {
			struct req_case c = req_base;
			unsigned k;

			for (k = 0; k < V34HS_STATE_COUNT; k += 7) {
				c.mst = (short)k;
				c.rxst = (short)((k + 29) % V34HS_STATE_COUNT);
				c.txst = (short)((k + 58) % V34HS_STATE_COUNT);
				diff_eq_int("three distinct states",
					    c.mst != c.rxst && c.rxst != c.txst
					    && c.mst != c.txst, 1,
					    6000 + (long)q * 100 + k);
				traced(reneg_thunk, &c, req_in[q],
				       6000 + (long)q * 100 + k,
				       "InitiateRateRenegotiation");
			}
		}

		/*
		 * And every one of the 87 names, reached by driving the three
		 * words together -- which is what t_v34hshak.c's first sweep
		 * does and what this one would otherwise miss, since the
		 * offsets above never make all three equal.
		 */
		for (i = 0; i < V34HS_STATE_COUNT; i++) {
			struct req_case c = req_base;

			c.mst = c.rxst = c.txst = (short)i;
			traced(reneg_thunk, &c, 3, 7000 + (long)i,
			       "InitiateRateRenegotiation, one name");
		}

		for (i = 0; i < 4; i++) {
			static const short rrn[] = { 0, 1, -1, 0x7fff };
			static const unsigned char cst[] = { 0, 1, 0x80, 0xff };
			struct req_case c = req_base;

			dsplib_debug_capture_reset();
			run_setv90(&c, rrn[i], cst[i], 8000 + (long)i);
			diff_eq_int("SetV90RateReneg transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, 8000 + (long)i);
			diff_eq_int("SetV90RateReneg said something",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    8000 + (long)i);
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	/*
	 * THE GATE ITSELF, which every transcript comparison in this tree is
	 * structurally blind to: they all raise the level first, so a call
	 * site that lost its `if (DSPLIB_DEBUG_ON())` prints the same thing
	 * and passes.  The capture is independent of the level, so turning it
	 * on with the level left down tests the other half -- below the
	 * threshold, these functions must say NOTHING.
	 *
	 * BOTH LEVELS BELOW IT, not just zero.  Every gate in the object is
	 * `> 1`, so 1 is the only value that separates it from the `>= 1` a
	 * reconstruction would write if it read the comparison as "on".
	 */
	diff_begin("v34 pcm interface: below the threshold, nothing is said");
	{
		unsigned lvl;

		dsplib_debug_capture_on = 1;

		for (lvl = 0; lvl <= 1; lvl++) {
			struct req_case c = req_base;

			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;
			dsplib_debug_capture_reset();

			run_hangup(&c, 9000 + (long)lvl * 10);
			run_reneg(&c, 3, 9001 + (long)lvl * 10);
			run_setv90(&c, 1, 1, 9002 + (long)lvl * 10);

			setup();
			VPcmV34SetTxScale(&oa);
			ref_VPcmV34SetTxScale(ob);
			V34XF_IndicateJdReceived(&oa, 1, 0);
			ref_V34XF_IndicateJdReceived(ob, 1, 0);
			V34XF_IndicateDilReceived(&oa, 1);
			ref_V34XF_IndicateDilReceived(ob, 1);
			V34XF_IndicateTrn2dReceived(&oa);
			ref_V34XF_IndicateTrn2dReceived(ob);
			V34XF_IndicateK56FlexRateDetermined(&oa);
			ref_V34XF_IndicateK56FlexRateDetermined(ob);

			diff_eq_int("ours printed nothing",
				    dsplib_debug_capture_text(0)[0], 0,
				    9000 + (long)lvl);
			diff_eq_int("and neither did the reference",
				    dsplib_debug_capture_text(1)[0], 0,
				    9000 + (long)lvl);
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	diff_begin("v34 pcm interface: every skipped pointer field earned it");
	{
		unsigned k;

		/*
		 * Four of the five are installed by the code under test, so
		 * the check is that something really installed them -- they
		 * no longer hold the seed.  The fifth, +0x3548, is the
		 * fixture's own: the object never writes it, and what
		 * justifies its hole is that the chain behind it was walked,
		 * which is what `check_session_chain` asserts per case.
		 */
		for (k = 1; k < NPTR; k++)
			diff_eq_int("pointer field was installed at least once",
				    saw_ptr_written[k], 1, (long)ptr_skip[k]);
		diff_eq_int("and the session chain was walked",
			    saw_chain_walked, 1, (long)ptr_skip[0]);
	}
	rc |= diff_end();

	return rc;
}
