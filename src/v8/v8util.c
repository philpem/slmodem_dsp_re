/*
 * v8util.c -- the arithmetic leaves of the V.8 handshake.
 *
 * Six small helpers that everything else in V.8 is built from: a Q14
 * multiply, an absolute value, the cosine table, the CRC bit step, a coeff
 * copy and the energy pass over a DFT bin array.  They are reconstructed
 * first because the whole of V.8 bottoms out here -- `V8Create` reaches the
 * signal layer through `v8handshakinit`, and the signal layer reaches these.
 */

#include <stddef.h>

#include "dsplib/v8.h"

/*
 * The offsets are the whole point of this file, so they are checked at
 * compile time rather than trusted.  Every number below was read out of the
 * object; if a struct is edited carelessly the build stops here instead of a
 * test failing somewhere far away.
 */
/*
 * Only on a 32-bit build.  The object holds pointers at fixed offsets, so its
 * layout is a property of the original's ABI and cannot hold where a pointer
 * is eight bytes.  `make check64` compiles this file for the host purely to
 * keep the C portable, and these numbers are not portable by construction.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V8_ASSERT_OFFSET(s, m, off) \
	typedef char v8_off_##m[offsetof(struct s, m) == (off) ? 1 : -1]
#else
#define V8_ASSERT_OFFSET(s, m, off) \
	typedef char v8_off_##m[1]
#define V8_OFFSETS_UNCHECKED	1
#endif

#ifdef V8_OFFSETS_UNCHECKED
#define V8_OFFSET_OK	0
#else
#define V8_OFFSET_OK	1
#endif

V8_ASSERT_OFFSET(v8, rx, 0x01c);
V8_ASSERT_OFFSET(v8, f110, 0x110);
V8_ASSERT_OFFSET(v8, tx_symbols, 0x11c);
V8_ASSERT_OFFSET(v8, f21c, 0x21c);
V8_ASSERT_OFFSET(v8, tx_ring, 0x228);
V8_ASSERT_OFFSET(v8, rx_stage, 0x5c8);
V8_ASSERT_OFFSET(v8, tx_stage, 0x5c0);
V8_ASSERT_OFFSET(v8, tx_shape, 0x77c);
V8_ASSERT_OFFSET(v8, rx_scratch, 0x894);
V8_ASSERT_OFFSET(v8, fa42, 0xa42);
V8_ASSERT_OFFSET(v8, cm, 0xa58);
V8_ASSERT_OFFSET(v8, v21_taps, 0xa5c);
V8_ASSERT_OFFSET(v8, detector, 0xad8);
V8_ASSERT_OFFSET(v8, v21_params, 0xc20);
V8_ASSERT_OFFSET(v8, tx_seq, 0xc48);
V8_ASSERT_OFFSET(v8, seq, 0xc54);
V8_ASSERT_OFFSET(v8, tone, 0xda4);
V8_ASSERT_OFFSET(v8, fdbc, 0xdbc);
V8_ASSERT_OFFSET(v8, fdc4, 0xdc4);
V8_ASSERT_OFFSET(v8, deadline_a, 0xe5c);
V8_ASSERT_OFFSET(v8, agc_line, 0xe68);
V8_ASSERT_OFFSET(v8, febc, 0xebc);
V8_ASSERT_OFFSET(v8, side, 0xa44);
V8_ASSERT_OFFSET(v8, phase_rev, 0xb40);
typedef char v8_pr_det[V8_OFFSET_OK == 0
			|| offsetof(struct v8_phase_rev, detected) == 0xdc ? 1 : -1];
V8_ASSERT_OFFSET(v8, v21, 0xdd8);
V8_ASSERT_OFFSET(v8, toneq_pending, 0xdd2);
typedef char v8_size_check[V8_OFFSET_OK == 0 || sizeof(struct v8) == V8_STATE_BYTES ? 1 : -1];

typedef char v8_v21_delay[V8_OFFSET_OK == 0 || offsetof(struct v8, v21)
			  + offsetof(struct v8_v21, delay) == 0xe0c ? 1 : -1];
typedef char v8_pr_window[V8_OFFSET_OK == 0 || offsetof(struct v8_phase_rev, window) == 0x14
			  ? 1 : -1];


/*
 * A Q14 cosine table, one full cycle in 256 steps.  Every entry but one is
 * exactly `(short)(16384.0 * cos(2 * PI * i / 256))` with C truncation
 * towards zero.
 *
 * The exception is index 128, half a cycle, where this holds -16383 and the
 * arithmetic says -16384.  That is the floating point showing through: their
 * cosine returned slightly more than -1, so truncation dropped a unit.  Kept
 * verbatim -- it is one LSB and reproducing it costs nothing, whereas
 * regenerating the table would silently change one sample of every tone V.8
 * emits.
 */
