/*
 * t_v17smc.c -- differential test of V.17's SMCv17 coder: SMCv17_init and its
 *               three encoders (differential, absolute, trellis).
 *
 * Modelled on t_v32smc.c, whose fixture this borrows nearly whole -- V.17's
 * trellis arm reuses V.32's own tables (`TrellisEncodeDifTable`,
 * `TrellisTransitionTable`) and the same three-arm shape, at different
 * offsets in an instance nothing here models.  So the "smc" state is a raw
 * byte buffer, not a struct, and every field under test is poked and read at
 * its offset directly -- a wrong offset sends the wrong value to the shift,
 * the index or the ring and the two sides part company immediately.
 *
 * The ring is deliberately SHORTER than the run in most cases, so the wrap is
 * taken many times and the whole buffer is compared afterwards -- a wrap that
 * happened one symbol early would otherwise be invisible.  The unused tail of
 * each ring is filled with a sentinel and compared too, which is what catches
 * a write past `len`.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/v17fax.h"
#include "dsplib/v32smc.h"	/* TrellisEncodeDifTable, TrellisTransitionTable */

extern void ref_SMCv17_init(void *smc, const short *cfg);
extern void ref_SMCv17_encoder_dif(void *smc, struct fpm_smc_ring *ring,
				   const unsigned short *data,
				   unsigned short count);
extern void ref_SMCv17_encoder_abs(void *smc, struct fpm_smc_ring *ring,
				   const unsigned short *data,
				   unsigned short count);
extern void ref_SMCv17_encoder_tcm(void *smc, struct fpm_smc_ring *ring,
				   unsigned short *data, unsigned short count);
extern const unsigned short ref_SMCv17_PMAP4[4];
extern const unsigned short ref_SMCv17_ABS4[4];
extern const unsigned short ref_SMCv17_MOD[8];
extern const short ref_SMCv17_CFG[2];
extern const short ref_TrellisEncodeDifTable[16];
extern const short ref_TrellisTransitionTable[32];

#define SMC_SIZE	0x18	/* covers every offset either side touches   */
#define RING		64
#define RUN		200

/* Field offsets, from v17data.h -- smc-relative, i.e. V17FP_SMC_SHORT_NN
 * minus V17FP_SMC. */
#define OFF_MODE	0x00	/* signed char, TCM's tag only               */
#define OFF_QUAD	0x06
#define OFF_STATE	0x08	/* DIF's accumulator only                    */
#define OFF_TRELLIS	0x0c	/* TCM only                                  */
#define OFF_PREV	0x0e	/* TCM only                                  */
#define OFF_NBITS	0x12	/* TCM only, unsigned                        */

static short
get_s(const unsigned char *buf, int off)
{
	short v;

	memcpy(&v, buf + off, sizeof(v));
	return v;
}

static void
put_s(unsigned char *buf, int off, short v)
{
	memcpy(buf + off, &v, sizeof(v));
}

static void
fill(unsigned short *buf, int n, int seed)
{
	int i;
	unsigned int x = (unsigned int)seed * 2654435761u + 11u;

	for (i = 0; i < n; i++) {
		x = x * 1103515245u + 12345u;
		buf[i] = (unsigned short)(x >> 11);
	}
}

/* The WHOLE state buffer, byte by byte -- a stray write anywhere shows. */
static void
diff_smc(const unsigned char *sb, const unsigned char *sa)
{
	int i;

	for (i = 0; i < SMC_SIZE; i++)
		diff_eq_int("smc byte %ld", sb[i], sa[i], i);
}

