/*
 * t_v23modem.c -- differential test of the V.23 composite.
 *
 * There is not much arithmetic in this module and that is the point: what it
 * does is DECIDE things, and every one of those decisions is a place where a
 * reconstruction can be plausible and wrong.  Which receiver gets built.
 * Which frequencies and which bit-period table the one transmitter is given.
 * Whether the answer tone is audible at this end.  Whether the caller's data
 * survives to reach the line.  None of those show up as a slightly different
 * sample; they show up as a modem that talks to itself perfectly and to
 * nothing else.
 *
 * So the first group compares the whole object GRAPH after create -- the
 * composite, the transmitter, whichever receiver was built, and the tone
 * generator -- for every combination of the two configuration inputs.  That
 * single comparison pins the frequencies, the tables, the tone scale and both
 * sub-objects' entire layout at once, which is how the V.8 create path was
 * validated and for the same reason.
 *
 * The rest drives it: the three-state answer sequence with a shortened sample
 * rate so it does not take three seconds of test, and then both ends in data
 * against a real signal from v23FP_tx.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v23fp.h"

extern void *ref_CreateV23Modem(void *m, int mode, const void *cfg);
extern void ref_DeleteV23Modem(void *m);
extern short ref_V23ModemMain(void *m, int *tx_bits, int *tx_nbits,
			      short *tx_out, int tx_count, short *rx_in,
			      int rx_count, int *rx_bits, int *rx_nbits);

/* Coverage. */
static int saw_tone_state, saw_silence_state, saw_data_state;
static int saw_tone_audible, saw_tone_silent;
static int saw_marked;		/* the caller's data was replaced by mark  */
static int saw_passed;		/* and a block where it was not            */
static int saw_rx_carrier;	/* a receiver reported carrier up          */
static int saw_rx_bits;		/* and produced bits                       */

/* ------------------------------------------------------------------ */

static void
compare_bytes(const char *what, const void *ap, const void *bp, unsigned n,
	      const unsigned *skip, unsigned nskip)
{
	const unsigned char *a = ap;
	const unsigned char *b = bp;
	unsigned i, k;

	for (i = 0; i < n; i++) {
		for (k = 0; k < nskip; k++)
			if (i >= skip[k] && i < skip[k] + sizeof(void *))
				break;
		if (k < nskip)
			continue;
		diff_eq_int(what, a[i], b[i], i);
	}
}

static void
compare_shorts(const char *what, const short *a, const short *b, int n)
{
	int i;

	for (i = 0; i < n; i++)
		diff_eq_int(what, a[i], b[i], i);
}

/*
 * The transmitter.  `period` and `tone` hold addresses; the table `period`
 * points at is compared through it, because which of the two tables this end
 * was handed is the single most load-bearing decision in CreateV23Modem.
 */
static void
compare_tx(const struct v23tx *a, const struct v23tx *b)
{
	static const unsigned skip[] = {
		__builtin_offsetof(struct v23tx, period),
		__builtin_offsetof(struct v23tx, tone)
	};

	compare_bytes("tx object byte %ld", a, b, sizeof(*a), skip, 2);
	diff_eq_int("tx period length", a->period_len, b->period_len, 0);
	compare_shorts("tx bit period[%ld]", a->period, b->period,
		       a->period_len);
}

