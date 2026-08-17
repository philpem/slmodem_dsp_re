/*
 * t_v32fse.c -- differential test of V.32's slicers.
 *
 * Every slicer is a state machine over the datapump object as well as a
 * decision, so each case drives OUR copy and the BLOB's from the same inputs
 * one symbol at a time and compares the whole object after every call.  A
 * slicer that decided right and shifted its history wrong would pass a
 * return-value check and fail this one.
 *
 * The one thing that cannot be compared directly is `cfg.decision`, since
 * each side installs its own build's successor; it is compared as an
 * identity instead, which is the check that the handover chain
 * AB -> CD -> trn -> 4pt is wired the same way on both sides.
 *
 * ===========================================================================
 * `_16pt` IS THE ODD ONE, AND ITS `*mag` IS GATED
 * ===========================================================================
 *
 * `FSE_decision_16pt` reads `DECv32_MAG9600` 8190, 16382 or 24574 bytes past
 * its base, depending on which ring the decided point is on (D302).  In this
 * binary the object is linked as `dsplibs_ref.o`, so those are reads at
 * `ref_DECv32_MAG9600 + 8190/16382/24574`.  The blob's `.data` is 0x9594
 * bytes and the table sits at 0x74f8, so:
 *
 *   inner ring, |I|+|Q| = 8192  -> .data+0x094f6, INSIDE the blob's own
 *                                  section, 158 bytes short of its end.  That
 *                                  byte is 0 and it travels with the blob
 *                                  into any link, so it is COMPARABLE.
 *   mid ring,   |I|+|Q| = 16384 -> 8034 bytes past the end of it
 *   outer ring, |I|+|Q| = 24576 -> 16226 bytes past the end of it
 *
 * The last two read whatever the LINKER put after the blob's `.data`, which
 * is a property of this binary and not of the code -- exactly finding 1603's
 * argument, now bounded to two rings instead of three.  So `*mag` is compared
 * on the inner ring ONLY, and the suite asserts both that it compared some
 * and that it excluded some of each other ring.  A run that quietly stopped
 * reaching the inner ring would report full coverage and have none.
 *
 * WHICH RING A TRIAL IS ON IS DERIVED FROM THE BLOB'S OWN OUTPUTS, not from
 * our copy of the search.  `*angle` alone cannot do it: `DECv32_ANGL9600` has
 * 12287 at points 0 and 3, 20480 at 5 and 6 and 4095 at 9 and 10, and each of
 * those pairs straddles inner and outer.  The PAIR (`*angle`, decision & 3)
 * is unique over all sixteen points, because the low two bits of the return
 * are the decided point's own index into `SMCv32_{I,Q}MAP16` -- every
 * `SMCv32_PMAP16` entry has its low two bits clear -- and the suite asserts
 * that uniqueness before it relies on it.
 *
 * AND THE SUITE HAS TO PROVIDE THE PAGES THE OBJECT READS, or it does not run
 * at all.  This binary's writable data is two LOAD segments -- `.data`, whose
 * last bytes are the blob's own, and then `.bss`, 64 K-aligned -- and both the
 * mid and the outer read land in the twenty-kilobyte hole between them.  The
 * process dies inside `ref_FSE_decision_16pt` on the first such symbol, before
 * anything at all can be compared, so `provide_oob_pages` maps that span and
 * asserts that it did.  WHAT LANDS THERE IS NEVER COMPARED: an anonymous page
 * reads zero and our reproduce-bugs arm writes zero, so comparing the two
 * would agree for a reason that has nothing to do with either implementation.
 * That is D65's trap and this is its shape here.
 */

#include <errno.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "harness.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/v32dec.h"
#include "dsplib/v32fse.h"

extern unsigned short ref_FSE_decision_4pt(void *s, short *a, short *m);
extern unsigned short ref_FSE_decision_trn(void *s, short *a, short *m);
extern unsigned short ref_FSE_decision_CD(void *s, short *a, short *m);
extern unsigned short ref_FSE_decision_AB(void *s, short *a, short *m);
extern unsigned short ref_FSE_decision_16pt(void *s, short *a, short *m);

extern short ref_DECv32_MAG9600[3];
extern short ref_DECv32_ANGL9600[16];
extern short ref_DECv32_IMAP16[16];
extern short ref_DECv32_QMAP16[16];
extern const short ref_SMCv32_IMAP16[17];
extern const short ref_SMCv32_QMAP16[17];

