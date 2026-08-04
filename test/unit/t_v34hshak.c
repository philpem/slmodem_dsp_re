/*
 * t_v34hshak.c -- differential test of the V.34 handshake's support functions.
 *
 * Seven functions, all of which write scattered fields of the 44 KB V.34
 * object, so the comparison is the whole object byte for byte.  Anything
 * narrower would pass a store that landed in the wrong pad.
 *
 * AND ONE TABLE THAT CANNOT BE COMPARED DIRECTLY.  `StateName` is a LOCAL
 * symbol, so `objcopy --redefine-syms` cannot produce a `ref_StateName` for
 * the fifteen-table section below to copy.  The only thing that reaches its
 * eighty-seven strings is what `v34handshakinit`'s thirteen traces print, so
 * the transcript sweeps at the bottom of this file are not a supplement to a
 * table comparison -- they ARE the comparison.  Two sweeps, and both are
 * needed: one drives the three state words together, which reaches every one
 * of the 87 names, and one drives them a third of the table apart, which is
 * the only thing that can tell the three machines' argument slots apart.
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
#include <stdlib.h>
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
extern void ref_v34modeminit(void *obj);
extern void ref_v34handshakinit(void *obj, int mode);
extern void ref_V34InitializeImplementationSpecific(void *obj);
extern const short ref_c1200_[8], ref_c2400_[8];

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
	0xaab0,		/* rx carrier descriptor                            */
	/*
	 * And the ones `v34modeminit` reaches through its callees.  txinit
	 * primes both sample queues with interior cursors; the two echo
	 * cancellers get five pointers each from
	 * V34InitializeImplementationSpecific; preinitdigital installs the
	 * scrambler pair and the convolution table in both shell contexts.
	 * Every one is an address, and every one is checked by what it
	 * selects instead.
	 */
	0x0268, 0x026c,			/* rxq read and write cursors  */
	0x2220, 0x2224,			/* txq                         */
	0x80b8, 0x80bc, 0x80c0, 0x80c4, 0x80c8,	/* echo canceller 0    */
	0x9138, 0x913c, 0x9140, 0x9144, 0x9148,	/* and 1               */
	0x0a28, 0x0e48,			/* receive shell context       */
	0x2608, 0x2a28,			/* transmit shell context      */
	/*
	 * And `v34handshakinit`'s two, which are POINTERS INTO THE OBJECT
	 * ITSELF -- +0xa97c and +0xa94c, two of the five 0x30-byte message
	 * records.  So the two sides necessarily differ, and the content
	 * check for them is not "the same bytes" (both records are nearly all
	 * zero, so a swapped pair would pass that) but "the same offset from
	 * its own object".  `check_self_ptr` below.
	 */
	0xaa6c, 0xaa70,
	/*
	 * And the timing filters' two coefficient pointers, which only mode 0
	 * reaches -- it is the one body that calls `rxtiminginit`, and
	 * `V34TimingFiltersInit` installs them.  t_v34rx.c skips the same two.
	 */
	0x0620, 0x0624			/* timing +0x114 and +0x118    */
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