static int
run(const char *label, int arm, short mode, short quad, short state,
    short limit)
{
	static unsigned short in[RUN];
	static short ring_a[RING], ring_b[RING];
	unsigned char sa[SMC_SIZE], sb[SMC_SIZE];
	struct fpm_smc_ring oa, ob;
	int fed, chunk, i;

	diff_begin(label);

	memset(sa, 0x5a, sizeof(sa));
	sa[OFF_MODE] = (unsigned char)mode;
	put_s(sa, OFF_QUAD, quad);
	put_s(sa, OFF_STATE, state);
	memcpy(sb, sa, sizeof(sa));

	memset(ring_a, 0x5a, sizeof(ring_a));
	memset(ring_b, 0x5a, sizeof(ring_b));
	memset(&oa, 0, sizeof(oa));
	oa.i = NULL;
	oa.q = NULL;
	oa.sym = ring_a;
	oa.widx = 3;			/* start off the origin */
	oa.ridx = 0;
	oa.len = limit;
	ob = oa;
	ob.sym = ring_b;

	fill(in, RUN, mode * 17 + quad * 31 + state);

	/* Ragged chunks: quad, state and widx are what carry across calls. */
	fed = 0;
	for (chunk = 1; fed + chunk <= RUN; chunk += 3) {
		if (arm) {
			ref_SMCv17_encoder_dif(sa, &oa, in + fed,
					       (unsigned short)chunk);
			SMCv17_encoder_dif(sb, &ob, in + fed,
					   (unsigned short)chunk);
		} else {
			ref_SMCv17_encoder_abs(sa, &oa, in + fed,
					       (unsigned short)chunk);
			SMCv17_encoder_abs(sb, &ob, in + fed,
					   (unsigned short)chunk);
		}
		diff_eq_int("quad after chunk at %ld", get_s(sb, OFF_QUAD),
			    get_s(sa, OFF_QUAD), fed);
		diff_eq_int("state after chunk at %ld", get_s(sb, OFF_STATE),
			    get_s(sa, OFF_STATE), fed);
		diff_eq_int("widx after chunk at %ld", ob.widx, oa.widx, fed);
		fed += chunk;
	}

	/* The whole ring, including the part beyond `limit`. */
	for (i = 0; i < RING; i++)
		diff_eq_int("ring[%ld]", ring_b[i], ring_a[i], i);

	diff_smc(sb, sa);
	diff_eq_int("the ring wrapped (%ld)", RUN > limit, 1, limit);

	return diff_end();
}

/*
 * The trellis coder needs its own fixture: it writes back to the input
 * buffer, so each side gets its own copy of the input and both copies are
 * compared afterwards.  `trellis` must start in 0..3 and `prev` in 0..7 --
 * every table index is derived from them and the object bounds neither.
 */
static int
run_tcm(const char *label, short nbits, short trellis, short prev, short quad,
	short limit)
{
	static unsigned short in_a[RUN], in_b[RUN], seed[RUN];
	static short ring_a[RING], ring_b[RING];
	unsigned char sa[SMC_SIZE], sb[SMC_SIZE];
	struct fpm_smc_ring oa, ob;
	unsigned short u;
	int fed, chunk, i;

	diff_begin(label);

	memset(sa, 0x5a, sizeof(sa));
	sa[OFF_MODE] = 1;
	put_s(sa, OFF_QUAD, quad);
	put_s(sa, OFF_TRELLIS, trellis);
	put_s(sa, OFF_PREV, prev);
	u = (unsigned short)nbits;
	memcpy(sa + OFF_NBITS, &u, sizeof(u));
	memcpy(sb, sa, sizeof(sa));

	memset(ring_a, 0x5a, sizeof(ring_a));
	memset(ring_b, 0x5a, sizeof(ring_b));
	memset(&oa, 0, sizeof(oa));
	oa.sym = ring_a;
	oa.widx = 1;
	oa.len = limit;
	ob = oa;
	ob.sym = ring_b;

	fill(seed, RUN, nbits * 13 + trellis);
	memcpy(in_a, seed, sizeof(seed));
	memcpy(in_b, seed, sizeof(seed));

	fed = 0;
	for (chunk = 1; fed + chunk <= RUN; chunk += 3) {
		ref_SMCv17_encoder_tcm(sa, &oa, in_a + fed,
				       (unsigned short)chunk);
		SMCv17_encoder_tcm(sb, &ob, in_b + fed, (unsigned short)chunk);
		diff_eq_int("quad at %ld", get_s(sb, OFF_QUAD),
			    get_s(sa, OFF_QUAD), fed);
		diff_eq_int("trellis at %ld", get_s(sb, OFF_TRELLIS),
			    get_s(sa, OFF_TRELLIS), fed);
		diff_eq_int("prev at %ld", get_s(sb, OFF_PREV),
			    get_s(sa, OFF_PREV), fed);
		diff_eq_int("widx at %ld", ob.widx, oa.widx, fed);
		fed += chunk;
	}

	/* The write-back: the caller's buffer is an output too. */
	for (i = 0; i < RUN; i++)
		diff_eq_int("input word %ld after masking", in_b[i], in_a[i],
			    i);
	for (i = 0; i < RING; i++)
		diff_eq_int("ring[%ld]", ring_b[i], ring_a[i], i);

	/* It must really have masked something, or the sweep proves little. */
	{
		int changed = 0;

		for (i = 0; i < RUN; i++)
			if (in_b[i] != seed[i])
				changed++;
		diff_eq_int("the input was masked in place (%ld)",
			    changed > 0, 1, changed);
	}

	diff_smc(sb, sa);

	return diff_end();
}

