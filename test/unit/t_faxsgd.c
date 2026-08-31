/*
 * t_faxsgd.c -- differential test of the SGD sequence engine (src/fax/sgd.c).
 *
 * THIS TEST IS A REBUILD, AND THE THING IT REBUILDS IS THE DETECTOR THAT
 * CAUGHT THE LAST ATTEMPT.  Wave 1 wrote the engine, failed 96 of 3,603
 * checks under the period compiler and withdrew it (finding F8497) -- and
 * then deleted the test along with the code, so what survived was a
 * description rather than a file.  The one property that description insists
 * on is reproduced here first:
 *
 *   `status.det_at` IS COMPARED AS AN OFFSET FROM THE HISTORY BUFFER, never
 *   as a pointer value.
 *
 * It is a pointer INTO the object's own `hist` allocation, so the two sides
 * hold two different addresses for ever and any direct comparison is
 * meaningless.  Subtracting each side's own `hist` makes them comparable --
 * and makes a wrong answer legible, because an offset of -6181684 says
 * "megabytes outside a fifty-symbol buffer" where a byte compare of the
 * object would have said "one differing run at +0x38" and named no field.
 *
 * WHAT THE OFFSET ACTUALLY CAUGHT LAST TIME, now that the object has been
 * re-read: `SGD_sequence_det` leaves its best-alignment index UNINITIALISED
 * when its search loop never runs, which happens for n <= 0, and then both
 * returns it and computes `det_at` from it.  That is deviation D1060, not a
 * layout error -- so this test drives n >= 1 everywhere and says so at the
 * call site.  F8495's object model is confirmed, not replaced.
 *
 * THE OTHER FOUR THINGS THE FIXTURE HAS TO ARRANGE, each of which is a way
 * of getting a green run that means nothing:
 *
 * 1. EVERY SYMBOL FITS IN A BYTE.  `SGD_correlate` and `SGD_sequence_det`
 *    XOR two symbol words and index `FPM_xor_table` with the full 16-bit
 *    result (finding F8494), so a pair differing above bit 7 reads PAST the
 *    256-entry table -- into the blob's neighbouring .rodata on one side and
 *    ours on the other.  That is D955/F8587's unplanted subscript in its
 *    exact form: both sides would read out of bounds, and the readings would
 *    NOT agree because the neighbours differ.  Every symbol this test
 *    generates is masked to 0x00ff.
 *
 * 2. THE HISTORY ALLOCATION IS SIZED FROM `hist_extra` AND ZEROED FROM
 *    `ref_len`.  `SGD_create` allocates 2*(hist_len+hist_extra)-2 bytes and
 *    then zeroes hist_len+ref_len-1 SYMBOLS through it, so any config with
 *    ref_len > hist_extra overruns its own buffer in the constructor.  Every
 *    config here keeps hist_extra >= ref_len; D1061 records the shape.
 *
 * 3. `SGD_correlate` COMPUTES 1/(p[0]*n) IN INTEGERS and takes SIGFPE when
 *    that product is zero.  p[0] and n are never both-or-either zero here.
 *
 * 4. `SGD_pattern_det` SHIFTS ITS BIT MASK ARITHMETICALLY.  With sym_bits
 *    == 16 the mask starts at (short)0x8000 and `>>= 1` never reaches zero,
 *    so the object spins for ever (D1062).  sym_bits stays in 1..15.
 *
 * ALLOCATOR FILL IS LOAD-BEARING HERE.  `struct sgd_status` has two 2-byte
 * holes that NOTHING ever writes and `SGD_status`'s struct assignment copies
 * out regardless.  Both sides' objects come from the harness's
 * sysdep_malloc, which fills with HARNESS_MALLOC_FILL, so the holes agree --
 * and a caller-supplied object is memset to the same byte on both sides for
 * the same reason.  Were either side to see a zero page instead, the two
 * would differ on bytes neither implementation is responsible for.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/fpm.h"
#include "dsplib/sgd.h"

extern struct sgd *ref_SGD_create(struct sgd *s, const struct sgd_cfg *cfg);
extern void ref_SGD_delete(struct sgd *s);
extern void ref_SGD_control(struct sgd *s, struct sgd_control_req *req);
extern void ref_SGD_status(struct sgd *s, struct sgd_status *out);
extern short ref_SGD_symbol_gen(struct sgd *s, unsigned short *out, short n);
extern short ref_SGD_sequence_gen(struct sgd *s, unsigned short *out, short n);
extern short ref_SGD_correlate(const short *p, const unsigned short *a,
			       const unsigned short *b, short n);
extern short ref_SGD_sequence_det(struct sgd *s, const unsigned short *sym,
				  short n);
extern short ref_SGD_pattern_det(struct sgd *s, const short *sym, short n);

extern const short ref_FPM_xor_table[256];

static unsigned long seed = 20260831UL;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

/* Symbols are byte-ranged -- see note 1 at the top of the file. */
static unsigned short
rnd_sym(void)
{
	return (unsigned short)(rnd() & 0xffUL);
}

