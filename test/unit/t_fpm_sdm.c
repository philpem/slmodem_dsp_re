/*
 * t_fpm_sdm.c -- differential test of the scrambler/descrambler module.
 *
 * The object's own configuration exercises very little of this code: V.22bis
 * uses one polynomial with one pair of taps, and `nbits` only ever takes two
 * values.  So every case below runs a set of synthetic configs alongside
 * SDMv22_CFG, chosen for the readings they would break:
 *
 *   tap == nbits          shift1 or shift2 of zero, the boundary of the
 *                         "a tap may not reach inside the word being formed"
 *                         constraint the biased shifts impose.
 *   nbits == 16           `mask` covers the whole word, so the second pass
 *                         the descrambler makes over it is a no-op.
 *   tap2 == 31            the register's top: bits shifted past 31 are lost,
 *                         and nothing masks the register to its own length.
 *   words with bits set above `nbits`
 *                         the descrambler feeds the RECEIVED word into the
 *                         register whole.  Those bits survive there and shift
 *                         down into the taps, so a `short` data pointer --
 *                         sign-extending rather than zero-extending -- would
 *                         diverge here and nowhere else.  This is the one
 *                         case that pins the declared type.
 *
 * Comparison is `reg` at every single step, not just the words: a scrambler
 * whose output happens to agree while its register does not is a scrambler
 * that will disagree later.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/v22txtab.h"

extern void ref_FPM_SDM_init(void *sdm, const void *cfg);
extern void ref_FPM_SDM_scrambler(void *sdm, unsigned short *data,
				  unsigned short count);
extern void ref_FPM_SDM_descrambler(void *sdm, unsigned short *data,
				    unsigned short count);
extern const struct fpm_sdm_cfg ref_SDMv22_CFG;

#define NWORDS 64

static unsigned int rnd_state;

static unsigned int
rnd(void)
{
	rnd_state = rnd_state * 1103515245u + 12345u;
	return rnd_state >> 8;
}

struct sdm_case {
	const char *name;
	short nbits;
	short tap1;
	short tap2;
};

static const struct sdm_case cases[] = {
	{ "V.22bis 2400 (4,14,17)",	4, 14, 17 },
	{ "V.22bis 1200 (2,14,17)",	2, 14, 17 },
	{ "bit at a time (1,14,17)",	1, 14, 17 },
	{ "answering poly (1,5,23)",	1,  5, 23 },
	{ "tap1 == nbits (8,8,17)",	8,  8, 17 },
	{ "both taps == nbits (4,4,4)",	4,  4,  4 },
	{ "whole word (16,16,17)",     16, 16, 17 },
	{ "register top (5,7,31)",	5,  7, 31 }
};

#define NCASES ((int)(sizeof(cases) / sizeof(cases[0])))

static void
make_cfg(struct fpm_sdm_cfg *cfg, const struct sdm_case *c)
{
	cfg->nbits = c->nbits;
	cfg->tap1 = c->tap1;
	cfg->tap2 = c->tap2;
}

/*
 * Both objects start life filled with the same non-zero pattern, so a field
 * init never writes -- pad06 -- compares equal only if both sides leave it
 * alone.  A struct assignment over unnamed padding would not.
 */
static void
init_pair(struct fpm_sdm *ours, struct fpm_sdm *ref,
	  const struct fpm_sdm_cfg *cfg, int input)
{
	memset(ours, 0xa5, sizeof(*ours));
	memset(ref, 0xa5, sizeof(*ref));
	ref_FPM_SDM_init(ref, cfg);
	FPM_SDM_init(ours, cfg);
	diff_eq_obj("after init", struct fpm_sdm, ours, ref, input);
}

/* One word at a time, so a divergence is localised to the step that caused
 * it rather than to the call that noticed. */
static void
step_stream(void (*ours_fn)(struct fpm_sdm *, unsigned short *,
			    unsigned short),
	    void (*ref_fn)(void *, unsigned short *, unsigned short),
	    struct fpm_sdm *ours, struct fpm_sdm *ref,
	    const unsigned short *in, int n, const char *tag)
{
	int i;

	(void)tag;
	for (i = 0; i < n; i++) {
		unsigned short wa = in[i];
		unsigned short wb = in[i];

		ref_fn(ref, &wb, 1);
		ours_fn(ours, &wa, 1);
		diff_eq_int("word %ld", wa, wb, i);
		diff_eq_int("reg after word %ld", (long)ours->reg,
			    (long)ref->reg, i);
	}
	diff_eq_obj("after stream", struct fpm_sdm, ours, ref, n);
}

