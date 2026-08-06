/*
 * v34hsstep.c -- the per-dispatch-case fixture for `v34handshak`.
 *
 * See v34hsstep.h for what this is for and docs/v34handshak.md for how to use
 * it.  This file is the mechanics: two objects, the pointer fields each one
 * needs aimed somewhere real, the four guards that pick a dispatch, and a
 * step that records what the call wrote.
 *
 * NOTHING HERE RECONSTRUCTS ANY PART OF `v34handshak`.  The offsets below are
 * read off its prologue and off `v34handshakinit`'s callees; no case body is
 * modelled, and the fixture has no opinion about what a case should do.
 */

#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "harness.h"
#include "v34hsstep.h"

#include "dsplib/v34fsk.h"	/* struct v34_object */
#include "dsplib/v34hshak.h"	/* v34handshakinit */

extern unsigned int dsplibs_debug_level;
extern unsigned int ref_dsplibs_debug_level;

extern void ref_v34handshak(void *obj);
extern void ref_v34handshakinit(void *obj, int mode);
extern void ref_V34InitializeImplementationSpecific(void *obj);
extern void V34InitializeImplementationSpecific(void *obj);

/*
 * THE SWAP, AND WHY IT IS PER TEST RATHER THAN PER BUILD.
 *
 * This used to be `#ifdef V34HS_OURS`, on the reasoning that the first agent
 * to land a dispatch case would turn it on and every test written against
 * the fixture would become a tier-1 differential test at once.  It cannot
 * be a compile-time switch: the Makefile builds ONE v34hsstep.o and links it
 * into every test binary, so defining it would move side A for
 * `t_v34hsstep.c` too -- whose whole claim is a blob-against-blob property
 * over all forty-three cases, most of which have no reconstruction and would
 * halt in `t3c_unwritten`.
 *
 * So the choice is a run-time one a test makes for itself.  `v34hs_ours(1)`
 * puts the reconstruction on side A; the default is the blob, which leaves
 * the fixture proving ITSELF exactly as before.  `V34HS_OURS` still works
 * and now sets that default, so a build that wants the old behaviour has it.
 */
#ifdef V34HS_OURS
static int use_ours = 1;
#else
static int use_ours;
#endif

void
v34hs_ours(int on)
{
	use_ours = on;
}

#define V34HS_CALL_A(o)		(use_ours ? v34handshak(o) : ref_v34handshak(o))
#define V34HS_LOG_A		(use_ours ? 0 : 1)

/* --- the two sides -------------------------------------------------------- */

#define OBJ_SIZE	((unsigned)sizeof(struct v34_object))

/*
 * The blocks the object points OUT of, per side.
 *
 * `V34SetupModulator` writes through the shaping buffer, and
 * `GetVPcmMinimalTxPowerReduction` -- which the handshake reaches through
 * `settxlevel` -- walks the session at +0x3548 to a PCM receiver and reads
 * the configuration at +0xac3c.  Sizes are t_v34hshak.c's, which derived them
 * from the highest offset each is indexed at.
 */
#define SHAPED_LEN	4096
#define SESS_LEN	0x6200
#define PCM_LEN		0x0520
#define CFG_LEN		0x0080
#define SESS_PCM	0x610c
#define DUMMY_LEN	8192

/*
 * ---------------------------------------------------------------------------
 * ONE ARENA PER SIDE, AND WHY THE TWO SIDES CANNOT JUST BE SEPARATE STATICS.
 *
 * The object and every block it points at live inside a single 64 KB-aligned
 * arena, at fixed offsets, with 32 KB of padding between and around them; the
 * whole of side B's arena is a byte copy of side A's.  So
 *
 *   - side B's arena is side A's arena plus a constant that is a multiple of
 *     64 KB, which makes every INTER-BLOCK DISTANCE equal on the two sides
 *     and every address's low sixteen bits equal too;
 *   - a read that runs off the end of any block, or off the front of one,
 *     lands on identical bytes rather than on whatever the linker happened to
 *     put next to that side's static.
 *
 * That is findings 319 and 321.  Before it, `obj_a` was a `struct v34_object`
 * and `obj_b` an `unsigned char[]` at two addresses the linker chose, with two
 * unrelated sets of neighbours, and the per-sample transmit loop gave two
 * different answers from two identical objects -- which D60 recorded as a
 * property of the object and which was a property of this file.
 *
 * WHAT IS *NOT* CLAIMED: nobody has caught the loop reading a particular byte
 * outside the object.  V34HS_PADVARY makes each padding region differ between
 * the sides and the sweep still agrees, and the step writes no padding byte at
 * all, so whatever the old layout fed it is not within 32 KB of any block.
 * Finding 322 has the bounds.
 *
 * The arena is also why `v34hs_compare` can now check WHICH block a pointer
 * out of the object selects: an offset within one's own arena is comparable
 * where a raw address is not.
 */
#define ARENA_PAD	0x8000

struct v34hs_arena {
	unsigned char	head[ARENA_PAD];
	struct v34_object obj;
	unsigned char	gap1[ARENA_PAD];
	short		shaped[SHAPED_LEN];
	unsigned char	gap2[ARENA_PAD];
	unsigned char	sess[SESS_LEN];
	unsigned char	gap3[ARENA_PAD];
	unsigned char	pcm[PCM_LEN];
	unsigned char	gap4[ARENA_PAD];
	unsigned char	cfg[CFG_LEN];
	unsigned char	gap5[ARENA_PAD];
	short		dummy[DUMMY_LEN];
	unsigned char	tail[ARENA_PAD];
};

#define ARENA_SIZE	((unsigned)sizeof(struct v34hs_arena))
#define ARENA_STRIDE	((ARENA_SIZE + 0xffffu) & ~0xffffu)

