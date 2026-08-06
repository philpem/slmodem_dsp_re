/*
 * t_vpcmflomodem.cpp -- five members of VPcmFloModem, against the blob.
 *
 * WHAT MAKES THIS TEST NON-TRIVIAL IS THE REACH, NOT THE ARITHMETIC.  The
 * object is 32 KB and these five touch about thirty of its fields, spread
 * from +0x00 to +0x7f27 with a whole embedded V90Modem in the middle.  So
 * every block is allocated separately on each side, seeded identically with
 * varied bytes (finding 230), and COMPARED WHOLE -- a store to the wrong
 * offset lands in a region both sides would otherwise still agree on, and
 * only a whole-slot comparison sees it.  No size is asserted anywhere here,
 * because none is settled; see include/dsplib/VPcmFloModem.h.
 *
 * THE POINTERS HOLD DIFFERENT ADDRESSES ON THE TWO SIDES and always will.
 * Each is replaced, before the compare, by the only thing the two sides can
 * agree about it: whether it still points where setup() put it, or -- for the
 * eight `setPhaseIIinfo` installs -- whether it points at the right one of
 * this side's own four float arrays.  Same idiom as snap_mod/snap_dem in
 * t_v90sessionflag.cpp.
 *
 * THE MODEM'S SIDE IS HELD OUTSIDE {0, 1} THROUGHOUT.  V90Modem::setSessionFlag
 * stores the flag and then fans out to a modulator or a demodulator only for
 * those two values; the fan-out is t_v90sessionflag's claim and reproducing
 * its object graph here would add 40 KB of blocks to prove something already
 * proved.  What THIS test has to see is that the flag reaches +0x1758+0x49b8
 * of the VPcmFloModem, and it sees that whichever way the side goes.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34fsk.h"
#include "dsplib/VPcmFloModem.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

int ref_getUinfoValue(void *self, short probe)
	asm("ref__ZN12VPcmFloModem13getUinfoValueEs");
void ref_setPhaseIIinfo(void *self, int *info0, int rtd)
	asm("ref__ZN12VPcmFloModem14setPhaseIIinfoEPii");
int ref_getV90CpBits(void *self, short *bits)
	asm("ref__ZN12VPcmFloModem12getV90CpBitsEPs");
int ref_getV90JaBits(void *self, short *bits)
	asm("ref__ZN12VPcmFloModem12getV90JaBitsEPs");
void ref_setPcmSessionType(void *self, int sessionType)
	asm("ref__ZN12VPcmFloModem17setPcmSessionTypeEi");
}

/*
 * Slots, each with slack past the modelled prefix so a store that overruns
 * shows up as a difference rather than as memory nobody looks at.
 */
#define SLACK		64

#define VPCM_SLOT	(0x7f28 + SLACK)
#define P90_SLOT	(0x24 + SLACK)
#define P92_SLOT	(0x28 + SLACK)
#define DEM_SLOT	(0x210 + SLACK)
#define SEEN_SLOT	(0x80 + SLACK)
#define U49_SLOT	(0x40 + SLACK)

static unsigned char vp[2][VPCM_SLOT] __attribute__((aligned(8)));
static unsigned char p90[2][P90_SLOT] __attribute__((aligned(8)));
static unsigned char p92[2][P92_SLOT] __attribute__((aligned(8)));
static unsigned char dem[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char seen[2][SEEN_SLOT] __attribute__((aligned(8)));
static unsigned char u49[2][U49_SLOT] __attribute__((aligned(8)));

/* The V.34 object the three V34XF_* handouts read.  One per side. */
static struct v34_object v34[2];

static short outbits[2];

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * VARIED BYTES, NEVER ZEROS (finding 230).  A zero fill makes a field the
 * function never writes compare equal for the wrong reason, and most of this
 * object is fields these five never write.
 */
static void
fill_pair(void *a, void *b, size_t n)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	size_t i;

	for (i = 0; i < n; i++)
		pa[i] = pb[i] = next_byte();
}

static VPcmFloModem *
M(int side)
{
	return (VPcmFloModem *)vp[side];
}

static V90Phase2Info *
P90(int side)
{
	return (V90Phase2Info *)p90[side];
}

