/*
 * t_v17cfg.c -- differential test of the V.17 fax receiver's twenty-four
 *               tables, and of the five DSP configurations `V17RX_create`
 *               patches them into.
 *
 *   FSEv17_QCOFF     .rodata 0x009780    98
 *   FSEv17_ICOFF     .rodata 0x009800    98
 *   SREv17_COFFS     .rodata 0x0099c0   362
 *   SREv17_XB_COFFS  .rodata 0x009b2a    22
 *   MRFv17_COFFS     .rodata 0x009b40   720
 *   AGCv17_CFG       .rodata 0x009e10    24
 *   VTBv17_QMAP128   .rodata 0x00b580   258
 *   VTBv17_IMAP128   .rodata 0x00b6a0   258
 *   VTBv17_QMAP64    .rodata 0x00b7c0   130
 *   VTBv17_IMAP64    .rodata 0x00b860   130
 *   VTBv17_QMAP32    .rodata 0x00b900    66
 *   VTBv17_IMAP32    .rodata 0x00b960    66
 *   VTBv17_QMAP16T   .rodata 0x00b9c0    34
 *   VTBv17_IMAP16T   .rodata 0x00ba00    34
 *   V17_MTD_COEFF    .data   0x007940    20
 *   CRRv17_CLK       .data   0x0079c8     8
 *   CRRv17_PLL_K1_S  .data   0x0079d0     6
 *   CRRv17_PLL_K2    .data   0x0079d6     6
 *   CRRv17_PLL_K1    .data   0x0079dc     6
 *   SREv17_PLL_K1_S  .data   0x0079e2     6
 *   SREv17_PLL_K2    .data   0x0079e8     6
 *   SREv17_PLL_K1    .data   0x0079ee     6
 *   SREv17_yCLOCK    .data   0x0079f4     6
 *   SREv17_xCLOCK    .data   0x0079fa     6
 *
 * FIVE LAYERS, and each one exists because of a specific wrong reading the
 * layer below it would pass.  This is `t_v29cfg.c`'s shape (F9140) applied to
 * V.17, and the layer that earns its keep loudest here is the first.
 *
 *   1. SHAPE.  `sizeof` against the `nm -S` size of every symbol.  A byte
 *      comparison run over OUR array's length cannot notice that our array is
 *      one element short of the object's -- it would simply compare fewer
 *      bytes and pass.  V.17 has NINE live cases, not one: `SREv17_COFFS` is
 *      181 where `sre.coeffs` is 180, and all eight `VTBv17_?MAP*` are N+1
 *      where the decoder can only reach N.  Declaring any of them from the
 *      consumer's count alone would be two bytes short and invisible.
 *
 *   2. VALUE, element by element against `ref_`.  This is what fixes the
 *      bytes, and on its own it is the weakest-looking layer.
 *
 *   3. THE DETECTOR IS SHOWN TO FIRE (F134).  `shorts_differ` is run over an
 *      identical copy, which must return 0, and then over one perturbed copy
 *      per element of every table, each of which must return non-zero.  A
 *      comparison of two arrays that are equal by construction proves nothing
 *      until it has been seen to reject something.
 *
 *   4. THE SHAPE OF THE VALUES, independently of `ref_`.  The long filters are
 *      asserted symmetric, the equaliser rails are asserted to tile the taps,
 *      `CRRv17_CLK` is asserted equal to `round(i * 32768 / 4)`, and the four
 *      constellations are asserted to be closed point sets AND to be
 *      byte-identical to V.32bis' own, which links two independently
 *      extracted table sets to each other with no blob in between.
 *
 *   5. USE.  Every table is driven through the DSP block that consumes it,
 *      ours against the blob's, with the configuration built exactly as
 *      `V17RX_create` builds it -- copy the library built-in, patch the
 *      tables and the lengths, call init -- and for BOTH settings of the flag
 *      that chooses between the `_S` gain arrays and the plain ones, and for
 *      all four bit rates the constructor distinguishes.  A wrong element
 *      stride survives layers 1 to 4 only if it also reproduces the bytes,
 *      but a wrong LENGTH FIELD does not survive this one: `FPM_SRE_init`
 *      copies `cfg.coeffs` entries out of `proto`, so a count that disagrees
 *      with the object's shows up in the coefficient buffer.
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
 * here only through `AGCv17_CFG.alpha` and `.beta`, which is the F9058/F9144
 * rule: where a name is ambiguous in the object, the consumer is the
 * comparison.
 */

#include <stddef.h>
#include <string.h>

#include "harness.h"

#include "dsplib/v17cfg.h"
#include "dsplib/faxcfg.h"
#include "dsplib/vtb.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/sysdep.h"

/* The blob's copies.  An undeclared `ref_` name is a hard error, not an int. */
extern const struct fpm_agc_cfg ref_AGCv17_CFG;
extern const short ref_FSEv17_QCOFF[49];
extern const short ref_FSEv17_ICOFF[49];
extern const short ref_SREv17_COFFS[181];
extern const short ref_SREv17_XB_COFFS[11];
extern const short ref_MRFv17_COFFS[360];
extern const short ref_VTBv17_QMAP128[129];
extern const short ref_VTBv17_IMAP128[129];
extern const short ref_VTBv17_QMAP64[65];
extern const short ref_VTBv17_IMAP64[65];
extern const short ref_VTBv17_QMAP32[33];
extern const short ref_VTBv17_IMAP32[33];
extern const short ref_VTBv17_QMAP16T[17];
extern const short ref_VTBv17_IMAP16T[17];
extern short ref_V17_MTD_COEFF[10];
extern short ref_CRRv17_CLK[4];
extern short ref_CRRv17_PLL_K1_S[3];
extern short ref_CRRv17_PLL_K2[3];
extern short ref_CRRv17_PLL_K1[3];
extern short ref_SREv17_PLL_K1_S[3];
extern short ref_SREv17_PLL_K2[3];
extern short ref_SREv17_PLL_K1[3];
extern short ref_SREv17_yCLOCK[3];
extern short ref_SREv17_xCLOCK[3];

extern short ref_V21_CHAN2_MTD_COEFF[10];