/*
 * BOTH ARENAS COME OUT OF ONE 64 KB-ALIGNED BUFFER, at a stride this fixture
 * chooses rather than one the linker chooses.  `V34HS_SKEW=n` moves side B by
 * n bytes, which is how the claim "the step does not depend on where the
 * object is" is demonstrated instead of asserted: the sweep is run again at a
 * skew that destroys the alignment agreement and has to give the same answer.
 * Finding 322 is what that measured.
 */
static unsigned char arena_mem[2 * ARENA_STRIDE + 0x10000]
	__attribute__((aligned(0x10000)));
static struct v34hs_arena *pa_arena, *pb_arena;

#define arena_a		(*pa_arena)
#define arena_b		(*pb_arena)

/*
 * The object is reached through a pointer rather than as `arena.obj` directly
 * so that `V34HS_OBJSKEW=n` can move side B's object n bytes further into its
 * own arena.  That breaks the OBJECT-TO-BLOCK DISTANCES and nothing else --
 * same contents, same neighbours, same alignment class -- which is the one
 * property the arena supplies that the skew and padding sweeps do not test.
 */
static unsigned char *obj_ptr[2];
static unsigned objskew, obj_seed;

/*
 * THE POSITIVE CONTROL.  `V34HS_LOOSEOBJ=1` puts side B's object HERE instead
 * of in its arena: a standalone static at an address the linker chose, with
 * the linker's neighbours and an unrelated distance to the five blocks it
 * points at.  That is the one asymmetry of the pre-arena fixture that
 * V34HS_SKEW, V34HS_OBJSKEW and V34HS_PADVARY between them cannot express, and
 * a claim that the arena's congruence is what fixed table 1 is worth nothing
 * without it.  Finding 319 reports what it did.
 */
static struct v34_object loose_obj;
static int looseobj;

#define obj_a		(*(struct v34_object *)obj_ptr[0])
#define obj_b		(obj_ptr[1])
#define OBJ_IN_ARENA	((unsigned)offsetof(struct v34hs_arena, obj))

#define shaped_a	(arena_a.shaped)
#define shaped_b	(arena_b.shaped)
#define sess_a		(arena_a.sess)
#define sess_b		(arena_b.sess)
#define pcm_a		(arena_a.pcm)
#define pcm_b		(arena_b.pcm)
#define cfg_a		(arena_a.cfg)
#define cfg_b		(arena_b.cfg)
#define dummy_a		(arena_a.dummy)
#define dummy_b		(arena_b.dummy)

static unsigned char snap_a[sizeof(struct v34_object)];
static unsigned char snap_b[sizeof(struct v34_object)];

/*
 * The arena's regions, so a differing byte can be named rather than reported
 * as a bare offset.  `pad` marks the seven filler regions, which V34HS_PADVARY
 * deliberately makes differ; the object itself is compared byte for byte by
 * `v34hs_compare` and so is skipped here.
 */
#define AR_OFF(f)	((unsigned)offsetof(struct v34hs_arena, f))
static const struct {
	unsigned	off, len;
	int		pad;
	const char     *name;
} regions[] = {
	{ AR_OFF(head), ARENA_PAD, 1, "padding before the object" },
	{ AR_OFF(obj), 0, 0, "the object" },
	{ AR_OFF(gap1), ARENA_PAD, 1, "padding after the object" },
	{ AR_OFF(shaped), SHAPED_LEN * 2, 0, "the shaping buffer" },
	{ AR_OFF(gap2), ARENA_PAD, 1, "padding after the shaping buffer" },
	{ AR_OFF(sess), SESS_LEN, 0, "the session block" },
	{ AR_OFF(gap3), ARENA_PAD, 1, "padding after the session block" },
	{ AR_OFF(pcm), PCM_LEN, 0, "the PCM receiver block" },
	{ AR_OFF(gap4), ARENA_PAD, 1, "padding after the PCM block" },
	{ AR_OFF(cfg), CFG_LEN, 0, "the configuration block" },
	{ AR_OFF(gap5), ARENA_PAD, 1, "padding after the configuration" },
	{ AR_OFF(dummy), DUMMY_LEN * 2, 0, "the seed tables" },
	{ AR_OFF(tail), ARENA_PAD, 1, "padding after the seed tables" }
};
#define NREGIONS	((unsigned)(sizeof(regions) / sizeof(regions[0])))

static int pad_varied, ref_both, noscrub, force_refinit;

static const char *
region_of(unsigned off)
{
	unsigned k;

	for (k = 0; k < NREGIONS; k++)
		if (off >= regions[k].off && off < regions[k].off
					     + (k == 1 ? OBJ_SIZE
						       : regions[k].len))
			return regions[k].name;
	return "outside every region";
}

static int
in_padding(unsigned off)
{
	unsigned k;

	for (k = 0; k < NREGIONS; k++)
		if (regions[k].pad && off >= regions[k].off
		    && off < regions[k].off + regions[k].len)
			return 1;
	return 0;
}

/*
 * Every pointer-sized field this fixture's bring-up leaves holding an
 * address.  Two objects at two addresses hold two different values in each,
 * necessarily and forever, so these are excluded from the byte comparison and
 * checked by what they select instead.
 *
 * The list is t_v34hshak.c's, which derived it from the code rather than from
 * watching a test fail, plus the four this fixture owns.  `saw_hole` asserts
 * every entry was reached, so it cannot quietly go stale.
 */