#define NSYM	64

/* The three values `|I| + |Q|` can take over the sixteen-point set. */
#define RING_INNER	8192
#define RING_MID	16384
#define RING_OUTER	24576

static struct fpm_fse st_a, st_b;
static struct v32_dec dec_a, dec_b;
static short i_a[NSYM], q_a[NSYM], i_b[NSYM], q_b[NSYM];

/* Which slicer a `cfg.decision` names, on whichever side it came from. */
static int
slicer_id(fpm_fse_decision f)
{
	if (f == 0)
		return 0;
	if (f == FSE_decision_4pt || f == (fpm_fse_decision)ref_FSE_decision_4pt)
		return 1;
	if (f == FSE_decision_trn || f == (fpm_fse_decision)ref_FSE_decision_trn)
		return 2;
	if (f == FSE_decision_CD || f == (fpm_fse_decision)ref_FSE_decision_CD)
		return 3;
	if (f == FSE_decision_AB || f == (fpm_fse_decision)ref_FSE_decision_AB)
		return 4;
	if (f == FSE_decision_16pt
	    || f == (fpm_fse_decision)ref_FSE_decision_16pt)
		return 5;
	return -1;
}

/* ------------------------------------------------------------------ */
/* The pages the object reads and does not own                         */
/* ------------------------------------------------------------------ */

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE	0x100000
#endif

/*
 * A byte neither the object nor the reconstruction can produce in `*mag`:
 * 0x5a5a is 23130, and `DECv32_MAG9600` holds 5792, 12953 and 17378.
 */
#define OOB_POISON	0x5a

/*
 * Is this page mapped?  Asked of the kernel rather than by touching it,
 * because touching it is the fault we are trying not to take.  `mincore`
 * reports ENOMEM for a range that is not mapped and says nothing about
 * whether it is resident, which is what we want.
 *
 * NOT `write(devnull, p, 1)`, which is the obvious probe and is WRONG: a
 * write to /dev/null is discarded without the buffer ever being read, so it
 * succeeds for an unmapped address and the probe reports every page present.
 * That version was written first and the suite went on segfaulting.
 */
static int
page_mapped(unsigned long p, unsigned long psz)
{
	unsigned char vec;

	errno = 0;
	if (mincore((void *)p, (size_t)psz, &vec) == 0)
		return 1;
	return errno != ENOMEM;
}

/*
 * The three reads are at `ref_DECv32_MAG9600 + 8190`, `+ 16382` and `+ 24574`
 * -- see the header.  Map whatever pages of that span this link left out, and
 * return whether the whole span is now readable.
 *
 * THE PAGES WE PROVIDE ARE POISONED, AND THAT IS THE POINT.  A fresh
 * anonymous page reads zero, our reproduce-bugs arm writes zero, and a mid-
 * or outer-ring comparison would therefore PASS -- for a reason that is
 * neither implementation's.  Measured: with the pages left at zero, comparing
 * all three rings is green over 1,016 trials and proves nothing.  Filling
 * them with a value neither side can produce makes the exclusion
 * self-enforcing: anyone who widens the gate gets a loud failure carrying
 * 0x5a5a rather than a quiet pass.  Pages that were ALREADY mapped are never
 * written -- the inner ring's page is one of the blob's own.
 */
static int
provide_oob_pages(void)
{
	unsigned long base = (unsigned long)(const char *)ref_DECv32_MAG9600;
	unsigned long psz = (unsigned long)sysconf(_SC_PAGESIZE);
	unsigned long lo, hi, p;

	if (psz == 0 || (psz & (psz - 1)) != 0)
		return 0;
	lo = (base + 8190) & ~(psz - 1);
	hi = base + 24576;

	for (p = lo; p < hi; p += psz)
		if (!page_mapped(p, psz)
		    && mmap((void *)p, (size_t)psz, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
			    -1, 0) == (void *)p)
			memset((void *)p, OOB_POISON, (size_t)psz);

	for (p = lo; p < hi; p += psz)
		if (!page_mapped(p, psz))
			return 0;
	return 1;
}

/* ------------------------------------------------------------------ */
/* The `_16pt` ring oracle, built from the BLOB's tables                */
/* ------------------------------------------------------------------ */

/*
 * For each of the sixteen decided points: the angle the object writes, the
 * low two bits of the value it returns, and the ring the point is on.  All
 * three come out of the blob's own tables, so nothing here trusts our copy of
 * the search or of the constellation.
 */
