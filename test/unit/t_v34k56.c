/*
 * t_v34k56.c -- differential test of `k56FlexPhase34`.
 *
 * One entry point and ten arms, eight of which a differential test can enter.
 * It writes scattered fields of the 44 KB V.34 object and reaches the
 * whole transmit chain through `txmit`, so the comparison is the whole object
 * byte for byte plus the two buffers that chain fills.
 *
 * POINTERS ARE THE ONE EXCEPTION, handled the way `t_v34ec.c` established and
 * `t_v34hshak.c` reuses: the two sides hold different addresses by
 * construction, so each pointer field is blanked in a COPY of each object and
 * the copies are what `diff_eq_obj` sees.  Blanking rather than skipping is
 * what keeps `diff_eq_obj` -- which cannot skip -- and with it the field
 * naming, the run coalescing and the one-check-per-object count.  What the
 * pointers select is compared by CONTENT instead, which is the thing that
 * matters.  `PTR` below is that list; `ptr_seen` asserts every entry really
 * did hold two different addresses, so the list cannot quietly go stale into
 * a set of blanked-out fields that were never pointers at all.
 *
 * ONE POINTER IS DELIBERATELY SHARED.  `pac18` is the `K56FlexFloModem` this
 * function hands to its two bit sources, and `t_v90leaves` has already
 * measured that all four members of that class write nothing at all through
 * the pointer they are given.  So both sides get the SAME buffer, there is
 * nothing to blank, and a store through it would show up as the two sides
 * disagreeing about a buffer only one of them could have written.
 *
 * THE FILL IS VARIED, NOT ZERO.  A zero fill makes every field the function
 * leaves alone compare equal for the wrong reason.  Both sides get the same
 * pseudo-random bytes, and only the pointer slots are zeroed afterwards --
 * a garbage pointer that `txmit` follows is a crash, not a test.
 *
 * ---------------------------------------------------------------------------
 * WHAT CANNOT BE REACHED, AND WHY IT IS STRUCTURAL.
 *
 * Two blocks -- the Ja completion arm and the MP completion arm -- run only
 * when `K56FlexFloModem::getK56FlexJaBits` or `::getK56FlexMpBits` returns
 * non-zero.  Both members are three bytes of `xor %eax,%eax; ret` in the
 * blob, and our reconstruction of them is the same, so BOTH SIDES of every
 * call return 0 and neither can enter either arm.  This is not a shortfall in
 * the sweep: no input to `k56FlexPhase34` can change it.
 *
 * The checks below therefore state the COMPLEMENT and not the reach: "the
 * data flag was never set" and "the receiver state never became 3 or 2 on
 * those arms".  A "this arm ran at least once" check for them would be
 * unsatisfiable arithmetic dressed as coverage, which is the shape that
 * failed loudly twice already in this tree.  Finding F281 measures the gap and
 * names the four mutations that go uncaught because of it.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"

extern int ref_k56FlexPhase34(void *obj);
extern void ref_txinit(void *obj);
extern void ref_V34InitializeImplementationSpecific(void *obj);
extern void ref_V34SetupModulator(void *m, short baud, short carrier,
				  short a, short b, short c);
extern const int ref_vect4[4];
extern const int ref_vect16[16];

#define OB_RECEIVER	0x264
#define OB_TXBITS	0x25d6
#define OB_TXSTATE	0x3596
#define OB_MODULATOR	0x1450

#define SHAPED_N	512
#define RING_N		64

/*
 * Every pointer field the fixture or the object under test can leave holding
 * two different addresses.  Derived from what the three setup calls install
 * plus what this test sets itself, not from watching a comparison fail.
 */
static const struct { unsigned lo, hi; } PTR[] = {
	{ 0x0268, 0x0270 },
	{ 0x2074, 0x2078 },
	{ 0x2220, 0x2228 },
	{ 0x35b0, 0x35b4 },
	{ 0x80b8, 0x80d8 },
	{ 0x9138, 0x9158 },
	{ OB_MODULATOR + 0x10, OB_MODULATOR + 0x18 },
	{ OB_MODULATOR + 0xc24, OB_MODULATOR + 0xc28 },
	{ OB_MODULATOR + 0xcb0, OB_MODULATOR + 0xcb4 }
};
/*
 * NOT IN THE LIST, and the check below is what says so: +0x20cc is
 * `prefilter.coeff`, which this fixture points at ONE const table for both
 * sides, and `OB_MODULATOR + 0xc7c` is the same eight bytes written a second
 * way.  Blanking a slot the two sides agree on would hide a real difference
 * there rather than tolerate an unavoidable one, so both are compared like
 * any other field.
 */
