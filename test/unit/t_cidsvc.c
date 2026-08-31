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
extern char *ref_cid_get_strings(void *ctx);
extern void *ref_cid_create(void *ctx, int cid_val, int mode);
extern void ref_cid_delete(void *ctx);
extern void ref_cid_reset(void *ctx);

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

/* ------------------------------------------------------------------ */
/*
 * cid_get_strings.  The renderers underneath it have their own test
 * (t_cidata); what is measured here is the ROUTING -- which receiver the mode
 * picks, which renderer `f264` picks, that the buffer is cleared in full
 * first, and that the answer is `ctx + 8` and not a copy.
 *
 * The box is heap-sized with a wide trailing guard because
 * `data_formatted_output` has no output bound of its own: 0x258 is the
 * object's budget, not a clamp it enforces, so a message that renders long
 * would run past `strings` into the rest of the object.  Both sides would do
 * it identically and the comparison would still pass -- the guard is what
 * turns that into a named failure instead of a stack smash.
 */

#define BIGGUARD	4096

struct sbox {
	struct cid_modem ctx;
	unsigned char guard[BIGGUARD];
	struct dtmf_rx dtmf;
	struct cid fsk;
};

static int seen_dtmf_path;
static int seen_raw_path;
static int seen_formatted_path;
static int seen_rendered;	/* the FSK path wrote something */

static void
sbuild(struct sbox *a, struct sbox *b, int mode, int f264,
       const unsigned char *msg, int msglen)
{
	fill_bytes(a, sizeof(*a));
	memset(a->guard, 0xa5, sizeof(a->guard));
	a->ctx.dtmf = &a->dtmf;
	a->ctx.fsk = &a->fsk;
	a->ctx.mode = mode;
	a->ctx.f264 = f264;

	/*
	 * D955/F8587: `pack_len` is a SUBSCRIPT into `data` for the renderers,
	 * so it is planted rather than left to the random fill, and the
	 * message is copied in whole.  A wild `pack_len` would be read
	 * identically by both sides and the test would agree on nonsense.
	 */
	memset(a->fsk.data, 0, sizeof(a->fsk.data));
	if (msglen > (int)sizeof(a->fsk.data))
		msglen = (int)sizeof(a->fsk.data);
	memcpy(a->fsk.data, msg, (size_t)msglen);
	a->fsk.pack_len = (short)msglen;

	memcpy(b, a, sizeof(*a));
	b->ctx.dtmf = &b->dtmf;
	b->ctx.fsk = &b->fsk;
}

static void
srun(const char *what, int mode, int f264, const unsigned char *msg,
     int msglen, long tag)
{
	static struct sbox a, b;
	char label[128];
	char *ra, *rb;
	unsigned char clean[BIGGUARD];

	memset(clean, 0xa5, sizeof(clean));
	sbuild(&a, &b, mode, f264, msg, msglen);

	ra = ref_cid_get_strings(&a.ctx);
	rb = cid_get_strings(&b.ctx);

	snprintf(label, sizeof(label), "%s: returns ctx + 8 (%%ld)", what);
	diff_eq_int(label, rb == b.ctx.strings, 1, tag);
	snprintf(label, sizeof(label), "%s: reference returns ctx + 8 (%%ld)",
		 what);
	diff_eq_int(label, ra == a.ctx.strings, 1, tag);

	snprintf(label, sizeof(label), "%s: strings after %%ld", what);
	diff_eq_int(label, memcmp(a.ctx.strings, b.ctx.strings,
				  sizeof(a.ctx.strings)), 0, tag);
	snprintf(label, sizeof(label), "%s: DTMF receiver after %%ld", what);
	diff_eq_obj(label, struct dtmf_rx, &b.dtmf, &a.dtmf, tag);
	snprintf(label, sizeof(label), "%s: FSK receiver after %%ld", what);
	diff_eq_obj(label, struct cid, &b.fsk, &a.fsk, tag);

	snprintf(label, sizeof(label), "%s: reference stayed in bounds (%%ld)",
		 what);
	diff_eq_int(label, memcmp(a.guard, clean, BIGGUARD), 0, tag);
	snprintf(label, sizeof(label), "%s: we stayed in bounds (%%ld)", what);
	diff_eq_int(label, memcmp(b.guard, clean, BIGGUARD), 0, tag);

	/* Which arm the OBJECT took, measured on the reference side. */
	if (mode != 0 && mode != 2) {
		seen_dtmf_path++;
		snprintf(label, sizeof(label),
			 "%s: DTMF path copied sixteen digits (%%ld)", what);
		diff_eq_int(label,
			    memcmp(a.ctx.strings, a.dtmf.digits, 16), 0, tag);
		snprintf(label, sizeof(label),
			 "%s: DTMF path stopped at sixteen (%%ld)", what);
		diff_eq_int(label, a.ctx.strings[16], 0, tag);
	} else {
		if (f264 == 2)
			seen_raw_path++;
		else
			seen_formatted_path++;
		if (a.ctx.strings[0] != 0)
			seen_rendered++;
	}
}

