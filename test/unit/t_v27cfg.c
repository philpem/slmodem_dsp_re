/*
 * t_v27cfg.c -- differential test of the V.27ter fax receiver's fifty-six
 *               data and rodata symbols: fourteen two-pointer rate selectors,
 *               their twenty-eight targets, eleven per-rate scalars, the two
 *               tone-detector banks and the AGC configuration.
 *
 * FIVE LAYERS, and each one exists because of a specific wrong reading the
 * layer below it would pass.  `t_v29cfg.c` is the pattern; what is new here is
 * that everything is done TWICE, once per rate, and that a whole class of
 * symbol -- the selector -- can be wrong in a way no byte comparison sees.
 *
 *   1. SHAPE.  `sizeof` against the `nm -S` size of every one of the
 *      fifty-six symbols, plus the relationships the constructor depends on.
 *      A byte comparison run over OUR array's length cannot notice that our
 *      array is one element short of the object's.  `V27RX_SRE_FILT_2400` is
 *      the live case: `sre.coeffs` is 270 and the table is 271, because the
 *      interpolator reads `proto[i+1]` at `i == coeffs-1`.
 *
 *   2. VALUE, element by element against `ref_`, first disagreement reported
 *      with its index.  And, for the selectors, TWO checks that no byte
 *      comparison contains: that ours point at the tables their names claim,
 *      by pointer identity, and that the BLOB's reach the same contents, by
 *      dereferencing.  A selector with 2400 and 4800 the wrong way round has
 *      perfectly correct bytes in every table and is still broken.  Finding
 *      F9153, which records the injection that proved this layer fires and
 *      which of the five layers saw it.
 *
 *   3. THE DETECTOR IS SHOWN TO FIRE (F134).  `shorts_differ` is run over an
 *      identical copy, which must return 0, and then over one perturbed copy
 *      per table with a single element changed by one -- EVERY element in
 *      turn -- which must all return non-zero.
 *
 *   4. THE SHAPE OF THE VALUES, independently of `ref_`.  The prototypes are
 *      asserted symmetric, the equaliser's rails symmetric and antisymmetric,
 *      their zero patterns asserted as IF AND ONLY IF against the closed form
 *      the carrier predicts, and the two carrier ramps asserted equal to
 *      `round(i * 32768 / n)`.  Those are what fix the element STRIDE, and
 *      they close on V.27ter's own 1800 Hz carrier at both rates.
 *
 *   5. USE.  Every table is driven through the DSP block that consumes it,
 *      ours against the blob's, with the configuration built exactly as
 *      `V27RX_create` builds it -- copy the library built-in, patch the
 *      tables and the lengths, call init -- FOR BOTH RATES.  A wrong LENGTH
 *      field does not survive this one: `FPM_SRE_init` copies `cfg.coeffs`
 *      entries out of `proto`, so a count that disagrees with the object's
 *      shows up in the coefficient buffer.
 *
 * WHERE THE PER-RATE SCALARS ARE CHECKED, AND WHY NOT IN LAYER 5.  The eleven
 * four-byte scalars are lengths and gains, and layer 5 hands OUR value to both
 * sides deliberately: `FPM_SRE_init` allocates from `cfg.coeffs`, so feeding
 * the two inits different counts would compare buffers of different sizes and
 * read off the end of one of them.  The scalars are proved in layer 2, element
 * by element against `ref_`, and tied to `st_size` in layer 1.  Changing
 * `V27RX_SAMP_PER_BAUD[0]` from 6 to 5 fails two checks in layer 1, one in
 * layer 2 and one in layer 4, which is how that division was confirmed rather
 * than assumed.
 *
 * WHAT IS COMPARED BY CONTENT RATHER THAN BY VALUE.  Every pointer here holds
 * an address, and ours can never equal the blob's.  Each is compared by
 * dereferencing it, which is the only claim a differential test can make
 * about a pointer -- and it is the claim that matters, since what a table IS
 * is its contents.
 *
 * `AGC_DEF_ALPHA` AND `AGC_DEF_BETA` HAVE NO `ref_` ALIAS AT ALL.  The object
 * defines each of those names six times, five of them local, so `symmap.py`
 * gives them none and there is nothing to compare by name.  They are reached
 * here only through `AGCv27_CFG.alpha` and `.beta`, which is the F9058 rule.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"

#include "dsplib/v27cfg.h"
#include "dsplib/faxcfg.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/sysdep.h"

/* The blob's copies.  An undeclared `ref_` name is a hard error, not an int. */
extern const struct fpm_agc_cfg ref_AGCv27_CFG;

extern const short ref_V27RX_FSE_FILT_LEN[2];
extern const short ref_V27RX_FSE_QFILT_4800[81];
extern const short ref_V27RX_FSE_QFILT_2400[97];
extern const short *const ref_V27RX_FSE_QFILT[2];
extern const short ref_V27RX_FSE_IFILT_4800[81];
extern const short ref_V27RX_FSE_IFILT_2400[97];
extern const short *const ref_V27RX_FSE_IFILT[2];
extern const short ref_V27RX_SAMP_PER_BAUD[2];
extern const short ref_V27RX_XB_COFFS_2400[11];
extern const short ref_V27RX_XB_COFFS_4800[11];
extern const short *const ref_V27RX_XB_COFFS[2];
extern const short ref_V27RX_SRE_FILT_2400[271];
extern const short ref_V27RX_SRE_FILT_4800[201];
extern const short ref_V27RX_SRE_FILT_LEN[2];
extern const short ref_V27RX_MRF_FILT_2400[270];
extern const short ref_V27RX_MRF_FILT_4800[36];
extern const short ref_V27RX_MRF_FILT_LEN[2];
extern const short ref_V27RX_MRF_DOWN[2];
extern const short ref_V27RX_MRF_UP[2];

extern short ref_V27_MTD_COEFF_2400[10];
extern short ref_V27_MTD_COEFF_4800[10];
extern short ref_V27RX_DEC_LAST_PHASE_2400[4];
extern short ref_V27RX_DEC_LAST_PHASE_4800[8];
extern short *ref_V27RX_DEC_LAST_PHASE[2];
extern short ref_V27RX_DEC_PMAP_2400[4];
extern short ref_V27RX_DEC_PMAP_4800[8];
extern short *ref_V27RX_DEC_PMAP[2];
extern short ref_V27RX_DEC_PHS_MASK[2];
extern short ref_V27RX_FSE_PLLK2_2400[3];
extern short ref_V27RX_FSE_PLLK1_2400[3];
extern short ref_V27RX_FSE_PLLK2_4800[3];
extern short ref_V27RX_FSE_PLLK1_4800[3];
extern short *ref_V27RX_FSE_PLLK2[2];
extern short *ref_V27RX_FSE_PLLK1[2];
extern short ref_V27RX_CRR_ADJUST[2];
extern short ref_V27RX_CRR_TABLE_2400[4];
extern short ref_V27RX_CRR_TABLE_4800[40];
extern short ref_V27RX_CRR_TABLE_LEN[2];
extern short *ref_V27RX_CRR_TABLE[2];
extern short ref_V27RX_FSE_MU_TRACK[2];
extern short ref_V27RX_FSE_MU_TRAIN[2];
extern short ref_V27RX_SRE_PLLK2_2400[3];
extern short ref_V27RX_SRE_PLLK1_2400[3];
extern short ref_V27RX_SRE_PLLK2_4800[3];
extern short ref_V27RX_SRE_PLLK1_4800[3];
extern short *ref_V27RX_SRE_PLLK2[2];
extern short *ref_V27RX_SRE_PLLK1[2];
extern short ref_V27RX_YCLOCK_2400[6];
extern short ref_V27RX_YCLOCK_4800[5];
extern short *ref_V27RX_YCLOCK[2];
extern short ref_V27RX_XCLOCK_2400[6];
extern short ref_V27RX_XCLOCK_4800[5];
extern short *ref_V27RX_XCLOCK[2];
extern const short *ref_V27RX_SRE_FILT[2];
extern const short *ref_V27RX_MRF_FILT[2];

extern short ref_V21_CHAN2_MTD_COEFF[10];

extern struct fpm_fse_cfg ref_FPM_FSE_CFG;
extern struct fpm_mtd_cfg ref_FPM_MTD_CFG;
extern const struct fpm_sre_cfg ref_FPM_SRE_CFG;

extern void ref_FPM_AGC_init(struct fpm_agc *agc,
			     const struct fpm_agc_cfg *cfg, int reset);
extern struct fpm_mtd *ref_FPM_MTD_create(struct fpm_mtd *state,
					  const struct fpm_mtd_cfg *cfg);
extern void ref_FPM_SRE_init(struct fpm_sre *sre,
			     const struct fpm_sre_cfg *cfg, int fresh);
extern void ref_FPM_SRE_free(struct fpm_sre *sre);
extern void ref_FPM_FSE_init(struct fpm_fse *state,
			     const struct fpm_fse_cfg *cfg, int fresh);
extern void ref_FPM_FSE_free(struct fpm_fse *state);
extern void ref_FPM_MRF_init(struct fpm_mrf *state,
			     const struct fpm_mrf_cfg *cfg, int fresh);

/* ------------------------------------------------------------------------- */

/*
 * Layer 3's detector.  Its own function rather than a `memcmp` call site, so
 * that it can be exercised on a known-perturbed input below.
 */
static int
shorts_differ(const short *a, const short *b, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return 1;
	return 0;
}

/* Compare `n` shorts and report the FIRST disagreement, with its index. */
static void
cmp_shorts(const char *what, const short *got, const short *want, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		if (got[i] != want[i]) {
			diff_eq_int(what, got[i], want[i], i);
			return;
		}
	}
	/* One passing check per table, so the count is not dominated by
	 * thousands of trivially equal entries. */
	diff_eq_int(what, 0, 0, n);
}