static int
run_case(const struct sdm_case *c)
{
	struct fpm_sdm_cfg cfg;
	struct fpm_sdm a, b;
	unsigned short buf_a[NWORDS], buf_b[NWORDS];
	unsigned short src[NWORDS];
	unsigned int wordmask;
	int i;

	make_cfg(&cfg, c);
	wordmask = (c->nbits >= 16) ? 0xffffu : ((1u << c->nbits) - 1u);

	diff_begin(c->name);

	/* init, over a dirty object. */
	init_pair(&a, &b, &cfg, 0);
	diff_eq_int("mask (%ld)", a.mask, (1 << c->nbits) - 1, 0);
	diff_eq_int("shift1 (%ld)", a.shift1, c->tap1 - c->nbits, 0);
	diff_eq_int("shift2 (%ld)", a.shift2, c->tap2 - c->nbits, 0);

	/* Scramble a long well-formed stream in one call. */
	for (i = 0; i < NWORDS; i++)
		src[i] = (unsigned short)(rnd() & wordmask);
	memcpy(buf_a, src, sizeof(src));
	memcpy(buf_b, src, sizeof(src));
	ref_FPM_SDM_scrambler(&b, buf_b, NWORDS);
	FPM_SDM_scrambler(&a, buf_a, NWORDS);
	for (i = 0; i < NWORDS; i++)
		diff_eq_int("scrambled[%ld]", buf_a[i], buf_b[i], i);
	diff_eq_obj("after bulk scramble", struct fpm_sdm, &a, &b, 0);

	/* And again, one word per call, continuing from that state. */
	for (i = 0; i < NWORDS; i++)
		src[i] = (unsigned short)(rnd() & wordmask);
	step_stream(FPM_SDM_scrambler, ref_FPM_SDM_scrambler, &a, &b, src,
		    NWORDS, "scramble");

	/*
	 * Descramble the same stream, then descramble words carrying rubbish
	 * above `nbits` -- the case that decides whether the register takes
	 * the received word zero-extended or sign-extended.
	 */
	init_pair(&a, &b, &cfg, 1);
	for (i = 0; i < NWORDS; i++)
		src[i] = (unsigned short)(rnd() & wordmask);
	step_stream(FPM_SDM_descrambler, ref_FPM_SDM_descrambler, &a, &b, src,
		    NWORDS, "descramble");

	for (i = 0; i < NWORDS; i++)
		src[i] = (unsigned short)(rnd() | 0x8000u);
	step_stream(FPM_SDM_descrambler, ref_FPM_SDM_descrambler, &a, &b, src,
		    NWORDS, "descramble dirty");

	/* Scrambling dirty words too: the output is masked but the input
	 * still reaches the XOR. */
	init_pair(&a, &b, &cfg, 2);
	for (i = 0; i < NWORDS; i++)
		src[i] = (unsigned short)(rnd() | 0x8000u);
	step_stream(FPM_SDM_scrambler, ref_FPM_SDM_scrambler, &a, &b, src,
		    NWORDS, "scramble dirty");

	/* count == 0 still writes the register back. */
	a.reg = b.reg = 0x12345678u;
	ref_FPM_SDM_scrambler(&b, buf_b, 0);
	FPM_SDM_scrambler(&a, buf_a, 0);
	diff_eq_obj("scramble of nothing", struct fpm_sdm, &a, &b, 0);
	ref_FPM_SDM_descrambler(&b, buf_b, 0);
	FPM_SDM_descrambler(&a, buf_a, 0);
	diff_eq_obj("descramble of nothing", struct fpm_sdm, &a, &b, 0);

	return diff_end();
}

/*
 * The property that makes the thing a scrambler: descrambling recovers the
 * plaintext, and recovers it after a bounded number of words even when the
 * descrambler's register starts wrong.  Proved against the REFERENCE
 * implementation's own output, so it is a statement about the object and not
 * only about our copy of it.
 */
