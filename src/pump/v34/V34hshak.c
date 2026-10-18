/*
 * V34hshak.c -- V.34 handshake, training support and datapump supervision.
 *
 * This file implements v34handshak, its state-name table and training
 * helpers, and datapumpv34, which invokes the handshake or supervises the
 * trained receive/transmit paths. v34hshak.h names the three cooperating
 * state machines; v34fsk.h and v34recv.h own their recovered object maps.
 *
 * The reference's local ApplyBulkDelay/getbit symbols and STT_FILE record
 * anchor the original V34hshak.c region (F117). v34handshak ends at
 * 0x71955 and datapumpv34 starts at 0x71960. These addresses describe the
 * reference object, not this build's current function placement.
 *
 * Historical fast-pass scope: the first version of this file implemented
 * fourteen support functions and StateName while the handshake was still
 * pending. That restriction is retired; the functions below are now
 * reconstructed. F221 established that file-local reference functions can
 * be globalised and renamed for direct tests, and F227 applies that method
 * to getbit and ApplyBulkDelay. F212 records the no-internal-caller audit
 * of several exported training helpers; F89 explains why those helpers
 * still need independently established component input boundaries.
 */

#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
#include "dsplib/v34det.h"
#include "dsplib/v34digital.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include <string.h>

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
/* `.rodata` 16 bytes at the object's 0x2a42, between c1680 and c1600.  Its
 * 1646 Hz midpoint places it on the same two-hertz-pole design as its
 * siblings and the columns F620 settled; the bytes carry no relocation. */
const short c1646[V34_CARRIER_DESC]  = { 0, 0, 0, 0, -15249, 15735, -15175, 15735 };
const short c1600[V34_CARRIER_DESC]  = { 0, 0, 0, 0, -16093, 15735, -16020, 15735 };

/*
 * Two 2-byte `.rodata` scalars the object defines in this file's V.34
 * `.rodata` band (dataaudit bracket: between V34RX.c's `edelay.2` and
 * v34filters.c's `tx600c1`, alongside this file's c-tables, scale tables,
 * probe and vectpp globals).  Neither is referenced by any relocation in the
 * object, so the bytes ARE the initialisers: 64 and 3.
 */
const short l2thresh = 64;
const short echoshift = 3;

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
 * v21_hibnd/v21_lobnd -- two more writable 60-tap symmetric filters, `.data`
 * 120 bytes each in the object's V.34-family `.data` band, adjacent to
 * `bpv22high`/`bpv22low` and `costbl`/`intcoef*`.
 *
 * THEY ARE NOT THIS TREE'S `temp_v21_hibnd`/`temp_v21_lobnd`.  That pair is
 * `static const short[61]` in V8Fsk.c, emitted to `.rodata` (122 bytes), and
 * the OBJECT DEFINES BOTH ITS OWN `temp_v21_*` (.rodata 122) AND THESE
 * (.data 120).  So F11445's "ours as temp_v21_*" reading was the coincidence
 * it suspected: same family, different symbols, different size.  Neither of
 * these has a relocation anywhere in the object, so the object's bytes are
 * the initialiser and they are transcribed exactly.
 */
short v21_hibnd[60] = {
	   16,     2,    -1,     8,   -11,   -47,   -11,    96,
	   94,   -90,  -200,   -11,   218,   128,  -100,   -84,
	   -1,  -180,  -197,   415,   800,  -172, -1464,  -764,
	 1544,  2021,  -630, -2777,  -981,  2392,  2392,  -981,
	-2777,  -630,  2021,  1544,  -764, -1464,  -172,   800,
	  415,  -197,  -180,    -1,   -84,  -100,   128,   218,
	  -11,  -200,   -90,    94,    96,   -11,   -47,   -11,
	    8,    -1,     2,    16
};

short v21_lobnd[60] = {
	   23,    13,    -2,   -12,   -12,    -4,     1,   -14,
	  -45,   -66,   -33,    74,   212,   279,   171,  -126,
	 -482,  -665,  -477,    80,   750,  1134,   922,   128,
	 -867, -1500, -1358,  -453,   751,  1589,  1589,   751,
	 -453, -1358, -1500,  -867,   128,   922,  1134,   750,
	   80,  -477,  -665,  -482,  -126,   171,   279,   212,
	   74,   -33,   -66,   -45,   -14,     1,    -4,   -12,
	  -12,    -2,    13,    23
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
	short i;

	for (i = 0; i <= 0x63; i++)
		obj->fsk_interp[i] = 0;
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
	obj->fsk.phase = 0;
	obj->fsk.prev = 0;
	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;
	obj->fsk.bit_lo = 1;
	obj->fsk.bit_hi = 0;
	obj->fsk.next = 6;
	obj->fsk.bit_len = 0xc;
	obj->fsk.resync_next = 6;
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
	struct v34_receiver *rx = &obj->receiver;
	short carrier = (mode == 0) ? 1200 : 2400;
	int i;

	fsk_clear(obj);

	V34SetupModulator(v34_object_modulator(obj),
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
	struct v34_receiver *rx = &obj->receiver;
	unsigned char *m = (unsigned char *)obj;
	struct v34_modulator *mod =
		v34_object_modulator(obj);
	int originate;
	int i;

	/*
	 * Three records of 44 bytes at +0xa81c, ending exactly where the
	 * int at +0xa8a0 and then `info0_bits` begin.  Each gets the same
	 * two constants and its own value at +2: 0x600, 0x800, 0xa00.
	 */
	for (i = 0; i < 3; i++) {
		struct v34_dftbin *r = &obj->retrain_bins[i];

		r->thresh_lo = 0x50;
		r->thresh_hi = 3000;
		r->phase = 0;
		r->acc_re = 0;
		r->acc_im = 0;
	}
	obj->retrain_bins[0].inc = 0x600;
	obj->retrain_bins[1].inc = 0x800;
	obj->retrain_bins[2].inc = 0xa00;

	obj->retrain_state = 1;
	obj->retrain_phase = 0;
	obj->retrain_runs = 0;
	obj->retrain_quiet_runs = 3;
	obj->retrain_tone_runs = 9;
	obj->retrain_poll = 0;

	rx->rrn_local_dir = 0;
	rx->baud_copy = 0;
	obj->is_short = 0;
	obj->tx_flags = V34_EC_FROZEN;
	obj->echo_calls = 0;
	obj->short_3588 = 0;
	obj->short_358a = 0;
	obj->short_aa78 = 0;
	obj->tx_fill_target = 6;
	obj->vect_idx = 0;
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

	detectorinit(&obj->detector,
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
	short *tx_baud    = &obj->ratecfg.baud;
	short *tx_preemp  = &obj->ratecfg.preemp;
	const short **tx_scale = &obj->ratecfg.divtab;
	short *tx_carrier = &obj->ratecfg.carrier;
	short *rx_baud    = &obj->ratecfg.rx_baud;
	short *rx_carrier = &obj->ratecfg.rx_carrier;
	const short **rx_scale = &obj->ratecfg.rx_divtab;
	const short **rx_cdesc = &obj->det_coefs;
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
	struct v34_receiver *rx = &obj->receiver;
	unsigned char *m = (unsigned char *)obj;
	short baud = obj->ratecfg.rx_baud;
	short carrier = obj->ratecfg.rx_carrier;
	const short *cdesc = obj->det_coefs;

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

	detectorinit(&obj->detector,
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
	 * The descending comparison chain and absence of default initializers
	 * reproduce the object's period code.  The five rates below are every
	 * rate it handles.  An unsupported rate leaves x and ratio unset;
	 * its result depends on incoming machine registers, not a defined C
	 * contract (D37).  Do not invent a default value for the reconstruction.
	 * See docs/v34-small-rtl.md for the crossed dispatch/default controls.
	 */

	if (baudrate == 3429) {
		x = preemp_get(p, PREEMP_M3429);
		ratio = 0x6626;
	} else if (baudrate == 3200) {
		x = preemp_get(p, PREEMP_M3200);
		ratio = 0x639f;
	} else if (baudrate == 3000) {
		x = preemp_get(p, PREEMP_M3000);
		ratio = 0x656f;
	} else if (baudrate == 2800) {
		x = preemp_get(p, PREEMP_M2400);
		ratio = 0x6789;
	} else if (baudrate == 2400) {
		x = preemp_get(p, PREEMP_M2400);
		ratio = 0x7da7;
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

	/* The blob promotes this signed state for its two dispatch comparisons.
	 * An ordinary switch reproduces that lowering on period GCC; see
	 * docs/v34-small-rtl.md, retrain-state dispatch. */
	switch (obj->retrain_state) {
	case 1:
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
			obj->retrain_runs = (short)(obj->retrain_runs + 1);
		} else {
			if (obj->retrain_runs >= obj->retrain_quiet_runs)
				obj->retrain_state = 2;
			obj->retrain_runs = 0;
		}
		return 0;
	case 2:
		break;
	default:
		return 0;
	}

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
 *
 * The words themselves are `struct v34_object`'s `microstate`, `rxstate` and
 * `txstate` members (v34fsk.h), and the setters below bind each one at
 * compile time; v34hshak.h's `V34HS_*_OFF` remain for the readers that
 * reach the words by offset.
 */

/*
 * The two counters every trace prints as `[1]` and `[2]`.  `[1]` is the
 * second short of the pair at +0x2aa0, whose first `v34modeminit` sets to 6
 * and this function's tail sets to 0x10.
 *
 * Struct-anchored (issue #260 wave 11): the offset is the member's own, so
 * byte accesses use the same offset as the declared member.
 */
#define HS_TRACE_2	((int)__builtin_offsetof(struct v34_object, \
							short_aa78))

/*
 * Both raw state accessors are retired. Issue #260 wave 11 removed hs_put;
 * MG-03 replaces hs_get's last three reads with obj->ratecfg.rxbits, the
 * same signed-short field. That bounded substitution leaves the complete
 * period object unchanged. The separate dp_rxget/dp_rxput supervisor
 * rewrite remains subject to the F8044/F8045 evidence limits; see
 * docs/maintainer-offset-access.md.
 */

/*
 * One state transition, with its diagnostic -- one setter per machine.
 *
 * All thirteen sites are this idiom -- compare, print, assign -- and each
 * prints its own change plus the other two machines' current values.  The
 * three setters bind their word at compile time (issue #260 wave 6): the
 * struct member IS the word, so there is no offset parameter left to hand
 * to the wrong machine and the trace cannot be separated from its store.
 *
 * THE TWO CONTEXT SLOTS ARE NOT INTERCHANGEABLE and the order below is the
 * object's: "rxstate" prints (tx, mst), "txstate" prints (rx, mst) and
 * "microstate" prints (tx, rx).  Passing the right strings in the wrong
 * order leaves every byte of the object identical, which is why the fixture
 * sweeps the three words to three DIFFERENT values and not to one.
 *
 * THE FORMAT IS A LITERAL PER SETTER, NOT A `static const char *const
 * fmt[3]` TABLE.  THE OBJECT HAS NO FORMAT TABLE: all 153 `R_386_32`
 * references to the three strings are from `.text`, none from `.rodata` or
 * `.data`, and its `v34handshak` carries 140 of them as the inlined copies
 * of these setters.  GCC 3.4.2 does NOT constant-fold a table index into a
 * direct string reference, so the table form emitted the indices and
 * referenced the strings through a `.rodata` table instead -- 0 direct
 * references where the object has 140 (finding F11509).  With a literal in
 * each setter, the inlined and constant-folded copies reference the string
 * directly, as the object does.  Do not restore a table here.
 *
 * The object's equality test uses the halfword before extending it for
 * signed StateName indexing on the debug path.  Compare and read the
 * member directly: GCC 3.4.2 promotes a signed read before the equality
 * test, and a debug-local read lets GCC retain the first halfword and
 * extend it only where the index is needed.  The signed field and the
 * signed debug index remain unchanged (see docs/v34-small-rtl.md).
 *
 * `static inline`: the object inlines the whole closure into
 * `v34handshak` and defines no `hs_*` symbol at all
 * (finding F11506/F11507).
 */
static inline void
hs_set_microstate(struct v34_object *obj, short next)
{
	if (obj->microstate == next)
		return;

	if (DSPLIB_DEBUG_ON()) {
		short now = obj->microstate;
		const char *ctx1 = StateName[obj->txstate];
		const char *ctx2 = StateName[obj->rxstate];

		dsplibs_debug_printf("V34HSHAKE: microstate %s=>%s(tx %s, rx %s, [1]%ld, [2]%ld)\n",
				     StateName[now], StateName[next],
				     ctx1, ctx2,
				     (long)obj->vect_idx,
				     (long)obj->short_aa78);
	}

	obj->microstate = next;
}

static inline void
hs_set_rxstate(struct v34_object *obj, short next)
{
	if (obj->rxstate == next)
		return;

	if (DSPLIB_DEBUG_ON()) {
		short now = obj->rxstate;
		const char *ctx1 = StateName[obj->txstate];
		const char *ctx2 = StateName[obj->microstate];

		dsplibs_debug_printf("V34HSHAKE: rxstate %s=>%s(tx %s, mst %s, [1]%ld, [2]%ld)\n",
				     StateName[now], StateName[next],
				     ctx1, ctx2,
				     (long)obj->vect_idx,
				     (long)obj->short_aa78);
	}

	obj->rxstate = next;
}

static inline void
hs_set_txstate(struct v34_object *obj, short next)
{
	if (obj->txstate == next)
		return;

	if (DSPLIB_DEBUG_ON()) {
		short now = obj->txstate;
		const char *ctx1 = StateName[obj->rxstate];
		const char *ctx2 = StateName[obj->microstate];

		dsplibs_debug_printf("V34HSHAKE: txstate %s=>%s(rx %s, mst %s, [1]%ld, [2]%ld)\n",
				     StateName[now], StateName[next],
				     ctx1, ctx2,
				     (long)obj->vect_idx,
				     (long)obj->short_aa78);
	}

	obj->txstate = next;
}

/*
 * Bit 4 of `tx_flags`, established as `PROG_TXBIT_DATA` beside its other
 * reader in v34pcmmain.cpp and catalogued in v34fsk.h's own comment on the
 * field; re-declared here, file-local, for this file's own reader below.
 */
#define PROG_TXBIT_DATA		0x10

/*
 * +0xaa78, the counter three of these six read.  V34hshak.c calls it
 * `HS_TRACE_2` because every handshake diagnostic prints it as `[2]`;
 * `v34handshakinit` zeroes it and the arms here count it DOWN to zero (78,
 * 85) or count `vect_idx` UP to it (86).  V34hshak.c calls the same member
 * `T3M_COUNTER`; both names are its own offset now (issue #260 wave 11).
 */
#define TX1_COUNT	((int)__builtin_offsetof(struct v34_object, \
							short_aa78))

/*
 * +0x358c.  60 masks it with one to choose between two of `vect4`'s four
 * points.  The member's own offset (issue #260 wave 11); the only other
 * site in the tree is V34hshak.c:1480, where `v34handshakinit` clears it.
 */
#define TX1_F358C	((int)__builtin_offsetof(struct v34_object, \
							short_358c))

/*
 * +0x2218, an int.  Table 2's tail reads it to choose four of its arms
 * (v34hstxblock.c's `TB_F2218`); 70 is a writer of it.
 *
 * Named `hs_mode` in `struct v34_object` (`v34fsk.h`), on `V34hshak.c`'s own
 * `DP_MODE`/`T3C_MODE` names for the same int: `datapumpv34`'s recovery
 * supervisor reads it as "handshake above 1", and 70 setting it to 1 here is
 * exactly that transition.  Kept as a raw-offset macro in this file rather
 * than reached as `o->hs_mode`, matching every other field here that a
 * second file also reaches by offset (`v34hstxblock.c`'s own `TB_F2218`) --
 * see `TX1_FAAE0`/`TX1_FAA3C` above for why that convention is deliberate.
 * The VALUE is the member's own offset now (issue #260 wave 11).
 */
#define TX1_HS_MODE	((int)__builtin_offsetof(struct v34_object, \
							hs_mode))

/*
 * +0xaa7a, +0xaa7c and +0xaa86, the three words beside the counter these
 * arms touch.  5/54/74 clears +0xaa7a before it writes the queue; 20 adds
 * +0xaa7c into the counter and accumulates +0xaa86.  Each is the member's
 * own offset (issue #260 wave 11): +0xaa7a is `short_aa7a`, +0xaa7c the
 * object's own `filtdelay`, and +0xaa86 the rate config's `period`.
 */
#define TX1_FAA7A	((int)__builtin_offsetof(struct v34_object, \
							short_aa7a))
#define TX1_FAA7C	((int)__builtin_offsetof(struct v34_object, \
							filtdelay))
#define TX1_FAA86	((int)__builtin_offsetof(struct v34_object, ratecfg) \
			 + (int)__builtin_offsetof(struct v34_ratecfg, period))

/*
 * +0xaae0 and +0xaae2.  `struct v34_object` names them `fsk.nbits` and
 * `fsk.sr` because the FSK demodulator reaches them that way; the handshake
 * uses them as two words of its own -- V34hshak.c's table-3 core reaches them
 * by offset as `T3C_FAAE0` and `T3C_FAAE2` for the same reason, and 74's
 * retrain writes -1 and 0 into them.  Reached by offset here so that the
 * name of the other reader is not asserted to be the meaning here; the
 * VALUE is the member's own offset (issue #260 wave 11).
 */
#define TX1_FAAE0	((int)__builtin_offsetof(struct v34_object, fsk) \
			 + (int)__builtin_offsetof(struct v34_fsk, nbits))
#define TX1_FAAE2	((int)__builtin_offsetof(struct v34_object, fsk) \
			 + (int)__builtin_offsetof(struct v34_fsk, sr))

/*
 * +0xabe8, a BYTE.  `v34handshakinit`'s Modem-on-Hold bring-up sets it to one
 * (V34hshak.c:1441) and 74's retrain tests it to choose which microstate the
 * handshake restarts in.  The object reads it with `cmpb`.
 *
 * Named `moh_active` in `struct v34_object` (`v34fsk.h`) -- kept as a
 * raw-offset macro here rather than `o->moh_active`, matching this file's own
 * convention for fields also reached elsewhere by offset.  The VALUE is the
 * member's own offset (issue #260 wave 11).
 */
#define TX1_MOH_ACTIVE	((int)__builtin_offsetof(struct v34_object, \
							moh_active))

/*
 * +0x25d6, +0x25d8 and +0x25da -- the three halfwords of `unmapped_25d6` that
 * 64/68 owns, reached off `0x4c(%esp)` as 0x3ba, 0x3bc and 0x3be.
 *
 * +0x25d6 is the BIT SOURCE: 64/68 shifts it right by twice `vect_idx` and
 * hands the bottom two bits to the scrambler, so it is sixteen bits read as
 * eight dibits, and the end of 64's segment reloads it with 0x899f.  +0x25d8
 * counts one per pass and ends the segment past 0x80.  +0x25da is a mode
 * word: 64 goes no further unless it holds 2, and the `role == 0x66` path
 * declines to move on when it holds 0.  No other site in this tree reads any
 * of the three.
 */
#define TX1_F25D6	0x25d6
#define TX1_F25D8	0x25d8
#define TX1_F25DA	0x25da

/*
 * +0x35a2 and +0x382 are members since issue #260 wave 9 (`md_length` on the
 * object; the receiver's `short_382`, the Indicate word whose low half the
 * TRN2 shift register also runs through) -- the sites below read them as
 * members now.  64/68 test `md_length` against zero on their `role == 0x66`
 * path and 69 tests `short_382` against 0x89b0 to choose between four points
 * and sixteen.
 */

/*
 * +0x3590, +0x3598 and +0x359e -- three halfwords 67 owns, and no other site
 * in this tree reads any of them.
 *
 * +0x3590 is the NEXT STUFF POINT: 67 compares `vect_idx` against it and, when
 * they meet, pushes one more bit into the reader and advances it by 0x11.
 * +0x359e counts the sequences 67 has sent, and +0x3598 is the once-only flag
 * that stops `initdigital` being called twice.  All three are inside
 * `unmapped_3564` and `unmapped_359e`.
 */
#define TX1_F3590	((int)__builtin_offsetof(struct v34_object, \
							short_3590))
#define TX1_F3598	((int)__builtin_offsetof(struct v34_object, \
							short_3598))
#define TX1_F359E	((int)__builtin_offsetof(struct v34_object, \
							short_359e))

/*
 * +0xaa3c, read as a BYTE.  `struct v34_object` names the halfword there
 * `info_caps` because the handshake reads the INFO capability nibbles out of
 * it, and v34hshak.h records that `getMPrecvdBits` stores obj+0xaa3c into
 * +0xaa6c -- so in the object's own configuration this byte and the reader's
 * `word[0]` are the same two bytes, and 0x30 bytes of record fit exactly
 * between +0xaa3c and the self-pointer at +0xaa6c.
 *
 * 67 READS BOTH SPELLINGS AND THEY ARE NOT THE SAME EXPRESSION.  0x645de
 * tests bit 0 at obj+0xaa3c; 0x647db tests bit 0 of `*(+0xaa6c)` and 0x647ef
 * sets it there.  Reached by offset here rather than as `info_caps` so that
 * the other reader's name is not asserted to be the meaning, exactly as
 * TX1_FAAE0 and TX1_FAAE2 are; the VALUE is the member's own offset
 * (issue #260 wave 11).
 */
#define TX1_FAA3C	((int)__builtin_offsetof(struct v34_object, \
							info_caps))

/*
 * +0xa244, an INT, and +0xaa80, a halfword.
 *
 * 21 reads +0xa244 three ways in one compare chain -- against its half, its
 * whole and nothing else -- and no other site in this tree reads it.  It is
 * `phase3_hd_length` now (issue #260 wave 11), and the low half its chain
 * tests is that int's low halfword, so the half reads stay helper reads of
 * the same anchored offset.  +0xaa80 is `rate_change_pending`: 21 stores one
 * into it and nothing here reads it back.
 */
#define TX1_FA244	((int)__builtin_offsetof(struct v34_object, \
						phase3_hd_length))
#define TX1_FAA80	((int)__builtin_offsetof(struct v34_object, \
						rate_change_pending))

/*
 * +0xaa0c, `msgrec[4]` of the one array that runs from +0xa94c to +0xaa3c
 * (issue #260 wave 8; V34hshak.c:1450 blanks the same record and its own
 * comment records that record 4 is msgrec[4] -- the "second of the five"
 * this comment used to say was this define's arithmetic error, caught by
 * the t_v34hstx1 fixture the moment the offset was spelled through the
 * member in issue #260 wave 11 and corrected here).  Its first halfword
 * carried the object's `info_rates` reading, and the MP transmitter builds
 * its message into it.  Reached by offset here for the reason TX1_FAAE0
 * and TX1_FAA3C are; the VALUE is the record's own offset.
 */
#define TX1_INFOREC	((int)__builtin_offsetof(struct v34_object, \
						msgrec[4]))

/*
 * +0xaa6c and +0xa94c: the self-pointer and the record it is aimed at.
 * `v34handshakinit` aims the same pair on its mode-4 path (V34hshak.c:1451),
 * and 54's completion aims it again before handing the record to
 * `V34SetINFO0aBits`.  The harness excludes +0xaa6c from the byte comparison
 * and checks it by offset instead (finding F324).  Both are members of
 * `struct v34_object` now (issue #260): `paa6c` and `msgrec`.
 */

/*
 * +0xaa98, a short: the negotiated rate INDEX rather than a bit rate --
 * `VPcmV34GetCurrentRxBitRate` multiplies the same units by 2400.  70 copies
 * it, sign-extended, into `rate_now` and `rate_want`; v34fsk.h names those
 * two off this site and off 0x63395's read-back.  The offset is the rate
 * config's `rxbits` member (issue #260 wave 11).
 */
#define TX1_RATEIDX	((int)__builtin_offsetof(struct v34_object, \
						ratecfg.rxbits))

/*
 * +0xaa88, a short: the TRANSMIT side's rate index, the other half of the
 * pair 70's data-mode transition reports.  It is `struct v34_ratecfg`'s
 * `txbits` member -- V34_RATECFG is +0xaa84 in v34fsk.h, so +4 -- and
 * `VPcmV34GetCurrentTxBitRate` reads the same field, `movswl 0xaa88; imul
 * $0x960`, on its way out.  The data-mode diagnostic at 0x63d1a reads it and
 * TX1_RATEIDX together and multiplies both by 2400; the format string calls
 * the first `Tx bit rate` and the second `Rx bit Rate`, which is the order
 * the object passes them and not a transcription slip.  The offset is the
 * member's own (issue #260 wave 11).
 */
#define TX1_TXRATEIDX	((int)__builtin_offsetof(struct v34_object, \
						ratecfg.txbits))

/*
 * The transmitted point.  `txpoint` is two shorts and every arm that sends a
 * constellation point writes them with ONE 32-bit store, which is what
 * `vect4` holds -- v34pcmmain.cpp and v34k56.cpp spell it the same way.
 *
 * This used to be a `memcpy`, which was a workaround for the C front end
 * warning about `*(int *)&o->txpoint` (then still bare, `f25d0`) where the
 * C++ one did not.  The
 * declaration carries it now: `txpoint` is a union, so the wide store has a
 * member of its own and there is nothing left to work around.
 */
static void
tx1_put_point(struct v34_object *o, int point)
{
	o->txpoint.word = point;
}

static short
tx1_get(const void *objp, unsigned off)
{
	return *(const short *)((const char *)objp + off);
}

static void
tx1_put(void *objp, unsigned off, short v)
{
	*(short *)((char *)objp + off) = v;
}

/*
 * `tx1_get_int`/`tx1_put_int` are GONE (issue #260 wave 11): their sites --
 * `hs_mode`, `phase3_hd_length`, `msg_acc`, `msg_acc0` -- became member
 * reads and stores, and no caller is left.
 */

/*
 * ---------------------------------------------------------------------------
 * The scrambler step 71 and 86 share, 0x641de..0x6424e and 0x63db5..0x63e2e.
 *
 * Both arms carry the loop twice -- once with the 0x04000000 tap and once
 * with 0x00002000 -- and choose between the copies with `role == 0x65`.
 * That is `V34scrambler`'s `mode` argument exactly: V34RX.c hoists the same
 * branch out of the same loop, over the same register at +0x25cc, with the
 * same two generators.  `bits` is the literal 3 and `nbits` is 2, so two
 * scrambled bits come out and the register advances twice.
 *
 * WHY THE RETURN VALUE IS USABLE THOUGH THE OBJECT DOES NOT MASK.  The arms
 * store `reg >> 29` into `cur_quadrant` unmasked where `V34scrambler` returns
 * `(reg >> 29) & 3`.  The two agree for every input: the loop's last act is
 * `reg >>= 1`, so bit 31 is clear on exit and `reg >> 29` is already 0..3.
 * The masked form is used here because it is the published function; nothing
 * downstream can tell them apart.
 */
static short
tx1_scramble2(struct v34_object *o)
{
	return (short)V34scrambler((unsigned *)&o->tx_scr_sr,
				   (short)(o->role == 0x65), 3, 2);
}

/*
 * ---------------------------------------------------------------------------
 * 78 `JaTXMIT`, 0x64139, and 85 `K56JaTXMIT`, 0x63fb0.
 *
 * Count +0xaa78 down.  On the step that reaches zero -- not before, and not
 * again afterwards, because zero is also the "do nothing" test at the top --
 * raise bit 2 of `tx_flags` and report both echo cancellers' coefficients.
 * Then hand the sample itself to the phase 3/4 half, which is where the
 * transmitting is done: neither arm calls `txmit`.
 *
 * The two differ in that call and in nothing else.  They are written as one
 * body and two entry points so that the identity is a property of the file
 * rather than a claim in a comment; finding F340 is the measurement that they
 * are still two behaviours, because the two callees are.
 *
 * AND IN THE MESSAGE, which is a third caller's evidence rather than a
 * qualification of that.  The completion block prints one line naming the
 * txstate it completed in -- `JaTXMIT`, `K56JaTXMIT` and `J1TXMIT` at
 * 0x6821d, 0x68282 and 0x682d6 -- and 64 shares this countdown too
 * (0x635cc, whose completion at 0x6533c falls back into the body).  Three
 * identical blocks with one string each is what a caller-supplied message
 * looks like from the outside, so it is passed rather than branched on.
 *
 * THE COUNTER IS SIXTEEN BITS.  The object loads it with `movzwl`, tests
 * `%ax`, and stores `%ax` back, so a negative `short` counts down through
 * 0x8000 rather than through zero.  Spelled with an `unsigned short` here
 * for that reason.
 */
static int
tx1_ja_common(struct v34_object *o, const char *msg)
{
	unsigned short c = (unsigned short)o->short_aa78;

	if (c == 0)
		return 0;

	c = (unsigned short)(c - 1);
	o->short_aa78 = (short)c;
	if (c != 0)
		return 0;

	/*
	 * The WHOLE message, not a name spliced into one format: the three
	 * sites hold three complete literals in `.rodata.str1.4` and push one
	 * argument each, which is what constant propagation into an inlined
	 * static leaves behind and a `%s` would not.
	 */
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf(msg);
	v34FreezeEcho(o);
	return 1;
}

/*
 * ---------------------------------------------------------------------------
 * The differential quadrant step 69 and 64/68 share, 0x638eb..0x6390d and
 * 0x63697..0x636b9.
 *
 * The scrambler's two bits are not transmitted directly: they are ADDED to
 * the quadrant last sent, modulo four, and the sum is both the point's index
 * and the new "last".  `cur_quadrant` is the quadrant chosen and `prev_quadrant` the one
 * carried forward, and the object writes them with two stores of the same
 * register either side of the table load.
 *
 * The two sites are instruction for instruction the same -- the same three
 * fields, the same mask, the same order -- so this is one static rather than
 * two copies, for the reason 78 and 85 share `tx1_ja_common`.  `prev_quadrant` is
 * read with `movzwl` and the sum is masked to two bits, so a fixture value
 * outside 0..3 cannot change the answer.
 */
static short
tx1_dpsk4(struct v34_object *o, short q)
{
	short k = (short)((q + (unsigned short)o->prev_quadrant) & 3);
	int point = vect4[k];

	o->cur_quadrant = k;
	o->prev_quadrant = k;
	tx1_put_point(o, point);
	return k;
}

/*
 * The message reader, at whatever +0xaa6c holds.  The object re-reads the
 * pointer at every site (0x639e2, 0x645ab, 0x64757, 0x6472d, 0x647d5,
 * 0x6462f) and nothing in this arm writes it, so the reads all give the same
 * record; they are written out one per site anyway, because that is the
 * object's shape and the equivalence stops being true the moment something
 * aims it.
 */

/*
 * 0x64635 -- re-arm the reader for the next sequence, and the counters with
 * it.  `nbits` is the ONE field bit 5 of the flags word chooses: 0x30 with
 * the bit set and 0x90 with it clear, computed in the object as
 * `sbb`/`and $0x60`/`add $0x30` rather than branched (0x64656..0x6466c).
 *
 * `repeat` goes to zero, so the reader will return -1 rather than restart
 * once this message runs out; `acc0`/`avail0` are still loaded, which is what
 * makes that a decision of the caller's rather than of the reader's.
 */

/*
 * 0x645d0 -- the sequence is over.  Count it in +0x359e, and then one of
 * three things.  Returns non-zero when the arm LEAVES the bit loop, which is
 * the only exit that is not "collect the next bit".
 *
 * THE FIRST TEST IS AT obj+0xaa3c AND NOT AT THE RECORD, and the two are the
 * same halfword only because `getMPrecvdBits` aims +0xaa6c at +0xaa3c.  The
 * object reads the object-relative one here (0x645de) and the record-relative
 * one twelve instructions later (0x647db), so a reconstruction using one for
 * both is wrong wherever the pointer is aimed anywhere else.
 *
 * WHERE THE THREE GO:
 *
 *   +0xaa3c bit 0 set, flags & 0x90 == 0x90, and this the FOURTH sequence
 *              or later: `initdigital` once (guarded by +0x3598), the
 *              transmit machine to 69 EXMIT, `vect_idx` cleared and bit 5 of
 *              the flags word dropped.  0x648a7 and 0x648bf, and it is the
 *              one path that leaves the loop;
 *   flags & 0x18 == 0x10 and +0x359e past one:
 *              raise bit 0 of the record's own first halfword, and clear
 *              +0x359e if it was NOT already up.  0x647be and 0x647d3;
 *   otherwise  straight to the reload.
 *
 * THE COMPARE AT 0x648ab IS SIGNED (`jle`) and so is 0x647cd's, which is why
 * both are spelled through `short`.
 */

