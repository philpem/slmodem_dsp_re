/*
 * t_faxcreate.c -- differential test of `FAX_create` and
 * `FAX_class1_command` (`src/service/voice.c`), the last two of fax's
 * nine-symbol core-service closure (F10121).
 *
 * `FAX_create` calls `RcFixed_Create`/`RcFixed_Delete` (untracked, plain
 * `calloc`/`free` per `t_faxdelete.c`'s own note) and `fax_class1_create`
 * (tracked, `sysdep_malloc`).  Pointer VALUES can never match between
 * `ref_` and `ours` -- different allocators, different binaries -- so what
 * is checked is STRUCTURAL: NULL-ness of `rc_a`/`rc_b`/`class1`, and every
 * plain-integer field `FAX_create` itself computes.
 *
 * `harness_sreg_set`/`HARNESS_SREGS` supply `modem_get_sreg`'s S7 answer --
 * shared storage, read identically by `ref_modem_get_sreg` and
 * `modem_get_sreg` (`test/harness/runtime.c`), so both sides see the same
 * value with no per-side setup needed.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/debug.h"
#include "dsplib/fax.h"

extern struct fax_ctx *ref_FAX_create(void *modem, int originate,
				      unsigned int rate);
extern void ref_FAX_delete(struct fax_ctx *ctx);
extern int ref_FAX_class1_command(struct fax_ctx *ctx, int cmd, void *arg);
extern unsigned int ref_dsplibs_debug_level;

static void
cmp_ctx(const char *what, struct fax_ctx *a, struct fax_ctx *b, long tag)
{
	char buf[64];

#define FLD(name) \
	do { \
		(void)snprintf(buf, sizeof(buf), "%s.%s (%%ld)", what, #name); \
		diff_eq_int(buf, b->name, a->name, tag); \
	} while (0)
	FLD(host_frame_samples);
	FLD(out_produced);
	FLD(out_write_half);
#undef FLD
	(void)snprintf(buf, sizeof(buf), "%s rc_a NULL-ness (%%ld)", what);
	diff_eq_int(buf, b->rc_a != NULL, a->rc_a != NULL, tag);
	(void)snprintf(buf, sizeof(buf), "%s rc_b NULL-ness (%%ld)", what);
	diff_eq_int(buf, b->rc_b != NULL, a->rc_b != NULL, tag);
	(void)snprintf(buf, sizeof(buf), "%s class1 NULL-ness (%%ld)", what);
	diff_eq_int(buf, b->class1 != NULL, a->class1 != NULL, tag);
	(void)snprintf(buf, sizeof(buf), "%s modem (%%ld)", what);
	diff_eq_int(buf, (long)(size_t)b->modem, (long)(size_t)a->modem, tag);
}

static int
run_create(int originate, unsigned int rate, long s7, long tag)
{
	struct fax_ctx *a, *b;
	int rc;
	void *fake_modem = (void *)0x1234;

	harness_sreg_set(7, s7);

	diff_begin("FAX_create");
	a = ref_FAX_create(fake_modem, originate, rate);
	b = FAX_create(fake_modem, originate, rate);

	diff_eq_int("a != NULL (%ld)", b != NULL, a != NULL, tag);
	if (a != NULL && b != NULL)
		cmp_ctx("create", a, b, tag);

	rc = diff_end();

	if (a != NULL)
		ref_FAX_delete(a);
	if (b != NULL)
		FAX_delete(b);

	return rc;
}

/* An unrecognised rate: both resamplers fail to build, teardown, NULL. */
static int
run_create_bad_rate(long tag)
{
	struct fax_ctx *a, *b;
	int rc;
	void *fake_modem = (void *)0x5678;

	harness_sreg_set(7, 0);

	diff_begin("FAX_create: unrecognised rate");
	a = ref_FAX_create(fake_modem, 1, 44100);
	b = FAX_create(fake_modem, 1, 44100);
	diff_eq_int("NULL on unrecognised rate (%ld)", b == NULL, a == NULL,
		    tag);
	rc = diff_end();

	if (a != NULL)
		ref_FAX_delete(a);
	if (b != NULL)
		FAX_delete(b);

	return rc;
}

static int
run_debug_on(void)
{
	struct fax_ctx *a, *b;
	int rc;

	dsplibs_debug_level = ref_dsplibs_debug_level = 2;
	harness_sreg_set(7, 42);

	diff_begin("FAX_create: debug_level > 1");
	a = ref_FAX_create((void *)0x9, 1, 9600);
	b = FAX_create((void *)0x9, 1, 9600);
	diff_eq_int("a != NULL (%ld)", b != NULL, a != NULL, 900);
	if (a != NULL && b != NULL)
		cmp_ctx("debug", a, b, 900);
	rc = diff_end();

	if (a != NULL)
		ref_FAX_delete(a);
	if (b != NULL)
		FAX_delete(b);

	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	return rc;
}

