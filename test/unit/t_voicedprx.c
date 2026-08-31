/*
 * t_voicedprx.c -- differential test of `voice_rx` (0xaf2d0, 949 bytes) and
 * `voice_set_rx` (0xaf190, 305).
 *
 * FOUR STAGES, DRIVEN TOGETHER.  `voice_rx` removes a running DC estimate
 * from the float block, scales it by the gain its output format selects,
 * converts it to escaped u-law, and lets `silence_progress` append to the
 * result.  Every stage feeds the next, so the observable is the whole output
 * byte stream plus the whole context, and both are compared byte for byte.
 *
 * THE DC ESTIMATE IS STATEFUL AND THAT IS THE POINT.  It is seeded whole on
 * the first block and folded at one part in a hundred after that, so a wrong
 * blend shows up only from the SECOND block onwards -- every sequence below
 * therefore runs several blocks through one context rather than one block
 * through a fresh one.
 *
 * FLOATS ARE COMPARED EXACTLY, and the comparison is of the u-law BYTES as
 * well as of the float buffer.  u-law quantises, so a small float error is
 * invisible in the bytes and visible in the buffer; both are checked.
 *
 * THE HOST CALLBACK IS OURS AND IS SHARED.  `voice_set_rx` reads three gains
 * through `cfg.fn_04`, which finding F8788 shows is the settings callback and
 * not the void hook `beepgen.h` calls it.  Both sides are given the same
 * function and the same answers, so the two contexts differ in nothing but
 * their own pointers.
 *
 * COVERAGE IS ASSERTED FROM THE RUN (F134).
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/detector.h"
#include "dsplib/pcm.h"
#include "dsplib/silence.h"
#include "dsplib/sysdep.h"
#include "dsplib/voice.h"

extern unsigned int ref_dsplibs_debug_level;

extern int ref_voice_rx(struct voice_ctx *v, short *rx_lin, float *rx_flt,
			float *tx_flt, short *tx_lin,
			unsigned short *hostcount, unsigned short *countp);
extern void ref_voice_set_rx(struct voice_ctx *v);
extern struct silence *ref_silence_create(struct silence *s, void *obj,
					  unsigned int (*q)(void *, int));

#define NBUF	4096

/*
 * What the host answers.  Three gains, the silence detector's two settings
 * (`voice_set_rx` hands the same callback to `silence_create`, which is
 * finding F8788's other half), and a catch-all.
 */
static int query_calls;
static unsigned int query_answer[6];

static unsigned int
query(void *obj, int what)
{
	(void)obj;
	query_calls++;
	switch (what) {
	case VOICE_PARAM_RX_GAIN_FMT1:
		return query_answer[0];
	case VOICE_PARAM_RX_GAIN_FMT3:
		return query_answer[1];
	case VOICE_PARAM_RX_GAIN_OTHER:
		return query_answer[2];
	case SILENCE_PARAM_LEVEL:
		return query_answer[3];
	case SILENCE_PARAM_TIME:
		return query_answer[4];
	default:
		return query_answer[5];
	}
}

static int modem_cookie;

/* Non-zero makes rx_block hand both sides a constant (so, silent) block. */
static int quiet_input;

static long n_blocks, n_setters;
static long arm_empty, arm_seed, arm_fold, arm_disarmed;
static long arm_fmt1, arm_fmt3, arm_other, arm_toolong;
static long arm_dle_double, arm_marker, arm_etx, arm_mode, arm_sil_escape;

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

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
sil_compare(const char *what, const struct silence *ours,
	    const struct silence *ref, long tag)
{
	static struct silence ca, cb;

	/*
	 * NOTHING IS MASKED HERE.  The detector's `obj` is the modem cookie,
	 * which both sides share by construction, so it is under test like
	 * every other byte -- a setter that stored the wrong handle would
	 * otherwise be invisible, and was, until this mask came off.
	 */
	cb = *ours;
	ca = *ref;
	diff_eq_obj(what, struct silence, &cb, &ca, tag);
}

static void
byte_compare(const char *what, const unsigned char *ours,
	     const unsigned char *ref, int n, long tag)
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

/* ------------------------------------------------------------------ */