/*
 * 0x64929 -- two bits, one differential quadrant, one `vect4` point.  This is
 * 69's four-point half with the scrambler's `bits` coming out of `cur_quadrant`
 * instead of a literal three, so `tx1_dpsk4` carries the rest of it.
 */

/*
 * 0x63b62 -- four bits, two scrambler steps, one `vect16` point.  Instruction
 * for instruction this is 69's sixteen-point block at 0x67031 with two
 * differences and no others: the two `bits` arguments are the low and the high
 * dibit of `cur_quadrant` where 69 passes the literals 0xf and 3, and this one does
 * not advance `vect_idx` (the bit loop already did, once per bit).  It is
 * written out rather than shared with 69 because sharing would have to rewrite
 * 69's body, and the mutations already aimed at that text are what say 69 is
 * right.
 *
 * `cur_quadrant` IS READ SIGNED (`movswl`, 0x63b83) AND IT CANNOT MATTER.  0x639ab
 * clears the field before the loop and the loop only ORs bits in, so the two
 * readings can differ only when bit 15 is set -- which needs the reader's
 * exhausted arm, whose -1 fills every bit.  Even then the scrambler consumes
 * bits 0 and 1 alone, and after the arithmetic `sar $2` bits 2 and 3, and all
 * four are ones under either reading.  So it is a proof and not a run.
 *
 * The second call takes the SAVED halfword shifted down by two, not the value
 * `cur_quadrant` now holds: 0x63c03 restores the register before 0x63c12 shifts it,
 * and 0x63c05 has already overwritten the field with the first quadrant.
 */

/*
 * ---------------------------------------------------------------------------
 * 24 `TX_DPSK`, 0x62b96 -- one bit of a message as one tone, and the whole of
 * Modem-on-Hold's clear-down when the message runs out.
 *
 * THE 2,220 BYTES ARE `getbit` INLINED TWICE, which is finding F424's result
 * at 67 for a second time and is why this arm is short.  0x62bb5..0x62c64
 * with 0x649f1, 0x654a7, 0x67b95, 0x68480 and 0x6876a is the reader
 * V34hshak.c:2513 carries, arm for arm and field for field: the "still have
 * bits" short cut at 0x62bc9, the whole-word refill at 0x654a7, the PART-word
 * refill at 0x62bfc that does not advance `idx`, the CRC-16 fold at 0x62c35,
 * the flush at 0x67ba8, the "overrun by exactly sixteen" arm at 0x6876a, and
 * the exhausted-with-`repeat` restart at 0x68490 -- which does not inline
 * either: it CALLS `getbit` at 0x684bb for the first bit of the repeat, the
 * same recursion 0x5ec47 makes.  0x67c4e..0x67d04 with 0x6887f, 0x69165,
 * 0x69a49, 0x6a842, 0x6a94b, 0x6c996, 0x6c9be and 0x70683 is the SAME reader
 * a second time, over a record the arm has just re-armed by hand.
 *
 * So the arm is two calls, the way 78 and 85 are written over
 * `tx1_ja_common` and 67 over `getbit`.  What it costs, named rather than
 * implied, is finding F424's cost unchanged: `tools/mutate.py` anchors on
 * source text and the reader's text is in `V34hshak.c`, which is
 * `t_v34hshak.c`'s suite.  What the runs here test is that each call is the
 * right one on the right record and every branch the arm takes around them.
 *
 * ---------------------------------------------------------------------------
 * THE SHAPE.  One bit, one tone, and three ways for the message to end:
 *
 *     if (moh_active)  vect_idx += 1        the Modem-on-Hold clock
 *     bit = getbit(*(obj + 0xaa6c))
 *     if (bit >= 0)  {  short_358c ^= bit; vect4[2 * (short_358c & 1)]; txmit;  }
 *     else if (!moh_active)      txstate = 60 TONE_AB and nothing else
 *     else if (!moh_msg_pending)      re-arm the reader BY HAND, take one more bit,
 *                           send it, and fall into the hold tail
 *     else                  the moh_message dispatch, then the hold tail
 *
 * THE TONE IS 60 `TONE_AB`'s, THE SAME TABLE ENTRY SCALED THE SAME WAY --
 * 0x64a35 here and 0x62d52 there, `mov 0x0(,%reg,8)` over four-byte entries,
 * so it selects `vect4[0]` or `vect4[2]`, the two ends of a diagonal.  What
 * 24 adds is that the message bit is XORed INTO +0x358c first, so the tone
 * alternates on a one and holds on a zero.  That is differential phase-shift
 * keying at one bit a symbol, which is the state's name.
 *
 * FINDING F323'S ONE COLLISION IS THIS, AND IT IS NOT IDENTITY.  Entered cold
 * 24 and 60 write the same 69 bytes with the same signature, and the reason
 * is narrow: the fixture's fill leaves +0xabe8 clear, so no `vect_idx` tick;
 * the reader hands back a ZERO bit, so the store into +0x358c writes the
 * value already there and the tone selected is the one 60 selects; and the
 * reader's own advance lands OUTSIDE the object, where finding F323's byte
 * count does not look.  Any one of those three moving separates them, and
 * `t_v34hstx1.c` moves all three.  They are two entries at two addresses --
 * 0x62b96 against 0x62d3d, 2,220 bytes against thirty-four -- and the file
 * asserts that too.
 *
 * ---------------------------------------------------------------------------
 * THE HOLD TAIL, 0x64fec, is where both Modem-on-Hold paths end, and its
 * first test is a TIME rather than a state: `vect_idx` against
 * `(rtd >> 4) + 1200`, both sign-extended from shorts and the shift
 * arithmetic.  Below it the arm simply leaves; at or above it the hold is
 * over and one of three things happens.
 *
 * THE FOURTEEN BLOCKS F341 FLAGGED ARE NOW WRITTEN (F11502, F11508).  They
 * are entered only when `dsplibs_debug_level > 1` -- 0x68704, 0x68b12,
 * 0x6c771, 0x6c7db, 0x6a87e, 0x6b06c, 0x6c760 and the `cmpl $0x1` sites that
 * guard them.  The seven that name a string live as `v34tx1_tx_dpsk`'s own
 * "End of current MOH msg" print, the `MOH_ILLEGAL` message and the setter;
 * the rest are the setters' three formats.  The object-first count that
 * retired the note is F11508.
 *
 * AND ONE PATH THAT WRITES ONE BYTE.  `moh_message` OUTSIDE 0..3 makes the
 * dispatch do NOTHING but clear its own one-shot at +0xabf8, and below the
 * threshold the hold tail leaves at 0x6431f -- so that run neither transmits
 * nor moves a state word, and `t_v34hstx1.c` guards it on the one-shot
 * instead.  It has to be that run and not one past the threshold: it is the
 * only place a clear-down that should not have happened is visible, because
 * BOTH of the tail's ways out leave exactly what a spurious clear-down leaves
 * -- the retrain's own `v34handshakinit` clears +0xabe4 (V34hshak.c:1307) and
 * rewrites both state words, and the tail's clear-down IS the clear-down.
 */

/* +0xabe4 and +0xabe6, `short_abe4`/`short_abe6` (issue #260 wave 9); the
 * offsets are the members' own now (wave 11).  The clear-down raises
 * +0xabe4 and the two `v34handshakinit` paths raise +0xabe6; no other site
 * in this tree reads either. */
#define TX1_FABE4	((int)__builtin_offsetof(struct v34_object, \
							short_abe4))
#define TX1_FABE6	((int)__builtin_offsetof(struct v34_object, \
							short_abe6))

/*
 * +0xabf8 and +0xabf9, two BYTES of `unmapped_abf8`, read with `cmpb`.
 * Named `moh_msg_pending` and `moh_path_sel` in `struct v34_object`
 * (`v34fsk.h`) -- kept as raw-offset macros here, matching this file's own
 * convention for fields also reached elsewhere by offset.
 *
 * +0xabf8 is a one-shot: while it is up the arm runs the message dispatch and
 * clears it, and once it is down the arm re-arms the reader instead.  +0xabf9
 * chooses twice -- which message is built at 0x6a3f3, and which of the tail's
 * three ways out is taken at 0x65012.  Neither has another reader here.
 */
/* now the member's own offset (issue #260). */
#define TX1_MOH_MSG_PENDING \
	((int)__builtin_offsetof(struct v34_object, moh_msg_pending))
/* now the member's own offset (issue #260). */
#define TX1_MOH_PATH_SEL \
	((int)__builtin_offsetof(struct v34_object, moh_path_sel))

/*
 * 0x64a13 and 0x67d07, the same nine instructions twice.  The message bit is
 * XORed into +0x358c as a 32-bit value and the field stored back as sixteen,
 * then bit 0 of it selects `vect4[0]` or `vect4[2]`.
 *
 * THE BIT REACHES THE XOR SIGN-EXTENDED (`cwtl` at 0x64a05 and 0x67d0e), so
 * the reader's -1 COMPLEMENTS the whole halfword rather than flipping its low
 * bit.  Only the low bit is then read, so the tone is the same either way and
 * the field is not: a run with an exhausted reader is what makes that
 * visible, and `t_v34hstx1.c` has one.
 */

/*
 * 0x64eaf, 0x65046 and 0x688b4 -- the clear-down, written three times in the
 * object and once here.  All three are the same three compare-and-store
 * pairs in the same order, and the third adds one store of its own.
 *
 * THE txstate COMPARE AT 0x64eb6 CANNOT BE FALSE, for finding F342's reason at
 * 65: that site is reached only from the dispatch at `txstate == 24` and
 * nothing between them writes +0x3596.  The OTHER TWO CAN, because the hold
 * tail runs after the dispatch has already moved the machine -- which is why
 * this is one function and not one inlined body.
 */

/*
 * The two 0x69041 sites share one literal; it is named so that the
 * sharing is visible in the source rather than only in the object.
 */
#define MOH_ILLEGAL	"MOH: Illegal MH sequence under MHreq,"	\
			" initiating retrain\r\n"

/*
 * 0x689f6 -- put the modem on hold: MOH_SILENCE, WAIT, and the two counters
 * the silence arm at 0x63d58 then runs on.  `t_v34hstx1.c` drives it through
 * both of its two entries, `moh_message == 1` and `moh_recvd` at 0 or 4.
 */

/*
 * 0x69041 and 0x6923d -- the retrain entry, twice over and identical.
 *
 * THE MESSAGE IS THE CALLER'S, for tx1_ja_common's reason: 0x69041 and
 * 0x69227 are two guards in front of two copies of these two stores, and
 * the only difference between the copies is which literal they push.  The
 * dispatch's TWO calls share one copy because they share one message.
 *
 * `v34handshakinit(obj, 1)` RE-ARMS THE LOOP THAT IS DISPATCHING THIS ARM:
 * `v34modeminit` sets the block's sample limit at +0x2aa0 to six and clears
 * `vect_idx`, and it moves the transmit machine to SILENCERETRAIN, so the
 * pass after this one is 5/54/74's four silent samples and the loop then ends
 * on its own.  Nothing here depends on that; it is recorded because a callee
 * rewriting the bound of the loop that called it is not what anybody expects
 * to find.
 */

/*
 * 0x6a3cf -- build the next Modem-on-Hold message and install a fresh reader
 * for it.
 *
 * THE CALL TAKES THE OLD RECORD AND THE STORE INSTALLS THE NEW ONE.  0x6a42e
 * reads +0xaa6c before the call and 0x6a448 writes it after, so
 * `VPcmV34SetMohMessageBits` fills `word[0]` of WHATEVER THE READER WAS
 * POINTING AT and the twelve stores below re-arm the record at +0xa94c.  In
 * the object's own configuration those are the same record --
 * `v34handshakinit`'s mode 4 aims +0xaa6c at +0xa94c (V34hshak.c:1451) -- so
 * only a poke separates them, and `t_v34hstx1.c` aims the pointer elsewhere
 * for exactly one run.
 *
 * The twelve stores are `v34handshakinit`'s mode-4 record, value for value:
 * an eight-bit message with an eight-bit word, its CRC armed, twelve bits of
 * 0xf72 in hand and no repeat.  They are written in the object's order here,
 * which is not that one's.
 */

/*
 * 0x64fec -- the hold tail, which both Modem-on-Hold paths fall into.
 *
 * `rtd` is a short and reaches the compare through `movswl` and an ARITHMETIC
 * `sar $0x4` (0x64ff3, 0x65001), so a negative round-trip delay pulls the
 * threshold below zero rather than above four hundred million.  That is the
 * one reading a positive `rtd` cannot tell apart, and `t_v34hstx1.c` drives
 * it negative.
 *
 * The three ways out are tested in this order and no other: +0xabf9, then
 * `moh_message` inside 2..3 as ONE unsigned compare of `moh_message - 2`
 * against one (0x65025), then everything else.
 */

/*
 * ---------------------------------------------------------------------------
 * 66 `TRNSEG4A`, 0x62e28, with the sixteen-point half at 0x66e59 and the
 * segment's completion at 0x62f22.
 *
 * THE SYMBOL IS 71's AND 86's, NOT 69's.  The generator is chosen on
 * `role == 0x65` -- the object carries the scrambler loop twice, at 0x62e70
 * with the 0x04000000 tap and at 0x6358c with 0x00002000, and picks between
 * the copies exactly as 71 and 86 do -- so `tx1_scramble2` is the same call
 * here.  69 chooses on bit 0 of `tx_flags` instead (finding F340 against 423);
 * one halfword apart and it is the whole difference between the two shapes.
 *
 * THE CONSTELLATION IS 69's.  The receiver's +0x11e against 0x89b0 is the
 * arm's first instruction, and the sixteen-point half scrambles TWICE: the
 * first call's two bits go into `cur_quadrant` and are READ BACK FROM THERE
 * (0x66f4d) as `vect16`'s row, with the second call's as the column.  Where
 * 69 differs is that neither half here differentially encodes and neither
 * writes `prev_quadrant` -- the quadrant this arm carries is the raw scrambler
 * output, and `prev_quadrant` is written once, at the completion, out of `cur_quadrant`.
 *
 * AND NEITHER HALF ADVANCES `vect_idx`.  This arm counts in `seg_symcount`, and it
 * counts BEFORE the segment-end compare rather than after it: 0x62edc reads
 * +0x25c0, increments, stores, and the STORED value is what 0x62f09 tests.
 * 86 increments last, at the shared 0x6430c; do not read the two as one
 * shape.
 *
 * THE SEGMENT ENDS ON THREE CONDITIONS AND NOT ONE.  With `n` the count just
 * stored and `lim` the length `baud + baud/2 + period` out of the rate
 * configuration at +0xaa84:
 *
 *     n == lim                                        the segment is over
 *     n  > lim  and  hs_mode > 3                        likewise
 *     n  < lim  and  hs_mode > 3  and  n >= baud+period
 *               and  rx->equerr <= rx+0x250 + 10        likewise (0x6559c)
 *
 * so a run that has passed the nominal length finishes at once, and a run
 * that has passed the SHORTER threshold finishes early when the equaliser
 * error at +0x21a has come down to within ten of the mark at +0x250.
 * `hs_mode` (`TX1_HS_MODE`, +0x2218) is the same int table 2's tail reads and
 * the compare is UNSIGNED (`cmpl $0x3 ; jbe`), so a negative value is a large
 * one here.
 *
 * `baud >> 1` IS AN ARITHMETIC SHIFT (0x62f02 is `sar`), not a divide: with
 * a negative `baud` the two differ, and the fixture's fill makes them differ.
 *
 * THE FOUR REJOINS ARE ALL THE SAME BLOCK.  0x63941, 0x6409a, 0x6431f and
 * 0x62d70 each reload the object, re-test the queue count against +0x2aa0
 * and jump to 0x629e7; the addresses are kept in the comments because the
 * blocks are distinct in the object, not because the exits differ.
 *
 * ---------------------------------------------------------------------------
 * THE COMPLETION AT 0x62f22 IS TWO THIRDS OF THE ARM and it is one job: it
 * settles the RECEIVE half of the rate configuration at +0xaa84 and publishes
 * the answer in the INFO capability record at +0xaa3c.  `tx1_ts_snapshot` is
 * its first half and `tx1_ts_rates` its second.
 *
 * `cfg->rx_divtab` IS `initdigital`'s TABLE, INDEXED `initdigital`'s WAY.
 * v34shell.c:1751 reads `rx_divtab[(short)bits + 14 * (short)umax - 1]` and
 * the four sites here read the same expression at -2, -1 and 0 -- so the
 * ladder below walks the same per-rate divisor list the receive context is
 * eventually brought up with, one rate at a time from the top.
 *
 * THE MULTIPLIER IS ONE PREDICATE WRITTEN TWO WAYS.  0x6308b and 0x6316f test
 * `rate >= 0xd` with `setge`; 0x631f6 and 0x6327a test `rate <= 0xc` with
 * `jle`.  They are the same question and `tx1_ts_scale` asks it once.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE ARM REQUIRES OF THE OBJECT, and it is not a style note.
 *
 * `cfg->rx_baud` MUST BE ONE OF 0x960, 0xaf0, 0xbb8, 0xc80 OR 0xd65.  The
 * five-way at 0x62f9c is the ONLY writer of the two locals the whole
 * completion then runs on -- the rate and its floor -- and it has no default:
 * on any other baud the object falls into 0x62fce reading two uninitialised
 * stack slots, and there is nothing in `v34handshak`'s prologue (0x628f0,
 * which writes 0x74, 0x4c and 0x78 and no more) that ever set them.  So that
 * is not a behaviour to reconstruct, it is a precondition the caller meets;
 * `t_v34hstx1.c` pins the field on every run and says so.  A default written
 * here would be a line no test could disagree with.
 *
 * AND THE FLOOR IS ALWAYS BELOW THE RATE, so the ladder always turns at least
 * once: 9 against 1 and 10..13 against 2, whichever way 0xa97e bit 2 goes.
 * That is what stops the term the ladder computes being read before it is
 * written at 0x631a1, and it is a property of those five pairs rather than of
 * the fixture.
 */

/*
 * +0x359a, the object's own `force_low_baud` (the offset is the member's
 * own, issue #260 wave 11).  v34pcmmain.cpp calls the same halfword
 * `OB_FORCE_LOW_BAUD` after `probeselect`'s use of it; here it is the first
 * of four conditions deciding whether the receiver's predictor is kept, and
 * this arm neither writes it nor asserts that meaning.
 */
#define TX1_F359A	((int)__builtin_offsetof(struct v34_object, \
						force_low_baud))

/*
 * +0xa97e, read as a BYTE and tested for bit 2.  It gates the one adjustment
 * the completion makes to the rate the baud implies.  It is `msgrec[1]`'s
 * word[1] (the record array, issue #260 wave 8), and the offset is the
 * member's own (issue #260 wave 11); the byte-wide read stays for the same
 * reason T3C_FAAE2 does.
 */
#define TX1_FA97E	((int)__builtin_offsetof(struct v34_object, \
						msgrec[1].word[1]))

/*
 * +0xe84, twelve shorts of `unmapped_0e84`.  `initdigital` hands exactly this
 * address to `initV34` as the RECEIVE context's coefficient array
 * (v34shell.c:1767), and the snapshot below fills it from the receiver's
 * predictor -- six taps as a conjugate pair and then straight, which is the
 * complex product `(b + ja)` written out.  Finding F424 is the other reader:
 * `initV34` stores an interior self-pointer to it at +0x0a24.
 */
#define TX1_RXCOEFF	0xe84

/*
 * The receiver's three-tap complex predictor at +0x288, read as ONE RUN OF
 * SIX rather than as `pred_b[3]` and `pred_a[3]`: the sum at 0x672de indexes
 * it with a single counter from zero to five across both halves, so the two
 * arrays' adjacency is what that loop is written on.  Reached by offset for
 * that reason -- `pred_b[4]` would be out of bounds and would be a claim the
 * object does not make.  The VALUE is `pred_b`'s own offset, whose six
 * halfwords run straight into `pred_a`'s (issue #260 wave 11).
 */
#define TX1_RX_PRED	((int)__builtin_offsetof(struct v34_receiver, \
							pred_b))

/* The sixth message record's `acc` and `acc0`, the two 32-bit fields at
 * +0xaa60 and +0xaa68 (issue #260 wave 11). */
#define TX1_FAA60	((int)__builtin_offsetof(struct v34_object, \
							msg_acc))
#define TX1_FAA68	((int)__builtin_offsetof(struct v34_object, \
							msg_acc0))

static short
tx1_ts_scale(const struct v34_ratecfg *cfg, int rate, int off)
{
	int x = cfg->rx_divtab[rate + 14 * (int)cfg->rx_use_max + off];

	x >>= 5;
	x *= x;
	x = (short)x;
	x *= (rate >= 0xd) ? 0x4268 : 0x3a98;
	return (short)(x >> 14);
}

/*
 * 0x672c7: what becomes of the receiver's predictor.  (0x62f5f, bit 5 of the
 * flags word, is the short way out and is written in the arm itself.)
 *
 * FOUR CONDITIONS DECIDE WHETHER THE PREDICTOR IS KEPT, in this order and all
 * of them 16-bit: +0x359a clear, the equaliser error at +0x21a no more than
 * 0x1ff, the predictor error at +0x224 strictly below it, and the sum of the
 * six coefficients' magnitudes no more than 0x3fff.  The sum is FORCED to
 * 0x5000 first when it is already past 0x1f40 and the error is both large and
 * improving, which is that last test's real trigger: the object computes a
 * number and then replaces it with one that cannot pass.
 *
 * KEPT means three things -- the six coefficients go into the record at
 * +0xaa40 in the order (b2, a2, b1, a1, b0, a0) and are then bit-reversed
 * over SIXTEEN bits in place, the same six fill the receive context's
 * coefficient array as a conjugate pair, and the predictor's two histories
 * are cleared.  THROWN AWAY means those six record words go to zero.  Either
 * way bit 12 of the flags comes down and the mark at +0x250 moves; only the
 * kept path raises bit 14, and the two branches move the mark to two
 * different fields.
 */
static void
tx1_ts_snapshot(struct v34_object *o, struct v34_receiver *rx, short *rec)
{
	short *co = (short *)((char *)o + TX1_RXCOEFF);
	int sum = 0, t;
	short i;

	rec[0] = (short)0x8000;
	for (i = 0; i <= 5; i++) {
		int v = tx1_get(rx, TX1_RX_PRED + 2u * (unsigned)i) >> 2;

		sum += (v < 0) ? -v : v;
	}

	t = (short)((unsigned short)rx->equerr - (unsigned short)rx->preerr);
	if (t <= 0)
		t = rx->preerr;				/* 0x6874c */
	t = (short)(t - (rx->equerr >> 2));
	if (sum > 0x1f40 && rx->equerr > 0xc8 && t > 0)
		sum = 0x5000;

	if (o->force_low_baud == 0 && rx->equerr <= 0x1ff
	    && rx->preerr < rx->equerr && sum <= 0x3fff) {
		/* 0x6738c */
		if (dsplibs_debug_level > 1)		/* 0x6adf6 */
			dsplibs_debug_printf(
				"V34DATARATE, precoefs [%d,%d,%d][%d,%d,%d]\n",
				tx1_get(rx, TX1_RX_PRED + 0),
				tx1_get(rx, TX1_RX_PRED + 2),
				tx1_get(rx, TX1_RX_PRED + 4),
				tx1_get(rx, TX1_RX_PRED + 6),
				tx1_get(rx, TX1_RX_PRED + 8),
				tx1_get(rx, TX1_RX_PRED + 10));

		rec[2] = rx->pred_b[2];
		rec[3] = rx->pred_a[2];
		rec[4] = rx->pred_b[1];
		rec[5] = rx->pred_a[1];
		rec[6] = rx->pred_b[0];
		rec[7] = rx->pred_a[0];

		co[7] = rec[2];
		co[0] = rec[2];
		co[1] = (short)-rec[3];
		co[6] = rec[3];
		co[9] = rec[4];
		co[2] = rec[4];
		co[3] = (short)-rec[5];
		co[8] = rec[5];
		co[11] = rec[6];
		co[4] = rec[6];
		co[5] = (short)-rec[7];
		co[10] = rec[7];

		for (i = 0; i <= 5; i++)
			rec[2 + i] = (short)bitreverse(
					(unsigned short)rec[2 + i], 0x10);

		rx->flags = (unsigned short)(rx->flags | 0x4000u);
		sysdep_memset(&rx->pred_i[0], 0, 0x10);
		o->receiver.short_4b4 = rx->preerr;
	} else {
		/* 0x674e5 */
		if (dsplibs_debug_level > 1)		/* 0x683b2 */
			dsplibs_debug_printf(
				"V34DATARATE, precoefs=0, 0, 0, 0, 0, 0\n");

		/*
		 * Rolled, this is:
		 *     for (i = 2; i <= 7; i++)
		 *             rec[i] = 0;
		 * -- the six zeros the debug line above names as precoefs.
		 */
		rec[2] = 0;
		rec[3] = 0;
		rec[4] = 0;
		rec[5] = 0;
		rec[6] = 0;
		rec[7] = 0;
		o->receiver.short_4b4 = rx->equerr;
	}

	/* 0x674c8 */
	rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_PREDICT);
}

/*
 * 0x62f82 to 0x63587: the receive rate, settled and published.
 *
 * The baud gives a starting rate and a floor.  The ladder then walks the rate
 * DOWN while the divisor table's term for it is still no more than the mark
 * at +0x250, so it stops at the highest rate the line is measured to carry;
 * the receiver's +0x25e and +0x260 clamp it from either side, and a stale
 * sample clock -- the running count at +0x238 within 96,000 of the mark at
 * +0x248, with `hs_mode` (+0x2218) low -- takes two more off it.  What comes out lands in
 * `cfg->rxbits`, is clamped again to the object's own `rate_min` and
 * `rate_max`, is overridden outright by `rate_want`, and is finally
 * bit-reversed into the capability word beside the transmit rate.
 *
 * THE TWO NIBBLES SWAP BY ROLE, exactly as `initdigital` swaps them
 * (v34shell.c:1568): `role == 0x65` puts the receive rate at bit 6 and the
 * transmit rate at bit 10, and any other value the other way round.  The
 * object open-codes `bitreverse(..., 4)` at 0x6346b and 0x677a8 as a branchless
 * chain of shifts and `lea` doubles -- the Horner evaluation
 * `((b6*2 + b7)*2 + b8)*2 + b9` -- and NOT a call to the `bitreverse` symbol,
 * which is why this site never appears in the object's call census.  The
 * open-coding is reproduced here, in the two-cap-nibble block below (finding
 * F11525): the compiler would otherwise keep a `bitreverse` call out-of-line
 * (that is the only path), so an explicit call is what the object never had.
 *
 * AND THE UPSTREAM CAP IS APPLIED LAST.  If the nibble just written asks for
 * more than `VPcmV34GetMaxUpstreamRateIndex` allows, the four bits are
 * replaced one at a time -- `mask` walking 0x40, 0x80, 0x100, 0x200 or the
 * same four shifted up by four -- from `(cap * 7) >> 14`, which is
 * v34pcmmain.cpp:318's divide by 2340 rather than by 2400.
 *
 * `rec[k]` IS THE OBJECT'S OWN ADDRESSING: one base register at +0xaa3c and a
 * byte displacement, so `rec[8]` is +0xaa4c and `rec[21]` is +0xaa66.
 *
 * THE TXSTATE COMPARE AT 0x634b3 CANNOT BE FALSE, for finding F342's reason at
 * 65: the arm is reached only through table 1 at `txstate == 66`, and neither
 * `txmit` nor anything else between the dispatch and here writes +0x3596.  It
 * is written as the object writes it.
 */
static int
tx1_ts_rates(struct v34_object *o, struct v34_receiver *rx,
	     struct v34_ratecfg *cfg, short *rec)
{
	int rate = 0, ratemin = 0, term = 0, cap, v, mask, k;
	short baud;

	/* 0x62f82 */
	cfg->rx_use_max = 1;
	baud = cfg->rx_baud;
	switch ((unsigned short)baud) {
	case 0x960: ratemin = 1; rate = 9;  break;
	case 0xaf0: ratemin = 2; rate = 11; break;
	case 0xbb8: ratemin = 2; rate = 12; break;
	case 0xc80: ratemin = 2; rate = 13; break;
	case 0xd65: ratemin = 2; rate = 14; break;
	}

	/* 0x62fce */
	if (cfg->rx_use_max != 0)
		rec[0] = (short)((unsigned short)rec[0] | 2u);

	if ((*((unsigned char *)o + TX1_FA97E) & 4) == 0) {
		/* 0x62ff8 */
		if (baud == 0x0c80 || baud == 0x0d65)
			rate = 0xc;
		else if (baud == 0x0bb8 || baud == 0x0af0)
			rate = (short)(rate - 1);	/* 0x67984 */

		/*
		 * 0x63025, and NOT part of the 3200/3429 arm: 0x6797e and
		 * 0x67994 both jump back to the guard, so every baud that
		 * reaches this block reports, including the ones that changed
		 * nothing.  Placing it inside the `rate = 0xc` branch left
		 * four cases short -- 2400, 2800, 3000 and the ladder's
		 * floor -- which is how the difference was found.
		 */
		if (dsplibs_debug_level > 1)		/* 0x6834b */
			dsplibs_debug_printf(
				"V34INFO, V.34bis is not possible \n");
	}

	/* 0x6302e */
	cfg->txbits = (short)rate;
	while ((short)rate > (short)ratemin) {
		term = tx1_ts_scale(cfg, rate, -1);
		if (dsplibs_debug_level > 1)		/* 0x66b61 */
			dsplibs_debug_printf(
				"V34DATARATE,threshold for data rate" " %d = %d\n", rate, term);
		if (o->receiver.short_4b4 < (short)term)
			break;
		rate = (short)(rate - 1);
	}

	/* 0x630e9 */
	if (rx->rrn_local_dir > 1) {
		int d = rx->baud_copy;

		if (rx->rrn_local_dir == 2 && rate > d - 1) {
			rate = (short)(d - 1);		/* 0x68308 */
			if (dsplibs_debug_level > 1)	/* 0x68334 */
				dsplibs_debug_printf(
					" TRNSEG4A : returning from local rrn" " down => forcing rate down\n");
		} else if (rate < d + 1) {
			rate = (short)(d + 1);		/* 0x63120 */
			if (dsplibs_debug_level > 1)	/* 0x69100 */
				dsplibs_debug_printf(
					" TRNSEG4A : returning from local rrn" " up => forcing rate up\n");
		}
		term = tx1_ts_scale(cfg, rate, -1);	/* 0x63134 */
	}

	/* 0x63198 */
	if (dsplibs_debug_level > 1)			/* 0x6759e */
		dsplibs_debug_printf(
			"V34DATARATE, ethresh data rate = %d,ethreh=%d," "rate2 = 0x%x,data=%d\n",
			rate, o->receiver.short_4b4,
			(unsigned short)o->receiver.short_382, term);
	if (dsplibs_debug_level > 1)			/* 0x675e6 */
		dsplibs_debug_printf("V34DATARATE, equerr = %d,preerr=%d\n",
				     rx->equerr, rx->preerr);

	rx->bad_thresh = (short)(2 * term);
	if ((short)rate > (short)ratemin) {
		rx->bad_long_thresh = tx1_ts_scale(cfg, rate, -2);
		rx->bad_long_thresh = (short)((rx->bad_long_thresh + term) >> 1);
	} else {
		/* 0x672ad */
		rx->bad_long_thresh = (short)(o->receiver.short_4b4 << 3);
	}

	/* 0x63234 */
	if ((short)cfg->txbits > (short)rate)
		rx->good_thresh = tx1_ts_scale(cfg, rate, 0);
	else
		rx->good_thresh = 0;				/* 0x6729b */

