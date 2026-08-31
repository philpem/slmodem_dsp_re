/*
 * t_cidsvc.c -- differential test of the Caller ID service object's setters.
 *
 * `cid_freq_sampl` retunes both receivers, and everything it does is gated on
 * `ctx->mode`, so the sweep is over the MODE as well as the rate: a test that
 * only ever passes mode 5 would pass with both gates deleted.
 *
 * THREE THINGS ARE PINNED HERE AND NONE OF THEM IS THE RETURN VALUE (there
 * is none):
 *
 *   - the whole of both receivers is compared, not the two fields the
 *     function writes.  `diff_eq_obj` over `struct dtmf_rx` and `struct cid`
 *     is what catches a store to the wrong offset, which is invisible to a
 *     field-by-field check that reads the same wrong offset back.
 *   - the receiver the mode excludes is asserted UNTOUCHED against its own
 *     pre-call copy, so "the gate is there" is measured rather than assumed.
 *   - the int-to-short truncation: 0x12580 is 9600 in the low half and is not
 *     9600 as an int, so it must set `rate` to 9600 and must NOT trip the
 *     `rate == 9600` arm.  That one input separates the two widths.
 *
 * ANTI-VACUITY.  Every arm of the object is counted ON THE REFERENCE SIDE and
 * asserted to have fired: the DTMF store, the FSK store, and each of the
 * three separate `f02c = 9` writes (mode 0 at 9600, mode 0 at 8000, mode
 * above 1).  A counter read off our own side would report our code's
 * coverage, not the object's -- finding F134.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/cid.h"
#include "dsplib/cid_modem.h"
#include "dsplib/dtmf_rx.h"

extern void ref_cid_freq_sampl(void *ctx, int rate);

#define GUARD	32

struct box {
	struct cid_modem ctx;
	struct dtmf_rx dtmf;
	struct cid fsk;
	unsigned char guard[GUARD];
};

static unsigned long seed = 20260831UL;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

static void
fill_bytes(void *p, size_t n)
{
	unsigned char *b = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++)
		b[i] = (unsigned char)(rnd() & 0xff);
}

/* Anti-vacuity, all counted on the REFERENCE side. */
static int seen_dtmf_store;
static int seen_fsk_store;
static int seen_f02c_9600;
static int seen_f02c_8000;
static int seen_f02c_mode;
static int seen_dtmf_gated;
static int seen_fsk_gated;

/*
 * Build the pair.  The whole box is filled with pseudorandom bytes first, so
 * every field the function does not write has a value that a spurious store
 * would change; the two pointers and the mode are then planted over the top.
 */
static void
build(struct box *a, struct box *b, int mode)
{
	fill_bytes(a, sizeof(*a));
	a->ctx.dtmf = &a->dtmf;
	a->ctx.fsk = &a->fsk;
	a->ctx.mode = mode;

	memcpy(b, a, sizeof(*a));
	b->ctx.dtmf = &b->dtmf;
	b->ctx.fsk = &b->fsk;
}