/* ------------------------------------------------------------------ */
/*
 * The lifecycle: cid_create, cid_reset, cid_delete.
 *
 * Three pointers hold different addresses on the two sides by construction --
 * the two receivers, the DTMF receiver's own `bufp`, and the resampler's
 * `mrf.history` -- so each is compared for NULLNESS and then zeroed in a copy
 * before the object comparison, which is CLAUDE.md's "a loop is still right
 * where some region must be skipped" applied four times.  The resampler
 * history is compared by CONTENT over `history_len` shorts, which is what
 * keeps the skip from hiding the buffer the reset just filled.
 */

static int seen_built_dtmf;
static int seen_built_fsk;
static int seen_built_both;
static int seen_reused;
static int seen_deletes;

static void
cmp_pair(const char *what, struct cid_modem *a, struct cid_modem *b, long tag)
{
	char label[160];
	struct cid_modem ca = *a, cb = *b;

	snprintf(label, sizeof(label), "%s: DTMF built on both sides (%%ld)",
		 what);
	diff_eq_int(label, (a->dtmf != 0) == (b->dtmf != 0), 1, tag);
	snprintf(label, sizeof(label), "%s: FSK built on both sides (%%ld)",
		 what);
	diff_eq_int(label, (a->fsk != 0) == (b->fsk != 0), 1, tag);

	ca.dtmf = cb.dtmf = 0;
	ca.fsk = cb.fsk = 0;
	snprintf(label, sizeof(label), "%s: service object %%ld", what);
	diff_eq_obj(label, struct cid_modem, &cb, &ca, tag);

	if (a->dtmf && b->dtmf) {
		struct dtmf_rx da = *a->dtmf, db = *b->dtmf;

		snprintf(label, sizeof(label),
			 "%s: DTMF bufp both set or both clear (%%ld)", what);
		diff_eq_int(label, (da.bufp != 0) == (db.bufp != 0), 1, tag);
		da.bufp = db.bufp = 0;
		snprintf(label, sizeof(label), "%s: DTMF receiver %%ld", what);
		diff_eq_obj(label, struct dtmf_rx, &db, &da, tag);
		seen_built_dtmf++;
	}

	if (a->fsk && b->fsk) {
		struct cid fa = *a->fsk, fb = *b->fsk;

		snprintf(label, sizeof(label),
			 "%s: mrf history both set or both clear (%%ld)", what);
		diff_eq_int(label,
			    (fa.mrf.history != 0) == (fb.mrf.history != 0), 1,
			    tag);
		if (fa.mrf.history && fb.mrf.history) {
			int n = fa.mrf.history_len < fb.mrf.history_len
				? fa.mrf.history_len : fb.mrf.history_len;

			snprintf(label, sizeof(label),
				 "%s: mrf history contents %%ld", what);
			diff_eq_int(label,
				    memcmp(fa.mrf.history, fb.mrf.history,
					   (size_t)n * sizeof(short)),
				    0, tag);
		}
		/*
		 * `mrf.cfg.coeff` points at V23_MRF_FILT, which is a
		 * file-static of Rxcid.c on both sides -- so the two pointers
		 * differ and the ninety taps behind them must not.  That
		 * comparison is the only place the blob's table is checked
		 * against ours through the reset that installs it.
		 */
		snprintf(label, sizeof(label),
			 "%s: mrf coeff both set or both clear (%%ld)", what);
		diff_eq_int(label,
			    (fa.mrf.cfg.coeff != 0) == (fb.mrf.cfg.coeff != 0),
			    1, tag);
		if (fa.mrf.cfg.coeff && fb.mrf.cfg.coeff) {
			int n = fa.mrf.cfg.taps < fb.mrf.cfg.taps
				? fa.mrf.cfg.taps : fb.mrf.cfg.taps;

			snprintf(label, sizeof(label),
				 "%s: mrf coefficients %%ld", what);
			diff_eq_int(label,
				    memcmp(fa.mrf.cfg.coeff, fb.mrf.cfg.coeff,
					   (size_t)n * sizeof(short)),
				    0, tag);
		}

		fa.mrf.history = fb.mrf.history = 0;
		fa.mrf.cfg.coeff = fb.mrf.cfg.coeff = 0;
		snprintf(label, sizeof(label), "%s: FSK receiver %%ld", what);
		diff_eq_obj(label, struct cid, &fb, &fa, tag);
		seen_built_fsk++;
	}

	if (a->dtmf && a->fsk)
		seen_built_both++;
}