static short cls_angle[16];
static short cls_low2[16];
static int cls_ring[16];

static void
cls_build(void)
{
	int k, j;

	for (k = 0; k < 16; k++) {
		int ib = ref_DECv32_IMAP16[k];
		int qb = ref_DECv32_QMAP16[k];
		int found = 0;

		for (j = 0; j <= 15; j++)
			if (ref_SMCv32_IMAP16[j] == ib
			    && ref_SMCv32_QMAP16[j] == qb) {
				found = j;
				break;
			}

		cls_angle[k] = ref_DECv32_ANGL9600[k];
		cls_low2[k] = (short)(found & 3);
		cls_ring[k] = (ib < 0 ? -ib : ib) + (qb < 0 ? -qb : qb);
	}
}

/* Which point the object decided, or -1.  See the header for why this pair. */
static int
cls_point(short ang, unsigned short dec)
{
	int k;

	for (k = 0; k < 16; k++)
		if (cls_angle[k] == ang && cls_low2[k] == (short)(dec & 3))
			return k;
	return -1;
}

/*
 * A REPLICA OF THE OBJECT'S WRAPPED SEARCH, USED ONLY TO CHOOSE INPUTS.
 * Nothing is gated or counted on it -- the counters come from `cls_point` --
 * so a wrong replica here costs coverage and cannot manufacture it.
 */
static int
replica_16(int i, int q)
{
	int k, best = 0;
	short min = 0x7fff;

	for (k = 0; k <= 15; k++) {
		int di = (short)(i - ref_DECv32_IMAP16[k]);
		int dq = (short)(q - ref_DECv32_QMAP16[k]);
		short d = (short)((unsigned)(di * di) + (unsigned)(dq * dq));

		if (d < min) {
			best = k;
			min = d;
		}
	}
	return best;
}

/*
 * `_16pt` mode for `step`: `*mag` is compared only where the object's
 * out-of-bounds read stays inside its own `.data`.
 */
static int mag_gate_on;
static long mag_compared, mag_skipped_mid, mag_skipped_outer, gate_trials;
static long point_seen[16];

static unsigned seed;

static int
rnd(int range)
{
	seed = seed * 1103515245u + 12345u;
	return (int)((seed >> 13) % (unsigned)range);
}

/*
 * Both sides start from the same zeroed objects.  The states are ours on
 * both sides -- the blob's slicers only read fields at fixed offsets -- so
 * only `owner`, the two symbol buffers and `decision` differ.
 */
static void
setup(fpm_fse_decision ours, fpm_fse_decision theirs)
{
	memset(&st_a, 0, sizeof(st_a));
	memset(&st_b, 0, sizeof(st_b));
	memset(&dec_a, 0, sizeof(dec_a));
	memset(&dec_b, 0, sizeof(dec_b));
	memset(i_a, 0, sizeof(i_a));
	memset(q_a, 0, sizeof(q_a));
	memset(i_b, 0, sizeof(i_b));
	memset(q_b, 0, sizeof(q_b));

	st_a.cfg = FSEv32_CFG;
	st_b.cfg = FSEv32_CFG;
	st_a.cfg.owner = &dec_a;
	st_b.cfg.owner = &dec_b;
	st_a.cfg.decision = ours;
	st_b.cfg.decision = theirs;
	st_a.out_i = i_a;
	st_a.out_q = q_a;
	st_b.out_i = i_b;
	st_b.out_q = q_b;
}

/* Everything both sides must agree on after one symbol. */
static void
compare(long n)
{
	diff_eq_obj("decoder object", struct v32_dec, &dec_a, &dec_b, n);
	diff_eq_int("mu_sel (%ld)", st_a.mu_sel, st_b.mu_sel, n);
	diff_eq_int("lms_on (%ld)", st_a.lms_on, st_b.lms_on, n);
	diff_eq_int("installed slicer (%ld)", slicer_id(st_a.cfg.decision),
		    slicer_id(st_b.cfg.decision), n);
	diff_eq_int("slicer is a known one (%ld)",
		    slicer_id(st_b.cfg.decision) >= 0, 1, n);
}

/*
 * One symbol through both sides.  `ai`/`aq` are the equaliser's output for
 * this symbol, `ang` and `mag` the in/out pair the carrier loop reads back.
 */