const short v8_costab[V8_COSTAB_SIZE] = {
	 16384,  16379,  16364,  16339,  16305,  16260,  16206,  16142,
	 16069,  15985,  15892,  15790,  15678,  15557,  15426,  15286,
	 15136,  14978,  14810,  14634,  14449,  14255,  14053,  13842,
	 13622,  13395,  13159,  12916,  12665,  12406,  12139,  11866,
	 11585,  11297,  11002,  10701,  10393,  10079,   9759,   9434,
	  9102,   8765,   8423,   8075,   7723,   7366,   7005,   6639,
	  6269,   5896,   5519,   5139,   4756,   4369,   3980,   3589,
	  3196,   2801,   2404,   2005,   1605,   1205,    803,    402,
	     0,   -402,   -803,  -1205,  -1605,  -2005,  -2404,  -2801,
	 -3196,  -3589,  -3980,  -4369,  -4756,  -5139,  -5519,  -5896,
	 -6269,  -6639,  -7005,  -7366,  -7723,  -8075,  -8423,  -8765,
	 -9102,  -9434,  -9759, -10079, -10393, -10701, -11002, -11297,
	-11585, -11866, -12139, -12406, -12665, -12916, -13159, -13395,
	-13622, -13842, -14053, -14255, -14449, -14634, -14810, -14978,
	-15136, -15286, -15426, -15557, -15678, -15790, -15892, -15985,
	-16069, -16142, -16206, -16260, -16305, -16339, -16364, -16379,
	-16383, -16379, -16364, -16339, -16305, -16260, -16206, -16142,
	-16069, -15985, -15892, -15790, -15678, -15557, -15426, -15286,
	-15136, -14978, -14810, -14634, -14449, -14255, -14053, -13842,
	-13622, -13395, -13159, -12916, -12665, -12406, -12139, -11866,
	-11585, -11297, -11002, -10701, -10393, -10079,  -9759,  -9434,
	 -9102,  -8765,  -8423,  -8075,  -7723,  -7366,  -7005,  -6639,
	 -6269,  -5896,  -5519,  -5139,  -4756,  -4369,  -3980,  -3589,
	 -3196,  -2801,  -2404,  -2005,  -1605,  -1205,   -803,   -402,
	     0,    402,    803,   1205,   1605,   2005,   2404,   2801,
	  3196,   3589,   3980,   4369,   4756,   5139,   5519,   5896,
	  6269,   6639,   7005,   7366,   7723,   8075,   8423,   8765,
	  9102,   9434,   9759,  10079,  10393,  10701,  11002,  11297,
	 11585,  11866,  12139,  12406,  12665,  12916,  13159,  13395,
	 13622,  13842,  14053,  14255,  14449,  14634,  14810,  14978,
	 15136,  15286,  15426,  15557,  15678,  15790,  15892,  15985,
	 16069,  16142,  16206,  16260,  16305,  16339,  16364,  16379
};

/* Q14 multiply: the product of two Q14 values, back in Q14. */
short
v8_mpyint(short a, short b)
{
	return (short)((a * b) >> 14);
}

