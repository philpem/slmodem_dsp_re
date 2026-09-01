/*
 * t_v21fax.c -- differential test of V.21's fax-channel primitives.
 *
 * Three of these are pure readers of an unmodelled handle and one fills a
 * caller's block.  A reader has almost no arithmetic of its own, so "it
 * agrees with the blob over a thousand random inputs" proves very little on
 * its own: what can be wrong is WHICH FIELD, and a wrong offset agrees with
 * the right one over every input unless the fixture makes the two differ.
 *
 * So every check is built around a NAMED WRONG READING, and the number of
 * trials that SEPARATE it is counted FROM THE RUN and asserted non-zero at
 * the end (findings F134 and F3052).  A zero there means the check above it
 * is decoration.  The wrong readings, in order:
 *
 *   CarrierDetectV21
 *       - the pair taken from +0x00/+0x04 or +0x08/+0x0c rather than
 *         +0x04/+0x08.  Both evaluated for real against a pseudorandom
 *         block, so a shifted read gets a different number and not a zero.
 *       - OR rather than AND.
 *
 *   GetSNRV21
 *       - the trace taken from `fsd.fir_hist` (+0x24 of the fsd) rather than
 *         `fsd.trace` (+0x1c), which is the neighbour a one-field slip
 *         reaches.
 *       - the count taken from `fsd.f22` rather than `fsd.last_count`.
 *       - the rectified value written over the trace rather than into `mag`.
 *       - the absolute value not taken.
 *       - the count read UNSIGNED.  Separated by construction: the blob is
 *         RUN with last_count negative and must write NOTHING, where an
 *         unsigned reading walks 65,535 entries.  The buffer is marked either
 *         side of the count so "wrote nothing" is a check and not a hope.
 *       - -32768 IS IN THE INPUT.  The object's abs is the branchless
 *         `cltd; xor; sub`, so it answers -32768 there, and a spelling that
 *         saturated or widened would not.  That trial is counted.
 *
 *   V21TX_status
 *       - the protocol word from +0x02 of the handle rather than +0x00.
 *       - the flag byte from +0x11 rather than +0x10.
 *       - the mask 0x02 rather than 0x04.
 *       - the last store spelled `|=` rather than `=`.  This is the one that
 *         pins deviation D1037: the object ASSIGNS, so the two bits cleared
 *         earlier and every other bit the caller had are lost, and the
 *         fixture hands in a block whose flags byte is 0xff so the two
 *         readings cannot agree.
 *       - a NULL destination must write nothing and return 0.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"
#include "dsplib/v21fax.h"

extern int ref_CarrierDetectV21(void *modem);
extern int ref_GetSNRV21(void *modem);
extern int ref_V21TX_status(void *modem, struct v21_status *st);
extern int ref_V21RX_status(void *modem, struct v21_status *st);

/* --------------------------------------------------------------------- */

#define RXOBJ_SIZE	0x60
#define TXOBJ_SIZE	0x40
#define TRACE_LEN	512
#define MAG_LEN		512
#define MARK		((short)0x5ead)

static unsigned rng_state;

static void
rng_seed(unsigned s)
{
	rng_state = s ? s : 1u;
}

