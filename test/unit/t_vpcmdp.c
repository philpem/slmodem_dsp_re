/*
 * t_vpcmdp.c -- differential test of the four parts of `src/pump/v90/vpcm.c`
 * that are not `vpcm_run`: `vpcm_create` (0x3a00, 969 B), `vpcm_delete`
 * (0x3dd0, 110 B), `dp_vpcm_init` (0x44c0, 72 B) and the `struct
 * dp_operations` at .data+0x30 that ties the three together.
 *
 * `vpcm_run` is `t_vpcmrun.c`'s and is not re-tested here.
 *
 * ===========================================================================
 * WHAT IS COMPARED, AND WHAT IS DELIBERATELY NOT
 * ===========================================================================
 *
 * `vpcm_create` allocates a 0xd258 root and then calls `VPCMXF_Create`,
 * `K56FLEX_Create` and `VPcmV34Create`, which between them build 124 further
 * heap regions -- at the host's own configuration, 127 allocations and 279,600
 * bytes, measured, and equal on both sides.  The two sides build their OWN
 * graphs at their own addresses, so nothing outside the root can be compared
 * word for word without inventing a correspondence between two independently
 * allocated graphs.
 *
 * THE BOUND, STATED PLAINLY.  This file compares
 *
 *   - the ROOT allocation over its whole 0xd258 request, with every pointer
 *     canonicalised (below).  The root CONTAINS the V.34 object at +0x2c, so
 *     everything `VPcmV34Create` wrote into it is inside this comparison;
 *   - the `struct _tagModemParameters` runtime block, all 0x88 bytes of it,
 *     which `vpcm_create` writes at +0x30, +0x34, +0x38, +0x3c, +0x64, +0x68,
 *     +0x6c and +0x78 and read-modify-writes at +0x02 twice;
 *   - the `struct dsp_info` block, which `vpcm_delete` writes two words of;
 *   - all five allocator counters, and the host's `modem_set_param` log;
 *   - the two functions' diagnostics, as text.
 *
 * and does NOT compare the other 124 regions.  Their contents are the callees'
 * claim: `test/unit/t_vpcmcreate.c` compares `VPcmV34Create` over the whole
 * 265,520-byte graph, `test/unit/t_vpcmctor.cpp` compares `VPCMXF_Create` and
 * `VPcmFloModem`'s constructor and destructor over theirs, and
 * `test/mutations/k56flexalloc.json` covers `K56FLEX_Create`'s twenty bytes.
 * What IS asserted about them here is that both sides made the same number of
 * allocations of the same total size, that the root points into them at the
 * same offsets of the same-sized blocks, and that everything is released again.
 *
 * ===========================================================================
 * HOW THE ROOT IS MADE COMPARABLE
 * ===========================================================================
 *
 * Every 4-aligned word is TRANSLATED before comparison:
 *
 *   into the root itself      -> 0x40000000 + offset      (`dp.dp_data`)
 *   the modem handle          -> 0x41000000
 *   into the runtime block    -> 0x42000000 + offset
 *   into the `dsp_info`       -> 0x43000000 + offset
 *   into a live allocation    -> 0x50000000 + K*0x100000 + offset, where K is
 *                                the index the block gets the FIRST time a
 *                                word of the root points at it, scanning the
 *                                root upwards.
 *
 * The first-appearance numbering is deliberate and is not an ordering
 * assumption about the allocator: it is derived from the thing under test, so
 * two graphs number their blocks alike exactly when the root points at
 * corresponding blocks in corresponding order -- which is what the comparison
 * is supposed to decide.  Two blocks are reachable from the root (`v34.xf` at
 * +0x3574 and `v34.k56` at +0xac44) and the block SIZES are compared too.
 *
 * A WORD WHOSE BYTES ARE ALREADY EQUAL IS NOT TRANSLATED AT ALL, and the long
 * comment in `root_compare` is the measurement that forced that: a plain
 * integer constant in the V.34 object falls inside one side's heap on about
 * one run in thirty, because the heap base is randomised and the constant is
 * not.
 *
 * TWELVE WORDS SURVIVE THE TRANSLATION AND CANNOT AGREE.  They are addresses
 * of STATIC objects that `v34handshakinit` installs in the V.34 object -- ten
 * coefficient tables and two scrambler entry points -- and ours point into our
 * image where the blob's point into its own.  They are t_vpcmcreate.c's twelve,
 * at the same offsets, and this file does not merely skip them: each must hold
 * two DIFFERENT addresses whose targets are byte-identical over the size of the
 * table named, or, for the two function slots, our symbol against the blob's
 * `ref_` alias.  A word that differs and is NOT one of the twelve is a failure
 * and is reported, never absorbed into a learned mask.  Measured: with the
 * root's 13,462 words compared on every trial, twelve differ and they are
 * exactly these twelve.
 *
 * ===========================================================================
 * THE ALLOCATOR'S SLACK
 * ===========================================================================
 *
 * `malloc_usable_size` overshoots a request by up to `grain - 1` bytes and the
 * residue belongs to neither side -- ours runs first on fresh memory and the
 * blob's second.  The granularity is MEASURED here rather than assumed.  It
 * costs this file nothing, because every one of the three compared regions has
 * a request length that is known exactly (0xd258 from the object's own
 * `movl $0xd258,(%esp)` at 0x3a42, 0x88 for the runtime block and 0x10 for the
 * `dsp_info`), so no residue is inside any comparison and there is nothing to
 * poison.  The honest bound is the other way round: a store PAST the root's
 * 0xd258 into those few residue bytes would not be seen here.
 *
 * ===========================================================================
 * SEEDING, AND THE ONE PLACE IT HAS TO BE RESTRAINED
 * ===========================================================================
 *
 * `dp_runtime_create` memsets the block and then explicitly zeroes +0x64,
 * +0x68 and +0x6c, so `vpcm_create`'s `addedDelay = 0` and `paramFile = 0`
 * would write zero over zero and be INVISIBLE.  Every field `vpcm_create`
 * writes is therefore seeded non-zero before each trial, which is
 * t_vpcmcreate.c's `seed_v34` argument applied to the runtime block.
 *
 * The block is NOT filled with random bytes.  `V90Parameters::init` derives a
 * 0x558-byte parameter block from it and feeds x87 arithmetic and allocation
 * lengths with the result; a seeded block gives a signalling NaN or a two
 * billion byte allocation often enough to make the run meaningless, which is
 * why t_vpcmctor.cpp zeroes it.  So the block starts as the host's own
 * `dp_runtime_create` left it -- that IS the definition of what
 * MDMPRM_DPRUNTIME answers with -- and only the fields the sweep needs are
 * moved off it.
 *
 * ONE RUNTIME BLOCK FOR THE WHOLE FILE, restored from a pristine image at each
 * trial rather than re-allocated.  `harness_alloc_reset` forgets the live set,
 * so a block allocated before the reset and freed after it counts as a
 * `bad_free`; keeping one block and never freeing it is what keeps the
 * allocator counters meaning "the graph" and nothing else.  Do not "fix" this
 * into a create/delete pair.
 *
 * BOTH SIDES SHARE THAT ONE BLOCK, because `MDMPRM_DPRUNTIME` is one override
 * read by both.  So it is saved before our side runs, saved again after it,
 * restored, and compared against the second save once the blob's side has run
 * -- otherwise the second writer's stores land on top of the first's and a
 * disagreement is invisible.  t_vpcmctor.cpp's shape, and for its reason.
 *
 * THREE OF THE ROOT'S STORES ARE INVISIBLE TO ANY DIFFERENTIAL TEST and that
 * is a property of the object, not a gap here: `status = 0`, `mode = 0` and
 * `stall = 0` write zero into a block that `sysdep_memset(s, 0, 0xd258)`
 * cleared three lines earlier.  They are asserted absolutely anyway, and the
 * mutation suite records the corresponding deletions as `equivalent` with the
 * memset as the proof.  `outq.count = 4` is not one of them and is checked.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "harness.h"

#include "dsplib/dp.h"
#include "dsplib/dp_wrapper.h"
#include "dsplib/modem_params.h"

/*
 * DECLARED HERE AND NOT INCLUDED FROM `dsplib/vpcm.h`.  The header carries
 * `struct vpcm_root`, which is the map this file is checking; a test that
 * shares the map with the thing it tests cannot catch the map being wrong.
 * Every root offset below is spelled as the literal the disassembly uses.
 */
extern int dp_vpcm_init(void);

extern struct dp *ref_vpcm_create(void *modem, int id, int caller, int srate,
				  int max_frag, struct dp_operations *op);
