/*
 * v23rx.c -- ITU-T V.23: the 1200 bps forward-channel receiver.
 *
 * Reconstructed from dsplibs.o v23rx.c:
 *   v23FP_rx_create    .text 0x086d30   581 bytes
 *   v23FP_rx_delete    .text 0x086f80    89 bytes
 *   v23FP_rx_progress  .text 0x086fe0   664 bytes
 *
 * The chain is the ordinary one and the rates are the interesting part:
 *
 *     8 kHz in
 *       -> FPM_iir_filt_II, V23_IIR_FILT, 4 sections   channel filter
 *       -> FPM_MRF_filter, 3:4                         6 kHz
 *       -> FPM_AGC_agc                                 level
 *       -> FPM_FSD_demodulate, 5 samples per bit       1200 bps
 *
 * 6000 / 5 is 1200 exactly.  Bell 103 had to resample to 7200 to get a whole
 * number of samples per symbol and still only landed on 24 for 300 baud;
 * V.23's 3:4 lands on an integer with nothing left over, which is why this
 * file has no equivalent of finding F24's arithmetic.
 *
 * ---------------------------------------------------------------------------
 * Two gain controls, and why they cannot be shared
 *
 * `det_agc` runs on a private copy of the raw 8 kHz block and exists only to
 * feed the carrier detector.  `agc` runs on the demodulator's own signal after
 * the channel filter and the resampler, and is FROZEN the moment carrier is
 * declared, so the demodulator sees a constant gain from then on and the
 * slicer's threshold means something stable.
 *
 * They are initialised from the same AGCv23_CFG and then `det_agc` is
 * overridden -- a higher reference and a longer measurement block.  Detecting
 * a tone wants a settled estimate over a long window; demodulating wants a
 * short one that reacts.  The two roles genuinely want different filters, and
 * the original writes the difference in as two stores after the init rather
 * than carrying a second config struct.
 *
 * ---------------------------------------------------------------------------
 * The acquisition gate accumulates; it does not require consecutive blocks
 *
 * `rx_state` starts at 0 and gains 5 for every block in which the detector
 * reports 1300 Hz.  At 10 the gate opens.  A failed detection leaves the
 * counter ALONE -- which is worth stating because Bell 103's DemodDataB103,
 * which this file otherwise mirrors line for line, resets it to zero.  So
 * V.23 accepts two detections however far apart, and Bell 103 demands them
 * back to back.  Reproduced as written; the original's own debug string
 * ("v23 tone detected, counter = %d,threshold = 2") confirms the count is of
 * detections rather than of consecutive ones.
 *
 * ---------------------------------------------------------------------------
 * Two timeouts, both in milliseconds
 *
 * Both counters are charged 20 per call, which is a 160-sample block at
 * 8 kHz -- so the units are milliseconds and the limits read as times.
 * `acquire_limit` is 60000: one minute to find carrier, after which the
 * original prints "Carrier Detection Time Out ".  `silence_limit` comes from
 * the configuration and is the carrier-loss timeout, whose message is "Energy
 * drop detected......".
 *
 * `acquire_limit` is 60000 stored in a SIGNED short, where it reads as -5536.
 * Both counters are compared unsigned, so it works; narrowing either
 * comparison to signed -- which the field's declared type invites -- would
 * make the timeout fire on the first call.  That is why both counters and
 * both limits are unsigned short here.
 */

#include "dsplib/fpm_iir.h"
#include "dsplib/sysdep.h"
#include "dsplib/v23fp.h"

/*
 * The smoother coefficients, by pointer as FPM_AGC always takes them.  Only
 * element 0 is ever selected; see b103_agc_cfg.c.
 *
 * bwchdem.c has statics of these two names AND of AGCv23_CFG at different
 * addresses.  The coefficients happen to be identical, the config does not.
 * Kept separate because the original keeps them separate, and merging two
 * tables that are only accidentally equal is how a later edit to one of them
 * silently changes the other.
 */