	/* 0x63299 */
	rx->bad_long_run = 0;
	rx->good_run = 0;
	rx->bad_run = 0;
	rec[1] = (short)0xfffd;
	if ((unsigned)o->sample_count
	    < (unsigned)(o->int_0248 + 0x17700)
	    && (unsigned)o->hs_mode <= 3u) {
		rate = (short)(rate - 2);
		if ((short)rate < (short)ratemin)
			rate = ratemin;
		cfg->txbits = (short)rate;
		rec[1] = (short)((unsigned short)rec[1] & ~1u);
	}

	/* 0x6332c */
	cfg->rxbits = (short)rate;
	if (dsplibs_debug_level > 1)			/* 0x67741 */
		dsplibs_debug_printf(
			"V34DATARATE, automatic: %d, min %d, max %d\n",
			0x960 * rate, 0x960 * o->rate_min,
			0x960 * o->rate_max);

	/* 0x6334a */
	v = cfg->rxbits;
	if (v < o->rate_min) {
		cfg->rxbits = (short)o->rate_min;
		v = (short)o->rate_min;
	}
	if (v > o->rate_max)
		cfg->rxbits = (short)o->rate_max;
	if (o->rate_want >= 0 && o->rate_want != o->rate_now)
		cfg->rxbits = (short)o->rate_want;

	/*
	 * Bench-only rate cap.  This is deliberately a compile-time switch: the
	 * normal reconstruction, including the differential build, retains the
	 * blob's exact outgoing-MP choice.  It lets the interop harness distinguish
	 * an early data-phase failure caused by the rate proposed to the peer from
	 * one which occurs independently of that proposal.
	 */
#ifdef DSPLIB_V34_TEST_TX_RATE_CAP
	if (cfg->txbits > DSPLIB_V34_TEST_TX_RATE_CAP)
		cfg->txbits = DSPLIB_V34_TEST_TX_RATE_CAP;
#endif

	/* 0x633b5 */
	rate = (short)bitreverse((unsigned short)cfg->rxbits, 4);
	o->short_358c =
		(short)bitreverse((unsigned short)cfg->txbits, 4);
	if (dsplibs_debug_level > 1)			/* 0x676f3 */
		dsplibs_debug_printf(
			"V34DATARATE, Final choice data rate = %d,"
			" retrainThresh = %d, renegDownthresh = %d," " renegUpthresh = %d\n",
			cfg->rxbits, rx->bad_thresh, rx->bad_long_thresh, rx->good_thresh);

	/* 0x6340f */
	if (o->role == 0x65)
		rec[0] = (short)((unsigned)(unsigned short)rec[0]
				 | ((unsigned)o->short_358c << 10)
				 | ((unsigned)rate << 6));
	else {
		rec[0] = (short)((unsigned)(unsigned short)rec[0]
				 | ((unsigned)rate << 10));
		rec[0] = (short)((unsigned)(unsigned short)rec[0]
				 | ((unsigned)o->short_358c << 6));
	}

	/* 0x63453 */
	cap = VPcmV34GetMaxUpstreamRateIndex(o);
	if (o->role == 0x65) {
		/* 0x677a8 */
		v = ((((((unsigned)(unsigned short)rec[0] >> 10) & 1) * 2
		      + (((unsigned)(unsigned short)rec[0] >> 11) & 1)) * 2
		      + (((unsigned)(unsigned short)rec[0] >> 12) & 1)) * 2
		      + (((unsigned)(unsigned short)rec[0] >> 13) & 1));
		mask = ~0x400;
	} else {
		/* 0x6346b */
		v = ((((((unsigned)(unsigned short)rec[0] >> 6) & 1) * 2
		      + (((unsigned)(unsigned short)rec[0] >> 7) & 1)) * 2
		      + (((unsigned)(unsigned short)rec[0] >> 8) & 1)) * 2
		      + (((unsigned)(unsigned short)rec[0] >> 9) & 1));
		mask = ~0x40;
	}
	if (v * 0x960 > cap) {
		/* 0x6768b */
		int rev = (short)bitreverse((unsigned short)((cap * 7) >> 14),
					    4);

		for (k = 0; k <= 3; k++) {
			unsigned short w = (unsigned short)rec[0];

			if ((rev >> k) & 1)
				w = (unsigned short)(w | (unsigned)~mask);
			else
				w = (unsigned short)(w & (unsigned)mask);
			rec[0] = (short)w;
			mask = (short)(mask * 2 + 1);
		}
	}

	/* 0x634a6 */
	rec[8] = 0;					/* +0xaa4c */
	hs_set_txstate(o, V34HS_XMITMP);
	o->vect_idx = 0;
	o->short_3598 = 0;
	o->short_359e = 0;
	if (dsplibs_debug_level > 1)			/* 0x67653 */
		dsplibs_debug_printf(
			"V34DATARATE, txmp bits 0x%x,0x%x,0x%x,0x%x,0x%x\n",
			(unsigned short)rec[0], (unsigned short)rec[1],
			(unsigned short)rec[2], (unsigned short)rec[3],
			(unsigned short)rec[4]);

	/* 0x63510 */
	o->msg_acc = 0x3fffe;
	o->msg_acc0 = 0x3fffe;
	rec[10] = -1;					/* +0xaa50 */
	rec[13] = 0;					/* +0xaa56 */
	rec[15] = 0;					/* +0xaa5a */
	rec[17] = 0;					/* +0xaa5e */
	rec[14] = 0x10;					/* +0xaa58 */
	rec[12] = (short)((rx->flags & V34_RX_FLAG_RENEG) ? 0x30 : 0x90);
	rec[11] = 1;					/* +0xaa52 */
	rec[20] = 0x12;					/* +0xaa64 */
	rec[21] = 0x12;					/* +0xaa66 */
	rec[16] = 0;					/* +0xaa5c */
	o->short_3590 = 0x22;
	return 0;				/* 0x62d70 */
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
static short
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

static struct v34_bitsource *
tx1_bitsource(struct v34_object *o)
{
	return (struct v34_bitsource *)o->paa6c;
}
static void
tx1_mp_reload(struct v34_object *o, struct v34_bitsource *b,
	      unsigned short flags)
{
	b->crc = (short)0xffff;
	b->pos = 0;
	b->idx = 0;
	b->repeats = 0;
	b->wordbits = 0x10;
	b->nbits = (short)((flags & 0x20u) ? 0x30 : 0x90);
	b->crc_on = 1;
	b->avail = 0x12;
	b->avail0 = 0x12;
	b->repeat = 0;
	b->acc = 0x3fffe;
	b->acc0 = 0x3fffe;

	/* 0x646a1 */
	o->vect_idx = 0;
	o->short_3590 = 0x22;

	/*
	 * 0x646b5.  `flags` IS the receiver's word and the object re-reads
	 * it here rather than using the copy it was handed; nothing between
	 * the two writes it, so the argument is the parameter.
	 */
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf(
			"V34MP, Starting txmit MP again(%d)," " rxflgs=0x%x,txflags=0x%x\n",
			o->short_359e, flags, o->tx_flags);
}

static int
tx1_mp_sequence_end(struct v34_object *o, struct v34_receiver *rx)
{
	unsigned short flags;
	short seq;
	int stamp;

	seq = (short)((unsigned short)o->short_359e + 1);
	o->short_359e = seq;

	if ((*((unsigned char *)o + TX1_FAA3C) & 1) != 0) {
		flags = rx->flags;			/* 0x645f9 */
		if ((flags & 0x90u) == 0x90u && seq > 3) {
			/* 0x648b1 */
			if (o->short_3598 == 0) {
				initdigital(o);
				o->short_3598 = 1;
			}
			/* 0x648bf */
			hs_set_txstate(o, V34HS_EXMIT);
			o->vect_idx = 0;
			rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_RENEG);
			return 1;
		}
		stamp = (flags & (V34_RX_FLAG_TRN_WATCH | V34_RX_FLAG_LATE_TRN))
			== V34_RX_FLAG_TRN_WATCH;		/* 0x64614 */
	} else {
		flags = rx->flags;			/* 0x647a9 */
		stamp = (flags & 0x18u) == 0x10u;	/* 0x647b0 */
	}

	/*
	 * 0x647c5 RE-READS +0x359e rather than using the value just stored.
	 * Nothing between the two writes it, so the two are the same number;
	 * it is written as the object writes it.
	 */
	if (stamp && o->short_359e > 1) {
		/* 0x647d3 */
		struct v34_bitsource *b = tx1_bitsource(o);

		if ((b->word[0] & 1) == 0)
			o->short_359e = 0;
		b->word[0] = (short)((unsigned short)b->word[0] | 1u);
		if (dsplibs_debug_level > 1)		/* 0x67254 */
			dsplibs_debug_printf(
				"V34MP, MP detected, starting MP' txmit");
		flags = rx->flags;			/* 0x64805 */
	}

	tx1_mp_reload(o, tx1_bitsource(o), flags);
	return 0;
}
static void
tx1_mp16(struct v34_object *o, short src)
{
	short pick = (short)((o->tx_flags & V34_TXFLAG_CALLER) == 0);
	short k, q;
	int point;

	q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, pick, src, 2);
	o->cur_quadrant = (short)((q + (unsigned short)o->prev_quadrant) & 3);

	q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, pick,
				(short)(src >> 2), 2);
	k = o->cur_quadrant;					/* re-read, 0x63c72 */
	o->prev_quadrant = k;
	point = vect16[q + 4 * k];
	tx1_put_point(o, point);
}
static void
tx1_mp4(struct v34_object *o, short src)
{
	short pick = (short)((o->tx_flags & V34_TXFLAG_CALLER) == 0);
	short q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, pick, src, 2);

	(void)tx1_dpsk4(o, q);
}

static void
tx1_dpsk_tone(struct v34_object *o, short bit)
{
	short sel = (short)((unsigned short)o->short_358c
			    ^ (unsigned short)bit);

	o->short_358c = sel;
	tx1_put_point(o, vect4[2 * (sel & 1)]);
}

static void
tx1_moh_cleardown(struct v34_object *o)
{
	hs_set_txstate(o, V34HS_MOH_CLEARDOWN);
	hs_set_rxstate(o, V34HS_WAIT);
	o->short_abe4 = 1;
}

static void
tx1_moh_reinit(struct v34_object *o, const char *msg)
{
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf(msg);
	v34handshakinit(o, 1);
	o->short_abe6 = 1;
}

static int
tx1_moh_hold(struct v34_object *o)
{
	if ((int)o->vect_idx < ((int)o->rtd >> 4) + 0x4b0)
		return 0;			/* 0x6431f */

	if (o->moh_path_sel != 0) {
		if (dsplibs_debug_level > 1)		/* 0x6889f */
			dsplibs_debug_printf(
				"MOH: Timeout waiting for MH sequence under"
				" MHreq, disconnecting...\r\n");
		/* 0x688b4 */
		tx1_moh_cleardown(o);
		o->short_abe2 = 1;
		return 0;			/* 0x629cf */
	}

	if ((unsigned)(o->moh_message - 2) > 1u) {
		/* 0x6923d */
		tx1_moh_reinit(o,
			       "MOH: Timeout waiting for MH sequence under"
			       " MHreq, initiating retrain\r\n");
		return 0;			/* 0x629cf */
	}

	if (dsplibs_debug_level > 1)			/* 0x65031 */
		dsplibs_debug_printf(
			"MOH: Timeout waiting for MH sequence under"
			" cleardown, terminating connection without" " acknowledge\r\n");

	/* 0x65046 */
	tx1_moh_cleardown(o);
	return 0;				/* 0x64326 */
}

static void
tx1_moh_on_hold(struct v34_object *o)
{
	hs_set_txstate(o, V34HS_MOH_SILENCE);
	hs_set_rxstate(o, V34HS_WAIT);
	o->short_aa78 = 0;
	o->vect_idx = 0;
}

static void
tx1_moh_send(struct v34_object *o)
{
	struct v34_bitsource *b;

	if (o->short_abe2 != 3)
		o->short_abe2 = 1;
	if (o->moh_path_sel == 0) {
		if (dsplibs_debug_level > 1)	/* 0x7041b */
			dsplibs_debug_printf(
				"MOH: MHnack received for MHreq," " sending MHfrr\r\n");
		o->moh_message = 1;		/* 0x70430 */
	} else {
		if (dsplibs_debug_level > 1)	/* 0x6a400 */
			dsplibs_debug_printf(
				"MOH: MHnack received for MHreq," " sending MHcda\r\n");
		o->moh_message = 3;		/* 0x6a415 */
	}

	/* 0x6a427 */
	VPcmV34SetMohMessageBits(o, (short *)tx1_bitsource(o));
	o->paa6c = o->msgrec;

	/* 0x6a44e */
	b = o->msgrec;
	b->crc = (short)0xffff;
	b->pos = 0;
	b->idx = 0;
	b->repeats = 0;
	b->nbits = 8;
	b->wordbits = 8;
	b->crc_on = 1;
	b->acc = 0xf72;
	b->acc0 = 0xf72;
	b->avail = 0xc;
	b->avail0 = 0xc;
	b->repeat = 0;
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
/*
 * The offsets `v34handshakinit` writes, hoisted above it: a #define is
 * visible only from its own line down, the T3C_ and T3M_ family blocks sit
 * further down beside the arms that own the rest of each family, and the
 * init sites spell these by name (issue #260).  Where a macro is defined has
 * no codegen presence -- the tokens at every use site are identical either
 * way -- so this is preprocessor data moved, not a statement reordered.
 */
#define T3C_F358C	((int)__builtin_offsetof(struct v34_object, \
					    short_358c))
	/* short: cleared by MOH_TONE's body; the member's own
	   offset (issue #260 wave 11)        */
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
#define T3M_F3588		((int)__builtin_offsetof(struct v34_object, \
					    short_3588))
	/* short, arm 47's second entry guard; the member's own offset
	   (issue #260 wave 11); `v34handshakinit` also writes it  */
#define T3M_F358A		((int)__builtin_offsetof(struct v34_object, \
					    short_358a))
	/* short, set to 1 beside it; the member's own offset    */
void
v34handshakinit(void *objp, int mode)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = &obj->receiver;
	unsigned char *m = (unsigned char *)obj;
	int delta;
	int base;

	obj->hs_mode = 0;

	delta = obj->int_0248;
	if (delta > 0) {
		unsigned d = (unsigned)(obj->sample_count - delta);

		if (d <= 0x176ff) {
			obj->sample_count = (int)d;
			obj->int_0248 = 0;
		} else {
			obj->sample_count = 0;
			obj->int_0248 = (int)0xfff15a00;
		}
	}

	base = obj->sample_count;
	obj->int_0244 = base;
	obj->timeout_deadline = base + 0x69780;

	obj->short_abe6 = 0;
	m[0xabe8] = 0;
	m[0xabf8] = 0;
	obj->short_abe4 = 0;
	obj->backward_clear = 0;
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

		hs_set_txstate(obj, V34HS_SILENCEINFO);
		hs_set_rxstate(obj, V34HS_RX_DPSK);
		hs_set_microstate(obj, V34HS_DET_SYNC);

		rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_PREDICT);
		obj->paa70 = &obj->msgrec[1];
		obj->msgrec[1].nbits = 0x11;
		obj->tx_pwr_reduction = 0;

		/*
		 * 0x66 and not 0x65: `role` is the originate/answer flag
		 * `v34modeminit` and `preinitdigital` both test against 0x65,
		 * and this one arm tests the other value.  Reproduced as
		 * written -- a sweep of {0x65, something-else} would not have
		 * told the two tests apart.
		 */
		if (obj->role == 0x66)
			V34SetINFO0dBits(obj, obj->msgrec[1].word);
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
		if ((rx->flags & V34_RX_FLAG_RETRAIN) || obj->remote_retrain_ind != 0) {
			short n = (short)(*(unsigned short *)(m + 0xac14) + 1);

			obj->remote_retrain_ind = 0;
			obj->short_ac14 = n;
		} else {
			obj->short_ac12 = (short)(*(unsigned short *)(m + 0xac12) + 1);
		}

		v34modeminit(obj);

		hs_set_txstate(obj, V34HS_SILENCERETRAIN);
		hs_set_rxstate(obj, V34HS_WAIT);
		/* No microstate transition here; mode 1 is the only body
		 * that leaves it alone. */

		rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_PREDICT);
		rx->agc_gain = rx->agc_start_gain;
		obj->short_358a = 2;
		obj->short_3588 = 2;
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
		hs_set_txstate(obj, V34HS_SSEG);
		hs_set_rxstate(obj, V34HS_RECEIVE);
		hs_set_microstate(obj, V34HS_DET_SYNC);

		/*
		 * The message record at +0xaa0c, blanked.  Same twelve fields
		 * and the same 0x30-byte stride as the one `VPcmV34Set-
		 * MohMessageBits` fills at +0xa94c, which is what says the
		 * five records from +0xa94c to +0xaa3c are one array.  The
		 * blob takes the record's address into a register first
		 * (0x5fdb2, `lea 0xaa0c(%ebx)`) and writes the twelve fields
		 * through it -- the local view is its own spelling, not a
		 * convenience (issue #260 wave 8: record 4 is msgrec[4] of
		 * the one array now).
		 */
		{
			struct v34_bitsource *r = &obj->msgrec[4];

			r->crc = -1;
			r->crc_on = 0;
			r->nbits = 0;
			r->pos = 0;
			r->wordbits = 0;
			r->idx = 0;
			r->repeat = 0;
			r->repeats = 0;
			r->acc = 0;
			r->avail = 0;
			r->avail0 = 0;
			r->acc0 = 0;
		}

		obj->seg_symcount = 0;
		obj->prev_quadrant = 0;
		obj->tx_scr_sr = 0;
		obj->tx_flags = (short)((obj->tx_flags & ~0x4018) | 0x2000);

		rx->rx_blocks = 0x21e;
		obj->receiver.short_382 = (short)0x8990;
		rx->flags = (unsigned short)((rx->flags & ~0x1d8) | 0x18);

		obj->vect_idx = 0;
		obj->short_aa78 = 0;

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

		hs_set_txstate(obj, V34HS_SILENCERETRAIN);
		hs_set_rxstate(obj, V34HS_WAIT);
		hs_set_microstate(obj, V34HS_MOH_TONE);

		*(short **)(m + 0xaa70) = (short *)(m + 0xa97c);
		obj->msgrec[1].nbits = 8;
		obj->paa6c = obj->msgrec;

		VPcmV34SetMohMessageBits(obj, obj->msgrec[0].word);

		/*
		 * RE-READ, not the pointer just stored: the object reloads
		 * +0xaa6c after the call, so the source names the field and
		 * not a local.  Nothing observable turns on it -- the callee
		 * does not write +0xaa6c -- but reproducing the read costs
		 * nothing and reproducing the wrong one might.
		 */
		{
			struct v34_bitsource *r =
				(struct v34_bitsource *)obj->paa6c;

			r->crc = -1;
			r->crc_on = 1;
			r->nbits = 8;
			r->pos = 0;
			r->wordbits = 8;
			r->idx = 0;
			r->repeat = 0;
			r->repeats = 0;
			r->acc = 0xf72;
			r->avail = 0xc;
			r->avail0 = 0xc;
			r->acc0 = 0xf72;
		}

		obj->short_358c = 0;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"v34handshakinit: initiating MOH negotiation\n");

		/*
		 * The phase-2 signalling carrier the tone detector watches
		 * for, the other way round from `v34modeminit`'s -- which is
		 * consistent, since the two ends listen for each other.
		 */
		detectorinit(&obj->detector,
			     (obj->role == 0x65) ? c2400_ : c1200_,
			     0, 0x64, 0x32, 0x800, 0);
		obj->detector.armed = 1;
		obj->short_358a = 0;
		break;

	default:
		/* Out of range runs the tail, and nothing else. */
		break;
	}

	obj->info_caps = 0;
	obj->tx_fill_target = 0x10;
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
 * twenty-four periods.  the per-sample transmit dispatch below implements that step
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
	struct v34_receiver *rx = &obj->receiver;
	short *msg = (short *)(m + 0xa9ac);
	const unsigned char *pcfg = (const unsigned char *)obj->pac3c;

	/*
	 * The rate config, by the names `setfinalrate` gives the same eight
	 * fields, plus the five per-rate pre-emphasis slots -- which are the
	 * only thing here that function does not also write.
	 */
	short *tx_baud		= &obj->ratecfg.baud;
	short *tx_preemp	= &obj->ratecfg.preemp;
	const short **tx_scale	= &obj->ratecfg.divtab;
	short *tx_carrier	= &obj->ratecfg.carrier;
	short *rx_baud		= &obj->ratecfg.rx_baud;
	short *rx_carrier	= &obj->ratecfg.rx_carrier;
	const short **rx_scale	= &obj->ratecfg.rx_divtab;
	const short **rx_cdesc	= &obj->det_coefs;
	short *pe2400		= &obj->ratecfg.pe_2400;
	short *pe2800		= &obj->ratecfg.pe_2800;
	short *pe3000		= &obj->ratecfg.pe_3000;
	short *pe3200		= &obj->ratecfg.pe_3200;
	short *pe3429		= &obj->ratecfg.pe_3429;

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
	snr_l1 = obj->nl_ratio;
	snr_l2 = obj->pb_ratio;

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
	role = obj->role;

	if (obj->force_low_baud != 0)
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
	int scale = obj->tx_scale;
	short extra;
	short want;
	short n;

	/* The second MP read follows this store, as in the object. */
	obj->tx_pwr_reduction = (short)bitreverse((unsigned short)(((unsigned short)mp[0] >> 5) & 7), 3);

	extra = (short)bitreverse((unsigned short)(((unsigned short)mp[0] >> 2) & 7), 3);
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
				     (int)obj->tx_scale, (int)want,
				     scale);

	obj->tx_scale = (short)((scale * 0x4b4b + 0x2000) >> 14);
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
	struct v34_receiver *rx = &obj->receiver;
	int pcm;

	settxlevel(obj, (const short *)&obj->msgrec[3]);

	pcm = (obj->v90_receiver != 0 || obj->k56flex_receiver != 0);

	V34SetupModulator(v34_object_modulator(obj),
			  obj->ratecfg.baud, obj->ratecfg.carrier,
			  obj->ratecfg.preemp, pcm, 1);

	rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_FIR);

	hs_set_rxstate(obj, V34HS_WAIT);
	hs_set_txstate(obj, V34HS_SSEG);

	obj->seg_symcount = 0;
	obj->tx_flags = (short)(obj->tx_flags | V34_EC_FEED);

	txinit(obj);
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
 * EVERY ARM IS WRITTEN.  The range test at 0x65329 sends a microstate
 * outside table 3's 41..80 window to the transmit dispatch, and the switch
 * below has a label for every value inside it, so there is no unwritten case
 * and no default arm -- the object has neither.
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
/*
 * The receiver's timing pair, +0x238/+0x23c of the receiver -- the members
 * `timing_offset`/`timing_phase` since wave 8, named by the blob's own
 * exported getters.  The two diagnostic sites that read them UNSIGNED keep
 * the helper; these are their offsets (issue #260 wave 9).
 */
#define T3C_TIMER_LO	((int)__builtin_offsetof(struct v34_receiver, \
							timing_offset))
#define T3C_TIMER_HI	((int)__builtin_offsetof(struct v34_receiver, \
							timing_phase))
#define T3C_MODE	((int)__builtin_offsetof(struct v34_object, \
					    hs_mode))
	/* int:   selects what the tail reports; the member's own
	   offset (issue #260 wave 11)       */
#define V34_MSGREC_BASE	((int)__builtin_offsetof(struct v34_object, \
					    msgrec))
	/* five struct v34_bitsource records; the member's own
	   offset (issue #260 wave 11) */
#define T3C_COUNT	((int)__builtin_offsetof(struct v34_object, \
					    short_aa78))
	/* short: the counter, and HS_TRACE_2; the member's own
	   offset (issue #260 wave 11)       */
#define T3C_COUNT_SRC	((int)__builtin_offsetof(struct v34_object, \
					    filtdelay))
	/* short: RX_PHASE3_CALL copies it in; the member's own
	   offset (issue #260 wave 11)       */
/* now the member's own offset (issue #260). */
#define T3C_PTR_AA6C	((int)__builtin_offsetof(struct v34_object, paa6c))
/* now the member's own offset (issue #260). */
#define T3C_PTR_AA70	((int)__builtin_offsetof(struct v34_object, paa70))
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
#define T3C_FAAE2	((int)__builtin_offsetof(struct v34_object, fsk) \
			 + (int)__builtin_offsetof(struct v34_fsk, sr))
	/* THE LOW BYTE of `fsk.sr`; see above; the member's own offset
	   (issue #260 wave 11)   */

/*
 * THE LAST TWO STRICT-ALIASING WARNINGS IN THE TREE WERE THE LINE THAT STOOD
 * HERE, AND `struct v34_receiver` (v34recv.h) IS WHAT FIXED THEM.  It gives
 * the same 0x264 bytes a genuine `struct v34_receiver *` -- `&obj->receiver`
 * -- so every site that read this macro now names a real struct member: no
 * cast, no punning, no warning left.  Issue #260 wave 8 made that struct the
 * ONE map of the region: the object's own readings moved into it and the
 * wave-7 union is gone; the wave-8 asserts in v34fsk.h hold the members at
 * their object offsets through the receiver.
 */

/* Handshake field offsets retained for accesses with explicit widths. */

/* These offsets name fields through their owner declaration.
 * The access macros preserve each site's signedness and width. */
#define T3M_TICK		((int)__builtin_offsetof(struct v34_object, \
						low_energy_count))
	/* +0x0234: 0x62aa9 increments this one 32-bit word and 0x62ab3
	   compares it against 0x257f */
#define T3M_ELAPSED		((int)__builtin_offsetof(struct v34_object, \
							sample_count))
#define T3M_DEADLINE		((int)__builtin_offsetof(struct v34_object, \
							timeout_deadline))
	/* int, ELAPSED plus 431,488; the two are compared UNSIGNED at
	   0x62a84 */
#define T3M_COUNTER		((int)__builtin_offsetof(struct v34_object, \
					    short_aa78))
	/* short, the counter six of these arms bump; it is the `[2]`
	   every trace prints (V34hshak.c's HS_TRACE_2); the member's
	   own offset (issue #260 wave 11)  */
#define T3M_FSKGATE		((int)__builtin_offsetof(struct v34_object, \
							retrain_poll))
#define T3M_TOGGLE		((int)__builtin_offsetof(struct v34_object, \
					    short_358c))
	/* short, arm 48 inverts bit 0 of it; the member's own
	   offset (issue #260 wave 11)  */

/*
 * Arms 47 and 63.  `short_abae`/`short_abc2` have been members since the
 * wave-5 clears converted; `p2_state` (+0xabff) is wave 9's, named from
 * VPcmV34Progress's phase-2 substate ladder, and `p2_delay_cntr` (+0x240)
 * is the object's own "p2DelayCntr".
 */
#define T3M_FABAE		((int)__builtin_offsetof(struct v34_object, \
					    short_abae))
	/* ten shorts arm 47 clears; the member's own offset   */
#define T3M_FABC2		((int)__builtin_offsetof(struct v34_object, \
					    short_abc2))
	/* the eleventh, cleared separately; the member's own
	   offset (issue #260 wave 11)  */
#define T3M_FABFF		((int)__builtin_offsetof(struct v34_object, \
							p2_state))
	/* SIGNED byte, arm 63's third guard */
#define T3M_F0240		((int)__builtin_offsetof(struct v34_object, \
							p2_delay_cntr))
	/* int, arm 63's fourth guard                          */

/*
 * The fields arms 49, 50 and 51 add.  `filtdelay` is not a guess: it is what
 * the object's own diagnostic at 0x709d7 calls the third thing it prints,
 * "On RX_PHASE1_ANS: is short=%d, bulkDelay=%d, filtDelay=%d".  The rest are
 * offsets because what they are FOR was not measured here.
 */
#define T3M_FILTDELAY		((int)__builtin_offsetof(struct v34_object, \
					    filtdelay))
	/* short; three of arm 49's and arm 50's four thresholds are
	   it plus a constant; the member's own offset (wave 11) */
#define T3M_F35A0		((int)__builtin_offsetof(struct v34_object, \
							short_35a0))
	/* short, arm 51 clears on both paths  */
#define T3M_F35A2		((int)__builtin_offsetof(struct v34_object, \
							md_length))
	/* short, arm 51's is_short path only  */
#define T3M_TXBAUD		((int)__builtin_offsetof(struct v34_object, \
							ratecfg.baud))
#define T3M_TXCARRIER		((int)__builtin_offsetof(struct v34_object, \
							ratecfg.carrier))
	/* short, the transmit carrier         */
#define T3M_RXCARRIER		((int)__builtin_offsetof(struct v34_object, \
							ratecfg.rx_carrier))
	/* short, arm 51 copies the transmit carrier here */

/* Within the receiver, and reached from `rx` and not from the object. */
#define T3M_RX_F264		((int)__builtin_offsetof(struct v34_receiver, \
					    short_264))
	/* short, arm 49's last write; the receiver member's own
	   offset (issue #260 wave 11); t72's read of it stays a
	   BYTE wide (finding F553's shape)     */

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
 * and that was the point: the READS here go through `hs_get`/`hs_put`, which
 * take the offset as a RUNTIME argument because one helper serving all three
 * state machines keeps the format strings' argument orders in one place
 * (finding F632) -- so for the busiest fifty-nine of these there is no field
 * for a field access to name.  Issue #260 wave 6 has since typed the WRITES
 * (`hs_set_microstate`/`hs_set_rxstate`/`hs_set_txstate` bind their word at
 * compile time); what is left on these offsets is the reads, and the readers
 * that reach another viewer's word by offset.  The offsets stay.
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

#define T3M_I32(obj, off)		(*(int *)((unsigned char *)(obj) + (off)))
#define T3M_U32(obj, off)		(*(unsigned *)((unsigned char *)(obj) + (off)))
#define T3M_I16(obj, off)		(*(short *)((unsigned char *)(obj) + (off)))
#define T3M_U16(obj, off)		(*(unsigned short *)((unsigned char *)(obj) + (off)))

/* Repeated INFO0 recovery operations shared by several microstates. */

static void
t3m_errrec_core(struct v34_object *obj)
{
	struct v34_bitsource *r = obj->msgrec;
	int k;

	obj->is_short = 0;
	obj->local_short = 0;
	obj->short_358a = 1;

	/* 0x6cc55, ten shorts, and the eleventh is not in the loop. */
	for (k = 0; k <= 9; k++)
		obj->short_abae[k] = 0;
	obj->short_abc2 = 0;

	/*
	 * Both transitions, and they are announced BEFORE the counter is
	 * cleared -- the counter is `[2]` in each of the two format strings,
	 * so clearing it first would print two different lines and change no
	 * byte of the object.
	 */
	hs_set_txstate(obj, V34HS_TX_DPSK);
	hs_set_microstate(obj, V34HS_DET_SYNC);

	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	/*
	 * 0x6cdc4 and 0x7094b.  The record is filled with one constant per
	 * field, and only +0x18 depends on anything: 0x1e when this end
	 * originates AND a V.90 receiver is running, 0x11 otherwise.  The
	 * originate test alone is not enough -- with no V.90 receiver the
	 * object jumps back into the 0x11 block.
	 */
	r->crc = -1;
	r->crc_on = 1;
	r->nbits = (obj->role == 0x65 && obj->v90_receiver != 0)
		   ? 0x1e : 0x11;
	r->pos = 0;
	r->wordbits = 8;
	r->idx = 0;
	r->repeat = 1;
	r->repeats = 0;
	r->acc = 0xf72;
	r->avail = 0xc;
	r->avail0 = 0xc;
	r->acc0 = 0xf72;
}

/*
 * 0x6cc1b, 0x70d7f, 0x66517 and 0x660b9 -- the four copies whose first store
 * is the constant 4 rather than a set of bit 0.
 */
static void
t3m_errrec_reset(struct v34_object *obj)
{
	obj->short_3588 = 4;
	t3m_errrec_core(obj);
}

/*
 * 0x6c14a, 0x6bc0b, 0x6fff7, 0x70447 and 0x6d709 -- the five whose first store
 * is `|= 1`, which is NOT the same thing: the reset can run with +0x3588
 * already holding 2, and 4 would destroy that bit while 3 keeps it.
 */
