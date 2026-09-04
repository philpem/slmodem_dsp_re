/*
 * t_v90p34.cpp -- differential test of `v90Phase34`, `v90RateReneg` and
 * `v90RateRenegSilence`.
 *
 * THREE FUNCTIONS, ONE FIXTURE, AND THAT IS THE POINT.  The two
 * rate-renegotiation transmitters are `v90Phase34`'s own ladder with the Ja
 * and arming arms trimmed and the state numbers moved up: they read and write
 * exactly the same fields of exactly the same object, through the same two
 * emitters, the same `VPcmFloModem` bit source and the same `initdigital`
 * tail.  A second copy of the 250-line fixture below would be 250 lines that
 * have to be kept in step with this one, and `t_v34k56.c` is the cautionary
 * example of what a divergent twin fixture costs.  So `setup()`, `compare()`
 * and the pointer bookkeeping are shared, and only the driving and the
 * per-state accounting are per function.
 *
 * ONE THING THE SHARED FIXTURE HAD TO CHANGE FOR THEM.  `pac3c` used to be
 * ONE buffer for both sides, because `v90Phase34` only reads through it.
 * `v90RateRenegSilence` state 18 WRITES through it -- it clears bit 2 of
 * `pac3c[3]` -- and the write is idempotent, so a shared buffer would compare
 * equal even if only one side ever performed it.  Each side now owns a copy,
 * `+0xac3c` is in the blanked-pointer list, and the two buffers are compared
 * whole.  That is strictly stronger for `v90Phase34` too.
 *
 * One entry point and eleven arms, and -- unlike its K56flex twin -- EVERY
 * ONE OF THEM CAN BE ENTERED.  `t_v34k56.c`'s two completion arms are dead
 * because `K56FlexFloModem`'s bit sources are three-byte stubs; here the
 * sources are `VPcmFloModem::getV90JaBits` and `::getV90CpBits`, both real
 * bodies, so the three completion tails are reachable and are driven.  The
 * counters at the end are "this arm ran", not "this arm could not run".
 *
 * WHAT HAS TO BE SET UP FOR THAT, and it is the whole difference from the
 * K56flex fixture: `p3548` is a REAL `VPcmFloModem` -- one per side, because
 * our side calls our reconstruction of those two members and the reference
 * side calls the blob's.  So each side owns a 32 KB VPcmFloModem block, a
 * V90Demodulator block and a V90ConnectionEvaluator block (`getV90CpBits`
 * writes through `modem.demodulator->connectionEvaluator`), all three seeded
 * identically and all three compared whole.  The wiring is
 * `t_vpcmflomodem.cpp`'s; only the driving is new.
 *
 * `pac3c` is the opposite case.  `v90Phase34` reads ONE byte through it and
 * writes nothing, so both sides get the SAME buffer: there is no address to
 * blank, and a store through it would show up as the two sides disagreeing
 * about a buffer only one of them could have written.
 *
 * POINTERS ELSEWHERE are handled the way `t_v34ec.c` established and
 * `t_v34k56.c` reuses: each field that holds two different addresses by
 * construction is blanked in a COPY of each object, and the copies are what
 * `diff_eq_obj` sees -- which keeps the field naming, the run coalescing and
 * the one-check-per-object count that a hand-written byte loop throws away.
 * `ptr_seen` asserts every entry really did hold two different addresses, so
 * the list cannot quietly go stale into a set of blanked-out fields that were
 * never pointers at all.  Finding F283 is why that check exists.
 *
 * THE FILL IS VARIED, NOT ZERO (finding F230).  Both sides get the same
 * pseudo-random bytes; only the pointer slots and the bulk-delay cursors are
 * seeded afterwards, because `txmit` indexes the ring with those cursors
 * unchecked and a random int there is a crash rather than a test.
 *
 * THE DIAGNOSTICS ARE COMPARED, which `t_v34k56.c` had no need to do.
 * `v90Phase34` has two `dsplibs_debug_printf` sites where the K56flex twin
 * has none, so the sweeps that reach them are run again at level 2 with both
 * sides' transcripts captured and compared -- text and line count.  At that
 * level `getV90CpBits` prints too; that is expected and the checks do not
 * assume the buffer holds only this function's two strings.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"
#include "dsplib/VPcmFloModem.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

int ref_v90Phase34(void *obj);
int ref_v90RateReneg(void *obj);
int ref_v90RateRenegSilence(void *obj);
void ref_txinit(void *obj);
void ref_V34InitializeImplementationSpecific(void *obj);
void ref_V34SetupModulator(void *m, short baud, short carrier,
			   short a, short b, short c);
extern const int ref_vect4[4];
extern const int ref_vect16[16];
}

#define OB_RECEIVER	0x264
#define OB_TXSTATE	0x3596
#define OB_MODULATOR	0x1450
#define OB_BACKCLEAR	0xabfe

#define SHAPED_N	512
#define RING_N		64

/* Slots with slack past the modelled prefix, as t_vpcmflomodem.cpp does. */
#define SLACK		64
#define VPCM_SLOT	(0x7f28 + SLACK)
#define DEM_SLOT	(0x210 + SLACK)
#define SEEN_SLOT	(0x80 + SLACK)
#define CFG_SLOT	(0x60 + SLACK)
#define PCM_SLOT	(0x500 + SLACK)

/*
 * Case 10's tail calls `getMPrecvdBits`, which follows a pointer at
 * `p3548 + 0x610c` to a block it reads two ints out of -- so a varied fill
 * there is a crash and not a test.  It is READ-ONLY on that path, so both
 * sides get the same block, exactly like the configuration at `pac3c`; the
 * gate at `p3548 + 0x6120` stays varied, so both of that function's branches
 * are taken across the sweep.
 */
#define OB_SESS_PCM	0x610c

/*
 * Every pointer field of the V.34 object that the fixture or the function
 * under test can leave holding two different addresses.  The first nine are
 * `t_v34k56.c`'s, derived from what the three setup calls install; `p3548`
 * is this test's own and is the one the K56flex fixture could zero and this
 * one cannot.
 *
 * NOT IN THE LIST, deliberately: +0x20cc is `prefilter.coeff`, pointed at one
 * const table for both sides.  Blanking a slot the two sides agree on would
 * hide a real difference there rather than tolerate an unavoidable one.
 *
 * +0xac3c IS in the list and used not to be.  See the head of the file: the
 * configuration buffer is now one per side, because `v90RateRenegSilence`
 * writes through it and the write is idempotent.  The two buffers are
 * compared in `compare()`; only the ADDRESS is blanked here.
 */
static const struct { unsigned lo, hi; } PTR[] = {
	{ 0x0268, 0x0270 },
	{ 0x2074, 0x2078 },
	{ 0x2220, 0x2228 },
	{ 0x3548, 0x354c },
	{ 0x35b0, 0x35b4 },
	{ 0x80b8, 0x80d8 },
	{ 0x9138, 0x9158 },
	{ OB_MODULATOR + 0x10, OB_MODULATOR + 0x18 },
	{ OB_MODULATOR + 0xc24, OB_MODULATOR + 0xc28 },
	{ OB_MODULATOR + 0xcb0, OB_MODULATOR + 0xcb4 },
	/*
	 * THE LAST FOUR ARE NOT THE FIXTURE'S, they are case 10's.  Its tail
	 * calls `initdigital`, which installs the two shell contexts' `coeff`
	 * and `conv` tables -- and those are CONST TABLES IN THE TWO BUILDS,
	 * so our side stores our address and the reference side stores the
	 * blob's.  There is nothing to compare there but the address, which
	 * is why they are blanked rather than fixed up; that the right table
	 * was chosen is `t_v34shell.c`'s claim and not this test's.
	 *
	 * +0xaa6c is different again and is a SELF-pointer: `getMPrecvdBits`
	 * aims it at +0xaa3c of its own object, so it holds two addresses for
	 * the same reason the two objects have two addresses.
	 */
	{ 0x0a24, 0x0a28 },
	{ 0x2604, 0x2608 },
	{ 0x2608, 0x260c },
	{ 0xaa6c, 0xaa70 },
	{ 0xac3c, 0xac40 }
};
#define NPTR	((int)(sizeof(PTR) / sizeof(PTR[0])))

static int ptr_seen[NPTR];