extern const short ref_VTB_BOUND_7200[128];
extern const short ref_VTB_BOUND_9600[416];
extern const short ref_VTB_BOUND_12000[960];
extern const short ref_VTB_BOUND_14400[1856];
extern const short ref_VTB_REGION_7200[8];
extern const short ref_VTB_REGION_9600[32];
extern const short ref_VTB_REGION_12000[72];
extern const short ref_VTB_REGION_14400[128];

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
extern void ref_VTB_decoder(struct vtb *state, short i, short q, short *out);

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
 * The twenty-three plain arrays, once, so that layers 2 and 3 walk the same
 * list and neither can silently drop one.  `AGCv17_CFG` is not here: it is a
 * struct with two pointers in it and is handled by hand.
 */
struct tabrow {
	const short *ours;
	const short *blob;
	int n;
	const char *name;
};

static const struct tabrow tabs[] = {
	{ FSEv17_QCOFF,    ref_FSEv17_QCOFF,     49, "FSEv17_QCOFF"    },
	{ FSEv17_ICOFF,    ref_FSEv17_ICOFF,     49, "FSEv17_ICOFF"    },
	{ SREv17_COFFS,    ref_SREv17_COFFS,    181, "SREv17_COFFS"    },
	{ SREv17_XB_COFFS, ref_SREv17_XB_COFFS,  11, "SREv17_XB_COFFS" },
	{ MRFv17_COFFS,    ref_MRFv17_COFFS,    360, "MRFv17_COFFS"    },
	{ VTBv17_QMAP128,  ref_VTBv17_QMAP128,  129, "VTBv17_QMAP128"  },
	{ VTBv17_IMAP128,  ref_VTBv17_IMAP128,  129, "VTBv17_IMAP128"  },
	{ VTBv17_QMAP64,   ref_VTBv17_QMAP64,    65, "VTBv17_QMAP64"   },
	{ VTBv17_IMAP64,   ref_VTBv17_IMAP64,    65, "VTBv17_IMAP64"   },
	{ VTBv17_QMAP32,   ref_VTBv17_QMAP32,    33, "VTBv17_QMAP32"   },
	{ VTBv17_IMAP32,   ref_VTBv17_IMAP32,    33, "VTBv17_IMAP32"   },
	{ VTBv17_QMAP16T,  ref_VTBv17_QMAP16T,   17, "VTBv17_QMAP16T"  },
	{ VTBv17_IMAP16T,  ref_VTBv17_IMAP16T,   17, "VTBv17_IMAP16T"  },
	{ V17_MTD_COEFF,   ref_V17_MTD_COEFF,    10, "V17_MTD_COEFF"   },
	{ CRRv17_CLK,      ref_CRRv17_CLK,        4, "CRRv17_CLK"      },
	{ CRRv17_PLL_K1_S, ref_CRRv17_PLL_K1_S,   3, "CRRv17_PLL_K1_S" },
	{ CRRv17_PLL_K2,   ref_CRRv17_PLL_K2,     3, "CRRv17_PLL_K2"   },
	{ CRRv17_PLL_K1,   ref_CRRv17_PLL_K1,     3, "CRRv17_PLL_K1"   },
	{ SREv17_PLL_K1_S, ref_SREv17_PLL_K1_S,   3, "SREv17_PLL_K1_S" },
	{ SREv17_PLL_K2,   ref_SREv17_PLL_K2,     3, "SREv17_PLL_K2"   },
	{ SREv17_PLL_K1,   ref_SREv17_PLL_K1,     3, "SREv17_PLL_K1"   },
	{ SREv17_yCLOCK,   ref_SREv17_yCLOCK,     3, "SREv17_yCLOCK"   },
	{ SREv17_xCLOCK,   ref_SREv17_xCLOCK,     3, "SREv17_xCLOCK"   }
};
#define NTABS ((int)(sizeof(tabs) / sizeof(tabs[0])))

/* ------------------------------------------------------------------------- */

