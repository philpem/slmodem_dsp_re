/*
 * dpsk.c -- ITU-T V.34: the FSK discriminator and bit slicer.
 *
 * The file name is the original's and it is wrong twice over: there is no
 * differential encoding here and no phase-shift keying.  What there is, is
 * the receiver for the V.21-rate FSK that carries V.34's INFO messages.  The
 * attribution is not a guess -- `fsklpfcoeff600` is a local symbol between
 * DPSK.c's STT_FILE entry and the next one, and only these two functions
 * reference it.
 *
 * `fskdetect` turns 4 input samples into 3 discriminator outputs, in three
 * stages, and every one of the three is fixed at compile time: the block
 * length, the interpolation factor and the decimation are loop bounds, not
 * parameters.  A caller cannot ask for a different amount of work.
 *
 *   1. INTERPOLATE 3x.  A 12-tap FIR per phase over a shared 13-entry
 *      history.  intcoef1 and intcoef3 are exact time-reverses of each other
 *      and intcoef2 is symmetric, which is one 36-tap linear-phase prototype
 *      decomposed by phase; only the third phase advances the history, which
 *      is what makes the three outputs successive rather than simultaneous.
 *
 *   2. DISCRIMINATE.  Each interpolated sample is pushed into a 49-entry
 *      line and multiplied by the sample `cfg->delay` taps behind it.
 *      Delay-and-multiply: the product's sign follows whether the two are in
 *      phase, so a frequency shift becomes an amplitude one.
 *
 *   3. LOW-PASS and decimate 4:1.  An 80-tap FIR over the products, run once
 *      per 4 of them, with `cfg->offset` subtracted from each result -- which
 *      is what moves the slicer's decision point off zero.
 *
 * ROUNDING IS NOT UNIFORM.  The three interpolator FIRs each start their
 * accumulator at 0x2000, which is half an LSB in Q14 and rounds to nearest.
 * The 80-tap low-pass starts at zero and truncates.  Both are reproduced.
 * It is not obviously deliberate -- the low-pass is the one whose output is
 * sliced against a threshold, so it is the one where half an LSB of bias
 * could matter -- but nothing in the object says which way round it was
 * meant, so nothing here changes it.
 *
 * `fskdemodulate` slices, with a bit clock that restarts on every zero
 * crossing.  See the comments there.
 *
 * See the standing caveat in v34det.h.
 */

#include "dsplib/v34fsk.h"

/* Half an LSB in Q14, the interpolator's rounding offset. */
#define V34_FSK_ROUND		0x2000

/*
 * The polyphase interpolator, one 36-tap linear-phase prototype in three
 * phases.  Global in the object; see docs/coefficients.md for the symmetry
 * and for what the three sum to.
 */
const short intcoef1[V34_FSK_TAPS] = {
	87, 305, -819, -1200, 4976, 11653, 7871, 66, -1368, 139, 195, -46
};

const short intcoef2[V34_FSK_TAPS] = {
	7, 293, -254, -1602, 2209, 10283, 10283, 2209, -1602, -254, 293, 7
};

const short intcoef3[V34_FSK_TAPS] = {
	-46, 195, 139, -1368, 66, 7871, 11653, 4976, -1200, -819, 305, 87
};

/*
 * The post-detection low-pass, 80 taps, symmetric, all positive.
 *
 * A local symbol in the object -- DPSK.c owns it -- so it is static here.
 * The name says 600, which at V.34's 9600 Hz input and this module's 3x
 * interpolation is a cutoff well inside the V.21 channel it has to pass.
 */
static const short fsklpfcoeff600[V34_FSK_LPF_TAPS] = {
	  22,   26,   30,   36,   44,   54,   67,   81,
	  99,  120,  144,  172,  202,  237,  274,  315,
	 359,  406,  456,  508,  562,  618,  675,  733,
	 792,  850,  907,  963, 1018, 1070, 1119, 1165,
	1207, 1245, 1278, 1306, 1329, 1347, 1359, 1365,
	1365, 1359, 1347, 1329, 1306, 1278, 1245, 1207,
	1165, 1119, 1070, 1018,  963,  907,  850,  792,
	 733,  675,  618,  562,  508,  456,  406,  359,
	 315,  274,  237,  202,  172,  144,  120,   99,
	  81,   67,   54,   44,   36,   30,   26,   22
};