static unsigned short
step(unsigned short (*ours)(struct fpm_fse *, short *, short *),
     unsigned short (*theirs)(void *, short *, short *),
     int slot, short ai, short aq, short ang, short mag, long n)
{
	short ang_a = ang, ang_b = ang;
	short mag_a = mag, mag_b = mag;
	unsigned short ra, rb;

	st_a.n_out = (unsigned short)slot;
	st_b.n_out = (unsigned short)slot;
	i_a[slot] = i_b[slot] = ai;
	q_a[slot] = q_b[slot] = aq;

	rb = theirs(&st_b, &ang_b, &mag_b);
	ra = ours(&st_a, &ang_a, &mag_a);

	diff_eq_int("decision (%ld)", ra, rb, n);
	diff_eq_int("angle out (%ld)", ang_a, ang_b, n);

	if (!mag_gate_on)
		diff_eq_int("magnitude out (%ld)", mag_a, mag_b, n);
	else {
		int pt = cls_point(ang_b, rb);

		gate_trials++;
		diff_eq_int("the ring oracle classified trial %ld", pt >= 0,
			    1, n);
		if (pt >= 0) {
			point_seen[pt]++;
			if (cls_ring[pt] == RING_INNER) {
				diff_eq_int("magnitude out, inner ring (%ld)",
					    mag_a, mag_b, n);
				mag_compared++;
			} else if (cls_ring[pt] == RING_MID)
				mag_skipped_mid++;
			else
				mag_skipped_outer++;
		}
	}

	compare(n);
	return rb;
}

/* The four constellation points the 4800 bit/s slicer decides between. */
static const short pt_i[4] = { -4096, -12288, 4096, 12288 };
static const short pt_q[4] = { 12288, -4096, -12288, 4096 };

static int
run_cd(void)
{
	int k;

	diff_begin("FSE_decision_CD: fifteen symbols, then trn");
	setup(FSE_decision_CD, (fpm_fse_decision)ref_FSE_decision_CD);

	/* Twice round the count, so the handover is taken more than once. */
	for (k = 0; k < 40; k++)
		step(FSE_decision_CD, ref_FSE_decision_CD, k % NSYM,
		     (short)rnd(30000), (short)rnd(30000),
		     (short)rnd(30000), (short)rnd(30000), k);

	diff_eq_int("it reached trn (%ld)",
		    slicer_id(st_b.cfg.decision) == 2, 1, 0);
	return diff_end();
}

static int
run_trn(void)
{
	int k;
	int saw_map = 0;
	unsigned short d;

	diff_begin("FSE_decision_trn: 1280 symbols, then 4pt");
	setup(FSE_decision_trn, (fpm_fse_decision)ref_FSE_decision_trn);

	/*
	 * The register has to be non-zero and the tap has to move, or the
	 * generator produces one value for ever and the `MAP_TRN` phase is
	 * never distinguishable from the forced one.
	 */
	dec_a.scram = dec_b.scram = 0x2a5f31c7u;
	dec_a.scram_tap = dec_b.scram_tap = 5;

	for (k = 0; k < 1400; k++) {
		if (k == 700) {
			dec_a.scram_tap = dec_b.scram_tap = 11;
			dec_a.chan = dec_b.chan = 3;
		}
		d = step(FSE_decision_trn, ref_FSE_decision_trn, k % NSYM,
			 (short)rnd(30000), (short)rnd(30000),
			 (short)rnd(30000), (short)rnd(30000), k);
		/*
		 * The first 256 symbols can only ever decide 1 or 3, so a 0
		 * or a 2 is proof the run got past them and into the mapped
		 * phase rather than merely counting.
		 */
		if (d == 0 || d == 2)
			saw_map = 1;
	}

	diff_eq_int("it reached 4pt (%ld)",
		    slicer_id(st_b.cfg.decision) == 1, 1, 0);
	diff_eq_int("a decision outside {1,3} appeared (%ld)", saw_map, 1, 0);
	return diff_end();
}

