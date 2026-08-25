/*
 * t_fpm_pps.c -- the generic transmit pulse shaper, against the blob's.
 *
 * THE TWO SIDES NOW BUILD THEIR OWN STATE.  This file used to run the
 * REFERENCE init on both, because ours did not exist; now `ours` is built by
 * `FPM_PPS_init` and `refs` by the blob's, which is what makes every filter
 * trial below also a test of init -- the filter is driven from a state our own
 * code laid down, buffers included, rather than from one handed to it.
 *
 * `FPM_PPS_free` and init's REUSE PATH are invisible to a whole-object
 * comparison -- every scalar is reset either way and a freed pointer is not
 * readable -- so they are measured with the allocator's books instead:
 * `harness_alloc.allocs`, `.frees`, `.live`, `.free_null` and `.bad_free`.
 *
 * The configuration is built here.  `FPM_PPS_CFG`'s four pointers are null and
 * a caller patches them, so the built-in on its own would fault on the first
 * symbol; what it supplies is the geometry -- ten phases, a step of three, 120
 * coefficients, hence twelve taps -- and that is what is used.
 *
 * BOTH SYMBOL SOURCES ARE DRIVEN.  `cfg.mapped` set is the only form V.22 can
 * reach; clear is the direct form, which reads the ring's own `i` and `q`
 * arrays -- the two fields this batch named out of `fpm_smc_ring::pad00`.  A
 * suite that drove only the mapped form would leave the naming untested.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"

extern void ref_FPM_PPS_init(void *state, const void *cfg, int fresh);
extern void ref_FPM_PPS_free(void *state);
extern unsigned short ref_FPM_PPS_filter(void *state, void *src, short *out,
					 unsigned short count);

#define PPS_PHASES	10
#define PPS_COEFFS	120
#define PPS_TAPS	(PPS_COEFFS / PPS_PHASES)
#define RING_LEN	16
#define MAP_LEN		256

/*
 * The coefficient arrays are LONGER than the configured count.  The reuse
 * block below drives a thirteen-tap configuration, whose rail walk reaches
 * `phase + (taps - 1) * phases` = 129, and a table sized at the configured 120
 * would be read off the end -- by both sides, so the differential comparison
 * would agree and say nothing.  Only `cfg.coeffs` is ever raised above 120;
 * every trial before that one is unaffected, since a twelve-tap walk stops at
 * 119 either way.
 */
#define PPS_COEFF_MAX	(PPS_COEFFS + 20)

static short coeff_i[PPS_COEFF_MAX], coeff_q[PPS_COEFF_MAX];
static short imap[MAP_LEN], qmap[MAP_LEN];
static short ring_sym[RING_LEN], ring_i[RING_LEN], ring_q[RING_LEN];

/* The two history pointers, which cannot agree between two separate states. */
static int
is_buffer_slot(int off)
{
	return off == 0x30 || off == 0x34;
}

static void
compare_state(const struct fpm_pps *ours, const struct fpm_pps *ref, int trial)
{
	const unsigned char *a = (const unsigned char *)ours;
	const unsigned char *b = (const unsigned char *)ref;
	int i;

	for (i = 0; i < (int)sizeof(struct fpm_pps); i += 2) {
		if (is_buffer_slot(i) || is_buffer_slot(i - 2))
			continue;
		diff_eq_int("state +0x%02lx", *(const short *)(a + i),
			    *(const short *)(b + i), i);
	}
	for (i = 0; i < PPS_TAPS; i++) {
		diff_eq_int("hist_i[%ld]", ours->hist_i[i], ref->hist_i[i], i);
		diff_eq_int("hist_q[%ld]", ours->hist_q[i], ref->hist_q[i], i);
	}
	(void)trial;
}

static void
compare_ring(const struct fpm_smc_ring *a, const struct fpm_smc_ring *b,
	     int trial)
{
	diff_eq_int("ring ridx (%ld)", a->ridx, b->ridx, trial);
	diff_eq_int("ring widx (%ld)", a->widx, b->widx, trial);
	diff_eq_int("ring len (%ld)", a->len, b->len, trial);
}