static void
t3m_errrec_arm(struct v34_object *obj)
{
	obj->short_3588 = (short)(T3M_U16(obj, T3M_F3588) | 1);
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
#ifndef DSPLIB_REPRODUCE_BUGS
/*
 * D77: The INFO0 slicer keeps clocking in silence. It can turn near-zero
 * input into a run of ones during the caller's required phase-2 quiet period
 * (V.34, 11.2.1.1.3). Require 64 counts RMS at the AGC output before treating
 * four ones as a repeat. This is well below the normalised handshake tone.
 * Divide each square before summing so full-scale shorts cannot overflow.
 */
static int
t3m_info0_carrier(const short *in)
{
	unsigned energy = 0;
	int i;

	for (i = 0; i < V34_FSK_BLOCK; i++)
		energy += (unsigned)((int)in[i] * in[i]) / V34_FSK_BLOCK;
	return energy >= 64u * 64u;
}
#endif

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
	struct v34_receiver *rx = &obj->receiver;

	if ((t3c_getb(obj, T3C_FAAE2) & 1) == 0) {
		/*
		 * 0x6abae.  Bit 0 clear -- `testb $0x1,0xaae2` at 0x65c81 --
		 * and the counter at +0xaa78 keeps whatever it had: the copy
		 * from +0xaa7c is on the other side of that branch, at 0x65c8e
		 * and 0x65c99.
		 */

		return;
	}

	/* Sixteen bits, and the trace below reads it back as [2]. */
	obj->short_aa78 = obj->filtdelay;

	hs_set_microstate(obj, V34HS_TX_PHASE2_CALL);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PHASE2, RX_PHASE3_CALL ,filtdelay = "
				     "%d, rxflgs= 0x%x,rx->gain=0x%x\n",
				     (int)obj->filtdelay,
				     (unsigned)rx->flags,
				     (unsigned)(int)rx->agc_gain);

	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);

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
	struct v34_receiver *rx = &obj->receiver;

	obj->short_aa78 = (short)(obj->short_aa78 + 1);

	return (short)tone_detect(rx, &obj->detector,
				  rx->burst,
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

		return;
	}

	if (obj->short_aa78 < obj->short_abfc) {
		/*
		 * 0x6c701.  `cmp 0xabfc(%esi),%ax` at 0x6581d and a SIGNED
		 * `jl`: the count and the limit are both halfwords and both
		 * read off the object by adjacent instructions.
		 */

		return;
	}

	hs_set_txstate(obj, V34HS_TX_DPSK);

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
		detectorinit(&obj->detector,
			     obj->role == 0x65 ? c2400_ : c1200_,
			     1, 0xf0, 0x32, 0x800, 0x400);
		/*
		 * 0x6d5db sits BETWEEN the microstate load at 0x6d5d4 and the
		 * `cmp $0x50` at 0x6d5e2, so +0x356a takes its 1 whether or
		 * not the transition below turns out to be a move.
		 */
		obj->detector.armed = 1;		/* 0x6d5db */
		hs_set_microstate(obj, V34HS_MOH_TONE_DROP);
	} else {
		hs_set_microstate(obj, V34HS_DET_SYNC);
	}

	obj->fsk.phase = 0;
	obj->fsk.nbits = 0;
	obj->msgrec[1].crc = -1;
	obj->msgrec[1].nbits = 8;
	obj->short_aa78 = 0;
	obj->paa70 = &obj->msgrec[1];
	obj->paa6c = obj->msgrec;
	obj->short_358c = 0;
	obj->vect_idx = 0;

}

static void
t3c_micro_moh_tone_drop(struct v34_object *obj)
{
	if (t3c_moh_step_detector(obj) != 0
	    && obj->moh_msg_pending == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34Handshake: Detected signal "
					     "drop on FRR request, time to " "move to phase1...\r\n");
		obj->moh_msg_pending = 1;
	}

	/* The round-trip delay sets how long the drop has to persist. */
	if ((int)obj->short_aa78 < (((int)obj->rtd) >> 2) + 0x12c0) {
		/*
		 * 0x6aae6.  Both halfwords are sign-extended before the
		 * arithmetic -- `movswl 0xaa7e`, `movswl 0xaa78`, `sar $0x2`,
		 * `add $0x12c0` -- so a negative delay shortens the wait
		 * rather than wrapping it.
		 */

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
	if (obj->moh_path_sel != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("MOH: Timeout waiting for MH " "sequence under MHfrr, "
					     "disconnecting...\r\n");

		hs_set_txstate(obj, V34HS_MOH_CLEARDOWN);
		hs_set_rxstate(obj, V34HS_WAIT);
		obj->short_abe4 = 1;		/* 0x6c983 */
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

		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("MOH: Timeout waiting for MH sequence "
				     "under MHfrr, initiating retrain\r\n");

	v34handshakinit(obj, 1);
	obj->short_abe6 = 1;

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
 * field are the same value, and the setters' own "already there" guard
 * reproduces each of them exactly.  They are written as transitions, not as
 * branches that could never be taken.
 */

#define T41_FABE8	((int)__builtin_offsetof(struct v34_object, \
					    moh_active))
	/* byte: non-zero takes the second door; the member's
	   own offset (issue #260 wave 11) */

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
	struct v34_receiver *rx = &obj->receiver;

	return (short)tone_detect(rx, &obj->detector,
				  rx->burst,
				  rx->rx_samples);
}

/* The first five stores of one 0x30-byte record; +0x18 is what varies. */
static void
t41_record_head(struct v34_bitsource *rec, short f18)
{
	rec->crc = -1;
	rec->pos = 0;
	rec->idx = 0;
	rec->repeats = 0;
	rec->nbits = f18;
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
	if (obj->short_35a0 <= 0x31) {

		return;
	}
	if (obj->fsk.sr != 0) {
		/*
		 * 0x6dd06.  `cmpw $0x0,0xaae2` at 0x6dcb1 -- sixteen bits,
		 * where the arm's entry test on the same field is eight.
		 */

		return;
	}
	if (obj->moh_msg_pending != 0) {
		/*
		 * 0x6dcf3.  `cmpb $0x0,0xabf8` at 0x6dcbb: the drop has
		 * already been reported, and reporting it is all the
		 * fall-through has left to do.
		 */

		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34Handshake: Detected signal drop on "
				     "FRR request (after NACK), time to move " "to phase1...\r\n");

	obj->moh_msg_pending = 1;

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
	struct v34_receiver *rx = &obj->receiver;
	short *rec = (short *)obj->paa70;

	hs_set_microstate(obj, V34HS_DET_INFO);

	*(short *)((char *)rec +
		    __builtin_offsetof(struct v34_bitsource, crc)) = -1;
	obj->fsk.nbits = 0;

	if (obj->short_358a == 0)
		rx->flags = (unsigned short)(rx->flags
					     & ~V34_RX_FLAG_DET_PENDING);

	obj->short_aa78 = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "DET_SYNC is received in RX_DPSK,rx->gain=0x%x\n",
		    (int)rx->agc_gain);

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
	struct v34_receiver *rx = &obj->receiver;
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

		return;
	}

	obj->is_short = 0;
	obj->local_short = 0;
	obj->short_358a = 1;
	obj->short_3588 = (short)(obj->short_3588 | 1);

	for (i = 0; i <= 9; i++)
		obj->short_abae[i] = 0;

	obj->short_abc2 = 0;

	hs_set_txstate(obj, V34HS_TX_DPSK);
	hs_set_microstate(obj, V34HS_DET_SYNC);

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
		t41_record_head(obj->msgrec, 0x1e);		/* 0x70d47 */
	else
		t41_record_head(obj->msgrec, 0x11);		/* 0x70bdd */

	/*
	 * 0x70c07 is where the two heads MEET: it is the next instruction
	 * after the 0x11 arm's `movw $0x11,0x18` and it is the 0x1e arm's
	 * `jmp` target at 0x70d6c, so everything from here down is emitted
	 * once.
	 */
	obj->msgrec[0].wordbits = 8;				/* 0x70c07 */
	obj->msgrec[0].crc_on = 1;
	obj->msgrec[0].acc = 0xf72;
	obj->msgrec[0].acc0 = 0xf72;
	obj->msgrec[0].avail = 0xc;
	obj->msgrec[0].avail0 = 0xc;
	obj->msgrec[0].repeat = 1;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "errorrecovery is initialized in DET_SYNC\n");

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
	struct v34_receiver *rx = &obj->receiver;
	const unsigned char *rec;
	short *out;
	int i;

	/*
	 * 0x6dac7's `test %ax,%ax` sends a silent detector to 0x6de83, which
	 * re-reads +0xabe8 with a `movzbl` before jumping to the junction --
	 * the re-read the note above says is why `abe8` is a parameter.
	 */
	if (t41_detect(obj) == 0) {
		t41_after_guards(obj, obj->moh_active); /* 0x6de83 */
		return;
	}

	rec = (const unsigned char *)obj->paa6c;
	if ((rec[4] & 0x80) == 0) {
		/*
		 * 0x6dae1's `testb $0x80,0x4(%eax)` on the record +0xaa6c
		 * points at, and 0x6de70 re-reads +0xabe8 exactly as 0x6de83
		 * does -- two stubs three instructions apart, one junction.
		 */
		t41_after_guards(obj, obj->moh_active); /* 0x6de70 */
		return;
	}

	obj->short_358a = 0;

	/*
	 * 0x6dafa is `js`, so a negative count copies nothing at all, and
	 * the loop itself is a do/while over 0..+0xabc2 INCLUSIVE -- the
	 * test at 0x6db1b compares the already-incremented index.
	 */
	if (obj->short_abc2 >= 0) {
		out = (short *)obj->paa70;
		for (i = 0; (short)i <= obj->short_abc2; i++)
			out[i] = obj->short_abae[i];
	}

	obj->short_aa78 = 0;

	if (obj->role == 0x65) {
		hs_set_microstate(obj, V34HS_RX_PHASE1_CALL);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Tone AB detected ending " "errorrecovery. Switch to "
					     "RX_PHASE1_CALL.\n");
	} else {
		obj->fsk.sr = -1;
		hs_set_microstate(obj, V34HS_TX_PHASE1_ANS);
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
		    (const unsigned short *)obj->paa70;

		dsplibs_debug_printf(
		    "%s 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
		    "V34INFO, rxinfo0",
		    (unsigned)p[0], (unsigned)p[1], (unsigned)p[2],
		    (unsigned)p[3], (unsigned)p[4], (unsigned)p[5],
		    (unsigned)p[6], (unsigned)p[7], (unsigned)p[8],
		    (unsigned)p[9]);
	}

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

		return;
	}

	if (obj->role != 0x65) {
		if (obj->retrain_state != 1) {
			/*
			 * 0x6dffb, another `jne` straight to the dispatch, on
			 * the `cmpw $0x1,0xa24a` at 0x6dff3 -- sixteen bits
			 * wide.
			 */

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

		return;
	}

	/* 0x6e03a, and +0x3588 gets bit 1 before any transition. */
	obj->short_3588 = (short)(obj->short_3588 | 2);
	hs_set_microstate(obj, V34HS_RX_PHASE1_CALL);
	hs_set_rxstate(obj, V34HS_RX_DPSK);
	hs_set_txstate(obj, V34HS_TONE_AB);

	obj->short_358c = 0;
	obj->vect_idx = 0;
	obj->short_aa78 = 0;
	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DET_SYNC not detected, during search " "for info1a\n");

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
		obj->short_35a0 = 0;			/* 0x6ceca */

	next = 0;
	if (aae2 == 0 || aae2 == 0xffff)
		next = (short)((unsigned short)obj->short_35a0 + 1);

	tx = obj->txstate;
	/*
	 * 0x6da8e, out of line: `cmp $0x3c,%cx` at 0x6cf0f -- TONE_AB --
	 * jumps here, zeroes +0x35a0 and returns to 0x6cf27 past the store,
	 * so transmitting the tone discards the stepped value.
	 */
	if (tx == V34HS_TONE_AB)
		obj->short_35a0 = 0;			/* 0x6da8e */
	else
		obj->short_35a0 = next;

	/*
	 * 0x6cf39.  The base is rebuilt here from the delay spilled at
	 * 0x6cead -- `movswl 0x42(%esp)` then `sar $0x4` at 0x6cf27 -- and
	 * `jl 62af1` is the once-per-block transmit dispatch (finding F390).
	 */
	if ((int)aae0 < base + 400) {

		return;
	}

	/*
	 * 0x6cf4e tests a +0x35a0 RE-READ at 0x6cf46, not the value the
	 * step above stored: the TONE_AB arm at 0x6da8e wrote zero there
	 * and rejoined below the store, so only memory has the answer.
	 */
	if (obj->short_35a0 <= 0x31) {
		t41_marks_late(obj, aae0);		/* 0x6cf4e */
		return;
	}

	if (aae2 == 0xffff) {
		/*
		 * 0x6d2f8.  The record at +0xa9ac is configured and +0xaa6c
		 * aimed at it -- an INTERIOR pointer, which the fixture
		 * compares by offset from each side's own base (finding F359).
		 */
		hs_set_txstate(obj, V34HS_TX_DPSK);

		t41_record_head(&obj->msgrec[2], 0x4d);
		obj->msgrec[2].wordbits = 8;
		obj->msgrec[2].crc_on = 1;
		obj->msgrec[2].avail = 0x10;
		obj->msgrec[2].avail0 = 0x10;
		obj->msgrec[2].repeat = 0;
		obj->paa6c = &obj->msgrec[2];
		obj->msgrec[2].acc = 0xff72;
		obj->msgrec[2].acc0 = 0xff72;
		obj->short_358c = 0;

		hs_set_microstate(obj, V34HS_INFODONE);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34PHASE2,detected INFOMARKS, "
					     "reinitializing Info1c\n");

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
	obj->short_3588 = (short)(obj->short_3588 | 2);
	hs_set_txstate(obj, V34HS_SILENCERETRAIN);
	hs_set_rxstate(obj, V34HS_WAIT);

	obj->short_aa78 = 0;
	obj->vect_idx = 0;
	obj->short_358c = 0;
	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DET_SYNC not detected, but Tone is "
				     "detected during info1\n");

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

	obj->short_aa7a = 0;

	if ((unsigned char)aae2 == 0x72) {
		/*
		 * 0x6c862 opens with `cmp $0x2c,%si` -- the setters'
		 * folded guard on DET_INFO, and one the dispatch that read
		 * 41 can never satisfy (finding F392).
		 */
		t41_to_det_info(obj);			/* 0x6c862 */
		return;
	}

	abe8 = obj->moh_active;
	short_358a = obj->short_358a;

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

				/*        +0x18 depends on it              */
#define T46_MSG_N	10

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
t46_init_record(struct v34_object *obj, struct v34_bitsource *rec)
{
	short arm = (obj->role == 0x65 && obj->v90_receiver != 0)
		    ? 0x1e : 0x11;

	rec->crc = -1;
	rec->pos = 0;
	rec->idx = 0;
	rec->repeats = 0;
	rec->nbits = arm;
	rec->wordbits = 8;
	rec->crc_on = 1;
	rec->acc = 0xf72;
	rec->acc0 = 0xf72;
	rec->avail = 0xc;
	rec->avail0 = 0xc;
	rec->repeat = 1;
}

/*
 * The reset all four bodies share: clear the message, move both machines on,
 * and rearm the record.
 *
 * The two transitions are the setters, traces and all -- 0x65e53 and
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
	obj->short_358a = 1;

	for (i = 0; i < T46_MSG_N; i++)
		obj->short_abae[i] = 0;
	obj->short_abc2 = 0;

	hs_set_txstate(obj, V34HS_TX_DPSK);
	hs_set_microstate(obj, V34HS_DET_SYNC);

	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	t46_init_record(obj, obj->msgrec);
}

/* 0x65df4 -- the counter past 1200. */
static void
t46_body_repeated_late(struct v34_object *obj)
{
	obj->short_3588 = 4;
	t46_reset_core(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Repeated info0 is detected (after 1200), "
				     "errorrecovery is initialized in " "TX_PHASE1_ANS\n");

}

/* 0x6abc1 -- the counter past 199 with +0xaae2's low ten bits at 0x372. */
static void
t46_body_repeated(struct v34_object *obj)
{
	obj->short_3588 = 4;
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
	obj->short_aa7a = 0;
	obj->short_3588 = 4;
	obj->paa6c = obj->msgrec;
	t46_reset_core(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("info0 is detected, info0 is initialized "
				     "in TX_PHASE1_ANS\n");

	/*
	 * 0x6adc7 enters the chain one compare in, past the `== 2` test.
	 * Its `test %ax,%ax` cannot fire from here -- this body stored 4
	 * into +0x3588 -- which is finding F418's unreachable arm.
	 */
	t46_chain_tail(obj, obj->short_3588);	/* 0x6adc7 */
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
	obj->paa6c = obj->msgrec;
	obj->paa70 = &obj->msgrec[1];

	V34SetINFO0aBits(obj, (short *)obj->msgrec);
	if (obj->role == 0x66)
		V34SetINFO0dBits(obj, (short *)obj->paa70);

	obj->short_3588 = (short)(obj->short_3588 | 4);
	t46_reset_core(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Repeated info0 is initialized on " "retrain\n");

	obj->retrain_req = 0;

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

	if (obj->short_aa78 <= T46_CNT_LOW) {
		/*
		 * 0x6add0, the tail the whole chain funnels into: reload
		 * the object, put +0x3596 in %ecx, jump to 0x62af1.  The
		 * guard above it is `cmpw $0xc7,0xaa78(%ebx)` at 0x6c3fa.
		 */

		return;
	}
	if ((obj->fsk.sr & 0xfff) != 0xf72) {
		/*
		 * 0x6add0 again, and this guard is TWELVE bits wide:
		 * `and $0xfff` then `cmp $0xf72` at 0x6c410, where the
		 * head at 0x65dbb masks ten and compares 0x372.  0xf72 &
		 * 0x3ff is 0x372, so this test is the stricter of the two.
		 */

		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("TX_PHASE_ANS: count3 = %d \n",
				     (int)obj->short_aa7a);

	n = (short)(obj->short_aa7a + 1);
	/*
	 * 0x70c7f stores the stepped count and re-enters the chain at
	 * 0x6adc7 with +0x3588 re-read at 0x70c8d -- still the 2 that
	 * got here, so its zero arm cannot fire (finding F418).
	 */
	if (n <= T46_COUNT3_LIM) {			/* 0x70c7f */
		obj->short_aa7a = n;
		t46_chain_tail(obj, obj->short_3588);
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
	if (obj->short_aa78 <= T46_CNT_HIGH) {
		/*
		 * 0x6c120, another copy of the tail: the object, +0x3596
		 * into %ecx, and `jmp 62af1` at 0x6c12e.  The counter was
		 * re-read at 0x65dd4 for the compare above.
		 */

		return;
	}
	/*
	 * 0x65de6 is `cmpw $0x0,0xaae2(%edi)` -- sixteen bits wide, not
	 * the byte spelling microstate 62 keeps (findings F417 and F553) --
	 * and 0x6bd7a is one more copy of the transmit tail.
	 */
	if (obj->fsk.sr == 0) {

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
	short sub = obj->short_3588;

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
	short tx = obj->txstate;

	/*
	 * 0x6b5b0, reached by `cmp $0x18,%cx; je` at 0x65d7b.  The two
	 * fields are two compares with their own `je 65d8f` (0x6b5bb and
	 * 0x6b5c6), so either one clear leaves the counter alone.
	 */
	if (tx == V34HS_TX_DPSK) {			/* 0x6b5b0 */
		short *r = (short *)obj->paa6c;

		if (r[0x20 / 2] != 0 && r[0x22 / 2] != 0) {
			obj->short_aa78 = 0;
			r[0x20 / 2] = 0;
		}
	/*
	 * 0x6b4ba, reached by `cmp $0x3c,%cx` at 0x65d85 on the transmit
	 * state the head read once.  It re-reads +0xaa78 through its own
	 * object load, steps it, and only then compares: `inc %eax` then
	 * `cmp $0x18f,%ax`.
	 */
	} else if (tx == V34HS_TONE_AB) {		/* 0x6b4ba */
		short n = (short)(obj->short_aa78 + 1);

		if (n <= T46_TONE_LIMIT) {
			/*
			 * 0x6f8f9 stores the step and jumps
			 * back to 0x65d8f -- the head's
			 * +0x3588 test, not the transmit tail.
			 */
			obj->short_aa78 = n;	/* 0x6f8f9 */
		} else if (obj->fsk.sr != 0) {
			/*
			 * 0x6fb6e is its twin for the
			 * past-399 path with +0xaae2 set:
			 * same store, same `jmp 65d8f`.
			 */
			obj->short_aa78 = n;	/* 0x6fb6e */
		} else if (obj->retrain_req != 0) {
			obj->short_aa78 = n;
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
			obj->short_aa78 = 0;
			obj->short_358c = (short)(obj->short_358c ^ 1);
			hs_set_microstate(obj, V34HS_RX_PHASE1_ANS);
			obj->short_358a = 2;

			return;
		}
	}

	/* 0x65d8f, where both head guards rejoin, 0x6f8f9 included. */
	/* 0x6adbd re-reads +0x3588 at 0x6adb6; 0x65d96 is this test's. */
	if (obj->short_3588 != 0) {
		t46_chain_full(obj);			/* 0x6adbd */
		return;
	}

	/* 0x65da6, on the %esi 0x65d8f loaded, and a ten-bit mask. */
	if (obj->short_aa78 > T46_CNT_LOW
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

/*
 * And what the three SIZED arms of the accept path reach.  A message of
 * eight bits is Modem-on-Hold, one of 0x26 is INFO1a and one of 0x4d is
 * INFO1c -- three different messages through one bit clock, so each arm ends
 * somewhere else.
 */
#define T44_PROBE	((int)__builtin_offsetof(struct v34_object, \
					    probe_bins))
	/* the probe bins V34GiveProbeResults takes; the member's
	   own offset (issue #260 wave 11) */

/*
 * The record reached through +0xaa70, and the one the restart installs at
 * +0xaa6c.  Both are inside the object, so the harness compares the two
 * pointers by offset from their own base rather than as addresses.
 *
 * The reads went through a `t44_record(obj, off)` helper until wave 7: the
 * members are the object's `void *` (F634's under-claim), so every site now
 * reads `obj->paa70`/`obj->paa6c` and casts at the site, exactly as the
 * wave-5 sites beside them do.  The +0xaab0 detector-coefficient pointer is
 * not one of them -- it has no member -- and its two readers spell the slot
 * read the way the `*(const short **)(m + 0xaab0)` at the top of this file
 * does.
 */

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
	struct v34_bitsource *rec = (struct v34_bitsource *)obj->paa70;
	struct v34_bitsource *blk = obj->msgrec;
	short nbits;
	int i;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34INFO, info0 CRC not received "
				     "properly in DET_INFO\n");

	/*
	 * RX_DPSK is the state this arm is only ever reached in, so this
	 * transition is a no-op on every path a test can drive -- it is here
	 * because the object writes it, and the setter is what makes it
	 * cost nothing when the state is already there.
	 */
	hs_set_rxstate(obj, V34HS_RX_DPSK);
	hs_set_microstate(obj, V34HS_DET_SYNC);

	rec->crc = -1;
	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	if (obj->role == 0x66) {
		/*
		 * 0x718fc, the answering side.  The store is BEFORE the
		 * diagnostic check and unconditional; only the printf is
		 * guarded.
		 */
		rec->nbits = 0x1e;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("SetINFO0dBits  \n");
	}

	/* 0x6bf18, where the +0x359c == 0x66 arm rejoins. */
	obj->short_358a = 1;
	obj->paa6c = blk;
	obj->short_3588 = (short)(obj->short_3588 | 4);
	obj->is_short = 0;
	obj->local_short = 0;

	/*
	 * TEN SHORTS FROM +0xabae, INDEXED FROM ZERO.  0x6bf1f loads 1 into
	 * %eax for the +0x358a store above and 0x6bf3e zeroes it again before
	 * the loop, so the loop runs 0..9 and not 1..9; the bound at 0x6bf71
	 * is `cmp $9,%ax; jle`, tested after the increment.
	 */
	for (i = 0; i <= 9; i++)
		obj->short_abae[i] = 0;
	obj->short_abc2 = 0;

	/*
	 * TX_DPSK is table 2's arm at 0x644c9, which is written -- so this
	 * path can be driven at all.  Finding F354 is the same constraint on
	 * microstate 79, and for the same reason.
	 */
	hs_set_txstate(obj, V34HS_TX_DPSK);
	hs_set_microstate(obj, V34HS_DET_SYNC);

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

	blk->crc = -1;
	blk->pos = 0;
	blk->idx = 0;
	blk->repeats = 0;
	blk->nbits = nbits;

	/*
	 * 0x6c0e8 is where the 0x1e arm rejoins -- `jmp 6c0e8` at
	 * 0x71950 -- so +0x1c and everything after it is written once
	 * for both lengths.
	 */
	blk->wordbits = 8;				/* 0x6c0e8 */
	blk->crc_on = 1;
	blk->acc = 0xf72;
	blk->acc0 = 0xf72;
	blk->avail = 0xc;
	blk->avail0 = 0xc;
	blk->repeat = 1;
}

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

	raw = (short)(((obj->msgrec[3].word[0] & 3) << 5)
		      | ((obj->msgrec[3].word[1] & 0xf8) >> 3));
	obj->md_length = raw;

	rev = bitreverse((unsigned short)raw, 7);
	obj->md_length = (short)rev;

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
		obj->moh_msg_pending = 1;
		hs_set_rxstate(obj, V34HS_RX_DPSK);
		hs_set_microstate(obj, V34HS_DET_SYNC);
		((struct v34_bitsource *)obj->paa70)->crc = -1;
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
	detectorinit(&obj->detector, obj->role == 0x65 ? c2400_ : c1200_,
		     1, 0x64, 0x32, 0x800, 0x400);
	/*
	 * 0x6ec4a stores the %ebp loaded at 0x6ec2f, before the
	 * `detectorinit` call it survives, and it lands between
	 * the setters' load of +0x3592 at 0x6ec43 and its compare
	 * against 0x50 at 0x6ec51.
	 */
	obj->detector.armed = 1;			/* 0x6ec4a */
	hs_set_microstate(obj, V34HS_MOH_TONE_DROP);
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
	struct v34_receiver *rx = &obj->receiver;
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
	if ((short)V34GiveINFO1aBits(obj,
			      (const short *)obj->paa70) != 0) {
		/* 0x6ed59, and +0x3594 against 0x23 is the WAIT guard. */
		hs_set_rxstate(obj, V34HS_WAIT);
		hs_set_microstate(obj, V34HS_INFODONE);
		/*
		 * 0x6ee72, with INFODONE stored at 0x6ee6b:
		 * `jmp 66956` back into the byte clock.
		 */
		return 0;				/* 0x6ee72 */
	}

	/* 0x6ee77, with bitreverse's 7 already staged in %eax. */
	(void)t44_mdlength(obj);
	setfinalrate(obj);
	hs_set_rxstate(obj, V34HS_RECEIVE);
	rxinit(obj);

	baud = obj->ratecfg.rx_baud;
	carrier = obj->ratecfg.rx_carrier;
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
	detectorinit(&obj->detector,
		     obj->det_coefs,
		     0, 8, 10, 0x600, 0);
	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);
	detectorinit(&obj->detector,
		     obj->det_coefs,
		     0, 8, 0x78, 0x600, 0);
	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);

	/*
	 * 0x6f146 follows the second `or $0x200` into the receiver's
	 * +0x122 at 0x6f13f, so the counter is cleared after both
	 * detector set-ups rather than between them.
	 */
	obj->short_aa78 = 0;			/* 0x6f146 */
	/*
	 * 0x6f15c is scheduled INSIDE the trace's guard: 0x6f155 compares
	 * dsplibs_debug_level and 0x6f163 is the `jbe` that leaves for the
	 * byte clock, and the store sits between them.  The 1 has been in
	 * %esi since 0x6f10b, held across the second `detectorinit`.
	 */
	obj->rate_change_pending = 1;			/* 0x6f15c */

	if (DSPLIB_DEBUG_ON())
		t44_print10("V34PROBE, rxinfo1a",
			    (const short *)&obj->msgrec[3]);
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
	struct v34_bitsource *blk;

	/*
	 * 0x6f443 and 0x6f44a, from the %ebx and %ecx zeroed at 0x6f43f and
	 * 0x6f441.  The arm's FIRST instruction is neither store: 0x6f438
	 * reads +0x3596 for the setter below, whose `cmp $0x18` --
	 * TX_DPSK is 24 -- is at 0x6f451.
	 */
	obj->vect_idx = 0;			/* 0x6f443 */
	obj->short_aa78 = 0;			/* 0x6f44a */
	hs_set_txstate(obj, V34HS_TX_DPSK);

	(void)t44_mdlength(obj);
	probeselect(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PROBESELECT, in ANSWER, txbaudrate = "
				     "%d,rxbaudrate = %d,\n",
				     (int)obj->ratecfg.baud,
				     (int)obj->ratecfg.rx_baud);
	if (DSPLIB_DEBUG_ON())
		t44_print10("V34PROBE, rxinfo1c",
			    (const short *)&obj->msgrec[3]);

	/*
	 * 0x6f60e installs the record BEFORE the decoder runs, and 0x6f651
	 * reads it back afterwards rather than reusing the pointer it just
	 * wrote -- the decoder is a call, so the object reloads.
	 */
	obj->paa6c = &obj->msgrec[2];
	if (V34GiveINFO1dBits(obj, (const short *)obj->paa70) != 0)
		return 0;				/* 0x6f622 */

	V34GiveProbeResults(obj, (const char *)obj + T44_PROBE);
	V34SetINFO1aBits(obj, (short *)obj->paa6c);

	/*
	 * The INFO1a this end will send: 0x26 bits, which is the length the
	 * 0x26 arm above answers to -- the two arms are the two ends of one
	 * exchange, and that is what makes the constant checkable twice.
	 *
	 * 0xff72 and not the restart's 0xf72 (finding F401): the same twelve
	 * bits of preamble with a thirteenth set above them, written as a
	 * full int at +0x24 and at +0x2c.
	 */
	blk = (struct v34_bitsource *)obj->paa6c;
	blk->crc = -1;
	blk->pos = 0;
	blk->idx = 0;
	blk->repeats = 0;
	blk->nbits = 0x26;
	blk->wordbits = 8;
	blk->crc_on = 1;
	blk->avail = 0x10;
	blk->avail0 = 0x10;
	blk->repeat = 0;
	blk->acc = 0xff72;
	blk->acc0 = 0xff72;

	/*
	 * 0x6f6ba, and the microstate it is about to move has already been
	 * read: 0x6f6a5 loads +0x3592 between the +0x20 and +0x24 stores
	 * above, and 0x6f6c1 is the `cmp $0x3f` against it.
	 */
	obj->short_358c = 0;			/* 0x6f6ba */
	hs_set_microstate(obj, V34HS_INFODONE);

	if (DSPLIB_DEBUG_ON())
		t44_print10("V34PROBE, txinfo1a",
			    (const short *)&obj->msgrec[2]);
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
t44_det_info_accept(struct v34_object *obj, struct v34_bitsource *rec,
		    short count2)
{
	struct v34_receiver *rx = &obj->receiver;
	short len = rec->nbits;
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
		return t44_accept_len26(obj, (short *)rec);	/* 0x6ed17 */
	if (len == 0x08)
		return t44_accept_len08(obj, (short *)rec, count2);	/* 0x6ea38 */

	if (obj->short_358a == 1) {
		struct v34_bitsource *blk = (struct v34_bitsource *)obj->paa6c;

		/* 0x6e571, and it is the OTHER record's +0x04. */
		if ((blk->word[2] & 0x80) == 0) {
			blk->repeats = 0;
			blk->word[2] = (short)(blk->word[2] | 0x80);
		}

		/*
		 * How many BYTES arrived: the bit count divided by eight
		 * truncating toward zero, plus one if any bits are left over.
		 * 0x6e59b's `lea 0x7(%edx)` before the `and` is how GCC spells
		 * a truncating divide of a value that may be negative, and
		 * 0x6e5b8 tests the low three bits of the SAME count.
		 */
		count = obj->short_aa78;
		nbytes = count / 8 + ((count & 7) != 0 ? 1 : 0);
		obj->short_abc2 = (short)nbytes;

		/* Into the array the restart clears -- finding F401. */
		for (i = 0; i < nbytes; i++)
			obj->short_abae[i] = rec->word[i];

		if ((rec->word[2] & 0x80) == 0) {
			/*
			 * 0x6e5fb.  This exit does NOT rejoin the bit clock:
			 * 0x6e740 jumps straight to the transmit dispatch.
			 */
			hs_set_rxstate(obj, V34HS_RX_DPSK);
			hs_set_microstate(obj, V34HS_DET_SYNC);
			((struct v34_bitsource *)obj->paa70)->crc = -1;
			/*
			 * 0x6e739 puts -1 in +0xaae2 and 0x6e740 is one
			 * `jmp 62af1`.  The `return 1` has no instructions
			 * of its own -- caller and callee leave through the
			 * same jump, which is why the caller's `return`
			 * carries this address too.
			 */
			obj->fsk.sr = -1;

			return 1;
		}
	}

	/* 0x6e745: F358A != 1 (0x6e561) and +0x04 bit 7 set (0x6e5f5). */
	obj->short_aa78 = 0;

	/*
	 * The buffer is obj+0xa97c LITERALLY and not the +0xaa70 record --
	 * 0x6e757 is `lea 0xa97c(%ebx)`.  `v34handshakinit` mode 0 hands
	 * `V34SetINFO0dBits` the same fixed address.
	 */
	V34GiveINFO0dBits(obj, (const short *)&obj->msgrec[1]);

	if (DSPLIB_DEBUG_ON()) {
		const short *r = (const short *)obj->paa70;

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
		hs_set_microstate(obj, V34HS_RX_PHASE1_CALL);
	} else {
		obj->fsk.sr = -1;			/* 0x6e814 */
		if (obj->is_short != 0) {
			hs_set_microstate(obj, V34HS_RX_PHASE1_ANS);
			/*
			 * 0x6e8ac is also where the inlined setter
			 * lands when the microstate ALREADY holds 0x31 --
			 * `je 6e8ac` at 0x6e82c -- so the reload of +0xaa78
			 * from +0xaa7c runs whether the state moved or not.
			 */
			obj->short_aa78 = obj->filtdelay;	/* 0x6e8ac */
		} else {
			hs_set_microstate(obj, V34HS_TX_PHASE1_ANS);
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
	struct v34_bitsource *rec;
	short nbits, count, next;
	int idx;

	obj->short_aa7a = 0;

	/* No bit waiting: the arm is over before it starts. */
	nbits = obj->fsk.nbits;
	if (nbits == 0) {

		return;
	}

	/* 0x668e1 reads the record BEFORE 0x668e7 writes the count back. */
	rec = (struct v34_bitsource *)obj->paa70;
	obj->fsk.nbits = (short)(nbits - 1);

	count = obj->short_aa78;

	/*
	 * The message's own bits go through the register; the sixteen after
	 * it are the CRC and do not.  One bit per call, MSB first:
	 *
	 *   668ff  movswl %ax,%edx / shr $0x1f,%edx    the register's bit 15
	 *   66907  testb  $0x1,0xaae2(%edi)            the incoming bit
	 *   66910  xor    $0x1,%edx                    so %edx is their XOR
	 *   66922  xor    $0x1021,%eax                 taken only when it is 1
	 */
	if (count < rec->nbits) {
		unsigned crc = (unsigned short)rec->crc;
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
		rec->crc = (short)crc;
	}

	next = (short)(count + 1);
	obj->short_aa78 = next;

	/*
	 * Sixteen CRC bits after the message: 0x66944 re-reads the length.
	 * 0x6bda4 then compares the register against what arrived, sixteen
	 * bits wide, and equal is the message accepted.
	 */
	if ((int)next == (int)rec->nbits + 0x10) {
		if ((unsigned short)obj->fsk.sr
		    == (unsigned short)rec->crc) {
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
	next = obj->short_aa78;

	/* Only every eighth bit completes a byte. */
	if ((next & 7) != 0) {
		/*
		 * 0x66966 `jne 6bd8d`, and the test at 0x66964 is
		 * `test $0x7,%al` -- ONE BYTE of a counter the source
		 * carries as a short.
		 */

		return;
	}
	if (next <= 0) {
		/*
		 * 0x6696c `test %ax,%ax` + `jle 7186a`: sixteen bits and
		 * SIGNED, so a negative counter leaves here too.
		 */

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

		return;
	}

	/*
	 * 0x6698e re-reads +0xaa70: the restart above may have run in
	 * between, and it is the LOW BYTE of `sr` that is stored, widened to
	 * a short.
	 */
	rec = (struct v34_bitsource *)obj->paa70;
	rec->word[idx] = (short)(unsigned char)obj->fsk.sr;

}

/*
 * ---------------------------------------------------------------------------
 * rxstate 53 `DET_AB`, 0x65473 -- the end of L2, and the retrain that is not
 * always one.
 *
 * 1,632 bytes over TEN ranges -- 0x65473, 0x6845e, 0x69815, 0x6af24,
 * 0x6b0e8, 0x6b138, 0x6d95f, 0x6fcb5, 0x6fd34 and 0x7170a -- and only 861 of
 * them are live.  The other 771, 47.2%, are the bodies of nine
 * `if (DSPLIB_DEBUG_ON())`, six of which are the setters' own transition
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
	struct v34_receiver *rx = &obj->receiver;

	V34agc(rx);					/* 0x65473 */

	/* 0x65486, `cmpw $0x34`: 52 is TX_L2, not TX_PHASE3_ANS. */
	if (obj->microstate != V34HS_TX_L2) {
		/*
		 * The object tests the other way: 0x65486 `cmpw $0x34` and
		 * `je 6845e` jump TO the guard, so 0x65494 is the not-equal
		 * fallthrough.
		 */

		return;
	}

	/* 0x6845e, sixteen bits and signed. */
	if (obj->vect_idx <= 0x5db) {

		return;
	}

	if (t41_detect(obj) == 0) {
		/*
		 * 0x6af24, and the arithmetic is THIRTY-TWO bits wide.  See
		 * the head of this function.
		 */
		if ((int)obj->vect_idx <= (int)obj->rtd + 0x2418) {

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
		hs_set_txstate(obj, V34HS_TX_DPSK);

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
		obj->paa6c = &obj->msgrec[2];		/* 0x6b1e3 */
		V34SetINFO1aBits(obj,			/* 0x6b1f0 */
				 (short *)&obj->msgrec[2]);

		/*
		 * 0x6b1fb IS FIVE STORES: +0x14 = 0xffff, +0x1a, +0x1e and
		 * +0x22 = 0, then +0x18 = 0x4d, all through the %ebp reloaded
		 * at 0x6b1f5.  The six that follow are in the object's order,
		 * +0x1c, +0x16, +0x28, +0x2a, +0x20 -- and 0x6f669..0x6f6b3
		 * writes the same twelve offsets in the same order with 0x26
		 * at +0x18 (finding F443).
		 */
		t41_record_head(&obj->msgrec[2], 0x4d);     /* 0x6b1fb */
		obj->msgrec[2].wordbits = 8;		    /* 0x6b219 */
		obj->msgrec[2].crc_on = 1;		    /* 0x6b21f */
		obj->msgrec[2].avail = 0x10;		    /* 0x6b225 */
		obj->msgrec[2].avail0 = 0x10;		    /* 0x6b22b */
		obj->msgrec[2].repeat = 0;		    /* 0x6b231 */
		/*
		 * THIRTY-TWO BITS, both of them: `movl $0xff72` and not
		 * `movw`, so the immediate is +65394 and the halfword above
		 * each is zeroed too.
		 */
		obj->msgrec[2].acc = 0xff72; /* 0x6b23e */
		obj->msgrec[2].acc0 = 0xff72; /* 0x6b245 */

		/*
		 * 0x6b24c and the 0x26 arm's 0x6f6ba are the same seven
		 * bytes, `mov %di,0x358c(%esi)`, and both clear +0x358c
		 * immediately before a state move.  The word that move
		 * compares was loaded at 0x6b237, in the middle of the record
		 * fill: `cmp $0x2b` -- RX_DPSK is 43 -- is at 0x6b253.
		 */
		obj->short_358c = 0;		/* 0x6b24c */

		hs_set_rxstate(obj, V34HS_RX_DPSK);
		hs_set_microstate(obj, V34HS_INFODONE);

		/* 0x6b379: the SECOND record, and INFO1a's length. */
		obj->paa70 = &obj->msgrec[3];
		obj->msgrec[3].nbits = 0x26;

		/*
		 * 0x6b3a2, and it reads the record through `%ebx` -- obj +
		 * 0xa9ac -- rather than through the pointer it has just
		 * aimed, which is the other half of the note above.
		 */
		if (DSPLIB_DEBUG_ON()) {
			const unsigned short *p =
			    (const unsigned short *)&obj->msgrec[2];

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
		hs_set_txstate(obj, V34HS_TONE_AB);
		hs_set_microstate(obj, V34HS_TX_PHASE3_ANS);
		hs_set_rxstate(obj, V34HS_RX_DPSK);
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
	obj->short_aa78 = 0;		/* 0x69982 */

	/*
	 * 0x69989 is an exit and not a call: `movzwl 0x3596(%ecx),%ecx` and
	 * `jmp 62af1`, where the txstate is sign-extended, has 5 subtracted
	 * and is bounds-checked against 0x45 before the dispatch.
	 */

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
 * the transmit-status join and TEN of the fifteen are the same nineteen-byte "read
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
/*
 * T4_RXF11E is the receiver's `short_382` (wave 9) -- the LOW half of the
 * TRN2 shift register whose high half the converted sites now read as
 * `rx->short_380`.  It keeps its helper because both its sites read it
 * UNSIGNED (`T4_U16`) and the member is signed; the extension is the
 * site's, not the field's (finding F11350).
 */
#define T4_RXF11E	((int)__builtin_offsetof(struct v34_receiver, \
							short_382))
#define T4_RXF120	((int)__builtin_offsetof(struct v34_receiver, \
					    vectpp_idx))
	/* receiver: cleared when either matches; the member's own
	   offset (issue #260 wave 11)   */
#define T4_PLLCNT	0x25da	/* short: obj+0x221c+0x3be, the arm's own
				   counter -- 0x4c(%esp) plus 0x3be        */
#define T4_MPCOEF	0x2a68	/* twelve shorts, `getMPrecvdBits`' other
				   half (docs/v90cpp.md, finding F227)      */
#define T4_MDLEN	((int)__builtin_offsetof(struct v34_object, \
							md_length))
	/* short: MD's length in bauds; the SIGNED sites read the member
	   (`obj->md_length`), these three stay helper reads UNSIGNED   */
/*
 * THE MP PACKER IS msgrec[4] ITSELF (issue #260 wave 9): `T4_MPTBL` is the
 * record's `word[]`, `T4_MPRUN` its `.pos`, `T4_MPIDX` its `.idx`,
 * `T4_MPACC` its `.acc` and `T4_MPBITS` its `.avail`.  The SIGNED sites
 * read and write the members directly; what survives here is the UNSIGNED
 * reads and the 32-bit stores, whose extension is the site's (F11350) --
 * and T4_MPACC's sites are all `T4_U32`, so it is helper-only for keeps.
 */
#define T4_MPTBL	((int)__builtin_offsetof(struct v34_object, \
							msgrec[4].word))
