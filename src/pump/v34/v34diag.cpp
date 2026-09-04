/*
 * v34diag.cpp -- `VPcmV34GetDiagnostics`, the data-mode diagnostics entry
 * point.
 *
 * WHAT IT IS FOR.  The application asks the datapump, at any point in a live
 * call, to fill in a `TAG_DiagnosticResults`: the negotiated symbol rates and
 * carriers in both directions, the bit rates in both directions, a transmit
 * level, a line-quality figure in dB, the round-trip delay, the number of rate
 * renegotiations so far and the recovered timing offset.  It is the difference
 * between "the call failed" and "the call failed and here is why".
 *
 * THREE DATAPUMPS THROUGH ONE DOOR.  `v34_object::status` picks the arm: 1 is
 * a V.90 session, 2 is a V.92 session and anything else is plain V.34 (or a
 * phase-2 probe -- the object's own diagnostic says "V.34 (or P2)").  On the
 * two PCM arms the RECEIVE half of the record belongs to
 * `V90Demodulator::getAT_UD` and this function fills only the transmit half;
 * on the V.34 arm it fills both.  Everything after the arm is common.
 *
 * `xf->info0Layout` is what says whether this modem is the ANALOG end.  It
 * gates the `getAT_UD` call, and the two messages either side of the test name
 * the two cases -- "It is diagnostics of V.90 Analog" when it is set,
 * "...V.90 Digital" when it is not.  A digital-side modem has no downstream
 * receiver to interrogate, which is why the call is skipped and the receive
 * half of the record is left alone.
 *
 * WHY THIS IS A .cpp AND WHY THE SYMBOL IS UNMANGLED.  The object exports
 * `VPcmV34GetDiagnostics` with no mangling, so it was declared `extern "C"`;
 * but one of its calls is a relocation against
 *
 *     _ZNK14V90Demodulator8getAT_UDEP21TAG_DiagnosticResults
 *
 * which a C translation unit cannot name.  `src/pump/v34/v34k56.cpp` and
 * `src/pump/v34/v34info1a.cpp` are the same arrangement for the same reason
 * and state it at length; the stem is separate from `v34pcmif.c` because the
 * Makefile turns both `%.c` and `%.cpp` into `$(BUILD)/%.o`, so a
 * `v34pcmif.cpp` beside it would be two sources racing for one object file.
 *
 * `docs/attribution.json` puts .text+0x75f0 in the `dp_wrapper.c|V34.c`
 * ambiguity, which is the same coarse block every function in `v34pcmif.c`
 * came out of; the file name here is a language constraint and not a claim
 * about which source file the original lived in.
 *
 * ===========================================================================
 * THE SNR IS `VPcmV34GetSNR`, INLINED, AND WE CALL IT INSTEAD
 * ===========================================================================
 *
 * .text+0x76a8..0x76f0 is `VPcmV34GetSNR`'s body verbatim -- the same
 * `sig_energy / equerr` ratio off the receiver, the same two reciprocal loops with
 * 0x1013 (a -6 dB step, counted six at a time) and 0x32d6 (a -1 dB step,
 * counted one at a time), and the same `>> 14`.  The two functions are in one
 * translation unit in the original and `-O3` inlined the callee.
 *
 * They are in two translation units here, so this is a call.  That is a
 * FACTORING difference and not a behavioural one: `make phase` compares what
 * the record ends up holding, and `compare.py` will read this function as
 * shorter by the inlined body for as long as the split lasts.  Writing the
 * loops out a second time would be a second copy to keep in step for a codegen
 * score, which finding F605 is the standing argument against.
 *
 * ===========================================================================
 * WHAT IS FORCED IN HERE
 * ===========================================================================
 *
 * 1. THE V.92 UPSTREAM RATE IS FLOATING-POINT AND UNSIGNED AT BOTH ENDS.
 *    `imul $0x1f40` makes a 32-bit product, `xor %edx,%edx` then `fildll`
 *    converts it as a 64-bit value whose high word is a hard zero -- which is
 *    how this compiler converts an `unsigned` to floating point -- and
 *    `fistpll` into eight bytes with only the low four read back is how it
 *    converts the other way.  So both conversions are unsigned, and the
 *    arithmetic between them is x87 at 80 bits: `fmuls` against the float
 *    1/12 and `fadd` of the float 0.5.  Written as integer arithmetic it
 *    would not reproduce -- see finding F5503.
 *
 * 2. BOTH `fistp`s ROUND TOWARD ZERO.  `fnstcw`, `or $0xc00`, `fldcw`,
 *    `fistp`, `fldcw` is what a C cast to an integer type compiles to, and
 *    the 0.5 added first is the source's own rounding rather than the
 *    hardware's.
 *
 * 3. `-12.0f - tx_pwr_reduction` IS A REVERSED SUBTRACT AND THE OPERAND ORDER IS THE
 *    OBJECT'S.  `filds` puts the power reduction on the stack and `fsubrs`
 *    against the constant computes `constant - st(0)`.  (The FSUBR trap of
 *    finding F245 is the `DE` POP encodings; this is `d8 /5`, which objdump
 *    renders correctly.)
 *
 * 4. THE V.92 ARM STORES ITS -12.0f WITH AN INTEGER `mov`.  A float constant
 *    with no arithmetic on it is a 32-bit move in this compiler, exactly as
 *    `getAT_UD` +0x074 is; CLAUDE.md's free column.
 *
 * 5. +0x228's ZERO TEST IS ON THE SHORT AND THE STORE IS SIGN-EXTENDED.  The
 *    load is `movzwl` and the widening is `cwtl`, so the upper half of the
 *    load never survives -- finding F614's case, and the value stored is
 *    `(int)(short)v90_timing_offset`.
 */

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/TAG_DiagnosticResults.h"
#include "dsplib/V90Demodulator.h"
#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92Modulator.h"
#include "dsplib/V92Transmitter.h"
#include "dsplib/int_complex.h"
#include "dsplib/K56FlexFloModem.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"
#include "dsplib/VPcmFloModem.h"

