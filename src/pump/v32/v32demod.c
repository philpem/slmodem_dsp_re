/*
 * v32demod.c -- ITU-T V.32 / V.32bis: the receive chain leaf.
 *
 * Reconstructed from dsplibs.o:
 *
 *   DemodDataV32   .text 0x081c00   604
 *
 * The five sub-object offsets, the two buffers and the four enables are
 * derived in `include/dsplib/v32demod.h`; this file is the flow.
 *
 * ---------------------------------------------------------------------------
 * THE DATAPUMP POINTER IS RE-READ AFTER EVERY CALL
 *
 * 81c1e, 81c3c, 81c86/81e3a, 81d00, 81d4e and 81e42 are six separate `mov
 * 0x68(%ebp),...`, one after each `FPM_*` call and one on each side of the
 * copy loop.  A local held across the calls would be cached in a register and
 * is a different instruction stream; `v32data.h` records the same idiom for
 * `ModDataV32` and it is written the same way here.
 *
 * `hdx` is re-read too -- 81cc9 and 81d79 are two loads of obj + 0x64 for two
 * tests of the same field.
 *
 * ---------------------------------------------------------------------------
 * THE TWO DIAGNOSTIC ARMS ARE IN DIFFERENT ORDERS, AND THAT IS THE AUTHOR'S
 *
 * On the no-carrier path the object prints and THEN clears the carrier bit
 * (81dcf print, 81ddb clear; and 81d67 clears on the arm that did not print).
 * On the low-energy path it clears the bit FIRST and then tests the level
 * (81e16 clear, 81e1a test).  Both orders leave the same bytes behind, so no
 * comparison of memory can separate them -- this is finding F573's shape, and
 * the transcript is the only tier that can see it.  Written as the object has
 * them rather than made uniform.
 *
 * ---------------------------------------------------------------------------
 * THE ECHO-CANCELLER GATE RETURNS BEFORE ANYTHING ELSE RUNS
 *
 * The flags are sampled BEFORE `FPM_ECC_cancel` (81c42, 81c4c) and acted on
 * AFTER the copy loop (81cc1).  So a block whose canceller is still adapting
 * is filtered, cancelled, copied and counted, and then discarded -- the
 * cleaned copy and the two lengths are updated, and nothing downstream of
 * them moves.  Sampling the flags after the call would be the same value in
 * every reachable case and a different program.
 *
 * ---------------------------------------------------------------------------
 * THE FOURTH ARGUMENT TO FPM_AGC_agc
 *
 * The object passes four (81ce6 stores the constant 1 at 0xc(%esp)) and
 * `FPM_AGC_agc` reads three -- it never touches 0x5c(%esp) in its 566 bytes.
 * `src/pump/v23/bwchdem.c` and `src/pump/v22/v22data.c` already record this
 * at their own call sites and pass three; so does this one.  The residual is
 * one instruction and is D490.
 */

#include "dsplib/v32demod.h"

#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/v32data.h"
#include "dsplib/v32hdx.h"

