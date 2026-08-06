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
 * WHAT IS HERE, AND WHAT IS NOT.  Fourteen functions and one table, chosen by
 * testability rather than by theme -- see docs/fastpass.md, whose one
 * unrelaxed rule is that nothing commits without a differential test:
 *
 *     dpskDetectInfo1Init   setfinalrate   setupreceiver   v34modeminit
 *     dpskinit              preempindex    v34handshakinit
 *     dftfreqinit           dftnlinitSignalBins    dftnlinitNoiseBins
 *     dftRetrainDetInit     detectRetrainReq
 *     getbit                ApplyBulkDelay
 *     StateName
 *
 * WHAT CHANGED ABOUT THE LAST TWO.  This note used to say that `getbit` and
 * `ApplyBulkDelay` could not be here at all: both are file-local, so
 * `objcopy --redefine-syms` could not give them a `ref_` alias, and the
 * project's answer for a local was to drive it through a reconstructed
 * caller -- which for these two means `v34handshak`, 61 KB that is still
 * unwritten.  0x6484c, 0x684bb and 0x706a9 call `getbit`; 0x6639f and
 * 0x66af5 call `ApplyBulkDelay`; there is not one relocation against either
 * name, because a call to a local in the same section needs none.  All of
 * that is still true and none of it blocks them any more.
 *
 * The claim that failed was the one about `objcopy`.  A local can be
 * PROMOTED first -- `--globalize-symbols` in one pass, `--redefine-syms` in a
 * second -- and then it renames like any other global, so `ref_getbit` and
 * `ref_ApplyBulkDelay` link and both are tested directly rather than through
 * a caller.  Finding 221 is the general result and finding 227 is these two.
 * Finding 117, which drew the original conclusion, stands as the reading of
 * the call graph and is superseded only in what it says can be tested.
 *
 * NINE OF THE TWELVE ARE CALLED BY NOTHING IN THE OBJECT.  Nothing reaches
 * `dpskDetectInfo1Init`, `dpskinit`, `setupreceiver`, `preempindex` or any
 * of the five DFT routines -- no relocation, no `call`, no `jmp`, and no
 * little-endian copy of their addresses in `.text`, `.data` or `.rodata`.
 * Only `setfinalrate` has a caller, in `v34handshak`.  The `jmp` and the
 * data scans are not belt and braces: this object tail-calls constantly, and
 * a section-symbol relocation with an addend does not answer to a grep for a
 * name.  Finding 212 gives the controls each scan was checked against.
 *
 * They are global, so they are testable regardless, but it means their
 * arguments and their bank sizes have to be read out of the code rather than
 * off a call site.  Finding 89 recorded the same shape twice already.
 *
 * `v34handshakinit` is the exception and has six callers, which is where its
 * mode numbers come from; see the declaration in `v34hshak.h`.
 */

#include <stdlib.h>		/* abort, in t3c_unwritten below */

#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
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
 * The DFT banks, and the retrain-request detector.
 *
 * Five functions over `struct v34_dftbin`, and the reason they are grouped
 * here rather than in dftc.c is link order: they sit between `V34scrambler`
 * and `getbit`, inside V34hshak.c's extent, while `dftupdate` and
 * `dftenergy` are in DFTC.c a hundred kilobytes further on.  The bank is
 * DFTC.c's; deciding what a bank measures is the handshake's.
 *
 * WHAT ONE BIN UNIT IS.  A bin's `inc` is its phase step per sample, the
 * accumulator is 14 bits, so bin `n` has period 16384/(n*256) = 64/n
 * samples.  At the 9600 Hz rate V.34 runs at -- docs/rate_assumptions.md
 * R-1, which the object does not state and this file does not depend on --
 * that is n * 150 Hz, and 150 Hz is the V.34 line probe's tone spacing.
 * Every bin number below is written as `<< 8` for that reason: the shift is
 * the object's own encoding of "bin n", not a scale factor picked here.
 */

/* The bin numbers, as the object writes them: the step is the bin times 256. */
#define DFT_BIN(n)	((short)((n) << 8))

/*
 * The line probe's twenty-five bins, 150 Hz to 3750 Hz.
 *
 * The only one of the three initialisers that clears the double
 * accumulators as well as the integer pair.  Whether that is deliberate or
 * an omission in the other two is not recoverable -- see the note in
 * v34hshak.h -- but it is reproduced either way, because the difference is
 * visible in memory and a test compares memory.
 */
void
dftfreqinit(struct v34_dftbin *bins)
{
	short i;

	/*
	 * ONE-BASED, and twenty-five iterations rather than twenty-six: the
	 * object starts the counter at 1, uses it as the bin number, and
	 * tests the INCREMENTED value against 25.  So bins[0] gets bin 1 and
	 * the last entry written is bins[24] with bin 25.  There is no bin 0
	 * -- a step of zero is DC.
	 */
	for (i = 1; i <= 25; i++, bins++) {
		bins->phase = 0;
		bins->inc = DFT_BIN(i);
		bins->acc_re = 0;
		bins->acc_im = 0;
		bins->sum_re = 0.0;
		bins->sum_im = 0.0;
		bins->denergy = 0.0;
	}
}

/*
 * The four bins the nonlinear-distortion measurement treats as signal:
 * 1050, 1350, 1950 and 2550 Hz.
 */
void
dftnlinitSignalBins(struct v34_dftbin *bins)
{
	short i;

	/*
	 * `inc` is cleared in the loop and then written again below.  That is
	 * the object's -- the loop zeroes all four fields uniformly and the
	 * frequencies are assigned afterwards -- and folding the two would
	 * lose the fact that the loop is the same loop as the noise bank's.
	 */
	for (i = 0; i <= 3; i++) {
		bins[i].phase = 0;
		bins[i].inc = 0;
		bins[i].acc_re = 0;
		bins[i].acc_im = 0;
	}

	bins[0].inc = DFT_BIN(7);
	bins[1].inc = DFT_BIN(9);
	bins[2].inc = DFT_BIN(13);
	bins[3].inc = DFT_BIN(17);
}

/*
 * And the four it treats as noise: 900, 1200, 1800 and 2400 Hz, each one
 * step below its partner above.
 */
void
dftnlinitNoiseBins(struct v34_dftbin *bins)
{
	short i;

	for (i = 0; i <= 3; i++) {
		bins[i].phase = 0;
		bins[i].inc = 0;
		bins[i].acc_re = 0;
		bins[i].acc_im = 0;
	}

	bins[0].inc = DFT_BIN(6);
	bins[1].inc = DFT_BIN(8);
	bins[2].inc = DFT_BIN(12);
	bins[3].inc = DFT_BIN(16);
}

/*
 * The retrain detector's two thresholds and three run limits.
 *
 * Both thresholds go on all three bins even though only bin 0's `thresh_lo`
 * and bin 1's `thresh_hi` are ever compared against -- bins 1 and 2 also
 * have their `thresh_lo` read, bin 2's `thresh_hi` never is.  Written on all
 * three because that is what the object does; the loop does not know which
 * arm will read which.
 */
#define RETRAIN_QUIET	0x50	/* +0x28: "this bin has gone silent"    */
#define RETRAIN_TONE	0xbb8	/* +0x2a: "and now something is there"  */

void
dftRetrainDetInit(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_dftbin *bins = obj->retrain_bins;
	short i;

	for (i = 0; i <= 2; i++) {
		bins[i].thresh_lo = RETRAIN_QUIET;
		bins[i].thresh_hi = RETRAIN_TONE;
		bins[i].phase = 0;
		bins[i].acc_re = 0;
		bins[i].acc_im = 0;
	}

	/* 900, 1200 and 1500 Hz. */
	bins[0].inc = DFT_BIN(6);
	bins[1].inc = DFT_BIN(8);
	bins[2].inc = DFT_BIN(10);

	obj->retrain_state = 1;
	obj->retrain_phase = 0;
	obj->retrain_runs = 0;
	obj->retrain_quiet_runs = 3;
	obj->retrain_tone_runs = 9;
}

/*
 * Poll it.
 *
 * The measurement is taken once every 128 samples and the counter advances
 * four at a time, so 32 calls of four samples -- or any other split, since
 * the counter is samples and not calls -- produce one decision.
 *
 * THE ENERGY IS WIDENED UNSIGNED AND THE THRESHOLD SIGNED.  The object does
 * `movzwl` on one side of each of these four comparisons and `movswl` on the
 * other, and neither cast is decoration -- both are observable and both are
 * driven by t_v34hshak.c.
 *
 * `energy` is a `short` that `dftenergy` writes as `(short)((int)e >> 16)`,
 * and it goes negative only when the two squares making up `e` are both at
 * full scale.  No sample sequence reaches that -- a tone puts nearly all of
 * its correlation on one axis -- but a seeded accumulator does: 0x04000000
 * in both halves gives exactly -32768, which read unsigned is 32768, and the
 * two spellings then disagree about every threshold between them.  Finding
 * 212, which records the sweep that missed this and why it missed it.
 */