static V92Phase2Info *
P92(int side)
{
	return (V92Phase2Info *)p92[side];
}

/*
 * Seed every block, then wire each side's graph to that side's own blocks.
 *
 * The probe magnitudes are all different, which is what pins the twenty-five
 * entry selection mask by POSITION and not merely by how many ones it has:
 * swap two adjacent mask bits and two L2 entries change.  They are also all
 * strictly positive, so log10 stays in its domain -- see the note in main().
 *
 * AND THEY ARE DELIBERATELY NOT DYADIC.  They used to be 1000 + 137i + i*i/2,
 * every one of them a multiple of a half -- so every one of them, divided by
 * 16384, was EXACTLY a float, the first of the three roundings did nothing,
 * and a mutation that skipped it went uncaught.  The irrational-looking term
 * is what makes the rounding real: swept over 20 million magnitudes, dropping
 * it changes the answer for 2.42% of them.
 */
static void
setup(int trial)
{
	int side, i;

	lfsr_state = 0x4d1u + 0x9e37u * (unsigned)trial;

	fill_pair(vp[0], vp[1], VPCM_SLOT);
	fill_pair(p90[0], p90[1], P90_SLOT);
	fill_pair(p92[0], p92[1], P92_SLOT);
	fill_pair(dem[0], dem[1], DEM_SLOT);
	fill_pair(seen[0], seen[1], SEEN_SLOT);
	fill_pair(u49[0], u49[1], U49_SLOT);
	fill_pair(&v34[0], &v34[1], sizeof(struct v34_object));

	outbits[0] = outbits[1] = (short)0x5a5a;

	for (side = 0; side < 2; side++) {
		VPcmFloModem *m = M(side);

		m->v34Object = &v34[side];
		m->modem.modulator = (V90Modulator *)0;
		m->modem.demodulator = (V90Demodulator *)dem[side];
		m->modem.phase2Info = P90(side);
		m->modem.ptr_49b4 = u49[side];
		m->modem.sessionFlag = 0x11223344u;
		m->modem.side = 2;		/* neither half; see the head */
		m->v92Phase2Info = P92(side);

		((V90Demodulator *)dem[side])->connectionEvaluator =
		    (V90ConnectionEvaluator *)seen[side];

		for (i = 0; i < V34_PROBE_RESULTS; i++)
			v34[side].probe_results[i] =
			    1013.0 + 137.0 * i + 0.5 * i * i
			    + 0.00073156789 * (trial * 25 + i + 1);
		for (i = 0; i < V34_INFO0_BITS; i++)
			v34[side].info0_bits[i] = 0;
		v34[side].rtd = 1234;
	}
}

/*
 * Copy a block, replacing every pointer in it with whether it still points
 * where it is supposed to.  The live block is not disturbed.
 */
static void
snap_vp(unsigned char *dst, int side)
{
	VPcmFloModem *s;

	memcpy(dst, vp[side], VPCM_SLOT);
	s = (VPcmFloModem *)dst;
	s->v34Object = (void *)(long)(M(side)->v34Object == &v34[side]);
	s->modem.demodulator = (V90Demodulator *)(long)
	    (M(side)->modem.demodulator == (V90Demodulator *)dem[side]);
	s->modem.phase2Info = (V90Phase2Info *)(long)
	    (M(side)->modem.phase2Info == P90(side));
	s->modem.ptr_49b4 = (void *)(long)(M(side)->modem.ptr_49b4 == u49[side]);
	s->v92Phase2Info = (V92Phase2Info *)(long)
	    (M(side)->v92Phase2Info == P92(side));
}

/*
 * The four float arrays, as a small integer: 1..4 for the four in offset
 * order, 0 for anything else.  A pointer that ends up holding the wrong one
 * of the four is the failure this is here to see, so "is it non-null" would
 * not do.
 */
static long
which_array(int side, const float *p)
{
	VPcmFloModem *m = M(side);

	if (p == m->array_7dd8)
		return 1;
	if (p == m->array_7e2c)
		return 2;
	if (p == m->L2)
		return 3;
	if (p == m->array_7ed4)
		return 4;
	return 0;
}