/*
 * COMPARE TWO SGD OBJECTS.
 *
 * Two fields cannot be compared by value and are handled separately rather
 * than skipped: `hist` is each side's own allocation, and `det_at` points
 * into it.  `det_at` becomes an offset -- the whole point of this test --
 * and `hist` becomes its CONTENTS over the span the engine actually uses.
 *
 * Everything else goes through diff_eq_obj so that a difference is reported
 * as a field path rather than a byte run.  The two pointer fields are set to
 * the same placeholder in scratch copies so that they cannot mask a real
 * difference by coalescing with one.
 */
static void
cmp_obj(const char *what, struct sgd *ours, struct sgd *ref, long input)
{
	static struct sgd a, b;
	long off_ours, off_ref;
	int i;

	/*
	 * det_at as an OFFSET IN SYMBOLS from each side's own history.  A
	 * null det_at (the constructor's value, and what a failed detection
	 * leaves) is reported as -1 rather than as an offset from the buffer
	 * base, so that "nothing detected" and "detected at symbol 0" are
	 * distinguishable.
	 */
	off_ours = ours->status.det_at == 0
		 ? -1L : (long)(ours->status.det_at - ours->hist);
	off_ref = ref->status.det_at == 0
		? -1L : (long)(ref->status.det_at - ref->hist);
	diff_eq_int("det_at offset, input %ld", off_ours, off_ref, input);

	/* The history contents, over the span create/control zero. */
	if (ours->hist != 0 && ref->hist != 0 && ours->hist_span == ref->hist_span) {
		for (i = 0; i < ref->hist_span; i++)
			diff_eq_int("hist[%ld]", ours->hist[i], ref->hist[i],
				    (long)i);
	}

	a = *ours;
	b = *ref;
	a.hist = 0;
	b.hist = 0;
	a.status.det_at = 0;
	b.status.det_at = 0;
	diff_eq_obj(what, struct sgd, &a, &b, input);
}

/*
 * The configurations driven.  `hist_extra >= ref_len` (note 2) and
 * `1 <= sym_bits <= 15` (note 4) hold for every one of them, and the
 * sequence/reference arrays are filled with byte-ranged symbols (note 1).
 */
#define MAXSEQ	24
#define MAXREF	12

/*
 * ONE SET OF ARRAYS, SHARED BY BOTH SIDES.  Both engines only READ the
 * sequence and reference tables, so handing them the same arrays makes
 * `cfg.gen.seq` and `cfg.det.ref` hold the same value on both sides and the
 * whole object comparable field by field.  Giving each side its own copy
 * would have put two legitimately different pointers inside the struct and
 * forced them to be excluded -- and an excluded field is a field no check
 * covers.
 */
static unsigned short seq_tab[MAXSEQ], ref_tab[MAXREF];

struct cfgspec {
	short sym_bits;
	short hist_len;
	unsigned short seq_len;
	int seq_enable;
	unsigned short idle_sym;
	unsigned short data_word;
	unsigned short word_syms;
	unsigned short ref_len;
	short ref_margin;
	int pat_match;
	int pat_mask;
	int pat_out_mask;
};

