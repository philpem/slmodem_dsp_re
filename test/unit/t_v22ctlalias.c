/* Synthetic component fidelity probe: constructor-created graphs with an
 * explicit union overlay for request bytes and scalar DSP storage. This is
 * not evidence that public modem histories produce overlapping requests.
 */
#include <string.h>
#include "harness.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22ctl.h"

extern int ref_V22FP_control(struct v22fp *, const struct v22fp_ctl *);

union v22_control_overlap {
	struct v22fp_dsp dsp;
	struct {
		unsigned char prefix[0x0b];
		struct v22fp_ctl ctl;
	} request;
};

union v22_hdx_control_overlap {
	struct v22fp_hdx hdx;
	struct v22fp_ctl ctl;
};

struct v22_control_scalar_prefix {
	unsigned char bytes[0x30];
};

struct v22_control_hdx_prefix {
	unsigned char bytes[0x14];
};

typedef char v22_control_high_byte_overlaps_scrambler[
 __builtin_offsetof(union v22_control_overlap, request.ctl.flags_0d)
 == __builtin_offsetof(struct v22fp_dsp, scrambler_on) ? 1 : -1];

int
main(void)
{
	static const unsigned char high[] = { 0, 4, 0x80, 0x84 };
	struct v22fp_cfg cfg = { 0, 0, 60000, 0, 700, 0, 1 };
	unsigned i, low;
	long tag = 0;

	diff_begin("V22 control request/DSP overlap (synthetic component)");
	for (i = 0; i < sizeof(high) / sizeof(high[0]); i++) {
		for (low = 0; low < 256; low++, tag++) {
			struct v22fp *a = V22FP_create(0, &cfg);
			struct v22fp *b = V22FP_create(0, &cfg);
			struct v22fp_dsp *saved_a, *saved_b;
			union v22_control_overlap oa, ob;
			int ra, rb;

			diff_eq_int("constructor graphs (%ld)", a != 0 && b != 0, 1, tag);
			if (!a || !b) {
				if (a) V22FP_delete(a);
				if (b) V22FP_delete(b);
				continue;
			}
			saved_a = a->dsp;
			saved_b = b->dsp;
			oa.dsp = *saved_a;
			ob.dsp = *saved_b;
			/* All bytes in this prefix are scalar storage, before the SDM. */
			memset(&oa, 0, sizeof(struct v22_control_scalar_prefix));
			memset(&ob, 0, sizeof(struct v22_control_scalar_prefix));
			oa.request.ctl.flags_0c = ob.request.ctl.flags_0c = low;
			oa.request.ctl.flags_0d = ob.request.ctl.flags_0d = high[i];
			a->dsp = &oa.dsp;
			b->dsp = &ob.dsp;

			rb = ref_V22FP_control(b, &ob.request.ctl);
			ra = V22FP_control(a, &oa.request.ctl);
			diff_eq_int("control result (%ld)", ra, rb, tag);
			diff_eq_obj("scalar DSP prefix", struct v22_control_scalar_prefix,
				    &oa, &ob, tag);
			diff_eq_int("adaptation (%ld)", oa.dsp.agc.f18, ob.dsp.agc.f18, tag);
			diff_eq_int("protocol (%ld)", a->hdx->protocol, b->hdx->protocol, tag);
			diff_eq_int("substate (%ld)", a->hdx->connect_substate,
				    b->hdx->connect_substate, tag);
			diff_eq_int("parameter flags (%ld)", a->params.flags, b->params.flags, tag);

			/* Restore heap ownership before either destructor walks the graph. */
			a->dsp = saved_a;
			b->dsp = saved_b;
			V22FP_delete(a);
			V22FP_delete(b);
		}
	}
	diff_eq_int("selected overlap cases", tag, 1024, 0);

	/* The high request byte overlaps the substate's high byte. RETRAIN clears
	 * it before the second decision; retaining the old request would select
	 * remote loopback incorrectly for 0x84. This remains a synthetic component
	 * representation probe, without a claimed modem-state history.
	 */
	for (i = 0; i < sizeof(high) / sizeof(high[0]); i++) {
		for (low = 0; low < 2; low++, tag++) {
			struct v22fp *a = V22FP_create(0, &cfg);
			struct v22fp *b = V22FP_create(0, &cfg);
			struct v22fp_hdx *saved_a, *saved_b;
			union v22_hdx_control_overlap oa, ob;
			int ra, rb;

			diff_eq_int("HDX constructor graphs (%ld)", a != 0 && b != 0, 1, tag);
			if (!a || !b) {
				if (a) V22FP_delete(a);
				if (b) V22FP_delete(b);
				continue;
			}
			saved_a = a->hdx;
			saved_b = b->hdx;
			oa.hdx = *saved_a;
			ob.hdx = *saved_b;
			memset(a->dsp, 0, sizeof(struct v22_control_scalar_prefix));
			memset(b->dsp, 0, sizeof(struct v22_control_scalar_prefix));
			oa.ctl.flags_0c = ob.ctl.flags_0c = low;
			oa.ctl.flags_0d = ob.ctl.flags_0d = high[i];
			a->hdx = &oa.hdx;
			b->hdx = &ob.hdx;
			rb = ref_V22FP_control(b, &ob.ctl);
			ra = V22FP_control(a, &oa.ctl);
			diff_eq_int("HDX control result (%ld)", ra, rb, tag);
			diff_eq_obj("HDX scalar prefix", struct v22_control_hdx_prefix,
				    &oa, &ob, tag);
			diff_eq_obj("DSP scalar prefix after HDX request", struct v22_control_scalar_prefix,
				    a->dsp, b->dsp, tag);
			diff_eq_int("HDX request adaptation (%ld)", a->dsp->agc.f18, b->dsp->agc.f18, tag);
			diff_eq_int("HDX request parameter flags (%ld)", a->params.flags, b->params.flags, tag);
			a->hdx = saved_a;
			b->hdx = saved_b;
			V22FP_delete(a);
			V22FP_delete(b);
		}
	}
	diff_eq_int("total overlap cases", tag, 1032, 0);
	return diff_end();
}