static void
snap_p90(unsigned char *dst, int side)
{
	V90Phase2Info *s;

	memcpy(dst, p90[side], P90_SLOT);
	s = (V90Phase2Info *)dst;
	s->array_10 = (float *)which_array(side, P90(side)->array_10);
	s->array_14 = (float *)which_array(side, P90(side)->array_14);
	s->L2 = (float *)which_array(side, P90(side)->L2);
	s->array_1c = (float *)which_array(side, P90(side)->array_1c);
}

static void
snap_p92(unsigned char *dst, int side)
{
	V92Phase2Info *s;

	memcpy(dst, p92[side], P92_SLOT);
	s = (V92Phase2Info *)dst;
	s->array_18 = (float *)which_array(side, P92(side)->array_18);
	s->array_1c = (float *)which_array(side, P92(side)->array_1c);
	s->L2 = (float *)which_array(side, P92(side)->L2);
	s->array_24 = (float *)which_array(side, P92(side)->array_24);
}

static void
snap_dem(unsigned char *dst, int side)
{
	V90Demodulator *s;

	memcpy(dst, dem[side], DEM_SLOT);
	s = (V90Demodulator *)dst;
	s->connectionEvaluator = (V90ConnectionEvaluator *)(long)
	    (((V90Demodulator *)dem[side])->connectionEvaluator
	     == (V90ConnectionEvaluator *)seen[side]);
}

/* Every block, compared whole. */
static void
compare_all(const char *what, long tag)
{
	static unsigned char a[VPCM_SLOT], b[VPCM_SLOT];

	snap_vp(a, 0);
	snap_vp(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "VPcmFloModem block",
		     a, b, VPCM_SLOT, tag);

	snap_p90(a, 0);
	snap_p90(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase2Info block",
		     a, b, P90_SLOT, tag);

	snap_p92(a, 0);
	snap_p92(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V92Phase2Info block",
		     a, b, P92_SLOT, tag);

	snap_dem(a, 0);
	snap_dem(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Demodulator block",
		     a, b, DEM_SLOT, tag);

	diff_eq_obj_(__FILE__, __LINE__, what, "+0x20c target block",
		     seen[0], seen[1], SEEN_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "+0x49b4 target block",
		     u49[0], u49[1], U49_SLOT, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "v34_object",
		     &v34[0], &v34[1], sizeof(struct v34_object), tag);

	diff_eq_int("transcript text (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	diff_eq_int("transcript line count (%ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), tag);
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
}

/*
 * ===========================================================================
 * getV90JaBits
 * ===========================================================================
 *
 * The three things that can go wrong are the wrap, the terminate and the two
 * bits' weights, so the cases below drive the pointer to one step short of
 * the wrap, one step past it and nowhere near it, with the terminate flag on
 * and off.  `nofBits` is signed and `bitPointer` unsigned, so both get a
 * value with the top bit set.
 */
struct ja_case {
	short nofBits;
	unsigned short bitPointer;
	unsigned char terminateJa;
};

static const struct ja_case ja_v[] = {
	{  0,	  0,	0 },		/* nothing live at all		*/
	{  0,	  7,	1 },		/* ... and terminating		*/
	{  6,	  0,	0 },
	{  6,	  2,	0 },
	{  6,	  4,	0 },		/* the wrap			*/
	{  6,	  4,	1 },		/* the wrap AND the terminate	*/
	{  6,	  0,	1 },
	{  6,	  0,	0x80 },		/* non-zero is not just 1	*/
	{ 40,	 17,	0 },
	{ 40,	 38,	0 },		/* the wrap, further out	*/
	{  1,	  0,	0 },		/* pointer steps PAST nofBits	*/
	{ -4,	  0,	0 },		/* negative count, non-zero	*/
	{ (short)0x8000, 3, 0 }
};
#define NJA ((int)(sizeof(ja_v) / sizeof(ja_v[0])))

