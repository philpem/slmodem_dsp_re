/*
 * t_voicedp.c -- differential tests for the voice service's beep-side block
 * handlers and three of the four setters: `voice_set_online`,
 * `voice_set_duplex`, `voice_set_tx`, `voice_online` and `voice_duplex`.
 * (`voice_set_rx` is in t_voicedprx with the handler it installs.)
 *
 * NONE OF THE FOUR HAS AN INTERNAL CALLER IN THE BLOB except through the
 * handler slot `voice_create` and the two setters write, so every input here
 * is constructed.  All four are GLOBAL in the object, so the `ref_` aliases
 * take the ordinary convention -- there is no regparm question, and the
 * prologues confirm it (each reads its arguments off the stack).
 *
 * THE WHOLE CONTEXT IS COMPARED, NOT THE FIELDS THESE FUNCTIONS ARE EXPECTED
 * TO TOUCH.  `struct voice_ctx` is 0x7dc bytes and most of it is `pad_*`, so
 * D955/F8587's rule bites hard: a fixture must plant every field a callee
 * uses as a SUBSCRIPT and not only every field it dereferences, and a
 * blob-against-blob dry run cannot catch an unplanted subscript because both
 * sides read the same wild index.  Both contexts are therefore prefilled with
 * 0xa5 -- never zero, which makes "never written" look like "written
 * correctly" -- and only the handful of fields each case needs is planted on
 * top.  `ctx_compare` then compares all 2,012 bytes.
 *
 * THE SIX POINTER FIELDS ARE MASKED and nothing else is.  Each side owns its
 * own beep generator, its own datapump status word and its own handler
 * address, so those slots differ by construction and would swamp every
 * comparison; they are zeroed in a COPY, leaving every other byte -- pad
 * included -- under test.  The handler slot is then checked separately and
 * absolutely, against `voice_online` on our side and `ref_voice_online` on
 * the blob's, which is the only check that can see a setter installing the
 * wrong function.
 *
 * FLOAT COMPARISONS ARE EXACT.  Both sides run x87 with the same shapes and a
 * tolerance would only hide a wrong reconstruction.
 *
 * COVERAGE IS ASSERTED FROM THE RUN (F134), not from this comment: `main`
 * fails unless every arm below was actually taken.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/beepgen.h"
#include "dsplib/debug.h"
#include "dsplib/detector.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"
#include "dsplib/voice.h"

extern unsigned int ref_dsplibs_debug_level;

extern void ref_voice_set_online(struct voice_ctx *v);
extern void ref_voice_set_duplex(struct voice_ctx *v);
extern void ref_voice_set_tx(struct voice_ctx *v);
extern int ref_voice_online(struct voice_ctx *v, short *rx_lin, float *rx_flt,
			    float *tx_flt, short *tx_lin,
			    unsigned short *hostcount,
			    unsigned short *countp);
extern int ref_voice_tx(struct voice_ctx *v, short *rx_lin, float *rx_flt,
			float *tx_flt, short *tx_lin,
			unsigned short *hostcount, unsigned short *countp);
extern int ref_voice_duplex(struct voice_ctx *v, short *rx_lin, float *rx_flt,
			    float *tx_flt, short *tx_lin,
			    unsigned short *hostcount,
			    unsigned short *countp);
extern struct beepgen *ref_beepgen_create(struct beepgen *bg,
					  const struct beepgen_config *cfg);
extern void ref_beepgen_start_beep(struct beepgen *bg, int freq1, int freq2,
				   int duration);

static int modem_cookie;
static const struct beepgen_config beep_cfg = { &modem_cookie, 0, 0, 0 };

#define NBUF	256

/* Coverage counters, all asserted at the end. */
static long n_online, n_duplex;
static long arm_done_lin, arm_done_flt, arm_beep_lin, arm_beep_flt;
static long arm_online_finish, arm_online_mode, arm_online_zerofill;
static long arm_duplex_finish, arm_duplex_mode, arm_duplex_skipped;

