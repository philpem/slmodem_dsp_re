/*
 * b103fp.c -- Bell 103 / V.21 transmit path and carrier detect.
 *
 * Reconstructed from dsplibs.o b103fp.c:
 *   ModDataB103        .text 0x08f9a0    99 bytes
 *   TxNoCarrierB103    .text 0x08fa10   124 bytes
 *   CarrierDetectB103  .text 0x08fcd0    16 bytes
 *
 * The transmitter is two calls and nothing else.  All the interesting
 * decisions were made in B103FP_create, which sized the FSK modulator at 24
 * samples per symbol -- 300 baud at 7200 Hz -- and configured the rate
 * converter 10:9 to lift that to the 8000 the datapump interface speaks.
 * See findings 17 and 24.
 */

#include "dsplib/b103fp.h"

/*
 * Modulate, then resample.
 *
 * The intermediate lands in the shared scratch buffer at dsp->scratch, which
 * is 324 bytes -- 162 samples, or six bits' worth at 24 samples each.  Nothing
 * here checks that `nbits` fits; B103FP_modem is what keeps the request small
 * enough, and the original has no guard either.
 */
short
ModDataB103(struct b103fp *fp, const unsigned short *bits, short *out,
	    unsigned short nbits)
{
	struct b103_dsp *dsp = fp->dsp;
	unsigned short nsamples;

	nsamples = (unsigned short)FPM_FSM_modulate(&dsp->fsm, bits,
						    dsp->scratch, nbits);

	return (short)(unsigned short)FPM_MRF_filter(&dsp->tx_mrf,
						     dsp->scratch, out,
						     nsamples);
}

/*
 * Silence, with the transmitter still running.
 *
 * Identical to ModDataB103 except that the modulator's output scale is forced
 * to zero for the duration and restored afterwards.  That matters: the phase
 * accumulator, the symbol counter and the resampler history all advance
 * exactly as they would have, so when the carrier comes back it comes back in
 * phase and on the same symbol boundary.  Muting the output buffer instead
 * would leave the modulator's state behind by however long the gap was.
 *
 * Note the scale is saved and restored around the FSM call only -- the
 * resampler runs with whatever it was given, which by then is silence anyway.
 */
short
TxNoCarrierB103(struct b103fp *fp, const unsigned short *bits, short *out,
		unsigned short nbits)
{
	struct b103_dsp *dsp = fp->dsp;
	short saved_scale = dsp->fsm.scale;
	unsigned short nsamples;

	dsp->fsm.scale = 0;
	nsamples = (unsigned short)FPM_FSM_modulate(&dsp->fsm, bits,
						    dsp->scratch, nbits);
	dsp->fsm.scale = saved_scale;

	return (short)(unsigned short)FPM_MRF_filter(&dsp->tx_mrf,
						     dsp->scratch, out,
						     nsamples);
}

/*
 * Carrier present.
 *
 * A bitwise AND of the two receiver flags, not a logical one -- the original
 * is literally `and %ecx,%eax`.  Both are set to 0 or 1 by the receiver, so
 * the two agree in practice; reproduced as written because nothing guarantees
 * it stays that way.
 */
int
CarrierDetectB103(struct b103fp *fp)
{
	struct b103_dsp *dsp = fp->dsp;

	return dsp->rx_tone & dsp->rx_energy;
}

/*
 * The structs above are laid out by hand around a handful of known offsets.
 * If a reserved region is ever resized to name a new field, these fail at
 * compile time rather than silently moving the transmit path's fields.
 *
 * 32-BIT ONLY, and necessarily so.  Unlike the rest of this tree, these two
 * structs are not merely *tested* against a 32-bit object -- they describe
 * one.  Their reserved regions are byte counts measured from a build where
 * pointers are four bytes, so under any other ABI the named fields land
 * somewhere else and the assertions below are simply false rather than
 * violated.  `make check64` exists to prove the *code* compiles cleanly for a
 * 64-bit target, not that this layout survives one; it cannot, and nothing in
 * this tree needs it to.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define B103_ASSERT_OFF(type, field, off) \
	typedef char b103_off_##field[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

B103_ASSERT_OFF(struct b103_dsp, rx_energy, 0x04);
B103_ASSERT_OFF(struct b103_dsp, rx_tone, 0x08);
B103_ASSERT_OFF(struct b103_dsp, agc, 0x0c);
B103_ASSERT_OFF(struct b103_dsp, tx_mrf, 0x64);
B103_ASSERT_OFF(struct b103_dsp, rx_mrf, 0x80);
B103_ASSERT_OFF(struct b103_dsp, fsd, 0x9c);
B103_ASSERT_OFF(struct b103_dsp, fsm, 0xd4);
B103_ASSERT_OFF(struct b103_dsp, scratch, 0xe8);
B103_ASSERT_OFF(struct b103_dsp, rx_scratch, 0xec);
B103_ASSERT_OFF(struct b103_dsp, p_f0, 0xf0);
B103_ASSERT_OFF(struct b103_dsp, rx_state, 0xfc);
B103_ASSERT_OFF(struct b103fp, hdx, 0x50);
B103_ASSERT_OFF(struct b103fp, dsp, 0x54);

typedef char b103_dsp_size[(sizeof(struct b103_dsp) == 0x100) ? 1 : -1];
typedef char b103fp_size[(sizeof(struct b103fp) == 0x58) ? 1 : -1];

#endif /* 32-bit */
