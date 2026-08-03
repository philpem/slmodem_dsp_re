/*
 * t_v34hshak.c -- differential test of the V.34 handshake's support functions.
 *
 * Five functions, all of which write scattered fields of the 44 KB V.34
 * object, so the comparison is the whole object byte for byte.  Anything
 * narrower would pass a store that landed in the wrong pad.
 *
 * POINTERS ARE THE ONE EXCEPTION, and they are handled the way t_v34ec.c
 * established: the two sides hold different addresses by construction, so
 * each pointer field is skipped in the byte compare and checked by the
 * CONTENT it selects instead -- which is the thing that matters and is
 * stronger than an address comparison could be.  `ptr_skip` below is the
 * complete list, derived from the code rather than from watching the test
 * fail, and `saw_ptr_skip` asserts every entry was actually reached so the
 * list cannot quietly grow stale.
 *
 * WHERE A FUNCTION CAN LEAVE A FIELD ALONE, THE FIELD IS SEEDED WITH A
 * PER-SIDE DUMMY.  `setfinalrate` sets nothing at all for rate codes 1, 6
 * and 7, and `setupreceiver` has no default arm on either of its switches.
 * Seeding the pointer with a buffer whose contents are a pattern no real
 * table has means "left alone" and "set to some table" are distinguishable
 * by content, which they would not be if the seed were one of the tables.
 *
 * `preempindex` IS NOT SWEPT OVER UNKNOWN BAUD RATES.  The object has no
 * default arm and leaves two registers unset, so an unrecognised rate makes
 * it multiply whatever the caller happened to leave in %edx and %esi.  That
 * is not reproducible and is recorded as D37 rather than tested; the five
 * rates it does handle are swept exhaustively over the index range.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34det.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34recv.h"

extern unsigned int ref_dsplibs_debug_level;

extern void ref_dpskDetectInfo1Init(void *obj);
extern void ref_dpskinit(void *obj, short mode, short high);
extern void ref_setfinalrate(void *obj);
extern void ref_setupreceiver(void *obj);
extern short ref_preempindex(void *obj, short baudrate);

extern const short ref_scale2400[28], ref_scale2800[28], ref_scale3000[28];
extern const short ref_scale3200[28], ref_scale3429[28];
extern const short ref_c1600[8], ref_c1680[8], ref_c1800_[8], ref_c1829[8];
extern const short ref_c1867[8], ref_c1920[8], ref_c1959[8], ref_c2000[8];
extern short ref_bpv22high[60], ref_bpv22low[60];

/* --- the two objects, and the buffers they point out of ------------------ */

static struct v34_object oa;
static unsigned char ob[sizeof(struct v34_object)];

/* V34SetupModulator writes through m->shaped; each side needs its own. */
static short shaped_a[4096], shaped_b[4096];

/*
 * The per-side dummies.  Filled with a pattern no table in the object holds,
 * so "the function left this pointer alone" is visible in the content check.
 */
#define DUMMY_LEN 64
static short dummy_a[DUMMY_LEN], dummy_b[DUMMY_LEN];

/*
 * Object offsets of every pointer-sized field these five functions or their
 * callees write.  All are 4 bytes on the 32-bit target the differential tier
 * builds for.
 */
static const unsigned ptr_skip[] = {
	0x0394,		/* receiver +0x130 rx_samples -- rxinit, interior   */
	0x0418,		/* receiver +0x1b4 carrier    -- setupreceiver      */
	0x0508,		/* receiver +0x2a4 f2a4       -- dpskinit           */
	0x1460,		/* modulator +0x10 sine       -- V34SetupModulator  */
	0x2074,		/* modulator +0xc24 shaped    -- the test seeds it  */
	0x20cc,		/* modulator +0xc7c ec_prem                         */
	0x2100,		/* modulator +0xcb0 preemp                          */
	0x3564,		/* detector +0x00 coeff       -- detectorinit       */
	0xaa90,		/* tx power scale             -- setfinalrate       */
	0xaaac,		/* rx power scale                                   */
	0xaab0		/* rx carrier descriptor                            */
};
#define NPTR (sizeof(ptr_skip) / sizeof(ptr_skip[0]))

static int saw_ptr_skip[NPTR];