static const struct cfgspec specs[] = {
	/* bits hist seq en idle  data  wsym ref margin  match   mask   out  */
	{  8,  50,  1, 0, 0x0000, 0x0000, 1,  1, 0x2000, 0,      0,      0      },
	{  8,  50,  6, 1, 0x00aa, 0x1234, 2,  4, 0x2000, 0x0055, 0x00ff, 0x0fff },
	{  4,  16,  8, 0, 0x000f, 0xbeef, 4,  6, 0x0000, 0x000a, 0x000f, 0x00ff },
	{  1,  12,  3, 1, 0x0001, 0xa5a5, 8,  2, 0x3fff, 0x0001, 0x0003, 0x000f },
	{  8,  20, 12, 0, 0x0000, 0xffff, 2,  8, 0x1000, 0x00c1, 0x00ff, 0xffff },
	{ 15,  30,  4, 1, 0x007f, 0x5555, 1, 12, 0x2000, 0x0000, 0x0000, 0x0001 },
	{  2,  10, 24, 1, 0x0003, 0x0001, 8,  3, 0x3ff0, 0x0002, 0x0003, 0x0007 },
	{  8,  50,  1, 0, 0x0000, 0x0000, 1,  1, 0x2000, 0x0001, 0x0001, 0x0001 }
};
#define NSPEC	((int)(sizeof(specs) / sizeof(specs[0])))

static void
build_cfg(int k, struct sgd_cfg *c)
{
	const struct cfgspec *s = &specs[k];
	unsigned long save = seed;
	int i;

	memset(c, 0, sizeof(*c));

	seed = 777UL + (unsigned long)k;
	for (i = 0; i < MAXSEQ; i++)
		seq_tab[i] = rnd_sym();
	for (i = 0; i < MAXREF; i++)
		ref_tab[i] = rnd_sym();
	seed = save;

	c->sym_bits = s->sym_bits;
	c->hist_len = s->hist_len;
	/* note 2: the zeroed span needs ref_len-1 symbols past hist_len. */
	c->hist_extra = (short)s->ref_len;
	c->gen.seq = seq_tab;
	c->gen.seq_len = s->seq_len;
	c->gen.seq_enable = s->seq_enable;
	c->gen.idle_sym = s->idle_sym;
	c->gen.data_word = s->data_word;
	c->gen.word_syms = s->word_syms;
	c->det.ref = ref_tab;
	c->det.ref_len = s->ref_len;
	c->det.ref_margin = s->ref_margin;
	c->det.pat_match = s->pat_match;
	c->det.pat_mask = s->pat_mask;
	c->det.pat_out_mask = s->pat_out_mask;
}

/*
 * CONSTRUCTION, both paths.
 *
 * The fresh path is checked for the malloc SIZES as well as the contents:
 * the object's 0x5c and the history's 2*(hist_len+hist_extra)-2 are the two
 * numbers the constructor commits to, and `harness_alloc_reqsize` reports
 * what was asked for rather than what the allocator served (F1353).
 */