static const unsigned holes[] = {
	0x0394,		/* receiver +0x130 rx_samples  -- rxinit, interior  */
	0x0418,		/* receiver +0x1b4 carrier     -- setupreceiver     */
	0x0508,		/* receiver +0x2a4 f2a4        -- dpskinit          */
	0x0620, 0x0624,	/* timing +0x114, +0x118      -- mode 0 only       */
	0x1460,		/* modulator +0x10 sine        -- V34SetupModulator */
	0x2074,		/* modulator +0xc24 shaped     -- seeded here       */
	0x20cc,		/* modulator +0xc7c ec_prem                         */
	0x2100,		/* modulator +0xcb0 preemp                          */
	0x3564,		/* detector +0x00 coeff        -- detectorinit      */
	0xaa90,		/* tx power scale              -- setfinalrate      */
	0xaaac,		/* rx power scale                                   */
	0xaab0,		/* rx carrier descriptor                            */
	0x0268, 0x026c,	/* receive sample queue cursors                     */
	0x2220, 0x2224,	/* transmit sample queue cursors                    */
	0x80b8, 0x80bc, 0x80c0, 0x80c4, 0x80c8,	/* echo canceller 0        */
	0x9138, 0x913c, 0x9140, 0x9144, 0x9148,	/* and 1                   */
	0x0a28, 0x0e48,	/* receive shell context                            */
	0x2608, 0x2a28,	/* transmit shell context                           */
	0xaa6c, 0xaa70,	/* the two SELF-pointers -- checked by offset       */
	0x3548, 0xac3c	/* the session and the configuration -- ours        */
};
#define NHOLES	((unsigned)(sizeof(holes) / sizeof(holes[0])))

/*
 * WAS EACH SKIP ENTRY ACTUALLY EXERCISED?  Accumulated across the whole run
 * and asserted once, never per case: a per-case reset would only ever report
 * the last one, which is a check that reads as a check and is not.  An entry
 * that never differs is an entry the two sides agree on, and a hole nothing
 * needs is a hole that has gone stale -- which is the failure this array
 * exists to catch and did not, until it was read as well as written.
 */
static int saw_hole[NHOLES];

static int
in_hole(unsigned off)
{
	unsigned k;

	for (k = 0; k < NHOLES; k++)
		if (off >= holes[k] && off < holes[k] + 4)
			return 1;
	return 0;
}

/* --- pokes and peeks ------------------------------------------------------ */

void *
v34hs_object(int side)
{
	return side ? (void *)obj_b : (void *)&obj_a;
}

static unsigned char *
base(int side)
{
	return side ? obj_b : (unsigned char *)&obj_a;
}

void
v34hs_poke_short(unsigned off, short v)
{
	memcpy(base(0) + off, &v, sizeof(v));
	memcpy(base(1) + off, &v, sizeof(v));
}

void
v34hs_poke_int(unsigned off, int v)
{
	memcpy(base(0) + off, &v, sizeof(v));
	memcpy(base(1) + off, &v, sizeof(v));
}

void
v34hs_poke_byte(unsigned off, unsigned char v)
{
	base(0)[off] = v;
	base(1)[off] = v;
}

/*
 * Aim a pointer field at each side's OWN object, at a chosen offset.
 *
 * `v34hs_poke_int` cannot do this and must not be used to try: the two
 * objects are at different addresses, so writing one address into both is
 * precisely the asymmetry findings 319-322 are about, and the failure it
 * produces looks like a defect in whatever ran next.
 */
void
v34hs_poke_self_ptr(unsigned off, unsigned target)
{
	void *pa = base(0) + target;
	void *pb = base(1) + target;

	memcpy(base(0) + off, &pa, sizeof(pa));
	memcpy(base(1) + off, &pb, sizeof(pb));
}

short
v34hs_peek_short(int side, unsigned off)
{
	short v;

	memcpy(&v, base(side) + off, sizeof(v));
	return v;
}

static void
poke_ptr(unsigned off, void *pa, void *pb)
{
	memcpy(base(0) + off, &pa, sizeof(pa));
	memcpy(base(1) + off, &pb, sizeof(pb));
}

static void *
peek_ptr(int side, unsigned off)
{
	void *p;

	memcpy(&p, base(side) + off, sizeof(p));
	return p;
}

/* --- construction --------------------------------------------------------- */

/*
 * VARIED BYTES, NEVER ZERO, AND THE SAME ON BOTH SIDES.
 *
 * A constant fill makes a 16-bit field and the two 8-bit fields beside it
 * indistinguishable, and zero additionally makes "nothing wrote this" look
 * like a deliberate value.  The generator is an LCG rather than `rand` so the
 * fill is the same on every host and in every build: a differential test that
 * drifts between runs cannot be bisected.  Finding 230.
 */
static void
fill(unsigned char *p, unsigned n, unsigned seed)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		seed = seed * 1103515245u + 12345u;
		p[i] = (unsigned char)(seed >> 16);
	}
}