#define T4_MPRUN	((int)__builtin_offsetof(struct v34_object, \
							msgrec[4].pos))
	/* short: the current run of one bits      */
#define T4_MPIDX	((int)__builtin_offsetof(struct v34_object, \
							msgrec[4].idx))
	/* short: which of the ten comes next      */
#define T4_MPACC	((int)__builtin_offsetof(struct v34_object, \
							msgrec[4].acc))
	/* int:   the bit accumulator              */
#define T4_MPBITS	((int)__builtin_offsetof(struct v34_object, \
							msgrec[4].avail))
	/* short: how many bits are in it          */
#define T4_MPCAPS	((int)__builtin_offsetof(struct v34_object, \
					    info_caps))
	/* THE LOW BYTE of `info_caps`; the three readers are all
	   `testb $1`, which is finding F553's case again; the
	   member's own offset (issue #260 wave 11) */

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
 * Both arms rejoin at 0x6ff6f, which is `hs_set_microstate(obj, DET_SYNC)`
 * and then 0x68c6e's two clears -- the same tail the packer's own
 * bit-16 path reaches, which is why one label serves both.
 *
 * THE TRANSITION'S "ALREADY THERE" BRANCH AT 0x6ff81 CAN NEVER FIRE.  This
 * arm is entered with rxstate 4 and the packer at 0x68c2b sends microstate
 * 41 to the run-length path instead, so every path that reaches here has a
 * microstate that is not 41.  the setter handles it anyway); finding F722's
 * case for the third time in this file.
 */
static unsigned short
t4_mp_sequence_end(struct v34_object *obj)
{
	struct v34_receiver *rx = &obj->receiver;
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
	hs_set_microstate(obj, V34HS_DET_SYNC);	/* 0x6ff6f */
	/*
	 * 0x68c6e and 0x68c78 are one shared pair of `movw $0x0` through %ebx
	 * = obj+0xaa0c, +0x1e and +0x1a.  0x6ff81's `je` lands on the first of
	 * them and the packer's resync falls into it from 0x68c67, which is
	 * why the same two addresses appear twice in this file.
	 */
	obj->msgrec[4].idx = 0;				/* 0x68c6e */
	obj->msgrec[4].pos = 0;				/* 0x68c78 */
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
	struct v34_receiver *rx = &obj->receiver;
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
		obj->short_aa78 = (short)(obj->short_aa78 + 1);	/* 0x6ff1e */

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
	struct v34_receiver *rx = &obj->receiver;
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
	obj->short_aa78 = 0;
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34MP - E sequence detected,"
				     "rxflgs= 0x%x\n", (unsigned)rx->flags);

	/*
	 * 0x711ee.  The argument is built by `lea 0x0(%ebp,%ebp,4)` and `add
	 * %eax,%eax` -- times five, then doubled -- and the `cwtl` at 0x711e9
	 * cuts the product to sixteen bits before sign-extending it again,
	 * which is what the (short) is.  +0x1d0 itself is read `movswl`.
	 */
	VPcmV34LogTimingOffset(obj, (short)(rx->timing_ppm * 10));	/* 0x711ee */

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
	if (obj->short_3598 == 0) {				/* 0x71216 */
		initdigital(obj);
		obj->short_3598 = 1;
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
	struct v34_receiver *rx = &obj->receiver;

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
				hs_set_microstate(obj, V34HS_DET_INFO);
				obj->short_aa78 = 0;
				obj->msgrec[4].idx = 0;
				obj->msgrec[4].pos = 0;
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
	struct v34_receiver *rx = &obj->receiver;
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
	hs_set_microstate(obj, V34HS_DET_SYNC);
	obj->msgrec[4].idx = 0;				/* 0x68c6e */
	obj->msgrec[4].pos = 0;				/* 0x68c78 */
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
	struct v34_receiver *rx = &obj->receiver;
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
				     | (rx->short_380 >> 2));
		rx->short_380 = a;
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
	a = rx->short_380;
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
		rx->short_380 = b;			/* 0x67a17 */
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
		rx->short_380 = b;			/* 0x6b49b */
		return flags;
	}

	/*
	 * 0x6fe67.  BOTH registers are cleared, not only the one that matched,
	 * and the bit is OR'ed into the value read back at 0x679c0 rather than
	 * into a fresh load of +0x122.  The timing offset is built the same
	 * way as at 0x711e3: times five, doubled, `cwtl`.
	 */
	/* 0x6fe67 */
	rx->short_380 = 0;
	T4_U16(rx, T4_RXF120) = 0;
	rx->flags = (unsigned short)(f | V34_RX_FLAG_LATE_TRN);
	VPcmV34LogTimingOffset(obj, (short)(rx->timing_ppm * 10));
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
	struct v34_receiver *rx = &obj->receiver;
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
	obj->rate_change_pending = 0;			/* 0x6a22c */
	detectorinit(&obj->detector,
		     obj->det_coefs,
		     0, 10, 0x28, 0x600, 0);			/* 0x6a25d */
	/*
	 * 0x6a276 raises 0x200 for the SECOND time in this function -- the
	 * inlined `setupreceiver` already raised it at 0x6a218 -- and 0x6a283
	 * then clears +0x35a2, the word the entry test at 0x6a097 reads, so
	 * the wait runs once and the next block falls straight through.
	 */
	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);
	obj->md_length = 0;				/* 0x6a283 */
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
	int v = obj->rtd_scaled;
	short baud = obj->ratecfg.rx_baud;

	if (baud == 0xd65)
		obj->rtd_scaled = (v * 0xb3) >> 8;
	else if (baud == 0xc80)
		obj->rtd_scaled = (v * 0xc0) >> 8;
	else if (baud == 0xbb8)
		obj->rtd_scaled = (v * 205) >> 8;
	else if (baud == 0xaf0)
		obj->rtd_scaled = (v * 0xdb) >> 8;
}

/*
 * 0x67999 -- everything the arm does once the retrain test at the entry has
 * declined.
 */
static void
t4_receive_body(struct v34_object *obj, unsigned short flags)
{
	struct v34_receiver *rx = &obj->receiver;
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
	    && obj->role == 0x65) {
		/*
		 * 0x68e50 is the body, not a call: `settxlevel` against the
		 * +0xa9dc record, `V34SetupModulator` on the three shorts at
		 * +0xaa84, +0xaa94 and +0xaa8a, +0x122 &= ~0x800, +0x3594
		 * driven to 0x23 and txstate +0x3596 to 0x12 -- each behind
		 * the setters' compare-then-trace -- +0x25c0 cleared with
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
		 * 62af1`, the tail into the transmit-status join with the tx state word
		 * already in %ecx. 0x69b3c, 0x6a291, 0x6d155 and 0x67b90 all
		 * end the same way.
		 */
		return;						/* 0x686f1 */

	/*
	 * 0x67a5b.  `cmpw $0x3,0x3570(%ecx)` straight against memory, and the
	 * 3 is the one 0x67b22 stores when the tone is detected: the arm sets
	 * the state it later tests.
	 */
	if (obj->detector.state == 3) {			/* 0x67a5b */
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
	if (obj->rate_change_pending != 0) {
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
	obj->detector.count = 0x1e;				/* 0x67b17 */
	obj->detector.state = 3;
	obj->detector.level = 0;
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
		short md = (short)(((((int)obj->ratecfg.rx_baud * 0x23d) >> 14)
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
	obj->rtd_scaled = rx->timing_offset;		/* 0x67b84 */
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

	rx = &obj->receiver;
	/*
	 * 0x653f7.  `movzwl 0x122(%edx),%esi` -- read once, and %esi carries
	 * it into the whole arm as the working copy -- 0x67999 reads it as
	 * `flags` -- while the retrain test below works on a second copy, in
	 * %ebx.
	 */
	flags = rx->flags;				/* 0x653f7 */

	if ((flags & V34_RX_FLAG_RETRAIN) != 0
	    || ((int)rx->rx_blocks > 7 * (int)obj->ratecfg.rx_baud
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
 * another one, so every path ends in the transmit-status join.
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
 * ELEVEN HUNDRED BYTES ARE DIAGNOSTICS and 894 of that is the setters'
 * own transition trace at eight sites, three of which can never fire: the
 * arm is entered from `62b7a: cmp $0x48,%eax; je 650c6` with rxstate at 72
 * and nothing between entry and 0x67e1e, 0x65162 or 0x6ca01 writes +0x3594.
 * Writing the setters reproduces all eight for free; unlike rxstate 4
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

/*
 * The six measurement slots t72_measure fills (issue #260 wave 9) are now
 * the members `nl_noise`/`nl_signal`/`nl_ratio` and `pb_noise`/`pb_signal`/
 * `pb_ratio`; this arm keeps its offset spellings because it hands them to
 * `t72_measure` as parameters.  The FSK gate (+0xa8a0) is `retrain_poll`,
 * the +0xaacc slot is `rtd_scaled`, the burst is `rx->burst` and the
 * receiver's timing pair is `timing_offset` -- all converted at their
 * direct sites.
 */
#define T72_FSKGATE	((int)__builtin_offsetof(struct v34_object, \
							retrain_poll))
#define T72_NL_NOISE	((int)__builtin_offsetof(struct v34_object, \
							nl_noise))
	/* int:   the 0x180 rung's noise total      */
#define T72_NL_SIGNAL	((int)__builtin_offsetof(struct v34_object, \
							nl_signal))
	/* int:   and its signal total              */
#define T72_PB_NOISE	((int)__builtin_offsetof(struct v34_object, \
							pb_noise))
	/* int:   the 0x300 rung's noise total      */
#define T72_PB_SIGNAL	((int)__builtin_offsetof(struct v34_object, \
							pb_signal))
	/* int:   and its signal total              */
#define T72_NL_RATIO	((int)__builtin_offsetof(struct v34_object, \
							nl_ratio))
	/* int:   round(256 * signal / noise)       */
