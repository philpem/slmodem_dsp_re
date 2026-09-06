/*
 * t_v34info1a.cpp -- `V34SetINFO1aBits`, against the blob.
 *
 * WHY THIS IS NOT PART OF t_v34info.  The function under test calls
 * `VPcmFloModem::getUinfoValue`, so `obj->p3548` has to be a real 32 KB
 * `VPcmFloModem` with its whole object graph wired up -- an embedded
 * V90Modem, two Phase 2 records, a demodulator and two loose blocks -- and
 * `t_v34info.c` is a C translation unit that cannot name the class.  It keeps
 * its 0x6140-byte byte array; this test builds the real thing, the way
 * t_vpcmflomodem.cpp does, and shares that file's machinery down to the LFSR.
 *
 * SO THE OBJECT GRAPH IS A CYCLE and both directions are set up here:
 * `v34[side].p3548` is the modem and `modem->v34Object` is the V.34 object.
 * The blob follows both.
 *
 * EVERY BLOCK IS COMPARED WHOLE, with slack past the modelled prefix, because
 * a store to the wrong offset lands where both sides would otherwise still
 * agree.  Pointers hold different addresses on the two sides and always will,
 * so each is replaced before the compare by the only thing the two sides can
 * agree about: whether it still points where setup() put it.
 *
 * WHAT A WHOLE-BUFFER COMPARE WOULD NOT CATCH, and what is done about it.
 * This is a bit-setting function; a test that only asserts "the message came
 * out the same" passes while never driving a field's width or its boundary.
 * So the case tables below drive, by construction:
 *
 *   - the PCM law int at `pac18 + 0xc` to 0, 1, 2 and 0x10000.  One branch
 *     tests it `!= 0` and the other `== 1`; with only {0, 1} the two are
 *     indistinguishable, and 0x10000 additionally catches a 16-bit misread.
 *   - `short_35a4` across the seven-bit boundary and negative -- 0, 0x7f, 0x80,
 *     0x1234 and -1 -- because it is `movswl`-loaded and then truncated.
 *   - `short_abce`, `short_abd0` and `short_abd2` with bits above 7 set, because the object
 *     reads them with `testb` and those bits must NOT reach the message.
 *   - `bits[7]` with a non-zero high byte on the INFO1d-clear path, because
 *     the clear is `and $0xdf` on a zero-extended short and not `&= ~0x20`.
 *   - `bits[2]` and `bits[3]` varied on entry, because the V.34-upstream
 *     branch recovers the upstream baud index out of them before
 *     overwriting them.
 *
 * Every one of those has a counter at the bottom of the run asserting that
 * the case really occurred, so a table edited into vacuity fails loudly
 * rather than passing on a smaller cross-product (findings F247, F262, F295).
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34info.h"
#include "dsplib/VPcmFloModem.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

int ref_V34SetINFO1aBits(void *obj, short *bits);
}

#define SLACK		64

#define VPCM_SLOT	(0x7f28 + SLACK)
#define P90_SLOT	(0x24 + SLACK)
#define P92_SLOT	(0x28 + SLACK)
#define DEM_SLOT	(0x210 + SLACK)
#define SEEN_SLOT	(0x80 + SLACK)
#define U49_SLOT	(0x40 + SLACK)
#define PCM_SLOT	(0x40 + SLACK)

/* Where in the block `pac18` points at the PCM law lives. */
#define PCM_LAW		0x0c

/* Where in the +0x49b4 block getUinfoValue finds the Uinfo. */
#define U49_UINFO	0x20

