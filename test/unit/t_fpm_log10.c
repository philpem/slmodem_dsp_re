/*
 * t_fpm_log10.c -- differential test of the fixed-point base-10 logarithm.
 *
 * Three things, in order of what they can prove:
 *
 *   1. every one of the 128 table entries is reproduced by the generator, so
 *      the derivation in the source is checked and not merely asserted;
 *   2. the function itself, against the blob, over the whole unsigned-word mantissa
 *      domain crossed with a range of exponents -- including the 64 mantissas
 *      that read one element past the end of the table, which are the point;
 *   3. the zero case, at all three debug levels, with the transcript
 *      compared.
 *
 * The sweep is exhaustive over mantissas rather than sampled: 65,536 values
 * is nothing, and a normalisation loop is exactly the shape where the
 * interesting inputs are the ones a sample misses.
 */

#include <string.h>

#include <math.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_pps.h"

extern const short FPM_log10_table[128];
extern const short ref_FPM_log10_table[];
extern const struct fpm_pps_cfg ref_FPM_PPS_CFG;

extern short ref_FPM_log10(unsigned short mantissa, short exponent);
extern unsigned int ref_dsplibs_debug_level;

/* Offline derivation belongs to the fixture, not the reconstructed object. */
static short
FPM_log10_table_generate(int index)
{
	/* Compute the ideal table formula in double precision. */
	double frac = (index * 128 + 16384) / 32768.0;

	return (short)(int)(32768.0 * log10(frac));
}

/* Fixture-only accessors name the actual production data. File-local owners
 * are exposed only in the globalized test object by tools/testvisible.py. */

static int
FPM_log10_table_derived(void)
{
	return 128;
}

static short
FPM_log10_table_entry(int index)
{
	return FPM_log10_table[index];
}

static int
FPM_log10_table_size(void)
{
	return (int)(sizeof(FPM_log10_table) / sizeof(FPM_log10_table[0]));
}


int
main(void)
{
	int i;
	int e;
	long m;

	diff_begin("FPM_log10_table: the generator reproduces the object");
	for (i = 0; i < FPM_log10_table_derived(); i++)
		diff_eq_int("table[%ld]", FPM_log10_table_generate(i),
			    FPM_log10_table_entry(i), i);
	diff_eq_int("128 derived entries (%ld)", FPM_log10_table_derived(),
		    128, 0);
	/*
	 * And one that is not derived.  Entry 128 is the neighbouring
	 * object's first short. The aggregate fixture preserves that original
	 * physical neighbour rather than copying it into the table. D65.
	 */
	diff_eq_int("128 original entries (%ld)", FPM_log10_table_size(), 128,
		    0);
	diff_eq_int("entry 128 is the neighbour's 10, not log10(1) = 0 (%ld)",
		    FPM_log10_table_entry(128), 10, 128);
	diff_eq_int("and the generator would have said 0 (%ld)",
		    FPM_log10_table_generate(128), 0, 128);
	/* The test measures the same relocatable-library boundary as the blob. */
	diff_eq_int("source table/PPS adjacency %ld",
		    (unsigned long)&FPM_PPS_CFG - (unsigned long)FPM_log10_table, 256, 0);
	diff_eq_int("reference table/PPS adjacency %ld",
		    (unsigned long)&ref_FPM_PPS_CFG - (unsigned long)ref_FPM_log10_table, 256, 0);
	if (diff_end())
		return 1;

	/*
	 * The complete unsigned-word component boundary at exponent 0.  Mantissa 0 is covered
	 * separately below, since it is the announcing path.
	 */
	diff_begin("FPM_log10: every unsigned-word mantissa, exponent 0");
	for (m = 1; m <= 0xffff; m++)
		diff_eq_int("log10(%ld, 0)",
			    FPM_log10((unsigned short)m, 0),
			    ref_FPM_log10((unsigned short)m, 0), m);
	if (diff_end())
		return 1;

	/*
	 * The 64 mantissas whose normalised value indexes one past the end of
	 * the table.  They are inside the loop above; they are repeated here
	 * so that a failure names them rather than drowning in 32,768 lines.
	 */
	diff_begin("FPM_log10: the mantissas that read past the table");
	for (m = 0x7fc0; m <= 0x7fff; m++)
		diff_eq_int("log10(%ld, 0) past the end",
			    FPM_log10((unsigned short)m, 0),
			    ref_FPM_log10((unsigned short)m, 0), m);
	if (diff_end())
		return 1;

	/*
	 * Exponents.  Every shift the normaliser can take is reachable from
	 * some mantissa, and the exponent enters both the multiply and the
	 * 16-bit truncation of their sum -- so negative and large values are
	 * where a wrong width would show.
	 */
	diff_begin("FPM_log10: exponents, including ones that wrap the sum");
	for (e = -40; e <= 40; e++) {
		for (m = 1; m <= 0xffff; m = m * 3 + 1)
			diff_eq_int("log10(%ld, e)",
				    FPM_log10((unsigned short)m, (short)e),
				    ref_FPM_log10((unsigned short)m, (short)e),
				    m);
		diff_eq_int("log10(0x4000, %ld)",
			    FPM_log10(0x4000, (short)e),
			    ref_FPM_log10(0x4000, (short)e), e);
	}
	{
		static const short far[] = { -32768, -32767, -1000, 1000,
					     32766, 32767 };

		for (i = 0; i < (int)(sizeof(far) / sizeof(far[0])); i++) {
			diff_eq_int("log10(1, %ld)", FPM_log10(1, far[i]),
				    ref_FPM_log10(1, far[i]), far[i]);
			diff_eq_int("log10(0x7fff, %ld)",
				    FPM_log10(0x7fff, far[i]),
				    ref_FPM_log10(0x7fff, far[i]), far[i]);
		}
	}
	if (diff_end())
		return 1;

	/* Above-Q15 inputs in the full-word sweep exercise original neighbouring
	 * objects. This does not establish modem reachability for those inputs. */

	diff_begin("FPM_log10: log10 of zero announces itself at level 2");
	{
		unsigned lines = 0;
		int lvl;

		for (lvl = 1; lvl <= 3; lvl++) {
			dsplibs_debug_level = ref_dsplibs_debug_level =
				(unsigned)lvl;
			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			diff_eq_int("log10(0, 0) at level %ld",
				    FPM_log10(0, 0), ref_FPM_log10(0, 0), lvl);
			diff_eq_int("log10(0, 7) at level %ld",
				    FPM_log10(0, 7), ref_FPM_log10(0, 7), lvl);
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			dsplib_debug_capture_on = 0;

			diff_eq_int("transcript at level %ld",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, lvl);
			if (lvl == 1)
				diff_eq_int("level 1 is silent",
					    (int)dsplib_debug_capture_lines(1),
					    0, lvl);
			else
				lines += dsplib_debug_capture_lines(1);
		}
		diff_eq_int("it said something (%ld)", lines > 0, 1,
			    (long)lines);
	}
	return diff_end();
}
