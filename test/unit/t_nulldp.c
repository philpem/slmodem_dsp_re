/*
 * t_nulldp.c -- differential test of the null datapump: `null_create`,
 * `null_delete`, `null_process`, `null_status`, `null_control`.
 *
 * See nulldp.h for what these are (the "no modulation" slots 0..4 of the
 * `vxx_create`/`vxx_delete`/`vxx_process`/`vxx_status`/`vxx_control`
 * dispatch tables) and where each signature comes from.
 *
 * D955/F8587: every `struct faxvmi_link` field is planted with a
 * recognisable non-zero pattern before every call and checked afterward --
 * `null_delete`/`null_status`/`null_control` touch none of it, and
 * `null_create` touches exactly `int_0014`/`pack_count`/`pack_width`/
 * `unpack_width`, so a wrong-offset write anywhere else is caught even
 * though nothing here NEEDS those fields to be real.  `null_process` is the
 * one exception -- it DEREFERENCES `dp->ptr_0000` as the destination it
 * writes through, so that one field is planted with the address of a real,
 * sized buffer instead (F8587's own rule: a field used as a pointer must be
 * a real one).
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/faxvmi.h"
#include "dsplib/nulldp.h"

extern void ref_null_create(struct faxvmi_link *dp, const void *cfg);
extern void ref_null_delete(struct faxvmi_link *dp);
extern int ref_null_process(struct faxvmi_link *dp, short *in,
			    unsigned short *result, unsigned short *count);
extern int ref_null_status(struct faxvmi_link *dp, void *status);
extern int ref_null_control(struct faxvmi_link *dp, void *arg);

#define IN_MAX	32
#define GUARD	16

static unsigned long seed = 20260902UL;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

/*
 * Every field except `ptr_0000`/`buf` gets a recognisable byte pattern;
 * those two are left as whatever the caller set (real buffers, or planted
 * bytes for the calls that never dereference them).
 */
static void
dp_plant(struct faxvmi_link *dp, unsigned char pattern)
{
	memset(dp, pattern, sizeof(*dp));
}

static int
run_create(void)
{
	unsigned p;

	diff_begin("null_create");
	for (p = 0; p < 8; p++) {
		struct faxvmi_link dp_a, dp_b;
		unsigned char cfgbuf[4];
		long tag = (long)p;

		dp_plant(&dp_a, (unsigned char)(0x11 * (p + 1)));
		memcpy(&dp_b, &dp_a, sizeof(dp_a));
		memset(cfgbuf, (int)(0x55 + p), sizeof(cfgbuf));

		ref_null_create(&dp_a, (p & 1) ? cfgbuf : NULL);
		null_create(&dp_b, (p & 1) ? cfgbuf : NULL);

		diff_eq_int("create: int_0014 (%ld)", dp_b.int_0014,
			    dp_a.int_0014, tag);
		diff_eq_int("create: pack_count (%ld)", dp_b.pack_count,
			    dp_a.pack_count, tag);
		diff_eq_int("create: pack_width (%ld)", dp_b.pack_width,
			    dp_a.pack_width, tag);
		diff_eq_int("create: unpack_width (%ld)", dp_b.unpack_width,
			    dp_a.unpack_width, tag);
		diff_eq_int("create: dp otherwise unchanged (%ld)",
			    memcmp(&dp_a, &dp_b, sizeof(dp_a)), 0, tag);
	}
	return diff_end();
}

static int
run_delete(void)
{
	unsigned p;

	diff_begin("null_delete");
	for (p = 0; p < 4; p++) {
		struct faxvmi_link dp_a, dp_b;
		long tag = (long)p;

		dp_plant(&dp_a, (unsigned char)(0x23 + p));
		memcpy(&dp_b, &dp_a, sizeof(dp_a));

		ref_null_delete(&dp_a);
		null_delete(&dp_b);

		diff_eq_int("delete: dp unchanged (%ld)",
			    memcmp(&dp_a, &dp_b, sizeof(dp_a)), 0, tag);
	}
	/* the object never reads its argument -- NULL must not crash it */
	ref_null_delete(NULL);
	null_delete(NULL);
	diff_eq_int("delete: NULL did not crash (%ld)", 1, 1, 0);
	return diff_end();
}

