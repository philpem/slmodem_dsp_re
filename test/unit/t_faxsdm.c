/*
 * t_faxsdm.c -- differential test of the fax pumps' copy of the SDM.
 *
 * `SDM_init`, `SDM_scrambler` and `SDM_descrambler` are byte-for-byte their
 * `FPM_SDM_*` twins (finding F8900), so this is a second copy of code
 * `t_fpm_sdm` already covers.  It is NOT a copy of that test: what it has to
 * establish is different.
 *
 *   - that OUR second copy is the same function, over the configs the FAX
 *     modems actually build -- V.29's 1 + x^-18 + x^-23 at three and four
 *     bits, which `t_fpm_sdm` never runs because V.22 does not use them;
 *   - that `SDM_CFG`'s six bytes are the object's six bytes, WITHOUT
 *     asserting that they are the polynomial, because they are not: every
 *     caller overwrites them (finding F8904);
 *   - that state carries across a call boundary, which is what a fax pump
 *     does with it -- `ScrambleDataV17` is called once per block.
 *
 * Every case compares the whole object after every single word, not the
 * buffer at the end, for the reason `t_sdmv27` sets out at more length: a
 * scrambler that agrees on a word while disagreeing on the register that
 * produced it will disagree on a word later, where nothing localises it.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/sdm.h"

extern void ref_SDM_init(void *sdm, const void *cfg);
extern void ref_SDM_scrambler(void *sdm, unsigned short *data,
			      unsigned short count);
extern void ref_SDM_descrambler(void *sdm, unsigned short *data,
				unsigned short count);
extern struct fpm_sdm_cfg ref_SDM_CFG;

#define NWORDS 256

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

/*
 * The first four are what V.17 and V.29 build: 1 + x^-18 + x^-23 with three
 * or four bits a symbol (V29TX_create's `nbits = <rate> + 3`).  The rest are
 * synthetic edges -- a shift of zero, a tap at the register's top, and a word
 * as wide as the register.
 */
static const struct sdm_case cases[] = {
	{ "V.29 9600 (4,18,23)",	 4, 18, 23 },
	{ "V.29 7200 (3,18,23)",	 3, 18, 23 },
	{ "V.17 (4,18,23)",		 4, 18, 23 },
	{ "V.17 short (3,18,23)",	 3, 18, 23 },
	{ "SDM_CFG as it stands (4,5,23)", 4,  5, 23 },
	{ "tap1 == nbits (4,4,23)",	 4,  4, 23 },
	{ "register top (4,18,31)",	 4, 18, 31 },
	{ "whole word (16,16,23)",	16, 16, 23 }
};

#define NCASES ((int)(sizeof(cases) / sizeof(cases[0])))

static void
step_stream(int dir, struct fpm_sdm *ours, struct fpm_sdm *ref,
	    const unsigned short *in, unsigned short *out, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		unsigned short wa = in[i];
		unsigned short wb = in[i];

		if (dir == 0) {
			ref_SDM_scrambler(ref, &wb, 1);
			SDM_scrambler(ours, &wa, 1);
		} else {
			ref_SDM_descrambler(ref, &wb, 1);
			SDM_descrambler(ours, &wa, 1);
		}
		diff_eq_int("word %ld", wa, wb, i);
		diff_eq_int("reg after word %ld", (long)ours->reg,
			    (long)ref->reg, i);
		diff_eq_obj("after word", struct fpm_sdm, ours, ref, i);
		if (out != 0)
			out[i] = wb;
	}
}

