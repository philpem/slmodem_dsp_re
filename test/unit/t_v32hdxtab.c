/*
 * t_v32hdxtab.c -- differential test of the tables the V.32 half-duplex
 *                  machine dispatches and configures itself from.
 *
 *   V32NextState      .data   0x0076cc  24   six function pointers
 *   V32_CONNECT       .data   0x007724  14
 *   V32_RX_MODE       .data   0x007732  14
 *   V32_TX_MODE       .data   0x007740  14
 *   V32_S_DATA_COEF   .rodata 0x006d60  30
 *   SREv32_CFG        .rodata 0x007020  56
 *
 * A TABLE OF POINTERS CANNOT BE COMPARED BY VALUE, and that is the whole
 * reason this file is longer than six memcmps.  Ours holds the addresses of
 * OUR functions and the blob's holds the addresses of the `ref_` aliases, so
 * the two arrays differ in every dword while being the same table.  What is
 * comparable is the PERMUTATION: for each slot, which of the five next-state
 * functions is named.  So each side is mapped to an index 0..4 through its own
 * roster and the two index sequences are compared -- and an address that
 * matches nothing in either roster fails as -1 rather than silently reading as
 * a match, which is what a `!= NULL` check would have done.
 *
 * The same argument applies to the six pointers inside `SREv32_CFG`, and
 * `diff_eq_obj` over the whole struct would report all six as differences and
 * bury the eighteen scalars that are the actual reading.  They are compared
 * field by field instead, the pointers by which table they name.
 *
 * WHAT EACH CHECK IS FOR, as a named wrong reading:
 *
 *   - `V32NextState` read from the file's BYTES rather than its relocations.
 *     All six dwords are zero in the object, so a byte comparison of two
 *     all-zero tables passes while naming nothing.  Every slot here is
 *     resolved to a function first.
 *   - slot 5 read as a hole.  `v32hdx.h`'s prose said "no relocation" for it;
 *     it is `V32RngRespNextState` (finding F8560), and slot 5 is checked
 *     explicitly and by name.
 *   - `V32LocLoopNextState` read as occupying one slot.  It occupies two, 2
 *     and 3, and the test asserts that the two slots hold the SAME function
 *     rather than merely that each is some next-state function.
 *   - the three short tables made `const`.  They are `.data` in the object
 *     and this test cannot see a section, so the check is that the VALUES
 *     round-trip; the placement is asserted in the source comment and by
 *     `make similarity`, not here.
 *   - `V32_RX_MODE` and `V32_TX_MODE` conflated.  Their contents coincide
 *     today, so each is compared against its OWN `ref_` symbol; swapping the
 *     two in the source would still pass, and that is stated rather than
 *     claimed otherwise.
 *   - `SREv32_CFG` written as a byte copy.  The eighteen scalars are compared
 *     as the named fields `FPM_SRE_init` and `FPM_SRE_recover` dereference,
 *     so a wrong field boundary fails on the field and not on an offset.
 */

#include "harness.h"

#include "dsplib/v32hdxst.h"
#include "dsplib/fpm_sre.h"

/*
 * The blob's copies.  `symmap.py` renames OBJECT symbols exactly as it
 * renames functions, so all six tables have a `ref_` alias; every one of
 * these declarations is required, because an undeclared `ref_` name is a hard
 * error rather than an implicit int.
 */
extern v32_nextstate_fn ref_V32NextState[6];
extern short ref_V32_CONNECT[7];
extern short ref_V32_RX_MODE[7];
extern short ref_V32_TX_MODE[7];
extern const short ref_V32_S_DATA_COEF[15];
extern const struct fpm_sre_cfg ref_SREv32_CFG;

extern void ref_V32OrgNextState(void *modem);
extern void ref_V32AnsNextState(void *modem);
extern void ref_V32RngInitNextState(void *modem);
extern void ref_V32RngRespNextState(void *modem);
extern void ref_V32LocLoopNextState(void *modem);

extern const short ref_SREv32_COFFS[181];
extern short ref_SREv32_XB_COFFS[FPM_SRE_DISC];
extern short ref_SREv32_PLL_K1[FPM_SRE_MODES];
extern short ref_SREv32_PLL_K2[FPM_SRE_MODES];
extern short ref_SREv32_xCLOCK[3];
extern short ref_SREv32_yCLOCK[3];

/* ------------------------------------------------------------------------- */

static int
ours_ns_index(v32_nextstate_fn p)
{
	if (p == V32OrgNextState)
		return 0;
	if (p == V32AnsNextState)
		return 1;
	if (p == V32RngInitNextState)
		return 2;
	if (p == V32RngRespNextState)
		return 3;
	if (p == V32LocLoopNextState)
		return 4;
	return -1;
}

