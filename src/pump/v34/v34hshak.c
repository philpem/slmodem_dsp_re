/*
 * v34hshak.c -- ITU-T V.34: the handshake's support functions.
 *
 * `V34hshak.c` is the object's largest C translation unit, 0x5dd10 to
 * 0x71960, and almost all of it is `v34handshak` -- 61,541 bytes across the
 * eighty-seven states of `include/dsplib/v34hshak.h`.  This file is the rest:
 * the routines that state machine calls to bring a receiver or a transmitter
 * up once the negotiation has settled something.
 *
 * The TU boundary is not inferred.  `ApplyBulkDelay` and `getbit` are the two
 * local .text symbols that follow V34hshak.c's `STT_FILE` entry, so the
 * anchor is exact at the bottom; `v34handshak` ends at 0x71955 and
 * `datapumpv34` starts at 0x71960, which fixes the top.
 *
 * WHAT IS HERE, AND WHAT IS NOT.  Seven functions and one table, chosen by
 * testability rather than by theme -- see docs/fastpass.md, whose one
 * unrelaxed rule is that nothing commits without a differential test:
 *
 *     dpskDetectInfo1Init   setfinalrate   setupreceiver   v34modeminit
 *     dpskinit              preempindex    v34handshakinit
 *     StateName
 *
 * `getbit` is NOT here although it is unblocked, and `ApplyBulkDelay` is not
 * either.  Both are file-local, so `objcopy` cannot give them a `ref_` alias
 * and no tier-1 test can call the blob's copy; the project's answer for a
 * local is to drive it through a reconstructed caller, and both are reached
 * only from `v34handshak`.  That is finding 117 exactly, and the same
 * conclusion: they are a task #39-#45 dependency, not a #38 one.
 *
 * THE FOUR SMALL ONES ARE CALLED BY NOTHING IN THE OBJECT.  No relocation
 * and no direct call reaches `dpskDetectInfo1Init`, `dpskinit`,
 * `setupreceiver` or `preempindex` -- only `setfinalrate` has a caller, in
 * `v34handshak`.  They are global, so they are testable regardless, but it
 * means their arguments have to be read out of the code rather than off a
 * call site.  Finding 89 recorded the same shape twice already.
 *
 * `v34handshakinit` is the exception and has six callers, which is where its
 * mode numbers come from; see the declaration in `v34hshak.h`.
 */

#include "dsplib/debug.h"
#include "dsplib/v34det.h"
#include "dsplib/v34digital.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34info.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"

/*
 * ---------------------------------------------------------------------------
 * The rate tables.
 *
 * Fifteen globals that only V34hshak.c refers to -- `setfinalrate`,
 * `probeselect`, `dpskinit`, `v34modeminit` and `v34handshak` between them
 * account for every relocation against all fifteen -- which is what puts
 * them here rather than in a constants file.  Their .rodata addresses sit
 * between V34RX.c's locals and v34filters.c's, which is consistent.
 *
 * Coefficient derivation is deferred to task #47 (docs/fastpass.md): a
 * byte-exact copy is byte-exact, and the differential test proves it with no
 * derivation at all.
 */

/*
 * The transmit power scales, one table per symbol rate, indexed by a
 * pre-emphasis index.  TWO ROWS OF FOURTEEN, and the split is visible in the
 * data rather than assumed: each row is a decreasing sequence that runs out
 * into zeros, and the second starts again at roughly the first's head.  The
 * row length is fourteen for every rate; the number of non-zero entries is
 * not, and grows with the symbol rate -- nine at 2400, thirteen at 3429 --
 * which is the ten pre-emphasis characteristics V.34 defines plus the
 * flat one, truncated where the rate cannot use them all.
 *
 * `setfinalrate` only ever stores the ADDRESS of one of these.  What indexes
 * it is `preempindex` below, and the two are not connected by anything in
 * this object: no function reads a scale table and calls preempindex.
 */
const short scale2400[V34_SCALE_ENTRIES] = {
	4579, 3527, 2512, 1767, 1248,  882,  626,
	 443,  313,    0,    0,    0,    0,    0,
	4579, 3511, 2576, 1817, 1284,  913,  646,
	 456,  323,    0,    0,    0,    0,    0
};

const short scale2800[V34_SCALE_ENTRIES] = {
	   0, 3966, 2957, 2152, 1642, 1207,  884,
	 658,  490,  363,  266,    0,    0,    0,
	   0, 3965, 2964, 2211, 1655, 1238,  914,
	 682,  507,  376,  280,    0,    0,    0
};

const short scale3000[V34_SCALE_ENTRIES] = {
	   0, 4173, 2856, 2394, 1763, 1356, 1005,
	 770,  585,  444,  340,  252,    0,    0,
	   0, 4155, 3123, 2389, 1812, 1391, 1052,
	 799,  606,  456,  348,  263,    0,    0
};

const short scale3200[V34_SCALE_ENTRIES] = {
	   0, 4579, 3133, 2507, 1899, 1452, 1138,
	 886,  681,  507,  402,  313,  240,    0,
	   0, 4579, 3139, 2572, 1985, 1526, 1189,
	 915,  706,  544,  420,  323,  250,    0
};

const short scale3429[V34_SCALE_ENTRIES] = {
	   0, 4579, 3400, 2732, 2047, 1646, 1315,
	1005,  798,  626,  494,  385,  303,  240,
	   0, 4579, 3403, 2730, 2146, 1699, 1343,
	1055,  825,  645,  509,  400,  311,  246
};

/*
 * The receive carrier descriptors, one per carrier frequency.
 *
 * Eight entries of four shorts, and the first two of every one are zero --
 * so this is a four-field record whose leading pair is a cleared state and
 * whose trailing pair is `{-k, 15735}` twice, with k rising as the carrier
 * frequency falls.  15735 is 0.9604 in Q14 and is the same in all eight;
 * only the first of each pair moves.  A resonator's two coefficients, at a
 * guess, but the object never dereferences these here -- `setfinalrate` and
 * `probeselect` store the address and nothing else -- so the guess stays a
 * guess and the derivation is #47's.
 *
 * The names are the object's, `c1800_` included.  The trailing underscore is
 * the original author's, presumably to dodge a collision.
 */
