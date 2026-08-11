/*
 * t_vpcmep3.cpp -- VPcmFloModem::enterPhase3 against the blob.
 *
 * The method is twenty-one CONSTANT stores, one call through a pointer, one
 * call on an embedded structure, and two diagnostics at two different debug
 * thresholds.  Nothing it stores depends on anything it reads, so there are
 * exactly four things a differential test here can be about, and each has its
 * own vacuity trap:
 *
 *   1. WHICH CONSTANT LANDS AT WHICH OFFSET.  The slot is seeded with varied
 *      bytes on both sides, so a store agrees for the right reason only when
 *      the value that was there differed from the value written.  A store of
 *      0 onto a seeded 0 is invisible THAT TRIAL.  `run_stores` therefore
 *      tracks every one of the twenty sites that survive the method and
 *      requires each to have been observably changed at least once across the
 *      sweep -- so "the constant is right" is never carried by a coincidence.
 *
 *   2. WHICH DEMODULATOR THE CALL GETS.  +0x175c is a LOAD (`mov
 *      0x175c(%ebx),%eax`), so the demodulator is pointed at, not embedded.
 *      Standing one up is test/harness/v90demfix.h's whole job.  The trap is
 *      that `V90Demodulator::enterPhase3` returns immediately when its +0x34
 *      already holds 1, and a latched call leaves nothing behind -- so the
 *      pointer would be unproven.  `run_demodulator` drives the latch both
 *      ways and requires the two to leave DIFFERENT graphs.
 *
 *   3. WHAT THE PACKER IS HANDED.  `lea 0x4(%ebx),%eax` is an ADD, so the DIL
 *      descriptor is INSIDE the object at +0x004.  The trap is that a
 *      descriptor of zero length packs to a fixed prefix no matter where it
 *      was read from, which would leave the address unproven.
 *      `run_descriptor` varies the three lengths AND the sequence bytes,
 *      requires both to move the output, and pins the address by re-packing a
 *      standalone copy of the descriptor into a copy of the pre-call bit
 *      vector and requiring the object's own vector to be exactly that.
 *      THAT CHECK IS ABOUT ADDRESSES, NOT ABOUT THE PACKER: both sides of it
 *      are this tree's `DILdescriptorPacker`, which t_dilpack tests against
 *      the blob.  What it establishes is that `enterPhase3` read a descriptor
 *      at +0x004 and wrote a vector at +0x21e, because every byte around both
 *      is varied and any other offset would pack differently.
 *
 *   4. THE TWO DIAGNOSTICS.  The opening one is
 *      `cmpl $0x1,dsplibs_debug_level; ja`, so level 2 and above; the closing
 *      one goes through `edprintf`, which applies the same test itself.
 *      Every group sweeps levels 0 to 3 and compares transcripts, and
 *      `run_stores` requires both arms of the gate to have been seen.
 *
 * ONE THING THIS TEST CANNOT SEE, AND IT IS NAMED RATHER THAN HIDDEN.  The
 * `mov %dx,0x1736(%ebx)` that clears `nofBits` is DEAD: `DILdescriptorPacker`
 * writes through its third argument on both of its exits, unconditionally, so
 * the zero never survives to be read.  A mutation that deletes that store
 * cannot fail any test, and this file says so by name instead of counting it
 * among the twenty sites it does check.
 */

#include "v90demfix.h"

extern "C" {
void ref_vpcm_ep3(void *self) asm("ref__ZN12VPcmFloModem11enterPhase3Ev");
}

#include "dsplib/DILdescriptorPacker.h"
#include "dsplib/VPcmFloModem.h"

/*
 * Larger than the last modelled field, so a store past the end of the map is
 * caught as well as one inside it.  No size is asserted: VPcmFloModem.h says
 * why -- +0x7f27 is a floor, not a measurement.
 */
#define VPCM_SLOT	(sizeof(VPcmFloModem) + 64)