/* FAX_class1_command: ctx == NULL, and ctx->class1 == NULL. */
static int
run_command_null_guards(long tag)
{
	struct fax_ctx dead;
	int ra, rb;

	memset(&dead, 0, sizeof(dead));

	diff_begin("FAX_class1_command: NULL guards");
	ra = ref_FAX_class1_command(NULL, FAXC1_FTH, (void *)3);
	rb = FAX_class1_command(NULL, FAXC1_FTH, (void *)3);
	diff_eq_int("ctx==NULL ret (%ld)", rb, ra, tag);

	ra = ref_FAX_class1_command(&dead, FAXC1_FTH, (void *)3);
	rb = FAX_class1_command(&dead, FAXC1_FTH, (void *)3);
	diff_eq_int("class1==NULL ret (%ld)", rb, ra, tag + 1);

	return diff_end();
}

/* Every FAXC1_* command, through a real session built by FAX_create. */
static int
run_command(int cmd, void *arg, long tag)
{
	struct fax_ctx *a, *b;
	int ra, rb, rc;

	harness_sreg_set(7, 60);
	a = ref_FAX_create((void *)0x42, 1, 8000);
	b = FAX_create((void *)0x42, 1, 8000);

	diff_begin("FAX_class1_command");
	ra = ref_FAX_class1_command(a, cmd, arg);
	rb = FAX_class1_command(b, cmd, arg);
	diff_eq_int("ret (%ld)", rb, ra, tag);
	if (a != NULL && b != NULL)
		diff_eq_int("class1 state (%ld)", b->class1->state,
			    a->class1->state, tag);

	rc = diff_end();

	if (a != NULL)
		ref_FAX_delete(a);
	if (b != NULL)
		FAX_delete(b);
	return rc;
}

/* An invalid command / rate combination: rejected, no dispatch. */
static int
run_command_invalid(long tag)
{
	struct fax_ctx *a, *b;
	int ra, rb, rc;

	harness_sreg_set(7, 0);
	a = ref_FAX_create((void *)0x1, 1, 8000);
	b = FAX_create((void *)0x1, 1, 8000);

	diff_begin("FAX_class1_command: invalid combinations");
	ra = ref_FAX_class1_command(a, 6, (void *)0);	/* cmd out of range */
	rb = FAX_class1_command(b, 6, (void *)0);
	diff_eq_int("bad cmd ret (%ld)", rb, ra, tag);

	ra = ref_FAX_class1_command(a, FAXC1_FTH, (void *)4);	/* != 3 */
	rb = FAX_class1_command(b, FAXC1_FTH, (void *)4);
	diff_eq_int("FTH bad rate ret (%ld)", rb, ra, tag + 1);

	ra = ref_FAX_class1_command(a, FAXC1_FTM, (void *)99);	/* not a rate */
	rb = FAX_class1_command(b, FAXC1_FTM, (void *)99);
	diff_eq_int("FTM bad rate ret (%ld)", rb, ra, tag + 2);

	rc = diff_end();

	if (a != NULL)
		ref_FAX_delete(a);
	if (b != NULL)
		FAX_delete(b);
	return rc;
}

static int
run_command_debug_on(void)
{
	struct fax_ctx *a, *b;
	int rc;

	dsplibs_debug_level = ref_dsplibs_debug_level = 2;
	harness_sreg_set(7, 0);
	a = ref_FAX_create((void *)0x1, 1, 8000);
	b = FAX_create((void *)0x1, 1, 8000);

	diff_begin("FAX_class1_command: debug_level > 1");
	(void)ref_FAX_class1_command(a, FAXC1_FTM, (void *)0x18);
	(void)FAX_class1_command(b, FAXC1_FTM, (void *)0x18);
	if (a != NULL && b != NULL)
		diff_eq_int("class1 f1244 (%ld)", b->class1->f1244,
			    a->class1->f1244, 900);
	rc = diff_end();

	if (a != NULL)
		ref_FAX_delete(a);
	if (b != NULL)
		FAX_delete(b);
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	return rc;
}

int
main(void)
{
	int rc = 0;

	rc |= run_create(1, 8000, 0, 10);	/* originate, identity rate */
	rc |= run_create(0, 8000, 15, 11);	/* answer, identity rate */
	rc |= run_create(1, 9600, 30, 12);
	rc |= run_create(1, 48000, 45, 13);
	rc |= run_create_bad_rate(14);
	rc |= run_debug_on();

	rc |= run_command_null_guards(20);
	rc |= run_command(FAXC1_FTH, (void *)3, 30);
	rc |= run_command(FAXC1_FRH, (void *)3, 31);
	rc |= run_command(FAXC1_FTM, (void *)0x18, 32);
	rc |= run_command(FAXC1_FRM, (void *)0x60, 33);
	rc |= run_command(FAXC1_FTS, (void *)3000, 34);
	rc |= run_command(FAXC1_FRS, (void *)3000, 35);
	rc |= run_command_invalid(40);
	rc |= run_command_debug_on();

	return rc;
}
