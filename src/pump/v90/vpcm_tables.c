/*
 * vpcm_tables.c -- the ten coefficient tables of the V.PCM construction path.
 *
 * WHICH TRANSLATION UNIT EACH ONE REALLY BELONGS TO, AND HOW THAT IS KNOWN.
 * All ten are LOCAL symbols, so each was `static` and can only be named from
 * inside the file that defines it.  That makes attribution exact for once,
 * with no bracket and no contiguity fill: a file-local table is defined in the
 * translation unit of the function that references it, and every one of these
 * has exactly one referencing function.  `tools/relocscan.py --at .data:0xNN`
 * gives the reference and it is always a constructor:
 *
 *     entFiltNum, entFiltDen, v34initialbauds  VPcmFloModem's constructor
 *     IIR2100_Coef_{A,B}_{8000,9600}           ANSamToneDetector's constructor
 *     v92echoPreFilter_{a,b}                   V92EchoCanceller's constructor
 *     v92TxPreFilter                           V92Modulator's constructor
 *
 * (each appears twice, because the C1 and C2 constructor variants are separate
 * bodies with the same code.)
 *
 * WHY THEY ARE GATHERED HERE ANYWAY.  Three of those four classes have no
 * source in this tree yet and the fourth would take an unrelated edit, and the
 * precedent is already set: `V34DisconnectThreshTable` is a file-local of
 * `VPcmV34Main.cpp` and lives globalised in `src/pump/v34/v34pcm_tables.c` for
 * the same reason -- a `static` cannot be reached by a differential test, so
 * the table is made global and the blob's is globalised by the harness.
 *
 * WHOEVER RECONSTRUCTS `VPcmFloModem`, `ANSamToneDetector`, `V92EchoCanceller`
 * OR `V92Modulator` SHOULD DELETE THAT CLASS'S TABLES FROM HERE and put them
 * back in the file as `static`.  Nothing will complain if they do not: a new
 * `static double entFiltNum[5]` in `VpcmFloModem.cpp` links perfectly beside
 * this one, the two copies diverge silently, and the test here keeps passing
 * against the copy nobody uses.
 *
 * THEY ARE NOT `const`, AND THAT IS DELIBERATE.  All nine of the coefficient
 * tables are in the blob's `.data` (section 143), not its `.rodata` (129), so
 * the original declared them writable; `v34initialbauds` is the one in
 * `.rodata` and is the one declared `const` here.  Section placement is forced
 * by the declaration rather than chosen by the compiler, so it is evidence in
 * CLAUDE.md's sense and is reproduced.  Nothing writes to any of them.
 *
 * AND THAT CLAIM HAS BEEN LOOKED AT RATHER THAN ASSERTED, because no test can
 * see it -- `t_vpcmtabs.c` compares bytes and a byte in `.rodata` is the same
 * byte.  `readelf -sW build/src/pump/v90/vpcm_tables.o` puts the nine in this
 * object's `.data` (`WA`) and `v34initialbauds` alone in its `.rodata` (`A`),
 * which is the blob's 143/129 split reproduced.  If somebody later adds
 * `const` to one of the nine for tidiness, nothing in `make phase` will
 * notice; that command is the check.
 *
 * THE VALUES ARE REFERENCE BYTES, NOT A GENERATOR.  Deriving a filter design
 * back out of its coefficients is `docs/fastpass.md`'s deferred work; until
 * the retarget wants them at another rate, byte-exact is byte-exact and
 * `test/unit/t_vpcmtabs.c` proves it against the blob's own copy.
 */

#include "dsplib/vpcm_tables.h"

/*
 * ---------------------------------------------------------------------------
 * VPcmFloModem's constructor: an entry filter and the V.34 baud allowances.
 * ---------------------------------------------------------------------------
 */

/*
 * `entFiltNum`, .data+0xe0 -- the NUMERATOR, fourth argument of
 * `GenericIIR<float,double>(5, 5, entFiltDen, entFiltNum, 99)` at
 * .text+0xfbcb.  Indexed by tap, 0 the most recent; five doubles.
 *
 * The five are NEARLY symmetric and NEARLY binomial, and neither is exact.
 * b[0] and b[4] agree to fifteen significant figures and differ in the
 * sixteenth; but b[1]/b[0] is -3.9999759042982252 and b[2]/b[0] is
 * 5.999951808669026, which miss -4 and 6 in the SIXTH significant figure --
 * relative errors of 6.0e-06 and 8.0e-06, a hundred million times coarser than
 * the sixteenth-figure disagreement above and far too coarse to be rounding.
 * So this is a computed design that came out close to (1 - z^-1)^4 rather than
 * that polynomial with rounded coefficients, and it is recorded as an
 * observation about the numbers and not as a derivation.  Contrast
 * `IIR2100_Coef_B_9600` below, where the ratios ARE exact to an ulp and the
 * derivation holds.
 */