static struct v34_object oa, ob, ca, cb, snap;
static short shp_a[SHAPED_N], shp_b[SHAPED_N];
static short snap_shp[SHAPED_N], snap_bra[RING_N];
static short bra[RING_N], brb[RING_N];

static unsigned char vp[2][VPCM_SLOT] __attribute__((aligned(8)));
static unsigned char dem[2][DEM_SLOT] __attribute__((aligned(8)));
static unsigned char seen[2][SEEN_SLOT] __attribute__((aligned(8)));
static unsigned char cfgbuf[2][CFG_SLOT] __attribute__((aligned(8)));
static unsigned char pcmbuf[PCM_SLOT] __attribute__((aligned(8)));

/*
 * The two divisor tables `initdigital` indexes, and the reason case 10's
 * tail needs the rate configuration seeded rather than filled.
 *
 * `initdigital` reads `divtab[bits + 14 * use_max - 1]` -- unclamped, and
 * BEFORE the zero-rate test, so a zero rate with mode 0 reads one entry
 * before the table.  A varied fill leaves `divtab` holding a random address
 * and `bits` a random 16-bit index, which is a crash and not a test.  Both
 * tables are pointed at the MIDDLE of a scratch array, on ONE address for
 * both sides, exactly as `t_v34shell.c`'s own `initdigital` sweep does; the
 * index is then bounded by the small `txbits`/`rxbits` seeded below.
 */
static short divstore[64], rxdivstore[64];

/*
 * Which function the current sweep is driving.  Only the labels change; the
 * comparison is the same object either way.
 */
static const char *who = "v90Phase34";

/* Per-block "this really ran" counters; every one of them must fire. */
static long n_ja, n_ja_tail, n_ja_clear, n_ja_noclear;
static long n_arm_low, n_arm_high;
static long n_s3, n_s3_adv, n_s3_dbg, n_s6, n_s6_adv;
static long n_s4, n_s4_adv, n_s7, n_s7_adv;
static long n_s5_16, n_s5_4, n_s5_zero;
static long n_s8, n_s8_adv, n_s9_16, n_s9_4, n_s10, n_s10_adv;
static long n_noarm, n_dbg_lines;

/*
 * The rate-renegotiation ladders' own counters.  `r11`..`r14` are
 * `v90RateReneg`'s four states and `s15`..`s20` are the silence one's six;
 * `_adv` counts the calls that moved the state on, which for four of the ten
 * is a different call from the one that entered the state.
 */
static long n_r11, n_r11_adv, n_r12, n_r12_adv, n_r13, n_r14, n_r14_adv;
static long n_rnone;
static long n_s15, n_s15_adv, n_s16, n_s16_adv, n_s17, n_s18, n_s18_adv;
static long n_s19_16, n_s19_4, n_s20, n_s20_adv, n_snone;
static long n_s18_dbg, n_s18_sas, n_s20_freeze;

static long tag;

static VPcmFloModem *
M(int side)
{
	return (VPcmFloModem *)vp[side];
}

static void
fill_varied(void *p, size_t n, unsigned s)
{
	unsigned char *b = (unsigned char *)p;
	size_t i;

	for (i = 0; i < n; i++) {
		s = s * 1103515245u + 12345u;
		b[i] = (unsigned char)(s >> 16);
	}
}

static void
blank_pointers(struct v34_object *o)
{
	int i;

	for (i = 0; i < NPTR; i++)
		memset((char *)o + PTR[i].lo, 0, PTR[i].hi - PTR[i].lo);
}

/*
 * The two blocks that hold an address the two sides cannot agree on, copied
 * with each pointer replaced by whether it still points where setup() put it.
 * Same idiom as snap_vp/snap_dem in t_vpcmflomodem.cpp.
 */