/*
 * One pair of contexts, armed through `voice_set_rx` so the gains, the
 * marker countdown and the silence detector all come from the object under
 * test rather than from this file.
 */
struct pair {
	struct voice_ctx a, b;		/* ref, ours                       */
	struct detector da, db;
	struct silence sa, sb;
};

static struct pair P;

static void
pair_arm(int out_format, unsigned short marker_period, long tag)
{
	memset(&P, HARNESS_MALLOC_FILL, sizeof P);

	P.a.detector = &P.da;
	P.b.detector = &P.db;
	P.a.silence = &P.sa;
	P.b.silence = &P.sb;
	/*
	 * ONE COOKIE FOR BOTH SIDES.  The handle is stored in the context and
	 * copied into the silence detector, so two different cookies would
	 * differ in two compared objects and say nothing about the code.  The
	 * callback ignores it.
	 */
	P.a.cfg.modem = &modem_cookie;
	P.b.cfg.modem = &modem_cookie;
	/*
	 * The cast this used to carry is gone: `voice_ctx.cfg` is a
	 * `struct voice_config` since finding F8813, and its `fn_04` is
	 * declared with the S-register getter's real signature, which is what
	 * `query` already has.
	 */
	P.a.cfg.fn_04 = query;
	P.b.cfg.fn_04 = query;
	P.a.out_format = out_format;
	P.b.out_format = out_format;
	P.a.marker_period = marker_period;
	P.b.marker_period = marker_period;
	P.a.detector_enable_rx = 0x3f;
	P.b.detector_enable_rx = 0x3f;
	P.a.dle_can = 0;
	P.b.dle_can = 0;
	P.a.mode = 0;
	P.b.mode = 0;
	P.a.int_0014 = 0;
	P.b.int_0014 = 0;

	ref_voice_set_rx(&P.a);
	voice_set_rx(&P.b);

	diff_eq_int("voice_set_rx installs voice_rx",
		    P.b.handler == voice_rx, 1, tag);
	diff_eq_int("voice_set_rx installs ref_voice_rx on the ref side",
		    (void *)P.a.handler == (void *)ref_voice_rx, 1, tag);
	diff_eq_int("voice_set_rx arms the path", P.b.rx_armed, 1, tag);
	diff_eq_int("voice_set_rx writes mode 0", P.b.mode, 0, tag);
	diff_eq_int("voice_set_rx writes the 8 kHz format",
		    (long)P.b.rate_bits.both, VOICE_RATE_BITS(8, 8000), tag);
	diff_eq_int("voice_set_rx seeds the DC estimate", P.b.dc_init, 1, tag);
	ctx_compare("context after voice_set_rx", &P.b, &P.a, tag);
	diff_eq_obj("detector after voice_set_rx", struct detector, &P.db,
		    &P.da, tag);
	sil_compare("silence after voice_set_rx", &P.sb, &P.sa, tag);
	n_setters++;
	if (marker_period != 0)
		arm_marker++;
}

/*
 * One block through both sides.  `seed` picks the sample pattern; the same
 * pattern goes to both, and both float buffers start from the same fill.
 */