int
detectRetrainReq(void *objp, short nbins, const short *samples, short nsamples)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_dftbin *bins = obj->retrain_bins;
	short runs;
	short i;

	dftupdate(bins, nbins, samples, nsamples);

	/*
	 * EQUALITY, not `>=`.  A caller that arrived with a sample count
	 * indivisible by four would step past 128 and never measure again;
	 * the object is written on the assumption that it does not, and the
	 * receive queue is drained four at a time everywhere.
	 */
	obj->retrain_phase += 4;
	if (obj->retrain_phase != 0x80)
		return 0;
	obj->retrain_phase = 0;

	dftenergy(bins, nbins, 5);

	/*
	 * Clear for the next window -- the integer path only.  The two double
	 * accumulators are NOT reset, so `denergy` integrates over the whole
	 * life of the detector while `energy` is per window.  Nothing reads
	 * `denergy`; see docs/findings.md.
	 */
	for (i = 0; i < nbins; i++) {
		bins[i].phase = 0;
		bins[i].acc_re = 0;
		bins[i].acc_im = 0;
	}

	if (obj->retrain_state == 1) {
		/*
		 * Waiting for silence on all three bins at once.  Any one of
		 * them still loud ends the run -- and if the run that just
		 * ended was long enough, that is the transition to state 2.
		 *
		 * So the arm that ADVANCES the machine is the one where the
		 * quiet test FAILS, which reads backwards until you notice
		 * that the run has to end before its length can be judged.
		 */
		if ((int)(unsigned short)bins[0].energy
					< (int)bins[0].thresh_lo
		    && (int)(unsigned short)bins[1].energy
					< (int)bins[1].thresh_lo
		    && (int)(unsigned short)bins[2].energy
					< (int)bins[2].thresh_lo) {
			runs = (short)(obj->retrain_runs + 1);
		} else {
			if (obj->retrain_runs >= obj->retrain_quiet_runs)
				obj->retrain_state = 2;
			runs = 0;
		}
		obj->retrain_runs = runs;
		return 0;
	}

	if (obj->retrain_state != 2)
		return 0;

	/*
	 * Waiting for the middle bin -- 1200 Hz -- to come back.
	 */
	if ((int)(unsigned short)bins[1].energy > (int)bins[1].thresh_hi) {
		obj->retrain_runs = (short)(obj->retrain_runs + 1);
	} else {
		obj->retrain_runs = 0;
		obj->retrain_state = 1;
	}

	/*
	 * ONE TAIL FOR BOTH ARMS, which matters: the reset arm falls into
	 * this comparison too, so a `retrain_tone_runs` of zero would report
	 * a retrain on the call that gave up.  It is 9, so it does not; the
	 * shared tail is reproduced rather than tidied into the first arm
	 * because that behaviour is the object's and a caller could change
	 * the limit.
	 */
	return obj->retrain_runs == obj->retrain_tone_runs;
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
 *
 * Reconstructed under the fast pass (docs/fastpass.md).  V34hshak.c is 118 KB
 * and `v34handshak` alone is 61 KB of it; these two are the leaves at the far
 * end, and they are here because they are the transmit half of a pair whose
 * other half is already written.
 *
 * WHAT THEY DO.  Both take a small field of bits, scramble it, map it
 * DIFFERENTIALLY onto the constellation, and hand the symbol to `txmit`.
 * They differ only in how many bits and which constellation:
 *
 *     txmitdibit    2 bits -> one quadrant of `vect4`,  added to the last
 *     txmitquadbit  4 bits -> `vect16`, as (quadrant, dibit) with only the
 *                             FIRST dibit differentially encoded
 *
 * Differential encoding is what makes the handshake survive a 180-degree
 * phase ambiguity: the receiver never has to know which way up the
 * constellation is, only how far it turned since the last symbol.  That is
 * why the quadrant is carried in the object rather than recomputed, and why
 * the second dibit of a quadbit is NOT added to anything -- the quadrant is
 * already differential, so encoding the offset within it twice would undo it.
 *
 * BOTH END IN A TAIL CALL to `txmit`, which is the whole transmit chain:
 * modulate, enqueue, pre-filter, feed the echo cancellers.  So these are the
 * handshake's entry into the datapump, not helpers beside it.
 *
 * The scrambler is not transcribed here.  The object inlines it -- twice in
 * txmitquadbit -- but it is `V34scrambler`, which v34rx.c already has, and
 * the taps prove it: 1<<26 against 1<<8 for the calling station and 1<<13
 * against 1<<8 for the answering one, which are V.34's 1 + x^-5 + x^-23 and
 * 1 + x^-18 + x^-23.  Its return value for a two-bit request is `(reg >> 29)
 * & 3`, which is exactly the expression the object forms by hand.  Calling it
 * is therefore the same function, and the differential test below is what
 * says so rather than the reading.
 *
 * The two handshake constellations, as packed complex ints: the real part in
 * the low half and the imaginary in the high, which is the form `txmit` reads
 * them back in and the reason a single 32-bit store lands both.
 */

/*
 * vect4 -- the four quadrant points, all at (+-4579, +-4579), in the order
 * (+,+) (+,-) (-,-) (-,+).  That is CLOCKWISE, so adding one to the quadrant
 * index turns the point by -90 degrees, not +90.
 */
const int vect4[4] = {
	300093923, -300084765, -300028387, 300150301,
};

/*
 * vect16 -- sixteen points as four quadrants of four, indexed by
 * (quadrant << 2) | dibit.  Same magnitudes throughout; this is V.34's
 * 16-point handshake constellation, not a data-mode one.
 */
const int vect16[16] = {
	134219776, 134277120, -402651136, -402593792,
	-134215680, 402655232, -134158336, 402712576,
	-134154240, -134211584, 402716672, 402659328,
	134281216, -402589696, 134223872, -402647040,
};

/*
 * The scrambler generator, from the flag the two share.  Bit 0 of f25c2 set
 * selects the calling station's polynomial, which is V34scrambler's mode 0 --
 * so the sense is inverted between the two, and that is the object's.
 */
static short
tx_scrambler_mode(const struct v34_object *o)
{
	return (short)((o->f25c2 & 1) == 0);
}

/*
 * Scramble two bits and turn them into a quadrant, differentially.
 *
 * The sum is taken modulo four with no regard for the previous quadrant's
 * sign, because it is stored back masked and so is never anything but 0..3.
 */
void
txmitdibit(void *obj, short bits)
{
	struct v34_object *o = (struct v34_object *)obj;
	unsigned sr = (unsigned)o->f25cc;
	int d, q;

	d = V34scrambler(&sr, tx_scrambler_mode(o), bits, 2);
	o->f25cc = (int)sr;

	q = (d + (unsigned short)o->f25c6) & 3;

	*(int *)&o->f25d0 = vect4[q];
	o->f25c8 = (short)q;
	o->f25c6 = (short)q;

	txmit(obj);
}

/*
 * Scramble four bits and turn them into one of sixteen points.
 *
 * The two dibits go through the scrambler as two separate two-bit requests
 * rather than as one four-bit one -- which matters, because the register is
 * written back between them and the second request reads it.  Only the first
 * is differentially encoded; the second selects within the quadrant the first
 * chose, and `f25c6` catches up to `f25c8` only at the end.
 */
void
txmitquadbit(void *obj, short bits)
{
	struct v34_object *o = (struct v34_object *)obj;
	short mode = tx_scrambler_mode(o);
	unsigned sr = (unsigned)o->f25cc;
	int d, q;

	d = V34scrambler(&sr, mode, bits, 2);
	o->f25cc = (int)sr;
	q = (d + (unsigned short)o->f25c6) & 3;
	o->f25c8 = (short)q;

	sr = (unsigned)o->f25cc;
	d = V34scrambler(&sr, mode, (short)(bits >> 2), 2);
	o->f25cc = (int)sr;

	q = (unsigned short)o->f25c8;
	o->f25c6 = (short)q;
	*(int *)&o->f25d0 = vect16[d + q * 4];

	txmit(obj);
}

/*
 * ---------------------------------------------------------------------------
 * The line probe's verdict.
 *
 * `probeselect` reads the twenty-five probe bins and decides three things:
 * how much transmit power to ask the far end for, which symbol rates to offer
 * or to choose, and what pre-emphasis each of them wants.  It takes the
 * object alone and returns nothing; everything it does lands in two places.
 *
 * THE TWO PLACES, AND WHY THEY MATTER TOGETHER.  The rate config at +0xaa84
 * is the same eight fields `setfinalrate` reads back, and the message it
 * builds at +0xa9ac is two shorts below the three `setfinalrate` unpacks its
 * rate fields out of -- so the two are an encode/decode pair over one message
 * and can be tested against each other rather than only against the blob.
 *
 * THE ROLE FLAG SPLITS IT IN TWO.  With `f359c == 0x65` this is the
 * originating side, and it walks the whole ladder ACCUMULATING message bits:
 * every rate the probe allows is offered, and each arm falls through into the
 * next test.  Otherwise it is answering, and it takes the first rate that
 * both the probe and the far end's capability bits allow, fills the rate
 * config in and returns.  That is why the two halves of one rate look so
 * different: one proposes and one decides.
 *
 * FIVE RATES, AND 2743 IS NOT ONE OF THEM.  The same gap `chkForceBaudRate`
 * has, where `allow[0]` and `allow[1]` are written and never read.  Neither
 * function says why.
 */

/* Which bin the pre-emphasis search measures everything against. */
#define PROBE_REF_BIN	4