static int
run_create(void)
{
	struct sgd_cfg ca;
	int k;

	diff_begin("SGD_create");

	/*
	 * The blob's own default configuration, taken through the null arg.
	 *
	 * SGD_CFG's two table pointers are relocations against FPM_xor_table,
	 * so ours names OUR copy and the blob's names the blob's -- two
	 * legitimately different addresses.  They are checked for what they
	 * POINT AT rather than compared, and every other field of the config
	 * is compared one at a time, so that a wrong constant in
	 * src/fax/sgd.c's SGD_CFG cannot hide behind the pointer exclusion.
	 */
	{
		struct sgd *x, *y;

		harness_alloc_reset();
		y = ref_SGD_create(0, 0);
		x = SGD_create(0, 0);

		diff_eq_int("default cfg: object size",
			    harness_alloc_reqsize(x),
			    harness_alloc_reqsize(y), 0);
		diff_eq_int("default cfg: history size",
			    harness_alloc_reqsize(x->hist),
			    harness_alloc_reqsize(y->hist), 0);

		diff_eq_int("SGD_CFG.gen.seq is the popcount table",
			    x->cfg.gen.seq == (const unsigned short *)
					      FPM_xor_table,
			    y->cfg.gen.seq == (const unsigned short *)
					      ref_FPM_xor_table, 0);
		diff_eq_int("SGD_CFG.det.ref is the popcount table",
			    x->cfg.det.ref == (const unsigned short *)
					      FPM_xor_table,
			    y->cfg.det.ref == (const unsigned short *)
					      ref_FPM_xor_table, 0);

		diff_eq_int("SGD_CFG.sym_bits", x->cfg.sym_bits,
			    y->cfg.sym_bits, 0);
		diff_eq_int("SGD_CFG.hist_len", x->cfg.hist_len,
			    y->cfg.hist_len, 0);
		diff_eq_int("SGD_CFG.hist_extra", x->cfg.hist_extra,
			    y->cfg.hist_extra, 0);
		diff_eq_int("SGD_CFG.short_0006", x->cfg.short_0006,
			    y->cfg.short_0006, 0);
		diff_eq_int("SGD_CFG.gen.seq_len", x->cfg.gen.seq_len,
			    y->cfg.gen.seq_len, 0);
		diff_eq_int("SGD_CFG.gen.short_0006", x->cfg.gen.short_0006,
			    y->cfg.gen.short_0006, 0);
		diff_eq_int("SGD_CFG.gen.seq_enable", x->cfg.gen.seq_enable,
			    y->cfg.gen.seq_enable, 0);
		diff_eq_int("SGD_CFG.gen.idle_sym", x->cfg.gen.idle_sym,
			    y->cfg.gen.idle_sym, 0);
		diff_eq_int("SGD_CFG.gen.short_000e", x->cfg.gen.short_000e,
			    y->cfg.gen.short_000e, 0);
		diff_eq_int("SGD_CFG.gen.data_word", x->cfg.gen.data_word,
			    y->cfg.gen.data_word, 0);
		diff_eq_int("SGD_CFG.gen.short_0012", x->cfg.gen.short_0012,
			    y->cfg.gen.short_0012, 0);
		diff_eq_int("SGD_CFG.gen.word_syms", x->cfg.gen.word_syms,
			    y->cfg.gen.word_syms, 0);
		diff_eq_int("SGD_CFG.gen.short_0016", x->cfg.gen.short_0016,
			    y->cfg.gen.short_0016, 0);
		diff_eq_int("SGD_CFG.det.ref_len", x->cfg.det.ref_len,
			    y->cfg.det.ref_len, 0);
		diff_eq_int("SGD_CFG.det.ref_margin", x->cfg.det.ref_margin,
			    y->cfg.det.ref_margin, 0);
		diff_eq_int("SGD_CFG.det.pat_match", x->cfg.det.pat_match,
			    y->cfg.det.pat_match, 0);
		diff_eq_int("SGD_CFG.det.pat_mask", x->cfg.det.pat_mask,
			    y->cfg.det.pat_mask, 0);
		diff_eq_int("SGD_CFG.det.pat_out_mask",
			    x->cfg.det.pat_out_mask,
			    y->cfg.det.pat_out_mask, 0);

		/* And everything the constructor DERIVED from it. */
		diff_eq_int("default cfg: word_left", x->word_left,
			    y->word_left, 0);
		diff_eq_int("default cfg: hist_span", x->hist_span,
			    y->hist_span, 0);
		diff_eq_int("default cfg: thresh", x->thresh, y->thresh, 0);
		diff_eq_int("default cfg: seq_pos", x->seq_pos, y->seq_pos, 0);
		diff_eq_int("default cfg: pat_sr", x->pat_sr, y->pat_sr, 0);

		SGD_delete(x);
		ref_SGD_delete(y);
	}

	for (k = 0; k < NSPEC; k++) {
		struct sgd *x, *y;
		static struct sgd sx, sy;
		static unsigned short hx[128], hy[128];

		build_cfg(k, &ca);

		harness_alloc_reset();
		y = ref_SGD_create(0, &ca);
		x = SGD_create(0, &ca);
		diff_eq_int("cfg %ld: object size", harness_alloc_reqsize(x),
			    harness_alloc_reqsize(y), (long)k);
		diff_eq_int("cfg %ld: history size",
			    harness_alloc_reqsize(x->hist),
			    harness_alloc_reqsize(y->hist), (long)k);
		cmp_obj("fresh object", x, y, (long)k);
		SGD_delete(x);
		ref_SGD_delete(y);

		/*
		 * THE CALLER-SUPPLIED PATH DOES NOT ALLOCATE THE HISTORY,
		 * and the constructor zeroes hist_span symbols through
		 * whatever `hist` already held.  Planting it is not optional
		 * and is not a nicety: an unplanted pointer here is a wild
		 * store, and D955/F8587's point is that both sides would make
		 * the same wild store and agree about it.
		 */
		memset(&sx, HARNESS_MALLOC_FILL, sizeof(sx));
		memset(&sy, HARNESS_MALLOC_FILL, sizeof(sy));
		memset(hx, 0x5a, sizeof(hx));
		memset(hy, 0x5a, sizeof(hy));
		sx.hist = hx;
		sy.hist = hy;
		y = ref_SGD_create(&sy, &ca);
		x = SGD_create(&sx, &ca);
		diff_eq_int("cfg %ld: reuse returns its argument",
			    x == &sx, y == &sy, (long)k);
		cmp_obj("reused object", x, y, (long)k);
		/*
		 * And the words PAST the zeroed span are untouched -- the
		 * anti-vacuity check for the zeroing loop's bound.  Without
		 * it a constructor that zeroed the whole buffer, or none of
		 * it, could still pass every check above.
		 */
		diff_eq_int("cfg %ld: word past hist_span untouched",
			    hx[sx.hist_span], hy[sy.hist_span], (long)k);
	}

	return diff_end();
}