static int
run_4pt(void)
{
	int k;
	int saw1 = 0, saw2 = 0;

	diff_begin("FSE_decision_4pt: four points and the quality measure");

	/* Pass A: a stationary point, so eqm decays and the count runs up. */
	setup(FSE_decision_4pt, (fpm_fse_decision)ref_FSE_decision_4pt);
	for (k = 0; k < 90; k++) {
		step(FSE_decision_4pt, ref_FSE_decision_4pt, k % NSYM,
		     4096, 12288, (short)rnd(30000), (short)rnd(30000), k);
		if (dec_b.retrain == 1)
			saw1 = 1;
	}

	/* Pass B: far-apart points, so eqm saturates and the count resets. */
	setup(FSE_decision_4pt, (fpm_fse_decision)ref_FSE_decision_4pt);
	for (k = 0; k < 120; k++) {
		int p = rnd(4);

		step(FSE_decision_4pt, ref_FSE_decision_4pt, k % NSYM,
		     (short)(pt_i[p] + rnd(6000) - 3000),
		     (short)(pt_q[p] + rnd(6000) - 3000),
		     (short)rnd(30000), (short)rnd(30000), 1000 + k);
	}

	/*
	 * Pass C: quiet for twenty-five symbols and then a jump, which is the
	 * window -- count between 21 and 59 with eqm still small -- that asks
	 * for the second kind of retrain.
	 */
	setup(FSE_decision_4pt, (fpm_fse_decision)ref_FSE_decision_4pt);
	for (k = 0; k < 200; k++) {
		short ii = (short)((k % 30 == 29) ? -12288 : 4096);
		short qq = (short)((k % 30 == 29) ? -4096 : 12288);

		step(FSE_decision_4pt, ref_FSE_decision_4pt, k % NSYM, ii, qq,
		     (short)rnd(30000), (short)rnd(30000), 2000 + k);
		if (dec_b.retrain == 2)
			saw2 = 1;
	}

	/* Pass D: every channel of the differential state gets used. */
	setup(FSE_decision_4pt, (fpm_fse_decision)ref_FSE_decision_4pt);
	for (k = 0; k < 160; k++) {
		int p = rnd(4);

		dec_a.chan = dec_b.chan = (unsigned short)(k % 8);
		step(FSE_decision_4pt, ref_FSE_decision_4pt, k % NSYM,
		     pt_i[p], pt_q[p], (short)rnd(30000), (short)rnd(30000),
		     3000 + k);
	}

	diff_eq_int("the count reached sixty (%ld)", saw1, 1, 0);
	diff_eq_int("the quiet-then-jump retrain fired (%ld)", saw2, 1, 0);
	return diff_end();
}

static int
run_ab(void)
{
	int k;

	diff_begin("FSE_decision_AB: the phase-alternating segment");

	/*
	 * A steadily advancing angle, so the phase step alternates between
	 * the two branches, with symbols close enough together that the
	 * quality test does not fire until the count allows it.
	 */
	setup(FSE_decision_AB, (fpm_fse_decision)ref_FSE_decision_AB);
	for (k = 0; k < 300; k++)
		step(FSE_decision_AB, ref_FSE_decision_AB, k % NSYM,
		     (short)(4096 + rnd(400)), (short)(12288 + rnd(400)),
		     (short)((k * 0x2000) & 0xffff), (short)(9000 + rnd(200)),
		     k);

	diff_eq_int("it stayed in AB (%ld)",
		    slicer_id(st_b.cfg.decision) == 4, 1, 0);

	/*
	 * Now with the symbols scattered, which drives the sum of squares
	 * over the threshold and takes the exit into CD once the count is
	 * past 151.
	 */
	setup(FSE_decision_AB, (fpm_fse_decision)ref_FSE_decision_AB);
	for (k = 0; k < 400; k++) {
		int p = rnd(4);

		step(FSE_decision_AB, ref_FSE_decision_AB, k % NSYM,
		     (short)(pt_i[p] + rnd(2000)), (short)(pt_q[p] + rnd(2000)),
		     (short)rnd(65535), (short)rnd(20000), 1000 + k);
	}

	diff_eq_int("it handed over to CD (%ld)",
		    slicer_id(st_b.cfg.decision) == 3, 1, 0);

	/* And a sweep of the differential channel. */
	setup(FSE_decision_AB, (fpm_fse_decision)ref_FSE_decision_AB);
	for (k = 0; k < 160; k++) {
		dec_a.chan = dec_b.chan = (unsigned short)(k % 8);
		step(FSE_decision_AB, ref_FSE_decision_AB, k % NSYM,
		     (short)rnd(30000), (short)rnd(30000),
		     (short)rnd(65535), (short)rnd(30000), 2000 + k);
	}

	return diff_end();
}

