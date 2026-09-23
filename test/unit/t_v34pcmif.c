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
 * and finding F134 is the reason that matters: a dropped call site is
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
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34scram.h"
#include "dsplib/v34shell.h"
/*
 * For the two `vpcm_run` callees `v34pcmif.c` defines.  The declarations were
 * made plain when the `DSPLIB_VPCM_UNWRITTEN` weak apparatus was removed: this
 * file calls them and never compares their addresses.  They are hard link
 * requirements now, as the blob's direct `R_386_PC32` calls make them.
 */
#include "dsplib/vpcm.h"

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
extern void ref_chkForceBaudRate(void *obj, struct v34_dftbin *bins);
extern short ref_GetVPcmMinimalTxPowerReduction(void *obj);
extern int ref_VPcmV34GetMaxUpstreamRateIndex(void *obj);
extern void *ref_VPcmV34GetCleanedSamples(void *obj, int *n);
extern int ref_VPcmV34GetCurrentSessionDP(void *obj);
extern int ref_VPcmV34GetCurrentRxBitRate(void *obj);
extern int ref_VPcmV34GetCurrentTxBitRate(void *obj);

/*
 * --- and the batch this file's second half covers -------------------------
 *
 * Thirteen more entry points out of the same translation unit, added after
 * the fixture above was frozen.  Everything below `main`'s existing sections
 * is ADDITIVE for the reason the rate block already gives: `ptr_skip`,
 * `compare()`, `seed_chain()` and `SESS_LEN` are exactly as the batch that
 * wrote them left them, and two recorded mutation suites were measured
 * against that fixture.  The three new sections that need more than the
 * object seed their own blocks on top and do their own comparing.
 */
extern int ref_VPcmV34Delete(void *obj);
extern void ref_VPcmV34SetMaxBlockLength(void *obj, int len);
extern int ref_VPcmV34GetQuickConnectIndication(void *obj);
extern int ref_VPcmV34GetCurrentRxBaudRate(void *obj);
extern int ref_VPcmV34GetCurrentTxBaudRate(void *obj);
extern int ref_VPcmV34GetCurrentRxCarrier(void *obj);
extern int ref_VPcmV34GetCurrentTxCarrier(void *obj);
extern int ref_VPcmV34GetSNR(void *obj);
extern void ref_VPcmV34NotifyDP(void *obj, int what);
extern int ref_VPcmV34RequestDPNotification(void *obj, int *flag, int *count,
					    int *done);
extern int ref_V34XF_GetMaxUpstreamRateIndex(void *obj);
extern void ref_V34XF_IndicateK56FlexJdReceived(void *obj,
						unsigned char constel);
extern void ref_VPcmV34SetIndicationOfRemoteRetrain(void *obj);

/* And what `V34XF_IndicateK56FlexJdReceived` needs to be legal. */
extern void ref_V34InitializeImplementationSpecific(void *obj);

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
	0xac3c,					/* the configuration object */
	0x0a28, 0x0e48,				/* receive shell context    */
	0x0a28 + V34_SHELL_TX, 0x0e48 + V34_SHELL_TX	/* transmit         */
};
#define NPTR (sizeof(ptr_skip) / sizeof(ptr_skip[0]))

/*
 * The first two are the FIXTURE's -- nothing under test writes either, and
 * what justifies their holes is that the memory behind them was reached,
 * which is asserted per case.  The rest are installed by the code under test
 * and the check at the bottom is that something really installed them.
 */
#define NFIXTURE_PTR 2

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
#define SESS_LEN	0x6200		/* indexed at +0x610c ... +0x6120 */
#define DEMOD_LEN	0x0240		/* indexed at +0x20c              */
#define LEAF_LEN	0x00c0		/* indexed at +0x8c               */
#define PCM_LEN		0x0520		/* indexed at +0x4f4 ... +0x4fc   */

#define SESS_PTR	0x175c
#define SESS_FLAG	0x173e
#define DEMOD_PTR	0x020c
#define LEAF_REQ	0x008c

/*
 * And the SECOND chain out of the session object, which the two questions
 * below use and the three requests above do not: a pointer at +0x610c to
 * whatever holds the PCM receiver's own limits, gated by an int at +0x6120.
 */
#define SESS_PCM	0x610c
#define SESS_GATE	0x6120
#define PCM_FLAG	0x04f4
#define PCM_SENS	0x04f8
#define PCM_CAP		0x04fc

#define CHAIN_FILL	0x3c

static unsigned char sess_a[SESS_LEN], sess_b[SESS_LEN];
static unsigned char demod_a[DEMOD_LEN], demod_b[DEMOD_LEN];
static unsigned char leaf_a[LEAF_LEN], leaf_b[LEAF_LEN];
static unsigned char pcm_a[PCM_LEN], pcm_b[PCM_LEN];

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
	memset(pcm_a, CHAIN_FILL, sizeof(pcm_a));
	memset(pcm_b, CHAIN_FILL, sizeof(pcm_b));

	put_ptr(sess_a, SESS_PTR, demod_a);
	put_ptr(sess_b, SESS_PTR, demod_b);
	put_ptr(demod_a, DEMOD_PTR, leaf_a);
	put_ptr(demod_b, DEMOD_PTR, leaf_b);
	put_ptr(sess_a, SESS_PCM, pcm_a);
	put_ptr(sess_b, SESS_PCM, pcm_b);

	poke_ptr(0x3548, sess_a, sess_b);
}

/* Is `i` inside one of the session's two pointer fields? */
static int
sess_hole(unsigned i)
{
	return (i >= SESS_PTR && i < SESS_PTR + 4)
	    || (i >= SESS_PCM && i < SESS_PCM + 4);
}

/*
 * Compare two buffers with one pointer-sized hole in each -- the links are
 * necessarily different addresses.  Everything else, including the flag byte
 * and the request word, is compared.
 *
 * `hole` of -1 means none; the session has two and passes its own predicate
 * instead, which is why the test is a function pointer rather than a range.
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

/* The same over the session, whose two pointer fields are both holes. */
static void
compare_sess(const char *what, long tag)
{
	unsigned i;
	int bad = 0;

	for (i = 0; i < SESS_LEN; i++) {
		if (sess_a[i] == sess_b[i] || sess_hole(i))
			continue;
		bad++;
		if (bad <= 4)
			diff_eq_int(what, sess_a[i], sess_b[i],
				    (long)i * 1000 + tag);
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
	compare_sess(what, tag);
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
			if (sess_a[i] != CHAIN_FILL && !sess_hole(i))
				touched = 1;
		for (i = 0; i < LEAF_LEN; i++)
			if (leaf_a[i] != CHAIN_FILL)
				touched = 1;
		diff_eq_int(what, touched, 0, tag);
	}
}

/*
 * --- and the fixture the two RATE getters need ----------------------------
 *
 * ADDITIVE ON PURPOSE.  Nothing above this point changes: `ptr_skip`,
 * `sess_hole`, `compare_link` and `DEMOD_LEN` are exactly as the batch that
 * wrote them left them, and two recorded mutation suites (`v34pcmif` and
 * `v34datapump_rrn`) were measured against that fixture.  Widening
 * `compare()`'s hole list or `seed_chain`'s wiring for the sake of two
 * read-only functions would move bytes in every one of the cases above, which
 * tiers.md's "adding a check to a file that has a mutation suite is not free"
 * is exactly about.  So this block seeds its own links on top, re-points the
 * one it shares, and does its own comparing.
 *
 * WHAT THE TWO REACH, and it is five chains rather than one:
 *
 *   Rx, role 0x66  p3548 +0x175c -> demodulator +0x18 -> a mapping block's
 *                  +0x00, gated by the demodulator's +0x280
 *   Rx, otherwise  pac18 +0x00, an int
 *   Tx, role !0x66 p3548 +0x1758 -> +0x40 -> one more indirection -> +0x04,
 *                  gated by +0x2c == 3
 *   Tx, role 0x66  p3548 +0x6124 -> +0x4c -> one more indirection -> +0x04,
 *                  gated by +0x2c == 3
 *   both, default  the rate configuration at +0xaa84
 *
 * THE TWO "ONE MORE INDIRECTION" LINKS ARE SEPARATE BUFFERS FROM THE COUNTS
 * THEY POINT AT, deliberately.  Folding each pair into one buffer whose first
 * word points at itself would work, and would make the extra dereference
 * invisible: a version that read `+0x04` of the FIRST pointer would land on
 * the same bytes.  Kept apart, that version reads the fill and is caught.
 */
#define RSESS_DEMOD	0x175c
#define RSESS_MOD	0x1758
#define RSESS_V92	0x6124

#define RDEM_LEN	0x2a0		/* indexed at +0x18 and +0x280   */
#define RTX_LEN		0x80		/* +0x2c, +0x40 and +0x4c        */
#define RSMALL_LEN	0x10		/* one word at +0x00 or +0x04    */

#define RDEM_MPAR	0x18
#define RDEM_GATE	0x280
#define RTX_STATE	0x2c
#define RTX_V90_FRAME	0x40
#define RTX_V92_FRAME	0x4c
#define RFRAME_BITS	0x04

#define RATE_FILL	0x5a

static unsigned char rdem_a[RDEM_LEN], rdem_b[RDEM_LEN];
static unsigned char rmpar_a[RSMALL_LEN], rmpar_b[RSMALL_LEN];
static unsigned char rmod_a[RTX_LEN], rmod_b[RTX_LEN];
static unsigned char rv92_a[RTX_LEN], rv92_b[RTX_LEN];
static unsigned char rl90_a[RSMALL_LEN], rl90_b[RSMALL_LEN];
static unsigned char rl92_a[RSMALL_LEN], rl92_b[RSMALL_LEN];
static unsigned char rf90_a[RSMALL_LEN], rf90_b[RSMALL_LEN];
static unsigned char rf92_a[RSMALL_LEN], rf92_b[RSMALL_LEN];
static unsigned char rp18_a[RSMALL_LEN], rp18_b[RSMALL_LEN];

/* The two knobs each PCM arm has, applied to both sides at once. */
static void
rate_poke(void *a, void *b, unsigned off, int v)
{
	memcpy((unsigned char *)a + off, &v, sizeof(v));
	memcpy((unsigned char *)b + off, &v, sizeof(v));
}

/*
 * Seed every link this block owns and hang them off the session.  Runs after
 * `setup()` and `seed_chain()`; it takes +0x175c away from `demod_a`, which
 * is 0x240 long and cannot hold the gate byte at +0x280.
 */
static void
seed_rates(void)
{
	memset(rdem_a, RATE_FILL, sizeof(rdem_a));
	memset(rdem_b, RATE_FILL, sizeof(rdem_b));
	memset(rmpar_a, RATE_FILL, sizeof(rmpar_a));
	memset(rmpar_b, RATE_FILL, sizeof(rmpar_b));
	memset(rmod_a, RATE_FILL, sizeof(rmod_a));
	memset(rmod_b, RATE_FILL, sizeof(rmod_b));
	memset(rv92_a, RATE_FILL, sizeof(rv92_a));
	memset(rv92_b, RATE_FILL, sizeof(rv92_b));
	memset(rl90_a, RATE_FILL, sizeof(rl90_a));
	memset(rl90_b, RATE_FILL, sizeof(rl90_b));
	memset(rl92_a, RATE_FILL, sizeof(rl92_a));
	memset(rl92_b, RATE_FILL, sizeof(rl92_b));
	memset(rf90_a, RATE_FILL, sizeof(rf90_a));
	memset(rf90_b, RATE_FILL, sizeof(rf90_b));
	memset(rf92_a, RATE_FILL, sizeof(rf92_a));
	memset(rf92_b, RATE_FILL, sizeof(rf92_b));
	memset(rp18_a, RATE_FILL, sizeof(rp18_a));
	memset(rp18_b, RATE_FILL, sizeof(rp18_b));

	put_ptr(sess_a, RSESS_DEMOD, rdem_a);
	put_ptr(sess_b, RSESS_DEMOD, rdem_b);
	put_ptr(sess_a, RSESS_MOD, rmod_a);
	put_ptr(sess_b, RSESS_MOD, rmod_b);
	put_ptr(sess_a, RSESS_V92, rv92_a);
	put_ptr(sess_b, RSESS_V92, rv92_b);

	put_ptr(rdem_a, RDEM_MPAR, rmpar_a);
	put_ptr(rdem_b, RDEM_MPAR, rmpar_b);
	put_ptr(rmod_a, RTX_V90_FRAME, rl90_a);
	put_ptr(rmod_b, RTX_V90_FRAME, rl90_b);
	put_ptr(rv92_a, RTX_V92_FRAME, rl92_a);
	put_ptr(rv92_b, RTX_V92_FRAME, rl92_b);
	put_ptr(rl90_a, 0, rf90_a);
	put_ptr(rl90_b, 0, rf90_b);
	put_ptr(rl92_a, 0, rf92_a);
	put_ptr(rl92_b, 0, rf92_b);

	poke_ptr(0xac18, rp18_a, rp18_b);
}

/*
 * The whole object, as `compare()` does it, plus the one extra hole this
 * block makes: `pac18` is a fixture pointer here and not in any case above,
 * so it is skipped HERE rather than added to `ptr_skip`.  What justifies the
 * hole is that the memory behind it is read back -- the Rx getter's middle
 * arm returns it -- and that is asserted per case below.
 */