static void
rx_block(unsigned short count, int dle_can, int rx_armed, int int_0014,
	 unsigned int seed, long tag)
{
	static short rxa[NBUF], rxb[NBUF];
	static float flta[NBUF], fltb[NBUF];
	static float txa[NBUF], txb[NBUF];
	static unsigned char outa[NBUF], outb[NBUF];
	unsigned short hca, hcb, cna, cnb;
	int ra, rb, i;
	int was_dc_init;

	P.a.dle_can = dle_can;
	P.b.dle_can = dle_can;
	P.a.rx_armed = (short)rx_armed;
	P.b.rx_armed = (short)rx_armed;
	P.a.int_0014 = int_0014;
	P.b.int_0014 = int_0014;
	was_dc_init = P.b.dc_init;

	for (i = 0; i < NBUF; i++) {
		unsigned int s = seed + (unsigned int)i * 2654435761u;

		if (quiet_input) {
			rxa[i] = 0;
			txa[i] = 0.125f;
		} else {
			rxa[i] = (short)(s >> 16);
			txa[i] = (float)((int)((s >> 8) & 0xffffu) - 32768)
				 / 32768.0f;
		}
		rxb[i] = rxa[i];
		txb[i] = txa[i];
	}
	memset(flta, HARNESS_MALLOC_FILL, sizeof flta);
	memset(fltb, HARNESS_MALLOC_FILL, sizeof fltb);
	memset(outa, HARNESS_MALLOC_FILL, sizeof outa);
	memset(outb, HARNESS_MALLOC_FILL, sizeof outb);
	hca = 0x4321;
	hcb = 0x4321;
	cna = count;
	cnb = count;

	ra = ref_voice_rx(&P.a, rxa, flta, txa, (short *)outa, &hca, &cna);
	rb = voice_rx(&P.b, rxb, fltb, txb, (short *)outb, &hcb, &cnb);

	diff_eq_int("voice_rx return", rb, ra, tag);
	diff_eq_int("voice_rx hostcount", hcb, hca, tag);
	diff_eq_int("voice_rx countp", cnb, cna, tag);
	ctx_compare("context after voice_rx", &P.b, &P.a, tag);
	sil_compare("silence after voice_rx", &P.sb, &P.sa, tag);
	diff_eq_obj("detector after voice_rx", struct detector, &P.db, &P.da,
		    tag);
	byte_compare("voice_rx output bytes", outb, outa, NBUF, tag);
	flt_compare("voice_rx works tx_flt in place", txb, txa, NBUF, tag);
	flt_compare("voice_rx leaves rx_flt alone", fltb, flta, NBUF, tag);
	for (i = 0; i < NBUF; i++)
		if (rxa[i] != rxb[i]) {
			diff_eq_int("voice_rx leaves rx_lin alone", i, -1, tag);
			break;
		}
	if (i == NBUF)
		diff_eq_int("voice_rx leaves rx_lin alone", 0, 0, tag);

	n_blocks++;
	if (count == 0)
		arm_empty++;
	if (rx_armed != 1)
		arm_disarmed++;
	else if (was_dc_init)
		arm_seed++;
	else
		arm_fold++;
	if (rb == 7)
		arm_toolong++;
	if (rb == 8)
		arm_etx++;
	if (rb == 4)
		arm_mode++;
	/* A doubled DLE shows as two 0x10 bytes running. */
	for (i = 0; i + 1 < cnb; i++)
		if (outb[i] == 0x10 && outb[i + 1] == 0x10) {
			arm_dle_double++;
			break;
		}
	/*
	 * And a silence escape as 0x10 followed by 'q' or 's' -- which is the
	 * only thing that can appear AFTER the byte count voice_rx published,
	 * so it is the seam between the two functions made visible.
	 */
	for (i = 0; i + 1 < cnb; i++)
		if (outb[i] == 0x10
		    && (outb[i + 1] == 'q' || outb[i + 1] == 's')) {
			arm_sil_escape++;
			break;
		}
}

/* ------------------------------------------------------------------ */

static int
t_setter(void)
{
	static const unsigned int answers[][3] = {
		{ 0, 0, 0 },
		{ 128, 256, 64 },
		{ 1, 2, 3 },
		{ 0x7fffffffu, 0x80000000u, 0xffffffffu },
		{ 4096, 8192, 16384 }
	};
	unsigned int q;
	int f;

	diff_begin("voice_set_rx over the host's answers and both marker states");

	for (q = 0; q < sizeof answers / sizeof answers[0]; q++) {
		query_answer[0] = answers[q][0];
		query_answer[1] = answers[q][1];
		query_answer[2] = answers[q][2];
		query_answer[3] = 0xdeadbeefu;
		for (f = 0; f < 4; f++) {
			pair_arm(f, 0, (long)(q * 10 + f));
			pair_arm(f, 3, (long)(q * 10 + f) + 500);
		}
	}

	/*
	 * The three gains, stated absolutely: each is the host's answer as an
	 * UNSIGNED int, divided by 128.  0x80000000 is the case that
	 * separates unsigned from signed and it is in the sweep above.
	 */
	query_answer[0] = 0x80000000u;
	query_answer[1] = 256;
	query_answer[2] = 0;
	pair_arm(1, 0, 900);
	diff_eq_int("the fmt1 gain is the unsigned answer over 128",
		    P.b.gain_fmt1 == (float)0x80000000u / 128.0f, 1, 900);
	diff_eq_int("the fmt3 gain is 2.0", P.b.gain_fmt3 == 2.0f, 1, 900);
	diff_eq_int("the other gain is 0.0", P.b.gain_other == 0.0f, 1, 900);
	diff_eq_int("the DC estimate starts at zero", P.b.dc == 0.0f, 1, 900);

	/* And the marker countdown is period * 800. */
	pair_arm(1, 7, 901);
	diff_eq_int("the marker countdown is period * 800",
		    (long)P.b.marker_countdown, 7 * 800, 901);

	return diff_end();
}