/*
 * `SGD_control`, all four combinations of its two optional halves.
 */
static int
run_control(void)
{
	struct sgd_cfg ca;
	int k, which;

	diff_begin("SGD_control");

	for (k = 0; k < NSPEC; k++) {
		build_cfg(k, &ca);

		for (which = 0; which < 4; which++) {
			struct sgd *x, *y;
			struct sgd_control_req qa, qb;
			struct sgd_gen_cfg ga, gb;
			struct sgd_det_cfg da, db;
			int j = (k + 1) % NSPEC;
			struct sgd_cfg na;
			unsigned short out[8];

			y = ref_SGD_create(0, &ca);
			x = SGD_create(0, &ca);

			/*
			 * Move the object off its construction state first,
			 * so that a control that fails to reset something is
			 * visible.  A control applied to a fresh object
			 * cannot show that.
			 */
			ref_SGD_sequence_gen(y, out, 3);
			SGD_sequence_gen(x, out, 3);

			build_cfg(j, &na);
			ga = na.gen;
			gb = na.gen;
			da = na.det;
			db = na.det;
			/*
			 * hist_extra is NOT settable through control, so the
			 * new detector half must not ask for more reference
			 * symbols than the constructed buffer holds -- note 2
			 * again, in its control-path form.
			 */
			if (da.ref_len > (unsigned short)ca.hist_extra) {
				da.ref_len = (unsigned short)ca.hist_extra;
				db.ref_len = da.ref_len;
			}

			qa.gen = (which & 1) ? &ga : 0;
			qa.det = (which & 2) ? &da : 0;
			qb.gen = (which & 1) ? &gb : 0;
			qb.det = (which & 2) ? &db : 0;

			ref_SGD_control(y, &qb);
			SGD_control(x, &qa);
			cmp_obj("after control", x, y,
				(long)(k * 4 + which));

			SGD_delete(x);
			ref_SGD_delete(y);
		}
	}

	return diff_end();
}

/*
 * `SGD_symbol_gen` and `SGD_sequence_gen`, stepped.
 *
 * Stepped rather than called once because both carry state across calls --
 * `word_left` and `seq_pos`/`seq_reps` -- and a single call cannot see a
 * wrap, a restart, or the one-shot handover to the idle symbol.  F8790's
 * argument: a wrong constant in a state update is invisible to any one-block
 * fixture.
 */