static void
compare_rates(const char *what, long tag)
{
	const unsigned char *p = (const unsigned char *)&oa;
	unsigned i;
	int bad = 0;

	for (i = 0; i < sizeof(oa); i++) {
		if (p[i] == ob[i] || skipped(i)
		    || (i >= 0xac18 && i < 0xac18 + 4))
			continue;
		bad++;
		if (bad <= 8)
			diff_eq_int(what, p[i], ob[i], (long)i * 1000 + tag);
	}
	diff_eq_int(what, bad, 0, tag);
}

/*
 * And every link, with its own pointer word held out.  NEITHER GETTER WRITES
 * ANYTHING, so this is the check that they do not -- and it is not free: the
 * demodulator is reached through the same +0x175c that
 * `VPcmV34InitiateHangUp` writes a request code down, and a getter that
 * cleared a gate after reading it would look perfect from the return value.
 */
static void
compare_rate_links(const char *what, long tag)
{
	unsigned i;
	int bad = 0;

	for (i = 0; i < SESS_LEN; i++) {
		if (sess_a[i] == sess_b[i]
		    || (i >= RSESS_MOD && i < RSESS_MOD + 4)
		    || (i >= RSESS_DEMOD && i < RSESS_DEMOD + 4)
		    || (i >= RSESS_V92 && i < RSESS_V92 + 4)
		    || (i >= SESS_PCM && i < SESS_PCM + 4))
			continue;
		bad++;
		if (bad <= 4)
			diff_eq_int(what, sess_a[i], sess_b[i],
				    (long)i * 1000 + tag);
	}
	diff_eq_int(what, bad, 0, tag);

	compare_link(what, rdem_a, rdem_b, RDEM_LEN, RDEM_MPAR, tag);
	compare_link(what, rmpar_a, rmpar_b, RSMALL_LEN, (unsigned)-1, tag);
	compare_link(what, rmod_a, rmod_b, RTX_LEN, RTX_V90_FRAME, tag);
	compare_link(what, rv92_a, rv92_b, RTX_LEN, RTX_V92_FRAME, tag);
	compare_link(what, rl90_a, rl90_b, RSMALL_LEN, 0, tag);
	compare_link(what, rl92_a, rl92_b, RSMALL_LEN, 0, tag);
	compare_link(what, rf90_a, rf90_b, RSMALL_LEN, (unsigned)-1, tag);
	compare_link(what, rf92_a, rf92_b, RSMALL_LEN, (unsigned)-1, tag);
	compare_link(what, rp18_a, rp18_b, RSMALL_LEN, (unsigned)-1, tag);
}

/*
 * One case: seed everything, set the six knobs, ask both getters on both
 * sides, and check the four answers and every buffer.  `want_rx` and
 * `want_tx` of -1 mean "agreement only"; everything else is the arm's answer
 * worked out by hand, which is the second oracle -- two implementations that
 * were wrong the same way would agree with each other for ever.
 */
struct rate_case {
	short	role;		/* +0x359c                                  */
	int	status;		/* +0x0000                                  */
	short	rxbits;		/* the rate configuration's +0x14           */
	short	txbits;		/* its +0x04                                */
	int	pac18;		/* the int the Rx middle arm returns        */
	int	gate;		/* the demodulator's +0x280, as a byte      */
	unsigned int mpar;	/* the mapping block's +0x00                */
	int	v90state;	/* the V.90 modulator's +0x2c               */
	unsigned int v90bits;	/* its frame count                          */
	int	v92state;	/* the V.92 object's +0x2c                  */
	unsigned int v92bits;	/* its frame count                          */
};

/*
 * THE SEVEN ANSWERS THE BASE CASE MAKES DISTINGUISHABLE, and the reason the
 * base knobs are the values they are: no two arms can produce the same
 * number, so "which arm ran" is readable off the return value alone and the
 * role x status cross below can assert that ALL SEVEN were reached without
 * restating the fork it is testing.  A cross that never left one arm would
 * otherwise be a long green run proving one thing.
 */
#define RARM_RX_CFG	16800		/*  7 * 2400                        */
#define RARM_RX_PAC18	0x1234		/*    the int behind pac18          */
#define RARM_RX_DEMOD	56000		/* 42 * 8000/6                      */
#define RARM_TX_CFG	21600		/*  9 * 2400                        */
#define RARM_TX_V90	28000		/* 21 * 8000/6                      */
#define RARM_TX_V92	22000		/* 33 * 8000/12                     */
#define RARM_TX_K56	30000		/*    the constant                  */

static int saw_rate_arm[7];
static int last_rx, last_tx;

static void
note_rate_arm(int v)
{
	static const int arm[7] = {
		RARM_RX_CFG, RARM_RX_PAC18, RARM_RX_DEMOD, RARM_TX_CFG,
		RARM_TX_V90, RARM_TX_V92, RARM_TX_K56
	};
	int k;

	for (k = 0; k < 7; k++)
		if (v == arm[k])
			saw_rate_arm[k] = 1;
}

static void
run_rate_case(const struct rate_case *c, long want_rx, long want_tx, long tag)
{
	int ra, rb, ta, tb;

	setup();
	seed_chain();
	seed_rates();

	poke_short(0x359c, c->role);
	poke_int(0x0000, c->status);
	poke_short(0xaa84 + 0x14, c->rxbits);
	poke_short(0xaa84 + 0x04, c->txbits);

	rate_poke(rp18_a, rp18_b, 0, c->pac18);
	rdem_a[RDEM_GATE] = (unsigned char)c->gate;
	rdem_b[RDEM_GATE] = (unsigned char)c->gate;
	rate_poke(rmpar_a, rmpar_b, 0, (int)c->mpar);
	rate_poke(rmod_a, rmod_b, RTX_STATE, c->v90state);
	rate_poke(rf90_a, rf90_b, RFRAME_BITS, (int)c->v90bits);
	rate_poke(rv92_a, rv92_b, RTX_STATE, c->v92state);
	rate_poke(rf92_a, rf92_b, RFRAME_BITS, (int)c->v92bits);

	ra = VPcmV34GetCurrentRxBitRate(&oa);
	rb = ref_VPcmV34GetCurrentRxBitRate(ob);
	ta = VPcmV34GetCurrentTxBitRate(&oa);
	tb = ref_VPcmV34GetCurrentTxBitRate(ob);

	diff_eq_int("GetCurrentRxBitRate", ra, rb, tag);
	diff_eq_int("GetCurrentTxBitRate", ta, tb, tag);
	if (want_rx >= 0)
		diff_eq_int("GetCurrentRxBitRate, worked out by hand",
			    rb, want_rx, tag);
	if (want_tx >= 0)
		diff_eq_int("GetCurrentTxBitRate, worked out by hand",
			    tb, want_tx, tag);

	compare_rates("the rate getters write nothing", tag);
	compare_rate_links("the rate getters write nothing down the chain",
			   tag);

	last_rx = rb;
	last_tx = tb;
}

/* The knobs every sweep starts from; see the seven constants above. */
static const struct rate_case rate_base = {
	0x66, 1, 7, 9, RARM_RX_PAC18, 1, 42u, 3, 21u, 3, 33u
};

static void *
ptr_at(const void *base, unsigned off)
{
	void *p;

	memcpy(&p, (const unsigned char *)base + off, sizeof(p));
	return p;
}

/* --- chkForceBaudRate ----------------------------------------------------- */

/*
 * The configuration object, per side.  Only one byte of it is read, but the
 * buffer is long enough that a reconstruction reading the wrong offset lands
 * inside it and gets the fill rather than reading off the end -- which would
 * crash the fixture instead of reporting the disagreement.
 *
 * The fill is NOT the byte under test, so "read +0x50" and "read anything
 * else" produce different maximum indices: 0x71 >> 5 is 3, and the sweep
 * below drives +0x50 through all eight.
 */
#define CFG_LEN		0x80
#define CFG_FILL	0x71
#define CFG_MAXBAUD	0x50		/* bits 5..7 are the index */

static unsigned char cfg_a[CFG_LEN], cfg_b[CFG_LEN];

/* Where the six per-rate flags live inside the session object. */
#define ALLOW_OFF	0x217
#define ALLOW_LEN	6

/*
 * Coverage flags.  A byte comparison passes when NEITHER side wrote
 * anything, so each of the three things this function can do has to be seen
 * happening at least once or the sweep proves nothing.
 */
static int saw_force_capped;		/* a bin's `shift` was written    */
static int saw_force_session;		/* the V.90 arm edited the session */
static int saw_force_local;		/* the local arm left it alone    */

static void
run_force(unsigned char cfgbyte, int v90, int k56, const unsigned char *allow,
	  long tag)
{
	unsigned char want[ALLOW_LEN];
	unsigned i;
	int capped = 0;

	setup();
	seed_chain();

	memset(cfg_a, CFG_FILL, sizeof(cfg_a));
	memset(cfg_b, CFG_FILL, sizeof(cfg_b));
	cfg_a[CFG_MAXBAUD] = cfg_b[CFG_MAXBAUD] = cfgbyte;
	poke_ptr(0xac3c, cfg_a, cfg_b);

	memcpy(want, allow, ALLOW_LEN);
	memcpy(sess_a + ALLOW_OFF, allow, ALLOW_LEN);
	memcpy(sess_b + ALLOW_OFF, allow, ALLOW_LEN);

	poke_int(0x24c, v90);
	poke_int(0x250, k56);

	chkForceBaudRate(&oa, oa.probe_bins);
	ref_chkForceBaudRate(ob, (struct v34_dftbin *)(ob + 0xa320));

	/*
	 * The bank is inside the object, exactly as it is at both of
	 * `probeselect`'s call sites, so the whole-object comparison covers
	 * it -- and covers a write to the wrong bin, which a check on the six
	 * bins the function is *about* would not.
	 */
	compare("chkForceBaudRate", tag);

	/* The session, which the V.90 arm writes and the other two must not. */
	compare_sess("chkForceBaudRate session", tag);
	if (memcmp(sess_a + ALLOW_OFF, want, ALLOW_LEN) != 0)
		saw_force_session = 1;
	else
		saw_force_local = 1;

	/* The configuration is an input; nothing may write it. */
	for (i = 0; i < CFG_LEN; i++) {
		unsigned char seed = (i == CFG_MAXBAUD) ? cfgbyte : CFG_FILL;

		if (cfg_a[i] != seed || cfg_b[i] != seed) {
			diff_eq_int("chkForceBaudRate wrote its configuration",
				    0, 1, (long)i * 1000 + tag);
			break;
		}
	}

	for (i = 0; i < V34_PROBE_BINS; i++)
		if (oa.probe_bins[i].shift == 7 || oa.probe_bins[i].shift == 11)
			capped = 1;
	if (capped)
		saw_force_capped = 1;
}

/* --- the two questions asked of the PCM configuration --------------------- */

/*
 * Every input either of them reads, one field per member.  The three arm
 * selectors are separate from each other because that is the only way to tell
 * an `&&` from an `||` -- findings F116b, F123 and F171 -- and `sens` is separate
 * from `v90` for the same reason: the object consults `+0x4f8` only after
 * `v90_receiver`, and a reconstruction that or-ed them agrees on every case
 * where both are set.
 */
struct pwr_case {
	int	gate;		/* session +0x6120        */
	int	v90;		/* object  +0x24c         */
	int	k56;		/* object  +0x250         */
	int	sens;		/* pcm     +0x4f8         */
	int	cap;		/* pcm     +0x4fc         */
	short	want;		/* config  +0x44          */
	int	flag54;		/* config  +0x54          */
	int	maxrate;	/* config  +0x3c          */
};

static const struct pwr_case pwr_base = {
	1,		/* gate: a PCM modem exists                        */
	4,		/* v90:  past 1, so both functions take the arm     */
	0,		/* k56                                             */
	1,		/* sens                                            */
	14,		/* cap:  14 * 2400 = 33600                         */
	3,		/* want: inside the clamp and positive             */
	4,		/* flag54                                          */
	31200		/* maxrate                                         */
};

static int saw_pwr_high, saw_pwr_low, saw_pwr_clamped;
static int saw_rate_capped, saw_rate_plain;

static void
setup_pwr(const struct pwr_case *c)
{
	setup();
	seed_chain();

	memset(cfg_a, CFG_FILL, sizeof(cfg_a));
	memset(cfg_b, CFG_FILL, sizeof(cfg_b));
	memcpy(cfg_a + 0x3c, &c->maxrate, sizeof(c->maxrate));
	memcpy(cfg_b + 0x3c, &c->maxrate, sizeof(c->maxrate));
	memcpy(cfg_a + 0x44, &c->want, sizeof(c->want));
	memcpy(cfg_b + 0x44, &c->want, sizeof(c->want));
	memcpy(cfg_a + 0x54, &c->flag54, sizeof(c->flag54));
	memcpy(cfg_b + 0x54, &c->flag54, sizeof(c->flag54));
	poke_ptr(0xac3c, cfg_a, cfg_b);

	memcpy(sess_a + SESS_GATE, &c->gate, sizeof(c->gate));
	memcpy(sess_b + SESS_GATE, &c->gate, sizeof(c->gate));
	memcpy(pcm_a + PCM_SENS, &c->sens, sizeof(c->sens));
	memcpy(pcm_b + PCM_SENS, &c->sens, sizeof(c->sens));
	memcpy(pcm_a + PCM_CAP, &c->cap, sizeof(c->cap));
	memcpy(pcm_b + PCM_CAP, &c->cap, sizeof(c->cap));

	poke_int(0x24c, c->v90);
	poke_int(0x250, c->k56);
}

