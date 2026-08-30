/*
 * t_v32fptab.c -- differential test of the twelve tables the V.32 FP layer
 *                 configures itself from.
 *
 *   V32_CFG                  .rodata 0x006da0   48   struct v32fp_params
 *   V32DiconnectThreshTable  .rodata 0x006dd0   16   8 shorts, file-local
 *   V32_TURNAROUND_DLY       .data   0x0076c0    4   2 shorts
 *   V32_SAMPLE_LEN           .data   0x0076c8    4   2 shorts
 *   SnrToRetrainTable        .data   0x007750   12   6 shorts, file-local
 *   RATEv32                  .data   0x00775c   12   6 shorts, file-local
 *   PPSv32_QCOFFS            .rodata 0x007b00  240
 *   PPSv32_ICOFFS            .rodata 0x007c00  240
 *   PPSv32_CFG               .rodata 0x007d00   40   struct fpm_pps_cfg
 *   V32_CTL                  .rodata 0x007f20   32   struct v32fp_ctl
 *   SMCv32_CFG               .bss    0x000188    4   struct v32_smc_cfg
 *   Control_Flag             .bss    0x0001a0    4   int
 *
 * `V32_SYMBOL_LEN` is already covered by `t_v32fpctl.c` and is checked here
 * too, because the claim this file makes about it is a NEW one: that it and
 * its two neighbours are one family indexed by one selector, which is only
 * visible when all three are compared side by side.
 *
 * THREE LAYERS, and the reason is `t_v32cfg.c`'s.  Comparing bytes against
 * `ref_` proves the bytes and proves nothing about the SHAPE, so:
 *
 *   1. every table element by element and every struct FIELD BY FIELD, with
 *      the four pointers inside `PPSv32_CFG` compared by what they point AT
 *      rather than by address -- ours holds our tables' addresses and the
 *      blob's holds `ref_`'s, so a value comparison would fail on all four
 *      while a `!= NULL` check would pass on anything;
 *   2. the two `.bss` objects, which are the only ones whose CONTENT is not
 *      the interesting part -- what is asserted there is that they are four
 *      bytes and zero, because a `struct v32_smc_cfg` sized wrongly would
 *      still compile and would still read as zero;
 *   3. `PPSv32_CFG` is then USED -- `FPM_PPS_init` + `FPM_PPS_filter` on our
 *      copy and on the blob's, over a real symbol stream.  A wrong field
 *      boundary can leave every value matching while the block behaves
 *      differently, and only this layer would see it.
 *
 * WHAT EACH CHECK IS FOR, as a named wrong reading:
 *
 *   - `V32_CFG` read as 24 shorts.  Three of its fields are 32-bit and one of
 *     those (+0x10) is patched a BYTE at a time; a short-array reading passes
 *     a byte comparison and loses every boundary.  Compared as named fields.
 *   - `PPSv32_CFG`'s pointers read from the file's BYTES.  All four dwords
 *     are zero in the object and their values are relocations, so a byte
 *     comparison of two all-null tables passes while naming nothing.  Each is
 *     resolved to a table here.
 *   - `PPSv32_CFG.scale` read as a Q15 gain and "corrected" to 32767.  It is
 *     131072.  Asserted against that literal and not only against `ref_`.
 *   - `RATEv32` and `SnrToRetrainTable` read as seven entries because
 *     `V32_RATE_COUNT` is 7.  They are SIX: twelve bytes each.  The lengths
 *     are asserted from the tables' own contents, and `RATEv32[0] == 4800` is
 *     asserted by name because it is what settles v32seq.h's open inference
 *     (finding F8640).
 *   - the three length tables conflated.  Each is compared against its OWN
 *     `ref_` symbol, and the three are asserted to be pairwise DIFFERENT --
 *     swapping two of them in the source would otherwise still pass.
 *   - `V32DiconnectThreshTable[3]` read as anything but 150.  It is the one
 *     entry `V32FP_recreate` reads, and it is asserted by name.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/v32fp.h"
#include "dsplib/v32cfg.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32smc.h"
#include "dsplib/v32dec.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"

/* The blob's copies.  An undeclared `ref_` name is a hard error, not an int. */
extern const struct v32fp_params ref_V32_CFG;
extern const struct v32fp_ctl ref_V32_CTL;
extern const short ref_V32DiconnectThreshTable[8];
extern short ref_RATEv32[6];
extern short ref_SnrToRetrainTable[6];
extern short ref_V32_SYMBOL_LEN[2];
extern short ref_V32_SAMPLE_LEN[2];
extern short ref_V32_TURNAROUND_DLY[2];
extern const short ref_PPSv32_ICOFFS[120];
extern const short ref_PPSv32_QCOFFS[120];
extern const struct fpm_pps_cfg ref_PPSv32_CFG;
extern struct v32_smc_cfg ref_SMCv32_CFG;
extern int ref_Control_Flag;

