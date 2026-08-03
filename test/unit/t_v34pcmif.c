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
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34pcmif.h"

extern unsigned int ref_dsplibs_debug_level;

extern void ref_VPcmV34LogTimingOffset(void *obj, short offset);
extern double *ref_V34XF_GetProbeResultsPtr(void *obj);
extern int *ref_V34XF_GetInfo0BitsPtr(void *obj);
extern short ref_V34XF_GetRTD(void *obj);
extern void ref_V34XF_IndicateJdReceived(void *obj, unsigned char constel,
					 unsigned char silence_scr);
extern void ref_V34XF_IndicateDilReceived(void *obj, unsigned char constel);
extern void ref_V34XF_IndicateTrn2dReceived(void *obj);
extern void ref_V34XF_IndicateK56FlexRateDetermined(void *obj);

/*
 * Ours is the mapped struct, the blob's is raw bytes of the same length.
 * Statics, not locals: 44 KB each and two of them would be a large stack
 * frame on a 32-bit target.
 */
static struct v34_object oa;
static unsigned char ob[sizeof(struct v34_object)];

static void
setup(void)
{
	memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
	memset(ob, HARNESS_MALLOC_FILL, sizeof(ob));
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
	unsigned i;

	for (i = 0; i < sizeof(oa); i++)
		if (p[i] != ob[i])
			diff_eq_int(what, p[i], ob[i],
				    (long)i * 1000 + tag);

	/*
	 * The loop above only reports differences, so an all-equal run
	 * records no check at all.  Count one, or a comparison that silently
	 * stopped comparing would look like a pass.
	 */
	diff_eq_int(what, memcmp(&oa, ob, sizeof(oa)) == 0, 1, tag);
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

	return rc;
}