/*
 * `v34_object::status` when a PCM receiver is running.  v34fsk.h records the
 * pair as "1 and 2 mean a PCM receiver is running" from three other functions
 * that test `(unsigned)(status - 1) <= 1`; this function is what separates
 * them, because it prints "V.90" on one and "V.92" on the other.
 */
#define V34DIAG_STATUS_V90	1
#define V34DIAG_STATUS_V92	2

/*
 * 0x1f40.  The V.92 upstream symbol rate: PCM upstream runs at the codec's own
 * 8000 symbols per second, which is why the same constant is the transmit
 * BAUD rate on this arm and the receive one in `getAT_UD`.  Spelled here
 * rather than reached from v34pcmif.c's `PCMIF_PCM_BAUD`, which is a private
 * constant of that file.
 */
#define V34DIAG_PCM_BAUD	8000

/*
 * Twelve symbols is the V.92 upstream mapping frame:
 * `V92BitsToSymbol::bitsPer12Symbols` is the author's "K", and
 * `V92Transmitter::process` consumes exactly K input bits per twelve output
 * samples.  So `8000 * K / 12` is bits per second, and the divisor is a
 * frame length rather than a scale factor.
 */
#define V34DIAG_UPSTREAM_SYMBOLS_PER_FRAME	12.0f

/*
 * The upstream rate is published only while the V.92 modulator is in its DATA
 * phase, and reported as zero in every other phase.  When this was first
 * written the field was `word_2c` and the test was a bare 3 with no meaning
 * attached; `V92Modulator` has since named the field `phase` and recovered all
 * three values from the messages printed beside their stores, so this is that
 * modulator's constant rather than a second spelling of the same number.
 */
#define V34DIAG_V92_UPSTREAM_ACTIVE	V92MOD_PHASE_DATA

/* The transmit level a V.PCM upstream transmitter reports; see +0x06c. */
#define V34DIAG_TX_LEVEL_BASE_DB	(-12.0f)

/*
 * `struct v34_ratecfg`'s `txbits` and `rxbits` count units of 2400 bps, so
 * the bit rate is 2400 times the count -- v34fsk.h says so at the struct and
 * `VPcmV34GetCurrentTxBitRate` multiplies the same units by the same figure.
 */
#define V34DIAG_BPS_PER_RATE_UNIT	2400

