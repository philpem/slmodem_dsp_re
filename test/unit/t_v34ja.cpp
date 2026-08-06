/*
 * t_v34ja.cpp -- `indicateJaTransmission`, against the blob.
 *
 * Fifty-seven bytes, and NOT fifty-seven bytes of test.  The function is pure
 * dispatch: two loads, two signed compares against 1, and two tail jumps.  It
 * has no arithmetic to get wrong.  What it can get wrong is WHICH FIELD gates
 * the decision, WHERE the boundary is, WHICH OBJECT is handed over, and
 * WHETHER anything happens at all -- so this file is about those four and
 * nothing else.
 *
 * WHY IT IS A .cpp AND WHY IT REUSES v90demfix.h.  The live arm calls
 * `VPcmFloModem::enterPhase3`, which reaches a demodulator through
 * `modem.demodulator` and packs the embedded DIL descriptor through
 * `DILdescriptorPacker`.  Standing that graph up is the most expensive fixture
 * in this tree and it already exists; t_vpcmep3.cpp drives the same method
 * directly and t_v90demod.cpp the demodulator under it.  This file is the
 * third user of the fixture rather than a fourth copy of it.
 *
 * THE BLOB SIDE IS ENTIRELY THE BLOB.  `$(REF)` prefixes every symbol the
 * object defines, so `ref_indicateJaTransmission` tail-jumps into
 * `ref__ZN12VPcmFloModem11enterPhase3Ev` and that into the blob's own
 * `V90Demodulator::enterPhase3` and `DILdescriptorPacker`.  Our side is
 * entirely this tree's.  So a difference anywhere in the graph is a
 * difference, and the callee being independently tested by t_vpcmep3 is what
 * makes it fair to read one here as a defect in the dispatch.
 *
 * THE FOUR CLAIMS, and the vacuity trap each carries:
 *
 *   1. PHASE 3 IS ENTERED EXACTLY WHEN `v90_receiver > 1`.  "The two sides
 *      agree" is satisfied by both sides doing nothing, so agreement alone
 *      proves nothing.  Every trial therefore snapshots the modem slot BEFORE
 *      the call and requires it to have changed on the BLOB's side if and
 *      only if `v90_receiver > 1` -- a statement about the object, not about
 *      the reconstruction agreeing with itself.  The transcript is the second,
 *      independent witness: `enterPhase3`'s two diagnostics are both gated at
 *      `dsplibs_debug_level > 1`, so a non-empty transcript is exactly
 *      `level > 1 && entered`, and that one cannot be carried by a seeded byte
 *      that happened to equal a stored constant.
 *
 *   2. THE BOUNDARY IS SIGNED AND IT IS `> 1`.  `cmpl $0x1,...; jg`.  0, 1 and
 *      2 separate `> 1` from `>= 1`, `> 0` and `!= 0`; 3 and larger separate it
 *      from `== 2`; and NEGATIVE values are what separate a signed compare
 *      from an unsigned one -- without them `(unsigned)v > 1` passes
 *      everything.  INT_MIN and -1 are both in the sweep and both are counted.
 *
 *   3. THE OBJECT HANDED OVER IS `p3548`, NOT `pac18` AND NOT THE V.34 OBJECT.
 *      `pac18` points at a DECOY that is itself a fully wired VPcmFloModem
 *      with its own demodulator pointer and a legal DIL descriptor, so a
 *      mis-wire corrupts it quietly instead of crashing, and the decoy is
 *      required to be byte-identical to its pre-call image on every trial.
 *      The V.34 object is required to be unchanged too: this function stores
 *      nothing anywhere.
 *
 *   4. THE ARMS ARE IN THE RIGHT ORDER AND READ THE RIGHT FIELDS.  The sweep
 *      is a full cross-product of the two receiver values, so it contains
 *      trials with `v90 <= 1 && k56 > 1` (which must NOT enter phase 3, and
 *      which catches arm 1 reading `k56flex_receiver`) and trials with
 *      `v90 > 1 && k56 <= 1` (which must, and which catches a swap).
 *
 * WHAT THIS TEST CANNOT SEE, NAMED RATHER THAN HIDDEN.  Everything downstream
 * of the second test is DEAD: `K56FlexFloModem::enterPhase3FullDuplex` is one
 * byte of code in the object -- a bare `ret` -- and is an empty body here.  So
 * the `k56flex_receiver > 1` comparison, the operand it reads, the pointer it
 * is given, and `else if` against two independent `if`s are ONE EQUIVALENCE
 * CLASS that no differential test can split, for as long as that callee stays
 * empty.  The comparison is written `> 1` because the object writes `cmpl
 * $0x1; jg`, not because anything here measured it.  test/mutations/v34ja.json
 * carries one mutation inside that arm precisely to record it as uncaught.
 *
 * ITS POSITION IS A DIFFERENT MATTER AND IS TESTED.  Trying the K56flex arm
 * FIRST is observable, because a trial with both receivers above 1 must still
 * enter phase 3 -- so `bothIn` below is not decoration, it is what makes that
 * mutation catchable.  The dead part is the arm's interior, not its place in
 * the chain; finding 381 draws the line.
 */