/*
 * THE EIGHT POINTS `_16pt` CAN NEVER DECIDE, and it is a proof rather than an
 * observation.  Write `I[k] = 4096*a`, `Q[k] = 4096*b` with a, b in
 * {+-1, +-3}.  Then
 *
 *     (i - I[k])^2 + (q - Q[k])^2  ==  i^2 + q^2 - 8192*(a*i + b*q)  (mod 2^16)
 *
 * because every I[k]^2 and Q[k]^2 is a multiple of 2^24.  `8192 * x mod 2^16`
 * depends only on `x mod 8`, so the metric takes at most EIGHT distinct
 * values over the sixteen points, and two points sharing a value tie -- which
 * the strict `<` awards to the lower index.  Only the first index carrying
 * each value can therefore ever win, and the union of those over all
 * sixty-four residue pairs `(i mod 8, q mod 8)` is exactly this list.
 */
static const int reachable[8] = { 0, 1, 2, 3, 4, 5, 8, 10 };

static int
is_reachable(int k)
{
	int j;

	for (j = 0; j < 8; j++)
		if (reachable[j] == k)
			return 1;
	return 0;
}

/*
 * The standing checks behind the FIXED arm of D302.  The differential tier is
 * built with -DDSPLIB_REPRODUCE_BUGS, so `*mag = DECv32_MAG9600[(n >> 13) -
 * 1]` is compiled OUT of this binary and no differential test can reach it.
 * These are what stands behind it instead: that the table is the blob's, that
 * its three entries are the constellation's three L2 magnitudes to the unit,
 * and that `>> 13` less one addresses them exactly over the only three sums
 * the sixteen points can produce.
 */
static void
check_mag_table(void)
{
	static const int ring_i[3] = { 4096, 12288, 12288 };
	static const int ring_q[3] = { 4096, 4096, 12288 };
	int k;

	for (k = 0; k < 3; k++) {
		double want = sqrt((double)ring_i[k] * ring_i[k]
				   + (double)ring_q[k] * ring_q[k]);

		diff_eq_int("MAG9600[%ld] matches the blob's",
			    DECv32_MAG9600[k], ref_DECv32_MAG9600[k], k);
		diff_eq_int("MAG9600[%ld] is the L2 magnitude to the unit",
			    fabs((double)DECv32_MAG9600[k] - want) < 1.0, 1, k);
	}

	for (k = 0; k < 16; k++) {
		int s = cls_ring[k];

		diff_eq_int("IMAP16[%ld] matches the blob's",
			    DECv32_IMAP16[k], ref_DECv32_IMAP16[k], k);
		diff_eq_int("QMAP16[%ld] matches the blob's",
			    DECv32_QMAP16[k], ref_DECv32_QMAP16[k], k);
		diff_eq_int("ANGL9600[%ld] matches the blob's",
			    DECv32_ANGL9600[k], ref_DECv32_ANGL9600[k], k);
		diff_eq_int("|I|+|Q| for point %ld is one of the three rings",
			    s == RING_INNER || s == RING_MID || s == RING_OUTER,
			    1, k);
		diff_eq_int("(|I|+|Q| >> 13) - 1 indexes MAG9600 for point %ld",
			    (s >> 13) - 1 >= 0 && (s >> 13) - 1 <= 2, 1, k);
	}

	/* And that the object's own shift does not. */
	diff_eq_int("the object's (|I|+|Q| >> 1) - 1 is off the table (%ld)",
		    (RING_INNER >> 1) - 1, 4095, 0);
}

/*
 * The oracle has to be unique before anything may be gated on it: two points
 * sharing an (angle, decision & 3) pair would let an outer-ring trial be
 * counted as an inner-ring one and compared against a byte the link owns.
 */
static void
check_oracle(void)
{
	int k, j;

	for (k = 0; k < 16; k++)
		for (j = k + 1; j < 16; j++)
			diff_eq_int("point %ld has a pair no other point has",
				    cls_angle[k] == cls_angle[j]
				    && cls_low2[k] == cls_low2[j], 0, k);
}