/*
 * How many entries `bitVector` has, and how many the packer can need.  2,654
 * is DILdescriptorPacker.h's own bound for the largest descriptor the fields
 * can describe, and 2,700 is (0x1736 - 0x21e) / 2 -- so the field as this
 * tree maps it is big enough for the worst case and only just, which is a
 * quiet corroboration of where `nofBits` sits.
 */
#define VPCM_BITS	((0x1736 - 0x21e) / 2)
#define DIL_MAX_BITS	2654

static unsigned char vpcm[2][VPCM_SLOT] __attribute__((aligned(8)));

/* One side's `bitVector` as it was before the call; see run_descriptor. */
static short prevec[VPCM_BITS];

static VPcmFloModem *
V(int side)
{
	return (VPcmFloModem *)vpcm[side];
}

/*
 * The descriptor shape.  The lengths are clamped to 0..128 because `seq1` and
 * `seq2` are 128 bytes each and the packer reads `seq1Length` of them; a
 * larger value is out of contract for the original too, and it would also
 * push the packed length past `bitVector`.
 */
struct vpcm_args {
	struct trial_args dem;	/* everything the demodulator graph needs */
	int dilCount;		/* 0..255 */
	int seq1Length;		/* 0..128 */
	int seq2Length;		/* 0..128 */
};

static void
vpcm_setup(int trial, const struct vpcm_args *a)
{
	int side;

	/*
	 * The graph first, so that `lfsr_state` is left running and the modem
	 * slot below is filled from the same stream -- pairwise identical and
	 * varied, which is finding 230's rule.
	 */
	setup(trial, &a->dem);
	fill_pair(vpcm[0], vpcm[1], VPCM_SLOT);

	for (side = 0; side < 2; side++) {
		VPcmFloModem *m = V(side);

		m->modem.demodulator = D(side);
		m->dil.dilCount = (unsigned char)a->dilCount;
		m->dil.seq1Length = (unsigned char)a->seq1Length;
		m->dil.seq2Length = (unsigned char)a->seq2Length;
	}

	memcpy(prevec, V(0)->bitVector, sizeof prevec);
}

static void
fire(void)
{
	dsplib_debug_capture_reset();
	V(0)->enterPhase3();
	ref_vpcm_ep3(V(1));
	teardown();
}

static void
run_pair(int trial, const struct vpcm_args *a)
{
	vpcm_setup(trial, a);
	fire();
}

/*
 * The only field of the modem that holds a per-side address, and it is in the
 * MIDDLE of the block rather than at either end, which is the placement that
 * has caught two earlier batches out.  Everything else came from `fill_pair`
 * and is byte-identical on the two sides, so neutralising any of it would
 * turn a compared word into an ignored one.
 */
static void
snap_vpcm(unsigned char *dst, int side)
{
	VPcmFloModem *s;
	VPcmFloModem *l = V(side);

	memcpy(dst, vpcm[side], VPCM_SLOT);
	s = (VPcmFloModem *)dst;
	s->modem.demodulator = (V90Demodulator *)(long)
	    (l->modem.demodulator == D(side));
}

static void
compare_everything(const char *what, long tag)
{
	static unsigned char a[VPCM_SLOT], b[VPCM_SLOT];

	snap_vpcm(a, 0);
	snap_vpcm(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "VPcmFloModem slot",
		     a, b, VPCM_SLOT, tag);

	compare_all(what, tag);
}