/* The instance is not modelled; see v32demod.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_INT(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_SHORT(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_BYTE(obj, off)	(*(unsigned char *)FIELD((obj), (off)))

#define AGC_OF(fp)	((struct fpm_agc *)(void *)FIELD((fp), V32FP_AGC))
#define ECC_OF(fp)	((struct fpm_ecc *)(void *)FIELD((fp), V32FP_ECC))
#define FSE_OF(fp)	((struct fpm_fse *)(void *)FIELD((fp), V32FP_FSE))
#define MRF_OF(fp)	((struct fpm_mrf *)(void *)FIELD((fp), V32FP_MRF))
#define SRE_OF(fp)	((struct fpm_sre *)(void *)FIELD((fp), V32FP_SRE))

#define RXBUF_OF(fp)	(*(short **)(void *)FIELD((fp), V32FP_RXBUF))
#define CLEAN_OF(fp)	(*(short **)(void *)FIELD((fp), V32FP_CLEAN))

unsigned short
DemodDataV32(void *modem, short *in, unsigned short *out, unsigned short count)
{
	int ec_training = 0;
	unsigned short n, m;
	int enables;
	void *hdx;
	void *fp;
	int i;

	fp = FIELD_PTR(modem, V32_OBJ_FP);
	n = (unsigned short)FPM_MRF_filter(MRF_OF(fp), in, RXBUF_OF(fp),
					   (short)count);

	fp = FIELD_PTR(modem, V32_OBJ_FP);
	if (ECC_OF(fp)->adapt_near != 0 || ECC_OF(fp)->adapt_far != 0)
		ec_training = 1;
	FPM_ECC_cancel(ECC_OF(fp), RXBUF_OF(fp), n);

	/* Keep a copy of the cancelled block for V32FP_GetCleanedSamples. */
	fp = FIELD_PTR(modem, V32_OBJ_FP);
	for (i = 0; i < (int)n; i++)
		CLEAN_OF(fp)[i] = RXBUF_OF(fp)[i];
	FIELD_SHORT(fp, V32FP_CLEANLEN) = (short)n;
	FIELD_SHORT(fp, V32FP_RXLEN) = (short)n;

	if (ec_training != 0)
		return 0;

	hdx = FIELD_PTR(modem, V32_OBJ_HDX);
	if (FIELD_SHORT(hdx, V32HDX_MODE) == V32_MODE_6) {
		if (FIELD_SHORT(modem, V32_OBJ_RMS_MIN)
		    > FPM_rms(RXBUF_OF(fp), n)) {
			FIELD_BYTE(modem, V32_OBJ_FLAGS) &=
				(unsigned char)~V32_FLAG_CARRIER;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("v32 low sig energy\n");
			return 0;
		}
		fp = FIELD_PTR(modem, V32_OBJ_FP);
	}

	/* The object passes a fourth argument here; see the header. */
	FPM_AGC_agc(AGC_OF(fp), RXBUF_OF(fp), n);

	fp = FIELD_PTR(modem, V32_OBJ_FP);
	FIELD_BYTE(modem, V32_OBJ_FLAGS) =
		(unsigned char)((FIELD_BYTE(modem, V32_OBJ_FLAGS)
				 & (unsigned char)~V32_FLAG_SILENCE)
				| (AGC_OF(fp)->signal == 0
				   ? V32_FLAG_SILENCE : 0));

	enables = AGC_OF(fp)->f18;
	SRE_OF(fp)->adapt = FIELD_INT(fp, V32FP_SRE_ADAPT_EN) & enables;
	m = FPM_SRE_recover(SRE_OF(fp), RXBUF_OF(fp), in, (short)n);

	fp = FIELD_PTR(modem, V32_OBJ_FP);
	if (SRE_OF(fp)->active == 0) {
		if (DSPLIB_DEBUG_VERBOSE())
			dsplibs_debug_printf("sre no carrier\n");
		FIELD_BYTE(modem, V32_OBJ_FLAGS) &=
			(unsigned char)~V32_FLAG_CARRIER;
		return 0;
	}
	FIELD_BYTE(modem, V32_OBJ_FLAGS) |= V32_FLAG_CARRIER;

	hdx = FIELD_PTR(modem, V32_OBJ_HDX);
	FSE_OF(fp)->pll_on = FIELD_INT(fp, V32FP_FSE_PLL_EN) & enables;
	if (FIELD_SHORT(hdx, V32HDX_MODE) == V32_MODE_6) {
		if (SRE_OF(fp)->mode == 0) {
			FSE_OF(fp)->tilt_on = 0;
			FSE_OF(fp)->lms_on = 0;
		} else {
			FSE_OF(fp)->tilt_on =
				FIELD_INT(fp, V32FP_FSE_TILT_EN) & enables;
			FSE_OF(fp)->lms_on =
				FIELD_INT(fp, V32FP_FSE_LMS_EN) & enables;
		}
	} else {
		FSE_OF(fp)->tilt_on = 1;
		/* NOT masked with `enables`, unlike its three siblings: D491. */
		FSE_OF(fp)->lms_on = FIELD_INT(fp, V32FP_FSE_LMS_EN);
	}

	return FPM_FSE_receive(FSE_OF(fp), in, out, m);
}