/* How many of `n` entries are non-zero.  Anti-vacuity for layer 2. */
static int
nonzero(const short *p, int n)
{
	int i, c = 0;

	for (i = 0; i < n; i++)
		if (p[i] != 0)
			c++;
	return c;
}

/*
 * The table register, used by three of the five layers so that a table added
 * to one is added to all of them.  `n` is the element count `nm -S` implies.
 */
struct tabent {
	const short *ours;
	const short *blob;
	int n;
	const char *name;
};

static const struct tabent TABLES[] = {
	{ V27RX_FSE_FILT_LEN,       ref_V27RX_FSE_FILT_LEN,        2,
	  "V27RX_FSE_FILT_LEN"       },
	{ V27RX_FSE_QFILT_4800,     ref_V27RX_FSE_QFILT_4800,     81,
	  "V27RX_FSE_QFILT_4800"     },
	{ V27RX_FSE_QFILT_2400,     ref_V27RX_FSE_QFILT_2400,     97,
	  "V27RX_FSE_QFILT_2400"     },
	{ V27RX_FSE_IFILT_4800,     ref_V27RX_FSE_IFILT_4800,     81,
	  "V27RX_FSE_IFILT_4800"     },
	{ V27RX_FSE_IFILT_2400,     ref_V27RX_FSE_IFILT_2400,     97,
	  "V27RX_FSE_IFILT_2400"     },
	{ V27RX_SAMP_PER_BAUD,      ref_V27RX_SAMP_PER_BAUD,       2,
	  "V27RX_SAMP_PER_BAUD"      },
	{ V27RX_XB_COFFS_2400,      ref_V27RX_XB_COFFS_2400,      11,
	  "V27RX_XB_COFFS_2400"      },
	{ V27RX_XB_COFFS_4800,      ref_V27RX_XB_COFFS_4800,      11,
	  "V27RX_XB_COFFS_4800"      },
	{ V27RX_SRE_FILT_2400,      ref_V27RX_SRE_FILT_2400,     271,
	  "V27RX_SRE_FILT_2400"      },
	{ V27RX_SRE_FILT_4800,      ref_V27RX_SRE_FILT_4800,     201,
	  "V27RX_SRE_FILT_4800"      },
	{ V27RX_SRE_FILT_LEN,       ref_V27RX_SRE_FILT_LEN,        2,
	  "V27RX_SRE_FILT_LEN"       },
	{ V27RX_MRF_FILT_2400,      ref_V27RX_MRF_FILT_2400,     270,
	  "V27RX_MRF_FILT_2400"      },
	{ V27RX_MRF_FILT_4800,      ref_V27RX_MRF_FILT_4800,      36,
	  "V27RX_MRF_FILT_4800"      },
	{ V27RX_MRF_FILT_LEN,       ref_V27RX_MRF_FILT_LEN,        2,
	  "V27RX_MRF_FILT_LEN"       },
	{ V27RX_MRF_DOWN,           ref_V27RX_MRF_DOWN,            2,
	  "V27RX_MRF_DOWN"           },
	{ V27RX_MRF_UP,             ref_V27RX_MRF_UP,              2,
	  "V27RX_MRF_UP"             },
	{ V27_MTD_COEFF_2400,       ref_V27_MTD_COEFF_2400,       10,
	  "V27_MTD_COEFF_2400"       },
	{ V27_MTD_COEFF_4800,       ref_V27_MTD_COEFF_4800,       10,
	  "V27_MTD_COEFF_4800"       },
	{ V27RX_DEC_LAST_PHASE_2400, ref_V27RX_DEC_LAST_PHASE_2400, 4,
	  "V27RX_DEC_LAST_PHASE_2400" },
	{ V27RX_DEC_LAST_PHASE_4800, ref_V27RX_DEC_LAST_PHASE_4800, 8,
	  "V27RX_DEC_LAST_PHASE_4800" },
	{ V27RX_DEC_PMAP_2400,      ref_V27RX_DEC_PMAP_2400,       4,
	  "V27RX_DEC_PMAP_2400"      },
	{ V27RX_DEC_PMAP_4800,      ref_V27RX_DEC_PMAP_4800,       8,
	  "V27RX_DEC_PMAP_4800"      },
	{ V27RX_DEC_PHS_MASK,       ref_V27RX_DEC_PHS_MASK,        2,
	  "V27RX_DEC_PHS_MASK"       },
	{ V27RX_FSE_PLLK2_2400,     ref_V27RX_FSE_PLLK2_2400,      3,
	  "V27RX_FSE_PLLK2_2400"     },
	{ V27RX_FSE_PLLK1_2400,     ref_V27RX_FSE_PLLK1_2400,      3,
	  "V27RX_FSE_PLLK1_2400"     },
	{ V27RX_FSE_PLLK2_4800,     ref_V27RX_FSE_PLLK2_4800,      3,
	  "V27RX_FSE_PLLK2_4800"     },
	{ V27RX_FSE_PLLK1_4800,     ref_V27RX_FSE_PLLK1_4800,      3,
	  "V27RX_FSE_PLLK1_4800"     },
	{ V27RX_CRR_ADJUST,         ref_V27RX_CRR_ADJUST,          2,
	  "V27RX_CRR_ADJUST"         },
	{ V27RX_CRR_TABLE_2400,     ref_V27RX_CRR_TABLE_2400,      4,
	  "V27RX_CRR_TABLE_2400"     },
	{ V27RX_CRR_TABLE_4800,     ref_V27RX_CRR_TABLE_4800,     40,
	  "V27RX_CRR_TABLE_4800"     },
	{ V27RX_CRR_TABLE_LEN,      ref_V27RX_CRR_TABLE_LEN,       2,
	  "V27RX_CRR_TABLE_LEN"      },
	{ V27RX_FSE_MU_TRACK,       ref_V27RX_FSE_MU_TRACK,        2,
	  "V27RX_FSE_MU_TRACK"       },
	{ V27RX_FSE_MU_TRAIN,       ref_V27RX_FSE_MU_TRAIN,        2,
	  "V27RX_FSE_MU_TRAIN"       },
	{ V27RX_SRE_PLLK2_2400,     ref_V27RX_SRE_PLLK2_2400,      3,
	  "V27RX_SRE_PLLK2_2400"     },
	{ V27RX_SRE_PLLK1_2400,     ref_V27RX_SRE_PLLK1_2400,      3,
	  "V27RX_SRE_PLLK1_2400"     },
	{ V27RX_SRE_PLLK2_4800,     ref_V27RX_SRE_PLLK2_4800,      3,
	  "V27RX_SRE_PLLK2_4800"     },
	{ V27RX_SRE_PLLK1_4800,     ref_V27RX_SRE_PLLK1_4800,      3,
	  "V27RX_SRE_PLLK1_4800"     },
	{ V27RX_YCLOCK_2400,        ref_V27RX_YCLOCK_2400,         6,
	  "V27RX_YCLOCK_2400"        },
	{ V27RX_YCLOCK_4800,        ref_V27RX_YCLOCK_4800,         5,
	  "V27RX_YCLOCK_4800"        },
	{ V27RX_XCLOCK_2400,        ref_V27RX_XCLOCK_2400,         6,
	  "V27RX_XCLOCK_2400"        },
	{ V27RX_XCLOCK_4800,        ref_V27RX_XCLOCK_4800,         5,
	  "V27RX_XCLOCK_4800"        }
};

#define NTABLES ((int)(sizeof(TABLES) / sizeof(TABLES[0])))

/* ------------------------------------------------------------------------- */

