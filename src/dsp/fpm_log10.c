/*
 * fpm_log10.c -- Fixed Point Modem: base-10 logarithm.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_log10        .text   0x0a8d20   203
 *   FPM_log10_table  .rodata 0x00c3a0   256   (file-local; 128 shorts)
 *
 * Reached by the V.32 datapump and, through it, by anything that wants a
 * level in decibels.  Fixed point throughout, so the equivalence criterion
 * is bit-exact with zero tolerance.
 *
 * Method
 * ------
 * The argument is a mantissa and a base-2 exponent, so
 *
 *     log10(m * 2^-e) = log10(m) - e * log10(2)
 *
 * and the two terms are computed separately.  The mantissa is normalised by
 * shifting left until it leaves [0, 0x3fff] -- that is, into [0x4000,
 * 0xffff] -- and each shift is added to the exponent.  The normalised value
 * then indexes a 128-entry table of log10, and the exponent term is a single
 * multiply.
 *
 *     idx = ((norm + 64) >> 7) - 128
 *     return (table[idx] >> 3) - e * 1228
 *
 * The table is Q15 and the result is Q12, which is what the `>> 3` is doing.
 *
 * Table derivation
 * ----------------
 * Entry i corresponds to a normalised mantissa of i * 128 + 16384, i.e. a Q15
 * fraction of (i * 128 + 16384) / 32768, and the entry is that fraction's
 * log10 in Q15, truncated TOWARD ZERO:
 *
 *     table[i] = (int)(32768.0 * log10((i * 128 + 16384) / 32768.0))
 *
 * `FPM_log10_table_generate()` implements exactly that and the unit test
 * checks it reproduces all 128 original entries.  Truncation is the rule and
 * it is not a near miss: rounding disagrees on 66 of the 128 entries and
 * flooring on all 128.  The generator is the maintainable artefact; the
 * extracted bytes are only the reference.
 *
 * The exponent coefficient, and why it is not log10(2)
 * ----------------------------------------------------
 * In Q12, log10(2) is 0.30103 * 4096 = 1233.2, so the coefficient should be
 * 1233.  The object multiplies by **1228** (`imul $0x4cc`), which is
 * 0.29980 -- 0.4% low, and it is the only constant here that does not follow
 * from the table.  The error is proportional to the exponent, so it grows
 * with how far the input had to be normalised: 5 in Q12 per octave, about
 * 0.0012 of a decade, or 0.024 dB per octave if the result is read as a
 * power ratio.  Recorded as D304 in docs/deviations.md, unmeasured;
 * reproduced exactly.
 *
 * The one-past-the-end read
 * -------------------------
 * For a Q15 caller -- mantissa 0x0000..0x7fff, which is what `FPM_sqrt`'s
 * domain analysis established for this layer -- the normalised value lands in
 * [0x4000, 0x7fff] and the index runs 0..128.  The table has 128 entries,
 * 0..127.  So mantissas of 0x7fc0..0x7fff, 64 of the 32768 Q15 values, read
 * one element past the end.
 *
 * `FPM_sqrt` has the same defect and gets away with it: what follows ITS
 * table is a 32768 that happens to be the mathematically correct entry.  This
 * one is not so lucky.  What follows this table at .rodata:0xc4a0 is
 * `FPM_PPS_CFG`, whose first short is **10**, where the correct value is
 * log10(1.0) = **0**.  10 in Q15 shifted down by 3 is 1 in Q12, so the answer
 * is one count high over that range rather than catastrophically wrong --
 * which is presumably why it was never noticed.
 *
 * SO THE TABLE HERE HAS 129 ENTRIES AND THE LAST ONE IS 10.  That is not a
 * derived value and the generator does not produce it: it is the neighbouring
 * object's first short, written down so that our copy reproduces the original
 * DELIBERATELY rather than by luck.  Without it our overrun reads whatever
 * our own linker put after the array -- which, when this file was first
 * written, happened to be a value in 8..15 and so happened to agree after the
 * `>> 3`.  A test that passes for that reason is a test that fails the next
 * time anything is added to this translation unit.  `FPM_sqrt` sets the
 * precedent by adding a 193rd entry; the difference is that its extra entry
 * is the mathematically correct one and this one is a transcription of the
 * defect.
 *
 * Recorded as D303 in docs/deviations.md; the test drives all 64 of those
 * mantissas explicitly.
 *
 * ABOVE Q15 THERE IS NO REPRODUCTION AND NONE IS CLAIMED.  A mantissa of
 * 0x8000..0xffff needs no normalising and indexes up to 384, so the original
 * reads up to 512 bytes past its table -- through `FPM_PPS_CFG` and whatever
 * follows it.  Reproducing that would mean transcribing a quarter of
 * .rodata, and nothing establishes that any caller does it.  The unit test
 * deliberately does NOT compare that range: two builds disagreeing about
 * memory neither of them owns is not a defect either of them has.
 */