static void
run_pwr(const struct pwr_case *c, long tag)
{
	short ra, rb;
	int flag;

	setup_pwr(c);

	ra = GetVPcmMinimalTxPowerReduction(&oa);
	rb = ref_GetVPcmMinimalTxPowerReduction(ob);

	diff_eq_int("GetVPcmMinimalTxPowerReduction", ra, rb, tag);
	compare("GetVPcmMinimalTxPowerReduction", tag);
	compare_sess("GetVPcmMinimalTxPowerReduction session", tag);
	compare_link("GetVPcmMinimalTxPowerReduction pcm", pcm_a, pcm_b,
		     PCM_LEN, (unsigned)-1, tag);

	/*
	 * The PCM flag is the thing this writes that the return value cannot
	 * show, and its two values are the two arms -- so seeing both proves
	 * the sweep reached both, which a byte comparison alone would not.
	 */
	memcpy(&flag, pcm_a + PCM_FLAG, sizeof(flag));
	if (flag == 0)
		saw_pwr_high = 1;
	else if (flag == 1)
		saw_pwr_low = 1;
	if (ra != c->want && ra != 0)
		saw_pwr_clamped = 1;
}

static void
run_rate(const struct pwr_case *c, long tag)
{
	int ra, rb;

	setup_pwr(c);

	ra = VPcmV34GetMaxUpstreamRateIndex(&oa);
	rb = ref_VPcmV34GetMaxUpstreamRateIndex(ob);

	diff_eq_int("VPcmV34GetMaxUpstreamRateIndex", ra, rb, tag);
	compare("VPcmV34GetMaxUpstreamRateIndex", tag);
	compare_sess("VPcmV34GetMaxUpstreamRateIndex session", tag);
	compare_link("VPcmV34GetMaxUpstreamRateIndex pcm", pcm_a, pcm_b,
		     PCM_LEN, (unsigned)-1, tag);

	if (ra == c->maxrate)
		saw_rate_plain = 1;
	else
		saw_rate_capped = 1;
}

/*
 * The four pointers `preinitdigital` installs, checked the way t_v34digital.c
 * checks them: by which function or table each side selected, which is the
 * thing `role` decides and the thing an address comparison cannot see.
 */