static void
snap_vp(unsigned char *dst, int side)
{
	VPcmFloModem *s;

	memcpy(dst, vp[side], VPCM_SLOT);
	s = (VPcmFloModem *)dst;
	s->modem.demodulator = (V90Demodulator *)(long)
	    (M(side)->modem.demodulator == (V90Demodulator *)dem[side]);
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

/*
 * How the two bit sources are configured for a sweep.  `getV90JaBits` reads
 * the first three; `getV90CpBits` reads the rest and `nofBits` as well, which
 * is the vector it copies from when CP gives way to CPnot.
 */
struct vpcm_cfg {
	short		nofBits;
	unsigned short	bitPointer;
	unsigned char	terminateJa;
	short		cpNofBits;
	unsigned char	nofBitsPerSymbol;
	unsigned char	terminateCp;
	unsigned char	terminateCpNot;
	unsigned char	cpNotLoaded;
	unsigned short	nofTransmitSequences;
	unsigned short	minNofTransmitSequences;
};

/* Neither source ever reports termination; the sequences just run. */
static const struct vpcm_cfg CFG_RUN = { 8, 0, 0, 32, 4, 0, 0, 0, 0, 0 };
/* Ja terminates, with bits to send and without. */
static const struct vpcm_cfg CFG_JA  = { 8, 0, 1, 32, 4, 0, 0, 0, 0, 0 };
static const struct vpcm_cfg CFG_JA0 = { 0, 3, 1, 32, 4, 0, 0, 0, 0, 0 };
/* CP terminates on the call that lands the pointer exactly on cpNofBits. */
static const struct vpcm_cfg CFG_CP  = { 11, 12, 0, 16, 4, 0, 1, 0, 9, 3 };
/* CP reaches the end of a sequence and loads the CPnot vector instead. */
static const struct vpcm_cfg CFG_LD  = { 11, 12, 0, 16, 4, 1, 0, 0, 9, 3 };

/*
 * Two V.34 objects and two VPcmFloModems, identically seeded, differing only
 * in which blocks they own.
 *
 * `dataflag` and `armed` are the two bits of the receiver's flags word that
 * gate the dispatch; `state` goes into `v90_receiver`, `constel` into `short_382`,
 * `symcnt` into `seg_symcount` -- which is what the two symbol counts are compared
 * against -- and `tx_flags` carries the scrambler generator in bit 0 and
 * `txmit`'s echo feed in bit 9, so it is swept over both.  `backclear` is bit
 * 2 of the configuration byte the Ja completion tail tests.
 */
static void
setup(int dataflag, int armed, int state, short constel, short symcnt,
      short tx_flags, const struct vpcm_cfg *cfg, int backclear, unsigned seed)
{
	struct v34_receiver *ra, *rb;
	int i, side;

	memset(&oa, 0, sizeof(oa));
	memset(&ob, 0, sizeof(ob));
	fill_varied(&oa, sizeof(oa), seed);
	memcpy(&ob, &oa, sizeof(ob));
	blank_pointers(&oa);
	blank_pointers(&ob);

	fill_varied(vp[0], VPCM_SLOT, seed ^ 0x5a5au);
	memcpy(vp[1], vp[0], VPCM_SLOT);
	fill_varied(dem[0], DEM_SLOT, seed ^ 0x3c3cu);
	memcpy(dem[1], dem[0], DEM_SLOT);
	fill_varied(seen[0], SEEN_SLOT, seed ^ 0x1e1eu);
	memcpy(seen[1], seen[0], SEEN_SLOT);
	fill_varied(cfgbuf[0], CFG_SLOT, seed ^ 0x0f0fu);
	cfgbuf[0][0x50] = (unsigned char)((cfgbuf[0][0x50] & ~0x04) |
					  (backclear ? 0x04 : 0));
	memcpy(cfgbuf[1], cfgbuf[0], CFG_SLOT);

	fill_varied(pcmbuf, sizeof(pcmbuf), seed ^ 0x2718u);


	for (side = 0; side < 2; side++) {
		VPcmFloModem *m = M(side);

		m->modem.demodulator = (V90Demodulator *)dem[side];
		*(unsigned char **)(vp[side] + OB_SESS_PCM) = pcmbuf;
		((V90Demodulator *)dem[side])->connectionEvaluator =
		    (V90ConnectionEvaluator *)seen[side];

		m->nofBits = cfg->nofBits;
		m->bitPointer = cfg->bitPointer;
		m->terminateJa = cfg->terminateJa;
		m->cpNofBits = cfg->cpNofBits;
		m->nofBitsPerSymbol = cfg->nofBitsPerSymbol;
		m->terminateCp = cfg->terminateCp;
		m->terminateCpNot = cfg->terminateCpNot;
		m->cpNotLoaded = cfg->cpNotLoaded;
		m->nofTransmitSequences = cfg->nofTransmitSequences;
		m->minNofTransmitSequences = cfg->minNofTransmitSequences;

		/*
		 * The live parts of the two vectors, as bits.  Everything
		 * outside them stays varied: only these entries are read, and
		 * a vector of arbitrary shorts would still be safe -- both
		 * emitters go through `V34scrambler` with `nbits == 2`, which
		 * masks -- but it would not be a bit vector.
		 */
		for (i = 0; i < 64; i++) {
			m->bitVector[i] = (short)((seed >> (i & 15)) & 1);
			m->cpBitVector[i] = (short)((i * 7 + (int)seed) & 1);
		}
	}

	fill_varied(shp_a, sizeof(shp_a), seed + 1);
	memcpy(shp_b, shp_a, sizeof(shp_b));
	fill_varied(bra, sizeof(bra), seed + 2);
	memcpy(brb, bra, sizeof(brb));

	V34InitializeImplementationSpecific(&oa);
	ref_V34InitializeImplementationSpecific(&ob);
	txinit(&oa);
	ref_txinit(&ob);

	((struct v34_modulator *)((char *)&oa + OB_MODULATOR))->shaped = shp_a;
	((struct v34_modulator *)((char *)&ob + OB_MODULATOR))->shaped = shp_b;
	V34SetupModulator((struct v34_modulator *)((char *)&oa + OB_MODULATOR),
			  2400, 1600, 0, 0, 1);
	ref_V34SetupModulator((char *)&ob + OB_MODULATOR, 2400, 1600, 0, 0, 1);
	{
		struct v34_ratecfg *ga = (struct v34_ratecfg *)
		    ((char *)&oa + V34_RATECFG);
		struct v34_ratecfg *gb = (struct v34_ratecfg *)
		    ((char *)&ob + V34_RATECFG);
		static const short bauds[4] = { 2400, 2743, 3200, 3429 };
		int k;

		for (k = 0; k < 64; k++) {
			divstore[k] = (short)(k * 37 - 300);
			rxdivstore[k] = (short)(k * 53 - 400);
		}
		ga->baud = gb->baud = bauds[seed & 3];
		ga->rx_baud = gb->rx_baud = bauds[(seed >> 2) & 3];
		ga->txbits = gb->txbits = (short)((seed >> 4) & 0xf);
		ga->rxbits = gb->rxbits = (short)((seed >> 8) & 0xf);
		ga->use_max = gb->use_max = (short)((seed >> 12) & 1);
		ga->rx_use_max = gb->rx_use_max = (short)((seed >> 13) & 1);
		ga->depth = gb->depth = 0;
		ga->divtab = gb->divtab = &divstore[16];
		ga->rx_divtab = gb->rx_divtab = &rxdivstore[16];
		/* Printed by case 3's diagnostic, and by nothing else here. */
		ga->period = gb->period = 0x2a1;
	}

	oa.prefilter.coeff = ob.prefilter.coeff = V34TimingPrefilterCoeff;
	oa.prefilter.shift = ob.prefilter.shift = 14;
	oa.tx_scale = ob.tx_scale = 0x4000;
	/*
	 * The bulk-delay ring and its two cursors -- `txmit` indexes it with
	 * them unchecked, so these three are seeded rather than filled.  That
	 * is the fill's boundary, not an exception to it.
	 */
	oa.bulk_ring = bra;  ob.bulk_ring = brb;
	oa.bulk_len  = ob.bulk_len = RING_N;
	oa.bulk_head = ob.bulk_head = 3;
	oa.bulk_tail = ob.bulk_tail = 17;

	/* A scrambler register that is neither zero nor all ones. */
	oa.tx_scr_sr = ob.tx_scr_sr = 0x2f6b3d51;
	oa.prev_quadrant = ob.prev_quadrant = 2;
	oa.cur_quadrant = ob.cur_quadrant = 1;
	oa.seg_symcount = ob.seg_symcount = symcnt;
	oa.tx_flags = ob.tx_flags = tx_flags;

	oa.p3548 = vp[0];    ob.p3548 = vp[1];
	oa.pac3c = cfgbuf[0]; ob.pac3c = cfgbuf[1];
	oa.pac18 = ob.pac18 = 0;

	oa.v90_receiver = ob.v90_receiver = state;
	oa.k56flex_receiver = ob.k56flex_receiver = 0;
	oa.short_382 = ob.short_382 = constel;
	*(short *)((char *)&oa + OB_TXSTATE) = 20;
	*(short *)((char *)&ob + OB_TXSTATE) = 20;
	((unsigned char *)&oa)[OB_BACKCLEAR] = 0;
	((unsigned char *)&ob)[OB_BACKCLEAR] = 0;

	ra = (struct v34_receiver *)((char *)&oa + OB_RECEIVER);
	rb = (struct v34_receiver *)((char *)&ob + OB_RECEIVER);
	ra->flags = (unsigned short)
	    ((0x0123 & ~(V34_RX_FLAG_DATA | V34_RX_FLAG_TRN_WATCH))
	     | (dataflag ? V34_RX_FLAG_DATA : 0)
	     | (armed ? V34_RX_FLAG_TRN_WATCH : 0));
	rb->flags = ra->flags;
}

static int
first_diff_short(const short *a, const short *b, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return i;
	return -1;
}

static void
compare(long t)
{
	static unsigned char sa[VPCM_SLOT], sb[VPCM_SLOT];

	int i;

	/*
	 * Which slots actually hold two different addresses, checked HERE and
	 * not in setup(): four of them are installed by the function under
	 * test rather than by the fixture, so a scan that ran before the call
	 * could never see them.  A skip list is the one part of a
	 * differential test that silently gets weaker as it grows (finding
	 * F283), and this is the check that keeps every entry load-bearing.
	 */
	for (i = 0; i < NPTR; i++) {
		if (memcmp((char *)&oa + PTR[i].lo, (char *)&ob + PTR[i].lo,
			   PTR[i].hi - PTR[i].lo) != 0)
			ptr_seen[i] = 1;
	}

	memcpy(&ca, &oa, sizeof(ca));
	memcpy(&cb, &ob, sizeof(cb));
	blank_pointers(&ca);
	blank_pointers(&cb);
	diff_eq_obj(who, struct v34_object, &ca, &cb, t);
	diff_eq_int("shaped %ld", first_diff_short(shp_a, shp_b, SHAPED_N),
		    -1, t);
	diff_eq_int("bulk ring %ld", first_diff_short(bra, brb, RING_N),
		    -1, t);

	snap_vp(sa, 0);
	snap_vp(sb, 1);
	diff_eq_obj_(__FILE__, __LINE__, who, "VPcmFloModem block",
		     sa, sb, VPCM_SLOT, t);
	snap_dem(sa, 0);
	snap_dem(sb, 1);
	diff_eq_obj_(__FILE__, __LINE__, who, "V90Demodulator block",
		     sa, sb, DEM_SLOT, t);
	diff_eq_obj_(__FILE__, __LINE__, who, "evaluator block",
		     seen[0], seen[1], SEEN_SLOT, t);
	/*
	 * The configuration buffers.  One per side since the silence ladder
	 * writes through them; see the head of the file.
	 */
	diff_eq_obj_(__FILE__, __LINE__, who, "configuration block",
		     cfgbuf[0], cfgbuf[1], CFG_SLOT, t);

	diff_eq_int("transcript text %ld",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, t);
	diff_eq_int("transcript lines %ld",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), t);
	n_dbg_lines += (long)dsplib_debug_capture_lines(0);
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
}

/*
 * Case 5's three claims, checked against our side alone so that they hold
 * whatever the blob does:
 *
 *   - `prev_quadrant` is untouched.  Both published emitters write it.
 *   - `cur_quadrant` is the scrambler's raw two bits, 0..3.
 *   - the point is `vect4[cur_quadrant]` exactly, or one of the four `vect16`
 *     entries of quadrant `cur_quadrant` -- NOT `vect4[(d + prev_quadrant) & 3]`.
 *
 * and, for the third arm, that a constellation code which is neither of the
 * two named values transmits the ZERO point without touching the scrambler
 * at all.  That arm is the K56flex twin's `else`, and it is not this one's.
 */