static const short V23_AGC_DEF_ALPHA[2] = { 16384, 32604 };
static const short V23_AGC_DEF_BETA[2] = { 16384, 1638 };

/*
 * Gain control for the data path.
 *
 * A reference of 6553 is 0.2 of full scale, so the loop settles at an output
 * RMS of 3276 -- a tenth of full scale, four times quieter than Bell 103's.
 * The discriminator that follows squares its input, so there is a real reason
 * to leave headroom here that a linear stage would not need.
 */
static const struct fpm_agc_cfg AGCv23_CFG = {
	.ref_level = 6553,
	.acquire_level = 32,
	.squelch_level = 32,
	.f06 = 1000,
	.f08 = 0,
	.block_len = 30,
	.alpha = V23_AGC_DEF_ALPHA,
	.beta = V23_AGC_DEF_BETA,
	.f14 = 0
};

/*
 * The two fields the detector's copy overrides after FPM_AGC_init.  40 samples
 * is 5 ms at 8 kHz; 30 samples at the resampler's 6 kHz output is also 5 ms,
 * so both gain controls measure over the same amount of TIME and differ only
 * in what they normalise to.
 */
#define V23RX_DET_REF_LEVEL	16384
#define V23RX_DET_BLOCK		40

/*
 * The tone the receiver waits for, and the two fields it overrides in the
 * shared FPM_TONE configuration.
 *
 * 1300 Hz is V.23's forward-channel mark, which is what an idle far end
 * transmits -- so the carrier detector and the mark detector are the same
 * thing.  The base configuration's `freq` of 1650 is the midpoint of the
 * 1300/2100 pair and is what v23tx.c leaves in place; here it is replaced,
 * because a detector has to be tuned and a generator does not (v23tx.c
 * rewrites its frequency per sample anyway).
 *
 * The ratio is raised from the shared 0.75 to 0.885: nearly nine tenths of the
 * energy must be at 1300 Hz before this counts as carrier.  That is a strict
 * test, and it is what stops modulated data -- which spends half its time at
 * 2100 Hz -- from being mistaken for an idle mark.
 */
#define V23RX_TONE_HZ		1300
#define V23RX_TONE_RATIO	28998	/* 0.885 in Q15, up from 0.75 */

/*
 * The transmit-side fields are inherited unchanged and unused: this object
 * never generates.  `scale` in particular stays at the shared 3200 rather than
 * v23tx.c's 0xee4, which is harmless and worth noting only because the two
 * files' copies of this struct are byte-identical in the blob and it would be
 * easy to conclude they are the same object.  They are not -- .rodata:0x8240
 * and .rodata:0x82a0, two file-static copies of the same 36 bytes.
 */
static const struct fpm_tone_cfg TONEv23_CFG = {
	.freq = 1650,
	.scale = 3200,
	.rev_period = 0,
	.ratio = 24576,		/* 0.75 in Q15 */
	.f08 = 327,
	.min_level = 3,
	.damp = 30720,		/* 0.9375 in Q15 */
	.len = 53,
	.rev_thresh = 16384
	/* src is filled in by create; everything else is zero */
};

/*
 * The demodulator.  Everything that describes a filter is zero here and
 * patched in by create; what is left is the bit plan.
 *
 *   high_bit    1, so a positive discriminator output is a mark
 *   bit_samples 5, at the resampler's 6 kHz -> 1200 bps
 *   max_bits    24, i.e. 120 samples of 6 kHz input, i.e. 160 of 8 kHz
 *   trace_len   160
 *
 * slice_level is 0, where Bell 103 uses 10: no hysteresis band at all, just a
 * sign test that holds its previous value on an exact zero.  At 1200 bps the
 * two tones are 800 Hz apart and the discriminator output is not marginal, so
 * the dead zone Bell 103 needs at 200 Hz separation buys nothing here.
 */