#include "v90demfix.h"

extern "C" {
void ref_indicate_ja(void *obj) asm("ref_indicateJaTransmission");
}

#include "dsplib/VPcmFloModem.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"

/*
 * Larger than the last modelled field on both, so a store past the end of
 * either map is caught as well as one inside it.  t_vpcmep3.cpp's note on why
 * no `sizeof(VPcmFloModem)` is asserted applies unchanged.
 */
#define SLACK		64
#define VPCM_SLOT	(sizeof(VPcmFloModem) + SLACK)
#define OBJ_SLOT	(sizeof(struct v34_object) + SLACK)

static unsigned char objb[2][OBJ_SLOT] __attribute__((aligned(8)));
static unsigned char vpcm[2][VPCM_SLOT] __attribute__((aligned(8)));
static unsigned char decoy[2][VPCM_SLOT] __attribute__((aligned(8)));

/* The pre-call images, taken from each side after wiring and before firing. */
static unsigned char pre_o[2][OBJ_SLOT];
static unsigned char pre_v[2][VPCM_SLOT];
static unsigned char pre_k[2][VPCM_SLOT];

static struct v34_object *
O(int side)
{
	return (struct v34_object *)objb[side];
}

static VPcmFloModem *
V(int side)
{
	return (VPcmFloModem *)vpcm[side];
}

/*
 * The decoy behind `pac18`.  In the real object that pointer is a
 * `K56FlexFloModem`, a class with no data members at all (see its header), so
 * there is nothing there for a test to compare.  Shaping the block as a
 * VPcmFloModem instead is what turns "the wrong object was handed to
 * enterPhase3" from a segmentation fault into a visible difference.
 */
static VPcmFloModem *
K(int side)
{
	return (VPcmFloModem *)decoy[side];
}

struct ja_args {
	struct trial_args dem;	/* everything the demodulator graph needs */
	int v90;		/* obj->v90_receiver   */
	int k56;		/* obj->k56flex_receiver */
};

/*
 * The DIL descriptor's three lengths, clamped exactly as t_vpcmep3.cpp clamps
 * them: `seq1` and `seq2` are 128 bytes each and the packer reads
 * `seq1Length` of them, so a larger value is out of contract for the original
 * too and would also push the packed length past `bitVector`.
 */
static void
wire_modem(VPcmFloModem *m, int side, int trial)
{
	m->modem.demodulator = D(side);
	m->dil.dilCount = (unsigned char)((trial * 37) & 0xff);
	m->dil.seq1Length = (unsigned char)((trial * 13 + 1) % 129);
	m->dil.seq2Length = (unsigned char)((trial * 29 + 7) % 129);
}