/* SMCv17_init: NULL takes SMCv17_CFG, non-NULL takes the caller's dword;
 * either way the five neutral shorts are cleared. */
static int
run_init(const char *label, int use_cfg, short c0, short c1)
{
	unsigned char sa[SMC_SIZE], sb[SMC_SIZE];
	short cfg[2];

	diff_begin(label);

	memset(sa, 0xa5, sizeof(sa));
	memcpy(sb, sa, sizeof(sa));
	cfg[0] = c0;
	cfg[1] = c1;

	ref_SMCv17_init(sa, use_cfg ? cfg : NULL);
	SMCv17_init(sb, use_cfg ? cfg : NULL);

	diff_smc(sb, sa);

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int i;

	diff_begin("the two quadrant maps");
	for (i = 0; i < 4; i++) {
		diff_eq_int("SMCv17_PMAP4[%ld]", SMCv17_PMAP4[i],
			    ref_SMCv17_PMAP4[i], i);
		diff_eq_int("SMCv17_ABS4[%ld]", SMCv17_ABS4[i],
			    ref_SMCv17_ABS4[i], i);
	}
	rc |= diff_end();

	diff_begin("the trellis coder's three tables");
	for (i = 0; i < 16; i++)
		diff_eq_int("TrellisEncodeDifTable[%ld]",
			    TrellisEncodeDifTable[i],
			    ref_TrellisEncodeDifTable[i], i);
	for (i = 0; i < 32; i++)
		diff_eq_int("TrellisTransitionTable[%ld]",
			    TrellisTransitionTable[i],
			    ref_TrellisTransitionTable[i], i);
	for (i = 0; i < 8; i++)
		diff_eq_int("SMCv17_MOD[%ld]", SMCv17_MOD[i],
			    ref_SMCv17_MOD[i], i);
	rc |= diff_end();

	diff_begin("SMCv17_CFG");
	diff_eq_int("SMCv17_CFG[%ld]", SMCv17_CFG[0], ref_SMCv17_CFG[0], 0);
	diff_eq_int("SMCv17_CFG[%ld]", SMCv17_CFG[1], ref_SMCv17_CFG[1], 1);
	rc |= diff_end();

	/* Absolute: mode only reaches the (untouched) tag byte. */
	rc |= run("abs, mode 0", 0, 0, 0, 0, 40);
	rc |= run("abs, mode 1, quad 2", 0, 1, 2, 0, 40);
	rc |= run("abs, ring 7", 0, 2, 3, 0, 7);
	rc |= run("abs, ring longer than the run", 0, 0, 1, 0, RING);

	/* Differential: the accumulator carries across chunks too. */
	rc |= run("dif, from rest", 1, 0, 0, 0, 40);
	rc |= run("dif, quad preloaded", 1, 1, 1, 0, 40);
	rc |= run("dif, state preloaded", 1, 2, 0, 9, 40);
	rc |= run("dif, both preloaded, ring 5", 1, 3, 2, 5, 5);

	/*
	 * The trellis coder.  `nbits` sets three masks at once, so the sweep
	 * is over it first (1..4, matching V17FP_SMC_SHORT_12's per-mode
	 * values); the `prev > 3` arm needs a prev above 3 to be taken at
	 * all, and the four trellis states are four different rows.
	 */
	rc |= run_tcm("tcm, nbits 3 (mode 0), from rest", 3, 0, 0, 0, 40);
	rc |= run_tcm("tcm, nbits 3, prev 5 (the +bit_hi arm)", 3, 0, 5, 0,
		      40);
	rc |= run_tcm("tcm, nbits 4 (mode 1), trellis 3, prev 7", 4, 3, 7, 3,
		      40);
	rc |= run_tcm("tcm, nbits 5 (mode 2)", 5, 1, 2, 1, 40);
	rc |= run_tcm("tcm, nbits 6 (mode 3), ring 9", 6, 2, 6, 2, 9);
	rc |= run_tcm("tcm, nbits 1", 1, 1, 4, 3, 40);
	rc |= run_tcm("tcm, nbits 2, ring 5", 2, 3, 3, 2, 5);

	rc |= run_init("init, NULL cfg", 0, 0, 0);
	rc |= run_init("init, explicit cfg", 1, 7, -3);
	rc |= run_init("init, explicit cfg, other values", 1, -1, 12345);

	return rc;
}
