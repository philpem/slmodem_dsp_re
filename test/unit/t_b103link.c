/*
 * t_b103link.c -- Bell 103 end to end: does the reconstruction carry data?
 *
 * Every other test in this tree asks "does the reconstruction agree with the
 * blob".  This one asks the question the project actually exists to answer:
 * feed a known bit stream into an originating transmitter, take it out of an
 * answering receiver, and count the errors.
 *
 * The two are a genuine pair, not a loopback.  An originating station
 * transmits 1070/1270 Hz and an answering one receives it, so this is the
 * real Bell 103 direction and the real tone pair.  The answering receiver
 * mixes with a 395 Hz local oscillator, bringing 1070/1270 down to 675/875 Hz
 * -- either side of the demodulator's 775 Hz discriminator null.  That is why
 * it works, and it is worth stating because feeding a station its own
 * transmitter instead measures BER 0.485 (finding 32).
 *
 * Three combinations are run, and all three must be error-free:
 *
 *     blob      -> blob          the reference link, to prove the setup
 *     ours      -> ours          the reconstruction standing alone
 *     ours <-> blob (both ways)  interoperation
 *
 * The last is the interesting one.  Bit-exactness already implies it, so it
 * is not new evidence -- but it is the form the claim has to take to be worth
 * anything to someone deciding whether to run this against real hardware.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/b103fp.h"

extern void *ref_B103FP_create(void *state, const void *cfg);
extern void ref_B103FP_delete(void *state);
extern short ref_B103_CFG[];
extern short ref_ModDataB103(void *fp, const unsigned short *bits, short *out,
			     unsigned short n);
extern short ref_DemodDataB103(void *fp, short *in, unsigned short *bits,
			       unsigned short n);
extern void (*ref_B103NextState[3])(void *fp);

#define NBITS 4000

static unsigned char sent[NBITS];
static unsigned char got[NBITS];

static struct b103fp *
make(int call_type)
{
	unsigned char cfg[28];

	memcpy(cfg, ref_B103_CFG, sizeof(cfg));
	*(int *)cfg = call_type;
	return ref_B103FP_create(0, cfg);
}

/*
 * Put a receiver straight into its data state.  Call setup is exercised by
 * t_b103hdx; here the question is whether bits survive the channel, so the
 * handshake is skipped rather than simulated.
 */
static void
force_data_state(struct b103fp *fp)
{
	fp->hdx->substate = B103_STATE_WAIT1;
	ref_B103NextState[fp->hdx->mode](fp);
	fp->dsp->rx_state = 16;
}

/*
 * Run `bits` bits through tx -> rx and return the bit error rate, ignoring a
 * lead-in and allowing for the pipeline delay.  `use_ours_tx`/`use_ours_rx`
 * select the reconstruction or the blob at each end.
 */
static double
run_link(int use_ours_tx, int use_ours_rx, int *nsent, int *ngot, int *lag_out)
{
	struct b103fp *tx = make(B103_CALL_ORIGINATE);
	struct b103fp *rx = make(B103_CALL_ANSWER);
	short air[512];
	unsigned short tbits[8], rbits[64];
	unsigned lfsr = 0xACE1u;
	int ns = 0, ng = 0, f, i, n8, nb;
	int best_err = NBITS, best_lag = 0;
	int lag;

	if (tx == 0 || rx == 0)
		return 1.0;
	force_data_state(rx);

	for (f = 0; ns + 6 < NBITS && ng + 8 < NBITS; f++) {
		for (i = 0; i < 6; i++) {
			lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
			tbits[i] = (unsigned short)(lfsr & 1);
			sent[ns++] = (unsigned char)(lfsr & 1);
		}

		n8 = use_ours_tx ? ModDataB103(tx, tbits, air, 6)
				 : ref_ModDataB103(tx, tbits, air, 6);
		nb = use_ours_rx
			? DemodDataB103(rx, air, rbits, (unsigned short)n8)
			: ref_DemodDataB103(rx, air, rbits, (unsigned short)n8);

		for (i = 0; i < nb && ng < NBITS; i++)
			got[ng++] = (unsigned char)rbits[i];
	}

	/* Find the pipeline delay, then count errors at it. */
	for (lag = -60; lag <= 60; lag++) {
		int err = 0, n = 0;

		for (i = 300; i < ns - 100 && i < ng - 100; i++) {
			int j = i + lag;

			if (j < 0 || j >= ng)
				continue;
			err += (sent[i] != got[j]);
			n++;
		}
		if (n > 500 && err < best_err) {
			best_err = err;
			best_lag = lag;
		}
	}

	ref_B103FP_delete(tx);
	ref_B103FP_delete(rx);

	*nsent = ns;
	*ngot = ng;
	*lag_out = best_lag;
	return (double)best_err / (double)(ns - 400);
}

