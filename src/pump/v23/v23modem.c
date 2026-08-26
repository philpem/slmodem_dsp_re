/*
 * v23modem.c -- ITU-T V.23: the composite modem.
 *
 * Reconstructed from dsplibs.o v23modem.c:
 *   CreateV23Modem  .text 0x086890   524 bytes
 *   DeleteV23Modem  .text 0x086aa0   103 bytes
 *   V23ModemMain    .text 0x086b10   543 bytes
 *
 * This is the layer that decides which end of a V.23 call it is.  V.23 is
 * asymmetric -- 1200 bps one way, 75 bps the other -- so the two ends do not
 * run the same code with the frequencies exchanged.  They run different code:
 *
 *              transmits                        receives
 *   mode 0     390/450 Hz, { 107, 107, 106 }    v23FP_rx_progress
 *   mode != 0  1300/2100 Hz, { 7, 7, 6 }        BwChDem_Progress
 *
 * The transmitter is the same object either way -- v23tx.c takes its
 * frequencies and its bit-period table as arguments, and this file is where
 * those two tables live -- but the receivers are two entirely separate
 * modules and only one of them is built.
 *
 * ---------------------------------------------------------------------------
 * The answer tone, and what it costs the other end
 *
 * When the configuration asks for it, the modem does not start in data.  It
 * starts in state 0 and plays a 2100 Hz tone for three seconds, then state 1
 * and a short silence, and only then state 2 and the modem proper.  That is
 * ITU-T V.25's answering sequence, which is why the tone generator is built
 * from the library's shared FPM_TONE_CFG -- already 2100 Hz, with the V.25
 * timing in it -- rather than from anything V.23-specific.
 *
 * Two things are changed in that configuration and both matter:
 *
 *   - `rev_period` is set to 0, disabling the 450 ms phase reversals.  The
 *     reversals are what tells a network echo canceller to disable itself;
 *     a 1200 bps FSK modem has no echo canceller of its own and no use for
 *     the far end's being turned off, so V.23 sends the plain tone.
 *   - `scale` is 0xee4 for the host end and ZERO for the terminal end.  So
 *     both ends run the same three-state sequence and take the same three
 *     seconds to reach data, but only one of them is audible.  The terminal
 *     end is generating silence through a live tone generator rather than
 *     skipping the state, which keeps the two ends' timing identical without
 *     either having to know what the other is doing.
 *
 * The same configuration byte is ALSO passed to v23FP_tx_create as its
 * one-shot mute, so the data transmitter emits one block of silence before it
 * starts -- see D19 for what that costs the caller's bit stream.
 *
 * ---------------------------------------------------------------------------
 * Mark is what goes out when there is nothing to say
 *
 * In data, before the transmitter runs, the caller's bit array is overwritten
 * with continuous mark whenever the receiver has no carrier.  Data handed
 * down during call setup is therefore lost silently, exactly as it is in
 * b103fp.c's B103FP_modem, and for the same reason: a modem that transmits
 * data while the far end is still training has thrown it away either way, and
 * mark is what an idle V.23 channel is supposed to carry.
 */

#include "dsplib/sysdep.h"
#include "dsplib/v23fp.h"

/*
 * The bit-period tables, one per channel.  Cyclic, and they sum to a whole
 * number of samples over three bits because neither rate is an integer number
 * of samples at 8 kHz:
 *
 *   { 7, 7, 6 }        20 samples / 3 bits = 6 2/3   -> 1200 bps
 *   { 107, 107, 106 }  320 / 3 = 106 2/3             ->   75 bps
 *
 * NOT const, because the original's are not: they sit in `.data` at 0x777c
 * and 0x7782 rather than in `.rodata`.  Nothing writes them.  Kept writable
 * so that the section a byte lands in still matches the original's, which is
 * how these two were attributed to this file in the first place.
 */
static short fw_ch_samp_per_bit_table[3] = { 7, 7, 6 };
static short bw_ch_samp_per_bit_table[3] = { 107, 107, 106 };

/* The four frequencies, as V.23 defines them. */
#define V23_FW_MARK	1300
#define V23_FW_SPACE	2100
#define V23_BW_MARK	390
#define V23_BW_SPACE	450

#define V23_PERIOD_LEN	3

/*
 * The answer tone: 2100 Hz, no phase reversals, and audible only at the host
 * end.  0xee4 is the same transmit level v23tx.c uses, so the tone and the
 * data that follows it come out at one gain.
 */
#define V23_ANSWER_TONE_HZ	2100
#define V23_ANSWER_TONE_SCALE	0xee4

