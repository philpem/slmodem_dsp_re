/*
 * dcr.c -- reconstructed from dsplibs.o `dcr.c`.
 *
 *   dcr_create   .text 0x000060   89 bytes
 *   dcr_delete   .text 0x0000c0   17 bytes
 *   dcr_reset    .text 0x0000e0   25 bytes
 *   dcr_process  .text 0x000100  568 bytes
 *
 * `include/dsplib/dcr.h` carries what DCR is and how that was established.
 * This file carries the arithmetic and the two places the object's shape is
 * evidence about the source's shape.
 *
 * THE BLEND, at 0x250.  Phase 2 does not replace the estimate, it leaks
 * towards the new mean:
 *
 *     imul $0x7333, dc_level      ; 29491
 *     imul $0x0ccd, sum/count     ;  3277
 *     lea  0x4000(...)            ; 16384
 *     sar  $0xf                   ; >> 15
 *
 * 29491/32768 = 0.90000 and 3277/32768 = 0.10001, so it is a one-pole
 * smoother at alpha = 0.1 in Q15, with the +16384 rounding to nearest rather
 * than towards minus infinity.  29491 + 3277 is 32768 exactly, so the two
 * weights sum to one: the filter has unity DC gain and a constant input
 * converges to itself rather than drifting.
 *
 * WHY THE ACCUMULATOR STORES LOOK CONDITIONAL.  In phase 1 the object writes
 * `sum` and `count` back to the struct only inside the debug arm (0x2cc,
 * 0x2d4) and then immediately zeroes them (0x27b); in phase 2, which has no
 * call in it, it writes them back only on the "not finished yet" arm.  That
 * is not four different pieces of source.  It is one unconditional
 * `dcr->sum = sum; dcr->count = count;` which dead-store elimination removes
 * wherever the next thing to happen is the zeroing -- and cannot remove in
 * phase 1's debug arm, because `dsplibs_debug_printf` is an opaque call that
 * might read the struct.  The stores are written unconditionally here for
 * that reason; moving them into the branches would make the two paths differ.
 *
 * WHAT IS NOT MODELLED AS A LOCAL.  `dcr->flags` is re-read after the printf
 * (0x2f6 `movzbl (%ebx),%ecx`), which a source-level `unsigned char flags =
 * dcr->flags;` would not do -- a local would live in a callee-saved register
 * or a spill slot and would not need refilling from the struct.  So every use
 * site below reads the field.  `0x13(%esp)` is GCC's own spill of it, not a
 * variable.  This is a codegen-tier claim, held by `compare.py` and by nothing
 * in `t_dcr.c`; the note at the end of `test/mutations/dcr.json` says why no
 * differential test can hold it.  Finding F4202.
 */

#include "dsplib/dcr.h"
#include "dsplib/debug.h"
#include "dsplib/sysdep.h"

/*
 * `dcr_create` passes 0x20 to both sysdep_malloc and sysdep_memset, so the
 * size is stated by the object.  Nothing else in the tree checks a total, and
 * `diff_eq_obj` compares `sizeof(type)` bytes -- a struct short by four would
 * quietly narrow every object comparison in t_dcr while every annotated
 * offset still matched.
 */
typedef int dcr_size_is_32[sizeof(struct dcr) == 32 ? 1 : -1];

struct dcr *
dcr_create(void)
{
	struct dcr *dcr = (struct dcr *)sysdep_malloc(sizeof(struct dcr));

	if (dcr) {
		sysdep_memset(dcr, 0, sizeof(struct dcr));
		/*
		 * `orb $0x7,(%ebx)` and `movl $0x0,0x4(%ebx)` after a memset
		 * that already zeroed both: the object really does write
		 * these two explicitly.  A plain `flags = 7` would be `movb`.
		 */
		dcr->flags |= DCR_ACTIVE | DCR_SUBTRACT | DCR_TRACK;
		dcr->state = DCR_STATE_SETTLE;
		dcr->settle_samples = 5760;
		dcr->evaluate_samples = 9600;
		dcr->track_samples = 19200;
		dcr->threshold = 3000;
	}
	return dcr;
}