/*
 * GetGain's three parameters, planted.
 *
 * WITHOUT THIS THE BEEP IS SILENT AND TWO MUTANTS SURVIVE.  The harness's
 * unset parameter store answers 0x5A000000 + 7*param, so `(6 - high) * 0.05`
 * is about -7e7, `pow` underflows to zero, and every beep sample is exactly
 * 0.0f -- which makes `(short)(s * 30000.0f)` and `(short)s` the same
 * function.  The mutation set caught 23 of 25 until these three lines
 * existed, and the two it missed were both the beep's full scale.  That is
 * F134's argument arriving from the mutation side: a check that cannot fail
 * looks exactly like a check that passes.
 */
static void
plant_params(void)
{
	harness_param_reset();
	harness_param_set(GetDTMFHighToneLevel, 3);
	harness_param_set(GetDTMFHighAndLowToneLevelDifference, 5);
	harness_param_set(GetAdditAttenToBeepgenVoice, 7);
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/*
 * Compare two contexts with only the six per-side pointer slots masked.
 * Everything else, pad included, is under test.
 */
static void
ctx_compare(const char *what, const struct voice_ctx *ours,
	    const struct voice_ctx *ref, long tag)
{
	static struct voice_ctx ca, cb;

	cb = *ours;
	ca = *ref;
	cb.beepgen = 0;
	ca.beepgen = 0;
	cb.detector = 0;
	ca.detector = 0;
	cb.handler = 0;
	ca.handler = 0;
	cb.fifo = 0;
	ca.fifo = 0;
	cb.silence = 0;
	ca.silence = 0;
	cb.dp = 0;
	ca.dp = 0;
	diff_eq_obj(what, struct voice_ctx, &cb, &ca, tag);
}

static void
flt_compare(const char *what, const float *ours, const float *ref, int n,
	    long tag)
{
	int i;

	for (i = 0; i < n; i++)
		if (ours[i] != ref[i]) {
			diff_eq_int(what, i, -1, tag);
			return;
		}
	diff_eq_int(what, 0, 0, tag);
}

static void
lin_compare(const char *what, const short *ours, const short *ref, int n,
	    long tag)
{
	int i;

	for (i = 0; i < n; i++)
		if (ours[i] != ref[i]) {
			diff_eq_int(what, i, -1, tag);
			return;
		}
	diff_eq_int(what, 0, 0, tag);
}

/* ------------------------------------------------------------------ */

/*
 * The two setters.  Both write a mode and a handler and then hand the
 * detector a word -- `voice_set_online` the context's own `detector_enable`,
 * `voice_set_duplex` the constant 0x24 -- so the detector object is planted
 * too and compared whole.
 */
static int
t_setters(void)
{
	static struct voice_ctx a, b;
	static struct detector da, db;
	unsigned int e;
	static const unsigned short enables[] = { 0, 1, 0x24, 0x3f, 0xffff,
						  0x8000 };

	diff_begin("voice_set_online, voice_set_duplex and voice_set_tx");

	for (e = 0; e < sizeof enables / sizeof enables[0]; e++) {
		long tag = (long)enables[e];

		memset(&a, HARNESS_MALLOC_FILL, sizeof a);
		memset(&b, HARNESS_MALLOC_FILL, sizeof b);
		memset(&da, HARNESS_MALLOC_FILL, sizeof da);
		memset(&db, HARNESS_MALLOC_FILL, sizeof db);
		a.detector = &da;
		b.detector = &db;
		a.detector_enable = enables[e];
		b.detector_enable = enables[e];

		ref_voice_set_online(&a);
		voice_set_online(&b);

		diff_eq_int("online installs voice_online",
			    b.handler == voice_online, 1, tag);
		diff_eq_int("online installs ref_voice_online on the ref side",
			    (void *)a.handler == (void *)ref_voice_online, 1,
			    tag);
		diff_eq_int("online sets mode 2", b.mode, 2, tag);
		ctx_compare("context after voice_set_online", &b, &a, tag);
		diff_eq_obj("detector after voice_set_online", struct detector,
			    &db, &da, tag);
		diff_eq_int("online enable is the context's",
			    (long)(unsigned short)db.enable,
			    (long)enables[e], tag);

		memset(&a, HARNESS_MALLOC_FILL, sizeof a);
		memset(&b, HARNESS_MALLOC_FILL, sizeof b);
		memset(&da, HARNESS_MALLOC_FILL, sizeof da);
		memset(&db, HARNESS_MALLOC_FILL, sizeof db);
		a.detector = &da;
		b.detector = &db;
		a.detector_enable = enables[e];
		b.detector_enable = enables[e];

		ref_voice_set_duplex(&a);
		voice_set_duplex(&b);

		diff_eq_int("duplex installs voice_duplex",
			    b.handler == voice_duplex, 1, tag);
		diff_eq_int("duplex installs ref_voice_duplex on the ref side",
			    (void *)a.handler == (void *)ref_voice_duplex, 1,
			    tag);
		diff_eq_int("duplex sets mode 3", b.mode, 3, tag);
		ctx_compare("context after voice_set_duplex", &b, &a, tag);
		diff_eq_obj("detector after voice_set_duplex", struct detector,
			    &db, &da, tag);
		diff_eq_int("duplex enable is the constant 0x24",
			    (long)(unsigned short)db.enable, 0x24, tag);

		/*
		 * And the transmit setter, which is the one that also writes a
		 * format and starts the underrun latch UP -- see voicedp.c for
		 * why an armed-but-empty FIFO must not report.
		 */
		memset(&a, HARNESS_MALLOC_FILL, sizeof a);
		memset(&b, HARNESS_MALLOC_FILL, sizeof b);
		memset(&da, HARNESS_MALLOC_FILL, sizeof da);
		memset(&db, HARNESS_MALLOC_FILL, sizeof db);
		a.detector = &da;
		b.detector = &db;
		a.detector_enable_tx = enables[e];
		b.detector_enable_tx = enables[e];

		ref_voice_set_tx(&a);
		voice_set_tx(&b);

		diff_eq_int("tx installs voice_tx", b.handler == voice_tx, 1,
			    tag);
		diff_eq_int("tx installs ref_voice_tx on the ref side",
			    (void *)a.handler == (void *)ref_voice_tx, 1, tag);
		diff_eq_int("tx sets mode 1", b.mode, 1, tag);
		diff_eq_int("tx sets the 8 kHz 8-bit format",
			    (long)b.rate_bits.both, VOICE_RATE_BITS(8, 8000),
			    tag);
		diff_eq_int("tx starts the underrun latch up", b.underrun, 1,
			    tag);
		ctx_compare("context after voice_set_tx", &b, &a, tag);
		diff_eq_obj("detector after voice_set_tx", struct detector,
			    &db, &da, tag);
		diff_eq_int("tx enable is the context's tx mask",
			    (long)(unsigned short)db.enable,
			    (long)enables[e], tag);
	}
	return diff_end();
}

/* ------------------------------------------------------------------ */

/*
 * One `voice_online` case.  `tones` beeps of `dur` tenths are queued, so a
 * `dur` of 0 retires the queue on the first sample of the block and anything
 * larger outlives it.
 */
static void
online_case(int beep_done, int out_format, unsigned short count, int mode,
	    int int_0014, int tones, int dur, long tag)
{
	static struct voice_ctx a, b;
	static struct beepgen bga, bgb;
	static float flta[NBUF], fltb[NBUF];
	static short lina[NBUF], linb[NBUF];
	static short rxa[NBUF], rxb[NBUF];
	static float txa[NBUF], txb[NBUF];
	unsigned short hca, hcb, cna, cnb;
	int ra, rb, i;

	memset(&a, HARNESS_MALLOC_FILL, sizeof a);
	memset(&b, HARNESS_MALLOC_FILL, sizeof b);
	memset(&bga, 0x5a, sizeof bga);
	memset(&bgb, 0x5a, sizeof bgb);
	plant_params();
	ref_beepgen_create(&bga, &beep_cfg);
	beepgen_create(&bgb, &beep_cfg);
	for (i = 0; i < tones; i++) {
		ref_beepgen_start_beep(&bga, 697, 1209, dur);
		beepgen_start_beep(&bgb, 697, 1209, dur);
	}

	a.beepgen = &bga;
	b.beepgen = &bgb;
	a.beep_done = beep_done;
	b.beep_done = beep_done;
	a.out_format = out_format;
	b.out_format = out_format;
	a.mode = mode;
	b.mode = mode;
	a.int_0014 = int_0014;
	b.int_0014 = int_0014;

	memset(flta, HARNESS_MALLOC_FILL, sizeof flta);
	memset(fltb, HARNESS_MALLOC_FILL, sizeof fltb);
	memset(lina, HARNESS_MALLOC_FILL, sizeof lina);
	memset(linb, HARNESS_MALLOC_FILL, sizeof linb);
	memset(rxa, HARNESS_MALLOC_FILL, sizeof rxa);
	memset(rxb, HARNESS_MALLOC_FILL, sizeof rxb);
	memset(txa, HARNESS_MALLOC_FILL, sizeof txa);
	memset(txb, HARNESS_MALLOC_FILL, sizeof txb);
	hca = hcb = 0x1234;
	cna = cnb = count;

	ra = ref_voice_online(&a, rxa, flta, txa, lina, &hca, &cna);
	rb = voice_online(&b, rxb, fltb, txb, linb, &hcb, &cnb);

	diff_eq_int("voice_online return", rb, ra, tag);
	diff_eq_int("voice_online hostcount", hcb, hca, tag);
	diff_eq_int("voice_online countp", cnb, cna, tag);
	ctx_compare("context after voice_online", &b, &a, tag);
	diff_eq_obj("beepgen after voice_online", struct beepgen, &bgb, &bga,
		    tag);
	flt_compare("voice_online rx_flt", fltb, flta, NBUF, tag);
	lin_compare("voice_online tx_lin", linb, lina, NBUF, tag);
	lin_compare("voice_online leaves rx_lin alone", rxb, rxa, NBUF, tag);
	flt_compare("voice_online leaves tx_flt alone", txb, txa, NBUF, tag);
	/*
	 * D997 stated absolutely: the first store to *hostcount is dead, so
	 * whatever *countp held, the caller sees zero in both slots.
	 */
	diff_eq_int("D997: hostcount ends at 0", hcb, 0, tag);
	diff_eq_int("countp ends at 0", cnb, 0, tag);

	n_online++;
	if (beep_done) {
		if ((unsigned int)(out_format - 1) <= 1u)
			arm_done_lin++;
		else
			arm_done_flt++;
	} else if ((unsigned int)(out_format - 1) <= 1u) {
		arm_beep_lin++;
	} else {
		arm_beep_flt++;
	}
	if (!beep_done && b.beep_done)
		arm_online_finish++;
	if (rb == 2)
		arm_online_mode++;
	if (!beep_done && count > (unsigned short)(tones * (dur * 800 + 1)))
		arm_online_zerofill++;
}

static int
t_online(void)
{
	static const unsigned short counts[] = { 0, 1, 2, 7, 40, 200 };
	static const int fmts[] = { 0, 1, 2, 3, -1 };
	unsigned int c, f;
	long tag = 0;

	diff_begin("voice_online over both output formats and both beep states");

	for (f = 0; f < sizeof fmts / sizeof fmts[0]; f++)
		for (c = 0; c < sizeof counts / sizeof counts[0]; c++) {
			/* Beep already finished: the whole block is silence. */
			online_case(1, fmts[f], counts[c], 2, 2, 0, 0, tag++);
			/* A beep that outlives the block. */
			online_case(0, fmts[f], counts[c], 2, 2, 1, 1, tag++);
			/* A beep that retires on the first sample. */
			online_case(0, fmts[f], counts[c], 2, 2, 1, 0, tag++);
			/* Two beeps: the second sample retires the queue. */
			online_case(0, fmts[f], counts[c], 2, 2, 2, 0, tag++);
		}

	/* The mode comparison, all four shapes of it. */
	online_case(1, 1, 8, 2, 2, 0, 0, 900);	/* equal -> 0    */
	online_case(1, 1, 8, 2, 4, 0, 0, 901);	/* differ, != 0  */
	online_case(1, 1, 8, 2, 0, 0, 0, 902);	/* differ, == 0  */
	online_case(1, 1, 8, 0, 0, 0, 0, 903);	/* equal at zero */

	return diff_end();
}

/* ------------------------------------------------------------------ */

static void
duplex_case(int beep_done, int out_format, unsigned short count, int mode,
	    int int_0014, int tones, int dur, long tag)
{
	static struct voice_ctx a, b;
	static struct beepgen bga, bgb;
	static float flta[NBUF], fltb[NBUF];
	static short lina[NBUF], linb[NBUF];
	static short rxa[NBUF], rxb[NBUF];
	static float txa[NBUF], txb[NBUF];
	static int dpa, dpb;
	unsigned short hca, hcb, cna, cnb;
	int ra, rb, i;

	memset(&a, HARNESS_MALLOC_FILL, sizeof a);
	memset(&b, HARNESS_MALLOC_FILL, sizeof b);
	memset(&bga, 0x5a, sizeof bga);
	memset(&bgb, 0x5a, sizeof bgb);
	plant_params();
	ref_beepgen_create(&bga, &beep_cfg);
	beepgen_create(&bgb, &beep_cfg);
	for (i = 0; i < tones; i++) {
		ref_beepgen_start_beep(&bga, 697, 1209, dur);
		beepgen_start_beep(&bgb, 697, 1209, dur);
	}

	dpa = dpb = 0x5a5a5a5a;
	a.beepgen = &bga;
	b.beepgen = &bgb;
	a.dp = &dpa;
	b.dp = &dpb;
	a.beep_done = beep_done;
	b.beep_done = beep_done;
	a.out_format = out_format;
	b.out_format = out_format;
	a.mode = mode;
	b.mode = mode;
	a.int_0014 = int_0014;
	b.int_0014 = int_0014;

	for (i = 0; i < NBUF; i++) {
		rxa[i] = rxb[i] = (short)(i * 173 - 4000);
		txa[i] = txb[i] = (float)i * 0.25f - 3.0f;
	}
	memset(flta, HARNESS_MALLOC_FILL, sizeof flta);
	memset(fltb, HARNESS_MALLOC_FILL, sizeof fltb);
	memset(lina, HARNESS_MALLOC_FILL, sizeof lina);
	memset(linb, HARNESS_MALLOC_FILL, sizeof linb);
	hca = hcb = 0x1234;
	cna = cnb = count;

	ra = ref_voice_duplex(&a, rxa, flta, txa, lina, &hca, &cna);
	rb = voice_duplex(&b, rxb, fltb, txb, linb, &hcb, &cnb);

	diff_eq_int("voice_duplex return", rb, ra, tag);
	diff_eq_int("voice_duplex hostcount", hcb, hca, tag);
	diff_eq_int("voice_duplex countp", cnb, cna, tag);
	diff_eq_int("voice_duplex dp status", dpb, dpa, tag);
	diff_eq_int("voice_duplex sets dp status to 2", dpb, 2, tag);
	ctx_compare("context after voice_duplex", &b, &a, tag);
	diff_eq_obj("beepgen after voice_duplex", struct beepgen, &bgb, &bga,
		    tag);
	flt_compare("voice_duplex rx_flt", fltb, flta, NBUF, tag);
	lin_compare("voice_duplex tx_lin", linb, lina, NBUF, tag);
	lin_compare("voice_duplex leaves rx_lin alone", rxb, rxa, NBUF, tag);
	flt_compare("voice_duplex leaves tx_flt alone", txb, txa, NBUF, tag);

	n_duplex++;
	if (!beep_done && b.beep_done)
		arm_duplex_finish++;
	if (rb == 5)
		arm_duplex_mode++;
	if (beep_done)
		arm_duplex_skipped++;
}

static int
t_duplex(void)
{
	static const unsigned short counts[] = { 0, 1, 2, 7, 40, 200 };
	static const int fmts[] = { 0, 1, 2 };
	unsigned int c, f;
	long tag = 0;

	diff_begin("voice_duplex, datapump then beep overlay");

	for (f = 0; f < sizeof fmts / sizeof fmts[0]; f++)
		for (c = 0; c < sizeof counts / sizeof counts[0]; c++) {
			duplex_case(1, fmts[f], counts[c], 3, 3, 0, 0, tag++);
			duplex_case(0, fmts[f], counts[c], 3, 3, 1, 1, tag++);
			duplex_case(0, fmts[f], counts[c], 3, 3, 1, 0, tag++);
			duplex_case(0, fmts[f], counts[c], 3, 3, 2, 0, tag++);
		}

	/*
	 * The mode comparison.  voice_duplex does NOT have voice_online's
	 * second test, so a zero `int_0014` that differs from `mode` still
	 * returns 5 -- which is exactly the difference between the two and is
	 * stated here absolutely.
	 */
	duplex_case(1, 1, 8, 3, 3, 0, 0, 900);
	duplex_case(1, 1, 8, 3, 4, 0, 0, 901);
	duplex_case(1, 1, 8, 3, 0, 0, 0, 902);

	return diff_end();
}

/*
 * The one place the two handlers' tail tests are separable: `int_0014 == 0`
 * and different from `mode`.  voice_online answers 0 there and voice_duplex
 * answers 5.  Stated against the numbers, not only against the blob, so a
 * reconstruction that agreed with a wrongly-patched blob still fails.
 */
static int
t_tail_difference(void)
{
	static struct voice_ctx v;
	static struct beepgen bg;
	static float flt[NBUF], tx[NBUF];
	static short lin[NBUF], rx[NBUF];
	static int dp;
	unsigned short hc, cn;
	int r;

	diff_begin("the two tail tests differ on int_0014 == 0");

	memset(&v, HARNESS_MALLOC_FILL, sizeof v);
	memset(&bg, 0x5a, sizeof bg);
	plant_params();
	beepgen_create(&bg, &beep_cfg);
	v.beepgen = &bg;
	v.dp = &dp;
	v.beep_done = 1;
	v.out_format = 1;
	v.mode = 2;
	v.int_0014 = 0;
	hc = 3;
	cn = 4;
	r = voice_online(&v, rx, flt, tx, lin, &hc, &cn);
	diff_eq_int("voice_online answers 0 when int_0014 is 0", r, 0, 0);

	memset(&v, HARNESS_MALLOC_FILL, sizeof v);
	v.beepgen = &bg;
	v.dp = &dp;
	v.beep_done = 1;
	v.out_format = 1;
	v.mode = 2;
	v.int_0014 = 0;
	hc = 3;
	cn = 4;
	r = voice_duplex(&v, rx, flt, tx, lin, &hc, &cn);
	diff_eq_int("voice_duplex answers 5 when int_0014 is 0", r, 5, 0);

	return diff_end();
}

/*
 * The diagnostic transcript.  Two gated sites reach this binary: the block
 * handlers' one line "beepgend end, send ok", printed when the beep queue
 * retires, and `voice_set_tx`'s "PCM 8 bit.", which no handler ever prints --
 * so the setter is driven here too, or that line would go untested.
 */
static int
t_debug(void)
{
	static const unsigned int levels[] = { 0, 1, 2 };
	static struct voice_ctx sa, sb;
	static struct detector sda, sdb;
	unsigned int l;

	diff_begin("the gated lines at levels 0, 1 and 2");

	for (l = 0; l < 3; l++) {
		unsigned int lvl = levels[l];

		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		online_case(0, 1, 8, 2, 2, 1, 0, 1000 + (long)lvl);
		duplex_case(0, 1, 8, 3, 3, 1, 0, 1000 + (long)lvl);
		memset(&sa, HARNESS_MALLOC_FILL, sizeof sa);
		memset(&sb, HARNESS_MALLOC_FILL, sizeof sb);
		memset(&sda, HARNESS_MALLOC_FILL, sizeof sda);
		memset(&sdb, HARNESS_MALLOC_FILL, sizeof sdb);
		sa.detector = &sda;
		sb.detector = &sdb;
		sa.detector_enable_tx = 0x3f;
		sb.detector_enable_tx = 0x3f;
		ref_voice_set_tx(&sa);
		voice_set_tx(&sb);
		dsplib_debug_capture_on = 0;

		diff_eq_int("transcript line count at level %ld",
			    dsplib_debug_capture_lines(0),
			    dsplib_debug_capture_lines(1), (long)lvl);
		diff_eq_int("transcript text at level %ld",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)lvl);
		if (lvl > 1) {
			diff_eq_int("the beep-finished line is there at %ld",
				    strstr(dsplib_debug_capture_text(1),
					   "beepgend end, send ok") != 0, 1,
				    (long)lvl);
			diff_eq_int("voice_set_tx's format line too, at %ld",
				    strstr(dsplib_debug_capture_text(1),
					   "PCM 8 bit.") != 0, 1, (long)lvl);
		} else {
			diff_eq_int("nothing is printed below level 2",
				    dsplib_debug_capture_lines(1), 0,
				    (long)lvl);
		}
	}
	set_level(0);
	return diff_end();
}