/*
 * How much pre-emphasis one rate wants.
 *
 * The candidate bin's energy is scaled down by a per-rate factor -- all five
 * are just under 1.0 in Q14 -- until it falls below the reference bin's, and
 * the number of steps that took is the index.  A band edge that started far
 * above bin 4 needs more pre-emphasis, so this is a logarithm taken by
 * repeated multiplication.
 *
 * THE COUNTER IS ADVANCED BEFORE THE TEST -- `lea 0x1(%ebx),%esi; movswl
 * %si,%ebx` sits ahead of `cmp %cx,%dx` -- so it is 6..10 at either exit and
 * the `i == 5` arm cannot be taken.  It is reproduced because the object has
 * it, five times over, and D53 records that its string is unreachable.
 *
 * INDEX 10 IS REACHED BY BOTH EXITS AND REPORTED DIFFERENTLY: "index is %d"
 * when the energy dropped on the tenth step, "index is 10" when it never
 * dropped at all.  The rate config gets 10 either way, so nothing but the
 * transcript tells them apart.
 */
static short
probe_preemph(const struct v34_dftbin *bins, unsigned n, int k, short baud)
{
	short ref = bins[PROBE_REF_BIN].energy;
	short x = bins[n].energy;
	short i = 5;

	for (;;) {
		x = (short)(((int)x * k) >> 14);
		i = (short)(i + 1);

		if (x > ref) {
			if (i == 5) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V34PREEMPHASIS, - index is 0, "
					    "baudrate= %d\n", (int)baud);
				return 0;
			}
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V34PREEMPHASIS, - index is %d, "
				    "baudrate= %d\n", (int)i, (int)baud);
			return i;
		}
		if (i > 9) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V34PREEMPHASIS, - index is 10, "
				    "baudrate= %d \n", (int)baud);
			return i;
		}
	}
}

/* `msg[i] |= bits`, read back unsigned as the object reads it. */
static void
mp_or(short *msg, unsigned i, unsigned bits)
{
	msg[i] = (short)((unsigned)(unsigned short)msg[i] | bits);
}

/* The tail four of the five rates share: the index, reversed, split in two. */
static void
mp_put_preemph(short *msg, short idx)
{
	unsigned rev = (unsigned short)bitreverse((unsigned short)idx, 4);

	mp_or(msg, 1, rev >> 2);
	mp_or(msg, 2, (rev << 6) & 0xff);
}

/*
 * Scale the AGC's starting gain up by one dB `n` times.
 *
 * TRUNCATED TO A SHORT EVERY ITERATION -- the object keeps the running value
 * in a register and re-reads it with `movswl %cx,%edx` at the top of each
 * pass -- and stored once at the end.
 */
static void
probe_backoff(struct v34_receiver *rx, int n)
{
	short g = rx->f262;
	int i;

	for (i = 0; i < n; i++)
		g = (short)(((int)g * 0x47cf + 0x2000) >> 14);

	rx->f262 = g;
}

/*
 * The two flat requests the "sensitive ISP" arms make.
 *
 * Neither consults the AGC or the computed reduction: a small enough `snr_l1`
 * asks for the maximum and that is that.  `second` is -1 when the message
 * carries only the first field.
 */
static void
probe_ask(struct v34_receiver *rx, short *msg, int first, int second, int n)
{
	mp_or(msg, 0, (unsigned)(unsigned short)
		      bitreverse((unsigned short)first, 3) << 5);
	if (second >= 0)
		mp_or(msg, 0, (unsigned)(unsigned short)
			      bitreverse((unsigned short)second, 3) << 2);

	probe_backoff(rx, n);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PROBE, asking for a power reduction "
				     "of %d\n", n);
}

void
probeselect(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	struct v34_dftbin *bins = obj->probe_bins;
	struct v34_receiver *rx = (struct v34_receiver *)(m + 0x264);
	short *msg = (short *)(m + 0xa9ac);
	const unsigned char *pcfg = (const unsigned char *)obj->pac3c;

	/*
	 * The rate config, by the names `setfinalrate` gives the same eight
	 * fields, plus the five per-rate pre-emphasis slots -- which are the
	 * only thing here that function does not also write.
	 */
	short *tx_baud		= (short *)(m + 0xaa84);
	short *tx_preemp	= (short *)(m + 0xaa8a);
	const short **tx_scale	= (const short **)(m + 0xaa90);
	short *tx_carrier	= (short *)(m + 0xaa94);
	short *rx_baud		= (short *)(m + 0xaa96);
	short *rx_carrier	= (short *)(m + 0xaaa8);
	const short **rx_scale	= (const short **)(m + 0xaaac);
	const short **rx_cdesc	= (const short **)(m + 0xaab0);
	short *pe2400		= (short *)(m + 0xaa9a);
	short *pe2800		= (short *)(m + 0xaa9c);
	short *pe3000		= (short *)(m + 0xaaa0);
	short *pe3200		= (short *)(m + 0xaaa2);
	short *pe3429		= (short *)(m + 0xaaa4);

	int snr_l1, snr_l2;
	unsigned ratio = 0;
	unsigned short gain;
	short dbcnt, req;
	short role, e22, e20;
	int minshift;
	short idx;
	unsigned low, high;
	int i;

	for (i = 0; i <= 9; i++)
		msg[i] = 0;

	/*
	 * --- the L2/L1 ratio ----------------------------------------------
	 *
	 * A fixed-point divide with the numerator pre-shifted as far left as
	 * it will go: the loop finds the highest set bit of `snr_l2` among
	 * bits 30..21, and the denominator is shifted down by the same amount,
	 * so the quotient lands in the same place whatever the magnitudes
	 * were.  Bit 31 set means no shift at all; nothing found in ten bits
	 * means the full ten, and that case reaches the same expression by a
	 * different path in the object.
	 */
	snr_l1 = *(int *)(m + 0xaac4);
	snr_l2 = *(int *)(m + 0xaac8);

	if (snr_l1 != 0) {
		short b = 0;
		unsigned den;

		if (snr_l2 >= 0) {
			do {
				b = (short)(b + 1);
			} while (((unsigned)snr_l2 & (0x80000000u >> b)) == 0
				 && b <= 9);
		}

		den = (unsigned)snr_l1 >> (10 - b);
		if (den != 0)
			ratio = (((unsigned)snr_l2 << b)
				 + ((unsigned)snr_l1 >> (11 - b))) / den;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PROBE, snr_L1=%d , snr_L2=%d , "
				     "L2toL1ratio=%d  (all not in dB)\n",
				     snr_l1, snr_l2, (int)ratio);

	/*
	 * --- and how many dB that is --------------------------------------
	 *
	 * 0x509 >> 10 is 1.2588, one dB of power, and the count is how many of
	 * them it takes to pass the ratio.  Above 0x18fff the loop is not run
	 * at all and the answer is 1000, which is the object's own bound
	 * rather than a saturation of anything downstream.
	 */
	if (ratio > 0x18fff) {
		dbcnt = 1000;
	} else {
		unsigned t = 0x47d;

		dbcnt = 0;
		while (t < ratio) {
			t = (t * 0x509 + 0x200) >> 10;
			dbcnt = (short)(dbcnt + 1);
		}
	}

	/*
	 * --- the reduction that many dB asks for --------------------------
	 *
	 * `0x640000 / gain` is the AGC gain's reciprocal in Q14, and
	 * multiplying by the dB count turns "the far signal is N dB too
	 * strong" into a request.  The clamp is UNSIGNED, so a negative gain
	 * -- which the signed divide above can produce -- lands on the ceiling
	 * rather than passing through as a huge reduction.
	 */
	gain = (unsigned short)rx->agc_gain;
	req = 0;
	if (gain != 0) {
		int g = (short)gain;
		int est = (g / 2 + 0x640000) / g;

		if ((unsigned)est > 0x4000)
			est = 0x4000;
		req = (short)(((unsigned)est * (unsigned)(int)dbcnt) >> 14);
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PROBE, dBcnt=%d , "
				     "powerReductionReq=%d , gain=%d\n",
				     (int)dbcnt, (int)req, (int)(short)gain);

	/*
	 * --- and whether to ask for it ------------------------------------
	 *
	 * Three ways to end up asking and they do not agree on how much.  The
	 * two "sensitive ISP" arms are flat -- a small enough `snr_l1` asks
	 * for the maximum whatever the AGC says -- and the ordinary arm asks
	 * for what was computed, capped at 7 dB and only while the AGC still
	 * has room.
	 *
	 * BOTH FLAT ARMS FALL INTO THE "NO REDUCTION" TAIL, so a run that has
	 * just asked for 7 or 9 dB then prints "not asking for power
	 * reduction".  D52.
	 */
	if ((pcfg[0x50] & 0x10) == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Sensitive RX Power Reduction "
					     "mechanism disabled!\r\n");
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Sensitive RX Power Reduction "
					     "mechanism enabled!\r\n");

		if ((unsigned)snr_l1 <= 0x1f3 && (m[0xa97e] & 0x80)) {
			probe_ask(rx, msg, 7, 2, 9);
			goto not_asking;
		}
		if ((unsigned)snr_l1 <= 0x3e7 && (m[0xa97e] & 0x80)) {
			probe_ask(rx, msg, 7, -1, 7);
			goto not_asking;
		}
	}

	if ((m[0xa97e] & 0x80) == 0)
		goto not_asking;
	if (!(rx->agc_gain <= 0xfff && req != 0))
		goto not_asking;

	if (req > 7)
		req = 7;

	mp_or(msg, 0, (unsigned)(unsigned short)
		      bitreverse((unsigned short)req, 3) << 5);

	if (DSPLIB_DEBUG_ON()) {
		dsplibs_debug_printf("V34PROBE, asking for a power reduction "
				     "of %d\n", (int)req);
		dsplibs_debug_printf("V34PROBE, agc gainestimate of L1 signal "
				     "is %d\n", (int)rx->f262);
	}

	probe_backoff(rx, req);

	if (rx->f262 > 0x1b58)
		rx->f262 = 0x1b58;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PROBE, agc gainestimate due to power "
				     "reduction request is %d\n",
				     (int)rx->f262);
	goto band_edges;

