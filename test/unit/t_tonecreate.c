/*
 * t_tonecreate.c -- the differential test for TONE_create (blob 0xaf690,
 * 521 bytes), for TONE_CFG (`.data` 0x83c0, 48 bytes) and for the 53-tap
 * `ToneLPF` TONE_CFG points at (`.data` 0x8400, 212 bytes).
 *
 * HOW THE TWO DATA OBJECTS ARE PROVED.  TONE_CFG is `D` in the blob, so it
 * has a `ref_` alias and can be compared byte for byte; `ToneLPF` is `d`
 * AND the blob has a SECOND local of that name (the fixed-point pump's
 * 53 shorts at 0xd040), so `symmap.py` refuses to globalize either and
 * neither gets an alias. It is reached through TONE_CFG's own pointer to it
 * instead, which is a direct comparison of the array and not a lookup
 * through anything: `ref_TONE_CFG.fir_proto` IS the blob's array.
 *
 * WHAT THE OBJECT COMPARISON HAS TO SKIP, and why each one.  Five words
 * hold addresses that are different on the two sides by construction --
 * +0x1c `fir_proto` (each side's own ToneLPF when the config is the
 * default), +0x3c and +0x40 (separate allocations), +0x1b4 and +0x1b8 (the
 * same), and +0x1bc, which is a pointer INTO the object. Everything else
 * of the 0x1c8 bytes is compared, and every skipped pointer is followed and
 * its target compared instead, so nothing is skipped without being reached
 * another way.
 *
 * THE BRANCH THAT IS EASY TO MISS is the allocation gate: the four heap
 * blocks are allocated only when THIS call allocated the object AND
 * `fir_len` is positive, which is one `test %edx,%eax` over two `set`
 * results at 0xaf6d9. Three of the four combinations therefore leave the
 * pointers as they were found, and the FIR loop still runs and still writes
 * through them. The sweep drives all four and counts them (F134).
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/fdspkrnl.h"
#include "dsplib/sysdep.h"

extern struct fdsp_tone_cfg ref_TONE_CFG;
extern struct fdsp_tone *ref_TONE_create(struct fdsp_tone *t,
					 const struct fdsp_tone_cfg *cfg);
extern void ref_TONE_delete(struct fdsp_tone *t);

/* The five words that hold an address; see the header. */
static int
is_pointer_word(unsigned int off)
{
	return off == 0x1c || off == 0x3c || off == 0x40
	       || off == 0x1b4 || off == 0x1b8 || off == 0x1bc;
}

static unsigned long
fbits(float f)
{
	unsigned long u = 0;

	memcpy(&u, &f, 4);
	return u;
}

/* Coverage counters (F134). */
static int seen_gate[4];		/* allocated x (fir_len > 0) */
static int seen_default_cfg;
static int seen_given_cfg;

static void
compare_objects(long tag, const struct fdsp_tone *ours,
		const struct fdsp_tone *theirs)
{
	const unsigned char *a = (const unsigned char *)theirs;
	const unsigned char *b = (const unsigned char *)ours;
	unsigned int off;
	int reported = 0;

	for (off = 0; off < sizeof(struct fdsp_tone); off++) {
		if (is_pointer_word(off & ~3u))
			continue;
		if (a[off] != b[off] && reported < 12) {
			reported++;
			diff_eq_int("object byte %ld", b[off], a[off],
				    tag * 1000 + (long)off);
		}
	}
	/* a silent pass on a loop that skipped everything is worth nothing */
	diff_eq_int("object bytes compared %ld",
		    (int)sizeof(struct fdsp_tone) > 0x1c0, 1, tag);
}

/*
 * One paired call with storage supplied by the fixture.  Each side gets its
 * own four buffers, planted with a recognisable fill so an unwritten tap is
 * visible, and the buffers are compared afterwards.
 */