const short c2000[V34_CARRIER_DESC]  = { 0, 0, 0, 0,  -8352, 15735,  -8271, 15735 };
const short c1959[V34_CARRIER_DESC]  = { 0, 0, 0, 0,  -9100, 15735,  -9181, 15735 };
const short c1920[V34_CARRIER_DESC]  = { 0, 0, 0, 0,  -9963, 15735,  -9883, 15735 };
const short c1867[V34_CARRIER_DESC]  = { 0, 0, 0, 0, -11016, 15735, -10937, 15735 };
const short c1829[V34_CARRIER_DESC]  = { 0, 0, 0, 0, -11763, 15735, -11685, 15735 };
const short c1800_[V34_CARRIER_DESC] = { 0, 0, 0, 0, -12328, 15735, -12250, 15735 };
const short c1680[V34_CARRIER_DESC]  = { 0, 0, 0, 0, -14616, 15735, -14541, 15735 };
const short c1600[V34_CARRIER_DESC]  = { 0, 0, 0, 0, -16093, 15735, -16020, 15735 };

/*
 * And two more of the same shape for the phase-2 signalling carriers, which
 * `v34modeminit` hands to the tone detector rather than storing.  Their
 * second coefficient is 15993 where the eight above all use 15735, and
 * `c2400_`'s first is zero -- a resonator at a quarter of the sample rate
 * needs no rotation, which is what a zero there would mean.  The trailing
 * underscores are the object's.
 */
const short c1200_[V34_CARRIER_DESC] = { 0, 0, 0, 0, -22892, 15993, -22892, 15993 };
const short c2400_[V34_CARRIER_DESC] = { 0, 0, 0, 0,      0, 15993,      0, 15993 };

/*
 * The phase-2 DPSK receive band-pass, one per channel, 60 symmetric taps.
 *
 * NOT `const`: both live in .data in the object, not .rodata, so the
 * original declared them writable.  Nothing writes them -- the reconstruction
 * keeps the storage class rather than the intent, because the storage class
 * is what the object records.
 *
 * `dpskinit` installs one of them at receiver +0x2a4, which `modem_serrint`
 * already knows as its 60-tap filter, and the length agrees exactly.
 */
short bpv22high[V34_BPV22_TAPS] = {
	  -68,   120,   160,  -180,  -175,   139,    72,    25,
	  145,  -279,  -414,   532,   616,  -644,  -600,   466,
	  230,   115,   569, -1126, -1773,  2489,  3248, -4018,
	-4765,  5456,  6055, -6534, -6867,  7039,  7039, -6867,
	-6534,  6055,  5456, -4765, -4018,  3248,  2489, -1773,
	-1126,   569,   115,   230,   466,  -600,  -644,   616,
	  532,  -414,  -279,   145,    25,    72,   139,  -175,
	 -180,   160,   120,   -68
};

short bpv22low[V34_BPV22_TAPS] = {
	  -37,  -156,  -209,   -98,    95,   182,    94,   -13,
	   79,   365,   541,   288,  -333,  -842,  -783,  -252,
	  124,  -150,  -743,  -609,   960,  3252,  4243,  2174,
	-2579, -7128, -7911, -3536,  3717,  9196,  9196,  3717,
	-3536, -7911, -7128, -2579,  2174,  4243,  3252,   960,
	 -609,  -743,  -150,   124,  -252,  -783,  -842,  -333,
	  288,   541,   365,    79,   -13,    94,   182,    95,
	  -98,  -209,  -156,   -37
};

/*
 * ---------------------------------------------------------------------------
 * The FSK receiver's cold start, shared by the two functions that do it.
 *
 * `dpskDetectInfo1Init` and `dpskinit` emit these eleven stores and this
 * clearing loop IDENTICALLY -- same values, same fields, differing only in
 * the order the compiler interleaved them, which is not a difference.  So
 * they are one helper here and not two transcriptions; finding 130 is the
 * standing warning about what a shared helper can get wrong, and the answer
 * is that both callers' tests compare the whole object, so a helper that was
 * wrong for one of them could not pass for the other.
 */

/*
 * ONE HUNDRED SHORTS, WHICH IS SEVEN MORE THAN THE TWO ARRAYS.
 *
 * +0xaae6 is `fsk_interp` (13 shorts) and +0xab00 is `fsk_lpf` (80), which is
 * 93; the loop runs to index 99 and so writes seven shorts past the end of
 * the low-pass, into the pad that follows it.  The bound is the object's own
 * `cmp $0x63` and both functions have it, so it is deliberate rather than an
 * off-by-one -- most likely the author sized the clear from a struct that
 * has something else in those fourteen bytes.  Written through a byte offset
 * for that reason: no member spans the range.
 */
static void
fsk_clear(struct v34_object *obj)
{
	short *p = (short *)((char *)obj + 0xaae6);
	int i;

	for (i = 0; i <= 0x63; i++)
		p[i] = 0;
}

/*
 * The demodulator's configuration for the V.21-rate INFO channel.
 *
 * bit_lo/bit_hi are 1 and 0, so the slicer shifts in a ONE for a negative
 * discriminator output -- inverted against the obvious reading, and the
 * reason `sr` starts at -1 rather than 0.  12 samples a bit at this module's
 * 3x interpolation and 9600 Hz input is 300 baud; `next` and `resync_next`
 * are both 6, half a bit, so the first sample is taken mid-cell.
 */
static void
fsk_state_init(struct v34_object *obj)
{
	obj->fsk.delay = 0x30;
	obj->fsk.offset = 0;
	obj->fsk.bit_lo = 1;
	obj->fsk.bit_hi = 0;
	obj->fsk.bit_len = 0xc;
	obj->fsk.resync_next = 6;
	obj->fsk.phase = 0;
	obj->fsk.next = 6;
	obj->fsk.nbits = 0;
	obj->fsk.sr = -1;
	obj->fsk.prev = 0;
}

/*
 * Arm the FSK receiver to look for INFO1, and nothing else.
 *
 * The smaller half of `dpskinit`: the same clear and the same configuration,
 * without the modulator, the band-pass or any of the receiver's own state.
 * So this is the re-arm and `dpskinit` is the cold start.
 */
void
dpskDetectInfo1Init(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	fsk_clear(obj);
	fsk_state_init(obj);
}

/*
 * Bring the phase-2 DPSK link up: modulator, receive band-pass, FSK
 * demodulator and the receiver's AGC.
 *
 * `mode` picks the transmit carrier and `high` the receive channel, and the
 * two are independent -- which is what a full-duplex V.21-style channel
 * needs, since the two directions occupy different bands.
 *
 * The carrier arithmetic is branchless in the object -- `cmp $1; sbb; and
 * $-1200; add $2400` -- and computes 1200 for mode 0 and 2400 for anything
 * else.  600 baud with those carriers is V.34's phase 2 signalling.
 */