static int
run_case(const struct sdm_case *c)
{
	struct fpm_sdm_cfg cfg;
	struct fpm_sdm a, b;
	static unsigned short src[NWORDS];
	static unsigned short mid[NWORDS];
	int i;

	cfg.nbits = c->nbits;
	cfg.tap1 = c->tap1;
	cfg.tap2 = c->tap2;

	diff_begin(c->name);

	/* Over a dirty object, so the two bytes init never writes have to
	 * compare equal by being left alone rather than by being cleared. */
	memset(&a, 0xa5, sizeof(a));
	memset(&b, 0xa5, sizeof(b));
	ref_SDM_init(&b, &cfg);
	SDM_init(&a, &cfg);
	diff_eq_obj("after init", struct fpm_sdm, &a, &b, 0);
	diff_eq_int("shift1 (%ld)", a.shift1, c->tap1 - c->nbits, 0);
	diff_eq_int("shift2 (%ld)", a.shift2, c->tap2 - c->nbits, 0);

	/*
	 * Half the words carry bits above `nbits`.  The descrambler feeds the
	 * received word into the register WHOLE, so those bits survive there
	 * and shift down into the taps; it is the one case that separates a
	 * zero-extended read of the data from a sign-extended one.
	 */
	for (i = 0; i < NWORDS; i++) {
		unsigned int r = rnd();

		src[i] = (unsigned short)((i & 1) ? r : (r & 0x0f));
	}

	step_stream(0, &a, &b, src, mid, NWORDS);

	/* A second block through the same objects -- the fax pumps call once
	 * per block and the register has to carry. */
	for (i = 0; i < NWORDS; i++)
		src[i] = (unsigned short)rnd();
	step_stream(0, &a, &b, src, 0, NWORDS);

	/* count == 0 still writes the register back. */
	{
		unsigned short w = 0x4321;

		SDM_scrambler(&a, &w, 0);
		ref_SDM_scrambler(&b, &w, 0);
		diff_eq_obj("scrambler count 0", struct fpm_sdm, &a, &b, 0);
		diff_eq_int("count 0 left the word (%ld)", w, 0x4321, 0);
	}

	/* The descrambler over what the scrambler produced. */
	memset(&a, 0x5a, sizeof(a));
	memset(&b, 0x5a, sizeof(b));
	ref_SDM_init(&b, &cfg);
	SDM_init(&a, &cfg);
	step_stream(1, &a, &b, mid, 0, NWORDS);

	/* A whole buffer in one call must land where the same words one at a
	 * time land. */
	{
		struct fpm_sdm ba;
		static unsigned short buf[NWORDS];

		/* The SAME fill as the object it is compared against: init
		 * never writes `pad06`, so a different fill would report a
		 * difference the functions did not make. */
		memset(&ba, 0x5a, sizeof(ba));
		SDM_init(&ba, &cfg);
		memcpy(buf, mid, sizeof(buf));
		SDM_descrambler(&ba, buf, NWORDS);
		diff_eq_obj("bulk vs stepped", struct fpm_sdm, &ba, &a, 0);
	}

	return diff_end();
}

/*
 * The table.  Compared to the blob's copy field by field, and then proved
 * equivalent by running init from both.  It is deliberately NOT asserted to
 * be 18 and 23: it is { 4, 5, 23 } and every caller overwrites it (F8904).
 */
static int
run_config(void)
{
	struct fpm_sdm a, b;

	diff_begin("SDM_CFG");

	diff_eq_int("nbits (%ld)", SDM_CFG.nbits, ref_SDM_CFG.nbits, 0);
	diff_eq_int("tap1 (%ld)", SDM_CFG.tap1, ref_SDM_CFG.tap1, 0);
	diff_eq_int("tap2 (%ld)", SDM_CFG.tap2, ref_SDM_CFG.tap2, 0);

	memset(&a, 0x5a, sizeof(a));
	memset(&b, 0x5a, sizeof(b));
	ref_SDM_init(&b, &ref_SDM_CFG);
	SDM_init(&a, &SDM_CFG);
	diff_eq_obj("init from the table", struct fpm_sdm, &a, &b, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int i;

	rnd_state = 0x0f1e2d3cu;

	for (i = 0; i < NCASES; i++)
		rc |= run_case(&cases[i]);

	rc |= run_config();

	return rc;
}