static const struct fpm_fsd_cfg FSDv23_CFG = {
	.fir = NULL,		/* create supplies _V23RX_ANSWER_INTRP */
	.fir_taps = 0,
	.delay = 0,
	.iir = NULL,		/* create supplies _V23RX_IIR_LPF      */
	.iir_len = 0,
	.slice_level = 0,
	.high_bit = 1,
	.bit_samples = 5,
	.max_bits = 24,
	.trace_len = 160,
	.f18 = 0
};

/* What create patches into the copy of FSDv23_CFG it actually passes. */
#define V23RX_FSD_FIR_TAPS	15
#define V23RX_FSD_DELAY		6
#define V23RX_FSD_IIR_LEN	3

/*
 * The resampler: 3 up, 4 down, 8 kHz -> 6 kHz.  48 taps across 3 phases is 16
 * per phase.
 */
static const struct fpm_mrf_cfg MRFv23_CFG = {
	.branches = 3,
	.decimate = 4,
	.coeff = NULL,		/* create supplies _V23_MRF_FILT */
	.taps = 48,
	.aux = NULL
};

/* The channel filter ahead of the resampler: 4 biquads, direct form I. */
#define V23RX_IIR_SECTIONS	4

/* One 8 kHz block is 20 ms, and both timeouts are counted in milliseconds. */
#define V23RX_TICK_MS		20

/* One minute to find carrier.  Does not fit in a signed short; see the top. */
#define V23RX_ACQUIRE_LIMIT_MS	60000

/* The acquisition gate: 5 per detection, open above 9, running at 11. */
#define V23RX_DETECT_STEP	5
#define V23RX_DETECT_GATE	9
#define V23RX_DETECT_DONE	10
#define V23RX_RUNNING		11

/* What progress returns. */
#define V23RX_DEMODULATED	0
#define V23RX_WAITING		1
#define V23RX_GIVEN_UP		2

struct v23rx *
v23FP_rx_create(struct v23rx *rx, const struct v23_cfg *cfg)
{
	struct fpm_mrf_cfg mrf;
	struct fpm_fsd_cfg fsd;
	struct fpm_tone_cfg tone;
	short i;

	if (rx == NULL)
		rx = sysdep_malloc(sizeof(*rx));

	mrf = MRFv23_CFG;
	mrf.coeff = _V23_MRF_FILT;
	FPM_MRF_init(&rx->mrf, &mrf, 1);

	fsd = FSDv23_CFG;
	fsd.fir = _V23RX_ANSWER_INTRP;
	fsd.fir_taps = V23RX_FSD_FIR_TAPS;
	fsd.delay = V23RX_FSD_DELAY;
	fsd.iir = _V23RX_IIR_LPF;
	fsd.iir_len = V23RX_FSD_IIR_LEN;
	FPM_FSD_init(&rx->fsd, &fsd, 1);

	tone = TONEv23_CFG;
	tone.freq = V23RX_TONE_HZ;
	tone.ratio = V23RX_TONE_RATIO;
	tone.src = FPM_TONE_CFG.src;	/* the shared 53-tap prototype */
	rx->tone = FPM_TONE_create(NULL, &tone);

	rx->rx_state = 0;
	rx->iir_coeff = V23_IIR_FILT;
	rx->iir_sections = V23RX_IIR_SECTIONS;

	/*
	 * Direct form I, so four words per section.  Zeroed explicitly rather
	 * than by a calloc: sysdep_malloc does not clear, and the differential
	 * harness fills what it hands out precisely so a missing initialiser
	 * shows up as a mismatch rather than as a zero that happened to be
	 * there.
	 */
	rx->iir_state = sysdep_malloc(4 * V23RX_IIR_SECTIONS * sizeof(short));
	for (i = 0; i <= 4 * V23RX_IIR_SECTIONS - 1; i++)
		rx->iir_state[i] = 0;

	FPM_AGC_init(&rx->agc, &AGCv23_CFG, 1);

	FPM_AGC_init(&rx->det_agc, &AGCv23_CFG, 1);
	rx->det_agc.cfg.block_len = V23RX_DET_BLOCK;
	rx->det_agc.cfg.ref_level = V23RX_DET_REF_LEVEL;

	rx->silence = 0;
	/*
	 * Truncated to 16 bits, where BwChDem_Create keeps all 32 of the same
	 * field.  The two receivers of one modem disagreeing about the width of one
	 * configuration value; see docs/deviations.md.
	 */
	rx->silence_limit = (unsigned short)cfg->silence_limit;
	rx->acquire = 0;
	rx->acquire_limit = V23RX_ACQUIRE_LIMIT_MS;
	rx->status = V23RX_WAITING;

	/*
	 * The original prints "V23FP Rx Created, version 10-December-02." at
	 * debug level 2.  Dropped, as everywhere else in this tree.
	 */
	return rx;
}