void
VPcmV34GetDiagnostics(void *objp, struct TAG_DiagnosticResults *results)
{
	struct v34_object *obj = (struct v34_object *)objp;
	const struct v34_ratecfg *cfg = (const struct v34_ratecfg *)
	    ((const unsigned char *)obj + V34_RATECFG);
	VPcmFloModem *xf = (VPcmFloModem *)obj->p3548;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34GetDiagnostics called...\r\n");

	if (obj->status == V34DIAG_STATUS_V90) {
		if (xf->info0Layout != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("...It is diagnostics "
						     "of V.90 Analog...\r\n");

			xf->modem.demodulator->getAT_UD(results);

			/*
			 * The upstream of a V.90 call is V.34, so the
			 * transmit half comes out of the same rate config
			 * the pure V.34 arm below uses.  The receive half
			 * was `getAT_UD`'s.
			 */
			results->txBaudRate = cfg->baud;
			results->txCarrier = cfg->carrier;
			results->txDataRate =
			    V34DIAG_BPS_PER_RATE_UNIT * cfg->txbits;
			results->float_06c = V34DIAG_TX_LEVEL_BASE_DB -
					     obj->tx_pwr_reduction;
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf("...It is diagnostics of V.90 "
					     "Digital...\r\n");
		}
	} else if (obj->status == V34DIAG_STATUS_V92) {
		if (xf->info0Layout != 0) {
			V92Modulator *mod;

			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("...It is diagnostics "
						     "of V.92 Analog...\r\n");

			xf->modem.demodulator->getAT_UD(results);

			/*
			 * The upstream of a V.92 call is PCM, not V.34: the
			 * symbol rate is the codec's 8000, there is no
			 * carrier, and the transmit level carries no power
			 * reduction.
			 */
			results->txBaudRate = V34DIAG_PCM_BAUD;
			results->float_06c = V34DIAG_TX_LEVEL_BASE_DB;

			mod = xf->v92modem.modulator;
			if (mod->phase == V34DIAG_V92_UPSTREAM_ACTIVE) {
				unsigned int bits = (unsigned int)
				    V34DIAG_PCM_BAUD * (unsigned int)
				    mod->bitsToSymbol->transmitter->K;

				results->txDataRate = (unsigned int)(0.5f +
				    (float)bits *
				    (1.0f /
				     V34DIAG_UPSTREAM_SYMBOLS_PER_FRAME));
			} else {
				results->txDataRate = 0;
			}

			results->txCarrier = 0;
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf("...It is diagnostics of V.92 "
					     "Digital...\r\n");
		}
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("...It is diagnostics of V.34 "
					     "(or P2)...\r\n");

		results->txBaudRate = cfg->baud;
		results->rxBaudRate = cfg->rx_baud;
		results->txCarrier = cfg->carrier;
		results->rxCarrier = cfg->rx_carrier;
		results->txDataRate = V34DIAG_BPS_PER_RATE_UNIT * cfg->txbits;
		results->dataRate   = V34DIAG_BPS_PER_RATE_UNIT * cfg->rxbits;
		results->float_06c = V34DIAG_TX_LEVEL_BASE_DB - obj->tx_pwr_reduction;

		/*
		 * The round-trip delay goes in unscaled, into both offsets.
		 * `getAT_UD` divides its own by 9.6 before storing it at
		 * +0x084; this writer does not.  Finding F5502.
		 */
		results->word_080 = results->roundTripDelay =
		    (unsigned int)obj->rtd;

		results->float_070 = results->float_074 =
		    (float)VPcmV34GetSNR(obj);
	}

	/* Common to every arm. */
	results->txBpsLatched = (unsigned int)obj->tx_bps;
	results->rxBpsLatched = (unsigned int)obj->rx_bps;
	results->word_0ec = (unsigned int)(obj->rrn_local + obj->rrn_remote);
	results->word_0f4 = (unsigned int)obj->short_ac14;
	results->word_0e8 = results->word_100 = results->word_0f0 =
	    (unsigned int)obj->short_ac12;

	if (obj->v90_timing_offset != 0)
		results->word_228 = (unsigned int)obj->v90_timing_offset;
	else
		results->word_228 = (unsigned int)-1;
}