void
v34hs_setup(int mode)
{
	unsigned i;

	if (pa_arena == NULL) {
		const char *s = getenv("V34HS_SKEW");
		unsigned long skew = s ? strtoul(s, NULL, 0) : 0;

		skew &= 0xfffcu;	/* the object wants four-byte alignment */
		pa_arena = (struct v34hs_arena *)arena_mem;
		pb_arena = (struct v34hs_arena *)(arena_mem + ARENA_STRIDE
						  + skew);

		s = getenv("V34HS_OBJSKEW");
		objskew = (unsigned)(s ? strtoul(s, NULL, 0) : 0) & 0x7ffcu;
		looseobj = getenv("V34HS_LOOSEOBJ") != NULL;
		obj_ptr[0] = (unsigned char *)&pa_arena->obj;
		obj_ptr[1] = looseobj
			     ? (unsigned char *)&loose_obj
			     : (unsigned char *)&pb_arena->obj + objskew;

		/*
		 * A different object, for the sweep that says the agreement is
		 * a property of the object rather than of one lucky fill.  The
		 * committed run leaves it at zero, because the separation
		 * counts the test asserts are counts for THIS fill.
		 */
		s = getenv("V34HS_SEED");
		obj_seed = (unsigned)(s ? strtoul(s, NULL, 0) : 0) * 2654435761u;
	}

	/*
	 * THE PADDING IS FILLED TOO, and side B's arena is a byte copy of
	 * side A's whole arena rather than a block-by-block copy.  A read that
	 * runs off the end of a block has to find the same bytes on both
	 * sides, or the fixture measures the linker rather than the object.
	 */
	fill((unsigned char *)&arena_a, ARENA_SIZE, 0x7ad10000u);
	fill((unsigned char *)&obj_a, OBJ_SIZE, 0x5eed1234u + obj_seed);
	fill((unsigned char *)shaped_a, sizeof(shaped_a), 0x11117777u);
	fill(sess_a, sizeof(sess_a), 0x22228888u);
	fill(pcm_a, sizeof(pcm_a), 0x33339999u);
	fill(cfg_a, sizeof(cfg_a), 0x4444aaaau);

	for (i = 0; i < DUMMY_LEN; i++)
		dummy_a[i] = (short)(0x4b00 + i);

	/*
	 * The configuration byte `GetVPcmMinimalTxPowerReduction` reads as a
	 * power reduction; left at the fill it is an absurd reduction.
	 */
	memset(cfg_a + 0x40, 0, 0x20);

	memcpy(&arena_b, &arena_a, ARENA_SIZE);

	if (objskew || looseobj) {
		memcpy(obj_b, (const unsigned char *)&obj_a, OBJ_SIZE);
		pad_varied = 1;	/* B's object is no longer where the arena has it */
	}

	if (getenv("V34HS_PADVARY")) {
		/*
		 * THE EXPERIMENT THAT NAMES THE CAUSE.  Re-fill one of the
		 * arena's seven padding regions on side B only, so the two
		 * sides differ in nothing except what lies OUTSIDE the blocks
		 * the object points at.  A step that then disagrees is a step
		 * that read past the end of one of them.  Finding 321.
		 */
		static struct { void *p; unsigned n; const char *name; } pad[7];
		int which = atoi(getenv("V34HS_PADVARY"));
		unsigned k;

		pad[0].p = arena_b.head; pad[0].name = "before the object";
		pad[1].p = arena_b.gap1; pad[1].name = "after the object";
		pad[2].p = arena_b.gap2; pad[2].name = "after shaped";
		pad[3].p = arena_b.gap3; pad[3].name = "after the session";
		pad[4].p = arena_b.gap4; pad[4].name = "after the PCM block";
		pad[5].p = arena_b.gap5; pad[5].name = "after the config";
		pad[6].p = arena_b.tail; pad[6].name = "after the seed tables";
		pad_varied = 1;
		for (k = 0; k < 7; k++) {
			pad[k].n = ARENA_PAD;
			if (which == 0 || which == (int)k + 1)
				fill(pad[k].p, pad[k].n, 0xdead0000u + k);
		}
	}

	/*
	 * The session's one pointer field, which is the only byte of the arena
	 * that legitimately differs between the sides.  `v34hs_compare` skips
	 * exactly these four bytes and nothing else.
	 */
	memcpy(sess_a + SESS_PCM, &(void *){ pcm_a }, sizeof(void *));
	memcpy(sess_b + SESS_PCM, &(void *){ pcm_b }, sizeof(void *));

	/*
	 * EVERY POINTER FIELD IS AIMED BEFORE ANY CODE RUNS.  The fill puts a
	 * pseudorandom address in each, and the bring-up below dereferences
	 * several of them; a fixture that let one through would fault rather
	 * than fail, and a fault has no offset in it.
	 */
	poke_ptr(0x2074, shaped_a, shaped_b);
	poke_ptr(0x3548, sess_a, sess_b);
	poke_ptr(0xac3c, cfg_a, cfg_b);
	for (i = 0; i < NHOLES; i++)
		if (holes[i] != 0x2074 && holes[i] != 0x3548
		    && holes[i] != 0xac3c)
			poke_ptr(holes[i], dummy_a, dummy_b);

	/*
	 * AND THE THREE STATE WORDS ARE PUT IN RANGE BEFORE ANYTHING TRACES.
	 *
	 * `StateName` is indexed with no bound (D42), and `v34handshakinit`
	 * announces each transition by printing the state it is LEAVING -- so
	 * with the diagnostics on, a pseudorandom halfword at +0x3592 is a
	 * wild `char *` handed to `vsnprintf`, and the fixture faults inside
	 * its own bring-up.  Found exactly that way.
	 */
	v34hs_state(0, 0, 0);

	/*
	 * `v34handshakinit` reaches `txinit`, which cleans both echo
	 * cancellers through five pointers each, and aiming those is
	 * `V34InitializeImplementationSpecific`'s job rather than its own.
	 * That is what a real caller does, not something the fixture invents:
	 * `VPcmV34Create` calls it before the handshake comes up.
	 *
	 * Side A runs OUR copy and side B the blob's, so the bring-up is
	 * itself differential -- which is what makes an agreeing step evidence
	 * rather than a tautology.
	 */
	ref_both = force_refinit || getenv("V34HS_REFINIT") != NULL;
	noscrub = getenv("V34HS_NOSCRUB") != NULL;
	if (ref_both) {
		ref_V34InitializeImplementationSpecific(&obj_a);
		ref_v34handshakinit(&obj_a, mode);
	} else {
		V34InitializeImplementationSpecific(&obj_a);
		v34handshakinit(&obj_a, mode);
	}
	ref_V34InitializeImplementationSpecific(obj_b);
	ref_v34handshakinit(obj_b, mode);

	if (getenv("V34HS_EQPTR")) {
		/*
		 * Force every hole that points into the PROGRAM IMAGE -- neither
		 * into the object nor into one of this fixture's blocks -- to
		 * hold side B's value on both sides.  Those are the library
		 * tables and functions the bring-up installs, and the fixture's
		 * comparison skips them entirely.
		 */
		static const void *ba[6];
		static const unsigned bl[6] = { sizeof(shaped_a), sizeof(sess_a),
						sizeof(pcm_a), sizeof(cfg_a),
						sizeof(dummy_a), OBJ_SIZE };
		unsigned k, j;

		ba[0] = shaped_a; ba[1] = sess_a; ba[2] = pcm_a;
		ba[3] = cfg_a; ba[4] = dummy_a; ba[5] = &obj_a;
		for (k = 0; k < NHOLES; k++) {
			const char *pa = peek_ptr(0, holes[k]);
			int owned = 0;

			for (j = 0; j < 6; j++)
				if (pa >= (const char *)ba[j]
				    && pa < (const char *)ba[j] + bl[j])
					owned = 1;
			if (!owned) {
				void *pb = peek_ptr(1, holes[k]);

				memcpy(base(0) + holes[k], &pb, sizeof(pb));
				if (getenv("V34HS_PROBE"))
					printf("  PROBE eqptr +0x%04x -> %p\n",
					       holes[k], pb);
			}
		}
	}

	if (getenv("V34HS_PROBE")) {
		static int once;
		unsigned nd = 0, first = ~0u;

		for (i = 0; i < OBJ_SIZE; i++)
			if (((const unsigned char *)&obj_a)[i] != obj_b[i]
			    && !in_hole(i)) {
				if (nd == 0)
					first = i;
				nd++;
			}
		printf("  PROBE after setup: %u object bytes differ", nd);
		if (nd)
			printf(", first +0x%04x", first);
		nd = 0;
		for (i = 0; i < ARENA_SIZE; i++)
			if (in_padding(i)
			    && ((const unsigned char *)&arena_a)[i]
			       != ((const unsigned char *)&arena_b)[i])
				nd++;
		printf("; %u padding bytes differ\n", nd);
		if (!once) {
			once = 1;
			printf("  PROBE obj_a=%p obj_b=%p  dummy_a=%p "
			       "dummy_b=%p\n", (void *)&obj_a, (void *)obj_b,
			       (void *)dummy_a, (void *)dummy_b);
			for (i = 0; i < NHOLES; i++) {
				long da = (char *)peek_ptr(0, holes[i])
					  - (char *)&obj_a;
				long db = (char *)peek_ptr(1, holes[i])
					  - (char *)obj_b;

				printf("  PROBE hole +0x%04x: A%+ld B%+ld%s%s\n",
				       holes[i], da, db,
				       da == db ? "" : "   <== DELTA DIFFERS",
				       (da >= 0 && da < (long)OBJ_SIZE)
				       ? "  (inside)" : "");
			}
		}
	}

	if (getenv("V34HS_DIAG")) {
		static const unsigned tab[] = { 0x0418, 0x0508, 0x0620,
						0x0624, 0x1460, 0x20cc,
						0x2100, 0x3564, 0xaa90,
						0xaaac, 0xaab0 };
		unsigned k, n;

		for (k = 0; k < sizeof(tab) / sizeof(tab[0]); k++) {
			const unsigned char *pa = peek_ptr(0, tab[k]);
			const unsigned char *pb = peek_ptr(1, tab[k]);
			int first = -1;

			if (pa == (const unsigned char *)dummy_a)
				continue;
			for (n = 0; n < 4096; n++)
				if (pa[n] != pb[n]) { first = (int)n; break; }
			printf("  DIAG table +0x%04x: %s at %d\n", tab[k],
			       first < 0 ? "equal to 4096" : "DIFFERS", first);
		}
	}
}

