/*
 * v22rxtab.c -- V.22 / V.22bis: the receiver's static configuration.
 *
 * Extracted from dsplibs.o:
 *
 *   AGCv22_CFG               .rodata 0x008bf4   24    two pointers
 *   AGCv22_CFG2              .rodata 0x008bdc   24    two pointers
 *   MTDs1_CFG                .rodata 0x0083a0   12    one pointer
 *   MTDv22_CFG               .rodata 0x0083b8   12    one pointer
 *   MTDv22_CFG2              .rodata 0x0083ac   12    one pointer
 *   V22_S1_HC_COEF           .rodata 0x0083e0   40
 *   MTDv22_COEF              .rodata 0x008426   30
 *   MTDv22_COEF2             .rodata 0x008408   30
 *   TONEv22_CFG              .rodata 0x0084e0   36
 *   TONEv22INIT_CFG          .rodata 0x008520   36
 *   V22_CFG                  .rodata 0x0084c4   28
 *   CRRv22_CLK               .rodata 0x0086ac   12
 *   CRRv22_PLL_K1            .rodata 0x0086be    6
 *   CRRv22_PLL_K2            .rodata 0x0086b8    6
 *   V22DiconnectThreshTable  .data   0x0077a8   16
 *
 * The five configurations that carry relocations are written as the structs
 * the relocations prove them to be; the rest stay arrays.  See v22tab.h.
 *
 * ---------------------------------------------------------------------------
 * V.22 IS THE TRANSLATION UNIT THAT GOT THE SLOW SMOOTHER RIGHT
 *
 * `AGC_DEF_ALPHA` and `AGC_DEF_BETA` are file-static in the original and
 * exist six times over, once per translation unit that wanted them, with
 * different contents.  D6 records that a first-order smoother
 * `y += alpha*y + beta*x` has unity DC gain only when alpha + beta == 32768,
 * that element 0 satisfies that everywhere, and that element 1 -- the slow
 * "tracking" pair -- does not, in four of the five copies.
 *
 * THIS is the copy that does: .data 0x77bc / 0x77b8 hold 32604 and 164, and
 * 32604 + 164 = 32768 exactly.  b103_agc_cfg.c's table already named this
 * address as the correct one; naming the TU it belongs to closes that loop.
 * It changes nothing observable -- element 1 is selected by nothing in the
 * whole object, here as everywhere else -- but it identifies which TU the
 * other four were copied FROM, which is the direction the defect travelled.
 *
 * ---------------------------------------------------------------------------
 * THE TWO TONE CONFIGURATIONS ARE BYTE-IDENTICAL
 *
 * `TONEv22_CFG` and `TONEv22INIT_CFG` differ in name, in address, and in
 * nothing else: all 36 bytes agree.  Both are file-static, so this is not a
 * linker artefact -- the author wrote the same 36 bytes twice under two
 * names.  The differential test asserts the identity rather than assuming it,
 * so if this is ever read as "one is a typo for the other" the object says
 * otherwise.
 *
 * Neither carries a relocation, so the pointer field a `struct fpm_tone_cfg`
 * has at +0x10 is NULL in both.  THE FIELD MAPPING IS CORROBORATED rather
 * than assumed: word for word against the blob's own `FPM_TONE_CFG`, whose
 * layout this tree already reconstructed and tested, `freq` is 2100 in both,
 * `f08` is 328 in both, `min_level` is 1 in both, and `len` is 53 in both --
 * so +0x14 really is `len`, and the NULL at +0x10 really is `src`.
 * `FPM_TONE_create` copies `cfg.src` into the object's own kernel using
 * `cfg.len`, so a create against either of these as they stand would read 53
 * words from address zero.
 *
 * The two configurations diverge at the tail: b103's carries 16384 at +0x1c
 * and 40 at +0x1e, V.22's carries 40 at +0x1c and 0 at +0x1e -- the same two
 * values shifted one word earlier.  Both fields are unread by everything
 * reconstructed so far, so which is intended is not decidable here.  Noted
 * because it is exactly the shape a copy-and-edit slip leaves, and because
 * an unrecorded observation is unrecoverable.
 * The FPM_MRF configs solve the same problem by having the caller copy the
 * static onto the stack and patch the pointer before use (see fpm_mrf.h), and
 * that is the natural reading here -- but `V22FP_create` is not reconstructed
 * yet, so it stays a reading and these stay untyped arrays.
 */

#include "dsplib/v22tab.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"

/*
 * { acquisition, tracking }.  Element 0 is what every config selects; see the
 * note above for element 1.
 */
static const short AGC_DEF_ALPHA[2] = { 16384, 32604 };
static const short AGC_DEF_BETA[2] = { 16384, 164 };

/*
 * The two AGC configurations.  The relocations at +0x0c and +0x10 are what
 * make `alpha` and `beta` pointers rather than the coefficients 30652 and
 * 30648 that an int16 dump shows.
 */
const struct fpm_agc_cfg AGCv22_CFG = {
	.ref_level = 17000,	/* output settles at half this: 8500 */
	.acquire_level = 32,
	.squelch_level = 1,
	.f06 = 1000,
	.f08 = 2621,
	.block_len = 36,
	.alpha = AGC_DEF_ALPHA,
	.beta = AGC_DEF_BETA,
	.f14 = 158
};

const struct fpm_agc_cfg AGCv22_CFG2 = {
	.ref_level = 12000,	/* output settles at half this: 6000 */
	.acquire_level = 32,
	.squelch_level = 1,
	.f06 = 1000,
	.f08 = 2621,
	.block_len = 40,
	.alpha = AGC_DEF_ALPHA,
	.beta = AGC_DEF_BETA,
	.f14 = 158
};

