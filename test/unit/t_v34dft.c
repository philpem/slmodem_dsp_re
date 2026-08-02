/*
 * t_v34dft.c -- differential test of the V.34 sliding-DFT bank.
 *
 * Three things need proving here and they need different tests.
 *
 * The TABLE is data, so it is compared entry for entry against the object's
 * own copy.  That is the only check in this file that could catch a
 * transcription slip, and it is worth having on its own because 255 of the
 * 256 entries are derivable and one is not (see dftc.c).
 *
 * The INTEGER path is ordinary fixed point and compares as bytes.
 *
 * The DOUBLE path is the one that needs care.  `denergy` is computed as two
 * squares and a sum at 80-bit extended precision and rounded once, on the
 * store.  Compared as a `double ==` that would pass on a build that rounded
 * three times instead, because the two agree to about fifteen digits; so it
 * is compared as eight raw bytes, which does not.  That check is the reason
 * this file exists as well as the reason the project sets -mfpmath=387 and
 * refuses -ffloat-store.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v34det.h"

extern const short ref_costbl[256];
extern short ref_cosread(unsigned char idx);
extern void ref_dftupdate(void *bins, short nbins, const short *samples,
			  short nsamples);
extern void ref_dftenergy(void *bins, short nbins, short scale);

#define SRATE		9600
#define NSAMP		1024
#define NBINS		6

/* Coverage. */
static int saw_shift_default;	/* the normalisation search fell through   */
static int saw_shift_found;	/* it found a bit                          */
static int saw_negative_acc;	/* an accumulator went negative            */

static short sig[NSAMP];

/*
 * The bins, as the object sees them: a plain byte array for the reference
 * side, so nothing this file declares can accidentally make the two agree.
 */
static struct v34_dftbin ours[NBINS];
static unsigned char ref[NBINS * 0x2c];

static void
compare_bins(const char *what, int n)
{
	const unsigned char *a = (const unsigned char *)ours;
	int i;

	for (i = 0; i < n * (int)sizeof(struct v34_dftbin); i++)
		diff_eq_int(what, a[i], ref[i], i);
}

/* phase step for `hz`: the accumulator is 14 bits per turn. */
static short
inc_for(int hz)
{
	return (short)(16384L * hz / SRATE);
}

static void
setup(const int *hz, int n)
{
	int i;

	/*
	 * Fill first, so every field the constructor-less object does not set
	 * is the same non-zero pattern on both sides.  There is no
	 * `dftinit` in the object -- the caller zeroes these itself -- so
	 * this is also the only place the test can establish that dftupdate
	 * touches exactly the fields it should and no others.
	 */
	memset(ours, HARNESS_MALLOC_FILL, sizeof(ours));
	memset(ref, HARNESS_MALLOC_FILL, sizeof(ref));

	for (i = 0; i < n; i++) {
		struct v34_dftbin z;

		memset(&z, 0, sizeof(z));
		z.inc = inc_for(hz[i]);
		z.phase = 0;
		ours[i] = z;
		memcpy(ref + i * sizeof(z), &z, sizeof(z));
	}
}

static void
fill_tone(int hz, int amp)
{
	unsigned phase = 0;
	unsigned inc = (unsigned)inc_for(hz);
	int i;

	for (i = 0; i < NSAMP; i++) {
		phase = (phase + inc) & V34_DFT_PHASE_MASK;
		sig[i] = (short)((costbl[phase >> V34_DFT_PHASE_SHIFT] * amp)
				 >> 14);
	}
}

/*
 * One run: correlate `nsamp` samples in blocks of `block`, then reduce with
 * `scale`, comparing after every step.
 */
static void
run(const char *what, const int *hz, int nbins, int nsamp, int block,
    short scale)
{
	int i;

	setup(hz, nbins);

	for (i = 0; i + block <= nsamp; i += block) {
		dftupdate(ours, (short)nbins, sig + i, (short)block);
		ref_dftupdate(ref, (short)nbins, sig + i, (short)block);
		compare_bins(what, nbins);
	}

	dftenergy(ours, (short)nbins, scale);
	ref_dftenergy(ref, (short)nbins, scale);
	compare_bins(what, nbins);

	for (i = 0; i < nbins; i++) {
		if (ours[i].shift == 0x14)
			saw_shift_default++;
		else
			saw_shift_found++;
		if (ours[i].acc_re < 0 || ours[i].acc_im < 0)
			saw_negative_acc++;
	}
}