/*
 * One interpolator phase.
 *
 * The history is read backwards from its newest-but-one entry, so tap k
 * multiplies history[TAPS-1-k]: an ordinary convolution, written the way the
 * original walks it.
 *
 * The accumulator wraps.  Twelve products of a Q14 coefficient and a full
 * scale sample exceed 32 bits between them, and the original lets that carry
 * round; the unsigned round trip is how this codebase says so without
 * inviting the optimiser to assume it cannot happen (src/dsp/fpm_agc.c).
 */
static int
interp_phase(const short *coeff, const short *hist)
{
	unsigned acc = V34_FSK_ROUND;
	int k;

	for (k = 0; k < V34_FSK_TAPS; k++)
		acc += (unsigned)(coeff[k] * hist[V34_FSK_TAPS - 1 - k]);

	return (int)acc >> 14;
}

void
fskdetect(struct v34_object *obj, const short *in, short *out,
	  const struct v34_fsk *cfg)
{
	short work[V34_FSK_INTERP];
	struct v34_fskdelay *d = obj->fsk_delay;
	short *hist = obj->fsk_interp;
	int i, j, k;

	/* --- 1: interpolate 4 samples to 12 --- */
	for (i = 0; i < V34_FSK_BLOCK; i++) {
		hist[0] = in[i];

		work[i * V34_FSK_PHASES + 0] =
			(short)interp_phase(intcoef1, hist);
		work[i * V34_FSK_PHASES + 1] =
			(short)interp_phase(intcoef2, hist);
		work[i * V34_FSK_PHASES + 2] =
			(short)interp_phase(intcoef3, hist);

		/*
		 * Only the last phase advances the history, and the original
		 * folds the shift into that phase's own loop -- reading
		 * hist[TAPS-1-k] and writing hist[TAPS-k] in the same
		 * iteration.  Walking downwards, so nothing is overwritten
		 * before it has been read.  Kept separate here because the
		 * multiply uses the pre-shift value either way.
		 */
		for (k = V34_FSK_TAPS; k > 0; k--)
			hist[k] = hist[k - 1];
		/*
		 * The history is 13 entries and only 12 are ever read: the
		 * shift writes hist[12] and no filter tap reaches it.  It is
		 * kept because the shift really does store there -- the slot
		 * sits between the history and the low-pass state, and a
		 * reconstruction that stopped at 12 would leave the original
		 * writing a short this struct did not account for.
		 */
	}

	/* --- 2: delay and multiply --- */
	for (j = 0; j < V34_FSK_INTERP; j++) {
		for (k = V34_FSK_DELAY_LINE - 1; k > 0; k--)
			d->line[k] = d->line[k - 1];
		d->line[0] = work[j];

		/*
		 * `delay` indexes the line AFTER the new sample went in at
		 * zero, so a delay of 0 squares the sample rather than
		 * multiplying it by the previous one.  The original computes
		 * this address once, before the loop, and it stays correct
		 * only because the line shifts under it rather than the index
		 * moving.
		 */
		work[j] = (short)((work[j] * d->line[cfg->delay]) >> 14);
	}

	/* --- 3: low-pass, decimating 4:1 --- */
	for (i = 0; i < V34_FSK_PHASES; i++) {
		unsigned acc = 0;		/* no rounding offset here */
		short *lpf = obj->fsk_lpf;

		for (k = 0; k < V34_FSK_BLOCK; k++)
			lpf[V34_FSK_LPF_TAPS - V34_FSK_BLOCK + k] =
				work[i * V34_FSK_BLOCK + k];

		for (k = 0; k < V34_FSK_LPF_TAPS; k++)
			acc += (unsigned)(fsklpfcoeff600[k] * lpf[k]);

		out[i] = (short)(((int)acc >> 14) - cfg->offset);

		for (k = 0; k < V34_FSK_LPF_TAPS - V34_FSK_BLOCK; k++)
			lpf[k] = lpf[k + V34_FSK_BLOCK];
	}
}