/*
 * The tone-detector banks.  Five words per biquad section, in the same
 * { b0, b2, b1, a2, a1 } order every other MTD bank in this object uses.
 *
 * `_COEF` and `_COEF2` differ in exactly two words -- section 0's b2 and b1,
 * -5530/5533 against 4920/-4924 -- so they are the same three-tone bank with
 * one resonator retuned.  Sections 1 and 2 are identical between them.
 */
const short MTDv22_COEF[V22_MTD_SECTIONS * FPM_IIR_COEFF_PER_SECTION] = {
	-15099, 15739, -5530, 5533, 15739,
	-15099, 15739, -6137, 6141, 15739,
	-15099, 15736, -19474, 19484, 15736,
};

const short MTDv22_COEF2[V22_MTD_SECTIONS * FPM_IIR_COEFF_PER_SECTION] = {
	-15099, 15739, 4920, -4924, 15739,
	-15099, 15739, -6137, 6141, 15739,
	-15099, 15736, -19474, 19484, 15736,
};

/* Four sections, and the bank MTDs1_CFG points at. */
const short V22_S1_HC_COEF[V22_S1_SECTIONS * FPM_IIR_COEFF_PER_SECTION] = {
	-13271, 16384, 0, 0, 16384,
	-13271, 16384, -14746, 16384, 16384,
	-13271, 16384, -25540, 28378, 16384,
	-13271, 16384, 14746, -16384, 16384,
};

/* The pointer at +0x00 is the relocation; the rest are scalars. */
const struct fpm_mtd_cfg MTDv22_CFG = {
	.coeff = MTDv22_COEF,
	.tones = V22_MTD_SECTIONS,
	.ratio = 29820,
	.min_level = 10,
	.f0a = 0
};

const struct fpm_mtd_cfg MTDv22_CFG2 = {
	.coeff = MTDv22_COEF2,
	.tones = V22_MTD_SECTIONS,
	.ratio = 27980,
	.min_level = 10,
	.f0a = 0
};

const struct fpm_mtd_cfg MTDs1_CFG = {
	.coeff = V22_S1_HC_COEF,
	.tones = V22_S1_SECTIONS,
	.ratio = 19384,
	.min_level = 10,
	.f0a = 0
};

/*
 * Carrier recovery.  Six equally spaced phases, and the spacing is NOT a
 * sixth of 0x8000.
 *
 * `round(k * 32768 / 6)` reproduces five of the six entries and gives 27307
 * where the object has 27306.  `round(k * 65535 / 12)` -- that is,
 * `round(k * 5461.25)` -- reproduces all six, and k = 5 is the only entry
 * where the two ever differ.
 *
 * THAT IS A FIT, NOT A RECOVERED DESIGN.  Two parameters over six points
 * establishes very little, and a design would have to come from a rate;
 * coefficient derivations are deferred here (docs/fastpass.md).  Both
 * statements are asserted in t_v22tab, the second in the negative, so the
 * exact fit is a transcription guard and the wrong reading cannot quietly
 * become the received one.  Neither check is differential -- both sides read
 * the original's copy -- so the differential tier says nothing about either.
 *
 * What the six phases are FOR is not settled: the code that indexes this is
 * not reconstructed yet, and V.22bis's constellation has four points, not
 * six, so the obvious reading is wrong.  Recorded as an observation.
 */
const short CRRv22_CLK[V22_CRR_CLK_STEPS] = {
	0, 5461, 10923, 16384, 21845, 27306,
};

/*
 * Loop gains, three sets.  K2 is zero in set 0, which makes that set a
 * first-order (frequency-blind) loop and the other two second order.
 */
const short CRRv22_PLL_K1[V22_CRR_PLL_SETS] = { 2928, 5856, 5856 };
const short CRRv22_PLL_K2[V22_CRR_PLL_SETS] = { 0, 262, 262 };

/*
 * Eight rising thresholds, in .data rather than .rodata -- so the original
 * declared them without `const` -- and nothing in the object writes them.
 */
short V22DiconnectThreshTable[V22_DISCONNECT_THRESHOLDS] = {
	75, 95, 119, 150, 168, 174, 212, 238,
};

/*
 * The tone configurations.  Byte-identical to each other; see the header
 * comment for why they are arrays and not `struct fpm_tone_cfg`.
 *
 * Laid out here at the offsets that struct would give, purely so the values
 * can be read: 2100 Hz, scale 11587, no phase reversal, detector ratio 2981,
 * minimum level 1, damping 31457, a NULL correlator prototype, length 53.
 */
const short TONEv22_CFG[V22_TONE_CFG_WORDS] = {
	2100, 11587, 0, 2981, 328, 1, 31457, 0,
	0, 0, 53, 0, 0, 0, 40, 0,
	0, 0,
};

const short TONEv22INIT_CFG[V22_TONE_CFG_WORDS] = {
	2100, 11587, 0, 2981, 328, 1, 31457, 0,
	0, 0, 53, 0, 0, 0, 40, 0,
	0, 0,
};

/*
 * The datapump's own parameter block.  Untyped: no relocation constrains it
 * and no reconstructed code reads it.  The two 2400s and the 103 are
 * legible -- the symbol rate twice over, and a Bell 103 datapump id -- and
 * nothing else here is, so nothing else is named.
 */
const short V22_CFG[V22_CFG_WORDS] = {
	1, 2400, 2400, 0, -11072, 1, 13014, 0,
	1627, 0, 1, 103, 0, 0,
};
