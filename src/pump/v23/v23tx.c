/*
 * v23tx.c -- ITU-T V.23: the transmitter.
 *
 * The same three functions serve both of V.23's channels.  Nothing in here
 * knows which one it is driving: the composite in v23modem.c decides that by
 * handing over a pair of frequencies and a table of bit periods, so the 1200
 * bps forward channel and the 75 bps backward channel differ only in their
 * arguments.  That is why this file is 638 bytes for a modem with two
 * asymmetric directions.
 *
 * BIT PERIODS.  1200 bps at 8 kHz is 6 2/3 samples per bit, which no integer
 * counter can hold.  The original carries a short cyclic table instead --
 * { 7, 7, 6 }, summing to 20 samples per 3 bits -- and steps through it one
 * entry per bit.  The backward channel uses { 107, 107, 106 }: 320 samples
 * per 3 bits, which is 75 bps to the same exactness.  Both tables live in
 * v23modem.c, which is also where their names (fw_ch_samp_per_bit_table and
 * bw_ch_samp_per_bit_table) come from.
 *
 * CARRYING A BIT ACROSS CALLS.  The caller asks for a number of samples, not
 * a number of bits, so a call almost always ends part way through a bit.  The
 * object keeps the bit being sent in `held` and sets `resume`; the next call
 * finishes it from there rather than re-reading it.
 *
 * WHICH MAKES `consumed` A TRAP, and it is worth being exact about.  It
 * counts bits FINISHED.  A bit that is still in flight has already been TAKEN
 * from the caller's buffer and is not counted, so `consumed` is one short of
 * the number of buffer slots read whenever a bit is held.
 *
 * The contract that follows is: **`bits[0]` is the next bit that has never
 * been handed over**.  A caller must refill from index zero with `consumed`
 * fresh bits each call, which is what v23_process does.  A caller that
 * instead advances a cursor into a long array by `consumed` re-supplies the
 * held bit, and this object sends it twice -- once out of `held` and once as
 * the next bit to start.
 *
 * That mistake is invisible on the forward channel, where 160 samples is
 * exactly eight cycles of { 7, 7, 6 } and no bit is ever held.  On the
 * backward channel every 160-sample block ends mid-bit, so it corrupts every
 * third bit -- which is how this paragraph came to replace an earlier one
 * saying a caller could simply advance by `consumed`.  It was the SpanDSP
 * interop test that found it; no differential test could, because the blob
 * does exactly the same thing with the same wrong caller.
 */

#include "dsplib/sysdep.h"
#include "dsplib/v23fp.h"

/*
 * The transmit tone's configuration, 36 bytes at .rodata:0x82a0.  v23rx.c
 * has a static of this same name with different contents -- see the note in
 * v23fp.h about why neither may be hoisted into a header.
 *
 * `freq` is only the starting value: v23FP_tx_progress calls
 * FPM_TONE_set_freq for every sample, so what is set here never reaches the
 * line.  1650 Hz is the midpoint of V.23's 1300/2100 pair, which is what a
 * detector configuration would want and a generator has no use for -- the two
 * roles share one struct.
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
 * The gain the generator actually runs at, overriding the 3200 above.
 * v23modem.c applies the same value to its own answer-tone generator, so it
 * is V.23's transmit level rather than anything specific to this object.
 */
#define V23TX_SCALE	0xee4

/*
 * Step to the next bit period and count the bit that just ended.
 *
 * The index arithmetic is written the way the original computes it: the
 * counter is read as unsigned, incremented, then truncated to 16 bits and
 * sign-extended before the division.  With an index that stays inside a
 * three-entry table the truncation never bites, but it is what the object
 * does and costs nothing to keep.
 */
static void
next_period(struct v23tx *tx, int *consumed)
{
	(*consumed)++;
	tx->period_index = (short)((short)(tx->period_index + 1)
				   % tx->period_len);
	tx->remaining = (unsigned short)tx->period[tx->period_index];
}

