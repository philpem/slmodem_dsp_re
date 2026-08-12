/*
 * t_fpm_sre.c -- the V.32 symbol-timing recovery tables against the blob's.
 *
 * A byte comparison proves the CONTENTS and says nothing about the SHAPE:
 * short[6] and int[3] compare identically.  Only SREv32_COFFS has its width
 * measured, by the `movzwl (%ecx,%edx,2)` FPM_SRE_init copies it with; the
 * other five are declared 16-bit on the strength of their contents and are
 * marked unverified in the header until FPM_SRE_recover is read.  So this
 * file pins the bytes and finding 1615 records what it does not pin.
 */

#include "harness.h"
#include "dsplib/fpm_sre.h"

extern const short ref_SREv32_COFFS[];
extern short ref_SREv32_XB_COFFS[];
extern short ref_SREv32_PLL_K1[];
extern short ref_SREv32_PLL_K2[];
extern short ref_SREv32_xCLOCK[];
extern short ref_SREv32_yCLOCK[];

static void
cmp(const char *tag, const short *got, const short *want, int n)
{
	int i;

	for (i = 0; i < n; i++)
		diff_eq_int(tag, got[i], want[i], i);
}

int
main(void)
{
	diff_begin("SREv32 tables");

	cmp("SREv32_COFFS[%ld]", SREv32_COFFS, ref_SREv32_COFFS, 181);
	cmp("SREv32_XB_COFFS[%ld]", SREv32_XB_COFFS, ref_SREv32_XB_COFFS, 11);
	cmp("SREv32_PLL_K1[%ld]", SREv32_PLL_K1, ref_SREv32_PLL_K1, 3);
	cmp("SREv32_PLL_K2[%ld]", SREv32_PLL_K2, ref_SREv32_PLL_K2, 3);
	cmp("SREv32_xCLOCK[%ld]", SREv32_xCLOCK, ref_SREv32_xCLOCK, 3);
	cmp("SREv32_yCLOCK[%ld]", SREv32_yCLOCK, ref_SREv32_yCLOCK, 3);

	/*
	 * The three-phase clock, spelled out: cos and sin of 0, 120 and 240
	 * degrees at a scale of 16384.  If this ever stops holding, the table
	 * is not what this file says it is.
	 */
	diff_eq_int("xCLOCK is cos(0) (%ld)", SREv32_xCLOCK[0], 16384, 0);
	diff_eq_int("xCLOCK[1] == xCLOCK[2] (%ld)",
		    SREv32_xCLOCK[1] == SREv32_xCLOCK[2], 1, 0);
	diff_eq_int("yCLOCK is sin(0) (%ld)", SREv32_yCLOCK[0], 0, 0);
	diff_eq_int("yCLOCK[1] == -yCLOCK[2] (%ld)",
		    SREv32_yCLOCK[1] == -SREv32_yCLOCK[2], 1, 0);

	return diff_end();
}