/*
 * ===========================================================================
 * `VPcmV34GetVisualDiagnostics` -- 1,023 bytes, nine selectors, four sources
 * ===========================================================================
 *
 * WHAT IT IS FOR.  A live scope on the receiver: the constellation the
 * demodulator is deciding against, the linear equaliser's taps, the
 * decision-feedback filter's, the resampler's phase and offset, and both echo
 * cancellers' coefficients.  `VPcmV34GetDiagnostics` says how the call is
 * doing in numbers; this says what the receiver is looking at.
 *
 * NINE SELECTORS AND A JUMP TABLE.  `cmp $0x8,%eax; ja` then
 * `jmp *0x298(,%eax,4)` -- the table is nine `.text` relocations at
 * .rodata+0x298 and the range check is unsigned, so a `switch` on 0..8 with
 * everything else answering zero.  Selector 7's entry points straight at the
 * common exit, which is a case that does nothing rather than a hole in the
 * table.
 *
 * FOUR SOURCES PER SELECTOR, AND THE SAME THREE-WAY TEST PICKS BETWEEN THEM.
 * `v34_object::status` says which datapump is running -- 1 V.90, 2 V.92, 3
 * K56flex, 0 plain V.34 -- and for status 1 and 3 the answer ALSO depends on
 * `role`, this modem's role.  Written out, the shape the first three
 * selectors share is
 *
 *     status == 2                             -> the VPcmFloModem
 *     status == 1 && role == PCMIF_ROLE_ANSWER-> the VPcmFloModem
 *     status == 3 && role == VDIAG_ROLE_CALL  -> the K56FlexFloModem
 *     otherwise                                -> V.34's own state, or none
 *
 * The role test is why a V.90 CALLER falls through to the V.34 arm: on that
 * side of the call there is no PCM receiver to interrogate, exactly as
 * `VPcmV34GetCurrentRxBaudRate` and its three neighbours in v34pcmif.c test
 * the same field for the same reason.
 *
 * WHAT IS FORCED IN HERE
 *
 * 1. SELECTORS 3 AND 4 IGNORE `maxCount` ENTIRELY.  Deviation D710: they
 *    write eight bytes at `points[0]` and return 1 whatever the caller said
 *    the buffer would hold, including zero.  Reproduced.
 *
 * 2. THEY ALSO LEAVE THE REAL HALF ALONE ON THE PCM ARMS.  With `status` 1 or
 *    2 only `points[0].im` is written; the real half keeps whatever the
 *    caller left there.  Invisible unless the array is seeded, which is why
 *    t_v34diag.cpp seeds it.
 *
 * 3. THE TWO K56flex RESAMPLER CALLS DISCARD THEIR RESULT.  Every other
 *    K56flex call in this function ends `mov %eax,%ebx`; those two jump
 *    straight to the shared tail that stores zero and returns 1.
 *
 * 4. SELECTOR 0's V.34 ARM DRAINS A RING AND THE DRAIN IS UNCONDITIONAL.
 *    `mov %bp,0x2aa4(%ecx)` with %bp zero happens before the loop and runs
 *    even when the loop does not, so asking for zero points still resets the
 *    write cursor and throws the buffered residuals away.
 *
 * 5. `v34_object::pac18` IS A `K56FlexFloModem *`.  Five call sites pass it
 *    as the first stack argument of a member of that class, which is `this`
 *    in this object (finding F215).  The FIELD is not retyped -- the class has
 *    no data members, so the type carries no layout, and `V34GiveINFO1aBits`
 *    reads an int at +0xc through the same pointer.  Finding F5510.
 */

/* `v34_object::status` for a K56flex session; 1 and 2 are the two PCM ones. */
#define V34DIAG_STATUS_K56FLEX	3

/*
 * `role` on the side that ORIGINATES.  `PCMIF_ROLE_ANSWER` is 0x66 and lives
 * in v34pcmif.c, which is a different translation unit with no shared private
 * header; 0x65 is the value this function tests for on every K56flex arm, and
 * `V34SetINFO1aBits`, `preinitdigital`, `initdigital`, `v34modeminit` and
 * `v34handshakinit` all test the same field against the same pair.
 */
#define VDIAG_ROLE_CALL		0x65
#define VDIAG_ROLE_ANSWER	0x66

/*
 * The nine selectors, in the jump table's order.  The names are what each arm
 * READS, which is the only thing the object says about them: there is no
 * string, no enum and no caller inside dsplibs.o -- the caller is the
 * application.
 */