extern int ref_vpcm_delete(struct dp *dp);
extern int ref_vpcm_run(struct dp *dp, void *in, void *out, int count);
extern int ref_dp_vpcm_init(void);
extern struct dp_operations ref_vpcm_op;

/*
 * Our four entry points are file-static in the object, so no name reaches
 * them.  `dp_vpcm_init` registers `vpcm_op`, and its `.process` IS `vpcm_run`
 * directly, so the registered table is the whole handle: `->create` calls
 * `vpcm_create`, `->destroy` calls `vpcm_delete` and `->process` calls
 * `vpcm_run`.  `find_ops` registers ours once and keeps the pointer.
 */
static struct dp_operations *our_ops;

static int
find_ops(void)
{
	harness_reg_reset();
	dp_vpcm_init();
	if (harness_reg_ours.count < 1)
		return 0;
	our_ops = (struct dp_operations *)harness_reg_ours.ops[0];
	return our_ops != 0 && our_ops->create != 0 && our_ops->destroy != 0
	    && our_ops->process != 0;
}

extern void *ref_dp_runtime_create(void *modem);

extern unsigned int dsplibs_debug_level;
extern unsigned int ref_dsplibs_debug_level;

/* The two scrambler entry points the twelfth and sixth alias words hold. */
extern void descrambleGPA(void);
extern void descrambleGPC(void);
extern void scrambleGPA(void);
extern void scrambleGPC(void);
extern void ref_descrambleGPA(void);
extern void ref_descrambleGPC(void);
extern void ref_scrambleGPA(void);
extern void ref_scrambleGPC(void);

/* --- the root's layout, as the disassembly spells it ---------------------- */

#define ROOT_BYTES	0xd258		/* movl $0xd258,(%esp)  at 0x3a42     */
#define R_ID		0x00000		/* movl %edi,(%ebx)     at 0x3a93     */
#define R_MODEM		0x00004		/* mov  %ecx,0x4(%ebx)  at 0x3a90     */
#define R_STATUS_DP	0x00008		/* struct dp's own `status`           */
#define R_OP		0x0000c
#define R_DPDATA	0x00010		/* mov  %ebx,0x10(%ebx) at 0x3a76     */
#define R_STATUS	0x00014
#define R_MODE		0x00018
#define R_NBITS		0x0001c
#define R_STALL		0x00020
#define R_INFO		0x00024		/* mov  %eax,0x24(%ebx) at 0x3abf     */
#define R_PARAMS	0x00028
#define R_V34		0x0002c		/* lea  0x2c(%ebx),%ebp at 0x3add     */
#define R_XF		0x03574		/* the V.34 object's +0x3548          */
#define R_K56		0x0ac44		/* the V.34 object's +0xac18          */
#define R_OUTQ		0x0d178
#define R_INQ		0x0d1e4
#define R_MUTE		0x0d250
#define R_EXTRADELAY	0x0d254

/* --- the literals `vpcm_create` branches on, spelled as its immediates ---- */

#define V_SRATE		9600		/* cmp $0x2580,%esi; jne  at 0x3a1c   */
#define V_MAX_FRAG	48		/* cmpl $0x30; jg         at 0x3a37   */
#define V_RATE_CAP	0xdac0u		/* cmp $0xdac0,%eax; jbe  at 0x3b65   */
#define V_PARAM_MIN	0x12c0		/* the literals at 0x3b90             */
#define V_PARAM_MAX	0x8340
#define V_MUTE		0x210		/* and $0x210,%edi        at 0x3bd9   */
#define V_DELAY_CAP	0xf4		/* mov $0xf4,%ecx         at 0x3bf1   */
#define V_EXTRA_MIN	0x180		/* cmp $0x180,%eax; jge   at 0x3d8c   */
#define V_DP_V34	0x22
#define V_DP_V90	0x5a
#define V_DP_V92	0x5c

#define MODEM		((void *)0xD1A1u)

/* --- the fixture ---------------------------------------------------------- */

static struct dsp_info dspinfo;
static struct _tagModemParameters *runtime;
static unsigned char rt_pristine[sizeof(struct _tagModemParameters)]
	__attribute__((aligned(8)));
static unsigned char rt_pre[sizeof(struct _tagModemParameters)]
	__attribute__((aligned(8)));
static unsigned char rt_ours[sizeof(struct _tagModemParameters)]
	__attribute__((aligned(8)));
static unsigned char di_pre[sizeof(struct dsp_info)]
	__attribute__((aligned(8)));
static unsigned char di_ours[sizeof(struct dsp_info)]
	__attribute__((aligned(8)));

/*
 * The `op` argument is a DUMMY and not the registered table.  Nothing
 * dereferences `dp.op`, so what the store has to prove is that it keeps the
 * ARGUMENT rather than a constant that happens to match the registered table;
 * two dummies, alternated across trials, is what says that.
 */
static struct dp_operations op_dummy[2];

static size_t alloc_grain = 16;

static void
measure_grain(void)
{
	size_t n, prev = 0, best = 0;

	for (n = 1; n <= 256; n++) {
		void *p = malloc(n);
		size_t u = malloc_usable_size(p);

		free(p);
		if (prev != 0 && u > prev && (best == 0 || u - prev < best))
			best = u - prev;
		prev = u;
	}
	if (best != 0)
		alloc_grain = best;
}

/* --- the two heap graphs -------------------------------------------------- */

#define MAXLIVE	1024

struct graph {
	void		*p[MAXLIVE];
	unsigned long	end[MAXLIVE];
	size_t		sz[MAXLIVE];
	int		idx[MAXLIVE];
	int		n;
	int		nass;
	int		assigned[MAXLIVE];	/* block index, by canonical k */
	unsigned long	lo, hi;
	const unsigned char *root;
};

static struct graph go, gb;
static unsigned char root_ours[ROOT_BYTES];
static unsigned char first_root[ROOT_BYTES];

static void
graph_bounds(struct graph *g)
{
	int i;

	g->lo = ~0ul;
	g->hi = 0;
	for (i = 0; i < g->n; i++) {
		if ((unsigned long)g->p[i] < g->lo)
			g->lo = (unsigned long)g->p[i];
		if (g->end[i] > g->hi)
			g->hi = g->end[i];
	}
	g->nass = 0;
}

/* Everything live, minus everything in `other` (pass 0 for "everything"). */
static int
graph_take(struct graph *g, const struct graph *other, const void *root)
{
	void *tmp[MAXLIVE];
	int n = harness_alloc_live_set(tmp, MAXLIVE);
	int i, j, k = 0;

	if (n > MAXLIVE)
		return -1;
	for (i = 0; i < n; i++) {
		int seen = 0;

		if (other != 0)
			for (j = 0; j < other->n; j++)
				if (other->p[j] == tmp[i])
					seen = 1;
		if (seen)
			continue;
		g->p[k] = tmp[i];
		g->sz[k] = malloc_usable_size(tmp[i]);
		g->end[k] = (unsigned long)tmp[i] + g->sz[k];
		g->idx[k] = -1;
		k++;
	}
	g->n = k;
	g->root = (const unsigned char *)root;
	graph_bounds(g);
	return k;
}

static unsigned long
translate(struct graph *g, unsigned long v)
{
	unsigned long r = (unsigned long)g->root;
	int i;

	if (v == 0)
		return 0;
	if (v >= r && v < r + ROOT_BYTES)
		return 0x40000000ul + (v - r);
	if (v == (unsigned long)MODEM)
		return 0x41000000ul;
	if (v >= (unsigned long)runtime
	    && v < (unsigned long)runtime + sizeof *runtime)
		return 0x42000000ul + (v - (unsigned long)runtime);
	if (v >= (unsigned long)&dspinfo
	    && v < (unsigned long)&dspinfo + sizeof dspinfo)
		return 0x43000000ul + (v - (unsigned long)&dspinfo);
	if (v < g->lo || v >= g->hi)
		return v;
	for (i = 0; i < g->n; i++)
		if (v >= (unsigned long)g->p[i] && v < g->end[i]) {
			if (g->idx[i] < 0) {
				g->idx[i] = g->nass;
				g->assigned[g->nass] = i;
				g->nass++;
			}
			return 0x50000000ul
			    + (unsigned long)g->idx[i] * 0x00100000ul
			    + (v - (unsigned long)g->p[i]);
		}
	return v;
}

/* --- the twelve words that cannot agree ----------------------------------- */

/*
 * t_vpcmcreate.c's `aliasword[]`, at the same twelve offsets in the V.34
 * object, checked the same way: two DIFFERENT addresses whose targets are
 * byte-identical over the size of the object named, or, for the two function
 * slots, our symbol against the blob's `ref_` alias.  Two of the ten tables
 * (`ec_prem_coef_B3429`, `preemp0`) are file-static in our build and cannot be
 * named from here, which is why the contents check exists at all.
 */