static int
run_jabits(void)
{
	int i, lvl, wrapped = 0, terminated = 0, wrote = 0;

	diff_begin("VPcmFloModem::getV90JaBits");
	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);
		for (i = 0; i < NJA; i++) {
			long tag = (long)lvl * 100 + i;
			int r0, r1;

			setup(i + lvl * NJA);
			for (int s = 0; s < 2; s++) {
				M(s)->nofBits = ja_v[i].nofBits;
				M(s)->bitPointer = ja_v[i].bitPointer;
				M(s)->terminateJa = ja_v[i].terminateJa;
			}
			dsplib_debug_capture_reset();

			r0 = M(0)->getV90JaBits(&outbits[0]);
			r1 = ref_getV90JaBits(vp[1], &outbits[1]);

			compare_all("after getV90JaBits", tag);
			diff_eq_int("return (%ld)", r0, r1, tag);
			diff_eq_int("packed bits (%ld)",
				    (long)(unsigned short)outbits[0],
				    (long)(unsigned short)outbits[1], tag);

			/*
			 * Witnesses, read off the BLOB's state rather than
			 * off the case table, so a case table that stopped
			 * reaching a path would show up here.
			 */
			if (ja_v[i].terminateJa == 0 && ja_v[i].nofBits != 0 &&
			    M(1)->bitPointer == 0)
				wrapped++;
			if (r1 != 0)
				terminated++;
			if ((unsigned short)outbits[1] != 0x5a5a)
				wrote++;
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the wrap was taken (%ld)", wrapped > 0, 1, 0);
	diff_eq_int("termination was reported (%ld)", terminated > 0, 1, 0);
	diff_eq_int("the caller's short was written (%ld)", wrote > 0, 1, 0);
	return diff_end();
}

/*
 * ===========================================================================
 * getV90CpBits
 * ===========================================================================
 *
 * Reaching the end-of-sequence tail needs `bitPointer + nofBitsPerSymbol ==
 * cpNofBits` exactly, so the cases below carry the pointer and let the
 * sequence length follow from it -- that arithmetic is satisfiable by
 * construction and is checked at run time all the same, by counting how many
 * times the blob actually took the tail (findings 247, 262).
 *
 * The three flags then select between: report termination, load the CPnot
 * vector, or neither.  The load has FOUR preconditions at once
 * (terminateCpNot clear, terminateCp set, cpNotLoaded clear, and the
 * post-increment sequence count at or above the minimum) and one case sets
 * all four.
 *
 * `nofBitsPerSymbol` is a byte and the shift is masked to five bits by the
 * hardware, so two cases carry counts above 31.
 */
struct cp_case {
	short cpNofBits;
	unsigned short bitPointer;
	unsigned char nofBitsPerSymbol;
	unsigned char terminateJa;	/* not read here; kept varied  */
	unsigned char terminateCp;
	unsigned char terminateCpNot;
	unsigned char cpNotLoaded;
	unsigned short nofTransmitSequences;
	unsigned short minNofTransmitSequences;
	short nofBits;
};

static const struct cp_case cp_v[] = {
	/*  cpN  ptr  bps  ja  cp cpN' load  n  min  nofBits */
	{    0,   3,   4,  1,  1,  1,  0,   0,   0,	  8 },	/* dead	 */
	{   32,   0,   4,  0,  0,  0,  0,   0,   0,	  8 },	/* mid	 */
	{   32,  12,   4,  0,  0,  0,  0,   3,   9,	  8 },
	{   16,  12,   4,  1,  0,  0,  0,   3,   9,	  8 },	/* tail	 */
	{   16,  12,   4,  0,  1,  0,  0,   3,   9,	  8 },	/* min	 */
	{   16,  12,   4,  0,  1,  0,  0,   9,   3,	 11 },	/* LOAD	 */
	{   16,  12,   4,  0,  1,  0,  1,   9,   3,	 11 },	/* latch */
	{   16,  12,   4,  0,  0,  1,  0,   9,   3,	 11 },	/* term	 */
	{   16,  12,   4,  0,  1,  1,  0,   9,   3,	 11 },	/* term	 */
	{   16,  16,   0,  0,  1,  0,  0,   4,   5,	 11 },	/* n+1==min */
	{   16,  16,   0,  0,  1,  0,  0,   3,   5,	 11 },	/* below */
	{   40,   7,  33,  0,  1,  0,  0,   6,   1,	 21 },	/* shift>31 */
	{   48,   8,  40,  0,  0,  0,  0,   6,   1,	 21 },	/* shift>31 */
	{  200,   0, 200,  0,  1,  0,  0,  10,   1,	500 },	/* long	 */
	{   16,  12,   4,  0,  1,  0,  0, 0xffff, 3,	 11 },	/* wraps */
	{   16,  12,   4,  0,  1,  0,  0,   9,   3,	  0 },	/* empty src */
	{   16,  12,   4,  0,  1,  0,  0,   9,   3,	 -3 }	/* negative  */
};
#define NCP ((int)(sizeof(cp_v) / sizeof(cp_v[0])))