#define NPTR	((int)(sizeof(PTR) / sizeof(PTR[0])))

static int ptr_seen[NPTR];

static struct v34_object oa, ob, ca, cb, snap;
static short shp_a[SHAPED_N], shp_b[SHAPED_N];
static short snap_shp[SHAPED_N], snap_bra[RING_N];
static short bra[RING_N], brb[RING_N];
static unsigned char k56obj[64];

/* Per-block "this really ran" counters; see the header comment. */
static long n_ja, n_idle_state, n_s3, n_s3_wrap, n_s4_dibit, n_s4_quad;
static long n_s5_dibit, n_s5_quad;
static long n_ja_tail, n_mp_tail;	/* must stay 0: unreachable */

static long tag;

static void
fill_varied(void *p, size_t n, unsigned s)
{
	unsigned char *b = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++) {
		s = s * 1103515245u + 12345u;
		b[i] = (unsigned char)(s >> 16);
	}
}

static void
blank_pointers(struct v34_object *o)
{
	int i;

	for (i = 0; i < NPTR; i++)
		memset((char *)o + PTR[i].lo, 0, PTR[i].hi - PTR[i].lo);
}

/*
 * Two objects, identically seeded, differing only in which buffers they own.
 *
 * `state` goes into `k56flex_receiver`, `constel` into `f382`, `vidx` into
 * `vect_idx` and `txbits` into the word at +0x25d6.  `f25c2` carries the
 * scrambler generator in bit 0 and `txmit`'s echo feed in bit 9, so it is
 * swept over both.
 */
static void
setup(int dataflag, int state, short constel, short vidx, short txbits,
      short f25c2, unsigned seed)
{
	struct v34_receiver *ra, *rb;
	int i;

	memset(&oa, 0, sizeof(oa));
	memset(&ob, 0, sizeof(ob));
	fill_varied(&oa, sizeof(oa), seed);
	memcpy(&ob, &oa, sizeof(ob));
	blank_pointers(&oa);
	blank_pointers(&ob);
	/* The three the fixture owns outright, and the shared K56flex one. */
	oa.p3548 = ob.p3548 = 0;
	oa.pac3c = ob.pac3c = 0;
	oa.pac18 = ob.pac18 = k56obj;
	fill_varied(k56obj, sizeof(k56obj), seed ^ 0x5a5au);

	fill_varied(shp_a, sizeof(shp_a), seed + 1);
	memcpy(shp_b, shp_a, sizeof(shp_b));
	fill_varied(bra, sizeof(bra), seed + 2);
	memcpy(brb, bra, sizeof(brb));

	V34InitializeImplementationSpecific(&oa);
	ref_V34InitializeImplementationSpecific(&ob);
	txinit(&oa);
	ref_txinit(&ob);

	((struct v34_modulator *)((char *)&oa + OB_MODULATOR))->shaped = shp_a;
	((struct v34_modulator *)((char *)&ob + OB_MODULATOR))->shaped = shp_b;
	V34SetupModulator((struct v34_modulator *)((char *)&oa + OB_MODULATOR),
			  2400, 1600, 0, 0, 1);
	ref_V34SetupModulator((char *)&ob + OB_MODULATOR, 2400, 1600, 0, 0, 1);

	oa.prefilter.coeff = ob.prefilter.coeff = V34TimingPrefilterCoeff;
	oa.prefilter.shift = ob.prefilter.shift = 14;
	oa.f25d4 = ob.f25d4 = 0x4000;
	/*
	 * The bulk-delay ring, and the two cursors into it.  A varied fill
	 * leaves those cursors holding a pseudo-random int, and `txmit`
	 * indexes the ring with them unchecked -- so these three are seeded
	 * rather than filled, and that is the fill's boundary, not an
	 * exception to it.
	 */
	oa.bulk_ring = bra;  ob.bulk_ring = brb;
	oa.bulk_len  = ob.bulk_len = RING_N;
	oa.bulk_head = ob.bulk_head = 3;
	oa.bulk_tail = ob.bulk_tail = 17;

	/* A scrambler register that is neither zero nor all ones. */
	oa.f25cc = ob.f25cc = 0x2f6b3d51;
	oa.f25c6 = ob.f25c6 = 2;
	oa.f25c8 = ob.f25c8 = 1;
	oa.f25c0 = ob.f25c0 = 0x1234;
	oa.f25c2 = ob.f25c2 = f25c2;

	oa.k56flex_receiver = ob.k56flex_receiver = state;
	oa.f382 = ob.f382 = constel;
	oa.vect_idx = ob.vect_idx = vidx;
	*(short *)((char *)&oa + OB_TXBITS) = txbits;
	*(short *)((char *)&ob + OB_TXBITS) = txbits;
	*(short *)((char *)&oa + OB_TXSTATE) = 20;
	*(short *)((char *)&ob + OB_TXSTATE) = 20;

	ra = (struct v34_receiver *)((char *)&oa + OB_RECEIVER);
	rb = (struct v34_receiver *)((char *)&ob + OB_RECEIVER);
	ra->flags = rb->flags = (unsigned short)
		(0x0123 | (dataflag ? V34_RX_FLAG_DATA : 0));
	if (!dataflag) {
		ra->flags = (unsigned short)(ra->flags & ~V34_RX_FLAG_DATA);
		rb->flags = ra->flags;
	}

	/* Which pointer slots actually hold two different addresses. */
	for (i = 0; i < NPTR; i++) {
		if (memcmp((char *)&oa + PTR[i].lo, (char *)&ob + PTR[i].lo,
			   PTR[i].hi - PTR[i].lo) != 0)
			ptr_seen[i] = 1;
	}
}

