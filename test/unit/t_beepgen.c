/*
 * t_beepgen.c -- differential tests for the `Beepgen.c` span's no-caller
 * leaves: the DTMF table, the gain query, the validity checks, and the
 * float/linear utility set.
 *
 * Nothing in the blob calls any of these (they are exported API surface
 * with no internal referrer), so every input here is constructed.  All
 * float comparisons are EXACT -- both sides run x87 with the same shapes,
 * and a tolerance would only hide a wrong reconstruction.
 *
 * GetGain is LOCAL in the blob, so its ref_ alias is called with the
 * regparm(2) convention GCC 3.4 gives static functions; our copy has
 * external linkage and the ordinary convention.  Same trade as
 * t_dialstring.c documents.
 */

#include <stdio.h>
#include <string.h>

#include <stddef.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/beepgen.h"
#include "dsplib/detector.h"
#include "dsplib/sysdep.h"
#include "dsplib/dtmf.h"
#include "dsplib/modem_params.h"

extern unsigned int ref_dsplibs_debug_level;

/* Per-side debug transcripts; see test/harness/runtime.c. */
extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
unsigned dsplib_debug_capture_lines(int side);
const char *dsplib_debug_capture_text(int side);

extern void ref_beepgen_get_freqs(unsigned char code, int *colp, int *rowp);
extern void ref_GetGain(struct beepgen *bg, float *gain1, float *gain2)
	__attribute__((regparm(2)));
extern int ref_check_for_valid(unsigned short *w);
extern int ref_check_for_valid_easy(unsigned short *w);
extern void ref_zFLTUTL_Float2Linear(float *src, short *dst, int n,
				     float gain);
extern void ref_zFLTUTL_Linear2Float(short *src, float *dst, int n,
				     float gain);
extern float ref_fComputeRMSValueFloatBuf(unsigned int n, float *buf);
extern float ref_fComputeRMSValueShortBuf(unsigned int n, short *buf);
extern void ref_CrossDataLinks(short *lin_in, float *flt_out, float *flt_in,
			       short *lin_out, int n);
extern int ref_bSearchEnergy(short *new1, short *new2, short *buf1,
			     short *buf2, unsigned int fresh,
			     unsigned int total);
extern int ref_FindCorrelation(short *pattern, short *sig,
			       unsigned int *posp, unsigned int len,
			       unsigned int *peakp, unsigned int *peakposp,
			       short *out);
extern float ref_zfFLTUTL_GetMaxAbsValue(float *buf, unsigned int n);

extern struct beepgen *ref_beepgen_create(struct beepgen *bg,
					  const struct beepgen_config *cfg);
extern void ref_beepgen_delete(struct beepgen *bg);
extern void ref_beepgen_start_beep(struct beepgen *bg, int freq1, int freq2,
				   int duration);
extern void ref_beepgen_start_dtmf(struct beepgen *bg, int code,
				   int duration);
extern int ref_beepgen_sample(struct beepgen *bg, float *out);
extern void ref_detector_set_enable(struct detector *d, short enable);
extern void ref_detector_set_output_status(struct detector *d);
extern void ref_detector_set_output_in_stream(struct detector *d);
extern struct dtmf *ref_create_dtmf(struct dtmf *d);
extern int ref_FDSP_DP_Run(int *status, short *rx_lin, float *rx_flt,
			   float *tx_flt, short *tx_lin,
			   unsigned short *hostcount,
			   unsigned short *countp);

/*
 * The three host callbacks the generator reaches through its config block.
 * BOTH SIDES GET THE SAME FUNCTIONS AND THE SAME MODEM COOKIE, so the two
 * objects stay byte-comparable; what tells the sides apart is that the ref
 * call is made first and its trace snapshotted before ours runs.
 *
 * `what` is recorded as well as the fact of the call, because the 24 that
 * beepgen_start_dtmf passes is otherwise unobservable.
 */
#define TRACE_MAX	64
#define TRACE_FN_011C	1
#define TRACE_HOOK_ON	2
#define TRACE_DURATION	3

static int trace_tag[TRACE_MAX];
static int trace_n;
static int stub_duration = 7;
static int modem_cookie;

static void
trace_add(int tag)
{
	if (trace_n < TRACE_MAX)
		trace_tag[trace_n] = tag;
	trace_n++;
}

static void
cb_fn_011c(void *modem)
{
	trace_add(modem == (void *)&modem_cookie ? TRACE_FN_011C : -1);
}

static void
cb_hook_on(void *modem)
{
	trace_add(modem == (void *)&modem_cookie ? TRACE_HOOK_ON : -1);
}

static int
cb_duration(void *modem, int what)
{
	trace_add(modem == (void *)&modem_cookie ? TRACE_DURATION : -1);
	trace_add(what);
	return stub_duration;
}

static const struct beepgen_config beep_cfg = {
	&modem_cookie, cb_fn_011c, cb_hook_on, cb_duration
};

/* A config with every callback absent -- the NULL arms of all three sites. */
static const struct beepgen_config beep_cfg_null = { &modem_cookie, 0, 0, 0 };

static int snap_n[2];
static int snap_tag[2][TRACE_MAX];

static void
trace_reset(void)
{
	int i;

	trace_n = 0;
	for (i = 0; i < TRACE_MAX; i++)
		trace_tag[i] = 0;
}

static void
trace_snap(int side)
{
	int i;

	snap_n[side] = trace_n;
	for (i = 0; i < TRACE_MAX; i++)
		snap_tag[side][i] = trace_tag[i];
}

