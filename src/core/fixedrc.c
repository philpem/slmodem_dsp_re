/*
 * fixedrc.c -- fixed rational-factor sample rate conversion.
 *
 * Reconstructed from dsplibs.o FixedRC.c, .text 0x0b0d90-0x0b1cf0 and the
 * factor tables at .data 0x94e0 / 0x9540.
 *
 * This is the module that lets the datapumps run at 8 kHz while the host runs
 * at 9600.  RcFixed_Resample() is called from exactly four places in the
 * original: dp_wrapper_run(), call_run(), FAX_process() and VOICE_process().
 *
 * STATUS: partial.  RcFixed_Check_Combination() and the factor tables are
 * complete and differentially verified.  RcFixed_Create(), _Reset(), _Delete()
 * and _Resample() are not yet reconstructed -- see the note at the end of this
 * file for what is known about them so far.
 */

#include "dsplib/fixedrc.h"

/*
 * Supported conversion ratios, as (down, up) pairs indexed by mode.
 *
 * The conversion applied is out = in * up / down, so a mode's *down* factor is
 * matched against the input rate and its *up* factor against the output rate,
 * both reduced by their GCD.
 *
 * Reading the interesting entries:
 *
 *   mode 2   5:6    8000 -> 9600   pump to host
 *   mode 3   6:5    9600 -> 8000   host to pump      <- the common case
 *   mode 14  2:3    e.g. 8000 -> 12000
 *   mode 15  3:2
 *   mode 18  9:10   e.g. 7200 -> 8000
 *   mode 19  10:9
 *
 * Modes 8 and 9 duplicate the ratios of modes 0 and 1 (4:1 and 1:4).  They are
 * distinct entries because each mode selects its own filter design in
 * RcFixed_Create()'s jump table, so the same ratio can be offered at two
 * different filter lengths or bandwidths.
 *
 * The trailing {0, 0} is a terminator, and its index doubles as the
 * "unsupported" return value from RcFixed_Check_Combination().
 */
static const int fixedRc_DownFact[RCFIXED_NMODES + 1] = {
	1, 4, 5, 6, 1, 6, 1, 5, 1, 4,
	5, 24, 4, 5, 2, 3, 3, 10, 9, 10,
	0,
};

static const int fixedRc_UpFact[RCFIXED_NMODES + 1] = {
	4, 1, 6, 5, 6, 1, 5, 1, 4, 1,
	24, 5, 5, 4, 3, 2, 10, 3, 10, 9,
	0,
};

int
RcFixed_UpFactor(int mode)
{
	return (mode >= 0 && mode <= RCFIXED_NMODES) ? fixedRc_UpFact[mode] : 0;
}

int
RcFixed_DownFactor(int mode)
{
	return (mode >= 0 && mode <= RCFIXED_NMODES) ? fixedRc_DownFact[mode] : 0;
}

/*
 * Greatest common divisor, by Euclid.
 *
 * Matches the original's loop exactly, including its behaviour on zero and
 * negative inputs: it uses the C remainder operator, so gcd(x, 0) == x and
 * signs propagate rather than being normalised.  Callers pass positive rates,
 * but the differential test covers the edges anyway.
 */
static int
gcd(int a, int b)
{
	while (b != 0) {
		int t = a % b;
		a = b;
		b = t;
	}
	return a;
}

int
RcFixed_Check_Combination(int in_rate, int out_rate)
{
	int g, down, up, mode;

	g = gcd(in_rate, out_rate);

	down = in_rate / g;
	up = out_rate / g;

	/*
	 * Start at 2, skipping the explicit-only x4 and /4 entries, and stop
	 * at the {0,0} terminator.  Returning the terminator's index on no
	 * match is deliberate: RcFixed_Create() treats any mode above the
	 * table as "no conversion required".
	 */
	for (mode = 2; fixedRc_UpFact[mode] != 0; mode++) {
		if (fixedRc_UpFact[mode] == up && fixedRc_DownFact[mode] == down)
			break;
	}

	return mode;
}

/*
 * Still to reconstruct: RcFixed_Create/_Reset/_Delete/_Resample.
 *
 * What is established so far, from the original:
 *
 *   RcFixed_Create(mode)
 *       Rejects mode > 999 outright.  Allocates an 8-byte handle
 *       { int kind; void *state; }.  Modes 0 and 1 take a separate path;
 *       modes 2..19 allocate 420 bytes of state, store the mode's up factor
 *       at state+0x198 and its down factor at state+0x196 (both u16), then
 *       dispatch through a 20-way jump table at .rodata+0x10f14 to a
 *       per-mode filter initialiser.  Modes above 19 leave kind = 0 and no
 *       state, i.e. an identity converter.
 *
 *   RcFixed_Resample()
 *       0xa50 bytes at .text 0x0b12a0 -- a polyphase FIR, with the phase
 *       accumulator and history buffer living in the 420-byte state block.
 *
 * The per-mode coefficient sets reached through that jump table are the real
 * content here, and each needs extracting with tools/tabdump.py and pairing
 * with a generator that reproduces it from its design parameters, per
 * docs/coefficients.md.
 */