static int
run_cpbits(void)
{
	int i, lvl, tail = 0, loaded = 0, terminated = 0, printed = 0;

	diff_begin("VPcmFloModem::getV90CpBits");
	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);
		for (i = 0; i < NCP; i++) {
			long tag = (long)lvl * 100 + i;
			int r0, r1;
			unsigned short before;

			setup(i + 7 + lvl * NCP);
			for (int s = 0; s < 2; s++) {
				VPcmFloModem *m = M(s);

				m->cpNofBits = cp_v[i].cpNofBits;
				m->bitPointer = cp_v[i].bitPointer;
				m->nofBitsPerSymbol = cp_v[i].nofBitsPerSymbol;
				m->terminateJa = cp_v[i].terminateJa;
				m->terminateCp = cp_v[i].terminateCp;
				m->terminateCpNot = cp_v[i].terminateCpNot;
				m->cpNotLoaded = cp_v[i].cpNotLoaded;
				m->nofTransmitSequences =
				    cp_v[i].nofTransmitSequences;
				m->minNofTransmitSequences =
				    cp_v[i].minNofTransmitSequences;
				m->nofBits = cp_v[i].nofBits;
			}
			before = M(1)->nofTransmitSequences;
			dsplib_debug_capture_reset();

			r0 = M(0)->getV90CpBits(&outbits[0]);
			r1 = ref_getV90CpBits(vp[1], &outbits[1]);

			compare_all("after getV90CpBits", tag);
			diff_eq_int("return (%ld)", r0, r1, tag);
			diff_eq_int("packed bits (%ld)",
				    (long)(unsigned short)outbits[0],
				    (long)(unsigned short)outbits[1], tag);

			if (M(1)->nofTransmitSequences !=
			    (unsigned short)before)
				tail++;
			if (cp_v[i].cpNotLoaded == 0 && M(1)->cpNotLoaded == 1)
				loaded++;
			if (r1 != 0)
				terminated++;
			printed += (int)dsplib_debug_capture_lines(1);
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the end-of-sequence tail ran (%ld)", tail > 0, 1, 0);
	diff_eq_int("the CPnot vector was loaded (%ld)", loaded > 0, 1, 0);
	diff_eq_int("termination was reported (%ld)", terminated > 0, 1, 0);
	diff_eq_int("a diagnostic was emitted (%ld)", printed > 0, 1, 0);
	return diff_end();
}

/*
 * ===========================================================================
 * setPcmSessionType
 * ===========================================================================
 *
 * The field gets a 0/1, the V.92 record gets the argument's LOW BYTE and the
 * modem gets the argument whole, so a value that is non-zero, wider than a
 * byte and different from 1 in its low byte separates all three.  The message
 * says 92 for every non-zero including a negative one, which is why the
 * comparison in the object is unsigned.
 */
static const int sess_v[] = {
	0, 1, 2, 92, 90, 0x100, 0x101, 0x1ff, -1, 0x7fffffff,
	(int)0x80000000, 0x5a5a5a00
};
#define NSESS ((int)(sizeof(sess_v) / sizeof(sess_v[0])))

static int
run_sessiontype(void)
{
	int i, lvl, printed = 0, sawLowByte = 0;

	diff_begin("VPcmFloModem::setPcmSessionType");
	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);
		for (i = 0; i < NSESS; i++) {
			long tag = (long)lvl * 100 + i;

			setup(i + 23 + lvl * NSESS);
			dsplib_debug_capture_reset();

			M(0)->setPcmSessionType(sess_v[i]);
			ref_setPcmSessionType(vp[1], sess_v[i]);

			compare_all("after setPcmSessionType", tag);
			diff_eq_int("the flag reached the modem (%ld)",
				    (long)M(1)->modem.sessionFlag,
				    (long)(unsigned int)sess_v[i], tag);

			printed += (int)dsplib_debug_capture_lines(1);
			if (P92(1)->v92CapabilitiesLocal !=
			    (unsigned char)(M(1)->pcmSessionType))
				sawLowByte++;
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the message was emitted (%ld)", printed > 0, 1, 0);
	diff_eq_int("the byte and the flag differed (%ld)",
		    sawLowByte > 0, 1, 0);
	return diff_end();
}