static unsigned char vp[2][VPCM_SLOT] __attribute__((aligned(8)));
static unsigned char p90[2][P90_SLOT] __attribute__((aligned(8)));
static unsigned char p92[2][P92_SLOT] __attribute__((aligned(8)));
static unsigned char dem[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char seen[2][SEEN_SLOT] __attribute__((aligned(8)));
static unsigned char u49[2][U49_SLOT] __attribute__((aligned(8)));
static unsigned char pcm[2][PCM_SLOT] __attribute__((aligned(8)));

/*
 * THE V.34 OBJECT AND ITS SLACK ARE ONE OBJECT, not two statics: nothing
 * requires the linker to lay two statics out adjacently, and slack that did
 * not follow the block it guards would be decoration.  Same for the message.
 */
static struct { struct v34_object o; unsigned char g[SLACK]; }
	vb[2] __attribute__((aligned(8)));
static struct { short o[V34_INFO_MSG_SHORTS]; unsigned char g[SLACK]; }
	mb[2] __attribute__((aligned(8)));

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/* VARIED BYTES, NEVER ZEROS (finding F230). */
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

static void
setup(int trial)
{
	int side, i;

	lfsr_state = 0x1f3du + 0x9e37u * (unsigned)trial;

	fill_pair(vp[0], vp[1], VPCM_SLOT);
	fill_pair(p90[0], p90[1], P90_SLOT);
	fill_pair(p92[0], p92[1], P92_SLOT);
	fill_pair(dem[0], dem[1], DEM_SLOT);
	fill_pair(seen[0], seen[1], SEEN_SLOT);
	fill_pair(u49[0], u49[1], U49_SLOT);
	fill_pair(pcm[0], pcm[1], PCM_SLOT);
	fill_pair(&vb[0], &vb[1], sizeof(vb[0]));
	fill_pair(&mb[0], &mb[1], sizeof(mb[0]));

	for (side = 0; side < 2; side++) {
		VPcmFloModem *m = M(side);

		m->v34Object = &vb[side].o;
		m->modem.modulator = (V90Modulator *)0;
		m->modem.demodulator = (V90Demodulator *)dem[side];
		m->modem.phase2Info = P90(side);
		m->modem.params = (V90Parameters *)u49[side];
		m->modem.sessionFlag = 0x11223344u;
		/*
		 * NEITHER HALF.  V90Modem::setSessionFlag -- which
		 * getUinfoValue reaches through setPhaseIIinfo -- fans out to
		 * a modulator or a demodulator only for sides 0 and 1, and
		 * reproducing that object graph here would re-prove
		 * t_v90sessionflag's claim.  What this test has to see is that
		 * the flag reaches the VPcmFloModem, and it sees that either
		 * way.
		 */
		m->modem.side = (V90ModemSide)2;
		m->v92modem.phase2Info = P92(side);

		((V90Demodulator *)dem[side])->connectionEvaluator =
		    (V90ConnectionEvaluator *)seen[side];

		vb[side].o.p3548 = m;
		vb[side].o.pac18 = pcm[side];

		/*
		 * STRICTLY POSITIVE AND NOT DYADIC.  getUinfoValue takes
		 * log10 of each of these over 2^14, so a zero or negative one
		 * leaves the domain; and a value that is exactly a float after
		 * the scaling hides the first of the three roundings.  Both
		 * points are t_vpcmflomodem.cpp's and both still apply,
		 * because this test calls the same code.
		 */
		for (i = 0; i < V34_PROBE_RESULTS; i++)
			vb[side].o.probe_results[i] =
			    1013.0 + 137.0 * i + 0.5 * i * i
			    + 0.00073156789 * (trial * 25 + i + 1);
		for (i = 0; i < V34_INFO0_BITS; i++)
			vb[side].o.info0_bits[i] = (trial >> (i & 7)) & 1;
		vb[side].o.rtd = (short)(1000 + trial);
	}
}

/* Copy a block, replacing every pointer with whether it still points home. */
static void
snap_v34(unsigned char *dst, int side)
{
	struct v34_object *s;

	memcpy(dst, &vb[side], sizeof(vb[side]));
	s = (struct v34_object *)dst;
	s->p3548 = (void *)(long)(vb[side].o.p3548 == M(side));
	s->pac18 = (void *)(long)(vb[side].o.pac18 == pcm[side]);
}

static void
snap_vp(unsigned char *dst, int side)
{
	VPcmFloModem *s;

	memcpy(dst, vp[side], VPCM_SLOT);
	s = (VPcmFloModem *)dst;
	s->v34Object = (void *)(long)(M(side)->v34Object == &vb[side].o);
	s->modem.demodulator = (V90Demodulator *)(long)
	    (M(side)->modem.demodulator == (V90Demodulator *)dem[side]);
	s->modem.phase2Info = (V90Phase2Info *)(long)
	    (M(side)->modem.phase2Info == P90(side));
	s->modem.params = (V90Parameters *)(long)
	    (M(side)->modem.params == (V90Parameters *)u49[side]);
	s->v92modem.phase2Info = (V92Phase2Info *)(long)
	    (M(side)->v92modem.phase2Info == P92(side));
}

/*
 * The four float arrays, as a small integer: 1..4 in offset order, 0 for
 * anything else.  A pointer holding the WRONG one of the four is the failure
 * this is here to see, so "is it non-null" would not do.
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

static void
compare_all(const char *what, long tag)
{
	static unsigned char a[VPCM_SLOT], b[VPCM_SLOT];
	static unsigned char va[sizeof(vb[0])], vbb[sizeof(vb[0])];

	diff_eq_obj_(__FILE__, __LINE__, what, "INFO message",
		     &mb[0], &mb[1], sizeof(mb[0]), tag);

	snap_v34(va, 0);
	snap_v34(vbb, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "v34_object",
		     va, vbb, sizeof(vb[0]), tag);

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
	diff_eq_obj_(__FILE__, __LINE__, what, "pac18 block",
		     pcm[0], pcm[1], PCM_SLOT, tag);

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
 * ---------------------------------------------------------------------------
 * The cases.
 *
 * `shape` picks which of the four prologue arms runs and whether the tail
 * runs at all; `tail` picks the path through it.  They are crossed with each
 * other and with the PCM law and `short_35a4`, so every prologue arm is seen with
 * every tail and every law value.
 *
 * `v90` and `k56` take values other than 1 where they are meant to be
 * non-zero, because the function stores 2 into `v90_receiver` and a test that
 * only ever started it at 1 would not see a missing store.
 */
struct shape {
	int v90;
	int k56;
	short role;
};

static const struct shape shape_v[] = {
	{ 0, 0, 0x65 },		/* neither receiver: the lone seven-bit field */
	{ 0, 0, 0x66 },		/* ... and the role must not matter here      */
	{ 0, 5, 0x65 },		/* K56Flex caller, no tail                    */
	{ 0, 5, 0x66 },		/* K56Flex answer, no tail                    */
	{ 3, 0, 0x65 },		/* tail only                                  */
	{ 1, 7, 0x65 },		/* K56Flex caller AND the tail                */
	{ 1, 7, 0x66 }		/* K56Flex answer AND the tail                */
};
#define NSHAPE	((int)(sizeof(shape_v) / sizeof(shape_v[0])))

struct tail {
	int layout;
	short isshort;
	int sesstype;
	unsigned char flag173d;
	short uinfo;
};

static const struct tail tail_v[] = {
	/* INFO1d: the one bit, set and cleared.  getUinfoValue is not called. */
	{ 0,	0,	0,	0,	5 },
	{ 0,	0,	9,	0,	5 },
	{ 0,	1,	0,	0,	5 },
	{ 0,	1,	9,	0,	5 },

	/* No Uinfo: the modem has none, or the lookup is short-circuited. */
	{ 1,	0,	1,	0,	0 },
	{ 1,	0,	1,	1,	5 },
	{ 1,	1,	1,	1,	5 },

	/* V.34 upstream, baud index recovered from the buffer. */
	{ 1,	0,	0,	0,	5 },
	{ 1,	0,	0,	0,	0x7f },
	{ 1,	0,	0,	0,	-1 },

	/*
	 * Short phase 2.  `pcmSessionType` is forced to 0 inside, so these
	 * land in the V.34 branch with the baud index 4 whatever they ask
	 * for -- which is the point of asking for both.
	 */
	{ 1,	1,	0,	0,	5 },
	{ 1,	1,	9,	0,	0x33 },
	{ 1,	0x100,	9,	0,	0x7f },

	/* PCM upstream: needs a non-zero session type and no short phase 2. */
	{ 1,	0,	1,	0,	5 },
	{ 1,	0,	9,	0,	0x7f },
	{ 1,	0,	9,	0,	-1 },
	{ 1,	0,	1,	0,	0x40 }
};
#define NTAIL	((int)(sizeof(tail_v) / sizeof(tail_v[0])))

/*
 * The PCM law.  One branch tests `!= 0` and the other `== 1`; both read 32
 * bits.  0x10000 is here so a 16-bit read of it would show.
 */
static const int law_v[] = { 0, 1, 2, 0x10000 };
#define NLAW	((int)(sizeof(law_v) / sizeof(law_v[0])))

/* Seven bits are sent, out of a signed short. */
static const short f35a4_v[] = { 0, 0x7f, 0x80, 0x1234, -1, 0x55 };
#define NF35A4	((int)(sizeof(f35a4_v) / sizeof(f35a4_v[0])))

/*
 * The three two-bit flag fields, WITH BITS ABOVE 7 SET.  The object reads
 * each with `testb`, so nothing above bit 7 may reach the message; these
 * values are what turns that from an assumption into a tested claim.
 */
static const short flag_v[] = { 0, 1, 2, 3, 0x0301, 0x7f02, -1 };
#define NFLAG	((int)(sizeof(flag_v) / sizeof(flag_v[0])))

int
main(void)
{
	int lvl, s, t, l, f;
	int trial = 0;
	int saw_blockx = 0, saw_blockc = 0, saw_blockd = 0;
	int saw_info1d_set = 0, saw_info1d_clear = 0, saw_info1d_high = 0;
	int saw_shortphase2 = 0, saw_nouinfo = 0;
	int saw_pcmup = 0, saw_v34up = 0, saw_baud4 = 0;
	int saw_flag_high = 0;
	unsigned baud_seen = 0;
	int law_seen[NLAW];

	diff_begin("v34 info1a: V34SetINFO1aBits");
	dsplib_debug_capture_on = 1;

	for (l = 0; l < NLAW; l++)
		law_seen[l] = 0;

	for (lvl = 0; lvl <= 2; lvl += 2) {
		set_level((unsigned int)lvl);

		for (s = 0; s < NSHAPE; s++)
		for (t = 0; t < NTAIL; t++)
		for (l = 0; l < NLAW; l++)
		for (f = 0; f < NF35A4; f++) {
			const struct shape *sh = &shape_v[s];
			const struct tail *ta = &tail_v[t];
			long tag;
			int side;
			int r0, r1;
			int ran_tail;
			short baud_in;

			trial++;
			tag = (long)lvl * 1000000 + s * 100000 + t * 1000
			    + l * 10 + f;

			setup(trial);

			for (side = 0; side < 2; side++) {
				struct v34_object *o = &vb[side].o;

				o->v90_receiver = sh->v90;
				o->k56flex_receiver = sh->k56;
				o->role = sh->role;
				o->short_35a4 = f35a4_v[f];
				o->is_short = ta->isshort;
				o->short_abce = flag_v[trial % NFLAG];
				o->short_abd0 = flag_v[(trial + 2) % NFLAG];
				o->short_abd2 = flag_v[(trial + 5) % NFLAG];

				*(int *)(pcm[side] + PCM_LAW) = law_v[l];

				M(side)->info0Layout = ta->layout;
				M(side)->pcmSessionType = ta->sesstype;
				M(side)->droppedToV34 = ta->flag173d;
				*(short *)(u49[side] + U49_UINFO) = ta->uinfo;
			}

			/*
			 * What the buffer holds BEFORE the call is what the
			 * baud recovery reads, so it is recorded here.
			 */
			baud_in = (short)((((unsigned short)mb[0].o[2] & 3) * 2)
					  | (((unsigned short)mb[0].o[3] & 0x80)
					     >> 7));

			/*
			 * GATED ON THE ARM THAT USES THEM, not merely on the
			 * value being offered.  A counter that increments on
			 * every trial stays non-zero after the arm it names
			 * stops running, which is the shape of a check that
			 * cannot fail (findings F247, F262, F295) -- and these
			 * two exist precisely to say that an interesting
			 * value class REACHED an arm.
			 */
			if (sh->v90 != 0 && ta->layout == 0
			    && ta->sesstype == 0
			    && (unsigned short)mb[0].o[7] >= 0x100)
				saw_info1d_high++;
			if (sh->v90 != 0 && ta->layout != 0
			    && ta->isshort == 0 && ta->sesstype != 0
			    && ta->flag173d == 0 && ta->uinfo != 0
			    && ((unsigned short)flag_v[trial % NFLAG]
				& 0xff00u) != 0)
				saw_flag_high++;

			dsplib_debug_capture_reset();

			r0 = V34SetINFO1aBits(&vb[0].o, mb[0].o);
			r1 = ref_V34SetINFO1aBits(&vb[1].o, mb[1].o);

			compare_all("after V34SetINFO1aBits", tag);

			/*
			 * A CHECK THAT CANNOT FAIL, AND IS NAMED AS ONE.  The
			 * function returns 0 on every path in the object, and
			 * neither side has any other value to return; this
			 * compares two constants.  It is here so that a
			 * reconstruction returning something else would still
			 * be caught, not because it covers anything.
			 */
			diff_eq_int("return (%ld)", r0, r1, tag);

			/* --- which arms actually ran ------------------- */
			ran_tail = sh->v90 != 0;

			if (sh->v90 == 0 && sh->k56 == 0)
				saw_blockx++;
			if (sh->k56 != 0 && sh->role == 0x65)
				saw_blockc++;
			if (sh->k56 != 0 && sh->role != 0x65)
				saw_blockd++;
			if (sh->k56 != 0)
				law_seen[l]++;

			if (ran_tail && ta->layout == 0) {
				if (ta->sesstype != 0)
					saw_info1d_set++;
				else
					saw_info1d_clear++;
			}
			if (ran_tail && ta->layout != 0) {
				if (ta->isshort != 0)
					saw_shortphase2++;
				if (ta->flag173d != 0 || ta->uinfo == 0) {
					saw_nouinfo++;
				} else if (ta->isshort == 0
					   && ta->sesstype != 0) {
					saw_pcmup++;
				} else {
					saw_v34up++;
					if (ta->isshort != 0)
						saw_baud4++;
					else
						baud_seen |=
						    1u << (baud_in & 7);
				}
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	/*
	 * ANTI-VACUITY.  Each of these names a path or a value class the
	 * cross-product above is supposed to have reached; a table trimmed
	 * until one of them stops happening fails here rather than passing on
	 * a smaller test.
	 */
	diff_eq_int("the no-receiver arm ran (%ld)", saw_blockx > 0, 1, 0);
	diff_eq_int("the INFO1c (caller) arm ran (%ld)", saw_blockc > 0, 1, 0);
	diff_eq_int("the INFO1a (answer) arm ran (%ld)", saw_blockd > 0, 1, 0);
	diff_eq_int("the INFO1d bit was set (%ld)", saw_info1d_set > 0, 1, 0);
	diff_eq_int("the INFO1d bit was cleared (%ld)",
		    saw_info1d_clear > 0, 1, 0);
	diff_eq_int("a message short with a non-zero high byte reached the "
		    "INFO1d clear arm (%ld)", saw_info1d_high > 0, 1, 0);
	diff_eq_int("the short-phase-2 block ran (%ld)",
		    saw_shortphase2 > 0, 1, 0);
	diff_eq_int("the no-Uinfo return ran (%ld)", saw_nouinfo > 0, 1, 0);
	diff_eq_int("the PCM-upstream branch ran (%ld)", saw_pcmup > 0, 1, 0);
	diff_eq_int("the V.34-upstream branch ran (%ld)", saw_v34up > 0, 1, 0);
	diff_eq_int("the V.34-upstream branch ran with a short phase 2 (%ld)",
		    saw_baud4 > 0, 1, 0);
	diff_eq_int("at least four distinct baud indexes were recovered (%ld)",
		    __builtin_popcount(baud_seen) >= 4, 1, 0);
	diff_eq_int("a flag field with bits above 7 reached the PCM-upstream "
		    "branch (%ld)", saw_flag_high > 0, 1, 0);

	for (l = 0; l < NLAW; l++)
		diff_eq_int("every PCM law reached a K56Flex arm (%ld)",
			    law_seen[l] > 0, 1, l);

	return diff_end();
}