struct alias_word {
	unsigned	off;		/* offset in the V.34 object          */
	unsigned	n;		/* bytes at the target, 0 = a function */
	const void	*fours[2];
	const void	*fblob[2];
	const char	*name;
};

#define DATAW(o, n, nm)	{ (o), (n), { 0, 0 }, { 0, 0 }, (nm) }

static const struct alias_word aliasword[] = {
	DATAW(0x0418,  64, "hsine1800"),
	DATAW(0x0508, 120, "bpv22high"),
	DATAW(0x0620,  80, "V34TimingPrefilterCoeff"),
	DATAW(0x0624,  80, "V34TimingHPFilterCoeff"),
	DATAW(0x0a28, 128, "Convolve16"),
	{ 0x0e48, 0,
	  { (const void *)&descrambleGPA, (const void *)&descrambleGPC },
	  { (const void *)&ref_descrambleGPA, (const void *)&ref_descrambleGPC },
	  "the descrambler" },
	DATAW(0x1460,  32, "hsine1200"),
	DATAW(0x20cc,  84, "ec_prem_coef_B3429"),
	DATAW(0x2100,  32, "preemp0"),
	DATAW(0x2608, 128, "Convolve16"),
	{ 0x2a28, 0,
	  { (const void *)&scrambleGPA, (const void *)&scrambleGPC },
	  { (const void *)&ref_scrambleGPA, (const void *)&ref_scrambleGPC },
	  "the scrambler" },
	DATAW(0x3564,  16, "c2400_")
};

#define NALIAS	((int)(sizeof(aliasword) / sizeof(aliasword[0])))

static int
alias_index(unsigned rootoff)
{
	int k;

	for (k = 0; k < NALIAS; k++)
		if (R_V34 + aliasword[k].off == rootoff)
			return k;
	return -1;
}

/*
 * Every one of the twelve checked to BE an alias pair rather than assumed to be
 * one.  Returns the number that were not.
 */
static int
check_alias_words(const unsigned char *ra, const unsigned char *rb)
{
	int k, bad = 0;

	for (k = 0; k < NALIAS; k++) {
		const void *a, *b;
		size_t w = (size_t)R_V34 + aliasword[k].off;

		memcpy(&a, ra + w, sizeof a);
		memcpy(&b, rb + w, sizeof b);

		if (a == b) {
			printf("    %s at v34+0x%x: both sides %p -- not an"
			       " alias pair\n", aliasword[k].name,
			       aliasword[k].off, a);
			bad++;
			continue;
		}
		if (aliasword[k].n == 0) {
			if ((a != aliasword[k].fours[0]
			     || b != aliasword[k].fblob[0])
			    && (a != aliasword[k].fours[1]
				|| b != aliasword[k].fblob[1])) {
				printf("    %s at v34+0x%x: %p/%p is neither"
				       " candidate pair\n", aliasword[k].name,
				       aliasword[k].off, a, b);
				bad++;
			}
			continue;
		}
		if (memcmp(a, b, aliasword[k].n) != 0) {
			printf("    %s at v34+0x%x: %p and %p differ over %u"
			       " bytes\n", aliasword[k].name, aliasword[k].off,
			       a, b, aliasword[k].n);
			bad++;
		}
	}
	return bad;
}

/*
 * The whole root, word by word, translated.  Returns the number of words that
 * differ and are NOT one of the twelve; `*nalias` comes back as how many of the
 * twelve really did differ, so "all twelve fired" is checkable and an
 * exclusion that quietly stopped applying cannot go unnoticed.
 */
static int reported;

static int
root_compare(const unsigned char *ra, const unsigned char *rb, int *nalias,
	     long tag)
{
	int bad = 0;
	unsigned o;

	*nalias = 0;
	go.nass = 0;
	gb.nass = 0;
	for (o = 0; o < (unsigned)go.n; o++)
		go.idx[o] = -1;
	for (o = 0; o < (unsigned)gb.n; o++)
		gb.idx[o] = -1;

	for (o = 0; o + 4 <= ROOT_BYTES; o += 4) {
		unsigned int wa, wb;
		unsigned long ta, tb;

		memcpy(&wa, ra + o, 4);
		memcpy(&wb, rb + o, 4);
		/*
		 * IDENTICAL BYTES ARE EQUAL AND ARE NOT TRANSLATED, and this
		 * is not an optimisation -- it is the fix for a measured
		 * flakiness.  `translate` cannot tell a pointer from an integer
		 * that happens to fall inside a live allocation, and the V.34
		 * object holds the constant 0x09600000 at its +0x434.  The heap
		 * base is randomised, so on about one run in thirty that
		 * constant landed inside one side's graph (or inside the root)
		 * and not the other's; the word then translated to a tag on one
		 * side and to itself on the other, and -- worse -- consumed a
		 * canonical block index, renumbering every real pointer after
		 * it.  15 of 400 runs failed that way before this line existed.
		 *
		 * Skipping equal bytes removes the whole class, and it removes
		 * nothing real: two words holding the same bit pattern agree by
		 * the strongest test there is, and two INDEPENDENT graphs can
		 * never put a block at one address, so an equal pair can never
		 * be two pointers that ought to have differed.
		 *
		 * The mirror hazard survives and is stated rather than fixed: a
		 * differing pair where one side's non-pointer integer happens to
		 * translate to exactly the other side's tag would be missed.
		 * That needs a coincidence in the low 20 bits as well as the
		 * high ones, and nothing in 400 runs produced one.
		 */
		if (wa == wb)
			continue;
		ta = translate(&go, wa);
		tb = translate(&gb, wb);
		if (ta == tb)
			continue;
		if (alias_index(o) >= 0) {
			++*nalias;
			continue;
		}
		bad++;
		if (reported++ < 12)
			printf("    trial %ld: root +0x%05x (the V.34 object"
			       " +0x%05x): ours %08x blob %08x, translated"
			       " %08lx/%08lx\n", tag, o, o - R_V34, wa, wb, ta,
			       tb);
	}
	return bad;
}

/* --- reading the root ----------------------------------------------------- */

static int
rd(const unsigned char *r, unsigned off)
{
	int v;

	memcpy(&v, r + off, sizeof v);
	return v;
}

static void *
rdp(const unsigned char *r, unsigned off)
{
	void *v;

	memcpy(&v, r + off, sizeof v);
	return v;
}

/* --- the sweep ------------------------------------------------------------ */

/*
 * THE FOUR ids ARE THE WHOLE OF WHAT THERE IS.  0x22, 0x5a and 0x5c are the
 * three `dp_vpcm_init` registers, and `vpcm_create` branches on exactly two of
 * them: `id == 0x5c` twice (the V.92 bit of `qcFlags`, and `sessionType = 2`)
 * and `id == 0x5a` once (`sessionType = 1`).  0x37 is none of the three and is
 * swept because "not V.92 and not V.90" has to be driven by something the host
 * would not send as well as by the one id that is.
 */
static const int id_v[] = { V_DP_V34, V_DP_V90, V_DP_V92, 0x37 };
#define NID	((int)(sizeof(id_v) / sizeof(id_v[0])))

/*
 * BIT 4 OF `qcFlags`, BOTH WAYS, AND SIX VALUES OF THE OTHER SEVEN BITS.  The
 * mute counter is `cmp $1 / sbb %edi,%edi / and $0x210,%edi`, which leaves the
 * mask when the bit is CLEAR -- the inverted reading -- so a sweep with one
 * value of that bit says nothing at all.  The other bits are varied because the
 * two read-modify-writes must preserve them: `& 0xef` then `& 0xdf`.
 */
static const unsigned char qc_v[] = { 0x00, 0x10, 0xef, 0xff, 0x2f, 0xdf };
#define NQC	((int)(sizeof(qc_v) / sizeof(qc_v[0])))

/*
 * THE DELAY AND RATE AXES, and each entry names the fork it exists to reach.
 *
 * `hwDelay = MDMPRM_IODELAY + 4`, and when 0xf4 - hwDelay is negative the
 * function tells the host by that much, clamps the negated difference up to at
 * least 0x180 and redoes the arithmetic with 0xf4.  So the boundaries that
 * matter are hwDelay at 0xf4 and just over it, and the `jge`'s two sides at an
 * excess of 0x17f, 0x180 and 0x181.
 *
 * `max_frag` is capped at 48 by `cmpl $0x30; jg`.  BELOW TEN IS NOT SWEPT and
 * that is recorded rather than hidden: `nsamples = max_frag * 1000 / srate` is
 * zero for max_frag < 10 at srate 9600, and `VPCMXF_Create`'s sub-constructors
 * then divide by it -- a SIGFPE on BOTH sides, which measures nothing about
 * `vpcm_create` (t_vpcmctor.cpp records the same case for `durationMs == 0`).
 */
