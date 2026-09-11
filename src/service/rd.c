/*
 * rd.c -- the slmodemd-facing ring-detector wrapper (blob span `rd.c`,
 * .text 0x002130-0x002350, four symbols in the object's own order:
 * RD_create, RD_delete, RD_process, RD_ring_details).
 *
 * This is a TU of its own in the blob -- the FILE symbol `rd.c` sits between
 * `fax.c` and `ringDetector.c` in the recovered input order -- and keeping it
 * separate is what makes the four wrappers reach the ringDetector.c entry
 * points by CALL.  With the two TUs merged into voice.c, O3 inlined
 * `RingDetector_Delete` into RD_delete, `RingDetector_Process` into
 * RD_process and `RingDetector_GetLastRing` into both RD_process and
 * RD_ring_details, so those three symbols could not be byte-identical.
 */

#include "dsplib/ringdet.h"
#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
#include "dsplib/modem_params.h"

/*
 * The two sample rates the detector is written for.  `RD_create` refuses
 * anything else before it allocates.
 */
#define RD_RATE_8000	8000
#define RD_RATE_9600	9600

/*
 * The comparator threshold, chosen from `MDMPRM_CODECTYPE`.  The object
 * names no codec constant anywhere -- the mangling of the V.90 code records
 * only that `__tHardwareCodecTypes__` exists, not its enumerators (see
 * V90CodecType.h) -- so these stay as the numbers the switch tests.
 *
 * The default is NEGATIVE, and that is not a sentinel: Reset takes the
 * threshold's absolute value everywhere and uses its SIGN to pick the second
 * set of debounce constants (0/200 rather than 2/100).
 */
#define RD_THRESHOLD_DEFAULT	(-3000)

void *
RD_create(void *modem, unsigned int rate)
{
	struct rd *rd;
	struct ring_detector_cfg cfg;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("RD: create...\n");

	if (rate != RD_RATE_8000 && rate != RD_RATE_9600)
		return 0;

	rd = sysdep_malloc(sizeof *rd);
	if (!rd)
		return 0;
	sysdep_memset(rd, 0, sizeof *rd);
	rd->modem = modem;

	cfg.fs = (int)rate;
	cfg.min_freq = 15;
	cfg.max_freq = 80;
	cfg.min_on_dur = 120;
	cfg.min_off_dur = 120;

	switch ((int)modem_get_param(modem, MDMPRM_CODECTYPE)) {
	case 4:
	case 12:
		cfg.threshold = 1000;
		break;
	case 13:
	case 15:
		cfg.threshold = 650;
		break;
	case 14:
		cfg.threshold = 850;
		break;
	default:
		cfg.threshold = RD_THRESHOLD_DEFAULT;
		break;
	}

	rd->det = RingDetector_Create(&cfg);
	if (!rd->det) {
		sysdep_free(rd);
		return 0;
	}
	return rd;
}

void
RD_delete(void *obj)
{
	struct rd *rd = (struct rd *)obj;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("RD: delete...\n");
	RingDetector_Delete(rd->det);
	sysdep_free(rd);
}

int
RD_process(void *obj, void *in, int count)
{
	struct rd *rd = (struct rd *)obj;
	int ret;

	ret = RingDetector_Process(rd->det, (short *)in, (unsigned int)count);
	if (ret) {
		int freq, duration;

		RingDetector_GetLastRing(rd->det, &freq, &duration);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("RD: RD: freq = %d, duration = %d\n",
					     freq, duration);
	}
	return ret;
}

/*
 * slmodemd declares the two out-parameters `long *`; the object stores 32-bit
 * words through them, which is the same thing on the ILP32 target it was
 * built for and not the same thing anywhere else.  `int *` is what the
 * instructions say, so `int *` is what is written -- the same call this tree
 * already made for `dsp_info::clock_deviation`.
 */
void
RD_ring_details(void *obj, int *freq, int *duration)
{
	struct rd *rd = (struct rd *)obj;

	RingDetector_GetLastRing(rd->det, freq, duration);
}