void
dcr_delete(struct dcr *dcr)
{
	if (dcr)
		sysdep_free(dcr);
}

void
dcr_reset(struct dcr *dcr)
{
	/*
	 * No NULL guard, unlike its two neighbours -- the object dereferences
	 * the argument straight out of the incoming slot.
	 */
	dcr->dc_level = 0;
	dcr->sum = 0;
	dcr->count = 0;
}

int
dcr_process(struct dcr *dcr, short *buf, int len)
{
	int over = 0;
	int i;
	int blocksum;
	int sum;
	int count;

	if (!dcr || !(dcr->flags & DCR_ACTIVE))
		return 0;

	/*
	 * Digital silence is not evidence about the line, so once the
	 * estimator is past its measuring phases an all-zero block is
	 * discarded whole -- including the correction, which would otherwise
	 * put `-dc_level` into a buffer that had nothing in it.
	 *
	 * The test is `(unsigned)(state - 2) <= 1` in the object; phases 0 and
	 * 1 are still filling counters and do not take it.  A zero-length
	 * block reaches `i == len` with i == 0 and returns here too.
	 */
	if (dcr->state == DCR_STATE_TRACK || dcr->state == DCR_STATE_HOLD) {
		for (i = 0; i < len; i++)
			if (buf[i] != 0)
				break;
		if (i == len)
			return 0;
	}

	switch (dcr->state) {
	case DCR_STATE_SETTLE:
		/*
		 * Count, do not measure: `sum` is never touched here, so the
		 * device's opening transient is outside the average rather
		 * than merely outweighed by it.
		 */
		count = dcr->count + len;
		dcr->count = count;
		if (count >= dcr->settle_samples) {
			dcr->state = DCR_STATE_EVALUATE;
			dcr->count = 0;
			dcr->sum = 0;
		}
		break;

	case DCR_STATE_EVALUATE:
		blocksum = 0;
		for (i = 0; i < len; i++)
			blocksum += buf[i];
		sum = dcr->sum + blocksum;
		count = dcr->count + len;
		dcr->sum = sum;
		dcr->count = count;
		if (count >= dcr->evaluate_samples) {
			/*
			 * The plain mean, truncated towards zero by `idiv`.
			 * No rounding term here, unlike the blend below.
			 */
			dcr->dc_level = (short)(sum / count);
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "DCR: initial DC Evaluation done, " "DC level %d, %sabled\n",
				    dcr->dc_level,
				    (dcr->flags & DCR_TRACK) ? "en" : "dis");
			dcr->state = (dcr->flags & DCR_TRACK)
				   ? DCR_STATE_TRACK : DCR_STATE_HOLD;
			dcr->sum = 0;
			dcr->count = 0;
		}
		break;

	case DCR_STATE_TRACK:
		blocksum = 0;
		for (i = 0; i < len; i++)
			blocksum += buf[i];
		sum = dcr->sum + blocksum;
		count = dcr->count + len;
		dcr->sum = sum;
		dcr->count = count;
		if (count >= dcr->track_samples) {
			/* 0.9 * old + 0.1 * new, Q15, rounded to nearest. */
			dcr->dc_level = (short)((dcr->dc_level * 29491
					       + (sum / count) * 3277
					       + 16384) >> 15);
			dcr->sum = 0;
			dcr->count = 0;
		}
		break;
	}

	/*
	 * The verdict and the correction, taken on every path that gets this
	 * far -- HOLD included, which is the only work it does.
	 */
	if (__builtin_abs(dcr->dc_level) >= dcr->threshold)
		over = 1;

	if (dcr->flags & DCR_SUBTRACT)
		for (i = 0; i < len; i++)
			buf[i] = (short)(buf[i] - dcr->dc_level);

	return over;
}