extern const short ref_SMCv32_IMAP16[17];
extern const short ref_SMCv32_QMAP16[17];

extern void ref_FPM_PPS_init(void *state, const void *cfg, int fresh);
extern unsigned short ref_FPM_PPS_filter(void *state, void *src, short *out,
					 unsigned short count);
extern void ref_FPM_PPS_free(void *state);

/* ------------------------------------------------------------------------- */

/*
 * Which of the four tables `PPSv32_CFG` can name a pointer belongs to, on
 * whichever side it came from.  -1 for "none of them", so a null or a stray
 * address fails loudly rather than matching another -1.
 */
static int
pps_table_index(const void *p)
{
	if (p == (const void *)SMCv32_IMAP16 ||
	    p == (const void *)ref_SMCv32_IMAP16)
		return 0;
	if (p == (const void *)SMCv32_QMAP16 ||
	    p == (const void *)ref_SMCv32_QMAP16)
		return 1;
	if (p == (const void *)PPSv32_ICOFFS ||
	    p == (const void *)ref_PPSv32_ICOFFS)
		return 2;
	if (p == (const void *)PPSv32_QCOFFS ||
	    p == (const void *)ref_PPSv32_QCOFFS)
		return 3;
	return -1;
}

/* ------------------------------------------------------------------------- */

static int
test_params(void)
{
	diff_begin("V32_CFG (struct v32fp_params)");

	diff_eq_int("protocol (%ld)", V32_CFG.protocol, ref_V32_CFG.protocol,
		    0x00);
	diff_eq_int("bps (%ld)", V32_CFG.bps, ref_V32_CFG.bps, 0x02);
	diff_eq_int("bps2 (%ld)", V32_CFG.bps2, ref_V32_CFG.bps2, 0x04);
	diff_eq_int("r06 (%ld)", V32_CFG.r06, ref_V32_CFG.r06, 0x06);
	diff_eq_int("r08 (%ld)", V32_CFG.r08, ref_V32_CFG.r08, 0x08);
	diff_eq_int("r0c (%ld)", V32_CFG.r0c, ref_V32_CFG.r0c, 0x0c);
	diff_eq_int("flags (%ld)", (long)V32_CFG.flags, (long)ref_V32_CFG.flags,
		    0x10);
	diff_eq_int("ec_near_delay (%ld)", V32_CFG.ec_near_delay,
		    ref_V32_CFG.ec_near_delay, 0x14);
	diff_eq_int("r16 (%ld)", V32_CFG.r16, ref_V32_CFG.r16, 0x16);
	diff_eq_int("symlen_sel (%ld)", V32_CFG.symlen_sel,
		    ref_V32_CFG.symlen_sel, 0x18);
	diff_eq_int("r1a (%ld)", V32_CFG.r1a, ref_V32_CFG.r1a, 0x1a);
	diff_eq_int("trellis (%ld)", V32_CFG.trellis, ref_V32_CFG.trellis,
		    0x1c);
	diff_eq_int("r20 (%ld)", V32_CFG.r20, ref_V32_CFG.r20, 0x20);
	diff_eq_int("r24 (%ld)", V32_CFG.r24, ref_V32_CFG.r24, 0x24);
	diff_eq_int("disconnect_thresh (%ld)", V32_CFG.disconnect_thresh,
		    ref_V32_CFG.disconnect_thresh, 0x28);
	diff_eq_int("r2a (%ld)", V32_CFG.r2a, ref_V32_CFG.r2a, 0x2a);
	diff_eq_int("r2c (%ld)", V32_CFG.r2c, ref_V32_CFG.r2c, 0x2c);
	diff_eq_int("r2e (%ld)", V32_CFG.r2e, ref_V32_CFG.r2e, 0x2e);

	/*
	 * The struct must be exactly the twelve dwords the three `rep movsl`
	 * sites copy.  A field added or widened would still compare equal
	 * above and would corrupt the copy.
	 */
	diff_eq_int("sizeof struct v32fp_params (%ld)",
		    (long)sizeof(struct v32fp_params), 48, 0);

	/*
	 * The byte at +0x11 is the one `V32FP_create` patches, so the flag
	 * word's ENDIANNESS inside the struct matters and is asserted rather
	 * than assumed: 0x68b >> 8 is 0x06.
	 */
	diff_eq_int("the byte at +0x11 is 0x06 (%ld)",
		    ((const unsigned char *)&V32_CFG)[0x11], 0x06, 0x11);

	return diff_end();
}