struct v23tx *
v23FP_tx_create(struct v23tx *tx, short mark, short space, short period_len,
		const short *period, int mute)
{
	struct fpm_tone_cfg cfg;

	if (tx == NULL)
		tx = sysdep_malloc(sizeof(*tx));

	tx->period_index = 0;
	tx->period_len = period_len;
	tx->period = period;
	tx->remaining = (unsigned short)period[0];
	tx->resume = 0;
	tx->mute = mute;
	tx->space = space;
	tx->mark = mark;

	/*
	 * `held` is deliberately not set, because the original does not set
	 * it: it is only read when `resume` is non-zero, and only the tail of
	 * v23FP_tx_progress sets that -- after writing `held` in the same
	 * breath.  Left alone here so the object matches byte for byte under
	 * the harness's fill pattern.
	 */

	cfg = TONEv23_CFG;
	cfg.scale = V23TX_SCALE;
	cfg.src = FPM_TONE_CFG_data.src;	/* the shared 53-tap prototype */
	tx->tone = FPM_TONE_create(NULL, &cfg);

	return tx;
}

void
v23FP_tx_delete(struct v23tx *tx)
{
	if (tx == NULL)
		return;
	FPM_TONE_delete(tx->tone);
	sysdep_free(tx);
}

/*
 * Generate `count` samples from `bits`, one int per bit, and report through
 * `consumed` how many bits that finished.
 *
 * `bits` is advanced internally and the caller's copy is untouched, which is
 * why it is passed by value and `consumed` exists at all.
 */
void
v23FP_tx_progress(struct v23tx *tx, short *out, int count, const int *bits,
		  int *consumed)
{
	int done = 0;
	int bit = 0;

	/*
	 * A one-shot mute.  Emits silence for the whole block, clears itself,
	 * and touches neither the bit stream nor the period counter.
	 *
	 * It reports `count` through `consumed`, which is a sample count where
	 * every other path reports a bit count.  That is the original's
	 * behaviour and is reproduced; a caller that advanced its bit stream
	 * by the returned figure would skip a block's worth of data.  See
	 * docs/deviations.md.
	 */
	if (tx->mute) {
		int i;

		for (i = 0; i < count; i++)
			out[i] = 0;
		tx->mute = 0;
		*consumed = count;
		return;
	}

	while (count > 0) {
		unsigned short r = tx->remaining;

		tx->remaining = (unsigned short)(r - 1);
		if (r == 0) {
			/*
			 * The bit just ended.  Advance the stream only if this
			 * was not the tail of a bit carried over from the last
			 * call -- in that case the caller has not yet been
			 * told the bit was consumed, so its stream still
			 * starts at the same place ours does.
			 */
			if (tx->resume)
				tx->resume = 0;
			else
				bits++;
			next_period(tx, &done);
			continue;
		}

		bit = tx->resume ? tx->held : *bits;
		FPM_TONE_set_freq(tx->tone, bit ? tx->mark : tx->space);
		count--;
		FPM_TONE_generate_demod(tx->tone, out, 1);
		out++;
	}

	if (tx->remaining != 0) {
		/* Stopped part way through a bit: carry it. */
		tx->resume = 1;
		tx->held = bit;
	} else {
		/*
		 * The block ended exactly on a bit boundary, so the bit is
		 * finished.  Count it and load the next period -- but do NOT
		 * advance `bits`, because it is the caller that owns the
		 * stream and `consumed` is how it is told to move.
		 */
		next_period(tx, &done);
	}

	*consumed = done;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.
 *
 * Every offset here was read off v23FP_tx_create's stores and
 * v23FP_tx_progress's loads, so an assertion failing means the struct and the
 * disassembly have drifted apart -- which a differential test would also
 * catch, but only after a confusing failure rather than at compile time.
 *
 * Guarded to a 32-bit ABI because two of the fields are pointers.  See the
 * same note in b103fp.c.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V23TX_ASSERT_OFF(field, off) \
	typedef char v23tx_off_##field[ \
		((int)__builtin_offsetof(struct v23tx, field) == (off)) ? 1 : -1]

V23TX_ASSERT_OFF(period_index, 0x00);
V23TX_ASSERT_OFF(period_len, 0x02);
V23TX_ASSERT_OFF(period, 0x04);
V23TX_ASSERT_OFF(remaining, 0x08);
V23TX_ASSERT_OFF(held, 0x0c);
V23TX_ASSERT_OFF(resume, 0x10);
V23TX_ASSERT_OFF(mute, 0x14);
V23TX_ASSERT_OFF(space, 0x18);
V23TX_ASSERT_OFF(mark, 0x1a);
V23TX_ASSERT_OFF(tone, 0x1c);

/* 0x20 is the size sysdep_malloc is asked for when `state` is NULL. */
typedef char v23tx_size[(sizeof(struct v23tx) == 0x20) ? 1 : -1];

#endif