double entFiltNum[VPCM_ENTFILT_TAPS] = {
	0.96284330984918198, -3.8513500390114781, 5.777013458394471,
	-3.8513500390114772, 0.96284330984918154,
};

/*
 * `entFiltDen`, .data+0x120 -- the DENOMINATOR, third argument of the same
 * call.  Five doubles, a[0] exactly 1.0 so the caller need not normalise.
 */
double entFiltDen[VPCM_ENTFILT_TAPS] = {
	1.0, -3.9242514351703268, 5.7756331881310974, -3.7784482934930401,
	0.92706723932132973,
};

/*
 * `v34initialbauds`, .rodata+0x3e0 -- six bytes, all 1, copied wholesale into
 * the modem object at +0x217 by VPcmFloModem's constructor (.text+0xfbf5).
 *
 * INDEXED BY V.34 SYMBOL RATE, of which there are six: 2400, 2743, 2800, 3000,
 * 3200 and 3429 baud.  Every entry is 1, so as shipped the table permits all
 * six.
 *
 * THE ELEMENT TYPE IS SETTLED BY THE DESTINATION OFFSETS AND NOT BY THE LOAD.
 * The constructor copies it as a 4-byte load plus a 2-byte load, which is just
 * how GCC moves six bytes and says nothing about the element size.  What says
 * it is where the six bytes LAND: +0x217 and +0x21b, both odd, so the
 * destination is byte-aligned and the array cannot be `short[3]`.
 */
const unsigned char v34initialbauds[V34_INITIAL_BAUDS] = {
	1, 1, 1, 1, 1, 1,
};

/*
 * ---------------------------------------------------------------------------
 * ANSamToneDetector's constructor: the 2100 Hz IIR, one design per sample
 * rate.
 *
 * WHICH PAIR IS USED IS CHOSEN BY THREE SEPARATE `cmp $0x1f40` TESTS on the
 * same value -- .text+0x108c7, +0x10905 and +0x1091d -- one for the B array,
 * one for the A array and one for the shared tap count.  0x1f40 is 8000, so
 * the `_8000` pair is the equality arm and the `_9600` pair is everything
 * else; the name is the design's rate and not a claim that 9600 is the only
 * other one.
 * ---------------------------------------------------------------------------
 */

/*
 * `IIR2100_Coef_A_8000`, .data+0x2a0 -- DENOMINATOR at 8 kHz, third argument
 * of `GenericToneDetector(13, 13, A, B, ...)`.  Thirteen doubles indexed by
 * tap, a[0] exactly 1.0.  A twelfth-order recursive section, and all thirteen
 * coefficients are positive, which is the signature of a lowpass-prototype
 * denominator rather than the alternating signs the 9600 design has.
 */
double IIR2100_Coef_A_8000[IIR2100_TAPS_8000] = {
	1.0, 0.89541491035646703, 6.0172148546971167, 4.3198953361432411,
	14.81300311749296, 8.3172895657106665, 19.12198197948349,
	7.9878009229030278, 13.65601668082814, 3.8260430702025792,
	5.1123037260126232, 0.73105659448409177, 0.78247411963482927,
};

/*
 * `IIR2100_Coef_B_8000`, .data+0x1c0 -- NUMERATOR at 8 kHz, fourth argument of
 * the same call.  Thirteen doubles, all small and all positive.
 */
double IIR2100_Coef_B_8000[IIR2100_TAPS_8000] = {
	0.0024082424508832588, 0.0019498558169405531, 0.011663689293966391,
	0.0076189528889452386, 0.02346701176517511, 0.012047109745466149,
	0.025043121085192872, 0.0096040039766794698, 0.01492339147837984,
	0.0038520411694815452, 0.0046978678998417017, 0.00062087677563193132,
	0.00060868589911538661,
};

/*
 * `IIR2100_Coef_A_9600`, .data+0x240 -- DENOMINATOR at the other rate.  Eleven
 * doubles, a[0] exactly 1.0, signs alternating.
 */
double IIR2100_Coef_A_9600[IIR2100_TAPS_9600] = {
	1.0, -1.943566985186252, 6.4682469545778698, -8.2972076136613016,
	14.445775463170071, -12.64954539798627, 14.337539482872881,
	-8.1733225465473112, 6.3239307716147168, -1.8859534703639531,
	0.96308623821118655,
};