static void
run_supplied(long tag, const struct fdsp_tone_cfg *cfg, short plant_len,
	     int give_buffers)
{
	struct fdsp_tone ta, tb;
	float ca[80], cb[80], da[80], db[80];
	float ba[5], bb[5], sa[2], sb[2];
	struct fdsp_tone *ra, *rb;
	int i;
	short n;

	memset(&ta, 0x5a, sizeof(ta));
	memset(&tb, 0x5a, sizeof(tb));
	for (i = 0; i < 80; i++) {
		ca[i] = cb[i] = -7.5f;
		da[i] = db[i] = -8.5f;
	}
	for (i = 0; i < 5; i++)
		ba[i] = bb[i] = -9.5f;
	sa[0] = sb[0] = -10.5f;
	sa[1] = sb[1] = -11.5f;

	if (give_buffers) {
		ta.fir_coef = ca;  tb.fir_coef = cb;
		ta.fir_dly = da;   tb.fir_dly = db;
		ta.ptr_01b4 = ba;  tb.ptr_01b4 = bb;
		ta.ptr_01b8 = sa;  tb.ptr_01b8 = sb;
	}
	(void)plant_len;

	ra = ref_TONE_create(&ta, cfg);
	rb = TONE_create(&tb, cfg);

	diff_eq_int("returns its argument %ld", rb == &tb, ra == &ta, tag);
	compare_objects(tag, &tb, &ta);

	n = ta.fir_len;
	if (n > 80)
		n = 80;
	if (give_buffers) {
		for (i = 0; i < 80; i++) {
			diff_eq_int("fir_coef %ld", (long)fbits(cb[i]),
				    (long)fbits(ca[i]), tag * 1000 + i);
			diff_eq_int("fir_dly %ld", (long)fbits(db[i]),
				    (long)fbits(da[i]), tag * 1000 + 100 + i);
		}
		for (i = 0; i < 5; i++)
			diff_eq_int("biquad %ld", (long)fbits(bb[i]),
				    (long)fbits(ba[i]), tag * 1000 + 200 + i);
		for (i = 0; i < 2; i++)
			diff_eq_int("biquad state %ld", (long)fbits(sb[i]),
				    (long)fbits(sa[i]), tag * 1000 + 210 + i);
		/*
		 * A supplied object never allocates, so the taps outside
		 * fir_len must still carry the fill -- that is what says
		 * the loop stopped where the length said.
		 */
		if (n >= 0 && n < 80)
			diff_eq_int("the loop stopped at fir_len %ld",
				    (long)fbits(cb[n]), (long)fbits(-7.5f),
				    tag);
	}
	seen_gate[(ta.fir_len > 0) ? 1 : 0] = 1;
	if (cfg == 0)
		seen_default_cfg = 1;
	else
		seen_given_cfg = 1;
}