#define VDIAG_CONSTELLATION	0
#define VDIAG_LINEAR_EQUALIZER	1
#define VDIAG_DFE		2
#define VDIAG_RESAMPLER_PHASE	3
#define VDIAG_RESAMPLER_OFFSET	4
#define VDIAG_ECHO_NEAR		5
#define VDIAG_ECHO_FAR		6
#define VDIAG_NOTHING		7
#define VDIAG_DECISION_ERRORS	8

/*
 * The V.34 receiver's own offset inside the object; v34pcmif.c reaches it the
 * same way at four places, and v34recv.h's `V34_RX_EQ_OFFSET` then reaches the
 * adaptive equaliser inside it.
 *
 * THE EQUALISER ARM'S CAP IS `V34_EQ_TAPS` AND THAT IS MEASURED, not a
 * coincidence: `cmp $0x50,%ebx; jbe` at 0x7356 clamps `maxCount` to 80, the
 * two arrays it then walks are at 0x50c and 0x5ac off the receiver, and
 * 0x264 + 0x3cc + 0x140 and + 0x1e0 are exactly those -- so they are
 * `v34_equalizer::re` and `::im`, whose declared length is `V34_EQ_TAPS` and
 * whose separation is 0xa0, which is 80 shorts.  The literal in the object
 * and the array bound in the header are the same eighty.
 */
#define V34_RECEIVER_OFFSET	0x264

/*
 * The V.34 constellation history's scale.  Both halves are doubled on the way
 * out -- `add %eax,%eax` at 0x7320 and 0x7322 -- which is a Q15-to-Q14 shift
 * written as a multiply by two and is all the object says about it.
 */
#define VDIAG_V34_POINT_SCALE	2

/*
 * The float scale on the two echo-canceller arms and the V.92 one, and it is
 * the same ten thousand `VPcmFloModem::getLinearEqualizer` uses -- a third
 * copy of the literal, in a third translation unit of the original.
 */
#define VDIAG_ECHO_SCALE	10000.0f