void
dpskinit(void *objp, short mode, short high)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
	short carrier = (mode == 0) ? 1200 : 2400;
	int i;

	fsk_clear(obj);

	V34SetupModulator((struct v34_modulator *)((char *)obj + 0x1450),
			  600, carrier, 0, 0, 1);

	rx->f2a4 = high ? bpv22high : bpv22low;

	fsk_state_init(obj);

	/*
	 * The AGC comes up at whatever +0x262 holds, with a step of 0x199a.
	 * `setupreceiver` does the same two stores with 0x2000 instead, so
	 * the step is the only thing that distinguishes the phase-2 AGC from
	 * the data-mode one.
	 */
	rx->agc_step = 0x199a;
	rx->agc_gain = rx->f262;

	/*
	 * Bits 9 and 11 of the receiver's flag word, set together.  Bit 9 is
	 * V34_RX_FLAG_DET_PENDING (finding 114) -- the AGC freeze -- so this
	 * arms a detector and holds the gain still while it settles.
	 */
	rx->flags = (unsigned short)(rx->flags | 0xa00);

	/*
	 * The RMS window and its two scalars.  FORTY-EIGHT SHORTS, which is
	 * twelve more than `rms_buf` holds: +0x13c plus 96 bytes lands
	 * exactly on +0x19c, so the clear covers the 36-sample window and
	 * the 24 bytes of unmapped state between it and the index.  The
	 * bound is the object's `cmp $0x2f`, and landing exactly on a named
	 * field is what says it is deliberate.  Written through a byte
	 * offset because no single member spans it.
	 */
	{
		short *w = (short *)((char *)rx + 0x13c);

		for (i = 0; i <= 0x2f; i++)
			w[i] = 0;
	}
	rx->f19c = 0;
	rx->f19e = 0;
}

/*
 * ---------------------------------------------------------------------------
 * Bringing the modem up.
 */

/*
 * Set the whole V.34 object up for phase 2.
 *
 * Long and almost entirely straight-line: three probe records, thirty-odd
 * scalars, two calls into the modulator, and then the SAME BODY TWICE with
 * four values changed.  `f359c == 0x65` -- the originate/answer flag
 * `preinitdigital` also reads (finding 177) -- picks between them:
 *
 *                        originate (0x65)      answer
 *      +0x25c2                   4                 5
 *      receiver flags            0                 4
 *      phase-2 carrier        1200              2400
 *      receive band-pass  bpv22high          bpv22low
 *      detector coeff        c2400_            c1200_
 *
 * and nothing else differs between the two, which is why the tail below is
 * written once.  The object duplicates all of it; that is the compiler
 * having no reason not to.
 *
 * THE FSK BLOCK IS `dpskinit`'S, INLINED.  Both arms clear the same hundred
 * shorts and write the same eleven fields, so they call the same two helpers
 * this file already has rather than a third and fourth copy.
 *
 * The modulator is set up TWICE: once at 2400 baud with `reset` clear, then
 * again at 600 with it set.  The first leaves the shaping tables loaded for
 * a rate phase 2 does not use, so it reads as preparing the data-mode
 * configuration before overwriting the live one -- but nothing here proves
 * that and the order is simply reproduced.
 */
void
v34modeminit(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
	unsigned char *m = (unsigned char *)obj;
	struct v34_modulator *mod =
		(struct v34_modulator *)((char *)obj + 0x1450);
	int originate;
	int i;

	/*
	 * Three records of 44 bytes at +0xa81c, ending exactly where the
	 * int at +0xa8a0 and then `info0_bits` begin.  Each gets the same
	 * two constants and its own value at +2: 0x600, 0x800, 0xa00.
	 */
	for (i = 0; i < 3; i++) {
		unsigned char *r = m + 0xa81c + i * 0x2c;

		*(short *)(r + 0x28) = 0x50;
		*(short *)(r + 0x2a) = 3000;
		*(short *)(r + 0x00) = 0;
		*(int *)(r + 0x04) = 0;
		*(int *)(r + 0x08) = 0;
	}
	*(short *)(m + 0xa81e) = 0x600;
	*(short *)(m + 0xa84a) = 0x800;
	*(short *)(m + 0xa876) = 0xa00;

	*(short *)(m + 0xa24a) = 1;
	*(int *)(m + 0xa24c) = 0;
	*(short *)(m + 0xa250) = 0;
	*(short *)(m + 0xa252) = 3;
	*(short *)(m + 0xa254) = 9;
	*(int *)(m + 0xa8a0) = 0;

	rx->f25e = 0;
	rx->f260 = 0;
	obj->is_short = 0;
	obj->f25c2 = 4;
	obj->f354c = 0;
	*(short *)(m + 0x3588) = 0;
	*(short *)(m + 0x358a) = 0;
	*(short *)(m + 0xaa78) = 0;
	*(short *)(m + 0x2aa0) = 6;
	*(short *)(m + 0x2aa2) = 0;
	*(short *)(m + 0x25d6) = (short)0x8990;
	*(short *)(m + 0x25d8) = 0;
	*(short *)(m + 0x25da) = 0;

	/* Twelve shorts each, in two unrelated places. */
	for (i = 0; i <= 0xb; i++) {
		*(short *)(m + 0xe84 + i * 2) = 0;
		*(short *)(m + 0x2a68 + i * 2) = 0;
	}

	VPcmV34SetTxScale(obj);

	/* Data-mode rate, and NOT a reset. */
	V34SetupModulator(mod, 2400, 1800, 0, 0, 0);
	rxinit(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V34SetupDemodulator: baudrate %ld, carrier %ld\n",
			(long)2400, (long)1800);

	rx->f128 = 4;
	rx->f1b0 = 0x3e80;
	rx->f1be = 0x3e80;
	rx->f1ae = 0x3e80;
	rx->f1ac = 0x1f40;
	rx->f1ba = 0x10;
	rx->carrier = hsine1800;

	originate = (obj->f359c == 0x65);

	obj->f25c2 = (short)(originate ? 4 : 5);
	rx->flags = (unsigned short)(originate ? 0 : 4);

	fsk_clear(obj);
	V34SetupModulator(mod, 600, (short)(originate ? 1200 : 2400), 0, 0, 1);
	rx->f2a4 = originate ? bpv22high : bpv22low;
	fsk_state_init(obj);

	rx->agc_step = 0x199a;
	rx->flags = (unsigned short)(rx->flags | 0xa00);
	rx->agc_gain = rx->f262;

	{
		short *w = (short *)((char *)rx + 0x13c);

		for (i = 0; i <= 0x2f; i++)
			w[i] = 0;
	}
	rx->f19c = 0;
	rx->f19e = 0;

	preinitdigital(obj);
	txinit(obj);

	detectorinit((struct v34_detector *)((char *)obj + 0x3564),
		     originate ? c2400_ : c1200_, 0, 0xc8, 0x32, 0x600, 0);

	rx->flags = (unsigned short)(rx->flags | 0x200);
}

/*
 * ---------------------------------------------------------------------------
 * Rate selection.
 */