#define T72_PB_RATIO	((int)__builtin_offsetof(struct v34_object, \
							pb_ratio))
	/* int:   the same, for the 0x300 rung      */

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
	return (short)(rx->rx_samples - rx->burst);
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
	const short *in = rx->burst;

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
	obj->rtd_scaled = rx->timing_offset;		/* 0x67d92 */

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
			  rx->burst, n);
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
	struct v34_receiver *rx = &obj->receiver;
	short nbits, microstate, tx;
	/* nbits precedes demodulation; microstate selects its dispatch.
	 * tx is carried to the status tail until an explicit reload join. */
	const short *fskin;
	short rxst;

	/* 0x62933: fill the transmit queue one symbol at a time.
	 * The native dispatch is .rodata+0x2da0 (states 5..86). Active arms
	 * reload the target at their loop rejoins. Unsupported states instead
	 * reach 0x629e0 with the target still in DX and make no progress.
	 * This C condition also makes no progress there; its load placement
	 * remains a difference in generated control flow, not a recovered snapshot.
	 */
	while (obj->txq.count < obj->tx_fill_target) {
		switch ((int)obj->txstate) {
		case V34HS_SILENCE:		/* 5  0x640b4, three tails */
		case V34HS_SILENCEINFO:		/* 54 */
		case V34HS_SILENCERETRAIN:	/* 74 */
			{
				short quiet[4];
				short txst;
				unsigned short n;

				quiet[0] = 0;
				quiet[1] = 0;
				quiet[2] = 0;
				quiet[3] = 0;
				obj->short_aa7a = 0;
				txwritequeue(&obj->txq, quiet);

				txst = obj->txstate;

				if (txst == V34HS_SILENCERETRAIN) {
					/* 0x66b87 */
					short want;

					n = (unsigned short)((unsigned short)obj->vect_idx + 1);
					obj->vect_idx = (short)n;
					if (n != 0xb4)
						goto tx_sample_done;		/* 0x63da2 */

					/* 0x66ba1 */
					if (obj->moh_active != 0)
						want = V34HS_MOH_TONE;
					else					/* 0x68a7c */
						want = obj->role == 0x65 ? V34HS_RX_PHASE1_CALL
									: V34HS_TX_PHASE1_ANS;
					hs_set_microstate(obj, want);

					/* 0x66be0 */
					hs_set_rxstate(obj, V34HS_RX_DPSK);
					/* 0x66c10 */
					hs_set_txstate(obj, V34HS_TONE_AB);

					/* 0x66c32 */
					obj->short_358c = 0;
					obj->vect_idx = 0;
					obj->short_aa78 = 0;
					obj->fsk.sr = -1;
					obj->fsk.nbits = 0;
					if (dsplibs_debug_level > 1)		/* 0x66c72 */
						dsplibs_debug_printf(
							"V34RETRAIN, SILENCERETRAIN finished," " rx->rxflgs,=0x%x,rx->gain=0x%x,"
							"gainestimate=0x%x\n",
							rx->flags, rx->agc_gain, rx->agc_start_gain);
					goto tx_sample_done;			/* 0x63941 */
				}

				if (txst != V34HS_SILENCEINFO)
					goto tx_sample_done;			/* 0x6409a */

				n = (unsigned short)((unsigned short)obj->vect_idx + 1);
				obj->vect_idx = (short)n;
				if (n != 0xa)
					goto tx_sample_done;			/* 0x629cf */

				/* 0x6858f */
				hs_set_txstate(obj, V34HS_TX_DPSK);
				obj->vect_idx = 0;
				obj->paa6c = obj->msgrec;
				V34SetINFO0aBits(obj, (short *)obj->msgrec);

				{
					/*
					 * RE-READ, not the pointer just stored, because that is what
					 * the object does at 0x685f2 and 0x69fe1 -- the same reading
					 * V34hshak.c:1459 records for `v34handshakinit`'s copy of
					 * this sequence.  `V34SetINFO0aBits` does not write +0xaa6c,
					 * so nothing observable turns on it.
					 */
					struct v34_bitsource *r = (struct v34_bitsource *)obj->paa6c;
					short lead = (obj->role == 0x65 && obj->v90_receiver != 0)
						     ? 0x1e : 0x11;

					r->crc = -1;
					r->pos = 0;
					r->idx = 0;
					r->repeats = 0;
					r->nbits = lead;
					/* 0x68616 */
					r->wordbits = 8;
					r->crc_on = 1;
					r->avail = 0x10;
					r->avail0 = 0x10;
					r->repeat = 0;
					r->acc = 0xff72;
					r->acc0 = 0xff72;
				}

				obj->short_358c = 0;
				goto tx_sample_done;				/* 0x640a1 */
			}
		case V34HS_SSEG:		/* 18 0x64048 */
			{
				unsigned short n;

				tx1_put_point(obj, vect4[0]);
				txmit(obj);
				tx1_put_point(obj, vect4[3]);
				txmit(obj);

				n = (unsigned short)((unsigned short)obj->seg_symcount + 1);
				obj->seg_symcount = (short)n;
				if (n == 0x40) {
					/* 0x66d11 */
					hs_set_txstate(obj, V34HS_SBARSEG);
					obj->seg_symcount = 0;
				}
				goto tx_sample_done;
			}
		case V34HS_SBARSEG:		/* 19 0x6296d */
			{
				unsigned short n;

				tx1_put_point(obj, vect4[2]);
				txmit(obj);
				tx1_put_point(obj, vect4[1]);
				txmit(obj);

				n = (unsigned short)((unsigned short)obj->seg_symcount + 1);
				if (n != 8) {
					obj->seg_symcount = (short)n;
					goto tx_sample_done;		/* 0x629c8 */
				}

				/* 0x671f0 */
				obj->seg_symcount = 0;
				if ((unsigned short)obj->tx_flags & V34_TXFLAG_SEG4A) {
					hs_set_txstate(obj, V34HS_TRNSEG4A);
				} else if (obj->short_35a4 == 0) {
					/* 0x68375 */
					hs_set_txstate(obj, V34HS_PPSEG);
				} else if ((unsigned short)obj->tx_flags & V34_TXFLAG_PPSEG) {
					/* 0x69eca */
					obj->seg_symcount = obj->seg_len;
					hs_set_txstate(obj, V34HS_PPSEG);
				} else {
					/* 0x6783e */
					int span;
					short q;

					obj->seg_len = (short)((unsigned short)obj->short_35a4 * 0x53);
					hs_set_txstate(obj, V34HS_TXMD);

					/* 0x67885 */
					V34SetupModulator(v34_object_modulator(obj),
							  4800, 2400, 0, 0, 0);

					/*
					 * `(0x5e8 - good_run) << 14` with the subtraction done in an int.
					 * Spelled through `unsigned` because the left shift of a
					 * negative int is undefined in C and the object's `shl` is
					 * not; the bits are the same either way.
					 */
					span = (int)((unsigned)(0x5e8 - obj->dmadelay) << 14);
					q = (short)(span / 9600);
					obj->short_aa78 = (short)((((int)q * 0x960) >> 14) + 0x96);
					if (dsplibs_debug_level > 1)		/* 0x67916 */
						dsplibs_debug_printf(
							"Moving to TX MD, would take %d symbols ,"
							" echo start delay is %d...\r\n",
							obj->seg_len,
							obj->short_aa78);
				}

				/* 0x67236 */
				obj->vect_idx = 0;
				obj->short_358e = 0;
				goto tx_sample_done;			/* 0x63948 */
			}
		case V34HS_PPSEG:		/* 20 0x642bf */
			{
				int i = obj->vect_idx;			/* movswl 0x2aa2 */
				int point;

				memcpy(&point, &vectpp[2 * i], sizeof(point));
				obj->vect_idx = (short)(i + 1);
				tx1_put_point(obj, point);
				txmit(obj);

				if ((unsigned short)obj->vect_idx == (unsigned short)V34_VECTPP_POINTS) {
					/* 0x66d57 */
					unsigned short n = (unsigned short)
						((unsigned short)obj->short_358e + 1);

					if (n != 6) {
						obj->short_358e = (short)n;
						obj->vect_idx = 0;
					} else {
						/* 0x680ac */
						struct v34_ratecfg *cfg = &obj->ratecfg;
						short v = (short)((unsigned short)obj->rtd + 0x90);
						short baud = cfg->baud;
						int span, q, acc;

						obj->vect_idx = v;
						switch ((unsigned short)baud) {
						case 2400:
							obj->ratecfg.period = (short)(v >> 2);
							break;
						case 2800:
							obj->ratecfg.period =
								(short)(((int)v * 0x12ab) >> 14);
							break;
						case 3000:
							obj->ratecfg.period =
								(short)(((int)v * 0x1400) >> 14);
							break;
						case 3200:
							obj->ratecfg.period =
								(short)(((int)v * 0x1555) >> 14);
							break;
						case 3429:
							obj->ratecfg.period =
								(short)(((int)v * 0x16dc) >> 14);
							break;
						default:
							break;
						}

						/* 0x680fd */
						obj->short_358e = 0;
						obj->ratecfg.period =
							(short)((unsigned short)obj->ratecfg.period
								+ 0x120);
						hs_set_txstate(obj, V34HS_TRNSEG4);

						/* 0x68154 */
						span = (0x5e8 - obj->dmadelay) * (int)baud;
						q = span / 9600;
						obj->short_aa78 = (short)q;	/* 0x6818d */

						acc = q + (unsigned short)obj->filtdelay
							+ (unsigned short)obj->seg_symcount + 1;
						if (obj->role == 0x65)			/* 0x6926d */
							acc += (unsigned short)obj->rtd;
						obj->short_aa78 = (short)acc;

						/* 0x681c6 */
						obj->ratecfg.period =
							(short)((unsigned short)obj->ratecfg.period
								+ (unsigned short)obj->short_aa78);
						if (dsplibs_debug_level > 1)	/* 0x681ef */
							dsplibs_debug_printf(
								"V34Hshak: echo start wait time would" " be: NEC %d symbols, FEC %d"
								" symbols...\r\n",
								obj->short_aa78,
								obj->ratecfg.period);
						goto tx_sample_done;		/* 0x63da2 */
					}
				}

				/* 0x6430c */
				goto tx_segment_count;			/* 0x6431f */
			}
		case V34HS_TRNSEG4:		/* 21 0x64339 */
			{
				struct v34_ratecfg *cfg = &obj->ratecfg;
				short q;
				int n, span, baud;

				/* 0x64339: the scrambler, the quadrant and one sample. */
				q = tx1_scramble2(obj);
				obj->cur_quadrant = q;
				tx1_put_point(obj, vect4[q]);
				txmit(obj);

				/* 0x643dc */
				obj->seg_symcount = (short)((unsigned short)obj->seg_symcount + 1);

				if (obj->short_aa78 == obj->seg_symcount) {
					/* 0x67613, and this one FALLS BACK INTO THE CHAIN */
					obj->echo_calls = 0;
					obj->tx_flags = (short)((unsigned short)obj->tx_flags & ~V34_EC_FROZEN);
					obj->echo_alpha = 0;
					obj->far_echo_alpha = 0;
					obj->echo_energy = 0;
					VPcmV34ReportStartOfEchoAdapt(obj);
				}

				/* 0x643f8 */
				if (((unsigned short)obj->tx_flags & 0x100u) == 0
				    && obj->seg_symcount >= obj->ratecfg.period)
					obj->tx_flags = (short)((unsigned short)obj->tx_flags | 0x100u);

				/* 0x64ae4 */
				n = obj->seg_symcount;
				span = obj->phase3_hd_length;

				if (n == (span >> 1)) {
					VPcmV34ReportMiddleOfEchoAdapt(obj);	/* 0x67734 */
					goto tx_sample_done;			/* 0x640a1 */
				}

				baud = cfg->baud;
				if (n == baud + (baud >> 1)) {
					/* 0x67278 */
					obj->tx_flags = (short)(((unsigned short)obj->tx_flags & ~V34_TXFLAG_PPSEG)
							   | 0x400u);
					goto tx_sample_done;			/* 0x63da2 */
				}

				if (n != span)
					goto tx_sample_done;			/* 0x629c8 */

				/* 0x64b24: the segment is over. */
				hs_set_txstate(obj, V34HS_JTXMIT);

				if (obj->rtd <= 2) {
					/* 0x6801e */
					v34FreezeEcho(obj);
					obj->short_aa78 = 0;
				} else {
					if (obj->v90_receiver != 0 || obj->k56flex_receiver != 0) {
						/* 0x68ad0 and 0x69de7, two copies of one store */
						obj->short_aa78 = obj->rtd;
					} else {
						/* 0x64b96 */
						obj->short_aa78 = (short)(obj->rtd >> 1);
					}

					/*
					 * 0x64ba9, and the nesting is what the object says: BOTH
					 * stores jump here (0x68ade and 0x69df5) and the `rtd <= 2`
					 * arm does not -- it reports the freeze instead.  An
					 * else-if chain cannot express a guard shared by two of its
					 * three arms.
					 */
					if (dsplibs_debug_level > 1)		/* 0x6917e */
						dsplibs_debug_printf(
							"V34Hshak: On J TX start, would freeze EC" " after bulk delay (%d samples,"
							" bulk=%d)\r\n",
							obj->short_aa78, obj->rtd);
				}

				/* 0x64bb2 */
				tx1_put(obj, TX1_F25D6, (short)0x8990);
				obj->prev_quadrant = obj->cur_quadrant;
				hs_set_rxstate(obj, V34HS_RECEIVE);
				setupreceiver(obj);				/* 0x64c04 */

				/* 0x64d37: the +0xaa0c record, in the object's order */
				{
					unsigned char *r = (unsigned char *)obj + TX1_INFOREC;

					*(int *)(r + 0x24) = 0;
					*(int *)(r + 0x2c) = 0;
					*(short *)(r + 0x14) = -1;
					*(short *)(r + 0x1a) = 0;
					*(short *)(r + 0x1e) = 0;
					*(short *)(r + 0x22) = 0;
					*(short *)(r + 0x18) = 0;
					*(short *)(r + 0x1c) = 0;
					*(short *)(r + 0x16) = 0;
					*(short *)(r + 0x28) = 0;
					*(short *)(r + 0x2a) = 0;
					*(short *)(r + 0x20) = 0;
				}

				/* 0x64d8d */
				hs_set_microstate(obj, V34HS_DET_SYNC);

				/* 0x64dba */
				obj->vect_idx = 0;
				obj->rate_change_pending = 1;

				if ((unsigned)obj->v90_receiver > 1u) {
					/* 0x64de7 */
					hs_set_txstate(obj, V34HS_JaTXMIT);
					hs_set_rxstate(obj, V34HS_WAIT);
					indicateJaTransmission(obj);
					goto tx_sample_done;			/* 0x629c8 */
				}

				if ((unsigned)obj->k56flex_receiver > 1u) {
					/* 0x67f91 */
					hs_set_txstate(obj, V34HS_K56JaTXMIT);
					hs_set_rxstate(obj, V34HS_WAIT);
					indicateJaTransmission(obj);
					goto tx_sample_done;			/* 0x63948 */
				}

				goto tx_sample_done;				/* 0x63da2 */
			}
		case V34HS_TX_DPSK:		/* 24 0x62b96 */
			{
				short bit;

				/* 0x62b9d */
				if (obj->moh_active != 0)
					obj->vect_idx = (short)((unsigned short)obj->vect_idx + 1);

				/* 0x62bb5, and 0x684bb where it does not inline */
				bit = getbit(tx1_bitsource(obj));
				if (bit >= 0) {
					/* 0x64a13 */
					tx1_dpsk_tone(obj, bit);
					txmit(obj);				/* 0x64a42 */
					goto tx_sample_done;			/* 0x640a1 */
				}

				/* 0x64e5c: the message is over.  The flag is RE-READ (0x64e63) */
				if (obj->moh_active == 0) {
					/*
					 * 0x654dd.  The compare against TONE_AB cannot be false, for
					 * finding F342's reason at 65: the arm is reached only through
					 * table 1 at `txstate == 24` and nothing between the dispatch
					 * and here writes +0x3596 -- `getbit` does not.  It is
					 * written as the object writes it.
					 */
					hs_set_txstate(obj, V34HS_TONE_AB);
					goto tx_sample_done;			/* 0x63948 */
				}

				if (dsplibs_debug_level > 1)			/* 0x68704 */
					dsplibs_debug_printf(
						"End of current MOH msg: isterm=%d, count1(%d)," " pktcount(%d)...\r\n",
						*((signed char *)obj + TX1_MOH_MSG_PENDING), obj->vect_idx,
						tx1_bitsource(obj)->repeats);

				if (obj->moh_msg_pending == 0) {
					/*
					 * 0x67c4e.  The reader is re-armed BY HAND and read again --
					 * `getbit`'s own restart arm with the `repeat` test taken out,
					 * so a reader that declines to repeat is restarted anyway.
					 * The stores are the object's, in the object's order; the
					 * compiler sank the rest of them into the successors, which is
					 * why 0x6a855 and 0x6c9c8 store 0xffff into `crc` again.
					 */
					struct v34_bitsource *b = tx1_bitsource(obj);

					b->repeats = (short)((unsigned short)b->repeats + 1);
					b->crc = (short)0xffff;
					b->pos = 0;
					b->idx = 0;
					b->acc = b->acc0;
					b->avail = b->avail0;

					tx1_dpsk_tone(obj, getbit(b));
					txmit(obj);				/* 0x67d3e */
					tx1_moh_hold(obj);
				goto tx_sample_done;			/* 0x64fec */
				}

				/*
				 * 0x64e91.  Both selectors are read as ints and compared UNSIGNED --
				 * `cmp $0x1 ; je ; jb` picks zero alone and `cmp $0x3 ; ja` sends a
				 * negative one to the same place a large one goes.
				 */
				if (obj->moh_message == 1) {
					tx1_moh_on_hold(obj);			/* 0x689f6 */
				} else if (obj->moh_message == 0) {
					int got = obj->moh_recvd;			/* 0x6901d */

					if (got == 4)
						tx1_moh_on_hold(obj);
					else if ((unsigned)got > 4u) {
						/* 0x6a3c6 */
						if (got != 5)
							tx1_moh_reinit(obj, MOH_ILLEGAL);	/* 0x69041 */
						else
							tx1_moh_send(obj);	/* 0x6a3cf */
					} else if (got == 0)
						tx1_moh_on_hold(obj);
					else
						tx1_moh_reinit(obj, MOH_ILLEGAL);	/* 0x69041 */
				} else if ((unsigned)obj->moh_message <= 3u) {
					tx1_moh_cleardown(obj);			/* 0x64eaf */
				}

				/* 0x64fde */
				obj->moh_msg_pending = 0;
				tx1_moh_hold(obj);
				goto tx_sample_done;				/* 0x64fec */
			}
		case V34HS_TX_L1:		/* 51 0x62c69 */
			{
				/* Arm 51 is the one arm here that reads a state word belonging to
				 * another machine: it selects between two copies of its own loop on
				 * `microstate == TX_L1`, having been dispatched on
				 * `txstate == TX_L1`.  The two 51s are the same number in two
				 * machines and `StateName` is one table for all three
				 * (docs/v34handshak.md). */
				int doubled = obj->microstate == V34HS_TX_L1;
				short buf[4];
				int i;

				for (i = 0; i < 4; i++) {
					unsigned idx = (unsigned short)obj->vect_idx;
					int v = probe[idx & 0x3f];

					if (doubled)
						v = (short)(2 * v);
					buf[i] = (short)((v * obj->tx_scale) >> 14);
					obj->vect_idx = (short)(idx + 1);
				}

				txwritequeue(&obj->txq, buf);

				if (obj->microstate != V34HS_TX_L1)
					goto tx_sample_done;
				if ((unsigned short)obj->vect_idx != 0x600u)
					goto tx_sample_done;

				hs_set_microstate(obj, V34HS_TX_L2);
				/* 0x62d32 */
				obj->vect_idx = 0;
				goto tx_sample_done;
			}
		case V34HS_TONE_AB:		/* 60 0x62d3d */
			{
				int sel = obj->short_358c & 1;

				tx1_put_point(obj, vect4[2 * sel]);
				txmit(obj);
				goto tx_sample_done;
			}
		case V34HS_JTXMIT:		/* 64 0x635cc, one body two tails */
		case V34HS_J1TXMIT:		/* 68 */
			{
				unsigned shift;
				unsigned short idx;
				short bits, mode, q;

				/* 0x635cc, and the completion at 0x6533c falls back into 0x635f0 */
				(void)tx1_ja_common(obj, "V34Hshak: on JTXMIT - time to freeze"
							   " echo...\r\n");		/* 0x682d6 */

				/* 0x635f0 */
				tx1_put(obj, TX1_F25D8,
					(short)((unsigned short)tx1_get(obj, TX1_F25D8) + 1));

				shift = (unsigned)(2 * (int)obj->vect_idx) & 31u;
				bits = (short)((unsigned)(unsigned short)tx1_get(obj, TX1_F25D6) >> shift);

				mode = (short)((obj->tx_flags & V34_TXFLAG_CALLER) == 0);
				q = (short)V34scrambler((unsigned *)&obj->tx_scr_sr, mode, bits, 2);
				(void)tx1_dpsk4(obj, q);
				txmit(obj);

				idx = (unsigned short)(((unsigned short)obj->vect_idx + 1) & 7);
				obj->vect_idx = (short)idx;

				if (obj->role == 0x66) {
					/* 0x66ca8 */
					if (!(rx->flags & V34_RX_FLAG_DATA)) {
						if (obj->md_length == 0)
							goto tx_sample_done;	/* 0x629c8 */
						if (tx1_get(obj, TX1_F25DA) == 0)
							goto tx_sample_done;	/* 0x63da2 */
					}
					/* 0x66cd1 */
					hs_set_txstate(obj, V34HS_XMIT0);
					obj->vect_idx = 0;			/* 0x62d32 */
					goto tx_sample_done;			/* 0x62d70 */
				}

				/* 0x636f0 */
				if (idx != 0)
					goto tx_sample_done;			/* 0x63941 */

				if (obj->txstate == V34HS_J1TXMIT) {
					/* 0x65653 */
					obj->tx_scr_sr = 0;
					hs_set_txstate(obj, V34HS_TRNSEG4A);
					obj->prev_quadrant = 0;
					obj->seg_symcount = 0;
					goto tx_sample_done;			/* 0x640a1 */
				}

				/* 0x6370d */
				if (tx1_get(obj, TX1_F25DA) != 2)
					goto tx_sample_done;			/* 0x6409a */
				if (tx1_get(obj, TX1_F25D8) <= 0x80)
					goto tx_sample_done;			/* 0x6431f */

				/* 0x6372e */
				tx1_put(obj, TX1_F25D6, (short)0x899f);
				hs_set_txstate(obj, V34HS_J1TXMIT);

				/* 0x637c8 */
				if (obj->short_aa78 != 0) {
					if (dsplibs_debug_level > 1)		/* 0x6c725 */
						dsplibs_debug_printf(
							"V34Hshak: on J1TXMIT - forced freeze echo" " (count2 = %d)...\r\n",
							obj->short_aa78);
					v34FreezeEcho(obj);
					obj->short_aa78 = 0;
					goto tx_sample_done;			/* 0x64326 */
				}
				goto tx_sample_done;				/* 0x629c8 */
			}
		case V34HS_XMIT0:		/* 65 0x62d83 */
			{

				obj->txpoint.c[0] = 0;
				obj->txpoint.c[1] = 0;
				txmit(obj);

				if (rx->flags & V34_RX_FLAG_LATE_TRN) {
					obj->tx_flags = (short)((unsigned short)obj->tx_flags | V34_TXFLAG_SEG4A);
					hs_set_txstate(obj, V34HS_SSEG);
					obj->prev_quadrant = 0;
					obj->seg_symcount = 0;
					obj->tx_scr_sr = 0;
				}
				goto tx_sample_done;
			}
		case V34HS_TRNSEG4A:		/* 66 0x62e28 */
			{
				struct v34_ratecfg *cfg = &obj->ratecfg;
				int n, lim, baud, period;
				short *rec;

				if ((unsigned short)obj->receiver.short_382 == 0x89b0u) {
					/* 0x66e59 */
					short k = tx1_scramble2(obj);
					short q;

					obj->cur_quadrant = k;
					q = tx1_scramble2(obj);
					tx1_put_point(obj, vect16[q + 4 * k]);
				} else {
					/* 0x62e3b */
					short q = tx1_scramble2(obj);

					obj->cur_quadrant = q;
					tx1_put_point(obj, vect4[q]);
				}

				/* 0x62ecd */
				txmit(obj);

				/* 0x62edc */
				obj->seg_symcount = (short)((unsigned short)obj->seg_symcount + 1);
				n = obj->seg_symcount;
				baud = cfg->baud;
				period = cfg->period;
				lim = baud + (baud >> 1) + period;

				if (n != lim) {
					if ((unsigned)obj->hs_mode <= 3u)
						goto tx_sample_done;		/* 0x63941 */
					if (n < lim) {
						/* 0x6559c */
						if (n < baud + period)
							goto tx_sample_done;	/* 0x6409a */
						if ((int)rx->equerr > (int)obj->receiver.short_4b4 + 10)
							goto tx_sample_done;	/* 0x6431f */
					}
				}

				/* 0x62f22 */
				rec = (short *)((char *)obj + TX1_FAA3C);
				obj->prev_quadrant = (short)(unsigned short)obj->cur_quadrant;
				obj->paa6c = rec;

				if (rx->flags & V34_RX_FLAG_RENEG) {
					/* 0x62f5f */
					rec[2] = 0;
					rec[0] = 0;
					obj->receiver.short_4b4 = rx->equerr;
				} else {
					tx1_ts_snapshot(obj, rx, rec);
				}

				tx1_ts_rates(obj, rx, cfg, rec);
				goto tx_sample_done;
			}
		case V34HS_XMITMP:		/* 67 0x6399b */
			{
				int wide = 0;
				int n = 0;

				obj->cur_quadrant = 0;					/* 0x639ab */

				for (;;) {
					struct v34_bitsource *b;
					unsigned short flags;
					short bit, idx;

					wide = (unsigned short)obj->receiver.short_382 == 0x89b0u;
					if (n > (wide ? 3 : 1))
						break;

					/* 0x639db, and 0x6484c where it does not inline */
					b = tx1_bitsource(obj);
					bit = getbit(b);

					/*
					 * 0x63ab1.  `n` reaches the shift through `movzbl`, and the
					 * bit reaches it sign-extended -- so the reader's -1 fills
					 * `cur_quadrant` from bit `n` up rather than setting one bit.
					 */
					obj->cur_quadrant = (short)((unsigned short)obj->cur_quadrant
							   | (unsigned short)
							     ((unsigned)(int)bit
							      << ((unsigned)(unsigned char)n & 31u)));

					/* 0x63ad8 */
					obj->vect_idx = (short)((unsigned short)obj->vect_idx + 1);
					idx = obj->vect_idx;
					flags = rx->flags;

					if (idx == ((flags & 0x20u) ? 0x55 : 0xbb)) {
						/* 0x6459b */
						b = tx1_bitsource(obj);
						if (flags & 0x20u) {
							b->acc = (int)((unsigned)b->acc << 3);
							b->avail = (short)
								   ((unsigned short)b->avail + 3);
						} else {
							b->acc = (int)((unsigned)b->acc << 1);
							b->avail = (short)
								   ((unsigned short)b->avail + 1);
						}
					} else if (idx == ((flags & 0x20u) ? 0x58 : 0xbc)) {
						if (tx1_mp_sequence_end(obj, rx)) {
							/* 0x6490e */
							wide = (unsigned short)
							       obj->receiver.short_382 == 0x89b0u;
							break;
						}
					} else if ((unsigned short)obj->vect_idx
						   == (unsigned short)obj->short_3590) {
						/* 0x64727 */
						b = tx1_bitsource(obj);
						b->acc = (int)((unsigned)b->acc << 1);
						b->avail = (short)((unsigned short)b->avail + 1);
						obj->short_3590 =
							(short)((unsigned short)obj->short_3590 + 0x11);
					}

					n = (short)(n + 1);			/* 0x63b38 */
				}

				if (wide)
					tx1_mp16(obj, obj->cur_quadrant);
				else
					tx1_mp4(obj, obj->cur_quadrant);

				txmit(obj);				/* 0x62d5f and 0x64a42 */
				goto tx_sample_done;			/* 0x62d70 and 0x640a1 */
			}
		case V34HS_EXMIT:		/* 69 0x63858 */
			{
				short mode = (short)((obj->tx_flags & V34_TXFLAG_CALLER) == 0);
				short q;

				if ((unsigned short)obj->receiver.short_382 == 0x89b0u) {
					/* 0x67031 */
					short k;
					int point;

					q = (short)V34scrambler((unsigned *)&obj->tx_scr_sr, mode, 0xf, 2);
					obj->cur_quadrant = (short)((q + (unsigned short)obj->prev_quadrant) & 3);

					q = (short)V34scrambler((unsigned *)&obj->tx_scr_sr, mode, 3, 2);
					k = obj->cur_quadrant;			/* re-read, 0x67131 */
					obj->prev_quadrant = k;
					point = vect16[q + 4 * k];
					tx1_put_point(obj, point);
					txmit(obj);
					obj->vect_idx = (short)((unsigned short)obj->vect_idx + 4);
				} else {
					/* 0x6386b */
					q = (short)V34scrambler((unsigned *)&obj->tx_scr_sr, mode, 3, 2);
					(void)tx1_dpsk4(obj, q);
					txmit(obj);
					obj->vect_idx = (short)((unsigned short)obj->vect_idx + 2);
				}

				/* 0x6392c */
				if ((unsigned short)obj->vect_idx == 0x14u) {
					/* 0x66e01 */
					short txst = obj->txstate;

					obj->vect_idx = 8;
					hs_set_txstate(obj, V34HS_DATAXMIT);
					if (dsplibs_debug_level > 1)		/* 0x66e48 */
						dsplibs_debug_printf("V34MP- E transmit completed\n");
					goto tx_sample_done;			/* 0x6409a, 0x6431f */
				}
				goto tx_sample_done;				/* 0x63941 */
			}
		case V34HS_DATAXMIT:		/* 70 0x63ca8 */
			{
				int idx;

				modulatevector(obj);

				if (!(obj->tx_flags & PROG_TXBIT_DATA))
					goto tx_sample_done;

				rx->equerr_accum = 0;
				rx->err_symcount = 0;
				obj->int_0248 = obj->sample_count;

				idx = obj->ratecfg.rxbits;
				obj->rate_now = idx;
				obj->rate_want = idx;

				/*
				 * 0x63d11.  The data-mode transition reports both negotiated rates ONCE,
				 * and only when the machine was not already in data mode: +0x2218 is read
				 * at 0x63cf4 and a non-zero value jumps over the whole block (0x63d0f).
				 * The guard is therefore the mode test AHEAD of the debug gate, which is
				 * why the `test` precedes the `cmpl $0x1` and not the other way round.
				 *
				 * Both arguments are the rate INDEX times 2400 (0x960, the V.34 symbol
				 * rate at the host rate), the transmit index first: 0x63d1a's `imul` is
				 * on 0xaa88 and its sibling on 0xaa98, and 0xaa88 lands in the first
				 * vararg slot.  Finding F11444's residual, closed here.
				 */
				if (obj->hs_mode == 0 && DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V34DATA, getting into data mode from Handshake, "
						"Tx bit rate - %d, Rx bit Rate - %d\n",
						obj->ratecfg.txbits * 2400, idx * 2400);

				obj->hs_mode = 1;
				goto tx_sample_done;
			}
		case V34HS_TXLEVEL:		/* 71 0x641d1 */
			{

				obj->cur_quadrant = tx1_scramble2(obj);
				tx1_put_point(obj, vect4[0]);
				txmit(obj);
				goto tx_sample_done;
			}
		case V34HS_JaTXMIT:		/* 78 0x64139 */
			{

				(void)tx1_ja_common(obj, "V34Hshak: on JaTXMIT - time to freeze"
							   " echo...\r\n");		/* 0x6821d */
				v90Phase34(obj);
				goto tx_sample_done;
			}
		case V34HS_MOH_SILENCE:		/* 81 0x63d58, one behaviour */
		case V34HS_MOH_ON_HOLD:		/* 82 */
		case V34HS_MOH_FRR:		/* 83 */
		case V34HS_MOH_CLEARDOWN:	/* 84 */
			{
				short quiet[4];

				quiet[0] = 0;
				quiet[1] = 0;
				quiet[2] = 0;
				quiet[3] = 0;
				txwritequeue(&obj->txq, quiet);

				obj->vect_idx = (short)(obj->vect_idx + 1);
				if ((unsigned short)obj->vect_idx != 0xc0u)
					goto tx_sample_done;			/* 0x63da2 */

				/* 0x66d85 */
				if (obj->txstate != V34HS_MOH_SILENCE)
					goto tx_sample_done;			/* 0x63941 */

				if (dsplibs_debug_level > 1)			/* 0x66d98 */
					dsplibs_debug_printf(
						"V34F MOH: After 192 silence, org = %d ," " act = %d\r\n",
						obj->moh_org, obj->moh_message);

				/* 0x66dac */
				hs_set_txstate(obj,
					    (obj->moh_org == 1 || obj->moh_message == 1)
					    ? V34HS_MOH_FRR : V34HS_MOH_ON_HOLD);

				obj->vect_idx = 0;				/* 0x66dec */
				goto tx_sample_done;				/* 0x63948 */
			}
		case V34HS_K56JaTXMIT:		/* 85 0x63fb0 */
			{

				(void)tx1_ja_common(obj, "V34Hshak: on K56JaTXMIT - time to freeze"
							   " echo...\r\n");		/* 0x68282 */
				(void)k56FlexPhase34(obj);
				goto tx_sample_done;
			}
		case V34HS_TXMD:		/* 86 0x63dae */
			{
				short q = tx1_scramble2(obj);

				obj->cur_quadrant = q;
				tx1_put_point(obj, vect4[q]);
				txmit(obj);

				obj->vect_idx = (short)(obj->vect_idx + 1);
				if ((unsigned short)obj->vect_idx
				    == (unsigned short)obj->short_aa78) {
					/* 0x66fe9 */
					if (dsplibs_debug_level > 1)
						dsplibs_debug_printf(
							"On MD - enabling echo adaptation...\r\n");

					obj->echo_calls = 0;				/* 0x6700e */
					obj->tx_flags =				/* 0x67017 */
						(short)((unsigned short)obj->tx_flags & ~V34_EC_FROZEN);
					obj->echo_alpha = 0;				/* 0x6701e */
					obj->far_echo_alpha = 0;				/* 0x67025 */
				}

				if ((unsigned short)obj->vect_idx
				    == (unsigned short)obj->seg_len) {
					struct v34_ratecfg *cfg = &obj->ratecfg;
					int pcm = (obj->v90_receiver != 0 || obj->k56flex_receiver != 0);

					if (dsplibs_debug_level > 1)		/* 0x6800d */
						dsplibs_debug_printf(
							"TX: Done with MD, moving to S/Sbar" " again...\r\n");

					V34SetupModulator(v34_object_modulator(obj),
							  cfg->baud, cfg->carrier, cfg->preemp, pcm, 0);

					hs_set_txstate(obj, V34HS_SSEG);
					obj->seg_symcount = 0;
					obj->tx_scr_sr = 0;
					obj->tx_flags = (short)((unsigned short)obj->tx_flags
							     | (V34_TXFLAG_PPSEG | V34_EC_FROZEN));
				}

				/* 0x6430c */
				goto tx_segment_count;
			}
		default:
			/*
			 * 0x629e0, the loop bottom.  Fifty-seven of the
			 * eighty-two entries, plus every txstate the range
			 * test rejects.
			 */
			break;
		}

		goto tx_sample_done;
	tx_segment_count: /* 0x6430c: PPSEG and TXMD share this increment. */
		obj->seg_symcount = (short)((unsigned short)obj->seg_symcount + 1);
	tx_sample_done:
		;
	}

	/*
	 * 0x629f1.  The receiver's first halfword, signed: `cmpw $0x5,(%ebx)`
	 * then `jle 62ae3`, and 0x62ae3 reads +0x3596 into %cx for the
	 * once-per-block dispatch at 0x62af1.  So five or fewer is the BLOCK
	 * route and not an idle return.  Finding F549.
	 */
	if (*(short *)rx <= 5) {
		goto transmit_status_reload;
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
	 * the transmit-status join and has been written since table 2 landed -- so every
	 * rxstate below 43 except 4, and every rxstate above 43 except 53 and
	 * 72, is complete here rather than guarded.  That is most of the
	 * eighty-seven.  Finding F717.
	 */
	rxst = obj->rxstate;

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
				goto transmit_status_reload;
			}
			/*
			 * 0x62b7d's `je` target.  0x650c6 is the counter
			 * increment at the head of the arm: `movzwl
			 * 0xaa78`, `inc`, `cmp $0x440`, the store, then
			 * `jle 6551a`.
			 */
			if (rxst == V34HS_RX_L1)
				goto receive_l1;
			/*
			 * 0x62b83.  `mov 0xc0(%esp),%esi; movzwl
			 * 0x3596(%esi),%ecx; jmp 62af1` -- the second
			 * chain's fall-through, reached by every rxstate
			 * above 43 that is not 53 or 72.  Finding F717.
			 */
			goto transmit_status_reload;
		}
		/*
		 * 0x62a18's `je` target, and 0x653e4 is not the arm's first
		 * statement but the argument setup for it: `mov
		 * 0xc0(%esp),%ecx; mov %ecx,(%esp)` ahead of the `receiver`
		 * call at 0x653ee.
		 */
		if (rxst == V34HS_RECEIVE) {
			t4_rx_receive(obj);		/* 0x653e4 */
			goto transmit_status_reload;
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
			rxreadqueue((struct v34_queue *)rx);
			goto transmit_status_reload;
		}
		/*
		 * 0x62a2a IS the dispatch rather than a jump to it: the same
		 * `movswl %cx,%eax; sub $0x5; cmp $0x45` that 0x62af1 has,
		 * `jbe 62b00` into table 2's indirect jump, and an
		 * out-of-range txstate falling into the tail at 0x62a40.
		 */
		goto transmit_status_reload;
	}

	/*
	 * 0x64a64.  `V34agc` and `fskdemodulate` first, and only now is
	 * +0x3592 read: nothing on the way here writes any of the three state
	 * words, which is finding F285 and is what makes a poke-and-step
	 * fixture possible at all.
	 */
	V34agc(rx);

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
	fskin = rx->burst;

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
	if (T3M_I32(obj, T3M_FSKGATE) != 0
	    && detectRetrainReq(obj, V34_RETRAIN_BINS, fskin, 4)) {
		/*
		 * 0x6936e.  The action block: the far end asked for a
		 * retrain while this end was still looking for INFO1.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("DET_SYNC : retrain request "
					     "detected while searching for " "info1\n");

		T3M_I32(obj, T3M_FSKGATE) = 0;	/* 0x6938c, 32-bit */

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

		/* 0x693fa/0x69411/0x69425: 16-bit read-modify-write. */
		obj->short_3588 |= 2;

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
		hs_set_microstate(obj, V34HS_TX_PHASE1_ANS);	/* 46 */
		hs_set_rxstate(obj, V34HS_RX_DPSK);		/* 43 */
		hs_set_txstate(obj, V34HS_TONE_AB);		/* 60 */

		/* 0x695cb is `cmpw $0x1` then `jbe`, so UNSIGNED. */
		if (T3M_U16(obj, T3M_TOGGLE) > 1)
			T3M_U16(obj, T3M_TOGGLE) = 0;

		/* 0x695f0..0x69605, in the object's order. */
		obj->vect_idx = 0;			/* +0x2aa2 */
		T3M_U16(obj, T3M_COUNTER) = 0;	/* +0xaa78 */
		obj->fsk.sr = -1;			/* +0xaae2 */
		obj->fsk.nbits = 0;			/* +0xaae0 */
	}

	/*
	 * 0x64a9e, and it has to be read HERE: `fskdemodulate` writes it --
	 * and so does the action block above, which zeroes it.  Both orders
	 * are observable and this is the object's.
	 */
	nbits = obj->fsk.nbits;

	fskdemodulate(obj, fskin, &obj->fsk);

	microstate = obj->microstate;

	if ((unsigned)((int)microstate - T3M_TBL3_FIRST) >= T3M_TBL3_COUNT) {
		/*
		 * 0x65329.  `mov 0xc0(%esp),%ebx; movzwl 0x3596(%ebx),%ecx;
		 * jmp 62af1`: a microstate outside table 3's range leaves
		 * through the same transmit dispatch as everything else here.
		 */
		goto transmit_status_reload;
	}

	/* Microstate dispatch: .rodata+0x3000, indexed at 0x64ad2. */
	switch ((int)microstate) {
	case V34HS_DET_SYNC:		/* 41, 0x669a4 */
		t41_micro_det_sync(obj);
		goto transmit_status_reload;
	case V34HS_DET_INFO:
		t44_micro_det_info(obj);
		goto transmit_status_reload;
	case V34HS_TX_PHASE2_ANS:	/* 47, and 56 is the same address */
	case V34HS_TX_PHASE2_CALL:	/* 56 */
		{
			unsigned short n;

			/*
			 * 0x6683b and 0x6cc1b.  `obj->fsk.sr` is the demodulator's shift
			 * register and `nbits` its bit count, and the two are reset together
			 * below -- which is what "repeated info0" means here.  The second
			 * test is what stops it running on every block: +0x3588 becomes 4.
			 */
			if ((obj->fsk.sr & 0xf) == 0xf && obj->short_3588 == 0
#ifndef DSPLIB_REPRODUCE_BUGS
			    && t3m_info0_carrier(rx->burst)
#endif
			   ) {
				t3m_errrec_reset(obj);

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("Repeated info0 is detected, "
							     "errorrecovery is initialized in " "TX_PHASE2_xxx\n");

				/*
				 * 0x6ce34 clears the counter and 0x6ce3b jumps back to
				 * 0x6684e, the increment below -- so the clear is the reset's
				 * last act, and 0x6684e re-loads the object pointer from
				 * 0xc0(%esp) because it is a join and not a straight line.
				 */
				T3M_U16(obj, T3M_COUNTER) = 0;
			}

			/* 0x6684e. */
			n = (unsigned short)(T3M_U16(obj, T3M_COUNTER) + 1);

			/*
			 * The `jle 6aaf9` at 0x66861 selects three instructions that are the
			 * whole low path: the txstate is read into %cx at 0x6ab00 and only
			 * then is the counter stored, at 0x6ab07.
			 */
			if ((short)n <= 0x5f) {
				/* 0x6aaf9. */
				T3M_U16(obj, T3M_COUNTER) = n;
				tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
				goto transmit_status;
			}

			/*
			 * 0x66867.  The counter is cleared FIRST, so the transition below
			 * prints `[2]` as 0 and not as the value that crossed the threshold;
			 * the object hands the printf a literal zero, which is how it shows.
			 */
			T3M_U16(obj, T3M_COUNTER) = 0;
			T3M_U16(obj, T3M_TOGGLE) ^= 1;
			hs_set_microstate(obj, V34HS_TX_L1);
			tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
		}
		goto transmit_status;
	case V34HS_TX_PHASE3_ANS:	/* 48 */
		{
			unsigned short n = (unsigned short)(T3M_U16(obj, T3M_COUNTER) + 1);

			T3M_U16(obj, T3M_COUNTER) = n;

			if (n == 0x78) {
				/* 0x6c6e4.  An XOR of bit 0, not a store of a constant. */
				T3M_U16(obj, T3M_TOGGLE) ^= 1;
				tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
				goto transmit_status;
			}

			if (n == 0xa2) {
				/*
				 * 0x6c670.  Both transitions are the setters inlined, and
				 * the first leaves 5 in %ecx on BOTH arms of its already-there
				 * test -- the value loaded from +0x3596 when it matches,
				 * 0x6c6a4's `mov $0x5,%ecx` when it does not -- which is why
				 * the dispatch is reached with a literal SILENCE and not with
				 * a re-read.
				 */
				/* 0x6c670. */
				hs_set_txstate(obj, V34HS_SILENCE);
				hs_set_microstate(obj, V34HS_RX_PHASE2_ANS);
				T3M_U16(obj, T3M_COUNTER) = 0;
				tx = (V34HS_SILENCE);
				goto transmit_status;
			}

			tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
		}
		goto transmit_status;
	case V34HS_RX_PHASE1_ANS:	/* 49 */
		{
			unsigned short n = (unsigned short)(T3M_U16(obj, T3M_COUNTER) + 1);
			int filt;
			short rtd;

			T3M_U16(obj, T3M_COUNTER) = n;

			/*
			 * 0x66a2d loads +0xaa7c zero-extended and 0x66a37 sign-extends it for
			 * the compare.  The four addresses below are the four conjuncts in
			 * source order -- `cmp $0x28,%ax`/`jle`, `cmp %ebx,%ebp`/`jge`,
			 * `cmp $0x372`/`je`, then `cmpw $0x0,0x3588`/`jne` out at 0x70d71 --
			 * and 0x70d7f, the reset itself, is that last one's fall-through.
			 */
			filt = obj->filtdelay;

			/* 0x66a1c, 0x66a3d, 0x66a4d and 0x70d71. */
			if ((short)n > 0x28 && (short)n < filt + 0x4c
			    && (((unsigned)(unsigned short)obj->fsk.sr & 0x3ff) == 0x372)
			    && obj->short_3588 == 0) {
				t3m_errrec_reset(obj);

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("Repeated info0 is detected, "
							     "errorrecovery is initialized in " "RX_PHASE1_ANS\n");
			}

			/*
			 * 0x66a58.  The counter is re-read off the object rather than reused,
			 * which matters because the reset above may have moved through it --
			 * it does not, but the object reads it again and so does this.
			 */
			if ((int)obj->short_aa78 <= filt + 0x50) {
				tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
				goto transmit_status;
			}

			/* 0x66a74, and it happens before the test that can leave. */
			rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);

			if ((obj->fsk.sr & 1) == 0) {
				tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
				goto transmit_status;
			}

			/*
			 * 0x6ce40.  The copy is one halfword move, +0xac02 into +0xaa7e at
			 * 0x6ce4e, and it is made BEFORE the debug-level branch at 0x6ce5c, so
			 * only the line is conditional.
			 */
			if (obj->is_short != 0) {
				/* 0x6ce40. */
				obj->rtd = obj->prev_bulk_delay;

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("Setting Bulk delay according to "
							     "prev session - %d\r\n",
							     (int)obj->rtd);
			} else {
				/*
				 * 0x66aa7.  Both halfwords are zero-extended and the whole
				 * sum is done 32-bit before it is stored back as a short, so a
				 * counter below `filtdelay` gives a huge product and not a
				 * negative one.
				 */
				unsigned d = (unsigned)T3M_U16(obj, T3M_COUNTER)
					   - (unsigned)T3M_U16(obj, T3M_FILTDELAY);

				obj->rtd = (short)(d * 4u - 0x18cu);
			}

			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("On RX_PHASE1_ANS: is short=%d, "
						     "bulkDelay=%d, filtDelay=%d\r\n",
						     (int)obj->is_short, (int)obj->rtd,
						     (int)obj->filtdelay);

			/* 0x66ae2, on the SIGNED halfword. */
			if (obj->rtd <= 0)
				obj->rtd = 1;

			rtd = obj->rtd;
			ApplyBulkDelay(obj, rtd);

			/*
			 * 0x66b13.  The counter takes `filtdelay` BEFORE the transition below,
			 * so the `[2]` the line prints is filtdelay and not the value that
			 * crossed the threshold.
			 */
			T3M_U16(obj, T3M_COUNTER) = T3M_U16(obj, T3M_FILTDELAY);
			hs_set_microstate(obj, V34HS_TX_PHASE2_ANS);

			/* 0x66b47, and 0x66b55 reads +0x3596 for the exit. */
			*(short *)((unsigned char *)rx + T3M_RX_F264) = rx->agc_gain;

			tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
		}
		goto transmit_status;
	case V34HS_RX_PHASE2_ANS:	/* 50 */
		{
			unsigned short n = (unsigned short)(T3M_U16(obj, T3M_COUNTER) + 1);
			int filt;

			T3M_U16(obj, T3M_COUNTER) = n;

			/*
			 * Four inline compares, one per conjunct: `cmp $0x32,%ax`/`jle 6af11`,
			 * `cmp %eax,%ebp`/`jge 66724`, `cmp $0x200,%edx`/`jle 66724` and
			 * `cmpw $0x0,0x3588`/`jne 66724`.  Arm 49's fourth conjunct is
			 * out-lined at 0x70d71; this arm's is inline with the other three.
			 */
			filt = obj->filtdelay;

			/* 0x664c7, 0x664e8, 0x664fd and 0x66509. */
			if ((short)n > 0x32 && (short)n < filt + 0x4c
			    && (int)((unsigned)(unsigned short)obj->fsk.sr & 0x3ff) > 0x200
			    && obj->short_3588 == 0) {
				t3m_errrec_reset(obj);

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
			if ((int)obj->short_aa78 <= filt + 0x50) {
				tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
				goto transmit_status;
			}

			if ((obj->fsk.sr & 1) == 0) {
				tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
				goto transmit_status;
			}

			/* 0x66759, and it is stored before the demodulator's diagnostic. */
			T3M_U16(obj, T3M_COUNTER) = 0x14;

			V34SetupDemodulator(obj, 2400, 1800);
			rxtiminginit(obj);

			hs_set_rxstate(obj, V34HS_RX_L1);

			/* 0x66812 and 0x66818.  0xfffff5ff is ~0x0a00 and not a mask of one
			   bit; the shift is arithmetic on the sign-extended halfword. */
			rx->agc_gain = (short)((int)rx->agc_gain >> 1);
			rx->flags = (unsigned short)(rx->flags
							& ~(V34_RX_FLAG_DET_PENDING | V34_RX_FLAG_FIR));

			tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
		}
		goto transmit_status;
	case V34HS_TX_L1:		/* 51 */
		{
			unsigned short n = (unsigned short)(T3M_U16(obj, T3M_COUNTER) + 1);
			short gain;

			T3M_U16(obj, T3M_COUNTER) = n;

			if (n != 0x2a) {
				tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
				goto transmit_status;
			}

			/*
			 * 0x6b5f5 opens the `is_short == 0` body with the txstate transition
			 * inlined -- `movzwl 0x3596`, `cmp $0x33`, `je 6b67d` -- and 0x33 is
			 * 51, the microstate this arm already is, so both transitions below
			 * can be no-ops and the object still emits both compares.
			 */
			if (obj->is_short == 0) {
				/* 0x6b5f5. */
				hs_set_txstate(obj, V34HS_TX_L1);
				hs_set_microstate(obj, V34HS_TX_L1);
				obj->vect_idx = 0;

				/*
				 * 0x6b764 and 0x6f8c2.  Seven arguments, six of them the same
				 * constants either way; only the coefficient table depends on
				 * which end this is.
				 */
				detectorinit(&obj->detector,
					     obj->role == 0x65 ? c2400_ : c1200_,
					     0, 0x64, 0x32, 0x800, 0);

				rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);
				hs_set_rxstate(obj, V34HS_DET_AB);
			} else {
				struct v34_bitsource *r;
				unsigned short *w;
				int baud;

				/* 0x6b8d0, and the two transitions are the other way round. */
				hs_set_microstate(obj, V34HS_INFODONE);
				hs_set_txstate(obj, V34HS_TX_DPSK);

				/* 0x6b9e3, and all three are cleared AFTER both lines are
				   printed -- vect_idx is `[1]` and the counter is `[2]`. */
				obj->vect_idx = 0;
				T3M_U16(obj, T3M_COUNTER) = 0;
				T3M_I16(obj, T3M_F35A2) = 0;

				w = (unsigned short *)&obj->msgrec[2];
				obj->paa6c = w;
				V34SetINFO1aBits(obj, (short *)w);

				/*
				 * 0x6ba1f.  A signed compare tree on the transmit baud rate,
				 * and every arm but 0xab7's writes one pointer and nothing
				 * else.  A rate not in the six leaves the scale alone, which
				 * is why a trial at the fixture's own fill tests none of them.
				 */
				baud = obj->ratecfg.baud;

				switch (baud) {
				case 0x960:		/* 2400 */
					obj->ratecfg.divtab = scale2400;
					break;
				case 0xab7:		/* 2743, and the only arm that
							   REPLACES the rate it matched */
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
							"V34HSHAK: Illegal prev session baud "
							"(2743), select 2800 instead...\r\n");
					{
						short c = T3M_U16(obj, T3M_TXCARRIER) == 0x725
							  ? 0x74b : 0x690;

						T3M_U16(obj, T3M_TXBAUD) = 0xaf0;
						obj->ratecfg.divtab =
							scale2800;
						T3M_U16(obj, T3M_TXCARRIER) = (unsigned short)c;
					}
					break;
				case 0xaf0:		/* 2800 */
					obj->ratecfg.divtab = scale2800;
					break;
				case 0xbb8:		/* 3000 */
					obj->ratecfg.divtab = scale3000;
					break;
				case 0xc80:		/* 3200 */
					obj->ratecfg.divtab = scale3200;
					break;
				case 0xd65:		/* 3429 */
					obj->ratecfg.divtab = scale3429;
					break;
				default:
					break;
				}

				/*
				 * 0x6ba5f.  The record is reached THROUGH +0xaa6c, re-read
				 * after `V34SetINFO1aBits`, and not through the address stored
				 * into it above.  Two of the twelve fields are 32-bit.
				 */
				r = obj->paa6c;

				r->crc = -1;
				r->crc_on = 1;
				r->nbits = 0x26;
				r->pos = 0;
				r->wordbits = 8;
				r->idx = 0;
				r->repeat = 0;
				r->repeats = 0;
				r->acc = 0xff72;
				r->avail = 0x10;
				r->avail0 = 0x10;
				r->acc0 = 0xff72;

				/* 0x6bac3.  Four copies, and the rate they copy is the one the
				   switch above may have replaced. */
				obj->short_358c = 0;
				obj->ratecfg.rx_baud = (short)T3M_U16(obj, T3M_TXBAUD);
				T3M_U16(obj, T3M_RXCARRIER) = T3M_U16(obj, T3M_TXCARRIER);
				obj->ratecfg.rx_divtab =
					obj->ratecfg.divtab;

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
			gain = rx->agc_gain;
			if (gain > 0x2000)
				rx->agc_gain = (short)((int)gain >> 2);
			else if (gain > 0x1000)
				rx->agc_gain = (short)((int)gain >> 1);

			T3M_I16(obj, T3M_F35A0) = 0;

			/*
			 * 0x6b84f.  The two phrases are the object's, at .rodata.str1.1+0x2b94
			 * and +0x2ba8, and `is_short` picks between them -- READ AGAIN here,
			 * not remembered, because the `is_short == 0` body above does not
			 * change it but the reader has no way to know that from this line.
			 */
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V34RETRAIN, %s, rx->rxflgs = 0x%x," "rx->gain=0x%x\n",
						     obj->is_short != 0
						     ? "transmitting info1a" : "starting DET_AB",
						     (unsigned)rx->flags,
						     (int)rx->agc_gain);

			tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
		}
		goto transmit_status;
	case V34HS_TX_PHASE1_CALL:	/* 55 */
		{
			unsigned short n;

			/* 0x65b79 and 0x65b82, and only then 0x6c14a. */
			if (obj->fsk.nbits != nbits && (obj->fsk.sr & 7) == 7) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("Repeated info0 is detected, "
							     "errorrecovery is initialized in " "TX_PHASE1_CALL\n");

				t3m_errrec_arm(obj);

				/*
				 * 0x6e3f8.  Printed after +0x3588 is armed and before the body
				 * below overwrites the register with -1, exactly as 59's is.
				 */
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V21RXBUF=0x%x\n",
							     (unsigned)(unsigned short)
							     obj->fsk.sr);

				t3m_errrec_core(obj);

				/* 0x6c2af, and then `jmp 65b95` -- back INTO the increment. */
				T3M_U16(obj, T3M_COUNTER) = 0;
			}

			/* 0x65b95.  Unsigned increment, SIGNED sixteen-bit compare. */
			n = (unsigned short)(T3M_U16(obj, T3M_COUNTER) + 1);

			/*
			 * 0x6ab9b is out of line and it does NOT leave: the store of the
			 * incremented counter, then `jmp 65c1d` back into the arm.  That is
			 * what separates this low arm from 47's at 0x6aaf9, which reads the
			 * txstate and goes to the dispatch.
			 */
			if ((short)n <= 0x5f) {
				/* 0x6ab9b. */
				T3M_U16(obj, T3M_COUNTER) = n;
			} else {
				/*
				 * 0x65bae.  The counter is cleared BEFORE the transition, so
				 * the `[2]` the line prints is 0 and not the value that
				 * crossed the threshold -- the same order arm 47's copy takes.
				 */
				T3M_U16(obj, T3M_COUNTER) = 0;
				T3M_U16(obj, T3M_TOGGLE) ^= 1;
				hs_set_microstate(obj, V34HS_RX_PHASE2_CALL);

				/*
				 * 0x65bff.  Printed whether or not the transition was a move,
				 * because 0x6c2bb rejoins below the setters' compare.
				 */
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V21RXBUF=0x%x\n",
							     (unsigned)(unsigned short)
							     obj->fsk.sr);
			}

			/* 0x65c24 and 0x6d243 -- one 32-bit compare of two halfwords. */
			/*
			 * 0x6fff7, the body that guard selects, opens by building the call:
			 * `xor %ebx,%ebx` into 0x4(%esp), so the second argument is a literal
			 * zero and not the 1 the retraining sites pass.
			 */
			if (T3M_I32(obj, T3M_F3588) == 0x20002
			    && (((unsigned)(unsigned short)obj->fsk.sr & 0x3ff) == 0x372)) {
				/* 0x6fff7. */
				v34handshakinit(obj, 0);

				/*
				 * 0x7001c.  +0x3588 is read back AFTER the call, because
				 * `v34handshakinit` writes it.
				 */
				obj->paa6c = obj->msgrec;
				t3m_errrec_arm(obj);
				t3m_errrec_core(obj);

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V34 In Retrain. errorrecovery for "
							     "info0 is initialized in " "TX_PHASE1_CALL\n");
			}

			tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
		}
		goto transmit_status;
	case V34HS_RX_PHASE1_CALL:	/* 58 */
		{
			unsigned short n;
			short c;

			/*
			 * 0x6600a.  The record is reached through the self-pointer and the
			 * resets below fill the one at +0xa94c by address, so the two are the
			 * same record only when something has aimed it there -- which block 2
			 * does and the entry does not.
			 */
			if ((short)T3M_U16(obj, V34HS_TXSTATE_OFF) == V34HS_TX_DPSK) {
				struct v34_bitsource *r = obj->paa6c;

				if (r->repeat != 0 && r->repeats != 0)
					r->repeat = 0;
			}

			/* 0x66018, and the store is before either threshold. */
			n = (unsigned short)(T3M_U16(obj, T3M_COUNTER) + 1);
			T3M_U16(obj, T3M_COUNTER) = n;

			if ((short)n > 0x5f) {
				/* 0x66038, and it happens whether or not the test below does. */
				rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);

				/* 0x6604c, a full-width equality and not one of 49's, 50's or
				   59's masked tests of the same register. */
				if ((unsigned short)obj->fsk.sr == 1) {
					/* 0x6e28d, printed before anything below is written. */
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
							"V34RETRAIN, RX_PHASE1_CALL received, "
							"count2=%d,rx->gain=0x%x,filtdelay=%d\n",
							(int)obj->short_aa78,
							(int)rx->agc_gain,
							(int)obj->filtdelay);

					/* 0x6c2dd.  The counter takes filtdelay BEFORE the
					   transition, so the `[2]` the line prints is
					   filtdelay and not the value that crossed 0x5f. */
					obj->short_358a = 2;
					T3M_U16(obj, T3M_COUNTER) = T3M_U16(obj, T3M_FILTDELAY);
					hs_set_microstate(obj,
						    V34HS_TX_PHASE1_CALL);

					/* 0x6c37f, the copy before the flag. */
					*(short *)((unsigned char *)rx + T3M_RX_F264) =
						rx->agc_gain;
					rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);

					tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
					goto transmit_status;
				}
			}

			/* 0x66061.  One load, and both thresholds read it from %cx. */
			c = (short)T3M_U16(obj, T3M_COUNTER);

			/*
			 * The four guards in source order, and only the last is out of line:
			 * `cmp $0x3bf,%cx`; `test $0x2,%dl`, a BYTE test of the halfword
			 * loaded at 0x66071; `cmpw $0x2,0x358a`; and at 0x6d709 the masked
			 * `cmp $0x372`, whose failure joins the first three at 0x6608b.
			 */
			/* 0x66068, 0x66071, 0x6607d and 0x6d709. */
			if (c > 0x3bf && (T3M_U16(obj, T3M_F3588) & 2) != 0
			    && T3M_U16(obj, T3M_F358A) == 2
			    && (((unsigned)(unsigned short)obj->fsk.sr & 0x3ff) == 0x372)) {
				/*
				 * 0x6d728.  Bit 1 alone is not enough for the call: the whole
				 * halfword has to be 2, so a +0x3588 of 3 or 6 arms the
				 * recovery without re-initialising anything.
				 */
				if (T3M_U16(obj, T3M_F3588) == 2) {
					v34handshakinit(obj, 0);
					obj->paa6c = obj->msgrec;
				}

				t3m_errrec_arm(obj);
				t3m_errrec_core(obj);

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("errorrecovery for info0 is "
							     "initialized in RX_PHASE1_CALL\n");

				/* 0x6d94c and 0x6d9e6 -- the counter a THIRD time. */
				c = (short)T3M_U16(obj, T3M_COUNTER);
			}

			/*
			 * Three disjuncts and three DIFFERENT out-of-line exits:
			 * `cmp $0x4b0,%cx`/`jle 6ade3`, `cmpw $0x0,0x3588`/`jne 6c3e0` and
			 * `cmpw $0x0,0xaae2`/`je 6e322`.  Each of the three branches away, so
			 * the reset below is the fall-through and runs only when all three
			 * fail.
			 */
			/* 0x6608b, 0x6609d and 0x660ab. */
			if (c <= 0x4b0 || T3M_U16(obj, T3M_F3588) != 0
			    || (unsigned short)obj->fsk.sr == 0) {
				tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
				goto transmit_status;
			}

			/*
			 * 0x660b9's first instruction is `mov $0x4,%ebx` -- the `= 4` form of
			 * the reset, not the `|= 1` one block 2 above takes at 0x6d709.
			 */
			/* 0x660b9. */
			t3m_errrec_reset(obj);
			tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
		}
		goto transmit_status;
	case V34HS_RX_PHASE2_CALL:	/* 59 */
		{
			unsigned short n = (unsigned short)(T3M_U16(obj, T3M_COUNTER) + 1);
			short c, filt, rtd;

			T3M_U16(obj, T3M_COUNTER) = n;

			/* 0x662c5 and 0x6bc0b.  Three bits, where 47's copy tests four. */
			if ((obj->fsk.sr & 7) == 7) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("Repeated info0 is detected, "
							     "errorrecovery is initialized in " "RX_PHASE2_CALL\n");

				t3m_errrec_arm(obj);

				/*
				 * 0x717d8.  Printed AFTER +0x3588 is armed and BEFORE the body
				 * below overwrites the shift register with -1, so what it shows
				 * is the register that satisfied the guard.
				 */
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V21RXBUF=0x%x\n",
							     (unsigned)(unsigned short)
							     obj->fsk.sr);

				t3m_errrec_core(obj);
				T3M_U16(obj, T3M_COUNTER) = 0;
			}

			/* 0x662e0.  One load of +0xaa78 feeds both compares. */
			c = (short)T3M_U16(obj, T3M_COUNTER);

			/*
			 * 0x662e7 and 0x6bbd5.  The object jumps straight past the next test on
			 * the branch where the txstate was already 5, which it can do because
			 * 0x2a is not above 0x125f either way; the setter is that branch.
			 */
			if (c == 0x2a)
				hs_set_txstate(obj, V34HS_SILENCE);

			if (c > 0x125f) {
				/*
				 * 0x662fc tests +0x19e sixteen bits wide (`cmpw $0x0`) and
				 * only a non-zero one reaches 0x6bb57, where the detector call
				 * is built out of +0x130 and +0x3564 -- so the `&&`
				 * short-circuits in the object as well as here.
				 */
				/* 0x662fc and 0x6bb57. */
				if (rx->retrain_gate != 0
				    && tone_detect(rx,
						   &obj->detector,
						   rx->burst,
						   rx->rx_samples)) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf("V34RETRAIN, retrain is " "initiated in "
								     "RX_PHASE2_CALL\n");

					rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_RETRAIN);
					v34handshakinit(obj, 1);
					tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
					goto transmit_status;
				}

				/* 0x6630a, reached whether +0x19e was set or the detector said
				   no, and the counter is read again at 0x66321. */
				rx->retrain_gate = 1;
				c = (short)T3M_U16(obj, T3M_COUNTER);
			}

			/*
			 * 0x66332.  The comparison sign-extends `filtdelay` and the subtraction
			 * below zero-extends it, and both are truncated to sixteen bits before
			 * anything is stored, so the two readings differ only in the compare.
			 */
			filt = obj->filtdelay;

			if ((int)c > (int)filt + 0x5c
			    && ((unsigned short)obj->fsk.sr == 0x08
				|| (unsigned short)obj->fsk.sr == 0x18)) {
				/*
				 * 0x66371.  `sub $0x63` BEFORE the shift, which is 49's
				 * `(n - filtdelay) * 4 - 0x18c` written the other way round --
				 * the same halfword, since 0x18c is four times 0x63.  The
				 * clamp is on the SHIFTED value and it is signed.
				 */
				rtd = (short)((unsigned)(unsigned short)c
					      - (unsigned)(unsigned short)filt - 0x63u);
				rtd = (short)((int)rtd * 4);
				obj->rtd = rtd <= 0 ? 1 : rtd;

				ApplyBulkDelay(obj, obj->rtd);

				/* 0x663b0, and it is stored before the demodulator's line. */
				T3M_U16(obj, T3M_COUNTER) = 0x14;

				/*
				 * 0x663bd..0x66414 is `V34SetupDemodulator` inlined with two
				 * literals, exactly as arm 50's 0x66766 is: `out_count`, the four
				 * 2400-baud words and the 1800-Hz carrier and its `half_len`, in
				 * that function's order.  The 0x960 and 0x708 at 0x717a4 are
				 * the diagnostic's own arguments and not a read of any field.
				 */
				V34SetupDemodulator(obj, 2400, 1800);
				rxtiminginit(obj);

				hs_set_rxstate(obj, V34HS_RX_L1);

				/* 0x66477 then 0x66481, the gain before the flags. */
				rx->agc_gain = (short)((int)rx->agc_gain >> 1);
				rx->flags = (unsigned short)(rx->flags
								& ~(V34_RX_FLAG_DET_PENDING | V34_RX_FLAG_FIR));

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V34AGC, rx->gain =0x%x," "rx->slowcf=%d, at end of "
							     "bulkdelay estimation\n",
							     (int)rx->agc_gain,
							     (int)rx->agc_step);
			}

			/*
			 * 0x66495 and 0x70447.  ONE 32-BIT COMPARE OF TWO HALFWORDS: +0x3588
			 * must be 2 and +0x358a must be 2, which is why this block cannot run
			 * in the same step as the reset above unless the reset ran first and
			 * left them 3 and 1 -- and 3 is not 2, so it cannot.
			 */
			if (T3M_I32(obj, T3M_F3588) == 0x20002
			    && (((unsigned)(unsigned short)obj->fsk.sr & 0x3ff) == 0x372)) {
				v34handshakinit(obj, 0);

				/*
				 * 0x70475.  +0x3588 IS READ BACK AFTER THE CALL and not before
				 * it: `v34handshakinit` writes that field, so a reading taken
				 * first would set bit 0 of the wrong value.
				 */
				obj->paa6c = obj->msgrec;
				t3m_errrec_arm(obj);
				t3m_errrec_core(obj);

				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("V34 In Retrain. errorrecovery for "
							     "info0 is initialized in " "RX_PHASE2_CALL\n");
			}

			tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
		}
		goto transmit_status;
	case V34HS_RX_PHASE3_CALL:
		t3c_micro_rx_phase3_call(obj);
		goto transmit_status_reload;
	case V34HS_INFODONE:		/* 63 */
		{
			short arm_tx = (short)T3M_U16(obj, V34HS_TXSTATE_OFF);
			unsigned short n;

			/*
			 * 0x6d160 is the BODY and not the test: 0x6592c compares the txstate
			 * already in %cx against 0x3c and jumps there, and 0x6d160 itself is
			 * the counter increment -- `movzwl 0xaa78`, `inc`, `cmp $0xf,%ax`.
			 */
			if (arm_tx == V34HS_TONE_AB) {			/* 0x6d160 */
				n = (unsigned short)(T3M_U16(obj, T3M_COUNTER) + 1);

				if ((short)n <= 0x0f) {
					/* 0x6d230, and %ecx still holds TONE_AB. */
					T3M_U16(obj, T3M_COUNTER) = n;
					tx = (arm_tx);
					goto transmit_status;
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
				T3M_U16(obj, T3M_COUNTER) = n;

				if (obj->role == 0x65) {		/* 0x6de96 */
					hs_set_txstate(obj, V34HS_SILENCE);
					hs_set_microstate(obj,
						    V34HS_DET_SYNC);
					obj->fsk.nbits = 0;
					tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
					goto transmit_status;
				}

				/*
				 * 0x6d187.  The answer side moves the txstate and REPLACES the
				 * counter, with one of two constants on `ptc`; it does not
				 * touch the microstate, so 63 is entered again next block.
				 */
				hs_set_txstate(obj, V34HS_SILENCE);
				T3M_U16(obj, T3M_COUNTER) =
					obj->ptc == 0x30 ? 0x1e : 0x96;
				tx = (V34HS_SILENCE);
				goto transmit_status;
			}

			if (arm_tx != V34HS_SILENCE) {			/* 0x6593a */
				tx = (arm_tx);
				goto transmit_status;
			}

			/* 0x65947, a SIGNED byte, and 0x65958, a SIGNED int against 0x240. */
			if ((signed char)((unsigned char *)obj)[T3M_FABFF] <= 0
			    || T3M_I32(obj, T3M_F0240) <= 0x240) {
				tx = (arm_tx);
				goto transmit_status;
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
			v34setuptxmit(obj);
			tx = ((short)T3M_U16(obj, V34HS_TXSTATE_OFF));
		}
		goto transmit_status;
	case V34HS_MOH_TONE:
		t3c_micro_moh_tone(obj);
		goto transmit_status_reload;
	case V34HS_MOH_TONE_DROP:
		t3c_micro_moh_tone_drop(obj);
		goto transmit_status_reload;
	case V34HS_TX_PHASE1_ANS:	/* 46, 0x65d6d */
		t46_micro_tx_phase1_ans(obj);
		goto transmit_status_reload;

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
		tx = (short)obj->txstate;
		goto transmit_status;
	}
	goto transmit_status_reload;
receive_l1: /* 0x650c6: probe reception and tone-A wait. */

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
		goto transmit_status_reload;
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
		hs_set_rxstate(obj, V34HS_RX_DPSK);	 /* 0x67e1e */
		/*
		 * 0x67e4b.  `cmp $0x3c,%dx`, and it is NOT one of the three
		 * the head of this section calls unreachable: those three
		 * compare +0x3594 and this one compares +0x3596.  An end
		 * already in TONE_AB stores nothing and prints nothing.
		 */
		hs_set_txstate(obj, V34HS_TONE_AB);	 /* 0x67e4b */
		/*
		 * 0x67e7f.  `cmp $0x29,%dx` on +0x3592: microstate 41, whose
		 * arm at 0x669a4 is the table-3 case at the bottom of this
		 * file.  This store is what sends the next block there.
		 */
		hs_set_microstate(obj, V34HS_DET_SYNC); /* 0x67e7f */

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
		obj->msgrec[3].nbits = 0x4d;		 /* 0x67f54 */
		obj->vect_idx = 0;				 /* 0x67f5a */
		/*
		 * 0x67f68.  `mov %ebx,0xa8a0(%esi)`, thirty-two bits.  This
		 * is the gate 0x64a87 tests, so from the next block on the
		 * RX_DPSK path polls the retrain detector as well -- and
		 * falls through to the FSK demodulator either way, which is
		 * finding F721.
		 */
		obj->retrain_poll = 1;				 /* 0x67f68 */
		obj->paa70 = &obj->msgrec[3];			 /* 0x67f6e */
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
		goto transmit_status_reload;
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
		hs_set_txstate(obj, V34HS_TONE_AB);	/* 0x696d0 */

		/*
		 * 0x69710.  `mov $0xffffffff,%ebp` and then a halfword store,
		 * so +0xaae2 gets 0xffff.  It is the shift register's reset
		 * value and this arm writes it at three sites -- 0x67f46,
		 * 0x68851 and here.
		 */
		obj->fsk.sr = -1;				/* 0x69710 */
		obj->fsk.nbits = 0;				/* 0x69717 */

		/* 0x6971e, and %ecx still holds TONE_AB from 0x696d0. */
		goto transmit_status_reload;
	}

	/*
	 * 0x65140, and the two guards below read what it writes:
	 * `cmpw $0x14,0xaae0` at 0x65145 and `testw $0xfff,0xaae2` at
	 * 0x65153, both sixteen bits, both on fields this call has
	 * just filled in.  No fixture can choose them by poking.
	 */
	fskdemodulate(obj,					/* 0x65140 */
		      rx->burst,
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
				hs_set_rxstate(obj, V34HS_RX_DPSK);
				hs_set_microstate(obj, V34HS_TX_L1);
			}
		}
		goto transmit_status_reload;
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
	hs_set_rxstate(obj, V34HS_RX_DPSK);		/* 0x65162 */
	/*
	 * 0x651e9.  `cmp $0x3e,%dx` on +0x3592 -- 62 -- and it is
	 * also 0x6516d's `je` target, so the microstate transition is
	 * where the rxstate transition's "already there" exit lands.
	 */
	hs_set_microstate(obj, V34HS_RX_PHASE3_CALL);	/* 0x651e9 */

	/*
	 * 0x65289.  The exit, and its %ecx was loaded from +0x3596 at
	 * 0x6527b -- after the trace call at 0x6526a and one
	 * instruction before the microstate store at 0x65282.
	 */

					/* 0x650c6 */
	goto transmit_status_reload;