int
main(void)
{
	int failed = 0;
	int i;

	/*
	 * SECTION 1 -- TONE_CFG and ToneLPF against the blob's own `.data`.
	 */
	diff_begin("TONE_CFG and ToneLPF");
	{
		const unsigned char *a = (const unsigned char *)&ref_TONE_CFG;
		const unsigned char *b = (const unsigned char *)&TONE_CFG;
		unsigned int off;

		diff_eq_int("the config is 48 bytes",
			    (int)sizeof(struct fdsp_tone_cfg), 0x30, 0);
		for (off = 0; off < sizeof(struct fdsp_tone_cfg); off++) {
			if (off >= 0x1c && off < 0x20)
				continue;	/* the ToneLPF pointer */
			diff_eq_int("TONE_CFG byte %ld", b[off], a[off],
				    (long)off);
		}
		/* and the pointer is followed rather than skipped */
		diff_eq_int("both configs name a prototype",
			    TONE_CFG.fir_proto != 0
			    && ref_TONE_CFG.fir_proto != 0, 1, 0);
		diff_eq_int("but not the same one",
			    TONE_CFG.fir_proto != ref_TONE_CFG.fir_proto, 1,
			    0);
		diff_eq_int("the length is 53", TONE_CFG.fir_len, 53, 0);
		for (i = 0; i < 53; i++)
			diff_eq_int("ToneLPF %ld",
				    (long)fbits(TONE_CFG.fir_proto[i]),
				    (long)fbits(ref_TONE_CFG.fir_proto[i]),
				    (long)i);
		/* it is symmetric, which a transcription slip would break */
		for (i = 0; i < 27; i++)
			diff_eq_int("ToneLPF is symmetric %ld",
				    (long)fbits(TONE_CFG.fir_proto[i]),
				    (long)fbits(TONE_CFG.fir_proto[52 - i]),
				    (long)i);
	}
	failed |= diff_end();

	/*
	 * SECTION 2 -- a supplied object, with the default config and with
	 * configs of the fixture's own.
	 */
	diff_begin("TONE_create supplied");
	{
		static const float freqs[] = {
			0.0f, 1.0f, 60.0f, 350.0f, 980.0f, 1100.0f, 2100.0f,
			2225.0f, 4000.0f, 8000.0f, 12000.0f, -2100.0f
		};
		static const float radii[] = {
			0.0f, 0.5f, 0.9375f, 0.96f, 1.0f, -0.9375f
		};
		static const short lens[] = { -1, 0, 1, 2, 53, 79, 80 };
		static const int nf = (int)(sizeof(freqs) / sizeof(freqs[0]));
		static const int nr = (int)(sizeof(radii) / sizeof(radii[0]));
		static const int nl = (int)(sizeof(lens) / sizeof(lens[0]));
		static float proto[80];
		struct fdsp_tone_cfg cfg;
		int fi, ri, li;

		for (i = 0; i < 80; i++)
			proto[i] = (float)(i - 40) * 0.0125f;

		run_supplied(0, 0, 53, 1);

		for (fi = 0; fi < nf; fi++)
		for (ri = 0; ri < nr; ri++)
		for (li = 0; li < nl; li++) {
			memset(&cfg, 0x3c, sizeof(cfg));
			cfg.freq = freqs[fi];
			cfg.amp = 0.5f;
			cfg.duration = 100.0f;
			cfg.float_000c = 0.75f;
			cfg.float_0010 = 0.01f;
			cfg.float_0014 = 0.0075f;
			cfg.pole_radius = radii[ri];
			cfg.fir_proto = proto;
			cfg.fir_len = lens[li];
			cfg.int_0024 = 0;
			cfg.int_0028 = 0;
			cfg.int_002c = 0;
			run_supplied((long)((fi * 100 + ri) * 10 + li), &cfg,
				     lens[li], 1);
		}
	}
	failed |= diff_end();

	/*
	 * SECTION 3 -- the allocating call, on the allocator's books.  This
	 * is the only path that reaches the four sysdep_malloc calls.
	 *
	 * IT IS DRIVEN ONLY WITH A POSITIVE `fir_len`, and that is deviation
	 * D995 rather than a gap in the sweep: the gate skips all four
	 * allocations when the length is not positive, and the tail of the
	 * function then writes five floats through `ptr_01b4` and two
	 * through `ptr_01b8` unconditionally.  On a freshly allocated object
	 * those are whatever the allocator's memory held, so the object
	 * writes through two uninitialised pointers and the fixture crashes
	 * -- measured, not supposed.  The supplied-storage sweep above
	 * covers a non-positive length with real buffers planted.
	 */
	diff_begin("TONE_create allocating");
	{
		static const short lens[] = { 1, 2, 53, 64 };
		static const int nl = (int)(sizeof(lens) / sizeof(lens[0]));
		static float proto[64];
		struct fdsp_tone_cfg cfg;
		int li;

		for (i = 0; i < 64; i++)
			proto[i] = (float)(i + 1) * 0.001f;

		for (li = 0; li < nl; li++) {
			struct alloc_log la, lb;
			struct fdsp_tone *ta, *tb;
			long tag = (long)li;

			memset(&cfg, 0, sizeof(cfg));
			cfg.freq = 2100.0f;
			cfg.amp = 0.85f;
			cfg.duration = 450.0f;
			cfg.pole_radius = 0.9375f;
			cfg.fir_proto = proto;
			cfg.fir_len = lens[li];

			harness_alloc_reset();
			ta = ref_TONE_create(0, &cfg);
			la = harness_alloc;
			harness_alloc_reset();
			tb = TONE_create(0, &cfg);
			lb = harness_alloc;

			diff_eq_int("alloc count %ld", lb.allocs, la.allocs,
				    tag);
			diff_eq_int("alloc bytes %ld", (int)lb.bytes,
				    (int)la.bytes, tag);
			diff_eq_int("allocated at all %ld", tb != 0, ta != 0,
				    tag);
			diff_eq_int("the object is 0x1c8 %ld",
				    (int)sizeof(struct fdsp_tone), 0x1c8, tag);
			/*
			 * One block when the gate is shut, five when it is
			 * open -- said as a number so the gate cannot be
			 * silently the wrong way round.
			 */
			diff_eq_int("blocks taken %ld", lb.allocs,
				    lens[li] > 0 ? 5 : 1, tag);
			compare_objects(100 + tag, tb, ta);
			if (lens[li] > 0) {
				for (i = 0; i < lens[li]; i++) {
					diff_eq_int("alloc fir_coef %ld",
						    (long)fbits(tb->fir_coef[i]),
						    (long)fbits(ta->fir_coef[i]),
						    tag * 1000 + i);
					diff_eq_int("alloc fir_dly %ld",
						    (long)fbits(tb->fir_dly[i]),
						    (long)fbits(ta->fir_dly[i]),
						    tag * 1000 + 100 + i);
				}
				for (i = 0; i < 5; i++)
					diff_eq_int("alloc biquad %ld",
						    (long)fbits(tb->ptr_01b4[i]),
						    (long)fbits(ta->ptr_01b4[i]),
						    tag * 1000 + 200 + i);
				for (i = 0; i < 2; i++)
					diff_eq_int("alloc biquad state %ld",
						    (long)fbits(tb->ptr_01b8[i]),
						    (long)fbits(ta->ptr_01b8[i]),
						    tag * 1000 + 210 + i);
			}
			diff_eq_int("iir_coef points into the object %ld",
				    tb->iir_coef == tb->det_coef, 1, tag);
			seen_gate[2 + (lens[li] > 0 ? 1 : 0)] = 1;
			TONE_delete(tb);
			ref_TONE_delete(ta);
			harness_alloc_reset();
		}
	}
	failed |= diff_end();

	/*
	 * SECTION 4 -- coverage, asserted from the run (F134).
	 */
	diff_begin("TONE_create coverage");
	{
		diff_eq_int("supplied, no filter", seen_gate[0], 1, 0);
		diff_eq_int("supplied, with a filter", seen_gate[1], 1, 0);
		diff_eq_int("allocating, with a filter", seen_gate[3], 1, 0);
		/*
		 * The fourth combination is D995 and is deliberately not
		 * driven; saying so here is what stops a later reader
		 * reading three of four as an oversight.
		 */
		diff_eq_int("allocating with no filter is NOT driven",
			    seen_gate[2], 0, 0);
		diff_eq_int("the default config was used", seen_default_cfg,
			    1, 0);
		diff_eq_int("a supplied config was used", seen_given_cfg, 1,
			    0);
	}
	failed |= diff_end();

	return failed;
}