/* 0 == ref, 1 == ours.  Compares length first, then every recorded tag. */
static void
trace_compare(const char *what, long input)
{
	int i;

	diff_eq_int(what, snap_n[1], snap_n[0], input);
	for (i = 0; i < TRACE_MAX && i < snap_n[0] && i < snap_n[1]; i++)
		diff_eq_int("callback tag", snap_tag[1][i], snap_tag[0][i],
			    input);
}

/*
 * Both objects start from the same fixed non-zero fill, so a field the code
 * under test does not touch is compared too, and an accidental extra store
 * is a failure rather than a coincidence.
 */
static void
beep_pair_create(struct beepgen *a, struct beepgen *b,
		 const struct beepgen_config *cfg)
{
	memset(a, 0x5a, sizeof(*a));
	memset(b, 0x5a, sizeof(*b));
	ref_beepgen_create(a, cfg);
	beepgen_create(b, cfg);
}

/* Deterministic 32-bit LCG; both sides always see the same stream. */
static unsigned int lcg_state = 0x1234567u;
static unsigned int
lcg(void)
{
	lcg_state = lcg_state * 1664525u + 1013904223u;
	return lcg_state;
}

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
	unsigned int i, n, g, c;

	set_level(0);

	/*
	 * SECTION 1 -- beepgen_get_freqs over every byte, both at silence
	 * and at debug level 2 so the '!' arm's gate runs on both sides.
	 */
	diff_begin("beepgen_get_freqs, all 256 codes x 2 levels");
	for (g = 0; g < 2; g++) {
		set_level(g ? 2 : 0);
		for (c = 0; c < 256; c++) {
			int c1 = -7, r1 = -7, c2 = -9, r2 = -9;

			ref_beepgen_get_freqs((unsigned char)c, &c1, &r1);
			beepgen_get_freqs((unsigned char)c, &c2, &r2);
			diff_eq_int("col(code %ld)", c2, c1, c);
			diff_eq_int("row(code %ld)", r2, r1, c);
		}
	}
	/*
	 * The '!' arm's transcript, compared line for line.  The expected
	 * count is asserted so a dead gate cannot pass as a quiet one.
	 */
	{
		int c1, r1, c2, r2;

		set_level(2);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		ref_beepgen_get_freqs('!', &c1, &r1);
		beepgen_get_freqs('!', &c2, &r2);
		diff_eq_int("'!' transcript lines ours",
			    dsplib_debug_capture_lines(0), 1, 0);
		diff_eq_int("'!' transcript lines ref",
			    dsplib_debug_capture_lines(1), 1, 0);
		diff_eq_int("'!' transcript text equal",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)), 0, 0);
		dsplib_debug_capture_on = 0;
	}
	set_level(0);
	failed |= diff_end();

	/*
	 * SECTION 2 -- the validity checks, exhaustively over the equality
	 * PATTERN: each window slot either repeats w[0] or differs from it.
	 * That is the whole decision space; the values themselves are
	 * varied but cannot matter beyond equality.
	 */
	diff_begin("check_for_valid(_easy), exhaustive equality patterns");
	for (i = 0; i < 128; i++) {
		unsigned short w[7];
		unsigned int b;
		unsigned short base = (unsigned short)(lcg() & 0xffff);

		w[0] = base;
		for (b = 1; b < 7; b++)
			w[b] = (i & (1u << b))
			    ? base : (unsigned short)(base + 1 + b);
		diff_eq_int("valid(pattern 0x%02lx)",
			    check_for_valid(w), ref_check_for_valid(w), i);
		diff_eq_int("easy(pattern 0x%02lx)",
			    check_for_valid_easy(w),
			    ref_check_for_valid_easy(w), i);
	}
	failed |= diff_end();

	/*
	 * SECTION 3 -- the conversion utilities.  The gain list includes
	 * exactly 0.0f (the "off" arm), both signs, and the two scales
	 * CrossDataLinks hardwires; the sample list includes values whose
	 * scaled product leaves the short range, because the truncating
	 * store's out-of-range result is part of the behaviour.
	 */
	diff_begin("zFLTUTL conversions and CrossDataLinks");
	{
		static const float gains[] = {
			0.0f, 1.0f, -1.0f, 0.5f, -2.0f,
			32000.0f, 1.0f / 32000.0f, 0.9999f,
		};
		for (g = 0; g < sizeof(gains) / sizeof(gains[0]); g++) {
			float fsrc[33], fdst_a[33], fdst_b[33];
			float fmid_a[33], fmid_b[33];
			short ssrc[33], sdst_a[33], sdst_b[33];
			short smid_a[33], smid_b[33];
			int n3 = 33;

			for (i = 0; i < 33; i++) {
				ssrc[i] = (short)(lcg() & 0xffff);
				fsrc[i] = (int)(lcg() % 70001) - 35000
				    + (int)(lcg() % 1000) * 0.001f;
			}
			memset(sdst_a, 0x11, sizeof(sdst_a));
			memcpy(sdst_b, sdst_a, sizeof(sdst_a));
			memset(fdst_a, 0x22, sizeof(fdst_a));
			memcpy(fdst_b, fdst_a, sizeof(fdst_a));

			ref_zFLTUTL_Float2Linear(fsrc, sdst_a, n3, gains[g]);
			zFLTUTL_Float2Linear(fsrc, sdst_b, n3, gains[g]);
			for (i = 0; i < 33; i++)
				diff_eq_int("f2l[%ld]", sdst_b[i], sdst_a[i],
					    (long)(g * 100 + i));

			ref_zFLTUTL_Linear2Float(ssrc, fdst_a, n3, gains[g]);
			zFLTUTL_Linear2Float(ssrc, fdst_b, n3, gains[g]);
			for (i = 0; i < 33; i++)
				diff_eq_float("l2f[%ld]", fdst_b[i],
					      fdst_a[i], (long)(g * 100 + i));

			memset(smid_a, 0x33, sizeof(smid_a));
			memcpy(smid_b, smid_a, sizeof(smid_a));
			memset(fmid_a, 0x44, sizeof(fmid_a));
			memcpy(fmid_b, fmid_a, sizeof(fmid_a));
			ref_CrossDataLinks(ssrc, fmid_a, fsrc, smid_a, n3);
			CrossDataLinks(ssrc, fmid_b, fsrc, smid_b, n3);
			for (i = 0; i < 33; i++) {
				diff_eq_float("cross f[%ld]", fmid_b[i],
					      fmid_a[i], (long)i);
				diff_eq_int("cross s[%ld]", smid_b[i],
					    smid_a[i], (long)i);
			}
		}
		/* n = 0 and negative n leave everything untouched. */
		{
			float f1[2] = { 1.0f, 2.0f }, f2[2] = { 1.0f, 2.0f };
			short s1[2] = { 3, 4 }, s2[2] = { 3, 4 };

			ref_zFLTUTL_Float2Linear(f1, s1, 0, 1.0f);
			zFLTUTL_Float2Linear(f2, s2, 0, 1.0f);
			ref_zFLTUTL_Linear2Float(s1, f1, -3, 1.0f);
			zFLTUTL_Linear2Float(s2, f2, -3, 1.0f);
			ref_CrossDataLinks(s1, f1, f1, s1, 0);
			CrossDataLinks(s2, f2, f2, s2, 0);
			diff_eq_int("n0 s[0]", s2[0], s1[0], 0);
			diff_eq_float("n0 f[0]", f2[0], f1[0], 0);
		}
	}
	failed |= diff_end();

	/*
	 * SECTION 4 -- the two power computations and the max-abs scan,
	 * across sizes.  Exact float comparison: the accumulation order and
	 * the integer-vs-float mean are the things under test.
	 */
	diff_begin("fComputeRMSValue{Float,Short}Buf, zfFLTUTL_GetMaxAbsValue");
	for (n = 1; n <= 96; n = n * 2 + 1)
	for (g = 0; g < 8; g++) {
		float fb[200];
		short sb[200];

		for (i = 0; i < n; i++) {
			sb[i] = (short)(lcg() & 0xffff);
			fb[i] = (int)(lcg() % 60001) - 30000
			    + (int)(lcg() % 997) * 0.001f;
		}
		diff_eq_float("rms float n=%ld",
			      fComputeRMSValueFloatBuf(n, fb),
			      ref_fComputeRMSValueFloatBuf(n, fb),
			      (long)(n * 10 + g));
		diff_eq_float("rms short n=%ld",
			      fComputeRMSValueShortBuf(n, sb),
			      ref_fComputeRMSValueShortBuf(n, sb),
			      (long)(n * 10 + g));
		diff_eq_float("maxabs n=%ld",
			      zfFLTUTL_GetMaxAbsValue(fb, n),
			      ref_zfFLTUTL_GetMaxAbsValue(fb, n),
			      (long)(n * 10 + g));
	}
	/* all-negative and all-zero buffers pin the seed-and-negate arm */
	{
		float neg[5] = { -5.0f, -1.0f, -9.5f, -2.0f, -9.5f };
		float zer[3] = { 0.0f, 0.0f, 0.0f };

		diff_eq_float("maxabs neg", zfFLTUTL_GetMaxAbsValue(neg, 5),
			      ref_zfFLTUTL_GetMaxAbsValue(neg, 5), 0);
		diff_eq_float("maxabs zero", zfFLTUTL_GetMaxAbsValue(zer, 3),
			      ref_zfFLTUTL_GetMaxAbsValue(zer, 3), 0);
	}
	failed |= diff_end();

	/*
	 * SECTION 5 -- bSearchEnergy.  2000-sample windows; the fresh count
	 * sweeps the empty, partial and full-replacement arms; the content
	 * alternates quiet and loud so the 40000 threshold is crossed both
	 * ways; buf2[1] gates the verdict and is driven both ways too.
	 */
	diff_begin("bSearchEnergy");
	{
		static const unsigned int freshes[] = { 0, 1, 160, 999, 2000 };
		static short b1a[2000], b1b[2000], b2a[2000], b2b[2000];
		static short n1[2000], n2[2000];
		unsigned int f, loud, gate;

		/*
		 * buf2 carries the GATE, tested for zero/nonzero at one
		 * index -- so it is seeded sparse (0 or 1) rather than
		 * with values that are nonzero almost surely, or a moved
		 * gate index would read the same truth value everywhere.
		 */
		for (i = 0; i < 2000; i++) {
			b1a[i] = (short)(lcg() % 201) - 100;
			b2a[i] = (short)(lcg() % 2);
		}
		memcpy(b1b, b1a, sizeof(b1a));
		memcpy(b2b, b2a, sizeof(b2a));
		/*
		 * Three drive levels: variance*0.001 lands near 33, 1.3e4
		 * and 3.3e7, so the 40000 threshold is approached from both
		 * sides and a moved threshold cannot hide between them.
		 */
		for (loud = 0; loud < 3; loud++)
		for (gate = 0; gate < 2; gate++)
		for (f = 0; f < sizeof(freshes) / sizeof(freshes[0]); f++) {
			unsigned int fresh = freshes[f];
			static const int amp[3] = { 10, 200, 10000 };
			int ra, rb;

			for (i = 0; i < fresh; i++) {
				n1[i] = (short)((int)(lcg()
				    % (2 * amp[loud] + 1)) - amp[loud]);
				n2[i] = gate ? (short)(lcg() % 2) : 0;
			}
			/* keep the streams identical for both sides: the
			 * generator ran once, the buffers carry the data */
			ra = ref_bSearchEnergy(n1, n2, b1a, b2a, fresh, 2000);
			rb = bSearchEnergy(n1, n2, b1b, b2b, fresh, 2000);
			diff_eq_int("verdict f=%ld", rb, ra,
				    (long)(loud * 1000 + gate * 100 + f));
			for (i = 0; i < 2000; i++) {
				diff_eq_int("b1[%ld]", b1b[i], b1a[i],
					    (long)i);
				diff_eq_int("b2[%ld]", b2b[i], b2a[i],
					    (long)i);
			}
		}
		/*
		 * The gate index, pinned deterministically: loud energy
		 * with buf2[0] and buf2[1] holding OPPOSITE truth values,
		 * both ways round, at fresh = 0 so nothing slides.  The
		 * grid above can only reach this state by luck.
		 */
		for (i = 0; i < 2000; i++)
			b1a[i] = (short)((int)(lcg() % 20001) - 10000);
		memcpy(b1b, b1a, sizeof(b1a));
		memset(b2a, 0, sizeof(b2a));
		b2a[1] = 1;
		memcpy(b2b, b2a, sizeof(b2a));
		diff_eq_int("gate on [1] only",
			    bSearchEnergy(n1, n2, b1b, b2b, 0, 2000),
			    ref_bSearchEnergy(n1, n2, b1a, b2a, 0, 2000), 0);
		memset(b2a, 0, sizeof(b2a));
		b2a[0] = 1;
		memcpy(b2b, b2a, sizeof(b2a));
		diff_eq_int("gate on [0] only",
			    bSearchEnergy(n1, n2, b1b, b2b, 0, 2000),
			    ref_bSearchEnergy(n1, n2, b1a, b2a, 0, 2000), 0);
	}
	failed |= diff_end();

	/*
	 * SECTION 6 -- FindCorrelation: the pos==end early out, the +20
	 * clamp, the len-1000 ceiling, and the running peak across calls.
	 */
	diff_begin("FindCorrelation");
	{
		static short sig[3200], pattern[1000], outa[24], outb[24];
		unsigned int posa, posb, peaka, peakb, ppa, ppb;
		unsigned int caseno;
		static const struct {
			unsigned int pos, len;
		} cases[] = {
			{ 0, 3200 },	/* clamped to pos+20   */
			{ 20, 1050 },	/* end from len-1000   */
			{ 30, 1030 },	/* pos == end: ret 1   */
			{ 40, 1030 },	/* end < pos: no loop  */
			{ 40, 1055 },	/* short scan          */
		};

		for (i = 0; i < 3200; i++)
			sig[i] = (short)((lcg() % 2001) - 1000);
		for (i = 0; i < 1000; i++)
			pattern[i] = (short)((lcg() % 201) - 100);

		for (caseno = 0;
		     caseno < sizeof(cases) / sizeof(cases[0]); caseno++) {
			int ra, rb;

			/*
			 * The peak resets per case: left running, whichever
			 * case scores highest silences every later case's
			 * peak bookkeeping, and a wrong *peakposp in those
			 * cases would never be looked at.
			 */
			peaka = peakb = 0;
			ppa = ppb = 77;
			posa = posb = cases[caseno].pos;
			memset(outa, 0x5a, sizeof(outa));
			memcpy(outb, outa, sizeof(outa));
			ra = ref_FindCorrelation(pattern, sig, &posa,
						 cases[caseno].len, &peaka,
						 &ppa, outa);
			rb = FindCorrelation(pattern, sig, &posb,
					     cases[caseno].len, &peakb,
					     &ppb, outb);
			diff_eq_int("ret case %ld", rb, ra, caseno);
			diff_eq_int("pos case %ld", posb, posa, caseno);
			diff_eq_int("peak case %ld", peakb, peaka, caseno);
			diff_eq_int("peakpos case %ld", ppb, ppa, caseno);
			for (i = 0; i < 24; i++)
				diff_eq_int("out[%ld]", outb[i], outa[i],
					    (long)(caseno * 100 + i));
		}
	}
	failed |= diff_end();

	/*
	 * SECTION 7 -- GetGain over a grid of the three parameters it
	 * reads, at debug level 0 and 2 (the level-2 arm re-reads a param
	 * and formats both gains; the formatted values go to the shared
	 * transcript, the gate behaviour is what is compared here).
	 */
	diff_begin("GetGain over parameter grid x 2 levels");
	{
		static const long highs[] = { -6, 0, 6, 9, 15 };
		static const long diffs[] = { -3, 0, 2, 12 };
		static const long attens[] = { 0, 3, 20 };
		unsigned int a, b, d;
		struct beepgen bg;
		int modem_cookie;

		bg.modem = &modem_cookie;
		for (g = 0; g < 2; g++) {
			set_level(g ? 2 : 0);
			for (a = 0; a < 5; a++)
			for (d = 0; d < 4; d++)
			for (b = 0; b < 3; b++) {
				float g1a = -1.0f, g2a = -2.0f;
				float g1b = -3.0f, g2b = -4.0f;
				long tag = (long)(a * 100 + d * 10 + b);

				harness_param_reset();
				harness_param_set(GetDTMFHighToneLevel,
						  highs[a]);
				harness_param_set(
				    GetDTMFHighAndLowToneLevelDifference,
				    diffs[d]);
				harness_param_set(
				    GetAdditAttenToBeepgenVoice, attens[b]);
				ref_GetGain(&bg, &g1a, &g2a);
				GetGain(&bg, &g1b, &g2b);
				diff_eq_float("gain1 %ld", g1b, g1a, tag);
				diff_eq_float("gain2 %ld", g2b, g2a, tag);
			}
		}
		/*
		 * One call with the transcripts captured: three gated
		 * prints per side, byte-identical text -- so the formatted
		 * gain integers are compared too, not just the gate.
		 */
		set_level(2);
		harness_param_reset();
		harness_param_set(GetDTMFHighToneLevel, 3);
		harness_param_set(GetDTMFHighAndLowToneLevelDifference, 5);
		harness_param_set(GetAdditAttenToBeepgenVoice, 7);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		{
			float g1a, g2a, g1b, g2b;

			ref_GetGain(&bg, &g1a, &g2a);
			GetGain(&bg, &g1b, &g2b);
		}
		diff_eq_int("GetGain transcript lines ours",
			    dsplib_debug_capture_lines(0), 3, 0);
		diff_eq_int("GetGain transcript lines ref",
			    dsplib_debug_capture_lines(1), 3, 0);
		diff_eq_int("GetGain transcript text equal",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)), 0, 0);
		dsplib_debug_capture_on = 0;
		set_level(0);
	}
	failed |= diff_end();

	/*
	 * SECTION 8 -- the object's layout.  Every offset below is read from
	 * a store in the disassembly, and 0x12c is beepgen_create's own
	 * allocation size (0xacda1).  These are not differential checks;
	 * they are here because a layout error makes every check that
	 * follows compare the wrong bytes.
	 */
	diff_begin("struct beepgen layout against the object's offsets");
	{
		diff_eq_int("sizeof", (long)sizeof(struct beepgen), 0x12c, 0);
		diff_eq_int("modem", (long)offsetof(struct beepgen, modem),
			    0x00, 0);
		diff_eq_int("phase1", (long)offsetof(struct beepgen, phase1),
			    0x04, 0);
		diff_eq_int("phase2", (long)offsetof(struct beepgen, phase2),
			    0x08, 0);
		diff_eq_int("freq1", (long)offsetof(struct beepgen, freq1),
			    0x0c, 0);
		diff_eq_int("freq2", (long)offsetof(struct beepgen, freq2),
			    0x10, 0);
		diff_eq_int("gain1", (long)offsetof(struct beepgen, gain1),
			    0x14, 0);
		diff_eq_int("gain2", (long)offsetof(struct beepgen, gain2),
			    0x18, 0);
		diff_eq_int("elapsed", (long)offsetof(struct beepgen, elapsed),
			    0x1c, 0);
		diff_eq_int("duration",
			    (long)offsetof(struct beepgen, duration), 0x20, 0);
		diff_eq_int("queued", (long)offsetof(struct beepgen, queued),
			    0x24, 0);
		diff_eq_int("playing", (long)offsetof(struct beepgen, playing),
			    0x28, 0);
		diff_eq_int("tone", (long)offsetof(struct beepgen, tone),
			    0x2c, 0);
		diff_eq_int("sizeof tone entry",
			    (long)sizeof(struct beepgen_tone), 12, 0);
		diff_eq_int("fn_011c", (long)offsetof(struct beepgen, fn_011c),
			    0x11c, 0);
		diff_eq_int("hook_on_proc",
			    (long)offsetof(struct beepgen, hook_on_proc),
			    0x120, 0);
		diff_eq_int("fn_0124", (long)offsetof(struct beepgen, fn_0124),
			    0x124, 0);
		diff_eq_int("dur_units_per_sec",
			    (long)offsetof(struct beepgen, dur_units_per_sec),
			    0x128, 0);
		diff_eq_int("sizeof detector", (long)sizeof(struct detector),
			    0x38, 0);
		diff_eq_int("detector.enable",
			    (long)offsetof(struct detector, enable), 0x00, 0);
		diff_eq_int("detector.output_mode",
			    (long)offsetof(struct detector, output_mode),
			    0x34, 0);
	}
	failed |= diff_end();

	/*
	 * SECTION 9 -- beepgen_create, both entries.  The whole 0x12c bytes
	 * are compared: the fill is fixed and non-zero, so the fields the
	 * constructor leaves alone (the tone queue, elapsed, duration) are
	 * part of the comparison and an extra store would fail.
	 */
	diff_begin("beepgen_create, preallocated and allocating");
	{
		static struct beepgen bga, bgb;
		struct beepgen *pa, *pb;
		unsigned int lvl;

		harness_param_reset();
		harness_param_set(GetDTMFHighToneLevel, 3);
		harness_param_set(GetDTMFHighAndLowToneLevelDifference, 5);
		harness_param_set(GetAdditAttenToBeepgenVoice, 7);

		for (lvl = 0; lvl < 2; lvl++) {
			set_level(lvl ? 2 : 0);
			beep_pair_create(&bga, &bgb, &beep_cfg);
			diff_eq_obj("created", struct beepgen, &bgb, &bga,
				    (long)lvl);
			beep_pair_create(&bga, &bgb, &beep_cfg_null);
			diff_eq_obj("created, no callbacks", struct beepgen,
				    &bgb, &bga, (long)lvl);
		}
		set_level(0);

		/*
		 * The allocating entry.  sysdep_malloc fills with a fixed
		 * pattern, so the untouched tail is still comparable.
		 */
		harness_alloc_reset();
		pa = ref_beepgen_create(0, &beep_cfg);
		pb = beepgen_create(0, &beep_cfg);
		diff_eq_int("ref allocated", pa != 0, 1, 0);
		diff_eq_int("ours allocated", pb != 0, 1, 0);
		diff_eq_int("two allocations", (long)harness_alloc.allocs, 2,
			    0);
		diff_eq_int("asked for 0x12c",
			    (long)harness_alloc_reqsize(pb), 0x12c, 0);
		if (pa != 0 && pb != 0)
			diff_eq_obj("allocated", struct beepgen, pb, pa, 0);
		ref_beepgen_delete(pa);
		beepgen_delete(pb);
		diff_eq_int("both freed", (long)harness_alloc.live, 0, 0);
		ref_beepgen_delete(0);
		beepgen_delete(0);
		diff_eq_int("NULL free counted both sides",
			    (long)harness_alloc.free_null, 2, 0);

		/*
		 * The transcript: five gated lines per side at level 2 --
		 * beepgen_create's own two, and GetGain's three from inside
		 * it.
		 */
		set_level(2);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		beep_pair_create(&bga, &bgb, &beep_cfg);
		diff_eq_int("create transcript lines ours",
			    dsplib_debug_capture_lines(0), 5, 0);
		diff_eq_int("create transcript lines ref",
			    dsplib_debug_capture_lines(1), 5, 0);
		diff_eq_int("create transcript text equal",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)), 0, 0);
		dsplib_debug_capture_on = 0;
		set_level(0);
	}
	failed |= diff_end();

	/*
	 * SECTION 10 -- beepgen_start_beep.  The grid covers both special
	 * frequencies in both positions, the queued/not-queued split (the
	 * second call in each case appends only), and durations either side
	 * of the divide.
	 */
	diff_begin("beepgen_start_beep over the frequency and duration grid");
	{
		static const int freqs[] = { 0, -1, 1, 697, 941, 1209, 1633,
					     -2, 32767 };
		static const int durs[] = { 0, 1, 2, 3, 10, 255, -1 };
		static struct beepgen bga, bgb;
		unsigned int fa, fb2, du, extra;
		long tag = 0;

		harness_param_reset();
		harness_param_set(GetDTMFHighToneLevel, 6);
		harness_param_set(GetDTMFHighAndLowToneLevelDifference, 3);
		harness_param_set(GetAdditAttenToBeepgenVoice, 2);
		set_level(0);

		for (fa = 0; fa < sizeof(freqs) / sizeof(freqs[0]); fa++)
		for (fb2 = 0; fb2 < sizeof(freqs) / sizeof(freqs[0]); fb2++)
		for (du = 0; du < sizeof(durs) / sizeof(durs[0]); du++) {
			tag++;
			beep_pair_create(&bga, &bgb, &beep_cfg);
			trace_reset();
			ref_beepgen_start_beep(&bga, freqs[fa], freqs[fb2],
					       durs[du]);
			trace_snap(0);
			trace_reset();
			beepgen_start_beep(&bgb, freqs[fa], freqs[fb2],
					   durs[du]);
			trace_snap(1);
			diff_eq_obj("first beep", struct beepgen, &bgb, &bga,
				    tag);
			trace_compare("callbacks, first beep", tag);

			/* Three more, which must append and nothing else. */
			for (extra = 0; extra < 3; extra++) {
				ref_beepgen_start_beep(&bga, 1000 + extra,
						       2000 + extra, 4);
				beepgen_start_beep(&bgb, 1000 + extra,
						   2000 + extra, 4);
				diff_eq_obj("appended", struct beepgen, &bgb,
					    &bga, tag * 10 + extra);
			}
		}

		/* The -1 marker with no callback installed. */
		beep_pair_create(&bga, &bgb, &beep_cfg_null);
		trace_reset();
		ref_beepgen_start_beep(&bga, -1, 0, 5);
		trace_snap(0);
		trace_reset();
		beepgen_start_beep(&bgb, -1, 0, 5);
		trace_snap(1);
		diff_eq_obj("marker, no callback", struct beepgen, &bgb, &bga,
			    0);
		trace_compare("callbacks, marker with none installed", 0);
	}
	failed |= diff_end();

	/*
	 * SECTION 11 -- beepgen_start_dtmf over every code value, then the
	 * two special characters with the transcripts captured.
	 */
	diff_begin("beepgen_start_dtmf, all 256 codes");
	{
		static struct beepgen bga, bgb;
		unsigned int code;
		unsigned int lvl;

		harness_param_reset();
		harness_param_set(GetDTMFHighToneLevel, 0);
		harness_param_set(GetDTMFHighAndLowToneLevelDifference, 6);
		harness_param_set(GetAdditAttenToBeepgenVoice, 0);
		set_level(0);
		stub_duration = 9;

		for (code = 0; code < 256; code++) {
			beep_pair_create(&bga, &bgb, &beep_cfg);
			trace_reset();
			ref_beepgen_start_dtmf(&bga, (int)code, 2);
			trace_snap(0);
			trace_reset();
			beepgen_start_dtmf(&bgb, (int)code, 2);
			trace_snap(1);
			diff_eq_obj("dtmf", struct beepgen, &bgb, &bga,
				    (long)code);
			trace_compare("callbacks, dtmf", (long)code);
		}

		/*
		 * '!' reaches the duration callback, and the value it
		 * returns has to be the one that lands in the queue.  A
		 * second stub value proves the field is not a constant.
		 */
		stub_duration = 40;
		beep_pair_create(&bga, &bgb, &beep_cfg);
		trace_reset();
		ref_beepgen_start_dtmf(&bga, '!', 2);
		trace_snap(0);
		trace_reset();
		beepgen_start_dtmf(&bgb, '!', 2);
		trace_snap(1);
		diff_eq_obj("'!' with a different duration", struct beepgen,
			    &bgb, &bga, 0);
		trace_compare("callbacks, '!'", 0);
		diff_eq_int("'!' duration came from the callback",
			    bgb.tone[0].duration, 40, 0);
		diff_eq_int("'!' asked for 24", snap_tag[1][1], 24, 0);
		diff_eq_int("'!' units per second", bgb.dur_units_per_sec,
			    100, 0);
		stub_duration = 9;

		/* ',' triples the argument and silences both halves. */
		beep_pair_create(&bga, &bgb, &beep_cfg);
		ref_beepgen_start_dtmf(&bga, ',', 5);
		beepgen_start_dtmf(&bgb, ',', 5);
		diff_eq_obj("','", struct beepgen, &bgb, &bga, 0);
		diff_eq_int("',' tripled", bgb.tone[0].duration, 15, 0);

		/* Codes that print, with the transcripts compared. */
		for (lvl = 0; lvl < 4; lvl++) {
			static const int codes[] = { '!', ',', '5', 'z' };

			set_level(2);
			beep_pair_create(&bga, &bgb, &beep_cfg);
			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			ref_beepgen_start_dtmf(&bga, codes[lvl], 3);
			beepgen_start_dtmf(&bgb, codes[lvl], 3);
			dsplib_debug_capture_on = 0;
			diff_eq_int("dtmf transcript lines %ld",
				    dsplib_debug_capture_lines(0),
				    dsplib_debug_capture_lines(1),
				    (long)codes[lvl]);
			diff_eq_int("dtmf transcript nonempty %ld",
				    dsplib_debug_capture_lines(0) >= 2, 1,
				    (long)codes[lvl]);
			diff_eq_int("dtmf transcript text %ld",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)), 0,
				    (long)codes[lvl]);
			set_level(0);
		}
	}
	failed |= diff_end();

	/*
	 * SECTION 12 -- beepgen_sample.  Each case queues a sequence and
	 * then runs it to completion in lockstep, comparing the sample, the
	 * verdict and the whole object every step.  The float comparison is
	 * exact: the object's sine is x87 and so is ours.
	 */
	diff_begin("beepgen_sample, whole queues run to completion");
	{
		static struct beepgen bga, bgb;
		static const int seq[][4] = {
			/* freq1, freq2, duration, count of entries used */
			{ 1209, 697, 1, 0 },
			{ 0, 941, 1, 0 },
			{ 1477, 0, 1, 0 },
			{ -1, -1, 1, 0 },
			{ 1633, 852, 0, 0 }
		};
		unsigned int caseno, step, k;
		int ra, rb;
		float outa, outb;
		long guard;

		harness_param_reset();
		harness_param_set(GetDTMFHighToneLevel, 2);
		harness_param_set(GetDTMFHighAndLowToneLevelDifference, 4);
		harness_param_set(GetAdditAttenToBeepgenVoice, 1);
		set_level(0);

		for (caseno = 0; caseno < 6; caseno++) {
			beep_pair_create(&bga, &bgb, &beep_cfg);
			/* Case 0 queues one tone, case k queues k + 1. */
			for (k = 0; k <= caseno && k < 5; k++) {
				ref_beepgen_start_beep(&bga, seq[k][0],
						       seq[k][1], seq[k][2]);
				beepgen_start_beep(&bgb, seq[k][0],
						   seq[k][1], seq[k][2]);
			}
			trace_reset();
			guard = 0;
			step = 0;
			for (;;) {
				outa = 0.0f;
				outb = 0.0f;
				ra = ref_beepgen_sample(&bga, &outa);
				trace_snap(0);
				trace_reset();
				rb = beepgen_sample(&bgb, &outb);
				trace_snap(1);
				trace_reset();
				diff_eq_float("sample", outb, outa,
					      (long)step);
				diff_eq_int("verdict", rb, ra, (long)step);
				diff_eq_obj("after sample", struct beepgen,
					    &bgb, &bga, (long)step);
				trace_compare("callbacks, sample",
					      (long)step);
				step++;
				if (ra != 0 || rb != 0)
					break;
				if (++guard > 100000)
					break;
			}
			diff_eq_int("case %ld terminated", guard <= 100000, 1,
				    (long)caseno);
			diff_eq_int("case %ld ran", step > 0, 1,
				    (long)caseno);
		}

		/* The marker tone with no callbacks installed. */
		beep_pair_create(&bga, &bgb, &beep_cfg_null);
		ref_beepgen_start_beep(&bga, -1, -1, 0);
		beepgen_start_beep(&bgb, -1, -1, 0);
		outa = 0.0f;
		outb = 0.0f;
		ra = ref_beepgen_sample(&bga, &outa);
		rb = beepgen_sample(&bgb, &outb);
		diff_eq_float("marker sample", outb, outa, 0);
		diff_eq_int("marker verdict", rb, ra, 0);
		diff_eq_obj("marker after sample", struct beepgen, &bgb, &bga,
			    0);

		/* Level 2, so the four gated lines are compared as text. */
		set_level(2);
		beep_pair_create(&bga, &bgb, &beep_cfg);
		ref_beepgen_start_beep(&bga, -1, 0, 0);
		beepgen_start_beep(&bgb, -1, 0, 0);
		ref_beepgen_start_beep(&bga, 1209, 697, 0);
		beepgen_start_beep(&bgb, 1209, 697, 0);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		outa = 0.0f;
		outb = 0.0f;
		ref_beepgen_sample(&bga, &outa);
		beepgen_sample(&bgb, &outb);
		dsplib_debug_capture_on = 0;
		diff_eq_int("sample transcript lines",
			    dsplib_debug_capture_lines(0),
			    dsplib_debug_capture_lines(1), 0);
		diff_eq_int("sample transcript nonempty",
			    dsplib_debug_capture_lines(0) >= 3, 1, 0);
		diff_eq_int("sample transcript text",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)), 0, 0);
		set_level(0);
	}
	failed |= diff_end();

	/*
	 * SECTION 13 -- the detector setters.  0x38 bytes of fill, one
	 * store each, everything else untouched.
	 */
	diff_begin("detector_set_enable / _output_status / _output_in_stream");
	{
		static struct detector da, db;
		static const short vals[] = { 0, 1, -1, 0x1234, 0x7fff,
					      (short)0x8000 };
		unsigned int v;

		for (v = 0; v < sizeof(vals) / sizeof(vals[0]); v++) {
			memset(&da, 0x5a, sizeof(da));
			memset(&db, 0x5a, sizeof(db));
			ref_detector_set_enable(&da, vals[v]);
			detector_set_enable(&db, vals[v]);
			diff_eq_obj("set_enable", struct detector, &db, &da,
				    (long)vals[v]);
		}

		memset(&da, 0x5a, sizeof(da));
		memset(&db, 0x5a, sizeof(db));
		ref_detector_set_output_status(&da);
		detector_set_output_status(&db);
		diff_eq_obj("output_status", struct detector, &db, &da, 0);

		memset(&da, 0x5a, sizeof(da));
		memset(&db, 0x5a, sizeof(db));
		ref_detector_set_output_in_stream(&da);
		detector_set_output_in_stream(&db);
		diff_eq_obj("output_in_stream", struct detector, &db, &da, 0);
	}
	failed |= diff_end();

	/*
	 * SECTION 14 -- create_dtmf.  0x98 bytes compared whole, so the
	 * 0x80..0x8f the constructor leaves alone is checked too.
	 */
	diff_begin("create_dtmf, preallocated and allocating");
	{
		static struct dtmf da, db;
		struct dtmf *pa, *pb;

		memset(&da, 0x5a, sizeof(da));
		memset(&db, 0x5a, sizeof(db));
		diff_eq_int("ref returns its argument",
			    ref_create_dtmf(&da) == &da, 1, 0);
		diff_eq_int("ours returns its argument",
			    create_dtmf(&db) == &db, 1, 0);
		diff_eq_obj("created", struct dtmf, &db, &da, 0);

		harness_alloc_reset();
		pa = ref_create_dtmf(0);
		pb = create_dtmf(0);
		diff_eq_int("ref allocated", pa != 0, 1, 0);
		diff_eq_int("ours allocated", pb != 0, 1, 0);
		diff_eq_int("asked for 0x98", (long)harness_alloc_reqsize(pb),
			    0x98, 0);
		if (pa != 0 && pb != 0)
			diff_eq_obj("allocated", struct dtmf, pb, pa, 0);
		sysdep_free(pa);
		sysdep_free(pb);
	}
	failed |= diff_end();

	/*
	 * SECTION 15 -- FDSP_DP_Run.  Both directions at once, over counts
	 * that straddle the zero-length guard.  Inputs stay inside +/-1.0 so
	 * the transmit product stays inside the short range and the
	 * truncating store is defined.
	 */
	diff_begin("FDSP_DP_Run, both directions");
	{
		static short rx_lin[200];
		static float rx_flt_a[200], rx_flt_b[200];
		static float tx_flt[200];
		static short tx_lin_a[200], tx_lin_b[200];
		static const unsigned short counts[] = { 0, 1, 2, 7, 8, 63,
							 160, 200 };
		unsigned int ci, j;
		unsigned short cnt;
		int status_a, status_b, ra, rb;
		/*
		 * The sixth argument, which this function never loads.  It
		 * was a `void *` here until `voice_online` typed it -- finding
		 * F8786 and D986 -- so it is now the `unsigned short *` it
		 * really is, planted with a value neither side may disturb.
		 */
		unsigned short spare_a, spare_b;

		for (ci = 0; ci < sizeof(counts) / sizeof(counts[0]); ci++) {
			for (j = 0; j < 200; j++) {
				rx_lin[j] = (short)(lcg() >> 16);
				tx_flt[j] = (float)((int)(lcg() >> 16)
						    - 32768) / 32768.0f;
			}
			for (j = 0; j < 200; j++) {
				rx_flt_a[j] = -1.0f;
				rx_flt_b[j] = -1.0f;
				tx_lin_a[j] = 0x5a5a;
				tx_lin_b[j] = 0x5a5a;
			}
			cnt = counts[ci];
			status_a = 0x55;
			status_b = 0x55;
			spare_a = 0x1234;
			spare_b = 0x1234;
			ra = ref_FDSP_DP_Run(&status_a, rx_lin, rx_flt_a,
					     tx_flt, tx_lin_a, &spare_a,
					     &cnt);
			rb = FDSP_DP_Run(&status_b, rx_lin, rx_flt_b, tx_flt,
					 tx_lin_b, &spare_b, &cnt);
			diff_eq_int("return %ld", rb, ra, (long)cnt);
			diff_eq_int("status %ld", status_b, status_a,
				    (long)cnt);
			diff_eq_int("count untouched %ld", (long)cnt,
				    (long)counts[ci], (long)counts[ci]);
			diff_eq_int("hostcount untouched %ld", spare_b, spare_a,
				    (long)cnt);
			diff_eq_int("hostcount is still 0x1234 %ld", spare_b,
				    0x1234, (long)cnt);
			for (j = 0; j < 200; j++) {
				diff_eq_float("rx[%ld]", rx_flt_b[j],
					      rx_flt_a[j], (long)j);
				diff_eq_int("tx[%ld]", tx_lin_b[j],
					    tx_lin_a[j], (long)j);
			}
		}
	}
	failed |= diff_end();

	return failed;
}