static int
test_shape(void)
{
	int i;

	diff_begin("v27cfg: sizes against the object's symbol table");

	/*
	 * The `nm -S` size of every one of the fifty-six symbols.  A table
	 * whose C array is shorter than the object's symbol compares equal
	 * over its own length and is still wrong.
	 */
	diff_eq_int("sizeof AGCv27_CFG (%ld)", (long)sizeof(AGCv27_CFG), 24, 0);

	diff_eq_int("sizeof V27RX_FSE_FILT_LEN (%ld)",
		    (long)sizeof(V27RX_FSE_FILT_LEN), 4, 0);
	diff_eq_int("sizeof V27RX_FSE_QFILT_4800 (%ld)",
		    (long)sizeof(V27RX_FSE_QFILT_4800), 162, 0);
	diff_eq_int("sizeof V27RX_FSE_QFILT_2400 (%ld)",
		    (long)sizeof(V27RX_FSE_QFILT_2400), 194, 0);
	diff_eq_int("sizeof V27RX_FSE_QFILT (%ld)",
		    (long)sizeof(V27RX_FSE_QFILT), 8, 0);
	diff_eq_int("sizeof V27RX_FSE_IFILT_4800 (%ld)",
		    (long)sizeof(V27RX_FSE_IFILT_4800), 162, 0);
	diff_eq_int("sizeof V27RX_FSE_IFILT_2400 (%ld)",
		    (long)sizeof(V27RX_FSE_IFILT_2400), 194, 0);
	diff_eq_int("sizeof V27RX_FSE_IFILT (%ld)",
		    (long)sizeof(V27RX_FSE_IFILT), 8, 0);
	diff_eq_int("sizeof V27RX_SAMP_PER_BAUD (%ld)",
		    (long)sizeof(V27RX_SAMP_PER_BAUD), 4, 0);
	diff_eq_int("sizeof V27RX_XB_COFFS_2400 (%ld)",
		    (long)sizeof(V27RX_XB_COFFS_2400), 22, 0);
	diff_eq_int("sizeof V27RX_XB_COFFS_4800 (%ld)",
		    (long)sizeof(V27RX_XB_COFFS_4800), 22, 0);
	diff_eq_int("sizeof V27RX_XB_COFFS (%ld)",
		    (long)sizeof(V27RX_XB_COFFS), 8, 0);
	diff_eq_int("sizeof V27RX_SRE_FILT_2400 (%ld)",
		    (long)sizeof(V27RX_SRE_FILT_2400), 542, 0);
	diff_eq_int("sizeof V27RX_SRE_FILT_4800 (%ld)",
		    (long)sizeof(V27RX_SRE_FILT_4800), 402, 0);
	diff_eq_int("sizeof V27RX_SRE_FILT_LEN (%ld)",
		    (long)sizeof(V27RX_SRE_FILT_LEN), 4, 0);
	diff_eq_int("sizeof V27RX_MRF_FILT_2400 (%ld)",
		    (long)sizeof(V27RX_MRF_FILT_2400), 540, 0);
	diff_eq_int("sizeof V27RX_MRF_FILT_4800 (%ld)",
		    (long)sizeof(V27RX_MRF_FILT_4800), 72, 0);
	diff_eq_int("sizeof V27RX_MRF_FILT_LEN (%ld)",
		    (long)sizeof(V27RX_MRF_FILT_LEN), 4, 0);
	diff_eq_int("sizeof V27RX_MRF_DOWN (%ld)",
		    (long)sizeof(V27RX_MRF_DOWN), 4, 0);
	diff_eq_int("sizeof V27RX_MRF_UP (%ld)",
		    (long)sizeof(V27RX_MRF_UP), 4, 0);

	diff_eq_int("sizeof V27_MTD_COEFF_2400 (%ld)",
		    (long)sizeof(V27_MTD_COEFF_2400), 20, 0);
	diff_eq_int("sizeof V27_MTD_COEFF_4800 (%ld)",
		    (long)sizeof(V27_MTD_COEFF_4800), 20, 0);
	diff_eq_int("sizeof V27RX_DEC_LAST_PHASE_2400 (%ld)",
		    (long)sizeof(V27RX_DEC_LAST_PHASE_2400), 8, 0);
	diff_eq_int("sizeof V27RX_DEC_LAST_PHASE_4800 (%ld)",
		    (long)sizeof(V27RX_DEC_LAST_PHASE_4800), 16, 0);
	diff_eq_int("sizeof V27RX_DEC_LAST_PHASE (%ld)",
		    (long)sizeof(V27RX_DEC_LAST_PHASE), 8, 0);
	diff_eq_int("sizeof V27RX_DEC_PMAP_2400 (%ld)",
		    (long)sizeof(V27RX_DEC_PMAP_2400), 8, 0);
	diff_eq_int("sizeof V27RX_DEC_PMAP_4800 (%ld)",
		    (long)sizeof(V27RX_DEC_PMAP_4800), 16, 0);
	diff_eq_int("sizeof V27RX_DEC_PMAP (%ld)",
		    (long)sizeof(V27RX_DEC_PMAP), 8, 0);
	diff_eq_int("sizeof V27RX_DEC_PHS_MASK (%ld)",
		    (long)sizeof(V27RX_DEC_PHS_MASK), 4, 0);
	diff_eq_int("sizeof V27RX_FSE_PLLK2_2400 (%ld)",
		    (long)sizeof(V27RX_FSE_PLLK2_2400), 6, 0);
	diff_eq_int("sizeof V27RX_FSE_PLLK1_2400 (%ld)",
		    (long)sizeof(V27RX_FSE_PLLK1_2400), 6, 0);
	diff_eq_int("sizeof V27RX_FSE_PLLK2_4800 (%ld)",
		    (long)sizeof(V27RX_FSE_PLLK2_4800), 6, 0);
	diff_eq_int("sizeof V27RX_FSE_PLLK1_4800 (%ld)",
		    (long)sizeof(V27RX_FSE_PLLK1_4800), 6, 0);
	diff_eq_int("sizeof V27RX_FSE_PLLK2 (%ld)",
		    (long)sizeof(V27RX_FSE_PLLK2), 8, 0);
	diff_eq_int("sizeof V27RX_FSE_PLLK1 (%ld)",
		    (long)sizeof(V27RX_FSE_PLLK1), 8, 0);
	diff_eq_int("sizeof V27RX_CRR_ADJUST (%ld)",
		    (long)sizeof(V27RX_CRR_ADJUST), 4, 0);
	diff_eq_int("sizeof V27RX_CRR_TABLE_2400 (%ld)",
		    (long)sizeof(V27RX_CRR_TABLE_2400), 8, 0);
	diff_eq_int("sizeof V27RX_CRR_TABLE_4800 (%ld)",
		    (long)sizeof(V27RX_CRR_TABLE_4800), 80, 0);
	diff_eq_int("sizeof V27RX_CRR_TABLE_LEN (%ld)",
		    (long)sizeof(V27RX_CRR_TABLE_LEN), 4, 0);
	diff_eq_int("sizeof V27RX_CRR_TABLE (%ld)",
		    (long)sizeof(V27RX_CRR_TABLE), 8, 0);
	diff_eq_int("sizeof V27RX_FSE_MU_TRACK (%ld)",
		    (long)sizeof(V27RX_FSE_MU_TRACK), 4, 0);
	diff_eq_int("sizeof V27RX_FSE_MU_TRAIN (%ld)",
		    (long)sizeof(V27RX_FSE_MU_TRAIN), 4, 0);
	diff_eq_int("sizeof V27RX_SRE_PLLK2_2400 (%ld)",
		    (long)sizeof(V27RX_SRE_PLLK2_2400), 6, 0);
	diff_eq_int("sizeof V27RX_SRE_PLLK1_2400 (%ld)",
		    (long)sizeof(V27RX_SRE_PLLK1_2400), 6, 0);
	diff_eq_int("sizeof V27RX_SRE_PLLK2_4800 (%ld)",
		    (long)sizeof(V27RX_SRE_PLLK2_4800), 6, 0);
	diff_eq_int("sizeof V27RX_SRE_PLLK1_4800 (%ld)",
		    (long)sizeof(V27RX_SRE_PLLK1_4800), 6, 0);
	diff_eq_int("sizeof V27RX_SRE_PLLK2 (%ld)",
		    (long)sizeof(V27RX_SRE_PLLK2), 8, 0);
	diff_eq_int("sizeof V27RX_SRE_PLLK1 (%ld)",
		    (long)sizeof(V27RX_SRE_PLLK1), 8, 0);
	diff_eq_int("sizeof V27RX_YCLOCK_2400 (%ld)",
		    (long)sizeof(V27RX_YCLOCK_2400), 12, 0);
	diff_eq_int("sizeof V27RX_YCLOCK_4800 (%ld)",
		    (long)sizeof(V27RX_YCLOCK_4800), 10, 0);
	diff_eq_int("sizeof V27RX_YCLOCK (%ld)",
		    (long)sizeof(V27RX_YCLOCK), 8, 0);
	diff_eq_int("sizeof V27RX_XCLOCK_2400 (%ld)",
		    (long)sizeof(V27RX_XCLOCK_2400), 12, 0);
	diff_eq_int("sizeof V27RX_XCLOCK_4800 (%ld)",
		    (long)sizeof(V27RX_XCLOCK_4800), 10, 0);
	diff_eq_int("sizeof V27RX_XCLOCK (%ld)",
		    (long)sizeof(V27RX_XCLOCK), 8, 0);
	diff_eq_int("sizeof V27RX_SRE_FILT (%ld)",
		    (long)sizeof(V27RX_SRE_FILT), 8, 0);
	diff_eq_int("sizeof V27RX_MRF_FILT (%ld)",
		    (long)sizeof(V27RX_MRF_FILT), 8, 0);

	/*
	 * THE RELATIONSHIPS THE CONSTRUCTOR DEPENDS ON, stated as arithmetic
	 * over the LENGTH TABLES rather than as literals.  This is the second
	 * of the two independent readings: `st_size` above, and the count
	 * `V27RX_create` writes into the DSP block, here.
	 */
	diff_eq_int("FSE_IFILT_2400 holds FSE_FILT_LEN[0] taps (%ld)",
		    (long)(sizeof(V27RX_FSE_IFILT_2400) / sizeof(short)),
		    V27RX_FSE_FILT_LEN[0], 0);
	diff_eq_int("FSE_QFILT_2400 holds FSE_FILT_LEN[0] taps (%ld)",
		    (long)(sizeof(V27RX_FSE_QFILT_2400) / sizeof(short)),
		    V27RX_FSE_FILT_LEN[0], 0);
	diff_eq_int("FSE_IFILT_4800 holds FSE_FILT_LEN[1] taps (%ld)",
		    (long)(sizeof(V27RX_FSE_IFILT_4800) / sizeof(short)),
		    V27RX_FSE_FILT_LEN[1], 1);
	diff_eq_int("FSE_QFILT_4800 holds FSE_FILT_LEN[1] taps (%ld)",
		    (long)(sizeof(V27RX_FSE_QFILT_4800) / sizeof(short)),
		    V27RX_FSE_FILT_LEN[1], 1);
	diff_eq_int("SRE_FILT_2400 holds coeffs+1 entries (%ld)",
		    (long)(sizeof(V27RX_SRE_FILT_2400) / sizeof(short)),
		    V27RX_SRE_FILT_LEN[0] + 1, 0);
	diff_eq_int("SRE_FILT_4800 holds coeffs+1 entries (%ld)",
		    (long)(sizeof(V27RX_SRE_FILT_4800) / sizeof(short)),
		    V27RX_SRE_FILT_LEN[1] + 1, 1);
	diff_eq_int("MRF_FILT_2400 holds MRF_FILT_LEN[0] taps (%ld)",
		    (long)(sizeof(V27RX_MRF_FILT_2400) / sizeof(short)),
		    V27RX_MRF_FILT_LEN[0], 0);
	diff_eq_int("MRF_FILT_4800 holds MRF_FILT_LEN[1] taps (%ld)",
		    (long)(sizeof(V27RX_MRF_FILT_4800) / sizeof(short)),
		    V27RX_MRF_FILT_LEN[1], 1);
	diff_eq_int("CRR_TABLE_2400 holds CRR_TABLE_LEN[0] entries (%ld)",
		    (long)(sizeof(V27RX_CRR_TABLE_2400) / sizeof(short)),
		    V27RX_CRR_TABLE_LEN[0], 0);
	diff_eq_int("CRR_TABLE_4800 holds CRR_TABLE_LEN[1] entries (%ld)",
		    (long)(sizeof(V27RX_CRR_TABLE_4800) / sizeof(short)),
		    V27RX_CRR_TABLE_LEN[1], 1);
	diff_eq_int("XCLOCK_2400 holds SAMP_PER_BAUD[0] entries (%ld)",
		    (long)(sizeof(V27RX_XCLOCK_2400) / sizeof(short)),
		    V27RX_SAMP_PER_BAUD[0], 0);
	diff_eq_int("YCLOCK_2400 holds SAMP_PER_BAUD[0] entries (%ld)",
		    (long)(sizeof(V27RX_YCLOCK_2400) / sizeof(short)),
		    V27RX_SAMP_PER_BAUD[0], 0);
	diff_eq_int("XCLOCK_4800 holds SAMP_PER_BAUD[1] entries (%ld)",
		    (long)(sizeof(V27RX_XCLOCK_4800) / sizeof(short)),
		    V27RX_SAMP_PER_BAUD[1], 1);
	diff_eq_int("YCLOCK_4800 holds SAMP_PER_BAUD[1] entries (%ld)",
		    (long)(sizeof(V27RX_YCLOCK_4800) / sizeof(short)),
		    V27RX_SAMP_PER_BAUD[1], 1);
	diff_eq_int("XB_COFFS_2400 holds FPM_SRE_DISC entries (%ld)",
		    (long)(sizeof(V27RX_XB_COFFS_2400) / sizeof(short)),
		    FPM_SRE_DISC, 0);
	diff_eq_int("XB_COFFS_4800 holds FPM_SRE_DISC entries (%ld)",
		    (long)(sizeof(V27RX_XB_COFFS_4800) / sizeof(short)),
		    FPM_SRE_DISC, 1);
	diff_eq_int("SRE_PLLK1_2400 holds FPM_SRE_MODES entries (%ld)",
		    (long)(sizeof(V27RX_SRE_PLLK1_2400) / sizeof(short)),
		    FPM_SRE_MODES, 0);
	diff_eq_int("SRE_PLLK1_4800 holds FPM_SRE_MODES entries (%ld)",
		    (long)(sizeof(V27RX_SRE_PLLK1_4800) / sizeof(short)),
		    FPM_SRE_MODES, 1);
	diff_eq_int("DEC_PMAP_2400 holds PHS_MASK[0]+1 entries (%ld)",
		    (long)(sizeof(V27RX_DEC_PMAP_2400) / sizeof(short)),
		    V27RX_DEC_PHS_MASK[0] + 1, 0);
	diff_eq_int("DEC_PMAP_4800 holds PHS_MASK[1]+1 entries (%ld)",
		    (long)(sizeof(V27RX_DEC_PMAP_4800) / sizeof(short)),
		    V27RX_DEC_PHS_MASK[1] + 1, 1);
	diff_eq_int("DEC_LAST_PHASE_2400 holds PHS_MASK[0]+1 entries (%ld)",
		    (long)(sizeof(V27RX_DEC_LAST_PHASE_2400) / sizeof(short)),
		    V27RX_DEC_PHS_MASK[0] + 1, 0);
	diff_eq_int("DEC_LAST_PHASE_4800 holds PHS_MASK[1]+1 entries (%ld)",
		    (long)(sizeof(V27RX_DEC_LAST_PHASE_4800) / sizeof(short)),
		    V27RX_DEC_PHS_MASK[1] + 1, 1);
	diff_eq_int("MRF_FILT_2400 is 9 branches x 30 taps (%ld)",
		    (long)(sizeof(V27RX_MRF_FILT_2400) / sizeof(short)),
		    V27RX_MRF_UP[0] * 30, 0);
	diff_eq_int("SRE_FILT_LEN is FPM_SRE_BRANCHES x taps, 2400 (%ld)",
		    V27RX_SRE_FILT_LEN[0] % FPM_SRE_BRANCHES, 0, 0);
	diff_eq_int("SRE_FILT_LEN is FPM_SRE_BRANCHES x taps, 4800 (%ld)",
		    V27RX_SRE_FILT_LEN[1] % FPM_SRE_BRANCHES, 0, 1);

	/* The register the other layers walk is the whole set, not a subset. */
	diff_eq_int("the table register covers 41 arrays (%ld)", NTABLES, 41, 0);
	for (i = 0; i < NTABLES; i++)
		diff_eq_int("register entry is non-empty (%ld)",
			    TABLES[i].n > 0 && TABLES[i].ours != 0 &&
			    TABLES[i].blob != 0, 1, i);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

static int
test_values(void)
{
	int i;

	diff_begin("v27cfg: table values against the blob");

	for (i = 0; i < NTABLES; i++)
		cmp_shorts(TABLES[i].name, TABLES[i].ours, TABLES[i].blob,
			   TABLES[i].n);

	/*
	 * THE SELECTORS, and this is the check no byte comparison contains.
	 * Ours are checked by POINTER IDENTITY -- entry 0 is the 2400 table and
	 * entry 1 the 4800 one -- and the blob's by CONTENT, because its
	 * addresses are its own.  A selector with the two the wrong way round
	 * has perfectly correct bytes everywhere else.
	 */
	diff_eq_int("V27RX_FSE_QFILT wiring (%ld)",
		    V27RX_FSE_QFILT[0] == V27RX_FSE_QFILT_2400 &&
		    V27RX_FSE_QFILT[1] == V27RX_FSE_QFILT_4800, 1, 0);
	diff_eq_int("V27RX_FSE_IFILT wiring (%ld)",
		    V27RX_FSE_IFILT[0] == V27RX_FSE_IFILT_2400 &&
		    V27RX_FSE_IFILT[1] == V27RX_FSE_IFILT_4800, 1, 0);
	diff_eq_int("V27RX_XB_COFFS wiring (%ld)",
		    V27RX_XB_COFFS[0] == V27RX_XB_COFFS_2400 &&
		    V27RX_XB_COFFS[1] == V27RX_XB_COFFS_4800, 1, 0);
	diff_eq_int("V27RX_SRE_FILT wiring (%ld)",
		    V27RX_SRE_FILT[0] == V27RX_SRE_FILT_2400 &&
		    V27RX_SRE_FILT[1] == V27RX_SRE_FILT_4800, 1, 0);
	diff_eq_int("V27RX_MRF_FILT wiring (%ld)",
		    V27RX_MRF_FILT[0] == V27RX_MRF_FILT_2400 &&
		    V27RX_MRF_FILT[1] == V27RX_MRF_FILT_4800, 1, 0);
	diff_eq_int("V27RX_DEC_LAST_PHASE wiring (%ld)",
		    V27RX_DEC_LAST_PHASE[0] == V27RX_DEC_LAST_PHASE_2400 &&
		    V27RX_DEC_LAST_PHASE[1] == V27RX_DEC_LAST_PHASE_4800, 1, 0);
	diff_eq_int("V27RX_DEC_PMAP wiring (%ld)",
		    V27RX_DEC_PMAP[0] == V27RX_DEC_PMAP_2400 &&
		    V27RX_DEC_PMAP[1] == V27RX_DEC_PMAP_4800, 1, 0);
	diff_eq_int("V27RX_FSE_PLLK1 wiring (%ld)",
		    V27RX_FSE_PLLK1[0] == V27RX_FSE_PLLK1_2400 &&
		    V27RX_FSE_PLLK1[1] == V27RX_FSE_PLLK1_4800, 1, 0);
	diff_eq_int("V27RX_FSE_PLLK2 wiring (%ld)",
		    V27RX_FSE_PLLK2[0] == V27RX_FSE_PLLK2_2400 &&
		    V27RX_FSE_PLLK2[1] == V27RX_FSE_PLLK2_4800, 1, 0);
	diff_eq_int("V27RX_SRE_PLLK1 wiring (%ld)",
		    V27RX_SRE_PLLK1[0] == V27RX_SRE_PLLK1_2400 &&
		    V27RX_SRE_PLLK1[1] == V27RX_SRE_PLLK1_4800, 1, 0);
	diff_eq_int("V27RX_SRE_PLLK2 wiring (%ld)",
		    V27RX_SRE_PLLK2[0] == V27RX_SRE_PLLK2_2400 &&
		    V27RX_SRE_PLLK2[1] == V27RX_SRE_PLLK2_4800, 1, 0);
	diff_eq_int("V27RX_XCLOCK wiring (%ld)",
		    V27RX_XCLOCK[0] == V27RX_XCLOCK_2400 &&
		    V27RX_XCLOCK[1] == V27RX_XCLOCK_4800, 1, 0);
	diff_eq_int("V27RX_YCLOCK wiring (%ld)",
		    V27RX_YCLOCK[0] == V27RX_YCLOCK_2400 &&
		    V27RX_YCLOCK[1] == V27RX_YCLOCK_4800, 1, 0);
	diff_eq_int("V27RX_CRR_TABLE wiring (%ld)",
		    V27RX_CRR_TABLE[0] == V27RX_CRR_TABLE_2400 &&
		    V27RX_CRR_TABLE[1] == V27RX_CRR_TABLE_4800, 1, 0);

	cmp_shorts("ref FSE_QFILT[0] reaches _2400 (%ld)",
		   ref_V27RX_FSE_QFILT[0], V27RX_FSE_QFILT_2400, 97);
	cmp_shorts("ref FSE_QFILT[1] reaches _4800 (%ld)",
		   ref_V27RX_FSE_QFILT[1], V27RX_FSE_QFILT_4800, 81);
	cmp_shorts("ref FSE_IFILT[0] reaches _2400 (%ld)",
		   ref_V27RX_FSE_IFILT[0], V27RX_FSE_IFILT_2400, 97);
	cmp_shorts("ref FSE_IFILT[1] reaches _4800 (%ld)",
		   ref_V27RX_FSE_IFILT[1], V27RX_FSE_IFILT_4800, 81);
	cmp_shorts("ref XB_COFFS[0] reaches _2400 (%ld)",
		   ref_V27RX_XB_COFFS[0], V27RX_XB_COFFS_2400, 11);
	cmp_shorts("ref XB_COFFS[1] reaches _4800 (%ld)",
		   ref_V27RX_XB_COFFS[1], V27RX_XB_COFFS_4800, 11);
	cmp_shorts("ref SRE_FILT[0] reaches _2400 (%ld)",
		   ref_V27RX_SRE_FILT[0], V27RX_SRE_FILT_2400, 271);
	cmp_shorts("ref SRE_FILT[1] reaches _4800 (%ld)",
		   ref_V27RX_SRE_FILT[1], V27RX_SRE_FILT_4800, 201);
	cmp_shorts("ref MRF_FILT[0] reaches _2400 (%ld)",
		   ref_V27RX_MRF_FILT[0], V27RX_MRF_FILT_2400, 270);
	cmp_shorts("ref MRF_FILT[1] reaches _4800 (%ld)",
		   ref_V27RX_MRF_FILT[1], V27RX_MRF_FILT_4800, 36);
	cmp_shorts("ref DEC_LAST_PHASE[0] reaches _2400 (%ld)",
		   ref_V27RX_DEC_LAST_PHASE[0], V27RX_DEC_LAST_PHASE_2400, 4);
	cmp_shorts("ref DEC_LAST_PHASE[1] reaches _4800 (%ld)",
		   ref_V27RX_DEC_LAST_PHASE[1], V27RX_DEC_LAST_PHASE_4800, 8);
	cmp_shorts("ref DEC_PMAP[0] reaches _2400 (%ld)",
		   ref_V27RX_DEC_PMAP[0], V27RX_DEC_PMAP_2400, 4);
	cmp_shorts("ref DEC_PMAP[1] reaches _4800 (%ld)",
		   ref_V27RX_DEC_PMAP[1], V27RX_DEC_PMAP_4800, 8);
	cmp_shorts("ref FSE_PLLK1[0] reaches _2400 (%ld)",
		   ref_V27RX_FSE_PLLK1[0], V27RX_FSE_PLLK1_2400, 3);
	cmp_shorts("ref FSE_PLLK1[1] reaches _4800 (%ld)",
		   ref_V27RX_FSE_PLLK1[1], V27RX_FSE_PLLK1_4800, 3);
	cmp_shorts("ref FSE_PLLK2[0] reaches _2400 (%ld)",
		   ref_V27RX_FSE_PLLK2[0], V27RX_FSE_PLLK2_2400, 3);
	cmp_shorts("ref FSE_PLLK2[1] reaches _4800 (%ld)",
		   ref_V27RX_FSE_PLLK2[1], V27RX_FSE_PLLK2_4800, 3);
	cmp_shorts("ref SRE_PLLK1[0] reaches _2400 (%ld)",
		   ref_V27RX_SRE_PLLK1[0], V27RX_SRE_PLLK1_2400, 3);
	cmp_shorts("ref SRE_PLLK1[1] reaches _4800 (%ld)",
		   ref_V27RX_SRE_PLLK1[1], V27RX_SRE_PLLK1_4800, 3);
	cmp_shorts("ref SRE_PLLK2[0] reaches _2400 (%ld)",
		   ref_V27RX_SRE_PLLK2[0], V27RX_SRE_PLLK2_2400, 3);
	cmp_shorts("ref SRE_PLLK2[1] reaches _4800 (%ld)",
		   ref_V27RX_SRE_PLLK2[1], V27RX_SRE_PLLK2_4800, 3);
	cmp_shorts("ref XCLOCK[0] reaches _2400 (%ld)",
		   ref_V27RX_XCLOCK[0], V27RX_XCLOCK_2400, 6);
	cmp_shorts("ref XCLOCK[1] reaches _4800 (%ld)",
		   ref_V27RX_XCLOCK[1], V27RX_XCLOCK_4800, 5);
	cmp_shorts("ref YCLOCK[0] reaches _2400 (%ld)",
		   ref_V27RX_YCLOCK[0], V27RX_YCLOCK_2400, 6);
	cmp_shorts("ref YCLOCK[1] reaches _4800 (%ld)",
		   ref_V27RX_YCLOCK[1], V27RX_YCLOCK_4800, 5);
	cmp_shorts("ref CRR_TABLE[0] reaches _2400 (%ld)",
		   ref_V27RX_CRR_TABLE[0], V27RX_CRR_TABLE_2400, 4);
	cmp_shorts("ref CRR_TABLE[1] reaches _4800 (%ld)",
		   ref_V27RX_CRR_TABLE[1], V27RX_CRR_TABLE_4800, 40);

	/* AGCv27_CFG, field by field; the two pointers by their contents. */
	diff_eq_int("AGCv27_CFG.ref_level (%ld)", AGCv27_CFG.ref_level,
		    ref_AGCv27_CFG.ref_level, 0x00);
	diff_eq_int("AGCv27_CFG.acquire_level (%ld)", AGCv27_CFG.acquire_level,
		    ref_AGCv27_CFG.acquire_level, 0x02);
	diff_eq_int("AGCv27_CFG.squelch_level (%ld)", AGCv27_CFG.squelch_level,
		    ref_AGCv27_CFG.squelch_level, 0x04);
	diff_eq_int("AGCv27_CFG.f06 (%ld)", AGCv27_CFG.f06,
		    ref_AGCv27_CFG.f06, 0x06);
	diff_eq_int("AGCv27_CFG.f08 (%ld)", AGCv27_CFG.f08,
		    ref_AGCv27_CFG.f08, 0x08);
	diff_eq_int("AGCv27_CFG.block_len (%ld)", AGCv27_CFG.block_len,
		    ref_AGCv27_CFG.block_len, 0x0a);
	diff_eq_int("AGCv27_CFG.f14 (%ld)", AGCv27_CFG.f14,
		    ref_AGCv27_CFG.f14, 0x14);
	diff_eq_int("AGCv27_CFG.f16 (%ld)", AGCv27_CFG.f16,
		    ref_AGCv27_CFG.f16, 0x16);
	cmp_shorts("AGCv27_CFG.alpha[%ld]", AGCv27_CFG.alpha,
		   ref_AGCv27_CFG.alpha, 2);
	cmp_shorts("AGCv27_CFG.beta[%ld]", AGCv27_CFG.beta,
		   ref_AGCv27_CFG.beta, 2);

	/*
	 * ANTI-VACUITY.  Two of these rails are half zero by construction, so
	 * assert that the comparison saw real data.
	 */
	diff_eq_int("MRF_FILT_2400 has non-zero entries (%ld)",
		    nonzero(V27RX_MRF_FILT_2400, 270), 270, 0);
	diff_eq_int("SRE_FILT_2400 has non-zero entries (%ld)",
		    nonzero(V27RX_SRE_FILT_2400, 271), 271, 0);
	diff_eq_int("FSE_IFILT_2400 non-zero entries (%ld)",
		    nonzero(V27RX_FSE_IFILT_2400, 97), 49, 0);
	diff_eq_int("FSE_QFILT_2400 non-zero entries (%ld)",
		    nonzero(V27RX_FSE_QFILT_2400, 97), 48, 0);
	diff_eq_int("FSE_IFILT_4800 non-zero entries (%ld)",
		    nonzero(V27RX_FSE_IFILT_4800, 81), 77, 1);
	diff_eq_int("FSE_QFILT_4800 non-zero entries (%ld)",
		    nonzero(V27RX_FSE_QFILT_4800, 81), 76, 1);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * Layer 3.  `cmp_shorts` and `shorts_differ` are the same loop; perturbing one
 * element of each table and requiring the detector to notice is what separates
 * "the tables agree" from "the comparison is dead".
 */
static int
test_detector_fires(void)
{
	static short copy[280];
	int i;

	diff_begin("v27cfg: the comparison rejects a perturbed table");

	for (i = 0; i < NTABLES; i++) {
		int j;

		memcpy(copy, TABLES[i].ours,
		       (size_t)TABLES[i].n * sizeof(short));
		diff_eq_int("identical copy compares equal (%ld)",
			    shorts_differ(copy, TABLES[i].ours, TABLES[i].n),
			    0, i);

		/* Every element in turn, so a detector that only looks at the
		 * first or the last one is caught too. */
		for (j = 0; j < TABLES[i].n; j++) {
			copy[j] = (short)(copy[j] + 1);
			diff_eq_int("one-element perturbation is seen (%ld)",
				    shorts_differ(copy, TABLES[i].ours,
						  TABLES[i].n), 1, j);
			copy[j] = (short)(copy[j] - 1);
		}
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/* Is `v` symmetric about its centre over `n` entries? */
static int
is_symmetric(const short *v, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (v[i] != v[n - 1 - i])
			return 0;
	return 1;
}

static int
is_antisymmetric(const short *v, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (v[i] != -v[n - 1 - i])
			return 0;
	return 1;
}

/*
 * Layer 4.  Facts about the values that hold independently of the blob, and
 * that a transcription error would break even if it were transcribed into both
 * sides -- which cannot happen here, but which is exactly what a generator
 * written from these bytes later WOULD risk.
 */
static int
test_value_shape(void)
{
	int i;

	diff_begin("v27cfg: the shape of the values");

	/*
	 * THE CARRIER, AND IT IS THE SAME AT BOTH RATES.  The resampler in
	 * front of the equaliser runs at 8000 * up/down and the ramp steps by
	 * `clk_inc` in `clk_mod`, so the carrier is 1800 Hz twice over --
	 * V.27ter's own number, computed here out of five tables that were
	 * read separately.
	 */
	diff_eq_int("2400: carrier is 1800 Hz (%ld)",
		    8000 * V27RX_MRF_UP[0] / V27RX_MRF_DOWN[0] *
		    V27RX_CRR_ADJUST[0] / V27RX_CRR_TABLE_LEN[0], 1800, 0);
	diff_eq_int("4800: carrier is 1800 Hz (%ld)",
		    8000 * V27RX_MRF_UP[1] / V27RX_MRF_DOWN[1] *
		    V27RX_CRR_ADJUST[1] / V27RX_CRR_TABLE_LEN[1], 1800, 1);

	/*
	 * And the symbol rates, from the same resampler ratios and
	 * `SAMP_PER_BAUD`: 1200 baud at four phases is 2400 bit/s, 1600 baud
	 * at eight phases is 4800 bit/s.
	 */
	diff_eq_int("2400: baud rate (%ld)",
		    8000 * V27RX_MRF_UP[0] / V27RX_MRF_DOWN[0] /
		    V27RX_SAMP_PER_BAUD[0], 1200, 0);
	diff_eq_int("4800: baud rate (%ld)",
		    8000 * V27RX_MRF_UP[1] / V27RX_MRF_DOWN[1] /
		    V27RX_SAMP_PER_BAUD[1], 1600, 1);
	diff_eq_int("2400: 4 phases, 2 bits, 2400 bit/s (%ld)",
		    1200 * 2, 2400, 0);
	diff_eq_int("4800: 8 phases, 3 bits, 4800 bit/s (%ld)",
		    1600 * 3, 4800, 1);
	diff_eq_int("PHS_MASK[0]+1 is 4 phases (%ld)",
		    V27RX_DEC_PHS_MASK[0] + 1, 4, 0);
	diff_eq_int("PHS_MASK[1]+1 is 8 phases (%ld)",
		    V27RX_DEC_PHS_MASK[1] + 1, 8, 1);

	/*
	 * The carrier ramps, in closed form: `round(i * 32768 / n)` computed
	 * in integers as `(i * 65536 + n) / (2 * n)`.  This is what fixes the
	 * element type -- a reading that made these anything but shorts of
	 * Q16 phase would not produce the 1800 Hz above.
	 */
	for (i = 0; i < 4; i++)
		diff_eq_int("CRR_TABLE_2400[%ld] is round(i*32768/4)",
			    V27RX_CRR_TABLE_2400[i], (i * 65536 + 4) / 8, i);
	for (i = 0; i < 40; i++)
		diff_eq_int("CRR_TABLE_4800[%ld] is round(i*32768/40)",
			    V27RX_CRR_TABLE_4800[i], (i * 65536 + 40) / 80, i);

	/* The decoder's angles are the same ramp over the constellation. */
	for (i = 0; i < 4; i++)
		diff_eq_int("DEC_LAST_PHASE_2400[%ld] is i*0x8000/4",
			    V27RX_DEC_LAST_PHASE_2400[i], i * 8192, i);
	for (i = 0; i < 8; i++)
		diff_eq_int("DEC_LAST_PHASE_4800[%ld] is i*0x8000/8",
			    V27RX_DEC_LAST_PHASE_4800[i], i * 4096, i);

	/* The four prototypes are linear phase: symmetric about centre. */
	diff_eq_int("MRF_FILT_2400 is symmetric (%ld)",
		    is_symmetric(V27RX_MRF_FILT_2400, 270), 1, 0);
	diff_eq_int("MRF_FILT_4800 is symmetric (%ld)",
		    is_symmetric(V27RX_MRF_FILT_4800, 36), 1, 1);
	diff_eq_int("SRE_FILT_2400 is symmetric (%ld)",
		    is_symmetric(V27RX_SRE_FILT_2400, 271), 1, 0);
	diff_eq_int("SRE_FILT_4800 is symmetric (%ld)",
		    is_symmetric(V27RX_SRE_FILT_4800, 201), 1, 1);

	/* The equaliser's rails are the I and Q halves of one filter. */
	diff_eq_int("FSE_IFILT_2400 is symmetric (%ld)",
		    is_symmetric(V27RX_FSE_IFILT_2400, 97), 1, 0);
	diff_eq_int("FSE_QFILT_2400 is antisymmetric (%ld)",
		    is_antisymmetric(V27RX_FSE_QFILT_2400, 97), 1, 0);
	diff_eq_int("FSE_IFILT_4800 is symmetric (%ld)",
		    is_symmetric(V27RX_FSE_IFILT_4800, 81), 1, 1);
	diff_eq_int("FSE_QFILT_4800 is antisymmetric (%ld)",
		    is_antisymmetric(V27RX_FSE_QFILT_4800, 81), 1, 1);

	/*
	 * THE ZERO PATTERN, ASSERTED AS IF AND ONLY IF.  Each rail is a real
	 * prototype times a cosine or a sine at the carrier, so it is zero
	 * exactly where its trigonometric factor is.  1800/7200 is 1/4, so at
	 * 2400 bit/s cos is zero at odd taps and sin at even ones; 1800/8000
	 * is 9/40, so at 4800 the period is 20 taps and 9n is a quarter turn
	 * at n == 10 (mod 20) and a half turn at n == 0 (mod 20).
	 *
	 * Stating it as an equivalence rather than as "these taps are zero" is
	 * what makes it a claim: a rail with an EXTRA zero somewhere fails.
	 */
	for (i = 0; i < 97; i++) {
		diff_eq_int("FSE_IFILT_2400[%ld] zero iff i odd",
			    V27RX_FSE_IFILT_2400[i] == 0, i % 2 == 1, i);
		diff_eq_int("FSE_QFILT_2400[%ld] zero iff i even",
			    V27RX_FSE_QFILT_2400[i] == 0, i % 2 == 0, i);
	}
	for (i = 0; i < 81; i++) {
		diff_eq_int("FSE_IFILT_4800[%ld] zero iff i==10 mod 20",
			    V27RX_FSE_IFILT_4800[i] == 0, i % 20 == 10, i);
		diff_eq_int("FSE_QFILT_4800[%ld] zero iff i==0 mod 20",
			    V27RX_FSE_QFILT_4800[i] == 0, i % 20 == 0, i);
	}

	/* Both PLL gain arrays start with a zero integral term, both rates. */
	diff_eq_int("FSE_PLLK2_2400[0] is zero (%ld)",
		    V27RX_FSE_PLLK2_2400[0], 0, 0);
	diff_eq_int("FSE_PLLK2_4800[0] is zero (%ld)",
		    V27RX_FSE_PLLK2_4800[0], 0, 1);
	diff_eq_int("SRE_PLLK2_2400[0] is zero (%ld)",
		    V27RX_SRE_PLLK2_2400[0], 0, 0);
	diff_eq_int("SRE_PLLK2_4800[0] is zero (%ld)",
		    V27RX_SRE_PLLK2_4800[0], 0, 1);

	/*
	 * THE EQUALISER'S GAINS ARE RATE-INDEPENDENT AND THE TIMING LOOP'S ARE
	 * NOT.  Recorded because it is the kind of fact a reader would assume
	 * one way or the other: the object really does carry four separate
	 * symbols for the equaliser's two identical pairs.
	 */
	diff_eq_int("FSE PLL gains agree across rates (%ld)",
		    shorts_differ(V27RX_FSE_PLLK1_2400, V27RX_FSE_PLLK1_4800,
				  3) == 0 &&
		    shorts_differ(V27RX_FSE_PLLK2_2400, V27RX_FSE_PLLK2_4800,
				  3) == 0, 1, 0);
	diff_eq_int("SRE PLL gains differ across rates (%ld)",
		    shorts_differ(V27RX_SRE_PLLK1_2400, V27RX_SRE_PLLK1_4800,
				  3), 1, 0);

	/*
	 * The clock legs are `SAMP_PER_BAUD` phases equally spaced round one
	 * turn in Q14: X is the cosine leg and Y the sine leg.  Both legs sum
	 * to zero, which is what makes the correlation a discriminant rather
	 * than a level measurement, and X starts at 1.0.
	 */
	{
		int sx = 0, sy = 0;

		for (i = 0; i < 6; i++) {
			sx += V27RX_XCLOCK_2400[i];
			sy += V27RX_YCLOCK_2400[i];
		}
		diff_eq_int("XCLOCK_2400 sums to zero (%ld)", sx, 0, 0);
		diff_eq_int("YCLOCK_2400 sums to zero (%ld)", sy, 0, 0);
		sx = sy = 0;
		for (i = 0; i < 5; i++) {
			sx += V27RX_XCLOCK_4800[i];
			sy += V27RX_YCLOCK_4800[i];
		}
		/* Five points at 72 degrees do not cancel exactly in Q14:
		 * cos 72 rounds to 5063 and cos 144 to -13255, and
		 * 16384 + 2*5063 - 2*13255 is 0.  It does. */
		diff_eq_int("XCLOCK_4800 sums to zero (%ld)", sx, 0, 1);
		diff_eq_int("YCLOCK_4800 sums to zero (%ld)", sy, 0, 1);
	}
	diff_eq_int("XCLOCK_2400[0] is 1.0 in Q14 (%ld)",
		    V27RX_XCLOCK_2400[0], 16384, 0);
	diff_eq_int("XCLOCK_4800[0] is 1.0 in Q14 (%ld)",
		    V27RX_XCLOCK_4800[0], 16384, 1);
	diff_eq_int("YCLOCK_2400[0] is zero (%ld)", V27RX_YCLOCK_2400[0], 0, 0);
	diff_eq_int("YCLOCK_4800[0] is zero (%ld)", V27RX_YCLOCK_4800[0], 0, 1);

	/*
	 * The two tone banks are two five-short biquad sections each, and the
	 * sections share their first two coefficients -- the same shape V.29's
	 * and the V.21 channel-2 bank have.  And the two rates' banks are NOT
	 * the same bank, which a copy-paste would have made them.
	 */
	diff_eq_int("MTD_COEFF_2400 sections share coeff 0 (%ld)",
		    V27_MTD_COEFF_2400[0] == V27_MTD_COEFF_2400[5], 1, 0);
	diff_eq_int("MTD_COEFF_2400 sections share coeff 1 (%ld)",
		    V27_MTD_COEFF_2400[1] == V27_MTD_COEFF_2400[6], 1, 0);
	diff_eq_int("MTD_COEFF_4800 sections share coeff 0 (%ld)",
		    V27_MTD_COEFF_4800[0] == V27_MTD_COEFF_4800[5], 1, 1);
	diff_eq_int("MTD_COEFF_4800 sections share coeff 1 (%ld)",
		    V27_MTD_COEFF_4800[1] == V27_MTD_COEFF_4800[6], 1, 1);
	diff_eq_int("the two rate banks differ (%ld)",
		    shorts_differ(V27_MTD_COEFF_2400, V27_MTD_COEFF_4800, 10),
		    1, 0);
	diff_eq_int("neither is the V.21 bank (%ld)",
		    shorts_differ(V27_MTD_COEFF_2400,
				  ref_V21_CHAN2_MTD_COEFF, 10) &&
		    shorts_differ(V27_MTD_COEFF_4800,
				  ref_V21_CHAN2_MTD_COEFF, 10), 1, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * Layer 5.  `V27RX_create` builds each configuration by copying the library
 * built-in and patching it; these helpers do exactly that, once per side, so
 * that the two inits see the same construction over different tables.  The
 * literals are the ones the constructor stores; see `v27cfg.h`.
 */
static void
build_mrf_cfg(struct fpm_mrf_cfg *c, const short *coeff, int up, int down,
	      int taps)
{
	*c = FPM_MRF_CFG;
	c->branches = (short)up;
	c->decimate = (short)down;
	c->coeff = coeff;
	c->taps = (short)taps;
	c->aux = 0;
}

static void
build_sre_cfg(struct fpm_sre_cfg *c, const struct fpm_sre_cfg *base,
	      int clock_len, int coeffs, const short *proto, const short *disc,
	      const short *xclk, const short *yclk, const short *k1,
	      const short *k2)
{
	*c = *base;
	c->clock_len = (short)clock_len;
	c->groups_acq = 1;
	c->groups_trk = 8;
	c->settle = 0x40;
	c->coeffs = (short)coeffs;
	c->proto = proto;
	c->disc = disc;
	c->xclock = xclk;
	c->yclock = yclk;
	c->pll_k1 = k1;
	c->pll_k2 = k2;
	c->mag_hi = 2;
	c->mag_lo = 1;
	c->err_hi = 0x3333;
	c->err_lo = 0x199a;
	/* The object computes this from a live AGC field divided by six; it
	 * is not a table, so both sides get the same fixed value. */
	c->rms_min = 0;
	c->rms_len = (short)(3 * clock_len);
}

static void
build_fse_cfg(struct fpm_fse_cfg *c, const struct fpm_fse_cfg *base, int block,
	      int interp, const short *icoff, const short *qcoff, int taps,
	      int mu_train, int mu_track, const short *clk, int clk_mod,
	      int clk_inc, const short *k1, const short *k2)
{
	*c = *base;
	c->block = (short)block;
	c->interp = (short)interp;
	c->icoff = icoff;
	c->qcoff = qcoff;
	c->taps = (short)taps;
	c->mu[0] = (short)mu_train;
	c->mu[1] = (short)mu_track;
	c->clk = clk;
	c->clk_mod = (short)clk_mod;
	c->clk_inc = (short)clk_inc;
	c->train_sym = 0x3e8;
	c->err_hi = 0x2666;
	c->err_lo = 0x8f6;
	c->pll_k1 = k1;
	c->pll_k2 = k2;
	c->owner = 0;
	c->decision = 0;
	c->reserved34 = 0;
}

static int
test_use_agc(void)
{
	struct fpm_agc a, b;
	int reset;

	diff_begin("v27cfg: AGCv27_CFG through FPM_AGC_init");

	for (reset = 0; reset <= 1; reset++) {
		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));

		FPM_AGC_init(&a, &AGCv27_CFG, reset);
		ref_FPM_AGC_init(&b, &ref_AGCv27_CFG, reset);

		diff_eq_int("agc.cfg.ref_level (%ld)", a.cfg.ref_level,
			    b.cfg.ref_level, reset);
		diff_eq_int("agc.cfg.acquire_level (%ld)", a.cfg.acquire_level,
			    b.cfg.acquire_level, reset);
		diff_eq_int("agc.cfg.squelch_level (%ld)", a.cfg.squelch_level,
			    b.cfg.squelch_level, reset);
		diff_eq_int("agc.cfg.block_len (%ld)", a.cfg.block_len,
			    b.cfg.block_len, reset);
		diff_eq_int("agc.cfg.f14 (%ld)", a.cfg.f14, b.cfg.f14, reset);
		diff_eq_int("agc.cfg.f16 (%ld)", a.cfg.f16, b.cfg.f16, reset);
		/* The pointers were copied wholesale; what they reach is the
		 * only thing the two sides can agree about. */
		cmp_shorts("agc.cfg.alpha[%ld]", a.cfg.alpha, b.cfg.alpha, 2);
		cmp_shorts("agc.cfg.beta[%ld]", a.cfg.beta, b.cfg.beta, 2);

		diff_eq_int("agc.f18 (%ld)", a.f18, b.f18, reset);
		diff_eq_int("agc.signal (%ld)", a.signal, b.signal, reset);
		diff_eq_int("agc.level (%ld)", a.level, b.level, reset);
		diff_eq_int("agc.mult (%ld)", a.mult, b.mult, reset);
		diff_eq_int("agc.shift (%ld)", a.shift, b.shift, reset);
		diff_eq_int("agc.freeze (%ld)", a.freeze, b.freeze, reset);
	}

	return diff_end();
}

static int
test_use_mtd(void)
{
	static struct fpm_mtd a, b;
	static short acc_a[8], acc_b[8];
	struct fpm_mtd_cfg ca, cb;
	int which;

	diff_begin("v27cfg: the tone banks through FPM_MTD_create");

	/*
	 * Three configurations, all three of which `V27RX_create` builds: the
	 * V.21 channel-2 detector at 0x4ccd / 300, and the V.27ter tone
	 * detector at 0x199a / 100 with one bank per rate.
	 */
	for (which = 0; which < 3; which++) {
		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));
		a.acc = acc_a;
		b.acc = acc_b;

		ca = FPM_MTD_CFG;
		cb = ref_FPM_MTD_CFG;
		ca.tones = cb.tones = 2;
		if (which == 0) {
			ca.coeff = V21_CHAN2_MTD_COEFF;
			cb.coeff = ref_V21_CHAN2_MTD_COEFF;
			ca.ratio = cb.ratio = 0x4ccd;
			ca.min_level = cb.min_level = 300;
		} else if (which == 1) {
			ca.coeff = V27_MTD_COEFF_2400;
			cb.coeff = ref_V27_MTD_COEFF_2400;
			ca.ratio = cb.ratio = 0x199a;
			ca.min_level = cb.min_level = 100;
		} else {
			ca.coeff = V27_MTD_COEFF_4800;
			cb.coeff = ref_V27_MTD_COEFF_4800;
			ca.ratio = cb.ratio = 0x199a;
			ca.min_level = cb.min_level = 100;
		}

		FPM_MTD_create(&a, &ca);
		ref_FPM_MTD_create(&b, &cb);

		diff_eq_int("mtd.cfg.tones (%ld)", a.cfg.tones, b.cfg.tones,
			    which);
		diff_eq_int("mtd.cfg.ratio (%ld)", a.cfg.ratio, b.cfg.ratio,
			    which);
		diff_eq_int("mtd.cfg.min_level (%ld)", a.cfg.min_level,
			    b.cfg.min_level, which);
		diff_eq_int("mtd.cfg.f0a (%ld)", a.cfg.f0a, b.cfg.f0a, which);
		cmp_shorts("mtd.cfg.coeff[%ld]", a.cfg.coeff, b.cfg.coeff, 10);
		diff_eq_int("mtd.dc_state[0] (%ld)", a.dc_state[0],
			    b.dc_state[0], which);
		diff_eq_int("mtd.dc_state[1] (%ld)", a.dc_state[1],
			    b.dc_state[1], which);
		diff_eq_int("mtd.out_of_band (%ld)", a.out_of_band,
			    b.out_of_band, which);
		diff_eq_int("mtd.wideband (%ld)", a.wideband, b.wideband,
			    which);
		cmp_shorts("mtd.acc[%ld]", a.acc, b.acc, 2 * 2);
	}

	return diff_end();
}

static int
test_use_sre(void)
{
	static struct fpm_sre a, b;
	struct fpm_sre_cfg ca, cb;
	int rate;

	diff_begin("v27cfg: the SRE tables through FPM_SRE_init, both rates");

	for (rate = 0; rate <= 1; rate++) {
		int clock_len = V27RX_SAMP_PER_BAUD[rate];
		int coeffs = V27RX_SRE_FILT_LEN[rate];

		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));

		build_sre_cfg(&ca, &FPM_SRE_CFG, clock_len, coeffs,
			      V27RX_SRE_FILT[rate], V27RX_XB_COFFS[rate],
			      V27RX_XCLOCK[rate], V27RX_YCLOCK[rate],
			      V27RX_SRE_PLLK1[rate], V27RX_SRE_PLLK2[rate]);
		build_sre_cfg(&cb, &ref_FPM_SRE_CFG, clock_len, coeffs,
			      ref_V27RX_SRE_FILT[rate],
			      ref_V27RX_XB_COFFS[rate],
			      ref_V27RX_XCLOCK[rate], ref_V27RX_YCLOCK[rate],
			      ref_V27RX_SRE_PLLK1[rate],
			      ref_V27RX_SRE_PLLK2[rate]);

		FPM_SRE_init(&a, &ca, 1);
		ref_FPM_SRE_init(&b, &cb, 1);

		diff_eq_int("sre.taps (%ld)", a.taps, b.taps, rate);
		diff_eq_int("sre.taps is coeffs/branches (%ld)", a.taps,
			    coeffs / FPM_SRE_BRANCHES, rate);
		diff_eq_int("sre.mode (%ld)", a.mode, b.mode, rate);
		diff_eq_int("sre.groups (%ld)", a.groups, b.groups, rate);
		diff_eq_int("sre.settle (%ld)", a.settle, b.settle, rate);
		diff_eq_int("sre.need (%ld)", a.need, b.need, rate);
		diff_eq_int("sre.rms_on (%ld)", a.rms_on, b.rms_on, rate);
		diff_eq_int("sre.acquiring (%ld)", a.acquiring, b.acquiring,
			    rate);
		diff_eq_int("sre.cfg.coeffs (%ld)", a.cfg.coeffs, b.cfg.coeffs,
			    rate);

		/*
		 * THE POINT OF THIS BLOCK.  `FPM_SRE_init` copies `cfg.coeffs`
		 * entries out of `proto` into its own buffer, so this compares
		 * shorts that reached the state THROUGH the count
		 * `V27RX_create` wrote -- a table of the right bytes and the
		 * wrong length cannot pass it.
		 */
		cmp_shorts("sre.coeff[%ld]", a.coeff, b.coeff, coeffs);

		cmp_shorts("sre.cfg.disc[%ld]", a.cfg.disc, b.cfg.disc,
			   FPM_SRE_DISC);
		cmp_shorts("sre.cfg.xclock[%ld]", a.cfg.xclock, b.cfg.xclock,
			   clock_len);
		cmp_shorts("sre.cfg.yclock[%ld]", a.cfg.yclock, b.cfg.yclock,
			   clock_len);
		cmp_shorts("sre.cfg.pll_k1[%ld]", a.cfg.pll_k1, b.cfg.pll_k1,
			   FPM_SRE_MODES);
		cmp_shorts("sre.cfg.pll_k2[%ld]", a.cfg.pll_k2, b.cfg.pll_k2,
			   FPM_SRE_MODES);
		/* proto's last entry is past `coeffs` and is read only by the
		 * interpolator, so it never reaches `sre.coeff`. */
		diff_eq_int("sre.cfg.proto[coeffs] (%ld)", a.cfg.proto[coeffs],
			    b.cfg.proto[coeffs], rate);

		FPM_SRE_free(&a);
		ref_FPM_SRE_free(&b);
	}

	return diff_end();
}