/*
 * Absolute value, with the usual two's-complement corner left in place:
 * `v8_absfn(-32768)` is -32768, because negating it overflows and the result
 * is narrowed back to a short.  No caller reaches it -- the signal path is
 * scaled well below full scale -- so it is reproduced rather than fixed.
 */
short
v8_absfn(short x)
{
	if (x < 0)
		return (short)(-x);
	return x;
}

/* One entry of the cosine table.  The index is a byte, so it wraps freely. */
short
v8_cosread(unsigned char phase)
{
	return v8_costab[phase];
}

/*
 * One bit into the CRC-16-CCITT register the handshake carries in its state.
 * Polynomial 0x1021, MSB first, no reflection: shift up, and if the bit
 * leaving the top disagrees with the bit going in, fold the polynomial back.
 *
 * `bit` is compared 16 bits at a time, so a value whose low half is zero
 * counts as a zero bit whatever the upper half holds.
 */
void
v8_crc(struct v8_handshake *hs, int bit)
{
	unsigned int crc = (unsigned short)hs->crc;
	int msb = ((int)(short)crc) < 0 ? 1 : 0;

	crc += crc;
	if ((short)bit != 0)
		msb ^= 1;
	if (msb != 0)
		crc ^= 0x1021;
	hs->crc = (short)crc;
}

/* Copy `n` coefficients.  The counter is a short, so `n` above 32767 never
 * terminates -- no caller comes close. */
void
v8_copycoeff(short *dst, const short *src, short n)
{
	short i;

	for (i = 0; i < n; i++)
		dst[i] = src[i];
}

/*
 * Energy of each DFT bin: the real and imaginary parts are shifted up by
 * `shift`, taken down to their top 16 bits, squared and summed, and the top
 * 16 bits of that are stored.  The shift is how the caller keeps a bin that
 * has grown small from squaring away to nothing.
 */
void
v8_dftenergy(struct v8_dft_bin *bin, short n, short shift)
{
	short i;

	for (i = 0; i < n; i++) {
		int re = (int)((unsigned int)bin[i].re << shift) >> 16;
		int im = (int)((unsigned int)bin[i].im << shift) >> 16;

		bin[i].energy = (short)((re * re + im * im) >> 16);
	}
}

/*
 * Point the V.21 modem at a set of filter designs.  The four are swapped
 * together, which is how one modem serves both channels of V.21: the
 * handshake calls this again whenever it changes direction.
 */
void
V8_setFilters(struct v8 *v, const short *a, const short *b, const short *c,
	      const short *d)
{
	v->v21.a = a;
	v->v21.b = b;
	v->v21.c = c;
	v->v21.d = d;
}

/* Clear the V.21 delay line and the three accumulators behind it. */
void
V8_V21_reset(struct v8 *v)
{
	int i;

	for (i = 0; i < V8_V21_DELAY; i++)
		v->v21.delay[i] = 0;
	v->v21.pos = 0;
	v->v21.space_run = 0;
	v->v21.mark_run = 0;
}

/* Arm the tone queue: nothing pending, and the period set to 0x688. */
void
v8_TONEq_init(struct v8 *v)
{
	v->toneq_pending = 0;
	v->toneq_period = 0x688;
}

/*
 * Arm the ANSam phase-reversal detector.  The window is cleared and the
 * countdown at +0x0e set to 32 -- half the window, which is how long it
 * waits before its first verdict.
 */
void
v8_phase_rev_init(struct v8_phase_rev *pr)
{
	int i;

	pr->detected = 0;
	pr->corr = 0;
	pr->energy = 0;
	pr->smoothed = 0;
	pr->run = 0;
	pr->reversals = 0;
	pr->half = 0x20;
	pr->widx = 0;
	for (i = 0; i < 64; i++)
		pr->window[i] = 0;
}