static void
transcripts_agree(long tag)
{
	diff_eq_int("transcript line count (%ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), tag);
	diff_eq_int("transcript text (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
}

/*
 * ===========================================================================
 * The twenty-one stores
 * ===========================================================================
 *
 * Sixteen bytes and five 16-bit words.  Twenty of them survive the method;
 * `nofBits` is the twenty-first and the packer overwrites it, which is why it
 * carries a width of zero and is checked only for having been written by
 * SOMEBODY.
 */
struct site {
	const char *name;
	unsigned int off;
	int width;		/* 1 or 2; 0 = written and then overwritten */
	unsigned int value;
};

static const struct site site_v[] = {
	{ "flags_173a[0]",		0x173a, 1, 0 },
	{ "flags_173a[1]",		0x173b, 1, 0 },
	{ "flags_173a[2]",		0x173c, 1, 0 },
	{ "flag_173d",			0x173d, 1, 0 },
	{ "flag_173e",			0x173e, 1, 0 },
	{ "flags_0217[0]",		0x0217, 1, 1 },
	{ "flags_0217[1]",		0x0218, 1, 0 },
	{ "flags_0217[2]",		0x0219, 1, 1 },
	{ "flags_0217[3]",		0x021a, 1, 1 },
	{ "flags_0217[4]",		0x021b, 1, 1 },
	{ "flags_0217[5]",		0x021c, 1, 0 },
	{ "terminateJa",		0x7dce, 1, 0 },
	{ "terminateCp",		0x7dcf, 1, 0 },
	{ "terminateCpNot",		0x7dd0, 1, 0 },
	{ "cpNotLoaded",		0x7dd1, 1, 0 },
	{ "nofBitsPerSymbol",		0x7dd2, 1, 2 },
	{ "cpNofBits",			0x7dcc, 2, 0 },
	{ "bitPointer",			0x1738, 2, 0 },
	{ "nofTransmitSequences",	0x7dd4, 2, 0 },
	{ "minNofTransmitSequences",	0x7dd6, 2, 1 },
	{ "nofBits (dead: the packer overwrites it)", 0x1736, 0, 0 },
};
#define NSITE ((int)(sizeof(site_v) / sizeof(site_v[0])))

static unsigned int
read_site(int side, const struct site *s)
{
	unsigned short w;

	if (s->width == 1)
		return vpcm[side][s->off];
	memcpy(&w, &vpcm[side][s->off], sizeof w);
	return w;
}

static int
run_stores(void)
{
	int lvl, d, seen[NSITE], i;
	int quiet = 0, loud = 0;
	unsigned int pre[NSITE];
	struct vpcm_args a;

	diff_begin("VPcmFloModem::enterPhase3 -- the twenty-one stores");

	for (i = 0; i < NSITE; i++)
		seen[i] = 0;

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);

		for (d = 0; d < 6; d++) {
			long tag = (long)lvl * 100 + d;
			int trial = 40 + lvl * 6 + d;

			a.dem.latch = (d & 1) ? 1u : 0u;
			a.dem.flag = 0;
			a.dem.eia6 = (d & 2) ? 6 : 0;
			a.dem.blockByte = 0;
			a.dem.pcmType = d & 1;
			a.dem.idx = d;
			a.dilCount = (trial * 37) & 0xff;
			a.seq1Length = (trial * 13 + 1) % 129;
			a.seq2Length = (trial * 29 + 7) % 129;

			vpcm_setup(trial, &a);
			for (i = 0; i < NSITE; i++)
				pre[i] = read_site(1, &site_v[i]);

			fire();

			compare_everything("after enterPhase3", tag);
			transcripts_agree(tag);

			/*
			 * The constants, by name, on the BLOB's side -- so
			 * this is a claim about the object and not about the
			 * reconstruction agreeing with itself.
			 */
			for (i = 0; i < NSITE; i++) {
				if (site_v[i].width == 0)
					continue;
				diff_eq_int(site_v[i].name,
					    (long)read_site(1, &site_v[i]),
					    (long)site_v[i].value, tag);
				if (pre[i] != site_v[i].value)
					seen[i] = 1;
			}

			/*
			 * The dead store's offset ends up holding the packed
			 * length instead, which is never 0 for any descriptor
			 * -- frame 0 alone is seventeen bits.
			 */
			diff_eq_int("nofBits came back non-zero (%ld)",
				    (long)(V(1)->nofBits != 0), 1, tag);

			if (dsplib_debug_capture_lines(1) == 0)
				quiet++;
			else
				loud++;
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	/*
	 * ANTI-VACUITY.  A store agrees for the right reason only where the
	 * value it replaced was different.  Twenty sites, and every one of
	 * them has to have been seen to change.
	 */
	for (i = 0; i < NSITE; i++) {
		if (site_v[i].width == 0)
			continue;
		diff_eq_int(site_v[i].name, seen[i], 1, 0);
	}

	diff_eq_int("both debug arms were taken", quiet > 0 && loud > 0, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * The demodulator behind the pointer at +0x175c
 * ===========================================================================
 */
static int
run_demodulator(void)
{
	static unsigned char latched[DEM_SLOT], ran[DEM_SLOT];
	static unsigned char withCall[VPCM_SLOT], withoutCall[VPCM_SLOT];
	static const unsigned int latch_v[] = { 0u, 2u, 0xffffffffu };
	struct vpcm_args a;
	int i;

	diff_begin("VPcmFloModem::enterPhase3 -- the demodulator it calls");

	set_level(0);

	a.dem.flag = 0;
	a.dem.eia6 = 6;
	a.dem.blockByte = 0;
	a.dem.pcmType = 0;
	a.dem.idx = 2;
	a.dilCount = 9;
	a.seq1Length = 5;
	a.seq2Length = 7;

	/*
	 * 1.  The callee latches its own +0x34 to 1 whatever it was.  That is
	 *     the shortest proof that the pointer at +0x175c was loaded and
	 *     used: nothing else in this method writes the demodulator.
	 */
	for (i = 0; i < 3; i++) {
		a.dem.latch = latch_v[i];
		run_pair(60 + i, &a);
		compare_everything("after enterPhase3", i);
		diff_eq_int("the demodulator's latch is set (%ld)",
			    (long)D(1)->inPhase3, 1, (long)i);
		diff_eq_int("...on our side too (%ld)", (long)D(0)->inPhase3,
			    1, (long)i);
		if (i == 0)
			memcpy(ran, dem[1], DEM_SLOT);
	}

	/*
	 * 2.  With the latch already set the callee returns at once, so the
	 *     graph it leaves behind must DIFFER from the one it leaves when
	 *     it runs.  Without this, "the demodulator was called" and "the
	 *     demodulator was not called" would be the same observation and
	 *     the pointer would be unproven.
	 */
	a.dem.latch = 1u;
	run_pair(60, &a);
	compare_everything("after enterPhase3", 3);
	memcpy(latched, dem[1], DEM_SLOT);

	diff_eq_int("the latched call leaves a different demodulator",
		    memcmp(latched, ran, DEM_SLOT) != 0, 1, 0);

	/*
	 * 3.  ...and the modem itself must NOT differ between the two: the
	 *     twenty-one stores and the packing happen either way, so a
	 *     difference here would mean the method had read the demodulator
	 *     back, which it does not.
	 */
	a.dem.latch = 0u;
	run_pair(61, &a);
	snap_vpcm(withCall, 0);

	a.dem.latch = 1u;
	run_pair(61, &a);
	snap_vpcm(withoutCall, 0);

	diff_eq_int("the modem's own state does not depend on it",
		    memcmp(withCall, withoutCall, VPCM_SLOT) == 0, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * The DIL descriptor embedded at +0x004
 * ===========================================================================
 */
static int
run_descriptor(void)
{
	static short expect[VPCM_BITS];
	static tagV90DILdescriptor copy;
	static short vecA[VPCM_BITS], vecB[VPCM_BITS];
	static const int len_v[] = { 0, 1, 2, 17, 63, 128 };
	static const int cnt_v[] = { 0, 1, 16, 129, 255 };
	struct vpcm_args a;
	short lenA, lenB, n;
	int i, lvl;
	int sawShort = 0, sawLong = 0;

	diff_begin("VPcmFloModem::enterPhase3 -- the descriptor at +0x004");

	dsplib_debug_capture_on = 1;

	a.dem.latch = 0;
	a.dem.flag = 0;
	a.dem.eia6 = 6;
	a.dem.blockByte = 0;
	a.dem.pcmType = 0;
	a.dem.idx = 1;

	/*
	 * 1.  A sweep over the three lengths, including both extremes of each,
	 *     compared whole.  128 and 255 are the largest the fields can
	 *     describe, and DILdescriptorPacker.h bounds that descriptor at
	 *     2,654 bits -- which must fit `bitVector`.
	 */
	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);

		for (i = 0; i < 6; i++) {
			long tag = (long)lvl * 100 + i;

			a.dilCount = cnt_v[i % 5];
			a.seq1Length = len_v[i];
			a.seq2Length = len_v[5 - i];

			run_pair(70 + lvl * 6 + i, &a);
			compare_everything("after enterPhase3", tag);
			transcripts_agree(tag);

			n = V(1)->nofBits;
			diff_eq_int("the packed length is even (%ld)",
				    (n & 1) == 0, 1, tag);
			diff_eq_int("the packed length fits bitVector (%ld)",
				    n > 0 && n <= DIL_MAX_BITS, 1, tag);

			/*
			 * 2.  THE ADDRESS.  Re-pack a standalone copy of the
			 *     descriptor the object holds, into a copy of the
			 *     bit vector as it was BEFORE the call, and
			 *     require the object's vector to be exactly that
			 *     -- all 2,700 entries, so a byte written past the
			 *     packed length fails here too.  Starting from the
			 *     pre-call contents is what makes the whole-array
			 *     comparison legitimate: a position the packer
			 *     does not write must still hold what it held.
			 */
			memcpy(&copy, &V(0)->dil, sizeof copy);
			memcpy(expect, prevec, sizeof expect);
			DILdescriptorPacker(&copy, expect, &lenA);
			diff_eq_int("the packed length is the descriptor's "
				    "(%ld)", (long)V(0)->nofBits, (long)lenA,
				    tag);
			diff_eq_obj_(__FILE__, __LINE__,
				     "the vector is the descriptor's",
				     "packed bits", V(0)->bitVector, expect,
				     sizeof expect, tag);

			if (n < 400)
				sawShort = 1;
			else
				sawLong = 1;
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	/*
	 * 3.  ANTI-VACUITY, the lengths.  Two descriptors of different lengths
	 *     must pack to different lengths -- otherwise check 2 above would
	 *     hold for a descriptor of any shape and the address would be
	 *     unproven.
	 */
	a.dilCount = 3;
	a.seq1Length = 4;
	a.seq2Length = 5;
	run_pair(80, &a);
	lenA = V(0)->nofBits;

	a.dilCount = 200;
	a.seq1Length = 128;
	a.seq2Length = 128;
	run_pair(80, &a);
	lenB = V(0)->nofBits;

	diff_eq_int("the three lengths reach the packer", lenA != lenB, 1, 0);
	diff_eq_int("...and both a short and a long vector were packed",
		    sawShort && sawLong, 1, 0);

	/*
	 * 4.  ANTI-VACUITY, the contents.  Same three lengths, different
	 *     sequence bytes -- which come out of `fill_pair` and so differ
	 *     between two trials -- must pack to the same LENGTH and a
	 *     different VECTOR.  Without this the descriptor's body could be
	 *     read from anywhere at all and nothing here would notice.
	 */
	a.dilCount = 40;
	a.seq1Length = 100;
	a.seq2Length = 90;

	run_pair(81, &a);
	lenA = V(0)->nofBits;
	memcpy(vecA, V(0)->bitVector, (size_t)lenA * sizeof(short));

	run_pair(82, &a);
	lenB = V(0)->nofBits;
	memcpy(vecB, V(0)->bitVector, (size_t)lenB * sizeof(short));

	diff_eq_int("the same lengths pack to the same length",
		    (long)lenA == (long)lenB, 1, 0);
	diff_eq_int("the descriptor's BODY reaches the packer too",
		    memcmp(vecA, vecB, (size_t)lenA * sizeof(short)) != 0, 1,
		    0);

	return diff_end();
}

/* ============================================ task #88: externalReset */

/*
 * `VPcmFloModem::externalReset` differs from `enterPhase3` in three ways and
 * every one of them needs something this file did not have: it reinitialises
 * a V90Parameters and a V92Parameters through two pointers the fixture never
 * wired, and it calls `V90Demodulator::reInit` when `info0Layout` is
 * non-zero.
 *
 * THE TWO PARAMETER BLOCKS ARE SHARED AND SNAPSHOTTED.  `V90Parameters::init`
 * and `V92Parameters::init` WRITE, so per-side blocks would need the two
 * pointers neutralised and would still compare only what each side wrote into
 * its own copy.  One block, given to both sides, keeps every stored pointer
 * identical (finding 1105) -- and the snapshot-run-restore-run shape (finding
 * 805) is what stops the second writer from hiding the first.
 */

#include "dsplib/V92Parameters.h"

extern "C" {
void ref_vpcm_extreset(void *self)
	asm("ref__ZN12VPcmFloModem13externalResetEv");
}

#define V90P_SLOT (V90PARAMETERS_BOUND + 64)
#define V92P_SLOT (sizeof(V92Parameters) + 64)

static unsigned char v90p[V90P_SLOT] __attribute__((aligned(8)));
static unsigned char v90p_save[V90P_SLOT], v90p_ours[V90P_SLOT];
static unsigned char v92p[V92P_SLOT] __attribute__((aligned(8)));
static unsigned char v92p_save[V92P_SLOT], v92p_ours[V92P_SLOT];

/*
 * BOTH BLOCKS NEED A HOST PARAMETER RECORD.  `V90Parameters::init` calls
 * `loadModemParamsData`, which dereferences the `_tagModemParameters *` at
 * +0x00 for three fields; a seeded pointer there is a segfault, not a test.
 * The record is zeroed rather than seeded, because what it holds is
 * `t_v90params.cpp`'s claim and not this file's -- here it only has to be a
 * real address and the same one on both sides.  It is written by neither
 * side, which the comparison below would notice if it were.
 */
static unsigned char hostparams[256] __attribute__((aligned(8)));

static int
run_externalreset(void)
{
	struct vpcm_args a;
	long tag = 60000;
	int layout, trial;
	unsigned lvl;
	int sawReInit = 0, sawNoReInit = 0, printed = 0;

	diff_begin("VPcmFloModem::externalReset");

	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 2; lvl += 2) {
		set_level(lvl);
		for (layout = 0; layout < 2; layout++)
			for (trial = 0; trial < 4; trial++) {
				int side;
				unsigned int was;

				tag++;
				a.dem.latch = 0;
				a.dem.flag = 0;
				a.dem.eia6 = 6;
				a.dem.blockByte = 0;
				a.dem.pcmType = trial & 1;
				a.dem.idx = trial;
				a.dilCount = 3;
				a.seq1Length = 8;
				a.seq2Length = 8;

				vpcm_setup((int)tag, &a);

				fill_pair(v90p, v90p_save, V90P_SLOT);
				fill_pair(v92p, v92p_save, V92P_SLOT);
				{
					void *hp = hostparams;

					memcpy(v90p, &hp, sizeof hp);
					memcpy(v92p, &hp, sizeof hp);
				}

				for (side = 0; side < 2; side++) {
					V(side)->modem.ptr_49b4 =
					    (V90Parameters *)v90p;
					V(side)->v92modem.parameters =
					    (V92Parameters *)v92p;
					V(side)->info0Layout =
					    layout ? 0x1234 : 0;
					((V90ConnectionEvaluator *)ce[side])
					    ->params = (V90Parameters *)parm[0];
				}
				P3(0)->verificationStatus =
				    P3(1)->verificationStatus = 0xc0de0000u;
				was = P3(1)->verificationStatus;

				memcpy(v90p_save, v90p, V90P_SLOT);
				memcpy(v92p_save, v92p, V92P_SLOT);
				dsplib_debug_capture_reset();

				V(0)->externalReset();

				memcpy(v90p_ours, v90p, V90P_SLOT);
				memcpy(v92p_ours, v92p, V92P_SLOT);
				memcpy(v90p, v90p_save, V90P_SLOT);
				memcpy(v92p, v92p_save, V92P_SLOT);

				ref_vpcm_extreset(V(1));

				compare_everything("after externalReset", tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after externalReset",
					     "V90Parameters (reinitialised)",
					     v90p_ours, v90p, V90P_SLOT, tag);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after externalReset",
					     "V92Parameters (reinitialised)",
					     v92p_ours, v92p, V92P_SLOT, tag);

				/*
				 * Both `init`s really ran: the block came out
				 * different from what it went in as.  A
				 * reconstruction that dropped either call
				 * would pass every comparison above.
				 */
				diff_eq_int("V90Parameters::init wrote (%ld)",
					    memcmp(v90p_save, v90p, V90P_SLOT)
					    != 0, 1, tag);
				diff_eq_int("V92Parameters::init wrote (%ld)",
					    memcmp(v92p_save, v92p, V92P_SLOT)
					    != 0, 1, tag);

				/* The flags, by value and not only compared. */
				diff_eq_int("flags_0217[0] (%ld)",
					    (long)V(1)->flags_0217[0], 1, tag);
				diff_eq_int("flags_0217[1] (%ld)",
					    (long)V(1)->flags_0217[1], 0, tag);
				diff_eq_int("flags_0217[5] (%ld)",
					    (long)V(1)->flags_0217[5], 0, tag);
				diff_eq_int("nofBitsPerSymbol (%ld)",
					    (long)V(1)->nofBitsPerSymbol, 2,
					    tag);
				diff_eq_int("minNofTransmitSequences (%ld)",
					    (long)V(1)->minNofTransmitSequences,
					    1, tag);
				diff_eq_int("word_6f98 (%ld)",
					    (long)V(1)->word_6f98, 0, tag);
				diff_eq_int("word_6fb4 (%ld)",
					    (long)V(1)->word_6fb4, 0, tag);
				diff_eq_int("byte_6118 (%ld)",
					    (long)V(1)->byte_6118, 0, tag);

				/*
				 * THE BRANCH.  `reInit` clears the phase 3
				 * demodulator's verification status, so the
				 * two arms are told apart by something the
				 * object comparison alone would attribute to
				 * the seed.
				 */
				if (layout) {
					diff_eq_int("reInit ran (%ld)",
						    (long)P3(1)
						    ->verificationStatus, 0,
						    tag);
					diff_eq_int("the evaluator was reset "
						    "(%ld)",
						    ((V90ConnectionEvaluator *)
						     ce[1])->word_64, 1600,
						    tag);
					sawReInit = 1;
				} else {
					diff_eq_int("reInit did not run (%ld)",
						    (long)P3(1)
						    ->verificationStatus,
						    (long)was, tag);
					sawNoReInit = 1;
				}

				if (lvl > 1 &&
				    dsplib_debug_capture_lines(1) > 0)
					printed = 1;

				teardown();
			}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the reInit arm was taken", sawReInit, 1, 0);
	diff_eq_int("and skipped", sawNoReInit, 1, 0);
	diff_eq_int("the diagnostics were reached", printed, 1, 0);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_externalreset();
	bad |= run_stores();
	bad |= run_demodulator();
	bad |= run_descriptor();

	return bad;
}
