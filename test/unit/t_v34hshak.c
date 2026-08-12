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
extern void ref_dftfreqinit(struct v34_dftbin *bins);
extern void ref_dftnlinitSignalBins(struct v34_dftbin *bins);
extern void ref_dftnlinitNoiseBins(struct v34_dftbin *bins);
extern void ref_dftRetrainDetInit(void *obj);
extern int ref_detectRetrainReq(void *obj, short nbins, const short *samples,
				short nsamples);
extern void ref_v34modeminit(void *obj);
extern void ref_settxlevel(void *obj, const short *mp);
extern void ref_v34setuptxmit(void *obj);
extern void ref_probeselect(void *obj);
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
	0x0620, 0x0624,			/* timing +0x114 and +0x118    */
	/*
	 * And two the FIXTURE owns rather than the code under test, for
	 * `settxlevel`: it calls `GetVPcmMinimalTxPowerReduction`, which
	 * reads the configuration object at +0xac3c and walks the session at
	 * +0x3548 to the PCM receiver.  Nothing here writes either field.
	 * What justifies these two holes is that the memory BEHIND them is
	 * compared instead, per case, in `run_settxlevel`.
	 *
	 * THEY ARE ONLY HOLES WHEN THE FIXTURE HAS SEEDED THEM.  Every other
	 * block in this file leaves both fields at the fill pattern on both
	 * sides, where they are comparable and a stray write to either would
	 * show -- so `skipped()` opens these two only after `seed_pwr`, and
	 * they must be LAST in the list for that test to be an index
	 * comparison.  Without it the hole is open for `v34handshakinit` too,
	 * which is 12 KB of stores nothing here seeds those fields for.
	 */
	0x3548, 0xac3c
};
#define NPTR (sizeof(ptr_skip) / sizeof(ptr_skip[0]))
#define NPTR_ALWAYS (NPTR - 2)

static int saw_ptr_skip[NPTR];

/* Set by `seed_pwr`, cleared by `setup`: are the last two entries live? */
static int pwr_seeded;

static int
skipped(unsigned off)
{
	unsigned k;

	for (k = 0; k < (pwr_seeded ? NPTR : NPTR_ALWAYS); k++)
		if (off >= ptr_skip[k] && off < ptr_skip[k] + 4)
			return 1;
	return 0;
}