static int
run_gen(void)
{
	struct sgd_cfg ca;
	int k, step;

	diff_begin("SGD_symbol_gen/SGD_sequence_gen");

	for (k = 0; k < NSPEC; k++) {
		struct sgd *x, *y;
		unsigned short oa[32], ob[32];
		int i;

		build_cfg(k, &ca);

		y = ref_SGD_create(0, &ca);
		x = SGD_create(0, &ca);
		for (step = 0; step < 24; step++) {
			short n = (short)(1 + (rnd() % 7));
			short ra, rb;

			memset(oa, 0x33, sizeof(oa));
			memset(ob, 0x33, sizeof(ob));
			rb = ref_SGD_symbol_gen(y, ob, n);
			ra = SGD_symbol_gen(x, oa, n);
			diff_eq_int("symbol_gen return, step %ld", ra, rb,
				    (long)step);
			for (i = 0; i < 32; i++)
				diff_eq_int("symbol_gen out[%ld]", oa[i],
					    ob[i], (long)i);
			cmp_obj("after symbol_gen", x, y,
				(long)(k * 100 + step));
		}
		SGD_delete(x);
		ref_SGD_delete(y);

		y = ref_SGD_create(0, &ca);
		x = SGD_create(0, &ca);
		for (step = 0; step < 24; step++) {
			short n = (short)(1 + (rnd() % 7));
			short ra, rb;

			memset(oa, 0x33, sizeof(oa));
			memset(ob, 0x33, sizeof(ob));
			rb = ref_SGD_sequence_gen(y, ob, n);
			ra = SGD_sequence_gen(x, oa, n);
			diff_eq_int("sequence_gen return, step %ld", ra, rb,
				    (long)step);
			for (i = 0; i < 32; i++)
				diff_eq_int("sequence_gen out[%ld]", oa[i],
					    ob[i], (long)i);
			cmp_obj("after sequence_gen", x, y,
				(long)(k * 100 + step));
		}
		SGD_delete(x);
		ref_SGD_delete(y);
	}

	return diff_end();
}

/*
 * `SGD_sequence_det`, stepped, over three kinds of input.
 *
 * n >= 1 ALWAYS: see D1060 and the note at the top.  n is also kept at or
 * below hist_len so that the sliding window's `hist_len - n` base stays
 * non-negative -- a negative base wraps through the unsigned short the
 * object truncates it to and indexes the history from 65535 downwards, which
 * is D1063 and is not what this test is measuring.
 */
static int
run_seqdet(void)
{
	struct sgd_cfg ca;
	int k, step;

	diff_begin("SGD_sequence_det");

	for (k = 0; k < NSPEC; k++) {
		struct sgd *x, *y;
		unsigned short sym[16];
		int mode;

		for (mode = 0; mode < 3; mode++) {
			build_cfg(k, &ca);
			y = ref_SGD_create(0, &ca);
			x = SGD_create(0, &ca);

			for (step = 0; step < 20; step++) {
				short n;
				short ra, rb;
				int i;

				n = (short)(1 + (rnd() % 6));
				if (n > ca.hist_len)
					n = ca.hist_len;

				for (i = 0; i < 16; i++) {
					if (mode == 0)
						sym[i] = rnd_sym();
					else if (mode == 1)
						/*
						 * The reference sequence
						 * itself, so the detector
						 * has something to find and
						 * the accept arm -- and
						 * det_at -- is exercised.
						 */
						sym[i] = ref_tab[i % ca.det.ref_len];
					else
						sym[i] = 0;
				}

				rb = ref_SGD_sequence_det(y, sym, n);
				ra = SGD_sequence_det(x, sym, n);
				diff_eq_int("sequence_det return, step %ld",
					    ra, rb, (long)step);
				cmp_obj("after sequence_det", x, y,
					(long)(k * 1000 + mode * 100 + step));
			}
			SGD_delete(x);
			ref_SGD_delete(y);
		}
	}

	return diff_end();
}

/*
 * `SGD_pattern_det`, stepped.  Symbols are byte-ranged here too, though for
 * a different reason: this function has no table to run off, but the bit
 * loop walks sym_bits positions down from the top and a value wider than the
 * mask contributes nothing, so the interesting inputs are the ones the mask
 * can see.
 */
static int
run_patdet(void)
{
	struct sgd_cfg ca;
	int k, step;

	diff_begin("SGD_pattern_det");

	for (k = 0; k < NSPEC; k++) {
		struct sgd *x, *y;
		short sym[16];

		build_cfg(k, &ca);
		y = ref_SGD_create(0, &ca);
		x = SGD_create(0, &ca);

		for (step = 0; step < 40; step++) {
			short n = (short)(rnd() % 9);	/* 0 IS safe here */
			short ra, rb;
			int i;

			for (i = 0; i < 16; i++)
				sym[i] = (short)(rnd() & 0x7fffUL);

			rb = ref_SGD_pattern_det(y, sym, n);
			ra = SGD_pattern_det(x, sym, n);
			diff_eq_int("pattern_det return, step %ld", ra, rb,
				    (long)step);
			cmp_obj("after pattern_det", x, y,
				(long)(k * 100 + step));
		}
		SGD_delete(x);
		ref_SGD_delete(y);
	}

	return diff_end();
}