unsigned long
VPcmV34GetVisualDiagnostics(void *objp, int what, struct int_complex *points,
			    unsigned long maxCount)
{
	struct v34_object *obj = (struct v34_object *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)
	    ((unsigned char *)obj + V34_RECEIVER_OFFSET);
	VPcmFloModem *xf = (VPcmFloModem *)obj->p3548;
	K56FlexFloModem *k56 = (K56FlexFloModem *)obj->pac18;
	unsigned long n = 0;
	unsigned long i;

	switch (what) {
	case VDIAG_CONSTELLATION:
		if (obj->status == V34DIAG_STATUS_V92 ||
		    (obj->status == V34DIAG_STATUS_V90 &&
		     obj->role == VDIAG_ROLE_ANSWER)) {
			n = xf->getConstellation(points, maxCount);
		} else if (obj->status == V34DIAG_STATUS_K56FLEX &&
			   obj->role == VDIAG_ROLE_CALL) {
			n = (unsigned long)k56->getConstellation(points,
								 maxCount);
		} else {
			/*
			 * V.34's own residual ring, and reading it EMPTIES
			 * it: `hist1_idx` is the write cursor `modem_serrint`
			 * advances and it is cleared here whether or not any
			 * point comes out.
			 */
			n = (unsigned int)obj->hist1_idx;
			if (n > maxCount)
				n = maxCount;
			obj->hist1_idx = 0;

			for (i = 0; i < n; i++) {
				points[i].re = VDIAG_V34_POINT_SCALE *
				    obj->hist_2aa8[i][0];
				points[i].im = VDIAG_V34_POINT_SCALE *
				    obj->hist_2aa8[i][1];
			}
		}
		break;

	case VDIAG_LINEAR_EQUALIZER:
		if (obj->status == V34DIAG_STATUS_V92 ||
		    (obj->status == V34DIAG_STATUS_V90 &&
		     obj->role == VDIAG_ROLE_ANSWER)) {
			n = xf->getLinearEqualizer(points, maxCount);
		} else if (obj->status == V34DIAG_STATUS_K56FLEX &&
			   obj->role == VDIAG_ROLE_CALL) {
			n = (unsigned long)k56->getLinearEqualizer(points,
								   maxCount);
		} else {
			const struct v34_equalizer *q =
			    (const struct v34_equalizer *)
			    ((const unsigned char *)rx + V34_RX_EQ_OFFSET);

			n = maxCount;
			if (n > V34_EQ_TAPS)
				n = V34_EQ_TAPS;

			for (i = 0; i < n; i++) {
				points[i].re = VDIAG_V34_POINT_SCALE * q->re[i];
				points[i].im = VDIAG_V34_POINT_SCALE * q->im[i];
			}
		}
		break;

	case VDIAG_DFE:
		if (obj->status == V34DIAG_STATUS_V92 ||
		    (obj->status == V34DIAG_STATUS_V90 &&
		     obj->role == VDIAG_ROLE_ANSWER)) {
			n = xf->getDFE(points, maxCount);
		} else if (obj->status == V34DIAG_STATUS_K56FLEX &&
			   obj->role == VDIAG_ROLE_CALL) {
			n = (unsigned long)k56->getDFE(points, maxCount);
		}
		/* V.34 has no decision-feedback filter to report. */
		break;

	case VDIAG_RESAMPLER_PHASE:
		/*
		 * D710: no `maxCount` anywhere in this arm.  Eight bytes go
		 * into `points[0]` and 1 comes back, whatever the caller said
		 * the buffer would hold.
		 */
		if (obj->status != V34DIAG_STATUS_V90 &&
		    obj->status != V34DIAG_STATUS_V92) {
			if (obj->status == V34DIAG_STATUS_K56FLEX &&
			    obj->role == VDIAG_ROLE_CALL)
				(void)k56->getResamplerPhase(points, maxCount);
			else
				points[0].re = rx->timing_frac;
		}
		points[0].im = 0;
		n = 1;
		break;

	case VDIAG_RESAMPLER_OFFSET:
		/* D710 again, and the same shape one field along. */
		if (obj->status != V34DIAG_STATUS_V90 &&
		    obj->status != V34DIAG_STATUS_V92) {
			if (obj->status == V34DIAG_STATUS_K56FLEX &&
			    obj->role == VDIAG_ROLE_CALL)
				(void)k56->getResamplerOffset(points, maxCount);
			else
				points[0].re = rx->timing_offset;
		}
		points[0].im = 0;
		n = 1;
		break;

	case VDIAG_ECHO_NEAR:
		if (obj->status == V34DIAG_STATUS_V92) {
			/*
			 * A V.92 session cancels its echo in the PCM modem
			 * rather than in the V.34 object, so the taps come
			 * out of the embedded V92EchoCanceller as floats.
			 */
			const float *coefs;

			n = xf->echoCanceller.filterLength;
			if (n > maxCount)
				n = maxCount;
			coefs = xf->echoCanceller.echoCoeff;

			for (i = 0; i < n; i++) {
				points[i].re = 0;
				points[i].im = (int)(coefs[i] *
						     VDIAG_ECHO_SCALE);
			}
		} else {
			const short *coefs;

			n = obj->echo0.taps;
			if (n > maxCount)
				n = maxCount;
			coefs = obj->echo0.coeff;

			for (i = 0; i < n; i++) {
				points[i].re = 0;
				points[i].im = coefs[i];
			}
		}
		break;

	case VDIAG_ECHO_FAR:
		/*
		 * The second canceller, and V.34 ONLY: `test %edx,%edx; jne`
		 * on `status` sends every PCM session and every K56flex one
		 * straight out with zero.
		 */
		if (obj->status == 0) {
			const short *coefs;

			n = obj->echo1.taps;
			if (n > maxCount)
				n = maxCount;
			coefs = obj->echo1.coeff;

			for (i = 0; i < n; i++) {
				points[i].re = 0;
				points[i].im = coefs[i];
			}
		}
		break;

	case VDIAG_DECISION_ERRORS:
		/*
		 * K56flex alone, and the two PCM sessions are excluded by a
		 * RANGE test rather than by two equalities:
		 * `lea -0x1(%edx),%eax; cmp $0x1,%eax; jbe` at 0x72ba.
		 */
		if ((unsigned int)(obj->status - 1) > 1u &&
		    obj->status == V34DIAG_STATUS_K56FLEX &&
		    obj->role == VDIAG_ROLE_CALL)
			n = (unsigned long)k56->getDecisionErrors(points,
								  maxCount);
		break;

	default:
		/* Selector 7, and everything above 8. */
		break;
	}

	return n;
}