static int
ref_ns_index(v32_nextstate_fn p)
{
	if (p == ref_V32OrgNextState)
		return 0;
	if (p == ref_V32AnsNextState)
		return 1;
	if (p == ref_V32RngInitNextState)
		return 2;
	if (p == ref_V32RngRespNextState)
		return 3;
	if (p == ref_V32LocLoopNextState)
		return 4;
	return -1;
}

/*
 * Which of the six SRE tables a pointer names, on whichever side it came
 * from.  -1 for "none of them", so a null or a stray address fails loudly.
 */
static int
sre_table_index(const void *p)
{
	if (p == (const void *)SREv32_COFFS || p == (const void *)ref_SREv32_COFFS)
		return 0;
	if (p == (const void *)SREv32_XB_COFFS ||
	    p == (const void *)ref_SREv32_XB_COFFS)
		return 1;
	if (p == (const void *)SREv32_xCLOCK ||
	    p == (const void *)ref_SREv32_xCLOCK)
		return 2;
	if (p == (const void *)SREv32_yCLOCK ||
	    p == (const void *)ref_SREv32_yCLOCK)
		return 3;
	if (p == (const void *)SREv32_PLL_K1 ||
	    p == (const void *)ref_SREv32_PLL_K1)
		return 4;
	if (p == (const void *)SREv32_PLL_K2 ||
	    p == (const void *)ref_SREv32_PLL_K2)
		return 5;
	return -1;
}

/* ------------------------------------------------------------------------- */

static int
test_nextstate(void)
{
	int i;
	int rc;

	diff_begin("V32NextState");

	for (i = 0; i < V32_NEXTSTATE_COUNT; i++) {
		int a = ours_ns_index(V32NextState[i]);
		int b = ref_ns_index(ref_V32NextState[i]);

		/*
		 * -1 on EITHER side is a failure in its own right: it means the
		 * slot holds something that is not one of the five, which a
		 * plain `a == b` would pass as -1 == -1.
		 */
		diff_eq_int("V32NextState[%ld] resolves on our side", a >= 0, 1,
			    i);
		diff_eq_int("ref_V32NextState[%ld] resolves on the blob's side",
			    b >= 0, 1, i);
		diff_eq_int("V32NextState[%ld] names the same function", a, b,
			    i);
	}

	/* The four facts F8560 corrects, asserted individually and by name. */
	diff_eq_int("slot 0 is V32OrgNextState",
		    ours_ns_index(V32NextState[0]), 0, 0);
	diff_eq_int("slot 1 is V32AnsNextState",
		    ours_ns_index(V32NextState[1]), 1, 1);
	diff_eq_int("slot 4 is V32RngInitNextState",
		    ours_ns_index(V32NextState[4]), 2, 4);
	diff_eq_int("slot 5 is V32RngRespNextState, not a hole",
		    ours_ns_index(V32NextState[5]), 3, 5);
	diff_eq_int("slots 2 and 3 are both V32LocLoopNextState",
		    ours_ns_index(V32NextState[2]) == 4 &&
		    ours_ns_index(V32NextState[3]) == 4, 1, 23);
	diff_eq_int("slots 2 and 3 are the SAME function",
		    V32NextState[2] == V32NextState[3], 1, 23);

	rc = diff_end();
	return rc;
}

static int
test_shorts(void)
{
	int i;

	diff_begin("V32_CONNECT / V32_RX_MODE / V32_TX_MODE");

	for (i = 0; i < 7; i++) {
		diff_eq_int("V32_CONNECT[%ld]", V32_CONNECT[i],
			    ref_V32_CONNECT[i], i);
		diff_eq_int("V32_RX_MODE[%ld]", V32_RX_MODE[i],
			    ref_V32_RX_MODE[i], i);
		diff_eq_int("V32_TX_MODE[%ld]", V32_TX_MODE[i],
			    ref_V32_TX_MODE[i], i);
	}

	for (i = 0; i < 15; i++)
		diff_eq_int("V32_S_DATA_COEF[%ld]", V32_S_DATA_COEF[i],
			    ref_V32_S_DATA_COEF[i], i);

	return diff_end();
}