static void
check_idle(short constel, short c6_before, short c8_before, int cc_before,
	   long t)
{
	int q = oa.cur_quadrant;
	int pt = oa.txpoint.word;

	diff_eq_int("idle leaves prev_quadrant alone %ld", oa.prev_quadrant, c6_before, t);
	if (constel != (short)0x89b0 && constel != (short)0x8990) {
		diff_eq_int("no-constel point re %ld", oa.txpoint.c[0], 0, t);
		diff_eq_int("no-constel point im %ld", oa.txpoint.c[1], 0, t);
		diff_eq_int("no-constel leaves cur_quadrant %ld", oa.cur_quadrant,
			    c8_before, t);
		diff_eq_int("no-constel leaves the scrambler %ld", oa.tx_scr_sr,
			    cc_before, t);
		return;
	}
	diff_eq_int("idle quadrant in range %ld", q >= 0 && q <= 3, 1, t);
	if (q < 0 || q > 3)
		return;
	if (constel == (short)0x89b0) {
		int i, hit = 0;

		for (i = 0; i < 4; i++)
			if (pt == vect16[q * 4 + i])
				hit = 1;
		diff_eq_int("idle point is vect16 of cur_quadrant %ld", hit, 1, t);
	} else {
		diff_eq_int("idle point is vect4[cur_quadrant] %ld", pt, vect4[q], t);
	}
}

/*
 * One call on each side, compared.  Returns nothing: every check it makes is
 * a `diff_eq_*`, and the per-arm bookkeeping is the caller's.
 */
static void
step(void)
{
	int r, rr;

	dsplib_debug_capture_reset();
	r  = v90Phase34(&oa);
	rr = ref_v90Phase34(&ob);
	diff_eq_int("return %ld", r, rr, tag);
	diff_eq_int("returns 0 %ld", r, 0, tag);
	compare(tag);
}

/* The per-arm bookkeeping, shared by every sweep that steps the dispatch. */
static void
account(int df, int armed, int before_state, short constel, short before_c6,
	short before_c8, int before_cc, unsigned short before_flags)
{
	struct v34_receiver *ra =
	    (struct v34_receiver *)((char *)&oa + OB_RECEIVER);

	if (!df) {
		n_ja++;
		if (ra->flags & V34_RX_FLAG_DATA) {
			n_ja_tail++;
			if (((const unsigned char *)oa.pac3c)[0x50] & 0x04) {
				n_ja_clear++;
				diff_eq_int("backward clear set %ld",
					    ((unsigned char *)&oa)[OB_BACKCLEAR],
					    1, tag);
			} else {
				n_ja_noclear++;
				diff_eq_int("backward clear left %ld",
					    ((unsigned char *)&oa)[OB_BACKCLEAR],
					    0, tag);
			}
		}
		diff_eq_int("Ja leaves the state %ld", oa.v90_receiver,
			    before_state, tag);
		return;
	}
	if (!armed) {
		if (before_state <= 2) {
			n_arm_low++;
			diff_eq_int("low state does not arm %ld",
				    (long)(ra->flags & V34_RX_FLAG_TRN_WATCH),
				    0, tag);
			diff_eq_int("low state leaves the flags %ld",
				    (long)ra->flags, (long)before_flags, tag);
		} else {
			n_arm_high++;
			diff_eq_int("high state arms %ld",
				    (long)(ra->flags & V34_RX_FLAG_TRN_WATCH)
				    != 0, 1, tag);
			diff_eq_int("arming zeroes seg_symcount %ld", oa.seg_symcount, 0,
				    tag);
		}
		diff_eq_int("the arming arm leaves the state %ld",
			    oa.v90_receiver, before_state, tag);
		return;
	}

	switch (before_state) {
	case 3:
		n_s3++;
		if (oa.v90_receiver != 3) {
			n_s3_adv++;
			diff_eq_int("case 3 advances to 4 %ld",
				    oa.v90_receiver, 4, tag);
			diff_eq_int("case 3 zeroes seg_symcount %ld", oa.seg_symcount, 0,
				    tag);
		}
		break;
	case 6:
		n_s6++;
		if (oa.v90_receiver != 6) {
			n_s6_adv++;
			diff_eq_int("case 6 advances to 7 %ld",
				    oa.v90_receiver, 7, tag);
			diff_eq_int("case 6 zeroes seg_symcount %ld", oa.seg_symcount, 0,
				    tag);
		}
		break;
	case 4:
		n_s4++;
		if (oa.v90_receiver != 4) {
			n_s4_adv++;
			diff_eq_int("case 4 advances to 5 %ld",
				    oa.v90_receiver, 5, tag);
			diff_eq_int("case 4 zeroes prev_quadrant %ld", oa.prev_quadrant, 0,
				    tag);
			diff_eq_int("case 4 zeroes seg_symcount %ld", oa.seg_symcount, 0,
				    tag);
			diff_eq_int("case 4 zeroes tx_scr_sr %ld", oa.tx_scr_sr, 0,
				    tag);
		}
		break;
	case 7:
		n_s7++;
		if (oa.v90_receiver != 7) {
			n_s7_adv++;
			diff_eq_int("case 7 advances to 8 %ld",
				    oa.v90_receiver, 8, tag);
			diff_eq_int("case 7 zeroes prev_quadrant %ld", oa.prev_quadrant, 0,
				    tag);
			diff_eq_int("case 7 zeroes seg_symcount %ld", oa.seg_symcount, 0,
				    tag);
			diff_eq_int("case 7 zeroes tx_scr_sr %ld", oa.tx_scr_sr, 0,
				    tag);
		}
		break;
	case 5:
		if (constel == (short)0x89b0)
			n_s5_16++;
		else if (constel == (short)0x8990)
			n_s5_4++;
		else
			n_s5_zero++;
		check_idle(constel, before_c6, before_c8, before_cc, tag);
		diff_eq_int("case 5 leaves the state %ld", oa.v90_receiver, 5,
			    tag);
		break;
	case 8:
		n_s8++;
		if (oa.v90_receiver != 8) {
			n_s8_adv++;
			diff_eq_int("case 8 advances to 9 %ld",
				    oa.v90_receiver, 9, tag);
		}
		break;
	case 9:
		if (constel == (short)0x89b0)
			n_s9_16++;
		else
			n_s9_4++;
		diff_eq_int("case 9 leaves the state %ld", oa.v90_receiver, 9,
			    tag);
		break;
	case 10:
		n_s10++;
		if (oa.v90_receiver != 10) {
			n_s10_adv++;
			diff_eq_int("case 10 hands over %ld", oa.v90_receiver,
				    2, tag);
			diff_eq_int("case 10 sets the transmit state %ld",
				    *(short *)((char *)&oa + OB_TXSTATE),
				    V34HS_EXMIT, tag);
		}
		break;
	default:
		/*
		 * No arm.  Nothing may move at all, which is a stronger
		 * statement than the differential makes on its own: both
		 * sides doing the same wrong thing would still agree.
		 */
		n_noarm++;
		diff_eq_int("no arm moves no byte %ld",
			    memcmp(&oa, &snap, sizeof(oa)), 0, tag);
		diff_eq_int("no arm writes the shaped buffer %ld",
			    first_diff_short(shp_a, snap_shp, SHAPED_N), -1,
			    tag);
		diff_eq_int("no arm writes the bulk ring %ld",
			    first_diff_short(bra, snap_bra, RING_N), -1, tag);
		break;
	}
}

/*
 * Step the dispatch `iters` times from one setup, accounting for each.
 *
 * THE TWO GATE BITS AND THE STATE ARE RE-READ BEFORE EVERY CALL, not taken
 * from the setup: four arms move the state and two set a gate bit, so which
 * arm the SECOND call takes is generally not the one the first did.  Reading
 * them from the setup is how the arming arm's checks came to be applied to
 * case 3.
 */
static void
run(short constel, int iters)
{
	int it;

	for (it = 0; it < iters; it++) {
		struct v34_receiver *ra =
		    (struct v34_receiver *)((char *)&oa + OB_RECEIVER);
		unsigned short before_flags = ra->flags;
		int df = (before_flags & V34_RX_FLAG_DATA) != 0;
		int armed = (before_flags & V34_RX_FLAG_TRN_WATCH) != 0;
		int before_state = oa.v90_receiver;
		short before_c6 = oa.prev_quadrant, before_c8 = oa.cur_quadrant;
		int before_cc = oa.tx_scr_sr;

		memcpy(&snap, &oa, sizeof(snap));
		memcpy(snap_shp, shp_a, sizeof(snap_shp));
		memcpy(snap_bra, bra, sizeof(snap_bra));
		step();
		account(df, armed, before_state, constel, before_c6,
			before_c8, before_cc, before_flags);
		tag++;
	}
}