static void
compare_rx(const struct v23rx *a, const struct v23rx *b)
{
	static const unsigned skip[] = {
		__builtin_offsetof(struct v23rx, tone),
		__builtin_offsetof(struct v23rx, agc)
			+ __builtin_offsetof(struct fpm_agc_cfg, alpha),
		__builtin_offsetof(struct v23rx, agc)
			+ __builtin_offsetof(struct fpm_agc_cfg, beta),
		__builtin_offsetof(struct v23rx, det_agc)
			+ __builtin_offsetof(struct fpm_agc_cfg, alpha),
		__builtin_offsetof(struct v23rx, det_agc)
			+ __builtin_offsetof(struct fpm_agc_cfg, beta),
		__builtin_offsetof(struct v23rx, mrf)
			+ __builtin_offsetof(struct fpm_mrf_cfg, coeff),
		__builtin_offsetof(struct v23rx, mrf)
			+ __builtin_offsetof(struct fpm_mrf, history),
		__builtin_offsetof(struct v23rx, fsd)
			+ __builtin_offsetof(struct fpm_fsd_cfg, fir),
		__builtin_offsetof(struct v23rx, fsd)
			+ __builtin_offsetof(struct fpm_fsd_cfg, iir),
		__builtin_offsetof(struct v23rx, fsd)
			+ __builtin_offsetof(struct fpm_fsd, trace),
		__builtin_offsetof(struct v23rx, fsd)
			+ __builtin_offsetof(struct fpm_fsd, fir_hist),
		__builtin_offsetof(struct v23rx, fsd)
			+ __builtin_offsetof(struct fpm_fsd, iir_hist),
		__builtin_offsetof(struct v23rx, iir_coeff),
		__builtin_offsetof(struct v23rx, iir_state)
	};

	compare_bytes("rx object byte %ld", a, b, sizeof(*a), skip, 14);
	compare_shorts("rx iir state[%ld]", a->iir_state, b->iir_state, 16);
	compare_shorts("rx mrf history[%ld]", a->mrf.history, b->mrf.history,
		       a->mrf.history_len);
	compare_shorts("rx fsd trace[%ld]", a->fsd.trace, b->fsd.trace,
		       a->fsd.cfg.trace_len);
	compare_shorts("rx fsd fir[%ld]", a->fsd.fir_hist, b->fsd.fir_hist,
		       a->fsd.cfg.fir_taps);
	compare_shorts("rx fsd iir[%ld]", a->fsd.iir_hist, b->fsd.iir_hist,
		       2 * a->fsd.cfg.iir_len);
}

static void
compare_bwch(const struct bwchdem *a, const struct bwchdem *b)
{
	static const unsigned skip[] = {
		__builtin_offsetof(struct bwchdem, tone),
		__builtin_offsetof(struct bwchdem, agc)
			+ __builtin_offsetof(struct fpm_agc_cfg, alpha),
		__builtin_offsetof(struct bwchdem, agc)
			+ __builtin_offsetof(struct fpm_agc_cfg, beta),
		__builtin_offsetof(struct bwchdem, iir_coeff),
		__builtin_offsetof(struct bwchdem, iir_state)
	};

	compare_bytes("bwch object byte %ld", a, b, sizeof(*a), skip, 5);
	compare_shorts("bwch iir state[%ld]", a->iir_state, b->iir_state, 12);
}

/*
 * The tone generator, as far as this module is responsible for it.
 *
 * Everything up to the two detector buffers, which is the configuration
 * CreateV23Modem chose plus the oscillator state -- so the 2100 Hz, the
 * disabled phase reversals and the scale that differs between the two ends
 * are all in here.  What comes after is FPM_TONE_create's own business and
 * t_fpm_tone compares it.
 */
static void
compare_tone(const struct fpm_tone *a, const struct fpm_tone *b)
{
	static const unsigned skip[] = {
		__builtin_offsetof(struct fpm_tone_cfg, src)
	};

	compare_bytes("tone header byte %ld", a, b,
		      __builtin_offsetof(struct fpm_tone, kernel), skip, 1);
	compare_bytes("tone detector byte %ld",
		      (const char *)a + __builtin_offsetof(struct fpm_tone,
							   hist_idx),
		      (const char *)b + __builtin_offsetof(struct fpm_tone,
							   hist_idx),
		      __builtin_offsetof(struct fpm_tone, r4c)
		      - __builtin_offsetof(struct fpm_tone, hist_idx),
		      NULL, 0);
}