static int
test_sre_cfg(void)
{
	diff_begin("SREv32_CFG");

	diff_eq_int("clock_len", SREv32_CFG.clock_len,
		    ref_SREv32_CFG.clock_len, 0x00);
	diff_eq_int("groups_acq", SREv32_CFG.groups_acq,
		    ref_SREv32_CFG.groups_acq, 0x02);
	diff_eq_int("groups_trk", SREv32_CFG.groups_trk,
		    ref_SREv32_CFG.groups_trk, 0x04);
	diff_eq_int("settle", SREv32_CFG.settle, ref_SREv32_CFG.settle, 0x06);
	diff_eq_int("acc_down", SREv32_CFG.acc_down, ref_SREv32_CFG.acc_down,
		    0x08);
	diff_eq_int("acc_up", SREv32_CFG.acc_up, ref_SREv32_CFG.acc_up, 0x0a);
	diff_eq_int("coeffs", SREv32_CFG.coeffs, ref_SREv32_CFG.coeffs, 0x0c);
	diff_eq_int("pad0e", SREv32_CFG.pad0e, ref_SREv32_CFG.pad0e, 0x0e);

	/*
	 * The six pointers: which TABLE each names, never the address.  A
	 * `sre_table_index` of -1 means the slot points at something that is
	 * not one of the six, which is a failure on either side.
	 */
	diff_eq_int("proto names the same table",
		    sre_table_index(SREv32_CFG.proto),
		    sre_table_index(ref_SREv32_CFG.proto), 0x10);
	diff_eq_int("proto resolves at all", sre_table_index(SREv32_CFG.proto),
		    0, 0x10);
	diff_eq_int("disc names the same table",
		    sre_table_index(SREv32_CFG.disc),
		    sre_table_index(ref_SREv32_CFG.disc), 0x14);
	diff_eq_int("disc resolves at all", sre_table_index(SREv32_CFG.disc), 1,
		    0x14);
	diff_eq_int("xclock names the same table",
		    sre_table_index(SREv32_CFG.xclock),
		    sre_table_index(ref_SREv32_CFG.xclock), 0x18);
	diff_eq_int("xclock resolves at all",
		    sre_table_index(SREv32_CFG.xclock), 2, 0x18);
	diff_eq_int("yclock names the same table",
		    sre_table_index(SREv32_CFG.yclock),
		    sre_table_index(ref_SREv32_CFG.yclock), 0x1c);
	diff_eq_int("yclock resolves at all",
		    sre_table_index(SREv32_CFG.yclock), 3, 0x1c);
	diff_eq_int("pll_k1 names the same table",
		    sre_table_index(SREv32_CFG.pll_k1),
		    sre_table_index(ref_SREv32_CFG.pll_k1), 0x20);
	diff_eq_int("pll_k1 resolves at all",
		    sre_table_index(SREv32_CFG.pll_k1), 4, 0x20);
	diff_eq_int("pll_k2 names the same table",
		    sre_table_index(SREv32_CFG.pll_k2),
		    sre_table_index(ref_SREv32_CFG.pll_k2), 0x24);
	diff_eq_int("pll_k2 resolves at all",
		    sre_table_index(SREv32_CFG.pll_k2), 5, 0x24);

	diff_eq_int("mag_hi", SREv32_CFG.mag_hi, ref_SREv32_CFG.mag_hi, 0x28);
	diff_eq_int("mag_lo", SREv32_CFG.mag_lo, ref_SREv32_CFG.mag_lo, 0x2a);
	diff_eq_int("err_hi", SREv32_CFG.err_hi, ref_SREv32_CFG.err_hi, 0x2c);
	diff_eq_int("err_lo", SREv32_CFG.err_lo, ref_SREv32_CFG.err_lo, 0x2e);
	diff_eq_int("rms_min", SREv32_CFG.rms_min, ref_SREv32_CFG.rms_min,
		    0x30);
	diff_eq_int("rms_len", SREv32_CFG.rms_len, ref_SREv32_CFG.rms_len,
		    0x32);
	diff_eq_int("pad34", SREv32_CFG.pad34, ref_SREv32_CFG.pad34, 0x34);
	diff_eq_int("pad36", SREv32_CFG.pad36, ref_SREv32_CFG.pad36, 0x36);

	/*
	 * `coeffs` is one short of the prototype table's length, and that
	 * relationship is what `fpm_sre.h` says the interpolator needs.  It is
	 * checked here because it is the one cross-table invariant a wrong
	 * transcription of a single number would break silently.
	 */
	diff_eq_int("coeffs is one short of SREv32_COFFS's length",
		    SREv32_CFG.coeffs + 1, 181, SREv32_CFG.coeffs);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= test_nextstate();
	rc |= test_shorts();
	rc |= test_sre_cfg();

	return rc;
}