static void
setup(void)
{
	pwr_seeded = 0;
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

/* --- settxlevel's and v34setuptxmit's inputs ------------------------------ */

/*
 * The two blocks `GetVPcmMinimalTxPowerReduction` reaches out to, per side.
 * `settxlevel` calls it unconditionally, so every case here needs them --
 * and it WRITES the PCM one, so the two copies are compared as well as read.
 */
#define SESS_LEN	0x6200		/* indexed at +0x610c and +0x6120 */
#define PCM_LEN		0x0520		/* indexed at +0x4f4 and +0x4f8   */
#define CFG_LEN		0x0080		/* indexed at +0x44 and +0x54     */
#define BLOCK_FILL	0x3c

#define SESS_PCM	0x610c
#define SESS_GATE	0x6120
#define PCM_FLAG	0x04f4
#define PCM_SENS	0x04f8

static unsigned char sess_a[SESS_LEN], sess_b[SESS_LEN];
static unsigned char pcm_a[PCM_LEN], pcm_b[PCM_LEN];
static unsigned char cfg_a[CFG_LEN], cfg_b[CFG_LEN];

static int saw_pwr_high, saw_pwr_low;

static void
put_ptr(unsigned char *base, unsigned off, void *p)
{
	memcpy(base + off, &p, sizeof(p));
}

/*
 * Seed the chain and the configuration.  `want` is the reduction the PCM
 * configuration asks for and `sens` the ISP bit that decides whether the
 * K56Flex word gets a say; both are `GetVPcmMinimalTxPowerReduction`'s
 * inputs rather than `settxlevel`'s, and they are here because that call is
 * what makes this function's answer depend on more than the MP message.
 */
static void
seed_pwr(short want, int sens, int gate, int flag54)
{
	memset(sess_a, BLOCK_FILL, sizeof(sess_a));
	memset(sess_b, BLOCK_FILL, sizeof(sess_b));
	memset(pcm_a, BLOCK_FILL, sizeof(pcm_a));
	memset(pcm_b, BLOCK_FILL, sizeof(pcm_b));
	memset(cfg_a, BLOCK_FILL, sizeof(cfg_a));
	memset(cfg_b, BLOCK_FILL, sizeof(cfg_b));

	put_ptr(sess_a, SESS_PCM, pcm_a);
	put_ptr(sess_b, SESS_PCM, pcm_b);
	memcpy(sess_a + SESS_GATE, &gate, sizeof(gate));
	memcpy(sess_b + SESS_GATE, &gate, sizeof(gate));
	memcpy(pcm_a + PCM_SENS, &sens, sizeof(sens));
	memcpy(pcm_b + PCM_SENS, &sens, sizeof(sens));
	memcpy(cfg_a + 0x44, &want, sizeof(want));
	memcpy(cfg_b + 0x44, &want, sizeof(want));
	memcpy(cfg_a + 0x54, &flag54, sizeof(flag54));
	memcpy(cfg_b + 0x54, &flag54, sizeof(flag54));

	poke_ptr(0x3548, sess_a, sess_b);
	poke_ptr(0xac3c, cfg_a, cfg_b);
	pwr_seeded = 1;
}

/* Compare the two blocks, with the session's one pointer field excluded. */
static void
compare_pwr_blocks(const char *what, long tag)
{
	unsigned i;
	int bad = 0;
	int f;

	for (i = 0; i < SESS_LEN; i++) {
		if (sess_a[i] == sess_b[i]
		    || (i >= SESS_PCM && i < SESS_PCM + 4))
			continue;
		bad++;
		if (bad <= 4)
			diff_eq_int(what, sess_a[i], sess_b[i],
				    (long)i * 1000 + tag);
	}
	diff_eq_int(what, bad, 0, tag);

	bad = 0;
	for (i = 0; i < PCM_LEN; i++) {
		if (pcm_a[i] == pcm_b[i])
			continue;
		bad++;
		if (bad <= 4)
			diff_eq_int(what, pcm_a[i], pcm_b[i],
				    (long)i * 1000 + tag);
	}
	diff_eq_int(what, bad, 0, tag);

	/* The configuration is an input; neither side may write it. */
	diff_eq_int(what, memcmp(cfg_a, cfg_b, CFG_LEN) == 0, 1, tag);

	memcpy(&f, pcm_a + PCM_FLAG, sizeof(f));
	if (f == 0)
		saw_pwr_high = 1;
	else if (f == 1)
		saw_pwr_low = 1;
}

/* --- probeselect's inputs ------------------------------------------------- */

static void seed_rate_pointers(void);

/*
 * `probeselect` is a decision tree over twenty-five bins crossed with a role
 * flag, eight capability bytes and five scalars, and no hand-written case
 * list covers it.  So the sweep is PSEUDO-RANDOM OVER EVERY DRIVING FIELD,
 * from a fixed generator both sides are fed from -- the whole-object
 * comparison is what turns a random input into a check, and the coverage
 * counters below are what say the random inputs reached the arms.
 *
 * The generator is an LCG rather than `rand`, so the sweep is the same on
 * every host and in every build: a differential test that drifts between runs
 * cannot be bisected.
 */
static unsigned probe_rng;

static unsigned
probe_next(void)
{
	probe_rng = probe_rng * 1103515245u + 12345u;
	return probe_rng >> 8;
}

/*
 * RANDOM ALONE DOES NOT REACH A THRESHOLD AT 499.
 *
 * `probe_next` yields 24 bits, so a uniform draw is never negative and never
 * small: `snr_l1 <= 0x1f3` has a chance of one in thirty thousand and
 * `snr_l2 < 0` has none at all.  Sixteen mutations survived four thousand
 * cases on exactly that -- every threshold in the power section, both
 * sensitive arms, and the bit scan's top half.
 *
 * So each scalar is drawn from a pool of its own boundaries three times in
 * four, and uniformly otherwise.  The pools are the constants the function
 * compares against, one either side.
 */
static int
probe_pick(const int *pool, unsigned n)
{
	if ((probe_next() & 3) != 0)
		return pool[probe_next() % n];
	return (int)probe_next() - 0x800000;
}

static const int probe_l1[] = {
	0, 1, 0x1f2, 0x1f3, 0x1f4, 0x3e6, 0x3e7, 0x3e8, 0x1000,
	0x18fff, 0x19000, 0x100000, -1, -0x1f3, 0x7fffffff
};
static const int probe_l2[] = {
	0, 1, -1, 2, 0x100, 0x100000, 0x200000, 0x1fffff, 0x20000000,
	0x40000000, 0x7fffffff, (-0x7fffffff - 1), 0x400, 0x4000
};
static const int probe_gain[] = {
	0, 1, 0xffe, 0xfff, 0x1000, 0x1001, 0x7fff, -1, -0x1000, 0x4000
};

/* Which arm each side took, by what it left behind. */
static int saw_rate[6];			/* 2400 2800 3000 3200 3429 none */
static int saw_preemph[12];		/* every index a search returned */

static void
probe_note(void)
{
	const unsigned char *p = (const unsigned char *)&oa;
	short baud;
	int k;

	memcpy(&baud, p + 0xaa84, sizeof(baud));
	switch (baud) {
	case 0x960: saw_rate[0] = 1; break;
	case 0xaf0: saw_rate[1] = 1; break;
	case 0xbb8: saw_rate[2] = 1; break;
	case 0xc80: saw_rate[3] = 1; break;
	case 0xd65: saw_rate[4] = 1; break;
	default:    saw_rate[5] = 1; break;
	}

	for (k = 0; k < 5; k++) {
		static const unsigned off[5] = { 0xaa9a, 0xaa9c, 0xaaa0,
						 0xaaa2, 0xaaa4 };
		short v;

		memcpy(&v, p + off[k], sizeof(v));
		if (v >= 0 && v < 12)
			saw_preemph[v] = 1;
	}
}

/*
 * One case.  Everything the function reads is driven, and driven separately:
 * the twenty-five shifts and the energies the pre-emphasis search uses are
 * independent inputs, and so are the eight capability bytes, the role flag
 * and the five scalars of the power section.
 *
 * `spread` narrows the shifts towards zero.  The ladder's thresholds are 5, 6
 * and 10, so a uniform 16-bit shift takes the same arm every time and the
 * interesting region is a handful of small integers -- which is also the
 * shape the function's own normalisation leaves behind, since it subtracts
 * the minimum before any of the tests.  One shift in thirty-two is left wide
 * anyway, because the normalisation's signed/unsigned mismatch (D54) needs a
 * negative one to show.
 */
static void
run_probeselect(unsigned seed, int spread, long tag)
{
	unsigned i;
	int high_bins;

	probe_rng = seed;

	setup();
	seed_rate_pointers();
	seed_pwr((short)((int)(probe_next() % 32) - 16),
		 (int)(probe_next() & 1), (int)(probe_next() & 1), 4);
	/* Likewise bit 4, which gates the sensitive-ISP arms entirely. */
	cfg_a[0x50] = cfg_b[0x50] =
		(unsigned char)(probe_next() | ((probe_next() & 1) ? 0x10 : 0));

	/*
	 * One case in eight puts every shift above 0x20, which is what the
	 * minimum search starts from -- otherwise some bin is always smaller
	 * and the starting value is never the answer.
	 */
	high_bins = (probe_next() & 7) == 0;

	for (i = 0; i < V34_PROBE_BINS; i++) {
		unsigned off = 0xa320 + i * sizeof(struct v34_dftbin);
		short sh = (short)(probe_next() % (unsigned)spread);

		if (high_bins)
			sh = (short)(0x21 + probe_next() % 16);
		else if ((probe_next() & 0x1f) == 0)
			sh = (short)probe_next();

		poke_short(off + 0x0c, (short)probe_next());
		poke_short(off + 0x0e, sh);
	}

	poke_int(0xaac4, probe_pick(probe_l1, sizeof(probe_l1)
					       / sizeof(probe_l1[0])));
	poke_int(0xaac8, probe_pick(probe_l2, sizeof(probe_l2)
					       / sizeof(probe_l2[0])));
	poke_short(0x264 + 0x136,
		   (short)probe_pick(probe_gain, sizeof(probe_gain)
						 / sizeof(probe_gain[0])));
	poke_short(0x264 + 0x262, (short)probe_next());
	/*
	 * Bit 7 is the one the power section tests, so it is set half the
	 * time rather than one time in two hundred and fifty-six.
	 */
	poke_byte(0xa97e, (unsigned char)(probe_next()
					  | ((probe_next() & 1) ? 0x80 : 0)));

	for (i = 0xa9de; i <= 0xa9ee; i++)
		poke_byte(i, (unsigned char)probe_next());

	poke_short(0x359a, (short)((probe_next() & 3) == 0 ? 1 : 0));
	poke_short(0x359c, (short)((probe_next() & 1) ? 0x65 : 0x11));

	probeselect(&oa);
	ref_probeselect(ob);

	compare("probeselect", tag);
	compare_pwr_blocks("probeselect blocks", tag);

	/*
	 * The three table pointers, by CONTENT.  Each answering arm installs
	 * a scale table and a carrier descriptor by address, and the two
	 * candidates of every pair are the same length -- so swapping them
	 * leaves every scalar in the object identical, which is exactly what
	 * a skipped pointer hides.  Seeded with the per-side dummy, so "left
	 * alone" is a comparable answer too.
	 */
	compare_table("probeselect tx scale",
		      (const short *)get_ptr_a(0xaa90),
		      (const short *)get_ptr_b(0xaa90), 28, tag);
	compare_table("probeselect rx scale",
		      (const short *)get_ptr_a(0xaaac),
		      (const short *)get_ptr_b(0xaaac), 28, tag);
	compare_table("probeselect rx carrier desc",
		      (const short *)get_ptr_a(0xaab0),
		      (const short *)get_ptr_b(0xaab0), 8, tag);

	probe_note();
}


/*
 * --- and the equalities, which random draws cannot reach -----------------
 *
 * Five mutations survived four thousand biased probes, and every one of them
 * turns on an exact equality: `ratio` landing on a term of the dB ladder, or
 * the scaled bin energy landing exactly on the reference.  Those are single
 * points in a 32-bit space, so they are CONSTRUCTED rather than sampled.
 *
 * `ratio` is controllable.  With `snr_l2` small enough that the bit scan runs
 * out -- anything below 0x200000 -- the shift is ten and the quotient is
 * `((snr_l2 << 10) + (snr_l1 >> 1)) / snr_l1`, so `snr_l1 = 0x400` makes the
 * ratio equal `snr_l2` exactly.  That turns "drive the dB loop to its own
 * boundary" into one assignment.
 */
static void
run_probe_ratio(unsigned want, short gain, long tag)
{
	unsigned i;

	probe_rng = 0x5eed1234u;

	setup();
	seed_rate_pointers();
	seed_pwr(3, 1, 1, 4);
	cfg_a[0x50] = cfg_b[0x50] = 0x10;

	for (i = 0; i < V34_PROBE_BINS; i++) {
		unsigned off = 0xa320 + i * sizeof(struct v34_dftbin);

		poke_short(off + 0x0c, (short)probe_next());
		poke_short(off + 0x0e, (short)(probe_next() % 9));
	}

	poke_int(0xaac4, 0x400);
	poke_int(0xaac8, (int)want);
	poke_short(0x264 + 0x136, gain);
	poke_short(0x264 + 0x262, 0x100);
	poke_byte(0xa97e, 0x80);
	for (i = 0xa9de; i <= 0xa9ee; i++)
		poke_byte(i, (unsigned char)probe_next());
	poke_short(0x359a, 0);
	poke_short(0x359c, (short)((probe_next() & 1) ? 0x65 : 0x11));

	probeselect(&oa);
	ref_probeselect(ob);

	compare("probeselect ratio", tag);
	compare_pwr_blocks("probeselect ratio blocks", tag);
	probe_note();
}

/*
 * The other equality is inside the pre-emphasis search: `x > ref` versus
 * `x >= ref` differ only when one scaled step lands exactly on the reference.
 * The step is `(x * k) >> 14`, so the pre-image of a chosen reference is
 * `(ref << 14) / k`, and driving the candidate bin to that value and its
 * neighbours puts the comparison on its own boundary for each of the five
 * per-rate constants.
 */
static void
run_probe_preemph_edge(unsigned bin, int k, short ref, int delta, long tag)
{
	unsigned i;
	int pre = (int)(((long long)ref << 14) / k) + delta;

	probe_rng = 0xc0ffee11u;

	setup();
	seed_rate_pointers();
	seed_pwr(3, 1, 1, 4);
	cfg_a[0x50] = cfg_b[0x50] = 0;

	for (i = 0; i < V34_PROBE_BINS; i++) {
		unsigned off = 0xa320 + i * sizeof(struct v34_dftbin);

		poke_short(off + 0x0c, 0);
		poke_short(off + 0x0e, (short)(probe_next() % 7));
	}
	poke_short(0xa320 + 4 * sizeof(struct v34_dftbin) + 0x0c, ref);
	poke_short(0xa320 + bin * sizeof(struct v34_dftbin) + 0x0c,
		   (short)pre);

	poke_int(0xaac4, 0);
	poke_int(0xaac8, 0);
	poke_short(0x264 + 0x136, 0x800);
	poke_short(0x264 + 0x262, 0x100);
	poke_byte(0xa97e, 0);
	for (i = 0xa9de; i <= 0xa9ee; i++)
		poke_byte(i, 0xff);
	poke_short(0x359a, 0);
	poke_short(0x359c, (short)((probe_next() & 1) ? 0x65 : 0x11));

	probeselect(&oa);
	ref_probeselect(ob);

	compare("probeselect preemph edge", tag);
	compare_pwr_blocks("probeselect preemph edge blocks", tag);
	probe_note();
}

/*
 * THE PRE-EMPHASIS SEARCH IS A TILT METER.  Drive it with a channel whose
 * band edge sits a KNOWN number of steps below the reference bin, and check
 * the index that comes back is the number of steps -- which is what finding
 * 1475 derived from the constants and this asserts against the object.
 *
 * `k` is a GAIN: 0x6626 is 26150, so `x * k >> 14` multiplies by 1.5961 and
 * the loop steps the band edge UP until it passes mid-band.  One step is
 * 20*log10(k/16384) dB -- 4.06 dB at 3429 baud.
 *
 * To need exactly `steps` iterations, divide the reference by the gain that
 * many times and then nudge a quarter-step up: the result clears `ref` on
 * iteration `steps` and not before, with half a step of margin either side so
 * the loop's per-iteration truncation to short cannot flip the bucket.
 *
 * The counter is preset to 5 and advanced BEFORE the test (D53), so the index
 * returned is steps + 5, saturating at 10.  That is the whole defect in one
 * assertion: a channel needing ONE step -- no tilt worth correcting -- comes
 * back as 6 rather than 0.
 */
static void
run_preemph_tilt(unsigned bin, int k, short ref, int steps, long tag)
{
	unsigned i;
	long long x = ref;
	int expect = steps + 5;

	if (expect > 10)
		expect = 10;
	for (i = 0; i < (unsigned)steps; i++)
		x = x * 16384 / k;
	x = x * 5 / 4;

	probe_rng = 0x5eed1234u;
	setup();
	seed_rate_pointers();
	seed_pwr(3, 1, 1, 4);
	cfg_a[0x50] = cfg_b[0x50] = 0;

	for (i = 0; i < V34_PROBE_BINS; i++) {
		unsigned off = 0xa320 + i * sizeof(struct v34_dftbin);

		poke_short(off + 0x0c, 0);
		poke_short(off + 0x0e, (short)(probe_next() % 7));
	}
	poke_short(0xa320 + 4 * sizeof(struct v34_dftbin) + 0x0c, ref);
	poke_short(0xa320 + bin * sizeof(struct v34_dftbin) + 0x0c, (short)x);

	poke_int(0xaac4, 0);
	poke_int(0xaac8, 0);
	poke_short(0x264 + 0x136, 0x800);
	poke_short(0x264 + 0x262, 0x100);
	poke_byte(0xa97e, 0);
	for (i = 0xa9de; i <= 0xa9ee; i++)
		poke_byte(i, 0xff);
	poke_short(0x359a, 0);
	poke_short(0x359c, 0x65);

	dsplib_debug_capture_reset();
	probeselect(&oa);
	ref_probeselect(ob);

	compare("probeselect tilt sweep", tag);

	/*
	 * READ THE INDEX THE OBJECT ITSELF PRINTS, rather than inferring it
	 * from a slot whose meaning would have to be assumed.  `compare` above
	 * already proves ours and the blob's agree; this pins the VALUE, which
	 * is the part a future change could alter while both sides still
	 * matched each other.
	 */
	{
		const char *txt = dsplib_debug_capture_text(1);
		const char *m = strstr(txt, "index is ");
		int got = -1;

		/*
		 * Forward, not backward from the baud rate: scanning back for
		 * 'i' finds the one in "is" long before it reaches "index",
		 * which is how the first version of this read -1 every time.
		 */
		while (m != NULL) {
			const char *b = strstr(m, "baudrate= ");

			if (b != NULL && atoi(b + 10) == 3429)
				got = atoi(m + 9);
			m = strstr(m + 1, "index is ");
		}
		diff_eq_int("tilt of N steps gives index N+5 (D53)",
			    got, expect, tag);
	}
	probe_note();
}

/* --- setfinalrate's inputs ------------------------------------------------ */

static void
seed_rate_pointers(void)
{
	poke_ptr(0xaa90, dummy_a, dummy_b);
	poke_ptr(0xaaac, dummy_a, dummy_b);
	poke_ptr(0xaab0, dummy_a, dummy_b);
}

/*
 * One `settxlevel` case.  The MP short, the scale it starts from, and every
 * input of the `GetVPcmMinimalTxPowerReduction` call it opens with -- driven
 * separately, because the two combine three different ways and a fixture
 * that tied them together could not tell the three apart.
 */
static void
run_settxlevel(unsigned short mp, short scale, short want, int sens, int gate,
	       int flag54, int v90, int k56, long tag)
{
	setup();
	seed_pwr(want, sens, gate, flag54);
	poke_short(0xa9dc, (short)mp);
	poke_short(0x25d4, scale);
	poke_int(0x24c, v90);
	poke_int(0x250, k56);

	settxlevel(&oa, (const short *)((const char *)&oa + 0xa9dc));
	ref_settxlevel(ob, (const short *)(ob + 0xa9dc));

	compare("settxlevel", tag);
	compare_pwr_blocks("settxlevel blocks", tag);
}

/*
 * One `v34setuptxmit` case.
 *
 * `V34InitializeImplementationSpecific` first, for the reason the
 * `v34modeminit` block gives: `txinit` cleans both echo cancellers through
 * five pointers each, and without the initialiser aiming them the first
 * dereference faults.  That is what a real caller does, not something the
 * fixture invents.
 *
 * The three state words are seeded in range and DIFFERENT from each other --
 * the two transitions print the other two machines' names, and equal words
 * make the three indistinguishable (D42 is why in-range matters at all).
 */
static void
run_setuptxmit(short baud, short carrier, short preemp, int v90, int k56,
	       short rxstate, short txstate, short mst, long tag)
{
	setup();
	seed_pwr(3, 1, 1, 4);
	V34InitializeImplementationSpecific(&oa);
	ref_V34InitializeImplementationSpecific(ob);

	poke_short(0xa9dc, (short)0x00e4);
	poke_short(0x25d4, 0x16a1);
	poke_short(0xaa84, baud);
	poke_short(0xaa94, carrier);
	poke_short(0xaa8a, preemp);
	poke_int(0x24c, v90);
	poke_int(0x250, k56);
	poke_short(0x3592, mst);
	poke_short(0x3594, rxstate);
	poke_short(0x3596, txstate);
	poke_short(0x2aa2, (short)0x1111);
	poke_short(0xaa78, (short)0x2222);
	poke_short(0x264 + 0x122, (short)0xffff);
	poke_short(0x25c2, (short)0x1234);

	v34setuptxmit(&oa);
	ref_v34setuptxmit(ob);

	compare("v34setuptxmit", tag);
	compare_pwr_blocks("v34setuptxmit blocks", tag);

	/*
	 * The modulator's two tables, by CONTENT: `V34SetupModulator`
	 * installs a shaping table and a pre-emphasis one by address, and an
	 * address comparison cannot see which of them was chosen.  Same
	 * argument as the carrier table in the `v34modeminit` block.
	 */
	compare_table("v34setuptxmit ec_prem",
		      (const short *)get_ptr_a(0x20cc),
		      (const short *)get_ptr_b(0x20cc), 16, tag);
	compare_table("v34setuptxmit preemp",
		      (const short *)get_ptr_a(0x2100),
		      (const short *)get_ptr_b(0x2100), 16, tag);
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

static short
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
	return got;
}

/*
 * One case with the transcripts captured and compared.
 *
 * IT RETURNS THE INDEX because the transcript cannot say which exit ran:
 * both of them can produce a 10, and the only difference between the two
 * lines is one space.  A caller that wants to claim it drove a particular
 * site needs the index and the reference's text, not either alone.
 */
static short
run_preempindex_traced(short limit, short meas, short baud, long tag)
{
	short got;

	dsplib_debug_capture_reset();
	got = run_preempindex(limit, meas, baud, tag);
	diff_eq_int("preempindex transcript",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("preempindex transcript non-empty",
		    dsplib_debug_capture_text(1)[0] != 0, 1, tag);
	diff_eq_int("and ours printed too",
		    dsplib_debug_capture_text(0)[0] != 0, 1, tag);
	return got;
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

/* --- the two the object keeps file-local --------------------------------- */

/*
 * THE CONVENTION, WHICH FINDING 51 SAYS TO CHECK BEFORE CALLING ANY `t`
 * SYMBOL.  Both of these are local in the object, so GCC gave them the local
 * calling convention -- arguments in registers -- and the blob's copies still
 * want it even though `--globalize-symbols` has made them linkable
 * (finding 221).  The disassembly says which:
 *
 *     5eaf0:  sub    $0x2c,%esp
 *     5eaf7:  mov    %eax,%esi          <- getbit's only argument, in eax
 *
 *     5dd10:  sub    $0x1c,%esp
 *     5dd17:  movswl %dx,%esi           <- ApplyBulkDelay's delay, in dx
 *     5dd21:  mov    %eax,%ebx             and the object in eax
 *
 * and from the caller's side, at 0x66398:
 *
 *     movswl 0xaa7e(%edi),%edx ; call 5dd10
 *
 * -- so regparm(1) and regparm(2).  The attribute goes on the REFERENCE
 * declaration only: our own copies have external linkage and the ordinary
 * convention, which is the same split t_v34demod.c documents.
 */
extern short ref_getbit(struct v34_bitsource *b) __attribute__((regparm(1)));
extern void ref_ApplyBulkDelay(void *obj, short delay)
	__attribute__((regparm(2)));

/*
 * `getbit` -- both sides driven from one seed, compared after every call.
 *
 * The struct has no padding, so assigning the seed leaves nothing
 * uninitialised and the whole 0x30 bytes are comparable.  The fill pattern is
 * deliberately NOT used here and would be actively harmful: `idx` is an array
 * subscript with nothing bounding it, and 0xa5a5 is a wild read.
 *
 * ANTI-VACUITY.  `saw_recursion` is set when a call increments `repeats`,
 * which only the restart arm does -- and the restart arm is the one that
 * calls `getbit` again.  A run in which no case ever restarted would exercise
 * the function's straight line and none of its recursion, which is exactly
 * the test the brief warns against, so main asserts the flag at the end.  The
 * other three flags do the same job for the arms that are easy to miss: the
 * -1 return, the CRC appended after the message, and the four-bit filler that
 * follows the CRC.
 */
static int saw_recursion, saw_minus_one, saw_crc_tail, saw_filler;

static void
run_getbit(const char *what, long tag, const struct v34_bitsource *seed,
	   int calls)
{
	struct v34_bitsource a, b;
	int i;

	a = *seed;
	b = *seed;

	for (i = 0; i < calls; i++) {
		short before = a.repeats;
		short pos_before = a.pos;
		short ra, rb;

		ra = getbit(&a);
		rb = ref_getbit(&b);

		if (a.repeats != before)
			saw_recursion = 1;
		if (ra == -1)
			saw_minus_one = 1;
		/*
		 * The CRC tail moves `pos` sixteen bits at once and leaves
		 * fifteen available; the filler moves it four and leaves
		 * three.  Neither is reachable by the ordinary refill, whose
		 * step is `wordbits`, so both are recognisable from `pos` and
		 * `avail` without reaching into the function.
		 */
		if (a.crc_on != 0 && a.avail == 15
		    && (short)(a.pos - pos_before) == 16)
			saw_crc_tail = 1;
		if (a.avail == 3 && (short)(a.pos - pos_before) == 4
		    && a.acc == 0xf)
			saw_filler = 1;

		diff_eq_int(what, ra, rb, tag * 1000 + i);
		diff_eq_obj(what, struct v34_bitsource, &a, &b,
			    tag * 1000 + i);
	}
}

/* A message of ten words, distinct and not symmetric under reversal. */
static void
seed_bits(struct v34_bitsource *b, short nbits, short wordbits, short crc_on,
	  short repeat)
{
	int i;

	memset(b, 0, sizeof(*b));
	for (i = 0; i < V34_BITSOURCE_WORDS; i++)
		b->word[i] = (short)(0x8d51 * (i + 1) + i * 7);
	b->crc = (short)0xffff;
	b->crc_on = crc_on;
	b->nbits = nbits;
	b->wordbits = wordbits;
	b->repeat = repeat;
}

/*
 * `ApplyBulkDelay` -- the whole object, both sides, per case.
 *
 * `bulk_len` is kept well inside the ring at +0x35b8: the clear is
 * `delay * 2` bytes from there and +0x80b8 is the next mapped field, so a
 * length of 0x400 leaves room for the largest delay it can pass.
 */
static void
run_bulkdelay(short delay, int v90, int k56, short far_on, int bulk_len,
	      short dma, long tag)
{
	unsigned k;

	setup();
	poke_int(0x024c, v90);
	poke_int(0x0250, k56);
	poke_int(0x35a8, 0x11111111);		/* head, to see it cleared  */
	poke_int(0x35ac, 0x22222222);		/* tail, likewise           */
	poke_int(0x35b4, bulk_len);
	poke_short(0xa23c, far_on);
	poke_short(0x025c, dma);

	/* A pattern in the ring, so a clear of the wrong length shows. */
	for (k = 0; k < 0x400; k++)
		poke_short(0x35b8 + k * 2, (short)(0x3000 + k));

	ApplyBulkDelay(&oa, delay);
	ref_ApplyBulkDelay(ob, delay);
	compare("ApplyBulkDelay", tag);
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

	/*
	 * THE TWO REACHABLE EXITS ANNOUNCE THEMSELVES, and until this block
	 * neither announcement had ever run.  Everything above drives
	 * `preempindex` at level 0, where every gate is false, so both of
	 * its live sites executed zero times over the whole suite and
	 * `tools/debugcov.py` named v34hshak.c:823 and :834 as the file's
	 * last two dead sites.  The function being under test is not the
	 * same thing as its diagnostics being driven.
	 *
	 * The third gate does not appear in that list at all, and its
	 * absence is not evidence of anything: "index is 0" is D36's dead
	 * branch, GCC folds the `i == 5` body away, and gcov marks an
	 * eliminated body NOT EXECUTABLE rather than executed-zero-times.
	 * Finding 219, which nearly published the opposite.
	 *
	 * WHAT SEPARATES THE TWO LIVE SITES IS ONE SPACE.  The object
	 * prints `baudrate= %d\n` where the multiply passed the limit and
	 * `baudrate= %d \n` where the loop ran out, and BOTH can return 10
	 * -- so the index does not say which site produced a line, and no
	 * assertion here quotes a string of ours.  The blob's own capture
	 * is the oracle, and the three cases are chosen so that it can be:
	 *
	 *   (0, 100, b)      the first multiply passes a zero limit, so
	 *                    this is the compare exit at index 6
	 *   (32767, 0, b)    no short is greater than 32767, so the
	 *                    compare can never be taken and the loop must
	 *                    run out: the other exit, index 10
	 *   (100, 7, 2400)   the compare exit AT index 10
	 *
	 * The last two return the same index for the same baud rate and
	 * must still print DIFFERENT lines.  That is the check that makes
	 * this a test of two sites rather than of one: a reconstruction
	 * that used either string at both exits, or dropped the space,
	 * agrees with everything else in this file.
	 */
	diff_begin("v34 handshake: preempindex names the index it found");
	{
		static const short b[] = { 2400, 2800, 3000, 3200, 3429 };
		unsigned lvl, k;
		int saw_cmp = 0, saw_brk = 0;

		dsplib_debug_capture_on = 1;

		for (lvl = 2; lvl <= 3; lvl++) {
			char brk10_ours[256], brk10_ref[256];

			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;

			for (k = 0; k < sizeof(b) / sizeof(b[0]); k++) {
				long t = (long)lvl * 1000 + k;

				if (run_preempindex_traced(0, 100, b[k],
							   21000 + t) == 6)
					saw_cmp = 1;
				if (run_preempindex_traced(32767, 0, b[k],
							   22000 + t) == 10)
					saw_brk = 1;
			}

			/*
			 * The pair that shares an index and not a site.
			 * BOTH SIDES' TEXT IS COPIED OUT, and each side is
			 * then compared with ITSELF at the other exit: our
			 * compare-exit line against the reference's
			 * loop-exhausted one would pass with our two exits
			 * printing the same thing, which is the mutant this
			 * check exists for.  The second run resets the
			 * buffer both texts live in.
			 */
			diff_eq_int("the loop-exhausted exit returned 10",
				    run_preempindex_traced(32767, 0, 2400,
							   23000 + (long)lvl),
				    10, (long)lvl);
			snprintf(brk10_ours, sizeof(brk10_ours), "%s",
				 dsplib_debug_capture_text(0));
			snprintf(brk10_ref, sizeof(brk10_ref), "%s",
				 dsplib_debug_capture_text(1));

			diff_eq_int("and so did the compare exit",
				    run_preempindex_traced(100, 7, 2400,
							   23100 + (long)lvl),
				    10, (long)lvl);
			diff_eq_int("the object says the two exits apart",
				    strcmp(dsplib_debug_capture_text(1),
					   brk10_ref) != 0, 1, (long)lvl);
			diff_eq_int("and so do we",
				    strcmp(dsplib_debug_capture_text(0),
					   brk10_ours) != 0, 1, (long)lvl);
		}

		/*
		 * The two witnesses, and only two: a third saying "the
		 * distinctness pair ran" would be set unconditionally inside
		 * a loop with constant bounds and could not fail, which is a
		 * comment rather than a check (gates.md, rule 2).  These two
		 * can fail -- they are what says the designated cases still
		 * land on the exits they are named for.
		 */
		diff_eq_int("the compare exit ran with the level up",
			    saw_cmp, 1, 0);
		diff_eq_int("the loop-exhausted exit ran too", saw_brk, 1, 0);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	/*
	 * The three DFT bank initialisers.
	 *
	 * Each takes the bank as an argument and nothing in the object calls
	 * any of them, so the bank size is read out of the code and the test
	 * has to check that reading rather than assume it: the arrays below
	 * are LONGER than the largest bank and poisoned past it, so a
	 * reconstruction that ran one bin too far writes into the tail and
	 * the byte compare says so.  A bank sized to fit would hide it.
	 */
	diff_begin("v34 handshake: the three DFT bank initialisers");
	{
		enum { BANK = 32 };
		static struct v34_dftbin bank_a[BANK], bank_b[BANK];
		static const struct {
			const char *what;
			void (*ours)(struct v34_dftbin *);
			void (*ref)(struct v34_dftbin *);
			int bins;
			short freq[4];
		} banks[] = {
			{ "dftfreqinit", dftfreqinit, ref_dftfreqinit, 25,
			  { 0, 0, 0, 0 } },
			{ "dftnlinitSignalBins", dftnlinitSignalBins,
			  ref_dftnlinitSignalBins, 4, { 7, 9, 13, 17 } },
			{ "dftnlinitNoiseBins", dftnlinitNoiseBins,
			  ref_dftnlinitNoiseBins, 4, { 6, 8, 12, 16 } }
		};
		unsigned bi, fill;

		/*
		 * TWO FILLS, and not because one might be missed.  Only one
		 * of the three clears the double accumulators, so with a
		 * single poison "cleared" and "left alone" are told apart by
		 * whether the poison survives -- but a poison of 0x00 makes
		 * those two indistinguishable.  0xa5 and 0x00 together
		 * distinguish them in one direction each.
		 */
		for (fill = 0; fill < 2; fill++)
		for (bi = 0; bi < sizeof(banks) / sizeof(banks[0]); bi++) {
			long tag = (long)fill * 100 + bi;
			unsigned char pat = fill ? 0 : 0xa5;
			unsigned b2;
			int k;

			memset(bank_a, pat, sizeof(bank_a));
			memset(bank_b, pat, sizeof(bank_b));
			banks[bi].ours(bank_a);
			banks[bi].ref(bank_b);

			for (b2 = 0; b2 < sizeof(bank_a); b2++)
				diff_eq_int(banks[bi].what,
					    ((unsigned char *)bank_a)[b2],
					    ((unsigned char *)bank_b)[b2],
					    tag * 10000 + b2);

			/*
			 * And the bank really is that long: the entry one
			 * past the end still holds the poison.  The compare
			 * above would catch a reconstruction that overran,
			 * because the reference does not -- this catches the
			 * two of them agreeing on a length the header's
			 * comment disagrees with.
			 */
			for (k = 0; k < (int)sizeof(struct v34_dftbin); k++)
				diff_eq_int("one past the end is untouched",
					    ((unsigned char *)
					     &bank_a[banks[bi].bins])[k],
					    pat, tag * 10000 + 9000 + k);

			/*
			 * The frequencies the header names, in the units the
			 * object writes.  Nothing else in the tree states
			 * them, and a comment that drifts from the code is
			 * worse than no comment.
			 */
			for (k = 0; k < 4 && banks[bi].freq[0]; k++)
				diff_eq_int("the bin number the header names",
					    bank_a[k].inc,
					    (short)(banks[bi].freq[k] << 8),
					    tag * 10000 + 8000 + k);
		}

		/* dftfreqinit's own: bin 1 first, bin 25 last, one-based. */
		memset(bank_a, 0xa5, sizeof(bank_a));
		dftfreqinit(bank_a);
		diff_eq_int("dftfreqinit starts at bin 1", bank_a[0].inc,
			    1 << 8, 0);
		diff_eq_int("dftfreqinit ends at bin 25", bank_a[24].inc,
			    25 << 8, 0);
	}
	rc |= diff_end();

	diff_begin("v34 handshake: dftRetrainDetInit");
	{
		/*
		 * It writes into the object rather than taking a bank, so
		 * the whole-object compare is the test -- three bins and
		 * five scalars scattered 22 KB apart, and a store that
		 * landed in the wrong one of them would still leave a
		 * plausible detector.
		 */
		setup();
		dftRetrainDetInit(&oa);
		ref_dftRetrainDetInit(ob);
		compare("dftRetrainDetInit", 30000);

		diff_eq_int("armed in state 1", oa.retrain_state, 1, 0);
		diff_eq_int("bin 0 is 900 Hz", oa.retrain_bins[0].inc,
			    6 << 8, 0);
		diff_eq_int("bin 1 is 1200 Hz", oa.retrain_bins[1].inc,
			    8 << 8, 0);
		diff_eq_int("bin 2 is 1500 Hz", oa.retrain_bins[2].inc,
			    10 << 8, 0);

		/* Twice, because it is not idempotent by construction. */
		dftRetrainDetInit(&oa);
		ref_dftRetrainDetInit(ob);
		compare("dftRetrainDetInit twice", 30100);
	}
	rc |= diff_end();

	/*
	 * detectRetrainReq, driven through its whole cycle.
	 *
	 * The detector answers once every 128 samples and needs three quiet
	 * measurements followed by nine loud ones, so nothing short of ~1700
	 * samples reaches the answer at all.  Feeding it random noise would
	 * never get there; the drive below is silence and then a full-scale
	 * 1200 Hz tone, which is the middle bin's own frequency.
	 */
	diff_begin("v34 handshake: detectRetrainReq, the whole cycle");
	{
		/* 1200 Hz at 9600 Hz is eight samples per period. */
		static const short tone[8] = {
			0, 22627, 32000, 22627, 0, -22627, -32000, -22627
		};
		static const short quiet[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		/*
		 * Silence long enough to arm it, one loud window to end the
		 * quiet run, nine to answer, then silence again to see the
		 * reset.  The short run at the front is the case where the
		 * run ends BEFORE the limit and the machine must stay put.
		 */
		static const struct { int loud; int windows; } drive[] = {
			{ 0, 2 }, { 1, 1 },	/* run of 2 -- too short   */
			{ 0, 4 }, { 1, 1 },	/* run of 4 -- state 2     */
			{ 1, 9 },		/* nine loud -- the answer */
			{ 0, 1 },		/* and back to state 1     */
			{ 1, 3 }
		};
		int saw_state2 = 0, saw_reset = 0, saw_answer = 0;
		int saw_short_run = 0, saw_between = 0;
		short was;
		unsigned d;
		int w, c, ph = 0;

		setup();
		dftRetrainDetInit(&oa);
		ref_dftRetrainDetInit(ob);
		was = oa.retrain_state;

		for (d = 0; d < sizeof(drive) / sizeof(drive[0]); d++)
		for (w = 0; w < drive[d].windows; w++) {
			long tag = (long)d * 1000 + w * 10;

			/* 128 samples, four at a time, as the object is fed. */
			for (c = 0; c < 32; c++) {
				const short *src = drive[d].loud ? tone : quiet;
				short in[4];
				int k, ra, rb;

				for (k = 0; k < 4; k++, ph++)
					in[k] = src[ph & 7];

				ra = detectRetrainReq(&oa, V34_RETRAIN_BINS,
						      in, 4);
				rb = ref_detectRetrainReq(ob, V34_RETRAIN_BINS,
							  in, 4);
				diff_eq_int("detectRetrainReq answered the "
					    "same", ra, rb, tag + c);

				if (ra)
					saw_answer = 1;
				if (was == 1 && oa.retrain_state == 2)
					saw_state2 = 1;
				if (was == 2 && oa.retrain_state == 1)
					saw_reset = 1;
				if (was == 1 && oa.retrain_state == 1
				    && oa.retrain_runs == 0)
					saw_short_run = 1;
				if (oa.retrain_state == 2 && !ra
				    && oa.retrain_runs > 0)
					saw_between = 1;
				was = oa.retrain_state;
			}

			/* The whole object, once per measurement. */
			compare("detectRetrainReq", 31000 + tag);
		}

		/*
		 * Five arms, and a sweep that reaches four of them proves
		 * nothing about the fifth.  Every one of these was reached
		 * by the drive above the first time it ran; if a change to
		 * the thresholds or the tone makes one unreachable, this
		 * fails rather than quietly testing less.
		 */
		diff_eq_int("the quiet run reached the limit", saw_state2, 1,
			    0);
		diff_eq_int("a short quiet run did NOT", saw_short_run, 1, 0);
		diff_eq_int("state 2 counted without answering", saw_between,
			    1, 0);
		diff_eq_int("the detector answered", saw_answer, 1, 0);
		diff_eq_int("and silence reset it", saw_reset, 1, 0);
	}
	rc |= diff_end();

	/*
	 * The thresholds are read SIGNED and the energy UNSIGNED.
	 *
	 * The energy side is not reachable: `dftenergy` writes `(short)((int)e
	 * >> 16)` and `e` is a sum of two squares of 16-bit values, so it
	 * exceeds 2^31 -- the only way the short goes negative -- when both
	 * halves are simultaneously at full scale, which no input found does.
	 * A sweep of all 32767 amplitudes of a 1200 Hz sine produced none.
	 *
	 * The threshold side is reachable, and this is what reaches it: a
	 * negative threshold read signed rejects every energy, read unsigned
	 * accepts every energy, and the two answers are opposite on the very
	 * first measurement.  `dftRetrainDetInit` never writes one -- 80 and
	 * 3000 -- so nothing but a seeded field gets here.
	 */
	diff_begin("v34 handshake: detectRetrainReq's thresholds, one bin at a "
		   "time");
	{
		/*
		 * ONE BIN AT A TIME, and the other two set to a value that
		 * always passes.  Driving all three together looks more
		 * thorough and tests less: the quiet arm is an AND over the
		 * three bins, so with equal thresholds a change to bin 0's
		 * comparison is masked by bin 1's still failing, and every
		 * per-bin claim -- the `<`, the sign of the widening, which
		 * bin's threshold each energy is read against -- passes
		 * whatever the code does.  Seven mutants survived exactly
		 * that way before this loop was written per-bin.
		 *
		 * `-32768` for the other two thresh_hi and `32767` for the
		 * other two thresh_lo is what "always passes" means on each
		 * arm: silence gives an energy of 0, and 0 is below one and
		 * above the other.
		 */
		static const short thr[] = { -1, -32768, 0, 1, 2, 3000,
					     32767 };
		unsigned ti, bi;
		int target, st;
		int saw_state1_yes = 0, saw_state1_no = 0;
		int saw_state2_yes = 0, saw_state2_no = 0;

		for (target = 0; target < V34_RETRAIN_BINS; target++)
		for (st = 1; st <= 2; st++)
		for (ti = 0; ti < sizeof(thr) / sizeof(thr[0]); ti++) {
			long tag = (long)target * 10000 + st * 1000
				 + (long)ti * 100;
			short in[4];
			int c, was;

			setup();
			dftRetrainDetInit(&oa);
			ref_dftRetrainDetInit(ob);
			poke_short(__builtin_offsetof(struct v34_object,
						      retrain_state),
				   (short)st);

			for (bi = 0; bi < V34_RETRAIN_BINS; bi++) {
				unsigned off = (unsigned)
					__builtin_offsetof(struct v34_object,
							   retrain_bins)
					+ bi * sizeof(struct v34_dftbin);
				short lo = (bi == (unsigned)target)
					 ? thr[ti] : 32767;
				short hi = (bi == (unsigned)target)
					 ? thr[ti] : -32768;

				poke_short(off + __builtin_offsetof(
						struct v34_dftbin, thresh_lo),
					   lo);
				poke_short(off + __builtin_offsetof(
						struct v34_dftbin, thresh_hi),
					   hi);
			}

			in[0] = in[1] = in[2] = in[3] = 0;
			was = oa.retrain_runs;
			for (c = 0; c < 32; c++) {
				int ra = detectRetrainReq(
					&oa, V34_RETRAIN_BINS, in, 4);
				int rb = ref_detectRetrainReq(
					ob, V34_RETRAIN_BINS, in, 4);

				diff_eq_int("seeded threshold answer", ra, rb,
					    tag + c);
			}
			compare("seeded threshold", 32000 + tag);

			/*
			 * One measurement happened, and the run either
			 * advanced or did not.  Which one is the whole
			 * discrimination, so both have to be seen in both
			 * states or the sweep is testing one arm.
			 */
			if (st == 1 && oa.retrain_runs > was)
				saw_state1_yes = 1;
			if (st == 1 && oa.retrain_runs == 0)
				saw_state1_no = 1;
			if (st == 2 && oa.retrain_runs > was)
				saw_state2_yes = 1;
			if (st == 2 && oa.retrain_runs == 0)
				saw_state2_no = 1;
		}

		diff_eq_int("state 1 counted silence as quiet", saw_state1_yes,
			    1, 0);
		diff_eq_int("and rejected it", saw_state1_no, 1, 0);
		diff_eq_int("state 2 counted silence as a tone",
			    saw_state2_yes, 1, 0);
		diff_eq_int("and rejected it", saw_state2_no, 1, 0);
	}
	rc |= diff_end();

	/*
	 * And the energy is widened UNSIGNED.
	 *
	 * `dftenergy` writes `(short)((int)e >> 16)` where `e` is a sum of two
	 * squares of 16-bit values, so the short goes negative only when `e`
	 * reaches 2^31 -- both halves at full scale at once.  No sample
	 * sequence tried reaches that: a sweep of all 32767 amplitudes of a
	 * 1200 Hz sine gave none, because a pure tone puts almost all of its
	 * correlation on one axis.
	 *
	 * The accumulators are object fields, though, and 0x04000000 in both
	 * of them is exactly the corner: `<< 5` wraps to -2^31, `>> 16` gives
	 * -32768, and the two squares sum to 2^31 on the nose.  Silence adds
	 * nothing to an accumulator, so a seed placed before the last update
	 * of the window survives into the measurement.
	 *
	 * With the threshold at 32767 the two widenings then disagree
	 * outright: -32768 is below it and 32768 is not.
	 */
	diff_begin("v34 handshake: detectRetrainReq's energy is widened "
		   "unsigned");
	{
		unsigned target, bi;
		int st;
		int saw_negative = 0, saw_quiet = 0, saw_loud = 0;

		for (st = 1; st <= 2; st++)
		for (target = 0; target < V34_RETRAIN_BINS; target++) {
			long tag = (long)st * 100 + target * 10;
			short in[4];
			int c;

			setup();
			dftRetrainDetInit(&oa);
			ref_dftRetrainDetInit(ob);
			poke_short(__builtin_offsetof(struct v34_object,
						      retrain_state),
				   (short)st);
			for (bi = 0; bi < V34_RETRAIN_BINS; bi++) {
				unsigned off = (unsigned)
					__builtin_offsetof(struct v34_object,
							   retrain_bins)
					+ bi * sizeof(struct v34_dftbin);

				poke_short(off + __builtin_offsetof(
						struct v34_dftbin, thresh_lo),
					   32767);
				poke_short(off + __builtin_offsetof(
						struct v34_dftbin, thresh_hi),
					   (bi == 1) ? 0 : -32768);
			}

			in[0] = in[1] = in[2] = in[3] = 0;
			for (c = 0; c < 31; c++) {
				int ra = detectRetrainReq(
					&oa, V34_RETRAIN_BINS, in, 4);
				int rb = ref_detectRetrainReq(
					ob, V34_RETRAIN_BINS, in, 4);

				diff_eq_int("warm-up answer", ra, rb, tag + c);
			}

			{
				unsigned off = (unsigned)
					__builtin_offsetof(struct v34_object,
							   retrain_bins)
					+ target * sizeof(struct v34_dftbin);

				poke_int(off + __builtin_offsetof(
						struct v34_dftbin, acc_re),
					 0x04000000);
				poke_int(off + __builtin_offsetof(
						struct v34_dftbin, acc_im),
					 0x04000000);
			}

			{
				int ra = detectRetrainReq(
					&oa, V34_RETRAIN_BINS, in, 4);
				int rb = ref_detectRetrainReq(
					ob, V34_RETRAIN_BINS, in, 4);

				diff_eq_int("the measurement answer", ra, rb,
					    tag + 90);
			}
			compare("seeded accumulator", 36000 + tag);

			/*
			 * The seed did what it was supposed to -- if it ever
			 * stops doing so this section is testing nothing and
			 * says so rather than passing.
			 */
			if (oa.retrain_bins[target].energy < 0)
				saw_negative = 1;
			if (st == 1 && oa.retrain_runs == 0)
				saw_quiet = 1;
			if (st == 2 && oa.retrain_runs > 0)
				saw_loud = 1;
		}

		diff_eq_int("the seeded accumulator gave a negative energy",
			    saw_negative, 1, 0);
		diff_eq_int("read unsigned, it is not below 32767", saw_quiet,
			    1, 0);
		diff_eq_int("read unsigned, it is above zero", saw_loud, 1, 0);
	}
	rc |= diff_end();

	/*
	 * The measurement window is an equality, and it is 128.
	 *
	 * With the counter starting at zero and stepping by four it reaches
	 * 128 exactly, so `!= 0x80` and `< 0x80` behave identically and the
	 * difference between them is invisible.  Seeding the counter off the
	 * grid is what separates them: at 0x7e the object steps to 0x82 and
	 * never measures again, which is the behaviour the header records,
	 * and a threshold test would measure on every call from then on.
	 */
	diff_begin("v34 handshake: detectRetrainReq's window is an equality");
	{
		static const int seeds[] = { 0, 2, 4, 0x7c, 0x7e, 0x80, 0x82,
					     0x84, -8, -4, 1 };
		unsigned si;
		int saw_measured = 0, saw_never = 0;

		for (si = 0; si < sizeof(seeds) / sizeof(seeds[0]); si++) {
			short in[4];
			int c;

			setup();
			dftRetrainDetInit(&oa);
			ref_dftRetrainDetInit(ob);
			poke_int(__builtin_offsetof(struct v34_object,
						    retrain_phase),
				 seeds[si]);

			in[0] = in[1] = in[2] = in[3] = 0;
			for (c = 0; c < 64; c++) {
				int ra = detectRetrainReq(
					&oa, V34_RETRAIN_BINS, in, 4);
				int rb = ref_detectRetrainReq(
					ob, V34_RETRAIN_BINS, in, 4);

				diff_eq_int("seeded counter answer", ra, rb,
					    (long)si * 100 + c);
			}
			compare("seeded counter", 35000 + (long)si);

			/*
			 * `retrain_runs` moves only when a measurement
			 * happened, so it is the witness for both cases.
			 */
			if (oa.retrain_runs > 0)
				saw_measured = 1;
			else
				saw_never = 1;
		}

		diff_eq_int("an on-grid counter measured", saw_measured, 1, 0);
		diff_eq_int("an off-grid one never did", saw_never, 1, 0);
	}
	rc |= diff_end();

	/*
	 * The two run limits, seeded.
	 *
	 * `dftRetrainDetInit` writes 3 and 9, and with those two numbers the
	 * shared tail at the bottom of the function is invisible: the arm
	 * that gives up sets the run to zero and then falls into the same
	 * `run == tone_limit` comparison as the arm that counted, and zero is
	 * not nine either way.  Set the limit to zero and the two spellings
	 * disagree on the very call that gives up -- which is the only thing
	 * that can tell "one tail" from "return 0 here".
	 *
	 * Same for the quiet limit: at zero, the first run to end at all
	 * advances the machine, including a run of length zero.
	 */
	diff_begin("v34 handshake: detectRetrainReq's two run limits");
	{
		static const short tone[8] = {
			0, 22627, 32000, 22627, 0, -22627, -32000, -22627
		};
		static const short limits[] = { 0, 1, 2, 3, 9, -1, 32767 };
		unsigned li;
		int loud;
		int saw_zero_tail = 0, saw_instant = 0;

		/*
		 * BOTH INPUTS, because the two arms are reached by opposite
		 * ones: state 1 advances when the quiet run ENDS, which
		 * silence never does, and state 2's give-up arm is reached by
		 * silence and not by a tone.  A single input tests one of
		 * them and reports a clean run for the other.
		 */
		for (loud = 0; loud <= 1; loud++)
		for (li = 0; li < sizeof(limits) / sizeof(limits[0]); li++) {
			short in[4];
			int c, st, ph = 0;

			/*
			 * 0 and 3 as well as the two real states: the
			 * machine has no default arm and must do nothing at
			 * all in a state it does not recognise, which is a
			 * claim about the `!= 2` guard and not about either
			 * arm.
			 */
			for (st = 0; st <= 3; st++) {
				setup();
				dftRetrainDetInit(&oa);
				ref_dftRetrainDetInit(ob);
				poke_short(__builtin_offsetof(
						   struct v34_object,
						   retrain_state),
					   (short)st);
				poke_short(__builtin_offsetof(
						   struct v34_object,
						   retrain_quiet_runs),
					   limits[li]);
				poke_short(__builtin_offsetof(
						   struct v34_object,
						   retrain_tone_runs),
					   limits[li]);

				for (c = 0; c < 64; c++) {
					int ra, rb, k;

					for (k = 0; k < 4; k++, ph++)
						in[k] = loud ? tone[ph & 7] : 0;

					ra = detectRetrainReq(
						&oa, V34_RETRAIN_BINS, in, 4);
					rb = ref_detectRetrainReq(
						ob, V34_RETRAIN_BINS, in, 4);

					diff_eq_int("seeded limit answer", ra,
						    rb,
						    (long)loud * 100000
						    + (long)li * 1000
						    + st * 100 + c);
					if (ra && st == 2 && !loud
					    && limits[li] == 0)
						saw_zero_tail = 1;
				}
				compare("seeded limit",
					33000 + (long)loud * 1000
					+ (long)li * 10 + st);
				if (st == 1 && loud && limits[li] == 0
				    && oa.retrain_state == 2)
					saw_instant = 1;
			}
		}

		/*
		 * Silence keeps state 2's run at zero, so a limit of zero is
		 * the case where the give-up arm answers "retrain" -- and it
		 * is the object's answer, not this fixture's opinion.
		 */
		diff_eq_int("a zero tone limit answers from the give-up arm",
			    saw_zero_tail, 1, 0);
		diff_eq_int("a zero quiet limit advances at once", saw_instant,
			    1, 0);
	}
	rc |= diff_end();

	/*
	 * `nbins` is the caller's.
	 *
	 * Everything above passes three, which is what `dftRetrainDetInit`
	 * arms, and with three the count is indistinguishable from a
	 * constant: `dftupdate`, `dftenergy` and the clear loop would all
	 * behave identically written as 3.  The decision arms still read all
	 * three bins whatever is passed, so a short count leaves the tail of
	 * the bank holding a stale energy and the machine acts on it -- which
	 * is the object's behaviour and worth pinning rather than tidying.
	 */
	diff_begin("v34 handshake: detectRetrainReq over a prefix of the bank");
	{
		static const short tone[8] = {
			0, 22627, 32000, 22627, 0, -22627, -32000, -22627
		};
		short nb;
		int loud;

		for (loud = 0; loud <= 1; loud++)
		for (nb = 0; nb <= V34_RETRAIN_BINS; nb++) {
			short in[4];
			int c, ph = 0;

			unsigned bi;

			setup();
			dftRetrainDetInit(&oa);
			ref_dftRetrainDetInit(ob);

			/*
			 * THE ACCUMULATORS ARE SEEDED, or the clear loop's
			 * extent is unobservable: `dftupdate` only touches
			 * the first `nb` bins, so on a freshly armed detector
			 * the rest are already zero and clearing them anyway
			 * changes nothing.  With a value in them, "cleared to
			 * `nb`" and "cleared to three" are different objects.
			 */
			for (bi = 0; bi < V34_RETRAIN_BINS; bi++) {
				unsigned off = (unsigned)
					__builtin_offsetof(struct v34_object,
							   retrain_bins)
					+ bi * sizeof(struct v34_dftbin);

				poke_short(off + __builtin_offsetof(
						struct v34_dftbin, phase),
					   (short)(0x1234 + bi));
				poke_int(off + __builtin_offsetof(
						struct v34_dftbin, acc_re),
					 0x5a5a00 + (int)bi);
				poke_int(off + __builtin_offsetof(
						struct v34_dftbin, acc_im),
					 -0x3c3c00 - (int)bi);
			}

			for (c = 0; c < 96; c++) {
				int ra, rb, k;

				for (k = 0; k < 4; k++, ph++)
					in[k] = loud ? tone[ph & 7] : 0;

				ra = detectRetrainReq(&oa, nb, in, 4);
				rb = ref_detectRetrainReq(ob, nb, in, 4);
				diff_eq_int("prefix answer", ra, rb,
					    (long)loud * 10000 + nb * 100 + c);
			}
			compare("prefix", 34000 + (long)loud * 10 + nb);
		}
	}
	rc |= diff_end();

	/* --- probeselect ------------------------------------------------ */

	/*
	 * FOUR THOUSAND PSEUDO-RANDOM CASES, AT FOUR SPREADS.  The ladder is
	 * fifteen comparisons over eleven bins crossed with a role flag and
	 * eight capability bytes, and its arms are not independent: each
	 * originating arm falls THROUGH into the next rate's test, so which
	 * message bits end up set depends on the whole path and not on one
	 * branch.  Enumerating that by hand would be a list of the cases I
	 * happened to think of; the counters below say what was reached.
	 *
	 * The spread is what makes random inputs useful here.  Every
	 * threshold in the ladder is 5, 6 or 10, so a uniform 16-bit shift
	 * takes the same arm every time; narrowing towards zero puts the
	 * inputs where the decisions are, and four spreads rather than one
	 * because the tests are on DIFFERENT bins and a spread that makes one
	 * comparison interesting saturates another.
	 */
	diff_begin("v34 handshake: probeselect, four thousand probes");
	{
		static const int spread[] = { 3, 8, 14, 40 };
		unsigned s, n;
		long tag = 70000;

		for (s = 0; s < sizeof(spread) / sizeof(spread[0]); s++)
		for (n = 0; n < 1000; n++)
			run_probeselect(0x1234567u + n * 2654435761u + s,
					spread[s], tag++);

		/*
		 * Every rate, and "no rate at all" -- which is a real outcome
		 * on both sides: the originating arms never write the baud
		 * rate, and the answering ladder runs off the bottom when the
		 * far end offers nothing.
		 */
		for (n = 0; n < 6; n++)
			diff_eq_int("every rate arm was reached", saw_rate[n],
				    1, (long)n);

		/*
		 * And the pre-emphasis search's whole range.  6..10 are the
		 * reachable ones; 0 is the arm D53 says cannot be taken, and
		 * asserting it was NOT seen is what would catch that reading
		 * being wrong.
		 */
		for (n = 6; n <= 10; n++)
			diff_eq_int("every pre-emphasis index was returned",
				    saw_preemph[n], 1, 100 + (long)n);
		diff_eq_int("and index 0 was not", saw_preemph[0], 0, 200);
		for (n = 1; n <= 5; n++)
			diff_eq_int("nor anything below six", saw_preemph[n],
				    0, 200 + (long)n);
	}
	rc |= diff_end();

	/*
	 * THE dB LADDER, ON ITS OWN TERMS.  The loop steps `t` by 1.2588 from
	 * 0x47d and stops when it passes the ratio, so `<` against `<=`, and
	 * the rounding term inside the step, are visible only when the ratio
	 * IS one of the terms.  Walking the sequence and driving the ratio to
	 * each term and its neighbours is the whole of that boundary, and
	 * 0x18fff is included because the short-circuit above the loop has an
	 * edge of its own.
	 *
	 * The gain is swept under it, because the reduction the ladder feeds
	 * is `(0x640000 / gain) * dbcnt >> 14` and the rounding in that divide
	 * only shows when the product crosses a multiple of 16384.
	 */
	diff_begin("v34 handshake: probeselect on the dB ladder's own terms");
	{
		/*
		 * THE MIDDLE SEVEN ARE SOLVED FOR, AND THE CONSTRAINT IS THE
		 * CLAMP.  The reciprocal `(gain / 2 + 0x640000) / gain`
		 * differs from the unrounded divide for about half of all
		 * gains, but the difference is ONE, and to be observable it
		 * has to survive `* dbcnt >> 14` AND the `req > 7` clamp
		 * immediately after.
		 *
		 * The first of those wants a large `dbcnt` and the second
		 * wants a small `req`, which is why the obvious choice --
		 * the 1000 the short-circuit produces -- shows nothing at
		 * all: every gain below 0x1000 then gives a `req` in the
		 * hundreds and both sides clamp to 7.  What is needed is a
		 * gain and a ladder step where the product crosses a
		 * multiple of 16384 while `req` is still under seven, and
		 * there are nineteen such gains below 0x1000.  These are
		 * seven of them, with the step each one needs:
		 *
		 *    560 at 7    600 at 3    900 at 9   960 at 12
		 *   1000 at 5   1040 at 13  1360 at 17
		 *
		 * The ladder sweep below reaches every step from 0 to 19, so
		 * naming the gains is enough.
		 */
		static const short gains[] = { 1, 2, 3, 7, 11, 13, 100,
					       560, 600, 900, 960, 1000,
					       1040, 1360,
					       0x7ff, 0x800, 0xfff, 0x1000 };
		unsigned t = 0x47d;
		unsigned g;
		long tag = 90000;

		while (t < 0x19100) {
			for (g = 0; g < sizeof(gains) / sizeof(gains[0]); g++) {
				run_probe_ratio(t - 1, gains[g], tag++);
				run_probe_ratio(t, gains[g], tag++);
				run_probe_ratio(t + 1, gains[g], tag++);
			}
			t = (t * 0x509 + 0x200) >> 10;
		}

		for (g = 0; g < sizeof(gains) / sizeof(gains[0]); g++) {
			run_probe_ratio(0x18ffe, gains[g], tag++);
			run_probe_ratio(0x18fff, gains[g], tag++);
			run_probe_ratio(0x19000, gains[g], tag++);
		}
	}
	rc |= diff_end();

	/*
	 * AND THE SEARCH'S OWN EQUALITY.  Each rate's constant gets its
	 * candidate bin driven to the exact pre-image of the reference, and to
	 * the two values either side, so `x > ref` and `x >= ref` disagree on
	 * one of the four.  Several references, because the pre-image is a
	 * division and lands differently for each.
	 */
	diff_begin("v34 handshake: probeselect's pre-emphasis equality");
	{
		static const struct { unsigned bin; int k; } rate[] = {
			{ 18, 0x7da7 }, { 18, 0x6789 }, { 19, 0x656f },
			{ 20, 0x639f }, { 22, 0x6626 }
		};
		static const short refs[] = { 1, 2, 17, 100, 1000, 4096,
					      0x4000, -1, -100 };
		unsigned r, v;
		int dd;
		long tag = 95000;

		for (r = 0; r < sizeof(rate) / sizeof(rate[0]); r++)
		for (v = 0; v < sizeof(refs) / sizeof(refs[0]); v++)
		for (dd = -2; dd <= 2; dd++)
			run_probe_preemph_edge(rate[r].bin, rate[r].k,
					       refs[v], dd, tag++);
	}
	rc |= diff_end();

	/*
	 * THE TILT SWEEP.  One step of the meter per row, over the range a real
	 * channel can present, at the baud rate this bench actually uses.  It
	 * pins the mapping finding 1475 derived, so a change to `probe_preemph`
	 * that alters which channel gets which filter cannot land quietly.
	 */
	diff_begin("v34 handshake: the pre-emphasis search is a tilt meter");
	{
		static const short refs[] = { 4096, 8000, 0x4000 };
		unsigned v;
		int steps;
		long tag = 96000;

		dsplib_debug_capture_on = 1;
		dsplibs_debug_level = 3;
		ref_dsplibs_debug_level = 3;

		for (v = 0; v < sizeof(refs) / sizeof(refs[0]); v++)
		for (steps = 1; steps <= 6; steps++)
			run_preemph_tilt(22, 0x6626, refs[v], steps, tag++);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	diff_begin("v34 handshake: probeselect narrates every decision");
	{
		unsigned lvl, n;
		long tag = 80000;

		dsplib_debug_capture_on = 1;

		for (lvl = 2; lvl <= 3; lvl++) {
			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;

			for (n = 0; n < 150; n++) {
				dsplib_debug_capture_reset();
				run_probeselect(0x9e3779b9u + n * 40503u,
						(int)(3 + n % 12), tag);
				diff_eq_int("probeselect transcript",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);
				diff_eq_int("and it said something",
					    dsplib_debug_capture_text(1)[0]
					    != 0, 1, tag);
				tag++;
			}
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	/* --- settxlevel ------------------------------------------------- */

	/*
	 * EXHAUSTIVE OVER THE MP SHORT'S TWO FIELDS.  Six bits decide the
	 * request -- three for the reduction and three for the addition --
	 * so all sixty-four combinations are driven rather than sampled, and
	 * the two other bit positions of the low byte are swept with them to
	 * show they are discarded.  A reconstruction that reversed the two
	 * three-bit fields, or clamped before reversing instead of after,
	 * disagrees somewhere in that grid and nowhere outside it.
	 *
	 * THE STARTING SCALE IS SWEPT PAST WHERE A SHORT STOPS.  Both loops
	 * multiply and shift, and the one that RAISES the scale truncates its
	 * accumulator to a short every iteration while the one that lowers it
	 * does not (D51).  A scale near 32767 with a negative reduction is
	 * the only input that shows the difference, and it is here.
	 */
	diff_begin("v34 handshake: settxlevel, every power field the MP "
		   "message can carry");
	{
		static const short scale_in[] = {
			0, 1, 0x16a1, 100, 1000, 0x4000, 0x7fff, 0x7ffe,
			-1, -1000, (short)0x8000
		};
		unsigned f, s;
		long tag = 20000;

		for (f = 0; f < 0x100; f++)
		for (s = 0; s < sizeof(scale_in) / sizeof(scale_in[0]); s++)
			run_settxlevel((unsigned short)f, scale_in[s],
				       0, 0, 0, 4, 0, 0, tag++);

		/* And the high byte, which is shifted out of both fields. */
		for (f = 0; f < 8; f++)
			run_settxlevel((unsigned short)(0xab00 | (f << 5)),
				       0x16a1, 0, 0, 0, 4, 0, 0, tag++);
	}
	rc |= diff_end();

	/*
	 * AND THE PCM SIDE, WHICH COMBINES TWO DIFFERENT WAYS.  A negative
	 * minimum is ADDED to the far end's request; a non-negative one is a
	 * FLOOR.  Both are reached only with a V.90 receiver running, so the
	 * two receiver words are swept under every reduction -- and the
	 * configured reduction is swept across its own clamp, since
	 * `GetVPcmMinimalTxPowerReduction` is what produces the value being
	 * combined here.
	 */
	diff_begin("v34 handshake: settxlevel against the local PCM minimum");
	{
		static const short want_in[] = {
			(short)0x8000, -20, -11, -10, -9, -1, 0, 1, 3, 6, 7,
			8, 100
		};
		static const unsigned char mp_in[] = {
			0x00, 0x20, 0x40, 0x60, 0x80, 0xa0, 0xc0, 0xe0,
			0x04, 0x1c, 0xfc
		};
		static const int recv_in[][2] = {
			{ 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 },
			{ -1, 0 }, { 0, -1 }
		};
		static const int sens_in[] = { 0, 1 };
		static const int gate_in[] = { 0, 1 };
		unsigned w, mi, r, s, g;
		long tag = 30000;

		for (w = 0; w < sizeof(want_in) / sizeof(want_in[0]); w++)
		for (mi = 0; mi < sizeof(mp_in) / sizeof(mp_in[0]); mi++)
		for (r = 0; r < sizeof(recv_in) / sizeof(recv_in[0]); r++)
		for (s = 0; s < sizeof(sens_in) / sizeof(sens_in[0]); s++)
		for (g = 0; g < sizeof(gate_in) / sizeof(gate_in[0]); g++)
			run_settxlevel(mp_in[mi], 0x16a1, want_in[w],
				       sens_in[s], gate_in[g], 4,
				       recv_in[r][0], recv_in[r][1], tag++);

		diff_eq_int("some case took the positive power arm",
			    saw_pwr_high, 1, 0);
		diff_eq_int("and some case took the other", saw_pwr_low, 1, 0);

		/*
		 * THE UP LOOP'S TRUNCATION, WHICH NEEDS BOTH HALVES AT ONCE.
		 * It is reachable only with a V.90 receiver and a NEGATIVE
		 * minimum -- that is the one path that makes the reduction
		 * negative -- and observable only from a scale large enough
		 * that one 1.122 step leaves a short.  The block above has
		 * the first and the block before it has the second, and
		 * neither has both; 0x16a1 survives four steps of gain, so
		 * every case up to here agrees whatever width the
		 * accumulator has.  D51.
		 */
		for (w = 0; w < sizeof(want_in) / sizeof(want_in[0]); w++) {
			static const short big[] = {
				20000, 29000, 29200, 30000, 0x7fff, 0x7ffe,
				-20000, -30000, (short)0x8000
			};
			unsigned b;

			for (b = 0; b < sizeof(big) / sizeof(big[0]); b++)
				run_settxlevel(0x00, big[b], want_in[w],
					       0, 1, 4, 1, 0, tag++);
		}
	}
	rc |= diff_end();

	diff_begin("v34 handshake: settxlevel says what it did");
	{
		static const unsigned char mp_in[] = { 0x00, 0x20, 0xe0,
						       0x1c, 0xfc };
		static const short want_in[] = { -10, -1, 0, 3, 7 };
		unsigned lvl, mi, w;
		long tag = 40000;

		dsplib_debug_capture_on = 1;

		for (lvl = 2; lvl <= 3; lvl++) {
			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;

			for (mi = 0; mi < sizeof(mp_in) / sizeof(mp_in[0]);
			     mi++)
			for (w = 0; w < sizeof(want_in) / sizeof(want_in[0]);
			     w++) {
				dsplib_debug_capture_reset();
				run_settxlevel(mp_in[mi], 0x16a1, want_in[w],
					       0, 1, 4, 1, 0, tag);
				diff_eq_int("settxlevel transcript",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);
				diff_eq_int("and it said something",
					    dsplib_debug_capture_text(1)[0]
					    != 0, 1, tag);
				tag++;
			}
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	/* --- v34setuptxmit ---------------------------------------------- */

	/*
	 * EVERY SYMBOL RATE WITH BOTH OF ITS CARRIERS, because the carrier is
	 * what picks the echo pre-emphasis table inside `V34SetupModulator`
	 * and the two tables of a pair are the same length -- so a swapped
	 * pair leaves every scalar in the object identical and shows up only
	 * in the CONTENT check on +0x20cc.  3429 has one carrier and 600 is
	 * the signalling rate, and both are here for the same reason.
	 *
	 * The two state words are swept across the value each transition
	 * moves them to, so the "already there" arm -- which prints nothing
	 * and assigns nothing -- is driven as well as the moving one.
	 */
	diff_begin("v34 handshake: v34setuptxmit, every rate and carrier");
	{
		static const struct { short baud, carrier; } rate_in[] = {
			{ 2400, 1600 }, { 2400, 1800 },
			{ 2800, 1680 }, { 2800, 1867 },
			{ 3000, 1800 }, { 3000, 2000 },
			{ 3200, 1829 }, { 3200, 1920 },
			{ 3429, 1959 }, { 600, 1200 }
		};
		static const short state_in[][3] = {
			/* rxstate, txstate, microstate */
			{ V34HS_PHASE1, V34HS_PHASE2, V34HS_DET_SYNC },
			{ V34HS_WAIT,   V34HS_PHASE2, V34HS_DET_SYNC },
			{ V34HS_PHASE1, V34HS_SSEG,   V34HS_DET_SYNC },
			{ V34HS_WAIT,   V34HS_SSEG,   V34HS_DET_SYNC }
		};
		unsigned r, p, st, v;
		long tag = 50000;

		for (r = 0; r < sizeof(rate_in) / sizeof(rate_in[0]); r++)
		for (p = 0; p < 3; p++)
		for (st = 0; st < sizeof(state_in) / sizeof(state_in[0]); st++)
		for (v = 0; v < 4; v++)
			run_setuptxmit(rate_in[r].baud, rate_in[r].carrier,
				       (short)(p * 4), (v & 1) ? 3 : 0,
				       (v & 2) ? 5 : 0,
				       state_in[st][0], state_in[st][1],
				       state_in[st][2], tag++);
	}
	rc |= diff_end();

	diff_begin("v34 handshake: v34setuptxmit's two transitions, logged");
	{
		unsigned lvl, k;
		long tag = 60000;

		dsplib_debug_capture_on = 1;

		for (lvl = 2; lvl <= 3; lvl++) {
			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;

			/*
			 * Every one of the 87 names, in all three slots: the
			 * two transitions print the other machines' names as
			 * context, and driving the words a third of the table
			 * apart is what tells the two context slots apart.
			 */
			for (k = 0; k < V34HS_STATE_COUNT; k++) {
				dsplib_debug_capture_reset();
				run_setuptxmit(3000, 1800, 0, 3, 0,
					       (short)k,
					       (short)((k + 29)
						       % V34HS_STATE_COUNT),
					       (short)((k + 58)
						       % V34HS_STATE_COUNT),
					       tag);
				diff_eq_int("v34setuptxmit transcript",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);
				diff_eq_int("and it said something",
					    dsplib_debug_capture_text(1)[0]
					    != 0, 1, tag);
				tag++;
			}
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
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

	/*
	 * The half every transcript block above leaves open.  All of them
	 * raise the level to 2 before capturing, so a site that lost its
	 * `if (DSPLIB_DEBUG_ON())` prints identically on both sides and
	 * passes: all twenty mutants -- ten gates, `if (1)` and `>= 1` each
	 * -- survived this file before this section existed.
	 *
	 * Level 1 is the one that matters.  Every gate in the object is
	 * `> 1`, so 1 is the single value at which it disagrees with the
	 * `>= 1` a reader would write; at 0 both spellings are silent and
	 * both mutants live.  0 is swept as well because it is free.
	 *
	 * ALL FIVE FUNCTIONS THAT CARRY A GATE, which is why this drives so
	 * much: `dpskinit` has one, `setfinalrate` one, `setupreceiver` two,
	 * `preempindex` three and `hs_setstate` three.  Driving four of the
	 * five would leave the fifth reading as covered.
	 *
	 * `run_handshakinit`, not `run_handshakinit_traced` -- the traced one
	 * asserts the transcript is NON-empty, which is the opposite claim.
	 */
	diff_begin("v34 handshake: below the threshold, nothing is said");
	{
		unsigned lvl;

		dsplib_debug_capture_on = 1;

		for (lvl = 0; lvl <= 1; lvl++) {
			struct hsi_case c = hsi_base;

			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;
			dsplib_debug_capture_reset();

			setup();
			poke_ptr(0x2074, shaped_a, shaped_b);
			poke_short(0x264 + 0x262, 0x1234);
			dpskinit(&oa, 1, 1);
			ref_dpskinit(ob, 1, 1);

			run_setupreceiver(2743, 1829, 0x200, 9500 + (long)lvl);
			run_setfinalrate(0x0004, 0x0000, 0x0080, 0x00,
					 9520 + (long)lvl);

			/*
			 * BOTH LIVE GATES, from the first pair onwards: the
			 * loop-exhausted exit and then a middle index.  The
			 * index-0 arm is unreachable -- see D36 -- so its
			 * gate cannot be driven from here or anywhere.
			 */
			run_preempindex(4000, 100, 3200, 9541 + (long)lvl);
			run_preempindex(0, 0x7fff, 3429, 9542 + (long)lvl);
			/*
			 * Four more combinations, compared at every level.
			 *
			 * THIS COMMENT USED TO SAY THEY REACH NEITHER OF THE
			 * OTHER TWO ANNOUNCEMENTS, "because every set tried
			 * here comes back 0 or 5" -- which the function
			 * cannot do at all, its range being 6..10 (D36).
			 * Measured instead of reasoned about, with the level
			 * raised and the reference's transcript printed:
			 * these return 9, 10, 10 and 10, so two of the six
			 * calls in this section take the compare exit and
			 * four the loop-exhausted one, and every gate this
			 * function has that CAN be driven is driven here at
			 * levels 0 and 1.  A note saying a site cannot be
			 * reached is the thing that stops anyone looking, so
			 * it is worth more than the four cases it describes.
			 */
			run_preempindex(100, 10, 2400, 9543 + (long)lvl);
			run_preempindex(1000, 10, 2800, 9544 + (long)lvl);
			run_preempindex(8000, 10, 3000, 9545 + (long)lvl);
			run_preempindex(0x7fff, 1, 3429, 9546 + (long)lvl);

			/*
			 * EVERY MODE, because the gates are spread across
			 * them: `v34modeminit`'s is reached from 0, 1 and 4,
			 * the MOH announcement only from 4, and `hs_setstate`
			 * from all of them.  Running mode 2 alone -- which is
			 * what this did first -- left three of the ten gates
			 * undriven and reading as covered.
			 */
			for (c.mode = 0; c.mode <= 4; c.mode++)
				run_handshakinit(&c, 9560 + (long)lvl * 10
						 + c.mode);

			diff_eq_int("ours printed nothing",
				    dsplib_debug_capture_text(0)[0], 0,
				    (long)lvl);
			diff_eq_int("and neither did the reference",
				    dsplib_debug_capture_text(1)[0], 0,
				    (long)lvl);
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
		/*
		 * And the two the fixture owns are only holes after
		 * `seed_pwr`; nothing must leave them open by accident.
		 */
		diff_eq_int("the fixture's two holes are closed by default",
			    pwr_seeded, 0, 0);
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

	/*
	 * -------------------------------------------------------------------
	 * `getbit`, the object's own bit reader, and the first RECURSIVE
	 * function this tree has driven against the blob.
	 *
	 * The cases are chosen so that every refill arm runs at least once and
	 * the four flags at the bottom prove it, rather than the run merely
	 * being long enough that one probably did.  `nbits` and `wordbits` are
	 * always chosen so `idx` stays inside `word[]`: nothing in `getbit`
	 * bounds it, and a case that ran off the end would be reading its own
	 * struct's tail on both sides and comparing equal for the wrong
	 * reason.
	 */
	diff_begin("v34 handshake: getbit");
	{
		struct v34_bitsource s;
		long tag = 0;

		/* Ten sixteen-bit words, nothing else.  Ends at -1. */
		seed_bits(&s, 160, 16, 0, 0);
		run_getbit("getbit plain", tag++, &s, 170);

		/*
		 * The same with the CRC on, which is how the -16 remainder
		 * the function tests for is actually produced: the CRC costs
		 * sixteen bits of `pos` that `nbits` does not cover, so the
		 * refill after it sees exactly -16 and loads the four-bit
		 * filler.  Nothing had to be contrived for that arm.
		 */
		seed_bits(&s, 160, 16, 1, 0);
		run_getbit("getbit crc", tag++, &s, 200);

		/* A message whose length is not a multiple of the word. */
		seed_bits(&s, 150, 16, 1, 0);
		run_getbit("getbit part word", tag++, &s, 200);

		seed_bits(&s, 80, 8, 1, 0);
		run_getbit("getbit 8-bit words", tag++, &s, 120);

		seed_bits(&s, 40, 4, 1, 0);
		run_getbit("getbit 4-bit words", tag++, &s, 80);

		seed_bits(&s, 20, 1, 1, 0);
		run_getbit("getbit 1-bit words", tag++, &s, 60);

		/* THE RESTART, which is the recursion.  Four hundred calls
		 * over a 160-bit message is at least two restarts. */
		seed_bits(&s, 160, 16, 0, 1);
		run_getbit("getbit repeat", tag++, &s, 400);

		seed_bits(&s, 160, 16, 1, 1);
		run_getbit("getbit repeat with crc", tag++, &s, 400);

		/* A restart that reloads a non-empty accumulator, so the
		 * recursive call returns without refilling at all. */
		seed_bits(&s, 64, 16, 0, 1);
		s.avail0 = 4;
		s.acc0 = 0x5a;
		s.repeats = 30000;		/* and the counter wraps */
		run_getbit("getbit repeat preloaded", tag++, &s, 300);

		/* Entered with bits already available. */
		seed_bits(&s, 160, 16, 1, 0);
		s.avail = 5;
		s.acc = 0x1234;
		s.pos = 32;
		s.idx = 2;
		run_getbit("getbit primed", tag++, &s, 200);

		/* An empty message: exhausted on the first call. */
		seed_bits(&s, 0, 16, 1, 0);
		run_getbit("getbit empty", tag++, &s, 40);

		seed_bits(&s, 0, 16, 0, 0);
		run_getbit("getbit empty no crc", tag++, &s, 4);

		/* `pos` already past `nbits` by exactly one word, which is
		 * the filler arm reached without going through the CRC. */
		seed_bits(&s, 0, 16, 0, 0);
		s.pos = 16;
		run_getbit("getbit overrun", tag++, &s, 40);

		/* And past it by something else, which is not. */
		seed_bits(&s, 0, 16, 0, 0);
		s.pos = 17;
		run_getbit("getbit overrun 17", tag++, &s, 4);

		/* A CRC seeded to something other than 0xffff, so the fold
		 * is being asked to carry state in rather than to build it. */
		seed_bits(&s, 96, 16, 1, 0);
		s.crc = 0x1234;
		run_getbit("getbit crc seeded", tag++, &s, 140);

		diff_eq_int("getbit: the recursive arm ran", saw_recursion,
			    1, 0);
		diff_eq_int("getbit: the exhausted arm ran", saw_minus_one,
			    1, 0);
		diff_eq_int("getbit: the CRC tail ran", saw_crc_tail, 1, 0);
		diff_eq_int("getbit: the four-bit filler ran", saw_filler,
			    1, 0);
	}
	rc |= diff_end();

	/*
	 * -------------------------------------------------------------------
	 * `ApplyBulkDelay`.  Swept rather than sampled across every boundary
	 * the function has: zero, `bulk_len`, the 144 default and its own
	 * bounds check, and 29/30 -- and both sides of each.
	 */
	diff_begin("v34 handshake: ApplyBulkDelay");
	{
		static const short delays[] = {
			-32768, -100, -1, 0, 1, 2, 29, 30, 31,
			0x8f, 0x90, 0x91, 0x3fe, 0x3ff, 0x400, 32767
		};
		static const int pcm[][3] = {
			{ 0, 0, 0 }, { 0, 0, 1 }, { 0, 0, -3 },
			{ 1, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 }, { 2, 0, 7 }
		};
		static const int lens[] = { 0, 1, 0x40, 0x400 };
		static const short dmas[] = { 0, 100, -5 };
		unsigned d, p, l, m;
		long tag = 0;

		for (d = 0; d < sizeof(delays) / sizeof(delays[0]); d++)
		    for (p = 0; p < sizeof(pcm) / sizeof(pcm[0]); p++)
			for (l = 0; l < sizeof(lens) / sizeof(lens[0]); l++)
			    for (m = 0; m < sizeof(dmas) / sizeof(dmas[0]);
				 m++)
				run_bulkdelay(delays[d], pcm[p][0], pcm[p][1],
					      (short)pcm[p][2], lens[l],
					      dmas[m], tag++);

		/*
		 * And the transcripts, because the two rejections share one
		 * format string: nothing in the byte comparison can tell a
		 * reconstruction that printed the wrong one, or printed once
		 * where the object printed twice.  Finding 134.
		 */
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;
		tag = 0;
		for (d = 0; d < sizeof(delays) / sizeof(delays[0]); d++)
		    for (p = 0; p < sizeof(pcm) / sizeof(pcm[0]); p++)
			for (l = 0; l < sizeof(lens) / sizeof(lens[0]); l++) {
				dsplib_debug_capture_reset();
				run_bulkdelay(delays[d], pcm[p][0], pcm[p][1],
					      (short)pcm[p][2], lens[l], 100,
					      10000 + tag);
				diff_eq_int("ApplyBulkDelay transcript",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);
				diff_eq_int("ApplyBulkDelay transcript "
					    "non-empty",
					    dsplib_debug_capture_lines(1) > 0,
					    1, tag);
				tag++;
			}
		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	return rc;
}
