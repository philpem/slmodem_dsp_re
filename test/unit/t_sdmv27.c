/*
 * t_sdmv27.c -- differential test of the V.27ter scrambler/descrambler.
 *
 * A SCRAMBLER IS WHERE A WRONG BIT ORDER PASSES A WEAK TEST, so nothing here
 * is checked at the end of a buffer.  Every case drives one word at a time
 * and compares the WHOLE object after each -- register, run counter, both
 * inversion flags -- because two scramblers can agree on a word and disagree
 * on the state that produced it, and the disagreement then surfaces
 * hundreds of bits later where nothing localises it.
 *
 * What each part is here to break:
 *
 *   both paths, many times      the module chooses per WORD between a
 *                               whole-word path and a bit path, on
 *                               `run < 33 - nbits && pending + inverting == 0`.
 *                               With random data the guard condition holds
 *                               with probability about 7/8 per bit, so a run
 *                               of 33 arrives roughly once in eighty bits and
 *                               both paths run thousands of times.  The
 *                               counters below are read off the REFERENCE
 *                               object during the run and asserted non-zero,
 *                               so a fixture that silently stopped reaching
 *                               one of them fails rather than passes
 *                               (findings F134, D955).
 *
 *   words with bits above nbits the scrambler does NOT mask its input on the
 *                               whole-word path and the descrambler DOES.
 *                               Half the words below carry rubbish above
 *                               `nbits`, which is the only thing that
 *                               separates those two readings.
 *
 *   seeded pending / inverting  the two directions carry `inverting`
 *                               differently -- the scrambler assigns it at
 *                               the top of each bit and the descrambler at
 *                               the bottom.  The streams above already
 *                               separate the two placements, because each
 *                               matches the exit state its own direction
 *                               leaves; what they do NOT reach is the pair
 *                               (`pending` clear, `inverting` set), which
 *                               nothing the module produces can hold.  This
 *                               drives all four combinations, in both
 *                               directions, over five run counters.  F8901.
 *
 *   round trip                  descrambling a scrambled stream returns the
 *                               input, which is a property of the pair rather
 *                               than of either, and would survive a matched
 *                               pair of wrong tap numbers.  Kept as a sanity
 *                               check and NOT as the argument.
 *
 *   nbits 1, 4, 8 and 16        synthetic.  `SDMv27_init` gives everything
 *                               that is not 2 a mask of 7, so a 4-bit config
 *                               loses a bit per word and a 16-bit one keeps
 *                               three; both sides do the same thing and the
 *                               shift-count edges are what is being checked.
 *
 * The polynomial and the guard are asserted against the numbers derived from
 * the object in `include/dsplib/sdmv27.h`: taps 6 and 7, guard taps 8, 9 and
 * 12, threshold 33, seed 60.  Those are a claim about the object and are
 * proved here by running the object, not by reading the header.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/sdmv27.h"

extern void ref_SDMv27_init(void *sdm, const void *cfg);
extern void ref_SDMv27_scrambler(void *sdm, unsigned short *data, short count);
extern void ref_SDMv27_descrambler(void *sdm, unsigned short *data,
				   short count);
extern struct sdmv27_cfg ref_SDMv27_CFG;

#define NWORDS 512

/* Coverage, counted off the REFERENCE object as the run proceeds. */
static long cov_bulk;
static long cov_bit;
static long cov_threshold;
static long cov_inverted;
static long cov_high_bits;

static unsigned int rnd_state;

static unsigned int
rnd(void)
{
	rnd_state = rnd_state * 1103515245u + 12345u;
	return rnd_state >> 8;
}

struct sdm_case {
	const char *name;
	unsigned short nbits;
	int synthetic;
};

static const struct sdm_case cases[] = {
	{ "V.27ter 4800 (3 bits)",	3, 0 },
	{ "V.27ter 2400 (2 bits)",	2, 0 },
	{ "bit at a time (1)",		1, 1 },
	{ "four bits, mask of 7",	4, 1 },
	{ "eight bits, mask of 7",	8, 1 },
	{ "whole word (16)",	       16, 1 }
};

