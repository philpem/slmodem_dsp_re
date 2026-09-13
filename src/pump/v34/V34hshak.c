/*
 * V34hshak.c -- ITU-T V.34: the handshake's support functions.
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
 * a caller.  Finding F221 is the general result and finding F227 is these two.
 * Finding F117, which drew the original conclusion, stands as the reading of
 * the call graph and is superseded only in what it says can be tested.
 *
 * NINE OF THE TWELVE ARE CALLED BY NOTHING IN THE OBJECT.  Nothing reaches
 * `dpskDetectInfo1Init`, `dpskinit`, `setupreceiver`, `preempindex` or any
 * of the five DFT routines -- no relocation, no `call`, no `jmp`, and no
 * little-endian copy of their addresses in `.text`, `.data` or `.rodata`.
 * Only `setfinalrate` has a caller, in `v34handshak`.  The `jmp` and the
 * data scans are not belt and braces: this object tail-calls constantly, and
 * a section-symbol relocation with an addend does not answer to a grep for a
 * name.  Finding F212 gives the controls each scan was checked against.
 *
 * They are global, so they are testable regardless, but it means their
 * arguments and their bank sizes have to be read out of the code rather than
 * off a call site.  Finding F89 recorded the same shape twice already.
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
#include "dsplib/v34hstx1.h"	/* table 1's nineteen arms, for the loop  */
#include "dsplib/v34info.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"
#include "dsplib/v34shell.h"	/* modulatevector, for datapumpv34 below */

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
 * Coefficient derivation was deferred to task #47 (docs/fastpass.md) on the
 * grounds that a byte-exact copy is byte-exact and the differential test
 * proves it with no derivation at all.  That holds right up to the 8 kHz
 * retarget, where the tables have to be REGENERATED rather than copied, and
 * a table nobody can regenerate is a table that pins the sample rate.
 *
 * Finding F620 did two of this file's three, and both are folded in below
 * rather than left in the findings file: the ten carrier descriptors are
 * solved exactly, the 2800 baud timing constants are solved exactly, and the
 * five transmit power scales are still open -- but open with the two obvious
 * answers DISPROVED rather than untried, which is worth more than an
 * untested guess.  Each table below says which of the three it is.
 */

/*
 * The transmit power scales, one table per symbol rate.  TWO ROWS OF
 * FOURTEEN, and the split is visible in the data rather than assumed: each
 * row is a decreasing sequence that runs out into zeros, and the second
 * starts again at roughly the first's head.  The row length is fourteen for
 * every rate; the number of non-zero entries is not, and grows with the
 * symbol rate -- nine at 2400, thirteen at 3429.
 *
 * WHAT INDEXES THEM IS NOT KNOWN.  This note used to answer "the ten
 * pre-emphasis characteristics V.34 defines plus the flat one, truncated
 * where the rate cannot use them all".  Finding F620 is task #47's attempt on
 * these tables, and it disproves that reading and one other:
 *
 *   NOT the pre-emphasis index.  V.34 defines eleven, in two template
 *   families -- indices 0-5 a linear tilt in dB with alpha from Table
 *   3/V.34, indices 6-10 flat to 0.8 of the band and then a step and a ramp
 *   with beta and gamma from Table 4/V.34.  Integrating both families over
 *   each rate's band (Table 2/V.34 gives the centre, the band is +/-0.45
 *   wide) predicts a JUMP BACK UP at the family boundary: 2400's prediction
 *   runs 4579, 3913, 3321, 2799, 2343, 1949 and then 4404 at index 6,
 *   because a template that is flat over most of the band has nearly unity
 *   gain.  The real rows march smoothly down across that boundary and do not
 *   know it exists.  Eleven indices also cannot fill 3429's thirteen.
 *
 *   NOT one row per carrier.  Table 2/V.34 gives two carriers per rate,
 *   which is the obvious reading of two rows.  3429 refutes it: its low and
 *   high carriers are BOTH 1959, so its two rows would have to be identical,
 *   and they are not -- 3400 against 3403, 2047 against 2146.
 *
 * WHAT IS ESTABLISHED IS THE SHAPE.  Each row is close to a geometric ladder
 * in dB, and the step scales inversely with the symbol rate: -2.958, -2.603,
 * -2.398, -2.281 and -2.115 dB at the five rates, every one of which
 * `step_dB = 7200/S` predicts to within 0.05 dB.  That is a
 * CHARACTERISATION AND NOT A DERIVATION, and the difference is measurable
 * rather than rhetorical: the within-row residuals reach 0.6 dB, an order of
 * magnitude more than the rule's own error, so something further is shaping
 * the rows and writing 7200/S down as the closed form would be fitting five
 * numbers with one.  Do not regenerate from it at 8 kHz.
 *
 * `setfinalrate` only ever stores the ADDRESS of one of these.  The obvious
 * candidate for what indexes it is `preempindex` below, and that does not
 * work either: `preempindex` sets `i = 5` and increments BEFORE its first
 * test, so it can only ever return 6 or more, and nothing here selects
 * entries 1 to 5.  The two are not connected by anything in this object --
 * no function reads a scale table and calls preempindex.
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
 * frequency falls.  The object never dereferences these here: `setfinalrate`
 * and `probeselect` store the address and nothing else.
 *
 * EACH ONE IS A PAIR OF TWO-POLE RESONATORS, TWO HERTZ EITHER SIDE OF THE
 * CARRIER.  That was task #47's, and finding F620 settled it exactly rather
 * than suggestively.  Read each pair as Q14 denominator coefficients
 *
 *      { -2*r*cos(2*pi*f/9600), r^2 }
 *
 * 15735/16384 is 0.9604, so r is 0.98 in all eight -- which is why only the
 * first of each pair moves -- and solving for f at the object's 9600 Hz
 * sample rate puts every pole on a whole hertz:
 *
 *      c2000  1998, 2002        c1829   1827, 1831      c1680  1678, 1682
 *      c1959  1961, 1957        c1800_  1798, 1802      c1600  1598, 1602
 *      c1920  1918, 1922        c1867   1865, 1869
 *
 * Every midpoint is the table's own name.  `c1959` is the one oddity worth
 * recording: its poles run high-then-low where the other seven run
 * low-then-high, and the midpoint is still exactly 1959.  A transcription
 * slip in the original, and harmless, because the pair is summed and not
 * ordered.
 *
 * This is the form the 8 kHz retarget needs.  At another sample rate these
 * are REGENERATED from r and f, not resampled: two hertz of detuning either
 * side is a design intent, and nothing about it survives a resampler.
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
 * `v34modeminit` hands to the tone detector rather than storing.  Same form,
 * two differences, and finding F620 has both exactly.
 *
 * 15993/16384 is 0.97614, so r is 0.988 here against 0.98 above -- a pole
 * nearer the unit circle, so a narrower and longer-ringing resonance, which
 * is what a signalling tone wants and a data carrier does not.  And both
 * poles of each pair sit on the SAME frequency rather than straddling it:
 * 1200.0 and 2400.0 exactly, one resonator written twice.
 *
 * `c2400_`'s first coefficient is zero because cos(2*pi*2400/9600) is
 * cos(90 degrees).  A resonator at a quarter of the sample rate needs no
 * rotation term, which is exactly what a zero there means -- so the zero is
 * the arithmetic and not a missing entry.
 *
 * The trailing underscores are the object's.
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
 * they are one helper here and not two transcriptions; finding F130 is the
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

	rx->fir_coeff = high ? bpv22high : bpv22low;

	fsk_state_init(obj);

	/*
	 * The AGC comes up at whatever +0x262 holds, with a step of 0x199a.
	 * `setupreceiver` does the same two stores with 0x2000 instead, so
	 * the step is the only thing that distinguishes the phase-2 AGC from
	 * the data-mode one.
	 */
	rx->agc_step = 0x199a;
	rx->agc_gain = rx->agc_start_gain;

	/*
	 * Bits 9 and 11 of the receiver's flag word, set together.  Bit 9 is
	 * V34_RX_FLAG_DET_PENDING (finding F114) -- the AGC freeze -- so this
	 * arms a detector and holds the gain still while it settles.
	 */
	rx->flags = (unsigned short)(rx->flags
				     | (V34_RX_FLAG_DET_PENDING | V34_RX_FLAG_FIR));

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
	rx->rms_idx = 0;
	rx->retrain_gate = 0;
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
 * four values changed.  `role == 0x65` -- the originate/answer flag
 * `preinitdigital` also reads (finding F177) -- picks between them:
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

	rx->rrn_local_dir = 0;
	rx->baud_copy = 0;
	obj->is_short = 0;
	obj->tx_flags = V34_EC_FROZEN;
	obj->echo_calls = 0;
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

	rx->out_count = 4;
	rx->phase_wrap = 0x3e80;
	rx->symbol_period = 0x3e80;
	rx->phase_inc = 0x3e80;
	rx->phase_frac = 0x1f40;
	rx->half_len = 0x10;
	rx->carrier = hsine1800;

	originate = (obj->role == 0x65);

	obj->tx_flags = (short)(originate ? V34_EC_FROZEN
					   : (V34_EC_FROZEN | V34_TXFLAG_CALLER));
	rx->flags = (unsigned short)(originate ? 0 : V34_SCR_ANSWERER);

	fsk_clear(obj);
	V34SetupModulator(mod, 600, (short)(originate ? 1200 : 2400), 0, 0, 1);
	rx->fir_coeff = originate ? bpv22high : bpv22low;
	fsk_state_init(obj);

	rx->agc_step = 0x199a;
	rx->flags = (unsigned short)(rx->flags
				     | (V34_RX_FLAG_DET_PENDING | V34_RX_FLAG_FIR));
	rx->agc_gain = rx->agc_start_gain;

	{
		short *w = (short *)((char *)rx + 0x13c);

		for (i = 0; i <= 0x2f; i++)
			w[i] = 0;
	}
	rx->rms_idx = 0;
	rx->retrain_gate = 0;

	preinitdigital(obj);
	txinit(obj);

	detectorinit((struct v34_detector *)((char *)obj + 0x3564),
		     originate ? c2400_ : c1200_, 0, 0xc8, 0x32, 0x600, 0);

	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);
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
 * WHY 2800 USES 0x3e82 AND 0x1f41 WHERE EVERY OTHER RATE USES 0x3e80 AND
 * 0x1f40.  This is the second of task #47's derivations (finding F620), and
 * the rule is that `step/wrap` is EXACTLY `2400/baud` at every rate:
 *
 *      baud   step    wrap    step/wrap   2400/baud
 *      2400   0x3e80  0x3e80     1          1
 *      2743   0x36b0  0x3e80     7/8        7/8
 *      2800   0x3594  0x3e82     6/7        6/7
 *      3000   0x3200  0x3e80     4/5        4/5
 *      3200   0x2ee0  0x3e80     3/4        3/4
 *      3429   0x2bc0  0x3e80     7/10       7/10
 *
 * 2743 and 3429 are rounded display names for 19200/7 and 24000/7 --
 * 2742.857 and 3428.571 -- which is why their ratios are sevenths and why
 * 2400/2743 does not look exact until the rate is written as a fraction.
 *
 * So the wrap is 16000 wherever the fraction is representable over it, and
 * 2800 is the one rate whose denominator needs a factor of seven: 16000 is
 * not divisible by 7, and 13716/16000 is merely close, while 16002 = 7*2286
 * makes 13716/16002 exactly 6/7.  Two counts on the wrap and one on the
 * half-step initial phase, and the interpolator is exact instead of drifting
 * one part in 6000.  Not, as this file used to say elsewhere, "a closer
 * rational fit" -- it is not close, it is exact, and only for 2800.
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

	rx->out_count = 4;

	switch (baud) {
	case 2400:
		rx->phase_wrap = 0x3e80; rx->phase_inc = 0x3e80;
		rx->symbol_period = 0x3e80; rx->phase_frac = 0x1f40;
		break;
	case 2743:
		rx->phase_wrap = 0x3e80; rx->phase_inc = 0x36b0;
		rx->symbol_period = 0x36b0; rx->phase_frac = 0x1f40;
		break;
	case 2800:
		rx->phase_wrap = 0x3e82; rx->phase_inc = 0x3594;
		rx->symbol_period = 0x3594; rx->phase_frac = 0x1f41;
		break;
	case 3000:
		rx->phase_wrap = 0x3e80; rx->phase_inc = 0x3200;
		rx->symbol_period = 0x3200; rx->phase_frac = 0x1f40;
		break;
	case 3200:
		rx->phase_wrap = 0x3e80; rx->phase_inc = 0x2ee0;
		rx->symbol_period = 0x2ee0; rx->phase_frac = 0x1f40;
		break;
	case 3429:
		rx->phase_wrap = 0x3e80; rx->phase_inc = 0x2bc0;
		rx->symbol_period = 0x2bc0; rx->phase_frac = 0x1f40;
		break;
	default:
		break;
	}

	switch (carrier) {
	case 1600: rx->carrier = hsine1600; rx->half_len = 6;    break;
	case 1680: rx->carrier = hsine1680; rx->half_len = 0x28; break;
	case 1800: rx->carrier = hsine1800; rx->half_len = 0x10; break;
	case 1829: rx->carrier = hsine1829; rx->half_len = 0x15; break;
	case 1867: rx->carrier = hsine1867; rx->half_len = 0x24; break;
	case 1920: rx->carrier = hsine1920; rx->half_len = 5;    break;
	case 1959: rx->carrier = hsine1959; rx->half_len = 0x31; break;
	case 2000: rx->carrier = hsine2000; rx->half_len = 0x18; break;
	default: break;
	}

	rx->agc_step = 0x2000;
	rx->agc_gain = rx->agc_start_gain;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34AGC, setup receiver gain = 0x%x\n",
				     rx->agc_start_gain);

	/*
	 * Clear bits 8..11 before arming the detector and set bit 9 after.
	 * The clear takes down the AGC freeze that `dpskinit` put up, and
	 * `detectorinit` runs with it down; the set puts it back, which is
	 * what leaves the gain still while the new detector settles.
	 */
	rx->flags = (unsigned short)(rx->flags
				     & (unsigned short)~(V34_RX_FLAG_TRAINED
							 | V34_RX_FLAG_DET_PENDING
							 | V34_RX_FLAG_DATA
							 | V34_RX_FLAG_FIR));

	detectorinit((struct v34_detector *)((char *)obj + 0x3564),
		     cdesc, 0, 8, 10, 0x600, 0);

	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);
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
						"V34PREEMPHASIS, - index is " "0, baudrate= %d\n", baudrate);
				return 0;
			}
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V34PREEMPHASIS, - index is %d, " "baudrate= %d\n", i, baudrate);
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
 * here rather than in DFTC.c is link order: they sit between `V34scrambler`
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
		bins->sum_re = 0.0;
		bins->sum_im = 0.0;
		bins->denergy = 0.0;
		bins->acc_im = 0;
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
	struct v34_dftbin *p;

	/*
	 * `inc` is cleared in the loop and then written again below.  That is
	 * the object's -- the loop zeroes all four fields uniformly and the
	 * frequencies are assigned afterwards -- and folding the two would
	 * lose the fact that the loop is the same loop as the noise bank's.
	 * Walked as a pointer, not indexed: the object holds a plain
	 * `add $0x2c,%edx` cursor over the loop rather than a
	 * base-plus-index address at each field.
	 */
	for (i = 0, p = bins; i <= 3; i++, p++) {
		p->phase = 0;
		p->inc = 0;
		p->acc_re = 0;
		p->acc_im = 0;
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
	struct v34_dftbin *p;

	for (i = 0, p = bins; i <= 3; i++, p++) {
		p->phase = 0;
		p->inc = 0;
		p->acc_re = 0;
		p->acc_im = 0;
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
 * F212, which records the sweep that missed this and why it missed it.
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
 * top of this file are compared.  That is finding F173's trap a third time.
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
 * arguments rather than guessed -- finding F171 is what guessing costs.  At
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
 * A FOURTH, INDEPENDENT SIGN, which is the same argument finding F171 used to
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
#define HS_TRACE_2	0xaa78

short
hs_get(const struct v34_object *obj, unsigned off)
{
	return *(const short *)((const char *)obj + off);
}

void
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
 *
 * NOT `static`, and neither are `hs_get` and `hs_put` above.  `v34handshak`
 * belongs to this translation unit and is being reconstructed one dispatch
 * arm at a time in files beside this one (v34hshak_t3mid.c is the first),
 * and its arms emit the same three transitions from the same three format
 * strings.  A second copy of this function next door is a second place for
 * the argument order above to be got wrong.  Finding F223's six functions
 * lost their `static` for the weaker reason that a test wanted to call them.
 */
void
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
				     (long)obj->vect_idx,
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
 * 0x234(that)`, which is +0x238 of the object and not +0x234.  Finding F179
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
	obj->tx_flags = (short)(obj->tx_flags & ~V34_TXFLAG_PPSEG);

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

		rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_PREDICT);
		*(short **)(m + 0xaa70) = (short *)(m + 0xa97c);
		hs_put(obj, 0xa97c + 0x18, 0x11);
		hs_put(obj, 0x25dc, 0);

		/*
		 * 0x66 and not 0x65: `role` is the originate/answer flag
		 * `v34modeminit` and `preinitdigital` both test against 0x65,
		 * and this one arm tests the other value.  Reproduced as
		 * written -- a sweep of {0x65, something-else} would not have
		 * told the two tests apart.
		 */
		if (obj->role == 0x66)
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
		if ((rx->flags & V34_RX_FLAG_RETRAIN) || m[0xac17] != 0) {
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

		rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_PREDICT);
		rx->agc_gain = rx->agc_start_gain;
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

		obj->seg_symcount = 0;
		obj->prev_quadrant = 0;
		obj->tx_scr_sr = 0;
		obj->tx_flags = (short)((obj->tx_flags & ~0x4018) | 0x2000);

		rx->rx_blocks = 0x21e;
		obj->short_382 = (short)0x8990;
		rx->flags = (unsigned short)((rx->flags & ~0x1d8) | 0x18);

		obj->vect_idx = 0;
		hs_put(obj, HS_TRACE_2, 0);

		preinitdigital(obj);

		rx->equ_step = 0x400;
		rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_PRECODE);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"V34RNEG, initialize RNEG, tx->txflags= 0x%x," "rx->rxflgs= 0x%x\n",
				(int)obj->tx_flags, (int)rx->flags);
		break;

	case 4:
		/*
		 * Modem-on-Hold.  `VPcmV34InitMOH` is the only caller.
		 */
		m[0xabe8] = 1;
		v34modeminit(obj);
		rx->agc_gain = rx->agc_start_gain;

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
			     (obj->role == 0x65) ? c2400_ : c1200_,
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
 * txmitquadbit -- but it is `V34scrambler`, which V34RX.c already has, and
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
 * probe -- 128 bytes at `.rodata + 0x2c00`, sixty-four signed shorts, and the
 * only table in the object that either line-probe transmit state reads.
 *
 * `v34handshak`'s txstate 51 `TX_L1` indexes it with the LOW SIX BITS of
 * `vect_idx`, scales each sample by `tx_scale` and shifts right by 14 -- so it
 * is one period of a waveform and the object plays it round and round.  Four
 * samples go out per pass of the per-sample loop and the segment ends when
 * `vect_idx` reaches 0x600 -- 1,536 samples, which is 384 passes and exactly
 * twenty-four periods.  src/pump/v34/v34hstx1.cpp is where that is written
 * and t_v34hstx1.c is where it compares.
 *
 * The values came out of `.rodata` BY TOOL, not read off a listing, and the
 * byte transcription remains proved by `t_v34hstx1.c`'s `memcmp` against
 * `ref_probe` -- the same check `vect4` and `vect16` get.  The independent
 * oracle there now derives the waveform from V.34 Table 17 as well: all
 * twenty-one frequencies and phases, the four omitted bins, equal tone
 * amplitudes, 24 repetitions and the L1/L2 power ratio.  Finding F10252.
 *
 * Two consequences of that derivation, verified over all sixty-four rather
 * than spotted, are that the array is EVEN about index 32
 * (`probe[32 + k] == probe[32 - k]` for every k in 1..31) and
 * `probe[32] == -probe[0]`.  The largest magnitude is 13,317, which is why
 * TX_L1's second loop can double a sample without a short overflowing.
 */
const short probe[V34_PROBE_SAMPLES] = {
	 13027,  13097,   7026,  -8421,  -2867,   3724, -12568,    251,
	  4448,  -9900,   7173,   -752, -11389,   9409,   9436,  10147,
	  5211,  -9736,   2748,  11799,   9862,   -547, -12548,   1980,
	   763, -13317,   3205,  10584,  -6029, -11058,  -4471,  -7261,
	-13027,  -7261,  -4471, -11058,  -6029,  10584,   3205, -13317,
	   763,   1980, -12548,   -547,   9862,  11799,   2748,  -9736,
	  5211,  10147,   9436,   9409, -11389,   -752,   7173,  -9900,
	  4448,    251, -12568,   3724,  -2867,  -8421,   7026,  13097,
};

/*
 * The scrambler generator, from the flag the two share.  Bit 0 of tx_flags set
 * selects the calling station's polynomial, which is V34scrambler's mode 0 --
 * so the sense is inverted between the two, and that is the object's.
 */
static short
tx_scrambler_mode(const struct v34_object *o)
{
	return (short)((o->tx_flags & V34_TXFLAG_CALLER) == 0);
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
	unsigned sr = (unsigned)o->tx_scr_sr;
	int d, q;

	d = V34scrambler(&sr, tx_scrambler_mode(o), bits, 2);
	o->tx_scr_sr = (int)sr;

	q = (d + (unsigned short)o->prev_quadrant) & 3;

	o->txpoint.word = vect4[q];
	o->cur_quadrant = (short)q;
	o->prev_quadrant = (short)q;

	txmit(obj);
}

/*
 * Scramble four bits and turn them into one of sixteen points.
 *
 * The two dibits go through the scrambler as two separate two-bit requests
 * rather than as one four-bit one -- which matters, because the register is
 * written back between them and the second request reads it.  Only the first
 * is differentially encoded; the second selects within the quadrant the first
 * chose, and `prev_quadrant` catches up to `cur_quadrant` only at the end.
 */
void
txmitquadbit(void *obj, short bits)
{
	struct v34_object *o = (struct v34_object *)obj;
	short mode = tx_scrambler_mode(o);
	unsigned sr = (unsigned)o->tx_scr_sr;
	int d, q;

	d = V34scrambler(&sr, mode, bits, 2);
	o->tx_scr_sr = (int)sr;
	q = (d + (unsigned short)o->prev_quadrant) & 3;
	o->cur_quadrant = (short)q;

	sr = (unsigned)o->tx_scr_sr;
	d = V34scrambler(&sr, mode, (short)(bits >> 2), 2);
	o->tx_scr_sr = (int)sr;

	q = (unsigned short)o->cur_quadrant;
	o->prev_quadrant = (short)q;
	o->txpoint.word = vect16[d + q * 4];

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
 * THE ROLE FLAG SPLITS IT IN TWO.  With `role == 0x65` this is the
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
/*
 * Default 1 for the differential tier, 0 for everyone else.  See the note in
 * probe_preemp() for why this is a flag with a define-set default rather than
 * an #ifndef around the call site.
 */
int dsplib_v34_blob_preemp =
#ifdef DSPLIB_REPRODUCE_BUGS
	1;
#else
	0;
#endif

#define PROBE_FIT_MAX		32	/* bins 1..25, comfortably */
#define PROBE_NOISE_SLOT(i)	((i) == 5 || (i) == 7 || (i) == 11 || (i) == 15)

/*
 * ---------------------------------------------------------------------------
 * `probe_preemp_shape` -- choose the filter whose SHAPE fits the channel.
 *
 * WHAT IS WRONG WITH THE COUNTER BELOW.  The object reduces the channel to a
 * two-point tilt.  A tilt is one number, and only one of the two filter
 * families IS a tilt.  Findings F1956 and F1957:
 *
 *   Figure 1/V.34, indices 0-5:  a straight line, 0 dB at f/S = 0 rising to
 *                                alpha at f/S = 1.0.  A BROADBAND tilt.
 *   Figure 2/V.34, indices 6-10: 0 dB out to f/S = 0.4, a free transition to
 *                                beta at 0.8, then linear to beta + gamma at
 *                                1.2.  A TOP-OF-BAND shelf.
 *
 * Two shapes, and the object measures a tilt in Table 3's own 2 dB quantum
 * (the multiplier works out at 2.03 dB in power) and then returns 6 + steps,
 * which indexes Table 4.  It also cannot return 0-5 at all: the counter starts
 * at 5 and is advanced before the test, so the reachable set is {6..10} and
 * the author's own `return 0` arm is dead (D53).
 *
 * So this scores ALL ELEVEN templates against the measured bins and returns
 * the one with the smallest residual.  A channel with real broadband tilt gets
 * a Table 3 answer; a flat-then-cliff channel gets Table 4; the magnitude comes
 * from the fit rather than from a counter.
 *
 * THE RESIDUAL IS A VARIANCE, NOT A MEAN-SQUARE, deliberately.  Pre-emphasis
 * cannot change the far end's overall transmit power -- only its shape -- so a
 * constant offset between the corrected channel and flat is not an error the
 * filter is able to fix, and including it would rank every template by how
 * much gain it happens to add.  Subtracting the mean scores flatness alone.
 *
 * ON THIS BENCH IT IS WORTH ALMOST NOTHING, and that is measured rather than
 * hoped: the path is flat to +/-0.4 dB from 450 to 3150 Hz, so there is no tilt
 * to correct, and its one real defect is a band-edge cliff far beyond the 7.5 dB
 * the largest template offers.  The gap between the object's choice and the
 * best available is about 0.13 dB (1957).  This is here for CORRECTNESS and for
 * lines that have a tilt; do not expect it to move a rate on the ATA path.
 */
#define PROBE_BIN_HZ	150.0

/* Table 2/V.34 carrier ratio d/e; the band is (d/e -+ 0.45) normalised. */
static double
probe_de_ratio(short baud)
{
	switch (baud) {
	case 2400:			return 2.0 / 3.0;
	case 2743: case 2800: case 3000: return 3.0 / 5.0;
	default:			return 4.0 / 7.0;   /* 3200, 3429 */
	}
}

static double
probe_template_db(int idx, double fs)
{
	static const double alpha[6] = { 0.0, 2.0, 4.0, 6.0, 8.0, 10.0 };
	static const double bg[5][2] = { { 0.5, 1.0 }, { 1.0, 2.0 },
					 { 1.5, 3.0 }, { 2.0, 4.0 },
					 { 2.5, 5.0 } };
	double beta, top;

	if (idx <= 5)
		return alpha[idx] * fs;
	beta = bg[idx - 6][0];
	top  = beta + bg[idx - 6][1];	/* NOT gamma -- the arrows stack */
	if (fs <= 0.4)
		return 0.0;
	/*
	 * 0.4 to 0.8 carries NO tolerance band in Figure 2, so the
	 * Recommendation constrains only the endpoints and any reasonable
	 * monotonic shape conforms.  Linear is a choice, not a reading.
	 */
	if (fs <= 0.8)
		return beta * (fs - 0.4) / 0.4;
	if (fs >= 1.2)
		return top;
	return beta + (top - beta) * (fs - 0.8) / 0.4;
}

static short
probe_preemp_shape(const struct v34_dftbin *bins, unsigned edge, short baud)
{
	double y[PROBE_FIT_MAX], fsv[PROBE_FIT_MAX];
	double de, lo, hi, bestvar = 0.0;
	unsigned i, cnt = 0;
	int idx, best = -1;

	if (baud <= 0)
		return -1;
	de = probe_de_ratio(baud);
	lo = de - 0.45;
	hi = de + 0.45;

	for (i = 1; i <= edge && cnt < PROBE_FIT_MAX; i++) {
		unsigned e;
		int b = 0;
		double fs;

		if (PROBE_NOISE_SLOT(i))
			continue;
		if (bins[i].energy <= 0)
			continue;
		/* bins[] is 0-based on 150 Hz: bins[i] is (i+1)*150 Hz. */
		fs = ((double)(i + 1) * PROBE_BIN_HZ) / (double)baud;
		if (fs < lo || fs > hi)
			continue;
		e = (unsigned)bins[i].energy;
		while (e >> (b + 1))
			b++;
		/* 10*log10 -- energy is a POWER quantity (1909). */
		y[cnt] = 3.0102999566
		       * ((double)b + ((double)e / (double)(1u << b) - 1.0))
		       - 3.0102999566 * (double)bins[i].shift;
		fsv[cnt] = fs;
		cnt++;
	}
	if (cnt < 6)
		return -1;			/* caller falls back */

	/*
	 * DROP NARROWBAND FEATURES; KEEP THE CHANNEL'S GENERAL SHAPE.
	 *
	 * Pre-emphasis exists to match the broad shape of the line.  Residual
	 * resonance and per-bin error are the EQUALISER's job -- it has 80
	 * complex taps and adapts every symbol, which is the right tool for a
	 * notch a few hundred hertz wide.  So the selector must not chase an
	 * isolated bin.  Before this existed, one corrupted bin moved the
	 * answer by EIGHT indices (a +12 dB interferer) and TEN (a -18 dB
	 * notch), which is worse than the object's two-point counter manages.
	 * The counter is accidentally robust: it reads two of twenty-five
	 * bins, so an interferer usually misses it entirely.
	 *
	 * THE DISCRIMINATOR IS NARROW VERSUS BROAD, not up versus down.  An
	 * earlier version rejected only upward outliers, on the reasoning that
	 * an interferer adds energy and a channel defect removes it.  That is
	 * true and it is the wrong rule: a narrow NOTCH is just as much the
	 * equaliser's problem as a narrow tone, and chasing it with a
	 * broadband filter is exactly the mistake.
	 *
	 * A three-point median over the level-vs-bin sequence does it: an
	 * isolated impulse of either sign vanishes, and a monotone edge --
	 * a band-edge cliff, a tilt -- survives untouched.
	 *
	 * THE ENDPOINTS ARE LEFT RAW, and that was measured rather than
	 * assumed.  Filtering them buys immunity to an isolated tone AND notch
	 * at the band edge, and BREAKS the identity property at 2400 baud:
	 * for a monotone ramp median(v0,v1,v2) == v1, so filtering an endpoint
	 * pulls a ramp's end inward and distorts the very shapes the templates
	 * are.  At 2400 there are ten in-band bins, so damaging two is a fifth
	 * of the evidence, and the selector could no longer name templates 5,
	 * 7, 8 or 10.  Correctness first: identity is what makes this worth
	 * having, edge immunity is a hardening.
	 *
	 * KNOWN LIMIT: an isolated bad bin EXACTLY at a band edge can still
	 * move the answer.  The interior bins are immune.  A fix needs an
	 * endpoint rule that preserves ramps -- a linear-extrapolation guard
	 * rather than a median -- and is not written.
	 *
	 * KNOWN LIMIT: a three-point median removes runs of ONE bin; two
	 * adjacent corrupted bins survive.  Widening to five would catch those
	 * and start blurring genuinely narrow channel features, which is a
	 * trade this deliberately does not make -- a real two-bin defect is
	 * 300 Hz wide and is a channel, not an interferer.
	 */
	if (cnt >= 3) {
		double sm[PROBE_FIT_MAX];
		unsigned a;

		sm[0] = y[0];
		sm[cnt - 1] = y[cnt - 1];
		for (a = 1; a + 1 < cnt; a++) {
			double p = y[a - 1], q = y[a], r = y[a + 1], t;

			if (p > q) { t = p; p = q; q = t; }
			if (q > r) { t = q; q = r; r = t; }
			if (p > q) { t = p; p = q; q = t; }
			sm[a] = q;			/* median of three */
		}
		for (a = 0; a < cnt; a++)
			y[a] = sm[a];
	}

	for (idx = 0; idx <= 10; idx++) {
		double s = 0.0, ss = 0.0, m, var;

		for (i = 0; i < cnt; i++) {
			double c = y[i] + probe_template_db(idx, fsv[i]);

			s += c;
			ss += c * c;
		}
		m = s / (double)cnt;
		var = ss / (double)cnt - m * m;
		if (var < 0.0)
			var = 0.0;
		if (best < 0 || var < bestvar) {
			bestvar = var;
			best = idx;
		}
	}

	/*
	 * VARIANCE, not RMS.  Printing an RMS would mean sqrt(), and the
	 * datapump does not otherwise link libm -- adding that dependency for
	 * one diagnostic would be a poor trade.  The number is in dB^2 and is
	 * for ranking arms against each other, not for quoting.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PREEMPHASIS, - SHAPE index %d over %d "
				     "bins, var %d.%02d dB2, baudrate= %d\n",
				     best, (int)cnt, (int)bestvar,
				     (int)((bestvar - (int)bestvar) * 100.0),
				     (int)baud);
	return (short)best;
}

static short
probe_preemp(const struct v34_dftbin *bins, unsigned n, int k, short baud)
{
	short ref = bins[PROBE_REF_BIN].energy;
	short x = bins[n].energy;
	short i = 5;

	/*
	 * SHAPE FIRST, then the object's counter.  Ordered most-informed to
	 * least: the shape matcher uses every usable bin and both filter
	 * families, the counter uses two bins and half of one family.  The
	 * matcher falls through to the counter when it has too little to work
	 * with.
	 *
	 * DELIBERATE FIX, AND IT IS THE DEFAULT.  D53: the object's counter
	 * starts at 5 and is advanced before its test, so indices 0-5 are
	 * unreachable and it cannot ask for a flat line on a flat channel --
	 * it asks for 6 or 7 and ADDS 1.5-3 dB of tilt (finding F1961 measures
	 * this as the right answer at every symbol rate below 3429 on the
	 * bench's own ATA).  Rejecting five of the eleven filters is a defect,
	 * not a behaviour to preserve, so it follows this tree's rule for
	 * every deliberate fix: `DSPLIB_REPRODUCE_BUGS` restores the object,
	 * the differential tier defines it and stays bit-exact, and everything
	 * else -- the interop tier, the bench, anyone linking this for real --
	 * gets the fix.  See docs/deviations.md D53 and `FPM_div`'s table for
	 * the same shape.
	 *
	 * A RUNTIME FLAG WHOSE DEFAULT THE DEFINE SETS, rather than an
	 * `#ifndef` around the call.  That was tried and is wrong here: this
	 * tree has ONE compilation rule (`$(BUILD)/%.o`) and it passes
	 * `$(REPRODUCE)` to everything, so a compile-time exclusion removes
	 * the arm from the bench hybrid as well as from the differential tier
	 * -- the binary that most needs it.  It was removed silently and the
	 * first emulated call after the change ran the object's counter with
	 * no SHAPE line in the log at all.
	 *
	 * So: `dsplib_v34_blob_preemp` defaults to 1 under
	 * DSPLIB_REPRODUCE_BUGS and 0 otherwise.  The differential tier builds
	 * with DSPLIB_REPRODUCE_BUGS, so it keeps the object's counter and
	 * stays bit-exact; every other build gets the shape matcher.
	 *
	 * The bench overrides it at run time from DSPLIB_V34_BLOB_PREEMP, via
	 * a `tools/benchflags.c` that is linked only into the bench hybrid.
	 * THAT FILE IS NOT ON master -- it lives on `v34-instrumentation` with
	 * the rest of the V.34 bench instrumentation, so the override exists
	 * only in a bench build made from that branch.  The variable is
	 * declared here rather than there precisely so that removing the bench
	 * cannot change what this function does.
	 */
	/*
	 * Bench-only actuator for measuring the FAR transmitter's real Table 3/4
	 * responses.  The selector is normally estimating the channel and asking
	 * the remote modem for a filter; this hook holds that request fixed so an
	 * HSF/Smart Link shared-channel sweep can distinguish a selector-model
	 * error from equaliser convergence.  It is deliberately compile-time only:
	 * production and the differential tree retain their existing behaviour.
	 */
#ifdef DSPLIB_V34_TEST_PREEMP_INDEX
# if DSPLIB_V34_TEST_PREEMP_INDEX < 0 || DSPLIB_V34_TEST_PREEMP_INDEX > 10
#  error "DSPLIB_V34_TEST_PREEMP_INDEX must be in 0..10"
# endif
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PREEMPHASIS, - TEST index %d, baudrate= %d\n",
				     DSPLIB_V34_TEST_PREEMP_INDEX, (int)baud);
	return DSPLIB_V34_TEST_PREEMP_INDEX;
#endif

	if (!dsplib_v34_blob_preemp) {
		short f = probe_preemp_shape(bins, n, baud);

		if (f >= 0)
			return f;		/* else fall through */
	}

	for (;;) {
		x = (short)(((int)x * k) >> 14);
		/*
		 * D53.  Advancing the counter HERE, before the test below, is
		 * what makes `i == 5` impossible and index 0 unreachable.  The
		 * object does it in this order and so does this.
		 */
		i = (short)(i + 1);

		if (x > ref) {
			if (i == 5) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V34PREEMPHASIS, - index is 0, " "baudrate= %d\n", (int)baud);
				return 0;
			}
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V34PREEMPHASIS, - index is %d, " "baudrate= %d\n", (int)i, (int)baud);
			return i;
		}
		/*
		 * NO SECOND INCREMENT HERE, and that is the whole of D53: the
		 * counter is advanced only above, before the test, so `i == 5`
		 * cannot hold and the author's own `return 0` arm is dead.
		 *
		 * A variant that advanced here instead -- making index 0
		 * reachable -- was written, measured over forty calls across two
		 * A/B runs, and REMOVED from master.  It shifts every other
		 * bucket down one, which finding F1477 shows is wrong on the
		 * Recommendation's own terms (V.34 5.4.1 puts indices 6-10 in
		 * Table 4 and this counter addresses that range deliberately),
		 * and finding F1901 measured it as no better and probably worse.
		 * It lives on `improve/v34-training`.
		 */
		if (i > 9) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V34PREEMPHASIS, - index is 10, " "baudrate= %d \n", (int)baud);
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
mp_put_preemp(short *msg, short idx)
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
	short g = rx->agc_start_gain;
	int i;

	for (i = 0; i < n; i++)
		g = (short)(((int)g * 0x47cf + 0x2000) >> 14);

	rx->agc_start_gain = g;
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
		dsplibs_debug_printf("V34PROBE, asking for a power reduction " "of %d\n", n);
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
				     "is %d\n", (int)rx->agc_start_gain);
	}

	probe_backoff(rx, req);

	if (rx->agc_start_gain > 0x1b58)
		rx->agc_start_gain = 0x1b58;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PROBE, agc gainestimate due to power "
				     "reduction request is %d\n",
				     (int)rx->agc_start_gain);
	goto band_edges;

not_asking:
	if (DSPLIB_DEBUG_ON()) {
		dsplibs_debug_printf("V34PROBE, not asking for power " "reduction\n");
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
		idx = probe_preemp(bins, 22, 0x6626, 0xd65);
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
	idx = probe_preemp(bins, 22, 0x6626, 0xd65);
	*pe3429 = idx;
	mp_put_preemp(msg, idx);
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
		idx = probe_preemp(bins, 20, 0x639f, 0xc80);
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
		idx = probe_preemp(bins, 20, 0x639f, 0xc80);
		*pe3200 = idx;
		mp_put_preemp(msg, idx);
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

		idx = probe_preemp(bins, 19, 0x656f, 0xbb8);
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
		idx = probe_preemp(bins, 19, 0x656f, 0xbb8);
		*pe3000 = idx;
		mp_put_preemp(msg, idx);
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
		idx = probe_preemp(bins, 18, 0x6789, 0xaf0);
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
	idx = probe_preemp(bins, 18, 0x6789, 0xaf0);
	*pe2800 = idx;
	mp_put_preemp(msg, idx);
	return;

rate_2400:
	if (role == 0x65) {
		mp_or(msg, 2, 0x24);
		idx = probe_preemp(bins, 18, 0x7da7, 0x960);
		*pe2400 = idx;
		mp_put_preemp(msg, idx);
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
	idx = probe_preemp(bins, 18, 0x7da7, 0x960);
	*pe2400 = idx;
	mp_put_preemp(msg, idx);
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

	obj->tx_pwr_reduction = (short)bitreverse((unsigned short)((w >> 5) & 7), 3);

	extra = (short)bitreverse((unsigned short)((w >> 2) & 7), 3);
	if (extra > 3)
		extra = 3;

	obj->tx_pwr_reduction = (short)(extra + (unsigned short)obj->tx_pwr_reduction);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34TXSCALE, power reduction requested "
				     "by remote modem is %d dB\n",
				     (int)obj->tx_pwr_reduction);

	if (obj->v90_receiver != 0) {
		if (minpr < 0)
			obj->tx_pwr_reduction = (short)(minpr
					     + (unsigned short)obj->tx_pwr_reduction);
		else if (obj->tx_pwr_reduction < minpr)
			obj->tx_pwr_reduction = minpr;
	}
	want = obj->tx_pwr_reduction;

	if (want < 0) {
		for (n = want; n < 0; n = (short)(n + 1))
			scale = (short)((scale * 0x47cf) >> 14);
	} else {
		for (n = 0; want > n; n = (short)(n + 1))
			scale = (scale * 0x390a) >> 14;
	}

	/*
	 * `tx_scale` is RE-READ here rather than kept: this prints the scale as
	 * it was on entry, and the store below is what changes it.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34TXSCALE, txscale before is %d, "
				     "reduced txscale is %d dB," "final txscale is %d\n",
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

	rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_FIR);

	hs_setstate(obj, HS_RXSTATE, V34HS_WAIT);
	hs_setstate(obj, HS_TXSTATE, V34HS_SSEG);

	obj->seg_symcount = 0;
	obj->tx_flags = (short)(obj->tx_flags | V34_EC_FEED);

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
 * canceller not armed to begin with, `far_echo_enable` is simply cleared.  Otherwise a
 * delay of 30 or more normalises it to 1 and leaves it on, and a delay below
 * that turns it off AND pulls the DMA delay at +0x25c back by `delay + 15`,
 * capped at 30 -- the object's own words, "RTD (%d) lower than min (%d),
 * masking Far EC..." and "...Modifying dma delay from %d to %d".
 *
 * The ring is cleared at the LITERAL offset +0x35b8, not through `bulk_ring`
 * at +0x35b0.  That is the object's: the pointer is never loaded here.
 */
static void
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
	    && obj->far_echo_enable != 0) {
		if ((short)d > 0x1d) {
			obj->far_echo_enable = 1;
		} else {
			int back;

			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "RTD (%d) lower than min (%d), " "masking Far EC...\r\n", d, 0x1e);

			back = (short)(d + 15);
			obj->far_echo_enable = 0;
			if ((short)back > 0x1e)
				back = 0x1e;

			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "...Modifying dma delay from %d to %d\r\n",
				    obj->dmadelay, obj->dmadelay - back);

			obj->dmadelay = (short)((unsigned short)obj->dmadelay - back);
		}
	} else {
		obj->far_echo_enable = 0;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34 bulk delay estimation %d (FAR=%d)\n",
				     d, obj->far_echo_enable);
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
 * time and finding F325 is what one collided macro cost.
 *
 * They are offsets rather than struct members because most of them land in
 * an `unmapped_*` pad today (tools/whichfield.py says which), and the two
 * members that are mapped -- `rtd` and the receiver's `flags`, `agc_level`,
 * `agc_gain`, `report_interval`, `rx_samples` -- are used by name below.
 */
#define T3C_TIMER_LO	0x0238	/* int:   a running sample count            */
#define T3C_TIMER_HI	0x023c	/* int:   that plus 431,488                 */
#define T3C_LVL_COUNT	0x0234	/* int:   blocks below it, capped at 0x257f */
#define T3C_MODE	0x2218	/* int:   selects what the tail reports     */
#define T3C_DETECTOR	0x3564	/* struct v34_detector, inside the object   */
#define T3C_F358C	0x358c	/* short: cleared by MOH_TONE's body        */
#define T3C_FSKGATE	0xa8a0	/* int:   non-zero diverts at 0x64a87       */
#define T3C_BLK_A94C	0xa94c	/* what +0xaa6c is aimed at                 */
#define T3C_BLK_A97C	0xa97c	/* what +0xaa70 is aimed at                 */
#define T3C_COUNT	0xaa78	/* short: the counter, and HS_TRACE_2       */
#define T3C_COUNT_SRC	0xaa7c	/* short: RX_PHASE3_CALL copies it in       */
#define T3C_PTR_AA6C	0xaa6c
#define T3C_PTR_AA70	0xaa70
/*
 * THE ONE OFFSET HERE THAT IS NOT A MISSING FIELD.  +0xaae2 IS `fsk.sr` and
 * every other reader in this file now spells it that way -- but 0x65c8a reads
 * it a BYTE wide (`movzbl`) to test bit 0, and `obj->fsk.sr & 1` is a
 * `movzwl`.  The two agree on every value the field can hold, on a
 * little-endian machine, so no differential test can tell them apart; a
 * load's width is something the compiler was FORCED to encode, so widening
 * it would be a codegen regression that nothing in this tree could catch.
 * The offset stays for that one read.  Finding F553.
 */
/*
 * +0x356a IS THE DETECTOR'S OWN `armed`, AND THE OBJECT SAYS SO RATHER THAN
 * THE STRUCTURE DOING.  +0x3564 is the base `detectorinit` is handed one
 * statement earlier, `struct v34_detector`'s third field sits at +6, and
 * 0x3564 + 6 is 0x356a -- so both writers of this offset (0x6d5db here and
 * 0x6ec4a in microstate 44, which the tree already carries as `T44_F356A`)
 * are arming the detector they have just built.
 *
 * IT IS STILL AN OFFSET AND NOT A FIELD ACCESS.  v34fsk.h declines to embed
 * the detector at +0x3564 on purpose -- two things meeting is adjacency, not
 * a bound (findings F215 and F630) -- and reaching through a cast here would
 * override that decision from the far side.  The name records what it is;
 * the access stays as the tree writes every other offset.  Finding F751.
 */
#define T3C_F356A	0x356a	/* short: the detector at +0x3564's `armed` */
#define T3C_FAAE2	0xaae2	/* THE LOW BYTE of `fsk.sr`; see above     */
#define T3C_FABE4	0xabe4	/* short: set to 1 on the disconnect path  */
#define T3C_FABE6	0xabe6	/* short: set to 1 on the retrain path     */
#define T3C_FABF8	0xabf8	/* byte:  "the drop has been reported"     */
#define T3C_FABF9	0xabf9	/* byte:  non-zero diverts at 0x6c8f8      */
#define T3C_FABFC	0xabfc	/* short: what the counter must reach      */
#define T3C_RX_SAMPS	0x010c	/* receiver: where a detector reads from   */

/*
 * THE LAST TWO STRICT-ALIASING WARNINGS IN THE TREE ARE THIS LINE, AND IT IS
 * A DOCUMENTED EXCEPTION RATHER THAN A SITE THAT WAS MISSED.  Findings F5300
 * and 5305.
 *
 * It is not one of the three shapes the other 25 were.  Those were a field
 * declared narrower than the object writes it, and the object's instruction
 * width said what the declaration should have been.  This is a WHOLE STRUCT
 * overlaid on another, and there is no width to read: `struct v34_receiver`
 * really is a sub-object of `struct v34_object` at +0x264, and the correct
 * model is to EMBED it there rather than to reinterpret the storage.
 *
 * The base is established, not guessed -- v34fsk.h's note on `short_382` says "as
 * a `struct v34_receiver` offset this is +0x11e", and 0x382 - 0x11e is 0x264,
 * which is `rxq`.  What blocks the fix is that the two headers model the same
 * 0x120 bytes twice and disagree about them: `v34_receiver::pad_000[0x120]`
 * against `v34_object`'s `rxq`, `rxq_ring_tail[63]`, `unmapped_0370` and
 * `short_382`.  Merging those is a batch with its own differential test and its
 * own offset assertions on both sides, and finding F3303 is the worked example
 * of getting a double-counted region wrong.
 *
 * WHAT MUST NOT HAPPEN IS RESPELLING IT AS `(char *)(obj) + 0x264` to match
 * the line below.  That silences the warning -- GCC stops seeing through a
 * `char *` -- and changes nothing about the access, which is exactly the
 * "papered over in `src/`" this tree forbids.  It stays visibly broken until
 * it is really fixed.
 */
#define T3C_RX(obj)	((struct v34_receiver *)&(obj)->rxq)
#define T3C_DET(obj)	((struct v34_detector *)((char *)(obj) + T3C_DETECTOR))

/*
 * ===========================================================================
 * WHAT FOLLOWS CAME FROM `v34hshak_t3mid.c`, WHICH NO LONGER EXISTS.
 *
 * `w4_hs_t3mid` branched before the skeleton landed and built a second,
 * self-contained reconstruction of this same 61,541-byte function -- its own
 * prologue, guards, rxstate chain, table-3 dispatch, tail and three of table
 * 2's arms (finding F348).  Both were differentially tested, by different
 * routes, so neither was dead code and neither could simply be deleted.
 * This is the merge: ONE `v34handshak`, forty of table 3's forty arms.
 *
 * The `T3M_*`/`t3m_*` prefixes are kept exactly as they were rather than
 * folded into this file's `T3C_*`.  They are not tidy, and that is the
 * point: 443 mutation anchors are written against this text character for
 * character, and a cosmetic rename here is 443 anchors to repair for no
 * measured gain.  Finding F548 records what the merge did change, and why
 * each change was forced.
 * ===========================================================================
 */

/*
 * ---------------------------------------------------------------------------
 * Offsets the header does not name.
 *
 * `struct v34_object` models about half of what these arms touch; the rest
 * lands in a `pad_*` or `unmapped_*` region, which `tools/whichfield.py`
 * reports as exactly that.  Reaching those by offset is this tree's existing
 * practice (src/pump/v34/v34pcmmain.cpp, src/pump/v34/v34k56.cpp) and is
 * preferred to widening the struct from one arm's reading of it.
 */
#define T3M_TICK		0x0234	/* int, and NOT `unmapped_0234` as a
					   whole: 0x62aa9 increments this one
					   32-bit word and 0x62ab3 compares it
					   against 0x257f                    */
#define T3M_ELAPSED		0x0238	/* int, the running sample count       */
#define T3M_DEADLINE		0x023c	/* int, ELAPSED plus 431,488; the two
					   are compared UNSIGNED at 0x62a84  */
#define T3M_MODE		0x2218	/* int, the value 0x62a47 dispatches
					   the tail on                       */
#define T3M_COUNTER		0xaa78	/* short, the counter six of these arms
					   bump; it is the `[2]` every trace
					   prints (V34hshak.c's HS_TRACE_2)  */
#define T3M_FSKGATE		0xa8a0	/* int, non-zero adds the retrain poll */
#define T3M_TOGGLE		0x358c	/* short, arm 48 inverts bit 0 of it   */

/*
 * The five fields arms 47 and 63 reach that nothing in this tree has named.
 * `fNNNN` rather than a description, deliberately: what each is FOR is not
 * something this batch measured, and a guessed name in the record is worse
 * than an offset (docs/findings.md's rule about decompiler-shaped names).
 * What IS measured is in the comment beside each use.
 */
#define T3M_F3588		0x3588	/* short, arm 47's second entry guard;
					   `v34handshakinit` also writes it  */
#define T3M_F358A		0x358a	/* short, set to 1 beside it           */
#define T3M_FABAE		0xabae	/* ten shorts arm 47 clears            */
#define T3M_FABC2		0xabc2	/* the eleventh, cleared separately    */
#define T3M_FABFF		0xabff	/* SIGNED byte, arm 63's third guard   */
#define T3M_F0240		0x0240	/* int, arm 63's fourth guard          */

/*
 * The fields arms 49, 50 and 51 add.  `filtdelay` is not a guess: it is what
 * the object's own diagnostic at 0x709d7 calls the third thing it prints,
 * "On RX_PHASE1_ANS: is short=%d, bulkDelay=%d, filtDelay=%d".  The rest are
 * offsets because what they are FOR was not measured here.
 */
#define T3M_FILTDELAY		0xaa7c	/* short; three of arm 49's and arm
					   50's four thresholds are it plus a
					   constant                          */
#define T3M_F35A0		0x35a0	/* short, arm 51 clears on both paths  */
#define T3M_F35A2		0x35a2	/* short, arm 51's is_short path only  */
#define T3M_INFOREC		0xa9ac	/* the THIRD of the five message
					   records at +0xa94c, 0x30 apart;
					   arm 51 hands it to
					   `V34SetINFO1aBits` and prints ten
					   of its halfwords                  */
#define T3M_SELFPTR		0xaa6c	/* arm 51 aims this at +0xa9ac and
					   then reads the record back through
					   it -- the harness knows it as one
					   of the two self-pointers          */
#define T3M_TXBAUD		0xaa84	/* short, the transmit baud rate       */
#define T3M_TXSCALE		0xaa90	/* const short *, the transmit power
					   scale                             */
#define T3M_TXCARRIER		0xaa94	/* short, the transmit carrier         */
#define T3M_RXCARRIER		0xaaa8	/* short, arm 51 copies the transmit
					   carrier here                      */
#define T3M_RXSCALE		0xaaac	/* const short *, and the scale        */
#define T3M_DETECTOR		0x3564	/* struct v34_detector, arm 51's       */

/* Within the receiver, and reached from `rx` and not from the object. */
#define T3M_RX_F264		0x0264	/* short, arm 49's last write          */

/*
 * The first of the five message records `V34hshak.c` names at +0xa94c ..
 * +0xaa3c -- twelve fields on a 0x30 stride, and `v34handshakinit`'s mode-2
 * body blanks the +0xaa0c one field for field.  Arm 47 fills this one.
 */
#define T3M_MSGREC0		0xa94c

/* Within the receiver. */
#define T3M_RX_FSKIN		0x010c	/* `fskdemodulate`'s second argument   */

/* The microstate table: 40 entries, index = microstate - 41. */
#define T3M_TBL3_FIRST		41
#define T3M_TBL3_COUNT		40

/* The once-per-block transmit table: 70 entries, index = txstate - 5. */
#define T3M_TBL2_FIRST		5
#define T3M_TBL2_COUNT		70

/*
 * Table 2's own companion field, and the only field it reads that nothing
 * else in this file does.  +0xe4c lands in `struct v34_object`'s
 * `unmapped_0404`, so an offset is the honest spelling -- finding F552's rule
 * for the seventy-nine it left alone.
 */
#define T3M_F0E4C		0x0e4c	/* unsigned short, txstate 70's       */

/*
 * ---------------------------------------------------------------------------
 * TWELVE OF THE OFFSETS ABOVE NOW NAME A FIELD, AND THE COMPILER HOLDS THE
 * TWO SPELLINGS TOGETHER.
 *
 * Task #33 measured `struct v34_object` where these macros land and turned
 * twelve spans into fields -- findings F630 to 636.  Not one use site changed,
 * and that was the point: `hs_get`, `hs_put` and `hs_setstate` take the
 * offset as a RUNTIME argument, because one function serving all three state
 * machines is what keeps the three format strings' argument orders in one
 * place (finding F632), so for the busiest fifty-nine of these there is no
 * field for a field access to name.  The offsets stay.
 *
 * Two spellings of one fact is two places to drift, and NOTHING ELSE IN THE
 * TREE COMPARES THEM.  `tools/offcheck.py` holds the header's per-field
 * offset annotations against the compiler and `tools/refcheck.py` holds the
 * prose, but a `#define` in a .c is invisible to both.
 *
 * The idiom is v34shell.c's `V34OB_ASSERT`, GUARD INCLUDED, and it is
 * repeated here rather than shared because these macros are here.  The guard
 * earns its keep twice: `make check64` compiles this file for a 64-bit target
 * where `paa6c` is eight bytes and every offset past it would be wrong, and
 * the period toolchain -- GCC 3.4.4, which is what `tools/toolchain` runs and
 * what this file must keep compiling under -- has neither `_Static_assert`
 * nor `__builtin_offsetof`, the latter arriving only in GCC 4.0.
 * `__SIZEOF_POINTER__` is GCC 4.3's, so it excuses both.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define HS_OFF_ASSERT(tag, field, off) \
	typedef char hs_off_##tag[ \
		((int)__builtin_offsetof(struct v34_object, field) == (off)) \
		? 1 : -1]

/*
 * The three state words -- BUT ONLY IN v34hshak.h's SPELLING, and the three
 * `HS_MICROSTATE`/`HS_RXSTATE`/`HS_TXSTATE` above are deliberately NOT here.
 *
 * They were, for one run.  `test/mutations/v34hshak.json` already carries
 * "rxstate and txstate offsets transposed", which rewrites `#define
 * HS_RXSTATE 0x3594` to 0x3596, and the recorded verdict is CAUGHT -- a
 * differential test tells the two machines apart.  Asserting the same fact
 * here turned that mutant into one that does not COMPILE, and the snapshot
 * caught the change as `caught -> unusable`.
 *
 * An unusable mutation does not fail a run (finding F347) and is the silent
 * loss `tools/mutsnap.py` exists to make visible, so trading a measured
 * guarantee for a tautological one is a bad trade even when the tautology is
 * checked earlier.  The split is therefore by who already covers what:
 * V34hshak.c's own three macros are covered by that mutation, and
 * v34hshak.h's three -- which every one of the fifty-nine call sites passes
 * and which no mutation touches -- are covered here.  Finding F637.
 */
HS_OFF_ASSERT(hdr_micro,   microstate, V34HS_MICROSTATE_OFF);
HS_OFF_ASSERT(hdr_rxstate, rxstate,    V34HS_RXSTATE_OFF);
HS_OFF_ASSERT(hdr_txstate, txstate,    V34HS_TXSTATE_OFF);

/* The counter every trace prints as `[2]`, under both of its names. */
HS_OFF_ASSERT(trace2,      short_aa78,      HS_TRACE_2);
HS_OFF_ASSERT(t3m_counter, short_aa78,      T3M_COUNTER);
HS_OFF_ASSERT(t3c_count,   short_aa78,      T3C_COUNT);

/* `filtdelay`, which the object's own diagnostic at 0x709d7 names. */
HS_OFF_ASSERT(filtdelay,   filtdelay,  T3M_FILTDELAY);
HS_OFF_ASSERT(count_src,   filtdelay,  T3C_COUNT_SRC);

/*
 * The two DFT banks rxstate 72's arm measures across.
 *
 * The second one is why this pair is here: `nl_noise_bins` was
 * `unmapped_a76c` until that arm read it, and what fixes it at 0xa76c is the
 * TWENTY-FIVE bins before it -- so a change to `V34_PROBE_BINS`, to
 * `struct v34_dftbin`'s size or to anything between them silently moves the
 * noise bank onto the probe's last bins and the arm goes on compiling.
 * 0xa76c + 4 * 0x2c = 0xa81c, which is `retrain_bins`, so the region holds
 * exactly four and there is no slack to absorb a mistake.  Finding F739.
 */
HS_OFF_ASSERT(probe_bins,  probe_bins,    0xa320);
HS_OFF_ASSERT(nl_noise,    nl_noise_bins, 0xa76c);

/* The pair at +0x3588 that two sites read 32 bits wide (finding F631). */
HS_OFF_ASSERT(short_3588,       short_3588,      T3M_F3588);
HS_OFF_ASSERT(short_358a,       short_358a,      T3M_F358A);

/* The toggle, whose sign the table index at 0x62d48 forces. */
HS_OFF_ASSERT(toggle,      short_358c,      T3M_TOGGLE);
HS_OFF_ASSERT(t3c_f358c,   short_358c,      T3C_F358C);

/* The two self-pointers, and the ten shorts arm 47 clears. */
HS_OFF_ASSERT(ptr_aa6c,    paa6c,      T3C_PTR_AA6C);
HS_OFF_ASSERT(selfptr,     paa6c,      T3M_SELFPTR);
HS_OFF_ASSERT(ptr_aa70,    paa70,      T3C_PTR_AA70);
HS_OFF_ASSERT(short_abae,       short_abae,      T3M_FABAE);
HS_OFF_ASSERT(short_abc2,       short_abc2,      T3M_FABC2);

/*
 * The two Modem-on-Hold fields the disconnect at 0x6c8f8 and 81's wrap at
 * 0x66d85 reach through a NAME rather than an offset.  `moh_org` is new with
 * that wrap and splits `unmapped_abe4`, so the assert is the thing that says
 * the split landed where the `cmpl $0x1,0xabec` did; `short_abe2` was already
 * declared and is asserted beside it because the disconnect writes the two
 * together and a shift in either would move both.
 *
 * +0xabe4's own store has no field to assert -- it is still inside
 * `unmapped_abe4` -- and `T3C_FABE4` is an offset for that reason.
 */
HS_OFF_ASSERT(short_abe2,       short_abe2,      0xabe2);
HS_OFF_ASSERT(moh_org,       moh_org,      0xabec);
#endif	/* 32-bit target with a compiler that has __builtin_offsetof */

/*
 * ---------------------------------------------------------------------------
 * The paths not written.
 *
 * Forty of table 3's forty arms are here and all seven of table 2's targets
 * are, but thirty-nine of table 1's are not, and "nothing" is the one answer
 * a differential test cannot tell from a wrong answer -- the object would
 * simply come back unmodified and the comparison would report whatever the
 * blob wrote.  So every unwritten path names itself here.
 *
 * A CODE AND NOT A STRING, and that is the strings firewall's doing rather
 * than a preference: `tools/debugaudit.py --invented` holds every literal in
 * src/ against the object's .rodata and .data, so a diagnostic phrase this
 * tree made up cannot live here at all (findings F180 and F201).  The names are
 * in the test, which is where an invented string belongs.
 *
 * AND IT BOTH RECORDS AND STOPS.  The two reconstructions this file was
 * assembled from did that differently and only one of the two could survive
 * the merge intact, so neither did: `V34hshak.c`'s `t3c_unwritten` called
 * `abort()`, on the argument that an arm returning quietly is
 * indistinguishable from an arm that correctly did nothing, and
 * `v34hshak_t3mid.c` recorded a code and returned, because a test that dies
 * cannot then be asked WHICH path it reached and `t_v34hst3mid.c` asks after
 * every step.  Both arguments hold.  So the code is ALWAYS recorded and the
 * stop is what a test opts out of, by name: `v34handshak_unwritten_reset`
 * says "I am going to read the code afterwards", and only a test that has
 * said so gets a return instead of an abort.  Finding F547.
 */
static int t3m_unwritten;
static int t3m_unwritten_soft;

int
v34handshak_unwritten(void)
{
	return t3m_unwritten;
}

void
v34handshak_unwritten_reset(void)
{
	t3m_unwritten = T3M_WRITTEN;
	t3m_unwritten_soft = 1;
}

static void
t3m_notwritten(int what)
{
	if (t3m_unwritten == T3M_WRITTEN)
		t3m_unwritten = what;
	if (!t3m_unwritten_soft)
		abort();
}

/*
 * ---------------------------------------------------------------------------
 * The four things the prologue computes once and every arm and the tail use.
 *
 * 0x628f0 puts them in four stack slots and they are live for the whole
 * function: 0x74(%esp) is the receiver, 0x78(%esp) is `&obj->progress`, and
 * 0x4c(%esp) is `obj + 0x221c`.  `%esi` holds the microstate from 0x64abc.
 *
 * `0x78(%esp)` matters more than it looks.  Every "progress code" store in
 * the tail is `movl $n,(%esi)` through it, and every timer field the tail
 * reads is an offset from it -- so `0x234(%esi)` is the object's +0x238 and
 * not its +0x234.  Finding F179 quoted the register-relative offsets and was
 * wrong by four for exactly this reason; V34hshak.c's `v34handshakinit`
 * comment records it.
 */
struct t3m_frame {
	struct v34_object	*obj;
	unsigned char		*m;	/* the object as bytes             */
	struct v34_receiver	*rx;	/* obj + 0x264                     */
	int			*progress;	/* &obj->progress             */
	short			mst;	/* the microstate the dispatch read*/
	/*
	 * `obj->fsk.nbits` AS IT WAS BEFORE `fskdemodulate` RAN.  0x64a9e
	 * loads it into `%ebx`, which is callee-saved and so survives the
	 * call, and arm 55's first guard at 0x65b79 compares the field against
	 * it sixteen bits wide.  It is the only thing either arm added here
	 * needs that is not in the object, and it is why this frame carries
	 * something that is not a pointer or a state word.
	 */
	short			nbits;
};

#define T3M_I32(f, off)		(*(int *)((f)->m + (off)))
#define T3M_U32(f, off)		(*(unsigned *)((f)->m + (off)))
#define T3M_I16(f, off)		(*(short *)((f)->m + (off)))
#define T3M_U16(f, off)		(*(unsigned short *)((f)->m + (off)))

/*
 * The prologue's four stack slots, in the one place that fills them.  Every
 * caller of `t3m_txblock` outside `v34handshak` itself -- the forty-odd
 * `t3c_txblock` sites in the arms below -- reaches the dispatch without
 * having gone through the prologue, so it needs one of these too.  `nbits`
 * is zero there and is not read on that path: only arms 55 and 58 read it,
 * and both are entered from the prologue.
 */
static void
t3m_frame_init(struct t3m_frame *f, struct v34_object *obj)
{
	f->obj = obj;
	f->m = (unsigned char *)obj;
	f->rx = T3C_RX(obj);
	f->progress = &obj->progress;
	f->mst = 0;
	f->nbits = 0;
}

/*
 * ---------------------------------------------------------------------------
 * 0x62a40 -- the tail, and the only `ret`.
 *
 * Eighty-eight instructions with no exit but its own four returns, which is
 * what makes a per-arm reconstruction possible at all: every microstate arm
 * and every transmit arm ends here, so writing it once covers all of them.
 *
 * `tx` IS A PARAMETER AND NOT `obj->txstate`.  The value in `%cx` arrives
 * from whichever arm jumped here and two comparisons read it -- 0x62a70 and
 * 0x62ac5 -- while `obj + 0x3596` may hold something else entirely.  Arm 48's
 * threshold path is the case that separates them: it stores 5 into the object
 * and passes 5, but 0x62b5f below RE-READS the object into `%cx`, so the two
 * readings differ on one path out of four and modelling either one as the
 * other passes at txstate 18 and fails at txstate 5.
 */
static void
t3m_tail(struct t3m_frame *f, short tx)
{
	int mode = T3M_I32(f, T3M_MODE);
	int esi;

	if (mode == 1) {
		/*
		 * The block at 0x62b45: `baud_rate` is the receive baud rate and
		 * the receiver's +0x1d2 gets three times it, by
		 * `lea (%ebp,%ebp,2)` on the sign-extended halfword, stored
		 * back as one.  THE RELOAD IS THE NEXT INSTRUCTION, 0x62b5f
		 * `movzwl 0x3596(%ebx),%ecx`, and 0x62b66 writes the 4.
		 */
		f->rx->report_interval = (short)(3 * (int)f->obj->baud_rate);
		tx = (short)T3M_U16(f, V34HS_TXSTATE_OFF);
		*f->progress = 4;
	}

	/* 0x62a56 and 0x62a68: both windows are UNSIGNED, so mode 1 and any
	   negative mode fall through both. */
	if ((unsigned)(mode - 4) <= 1u)
		*f->progress = 6;

	if ((unsigned)(mode - 2) <= 1u && tx == 0x4a) {
		/*
		 * 0x64884.  `setg`/`dec`/`and $7` is 0 when `vect_idx` is
		 * above 60 and 7 when it is not -- the arithmetic is the
		 * compiler's way of writing a two-valued select and the 7 is
		 * not a mask of anything.
		 */
		*f->progress = f->obj->vect_idx > 0x3c ? 0 : 7;
	}

	/* 0x62a7a.  Unsigned, so an elapsed count past the deadline's wrap
	   is late rather than early. */
	if (T3M_U32(f, T3M_ELAPSED) > T3M_U32(f, T3M_DEADLINE))
		*f->progress = 8;

	/* 0x62a92.  Signed: the level is a sign-extended halfword. */
	esi = f->rx->agc_level;
	if (esi >= f->obj->rx_energy_floor)
		T3M_I32(f, T3M_TICK) = 0;
	else
		T3M_I32(f, T3M_TICK)++;

	if (T3M_I32(f, T3M_TICK) > 0x257f)
		*f->progress = 9;

	/* 0x62ac5, a three-way compare on the SIGN-EXTENDED halfword. */
	if (tx == 0x53)
		*f->progress = 0x0f;
	else if (tx > 0x53) {
		if (tx == 0x54)
			*f->progress = 0x10;
	} else if (tx == 0x52)
		*f->progress = 0x0d;
}

/*
 * ---------------------------------------------------------------------------
 * .rodata+0x2ee8 -- the once-per-block transmit dispatch at 0x62af1.
 *
 * SEVEN TARGETS OVER SEVENTY ENTRIES, and all seven are here.  Read out of
 * the object with their relocations attached (finding F360), the table's own
 * partition of txstates 5..74 is:
 *
 *        0x64480   5                       SILENCE
 *        0x64518   18 19                   SSEG SBARSEG
 *        0x64509   20 21 64 68             PPSEG TRNSEG4 JTXMIT J1TXMIT
 *        0x644c9   24 51 54 60 74          TX_DPSK TX_L1 SILENCEINFO
 *                                          TONE_AB SILENCERETRAIN
 *        0x644fa   66 67 69                TRNSEG4A XMITMP EXMIT
 *        0x644d8   70                      DATAXMIT
 *        0x62a40   the other fifty-four, and every txstate outside 5..74
 *
 * Six of the seven set the int at +0x0004 and fall into the seventh, which is
 * both the default arm and the shared tail.  So the tail runs on every path
 * and the six arms are, between them, a decision about ONE field.
 *
 * WHAT +0x0004 IS.  `struct v34_object`'s own note calls it "an int the shell
 * polls"; the shell's writers put 5, 6 and 10 in it.  This dispatch is the
 * other writer, and the eleven values it can leave are 0, 1, 2, 3, 4, 6, 7,
 * 8, 9, 0xd, 0xf and 0x10.  Nothing reconstructed reads it, so that is all
 * this dispatch claims about it -- the names above are offsets, not meanings.
 *
 * THE DOMAIN IS CLOSED, which is what made table 2 a batch of its own before
 * it was folded back in here.  The seven targets branch out of the table's
 * own address range five times -- 0x6778b, 0x655c9, 0x67d48, 0x6780f and
 * 0x64884 -- and every one of those five is a handful of instructions that
 * returns to the table's arms or to the shared tail.  Nothing in the whole
 * closure calls anything and nothing in it traces, which is why every
 * table-2 case prints zero lines on both sides and why the transcript axis of
 * the harness's comparison contributes nothing here.  Finding F362 records
 * that as a gap in the evidence rather than as a passing check; finding F360
 * is the closure.
 *
 * THREE ROUTES REACH IT and all three are the caller's business:
 *
 *      [obj+0x221c] >= [obj+0x2aa0]  and  [obj+0x264] <= 5      0x62ae3
 *      ... and [obj+0x264] > 5, rxstate < 43 and not 4 or 35    0x62a2a
 *      ... and rxstate > 43 and not 53 or 72                    0x62b83
 *
 * All three arrive at 0x62af1 with the same two registers holding the same
 * two values, so the three are one entry point; finding F361 measures that
 * rather than assuming it.
 *
 * THE BOUND IS NOT SEPARATELY OBSERVABLE ABOVE ITS TOP.  The object tests
 * `(unsigned)(txstate - 5) <= 0x45` and sends everything else to the default
 * arm, which is the same block the table's own fifty-four default entries
 * name -- so both sides of the bound reach 0x62a40 with the same txstate and
 * only the two EDGES, txstate 5 and txstate 74, can be told apart by their
 * arms.  The range test is kept because the object encodes it; finding F591
 * measures what that costs the mutation set.
 */
static void
t3m_txblock(struct t3m_frame *f, short tx)
{
	unsigned idx = (unsigned)((int)tx - T3M_TBL2_FIRST);

	if (idx >= T3M_TBL2_COUNT) {
		t3m_tail(f, tx);
		return;
	}

	switch ((int)tx) {
	case 5:				/* 0x64480 SILENCE */
		/*
		 * Three ways to reach progress 1, all of them a microstate
		 * and rxstate pair agreeing with the answer/originate flag
		 * at +0x359c, and every other way is 0.
		 */
		{
			unsigned short mst = T3M_U16(f, V34HS_MICROSTATE_OFF);
			unsigned short rx = T3M_U16(f, V34HS_RXSTATE_OFF);

			if (mst == 0x3f && f->obj->role == 0x66)
				*f->progress = 1;
			else if (rx == 0x04 && mst == 0x2c
				 && f->obj->role == 0x65)
				*f->progress = 1;
			else if (rx == 0x23 && mst == 0x3f
				 && f->obj->role == 0x65)
				*f->progress = 1;
			else
				*f->progress = 0;
		}
		break;

	case 18:			/* 0x64518 SSEG   */
	case 19:			/*          SBARSEG */
		/*
		 * `sbb`/`add $3` is 3 when bit 3 of the receiver's flags is
		 * set and 2 when it is not.  The originate side is 2 whatever
		 * the flag says.
		 */
		if (f->obj->role == 0x65)
			*f->progress = 2;
		else
			*f->progress = (f->rx->flags >> 3) & 1 ? 3 : 2;
		break;

	case 20:			/* 0x64509 PPSEG    */
	case 21:			/*         TRNSEG4  */
	case 64:			/*         JTXMIT   */
	case 68:			/*         J1TXMIT  */
		*f->progress = 2;
		break;

	case 24:			/* 0x644c9 TX_DPSK  */
	case 51:			/*         TX_L1    */
	case 54:			/*         SILENCEINFO */
	case 60:			/*         TONE_AB  */
	case 74:			/*         SILENCERETRAIN */
		*f->progress = 0;
		break;

	case 66:			/* 0x644fa TRNSEG4A */
	case 67:			/*         XMITMP   */
	case 69:			/*         EXMIT    */
		*f->progress = 3;
		break;

	case 70:			/* 0x644d8 DATAXMIT */
		/*
		 * `cmp $0x1,%bp` then `sbb %eax,%eax` and `add $0x4,%eax`:
		 * the borrow is set only when the halfword is zero.
		 */
		*f->progress = T3M_U16(f, T3M_F0E4C) == 0 ? 3 : 4;
		break;

	default:			/* 0x62a40, straight to the tail */
		break;
	}

	t3m_tail(f, tx);
}

/*
 * ---------------------------------------------------------------------------
 * The arms.
 *
 * Each is `static void t3m_micro<n>(struct t3m_frame *)` and each ends by
 * calling `t3m_txblock` with the value the object's `jmp 62af1` leaves in
 * `%cx`.  Nothing else may end an arm: the tail is the only `ret`.
 */

/*
 * 0x6cc1b, 0x70d7f, 0x66517, 0x660b9, 0x6c14a, 0x6fff7, 0x6bc0b, 0x70447 and
 * 0x6d709 -- ONE BODY IN NINE COPIES.
 *
 * Arms 47, 49, 50, 55, 58 and 59 each re-arm the error recovery when the FSK
 * demodulator has repeated an info0, and GCC emitted the same twenty-five
 * stores nine times: 47, 49, 50 and 58 once apiece and 55 and 59 twice each.
 * Compared instruction for instruction across the nine: the same fields in the
 * same values, the same two transitions in the same order, the same
 * conditional 0x1e, the same twelve-field record.  One copy here.
 *
 * WHAT IS NOT SHARED STAYS AT THE CALL SITE, and there are four such things:
 *
 *   - the GUARD.  47 wants the shift register's low four bits all ones, 49
 *     wants `sr & 0x3ff` exactly 0x372, 50 wants it above 0x200, 55 and 59
 *     want its low THREE bits all ones.  Four different tests of the same
 *     register.
 *   - the DIAGNOSTIC.  Six strings that differ only in the state they name,
 *     and 47's, 49's and 50's are printed after the record is filled while
 *     55's and 59's are printed before it.
 *   - the COUNTER.  47, 55 and 59 clear it and 49, 50 and 58 do not.  Putting
 *     that in here would make a mutation deleting 47's clear unfalsifiable.
 *   - `+0x3588`, which is why the body below starts one store later than 47's,
 *     49's and 50's did.  The nine copies disagree on exactly that word: 47,
 *     49, 50 and 58 store the constant 4 and the other five SET BIT 0 of
 *     whatever is there.  A helper that wrote 4 unconditionally would be wrong
 *     at five of the nine call sites, and one that took the value as an
 *     argument would hide the difference behind a parameter.  So
 *     `t3m_errrec_reset` is the `= 4` form and the callers that want `|= 1`
 *     write it themselves and call `t3m_errrec_core`.
 */
static void
t3m_errrec_core(struct t3m_frame *f)
{
	unsigned char *r = f->m + T3M_MSGREC0;
	int k;

	f->obj->is_short = 0;
	f->obj->local_short = 0;
	T3M_I16(f, T3M_F358A) = 1;

	/* 0x6cc55, ten shorts, and the eleventh is not in the loop. */
	for (k = 0; k <= 9; k++)
		T3M_I16(f, T3M_FABAE + 2 * k) = 0;
	T3M_I16(f, T3M_FABC2) = 0;

	/*
	 * Both transitions, and they are announced BEFORE the counter is
	 * cleared -- the counter is `[2]` in each of the two format strings,
	 * so clearing it first would print two different lines and change no
	 * byte of the object.
	 */
	hs_setstate(f->obj, V34HS_TXSTATE_OFF, V34HS_TX_DPSK);
	hs_setstate(f->obj, V34HS_MICROSTATE_OFF, V34HS_DET_SYNC);

	f->obj->fsk.sr = -1;
	f->obj->fsk.nbits = 0;

	/*
	 * 0x6cdc4 and 0x7094b.  The record is filled with one constant per
	 * field, and only +0x18 depends on anything: 0x1e when this end
	 * originates AND a V.90 receiver is running, 0x11 otherwise.  The
	 * originate test alone is not enough -- with no V.90 receiver the
	 * object jumps back into the 0x11 block.
	 */
	*(short *)(r + 0x14) = -1;
	*(short *)(r + 0x16) = 1;
	*(short *)(r + 0x18) =
		(f->obj->role == 0x65 && f->obj->v90_receiver != 0)
		? 0x1e : 0x11;
	*(short *)(r + 0x1a) = 0;
	*(short *)(r + 0x1c) = 8;
	*(short *)(r + 0x1e) = 0;
	*(short *)(r + 0x20) = 1;
	*(short *)(r + 0x22) = 0;
	*(int *)(r + 0x24) = 0xf72;
	*(short *)(r + 0x28) = 0xc;
	*(short *)(r + 0x2a) = 0xc;
	*(int *)(r + 0x2c) = 0xf72;
}

/*
 * 0x6cc1b, 0x70d7f, 0x66517 and 0x660b9 -- the four copies whose first store
 * is the constant 4 rather than a set of bit 0.
 */
static void
t3m_errrec_reset(struct t3m_frame *f)
{
	T3M_I16(f, T3M_F3588) = 4;
	t3m_errrec_core(f);
}

/*
 * 0x6c14a, 0x6bc0b, 0x6fff7, 0x70447 and 0x6d709 -- the five whose first store
 * is `|= 1`, which is NOT the same thing: the reset can run with +0x3588
 * already holding 2, and 4 would destroy that bit while 3 keeps it.
 */
static void
t3m_errrec_arm(struct t3m_frame *f)
{
	T3M_I16(f, T3M_F3588) = (short)(T3M_U16(f, T3M_F3588) | 1);
}

/*
 * 0x65d30 -- microstate 48 `TX_PHASE3_ANS`.
 *
 * A counter with two thresholds and nothing else, which is the shape finding
 * F288 gives to six of table 3's arms.  The counter at +0xaa78 is incremented
 * as an UNSIGNED halfword (`movzwl`/`inc`/`cmp %ax`), stored back before
 * either threshold is tested, and the two thresholds are exact equalities --
 * not "at least", so a counter that steps past 0x78 without landing on it
 * never takes that arm again until it wraps.
 *
 *      0x78    invert bit 0 of +0x358c, and leave with the object's txstate
 *      0xa2    force txstate to SILENCE and microstate to RX_PHASE2_ANS
 *              (0x32), clear the
 *              counter, and leave with 5 -- both forcings announce
 *              themselves, and both are `hs_setstate`'s compare-print-store
 *      else    leave with the object's txstate
 */
static void
t3m_micro48(struct t3m_frame *f)
{
	unsigned short n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);

	T3M_U16(f, T3M_COUNTER) = n;

	if (n == 0x78) {
		/* 0x6c6e4.  An XOR of bit 0, not a store of a constant. */
		T3M_U16(f, T3M_TOGGLE) ^= 1;
		t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
		return;
	}

	if (n == 0xa2) {
		/*
		 * 0x6c670.  Both transitions are `hs_setstate` inlined, and
		 * the first leaves 5 in %ecx on BOTH arms of its already-there
		 * test -- the value loaded from +0x3596 when it matches,
		 * 0x6c6a4's `mov $0x5,%ecx` when it does not -- which is why
		 * the dispatch is reached with a literal SILENCE and not with
		 * a re-read.
		 */
		/* 0x6c670. */
		hs_setstate(f->obj, V34HS_TXSTATE_OFF, V34HS_SILENCE);
		hs_setstate(f->obj, V34HS_MICROSTATE_OFF, V34HS_RX_PHASE2_ANS);
		T3M_U16(f, T3M_COUNTER) = 0;
		t3m_txblock(f, V34HS_SILENCE);
		return;
	}

	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * 0x66834 -- microstates 47 `TX_PHASE2_ANS` AND 56 `TX_PHASE2_CALL`.
 *
 * ONE ARM FOR TWO STATES.  .rodata+0x3000's entries 6 and 15 hold the same
 * address, so the answer side and the originate side of phase 2 run the same
 * code and the arm never looks at which of the two it was entered with.  The
 * test asserts that rather than assuming it.
 *
 * The body is the counter shape of finding F288 with a reset in front of it:
 *
 *      the FSK shift register's low four bits are all ones AND +0x3588 is
 *      still zero  ->  re-arm the error recovery, then fall into the counter
 *      counter+1 <= 0x5f    store it and leave with the object's txstate
 *      otherwise            clear the counter, invert bit 0 of +0x358c, move
 *                           the microstate to TX_L1 and leave
 *
 * BOTH COMPARES ARE SIGNED 16-BIT (`cmp $0x5f,%ax` after a `movzwl`/`inc`),
 * so a counter of 0x7fff steps to -32768 and takes the low arm, and one of
 * 0xffff steps to 0 and takes it as well.
 *
 * THE RESET FALLS THROUGH rather than returning: it clears the counter as its
 * last act and jumps back to 0x6684e, so a step that takes it always leaves
 * with the counter at 1.  That is why the reset cannot be tested by the
 * counter alone.
 */
static void
t3m_micro47(struct t3m_frame *f)
{
	unsigned short n;

	/*
	 * 0x6683b and 0x6cc1b.  `obj->fsk.sr` is the demodulator's shift
	 * register and `nbits` its bit count, and the two are reset together
	 * below -- which is what "repeated info0" means here.  The second
	 * test is what stops it running on every block: +0x3588 becomes 4.
	 */
	if ((f->obj->fsk.sr & 0xf) == 0xf && T3M_I16(f, T3M_F3588) == 0) {
		t3m_errrec_reset(f);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Repeated info0 is detected, "
					     "errorrecovery is initialized in " "TX_PHASE2_xxx\n");

		/*
		 * 0x6ce34 clears the counter and 0x6ce3b jumps back to
		 * 0x6684e, the increment below -- so the clear is the reset's
		 * last act, and 0x6684e re-loads the object pointer from
		 * 0xc0(%esp) because it is a join and not a straight line.
		 */
		T3M_U16(f, T3M_COUNTER) = 0;
	}

	/* 0x6684e. */
	n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);

	/*
	 * The `jle 6aaf9` at 0x66861 selects three instructions that are the
	 * whole low path: the txstate is read into %cx at 0x6ab00 and only
	 * then is the counter stored, at 0x6ab07.
	 */
	if ((short)n <= 0x5f) {
		/* 0x6aaf9. */
		T3M_U16(f, T3M_COUNTER) = n;
		t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
		return;
	}

	/*
	 * 0x66867.  The counter is cleared FIRST, so the transition below
	 * prints `[2]` as 0 and not as the value that crossed the threshold;
	 * the object hands the printf a literal zero, which is how it shows.
	 */
	T3M_U16(f, T3M_COUNTER) = 0;
	T3M_U16(f, T3M_TOGGLE) ^= 1;
	hs_setstate(f->obj, V34HS_MICROSTATE_OFF, V34HS_TX_L1);
	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * 0x6591e -- microstate 63 `INFODONE`.
 *
 * THE TXSTATE IS THE SELECTOR, not a companion: three bodies chosen by the
 * object's own +0x3596, and every other txstate is the three-instruction
 * shared arm -- read the txstate, jump to the transmit dispatch.  That is why
 * this arm is indistinguishable from twenty-four others when it is entered
 * cold at SSEG.
 *
 *      TONE_AB   a counter with one threshold, and two different endings on
 *                the answer/originate flag
 *      SILENCE   three more guards and then `v34setuptxmit`
 *      anything  leave with it
 */
static void
t3m_micro63(struct t3m_frame *f)
{
	short tx = (short)T3M_U16(f, V34HS_TXSTATE_OFF);
	unsigned short n;

	/*
	 * 0x6d160 is the BODY and not the test: 0x6592c compares the txstate
	 * already in %cx against 0x3c and jumps there, and 0x6d160 itself is
	 * the counter increment -- `movzwl 0xaa78`, `inc`, `cmp $0xf,%ax`.
	 */
	if (tx == V34HS_TONE_AB) {			/* 0x6d160 */
		n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);

		if ((short)n <= 0x0f) {
			/* 0x6d230, and %ecx still holds TONE_AB. */
			T3M_U16(f, T3M_COUNTER) = n;
			t3m_txblock(f, tx);
			return;
		}

		/*
		 * The incremented counter is stored before either ending, and
		 * that is not bookkeeping: it is `[2]` in the txstate
		 * transition printed next, and the originate ending keeps it.
		 */
		/*
		 * `cmpw $0x65,0x359c` at 0x6d179 picks the ending, and GCC
		 * sank this store into both of them: unconditional at 0x6de9d
		 * on the originate side, and on the answer side only where the
		 * print needs it, at 0x6d1a0 -- 0x6d21e replaces it with 0x1e
		 * or 0x96 either way.
		 */
		T3M_U16(f, T3M_COUNTER) = n;

		if (f->obj->role == 0x65) {		/* 0x6de96 */
			hs_setstate(f->obj, V34HS_TXSTATE_OFF, V34HS_SILENCE);
			hs_setstate(f->obj, V34HS_MICROSTATE_OFF,
				    V34HS_DET_SYNC);
			f->obj->fsk.nbits = 0;
			t3m_txblock(f,
				    (short)T3M_U16(f, V34HS_TXSTATE_OFF));
			return;
		}

		/*
		 * 0x6d187.  The answer side moves the txstate and REPLACES the
		 * counter, with one of two constants on `ptc`; it does not
		 * touch the microstate, so 63 is entered again next block.
		 */
		hs_setstate(f->obj, V34HS_TXSTATE_OFF, V34HS_SILENCE);
		T3M_U16(f, T3M_COUNTER) =
			f->obj->ptc == 0x30 ? 0x1e : 0x96;
		t3m_txblock(f, V34HS_SILENCE);
		return;
	}

	if (tx != V34HS_SILENCE) {			/* 0x6593a */
		t3m_txblock(f, tx);
		return;
	}

	/* 0x65947, a SIGNED byte, and 0x65958, a SIGNED int against 0x240. */
	if ((signed char)f->m[T3M_FABFF] <= 0
	    || T3M_I32(f, T3M_F0240) <= 0x240) {
		t3m_txblock(f, tx);
		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34Hshak: Setting up transmitter for "
				     "phase3...\r\n");

	/*
	 * 0x6597d..0x65b6d is `v34setuptxmit` inlined -- the same six steps in
	 * the same order, down to the two transitions and the tail call to
	 * `txinit`.  Calling it keeps one copy of the sequence that
	 * `t_v34hshak.c` already sweeps over every rate and carrier.
	 */
	v34setuptxmit(f->obj);
	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * 0x66a0d -- microstate 49 `RX_PHASE1_ANS`.
 *
 * FOUR THRESHOLDS ON ONE COUNTER, and three of the four are `+0xaa7c` plus a
 * constant rather than a constant -- so the arm cannot be tested at all
 * without seeding that field, and the fixture's fill sends every counter down
 * the first exit.  `+0xaa7c` is the object's own `filtDelay`; the diagnostic
 * at 0x709d7 names it.
 *
 *      n = ++counter, stored back before anything is tested
 *
 *      n > 0x28 AND n < filtdelay + 0x4c AND (fsk.sr & 0x3ff) == 0x372
 *          AND +0x3588 == 0            -> re-arm the error recovery
 *      n <= filtdelay + 0x50           -> leave with the object's txstate
 *      otherwise, and in this order:
 *          rx->flags |= 0x200          -- ALWAYS, even on the next exit
 *          fsk.sr bit 0 clear          -> leave with the object's txstate
 *          rtd  =  is_short ? prev_bulk_delay
 *                           : (n - filtdelay) * 4 - 0x18c
 *          rtd <= 0                    -> rtd = 1
 *          ApplyBulkDelay(obj, rtd)
 *          counter = filtdelay, THEN microstate -> TX_PHASE2_ANS
 *          receiver's +0x264 = its agc_gain
 *          leave with the object's txstate
 *
 * THE RESET AND THE BULK-DELAY PATH CANNOT BOTH RUN, and that is structural
 * rather than a property of any seed: 0x4c is below 0x50, so the window the
 * reset needs is entirely below the threshold the rest needs.
 *
 * THE FIRST COMPARE IS SIGNED 16-BIT and the increment is unsigned, so a
 * counter of 0x7fff steps to -32768 and takes the first exit.
 */
static void
t3m_micro49(struct t3m_frame *f)
{
	unsigned short n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);
	int filt;
	short rtd;

	T3M_U16(f, T3M_COUNTER) = n;

	/*
	 * 0x66a2d loads +0xaa7c zero-extended and 0x66a37 sign-extends it for
	 * the compare.  The four addresses below are the four conjuncts in
	 * source order -- `cmp $0x28,%ax`/`jle`, `cmp %ebx,%ebp`/`jge`,
	 * `cmp $0x372`/`je`, then `cmpw $0x0,0x3588`/`jne` out at 0x70d71 --
	 * and 0x70d7f, the reset itself, is that last one's fall-through.
	 */
	filt = T3M_I16(f, T3M_FILTDELAY);

	/* 0x66a1c, 0x66a3d, 0x66a4d and 0x70d71. */
	if ((short)n > 0x28 && (short)n < filt + 0x4c
	    && (((unsigned)(unsigned short)f->obj->fsk.sr & 0x3ff) == 0x372)
	    && T3M_I16(f, T3M_F3588) == 0) {
		t3m_errrec_reset(f);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Repeated info0 is detected, "
					     "errorrecovery is initialized in " "RX_PHASE1_ANS\n");
	}

	/*
	 * 0x66a58.  The counter is re-read off the object rather than reused,
	 * which matters because the reset above may have moved through it --
	 * it does not, but the object reads it again and so does this.
	 */
	if ((int)T3M_I16(f, T3M_COUNTER) <= filt + 0x50) {
		t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
		return;
	}

	/* 0x66a74, and it happens before the test that can leave. */
	f->rx->flags = (unsigned short)(f->rx->flags | V34_RX_FLAG_DET_PENDING);

	if ((f->obj->fsk.sr & 1) == 0) {
		t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
		return;
	}

	/*
	 * 0x6ce40.  The copy is one halfword move, +0xac02 into +0xaa7e at
	 * 0x6ce4e, and it is made BEFORE the debug-level branch at 0x6ce5c, so
	 * only the line is conditional.
	 */
	if (f->obj->is_short != 0) {
		/* 0x6ce40. */
		f->obj->rtd = f->obj->prev_bulk_delay;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Setting Bulk delay according to "
					     "prev session - %d\r\n",
					     (int)f->obj->rtd);
	} else {
		/*
		 * 0x66aa7.  Both halfwords are zero-extended and the whole
		 * sum is done 32-bit before it is stored back as a short, so a
		 * counter below `filtdelay` gives a huge product and not a
		 * negative one.
		 */
		unsigned d = (unsigned)T3M_U16(f, T3M_COUNTER)
			   - (unsigned)T3M_U16(f, T3M_FILTDELAY);

		f->obj->rtd = (short)(d * 4u - 0x18cu);
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("On RX_PHASE1_ANS: is short=%d, "
				     "bulkDelay=%d, filtDelay=%d\r\n",
				     (int)f->obj->is_short, (int)f->obj->rtd,
				     (int)T3M_I16(f, T3M_FILTDELAY));

	/* 0x66ae2, on the SIGNED halfword. */
	if (f->obj->rtd <= 0)
		f->obj->rtd = 1;

	rtd = f->obj->rtd;
	ApplyBulkDelay(f->obj, rtd);

	/*
	 * 0x66b13.  The counter takes `filtdelay` BEFORE the transition below,
	 * so the `[2]` the line prints is filtdelay and not the value that
	 * crossed the threshold.
	 */
	T3M_U16(f, T3M_COUNTER) = T3M_U16(f, T3M_FILTDELAY);
	hs_setstate(f->obj, V34HS_MICROSTATE_OFF, V34HS_TX_PHASE2_ANS);

	/* 0x66b47, and 0x66b55 reads +0x3596 for the exit. */
	*(short *)((unsigned char *)f->rx + T3M_RX_F264) = f->rx->agc_gain;

	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * 0x664b8 -- microstate 50 `RX_PHASE2_ANS`.
 *
 * The same four-threshold shape as 49 against the same `filtdelay`, with a
 * different first threshold, a different test of the shift register and a
 * different ending:
 *
 *      n > 0x32 AND n < filtdelay + 0x4c AND (fsk.sr & 0x3ff) > 0x200
 *          AND +0x3588 == 0            -> re-arm the error recovery
 *      n <= filtdelay + 0x50           -> leave with the object's txstate
 *      fsk.sr bit 0 clear              -> leave with the object's txstate
 *      otherwise
 *          counter = 0x14
 *          V34SetupDemodulator(obj, 2400, 1800)
 *          rxtiminginit(obj)
 *          rxstate -> RX_L1
 *          rx->flags &= ~0x0a00
 *          rx->agc_gain >>= 1, arithmetic
 *          leave with the object's txstate
 *
 * 0x66766..0x667c4 IS `V34SetupDemodulator` INLINED with two literals -- the
 * `mov $0x960`/`mov $0x708` at 0x71327 are the diagnostic's own arguments and
 * not a read of any field.  Compared field for field against
 * `src/pump/v34/V34RX.c`: `out_count`, the four 2400-baud words and the 1800-Hz
 * carrier and its `half_len`, in that function's order.  Calling it keeps one
 * copy of a switch `t_v34rx.c` already sweeps over all six rates.
 *
 * ARM 49 SETS bit 0x200 IN THE RECEIVER'S FLAGS AND THIS ONE CLEARS IT, along
 * with 0x800.  They are not the same arm with a sign flipped: 49 sets it
 * before its own last guard and 50 clears it after everything.
 */
static void
t3m_micro50(struct t3m_frame *f)
{
	unsigned short n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);
	int filt;

	T3M_U16(f, T3M_COUNTER) = n;

	/*
	 * Four inline compares, one per conjunct: `cmp $0x32,%ax`/`jle 6af11`,
	 * `cmp %eax,%ebp`/`jge 66724`, `cmp $0x200,%edx`/`jle 66724` and
	 * `cmpw $0x0,0x3588`/`jne 66724`.  Arm 49's fourth conjunct is
	 * out-lined at 0x70d71; this arm's is inline with the other three.
	 */
	filt = T3M_I16(f, T3M_FILTDELAY);

	/* 0x664c7, 0x664e8, 0x664fd and 0x66509. */
	if ((short)n > 0x32 && (short)n < filt + 0x4c
	    && (int)((unsigned)(unsigned short)f->obj->fsk.sr & 0x3ff) > 0x200
	    && T3M_I16(f, T3M_F3588) == 0) {
		t3m_errrec_reset(f);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Repeated info0 is detected, "
					     "errorrecovery is initialized in " "RX_PHASE2_ANS\n");
	}

	/*
	 * 0x66724, where the last three guards above converge.  The COUNTER is
	 * re-read off the object -- `movswl 0xaa78(%ebx),%ebp` -- while the
	 * delay is still the +0xaa7c that 0x664d8 left in %cx.
	 */
	/* 0x66724. */
	if ((int)T3M_I16(f, T3M_COUNTER) <= filt + 0x50) {
		t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
		return;
	}

	if ((f->obj->fsk.sr & 1) == 0) {
		t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
		return;
	}

	/* 0x66759, and it is stored before the demodulator's diagnostic. */
	T3M_U16(f, T3M_COUNTER) = 0x14;

	V34SetupDemodulator(f->obj, 2400, 1800);
	rxtiminginit(f->obj);

	hs_setstate(f->obj, V34HS_RXSTATE_OFF, V34HS_RX_L1);

	/* 0x66812 and 0x66818.  0xfffff5ff is ~0x0a00 and not a mask of one
	   bit; the shift is arithmetic on the sign-extended halfword. */
	f->rx->agc_gain = (short)((int)f->rx->agc_gain >> 1);
	f->rx->flags = (unsigned short)(f->rx->flags
					& ~(V34_RX_FLAG_DET_PENDING | V34_RX_FLAG_FIR));

	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * 0x65c47 -- microstate 51 `TX_L1`.
 *
 * ONE THRESHOLD, AND IT IS AN EQUALITY: `cmp $0x2a,%dx` / `je`, so a counter
 * that steps past 0x2a without landing on it never runs the body again until
 * it has wrapped through 65,536.  Everything else leaves with the object's
 * txstate.
 *
 * `is_short` then chooses between two whole bodies, and they announce their
 * two transitions in OPPOSITE ORDERS -- which is the only thing separating
 * them that a byte comparison can see nothing of, because both end with the
 * same two state words written:
 *
 *   is_short == 0    txstate -> TX_L1, then microstate -> TX_L1 (which is a
 *                    no-op, since 51 IS TX_L1 and `hs_setstate` compares
 *                    first), vect_idx = 0, then `detectorinit` on the
 *                    detector at +0x3564 with `c2400_` when this end
 *                    originates and `c1200_` when it answers, then
 *                    rx->flags |= 0x200 and rxstate -> DET_AB
 *
 *   is_short != 0    microstate -> INFODONE, then txstate -> TX_DPSK, then
 *                    vect_idx, the counter and +0x35a2 all cleared, +0xaa6c
 *                    aimed at the message record at +0xa9ac,
 *                    `V34SetINFO1aBits` on it, the transmit power scale
 *                    chosen from the baud rate, the record filled, and four
 *                    fields copied across to the receive side
 *
 * Both then scale the receiver's gain, clear +0x35a0 and print one line.
 */
static void
t3m_micro51(struct t3m_frame *f)
{
	unsigned short n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);
	short gain;

	T3M_U16(f, T3M_COUNTER) = n;

	if (n != 0x2a) {
		t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
		return;
	}

	/*
	 * 0x6b5f5 opens the `is_short == 0` body with the txstate transition
	 * inlined -- `movzwl 0x3596`, `cmp $0x33`, `je 6b67d` -- and 0x33 is
	 * 51, the microstate this arm already is, so both transitions below
	 * can be no-ops and the object still emits both compares.
	 */
	if (f->obj->is_short == 0) {
		/* 0x6b5f5. */
		hs_setstate(f->obj, V34HS_TXSTATE_OFF, V34HS_TX_L1);
		hs_setstate(f->obj, V34HS_MICROSTATE_OFF, V34HS_TX_L1);
		f->obj->vect_idx = 0;

		/*
		 * 0x6b764 and 0x6f8c2.  Seven arguments, six of them the same
		 * constants either way; only the coefficient table depends on
		 * which end this is.
		 */
		detectorinit((struct v34_detector *)(f->m + T3M_DETECTOR),
			     f->obj->role == 0x65 ? c2400_ : c1200_,
			     0, 0x64, 0x32, 0x800, 0);

		f->rx->flags = (unsigned short)(f->rx->flags | V34_RX_FLAG_DET_PENDING);
		hs_setstate(f->obj, V34HS_RXSTATE_OFF, V34HS_DET_AB);
	} else {
		unsigned char *r;
		unsigned short *w;
		int baud;

		/* 0x6b8d0, and the two transitions are the other way round. */
		hs_setstate(f->obj, V34HS_MICROSTATE_OFF, V34HS_INFODONE);
		hs_setstate(f->obj, V34HS_TXSTATE_OFF, V34HS_TX_DPSK);

		/* 0x6b9e3, and all three are cleared AFTER both lines are
		   printed -- vect_idx is `[1]` and the counter is `[2]`. */
		f->obj->vect_idx = 0;
		T3M_U16(f, T3M_COUNTER) = 0;
		T3M_I16(f, T3M_F35A2) = 0;

		w = (unsigned short *)(f->m + T3M_INFOREC);
		*(unsigned short **)(f->m + T3M_SELFPTR) = w;
		V34SetINFO1aBits(f->obj, (short *)w);

		/*
		 * 0x6ba1f.  A signed compare tree on the transmit baud rate,
		 * and every arm but 0xab7's writes one pointer and nothing
		 * else.  A rate not in the six leaves the scale alone, which
		 * is why a trial at the fixture's own fill tests none of them.
		 */
		baud = T3M_I16(f, T3M_TXBAUD);

		switch (baud) {
		case 0x960:		/* 2400 */
			*(const short **)(f->m + T3M_TXSCALE) = scale2400;
			break;
		case 0xab7:		/* 2743, and the only arm that
					   REPLACES the rate it matched */
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V34HSHAK: Illegal prev session baud "
					"(2743), select 2800 instead...\r\n");
			{
				short c = T3M_U16(f, T3M_TXCARRIER) == 0x725
					  ? 0x74b : 0x690;

				T3M_U16(f, T3M_TXBAUD) = 0xaf0;
				*(const short **)(f->m + T3M_TXSCALE) =
					scale2800;
				T3M_U16(f, T3M_TXCARRIER) = (unsigned short)c;
			}
			break;
		case 0xaf0:		/* 2800 */
			*(const short **)(f->m + T3M_TXSCALE) = scale2800;
			break;
		case 0xbb8:		/* 3000 */
			*(const short **)(f->m + T3M_TXSCALE) = scale3000;
			break;
		case 0xc80:		/* 3200 */
			*(const short **)(f->m + T3M_TXSCALE) = scale3200;
			break;
		case 0xd65:		/* 3429 */
			*(const short **)(f->m + T3M_TXSCALE) = scale3429;
			break;
		default:
			break;
		}

		/*
		 * 0x6ba5f.  The record is reached THROUGH +0xaa6c, re-read
		 * after `V34SetINFO1aBits`, and not through the address stored
		 * into it above.  Two of the twelve fields are 32-bit.
		 */
		r = *(unsigned char **)(f->m + T3M_SELFPTR);

		*(short *)(r + 0x14) = -1;
		*(short *)(r + 0x16) = 1;
		*(short *)(r + 0x18) = 0x26;
		*(short *)(r + 0x1a) = 0;
		*(short *)(r + 0x1c) = 8;
		*(short *)(r + 0x1e) = 0;
		*(short *)(r + 0x20) = 0;
		*(short *)(r + 0x22) = 0;
		*(int *)(r + 0x24) = 0xff72;
		*(short *)(r + 0x28) = 0x10;
		*(short *)(r + 0x2a) = 0x10;
		*(int *)(r + 0x2c) = 0xff72;

		/* 0x6bac3.  Four copies, and the rate they copy is the one the
		   switch above may have replaced. */
		T3M_I16(f, T3M_TOGGLE) = 0;
		f->obj->baud_rate = (short)T3M_U16(f, T3M_TXBAUD);
		T3M_U16(f, T3M_RXCARRIER) = T3M_U16(f, T3M_TXCARRIER);
		*(const short **)(f->m + T3M_RXSCALE) =
			*(const short **)(f->m + T3M_TXSCALE);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"%s 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x," "0x%x,0x%x,0x%x\n",
				"V34PROBE, txinfo1a (QC)",
				w[0], w[1], w[2], w[3], w[4],
				w[5], w[6], w[7], w[8], w[9]);
	}

	/*
	 * 0x6b81a.  Two thresholds and two different shifts, both signed, and
	 * a gain at or below 0x1000 is left alone.
	 */
	gain = f->rx->agc_gain;
	if (gain > 0x2000)
		f->rx->agc_gain = (short)((int)gain >> 2);
	else if (gain > 0x1000)
		f->rx->agc_gain = (short)((int)gain >> 1);

	T3M_I16(f, T3M_F35A0) = 0;

	/*
	 * 0x6b84f.  The two phrases are the object's, at .rodata.str1.1+0x2b94
	 * and +0x2ba8, and `is_short` picks between them -- READ AGAIN here,
	 * not remembered, because the `is_short == 0` body above does not
	 * change it but the reader has no way to know that from this line.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34RETRAIN, %s, rx->rxflgs = 0x%x," "rx->gain=0x%x\n",
				     f->obj->is_short != 0
				     ? "transmitting info1a" : "starting DET_AB",
				     (unsigned)f->rx->flags,
				     (int)f->rx->agc_gain);

	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * 0x662b0 -- microstate 59 `RX_PHASE2_CALL`.
 *
 * The one arm in this batch that has its own behaviour when the fixture drives
 * it cold, and the reason is the FIRST guard: 55 and 58 read a companion field
 * before they do anything, and this one increments the counter and then tests
 * the shift register's low THREE bits, which the fixture's fill satisfies.  So
 * a cold step re-arms the error recovery, writes 77 bytes and prints four
 * lines, and the "group D" the sweep put the other two in is not where this
 * one lives.  Finding F286's table.
 *
 * FIVE BLOCKS IN SEQUENCE, and they are not exclusive -- a single step can run
 * the reset, the retrain check, the bulk-delay block and the second reset one
 * after another:
 *
 *      n = ++counter, stored back before anything is tested
 *
 *   1  (fsk.sr & 7) == 7      -> print, `+0x3588 |= 1`, print the shift
 *                               register, re-arm the error recovery, and
 *                               CLEAR THE COUNTER.  0x6bc0b.
 *
 *      c = counter, RE-READ -- which is the whole reason the clear above is
 *      visible: after the reset this arm leaves with 0 and 55's leaves with 1,
 *      because 55's copy jumps back into the increment and this one does not.
 *
 *   2  c == 0x2a              -> txstate to SILENCE, announced.  0x6bbd5.
 *
 *   3  c > 0x125f AND the receiver's +0x19e is set AND `tone_detect` asserts
 *                             -> print, set 0x40 in the receiver's flags,
 *                                `v34handshakinit(obj, 1)` and LEAVE.  The
 *                                only exit that skips everything below.
 *                                Otherwise +0x19e is set to 1 and c re-read.
 *
 *   4  c > filtdelay + 0x5c AND fsk.sr is exactly 8 or exactly 0x18
 *                             -> rtd = (c - filtdelay - 0x63) * 4, clamped up
 *                                to 1, `ApplyBulkDelay`, counter = 0x14,
 *                                `V34SetupDemodulator(obj, 2400, 1800)`,
 *                                `rxtiminginit`, rxstate to RX_L1, the gain
 *                                halved and 0x0a00 cleared, then one line.
 *
 *   5  the 32-bit word at +0x3588 is 0x20002 AND (fsk.sr & 0x3ff) == 0x372
 *                             -> `v34handshakinit(obj, 0)`, +0xaa6c aimed at
 *                                the first message record, `+0x3588 |= 1`,
 *                                the error recovery re-armed again, one line.
 *
 * THE THIRD THRESHOLD IS `filtdelay + 0x5c` AND NOT 49'S 0x4c OR 0x50, and
 * the shift-register test is a full-width equality against two values rather
 * than either of the masked tests 49 and 50 use.  Those two facts are what
 * separate this arm from the group-D six it was measured with.
 *
 * `tone_detect`'s window is the receiver's +0x10c -- `fskdemodulate`'s own
 * input -- and its end is the receiver's sample pointer at +0x130, so the two
 * arguments come from two different fields and not from one buffer.
 *
 * EVERY ONE OF THE FIVE EXITS RE-READS +0x3596.  None of them passes a
 * literal, so the transmit dispatch always runs the arm for whatever the
 * txstate is by then -- which block 2 above may have changed to SILENCE.
 */
static void
t3m_micro59(struct t3m_frame *f)
{
	unsigned short n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);
	short c, filt, rtd;

	T3M_U16(f, T3M_COUNTER) = n;

	/* 0x662c5 and 0x6bc0b.  Three bits, where 47's copy tests four. */
	if ((f->obj->fsk.sr & 7) == 7) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Repeated info0 is detected, "
					     "errorrecovery is initialized in " "RX_PHASE2_CALL\n");

		t3m_errrec_arm(f);

		/*
		 * 0x717d8.  Printed AFTER +0x3588 is armed and BEFORE the body
		 * below overwrites the shift register with -1, so what it shows
		 * is the register that satisfied the guard.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RXBUF=0x%x\n",
					     (unsigned)(unsigned short)
					     f->obj->fsk.sr);

		t3m_errrec_core(f);
		T3M_U16(f, T3M_COUNTER) = 0;
	}

	/* 0x662e0.  One load of +0xaa78 feeds both compares. */
	c = (short)T3M_U16(f, T3M_COUNTER);

	/*
	 * 0x662e7 and 0x6bbd5.  The object jumps straight past the next test on
	 * the branch where the txstate was already 5, which it can do because
	 * 0x2a is not above 0x125f either way; `hs_setstate` is that branch.
	 */
	if (c == 0x2a)
		hs_setstate(f->obj, V34HS_TXSTATE_OFF, V34HS_SILENCE);

	if (c > 0x125f) {
		/*
		 * 0x662fc tests +0x19e sixteen bits wide (`cmpw $0x0`) and
		 * only a non-zero one reaches 0x6bb57, where the detector call
		 * is built out of +0x130 and +0x3564 -- so the `&&`
		 * short-circuits in the object as well as here.
		 */
		/* 0x662fc and 0x6bb57. */
		if (f->rx->retrain_gate != 0
		    && tone_detect(f->rx,
				   (struct v34_detector *)(f->m + T3M_DETECTOR),
				   (const short *)((unsigned char *)f->rx
						   + T3M_RX_FSKIN),
				   f->rx->rx_samples)) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V34RETRAIN, retrain is " "initiated in "
						     "RX_PHASE2_CALL\n");

			f->rx->flags = (unsigned short)(f->rx->flags | V34_RX_FLAG_RETRAIN);
			v34handshakinit(f->obj, 1);
			t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
			return;
		}

		/* 0x6630a, reached whether +0x19e was set or the detector said
		   no, and the counter is read again at 0x66321. */
		f->rx->retrain_gate = 1;
		c = (short)T3M_U16(f, T3M_COUNTER);
	}

	/*
	 * 0x66332.  The comparison sign-extends `filtdelay` and the subtraction
	 * below zero-extends it, and both are truncated to sixteen bits before
	 * anything is stored, so the two readings differ only in the compare.
	 */
	filt = T3M_I16(f, T3M_FILTDELAY);

	if ((int)c > (int)filt + 0x5c
	    && ((unsigned short)f->obj->fsk.sr == 0x08
		|| (unsigned short)f->obj->fsk.sr == 0x18)) {
		/*
		 * 0x66371.  `sub $0x63` BEFORE the shift, which is 49's
		 * `(n - filtdelay) * 4 - 0x18c` written the other way round --
		 * the same halfword, since 0x18c is four times 0x63.  The
		 * clamp is on the SHIFTED value and it is signed.
		 */
		rtd = (short)((unsigned)(unsigned short)c
			      - (unsigned)(unsigned short)filt - 0x63u);
		rtd = (short)((int)rtd * 4);
		f->obj->rtd = rtd <= 0 ? 1 : rtd;

		ApplyBulkDelay(f->obj, f->obj->rtd);

		/* 0x663b0, and it is stored before the demodulator's line. */
		T3M_U16(f, T3M_COUNTER) = 0x14;

		/*
		 * 0x663bd..0x66414 is `V34SetupDemodulator` inlined with two
		 * literals, exactly as arm 50's 0x66766 is: `out_count`, the four
		 * 2400-baud words and the 1800-Hz carrier and its `half_len`, in
		 * that function's order.  The 0x960 and 0x708 at 0x717a4 are
		 * the diagnostic's own arguments and not a read of any field.
		 */
		V34SetupDemodulator(f->obj, 2400, 1800);
		rxtiminginit(f->obj);

		hs_setstate(f->obj, V34HS_RXSTATE_OFF, V34HS_RX_L1);

		/* 0x66477 then 0x66481, the gain before the flags. */
		f->rx->agc_gain = (short)((int)f->rx->agc_gain >> 1);
		f->rx->flags = (unsigned short)(f->rx->flags
						& ~(V34_RX_FLAG_DET_PENDING | V34_RX_FLAG_FIR));

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34AGC, rx->gain =0x%x," "rx->slowcf=%d, at end of "
					     "bulkdelay estimation\n",
					     (int)f->rx->agc_gain,
					     (int)f->rx->agc_step);
	}

	/*
	 * 0x66495 and 0x70447.  ONE 32-BIT COMPARE OF TWO HALFWORDS: +0x3588
	 * must be 2 and +0x358a must be 2, which is why this block cannot run
	 * in the same step as the reset above unless the reset ran first and
	 * left them 3 and 1 -- and 3 is not 2, so it cannot.
	 */
	if (T3M_I32(f, T3M_F3588) == 0x20002
	    && (((unsigned)(unsigned short)f->obj->fsk.sr & 0x3ff) == 0x372)) {
		v34handshakinit(f->obj, 0);

		/*
		 * 0x70475.  +0x3588 IS READ BACK AFTER THE CALL and not before
		 * it: `v34handshakinit` writes that field, so a reading taken
		 * first would set bit 0 of the wrong value.
		 */
		*(unsigned char **)(f->m + T3M_SELFPTR) = f->m + T3M_MSGREC0;
		t3m_errrec_arm(f);
		t3m_errrec_core(f);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34 In Retrain. errorrecovery for "
					     "info0 is initialized in " "RX_PHASE2_CALL\n");
	}

	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * 0x65b72 -- microstate 55 `TX_PHASE1_CALL`.
 *
 * THE FIRST GUARD IS NOT IN THE OBJECT.  `fskdemodulate` moves `fsk.nbits`,
 * and this arm asks whether it moved: 0x64a9e reads the field into `%ebx`
 * before the call and 0x65b79 compares the field against it afterwards.  That
 * is what the cold sweep could not see and what put 55 in "group D" with five
 * arms it has nothing else in common with -- and it is why this is the one arm
 * of the batch whose reconstruction needed a field in the frame.
 *
 *   A  fsk.nbits CHANGED across the demodulator AND (fsk.sr & 7) == 7
 *          ->  print, `+0x3588 |= 1`, print the shift register, the shared
 *              reset, CLEAR THE COUNTER -- and then FALL BACK INTO the
 *              increment below, which is what makes this copy of the reset
 *              leave the counter at 1 where 59's leaves it at 0.  0x6c14a.
 *
 *      n = ++counter
 *
 *   B  n <= 0x5f          ->  store it
 *      otherwise          ->  counter = 0, invert bit 0 of +0x358c, microstate
 *                             to RX_PHASE2_CALL, and print the shift register
 *
 *   C  the 32-bit word at +0x3588 is 0x20002 AND (fsk.sr & 0x3ff) == 0x372
 *                         ->  `v34handshakinit(obj, 0)`, +0xaa6c aimed at the
 *                             first message record, `+0x3588 |= 1`, the shared
 *                             reset again, one line.  0x6fff7, and it is 59's
 *                             fifth block instruction for instruction.
 *
 * BLOCK A AND BLOCK C CANNOT BOTH RUN: A leaves +0x3588 with bit 0 set and the
 * whole 32-bit word must be exactly 0x20002 for C, whose low halfword is 2.
 * That is structural and not a property of any seed.
 *
 * THE SHIFT-REGISTER TEST IN A IS THREE BITS, where 47's copy of the same
 * reset tests four and 49's tests `& 0x3ff == 0x372`.  It is 59's test, and
 * the two arms are separated instead by the `nbits` compare, which 59 does not
 * make at all.
 *
 * BOTH OF THIS ARM'S RESETS ARE ON THE `|= 1` SIDE of finding F375's split.
 */
static void
t3m_micro55(struct t3m_frame *f)
{
	unsigned short n;

	/* 0x65b79 and 0x65b82, and only then 0x6c14a. */
	if (f->obj->fsk.nbits != f->nbits && (f->obj->fsk.sr & 7) == 7) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Repeated info0 is detected, "
					     "errorrecovery is initialized in " "TX_PHASE1_CALL\n");

		t3m_errrec_arm(f);

		/*
		 * 0x6e3f8.  Printed after +0x3588 is armed and before the body
		 * below overwrites the register with -1, exactly as 59's is.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RXBUF=0x%x\n",
					     (unsigned)(unsigned short)
					     f->obj->fsk.sr);

		t3m_errrec_core(f);

		/* 0x6c2af, and then `jmp 65b95` -- back INTO the increment. */
		T3M_U16(f, T3M_COUNTER) = 0;
	}

	/* 0x65b95.  Unsigned increment, SIGNED sixteen-bit compare. */
	n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);

	/*
	 * 0x6ab9b is out of line and it does NOT leave: the store of the
	 * incremented counter, then `jmp 65c1d` back into the arm.  That is
	 * what separates this low arm from 47's at 0x6aaf9, which reads the
	 * txstate and goes to the dispatch.
	 */
	if ((short)n <= 0x5f) {
		/* 0x6ab9b. */
		T3M_U16(f, T3M_COUNTER) = n;
	} else {
		/*
		 * 0x65bae.  The counter is cleared BEFORE the transition, so
		 * the `[2]` the line prints is 0 and not the value that
		 * crossed the threshold -- the same order arm 47's copy takes.
		 */
		T3M_U16(f, T3M_COUNTER) = 0;
		T3M_U16(f, T3M_TOGGLE) ^= 1;
		hs_setstate(f->obj, V34HS_MICROSTATE_OFF, V34HS_RX_PHASE2_CALL);

		/*
		 * 0x65bff.  Printed whether or not the transition was a move,
		 * because 0x6c2bb rejoins below `hs_setstate`'s compare.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RXBUF=0x%x\n",
					     (unsigned)(unsigned short)
					     f->obj->fsk.sr);
	}

	/* 0x65c24 and 0x6d243 -- one 32-bit compare of two halfwords. */
	/*
	 * 0x6fff7, the body that guard selects, opens by building the call:
	 * `xor %ebx,%ebx` into 0x4(%esp), so the second argument is a literal
	 * zero and not the 1 the retraining sites pass.
	 */
	if (T3M_I32(f, T3M_F3588) == 0x20002
	    && (((unsigned)(unsigned short)f->obj->fsk.sr & 0x3ff) == 0x372)) {
		/* 0x6fff7. */
		v34handshakinit(f->obj, 0);

		/*
		 * 0x7001c.  +0x3588 is read back AFTER the call, because
		 * `v34handshakinit` writes it.
		 */
		*(unsigned char **)(f->m + T3M_SELFPTR) = f->m + T3M_MSGREC0;
		t3m_errrec_arm(f);
		t3m_errrec_core(f);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34 In Retrain. errorrecovery for "
					     "info0 is initialized in " "TX_PHASE1_CALL\n");
	}

	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * 0x66003 -- microstate 58 `RX_PHASE1_CALL`.
 *
 * FOUR BLOCKS IN SEQUENCE and only the first is exclusive of the rest, so a
 * single step can run the head, cross the second threshold, take one reset and
 * then leave -- the same shape as 59 and not the "one guard, one exit" shape
 * the cold sweep put this arm's group in.
 *
 *   0  txstate == TX_DPSK  ->  the message record reached THROUGH +0xaa6c (and
 *                              NOT the one at +0xa94c the resets below fill)
 *                              has its +0x20 cleared, but only when +0x20 and
 *                              +0x22 are BOTH non-zero.  0x6c3b7.
 *
 *      n = ++counter, stored back before anything is tested
 *
 *   1  n > 0x5f            ->  `rx->flags |= 0x200` UNCONDITIONALLY, and then
 *                              only if `fsk.sr` is exactly 1:
 *                              print, `+0x358a = 2`, counter = filtdelay,
 *                              microstate -> TX_PHASE1_CALL, the receiver's
 *                              +0x264 takes its gain, `rx->flags |= 0x200`
 *                              again, and LEAVE.  0x6c2d0.
 *
 *      c = counter, RE-READ off the object rather than reused
 *
 *   2  c > 0x3bf AND +0x3588 has bit 1 AND +0x358a == 2 AND
 *      (fsk.sr & 0x3ff) == 0x372
 *                          ->  RESET D at 0x6d709: `v34handshakinit(obj, 0)`
 *                              and +0xaa6c re-aimed at +0xa94c, but ONLY when
 *                              +0x3588 is exactly 2; then `+0x3588 |= 1`, the
 *                              shared reset, one line -- and it REJOINS block
 *                              3 with the counter read a third time rather
 *                              than leaving.
 *
 *   3  c > 0x4b0 AND +0x3588 == 0 AND fsk.sr != 0
 *                          ->  RESET C at 0x660b9, which is `t3m_errrec_reset`
 *                              unchanged, and then leave.
 *
 * BLOCK 2 AND BLOCK 3'S RESETS CANNOT BOTH RUN IN ONE STEP, and it is
 * structural rather than a property of any seed: block 2 needs bit 1 of
 * +0x3588 set and leaves bit 0 set as well, and block 3 needs the whole
 * halfword to be zero.  So D never enables C, and C is unreachable in any step
 * D ran.  Finding F372's disjoint windows, in the form this arm takes.
 *
 * `+0x3588` IS READ TWICE IN BLOCK 2 and the second read is what decides
 * whether `v34handshakinit` runs.  The object holds the first read in `%dx`
 * across the call's branch and re-reads it AFTER the call on the branch that
 * makes it, which is the same care 59's second reset takes -- and it is why
 * `t3m_errrec_arm` reading the field fresh is exact on both branches.
 *
 * FINDING F376 SAYS THIS ARM IS ON THE `= 4` SIDE OF FINDING F375'S SPLIT.  It
 * has ONE OF EACH: 0x660b9 stores the constant 4 and 0x6d709 sets bit 0.  The
 * nine-copy list above already had it right; the summary in 376 is the thing
 * that is short.
 */
static void
t3m_micro58(struct t3m_frame *f)
{
	unsigned short n;
	short c;

	/*
	 * 0x6600a.  The record is reached through the self-pointer and the
	 * resets below fill the one at +0xa94c by address, so the two are the
	 * same record only when something has aimed it there -- which block 2
	 * does and the entry does not.
	 */
	if ((short)T3M_U16(f, V34HS_TXSTATE_OFF) == V34HS_TX_DPSK) {
		unsigned char *r = *(unsigned char **)(f->m + T3M_SELFPTR);

		if (*(short *)(r + 0x20) != 0 && *(short *)(r + 0x22) != 0)
			*(short *)(r + 0x20) = 0;
	}

	/* 0x66018, and the store is before either threshold. */
	n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);
	T3M_U16(f, T3M_COUNTER) = n;

	if ((short)n > 0x5f) {
		/* 0x66038, and it happens whether or not the test below does. */
		f->rx->flags = (unsigned short)(f->rx->flags | V34_RX_FLAG_DET_PENDING);

		/* 0x6604c, a full-width equality and not one of 49's, 50's or
		   59's masked tests of the same register. */
		if ((unsigned short)f->obj->fsk.sr == 1) {
			/* 0x6e28d, printed before anything below is written. */
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V34RETRAIN, RX_PHASE1_CALL received, "
					"count2=%d,rx->gain=0x%x,filtdelay=%d\n",
					(int)T3M_I16(f, T3M_COUNTER),
					(int)f->rx->agc_gain,
					(int)T3M_I16(f, T3M_FILTDELAY));

			/* 0x6c2dd.  The counter takes filtdelay BEFORE the
			   transition, so the `[2]` the line prints is
			   filtdelay and not the value that crossed 0x5f. */
			T3M_I16(f, T3M_F358A) = 2;
			T3M_U16(f, T3M_COUNTER) = T3M_U16(f, T3M_FILTDELAY);
			hs_setstate(f->obj, V34HS_MICROSTATE_OFF,
				    V34HS_TX_PHASE1_CALL);

			/* 0x6c37f, the copy before the flag. */
			*(short *)((unsigned char *)f->rx + T3M_RX_F264) =
				f->rx->agc_gain;
			f->rx->flags = (unsigned short)(f->rx->flags | V34_RX_FLAG_DET_PENDING);

			t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
			return;
		}
	}

	/* 0x66061.  One load, and both thresholds read it from %cx. */
	c = (short)T3M_U16(f, T3M_COUNTER);

	/*
	 * The four guards in source order, and only the last is out of line:
	 * `cmp $0x3bf,%cx`; `test $0x2,%dl`, a BYTE test of the halfword
	 * loaded at 0x66071; `cmpw $0x2,0x358a`; and at 0x6d709 the masked
	 * `cmp $0x372`, whose failure joins the first three at 0x6608b.
	 */
	/* 0x66068, 0x66071, 0x6607d and 0x6d709. */
	if (c > 0x3bf && (T3M_U16(f, T3M_F3588) & 2) != 0
	    && T3M_U16(f, T3M_F358A) == 2
	    && (((unsigned)(unsigned short)f->obj->fsk.sr & 0x3ff) == 0x372)) {
		/*
		 * 0x6d728.  Bit 1 alone is not enough for the call: the whole
		 * halfword has to be 2, so a +0x3588 of 3 or 6 arms the
		 * recovery without re-initialising anything.
		 */
		if (T3M_U16(f, T3M_F3588) == 2) {
			v34handshakinit(f->obj, 0);
			*(unsigned char **)(f->m + T3M_SELFPTR) =
				f->m + T3M_MSGREC0;
		}

		t3m_errrec_arm(f);
		t3m_errrec_core(f);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("errorrecovery for info0 is "
					     "initialized in RX_PHASE1_CALL\n");

		/* 0x6d94c and 0x6d9e6 -- the counter a THIRD time. */
		c = (short)T3M_U16(f, T3M_COUNTER);
	}

	/*
	 * Three disjuncts and three DIFFERENT out-of-line exits:
	 * `cmp $0x4b0,%cx`/`jle 6ade3`, `cmpw $0x0,0x3588`/`jne 6c3e0` and
	 * `cmpw $0x0,0xaae2`/`je 6e322`.  Each of the three branches away, so
	 * the reset below is the fall-through and runs only when all three
	 * fail.
	 */
	/* 0x6608b, 0x6609d and 0x660ab. */
	if (c <= 0x4b0 || T3M_U16(f, T3M_F3588) != 0
	    || (unsigned short)f->obj->fsk.sr == 0) {
		t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
		return;
	}

	/*
	 * 0x660b9's first instruction is `mov $0x4,%ebx` -- the `= 4` form of
	 * the reset, not the `|= 1` one block 2 above takes at 0x6d709.
	 */
	/* 0x660b9. */
	t3m_errrec_reset(f);
	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * A dispatch arm this reconstruction has not written.
 *
 * `v34handshak` is being landed one arm at a time against
 * test/harness/v34hsstep.c and most of table 1 is still missing, so this is
 * the coarse form of `t3m_notwritten` above: it says "some path with no
 * reconstruction ran" without saying which, for the call sites that predate
 * the codes.  It stops, or records and returns, on the same rule as every
 * other unwritten path -- see the comment on `t3m_notwritten`.
 */
static void
t3c_unwritten(void)
{
	t3m_notwritten(T3M_UNWRITTEN_OTHER);
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
 * The once-per-block transmit dispatch at 0x62af1, table 2 at .rodata+0x2ee8,
 * and the 88-instruction tail at 0x62a40 every one of its arms falls into.
 *
 * Every microstate arm below leaves through here: the microstate machine is
 * a set of guards in front of the transmit machine rather than sixteen
 * independent bodies (finding F288), so a microstate case cannot be landed
 * without whichever transmit arm its own txstate selects.
 *
 * THIS IS NOW A SHIM ROUND `t3m_txblock`, and the reason is measured rather
 * than tidy.  THREE readings of table 2 and its tail existed; the two that
 * came into this file were collapsed by finding F550 and the third, which had
 * been `src/pump/v34/v34hstxblock.c`, by finding F591.  `t3m_txblock`/
 * `t3m_tail` is the survivor because it is the only one whose SIGNATURE can
 * hold the object's behaviour: the tail's `tx` is the value in `%cx` that
 * whichever arm jumped here left, and 0x62b5f re-reads +0x3596 over it, so a
 * dispatch that takes only the object cannot tell the two readings apart at
 * all.  It also writes 0x64884, 0x62b2f and 0x64a4f, which this side left
 * calling `t3c_unwritten`.
 *
 * The `%cx` the object's `jmp 62af1` leaves is the object's own txstate at
 * every one of these call sites, so the shim reads it here.
 */
static void
t3c_txblock(struct v34_object *obj)
{
	struct t3m_frame f;

	t3m_frame_init(&f, obj);
	t3m_txblock(&f, hs_get(obj, HS_TXSTATE));
}

/*
 * The dispatch on its own, for `t_v34hstbl2.c`.
 *
 * The harness in test/harness/v34hsstep.h can steer the object into this one
 * dispatch and no other, and over that domain the dispatch IS the whole of
 * `v34handshak` -- the guards read three halfwords and branch, and nothing
 * else in the 61,541 bytes runs (finding F361).  That is what makes table 2
 * comparable against the blob without the other three dispatches existing,
 * and it is the only reason this name is external.
 *
 * IT IS `t3c_txblock` AND NOT A SECOND SPELLING OF IT.  Every route in reads
 * +0x3596 on the instruction BEFORE the entry -- 0x62aea is `movzwl
 * 0x3596(%edi),%ecx` and 0x62af1 is already the dispatch, `movswl %cx,%eax;
 * sub $0x5,%eax; cmp $0x45,%eax; ja 62a40` -- so `%cx` arrives holding the
 * object's own txstate whichever route was taken, and a test entry that took
 * a txstate would be testing a call the object never makes.  The read belongs
 * to the routes in and not to the dispatch, and one route proves it: the exit
 * at 0x6c991 jumps to 0x62af1 with NO re-read at all, because `%cx` already
 * holds MOH_CLEARDOWN -- 0x54 -- either from the read at 0x6c90c or from the
 * literal at 0x6c939 that follows the transition storing it at 0x6c932.  What that costs is recorded rather
 * than hidden: the reload at 0x62b5f cannot be reached with a `tx` that
 * differs from +0x3596 through this entry, so the one thing `t3m_tail` models
 * that its predecessors did not is not tested from here.  Finding F591.
 */
void
v34handshak_txblock(struct v34_object *obj)
{
	t3c_txblock(obj);
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
		/*
		 * 0x6abae.  Bit 0 clear -- `testb $0x1,0xaae2` at 0x65c81 --
		 * and the counter at +0xaa78 keeps whatever it had: the copy
		 * from +0xaa7c is on the other side of that branch, at 0x65c8e
		 * and 0x65c99.
		 */
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
		/*
		 * 0x6afc4.  The detector said nothing -- `test %ax,%ax` at
		 * 0x65806 -- and the counter step at 0x657ea/0x657eb stands,
		 * so the count below resumes from it on the next block.
		 */
		t3c_txblock(obj);			/* 0x6afc4 */
		return;
	}

	if (hs_get(obj, T3C_COUNT) < hs_get(obj, T3C_FABFC)) {
		/*
		 * 0x6c701.  `cmp 0xabfc(%esi),%ax` at 0x6581d and a SIGNED
		 * `jl`: the count and the limit are both halfwords and both
		 * read off the object by adjacent instructions.
		 */
		t3c_txblock(obj);			/* 0x6c701 */
		return;
	}

	hs_setstate(obj, HS_TXSTATE, V34HS_TX_DPSK);

	/*
	 * 0x6d57c.  THE GUARD IS 32 BITS WIDE (`cmpl $0x1,0xabf0`) and the
	 * two sides of it choose a different NEXT MICROSTATE, not a different
	 * amount of work: MHfrr re-aims the tone detector and waits for the
	 * far end's carrier to DROP, anything else goes straight to DET_SYNC.
	 * Both then run the block below, which 0x6d57c's own exit at 0x6589f
	 * joins past the DET_SYNC store.
	 *
	 * AND THIS IS 0x6ebdf'S CALL WITH ONE CONSTANT CHANGED.  Microstate
	 * 44's Modem-on-Hold accept arm builds the same detector -- same
	 * coefficients, chosen the same way by `role`, same polarity, same
	 * warm-up, same thresholds, and the same +0x356a store and
	 * MOH_TONE_DROP after it.  The one difference is `limit`, 0xf0 here
	 * against 0x64 there, so this end waits about two and a half times as
	 * long before asserting.  Finding F749.
	 */
	if (obj->moh_message == 1) {
		detectorinit(T3C_DET(obj),
			     obj->role == 0x65 ? c2400_ : c1200_,
			     1, 0xf0, 0x32, 0x800, 0x400);
		/*
		 * 0x6d5db sits BETWEEN the microstate load at 0x6d5d4 and the
		 * `cmp $0x50` at 0x6d5e2, so +0x356a takes its 1 whether or
		 * not the transition below turns out to be a move.
		 */
		hs_put(obj, T3C_F356A, 1);		/* 0x6d5db */
		hs_setstate(obj, HS_MICROSTATE, V34HS_MOH_TONE_DROP);
	} else {
		hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);
	}

	obj->fsk.phase = 0;
	obj->fsk.nbits = 0;
	hs_put(obj, T3C_BLK_A97C + 0x14, -1);
	hs_put(obj, T3C_BLK_A97C + 0x18, 8);
	hs_put(obj, T3C_COUNT, 0);
	t3c_putp(obj, T3C_PTR_AA70, (char *)obj + T3C_BLK_A97C);
	t3c_putp(obj, T3C_PTR_AA6C, (char *)obj + T3C_BLK_A94C);
	hs_put(obj, T3C_F358C, 0);
	obj->vect_idx = 0;

	t3c_txblock(obj);
}

static void
t3c_micro_moh_tone_drop(struct v34_object *obj)
{
	if (t3c_moh_step_detector(obj) != 0
	    && t3c_getb(obj, T3C_FABF8) == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34Handshake: Detected signal "
					     "drop on FRR request, time to " "move to phase1...\r\n");
		t3c_putb(obj, T3C_FABF8, 1);
	}

	/* The round-trip delay sets how long the drop has to persist. */
	if ((int)hs_get(obj, T3C_COUNT) < (((int)obj->rtd) >> 2) + 0x12c0) {
		/*
		 * 0x6aae6.  Both halfwords are sign-extended before the
		 * arithmetic -- `movswl 0xaa7e`, `movswl 0xaa78`, `sar $0x2`,
		 * `add $0x12c0` -- so a negative delay shortens the wait
		 * rather than wrapping it.
		 */
		t3c_txblock(obj);			/* 0x6aae6 */
		return;
	}

	/*
	 * 0x6c8f8.  THE GUARD IS A BYTE (`cmpb $0x0,0xabf9`) and it is not
	 * symmetrical with the one microstate 79 takes four fields away: this
	 * one picks between giving up and retraining, and the fall-through --
	 * which is the retrain -- is the case the byte being zero selects.
	 *
	 * GIVING UP IS FOUR STORES AND NO CALL.  The transmit machine goes to
	 * MOH_CLEARDOWN and the receive machine to WAIT, +0xabe4 and +0xabe2
	 * are both raised, and the arm leaves through the once-per-block
	 * dispatch like every other path here -- no `v34handshakinit`, so
	 * findings F359 and F324 do not apply to it and the library tables stay
	 * where the bring-up put them.  Both state compares are LIVE, unlike
	 * 81's: nothing on the way in constrains either word.
	 */
	if (t3c_getb(obj, T3C_FABF9) != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("MOH: Timeout waiting for MH " "sequence under MHfrr, "
					     "disconnecting...\r\n");

		hs_setstate(obj, HS_TXSTATE, V34HS_MOH_CLEARDOWN);
		hs_setstate(obj, HS_RXSTATE, V34HS_WAIT);
		hs_put(obj, T3C_FABE4, 1);		/* 0x6c983 */
		/*
		 * 0x6c983 and 0x6c98a are two halfword stores of the same
		 * constant, out of %ax and %di loaded at 0x6c979 and 0x6c97e
		 * -- so both fields are shorts, where the two guards that
		 * chose this path read bytes.
		 */
		obj->short_abe2 = 1;				/* 0x6c98a */

		/*
		 * 0x62af1 IS the dispatch entry and not one of the stubs that
		 * re-read +0x3596: 0x6c991 jumps straight in, because the
		 * transition at 0x6c8f8 already left MOH_CLEARDOWN in %cx,
		 * which the four stores between 0x6c93e and 0x6c991 do not
		 * disturb.
		 */
		t3c_txblock(obj);			/* 0x62af1 */
		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("MOH: Timeout waiting for MH sequence "
				     "under MHfrr, initiating retrain\r\n");

	v34handshakinit(obj, 1);
	hs_put(obj, T3C_FABE6, 1);

	t3c_txblock(obj);
}

/*
 * ---------------------------------------------------------------------------
 * Microstate 41 `DET_SYNC`, .rodata+0x3000's entry at 0x669a4 -- 3,945 bytes
 * by cfgsplit's exclusive count and the third largest arm of the table.
 *
 * WHAT THE STATE IS.  DET_SYNC is the receiver waiting for the far end's
 * synchronisation pattern during V.34 phase 2.  Everything the arm does is
 * decided by four fields it reads on the way in and one of three values in
 * +0x358a, which is the arm's own sub-state:
 *
 *     +0xaae2   0, -1, or a low byte of 0x72; the entry test is on the BYTE
 *     +0xabe8   byte, non-zero takes the second door into the junction
 *     +0x358a   0 nothing yet, 1 a tone is being waited for, 2 info marks
 *     +0xaae0   short, blocks spent in the state, against three thresholds
 *
 * and every path leaves through the once-per-block transmit dispatch, which
 * is finding F288: a microstate arm is a microstate arm AND a transmit arm.
 * The five transmit states this one can leave behind -- 24 `TX_DPSK`, 60
 * `TONE_AB` and 74 `SILENCERETRAIN`, plus whatever it was entered with --
 * all select table 2's arm at 0x644c9 or its default, both of which exist.
 *
 * THE MACRO PREFIX IS `T41_` and it is not decoration: three agents are
 * writing arms of this function into this one translation unit at the same
 * time, and finding F325 is what one collided `#define` cost.  Where an offset
 * already has a `T3C_` name it gets a `T41_` one as well rather than a
 * reference to somebody else's block.
 *
 * WHAT IS DELIBERATELY NOT MODELLED, and why it is not a gap.  Four compares
 * in this arm test the microstate the DISPATCH read, cached in `%si`, rather
 * than re-reading +0x3592: 0x6c862 against 44, 0x6d3f4's neighbour at
 * 0x6e044 against 58.  The arm is reachable only with +0x3592 == 41 and
 * nothing on the way in writes it (finding F285), so the cached value and the
 * field are the same value, and `hs_setstate`'s own "already there" guard
 * reproduces each of them exactly.  They are written as transitions, not as
 * branches that could never be taken.
 */

#define T41_DETECTOR	0x3564	/* struct v34_detector, inside the object   */
#define T41_F3588	0x3588	/* short: bit 0 and bit 1, both set here    */
#define T41_F358A	0x358a	/* short: the arm's own sub-state, 0/1/2    */
#define T41_F358C	0x358c	/* short: cleared on three ways out         */
#define T41_F35A0	0x35a0	/* short: a counter this arm steps and caps */
#define T41_BLK_A94C	0xa94c	/* the 0x30-byte record 0x70bdd fills       */
#define T41_BLK_A9AC	0xa9ac	/* the one 0x6d387 fills, and aims +0xaa6c  */
#define T41_PTR_AA6C	0xaa6c
#define T41_PTR_AA70	0xaa70
#define T41_COUNT	0xaa78	/* short: the counter, and the trace's [2]  */
#define T41_FAA7A	0xaa7a	/* short: cleared unconditionally on entry  */
#define T41_RTD		0xaa7e	/* short: the round-trip delay, >> 4 here   */
#define T41_ABAE	0xabae	/* short[10]: the info record staged here   */
#define T41_FABC2	0xabc2	/* short: the last index of it to copy out  */
#define T41_FABE8	0xabe8	/* byte: non-zero takes the second door     */
#define T41_FABF8	0xabf8	/* byte: "the drop has been reported"       */
#define T41_RX_SAMPS	0x010c	/* receiver: where the detector reads from  */

#define T41_RX(obj)	((struct v34_receiver *)((char *)(obj) + 0x0264))
#define T41_DET(obj)	((struct v34_detector *)((char *)(obj) + T41_DETECTOR))

static void *
t41_getp(const struct v34_object *obj, unsigned off)
{
	return *(void *const *)((const char *)obj + off);
}

/*
 * The detector, run over exactly the samples `V34agc` just delivered.
 *
 * Two paths of this arm call it and neither steps a counter first, which is
 * what distinguishes them from 79 and 80's `t3c_moh_step_detector`.  The
 * range is not this code's choice: `V34agc` ends by setting `rx_samples`, so
 * the end pointer is already the queue's, and finding F357 is what depending
 * on that costs a test that wants to move it.
 */
static short
t41_detect(struct v34_object *obj)
{
	struct v34_receiver *rx = T41_RX(obj);

	return (short)tone_detect(rx, T41_DET(obj),
				  (const short *)((const char *)rx
						  + T41_RX_SAMPS),
				  rx->rx_samples);
}

/* The first five stores of one 0x30-byte record; +0x18 is what varies. */
static void
t41_record_head(struct v34_object *obj, unsigned base, short f18)
{
	hs_put(obj, base + 0x14, -1);
	hs_put(obj, base + 0x1a, 0);
	hs_put(obj, base + 0x1e, 0);
	hs_put(obj, base + 0x22, 0);
	hs_put(obj, base + 0x18, f18);
}

/*
 * 0x6dca7: the far end refused the FRR request, reported once.
 *
 * Reached only with +0xabe8 non-zero, +0x358a outside {1, 2} and +0xabf0
 * exactly 1.  Three guards and one flag; the +0xaae2 test is SIXTEEN bits
 * wide here (`cmpw`) where the arm's entry test on the same field is eight.
 */
static void
t41_frr_nack(struct v34_object *obj)
{
	/*
	 * 0x6dca7's `cmpw $0x31,0x35a0` and `jle 6dd19`: the counter has to be
	 * past 0x31 and not at it, and the compare is signed.
	 */
	if (hs_get(obj, T41_F35A0) <= 0x31) {
		t3c_txblock(obj);			/* 0x6dd19 */
		return;
	}
	if (obj->fsk.sr != 0) {
		/*
		 * 0x6dd06.  `cmpw $0x0,0xaae2` at 0x6dcb1 -- sixteen bits,
		 * where the arm's entry test on the same field is eight.
		 */
		t3c_txblock(obj);			/* 0x6dd06 */
		return;
	}
	if (t3c_getb(obj, T41_FABF8) != 0) {
		/*
		 * 0x6dcf3.  `cmpb $0x0,0xabf8` at 0x6dcbb: the drop has
		 * already been reported, and reporting it is all the
		 * fall-through has left to do.
		 */
		t3c_txblock(obj);			/* 0x6dcf3 */
		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34Handshake: Detected signal drop on "
				     "FRR request (after NACK), time to move " "to phase1...\r\n");

	t3c_putb(obj, T41_FABF8, 1);
	t3c_txblock(obj);
}

/*
 * 0x6ab33, where four of the arm's paths converge: the second door's own
 * two exits, and the two early returns out of +0x358a's 1 and 2 bodies.
 *
 * `abe8` is the byte read at 0x669cd -- and re-read at 0x6de70 and 0x6de83
 * on the two paths that come back here from the detector, which is why it is
 * a parameter rather than a field read once.
 */
static void
t41_after_guards(struct v34_object *obj, unsigned char abe8)
{
	/*
	 * 0x6ab33's `test %cl,%cl` is a BYTE test of the +0xabe8 its callers
	 * loaded, and 0x669fa is also where the arm's first door leaves -- one
	 * stub for two paths.
	 */
	if (abe8 == 0) {
		t3c_txblock(obj);			/* 0x669fa */
		return;
	}
	/*
	 * 0x6ab42's `cmpl $0x1,0xabf0` is THIRTY-TWO bits wide, so a low
	 * halfword of 1 with anything at all in the high half does not reach
	 * 0x6dca7.
	 */
	if (obj->moh_message == 1) {
		t41_frr_nack(obj);			/* 0x6dca7 */
		return;
	}
	/* 0x6ab4f, the fall-through: +0xabe8 set, +0xabf0 not 1. */
	t3c_txblock(obj);				/* 0x6ab4f */
}

/*
 * 0x6c862: the low byte of +0xaae2 is 0x72, and the state moves to DET_INFO.
 *
 * The record +0xaa70 points at is invalidated, the block counter and the
 * trace counter are cleared, and the receiver's detector-pending flag is
 * lowered -- but only when +0x358a says no sub-state is running.
 */
static void
t41_to_det_info(struct v34_object *obj)
{
	struct v34_receiver *rx = T41_RX(obj);
	short *rec = (short *)t41_getp(obj, T41_PTR_AA70);

	hs_setstate(obj, HS_MICROSTATE, V34HS_DET_INFO);

	*(short *)((char *)rec + 0x14) = -1;
	obj->fsk.nbits = 0;

	if (hs_get(obj, T41_F358A) == 0)
		rx->flags = (unsigned short)(rx->flags
					     & ~V34_RX_FLAG_DET_PENDING);

	hs_put(obj, T41_COUNT, 0);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "DET_SYNC is received in RX_DPSK,rx->gain=0x%x\n",
		    (int)rx->agc_gain);

	t3c_txblock(obj);
}

/*
 * 0x709e1: the state has run long enough that the tone detector is consulted.
 *
 * Entered with +0xabe8 zero, +0x358a zero and +0xaae0 past 100 blocks.  The
 * detector is armed down first -- the flag is lowered whatever it answers --
 * and a silent detector before block 900 simply leaves.  Otherwise the arm
 * restarts its own sub-state at 1, stages ten zero words, drops the transmit
 * machine into TX_DPSK and re-enters itself, and configures the record at
 * +0xa94c for the tone it is now waiting for.
 */
static void
t41_tone_search(struct v34_object *obj)
{
	struct v34_receiver *rx = T41_RX(obj);
	short det;
	int i;

	rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_DET_PENDING);

	det = t41_detect(obj);
	if (det == 0 && obj->fsk.nbits <= 0x384) {
		/*
		 * 0x70f8f.  Both halves of the guard: `test %ax,%ax` at
		 * 0x70a1b and `cmpw $0x384,0xaae0` at 0x70a20.  The flag
		 * lowered at 0x709f8 stays lowered on this exit -- it is
		 * cleared before the detector runs, not after it answers.
		 */
		t3c_txblock(obj);			/* 0x70f8f */
		return;
	}

	obj->is_short = 0;
	obj->local_short = 0;
	hs_put(obj, T41_F358A, 1);
	hs_put(obj, T41_F3588, (short)(hs_get(obj, T41_F3588) | 1));

	for (i = 0; i <= 9; i++)
		hs_put(obj, T41_ABAE + 2u * (unsigned)i, 0);

	hs_put(obj, T41_FABC2, 0);

	hs_setstate(obj, HS_TXSTATE, V34HS_TX_DPSK);
	hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);

	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	/*
	 * 0x70d35.  The longer warm-up is taken only when the far end's
	 * marker says 0x65 AND the V.90 receiver is present; the object reads
	 * that int through `obj + 4`, so it is +0x24c and not +0x248.
	 */
	if (obj->role == 0x65 && obj->v90_receiver != 0)
		/*
		 * ONE record with two heads.  0x70bd7 falls into the 0x11 head
		 * at 0x70bdd, and 0x70d41's `test`/`je` jumps BACK to it when
		 * the int loaded at 0x70d39 is zero, so 0x70d47 is reached
		 * only with both conditions -- and both heads end at 0x70c07.
		 */
		t41_record_head(obj, T41_BLK_A94C, 0x1e);	/* 0x70d47 */
	else
		t41_record_head(obj, T41_BLK_A94C, 0x11);	/* 0x70bdd */

	/*
	 * 0x70c07 is where the two heads MEET: it is the next instruction
	 * after the 0x11 arm's `movw $0x11,0x18` and it is the 0x1e arm's
	 * `jmp` target at 0x70d6c, so everything from here down is emitted
	 * once.
	 */
	hs_put(obj, T41_BLK_A94C + 0x1c, 8);			/* 0x70c07 */
	hs_put(obj, T41_BLK_A94C + 0x16, 1);
	t3c_puti(obj, T41_BLK_A94C + 0x24, 0xf72);
	t3c_puti(obj, T41_BLK_A94C + 0x2c, 0xf72);
	hs_put(obj, T41_BLK_A94C + 0x28, 0xc);
	hs_put(obj, T41_BLK_A94C + 0x2a, 0xc);
	hs_put(obj, T41_BLK_A94C + 0x20, 1);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "errorrecovery is initialized in DET_SYNC\n");

	t3c_txblock(obj);
}

/*
 * 0x6da9c: +0x358a is 1, so a tone is being waited for.
 *
 * The detector answers, and a silent one -- or a record at +0xaa6c whose
 * byte at +0x04 has not got bit 7 -- puts the arm straight back on the
 * junction's tail.  Otherwise the sub-state is cleared, the staged words are
 * copied out through +0xaa70, and the state moves on: to 46
 * `TX_PHASE1_ANS`, or to 58 `RX_PHASE1_CALL` when the marker at +0x359c says
 * 0x65.  Only the first of those two invalidates +0xaae2.
 */
static void
t41_tone_ab(struct v34_object *obj)
{
	struct v34_receiver *rx = T41_RX(obj);
	const unsigned char *rec;
	short *out;
	int i;

	/*
	 * 0x6dac7's `test %ax,%ax` sends a silent detector to 0x6de83, which
	 * re-reads +0xabe8 with a `movzbl` before jumping to the junction --
	 * the re-read the note above says is why `abe8` is a parameter.
	 */
	if (t41_detect(obj) == 0) {
		t41_after_guards(obj, t3c_getb(obj, T41_FABE8)); /* 0x6de83 */
		return;
	}

	rec = (const unsigned char *)t41_getp(obj, T41_PTR_AA6C);
	if ((rec[4] & 0x80) == 0) {
		/*
		 * 0x6dae1's `testb $0x80,0x4(%eax)` on the record +0xaa6c
		 * points at, and 0x6de70 re-reads +0xabe8 exactly as 0x6de83
		 * does -- two stubs three instructions apart, one junction.
		 */
		t41_after_guards(obj, t3c_getb(obj, T41_FABE8)); /* 0x6de70 */
		return;
	}

	hs_put(obj, T41_F358A, 0);

	/*
	 * 0x6dafa is `js`, so a negative count copies nothing at all, and
	 * the loop itself is a do/while over 0..+0xabc2 INCLUSIVE -- the
	 * test at 0x6db1b compares the already-incremented index.
	 */
	if (hs_get(obj, T41_FABC2) >= 0) {
		out = (short *)t41_getp(obj, T41_PTR_AA70);
		for (i = 0; (short)i <= hs_get(obj, T41_FABC2); i++)
			out[i] = hs_get(obj, T41_ABAE + 2u * (unsigned)i);
	}

	hs_put(obj, T41_COUNT, 0);

	if (obj->role == 0x65) {
		hs_setstate(obj, HS_MICROSTATE, V34HS_RX_PHASE1_CALL);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Tone AB detected ending " "errorrecovery. Switch to "
					     "RX_PHASE1_CALL.\n");
	} else {
		obj->fsk.sr = -1;
		hs_setstate(obj, HS_MICROSTATE, V34HS_TX_PHASE1_ANS);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Tone AB detected ending " "errorrecovery. Switch to "
					     "TX_PHASE1_ANS.\n");
	}

	/* 0x6dbf3, which both of the two above fall into. */
	rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_DET_PENDING);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34INFO, info0 received in DET_SYNC\n");

	if (DSPLIB_DEBUG_ON()) {
		const unsigned short *p =
		    (const unsigned short *)t41_getp(obj, T41_PTR_AA70);

		dsplibs_debug_printf(
		    "%s 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
		    "V34INFO, rxinfo0",
		    (unsigned)p[0], (unsigned)p[1], (unsigned)p[2],
		    (unsigned)p[3], (unsigned)p[4], (unsigned)p[5],
		    (unsigned)p[6], (unsigned)p[7], (unsigned)p[8],
		    (unsigned)p[9]);
	}

	t3c_txblock(obj);
}

/*
 * 0x6dfc2, the late half of +0x358a == 2: the third and largest of the
 * arm's three thresholds on +0xaae0, (rtd >> 4) + 490.
 *
 * Below it the arm simply leaves.  At or past it there are three answers,
 * and which one depends on the marker at +0x359c and on +0xa24a.
 */
static void
t41_marks_late(struct v34_object *obj, short aae0)
{
	if ((int)aae0 < (((int)obj->rtd) >> 4) + 0x1ea) {
		/*
		 * 0x6dfd5 is the `jl` itself, straight into the dispatch entry
		 * with no stub, and the delay it compares against is the spill
		 * at 0x42(%esp) -- read there at 0x6dfc2, not off the object.
		 */
		t3c_txblock(obj);			/* 0x6dfd5 */
		return;
	}

	if (obj->role != 0x65) {
		if (obj->retrain_state != 1) {
			/*
			 * 0x6dffb, another `jne` straight to the dispatch, on
			 * the `cmpw $0x1,0xa24a` at 0x6dff3 -- sixteen bits
			 * wide.
			 */
			t3c_txblock(obj);		/* 0x6dffb */
			return;
		}
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("DET_SYNC not detected, during "
					     "search for info1c initiating a " "retrain\n");
		/*
		 * 0x6e029.  The second argument is built at 0x6e01d as a
		 * literal 1, and 0x6e02e reloads +0x3596 after the call, so
		 * the dispatch runs on whatever `v34handshakinit` left there.
		 */
		v34handshakinit(obj, 1);		/* 0x6e029 */
		t3c_txblock(obj);
		return;
	}

	/* 0x6e03a, and +0x3588 gets bit 1 before any transition. */
	hs_put(obj, T41_F3588, (short)(hs_get(obj, T41_F3588) | 2));
	hs_setstate(obj, HS_MICROSTATE, V34HS_RX_PHASE1_CALL);
	hs_setstate(obj, HS_RXSTATE, V34HS_RX_DPSK);
	hs_setstate(obj, HS_TXSTATE, V34HS_TONE_AB);

	hs_put(obj, T41_F358C, 0);
	obj->vect_idx = 0;
	hs_put(obj, T41_COUNT, 0);
	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DET_SYNC not detected, during search " "for info1a\n");

	t3c_txblock(obj);
}

/*
 * 0x6ce8b: +0x358a is 2, the info marks.
 *
 * Three thresholds on +0xaae0, all of them (rtd >> 4) plus a constant --
 * 100, 400 and 490 -- and the round-trip delay is read ONCE and spilled, so
 * a later write to it could not move the second and third tests.  Between
 * the first two the arm steps a counter at +0x35a0, and only if +0xaae2 is
 * one of its two sentinel values.
 */
static void
t41_info_marks(struct v34_object *obj, unsigned short aae2, unsigned char abe8)
{
	short rtd = obj->rtd;
	short aae0 = obj->fsk.nbits;
	int base = ((int)rtd) >> 4;
	short next;
	short tx;

	if ((int)aae0 <= base + 100) {
		/*
		 * 0x6ceba's `jle` goes straight to the junction at 0x6ab33
		 * with no stub, and 0x6cead has already spilled the delay to
		 * 0x42(%esp) -- the copy `t41_marks_late` reads at 0x6dfc2.
		 */
		t41_after_guards(obj, abe8);		/* 0x6ceba */
		return;
	}

	/*
	 * 0x6ceca.  The reset is BEFORE the sentinel step at 0x6cef0, so
	 * below the second threshold +0x35a0 never climbs: it is zero or
	 * one whatever the sentinel does (finding F391).
	 */
	if ((int)aae0 <= base + 400)
		hs_put(obj, T41_F35A0, 0);		/* 0x6ceca */

	next = 0;
	if (aae2 == 0 || aae2 == 0xffff)
		next = (short)((unsigned short)hs_get(obj, T41_F35A0) + 1);

	tx = hs_get(obj, HS_TXSTATE);
	/*
	 * 0x6da8e, out of line: `cmp $0x3c,%cx` at 0x6cf0f -- TONE_AB --
	 * jumps here, zeroes +0x35a0 and returns to 0x6cf27 past the store,
	 * so transmitting the tone discards the stepped value.
	 */
	if (tx == V34HS_TONE_AB)
		hs_put(obj, T41_F35A0, 0);		/* 0x6da8e */
	else
		hs_put(obj, T41_F35A0, next);

	/*
	 * 0x6cf39.  The base is rebuilt here from the delay spilled at
	 * 0x6cead -- `movswl 0x42(%esp)` then `sar $0x4` at 0x6cf27 -- and
	 * `jl 62af1` is the once-per-block transmit dispatch (finding F390).
	 */
	if ((int)aae0 < base + 400) {
		t3c_txblock(obj);			/* 0x6cf39 */
		return;
	}

	/*
	 * 0x6cf4e tests a +0x35a0 RE-READ at 0x6cf46, not the value the
	 * step above stored: the TONE_AB arm at 0x6da8e wrote zero there
	 * and rejoined below the store, so only memory has the answer.
	 */
	if (hs_get(obj, T41_F35A0) <= 0x31) {
		t41_marks_late(obj, aae0);		/* 0x6cf4e */
		return;
	}

	if (aae2 == 0xffff) {
		/*
		 * 0x6d2f8.  The record at +0xa9ac is configured and +0xaa6c
		 * aimed at it -- an INTERIOR pointer, which the fixture
		 * compares by offset from each side's own base (finding F359).
		 */
		hs_setstate(obj, HS_TXSTATE, V34HS_TX_DPSK);

		t41_record_head(obj, T41_BLK_A9AC, 0x4d);
		hs_put(obj, T41_BLK_A9AC + 0x1c, 8);
		hs_put(obj, T41_BLK_A9AC + 0x16, 1);
		hs_put(obj, T41_BLK_A9AC + 0x28, 0x10);
		hs_put(obj, T41_BLK_A9AC + 0x2a, 0x10);
		hs_put(obj, T41_BLK_A9AC + 0x20, 0);
		t3c_putp(obj, T41_PTR_AA6C, (char *)obj + T41_BLK_A9AC);
		t3c_puti(obj, T41_BLK_A9AC + 0x24, 0xff72);
		t3c_puti(obj, T41_BLK_A9AC + 0x2c, 0xff72);
		hs_put(obj, T41_F358C, 0);

		hs_setstate(obj, HS_MICROSTATE, V34HS_INFODONE);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34PHASE2,detected INFOMARKS, "
					     "reinitializing Info1c\n");

		t3c_txblock(obj);
		return;
	}

	if (aae2 != 0) {
		/*
		 * 0x6cf61.  Finding F391's third answer: -1 took 0x6d2f8 at
		 * 0x6cf58 and zero falls through to 0x6cf67, so every other
		 * +0xaae2 lands in the late half.
		 */
		t41_marks_late(obj, aae0);		/* 0x6cf61 */
		return;
	}

	/* 0x6cf67, +0xaae2 exactly zero -- finding F391's second answer. */
	hs_put(obj, T41_F3588, (short)(hs_get(obj, T41_F3588) | 2));
	hs_setstate(obj, HS_TXSTATE, V34HS_SILENCERETRAIN);
	hs_setstate(obj, HS_RXSTATE, V34HS_WAIT);

	hs_put(obj, T41_COUNT, 0);
	obj->vect_idx = 0;
	hs_put(obj, T41_F358C, 0);
	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DET_SYNC not detected, but Tone is "
				     "detected during info1\n");

	t3c_txblock(obj);
}

/*
 * Microstate 41 `DET_SYNC`, 0x669a4.
 *
 * The entry stores one halfword unconditionally and then takes at most four
 * guards.  The first of them reads +0xaae2 as a BYTE (`cmp $0x72,%dl` on a
 * halfword that was zero-extended), which is the same field the two bodies
 * below test sixteen bits wide; both spellings are the object's.
 */
static void
t41_micro_det_sync(struct v34_object *obj)
{
	unsigned short aae2 = (unsigned short)obj->fsk.sr;
	unsigned char abe8;
	short short_358a;

	hs_put(obj, T41_FAA7A, 0);

	if ((unsigned char)aae2 == 0x72) {
		/*
		 * 0x6c862 opens with `cmp $0x2c,%si` -- `hs_setstate`'s
		 * folded guard on DET_INFO, and one the dispatch that read
		 * 41 can never satisfy (finding F392).
		 */
		t41_to_det_info(obj);			/* 0x6c862 */
		return;
	}

	abe8 = t3c_getb(obj, T41_FABE8);
	short_358a = hs_get(obj, T41_F358A);

	if (abe8 == 0 && short_358a == 0) {
		if (obj->fsk.nbits > 0x64) {
			/*
			 * 0x709e1 clears bit 9 of the receiver's
			 * +0x122 unconditionally (`and $0xfffffdff`
			 * at 0x709f2), where the DET_INFO body only
			 * does it when +0x358a is clear (finding F390).
			 */
			t41_tone_search(obj);		/* 0x709e1 */
			return;
		}
		/*
		 * 0x669fa.  A hundred blocks have not passed, so nothing
		 * happens this call beyond the entry's +0xaa7a clear; the
		 * tail loads +0x3596 into %ecx and jumps to 0x62af1.
		 */
		t3c_txblock(obj);			/* 0x669fa */
		return;
	}

	/* 0x6ab21, which the two doors above arrive at with the same two. */
	/*
	 * 0x6ce8b and 0x6da9c, the junction's two bodies.  The first reads
	 * the delay at +0xaa7e and spills it to 0x42(%esp) at 0x6cead, so
	 * every threshold below sees the value the first one saw; the
	 * second loads the receiver and aims the detector at obj+0x3564
	 * (0x6dab1) before anything else.
	 */
	if (short_358a == 2) {
		t41_info_marks(obj, aae2, abe8);	/* 0x6ce8b */
		return;
	}
	if (short_358a == 1) {
		t41_tone_ab(obj);			/* 0x6da9c */
		return;
	}
	t41_after_guards(obj, abe8);
}

/*
 * ---------------------------------------------------------------------------
 * Microstate 46 `TX_PHASE1_ANS`, 0x65d6d -- 3,198 bytes exclusive to this one
 * dispatch entry, in twenty-five ranges scattered over the function.
 *
 * WHAT THE ARM IS.  The answerer has sent its phase-1 INFO0 and is waiting to
 * hear the caller's back.  Four things can happen and each has its own body,
 * and the four bodies are the SAME reset with a different prelude and a
 * different message -- they rearm the outbound record at +0xa94c, clear the
 * message buffer at +0xabae, and hand the machine to TX_DPSK/DET_SYNC:
 *
 *     0x65df4   the counter is past 1200 with +0xaae2 non-zero
 *     0x6abc1   the counter is past 199 and +0xaae2's low ten bits are 0x372
 *     0x6c459   +0x3588 is 2, and +0xaa7a has counted past twelve
 *     0x6f90c   the transmit state is TONE_AB and +0xac00 is set
 *
 * Only the last of the four builds a message: it aims +0xaa6c and +0xaa70 at
 * the two records and calls `V34SetINFO0aBits`, and `V34SetINFO0dBits` too
 * when +0x359c is 0x66.  The other three rearm and say so.
 *
 * `t46_reset_core` is that shared reset written once.  The four bodies write
 * its fields in three different orders, which is immaterial -- they are
 * distinct locations and no read is interleaved -- and the difference between
 * them is entirely in the prelude and the message.  What is NOT shared is the
 * `+0x3588` write: three bodies STORE 4 and 0x6f90c ORs it in.
 *
 * THE FIVE READS OF +0xaae2 HERE ARE SIXTEEN BITS WIDE, one of them a compare
 * of the whole halfword against zero, and they are now `obj->fsk.sr` -- which
 * is what +0xaae2 is.  `T3C_FAAE2` survives for the ONE reader that is a byte
 * wide, microstate 62's guard at 0x65c8a, and finding F553 is why that one
 * cannot be widened to match.
 */

#define T46_V90RX	0x024c	/* int:   `v90_receiver`, and the record's  */
				/*        +0x18 depends on it              */
#define T46_F3588	0x3588	/* short: which of the four bodies is due   */
#define T46_F358A	0x358a	/* short: 1 from a body, 2 from 0x6b4ee    */
#define T46_F358C	0x358c	/* short: bit 0 toggled at 0x6b50e         */
#define T46_INFO0A	0xa94c	/* the record +0xaa6c is aimed at          */
#define T46_INFO0D	0xa97c	/* the record +0xaa70 is aimed at          */
#define T46_COUNT3	0xaa7a	/* short: the "count3" the trace prints    */
#define T46_MSG		0xabae	/* ten shorts, cleared by every body       */
#define T46_MSG_N	10
#define T46_MSG_LAST	0xabc2	/* the eleventh, cleared on its own        */
#define T46_RETRAIN	0xac00	/* byte:  picks 0x6f90c, and is cleared     */

/* Where the two entry guards and the four bodies branch. */
#define T46_CNT_LOW	0xc7	/* the counter, against 199                 */
#define T46_CNT_HIGH	0x4b0	/* and against 1200                         */
#define T46_TONE_LIMIT	0x18f	/* TONE_AB's own, against 399               */
#define T46_COUNT3_LIM	0xc	/* +0xaa7a, against twelve                  */

static void t46_chain_full(struct v34_object *obj);
static void t46_chain_tail(struct v34_object *obj, short sub);
static void t46_past_the_counter(struct v34_object *obj);

/*
 * The outbound record at +0xa94c, rearmed.
 *
 * Twelve stores, and the ONLY thing that varies is +0x18: 0x11 normally, 0x1e
 * when +0x359c is 0x65 and a V.90 receiver is running.  All four bodies test
 * that pair and all four reach the rest of the sequence through the same
 * `+0x1c` store, so the variant is one field and not one body.
 *
 * The `v90_receiver` read is through the object's `obj + 4` base -- 0x248 of
 * it, which is +0x24c of the object.  Finding F354's 0x644c9 writes the
 * progress code through the same base, which is what pins it.
 */
static void
t46_init_record(struct v34_object *obj)
{
	char *r = (char *)obj + T46_INFO0A;
	short arm = (obj->role == 0x65 && obj->v90_receiver != 0)
		    ? 0x1e : 0x11;

	*(short *)(r + 0x14) = -1;
	*(short *)(r + 0x1a) = 0;
	*(short *)(r + 0x1e) = 0;
	*(short *)(r + 0x22) = 0;
	*(short *)(r + 0x18) = arm;
	*(short *)(r + 0x1c) = 8;
	*(short *)(r + 0x16) = 1;
	*(int *)(r + 0x24) = 0xf72;
	*(int *)(r + 0x2c) = 0xf72;
	*(short *)(r + 0x28) = 0xc;
	*(short *)(r + 0x2a) = 0xc;
	*(short *)(r + 0x20) = 1;
}

/*
 * The reset all four bodies share: clear the message, move both machines on,
 * and rearm the record.
 *
 * The two transitions are `hs_setstate`, traces and all -- 0x65e53 and
 * 0x65ef1 are that idiom with the constant arguments folded, and so are the
 * other three bodies' copies.  The microstate compare against DET_SYNC can
 * never fire here (the dispatch that got us in read 46), and the txstate one
 * fires whenever the arm was entered at TX_DPSK.
 */
static void
t46_reset_core(struct v34_object *obj)
{
	int i;

	obj->is_short = 0;
	obj->local_short = 0;
	hs_put(obj, T46_F358A, 1);

	for (i = 0; i < T46_MSG_N; i++)
		hs_put(obj, T46_MSG + 2 * i, 0);
	hs_put(obj, T46_MSG_LAST, 0);

	hs_setstate(obj, HS_TXSTATE, V34HS_TX_DPSK);
	hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);

	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	t46_init_record(obj);
}

/* 0x65df4 -- the counter past 1200. */
static void
t46_body_repeated_late(struct v34_object *obj)
{
	hs_put(obj, T46_F3588, 4);
	t46_reset_core(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Repeated info0 is detected (after 1200), "
				     "errorrecovery is initialized in " "TX_PHASE1_ANS\n");

	t3c_txblock(obj);
}

/* 0x6abc1 -- the counter past 199 with +0xaae2's low ten bits at 0x372. */
static void
t46_body_repeated(struct v34_object *obj)
{
	hs_put(obj, T46_F3588, 4);
	t46_reset_core(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Repeated info0 is detected, errorrecovery "
				     "is initialized in TX_PHASE1_ANS\n");

	/*
	 * 0x6adbd re-reads +0x3588 at 0x6adb6 before `cmp $0x2,%ax`, so
	 * the 4 this body has just stored is what the chain sees and the
	 * only way out is 0x6add0 (finding F418).
	 */
	t46_chain_full(obj);			/* 0x6adbd */
}

/* 0x6c459 -- +0x3588 was 2 and +0xaa7a has run out. */
static void
t46_body_detected(struct v34_object *obj)
{
	hs_put(obj, T46_COUNT3, 0);
	hs_put(obj, T46_F3588, 4);
	t3c_putp(obj, T3C_PTR_AA6C, (char *)obj + T46_INFO0A);
	t46_reset_core(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("info0 is detected, info0 is initialized "
				     "in TX_PHASE1_ANS\n");

	/*
	 * 0x6adc7 enters the chain one compare in, past the `== 2` test.
	 * Its `test %ax,%ax` cannot fire from here -- this body stored 4
	 * into +0x3588 -- which is finding F418's unreachable arm.
	 */
	t46_chain_tail(obj, hs_get(obj, T46_F3588));	/* 0x6adc7 */
}

/*
 * 0x6f90c -- the transmit state is TONE_AB and +0xac00 says a retrain is due.
 *
 * The only body that builds a message rather than just rearming.  Both
 * records are aimed first, `V34SetINFO0aBits` reads `local_short` BEFORE the
 * reset clears it, and +0xaa70 is read back rather than reused.
 */
static void
t46_body_retrain(struct v34_object *obj)
{
	t3c_putp(obj, T3C_PTR_AA6C, (char *)obj + T46_INFO0A);
	t3c_putp(obj, T3C_PTR_AA70, (char *)obj + T46_INFO0D);

	V34SetINFO0aBits(obj, (short *)((char *)obj + T46_INFO0A));
	if (obj->role == 0x66)
		V34SetINFO0dBits(obj,
				 *(short **)((char *)obj + T3C_PTR_AA70));

	hs_put(obj, T46_F3588, (short)(hs_get(obj, T46_F3588) | 4));
	t46_reset_core(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Repeated info0 is initialized on " "retrain\n");

	t3c_putb(obj, T46_RETRAIN, 0);
	t3c_txblock(obj);
}

/*
 * 0x6c3f3 -- +0x3588 is 2: an INFO0 has been seen and is being counted.
 *
 * +0xaa7a is stepped once per block and the arm gives up after twelve; the
 * trace prints the count BEFORE it is stepped.
 */
static void
t46_info0_counting(struct v34_object *obj)
{
	short n;

	if (hs_get(obj, T3C_COUNT) <= T46_CNT_LOW) {
		/*
		 * 0x6add0, the tail the whole chain funnels into: reload
		 * the object, put +0x3596 in %ecx, jump to 0x62af1.  The
		 * guard above it is `cmpw $0xc7,0xaa78(%ebx)` at 0x6c3fa.
		 */
		t3c_txblock(obj);			/* 0x6add0 */
		return;
	}
	if ((obj->fsk.sr & 0xfff) != 0xf72) {
		/*
		 * 0x6add0 again, and this guard is TWELVE bits wide:
		 * `and $0xfff` then `cmp $0xf72` at 0x6c410, where the
		 * head at 0x65dbb masks ten and compares 0x372.  0xf72 &
		 * 0x3ff is 0x372, so this test is the stricter of the two.
		 */
		t3c_txblock(obj);			/* 0x6add0 */
		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("TX_PHASE_ANS: count3 = %d \n",
				     (int)hs_get(obj, T46_COUNT3));

	n = (short)(hs_get(obj, T46_COUNT3) + 1);
	/*
	 * 0x70c7f stores the stepped count and re-enters the chain at
	 * 0x6adc7 with +0x3588 re-read at 0x70c8d -- still the 2 that
	 * got here, so its zero arm cannot fire (finding F418).
	 */
	if (n <= T46_COUNT3_LIM) {			/* 0x70c7f */
		hs_put(obj, T46_COUNT3, n);
		t46_chain_tail(obj, hs_get(obj, T46_F3588));
		return;
	}

	/*
	 * 0x6c459.  The give-up zeroes +0xaa7a at 0x6c462 rather than
	 * leaving the thirteenth step there -- 0x6c447 re-read the count
	 * and `inc %eax`, and only 0x70c7f's arm stores it back.
	 */
	t46_body_detected(obj);				/* 0x6c459 */
}

/*
 * 0x65dcd -- the counter against 1200, and +0xaae2 against zero.
 *
 * Reached both from the head of the arm with +0x3588 clear and from 0x6adc7,
 * and it re-reads the counter at both.
 */
static void
t46_past_the_counter(struct v34_object *obj)
{
	if (hs_get(obj, T3C_COUNT) <= T46_CNT_HIGH) {
		/*
		 * 0x6c120, another copy of the tail: the object, +0x3596
		 * into %ecx, and `jmp 62af1` at 0x6c12e.  The counter was
		 * re-read at 0x65dd4 for the compare above.
		 */
		t3c_txblock(obj);			/* 0x6c120 */
		return;
	}
	/*
	 * 0x65de6 is `cmpw $0x0,0xaae2(%edi)` -- sixteen bits wide, not
	 * the byte spelling microstate 62 keeps (findings F417 and F553) --
	 * and 0x6bd7a is one more copy of the transmit tail.
	 */
	if (obj->fsk.sr == 0) {
		t3c_txblock(obj);			/* 0x6bd7a */
		return;
	}

	/*
	 * 0x65df4 opens with the 1 for +0x358a in %eax and a 4 staged in
	 * %ebp: this body STORES +0x3588, where 0x6f90c ORs into it
	 * (finding F410).
	 */
	t46_body_repeated_late(obj);			/* 0x65df4 */
}

/*
 * 0x6adbd and 0x6adc7, the two doors into one chain on +0x3588.
 *
 * The bodies at 0x6abc1 and 0x6c459 leave through here rather than
 * returning, and they leave +0x3588 at 4, so the chain then falls to
 * 0x6add0 and the transmit dispatch.  `t46_chain_tail`'s `sub == 0` arm is
 * unreachable from either of them for that reason and is written because the
 * object writes it, not because anything drives it.
 */
static void
t46_chain_full(struct v34_object *obj)
{
	short sub = hs_get(obj, T46_F3588);

	/*
	 * 0x6c3f3 reloads the object and gates on the same 199 the head
	 * uses at 0x65dad -- `cmpw $0xc7,0xaa78(%ebx)` at 0x6c3fa -- on
	 * a path along which the head never read the counter at all.
	 */
	if (sub == 2) {
		t46_info0_counting(obj);		/* 0x6c3f3 */
		return;
	}
	t46_chain_tail(obj, sub);
}

static void
t46_chain_tail(struct v34_object *obj, short sub)
{
	/*
	 * 0x65dcd is the second limit on one counter: 0xc7 at the head
	 * (0x65dad) and 0x4b0 here (0x65ddb), both `jle`, and this one
	 * re-reads +0xaa78 at 0x65dd4 through a freshly loaded object.
	 */
	if (sub == 0) {
		t46_past_the_counter(obj);		/* 0x65dcd */
		return;
	}
	/*
	 * 0x6add0.  With +0x3588 at 4 or 2 on every entry (finding F418),
	 * this is where the two bodies that tail-call the chain really
	 * end up, and it is two instructions and `jmp 62af1` -- `mov
	 * 0xc0(%esp),%ebp` then `movzwl 0x3596(%ebp),%ecx`.
	 */
	t3c_txblock(obj);				/* 0x6add0 */
}

/*
 * The head of the arm at 0x65d6d: two transmit states are special-cased
 * before the chain on +0x3588 is entered at all.
 *
 * TX_DPSK looks through +0xaa6c at the record's +0x20 and +0x22 and, with
 * both set, restarts the counter and clears +0x20.  It then rejoins at
 * 0x65d85 -- the TONE_AB compare -- which cannot match, because the transmit
 * state it re-reads is the one that got it here.
 */
static void
t46_micro_tx_phase1_ans(struct v34_object *obj)
{
	short tx = hs_get(obj, HS_TXSTATE);

	/*
	 * 0x6b5b0, reached by `cmp $0x18,%cx; je` at 0x65d7b.  The two
	 * fields are two compares with their own `je 65d8f` (0x6b5bb and
	 * 0x6b5c6), so either one clear leaves the counter alone.
	 */
	if (tx == V34HS_TX_DPSK) {			/* 0x6b5b0 */
		short *r = *(short **)((char *)obj + T3C_PTR_AA6C);

		if (r[0x20 / 2] != 0 && r[0x22 / 2] != 0) {
			hs_put(obj, T3C_COUNT, 0);
			r[0x20 / 2] = 0;
		}
	/*
	 * 0x6b4ba, reached by `cmp $0x3c,%cx` at 0x65d85 on the transmit
	 * state the head read once.  It re-reads +0xaa78 through its own
	 * object load, steps it, and only then compares: `inc %eax` then
	 * `cmp $0x18f,%ax`.
	 */
	} else if (tx == V34HS_TONE_AB) {		/* 0x6b4ba */
		short n = (short)(hs_get(obj, T3C_COUNT) + 1);

		if (n <= T46_TONE_LIMIT) {
			/*
			 * 0x6f8f9 stores the step and jumps
			 * back to 0x65d8f -- the head's
			 * +0x3588 test, not the transmit tail.
			 */
			hs_put(obj, T3C_COUNT, n);	/* 0x6f8f9 */
		} else if (obj->fsk.sr != 0) {
			/*
			 * 0x6fb6e is its twin for the
			 * past-399 path with +0xaae2 set:
			 * same store, same `jmp 65d8f`.
			 */
			hs_put(obj, T3C_COUNT, n);	/* 0x6fb6e */
		} else if (t3c_getb(obj, T46_RETRAIN) != 0) {
			hs_put(obj, T3C_COUNT, n);
			/*
			 * 0x6f90c stores the step through
			 * the %edi 0x6b4ba loaded, then
			 * aims +0xaa6c at obj+0xa94c and
			 * +0xaa70 at obj+0xa97c.
			 */
			t46_body_retrain(obj);		/* 0x6f90c */
			return;
		/*
		 * 0x6b4ee is the one path of the four that does NOT keep
		 * the step: `xor %eax,%eax` and `mov %ax,0xaa78(%esi)`
		 * restart the counter, and +0x358c is `xor $0x1` at
		 * 0x6b50e, stored back sixteen bits wide (finding F413).
		 */
		} else {				/* 0x6b4ee */
			hs_put(obj, T3C_COUNT, 0);
			hs_put(obj, T46_F358C,
			       (short)(hs_get(obj, T46_F358C) ^ 1));
			hs_setstate(obj, HS_MICROSTATE, V34HS_RX_PHASE1_ANS);
			hs_put(obj, T46_F358A, 2);
			t3c_txblock(obj);
			return;
		}
	}

	/* 0x65d8f, where both head guards rejoin, 0x6f8f9 included. */
	/* 0x6adbd re-reads +0x3588 at 0x6adb6; 0x65d96 is this test's. */
	if (hs_get(obj, T46_F3588) != 0) {
		t46_chain_full(obj);			/* 0x6adbd */
		return;
	}

	/* 0x65da6, on the %esi 0x65d8f loaded, and a ten-bit mask. */
	if (hs_get(obj, T3C_COUNT) > T46_CNT_LOW
	    && (obj->fsk.sr & 0x3ff) == 0x372) {
		/*
		 * 0x6abc1 opens by clearing +0xabca and storing 4 into
		 * +0x3588 at 0x6abd8 -- a store, like the other two, and
		 * not 0x6f90c's OR (finding F410).
		 */
		t46_body_repeated(obj);			/* 0x6abc1 */
		return;
	}

	/*
	 * 0x65dcd, and the head is the only thing that reaches it: the
	 * chain's own `test %ax,%ax` at 0x6adc7 never sees zero
	 * (finding F418).
	 */
	t46_past_the_counter(obj);			/* 0x65dcd */
}

/*
 * ---------------------------------------------------------------------------
 * Microstate 44 `DET_INFO`, 0x668c0 -- the bit clock of the phase-2 INFO0
 * exchange, and the largest arm in table 3 at 6,046 exclusive bytes.
 *
 * WHAT IT IS.  `fskdemodulate` has already run on this route and left its
 * recovered bits in `obj->fsk` -- `nbits` counts them and `sr` is the shift
 * register (docs are in v34fsk.h).  This arm consumes exactly ONE of them per
 * call: it decrements `nbits`, feeds bit 0 of `sr` through a CRC-16 register
 * living in the record at +0xaa70, steps the bit counter at +0xaa78, and
 * every eighth bit copies the low byte of `sr` into that record's byte array.
 * When the counter reaches the message length plus sixteen -- the sixteen
 * being the CRC that follows the message -- it compares the register against
 * the last sixteen bits received and either accepts the message or restarts
 * the whole exchange.
 *
 * The polynomial pins itself: 0x1021 xored in when the register's top bit
 * differs from the incoming bit is CRC-16-CCITT, and the arm's own diagnostic
 * is "V34INFO, info0 CRC not received properly in DET_INFO".
 *
 * WHAT IS HERE.  All of it: the bit clock and its five exits, the RESTART a
 * failed CRC takes at 0x6bda0, and all four arms of the accept path, which
 * dispatches on the record's +0x18 -- the message length in bits, still in
 * `%dx` from 0x66944 and never reloaded.
 *
 *     0x4d (77 bits)  0x6f438  INFO1c        0x26 (38)  0x6ed17  INFO1a
 *     0x08 ( 8 bits)  0x6ea38  MOH           else       0x6e552  the default
 *
 * THE THREE SIZED ARMS ARE THREE DIFFERENT MESSAGES, not three shapes of one:
 * eight bits is a Modem-on-Hold byte, 0x26 is the answering modem's INFO1a
 * and 0x4d is the caller's INFO1c.  Only the default arm copies the received
 * bytes into +0xabae -- the sized arms reach the dispatch before that code --
 * so the unbounded copy at 0x6e5c8 is the default's alone and no message
 * length of 0x4d can drive it.
 */

/*
 * Offsets microstate 44 reads or writes.  `T44_` because three batches are
 * writing arms of this one function into this one translation unit at the
 * same time, and finding F325 is what one collided macro cost.  Fields the
 * struct already names -- `fsk`, `role`, `v90_receiver`, `local_short`,
 * `is_short` -- are used by name instead.
 */
#define T44_BLK_A94C	0xa94c	/* the record the restart installs         */
#define T44_REC_A97C	0xa97c	/* where the bring-up aims +0xaa70         */
#define T44_COUNT_SRC	0xaa7c	/* short: reloaded into the counter        */
#define T44_PTR_AA6C	0xaa6c	/* where it installs it                    */
#define T44_PTR_AA70	0xaa70	/* the record this arm's bit clock feeds   */
#define T44_COUNT	0xaa78	/* short: bits taken, and HS_TRACE_2       */
#define T44_FAA7A	0xaa7a	/* short: cleared on entry, every call     */
#define T44_F3588	0x3588	/* short: bit 2 raised by the restart      */
#define T44_F358A	0x358a	/* short: set to 1 by the restart          */
#define T44_FABAE	0xabae	/* ten shorts the restart clears           */
#define T44_FABC2	0xabc2	/* short: cleared with them, separately    */

/*
 * And what the three SIZED arms of the accept path reach.  A message of
 * eight bits is Modem-on-Hold, one of 0x26 is INFO1a and one of 0x4d is
 * INFO1c -- three different messages through one bit clock, so each arm ends
 * somewhere else.
 */
#define T44_F356A	0x356a	/* short: the 8-bit arm sets it to 1       */
#define T44_F358C	0x358c	/* short: the 0x4d arm clears it          */
#define T44_F35A2	0x35a2	/* short: the message-descriptor length    */
#define T44_PROBE	0xa320	/* the probe bins V34GiveProbeResults takes*/
#define T44_REC_A9AC	0xa9ac	/* the INFO1a record the 0x4d arm installs */
#define T44_REC_A9DC	0xa9dc	/* the INFO1a/1c record that arrived       */
#define T44_TXBAUD	0xaa84	/* short: what probeselect chose to send   */
#define T44_CARRIER	0xaaa8	/* short: the receive carrier, in Hz       */
#define T44_RXCARRDESC	0xaab0	/* the detector coefficients for it        */
#define T44_FAA80	0xaa80	/* short: the 0x26 arm sets it to 1        */
#define T44_FABF8	0xabf8	/* byte:  raised by the 8-bit arm's other  */

/*
 * The record reached through +0xaa70, and the one the restart installs at
 * +0xaa6c.  Both are inside the object, so the harness compares the two
 * pointers by offset from their own base rather than as addresses.
 */
#define T44_R_BYTES	0x00	/* ten shorts, one per byte received       */
#define T44_R_CRC	0x14	/* short: the CRC-16 register, poly 0x1021 */
#define T44_R_F16	0x16
#define T44_R_NBITS	0x18	/* short: message length in bits           */
#define T44_R_F1A	0x1a
#define T44_R_F1C	0x1c
#define T44_R_F1E	0x1e
#define T44_R_F20	0x20
#define T44_R_F22	0x22
#define T44_R_F24	0x24	/* int */
#define T44_R_F28	0x28
#define T44_R_F2A	0x2a
#define T44_R_F2C	0x2c	/* int */

static short *
t44_record(const struct v34_object *obj, unsigned off)
{
	return *(short *const *)((const char *)obj + off);
}

static short
t44_recget(const short *rec, unsigned off)
{
	return *(const short *)((const char *)rec + off);
}

static void
t44_recput(short *rec, unsigned off, short v)
{
	*(short *)((char *)rec + off) = v;
}

static void
t44_recputi(short *rec, unsigned off, int v)
{
	*(int *)((char *)rec + off) = v;
}

/*
 * The restart at 0x6bda0: the message is complete and its CRC did not check
 * out, so the exchange goes back to hunting for the synchronisation pattern.
 *
 * Entered with the counter already stepped.  It rejoins the bit clock's tail
 * at 0x66956 rather than returning, so the eighth-bit store below still runs
 * -- on the 0xffff this leaves in `sr`, which is what makes the two halves
 * one function and not two.
 *
 * The record at +0xaa70 and the one installed at +0xaa6c are DIFFERENT
 * records: 0x6bef0 reloads +0xaa70 into %eax and writes its +0x14, while
 * every write from 0x6c0ca on goes through %ebx, which 0x6bf2a set to
 * obj+0xa94c and which survives the intervening calls because it is callee
 * saved.
 */
static void
t44_det_info_restart(struct v34_object *obj)
{
	short *rec = t44_record(obj, T44_PTR_AA70);
	short *blk = (short *)((char *)obj + T44_BLK_A94C);
	short nbits;
	int i;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34INFO, info0 CRC not received "
				     "properly in DET_INFO\n");

	/*
	 * RX_DPSK is the state this arm is only ever reached in, so this
	 * transition is a no-op on every path a test can drive -- it is here
	 * because the object writes it, and `hs_setstate` is what makes it
	 * cost nothing when the state is already there.
	 */
	hs_setstate(obj, HS_RXSTATE, V34HS_RX_DPSK);
	hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);

	t44_recput(rec, T44_R_CRC, -1);
	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	if (obj->role == 0x66) {
		/*
		 * 0x718fc, the answering side.  The store is BEFORE the
		 * diagnostic check and unconditional; only the printf is
		 * guarded.
		 */
		t44_recput(rec, T44_R_NBITS, 0x1e);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("SetINFO0dBits  \n");
	}

	/* 0x6bf18, where the +0x359c == 0x66 arm rejoins. */
	hs_put(obj, T44_F358A, 1);
	t3c_putp(obj, T44_PTR_AA6C, blk);
	hs_put(obj, T44_F3588, (short)(hs_get(obj, T44_F3588) | 4));
	obj->is_short = 0;
	obj->local_short = 0;

	/*
	 * TEN SHORTS FROM +0xabae, INDEXED FROM ZERO.  0x6bf1f loads 1 into
	 * %eax for the +0x358a store above and 0x6bf3e zeroes it again before
	 * the loop, so the loop runs 0..9 and not 1..9; the bound at 0x6bf71
	 * is `cmp $9,%ax; jle`, tested after the increment.
	 */
	for (i = 0; i <= 9; i++)
		hs_put(obj, T44_FABAE + 2u * (unsigned)i, 0);
	hs_put(obj, T44_FABC2, 0);

	/*
	 * TX_DPSK is table 2's arm at 0x644c9, which is written -- so this
	 * path can be driven at all.  Finding F354 is the same constraint on
	 * microstate 79, and for the same reason.
	 */
	hs_setstate(obj, HS_TXSTATE, V34HS_TX_DPSK);
	hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);

	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	/*
	 * The installed record's length: 0x11 bits, or 0x1e on the
	 * ORIGINATING side (`role == 0x65`) when a V.90 receiver is there.
	 * 0x71920 falls back into the 0x11 arm when it is not, so this is one
	 * condition and not two arms.  Everything from +0x1c on is common.
	 */
	/*
	 * 0x6c0ca is the head of the 0x11 arm and not the store: the
	 * four common stores run from here and `movw $0x11,0x18(%ebx)`
	 * is 0x6c0e2.  The 0x1e arm repeats all four at 0x71932.
	 *
	 * 0x71920 reads `0x248(%edx)` with %edx the obj+4 base -- +0x24c,
	 * `v90_receiver`, the same base finding F412 resolved for
	 * microstate 46 -- and a zero receiver takes `je 6c0ca` at
	 * 0x7192c straight back into the 0x11 arm.
	 */
	nbits = 0x11;					/* 0x6c0ca */
	if (obj->role == 0x65 && obj->v90_receiver != 0)
		nbits = 0x1e;				/* 0x71920 */

	t44_recput(blk, T44_R_CRC, -1);
	t44_recput(blk, T44_R_F1A, 0);
	t44_recput(blk, T44_R_F1E, 0);
	t44_recput(blk, T44_R_F22, 0);
	t44_recput(blk, T44_R_NBITS, nbits);

	/*
	 * 0x6c0e8 is where the 0x1e arm rejoins -- `jmp 6c0e8` at
	 * 0x71950 -- so +0x1c and everything after it is written once
	 * for both lengths.
	 */
	t44_recput(blk, T44_R_F1C, 8);			/* 0x6c0e8 */
	t44_recput(blk, T44_R_F16, 1);
	t44_recputi(blk, T44_R_F24, 0xf72);
	t44_recputi(blk, T44_R_F2C, 0xf72);
	t44_recput(blk, T44_R_F28, 0xc);
	t44_recput(blk, T44_R_F2A, 0xc);
	t44_recput(blk, T44_R_F20, 1);
}

/*
 * The ten byte slots of a record, printed.
 *
 * Three sites emit this and the default arm at 0x6e552 is a fourth; they
 * differ only in the tag and in which record they read, and each pushes ten
 * ZERO-EXTENDED halfwords, which is what `%x` prints and what a `movswl`
 * would not.  The default arm's copy is left inline where it is: it belongs
 * to another batch's commit and moving it would put that batch's mutation
 * anchors on code no test of theirs drives.
 */
static void
t44_print10(const char *tag, const short *rec)
{
	dsplibs_debug_printf("%s 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,"
			     "0x%x,0x%x\n", tag,
			     (unsigned)(unsigned short)rec[0],
			     (unsigned)(unsigned short)rec[1],
			     (unsigned)(unsigned short)rec[2],
			     (unsigned)(unsigned short)rec[3],
			     (unsigned)(unsigned short)rec[4],
			     (unsigned)(unsigned short)rec[5],
			     (unsigned)(unsigned short)rec[6],
			     (unsigned)(unsigned short)rec[7],
			     (unsigned)(unsigned short)rec[8],
			     (unsigned)(unsigned short)rec[9]);
}

/*
 * The message-descriptor length carried in the first two halfwords of the
 * record at +0xa9dc, which the 0x26 arm and the 0x4d arm decode identically.
 *
 * Seven bits assembled from two halfwords -- the low two bits of the first
 * shifted up five, the top five bits of the second shifted down three -- and
 * then put through `bitreverse` over seven bits.  +0x35a2 is written TWICE,
 * once before the reversal and once after, and the first store is what a
 * reader who thought the reversal happened in place would drop.
 */
static short
t44_mdlength(struct v34_object *obj)
{
	short raw;
	int rev;

	raw = (short)(((hs_get(obj, T44_REC_A9DC) & 3) << 5)
		      | ((hs_get(obj, T44_REC_A9DC + 2) & 0xf8) >> 3));
	hs_put(obj, T44_F35A2, raw);

	rev = bitreverse((unsigned short)raw, 7);
	hs_put(obj, T44_F35A2, (short)rev);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("RX MDLENGTH = %d\n", (int)(short)rev);

	return (short)rev;
}

/*
 * A MESSAGE OF EIGHT BITS -- 0x6ea38, Modem-on-Hold.
 *
 * Eight bits is one byte of MOH message, and the arm hands the record
 * straight to `VPcmV34InterpretMohMessageBits` and then asks +0xabf0 what
 * that made of it.  Its two exits are not variations of each other: one goes
 * back to hunting for the synchronisation pattern and the other arms the tone
 * detector and moves to MOH_TONE_DROP.  Both rejoin the byte clock.
 *
 * `count2` is the STEPPED counter, still in `%ebx` from 0x6693a and not the
 * value the tail will re-read.  This arm never touches +0xaa78, so here the
 * two agree -- which is the exception finding F406 names rather than a
 * contradiction of it.
 */
static int
t44_accept_len08(struct v34_object *obj, short *rec, short count2)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Received MOH message ! (count2=%d, "
				     "data0=%X, data1=%X)\r\n", (int)count2,
				     (unsigned)(unsigned short)rec[0],
				     (unsigned)(unsigned short)rec[1]);

	/*
	 * `rec` AND NOT A RE-READ, and here the object agrees.  0x6ea67
	 * reloads +0xaa70 only on the path the diagnostic above took, and
	 * with the diagnostics off 0x6ea6d passes the `%ecx` 0x668e1 left --
	 * so the cached pointer is the object's own answer on the quiet path
	 * and the reload is the register allocator putting it back after a
	 * call clobbered it.  The 0x26 and 0x4d arms reload unconditionally
	 * and are written that way; this one does not.
	 */
	VPcmV34InterpretMohMessageBits(obj, rec);

	if (obj->moh_message != 1) {
		/*
		 * 0x6ea8d.  The record is RE-READ from +0xaa70 at 0x6ebc7:
		 * the decoder above is a call, so nothing may be assumed to
		 * have survived it, and the object says so by reloading.
		 */
		t3c_putb(obj, T44_FABF8, 1);
		hs_setstate(obj, HS_RXSTATE, V34HS_RX_DPSK);
		hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);
		t44_recput(t44_record(obj, T44_PTR_AA70), T44_R_CRC, -1);
		obj->fsk.sr = -1;
		/*
		 * 0x6ebda is `jmp 66956`: the arm rejoins the byte
		 * clock rather than returning (finding F401).
		 */
		return 0;				/* 0x6ebda */
	}

	/*
	 * 0x6ebdf.  The tone detector at +0x3564, aimed with 2400 Hz's
	 * coefficients when this end originated the call and 1200 Hz's when
	 * it answered -- the same pairing `v34handshakinit` uses, and the
	 * opposite way round from what "the tone I am listening for" would
	 * suggest, because it is the OTHER end's carrier.  Polarity 1, so the
	 * high threshold is the one that is read.
	 */
	detectorinit(T3C_DET(obj), obj->role == 0x65 ? c2400_ : c1200_,
		     1, 0x64, 0x32, 0x800, 0x400);
	/*
	 * 0x6ec4a stores the %ebp loaded at 0x6ec2f, before the
	 * `detectorinit` call it survives, and it lands between
	 * `hs_setstate`'s load of +0x3592 at 0x6ec43 and its compare
	 * against 0x50 at 0x6ec51.
	 */
	hs_put(obj, T44_F356A, 1);			/* 0x6ec4a */
	hs_setstate(obj, HS_MICROSTATE, V34HS_MOH_TONE_DROP);
	/*
	 * 0x6ecd5, with MOH_TONE_DROP stored at 0x6ecce: another
	 * `jmp 66956` into the byte clock, not a return.
	 */
	return 0;					/* 0x6ecd5 */
}

/*
 * The receiver's four rate constants, chosen by the baud rate `probeselect`
 * settled on: 0x6efc5's compare chain, which is GCC's binary search over six
 * values and not a range test.
 *
 * A baud rate that is none of the six leaves all four fields alone, which is
 * a real exit and not an oversight -- the carrier table below is reached
 * either way.
 */
static void
t44_setup_rate(struct v34_receiver *rx, short baud)
{
	switch (baud) {
	case 0x960:					/* 2400, 0x6f31b */
		rx->phase_wrap = 0x3e80;
		rx->phase_inc = 0x3e80;
		rx->symbol_period = 0x3e80;
		rx->phase_frac = 0x1f40;
		break;
	case 0xab7:					/* 2743, 0x6f2e2 */
		rx->phase_wrap = 0x3e80;
		rx->phase_inc = 0x36b0;
		rx->symbol_period = 0x36b0;
		rx->phase_frac = 0x1f40;
		break;
	case 0xaf0:					/* 2800, 0x6f3a7 */
		rx->phase_wrap = 0x3e82;
		rx->phase_inc = 0x3594;
		rx->symbol_period = 0x3594;
		rx->phase_frac = 0x1f41;
		break;
	case 0xbb8:					/* 3000, 0x6f36e */
		rx->phase_wrap = 0x3e80;
		rx->phase_inc = 0x3200;
		rx->symbol_period = 0x3200;
		rx->phase_frac = 0x1f40;
		break;
	case 0xc80:					/* 3200, 0x6f412 */
		rx->phase_wrap = 0x3e80;
		rx->phase_inc = 0x2ee0;
		rx->symbol_period = 0x2ee0;
		rx->phase_frac = 0x1f40;
		break;
	case 0xd65:					/* 3429, 0x6f3ec */
		rx->phase_wrap = 0x3e80;
		rx->phase_inc = 0x2bc0;
		rx->symbol_period = 0x2bc0;
		rx->phase_frac = 0x1f40;
		break;
	default:
		break;
	}
}

/*
 * And the half-sine window for the receive carrier: 0x6eff6's chain over
 * eight carriers.
 *
 * EACH COUNT IS HALF ITS TABLE'S LENGTH -- 6 of `hsine1600[12]`, 40 of
 * `hsine1680[80]`, 16 of `hsine1800[32]`, 21 of `hsine1829[42]`, 36 of
 * `hsine1867[72]`, 5 of `hsine1920[10]`, 49 of `hsine1959[98]` and 24 of
 * `hsine2000[48]`.  That is eight independent agreements between a constant
 * in this arm and an array length `include/dsplib/v34filt.h` already had.
 *
 * A carrier that is none of the eight leaves the pointer and the count alone.
 */
static void
t44_setup_carrier(struct v34_receiver *rx, short carrier)
{
	switch (carrier) {
	case 0x640:					/* 1600, 0x6f022 */
		rx->carrier = (short *)(unsigned long)hsine1600;
		rx->half_len = 6;
		break;
	case 0x690:					/* 1680, 0x6f202 */
		rx->carrier = (short *)(unsigned long)hsine1680;
		rx->half_len = 0x28;
		break;
	case 0x708:					/* 1800, 0x6f1e2 */
		rx->carrier = (short *)(unsigned long)hsine1800;
		rx->half_len = 0x10;
		break;
	case 0x725:					/* 1829, 0x6f25c */
		rx->carrier = (short *)(unsigned long)hsine1829;
		rx->half_len = 0x15;
		break;
	case 0x74b:					/* 1867, 0x6f23c */
		rx->carrier = (short *)(unsigned long)hsine1867;
		rx->half_len = 0x24;
		break;
	case 0x780:					/* 1920, 0x6f2b0 */
		rx->carrier = (short *)(unsigned long)hsine1920;
		rx->half_len = 5;
		break;
	case 0x7a7:					/* 1959, 0x6f2c9 */
		rx->carrier = (short *)(unsigned long)hsine1959;
		rx->half_len = 0x31;
		break;
	case 0x7d0:					/* 2000, 0x6f290 */
		rx->carrier = (short *)(unsigned long)hsine2000;
		rx->half_len = 0x18;
		break;
	default:
		break;
	}
}

/*
 * A MESSAGE OF 0x26 BITS -- 0x6ed17, INFO1a.
 *
 * The answering modem's INFO1a has arrived.  `V34GiveINFO1aBits` decodes it
 * and its answer decides everything: NON-ZERO and the exchange is over for
 * this phase -- WAIT and INFODONE and nothing else -- ZERO and the whole
 * receiver is configured from what the message said and from the probe
 * results that came with it.
 *
 * THE TEST IS SIXTEEN BITS WIDE.  0x6ed50 is `test %ax,%ax`, so a return
 * value whose low halfword is zero takes the long branch whatever the high
 * half holds.
 */
static int
t44_accept_len26(struct v34_object *obj, short *rec)
{
	struct v34_receiver *rx = T3C_RX(obj);
	short baud, carrier;

	V34GiveProbeResults(obj, (const char *)obj + T44_PROBE);

	/*
	 * 0x6ed3e RE-READS +0xaa70 after the call above, exactly as 0x6f602
	 * does in the 0x4d arm; `rec` is the cached pointer from 0x668e1 and
	 * the two agree on every path that exists, because nothing in
	 * `V34GiveProbeResults` writes +0xaa70.  Written as the object writes
	 * it, and the mutation that caches instead is recorded as an
	 * equivalence with that condition named.
	 */
	if ((short)V34GiveINFO1aBits(obj, t44_record(obj, T44_PTR_AA70)) != 0) {
		/* 0x6ed59, and +0x3594 against 0x23 is the WAIT guard. */
		hs_setstate(obj, HS_RXSTATE, V34HS_WAIT);
		hs_setstate(obj, HS_MICROSTATE, V34HS_INFODONE);
		/*
		 * 0x6ee72, with INFODONE stored at 0x6ee6b:
		 * `jmp 66956` back into the byte clock.
		 */
		return 0;				/* 0x6ee72 */
	}

	/* 0x6ee77, with bitreverse's 7 already staged in %eax. */
	(void)t44_mdlength(obj);
	setfinalrate(obj);
	hs_setstate(obj, HS_RXSTATE, V34HS_RECEIVE);
	rxinit(obj);

	baud = obj->baud_rate;
	carrier = hs_get(obj, T44_CARRIER);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34SetupDemodulator: baudrate %ld, "
				     "carrier %ld\n", (long)baud, (long)carrier);

	rx->out_count = 4;
	t44_setup_rate(rx, baud);
	t44_setup_carrier(rx, carrier);

	/* 0x6f03d.  The gain is read BEFORE the step is stored beside it. */
	rx->agc_gain = rx->agc_start_gain;
	rx->agc_step = 0x2000;
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34AGC, setup receiver gain = 0x%x\n",
				     (unsigned)(int)rx->agc_gain);

	/*
	 * TWO SET-UPS OVER ONE DETECTOR, and the second wins: same
	 * coefficients, same polarity, same thresholds, and a warm-up of 0x78
	 * where the first asked for 10.  Both are here because both are in
	 * the object and nothing between them reads +0x3564.
	 *
	 * The flag word is cleared of its top nibble first and then has
	 * DET_PENDING raised twice, once after each call.  The second raise
	 * is on a value that already has the bit, so it changes nothing --
	 * recorded as an equivalence rather than dropped as a duplicate.
	 */
	rx->flags = (unsigned short)(rx->flags
				     & (unsigned short)~(V34_RX_FLAG_TRAINED
							 | V34_RX_FLAG_DET_PENDING
							 | V34_RX_FLAG_DATA
							 | V34_RX_FLAG_FIR));
	detectorinit(T3C_DET(obj), t44_record(obj, T44_RXCARRDESC),
		     0, 8, 10, 0x600, 0);
	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);
	detectorinit(T3C_DET(obj), t44_record(obj, T44_RXCARRDESC),
		     0, 8, 0x78, 0x600, 0);
	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);

	/*
	 * 0x6f146 follows the second `or $0x200` into the receiver's
	 * +0x122 at 0x6f13f, so the counter is cleared after both
	 * detector set-ups rather than between them.
	 */
	hs_put(obj, T44_COUNT, 0);			/* 0x6f146 */
	/*
	 * 0x6f15c is scheduled INSIDE the trace's guard: 0x6f155 compares
	 * dsplibs_debug_level and 0x6f163 is the `jbe` that leaves for the
	 * byte clock, and the store sits between them.  The 1 has been in
	 * %esi since 0x6f10b, held across the second `detectorinit`.
	 */
	hs_put(obj, T44_FAA80, 1);			/* 0x6f15c */

	if (DSPLIB_DEBUG_ON())
		t44_print10("V34PROBE, rxinfo1a",
			    (const short *)((const char *)obj + T44_REC_A9DC));
	/*
	 * 0x6f1d1 is `jmp 66956` and it belongs to BOTH sized arms: the
	 * 0x4d arm's trace jumps into this one's argument setup at 0x6f7b6,
	 * so the format string at 0x6f1c5, the call and this exit are one
	 * copy.  Only the label differs -- .rodata.str1.1+0x2c1f here and
	 * +0x2c45 there.
	 */
	return 0;					/* 0x6f1d1 */
}

/*
 * A MESSAGE OF 0x4d BITS -- 0x6f438, INFO1c.
 *
 * The caller's INFO1c has arrived, carrying the probe results this end asked
 * for; the answer to it is an INFO1a, which this arm builds in the record at
 * +0xa9ac and hands to the bit clock by installing it at +0xaa6c.
 *
 * THE COUNTER AND ITS COMPANION ARE ZEROED BEFORE ANYTHING ELSE, at 0x6f443
 * and 0x6f44a -- so the byte clock this returns into runs on a count of zero
 * and stores nothing, which is finding F406's shape again and the reason the
 * two stores are the first thing in the arm rather than the last.
 */
static int
t44_accept_len4d(struct v34_object *obj)
{
	short *blk;

	/*
	 * 0x6f443 and 0x6f44a, from the %ebx and %ecx zeroed at 0x6f43f and
	 * 0x6f441.  The arm's FIRST instruction is neither store: 0x6f438
	 * reads +0x3596 for the `hs_setstate` below, whose `cmp $0x18` --
	 * TX_DPSK is 24 -- is at 0x6f451.
	 */
	obj->vect_idx = 0;			/* 0x6f443 */
	hs_put(obj, T44_COUNT, 0);			/* 0x6f44a */
	hs_setstate(obj, HS_TXSTATE, V34HS_TX_DPSK);

	(void)t44_mdlength(obj);
	probeselect(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PROBESELECT, in ANSWER, txbaudrate = "
				     "%d,rxbaudrate = %d,\n",
				     (int)hs_get(obj, T44_TXBAUD),
				     (int)obj->baud_rate);
	if (DSPLIB_DEBUG_ON())
		t44_print10("V34PROBE, rxinfo1c",
			    (const short *)((const char *)obj + T44_REC_A9DC));

	/*
	 * 0x6f60e installs the record BEFORE the decoder runs, and 0x6f651
	 * reads it back afterwards rather than reusing the pointer it just
	 * wrote -- the decoder is a call, so the object reloads.
	 */
	t3c_putp(obj, T44_PTR_AA6C, (char *)obj + T44_REC_A9AC);
	if (V34GiveINFO1dBits(obj, t44_record(obj, T44_PTR_AA70)) != 0)
		return 0;				/* 0x6f622 */

	V34GiveProbeResults(obj, (const char *)obj + T44_PROBE);
	V34SetINFO1aBits(obj, t44_record(obj, T44_PTR_AA6C));

	/*
	 * The INFO1a this end will send: 0x26 bits, which is the length the
	 * 0x26 arm above answers to -- the two arms are the two ends of one
	 * exchange, and that is what makes the constant checkable twice.
	 *
	 * 0xff72 and not the restart's 0xf72 (finding F401): the same twelve
	 * bits of preamble with a thirteenth set above them, written as a
	 * full int at +0x24 and at +0x2c.
	 */
	blk = t44_record(obj, T44_PTR_AA6C);
	t44_recput(blk, T44_R_CRC, -1);
	t44_recput(blk, T44_R_F1A, 0);
	t44_recput(blk, T44_R_F1E, 0);
	t44_recput(blk, T44_R_F22, 0);
	t44_recput(blk, T44_R_NBITS, 0x26);
	t44_recput(blk, T44_R_F1C, 8);
	t44_recput(blk, T44_R_F16, 1);
	t44_recput(blk, T44_R_F28, 0x10);
	t44_recput(blk, T44_R_F2A, 0x10);
	t44_recput(blk, T44_R_F20, 0);
	t44_recputi(blk, T44_R_F24, 0xff72);
	t44_recputi(blk, T44_R_F2C, 0xff72);

	/*
	 * 0x6f6ba, and the microstate it is about to move has already been
	 * read: 0x6f6a5 loads +0x3592 between the +0x20 and +0x24 stores
	 * above, and 0x6f6c1 is the `cmp $0x3f` against it.
	 */
	hs_put(obj, T44_F358C, 0);			/* 0x6f6ba */
	hs_setstate(obj, HS_MICROSTATE, V34HS_INFODONE);

	if (DSPLIB_DEBUG_ON())
		t44_print10("V34PROBE, txinfo1a",
			    (const short *)((const char *)obj + T44_REC_A9AC));
	/*
	 * 0x6f1d1 ONLY WHEN THE TRACE IS ON.  With it off the arm leaves at
	 * 0x6f74d, a `jbe 66956` of its own; with it on, 0x6f7b6 jumps into
	 * the 0x26 arm's print at 0x6f1c5 and exits through its jump.
	 */
	return 0;					/* 0x6f1d1 */
}

/*
 * The message is complete and its CRC checks out: 0x6e534.
 *
 * The dispatch is on the message length still in `%dx` from 0x66944 -- three
 * lengths have bodies of their own, above, and the rest of this is the fourth
 * arm, the DEFAULT at 0x6e552.  Returns non-zero when it has already left
 * through the transmit dispatch, and zero when the caller is to rejoin the
 * bit clock at 0x66956, which two of the default's three exits do and which
 * every exit of all three sized arms does.
 *
 * `rec` is the record at +0xaa70, still in `%ecx` from 0x668e1 and not
 * reloaded anywhere on this path.  `count2` is the STEPPED counter, still in
 * `%ebx` from 0x6693a -- the 8-bit arm's diagnostic prints it and nothing
 * else in the accept path reads it.
 */
static int
t44_det_info_accept(struct v34_object *obj, short *rec, short count2)
{
	struct v34_receiver *rx = T3C_RX(obj);
	short len = t44_recget(rec, T44_R_NBITS);
	short count;
	int nbytes;
	int i;

	/*
	 * THE ORDER IS THE OBJECT'S: 0x4d, then 0x26, then 0x08, and the
	 * default is what falls out of all three.  The three constants are
	 * pinned from both sides -- a case at each length drives its body and
	 * a case one away drives the default.
	 */
	if (len == 0x4d)
		return t44_accept_len4d(obj);		/* 0x6f438 */
	if (len == 0x26)
		return t44_accept_len26(obj, rec);	/* 0x6ed17 */
	if (len == 0x08)
		return t44_accept_len08(obj, rec, count2);	/* 0x6ea38 */

	if (hs_get(obj, T44_F358A) == 1) {
		short *blk = t44_record(obj, T44_PTR_AA6C);

		/* 0x6e571, and it is the OTHER record's +0x04. */
		if ((t44_recget(blk, 0x04) & 0x80) == 0) {
			t44_recput(blk, 0x22, 0);
			t44_recput(blk, 0x04,
				   (short)(t44_recget(blk, 0x04) | 0x80));
		}

		/*
		 * How many BYTES arrived: the bit count divided by eight
		 * truncating toward zero, plus one if any bits are left over.
		 * 0x6e59b's `lea 0x7(%edx)` before the `and` is how GCC spells
		 * a truncating divide of a value that may be negative, and
		 * 0x6e5b8 tests the low three bits of the SAME count.
		 */
		count = hs_get(obj, T44_COUNT);
		nbytes = count / 8 + ((count & 7) != 0 ? 1 : 0);
		hs_put(obj, T44_FABC2, (short)nbytes);

		/* Into the array the restart clears -- finding F401. */
		for (i = 0; i < nbytes; i++)
			hs_put(obj, T44_FABAE + 2u * (unsigned)i,
			       t44_recget(rec, 2u * (unsigned)i));

		if ((t44_recget(rec, 0x04) & 0x80) == 0) {
			/*
			 * 0x6e5fb.  This exit does NOT rejoin the bit clock:
			 * 0x6e740 jumps straight to the transmit dispatch.
			 */
			hs_setstate(obj, HS_RXSTATE, V34HS_RX_DPSK);
			hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);
			t44_recput(t44_record(obj, T44_PTR_AA70),
				   T44_R_CRC, -1);
			/*
			 * 0x6e739 puts -1 in +0xaae2 and 0x6e740 is one
			 * `jmp 62af1`.  The `return 1` has no instructions
			 * of its own -- caller and callee leave through the
			 * same jump, which is why the caller's `return`
			 * carries this address too.
			 */
			obj->fsk.sr = -1;
			t3c_txblock(obj);		/* 0x6e740 */
			return 1;
		}
	}

	/* 0x6e745: F358A != 1 (0x6e561) and +0x04 bit 7 set (0x6e5f5). */
	hs_put(obj, T44_COUNT, 0);

	/*
	 * The buffer is obj+0xa97c LITERALLY and not the +0xaa70 record --
	 * 0x6e757 is `lea 0xa97c(%ebx)`.  `v34handshakinit` mode 0 hands
	 * `V34SetINFO0dBits` the same fixed address.
	 */
	V34GiveINFO0dBits(obj, (const short *)((char *)obj + T44_REC_A97C));

	if (DSPLIB_DEBUG_ON()) {
		const short *r = t44_record(obj, T44_PTR_AA70);

		dsplibs_debug_printf("%s 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,"
				     "0x%x,0x%x,0x%x\n", "V34INFO, rxinfo0",
				     (unsigned)(unsigned short)r[0],
				     (unsigned)(unsigned short)r[1],
				     (unsigned)(unsigned short)r[2],
				     (unsigned)(unsigned short)r[3],
				     (unsigned)(unsigned short)r[4],
				     (unsigned)(unsigned short)r[5],
				     (unsigned)(unsigned short)r[6],
				     (unsigned)(unsigned short)r[7],
				     (unsigned)(unsigned short)r[8],
				     (unsigned)(unsigned short)r[9]);
	}

	/*
	 * Where the handshake goes next, and `is_short` is `V34GiveINFO0dBits`'s
	 * own output rather than an input (v34info.c).
	 *
	 * The originating side skips BOTH the `sr` reset at 0x6e814 and the
	 * counter reload at 0x6e8ac; the answering side does the reset either
	 * way and the reload only on the short-phase-2 branch.
	 *
	 * The reset is one instruction sitting BETWEEN the compare and its
	 * branch -- 0x6e80c is `cmpw $0x0,0xabcc(%eax)`, 0x6e814 is the store
	 * to +0xaae2, and 0x6e81b is the `je`.  So "either way" is not a
	 * reading of the control flow, it is what the instruction order
	 * forces: the store has already happened whichever way the `je` goes.
	 */
	if (obj->role == 0x65) {
		hs_setstate(obj, HS_MICROSTATE, V34HS_RX_PHASE1_CALL);
	} else {
		obj->fsk.sr = -1;			/* 0x6e814 */
		if (obj->is_short != 0) {
			hs_setstate(obj, HS_MICROSTATE, V34HS_RX_PHASE1_ANS);
			/*
			 * 0x6e8ac is also where the inlined `hs_setstate`
			 * lands when the microstate ALREADY holds 0x31 --
			 * `je 6e8ac` at 0x6e82c -- so the reload of +0xaa78
			 * from +0xaa7c runs whether the state moved or not.
			 */
			hs_put(obj, T44_COUNT,		/* 0x6e8ac */
			       hs_get(obj, T44_COUNT_SRC));
		} else {
			hs_setstate(obj, HS_MICROSTATE, V34HS_TX_PHASE1_ANS);
		}
	}

	/* 0x6e8c1: the detector this INFO0 was waiting on is disarmed. */
	rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_DET_PENDING);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34INFO,%s info0 received in DET_INFO\n",
				     obj->is_short != 0 ? " Short Phase 2" : "");

	return 0;					/* 0x6e90c */
}

static void
t44_micro_det_info(struct v34_object *obj)
{
	short *rec;
	short nbits, count, next;
	int idx;

	hs_put(obj, T44_FAA7A, 0);

	/* No bit waiting: the arm is over before it starts. */
	nbits = obj->fsk.nbits;
	if (nbits == 0) {
		t3c_txblock(obj);			/* 0x6ab88 */
		return;
	}

	/* 0x668e1 reads the record BEFORE 0x668e7 writes the count back. */
	rec = t44_record(obj, T44_PTR_AA70);
	obj->fsk.nbits = (short)(nbits - 1);

	count = hs_get(obj, T44_COUNT);

	/*
	 * The message's own bits go through the register; the sixteen after
	 * it are the CRC and do not.  One bit per call, MSB first:
	 *
	 *   668ff  movswl %ax,%edx / shr $0x1f,%edx    the register's bit 15
	 *   66907  testb  $0x1,0xaae2(%edi)            the incoming bit
	 *   66910  xor    $0x1,%edx                    so %edx is their XOR
	 *   66922  xor    $0x1021,%eax                 taken only when it is 1
	 */
	if (count < t44_recget(rec, T44_R_NBITS)) {
		unsigned crc = (unsigned short)t44_recget(rec, T44_R_CRC);
		int fb = (int)(crc >> 15);

		if ((obj->fsk.sr & 1) != 0)
			fb ^= 1;
		crc <<= 1;
		/*
		 * The `else` is a whole out-of-line block, not a fallthrough:
		 * `mov %ax,0x14(%ecx)` exists twice, at 0x66927 after the xor
		 * and at 0x6c133 without it; `je 6c133` at 0x66915 picks it.
		 * The shift is unconditional -- 0x66905 `add %eax,%eax` runs
		 * before the test.
		 */
		if (fb != 0)
			crc ^= 0x1021;			/* else 0x6c133 */
		t44_recput(rec, T44_R_CRC, (short)crc);
	}

	next = (short)(count + 1);
	hs_put(obj, T44_COUNT, next);

	/*
	 * Sixteen CRC bits after the message: 0x66944 re-reads the length.
	 * 0x6bda4 then compares the register against what arrived, sixteen
	 * bits wide, and equal is the message accepted.
	 */
	if ((int)next == (int)t44_recget(rec, T44_R_NBITS) + 0x10) {
		if ((unsigned short)obj->fsk.sr
		    == (unsigned short)t44_recget(rec, T44_R_CRC)) {
			if (t44_det_info_accept(obj, rec, next) != 0)
				return;			/* 0x6e740 */
		} else {
			/*
			 * The restart is the fallthrough of `je 6e534` at
			 * 0x6bdab, and 0x6bdb1 is its FIRST instruction: the
			 * debug-level test.  The compare feeding it is
			 * 0x6bda4, sixteen bits, +0xaae2 against +0x14.
			 */
			t44_det_info_restart(obj);	/* 0x6bdb1 */
		}
	}

	/*
	 * 0x6695d RE-READS THE COUNTER, and it is not the value stored at
	 * 0x6693d.  The restart leaves +0xaa78 alone, so on that path the two
	 * agree -- but the accept path resets it at 0x6e750 and may reload it
	 * from +0xaa7c at 0x6e8ac, and everything below runs on what is in
	 * memory.  A reset counter therefore makes the whole byte clock a
	 * no-op on the call that accepted a message, which is not what a
	 * cached local would do and is how the object was caught saying so.
	 */
	next = hs_get(obj, T44_COUNT);

	/* Only every eighth bit completes a byte. */
	if ((next & 7) != 0) {
		/*
		 * 0x66966 `jne 6bd8d`, and the test at 0x66964 is
		 * `test $0x7,%al` -- ONE BYTE of a counter the source
		 * carries as a short.
		 */
		t3c_txblock(obj);			/* 0x6bd8d */
		return;
	}
	if (next <= 0) {
		/*
		 * 0x6696c `test %ax,%ax` + `jle 7186a`: sixteen bits and
		 * SIGNED, so a negative counter leaves here too.
		 */
		t3c_txblock(obj);			/* 0x7186a */
		return;
	}

	/*
	 * Ten bytes and no more.  0x6697a re-signs the index through sixteen
	 * bits before comparing, which cannot change it here -- `next` is
	 * positive and a multiple of eight -- but the object does it, so the
	 * cast stays where the compare is.
	 */
	idx = ((int)next >> 3) - 1;
	if ((short)idx > 9) {
		t3c_txblock(obj);			/* 0x71857 */
		return;
	}

	/*
	 * 0x6698e re-reads +0xaa70: the restart above may have run in
	 * between, and it is the LOW BYTE of `sr` that is stored, widened to
	 * a short.
	 */
	rec = t44_record(obj, T44_PTR_AA70);
	t44_recput(rec, T44_R_BYTES + 2u * (unsigned)idx,
		   (short)(unsigned char)obj->fsk.sr);

	t3c_txblock(obj);
}

/*
 * ---------------------------------------------------------------------------
 * rxstate 53 `DET_AB`, 0x65473 -- the end of L2, and the retrain that is not
 * always one.
 *
 * 1,632 bytes over TEN ranges -- 0x65473, 0x6845e, 0x69815, 0x6af24,
 * 0x6b0e8, 0x6b138, 0x6d95f, 0x6fcb5, 0x6fd34 and 0x7170a -- and only 861 of
 * them are live.  The other 771, 47.2%, are the bodies of nine
 * `if (DSPLIB_DEBUG_ON())`, six of which are `hs_setstate`'s own transition
 * trace with `StateName[next]` constant-folded (`*0x6cf0` is [60], `*0x6cc0`
 * is [48], `*0x6cac` is [43], `*0x6c60` is [24] and `*0x6cfc` is [63]).
 * Only three of the nine are this arm's own literals.
 *
 * Every one of the exits is `jmp 0x62af1`, the once-per-block transmit
 * dispatch: the arm decides and leaves, and nothing falls into another arm.
 *
 * TWO GUARDS, TWO WIDTHS, AND THE SECOND IS THE ONE THAT MATTERS.  0x6845e
 * is `cmpw $0x5db` + `jg`, sixteen bits and signed.  0x6af24 is NOT
 * sixteen-bit: it sign-extends BOTH halfwords into 32-bit registers with
 * `movswl`, adds 0x2418 there, and compares `%ebx` against `%edx` -- because
 * `rtd + 9240` does not fit a short.  Spelling that comparison sixteen bits
 * wide is a defect no small-value test can see, so `t_v34hsrx53.c` drives it
 * at rtd 30000, where the two readings disagree about whether the modem
 * retrains at all.  Finding F724.
 *
 * THE DETECTOR IS `t41_detect`'s, argument for argument -- the receiver, the
 * detector at +0x3564, the samples at receiver + 0x10c and `rx_samples` as
 * the end pointer.  All seven `tone_detect` sites in the object test `%ax`
 * and not `%eax` though the function is declared `int`, which is why that
 * helper narrows to `short`; the header is not changed on that evidence
 * (finding F727).
 */
static void
t53_rx_det_ab(struct v34_object *obj)
{
	struct v34_receiver *rx = T41_RX(obj);

	/*
	 * 0x65473, and it runs before either guard -- so even the two exits
	 * that do nothing else have run the gain control.
	 *
	 * THE ADDRESS IS ON THE LINE because rxstate 72's arm calls the same
	 * function with the same argument, and `\tV34agc(rx);` is a PREFIX of
	 * that line -- so the mutation anchored here matched twice the moment
	 * 0x650ee landed.  Finding F432's repair: information, not indentation.
	 */
	V34agc(rx);					/* 0x65473 */

	/* 0x65486, `cmpw $0x34`: 52 is TX_L2, not TX_PHASE3_ANS. */
	if (hs_get(obj, HS_MICROSTATE) != V34HS_TX_L2) {
		/*
		 * The object tests the other way: 0x65486 `cmpw $0x34` and
		 * `je 6845e` jump TO the guard, so 0x65494 is the not-equal
		 * fallthrough.
		 */
		t3c_txblock(obj);			/* 0x65494 */
		return;
	}

	/* 0x6845e, sixteen bits and signed. */
	if (obj->vect_idx <= 0x5db) {
		t3c_txblock(obj);			/* 0x6846d */
		return;
	}

	if (t41_detect(obj) == 0) {
		/*
		 * 0x6af24, and the arithmetic is THIRTY-TWO bits wide.  See
		 * the head of this function.
		 */
		if ((int)obj->vect_idx <= (int)obj->rtd + 0x2418) {
			t3c_txblock(obj);		/* 0x6af47 */
			return;
		}

		/*
		 * 0x6b0f4.  This runs INSIDE the step, so a test driving it
		 * is the case finding F359 says `V34HS_REFINIT=1` cannot be
		 * used against: side A installs our library tables and side B
		 * the blob's, and no address comparison can settle two copies
		 * of one table.
		 */
		v34handshakinit(obj, 1);		/* 0x6b0f4 */

		/*
		 * The trace is exiled past the end of the arm: `ja 7170a` at
		 * 0x6b100, and it does not come back.  0x71716 is its own
		 * copy of the exit below, so that exit is in the object twice.
		 */
		if (DSPLIB_DEBUG_ON())			/* 0x7170a */
			dsplibs_debug_printf("V34RETRAIN, retrain is " "initiated in DET_AB\n");

		t3c_txblock(obj);
		return;
	}

	/*
	 * 0x69851.  Both arguments are sign-extended halfwords: the gain is
	 * the receiver's +0x136 and `count1` is the trace's `[1]`.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34RETRAIN, End of L2,rx->gain=0x%x," "count1=%d\n",
				     rx->agc_gain, obj->vect_idx);

	if (obj->role == 0x65) {
		/*
		 * 0x6b138.  The originating end builds INFO1c and hands the
		 * receive machine back to the FSK demodulator.
		 *
		 * IT IS INFO1c AND NOT INFO1a, and the length field is what
		 * says so: +0x18 of this record gets 0x4d, which is INFO1c's,
		 * and the 0x26 INFO1a length goes to a SECOND record at
		 * +0xa9dc below.  `V34SetINFO1aBits` assembles INFO1a or
		 * INFO1c or INFO1d (v34info.h), so the callee's name settles
		 * nothing; the object's own label for the trace at the bottom
		 * is "V34PROBE, txinfo1c".
		 */
		hs_setstate(obj, HS_TXSTATE, V34HS_TX_DPSK);

		probeselect(obj);			/* 0x6b1de */

		/*
		 * 0x6b1e3, and the store is BEFORE the call because the call
		 * takes the same interior pointer as its second argument.
		 *
		 * THE OBJECT WRITES THE TWELVE STORES BELOW THROUGH +0xaa6c
		 * AND THIS DOES NOT.  0x6b1f5 reloads the field into `%ebp`
		 * although `%ebx` still holds the identical value, which is
		 * what a source dereferencing the pointer rather than a local
		 * compiles to.  The two spellings cannot differ: the store
		 * two instructions earlier is what the reload reads, and
		 * `V34SetINFO1aBits` does not touch +0xaa6c.  So the
		 * differential tier cannot see the difference and these are
		 * microstate 41's own statements verbatim rather than a
		 * second reading of the same record.  Finding F726.
		 */
		t3c_putp(obj, T41_PTR_AA6C,		/* 0x6b1e3 */
			 (char *)obj + T41_BLK_A9AC);
		V34SetINFO1aBits(obj,			/* 0x6b1f0 */
				 (short *)((char *)obj + T41_BLK_A9AC));

		/*
		 * EACH STORE CARRIES ITS ADDRESS, and that is not decoration:
		 * these nine statements are character for character
		 * microstate 41's at 0x6d387, so without the addresses no
		 * mutation anchored on one of them could tell the two apart.
		 * Finding F432's hazard, answered with information rather than
		 * with indentation.
		 */
		/*
		 * 0x6b1fb IS FIVE STORES: +0x14 = 0xffff, +0x1a, +0x1e and
		 * +0x22 = 0, then +0x18 = 0x4d, all through the %ebp reloaded
		 * at 0x6b1f5.  The six that follow are in the object's order,
		 * +0x1c, +0x16, +0x28, +0x2a, +0x20 -- and 0x6f669..0x6f6b3
		 * writes the same twelve offsets in the same order with 0x26
		 * at +0x18 (finding F443).
		 */
		t41_record_head(obj, T41_BLK_A9AC, 0x4d);   /* 0x6b1fb */
		hs_put(obj, T41_BLK_A9AC + 0x1c, 8);	    /* 0x6b219 */
		hs_put(obj, T41_BLK_A9AC + 0x16, 1);	    /* 0x6b21f */
		hs_put(obj, T41_BLK_A9AC + 0x28, 0x10);	    /* 0x6b225 */
		hs_put(obj, T41_BLK_A9AC + 0x2a, 0x10);	    /* 0x6b22b */
		hs_put(obj, T41_BLK_A9AC + 0x20, 0);	    /* 0x6b231 */
		/*
		 * THIRTY-TWO BITS, both of them: `movl $0xff72` and not
		 * `movw`, so the immediate is +65394 and the halfword above
		 * each is zeroed too.
		 */
		t3c_puti(obj, T41_BLK_A9AC + 0x24, 0xff72); /* 0x6b23e */
		t3c_puti(obj, T41_BLK_A9AC + 0x2c, 0xff72); /* 0x6b245 */

		/*
		 * 0x6b24c and the 0x26 arm's 0x6f6ba are the same seven
		 * bytes, `mov %di,0x358c(%esi)`, and both clear +0x358c
		 * immediately before a state move.  The word that move
		 * compares was loaded at 0x6b237, in the middle of the record
		 * fill: `cmp $0x2b` -- RX_DPSK is 43 -- is at 0x6b253.
		 */
		hs_put(obj, T41_F358C, 0);		/* 0x6b24c */

		hs_setstate(obj, HS_RXSTATE, V34HS_RX_DPSK);
		hs_setstate(obj, HS_MICROSTATE, V34HS_INFODONE);

		/* 0x6b379: the SECOND record, and INFO1a's length. */
		t3c_putp(obj, T41_PTR_AA70, (char *)obj + 0xa9dc);
		hs_put(obj, 0xa9dc + 0x18, 0x26);

		/*
		 * 0x6b3a2, and it reads the record through `%ebx` -- obj +
		 * 0xa9ac -- rather than through the pointer it has just
		 * aimed, which is the other half of the note above.
		 */
		if (DSPLIB_DEBUG_ON()) {
			const unsigned short *p =
			    (const unsigned short *)((const char *)obj
						     + T41_BLK_A9AC);

			dsplibs_debug_printf(
			    "%s 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x," "0x%x,0x%x\n",
			    "V34PROBE, txinfo1c",
			    (unsigned)p[0], (unsigned)p[1], (unsigned)p[2],
			    (unsigned)p[3], (unsigned)p[4], (unsigned)p[5],
			    (unsigned)p[6], (unsigned)p[7], (unsigned)p[8],
			    (unsigned)p[9]);
		}
	} else {
		/*
		 * 0x69888.  The answering end moves three state words and
		 * builds nothing.
		 */
		hs_setstate(obj, HS_TXSTATE, V34HS_TONE_AB);
		hs_setstate(obj, HS_MICROSTATE, V34HS_TX_PHASE3_ANS);
		hs_setstate(obj, HS_RXSTATE, V34HS_RX_DPSK);
	}

	/*
	 * 0x69924, the tail both halves reach.  The receiver's +0x136 is
	 * restored from its +0x264 -- an unmodelled halfword this arm only
	 * reads -- and the FSK demodulator's four scalars are re-armed for
	 * the next message, `next` at 6 rather than 0.
	 */
	/* 0x69933 */
	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);
	rx->agc_gain = (short)*(const unsigned short *)((const char *)rx
							+ T3M_RX_F264);

	/*
	 * +0xaae2 `sr` (finding F553), +0xaae0 `nbits`, +0xaadc `phase`,
	 * +0xaade `next`: the object stores them in THAT order, which is
	 * not the order they lie in, and all four values were already in
	 * registers -- 0x69928, 0x6992a, 0x6992c and `mov $0x6,%ebx` at
	 * 0x6992e -- before the flag word above was even read.
	 */
	obj->fsk.sr = 0;			/* 0x6995f */
	obj->fsk.nbits = 0;			/* 0x69966 */
	obj->fsk.phase = 0;			/* 0x6996d */
	obj->fsk.next = 6;			/* 0x69974 */
	/*
	 * 0x6997b and 0x69982 store registers the block above cleared -- `xor
	 * %eax,%eax` at 0x6994d, `xor %edi,%edi` at 0x6995d -- and both fields
	 * are ones the state-transition trace reports: 0x6ff94 pushes +0x2aa2
	 * and +0xaa78, finding F633's busiest counter, as two of its arguments.
	 * The whole run of six goes through the object pointer reloaded at
	 * 0x69956.
	 */
	obj->vect_idx = 0;			/* 0x6997b */
	hs_put(obj, T41_COUNT, 0);		/* 0x69982 */

	/*
	 * 0x69989 is an exit and not a call: `movzwl 0x3596(%ecx),%ecx` and
	 * `jmp 62af1`, where the txstate is sign-extended, has 5 subtracted
	 * and is bounds-checked against 0x45 before the dispatch.
	 */
	t3c_txblock(obj);				/* 0x69989 */
}

/*
 * ---------------------------------------------------------------------------
 * rxstate 4 `RECEIVE`, 0x653e4 -- TRN2, the MP/E sequence, and the end of
 * phase 3.
 *
 * 4,881 bytes over TWENTY-EIGHT ranges, and three quarters of that is either
 * a function this tree already has or a diagnostic.  `setupreceiver` is
 * INLINED here -- 0x6a0c8..0x6a225 with its baud and carrier arms scattered
 * to 0x6e477, 0x7142b..0x71657 and its two debug bodies at 0x71648 and
 * 0x716f4 -- and so is `v34setuptxmit`, 0x68e50..0x68f73, the same six steps
 * microstate 63's arm inlines at 0x6597d (V34hshak.c's own comment there).
 * Both come out as calls, which is finding F426's rule for the third and
 * fourth time.  What is left is ten diagnostics, the MP bit packer, and the
 * four transfers below.
 *
 * FIFTEEN EXITS AND ALL FIFTEEN ARE `jmp 0x62af1` -- 0x6546e, 0x67b90,
 * 0x686ff, 0x690fb, 0x69b4a, 0x6a298, 0x6a4dc, 0x6aae1, 0x6d155, 0x6d28e,
 * 0x6d2a1, 0x70c70, 0x70cbc, 0x70cdb and 0x70d06.  There is no `ret` in the
 * arm and no fall-through into another one, so every path here ends in
 * `t3c_txblock` and TEN of the fifteen are the same nineteen-byte "read
 * txstate, jump" idiom writing nothing at all.
 *
 * `0x67a1e` IS NOT A LOOP HEADER, which is what lets this be straight-line C
 * rather than a state machine.  Everything that branches back to it --
 * 0x68bd8's packer, 0x6a377, 0x6b47a, 0x6cafd, 0x6cb70, 0x710e1, 0x71264 --
 * is reached only from the head at 0x67999, above it, so the merge is taken
 * at most once per call.
 */

/*
 * The offsets this arm needs that `struct v34_object` and `struct
 * v34_receiver` do not yet name.  Finding F552's rule: an offset is the
 * honest spelling for a span `tools/whichfield.py` reports as `unmapped_*`.
 */
#define T4_RXF11C	0x011c	/* receiver: the shift register TRN2 fills */
#define T4_RXF11E	0x011e	/* receiver: the one behind it             */
#define T4_RXF120	0x0120	/* receiver: cleared when either matches   */
#define T4_PLLCNT	0x25da	/* short: obj+0x221c+0x3be, the arm's own
				   counter -- 0x4c(%esp) plus 0x3be        */
#define T4_MPCOEF	0x2a68	/* twelve shorts, `getMPrecvdBits`' other
				   half (docs/v90cpp.md, finding F227)      */
#define T4_F356C	0x356c	/* short: 0x1e once S has been detected    */
#define T4_F3570	0x3570	/* short: 3 once S has been detected       */
#define T4_F3576	0x3576	/* short: cleared with it                  */
#define T4_F3598	0x3598	/* short: "initdigital has run"            */
#define T4_MDLEN	0x35a2	/* short: MD's length in bauds -- see below */
#define T4_MPTBL	0xaa0c	/* ten shorts, the MP sequence received    */
#define T4_MPRUN	0xaa26	/* short: the current run of one bits      */
#define T4_MPIDX	0xaa2a	/* short: which of the ten comes next      */
#define T4_MPACC	0xaa30	/* int:   the bit accumulator              */
#define T4_MPBITS	0xaa34	/* short: how many bits are in it          */
#define T4_MPCAPS	0xaa3c	/* THE LOW BYTE of `info_caps`; the three
				   readers are all `testb $1`, which is
				   finding F553's case again                */
#define T4_FAA80	0xaa80	/* short: cleared when the MD is over      */
#define T4_RXCARRDESC	0xaab0	/* the detector coefficients for the rate  */
#define T4_RTSCALE	0xaacc	/* int, SIGNED: scaled per baud at 0x6d111 */

#define T4_RX(obj)	((struct v34_receiver *)((char *)(obj) + 0x0264))
#define T4_M(p)		((unsigned char *)(p))
#define T4_I16(p, off)	(*(short *)(T4_M(p) + (off)))
#define T4_U16(p, off)	(*(unsigned short *)(T4_M(p) + (off)))
#define T4_I32(p, off)	(*(int *)(T4_M(p) + (off)))
#define T4_U32(p, off)	(*(unsigned *)(T4_M(p) + (off)))
#define T4_U8(p, off)	(*(unsigned char *)(T4_M(p) + (off)))

/*
 * The ten shorts of the received MP sequence, printed once when the sequence
 * ends.  Two format strings, one body: 0x70379 jumps into 0x70278 having
 * loaded the other literal, which is what says the two are one call site in
 * the source with the string chosen above it.
 *
 * Every argument is `movzwl`, so the table is read UNSIGNED here -- while
 * 0x6ff3d reads element zero `cmpw $0x0`/`js`, which is signed.  Both
 * spellings are in the object and both are below.
 */
static void
t4_mp_print(const char *fmt, const unsigned short *tbl)
{
	dsplibs_debug_printf(fmt,
			     (unsigned)tbl[0], (unsigned)tbl[1],
			     (unsigned)tbl[2], (unsigned)tbl[3],
			     (unsigned)tbl[4], (unsigned)tbl[5],
			     (unsigned)tbl[6], (unsigned)tbl[7],
			     (unsigned)tbl[8], (unsigned)tbl[9]);
}

/*
 * 0x702a5 and 0x7111a -- ONE construct emitted TWICE, 113 bytes each, same
 * shape and different register allocation.  Six values out of the MP
 * sequence become twelve shorts at +0x2a68: each lands twice, once as itself
 * and once negated or displaced by six, which is the shape of a complex
 * conjugate pair.  `getMPrecvdBits` is the reader and is already
 * reconstructed (finding F227).
 *
 * Written in the object's store order.  GCC does not simply preserve
 * statement order (finding F617), so the order here is a record of what is
 * there and not a claim about the source.
 */
static void
t4_mp_coeffs(struct v34_object *obj, const unsigned short *tbl)
{
	short *d = (short *)(T4_M(obj) + T4_MPCOEF);

	d[7]  = (short)tbl[2];
	d[0]  = (short)tbl[2];
	d[1]  = (short)-tbl[3];
	d[6]  = (short)tbl[3];
	d[9]  = (short)tbl[4];
	d[2]  = (short)tbl[4];
	d[3]  = (short)-tbl[5];
	d[8]  = (short)tbl[5];
	d[11] = (short)tbl[6];
	d[4]  = (short)tbl[6];
	d[5]  = (short)-tbl[7];
	d[10] = (short)tbl[7];
}

/*
 * 0x6ff3d -- the sequence is complete.  Ten words have arrived and the low
 * bit of the first says whether they carried coefficients; the sign bit of
 * the same word says which of the two messages it was.
 *
 * Both arms rejoin at 0x6ff6f, which is `hs_setstate(microstate, DET_SYNC)`
 * and then 0x68c6e's two clears -- the same tail the packer's own
 * bit-16 path reaches, which is why one label serves both.
 *
 * THE TRANSITION'S "ALREADY THERE" BRANCH AT 0x6ff81 CAN NEVER FIRE.  This
 * arm is entered with rxstate 4 and the packer at 0x68c2b sends microstate
 * 41 to the run-length path instead, so every path that reaches here has a
 * microstate that is not 41.  `hs_setstate` handles it anyway; finding F722's
 * case for the third time in this file.
 */
static unsigned short
t4_mp_sequence_end(struct v34_object *obj)
{
	struct v34_receiver *rx = T4_RX(obj);
	unsigned short *tbl = (unsigned short *)(T4_M(obj) + T4_MPTBL);

	if ((short)tbl[0] < 0) {
		/*
		 * 0x70282.  Bit 15 of word 0 is MP bit 33, the ACKNOWLEDGE bit
		 * of Tables 20 and 21/V.34 -- "0 = modem has not received MP
		 * from far end" -- so the sign test above it is what separates
		 * MP' from MP.  This arm ORs 0x90 where 0x6ff47 ORs 0x10, and
		 * 0x90 is exactly what 0x710e4 compares for before printing "E
		 * received before MP'".
		 */
		/* 0x70282 */
		rx->flags = (unsigned short)((rx->flags & ~0x98) | 0x90);
		if (T4_U8(tbl, 0) & 1)
			t4_mp_coeffs(obj, tbl);
		if (DSPLIB_DEBUG_ON())
			t4_mp_print("V34MP -MP1 sequence,%x,%x,%x,%x,%x,%x," "%x,%x,%x,%x\n", tbl);
	} else {
		/*
		 * 0x6ff47.  `and $0xffffff67` is ~0x98 done thirty-two bits
		 * wide on a halfword that was loaded `movzwl`, and only the
		 * store at 0x6ff62 narrows it again.
		 */
		/* 0x6ff47 */
		rx->flags = (unsigned short)((rx->flags & ~0x98)
					     | V34_RX_FLAG_TRN_WATCH);
		if (DSPLIB_DEBUG_ON())
			t4_mp_print("V34MP -MP sequence,%x,%x,%x,%x,%x,%x," "%x,%x,%x,%x\n", tbl);
	}

	/*
	 * 0x6ff6f reads +0x3592 and 0x6ff7d compares it with 0x29; equal jumps
	 * straight to the clears at 0x68c6e and stores nothing.  The `jbe` at
	 * 0x6ff8e skips the trace body at 0x6ff94, and that body is shared:
	 * 0x7093f, which the packer's own `ja` at 0x68c55 reaches, does
	 * nothing but reload the object and jump into it.
	 */
	hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);	/* 0x6ff6f */
	/*
	 * 0x68c6e and 0x68c78 are one shared pair of `movw $0x0` through %ebx
	 * = obj+0xaa0c, +0x1e and +0x1a.  0x6ff81's `je` lands on the first of
	 * them and the packer's resync falls into it from 0x68c67, which is
	 * why the same two addresses appear twice in this file.
	 */
	T4_I16(obj, T4_MPIDX) = 0;				/* 0x68c6e */
	T4_I16(obj, T4_MPRUN) = 0;				/* 0x68c78 */
	return rx->flags;
}

/*
 * 0x6cafd -- seventeen bits are in the accumulator, so one word of the MP
 * sequence is complete.
 *
 * THE COMPARE MASKS WITH 0x7fff AND THE STORE DOES NOT, and that is the one
 * defect in this arm no ordinary trial can see: a reconstruction that stores
 * the masked value agrees on every word whose bit 15 is clear.  0x6cb0f
 * masks the table entry, 0x6cb04 masks the new word, and 0x6cb2a stores
 * `%di` -- the UNMASKED low half of the sign-extended accumulator.  The
 * repeat counter at +0xaa78 is the only thing the equal case does
 * differently (0x6ff1e, thirty-one bytes).
 */
static unsigned short
t4_mp_word(struct v34_object *obj, unsigned acc)
{
	struct v34_receiver *rx = T4_RX(obj);
	unsigned short *tbl = (unsigned short *)(T4_M(obj) + T4_MPTBL);
	unsigned short idx = T4_U16(obj, T4_MPIDX);
	short w = (short)acc;

	if ((tbl[(short)idx] & 0x7fff) == (w & 0x7fff))
		/*
		 * 0x6ff1e steps +0xaa78, finding F633's busiest counter, and
		 * comes back by way of a reload: 0x6ff34 re-reads the index
		 * from +0xaa2a because the increment used %ecx.  The 0x7fff
		 * mask above drops bit 15, which in word 0 is the acknowledge
		 * bit, so an MP and the MP' that follows it count as a repeat.
		 */
		T4_U16(obj, T3C_COUNT)++;		/* 0x6ff1e */

	/*
	 * 0x6cb21, and seventeen is not a typo for sixteen: `shr $0x11` at
	 * 0x6cb24 and `sub $0x11` at 0x6cb40.  A group is sixteen data bits
	 * plus the start bit behind them -- MP bits 34, 51, 68 in Tables 20
	 * and 21/V.34 -- and the store at 0x6cb2a keeps only the low half.
	 */
	/* 0x6cb21 */
	tbl[(short)idx] = (unsigned short)w;
	T4_U32(obj, T4_MPACC) = acc >> 17;
	idx = (unsigned short)(idx + 1);
	T4_U16(obj, T4_MPIDX) = idx;
	T4_U16(obj, T4_MPBITS) = (unsigned short)(T4_U16(obj, T4_MPBITS) - 0x11);

	/*
	 * 0x6cb43.  The low bit of the first word says how long the sequence
	 * is: ten words with it, four without.  Both tests are `cmp`/`je` on
	 * the index just written, so a sequence that overruns either count
	 * never ends -- which is the object's behaviour and not a defect of
	 * this reading.
	 */
	/*
	 * TEN AND FOUR ARE THE TWO MP TYPES.  The bit 0x6cb43 tests is bit 0
	 * of word 0, which is MP bit 18: "Type: 0" in Table 20/V.34 and "Type:
	 * 1" in Table 21.  Type 0 carries four sixteen-bit fields (bits 18:33,
	 * 35:50, 52:67, 69:84) and Type 1 carries ten, the extra six being the
	 * precoding coefficients h(1) to h(3), real and imaginary.  0x6cb54
	 * and 0x6d2de are those two counts.
	 */
	if (T4_U8(obj, T4_MPTBL) & 1) {
		if (idx == 0xa)				/* 0x6cb54 */
			return t4_mp_sequence_end(obj);
	} else {
		if (idx == 4)				/* 0x6d2de */
			return t4_mp_sequence_end(obj);
	}
	return rx->flags;
}

/*
 * 0x710e1 -- a run of more than nineteen one bits.  E has arrived, phase 3
 * is over for the receiver, and the digital half is brought up if nothing
 * else has done it.
 */
static unsigned short
t4_mp_e_sequence(struct v34_object *obj, unsigned acc, unsigned short f)
{
	struct v34_receiver *rx = T4_RX(obj);
	unsigned short *tbl = (unsigned short *)(T4_M(obj) + T4_MPTBL);

	T4_U32(obj, T4_MPACC) = acc;

	/*
	 * 0x710e4 jumps PAST the coefficient copy as well as past the
	 * diagnostic, so the copy is inside this test and not beside it.
	 */
	if ((f & 0x98) != 0x90) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34MP- E received before MP', "
					     "rxflgs = 0x%x\n", (unsigned)f);
		/*
		 * 0x71111's `testb $0x1,(%ebx)` is the Type bit again, and
		 * only a Type 1 sequence carries coefficients (Table 21/V.34),
		 * so a Type 0 leaves the twelve shorts alone.  0x71114's `je`
		 * and 0x710ea's both arrive at 0x71189, through 0x71249 and
		 * 0x71259, and 0x71189 is where the flag word is written.
		 */
		if (T4_U8(tbl, 0) & 1)
			t4_mp_coeffs(obj, tbl);
	}

	/* 0x71189 */
	rx->flags = (unsigned short)(rx->flags | 0x98);
	rx->subframe_idx = 0;
	T4_I16(obj, T3C_COUNT) = 0;
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34MP - E sequence detected,"
				     "rxflgs= 0x%x\n", (unsigned)rx->flags);

	/*
	 * 0x711ee.  The argument is built by `lea 0x0(%ebp,%ebp,4)` and `add
	 * %eax,%eax` -- times five, then doubled -- and the `cwtl` at 0x711e9
	 * cuts the product to sixteen bits before sign-extending it again,
	 * which is what the (short) is.  +0x1d0 itself is read `movswl`.
	 */
	VPcmV34LogTimingOffset(obj, (short)(rx->timing_offset * 10));	/* 0x711ee */

	/*
	 * 0x711fe is `test $0x40,%ah`, bit 14 of the word loaded at 0x711f7,
	 * and the bit it sets is 0x2000.  The store at 0x71208 is INSIDE the
	 * branch, so a clear 0x4000 leaves +0x122 untouched rather than
	 * rewriting it with the value just read.
	 */
	if (rx->flags & 0x4000)					/* 0x711fe */
		rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_PRECODE);

	/*
	 * 0x71216 is `cmpw $0x0,0x3598(%esi)`, and the 1 that replaces it is
	 * materialised in %ebx at 0x71223 -- before the call, because %ebx
	 * survives it -- and stored at 0x7122d once `initdigital` returns.
	 */
	if (T4_I16(obj, T4_F3598) == 0) {			/* 0x71216 */
		initdigital(obj);
		T4_I16(obj, T4_F3598) = 1;
	}
	/*
	 * 0x7123d, and then `jmp 68c7e` -- PAST the two clears at 0x68c6e and
	 * 0x68c78, onto the flag reload behind them.  E leaves the index and
	 * the run where they are; only the framing paths clear them.
	 */
	rx->equ_step = 0x4000;					/* 0x7123d */
	return rx->flags;
}

/*
 * 0x6cb70 -- microstate 41 `DET_SYNC`, so the packer is not collecting
 * words: it is counting runs of one bits, looking for the nineteen that mark
 * E and for the seventeen that mark the end of MP.
 *
 * The loop is the object's: take the low bit, step the counter in the object
 * rather than in a register, shift, and act.  0x71264's transition to 44
 * `DET_INFO` has an "already there" branch at 0x71275 that can never fire --
 * the microstate is 41 on every path that reaches it.
 */
static unsigned short
t4_mp_runlength(struct v34_object *obj, unsigned acc, unsigned short count)
{
	struct v34_receiver *rx = T4_RX(obj);

	/*
	 * 0x6cbca re-reads the count from +0xaa34 at the top of every pass
	 * instead of keeping it in a register, and the accumulator lives in a
	 * stack slot: 0x6cbd3 and 0x6cbd8 both load 0x70(%esp), one copy for
	 * the low bit and one for the shift.
	 */
	while (count != 0) {					/* 0x6cbca */
		/*
		 * 0x6cbe0 takes the bit BEFORE the shift and from the second
		 * copy, and 0x6cbdc has already written the decremented count
		 * back to +0xaa34 -- so a return from inside this loop leaves
		 * the object holding exactly the bits it has not consumed.
		 */
		int one = (int)(acc & 1);			/* 0x6cbe0 */

		count = (unsigned short)(count - 1);
		T4_U16(obj, T4_MPBITS) = count;
		acc >>= 1;

		if (one) {
			/*
			 * 0x6cb83 keeps the incremented run in BOTH places,
			 * +0xaa26 and 0x3c(%esp), and 0x6cbbe compares the
			 * stack copy.  Twenty ones is E, "a 20-bit sequence of
			 * binary ones used to signal the end of MP" (V.34
			 * 10.1.3.2), which is why the test is `cmpw $0x13`
			 * with `jg`.
			 */
			/* 0x6cb83 */
			unsigned short run;
			unsigned short f;

			run = (unsigned short)(T4_U16(obj, T4_MPRUN) + 1);
			T4_U16(obj, T4_MPRUN) = run;
			f = rx->flags;
			if ((f & 0x98) == 0x90 || (T4_U8(obj, T4_MPCAPS) & 1)) {
				if ((short)run > 0x13)
					return t4_mp_e_sequence(obj, acc, f);
			}
		} else {
			/*
			 * 0x6cbed reads the run straight out of +0xaa26 --
			 * `cmpw $0x10,0x1a(%ebx)` -- so more than sixteen ones
			 * followed by a zero is the seventeen-bit frame sync
			 * with its start bit behind it, MP bits 0:16 and 17 of
			 * Tables 20 and 21/V.34.
			 */
			/* 0x6cbed */
			if ((short)T4_U16(obj, T4_MPRUN) > 0x10) {
				/*
				 * 0x71264 stores %ecx, the accumulator AFTER
				 * 0x6cbe3's shift, so the zero that ended the
				 * run is already out of it when the collector
				 * starts.
				 */
				/* 0x71264 */
				T4_U32(obj, T4_MPACC) = acc;
				hs_setstate(obj, HS_MICROSTATE, V34HS_DET_INFO);
				T4_I16(obj, T3C_COUNT) = 0;
				T4_I16(obj, T4_MPIDX) = 0;
				T4_I16(obj, T4_MPRUN) = 0;
				return rx->flags;
			}
			T4_U16(obj, T4_MPRUN) = 0;
		}
	}

	/*
	 * 0x6cc00 stores the accumulator and jumps to 0x6a4e4 -- one
	 * instruction PAST the identical store at 0x6a4e1 -- so both ways out
	 * of the packer share the flag reload and the `jmp 67a1e` behind it.
	 */
	T4_U32(obj, T4_MPACC) = acc;				/* 0x6cc00 */
	return rx->flags;
}

/*
 * 0x68bd8 -- the MP bit packer.  Two bits arrive per block in the decoder's
 * `best_index`, and they are shifted into a 32-bit accumulator at +0xaa30 at
 * whatever bit position +0xaa34 says.
 *
 * THE TEST AT 0x68c3f IS OF BIT 16 of that accumulator, not of a halfword,
 * which is what says the accumulator is one 32-bit object and not two.
 */
static unsigned short
t4_mp_packer(struct v34_object *obj)
{
	struct v34_receiver *rx = T4_RX(obj);
	/*
	 * %ebx IS obj+0xaa0c from 0x68bf1, so five of the packer's fields are
	 * addressed off one base and the disassembly names them by their
	 * displacement: +0x0 the table, +0x1a the run, +0x1e the index, +0x24
	 * the accumulator, +0x28 the bit count.  0x68bf7 is a full 32-bit
	 * `mov` and 0x68bfa a `movzwl`.
	 */
	unsigned acc = T4_U32(obj, T4_MPACC);			/* 0x68bf7 */
	unsigned short n = T4_U16(obj, T4_MPBITS);		/* 0x68bfa */
	/*
	 * 0x68c05 is `movswl %bp,%ecx`: the shift count is the SIGNED halfword
	 * although 0x68bfa loaded it zero-extended.  The mask is built the
	 * long way -- $0xffffffff into %esi at 0x68be8, `shl %cl`, `not` --
	 * and that is a thirty-two-bit object being cleared from bit `sh`
	 * upwards.
	 *
	 * AND THE VALUE GOING IN IS NOT MASKED.  0x68bfe reads +0x126 with
	 * `movswl` and 0x68c0c shifts it straight, so the `or` at 0x68c17
	 * carries every bit above bit 1 into the accumulator.  The TRN2 path
	 * reading the same field does mask it -- 0x679f8 `movswl`, then
	 * 0x67a02 `and $0x3` -- so the two readers of +0x126 disagree about
	 * its width, and this one is the object's behaviour, not an omission
	 * here.
	 */
	int sh = (short)n;					/* 0x68c05 */

	acc = (acc & ~(0xffffffffu << sh))
	      | ((unsigned)((int)rx->best_index << sh));		/* 0x68c17 */
	n = (unsigned short)(n + 2);
	T4_U16(obj, T4_MPBITS) = n;				/* 0x68c1c */

	/*
	 * 0x68c2b's `je 6cb70` hands the run counter the accumulator and the
	 * count it has just computed: 0x68c27 spilled the accumulator to
	 * 0x70(%esp) and %ax still holds the count, so nothing is re-read
	 * across the split.
	 */
	if (obj->microstate == V34HS_DET_SYNC)			/* 0x68c2b */
		return t4_mp_runlength(obj, acc, n);

	/*
	 * 0x68c35 is `cmp $0x10,%ax` with `jle`, a SIGNED sixteen-bit compare.
	 * Falling through means seventeen or more bits are in the accumulator,
	 * which is what lets 0x68c3f treat bit 16 as the group's start bit.
	 */
	if ((short)n <= 0x10) {					/* 0x68c35 */
		/*
		 * 0x6a4e1 is shared with the run counter: 0x6cb73's
		 * empty-count exit jumps to this store and 0x6cc00 jumps one
		 * instruction past it.
		 */
		T4_U32(obj, T4_MPACC) = acc;			/* 0x6a4e1 */
		return rx->flags;
	}

	/*
	 * THIS IS MP'S START BIT.  Every group in Tables 20 and 21/V.34 is
	 * sixteen bits followed by a start bit of 0 -- MP bits 17, 34, 51, 68
	 * and on -- so bit 16 of the accumulator is 0 while the framing holds
	 * and 1 once it has been lost.  0x68c3f tests it thirty-two bits wide
	 * and 0x68c45 takes the good case to the word store.
	 */
	if ((acc & 0x10000) == 0)				/* 0x68c3f */
		return t4_mp_word(obj, acc);

	/*
	 * The framing is gone, so 0x68c52 keeps the accumulator and the arm
	 * goes back to counting ones for the seventeen-bit frame sync.  There
	 * is no "already there" compare in front of this transition: 0x68c2b
	 * has already proved the microstate is not 0x29 on this path and the
	 * compiler kept that, which is finding F392's case.  The two clears
	 * below are 0x68c6e and 0x68c78, the pair 0x6ff81 also jumps into.
	 */
	T4_U32(obj, T4_MPACC) = acc;				/* 0x68c52 */
	hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC);
	T4_I16(obj, T4_MPIDX) = 0;				/* 0x68c6e */
	T4_I16(obj, T4_MPRUN) = 0;				/* 0x68c78 */
	return rx->flags;
}

/*
 * 0x679bc -- the TRN2 shift registers, and the three patterns they are
 * compared against.
 *
 * Two fourteen-bit registers are fed two bits a block out of the decoder,
 * and which of the two is fed depends on a flag the arm itself raises.  All
 * three constants -- 0x899f, 0x8990 and 0x89b0 -- are sync words; matching
 * either of the last two is what says the timing offset is worth reporting.
 */
static unsigned short
t4_trn2(struct v34_object *obj, unsigned short flags)
{
	struct v34_receiver *rx = T4_RX(obj);
	unsigned short f = rx->flags;			/* 0x679c0, re-read */
	unsigned short a, b, d;

	/*
	 * 0x899f IS J', Table 19/V.34.  Its sixteen bits are 1111100110010001
	 * with the leftmost first in time, and read into this register --
	 * first bit at bit 0 -- that is 0x899f exactly.  The arm 0x679c7's
	 * `test $0x8,%di` selects is the one that looks for it, only +0x11c is
	 * fed here, and 0x6a39d's store sits between the compare and the
	 * branch, so both ways out store.
	 */
	if (f & V34_RX_FLAG_LATE_TRN) {
		/* 0x6a377 */
		a = (unsigned short)((((int)rx->best_index & 3) << 14)
				     | (T4_U16(rx, T4_RXF11C) >> 2));
		T4_U16(rx, T4_RXF11C) = a;
		if (a != 0x899f)
			return flags;
		T4_U16(rx, T4_RXF120) = 0;
		rx->flags = (unsigned short)(f | V34_RX_FLAG_TRN_WATCH);
		return rx->flags;
	}

	/*
	 * 0x679d2.  The two halfwords are ONE thirty-two-bit shift register:
	 * the new pair enters at the top of +0x11c and +0x11c's bottom two
	 * bits enter the top of +0x11e, so +0x11e is +0x11c delayed by eight
	 * blocks.  Comparing them compares a sixteen-bit window with the
	 * sixteen bits before it, which is what "a whole number of repetitions
	 * of one of the two 16-bit patterns" (V.34 10.1.3.3) makes equal.
	 */
	/* 0x679d2 */
	a = T4_U16(rx, T4_RXF11C);
	d = (unsigned short)(((a & 3) << 14) | (T4_U16(rx, T4_RXF11E) >> 2));
	T4_U16(rx, T4_RXF11E) = d;
	b = (unsigned short)((((int)rx->best_index & 3) << 14) | (a >> 2));

	/*
	 * 0x67a0a compares the two windows in registers, `cmp %dx,%bx`, and
	 * the unequal path at 0x67a17 stores the new one and falls straight
	 * into the merge at 0x67a1e -- which reads %esi, the flags the arm was
	 * called with.  +0x122 is not re-read on this path.
	 */
	if (b != d) {						/* 0x67a0a */
		T4_U16(rx, T4_RXF11C) = b;			/* 0x67a17 */
		return flags;
	}

	/*
	 * BOTH CONSTANTS ARE J, Table 18/V.34.  0000100110010001 is the
	 * 4-point pattern and 0000110110010001 the 16-point one, leftmost bit
	 * first in time; read into this register, first bit at bit 0, they are
	 * 0x8990 and 0x89b0 exactly.  0x6b47a tests them branchlessly -- `sete
	 * %dl`, `sete %al`, `or`, `test $0x1,%al` -- so both compares always
	 * run.
	 */
	/* 0x6b47a */
	if (b != 0x8990 && b != 0x89b0) {
		/*
		 * 0x6b49b is the same store as 0x67a17 and the object did not
		 * share it: a window that repeated but is not J leaves through
		 * its own copy, so the address is the only thing telling the
		 * two returns apart.
		 */
		T4_U16(rx, T4_RXF11C) = b;			/* 0x6b49b */
		return flags;
	}

	/*
	 * 0x6fe67.  BOTH registers are cleared, not only the one that matched,
	 * and the bit is OR'ed into the value read back at 0x679c0 rather than
	 * into a fresh load of +0x122.  The timing offset is built the same
	 * way as at 0x711e3: times five, doubled, `cwtl`.
	 */
	/* 0x6fe67 */
	T4_U16(rx, T4_RXF11C) = 0;
	T4_U16(rx, T4_RXF120) = 0;
	rx->flags = (unsigned short)(f | V34_RX_FLAG_LATE_TRN);
	VPcmV34LogTimingOffset(obj, (short)(rx->timing_offset * 10));
	return rx->flags;
}

/*
 * 0x6a090 -- MD's bauds have run out.
 *
 * MD IS "MANUFACTURER-DEFINED", NOT "message descriptor", which is what this
 * file used to call it here and at `T4_MDLEN`.  ITU-T V.34 (02/98) 10.1.3.5:
 * an OPTIONAL signal a transmitting modem sends to train its echo canceller
 * when the phase-3 TRN cannot do it, whose length is carried in that modem's
 * INFO1 and is **0 when the signal is absent** -- which is exactly the `md ==
 * 0` guard below, and why the object's own trace reads "RX MDLENGTH over, it
 * was %d bauds".  Bauds, not bytes: nothing here is a descriptor.
 *
 * `setupreceiver` INLINED, and then a SECOND `detectorinit` over the same
 * detector with different arguments: 8/10 for the one inside the callee
 * against 10/0x28 here, which is what fixes the 0x6a225 boundary
 * independently of the byte comparison against microstate 44's copy.
 */
static void
t4_md_over(struct v34_object *obj, unsigned short flags)
{
	struct v34_receiver *rx = T4_RX(obj);
	unsigned short md = T4_U16(obj, T4_MDLEN);

	/*
	 * BOTH ADDRESSES BELOW NAME EXITS, not compares.  The compares are
	 * back at the head, 0x6a09e and 0x6a0b2; 0x6aad3 and 0x6d293 are the
	 * nineteen-byte "read txstate, jump 0x62af1" idiom the block comment
	 * above counts fifteen of.  A zero length is the signal's absence --
	 * the INFO1 length indication is 0 when it is not sent (V.34
	 * 10.1.3.5).
	 */
	if (md == 0)						/* 0x6aad3 */
		return;
	/* 0x6d293 again, and 0x6a0b2's `cmp %ax,%cx`/`jle` is signed. */
	if ((short)rx->rx_blocks <= (short)md)			/* 0x6d293 */
		return;

	/*
	 * 0x71667 is the diagnostic's BODY, placed out of line: the level test
	 * is back at 0x6a0bb and the body returns to 0x6a0c8.  The object's
	 * own format string names two of the fields it pushes -- +0x1c0 is
	 * "pllcnt" and +0x136 is "rxgain" -- and every argument is loaded
	 * `movswl`.
	 */
	if (DSPLIB_DEBUG_ON())					/* 0x71667 */
		dsplibs_debug_printf("RX MDLENGTH over, it was %d bauds,"
				     "rxflgs = 0x%x,pllcnt = %d,rxgain=0x%x\n",
				     (int)rx->rx_blocks, (unsigned)flags,
				     (int)rx->pllcnt, (int)rx->agc_gain);

	/*
	 * 0x6a0c8.  The call there is `rxinit`; the rest of `setupreceiver`
	 * runs inline to 0x6a225 -- the ladder on +0xaa96, +0x128 = 4, the
	 * +0xaaa8 ladder, +0x13a = 0x2000, +0x136 out of +0x262, flags &=
	 * 0xf0ff, its own `detectorinit(det, *(+0xaab0), 0, 8, 10, 0x600, 0)`
	 * at 0x6a200 and flags |= 0x200.  Finding F426 read that inline store
	 * for store in another arm.
	 */
	setupreceiver(obj);					/* 0x6a0c8 */

	/*
	 * 0x6a22c.  %ax was zeroed at 0x6a20c: this clears the same +0xaa80
	 * the freeze at 0x67a82 tests with `cmpw $0x0`, so the rate change
	 * that would have raised 0x200 on the next block is cancelled before
	 * the detector below is armed.
	 */
	T4_I16(obj, T4_FAA80) = 0;				/* 0x6a22c */
	detectorinit(T41_DET(obj),
		     *(const short **)(T4_M(obj) + T4_RXCARRDESC),
		     0, 10, 0x28, 0x600, 0);			/* 0x6a25d */
	/*
	 * 0x6a276 raises 0x200 for the SECOND time in this function -- the
	 * inlined `setupreceiver` already raised it at 0x6a218 -- and 0x6a283
	 * then clears +0x35a2, the word the entry test at 0x6a097 reads, so
	 * the wait runs once and the next block falls straight through.
	 */
	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);
	T4_I16(obj, T4_MDLEN) = 0;				/* 0x6a283 */
	/*
	 * 0x6a28a.  `mov %ax,0x3be(%ebp)`, %ebp reloaded from 0x4c(%esp) at
	 * 0x6a272 -- the base the increment at 0x67b37 works on, which is what
	 * the `T4_PLLCNT` define means by 0x4c(%esp) plus 0x3be.  Detecting S
	 * counts it up; the end of the wait puts it back to zero.
	 */
	T4_I16(obj, T4_PLLCNT) = 0;				/* 0x6a28a */
}

/*
 * 0x6d111 -- the round-trip measurement at +0xaacc, scaled by the baud rate
 * that was chosen.  Four arms and no default, all of them a 32-bit SIGNED
 * `sar $0x8` on a value the object holds as an `int`; `tools/whichfield.py`
 * calls +0xaacc `unmapped_aa98[52]`, so declaring its width is part of the
 * work and the `sar` is what declares it.
 */
static void
t4_scale_rtd(struct v34_object *obj)
{
	int v = T4_I32(obj, T4_RTSCALE);
	short baud = obj->baud_rate;

	if (baud == 0xd65)
		T4_I32(obj, T4_RTSCALE) = (v * 0xb3) >> 8;
	else if (baud == 0xc80)
		T4_I32(obj, T4_RTSCALE) = (v * 0xc0) >> 8;
	else if (baud == 0xbb8)
		T4_I32(obj, T4_RTSCALE) = (v * 205) >> 8;
	else if (baud == 0xaf0)
		T4_I32(obj, T4_RTSCALE) = (v * 0xdb) >> 8;
}

/*
 * 0x67999 -- everything the arm does once the retrain test at the entry has
 * declined.
 */
static void
t4_receive_body(struct v34_object *obj, unsigned short flags)
{
	struct v34_receiver *rx = T4_RX(obj);
	short n;

	/* 0x679a4.  `je 67a1e` -- all three up skips both TRN arms. */
	if ((flags & 0x98) != 0x98) {
		/*
		 * 0x679af `test $0x10,%dl; jne 68bd8` takes the watch arm out
		 * of line, and 0x68bd8's first act is `test $0x4,%dh; je
		 * 67a1e` -- with 0x400 clear it goes back to the J test
		 * without touching the packer.
		 */
		if (flags & V34_RX_FLAG_TRN_WATCH) {
			/* 0x68bd8 */
			if (flags & V34_RX_FLAG_DATA)
				flags = t4_mp_packer(obj);
		/*
		 * 0x679b5.  `cmp $0xbb8,%di` with `jle 67a1e`: a SIGNED
		 * sixteen-bit compare on the same counter the cap at 0x67a3a
		 * bounds, so the J registers are fed only past 3,000 bauds and
		 * a counter that has gone negative takes the same exit as a
		 * small one.
		 */
		} else if ((short)rx->rx_blocks > 0xbb8) {		/* 0x679b5 */
			flags = t4_trn2(obj, flags);
		}
	}

	/*
	 * 0x67a1e.  The far end's J: 0x08 raised with 0x10 and 0x80 clear,
	 * and the marker at +0x359c saying this end is the one that
	 * ORIGINATES -- 0x65, which `v34modeminit` reads as `originate` and
	 * every other site in this file agrees with.  This clause used to say
	 * "answers", against the guard three lines below it.
	 * `v34setuptxmit` FORCES txstate 18 SSEG, which is the only path in
	 * this arm that moves a state word the caller did not seed.
	 */
	if ((flags & 0x98) == V34_RX_FLAG_LATE_TRN
	    && T4_I16(obj, 0x359c) == 0x65) {
		/*
		 * 0x68e50 is the body, not a call: `settxlevel` against the
		 * +0xa9dc record, `V34SetupModulator` on the three shorts at
		 * +0xaa84, +0xaa94 and +0xaa8a, +0x122 &= ~0x800, +0x3594
		 * driven to 0x23 and txstate +0x3596 to 0x12 -- each behind
		 * `hs_setstate`'s compare-then-trace -- +0x25c0 cleared with
		 * 0x200 raised in +0x25c2, and `txinit` last.
		 */
		v34setuptxmit(obj);				/* 0x68e50 */

		/* 0x68f86.  0x6666 is 0.8 in Q15, 0x4000 the rounding. */
		rx->agc_start_gain = (short)(((int)rx->agc_gain * 0x6666 + 0x4000) >> 15);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Agc gain estimate at the end of "
					     "phase 3 is %d\n", (int)rx->agc_start_gain);

		/*
		 * 0x68fb1 zeroes +0x1c0 and 0x68fbd raises 0x10 in the same
		 * flag word; 0x68fc7 then copies the stored value back into
		 * %esi, which is the `flags` the rest of the arm works from.
		 */
		rx->pllcnt = 0;					/* 0x68fb1 */
		rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_TRN_WATCH);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34TIMING, detected  J, Phase =  "
					     "%x,%x, clk =  %x,%x  \n",
					     T4_U32(rx, T3C_TIMER_HI) >> 16,
					     T4_U32(rx, T3C_TIMER_HI) & 0xffff,
					     T4_U32(rx, T3C_TIMER_LO) >> 16,
					     T4_U32(rx, T3C_TIMER_LO) & 0xffff);
		flags = rx->flags;
	}

	/* 0x67a2f.  The baud counter, capped rather than wrapped. */
	n = rx->rx_blocks;
	if (n <= 0x61a7)
		rx->rx_blocks = (short)(n + 1);

	/*
	 * 0x67a4b.  `test $0x4,%dh` -- bit 0x400, tested in the high byte of
	 * the working copy in %si.  Taken, it leaves at 0x686f1: nothing below
	 * this line runs while the data bit is up.
	 */
	if (flags & V34_RX_FLAG_DATA)				/* 0x67a4b */
		/*
		 * 0x686f1 is not a `ret`: `movzwl 0x3596(%ebp),%ecx; jmp
		 * 62af1`, the tail into `t3c_txblock` with the tx state word
		 * already in %ecx. 0x69b3c, 0x6a291, 0x6d155 and 0x67b90 all
		 * end the same way.
		 */
		return;						/* 0x686f1 */

	/*
	 * 0x67a5b.  `cmpw $0x3,0x3570(%ecx)` straight against memory, and the
	 * 3 is the one 0x67b22 stores when the tone is detected: the arm sets
	 * the state it later tests.
	 */
	if (T4_I16(obj, T4_F3570) == 3) {			/* 0x67a5b */
		/*
		 * 0x67a63 `je 6a090` -- a branch, not a call, and all three
		 * exits of that block are the shared tail.  MD is V.34's
		 * optional Manufacturer-Defined echo-canceller training signal
		 * (10.1.3.5); the modem that sends it announces its length in
		 * INFO1, and the receiver sits out that length before it looks
		 * for S again.
		 */
		t4_md_over(obj, flags);				/* 0x6a090 */
		return;
	}

	/*
	 * 0x67a69.  The AGC is frozen before the detector runs -- 0x100
	 * always, 0x200 as well when +0xaa80 says a rate change is pending --
	 * and BOTH stores go to the object, so a detector that does not
	 * assert still leaves the flag raised.
	 */
	flags = (unsigned short)(flags | V34_RX_FLAG_TRAINED);
	rx->flags = flags;
	if (T4_I16(obj, T4_FAA80) != 0) {
		flags = (unsigned short)(flags | V34_RX_FLAG_DET_PENDING);
		rx->flags = flags;
	}

	/*
	 * 0x67a98.  `t41_detect`'s arguments exactly -- the receiver, the
	 * detector at +0x3564, the samples at receiver + 0x10c and
	 * `rx_samples` as the end pointer -- and the result is tested `%ax`.
	 *
	 * IT WRITES `rx->flags`.  0x73869 clears 0x200 on the asserting path,
	 * so the value 0x67b56 reads back is not the one stored above.
	 * Finding F429's trap in a new place.
	 */
	/*
	 * 0x67acd `je 69b3c`, and 0x69b3c is three instructions: reload obj,
	 * `movzwl 0x3596`, `jmp 62af1`.  A detector that does not assert ends
	 * the block there without another store.
	 */
	if (t41_detect(obj) == 0)				/* 0x69b3c */
		return;

	/*
	 * 0x67ad3.  This trace is inline -- `jbe 67afe` steps over it -- where
	 * the arm's others are cold blocks reached by `ja`.  Its two arguments
	 * are read at different widths: `movzwl 0x122` for the flags and
	 * `movswl 0x136` for the gain.
	 */
	if (DSPLIB_DEBUG_ON())					/* 0x67ad3 */
		dsplibs_debug_printf("S detected,rxflgs= 0x%x,rxgain=0x%x\n",
				     (unsigned)rx->flags, (int)rx->agc_gain);

	/*
	 * 0x67b17.  Four stores in one group, out of constants loaded at
	 * 0x67b05-0x67b12: +0x356c = 0x1e, +0x3570 = 3, +0x3576 = 0 and the
	 * baud counter back to zero.  The 3 is what 0x67a5b reads on every
	 * block after this one.
	 */
	T4_I16(obj, T4_F356C) = 0x1e;				/* 0x67b17 */
	T4_I16(obj, T4_F3570) = 3;
	T4_I16(obj, T4_F3576) = 0;
	/*
	 * 0x67b30 puts the baud counter back to zero, and 0x67b37 is
	 * `movzwl`/`inc`/`mov` on +0x3be: sixteen bits, unsigned and with no
	 * cap, so this counter wraps where the one at 0x67a3a saturates.
	 */
	rx->rx_blocks = 0;
	T4_U16(obj, T4_PLLCNT)++;				/* 0x67b37 */

	if (T4_U16(obj, T4_MDLEN) != 0) {
		/*
		 * 0x6a49d.  THE WHOLE EXPRESSION IS 32 BITS and only the
		 * store truncates: +0xaa96 is sign-extended, +0x35a2 is
		 * ZERO-extended into the multiply, and 0x1e is added before
		 * anything narrows.
		 */
		short md = (short)(((((int)obj->baud_rate * 0x23d) >> 14)
				    * (int)T4_U16(obj, T4_MDLEN)) + 0x1e);

		T4_I16(obj, T4_MDLEN) = md;
		/*
		 * 0x6d26f's argument is the `cwtl` at 0x6d276 -- %ax, the
		 * value the store at 0x6a4c1 truncated, sign-extended -- so
		 * the trace prints what landed in +0x35a2 and not the 32-bit
		 * product.  The 0x23d over 2^14 above is 0.03497 s: one of the
		 * 35 ms increments V.34's INFO1 counts an MD length in
		 * (10.1.3.5).
		 */
		if (DSPLIB_DEBUG_ON())				/* 0x6d26f */
			dsplibs_debug_printf("RX MDLENGTH, getting into "
					     "waiting state for MD = %d\n",
					     (int)md);
		return;
	}

	/* 0x67b56.  Re-read, for the reason above. */
	flags = rx->flags;
	rx->pllcnt = 1;
	flags = (unsigned short)(flags & ~(V34_RX_FLAG_TRAINED
					   | V34_RX_FLAG_DET_PENDING));
	rx->flags = flags;

	/*
	 * 0x67b6e `test $0x10,%al` runs between the `and` and the store, so
	 * the bit is read out of the register on its way to +0x122; 0x67b77's
	 * `je` sends the not-watching case to 0x6d111.
	 */
	if ((flags & V34_RX_FLAG_TRN_WATCH) == 0) {		/* 0x67b77 */
		/*
		 * 0x6d111 re-reads +0xaa96 and compares it against 0xd65,
		 * 0xc80, 0xbb8 and 0xaf0 -- the top four of V.34's six symbol
		 * rates.  There is no default: 0xab7 and 0x960, which the
		 * ladder at 0x6a104 shows the object does know, leave +0xaacc
		 * unscaled.  Every arm of it ends in the shared tail, so this
		 * is the last thing the block does.
		 */
		t4_scale_rtd(obj);				/* 0x6d111 */
		return;
	}

	/*
	 * 0x67b84.  `mov 0x238(%edi),%edx; mov %edx,0xaacc(%esi)` -- 32 bits
	 * at both ends, which is what declares the pair int.  rxstate 72
	 * copies the same two fields with the same instruction pair at
	 * 0x67d92.
	 */
	T4_I32(obj, T4_RTSCALE) = T4_I32(rx, T3C_TIMER_LO);	/* 0x67b84 */
}

/*
 * 0x653e4 -- rxstate 4 `RECEIVE`.
 *
 * THE ENTRY IS A RETRAIN TEST AND THE COMPARISON IS THIRTY-TWO BITS WIDE.
 * 0x65417 sign-extends +0xaa96 into a register, 0x6541e forms `7 *` there
 * with a `lea`/`sub` pair, and 0x65427 compares 32 bits against the
 * sign-extended baud counter.  Spelling it sixteen bits wide is a defect no
 * legal baud can expose -- 7 * 3429 is 23,853, inside a short -- so
 * `t_v34hsrx4.c` drives it at a baud of 20,000, where the two readings
 * disagree about whether the modem retrains at all.  Finding F724's shape in
 * a second arm.
 *
 * AND THE DIRECTION IS `jle`: less-or-equal goes to the BODY.
 */
static void
t4_rx_receive(struct v34_object *obj)
{
	struct v34_receiver *rx;
	unsigned short flags;

	/*
	 * 0x653ee.  The arm's first act, ahead of the retrain test at 0x65401:
	 * the block is demodulated even on the call that hands the handshake
	 * back to `v34handshakinit`.
	 */
	receiver(obj);					/* 0x653ee */

	rx = T4_RX(obj);
	/*
	 * 0x653f7.  `movzwl 0x122(%edx),%esi` -- read once, and %esi carries
	 * it into the whole arm as the working copy -- 0x67999 reads it as
	 * `flags` -- while the retrain test below works on a second copy, in
	 * %ebx.
	 */
	flags = rx->flags;				/* 0x653f7 */

	if ((flags & V34_RX_FLAG_RETRAIN) != 0
	    || ((int)rx->rx_blocks > 7 * (int)obj->baud_rate
		&& (flags & 0x80) == 0)) {
		/*
		 * 0x6544e.  The second argument is a literal 1, pushed at
		 * 0x65447, and nothing follows the call but the trace and the
		 * shared tail -- the retrain path writes no state of its own.
		 */
		v34handshakinit(obj, 1);		/* 0x6544e */
		/*
		 * 0x690e1 is a cold block: the format string and no arguments
		 * at all, then `movzwl 0x3596`/`jmp 62af1` rather than a
		 * return to the gate at 0x65453.
		 */
		if (DSPLIB_DEBUG_ON())			/* 0x690e1 */
			dsplibs_debug_printf("V34RETRAIN, going into retrain " "in Handshake\n");
	} else {
		/*
		 * 0x67999 is the merge of both not-retraining branches -- the
		 * `jle` at 0x65429 and the `jne` at 0x65435 -- and it is
		 * entered with the flags in %si and the baud counter in %di,
		 * where the entry left them.
		 */
		t4_receive_body(obj, flags);		/* 0x67999 */
	}

	t3c_txblock(obj);
}

/*
 * ---------------------------------------------------------------------------
 * rxstate 72 `RX_L1`, 0x650c6 -- the line probe, measured one block at a time,
 * and the counter that decides what each block is for.
 *
 * 3,811 bytes over TWENTY-FOUR ranges and ninety-five blocks, every one of
 * them exclusive to this arm -- 0x650c6, 0x6551a, 0x67d70, 0x686ba, 0x6881e,
 * 0x68dd2, 0x691d0, 0x69611, 0x69995, 0x69a6f, 0x69bd1, 0x69c7a, 0x6a4f4,
 * 0x6a668, 0x6aa0b, 0x6aec1, 0x6afd7, 0x6b120, 0x6c83e, 0x6c9aa, 0x6c9f1,
 * 0x6ca93, 0x7086b and 0x71343.  SEVENTEEN EXITS AND ALL SEVENTEEN ARE
 * `jmp 0x62af1`: there is no `ret` in the arm and no fall-through into
 * another one, so every path ends in `t3c_txblock`.
 *
 * WHAT IT IS.  `obj->short_aa78` counts blocks up from the moment L1 starts.
 * While it is at or below 0x440 the arm runs `rxtiming` and then a LADDER on
 * the counter, and each rung correlates the same burst against one or both
 * DFT banks; once the counter passes 0x440 the probe is over and the arm
 * finishes the measurement, sets the demodulator up for 2400 baud and hands
 * the receive machine to phase 2's DPSK.  The originating end
 * (`obj->role == 0x65`) gets an extra block and then watches the FSK
 * demodulator for tone A; the answering end leaves immediately.
 *
 * NEARLY A QUARTER OF IT IS FIVE FUNCTIONS THIS TREE ALREADY HAS, INLINED --
 * finding F426's rule for the fifth, sixth and seventh time:
 *
 *     dftnlinitSignalBins  0x6a54c and 0x6a760, both on obj + 0xa320
 *     dftnlinitNoiseBins   0x6a5a1 and 0x6a7aa, both on obj + 0xa76c
 *     dftfreqinit          0x69d78, ninety-two bytes
 *     dpskDetectInfo1Init  0x67eb3, the clear and all eleven scalars
 *     V34SetupDemodulator  0x67db5 and 0x69667, both `(obj, 0x960, 0)`
 *
 * `V34SetupDemodulator`'s two sites schedule their stores DIFFERENTLY --
 * 0x67de7 writes +0x1ae before +0x1b0 and 0x69699 the other way -- which is
 * two schedules of one source and is why an n-gram screen missed it
 * entirely.  Its debug line at 0x691d0 and 0x6964c prints the literal 0x960
 * and 0, and the carrier switch has folded away completely because the
 * argument is a literal zero.  Do not reconstruct a store order for it.
 *
 * AND IT PINS A LAYOUT NOTHING ELSE COULD.  The two `dftnlinit*` bodies run
 * on obj + 0xa320 and obj + 0xa76c, which are `probe_bins[0]` and the four
 * bins immediately after `probe_bins[24]`; the averaging loops at 0x69d28
 * and 0x6a70f then read +0xc of each, so the signal bank OVERLAYS the probe
 * bank's first four bins and the noise bank is `nl_noise_bins`, which was
 * `unmapped_a76c` until this arm gave it a reader.  v34hshak.h's "neither
 * initialiser has a caller -- so nothing here reads across" was true when it
 * was written and is not any more.  Finding F739.
 *
 * ELEVEN HUNDRED BYTES ARE DIAGNOSTICS and 894 of that is `hs_setstate`'s
 * own transition trace at eight sites, three of which can never fire: the
 * arm is entered from `62b7a: cmp $0x48,%eax; je 650c6` with rxstate at 72
 * and nothing between entry and 0x67e1e, 0x65162 or 0x6ca01 writes +0x3594.
 * Writing `hs_setstate` reproduces all eight for free; unlike rxstate 4
 * nothing else here is dead.
 *
 * THREE THIRTY-TWO-BIT COMPARES A READER WOULD ASSUME ARE SIXTEEN.  0x6881e
 * sign-extends the counter and the round-trip delay into 32-bit registers
 * and compares there, against `0xf10 + rtd/4`, `0xf14 + rtd/4` and
 * `0xf2c + rtd/4`.  NO DIFFERENTIAL TRIAL CAN SEPARATE THAT FROM A 16-BIT
 * SPELLING, and that is a property of the object rather than of the fixture:
 * `rtd` is a short, so `rtd >> 2` is bounded by +-8192 and `0xf10 + rtd/4`
 * spans -4304..12048, which fits a short at every value the field can hold.
 * Unlike rxstate 53's `rtd + 0x2418`, the sum cannot overflow.  So this is
 * finding F613's case: the `movswl` is FORCED ENCODING and settles the
 * declared type even though the two readings agree everywhere.  What IS
 * testable, and is tested, is the SIGNEDNESS of the two loads -- a `movzwl`
 * reading of either moves the thresholds by tens of thousands.
 * ---------------------------------------------------------------------------
 */

#define T72_F358C	0x358c	/* short: toggled while waiting for tone A  */
#define T72_F359C	0x359c	/* short: 0x65 is the originating end       */
#define T72_FSKGATE	0xa8a0	/* int:   armed on the answering end's exit */
#define T72_BLK_A9DC	0xa9dc	/* the record whose length this arm sets    */
#define T72_PTR_AA70	0xaa70	/* pointer, aimed at that record            */
#define T72_NL_NOISE	0xaab4	/* int:   the 0x180 rung's noise total      */
#define T72_NL_SIGNAL	0xaab8	/* int:   and its signal total              */
#define T72_PB_NOISE	0xaabc	/* int:   the 0x300 rung's noise total      */
#define T72_PB_SIGNAL	0xaac0	/* int:   and its signal total              */
#define T72_NL_RATIO	0xaac4	/* int:   round(256 * signal / noise)       */
#define T72_PB_RATIO	0xaac8	/* int:   the same, for the 0x300 rung      */
#define T72_FAACC	0xaacc	/* int:   copied from the receiver's +0x238 */
#define T72_RX_SAMPS	0x010c	/* receiver: the burst every bank reads     */
#define T72_RX_F238	0x0238	/* receiver: an int, and rxstate 4 copies   */
				/* it too, at 0x67b84 -- not this arm's    */

/*
 * How many samples `rxtiming` left in the burst.
 *
 * 0x65534: `rx_samples` minus the start of the buffer, `sar $1` for the
 * element size, narrowed to a short because that is `dftupdate`'s parameter.
 * Computed once on the ladder path and handed to every correlation below.
 */
static short
t72_nsamples(const struct v34_receiver *rx)
{
	return (short)(rx->rx_samples
		       - (const short *)((const char *)rx + T72_RX_SAMPS));
}

/*
 * Correlate the burst against BOTH four-bin banks.
 *
 * 0x68dd2, 0x69995 and the heads of 0x69c7a and 0x6a668 -- the same two
 * calls four times over, which is one helper and not four transcriptions.
 * The four differ only in what they do afterwards.
 */
static void
t72_update_both(struct v34_object *obj, struct v34_receiver *rx, short n)
{
	const short *in = (const short *)((const char *)rx + T72_RX_SAMPS);

	/*
	 * 0x68dfd.  Four bins at obj+0xa320, which is `probe_bins[0]`: the
	 * signal bank overlays the first four of the twenty-five the 0x686ba
	 * rung fills.
	 */
	dftupdate(obj->probe_bins, V34_NL_BINS, in, n);		/* 0x68dfd */
	/*
	 * 0x68e23.  obj+0xa76c, and the burst pointer in %ebx is the one the
	 * call above already used -- one `add $0x10c` serves both.  Finding
	 * F739 is what says +0xa76c is exactly four bins.
	 */
	dftupdate(obj->nl_noise_bins, V34_NL_BINS, in, n);	/* 0x68e23 */
}

/*
 * Reduce both banks and record signal, noise and their ratio.
 *
 * 0x69cd7..0x69d77 and 0x6a6c8..0x6a75f, one construct at two sets of
 * offsets: the 0x300 rung writes +0xaabc/+0xaac0/+0xaac8 and the 0x180 rung
 * +0xaab4/+0xaab8/+0xaac4.  Nothing else differs, which is why they are one
 * function here and why the offsets are parameters.
 *
 * `energy` IS WIDENED UNSIGNED -- `movzwl 0xa778` and `movzwl 0xa32c` with
 * the 32-bit result used -- which is forced encoding and is already this
 * tree's reading of that field (v34det.h, finding F212).  The divide is
 * unsigned too: `shr $1` and `div`, not `sar` and `idiv`.  The halved
 * denominator is a rounding term, so the result is round(256 * signal /
 * noise).
 *
 * THE TWO ZERO STORES BEFORE THE LOOP ARE DEAD IN THE OBJECT and are here
 * anyway.  They are what GCC's loop store motion leaves behind when the
 * source zeroes two memory accumulators and then adds to them in a loop: the
 * initialising stores stay put, the loop runs in registers, and the final
 * pair is sunk out -- which is also why the object stores signal before
 * noise on the way out and noise before signal on the way in.  Both are
 * overwritten unconditionally, so no differential test can see them; they
 * are written because the reading that explains the codegen is the one that
 * says the source had them.
 */
static void
t72_measure(struct v34_object *obj, unsigned noise_off, unsigned signal_off,
	    unsigned ratio_off)
{
	unsigned int noise;
	unsigned int signal;
	short i;

	/*
	 * 0x69cec.  The noise bank is reduced with `scale` 6 and the signal
	 * bank at 0x69d03 with 2 -- the only argument that differs between the
	 * two calls, and 0x67da3 reduces the same signal array over
	 * twenty-five bins with that same scale of 2.
	 */
	dftenergy(obj->nl_noise_bins, V34_NL_BINS, 6);	/* 0x69cec */
	/*
	 * 0x69d03.  The array 0x67da3 reduces over twenty-five bins, reduced
	 * here over four and with the same `scale` of 2.
	 */
	dftenergy(obj->probe_bins, V34_NL_BINS, 2);	/* 0x69d03 */

	/*
	 * %esi is zeroed at 0x69cd0, before the second `dftupdate`, and comes
	 * back through the call because %esi is callee-saved: that is how the
	 * stores at 0x69d1a and 0x69d22 are known to be zeroes rather than
	 * leftovers.
	 */
	t3c_puti(obj, noise_off, 0);			/* 0x69d1a */
	t3c_puti(obj, signal_off, 0);			/* 0x69d22, dead */

	/*
	 * 0x69d28.  The loop reads +0xc of each bank -- 0xa32c and 0xa778 --
	 * and walks both with one `add $0x2c,%edx`, the bin stride.  `i` is
	 * narrowed by `movswl %bp,%ebx` every turn and closed by a signed
	 * sixteen-bit compare against 3, which is what makes it a short.  The
	 * `shl $0x8` is per bin, before the add.
	 *
	 * The block sits above the accumulators rather than tight against the
	 * `for`, and that placement is forced: a comment CLOSE immediately
	 * above `for (i = 0; i <= 3; i++) {` is a mutation anchor elsewhere in
	 * this file, and a second copy of it makes that anchor match twice.
	 * `anchorcheck.py` catches it; nothing else would.
	 */
	noise = 0;
	signal = 0;
	for (i = 0; i <= 3; i++) {			/* 0x69d28 */
		noise += (unsigned short)obj->nl_noise_bins[i].energy;
		signal += (unsigned int)
			  (unsigned short)obj->probe_bins[i].energy << 8;
	}

	/*
	 * 0x69d55 stores the signal and 0x69d5b the noise -- the reverse of
	 * the zeroing order above -- and `test %ecx,%ecx` at 0x69d53 is
	 * scheduled ahead of both, so the guard below reads the register and
	 * not the field it has just written.
	 */
	t3c_puti(obj, signal_off, signal);		/* 0x69d55 */
	t3c_puti(obj, noise_off, noise);		/* 0x69d5b */

	/*
	 * The zero-noise arm is out of line in both rungs -- 0x6aa0b writes
	 * +0xaac8 and 0x6c9aa writes +0xaac4 -- and each `jmp`s back into its
	 * rung, at 0x69d78 and 0x6a760.  It has to exist because 0x69d70 is a
	 * hardware `div`, which faults on a zero divisor.
	 */
	if (noise == 0)
		t3c_puti(obj, ratio_off, 0);		/* 0x6aa0b, 0x6c9aa */
	else
		/*
		 * 0x69d69 halves the noise with `shr $1` and 0x69d70 divides
		 * with `f7 f1` -- DIV, not IDIV -- so both accumulators are
		 * unsigned, and the halved denominator is the rounding term.
		 */
		t3c_puti(obj, ratio_off,		/* 0x69d72, 0x6a75a */
			 (signal + noise / 2) / noise);
}

/*
 * What both ends do when the probe is over.
 *
 * 0x67d70 and 0x69611, store for store: an int off the receiver kept, the
 * whole twenty-five-bin bank reduced, the demodulator set up for 2400 baud
 * with no carrier, one receiver flag bit set and the gain restored from the
 * receiver's +0x264.  The two arms then diverge completely.
 */
static void
t72_probe_done(struct v34_object *obj, struct v34_receiver *rx)
{
	/*
	 * 0x67d85 loads the receiver's +0x238 and 0x67d92 stores it to
	 * +0xaacc, both 32 bits -- the same pair rxstate 4 copies at 0x67b84
	 * before it scales it per baud.
	 */
	t3c_puti(obj, T72_FAACC,				/* 0x67d92 */
		 *(const int *)((const char *)rx + T72_RX_F238));

	/*
	 * 0x67da3.  `$0x19` bins -- the whole bank, not the four the rungs
	 * used -- and `scale` 2, the same scale as 0x69d03.
	 */
	dftenergy(obj->probe_bins, V34_PROBE_BINS, 2);		/* 0x67da3 */

	/*
	 * 0x67db5 is the body, not a call: the baud argument has already
	 * folded into constants on the receiver -- +0x1ac = 0x1f40, and
	 * +0x1ae, +0x1b0 and +0x1be all 0x3e80 -- with +0x128 = 4 alongside.
	 */
	V34SetupDemodulator(obj, 0x960, 0);			/* 0x67db5 */

	/*
	 * 0x67dd2 reads +0x122 eight instructions ahead of the demodulator's
	 * stores and 0x67e10 writes it back; 0x67e10 is the only write to
	 * +0x122 in the block.  The bit it raises is the one rxstate 4 clears
	 * at 0x68edd.
	 */
	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_FIR);	/* 0x67e10 */
	/*
	 * 0x67de0 `movzwl 0x264` and 0x67e17 `mov %si,0x136`: sixteen bits
	 * copied, so the zero-extension never reaches the field -- which
	 * 0x67adc reads back with `movswl`.
	 */
	rx->agc_gain = (short)*(const unsigned short *)		/* 0x67e17 */
			((const char *)rx + T3M_RX_F264);
}

/*
 * The counter ladder, 0x6551a -- what one block of L1 is worth.
 *
 * THE LADDER MIXES SIGNEDNESS AND BOTH HALVES ARE FORCED.  `cmp $0x300,%dx`
 * with `jg` and `je` is a SIGNED sixteen-bit compare; the two window tests
 * are `lea -0x1c1(%edx),%eax; cmp $0x13e,%ax; jbe`, which is what GCC emits
 * for a two-sided test on a signed short and is exactly equivalent to it
 * over the whole range.  A counter that has gone negative therefore matches
 * no rung at all and the block is dropped.
 */
static void
t72_ladder(struct v34_object *obj, struct v34_receiver *rx)
{
	short count;
	short n;

	/*
	 * 0x65524.  Everything the ladder below needs is read on the
	 * instructions that follow the call: 0x65529 takes the counter at
	 * +0xaa78 and 0x65534 the receiver's +0x130.
	 */
	rxtiming(obj);					/* 0x65524 */

	n = t72_nsamples(rx);
	count = obj->short_aa78;

	if (count > 0x300) {
		/*
		 * 0x686ba.  The whole twenty-five-bin probe bank and nothing
		 * else: this is the probe itself being received.
		 */
		dftupdate(obj->probe_bins, V34_PROBE_BINS,
			  (const short *)((const char *)rx + T72_RX_SAMPS), n);
	} else if (count == 0x300) {
		/* 0x69c7a.  The probe's own signal-to-noise, and re-arm. */
		t72_update_both(obj, rx, n);
		t72_measure(obj, T72_PB_NOISE, T72_PB_SIGNAL, T72_PB_RATIO);
		/*
		 * 0x69d78.  The re-arm: ninety-two bytes of `dftfreqinit`
		 * inlined, twenty-five bins at the probe's own 150 Hz
		 * spacing, and the only one of the three initialisers that
		 * clears the double accumulators as well.  0x69d7f and
		 * 0x69d89 are its first two stores, `movw $0x0,(%edx)` and
		 * `movl $0x0,0x4(%edx)`.
		 */
		dftfreqinit(obj->probe_bins);		/* 0x69d78 */
	} else if (count >= 0x1c1 && count <= 0x2ff) {
		/* 0x68dd2.  Accumulate only. */
		t72_update_both(obj, rx, n);
	} else if (count == 0x180) {
		/* 0x6a668.  The nonlinear measurement, and re-arm. */
		t72_update_both(obj, rx, n);
		t72_measure(obj, T72_NL_NOISE, T72_NL_SIGNAL, T72_NL_RATIO);
		/*
		 * 0x6a760.  Four bins at 1050, 1350, 1950 and 2550 Hz laid
		 * over `probe_bins[0..3]` -- the four-bin clear at stride
		 * 0x2c, then `inc` written at +0x2, +0x2e, +0x5a and +0x86.
		 * Re-arming the signal bank destroys the probe bank's first
		 * four bins, and finding F739 reads the 0x40 and 0x180 rungs
		 * as doing that on purpose.
		 */
		dftnlinitSignalBins(obj->probe_bins);	/* 0x6a760 */
		/*
		 * 0x6a7aa.  Its partner on obj + 0xa76c: 900, 1200, 1800 and
		 * 2400 Hz, each one 150 Hz step below the signal bin above
		 * it, which is what makes the pair a distortion measurement
		 * and 0xa76c the denominator of the ratio just stored.
		 * Finding F739.
		 */
		dftnlinitNoiseBins(obj->nl_noise_bins);	/* 0x6a7aa */
	} else if (count >= 0x41 && count <= 0x17f) {
		/*
		 * 0x69995.  The same accumulation as 0x1c1..0x2ff, plus the
		 * receiver's own block counter -- the ONLY thing that
		 * separates the two windows.
		 */
		t72_update_both(obj, rx, n);
		/*
		 * 0x699fd.  `inc %ebp` then `mov %bp,0x124(%ecx)`, and the
		 * rung ends there: 0x69a04 reloads the txstate from +0x3596
		 * into %ecx and 0x69a0b jumps to the once-per-block dispatch
		 * at 0x62af1.
		 */
		rx->rx_blocks = (short)(rx->rx_blocks + 1);	/* 0x699fd */
	} else if (count == 0x40) {
		/*
		 * 0x6a4f4.  The gain the AGC settled on becomes the starting
		 * gain for what follows, doubled -- unless it is out of
		 * range, when 0x6aec1 substitutes 0x6000 and says so.  That
		 * path REJOINS at 0x6a52c, so the "beginning of RX_L1"
		 * message prints on both.
		 */
		/*
		 * 0x6a510, AND THE STORE PRECEDES THE BRANCH: `or
		 * $0x200,%ecx` at 0x6a506 and `cmp $0x34ff,%ax` at 0x6a50c
		 * both run before the store, and the `jg 6aec1` that picks
		 * the substituted gain is at 0x6a517 -- after it.  So the
		 * flag is raised on the out-of-range path too.
		 */
		rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);  /* 0x6a510 */

		if (rx->agc_gain > 0x34ff) {		/* 0x6a50c, signed */
			/*
			 * 0x71343 is out of line and it comes back: the
			 * string, `cwtl` so the gain reaches printf
			 * sign-extended, the call, a reload of
			 * `dsplibs_debug_level`, then `jmp 6aed0` -- the
			 * store below.
			 */
			if (DSPLIB_DEBUG_ON())		/* 0x71343 */
				dsplibs_debug_printf(
				    "V34AGC, -- ERROR-- gainestimate in " "RX_L1,0x%x\n", rx->agc_gain);
			/*
			 * 0x6aed9.  The `%edx` that 0x6a52c tests below is
			 * loaded at 0x6a51d on the in-range path and at
			 * 0x6aec1 on this one, reloaded at 0x71354 across
			 * the call -- three loads, and 0x6aec1's serves
			 * both tests.
			 */
			rx->agc_start_gain = 0x6000;		/* 0x6aed9 */
		} else {
			/*
			 * 0x6a525.  `add %eax,%eax` on the same register
			 * 0x6a50c compared, then a halfword store to
			 * receiver +0x262 -- so the doubled gain is
			 * truncated to sixteen bits, not saturated.
			 */
			rx->agc_start_gain = (short)(rx->agc_gain * 2);	/* 0x6a525 */
		}

		/*
		 * 0x6a531 is the body; the test is `cmp $0x1,%edx; jbe
		 * 6a54c` at 0x6a52c.  The argument is re-read `movswl
		 * 0x136(%edi),%esi` -- SIGNED, and forced, because the whole
		 * 32-bit value goes to a vararg.
		 */
		if (DSPLIB_DEBUG_ON())			/* 0x6a531 */
			dsplibs_debug_printf("V34AGC, rx->gain = 0x%x, at "
					     "the beginning of RX_L1\n",
					     rx->agc_gain);

		/*
		 * 0x6a54c.  The counter only counts up, so this rung runs
		 * FIRST: it arms both nl banks, 0x41..0x17f accumulates into
		 * them, 0x180 reduces them and arms them again, 0x300 does
		 * the same for the probe bank, and everything above 0x300 is
		 * the probe itself.  Finding F739.
		 */
		dftnlinitSignalBins(obj->probe_bins);	/* 0x6a54c */
		/*
		 * 0x6a5a1.  The other half, on obj + 0xa76c.  The two banks
		 * are told apart by their phase steps --
		 * 0x700/0x900/0xd00/0x1100 for the signal bank and
		 * 0x600/0x800/0xc00/0x1000 for this one, each 0x100 step
		 * being 150 Hz.  Finding F739.
		 */
		dftnlinitNoiseBins(obj->nl_noise_bins);	/* 0x6a5a1 */
	}
}

/*
 * The arm.
 */
static void
t72_rx_l1(struct v34_object *obj)
{
	struct v34_receiver *rx = T41_RX(obj);

	/*
	 * 0x650c6, and THE STORE PRECEDES THE BRANCH: `movzwl`, `inc`,
	 * `cmp $0x440,%di`, `mov %di,0xaa78`, `jle`.  So everything below
	 * sees the incremented value, which is what makes the counter
	 * pokeable in spite of finding F429 -- a poke of n drives the whole
	 * arm as n + 1.  The compare is signed and sixteen bits.
	 */
	obj->short_aa78 = (short)(obj->short_aa78 + 1);

	if (obj->short_aa78 <= 0x440) {
		t72_ladder(obj, rx);
		/*
		 * 0x65597.  The ladder's exit, one of the arm's seventeen
		 * `jmp 62af1`s: `mov 0xc0(%esp),%edi`, `movzwl
		 * 0x3596(%edi),%ecx`, jump.  The txstate is reloaded into
		 * %ecx first because the tail dispatches on whatever the
		 * jumping arm left there.
		 */
		t3c_txblock(obj);			/* 0x65597 */
		return;
	}

	/*
	 * 0x650ee.  The `jle 6551a` at 0x650e1 keeps this off the ladder
	 * path entirely, so the AGC runs only once the counter is past
	 * 0x440 -- which on the originating end is every block of the
	 * tone-A wait below, not one.  Its argument is `0x74(%esp)` --
	 * the receiver at obj + 0x264 -- pushed with no displacement.
	 */
	V34agc(rx);					/* 0x650ee */

	if (obj->role != 0x65) {
		/*
		 * 0x67d70 -- the ANSWERING end.  It finishes the probe, arms
		 * the FSK receiver for INFO1, aims the second record pointer
		 * at obj + 0xa9dc with INFO1c's length in it, opens the gate
		 * at +0xa8a0 that diverts 0x64a87, and restarts the counter.
		 */
		t72_probe_done(obj, rx);

		/*
		 * 0x67e1e.  `movzwl 0x3594(%ecx),%edx; cmp $0x2b,%dx; je
		 * 67e4b` -- and the `je` can never take, because the arm is
		 * entered with the rxstate at 72 and nothing between 0x650c6
		 * and here writes +0x3594.  The store and its trace always
		 * run.
		 */
		hs_setstate(obj, HS_RXSTATE, V34HS_RX_DPSK);	 /* 0x67e1e */
		/*
		 * 0x67e4b.  `cmp $0x3c,%dx`, and it is NOT one of the three
		 * the head of this section calls unreachable: those three
		 * compare +0x3594 and this one compares +0x3596.  An end
		 * already in TONE_AB stores nothing and prints nothing.
		 */
		hs_setstate(obj, HS_TXSTATE, V34HS_TONE_AB);	 /* 0x67e4b */
		/*
		 * 0x67e7f.  `cmp $0x29,%dx` on +0x3592: microstate 41, whose
		 * arm at 0x669a4 is the table-3 case at the bottom of this
		 * file.  This store is what sends the next block there.
		 */
		hs_setstate(obj, HS_MICROSTATE, V34HS_DET_SYNC); /* 0x67e7f */

		/*
		 * 0x67eb3.  Inlined, and it runs to 0x67f4d: `mov
		 * %bx,0xaae6(%esi,%eax,2)` at 0x67ebe is the head of the
		 * hundred-halfword clear, and the two stores at 0x67f46 and
		 * 0x67f4d -- +0xaae2 to all ones, +0xaae0 to zero -- are the
		 * last two of `fsk_state_init`'s eleven, not statements of
		 * this arm.
		 */
		dpskDetectInfo1Init(obj);			 /* 0x67eb3 */

		/*
		 * 0x67f54.  `movw $0x4d,0x18(%ebp)`, and %ebp already holds
		 * obj + 0xa9dc: the length field is filled in BEFORE 0x67f6e
		 * publishes the pointer to the record.  Microstate 41 puts
		 * INFO1a's 0x26 in the same field of the same record at
		 * 0x6b379 before aiming the same pointer at it -- which
		 * sequence the end expects is the whole difference.
		 */
		hs_put(obj, T72_BLK_A9DC + 0x18, 0x4d);		 /* 0x67f54 */
		obj->vect_idx = 0;				 /* 0x67f5a */
		/*
		 * 0x67f68.  `mov %ebx,0xa8a0(%esi)`, thirty-two bits.  This
		 * is the gate 0x64a87 tests, so from the next block on the
		 * RX_DPSK path polls the retrain detector as well -- and
		 * falls through to the FSK demodulator either way, which is
		 * finding F721.
		 */
		t3c_puti(obj, T72_FSKGATE, 1);			 /* 0x67f68 */
		t3c_putp(obj, T72_PTR_AA70,			 /* 0x67f6e */
			 (char *)obj + T72_BLK_A9DC);
		/*
		 * 0x67f74.  `mov %di,0xaa78(%esi)`, a halfword.  +0xaa78 is
		 * not this arm's private counter -- six arms bump it and it
		 * is the `[2]` every state trace prints -- so leaving it at
		 * 0x441 on the way out would be visible everywhere.
		 */
		obj->short_aa78 = 0;					 /* 0x67f74 */

		/*
		 * 0x67f7b.  The exit, and the %ecx it dispatches on was
		 * loaded from +0x3596 at 0x67f61 -- scheduled into the middle
		 * of the store run, three stores before the jump.
		 */
		t3c_txblock(obj);				 /* 0x67f7b */
		return;
	}

	if (obj->short_aa78 == 0x441) {
		/*
		 * 0x69611 -- the ORIGINATING end, on the one block after the
		 * probe.  Same finish, but it stays in RX_L1 and only clears
		 * the FSK shift register, because what it is waiting for is
		 * tone A and that is the test below.
		 */
		t72_probe_done(obj, rx);

		/*
		 * 0x696d0.  `cmp $0x3c,%cx; je 69702`: when the txstate is
		 * already 60 the store at 0x696f6 and the trace are both
		 * skipped -- and both paths leave 0x3c in %ecx, which is what
		 * the exit at 0x6971e dispatches on.
		 */
		hs_setstate(obj, HS_TXSTATE, V34HS_TONE_AB);	/* 0x696d0 */

		/*
		 * 0x69710.  `mov $0xffffffff,%ebp` and then a halfword store,
		 * so +0xaae2 gets 0xffff.  It is the shift register's reset
		 * value and this arm writes it at three sites -- 0x67f46,
		 * 0x68851 and here.
		 */
		obj->fsk.sr = -1;				/* 0x69710 */
		obj->fsk.nbits = 0;				/* 0x69717 */

		/* 0x6971e, and %ecx still holds TONE_AB from 0x696d0. */
		t3c_txblock(obj);				/* 0x6971e */
		return;
	}

	/*
	 * 0x65140, and the two guards below read what it writes:
	 * `cmpw $0x14,0xaae0` at 0x65145 and `testw $0xfff,0xaae2` at
	 * 0x65153, both sixteen bits, both on fields this call has
	 * just filled in.  No fixture can choose them by poking.
	 */
	fskdemodulate(obj,					/* 0x65140 */
		      (const short *)((const char *)rx + T72_RX_SAMPS),
		      &obj->fsk);

	if (obj->fsk.nbits <= 0x14 || (obj->fsk.sr & 0xfff) != 0) {
		/*
		 * 0x6881e.  Not enough bits, or the twelve are not all ZERO:
		 * no tone A this block, so all that is left is the timeout
		 * ladder.  The polarity is the object's and is worth stating,
		 * because this comment used to have it backwards: 0x65153 is
		 * `testw $0xfff,0xaae2(%esi)` with `jne 6881e`, so it is a
		 * NON-zero masked register that fails.  The register is reset
		 * to all ones at all three sites above, so twelve ZEROS
		 * shifting in is what tone A looks like here.  See the head of this section for why the 32-bit
		 * arithmetic here cannot be separated from a 16-bit spelling
		 * by any trial, and what is tested instead.
		 */
		int limit = obj->rtd >> 2;
		int count = obj->short_aa78;

		if (count > 0xf10 + limit) {
			/*
			 * 0x68851, AND THE STORE PRECEDES THE BRANCH: it
			 * sits between `cmp %ebx,%ecx` at 0x6884f and `je
			 * 6afd7` at 0x68858, so every block past 0xf10 +
			 * rtd/4 clears the shift register, not only the one
			 * that matches 0xf14.
			 */
			obj->fsk.sr = -1;		/* 0x68851 */

			if (count == 0xf14 + limit) {
				/*
				 * 0x6afe8 sits between the debug
				 * test at 0x6afe1 and its `ja
				 * 6b120` at 0x6afef -- the toggle
				 * happens whether or not the
				 * message prints.
				 */
				obj->short_358c ^= 1;	/* 0x6afe8 */
				/*
				 * 0x6b120 is out of line and does
				 * not come back: it prints,
				 * reloads +0x3596 into %ecx and
				 * jumps to 0x62af1 itself.
				 */
				if (DSPLIB_DEBUG_ON())	/* 0x6b120 */
					dsplibs_debug_printf(
					    "V34RETRAIN, waiting for tone A " "at the end of RX_L2\n");
			} else if (count == 0xf2c + limit) {
				/* 0x6c9f1: give up and go round again. */
				/*
				 * 0x6c9fa zeroes the counter
				 * before either transition below,
				 * and 0x6ca01's `cmp $0x2b` is one
				 * of the three that can never
				 * match.
				 */
				obj->short_aa78 = 0;		/* 0x6c9fa */
				hs_setstate(obj, HS_RXSTATE, V34HS_RX_DPSK);
				hs_setstate(obj, HS_MICROSTATE, V34HS_TX_L1);
			}
		}

		t3c_txblock(obj);
		return;
	}

	/*
	 * 0x65162.  Tone A: hand the receive machine to phase 2's DPSK and
	 * the microstate to the caller's phase-3 wait.
	 */
	/*
	 * 0x65162.  `cmp $0x2b,%dx` on +0x3594, and the `je 651e9`
	 * can never take -- the arm is entered with the rxstate at 72
	 * and nothing on the way here writes it -- so the store and
	 * its trace always run.
	 */
	hs_setstate(obj, HS_RXSTATE, V34HS_RX_DPSK);		/* 0x65162 */
	/*
	 * 0x651e9.  `cmp $0x3e,%dx` on +0x3592 -- 62 -- and it is
	 * also 0x6516d's `je` target, so the microstate transition is
	 * where the rxstate transition's "already there" exit lands.
	 */
	hs_setstate(obj, HS_MICROSTATE, V34HS_RX_PHASE3_CALL);	/* 0x651e9 */

	/*
	 * 0x65289.  The exit, and its %ecx was loaded from +0x3596 at
	 * 0x6527b -- after the trace call at 0x6526a and one
	 * instruction before the microstate store at 0x65282.
	 */
	t3c_txblock(obj);					/* 0x65289 */
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
	struct t3m_frame frame;
	const short *fskin;
	short rxst;

	t3m_frame_init(&frame, obj);

	/*
	 * 0x62933..0x629ed.  Table 1, the per-sample transmit loop.
	 *
	 * The cursor against the limit, both signed halfwords: 0x62933 is
	 * `cmp %dx,0x221c(%ebx)` then `jge 629ed`, so cursor >= limit skips
	 * the loop entirely and falls into the receiver's own test below.
	 * Below it is a DO-WHILE -- the per-sample dispatch at 0x62950,
	 * re-tested at 0x629e0 with `cmp %dx,(%eax); jl 62950` -- which falls
	 * out at 0x629ed, the same instruction the skip jumps to.
	 *
	 * BOTH OPERANDS ARE RE-READ EVERY PASS, which is why this is a
	 * `while` over two loads and not a loop over one cached limit.  The
	 * object caches the limit in `%dx` across the arm body, but `%dx` is
	 * caller-saved and every arm that calls anything would lose it, so
	 * each rejoin block reloads it first: 0x629cf, 0x62d70, 0x640a1 and
	 * 0x63948 are all `movzwl 0x2aa0(reg),%edx` immediately before the
	 * compare.  Measured over all four; finding F710.
	 *
	 * The dispatch is `.rodata+0x2da0` read at 0x62966, indexed
	 * `(short)txstate - 5` and range-checked `cmp $0x51,%eax; ja 629e0`,
	 * so 5..86 and nothing else.  Eighty-two entries, twenty distinct
	 * targets, all eighty-two carrying an `R_386_32` against `.text`:
	 * nineteen arms and the loop bottom itself, which fifty-seven entries
	 * hold.  Read out of the object, not off a doc line.
	 *
	 * THE DEFAULT DOES NOT TERMINATE, and that is the object's and not
	 * ours: an entry that is the loop bottom leaves the cursor where it
	 * was, so the test that sent us here is still true.  Fifty-seven of
	 * the eighty-two spin (finding F287, D59), and `v34hs_step` arms a
	 * SIGALRM so that is a named case rather than a run that never
	 * returns.
	 *
	 * The arms are `src/pump/v34/v34hstx1.cpp`, which is a `.cpp` because
	 * 78 and 85 tail-call into the V.90/V.92 C++ half (finding F344).
	 * Calling them from this `.c` is sound: the interop link carries
	 * `$(CXXOBJ64)` and this file already calls `V34SetINFO1aBits`.
	 * Finding F711.
	 */
	while (obj->txq.count < obj->tx_fill_target) {
		switch ((int)hs_get(obj, HS_TXSTATE)) {
		case V34HS_SILENCE:		/* 5  0x640b4, three tails */
		case V34HS_SILENCEINFO:		/* 54 */
		case V34HS_SILENCERETRAIN:	/* 74 */
			v34tx1_silence(obj);
			break;
		case V34HS_SSEG:		/* 18 0x64048 */
			v34tx1_sseg(obj);
			break;
		case V34HS_SBARSEG:		/* 19 0x6296d */
			v34tx1_sbarseg(obj);
			break;
		case V34HS_PPSEG:		/* 20 0x642bf */
			v34tx1_ppseg(obj);
			break;
		case V34HS_TRNSEG4:		/* 21 0x64339 */
			v34tx1_trnseg4(obj);
			break;
		case V34HS_TX_DPSK:		/* 24 0x62b96 */
			v34tx1_tx_dpsk(obj);
			break;
		case V34HS_TX_L1:		/* 51 0x62c69 */
			v34tx1_tx_l1(obj);
			break;
		case V34HS_TONE_AB:		/* 60 0x62d3d */
			v34tx1_tone_ab(obj);
			break;
		case V34HS_JTXMIT:		/* 64 0x635cc, one body two tails */
		case V34HS_J1TXMIT:		/* 68 */
			v34tx1_jtxmit(obj);
			break;
		case V34HS_XMIT0:		/* 65 0x62d83 */
			v34tx1_xmit0(obj);
			break;
		case V34HS_TRNSEG4A:		/* 66 0x62e28 */
			v34tx1_trnseg4a(obj);
			break;
		case V34HS_XMITMP:		/* 67 0x6399b */
			v34tx1_xmitmp(obj);
			break;
		case V34HS_EXMIT:		/* 69 0x63858 */
			v34tx1_exmit(obj);
			break;
		case V34HS_DATAXMIT:		/* 70 0x63ca8 */
			v34tx1_dataxmit(obj);
			break;
		case V34HS_TXLEVEL:		/* 71 0x641d1 */
			v34tx1_txlevel(obj);
			break;
		case V34HS_JaTXMIT:		/* 78 0x64139 */
			v34tx1_jatxmit(obj);
			break;
		case V34HS_MOH_SILENCE:		/* 81 0x63d58, one behaviour */
		case V34HS_MOH_ON_HOLD:		/* 82 */
		case V34HS_MOH_FRR:		/* 83 */
		case V34HS_MOH_CLEARDOWN:	/* 84 */
			v34tx1_moh_silence(obj);
			break;
		case V34HS_K56JaTXMIT:		/* 85 0x63fb0 */
			v34tx1_k56jatxmit(obj);
			break;
		case V34HS_TXMD:		/* 86 0x63dae */
			v34tx1_txmd(obj);
			break;
		default:
			/*
			 * 0x629e0, the loop bottom.  Fifty-seven of the
			 * eighty-two entries, plus every txstate the range
			 * test rejects.
			 */
			break;
		}

		/*
		 * AND NOTHING IS DISPATCHED ON THE RETURN VALUE ANY MORE.
		 * Two arms used to be able to leave for a block that was not
		 * reconstructed -- 81's wrap at 0x66d85 and 86's segment end
		 * at 0x66fe9 -- and said so in their return value, which this
		 * point tested and turned into `T3M_UNWRITTEN_TBL1`.  Both
		 * blocks are written now, in the arms, and neither turned out
		 * to be a transfer out of the loop at all: 0x66d85 ends at
		 * 0x63941 or 0x63948 and 0x66fe9 at 0x63e7f, which are the
		 * loop test and a fall-through inside 86's own body.  So the
		 * dispatch is gone, `enum v34tx1_exit` has one value left,
		 * and the guard has no call site.  Findings F748 and F750.
		 */
	}

	/*
	 * 0x629f1.  The receiver's first halfword, signed: `cmpw $0x5,(%ebx)`
	 * then `jle 62ae3`, and 0x62ae3 reads +0x3596 into %cx for the
	 * once-per-block dispatch at 0x62af1.  So five or fewer is the BLOCK
	 * route and not an idle return.  Finding F549.
	 */
	if (*(short *)frame.rx <= 5) {
		t3c_txblock(obj);
		return;
	}

	/*
	 * The compare chain at 0x62a02.  It has no table -- it is `cmp`/`je`
	 * against the rxstate, sign-extended out of +0x3594:
	 *
	 *   62a09  cmp $0x2b,%eax ; je 64a64    43 RX_DPSK, below
	 *   62a12  jg  62b71                    above 43, the SECOND chain
	 *   62a18  cmp $0x04,%eax ; je 653e4     4 RECEIVE
	 *   62a21  cmp $0x23,%eax ; je 6752c    35 WAIT
	 *   62a2a  (fall through)               the transmit dispatch
	 *
	 * and the second chain is three compares more:
	 *
	 *   62b71  cmp $0x35,%eax ; je 65473    53 DET_AB
	 *   62b7a  cmp $0x48,%eax ; je 650c6    72 RX_L1
	 *   62b83  (reload txstate)             the transmit dispatch
	 *
	 * THE ORDER IS THE OBJECT'S AND NOT A TIDYING.  `jg` before the
	 * compares against 4 and 35 means 53 and 72 are reached without ever
	 * being tested against those two, and a chain rewritten as one flat
	 * `switch` would compare the same values in a different order.  It
	 * would behave identically; it is written this way because this is
	 * what is there.
	 *
	 * TWO OF THE SIX EXITS NEED NO NEW CODE.  Both "anything else" arms
	 * are the once-per-block transmit dispatch at 0x62af1, which is
	 * `t3c_txblock` and has been written since table 2 landed -- so every
	 * rxstate below 43 except 4, and every rxstate above 43 except 53 and
	 * 72, is complete here rather than guarded.  That is most of the
	 * eighty-seven.  Finding F717.
	 */
	rxst = hs_get(obj, HS_RXSTATE);

	if (rxst != V34HS_RX_DPSK) {
		if (rxst > V34HS_RX_DPSK) {
			/*
			 * 0x62b71 `cmp $0x35,%eax` heads the above-DPSK
			 * half of the chain, and the clause that used to
			 * sit here -- "72 is the one that is not written"
			 * -- is stale: `t72_rx_l1` is defined just above
			 * and called below.
			 */
			/*
			 * 0x62b74's `je` target, and like 0x653e4 below it
			 * names the argument setup rather than the call:
			 * `mov 0x74(%esp),%esi; mov %esi,(%esp)` ahead of
			 * the `V34agc` at 0x6547a.
			 */
			if (rxst == V34HS_DET_AB) {
				t53_rx_det_ab(obj);	/* 0x65473 */
				return;
			}
			/*
			 * 0x62b7d's `je` target.  0x650c6 is the counter
			 * increment at the head of the arm: `movzwl
			 * 0xaa78`, `inc`, `cmp $0x440`, the store, then
			 * `jle 6551a`.
			 */
			if (rxst == V34HS_RX_L1) {
				t72_rx_l1(obj);		/* 0x650c6 */
				return;
			}
			/*
			 * 0x62b83.  `mov 0xc0(%esp),%esi; movzwl
			 * 0x3596(%esi),%ecx; jmp 62af1` -- the second
			 * chain's fall-through, reached by every rxstate
			 * above 43 that is not 53 or 72.  Finding F717.
			 */
			t3c_txblock(obj);		/* 0x62b83 */
			return;
		}
		/*
		 * 0x62a18's `je` target, and 0x653e4 is not the arm's first
		 * statement but the argument setup for it: `mov
		 * 0xc0(%esp),%ecx; mov %ecx,(%esp)` ahead of the `receiver`
		 * call at 0x653ee.
		 */
		if (rxst == V34HS_RECEIVE) {
			t4_rx_receive(obj);		/* 0x653e4 */
			return;
		}
		if (rxst == V34HS_WAIT) {
			/*
			 * 0x6752c, and it is four instructions: drain four
			 * entries off the receive queue and go to the
			 * transmit dispatch.  The queue is the receiver's
			 * first member, which is why the object passes
			 * `0x74(%esp)` -- obj + 0x264 -- straight to
			 * `rxreadqueue` with no displacement.
			 */
			rxreadqueue((struct v34_queue *)frame.rx);
			t3c_txblock(obj);		/* 0x67546 -> 0x62af1 */
			return;
		}
		/*
		 * 0x62a2a IS the dispatch rather than a jump to it: the same
		 * `movswl %cx,%eax; sub $0x5; cmp $0x45` that 0x62af1 has,
		 * `jbe 62b00` into table 2's indirect jump, and an
		 * out-of-range txstate falling into the tail at 0x62a40.
		 */
		t3c_txblock(obj);			/* 0x62a2a */
		return;
	}

	/*
	 * 0x64a64.  `V34agc` and `fskdemodulate` first, and only now is
	 * +0x3592 read: nothing on the way here writes any of the three state
	 * words, which is finding F285 and is what makes a poke-and-step
	 * fixture possible at all.
	 */
	V34agc(frame.rx);

	/*
	 * 0x64a81, and it is computed BEFORE the gate is tested: the `lea
	 * 0x10c(%ecx),%edi` sits between the load of +0xa8a0 and the `test`
	 * at 0x64a87, and the SAME `%edi` is handed to the retrain detector
	 * at 0x67551 and to `fskdemodulate` at 0x64aa9.  One pointer, two
	 * readers.
	 *
	 * IT IS `V34agc`'s OUTPUT, not the receive queue.  The call above
	 * writes the four samples at receiver + 0x10c (v34rx.h), so no
	 * fixture can choose them by poking -- they are produced by the
	 * neighbouring call on every step.  That is not finding F429's
	 * write-then-read case; the field is written by a NEIGHBOUR.
	 */
	fskin = (const short *)((unsigned char *)frame.rx + T3M_RX_FSKIN);

	/*
	 * 0x64a87, and 0x6754b: the FSK gate.
	 *
	 * `detectRetrainReq` INLINED, 1,089 bytes over five ranges --
	 * 0x6754b-0x6759e, 0x692ca-0x69611, 0x6aa85-0x6aad3,
	 * 0x6ca72-0x6ca93 and 0x6d2a6-0x6d2de.  It and `dftRetrainDetInit`
	 * are both `T` globals with out-of-line copies at 0x5e8f0 and
	 * 0x5ea60 and NO RELOCATION anywhere in the object, so every use is
	 * inlined; both are reconstructed above and both come out as calls
	 * here rather than a second copy of swept code.  Findings F719, F720.
	 *
	 * AND IT FALLS THROUGH.  Every one of the arm's six exits is `jmp
	 * 0x64a8f` -- 0x67599, 0x69327, 0x69368, 0x6960c, 0x6aace and
	 * 0x6d2d9 -- which is the instruction immediately below, the one the
	 * ungated path reaches too.  There is no `ret` in the arm.  So the
	 * gate is "poll the retrain detector as well", not "instead of":
	 * `fskdemodulate` and the microstate dispatch run either way, and on
	 * the fired path they run against the fields the action block has
	 * just reset.  Finding F721.
	 *
	 * The counter is `retrain_phase`, +0xa24c: `add $0x4` then `cmp
	 * $0x80` and `je`, EQUALITY and not `>=`, so its signedness is moot
	 * and a caller arriving out of step with four would never measure.
	 * Four is what the receive queue drains everywhere.
	 */
	/*
	 * The action block below ends with four halfword stores at
	 * 0x695f0..0x69605, all through one base: +0x2aa2, +0xaa78,
	 * +0xaae2 and +0xaae0 -- the vector index, the counter six
	 * arms share, the FSK shift register and its bit count.
	 * 0x6960c is the `jmp 64a8f` finding F721 describes, so this
	 * is a reset and not an exit.
	 */
	if (T3M_I32(&frame, T3M_FSKGATE) != 0
	    && detectRetrainReq(obj, V34_RETRAIN_BINS, fskin, 4)) {
		/*
		 * 0x6936e.  The action block: the far end asked for a
		 * retrain while this end was still looking for INFO1.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("DET_SYNC : retrain request "
					     "detected while searching for " "info1\n");

		T3M_I32(&frame, T3M_FSKGATE) = 0;	/* 0x6938c, 32-bit */

		/*
		 * 0x69392-0x69425, and it is `dftRetrainDetInit` inlined:
		 * thirteen store sites at identical offsets, widths and
		 * constants against the standalone copy at 0x5ea60 --
		 * +0x28/+0x2a/+0x00/+0x04/+0x08 over three bins at stride
		 * 0x2c, the three phase steps 0x600/0x800/0xa00, and the
		 * five scalars at +0xa24a.  +0xa24c is the only 32-bit store
		 * on either side.  Finding F720.
		 */
		dftRetrainDetInit(obj);

		/* 0x693fa/0x69411/0x69425, a 16-bit read-modify-write. */
		T3M_I16(&frame, T3M_F3588) =
			(short)(T3M_U16(&frame, T3M_F3588) | 2);

		/*
		 * Three transitions, 0x6941a, 0x694b6 and 0x69544, each the
		 * compare-print-store idiom with the second `%s` folded to
		 * its own `StateName` entry: .data+0x6cb8, +0x6cac and
		 * +0x6cf0 are indices 46, 43 and 60.
		 *
		 * THE MIDDLE ONE CAN NEVER FIRE.  0x64a64 is reached only
		 * through `cmp $0x2b,%eax; je 64a64` at 0x62a09 and 0x6754b
		 * only from inside that, so the rxstate is 43 on every path
		 * that gets here -- the object's `je 69536` at 0x694ba
		 * always takes and 0x694bc-0x69535 is dead in the blob too.
		 * It is written because it is what the object has.  Finding
		 * F722.
		 */
		hs_setstate(obj, HS_MICROSTATE, V34HS_TX_PHASE1_ANS);	/* 46 */
		hs_setstate(obj, HS_RXSTATE, V34HS_RX_DPSK);		/* 43 */
		hs_setstate(obj, HS_TXSTATE, V34HS_TONE_AB);		/* 60 */

		/* 0x695cb is `cmpw $0x1` then `jbe`, so UNSIGNED. */
		if (T3M_U16(&frame, T3M_TOGGLE) > 1)
			T3M_U16(&frame, T3M_TOGGLE) = 0;

		/* 0x695f0..0x69605, in the object's order. */
		obj->vect_idx = 0;			/* +0x2aa2 */
		T3M_U16(&frame, T3M_COUNTER) = 0;	/* +0xaa78 */
		obj->fsk.sr = -1;			/* +0xaae2 */
		obj->fsk.nbits = 0;			/* +0xaae0 */
	}

	/*
	 * 0x64a9e, and it has to be read HERE: `fskdemodulate` writes it --
	 * and so does the action block above, which zeroes it.  Both orders
	 * are observable and this is the object's.
	 */
	frame.nbits = obj->fsk.nbits;

	fskdemodulate(obj, fskin, &obj->fsk);

	frame.mst = hs_get(obj, HS_MICROSTATE);

	if ((unsigned)((int)frame.mst - T3M_TBL3_FIRST) >= T3M_TBL3_COUNT) {
		/*
		 * 0x65329.  `mov 0xc0(%esp),%ebx; movzwl 0x3596(%ebx),%ecx;
		 * jmp 62af1`: a microstate outside table 3's range leaves
		 * through the same transmit dispatch as everything else here.
		 */
		t3c_txblock(obj);			/* 0x65329 */
		return;
	}

	/*
	 * .rodata+0x3000, read at 0x64ad2.  FLATTENED ON PURPOSE: the nine
	 * arms that came from `v34hshak_t3mid.c` were behind a nested
	 * `t3m_table3(&frame)`, and `tools/anchorcheck.py`'s Rule 1 reads a
	 * `case V34HS_*:` label straight to the function it calls -- with the
	 * nesting in place all nine microstates map to `t3m_table3`, which
	 * owns no microstate, and 455 anchors' worth of coverage evaporates
	 * without printing anything.  It is also what the object does.
	 */
	switch ((int)frame.mst) {
	case V34HS_DET_SYNC:		/* 41, 0x669a4 */
		t41_micro_det_sync(obj);
		return;
	case V34HS_DET_INFO:
		t44_micro_det_info(obj);
		return;
	case V34HS_TX_PHASE2_ANS:	/* 47, and 56 is the same address */
	case V34HS_TX_PHASE2_CALL:	/* 56 */
		t3m_micro47(&frame);
		return;
	case V34HS_TX_PHASE3_ANS:	/* 48 */
		t3m_micro48(&frame);
		return;
	case V34HS_RX_PHASE1_ANS:	/* 49 */
		t3m_micro49(&frame);
		return;
	case V34HS_RX_PHASE2_ANS:	/* 50 */
		t3m_micro50(&frame);
		return;
	case V34HS_TX_L1:		/* 51 */
		t3m_micro51(&frame);
		return;
	case V34HS_TX_PHASE1_CALL:	/* 55 */
		t3m_micro55(&frame);
		return;
	case V34HS_RX_PHASE1_CALL:	/* 58 */
		t3m_micro58(&frame);
		return;
	case V34HS_RX_PHASE2_CALL:	/* 59 */
		t3m_micro59(&frame);
		return;
	case V34HS_RX_PHASE3_CALL:
		t3c_micro_rx_phase3_call(obj);
		return;
	case V34HS_INFODONE:		/* 63 */
		t3m_micro63(&frame);
		return;
	case V34HS_MOH_TONE:
		t3c_micro_moh_tone(obj);
		return;
	case V34HS_MOH_TONE_DROP:
		t3c_micro_moh_tone_drop(obj);
		return;
	case V34HS_TX_PHASE1_ANS:	/* 46, 0x65d6d */
		t46_micro_tx_phase1_ans(obj);
		return;

	/*
	 * The arm twenty-four of the forty states share, 0x6590b: read the
	 * transmit state and jump to the once-per-block dispatch.  These are
	 * .rodata+0x3000's twenty-four entries holding 0x6590b, not a guess
	 * at which states "do nothing" -- and the range test above is the
	 * same three instructions at a different address.
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
		/*
		 * Unreachable: the fifteen written arms and the twenty-four
		 * shared ones are forty labels over the forty values the
		 * range test admits.  It stays because the range test and the
		 * label set are two statements of one fact and a mutation to
		 * either has to land somewhere.
		 */
		t3c_unwritten();
		return;
	}
}

/*
 * ---------------------------------------------------------------------------
 * `datapumpv34`, 0x71960..0x71d64 -- the datapump's per-block entry point,
 * and the last function of V34hshak.c.
 *
 * WHY IT IS IN THIS FILE.  It is the code between `v34handshak`, which ends
 * at 0x71955, and `V34InitializeImplementationSpecific` at 0x71d70, which
 * belongs to v34filters.c because it initialises that file's own object;
 * this one calls `v34handshak`, `v34handshakinit`, `modulatevector` and
 * `receiver` and none of those is v34filters.c's.  Finding F98 drew that
 * conclusion before either function was written and this is where it lands.
 *
 * THREE THINGS HAPPEN HERE, and which of them happens is decided by the int
 * at +0x2218 -- the same word `t3c_block_tail` above reports on and
 * `VPcmV34InitiateRateRenegotiation` sets to 5:
 *
 *   > 1     the handshake.  Call `v34handshak` until the transmit block has
 *           been filled AND the receive queue has been drained, then return.
 *           NOTHING ELSE runs on this path: no modulator, no receiver, and
 *           none of the supervision below.
 *
 *   <= 1    data.  Fill the transmit block a vector at a time, drain the
 *           receive queue a burst at a time, and then supervise: retrain or
 *           renegotiate if the receiver's error measure has been bad, or
 *           good, for long enough.
 *
 * THE SUPERVISOR IS FOUR INDEPENDENT `if`s AND NOT A CHAIN.  Every one of
 * the four falls into the next -- the retrain's tail re-reads +0x122 and
 * rejoins at the second test, the remote-renegotiation block rejoins at the
 * third, the step down rejoins at the fourth -- so a single block can
 * retrain, notice a remote renegotiation and start one of its own, calling
 * `v34handshakinit` three times.  Each block reads `+0xaa96` and the
 * receiver's counters AFTER its `v34handshakinit` call, which is why the
 * reads below are inside the blocks rather than hoisted: `v34handshakinit`
 * is free to move them and the object gives it the chance.
 *
 * THE THREE COUNTERS are the receiver's +0x258, +0x25a and +0x25c, and they
 * are consecutive-run counts rather than totals: each is bumped when this
 * block's error measure at +0x21a fails its own threshold and RESET TO ZERO
 * when it passes.  +0x258 has no timer condition, +0x25a starts once the
 * span at +0x238 has run 144,000 past the mark at +0x248, and +0x25c once it
 * has run 1,152,000 -- so the two renegotiation counters cannot fire in the
 * first seconds of a connection whatever the line does.  +0x25c's compare is
 * the other way round: it counts symbols whose error is SMALL, which is what
 * makes it the step UP.
 *
 * The three timer spans -- 287,488 here, 144,000 and 1,152,000 in the loop --
 * are all UNSIGNED compares of one int minus another, so a mark ahead of the
 * count reads as an enormous span rather than a negative one.
 */

#define DP_TIMER	0x0238	/* int:   the running sample count           */
#define DP_TIMER_MARK	0x0248	/* int:   the instant a span is measured from */
#define DP_MODE		0x2218	/* int:   T3C_MODE, handshake above 1        */
#define DP_FAA98	0xaa98	/* short: copied into the receiver's +0x260  */

/*
 * The receiver's own fields.  THESE ARE ALL MEMBERS OF `struct v34_receiver`
 * ALREADY -- `rx_blocks`, `equerr`, `bad_thresh`, `bad_long_thresh`, `good_thresh`, `bad_run`, `bad_long_run`, `good_run`,
 * `tx_sample`, `echo_residual` and `flags`, at exactly these offsets, in
 * `include/dsplib/v34recv.h`.  This comment used to say none of them was
 * mapped, which is how the offset-passing accessors below survived a reading
 * of this function's codegen.
 *
 * THE ACCESSORS ARE A KNOWN DEFECT AND FINDING F8044 IS THE WRITE-UP.  The
 * object holds `&obj->rxq` and `&obj->txq` in registers -- `lea 0x264(%ebx),
 * %esi` and `lea 0x221c(%ebx),%edi` -- and addresses every field as a
 * constant displacement off them.  Because `off` here is a PARAMETER, ours
 * loads fold but the STORES do not, and `dp_run`'s `int failed` parameter
 * materialises a boolean where the object branches.  Rewriting through a
 * local `struct v34_receiver *rx` takes this from 40 differing bytes to 34
 * with every extension site right, and was DECLINED under F7782 because no
 * cell in a fourteen-cell enumeration maps onto the object -- see F8045,
 * which also records that the two byte-CLOSEST cells compute something the
 * object does not.
 */
#define DP_RX_ERR	0x021a	/* short: the block's error measure          */
#define DP_RX_BLOCKS	0x0124	/* short: blocks received, capped at 30,000  */
#define DP_RX_THR_A	0x0252	/* short: +0x258's threshold                 */
#define DP_RX_THR_B	0x0254	/* short: +0x25a's                          */
#define DP_RX_THR_C	0x0256	/* short: +0x25c's                          */
#define DP_RX_BAD	0x0258	/* short: consecutive blocks over THR_A      */
#define DP_RX_BAD_LONG	0x025a	/* short: consecutive blocks over THR_B      */
#define DP_RX_GOOD	0x025c	/* short: consecutive blocks under THR_C     */
#define DP_RX_WHY	0x025e	/* short: 1 remote, 2 down, 3 up            */
#define DP_RX_RATE	0x0260	/* short: the rate index the change was at   */

#define DP_TIMER_STALE	288000u		/* 0x46500, 36 s at 8 kHz  */
#define DP_TIMER_MID	144000u		/* 0x23280, 18 s           */
#define DP_TIMER_LONG	1152000u	/* 0x119400, 144 s         */
#define DP_RX_BLOCK_CAP	0x752f		/* the last value that still bumps  */

static short
dp_rxget(const struct v34_object *obj, unsigned off)
{
	return *(const short *)((const char *)&obj->rxq + off);
}

static void
dp_rxput(struct v34_object *obj, unsigned off, short v)
{
	*(short *)((char *)&obj->rxq + off) = v;
}

/*
 * One consecutive-run counter: bumped while the measure fails, cleared the
 * moment it passes.  All three sites are this shape and the only difference
 * between them is which way the compare runs, so the caller passes the
 * verdict.  Sixteen-bit, and it wraps.
 */
static void
dp_run(struct v34_object *obj, unsigned counter, int failed)
{
	dp_rxput(obj, counter,
		 failed ? (short)(dp_rxget(obj, counter) + 1) : 0);
}

void
datapumpv34(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if ((unsigned)t3c_geti(obj, DP_TIMER)
	    - (unsigned)t3c_geti(obj, DP_TIMER_MARK) > DP_TIMER_STALE)
		obj->progress = 5;

	/*
	 * The handshake, and the only path that calls `v34handshak`.  Its own
	 * prologue reads the same two guards, so an iteration whose arm moves
	 * neither the cursor nor the queue count repeats forever; that is the
	 * object's shape and not a reading of it, and it is why the test can
	 * drive this branch only with the loop already satisfied.
	 */
	if ((unsigned)t3c_geti(obj, DP_MODE) > 1u) {
		while (obj->txq.count < obj->tx_fill_target || obj->rxq.count > 5)
			v34handshak(obj);
		return;
	}

	while (obj->txq.count < obj->tx_fill_target)
		modulatevector(obj);

	while (obj->rxq.count > 5) {
		unsigned span;
		short err;

		if (dp_rxget(obj, DP_RX_BLOCKS) <= DP_RX_BLOCK_CAP)
			dp_rxput(obj, DP_RX_BLOCKS,
				 (short)(dp_rxget(obj, DP_RX_BLOCKS) + 1));

		receiver(obj);

		err = dp_rxget(obj, DP_RX_ERR);
		dp_run(obj, DP_RX_BAD, err > dp_rxget(obj, DP_RX_THR_A));

		span = (unsigned)t3c_geti(obj, DP_TIMER)
		     - (unsigned)t3c_geti(obj, DP_TIMER_MARK);

		if (span > DP_TIMER_MID)
			dp_run(obj, DP_RX_BAD_LONG,
			       err > dp_rxget(obj, DP_RX_THR_B));
		if (span > DP_TIMER_LONG)
			dp_run(obj, DP_RX_GOOD,
			       err < dp_rxget(obj, DP_RX_THR_C));
	}

	/*
	 * Retrain: either the receiver asked with bit 6 of +0x122, or the
	 * plain bad-block run has passed half the block rate at +0xaa96.
	 * The mode it leaves behind distinguishes the two AFTER the fact --
	 * the same test again, so a `v34handshakinit` that cleared the run
	 * would report 3 where the entry condition was the flag.
	 */
	if ((T3C_RX(obj)->flags & V34_RX_FLAG_RETRAIN)
	    || dp_rxget(obj, DP_RX_BAD) > (short)(obj->baud_rate >> 1)) {
		v34handshakinit(obj, 1);
		t3c_puti(obj, DP_MODE,
			 dp_rxget(obj, DP_RX_BAD) <= (short)(obj->baud_rate >> 1)
			 ? 3 : 2);
		dp_rxput(obj, DP_RX_BAD, 0);
		dp_rxput(obj, DP_RX_BAD_LONG, 0);
		dp_rxput(obj, DP_RX_GOOD, 0);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34TIMING,Retrain started\n");
	}

	/* The far end asked, on bit 5 of the same word, re-read. */
	if (T3C_RX(obj)->flags & V34_RX_FLAG_RENEG) {
		v34handshakinit(obj, 3);
		dp_rxput(obj, DP_RX_GOOD, 0);
		t3c_puti(obj, DP_MODE, 4);
		dp_rxput(obj, DP_RX_BAD, 0);
		dp_rxput(obj, DP_RX_BAD_LONG, 0);
		dp_rxput(obj, DP_RX_WHY, 1);
		dp_rxput(obj, DP_RX_RATE, hs_get(obj, DP_FAA98));
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"V34RNEG, rate renegotiation detected \n");
		VPcmV34IndicateRemoteRRN(obj);
	}

	/*
	 * Down, then up, and both compares are 32-bit: the counter is
	 * sign-extended and the block rate multiplied as an int, so a large
	 * +0xaa96 does not wrap the threshold into range.
	 */
	if (dp_rxget(obj, DP_RX_BAD_LONG) > 2 * (int)obj->baud_rate) {
		v34handshakinit(obj, 2);
		t3c_puti(obj, DP_MODE, 5);
		dp_rxput(obj, DP_RX_BAD, 0);
		dp_rxput(obj, DP_RX_BAD_LONG, 0);
		dp_rxput(obj, DP_RX_GOOD, 0);
		dp_rxput(obj, DP_RX_WHY, 2);
		dp_rxput(obj, DP_RX_RATE, hs_get(obj, DP_FAA98));
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34RNEG, rate renegotiation "
					     "DOWN initiated due to large " "error \n");
		VPcmV34IndicateLocalRRN(obj);
	}

	if (dp_rxget(obj, DP_RX_GOOD) > 8 * (int)obj->baud_rate) {
		v34handshakinit(obj, 2);
		dp_rxput(obj, DP_RX_GOOD, 0);
		t3c_puti(obj, DP_MODE, 5);
		dp_rxput(obj, DP_RX_BAD, 0);
		dp_rxput(obj, DP_RX_BAD_LONG, 0);
		dp_rxput(obj, DP_RX_WHY, 3);
		dp_rxput(obj, DP_RX_RATE, hs_get(obj, DP_FAA98));
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34RNEG, rate renegotiation "
					     "UP initiated due to small " "error \n");
		VPcmV34IndicateLocalRRN(obj);
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

V34HS_OFF(out_count,    struct v34_receiver, out_count,       0x128);
V34HS_OFF(flags,   struct v34_receiver, flags,      0x122);
V34HS_OFF(gain,    struct v34_receiver, agc_gain,   0x136);
V34HS_OFF(step,    struct v34_receiver, agc_step,   0x13a);
V34HS_OFF(rms,     struct v34_receiver, rms_buf,    0x13c);
V34HS_OFF(rms_idx, struct v34_receiver, rms_idx,    0x19c);
V34HS_OFF(retrain_gate, struct v34_receiver, retrain_gate, 0x19e);
V34HS_OFF(phase_frac,    struct v34_receiver, phase_frac,       0x1ac);
V34HS_OFF(phase_inc,    struct v34_receiver, phase_inc,       0x1ae);
V34HS_OFF(phase_wrap,    struct v34_receiver, phase_wrap,       0x1b0);
V34HS_OFF(carrier, struct v34_receiver, carrier,    0x1b4);
V34HS_OFF(half_len,    struct v34_receiver, half_len,       0x1ba);
V34HS_OFF(symbol_period,    struct v34_receiver, symbol_period,       0x1be);
V34HS_OFF(agc_start_gain,    struct v34_receiver, agc_start_gain,       0x262);
V34HS_OFF(fir_coeff,    struct v34_receiver, fir_coeff,       0x2a4);

/*
 * The clear runs to +0xab00 + 100*2 - (0xab00 - 0xaae6) = 0xabae, which is
 * inside the object.  Asserted so that a struct which grew a member into
 * that pad would break here rather than have the clear silently start
 * overwriting it.
 */
typedef char v34hs_clear_fits[
	((0xaae6 + 100 * 2) <= (int)sizeof(struct v34_object)) ? 1 : -1];

V34HS_OFF(tx_flags,   struct v34_object,   tx_flags,      0x25c2);
V34HS_OFF(prev_quadrant,   struct v34_object,   prev_quadrant,      0x25c6);
V34HS_OFF(cur_quadrant,   struct v34_object,   cur_quadrant,      0x25c8);
V34HS_OFF(tx_scr_sr,   struct v34_object,   tx_scr_sr,      0x25cc);
V34HS_OFF(txpoint, struct v34_object,   txpoint,    0x25d0);
V34HS_OFF(txpim,   struct v34_object,   txpoint.c[1], 0x25d2);

#endif
