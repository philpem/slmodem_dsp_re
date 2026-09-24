/*
 * v8util.c -- the arithmetic and buffer leaves of the V.8 handshake.
 *
 * Small helpers that everything else in V.8 is built from: a Q14 multiply,
 * an absolute value, the CRC bit step, a coeff copy, the tone-queue arm, and
 * the transmit/receive buffer setup.  They are reconstructed first because the
 * whole of V.8 bottoms out here -- `V8Create` reaches the signal layer through
 * `v8handshakinit`, and the signal layer reaches these.  The cosine table and
 * DFT energy pass are `V8Dftc.c`'s, and the V.21 setup is `V8Dpsk.c`'s, which
 * is where the object keeps them.
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
V8_ASSERT_OFFSET(v8, sym_avail, 0x110);
V8_ASSERT_OFFSET(v8, tx_symbols, 0x11c);
V8_ASSERT_OFFSET(v8, tx_avail, 0x21c);
V8_ASSERT_OFFSET(v8, tx_ring, 0x228);
V8_ASSERT_OFFSET(v8, rx_stage, 0x5c8);
V8_ASSERT_OFFSET(v8, tx_stage, 0x5c0);
V8_ASSERT_OFFSET(v8, tx_shape, 0x77c);
V8_ASSERT_OFFSET(v8, rx_scratch, 0x894);
V8_ASSERT_OFFSET(v8, tx_gain, 0xa42);
V8_ASSERT_OFFSET(v8, cm, 0xa58);
V8_ASSERT_OFFSET(v8, v21_taps, 0xa5c);
V8_ASSERT_OFFSET(v8, detector, 0xad8);
V8_ASSERT_OFFSET(v8, v21_params, 0xc20);
V8_ASSERT_OFFSET(v8, tx_seq, 0xc48);
V8_ASSERT_OFFSET(v8, seq, 0xc54);
V8_ASSERT_OFFSET(v8, tone, 0xda4);
V8_ASSERT_OFFSET(v8, word_count, 0xdbc);
V8_ASSERT_OFFSET(v8, quick_connect, 0xdc4);
V8_ASSERT_OFFSET(v8, deadline_a, 0xe5c);
V8_ASSERT_OFFSET(v8, agc_line, 0xe68);
V8_ASSERT_OFFSET(v8, fn_matched, 0xebc);
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

/* Arm the tone queue: nothing pending, and the period set to 0x688. */
void
v8_TONEq_init(struct v8 *v)
{
	v->toneq_pending = 0;
	v->toneq_period = 0x688;
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

	v->short_014 = 1;
	v->short_00c = 0;
	v->short_018 = 0;
	v->int_004 = 0;

	for (i = 0; i < V8_TX_SHAPE; i++)
		v->tx_shape[i] = 0;

	v->tx_ring_base = v->tx_ring;
	for (i = 0; i < V8_TX_RING; i++)
		v->tx_ring[i] = 0;

	v->tx_avail = 0x20;
	v->tx_ring_half = v->tx_ring + V8_TX_RING_HALF;

	v->tx_sym_a = v->tx_symbols;
	v->tx_sym_b = v->tx_symbols;
	v->sym_avail = 0;
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

	v->rx.gain_ref = 0x200;
	v->rx.hist_idx = 0;
	v->rx.gain = 0x200;
	v->rx.adapt_rate = 0x3333;

	for (i = 0; i < V8_RX_HIST; i++)
		v->rx.hist[i] = 0;

	v->rx.accum = 0;
	v->rx.refresh_timer = 0;
	v->rx.stable_timer = 0;
	v->rx.stable = 0;
	v->rx.fc2 = 0x50;
	v->rx.fc8 = 0;
	v->rx.fc6 = 0;
	v->rx.fda = 0;
	v->rx.fd8 = 0;

	v->rx.buf = v->rx_stage;
	v->rx.level = 0;
	/*
	 * One 32-bit store in the original, covering both halves.  They are
	 * two shorts here because v8_agcadapt reads the upper one on its own.
	 */
	v->rx.energy_lo = 0;
	v->rx.energy_hi = 0;
	v->rx.clip_count = 0;

	return 0;
}