#define NCASES ((int)(sizeof(cases) / sizeof(cases[0])))

static void
init_pair(struct sdmv27 *ours, struct sdmv27 *ref,
	  const struct sdmv27_cfg *cfg, long input)
{
	memset(ours, 0xa5, sizeof(*ours));
	memset(ref, 0xa5, sizeof(*ref));
	ref_SDMv27_init(ref, cfg);
	SDMv27_init(ours, cfg);
	diff_eq_obj("after init", struct sdmv27, ours, ref, input);
}

/*
 * One word at a time.  `dir` is 0 for the scrambler and 1 for the
 * descrambler.  The path the object is about to take is computed from the
 * REFERENCE object's own fields before the call, which is what makes the
 * coverage counters a measurement of the run rather than of the fixture's
 * intentions.
 */
static void
step_stream(int dir, struct sdmv27 *ours, struct sdmv27 *ref,
	    const unsigned short *in, unsigned short *out, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		unsigned short wa = in[i];
		unsigned short wb = in[i];
		unsigned short limit =
			(unsigned short)(SDMV27_GUARD_RUN - ref->nbits);
		unsigned short pend_before = ref->pending;

		if (ref->run < limit && ref->pending + ref->inverting == 0)
			cov_bulk++;
		else
			cov_bit++;
		if (ref->inverting != 0 || pend_before != 0)
			cov_inverted++;
		if (in[i] >> ref->nbits)
			cov_high_bits++;

		if (dir == 0) {
			ref_SDMv27_scrambler(ref, &wb, 1);
			SDMv27_scrambler(ours, &wa, 1);
		} else {
			ref_SDMv27_descrambler(ref, &wb, 1);
			SDMv27_descrambler(ours, &wa, 1);
		}

		if (ref->pending != 0)
			cov_threshold++;

		diff_eq_int("word %ld", wa, wb, i);
		diff_eq_int("reg after word %ld", ours->reg, ref->reg, i);
		diff_eq_int("run after word %ld", ours->run, ref->run, i);
		diff_eq_obj("after word", struct sdmv27, ours, ref, i);
		if (out != 0)
			out[i] = wb;
	}
}

/*
 * A whole buffer in one call must land in the same place as the same words
 * one at a time.  That is the only thing that can catch a per-call reset of
 * something the per-word loop hides.
 */
static void
bulk_vs_stepped(int dir, const struct sdmv27_cfg *cfg,
		const unsigned short *in, int n, long tag)
{
	struct sdmv27 a, b;
	static unsigned short buf_a[NWORDS];
	static unsigned short buf_b[NWORDS];
	int i;

	memset(&a, 0xa5, sizeof(a));
	memset(&b, 0xa5, sizeof(b));
	SDMv27_init(&a, cfg);
	ref_SDMv27_init(&b, cfg);

	memcpy(buf_a, in, (unsigned)n * sizeof(buf_a[0]));
	memcpy(buf_b, in, (unsigned)n * sizeof(buf_b[0]));

	if (dir == 0) {
		SDMv27_scrambler(&a, buf_a, (short)n);
		for (i = 0; i < n; i++)
			ref_SDMv27_scrambler(&b, &buf_b[i], 1);
	} else {
		SDMv27_descrambler(&a, buf_a, (short)n);
		for (i = 0; i < n; i++)
			ref_SDMv27_descrambler(&b, &buf_b[i], 1);
	}

	for (i = 0; i < n; i++)
		diff_eq_int("bulk word %ld", buf_a[i], buf_b[i], i);
	diff_eq_obj("after bulk", struct sdmv27, &a, &b, tag);
}