/*
 * `SGD_correlate`, which touches no object at all: four arguments in, one
 * short out.  p[0]*n is never zero (note 3).
 */
static int
run_correlate(void)
{
	static const short scales[] = { 1, -1, 2, -2, 3, 100, -100, 0x0100 };
	unsigned short a[16], b[16];
	int s, t, i;

	diff_begin("SGD_correlate");

	for (s = 0; s < (int)(sizeof(scales) / sizeof(scales[0])); s++) {
		for (t = 0; t < 24; t++) {
			short p = scales[s];
			short n = (short)(1 + (rnd() % 12));
			short ra, rb;

			for (i = 0; i < 16; i++) {
				a[i] = rnd_sym();
				/*
				 * Half the trials correlate a sequence
				 * against itself, so the zero-distance case
				 * is reached rather than left to chance.
				 */
				b[i] = (t & 1) ? a[i] : rnd_sym();
			}

			rb = ref_SGD_correlate(&p, a, b, n);
			ra = SGD_correlate(&p, a, b, n);
			diff_eq_int("correlate(p=%ld)", ra, rb, (long)p);
		}
	}

	return diff_end();
}

/*
 * `SGD_status`: the copy out AND the clear it leaves behind.
 *
 * Driven from an object that has been through the detectors, so that the
 * fields being copied are non-zero -- reading a status block out of a fresh
 * object compares six zeroes and proves nothing.  The out-parameter is
 * pre-filled with a pattern on both sides so that a field the copy FAILS to
 * write is visible as the pattern rather than as an accidental zero.
 */
static int
run_status(void)
{
	struct sgd_cfg ca;
	int k;

	diff_begin("SGD_status");

	for (k = 0; k < NSPEC; k++) {
		struct sgd *x, *y;
		struct sgd_status sa, sb;
		unsigned short sym[16];
		short psym[16];
		int i, step;
		long off_a, off_b;

		build_cfg(k, &ca);
		y = ref_SGD_create(0, &ca);
		x = SGD_create(0, &ca);

		for (step = 0; step < 6; step++) {
			for (i = 0; i < 16; i++) {
				sym[i] = ref_tab[i % ca.det.ref_len];
				psym[i] = (short)(rnd() & 0x7fffUL);
			}
			ref_SGD_sequence_det(y, sym, 4);
			SGD_sequence_det(x, sym, 4);
			ref_SGD_pattern_det(y, psym, 5);
			SGD_pattern_det(x, psym, 5);
			ref_SGD_sequence_gen(y, sym, 5);
			SGD_sequence_gen(x, sym, 5);
		}

		memset(&sa, 0x77, sizeof(sa));
		memset(&sb, 0x77, sizeof(sb));
		ref_SGD_status(y, &sb);
		SGD_status(x, &sa);

		/*
		 * det_at, again as an offset -- it is the field that carries
		 * out of the object through this copy, and the reason this
		 * test exists.
		 */
		off_a = sa.det_at == 0 ? -1L : (long)(sa.det_at - x->hist);
		off_b = sb.det_at == 0 ? -1L : (long)(sb.det_at - y->hist);
		diff_eq_int("status det_at offset, cfg %ld", off_a, off_b,
			    (long)k);

		sa.det_at = 0;
		sb.det_at = 0;
		diff_eq_obj("status copy", struct sgd_status, &sa, &sb,
			    (long)k);

		/* And the object the copy left behind. */
		cmp_obj("after status", x, y, (long)k);

		/* A second read must come back cleared, not stale. */
		memset(&sa, 0x77, sizeof(sa));
		memset(&sb, 0x77, sizeof(sb));
		ref_SGD_status(y, &sb);
		SGD_status(x, &sa);
		sa.det_at = 0;
		sb.det_at = 0;
		diff_eq_obj("second status copy", struct sgd_status, &sa, &sb,
			    (long)k);

		SGD_delete(x);
		ref_SGD_delete(y);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_create();
	rc |= run_control();
	rc |= run_gen();
	rc |= run_seqdet();
	rc |= run_patdet();
	rc |= run_correlate();
	rc |= run_status();
	return rc;
}