static int
t_coverage(void)
{
	diff_begin("every arm was taken");
	diff_eq_int("voice_online calls", n_online > 100, 1, 0);
	diff_eq_int("voice_duplex calls", n_duplex > 60, 1, 0);
	diff_eq_int("beep-done linear arm", arm_done_lin > 0, 1, 0);
	diff_eq_int("beep-done float arm", arm_done_flt > 0, 1, 0);
	diff_eq_int("beeping linear arm", arm_beep_lin > 0, 1, 0);
	diff_eq_int("beeping float arm", arm_beep_flt > 0, 1, 0);
	diff_eq_int("the beep finished inside a block", arm_online_finish > 0,
		    1, 0);
	diff_eq_int("voice_online returned 2", arm_online_mode > 0, 1, 0);
	diff_eq_int("the short-beep zero fill ran", arm_online_zerofill > 0, 1,
		    0);
	diff_eq_int("voice_duplex's beep finished", arm_duplex_finish > 0, 1,
		    0);
	diff_eq_int("voice_duplex returned 5", arm_duplex_mode > 0, 1, 0);
	diff_eq_int("voice_duplex skipped the beep", arm_duplex_skipped > 0, 1,
		    0);
	fprintf(stderr,
		"t_voicedp: online=%ld duplex=%ld arms=%ld/%ld/%ld/%ld "
		"finish=%ld/%ld mode=%ld/%ld fill=%ld skip=%ld\n",
		n_online, n_duplex, arm_done_lin, arm_done_flt, arm_beep_lin,
		arm_beep_flt, arm_online_finish, arm_duplex_finish,
		arm_online_mode, arm_duplex_mode, arm_online_zerofill,
		arm_duplex_skipped);
	return diff_end();
}

int
main(void)
{
	int failed = 0;

	failed |= t_setters();
	failed |= t_online();
	failed |= t_duplex();
	failed |= t_tail_difference();
	failed |= t_debug();
	failed |= t_coverage();
	return failed;
}
