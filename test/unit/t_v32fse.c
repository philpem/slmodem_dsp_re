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
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/v32dec.h"
#include "dsplib/v32fse.h"

extern unsigned short ref_FSE_decision_4pt(void *s, short *a, short *m);
extern unsigned short ref_FSE_decision_trn(void *s, short *a, short *m);
extern unsigned short ref_FSE_decision_CD(void *s, short *a, short *m);
extern unsigned short ref_FSE_decision_AB(void *s, short *a, short *m);

#define NSYM	64

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
	return -1;
}

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
	diff_eq_int("magnitude out (%ld)", mag_a, mag_b, n);
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

	return rc;
}
