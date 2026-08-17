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
 * `f248 / f21a` ratio off the receiver, the same two reciprocal loops with
 * 0x1013 (a -6 dB step, counted six at a time) and 0x32d6 (a -1 dB step,
 * counted one at a time), and the same `>> 14`.  The two functions are in one
 * translation unit in the original and `-O3` inlined the callee.
 *
 * They are in two translation units here, so this is a call.  That is a
 * FACTORING difference and not a behavioural one: `make phase` compares what
 * the record ends up holding, and `compare.py` will read this function as
 * shorter by the inlined body for as long as the split lasts.  Writing the
 * loops out a second time would be a second copy to keep in step for a codegen
 * score, which finding 605 is the standing argument against.
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
 *    would not reproduce -- see finding 5503.
 *
 * 2. BOTH `fistp`s ROUND TOWARD ZERO.  `fnstcw`, `or $0xc00`, `fldcw`,
 *    `fistp`, `fldcw` is what a C cast to an integer type compiles to, and
 *    the 0.5 added first is the source's own rounding rather than the
 *    hardware's.
 *
 * 3. `-12.0f - f25dc` IS A REVERSED SUBTRACT AND THE OPERAND ORDER IS THE
 *    OBJECT'S.  `filds` puts the power reduction on the stack and `fsubrs`
 *    against the constant computes `constant - st(0)`.  (The FSUBR trap of
 *    finding 245 is the `DE` POP encodings; this is `d8 /5`, which objdump
 *    renders correctly.)
 *
 * 4. THE V.92 ARM STORES ITS -12.0f WITH AN INTEGER `mov`.  A float constant
 *    with no arithmetic on it is a 32-bit move in this compiler, exactly as
 *    `getAT_UD` +0x074 is; CLAUDE.md's free column.
 *
 * 5. +0x228's ZERO TEST IS ON THE SHORT AND THE STORE IS SIGN-EXTENDED.  The
 *    load is `movzwl` and the widening is `cwtl`, so the upper half of the
 *    load never survives -- finding 614's case, and the value stored is
 *    `(int)(short)fac0c`.
 */

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/TAG_DiagnosticResults.h"
#include "dsplib/V90Demodulator.h"
#include "dsplib/V92BitsToSymbol.h"
#include "dsplib/V92Modulator.h"
#include "dsplib/V92Transmitter.h"
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
 * `V92Modulator::word_2c` selects the upstream mode.  Only the value 3
 * publishes a rate; every other value reports zero, and nothing
 * reconstructed says what the other values are, so the constant states the
 * test and not a meaning.
 */
#define V34DIAG_V92_UPSTREAM_ACTIVE	3

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
					     obj->f25dc;
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
			if (mod->word_2c == V34DIAG_V92_UPSTREAM_ACTIVE) {
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
		results->float_06c = V34DIAG_TX_LEVEL_BASE_DB - obj->f25dc;

		/*
		 * The round-trip delay goes in unscaled, into both offsets.
		 * `getAT_UD` divides its own by 9.6 before storing it at
		 * +0x084; this writer does not.  Finding 5502.
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

	if (obj->fac0c != 0)
		results->word_228 = (unsigned int)obj->fac0c;
	else
		results->word_228 = (unsigned int)-1;
}