static void
run(int mode, int rate)
{
	struct box a, b, a0;
	char what[96];
	long tag = (long)rate;

	build(&a, &b, mode);
	memcpy(&a0, &a, sizeof(a0));

	ref_cid_freq_sampl(&a.ctx, rate);
	cid_freq_sampl(&b.ctx, rate);

	/*
	 * The two receiver pointers hold two different addresses on the two
	 * sides and always will, so they are zeroed in a copy rather than
	 * compared -- CLAUDE.md's "a loop is still right where some region
	 * must be skipped", done as a copy so the rest of the object still
	 * goes through diff_eq_obj and reports by field.
	 */
	{
		struct cid_modem ca = a.ctx, cb = b.ctx;

		ca.dtmf = cb.dtmf = 0;
		ca.fsk = cb.fsk = 0;
		snprintf(what, sizeof(what),
			 "mode %d: service object after %%ld", mode);
		diff_eq_obj(what, struct cid_modem, &cb, &ca, tag);
	}
	snprintf(what, sizeof(what), "mode %d: DTMF receiver after %%ld", mode);
	diff_eq_obj(what, struct dtmf_rx, &b.dtmf, &a.dtmf, tag);
	snprintf(what, sizeof(what), "mode %d: FSK receiver after %%ld", mode);
	diff_eq_obj(what, struct cid, &b.fsk, &a.fsk, tag);
	snprintf(what, sizeof(what), "mode %d: guard after %%ld", mode);
	diff_eq_int(what, memcmp(a.guard, b.guard, GUARD), 0, tag);

	/*
	 * The gates, against the pre-call copy of the reference side.  These
	 * are assertions about the OBJECT, so they are taken on `a`.
	 */
	if (mode == 0) {
		snprintf(what, sizeof(what),
			 "mode 0 leaves the DTMF receiver alone (%%ld)");
		diff_eq_int(what,
			    memcmp(&a.dtmf, &a0.dtmf, sizeof(a.dtmf)), 0, tag);
		seen_dtmf_gated++;
	}
	if (mode == 1) {
		snprintf(what, sizeof(what),
			 "mode 1 leaves the FSK receiver alone (%%ld)");
		diff_eq_int(what,
			    memcmp(&a.fsk, &a0.fsk, sizeof(a.fsk)), 0, tag);
		seen_fsk_gated++;
	}

	/* What the object actually did, read off the reference side. */
	if (mode != 0 && a.dtmf.rate == (short)rate
	    && a0.dtmf.rate != (short)rate)
		seen_dtmf_store++;
	if (mode != 1 && a.fsk.rate == (short)rate
	    && a0.fsk.rate != (short)rate)
		seen_fsk_store++;
	if (a.fsk.f02c == 9 && a0.fsk.f02c != 9) {
		if (mode > 1)
			seen_f02c_mode++;
		else if (mode == 0 && rate == 9600)
			seen_f02c_9600++;
		else if (mode == 0 && rate == 8000)
			seen_f02c_8000++;
	}
}

int
main(void)
{
	/*
	 * The modes the service uses are 0, 1 and 5; the others are here
	 * because the object tests `!= 0`, `!= 1` and `> 1` rather than a
	 * member of a set, and a negative one is what separates `> 1` from a
	 * `!= 1` reading.
	 */
	static const int modes[] = { 0, 1, 2, 3, 4, 5, 6, 100, -1, -5 };
	/*
	 * 0x12580 and 0x11f40 are 9600 and 8000 in the low sixteen bits and
	 * neither as an int -- the truncation probe.  0x8000 and 0xffff turn
	 * the stored short negative.
	 */
	static const int rates[] = {
		8000, 9600, 7200, 0, 1, -1, 0x12580, 0x11f40,
		0x8000, 0xffff, 32767, -32768, 0x7fffffff
	};
	unsigned i, j;
	int rc;

	diff_begin("cid_freq_sampl over every mode and rate");
	for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
		for (j = 0; j < sizeof(rates) / sizeof(rates[0]); j++)
			run(modes[i], rates[j]);
	rc = diff_end();

	diff_begin("cid_freq_sampl: every arm of the object fired");
	diff_eq_int("DTMF rate stores (%ld)", seen_dtmf_store > 0, 1,
		    seen_dtmf_store);
	diff_eq_int("FSK rate stores (%ld)", seen_fsk_store > 0, 1,
		    seen_fsk_store);
	diff_eq_int("f02c stores from mode 0 at 9600 (%ld)",
		    seen_f02c_9600 > 0, 1, seen_f02c_9600);
	diff_eq_int("f02c stores from mode 0 at 8000 (%ld)",
		    seen_f02c_8000 > 0, 1, seen_f02c_8000);
	diff_eq_int("f02c stores from mode > 1 (%ld)", seen_f02c_mode > 0, 1,
		    seen_f02c_mode);
	diff_eq_int("mode 0 cases with the DTMF gate shut (%ld)",
		    seen_dtmf_gated > 0, 1, seen_dtmf_gated);
	diff_eq_int("mode 1 cases with the FSK gate shut (%ld)",
		    seen_fsk_gated > 0, 1, seen_fsk_gated);
	rc |= diff_end();

	return rc;
}