static void
compare_graph(const char *tag, const struct v23modem *a,
	      const struct v23modem *b)
{
	static const unsigned skip[] = {
		__builtin_offsetof(struct v23modem, answer_tone),
		__builtin_offsetof(struct v23modem, tx),
		__builtin_offsetof(struct v23modem, rx)
	};

	compare_bytes(tag, a, b, sizeof(*a), skip, 3);

	/* That the two agree about whether there IS a tone, before using it. */
	diff_eq_int("a tone was built", a->answer_tone != NULL,
		    b->answer_tone != NULL, 0);
	if (a->answer_tone != NULL && b->answer_tone != NULL)
		compare_tone(a->answer_tone, b->answer_tone);

	compare_tx(a->tx, b->tx);
	if (a->mode != 0)
		compare_bwch(a->rx, b->rx);
	else
		compare_rx(a->rx, b->rx);
}

/* ------------------------------------------------------------------ */

/* Generate FSK from `bits` using the transmitter this tree already proves. */
static void
generate(short *out, int n, short mark, short space, const short *period,
	 const int *bits, int nbits)
{
	struct v23tx *tx;
	int at = 0;
	int done = 0;

	tx = v23FP_tx_create(NULL, mark, space, 3, period, 0);
	while (done < n) {
		int chunk = n - done > 160 ? 160 : n - done;
		int used = 0;

		v23FP_tx_progress(tx, out + done, chunk, bits + at, &used);
		done += chunk;
		at += used;
		if (at + 32 > nbits)
			at = 0;
	}
	v23FP_tx_delete(tx);
}

/*
 * One call on both sides, with every buffer poisoned first and every buffer
 * compared afterwards.
 */
static short
step(struct v23modem *a, struct v23modem *b, const short *rx_signal,
     int count, const int *want_bits)
{
	int tb_a[64], tb_b[64];
	int tn_a, tn_b;
	short to_a[192], to_b[192];
	short ri_a[192], ri_b[192];
	int rb_a[64], rb_b[64];
	int rn_a, rn_b;
	short rc_a, rc_b;
	int i;

	for (i = 0; i < 64; i++)
		tb_a[i] = tb_b[i] = want_bits[i];
	tn_a = tn_b = 24;
	memset(to_a, 0x5a, sizeof(to_a));
	memset(to_b, 0x5a, sizeof(to_b));
	memcpy(ri_a, rx_signal, count * sizeof(short));
	memcpy(ri_b, rx_signal, count * sizeof(short));
	memset(rb_a, 0x5a, sizeof(rb_a));
	memset(rb_b, 0x5a, sizeof(rb_b));
	rn_a = rn_b = 0x5a5a5a5a;

	rc_a = V23ModemMain(a, tb_a, &tn_a, to_a, count, ri_a, count, rb_a,
			    &rn_a);
	rc_b = ref_V23ModemMain(b, tb_b, &tn_b, to_b, count, ri_b, count, rb_b,
				&rn_b);

	diff_eq_int("return code", rc_a, rc_b, 0);
	diff_eq_int("transmit bits consumed", tn_a, tn_b, 0);
	diff_eq_int("receive bits produced", rn_a, rn_b, 0);
	for (i = 0; i < 64; i++) {
		diff_eq_int("transmit bit[%ld]", tb_a[i], tb_b[i], i);
		diff_eq_int("receive bit[%ld]", rb_a[i], rb_b[i], i);
	}
	compare_shorts("transmit sample[%ld]", to_a, to_b, count);
	compare_shorts("receive sample[%ld]", ri_a, ri_b, count);
	diff_eq_int("wrote past the transmit block", to_a[count], 0x5a5a, 0);
	compare_graph("object byte %ld", a, b);

	/* Coverage, read off ours. */
	if (a->state == 0)
		saw_tone_state++;
	else if (a->state == 1)
		saw_silence_state++;
	else
		saw_data_state++;
	if (rc_a == 0) {
		saw_rx_carrier++;
		if (rn_a > 0)
			saw_rx_bits++;
	}
	if (a->state == 2) {
		int marked = 1;

		for (i = 0; i < 64; i++)
			if (tb_a[i] != want_bits[i])
				break;
		if (i == 64)
			marked = 0;
		if (marked)
			saw_marked++;
		else
			saw_passed++;
	}
	return rc_a;
}