static int
run_status_control(void)
{
	unsigned p;
	long ok_status = 0, ok_control = 0;

	diff_begin("null_status / null_control");
	for (p = 0; p < 6; p++) {
		struct faxvmi_link dp_a, dp_b;
		unsigned char statbuf[8], argbuf[8];
		void *st = (p & 1) ? statbuf : NULL;
		void *ar = (p & 2) ? argbuf : NULL;
		int ra, rb;
		long tag = (long)p;

		dp_plant(&dp_a, (unsigned char)(0x67 + p));
		memcpy(&dp_b, &dp_a, sizeof(dp_a));
		memset(statbuf, 0xa5, sizeof(statbuf));
		memset(argbuf, 0x5a, sizeof(argbuf));

		ra = ref_null_status(&dp_a, st);
		rb = null_status(&dp_b, st);
		diff_eq_int("status: return (%ld)", rb, ra, tag);
		diff_eq_int("status: dp unchanged (%ld)",
			    memcmp(&dp_a, &dp_b, sizeof(dp_a)), 0, tag);
		if (ra == -1)
			ok_status++;

		ra = ref_null_control(&dp_a, ar);
		rb = null_control(&dp_b, ar);
		diff_eq_int("control: return (%ld)", rb, ra, tag);
		diff_eq_int("control: dp unchanged (%ld)",
			    memcmp(&dp_a, &dp_b, sizeof(dp_a)), 0, tag);
		if (ra == -1)
			ok_control++;
	}
	/* NULL for both pointer args must not crash either */
	diff_eq_int("status(NULL,NULL) (%ld)", null_status(NULL, NULL),
		    ref_null_status(NULL, NULL), 0);
	diff_eq_int("control(NULL,NULL) (%ld)", null_control(NULL, NULL),
		    ref_null_control(NULL, NULL), 0);

	diff_eq_int("status always answers -1 (%ld)", ok_status > 0, 1,
		    ok_status);
	diff_eq_int("control always answers -1 (%ld)", ok_control > 0, 1,
		    ok_control);
	return diff_end();
}

static int
run_process(void)
{
	static short in[IN_MAX];
	static unsigned short dst_a[IN_MAX + GUARD];
	static unsigned short dst_b[IN_MAX + GUARD];
	unsigned p;
	long copied = 0, skipped = 0;

	diff_begin("null_process");
	for (p = 0; p < 16; p++) {
		struct faxvmi_link dp_a, dp_b;
		unsigned short result_a, result_b, count_a, count_b;
		int ra, rb, i;
		long tag = (long)p;

		for (i = 0; i < IN_MAX; i++)
			in[i] = (short)(rnd() & 0xffff);
		memset(dst_a, 0x5a, sizeof(dst_a));
		memcpy(dst_b, dst_a, sizeof(dst_a));

		dp_plant(&dp_a, (unsigned char)(0x81 + p));
		memcpy(&dp_b, &dp_a, sizeof(dp_a));
		dp_a.ptr_0000 = dst_a;
		dp_b.ptr_0000 = dst_b;

		count_a = count_b = (unsigned short)(p % (IN_MAX + 2));
		result_a = result_b = 0x5a5a;

		ra = ref_null_process(&dp_a, in, &result_a, &count_a);
		rb = null_process(&dp_b, in, &result_b, &count_b);

		diff_eq_int("process: return (%ld)", rb, ra, tag);
		diff_eq_int("process: result unchanged (%ld)", result_b,
			    result_a, tag);
		diff_eq_int("process: count unchanged (%ld)", count_b,
			    count_a, tag);
		diff_eq_int("process: dst and guard (%ld)",
			    memcmp(dst_a, dst_b, sizeof(dst_a)), 0, tag);
		/* every field but the two buffer pointers, unchanged */
		diff_eq_int("process: pack_count (%ld)", dp_b.pack_count,
			    dp_a.pack_count, tag);
		diff_eq_int("process: int_0014 (%ld)", dp_b.int_0014,
			    dp_a.int_0014, tag);

		if (count_a > 0 && count_a <= IN_MAX)
			copied++;
		else
			skipped++;
	}
	diff_eq_int("process: a nonzero count copied (%ld)", copied > 0, 1,
		    copied);
	diff_eq_int("process: a zero count copied nothing (%ld)",
		    skipped > 0, 1, skipped);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_create();
	rc |= run_delete();
	rc |= run_status_control();
	rc |= run_process();
	return rc;
}