static int
test_use_fse(void)
{
	static struct fpm_fse a, b;
	struct fpm_fse_cfg ca, cb;
	int rate;

	diff_begin("v27cfg: the FSE tables through FPM_FSE_init, both rates");

	for (rate = 0; rate <= 1; rate++) {
		int taps = V27RX_FSE_FILT_LEN[rate];
		int clk_mod = V27RX_CRR_TABLE_LEN[rate];
		int block = rate == 0 ? 0x90 : 0xa0;

		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));

		build_fse_cfg(&ca, &FPM_FSE_CFG, block,
			      V27RX_SAMP_PER_BAUD[rate],
			      V27RX_FSE_IFILT[rate], V27RX_FSE_QFILT[rate],
			      taps, V27RX_FSE_MU_TRAIN[rate],
			      V27RX_FSE_MU_TRACK[rate], V27RX_CRR_TABLE[rate],
			      clk_mod, V27RX_CRR_ADJUST[rate],
			      V27RX_FSE_PLLK1[rate], V27RX_FSE_PLLK2[rate]);
		build_fse_cfg(&cb, &ref_FPM_FSE_CFG, block,
			      V27RX_SAMP_PER_BAUD[rate],
			      ref_V27RX_FSE_IFILT[rate],
			      ref_V27RX_FSE_QFILT[rate], taps,
			      V27RX_FSE_MU_TRAIN[rate],
			      V27RX_FSE_MU_TRACK[rate],
			      ref_V27RX_CRR_TABLE[rate], clk_mod,
			      V27RX_CRR_ADJUST[rate],
			      ref_V27RX_FSE_PLLK1[rate],
			      ref_V27RX_FSE_PLLK2[rate]);

		FPM_FSE_init(&a, &ca, 1);
		ref_FPM_FSE_init(&b, &cb, 1);

		diff_eq_int("fse.cfg.block (%ld)", a.cfg.block, b.cfg.block,
			    rate);
		diff_eq_int("fse.cfg.taps (%ld)", a.cfg.taps, b.cfg.taps, rate);
		diff_eq_int("fse.cfg.interp (%ld)", a.cfg.interp, b.cfg.interp,
			    rate);
		diff_eq_int("fse.cfg.clk_mod (%ld)", a.cfg.clk_mod,
			    b.cfg.clk_mod, rate);
		diff_eq_int("fse.cfg.clk_inc (%ld)", a.cfg.clk_inc,
			    b.cfg.clk_inc, rate);
		diff_eq_int("fse.mu_sel (%ld)", a.mu_sel, b.mu_sel, rate);
		diff_eq_int("fse.pll_sel (%ld)", a.pll_sel, b.pll_sel, rate);
		diff_eq_int("fse.widx (%ld)", a.widx, b.widx, rate);
		diff_eq_int("fse.need (%ld)", a.need, b.need, rate);

		/* The coefficients that reached the state through `cfg.taps`. */
		cmp_shorts("fse.icoeff[%ld]", a.icoeff, b.icoeff, taps);
		cmp_shorts("fse.qcoeff[%ld]", a.qcoeff, b.qcoeff, taps);
		cmp_shorts("fse.cfg.clk[%ld]", a.cfg.clk, b.cfg.clk, clk_mod);
		cmp_shorts("fse.cfg.pll_k1[%ld]", a.cfg.pll_k1, b.cfg.pll_k1,
			   3);
		cmp_shorts("fse.cfg.pll_k2[%ld]", a.cfg.pll_k2, b.cfg.pll_k2,
			   3);
		cmp_shorts("fse.tilt_coeff[%ld]", a.tilt_coeff, b.tilt_coeff,
			   4);

		FPM_FSE_free(&a);
		ref_FPM_FSE_free(&b);
	}

	return diff_end();
}