static int
skipped(unsigned off)
{
	unsigned k;

	for (k = 0; k < NPTR; k++)
		if (off >= ptr_skip[k] && off < ptr_skip[k] + 4)
			return 1;
	return 0;
}

static void
setup(void)
{
	memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
	memset(ob, HARNESS_MALLOC_FILL, sizeof(ob));
	memset(shaped_a, 0x5a, sizeof(shaped_a));
	memset(shaped_b, 0x5a, sizeof(shaped_b));
	memset(dummy_a, 0, sizeof(dummy_a));
	memset(dummy_b, 0, sizeof(dummy_b));
	{
		int i;

		for (i = 0; i < DUMMY_LEN; i++)
			dummy_a[i] = dummy_b[i] = (short)(0x4b00 + i);
	}
}

static void
poke_ptr(unsigned off, void *pa, void *pb)
{
	memcpy((unsigned char *)&oa + off, &pa, sizeof(pa));
	memcpy(ob + off, &pb, sizeof(pb));
}

static void
poke_short(unsigned off, short v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

static void
poke_byte(unsigned off, unsigned char v)
{
	*((unsigned char *)&oa + off) = v;
	ob[off] = v;
}

static void *
get_ptr_a(unsigned off)
{
	void *p;

	memcpy(&p, (unsigned char *)&oa + off, sizeof(p));
	return p;
}

static void *
get_ptr_b(unsigned off)
{
	void *p;

	memcpy(&p, ob + off, sizeof(p));
	return p;
}

static short
get_short_a(unsigned off)
{
	short v;

	memcpy(&v, (unsigned char *)&oa + off, sizeof(v));
	return v;
}

/*
 * Compare the two objects, pointers excluded.  Only mismatches are reported,
 * plus one summary check so an all-equal run still counts.
 */
static void
compare(const char *what, long tag)
{
	const unsigned char *p = (const unsigned char *)&oa;
	unsigned i, k;
	int bad = 0;

	for (k = 0; k < NPTR; k++)
		if (memcmp(p + ptr_skip[k], ob + ptr_skip[k], 4) != 0)
			saw_ptr_skip[k] = 1;

	for (i = 0; i < sizeof(oa); i++) {
		if (p[i] == ob[i] || skipped(i))
			continue;
		bad++;
		if (bad <= 8) {
			/*
			 * The offset is the diagnosis for a struct that is
			 * mostly padding, so it goes in the message rather
			 * than being folded into the input number where it
			 * would have to be decoded by hand.
			 */
			char msg[160];

			snprintf(msg, sizeof(msg),
				 "%s: object byte at +0x%x (case %ld)",
				 what, i, tag);
			diff_eq_int(msg, p[i], ob[i], (long)i);
		}
	}
	diff_eq_int(what, bad, 0, tag);
}

/* Compare the shorts two pointers select. */
static void
compare_table(const char *what, const short *a, const short *b, int n, long tag)
{
	int i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			diff_eq_int(what, a[i], b[i], (long)i * 1000 + tag);
	diff_eq_int(what, memcmp(a, b, (size_t)n * sizeof(short)) == 0, 1, tag);
}

/* --- setfinalrate's inputs ------------------------------------------------ */

static void
seed_rate_pointers(void)
{
	poke_ptr(0xaa90, dummy_a, dummy_b);
	poke_ptr(0xaaac, dummy_a, dummy_b);
	poke_ptr(0xaab0, dummy_a, dummy_b);
}

static void
run_setfinalrate(unsigned short a9de, unsigned short a9e0,
		 unsigned short a9e2, unsigned char cbits, long tag)
{
	setup();
	seed_rate_pointers();
	poke_short(0xa9de, (short)a9de);
	poke_short(0xa9e0, (short)a9e0);
	poke_short(0xa9e2, (short)a9e2);
	poke_byte(0xa9ae, cbits);
	poke_byte(0xa9b2, cbits);
	poke_byte(0xa9b6, cbits);
	poke_byte(0xa9b8, cbits);

	setfinalrate(&oa);
	ref_setfinalrate(ob);

	compare("setfinalrate", tag);
	compare_table("setfinalrate tx scale",
		      (const short *)get_ptr_a(0xaa90),
		      (const short *)get_ptr_b(0xaa90), 28, tag);
	compare_table("setfinalrate rx scale",
		      (const short *)get_ptr_a(0xaaac),
		      (const short *)get_ptr_b(0xaaac), 28, tag);
	compare_table("setfinalrate rx carrier desc",
		      (const short *)get_ptr_a(0xaab0),
		      (const short *)get_ptr_b(0xaab0), 8, tag);
}

/* --- setupreceiver -------------------------------------------------------- */

static void
run_setupreceiver(short baud, short carrier, short gain, long tag)
{
	setup();
	/* The carrier table and its length, seeded so the default arm is
	 * visible as "still the dummy" rather than as a wild pointer. */
	poke_ptr(0x0418, dummy_a, dummy_b);
	poke_short(0x264 + 0x1ba, 8);
	poke_ptr(0xaab0, dummy_a, dummy_b);
	poke_short(0xaa96, baud);
	poke_short(0xaaa8, carrier);
	poke_short(0x264 + 0x262, gain);

	setupreceiver(&oa);
	ref_setupreceiver(ob);

	compare("setupreceiver", tag);
	compare_table("setupreceiver carrier table",
		      (const short *)get_ptr_a(0x0418),
		      (const short *)get_ptr_b(0x0418),
		      2 * get_short_a(0x264 + 0x1ba), tag);
	compare_table("setupreceiver detector coeff",
		      (const short *)get_ptr_a(0x3564),
		      (const short *)get_ptr_b(0x3564), 8, tag);
}

/* --- preempindex ---------------------------------------------------------- */

/*
 * Its argument is an object nothing in the blob constructs, so the fixture
 * builds one: a block large enough for the highest offset it reads plus the
 * short there, and no larger, so an index past the end faults rather than
 * reading something that happens to agree on both sides.  Finding 129.
 */
#define PREEMP_HIGH	0x3d4
#define PREEMP_SIZE	(PREEMP_HIGH + (int)sizeof(short))

static unsigned char preemp_obj[PREEMP_SIZE];

static void
run_preempindex(short limit, short meas, short baud, long tag)
{
	short got, want;
	unsigned k;
	static const unsigned slot[] = { 0x324, 0x350, 0x37c, 0x3d4 };

	memset(preemp_obj, 0, sizeof(preemp_obj));
	memcpy(preemp_obj + 0xbc, &limit, sizeof(limit));
	for (k = 0; k < sizeof(slot) / sizeof(slot[0]); k++)
		memcpy(preemp_obj + slot[k], &meas, sizeof(meas));

	got = preempindex(preemp_obj, baud);
	want = ref_preempindex(preemp_obj, baud);
	diff_eq_int("preempindex", got, want, tag);
}

int
main(void)
{
	unsigned i, k;
	int rc = 0;

	diff_begin("v34 handshake: the fifteen rate tables");
	{
		compare_table("scale2400", scale2400, ref_scale2400, 28, 0);
		compare_table("scale2800", scale2800, ref_scale2800, 28, 0);
		compare_table("scale3000", scale3000, ref_scale3000, 28, 0);
		compare_table("scale3200", scale3200, ref_scale3200, 28, 0);
		compare_table("scale3429", scale3429, ref_scale3429, 28, 0);
		compare_table("c1600", c1600, ref_c1600, 8, 0);
		compare_table("c1680", c1680, ref_c1680, 8, 0);
		compare_table("c1800_", c1800_, ref_c1800_, 8, 0);
		compare_table("c1829", c1829, ref_c1829, 8, 0);
		compare_table("c1867", c1867, ref_c1867, 8, 0);
		compare_table("c1920", c1920, ref_c1920, 8, 0);
		compare_table("c1959", c1959, ref_c1959, 8, 0);
		compare_table("c2000", c2000, ref_c2000, 8, 0);
		compare_table("bpv22high", bpv22high, ref_bpv22high, 60, 0);
		compare_table("bpv22low", bpv22low, ref_bpv22low, 60, 0);
	}
	rc |= diff_end();

	diff_begin("v34 handshake: dpskDetectInfo1Init");
	{
		setup();
		dpskDetectInfo1Init(&oa);
		ref_dpskDetectInfo1Init(ob);
		compare("dpskDetectInfo1Init", 0);

		/*
		 * And again over an object that is already initialised, so
		 * the clear is being asked to clear something rather than to
		 * overwrite one fill pattern with zeroes.
		 */
		setup();
		dpskDetectInfo1Init(&oa);
		ref_dpskDetectInfo1Init(ob);
		for (i = 0; i < 100; i++) {
			poke_short(0xaae6 + i * 2, (short)(i * 331 - 5000));
		}
		dpskDetectInfo1Init(&oa);
		ref_dpskDetectInfo1Init(ob);
		compare("dpskDetectInfo1Init twice", 1);

		/*
		 * The clear runs seven shorts past the low-pass.  Assert the
		 * fourteen bytes after +0xaba0 really were written, or the
		 * comment in v34hshak.c is describing something the test
		 * cannot see.
		 */
		diff_eq_int("the clear passes the low-pass",
			    get_short_a(0xabac), 0, 0);
		diff_eq_int("and stops at 100 shorts",
			    get_short_a(0xabae), (short)0xa5a5, 0);
	}
	rc |= diff_end();

	diff_begin("v34 handshake: dpskinit");
	{
		static const short modes[] = { 0, 1, 2, -1, 0x7fff };
		static const short highs[] = { 0, 1, 2, -1 };
		unsigned mi, hi;

		for (mi = 0; mi < sizeof(modes) / sizeof(modes[0]); mi++)
		for (hi = 0; hi < sizeof(highs) / sizeof(highs[0]); hi++) {
			long tag = (long)mi * 10 + hi;
			const struct v34_modulator *ma;

			setup();
			poke_ptr(0x2074, shaped_a, shaped_b);
			poke_short(0x264 + 0x262, (short)(0x1234 + mi));

			dpskinit(&oa, modes[mi], highs[hi]);
			ref_dpskinit(ob, modes[mi], highs[hi]);

			compare("dpskinit", tag);

			ma = (const struct v34_modulator *)
			     ((const char *)&oa + 0x1450);

			compare_table("dpskinit band-pass",
				      (const short *)get_ptr_a(0x0508),
				      (const short *)get_ptr_b(0x0508),
				      V34_BPV22_TAPS, tag);
			compare_table("dpskinit carrier table",
				      (const short *)get_ptr_a(0x1460),
				      (const short *)get_ptr_b(0x1460),
				      ma->sine_len * 2, tag);
			compare_table("dpskinit ec_prem",
				      (const short *)get_ptr_a(0x20cc),
				      (const short *)get_ptr_b(0x20cc),
				      42, tag);
			compare_table("dpskinit preemp",
				      (const short *)get_ptr_a(0x2100),
				      (const short *)get_ptr_b(0x2100),
				      16, tag);
			compare_table("dpskinit shaped", shaped_a, shaped_b,
				      ma->rows * V34_MOD_ROW, tag);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: setfinalrate");
	{
		/*
		 * The three source shorts carry four independent fields
		 * between them, so a9e2 is swept over its whole low byte --
		 * that is both rate codes' bits and the receive side's
		 * carried bit -- against a spread of a9de and a9e0 and both
		 * settings of the four scattered carrier bits.
		 */
		static const unsigned short de[] = { 0x0000, 0x0004, 0x0003,
						     0x0007, 0xfffb, 0xffff };
		static const unsigned short e0[] = { 0x0000, 0x0007, 0x00c0,
						     0x00c7, 0x00ff, 0xff3f };
		unsigned di, ei, cb;

		for (i = 0; i < 256; i++)
		for (cb = 0; cb <= 1; cb++)
			run_setfinalrate(0x0004, 0x0000, (unsigned short)i,
					 cb ? 0xff : 0x00,
					 (long)i * 10 + cb);

		for (di = 0; di < sizeof(de) / sizeof(de[0]); di++)
		for (ei = 0; ei < sizeof(e0) / sizeof(e0[0]); ei++)
		for (i = 0; i < 8; i++)
			run_setfinalrate(de[di], e0[ei],
					 (unsigned short)((i << 4) | (i << 7)),
					 0x00,
					 100000L + (long)di * 1000
					 + (long)ei * 100 + i);
	}
	rc |= diff_end();

	diff_begin("v34 handshake: setupreceiver");
	{
		static const short bauds[] = { 2400, 2743, 2800, 3000, 3200,
					       3429, 0, 600, 4800, -1 };
		static const short carrs[] = { 1600, 1680, 1800, 1829, 1867,
					       1920, 1959, 2000, 1200, 2400,
					       0, -1 };
		unsigned bi, ci;

		for (bi = 0; bi < sizeof(bauds) / sizeof(bauds[0]); bi++)
		for (ci = 0; ci < sizeof(carrs) / sizeof(carrs[0]); ci++)
			run_setupreceiver(bauds[bi], carrs[ci],
					  (short)(0x300 + bi * 16 + ci),
					  (long)bi * 100 + ci);
	}
	rc |= diff_end();

	diff_begin("v34 handshake: setupreceiver with the debug sites live");
	{
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		for (i = 0; i < 6; i++) {
			static const short bauds[] = { 2400, 2743, 2800, 3000,
						       3200, 3429 };

			dsplib_debug_capture_reset();
			run_setupreceiver(bauds[i], 1829, (short)(0x200 + i),
					  9000 + i);
			diff_eq_int("setupreceiver transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, 9000 + i);

			dsplib_debug_capture_reset();
			run_setfinalrate(0x0004, 0x0000,
					 (unsigned short)((i << 4) | 0x80),
					 0x00, 9100 + i);
			diff_eq_int("setfinalrate transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, 9100 + i);
		}

		diff_eq_int("transcript non-empty",
			    dsplib_debug_capture_text(1)[0] != 0, 1, 0);
		diff_eq_int("ours printed too",
			    dsplib_debug_capture_text(0)[0] != 0, 1, 0);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	diff_begin("v34 handshake: preempindex");
	{
		static const short bauds[] = { 2400, 2800, 3000, 3200, 3429 };
		static const short limits[] = { 0, 1, 100, 1000, 4000, 8000,
						16000, 32767, -1, -1000,
						-32768 };
		static const short meas[] = { 0, 1, 2, 7, 100, 1000, 2000,
					      4000, 8000, 16000, 20000, 32767,
					      -1, -100, -4000, -32768 };
		unsigned bi, li, mi;

		for (bi = 0; bi < sizeof(bauds) / sizeof(bauds[0]); bi++)
		for (li = 0; li < sizeof(limits) / sizeof(limits[0]); li++)
		for (mi = 0; mi < sizeof(meas) / sizeof(meas[0]); mi++)
			run_preempindex(limits[li], meas[mi], bauds[bi],
					(long)bi * 10000 + (long)li * 100 + mi);

		/*
		 * The sweep must reach both ends of the index range, or it
		 * is only testing one exit.
		 */
		{
			/*
			 * The sweep has to reach both exits or it is only
			 * testing one of them: index 6 is "the very first
			 * multiplication passed the limit" and index 10 is
			 * "none of the five did".
			 *
			 * Reaching 6 needs a measurement that grows without
			 * wrapping -- 32767 does NOT do it, because one
			 * multiply by a ratio near 1.6 overflows the short
			 * and comes back negative, which is below every
			 * non-negative limit and keeps the loop going.  That
			 * is the object's arithmetic, not the fixture's, and
			 * it is why the pair below is small-and-zero rather
			 * than large-and-large.
			 */
			int saw6 = 0, saw10 = 0, k2;

			for (k2 = 0; k2 < 5; k2++) {
				static const short b[] = { 2400, 2800, 3000,
							   3200, 3429 };

				run_preempindex(0, 100, b[k2], 20000 + k2);
				if (preempindex(preemp_obj, b[k2]) == 6)
					saw6 = 1;

				run_preempindex(32767, 0, b[k2], 20100 + k2);
				if (preempindex(preemp_obj, b[k2]) == 10)
					saw10 = 1;
			}
			diff_eq_int("the sweep reached index 6", saw6, 1, 0);
			diff_eq_int("the sweep reached index 10", saw10, 1, 0);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: every skipped pointer field was written");
	{
		/*
		 * The skip list is a hole in the byte comparison, so each
		 * entry has to earn its place: if a field never differs
		 * between the two sides it is not a pointer field and should
		 * not be skipped.
		 */
		for (k = 0; k < NPTR; k++)
			diff_eq_int("pointer field differed at least once",
				    saw_ptr_skip[k], 1, (long)ptr_skip[k]);
	}
	rc |= diff_end();

	return rc;
}