/*
 * ===========================================================================
 * The two rate-renegotiation ladders
 * ===========================================================================
 *
 * Same object, same fixture, same comparison; what is new is the dispatch and
 * the two states `v90Phase34` has no counterpart for -- 18's completion tail,
 * which clears the echo-canceller freeze and the SAS detector instead of
 * handing the handshake over, and 20's, which puts the freeze back before
 * every symbol.
 */

/* Bit 2 of `pac3c[3]`; see v34pcmmain.cpp for why the byte is offset-named. */
#define CFG_FLAGS03	0x03
#define CFG_SAS_DETECT	0x04

static void
set_sas(int on)
{
	int side;

	for (side = 0; side < 2; side++)
		cfgbuf[side][CFG_FLAGS03] = (unsigned char)
		    ((cfgbuf[side][CFG_FLAGS03] & ~CFG_SAS_DETECT)
		     | (on ? CFG_SAS_DETECT : 0));
}

static void
step_rrn(int silence)
{
	int r, rr;

	dsplib_debug_capture_reset();
	if (silence) {
		r  = v90RateRenegSilence(&oa);
		rr = ref_v90RateRenegSilence(&ob);
	} else {
		r  = v90RateReneg(&oa);
		rr = ref_v90RateReneg(&ob);
	}
	diff_eq_int("return %ld", r, rr, tag);
	diff_eq_int("returns 0 %ld", r, 0, tag);
	compare(tag);
}

/*
 * State 19's idle symbol, checked against our side alone -- the same three
 * claims `check_idle` makes for case 5, with the two that differ stated as
 * differences:
 *
 *   - there is NO third arm.  Anything that is not 0x89b0 is the four-point
 *     arm, where case 5 gives 0x8990 an arm of its own and everything else
 *     the zero point.
 *   - `prev_quadrant` IS written, from `cur_quadrant`, where case 5 leaves it alone.
 */
static void
check_idle_rrn(short constel, int cc_before, long t)
{
	int q = oa.cur_quadrant;
	int pt = oa.txpoint.word;

	diff_eq_int("silence idle advances the quadrant %ld", oa.prev_quadrant,
		    oa.cur_quadrant, t);
	diff_eq_int("silence idle clocks the scrambler %ld",
		    oa.tx_scr_sr != cc_before, 1, t);
	diff_eq_int("silence idle quadrant in range %ld",
		    q >= 0 && q <= 3, 1, t);
	if (q < 0 || q > 3)
		return;
	if (constel == (short)0x89b0) {
		int i, hit = 0;

		for (i = 0; i < 4; i++)
			if (pt == vect16[q * 4 + i])
				hit = 1;
		diff_eq_int("silence idle point is vect16 of cur_quadrant %ld", hit,
			    1, t);
	} else {
		diff_eq_int("silence idle point is vect4[cur_quadrant] %ld", pt,
			    vect4[q], t);
	}
}

static void
account_rrn(int silence, int before_state, short constel, short before_c6,
	    int before_cc, short before_f25c2, int before_sas)
{
	const unsigned char *cfg = (const unsigned char *)oa.pac3c;

	(void)before_c6;
	if (!silence) {
		switch (before_state) {
		case 11:
			n_r11++;
			if (oa.v90_receiver != 11) {
				n_r11_adv++;
				diff_eq_int("state 11 advances to 12 %ld",
					    oa.v90_receiver, 12, tag);
				diff_eq_int("state 11 zeroes seg_symcount %ld",
					    oa.seg_symcount, 0, tag);
			}
			break;
		case 12:
			n_r12++;
			if (oa.v90_receiver != 12) {
				n_r12_adv++;
				diff_eq_int("state 12 advances to 13 %ld",
					    oa.v90_receiver, 13, tag);
				diff_eq_int("state 12 zeroes prev_quadrant %ld",
					    oa.prev_quadrant, 0, tag);
				diff_eq_int("state 12 zeroes seg_symcount %ld",
					    oa.seg_symcount, 0, tag);
				diff_eq_int("state 12 zeroes tx_scr_sr %ld",
					    oa.tx_scr_sr, 0, tag);
			}
			break;
		case 13:
			n_r13++;
			diff_eq_int("state 13 stays %ld", oa.v90_receiver, 13,
				    tag);
			break;
		case 14:
			n_r14++;
			if (oa.v90_receiver != 14) {
				n_r14_adv++;
				diff_eq_int("state 14 hands over %ld",
					    oa.v90_receiver, 2, tag);
				diff_eq_int("state 14 sets the transmit "
					    "state %ld",
					    *(short *)((char *)&oa
						       + OB_TXSTATE),
					    V34HS_EXMIT, tag);
			}
			break;
		default:
			n_rnone++;
			diff_eq_int("no reneg arm moves no byte %ld",
				    memcmp(&oa, &snap, sizeof(oa)), 0, tag);
			diff_eq_int("no reneg arm writes the shaped buffer "
				    "%ld",
				    first_diff_short(shp_a, snap_shp,
						     SHAPED_N), -1, tag);
			break;
		}
		return;
	}

	switch (before_state) {
	case 15:
		n_s15++;
		if (oa.v90_receiver != 15) {
			n_s15_adv++;
			diff_eq_int("state 15 advances to 16 %ld",
				    oa.v90_receiver, 16, tag);
			diff_eq_int("state 15 zeroes seg_symcount %ld", oa.seg_symcount, 0,
				    tag);
		}
		break;
	case 16:
		n_s16++;
		if (oa.v90_receiver != 16) {
			n_s16_adv++;
			diff_eq_int("state 16 advances to 17 %ld",
				    oa.v90_receiver, 17, tag);
			diff_eq_int("state 16 zeroes prev_quadrant %ld", oa.prev_quadrant, 0,
				    tag);
			diff_eq_int("state 16 zeroes seg_symcount %ld", oa.seg_symcount, 0,
				    tag);
			diff_eq_int("state 16 zeroes tx_scr_sr %ld", oa.tx_scr_sr, 0,
				    tag);
		}
		break;
	case 17:
		n_s17++;
		diff_eq_int("state 17 stays %ld", oa.v90_receiver, 17, tag);
		break;
	case 18:
		n_s18++;
		if (oa.v90_receiver != 18) {
			n_s18_adv++;
			diff_eq_int("state 18 goes to the idle state %ld",
				    oa.v90_receiver, 19, tag);
			diff_eq_int("state 18 lifts the freeze %ld",
				    (long)(oa.tx_flags & V34_EC_FROZEN), 0, tag);
			diff_eq_int("state 18 leaves the rest of tx_flags %ld",
				    (long)(unsigned short)
				    (oa.tx_flags ^ before_f25c2),
				    (long)(before_f25c2 & V34_EC_FROZEN), tag);
			diff_eq_int("state 18 clears the SAS detector %ld",
				    (long)(cfg[CFG_FLAGS03] & CFG_SAS_DETECT),
				    0, tag);
			if (before_sas)
				n_s18_sas++;
			/*
			 * THE WITNESS IS THE TEXT AND NOT THE LINE COUNT.  At
			 * level 2 `getV90CpBits` prints as well, so
			 * `lines > 0` is satisfied whether or not this arm's
			 * own site fired -- which is the counter that proves
			 * nothing that findings F3509 and F3403 are about.  This
			 * message goes out through `dsplibs_debug_printf` and
			 * not `edprintf`, so it is in the capture in plain
			 * text and can be looked for.
			 */
			if (strstr(dsplib_debug_capture_text(0),
				   "move to SCR on silence rrn") != 0)
				n_s18_dbg++;
		} else {
			/*
			 * NOT ADVANCING MUST LEAVE BOTH ALONE.  Both stores
			 * are inside the completion tail, so a reconstruction
			 * that hoisted either to the top of the arm would
			 * agree with the blob on every advancing call and
			 * disagree here.
			 */
			diff_eq_int("state 18 leaves tx_flags alone %ld",
				    (long)(unsigned short)oa.tx_flags,
				    (long)(unsigned short)before_f25c2, tag);
			diff_eq_int("state 18 leaves the SAS detector %ld",
				    (long)(cfg[CFG_FLAGS03] & CFG_SAS_DETECT)
				    != 0, before_sas != 0, tag);
		}
		break;
	case 19:
		if (constel == (short)0x89b0)
			n_s19_16++;
		else
			n_s19_4++;
		diff_eq_int("state 19 stays %ld", oa.v90_receiver, 19, tag);
		check_idle_rrn(constel, before_cc, tag);
		break;
	case 20:
		n_s20++;
		/*
		 * THE FREEZE GOES ON EVERY TIME, advancing or not: the store
		 * is the first statement of the arm and not part of the
		 * completion tail, which is the mirror image of state 18's
		 * check above.
		 */
		diff_eq_int("state 20 sets the freeze %ld",
			    (long)(oa.tx_flags & V34_EC_FROZEN) != 0, 1, tag);
		if ((before_f25c2 & V34_EC_FROZEN) == 0)
			n_s20_freeze++;
		if (oa.v90_receiver != 20) {
			n_s20_adv++;
			diff_eq_int("state 20 hands over %ld",
				    oa.v90_receiver, 2, tag);
			diff_eq_int("state 20 sets the transmit state %ld",
				    *(short *)((char *)&oa + OB_TXSTATE),
				    V34HS_EXMIT, tag);
		}
		break;
	default:
		n_snone++;
		diff_eq_int("no silence arm moves no byte %ld",
			    memcmp(&oa, &snap, sizeof(oa)), 0, tag);
		diff_eq_int("no silence arm writes the shaped buffer %ld",
			    first_diff_short(shp_a, snap_shp, SHAPED_N), -1,
			    tag);
		break;
	}
}