/*
 * Turn the negotiated MP bits into transmit and receive rate settings.
 *
 * The three source bytes are at +0xa9de, +0xa9e0 and +0xa9e2, and each field
 * is extracted and then BIT-REVERSED: V.34 sends these fields most
 * significant bit first, so the value as assembled is backwards and
 * `bitreverse` puts it right.  Three fields come out:
 *
 *     tx pre-emphasis   4 bits, from a9de[1:0] and a9e0[7:6]
 *     tx symbol rate    3 bits, from a9e2[6:4]
 *     rx symbol rate    3 bits, from a9e0[2:0] and a9e2[7]
 *
 * and the two rate codes select 2400, 2800, 3000, 3200 or 3429.  CODES 1, 6
 * AND 7 FALL THROUGH SETTING NOTHING, on each side independently: the object
 * has no default arm, so an unrecognised code leaves the previous rate,
 * scale and carrier in place rather than picking one.  Reproduced.
 *
 * THE HIGH/LOW CARRIER BIT IS NOT IN ONE PLACE.  The transmit side tests
 * a9de bit 2 for all four rates that have a choice; the receive side tests a
 * DIFFERENT bit of a DIFFERENT byte for each rate -- a9ae bit 2, a9b2 bit 0,
 * a9b6 bit 7, a9b8 bit 6.  Those four are one bit-field walking through a
 * packed message at a different alignment per rate, not four flags.  3429
 * has a single carrier and so tests nothing on either side.
 */
void
setfinalrate(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	unsigned short a9de = *(unsigned short *)(m + 0xa9de);
	unsigned short a9e0 = *(unsigned short *)(m + 0xa9e0);
	unsigned short a9e2 = *(unsigned short *)(m + 0xa9e2);
	short *tx_baud    = (short *)(m + 0xaa84);
	short *tx_preemp  = (short *)(m + 0xaa8a);
	const short **tx_scale = (const short **)(m + 0xaa90);
	short *tx_carrier = (short *)(m + 0xaa94);
	short *rx_baud    = (short *)(m + 0xaa96);
	short *rx_carrier = (short *)(m + 0xaaa8);
	const short **rx_scale = (const short **)(m + 0xaaac);
	const short **rx_cdesc = (const short **)(m + 0xaab0);
	int code;
	int high;

	*tx_preemp = (short)bitreverse((unsigned short)
				       (((a9de & 3) << 2)
					| ((a9e0 & 0xc0) >> 6)), 4);

	code = bitreverse((unsigned short)((a9e2 >> 4) & 7), 3);
	high = (a9de & 4) != 0;

	switch (code) {
	case 0:
		*tx_baud = 2400;
		*tx_scale = scale2400;
		*tx_carrier = high ? 1800 : 1600;
		break;
	case 2:
		*tx_baud = 2800;
		*tx_scale = scale2800;
		*tx_carrier = high ? 1867 : 1680;
		break;
	case 3:
		*tx_baud = 3000;
		*tx_scale = scale3000;
		*tx_carrier = high ? 2000 : 1800;
		break;
	case 4:
		*tx_baud = 3200;
		*tx_scale = scale3200;
		*tx_carrier = high ? 1920 : 1829;
		break;
	case 5:
		*tx_baud = 3429;
		*tx_scale = scale3429;
		*tx_carrier = 1959;
		break;
	default:
		break;
	}

	code = bitreverse((unsigned short)(((a9e0 * 2) & 7)
					   | ((a9e2 >> 7) & 1)), 3);

	switch (code) {
	case 0:
		*rx_baud = 2400;
		*rx_scale = scale2400;
		if (m[0xa9ae] & 4) {
			*rx_carrier = 1800;
			*rx_cdesc = c1800_;
		} else {
			*rx_carrier = 1600;
			*rx_cdesc = c1600;
		}
		break;
	case 2:
		*rx_baud = 2800;
		*rx_scale = scale2800;
		if (m[0xa9b2] & 1) {
			*rx_carrier = 1867;
			*rx_cdesc = c1867;
		} else {
			*rx_carrier = 1680;
			*rx_cdesc = c1680;
		}
		break;
	case 3:
		*rx_baud = 3000;
		*rx_scale = scale3000;
		if (m[0xa9b6] & 0x80) {
			*rx_carrier = 2000;
			*rx_cdesc = c2000;
		} else {
			*rx_carrier = 1800;
			*rx_cdesc = c1800_;
		}
		break;
	case 4:
		*rx_baud = 3200;
		*rx_scale = scale3200;
		if (m[0xa9b8] & 0x40) {
			*rx_carrier = 1920;
			*rx_cdesc = c1920;
		} else {
			*rx_carrier = 1829;
			*rx_cdesc = c1829;
		}
		break;
	case 5:
		*rx_baud = 3429;
		*rx_scale = scale3429;
		*rx_carrier = 1959;
		*rx_cdesc = c1959;
		break;
	default:
		break;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V34PROBESELECT, setfinalrate, txbaudrate = %d,"
			"rxbaudrate = %d,txpreemp= %d\n",
			*tx_baud, *rx_baud, *tx_preemp);
}

/*
 * Configure the demodulator for the rate `setfinalrate` chose.
 *
 * Two independent switches, on the receive symbol rate and on the receive
 * carrier, and neither has a default: an unrecognised rate leaves the timing
 * constants alone and an unrecognised carrier leaves the carrier table
 * alone.  Between them they set four timing constants, a sine table and its
 * half-length, and then hand the carrier DESCRIPTOR -- the `c*` table
 * `setfinalrate` stored -- to `detectorinit`.
 *
 * 2743 IS IN THE RATE SWITCH AND NOT IN `setfinalrate`'S.  0xab7 is 2743
 * baud, which is V.34's optional sixth symbol rate; nothing can put it in
 * +0xaa96 through `setfinalrate`, so either another writer can or the arm is
 * left over.  It is reproduced and recorded (D35).
 *
 * The debug string is `V34SetupDemodulator`'s, shared by string pooling --
 * `V34SetupDemodulator` at 0x5def0 prints the same one.  This is a second
 * call site for one literal, not a call to that function.
 */