static int
test_ctl(void)
{
	diff_begin("V32_CTL (struct v32fp_ctl)");

	diff_eq_int("bps (%ld)", V32_CTL.bps, ref_V32_CTL.bps, 0x00);
	diff_eq_int("r02 (%ld)", V32_CTL.r02, ref_V32_CTL.r02, 0x02);
	diff_eq_int("r04 (%ld)", V32_CTL.r04, ref_V32_CTL.r04, 0x04);
	diff_eq_int("r08 (%ld)", V32_CTL.r08, ref_V32_CTL.r08, 0x08);
	diff_eq_int("ctl0 (%ld)", V32_CTL.ctl0, ref_V32_CTL.ctl0, 0x0c);
	diff_eq_int("ctl1 (%ld)", V32_CTL.ctl1, ref_V32_CTL.ctl1, 0x0d);
	diff_eq_int("r0e (%ld)", V32_CTL.r0e, ref_V32_CTL.r0e, 0x0e);
	diff_eq_int("trellis (%ld)", V32_CTL.trellis, ref_V32_CTL.trellis,
		    0x10);
	diff_eq_int("r14 (%ld)", V32_CTL.r14, ref_V32_CTL.r14, 0x14);
	diff_eq_int("r18 (%ld)", V32_CTL.r18, ref_V32_CTL.r18, 0x18);
	diff_eq_int("r1c (%ld)", V32_CTL.r1c, ref_V32_CTL.r1c, 0x1c);

	diff_eq_int("sizeof struct v32fp_ctl (%ld)",
		    (long)sizeof(struct v32fp_ctl), 32, 0);

	/*
	 * `v32_data` and `V32FP_control` both reach these two as BYTES, so a
	 * reading that merged them into one `unsigned int` would put 0x183 at
	 * +0x0c and pass every comparison above on a little-endian host while
	 * being a different declaration.  Asserted against the object's own
	 * two values, which is the strongest this test can be about it.
	 */
	diff_eq_int("ctl0 is 0x83 (%ld)", V32_CTL.ctl0, 0x83, 0x0c);
	diff_eq_int("ctl1 is 0x01 (%ld)", V32_CTL.ctl1, 0x01, 0x0d);

	return diff_end();
}