static void
poke_int(unsigned off, int v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
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
	/*
	 * AND THE DETECTOR'S, for the same reason -- it was not seeded, and
	 * the comparison below dereferences it, so a reconstruction that took
	 * a path not reaching `detectorinit` crashed this test instead of
	 * failing it.  Found by mutating the baud switch's default arm.
	 */
	poke_ptr(0x3564, dummy_a, dummy_b);
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

/* --- v34handshakinit ------------------------------------------------------ */

/*
 * The three state words.  Named here as well as in v34hshak.c so that this
 * file does not silently agree with a transposition it is meant to detect:
 * every case below seeds all three to DIFFERENT values, and the transcript
 * sweep drives them on three offset cycles for the same reason.
 */
#define HSI_MICROSTATE	0x3592
#define HSI_RXSTATE	0x3594
#define HSI_TXSTATE	0x3596

/*
 * Every input the five bodies read, one field per member, so that no two can
 * be swept from one variable.  That is the fixture defect of findings 116b,
 * 123 and 171, and it turned up three times in the previous session alone --
 * two inputs driven together cannot be told apart, however thorough the
 * sweep looks.
 *
 * `timer_base` and `timer_delta` are separate for exactly that reason: the
 * guard subtracts one from the other and compares UNSIGNED, so a signed
 * reconstruction differs only when the difference goes negative, which one
 * variable driving both could never produce.
 */
struct hsi_case {
	int		mode;
	int		timer_base;	/* +0x238 */
	int		timer_delta;	/* +0x248 */
	short		f359c;		/* originate/answer, 0x65 vs 0x66 */
	unsigned short	rxflags;	/* receiver +0x122 */
	unsigned short	txflags;	/* +0x25c2 */
	unsigned char	ac17;		/* mode 1's second branch input */
	short		ac12;		/* mode 1's two counters */
	short		ac14;
	short		mst;		/* the three machines, in range */
	short		rxst;
	short		txst;
	short		trace1;		/* +0x2aa2, printed as [1] */
	short		trace2;		/* +0xaa78, printed as [2] */
	short		f262;		/* the AGC's starting gain */
	int		v90_receiver;	/* +0x24c, gates V34SetINFO0dBits */
	int		moh_message;	/* +0xabf0 */
};

/*
 * The default case.  Three DIFFERENT state values, two DIFFERENT trace
 * counters, and a Modem-on-Hold selector out of range so the message builder
 * writes nothing unless a case asks it to.
 */
static const struct hsi_case hsi_base = {
	0,			/* mode                                     */
	0, 0,			/* timer base, delta                        */
	0x65,			/* f359c                                    */
	0x0000, 0x0000,		/* rxflags, txflags                         */
	0,			/* ac17                                     */
	0x0111, 0x0222,		/* ac12, ac14                               */
	V34HS_PHASE1,		/* mst  = 33                                */
	V34HS_PHASE2,		/* rxst = 34                                */
	V34HS_TONE_AB,		/* txst = 60                                */
	0x1111, 0x2222,		/* [1], [2]                                 */
	0x0600,			/* f262                                     */
	0,			/* v90_receiver                             */
	9			/* moh_message: above 5, builds nothing     */
};

/*
 * A pointer the object aims at itself: compare the OFFSET, not the address
 * and not the bytes.  `want` is where it should land, or the dummy when the
 * mode in question leaves the field alone.
 */
static void
check_self_ptr(const char *what, unsigned off, long want, long tag)
{
	long da = (long)((char *)get_ptr_a(off) - (char *)&oa);
	long db = (long)((char *)get_ptr_b(off) - (char *)ob);

	if (want < 0) {
		/* Untouched: each side must still hold its own dummy. */
		diff_eq_int(what, get_ptr_a(off) == (void *)dummy_a, 1, tag);
		diff_eq_int(what, get_ptr_b(off) == (void *)dummy_b, 1, tag);
		return;
	}
	diff_eq_int(what, (int)da, (int)db, tag);
	diff_eq_int(what, (int)da, (int)want, tag);
}

static void
run_handshakinit(const struct hsi_case *c, long tag)
{
	setup();

	/*
	 * Modes 0, 1 and 4 reach `txinit` through `v34modeminit`, which cleans
	 * both echo cancellers through five pointers each.  Aiming them is
	 * `V34InitializeImplementationSpecific`'s job and not v34handshakinit's,
	 * and without it the first dereference faults -- the same reason the
	 * v34modeminit case above calls it.
	 */
	V34InitializeImplementationSpecific(&oa);
	ref_V34InitializeImplementationSpecific(ob);

	/*
	 * The two self-pointers, seeded per side so that "this mode left the
	 * field alone" and "this mode aimed it somewhere" are distinguishable.
	 */
	poke_ptr(0xaa6c, dummy_a, dummy_b);
	poke_ptr(0xaa70, dummy_a, dummy_b);
	/* Likewise the two timing coefficients, so "mode 0 installed them"
	 * and "every other mode left them" are both visible. */
	poke_ptr(0x0620, dummy_a, dummy_b);
	poke_ptr(0x0624, dummy_a, dummy_b);

	poke_int(0x238, c->timer_base);
	poke_int(0x248, c->timer_delta);
	poke_int(0x24c, c->v90_receiver);
	poke_int(0xabf0, c->moh_message);
	poke_short(0x359c, c->f359c);
	poke_short(0x264 + 0x122, (short)c->rxflags);
	poke_short(0x25c2, (short)c->txflags);
	poke_byte(0xac17, c->ac17);
	poke_short(0xac12, c->ac12);
	poke_short(0xac14, c->ac14);
	poke_short(HSI_MICROSTATE, c->mst);
	poke_short(HSI_RXSTATE, c->rxst);
	poke_short(HSI_TXSTATE, c->txst);
	poke_short(0x2aa2, c->trace1);
	poke_short(0xaa78, c->trace2);
	poke_short(0x264 + 0x262, c->f262);

	v34handshakinit(&oa, c->mode);
	ref_v34handshakinit(ob, c->mode);

	compare("v34handshakinit", tag);

	/*
	 * Modes 0 and 4 aim +0xaa70 at the record at +0xa97c; only mode 4 also
	 * aims +0xaa6c at +0xa94c.  Everything else must leave both alone,
	 * which is the half of the check a byte comparison with a hole in it
	 * cannot make.
	 */
	check_self_ptr("v34handshakinit +0xaa70", 0xaa70,
		       (c->mode == 0 || c->mode == 4) ? 0xa97c : -1, tag);
	check_self_ptr("v34handshakinit +0xaa6c", 0xaa6c,
		       (c->mode == 4) ? 0xa94c : -1, tag);

	/*
	 * The timing filters' coefficients: mode 0 alone installs them, and
	 * they point OUT of the object, so these two are compared by content
	 * the ordinary way.
	 */
	if (c->mode == 0) {
		compare_table("v34handshakinit prefilter coeff",
			      (const short *)get_ptr_a(0x0620),
			      (const short *)get_ptr_b(0x0620),
			      V34_TIMING_PRE_TAPS, tag);
		compare_table("v34handshakinit hp coeff",
			      (const short *)get_ptr_a(0x0624),
			      (const short *)get_ptr_b(0x0624),
			      V34_TIMING_HP_TAPS, tag);
	} else {
		diff_eq_int("v34handshakinit left the prefilter coeff",
			    get_ptr_a(0x0620) == (void *)dummy_a, 1, tag);
		diff_eq_int("v34handshakinit left the hp coeff",
			    get_ptr_a(0x0624) == (void *)dummy_a, 1, tag);
	}

	/*
	 * Modes 0, 1 and 4 run v34modeminit, so its pointers have to be
	 * checked by content here too -- the phase-2 carrier pair is the
	 * mutation that survived the first fixture (hsine1200 and hsine2400
	 * are the same length, so swapping them changes nothing a skipped
	 * pointer can see).
	 */
	if (c->mode == 0 || c->mode == 1 || c->mode == 4) {
		const struct v34_modulator *ma =
			(const struct v34_modulator *)
			((const char *)&oa + 0x1450);

		compare_table("v34handshakinit band-pass",
			      (const short *)get_ptr_a(0x0508),
			      (const short *)get_ptr_b(0x0508),
			      V34_BPV22_TAPS, tag);
		compare_table("v34handshakinit carrier table",
			      (const short *)get_ptr_a(0x1460),
			      (const short *)get_ptr_b(0x1460),
			      ma->sine_len * 2, tag);
		compare_table("v34handshakinit detector coeff",
			      (const short *)get_ptr_a(0x3564),
			      (const short *)get_ptr_b(0x3564), 8, tag);
	}
	/*
	 * And mode 4 arms the detector a SECOND time, with the pair the other
	 * way round from v34modeminit's.  Checked by identity because the two
	 * descriptors differ in only two of their eight shorts.
	 */
	if (c->mode == 4)
		diff_eq_int("v34handshakinit MOH detector coeff",
			    get_ptr_a(0x3564)
			    == (void *)(c->f359c == 0x65 ? c2400_ : c1200_),
			    1, tag);
}

/*
 * One case with the transcripts captured and compared.
 *
 * A transcript mismatch otherwise reports as "got 0, reference 1" and nothing
 * else, which over a 174-case sweep is not a diagnosis.  `HSI_DUMP=1` in the
 * environment prints both sides of any case that differs; it paid for itself
 * on the first run, where the difference was one line deep inside
 * `v34modeminit`'s call tree and not in this function at all.
 */
static void
run_handshakinit_traced(const struct hsi_case *c, long tag)
{
	dsplib_debug_capture_reset();
	run_handshakinit(c, tag);
	if (getenv("HSI_DUMP")
	    && strcmp(dsplib_debug_capture_text(0),
		      dsplib_debug_capture_text(1)) != 0)
		fprintf(stderr, "--- case %ld mode %d\n=== ours\n%s=== ref\n%s",
			tag, c->mode, dsplib_debug_capture_text(0),
			dsplib_debug_capture_text(1));
	diff_eq_int("v34handshakinit transcript",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("v34handshakinit transcript non-empty",
		    dsplib_debug_capture_text(1)[0] != 0, 1, tag);
}
extern void txmitdibit(void *obj, short bits);
extern void txmitquadbit(void *obj, short bits);
extern void ref_txmitdibit(void *obj, short bits);
extern void ref_txmitquadbit(void *obj, short bits);
extern const int ref_vect4[4];
extern const int ref_vect16[16];
extern void ref_txinit(void *obj);
extern void ref_V34InitializeImplementationSpecific(void *obj);
extern void ref_V34SetupModulator(void *m, short baud, short carrier,
				  short a, short b, short c);

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

	diff_begin("v34 handshake: v34modeminit, both ends of the call");
	{
		/*
		 * 1356 bytes of straight-line stores over the whole object,
		 * so the whole-object compare is the test.  Both values of
		 * f359c, because the two configurations differ in five
		 * places and nothing else -- a reconstruction that folded
		 * them wrongly would be right for one end and wrong for the
		 * other.
		 *
		 * Run twice per case as well: it calls rxinit and txinit,
		 * neither of which is idempotent by construction, so a second
		 * pass over an already-initialised object exercises paths the
		 * first cannot.
		 */
		static const short flags[] = { 0x65, 0, 1, 0x64, 0x66, -1 };
		unsigned fi;

		for (fi = 0; fi < sizeof(flags) / sizeof(flags[0]); fi++) {
			setup();
			/*
			 * NO `shaped` POKE HERE.  The initialiser below
			 * aims +0x2074 into the object itself, which is
			 * what the original does, so the modulator's
			 * output lands where the byte compare can see it
			 * rather than in a buffer the test owns.
			 *
			 * `v34modeminit` calls `txinit`, which cleans both
			 * echo cancellers through five pointers each and
			 * their two lengths.  None of that is v34modeminit's
			 * to set: `V34InitializeImplementationSpecific` is
			 * what aims them, and running it first is what a
			 * real caller does rather than something the fixture
			 * invents.  Without it the first dereference faults.
			 */
			V34InitializeImplementationSpecific(&oa);
			ref_V34InitializeImplementationSpecific(ob);
			poke_short(0x359c, flags[fi]);
			poke_short(0x264 + 0x262, (short)(0x400 + fi));

			v34modeminit(&oa);
			ref_v34modeminit(ob);
			compare("v34modeminit", 5000 + flags[fi]);

			compare_table("v34modeminit band-pass",
				      (const short *)get_ptr_a(0x0508),
				      (const short *)get_ptr_b(0x0508),
				      V34_BPV22_TAPS, 5000 + flags[fi]);
			compare_table("v34modeminit detector coeff",
				      (const short *)get_ptr_a(0x3564),
				      (const short *)get_ptr_b(0x3564),
				      8, 5000 + flags[fi]);
			/*
			 * THE CARRIER TABLE HAS TO BE COMPARED BY CONTENT.
			 * hsine1200 and hsine2400 are both eight pairs long,
			 * so swapping the two phase-2 carriers leaves every
			 * scalar in the object identical and changes only
			 * which table the pointer selects -- which is
			 * exactly what a skipped pointer hides.  It did:
			 * that mutation passed until this check existed.
			 */
			{
				const struct v34_modulator *ma =
					(const struct v34_modulator *)
					((const char *)&oa + 0x1450);

				compare_table("v34modeminit carrier table",
					      (const short *)get_ptr_a(0x1460),
					      (const short *)get_ptr_b(0x1460),
					      ma->sine_len * 2,
					      5000 + flags[fi]);
			}
			/*
			 * And the detector really got the phase-2 pair, not
			 * one of the eight data-mode descriptors.
			 */
			diff_eq_int("the detector coeff is c1200_/c2400_",
				    get_ptr_a(0x3564)
				    == (void *)(flags[fi] == 0x65 ? c2400_
								  : c1200_),
				    1, flags[fi]);

			v34modeminit(&oa);
			ref_v34modeminit(ob);
			compare("v34modeminit twice", 5100 + flags[fi]);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: v34handshakinit, five modes and the tail");
	{
		/*
		 * Two of the five jump-table slots hold the same address, so
		 * 2 and 3 must reach the same body; everything outside 0..4
		 * must reach the tail, and that is a RANGE CHECK on an int,
		 * so both ends of the range matter.
		 */
		static const int modes[] = { 0, 1, 2, 3, 4, 5, 6, -1, -2,
					     0x7fffffff, (-0x7fffffff - 1) };
		unsigned mi;

		for (mi = 0; mi < sizeof(modes) / sizeof(modes[0]); mi++) {
			struct hsi_case c = hsi_base;

			c.mode = modes[mi];
			run_handshakinit(&c, 1000 + (long)mi);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: v34handshakinit's timer guard, all three arms");
	{
		/*
		 * `delta > 0` gates the block; the difference is then compared
		 * UNSIGNED against 95,999.  So there are four cases and the
		 * fourth is the one a signed reconstruction gets wrong: a base
		 * BELOW the delta makes the difference negative, which is far
		 * above the limit unsigned and takes the reset arm.
		 *
		 * +0x244 and +0x23c are written from the base AFTER the guard
		 * has run, so each arm has to be exercised for those two to be
		 * pinned as well.
		 */
		static const struct { int base, delta; } t[] = {
			{	     0,	         0 },	/* delta 0: skipped   */
			{	 50000,	   -100000 },	/* delta < 0: skipped */
			{	100000,	     50000 },	/* diff 50000: kept   */
			{	100000,	         1 },	/* diff 99999: reset  */
			{    0x176ff,	         0 },	/* skipped, at limit  */
			{	200000,	    104001 },	/* diff 95999: kept   */
			{	200000,	    104000 },	/* diff 96000: reset  */
			{	  1000,	     50000 },	/* negative: reset    */
			{	     0,	         1 },	/* -1: reset          */
			{	    -5,	         3 },	/* -8: reset          */
			{ 0x7fffffff,	0x7fffffff },	/* 0: kept            */
			{ -0x7fffffff,	0x7fffffff }	/* overflows: either  */
		};
		unsigned ti, mi;
		static const int modes[] = { 0, 2, 4, 9 };

		for (ti = 0; ti < sizeof(t) / sizeof(t[0]); ti++)
		for (mi = 0; mi < sizeof(modes) / sizeof(modes[0]); mi++) {
			struct hsi_case c = hsi_base;

			c.mode = modes[mi];
			c.timer_base = t[ti].base;
			c.timer_delta = t[ti].delta;
			run_handshakinit(&c, 2000 + (long)ti * 10 + mi);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: v34handshakinit's mode-1 branch, all four ways");
	{
		/*
		 * `(rx->flags & 0x40) || obj[0xac17]` picks between two
		 * counters, so the two inputs are swept INDEPENDENTLY -- they
		 * are exactly the shape finding 171 warns about, and driving
		 * them together would make `||` and `&&` indistinguishable.
		 * The arms write different fields, so all four are visible.
		 */
		unsigned fi, ai;
		static const unsigned short fl[] = { 0x0000, 0x0040, 0xffbf,
						     0xffff };
		static const unsigned char a17[] = { 0, 1, 0xff };

		for (fi = 0; fi < sizeof(fl) / sizeof(fl[0]); fi++)
		for (ai = 0; ai < sizeof(a17) / sizeof(a17[0]); ai++) {
			struct hsi_case c = hsi_base;

			c.mode = 1;
			c.rxflags = fl[fi];
			c.ac17 = a17[ai];
			/* Distinct, and distinct from each other, so a
			 * reconstruction that bumped the wrong counter shows
			 * up in the value and not only in the offset. */
			c.ac12 = (short)(0x0100 + fi);
			c.ac14 = (short)(0x0200 + ai);
			run_handshakinit(&c, 3000 + (long)fi * 10 + ai);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: v34handshakinit's f359c, three values");
	{
		/*
		 * THREE, NOT TWO.  `v34modeminit` and mode 4 test `== 0x65`;
		 * mode 0 tests `!= 0x66`.  A sweep of {0x65, anything else}
		 * would pass a reconstruction that used 0x65 in mode 0.
		 */
		static const short f[] = { 0x65, 0x66, 0x00, 0x64, 0x67, -1 };
		static const int modes[] = { 0, 1, 2, 4 };
		unsigned si, mi, vi;
		static const int v90[] = { 0, 1, -1 };

		for (si = 0; si < sizeof(f) / sizeof(f[0]); si++)
		for (mi = 0; mi < sizeof(modes) / sizeof(modes[0]); mi++)
		for (vi = 0; vi < sizeof(v90) / sizeof(v90[0]); vi++) {
			struct hsi_case c = hsi_base;

			c.mode = modes[mi];
			c.f359c = f[si];
			/* Mode 0's V34SetINFO0dBits writes index 12 of the
			 * same record mode 0 just wrote, so both arms of its
			 * own gate change what lands there. */
			c.v90_receiver = v90[vi];
			run_handshakinit(&c, 4000 + (long)si * 100
					 + (long)mi * 10 + vi);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: v34handshakinit's four flag-word masks");
	{
		/*
		 * The prologue does `txflags &= 0x7fff`; mode 2/3 does
		 * `txflags = (txflags & ~0x4018) | 0x2000`, `rxflags =
		 * (rxflags & ~0x1d8) | 0x18` and then `rxflags &= ~0x2000`;
		 * modes 0 and 1 do `rxflags |= 0x1000`.  All ones and all
		 * zeroes miss a swapped mask, so a mixed pattern is swept too.
		 */
		static const unsigned short pat[] = {
			0x0000, 0xffff, 0x5555, 0xaaaa, 0x4018, 0x21d8, 0x8000
		};
		/*
		 * AN OUT-OF-RANGE MODE IS IN THE LIST, and it is the only case
		 * that can see the prologue's `txflags &= 0x7fff` at all: every
		 * body either overwrites +0x25c2 outright (0, 1 and 4, through
		 * v34modeminit) or masks bit 14 off again (2 and 3).  Without
		 * mode 7 here, widening that mask to 0x3fff passes.
		 */
		static const int modes[] = { 0, 1, 2, 3, 4, 7 };
		unsigned pi, qi, mi;

		for (pi = 0; pi < sizeof(pat) / sizeof(pat[0]); pi++)
		for (qi = 0; qi < sizeof(pat) / sizeof(pat[0]); qi++)
		for (mi = 0; mi < sizeof(modes) / sizeof(modes[0]); mi++) {
			struct hsi_case c = hsi_base;

			c.mode = modes[mi];
			/* Swept SEPARATELY: one variable driving both would
			 * not tell the two masks apart. */
			c.rxflags = pat[pi];
			c.txflags = pat[qi];
			run_handshakinit(&c, 5000 + (long)pi * 100
					 + (long)qi * 10 + mi);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: v34handshakinit, MOH message and counters");
	{
		/*
		 * Mode 4 hands +0xa94c to VPcmV34SetMohMessageBits and then
		 * overwrites eleven of its fields, so the selector is swept
		 * over all six messages plus one out of range: a reconstruction
		 * that overwrote index 0 as well would pass at 9 and fail here.
		 */
		int sel;

		for (sel = -1; sel <= 6; sel++) {
			struct hsi_case c = hsi_base;

			c.mode = 4;
			c.moh_message = sel;
			run_handshakinit(&c, 6000 + sel);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: v34handshakinit's transcript, all 87 names");
	{
		/*
		 * THIS IS THE ONLY CHECK ON `StateName`.  The table is a LOCAL
		 * symbol, so `objcopy --redefine-syms` cannot make a
		 * `ref_StateName` for the fifteen-table comparison at the top
		 * of this file to copy; the strings are reachable only through
		 * what the traces print.  Finding 173's trap, and finding
		 * 176's -- delete this section and nothing checks the eighty-
		 * seven names at all.
		 *
		 * MODE 2 IS THE ONE THAT CAN SEE `[1]` AND `[2]`.  Modes 0, 1
		 * and 4 run v34modeminit first, which zeroes both counters
		 * before any trace fires; mode 2/3 clears them AFTER its three
		 * transitions, so it is the only body whose traces print
		 * anything else.
		 *
		 * The sweep stops at 86.  The object indexes StateName with no
		 * bound (D42), so an out-of-range state word would read past
		 * the table on both sides -- into different memory, which is
		 * not a comparison of anything.
		 */
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		for (i = 0; i < V34HS_STATE_COUNT; i++) {
			struct hsi_case c = hsi_base;

			c.mode = 2;
			c.mst = c.rxst = c.txst = (short)i;
			c.trace1 = (short)(0x1000 + i);
			c.trace2 = (short)(-0x2000 - (int)i);
			run_handshakinit_traced(&c, 7000 + (long)i);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: v34handshakinit's traces, three machines apart");
	{
		/*
		 * THE SWEEP ABOVE CANNOT TELL THE THREE MACHINES APART.  With
		 * all three words equal every `%s` carries the same string, so
		 * a reconstruction that fed the `mst` slot from rxstate would
		 * produce a byte-identical transcript.  That is the defect of
		 * finding 171 applied to the one thing the brief for this work
		 * said not to guess at, so the three are driven on three
		 * offset cycles here and are never equal.
		 *
		 * 29 and 58 are 87/3 and 2*87/3, so the three cycles are the
		 * three residues of i mod 87 spaced a third of the table
		 * apart: never equal, and every one of the 87 names still
		 * appears in each of the three slots across the sweep.  The
		 * assertion below is that, checked rather than asserted in
		 * prose.
		 */
		for (i = 0; i < V34HS_STATE_COUNT; i++) {
			struct hsi_case c = hsi_base;
			int distinct;

			c.mode = 2;
			c.txst = (short)i;
			c.rxst = (short)((i + 29) % V34HS_STATE_COUNT);
			c.mst  = (short)((i + 58) % V34HS_STATE_COUNT);
			/* [1] and [2] likewise: distinct from each other, and
			 * of opposite sign, so their order is visible too. */
			c.trace1 = (short)(0x0100 + i);
			c.trace2 = (short)(-0x0100 - (int)i);

			distinct = c.txst != c.rxst && c.rxst != c.mst
				   && c.txst != c.mst
				   && c.trace1 != c.trace2;
			diff_eq_int("the three machines were seeded apart",
				    distinct, 1, 7200 + (long)i);

			run_handshakinit_traced(&c, 7200 + (long)i);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: v34handshakinit's transcript, the other modes");
	{
		/*
		 * The other four bodies' traces, their two un-gated printfs
		 * ("initiating MOH negotiation" and the V34RNEG line, which
		 * prints both flag words) and mode 0's "V90, setINFO0dBits".
		 * Modes 0, 1 and 4 print [1] and [2] as zero whatever they
		 * were seeded to; that is v34modeminit's doing and is what
		 * makes mode 2 the only place the pair is testable.
		 */
		static const int modes[] = { 0, 1, 2, 3, 4, 7 };
		static const short f[] = { 0x65, 0x66, 0x00 };
		static const unsigned short fl[] = { 0x0000, 0x0040, 0xffff };
		unsigned mi, si, fi;

		for (mi = 0; mi < sizeof(modes) / sizeof(modes[0]); mi++)
		for (si = 0; si < sizeof(f) / sizeof(f[0]); si++)
		for (fi = 0; fi < sizeof(fl) / sizeof(fl[0]); fi++) {
			struct hsi_case c = hsi_base;

			c.mode = modes[mi];
			c.f359c = f[si];
			c.rxflags = fl[fi];
			c.txflags = (unsigned short)(0x1234 + fi);
			c.v90_receiver = (int)si;
			c.moh_message = (int)si;
			c.ac17 = (unsigned char)fi;
			/*
			 * Mode 7 reaches only the tail and prints nothing, so
			 * it is compared without the non-empty assertion.
			 */
			if (modes[mi] > 4) {
				dsplib_debug_capture_reset();
				run_handshakinit(&c, 7400 + (long)mi * 100
						 + (long)si * 10 + fi);
				diff_eq_int("v34handshakinit tail transcript",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, 7400 + (long)mi);
			} else {
				run_handshakinit_traced(&c,
							7400 + (long)mi * 100
							+ (long)si * 10 + fi);
			}
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
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

	diff_begin("v34 handshake constellations");
	{
		diff_eq_int("vect4", memcmp(vect4, ref_vect4, sizeof(vect4)),
			    0, 0);
		diff_eq_int("vect16",
			    memcmp(vect16, ref_vect16, sizeof(vect16)), 0, 0);
	}
	rc |= diff_end();

	/*
	 * txmitdibit and txmitquadbit.
	 *
	 * `quad` picks which of the two runs; `gpc` drives bit 0 of f25c2,
	 * which is the scrambler generator and the one branch inside them;
	 * `gate` drives bit 9, which is txmit's echo feed, so both settings
	 * of it exercise a different amount of the tail call.
	 *
	 * The bit patterns run past the field width on purpose -- txmitdibit
	 * takes two bits out of a short and txmitquadbit four, and neither
	 * masks what it was handed, so a caller passing 0xffff is a caller
	 * the object accepts.  The scrambler shifts ARITHMETICALLY, so a
	 * negative value feeds ones for ever rather than running out, and
	 * that is reachable from a plain -1.
	 */
	diff_begin("v34 txmitdibit/txmitquadbit");
	{
		static struct v34_object oa, ob;
		static short shp_a[512], shp_b[512];
		static short bra[64], brb[64];
		static const short bits[10] = { 0, 1, 2, 3, 5, 10, 15,
						0x5a5a, -1, 0x7fff };
		int quad, gpc, gate, it;

		for (quad = 0; quad <= 1; quad++)
		for (gpc = 0; gpc <= 1; gpc++)
		for (gate = 0; gate <= 1; gate++) {
			memset(&oa, 0, sizeof(oa)); memset(&ob, 0, sizeof(ob));
			memset(shp_a, 0, sizeof(shp_a));
			memset(shp_b, 0, sizeof(shp_b));
			memset(bra, 0, sizeof(bra));
			memset(brb, 0, sizeof(brb));

			V34InitializeImplementationSpecific(&oa);
			ref_V34InitializeImplementationSpecific(&ob);
			txinit(&oa); ref_txinit(&ob);

			((struct v34_modulator *)((char *)&oa + 0x1450))->shaped
				= shp_a;
			((struct v34_modulator *)((char *)&ob + 0x1450))->shaped
				= shp_b;
			V34SetupModulator((struct v34_modulator *)
					  ((char *)&oa + 0x1450), 2400, 1600,
					  0, 0, 1);
			ref_V34SetupModulator((char *)&ob + 0x1450, 2400,
					      1600, 0, 0, 1);

			oa.prefilter.coeff = ob.prefilter.coeff =
				V34TimingPrefilterCoeff;
			oa.prefilter.shift = ob.prefilter.shift = 14;
			oa.f25d4 = ob.f25d4 = 0x4000;
			oa.f25c2 = ob.f25c2 =
				(short)((gate ? 0x200 : 0) | (gpc ? 1 : 0));
			oa.bulk_ring = bra;  ob.bulk_ring = brb;
			oa.bulk_len  = ob.bulk_len = 64;

			/* A scrambler state that is not all zeroes. */
			oa.f25cc = ob.f25cc = 0x2f6b3d51;
			oa.f25c6 = ob.f25c6 = 2;

			for (it = 0; it < 40; it++) {
				short b = bits[it % 10];
				unsigned k;

				if (quad) {
					txmitquadbit(&oa, b);
					ref_txmitquadbit(&ob, b);
				} else {
					txmitdibit(&oa, b);
					ref_txmitdibit(&ob, b);
				}

				for (k = 0; k < sizeof(oa); k++) {
					/* Every pointer field: two objects. */
					if ((k >= 0x268 && k < 0x270)
					    || (k >= 0x2074 && k < 0x2078)
					    || (k >= 0x20cc && k < 0x20d0)
					    || (k >= 0x2220 && k < 0x2228)
					    || (k >= 0x35b0 && k < 0x35b4)
					    || (k >= 0x80b8 && k < 0x80d8)
					    || (k >= 0x9138 && k < 0x9158)
					    || (k >= 0x1450 + 0x10
						&& k < 0x1450 + 0x18)
					    || (k >= 0x1450 + 0xc24
						&& k < 0x1450 + 0xc28)
					    || (k >= 0x1450 + 0xc7c
						&& k < 0x1450 + 0xc80)
					    || (k >= 0x1450 + 0xcb0
						&& k < 0x1450 + 0xcb4))
						continue;
					/*
					 * Stride larger than the object, so
					 * (case, iteration, offset) stays
					 * unambiguous -- finding 116a.
					 */
					diff_eq_int("txmit* at %ld",
						    ((unsigned char *)&oa)[k],
						    ((unsigned char *)&ob)[k],
						    ((long)quad * 4
						     + gpc * 2 + gate)
						    * 100000000L
						    + (long)it * 100000 + k);
				}
				for (k = 0; k < 64; k++)
					diff_eq_int("bulk ring", bra[k],
						    brb[k], k);
				for (k = 0; k < 512; k++)
					diff_eq_int("shaped", shp_a[k],
						    shp_b[k], k);
			}
		}
	}
	rc |= diff_end();

	return rc;
}
