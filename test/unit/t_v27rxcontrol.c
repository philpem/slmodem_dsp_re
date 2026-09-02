/*
 * t_v27rxcontrol.c -- differential test of `V27RX_control` (.text 0x0a32a0,
 *                     125 bytes).
 *
 * Confirmed READY and standalone by `tools/readyqueue.py` -- the blob span
 * that names it (`class1tx.c +94`) is a span-name trap, CLAUDE.md's own; the
 * function itself is v27.c's, on the receive side.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"

#include "dsplib/faxcfg.h"
#include "dsplib/v27fax.h"

extern void *ref_V27RX_create(void *modem, const struct v27rx_cfg *cfg);
extern void ref_V27RX_delete(void *modem);
extern int ref_V27RX_control(void *rx, void *req);

#define FIELD(obj, off)		((unsigned char *)(void *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define AT_I(obj, off)		(*(int *)(void *)FIELD((obj), (off)))

/* The request `V27RX_control` reads; see v27fax.h for the field layout. */
struct req {
	unsigned char pad_00[4];
	int int_0004;
	unsigned char pad_08[4];
	unsigned char mask;
	unsigned char flags;
};

static void
compare_after(const char *what, void *a, void *b, long tag)
{
	void *sha, *shb;
	void *rxa, *rxb;
	char buf[128];

	sha = FIELD_PTR(a, V27_OBJ_SHARED);
	shb = FIELD_PTR(b, V27_OBJ_SHARED);
	snprintf(buf, sizeof(buf), "%.80s sh.int_0004 (%%ld)", what);
	diff_eq_int(buf, AT_I(sha, V27SH_INT_0004), AT_I(shb, V27SH_INT_0004),
		    tag);

	snprintf(buf, sizeof(buf), "%.80s cfg.int_0008 (%%ld)", what);
	diff_eq_int(buf, ((struct v27rx_cfg *)a)->int_0008,
		    ((struct v27rx_cfg *)b)->int_0008, tag);

	rxa = FIELD_PTR(a, V27_OBJ_RX);
	rxb = FIELD_PTR(b, V27_OBJ_RX);
	snprintf(buf, sizeof(buf), "%.80s rx.en_00 (%%ld)", what);
	diff_eq_int(buf, AT_I(rxa, V27RX_EN_00), AT_I(rxb, V27RX_EN_00), tag);
	snprintf(buf, sizeof(buf), "%.80s rx.en_fse_lms (%%ld)", what);
	diff_eq_int(buf, AT_I(rxa, V27RX_EN_FSE_LMS),
		    AT_I(rxb, V27RX_EN_FSE_LMS), tag);
}

static const struct req cases[] = {
	{ { 0 }, 0,        { 0 }, 0x00, 0x00 },
	{ { 0 }, 12345,    { 0 }, 0x00, 0x00 },
	{ { 0 }, 0,        { 0 }, 0x08, 0x00 },
	{ { 0 }, 0,        { 0 }, 0x20, 0x00 },
	{ { 0 }, 0,        { 0 }, 0x28, 0x00 },
	{ { 0 }, 0,        { 0 }, 0x00, 0x10 },
	{ { 0 }, 0,        { 0 }, 0x00, 0x02 },
	{ { 0 }, 99,       { 0 }, 0x28, 0x12 },
	{ { 0 }, 0,        { 0 }, 0xff, 0xff },
};
#define NCASES ((long)(sizeof(cases) / sizeof(cases[0])))

static int
test_control(void)
{
	long k;

	diff_begin("v27rxcontrol: V27RX_control over its request's bits");

	for (k = 0; k < NCASES; k++) {
		void *a, *b;
		struct req ra, rb;
		int reta, retb;

		a = V27RX_create(0, 0);
		b = ref_V27RX_create(0, 0);
		diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, k);
		if (a == 0 || b == 0)
			continue;

		ra = cases[k];
		rb = cases[k];

		reta = V27RX_control(a, &ra);
		retb = ref_V27RX_control(b, &rb);

		diff_eq_int("return (%ld)", reta, retb, k);
		compare_after(cases[k].flags ? "flagged" : "plain", a, b, k);

		V27RX_delete(a);
		ref_V27RX_delete(b);
	}

	return diff_end();
}

static int
test_control_null_req(void)
{
	void *a, *b;
	int reta, retb;

	diff_begin("v27rxcontrol: V27RX_control(rx, NULL)");

	a = V27RX_create(0, 0);
	b = ref_V27RX_create(0, 0);
	diff_eq_int("both built (%ld)", a != 0 && b != 0, 1, 0);
	if (a == 0 || b == 0)
		return diff_end();

	reta = V27RX_control(a, 0);
	retb = ref_V27RX_control(b, 0);
	diff_eq_int("return (%ld)", reta, retb, 0);

	V27RX_delete(a);
	ref_V27RX_delete(b);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= test_control();
	rc |= test_control_null_req();

	return rc;
}