void
v23FP_rx_delete(struct v23rx *rx)
{
	if (rx == NULL)
		return;
	FPM_TONE_delete(rx->tone);
	/*
	 * Both frees are called with a second argument they do not have; see
	 * the same note in b103fp.c.  Neither reads it.
	 */
	FPM_MRF_free(&rx->mrf);
	FPM_FSD_free(&rx->fsd);
	sysdep_free(rx->iir_state);
	sysdep_free(rx);
}

/*
 * Demodulate one block.
 *
 * `samples` is worked on IN PLACE and shrinks: the channel filter runs over
 * `count` samples, the resampler writes 3/4 as many back over the front of the
 * same buffer, and everything after it works on those.
 *
 * `count` is an int and is compared as one in the copy below, unlike
 * BwChDem_Progress's short.  Both are as the original has them.
 */
short
v23FP_rx_progress(struct v23rx *rx, short *samples, int count, int *bits,
		  int *nbits)
{
	int signal;
	short nout;
	short i;

	if (rx->rx_state <= V23RX_DETECT_GATE) {
		/*
		 * Charged whether or not the tone is there, and stopped
		 * entirely once the gate opens -- so this is a deadline on
		 * finishing acquisition, not a measure of how quiet the line
		 * has been.  The counter is advanced before the test, which
		 * is why an exactly-equal limit still fires.
		 */
		rx->acquire = (unsigned short)(rx->acquire + V23RX_TICK_MS);
		if (rx->acquire_limit <= rx->acquire)
			return V23RX_GIVEN_UP;

		/*
		 * The detector gets its own copy, because everything below
		 * overwrites `samples`.  Nothing bounds this against the 160
		 * shorts `det_buf` has -- the original has no guard either,
		 * and the caller that keeps it honest is V23ModemMain, which
		 * asks for one 8 kHz frame at a time.  Same class of
		 * un-guarded buffer as b103fp.c's `short lo[160]`.
		 */
		for (i = 0; i < count; i++)
			rx->det_buf[i] = samples[i];

		FPM_AGC_agc(&rx->det_agc, rx->det_buf, (unsigned short)count);

		if (FPM_TONE_detect(rx->tone, rx->det_buf, (short)count)
		    == FPM_TONE_PRESENT) {
			/*
			 * NOT reset on a miss; see the header comment.  The
			 * original's debug line divides this by 5 to print a
			 * count against a threshold of 2.
			 */
			rx->rx_state = (short)(rx->rx_state
					       + V23RX_DETECT_STEP);
		}
	}

	FPM_iir_filt_II(samples, rx->iir_coeff, rx->iir_state,
			rx->iir_sections, (short)count);

	nout = FPM_MRF_filter(&rx->mrf, samples, samples, (short)count);

	/*
	 * The original uses the value FPM_AGC_agc leaves in %eax, which is the
	 * `signal` flag the same instruction stored into the object.  Read the
	 * field: it is the same number and does not rest on a return value the
	 * function never promised.  Same as bwchdem.c and b103fp.c.
	 */
	FPM_AGC_agc(&rx->agc, samples, (unsigned short)nout);
	signal = rx->agc.signal;

	/*
	 * Exactly one block sees state 10, so the gain is frozen once and the
	 * demodulator runs at a fixed gain from here on.
	 */
	if (rx->rx_state == V23RX_DETECT_DONE)
		FPM_AGC_Freeze(&rx->agc);

	if (rx->rx_state <= V23RX_DETECT_GATE) {
		/*
		 * Still acquiring.  Note the chain above has already run: the
		 * filters and the resampler are kept warm from the first
		 * block, and only the demodulator is skipped.  `bits` and
		 * `nbits` are left exactly as the caller passed them.
		 */
		rx->status = V23RX_WAITING;
		return rx->status;
	}

	rx->rx_state = V23RX_RUNNING;

	if (signal) {
		rx->silence = 0;
	} else {
		rx->silence = (unsigned short)(rx->silence + V23RX_TICK_MS);
		if (rx->silence_limit <= rx->silence)
			return V23RX_GIVEN_UP;
	}

	rx->nbits = FPM_FSD_demodulate(&rx->fsd, samples, rx->bits,
				       (unsigned short)nout);

	if (rx->silence != 0) {
		/*
		 * The line has gone quiet but not yet for long enough to give
		 * up.  The bits were demodulated and are DISCARDED: the count
		 * is reported as zero and the caller's buffer is not written.
		 * Anything the demodulator recovered from a fading signal is
		 * thrown away rather than passed up as data, which is the
		 * conservative choice and costs a bit period or two of real
		 * data at the start of every recovery.
		 */
		rx->status = V23RX_DEMODULATED;
		*nbits = 0;
		return rx->status;
	}

	*nbits = rx->nbits;
	for (i = 0; i < rx->nbits; i++)
		bits[i] = rx->bits[i];

	rx->status = V23RX_DEMODULATED;
	return rx->status;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.
 *
 * Every offset was read off create's stores and progress's loads.  The two
 * buffer offsets matter most: `det_buf` and `bits` are sized by the gap to
 * whatever follows them, not by any instruction, so if a field ahead of them
 * is ever resized these are what catches it.
 *
 * Guarded to a 32-bit ABI: four of the fields are pointers, and three of the
 * embedded structs contain more.  See the same note in b103fp.c.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V23RX_ASSERT_OFF(field, off) \
	typedef char v23rx_off_##field[ \
		((int)__builtin_offsetof(struct v23rx, field) == (off)) ? 1 : -1]