/*
 * Several blocks through one context, so the DC estimate's fold is exercised
 * and not only its seed.
 */
static int
t_blocks(void)
{
	static const unsigned short counts[] = { 0, 1, 2, 40, 160, 200 };
	unsigned int c;
	int f;
	long tag = 0;

	diff_begin("voice_rx over both DC arms, every format and every length");

	query_answer[0] = 128 * 2;
	query_answer[1] = 128 * 3;
	query_answer[2] = 128;
	query_answer[3] = 0;
	query_answer[4] = 0;
	query_answer[5] = 0;

	for (f = 0; f < 4; f++)
		for (c = 0; c < sizeof counts / sizeof counts[0]; c++) {
			pair_arm(f, 0, tag);
			/* Four blocks: seed, then three folds. */
			rx_block(counts[c], 0, 1, 0, 0x1000u + tag, tag);
			tag++;
			rx_block(counts[c], 0, 1, 0, 0x2000u + tag, tag);
			tag++;
			rx_block(counts[c], 0, 1, 0, 0x3000u + tag, tag);
			tag++;
			rx_block(counts[c], 0, 1, 0, 0x4000u + tag, tag);
			tag++;
			if (f == 1)
				arm_fmt1 += 4;
			else if (f == 3)
				arm_fmt3 += 4;
			else
				arm_other += 4;
		}
	return diff_end();
}

/*
 * The 200-sample bound.  Formats 1 and 3 refuse a longer block outright and
 * the float arm does not test it at all -- that asymmetry is D-worthy and is
 * driven here from both sides.
 */
static int
t_too_long(void)
{
	long tag = 300;
	int f;

	diff_begin("the 200-sample bound, and the arm that does not have one");

	for (f = 0; f < 4; f++) {
		pair_arm(f, 0, tag);
		rx_block(200, 0, 1, 0, 0x5000u, tag++);
		rx_block(201, 0, 1, 0, 0x6000u, tag++);
		rx_block(400, 0, 1, 0, 0x7000u, tag++);
	}
	return diff_end();
}

/*
 * The marker, the disarmed path, the <DLE><ETX> close and the mode
 * comparison -- the four things that decide what `voice_rx` returns.
 */
static int
t_arms(void)
{
	long tag = 400;

	diff_begin("the marker, the abort, the stream close and the mode test");

	/*
	 * A marker period of 1 is 800 samples, so 160-sample blocks fire it
	 * on the fifth.  Six blocks are run so the reload is exercised too.
	 */
	pair_arm(0, 1, tag);
	rx_block(160, 0, 1, 0, 0x8000u, tag++);
	rx_block(160, 0, 1, 0, 0x8100u, tag++);
	rx_block(160, 0, 1, 0, 0x8200u, tag++);
	rx_block(160, 0, 1, 0, 0x8300u, tag++);
	rx_block(160, 0, 1, 0, 0x8400u, tag++);
	rx_block(160, 0, 1, 0, 0x8500u, tag++);
	diff_eq_int("the marker countdown reloaded",
		    P.b.marker_countdown > 0, 1, tag);

	/* Disarmed: the abort path, whatever the block holds. */
	pair_arm(1, 0, tag);
	rx_block(64, 0, 0, 0, 0x9000u, tag++);
	rx_block(64, 0, 2, 0, 0x9100u, tag++);
	rx_block(0, 0, 0, 0, 0x9200u, tag++);

	/* <DLE><CAN> pending: the stream closes and the path disarms. */
	pair_arm(1, 0, tag);
	rx_block(64, 0, 1, 0, 0xa000u, tag++);
	rx_block(64, 1, 1, 0, 0xa100u, tag++);
	diff_eq_int("the stream close disarms the path", P.b.rx_armed, 0, tag);
	diff_eq_int("...and clears the detector", P.db.enable, 0, tag);
	/* The next block therefore takes the abort path. */
	rx_block(64, 1, 0, 0, 0xa200u, tag++);

	/* The mode comparison, both ways. */
	pair_arm(1, 0, tag);
	rx_block(64, 0, 1, 0, 0xb000u, tag++);
	rx_block(64, 0, 1, 5, 0xb100u, tag++);
	rx_block(0, 0, 1, 5, 0xb200u, tag++);

	return diff_end();
}