transmit_status_reload:
	tx = obj->txstate;
transmit_status:
	{
		unsigned idx = (unsigned)((int)tx - T3M_TBL2_FIRST);

		if (idx >= T3M_TBL2_COUNT) {
			goto block_tail;
		}

		switch ((int)tx) {
		case 5:				/* 0x64480 SILENCE */
			/*
			 * Three ways to reach progress 1, all of them a microstate
			 * and rxstate pair agreeing with the answer/originate flag
			 * at +0x359c, and every other way is 0.
			 */
			{
				unsigned short mst = T3M_U16(obj, V34HS_MICROSTATE_OFF);
				unsigned short rx = T3M_U16(obj, V34HS_RXSTATE_OFF);

				if (mst == 0x3f && obj->role == 0x66)
					obj->progress = 1;
				else if (rx == 0x04 && mst == 0x2c
					 && obj->role == 0x65)
					obj->progress = 1;
				else if (rx == 0x23 && mst == 0x3f
					 && obj->role == 0x65)
					obj->progress = 1;
				else
					obj->progress = 0;
			}
			break;

		case 18:			/* 0x64518 SSEG   */
		case 19:			/*          SBARSEG */
			/*
			 * `sbb`/`add $3` is 3 when bit 3 of the receiver's flags is
			 * set and 2 when it is not.  The originate side is 2 whatever
			 * the flag says.
			 */
			if (obj->role == 0x65)
				obj->progress = 2;
			else
				obj->progress = (rx->flags >> 3) & 1 ? 3 : 2;
			break;

		case 20:			/* 0x64509 PPSEG    */
		case 21:			/*         TRNSEG4  */
		case 64:			/*         JTXMIT   */
		case 68:			/*         J1TXMIT  */
			obj->progress = 2;
			break;

		case 24:			/* 0x644c9 TX_DPSK  */
		case 51:			/*         TX_L1    */
		case 54:			/*         SILENCEINFO */
		case 60:			/*         TONE_AB  */
		case 74:			/*         SILENCERETRAIN */
			obj->progress = 0;
			break;

		case 66:			/* 0x644fa TRNSEG4A */
		case 67:			/*         XMITMP   */
		case 69:			/*         EXMIT    */
			obj->progress = 3;
			break;

		case 70:			/* 0x644d8 DATAXMIT */
			/*
			 * `cmp $0x1,%bp` then `sbb %eax,%eax` and `add $0x4,%eax`:
			 * the borrow is set only when the halfword is zero.
			 */
			obj->progress = T3M_U16(obj, T3M_F0E4C) == 0 ? 3 : 4;
			break;

		default:			/* 0x62a40, straight to the tail */
			break;
		}

		goto block_tail;
	}