static void
run_rrn(int silence, short constel, int iters)
{
	int it;

	for (it = 0; it < iters; it++) {
		int before_state = oa.v90_receiver;
		short before_c6 = oa.prev_quadrant;
		short before_f25c2 = oa.tx_flags;
		int before_cc = oa.tx_scr_sr;
		int before_sas =
		    ((const unsigned char *)oa.pac3c)[CFG_FLAGS03]
		    & CFG_SAS_DETECT;

		memcpy(&snap, &oa, sizeof(snap));
		memcpy(snap_shp, shp_a, sizeof(snap_shp));
		memcpy(snap_bra, bra, sizeof(snap_bra));
		step_rrn(silence);
		account_rrn(silence, before_state, constel, before_c6,
			    before_cc, before_f25c2, before_sas);
		tag++;
	}
}

int
main(void)
{
	int rc = 0;
	int i;

	dsplib_debug_capture_on = 1;
	set_level(0);

	/*
	 * ------------------------------------------------------------------
	 * The tables both sides index, before anything indexes them.
	 */
	diff_begin("v90 phase 3/4 constellations");
	{
		diff_eq_int("vect4", memcmp(vect4, ref_vect4, sizeof(vect4)),
			    0, 0);
		diff_eq_int("vect16",
			    memcmp(vect16, ref_vect16, sizeof(vect16)), 0, 0);
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The dispatch, swept.
	 *
	 * `state` covers the eight values the object names and five it does
	 * not, because there is no default arm and "does nothing" is a claim
	 * about every other value.  `constel` covers both values `short_382` is
	 * documented to take and one that is neither -- which for case 5 is
	 * an ARM and not a fall-through, unlike the K56flex twin.  `tx_flags`
	 * sweeps the scrambler generator in bit 0 and `txmit`'s echo feed in
	 * bit 9.
	 */
	diff_begin("v90Phase34 dispatch");
	{
		static const int states[13] = { -1, 0, 1, 2, 3, 4, 5, 6, 7, 8,
						9, 10, 11 };
		static const short constels[3] = { (short)0x89b0,
						   (short)0x8990, 0x1234 };
		static const short f25c2s[4] = { 0, 1, 0x200, 0x201 };
		int df, am, s, c, g;

		for (df = 0; df <= 1; df++)
		for (am = 0; am <= 1; am++)
		for (s = 0; s < 13; s++)
		for (c = 0; c < 3; c++)
		for (g = 0; g < 4; g++) {
			setup(df, am, states[s], constels[c], 0x1234,
			      f25c2s[g], &CFG_RUN, 0,
			      0x1000u + (unsigned)tag);
			run(constels[c], 8);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The two symbol counts, and their two different comparisons.
	 *
	 * Cases 3 and 6 advance when the count passes 0x7f; cases 4 and 7
	 * when it reaches 0x10 EXACTLY.  Both counts are read zero-extended
	 * and compared as SIGNED 16-bit quantities, so 0x7ffe -- which makes
	 * the second increment 0x8000 -- stays below the threshold rather
	 * than passing it, and 0xfffe wraps to 0 rather than to 0x10000.
	 * Those two are what separate the object's comparison from every
	 * other reading of it.
	 */
	diff_begin("v90Phase34 symbol counts");
	{
		static const int states[4] = { 3, 4, 6, 7 };
		static const short cnts[8] = { 0, 0xd, 0xe, 0x7d, 0x7e,
					       (short)0xfffe, (short)0x7ffe,
					       (short)0x7fff };
		int s, k;

		for (s = 0; s < 4; s++)
		for (k = 0; k < 8; k++) {
			setup(1, 1, states[s], (short)0x8990, cnts[k], 1,
			      &CFG_RUN, 0, 0x3000u + (unsigned)tag);
			run((short)0x8990, 4);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The Ja completion tail, which the K56flex twin cannot reach at all.
	 *
	 * `terminateJa` makes `getV90JaBits` report the end of the sequence,
	 * and bit 2 of the configuration byte then selects between setting
	 * the backward-clear byte -- with a diagnostic -- and returning.
	 * Both are driven, and both are driven again at level 2 so the
	 * diagnostic itself is compared.
	 */
	diff_begin("v90Phase34 Ja completion");
	{
		static const short f25c2s[2] = { 0, 1 };
		int lvl, bc, g, k;

		/*
		 * THREE LEVELS, NOT TWO.  Both gates are `> 1`, so level 1 is
		 * the only value that separates them from the `>= 1` a reader
		 * would write -- and level 1 must be as silent as level 0.
		 */
		for (lvl = 0; lvl <= 2; lvl++) {
			set_level((unsigned int)lvl);
			for (bc = 0; bc <= 1; bc++)
			for (g = 0; g < 2; g++)
			for (k = 0; k < 2; k++) {
				setup(0, 1, 5, (short)0x8990, 0x1234,
				      f25c2s[g], k ? &CFG_JA0 : &CFG_JA, bc,
				      0x4000u + (unsigned)tag);
				run((short)0x8990, 4);
			}
		}
		set_level(0);
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The two CP completion tails.
	 *
	 * Case 8's moves the state on and stops; case 10's unpacks what the
	 * far end sent, sets the rates up and hands the handshake over.  Both
	 * need `getV90CpBits` to report termination, which needs the bit
	 * pointer to land exactly on `cpNofBits` with `terminateCpNot` set --
	 * satisfiable by construction and counted at run time all the same,
	 * because a case table that stopped reaching a path reads exactly
	 * like a passing test (findings F247, F262).
	 *
	 * `CFG_LD` is the other end of the same call: the sequence ends,
	 * `terminateCp` is set and the CPnot vector is loaded instead, so the
	 * arm runs its emitter and does NOT move the state.
	 */
	diff_begin("v90Phase34 CP completion");
	{
		static const int states[2] = { 8, 10 };
		static const short constels[2] = { (short)0x89b0,
						   (short)0x8990 };
		int lvl, s, c, k;

		for (lvl = 0; lvl <= 2; lvl++) {
			set_level((unsigned int)lvl);
			for (s = 0; s < 2; s++)
			for (c = 0; c < 2; c++)
			for (k = 0; k < 2; k++) {
				setup(1, 1, states[s], constels[c], 0x1234, 1,
				      k ? &CFG_LD : &CFG_CP, 0,
				      0x5000u + (unsigned)tag);
				run(constels[c], 4);
			}
		}
		set_level(0);
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * Case 3's diagnostic, which is the only one of the two that carries
	 * arguments -- the symbol count and the `period` at +0xaa86.
	 *
	 * Case 6 is byte-for-byte the same arm without it, so it is run in
	 * the same block at the same level: if the site migrated there, the
	 * transcripts would differ.
	 */
	diff_begin("v90Phase34 case 3 diagnostic");
	{
		static const int states[2] = { 3, 6 };
		int s, k, lvl;

		for (lvl = 0; lvl <= 2; lvl++) {
			set_level((unsigned int)lvl);
			for (s = 0; s < 2; s++)
			for (k = 0; k < 4; k++) {
				/*
				 * SNAPSHOT, NOT THE RUNNING TOTAL.  `n_s3_adv`
				 * is cumulative and the dispatch sweep above
				 * already drove it into the thousands, so
				 * `n_s3_adv > 0` here would be a tautology --
				 * a witness that cannot fail, which reads
				 * exactly like a covered path and is the shape
				 * findings F247, F262 and F295 record.  The
				 * advance is the diagnostic's own gate (`n`
				 * above 0x7f) and the level is 2, so an
				 * advance ACROSS THIS CALL is a witness that
				 * the site fired.
				 */
				long adv0 = n_s3_adv;

				setup(1, 1, states[s], (short)0x8990,
				      (short)(0x7c + k), 1, &CFG_RUN, 0,
				      0x6000u + (unsigned)tag);
				run((short)0x8990, 3);
				if (lvl == 2 && states[s] == 3
				    && n_s3_adv > adv0)
					n_s3_dbg++;
			}
		}
		set_level(0);
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The idle symbol's generator is not `tx_flags`.
	 *
	 * Both published emitters pass `tx_scrambler_mode(o)`, which is bit 0
	 * of `tx_flags`; case 5 passes the LITERAL 0.  So flipping that bit with
	 * everything else held fixed must leave case 5's outputs IDENTICAL --
	 * a claim about our side alone, and the one a reconstruction calling
	 * either emitter would fail even though the blob agreed with it on
	 * every other sweep.  Mode 0 is the calling station's polynomial, so
	 * the differential sees the substitution only for bit 0 SET; this
	 * check sees it either way.
	 *
	 * Bit 9 is held clear throughout: it gates `txmit`'s echo feed, which
	 * would move the object for reasons that have nothing to do with the
	 * scrambler.
	 */
	diff_begin("v90Phase34 idle generator is not tx_flags");
	{
		static const short constels[2] = { (short)0x89b0,
						   (short)0x8990 };
		static int cc0, c80, pt0;
		int c, gpc;

		for (c = 0; c < 2; c++) {
			for (gpc = 0; gpc <= 1; gpc++) {
				setup(1, 1, 5, constels[c], 0x1234,
				      (short)gpc, &CFG_RUN, 0,
				      0x7000u + (unsigned)c);
				run(constels[c], 6);
				if (gpc == 0) {
					cc0 = oa.tx_scr_sr;
					c80 = oa.cur_quadrant;
					pt0 = oa.txpoint.word;
				} else {
					diff_eq_int("gpc does not move tx_scr_sr "
						    "%ld", oa.tx_scr_sr, cc0, c);
					diff_eq_int("gpc does not move cur_quadrant "
						    "%ld", oa.cur_quadrant, c80, c);
					diff_eq_int("gpc does not move the "
						    "point %ld",
						    oa.txpoint.word,
						    pt0, c);
				}
			}
		}
	}
	rc |= diff_end();

	/*
	 * ==================================================================
	 * The two rate-renegotiation ladders.
	 * ==================================================================
	 *
	 * The dispatch first, over every state either ladder names and five
	 * it does not.  BOTH FUNCTIONS SEE THE WHOLE LIST, which is the check
	 * that they are two ladders and not one: `v90RateReneg` must do
	 * nothing for 15..20 and `v90RateRenegSilence` nothing for 11..14,
	 * and "nothing" is asserted as a byte-for-byte memcmp against the
	 * object before the call, not merely as agreement with the blob.
	 */
	who = "v90RateReneg";
	diff_begin("rate renegotiation dispatch");
	{
		static const int states[15] = { -1, 0, 2, 10, 11, 12, 13, 14,
						15, 16, 17, 18, 19, 20, 21 };
		static const short constels[3] = { (short)0x89b0,
						   (short)0x8990, 0x1234 };
		static const short f25c2s[4] = { 0, 1, 4, 0x205 };
		int sil, s, c, g;

		for (sil = 0; sil <= 1; sil++) {
			who = sil ? "v90RateRenegSilence" : "v90RateReneg";
			for (s = 0; s < 15; s++)
			for (c = 0; c < 3; c++)
			for (g = 0; g < 4; g++) {
				setup(1, 1, states[s], constels[c], 0x1234,
				      f25c2s[g], &CFG_RUN, 0,
				      0x8000u + (unsigned)tag);
				set_sas((int)(tag & 1));
				run_rrn(sil, constels[c], 8);
			}
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The four counted states, and the same two comparisons `v90Phase34`
	 * has: 11 and 15 advance when the count passes 0x7f, 12 and 16 when
	 * it reaches 0x10 EXACTLY.  The same eight counts, including the two
	 * that separate the object's signed 16-bit comparison from every
	 * other reading of it.
	 */
	diff_begin("rate renegotiation symbol counts");
	{
		static const int states[4] = { 11, 12, 15, 16 };
		static const short cnts[8] = { 0, 0xd, 0xe, 0x7d, 0x7e,
					       (short)0xfffe, (short)0x7ffe,
					       (short)0x7fff };
		int s, k;

		for (s = 0; s < 4; s++)
		for (k = 0; k < 8; k++) {
			int sil = states[s] >= 15;

			who = sil ? "v90RateRenegSilence" : "v90RateReneg";
			setup(1, 1, states[s], (short)0x8990, cnts[k], 1,
			      &CFG_RUN, 0, 0x9000u + (unsigned)tag);
			set_sas(1);
			run_rrn(sil, (short)0x8990, 4);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The three CP completion tails, which need `getV90CpBits` to report
	 * termination.  `CFG_CP` lands the bit pointer exactly on `cpNofBits`
	 * with `terminateCpNot` set; `CFG_LD` is the other end of the same
	 * call, where the sequence ends and the CPnot vector is loaded
	 * instead, so the arm emits and does NOT move the state.
	 *
	 * State 18 is the one that carries a diagnostic, so the levels are
	 * swept 0, 1 and 2 -- level 1 being the only value that separates the
	 * object's `> 1` gate from the `>= 1` a reader would write.  The SAS
	 * bit is driven both ways so that "clears it" and "leaves it clear"
	 * are two different trials, and `tx_flags` carries the freeze both set
	 * and clear so that state 18's clear and state 20's set each have a
	 * trial in which they change something.
	 */
	diff_begin("rate renegotiation CP completion");
	{
		static const int states[3] = { 14, 18, 20 };
		static const short constels[2] = { (short)0x89b0,
						   (short)0x8990 };
		static const short f25c2s[2] = { 1, 5 };
		int lvl, s, c, g, k, sas;

		for (lvl = 0; lvl <= 2; lvl++) {
			set_level((unsigned int)lvl);
			for (s = 0; s < 3; s++)
			for (c = 0; c < 2; c++)
			for (g = 0; g < 2; g++)
			for (sas = 0; sas <= 1; sas++)
			for (k = 0; k < 2; k++) {
				int sil = states[s] != 14;

				who = sil ? "v90RateRenegSilence"
					  : "v90RateReneg";
				setup(1, 1, states[s], constels[c], 0x1234,
				      f25c2s[g], k ? &CFG_LD : &CFG_CP, 0,
				      0xa000u + (unsigned)tag);
				set_sas(sas);
				run_rrn(sil, constels[c], 4);
			}
		}
		set_level(0);
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * State 19's idle symbol carries the literal 0 as its scrambler mode,
	 * exactly as `v90Phase34`'s case 5 does -- so flipping bit 0 of
	 * `tx_flags` with everything else held fixed must leave its outputs
	 * IDENTICAL.  A claim about our side alone, and the one a
	 * reconstruction that called `txmitdibit` here would fail even though
	 * the blob agreed with it on every other sweep.
	 *
	 * Bit 9 is held clear: it gates `txmit`'s echo feed, which would move
	 * the object for reasons that have nothing to do with the scrambler.
	 */
	who = "v90RateRenegSilence";
	diff_begin("silence idle generator is not tx_flags");
	{
		static const short constels[3] = { (short)0x89b0,
						   (short)0x8990, 0x1234 };
		static int cc0, c80, c60, pt0;
		int c, gpc;

		for (c = 0; c < 3; c++) {
			for (gpc = 0; gpc <= 1; gpc++) {
				setup(1, 1, 19, constels[c], 0x1234,
				      (short)gpc, &CFG_RUN, 0,
				      0xb000u + (unsigned)c);
				set_sas(1);
				run_rrn(1, constels[c], 6);
				if (gpc == 0) {
					cc0 = oa.tx_scr_sr;
					c80 = oa.cur_quadrant;
					c60 = oa.prev_quadrant;
					pt0 = oa.txpoint.word;
				} else {
					diff_eq_int("gpc does not move tx_scr_sr "
						    "%ld", oa.tx_scr_sr, cc0, c);
					diff_eq_int("gpc does not move cur_quadrant "
						    "%ld", oa.cur_quadrant, c80, c);
					diff_eq_int("gpc does not move prev_quadrant "
						    "%ld", oa.prev_quadrant, c60, c);
					diff_eq_int("gpc does not move the "
						    "point %ld",
						    oa.txpoint.word, pt0, c);
				}
			}
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * AND STATE 19 IS NOT `v90Phase34`'s CASE 5.  Two differences, and
	 * each is a separating trial with an observable result:
	 *
	 *   - a constellation code that is neither 0x89b0 nor 0x8990 takes
	 *     the FOUR-POINT arm here and the ZERO-POINT arm there, so the
	 *     transmitted point and the scrambler both move here and neither
	 *     moves there;
	 *   - `prev_quadrant` follows `cur_quadrant` here and is untouched there.
	 *
	 * Both are driven from the same object with the same seed, one call
	 * each, so the only difference between the two runs is which function
	 * was called.
	 */
	diff_begin("state 19 is not v90Phase34 case 5");
	{
		int cc5, c65, pt5;

		setup(1, 1, 5, 0x1234, 0x1234, 1, &CFG_RUN, 0, 0xc001u);
		set_sas(1);
		who = "v90Phase34";
		run(0x1234, 1);
		cc5 = oa.tx_scr_sr;
		c65 = oa.prev_quadrant;
		pt5 = oa.txpoint.word;

		setup(1, 1, 19, 0x1234, 0x1234, 1, &CFG_RUN, 0, 0xc001u);
		set_sas(1);
		who = "v90RateRenegSilence";
		run_rrn(1, 0x1234, 1);

		diff_eq_int("case 5 leaves the scrambler on an unknown "
			    "constellation %ld", cc5, 0x2f6b3d51, 0);
		diff_eq_int("state 19 clocks it %ld",
			    oa.tx_scr_sr != 0x2f6b3d51, 1, 0);
		diff_eq_int("the two transmit different points %ld",
			    oa.txpoint.word != pt5, 1, 0);
		diff_eq_int("case 5 leaves prev_quadrant at its seed %ld", c65, 2, 0);
		diff_eq_int("state 19 advances prev_quadrant %ld", oa.prev_quadrant,
			    oa.cur_quadrant, 0);
	}
	rc |= diff_end();

	who = "v90Phase34";

	/*
	 * ------------------------------------------------------------------
	 * Anti-vacuity.  Every arm of this function is reachable, so every
	 * counter is asserted to have fired -- there is no complement here,
	 * because there is nothing that cannot be entered.
	 */
	diff_begin("v90Phase34 coverage");
	{
		diff_eq_int("Ja arm ran", n_ja > 0, 1, 0);
		diff_eq_int("Ja completion tail ran", n_ja_tail > 0, 1, 0);
		diff_eq_int("backward clear taken", n_ja_clear > 0, 1, 0);
		diff_eq_int("backward clear not taken", n_ja_noclear > 0, 1,
			    0);
		diff_eq_int("arming arm below the gate", n_arm_low > 0, 1, 0);
		diff_eq_int("arming arm above the gate", n_arm_high > 0, 1, 0);
		diff_eq_int("case 3 ran", n_s3 > 0, 1, 0);
		diff_eq_int("case 3 advanced", n_s3_adv > 0, 1, 0);
		diff_eq_int("case 3 diagnostic reached", n_s3_dbg > 0, 1, 0);
		diff_eq_int("case 4 ran", n_s4 > 0, 1, 0);
		diff_eq_int("case 4 advanced", n_s4_adv > 0, 1, 0);
		diff_eq_int("case 5 sixteen-point ran", n_s5_16 > 0, 1, 0);
		diff_eq_int("case 5 four-point ran", n_s5_4 > 0, 1, 0);
		diff_eq_int("case 5 zero-point ran", n_s5_zero > 0, 1, 0);
		diff_eq_int("case 6 ran", n_s6 > 0, 1, 0);
		diff_eq_int("case 6 advanced", n_s6_adv > 0, 1, 0);
		diff_eq_int("case 7 ran", n_s7 > 0, 1, 0);
		diff_eq_int("case 7 advanced", n_s7_adv > 0, 1, 0);
		diff_eq_int("case 8 ran", n_s8 > 0, 1, 0);
		diff_eq_int("case 8 advanced", n_s8_adv > 0, 1, 0);
		diff_eq_int("case 9 quadbit ran", n_s9_16 > 0, 1, 0);
		diff_eq_int("case 9 dibit ran", n_s9_4 > 0, 1, 0);
		diff_eq_int("case 10 ran", n_s10 > 0, 1, 0);
		diff_eq_int("case 10 handed over", n_s10_adv > 0, 1, 0);
		diff_eq_int("no-arm case ran", n_noarm > 0, 1, 0);
		diff_eq_int("something was printed", n_dbg_lines > 0, 1, 0);

		for (i = 0; i < NPTR; i++)
			diff_eq_int("pointer slot %ld held two addresses",
				    ptr_seen[i], 1, i);
	}
	rc |= diff_end();

	/*
	 * The same for the two ladders.  Ten states between them and every
	 * one of them entered; the four that advance on a count and the three
	 * that advance on a CP have their advance counted separately, because
	 * an arm that runs and never advances is the shape a case table stops
	 * reaching without anything failing (findings F247, F262, F295).
	 */
	diff_begin("rate renegotiation coverage");
	{
		diff_eq_int("state 11 ran", n_r11 > 0, 1, 0);
		diff_eq_int("state 11 advanced", n_r11_adv > 0, 1, 0);
		diff_eq_int("state 12 ran", n_r12 > 0, 1, 0);
		diff_eq_int("state 12 advanced", n_r12_adv > 0, 1, 0);
		diff_eq_int("state 13 ran", n_r13 > 0, 1, 0);
		diff_eq_int("state 14 ran", n_r14 > 0, 1, 0);
		diff_eq_int("state 14 handed over", n_r14_adv > 0, 1, 0);
		diff_eq_int("v90RateReneg's no-arm case ran", n_rnone > 0, 1,
			    0);

		diff_eq_int("state 15 ran", n_s15 > 0, 1, 0);
		diff_eq_int("state 15 advanced", n_s15_adv > 0, 1, 0);
		diff_eq_int("state 16 ran", n_s16 > 0, 1, 0);
		diff_eq_int("state 16 advanced", n_s16_adv > 0, 1, 0);
		diff_eq_int("state 17 ran", n_s17 > 0, 1, 0);
		diff_eq_int("state 18 ran", n_s18 > 0, 1, 0);
		diff_eq_int("state 18 advanced", n_s18_adv > 0, 1, 0);
		diff_eq_int("state 18's own message was printed",
			    n_s18_dbg > 0, 1, 0);
		diff_eq_int("state 18 cleared a set SAS bit", n_s18_sas > 0,
			    1, 0);
		diff_eq_int("state 19 sixteen-point ran", n_s19_16 > 0, 1, 0);
		diff_eq_int("state 19 four-point ran", n_s19_4 > 0, 1, 0);
		diff_eq_int("state 20 ran", n_s20 > 0, 1, 0);
		diff_eq_int("state 20 handed over", n_s20_adv > 0, 1, 0);
		diff_eq_int("state 20 set a clear freeze", n_s20_freeze > 0,
			    1, 0);
		diff_eq_int("v90RateRenegSilence's no-arm case ran",
			    n_snone > 0, 1, 0);
	}
	rc |= diff_end();

	dsplib_debug_capture_on = 0;
	return rc;
}