static void
make_cfg(struct fpm_pps_cfg *c, int mapped, short step, unsigned short adj,
	 int scale)
{
	memset(c, 0, sizeof(*c));
	c->phases = PPS_PHASES;
	c->step = step;
	c->mapped = mapped;
	c->scale = scale;
	c->step_adj = adj;
	c->imap = imap;
	c->qmap = qmap;
	c->coeff_i = coeff_i;
	c->coeff_q = coeff_q;
	c->coeffs = PPS_COEFFS;
}

static void
make_ring(struct fpm_smc_ring *r, int seed)
{
	int i;

	for (i = 0; i < RING_LEN; i++) {
		ring_sym[i] = (short)(((i * 37 + seed) & 0xff)
				      | ((i & 3) << 12));
		ring_i[i] = (short)((i * 2731 + seed * 13) % 20001 - 10000);
		ring_q[i] = (short)((i * 4177 + seed * 29) % 20001 - 10000);
	}
	memset(r, 0, sizeof(*r));
	r->i = ring_i;
	r->q = ring_q;
	r->sym = ring_sym;
	r->widx = 0;
	r->ridx = (short)(seed % RING_LEN);
	r->len = RING_LEN;
}

int
main(void)
{
	static struct fpm_pps ours, refs;
	static short oa[8192], ob[8192];
	struct fpm_smc_ring ra, rb;
	struct fpm_pps_cfg cfg;
	int rc = 0;
	int i, k, mapped, trial;
	int nonzero = 0, varied = 0, wrapped = 0, consumed = 0, direct = 0;

	/*
	 * A prototype with no repeats anywhere: every coefficient distinct in
	 * magnitude, so a wrong stride, a wrong phase or a transposed rail
	 * lands on a different number rather than the same one.  V.32's SRE
	 * tables taught this the hard way -- see finding F3574.
	 */
	for (i = 0; i < PPS_COEFF_MAX; i++) {
		coeff_i[i] = (short)((i * 271 + 13) % 4001 - 2000);
		coeff_q[i] = (short)((i * 397 + 101) % 4001 - 2000);
	}
	for (i = 0; i < MAP_LEN; i++) {
		imap[i] = (short)((i * 173) % 8001 - 4000);
		qmap[i] = (short)((i * 311) % 8001 - 4000);
	}

	/*
	 * init on a never-initialised object.  Both sides allocate, and the
	 * two buffers are compared BY CONTENT because their addresses cannot
	 * agree -- with the request SIZES compared as well, which is what says
	 * the histories are `taps` entries and not V.22's `2 * taps`.
	 */
	diff_begin("FPM_PPS_init fresh");
	{
		int aa, ab;

		make_cfg(&cfg, 1, 3, 0, 32767);
		memset(&ours, 0, sizeof(ours));
		memset(&refs, 0, sizeof(refs));

		aa = harness_alloc.allocs;
		ref_FPM_PPS_init(&refs, &cfg, 1);
		aa = harness_alloc.allocs - aa;

		ab = harness_alloc.allocs;
		FPM_PPS_init(&ours, &cfg, 1);
		ab = harness_alloc.allocs - ab;

		compare_state(&ours, &refs, 0);
		diff_eq_int("allocations (%ld)", ab, aa, 0);
		diff_eq_int("hist_i bytes (%ld)",
			    (int)harness_alloc_reqsize(ours.hist_i),
			    (int)harness_alloc_reqsize(refs.hist_i), 0);
		diff_eq_int("hist_q bytes (%ld)",
			    (int)harness_alloc_reqsize(ours.hist_q),
			    (int)harness_alloc_reqsize(refs.hist_q), 0);

		/*
		 * Anti-vacuity, each against a number derived outside both
		 * implementations: two buffers of `taps` shorts, a tap count
		 * that is the quotient, a phase seeded from the STEP rather
		 * than from zero, and a debt of zero.
		 */
		diff_eq_int("two buffers were allocated (%ld)", aa, 2, 0);
		diff_eq_int("each is taps shorts (%ld)",
			    (int)harness_alloc_reqsize(refs.hist_i),
			    2 * PPS_TAPS, 0);
		diff_eq_int("... and both the same (%ld)",
			    (int)harness_alloc_reqsize(refs.hist_q),
			    2 * PPS_TAPS, 0);
		diff_eq_int("taps is coeffs / phases (%ld)", refs.taps,
			    PPS_TAPS, 0);
		diff_eq_int("phase is seeded from step (%ld)", refs.phase, 3, 0);
		diff_eq_int("need starts at zero (%ld)", refs.need, 0, 0);
		diff_eq_int("widx starts at zero (%ld)", refs.widx, 0, 0);
		diff_eq_int("the histories are cleared (%ld)",
			    refs.hist_i[PPS_TAPS - 1] == 0
			    && refs.hist_q[PPS_TAPS - 1] == 0, 1, 0);
		diff_eq_int("the configuration was copied (%ld)",
			    refs.cfg.coeffs, PPS_COEFFS, 0);
	}
	rc |= diff_end();

	/*
	 * Both symbol sources, three phase steps, three gains.  A step that
	 * does not divide the phase count is deliberate: the wrap then lands
	 * on a different phase every symbol, which is the case an even ratio
	 * hides.
	 */
	for (mapped = 0; mapped < 2; mapped++) {
		static const short steps[] = { 3, 1, 7 };
		static const unsigned short adjs[] = { 0, 1, 0 };
		static const int scales[] = { 32767, 16384, 4096 };
		int s;

		for (s = 0; s < 3; s++) {
			char tag[64];

			make_cfg(&cfg, mapped, steps[s], adjs[s], scales[s]);
			memset(&ours, 0, sizeof(ours));
			memset(&refs, 0, sizeof(refs));
			ref_FPM_PPS_init(&refs, &cfg, 1);
			FPM_PPS_init(&ours, &cfg, 1);

			sprintf(tag, "FPM_PPS_filter %s step %d",
				mapped ? "mapped" : "direct", steps[s]);
			diff_begin(tag);

			for (k = 0; k < 48; k++) {
				unsigned short n = (unsigned short)((k % 7) + 1);
				unsigned short pa, pb;

				make_ring(&ra, k);
				make_ring(&rb, k);
				memset(oa, 0x5a, sizeof(oa));
				memset(ob, 0x5a, sizeof(ob));

				pa = ref_FPM_PPS_filter(&refs, &ra, oa, n);
				pb = FPM_PPS_filter(&ours, &rb, ob, n);

				diff_eq_int("produced (%ld)", pb, pa, k);
				for (i = 0; i < 256; i++) {
					diff_eq_int("out[%ld]", ob[i], oa[i],
						    i);
					if (i < pa && oa[i] != 0)
						nonzero++;
					if (i < pa && i > 0
					    && oa[i] != oa[i - 1])
						varied++;
				}
				compare_state(&ours, &refs, k);
				compare_ring(&rb, &ra, k);

				if (refs.need != 0)
					wrapped++;
				if (ra.ridx != (short)(k % RING_LEN))
					consumed++;
				if (!mapped && pa > 0)
					direct++;
			}
			rc |= diff_end();
		}
	}

	/*
	 * A zero count must produce nothing and leave everything alone, and it
	 * is the one call that can distinguish an unsigned `count` from a
	 * signed one: `while (count != 0)` on an unsigned zero returns at
	 * once, and the object's `movzwl` says unsigned.
	 */
	diff_begin("FPM_PPS_filter zero count");
	{
		struct fpm_pps za, zb;

		make_cfg(&cfg, 1, 3, 0, 32767);
		memset(&ours, 0, sizeof(ours));
		memset(&refs, 0, sizeof(refs));
		ref_FPM_PPS_init(&refs, &cfg, 1);
		FPM_PPS_init(&ours, &cfg, 1);
		make_ring(&ra, 5);
		make_ring(&rb, 5);
		/* Run a little first, so there is state worth preserving. */
		(void)ref_FPM_PPS_filter(&refs, &ra, oa, 4);
		(void)FPM_PPS_filter(&ours, &rb, ob, 4);
		za = refs;
		zb = ours;

		diff_eq_int("produced (%ld)",
			    FPM_PPS_filter(&ours, &rb, ob, 0),
			    ref_FPM_PPS_filter(&refs, &ra, oa, 0), 0);
		compare_state(&ours, &refs, 0);
		compare_ring(&rb, &ra, 0);
		diff_eq_int("reference unchanged (%ld)",
			    memcmp(&za, &refs, sizeof(za)) == 0, 1, 0);
		diff_eq_int("ours unchanged (%ld)",
			    memcmp(&zb, &ours, sizeof(zb)) == 0, 1, 0);
	}
	rc |= diff_end();

	/*
	 * A state seeded mid-history, so the dot product's SECOND run has
	 * something in it.  With `widx` at zero the first run is one term and
	 * the second is all the rest; with `widx` at taps-1 the second run is
	 * empty.  Both ends, and everything between, on both sides.
	 */
	diff_begin("FPM_PPS_filter seeded widx");
	{
		/*
		 * FULL-SCALE coefficients and history for this block, and a
		 * seeded debt of up to two.
		 *
		 * Both are there to separate a claim the gentler blocks
		 * cannot.  The rails are truncated to sixteen bits SEPARATELY
		 * and their difference is truncated AGAIN before the output
		 * gain; with rails of a few thousand the second truncation
		 * never bites, and only rails near full scale in opposite
		 * directions make it observable.  And `count` is decremented
		 * by `need` rather than by one -- which are the same thing
		 * unless a caller seeds a debt above one, since nothing in the
		 * block ever sets `need` to more than 1.
		 */
		for (i = 0; i < PPS_COEFF_MAX; i++) {
			coeff_i[i] = (short)((i * 6421 + 977) % 65536 - 32768);
			coeff_q[i] = (short)((i * 5237 + 311) % 65536 - 32768);
		}
		make_cfg(&cfg, 1, 3, 0, 32767);
		for (trial = 0; trial < PPS_TAPS * 16; trial++) {
			unsigned short pa, pb;

			memset(&ours, 0, sizeof(ours));
			memset(&refs, 0, sizeof(refs));
			ref_FPM_PPS_init(&refs, &cfg, 1);
			FPM_PPS_init(&ours, &cfg, 1);
			make_ring(&ra, trial);
			make_ring(&rb, trial);

			refs.widx = ours.widx = (short)(trial % PPS_TAPS);
			refs.phase = ours.phase =
				(short)((trial * 3) % PPS_PHASES);
			refs.need = ours.need = (short)(trial % 3);
			for (i = 0; i < PPS_TAPS; i++) {
				refs.hist_i[i] = ours.hist_i[i] = (short)
					((i * 9973 + trial * 4441) % 65536
					 - 32768);
				refs.hist_q[i] = ours.hist_q[i] = (short)
					((i * 8117 + trial * 2731) % 65536
					 - 32768);
			}

			memset(oa, 0x5a, sizeof(oa));
			memset(ob, 0x5a, sizeof(ob));
			pa = ref_FPM_PPS_filter(&refs, &ra, oa, 5);
			pb = FPM_PPS_filter(&ours, &rb, ob, 5);
			diff_eq_int("seeded produced (%ld)", pb, pa, trial);
			for (i = 0; i < 128; i++)
				diff_eq_int("seeded out[%ld]", ob[i], oa[i], i);
			compare_state(&ours, &refs, trial);
			compare_ring(&rb, &ra, trial);
		}
	}
	rc |= diff_end();

	/*
	 * Re-init.  `fresh` zero keeps the two buffers unless the tap count
	 * now asked for is LARGER than the one they were allocated with, and
	 * that branch is invisible to a whole-object comparison -- every scalar
	 * is reset either way and both histories are cleared either way -- so
	 * it is read out of the allocator's books instead.
	 *
	 * PASS 1 IS THE ONE THAT SEPARATES THIS FUNCTION FROM ITS SIBLING.  It
	 * raises `cfg.coeffs` from 120 to 125 with `phases` at ten, so the
	 * quotient stays at twelve and both buffers must be KEPT.
	 * `FPM_SRE_init` decides on `cfg.coeffs` itself and would reallocate
	 * here; this one decides on the quotient, which is exactly the claim
	 * deviation D400 rests on and which nothing before this drove.
	 *
	 * Pass 3 shrinks the count, so `taps` falls below what the buffers
	 * hold: still a reuse, and the clear loop must run to the NEW count.
	 */
	diff_begin("FPM_PPS_init reuse and realloc");
	{
		static const struct {
			short coeffs;
			int moved;	/* frees, and allocations, per side */
			short taps;
		} pass[] = {
			{ PPS_COEFFS,		0, PPS_TAPS	 },
			{ PPS_COEFFS + 5,	0, PPS_TAPS	 },
			{ PPS_COEFFS + 10,	2, PPS_TAPS + 1 },
			{ PPS_COEFFS - 20,	0, PPS_TAPS - 2 }
		};
		int p;

		make_cfg(&cfg, 1, 3, 0, 32767);
		memset(&ours, 0, sizeof(ours));
		memset(&refs, 0, sizeof(refs));
		ref_FPM_PPS_init(&refs, &cfg, 1);
		FPM_PPS_init(&ours, &cfg, 1);

		for (p = 0; p < (int)(sizeof(pass) / sizeof(pass[0])); p++) {
			struct fpm_pps_cfg c2 = cfg;
			int fa, fb, aa, ab;

			c2.coeffs = pass[p].coeffs;

			/*
			 * DIRTY THE STATE FIRST, and it is not decoration: a
			 * re-init handed a state straight out of init agrees
			 * with one that resets nothing at all, because every
			 * field it would reset is already at its reset value.
			 * Sixteen symbols moves `widx`, `phase` and `need` off
			 * zero and fills BOTH histories to their full length --
			 * which is also what lets the SHRINKING pass see a
			 * clear loop that runs one entry too far, since the
			 * entries above the new tap count have to survive it.
			 */
			make_ring(&ra, p + 1);
			make_ring(&rb, p + 1);
			(void)ref_FPM_PPS_filter(&refs, &ra, oa, 16);
			(void)FPM_PPS_filter(&ours, &rb, ob, 16);
			diff_eq_int("dirtied widx (%ld)", refs.widx != 0, 1,
				    refs.widx);
			diff_eq_int("both sides dirtied alike (%ld)",
				    ours.widx, refs.widx, p);

			fa = harness_alloc.frees;
			aa = harness_alloc.allocs;
			ref_FPM_PPS_init(&refs, &c2, 0);
			fa = harness_alloc.frees - fa;
			aa = harness_alloc.allocs - aa;

			fb = harness_alloc.frees;
			ab = harness_alloc.allocs;
			FPM_PPS_init(&ours, &c2, 0);
			fb = harness_alloc.frees - fb;
			ab = harness_alloc.allocs - ab;

			diff_eq_int("frees (%ld)", fb, fa, p);
			diff_eq_int("allocations (%ld)", ab, aa, p);
			/*
			 * Anti-vacuity, and the whole point of the four passes:
			 * a reconstruction that always reallocated would agree
			 * with the reference on every state word and differ
			 * only here.
			 */
			diff_eq_int("frees for this branch (%ld)", fa,
				    pass[p].moved, p);
			diff_eq_int("allocations for this branch (%ld)", aa,
				    pass[p].moved, p);
			diff_eq_int("taps for this branch (%ld)", refs.taps,
				    pass[p].taps, p);
			diff_eq_int("no bad frees (%ld)", harness_alloc.bad_free,
				    0, p);

			compare_state(&ours, &refs, p);
			for (i = 0; i < pass[p].taps; i++) {
				diff_eq_int("reinit hist_i[%ld]", ours.hist_i[i],
					    refs.hist_i[i], i);
				diff_eq_int("reinit hist_q[%ld]", ours.hist_q[i],
					    refs.hist_q[i], i);
			}
		}
	}
	rc |= diff_end();

	/*
	 * FPM_PPS_free.  Nothing it does is visible in the state -- it does not
	 * clear the two pointers -- so the whole of its behaviour is in the
	 * allocator: two releases, of the buffers this state owns and of
	 * nothing else.
	 *
	 * THE SECOND PASS FREES A ZEROED STATE, and that is what pins the
	 * NUMBER of calls rather than their effect.  `free_null` counts a
	 * release of NULL and `frees` does not, so a version that released one
	 * buffer and not the other agrees on `frees` in pass 0 -- both sides
	 * would have dropped live by one -- and disagrees here.
	 */
	diff_begin("FPM_PPS_free");
	{
		int pass;

		/*
		 * The books are reset first because every `fresh` init above
		 * leaks its two buffers and the harness's live set is 4096
		 * slots.  This file does not reach that today; `t_fpm_sre.c`
		 * does -- 8724 allocations, 8716 still live -- and its free
		 * block read every release as a `bad_free` until it reset.
		 * Nothing after this point frees anything allocated before it.
		 */
		harness_alloc_reset();

		for (pass = 0; pass < 2; pass++) {
			static struct fpm_pps fs, gs;
			int fa, fb, la, lb, na, nb;

			memset(&fs, 0, sizeof(fs));
			memset(&gs, 0, sizeof(gs));
			if (pass == 0) {
				make_cfg(&cfg, 1, 3, 0, 32767);
				ref_FPM_PPS_init(&fs, &cfg, 1);
				FPM_PPS_init(&gs, &cfg, 1);
			}

			la = harness_alloc.live;
			fa = harness_alloc.frees;
			na = harness_alloc.free_null;
			ref_FPM_PPS_free(&fs);
			fa = harness_alloc.frees - fa;
			la = la - harness_alloc.live;
			na = harness_alloc.free_null - na;

			lb = harness_alloc.live;
			fb = harness_alloc.frees;
			nb = harness_alloc.free_null;
			FPM_PPS_free(&gs);
			fb = harness_alloc.frees - fb;
			lb = lb - harness_alloc.live;
			nb = harness_alloc.free_null - nb;

			diff_eq_int("frees (%ld)", fb, fa, pass);
			diff_eq_int("live dropped by (%ld)", lb, la, pass);
			diff_eq_int("null frees (%ld)", nb, na, pass);
			diff_eq_int("frees for this pass (%ld)", fa,
				    pass == 0 ? 2 : 0, pass);
			diff_eq_int("null frees for this pass (%ld)", na,
				    pass == 0 ? 0 : 2, pass);
			diff_eq_int("no bad frees (%ld)", harness_alloc.bad_free,
				    0, pass);
		}
	}
	rc |= diff_end();

	/*
	 * Anti-vacuity.  Every one counts a COMPARED result -- an output
	 * sample, a state word, a ring cursor -- and none counts a path.
	 */
	diff_begin("FPM_PPS_filter coverage");
	diff_eq_int("outputs are non-zero (%ld)", nonzero > 0, 1, nonzero);
	diff_eq_int("outputs vary (%ld)", varied > 0, 1, varied);
	diff_eq_int("the phase wrapped (%ld)", wrapped > 0, 1, wrapped);
	diff_eq_int("symbols were consumed (%ld)", consumed > 0, 1, consumed);
	diff_eq_int("the direct source produced output (%ld)", direct > 0, 1,
		    direct);
	rc |= diff_end();

	return rc;
}
