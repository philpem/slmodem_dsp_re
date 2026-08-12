/*
 * t_fpm_ecc.c -- differential test of the echo canceller's setup.
 *
 * The eight heap pointers necessarily differ between the two builds, so the
 * states are copied, the pointer slots blanked in both copies, and the rest
 * compared as one object -- which keeps the five padding holes (0x06, 0x12,
 * 0x1a, 0x26, 0x36) inside the comparison, where a field-by-field check would
 * have skipped exactly the bytes an unmodelled store would land in.  The
 * buffers themselves are then compared by content.
 *
 * ECC_CFG is compared against the blob's own copy directly, and again
 * indirectly: FPM_ECC_init(state, NULL, 1) takes it as the configuration, so
 * every field of it reaches the state comparison.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_ecc.h"

extern void ref_FPM_ECC_init(void *state, const void *cfg, int fresh);
extern void ref_FPM_ECC_free(void *state);
extern struct fpm_ecc_cfg ref_ECC_CFG;

/*
 * Six constellation maps of our own.  The real ones are SMCv32_IMAP16 and
 * friends; init only copies the two pointers, and the cancel test needs maps
 * wide enough to accept any low byte, so these are 256 entries each.
 */
static short map_i[6][256];
static short map_q[6][256];
static const short *const test_imap[6] = {
	map_i[0], map_i[1], map_i[2], map_i[3], map_i[4], map_i[5]
};
static const short *const test_qmap[6] = {
	map_q[0], map_q[1], map_q[2], map_q[3], map_q[4], map_q[5]
};

static void
cmp_array(const char *tag, const short *got, const short *want, int n,
	  long input)
{
	int i;
	int bad = -1;

	for (i = 0; i < n; i++) {
		if (got[i] != want[i]) {
			bad = i;
			break;
		}
	}
	diff_eq_int(tag, bad, -1, input);
	if (bad >= 0)
		diff_eq_int("  first differing element", got[bad], want[bad],
			    bad);
}

static void
cmp_state(const char *tag, const struct fpm_ecc *got,
	  const struct fpm_ecc *want, long input)
{
	struct fpm_ecc a, b;
	int j;

	memcpy(&a, got, sizeof(a));
	memcpy(&b, want, sizeof(b));
	a.line = b.line = 0;
	a.near_i = b.near_i = 0;
	a.near_q = b.near_q = 0;
	a.far_i = b.far_i = 0;
	a.far_q = b.far_q = 0;
	for (j = 0; j < 3; j++)
		a.coef[j] = b.coef[j] = 0;
	diff_eq_obj(tag, struct fpm_ecc, &a, &b, input);

	diff_eq_int("line allocated", got->line != 0, 1, input);
	cmp_array("line contents", got->line, want->line, want->line_len,
		  input);
	cmp_array("near_i contents", got->near_i, want->near_i,
		  want->near_len, input);
	cmp_array("near_q contents", got->near_q, want->near_q,
		  want->near_len, input);
	cmp_array("far_i contents", got->far_i, want->far_i, want->far_len,
		  input);
	cmp_array("far_q contents", got->far_q, want->far_q, want->far_len,
		  input);
	for (j = 0; j < 3; j++)
		cmp_array("coef contents", got->coef[j], want->coef[j],
			  2 * (want->cfg.near_taps + want->cfg.far_taps), j);
}

/*
 * One configuration, through the three paths: fresh allocation, re-init in
 * place, and re-init after the caller has scribbled on the state.
 */
static void
run(const struct fpm_ecc_cfg *cfg, short near_delay, short far_delay,
    long input)
{
	struct fpm_ecc a, b;
	short *keep;
	int i;

	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	a.near_delay = b.near_delay = near_delay;
	a.far_delay = b.far_delay = far_delay;

	ref_FPM_ECC_init(&a, cfg, 1);
	FPM_ECC_init(&b, cfg, 1);
	cmp_state("fresh", &b, &a, input);

	/*
	 * Re-init in place.  Nothing is freed and nothing is allocated, so
	 * the buffers must be the same ones, cleared again.
	 */
	keep = b.line;
	for (i = 0; i < a.line_len; i++) {
		a.line[i] = (short)(i * 7);
		b.line[i] = (short)(i * 7);
	}
	a.mu = b.mu = 0x1234;
	a.phase = b.phase = 2;
	ref_FPM_ECC_init(&a, cfg, 0);
	FPM_ECC_init(&b, cfg, 0);
	cmp_state("re-init", &b, &a, input);
	diff_eq_int("re-init kept the buffer", b.line == keep, 1, input);

	ref_FPM_ECC_free(&a);
	FPM_ECC_free(&b);
}

int
main(void)
{
	struct fpm_ecc_cfg cfg;
	int i, j;

	for (i = 0; i < 6; i++)
		for (j = 0; j < 256; j++) {
			map_i[i][j] = (short)(1000 * i + j);
			map_q[i][j] = (short)(-500 * i - j);
		}

	diff_begin("ECC_CFG");
	diff_eq_obj("the library default", struct fpm_ecc_cfg, &ECC_CFG,
		    &ref_ECC_CFG, 0);
	if (diff_end() != 0)
		return 1;

	diff_begin("FPM_ECC_init");

	/* The V.32 shape: 480 symbols of far lag, 40 taps a side. */
	memset(&cfg, 0, sizeof(cfg));
	cfg.far_lag = 480;
	cfg.near_taps = 40;
	cfg.far_taps = 40;
	cfg.imap = test_imap;
	cfg.qmap = test_qmap;
	cfg.fill = 16;
	run(&cfg, 24, 8, 1);
	run(&cfg, 1, 1, 2);
	run(&cfg, 0, 0, 3);		/* both taps at the top of the line */
	run(&cfg, 200, 300, 4);

	/* A near section only, and a far section only. */
	cfg.near_taps = 0;
	run(&cfg, 4, 4, 5);
	cfg.near_taps = 40;
	cfg.far_taps = 0;
	run(&cfg, 4, 4, 6);

	/* Short lines, so the read taps sit near their wrap. */
	cfg.far_lag = 8;
	cfg.near_taps = 6;
	cfg.far_taps = 5;
	cfg.fill = 0x0103;
	run(&cfg, 3, 2, 7);
	run(&cfg, 0, 1, 8);
	cfg.far_lag = 1;
	run(&cfg, 0, 0, 9);		/* line_len 1: the modulo's edge */

	/* No configuration at all: the library default is taken. */
	run(0, 12, 6, 10);
	run(0, 0, 0, 11);

	return diff_end();
}