void
v34hs_route(enum v34hs_route r, short samples)
{
	switch (r) {
	case V34HS_ROUTE_TXSAMPLE:
		/* cursor < limit: the per-sample loop, `samples` passes. */
		v34hs_poke_short(V34HS_TXLIMIT, samples);
		v34hs_poke_short(V34HS_TXCURSOR, 0);
		break;
	case V34HS_ROUTE_TXBLOCK:
		/* cursor >= limit, then receiver +0x00 <= 5. */
		v34hs_poke_short(V34HS_TXLIMIT, 0);
		v34hs_poke_short(V34HS_TXCURSOR, 0);
		v34hs_poke_short(V34HS_RXCOUNT, 5);
		break;
	case V34HS_ROUTE_RXCHAIN:
		/* cursor >= limit, receiver +0x00 > 5, and the gate at
		 * +0xa8a0 clear so rxstate 43 reaches `fskdemodulate` and
		 * the microstate table rather than diverting at 0x64a87. */
		v34hs_poke_short(V34HS_TXLIMIT, 0);
		v34hs_poke_short(V34HS_TXCURSOR, 0);
		v34hs_poke_short(V34HS_RXCOUNT, 6);
		v34hs_poke_int(V34HS_FSKGATE, 0);
		break;
	}
}

void
v34hs_state(short mst, short rxst, short txst)
{
	v34hs_poke_short(V34HS_MICROSTATE, mst);
	v34hs_poke_short(V34HS_RXSTATE, rxst);
	v34hs_poke_short(V34HS_TXSTATE, txst);
}

/*
 * Bring BOTH sides up with the blob's initialisers, which is what makes the
 * third class of `check_self_ptr` checkable: with ours on side A and the
 * blob's on side B the two hold two addresses of two copies of one library
 * table and no address comparison can say anything, but with the blob on both
 * they must select the IDENTICAL address.  `t_v34hsstep.c` runs the whole
 * sweep a second time this way so that check is in `make phase` rather than
 * behind an environment variable nothing sets.  Finding 324.
 */
void
v34hs_refinit(int on)
{
	force_refinit = on;
}

void
v34hs_debug(int on)
{
	dsplib_debug_capture_on = on;
	dsplibs_debug_level = on ? 2u : 0u;
	ref_dsplibs_debug_level = on ? 2u : 0u;
}

/* --- the step ------------------------------------------------------------- */

static struct v34hs_obs obs[2];
static char text[2][4096];

