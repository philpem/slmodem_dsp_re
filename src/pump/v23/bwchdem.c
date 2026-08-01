/*
 * bwchdem.c -- ITU-T V.23: the backward-channel demodulator.
 *
 * V.23's backward channel is 75 bps FSK, 390 Hz for a mark and 450 Hz for a
 * space.  At 8 kHz that is 106 2/3 samples per bit, which is why nothing here
 * counts in bits: the whole module works in quarter-bit blocks taken from a
 * twelve-entry table that sums to 320 samples -- three bits exactly -- and
 * decides one bit every fourth block.
 *
 * HOW IT DEMODULATES.  Not with a discriminator, the way the 1200 bps side
 * does.  Two single-pole-pair resonators run in parallel, one tuned near each
 * tone; the squared output of each is accumulated over a quarter-bit block,
 * and the bit is whichever accumulator is larger.  That is cheap and it works
 * at 75 bps, where a bit is long enough that a resonator has time to ring.
 *
 * BIT TIMING comes free from the same mechanism.  `settle` is reloaded to 2
 * whenever the decision changes, which re-centres the sampling instant on the
 * transition, and to 3 after a bit is emitted, which is what makes it one bit
 * per four blocks.  There is no separate timing-recovery loop.
 *
 * BEFORE ANY OF THAT the module waits for carrier: ten consecutive blocks in
 * which FPM_TONE_detect reports 390 Hz present.  The backward channel idles
 * at mark, so a steady 390 Hz IS the idle condition and detecting it is how
 * the receiver knows the channel exists.
 */

#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_iir.h"
#include "dsplib/sysdep.h"
#include "dsplib/v23fp.h"

/*
 * The quarter-bit block sizes, twelve of them summing to 320 samples.  Three
 * bits of 106 2/3 samples cannot be written as integers; twelve blocks
 * averaging 26 2/3 can be, and the pattern below is that average distributed
 * as evenly as integers allow.
 */
static const int block_size_table[12] = {
	26, 27, 26, 27, 27, 27,
	26, 27, 27, 27, 26, 27,
};

/*
 * The two resonators, one biquad each, in FPM_iir_filt's coefficient order
 * { -a1, b1, -a2, b2, b0 }.
 *
 * Both have a pole radius of 0.95.  Solving for the pole angles puts
 * Space_Coefs at 451 Hz, which is V.23's space to within a hertz, and
 * Mark_Coefs at 364 Hz, which is 26 Hz below the nominal 390.  The tone
 * detector this module gates on is configured for 390 exactly, so the offset
 * is in the discriminator alone.  Recorded rather than explained: a
 * deliberate bias away from the 450 Hz filter and a design slip look the same
 * from here, and the bytes are what the original ran.
 */
static const short Mark_Coefs[FPM_IIR_COEFF_PER_SECTION] = {
	-14786, 16384, 29861, -31242, 16384,
};

static const short Space_Coefs[FPM_IIR_COEFF_PER_SECTION] = {
	-14786, 16384, 29206, -30743, 16384,
};

/*
 * The channel filter ahead of both resonators: 3 biquads in
 * FPM_iir_filt_II's order { b0, b2, b1, a2, a1 }.
 */
static const short BwCh_IIR_FILT[15] = {
	36, 0, 36, 0, -14009,
	16384, 16384, -13845, 13198, -27940,
	16384, 16384, -23027, 15350, -28296,
};

/*
 * Gain control.  Its own configuration, and its own copy of the smoother
 * coefficients -- v23rx.c has statics of these same three names at different
 * addresses.  The contents happen to be identical for the coefficients and
 * differ for the config; the duplication is the original's and is kept
 * because merging them would merge two tables that are only accidentally
 * equal.  See b103_agc_cfg.c for why only element 0 is ever selected.
 */
static const short V23_AGC_DEF_ALPHA[2] = { 16384, 32604 };
static const short V23_AGC_DEF_BETA[2] = { 16384, 1638 };

static const struct fpm_agc_cfg AGCv23_CFG = {
	.ref_level = 16384,
	.acquire_level = 80,
	.squelch_level = 80,
	.f06 = 1000,
	.f08 = 0,
	.block_len = 40,
	.alpha = V23_AGC_DEF_ALPHA,
	.beta = V23_AGC_DEF_BETA,
	.f14 = 0
};

/* The tone the module waits for: V.23's backward-channel mark. */
#define BWCH_MARK_HZ		390

/* Overrides applied to the shared FPM_TONE configuration. */
#define BWCH_TONE_RATIO		28996	/* 0.885 in Q15 */
#define BWCH_TONE_MIN_LEVEL	3