void
setupreceiver(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
	unsigned char *m = (unsigned char *)obj;
	short baud = *(short *)(m + 0xaa96);
	short carrier = *(short *)(m + 0xaaa8);
	const short *cdesc = *(const short **)(m + 0xaab0);

	rxinit(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V34SetupDemodulator: baudrate %ld, carrier %ld\n",
			(long)baud, (long)carrier);

	rx->f128 = 4;

	switch (baud) {
	case 2400:
		rx->f1b0 = 0x3e80; rx->f1ae = 0x3e80;
		rx->f1be = 0x3e80; rx->f1ac = 0x1f40;
		break;
	case 2743:
		rx->f1b0 = 0x3e80; rx->f1ae = 0x36b0;
		rx->f1be = 0x36b0; rx->f1ac = 0x1f40;
		break;
	case 2800:
		rx->f1b0 = 0x3e82; rx->f1ae = 0x3594;
		rx->f1be = 0x3594; rx->f1ac = 0x1f41;
		break;
	case 3000:
		rx->f1b0 = 0x3e80; rx->f1ae = 0x3200;
		rx->f1be = 0x3200; rx->f1ac = 0x1f40;
		break;
	case 3200:
		rx->f1b0 = 0x3e80; rx->f1ae = 0x2ee0;
		rx->f1be = 0x2ee0; rx->f1ac = 0x1f40;
		break;
	case 3429:
		rx->f1b0 = 0x3e80; rx->f1ae = 0x2bc0;
		rx->f1be = 0x2bc0; rx->f1ac = 0x1f40;
		break;
	default:
		break;
	}

	switch (carrier) {
	case 1600: rx->carrier = hsine1600; rx->f1ba = 6;    break;
	case 1680: rx->carrier = hsine1680; rx->f1ba = 0x28; break;
	case 1800: rx->carrier = hsine1800; rx->f1ba = 0x10; break;
	case 1829: rx->carrier = hsine1829; rx->f1ba = 0x15; break;
	case 1867: rx->carrier = hsine1867; rx->f1ba = 0x24; break;
	case 1920: rx->carrier = hsine1920; rx->f1ba = 5;    break;
	case 1959: rx->carrier = hsine1959; rx->f1ba = 0x31; break;
	case 2000: rx->carrier = hsine2000; rx->f1ba = 0x18; break;
	default: break;
	}

	rx->agc_step = 0x2000;
	rx->agc_gain = rx->f262;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34AGC, setup receiver gain = 0x%x\n",
				     rx->f262);

	/*
	 * Clear bits 8..11 before arming the detector and set bit 9 after.
	 * The clear takes down the AGC freeze that `dpskinit` put up, and
	 * `detectorinit` runs with it down; the set puts it back, which is
	 * what leaves the gain still while the new detector settles.
	 */
	rx->flags = (unsigned short)(rx->flags & 0xf0ff);

	detectorinit((struct v34_detector *)((char *)obj + 0x3564),
		     cdesc, 0, 8, 10, 0x600, 0);

	rx->flags = (unsigned short)(rx->flags | 0x200);
}

/*
 * ---------------------------------------------------------------------------
 * The pre-emphasis index search.
 *
 * WHAT IT DOES.  Take a per-rate starting measurement, multiply it by a
 * per-rate Q14 ratio repeatedly, and return the number of steps it took to
 * exceed a limit -- so it is a logarithm to a rate-dependent base, computed
 * by iteration because the ratios are not powers of two.  The counter starts
 * at 5 and runs to at most 10, which is the top of the pre-emphasis index
 * range, and the three debug strings name the three exits: "index is 0",
 * "index is %d" and "index is 10".
 *
 * WHAT ITS ARGUMENT IS, NOBODY IN THE OBJECT SAYS.  Nothing calls
 * preempindex, so there is no call site to read the type off, and the
 * offsets it uses do not land on anything named: +0x324, +0x350, +0x37c and
 * +0x3d4 are 44 bytes apart -- entries 0, 1, 2 and 4 of a stride-0x2c array,
 * with entry 3 unused -- and 44 is also the stride of the record
 * `V34GiveProbeResults` copies its doubles out of.  That is suggestive and
 * not more, so the parameter is a `void *` here and the offsets are named
 * constants.  Do not give it a struct on the strength of the stride.
 *
 * "index is 0" IS UNREACHABLE.  The counter is incremented at the TOP of the
 * loop, so it is at least 6 by the time the limit test can send control to
 * the `== 5` check, and that branch can never be taken.  Dead in the object,
 * reproduced here, and recorded as D36 -- it reads as a loop that once
 * tested before incrementing.
 */

#define PREEMP_LIMIT	0x0bc	/* the threshold, a short */
#define PREEMP_M2400	0x324	/* also 2800: they share an entry */
#define PREEMP_M3000	0x350
#define PREEMP_M3200	0x37c
#define PREEMP_M3429	0x3d4

static short
preemp_get(const void *p, unsigned off)
{
	return *(const short *)((const char *)p + off);
}

short
preempindex(void *p, short baudrate)
{
	short limit = preemp_get(p, PREEMP_LIMIT);
	short x;
	int ratio;
	short i;

	/*
	 * NO DEFAULT ARM, and the object has none either: an unrecognised
	 * baud rate drops into the loop with `x` and `ratio` never set.  The
	 * two live in callee-saved registers the object does not initialise,
	 * so what the loop actually multiplies is whatever the caller left
	 * in %edx and %esi.  That is not reproducible in C and is not
	 * reproduced -- see D37.  The five rates below are every one the
	 * object handles.
	 */
	x = 0;
	ratio = 0;

	switch (baudrate) {
	case 2400: x = preemp_get(p, PREEMP_M2400); ratio = 0x7da7; break;
	case 2800: x = preemp_get(p, PREEMP_M2400); ratio = 0x6789; break;
	case 3000: x = preemp_get(p, PREEMP_M3000); ratio = 0x656f; break;
	case 3200: x = preemp_get(p, PREEMP_M3200); ratio = 0x639f; break;
	case 3429: x = preemp_get(p, PREEMP_M3429); ratio = 0x6626; break;
	default: break;
	}

	i = 5;
	for (;;) {
		i = (short)(i + 1);
		x = (short)(((int)x * ratio) >> 14);

		if (x > limit) {
			/* Unreachable: i is 6 or more here.  See D36. */
			if (i == 5) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V34PREEMPHASIS, - index is "
						"0, baudrate= %d\n", baudrate);
				return 0;
			}
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V34PREEMPHASIS, - index is %d, "
					"baudrate= %d\n", i, baudrate);
			return i;
		}

		if (i > 9)
			break;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V34PREEMPHASIS, - index is 10, baudrate= %d \n",
			baudrate);
	return i;
}

/*
 * ---------------------------------------------------------------------------
 * The handshake's state names, and the three machines that use them.
 */