/*
 * ===========================================================================
 * setPhaseIIinfo
 * ===========================================================================
 *
 * The INFO0 bits are not held to 0 and 1: the power sum multiplies by a
 * weight table, so 3 in bit 33 has to come out as 3 and not as 1, and the two
 * remote bytes are truncated to a char, so 0x100 has to come out as 0.
 *
 * `info0Layout` is the branch, and it gates a SKIP and a SWAP.  The witnesses
 * below check the blob really took each arm, read off its own output rather
 * than off the loop variable.
 */
static int info0[V34_INFO0_BITS];

static void
load_info0(int variant)
{
	int i;

	for (i = 0; i < V34_INFO0_BITS; i++)
		info0[i] = 0;

	switch (variant) {
	case 0:
		break;
	case 1:
		info0[33] = 1;
		break;
	case 2:
		info0[34] = 1;
		break;
	case 3:
		info0[35] = 1;
		break;
	case 4:
		info0[36] = 1;
		break;
	case 5:
		info0[37] = 1;
		break;
	case 6:
		info0[33] = info0[34] = info0[35] = 1;
		info0[36] = info0[37] = 1;
		break;
	case 7:
		info0[33] = 3;			/* not a bit at all */
		info0[37] = 2;
		break;
	case 8:
		info0[26] = 1;
		info0[27] = 0;
		break;
	case 9:
		info0[26] = 0;
		info0[27] = 1;
		break;
	case 10:
		info0[26] = 0x41;
		info0[27] = 0x42;
		break;
	case 11:
		info0[26] = 0x100;		/* truncates to 0 */
		info0[27] = 0x1ff;
		break;
	case 12:
		info0[38] = 1;
		break;
	case 13:
		info0[39] = 1;
		break;
	case 14:
		info0[38] = 7;
		info0[39] = 0x100;		/* non-zero, wide */
		break;
	case 15:
		info0[38] = info0[39] = 1;
		info0[26] = 0x11;
		info0[27] = 0x22;
		info0[33] = info0[35] = info0[37] = 1;
		break;
	default:
		break;
	}
}
#define NINFO	16

static const int layout_v[] = { 0, 1, -1, 0x100 };
#define NLAYOUT ((int)(sizeof(layout_v) / sizeof(layout_v[0])))

static const int rtd_v[] = { 0, 1, -1, 480, 0x7fffffff };
#define NRTD ((int)(sizeof(rtd_v) / sizeof(rtd_v[0])))