static int
run_16pt(void)
{
	static const int edge[] = {
		0, 1, -1, 2896, -2896, 4096, -4096, 8191, 8192, 8193,
		-8191, -8192, -8193, 12288, -12288, 16384, -16384,
		23170, -23170, 30000
	};
	int nedge = (int)(sizeof(edge) / sizeof(edge[0]));
	int k, j, slot = 0;
	long trial = 0;
	int saw1 = 0, saw2 = 0;
	int replica_seen[16];

	diff_begin("FSE_decision_16pt: 9600 bit/s, no trellis");

	cls_build();
	check_mag_table();
	check_oracle();

	/*
	 * Before a single symbol goes through: the object's own reads have to
	 * land somewhere.  A failure here is this assertion rather than a
	 * segmentation fault inside the blob, and the suite stops.
	 */
	if (!provide_oob_pages()) {
		diff_eq_int("the object's out-of-bounds reads are mapped (%ld)",
			    0, 1, 0);
		return diff_end();
	}
	diff_eq_int("the object's out-of-bounds reads are mapped (%ld)", 1, 1,
		    0);

	memset(point_seen, 0, sizeof(point_seen));
	memset(replica_seen, 0, sizeof(replica_seen));
	mag_compared = mag_skipped_mid = mag_skipped_outer = 0;
	gate_trials = 0;

	/*
	 * Reseeded, so this suite's coverage does not depend on how many
	 * values the four suites above happened to draw.
	 */
	seed = 20250812u;

	setup(FSE_decision_16pt, (fpm_fse_decision)ref_FSE_decision_16pt);
	mag_gate_on = 1;

	/*
	 * Pass A: the sixteen constellation points EXACTLY.  Every squared
	 * difference is then a multiple of 2**26 and vanishes in the low
	 * sixteen bits, so all sixteen candidates score zero and the object
	 * decides point 0 sixteen times out of sixteen.  That is D451, driven
	 * rather than argued.
	 */
	for (k = 0; k < 16; k++) {
		replica_seen[replica_16(DECv32_IMAP16[k], DECv32_QMAP16[k])]++;
		step(FSE_decision_16pt, ref_FSE_decision_16pt, slot++ % NSYM,
		     DECv32_IMAP16[k], DECv32_QMAP16[k],
		     (short)rnd(30000), (short)rnd(30000), trial++);
	}
	diff_eq_int("the exact lattice collapsed onto point 0 (%ld)",
		    point_seen[0], 16, 0);

	/* Pass B: the edge list crossed with itself. */
	for (k = 0; k < nedge; k++)
		for (j = 0; j < nedge; j++) {
			replica_seen[replica_16(edge[k], edge[j])]++;
			step(FSE_decision_16pt, ref_FSE_decision_16pt,
			     slot++ % NSYM, (short)edge[k], (short)edge[j],
			     (short)rnd(30000), (short)rnd(30000), trial++);
		}

	/*
	 * Pass C: the lattice with a small error on it, which is the only
	 * thing that reaches the inner ring often.  Point 3 is the ONLY inner
	 * point the wrapped metric can decide, so the mag comparison lives or
	 * dies here.
	 */
	for (k = 0; k < 300; k++) {
		int p = k % 16;
		short ii = (short)(DECv32_IMAP16[p] + rnd(1200) - 600);
		short qq = (short)(DECv32_QMAP16[p] + rnd(1200) - 600);

		replica_seen[replica_16(ii, qq)]++;
		step(FSE_decision_16pt, ref_FSE_decision_16pt, slot++ % NSYM,
		     ii, qq, (short)rnd(30000), (short)rnd(30000), trial++);
	}

	/* Pass D: the plane at random. */
	for (k = 0; k < 300; k++) {
		short ii = (short)(rnd(60000) - 30000);
		short qq = (short)(rnd(60000) - 30000);

		replica_seen[replica_16(ii, qq)]++;
		step(FSE_decision_16pt, ref_FSE_decision_16pt, slot++ % NSYM,
		     ii, qq, (short)rnd(30000), (short)rnd(30000), trial++);
	}

	/* Pass E: every channel of the differential state. */
	for (k = 0; k < 160; k++) {
		int p = k % 16;
		short ii = (short)(DECv32_IMAP16[p] + rnd(1200) - 600);
		short qq = (short)(DECv32_QMAP16[p] + rnd(1200) - 600);

		dec_a.chan = dec_b.chan = (unsigned short)(k % 8);
		replica_seen[replica_16(ii, qq)]++;
		step(FSE_decision_16pt, ref_FSE_decision_16pt, slot++ % NSYM,
		     ii, qq, (short)rnd(30000), (short)rnd(30000), trial++);
	}

	/*
	 * The inlined quality preamble has its own copy in here too.  Still
	 * under the gate: these inputs decide outer-ring points and the
	 * object reads the provided page for every one of them.
	 */
	setup(FSE_decision_16pt, (fpm_fse_decision)ref_FSE_decision_16pt);
	for (k = 0; k < 90; k++) {
		replica_seen[replica_16(4096, 12288)]++;
		step(FSE_decision_16pt, ref_FSE_decision_16pt, k % NSYM,
		     4096, 12288, (short)rnd(30000), (short)rnd(30000),
		     100000 + k);
		trial++;
		if (dec_b.retrain == 1)
			saw1 = 1;
	}
	setup(FSE_decision_16pt, (fpm_fse_decision)ref_FSE_decision_16pt);
	for (k = 0; k < 200; k++) {
		short ii = (short)((k % 30 == 29) ? -12288 : 4096);
		short qq = (short)((k % 30 == 29) ? -4096 : 12288);

		replica_seen[replica_16(ii, qq)]++;
		step(FSE_decision_16pt, ref_FSE_decision_16pt, k % NSYM, ii, qq,
		     (short)rnd(30000), (short)rnd(30000), 200000 + k);
		trial++;
		if (dec_b.retrain == 2)
			saw2 = 1;
	}
	diff_eq_int("the count reached sixty (%ld)", saw1, 1, 0);
	diff_eq_int("the quiet-then-jump retrain fired (%ld)", saw2, 1, 0);

	mag_gate_on = 0;

	/*
	 * COVERAGE, and it is the point of the whole file.  Eight points are
	 * reachable and eight are not; both halves are asserted, because a
	 * metric that had been "tidied" with the shift its siblings have
	 * would make the other eight reachable and this is where that shows.
	 */
	for (k = 0; k < 16; k++) {
		diff_eq_int("point %ld was decided at least once",
			    point_seen[k] > 0, is_reachable(k), k);
		diff_eq_int("our search agrees on how often point %ld won",
			    replica_seen[k], (int)point_seen[k], k);
	}

	/*
	 * AND THE GATE ITSELF.  `mag_compared` is the only mag coverage this
	 * function has; the two skip counts are the assertion that the
	 * exclusion happened rather than being decorative.
	 */
	diff_eq_int("*mag was compared on the inner ring (%ld)",
		    mag_compared > 0, 1, mag_compared);
	diff_eq_int("*mag was EXCLUDED on the mid ring (%ld)",
		    mag_skipped_mid > 0, 1, mag_skipped_mid);
	diff_eq_int("*mag was EXCLUDED on the outer ring (%ld)",
		    mag_skipped_outer > 0, 1, mag_skipped_outer);
	diff_eq_int("every gated trial was classified (%ld)",
		    mag_compared + mag_skipped_mid + mag_skipped_outer,
		    gate_trials, 0);
	diff_eq_int("the gate saw every symbol driven (%ld)", gate_trials,
		    trial, 0);

	diff_eq_int("it installed no successor (%ld)",
		    slicer_id(st_b.cfg.decision), 5, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	diff_begin("v32_dec: the datapump object's layout");
	diff_eq_int("prev_sym (%ld)",
		    (long)((char *)&dec_a.prev_sym - (char *)&dec_a), 0x08, 0);
	diff_eq_int("ang_prev (%ld)",
		    (long)((char *)&dec_a.ang_prev - (char *)&dec_a), 0x50, 0);
	diff_eq_int("sym_i (%ld)",
		    (long)((char *)&dec_a.sym_i - (char *)&dec_a), 0x52, 0);
	diff_eq_int("eqm (%ld)",
		    (long)((char *)&dec_a.eqm - (char *)&dec_a), 0x5e, 0);
	diff_eq_int("retrain (%ld)",
		    (long)((char *)&dec_a.retrain - (char *)&dec_a), 0x62, 0);
	diff_eq_int("rate_change (%ld)",
		    (long)((char *)&dec_a.rate_change - (char *)&dec_a),
		    0x64, 0);
	diff_eq_int("scram_tap (%ld)",
		    (long)((char *)&dec_a.scram_tap - (char *)&dec_a),
		    0x6c, 0);
	diff_eq_int("count (%ld)",
		    (long)((char *)&dec_a.count - (char *)&dec_a), 0x6e, 0);
	diff_eq_int("scram (%ld)",
		    (long)((char *)&dec_a.scram - (char *)&dec_a), 0x70, 0);
	diff_eq_int("sizeof (%ld)", (long)sizeof(struct v32_dec), 0x74, 0);
	rc |= diff_end();

	seed = 20250812u;
	rc |= run_cd();
	rc |= run_trn();
	rc |= run_4pt();
	rc |= run_ab();
	rc |= run_16pt();

	return rc;
}