static unsigned
rng_next(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

/* --------------------------------------------------------------------- */
/* The receive fixture                                                   */

struct rxfix {
	unsigned char		obj[RXOBJ_SIZE];
	struct v21_rx_dsp	dsp;
	short			trace[TRACE_LEN];
	short			mag[MAG_LEN];
	double			align;
};

static struct rxfix ra, rb;

/*
 * Lay one receiver down.  The whole handle and the whole DSP block are
 * pseudorandom before the named fields go in, so an offset wrong by a few
 * bytes reads garbage rather than a plausible zero -- and every field the
 * function under test uses as a SUBSCRIPT is planted, not only the ones it
 * dereferences.  That is D955 / finding F8587: a blob-against-blob dry run
 * cannot catch an unplanted subscript, because both sides read the same wild
 * index into the same array and agree.
 */
static void
rx_fixture(struct rxfix *f, unsigned seed, short count)
{
	int i;

	memset(f, 0, sizeof(*f));

	rng_seed(seed);
	for (i = 0; i < RXOBJ_SIZE; i++)
		f->obj[i] = (unsigned char)rng_next();
	for (i = 0; i < (int)sizeof(f->dsp); i++)
		((unsigned char *)(void *)&f->dsp)[i] = (unsigned char)rng_next();

	*(void **)(void *)(f->obj + V21RX_OBJ_DSP) = (void *)&f->dsp;

	/*
	 * Everything the fsd and the mrf hold that is a POINTER is set the
	 * same on both sides -- NULL, or into this fixture -- so that the two
	 * blocks can be compared byte for byte afterwards with only `trace`
	 * and `mag` skipped.
	 */
	memset(&f->dsp.fsd.cfg, 0, sizeof(f->dsp.fsd.cfg));
	memset(&f->dsp.mrf.cfg, 0, sizeof(f->dsp.mrf.cfg));
	f->dsp.fsd.fir_hist = 0;
	f->dsp.fsd.iir_hist = 0;
	f->dsp.mrf.history = 0;
	f->dsp.mtd = 0;

	f->dsp.fsd.trace = f->trace;
	f->dsp.mag = f->mag;
	f->dsp.fsd.last_count = count;

	/*
	 * `f22` is `last_count`'s neighbour and is what a one-field slip
	 * reads; give it a DIFFERENT value so the slip is separable.
	 */
	f->dsp.fsd.f22 = (short)(count + 3);

	for (i = 0; i < TRACE_LEN; i++)
		f->trace[i] = (short)(rng_next() & 0xffffu);
	/*
	 * The corner the object's branchless abs turns into itself.  Placed
	 * where a short count reaches it.
	 */
	f->trace[0] = (short)-32768;
	f->trace[1] = 0;
	f->trace[2] = -1;
	f->trace[3] = 32767;

	for (i = 0; i < MAG_LEN; i++)
		f->mag[i] = MARK;
}

/*
 * The DSP block compared byte for byte, with the two per-fixture array
 * pointers skipped.  Returns the first differing offset, or -1.
 */
static long
dsp_first_diff(const struct rxfix *a, const struct rxfix *b)
{
	int i;
	int trace_lo = (int)((const char *)&a->dsp.fsd.trace
			     - (const char *)&a->dsp);
	int mag_lo = (int)((const char *)&a->dsp.mag - (const char *)&a->dsp);

	for (i = 0; i < (int)sizeof(a->dsp); i++) {
		if (i >= trace_lo && i < trace_lo + (int)sizeof(void *))
			continue;
		if (i >= mag_lo && i < mag_lo + (int)sizeof(void *))
			continue;
		if (((const unsigned char *)(const void *)&a->dsp)[i]
		    != ((const unsigned char *)(const void *)&b->dsp)[i])
			return i;
	}
	return -1;
}

static long
rxobj_first_diff(const struct rxfix *a, const struct rxfix *b)
{
	int i;

	for (i = 0; i < RXOBJ_SIZE; i++) {
		if (i >= V21RX_OBJ_DSP
		    && i < V21RX_OBJ_DSP + (int)sizeof(void *))
			continue;		/* the fixture's own pointer */
		if (a->obj[i] != b->obj[i])
			return i;
	}
	return -1;
}

/* --------------------------------------------------------------------- */
/* CarrierDetectV21                                                      */

static long cd_low_sep, cd_high_sep, cd_or_sep, cd_true, cd_false;

static const int cd_values[] = {
	0, 1, 2, 3, -1, 4, 0x5a5a5a5a, (int)0xa5a5a5a5u, 0x0f0f0f0f,
	(int)0xf0f0f0f0u
};

#define CD_NVALUES ((int)(sizeof(cd_values) / sizeof(cd_values[0])))

static int
run_carrier(void)
{
	int i, j;

	diff_begin("CarrierDetectV21");
	for (i = 0; i < CD_NVALUES; i++) {
		for (j = 0; j < CD_NVALUES; j++) {
			long where = (long)i * 100 + j;
			int got_a, got_b;

			rx_fixture(&ra, 0x1234abcdu + (unsigned)(i * 31 + j),
				   0);
			rx_fixture(&rb, 0x1234abcdu + (unsigned)(i * 31 + j),
				   0);
			ra.dsp.int_0004 = rb.dsp.int_0004 = cd_values[i];
			ra.dsp.int_0008 = rb.dsp.int_0008 = cd_values[j];

			got_a = ref_CarrierDetectV21(ra.obj);
			got_b = CarrierDetectV21(rb.obj);

			diff_eq_int("at %ld: carrier", (long)got_b, (long)got_a,
				    where);
			diff_eq_int("at %ld: first differing DSP byte",
				    dsp_first_diff(&ra, &rb), -1, where);
			diff_eq_int("at %ld: first differing handle byte",
				    rxobj_first_diff(&ra, &rb), -1, where);

			if (got_a != 0)
				cd_true++;
			else
				cd_false++;

			/* WRONG READING: the pair one field low. */
			if ((ra.dsp.int_0000 & ra.dsp.int_0004) != got_a)
				cd_low_sep++;
			/*
			 * WRONG READING: the pair one field high.  The int at
			 * +0x0c is now the head of `struct fpm_agc`, which
			 * `DemodDataV21` typed; the probe still reads the same
			 * four bytes at the same offset.
			 */
			if ((ra.dsp.int_0008
			     & *(const int *)(const void *)
				     &ra.dsp.agc) != got_a)
				cd_high_sep++;
			/* WRONG READING: OR rather than AND. */
			if ((cd_values[i] | cd_values[j]) != got_a)
				cd_or_sep++;
		}
	}
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* GetSNRV21                                                             */

static long snr_trace_sep, snr_count_sep, snr_dest_sep, snr_abs_sep;
static long snr_wrote, snr_neg_count, snr_min_short, snr_zero_count;

static void
run_snr_one(short count, unsigned seed)
{
	long where = (long)count;
	int got_a, got_b;
	int i;
	int lim;

	rx_fixture(&ra, seed, count);
	rx_fixture(&rb, seed, count);

	got_a = ref_GetSNRV21(ra.obj);
	got_b = GetSNRV21(rb.obj);

	diff_eq_int("at %ld: return", (long)got_b, (long)got_a, where);
	diff_eq_int("at %ld: first differing DSP byte",
		    dsp_first_diff(&ra, &rb), -1, where);
	diff_eq_int("at %ld: first differing handle byte",
		    rxobj_first_diff(&ra, &rb), -1, where);

	for (i = 0; i < MAG_LEN; i++)
		diff_eq_int("mag[%ld]", rb.mag[i], ra.mag[i], i);
	for (i = 0; i < TRACE_LEN; i++)
		diff_eq_int("trace[%ld]", rb.trace[i], ra.trace[i], i);

	/*
	 * COVERAGE ASSERTED FROM THE RUN, not from the trial row: what is
	 * counted is what the BLOB actually left behind.
	 */
	lim = count > 0 ? (int)count : 0;
	if (lim > 0 && ra.mag[0] != MARK)
		snr_wrote++;
	if (count < 0) {
		snr_neg_count++;
		/* An unsigned reading would have walked 65,535 entries. */
		diff_eq_int("at %ld: a negative count wrote nothing",
			    ra.mag[0] == MARK, 1, where);
	}
	if (count == 0)
		snr_zero_count++;
	if (lim > 0 && ra.mag[0] == (short)-32768)
		snr_min_short++;

	/* Nothing past the count, on either side. */
	if (lim >= 0 && lim < MAG_LEN)
		diff_eq_int("at %ld: nothing past the count",
			    ra.mag[lim] == MARK, 1, where);

	if (lim == 0)
		return;

	/* WRONG READING: the trace taken from fir_hist. */
	if (ra.dsp.fsd.fir_hist != ra.dsp.fsd.trace)
		snr_trace_sep++;

	/* WRONG READING: the count taken from f22. */
	{
		int alt = ra.dsp.fsd.f22;
		int k;
		int differs = 0;

		if (alt != count)
			for (k = lim; k < alt && k < MAG_LEN; k++)
				if (ra.mag[k] != MARK)
					differs = 1;
		if (alt != count && (alt < lim || differs == 0))
			snr_count_sep++;
	}

	/* WRONG READING: the trace written over rather than `mag`. */
	for (i = 0; i < lim && i < TRACE_LEN; i++) {
		short v = ra.trace[i];

		if (v < 0 && v != (short)-32768) {
			snr_dest_sep++;
			break;
		}
	}

	/* WRONG READING: the absolute value not taken. */
	for (i = 0; i < lim && i < TRACE_LEN; i++)
		if (ra.mag[i] != ra.trace[i]) {
			snr_abs_sep++;
			break;
		}
}

static int
run_snr(void)
{
	static const short counts[] = { -32768, -7, -1, 0, 1, 2, 3, 4, 7, 64,
					255, 300 };
	int i;

	diff_begin("GetSNRV21");
	for (i = 0; i < (int)(sizeof(counts) / sizeof(counts[0])); i++)
		run_snr_one(counts[i], 0x0f1e2d3cu + (unsigned)i * 7919u);
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V21RX_status                                                          */

/*
 * The receive half of the report, which is not the transmit one with the
 * handle changed.  What has to be separated is every place the two differ:
 * which slot takes the rate, that `snr` carries GetSNRV21's answer, that
 * `quality` is the COMPLEMENT of the LOW_SNR bit, that +0x0e is zeroed and
 * +0x0c is not, and the computed +0x12.
 *
 * `bit_samples` IS PLANTED NON-ZERO ON EVERY TRIAL.  `rx_fixture` zeroes the
 * fsd's configuration, and +0x12 is an `idiv` by that field with no guard, so
 * an unplanted fixture would take SIGFPE on both sides -- identically, and
 * therefore invisibly to a differential comparison, which is why the
 * precondition is asserted here rather than trusted.  See D1098.
 */
static long rxst_q0, rxst_q1, rxst_snr_sep, rxst_bps_sep, rxst_gap_sep;
static long rxst_flags_sep, rxst_div_sep, rxst_null_trials, rxst_ret1;

static struct rxfix sa, sb;
static struct v21_status rsta, rstb;

static void
run_rxstatus_one(unsigned seed, short count, unsigned char flags,
		 short bit_samples, short f22, unsigned char stflags)
{
	long where = (long)bit_samples * 1000 + (long)f22 * 10 + flags;
	int got_a, got_b;
	int i;

	rx_fixture(&sa, seed, count);
	rx_fixture(&sb, seed, count);
	sa.obj[V21RX_OBJ_FLAGS] = sb.obj[V21RX_OBJ_FLAGS] = flags;
	sa.dsp.fsd.cfg.bit_samples = sb.dsp.fsd.cfg.bit_samples = bit_samples;
	sa.dsp.fsd.f22 = sb.dsp.fsd.f22 = f22;
	*(unsigned short *)(void *)(sa.obj + V21RX_OBJ_PROTOCOL) =
	*(unsigned short *)(void *)(sb.obj + V21RX_OBJ_PROTOCOL) =
		(unsigned short)(0x4200u + (unsigned)flags);

	/* THE PRECONDITION, asserted on both sides rather than assumed. */
	diff_eq_int("at %ld: the divisor is non-zero",
		    sa.dsp.fsd.cfg.bit_samples != 0, 1, where);

	memset(&rsta, 0, sizeof(rsta));
	memset(&rstb, 0, sizeof(rstb));
	rng_seed(seed ^ 0x5eed5eedu);
	for (i = 0; i < (int)sizeof(rsta); i++)
		((unsigned char *)(void *)&rsta)[i] =
		((unsigned char *)(void *)&rstb)[i] =
			(unsigned char)rng_next();
	rsta.flags = rstb.flags = stflags;
	rsta.flags1 = rstb.flags1 = (unsigned char)(stflags ^ 0xffu);

	got_a = ref_V21RX_status(sa.obj, &rsta);
	got_b = V21RX_status(sb.obj, &rstb);

	diff_eq_int("at %ld: return", (long)got_b, (long)got_a, where);
	for (i = 0; i < (int)sizeof(rsta); i++)
		diff_eq_int("rx status byte %ld",
			    ((const unsigned char *)(const void *)&rstb)[i],
			    ((const unsigned char *)(const void *)&rsta)[i],
			    i);
	diff_eq_int("at %ld: first differing DSP byte",
		    dsp_first_diff(&sa, &sb), -1, where);
	diff_eq_int("at %ld: first differing handle byte",
		    rxobj_first_diff(&sa, &sb), -1, where);
	for (i = 0; i < MAG_LEN; i++)
		diff_eq_int("rx status mag[%ld]", sb.mag[i], sa.mag[i], i);

	if (got_a == 1)
		rxst_ret1++;

	/* ARM COVERAGE, from what the REFERENCE left behind. */
	if (rsta.quality != 0)
		rxst_q1++;
	else
		rxst_q0++;

	/*
	 * WRONG READING: the rate written to `tx_bps` as the transmit side
	 * does.  Separated on every trial, since the object writes 0 there
	 * and 300 in `rx_bps`.
	 */
	if (rsta.tx_bps == 0 && rsta.rx_bps == V21_STATUS_BPS)
		rxst_bps_sep++;
	/*
	 * WRONG READING: `snr` written 0, as `V21TX_status` does.  D1038 has
	 * `GetSNRV21` returning a literal zero, so this can only separate on
	 * the RETURN being copied at all -- which it cannot, and that is
	 * recorded rather than papered over: the counter below is the number
	 * of trials on which the two agree that the call was made, taken from
	 * the fact that `mag` was rewritten by it.
	 */
	if (sa.mag[0] != MARK)
		rxst_snr_sep++;
	/* WRONG READING: +0x0c zeroed rather than +0x0e. */
	if (rsta.short_0c != 0 && rsta.short_0e == 0)
		rxst_gap_sep++;
	/*
	 * WRONG READING: the flags byte MERGED rather than stored as a
	 * literal zero.  Separated whenever the caller had any bit set.
	 */
	if (stflags != 0 && rsta.flags == 0)
		rxst_flags_sep++;
	/*
	 * WRONG READING: the quotient not doubled, or the constant wrong.
	 * Separated whenever the computed field is not `rx_bps`, which the
	 * odd `bit_samples` trials arrange.
	 */
	if (rsta.short_12 != V21_STATUS_BPS)
		rxst_div_sep++;
}

static int
run_rxstatus(void)
{
	static const short bs[] = { 8, 6, 16, 3, 5, 1, -8, 32767 };
	static const short fx[] = { 4, 3, 8, 0, -1, 2 };
	static const unsigned char fl[] = { 0x00, 0x80, 0xff, 0x7f };
	static const unsigned char sf[] = { 0x00, 0xff, 0x5a };
	int i, j, k;

	diff_begin("V21RX_status");

	for (i = 0; i < (int)(sizeof(bs) / sizeof(bs[0])); i++)
		for (j = 0; j < (int)(sizeof(fx) / sizeof(fx[0])); j++)
			for (k = 0; k < (int)(sizeof(fl) / sizeof(fl[0])); k++)
				run_rxstatus_one(0x31000000u
						 + (unsigned)(i * 97 + j * 7
							      + k),
						 (short)(8 + i),
						 fl[k], bs[i], fx[j],
						 sf[(i + j + k) % 3]);

	/* The NULL destination: nothing written, zero returned. */
	{
		int got_a, got_b;
		int i2;

		rx_fixture(&sa, 0x4d4d4d4du, 12);
		rx_fixture(&sb, 0x4d4d4d4du, 12);
		sa.dsp.fsd.cfg.bit_samples = sb.dsp.fsd.cfg.bit_samples = 8;
		got_a = ref_V21RX_status(sa.obj, 0);
		got_b = V21RX_status(sb.obj, 0);
		diff_eq_int("rx NULL destination: return", (long)got_b,
			    (long)got_a, 0);
		diff_eq_int("rx NULL destination: the blob returned 0",
			    got_a == 0, 1, 0);
		/*
		 * `rxobj_first_diff` and not a raw loop: the handle carries
		 * the fixture's OWN DSP pointer at +0x50, which is a
		 * per-fixture address and differs on every trial.  Written as
		 * a raw loop first, and byte 0x51 is what said so.
		 */
		i2 = 0;
		diff_eq_int("rx NULL: first differing handle byte",
			    rxobj_first_diff(&sa, &sb), -1, i2);
		diff_eq_int("rx NULL: first differing DSP byte",
			    dsp_first_diff(&sa, &sb), -1, 0);
		rxst_null_trials++;
	}

	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V21TX_status                                                          */

struct txfix {
	unsigned char		obj[TXOBJ_SIZE];
	struct v21_status	st;
	double			align;
};

static struct txfix ta, tb;

static void
tx_fixture(struct txfix *f, unsigned seed, unsigned char stflags)
{
	int i;

	memset(f, 0, sizeof(*f));
	rng_seed(seed);
	for (i = 0; i < TXOBJ_SIZE; i++)
		f->obj[i] = (unsigned char)rng_next();
	for (i = 0; i < (int)sizeof(f->st); i++)
		((unsigned char *)(void *)&f->st)[i] = (unsigned char)rng_next();
	f->st.flags = stflags;
	f->st.flags1 = (unsigned char)(stflags ^ 0xffu);
}

static long st_proto_sep, st_flagoff_sep, st_mask_sep, st_assign_sep;
static long st_null_trials, st_ret1, st_bits_lost;

static void
run_status_one(unsigned seed, unsigned char stflags, unsigned char txflags)
{
	long where = (long)stflags * 256 + txflags;
	int got_a, got_b;
	int i;

	tx_fixture(&ta, seed, stflags);
	tx_fixture(&tb, seed, stflags);
	ta.obj[V21TX_OBJ_FLAGS] = tb.obj[V21TX_OBJ_FLAGS] = txflags;

	got_a = ref_V21TX_status(ta.obj, &ta.st);
	got_b = V21TX_status(tb.obj, &tb.st);

	diff_eq_int("at %ld: return", (long)got_b, (long)got_a, where);
	for (i = 0; i < (int)sizeof(ta.st); i++)
		diff_eq_int("status byte %ld",
			    ((const unsigned char *)(const void *)&tb.st)[i],
			    ((const unsigned char *)(const void *)&ta.st)[i],
			    i);
	for (i = 0; i < TXOBJ_SIZE; i++)
		diff_eq_int("handle byte %ld", tb.obj[i], ta.obj[i], i);

	if (got_a == 1)
		st_ret1++;

	/* WRONG READING: the protocol word from +0x02. */
	if (*(const unsigned short *)(const void *)(ta.obj + 2)
	    != (unsigned short)ta.st.protocol)
		st_proto_sep++;

	/* WRONG READING: the flag byte from +0x11. */
	if ((ta.obj[V21TX_OBJ_FLAGS + 1] & V21_STATUS_BIT2)
	    != (ta.st.flags & 0xffu))
		st_flagoff_sep++;

	/* WRONG READING: the mask 0x02 rather than 0x04. */
	if ((txflags & V21_STATUS_BIT1) != ta.st.flags)
		st_mask_sep++;

	/*
	 * WRONG READING: `|=` rather than `=`.  With the caller's flags byte
	 * carrying bits outside the mask, the merged reading keeps them and
	 * the object's does not -- which is D1037 as a measurement.
	 */
	{
		unsigned merged = (stflags
				   & (unsigned)~(V21_STATUS_BIT0
						 | V21_STATUS_BIT1))
				  | (txflags & V21_STATUS_BIT2);

		if ((merged & 0xffu) != ta.st.flags) {
			st_assign_sep++;
			if ((stflags & ~(unsigned)(V21_STATUS_BIT0
						   | V21_STATUS_BIT1
						   | V21_STATUS_BIT2)) != 0)
				st_bits_lost++;
		}
	}
}

static int
run_status(void)
{
	static const unsigned char stf[] = { 0x00, 0xff, 0x55, 0xaa, 0x03,
					     0xfc };
	static const unsigned char txf[] = { 0x00, 0x04, 0xfb, 0xff, 0x06,
					     0x02 };
	int i, j;
	int rc;

	diff_begin("V21TX_status");
	for (i = 0; i < (int)(sizeof(stf) / sizeof(stf[0])); i++)
		for (j = 0; j < (int)(sizeof(txf) / sizeof(txf[0])); j++)
			run_status_one(0x77aabb00u + (unsigned)(i * 13 + j),
				       stf[i], txf[j]);

	/* The NULL destination: nothing written, zero returned. */
	{
		int got_a, got_b;

		tx_fixture(&ta, 0x2b2b2b2bu, 0x3c);
		tx_fixture(&tb, 0x2b2b2b2bu, 0x3c);
		got_a = ref_V21TX_status(ta.obj, 0);
		got_b = V21TX_status(tb.obj, 0);
		diff_eq_int("NULL destination: return", (long)got_b,
			    (long)got_a, 0);
		diff_eq_int("NULL destination: the blob returned 0",
			    got_a == 0, 1, 0);
		for (i = 0; i < TXOBJ_SIZE; i++)
			diff_eq_int("NULL: handle byte %ld", tb.obj[i],
				    ta.obj[i], i);
		st_null_trials++;
	}

	rc = diff_end();
	return rc;
}

/* --------------------------------------------------------------------- */
/* ModDataV21 and TxNoCarrierV21                                         */

/*
 * THE FIXTURE IS DRIVEN OVER MANY BLOCKS, not one.  Both the modulator's
 * tone phase and the resampler's history and phase persist across calls, so
 * a one-block fixture cannot see a state update that is wrong only in how it
 * CARRIES -- finding F8790, where a swapped smoothing weight was invisible to
 * every codegen check and to any single-block test.
 */

#define SCRATCH_LEN	4096
#define TXOUT_LEN	4096
#define MRF_TAPS	120
#define MRF_BRANCHES	10
#define TX_BLOCKS	9
#define TX_NBITS	7

struct txdfix {
	unsigned char		obj[TXOBJ_SIZE];
	struct v21_tx_dsp	dsp;
	short			scratch[SCRATCH_LEN];
	double			align;
};

static struct txdfix da, db, dc;
static short mrf_coeff[MRF_TAPS];
static short txout_a[TXOUT_LEN], txout_b[TXOUT_LEN], txout_c[TXOUT_LEN];
static unsigned short txbits[TX_BLOCKS * TX_NBITS];

extern void ref_FPM_FSM_init(struct fpm_fsm *state,
			     const struct fpm_fsm_cfg *cfg);
extern void ref_FPM_MRF_init(struct fpm_mrf *state,
			     const struct fpm_mrf_cfg *cfg, int fresh);
extern unsigned short ref_ModDataV21(void *modem, const unsigned short *bits,
				     short *out, unsigned short nbits);
extern unsigned short ref_TxNoCarrierV21(void *modem,
					 const unsigned short *bits,
					 short *out, unsigned short nbits);

/*
 * Built here rather than taken from the object's own banks, and deliberately:
 * finding F3574 is three mutations that survived because a real bank repeats
 * two entries and zeroes a third, so a transposition was invisible.  Every
 * entry below is distinct.
 */
static void
build_tx_tables(void)
{
	int i;

	for (i = 0; i < MRF_TAPS; i++)
		mrf_coeff[i] = (short)(((i * 811) % 6007) - 3000);

	rng_seed(0x51ee2200u);
	for (i = 0; i < TX_BLOCKS * TX_NBITS; i++)
		txbits[i] = (unsigned short)rng_next();
}

static void
tx_dsp_fixture(struct txdfix *f, unsigned seed, short scale)
{
	struct fpm_fsm_cfg fsm;
	struct fpm_mrf_cfg mrf;
	int i;

	memset(f, 0, sizeof(*f));

	rng_seed(seed);
	for (i = 0; i < TXOBJ_SIZE; i++)
		f->obj[i] = (unsigned char)rng_next();
	for (i = 0; i < SCRATCH_LEN; i++)
		f->scratch[i] = (short)rng_next();

	*(void **)(void *)(f->obj + V21TX_OBJ_DSP) = (void *)&f->dsp;
	f->dsp.scratch = f->scratch;

	memset(&fsm, 0, sizeof(fsm));
	fsm.freq[0] = 1180;		/* V.21 channel 1, space */
	fsm.freq[1] = 980;		/*                 mark  */
	fsm.samples_per_sym = 24;
	fsm.scale = scale;
	ref_FPM_FSM_init(&f->dsp.fsm, &fsm);

	memset(&mrf, 0, sizeof(mrf));
	mrf.branches = MRF_BRANCHES;
	mrf.decimate = 9;
	mrf.coeff = mrf_coeff;
	mrf.taps = MRF_TAPS;
	ref_FPM_MRF_init(&f->dsp.mrf, &mrf, 1);
}

/*
 * The DSP block byte for byte, with the three per-fixture pointers skipped.
 * What they point AT is compared separately.
 */
static long
txdsp_first_diff(const struct txdfix *a, const struct txdfix *b)
{
	int i;
	int tone_lo = (int)((const char *)&a->dsp.fsm.tone
			    - (const char *)&a->dsp);
	int hist_lo = (int)((const char *)&a->dsp.mrf.history
			    - (const char *)&a->dsp);
	int scr_lo = (int)((const char *)&a->dsp.scratch
			   - (const char *)&a->dsp);

	for (i = 0; i < (int)sizeof(a->dsp); i++) {
		if (i >= tone_lo && i < tone_lo + (int)sizeof(void *))
			continue;
		if (i >= hist_lo && i < hist_lo + (int)sizeof(void *))
			continue;
		if (i >= scr_lo && i < scr_lo + (int)sizeof(void *))
			continue;
		if (((const unsigned char *)(const void *)&a->dsp)[i]
		    != ((const unsigned char *)(const void *)&b->dsp)[i])
			return i;
	}
	return -1;
}

/*
 * The five slots of `struct fpm_tone` that hold a pointer INTO THE OBJECT'S
 * OWN allocation, and so differ between two instances of the same modulator
 * however identical their state.  They are named from `fpm_tone.h`, not
 * found by watching the comparison fail: `kernel` +0x2c, `history` +0x30,
 * `rev_block` +0xf4, `rev_acc` +0xf8 and `iir_self` +0xfc.  `cfg.src` at
 * +0x10 is the SHARED prototype and is compared like any other word.
 */
static int
tone_is_pointer(int off)
{
	static const int slots[] = { 0x2c, 0x30, 0xf4, 0xf8, 0xfc };
	int i;

	for (i = 0; i < (int)(sizeof(slots) / sizeof(slots[0])); i++)
		if (off >= slots[i] && off < slots[i] + (int)sizeof(void *))
			return 1;
	return 0;
}

static long tx_mod_ret, tx_mod_state, tx_mod_wrap;
static long tx_mute_sep, tx_restore_sep, tx_bits_sep, tx_share_sep;
static long tx_count_sep;

/*
 * Drive `TX_BLOCKS` consecutive blocks through both sides and compare after
 * every one.  `mute` selects TxNoCarrierV21 over ModDataV21.
 */
static void
run_tx_stream(int mute, short scale, unsigned seed)
{
	int blk;

	tx_dsp_fixture(&da, seed, scale);
	tx_dsp_fixture(&db, seed, scale);

	for (blk = 0; blk < TX_BLOCKS; blk++) {
		const unsigned short *bits = txbits + blk * TX_NBITS;
		unsigned short nbits = (unsigned short)(blk % (TX_NBITS + 1));
		unsigned short ka, kb;
		long where = (long)mute * 1000 + blk;
		int i;

		for (i = 0; i < TXOUT_LEN; i++)
			txout_a[i] = txout_b[i] = MARK;

		if (mute) {
			ka = ref_TxNoCarrierV21(da.obj, bits, txout_a, nbits);
			kb = TxNoCarrierV21(db.obj, bits, txout_b, nbits);
		} else {
			ka = ref_ModDataV21(da.obj, bits, txout_a, nbits);
			kb = ModDataV21(db.obj, bits, txout_b, nbits);
		}

		diff_eq_int("at %ld: samples returned", (long)kb, (long)ka,
			    where);
		diff_eq_int("at %ld: the return fits the buffer",
			    ka < TXOUT_LEN, 1, where);
		if (ka >= TXOUT_LEN)
			return;
		for (i = 0; i < (int)ka; i++)
			diff_eq_int("out[%ld]", txout_b[i], txout_a[i], i);
		diff_eq_int("at %ld: nothing past the returned count",
			    txout_a[ka] == MARK, 1, where);

		diff_eq_int("at %ld: first differing DSP byte",
			    txdsp_first_diff(&da, &db), -1, where);
		for (i = 0; i < TXOBJ_SIZE; i++) {
			if (i >= V21TX_OBJ_DSP
			    && i < V21TX_OBJ_DSP + (int)sizeof(void *))
				continue;	/* the fixture's own pointer */
			diff_eq_int("handle byte %ld", db.obj[i], da.obj[i], i);
		}
		for (i = 0; i < SCRATCH_LEN; i++)
			diff_eq_int("scratch[%ld]", db.scratch[i],
				    da.scratch[i], i);
		/* The modulator's tone object, which both sides own one of. */
		for (i = 0; i < (int)sizeof(struct fpm_tone); i++) {
			if (tone_is_pointer(i))
				continue;
			diff_eq_int("tone byte %ld",
				    ((const unsigned char *)(const void *)
				     db.dsp.fsm.tone)[i],
				    ((const unsigned char *)(const void *)
				     da.dsp.fsm.tone)[i], i);
		}
		/* And the resampler's history. */
		for (i = 0; i < da.dsp.mrf.history_len; i++)
			diff_eq_int("mrf history[%ld]", db.dsp.mrf.history[i],
				    da.dsp.mrf.history[i], i);

		if (ka != 0)
			tx_mod_ret++;
		if (da.dsp.mrf.phase != 0 || da.dsp.mrf.widx != 0)
			tx_mod_state++;
		if (blk > 0 && da.dsp.mrf.widx < db.dsp.mrf.history_len
		    && da.dsp.mrf.widx == 0)
			tx_mod_wrap++;

		/* The scale is back where it started, muted or not. */
		diff_eq_int("at %ld: the output scale survived",
			    (long)da.dsp.fsm.cfg.scale, (long)scale, where);
		if (mute && scale != 0 && ka != 0)
			tx_restore_sep++;
	}
}

/*
 * The named wrong readings, each evaluated for real against the blob's own
 * answer over one block.
 */
static void
run_tx_alternatives(void)
{
	static short alt_scratch[SCRATCH_LEN];
	const unsigned short *bits = txbits;
	unsigned short ka, kc;
	int i;

	/*
	 * WRONG READING (TxNoCarrierV21): the scale not forced to zero.  Run
	 * the blob's ModDataV21 over an identical fixture; the samples must
	 * differ, or muting is not observable and the check above is
	 * decoration.
	 */
	tx_dsp_fixture(&da, 0x1a2b3c4du, 0x4000);
	tx_dsp_fixture(&dc, 0x1a2b3c4du, 0x4000);
	for (i = 0; i < TXOUT_LEN; i++)
		txout_a[i] = txout_c[i] = MARK;
	ka = ref_TxNoCarrierV21(da.obj, bits, txout_a, TX_NBITS);
	kc = ref_ModDataV21(dc.obj, bits, txout_c, TX_NBITS);
	if (ka == kc && ka != 0
	    && memcmp(txout_a, txout_c, (size_t)ka * sizeof(txout_a[0])) != 0)
		tx_mute_sep++;

	/*
	 * WRONG READING (both): the resampler fed the BIT count rather than
	 * the modulator's sample count.  Driven with the blob's own pieces so
	 * that only the one reading differs.
	 */
	tx_dsp_fixture(&dc, 0x5f5f0101u, 0x4000);
	for (i = 0; i < TXOUT_LEN; i++)
		txout_c[i] = MARK;
	{
		short n = FPM_MRF_filter(&dc.dsp.mrf, dc.dsp.scratch, txout_c,
					 (short)TX_NBITS);

		tx_dsp_fixture(&da, 0x5f5f0101u, 0x4000);
		for (i = 0; i < TXOUT_LEN; i++)
			txout_a[i] = MARK;
		ka = ref_ModDataV21(da.obj, bits, txout_a, TX_NBITS);
		if ((unsigned short)n != ka)
			tx_count_sep++;
	}

	/*
	 * WRONG READING (both): the resampler not fed the buffer the
	 * modulator wrote.  Same two calls, a different intermediate.
	 */
	tx_dsp_fixture(&dc, 0x2c2c9999u, 0x4000);
	tx_dsp_fixture(&da, 0x2c2c9999u, 0x4000);
	for (i = 0; i < SCRATCH_LEN; i++)
		alt_scratch[i] = (short)((i * 37) & 0x7ff);
	for (i = 0; i < TXOUT_LEN; i++)
		txout_a[i] = txout_c[i] = MARK;
	ka = (unsigned short)ref_ModDataV21(da.obj, bits, txout_a, TX_NBITS);
	{
		short n = FPM_FSM_modulate(&dc.dsp.fsm, bits, dc.dsp.scratch,
					   TX_NBITS);
		short m = FPM_MRF_filter(&dc.dsp.mrf, alt_scratch, txout_c, n);

		if ((unsigned short)m != ka
		    || memcmp(txout_a, txout_c,
			      (size_t)ka * sizeof(txout_a[0])) != 0)
			tx_share_sep++;
	}

	/*
	 * WRONG READING (TxNoCarrierV21): the bits ignored.  At scale zero
	 * every sample is silence, so the only thing that can tell two bit
	 * patterns apart is the modulator's own phase -- which is exactly the
	 * state a one-block fixture would not compare.
	 */
	tx_dsp_fixture(&da, 0x66aa1177u, 0x4000);
	tx_dsp_fixture(&dc, 0x66aa1177u, 0x4000);
	{
		static unsigned short other[TX_NBITS];

		for (i = 0; i < TX_NBITS; i++)
			other[i] = (unsigned short)(bits[i] ^ 1u);
		(void)ref_TxNoCarrierV21(da.obj, bits, txout_a, TX_NBITS);
		(void)ref_TxNoCarrierV21(dc.obj, other, txout_c, TX_NBITS);
		for (i = 0; i < (int)sizeof(struct fpm_tone); i++) {
			if (tone_is_pointer(i))
				continue;
			if (((const unsigned char *)(const void *)
			     da.dsp.fsm.tone)[i]
			    != ((const unsigned char *)(const void *)
				dc.dsp.fsm.tone)[i]) {
				tx_bits_sep++;
				break;
			}
		}
	}
}

static int
run_txdata(void)
{
	diff_begin("ModDataV21 / TxNoCarrierV21");
	run_tx_stream(0, 0x4000, 0x13572468u);
	run_tx_stream(1, 0x4000, 0x13572468u);
	run_tx_stream(0, 32767, 0x2468ace0u);
	run_tx_stream(1, 32767, 0x2468ace0u);
	run_tx_stream(1, 0, 0x0badf00du);
	run_tx_alternatives();
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V21RX_modem                                                           */

extern int ref_V21RX_modem(void *modem, short *in, short *out, short *count);

#define RXBUF_LEN	4096
#define RXBUF_LOW	1024		/* headroom below the cursor        */
#define STUB_MAX	32

struct stub_call {
	long	in_off;
	long	out_off;
	long	count_in;
	long	count_out;
	long	ret;
};

static struct stub_call stub_log[STUB_MAX];
static int stub_calls;
static short *stub_in_base, *stub_out_base;
/*
 * `leave` is what the handler STORES into `*count`, not what it consumes.
 * Storing it directly is what lets a script leave a value with the top bit
 * set, which is the only way to reach the object's mixed-signedness cursor
 * arithmetic -- `movswl` on the previous count against `movzwl` on the new
 * one.  A consume-based script cannot get there, because it can only ever
 * leave something no larger than what it was given.
 */
static const short *stub_script_leave;
static const short *stub_script_produce;
static int stub_script_len;

static short rx_in_a[RXBUF_LEN], rx_in_b[RXBUF_LEN];
static short rx_out_a[RXBUF_LEN], rx_out_b[RXBUF_LEN];

/*
 * The handler both sides dispatch through.
 *
 * It only touches memory through cursors it has checked against the window
 * it was given, because `V21RX_modem` advances the input cursor by a
 * difference the object computes with one operand SIGN-extended and the other
 * ZERO-extended -- so a deliberately hostile count sends the cursor a long
 * way outside the buffer, on the blob's side as much as ours.  That corner is
 * the point of the test; dereferencing there is not.
 */
static short
stub_handler(void *rx, short *in, short *out, short *count)
{
	int idx = stub_calls;
	short produce, avail;

	avail = *count;
	produce = stub_script_produce[idx % stub_script_len];

	if (idx < STUB_MAX) {
		stub_log[idx].in_off = (long)(in - stub_in_base);
		stub_log[idx].out_off = (long)(out - stub_out_base);
		stub_log[idx].count_in = avail;
	}

	*count = stub_script_leave[idx % stub_script_len];

	/* Stop runaway scripts: the last entry always drains. */
	if (idx + 1 >= stub_script_len)
		*count = 0;

	{
		long o = (long)(out - stub_out_base) + RXBUF_LOW;
		int k;

		for (k = 0; k < produce; k++)
			if (o + k >= 0 && o + k < RXBUF_LEN)
				out[k] = (short)(0x2000 + (idx << 8)
						 + (k & 0xff));
	}

	if (idx < STUB_MAX) {
		stub_log[idx].count_out = *count;
		stub_log[idx].ret = produce;
	}
	stub_calls++;
	(void)rx;
	return produce;
}

struct rxmfix {
	unsigned char		obj[RXOBJ_SIZE];
	struct v21_rx_hdx	hdx;
	double			align;
};

static struct rxmfix ma, mb;

static void
rxm_fixture(struct rxmfix *f, unsigned seed, unsigned char flags, int word)
{
	int i;

	memset(f, 0, sizeof(*f));
	rng_seed(seed);
	for (i = 0; i < RXOBJ_SIZE; i++)
		f->obj[i] = (unsigned char)rng_next();

	*(void **)(void *)(f->obj + V21RX_OBJ_HDX) = (void *)&f->hdx;
	f->hdx.handler = stub_handler;
	f->obj[V21RX_OBJ_FLAGS] = flags;
	memcpy(f->obj + V21RX_OBJ_STATUS, &word, sizeof word);
}

static long rxm_calls, rxm_flag_sep, rxm_wrap, rxm_signed_sep, rxm_zero_entry;
static long rxm_advance_sep;

static void
run_rxm_one(short count, unsigned char flags, int word,
	    const short *leave, const short *produce, int len, unsigned seed)
{
	long where = (long)count * 1000 + flags;
	short ca, cb;
	int got_a, got_b;
	struct stub_call log_a[STUB_MAX];
	int calls_a;
	int i;

	stub_script_leave = leave;
	stub_script_produce = produce;
	stub_script_len = len;

	for (i = 0; i < RXBUF_LEN; i++) {
		rx_in_a[i] = rx_in_b[i] = (short)(i * 7 + 1);
		rx_out_a[i] = rx_out_b[i] = MARK;
	}

	rxm_fixture(&ma, seed, flags, word);
	rxm_fixture(&mb, seed, flags, word);

	ca = count;
	stub_calls = 0;
	memset(stub_log, 0, sizeof(stub_log));
	stub_in_base = rx_in_a + RXBUF_LOW;
	stub_out_base = rx_out_a + RXBUF_LOW;
	got_a = ref_V21RX_modem(ma.obj, rx_in_a + RXBUF_LOW,
				rx_out_a + RXBUF_LOW, &ca);
	calls_a = stub_calls;
	memcpy(log_a, stub_log, sizeof(log_a));

	cb = count;
	stub_calls = 0;
	memset(stub_log, 0, sizeof(stub_log));
	stub_in_base = rx_in_b + RXBUF_LOW;
	stub_out_base = rx_out_b + RXBUF_LOW;
	got_b = V21RX_modem(mb.obj, rx_in_b + RXBUF_LOW,
			    rx_out_b + RXBUF_LOW, &cb);

	diff_eq_int("at %ld: return word", (long)got_b, (long)got_a, where);
	diff_eq_int("at %ld: count out", (long)cb, (long)ca, where);
	diff_eq_int("at %ld: handler calls", (long)stub_calls, (long)calls_a,
		    where);
	for (i = 0; i < calls_a && i < STUB_MAX; i++) {
		diff_eq_int("call %ld: input cursor", stub_log[i].in_off,
			    log_a[i].in_off, i);
		diff_eq_int("call %ld: output cursor", stub_log[i].out_off,
			    log_a[i].out_off, i);
		diff_eq_int("call %ld: count in", stub_log[i].count_in,
			    log_a[i].count_in, i);
		diff_eq_int("call %ld: count out", stub_log[i].count_out,
			    log_a[i].count_out, i);
	}
	for (i = 0; i < RXBUF_LEN; i++) {
		diff_eq_int("rx out[%ld]", rx_out_b[i], rx_out_a[i], i);
		diff_eq_int("rx in[%ld]", rx_in_b[i], rx_in_a[i], i);
	}
	for (i = 0; i < RXOBJ_SIZE; i++) {
		if (i >= V21RX_OBJ_HDX && i < V21RX_OBJ_HDX + (int)sizeof(void *))
			continue;
		diff_eq_int("handle byte %ld", mb.obj[i], ma.obj[i], i);
	}

	/* COVERAGE, FROM THE RUN. */
	if (calls_a > 0)
		rxm_calls++;
	if (count == 0 && calls_a > 0)
		rxm_zero_entry++;
	if ((flags & V21RX_FLAG_ERROR) != 0
	    && (ma.obj[V21RX_OBJ_FLAGS] & V21RX_FLAG_ERROR) == 0)
		rxm_flag_sep++;
	if (calls_a > 0 && ca != count)
		rxm_advance_sep++;

	/*
	 * The signed/unsigned asymmetry, evaluated: what the input cursor
	 * would have been had BOTH counts been read the same way.
	 */
	for (i = 1; i < calls_a && i < STUB_MAX; i++) {
		long before = log_a[i - 1].count_in;
		long after = log_a[i - 1].count_out;
		long both_signed = log_a[i - 1].in_off + (before - after);
		long objects = log_a[i].in_off;

		if (both_signed != objects)
			rxm_signed_sep++;
		if (log_a[i].out_off < log_a[i - 1].out_off)
			rxm_wrap++;
	}
}

static int
run_rxmodem(void)
{
	static const short c1[] = { 8, 4, 1, 0 };
	static const short p1[] = { 3, 5, 1, 2 };
	static const short c2[] = { 0, 0 };
	static const short p2[] = { 7, 0 };
	static const short c3[] = { 200, 100, 50, 0 };
	static const short p3[] = { 20000, 20000, 20000, 1 };
	/*
	 * The hostile script: the handler leaves a count with the top bit
	 * set, so the object's sign-extended previous count and zero-extended
	 * new one disagree by 65,536 and the input cursor lands a long way
	 * outside the buffer.  Both sides go there; what is compared is the
	 * cursor the handler was handed, which is why the handler checks its
	 * window before touching anything.
	 */
	static const short c4[] = { -32768, 0 };
	static const short p4[] = { 2, 1 };

	diff_begin("V21RX_modem");
	run_rxm_one(12, 0x00, 0x11223344, c1, p1, 4, 0x0a0b0c0du);
	run_rxm_one(12, 0xff, 0x00000000, c1, p1, 4, 0x0a0b0c0du);
	run_rxm_one(0, 0x02, 0x7f000001, c2, p2, 2, 0x1b2b3b4bu);
	run_rxm_one(1, 0x02, (int)0xfffffffeu, c2, p2, 2, 0x1b2b3b4bu);
	run_rxm_one(300, 0x22, 0x0000ff00, c3, p3, 4, 0x5a5a5a5au);
	run_rxm_one(-1, 0x02, 0x12345678, c4, p4, 2, 0x77665544u);
	return diff_end();
}

/* --------------------------------------------------------------------- */
/* V21RX_delete                                                          */

extern void ref_V21RX_delete(void *modem);

#define DEL_BLOCKS	10

struct delfix {
	void	*p[DEL_BLOCKS];
	int	live[DEL_BLOCKS];
	int	allocs, frees, bad_free, live_total;
};

/*
 * Build a whole receiver out of the harness allocator, so that "was this
 * freed" is a question the allocator can answer rather than a guess.  Every
 * block below is a SEPARATE allocation, which is what makes the liveness
 * vector a per-sub-object answer.
 */
static void *
build_receiver(struct delfix *d)
{
	unsigned char *obj;
	struct v21_rx_dsp *dsp;
	struct fpm_mtd *mtd;
	int i;

	for (i = 0; i < DEL_BLOCKS; i++)
		d->p[i] = 0;

	obj = (unsigned char *)sysdep_malloc(RXOBJ_SIZE);
	dsp = (struct v21_rx_dsp *)sysdep_malloc(sizeof(*dsp));
	mtd = (struct fpm_mtd *)sysdep_malloc(sizeof(*mtd));

	memset(obj, 0, RXOBJ_SIZE);
	memset(dsp, 0, sizeof(*dsp));
	memset(mtd, 0, sizeof(*mtd));

	d->p[0] = obj;
	d->p[1] = sysdep_malloc(0x10);		/* the half-duplex context  */
	d->p[2] = dsp;
	d->p[3] = sysdep_malloc(64 * sizeof(short));	/* mag              */
	d->p[4] = mtd;
	d->p[5] = sysdep_malloc(16 * sizeof(short));	/* mtd->acc         */
	d->p[6] = sysdep_malloc(32 * sizeof(short));	/* fsd.trace        */
	d->p[7] = sysdep_malloc(32 * sizeof(short));	/* fsd.fir_hist     */
	d->p[8] = sysdep_malloc(32 * sizeof(short));	/* fsd.iir_hist     */
	d->p[9] = sysdep_malloc(32 * sizeof(short));	/* mrf.history      */

	memset(d->p[1], 0, 0x10);

	*(void **)(void *)(obj + V21RX_OBJ_HDX) = d->p[1];
	*(void **)(void *)(obj + V21RX_OBJ_DSP) = (void *)dsp;

	dsp->mag = (short *)d->p[3];
	dsp->mtd = mtd;
	mtd->acc = (short *)d->p[5];
	dsp->fsd.trace = (short *)d->p[6];
	dsp->fsd.fir_hist = (short *)d->p[7];
	dsp->fsd.iir_hist = (short *)d->p[8];
	dsp->mrf.history = (short *)d->p[9];

	return (void *)obj;
}

static void
record_liveness(struct delfix *d)
{
	int i;

	for (i = 0; i < DEL_BLOCKS; i++)
		d->live[i] = harness_alloc_ordinal(d->p[i]) != 0;
	d->allocs = harness_alloc.allocs;
	d->frees = harness_alloc.frees;
	d->bad_free = harness_alloc.bad_free;
	d->live_total = harness_alloc.live;
}

static long del_trials, del_freed_all;

static int
run_delete(void)
{
	struct delfix da_, db_;
	void *m;
	int i;

	diff_begin("V21RX_delete");

	harness_alloc_reset();
	m = build_receiver(&da_);
	ref_V21RX_delete(m);
	record_liveness(&da_);

	harness_alloc_reset();
	m = build_receiver(&db_);
	V21RX_delete(m);
	record_liveness(&db_);

	for (i = 0; i < DEL_BLOCKS; i++)
		diff_eq_int("block %ld still live", (long)db_.live[i],
			    (long)da_.live[i], i);
	diff_eq_int("allocations", (long)db_.allocs, (long)da_.allocs, 0);
	diff_eq_int("frees", (long)db_.frees, (long)da_.frees, 0);
	diff_eq_int("bad frees", (long)db_.bad_free, (long)da_.bad_free, 0);
	diff_eq_int("outstanding", (long)db_.live_total, (long)da_.live_total,
		    0);

	/*
	 * The two assertions that make the vector mean something rather than
	 * merely agree: the blob freed EVERY block and fumbled none.  Without
	 * these, two implementations that both leaked everything would pass.
	 */
	diff_eq_int("the blob left nothing live", (long)da_.live_total, 0, 0);
	diff_eq_int("the blob made no bad free", (long)da_.bad_free, 0, 0);
	for (i = 0; i < DEL_BLOCKS; i++)
		diff_eq_int("the blob freed block %ld", (long)da_.live[i], 0,
			    i);

	del_trials++;
	if (da_.live_total == 0 && da_.frees == DEL_BLOCKS)
		del_freed_all++;

	return diff_end();
}

/* --------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	build_tx_tables();

	rc |= run_carrier();
	rc |= run_snr();
	rc |= run_status();
	rc |= run_rxstatus();
	rc |= run_txdata();
	rc |= run_rxmodem();
	rc |= run_delete();

	/*
	 * The separating counts.  Each is the number of trials on which a
	 * NAMED wrong reading produced a different OBSERVABLE answer from the
	 * blob's, counted from what the blob actually did.  A zero here means
	 * the corresponding check above is decoration -- finding F134.
	 */
	diff_begin("v21fax separating trials");
	diff_eq_int("the carrier pair one field low separates (%ld)",
		    cd_low_sep > 0, 1, cd_low_sep);
	diff_eq_int("the carrier pair one field high separates (%ld)",
		    cd_high_sep > 0, 1, cd_high_sep);
	diff_eq_int("OR rather than AND separates (%ld)", cd_or_sep > 0, 1,
		    cd_or_sep);
	diff_eq_int("the blob reported carrier present (%ld)", cd_true > 0, 1,
		    cd_true);
	diff_eq_int("the blob reported carrier absent (%ld)", cd_false > 0, 1,
		    cd_false);

	diff_eq_int("GetSNRV21 wrote its buffer (%ld)", snr_wrote > 0, 1,
		    snr_wrote);
	diff_eq_int("a negative count was driven (%ld)", snr_neg_count > 0, 1,
		    snr_neg_count);
	diff_eq_int("a zero count was driven (%ld)", snr_zero_count > 0, 1,
		    snr_zero_count);
	diff_eq_int("-32768 came back as itself (%ld)", snr_min_short > 0, 1,
		    snr_min_short);
	diff_eq_int("the trace field separates (%ld)", snr_trace_sep > 0, 1,
		    snr_trace_sep);
	diff_eq_int("the count field separates (%ld)", snr_count_sep > 0, 1,
		    snr_count_sep);
	diff_eq_int("the destination separates (%ld)", snr_dest_sep > 0, 1,
		    snr_dest_sep);
	diff_eq_int("taking the absolute value separates (%ld)",
		    snr_abs_sep > 0, 1, snr_abs_sep);

	diff_eq_int("V21TX_status returned 1 (%ld)", st_ret1 > 0, 1, st_ret1);

	diff_eq_int("V21RX_status returned 1 (%ld)", rxst_ret1 > 0, 1,
		    rxst_ret1);
	diff_eq_int("rx quality came out one (%ld)", rxst_q1 > 0, 1, rxst_q1);
	diff_eq_int("rx quality came out zero (%ld)", rxst_q0 > 0, 1, rxst_q0);
	diff_eq_int("the rate went to rx_bps and not tx_bps (%ld)",
		    rxst_bps_sep > 0, 1, rxst_bps_sep);
	diff_eq_int("GetSNRV21 was actually called (%ld)", rxst_snr_sep > 0, 1,
		    rxst_snr_sep);
	diff_eq_int("+0x0c was left alone where +0x0e was zeroed (%ld)",
		    rxst_gap_sep > 0, 1, rxst_gap_sep);
	diff_eq_int("the rx flags byte being STORED separates (%ld)",
		    rxst_flags_sep > 0, 1, rxst_flags_sep);
	diff_eq_int("the computed +0x12 left the bit rate (%ld)",
		    rxst_div_sep > 0, 1, rxst_div_sep);
	diff_eq_int("the rx null destination was exercised (%ld)",
		    rxst_null_trials > 0, 1, rxst_null_trials);
	diff_eq_int("the protocol offset separates (%ld)", st_proto_sep > 0, 1,
		    st_proto_sep);
	diff_eq_int("the flag-byte offset separates (%ld)",
		    st_flagoff_sep > 0, 1, st_flagoff_sep);
	diff_eq_int("the flag mask separates (%ld)", st_mask_sep > 0, 1,
		    st_mask_sep);
	diff_eq_int("assigning rather than merging separates (%ld)",
		    st_assign_sep > 0, 1, st_assign_sep);
	diff_eq_int("the caller lost bits to the assignment (%ld)",
		    st_bits_lost > 0, 1, st_bits_lost);
	diff_eq_int("the NULL destination was driven (%ld)",
		    st_null_trials > 0, 1, st_null_trials);

	diff_eq_int("ModDataV21 returned samples (%ld)", tx_mod_ret > 0, 1,
		    tx_mod_ret);
	diff_eq_int("the resampler carried state between blocks (%ld)",
		    tx_mod_state > 0, 1, tx_mod_state);
	diff_eq_int("muting separates from ModDataV21 (%ld)", tx_mute_sep > 0,
		    1, tx_mute_sep);
	diff_eq_int("the muted scale was restored and observed (%ld)",
		    tx_restore_sep > 0, 1, tx_restore_sep);
	diff_eq_int("the bit count separates from the sample count (%ld)",
		    tx_count_sep > 0, 1, tx_count_sep);
	diff_eq_int("an unshared intermediate separates (%ld)",
		    tx_share_sep > 0, 1, tx_share_sep);
	diff_eq_int("the bits reach the muted modulator (%ld)",
		    tx_bits_sep > 0, 1, tx_bits_sep);

	diff_eq_int("V21RX_modem dispatched (%ld)", rxm_calls > 0, 1,
		    rxm_calls);
	diff_eq_int("a zero count still dispatched (%ld)", rxm_zero_entry > 0,
		    1, rxm_zero_entry);
	diff_eq_int("the error flag was cleared (%ld)", rxm_flag_sep > 0, 1,
		    rxm_flag_sep);
	diff_eq_int("the count came back changed (%ld)", rxm_advance_sep > 0,
		    1, rxm_advance_sep);
	diff_eq_int("the mixed-signedness cursor separates (%ld)",
		    rxm_signed_sep > 0, 1, rxm_signed_sep);

	diff_eq_int("V21RX_delete was driven (%ld)", del_trials > 0, 1,
		    del_trials);
	diff_eq_int("the blob freed every block (%ld)", del_freed_all > 0, 1,
		    del_freed_all);
	rc |= diff_end();

	return rc;
}