not_asking:
	if (DSPLIB_DEBUG_ON()) {
		dsplibs_debug_printf("V34PROBE, not asking for power "
				     "reduction\n");
		dsplibs_debug_printf("V34PROBE, rx->gain=%d ,"
				     "(obj->rxinfo0.data[1]&0x80)=%d\n",
				     (int)rx->agc_gain,
				     (int)(m[0xa97e] & 0x80));
	}

band_edges:
	/*
	 * --- normalise the bank -------------------------------------------
	 *
	 * Subtract the smallest `shift` from every one, so what is left is
	 * each bin's level relative to the quietest.  THE MINIMUM IS COMPARED
	 * SIGNED AND KEPT UNSIGNED -- `movswl` on one side and `movzwl` on the
	 * assignment -- so a single negative shift makes the minimum a large
	 * positive number and every subtraction after it runs the other way.
	 * D54.
	 */
	minshift = 0x20;
	for (i = 0; i <= 24; i++)
		if ((int)bins[i].shift < minshift)
			minshift = (unsigned short)bins[i].shift;
	for (i = 0; i <= 24; i++)
		bins[i].shift = (short)((unsigned)(unsigned short)bins[i].shift
					- (unsigned)minshift);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V34PROBE,0=%d,%d,%d,%d,%d,5=%d,%d,%d,%d,%d,10=%d,%d,%d,"
		    "%d,%d,%d,%d,%d,%d,%d,20=%d,%d,22=%d,%d,24=%d\n",
		    (int)bins[0].shift, (int)bins[1].shift, (int)bins[2].shift,
		    (int)bins[3].shift, (int)bins[4].shift, (int)bins[5].shift,
		    (int)bins[6].shift, (int)bins[7].shift, (int)bins[8].shift,
		    (int)bins[9].shift, (int)bins[10].shift,
		    (int)bins[11].shift, (int)bins[12].shift,
		    (int)bins[13].shift, (int)bins[14].shift,
		    (int)bins[15].shift, (int)bins[16].shift,
		    (int)bins[17].shift, (int)bins[18].shift,
		    (int)bins[19].shift, (int)bins[20].shift,
		    (int)bins[21].shift, (int)bins[22].shift,
		    (int)bins[23].shift, (int)bins[24].shift);

	/*
	 * --- and pull the top of the band down if the edges are too strong -
	 *
	 * Four low bins against four high ones.  More than four times and the
	 * upper octave loses two steps, more than twice and it loses one.  A
	 * zero low sum is forced to one rather than skipping the test, so the
	 * comparison is always against at least four.  Both sums are truncated
	 * to sixteen bits before the comparison; each term was zero-extended.
	 */
	low = (unsigned short)(bins[2].shift + bins[3].shift
			       + bins[4].shift + bins[6].shift);
	high = (unsigned short)(bins[12].shift + bins[13].shift
				+ bins[14].shift + bins[16].shift);
	if (low == 0)
		low = 1;

	if ((int)high > (int)(low * 4)) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34PROBE, min = %d, i= %d, so "
					     "reducing band edge norm by 2\n",
					     (int)high, (int)low);
		for (i = 16; i <= 23; i++)
			if (bins[i].shift > 2)
				bins[i].shift = (short)(bins[i].shift - 2);
	} else if ((int)high > (int)(low * 2)) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34PROBE, min = %d, i= %d, so "
					     "reducing band edge norm by 1\n",
					     (int)high, (int)low);
		for (i = 16; i <= 23; i++)
			if (bins[i].shift > 1)
				bins[i].shift = (short)(bins[i].shift - 1);
	}

	chkForceBaudRate(obj, bins);

	/*
	 * --- and now the ladder --------------------------------------------
	 *
	 * Written with the object's own labels rather than as nested `if`s.
	 * The two are not the same shape: the originating arms FALL THROUGH
	 * into the next rate's test and the answering arms return, and the
	 * re-entry points differ between them -- 3200's originating arm comes
	 * back below its first band-edge test and 3000's comes back above its
	 * own.  Nesting that would have to duplicate a test.
	 */
	role = *(short *)(m + 0x359c);

	if (*(short *)(m + 0x359a) != 0)
		goto rate_2400;

	if (bins[24].shift > 10)
		goto rate_3200;
	if (bins[23].shift > 6)
		goto rate_3200;
	if (bins[1].shift > 5)
		goto rate_3200;

	if (role == 0x65) {
		mp_or(msg, 8, 0xe0);
		idx = probe_preemph(bins, 22, 0x6626, 0xd65);
		*pe3429 = idx;
		mp_or(msg, 7, (unsigned)(unsigned short)
			      bitreverse((unsigned short)idx, 4) << 1);
		goto rate_3200;
	}
	if ((m[0xa9ea] & 1) == 0 && (m[0xa9ec] & 0xe0) == 0)
		goto rate_3200;

	*tx_baud = 0xd65;
	*tx_carrier = 0x7a7;
	*tx_scale = scale3429;
	*tx_preemp = (short)bitreverse((unsigned short)
				       ((*(unsigned short *)(m + 0xa9ea) >> 1)
					& 0xf), 4);
	*rx_scale = scale3429;
	*rx_cdesc = c1959;
	mp_or(msg, 2, 2);
	mp_or(msg, 3, 0xd0);
	*rx_baud = 0xd65;
	*rx_carrier = 0x7a7;
	mp_or(msg, 2, 0x24);
	idx = probe_preemph(bins, 22, 0x6626, 0xd65);
	*pe3429 = idx;
	mp_put_preemph(msg, idx);
	return;

rate_3200:
	e22 = bins[22].shift;
	if (e22 > 6)
		goto rate_3000;
	if (bins[1].shift > 6)
		goto rate_3200_alt;

	if (role == 0x65) {
		if (bins[23].shift <= 5)
			mp_or(msg, 6, 0x40);
		if (bins[23].shift > 7) {
			mp_or(msg, 6, 3);
			mp_or(msg, 7, 0x40);
		} else {
			mp_or(msg, 6, 2);
			mp_or(msg, 7, 0xc0);
		}
		idx = probe_preemph(bins, 20, 0x639f, 0xc80);
		*pe3200 = idx;
		mp_or(msg, 6, (unsigned)(unsigned short)
			      bitreverse((unsigned short)idx, 4) << 2);
		goto rate_3200_alt;
	}
	if ((m[0xa9e8] & 3) != 0 || (m[0xa9ea] & 0xc0) != 0) {
		*tx_baud = 0xc80;
		*tx_scale = scale3200;
		*tx_carrier = (short)((m[0xa9e8] & 0x40) ? 0x780 : 0x725);
		*tx_preemp = (short)
			bitreverse((unsigned short)
				   ((*(unsigned short *)(m + 0xa9e8) >> 2)
				    & 0xf), 4);
		*rx_scale = scale3200;
		mp_or(msg, 3, 0x90);
		*rx_baud = 0xc80;
		if (bins[23].shift > 5) {
			*rx_carrier = 0x725;
			*rx_cdesc = c1829;
		} else {
			mp_or(msg, 1, 4);
			*rx_cdesc = c1920;
			*rx_carrier = 0x780;
		}
		mp_or(msg, 2, 0x24);
		idx = probe_preemph(bins, 20, 0x639f, 0xc80);
		*pe3200 = idx;
		mp_put_preemph(msg, idx);
		return;
	}

rate_3200_alt:
	e22 = bins[22].shift;
	if (e22 > 6)
		goto rate_3000;
	if (bins[2].shift > 6)
		goto rate_3000;
	goto rate_3000_body;

rate_3000:
	if (bins[21].shift > 6)
		goto rate_2800;
	if (bins[1].shift > 6)
		goto rate_2800;

