/*
 * t_class1status.c -- differential test of `fax_class1_status` (class1.c,
 * `.text` 0x0093b50, 150 bytes).
 *
 * Exercises every `ctx->state` value 0..14 (the in-range 4..6 and 12..13
 * plus enough neighbours to prove the boundaries), with `modem_status` both
 * NULL and non-NULL.  A non-NULL `modem_status` is only ever tried against
 * a NULL-dispatch slot (`vmi->slot == 0`) -- the same "already proven safe"
 * technique `t_faxvmids.c` uses for `FAXVMI_status` itself, since the real
 * per-modulation `vxx_status` entries dereference an instance this wave does
 * not build.  `vmi_a`/`vmi_b` are stack objects: `fax_class1_status` neither
 * allocates nor frees anything itself, so the allocation-log technique
 * `t_class1delete.c` needs is not needed here.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/faxvmi.h"

extern int ref_fax_class1_status(struct fax_class1 *ctx, void *modem_status);
extern const unsigned char ref_FAXVMI_STS[28];

static unsigned long seed = 20260903UL;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

static void
fill(void *p, unsigned n)
{
	unsigned char *b = (unsigned char *)p;
	unsigned i;

	for (i = 0; i < n; i++)
		b[i] = (unsigned char)(rnd() & 0xff);
}

/*
 * `FAXVMI_STS` is checked once, on its own: the object's own template is
 * 28 bytes, all zero (F10058).
 */
static int
test_template(void)
{
	static const unsigned char zero[28];

	diff_begin("FAXVMI_STS");
	diff_eq_int("all-zero, ours", memcmp(&FAXVMI_STS, zero, 28), 0, 0);
	diff_eq_int("all-zero, blob", memcmp(ref_FAXVMI_STS, zero, 28), 0, 0);
	return diff_end();
}

/*
 * Two DISTINCT role objects, `A` (for `ctx->vmi_a`) and `B` (for
 * `ctx->vmi_b`), each mirrored onto an "ours" and a "blob" copy with
 * IDENTICAL starting content -- so whichever one `fax_class1_status`
 * actually reads from is provable by which pair changes, and the other
 * pair staying byte-for-byte equal to its own untouched original proves
 * the field NOT selected was not read at all.
 */
static void
run_case(int state, int use_modem_status, long tag)
{
	struct fax_class1 ctx_a, ctx_b;
	struct faxvmi vmiA_ours, vmiA_blob, vmiB_ours, vmiB_blob;
	struct faxvmi_framer frA_orig, frA_ours, frA_blob;
	struct faxvmi_framer frB_orig, frB_ours, frB_blob;
	struct faxvmi_link lkA, lkB;
	unsigned char modembuf[8];
	int ra, rb;

	fill(&frA_orig, sizeof(frA_orig));
	fill(&frB_orig, sizeof(frB_orig));
	fill(&lkA, sizeof(lkA));
	fill(&lkB, sizeof(lkB));
	fill(modembuf, sizeof(modembuf));
	frA_ours = frA_blob = frA_orig;
	frB_ours = frB_blob = frB_orig;

	memset(&vmiA_ours, 0, sizeof(vmiA_ours));
	vmiA_ours.slot = 0;		/* the null slot, proven safe */
	vmiA_ours.framer = &frA_ours;
	vmiA_ours.link = &lkA;
	vmiA_blob = vmiA_ours;
	vmiA_blob.framer = &frA_blob;

	memset(&vmiB_ours, 0, sizeof(vmiB_ours));
	vmiB_ours.slot = 0;
	vmiB_ours.framer = &frB_ours;
	vmiB_ours.link = &lkB;
	vmiB_blob = vmiB_ours;
	vmiB_blob.framer = &frB_blob;

	memset(&ctx_a, 0, sizeof(ctx_a));
	ctx_a.state = state;
	ctx_a.vmi_a = &vmiA_ours;
	ctx_a.vmi_b = &vmiB_ours;
	memset(&ctx_b, 0, sizeof(ctx_b));
	ctx_b.state = state;
	ctx_b.vmi_a = &vmiA_blob;
	ctx_b.vmi_b = &vmiB_blob;

	ra = fax_class1_status(&ctx_a, use_modem_status ? modembuf : NULL);
	rb = ref_fax_class1_status(&ctx_b, use_modem_status ? modembuf : NULL);

	diff_eq_int("state %d, modem_status %d: return", ra, rb, tag);

	if (state >= 4 && state <= 6) {
		diff_eq_int("state %d: return is 1 (vmi_a range)", ra, 1, tag);
		diff_eq_obj("state %d: vmi_a's framer, ours == blob",
			    struct faxvmi_framer, &frA_ours, &frA_blob, tag);
		diff_eq_obj("state %d: vmi_b's framer untouched",
			    struct faxvmi_framer, &frB_ours, &frB_orig, tag);
	} else if (state == 12 || state == 13) {
		diff_eq_int("state %d: return is 1 (vmi_b range)", ra, 1, tag);
		diff_eq_obj("state %d: vmi_b's framer, ours == blob",
			    struct faxvmi_framer, &frB_ours, &frB_blob, tag);
		diff_eq_obj("state %d: vmi_a's framer untouched",
			    struct faxvmi_framer, &frA_ours, &frA_orig, tag);
	} else {
		diff_eq_int("state %d: return is 0 (out of range)", ra, 0,
			    tag);
		diff_eq_obj("state %d: vmi_a's framer untouched",
			    struct faxvmi_framer, &frA_ours, &frA_orig, tag);
		diff_eq_obj("state %d: vmi_b's framer untouched",
			    struct faxvmi_framer, &frB_ours, &frB_orig, tag);
	}
}

static int
test_status(void)
{
	static const int states[] = {
		-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 100,
	};
	unsigned i;

	diff_begin("fax_class1_status");

	for (i = 0; i < sizeof(states) / sizeof(states[0]); i++) {
		run_case(states[i], 0, (long)(i * 2));
		run_case(states[i], 1, (long)(i * 2 + 1));
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= test_template();
	rc |= test_status();

	return rc;
}