/*
 * THE SILENCE DETECTOR'S OWN OUTPUT, which is the only thing that can move
 * `*countp` after `voice_rx` has published it.
 *
 * `silence_progress` accumulates 800 samples before it decides anything and
 * appends nothing until a run of silent blocks passes the time setting, so a
 * fixture of short noisy blocks never sees it fire -- and three mutants
 * survived the whole suite because of that: one that gave the detector the
 * wrong handle, one that published the byte count after the call instead of
 * before, and one that pointed the call at the head of the buffer instead of
 * the tail.  All three are about the SEAM between the two, and the seam only
 * exists once the detector emits.
 *
 * So: the settings are turned on through the same callback `voice_set_rx`
 * hands the detector, the input is constant (silent after its own mean is
 * removed), and enough blocks are run to cross 800 samples several times.
 */
static int
t_silence(void)
{
	long tag = 800;
	int b;
	int emitted = 0;

	diff_begin("what silence_progress appends after voice_rx's own bytes");

	query_answer[0] = 128;
	query_answer[1] = 128;
	query_answer[2] = 128;
	query_answer[3] = 1;		/* SILENCE_PARAM_LEVEL: row 1     */
	query_answer[4] = 1;		/* SILENCE_PARAM_TIME: one block  */
	query_answer[5] = 0;

	quiet_input = 1;
	pair_arm(0, 0, tag);
	for (b = 0; b < 20; b++) {
		rx_block(160, 0, 1, 0, 0xd000u + (unsigned)b, tag++);
		if (P.sb.nsamp != 0 || P.sb.count != 0)
			emitted = 1;
	}
	/*
	 * The denominator, and it is not `count`: the detector RESETS that
	 * every time it emits, so reading it at the end says nothing.  What
	 * says something is the escape itself appearing in the byte stream,
	 * after the count voice_rx published -- which is exactly the seam the
	 * three surviving mutants lived in.  F134.
	 */
	diff_eq_int("the silence detector accumulated", emitted, 1, tag);
	diff_eq_int("...and appended at least one escape", arm_sil_escape > 0,
		    1, tag);

	/* And again with the format that converts through rx_lin. */
	{
		long before = arm_sil_escape;

		pair_arm(1, 0, tag);
		for (b = 0; b < 20; b++)
			rx_block(160, 0, 1, 0, 0xe000u + (unsigned)b, tag++);
		diff_eq_int("the same with a 16-bit input format",
			    arm_sil_escape > before, 1, tag);
	}

	quiet_input = 0;
	query_answer[3] = 0;
	query_answer[4] = 0;
	return diff_end();
}