/*
 * THE LOOP AT 0x629e0 DOES NOT ALWAYS TERMINATE, so the step is guarded.
 *
 * Table 1's default arm is the loop bottom itself: an unhandled txstate
 * re-tests the cursor against the limit, finds it unchanged, and jumps back to
 * the dispatch.  Without this the failure is a test run that never returns,
 * which `make phase` reports as nothing at all.  Finding 287.
 */
static volatile int step_side;
static short step_mst, step_rxst, step_txst;

static void
step_alarm(int sig)
{
	char msg[160];
	int n;

	(void)sig;
	n = snprintf(msg, sizeof(msg),
		     "\nv34hs_step: side %d did not return with "
		     "mst=%d rxstate=%d txstate=%d -- see finding 287\n",
		     step_side, step_mst, step_rxst, step_txst);
	/* Nothing useful to do about a failed write from a signal handler;
	 * the exit status carries the failure either way. */
	if (n > 0)
		(void)!write(2, msg, (size_t)n);
	_exit(3);
}

/*
 * SCRUB THE STACK BEFORE EACH SIDE'S CALL.
 *
 * This was added because the per-sample transmit route disagreed and the
 * reasoning was that side A runs first and leaves residue side B then reads.
 * THAT REASONING WAS WRONG -- finding 320's probe runs side B a second time
 * at the same address, one place further along the sequence, with everything
 * the first two calls left in the machine still there, and gets a
 * byte-identical answer on all forty-three cases.  Nothing is carried.
 *
 * It stays for two reasons and neither is the one it was written for.  It
 * costs a memset, and it removes a variable from a fixture whose whole job is
 * to have none: `V34HS_NOSCRUB=1` turns it off and the sweep still passes, so
 * that is measured rather than assumed.
 */
static volatile unsigned scrub_sink;

/*
 * V34HS_PROBE -- the experiment that separates "address" from "carried state".
 *
 * D60 said the per-sample loop's result was not a function of the object, and
 * finding 289's five exclusions all sat in the address/memory family.  What
 * none of them touched is that side A ALWAYS runs before side B, so anything
 * the first call leaves in the machine is read by the second.
 *
 * The probe holds the address constant and varies only the position in the
 * call sequence: B, A, B, with B's object and every block it points at
 * restored from the snapshot in between.  If the two B runs differ, the
 * address is irrelevant and the cause is state carried across calls.  They do
 * not differ, and the FPU status word is zero before all three calls, so it
 * is neither the x87 nor a blob global.  Finding 320.
 *
 * It also reports whether the two sides were equal after `v34hs_setup` -- the
 * fixture never used to check its own starting state -- and whether the step
 * wrote any byte outside the blocks it models, which is how finding 322
 * excludes an out-of-bounds write.
 */
static unsigned char probe_b2[sizeof(struct v34_object)];
static short probe_shaped[SHAPED_LEN];
static unsigned char probe_sess[SESS_LEN], probe_pcm[PCM_LEN];
static unsigned char probe_cfg[CFG_LEN];
static short probe_dummy[DUMMY_LEN];
static unsigned short probe_sw[3], probe_cw[3];
static unsigned char probe_pad[sizeof(struct v34hs_arena)];

static unsigned short
fpu_status(void)
{
	unsigned short w;

	__asm__ __volatile__("fnstsw %0" : "=a"(w));
	return w;
}

static unsigned short
fpu_control(void)
{
	unsigned short w;

	__asm__ __volatile__("fnstcw %0" : "=m"(w));
	return w;
}

static void
scrub_stack(void)
{
	unsigned char pad[65536];

	memset(pad, 0x5a, sizeof(pad));
	scrub_sink = (unsigned)pad[0] + pad[sizeof(pad) - 1];
}

static void
observe(int side, const unsigned char *now, const unsigned char *was,
	unsigned lines)
{
	struct v34hs_obs *o = &obs[side];
	unsigned i;

	o->changed = 0;
	o->first = ~0u;
	o->last = 0;
	o->hash = 2166136261u;
	o->lines = lines;

	for (i = 0; i < OBJ_SIZE; i++) {
		/*
		 * THE POINTER FIELDS ARE OUT OF THE SIGNATURE ENTIRELY, value
		 * AND fact-of-change.
		 *
		 * The first version hashed a marker for a changed pointer, on
		 * the reasoning that a step which only re-aimed one would
		 * otherwise signature as having done nothing.  That is
		 * address-dependent and therefore wrong: writing a pointer
		 * over a previous value changes however many BYTES the two
		 * addresses happen to differ in, which is two on one side and
		 * three on the other.  Every microstate case then differed by
		 * exactly one byte -- but only in the instrumented build,
		 * where the addresses move.  `make debugcov` caught it; the
		 * ordinary build never would have.
		 *
		 * What replaces it is `check_self_ptr` over all thirty-five
		 * holes, which compares OFFSETS and is address-independent by
		 * construction.
		 */
		if (now[i] == was[i] || in_hole(i))
			continue;
		o->changed++;
		if (o->first == ~0u)
			o->first = i;
		o->last = i;
		o->hash = (o->hash ^ i) * 16777619u;
		o->hash = (o->hash ^ now[i]) * 16777619u;
	}

	o->mst = v34hs_peek_short(side, V34HS_MICROSTATE);
	o->rxst = v34hs_peek_short(side, V34HS_RXSTATE);
	o->txst = v34hs_peek_short(side, V34HS_TXSTATE);
	memcpy(&o->progress, base(side) + V34HS_PROGRESS, sizeof(o->progress));
}