/*
 * `IIR2100_Coef_B_9600`, .data+0x160 -- NUMERATOR at the other rate.  Eleven
 * doubles, and the only table here with a structure worth naming: EVERY
 * ODD-INDEXED ENTRY IS EXACTLY ZERO and the five non-zero ones are the
 * binomial coefficients of (1 - z^-2)^5 scaled by 2.3122e-09.  A five-fold
 * zero pair, in other words -- five zeros at DC and five at Nyquist.
 *
 * That is what makes the table a real test rather than a formality: half its
 * entries are zero, so a test that only checked "not all the same" would pass
 * on a table that had lost the other half.  t_vpcmtabs.c checks the byte
 * pattern against the blob's and the zero positions separately.
 */
double IIR2100_Coef_B_9600[IIR2100_TAPS_9600] = {
	2.312202029234315e-09, 0.0, -1.1561010146171571e-08, 0.0,
	2.3122020292343141e-08, 0.0, -2.3122020292343141e-08, 0.0,
	1.1561010146171571e-08, 0.0, -2.312202029234315e-09,
};

/*
 * ---------------------------------------------------------------------------
 * V92EchoCanceller's constructor: the echo pre-filter, an ARMA pair.
 * ---------------------------------------------------------------------------
 */

/*
 * `v92echoPreFilter_a`, .data+0x360 -- the AR (recursive) half, third argument
 * of `FloatARMA(n, 12, a, b, 99)` at .text+0x1119b.  Twelve floats indexed by
 * tap, a[0] exactly 1.0.
 */
float v92echoPreFilter_a[V92_ECHO_PREFILTER_TAPS] = {
	1.0f, 0.708999991f, -2.30019999f, -2.67490005f, 1.53729999f,
	3.62899995f, 0.467999995f, -2.13840008f, -1.00730002f, 0.423099995f,
	0.344900012f, 0.0285f,
};

/*
 * `v92echoPreFilter_b`, .data+0x320 -- the MA (feed-forward) half, fourth
 * argument of the same call.  Twelve floats, and b[0] is 1.4986 rather than 1,
 * so this pair is not normalised the way the two IIR pairs above are.
 */
float v92echoPreFilter_b[V92_ECHO_PREFILTER_TAPS] = {
	1.49860001f, 0.621999979f, -2.73329997f, -2.60159993f, 1.71739995f,
	3.37879992f, 0.135000005f, -1.75409997f, -0.682799995f, 0.350800008f,
	0.221100003f, 0.00579999993f,
};

/*
 * ---------------------------------------------------------------------------
 * `v92TxPreFilter`, .data+0x3a0 -- V92Modulator's transmit shaping FIR.
 *
 * Thirty-six floats indexed by tap, second argument of `FloatFIR(36,
 * v92TxPreFilter, 99)` at .text+0x15340.  The order is the tap count exactly,
 * so all thirty-six are used.
 *
 * THE SHAPE IS A 35-TAP SYMMETRIC FIR WITH A 36TH SLOT THAT IS ZERO.  Taps 0
 * to 34 mirror about tap 17 -- v[17-k] == v[17+k] to the bit for every k --
 * and tap 35 is +0.0.  So the design is odd-length and linear phase, and the
 * trailing zero is padding to an even count and not a coefficient; a
 * differential test that stopped at 35 would never see it, which is why the
 * one here compares all 144 bytes.
 *
 * Tap 17 is 0.9379 and every other tap is negative or (taps 0, 34) barely
 * positive: a highpass-ish shaper, one big centre tap with a shallow negative
 * skirt.  Taps 1 and 33 are -3.639e-18, which is zero as designed and a
 * rounding residue as stored -- exactly the kind of value a decimal round trip
 * loses, so it is the one the test's mantissa check watches.
 * ---------------------------------------------------------------------------
 */
float v92TxPreFilter[V92_TXPREFILTER_TAPS] = {
	0.000292354584f, -3.63938128e-18f, -0.000459987583f, -0.00129610382f,
	-0.00272257649f, -0.00493108528f, -0.00806269515f, -0.0121834269f,
	-0.0172666069f, -0.0231844559f, -0.0297103822f, -0.0365322642f,
	-0.0432757102f, -0.0495351218f, -0.0549094528f, -0.0590389259f,
	-0.0616387613f, 0.937895119f, -0.0616387613f, -0.0590389259f,
	-0.0549094528f, -0.0495351218f, -0.0432757102f, -0.0365322642f,
	-0.0297103822f, -0.0231844559f, -0.0172666069f, -0.0121834269f,
	-0.00806269515f, -0.00493108528f, -0.00272257649f, -0.00129610382f,
	-0.000459987583f, -3.63938128e-18f, 0.000292354584f, 0.0f,
};