V23RX_ASSERT_OFF(rx_state, 0x000);
V23RX_ASSERT_OFF(tone, 0x004);
V23RX_ASSERT_OFF(agc, 0x008);
V23RX_ASSERT_OFF(det_agc, 0x034);
V23RX_ASSERT_OFF(mrf, 0x060);
V23RX_ASSERT_OFF(fsd, 0x07c);
V23RX_ASSERT_OFF(iir_coeff, 0x0b4);
V23RX_ASSERT_OFF(iir_state, 0x0b8);
V23RX_ASSERT_OFF(iir_sections, 0x0bc);
V23RX_ASSERT_OFF(det_buf, 0x0c0);
V23RX_ASSERT_OFF(bits, 0x200);
V23RX_ASSERT_OFF(nbits, 0x234);
V23RX_ASSERT_OFF(silence, 0x236);
V23RX_ASSERT_OFF(silence_limit, 0x238);
V23RX_ASSERT_OFF(acquire, 0x23a);
V23RX_ASSERT_OFF(acquire_limit, 0x23c);
V23RX_ASSERT_OFF(status, 0x23e);

/* 0x240 is the size sysdep_malloc is asked for when `state` is NULL. */
typedef char v23rx_size[(sizeof(struct v23rx) == 0x240) ? 1 : -1];

#endif