static int
run_selfsync(const struct sdm_case *c)
{
	struct fpm_sdm_cfg cfg;
	struct fpm_sdm tx, rx, rx_ref;
	unsigned short plain[NWORDS], line[NWORDS];
	unsigned short got[NWORDS], got_ref[NWORDS];
	unsigned int wordmask;
	int settle;
	int i;

	make_cfg(&cfg, c);
	wordmask = (c->nbits >= 16) ? 0xffffu : ((1u << c->nbits) - 1u);
	/* Words needed for a wrong register to shift completely out. */
	settle = (c->tap2 + c->nbits - 1) / c->nbits;

	diff_begin("self-synchronisation");

	for (i = 0; i < NWORDS; i++)
		plain[i] = (unsigned short)(rnd() & wordmask);

	FPM_SDM_init(&tx, &cfg);
	memcpy(line, plain, sizeof(plain));
	FPM_SDM_scrambler(&tx, line, NWORDS);

	/* Clean register: exact recovery from the first word. */
	FPM_SDM_init(&rx, &cfg);
	ref_FPM_SDM_init(&rx_ref, &cfg);
	memcpy(got, line, sizeof(line));
	memcpy(got_ref, line, sizeof(line));
	FPM_SDM_descrambler(&rx, got, NWORDS);
	ref_FPM_SDM_descrambler(&rx_ref, got_ref, NWORDS);
	for (i = 0; i < NWORDS; i++) {
		diff_eq_int("recovered[%ld]", got[i], plain[i], i);
		diff_eq_int("reference recovered[%ld]", got_ref[i], plain[i],
			    i);
	}

	/*
	 * Wrong register.  Both sides must be wrong in the SAME way for the
	 * settling window, then right afterwards -- which is the whole point
	 * of a feed-forward descrambler and the only "reset path" there is:
	 * there is no reset branch in the code, the register simply flushes.
	 */
	FPM_SDM_init(&rx, &cfg);
	ref_FPM_SDM_init(&rx_ref, &cfg);
	rx.reg = rx_ref.reg = 0xdeadbeefu;
	memcpy(got, line, sizeof(line));
	memcpy(got_ref, line, sizeof(line));
	for (i = 0; i < NWORDS; i++) {
		FPM_SDM_descrambler(&rx, &got[i], 1);
		ref_FPM_SDM_descrambler(&rx_ref, &got_ref[i], 1);
		diff_eq_int("resync word %ld", got[i], got_ref[i], i);
		diff_eq_int("resync reg %ld", (long)rx.reg, (long)rx_ref.reg,
			    i);
		if (i >= settle)
			diff_eq_int("resynced by word %ld", got[i], plain[i],
				    i);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int i;

	rnd_state = 0x13579bdfu;

	for (i = 0; i < NCASES; i++)
		rc |= run_case(&cases[i]);

	/* The two the modem actually runs, plus one with a shift of zero. */
	rc |= run_selfsync(&cases[0]);
	rc |= run_selfsync(&cases[1]);
	rc |= run_selfsync(&cases[4]);

	/*
	 * The table.  SDMv22_CFG is six bytes and every one of them decides
	 * the polynomial, so it is compared field by field and then proved
	 * equivalent by running init from both copies.
	 */
	diff_begin("SDMv22_CFG");
	{
		struct fpm_sdm a, b;

		diff_eq_int("nbits (%ld)", SDMv22_CFG.nbits,
			    ref_SDMv22_CFG.nbits, 0);
		diff_eq_int("tap1 (%ld)", SDMv22_CFG.tap1,
			    ref_SDMv22_CFG.tap1, 0);
		diff_eq_int("tap2 (%ld)", SDMv22_CFG.tap2,
			    ref_SDMv22_CFG.tap2, 0);
		/* V.22bis section 2.5, calling modem: 1 + x^-14 + x^-17. */
		diff_eq_int("polynomial tap1 (%ld)", SDMv22_CFG.tap1, 14, 0);
		diff_eq_int("polynomial tap2 (%ld)", SDMv22_CFG.tap2, 17, 0);

		memset(&a, 0x5a, sizeof(a));
		memset(&b, 0x5a, sizeof(b));
		ref_FPM_SDM_init(&b, &ref_SDMv22_CFG);
		FPM_SDM_init(&a, &SDMv22_CFG);
		diff_eq_obj("init from the table", struct fpm_sdm, &a, &b, 0);
	}
	rc |= diff_end();

	return rc;
}
