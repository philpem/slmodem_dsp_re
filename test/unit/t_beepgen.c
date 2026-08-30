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

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/beepgen.h"
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

	return failed;
}