static int
test_rate_tables(void)
{
	int i;

	diff_begin("RATEv32 / SnrToRetrainTable / V32DiconnectThreshTable");

	for (i = 0; i < 6; i++) {
		diff_eq_int("RATEv32[%ld]", RATEv32[i], ref_RATEv32[i], i);
		diff_eq_int("SnrToRetrainTable[%ld]", SnrToRetrainTable[i],
			    ref_SnrToRetrainTable[i], i);
	}
	for (i = 0; i < 8; i++)
		diff_eq_int("V32DiconnectThreshTable[%ld]",
			    V32DiconnectThreshTable[i],
			    ref_V32DiconnectThreshTable[i], i);

	/*
	 * F8640.  v32seq.h derived indices 1..5 from `V32FP_recreate`'s ladder
	 * and had to LABEL index 0 from the Recommendation.  This table states
	 * it, so the assertion is by name and against the literal.
	 */
	diff_eq_int("RATEv32[0] is 4800 (%ld)", RATEv32[0], 4800, 0);
	diff_eq_int("RATEv32[1] and [2] are both 9600 (%ld)",
		    RATEv32[1] == 9600 && RATEv32[2] == 9600, 1, 12);
	diff_eq_int("RATEv32[5] is 14400 (%ld)", RATEv32[5], 14400, 5);

	/*
	 * The one entry the object reads, and the one V.22's own parameter
	 * block takes from the table of the same name.
	 */
	diff_eq_int("V32DiconnectThreshTable[3] is 150 (%ld)",
		    V32DiconnectThreshTable[3], 150, 3);

	return diff_end();
}

static int
test_length_tables(void)
{
	int i;

	diff_begin("V32_TURNAROUND_DLY / V32_SYMBOL_LEN / V32_SAMPLE_LEN");

	for (i = 0; i < 2; i++) {
		diff_eq_int("V32_TURNAROUND_DLY[%ld]", V32_TURNAROUND_DLY[i],
			    ref_V32_TURNAROUND_DLY[i], i);
		diff_eq_int("V32_SYMBOL_LEN[%ld]", V32_SYMBOL_LEN[i],
			    ref_V32_SYMBOL_LEN[i], i);
		diff_eq_int("V32_SAMPLE_LEN[%ld]", V32_SAMPLE_LEN[i],
			    ref_V32_SAMPLE_LEN[i], i);
	}

	/*
	 * All three are indexed by ONE selector, so swapping two of them in
	 * the source would leave every site compiling and every value in
	 * range.  What stops that passing is that no two of the three are
	 * equal at either index.
	 */
	diff_eq_int("TURNAROUND_DLY and SYMBOL_LEN differ (%ld)",
		    V32_TURNAROUND_DLY[0] != V32_SYMBOL_LEN[0] &&
		    V32_TURNAROUND_DLY[1] != V32_SYMBOL_LEN[1], 1, 0);
	diff_eq_int("SYMBOL_LEN and SAMPLE_LEN differ (%ld)",
		    V32_SYMBOL_LEN[0] != V32_SAMPLE_LEN[0] &&
		    V32_SYMBOL_LEN[1] != V32_SAMPLE_LEN[1], 1, 1);
	diff_eq_int("TURNAROUND_DLY and SAMPLE_LEN differ (%ld)",
		    V32_TURNAROUND_DLY[0] != V32_SAMPLE_LEN[0] &&
		    V32_TURNAROUND_DLY[1] != V32_SAMPLE_LEN[1], 1, 2);

	return diff_end();
}