static int
run_phase2(void)
{
	int v, L, r, lvl, printed = 0, sawSwapA = 0, sawSwapB = 0, skipped = 0;

	diff_begin("VPcmFloModem::setPhaseIIinfo");
	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);
		for (L = 0; L < NLAYOUT; L++) {
			for (v = 0; v < NINFO; v++) {
				long tag = (long)lvl * 10000 + L * 100 + v;
				int pcm0, pcm1;

				r = (v + L) % NRTD;
				setup(v + 41 + L * NINFO + lvl * 97);
				load_info0(v);
				for (int s = 0; s < 2; s++) {
					M(s)->info0Layout = layout_v[L];
					M(s)->pcmSessionType = (v & 1);
				}
				pcm1 = P90(1)->pcmType;
				dsplib_debug_capture_reset();

				M(0)->setPhaseIIinfo(info0, rtd_v[r]);
				ref_setPhaseIIinfo(vp[1], info0, rtd_v[r]);

				compare_all("after setPhaseIIinfo", tag);

				pcm0 = P90(1)->pcmType;
				printed += (int)dsplib_debug_capture_lines(1);
				if (P92(1)->shortPhase2Remote ==
				    (unsigned char)info0[26] &&
				    info0[26] != info0[27])
					sawSwapA++;
				if (P92(1)->shortPhase2Remote ==
				    (unsigned char)info0[27] &&
				    info0[26] != info0[27])
					sawSwapB++;
				if (layout_v[L] == 0 && pcm0 == pcm1)
					skipped++;
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("bit 26 reached the short-phase-2 byte (%ld)",
		    sawSwapA > 0, 1, 0);
	diff_eq_int("bit 27 reached it instead (%ld)", sawSwapB > 0, 1, 0);
	diff_eq_int("the skip arm left pcmType alone (%ld)", skipped > 0, 1, 0);
	diff_eq_int("the P2 REPORT lines were emitted (%ld)", printed > 0, 1, 0);
	return diff_end();
}

/*
 * ===========================================================================
 * getUinfoValue
 * ===========================================================================
 *
 * The float half is the point of this one: three roundings, a coprocessor
 * log10 and a selection mask whose positions matter.  Nothing is compared
 * with a tolerance -- the four arrays are inside the block comparison and are
 * therefore compared as bytes, which is the only thing that can settle
 * whether the x87 sequence was reproduced or merely approximated.
 */
static const short skip_v[] = { 0, 1, -1, 0x100, 0x7fff };
#define NSKIP ((int)(sizeof(skip_v) / sizeof(skip_v[0])))

static const short uinfo_v[] = { 0, 1, -1, 300, (short)0x8000 };
#define NUINFO ((int)(sizeof(uinfo_v) / sizeof(uinfo_v[0])))

static int
run_uinfo(void)
{
	int k, u, f, lvl, ran = 0, nonzero = 0, defaulted = 0, printed = 0;

	diff_begin("VPcmFloModem::getUinfoValue");
	dsplib_debug_capture_on = 1;

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);
		for (k = 0; k < NSKIP; k++) {
			for (u = 0; u < NUINFO; u++) {
				for (f = 0; f < 2; f++) {
					long tag = (long)lvl * 100000 +
					    k * 1000 + u * 10 + f;
					int r0, r1;

					setup(k * 13 + u * 5 + f +
					      lvl * 211 + 3);
					for (int s = 0; s < 2; s++) {
						M(s)->flag_173d =
						    (unsigned char)f;
						M(s)->info0Layout = 1;
						M(s)->pcmSessionType = f;
						*(short *)(u49[s] + 0x20) =
						    uinfo_v[u];
					}
					dsplib_debug_capture_reset();

					r0 = M(0)->getUinfoValue(skip_v[k]);
					r1 = ref_getUinfoValue(vp[1],
							       skip_v[k]);

					compare_all("after getUinfoValue", tag);
					diff_eq_int("return (%ld)", r0, r1, tag);

					if (M(1)->L2[20] != 0.0f)
						ran++;
					if (r1 != 0)
						nonzero++;
					if (M(1)->flags_0217[0] == 1 &&
					    M(1)->flags_0217[1] == 0 &&
					    M(1)->flags_0217[2] == 1 &&
					    M(1)->flags_0217[3] == 1 &&
					    M(1)->flags_0217[4] == 1 &&
					    M(1)->flags_0217[5] == 1)
						defaulted++;
					printed +=
					    (int)dsplib_debug_capture_lines(1);
				}
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the L2 computation ran (%ld)", ran > 0, 1, 0);
	diff_eq_int("a non-zero Uinfo came back (%ld)", nonzero > 0, 1, 0);
	diff_eq_int("the six default flags were written (%ld)",
		    defaulted > 0, 1, 0);
	diff_eq_int("the L2 transcript was emitted (%ld)", printed > 0, 1, 0);
	return diff_end();
}

/*
 * The probe magnitudes are strictly positive throughout, so log10 never
 * leaves its domain.  What that leaves untested is stated rather than
 * papered over: a zero or negative probe magnitude makes fyl2x produce
 * -infinity or a NaN, and the SIGN CHARACTER in the L2 transcript would then
 * diverge -- `fcompp` reads an unordered comparison as "greater" and C's
 * `>` reads it as false.  Every path through the arithmetic that a real
 * probe can produce is covered; that one edge is a known gap, recorded in
 * docs/findings.md rather than tested here.
 */
int
main(void)
{
	int bad = 0;

	bad |= run_jabits();
	bad |= run_cpbits();
	bad |= run_sessiontype();
	bad |= run_phase2();
	bad |= run_uinfo();

	return bad;
}