/* Carrier is up once the tone has been present for this many blocks. */
#define BWCH_CARRIER_BLOCKS	9

/* Give up if carrier never arrives within this many blocks. */
#define BWCH_CARRIER_TIMEOUT	1000

/* Silence is charged at this much per block against the configured limit. */
#define BWCH_SILENCE_PER_BLOCK	20

/* Blocks to wait before the next bit, after a decision and after a change. */
#define BWCH_SETTLE_AFTER_BIT	3
#define BWCH_SETTLE_ON_CHANGE	2

/* What Progress returns. */
#define BWCH_DEMODULATED	0
#define BWCH_WAITING		1
#define BWCH_GIVEN_UP		2

struct bwchdem *
BwChDem_Create(struct bwchdem *bw, const struct v23_cfg *cfg)
{
	struct fpm_tone_cfg tone;
	int i;

	if (bw == NULL)
		bw = sysdep_malloc(sizeof(*bw));

	bw->mark_state[0] = 0;
	bw->mark_state[1] = 0;
	bw->space_state[0] = 0;
	bw->space_state[1] = 0;
	bw->mark_energy = 0;
	bw->space_energy = 0;
	bw->r0c = 0;
	bw->blocks = 0;

	bw->remaining = block_size_table[0];
	bw->block_index = 0;
	bw->settle = BWCH_SETTLE_ON_CHANGE;
	bw->last_bit = 1;

	/*
	 * The tone detector starts from the shared V.25 configuration and
	 * changes three fields.  Everything else -- the 53-tap prototype, the
	 * notch radius, the 450 ms reversal period -- comes along unexamined,
	 * which is the original's doing and is why `rev_period` is still set
	 * on a detector that never looks for reversals.
	 */
	tone = FPM_TONE_CFG_data;
	tone.freq = BWCH_MARK_HZ;
	tone.ratio = BWCH_TONE_RATIO;
	tone.min_level = BWCH_TONE_MIN_LEVEL;
	bw->tone = FPM_TONE_create(NULL, &tone);
	bw->carrier_blocks = 0;

	FPM_AGC_init(&bw->agc, &AGCv23_CFG, 1);

	bw->iir_coeff = BwCh_IIR_FILT;
	bw->iir_sections = 3;
	bw->iir_state = sysdep_malloc(FPM_IIR_II_STATE_PER_SECTION * 3
				      * sizeof(short));
	for (i = 0; i < FPM_IIR_II_STATE_PER_SECTION * 3; i++)
		bw->iir_state[i] = 0;

	bw->silence = 0;
	bw->status = BWCH_WAITING;
	bw->silence_limit = cfg->silence_limit;

	return bw;
}

void
BwChDem_Delete(struct bwchdem *bw)
{
	if (bw == NULL)
		return;
	FPM_TONE_delete(bw->tone);
	sysdep_free(bw->iir_state);
	sysdep_free(bw);
}

/*
 * Decide one bit from a completed quarter-bit block.
 *
 * `space` is passed in rather than read back from the object because the
 * original never stores it on this path -- the accumulator in memory still
 * holds the previous block's value at the moment the decision is made, and
 * both are cleared immediately afterwards, so only the register matters.
 *
 * The rule has two halves.  When exactly one accumulator is above the floor
 * of 1, that one wins outright; when both are or neither is, the larger wins.
 * The first half is what makes the decision robust to one resonator sitting
 * in noise, which is the usual case at the start of a bit.
 */
static int
decide_bit(short mark, short space)
{
	int mark_hi = mark > 1;
	int space_hi = space > 1;

	if (space_hi == mark_hi)
		return mark <= space;
	return space_hi;
}

/*
 * Demodulate one block.  Bits go to `bits`, with the count written through
 * `nbits` -- usually one per four calls, because a bit spans four blocks.
 *
 * Returns BWCH_DEMODULATED once carrier is up, BWCH_WAITING while it is
 * being acquired, and BWCH_GIVEN_UP if carrier never arrived or the line went
 * quiet for longer than the configuration allows.
 */