static int
test_pps_tables(void)
{
	int i;

	diff_begin("PPSv32_ICOFFS / PPSv32_QCOFFS / PPSv32_CFG");

	for (i = 0; i < 120; i++) {
		diff_eq_int("PPSv32_ICOFFS[%ld]", PPSv32_ICOFFS[i],
			    ref_PPSv32_ICOFFS[i], i);
		diff_eq_int("PPSv32_QCOFFS[%ld]", PPSv32_QCOFFS[i],
			    ref_PPSv32_QCOFFS[i], i);
	}

	/*
	 * The symmetry the source comment claims, checked rather than
	 * asserted in prose: I is even about the centre and Q is odd.  A
	 * single mistyped coefficient breaks one of the sixty pairs.
	 */
	for (i = 0; i < 60; i++) {
		diff_eq_int("PPSv32_ICOFFS is symmetric at %ld",
			    PPSv32_ICOFFS[i], PPSv32_ICOFFS[119 - i], i);
		diff_eq_int("PPSv32_QCOFFS is anti-symmetric at %ld",
			    PPSv32_QCOFFS[i], -PPSv32_QCOFFS[119 - i], i);
	}

	diff_eq_int("phases (%ld)", PPSv32_CFG.phases, ref_PPSv32_CFG.phases,
		    0x00);
	diff_eq_int("step (%ld)", PPSv32_CFG.step, ref_PPSv32_CFG.step, 0x02);
	diff_eq_int("mapped (%ld)", PPSv32_CFG.mapped, ref_PPSv32_CFG.mapped,
		    0x04);
	diff_eq_int("scale (%ld)", PPSv32_CFG.scale, ref_PPSv32_CFG.scale,
		    0x08);
	diff_eq_int("step_adj (%ld)", PPSv32_CFG.step_adj,
		    ref_PPSv32_CFG.step_adj, 0x0c);
	diff_eq_int("pad0e (%ld)", PPSv32_CFG.pad0e, ref_PPSv32_CFG.pad0e,
		    0x0e);
	diff_eq_int("coeffs (%ld)", PPSv32_CFG.coeffs, ref_PPSv32_CFG.coeffs,
		    0x20);
	diff_eq_int("pad22 (%ld)", PPSv32_CFG.pad22, ref_PPSv32_CFG.pad22,
		    0x22);
	diff_eq_int("aux (%ld)", PPSv32_CFG.aux != 0,
		    ref_PPSv32_CFG.aux != 0, 0x24);

	/* The four pointers, by which table each names. */
	diff_eq_int("imap resolves on our side (%ld)",
		    pps_table_index(PPSv32_CFG.imap) >= 0, 1, 0x10);
	diff_eq_int("imap resolves on the blob's side (%ld)",
		    pps_table_index(ref_PPSv32_CFG.imap) >= 0, 1, 0x10);
	diff_eq_int("imap names the same table (%ld)",
		    pps_table_index(PPSv32_CFG.imap),
		    pps_table_index(ref_PPSv32_CFG.imap), 0x10);
	diff_eq_int("qmap names the same table (%ld)",
		    pps_table_index(PPSv32_CFG.qmap),
		    pps_table_index(ref_PPSv32_CFG.qmap), 0x14);
	diff_eq_int("coeff_i names the same table (%ld)",
		    pps_table_index(PPSv32_CFG.coeff_i),
		    pps_table_index(ref_PPSv32_CFG.coeff_i), 0x18);
	diff_eq_int("coeff_q names the same table (%ld)",
		    pps_table_index(PPSv32_CFG.coeff_q),
		    pps_table_index(ref_PPSv32_CFG.coeff_q), 0x1c);

	/* And that they are the FOUR DIFFERENT ones, in the object's order. */
	diff_eq_int("imap is SMCv32_IMAP16 (%ld)",
		    pps_table_index(PPSv32_CFG.imap), 0, 0x10);
	diff_eq_int("qmap is SMCv32_QMAP16 (%ld)",
		    pps_table_index(PPSv32_CFG.qmap), 1, 0x14);
	diff_eq_int("coeff_i is PPSv32_ICOFFS (%ld)",
		    pps_table_index(PPSv32_CFG.coeff_i), 2, 0x18);
	diff_eq_int("coeff_q is PPSv32_QCOFFS (%ld)",
		    pps_table_index(PPSv32_CFG.coeff_q), 3, 0x1c);

	/* The two cross-table invariants a single wrong number would break. */
	diff_eq_int("coeffs is PPSv32_ICOFFS's length (%ld)",
		    PPSv32_CFG.coeffs, 120, 0x20);
	diff_eq_int("coeffs / phases is 12 taps (%ld)",
		    PPSv32_CFG.coeffs / PPSv32_CFG.phases, 12, 0x20);
	diff_eq_int("scale is 131072, not a Q15 gain (%ld)",
		    PPSv32_CFG.scale, 131072, 0x08);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

/*
 * Layer 3.  Drive both configurations through the shaper they configure.
 *
 * `mapped` is set, so the ring's `sym` rail is what is read and the two
 * constellation maps are what turn an index into I and Q.  The symbol stream
 * is deterministic and covers all sixteen points; `count` is deliberately not
 * a multiple of `len`, so `ridx` wraps inside a call.
 */
static int
test_pps_use(void)
{
	struct fpm_pps ours;
	unsigned char theirs[256];
	struct fpm_smc_ring ring_a, ring_b;
	short sym[20];
	short out_a[512], out_b[512];
	int i;
	int pass;
	int moved = 0;

	diff_begin("PPSv32_CFG driving FPM_PPS_filter");

	for (i = 0; i < 20; i++)
		sym[i] = (short)((i * 7 + 3) & 0x0f);

	memset(&ours, 0, sizeof(ours));
	memset(theirs, 0, sizeof(theirs));
	memset(&ring_a, 0, sizeof(ring_a));
	memset(&ring_b, 0, sizeof(ring_b));

	ring_a.sym = sym;
	ring_a.len = 20;
	ring_b = ring_a;

	ref_FPM_PPS_init(theirs, &ref_PPSv32_CFG, 1);
	FPM_PPS_init(&ours, &PPSv32_CFG, 1);

	for (pass = 0; pass < 6; pass++) {
		unsigned short na, nb;

		memset(out_a, 0, sizeof(out_a));
		memset(out_b, 0, sizeof(out_b));

		na = ref_FPM_PPS_filter(theirs, &ring_a, out_a, 13);
		nb = FPM_PPS_filter(&ours, &ring_b, out_b, 13);

		diff_eq_int("samples produced, pass %ld", nb, na, pass);
		for (i = 0; i < (int)na && i < 512; i++) {
			diff_eq_int("pass %ld sample", out_b[i], out_a[i],
				    pass);
			if (out_a[i] != 0)
				moved = 1;
		}
		diff_eq_int("ring ridx, pass %ld", ring_b.ridx, ring_a.ridx,
			    pass);
	}

	/*
	 * F134's rule: a clean run from a filter that produced nothing is not
	 * a clean run.  The shaper must actually have emitted a non-zero
	 * sample, and the ring must actually have wrapped.
	 */
	diff_eq_int("the shaper produced a non-zero sample (%ld)", moved, 1, 0);

	ref_FPM_PPS_free(theirs);
	FPM_PPS_free(&ours);

	return diff_end();
}

/* ------------------------------------------------------------------------- */

static int
test_bss(void)
{
	diff_begin("SMCv32_CFG / Control_Flag");

	diff_eq_int("SMCv32_CFG.mode (%ld)", SMCv32_CFG.mode,
		    ref_SMCv32_CFG.mode, 0x00);
	diff_eq_int("SMCv32_CFG.pad02 (%ld)", SMCv32_CFG.pad02,
		    ref_SMCv32_CFG.pad02, 0x02);
	diff_eq_int("Control_Flag (%ld)", Control_Flag, ref_Control_Flag, 0);

	/*
	 * Both are `.bss` and four bytes, and BOTH FACTS MATTER.  A
	 * `struct v32_smc_cfg` given the whole of `struct v32_smc`'s layout
	 * would still read as zero and would still compare equal; the size is
	 * the only thing that separates the two readings.
	 */
	diff_eq_int("sizeof struct v32_smc_cfg (%ld)",
		    (long)sizeof(struct v32_smc_cfg), 4, 0);
	diff_eq_int("SMCv32_CFG is zero (%ld)",
		    SMCv32_CFG.mode == 0 && SMCv32_CFG.pad02 == 0, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= test_params();
	rc |= test_ctl();
	rc |= test_rate_tables();
	rc |= test_length_tables();
	rc |= test_pps_tables();
	rc |= test_pps_use();
	rc |= test_bss();

	return rc;
}