/*
 * Build one modem on each side and hand back both.  Every create in this file
 * goes through here so that the graph comparison is never skipped.
 */
static void
build(const char *tag, int mode, int answer_tone, int sample_rate,
      struct v23modem **ours, struct v23modem **ref)
{
	struct v23_cfg cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.answer_tone = (unsigned char)answer_tone;
	cfg.sample_rate = sample_rate;
	cfg.silence_limit = 100000;

	*ours = CreateV23Modem(NULL, mode, &cfg);
	*ref = ref_CreateV23Modem(NULL, mode, &cfg);
	compare_graph(tag, *ours, *ref);
}

/* ------------------------------------------------------------------ */

#define NSAMP 24000

static short fw_signal[NSAMP];	/* 1300/2100, what a terminal listens to */
static short bw_signal[NSAMP];	/*   390/450, what a host listens to     */
static short quiet[NSAMP];

static const short fw_period[3] = { 7, 7, 6 };
static const short bw_period[3] = { 107, 107, 106 };

int
main(void)
{
	static int idle[64], data[64], want[64];
	struct v23modem *a, *b;
	int rc = 0;
	int i;

	for (i = 0; i < 64; i++) {
		idle[i] = 1;
		data[i] = (i / 2) & 1;
		want[i] = (i / 3) & 1;	/* what the caller wants transmitted */
	}
	generate(fw_signal, NSAMP / 4, 1300, 2100, fw_period, idle, 64);
	generate(fw_signal + NSAMP / 4, NSAMP - NSAMP / 4, 1300, 2100,
		 fw_period, data, 64);
	generate(bw_signal, NSAMP / 3, 390, 450, bw_period, idle, 64);
	generate(bw_signal + NSAMP / 3, NSAMP - NSAMP / 3, 390, 450, bw_period,
		 data, 64);
	memset(quiet, 0, sizeof(quiet));

	diff_begin("CreateV23Modem: the whole graph, four configurations");
	/*
	 * Both ends, with and without the answer tone.  The four graphs differ
	 * in which receiver exists, which table the transmitter holds, whether
	 * a tone generator exists at all and what gain it runs at -- so this
	 * one group is where every decision in CreateV23Modem is checked.
	 */
	build("terminal, no tone: byte %ld", 0, 0, 8000, &a, &b);
	DeleteV23Modem(a);
	ref_DeleteV23Modem(b);
	build("terminal, answer tone: byte %ld", 0, 1, 8000, &a, &b);
	DeleteV23Modem(a);
	ref_DeleteV23Modem(b);
	build("host, no tone: byte %ld", 1, 0, 8000, &a, &b);
	DeleteV23Modem(a);
	ref_DeleteV23Modem(b);
	build("host, answer tone: byte %ld", 1, 1, 8000, &a, &b);
	if (a->answer_tone != NULL)
		saw_tone_audible += a->answer_tone->cfg.scale != 0;
	DeleteV23Modem(a);
	ref_DeleteV23Modem(b);

	/* And that a mode other than 1 is still a host. */
	build("host, mode 7: byte %ld", 7, 1, 8000, &a, &b);
	DeleteV23Modem(a);
	ref_DeleteV23Modem(b);
	rc |= diff_end();

	diff_begin("CreateV23Modem: the answer tone is silent at the terminal");
	{
		struct v23modem *t, *tref;

		build("terminal tone: byte %ld", 0, 1, 8000, &t, &tref);
		diff_eq_int("a generator was still built",
			    t->answer_tone != NULL, 1, 0);
		diff_eq_int("but its scale is zero", t->answer_tone->cfg.scale,
			    0, 0);
		saw_tone_silent++;
		DeleteV23Modem(t);
		ref_DeleteV23Modem(tref);
	}
	rc |= diff_end();

	diff_begin("CreateV23Modem: a caller-supplied object is not built");
	{
		/*
		 * D22.  The original does all of its construction inside the
		 * `state == NULL` branch, so this fills in the timing fields
		 * and leaves `mode`, `tx` and `rx` exactly as it found them.
		 * Compared byte for byte -- including the fields it does NOT
		 * write, which both sides must leave alone identically -- and
		 * not driven, because the pointers are not pointers.
		 */
		static struct v23modem sa, sb;
		struct v23_cfg cfg;
		static const unsigned skip[] = { 0 };

		memset(&cfg, 0, sizeof(cfg));
		cfg.sample_rate = 8000;
		cfg.silence_limit = 100000;
		memset(&sa, 0x3c, sizeof(sa));
		memset(&sb, 0x3c, sizeof(sb));

		diff_eq_int("it returns what it was given",
			    CreateV23Modem(&sa, 1, &cfg) == &sa, 1, 0);
		ref_CreateV23Modem(&sb, 1, &cfg);
		compare_bytes("supplied object byte %ld", &sa, &sb, sizeof(sa),
			      skip, 0);
		diff_eq_int("mode was left alone", sa.mode, 0x3c3c, 0);
	}
	rc |= diff_end();

	diff_begin("V23ModemMain: the answer-tone sequence");
	{
		/*
		 * A sample rate of 800 makes the sequence 2400 samples of tone
		 * and 80 of silence, so 40-sample blocks walk all three states
		 * in sixty-odd calls instead of three seconds of them.  The
		 * durations are the only thing sample_rate feeds, so shrinking
		 * it changes the length of the test and nothing else.
		 */
		build("tone seq: byte %ld", 1, 1, 800, &a, &b);
		for (i = 0; i < 80; i++)
			step(a, b, quiet + i * 40, 40, want);
		DeleteV23Modem(a);
		ref_DeleteV23Modem(b);
	}
	rc |= diff_end();

	diff_begin("V23ModemMain: the terminal end, in data");
	{
		build("terminal: byte %ld", 0, 0, 8000, &a, &b);
		for (i = 0; i + 160 <= NSAMP; i += 160)
			if (step(a, b, fw_signal + i, 160, want) == 2)
				break;
		DeleteV23Modem(a);
		ref_DeleteV23Modem(b);
	}
	rc |= diff_end();

	diff_begin("V23ModemMain: the host end, in data");
	{
		build("host: byte %ld", 1, 0, 8000, &a, &b);
		for (i = 0; i + 160 <= NSAMP; i += 160)
			if (step(a, b, bw_signal + i, 160, want) == 2)
				break;
		DeleteV23Modem(a);
		ref_DeleteV23Modem(b);
	}
	rc |= diff_end();

	diff_begin("V23ModemMain: coverage");
	diff_eq_int("it played the answer tone", saw_tone_state > 0, 1, 0);
	diff_eq_int("then the silence after it", saw_silence_state > 0, 1, 0);
	diff_eq_int("then data", saw_data_state > 0, 1, 0);
	diff_eq_int("the tone is audible at the host", saw_tone_audible > 0, 1,
		    0);
	diff_eq_int("and silent at the terminal", saw_tone_silent > 0, 1, 0);
	diff_eq_int("data was replaced by mark before carrier", saw_marked > 0,
		    1, 0);
	diff_eq_int("and passed through after it", saw_passed > 0, 1, 0);
	diff_eq_int("a receiver reported carrier", saw_rx_carrier > 0, 1, 0);
	diff_eq_int("and produced bits", saw_rx_bits > 0, 1, 0);
	printf("  tone %d, silence %d, data %d, marked %d, passed %d, "
	       "carrier %d, bits %d\n",
	       saw_tone_state, saw_silence_state, saw_data_state, saw_marked,
	       saw_passed, saw_rx_carrier, saw_rx_bits);
	rc |= diff_end();

	return rc;
}