rate_3000_body:
	if (role == 0x65) {
		short e = bins[22].shift;

		if (e <= 5 && bins[2].shift <= 5)
			mp_or(msg, 5, 0x80);
		if (e <= 7)
			mp_or(msg, 5, (unsigned)((e <= 6) ? 1u : 2u));
		else
			mp_or(msg, 5, 4);
		mp_or(msg, 6, 0x80);

		idx = probe_preemph(bins, 19, 0x656f, 0xbb8);
		*pe3000 = idx;
		mp_or(msg, 5, (unsigned)(unsigned short)
			      bitreverse((unsigned short)idx, 4) << 3);
		goto rate_2800;
	}
	if ((m[0xa9e6] & 7) != 0 || (m[0xa9e8] & 0x80) != 0) {
		*tx_baud = 0xbb8;
		*tx_scale = scale3000;
		*tx_carrier = (short)((m[0xa9e6] & 0x80) ? 0x7d0 : 0x708);
		*tx_preemp = (short)
			bitreverse((unsigned short)
				   ((*(unsigned short *)(m + 0xa9e6) >> 3)
				    & 0xf), 4);
		*rx_scale = scale3000;
		mp_or(msg, 2, 3);
		mp_or(msg, 3, 0x60);
		*rx_baud = 0xbb8;
		if (bins[22].shift > 5 || bins[2].shift > 5) {
			*rx_carrier = 0x708;
			*rx_cdesc = c1800_;
		} else {
			*rx_cdesc = c2000;
			*rx_carrier = 0x7d0;
			mp_or(msg, 1, 4);
		}
		mp_or(msg, 2, 0x24);
		idx = probe_preemph(bins, 19, 0x656f, 0xbb8);
		*pe3000 = idx;
		mp_put_preemph(msg, idx);
		return;
	}

rate_2800:
	e20 = bins[20].shift;
	if (e20 > 6)
		goto rate_2800_alt;
	if (bins[2].shift > 6)
		goto rate_2800_alt;
	goto rate_2800_body;

rate_2800_alt:
	if (bins[19].shift > 6)
		goto rate_2400;
	if (bins[1].shift > 6)
		goto rate_2400;

rate_2800_body:
	if (role == 0x65) {
		/*
		 * bins[20], not bins[22]: the register the object compares
		 * here was last loaded at the top of this rate's band-edge
		 * test, and the 3000 arm above -- which looks identical --
		 * is the one that still holds bins[22].
		 */
		if (bins[20].shift <= 5 && bins[2].shift <= 5)
			mp_or(msg, 3, 1);
		mp_or(msg, 4, 0xd);
		idx = probe_preemph(bins, 18, 0x6789, 0xaf0);
		*pe2800 = idx;
		mp_or(msg, 4,
		      ((unsigned)(unsigned short)
		       bitreverse((unsigned short)idx, 4) & 0xf) << 4);
		goto rate_2400;
	}
	if ((m[0xa9e4] & 0xf) == 0)
		goto rate_2400;

	*tx_baud = 0xaf0;
	*tx_scale = scale2800;
	*tx_carrier = (short)((m[0xa9e2] & 1) ? 0x74b : 0x690);
	*tx_preemp = (short)
		bitreverse((unsigned short)
			   ((*(unsigned short *)(m + 0xa9e4) >> 4) & 0xf), 4);
	*rx_scale = scale2800;
	mp_or(msg, 2, 1);
	mp_or(msg, 3, 0x20);
	*rx_baud = 0xaf0;
	if (bins[20].shift > 5 || bins[2].shift > 5) {
		*rx_carrier = 0x690;
		*rx_cdesc = c1680;
	} else {
		mp_or(msg, 1, 4);
		*rx_cdesc = c1867;
		*rx_carrier = 0x74b;
	}
	mp_or(msg, 2, 0x24);
	idx = probe_preemph(bins, 18, 0x6789, 0xaf0);
	*pe2800 = idx;
	mp_put_preemph(msg, idx);
	return;

rate_2400:
	if (role == 0x65) {
		mp_or(msg, 2, 0x24);
		idx = probe_preemph(bins, 18, 0x7da7, 0x960);
		*pe2400 = idx;
		mp_put_preemph(msg, idx);
		return;
	}
	if ((*(unsigned short *)(m + 0xa9e0) & 0x3c) == 0)
		return;

	*tx_baud = 0x960;
	*tx_scale = scale2400;
	*tx_carrier = (short)((m[0xa9de] & 4) ? 0x708 : 0x640);
	*tx_preemp = (short)
		bitreverse((unsigned short)
			   (((*(unsigned short *)(m + 0xa9de) & 3) << 2)
			    | ((*(unsigned short *)(m + 0xa9e0) & 0xc0) >> 6)),
			   4);
	*rx_baud = 0x960;
	*rx_carrier = 0x640;
	*rx_scale = scale2400;
	*rx_cdesc = c1600;
	mp_or(msg, 2, 0x24);
	idx = probe_preemph(bins, 18, 0x7da7, 0x960);
	*pe2400 = idx;
	mp_put_preemph(msg, idx);
}

/*
 * ---------------------------------------------------------------------------
 * Bringing the data-mode transmitter up.
 */

/*
 * Apply the far end's requested power reduction to the transmit scale.
 *
 * `mp` points at the received MP message, which the object always reaches as
 * `obj + 0xa9dc` -- two bytes below the three `setfinalrate` unpacks the rate
 * fields out of.  Only its first short is read here, and it holds V.34's two
 * power fields, both sent most significant bit first and so bit-reversed on
 * the way in:
 *
 *     bits 7..5   power reduction, 0..7 dB
 *     bits 4..2   additional power reduction, CLAMPED TO 3 dB
 *
 * The clamp is the object's `cmp $3; jle`, and it is applied AFTER the bit
 * reversal -- so a field arriving as 7 becomes 7 and is then cut to 3, not
 * cut first and reversed after.
 *
 * THE LOCAL PCM REQUIREMENT COMBINES TWO DIFFERENT WAYS.  With a V.90
 * receiver running, `GetVPcmMinimalTxPowerReduction`'s answer is
 *
 *   - ADDED to the far end's request when it is negative, so a PCM modem
 *     asking for more power cancels part of what the far end asked to lose;
 *   - taken as a FLOOR when it is not, so the larger of the two wins.
 *
 * With no V.90 receiver neither happens and the far end's request stands.
 *
 * THE TWO SCALING LOOPS ARE NOT SYMMETRIC, and not only in their constant.
 * 0x390a >> 14 is 0.8912, one dB down, and 0x47cf >> 14 is 1.1220, one dB up
 * -- but the down loop keeps its accumulator at 32 bits and the up loop
 * TRUNCATES IT TO A SHORT every iteration.  See D51.
 *
 * The final 0x4b4b (1.1765, about +1.4 dB) with 0x2000 for rounding is
 * applied to whatever the loop produced and is not part of either dB step.
 * The diagnostic calls the value BEFORE it "final txscale", so the object's
 * own words do not account for it either.
 */
void
settxlevel(void *objp, const short *mp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	short minpr = GetVPcmMinimalTxPowerReduction(obj);
	unsigned w = (unsigned short)mp[0];
	int scale = *(short *)(m + 0x25d4);
	short extra;
	short want;
	short n;

	obj->f25dc = (short)bitreverse((unsigned short)((w >> 5) & 7), 3);

	extra = (short)bitreverse((unsigned short)((w >> 2) & 7), 3);
	if (extra > 3)
		extra = 3;

	obj->f25dc = (short)(extra + (unsigned short)obj->f25dc);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34TXSCALE, power reduction requested "
				     "by remote modem is %d dB\n",
				     (int)obj->f25dc);

	if (obj->v90_receiver != 0) {
		if (minpr < 0)
			obj->f25dc = (short)(minpr
					     + (unsigned short)obj->f25dc);
		else if (obj->f25dc < minpr)
			obj->f25dc = minpr;
	}
	want = obj->f25dc;

	if (want < 0) {
		for (n = want; n < 0; n = (short)(n + 1))
			scale = (short)((scale * 0x47cf) >> 14);
	} else {
		for (n = 0; want > n; n = (short)(n + 1))
			scale = (scale * 0x390a) >> 14;
	}

	/*
	 * `f25d4` is RE-READ here rather than kept: this prints the scale as
	 * it was on entry, and the store below is what changes it.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34TXSCALE, txscale before is %d, "
				     "reduced txscale is %d dB,"
				     "final txscale is %d\n",
				     (int)*(short *)(m + 0x25d4), (int)want,
				     scale);

	*(short *)(m + 0x25d4) = (short)((scale * 0x4b4b + 0x2000) >> 14);
}

/*
 * Configure the transmitter for the rate that has just been negotiated.
 *
 * Six steps and no decisions of its own: the power scale, the modulator, one
 * receiver flag cleared, two state words moved, two transmit flags, and
 * `txinit`.  Everything it feeds the modulator comes out of the rate config
 * `setfinalrate` filled -- baud at +0xaa84, carrier at +0xaa94 and the
 * pre-emphasis index at +0xaa8a -- reached as raw offsets because that is
 * how `setfinalrate` writes them.
 *
 * `V34SetupModulator`'s `v90` argument is 1 when EITHER PCM receiver is
 * running.  That argument is only printed, so what it selects is nothing; it
 * is computed here because the object computes it.
 *
 * The two transitions are WAIT for the receive machine and SSEG for the
 * transmit one, through the same compare-print-assign every other transition
 * in this file uses -- so a run with diagnostics up prints two lines here, or
 * fewer if a machine was already there.
 *
 * It TAIL-CALLS `txinit`, which is why nothing follows the flag stores.
 */