/*
 * `StateName`, eighty-seven string pointers at .data+0x6c00.
 *
 * NOT `const` and not in .rodata: `nm` gives it a lowercase `d`, so the
 * original declared an array of pointers to string literals in writable
 * storage.  Same reading, and the same reason, as `bpv22high` above -- the
 * storage class is what the object records, whatever the intent was.
 *
 * `static` because the symbol is LOCAL (`readelf` says so), which is also
 * what makes it untestable the ordinary way: `objcopy --redefine-syms`
 * renames a local symbol but cannot make it linkable, so there is no
 * `ref_StateName` to compare against the way the fifteen rate tables at the
 * top of this file are compared.  That is finding 173's trap a third time.
 *
 * SO THE TRANSCRIPT COMPARISON IS NOT A SUPPLEMENTARY CHECK ON THESE EIGHTY-
 * SEVEN STRINGS -- IT IS THE ONLY ONE.  `t_v34hshak.c` sweeps the three state
 * words over 0..86 with both debug levels raised so that every entry is
 * printed by both sides and the two transcripts compared.  Delete that sweep
 * and nothing in the tree checks this table at all.
 *
 * The names are the author's, gaps included: NOSTATE0, NOSTATE2, NOSTATE3 and
 * NOSTATE36 are placeholders, so eighty-three of the eighty-seven are real.
 * The indices are `include/dsplib/v34hshak.h`'s `V34HS_*`.
 */
static const char *StateName[V34HS_STATE_COUNT] = {
	"NOSTATE0",	"TXRENEG",	"NOSTATE2",	"NOSTATE3",
	"RECEIVE",	"SILENCE",	"ANSAM",	"TONE_2100",
	"TONE2225",	"AA_TX",	"CC_TX",	"AC_TX",
	"CA_TX",	"SXMIT",	"XMIT1",	"XMIT2",
	"XMIT3",	"XMITV22",	"SSEG",		"SBARSEG",
	"PPSEG",	"TRNSEG4",	"TRNSEG16",	"TX_JM_CM",
	"TX_DPSK",	"DET_2100",	"DET_2250",	"DET_2400",
	"DET_1200",	"DET_AC",	"DET_AC_RTN",	"DET_AC_END",
	"DET_AA",	"PHASE1",	"PHASE2",	"WAIT",
	"NOSTATE36",	"RECEIVE1",	"RECEIVE2",	"RECEIVEV22",
	"DET_CM_JM",	"DET_SYNC",	"DET_CJ",	"RX_DPSK",
	"DET_INFO",	"TONE_AB_ANS",	"TX_PHASE1_ANS","TX_PHASE2_ANS",
	"TX_PHASE3_ANS","RX_PHASE1_ANS","RX_PHASE2_ANS","TX_L1",
	"TX_L2",	"DET_AB",	"SILENCEINFO",	"TX_PHASE1_CALL",
	"TX_PHASE2_CALL","TX_PHASE3_CALL","RX_PHASE1_CALL","RX_PHASE2_CALL",
	"TONE_AB",	"TONE_AB_CALL",	"RX_PHASE3_CALL","INFODONE",
	"JTXMIT",	"XMIT0",	"TRNSEG4A",	"XMITMP",
	"J1TXMIT",	"EXMIT",	"DATAXMIT",	"TXLEVEL",
	"RX_L1",	"RX_L2",	"SILENCERETRAIN","RX_RETRAIN_CALL",
	"RX_RETRAIN_ANSWER","TX_RETRAIN_ANS","JaTXMIT","MOH_TONE",
	"MOH_TONE_DROP","MOH_SILENCE",	"MOH_ON_HOLD",	"MOH_FRR",
	"MOH_CLEARDOWN","K56JaTXMIT",	"TXMD"
};

/*
 * The three state words, and WHICH IS WHICH.
 *
 * +0x3592, +0x3594 and +0x3596 are three concurrent machines, not one, and
 * the assignment below is read off the format strings against their
 * arguments rather than guessed -- finding 171 is what guessing costs.  At
 * every one of the thirteen sites the slot holding a FIXED `StateName[k]`
 * carries the same k the site then assigns, which pins the word that is
 * changing; the two variable slots are then named by the format:
 *
 *   +0x3594 <- 4 at 0x5fd80, and 0x60118 prints "rxstate %s=>%s" with
 *              StateName[4] as the new value          => rxstate
 *   +0x3596 <- 18 at 0x5fd5a, and 0x60492 prints "txstate %s=>%s" with
 *              StateName[18] as the new value         => txstate
 *   +0x3592 <- 41 at 0x5fda6, and 0x6017b prints "microstate %s=>%s" with
 *              StateName[41] as the new value         => microstate
 *
 * and the other two slots agree in all three directions: the rxstate trace's
 * "tx %s" reads +0x3596, the txstate trace's "rx %s" reads +0x3594, and both
 * "mst %s" read +0x3592.
 *
 * A FOURTH, INDEPENDENT SIGN, which is the same argument finding 171 used to
 * settle `Uinfo`: +0x3596 only ever receives SSEG, SILENCEINFO and
 * SILENCERETRAIN, and +0x3594 only ever receives RECEIVE, WAIT and RX_DPSK.
 * Transposed, the RECEIVE machine would be the one entering SSEG.  The
 * table's own `TX_` and `RX_` prefixes say that is the wrong way round.
 */
#define HS_MICROSTATE	0x3592
#define HS_RXSTATE	0x3594
#define HS_TXSTATE	0x3596

/*
 * The two counters every trace prints as `[1]` and `[2]`.  `[1]` is the
 * second short of the pair at +0x2aa0, whose first `v34modeminit` sets to 6
 * and this function's tail sets to 0x10.
 */
#define HS_TRACE_1	0x2aa2
#define HS_TRACE_2	0xaa78

static short
hs_get(const struct v34_object *obj, unsigned off)
{
	return *(const short *)((const char *)obj + off);
}

static void
hs_put(struct v34_object *obj, unsigned off, short v)
{
	*(short *)((char *)obj + off) = v;
}

/*
 * One state transition, with its diagnostic.
 *
 * All thirteen sites are this idiom -- compare, print, assign -- and each
 * prints its own change plus the other two machines' current values, so the
 * three format strings differ only in which word they call the subject.
 *
 * THE TWO CONTEXT SLOTS ARE NOT INTERCHANGEABLE and the order below is the
 * object's: "rxstate" prints (tx, mst), "txstate" prints (rx, mst) and
 * "microstate" prints (tx, rx).  Passing the right strings in the wrong
 * order leaves every byte of the object identical, which is why the fixture
 * sweeps the three words to three DIFFERENT values and not to one.
 */