short
BwChDem_Progress(struct bwchdem *bw, short *samples, short count, int *bits,
		 int *nbits)
{
	int signal;
	int i;

	*nbits = 0;

	FPM_iir_filt_II(samples, bw->iir_coeff, bw->iir_state,
			bw->iir_sections, count);

	/*
	 * The original passes FPM_AGC_agc a fourth argument and the function
	 * has three; it is simply ignored.  It also uses the value left in
	 * %eax on return, which is the `signal` flag the same instruction
	 * stored in the object -- so read the field, which is the same number
	 * and does not depend on a return value the function never promised.
	 */
	FPM_AGC_agc(&bw->agc, samples, (unsigned short)count);
	signal = bw->agc.signal;

	if (bw->carrier_blocks <= BWCH_CARRIER_BLOCKS) {
		/*
		 * Still acquiring.  FPM_TONE_PRESENT is zero, so a zero
		 * return means the 390 Hz idle mark is there and the counter
		 * advances; anything else resets it.
		 */
		if (FPM_TONE_detect(bw->tone, samples, count)
		    == FPM_TONE_PRESENT)
			bw->carrier_blocks = (short)(bw->carrier_blocks + 1);
		else
			bw->carrier_blocks = 0;

		/* Tested before the increment, exactly as the original does. */
		if (bw->blocks > BWCH_CARRIER_TIMEOUT) {
			bw->blocks = (short)(bw->blocks + 1);
			return BWCH_GIVEN_UP;
		}
		bw->blocks = (short)(bw->blocks + 1);

		bw->status = BWCH_WAITING;
		if (bw->carrier_blocks <= BWCH_CARRIER_BLOCKS)
			return bw->status;
		/* Carrier just came up: demodulate this block after all. */
	}

	if (signal == 0) {
		/*
		 * Silence.  Charged at a flat rate per block against the
		 * limit the configuration set, which is a timeout in the same
		 * units and not a sample count.
		 */
		bw->silence = (short)(bw->silence + BWCH_SILENCE_PER_BLOCK);
		if ((int)(unsigned short)bw->silence >= bw->silence_limit)
			return BWCH_GIVEN_UP;
	} else {
		bw->silence = 0;
	}

	for (i = 0; i < count; i++) {
		int x = samples[i] >> 4;
		short mark, space;
		int space_energy;

		mark = FPM_iir_filt((short)x, Mark_Coefs, bw->mark_state, 1);
		bw->mark_energy = (short)(((mark * mark) >> 8)
					  + (unsigned short)bw->mark_energy);

		space = FPM_iir_filt((short)x, Space_Coefs, bw->space_state, 1);
		space_energy = ((space * space) >> 8)
			       + (unsigned short)bw->space_energy;

		if (--bw->remaining != 0) {
			bw->space_energy = (short)space_energy;
			continue;
		}

		/* The block is complete: one decision. */
		{
			int bit = decide_bit(bw->mark_energy,
					     (short)space_energy);

			if (bit != bw->last_bit)
				bw->settle = BWCH_SETTLE_ON_CHANGE;
			bw->last_bit = bit;

			if (bw->settle != 0) {
				bw->settle--;
			} else {
				bw->settle = BWCH_SETTLE_AFTER_BIT;
				bits[*nbits] = bit;
				(*nbits)++;
			}
		}

		bw->space_energy = 0;
		bw->mark_energy = 0;
		bw->block_index = (bw->block_index + 1) % 12;
		bw->remaining = block_size_table[bw->block_index];
	}

	bw->status = BWCH_DEMODULATED;
	return bw->status;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  See the note in v23tx.c.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define BWCH_ASSERT_OFF(field, off) \
	typedef char bwch_off_##field[ \
		((int)__builtin_offsetof(struct bwchdem, field) == (off)) \
		? 1 : -1]

BWCH_ASSERT_OFF(mark_state, 0x00);
BWCH_ASSERT_OFF(space_state, 0x04);
BWCH_ASSERT_OFF(mark_energy, 0x08);
BWCH_ASSERT_OFF(space_energy, 0x0a);
BWCH_ASSERT_OFF(blocks, 0x0e);
BWCH_ASSERT_OFF(remaining, 0x10);
BWCH_ASSERT_OFF(block_index, 0x14);
BWCH_ASSERT_OFF(settle, 0x18);
BWCH_ASSERT_OFF(last_bit, 0x1c);
BWCH_ASSERT_OFF(silence, 0x20);
BWCH_ASSERT_OFF(silence_limit, 0x24);
BWCH_ASSERT_OFF(carrier_blocks, 0x28);
BWCH_ASSERT_OFF(tone, 0x2c);
BWCH_ASSERT_OFF(agc, 0x30);
BWCH_ASSERT_OFF(iir_coeff, 0x5c);
BWCH_ASSERT_OFF(iir_state, 0x60);
BWCH_ASSERT_OFF(iir_sections, 0x64);
BWCH_ASSERT_OFF(status, 0x66);

/* 0x68 is the size BwChDem_Create asks sysdep_malloc for. */
typedef char bwch_size[(sizeof(struct bwchdem) == 0x68) ? 1 : -1];

#endif