int
main(void)
{
	static const struct {
		const char *name;
		int ours_tx, ours_rx;
	} links[] = {
		{ "blob -> blob", 0, 0 },
		{ "ours -> ours", 1, 1 },
		{ "ours -> blob", 1, 0 },
		{ "blob -> ours", 0, 1 }
	};
	int rc = 0;
	unsigned k;

	/*
	 * Confirm the two ends really are a pair before measuring anything.
	 * If the oscillators came out wrong the BER would be ~0.5 and it
	 * would not be obvious why.
	 */
	/*
	 * The reconstructed config must be the blob's, byte for byte, and the
	 * field meanings documented in b103fp.h must actually hold.  Prose in
	 * a header is worth what the build enforces, so each documented effect
	 * is asserted against a freshly built object here.
	 */
	diff_begin("B103_CFG reconstruction");
	{
		diff_eq_int("config is 28 bytes (%ld)",
			    (long)sizeof(struct b103_cfg), 28, 0);
		diff_eq_int("matches the blob byte for byte (%ld)",
			    memcmp(&B103_CFG_data, ref_B103_CFG,
				   sizeof(struct b103_cfg)), 0, 0);
		diff_eq_int("the built-in config is LOOPBACK (%ld)",
			    B103_CFG_data.call_type, B103_CALL_LOOPBACK, 0);
	}
	rc |= diff_end();

	diff_begin("B103_CFG documented field effects");
	{
		struct b103_cfg cfg;
		struct b103fp *fp;

		/* call_type: the documented table, one row at a time. */
		cfg = B103_CFG_data;
		cfg.call_type = B103_CALL_ORIGINATE;
		fp = ref_B103FP_create(0, &cfg);
		if (fp) {
			diff_eq_int("originate -> mode 1 (%ld)",
				    fp->hdx->mode, 1, 0);
			diff_eq_int("originate -> 40-tap bandpass (%ld)",
				    fp->dsp->bpf_taps, 40, 0);
			diff_eq_int("originate -> tone detector (%ld)",
				    fp->hdx->tone_detect != 0, 1, 0);
			ref_B103FP_delete(fp);
		}
		cfg.call_type = B103_CALL_ANSWER;
		fp = ref_B103FP_create(0, &cfg);
		if (fp) {
			diff_eq_int("answer -> mode 2 (%ld)",
				    fp->hdx->mode, 2, 0);
			diff_eq_int("answer -> 50-tap bandpass (%ld)",
				    fp->dsp->bpf_taps, 50, 0);
			ref_B103FP_delete(fp);
		}
		/* Anything else is loopback -- no range check in the original. */
		cfg.call_type = 99;
		fp = ref_B103FP_create(0, &cfg);
		if (fp) {
			diff_eq_int("out-of-range call_type -> loopback (%ld)",
				    fp->hdx->mode, 0, 0);
			diff_eq_int("loopback -> no bandpass (%ld)",
				    fp->dsp->bpf_taps, 0, 0);
			diff_eq_int("loopback -> no tone detector (%ld)",
				    fp->hdx->tone_detect == 0, 1, 0);
			ref_B103FP_delete(fp);
		}

		/* loop_high_channel: loopback only. */
		cfg = B103_CFG_data;
		cfg.loop_high_channel = 1;
		fp = ref_B103FP_create(0, &cfg);
		if (fp) {
			diff_eq_int("loopback high channel -> 2025 (%ld)",
				    fp->dsp->fsm.cfg.freq[0], 2025, 0);
			ref_B103FP_delete(fp);
		}
		cfg.call_type = B103_CALL_ORIGINATE;
		fp = ref_B103FP_create(0, &cfg);
		if (fp) {
			diff_eq_int("originate ignores it -> 1070 (%ld)",
				    fp->dsp->fsm.cfg.freq[0], 1070, 0);
			ref_B103FP_delete(fp);
		}

		/* tone_timeout_ticks: /20, floored at 700. */
		cfg = B103_CFG_data;
		cfg.tone_timeout_ticks = 28000;
		fp = ref_B103FP_create(0, &cfg);
		if (fp) {
			diff_eq_int("28000/20 -> 1400 blocks (%ld)",
				    fp->hdx->tone_timeout, 1400, 0);
			ref_B103FP_delete(fp);
		}
		cfg.tone_timeout_ticks = 100;
		fp = ref_B103FP_create(0, &cfg);
		if (fp) {
			diff_eq_int("below the floor -> 700 blocks (%ld)",
				    fp->hdx->tone_timeout, 700, 0);
			ref_B103FP_delete(fp);
		}

		/* tx_scale: straight into the modulator. */
		cfg = B103_CFG_data;
		cfg.tx_scale = 1234;
		fp = ref_B103FP_create(0, &cfg);
		if (fp) {
			diff_eq_int("tx_scale reaches fsm.scale (%ld)",
				    fp->dsp->fsm.cfg.scale, 1234, 0);
			ref_B103FP_delete(fp);
		}
	}
	rc |= diff_end();


	/*
	 * The tone plan table from b103fp.h, driven.  Every row of it -- and
	 * in particular the last, where the receive side is configured for
	 * V.21 channel 1 while the transmitter stays on Bell 103's tones.
	 * That asymmetry is measured, not inferred, and asserting it here
	 * means a future decode of B103FP_create that "fixes" it fails rather
	 * than silently diverging from the blob.
	 */
	diff_begin("B103 tone plans");
	{
		static const struct {
			int call_type, v21;
			int tx0, tx1, detect_hz, lo_hz, lands_at;
			const char *plan;
		} plans[] = {
			{ 0, 0, 1070, 1270, 2225, 1350, 875, "Bell 103 originate" },
			{ 0, 1, 1180,  980, 1650,  975, 675, "V.21 channel 1" },
			{ 1, 0, 2025, 2225, 1270,  395, 875, "Bell 103 answer" },
			{ 1, 1, 2025, 2225, 1180,  305, 875, "V.21 answer, mixed" }
		};
		unsigned p;

		for (p = 0; p < sizeof(plans) / sizeof(plans[0]); p++) {
			struct b103_cfg cfg = B103_CFG_data;
			struct b103fp *fp;
			char buf[96];
			int det, lo;

			cfg.call_type = plans[p].call_type;
			cfg.v21 = plans[p].v21;
			fp = ref_B103FP_create(0, &cfg);
			if (fp == 0)
				continue;

			snprintf(buf, sizeof(buf), "%s: transmits mark (%%ld)",
				 plans[p].plan);
			diff_eq_int(buf, fp->dsp->fsm.cfg.freq[0], plans[p].tx0, (long)p);
			snprintf(buf, sizeof(buf), "%s: transmits space (%%ld)",
				 plans[p].plan);
			diff_eq_int(buf, fp->dsp->fsm.cfg.freq[1], plans[p].tx1, (long)p);

			det = (*(short *)((char *)fp->hdx->tone_detect + 0x26)
			       * 8000 + 16384) / 32768;
			lo = (*(short *)((char *)fp->hdx->tone_lo + 0x26)
			      * 8000 + 16384) / 32768;
			snprintf(buf, sizeof(buf), "%s: detector (%%ld)", plans[p].plan);
			diff_eq_int(buf, det, plans[p].detect_hz, (long)p);
			snprintf(buf, sizeof(buf), "%s: oscillator (%%ld)", plans[p].plan);
			diff_eq_int(buf, lo, plans[p].lo_hz, (long)p);

			/*
			 * The invariant behind all of it: whatever the plan,
			 * the detector's tone mixes down onto 675 or 875 Hz --
			 * the pair the one demodulator design is built for.
			 *
			 * WHICH of the two took two corrections to get right,
			 * and both were the test catching the assumption:
			 *
			 *  - asserting 875 everywhere failed on V.21 channel 1.
			 *    Bell 103's mark is the HIGHER tone of its pair
			 *    (1270, 2225); V.21's is the LOWER (980, 1650), so
			 *    the detector sits on mark at 875 for one and 675
			 *    for the other.
			 *  - asserting "675 whenever v21" then failed on the
			 *    last row, which lands at 875 because its detector
			 *    is on channel 1's SPACE.  That row is the one
			 *    whose transmitter is also mis-set (see above), so
			 *    it is tabulated rather than reasoned about.
			 *
			 * Hence `lands_at` is a column, not a rule.
			 */
			snprintf(buf, sizeof(buf),
				 "%s: detector mixes to %d Hz (%%ld)",
				 plans[p].plan, plans[p].lands_at);
			diff_eq_int(buf, det - lo, plans[p].lands_at, (long)p);

			ref_B103FP_delete(fp);
		}
	}
	rc |= diff_end();

	diff_begin("B103 link setup");
	{
		struct b103fp *o = make(B103_CALL_ORIGINATE);
		struct b103fp *a = make(B103_CALL_ANSWER);

		if (o && a) {
			diff_eq_int("originate transmits 1070 (%ld)",
				    o->dsp->fsm.cfg.freq[0], 1070, 0);
			diff_eq_int("originate transmits 1270 (%ld)",
				    o->dsp->fsm.cfg.freq[1], 1270, 0);
			diff_eq_int("answer transmits 2025 (%ld)",
				    a->dsp->fsm.cfg.freq[0], 2025, 0);
			diff_eq_int("answer transmits 2225 (%ld)",
				    a->dsp->fsm.cfg.freq[1], 2225, 0);
			/*
			 * Both oscillators bring the pair they receive down to
			 * 675/875 Hz, straddling the 775 Hz null.  1350 - 1070
			 * = 280 and 1350 - 1270 = 80 would NOT work, which is
			 * the check: it is 2025/2225 the originator receives.
			 */
			diff_eq_int("originate LO is 1350 Hz (%ld)",
				    (*(short *)((char *)o->hdx->tone_lo + 0x26)
				     * 8000 + 16384) / 32768, 1350, 0);
			diff_eq_int("answer LO is 395 Hz (%ld)",
				    (*(short *)((char *)a->hdx->tone_lo + 0x26)
				     * 8000 + 16384) / 32768, 395, 0);
			diff_eq_int("originate installs a 40-tap bandpass (%ld)",
				    o->dsp->bpf_taps, 40, 0);
			diff_eq_int("answer installs a 50-tap bandpass (%ld)",
				    a->dsp->bpf_taps, 50, 0);
			diff_eq_int("both have a tone detector (%ld)",
				    o->hdx->tone_detect != 0
				    && a->hdx->tone_detect != 0, 1, 0);
			ref_B103FP_delete(o);
			ref_B103FP_delete(a);
		} else {
			diff_eq_int("objects built (%ld)", 0, 1, 0);
		}
	}
	rc |= diff_end();

	diff_begin("B103 bit error rate");
	for (k = 0; k < sizeof(links) / sizeof(links[0]); k++) {
		int ns = 0, ng = 0, lag = 0;
		double ber = run_link(links[k].ours_tx, links[k].ours_rx,
				      &ns, &ng, &lag);

		printf("  %-14s %d bits sent, %d received, lag %d, BER %.5f\n",
		       links[k].name, ns, ng, lag, ber);

		/*
		 * Zero, not "low".  This is a noiseless channel with no
		 * impairment at all, so a single error would mean a real
		 * defect rather than bad luck.
		 */
		diff_eq_int("%s: bit errors", (long)(ber * (ns - 400) + 0.5),
			    0, (long)k);
		diff_eq_int("%s: bits recovered", ng > 3000, 1, (long)k);
	}
	rc |= diff_end();

	return rc;
}