static int
test_shape(void)
{
	diff_begin("v17cfg: sizes against the object's symbol table");

	/*
	 * Every one of these is `nm -S`'s size for the symbol.  A table whose
	 * C array is shorter than the object's symbol compares equal over its
	 * own length and is still wrong.
	 */
	diff_eq_int("sizeof AGCv17_CFG (%ld)",
		    (long)sizeof(AGCv17_CFG), 24, 0);
	diff_eq_int("sizeof FSEv17_QCOFF (%ld)",
		    (long)sizeof(FSEv17_QCOFF), 98, 0);
	diff_eq_int("sizeof FSEv17_ICOFF (%ld)",
		    (long)sizeof(FSEv17_ICOFF), 98, 0);
	diff_eq_int("sizeof SREv17_COFFS (%ld)",
		    (long)sizeof(SREv17_COFFS), 362, 0);
	diff_eq_int("sizeof SREv17_XB_COFFS (%ld)",
		    (long)sizeof(SREv17_XB_COFFS), 22, 0);
	diff_eq_int("sizeof MRFv17_COFFS (%ld)",
		    (long)sizeof(MRFv17_COFFS), 720, 0);
	diff_eq_int("sizeof VTBv17_QMAP128 (%ld)",
		    (long)sizeof(VTBv17_QMAP128), 258, 0);
	diff_eq_int("sizeof VTBv17_IMAP128 (%ld)",
		    (long)sizeof(VTBv17_IMAP128), 258, 0);
	diff_eq_int("sizeof VTBv17_QMAP64 (%ld)",
		    (long)sizeof(VTBv17_QMAP64), 130, 0);
	diff_eq_int("sizeof VTBv17_IMAP64 (%ld)",
		    (long)sizeof(VTBv17_IMAP64), 130, 0);
	diff_eq_int("sizeof VTBv17_QMAP32 (%ld)",
		    (long)sizeof(VTBv17_QMAP32), 66, 0);
	diff_eq_int("sizeof VTBv17_IMAP32 (%ld)",
		    (long)sizeof(VTBv17_IMAP32), 66, 0);
	diff_eq_int("sizeof VTBv17_QMAP16T (%ld)",
		    (long)sizeof(VTBv17_QMAP16T), 34, 0);
	diff_eq_int("sizeof VTBv17_IMAP16T (%ld)",
		    (long)sizeof(VTBv17_IMAP16T), 34, 0);
	diff_eq_int("sizeof V17_MTD_COEFF (%ld)",
		    (long)sizeof(V17_MTD_COEFF), 20, 0);
	diff_eq_int("sizeof CRRv17_CLK (%ld)",
		    (long)sizeof(CRRv17_CLK), 8, 0);
	diff_eq_int("sizeof CRRv17_PLL_K1_S (%ld)",
		    (long)sizeof(CRRv17_PLL_K1_S), 6, 0);
	diff_eq_int("sizeof CRRv17_PLL_K2 (%ld)",
		    (long)sizeof(CRRv17_PLL_K2), 6, 0);
	diff_eq_int("sizeof CRRv17_PLL_K1 (%ld)",
		    (long)sizeof(CRRv17_PLL_K1), 6, 0);
	diff_eq_int("sizeof SREv17_PLL_K1_S (%ld)",
		    (long)sizeof(SREv17_PLL_K1_S), 6, 0);
	diff_eq_int("sizeof SREv17_PLL_K2 (%ld)",
		    (long)sizeof(SREv17_PLL_K2), 6, 0);
	diff_eq_int("sizeof SREv17_PLL_K1 (%ld)",
		    (long)sizeof(SREv17_PLL_K1), 6, 0);
	diff_eq_int("sizeof SREv17_yCLOCK (%ld)",
		    (long)sizeof(SREv17_yCLOCK), 6, 0);
	diff_eq_int("sizeof SREv17_xCLOCK (%ld)",
		    (long)sizeof(SREv17_xCLOCK), 6, 0);

	/*
	 * The relationships `V17RX_create` depends on, stated as arithmetic
	 * rather than as a second set of literals.
	 */
	diff_eq_int("SREv17_COFFS holds coeffs+1 entries (%ld)",
		    (long)(sizeof(SREv17_COFFS) / sizeof(short)), 180 + 1, 0);
	diff_eq_int("XB_COFFS holds FPM_SRE_DISC entries (%ld)",
		    (long)(sizeof(SREv17_XB_COFFS) / sizeof(short)),
		    FPM_SRE_DISC, 0);
	diff_eq_int("xCLOCK holds clock_len entries (%ld)",
		    (long)(sizeof(SREv17_xCLOCK) / sizeof(short)), 3, 0);
	diff_eq_int("yCLOCK holds clock_len entries (%ld)",
		    (long)(sizeof(SREv17_yCLOCK) / sizeof(short)), 3, 0);
	diff_eq_int("SREv17_PLL_K1 holds FPM_SRE_MODES entries (%ld)",
		    (long)(sizeof(SREv17_PLL_K1) / sizeof(short)),
		    FPM_SRE_MODES, 0);
	diff_eq_int("SREv17_PLL_K1_S holds FPM_SRE_MODES entries (%ld)",
		    (long)(sizeof(SREv17_PLL_K1_S) / sizeof(short)),
		    FPM_SRE_MODES, 0);
	diff_eq_int("MRFv17_COFFS holds 9 branches x 40 taps (%ld)",
		    (long)(sizeof(MRFv17_COFFS) / sizeof(short)), 9 * 40, 0);
	diff_eq_int("SREv17_COFFS holds 10 branches x 18 taps, plus one (%ld)",
		    (long)(sizeof(SREv17_COFFS) / sizeof(short)),
		    FPM_SRE_BRANCHES * 18 + 1, 0);
	diff_eq_int("FSEv17_ICOFF holds fse.taps entries (%ld)",
		    (long)(sizeof(FSEv17_ICOFF) / sizeof(short)), 49, 0);
	diff_eq_int("FSEv17_QCOFF holds fse.taps entries (%ld)",
		    (long)(sizeof(FSEv17_QCOFF) / sizeof(short)), 49, 0);
	diff_eq_int("CRRv17_CLK holds fse.clk_mod entries (%ld)",
		    (long)(sizeof(CRRv17_CLK) / sizeof(short)), 4, 0);

	/*
	 * THE CONSTELLATION MAPS ARE ONE LONGER THAN THE CONSTELLATION.  The
	 * decoder's own count is twice `vtb.mask + 1`, and `V17RX_create`
	 * writes `mask = (1 << (nsub + 2)) - 1`, so the point count is
	 * `2 << (nsub + 2)`: 16, 32, 64, 128 for `nsub` 1, 2, 3, 4.  The
	 * symbol is that plus one entry.
	 */
	diff_eq_int("QMAP16T is 16 points plus one (%ld)",
		    (long)(sizeof(VTBv17_QMAP16T) / sizeof(short)),
		    (1 << (1 + 2)) * 2 + 1, 0);
	diff_eq_int("QMAP32 is 32 points plus one (%ld)",
		    (long)(sizeof(VTBv17_QMAP32) / sizeof(short)),
		    (1 << (2 + 2)) * 2 + 1, 0);
	diff_eq_int("QMAP64 is 64 points plus one (%ld)",
		    (long)(sizeof(VTBv17_QMAP64) / sizeof(short)),
		    (1 << (3 + 2)) * 2 + 1, 0);
	diff_eq_int("QMAP128 is 128 points plus one (%ld)",
		    (long)(sizeof(VTBv17_QMAP128) / sizeof(short)),
		    (1 << (4 + 2)) * 2 + 1, 0);
	/* and the I rail matches the Q rail, rate by rate */
	diff_eq_int("IMAP16T matches QMAP16T (%ld)",
		    (long)sizeof(VTBv17_IMAP16T),
		    (long)sizeof(VTBv17_QMAP16T), 0);
	diff_eq_int("IMAP32 matches QMAP32 (%ld)",
		    (long)sizeof(VTBv17_IMAP32), (long)sizeof(VTBv17_QMAP32),
		    0);
	diff_eq_int("IMAP64 matches QMAP64 (%ld)",
		    (long)sizeof(VTBv17_IMAP64), (long)sizeof(VTBv17_QMAP64),
		    0);
	diff_eq_int("IMAP128 matches QMAP128 (%ld)",
		    (long)sizeof(VTBv17_IMAP128), (long)sizeof(VTBv17_QMAP128),
		    0);

	/* The library built-ins the constructor copies. */
	diff_eq_int("sizeof FPM_FSE_CFG (%ld)",
		    (long)sizeof(FPM_FSE_CFG), 56, 0);
	diff_eq_int("sizeof FPM_MTD_CFG (%ld)",
		    (long)sizeof(FPM_MTD_CFG), 12, 0);
	diff_eq_int("sizeof FPM_SRE_CFG (%ld)",
		    (long)sizeof(FPM_SRE_CFG), 56, 0);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

static int
test_values(void)
{
	int i;

	diff_begin("v17cfg: table values against the blob");

	for (i = 0; i < NTABS; i++)
		cmp_shorts(tabs[i].name, tabs[i].ours, tabs[i].blob,
			   tabs[i].n);

	/* AGCv17_CFG, field by field; the two pointers by their contents. */
	diff_eq_int("AGCv17_CFG.ref_level (%ld)", AGCv17_CFG.ref_level,
		    ref_AGCv17_CFG.ref_level, 0x00);
	diff_eq_int("AGCv17_CFG.acquire_level (%ld)",
		    AGCv17_CFG.acquire_level, ref_AGCv17_CFG.acquire_level,
		    0x02);
	diff_eq_int("AGCv17_CFG.squelch_level (%ld)",
		    AGCv17_CFG.squelch_level, ref_AGCv17_CFG.squelch_level,
		    0x04);
	diff_eq_int("AGCv17_CFG.f06 (%ld)", AGCv17_CFG.f06,
		    ref_AGCv17_CFG.f06, 0x06);
	diff_eq_int("AGCv17_CFG.f08 (%ld)", AGCv17_CFG.f08,
		    ref_AGCv17_CFG.f08, 0x08);
	diff_eq_int("AGCv17_CFG.block_len (%ld)", AGCv17_CFG.block_len,
		    ref_AGCv17_CFG.block_len, 0x0a);
	diff_eq_int("AGCv17_CFG.f14 (%ld)", AGCv17_CFG.f14,
		    ref_AGCv17_CFG.f14, 0x14);
	diff_eq_int("AGCv17_CFG.f16 (%ld)", AGCv17_CFG.f16,
		    ref_AGCv17_CFG.f16, 0x16);
	cmp_shorts("AGCv17_CFG.alpha[%ld]", AGCv17_CFG.alpha,
		   ref_AGCv17_CFG.alpha, 2);
	cmp_shorts("AGCv17_CFG.beta[%ld]", AGCv17_CFG.beta,
		   ref_AGCv17_CFG.beta, 2);

	/*
	 * ANTI-VACUITY.  Several of these tables are largely small numbers, so
	 * assert the comparison saw real data.  The two equaliser rails tile
	 * the taps: the I rail is zero at the 24 odd indices and the Q rail at
	 * the 25 even ones, so 25 and 24 entries are non-zero.
	 */
	diff_eq_int("MRFv17_COFFS has non-zero entries (%ld)",
		    nonzero(MRFv17_COFFS, 360) > 350, 1, 0);
	diff_eq_int("SREv17_COFFS has non-zero entries (%ld)",
		    nonzero(SREv17_COFFS, 181) > 170, 1, 0);
	diff_eq_int("FSEv17_ICOFF non-zero entries (%ld)",
		    nonzero(FSEv17_ICOFF, 49), 25, 0);
	diff_eq_int("FSEv17_QCOFF non-zero entries (%ld)",
		    nonzero(FSEv17_QCOFF, 49), 24, 0);
	diff_eq_int("VTBv17_IMAP128 non-zero entries (%ld)",
		    nonzero(VTBv17_IMAP128, 129) > 100, 1, 0);

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
	static short copy[400];		/* MRFv17_COFFS is the longest, 360 */
	int i;

	diff_begin("v17cfg: the comparison rejects a perturbed table");

	for (i = 0; i < NTABS; i++) {
		int j;

		memcpy(copy, tabs[i].ours,
		       (size_t)tabs[i].n * sizeof(short));
		diff_eq_int("identical copy compares equal (%ld)",
			    shorts_differ(copy, tabs[i].ours, tabs[i].n), 0, i);

		/* Every element in turn, so a detector that only looks at the
		 * first or the last one is caught too. */
		for (j = 0; j < tabs[i].n; j++) {
			copy[j] = (short)(copy[j] + 1);
			diff_eq_int("one-element perturbation is seen (%ld)",
				    shorts_differ(copy, tabs[i].ours,
						  tabs[i].n),
				    1, i * 1000 + j);
			copy[j] = (short)(copy[j] - 1);
		}
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * Layer 4.  Facts about the values that hold independently of the blob, and
 * that a transcription error would break even if it were transcribed into both
 * sides -- which cannot happen here, but which is exactly what a generator
 * written from these bytes later WOULD risk.
 */
static int
test_value_shape(void)
{
	int i, sym;

	diff_begin("v17cfg: the shape of the values");

	/*
	 * The carrier reference, in closed form.  `round(i * 32768 / 4)`
	 * computed in integers as `(i * 65536 + 4) / 8`.
	 */
	for (i = 0; i < 4; i++)
		diff_eq_int("CRRv17_CLK[%ld] is round(i*32768/4)",
			    CRRv17_CLK[i], (i * 65536 + 4) / 8, i);

	/* And the step is what makes the carrier 1800 Hz at 7200 Hz: four
	 * entries is one turn, one entry per input sample. */
	diff_eq_int("7200 * clk_inc / clk_mod is V.17's carrier (%ld)",
		    7200 * 1 / 4, 1800, 0);

	/* The two long prototypes are linear phase: symmetric about centre.
	 * `SREv17_COFFS`'s palindrome runs over all 181 entries, so the extra
	 * entry past `coeffs` is part of the design and not an appendix. */
	sym = 1;
	for (i = 0; i < 360; i++)
		if (MRFv17_COFFS[i] != MRFv17_COFFS[359 - i])
			sym = 0;
	diff_eq_int("MRFv17_COFFS is symmetric (%ld)", sym, 1, 0);
	diff_eq_int("MRFv17_COFFS centre pair (%ld)",
		    MRFv17_COFFS[179] == MRFv17_COFFS[180], 1, 0);

	sym = 1;
	for (i = 0; i < 181; i++)
		if (SREv17_COFFS[i] != SREv17_COFFS[180 - i])
			sym = 0;
	diff_eq_int("SREv17_COFFS is symmetric over all 181 (%ld)", sym, 1, 0);

	/*
	 * THE ZERO PATTERN, AND IT IS NOT V.29's.  V.29's rails are zero every
	 * THIRD tap, which reads as three samples per symbol.  V.17's are zero
	 * every SECOND: the Q rail on the even indices and the I rail on the
	 * odd ones, so the two rails tile all 49 taps and never overlap.  Both
	 * receivers run three samples per symbol, so this spacing is not the
	 * interpolation factor and the tidy reading is the wrong one.
	 */
	for (i = 0; i < 49; i += 2)
		diff_eq_int("FSEv17_QCOFF[%ld] is zero", FSEv17_QCOFF[i], 0, i);
	for (i = 1; i < 49; i += 2)
		diff_eq_int("FSEv17_ICOFF[%ld] is zero", FSEv17_ICOFF[i], 0, i);
	diff_eq_int("FSEv17_ICOFF[24] is the main tap (%ld)",
		    FSEv17_ICOFF[24] != 0, 1, 24);
	sym = 1;
	for (i = 0; i < 49; i++)
		if (FSEv17_ICOFF[i] != FSEv17_ICOFF[48 - i])
			sym = 0;
	diff_eq_int("FSEv17_ICOFF is symmetric (%ld)", sym, 1, 0);
	sym = 1;
	for (i = 0; i < 49; i++)
		if (FSEv17_QCOFF[i] != -FSEv17_QCOFF[48 - i])
			sym = 0;
	diff_eq_int("FSEv17_QCOFF is antisymmetric (%ld)", sym, 1, 0);

	/* Both PLL gain arrays start with a zero integral term. */
	diff_eq_int("CRRv17_PLL_K2[0] is zero (%ld)", CRRv17_PLL_K2[0], 0, 0);
	diff_eq_int("SREv17_PLL_K2[0] is zero (%ld)", SREv17_PLL_K2[0], 0, 0);

	/*
	 * THE TWO SRE GAIN ARRAYS ARE EQUAL AND THE TWO CARRIER ONES ARE NOT.
	 * Asserted so that an edit to either half of either pair fails rather
	 * than passing quietly; the object has four distinct symbols here and
	 * only three distinct values.
	 */
	diff_eq_int("SREv17_PLL_K1 == SREv17_PLL_K1_S (%ld)",
		    shorts_differ(SREv17_PLL_K1, SREv17_PLL_K1_S,
				  FPM_SRE_MODES), 0, 0);
	diff_eq_int("CRRv17_PLL_K1 != CRRv17_PLL_K1_S (%ld)",
		    shorts_differ(CRRv17_PLL_K1, CRRv17_PLL_K1_S, 3), 1, 0);
	diff_eq_int("... and they differ in gear 0 only (%ld)",
		    CRRv17_PLL_K1[1] == CRRv17_PLL_K1_S[1] &&
		    CRRv17_PLL_K1[2] == CRRv17_PLL_K1_S[2] &&
		    CRRv17_PLL_K1[0] != CRRv17_PLL_K1_S[0], 1, 0);

	/*
	 * The clock legs are three phases 120 degrees apart in Q14: the X leg
	 * is (1, -1/2, -1/2) and the Y leg (0, +sqrt(3)/2, -sqrt(3)/2).  Both
	 * legs sum to zero, which is what makes the correlation a discriminant
	 * rather than a level measurement.
	 */
	diff_eq_int("xCLOCK sums to zero (%ld)",
		    SREv17_xCLOCK[0] + SREv17_xCLOCK[1] + SREv17_xCLOCK[2],
		    0, 0);
	diff_eq_int("yCLOCK sums to zero (%ld)",
		    SREv17_yCLOCK[0] + SREv17_yCLOCK[1] + SREv17_yCLOCK[2],
		    0, 0);
	diff_eq_int("xCLOCK[0] is 1.0 in Q14 (%ld)", SREv17_xCLOCK[0],
		    16384, 0);

	/*
	 * The two MTD banks are two five-short biquad sections each, sharing
	 * their first two coefficients across both sections -- and they are
	 * NOT the same bank, which a copy-paste between the two files would
	 * have made them.
	 */
	diff_eq_int("V17_MTD_COEFF sections share coeff 0 (%ld)",
		    V17_MTD_COEFF[0] == V17_MTD_COEFF[5], 1, 0);
	diff_eq_int("V17 and V21 banks differ (%ld)",
		    shorts_differ(V17_MTD_COEFF, V21_CHAN2_MTD_COEFF, 10),
		    1, 0);

	return diff_end();
}

/*
 * Layer 4, second half: the four constellations.  Two independent claims --
 * that each is a well-formed trellis point set, and that it is V.32bis'.
 */
static int
test_constellations(void)
{
	static const struct {
		const short *imap;
		const short *qmap;
		const short *v32i;
		const short *v32q;
		int n;
		int rate;
	} sets[] = {
		{ VTBv17_IMAP16T, VTBv17_QMAP16T,
		  VTBv32_IMAP16T, VTBv32_QMAP16T,  16,  7200 },
		{ VTBv17_IMAP32,  VTBv17_QMAP32,
		  VTBv32_IMAP32,  VTBv32_QMAP32,   32,  9600 },
		{ VTBv17_IMAP64,  VTBv17_QMAP64,
		  VTBv32_IMAP64,  VTBv32_QMAP64,   64, 12000 },
		{ VTBv17_IMAP128, VTBv17_QMAP128,
		  VTBv32_IMAP128, VTBv32_QMAP128, 128, 14400 }
	};
	int s;

	diff_begin("v17cfg: the four constellations are V.32bis' own");

	for (s = 0; s < 4; s++) {
		int n = sets[s].n;
		int i, j, distinct = 1, negclosed = 1, rotclosed = 1;

		/*
		 * IDENTITY WITH V.32bis, entry by entry.  This is the strongest
		 * check in the file that does not go through `ref_`: it links
		 * two table sets extracted in two different passes, from two
		 * different addresses, and a slip in either fails it.
		 */
		cmp_shorts("IMAP is VTBv32's (%ld)", sets[s].imap, sets[s].v32i,
			   n);
		cmp_shorts("QMAP is VTBv32's (%ld)", sets[s].qmap, sets[s].v32q,
			   n);

		/* The spare entry past the constellation is zero on both
		 * rails, in all four sets. */
		diff_eq_int("IMAP spare entry is zero (%ld)", sets[s].imap[n],
			    0, sets[s].rate);
		diff_eq_int("QMAP spare entry is zero (%ld)", sets[s].qmap[n],
			    0, sets[s].rate);

		/* The N points are distinct: a repeat is exactly what a
		 * transcription slip in a point table looks like. */
		for (i = 0; i < n; i++)
			for (j = i + 1; j < n; j++)
				if (sets[s].imap[i] == sets[s].imap[j] &&
				    sets[s].qmap[i] == sets[s].qmap[j])
					distinct = 0;
		diff_eq_int("the points are distinct (%ld)", distinct, 1,
			    sets[s].rate);

		/*
		 * And the set is closed under negation and under a 90-degree
		 * rotation, which every QAM constellation with four-way phase
		 * symmetry must be -- and which fixes the SIGNS, where a
		 * distinctness check cannot.
		 */
		for (i = 0; i < n; i++) {
			int negfound = 0, rotfound = 0;

			for (j = 0; j < n; j++) {
				if (sets[s].imap[j] == -sets[s].imap[i] &&
				    sets[s].qmap[j] == -sets[s].qmap[i])
					negfound = 1;
				if (sets[s].imap[j] == -sets[s].qmap[i] &&
				    sets[s].qmap[j] == sets[s].imap[i])
					rotfound = 1;
			}
			if (!negfound)
				negclosed = 0;
			if (!rotfound)
				rotclosed = 0;
		}
		diff_eq_int("closed under negation (%ld)", negclosed, 1,
			    sets[s].rate);
		diff_eq_int("closed under 90-degree rotation (%ld)", rotclosed,
			    1, sets[s].rate);
	}

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * Layer 5.  `V17RX_create` builds each configuration by copying the library
 * built-in and patching it; these helpers do exactly that, once per side, so
 * that the two inits see the same construction over different tables.
 *
 * `warm` is the flag the constructor reads out of the receiver object's +0x10
 * (the caller's parameter block +0x14): non-zero selects the `_S` gain array
 * and the shorter settle / train count, zero selects the plain one.
 */
static void
build_sre_cfg(struct fpm_sre_cfg *c, const struct fpm_sre_cfg *base,
	      const short *proto, const short *disc, const short *xclk,
	      const short *yclk, const short *k1, const short *k2, int warm)
{
	*c = *base;
	c->clock_len = 3;
	c->groups_acq = 3;
	c->groups_trk = 16;
	c->settle = (short)(warm ? 48 : 85);
	c->acc_down = 0x2000;
	c->acc_up = 0x4000;
	c->coeffs = 180;
	c->proto = proto;
	c->disc = disc;
	c->xclock = xclk;
	c->yclock = yclk;
	c->pll_k1 = k1;
	c->pll_k2 = k2;
	c->mag_hi = 2;
	c->mag_lo = 1;
	c->err_hi = 0x2666;
	c->err_lo = 200;
	c->rms_min = 0;
	c->rms_len = 9;
}

static void
build_fse_cfg(struct fpm_fse_cfg *c, const struct fpm_fse_cfg *base,
	      const short *icoff, const short *qcoff, const short *clk,
	      const short *k1, const short *k2, int warm)
{
	*c = *base;
	c->block = 144;
	c->interp = 3;
	c->icoff = icoff;
	c->qcoff = qcoff;
	c->taps = 49;
	c->mu[0] = 0;
	c->mu[1] = 0;
	c->clk = clk;
	c->clk_mod = 4;
	c->clk_inc = 1;
	c->train_sym = (short)(warm ? 256 : 1500);
	c->err_hi = 0x199a;
	c->err_lo = 0x666;
	c->pll_k1 = k1;
	c->pll_k2 = k2;
	c->owner = 0;
	c->decision = 0;
}

static int
test_use_agc(void)
{
	struct fpm_agc a, b;
	int reset;

	diff_begin("v17cfg: AGCv17_CFG through FPM_AGC_init");

	for (reset = 0; reset <= 1; reset++) {
		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));

		FPM_AGC_init(&a, &AGCv17_CFG, reset);
		ref_FPM_AGC_init(&b, &ref_AGCv17_CFG, reset);

		diff_eq_int("agc.cfg.ref_level (%ld)", a.cfg.ref_level,
			    b.cfg.ref_level, reset);
		diff_eq_int("agc.cfg.acquire_level (%ld)",
			    a.cfg.acquire_level, b.cfg.acquire_level, reset);
		diff_eq_int("agc.cfg.squelch_level (%ld)",
			    a.cfg.squelch_level, b.cfg.squelch_level, reset);
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

	diff_begin("v17cfg: the two MTD banks through FPM_MTD_create");

	/*
	 * `V17RX_create` calls `FPM_MTD_create` twice: once around
	 * `V17_MTD_COEFF` with a minimum level of 100, and once around
	 * `V21_CHAN2_MTD_COEFF` with 300.  Both carry `tones = 2` and the same
	 * Q15 ratio of 0x4ccd.
	 */
	for (which = 0; which < 2; which++) {
		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));
		a.acc = acc_a;
		b.acc = acc_b;

		ca = FPM_MTD_CFG;
		cb = ref_FPM_MTD_CFG;
		if (which == 0) {
			ca.coeff = V17_MTD_COEFF;
			cb.coeff = ref_V17_MTD_COEFF;
			ca.min_level = cb.min_level = 100;
		} else {
			ca.coeff = V21_CHAN2_MTD_COEFF;
			cb.coeff = ref_V21_CHAN2_MTD_COEFF;
			ca.min_level = cb.min_level = 300;
		}
		ca.tones = cb.tones = 2;
		ca.ratio = cb.ratio = 0x4ccd;

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
	int warm;

	diff_begin("v17cfg: the SRE tables through FPM_SRE_init");

	for (warm = 0; warm <= 1; warm++) {
		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));

		build_sre_cfg(&ca, &FPM_SRE_CFG, SREv17_COFFS,
			      SREv17_XB_COFFS, SREv17_xCLOCK, SREv17_yCLOCK,
			      warm ? SREv17_PLL_K1_S : SREv17_PLL_K1,
			      SREv17_PLL_K2, warm);
		build_sre_cfg(&cb, &ref_FPM_SRE_CFG, ref_SREv17_COFFS,
			      ref_SREv17_XB_COFFS, ref_SREv17_xCLOCK,
			      ref_SREv17_yCLOCK,
			      warm ? ref_SREv17_PLL_K1_S : ref_SREv17_PLL_K1,
			      ref_SREv17_PLL_K2, warm);

		FPM_SRE_init(&a, &ca, 1);
		ref_FPM_SRE_init(&b, &cb, 1);

		diff_eq_int("sre.taps (%ld)", a.taps, b.taps, warm);
		diff_eq_int("sre.mode (%ld)", a.mode, b.mode, warm);
		diff_eq_int("sre.groups (%ld)", a.groups, b.groups, warm);
		diff_eq_int("sre.settle (%ld)", a.settle, b.settle, warm);
		diff_eq_int("sre.need (%ld)", a.need, b.need, warm);
		diff_eq_int("sre.rms_on (%ld)", a.rms_on, b.rms_on, warm);
		diff_eq_int("sre.acquiring (%ld)", a.acquiring, b.acquiring,
			    warm);
		diff_eq_int("sre.cfg.coeffs (%ld)", a.cfg.coeffs, b.cfg.coeffs,
			    warm);

		/*
		 * THE POINT OF THIS BLOCK.  `FPM_SRE_init` copies `cfg.coeffs`
		 * entries out of `proto` into its own buffer, so this compares
		 * 180 shorts that reached the state THROUGH the count
		 * `V17RX_create` wrote -- a table of the right bytes and the
		 * wrong length cannot pass it.
		 */
		cmp_shorts("sre.coeff[%ld]", a.coeff, b.coeff, 180);

		cmp_shorts("sre.cfg.disc[%ld]", a.cfg.disc, b.cfg.disc,
			   FPM_SRE_DISC);
		cmp_shorts("sre.cfg.xclock[%ld]", a.cfg.xclock, b.cfg.xclock,
			   3);
		cmp_shorts("sre.cfg.yclock[%ld]", a.cfg.yclock, b.cfg.yclock,
			   3);
		cmp_shorts("sre.cfg.pll_k1[%ld]", a.cfg.pll_k1, b.cfg.pll_k1,
			   FPM_SRE_MODES);
		cmp_shorts("sre.cfg.pll_k2[%ld]", a.cfg.pll_k2, b.cfg.pll_k2,
			   FPM_SRE_MODES);
		/* proto's last entry is past `coeffs` and is read only by the
		 * interpolator, so it never reaches `sre.coeff`. */
		diff_eq_int("sre.cfg.proto[180] (%ld)", a.cfg.proto[180],
			    b.cfg.proto[180], warm);

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
	int warm;

	diff_begin("v17cfg: the FSE tables through FPM_FSE_init");

	for (warm = 0; warm <= 1; warm++) {
		memset(&a, 0xa5, sizeof(a));
		memset(&b, 0xa5, sizeof(b));

		build_fse_cfg(&ca, &FPM_FSE_CFG, FSEv17_ICOFF, FSEv17_QCOFF,
			      CRRv17_CLK,
			      warm ? CRRv17_PLL_K1_S : CRRv17_PLL_K1,
			      CRRv17_PLL_K2, warm);
		build_fse_cfg(&cb, &ref_FPM_FSE_CFG, ref_FSEv17_ICOFF,
			      ref_FSEv17_QCOFF, ref_CRRv17_CLK,
			      warm ? ref_CRRv17_PLL_K1_S : ref_CRRv17_PLL_K1,
			      ref_CRRv17_PLL_K2, warm);

		FPM_FSE_init(&a, &ca, 1);
		ref_FPM_FSE_init(&b, &cb, 1);

		diff_eq_int("fse.cfg.block (%ld)", a.cfg.block, b.cfg.block,
			    warm);
		diff_eq_int("fse.cfg.taps (%ld)", a.cfg.taps, b.cfg.taps, warm);
		diff_eq_int("fse.cfg.clk_mod (%ld)", a.cfg.clk_mod,
			    b.cfg.clk_mod, warm);
		diff_eq_int("fse.cfg.train_sym (%ld)", a.cfg.train_sym,
			    b.cfg.train_sym, warm);
		diff_eq_int("fse.mu_sel (%ld)", a.mu_sel, b.mu_sel, warm);
		diff_eq_int("fse.pll_sel (%ld)", a.pll_sel, b.pll_sel, warm);
		diff_eq_int("fse.widx (%ld)", a.widx, b.widx, warm);
		diff_eq_int("fse.need (%ld)", a.need, b.need, warm);

		/* The coefficients that reached the state through `cfg.taps`. */
		cmp_shorts("fse.icoeff[%ld]", a.icoeff, b.icoeff, 49);
		cmp_shorts("fse.qcoeff[%ld]", a.qcoeff, b.qcoeff, 49);
		cmp_shorts("fse.cfg.clk[%ld]", a.cfg.clk, b.cfg.clk, 4);
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

	diff_begin("v17cfg: MRFv17_COFFS through FPM_MRF_init");

	memset(&a, 0xa5, sizeof(a));
	memset(&b, 0xa5, sizeof(b));

	/* As `V17RX_create` builds it: 9 branches, decimate 10, 360 taps. */
	ca = FPM_MRF_CFG;
	cb = FPM_MRF_CFG;
	ca.branches = cb.branches = 9;
	ca.decimate = cb.decimate = 10;
	ca.taps = cb.taps = 360;
	ca.coeff = MRFv17_COFFS;
	cb.coeff = ref_MRFv17_COFFS;
	ca.aux = cb.aux = 0;

	FPM_MRF_init(&a, &ca, 1);
	ref_FPM_MRF_init(&b, &cb, 1);

	diff_eq_int("mrf.cfg.branches (%ld)", a.cfg.branches, b.cfg.branches,
		    0);
	diff_eq_int("mrf.cfg.decimate (%ld)", a.cfg.decimate, b.cfg.decimate,
		    0);
	diff_eq_int("mrf.cfg.taps (%ld)", a.cfg.taps, b.cfg.taps, 0);
	diff_eq_int("mrf.history_len (%ld)", a.history_len, b.history_len, 0);
	diff_eq_int("mrf.history_len is taps/branches (%ld)",
		    a.history_len, 40, 0);
	diff_eq_int("mrf.need (%ld)", a.need, b.need, 0);
	diff_eq_int("mrf.phase (%ld)", a.phase, b.phase, 0);
	diff_eq_int("mrf.widx (%ld)", a.widx, b.widx, 0);
	cmp_shorts("mrf.cfg.coeff[%ld]", a.cfg.coeff, b.cfg.coeff, 360);
	cmp_shorts("mrf.history[%ld]", a.history, b.history, 40);

	sysdep_free(a.history);
	sysdep_free(b.history);

	return diff_end();
}

/*
 * Layer 5, the constellations: `V17RX_create` fills a `struct vtb` by hand --
 * there is no `VTBv17_init` -- so this reproduces that fill for each of the
 * four bit rates and drives `VTB_decoder`, ours over our maps and the blob's
 * over its own.  A map that reproduced the bytes but was read at the wrong
 * stride would decode different symbols here.
 */
static unsigned seed;

static int
rnd(int n)
{
	seed = seed * 1103515245u + 12345u;
	return (int)((seed >> 16) % (unsigned)n);
}

static void
build_vtb(struct vtb *v, struct vtb_path *ring, int nsub,
	  const short *imap, const short *qmap,
	  const short *bound, const short *region)
{
	int k;

	memset(v, 0, sizeof(*v));
	v->paths = ring;
	for (k = 0; k < 16 * 8; k++) {
		ring[k].sym = 0;
		ring[k].surv = 0;
	}
	for (k = 0; k < 8; k++)
		v->metric[k] = 0;
	v->ring = 0;
	v->imap = imap;
	v->qmap = qmap;
	v->bound = bound;
	v->region = region;
	v->nsub = (unsigned short)nsub;
	v->grid = (short)(2 * nsub);
	v->depth = 16;
	v->prev = 0;
	v->mask = (unsigned short)((1 << (nsub + 2)) - 1);
	v->shift = (short)nsub;
}

static int
test_use_vtb(void)
{
	static struct vtb_path ring_a[16 * 8], ring_b[16 * 8];
	static struct vtb va, vb;
	int rate;

	diff_begin("v17cfg: the constellations through VTB_decoder");

	seed = 20260901u;

	for (rate = 0; rate < 4; rate++) {
		int k, j;

		switch (rate) {
		case 0:		/* 7200 bit/s, nsub 1, 16 rotated points */
			build_vtb(&va, ring_a, 1, VTBv17_IMAP16T,
				  VTBv17_QMAP16T, VTB_BOUND_7200,
				  VTB_REGION_7200);
			build_vtb(&vb, ring_b, 1, ref_VTBv17_IMAP16T,
				  ref_VTBv17_QMAP16T, ref_VTB_BOUND_7200,
				  ref_VTB_REGION_7200);
			break;
		case 1:		/* 9600 bit/s, nsub 2, 32 points */
			build_vtb(&va, ring_a, 2, VTBv17_IMAP32,
				  VTBv17_QMAP32, VTB_BOUND_9600,
				  VTB_REGION_9600);
			build_vtb(&vb, ring_b, 2, ref_VTBv17_IMAP32,
				  ref_VTBv17_QMAP32, ref_VTB_BOUND_9600,
				  ref_VTB_REGION_9600);
			break;
		case 2:		/* 12000 bit/s, nsub 3, 64 rotated points */
			build_vtb(&va, ring_a, 3, VTBv17_IMAP64,
				  VTBv17_QMAP64, VTB_BOUND_12000,
				  VTB_REGION_12000);
			build_vtb(&vb, ring_b, 3, ref_VTBv17_IMAP64,
				  ref_VTBv17_QMAP64, ref_VTB_BOUND_12000,
				  ref_VTB_REGION_12000);
			break;
		default:	/* 14400 bit/s, nsub 4, 128 points */
			build_vtb(&va, ring_a, 4, VTBv17_IMAP128,
				  VTBv17_QMAP128, VTB_BOUND_14400,
				  VTB_REGION_14400);
			build_vtb(&vb, ring_b, 4, ref_VTBv17_IMAP128,
				  ref_VTBv17_QMAP128, ref_VTB_BOUND_14400,
				  ref_VTB_REGION_14400);
			break;
		}

		for (k = 0; k < 48; k++) {
			short got = -1, want = -1;
			int i, q;

			/*
			 * Points over the whole plane the equaliser can
			 * produce, not only near constellation points: the
			 * region clamp and the quadrant split both need the
			 * corners.  Every eighth trial lands exactly on a
			 * decision boundary.
			 */
			i = rnd(60000) - 30000;
			q = rnd(60000) - 30000;
			if ((k & 7) == 0) {
				i = (rnd(9) - 4) * 4096;
				q = (rnd(9) - 4) * 4096;
			}

			VTB_decoder(&va, (short)i, (short)q, &got);
			ref_VTB_decoder(&vb, (short)i, (short)q, &want);

			diff_eq_int("decoded symbol (trial %ld)", got, want,
				    rate * 1000 + k);
			diff_eq_int("vtb.ring (trial %ld)", va.ring, vb.ring,
				    rate * 1000 + k);
			diff_eq_int("vtb.prev (trial %ld)", va.prev, vb.prev,
				    rate * 1000 + k);
			for (j = 0; j < 8; j++)
				diff_eq_int("vtb.metric (trial.state %ld)",
					    va.metric[j], vb.metric[j],
					    (rate * 1000 + k) * 8 + j);
		}

		/* The whole survivor ring, once, at the end of the run. */
		for (j = 0; j < 16 * 8; j++) {
			diff_eq_int("ring sym (rate.node %ld)",
				    ring_a[j].sym, ring_b[j].sym,
				    rate * 1000 + j);
			diff_eq_int("ring surv (rate.node %ld)",
				    ring_a[j].surv, ring_b[j].surv,
				    rate * 1000 + j);
		}
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
	rc |= test_constellations();
	rc |= test_use_agc();
	rc |= test_use_mtd();
	rc |= test_use_sre();
	rc |= test_use_fse();
	rc |= test_use_mrf();
	rc |= test_use_vtb();

	return rc;
}