/*
 * How long the sequence lasts, in terms of the configured sample rate.  At
 * 8000 that is 24000 samples of tone -- three seconds, inside V.25's 2.6 to 4
 * -- and 400 samples of silence, which is 50 ms and is NOT inside V.25's
 * 75 +/- 20.
 *
 * It arrives there anyway, by two accidents that cancel.  The state does not
 * end until `elapsed` is strictly GREATER than the limit, and `elapsed`
 * advances a whole frame at a time, so with the 160-sample frames the
 * datapump delivers the silence actually runs three frames: 480 samples,
 * 60 ms, inside the window.  Change the frame size and it stops being.
 */
#define V23_TONE_SECONDS	3
#define V23_SILENCE_DIVISOR	20

/* The three states, in the order they run. */
#define V23_STATE_TONE		0
#define V23_STATE_SILENCE	1
#define V23_STATE_DATA		2

struct v23modem *
CreateV23Modem(struct v23modem *m, int mode, const struct v23_cfg *cfg)
{
	struct fpm_tone_cfg tone;

	/*
	 * EVERYTHING that builds a modem is inside this branch, including the
	 * store of `mode`.  A caller supplying its own storage gets the timing
	 * fields below and nothing else -- see D22.  Reproduced as written.
	 */
	if (m == NULL) {
		m = sysdep_malloc(sizeof(*m));
		m->mode = (short)mode;

		if (m->mode != 0) {
			/* The host end: 1200 bps out, 75 bps in. */
			m->rx = BwChDem_Create(NULL, cfg);
			m->tx = v23FP_tx_create(NULL, V23_FW_MARK,
						V23_FW_SPACE, V23_PERIOD_LEN,
						fw_ch_samp_per_bit_table,
						cfg->answer_tone);
		} else {
			/* The terminal end: 75 bps out, 1200 bps in. */
			m->rx = v23FP_rx_create(NULL, cfg);
			m->tx = v23FP_tx_create(NULL, V23_BW_MARK,
						V23_BW_SPACE, V23_PERIOD_LEN,
						bw_ch_samp_per_bit_table,
						cfg->answer_tone);
		}
	}

	/*
	 * The original prints "V23FP version %s %s" with __TIME__ and __DATE__
	 * -- 15:48:09 on Sep 22 2005 -- at debug level 2.  Dropped, as
	 * everywhere else in this tree, but recorded because it is the only
	 * date this module carries.
	 */

	m->elapsed = 0;
	m->sample_rate = cfg->sample_rate;
	m->tone_samples = cfg->sample_rate * V23_TONE_SECONDS;
	m->silence_samples = cfg->sample_rate / V23_SILENCE_DIVISOR;

	if (cfg->answer_tone) {
		tone = FPM_TONE_CFG_data;
		tone.freq = V23_ANSWER_TONE_HZ;		/* already 2100 */
		tone.rev_period = 0;			/* no V.25 reversals */
		tone.scale = (short)(m->mode != 0 ? V23_ANSWER_TONE_SCALE : 0);
		m->answer_tone = FPM_TONE_create(NULL, &tone);
		m->state = V23_STATE_TONE;
	} else {
		m->answer_tone = NULL;
		m->state = V23_STATE_DATA;
	}
	/*
	 * Start the two in step, so the first V23ModemMain does not announce a
	 * transition that never happened.
	 */
	m->reported = m->state;

	return m;
}

void
DeleteV23Modem(struct v23modem *m)
{
	if (m == NULL)
		return;

	if (m->answer_tone != NULL)
		FPM_TONE_delete(m->answer_tone);
	v23FP_tx_delete(m->tx);
	/*
	 * TESTED ON mode == 0, NOT ON mode != 0, and the two spellings are
	 * not interchangeable in the object.  Both arms do the same work
	 * either way round; what the order decides is which one the compiler
	 * leaves as the FALLTHROUGH.  The blob falls through to
	 * BwChDem_Delete and branches away to v23FP_rx_delete, so the source
	 * tested the mode-0 case.  Written the other way (`mode != 0` first)
	 * the whole function is byte-identical except the one condition byte
	 * -- 0x74 against 0x75 -- because the two arms happen to be the same
	 * length and the displacement does not move.  Finding F8120.
	 */
	if (m->mode == 0)
		v23FP_rx_delete(m->rx);
	else
		BwChDem_Delete(m->rx);
	sysdep_free(m);
}

/*
 * Is the far end there?  Both receivers park their last return value in a
 * field, and zero means carrier is up.  Which field depends on which receiver
 * was built, which is what `mode` is for.
 */
static int
carrier_up(const struct v23modem *m)
{
	if (m->mode != 0)
		return ((const struct bwchdem *)m->rx)->status == 0;
	return ((const struct v23rx *)m->rx)->status == 0;
}