static void
lifecycle(int mode, int cid_val)
{
	struct cid_modem *a, *b;
	char what[96];
	void *ra, *rb;
	long tag = (long)cid_val;
	int base, dref, dours;

	snprintf(what, sizeof(what), "mode %d value %d", mode, cid_val);

	/*
	 * ALLOCATION BALANCE IS WHAT MAKES cid_delete OBSERVABLE.  Nothing
	 * survives a delete to be compared, so the only differential evidence
	 * is how many blocks each side released; a delete that skips the FSK
	 * receiver or frees the wrong pointer moves this and nothing else.
	 * The counts are taken as DELTAS per side and compared with each
	 * other, so the second create's known leak (create_cid nulls
	 * mrf.history and the reset allocates a new buffer) does not have to
	 * be modelled -- only matched.
	 */
	base = harness_alloc.live;
	ra = ref_cid_create(0, cid_val, mode);
	dref = harness_alloc.live - base;
	base = harness_alloc.live;
	rb = cid_create(0, cid_val, mode);
	dours = harness_alloc.live - base;
	diff_eq_int("cid_create allocated the same block count (%ld)", dours,
		    dref, tag);

	diff_eq_int("cid_create returned non-null on both sides (%ld)",
		    (ra != 0) == (rb != 0), 1, tag);
	if (!ra || !rb)
		return;
	a = (struct cid_modem *)ra;
	b = (struct cid_modem *)rb;
	cmp_pair(what, a, b, tag);

	/* A second create on the SAME object must reuse, not rebuild. */
	{
		void *da = a->dtmf, *fa = a->fsk;

		ref_cid_create(a, cid_val + 1, 0);
		cid_create(b, cid_val + 1, 0);
		cmp_pair("second create", a, b, tag);
		diff_eq_int("second create reused the DTMF receiver (%ld)",
			    a->dtmf == da, 1, tag);
		diff_eq_int("second create reused the FSK receiver (%ld)",
			    a->fsk == fa, 1, tag);
		seen_reused++;
	}

	/*
	 * DIRTY THE STATE cid_reset IS SUPPOSED TO CLEAR before calling it.
	 * `f3f8` comes out of create already zero, so a reset that never
	 * touches it is indistinguishable from one that does unless something
	 * puts a value there first -- the injection ritual missed exactly that
	 * mutant until this existed (finding F8732).  The same argument covers
	 * the receivers, so both are scribbled on identically.
	 */
	a->f3f8 = b->f3f8 = 0x1234;
	if (a->fsk && b->fsk) {
		a->fsk->pack_len = b->fsk->pack_len = 11;
		a->fsk->pack_state = b->fsk->pack_state = 2;
		a->fsk->mark_bal = b->fsk->mark_bal = 9;
		a->fsk->thresh = b->fsk->thresh = 4242;
	}
	if (a->dtmf && b->dtmf) {
		a->dtmf->ndigits = b->dtmf->ndigits = 4;
		a->dtmf->state = b->dtmf->state = 2;
	}

	ref_cid_reset(a);
	cid_reset(b);
	cmp_pair("after reset", a, b, tag);

	/*
	 * And the clamp on the reset path, which is a different statement
	 * from cid_create's: raise the mode above 1 and it must come back 5.
	 * Both receivers already exist here, so nothing walks a null.
	 */
	if (a->dtmf && a->fsk) {
		a->mode = b->mode = 3;
		a->f3f8 = b->f3f8 = 0x4321;
		ref_cid_reset(a);
		cid_reset(b);
		cmp_pair("after reset from mode 3", a, b, tag);
		diff_eq_int("reset clamped mode 3 to 5 (%ld)", a->mode, 5, tag);
	}

	base = harness_alloc.live;
	ref_cid_delete(a);
	dref = harness_alloc.live - base;
	base = harness_alloc.live;
	cid_delete(b);
	dours = harness_alloc.live - base;
	diff_eq_int("cid_delete released the same block count (%ld)", dours,
		    dref, tag);
	diff_eq_int("no free of a pointer the allocator never gave out (%ld)",
		    harness_alloc.bad_free, 0, tag);
	diff_eq_int("the live set never overflowed (%ld)",
		    harness_alloc.overflow, 0, tag);
	seen_deletes++;
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

	/* ---------------------------------------------------------------- */
	{
		/*
		 * A multiple-data-message frame: type, total length, then
		 * (tag, length, bytes) entries.  Tags 1, 2 and 7 are the three
		 * `_look_for_other_than` excludes, so a frame carrying only
		 * those exercises the labelled fields and one carrying tag 3
		 * as well exercises the leftover arm.
		 */
		static const unsigned char mdmf[] = {
			0x80, 29,
			1, 8, '0','8','3','1','1','2','3','4',
			2, 10, '5','5','5','1','2','3','4','5','6','7',
			7, 5, 'A','B','C','D','E'
		};
		static const unsigned char mdmf_extra[] = {
			0x80, 36,
			1, 8, '0','8','3','1','1','2','3','4',
			2, 10, '5','5','5','1','2','3','4','5','6','7',
			7, 5, 'A','B','C','D','E',
			3, 5, 'x','y','z','!','?'
		};
		/* A single-data-message frame: no tags at all. */
		static const unsigned char sdmf[] = {
			0x04, 18,
			'0','8','3','1','1','2','3','4',
			'5','5','5','1','2','3','4','5','6','7'
		};
		static const unsigned char empty[] = { 0x80, 0 };
		static const struct {
			const char *name;
			const unsigned char *msg;
			int len;
		} frames[] = {
			{ "mdmf", mdmf, (int)sizeof(mdmf) },
			{ "mdmf+leftover", mdmf_extra, (int)sizeof(mdmf_extra) },
			{ "sdmf", sdmf, (int)sizeof(sdmf) },
			{ "empty frame", empty, (int)sizeof(empty) },
			{ "no message", empty, 0 }
		};
		static const int gs_modes[] = { 0, 2, 1, 5, 3, -1 };
		static const int gs_vals[] = { 0, 1, 2, 3, -2 };
		unsigned f, m, v;
		char what[96];

		diff_begin("cid_get_strings routing");
		for (f = 0; f < sizeof(frames) / sizeof(frames[0]); f++)
			for (m = 0; m < sizeof(gs_modes) / sizeof(gs_modes[0]);
			     m++)
				for (v = 0;
				     v < sizeof(gs_vals) / sizeof(gs_vals[0]);
				     v++) {
					snprintf(what, sizeof(what),
						 "%s mode %d value %d",
						 frames[f].name, gs_modes[m],
						 gs_vals[v]);
					srun(what, gs_modes[m], gs_vals[v],
					     frames[f].msg, frames[f].len,
					     (long)f);
				}
		rc |= diff_end();

		diff_begin("cid_get_strings: every route was taken");
		diff_eq_int("DTMF routes (%ld)", seen_dtmf_path > 0, 1,
			    seen_dtmf_path);
		diff_eq_int("raw routes (%ld)", seen_raw_path > 0, 1,
			    seen_raw_path);
		diff_eq_int("formatted routes (%ld)", seen_formatted_path > 0,
			    1, seen_formatted_path);
		diff_eq_int("FSK routes that rendered something (%ld)",
			    seen_rendered > 0, 1, seen_rendered);
		rc |= diff_end();
	}

	/* ---------------------------------------------------------------- */
	{
		static const int lc_modes[] = { 0, 1, 2, 3, 5, -1 };
		static const int lc_vals[] = { 0, 2, 7 };
		unsigned m, v;

		diff_begin("cid_create / cid_reset / cid_delete lifecycle");
		for (m = 0; m < sizeof(lc_modes) / sizeof(lc_modes[0]); m++)
			for (v = 0; v < sizeof(lc_vals) / sizeof(lc_vals[0]);
			     v++)
				lifecycle(lc_modes[m], lc_vals[v]);
		rc |= diff_end();

		diff_begin("the lifecycle built both receivers");
		diff_eq_int("objects with a DTMF receiver (%ld)",
			    seen_built_dtmf > 0, 1, seen_built_dtmf);
		diff_eq_int("objects with an FSK receiver (%ld)",
			    seen_built_fsk > 0, 1, seen_built_fsk);
		diff_eq_int("objects with both (%ld)", seen_built_both > 0, 1,
			    seen_built_both);
		diff_eq_int("second creates that reused (%ld)", seen_reused > 0,
			    1, seen_reused);
		diff_eq_int("lifecycles that reached the delete (%ld)",
			    seen_deletes > 0, 1, seen_deletes);
		rc |= diff_end();
	}

	return rc;
}