#include <math.h>

#include "dsplib/debug.h"
#include "dsplib/fpm.h"

/*
 * Extracted from the original object; regenerated and checked by the test.
 * File-local in the blob -- the reference to it is section-relative with the
 * offset as an addend, not a named symbol -- so it is static here.
 */
static const short FPM_log10_table[129] = {
	-9864, -9753, -9643, -9534, -9426, -9318, -9212, -9106,
	-9001, -8897, -8793, -8690, -8588, -8487, -8387, -8287,
	-8187, -8089, -7991, -7894, -7798, -7702, -7607, -7512,
	-7418, -7325, -7232, -7140, -7048, -6957, -6867, -6777,
	-6688, -6599, -6511, -6424, -6337, -6250, -6164, -6079,
	-5994, -5909, -5825, -5742, -5659, -5576, -5494, -5413,
	-5332, -5251, -5171, -5091, -5012, -4933, -4855, -4777,
	-4699, -4622, -4545, -4469, -4393, -4318, -4243, -4168,
	-4093, -4020, -3946, -3873, -3800, -3728, -3656, -3584,
	-3513, -3442, -3371, -3301, -3231, -3161, -3092, -3023,
	-2954, -2886, -2818, -2751, -2683, -2616, -2550, -2483,
	-2417, -2352, -2286, -2221, -2156, -2092, -2027, -1963,
	-1900, -1836, -1773, -1710, -1648, -1586, -1524, -1462,
	-1400, -1339, -1278, -1218, -1157, -1097, -1037, -977,
	-918, -859, -800, -741, -683, -625, -567, -509,
	-451, -394, -337, -280, -224, -167, -111, -55,
	/*
	 * Entry 128 is NOT log10 of anything.  The original's table stops at
	 * 127 and its index reaches 128 for mantissas 0x7fc0..0x7fff; what it
	 * reads there is the first short of `FPM_PPS_CFG` at .rodata:0xc4a0.
	 * Transcribed so the overrun is reproduced deterministically.  D303.
	 */
	10
};

short
FPM_log10(unsigned short mantissa, short exponent)
{
	unsigned short norm;
	unsigned short shifts;
	unsigned short e;
	int idx;

	if (mantissa == 0) {
		/*
		 * "Fatal" in the message only: it returns 0 and the caller
		 * carries on.  Gated at level 2, like every diagnostic here.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Fatal error: log10 of zero!\n");
		return 0;
	}

	/*
	 * Normalise into [0x4000, 0xffff], counting the shifts.  The test is
	 * `<= 0x3fff` on the 16-bit value, so the loop is skipped entirely
	 * for a mantissa that already has its top two bits clear of it.
	 */
	norm = mantissa;
	shifts = 0;
	if (norm <= 0x3fff) {
		do {
			norm = (unsigned short)(norm << 1);
			shifts++;
		} while (norm <= 0x3fff);
	}

	/*
	 * The sum is kept to 16 bits -- the object computes it in a register
	 * and reads back only `%si` -- so an exponent large enough to wrap
	 * wraps here too.
	 */
	e = (unsigned short)(exponent + (short)shifts);

	idx = (int)(unsigned short)((((int)norm + 0x40) >> 7) - 128);

	return (short)((FPM_log10_table[idx] >> 3) - (int)e * 1228);
}

/*
 * Regenerate one table entry from the design parameters.  Kept alongside the
 * table so the derivation stays checkable: the unit test regenerates all 128
 * and compares.
 */
/*
 * Entry 128 is excluded by construction: it is the neighbour's byte, not a
 * logarithm, so there is nothing for a generator to produce.  The unit test
 * checks 0..127 against this and asserts 128 separately.
 */
int
FPM_log10_table_derived(void)
{
	return 128;
}

short
FPM_log10_table_generate(int index)
{
	/* Deliberately double precision: the original was generated offline. */
	double frac = (index * 128 + 16384) / 32768.0;

	return (short)(int)(32768.0 * log10(frac));
}

short
FPM_log10_table_entry(int index)
{
	return FPM_log10_table[index];
}

int
FPM_log10_table_size(void)
{
	return (int)(sizeof(FPM_log10_table) / sizeof(FPM_log10_table[0]));
}