void
v34hs_step(void)
{
	void (*prev)(int);

	step_mst = v34hs_peek_short(0, V34HS_MICROSTATE);
	step_rxst = v34hs_peek_short(0, V34HS_RXSTATE);
	step_txst = v34hs_peek_short(0, V34HS_TXSTATE);

	memcpy(snap_a, &obj_a, OBJ_SIZE);
	memcpy(snap_b, obj_b, OBJ_SIZE);
	if (getenv("V34HS_PROBE")) {
		memcpy(probe_pad, &arena_a, ARENA_SIZE);
		memcpy(probe_shaped, shaped_b, sizeof(shaped_b));
		memcpy(probe_sess, sess_b, sizeof(sess_b));
		memcpy(probe_pcm, pcm_b, sizeof(pcm_b));
		memcpy(probe_cfg, cfg_b, sizeof(cfg_b));
		memcpy(probe_dummy, dummy_b, sizeof(dummy_b));
	}

	prev = signal(SIGALRM, step_alarm);

	step_side = 0;
	dsplib_debug_capture_reset();
	if (!noscrub)
		scrub_stack();
	probe_sw[0] = fpu_status();
	probe_cw[0] = fpu_control();
	alarm(5);
	V34HS_CALL_A(&obj_a);
	alarm(0);
	snprintf(text[0], sizeof(text[0]), "%s",
		 dsplib_debug_capture_text(V34HS_LOG_A));
	observe(0, (const unsigned char *)&obj_a, snap_a,
		dsplib_debug_capture_lines(V34HS_LOG_A));

	step_side = 1;
	dsplib_debug_capture_reset();
	if (!noscrub)
		scrub_stack();
	probe_sw[1] = fpu_status();
	probe_cw[1] = fpu_control();
	alarm(5);
	ref_v34handshak(obj_b);
	alarm(0);
	snprintf(text[1], sizeof(text[1]), "%s", dsplib_debug_capture_text(1));
	observe(1, obj_b, snap_b, dsplib_debug_capture_lines(1));

	if (getenv("V34HS_PROBE")) {
		unsigned i, nd = 0, first = ~0u;

		for (i = 0; i < ARENA_SIZE; i++)
			if (in_padding(i)
			    && ((const unsigned char *)&arena_a)[i]
			       != probe_pad[i]) {
				if (nd == 0)
					first = i;
				nd++;
			}
		printf("  PROBE step wrote %u padding bytes", nd);
		if (nd)
			printf(", first +0x%x (%s)", first, region_of(first));
		printf("\n");
		nd = 0;
		first = ~0u;

		memcpy(probe_b2, obj_b, OBJ_SIZE);
		memcpy(obj_b, snap_b, OBJ_SIZE);
		memcpy(shaped_b, probe_shaped, sizeof(shaped_b));
		memcpy(sess_b, probe_sess, sizeof(sess_b));
		memcpy(pcm_b, probe_pcm, sizeof(pcm_b));
		memcpy(cfg_b, probe_cfg, sizeof(cfg_b));
		memcpy(dummy_b, probe_dummy, sizeof(dummy_b));

		step_side = 1;
		dsplib_debug_capture_reset();
		scrub_stack();
		probe_sw[2] = fpu_status();
		probe_cw[2] = fpu_control();
		alarm(5);
		ref_v34handshak(obj_b);
		alarm(0);
		for (i = 0; i < OBJ_SIZE; i++)
			if (obj_b[i] != probe_b2[i]) {
				if (nd == 0)
					first = i;
				nd++;
			}
		printf("  PROBE mst=%d rx=%d tx=%d  fpsw %04x/%04x/%04x "
		       "fpcw %04x/%04x/%04x  B-vs-B differs in %u bytes",
		       step_mst, step_rxst, step_txst,
		       probe_sw[0], probe_sw[1], probe_sw[2],
		       probe_cw[0], probe_cw[1], probe_cw[2], nd);
		if (nd)
			printf(" from +0x%04x", first);
		printf("\n");
	}

	signal(SIGALRM, prev);
}

void
v34hs_holes_check(void)
{
	unsigned k;
	char msg[128];

	/*
	 * Under V34HS_REFINIT both sides are brought up by the blob, and the
	 * eleven pointers the bring-up aims at a LIBRARY TABLE then hold the
	 * same address on both sides rather than ours and the blob's copy --
	 * so eleven of the skips legitimately never differ and this assertion
	 * does not apply to that run.  That agreement is itself a measurement:
	 * finding 324.
	 */
	if (!ref_both)
		for (k = 0; k < NHOLES; k++) {
			snprintf(msg, sizeof(msg),
				 "pointer skip +0x%04x was exercised",
				 holes[k]);
			diff_eq_int(msg, saw_hole[k], 1, (long)holes[k]);
		}
	diff_eq_int("pointer skips", NHOLES, V34HS_NHOLES, 0);
}

const struct v34hs_obs *
v34hs_observed(int side)
{
	return &obs[side & 1];
}

const char *
v34hs_text(int side)
{
	return text[side & 1];
}

/* --- the comparison ------------------------------------------------------- */

/*
 * The two self-pointers.  `v34handshakinit` aims +0xaa70 at the record at
 * +0xa97c (and mode 4 also aims +0xaa6c at +0xa94c), so the two sides hold
 * two different addresses and the check that means anything is "the same
 * offset from its own base", not "the same bytes" -- both records are nearly
 * all zero, so a swapped pair would pass a content comparison.
 */
