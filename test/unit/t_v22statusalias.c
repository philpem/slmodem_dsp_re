/* Synthetic component overlap probe: real constructor graphs, caller report
 * aliases aligned scalar DSP storage. No public modem reachability claim.
 */
#include <string.h>
#include "harness.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22status.h"

extern int ref_V22_status(struct v22fp *, struct v22_status *);

struct v22_alias_prefix {
	unsigned char bytes[0x2c];
};
typedef char report_alias_is_in_scalar_prefix[
	16 + sizeof(struct v22_status)
	<= __builtin_offsetof(struct v22fp_dsp, pps) ? 1 : -1];

int
main(void)
{
	static const unsigned offsets[] = { 0, 4, 8, 12, 16 };
	struct v22fp_cfg cfg = { 0, 0, 60000, 0, 700, 0, 1 };
	unsigned i;

	diff_begin("V22 status synthetic aligned report overlap scalar DSP prefix");
	for (i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
		struct v22fp *a = V22FP_create(0, &cfg);
		struct v22fp *b = V22FP_create(0, &cfg);
		struct v22_status *sa, *sb;
		int ra, rb;

		diff_eq_int("both constructor graphs (%ld)", a != 0 && b != 0, 1, i);
		if (!a || !b)
			continue;
		a->dsp->r00 = b->dsp->r00 = 0;
		a->dsp->r0c = b->dsp->r0c = 0x100;
		a->dsp->eq_adapt = b->dsp->eq_adapt = 1;
		a->dsp->scrambler_on = b->dsp->scrambler_on = 0x102;
		a->dsp->descrambler_on = b->dsp->descrambler_on = 0x103;
		a->dsp->r20 = b->dsp->r20 = 0x104;
		sa = (struct v22_status *)((unsigned char *)a->dsp + offsets[i]);
		sb = (struct v22_status *)((unsigned char *)b->dsp + offsets[i]);
		rb = ref_V22_status(b, sb);
		ra = V22_status(a, sa);
		diff_eq_int("status return (%ld)", ra, rb, i);
		diff_eq_obj("aliased scalar DSP prefix", struct v22_alias_prefix,
			    a->dsp, b->dsp, i);
		diff_eq_obj("aliased report", struct v22_status, sa, sb, i);
		V22FP_delete(a);
		V22FP_delete(b);
	}
	return diff_end();
}