int
main(void)
{
	static const int probe[NBINS] = { 150, 600, 1200, 2100, 3000, 3600 };
	int rc = 0;
	int i;

	diff_begin("v34 costbl: the table itself");
	for (i = 0; i < 256; i++)
		diff_eq_int("costbl[%ld]", costbl[i], ref_costbl[i], i);
	rc |= diff_end();

	diff_begin("v34 cosread: every input");
	/*
	 * All 256, which is the whole domain -- the parameter is a byte and
	 * the original truncates it before the load, so there is no input
	 * this does not cover.
	 */
	for (i = 0; i < 256; i++)
		diff_eq_int("cosread(%ld)", cosread((unsigned char)i),
			    ref_cosread((unsigned char)i), i);
	rc |= diff_end();

	diff_begin("v34 dft: a tone against a bank of bins");

	fill_tone(2100, 12000);
	run("2100 Hz, 1024 samples in one block[%ld]", probe, NBINS, 1024,
	    1024, 0);
	run("2100 Hz, blocks of 40[%ld]", probe, NBINS, 1000, 40, 0);
	/*
	 * The same input reduced with a non-zero scale, which is the shift
	 * the caller uses to move the integer path's saturation point.
	 */
	run("2100 Hz, scale 4[%ld]", probe, NBINS, 1000, 40, 4);
	run("2100 Hz, scale 12[%ld]", probe, NBINS, 1000, 40, 12);
	/*
	 * A scale of 256 exercises the byte truncation: it must shift by
	 * zero, not by 256 and not by 256 & 31.
	 */
	run("2100 Hz, scale 256[%ld]", probe, NBINS, 1000, 40, 256);

	rc |= diff_end();

	diff_begin("v34 dft: quiet, loud, and one sample at a time");

	memset(sig, 0, sizeof(sig));
	run("silence[%ld]", probe, NBINS, 1000, 40, 0);

	/*
	 * Full scale for long enough to run the integer accumulators past
	 * 32 bits, which is where the wrapping matters and where a
	 * reconstruction that had let the optimiser assume no overflow would
	 * diverge.
	 */
	fill_tone(2100, 32767);
	run("full scale, 1024 samples[%ld]", probe, NBINS, 1024, 1024, 0);
	run("full scale, one sample per call[%ld]", probe, NBINS, 512, 1, 0);

	/* A single bin, and a zero-length update, which must be a no-op. */
	run("one bin[%ld]", probe, 1, 1000, 40, 0);
	setup(probe, NBINS);
	dftupdate(ours, NBINS, sig, 0);
	ref_dftupdate(ref, NBINS, sig, 0);
	compare_bins("zero samples changed nothing[%ld]", NBINS);
	dftupdate(ours, 0, sig, 40);
	ref_dftupdate(ref, 0, sig, 40);
	compare_bins("zero bins changed nothing[%ld]", NBINS);

	rc |= diff_end();

	diff_begin("v34 dft: it behaves like a DFT");

	fill_tone(2100, 12000);
	run("tuned bin dominates[%ld]", probe, NBINS, 1024, 1024, 0);
	printf("  energy by bin:");
	for (i = 0; i < NBINS; i++)
		printf(" %d:%d", probe[i], ours[i].energy);
	printf("\n");

	/*
	 * Not a differential check.  Both sides would agree on a bank that
	 * correlated against the wrong table entirely, and this is what
	 * would notice.
	 */
	for (i = 0; i < NBINS; i++)
		if (probe[i] != 2100)
			diff_eq_int("the 2100 Hz bin beats bin %ld",
				    ours[3].energy > ours[i].energy, 1,
				    probe[i]);
	diff_eq_int("and it is not merely zero", ours[3].energy > 0, 1,
		    ours[3].energy);

	rc |= diff_end();

	diff_begin("v34 dft: coverage");
	diff_eq_int("the normalisation search found a bit",
		    saw_shift_found > 0, 1, 0);
	diff_eq_int("and fell through to the default", saw_shift_default > 0,
		    1, 0);
	diff_eq_int("an accumulator went negative", saw_negative_acc > 0, 1,
		    0);
	rc |= diff_end();

	return rc;
}