void
v34setuptxmit(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	struct v34_receiver *rx = (struct v34_receiver *)(m + 0x264);
	int pcm;

	settxlevel(obj, (const short *)(m + 0xa9dc));

	pcm = (obj->v90_receiver != 0 || obj->k56flex_receiver != 0);

	V34SetupModulator((struct v34_modulator *)(m + 0x1450),
			  *(short *)(m + 0xaa84), *(short *)(m + 0xaa94),
			  *(short *)(m + 0xaa8a), pcm, 1);

	rx->flags = (unsigned short)(rx->flags & ~0x800);

	hs_setstate(obj, HS_RXSTATE, V34HS_WAIT);
	hs_setstate(obj, HS_TXSTATE, V34HS_SSEG);

	obj->f25c0 = 0;
	obj->f25c2 = (short)(obj->f25c2 | 0x200);

	txinit(obj);
}

/*
 * ---------------------------------------------------------------------------
 * `getbit` -- one bit of a V.34 message, with its CRC on the end.
 *
 * The reader holds an accumulator (`acc`) and how many of its low bits are
 * still unread (`avail`); every call hands back the top unread bit and drops
 * `avail` by one.  When `avail` reaches zero it refills, and the refill is
 * where the interesting cases are:
 *
 *   - a whole word, when `wordbits` or fewer bits of the message are still
 *     to come: shift `acc` up by `wordbits`, OR the next word in, advance
 *     `idx`;
 *   - a PART word, when fewer than `wordbits` are left: shift up by only
 *     what is left and OR THE WHOLE WORD IN ANYWAY, without advancing `idx`.
 *     Both of those are the object's, and the second is why the tail of a
 *     message is not simply its last few bits;
 *   - nothing left and `crc_on` set: the 16-bit CRC becomes the next word,
 *     which is how the sequence carries it;
 *   - nothing left, no CRC, and `repeat` set: reload `acc` and `avail` from
 *     `acc0`/`avail0`, zero `pos` and `idx`, re-arm the CRC to 0xffff, count
 *     the repeat in `repeats` -- and CALL ITSELF for the first bit of it.
 *     That recursion is the object's own; 0x5ec47 calls 0x5eaf0.
 *   - nothing left, no CRC, no repeat: -1.
 *
 * AND ONE ARM THAT IS NOT ANY OF THOSE.  If the message is not merely
 * exhausted but overrun by EXACTLY sixteen bits -- `nbits - pos` is -16 --
 * the reader loads 0xf and four bits instead, so the next four calls return
 * 1, 1, 1, 1.  Nothing else in the function tests a specific negative
 * remainder.  Recorded as the code's, not explained: no caller reconstructed
 * so far arranges it.
 *
 * The CRC is CRC-16-CCITT, polynomial 0x1021, MSB-first and unreflected --
 * the same one `v8_crc` computes in src/v8/v8util.c -- folded one bit at a
 * time over exactly the bits the refill just brought in, high bit first.
 *
 * `pos` is kept in a 16-bit slot: every update is `mov %ax,0x1a(%esi)` after
 * a 32-bit add, so the carry out of bit 15 is dropped.  Hence the
 * `unsigned short` here.
 */
short
getbit(struct v34_bitsource *b)
{
	unsigned int acc;
	unsigned short pos = 0;
	int n = (unsigned short)b->avail;
	int folded = 0;

	if (n == 0) {
		int left;

		pos = (unsigned short)b->pos;
		left = (short)((unsigned short)b->nbits - pos);

		if (left <= 0) {
			if (left != 0) {
				if ((short)left == (short)0xfff0) {
					/* Overrun by exactly one word. */
					b->acc = 0xf;
					b->avail = 4;
					b->pos = (short)(pos + 4);
					n = 4;
					acc = 0xf;
					goto emit;
				}
			} else if (b->crc_on != 0) {
				/* The message is followed by its CRC. */
				acc = (unsigned short)b->crc;
				b->avail = 16;
				b->pos = (short)(pos + 16);
				b->acc = (int)acc;
				n = 16;
				goto emit;
			}

			if (b->repeat == 0)
				return -1;

			b->crc = (short)0xffff;
			b->pos = 0;
			b->repeats = (short)((unsigned short)b->repeats + 1);
			b->idx = 0;
			b->acc = b->acc0;
			b->avail = (short)(unsigned short)b->avail0;

			return (short)getbit(b);
		}

		if ((short)b->wordbits <= left) {
			/* A whole word. */
			int wb = (unsigned short)b->wordbits;
			int idx = (unsigned short)b->idx;

			b->avail = (short)wb;
			acc = (unsigned int)b->acc << ((short)wb & 31);
			acc |= (unsigned short)b->word[(short)idx];
			b->idx = (short)(idx + 1);
			b->pos = (short)(pos + wb);
			b->acc = (int)acc;
			folded = (short)wb;
			n = wb;
		} else if (left > 0) {
			/*
			 * The tail.  `idx` does not advance and the whole word
			 * is ORed in, not just its top `left` bits.
			 */
			int idx = (short)b->idx;

			b->avail = (short)left;
			acc = (unsigned int)b->acc << (left & 31);
			acc |= (unsigned short)b->word[idx];
			b->pos = (short)(pos + left);
			b->acc = (int)acc;
			folded = left;
			n = left;
		}

		if (b->crc_on != 0 && folded != 0) {
			acc = (unsigned int)b->acc;
			do {
				unsigned int crc = (unsigned short)b->crc;
				int bit = (int)(crc >> 15);

				folded = (short)(folded - 1);
				if ((acc >> (folded & 31)) & 1)
					bit ^= 1;
				crc += crc;
				if (bit)
					crc ^= 0x1021;
				b->crc = (short)crc;
			} while (folded != 0);
			goto emit;
		}
	}

	acc = (unsigned int)b->acc;

emit:
	n--;
	b->avail = (short)n;
	return (short)((acc >> (n & 31)) & 1);
}

/*
 * ---------------------------------------------------------------------------
 * `ApplyBulkDelay` -- point the second echo canceller at a round-trip delay.
 *
 * `delay` is `rtd`, the round-trip delay in samples that `v34handshak`
 * measured; both call sites pass it straight out of +0xaa7e.  What comes back
 * is `bulk_tail` -- the read cursor of the ring at +0x35b8 that feeds the far
 * canceller -- with the ring itself cleared to that length and `bulk_head`
 * put back to zero.
 *
 * TWO REJECTIONS SHARE ONE DIAGNOSTIC.  A delay at or below zero becomes 144,
 * and a delay at or past `bulk_len` becomes zero, and both print "V34 bulk
 * delay first estimation %d" with the delay they rejected.  So the two are
 * distinguishable only by the number in the message and by what happens
 * next -- 144 is then bounds-checked in its turn, and can itself be
 * rejected.  The bound is an UNSIGNED comparison (`jb`, not `jl`), which is
 * the wrong way round for safety: a NEGATIVE `bulk_len` is huge unsigned and
 * accepts every delay, and the clear below then runs off the end of the ring.
 * Nothing reconstructed writes `bulk_len`, so no call site is known to reach
 * that; it is recorded rather than fixed, and the tests keep `bulk_len`
 * positive because the overrun would be identical on both sides and prove
 * nothing.
 *
 * THEN THE FAR CANCELLER.  With either PCM receiver running, or with the far
 * canceller not armed to begin with, `fa23c` is simply cleared.  Otherwise a
 * delay of 30 or more normalises it to 1 and leaves it on, and a delay below
 * that turns it off AND pulls the DMA delay at +0x25c back by `delay + 15`,
 * capped at 30 -- the object's own words, "RTD (%d) lower than min (%d),
 * masking Far EC..." and "...Modifying dma delay from %d to %d".
 *
 * The ring is cleared at the LITERAL offset +0x35b8, not through `bulk_ring`
 * at +0x35b0.  That is the object's: the pointer is never loaded here.
 */
void
ApplyBulkDelay(void *objp, short delay)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	int d = delay;

	if (d <= 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V34 bulk delay first estimation %d\n", d);
		d = 0x90;
	}

	if ((unsigned int)d >= (unsigned int)obj->bulk_len) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V34 bulk delay first estimation %d\n", d);
		d = 0;
	}

	obj->bulk_tail = d;
	obj->bulk_head = 0;
	sysdep_memset(m + 0x35b8, 0, (size_t)(d * 2));

	if (obj->v90_receiver == 0 && obj->k56flex_receiver == 0
	    && obj->fa23c != 0) {
		if ((short)d > 0x1d) {
			obj->fa23c = 1;
		} else {
			int back;

			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "RTD (%d) lower than min (%d), "
				    "masking Far EC...\r\n", d, 0x1e);

			back = (short)(d + 15);
			obj->fa23c = 0;
			if ((short)back > 0x1e)
				back = 0x1e;

			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "...Modifying dma delay from %d to %d\r\n",
				    obj->f25c, obj->f25c - back);

			obj->f25c = (short)((unsigned short)obj->f25c - back);
		}
	} else {
		obj->fa23c = 0;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34 bulk delay estimation %d (FAR=%d)\n",
				     d, obj->fa23c);
}