static int
first_diff_short(const short *a, const short *b, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return i;
	return -1;
}

static void
compare(long t)
{
	memcpy(&ca, &oa, sizeof(ca));
	memcpy(&cb, &ob, sizeof(cb));
	blank_pointers(&ca);
	blank_pointers(&cb);
	diff_eq_obj("k56FlexPhase34", struct v34_object, &ca, &cb, t);
	diff_eq_int("shaped %ld", first_diff_short(shp_a, shp_b, SHAPED_N),
		    -1, t);
	diff_eq_int("bulk ring %ld", first_diff_short(bra, brb, RING_N),
		    -1, t);
}

/*
 * The idle symbol's three claims, checked against our side alone so that they
 * hold whatever the blob does.
 *
 *   - `f25c6` is untouched.  `txmitdibit` and `txmitquadbit` both write it.
 *   - `f25c8` is the scrambler's raw two bits, 0..3.
 *   - the point is `vect4[f25c8]` exactly, or one of the four `vect16`
 *     entries of quadrant `f25c8` -- NOT `vect4[(d + f25c6) & 3]`.
 *
 * Together these are what distinguishes the object's inlined idle symbol from
 * a call to either published emitter; see src/pump/v34/v34k56.cpp.
 */
static void
check_idle(int quad, short f25c6_before, long t)
{
	int q = oa.f25c8;
	int pt = oa.txpoint.word;

	diff_eq_int("idle leaves f25c6 alone %ld", oa.f25c6, f25c6_before, t);
	diff_eq_int("idle quadrant in range %ld", q >= 0 && q <= 3, 1, t);
	if (q < 0 || q > 3)
		return;
	if (quad) {
		int i, hit = 0;

		for (i = 0; i < 4; i++)
			if (pt == vect16[q * 4 + i])
				hit = 1;
		diff_eq_int("idle point is vect16 of f25c8 %ld", hit, 1, t);
	} else {
		diff_eq_int("idle point is vect4[f25c8] %ld", pt, vect4[q], t);
	}
}

