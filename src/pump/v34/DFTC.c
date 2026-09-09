/*
 * DFTC.c -- ITU-T V.34: a bank of single-bin sliding DFTs.
 *
 * Where detector.c answers "is this one tone present", this answers "how much
 * energy is at each of these frequencies" -- which is what V.34 phase 2 needs
 * to read the line probe, and what several handshake states use to tell one
 * expected tone from another.  Between them, `dftupdate` and `dftenergy` are
 * called nineteen times from v34handshak and twice from V34RX.c.
 *
 * Each bin is an independent phase accumulator stepping through one shared
 * cosine table, so a bin's frequency is entirely a matter of its `inc` and
 * nothing here needs to know what the sample rate is.
 *
 * TWO ACCUMULATORS, ONE CORRELATION.  Every bin carries the same product
 * twice: once as a 32-bit integer scaled down by 64, and once as a double at
 * full width.  `dftenergy` reduces both, writing `energy` from the integer
 * pair and `denergy` from the double pair.  Nothing in the object reads
 * `denergy`, and nothing reads `cosread` either -- see docs/findings.md.
 * Both are reproduced: they cost only their own arithmetic, and removing
 * something merely because this reconstruction has not yet found its reader
 * would be a bet against the 91% of the object that is still untranslated.
 *
 * The double path is why this file needs the project's x87 settings.  The two
 * squares and their sum are computed at 80-bit extended precision and rounded
 * once, on the store; compiling with -ffloat-store would round three times
 * and give a different answer.  See the same note in src/dsp/FloatIIR.cpp.
 *
 * No tier-2 peer exists for any of this -- see the note in v34det.h.
 */

#include "dsplib/v34det.h"

/*
 * The cosine table: 256 entries over one full period, Q14.
 *
 * DERIVATION, and one entry that breaks it.  255 of the 256 entries are
 * exactly `(short)(16384.0 * cos(2*pi*i/256))` -- truncated toward zero, not
 * rounded, which is what a C generator that simply assigns a double to a
 * short produces.  The exception is index 128, where cos is -1 and the
 * formula gives -16384.  The table has -16383.
 *
 * -16384 is perfectly representable, and index 0 carries +16384, so this is
 * not a range limit and not a rounding artefact.  It is a floor applied to
 * one entry, and the reason for it is not recoverable from the object.  A
 * plausible one is that negating a Q14 sample is safe only above -16384, and
 * this table is read by code that does negate it -- but that is a guess, and
 * the reconstruction does not act on it.
 *
 * The consequence for this file is small and concrete: the table is emitted
 * as data, because a generated one would be wrong by one LSB at the single
 * index where the phase accumulator lands on half a turn.
 *
 * Global in the object, and shared with V34RX.c.  Attributed to this file by
 * link order -- it sits between V34hshak.c's last static and DPSK.c's first
 * -- and by `cosread` being a one-line accessor for it and nothing else.
 */
const short costbl[256] = {
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
	 16069,  16142,  16206,  16260,  16305,  16339,  16364,  16379,
};

/*
 * Read the table with an index that cannot leave it.
 *
 * The parameter really is a byte: the original's only instruction before the
 * load is `movzbl 0x4(%esp),%eax`, so a caller passing 256 reads entry 0.
 * That is the whole function.
 *
 * NOTHING IN THE OBJECT CALLS IT.  It is global, so a caller outside
 * dsplibs.o could reach it, but the relocation table has no reference and
 * neither does any datapump.  Reproduced rather than dropped, and recorded
 * in docs/findings.md, because an exported symbol with no internal caller is
 * a fact about the original's interface, not dead weight this reconstruction
 * gets to remove.
 */
short
cosread(unsigned char idx)
{
	return costbl[idx];
}