static void
hs_setstate(struct v34_object *obj, unsigned off, short next)
{
	static const char *const fmt[3] = {
		/* HS_MICROSTATE */
		"V34HSHAKE: microstate %s=>%s(tx %s, rx %s, [1]%ld, [2]%ld)\n",
		/* HS_RXSTATE */
		"V34HSHAKE: rxstate %s=>%s(tx %s, mst %s, [1]%ld, [2]%ld)\n",
		/* HS_TXSTATE */
		"V34HSHAKE: txstate %s=>%s(rx %s, mst %s, [1]%ld, [2]%ld)\n"
	};
	short now = hs_get(obj, off);

	if (now == next)
		return;

	if (DSPLIB_DEBUG_ON()) {
		const char *ctx1;
		const char *ctx2;

		if (off == HS_MICROSTATE) {
			ctx1 = StateName[hs_get(obj, HS_TXSTATE)];
			ctx2 = StateName[hs_get(obj, HS_RXSTATE)];
		} else if (off == HS_RXSTATE) {
			ctx1 = StateName[hs_get(obj, HS_TXSTATE)];
			ctx2 = StateName[hs_get(obj, HS_MICROSTATE)];
		} else {
			ctx1 = StateName[hs_get(obj, HS_RXSTATE)];
			ctx2 = StateName[hs_get(obj, HS_MICROSTATE)];
		}

		dsplibs_debug_printf(fmt[(off - HS_MICROSTATE) / 2],
				     StateName[now], StateName[next],
				     ctx1, ctx2,
				     (long)hs_get(obj, HS_TRACE_1),
				     (long)hs_get(obj, HS_TRACE_2));
	}

	hs_put(obj, off, next);
}

/*
 * Bring the handshake up in one of five modes.
 *
 * WHAT THE MODES ARE comes from the call sites, not from this function; see
 * the declaration in `v34hshak.h`.  The second argument indexes a jump table
 * at .rodata+0x2d44 whose entries for 2 and 3 are the SAME address, so there
 * are four bodies for five modes, and an out-of-range mode is not an error --
 * `ja` skips straight to the tail, which every body also falls into.
 *
 * THE OPENING BLOCK IS A TIMER, and its three fields are reached through
 * `obj + 4`: the object's code generation here is `lea 0x4(obj); mov
 * 0x234(that)`, which is +0x238 of the object and not +0x234.  Finding 179
 * quoted the register-relative offsets and is wrong by four; the fields are
 *
 *      +0x238   a running sample count
 *      +0x23c   that plus 431,488
 *      +0x244   a copy of it, taken after the guard has run
 *      +0x248   a delta, and the thing the guard tests
 *
 * The guard runs only for a positive delta, subtracts it, and accepts the
 * difference if it is at most 95,999 UNSIGNED -- so a base below the delta
 * wraps to a huge value and takes the reset arm, which is a behaviour a
 * signed comparison would get backwards.  V.34 runs at the host rate, which
 * is 9600 (`docs/rate_assumptions.md` R-1), so 95,999 is one short of ten
 * seconds and the reset value -960,000 is a hundred.  431,488 is round at
 * neither 9600 nor the 8 kHz retarget and is left unexplained; see D43.
 *
 * `VPcmV34SetV90RateReneg` writes the same three fields with the same two
 * constants through the same `obj + 4` base, which is the corroboration that
 * they are one group and that the reset arm is a reset.
 */