struct cfg {
	int		max_frag;
	int		iodelay;
	long		minrate;
	long		maxrate;
	const char	*what;
};

static const struct cfg cfg_v[] = {
	{ 48,     0,   300, 56000, "the host's own configuration" },
	{ 48, 0x0f0,   300, 56000, "hwDelay exactly 0xf4: the cap NOT taken" },
	{ 48, 0x0f1,   300, 56000, "hwDelay 0xf5: excess 1, clamped to 0x180" },
	{ 48, 0x26f,   300, 56000, "excess 0x17f: just under the jge" },
	{ 48, 0x270,   300, 56000, "excess 0x180: the jge boundary itself" },
	{ 48, 0x271,   300, 56000, "excess 0x181: just over the jge" },
	{ 48,    -8,   300, 56000, "a negative iodelay: hwDelay -4, no cap" },
	{ 10,     0,   300, 56000, "max_frag 10, the smallest giving nsamples>0" },
	{ 24, 0x100,   300, 56000, "max_frag 24 with the cap taken" },
	{ 47,     0,  2400, 56001, "max_frag 47; maxRate one over the cap" },
	{ 48,     0,     0, 0xdac0,"maxRate exactly the cap; minRate 0" },
	{ 48,     0, 33600, 0xffff0000L,
				   "maxRate huge: the compare is UNSIGNED" },
	{ 48, 0x2000,  1200,  4800, "far over the cap, and a low rate window" }
};

#define NCFG	((int)(sizeof(cfg_v) / sizeof(cfg_v[0])))

/*
 * THE SWEEP IS THE FULL CROSS PRODUCT: 4 ids x 6 `qcFlags` seeds x 2 callers x
 * 13 configurations = 624 trials, and it is spelled that way rather than as a
 * covering design because it costs about half a second.  The (id, bit 4) pair
 * HAS to be crossed -- the V.92 bit survives only when the id is V.92, so those
 * two are one fork and not two -- and once the loop exists the other two axes
 * are free.
 */
#define NTRIAL	(NID * NQC * 2 * NCFG)

struct expect {
	unsigned char	qcflags;
	int		mute;
	int		extradelay;
	int		hwdelay;
	int		dmadelay;
	int		setdelay;	/* the MDMPRM_UPDATE_DELAY argument   */
	int		nsetdelay;	/* how many such calls                */
	unsigned	ratelo, ratehi;
};

static void
expected(const struct cfg *c, int id, unsigned char qc, struct expect *e)
{
	int hw = c->iodelay + 4;
	int keep = (id == V_DP_V92) ? (qc & 0x10) : 0;
	unsigned rhi = (unsigned)(int)c->maxrate;

	e->qcflags = (unsigned char)((qc & 0xcf) | keep);
	e->mute = (((e->qcflags >> 4) & 1) < 1) ? V_MUTE : 0;

	e->extradelay = 0;
	e->setdelay = 0;
	e->nsetdelay = 0;
	if (V_DELAY_CAP - hw < 0) {
		int excess = hw - V_DELAY_CAP;

		e->setdelay = V_DELAY_CAP - hw;
		e->nsetdelay = 1;
		if (excess < V_EXTRA_MIN)
			excess = V_EXTRA_MIN;
		e->extradelay = excess;
		hw = V_DELAY_CAP;
	}
	e->hwdelay = hw;
	e->dmadelay = hw - 0x30 + e->extradelay;

	e->ratelo = (unsigned)(int)c->minrate;
	e->ratehi = rhi > V_RATE_CAP ? V_RATE_CAP : rhi;
}

static char paramfile_seed[] = "";

/* Seed everything `vpcm_create` writes, so that no store writes zero on zero. */
static void
seed_runtime(long trial, unsigned char qc)
{
	memcpy(runtime, rt_pristine, sizeof rt_pristine);
	runtime->qcFlags = qc;
	runtime->vpcmRateLimitLow = 0xdead0000u + (unsigned)trial;
	runtime->vpcmRateLimitHigh = 0xbeef0000u + (unsigned)trial;
	runtime->minRate = 0x11110000u + (unsigned)trial;
	runtime->maxRate = 0x22220000u + (unsigned)trial;
	runtime->hwDelay = -0x5150 - (int)trial;
	runtime->dmaDelay = -0x6160 - (int)trial;
	runtime->addedDelay = -0x7170 - (int)trial;
	/*
	 * A REAL string and not a bogus address: the blob's `V90Parameters::
	 * init` does `if (paramFile) loadParams(paramFile)`, so a mutation
	 * that drops `vpcm_create`'s null store must leave something safe to
	 * dereference or the run dies for the wrong reason.
	 */
	runtime->paramFile = paramfile_seed;
	runtime->connectionType = (int)(0x31410000 + trial * 7);
	runtime->clockDeviation = (int)(-0x27180000 - trial * 11);
}

static void
seed_dspinfo(long trial)
{
	dspinfo.connection_type = 0xa5a50000u + (unsigned)trial;
	dspinfo.clock_deviation = (int)(0x5a5a0000u + (unsigned)trial);
	dspinfo.qc_lapm = 0x11220000u + (unsigned)trial;
	dspinfo.qc_index = 0x33440000u + (unsigned)trial;
}

static int
count_setdelay(const struct modem_shim *s, int *value)
{
	int i, n = 0;

	for (i = 0; i < s->nparams && i < HARNESS_SHIM_PARAMS; i++)
		if (s->param_name[i] == MDMPRM_UPDATE_DELAY) {
			if (n == 0)
				*value = s->param_value[i];
			n++;
		}
	return n;
}

/*
 * Every absolute claim about one side's root and the shared runtime block.
 *
 * `diff_eq_int`'s format takes exactly ONE conversion and it is the input, so
 * the side is folded into the tag as `2 * trial + side` -- even is ours and odd
 * is the blob's.  Both sides are asserted absolutely and not merely compared: a
 * reading of the inverted `sbb` that is wrong the same way on both sides would
 * survive any number of comparisons.
 */
static void
check_side(const unsigned char *r, const struct dp *d,
	   const struct _tagModemParameters *p, int id,
	   const struct expect *e, const struct dp_operations *op, long tag)
{
	diff_eq_int("dp.id is the id it was given (2*trial+side %ld)",
		    rd(r, R_ID), id, tag);
	diff_eq_int("dp.modem is the handle (2*trial+side %ld)",
		    rdp(r, R_MODEM) == MODEM, 1, tag);
	diff_eq_int("dp.op is the ARGUMENT (2*trial+side %ld)",
		    rdp(r, R_OP) == (const void *)op, 1, tag);
	/*
	 * `r` is our SAVED IMAGE of the root and `d` is the live block, so the
	 * word has to be compared against `d` on both sides -- an image cannot
	 * point at itself.
	 */
	diff_eq_int("dp.dp_data is the root itself (2*trial+side %ld)",
		    rdp(r, R_DPDATA) == (const void *)d, 1, tag);
	diff_eq_int("struct dp's own status is 0 (2*trial+side %ld)",
		    rd(r, R_STATUS_DP), 0, tag);
	diff_eq_int("status is 0 (2*trial+side %ld)", rd(r, R_STATUS), 0, tag);
	diff_eq_int("mode is 0 (2*trial+side %ld)", rd(r, R_MODE), 0, tag);
	diff_eq_int("nbits is 0 (2*trial+side %ld)", rd(r, R_NBITS), 0, tag);
	diff_eq_int("stall is 0 (2*trial+side %ld)", rd(r, R_STALL), 0, tag);
	diff_eq_int("info is the dsp_info (2*trial+side %ld)",
		    rdp(r, R_INFO) == (void *)&dspinfo, 1, tag);
	diff_eq_int("params is the runtime block (2*trial+side %ld)",
		    rdp(r, R_PARAMS) == (void *)runtime, 1, tag);
	diff_eq_int("outq.count is 4 (2*trial+side %ld)", rd(r, R_OUTQ), 4, tag);
	diff_eq_int("inq.count is 0 (2*trial+side %ld)", rd(r, R_INQ), 0, tag);
	/*
	 * THE INVERTED `sbb`, ASSERTED ABSOLUTELY.  A modem that is NOT doing
	 * V.92 mutes its first 528 samples and one that is does not; comparing
	 * the two sides alone would leave both readings passing.
	 */
	diff_eq_int("mute is 528 exactly when the V.92 bit came out CLEAR"
		    " (2*trial+side %ld)", rd(r, R_MUTE), e->mute, tag);
	diff_eq_int("extradelay (2*trial+side %ld)", rd(r, R_EXTRADELAY),
		    e->extradelay, tag);

	diff_eq_int("qcFlags: bit 4 kept only for V.92, bit 5 cleared, the"
		    " other six untouched (2*trial+side %ld)", p->qcFlags,
		    e->qcflags, tag);
	diff_eq_int("vpcmRateLimitLow is MDMPRM_MIN_RATE (2*trial+side %ld)",
		    (long)p->vpcmRateLimitLow, (long)e->ratelo, tag);
	diff_eq_int("vpcmRateLimitHigh is MDMPRM_MAX_RATE capped at 56000"
		    " (2*trial+side %ld)", (long)p->vpcmRateLimitHigh,
		    (long)e->ratehi, tag);
	diff_eq_int("minRate is the literal 4800 (2*trial+side %ld)",
		    (long)p->minRate, V_PARAM_MIN, tag);
	diff_eq_int("maxRate is the literal 33600 (2*trial+side %ld)",
		    (long)p->maxRate, V_PARAM_MAX, tag);
	diff_eq_int("hwDelay (2*trial+side %ld)", p->hwDelay, e->hwdelay, tag);
	diff_eq_int("dmaDelay is hwDelay - 0x30 + extradelay (2*trial+side"
		    " %ld)", p->dmaDelay, e->dmadelay, tag);
	diff_eq_int("addedDelay is 0 (2*trial+side %ld)", p->addedDelay, 0, tag);
	diff_eq_int("paramFile is NULL (2*trial+side %ld)", p->paramFile == 0,
		    1, tag);
}