/*
 * ---------------------------------------------------------------------------
 * `v34handshak`, four arms of it.
 *
 * The function is 61,541 bytes and three concurrent state machines behind
 * four dispatches; docs/v34handshak.md is the map and
 * test/harness/v34hsstep.c is the fixture that makes one arm a committable
 * unit.  What is here is the microstate table's core -- the arm twenty-four
 * of its forty states share, and the three states 62, 79 and 80 -- plus the
 * infrastructure those arms cannot be reached or left through:
 *
 *     0x628f0  the prologue and the guards that choose a dispatch
 *     0x64a64  the RX_DPSK route: V34agc, the +0xa8a0 gate, fskdemodulate
 *     0x64ac3  the microstate dispatch, .rodata+0x3000
 *     0x62af1  the once-per-block txstate dispatch, .rodata+0x2ee8
 *     0x62a40  the tail every arm of that dispatch falls into
 *
 * EVERY OTHER ARM STOPS.  `t3c_unwritten` is called where an arm this batch
 * did not write would begin, so an unwritten case is a halt and never an
 * answer.  A `return` there would be a wrong result the differential test
 * could only catch on a case some test happens to drive, which is the shape
 * this tree refuses; see the rule in CLAUDE.md.
 */

/*
 * Offsets this batch reads or writes.  Prefixed because four batches are
 * writing arms of this function into this one translation unit at the same
 * time and finding 325 is what one collided macro cost.
 *
 * They are offsets rather than struct members because most of them land in
 * an `unmapped_*` pad today (tools/whichfield.py says which), and the two
 * members that are mapped -- `rtd` and the receiver's `flags`, `agc_level`,
 * `agc_gain`, `f1d2`, `rx_samples` -- are used by name below.
 */
#define T3C_PROGRESS	0x0004	/* int:   the code the caller reads back    */
#define T3C_TIMER_LO	0x0238	/* int:   a running sample count            */
#define T3C_TIMER_HI	0x023c	/* int:   that plus 431,488                 */
#define T3C_LVL_LIMIT	0x0230	/* int:   compared against the AGC level    */
#define T3C_LVL_COUNT	0x0234	/* int:   blocks below it, capped at 0x257f */
#define T3C_RECEIVER	0x0264	/* struct v34_receiver, inside the object   */
#define T3C_MODE	0x2218	/* int:   selects what the tail reports     */
#define T3C_TXCURSOR	0x221c	/* short: samples emitted this block        */
#define T3C_TXLIMIT	0x2aa0	/* short: how many the block wants          */
#define T3C_DETECTOR	0x3564	/* struct v34_detector, inside the object   */
#define T3C_F358C	0x358c	/* short: cleared by MOH_TONE's body        */
#define T3C_FSKGATE	0xa8a0	/* int:   non-zero diverts at 0x64a87       */
#define T3C_BLK_A94C	0xa94c	/* what +0xaa6c is aimed at                 */
#define T3C_BLK_A97C	0xa97c	/* what +0xaa70 is aimed at                 */
#define T3C_COUNT	0xaa78	/* short: the counter, and HS_TRACE_2       */
#define T3C_COUNT_SRC	0xaa7c	/* short: RX_PHASE3_CALL copies it in       */
#define T3C_PTR_AA6C	0xaa6c
#define T3C_PTR_AA70	0xaa70
#define T3C_FAA96	0xaa96	/* short: tripled into the receiver        */
#define T3C_FAADC	0xaadc
#define T3C_FAAE0	0xaae0
#define T3C_FAAE2	0xaae2	/* byte:  bit 0 gates RX_PHASE3_CALL       */
#define T3C_FABE6	0xabe6	/* short: set to 1 on the retrain path     */
#define T3C_FABF0	0xabf0	/* int:   1 diverts MOH_TONE at 0x6d57c    */
#define T3C_FABF8	0xabf8	/* byte:  "the drop has been reported"     */
#define T3C_FABF9	0xabf9	/* byte:  non-zero diverts at 0x6c8f8      */
#define T3C_FABFC	0xabfc	/* short: what the counter must reach      */
#define T3C_RX_SAMPS	0x010c	/* receiver: where a detector reads from   */

#define T3C_RX(obj)	((struct v34_receiver *)((char *)(obj) + T3C_RECEIVER))
#define T3C_DET(obj)	((struct v34_detector *)((char *)(obj) + T3C_DETECTOR))

/*
 * A dispatch arm this reconstruction has not written.
 *
 * It has to stop rather than return.  `v34handshak` is being landed one arm
 * at a time against test/harness/v34hsstep.c, so at any moment most of the
 * table is missing, and an arm that returns quietly is indistinguishable
 * from an arm that correctly did nothing -- the differential test would
 * catch it only on a case some test happens to drive, and the whole point of
 * the per-case split is that most cases are not driven yet.
 */
static void
t3c_unwritten(void)
{
	abort();
}

static unsigned char
t3c_getb(const struct v34_object *obj, unsigned off)
{
	return *((const unsigned char *)obj + off);
}

static void
t3c_putb(struct v34_object *obj, unsigned off, unsigned char v)
{
	*((unsigned char *)obj + off) = v;
}

static int
t3c_geti(const struct v34_object *obj, unsigned off)
{
	return *(const int *)((const char *)obj + off);
}

static void
t3c_puti(struct v34_object *obj, unsigned off, int v)
{
	*(int *)((char *)obj + off) = v;
}

static void
t3c_putp(struct v34_object *obj, unsigned off, void *p)
{
	*(void **)((char *)obj + off) = p;
}

/*
 * The tail at 0x62a40, which every arm of the once-per-block dispatch falls
 * into and which is also that dispatch's own default.
 *
 * `tx` is the transmit state the dispatch read, sign extended -- the last
 * three comparisons are against it and not against the table index.
 *
 * The two unsigned range tests are the object's `lea -N(%edx); cmp $1; ja`,
 * which is how GCC spells "one of these two values".
 */
static void
t3c_block_tail(struct v34_object *obj, int tx)
{
	struct v34_receiver *rx = T3C_RX(obj);
	int mode = t3c_geti(obj, T3C_MODE);

	if (mode == 1) {
		/* 0x62b45, and it rejoins below rather than returning. */
		rx->f1d2 = (short)(3 * hs_get(obj, T3C_FAA96));
		t3c_puti(obj, T3C_PROGRESS, 4);
	}

	if ((unsigned)(mode - 4) <= 1u)
		t3c_puti(obj, T3C_PROGRESS, 6);

	if ((unsigned)(mode - 2) <= 1u && tx == V34HS_SILENCERETRAIN)
		t3c_unwritten();			/* 0x64884 */

	/* Unsigned, so a wrapped count reads as enormous rather than as past. */
	if ((unsigned)t3c_geti(obj, T3C_TIMER_LO)
	    > (unsigned)t3c_geti(obj, T3C_TIMER_HI))
		t3c_puti(obj, T3C_PROGRESS, 8);

	if ((int)rx->agc_level >= t3c_geti(obj, T3C_LVL_LIMIT))
		t3c_puti(obj, T3C_LVL_COUNT, 0);
	else
		t3c_puti(obj, T3C_LVL_COUNT,
			 t3c_geti(obj, T3C_LVL_COUNT) + 1);

	if (t3c_geti(obj, T3C_LVL_COUNT) > 0x257f)
		t3c_puti(obj, T3C_PROGRESS, 9);

	/*
	 * The three Modem-on-Hold transmit states, and they are compared
	 * against the STATE and not against the table index -- 0x62ac5
	 * re-signs `%cx` rather than reusing the `-5` the dispatch made.
	 */
	if (tx == V34HS_MOH_FRR)
		t3c_unwritten();			/* 0x62b2f */
	else if (tx == V34HS_MOH_CLEARDOWN)
		t3c_puti(obj, T3C_PROGRESS, 0x10);
	else if (tx == V34HS_MOH_ON_HOLD)
		t3c_unwritten();			/* 0x64a4f */
}

/*
 * The once-per-block transmit dispatch at 0x62af1, table 2 at .rodata+0x2ee8.
 *
 * Every microstate arm below leaves through here: the microstate machine is
 * a set of guards in front of the transmit machine rather than sixteen
 * independent bodies (finding 288), so a microstate case cannot be landed
 * without whichever transmit arm its own txstate selects.
 *
 * ONLY ONE OF TABLE 2'S SEVEN TARGETS IS WRITTEN HERE, and it belongs to
 * #56's batch rather than to this one: 0x644c9 is three instructions, and
 * without it microstate 79's body cannot be reached at all -- it sets
 * txstate to TX_DPSK, and the retrain `v34handshakinit` on microstate 80's
 * far path leaves SILENCERETRAIN, and both select 0x644c9.  Whoever merges
 * #56 should expect to find it already here.
 */
static void
t3c_txblock(struct v34_object *obj)
{
	int tx = hs_get(obj, HS_TXSTATE);

	if ((unsigned)(tx - 5) <= 0x45u) {
		switch (tx) {
		case V34HS_TX_DPSK:		/* 0x644c9, with 51 54 60 74 */
		case V34HS_TX_L1:
		case V34HS_SILENCEINFO:
		case V34HS_TONE_AB:
		case V34HS_SILENCERETRAIN:
			t3c_puti(obj, T3C_PROGRESS, 0);
			break;
		default:
			t3c_unwritten();
			break;
		}
	}

	t3c_block_tail(obj, tx);
}