static int
t_debug(void)
{
	static const unsigned int levels[] = { 0, 1, 2 };
	unsigned int l;

	diff_begin("voice_rx's three diagnostic lines at levels 0, 1 and 2");

	for (l = 0; l < 3; l++) {
		unsigned int lvl = levels[l];

		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		pair_arm(1, 0, 700 + (long)lvl);
		rx_block(64, 1, 1, 0, 0xc000u, 700 + (long)lvl);
		rx_block(64, 0, 0, 0, 0xc100u, 701 + (long)lvl);
		rx_block(400, 0, 1, 0, 0xc200u, 702 + (long)lvl);
		dsplib_debug_capture_on = 0;

		diff_eq_int("transcript line count at level %ld",
			    dsplib_debug_capture_lines(0),
			    dsplib_debug_capture_lines(1), (long)lvl);
		diff_eq_int("transcript text at level %ld",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)lvl);
		if (lvl > 1) {
			const char *t = dsplib_debug_capture_text(1);

			diff_eq_int("the sample-rate line is there",
				    strstr(t, "Sample rate 8000") != 0, 1,
				    (long)lvl);
			diff_eq_int("the abort line is there",
				    strstr(t, "RX WAIT ABORT") != 0, 1,
				    (long)lvl);
			diff_eq_int("the stream-close line is there",
				    strstr(t, "Send DLE ETX") != 0, 1,
				    (long)lvl);
			diff_eq_int("the over-long line is there",
				    strstr(t, "rx buffer greater than internal")
					!= 0, 1, (long)lvl);
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
	diff_eq_int("setters run", n_setters > 40, 1, 0);
	diff_eq_int("blocks run", n_blocks > 100, 1, 0);
	diff_eq_int("empty blocks", arm_empty > 0, 1, 0);
	diff_eq_int("the DC seed arm", arm_seed > 0, 1, 0);
	diff_eq_int("the DC fold arm", arm_fold > 0, 1, 0);
	diff_eq_int("the disarmed arm", arm_disarmed > 0, 1, 0);
	diff_eq_int("format 1", arm_fmt1 > 0, 1, 0);
	diff_eq_int("format 3", arm_fmt3 > 0, 1, 0);
	diff_eq_int("the other formats", arm_other > 0, 1, 0);
	diff_eq_int("the over-long refusal", arm_toolong > 0, 1, 0);
	/*
	 * D984: the doubling arm CANNOT FIRE, and this is the measurement
	 * rather than the claim.  `voice_rx` hands `linear2ulaw` a value of
	 * the form `(short)x >> 2`, which lies in [-8192, 8191], and over
	 * that whole domain the encoder never answers 0x10 -- so the DLE
	 * shield is dead code in the object.  The sweep prints its own
	 * denominator, because a check that cannot fail looks exactly like a
	 * check that passes (F134).
	 */
	{
		int x, hits = 0, seen = 0;

		for (x = -8192; x <= 8191; x++) {
			seen++;
			if (linear2ulaw(x) == VOICE_DLE)
				hits++;
		}
		diff_eq_int("the reachable domain was swept", seen, 16384, 0);
		diff_eq_int("no reachable sample encodes as a DLE", hits, 0, 0);
		diff_eq_int("...so no doubling was seen either", arm_dle_double,
			    0, 0);
		/* And the encoder DOES answer 0x10 somewhere, so the sweep is
		 * finding a real absence and not a broken call. */
		diff_eq_int("linear2ulaw answers 0x10 outside that domain",
			    linear2ulaw(-16250) == VOICE_DLE, 1, 0);
	}
	diff_eq_int("the marker was armed", arm_marker > 0, 1, 0);
	diff_eq_int("the stream was closed", arm_etx > 0, 1, 0);
	diff_eq_int("a mode mismatch was answered", arm_mode > 0, 1, 0);
	diff_eq_int("the host callback fired", query_calls > 100, 1, 0);
	diff_eq_int("silence escapes reached the stream", arm_sil_escape > 0,
		    1, 0);
	fprintf(stderr,
		"t_voicedprx: %ld setters, %ld blocks, empty=%ld seed=%ld "
		"fold=%ld disarmed=%ld fmt=%ld/%ld/%ld long=%ld dbl=%ld "
		"mark=%ld etx=%ld mode=%ld queries=%d\n",
		n_setters, n_blocks, arm_empty, arm_seed, arm_fold,
		arm_disarmed, arm_fmt1, arm_fmt3, arm_other, arm_toolong,
		arm_dle_double, arm_marker, arm_etx, arm_mode, query_calls);
	return diff_end();
}

int
main(void)
{
	int failed = 0;

	failed |= t_setter();
	failed |= t_blocks();
	failed |= t_too_long();
	failed |= t_arms();
	failed |= t_silence();
	failed |= t_debug();
	failed |= t_coverage();
	return failed;
}
