/*
 * t_v90p4dnan.cpp -- ONE check, and it is in its own binary on purpose.
 *
 * `V90Phase4Demodulator`'s `CalcErrorEnergyAfterEchoCancellation` tail stores
 * one bit into +0x3510:
 *
 *     26956:  d9 44 24 10      flds  0x10(%esp)       ; dB
 *     2696b:  d9 83 04 04 ..   flds  0x404(%ebx)      ; the parameter
 *     26971:  de d9            fcompp
 *     26976:  0f 92 c2         setb  %dl
 *
 * -- ONE ordered compare and no parity test, so the flag is `PARAM < dB` and
 * an UNORDERED result leaves CF set and stores 1.  `dB` is unordered exactly
 * when the two silence energies have opposite signs, because then
 * `1.0f / after * before` is negative and `fyl2x` answers a NaN.
 *
 * WHY IT IS NOT IN `t_v90p4ddec`, and this is the whole reason the file
 * exists.  IEEE C requires a relational compare against a NaN to be false and
 * GCC 13 emits the parity test whatever it is told (finding 2304), so the
 * modern build stores 0 here and `tools/gccdiverge.json` has to excuse the
 * check.  An excused binary EXITS NON-ZERO, and `tools/mutate.py` judges a
 * mutant caught by a non-zero exit -- so a binary that is already red cannot
 * score a mutation set at all, and it refuses rather than scoring one wrongly.
 * Splitting the divergent check into its own binary is what that refusal asks
 * for, and it leaves `t_v90p4ddec`'s 66,000 checks green under both compilers
 * and mutation-testable.
 *
 * `make period` has no allow-list and passes this file with the object's own
 * compiler, which is the tier that decides.  Findings 4712 and 2301.
 *
 * THE FIXTURE IS TINY BECAUSE THE ARM IS.  State
 * `CALC_ENERGY_AFTER_EC` reads `params`, calls
 * `V90Demapper::incrementRBSFramePosition` and touches nothing else, so one
 * demodulator per side, one shared parameter block and one shared demapper is
 * the whole of it.  The demapper is shared because the only thing that member
 * writes is its own cursor, which both sides then agree about by construction.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Demapper.h"
#include "dsplib/V90Phase4Demodulator.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

short ref_p4d_getv90decision(void *, short)
	asm("ref__ZN20V90Phase4Demodulator14getV90DecisionEs");
short ref_p4d_getv92decision(void *, short)
	asm("ref__ZN20V90Phase4Demodulator14getV92DecisionEs");
short our_p4d_getv90decision(void *, short)
	asm("_ZN20V90Phase4Demodulator14getV90DecisionEs");
short our_p4d_getv92decision(void *, short)
	asm("_ZN20V90Phase4Demodulator14getV92DecisionEs");
}

#define P4D_SLOT	((unsigned)sizeof(V90Phase4Demodulator) + 64u)

static unsigned char p4d_s[2][P4D_SLOT] __attribute__((aligned(8)));
static unsigned char dem_s[sizeof(V90Demapper) + 64] __attribute__((aligned(8)));
static unsigned char parm_s[sizeof(V90Parameters) + 64]
	__attribute__((aligned(8)));

#define P4D(s)	(*(V90Phase4Demodulator *)p4d_s[s])
#define DEM	((V90Demapper *)dem_s)
#define PARAMS	((V90Parameters *)parm_s)

/* Varied, never zero: findings 223, 224, 230. */
static void
fill(unsigned char *p, unsigned n, unsigned lfsr)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		p[i] = (unsigned char)((lfsr >> 3) | 1u);
	}
}

int
main(void)
{
	long trial = 800000L;
	int v92, s;
	int sawnan = 0;

	diff_begin("V90Phase4Demodulator: the keep-rate flag on an unordered "
		   "dB");
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	for (v92 = 0; v92 < 2; v92++) {
		fill(parm_s, (unsigned)sizeof parm_s, 0x51edu + (unsigned)v92);
		fill(dem_s, (unsigned)sizeof dem_s, 0x9e37u + (unsigned)v92);
		fill(p4d_s[0], P4D_SLOT, 0x37c1u + (unsigned)v92);
		memcpy(p4d_s[1], p4d_s[0], P4D_SLOT);

		PARAMS->RRN_SILENCE_ECHO_CALC_PERIOD = 0x24;
		PARAMS->RRN_SILENCE_MIN_ECHO_ENERGY_FOR_KEEP_RATE = -80.0f;
		DEM->rbsFramePosition = 2u;

		for (s = 0; s < 2; s++) {
			P4D(s).params = PARAMS;
			P4D(s).demapper = DEM;
			P4D(s).state = P4D_STATE_CALC_ENERGY_AFTER_EC;
			P4D(s).sessionFlag = v92 ? 1u : 0u;
			P4D(s).countInState = 41u;   /* 42, and 42 % 6 == 0 */
			P4D(s).int_0028 = 0;
			/*
			 * OPPOSITE SIGNS, which is the whole input: the
			 * accumulator goes positive on the first squared
			 * decision and the numerator stays negative, so the
			 * ratio is negative and the logarithm is a NaN.
			 */
			P4D(s).errorEnergyBeforeEC = -4.0f;
			P4D(s).errorEnergyAfterEC = 1.0f;
			P4D(s).int_3510 = 0;
		}

		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		if (v92) {
			(void)our_p4d_getv92decision(p4d_s[0], 300);
			(void)ref_p4d_getv92decision(p4d_s[1], 300);
		} else {
			(void)our_p4d_getv90decision(p4d_s[0], 300);
			(void)ref_p4d_getv90decision(p4d_s[1], 300);
		}
		dsplib_debug_capture_on = 0;

		/*
		 * ANTI-VACUITY, and it is an OBSERVABLE rather than a path:
		 * the blob's flag is 1, which the ordered reading of the same
		 * two numbers cannot produce, because the parameter is -80 and
		 * the dB is not above it -- it is not a number at all.  If a
		 * future change made the ratio positive this check would go
		 * green for the wrong reason, and this counter is what fails
		 * instead.
		 */
		if (P4D(1).int_3510 == 1)
			sawnan++;
		diff_eq_int("the keep-rate flag (%ld)", (long)P4D(0).int_3510,
			    (long)P4D(1).int_3510, trial);
		diff_eq_int("the transcripts agreed (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    trial);
		diff_eq_int("the progress code (%ld)", (long)P4D(0).int_0028,
			    (long)P4D(1).int_0028, trial);
		diff_eq_int("the state (%ld)", (long)P4D(0).state,
			    (long)P4D(1).state, trial);
		trial++;
	}

	diff_eq_int("the blob took the unordered path", sawnan, 2, 0);
	return diff_end();
}