void
fskdemodulate(struct v34_object *obj, const short *in, struct v34_fsk *st)
{
	short work[V34_FSK_PHASES];
	int bit_len;
	int wrap;
	short prev;
	int i;

	/* Switched off: not even the detector runs. */
	if (obj->fsk_inhibit != 0)
		return;

	fskdetect(obj, in, work, st);

	/*
	 * Both are read once, before the loop, and the original keeps them in
	 * registers throughout -- so a slicer that changed `bit_len` mid-block
	 * would not see it until the next call.  Nothing does.
	 */
	bit_len = st->bit_len;
	wrap = (int)st->bit_len << 8;
	prev = st->prev;

	for (i = 0; i < V34_FSK_PHASES; i++) {
		short cur;
		int phase;

		/*
		 * Sample the bit.  The value taken is the PREVIOUS sample's
		 * sign, not this one's -- the comparison that decides whether
		 * to sample happens before the current sample is even loaded.
		 */
		if (st->phase >= st->next) {
			st->sr = (short)(((unsigned)st->sr << 1)
					 | (unsigned)(prev > 0 ? st->bit_hi
							       : st->bit_lo));
			st->nbits = (short)(st->nbits + 1);
			st->next = (short)(st->next + bit_len);
		}

		/*
		 * The phase counter runs to bit_len * 256 and then restarts,
		 * which is 256 bit periods -- far longer than any INFO
		 * message, so this is a backstop against a counter that has
		 * been left running, not the bit clock.
		 */
		phase = (short)(st->phase + 1);
		if (phase >= wrap) {
			st->next = st->resync_next;
			phase = 0;
		}

		cur = work[i];

		/*
		 * Bit-clock recovery: any sign change restarts the phase and
		 * the sampling instant together, so the slicer re-centres
		 * itself on every transition it sees.
		 */
		if ((prev < 0) != (cur < 0)) {
			st->phase = 0;
			st->next = st->resync_next;
		} else {
			st->phase = (short)phase;
		}

		prev = cur;
	}

	st->prev = prev;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.
 *
 * The offsets into `struct v34_object` matter more than most in this project:
 * the struct is mostly padding, so a field that drifted would land in a pad
 * and compile silently.  These are the only thing holding it in place until
 * the surrounding translation units arrive.
 *
 * Guarded to a 32-bit ABI because one of the fields is a pointer; see the
 * same note in b103fp.c.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34FSK_ASSERT(name, type, field, off) \
	typedef char v34fsk_off_##name[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V34FSK_ASSERT(delay,       struct v34_fsk, delay,       0x00);
V34FSK_ASSERT(offset,      struct v34_fsk, offset,      0x02);
V34FSK_ASSERT(bit_lo,      struct v34_fsk, bit_lo,      0x04);
V34FSK_ASSERT(bit_hi,      struct v34_fsk, bit_hi,      0x06);
V34FSK_ASSERT(bit_len,     struct v34_fsk, bit_len,     0x08);
V34FSK_ASSERT(resync_next, struct v34_fsk, resync_next, 0x0a);
V34FSK_ASSERT(phase,       struct v34_fsk, phase,       0x0c);
V34FSK_ASSERT(next,        struct v34_fsk, next,        0x0e);
V34FSK_ASSERT(nbits,       struct v34_fsk, nbits,       0x10);
V34FSK_ASSERT(sr,          struct v34_fsk, sr,          0x12);
V34FSK_ASSERT(prev,        struct v34_fsk, prev,        0x14);
typedef char v34fsk_size[(sizeof(struct v34_fsk) == 0x16) ? 1 : -1];

V34FSK_ASSERT(line,    struct v34_fskdelay, line,        0x14);

V34FSK_ASSERT(inhibit, struct v34_object, fsk_inhibit,   0x402);
V34FSK_ASSERT(dline,   struct v34_object, fsk_delay,     0x80c4);
V34FSK_ASSERT(state,   struct v34_object, fsk,           0xaad0);
V34FSK_ASSERT(interp,  struct v34_object, fsk_interp,    0xaae6);
V34FSK_ASSERT(lpf,     struct v34_object, fsk_lpf,       0xab00);

#endif