void
dftupdate(struct v34_dftbin *bins, short nbins, const short *samples,
	  short nsamples)
{
	short i;

	/*
	 * Sample outer, bin inner.  The sample is loaded once per bin in the
	 * original rather than hoisted, which changes nothing; the loop order
	 * itself does matter, because it is what makes one call advance every
	 * bin by the same number of samples.
	 */
	for (i = 0; i < nsamples; i++) {
		struct v34_dftbin *b = bins;
		int x = samples[i];
		short j;

		for (j = 0; j < nbins; j++, b++) {
			unsigned idx;
			int re, im;

			/*
			 * The accumulator is 14 bits wide and the table has
			 * 256 entries, so the top 8 bits of the phase index
			 * it and the low 6 are the fractional part -- which
			 * is kept, but never interpolated.
			 *
			 * Read as unsigned, so a bin configured with a
			 * negative `inc` steps backwards through the table
			 * exactly as it would if it had been given the
			 * congruent positive value.
			 */
			b->phase = (short)(((unsigned)(unsigned short)b->phase
					    + (unsigned short)b->inc)
					   & V34_DFT_PHASE_MASK);
			idx = (unsigned)b->phase >> V34_DFT_PHASE_SHIFT;

			/*
			 * Quadrature by table offset: a quarter turn is 64
			 * entries, and cos(t + 90) is -sin(t), so `acc_im`
			 * accumulates the negated imaginary part.  Only the
			 * squared magnitude is ever read, so the sign never
			 * surfaces.
			 */
			re = costbl[idx] * x;
			im = costbl[(idx + V34_DFT_QUARTER) & 0xff] * x;

			/*
			 * Wrapping adds.  A long integration overflows these
			 * and the original wraps rather than saturating; the
			 * unsigned round trip is how this codebase spells
			 * that without inviting the optimiser to assume it
			 * cannot happen (see src/dsp/fpm_agc.c).
			 */
			b->acc_re = (int)((unsigned)b->acc_re
					  + (unsigned)(re >> 6));
			b->acc_im = (int)((unsigned)b->acc_im
					  + (unsigned)(im >> 6));

			/* The same products again, unshifted, at 80 bits. */
			b->sum_re += (double)re;
			b->sum_im += (double)im;
		}
	}
}

void
dftenergy(struct v34_dftbin *bins, short nbins, short scale)
{
	short i;

	for (i = 0; i < nbins; i++, bins++) {
		/*
		 * `scale` is used as a byte, and then as a shift count.
		 *
		 * The truncation to a byte is the original's own load
		 * (`movzbl`), so a caller passing 256 shifts by zero.  The
		 * mask to 5 bits after it is the `shl %cl` instruction's,
		 * which x86 applies whether the compiler asks for it or not
		 * -- so `& 31` here is not added behaviour, it is the only
		 * way to spell what the object does for a count of 32 or
		 * more without leaving the C undefined.  Same idiom as
		 * src/dsp/fpm_agc.c.
		 *
		 * All eight call sites pass a small literal -- 2, 4, 5 or 6,
		 * materialised through a register by GCC 3.4 rather than as
		 * an immediate -- so the masked range is not reached in
		 * practice.  It is handled anyway because "not reached by any
		 * caller we have translated" is a weaker claim than it looks
		 * while 90% of the object is still opaque.
		 */
		unsigned sh = (unsigned char)scale & 31;
		int re = (int)((unsigned)bins->acc_re << sh);
		int im = (int)((unsigned)bins->acc_im << sh);
		unsigned e;
		short k;

		/*
		 * One expression, so the two squares and the sum stay in x87
		 * registers and round once.  Splitting it would change the
		 * result in the last place.
		 */
		bins->denergy = bins->sum_re * bins->sum_re
			      + bins->sum_im * bins->sum_im;

		re >>= 16;
		im >>= 16;
		e = (unsigned)(re * re) + (unsigned)(im * im);
		bins->energy = (short)((int)e >> 16);

		/*
		 * Count the redundant sign bits of `e`, which is the exponent
		 * a caller needs to renormalise it.
		 *
		 * The test is that bit (31 - k) differs from bit (32 - k),
		 * spelled as an XOR of two shifted copies and a test of bit
		 * 12 -- the original's `test $0x10,%ah`.  Written the same
		 * way rather than as a leading-zero count, because the two
		 * disagree on how they treat a zero accumulator and this one
		 * falls out of the loop to the default below.
		 */
		for (k = 0; k <= 0x12; k++) {
			unsigned a = (unsigned)((int)e >> (0x13 - k));
			unsigned b = (unsigned)((int)e >> (0x14 - k));

			if ((a ^ b) & (1u << 12)) {
				bins->shift = (short)(k + 1);
				break;
			}
		}
		if (k > 0x12)
			bins->shift = 0x14;
	}
}