/*
 * Arm the transmitter.  Three buffers are cleared and four pointers set to
 * point inside them: the symbol buffer gets two pointers to its start, and
 * the ring gets one to its start and one to the sixty-fourth sample -- a read
 * and a write cursor half a buffer apart, which is how the shaping filter is
 * kept fed while the modulator drains behind it.
 */
int
v8_txinit(struct v8 *v)
{
	int i;

	v->f014 = 1;
	v->f00c = 0;
	v->f018 = 0;
	v->f004 = 0;

	for (i = 0; i < V8_TX_SHAPE; i++)
		v->tx_shape[i] = 0;

	v->tx_ring_base = v->tx_ring;
	for (i = 0; i < V8_TX_RING; i++)
		v->tx_ring[i] = 0;

	v->f21c = 0x20;
	v->tx_ring_half = v->tx_ring + V8_TX_RING_HALF;

	v->tx_sym_a = v->tx_symbols;
	v->tx_sym_b = v->tx_symbols;
	v->f110 = 0;
	for (i = 0; i < V8_TX_SYMBOLS; i++)
		v->tx_symbols[i] = 0;

	return 0;
}

/*
 * Arm the receiver.  Note the order at the top: the scratch buffer is cleared
 * and then one element of it is written again.  Reproduced as written --
 * seeding after the clear is what the original does, and doing it the tidy
 * way round would be the same result only by luck of the index.
 */
int
v8_rxinit(struct v8 *v)
{
	int i;

	for (i = 0; i < V8_RX_SCRATCH; i++)
		v->rx_scratch[i] = 0;
	v->rx_scratch[V8_RX_SCRATCH_SEED_INDEX] = V8_RX_SCRATCH_SEED;

	v->rx.f86 = 0x200;
	v->rx.f82 = 0;
	v->rx.f1c = 0x200;
	v->rx.f20 = 0x3333;

	for (i = 0; i < V8_RX_HIST; i++)
		v->rx.hist[i] = 0;

	v->rx.f1e = 0;
	v->rx.f84 = 0;
	v->rx.f88 = 0;
	v->rx.f8a = 0;
	v->rx.fc2 = 0x50;
	v->rx.fc8 = 0;
	v->rx.fc6 = 0;
	v->rx.fda = 0;
	v->rx.fd8 = 0;

	v->rx.buf = v->rx_stage;
	v->rx.f1a = 0;
	/*
	 * One 32-bit store in the original, covering both halves.  They are
	 * two shorts here because v8_agcadapt reads the upper one on its own.
	 */
	v->rx.f14 = 0;
	v->rx.f16 = 0;
	v->rx.fac = 0;

	return 0;
}

/*
 * Arm the tone detector.
 *
 * The original has an empty inner loop here -- three iterations that do
 * nothing -- left over from whatever the accumulators used to be.  It has no
 * effect and is not reproduced; everything that touches memory is.
 */
void
v8_detectorinit(struct v8 *v, struct v8_detector *d, const short *table,
		short a3, short a4, short a5, short a6, short a7)
{
	int i;

	for (i = 0; i < 4; i++) {
		d->acc_a[i] = 0;
		d->acc_b[i] = 0;
	}
	for (i = 0; i < 3; i++) {
		d->acc_c[i] = 0;
		d->acc_d[i] = 0;
	}

	d->f04 = a3;
	d->f08 = (short)-a5;
	d->f0c = 1;
	d->table = table;
	d->f06 = 0;
	d->f0a = a4;
	d->f10 = a6;
	d->f0e = a7;
	d->f12 = 0;
	d->f30 = 0;

	v->rx.flags |= V8_RX_DETECTOR_ARMED;
}

/*
 * Reverse the bits of a nibble.  The original stores this rather than
 * computing it, and `charFlip` uses it twice.
 */
static const unsigned char nibble_reverse[16] = {
	0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
};

unsigned char
charFlip(unsigned char b)
{
	return (unsigned char)((nibble_reverse[b & 0x0f] << 4)
			       | nibble_reverse[b >> 4]);
}