block_tail:
	{
		int mode = obj->hs_mode;
		int level;

		if (mode == 1) {
			/*
			 * The block at 0x62b45: `ratecfg.rx_baud` is the receive baud rate
			 * and the receiver's +0x1d2 gets three times it, by
			 * `lea (%ebp,%ebp,2)` on the sign-extended halfword, stored
			 * back as one.  THE RELOAD IS THE NEXT INSTRUCTION, 0x62b5f
			 * `movzwl 0x3596(%ebx),%ecx`, and 0x62b66 writes the 4.
			 */
			rx->report_interval = (short)(3 * (int)obj->ratecfg.rx_baud);
			tx = (short)T3M_U16(obj, V34HS_TXSTATE_OFF);
			obj->progress = 4;
		}

		/* 0x62a56 and 0x62a68: both windows are UNSIGNED, so mode 1 and any
		   negative mode fall through both. */
		if ((unsigned)(mode - 4) <= 1u)
			obj->progress = 6;

		if ((unsigned)(mode - 2) <= 1u && tx == 0x4a) {
			/*
			 * 0x64884.  `setg`/`dec`/`and $7` is 0 when `vect_idx` is
			 * above 60 and 7 when it is not -- the arithmetic is the
			 * compiler's way of writing a two-valued select and the 7 is
			 * not a mask of anything.
			 */
			obj->progress = obj->vect_idx > 0x3c ? 0 : 7;
		}

		/* 0x62a7a.  Unsigned, so an elapsed count past the deadline's wrap
		   is late rather than early. */
		if (T3M_U32(obj, T3M_ELAPSED) > T3M_U32(obj, T3M_DEADLINE))
			obj->progress = 8;

		/* 0x62a92.  Signed: the level is a sign-extended halfword. */
		level = rx->agc_level;
		if (level >= obj->rx_energy_floor)
			T3M_I32(obj, T3M_TICK) = 0;
		else
			T3M_I32(obj, T3M_TICK)++;

		if (T3M_I32(obj, T3M_TICK) > 0x257f)
			obj->progress = 9;

		/* 0x62ac5, a three-way compare on the SIGN-EXTENDED halfword. */
		if (tx == 0x53)
			obj->progress = 0x0f;
		else if (tx > 0x53) {
			if (tx == 0x54)
				obj->progress = 0x10;
		} else if (tx == 0x52)
			obj->progress = 0x0d;
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
 * `v34handshakinit` three times. Retrain rechecks the block rate and run
 * count AFTER
 * `v34handshakinit`; renegotiation copies the rate index after init.
 * Later tests reload fields after the preceding indication calls too.
 * Keep those reads at their original call boundaries.
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
 * The three timer spans -- 288,000 here, 144,000 and 1,152,000 in the loop --
 * are all UNSIGNED compares of one int minus another, so a mark ahead of the
 * count reads as an enormous span rather than a negative one.
 */

/* The object keeps receiver and transmit-queue bases in registers.  Use
 * those owners directly: every supervisor word is already modelled in
 * v34_receiver. Receiver-loop comparisons are signed 16-bit despite
 * zero-extending loads; increments store the low 16 bits. Down/up threshold
 * comparisons below use 32 bits. See issue #286 and
 * docs/maintainer-offset-access.md for the operand audit.
 */
#define DP_TIMER_STALE	288000u		/* 0x46500, 36 s at 8 kHz  */
#define DP_TIMER_MID	144000u		/* 0x23280, 18 s           */
#define DP_TIMER_LONG	1152000u	/* 0x119400, 144 s         */
#define DP_RX_BLOCK_CAP	0x752f		/* the last value that still bumps  */

void
datapumpv34(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = &obj->receiver;
	struct v34_queue *tx = &obj->txq;

	if ((unsigned)obj->sample_count
	    - (unsigned)obj->int_0248 > DP_TIMER_STALE)
		obj->progress = 5;

	/*
	 * The handshake, and the only path that calls `v34handshak`.  Its own
	 * prologue reads the same two guards, so an iteration whose arm moves
	 * neither the cursor nor the queue count repeats forever; that is the
	 * object's shape. A fixture must establish progress in the selected
	 * handshake arm, or enter with the loop already satisfied.
	 */
	if ((unsigned)obj->hs_mode > 1u) {
		while (tx->count < obj->tx_fill_target || rx->rxq.count > 5)
			v34handshak(obj);
		return;
	}

	while (tx->count < obj->tx_fill_target)
		modulatevector(obj);

	while (rx->rxq.count > 5) {
		unsigned span;
		short err;

		if (rx->rx_blocks <= DP_RX_BLOCK_CAP)
			rx->rx_blocks = (short)(rx->rx_blocks + 1);

		receiver(obj);

		err = rx->equerr;
		if (err > rx->bad_thresh)
			rx->bad_run = (short)(rx->bad_run + 1);
		else
			rx->bad_run = 0;

		span = (unsigned)obj->sample_count
		     - (unsigned)obj->int_0248;

		if (span > DP_TIMER_MID) {
			if (err > rx->bad_long_thresh)
				rx->bad_long_run = (short)(rx->bad_long_run + 1);
			else
				rx->bad_long_run = 0;
		}
		if (span > DP_TIMER_LONG) {
			if (err < rx->good_thresh)
				rx->good_run = (short)(rx->good_run + 1);
			else
				rx->good_run = 0;
		}
	}

	/*
	 * Retrain: either the receiver asked with bit 6 of +0x122, or the
	 * plain bad-block run has passed half the block rate at +0xaa96.
	 * The mode it leaves behind distinguishes the two AFTER the fact --
	 * the same test again, so a `v34handshakinit` that cleared the run
	 * would report 3 where the entry condition was the flag.
	 */
	if ((rx->flags & V34_RX_FLAG_RETRAIN)
	    || rx->bad_run > (short)(obj->ratecfg.rx_baud >> 1)) {
		v34handshakinit(obj, 1);
		obj->hs_mode =
			 rx->bad_run <= (short)(obj->ratecfg.rx_baud >> 1)
			 ? 3 : 2;
		rx->bad_run = 0;
		rx->bad_long_run = 0;
		rx->good_run = 0;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34TIMING,Retrain started\n");
	}

	/* The far end asked, on bit 5 of the same word, re-read. */
	if (rx->flags & V34_RX_FLAG_RENEG) {
		v34handshakinit(obj, 3);
		rx->good_run = 0;
		obj->hs_mode = 4;
		rx->bad_run = 0;
		rx->bad_long_run = 0;
		rx->rrn_local_dir = 1;
		rx->baud_copy = obj->ratecfg.rxbits;
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
	if (rx->bad_long_run > 2 * (int)obj->ratecfg.rx_baud) {
		v34handshakinit(obj, 2);
		obj->hs_mode = 5;
		rx->bad_run = 0;
		rx->bad_long_run = 0;
		rx->good_run = 0;
		rx->rrn_local_dir = 2;
		rx->baud_copy = obj->ratecfg.rxbits;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34RNEG, rate renegotiation "
					     "DOWN initiated due to large " "error \n");
		VPcmV34IndicateLocalRRN(obj);
	}

	if (rx->good_run > 8 * (int)obj->ratecfg.rx_baud) {
		v34handshakinit(obj, 2);
		rx->good_run = 0;
		obj->hs_mode = 5;
		rx->bad_run = 0;
		rx->bad_long_run = 0;
		rx->rrn_local_dir = 3;
		rx->baud_copy = obj->ratecfg.rxbits;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34RNEG, rate renegotiation "
					     "UP initiated due to small " "error \n");
		VPcmV34IndicateLocalRRN(obj);
	}
}

/*
 * ---------------------------------------------------------------------------
 * txrxdmainit -- build the twelve-short coefficient block from three pairs.
 *
 * `src` is read from +0x4, as three complex coefficients packed (re, im).
 * `dst` gets each of them twice, in the two arrangements a fixed-point
 * complex multiply needs:
 *
 *     dst[0..5]    (re, -im) for each pair -- the conjugates
 *     dst[6..11]   (im,  re) for each pair -- real and imaginary swapped
 *
 * So a caller wanting `a * conj(c)` dots against the first half and `a * c`
 * against the second, without either having to negate or swap at run time.
 * The original spells all twelve stores out.  Each real input supplies a
 * chained assignment (swapped destination first); each imaginary input is
 * read again after its negated store.  Those reloads also preserve the blob's
 * behavior when the two arrays overlap.  See docs/v34-small-rtl.md.
 */
void
txrxdmainit(short *dst, const short *src)
{
	dst[0] = dst[7] = src[2];
	dst[1] = (short)-src[3];
	dst[6] = src[3];
	dst[2] = dst[9] = src[4];
	dst[3] = (short)-src[5];
	dst[8] = src[5];
	dst[4] = dst[11] = src[6];
	dst[5] = (short)-src[7];
	dst[10] = src[7];
}

/*
 * ---------------------------------------------------------------------------
 * v34FreezeEcho -- stop both cancellers adapting.
 *
 * Sets bit 2 of tx_flags and then dumps both cancellers' coefficients.  The
 * dump is the whole reason the debug hooks are carried (see debug.h): with
 * `dsplibs_debug_level` at its shipped zero this function is three stores
 * and two calls, and the message names -- "Near" and "Far" -- are what fix
 * which of echo0 and echo1 is which.
 */
void
v34FreezeEcho(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34HSHAK: Freeze EC\n");

	obj->tx_flags = (short)(obj->tx_flags | V34_EC_FROZEN);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"==== Near Echo Canceller report ======\n");
	V34EchoReportCoeff(&obj->echo0);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"==== Far Echo Canceller report ======\n");
	V34EchoReportCoeff(&obj->echo1);
}

/*
 * ---------------------------------------------------------------------------
 * V34scrambler -- the transmit side of V34descrambler.
 *
 * Same two generators and the same register, run the other way round: the
 * scrambled bit is fed back into the register, where the descrambler feeds
 * back the bit it received.  `mode` selects the generator directly here
 * rather than through the flags word.
 *
 * THE REGISTER SHIFTS RIGHT, which is what makes the tap positions look
 * wrong.  A new bit is OR-ed in at bit 31 and the register is then shifted,
 * so it lands at bit 30 and the bit generated `m` steps ago sits at 30 - m:
 *
 *     bit 26  ->  m = 4   ->  five steps back   ->  x^-5
 *     bit 13  ->  m = 17  ->  eighteen back     ->  x^-18
 *     bit  8  ->  m = 22  ->  twenty-three back ->  x^-23
 *
 * giving 1 + x^-5 + x^-23 for the caller and 1 + x^-18 + x^-23 for the
 * answerer, exactly as V.34 4.2 specifies.  V34descrambler reaches the same
 * two polynomials with taps at 5/18 and 23 because it shifts the other way;
 * see finding F110, which is the same offset trap from the other side.
 *
 * The three-way XOR is spelled as a running increment and a parity test,
 * not as `^`, so a tap that fires twice cancels the same way.  Keep the mode
 * test inside this loop: period GCC unswitches it into the two loops in the
 * blob.  Direct shift-register expressions also let it delay the first load
 * until a positive bit count, then hold the register and sink its store.
 * See docs/v34-small-rtl.md for the source/unswitching controls.
 */
int
V34scrambler(unsigned *sr, short mode, short bits, short nbits)
{
	int mask = (short)((int)(1u << nbits) - 1);
	short i;
	for (i = 0; i < nbits; i = (short)(i + 1)) {
		short parity = 0;
		if (bits & 1)
			parity++;
		bits = (short)(bits >> 1);
		if (mode != 0) {
			if (*sr & 0x00002000u)
				parity++;
		} else {
			if (*sr & 0x04000000u)
				parity++;
		}
		if (*sr & 0x00000100u)
			parity++;
		if (parity & 1)
			*sr |= 0x80000000u;
		*sr >>= 1;
	}
	return (short)((*sr >> (0x1f - (int)nbits)) & (unsigned)mask);
}

/*
 * ---------------------------------------------------------------------------
 * V34SetupDemodulator -- point the receiver at one of the six symbol rates
 * and one of the eight carriers.
 *
 * Two independent lookups, both spelled as compare chains in the original.
 * Neither has a default: an unrecognised rate leaves the timing constants
 * alone and an unrecognised carrier leaves the table pointer alone, so a bad
 * argument keeps whatever the previous call installed rather than failing.
 *
 * THE TIMING CONSTANTS.  `phase_wrap` is the interpolator's wrap and `phase_inc` its
 * step, so `phase_inc / phase_wrap` is the ratio between the symbol rate and the sample
 * rate; `symbol_period` keeps the step as configured, since the timing loop slews the
 * live one; `phase_frac` starts the phase at half a step.  2400 baud is the
 * degenerate case where step equals wrap -- one output per input, no
 * resampling -- and every other rate interpolates down from it.
 *
 * 2800 is the odd one out: 0x3e82 and 0x1f41 where every other rate uses
 * 0x3e80 and 0x1f40.  Two counts on the wrap, one on the initial phase.  A
 * wrap of 0x3e82 makes 2800's ratio 0x3594/0x3e82 rather than 0x3594/0x3e80,
 * which is a closer rational fit to 2800/9600 -- so it reads as deliberate
 * rather than as a typo, but the derivation is owed to task #47.
 *
 * THE CARRIER TABLES are each exactly twice `half_len` shorts long, which is
 * what makes V34demodulate's `carrier[i]` and `carrier[i + half_len]` a cosine
 * and its sine: the second half is the first shifted a quarter cycle.  That
 * relation holds for all eight and is the reason half_len is stored at all.
 */
void
V34SetupDemodulator(void *objp, short baud, short carrier)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = &obj->receiver;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V34SetupDemodulator: baudrate %ld, carrier %ld\n",
			(long)baud, (long)carrier);

	/* Four outputs per call, whatever the rate. */
	rx->out_count = 4;

	switch (baud) {
	case 2400:
		rx->phase_wrap = 0x3e80; rx->phase_inc = 0x3e80;
		rx->symbol_period = 0x3e80; rx->phase_frac = 0x1f40;
		break;
	case 2743:
		rx->phase_inc = 0x36b0; rx->phase_wrap = 0x3e80;
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
		rx->phase_inc = 0x2ee0; rx->phase_wrap = 0x3e80;
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
	default:   break;
	}
}

/*
 * ---------------------------------------------------------------------------
 * polyValue -- the quadratic setInitialPhase fits its timing metric against.
 *
 *     P(k) = -21k^2 + 837k - 354,  truncated to a short.
 *
 * It peaks near k = 20 and is what turns a measured ratio into a phase
 * index; the derivation belongs with task #47.  The coefficient locals and
 * square-first association reproduce period GCC's late constant propagation
 * into multiplication instructions; const coefficients lower differently
 * (see docs/v34-small-rtl.md).
 */
int
polyValue(short k)
{
	int a = -21, b = 837, c = -354;

	return (short)(a * ((int)k * k) + b * k + c);
}

/*
 * ---------------------------------------------------------------------------
 * setInitialPhase -- put the interpolator's phase where the timing metric
 * says the symbol centre is.
 *
 * Three steps.  First find where `timing_out[]` changes sign: the metric is
 * a discriminant, so the crossing is the symbol boundary.  The search only
 * runs when the first two samples already disagree in sign, and it gives up
 * at index 5 with a message rather than looking further.
 *
 * Second, interpolate across the crossing -- `(b - a) / (b + a)` in Q13,
 * with both negated first if `a` is negative so the ratio is computed on the
 * positive side either way.
 *
 * Third, find which of twenty candidate phases the ratio matches, by running
 * the same ratio over `polyValue(i + 20)` against `polyValue(i)` and keeping
 * the closest.  That index then moves the phase by `(10 - i) * 560`, clamped
 * one below the wrap -- so index 10 means "already centred".
 *
 * The two error paths divide by zero if not guarded, and the original guards
 * both and says so in the message; that is a check the author put in, not
 * hardening added here.
 */
void
setInitialPhase(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = &obj->receiver;
	short k = 1;
	int a, b;
	int ratio = 0;
	int best = 0x7d00;
	int besti = 0;
	int i;
	int pos;

	/* Only search when the first pair already straddles the crossing. */
	if ((int)rx->timing_out[0] * rx->timing_out[1] < 0) {
		do {
			k = (short)(k + 1);
		} while ((int)rx->timing_out[k - 1] * rx->timing_out[k] < 0
			 && k <= 4);

		if (k == 5 && DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"setInitialPhase: Error (i==5)  !!!!!!!\n");
		if (k == 5)
			k = 1;
	}

	a = (short)rx->timing_out[k - 1];
	b = (short)rx->timing_out[k];
	if (a < 0) {
		a = (short)-a;
		b = (short)-b;
	}

	/* The order of the pair records which way the metric was going. */
	if (k == 2) {
		rx->timing_idx_a = 2;
		rx->timing_idx_b = 1;
	} else {
		rx->timing_idx_a = 1;
		rx->timing_idx_b = 2;
	}

	if (a + b == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("setInitialPhase() : Error - "
					     "deviding by 0 (samp2+samp1=0)\n");
	} else {
		ratio = (((b - a) << 13) + ((a + b) >> 1)) / (a + b);
	}

	for (i = 0; i <= 0x13; i++) {
		int p0 = polyValue((short)i);
		int p1 = polyValue((short)(i + 20));
		int r = 0;
		int e;

		if (p1 + p0 == 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"setInitialPhase() : Error - deviding "
					"by 0 (polyValue(k2)+polyValue(k)=0)\n");
		} else {
			r = ((((p1 - p0) << 13) + ((p1 + p0) >> 1))
			     / (p1 + p0));
		}

		e = (short)((((ratio - r) * (ratio - r)) + 0x1000) >> 13);
		if (e < best) {
			best = e;
			besti = i;
		}
	}

	pos = (10 - besti) * 0x230 + (unsigned short)rx->phase_frac;

	if ((int)(unsigned short)pos < (int)(short)rx->phase_wrap)
		rx->phase_frac = (short)pos;
	else
		rx->phase_frac = (short)(rx->phase_wrap - 1);
}

/*
 * ---------------------------------------------------------------------------
 * setTimingStateParameters -- install the timing loop's gains for a state.
 *
 * A nine-way switch on `pllcnt`, the timing state, spelled as a jump table.
 * There are TWO tables, chosen by whether `role` is 0x65, and they agree
 * except on states 5, 6 and 7 -- the fast part of the acquisition ramp --
 * so the second is a retuning of the same schedule rather than a different
 * one. Keep both role-specific switches and their direct reporting arms:
 * collapsing these lowers the period compiler's inline cost and absorbs
 * this helper into TimingV34, where the reference retains two calls.
 *
 * States 0 and 1 install nothing.  States 5 and 8 additionally report the
 * timing offset to the V.90 side, as `timing_ppm * 10` -- the only place that
 * number leaves the datapump.
 *
 * The state is compared UNSIGNED against 8, so a negative `pllcnt` misses the
 * table entirely rather than indexing behind it.  That is the bounds check
 * the other three tables in this reconstruction do not have (finding F129).
 */
void
setTimingStateParameters(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = &obj->receiver;
	if (obj->role != 0x65) {
		switch ((int)(short)rx->pllcnt) {
		case 0:
		case 1:
			break;
		case 2:
			rx->timing_p_gain = 0x36b0;
			rx->timing_i_gain = 0x0;
			rx->dwell_limit = 0x190;
			break;
		case 3:
			rx->timing_p_gain = 0x2ee0;
			rx->timing_i_gain = 0xd2;
			rx->dwell_limit = 0x3e8;
			break;
		case 4:
			rx->timing_p_gain = 0x1770;
			rx->timing_i_gain = 0x5a;
			rx->dwell_limit = 0x3e8;
			break;
		case 5:
			rx->timing_p_gain = 0xdac;
			rx->timing_i_gain = 0x1e;
			rx->dwell_limit = 0x3e8;
			VPcmV34LogTimingOffset(obj, (short)(rx->timing_ppm * 10));
			break;
		case 6:
			rx->timing_p_gain = 0x7d0;
			rx->timing_i_gain = 0xa;
			rx->dwell_limit = 0x7d0;
			break;
		case 7:
			rx->timing_p_gain = 0x5dc;
			rx->timing_i_gain = 0x2;
			rx->dwell_limit = 0xfa0;
			break;
		case 8:
			rx->timing_p_gain = 0x1f4;
			rx->timing_i_gain = 0x1;
			rx->dwell_limit = -1;
			VPcmV34LogTimingOffset(obj, (short)(rx->timing_ppm * 10));
			break;
		default:
			break;
		}
	} else {
		switch ((int)(short)rx->pllcnt) {
		case 0:
		case 1:
			break;
		case 2:
			rx->timing_p_gain = 0x36b0;
			rx->timing_i_gain = 0x0;
			rx->dwell_limit = 0x190;
			break;
		case 3:
			rx->timing_p_gain = 0x2ee0;
			rx->timing_i_gain = 0xd2;
			rx->dwell_limit = 0x3e8;
			break;
		case 4:
			rx->timing_p_gain = 0x1770;
			rx->timing_i_gain = 0x5a;
			rx->dwell_limit = 0x3e8;
			break;
		case 5:
			rx->timing_p_gain = 0xdac;
			rx->timing_i_gain = 0x1e;
			rx->dwell_limit = -1;
			VPcmV34LogTimingOffset(obj, (short)(rx->timing_ppm * 10));
			break;
		case 6:
			rx->timing_p_gain = 0xdac;
			rx->timing_i_gain = 0x3;
			rx->dwell_limit = 0x7d0;
			break;
		case 7:
			rx->timing_p_gain = 0x3e8;
			rx->timing_i_gain = 0x2;
			rx->dwell_limit = 0x7d0;
			break;
		case 8:
			rx->timing_p_gain = 0x1f4;
			rx->timing_i_gain = 0x1;
			rx->dwell_limit = -1;
			VPcmV34LogTimingOffset(obj, (short)(rx->timing_ppm * 10));
			break;
		default:
			break;
		}
	}
	if ((unsigned short)rx->pllcnt == 2)
		rx->report_interval = (short)(obj->ratecfg.rx_baud >> 3);
}

/*
 * ---------------------------------------------------------------------------
 * TimingV34 -- the timing recovery loop's state machine and its integrator.
 *
 * Called once per symbol from `receiver`.  Three parts:
 *
 *   THE STATE MACHINE.  `pllcnt` is the state.  -1 means done and returns
 *   immediately.  1 means "start": zero the dwell counter, centre the phase
 *   with setInitialPhase, and move to state 2 -- or to state 6 if `slow_ramp`
 *   says to skip the slow part of the ramp.  Otherwise, once the dwell
 *   counter `dwell_count` reaches the limit `dwell_limit`, the state advances by one and
 *   the gains are reinstalled.  A limit of -1 means never advance.
 *
 *   THE PHASE DETECTOR.  Two timing_out[] entries, indexed by the pair
 *   setInitialPhase chose, are each squared and summed as (I^2 + Q^2) for
 *   two positions; the loop then forms (b - a) / (b + a) in Q15 after
 *   normalising both up until neither has bits above 30.  That
 *   normalisation is a loop with its own 16-step cap, and the shift is
 *   applied to both so the ratio is unaffected -- it is there for the
 *   divide's range, not for accuracy.
 *
 *   THE INTEGRATOR.  error * timing_i_gain in Q15 accumulates into the 32-bit timing_integrator;
 *   error * timing_p_gain in Q11 is added on top per symbol.  The sum is carried in
 *   timing_frac and its whole part, in units of 1/32768 of a symbol, is added to
 *   the interpolator's step phase_inc.  Every 0x1d2 symbols the accumulated
 *   offset is converted to parts per million -- the `* 10000 / n` then
 *   `* 100 / symbol_period` -- and stored in timing_ppm for setTimingStateParameters to
 *   report onward.
 */
void
TimingV34(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = &obj->receiver;
	int state;
	int i0, i1;
	int a, b;
	int err = 0;
	int acc;
	int whole;
	short n;

	state = (unsigned short)rx->pllcnt;

	if (state == 0xffff)
		return;

	if (state == 1) {
		rx->dwell_count = 0;
		setInitialPhase(obj);
		if (rx->slow_ramp == 1) {
			rx->pllcnt = 2;
			rx->slow_ramp = 0;
		} else {
			rx->pllcnt = 6;
		}
		setTimingStateParameters(obj);
		state = (unsigned short)rx->pllcnt;
	}

	/* Dwell: advance a state once dwell_count reaches dwell_limit, which -1 disables. */
	if ((unsigned short)rx->dwell_limit != 0xffff && state != 0) {
		int next = (unsigned short)(rx->dwell_count + 1);

		if (next == (int)(short)rx->dwell_limit) {
			rx->dwell_count = 0;
			rx->pllcnt = (short)(state + 1);
			setTimingStateParameters(obj);
			state = (unsigned short)rx->pllcnt;
		} else {
			rx->dwell_count = (short)next;
		}
	}

	/* The phase detector: two squared magnitudes, differenced. */
	i0 = rx->timing_idx_a;
	i1 = rx->timing_idx_b;
	/*
	 * EARLY AND LATE, either side of the index -- [i-1] and [i+1], not
	 * [i-1] and [i].  Reading them as adjacent gives a discriminator
	 * with no gap in the middle and an error term about 2.5x too large.
	 */
	a = (int)rx->timing_out[i0 - 1] * rx->timing_out[i0 - 1]
	  + (int)rx->timing_out[i0 + 1] * rx->timing_out[i0 + 1];
	b = (int)rx->timing_out[i1 - 1] * rx->timing_out[i1 - 1]
	  + (int)rx->timing_out[i1 + 1] * rx->timing_out[i1 + 1];

	if (state == 0) {
		acc = rx->timing_integrator;
	} else {
		int sh = 0;

		/*
		 * Normalise both up together, capped at 16 steps.  Applied to
		 * both, so the ratio below is unchanged -- this is range for
		 * the divide, not precision.
		 */
		if (a >= 0 && b >= 0) {
			unsigned m = 0x80000000u;

			/*
			 * The counter is incremented BEFORE the first test,
			 * so an exit on `a` still counts the step.  Doing it
			 * after leaves the shift one short and the error
			 * term exactly twice too large.
			 */
			for (;;) {
				m >>= 1;
				sh = (short)(sh + 1);
				if ((unsigned)a & m)
					break;
				if (((unsigned)b & m) != 0 || sh > 15)
					break;
			}
		}

		a = (unsigned short)((a >> ((16 - sh) & 31)));
		b = (unsigned short)((b >> ((16 - sh) & 31)));

		if ((a | b) != 0)
			err = (((b - a) << 15) + ((a + b) >> 1)) / (a + b);

		acc = rx->timing_integrator + (((int)rx->timing_i_gain * err + 0x4000) >> 15);
		rx->timing_integrator = acc;
		acc += ((int)rx->timing_p_gain * err + 0x200) >> 11;
	}

	/* Carry the fraction, hand the whole part to the interpolator. */
	acc += rx->timing_frac;
	whole = (short)((acc + 0x4000) >> 15);
	rx->timing_frac = acc - (whole << 15);
	rx->phase_inc = (short)(whole + (unsigned short)rx->symbol_period);

	/* Signed word count and arithmetic halves match the object (F11544). */
	n = (short)(rx->ppm_count + 1);
	acc = whole + (unsigned short)rx->ppm_acc;

	if (n < (int)(short)rx->report_interval) {
		rx->ppm_count = (short)n;
		rx->ppm_acc = (short)acc;
		return;
	}

	/* Every report_interval symbols: convert the accumulated slip to ppm. */
	{
		int ppm = ((short)acc * 10000 + (n >> 1)) / n;

		ppm = (ppm * 25 * 4 + ((short)rx->symbol_period >> 1))
		      / (short)rx->symbol_period;
		rx->timing_ppm = (short)ppm;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"TimingV34: Timing Offset [ppm] = %d\n",
				(int)(short)ppm);
	}

	rx->ppm_count = 0;
	rx->ppm_acc = 0;
}

/*
 * ---------------------------------------------------------------------------
 * vectpp -- the phase-reference sequence the handshake slices against.
 *
 * Forty-eight complex points at .rodata+0x2c80, packed (re, im) per entry,
 * every one of magnitude 6476 at a multiple of 60 degrees: (6476, 0),
 * (+/-3238, +/-5609) and (-6476, 0).  Six phases, which is V.34's PP signal
 * (10.1.3.5) -- the periodic sequence sent during phase 3 so the receiver can
 * measure the channel's phase response.
 *
 * `receiver` halves both halves on the way out, so the constellation it
 * actually compares against has magnitude 3238.
 *
 * GLOBAL, which is the object's binding and not a convenience: the blob
 * exports `vectpp` because `v34handshak` reads it as well.  Table 1's 20
 * `PPSEG` in the transmit dispatch loads it as forty-eight FOUR-BYTE entries, one
 * (re, im) pair each, where this file reads the same bytes as ninety-six
 * shorts.  It was static while this file was its only reader; the second
 * reader is what made it global, and `t_v34hstx1.c` proves it against
 * `ref_vectpp` the way `probe` and `vect4` are proved.
 */
const short vectpp[96] = {
	6476, 0, 6476, 0, 6476, 0, 6476, 0,
	-3238, 5609, -5609, 3238, -6476, 0, -5609, -3238,
	6476, 0, 3238, 5609, -3238, 5609, -6476, 0,
	6476, 0, 0, 6476, -6476, 0, 0, -6476,
	-3238, 5609, -3238, -5609, 6476, 0, -3238, 5609,
	6476, 0, -5609, 3238, 3238, -5609, 0, 6476,
	6476, 0, -6476, 0, 6476, 0, -6476, 0,
	-3238, 5609, 5609, -3238, -6476, 0, 5609, 3238,
	6476, 0, -3238, -5609, -3238, 5609, 6476, 0,
	6476, 0, 0, -6476, -6476, 0, 0, 6476,
	-3238, 5609, 3238, 5609, 6476, 0, 3238, -5609,
	6476, 0, 5609, -3238, 3238, -5609, 0, -6476,
};

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

/*
 * The five struct v34_bitsource records from V34_MSGREC_BASE end exactly
 * where the object's next member begins -- F634's dual reading of the
 * region, compile-checked (issue #260).  Wave 8 made the region one
 * `msgrec[5]` array, so the extent is the array's own: record 4's word
 * pair (`info_rates`/`rate_mask` in the object's earlier naming) is
 * `msgrec[4].word[0]/word[1]` by construction, and the rec4_base assert
 * that used to pin the boundary retired with it.
 */
typedef char v34msgrec_extent[
	(__builtin_offsetof(struct v34_object, info_caps)
	 == V34_MSGREC_BASE + 5 * (int)sizeof(struct v34_bitsource)) ? 1 : -1];

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