short
V23ModemMain(struct v23modem *m, int *tx_bits, int *tx_nbits, short *tx_out,
	     int tx_count, short *rx_in, int rx_count, int *rx_bits,
	     int *rx_nbits)
{
	int i;

	/*
	 * The original compares these two, announces any difference -- it
	 * prints "V23ModemMain: modem state moved from %d to %d" -- and then
	 * makes them equal again.  That is the only thing `reported` is for.
	 * With the print dropped the comparison has nothing left to guard, but
	 * the assignment stays: the field is part of the object, and a
	 * differential test compares the object.
	 */
	m->reported = m->state;

	switch (m->state) {
	case V23_STATE_TONE:
		/*
		 * Sized by `rx_count`, not `tx_count`.  Both are one frame in
		 * every real caller so it never bites; see D23.
		 */
		FPM_TONE_generate(m->answer_tone, tx_out, (short)rx_count);
		m->elapsed += rx_count;
		if (m->elapsed >= m->tone_samples) {
			m->elapsed = 0;
			m->state = V23_STATE_SILENCE;
		}
		return 1;

	case V23_STATE_SILENCE:
		for (i = 0; i < rx_count; i++)
			tx_out[i] = 0;
		m->elapsed += rx_count;
		/*
		 * `>` here where the tone state uses `>=`, so the silence runs
		 * one whole frame longer than its nominal length.  That is not
		 * a rounding detail: 400 samples is 50 ms, below V.25's
		 * window, and the extra frame is what carries it into range.
		 * See the note at V23_SILENCE_DIVISOR.
		 */
		if (m->elapsed > m->silence_samples) {
			m->elapsed = 0;
			m->state = V23_STATE_DATA;
		}
		return 1;

	default:
		break;
	}

	/*
	 * Data.  Transmit first, then receive -- so the carrier test below is
	 * made against the PREVIOUS block's receive result, which is the only
	 * one that exists when the transmitter needs it.
	 */
	if (!carrier_up(m)) {
		for (i = 0; i < *tx_nbits; i++)
			tx_bits[i] = 1;		/* mark */
	}

	v23FP_tx_progress(m->tx, tx_out, tx_count, tx_bits, tx_nbits);

	/*
	 * The two returns are not widened the same way in the original: it
	 * sign-extends after v23FP_rx_progress (`cwtl` at 0x86c28) and returns
	 * BwChDem_Progress's %eax untouched.  So its own return type is `int`,
	 * with one of the two receivers declared `short` to it and the other
	 * `int` -- a header disagreement, and the kind that normally costs
	 * something.
	 *
	 * It costs nothing here.  Every return path in BwChDem_Progress leaves
	 * a whole 32-bit value in %eax: either `movswl 0x66(%esi),%eax` at
	 * 0x87896, sign-extending the status field, or `mov $0x2,%eax` at
	 * 0x878c6.  There is no path that writes only the low half.  So the
	 * unwidened return is 0, 1 or 2 exactly as the widened one is, and
	 * declaring this function `short` -- which forces both -- is the same
	 * function.  Checked rather than assumed: if BwChDem_Progress ever
	 * grew a path that left rubbish above bit 15, the original's host end
	 * would take v23_process's `else` branch and report DPSTAT_ERROR on
	 * every block.
	 */
	if (m->mode != 0)
		return BwChDem_Progress(m->rx, rx_in, (short)rx_count,
					rx_bits, rx_nbits);
	return v23FP_rx_progress(m->rx, rx_in, rx_count, rx_bits, rx_nbits);
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  32-bit ABI only; three of the fields are pointers.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V23M_ASSERT_OFF(field, off) \
	typedef char v23m_off_##field[ \
		((int)__builtin_offsetof(struct v23modem, field) == (off)) \
		? 1 : -1]

V23M_ASSERT_OFF(mode, 0x00);
V23M_ASSERT_OFF(state, 0x04);
V23M_ASSERT_OFF(reported, 0x08);
V23M_ASSERT_OFF(elapsed, 0x0c);
V23M_ASSERT_OFF(tone_samples, 0x10);
V23M_ASSERT_OFF(silence_samples, 0x14);
V23M_ASSERT_OFF(sample_rate, 0x18);
V23M_ASSERT_OFF(answer_tone, 0x1c);
V23M_ASSERT_OFF(tx, 0x20);
V23M_ASSERT_OFF(rx, 0x24);

/* 0x28 is the size sysdep_malloc is asked for. */
typedef char v23m_size[(sizeof(struct v23modem) == 0x28) ? 1 : -1];

#endif