void
v34handshakinit(void *objp, int mode)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)((char *)obj + 0x264);
	unsigned char *m = (unsigned char *)obj;
	int delta;
	int base;

	*(int *)(m + 0x2218) = 0;

	delta = *(int *)(m + 0x248);
	if (delta > 0) {
		unsigned d = (unsigned)(*(int *)(m + 0x238) - delta);

		if (d <= 0x176ff) {
			*(int *)(m + 0x238) = (int)d;
			*(int *)(m + 0x248) = 0;
		} else {
			*(int *)(m + 0x238) = 0;
			*(int *)(m + 0x248) = (int)0xfff15a00;
		}
	}

	base = *(int *)(m + 0x238);
	*(int *)(m + 0x244) = base;
	*(int *)(m + 0x23c) = base + 0x69780;

	hs_put(obj, 0xabe6, 0);
	m[0xabe8] = 0;
	m[0xabf8] = 0;
	hs_put(obj, 0xabe4, 0);
	m[0xabfe] = 0;
	m[0xabff] = 0;

	/* txflags, which the V34RNEG diagnostic below names. */
	obj->f25c2 = (short)(obj->f25c2 & 0x7fff);

	switch (mode) {
	case 0:
		/*
		 * VPcmV34Create's mode, and the only one that re-arms the
		 * timing recovery.
		 */
		v34modeminit(obj);
		rxtiminginit(obj);

		hs_setstate(obj, HS_TXSTATE, V34HS_SILENCEINFO);
		hs_setstate(obj, HS_RXSTATE, V34HS_RX_DPSK);
		hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);

		rx->flags = (unsigned short)(rx->flags | 0x1000);
		*(short **)(m + 0xaa70) = (short *)(m + 0xa97c);
		hs_put(obj, 0xa97c + 0x18, 0x11);
		hs_put(obj, 0x25dc, 0);

		/*
		 * 0x66 and not 0x65: `f359c` is the originate/answer flag
		 * `v34modeminit` and `preinitdigital` both test against 0x65,
		 * and this one arm tests the other value.  Reproduced as
		 * written -- a sweep of {0x65, something-else} would not have
		 * told the two tests apart.
		 */
		if (obj->f359c == 0x66)
			V34SetINFO0dBits(obj, (short *)(m + 0xa97c));
		break;

	case 1:
		/*
		 * The retrain entry, and the one place two independent inputs
		 * pick between two counters: bit 6 of the receiver's flag word
		 * OR the byte at +0xac17.  EITHER of them bumps +0xac14 and
		 * clears the byte; only neither bumps +0xac12.  So the two
		 * are counters of two kinds of retrain, and which kind is
		 * which the object does not say.
		 */
		if ((rx->flags & 0x40) || m[0xac17] != 0) {
			short n = (short)(*(unsigned short *)(m + 0xac14) + 1);

			m[0xac17] = 0;
			hs_put(obj, 0xac14, n);
		} else {
			hs_put(obj, 0xac12,
			       (short)(*(unsigned short *)(m + 0xac12) + 1));
		}

		v34modeminit(obj);

		hs_setstate(obj, HS_TXSTATE, V34HS_SILENCERETRAIN);
		hs_setstate(obj, HS_RXSTATE, V34HS_WAIT);
		/* No microstate transition here; mode 1 is the only body
		 * that leaves it alone. */

		rx->flags = (unsigned short)(rx->flags | 0x1000);
		rx->agc_gain = rx->f262;
		hs_put(obj, 0x358a, 2);
		hs_put(obj, 0x3588, 2);
		break;

	case 2:
	case 3:
		/*
		 * The rate-renegotiation and hang-up entry.  Two jump-table
		 * slots, ONE body -- 3 has no caller anywhere in the object.
		 *
		 * The only body that does not call `v34modeminit`, so it is
		 * also the only one whose traces print [1] and [2] as anything
		 * but zero: the two counters are cleared below, after the
		 * three transitions rather than before them.
		 */
		hs_setstate(obj, HS_TXSTATE, V34HS_SSEG);
		hs_setstate(obj, HS_RXSTATE, V34HS_RECEIVE);
		hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);

		/*
		 * The message record at +0xaa0c, blanked.  Same twelve fields
		 * and the same 0x30-byte stride as the one `VPcmV34Set-
		 * MohMessageBits` fills at +0xa94c, which is what says the
		 * five records from +0xa94c to +0xaa3c are one array.
		 */
		{
			unsigned char *r = m + 0xaa0c;

			*(short *)(r + 0x14) = -1;
			*(short *)(r + 0x16) = 0;
			*(short *)(r + 0x18) = 0;
			*(short *)(r + 0x1a) = 0;
			*(short *)(r + 0x1c) = 0;
			*(short *)(r + 0x1e) = 0;
			*(short *)(r + 0x20) = 0;
			*(short *)(r + 0x22) = 0;
			*(int *)(r + 0x24) = 0;
			*(short *)(r + 0x28) = 0;
			*(short *)(r + 0x2a) = 0;
			*(int *)(r + 0x2c) = 0;
		}

		obj->f25c0 = 0;
		obj->f25c6 = 0;
		obj->f25cc = 0;
		obj->f25c2 = (short)((obj->f25c2 & ~0x4018) | 0x2000);

		rx->f124 = 0x21e;
		obj->f382 = (short)0x8990;
		rx->flags = (unsigned short)((rx->flags & ~0x1d8) | 0x18);

		hs_put(obj, HS_TRACE_1, 0);
		hs_put(obj, HS_TRACE_2, 0);

		preinitdigital(obj);

		rx->f218 = 0x400;
		rx->flags = (unsigned short)(rx->flags & ~0x2000);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"V34RNEG, initialize RNEG, tx->txflags= 0x%x,"
				"rx->rxflgs= 0x%x\n",
				(int)obj->f25c2, (int)rx->flags);
		break;

	case 4:
		/*
		 * Modem-on-Hold.  `VPcmV34InitMOH` is the only caller.
		 */
		m[0xabe8] = 1;
		v34modeminit(obj);
		rx->agc_gain = rx->f262;

		hs_setstate(obj, HS_TXSTATE, V34HS_SILENCERETRAIN);
		hs_setstate(obj, HS_RXSTATE, V34HS_WAIT);
		hs_setstate(obj, HS_MICROSTATE, V34HS_MOH_TONE);

		*(short **)(m + 0xaa70) = (short *)(m + 0xa97c);
		hs_put(obj, 0xa97c + 0x18, 8);
		*(short **)(m + 0xaa6c) = (short *)(m + 0xa94c);

		VPcmV34SetMohMessageBits(obj, (short *)(m + 0xa94c));

		/*
		 * RE-READ, not the pointer just stored: the object reloads
		 * +0xaa6c after the call, so the source names the field and
		 * not a local.  Nothing observable turns on it -- the callee
		 * does not write +0xaa6c -- but reproducing the read costs
		 * nothing and reproducing the wrong one might.
		 */
		{
			unsigned char *r = (unsigned char *)
					   *(short **)(m + 0xaa6c);

			*(short *)(r + 0x14) = -1;
			*(short *)(r + 0x16) = 1;
			*(short *)(r + 0x18) = 8;
			*(short *)(r + 0x1a) = 0;
			*(short *)(r + 0x1c) = 8;
			*(short *)(r + 0x1e) = 0;
			*(short *)(r + 0x20) = 0;
			*(short *)(r + 0x22) = 0;
			*(int *)(r + 0x24) = 0xf72;
			*(short *)(r + 0x28) = 0xc;
			*(short *)(r + 0x2a) = 0xc;
			*(int *)(r + 0x2c) = 0xf72;
		}

		hs_put(obj, 0x358c, 0);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"v34handshakinit: initiating MOH negotiation\n");

		/*
		 * The phase-2 signalling carrier the tone detector watches
		 * for, the other way round from `v34modeminit`'s -- which is
		 * consistent, since the two ends listen for each other.
		 */
		detectorinit((struct v34_detector *)(m + 0x3564),
			     (obj->f359c == 0x65) ? c2400_ : c1200_,
			     0, 0x64, 0x32, 0x800, 0);
		hs_put(obj, 0x356a, 1);
		hs_put(obj, 0x358a, 0);
		break;

	default:
		/* Out of range runs the tail, and nothing else. */
		break;
	}

	hs_put(obj, 0xaa3c, 0);
	hs_put(obj, 0x2aa0, 0x10);
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  Guarded to a 32-bit ABI: `struct v34_object` and
 * `struct v34_receiver` both hold pointers.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34HS_OFF(name, type, field, off) \
	typedef char v34hs_off_##name[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V34HS_OFF(fsk,     struct v34_object,   fsk,        0xaad0);
V34HS_OFF(interp,  struct v34_object,   fsk_interp, 0xaae6);
V34HS_OFF(lpf,     struct v34_object,   fsk_lpf,    0xab00);

V34HS_OFF(f128,    struct v34_receiver, f128,       0x128);
V34HS_OFF(flags,   struct v34_receiver, flags,      0x122);
V34HS_OFF(gain,    struct v34_receiver, agc_gain,   0x136);
V34HS_OFF(step,    struct v34_receiver, agc_step,   0x13a);
V34HS_OFF(rms,     struct v34_receiver, rms_buf,    0x13c);
V34HS_OFF(f19c,    struct v34_receiver, f19c,       0x19c);
V34HS_OFF(f19e,    struct v34_receiver, f19e,       0x19e);
V34HS_OFF(f1ac,    struct v34_receiver, f1ac,       0x1ac);
V34HS_OFF(f1ae,    struct v34_receiver, f1ae,       0x1ae);
V34HS_OFF(f1b0,    struct v34_receiver, f1b0,       0x1b0);
V34HS_OFF(carrier, struct v34_receiver, carrier,    0x1b4);
V34HS_OFF(f1ba,    struct v34_receiver, f1ba,       0x1ba);
V34HS_OFF(f1be,    struct v34_receiver, f1be,       0x1be);
V34HS_OFF(f262,    struct v34_receiver, f262,       0x262);
V34HS_OFF(f2a4,    struct v34_receiver, f2a4,       0x2a4);

/*
 * The clear runs to +0xab00 + 100*2 - (0xab00 - 0xaae6) = 0xabae, which is
 * inside the object.  Asserted so that a struct which grew a member into
 * that pad would break here rather than have the clear silently start
 * overwriting it.
 */
typedef char v34hs_clear_fits[
	((0xaae6 + 100 * 2) <= (int)sizeof(struct v34_object)) ? 1 : -1];

#endif