static void
ja_setup(int trial, const struct ja_args *a)
{
	int side;

	/*
	 * The graph first, so that `lfsr_state` is left running and the three
	 * blocks below are filled from the same stream -- pairwise identical
	 * and varied, which is finding 230's rule.
	 */
	setup(trial, &a->dem);
	fill_pair(objb[0], objb[1], OBJ_SLOT);
	fill_pair(vpcm[0], vpcm[1], VPCM_SLOT);
	fill_pair(decoy[0], decoy[1], VPCM_SLOT);

	for (side = 0; side < 2; side++) {
		wire_modem(V(side), side, trial);
		wire_modem(K(side), side, trial + 1);

		O(side)->p3548 = (void *)V(side);
		O(side)->pac18 = (void *)K(side);
		O(side)->v90_receiver = a->v90;
		O(side)->k56flex_receiver = a->k56;

		memcpy(pre_o[side], objb[side], OBJ_SLOT);
		memcpy(pre_v[side], vpcm[side], VPCM_SLOT);
		memcpy(pre_k[side], decoy[side], VPCM_SLOT);
	}
}

static void
fire(void)
{
	dsplib_debug_capture_reset();
	indicateJaTransmission(O(0));
	ref_indicate_ja(O(1));
	teardown();
}

/*
 * The only field of the modem that holds a per-side address, and it is in the
 * MIDDLE of the block rather than at either end -- the placement that has
 * caught two earlier batches out.  Everything else came from `fill_pair` and
 * is byte-identical on the two sides, so neutralising any of it would turn a
 * compared word into an ignored one.
 */
static void
snap_modem(unsigned char *dst, const unsigned char *src, int side)
{
	VPcmFloModem *s;
	const VPcmFloModem *l = (const VPcmFloModem *)src;

	memcpy(dst, src, VPCM_SLOT);
	s = (VPcmFloModem *)dst;
	s->modem.demodulator = (V90Demodulator *)(long)
	    (l->modem.demodulator == D(side));
}

/* The V.34 object holds two per-side addresses, and both are ours. */
static void
snap_obj(unsigned char *dst, int side)
{
	struct v34_object *s;
	const struct v34_object *l = O(side);

	memcpy(dst, objb[side], OBJ_SLOT);
	s = (struct v34_object *)dst;
	s->p3548 = (void *)(long)(l->p3548 == (void *)V(side));
	s->pac18 = (void *)(long)(l->pac18 == (void *)K(side));
}

static void
compare_everything(const char *what, long tag)
{
	static unsigned char a[VPCM_SLOT], b[VPCM_SLOT];
	static unsigned char c[OBJ_SLOT], d[OBJ_SLOT];

	snap_modem(a, vpcm[0], 0);
	snap_modem(b, vpcm[1], 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "VPcmFloModem at p3548",
		     a, b, VPCM_SLOT, tag);

	snap_modem(a, decoy[0], 0);
	snap_modem(b, decoy[1], 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "the block at pac18",
		     a, b, VPCM_SLOT, tag);

	snap_obj(c, 0);
	snap_obj(d, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "struct v34_object",
		     c, d, OBJ_SLOT, tag);

	compare_all(what, tag);
}

/*
 * ===========================================================================
 * The dispatch
 * ===========================================================================
 *
 * `v90_receiver` on one axis, `k56flex_receiver` on the other, the debug level
 * on a third, and the whole cross-product.  Nothing here is a corner case
 * picked by hand: the point of the function is a two-way decision, so the
 * table is the decision's own domain.
 *
 * INT_MIN and INT_MAX are spelled out rather than included, so that this file
 * does not acquire a header for two constants.
 */
#define JA_INT_MIN	(-2147483647 - 1)
#define JA_INT_MAX	2147483647

static const int v90_v[] = {
	JA_INT_MIN, -3, -1, 0, 1, 2, 3, 0x10000, JA_INT_MAX
};
#define NV90 ((int)(sizeof(v90_v) / sizeof(v90_v[0])))

static const int k56_v[] = {
	JA_INT_MIN, -1, 0, 1, 2, 7, JA_INT_MAX
};
#define NK56 ((int)(sizeof(k56_v) / sizeof(k56_v[0])))