static int
test_use_mrf(void)
{
	static struct fpm_mrf a, b;
	struct fpm_mrf_cfg ca, cb;
	int rate;

	diff_begin("v27cfg: V27RX_MRF_FILT through FPM_MRF_init, both rates");

	for (rate = 0; rate <= 1; rate++) {
		int up = V27RX_MRF_UP[rate];
		int down = V27RX_MRF_DOWN[rate];
		int taps = V27RX_MRF_FILT_LEN[rate];

		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));

		build_mrf_cfg(&ca, V27RX_MRF_FILT[rate], up, down, taps);
		build_mrf_cfg(&cb, ref_V27RX_MRF_FILT[rate], up, down, taps);

		FPM_MRF_init(&a, &ca, 1);
		ref_FPM_MRF_init(&b, &cb, 1);

		diff_eq_int("mrf.cfg.branches (%ld)", a.cfg.branches,
			    b.cfg.branches, rate);
		diff_eq_int("mrf.cfg.decimate (%ld)", a.cfg.decimate,
			    b.cfg.decimate, rate);
		diff_eq_int("mrf.cfg.taps (%ld)", a.cfg.taps, b.cfg.taps, rate);
		diff_eq_int("mrf.history_len (%ld)", a.history_len,
			    b.history_len, rate);
		diff_eq_int("mrf.history_len is taps/branches (%ld)",
			    a.history_len, taps / up, rate);
		diff_eq_int("mrf.need (%ld)", a.need, b.need, rate);
		diff_eq_int("mrf.phase (%ld)", a.phase, b.phase, rate);
		diff_eq_int("mrf.widx (%ld)", a.widx, b.widx, rate);
		cmp_shorts("mrf.cfg.coeff[%ld]", a.cfg.coeff, b.cfg.coeff,
			   taps);
		cmp_shorts("mrf.history[%ld]", a.history, b.history,
			   taps / up);

		sysdep_free(a.history);
		sysdep_free(b.history);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;

	rc |= test_shape();
	rc |= test_values();
	rc |= test_detector_fires();
	rc |= test_value_shape();
	rc |= test_use_agc();
	rc |= test_use_mtd();
	rc |= test_use_sre();
	rc |= test_use_fse();
	rc |= test_use_mrf();

	return rc;
}