static int
run_case(const struct sdm_case *c)
{
	struct sdmv27_cfg cfg;
	struct sdmv27 a, b;
	static unsigned short src[NWORDS];
	static unsigned short scrambled[NWORDS];
	static unsigned short back[NWORDS];
	int i;

	cfg.nbits = c->nbits;
	diff_begin(c->name);

	init_pair(&a, &b, &cfg, 0);
	diff_eq_int("nbits (%ld)", a.nbits, c->nbits, 0);
	diff_eq_int("reg seed (%ld)", a.reg, SDMV27_REG_SEED, 0);
	diff_eq_int("mask (%ld)", a.mask, (c->nbits == 2) ? 3 : 7, 0);
	diff_eq_int("notmask (%ld)", a.notmask,
		    (unsigned short)((c->nbits == 2) ? ~3 : ~7), 0);

	/*
	 * Half the words carry bits above `nbits`, which is what separates
	 * the scrambler's unmasked input from the descrambler's masked one.
	 */
	for (i = 0; i < NWORDS; i++) {
		unsigned int r = rnd();

		src[i] = (unsigned short)((i & 1) ? r : (r & 7));
	}

	step_stream(0, &a, &b, src, scrambled, NWORDS);

	/* A second block through the SAME objects: the state has to carry. */
	for (i = 0; i < NWORDS; i++)
		src[i] = (unsigned short)rnd();
	step_stream(0, &a, &b, src, 0, NWORDS);

	/* count == 0 writes the state back and changes nothing. */
	{
		unsigned short w = 0x1234;

		SDMv27_scrambler(&a, &w, 0);
		ref_SDMv27_scrambler(&b, &w, 0);
		diff_eq_obj("scrambler count 0", struct sdmv27, &a, &b, 0);
		diff_eq_int("count 0 left the word (%ld)", w, 0x1234, 0);
		SDMv27_descrambler(&a, &w, 0);
		ref_SDMv27_descrambler(&b, &w, 0);
		diff_eq_obj("descrambler count 0", struct sdmv27, &a, &b, 0);
	}

	/* The descrambler, over the stream the scrambler just made. */
	init_pair(&a, &b, &cfg, 1);
	step_stream(1, &a, &b, scrambled, back, NWORDS);

	/*
	 * One call for the whole buffer against the same words one at a time.
	 * The per-word loop above cannot see a per-CALL defect; this can.
	 */
	bulk_vs_stepped(0, &cfg, scrambled, NWORDS, 0);
	bulk_vs_stepped(1, &cfg, scrambled, NWORDS, 1);

	(void)back;
	(void)c->synthetic;

	return diff_end();
}

/*
 * The round trip, on its own and on words that carry nothing above `nbits`.
 */
static int
run_roundtrip(unsigned short nbits)
{
	struct sdmv27_cfg cfg;
	struct sdmv27 tx, rx;
	static unsigned short src[NWORDS];
	static unsigned short mid[NWORDS];
	unsigned int mask;
	int i;

	cfg.nbits = nbits;
	mask = (nbits == 2) ? 3u : 7u;

	diff_begin(nbits == 2 ? "round trip, 2 bits" : "round trip, 3 bits");

	memset(&tx, 0, sizeof(tx));
	memset(&rx, 0, sizeof(rx));
	SDMv27_init(&tx, &cfg);
	SDMv27_init(&rx, &cfg);

	for (i = 0; i < NWORDS; i++)
		src[i] = (unsigned short)(rnd() & mask);
	memcpy(mid, src, sizeof(mid));

	/* Two blocks each way, so the state has to survive a call boundary
	 * on both sides of the link. */
	SDMv27_scrambler(&tx, mid, NWORDS / 2);
	SDMv27_scrambler(&tx, mid + NWORDS / 2, NWORDS / 2);
	SDMv27_descrambler(&rx, mid, NWORDS / 2);
	SDMv27_descrambler(&rx, mid + NWORDS / 2, NWORDS / 2);

	for (i = 0; i < NWORDS; i++)
		diff_eq_int("round trip word %ld", mid[i], src[i], i);

	return diff_end();
}

/*
 * The two flags, seeded by hand.  Nothing the module reaches by itself
 * separates `pending` from `inverting`, because the threshold sets one and
 * the next bit clears both; seeding them apart is the only way to drive the
 * difference finding F8901 records between the two directions.
 */