static int
run_create(void)
{
	int i;
	long changed = 0, distinct = 0;
	int first_set = 0;
	int aliasfired = 0;

	diff_begin("vpcm_create against the blob's: the root, the runtime block"
		   " and the allocator");

	for (i = 0; i < NTRIAL; i++) {
		const struct cfg *c = &cfg_v[(i / (NID * NQC * 2)) % NCFG];
		int id = id_v[i % NID];
		unsigned char qc = qc_v[(i / NID) % NQC];
		int caller = (i / (NID * NQC)) % 2;
		struct dp_operations *op = &op_dummy[i & 1];
		struct dp *da, *db;
		struct expect e;
		int a_allocs, a_frees, a_live, a_bad;
		unsigned a_bytes;
		int b_allocs, b_frees, b_live, b_bad;
		unsigned b_bytes;
		int nalias = 0, bad;
		int sa = 0, sb = 0, va = 0, vb = 0;
		long tag = i;

		expected(c, id, qc, &e);

		seed_runtime(tag, qc);
		seed_dspinfo(tag);
		memcpy(rt_pre, runtime, sizeof rt_pre);
		memcpy(di_pre, &dspinfo, sizeof di_pre);

		harness_param_set(MDMPRM_MIN_RATE, c->minrate);
		harness_param_set(MDMPRM_MAX_RATE, c->maxrate);
		harness_param_set(MDMPRM_IODELAY, c->iodelay);

		harness_alloc_reset();
		harness_modem_reset(0, 0);

		da = our_ops->create(MODEM, id, caller, V_SRATE, c->max_frag,
				     op);
		a_allocs = harness_alloc.allocs;
		a_frees = harness_alloc.frees;
		a_live = harness_alloc.live;
		a_bytes = harness_alloc.bytes;
		a_bad = harness_alloc.bad_free;
		sa = count_setdelay(&harness_modem_ours, &va);

		diff_eq_int("our vpcm_create returned an object (trial %ld)",
			    da != 0, 1, tag);
		if (da == 0)
			continue;
		memcpy(root_ours, da, ROOT_BYTES);
		memcpy(rt_ours, runtime, sizeof rt_ours);
		if (graph_take(&go, 0, da) < 0) {
			printf("FIXTURE: more than %d live regions\n", MAXLIVE);
			return 1;
		}

		/*
		 * The blob's side sees the runtime block exactly as ours did,
		 * and the `dsp_info` too -- `vpcm_create` does not write the
		 * latter, which is itself asserted below.
		 */
		memcpy(runtime, rt_pre, sizeof rt_pre);
		memcpy(&dspinfo, di_pre, sizeof di_pre);

		db = ref_vpcm_create(MODEM, id, caller, V_SRATE, c->max_frag,
				     op);
		b_allocs = harness_alloc.allocs - a_allocs;
		b_frees = harness_alloc.frees - a_frees;
		b_live = harness_alloc.live - a_live;
		b_bytes = harness_alloc.bytes - a_bytes;
		b_bad = harness_alloc.bad_free - a_bad;
		sb = count_setdelay(&harness_modem_ref, &vb);

		diff_eq_int("the blob's vpcm_create returned an object (trial"
			    " %ld)", db != 0, 1, tag);
		if (db == 0)
			continue;
		if (graph_take(&gb, &go, db) < 0) {
			printf("FIXTURE: more than %d live regions\n", MAXLIVE);
			return 1;
		}

		diff_eq_int("allocations (trial %ld)", a_allocs, b_allocs, tag);
		diff_eq_int("frees inside create (trial %ld)", a_frees, b_frees,
			    tag);
		diff_eq_int("bytes allocated (trial %ld)", (long)a_bytes,
			    (long)b_bytes, tag);
		diff_eq_int("regions still live (trial %ld)", a_live, b_live,
			    tag);
		diff_eq_int("bad frees (trial %ld)", a_bad, b_bad, tag);
		diff_eq_int("and neither side made one (trial %ld)",
			    a_bad + b_bad, 0, tag);
		diff_eq_int("and the graph really is a graph (trial %ld)",
			    go.n > 1 && gb.n > 1, 1, tag);
		diff_eq_int("the two graphs have the same number of regions"
			    " (trial %ld)", go.n, gb.n, tag);

		/* The host's delay report, compared AND asserted absolutely. */
		diff_eq_int("MDMPRM_UPDATE_DELAY calls (trial %ld)", sa, sb, tag);
		diff_eq_int("and it made exactly as many as the delay arithmetic"
			    " requires (trial %ld)", sa, e.nsetdelay, tag);
		if (sa > 0 && sb > 0) {
			diff_eq_int("the delay reported to the host (trial %ld)",
				    va, vb, tag);
			diff_eq_int("and it is 0xf4 - hwDelay (trial %ld)", va,
				    e.setdelay, tag);
		}
		diff_eq_int("set_param calls in total (trial %ld)",
			    harness_modem_ours.nparams,
			    harness_modem_ref.nparams, tag);

		bad = root_compare(root_ours, (const unsigned char *)db,
				   &nalias, tag);
		diff_eq_int("the whole 0xd258 root, word for word (%ld words"
			    " differ outside the twelve)", bad, 0, bad);
		diff_eq_int("all twelve alias words really did differ (trial"
			    " %ld)", nalias, NALIAS, tag);
		diff_eq_int("and every one of them is an alias pair (%ld bad)",
			    check_alias_words(root_ours,
					      (const unsigned char *)db), 0,
			    tag);
		if (nalias == NALIAS)
			aliasfired = 1;
		diff_eq_int("the root reaches the same number of blocks on both"
			    " sides (trial %ld)", go.nass, gb.nass, tag);
		diff_eq_int("and it reaches some (trial %ld)", go.nass > 0, 1,
			    tag);
		{
			int k, sz = 0;

			for (k = 0; k < go.nass && k < gb.nass; k++)
				if (go.sz[go.assigned[k]]
				    != gb.sz[gb.assigned[k]])
					sz++;
			diff_eq_int("and each of them is the same size (%ld"
				    " differ)", sz, 0, tag);
		}

		diff_eq_obj_(__FILE__, __LINE__, "the runtime block",
			     "struct _tagModemParameters", runtime, rt_ours,
			     sizeof *runtime, tag);
		diff_eq_obj_(__FILE__, __LINE__, "the dsp_info block",
			     "struct dsp_info", &dspinfo, di_pre, sizeof dspinfo, tag);

		check_side(root_ours, da,
			   (const struct _tagModemParameters *)(void *)rt_ours,
			   id, &e, op, 2 * tag);
		check_side((const unsigned char *)db, db, runtime, id, &e, op,
			   2 * tag + 1);

		/*
		 * ANTI-VACUITY.  The runtime block was seeded away from every
		 * value `vpcm_create` writes, so a call that wrote nothing
		 * would leave it as the seed; and the roots of two trials must
		 * not be the same 64 bytes, or the sweep is one case repeated.
		 */
		if (memcmp(rt_ours, rt_pre, sizeof rt_pre) != 0)
			changed++;
		if (!first_set) {
			memcpy(first_root, root_ours, ROOT_BYTES);
			first_set = 1;
		} else if (memcmp(first_root, root_ours, ROOT_BYTES) != 0) {
			distinct++;
		}

		/* --- and now tear both graphs down ------------------------ */
		{
			int f0, f1, l1, rc_a, rc_b;

			seed_dspinfo(tag + 1000);
			memcpy(di_pre, &dspinfo, sizeof di_pre);
			/*
			 * `vpcm_delete` reads these two out of the block the
			 * root points at, so both sides must see the same two.
			 * They are DIFFERENT from each other and one of them
			 * is negative, or a swap of the two stores passes.
			 */
			runtime->connectionType = (int)(0x31410000 + tag * 7);
			runtime->clockDeviation = (int)(-0x27180000 - tag * 11);

			f0 = harness_alloc.frees;
			rc_a = our_ops->destroy(da);
			f1 = harness_alloc.frees - f0;
			l1 = harness_alloc.live;
			memcpy(di_ours, &dspinfo, sizeof di_ours);
			memcpy(&dspinfo, di_pre, sizeof di_pre);

			f0 = harness_alloc.frees;
			rc_b = ref_vpcm_delete(db);

			diff_eq_int("vpcm_delete's return value (trial %ld)",
				    rc_a, rc_b, tag);
			diff_eq_int("and it is 0 (trial %ld)", rc_a, 0, tag);
			diff_eq_int("blocks released (trial %ld)", f1,
				    harness_alloc.frees - f0, tag);
			diff_eq_int("and it released the whole graph (trial"
				    " %ld)", f1, go.n, tag);
			diff_eq_int("the blob's graph still stood in between"
				    " (trial %ld)", l1 > 0, 1, tag);
			diff_eq_int("and then nothing is left (trial %ld)",
				    harness_alloc.live, 0, tag);
			diff_eq_int("no bad free (trial %ld)",
				    harness_alloc.bad_free, 0, tag);

			diff_eq_obj_(__FILE__, __LINE__,
				     "the dsp_info after delete", "struct dsp_info",
				     di_ours, &dspinfo, sizeof dspinfo, tag);
			/*
			 * THE TWO WORDS, ABSOLUTELY, and the two the function
			 * must NOT touch.  `connection_type` is the runtime
			 * block's `connectionType` and `clock_deviation` its
			 * `clockDeviation`, in that pairing and not crossed.
			 */
			diff_eq_int("connection_type is connectionType (trial"
				    " %ld)", (long)dspinfo.connection_type,
				    (long)(unsigned)runtime->connectionType,
				    tag);
			diff_eq_int("clock_deviation is clockDeviation (trial"
				    " %ld)", dspinfo.clock_deviation,
				    runtime->clockDeviation, tag);
			diff_eq_int("qc_lapm is untouched (trial %ld)",
				    (long)dspinfo.qc_lapm,
				    (long)(0x11220000u + (unsigned)(tag + 1000)),
				    tag);
			diff_eq_int("qc_index is untouched (trial %ld)",
				    (long)dspinfo.qc_index,
				    (long)(0x33440000u + (unsigned)(tag + 1000)),
				    tag);
		}
	}

	diff_eq_int("some trial changed the runtime block (%ld)", changed > 0,
		    1, changed);
	diff_eq_int("and the root is not the same on every trial (%ld)",
		    distinct > 0, 1, distinct);
	diff_eq_int("and the twelve alias words fired at all", aliasfired, 1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * THE COMPARISON MADE TO FAIL
 * ===========================================================================
 *
 * Without this the whole file could be comparing an image against itself: one
 * flipped byte of our root must be reported, and putting it back must silence
 * it.  t_vpcmcreate.c's `run_made_to_fail`, on this file's comparison.
 */
static int
run_made_to_fail(void)
{
	struct dp *da, *db;
	int nalias = 0, bad;
	unsigned char *p;

	diff_begin("the vpcm_create fixture: the root comparison is live");

	seed_runtime(0, 0x10);
	seed_dspinfo(0);
	memcpy(rt_pre, runtime, sizeof rt_pre);
	harness_param_set(MDMPRM_MIN_RATE, 300);
	harness_param_set(MDMPRM_MAX_RATE, 56000);
	harness_param_set(MDMPRM_IODELAY, 0);
	harness_alloc_reset();
	harness_modem_reset(0, 0);

	da = our_ops->create(MODEM, V_DP_V34, 1, V_SRATE, V_MAX_FRAG,
			     &op_dummy[0]);
	if (da == 0) {
		printf("FIXTURE: our create failed\n");
		return 1;
	}
	memcpy(root_ours, da, ROOT_BYTES);
	graph_take(&go, 0, da);
	memcpy(runtime, rt_pre, sizeof rt_pre);
	db = ref_vpcm_create(MODEM, V_DP_V34, 1, V_SRATE, V_MAX_FRAG,
			     &op_dummy[0]);
	if (db == 0) {
		printf("FIXTURE: the blob's create failed\n");
		return 1;
	}
	graph_take(&gb, &go, db);

	bad = root_compare(root_ours, (const unsigned char *)db, &nalias, -1);
	diff_eq_int("undisturbed, the two roots agree (%ld)", bad, 0, bad);

	printf("    the two `trial -1' lines below are DELIBERATE differences:"
	       " they are this pass proving the comparison fires, not"
	       " failures\n");

	/* One byte of the V.34 object, on our side only. */
	p = root_ours + R_V34 + 0x260;
	*p = (unsigned char)(*p ^ 0xffu);
	bad = root_compare(root_ours, (const unsigned char *)db, &nalias, -1);
	diff_eq_int("one flipped byte is reported (%ld)", bad > 0, 1, bad);
	*p = (unsigned char)(*p ^ 0xffu);

	/*
	 * And a pointer word set to a WRONG heap block, not merely to junk.
	 *
	 * THE GATE IS ASSERTED, not merely obeyed: if the root ever stops
	 * reaching two blocks this sub-check would silently stop running, which
	 * is `extcheck` printing "(none)" through four broken versions.
	 */
	diff_eq_int("the root reaches more than one block, so the wrong-pointer"
		    " check below really runs (%ld)", go.nass > 1, 1, go.nass);
	if (go.nass > 1) {
		unsigned char *q = root_ours + R_XF;
		void *save;

		memcpy(&save, q, sizeof save);
		memcpy(q, &go.p[go.assigned[1]], sizeof save);
		bad = root_compare(root_ours, (const unsigned char *)db,
				   &nalias, -1);
		diff_eq_int("a pointer aimed at the WRONG block is reported"
			    " (%ld)", bad > 0, 1, bad);
		memcpy(q, &save, sizeof save);
	}

	bad = root_compare(root_ours, (const unsigned char *)db, &nalias, -1);
	diff_eq_int("and putting them back silences it (%ld)", bad, 0, bad);

	our_ops->destroy(da);
	ref_vpcm_delete(db);
	diff_eq_int("nothing left allocated", harness_alloc.live, 0, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * THE THREE EARLY RETURNS
 * ===========================================================================
 *
 * `srate != 9600` (`cmp $0x2580,%esi; jne` at 0x3a1c) and `max_frag > 48`
 * (`cmpl $0x30; jg` at 0x3a37) both reach 0x3c7f with %ecx zeroed, and so does
 * a failed `sysdep_malloc`.  The third CANNOT BE DRIVEN HERE and that is
 * recorded rather than papered over: the harness allocator has no failure
 * injection, so nothing in this tree can make `sysdep_malloc` return zero.
 * What the two that can be driven must show is a null return AND that not one
 * byte of heap was taken and not one byte of the runtime block or the
 * `dsp_info` was written -- both guards precede `dp_param_get`.
 */
static int
run_guards(void)
{
	static const struct {
		int		srate;
		int		max_frag;
		const char	*what;
	} guard_v[] = {
		{ 9599, 48, "srate one below 9600" },
		{ 9601, 48, "srate one above 9600" },
		{    0, 48, "srate zero" },
		{   -1, 48, "srate negative" },
		{ 8000, 48, "srate 8000, the other plausible rate" },
		{ 9600, 49, "max_frag one over the cap" },
		{ 9600, 96, "max_frag well over the cap" },
		{ 9600, 0x7fffffff, "max_frag at INT_MAX" }
	};
	int i;

	diff_begin("vpcm_create's two drivable early returns");

	for (i = 0; i < (int)(sizeof guard_v / sizeof guard_v[0]); i++) {
		struct dp *da, *db;
		long tag = i;

		seed_runtime(tag, 0x10);
		seed_dspinfo(tag);
		memcpy(rt_pre, runtime, sizeof rt_pre);
		memcpy(di_pre, &dspinfo, sizeof di_pre);
		harness_param_set(MDMPRM_MIN_RATE, 300);
		harness_param_set(MDMPRM_MAX_RATE, 56000);
		harness_param_set(MDMPRM_IODELAY, 0);
		harness_alloc_reset();
		harness_modem_reset(0, 0);

		da = our_ops->create(MODEM, V_DP_V34, 1, guard_v[i].srate,
				 guard_v[i].max_frag, &op_dummy[0]);
		db = ref_vpcm_create(MODEM, V_DP_V34, 1, guard_v[i].srate,
				     guard_v[i].max_frag, &op_dummy[0]);

		diff_eq_int("the return value (%ld)", da == 0, db == 0, tag);
		diff_eq_int("and it is NULL (%ld)", da == 0 && db == 0, 1, tag);
		diff_eq_int("nothing was allocated (%ld)", harness_alloc.allocs,
			    0, tag);
		diff_eq_int("nothing is live (%ld)", harness_alloc.live, 0, tag);
		diff_eq_int("no byte was freed (%ld)", harness_alloc.frees, 0,
			    tag);
		diff_eq_int("the runtime block is untouched (%ld)",
			    memcmp(runtime, rt_pre, sizeof rt_pre) == 0, 1, tag);
		diff_eq_int("the dsp_info is untouched (%ld)",
			    memcmp(&dspinfo, di_pre, sizeof di_pre) == 0, 1,
			    tag);
		diff_eq_int("and the host was told nothing (%ld)",
			    harness_modem_ours.nparams
			    + harness_modem_ref.nparams, 0, tag);
	}

	printf("    the third early return -- a failed sysdep_malloc -- is NOT"
	       " driven: the harness allocator has no failure injection\n");

	return diff_end();
}

/*
 * ===========================================================================
 * `vpcm_op`, THE DATA OBJECT
 * ===========================================================================
 *
 * .data+0x30, six words: 0x47c into .rodata.str1.1 ("VPCM"), 0, 0x3a00,
 * 0x3dd0, 0x3e40 and 0.  The table and its three entry points are file-static
 * in the object, so ours is taken from `dp_vpcm_init`'s registration and the
 * blob's from the `ref_` aliases; what says `.process` is `vpcm_run` DIRECTLY
 * and not `dp_wrapper_run` is the negative check against `dp_wrapper_run` --
 * the distinction between this datapump and V.23 and Bell 103, which do go
 * through the wrapper.
 *
 * THE TWO `name` POINTERS ARE ONE POINTER, AND THAT IS A LINKER ARTEFACT
 * RATHER THAN A RESULT.  Both objects carry their "VPCM" in a
 * `.rodata.str1.1` marked SHF_MERGE|SHF_STRINGS -- `readelf -SW` prints `AMS`
 * on both build/src/pump/v90/vpcm.o's and build/dsplibs_ref.o's -- so ld folds
 * the two literals into a single address and `vpcm_op.name ==
 * ref_vpcm_op.name` comes out true.
 * That was measured here, not assumed: the first version of this pass asserted
 * the two were DIFFERENT addresses -- t_vpcmcreate.c's alias-word rule -- and
 * failed.  So a plain `strcmp` of the two against each other is vacuous: it
 * compares a string with itself.  What is asserted instead is each side's
 * pointer against the LITERAL "VPCM" separately, which is a real claim about
 * each of the two tables and does not depend on where the bytes live.
 */
static int
run_op_table(void)
{
	diff_begin("vpcm_op against the blob's, field by field");

	diff_eq_int("dp_vpcm_init registered our table (%ld)", find_ops(), 1, 0);
	if (our_ops == 0) {
		return diff_end();
	}

	printf("    the two `name' pointers are %s (%p and %p): the two"
	       " .rodata.str1.1 sections are mergeable and ld folded the"
	       " literals\n",
	       (const void *)our_ops->name == (const void *)ref_vpcm_op.name
	       ? "ONE address" : "two addresses",
	       (const void *)our_ops->name, (const void *)ref_vpcm_op.name);

	diff_eq_int("the name is not null on either side",
		    our_ops->name != 0 && ref_vpcm_op.name != 0, 1, 0);
	if (our_ops->name != 0 && ref_vpcm_op.name != 0) {
		diff_eq_int("ours is the string \"VPCM\" (%ld)",
			    strcmp(our_ops->name, "VPCM"), 0, 0);
		diff_eq_int("and the blob's is too (%ld)",
			    strcmp(ref_vpcm_op.name, "VPCM"), 0, 0);
		diff_eq_int("and it is four characters and a NUL (%ld)",
			    (long)strlen(ref_vpcm_op.name), 4, 0);
	}

	diff_eq_int("use_count (%ld)", our_ops->use_count,
		    ref_vpcm_op.use_count, 0);
	diff_eq_int("and it is zero on both (%ld)",
		    our_ops->use_count | ref_vpcm_op.use_count, 0, 0);
	diff_eq_int("hangup (%ld)", our_ops->hangup == 0,
		    ref_vpcm_op.hangup == 0, 0);
	diff_eq_int("and it is null on both (%ld)",
		    our_ops->hangup == 0 && ref_vpcm_op.hangup == 0, 1, 0);

	/*
	 * `create`, `destroy` and `process` are file-static now, so the
	 * table's own slots are the only handles on them.  What is asserted
	 * is that all three are present, that they are three distinct
	 * functions, and that `process` is NOT `dp_wrapper_run` -- the
	 * distinction between this datapump and V.23 and Bell 103, which do
	 * go through the wrapper and whose comparison this replaces.
	 */
	diff_eq_int("create/destroy/process are all registered (%ld)",
		    our_ops->create != 0 && our_ops->destroy != 0
		    && our_ops->process != 0, 1, 0);
	diff_eq_int("and the blob's is ref_vpcm_create (%ld)",
		    (void *)ref_vpcm_op.create == (void *)&ref_vpcm_create, 1,
		    0);
	diff_eq_int("and the blob's is ref_vpcm_delete (%ld)",
		    (void *)ref_vpcm_op.destroy == (void *)&ref_vpcm_delete, 1,
		    0);
	diff_eq_int("process is NOT dp_wrapper_run, unlike V.23/B103 (%ld)",
		    (void *)our_ops->process != (void *)dp_wrapper_run, 1, 0);
	diff_eq_int("and the blob's is ref_vpcm_run (%ld)",
		    (void *)ref_vpcm_op.process == (void *)&ref_vpcm_run, 1, 0);

	/* The three slots are three DIFFERENT functions on each side. */
	diff_eq_int("the three slots are three distinct functions, ours (%ld)",
		    (void *)our_ops->create != (void *)our_ops->destroy
		    && (void *)our_ops->create != (void *)our_ops->process
		    && (void *)our_ops->destroy != (void *)our_ops->process, 1, 0);
	diff_eq_int("and the blob's (%ld)",
		    (void *)ref_vpcm_op.create != (void *)ref_vpcm_op.destroy
		    && (void *)ref_vpcm_op.create != (void *)ref_vpcm_op.process
		    && (void *)ref_vpcm_op.destroy != (void *)ref_vpcm_op.process,
		    1, 0);

	return diff_end();
}

/*
 * ===========================================================================
 * `dp_vpcm_init`
 * ===========================================================================
 *
 * Three `modem_dp_register` calls and a zero: ONE table under THREE ids, 0x22,
 * 0x5a and 0x5c.  The ids, their ORDER, the count and the identity of the ops
 * pointer are all asserted, on both sides -- a transposed id is invisible to
 * everything downstream except the wrong modulation being selected.
 */
static int
run_dp_init(void)
{
	static const int want[] = { V_DP_V34, V_DP_V90, V_DP_V92 };
	int rep, i;

	diff_begin("dp_vpcm_init against the blob's");

	for (rep = 0; rep < 2; rep++) {
		int rc_a, rc_b;

		harness_reg_reset();
		rc_a = dp_vpcm_init();
		rc_b = ref_dp_vpcm_init();

		diff_eq_int("the return value (%ld)", rc_a, rc_b, rep);
		diff_eq_int("and it is 0 (%ld)", rc_a, 0, rep);
		diff_eq_int("registrations (%ld)", harness_reg_ours.count,
			    harness_reg_ref.count, rep);
		diff_eq_int("and there are three (%ld)", harness_reg_ours.count,
			    3, rep);
		diff_eq_int("nothing was DEregistered (%ld)",
			    harness_reg_ours.deregistered
			    + harness_reg_ref.deregistered, 0, rep);
		if (harness_reg_ours.count != 3 || harness_reg_ref.count != 3)
			continue;
		for (i = 0; i < 3; i++) {
			diff_eq_int("id %ld: the two sides agree",
				    harness_reg_ours.id[i],
				    harness_reg_ref.id[i], i);
			diff_eq_int("and registration %ld is the right id, in"
				    " the right place", harness_reg_ours.id[i],
				    want[i], i);
			diff_eq_int("ours registered the same table (%ld)",
				    harness_reg_ours.ops[i] == (void *)our_ops,
				    1, i);
			diff_eq_int("and the blob's &ref_vpcm_op (%ld)",
				    harness_reg_ref.ops[i]
				    == (void *)&ref_vpcm_op, 1, i);
		}
		diff_eq_int("all three of ours carry ONE table (%ld)",
			    harness_reg_ours.ops[0] == harness_reg_ours.ops[1]
			    && harness_reg_ours.ops[1]
			       == harness_reg_ours.ops[2], 1, rep);
		diff_eq_int("and all three of the blob's do too (%ld)",
			    harness_reg_ref.ops[0] == harness_reg_ref.ops[1]
			    && harness_reg_ref.ops[1]
			       == harness_reg_ref.ops[2], 1, rep);
	}

	return diff_end();
}

/*
 * ===========================================================================
 * THE DIAGNOSTICS, WHICH NO COMPARISON OF MEMORY CAN SEE
 * ===========================================================================
 *
 * `vpcm_create` has four `DSPLIB_DEBUG_ON()` print sites and `vpcm_delete`
 * one, and `main` runs at level 0, so all five are dark in every case above: a
 * wrong format string, a wrong argument or a whole announcement dropped would
 * pass the entire sweep.  Both `dsplibs_debug_level` and its `ref_` twin are
 * raised, or the two sides take different branches for reasons unrelated to
 * the modem.
 *
 * THE GUARD ORDER IS ONLY VISIBLE HERE.  `srate != 9600` returns BEFORE the
 * first print and `max_frag > 48` returns AFTER it, so the sequence (srate
 * guard, "vpcm: create: ...", max_frag guard) is a claim no memory comparison
 * can make -- both guards leave the same nothing behind.  Both are swept.
 *
 * The harness also drops a marker into the transcript for every
 * `modem_get_param`, `modem_set_param` and `modem_debug_log_data` callback, so
 * this pass additionally compares the ORDER and the ARGUMENTS of the parameter
 * traffic, which is where `nsamples` -- stored nowhere in the root -- becomes
 * visible through `VPCMXF_Create`'s own announcement of `maxDataBuffer`.
 */
static int
run_transcripts(void)
{
	static const struct {
		int	id;
		int	caller;
		int	srate;
		int	max_frag;
		int	cfg;
		unsigned char qc;
	} tr_v[] = {
		{ V_DP_V34, 1, V_SRATE, 48, 0,  0x10 },
		{ V_DP_V34, 0, V_SRATE, 48, 0,  0x00 },
		{ V_DP_V90, 1, V_SRATE, 48, 2,  0x10 },
		{ V_DP_V92, 1, V_SRATE, 48, 4,  0x10 },
		{ V_DP_V92, 0, V_SRATE, 48, 5,  0x00 },
		{ 0x37,     1, V_SRATE, 48, 9,  0xff },
		{ V_DP_V34, 1, V_SRATE, 10, 11, 0x2f },
		{ V_DP_V34, 1, V_SRATE, 24, 12, 0xef },
		/* the two guards, and the print that sits between them */
		{ V_DP_V34, 1,    9599, 48, 0,  0x10 },
		{ V_DP_V34, 1,    8000, 48, 0,  0x10 },
		{ V_DP_V34, 1, V_SRATE, 49, 0,  0x10 },
		{ V_DP_V92, 0, V_SRATE, 96, 3,  0x00 }
	};
	int i, printed = 0;
	unsigned maxlen = 0;

	diff_begin("vpcm_create's and vpcm_delete's diagnostics, both sides"
		   " talking");

	for (i = 0; i < (int)(sizeof tr_v / sizeof tr_v[0]); i++) {
		const struct cfg *c = &cfg_v[tr_v[i].cfg];
		struct dp *da, *db;
		long tag = i;
		unsigned la, lb;

		seed_runtime(tag, tr_v[i].qc);
		seed_dspinfo(tag);
		memcpy(rt_pre, runtime, sizeof rt_pre);
		harness_param_set(MDMPRM_MIN_RATE, c->minrate);
		harness_param_set(MDMPRM_MAX_RATE, c->maxrate);
		harness_param_set(MDMPRM_IODELAY, c->iodelay);
		harness_alloc_reset();
		harness_modem_reset(0, 0);

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;

		da = our_ops->create(MODEM, tr_v[i].id, tr_v[i].caller,
				 tr_v[i].srate, tr_v[i].max_frag, &op_dummy[0]);
		if (da != 0)
			our_ops->destroy(da);
		memcpy(runtime, rt_pre, sizeof rt_pre);
		seed_dspinfo(tag);
		db = ref_vpcm_create(MODEM, tr_v[i].id, tr_v[i].caller,
				     tr_v[i].srate, tr_v[i].max_frag,
				     &op_dummy[0]);
		if (db != 0)
			ref_vpcm_delete(db);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		la = (unsigned)strlen(dsplib_debug_capture_text(0));
		lb = (unsigned)strlen(dsplib_debug_capture_text(1));
		if (la > maxlen)
			maxlen = la;
		if (lb > maxlen)
			maxlen = lb;

		diff_eq_int("the transcripts agree (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		diff_eq_int("both sides returned the same thing (%ld)",
			    da == 0, db == 0, tag);
		diff_eq_int("nothing left allocated (%ld)", harness_alloc.live,
			    0, tag);
		if (dsplib_debug_capture_lines(1) > 0)
			printed = 1;
	}

	/*
	 * ANTI-VACUITY, twice over: two empty transcripts compare equal, which
	 * is the state every case in `run_create` is in; and a transcript that
	 * reached the 16 KB capture buffer would be TRUNCATED, so a difference
	 * past the cut would be invisible.
	 */
	diff_eq_int("and some case actually printed something", printed, 1, 0);
	diff_eq_int("no transcript reached the capture buffer's 16383-byte cap"
		    " (%ld)", maxlen < 16383u, 1, (long)maxlen);
	printf("    the longest transcript was %u bytes\n", maxlen);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;

	measure_grain();

	memset(&dspinfo, 0, sizeof dspinfo);
	harness_param_set(MDMPRM_DSPINFO, (long)(size_t)&dspinfo);
	harness_param_set(MDMPRM_CODECTYPE, 4);

	/*
	 * MDMPRM_DSPINFO MUST POINT AT A REAL `struct dsp_info`.  `vpcm_delete`
	 * dereferences it unguarded (0x3de7-0x3df3) and so does
	 * `dp_runtime_create`, so a harness that left the parameter at its
	 * derived default faults here and not where it was set.
	 */
	runtime = (struct _tagModemParameters *)ref_dp_runtime_create(MODEM);
	if (runtime == 0) {
		printf("FIXTURE: dp_runtime_create failed\n");
		return 1;
	}
	memcpy(rt_pristine, runtime, sizeof rt_pristine);
	harness_param_set(MDMPRM_DPRUNTIME, (long)(size_t)runtime);

	{
		void *t = malloc(ROOT_BYTES);
		size_t u = malloc_usable_size(t);

		free(t);
		printf("    the allocator's granularity is %lu bytes; the root"
		       " asks for 0x%x and gets %lu usable, and the runtime"
		       " block asks for %lu and gets %lu, so %lu and %lu"
		       " byte(s) of residue sit past the two objects and are"
		       " not compared\n", (unsigned long)alloc_grain,
		       ROOT_BYTES, (unsigned long)u,
		       (unsigned long)sizeof *runtime,
		       (unsigned long)malloc_usable_size(runtime),
		       (unsigned long)(u - ROOT_BYTES),
		       (unsigned long)(malloc_usable_size(runtime)
				       - sizeof *runtime));
	}

	rc |= run_op_table();
	rc |= run_dp_init();
	rc |= run_guards();
	rc |= run_create();
	rc |= run_made_to_fail();
	rc |= run_transcripts();

	return rc;
}