/*
 * Microstate 62 `RX_PHASE3_CALL`, 0x65c7a.
 *
 * Bit 0 of +0xaae2 is the whole guard: clear, and the arm is the shared one.
 * Set, and it copies +0xaa7c over the counter, moves the microstate on to
 * TX_PHASE2_CALL and arms a detector by raising the receiver's pending flag.
 *
 * WHAT IS DELIBERATELY NOT HERE.  0x65c95 compares the entered microstate
 * against 56 and 0x65ca0 jumps to 0x6c2c5 when it matches.  56 does not
 * dispatch here -- .rodata+0x3000's entry for it is 0x66834, which is 47's
 * arm -- so that block is tail-merged from 47/56 and is that case's, not
 * this one's.
 */
static void
t3c_micro_rx_phase3_call(struct v34_object *obj)
{
	struct v34_receiver *rx = T3C_RX(obj);

	if ((t3c_getb(obj, T3C_FAAE2) & 1) == 0) {
		t3c_txblock(obj);			/* 0x6abae */
		return;
	}

	/* Sixteen bits, and the trace below reads it back as [2]. */
	hs_put(obj, T3C_COUNT, hs_get(obj, T3C_COUNT_SRC));

	hs_setstate(obj, HS_MICROSTATE, V34HS_TX_PHASE2_CALL);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PHASE2, RX_PHASE3_CALL ,filtdelay = "
				     "%d, rxflgs= 0x%x,rx->gain=0x%x\n",
				     (int)hs_get(obj, T3C_COUNT_SRC),
				     (unsigned)rx->flags,
				     (unsigned)(int)rx->agc_gain);

	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);
	t3c_txblock(obj);
}

/*
 * Microstate 79 `MOH_TONE`, 0x657ca, and 80 `MOH_TONE_DROP`, 0x656e0.
 *
 * Both count one call, run the detector at +0x3564 over the receiver's input
 * from +0x10c to `rx_samples`, and decide on the answer -- and then they
 * diverge completely, which is what makes them two cases and not one.  The
 * detector's result is tested sixteen bits wide in both (`test %ax,%ax`).
 */
static short
t3c_moh_step_detector(struct v34_object *obj)
{
	struct v34_receiver *rx = T3C_RX(obj);

	hs_put(obj, T3C_COUNT, (short)(hs_get(obj, T3C_COUNT) + 1));

	return (short)tone_detect(rx, T3C_DET(obj),
				  (const short *)((const char *)rx
						  + T3C_RX_SAMPS),
				  rx->rx_samples);
}

static void
t3c_micro_moh_tone(struct v34_object *obj)
{
	if (t3c_moh_step_detector(obj) == 0) {
		t3c_txblock(obj);			/* 0x6afc4 */
		return;
	}

	if (hs_get(obj, T3C_COUNT) < hs_get(obj, T3C_FABFC)) {
		t3c_txblock(obj);			/* 0x6c701 */
		return;
	}

	hs_setstate(obj, HS_TXSTATE, V34HS_TX_DPSK);

	if (t3c_geti(obj, T3C_FABF0) == 1)
		t3c_unwritten();			/* 0x6d57c */

	hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);

	hs_put(obj, T3C_FAADC, 0);
	hs_put(obj, T3C_FAAE0, 0);
	hs_put(obj, T3C_BLK_A97C + 0x14, -1);
	hs_put(obj, T3C_BLK_A97C + 0x18, 8);
	hs_put(obj, T3C_COUNT, 0);
	t3c_putp(obj, T3C_PTR_AA70, (char *)obj + T3C_BLK_A97C);
	t3c_putp(obj, T3C_PTR_AA6C, (char *)obj + T3C_BLK_A94C);
	hs_put(obj, T3C_F358C, 0);
	hs_put(obj, HS_TRACE_1, 0);

	t3c_txblock(obj);
}

static void
t3c_micro_moh_tone_drop(struct v34_object *obj)
{
	if (t3c_moh_step_detector(obj) != 0
	    && t3c_getb(obj, T3C_FABF8) == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34Handshake: Detected signal "
					     "drop on FRR request, time to "
					     "move to phase1...\r\n");
		t3c_putb(obj, T3C_FABF8, 1);
	}

	/* The round-trip delay sets how long the drop has to persist. */
	if ((int)hs_get(obj, T3C_COUNT) < (((int)obj->rtd) >> 2) + 0x12c0) {
		t3c_txblock(obj);			/* 0x6aae6 */
		return;
	}

	if (t3c_getb(obj, T3C_FABF9) != 0)
		t3c_unwritten();			/* 0x6c8f8 */

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("MOH: Timeout waiting for MH sequence "
				     "under MHfrr, initiating retrain\r\n");

	v34handshakinit(obj, 1);
	hs_put(obj, T3C_FABE6, 1);

	t3c_txblock(obj);
}

/*
 * The handshake, once per block.
 *
 * Four guards choose one of three dispatches, read off the prologue at
 * 0x628f0; the cursor/limit compare is signed and sixteen bits wide, and so
 * is the receiver's own first halfword against 5.
 */
void
v34handshak(void *vobj)
{
	struct v34_object *obj = (struct v34_object *)vobj;
	struct v34_receiver *rx = T3C_RX(obj);
	int mst;

	if (hs_get(obj, T3C_TXCURSOR) < hs_get(obj, T3C_TXLIMIT))
		t3c_unwritten();	/* table 1 at .rodata+0x2da0 -- #56 */

	if (hs_get(obj, T3C_RECEIVER) <= 5) {
		t3c_txblock(obj);
		return;
	}

	if (hs_get(obj, HS_RXSTATE) != V34HS_RX_DPSK) {
		/*
		 * The compare chain at 0x62a02 -- RECEIVE at 0x653e4, WAIT at
		 * 0x6752c, more above 43 at 0x62b71, and anything it does not
		 * name falls into the transmit dispatch.  #58.
		 */
		t3c_unwritten();
		return;
	}

	V34agc(rx);

	if (t3c_geti(obj, T3C_FSKGATE) != 0)
		t3c_unwritten();			/* 0x6754b */

	fskdemodulate(obj, (const short *)((const char *)rx + T3C_RX_SAMPS),
		      &obj->fsk);

	/*
	 * Only now is +0x3592 read: nothing on the way here writes any of the
	 * three state words, which is finding 285 and is what makes a
	 * poke-and-step fixture possible at all.
	 */
	mst = hs_get(obj, HS_MICROSTATE);

	if ((unsigned)(mst - 41) > 0x27u) {
		t3c_txblock(obj);			/* 0x65329 */
		return;
	}

	switch (mst) {
	case V34HS_RX_PHASE3_CALL:
		t3c_micro_rx_phase3_call(obj);
		return;
	case V34HS_MOH_TONE:
		t3c_micro_moh_tone(obj);
		return;
	case V34HS_MOH_TONE_DROP:
		t3c_micro_moh_tone_drop(obj);
		return;

	/*
	 * The arm twenty-four of the forty states share, 0x6590b: read the
	 * transmit state and jump to the once-per-block dispatch.  These are
	 * .rodata+0x3000's twenty-four entries holding 0x6590b, not a guess
	 * at which states "do nothing" -- and the default above is the same
	 * three instructions at a different address.
	 */
	case V34HS_DET_CJ:		/* 42 */
	case V34HS_RX_DPSK:		/* 43 */
	case V34HS_TONE_AB_ANS:		/* 45 */
	case V34HS_TX_L2:		/* 52 */
	case V34HS_DET_AB:		/* 53 */
	case V34HS_SILENCEINFO:		/* 54 */
	case V34HS_TX_PHASE3_CALL:	/* 57 */
	case V34HS_TONE_AB:		/* 60 */
	case V34HS_TONE_AB_CALL:	/* 61 */
	case V34HS_JTXMIT:		/* 64 */
	case V34HS_XMIT0:		/* 65 */
	case V34HS_TRNSEG4A:		/* 66 */
	case V34HS_XMITMP:		/* 67 */
	case V34HS_J1TXMIT:		/* 68 */
	case V34HS_EXMIT:		/* 69 */
	case V34HS_DATAXMIT:		/* 70 */
	case V34HS_TXLEVEL:		/* 71 */
	case V34HS_RX_L1:		/* 72 */
	case V34HS_RX_L2:		/* 73 */
	case V34HS_SILENCERETRAIN:	/* 74 */
	case V34HS_RX_RETRAIN_CALL:	/* 75 */
	case V34HS_RX_RETRAIN_ANSWER:	/* 76 */
	case V34HS_TX_RETRAIN_ANS:	/* 77 */
	case V34HS_JaTXMIT:		/* 78 */
		t3c_txblock(obj);
		return;

	default:
		t3c_unwritten();
		return;
	}
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

V34HS_OFF(f25c2,   struct v34_object,   f25c2,      0x25c2);
V34HS_OFF(f25c6,   struct v34_object,   f25c6,      0x25c6);
V34HS_OFF(f25c8,   struct v34_object,   f25c8,      0x25c8);
V34HS_OFF(f25cc,   struct v34_object,   f25cc,      0x25cc);
V34HS_OFF(f25d0,   struct v34_object,   f25d0,      0x25d0);
V34HS_OFF(f25d2,   struct v34_object,   f25d2,      0x25d2);

#endif