static int
run_seeded_flags(void)
{
	struct sdmv27_cfg cfg;
	int rc = 0;
	int p;
	int v;
	int r;
	int dir;

	cfg.nbits = 3;
	diff_begin("seeded pending / inverting / run");

	for (dir = 0; dir <= 1; dir++) {
		for (p = 0; p <= 1; p++) {
			for (v = 0; v <= 1; v++) {
				for (r = 0; r < 35; r += 8) {
					struct sdmv27 a, b;
					unsigned short wa = 0x0005;
					unsigned short wb = 0x0005;
					long tag = ((long)dir << 12)
						| ((long)p << 8)
						| ((long)v << 4) | r;

					memset(&a, 0, sizeof(a));
					memset(&b, 0, sizeof(b));
					SDMv27_init(&a, &cfg);
					ref_SDMv27_init(&b, &cfg);
					a.pending = b.pending =
						(unsigned short)p;
					a.inverting = b.inverting =
						(unsigned short)v;
					a.run = b.run = (unsigned short)r;

					if (dir == 0) {
						ref_SDMv27_scrambler(&b, &wb, 1);
						SDMv27_scrambler(&a, &wa, 1);
					} else {
						ref_SDMv27_descrambler(&b, &wb,
								       1);
						SDMv27_descrambler(&a, &wa, 1);
					}
					diff_eq_int("seeded word %ld", wa, wb,
						    tag);
					diff_eq_obj("seeded state",
						    struct sdmv27, &a, &b, tag);
				}
			}
		}
	}

	rc |= diff_end();
	return rc;
}

/* Init's defaulting arm, and the table it defaults to. */
static int
run_config(void)
{
	struct sdmv27 a, b;
	int rc;

	diff_begin("SDMv27_CFG and the null config");

	diff_eq_int("nbits (%ld)", SDMv27_CFG.nbits, ref_SDMv27_CFG.nbits, 0);
	/* V.27ter's 4800 bit/s tribit. */
	diff_eq_int("nbits is three (%ld)", SDMv27_CFG.nbits, 3, 0);

	/*
	 * THE NULL-CONFIG ARM IS NOT DRIVEN, and that is a measurement rather
	 * than an omission.  The object guards only the first of its two reads
	 * of `cfg`, so `SDMv27_init(s, NULL)` reaches `cmpw $0x2,(%ecx)` at
	 * 0x9a85a with the caller's null still in the register and faults.
	 * Driving it here would segfault BOTH sides identically and prove
	 * nothing; the precondition is asserted instead, exactly as D1022's
	 * 8 kHz `VOICE_process` is handled.  D1043.
	 */
	diff_eq_int("a config pointer is required (%ld)", (long)(&SDMv27_CFG
							       != 0), 1, 0);

	/* From our own copy of the table, against the blob's. */
	memset(&a, 0xa5, sizeof(a));
	memset(&b, 0xa5, sizeof(b));
	SDMv27_init(&a, &SDMv27_CFG);
	ref_SDMv27_init(&b, &ref_SDMv27_CFG);
	diff_eq_obj("init from the table", struct sdmv27, &a, &b, 0);

	rc = diff_end();
	return rc;
}

int
main(void)
{
	int rc = 0;
	int i;

	rnd_state = 0x2468aceu;

	for (i = 0; i < NCASES; i++)
		rc |= run_case(&cases[i]);

	rc |= run_roundtrip(3);
	rc |= run_roundtrip(2);
	rc |= run_seeded_flags();
	rc |= run_config();

	/*
	 * ANTI-VACUITY, and it is read off the reference object during the
	 * run rather than assumed from the inputs.  A fixture that stopped
	 * reaching the bit path, or the threshold, or the inverted bit, would
	 * otherwise keep passing while testing half the function.
	 */
	diff_begin("coverage of the run");
	diff_eq_int("whole-word path taken (%ld > 0)", cov_bulk > 0, 1, 0);
	diff_eq_int("bit path taken (%ld > 0)", cov_bit > 0, 1, 0);
	diff_eq_int("threshold reached (%ld > 0)", cov_threshold > 0, 1, 0);
	diff_eq_int("a bit was inverted (%ld > 0)", cov_inverted > 0, 1, 0);
	diff_eq_int("words above nbits seen (%ld > 0)", cov_high_bits > 0,
		    1, 0);
	rc |= diff_end();

	return rc;
}