static int
run_dispatch(void)
{
	struct ja_args a;
	int i, j, lvl, side, trial = 0;
	int entered = 0, skipped = 0;
	int negIn = 0, zeroIn = 0, oneIn = 0, twoIn = 0;
	int k56OnlyIn = 0, v90OnlyIn = 0, bothIn = 0, neitherIn = 0;
	int quiet = 0, loud = 0;

	diff_begin("indicateJaTransmission -- which modem enters phase 3");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);

		for (i = 0; i < NV90; i++) {
			for (j = 0; j < NK56; j++) {
				long tag = (long)lvl * 10000 + i * 100 + j;
				int want = (v90_v[i] > 1);
				int ran[2];

				trial++;

				a.dem.latch = (unsigned)(trial & 1);
				a.dem.flag = 0;
				a.dem.eia6 = (trial & 2) ? 6 : 0;
				a.dem.blockByte = 0;
				a.dem.pcmType = (trial >> 2) & 1;
				a.dem.idx = trial % 6;
				a.v90 = v90_v[i];
				a.k56 = k56_v[j];

				ja_setup(trial, &a);
				fire();

				compare_everything("after indicateJa", tag);

				/*
				 * 1.  THE DECISION, read off the BLOB.  A
				 *     changed modem slot is the whole of
				 *     `enterPhase3`'s twenty-one stores and its
				 *     packed JA vector; an unchanged one is the
				 *     function having returned.
				 */
				for (side = 0; side < 2; side++)
					ran[side] = memcmp(vpcm[side],
							   pre_v[side],
							   VPCM_SLOT) != 0;

				diff_eq_int("the blob entered phase 3 iff "
					    "v90_receiver > 1 (%ld)",
					    (long)ran[1], (long)want, tag);
				diff_eq_int("...and so did we (%ld)",
					    (long)ran[0], (long)want, tag);

				/*
				 * 2.  THE OBJECT HANDED OVER.  Neither the
				 *     block at `pac18` nor the V.34 object is
				 *     touched, on either side, ever.
				 */
				for (side = 0; side < 2; side++) {
					diff_eq_int("the pac18 block is never "
						    "written (%ld)",
						    memcmp(decoy[side],
							   pre_k[side],
							   VPCM_SLOT) == 0,
						    1, tag * 10 + side);
					diff_eq_int("the V.34 object is never "
						    "written (%ld)",
						    memcmp(objb[side],
							   pre_o[side],
							   OBJ_SLOT) == 0,
						    1, tag * 10 + side);
				}

				/*
				 * 3.  THE TRANSCRIPT, the second witness.
				 *     Both of `enterPhase3`'s diagnostics are
				 *     gated at `dsplibs_debug_level > 1`, and
				 *     nothing else on any path here prints, so
				 *     a line exists exactly when the level is
				 *     above 1 AND phase 3 was entered.
				 */
				diff_eq_int("a transcript exists iff level > 1 "
					    "and phase 3 was entered (%ld)",
					    (long)(dsplib_debug_capture_lines(1)
						   != 0),
					    (long)(lvl > 1 && want), tag);
				diff_eq_int("transcript line count (%ld)",
					    (long)dsplib_debug_capture_lines(0),
					    (long)dsplib_debug_capture_lines(1),
					    tag);
				diff_eq_int("transcript text (%ld)",
					    strcmp(dsplib_debug_capture_text(0),
						   dsplib_debug_capture_text(1))
					    == 0, 1, tag);

				if (dsplib_debug_capture_lines(1) == 0)
					quiet++;
				else
					loud++;

				if (want)
					entered++;
				else
					skipped++;
				if (v90_v[i] < 0)
					negIn++;
				if (v90_v[i] == 0)
					zeroIn++;
				if (v90_v[i] == 1)
					oneIn++;
				if (v90_v[i] == 2)
					twoIn++;
				if (want && k56_v[j] > 1)
					bothIn++;
				else if (want)
					v90OnlyIn++;
				else if (k56_v[j] > 1)
					k56OnlyIn++;
				else
					neitherIn++;
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	/*
	 * ANTI-VACUITY.  Each of these is a case the checks above depend on
	 * having occurred; a table edited down to nothing would otherwise pass
	 * on a smaller cross-product (findings 247, 262, 295).  Every counter
	 * below is derived from the sweep tables rather than from the
	 * function's behaviour, so none of them can be satisfied by the code
	 * under test agreeing with itself.
	 */
	diff_eq_int("phase 3 was entered at least once", entered > 0, 1, 0);
	diff_eq_int("...and skipped at least once", skipped > 0, 1, 0);
	diff_eq_int("a NEGATIVE receiver was swept -- signed, not unsigned",
		    negIn > 0, 1, 0);
	diff_eq_int("0, the value below the boundary", zeroIn > 0, 1, 0);
	diff_eq_int("1, the value ON the boundary", oneIn > 0, 1, 0);
	diff_eq_int("2, the value above it", twoIn > 0, 1, 0);
	diff_eq_int("a trial with the K56flex receiver up and the V.90 one "
		    "down", k56OnlyIn > 0, 1, 0);
	diff_eq_int("...and one the other way round", v90OnlyIn > 0, 1, 0);
	diff_eq_int("...and one with both up", bothIn > 0, 1, 0);
	diff_eq_int("...and one with neither", neitherIn > 0, 1, 0);
	diff_eq_int("both debug arms were taken", quiet > 0 && loud > 0, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * The two arms leave DIFFERENT graphs
 * ===========================================================================
 *
 * Check 1 above reads "the modem slot changed" as "phase 3 was entered".  That
 * inference is only worth anything if entering phase 3 is observable at all in
 * this fixture -- if `enterPhase3` happened to leave the slot exactly as it
 * found it, "changed iff `v90 > 1`" would be an assertion that nothing ever
 * changes and it would hold for a function with no body.
 *
 * So: one pair of trials identical in everything except `v90_receiver`, with
 * the resulting modem slots and demodulator graphs required to DIFFER.  This
 * is the check that fails if the fixture ever stops making the call visible.
 */
static int
run_observable(void)
{
	static unsigned char withCall[VPCM_SLOT], withoutCall[VPCM_SLOT];
	static unsigned char demWith[DEM_SLOT], demWithout[DEM_SLOT];
	struct ja_args a;

	diff_begin("indicateJaTransmission -- entering phase 3 is visible");

	set_level(0);

	a.dem.latch = 0;
	a.dem.flag = 0;
	a.dem.eia6 = 6;
	a.dem.blockByte = 0;
	a.dem.pcmType = 0;
	a.dem.idx = 2;
	a.k56 = 0;

	a.v90 = 2;
	ja_setup(90, &a);
	fire();
	compare_everything("after indicateJa", 0);
	memcpy(withCall, vpcm[1], VPCM_SLOT);
	snap_dem(demWith, 1);

	a.v90 = 1;
	ja_setup(90, &a);
	fire();
	compare_everything("after indicateJa", 1);
	memcpy(withoutCall, vpcm[1], VPCM_SLOT);
	snap_dem(demWithout, 1);

	diff_eq_int("the two arms leave different modems",
		    memcmp(withCall, withoutCall, VPCM_SLOT) != 0, 1, 0);
	diff_eq_int("...and different demodulators",
		    memcmp(demWith, demWithout, DEM_SLOT) != 0, 1, 0);

	/*
	 * And the demodulator really was the one behind `p3548`'s modem: its
	 * own `inPhase3` latch is set by `V90Demodulator::enterPhase3` and by
	 * nothing else this function can reach.
	 */
	a.v90 = 2;
	ja_setup(91, &a);
	fire();
	diff_eq_int("the demodulator's latch is set (%ld)",
		    (long)D(1)->inPhase3, 1, 0);
	diff_eq_int("...on our side too (%ld)", (long)D(0)->inPhase3, 1, 0);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_dispatch();
	bad |= run_observable();

	return bad;
}