static void
check_self_ptr(const char *what, unsigned off, long tag)
{
	const char *pa = peek_ptr(0, off), *pb = peek_ptr(1, off);
	long oa = pa - (const char *)&obj_a, ob = pb - (const char *)obj_b;
	long aa = pa - (const char *)&arena_a, ab = pb - (const char *)&arena_b;
	int ino_a = oa >= 0 && oa < (long)OBJ_SIZE;
	int ino_b = ob >= 0 && ob < (long)OBJ_SIZE;
	int ina = aa >= 0 && aa < (long)ARENA_SIZE;
	int inb = ab >= 0 && ab < (long)ARENA_SIZE;
	char msg[192];

	snprintf(msg, sizeof(msg), "%s +0x%04x: points into the object",
		 what, off);
	diff_eq_int(msg, ino_a, ino_b, tag);
	if (ino_a && ino_b) {
		snprintf(msg, sizeof(msg), "%s +0x%04x: offset within it",
			 what, off);
		diff_eq_int(msg, (int)oa, (int)ob, tag);
		return;
	}

	/*
	 * WHICH BLOCK A POINTER OUT OF THE OBJECT SELECTS.  Before the arena
	 * this could not be asked: the two sides held two addresses of two
	 * separate statics and the only comparable fact was "outside".  Now
	 * every block the object can point at lives at a fixed offset inside
	 * its own side's arena, so the block AND the offset within it are one
	 * subtraction and are address-independent.  That closes the gap
	 * docs/v34handshak.md named as the harness's one unchecked hole.
	 */
	snprintf(msg, sizeof(msg),
		 "%s +0x%04x: points into the fixture's own memory", what, off);
	diff_eq_int(msg, ina, inb, tag);
	if (ina && inb) {
		snprintf(msg, sizeof(msg), "%s +0x%04x: selects %s, at the "
			 "same offset in it", what, off,
			 region_of((unsigned)aa));
		diff_eq_int(msg, (int)aa, (int)ab, tag);
		return;
	}

	/*
	 * AND WHAT IS STILL NOT CHECKED, said plainly.  A pointer outside both
	 * arenas is a library table or a function, and side A's bring-up
	 * installs OURS where side B's installs the blob's -- two different
	 * addresses of two copies, which no address comparison can tell from
	 * two different tables.  `V34HS_REFINIT=1` brings both sides up with
	 * the blob's initialisers, and then the two must select the identical
	 * address; that run is the one that checks it, and it passes.
	 */
	if (ref_both) {
		snprintf(msg, sizeof(msg),
			 "%s +0x%04x: selects the same library table", what,
			 off);
		diff_eq_int(msg, pa == pb, 1, tag);
	}
}

void
v34hs_compare(const char *what, long tag)
{
	const unsigned char *a = (const unsigned char *)&obj_a;
	unsigned i, k;
	int bad = 0;
	char msg[192];

	for (k = 0; k < NHOLES; k++)
		if (memcmp(a + holes[k], obj_b + holes[k], 4) != 0)
			saw_hole[k] = 1;

	for (i = 0; i < OBJ_SIZE; i++) {
		if (a[i] == obj_b[i] || in_hole(i))
			continue;
		bad++;
		if (getenv("V34HS_DIAG"))
			printf("  DIAG %s +0x%05x: %02x vs %02x (case %ld)\n",
			       what, i, a[i], obj_b[i], tag);
		if (bad <= 8) {
			snprintf(msg, sizeof(msg),
				 "%s: object byte at +0x%x (case %ld)",
				 what, i, tag);
			diff_eq_int(msg, a[i], obj_b[i], (long)i);
		}
	}
	diff_eq_int(what, bad, 0, tag);

	/*
	 * EVERY INTERIOR POINTER, BY OFFSET.  Half the holes hold addresses
	 * INTO the object -- the two sample-queue cursor pairs, the ten echo
	 * canceller pointers, the four shell contexts and the two message
	 * records -- and a skipped pointer is a hole a wrong offset walks
	 * straight through.  t_v34hshak.c checks two of them this way; every
	 * one is checked here, because a cursor one element out changes what
	 * the next step reads without changing a byte this comparison sees.
	 */
	for (k = 0; k < NHOLES; k++)
		check_self_ptr("v34handshak interior pointer", holes[k], tag);

	/*
	 * EVERYTHING ELSE IN THE ARENA, in one sweep: the shaping buffer, the
	 * session, the PCM receiver, the configuration, the seed tables AND
	 * the padding between and around them.
	 *
	 * The padding is the half that is new and the half that matters.  Four
	 * named memcmps over the four blocks were what this used to be, and
	 * they said nothing about a read or a write one element off the end of
	 * one -- which is exactly what D60 turned out to be.  The seed tables
	 * were not compared at all, though thirty of the thirty-five skipped
	 * pointers aim at them.
	 *
	 * The only four bytes exempt are the session's pointer to the PCM
	 * receiver, which is an address and so differs by construction.
	 */
	{
		const unsigned char *pa = (const unsigned char *)&arena_a;
		const unsigned char *pb = (const unsigned char *)&arena_b;
		unsigned sp = AR_OFF(sess) + SESS_PCM;
		unsigned nbad = 0, firstbad = 0;

		for (i = 0; i < ARENA_SIZE; i++) {
			if (i >= OBJ_IN_ARENA && i < OBJ_IN_ARENA + OBJ_SIZE)
				continue;
			if (i >= sp && i < sp + 4)
				continue;
			if (pad_varied && in_padding(i))
				continue;
			if (pa[i] == pb[i])
				continue;
			if (nbad++ == 0)
				firstbad = i;
		}
		snprintf(msg, sizeof(msg), "%s: the arena outside the object "
			 "(first differing byte in %s)", what,
			 nbad ? region_of(firstbad) : "nothing");
		diff_eq_int(msg, nbad, 0, tag);
	}

	/* And what each side observed, including the transcript. */
	snprintf(msg, sizeof(msg), "%s: bytes written", what);
	diff_eq_int(msg, obs[0].changed, obs[1].changed, tag);
	snprintf(msg, sizeof(msg), "%s: step signature", what);
	diff_eq_int(msg, obs[0].hash, obs[1].hash, tag);
	snprintf(msg, sizeof(msg), "%s: diagnostic lines", what);
	diff_eq_int(msg, obs[0].lines, obs[1].lines, tag);
	snprintf(msg, sizeof(msg), "%s: transcript", what);
	diff_eq_int(msg, strcmp(text[0], text[1]) == 0, 1, tag);
}