int
main(void)
{
	int rc = 0;
	int i;

	/*
	 * ------------------------------------------------------------------
	 * The tables both sides index, before anything indexes them.
	 */
	diff_begin("v34 k56 constellations");
	{
		diff_eq_int("vect4", memcmp(vect4, ref_vect4, sizeof(vect4)),
			    0, 0);
		diff_eq_int("vect16",
			    memcmp(vect16, ref_vect16, sizeof(vect16)), 0, 0);
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The dispatch, swept.
	 *
	 * `state` covers the three arms the object names and five values it
	 * does not, because the object has no default arm and "does nothing"
	 * is a claim about every other value and not only about the ones
	 * next door.  `constel` covers both documented values of `f382` and
	 * one that is neither, since both readers test for equality with
	 * 0x89b0 and nothing privileges 0x8990.
	 *
	 * Twelve iterations per case: case 3 needs eight to wrap `vect_idx`
	 * and the ninth is what shows the state advanced.
	 */
	diff_begin("k56FlexPhase34 dispatch");
	{
		static const int states[8] = { 0, 1, 2, 3, 4, 5, 6, -1 };
		static const short constels[3] = { (short)0x89b0,
						   (short)0x8990, 0x1234 };
		static const short f25c2s[4] = { 0, 1, 0x200, 0x201 };
		int df, s, c, g, it;

		for (df = 0; df <= 1; df++)
		for (s = 0; s < 8; s++)
		for (c = 0; c < 3; c++)
		for (g = 0; g < 4; g++) {
			int state = states[s];
			int quad = constels[c] == (short)0x89b0;

			setup(df, state, constels[c], 0, (short)0x5a3c,
			      f25c2s[g], 0x1000u + (unsigned)tag);

			for (it = 0; it < 12; it++) {
				struct v34_receiver *ra = (struct v34_receiver *)
					((char *)&oa + OB_RECEIVER);
				short before_c6 = oa.f25c6;
				short before_c0 = oa.f25c0;
				int before_state = oa.k56flex_receiver;
				int r, rr;

				memcpy(&snap, &oa, sizeof(snap));
				memcpy(snap_shp, shp_a, sizeof(snap_shp));
				memcpy(snap_bra, bra, sizeof(snap_bra));
				r  = k56FlexPhase34(&oa);
				rr = ref_k56FlexPhase34(&ob);

				diff_eq_int("return %ld", r, rr, tag);
				diff_eq_int("returns 0 %ld", r, 0, tag);
				compare(tag);

				if (!df) {
					/*
					 * Ja.  The bit source is a stub
					 * returning 0, so the completion arm
					 * cannot run: the flag stays clear
					 * and the state is never touched.
					 */
					n_ja++;
					if (ra->flags & V34_RX_FLAG_DATA)
						n_ja_tail++;
					diff_eq_int("Ja leaves state %ld",
						    oa.k56flex_receiver,
						    before_state, tag);
				} else if (before_state == 3) {
					n_s3++;
					if (oa.k56flex_receiver == 4) {
						n_s3_wrap++;
						diff_eq_int("wrap zeroes "
							    "f25c6 %ld",
							    oa.f25c6, 0, tag);
						diff_eq_int("wrap zeroes "
							    "f25c0 %ld",
							    oa.f25c0, 0, tag);
						diff_eq_int("wrap zeroes "
							    "f25cc %ld",
							    oa.f25cc, 0, tag);
					}
				} else if (before_state == 4) {
					if (quad)
						n_s4_quad++;
					else
						n_s4_dibit++;
					check_idle(quad, before_c6, tag);
					diff_eq_int("idle bumps f25c0 %ld",
						    (unsigned short)oa.f25c0,
						    (unsigned short)
						    (before_c0 + 1), tag);
					diff_eq_int("idle leaves state %ld",
						    oa.k56flex_receiver, 4,
						    tag);
				} else if (before_state == 5) {
					if (quad)
						n_s5_quad++;
					else
						n_s5_dibit++;
					/*
					 * The MP bit source is a stub
					 * returning 0, so the completion arm
					 * cannot run and the state stays 5.
					 */
					if (oa.k56flex_receiver != 5)
						n_mp_tail++;
				} else {
					/*
					 * No arm.  Nothing may move at all,
					 * which is a stronger statement than
					 * the differential makes on its own:
					 * both sides doing the same wrong
					 * thing would still agree.
					 */
					n_idle_state++;
					diff_eq_int("no arm moves no byte "
						    "%ld",
						    memcmp(&oa, &snap,
							   sizeof(oa)), 0,
						    tag);
					diff_eq_int("no arm writes the "
						    "shaped buffer %ld",
						    first_diff_short(shp_a,
								     snap_shp,
								     SHAPED_N),
						    -1, tag);
					diff_eq_int("no arm writes the bulk "
						    "ring %ld",
						    first_diff_short(bra,
								     snap_bra,
								     RING_N),
						    -1, tag);
					diff_eq_int("no arm leaves state %ld",
						    oa.k56flex_receiver,
						    before_state, tag);
				}
				tag++;
			}
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * Case 3's shift count, which is the one place the object's x86
	 * shift-count masking is observable.
	 *
	 * `vect_idx` is masked to 0..7 on the way OUT of case 3, so only a
	 * caller can present it with anything else -- and `v34handshakinit`
	 * and `modulatevector` both write it, so "only a caller" is not
	 * hypothetical.  The count is `2 * vect_idx` taken from a SIGN-
	 * extended load, so -1 shifts by 30 and 100 shifts by 8; both are the
	 * bottom five bits of the doubled value and neither is the shift a
	 * plain C `>>` would perform.  Sweeping them is what turns `& 31` in
	 * the reconstruction from an assumption into a measurement.
	 */
	diff_begin("k56FlexPhase34 case 3 shift count");
	{
		static const short idxs[10] = { 0, 1, 2, 6, 7, 8, 15, 100,
						-1, -9 };
		static const short words[4] = { (short)0x899f, (short)0x8990,
						(short)0xffff, 0x0001 };
		int a, b, it;

		for (a = 0; a < 10; a++)
		for (b = 0; b < 4; b++) {
			setup(1, 3, (short)0x8990, idxs[a], words[b], 1,
			      0x7000u + (unsigned)tag);
			for (it = 0; it < 3; it++) {
				int r, rr;

				r  = k56FlexPhase34(&oa);
				rr = ref_k56FlexPhase34(&ob);
				diff_eq_int("return %ld", r, rr, tag);
				compare(tag);
				n_s3++;
				tag++;
			}
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The generator does not follow `f25c2` bit 0.
	 *
	 * `txmitdibit` and `txmitquadbit` pass `tx_scrambler_mode(o)`, which
	 * is that bit; the idle symbol passes the literal 1.  So flipping the
	 * bit with everything else held fixed must leave case 4's outputs
	 * IDENTICAL -- which is a claim about our side alone, and the one
	 * that a reconstruction calling either published emitter would fail
	 * even though the blob agreed with it on every other sweep.
	 *
	 * Bit 9 is held clear throughout: it gates `txmit`'s echo feed, which
	 * would move the object for a reason that has nothing to do with the
	 * scrambler.
	 */
	diff_begin("k56FlexPhase34 idle generator is not f25c2");
	{
		static const short constels[2] = { (short)0x89b0,
						   (short)0x8990 };
		static int f25cc0, f25c80, pt0;
		int c, it, gpc;

		for (c = 0; c < 2; c++) {
			for (gpc = 0; gpc <= 1; gpc++) {
				setup(1, 4, constels[c], 0, 0, (short)gpc,
				      0x2000u + (unsigned)c);
				for (it = 0; it < 6; it++) {
					int r, rr;

					r  = k56FlexPhase34(&oa);
					rr = ref_k56FlexPhase34(&ob);
					diff_eq_int("return %ld", r, rr, tag);
					compare(tag);
					if (constels[c] == (short)0x89b0)
						n_s4_quad++;
					else
						n_s4_dibit++;
					tag++;
				}
				if (gpc == 0) {
					f25cc0 = oa.f25cc;
					f25c80 = oa.f25c8;
					pt0 = oa.txpoint.word;
				} else {
					diff_eq_int("gpc does not move f25cc "
						    "%ld", oa.f25cc, f25cc0,
						    c);
					diff_eq_int("gpc does not move f25c8 "
						    "%ld", oa.f25c8, f25c80,
						    c);
					diff_eq_int("gpc does not move the "
						    "point %ld",
						    oa.txpoint.word,
						    pt0, c);
				}
			}
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * Anti-vacuity.
	 *
	 * Seven reachable blocks, each asserted to have run.  The two
	 * unreachable ones are asserted NOT to have run, because that is the
	 * only form of the statement that is satisfiable: their gate is a
	 * three-byte stub returning 0 on both sides.  See the header comment
	 * and finding F281.
	 */
	diff_begin("k56FlexPhase34 coverage");
	{
		diff_eq_int("Ja arm ran", n_ja > 0, 1, 0);
		diff_eq_int("case 3 ran", n_s3 > 0, 1, 0);
		diff_eq_int("case 3 wrapped to 4", n_s3_wrap > 0, 1, 0);
		diff_eq_int("case 4 dibit ran", n_s4_dibit > 0, 1, 0);
		diff_eq_int("case 4 quadbit ran", n_s4_quad > 0, 1, 0);
		diff_eq_int("case 5 dibit ran", n_s5_dibit > 0, 1, 0);
		diff_eq_int("case 5 quadbit ran", n_s5_quad > 0, 1, 0);
		diff_eq_int("no-arm case ran", n_idle_state > 0, 1, 0);

		/* Unreachable, and measured as such rather than assumed. */
		diff_eq_int("Ja completion arm never entered", n_ja_tail, 0,
			    0);
		diff_eq_int("MP completion arm never entered", n_mp_tail, 0,
			    0);

		for (i = 0; i < NPTR; i++)
			diff_eq_int("pointer slot %ld held two addresses",
				    ptr_seen[i], 1, i);
	}
	rc |= diff_end();

	return rc;
}