static void
check_shell_ptrs(short role, long tag)
{
	int orig = (role == 0x65);
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
 * driven from one variable cannot be told apart -- findings F116b, F123 and F171
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
	short		role;
	short		mst;		/* +0x3592 */
	short		rxst;		/* +0x3594 */
	short		txst;		/* +0x3596 */
	short		trace1;		/* +0x2aa2 */
	short		trace2;		/* +0xaa78 */
	unsigned short	txflags;	/* +0x25c2 */
	unsigned short	rxflags;	/* receiver +0x122 */
	int		v90_receiver;	/* +0x24c */
	short		short_382;
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
	0x65,			/* role                                    */
	V34HS_PHASE1,		/* mst  = 33                                */
	V34HS_PHASE2,		/* rxst = 34                                */
	V34HS_TONE_AB,		/* txst = 60                                */
	0x1111, 0x2222,		/* [1], [2]                                 */
	0x0000, 0x0000,		/* txflags, rxflags                         */
	6,			/* v90_receiver                             */
	0x1234			/* short_382                                     */
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
	poke_short(0x359c, c->role);
	poke_short(0x3592, c->mst);
	poke_short(0x3594, c->rxst);
	poke_short(0x3596, c->txst);
	poke_short(0x2aa2, c->trace1);
	poke_short(0xaa78, c->trace2);
	poke_short(0x25c2, (short)c->txflags);
	poke_short(0x264 + 0x122, (short)c->rxflags);
	poke_short(0x0382, c->short_382);
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
		check_shell_ptrs(c->role, tag);
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
		check_shell_ptrs(c->role, tag);
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
	check_shell_ptrs(c->role, tag);
	/*
	 * The two polarities a plausible-but-wrong reconstruction gets
	 * backwards, asserted against the value rather than only against the
	 * blob: `rrn_type` is tested for ZERO and not for sign, and
	 * `constel_size` is read unsigned.
	 */
	diff_eq_int("SetV90RateReneg v90_receiver",
		    oa.v90_receiver, rrn_type != 0 ? 15 : 11, tag);
	diff_eq_int("SetV90RateReneg short_382",
		    (int)(unsigned short)oa.short_382,
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

/*
 * ===========================================================================
 * THE PUBLIC ACCESSOR SURFACE, thirteen more entry points.
 *
 * ADDITIVE, for the reason the rate block above gives at length: the fixture
 * everything before this point shares is frozen and two recorded mutation
 * suites were measured against it.  What follows seeds its own blocks where
 * it needs more than the object, and compares them itself.
 *
 * Offsets, all read out of `tools/dis.py` and all named here rather than
 * inline so that a sweep and its expectation cannot drift apart.
 */
#define OB_STATUS	0x0000
#define OB_PTC		0x0008
#define OB_SAMPLE_CNT	0x0238
#define OB_SAMPLES_VLD	0x0262
#define OB_RX_EQUERR	(0x0264 + 0x21a)	/* short, SIGNED, `jle`     */
#define OB_RX_SIG_ENERGY	(0x0264 + 0x248)	/* int, the numerator       */
#define OB_TXSTATE	0x3596
#define OB_F35A4	0x35a4
#define OB_FAA74	0xaa74
#define OB_CFG_BAUD	(V34_RATECFG + 0x00)
#define OB_CFG_PREEMP	(V34_RATECFG + 0x06)
#define OB_CFG_CARRIER	(V34_RATECFG + 0x10)
#define OB_CFG_RXBAUD	(V34_RATECFG + 0x12)
#define OB_CFG_RXCARR	(V34_RATECFG + 0x24)
#define OB_IS_SHORT	0xabcc
#define OB_MOH_TIMER	0xabdc
#define OB_AC17		0xac17
#define OB_CLR_FLAG	0xac40
#define OB_CLR_COUNT	0xac44
#define OB_CLR_DONE	0xac48

static int
get_int_a(unsigned off)
{
	int v;

	memcpy(&v, (const unsigned char *)&oa + off, sizeof(v));
	return v;
}

static short
get_short_a(unsigned off)
{
	short v;

	memcpy(&v, (const unsigned char *)&oa + off, sizeof(v));
	return v;
}

/* --- the four "current" getters ------------------------------------------- */

/*
 * FOUR DIFFERENT PREDICATES, NOT FOUR COPIES.  All four open on
 * `role == 0x66` and all four differ inside it, so `role` and `status` are
 * CROSSED: a version that took the receive pair's role test for the transmit
 * pair's agrees with the object on every case where the two happen to select
 * the same arm, and differs on exactly the cases this cross contains.
 *
 * The four configuration fields are seeded to four DIFFERENT values, none of
 * them 8000 and none of them 0 -- otherwise "returned the PCM constant" and
 * "returned the configured rate" are the same number and the arm that ran is
 * unreadable from the answer.  Each is a `short` returned as an `int` through
 * `movswl`, which is the only place the declared type is observable, so the
 * sweep also runs them negative.
 */
#define GBAUD		1234
#define GCARRIER	2345
#define GRXBAUD		3456
#define GRXCARRIER	4321

static int saw_getter_arm[8];

static void
run_getters(short role, int status, short baud, short carrier, short rxbaud,
	    short rxcarrier, long tag)
{
	int n_rx, n_tx;
	int want_rxb, want_txb, want_rxc, want_txc;
	int a, b;

	setup();
	poke_short(0x359c, role);
	poke_int(OB_STATUS, status);
	poke_short(OB_CFG_BAUD, baud);
	poke_short(OB_CFG_CARRIER, carrier);
	poke_short(OB_CFG_RXBAUD, rxbaud);
	poke_short(OB_CFG_RXCARR, rxcarrier);

	/*
	 * The hand calculation, which is the second oracle: two
	 * implementations wrong the same way agree with each other for ever.
	 * `n_rx` is the offset the RECEIVE pair uses and `n_tx` the one the
	 * TRANSMIT pair uses, and they swap across the role test.
	 */
	n_rx = (role == 0x66) ? status - 1 : status - 2;
	n_tx = (role == 0x66) ? status - 2 : status - 1;
	want_rxb = ((unsigned int)n_rx <= 1u) ? 8000 : (int)rxbaud;
	want_txb = ((unsigned int)n_tx <= 1u) ? 8000 : (int)baud;
	want_rxc = ((unsigned int)n_rx <= 1u) ? 0 : (int)rxcarrier;
	if (role == 0x66)
		want_txc = ((unsigned int)(status - 2) <= 1u)
			   ? 0 : (int)carrier;
	else
		want_txc = (status == 2) ? 0 : (int)carrier;

	a = VPcmV34GetCurrentRxBaudRate(&oa);
	b = ref_VPcmV34GetCurrentRxBaudRate(ob);
	diff_eq_int("GetCurrentRxBaudRate", a, b, tag);
	diff_eq_int("GetCurrentRxBaudRate, by hand", b, want_rxb, tag);
	saw_getter_arm[(a == 8000) ? 0 : 1] = 1;

	a = VPcmV34GetCurrentTxBaudRate(&oa);
	b = ref_VPcmV34GetCurrentTxBaudRate(ob);
	diff_eq_int("GetCurrentTxBaudRate", a, b, tag);
	diff_eq_int("GetCurrentTxBaudRate, by hand", b, want_txb, tag);
	saw_getter_arm[(a == 8000) ? 2 : 3] = 1;

	a = VPcmV34GetCurrentRxCarrier(&oa);
	b = ref_VPcmV34GetCurrentRxCarrier(ob);
	diff_eq_int("GetCurrentRxCarrier", a, b, tag);
	diff_eq_int("GetCurrentRxCarrier, by hand", b, want_rxc, tag);
	saw_getter_arm[(a == 0) ? 4 : 5] = 1;

	a = VPcmV34GetCurrentTxCarrier(&oa);
	b = ref_VPcmV34GetCurrentTxCarrier(ob);
	diff_eq_int("GetCurrentTxCarrier", a, b, tag);
	diff_eq_int("GetCurrentTxCarrier, by hand", b, want_txc, tag);
	saw_getter_arm[(a == 0) ? 6 : 7] = 1;

	/* None of the four writes anything. */
	compare("the four current getters store nothing", tag);
}

/* --- GetQuickConnectIndication -------------------------------------------- */

static int saw_qc_arm[3];

static void
run_quickconnect(int status, short is_short, long tag)
{
	int a, b, want;

	setup();
	poke_int(OB_STATUS, status);
	poke_short(OB_IS_SHORT, is_short);

	/*
	 * `1 << status` against three masks, and the shift is only reached
	 * when `status` is 0..10 UNSIGNED -- so a negative one leaves at the
	 * first test and never shifts.  `is_short` comes back through
	 * `movswl`, so a negative one comes back negative.
	 */
	if ((unsigned int)status > 10u) {
		want = 0;
	} else if (((1 << status) & 0xe7) != 0) {
		want = is_short;
		saw_qc_arm[0] = 1;
	} else if (((1 << status) & 0x408) != 0) {
		want = 0;
		saw_qc_arm[1] = 1;
	} else {
		want = 1;
		saw_qc_arm[2] = 1;
	}

	a = VPcmV34GetQuickConnectIndication(&oa);
	b = ref_VPcmV34GetQuickConnectIndication(ob);
	diff_eq_int("GetQuickConnectIndication", a, b, tag);
	diff_eq_int("GetQuickConnectIndication, by hand", b, want, tag);
	compare("GetQuickConnectIndication stores nothing", tag);
}

/* --- GetSNR ---------------------------------------------------------------- */

/*
 * TWO LOOPS AND A HAND-OFF, and the hand-off is where an off-by-one lives:
 * the coarse loop hands the fine one the LAST value that was still positive
 * rather than the one that ended it.  So the expectation here is written the
 * object's way -- from the two step constants -- and not as `10 * log10`,
 * which would be a different function that happens to agree at most points.
 *
 * `equerr` is SIGNED and the guard is `jle`, so zero and negative return 0
 * without dividing; the sweep contains both.
 *
 * THE MULTIPLY IS `imul` AND THE SHIFT IS `sar`, and this helper is written
 * from those two instructions rather than from the source it is checking.
 * The distinction is the whole of what it is worth: a wrapped product is
 * NEGATIVE to a `sar` and huge to a `shr`, so a helper that shifted the
 * unsigned product -- which is what the first version of this one did --
 * disagrees with the object on every ratio past 2^31/0x1013 and agrees on
 * every smaller one.  It reported 56 dB where the object answers 35.
 */
static int saw_snr_zero, saw_snr_coarse, saw_snr_fine;

static int
snr_by_hand(short equerr, int sig_energy)
{
	int db = 0;
	int last = 0;

	if (equerr > 0) {
		int v = sig_energy / equerr;

		if (v > 0) {
			for (;;) {
				last = v;
				v = (int)((unsigned int)v * 0x1013u) >> 14;
				if (v <= 0)
					break;
				db += 6;
			}
		}
	}

	if (last > 0) {
		for (;;) {
			last = (int)((unsigned int)last * 0x32d6u) >> 14;
			if (last <= 0)
				break;
			db += 1;
		}
	}

	return db;
}

static void
run_snr(short equerr, int sig_energy, long tag)
{
	int a, b;

	setup();
	poke_short(OB_RX_EQUERR, equerr);
	poke_int(OB_RX_SIG_ENERGY, sig_energy);

	a = VPcmV34GetSNR(&oa);
	b = ref_VPcmV34GetSNR(ob);

	diff_eq_int("GetSNR", a, b, tag);
	diff_eq_int("GetSNR, by hand", b, snr_by_hand(equerr, sig_energy), tag);
	compare("GetSNR stores nothing", tag);

	if (b == 0)
		saw_snr_zero = 1;
	if (b >= 6)
		saw_snr_coarse = 1;
	if (b % 6 != 0)
		saw_snr_fine = 1;
}

/* --- NotifyDP -------------------------------------------------------------- */

static int saw_notify_arm[5];

static void
run_notify(int what, int moh, int status, int count, short valid, long tag)
{
	setup();
	poke_int(OB_MOH_TIMER, moh);
	poke_int(OB_STATUS, status);
	poke_int(OB_SAMPLE_CNT, count);
	poke_short(OB_SAMPLES_VLD, valid);

	VPcmV34NotifyDP(&oa, what);
	ref_VPcmV34NotifyDP(ob, what);

	compare("NotifyDP", tag);

	/*
	 * And what each arm was supposed to do, worked out by hand.  The
	 * byte comparison sees a store landing one field over; it cannot see
	 * both sides storing the same wrong constant.
	 */
	switch (what) {
	case 0:
		diff_eq_int("NotifyDP 0 invalidated the samples",
			    (int)get_short_a(OB_SAMPLES_VLD), 0, tag);
		diff_eq_int("...and left the status", get_int_a(OB_STATUS),
			    status, tag);
		saw_notify_arm[0] = 1;
		break;
	case 1:
		diff_eq_int("NotifyDP 1 validated the samples",
			    (int)get_short_a(OB_SAMPLES_VLD), 1, tag);
		diff_eq_int("...and left the status", get_int_a(OB_STATUS),
			    status, tag);
		saw_notify_arm[1] = 1;
		break;
	case 2:
		diff_eq_int("NotifyDP 2 set the status", get_int_a(OB_STATUS),
			    5, tag);
		diff_eq_int("...and restarted the sample count",
			    get_int_a(OB_SAMPLE_CNT), 0, tag);
		diff_eq_int("...and left the sample flag",
			    (int)get_short_a(OB_SAMPLES_VLD), (int)valid, tag);
		saw_notify_arm[2] = 1;
		break;
	case 3:
		diff_eq_int("NotifyDP 3 set the status", get_int_a(OB_STATUS),
			    6, tag);
		diff_eq_int("...and set count2 48000 samples on",
			    get_int_a(OB_FAA74),
			    (int)((unsigned int)moh + 0xbb80u), tag);
		diff_eq_int("...and left the sample count",
			    get_int_a(OB_SAMPLE_CNT), count, tag);
		saw_notify_arm[3] = 1;
		break;
	default:
		diff_eq_int("NotifyDP did nothing at all",
			    get_int_a(OB_STATUS) == status
			    && get_int_a(OB_SAMPLE_CNT) == count
			    && get_short_a(OB_SAMPLES_VLD) == valid, 1, tag);
		saw_notify_arm[4] = 1;
		break;
	}
}

/* --- RequestDPNotification ------------------------------------------------- */

/*
 * THREE DISTINCT SENTINELS IN THE THREE DESTINATIONS, and three distinct
 * values in the mailbox.  One repeated pattern cannot tell a version that
 * handed out the wrong one of the three, and a shared sentinel cannot tell
 * "wrote nothing" from "wrote the same thing".
 */
#define RQ_SENT0	0x11112222
#define RQ_SENT1	0x33334444
#define RQ_SENT2	0x55556666
#define RQ_CFG_BIT	0x51

static int saw_reqdp_empty, saw_reqdp_full, saw_reqdp_bit_was_clear;

static void
run_reqdp(int flagv, int countv, int donev, unsigned char cfg51, long tag)
{
	int fa = RQ_SENT0, ca = RQ_SENT1, da = RQ_SENT2;
	int fb = RQ_SENT0, cb = RQ_SENT1, db = RQ_SENT2;
	int ra, rb;
	unsigned k;
	int bad = 0;

	setup();
	memset(cfg_a, CFG_FILL, sizeof(cfg_a));
	memset(cfg_b, CFG_FILL, sizeof(cfg_b));
	cfg_a[RQ_CFG_BIT] = cfg_b[RQ_CFG_BIT] = cfg51;
	poke_ptr(0xac3c, cfg_a, cfg_b);

	poke_int(OB_CLR_FLAG, flagv);
	poke_int(OB_CLR_COUNT, countv);
	poke_int(OB_CLR_DONE, donev);

	ra = VPcmV34RequestDPNotification(&oa, &fa, &ca, &da);
	rb = ref_VPcmV34RequestDPNotification(ob, &fb, &cb, &db);

	diff_eq_int("RequestDPNotification", ra, rb, tag);
	diff_eq_int("RequestDPNotification flag out", fa, fb, tag);
	diff_eq_int("RequestDPNotification count out", ca, cb, tag);
	diff_eq_int("RequestDPNotification done out", da, db, tag);
	compare("RequestDPNotification", tag);

	for (k = 0; k < CFG_LEN; k++)
		if (cfg_a[k] != cfg_b[k])
			bad++;
	diff_eq_int("RequestDPNotification configuration", bad, 0, tag);

	if (flagv < 0) {
		/*
		 * NEGATIVE MEANS EMPTY, and the whole of what that promises
		 * is that nothing is written THROUGH THE POINTERS.  Proved
		 * by the sentinels surviving, which a comparison against the
		 * reference cannot show on its own: two versions that both
		 * wrote rubbish would agree.
		 */
		diff_eq_int("an empty mailbox answers 0", ra, 0, tag);
		diff_eq_int("...and leaves the flag destination", fa,
			    RQ_SENT0, tag);
		diff_eq_int("...and the count destination", ca, RQ_SENT1, tag);
		diff_eq_int("...and the done destination", da, RQ_SENT2, tag);
		diff_eq_int("...and the mailbox itself",
			    get_int_a(OB_CLR_FLAG), flagv, tag);
		diff_eq_int("...and the configuration byte",
			    (int)cfg_a[RQ_CFG_BIT], (int)cfg51, tag);
		saw_reqdp_empty = 1;
	} else {
		diff_eq_int("a full mailbox answers 1", ra, 1, tag);
		diff_eq_int("...and hands out the flag", fa, flagv, tag);
		diff_eq_int("...and the count", ca, countv, tag);
		diff_eq_int("...and the done word", da, donev, tag);
		diff_eq_int("...and resets the flag to -1",
			    get_int_a(OB_CLR_FLAG), -1, tag);
		diff_eq_int("...and clears the count",
			    get_int_a(OB_CLR_COUNT), 0, tag);
		diff_eq_int("...and clears the done word",
			    get_int_a(OB_CLR_DONE), 0, tag);
		diff_eq_int("...and clears bit 0 of the configuration",
			    (int)cfg_a[RQ_CFG_BIT],
			    (int)(unsigned char)(cfg51 & 0xfe), tag);
		if ((cfg51 & 1) == 0)
			saw_reqdp_bit_was_clear = 1;
		saw_reqdp_full = 1;
	}
}

/* --- V34XF_GetMaxUpstreamRateIndex ---------------------------------------- */

/*
 * BYTE FOR BYTE THE SAME FUNCTION as `VPcmV34GetMaxUpstreamRateIndex`, so it
 * is swept the same way and through the same fixture: five inputs across two
 * blocks, the gate at the session's +0x6120, `v90_receiver` over values that
 * separate `> 1` from `!= 0`, the PCM object's +0x4f8 both ways, and a
 * configured rate both above and below `+0x4fc * 2400`.
 *
 * Sharing `pwr_case` with the other one is deliberate: if the two ever stop
 * agreeing, the sweep that catches it is the one they both run.
 */
static int saw_xf_capped, saw_xf_plain;

static void
run_xfrate(const struct pwr_case *c, int cross, long tag)
{
	int ra, rb, rc2;

	setup_pwr(c);

	ra = V34XF_GetMaxUpstreamRateIndex(&oa);
	rb = ref_V34XF_GetMaxUpstreamRateIndex(ob);

	diff_eq_int("V34XF_GetMaxUpstreamRateIndex", ra, rb, tag);
	compare("V34XF_GetMaxUpstreamRateIndex", tag);
	compare_sess("V34XF_GetMaxUpstreamRateIndex session", tag);
	compare_link("V34XF_GetMaxUpstreamRateIndex pcm", pcm_a, pcm_b,
		     PCM_LEN, (unsigned)-1, tag);

	/*
	 * AND IT IS THE SAME ANSWER THE OTHER PREFIX GIVES.  That is the
	 * claim the source makes about this function -- one body under two
	 * names -- and it is checked here rather than assumed.
	 *
	 * OFF WHILE THE TRANSCRIPT IS BEING CAPTURED: the second call prints
	 * a second line on OUR side only, and the comparison would then be
	 * measuring the fixture.
	 *
	 * NOT RE-SEEDED FIRST, because the comparison two lines up has just
	 * proved that neither side wrote anything -- so the state the second
	 * call sees is the state the first one saw.
	 */
	if (cross) {
		rc2 = VPcmV34GetMaxUpstreamRateIndex(&oa);
		diff_eq_int("...and the two prefixes agree", ra, rc2, tag);
	}

	if (ra == c->maxrate)
		saw_xf_plain = 1;
	else
		saw_xf_capped = 1;
}

/* --- V34XF_IndicateK56FlexJdReceived -------------------------------------- */

/*
 * THE ONE THAT CALLS OUT.  `v34setuptxmit` runs `settxlevel`,
 * `V34SetupModulator`, two state transitions and `txinit`, so the object has
 * to be LEGAL rather than merely filled: `V34InitializeImplementationSpecific`
 * aims the two echo cancellers' five pointers each -- without it the first
 * dereference faults -- the rate configuration has to name a real symbol rate,
 * and the three state words have to be in range because the transitions index
 * `StateName` with whatever they find (D42).
 *
 * Its own hole list, because the initialiser and the handshake install
 * pointers INTO the object and the two sides are at two addresses.  It was
 * built by running with an empty list and classifying what differed, which
 * is what t_v34retrain.c did; every entry is asserted reached at the end, so
 * one that stops being written fails rather than quietly widening the test.
 */
static const unsigned k56_ptr_skip[] = {
	0x3548, 0xac3c,			/* the fixture's own            */
	0x0a28, 0x0e48,			/* receive shell context        */
	0x0a28 + V34_SHELL_TX, 0x0e48 + V34_SHELL_TX,
	0x0268, 0x026c,			/* rxq read and write cursors   */
	/*
	 * NOT the receiver's three -- +0x394, +0x418 and +0x508 -- nor the
	 * detector's coefficients at +0x3564.  `rxinit`, `setupreceiver`,
	 * `dpskinit` and `detectorinit` install those, and `v34setuptxmit`
	 * calls none of them: it is `settxlevel`, `V34SetupModulator`, two
	 * transitions and `txinit`.  They were in this list, transcribed from
	 * t_v34hshak.c, until the run said they had never been written --
	 * which is what the assertion at the end of the sweep is for.
	 */
	0x1460,				/* modulator +0x10 sine         */
	0x2074,				/* modulator +0xc24 shaped      */
	0x20cc,				/* modulator +0xc7c ec_prem     */
	0x2100,				/* modulator +0xcb0 preemp      */
	0x2220, 0x2224,			/* txq cursors                  */
	0x2608, 0x2a28,			/* transmit shell context       */
	0x80b8, 0x80bc, 0x80c0, 0x80c4, 0x80c8,	/* echo canceller 0     */
	0x9138, 0x913c, 0x9140, 0x9144, 0x9148	/* and 1                */
};
#define NK56PTR (sizeof(k56_ptr_skip) / sizeof(k56_ptr_skip[0]))
#define NK56FIXTURE 2

static int saw_k56_ptr[NK56PTR];

static int
k56_skipped(unsigned off)
{
	unsigned k;

	for (k = 0; k < NK56PTR; k++)
		if (off >= k56_ptr_skip[k] && off < k56_ptr_skip[k] + 4)
			return 1;
	return 0;
}

static void
compare_k56(const char *what, long tag)
{
	const unsigned char *p = (const unsigned char *)&oa;
	unsigned i, k;
	int bad = 0;

	for (k = 0; k < NK56PTR; k++)
		if (memcmp(p + k56_ptr_skip[k], ob + k56_ptr_skip[k], 4) != 0)
			saw_k56_ptr[k] = 1;

	for (i = 0; i < sizeof(oa); i++) {
		if (p[i] == ob[i] || k56_skipped(i))
			continue;
		bad++;
		if (bad <= 8)
			diff_eq_int(what, p[i], ob[i], (long)i * 1000 + tag);
	}
	diff_eq_int(what, bad, 0, tag);
}

/* The two modulator tables, by CONTENT: they are installed by address. */
static void
compare_k56_table(const char *what, unsigned off, long tag)
{
	const short *a = (const short *)ptr_at(&oa, off);
	const short *b = (const short *)ptr_at(ob, off);
	int k, diffs = 0;

	for (k = 0; k < 16; k++)
		if (a[k] != b[k])
			diffs++;
	diff_eq_int(what, diffs, 0, tag);
}

static int saw_k56_constel[2], saw_k56_txstate[2];

static void
run_k56jd(const struct pwr_case *pc, unsigned char constel, short short_35a4,
	  short txstate, short baud, short carrier, short preemp,
	  unsigned short rxflags, long tag)
{
	setup_pwr(pc);

	V34InitializeImplementationSpecific(&oa);
	ref_V34InitializeImplementationSpecific(ob);

	/* Re-aim the two fixture pointers, in case the initialiser moved them. */
	poke_ptr(0x3548, sess_a, sess_b);
	poke_ptr(0xac3c, cfg_a, cfg_b);

	poke_short(0xa9dc, (short)0x00e4);
	poke_short(0x25d4, 0x16a1);
	poke_short(OB_CFG_BAUD, baud);
	poke_short(OB_CFG_CARRIER, carrier);
	poke_short(OB_CFG_PREEMP, preemp);
	poke_short(0x3592, V34HS_PHASE1);
	poke_short(0x3594, V34HS_PHASE2);
	poke_short(OB_TXSTATE, txstate);
	poke_short(0x2aa2, (short)0x1111);
	poke_short(0xaa78, (short)0x2222);
	poke_short(0x264 + 0x122, (short)rxflags);
	poke_short(0x25c2, (short)0x1234);
	poke_short(OB_F35A4, short_35a4);
	poke_short(0x382, (short)0x4321);

	V34XF_IndicateK56FlexJdReceived(&oa, constel);
	ref_V34XF_IndicateK56FlexJdReceived(ob, constel);

	compare_k56("IndicateK56FlexJdReceived", tag);
	compare_sess("IndicateK56FlexJdReceived session", tag);
	compare_link("IndicateK56FlexJdReceived pcm", pcm_a, pcm_b, PCM_LEN,
		     (unsigned)-1, tag);
	diff_eq_int("IndicateK56FlexJdReceived configuration",
		    memcmp(cfg_a, cfg_b, CFG_LEN) == 0, 1, tag);
	compare_k56_table("IndicateK56FlexJdReceived ec_prem", 0x20cc, tag);
	compare_k56_table("IndicateK56FlexJdReceived preemp", 0x2100, tag);

	/*
	 * The three things the object comparison cannot see on its own,
	 * against the value rather than only against the blob.  The
	 * constellation test is `== 0x10` and NOT `!= 0`, which is what makes
	 * 1 and 0xff interesting; the DC seed is `336 * short_35a4 + 10000`
	 * truncated to a short, which is where a version that kept 32 bits
	 * differs; and bit 3 of the receiver's flags is SET rather than
	 * assigned.
	 */
	diff_eq_int("IndicateK56FlexJdReceived short_382",
		    (int)(unsigned short)get_short_a(0x382),
		    constel == 0x10 ? 0x89b0 : 0x8990, tag);
	saw_k56_constel[constel == 0x10] = 1;
	diff_eq_int("IndicateK56FlexJdReceived seeded the DC estimator",
		    (int)get_short_a(0x254),
		    (int)(short)(336 * (int)short_35a4 + 10000), tag);
	diff_eq_int("...and cleared the two behind it",
		    get_short_a(0x256) == 0 && get_int_a(0x258) == 0, 1, tag);
	diff_eq_int("IndicateK56FlexJdReceived set the AGC bit",
		    (int)(unsigned short)get_short_a(0x264 + 0x122) & 8, 8,
		    tag);
	diff_eq_int("IndicateK56FlexJdReceived forced the transmit state",
		    (int)get_short_a(OB_TXSTATE), 0x12, tag);
	saw_k56_txstate[txstate == 0x12] = 1;
}

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

	/* --- the two `vpcm_run` callees this file now defines ------------ */

	/*
	 * GetCleanedSamples IS A DRAIN, so the count and the RESET are two
	 * separate claims and the second is the one a plausible wrong version
	 * gets wrong.  Each case therefore calls it TWICE: the first call must
	 * hand back what was seeded and the second must hand back zero.  A
	 * reconstruction that returned the index and left it alone passes
	 * every single-call check there is.
	 *
	 * The index is swept SIGNED and past its ring bound.  `hist2_idx` is a
	 * `short` and the object reads it with `movswl`, so 0x8000 and 0xffff
	 * are the two values that separate that from a `movzwl`, and the two
	 * agree over every value the ring actually produces -- which is
	 * CLAUDE.md's "forced, so act on it" case observed from the test side
	 * instead of from the codegen.  0x257 and 0x258 are the ring's last
	 * entry and one past it, and nothing in the function bounds either.
	 *
	 * The RETURN is compared as an OFFSET from the object, not as a
	 * pointer: the two objects are at two addresses and always will be, so
	 * the pointer values differ for no reason and the displacement is the
	 * whole of what the function computes.
	 */
	diff_begin("v34 pcm interface: GetCleanedSamples drains the ring and "
		   "hands back its base");
	{
		static const short idx[] = {
			0, 1, -1, 2, 0x257, 0x258, 0x259, 0x7fff,
			(short)0x8000, (short)0xffff, 1234
		};

		for (i = 0; i < sizeof(idx) / sizeof(idx[0]); i++) {
			int na = 0x5eed, nb = 0x5eed;
			void *ra, *rb;
			long oa_off, ob_off;

			setup();
			poke_short(0x2aa6, idx[i]);

			ra = VPcmV34GetCleanedSamples(&oa, &na);
			rb = ref_VPcmV34GetCleanedSamples(ob, &nb);
			oa_off = (long)((unsigned char *)ra
					- (unsigned char *)&oa);
			ob_off = (long)((unsigned char *)rb - ob);

			diff_eq_int("GetCleanedSamples: the count", na, nb,
				    1200 + i);
			diff_eq_int("GetCleanedSamples: the buffer offset",
				    (int)oa_off, (int)ob_off, 1200 + i);
			compare("GetCleanedSamples", 1200 + i);

			/*
			 * AND AGAIN, WITHOUT RE-SEEDING.  This is the half
			 * that catches a peek written where a drain belongs;
			 * without it the zeroing store is invisible to the
			 * count check and visible to the object compare only
			 * because the fill pattern happens to differ from 0.
			 */
			na = nb = 0x5eed;
			ra = VPcmV34GetCleanedSamples(&oa, &na);
			rb = ref_VPcmV34GetCleanedSamples(ob, &nb);
			diff_eq_int("GetCleanedSamples: drained, so the "
				    "second ask is zero", na, nb, 1300 + i);
			diff_eq_int("...and it really is zero, not merely "
				    "equal", na, 0, 1300 + i);
			diff_eq_int("GetCleanedSamples: the buffer offset is "
				    "unconditional",
				    (int)((unsigned char *)ra
					  - (unsigned char *)&oa),
				    (int)((unsigned char *)rb - ob),
				    1300 + i);
			compare("GetCleanedSamples, second ask", 1300 + i);
		}
	}
	rc |= diff_end();

	/*
	 * GetCurrentSessionDP: THE SELECTOR AND THE V.92 PAIR ARE CROSSED, not
	 * sampled.  `status` decides four arms and only one of them consults
	 * the pair, so a version that tested the pair on the wrong arm -- or
	 * that used `||` where the object uses `&&` -- differs from this one
	 * on exactly two of the thirty-three combinations below and on nothing
	 * else.  Both `local_v92` and `remote_v92` therefore take 0, 1 and -1,
	 * and `status` takes every arm plus both sides of each boundary and
	 * both extremes of the signed range, because the object's `cmp`s are
	 * signed and an unsigned reconstruction would agree everywhere except
	 * there.
	 *
	 * The function writes nothing, so the object comparison is the check
	 * that it writes nothing -- which is not free: it shares its selector
	 * with two entry points a few lines away that do write, and a
	 * reconstruction that reset the pair after reading it would look
	 * perfect from the return value alone.
	 */
	diff_begin("v34 pcm interface: GetCurrentSessionDP, every arm crossed "
		   "with the V.92 pair");
	{
		static const int st[] = {
			(-0x7fffffff - 1), -1, 0, 1, 2, 3, 4, 5, 90, 92,
			0x7fffffff
		};
		static const short v92[] = { 0, 1, -1 };
		unsigned l, r;

		for (i = 0; i < sizeof(st) / sizeof(st[0]); i++)
		for (l = 0; l < sizeof(v92) / sizeof(v92[0]); l++)
		for (r = 0; r < sizeof(v92) / sizeof(v92[0]); r++) {
			long tag = (long)(i * 9 + l * 3 + r);
			int da, db;

			setup();
			poke_int(0x0000, st[i]);
			poke_short(0xabc6, v92[l]);
			poke_short(0xabc8, v92[r]);

			da = VPcmV34GetCurrentSessionDP(&oa);
			db = ref_VPcmV34GetCurrentSessionDP(ob);

			diff_eq_int("GetCurrentSessionDP", da, db,
				    2000 + tag);
			/*
			 * AND IT IS ONE OF THE FOUR.  Two wrong answers that
			 * agree would pass the line above; nothing else in
			 * this tree looks at the value, and `vpcm_run` puts it
			 * straight into `dp.id` for the host to read.
			 */
			diff_eq_int("...and it is a modulation number",
				    da == 34 || da == 56 || da == 90
				    || da == 92, 1, 2000 + tag);
			compare("GetCurrentSessionDP", 2000 + tag);
		}
	}
	rc |= diff_end();

	/* --- the two rate getters ---------------------------------------- */

	/*
	 * `role` AND `status` ARE CROSSED, and that is the whole point of this
	 * sweep: the two functions do NOT ask the same question of `status`,
	 * and each asks it on the opposite side of the `role` fork from the
	 * other.  The receiver takes its PCM arm when role IS 0x66, the
	 * transmitter's V.90 arm when role is NOT; a reconstruction that put
	 * either test on the wrong side of the other agrees with the blob on
	 * every case where the two happen to select the same arm, which is
	 * most of them, and differs here.  Both signed extremes of `status`
	 * are in, because the object's `cmp`s are signed.
	 *
	 * ALL SEVEN ARMS ARE ASSERTED REACHED at the end, by the numbers the
	 * base knobs make unique.  That is the anti-vacuity check for the
	 * sweep itself, and it does not restate the fork: it says only that
	 * seven distinguishable answers came back, which a cross stuck in one
	 * arm cannot produce.
	 */
	diff_begin("v34 pcm interface: the two rate getters, role crossed "
		   "with status");
	{
		static const short role_v[] = {
			0x66, 0x65, 0, 1, 0x67, -1
		};
		static const int st_v[] = {
			(-0x7fffffff - 1), -1, 0, 1, 2, 3, 4, 90, 92,
			0x7fffffff
		};
		struct rate_case c;
		unsigned r, s;

		for (r = 0; r < sizeof(role_v) / sizeof(role_v[0]); r++)
		for (s = 0; s < sizeof(st_v) / sizeof(st_v[0]); s++) {
			c = rate_base;
			c.role = role_v[r];
			c.status = st_v[s];
			run_rate_case(&c, -1, -1, 3000 + (long)(r * 10 + s));
			note_rate_arm(last_rx);
			note_rate_arm(last_tx);
		}

		diff_eq_int("the config arm answered the receiver",
			    saw_rate_arm[0], 1, 3900);
		diff_eq_int("the pac18 arm answered the receiver",
			    saw_rate_arm[1], 1, 3901);
		diff_eq_int("the demodulator arm answered the receiver",
			    saw_rate_arm[2], 1, 3902);
		diff_eq_int("the config arm answered the transmitter",
			    saw_rate_arm[3], 1, 3903);
		diff_eq_int("the V.90 arm answered the transmitter",
			    saw_rate_arm[4], 1, 3904);
		diff_eq_int("the V.92 arm answered the transmitter",
			    saw_rate_arm[5], 1, 3905);
		diff_eq_int("the K56flex arm answered the transmitter",
			    saw_rate_arm[6], 1, 3906);
	}
	rc |= diff_end();

	/*
	 * THE SEVEN ARMS, EACH NAMED, WITH ITS ANSWER WORKED OUT BY HAND.
	 * Agreement with the blob is the oracle; this is the second one, and
	 * it is what says the numbers above are the arithmetic they are meant
	 * to be rather than two implementations agreeing about nonsense.
	 */
	diff_begin("v34 pcm interface: each rate arm, against a hand "
		   "calculation");
	{
		struct rate_case c;

		/* role 0x66, status 1 and 2: the demodulator, both ways. */
		c = rate_base;
		c.role = 0x66;
		c.status = 1;
		run_rate_case(&c, RARM_RX_DEMOD, RARM_TX_CFG, 3910);
		c.status = 2;
		run_rate_case(&c, RARM_RX_DEMOD, RARM_TX_V92, 3911);

		/* role 0x66, status 3: K56flex out, and the config back. */
		c.status = 3;
		run_rate_case(&c, RARM_RX_CFG, RARM_TX_K56, 3912);

		/* role 0x66, status 0 and 4: neither PCM arm on either side. */
		c.status = 0;
		run_rate_case(&c, RARM_RX_CFG, RARM_TX_CFG, 3913);
		c.status = 4;
		run_rate_case(&c, RARM_RX_CFG, RARM_TX_CFG, 3914);

		/* role 0x65: the transmitter's V.90 arm, status 1 and 2. */
		c.role = 0x65;
		c.status = 1;
		run_rate_case(&c, RARM_RX_CFG, RARM_TX_V90, 3915);
		c.status = 2;
		run_rate_case(&c, RARM_RX_CFG, RARM_TX_V90, 3916);

		/* role 0x65, status 3: the receiver's pac18 arm. */
		c.status = 3;
		run_rate_case(&c, RARM_RX_PAC18, RARM_TX_CFG, 3917);
	}
	rc |= diff_end();

	/*
	 * THE THREE GATES, EACH SWEPT PAST ITS BOUNDARY.
	 *
	 * The demodulator's is a BYTE tested `cmpb $0x0`, so any non-zero
	 * value opens it and 0x80 is what separates that from a `signed char
	 * > 0` reading.  The two transmit gates are `int`s tested against
	 * exactly 3, so 2 and 4 must both close them -- a `>=` or a `!= 0`
	 * reconstruction passes every sample that is 0 or 3.
	 *
	 * AND A CLOSED TRANSMIT GATE RETURNS ZERO rather than falling through
	 * to the configuration, which is the one thing about these two arms a
	 * plausible version gets wrong: 0 and 21600 are different answers and
	 * only one of them is the object's.
	 */
	diff_begin("v34 pcm interface: the three rate gates");
	{
		static const int gate_v[] = { 0, 1, 2, 0x7f, 0x80, 0xff };
		static const int st_v[] = {
			(-0x7fffffff - 1), 0, 1, 2, 3, 4, 0x7fffffff
		};
		struct rate_case c;
		unsigned i;

		for (i = 0; i < sizeof(gate_v) / sizeof(gate_v[0]); i++) {
			c = rate_base;
			c.role = 0x66;
			c.status = 1;
			c.gate = gate_v[i];
			run_rate_case(&c, gate_v[i] != 0 ? RARM_RX_DEMOD : 0,
				      RARM_TX_CFG, 3920 + (long)i);
		}

		for (i = 0; i < sizeof(st_v) / sizeof(st_v[0]); i++) {
			/* The V.90 transmit gate, role 0x65 status 1. */
			c = rate_base;
			c.role = 0x65;
			c.status = 1;
			c.v90state = st_v[i];
			run_rate_case(&c, RARM_RX_CFG,
				      st_v[i] == 3 ? RARM_TX_V90 : 0,
				      3930 + (long)i);

			/* And the V.92 one, role 0x66 status 2. */
			c = rate_base;
			c.role = 0x66;
			c.status = 2;
			c.v92state = st_v[i];
			run_rate_case(&c, RARM_RX_DEMOD,
				      st_v[i] == 3 ? RARM_TX_V92 : 0,
				      3940 + (long)i);
		}
	}
	rc |= diff_end();

	/*
	 * THE TWO FRAME GRANULARITIES, AND THE ROUNDING.
	 *
	 * 8000/6 for the V.90 arms and 8000/12 for the V.92 one, both as a
	 * multiply by the nearest `float` to the reciprocal with 0.5 added
	 * before the truncation.  8000 mod 6 is 2 and 8000 mod 12 is 8, so
	 * the exact quotient's fraction is 0, 1/3 or 2/3 in both cases and
	 * only the 2/3 residue rounds up -- dropping the constant is right
	 * for two thirds of the inputs and wrong by one for the rest, which
	 * is why every residue is present.  A version that used 1/6 where the
	 * object uses 1/12 is out by a factor of two and caught by any of
	 * them.
	 *
	 * THE COUNT IS UNSIGNED, and the sweep goes past where that starts to
	 * matter: `n * 8000` crosses 2^31 at n = 268435.
	 */
	diff_begin("v34 pcm interface: the two PCM frame granularities");
	{
		static const unsigned int n_v[] = {
			0u, 1u, 2u, 3u, 21u, 22u, 23u, 42u, 43u,
			268434u, 268435u, 268436u, 0x7fffffffu, 0xffffffffu
		};
		/* n * 8000 / 6, rounded, for the first nine of those. */
		static const long v90_bps[] = {
			0, 1333, 2667, 4000, 28000, 29333, 30667, 56000,
			57333, -1, -1, -1, -1, -1
		};
		/* And n * 8000 / 12. */
		static const long v92_bps[] = {
			0, 667, 1333, 2000, 14000, 14667, 15333, 28000,
			28667, -1, -1, -1, -1, -1
		};
		struct rate_case c;
		unsigned i;

		for (i = 0; i < sizeof(n_v) / sizeof(n_v[0]); i++) {
			c = rate_base;
			c.role = 0x65;
			c.status = 1;
			c.v90bits = n_v[i];
			run_rate_case(&c, RARM_RX_CFG, v90_bps[i],
				      3960 + (long)i);

			c = rate_base;
			c.role = 0x66;
			c.status = 2;
			c.v92bits = n_v[i];
			run_rate_case(&c, RARM_RX_DEMOD, v92_bps[i],
				      3980 + (long)i);

			/*
			 * And the receiver's own, which is the same 8000/6
			 * inside `V90Demodulator::getBitRate` reached through
			 * two more pointers.
			 */
			c = rate_base;
			c.role = 0x66;
			c.status = 1;
			c.mpar = n_v[i];
			run_rate_case(&c, v90_bps[i], RARM_TX_CFG,
				      4000 + (long)i);
		}
	}
	rc |= diff_end();

	/*
	 * AND THE CONFIGURATION ARM, WHICH IS SIGNED.
	 *
	 * `rxbits` and `txbits` are `short` and the object loads both with
	 * `movswl`, so 0x8000 gives -32768 * 2400 and not 32768 * 2400.  The
	 * two readings agree over every rate index a session can hold -- 0 to
	 * 14 -- so this is the only place the declared type is observable,
	 * finding F1102's three-check case again.  The extremes overflow the
	 * multiply; both sides overflow identically and the value is compared
	 * for agreement rather than against a hand figure.
	 */
	diff_begin("v34 pcm interface: the rate configuration arm is signed");
	{
		static const short bits_v[] = {
			0, 1, 7, 14, -1, 0x7fff, (short)0x8000,
			(short)0xffff, (short)0x8001
		};
		struct rate_case c;
		unsigned i;

		for (i = 0; i < sizeof(bits_v) / sizeof(bits_v[0]); i++) {
			c = rate_base;
			c.role = 0x65;
			c.status = 0;
			c.rxbits = bits_v[i];
			c.txbits = (short)-bits_v[i];
			run_rate_case(&c, bits_v[i] >= 0
				      ? (long)bits_v[i] * 2400 : -1,
				      -1, 4020 + (long)i);

			/*
			 * AND THE `pac18` ARM, whose value is an int and is
			 * returned whole -- no scaling, which is what makes it
			 * distinguishable from the configuration one.
			 */
			c = rate_base;
			c.role = 0x65;
			c.status = 3;
			c.pac18 = (int)bits_v[i] * 7 + 1;
			run_rate_case(&c, (long)((int)bits_v[i] * 7 + 1) >= 0
				      ? (long)((int)bits_v[i] * 7 + 1) : -1,
				      -1, 4040 + (long)i);
		}
	}
	rc |= diff_end();

	/* --- the two questions asked of the PCM configuration ------------ */

	/*
	 * THE FOUR SELECTORS ARE CROSSED, NOT SAMPLED.  Which arm either
	 * function takes is decided by `+0x6120`, `v90_receiver`,
	 * `k56flex_receiver` and the PCM object's `+0x4f8`, and the object
	 * does not combine them with a single operator: the V.90 test SKIPS
	 * the K56Flex one rather than being or-ed with it, and only the case
	 * where V.90 is up, `+0x4f8` is clear and K56Flex is running tells an
	 * `&&` from an `||`.  So all four are driven together, over values
	 * that separate `!= 0` from `> 1` and from `== 1`.
	 *
	 * The reduction is swept across both edges of its clamp and both
	 * sides of zero, because -10 and +7 are answers rather than
	 * saturations and the sign is what picks the echo constants.
	 */
	diff_begin("v34 pcm interface: GetVPcmMinimalTxPowerReduction, "
		   "every arm against every clamp edge");
	{
		static const int gate_in[] = { 0, 1, -1 };
		static const int v90_in[] = { 0, 1, 2, -1, 0x7fffffff };
		static const int k56_in[] = { 0, 1, -1 };
		static const int sens_in[] = { 0, 1, -1 };
		static const short want_in[] = {
			(short)0x8000, -1000, -11, -10, -9, -1, 0, 1,
			6, 7, 8, 1000, 0x7fff
		};
		static const int flag_in[] = { 0, 3, 4, 5, -4, 0x7fffffff };
		unsigned g, v, k, s, w, f;
		long tag = 30000;

		for (g = 0; g < sizeof(gate_in) / sizeof(gate_in[0]); g++)
		for (v = 0; v < sizeof(v90_in) / sizeof(v90_in[0]); v++)
		for (k = 0; k < sizeof(k56_in) / sizeof(k56_in[0]); k++)
		for (s = 0; s < sizeof(sens_in) / sizeof(sens_in[0]); s++)
		for (w = 0; w < sizeof(want_in) / sizeof(want_in[0]); w++) {
			struct pwr_case c = pwr_base;

			c.gate = gate_in[g];
			c.v90 = v90_in[v];
			c.k56 = k56_in[k];
			c.sens = sens_in[s];
			c.want = want_in[w];
			run_pwr(&c, tag++);
		}

		/* And +0x54, which only the negative arm consults. */
		for (f = 0; f < sizeof(flag_in) / sizeof(flag_in[0]); f++)
		for (w = 0; w < sizeof(want_in) / sizeof(want_in[0]); w++) {
			struct pwr_case c = pwr_base;

			c.flag54 = flag_in[f];
			c.want = want_in[w];
			run_pwr(&c, tag++);
		}

		diff_eq_int("some case took the positive arm", saw_pwr_high,
			    1, 0);
		diff_eq_int("some case took the zero-or-negative arm",
			    saw_pwr_low, 1, 0);
		diff_eq_int("and some case was clamped", saw_pwr_clamped, 1, 0);
	}
	rc |= diff_end();

	/*
	 * THE SECOND REPORT PRINTS THE PCM FLAG, AND `sens` AND `k56` ARE
	 * SWEPT UNDER IT FOR THAT REASON ALONE.  The flag is `red > 0 ? 0 :
	 * 1` and `sens` is an input eight bytes further on, and in the
	 * obvious cases they are EQUAL -- with K56Flex off, `sens` of 0 gives
	 * a flag of 0 and `sens` of 1 gives a flag of 1.  A reconstruction
	 * printing +0x4f8 instead of +0x4f4 survives all of those, and the
	 * byte comparison cannot see it because the argument is not stored
	 * anywhere.  What separates them is K56Flex running, or a `sens` that
	 * is neither 0 nor 1.
	 */
	diff_begin("v34 pcm interface: GetVPcmMinimalTxPowerReduction "
		   "names the echo constants it set");
	{
		static const short want_in[] = { -10, -1, 0, 1, 7 };
		static const int flag_in[] = { 4, 5 };
		static const int sens_in[] = { 0, 1, -1 };
		static const int k56_in[] = { 0, 1 };
		unsigned w, f, s, k;
		long tag = 40000;
		int saw_flag_unlike_sens = 0;

		dsplib_debug_capture_on = 1;
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;

		for (w = 0; w < sizeof(want_in) / sizeof(want_in[0]); w++)
		for (f = 0; f < sizeof(flag_in) / sizeof(flag_in[0]); f++)
		for (s = 0; s < sizeof(sens_in) / sizeof(sens_in[0]); s++)
		for (k = 0; k < sizeof(k56_in) / sizeof(k56_in[0]); k++) {
			struct pwr_case c = pwr_base;
			int flag;

			c.want = want_in[w];
			c.flag54 = flag_in[f];
			c.sens = sens_in[s];
			c.k56 = k56_in[k];
			dsplib_debug_capture_reset();
			run_pwr(&c, tag);
			diff_eq_int("MinimalTxPowerReduction transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("and it said something",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    tag);
			memcpy(&flag, pcm_a + PCM_FLAG, sizeof(flag));
			if (flag != c.sens)
				saw_flag_unlike_sens = 1;
			tag++;
		}

		diff_eq_int("and some case told the flag from the ISP bit",
			    saw_flag_unlike_sens, 1, 0);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	/*
	 * THE CAP IS SWEPT THROUGH ITS OWN OVERFLOW.  `+0x4fc` is multiplied
	 * by 2400 in 32 bits with no widening, so 0x7fffffff wraps to a
	 * negative cap and the `>` then takes it -- which is the object's
	 * behaviour and not a case any real receiver reaches.  It is here
	 * because a reconstruction using a long, or clamping, differs only
	 * there.
	 */
	diff_begin("v34 pcm interface: VPcmV34GetMaxUpstreamRateIndex, "
		   "both ISPs and the multiply that wraps");
	{
		static const int gate_in[] = { 0, 1, -1 };
		static const int v90_in[] = { 0, 1, 2, -1, 0x7fffffff };
		static const int sens_in[] = { 0, 1, -1 };
		static const int cap_in[] = { 0, 1, 5, 13, 14, 15,
					      0x7fffffff, -1 };
		static const int rate_in2[] = { 0, 1, 2400, 31200, 33600,
						56000, 0x7fffffff, -1 };
		unsigned g, v, s, c2, r;
		long tag = 50000;

		for (g = 0; g < sizeof(gate_in) / sizeof(gate_in[0]); g++)
		for (v = 0; v < sizeof(v90_in) / sizeof(v90_in[0]); v++)
		for (s = 0; s < sizeof(sens_in) / sizeof(sens_in[0]); s++)
		for (c2 = 0; c2 < sizeof(cap_in) / sizeof(cap_in[0]); c2++)
		for (r = 0; r < sizeof(rate_in2) / sizeof(rate_in2[0]); r++) {
			struct pwr_case c = pwr_base;

			c.gate = gate_in[g];
			c.v90 = v90_in[v];
			c.sens = sens_in[s];
			c.cap = cap_in[c2];
			c.maxrate = rate_in2[r];
			run_rate(&c, tag++);
		}

		diff_eq_int("some case was capped", saw_rate_capped, 1, 0);
		diff_eq_int("and some case was not", saw_rate_plain, 1, 0);
	}
	rc |= diff_end();

	diff_begin("v34 pcm interface: GetMaxUpstreamRateIndex names its ISP");
	{
		static const int sens_in[] = { 0, 1 };
		unsigned s;

		dsplib_debug_capture_on = 1;
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;

		for (s = 0; s < sizeof(sens_in) / sizeof(sens_in[0]); s++) {
			struct pwr_case c = pwr_base;

			c.sens = sens_in[s];
			dsplib_debug_capture_reset();
			run_rate(&c, 60000 + (long)s);
			diff_eq_int("GetMaxUpstreamRateIndex transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, 60000 + (long)s);
			diff_eq_int("and it said something",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    60000 + (long)s);
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	/* --- chkForceBaudRate ------------------------------------------- */

	/*
	 * EXHAUSTIVE OVER THE CAP, BECAUSE IT CAN BE.  The index is three
	 * bits of one byte, so all eight values are reachable by
	 * construction and there is no argument to make about which are
	 * interesting: 0 clears nothing by a guard of its own, 1 through 5
	 * each clear one more rate, and 6 and 7 fall out of the chain.
	 *
	 * The low five bits are swept with them.  They are shifted out, so a
	 * reconstruction that read the byte as a whole -- or masked before
	 * shifting instead of after -- agrees on every case where they are
	 * zero and on none where they are not.
	 *
	 * THE TWO RECEIVER WORDS ARE DRIVEN SEPARATELY AND WITH VALUES THAT
	 * ARE NOT 1.  The object tests both with `test`/`je`, so any non-zero
	 * takes the arm; a reconstruction comparing `== 1` or `> 0` would
	 * pass on 1 and fail on -1, and one reading a short would pass on
	 * everything except 0x10000.
	 *
	 * AND THE SESSION'S FLAGS ARE SWEPT UNDER THEM.  On the V.90 arm the
	 * six bytes this function edits are ALSO what it reads back, so what
	 * they held before the call decides which bins get written.  The
	 * local arm cannot see that -- its array is seeded 1,1,1,1,1,x every
	 * time -- which is exactly why the two arms need different patterns
	 * rather than one.
	 */
	diff_begin("v34 pcm interface: chkForceBaudRate, every cap "
		   "against every arm");
	{
		static const int recv_in[][2] = {	/* v90, k56flex */
			{ 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 },
			{ 0, -1 }, { -1, 0 }, { 0, 0x10000 }, { 0x10000, 0 },
			{ 0, 0x7fffffff }, { 0x7fffffff, 0 }
		};
		static const unsigned char allow_in[][ALLOW_LEN] = {
			{ 0, 0, 0, 0, 0, 0 },
			{ 1, 1, 1, 1, 1, 1 },
			{ 1, 0, 1, 0, 1, 0 },
			{ 0, 1, 0, 1, 0, 1 },
			{ CHAIN_FILL, CHAIN_FILL, CHAIN_FILL,
			  CHAIN_FILL, CHAIN_FILL, CHAIN_FILL }
		};
		unsigned idx, low, r, a;

		for (idx = 0; idx < 8; idx++)
		for (low = 0; low < 2; low++)
		for (r = 0; r < sizeof(recv_in) / sizeof(recv_in[0]); r++)
		for (a = 0; a < sizeof(allow_in) / sizeof(allow_in[0]); a++)
			run_force((unsigned char)((idx << 5) | (low ? 0x1f : 0)),
				  recv_in[r][0], recv_in[r][1], allow_in[a],
				  10000 + (long)idx * 1000 + (long)low * 500
				  + (long)r * 10 + a);

		diff_eq_int("some case capped a rate", saw_force_capped, 1, 0);
		diff_eq_int("some case edited the session",
			    saw_force_session, 1, 0);
		diff_eq_int("and some case left it alone",
			    saw_force_local, 1, 0);
	}
	rc |= diff_end();

	diff_begin("v34 pcm interface: chkForceBaudRate says what it capped");
	{
		unsigned idx;

		dsplib_debug_capture_on = 1;
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;

		for (idx = 0; idx < 8; idx++) {
			static const unsigned char none[ALLOW_LEN] =
				{ 1, 1, 1, 1, 1, 1 };

			dsplib_debug_capture_reset();
			run_force((unsigned char)(idx << 5), 0, 0, none,
			      20000 + (long)idx);
			diff_eq_int("chkForceBaudRate transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, 20000 + (long)idx);
			diff_eq_int("and it said something",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    20000 + (long)idx);
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
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

			c.role = which[w];
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

			{
				static const unsigned char none[ALLOW_LEN] =
					{ 1, 1, 1, 1, 1, 1 };

				run_force(0x20, 0, 0, none,
					  9003 + (long)lvl * 10);
			}

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

	/* =================================================================
	 * THE PUBLIC ACCESSOR SURFACE.
	 */

	diff_begin("v34 pcm interface: Delete, SetMaxBlockLength and "
		   "SetIndicationOfRemoteRetrain");
	{
		static const int len_in[] = {
			0, 1, -1, 2, 0x1234, -0x1234, 0x7fffffff,
			(-0x7fffffff - 1)
		};
		static const unsigned char prior[] = {
			0, 1, 2, 0x7f, 0x80, 0xff, HARNESS_MALLOC_FILL
		};

		/*
		 * `VPcmV34Delete` is three instructions and returns zero.  The
		 * only thing there is to check is that it really is zero and
		 * that it really writes nothing -- a reconstruction that
		 * cleared a field on the way out would look identical from the
		 * return value.
		 */
		setup();
		diff_eq_int("Delete", VPcmV34Delete(&oa), ref_VPcmV34Delete(ob),
			    70000);
		diff_eq_int("...and it is zero", ref_VPcmV34Delete(ob), 0,
			    70000);
		compare("Delete stores nothing", 70000);

		/*
		 * SetMaxBlockLength is one store, swept past both ends of the
		 * signed range: the field is an `int` and a reconstruction
		 * that truncated it agrees on every plausible block length.
		 */
		for (i = 0; i < sizeof(len_in) / sizeof(len_in[0]); i++) {
			setup();
			poke_int(OB_PTC, 0x5a5a5a5a);
			VPcmV34SetMaxBlockLength(&oa, len_in[i]);
			ref_VPcmV34SetMaxBlockLength(ob, len_in[i]);
			compare("SetMaxBlockLength", 70100 + (long)i);
			diff_eq_int("SetMaxBlockLength stored the whole int",
				    get_int_a(OB_PTC), len_in[i],
				    70100 + (long)i);
		}

		/*
		 * And the retrain indication, whose prior value is swept
		 * because the store is unconditional: a reconstruction that
		 * OR-ed instead of assigning agrees on every case that starts
		 * at 0 or 1.
		 */
		for (i = 0; i < sizeof(prior) / sizeof(prior[0]); i++) {
			setup();
			((unsigned char *)&oa)[OB_AC17] = prior[i];
			ob[OB_AC17] = prior[i];
			VPcmV34SetIndicationOfRemoteRetrain(&oa);
			ref_VPcmV34SetIndicationOfRemoteRetrain(ob);
			compare("SetIndicationOfRemoteRetrain", 70200 + (long)i);
			diff_eq_int("SetIndicationOfRemoteRetrain set the byte",
				    (int)((unsigned char *)&oa)[OB_AC17], 1,
				    70200 + (long)i);
		}
	}
	rc |= diff_end();

	/*
	 * THE FOUR "CURRENT" GETTERS, ROLE CROSSED WITH STATUS.  Nothing less
	 * than the cross can tell the receive pair from the transmit pair:
	 * they differ only in which side of the role test each status offset
	 * sits on, so a swap agrees everywhere the two offsets coincide.
	 */
	diff_begin("v34 pcm interface: the four current getters, role crossed "
		   "with status");
	{
		static const short role_v[] = { 0x64, 0x65, 0x66 };
		static const int st_v[] = {
			-1, 0, 1, 2, 3, 4, 5, 6, (-0x7fffffff - 1), 0x7fffffff
		};
		unsigned r, s;

		for (r = 0; r < sizeof(role_v) / sizeof(role_v[0]); r++)
		for (s = 0; s < sizeof(st_v) / sizeof(st_v[0]); s++)
			run_getters(role_v[r], st_v[s], GBAUD, GCARRIER,
				    GRXBAUD, GRXCARRIER,
				    71000 + (long)(r * 100 + s));

		/*
		 * AND THE SAME CROSS WITH THE FOUR FIELDS SIGNED.  Each comes
		 * back through `movswl`, and 0x8000 is the one value that
		 * separates that from a `movzwl` -- the two readings agree
		 * over every rate a session can hold.
		 */
		for (r = 0; r < sizeof(role_v) / sizeof(role_v[0]); r++)
		for (s = 0; s < sizeof(st_v) / sizeof(st_v[0]); s++)
			run_getters(role_v[r], st_v[s], (short)0x8000, -1,
				    (short)0xffff, (short)0x8001,
				    71500 + (long)(r * 100 + s));

		diff_eq_int("the receive baud PCM arm was reached",
			    saw_getter_arm[0], 1, 71900);
		diff_eq_int("and its configured arm", saw_getter_arm[1], 1,
			    71901);
		diff_eq_int("the transmit baud PCM arm was reached",
			    saw_getter_arm[2], 1, 71902);
		diff_eq_int("and its configured arm", saw_getter_arm[3], 1,
			    71903);
		diff_eq_int("the receive carrier PCM arm was reached",
			    saw_getter_arm[4], 1, 71904);
		diff_eq_int("and its configured arm", saw_getter_arm[5], 1,
			    71905);
		diff_eq_int("the transmit carrier PCM arm was reached",
			    saw_getter_arm[6], 1, 71906);
		diff_eq_int("and its configured arm", saw_getter_arm[7], 1,
			    71907);
	}
	rc |= diff_end();

	/*
	 * GetQuickConnectIndication: the shift is only reached for 0..10
	 * UNSIGNED, so the sweep runs past both ends of that, and `is_short`
	 * is swept because one of the three arms returns it -- including
	 * negative, since it is read with `movswl`.
	 */
	diff_begin("v34 pcm interface: GetQuickConnectIndication, "
		   "every state and both signs of the answer");
	{
		static const short short_v[] = {
			0, 1, -1, 2, 0x7fff, (short)0x8000
		};
		int s;
		unsigned k;

		for (s = -2; s <= 13; s++)
		for (k = 0; k < sizeof(short_v) / sizeof(short_v[0]); k++)
			run_quickconnect(s, short_v[k],
					 72000 + (long)(s + 2) * 10 + k);

		run_quickconnect((-0x7fffffff - 1), 1, 72900);
		run_quickconnect(0x7fffffff, 1, 72901);

		diff_eq_int("the stored-answer arm was reached", saw_qc_arm[0],
			    1, 72910);
		diff_eq_int("the zero arm was reached", saw_qc_arm[1], 1,
			    72911);
		diff_eq_int("and the one arm", saw_qc_arm[2], 1, 72912);
	}
	rc |= diff_end();

	/*
	 * GetSNR: `equerr` over the whole of its guard and `sig_energy` over
	 * quotients that land either side of the hand-off between the two
	 * loops.  The 6 dB loop runs the ratio down to below 4 and the 1 dB
	 * loop finishes it, and the value it hands over is the LAST one that
	 * was still positive -- so 3, 4 and 5 are three different answers and
	 * a version that handed over the post-multiply value is out by six
	 * across the whole range.
	 */
	diff_begin("v34 pcm interface: GetSNR, both loops and the hand-off");
	{
		static const short eq_v[] = {
			(short)0x8000, -1000, -1, 0, 1, 2, 3, 8, 1000, 0x7fff
		};
		static const int num_v[] = {
			(-0x7fffffff - 1), -1000000, -1, 0, 1, 2, 3, 4, 5, 6,
			7, 100, 1000, 1000000, 100000000, 0x7fffffff
		};
		static const int q_v[] = {
			0, 1, 2, 3, 4, 5, 6, 7, 100, 1000, 1000000
		};
		unsigned e, n;
		long tag = 73000;

		for (e = 0; e < sizeof(eq_v) / sizeof(eq_v[0]); e++)
		for (n = 0; n < sizeof(num_v) / sizeof(num_v[0]); n++)
			run_snr(eq_v[e], num_v[n], tag++);

		/*
		 * AND THE EXACT QUOTIENTS, which the raw numerators above only
		 * reach for `equerr` of 1.  `sig_energy = q * equerr` puts the
		 * division's answer exactly on each of them.
		 */
		for (e = 0; e < sizeof(eq_v) / sizeof(eq_v[0]); e++) {
			if (eq_v[e] <= 0)
				continue;
			for (n = 0; n < sizeof(q_v) / sizeof(q_v[0]); n++)
				run_snr(eq_v[e], q_v[n] * (int)eq_v[e], tag++);
		}

		diff_eq_int("some case answered zero", saw_snr_zero, 1, 73900);
		diff_eq_int("some case ran the 6 dB loop", saw_snr_coarse, 1,
			    73901);
		diff_eq_int("and some case ran the 1 dB loop", saw_snr_fine, 1,
			    73902);
	}
	rc |= diff_end();

	/*
	 * NotifyDP: four codes and a default, swept from -2 to 6 so that the
	 * `jle` into the zero test is driven on both sides.  The two fields
	 * case 2 writes are seeded to values that are neither 5 nor 0, and
	 * `moh_timer` is swept because case 3's deadline is computed from it
	 * -- including past the point where `+ 48000` overflows.
	 */
	diff_begin("v34 pcm interface: NotifyDP, every code and the default");
	{
		static const int moh_v[] = {
			0, 1, -1, 48000, 0x7fff0000, 0x7fffffff,
			(-0x7fffffff - 1)
		};
		int w;
		unsigned k;

		for (w = -2; w <= 6; w++)
		for (k = 0; k < sizeof(moh_v) / sizeof(moh_v[0]); k++)
			run_notify(w, moh_v[k], 3, 0x1234, (short)0x4321,
				   74000 + (long)(w + 2) * 10 + k);

		for (k = 0; k < 5; k++)
			diff_eq_int("every NotifyDP arm was reached",
				    saw_notify_arm[k], 1, 74900 + (long)k);
	}
	rc |= diff_end();

	/*
	 * RequestDPNotification: the flag is swept over both signs and both
	 * extremes, the other two words carry values distinct from it and
	 * from each other, and the configuration byte is driven with bit 0
	 * both set and clear -- the fill pattern only ever gives it set.
	 */
	diff_begin("v34 pcm interface: RequestDPNotification, the empty "
		   "mailbox writes nothing");
	{
		static const int flag_v[] = {
			(-0x7fffffff - 1), -1000, -2, -1, 0, 1, 2, 1000,
			0x7fffffff
		};
		static const unsigned char cfg_v[] = {
			0x00, 0x01, 0xfe, 0xff, 0x71, 0x70
		};
		unsigned f, k;

		for (f = 0; f < sizeof(flag_v) / sizeof(flag_v[0]); f++)
		for (k = 0; k < sizeof(cfg_v) / sizeof(cfg_v[0]); k++)
			run_reqdp(flag_v[f], 0x0badf00d, 0x0c0ffee0, cfg_v[k],
				  75000 + (long)f * 10 + k);

		diff_eq_int("some case found the mailbox empty",
			    saw_reqdp_empty, 1, 75900);
		diff_eq_int("and some case found it full", saw_reqdp_full, 1,
			    75901);
		diff_eq_int("and some full case started with the bit clear",
			    saw_reqdp_bit_was_clear, 1, 75902);
	}
	rc |= diff_end();

	/*
	 * V34XF_GetMaxUpstreamRateIndex, swept exactly as the other prefix is
	 * -- the same five inputs across the same two blocks.
	 */
	diff_begin("v34 pcm interface: V34XF_GetMaxUpstreamRateIndex, "
		   "both ISPs and the multiply that wraps");
	{
		static const int gate_in[] = { 0, 1, -1 };
		static const int v90_in[] = { 0, 1, 2, -1, 0x7fffffff };
		static const int sens_in[] = { 0, 1, -1 };
		static const int cap_in[] = { 0, 1, 5, 13, 14, 15,
					      0x7fffffff, -1 };
		static const int rate_in2[] = { 0, 1, 2400, 31200, 33600,
						56000, 0x7fffffff, -1 };
		unsigned g, v, s, c2, r;
		long tag = 76000;

		for (g = 0; g < sizeof(gate_in) / sizeof(gate_in[0]); g++)
		for (v = 0; v < sizeof(v90_in) / sizeof(v90_in[0]); v++)
		for (s = 0; s < sizeof(sens_in) / sizeof(sens_in[0]); s++)
		for (c2 = 0; c2 < sizeof(cap_in) / sizeof(cap_in[0]); c2++)
		for (r = 0; r < sizeof(rate_in2) / sizeof(rate_in2[0]); r++) {
			struct pwr_case c = pwr_base;

			c.gate = gate_in[g];
			c.v90 = v90_in[v];
			c.sens = sens_in[s];
			c.cap = cap_in[c2];
			c.maxrate = rate_in2[r];
			run_xfrate(&c, 1, tag++);
		}

		diff_eq_int("some case was capped", saw_xf_capped, 1, 76900);
		diff_eq_int("and some case was not", saw_xf_plain, 1, 76901);
	}
	rc |= diff_end();

	/*
	 * V34XF_IndicateK56FlexJdReceived.  Every real symbol rate, because
	 * `v34setuptxmit` picks the modulator's shaping and pre-emphasis
	 * tables from the rate and the carrier; the constellation argument
	 * across the `== 0x10` test rather than a `!= 0` one; the DC seed's
	 * input across both signs, since the store truncates to a short; and
	 * the transmit state both at 0x12 and away from it, which is the arm
	 * that stores nothing.
	 */
	diff_begin("v34 pcm interface: V34XF_IndicateK56FlexJdReceived, "
		   "the transmitter rebuild");
	{
		static const struct { short baud, carrier; } rate_v[] = {
			{ 2400, 1600 }, { 2400, 1800 }, { 3000, 1800 },
			{ 3200, 1920 }, { 3429, 1959 }
		};
		static const unsigned char cst_v[] = { 0, 1, 0x10, 0x11, 0xff };
		static const short seed_v[] = {
			0, 1, -1, 3, 100, -100, 0x7fff, (short)0x8000
		};
		static const short txst_v[] = { 0x12, 0x11, 0, V34HS_SSEG };
		unsigned r, c2, s, t;
		long tag = 77000;

		for (r = 0; r < sizeof(rate_v) / sizeof(rate_v[0]); r++)
		for (c2 = 0; c2 < sizeof(cst_v) / sizeof(cst_v[0]); c2++)
		for (t = 0; t < sizeof(txst_v) / sizeof(txst_v[0]); t++) {
			struct pwr_case c = pwr_base;

			run_k56jd(&c, cst_v[c2], 3, txst_v[t],
				  rate_v[r].baud, rate_v[r].carrier,
				  (short)(c2 % 3), (unsigned short)0x0000,
				  tag++);
		}

		for (s = 0; s < sizeof(seed_v) / sizeof(seed_v[0]); s++) {
			struct pwr_case c = pwr_base;

			run_k56jd(&c, 0x10, seed_v[s], 0x12, 3000, 1800, 0,
				  (unsigned short)0xffff, tag++);
			run_k56jd(&c, 0, seed_v[s], 0x11, 3000, 2000, 2,
				  (unsigned short)0x0800, tag++);
		}

		diff_eq_int("the non-0x10 constellation arm was reached",
			    saw_k56_constel[0], 1, 77900);
		diff_eq_int("and the 0x10 one", saw_k56_constel[1], 1, 77901);
		diff_eq_int("the transmit state was moved at least once",
			    saw_k56_txstate[0], 1, 77902);
		diff_eq_int("and left alone at least once", saw_k56_txstate[1],
			    1, 77903);

		for (i = NK56FIXTURE; i < NK56PTR; i++)
			diff_eq_int("K56flex Jd: pointer field was installed",
				    saw_k56_ptr[i], 1, (long)k56_ptr_skip[i]);
	}
	rc |= diff_end();

	/*
	 * AND ALL OF THEM WITH THE DIAGNOSTICS LIVE.  Four of the thirteen
	 * print, and finding F134 is why that needs its own pass: a dropped
	 * call site is invisible to every state comparison in this file.
	 */
	diff_begin("v34 pcm interface: the accessor surface, transcripts too");
	{
		static const int sens_in[] = { 0, 1 };
		int w;
		unsigned k;
		long tag = 78000;

		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		for (k = 0; k < 4; k++) {
			static const int len_in[] = { 0, 1, -1, 0x7fffffff };

			dsplib_debug_capture_reset();
			setup();
			VPcmV34SetMaxBlockLength(&oa, len_in[k]);
			ref_VPcmV34SetMaxBlockLength(ob, len_in[k]);
			compare("SetMaxBlockLength, logging", tag);
			diff_eq_int("SetMaxBlockLength transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("and it said something",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    tag);
			tag++;
		}

		/* Every NotifyDP arm, because each prints its own line. */
		for (w = -1; w <= 4; w++) {
			dsplib_debug_capture_reset();
			run_notify(w, 12345, 3, 0x1234, (short)0x4321, tag);
			diff_eq_int("NotifyDP transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			if (w >= 0 && w <= 3)
				diff_eq_int("and the coded arms said something",
					    dsplib_debug_capture_text(1)[0] != 0,
					    1, tag);
			tag++;
		}

		/* Both arms of the upstream rate cap name themselves. */
		for (k = 0; k < sizeof(sens_in) / sizeof(sens_in[0]); k++) {
			struct pwr_case c = pwr_base;

			c.sens = sens_in[k];
			dsplib_debug_capture_reset();
			run_xfrate(&c, 0, tag);
			diff_eq_int("V34XF_GetMaxUpstreamRateIndex transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("and it said something",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    tag);
			tag++;
		}

		/*
		 * And the transmitter rebuild, whose own line prints three
		 * fields of the rate configuration and whose callees print
		 * two state transitions.
		 */
		for (k = 0; k < 3; k++) {
			struct pwr_case c = pwr_base;

			dsplib_debug_capture_reset();
			run_k56jd(&c, (unsigned char)(k == 1 ? 0x10 : k), 3,
				  (short)(k == 2 ? 0x12 : 0x11), 3000, 1800,
				  (short)k, (unsigned short)0x0000, tag);
			diff_eq_int("IndicateK56FlexJdReceived transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("and it said something",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    tag);
			tag++;
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	/*
	 * AND THE GATE, which the section above is structurally blind to: it
	 * raises the level first, so a call site that lost its
	 * `if (DSPLIB_DEBUG_ON())` prints the same thing and passes.  Both
	 * levels below the threshold, because every gate in the object is
	 * `> 1` and 1 is the only value that separates it from `>= 1`.
	 */
	diff_begin("v34 pcm interface: the accessor surface is silent below "
		   "the threshold");
	{
		unsigned lvl;

		dsplib_debug_capture_on = 1;

		for (lvl = 0; lvl <= 1; lvl++) {
			struct pwr_case c = pwr_base;
			int w;

			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;
			dsplib_debug_capture_reset();

			setup();
			VPcmV34SetMaxBlockLength(&oa, 1234);
			ref_VPcmV34SetMaxBlockLength(ob, 1234);
			for (w = 0; w <= 3; w++)
				run_notify(w, 12345, 3, 0x1234, (short)0x4321,
					   79000 + (long)lvl * 100 + w);
			run_xfrate(&c, 1, 79010 + (long)lvl * 100);
			run_k56jd(&c, 0x10, 3, 0x11, 3000, 1800, 0,
				  (unsigned short)0, 79020 + (long)lvl * 100);
			run_getters(0x66, 1, GBAUD, GCARRIER, GRXBAUD,
				    GRXCARRIER, 79030 + (long)lvl * 100);
			run_quickconnect(3, 1, 79040 + (long)lvl * 100);
			run_snr(8, 800000, 79050 + (long)lvl * 100);
			run_reqdp(1, 2, 3, 0x71, 79060 + (long)lvl * 100);

			diff_eq_int("ours printed nothing",
				    dsplib_debug_capture_text(0)[0], 0,
				    79000 + (long)lvl);
			diff_eq_int("and neither did the reference",
				    dsplib_debug_capture_text(1)[0], 0,
				    79000 + (long)lvl);
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
		for (k = NFIXTURE_PTR; k < NPTR; k++)
			diff_eq_int("pointer field was installed at least once",
				    saw_ptr_written[k], 1, (long)ptr_skip[k]);
		diff_eq_int("and the session chain was walked",
			    saw_chain_walked, 1, (long)ptr_skip[0]);
		diff_eq_int("and the configuration object was read",
			    saw_force_capped, 1, (long)ptr_skip[1]);
	}
	rc |= diff_end();

	return rc;
}
