/*
 * t_v34conn.c -- the same whole V.34 call as `t_v34call.c`, four ways, but
 * between two endpoints the BLOB'S OWN CONSTRUCTOR built.
 *
 * WHAT CHANGES FROM `t_v34call.c`, AND IT IS ONLY ONE THING: where the two
 * objects come from.  That file brings two raw arenas up by calling
 * `v34handshakinit` on them, so every field the handshake initialiser does
 * not write is pseudorandom and the call cannot converge (finding F786 records
 * that honestly).  This file calls the operations table `dp_vpcm_init`
 * registers -- `ops->create(modem, 34, caller, 9600, 48, ops)` -- and drives
 * the V.34 object at +0x2c of what it returns.  The wire, the four runs, the
 * per-block comparison, the alarm and the recorded literals are the same
 * shape.
 *
 * WHY THAT IS A DIFFERENTIAL TEST AND NOT A HYBRID.  Findings F800-806
 * measured it rather than assuming it: of the 265,520 bytes and 125 heap
 * regions one construction produces, exactly TWO words point at blob CODE,
 * both are `struct v34_shell` scrambler callbacks, both name functions this
 * tree has reconstructed and tested four suites' worth, and two stores
 * replace them.  Everything else is data -- 21 constant tables, and two
 * vtables in V.90 resampler sub-objects a V.34 handshake never dispatches
 * through.  There is no vtable in the root arena and no function-pointer
 * table, so nothing silently routes a `datapumpv34` block back into
 * `VPcmV34Main.cpp`.  `ours_scramblers()` below is those two stores.
 *
 * WHAT THE FIXTURE THEREFORE CANNOT TEST is the constructor, and the
 * configuration is the blob's.  Five parameters had to be chosen to construct
 * at all (finding F801) and two of them are ADDRESSES rather than numbers:
 * `MDMPRM_DPRUNTIME` is dereferenced at once and `MDMPRM_DSPINFO` is written
 * through by `vpcm_delete`.
 *
 * THOSE FIVE ARE NOW DERIVED RATHER THAN CHOSEN, which is the one thing about
 * this file that has changed since finding F902 (findings F820-825).  Both
 * addresses have types and both types have sizes: DPRUNTIME is a
 * `struct _tagModemParameters` built by `dp_runtime_create`, which is in this
 * object at 0x58e0 and is now reconstructed and differentially tested
 * (`t_dp_param`), and DSPINFO is a 16-byte `struct dsp_info` whose four words
 * are the four this object reads and writes.  The three numbers are the
 * HOST's, not a modem-shaped guess: `MODEM_MIN_RATE` 300 and `MODEM_MAX_RATE`
 * 56000 are what `slmodemd/modem.c` puts in `m->min_rate`/`m->max_rate`, and
 * `MDMPRM_IODELAY` is `m->driver.ioctl(m, MDMCTL_IODELAY, 0)`, which is 0 for
 * slmodemd's own socket driver.  `CFG_SRATE` 9600 is `MODEM_RATE` and
 * `CFG_MAX_FRAG` 48 is `MODEM_FRAG`, which is `MODEM_RATE/200` -- so the two
 * the constructor guards were never free either.
 *
 * IT STILL DOES NOT CONNECT, and the trajectory did not move (finding F825).
 * That is the point of deriving them: the negative result now costs the
 * configuration as a suspect instead of leaving it as one.
 *
 * CONGRUENCE, WHICH A CONSTRUCTED OBJECT DOES NOT GET FOR FREE.  `t_v34call`
 * compares four runs over the same two static arenas, so every address is
 * identical by construction.  Two constructions land 125 regions at 125
 * different addresses, and 19 pointer words in each root would then differ
 * run to run for a reason that has nothing to do with the modem.  So the two
 * endpoints are constructed ONCE and the whole live allocation set is
 * snapshotted; every run restores it first.  That is finding F805's
 * snapshot-restore trick at graph scope, and it makes the four runs literally
 * the same memory at the same addresses again.  `graph_congruent` asserts the
 * restore is exact rather than trusting it.
 *
 * AND +0x2218.  `VPcmV34Create` leaves it 0, which is `datapumpv34`'s DATA
 * branch.  The blob's own writers of that word are `VPcmV34InitiateRetrain`
 * and `VPcmV34InitMOH` (2), `VPcmV34InitiateHangUp` and
 * `VPcmV34SetV90RateReneg` (5), `datapumpv34` itself (2, 3, 4 and 5) and
 * `v34handshak` (1, at 0x63d4d, immediately after printing both connection
 * rates).  So 2 is "handshaking" and 1 is CONNECTED, and this file writes the
 * 2 exactly as `src/pump/v34/v34pcmmain.cpp` does after its own bring-up.
 *
 * DOES IT CONNECT?  NO -- and that is asserted here as an exact fact rather
 * than passed over.  See `expect[]`: the mode word is claimed to be still 2
 * at the end on both endpoints, the number of blocks in which either endpoint
 * left mode 2 is claimed to be 0, and the trajectory is pinned block by
 * block.  This is a RECORDED FRONTIER, not a requirement: the day the
 * handshake completes, this test fails, and the failure names the mode word
 * and both rate fields.  That is the intended way to find out.
 */

#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "harness.h"
#include "v34hsstep.h"

#include "dsplib/dp.h"
#include "dsplib/modem_params.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34scram.h"
#include "dsplib/v34shell.h"

extern void ref_dp_vpcm_init(void);
extern void *ref_dp_runtime_create(void *modem);
extern void ref_dp_runtime_delete(void *runtime);
extern void ref_datapumpv34(void *obj);
extern int ref_modem_serrint(void *obj);
extern unsigned int ref_dsplibs_debug_level;
extern unsigned int dsplibs_debug_level;

/* --- the object, by offset, so this file reads as the disassembly does ----- */

/*
 * `vpcm_create` allocates 0xd258 bytes and hands `lea 0x2c(%ebx),%ebp` to
 * `VPcmV34Create`, which hands the same pointer to `v34handshakinit`.  So the
 * V.34 object is the root plus 0x2c and everything below is an offset in IT.
 */
#define ROOT_LEN	0xd258
#define ROOT_V34	0x2c

#define O_F25E		0x025e	/* short: the sample this step puts on the line */
#define O_F260		0x0260	/* short: the sample this step takes off it     */
#define O_RXCOUNT	0x0264	/* short: the receive queue's count             */
#define O_MODE		0x2218	/* int:   2 handshaking, 1 CONNECTED            */
#define O_TXCOUNT	0x221c	/* short: the transmit queue's count            */
#define O_TXLIMIT	0x2aa0	/* short: how many samples the block wants      */
#define O_MICROSTATE	0x3592
#define O_RXSTATE	0x3594
#define O_TXSTATE	0x3596
#define O_ROLE		0x359c	/* short: 0x65 originate, 0x66 answer           */
/*
 * The two connection rates in units of 2400 bit/s: `v34handshak` at 0x63d1a
 * and 0x63d32 multiplies each by 0x960 to print them, and writes mode 1 five
 * instructions later.  Zero while the call has not converged.
 */
#define O_TXRATE	0xaa88
#define O_RXRATE	0xaa98

#define ROLE_ORIGINATE	0x65
#define ROLE_ANSWER	0x66

#define MODE_HANDSHAKE	2
#define MODE_CONNECTED	1

/* --- the configuration, DERIVED -------------------------------------------- */

/*
 * Finding F801's five, each traced to what actually answers it.  Two are
 * addresses: +0x28 holds `MDMPRM_DPRUNTIME` and `movl $0x0,0x78(%eax)` writes
 * through it at once, and `vpcm_delete` writes two words through
 * `MDMPRM_DSPINFO` at 0x3ded.  Each endpoint gets its OWN pair -- one block
 * written into both constructions is exactly the shared state findings
 * F319-322 exist to avoid.
 *
 * The three numbers are `slmodemd`'s, which is the host this object was
 * compiled against and whose source survives:
 *
 *   MDMPRM_MIN_RATE   m->min_rate = MODEM_MIN_RATE = 300    (modem.h:89)
 *   MDMPRM_MAX_RATE   m->max_rate = MODEM_MAX_RATE = 56000  (modem.h:90)
 *   MDMPRM_IODELAY    m->driver.ioctl(m, MDMCTL_IODELAY, 0).  A host
 *                     MEASUREMENT, so what is derived is the formula the
 *                     object applies to it and not the input.  READ THE NOTE
 *                     ON CFG_IODELAY BELOW BEFORE CHANGING IT: 0 is the
 *                     socket driver's, ALSA's is 424, and the difference is
 *                     visible in the handshake.
 *   MDMPRM_CODECTYPE  the socket driver's 4 = CODEC_STLC7550; read only by
 *                     `dp_runtime_create`, into +0x54
 *
 * MODEM_MAX_RATE 56000 is worth a second look: it is the SAME 0xdac0 the
 * constructor clamps to at 0x3b65, so the host's ceiling and the library's
 * are one number written twice, and the clamp is unreachable from slmodemd.
 *
 * CFG_SRATE and CFG_MAX_FRAG were recorded as the constructor's two guards.
 * They are also exactly what the host passes: `m->srate = MODEM_RATE` = 9600
 * and `m->frag = MODEM_FRAG` = MODEM_RATE/200 = 48 (modem.h:85-86, modem.c:
 * 1930), reaching `op->create(m, dp_id, m->caller, m->srate, m->frag, op)` at
 * modem.c:1051.  So 48 is the value, not merely a value the guard admits.
 */
#define CFG_MIN_RATE	300
#define CFG_MAX_RATE	56000
#define CFG_IODELAY	0
#define CFG_CODECTYPE	4

/*
 * CFG_IODELAY IS THE ONE VALUE HERE THAT IS A JUDGEMENT AND NOT A DERIVATION,
 * AND IT IS THE ONE THE CALL IS SENSITIVE TO.  Say so loudly, because a later
 * reader will otherwise change it and think they have found something.
 *
 * The three drivers slmodemd ships answer MDMCTL_IODELAY differently:
 *
 *   socket     0.  And it is a STUB -- `modem_main.c:682` has the real
 *              expression commented out beside it, with the note that the
 *              kernel module returns `s->delay + ST7554_HW_IODELAY (48)`.
 *   ALSA       `dev->delay`, which `alsa_start` sets to the 384 samples of
 *              silence it writes at startup plus `INTERNAL_DELAY` 40 =  424.
 *   modemap    the kernel's answer plus `dev->delay`, itself the 192 samples
 *              `modemap_start` writes.
 *
 * So a REAL sound card reports a few hundred, not zero, and 424 + 4 trips the
 * 0x3d6f clamp: HW pins to 244 and the DMA correction at root +0xd254 becomes
 * 384.  Finding F824 measures that this takes the handshake THREE MICROSTATES
 * FURTHER -- the originator ends in 59 RX_PHASE2_CALL instead of error-
 * recovering to 44 DET_INFO.
 *
 * 0 is committed anyway, for a reason that is not "it is what this project's
 * host says", though it is: **the I/O delay and this file's wire are the same
 * physical quantity modelled twice.** The wire below is 288 samples each way
 * because finding F903 asked the object and it said 1 was out of spec; with
 * that wire the object measures `bulkDelay=500, count2=509`, which is the
 * round trip it actually has. Setting the I/O delay to a real card's 424 while
 * leaving the wire at 288 would describe a line this test does not simulate,
 * and the pair would then be incoherent rather than merely approximate.
 *
 * The two move together or not at all. Whoever raises one raises the other.
 *
 * WHICH WAS THEN DONE, ten ways, and finding F839 has the table. A longer line
 * takes BOTH endpoints three microstates further -- the answerer reaches
 * 52/53/51 and the originator 59 RX_PHASE2_CALL -- and **not one of the ten
 * connects**: mode 2 and four zero rate words every time. So the delay pairing
 * is not what stops the call, and this file keeps the short line because
 * finding F902's recorded numbers are the ones fourteen hand mutations are
 * pinned to. Change both together or neither.
 */
#define CFG_SRATE	9600	/* `cmp $0x2580,%esi`, and MODEM_RATE          */
#define CFG_MAX_FRAG	48	/* `cmpl $0x30,...` / `jg`, and MODEM_FRAG     */

/*
 * WHERE EACH ONE LANDS.  Read off `vpcm_create` and asserted below, so this
 * is a claim about the blob's construction and not a restatement of the two
 * lines that set it up.
 */
#define RT_RATE_LOW	0x30	/* MDMPRM_MIN_RATE, verbatim         (0x3b8a) */
#define RT_RATE_HIGH	0x34	/* MDMPRM_MAX_RATE, min(., 0xdac0)   (0x3b8d) */
#define RT_MINRATE	0x38	/* the LITERAL 4800                  (0x3b90) */
#define RT_MAXRATE	0x3c	/* the LITERAL 33600                 (0x3b97) */
#define RT_HWDELAY	0x64	/* MDMPRM_IODELAY + 4                (0x3c0c) */
#define RT_DMADELAY	0x68	/* HW - 48 + root +0xd254            (0x3c1e) */
#define O_ROOT_D250	0xd250	/* 0x210 unless runtime +2 bit 4     (0x3bdf) */
#define O_ROOT_D254	0xd254	/* the delay correction, 0 unclamped          */
#define O_ROOT_DSPINFO	0x24
#define O_ROOT_RUNTIME	0x28

/* --- the shape of a run --------------------------------------------------- */

#define NSAMP		4	/* samples per endpoint per block  */
#define NBLOCK		1600	/* blocks in a call                */
#define NEP		2	/* endpoints                       */

/*
 * THE WIRE IS 30 ms EACH WAY, WHICH IS THE ONE THING HERE THE OBJECT ASKED
 * FOR IN WORDS.  `t_v34call`'s wire is one sample, and on a constructed
 * object the blob says what it thinks of that:
 *
 *     RTD (1) lower than min (30), masking Far EC...
 *
 * -- a round-trip delay of 1 against a floor of 30.  288 samples at the
 * 9,600 Hz the constructor insists on is 30 ms, so the two ends measure a
 * round trip inside the range V.34's phase 2 is written for.  It is a
 * property of the LINE and not of the modem, it is applied identically to all
 * four runs, and it therefore cannot manufacture agreement between them; what
 * it buys is a trajectory that reaches phase 2 rather than stopping short of
 * it, which is more of `v34handshak` under the differential comparison.
 */
#define WIRE_DELAY	288

#define TEXTMAX		8192	/* one endpoint's transcript for one block */
#define TEXTHEAD	160

enum run {
	RUN_BLOB,		/* the oracle: the blob on both ends */
	RUN_OURS,
	RUN_OURS_BLOB,		/* originate ours, answer the blob's */
	RUN_BLOB_OURS,
	NRUN
};

static const char *const run_name[NRUN] = {
	"blob-blob", "ours-ours", "ours-blob", "blob-ours"
};

/* Which endpoint runs OUR code, per run.  Index [run][endpoint]. */
static const int run_ours[NRUN][NEP] = {
	{ 0, 0 },
	{ 1, 1 },
	{ 1, 0 },
	{ 0, 1 }
};

static const char *const ep_name[NEP] = { "originate", "answer" };
static const int ep_caller[NEP] = { 1, 0 };
static const short ep_role[NEP] = { ROLE_ORIGINATE, ROLE_ANSWER };

#define TRIPLE(m, r, t)	(((unsigned)(unsigned short)(m) << 16) \
			 | ((unsigned)(unsigned char)(r) << 8) \
			 | (unsigned)(unsigned char)(t))

/* What one endpoint did in one block. */
struct blockrec {
	unsigned	objhash;	/* the root object, skips excluded */
	unsigned	txthash;	/* this block's transcript         */
	unsigned	lines;
	short		mst, rxst, txst;
	short		txcount, rxcount;
	int		mode;		/* +0x2218                         */
	int		nonzero;	/* non-zero transmit samples       */
	char		head[TEXTHEAD];
};

static struct blockrec rec[NRUN][NEP][NBLOCK];

/* Totals, per run per endpoint, for the anti-vacuity claims. */
struct total {
	unsigned	lines;
	int		distinct;	/* distinct (mst, rxst, txst) triples */
	int		moved;		/* blocks in which the triple changed */
	int		nonzero;
	int		connected;	/* blocks in which mode left 2        */
	unsigned	seed;		/* the triple construction left       */
	unsigned	final;
	int		finalmode;
	short		txrate, rxrate;
	int		iterated;	/* blocks in which the pump moved txq */
	/*
	 * Which scrambler pair this endpoint was still holding when the run
	 * ENDED.  Read at the end rather than after the store, so it also
	 * says the substitution survived 1,600 blocks: recording it here
	 * instead of checking the object later is not tidiness, because the
	 * four runs share one object and the last run's value is all a later
	 * reader can see.
	 */
	int		rx_ours, tx_ours;
	unsigned	triple[NBLOCK];
	int		ntriple;
};

static struct total tot[NRUN][NEP];

/* The graph outside the two roots, once per run: hashed before and after. */
static unsigned graph0[NRUN], graph1[NRUN];

static int dump;

/* --- construction --------------------------------------------------------- */

static char *root[NEP], *obj[NEP];

/*
 * THE HOST'S TWO BLOCKS, one pair per endpoint and both of a KNOWN TYPE now.
 *
 * `dsp_info` is the host's own storage -- nothing in this object allocates
 * one -- so it stays a static here, zeroed, which is what a first call on a
 * fresh slmodemd has: `datafile_load_info` has not run, and
 * `m->dsp_info.qc_lapm` is `m->cfg.ec && m->cfg.ec_detector`.
 *
 * The runtime block is NOT a static, and that is a change worth its own line.
 * `dp_runtime_create` heap-allocates it, so putting it on the heap here is
 * what the host does -- and it also puts it inside the snapshotted graph.  A
 * static one sat OUTSIDE the graph and was never restored between the four
 * runs, so anything the call wrote into it would have leaked from one run to
 * the next.  Nothing observed ever did, but the hole was real.
 */
static struct dsp_info dspinfo[NEP];
static struct _tagModemParameters *runtime[NEP];

#define MAXREG	512
static void *reg[MAXREG];
static size_t regsz[MAXREG];
static unsigned char *snap[MAXREG];
static int nreg;

static short
peek16(int ep, unsigned off)
{
	short v;

	memcpy(&v, obj[ep] + off, sizeof(v));
	return v;
}

static int
peek32(int ep, unsigned off)
{
	int v;

	memcpy(&v, obj[ep] + off, sizeof(v));
	return v;
}

/* The V.PCM root, not the V.34 object at +0x2c of it. */
static int
peek_root(int ep, unsigned off)
{
	int v;

	memcpy(&v, root[ep] + off, sizeof(v));
	return v;
}

static void
poke16(int ep, unsigned off, short v)
{
	memcpy(obj[ep] + off, &v, sizeof(v));
}

static void
poke32(int ep, unsigned off, int v)
{
	memcpy(obj[ep] + off, &v, sizeof(v));
}

static struct dp_operations *
vpcm_ops(void)
{
	int i;

	harness_reg_reset();
	ref_dp_vpcm_init();
	/*
	 * ONE table under THREE ids -- 34, 90 and 92, all named "VPCM"
	 * (finding F800).  34 is asked for by name rather than taken as the
	 * first, so a registry that stopped offering V.34 fails here.
	 */
	for (i = 0; i < harness_reg_ref.count; i++)
		if (harness_reg_ref.id[i] == 34)
			return harness_reg_ref.ops[i];
	return 0;
}

static int
build(struct dp_operations *ops, int ep)
{
	struct dp *d;

	memset(&dspinfo[ep], 0, sizeof(dspinfo[ep]));
	harness_param_set(MDMPRM_DSPINFO, (long)(size_t)&dspinfo[ep]);
	harness_param_set(MDMPRM_CODECTYPE, CFG_CODECTYPE);

	/*
	 * THE HOST BUILDS THE RUNTIME BLOCK BEFORE IT BUILDS THE DATAPUMP --
	 * `slmodemd/modem.c:1136` -- so this call is in the right place and
	 * not a convenience.  The BLOB's `dp_runtime_create` is used, for the
	 * same reason every other part of this fixture is the blob's: ours is
	 * proved identical to it by `t_dp_param`, and a fixture that mixed
	 * the two would need that proof restated here.
	 */
	runtime[ep] = (struct _tagModemParameters *)
		ref_dp_runtime_create((void *)0xD1A1u);
	if (runtime[ep] == 0)
		return 0;

	harness_param_set(MDMPRM_DPRUNTIME, (long)(size_t)runtime[ep]);
	harness_param_set(MDMPRM_MIN_RATE, CFG_MIN_RATE);
	harness_param_set(MDMPRM_MAX_RATE, CFG_MAX_RATE);
	harness_param_set(MDMPRM_IODELAY, CFG_IODELAY);

	d = ops->create((void *)0xD1A1u, 34, ep_caller[ep], CFG_SRATE,
			CFG_MAX_FRAG, ops);
	root[ep] = (char *)d;
	obj[ep] = root[ep] + ROOT_V34;
	return d != 0;
}

/* --- the heap graph, snapshotted so four runs are congruent ---------------- */

static int
is_root(const void *p)
{
	return p == (const void *)root[0] || p == (const void *)root[1];
}

static void
graph_take(void)
{
	int i;

	nreg = harness_alloc_live_set(reg, MAXREG);
	if (nreg > MAXREG) {
		printf("FIXTURE: %d live regions, table holds %d\n", nreg,
		       MAXREG);
		exit(1);
	}
	for (i = 0; i < nreg; i++) {
		regsz[i] = malloc_usable_size(reg[i]);
		snap[i] = malloc(regsz[i]);
		if (snap[i] == 0) {
			printf("FIXTURE: out of memory snapshotting\n");
			exit(1);
		}
		memcpy(snap[i], reg[i], regsz[i]);
	}
}

static void
graph_restore(void)
{
	int i;

	for (i = 0; i < nreg; i++)
		memcpy(reg[i], snap[i], regsz[i]);
}

/*
 * The graph OUTSIDE the two root objects.  The roots are hashed per block and
 * separately, and they are also the only two regions holding a word that
 * legitimately differs between runs, so keeping them out makes this hash
 * comparable run against run with nothing excluded at all.
 */
static unsigned
graph_hash(void)
{
	unsigned h = 2166136261u;
	int i;
	size_t j;

	for (i = 0; i < nreg; i++) {
		const unsigned char *p = (const unsigned char *)reg[i];

		if (is_root(p))
			continue;
		for (j = 0; j < regsz[i]; j++)
			h = (h ^ p[j]) * 16777619u;
	}
	return h;
}

/* --- the two stores, and the hash that has to skip them ------------------- */

/*
 * `preinitdigital` installs a scrambler in the transmit shell and a
 * descrambler in the receive one, at the same offset in each --
 * `offsetof(struct v34_shell, scramble)`, with the transmit shell
 * `V34_SHELL_TX` further along.  Computed rather than written as 0xe48 and
 * 0x2a28 so that a shell layout change moves this with it.
 */
#define RX_SCRAMBLE	((unsigned)__builtin_offsetof(struct v34_shell, scramble))
#define TX_SCRAMBLE	(RX_SCRAMBLE + (unsigned)V34_SHELL_TX)

/*
 * THE PAIRING IS `src/pump/v34/v34digital.c`'s, ON `src/pump/v34/v34digital.c`'s
 * CONDITION, and the blob's constructor was measured to install exactly it
 * (finding F802) -- the two were derived independently and agree, which is
 * what makes replacing them a substitution rather than a patch.
 */
static void
ours_scramblers(int ep)
{
	void *rx, *tx;

	if (peek16(ep, O_ROLE) == ROLE_ORIGINATE) {
		tx = (void *)scrambleGPC;
		rx = (void *)descrambleGPA;
	} else {
		tx = (void *)scrambleGPA;
		rx = (void *)descrambleGPC;
	}
	memcpy(obj[ep] + RX_SCRAMBLE, &rx, sizeof(rx));
	memcpy(obj[ep] + TX_SCRAMBLE, &tx, sizeof(tx));
}

static void *
scrambler_at(int ep, unsigned off)
{
	void *p;

	memcpy(&p, obj[ep] + off, sizeof(p));
	return p;
}

static int
is_ours_scrambler(const void *p)
{
	return p == (const void *)scrambleGPC || p == (const void *)scrambleGPA
	       || p == (const void *)descrambleGPA
	       || p == (const void *)descrambleGPC;
}

/*
 * FNV-1a over the whole 53,848-byte root, with two classes of skip and no
 * others.
 *
 *   the two scrambler callbacks   they hold our address in a run where this
 *                                 endpoint is ours and the blob's otherwise,
 *                                 which is the whole point of the fixture
 *   `v34hs_in_hole`               the thirty-seven pointer fields the step
 *                                 fixture already lists, mapped through the
 *                                 +0x2c the V.34 object sits at.  Our code
 *                                 installs OUR library tables where the blob
 *                                 installs ITS own.
 *
 * Nothing is skipped pre-emptively beyond that.  A pointer-shaped divergence
 * anywhere else is a real disagreement and `diagnose` names its offset.
 */
static unsigned
objhash(int ep)
{
	const unsigned char *p = (const unsigned char *)root[ep];
	unsigned h = 2166136261u;
	unsigned i;

	for (i = 0; i < ROOT_LEN; i++) {
		if (i >= ROOT_V34) {
			unsigned o = i - ROOT_V34;

			if (o >= RX_SCRAMBLE && o < RX_SCRAMBLE + 4)
				continue;
			if (o >= TX_SCRAMBLE && o < TX_SCRAMBLE + 4)
				continue;
			if (o < sizeof(struct v34_object) && v34hs_in_hole(o))
				continue;
		}
		h = (h ^ p[i]) * 16777619u;
	}
	return h;
}

static unsigned
strhash(const char *s)
{
	unsigned h = 2166136261u;

	for (; *s != '\0'; s++)
		h = (h ^ (unsigned char)*s) * 16777619u;
	return h;
}

/* --- the four implementations, chosen per endpoint per run ---------------- */

static int ep_ours[NEP];

static void
ep_serrint(int ep)
{
	if (ep_ours[ep])
		modem_serrint(obj[ep]);
	else
		ref_modem_serrint(obj[ep]);
}

static void
ep_pump(int ep)
{
	if (ep_ours[ep])
		datapumpv34(obj[ep]);
	else
		ref_datapumpv34(obj[ep]);
}

/* Our code logs to capture channel 0 and the blob's to channel 1. */
static int
ep_slot(int ep)
{
	return ep_ours[ep] ? 0 : 1;
}

/* --- the alarm ------------------------------------------------------------ */

static volatile int cur_run, cur_block, cur_ep;
static volatile int cur_state[NEP][3];

static void
call_alarm(int sig)
{
	char msg[256];
	int n;

	(void)sig;
	n = snprintf(msg, sizeof(msg),
		     "\nt_v34conn: %s block %d endpoint %s did not return.\n"
		     "  originate mst=%d rxstate=%d txstate=%d\n"
		     "  answer    mst=%d rxstate=%d txstate=%d\n"
		     "  see finding F287 for why the loop need not terminate\n",
		     run_name[cur_run], cur_block, ep_name[cur_ep],
		     cur_state[0][0], cur_state[0][1], cur_state[0][2],
		     cur_state[1][0], cur_state[1][1], cur_state[1][2]);
	if (n > 0)
		(void)!write(2, msg, (size_t)n);
	_exit(3);
}

/* --- one run -------------------------------------------------------------- */

static void
grab(char *buf, unsigned *len, int ep)
{
	const char *t = dsplib_debug_capture_text(ep_slot(ep));
	size_t n = strlen(t);

	if (*len + n + 1 >= TEXTMAX) {
		printf("FIXTURE: block transcript over %d bytes\n", TEXTMAX);
		exit(1);
	}
	memcpy(buf + *len, t, n);
	*len += (unsigned)n;
	buf[*len] = '\0';
}

static void
note_triple(struct total *t, unsigned triple)
{
	int i;

	for (i = 0; i < t->ntriple; i++)
		if (t->triple[i] == triple)
			return;
	if (t->ntriple >= NBLOCK) {
		printf("FIXTURE: more than %d distinct state triples\n", NBLOCK);
		exit(1);
	}
	t->triple[t->ntriple++] = triple;
	t->distinct++;
}

static void
run_call(int r, int nblock)
{
	static char text[NEP][TEXTMAX];
	static short wire[NEP][WIRE_DELAY];
	void (*prev)(int);
	int wpos = 0;
	int ep, blk, i;

	cur_run = r;
	memset(wire, 0, sizeof(wire));
	for (ep = 0; ep < NEP; ep++) {
		ep_ours[ep] = run_ours[r][ep];
		memset(&tot[r][ep], 0, sizeof(tot[r][ep]));
	}

	/*
	 * THE RESTORE IS THE BRING-UP.  There is no `v34handshakinit` call
	 * here and no `V34InitializeImplementationSpecific`: the constructor
	 * did both, and finding F801 shows it logging all three transitions
	 * (txstate NOSTATE0 => SILENCEINFO, rxstate => RX_DPSK, microstate =>
	 * DET_SYNC).  What each run needs is that state back, byte for byte,
	 * at the same addresses.
	 */
	graph_restore();
	graph0[r] = graph_hash();

	for (ep = 0; ep < NEP; ep++) {
		if (ep_ours[ep])
			ours_scramblers(ep);
		poke32(ep, O_MODE, MODE_HANDSHAKE);
		cur_state[ep][0] = peek16(ep, O_MICROSTATE);
		cur_state[ep][1] = peek16(ep, O_RXSTATE);
		cur_state[ep][2] = peek16(ep, O_TXSTATE);
		tot[r][ep].seed = TRIPLE(cur_state[ep][0], cur_state[ep][1],
					 cur_state[ep][2]);
	}

	prev = signal(SIGALRM, call_alarm);
	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = 2u;
	ref_dsplibs_debug_level = 2u;

	for (blk = 0; blk < nblock; blk++) {
		unsigned len[NEP];
		int nonzero[NEP];

		cur_block = blk;
		for (ep = 0; ep < NEP; ep++) {
			len[ep] = 0;
			text[ep][0] = '\0';
			nonzero[ep] = 0;
		}

		/*
		 * THE WIRE.  `WIRE_DELAY` samples each way: what an endpoint
		 * emits now is what the other hears that many sample instants
		 * later.  The capture is reset around every single call
		 * because in a homogeneous run both endpoints write the same
		 * channel, and a transcript nobody can attribute to an
		 * endpoint cannot name which one diverged.
		 */
		for (i = 0; i < NSAMP; i++) {
			short out[NEP];

			for (ep = 0; ep < NEP; ep++) {
				cur_ep = ep;
				dsplib_debug_capture_reset();
				poke16(ep, O_F260, wire[ep][wpos]);
				alarm(20);
				ep_serrint(ep);
				alarm(0);
				out[ep] = peek16(ep, O_F25E);
				if (out[ep] != 0)
					nonzero[ep]++;
				grab(text[ep], &len[ep], ep);
			}
			wire[0][wpos] = out[1];
			wire[1][wpos] = out[0];
			wpos = (wpos + 1) % WIRE_DELAY;
		}

		for (ep = 0; ep < NEP; ep++) {
			struct blockrec *b = &rec[r][ep][blk];
			struct total *t = &tot[r][ep];
			unsigned triple;
			short pre_tx;

			cur_ep = ep;
			dsplib_debug_capture_reset();
			pre_tx = peek16(ep, O_TXCOUNT);
			alarm(20);
			ep_pump(ep);
			alarm(0);
			grab(text[ep], &len[ep], ep);

			b->mst = peek16(ep, O_MICROSTATE);
			b->rxst = peek16(ep, O_RXSTATE);
			b->txst = peek16(ep, O_TXSTATE);
			b->txcount = peek16(ep, O_TXCOUNT);
			b->rxcount = peek16(ep, O_RXCOUNT);
			b->mode = peek32(ep, O_MODE);
			b->objhash = objhash(ep);
			b->txthash = strhash(text[ep]);
			b->nonzero = nonzero[ep];
			snprintf(b->head, sizeof(b->head), "%s", text[ep]);

			/*
			 * DID THE HANDSHAKE LOOP ACTUALLY RUN?  Finding F805's
			 * passes 0 and 1 were vacuous for exactly this reason:
			 * `txinit` leaves the transmit queue at 32 against a
			 * limit of 16, so `datapumpv34` returns having done
			 * nothing and "it did not fault" is not a result.  A
			 * block in which the pump RAISED the queue is a block
			 * in which `v34handshak` was called.
			 */
			if (b->txcount > pre_tx)
				t->iterated++;
			if (b->mode != MODE_HANDSHAKE)
				t->connected++;

			triple = TRIPLE(b->mst, b->rxst, b->txst);
			t->final = triple;
			t->finalmode = b->mode;
			t->txrate = peek16(ep, O_TXRATE);
			t->rxrate = peek16(ep, O_RXRATE);
			note_triple(t, triple);
			if (b->mst != cur_state[ep][0]
			    || b->rxst != cur_state[ep][1]
			    || b->txst != cur_state[ep][2])
				t->moved++;
			cur_state[ep][0] = b->mst;
			cur_state[ep][1] = b->rxst;
			cur_state[ep][2] = b->txst;
			t->nonzero += nonzero[ep];

			{
				const char *p = text[ep];
				unsigned n = 0;

				for (; *p != '\0'; p++)
					if (*p == '\n')
						n++;
				b->lines = n;
				t->lines += n;
			}

			if (dump)
				printf("  %-9s blk %4d %-9s mst %3d rx %3d "
				       "tx %3d  mode %d  txq %4d rxq %4d  "
				       "obj %08x  lines %u  nz %d\n",
				       run_name[r], blk, ep_name[ep],
				       b->mst, b->rxst, b->txst, b->mode,
				       b->txcount, b->rxcount, b->objhash,
				       b->lines, b->nonzero);
		}
	}

	graph1[r] = graph_hash();
	for (ep = 0; ep < NEP; ep++) {
		tot[r][ep].rx_ours =
			is_ours_scrambler(scrambler_at(ep, RX_SCRAMBLE));
		tot[r][ep].tx_ours =
			is_ours_scrambler(scrambler_at(ep, TX_SCRAMBLE));
	}
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0u;
	ref_dsplibs_debug_level = 0u;
	signal(SIGALRM, prev);
}

/* --- comparing a run against the oracle ----------------------------------- */

static void
show(int r, int ep, int blk)
{
	const struct blockrec *b = &rec[r][ep][blk];

	printf("      %-9s %-9s block %4d: mst %d rxstate %d txstate %d  "
	       "mode %d  txq %d rxq %d  obj %08x  %u lines\n",
	       run_name[r], ep_name[ep], blk, b->mst, b->rxst, b->txst,
	       b->mode, b->txcount, b->rxcount, b->objhash, b->lines);
	if (b->head[0] != '\0')
		printf("        first of its transcript: %.80s\n", b->head);
}

static int bad_run = -1, bad_ep, bad_blk;

static int
compare_run(int r, int nblock)
{
	char msg[192];
	int ep, blk;
	int bad = 0;
	int agreed = 0;

	for (blk = 0; blk < nblock && !bad; blk++)
		for (ep = 0; ep < NEP; ep++) {
			const struct blockrec *a = &rec[RUN_BLOB][ep][blk];
			const struct blockrec *b = &rec[r][ep][blk];

			if (a->objhash == b->objhash
			    && a->txthash == b->txthash
			    && a->lines == b->lines
			    && a->mst == b->mst && a->rxst == b->rxst
			    && a->txst == b->txst && a->mode == b->mode
			    && a->nonzero == b->nonzero) {
				agreed++;
				continue;
			}

			printf("    %s diverges from %s at block %d, %s:\n",
			       run_name[r], run_name[RUN_BLOB], blk,
			       ep_name[ep]);
			show(RUN_BLOB, ep, blk);
			show(r, ep, blk);
			bad = 1;
			if (bad_run < 0 || blk < bad_blk) {
				bad_run = r;
				bad_ep = ep;
				bad_blk = blk;
			}

			snprintf(msg, sizeof(msg),
				 "%s vs %s: %s block %d microstate", run_name[r],
				 run_name[RUN_BLOB], ep_name[ep], blk);
			diff_eq_int(msg, b->mst, a->mst, blk);
			snprintf(msg, sizeof(msg),
				 "%s vs %s: %s block %d rxstate", run_name[r],
				 run_name[RUN_BLOB], ep_name[ep], blk);
			diff_eq_int(msg, b->rxst, a->rxst, blk);
			snprintf(msg, sizeof(msg),
				 "%s vs %s: %s block %d txstate", run_name[r],
				 run_name[RUN_BLOB], ep_name[ep], blk);
			diff_eq_int(msg, b->txst, a->txst, blk);
			snprintf(msg, sizeof(msg),
				 "%s vs %s: %s block %d mode", run_name[r],
				 run_name[RUN_BLOB], ep_name[ep], blk);
			diff_eq_int(msg, b->mode, a->mode, blk);
			snprintf(msg, sizeof(msg),
				 "%s vs %s: %s block %d object", run_name[r],
				 run_name[RUN_BLOB], ep_name[ep], blk);
			diff_eq_int(msg, b->objhash, a->objhash, blk);
			snprintf(msg, sizeof(msg),
				 "%s vs %s: %s block %d transcript",
				 run_name[r], run_name[RUN_BLOB], ep_name[ep],
				 blk);
			diff_eq_int(msg, b->txthash, a->txthash, blk);
			snprintf(msg, sizeof(msg),
				 "%s vs %s: %s block %d diagnostic lines",
				 run_name[r], run_name[RUN_BLOB], ep_name[ep],
				 blk);
			diff_eq_int(msg, b->lines, a->lines, blk);
			snprintf(msg, sizeof(msg),
				 "%s vs %s: %s block %d non-zero tx samples",
				 run_name[r], run_name[RUN_BLOB], ep_name[ep],
				 blk);
			diff_eq_int(msg, b->nonzero, a->nonzero, blk);
			break;
		}

	snprintf(msg, sizeof(msg),
		 "%s: block-endpoint pairs that agree with %s", run_name[r],
		 run_name[RUN_BLOB]);
	diff_eq_int(msg, agreed, nblock * NEP, r);

	/*
	 * AND THE 123 REGIONS THAT ARE NOT THE TWO OBJECTS.  The session
	 * block `V34SetINFO0aBits` writes through lives out there, and so do
	 * the V.90 sub-objects; a run that agreed on both roots and diverged
	 * in a sub-object would otherwise pass.  Nothing is excluded from
	 * this hash -- the two words that legitimately differ between runs
	 * are both in a root, which is why the roots are hashed separately.
	 */
	snprintf(msg, sizeof(msg),
		 "%s: the heap graph outside the two objects", run_name[r]);
	diff_eq_int(msg, graph1[r], graph1[RUN_BLOB], r);
	return bad;
}

static void
diagnose(int r, int ep, int blk)
{
	static unsigned char ref_obj[ROOT_LEN];
	const unsigned char *now;
	unsigned i, nbad = 0;

	printf("\n  replaying %s and %s to block %d to name the bytes:\n",
	       run_name[RUN_BLOB], run_name[r], blk);

	run_call(RUN_BLOB, blk + 1);
	memcpy(ref_obj, root[ep], sizeof(ref_obj));

	run_call(r, blk + 1);
	now = (const unsigned char *)root[ep];

	for (i = 0; i < ROOT_LEN; i++) {
		unsigned o = i - ROOT_V34;

		if (i >= ROOT_V34 && o < sizeof(struct v34_object)
		    && v34hs_in_hole(o))
			continue;
		if (now[i] == ref_obj[i])
			continue;
		if (nbad++ < 16)
			printf("    root +0x%05x (v34 object +0x%05x): "
			       "%s %02x, %s %02x\n", i,
			       i >= ROOT_V34 ? o : 0, run_name[r], now[i],
			       run_name[RUN_BLOB], ref_obj[i]);
	}
	printf("    %u root bytes differ at block %d, %s\n", nbad, blk,
	       ep_name[ep]);
	if (nbad == 0)
		printf("    ...so the difference is OUTSIDE the root: one of "
		       "the other 123 heap regions, or the transcript alone\n");
}

/* --- the anti-vacuity claims ---------------------------------------------- */

/*
 * MEASURED FROM THE BLOB-BLOB RUN, exact rather than a floor, and per
 * ENDPOINT rather than for the pair.  Two calls that both go nowhere agree
 * perfectly, and one dead endpoint must not be able to hide behind a live
 * one.  `V34CONN_DUMP=1` prints them.
 */
struct claim {
	unsigned	lines;
	int		distinct;
	int		moved;
	int		nonzero;
	int		iterated;
	int		connected;
	unsigned	seed;
	unsigned	final;
	int		finalmode;
	short		txrate, rxrate;
};

static const struct claim expect[NEP] = {
	/*
	 * originate.  Construction leaves microstate 41 DET_SYNC, rxstate 43
	 * RX_DPSK, txstate 54 SILENCEINFO -- exactly the three transitions
	 * the blob narrates at the end of `VPcmV34Create` (finding F801).
	 * From there, and every block number here is a measurement:
	 *
	 *     13   txstate    24 TX_DPSK
	 *    181   microstate 44 DET_INFO
	 *    226   txstate    60 TONE_AB
	 *    312   "V34INFO, rxinfo0 0xff,0x84,0x47,0xec,0,0,0,0,0,0"
	 *          microstate 58 RX_PHASE1_CALL
	 *    821   "V34RETRAIN, RX_PHASE1_CALL received, count2=509"
	 *          microstate 55 TX_PHASE1_CALL
	 *    872   microstate 59 RX_PHASE2_CALL
	 *    914   txstate     5 SILENCE
	 *   1140   "Repeated info0 is detected, errorrecovery is initialized
	 *          in RX_PHASE2_CALL" -- back to 41 DET_SYNC and txstate 24
	 *
	 * and from there it cycles 41 DET_SYNC / 44 DET_INFO for ever.  Eight
	 * distinct triples and thirteen blocks in which one of the three
	 * moved.  Finding F902 for what that says and what it does not.
	 */
	{ 25, 8, 13, 4072, 574, 0, TRIPLE(41, 43, 54), TRIPLE(44, 43, 24),
	  MODE_HANDSHAKE, 0, 0 },
	/*
	 * answer: 41 DET_SYNC -> txstate 24 TX_DPSK at 13, 44 DET_INFO at
	 * 181, txstate 60 TONE_AB at 226, 46 TX_PHASE1_ANS at 312, 49
	 * RX_PHASE1_ANS at 712, "On RX_PHASE1_ANS: is short=0, bulkDelay=500,
	 * filtDelay=45" and 47 TX_PHASE2_ANS at 981, and at 1023 txstate 24
	 * TX_DPSK with the microstate back at 41.  Then the same 41/44 cycle.
	 *
	 * Five microstates each and a symmetric shape -- 58/55/59 on the
	 * calling side against 46/49/47 on the answering one -- and seven
	 * distinct triples against the originator's eight, because only the
	 * originator's 59 RX_PHASE2_CALL is seen with two different txstates
	 * (60 TONE_AB, then 5 SILENCE at block 914).
	 */
	{ 20, 7, 10, 6320, 407, 0, TRIPLE(41, 43, 54), TRIPLE(44, 43, 24),
	  MODE_HANDSHAKE, 0, 0 }
};

static void
check_totals(int r)
{
	char msg[192];
	int ep;

	for (ep = 0; ep < NEP; ep++) {
		const struct total *t = &tot[r][ep];

		snprintf(msg, sizeof(msg), "%s %s: diagnostic lines",
			 run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->lines, expect[ep].lines, r);
		snprintf(msg, sizeof(msg), "%s %s: distinct state triples",
			 run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->distinct, expect[ep].distinct, r);
		snprintf(msg, sizeof(msg), "%s %s: blocks in which the state "
			 "moved", run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->moved, expect[ep].moved, r);
		snprintf(msg, sizeof(msg), "%s %s: non-zero transmit samples",
			 run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->nonzero, expect[ep].nonzero, r);

		/*
		 * THE HANDSHAKE LOOP RAN.  Finding F805's vacuous passes are
		 * what this is for: a constructed object comes out with the
		 * transmit queue above the block's limit, so the first blocks
		 * drain it and `datapumpv34` does nothing at all.  A count of
		 * the blocks in which the pump RAISED the queue is a count of
		 * the blocks in which `v34handshak` was actually called.
		 */
		snprintf(msg, sizeof(msg), "%s %s: blocks in which the pump "
			 "entered the handshake loop", run_name[r],
			 ep_name[ep]);
		diff_eq_int(msg, t->iterated, expect[ep].iterated, r);

		snprintf(msg, sizeof(msg), "%s %s: the triple construction "
			 "left", run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->seed, expect[ep].seed, r);
		snprintf(msg, sizeof(msg), "%s %s: the triple the call ended "
			 "on", run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->final, expect[ep].final, r);
		snprintf(msg, sizeof(msg), "%s %s: the call left the state it "
			 "started in", run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->final != t->seed, 1, r);

		/*
		 * AND WHETHER IT CONNECTED, WHICH IS NOT LEFT IMPLICIT.
		 *
		 * `v34handshak` writes 1 into +0x2218 at 0x63d4d, five
		 * instructions after printing both connection rates, and
		 * `datapumpv34` then takes the DATA branch.  So mode 1 IS the
		 * connection and these three claims are the whole question:
		 * how many blocks left mode 2, what the mode was at the end,
		 * and what the two rate words hold.
		 *
		 * They are asserted as the exact NEGATIVE result this batch
		 * measured.  A call that starts connecting fails this test,
		 * which is the intent: it is a recorded frontier and the
		 * failure names the mode and both rates.
		 */
		snprintf(msg, sizeof(msg), "%s %s: blocks in which the mode "
			 "word left handshaking", run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->connected, expect[ep].connected, r);
		snprintf(msg, sizeof(msg), "%s %s: the mode word at the end "
			 "(1 would be CONNECTED)", run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->finalmode, expect[ep].finalmode, r);
		snprintf(msg, sizeof(msg), "%s %s: the transmit rate the call "
			 "settled on, in units of 2400", run_name[r],
			 ep_name[ep]);
		diff_eq_int(msg, t->txrate, expect[ep].txrate, r);
		snprintf(msg, sizeof(msg), "%s %s: the receive rate the call "
			 "settled on, in units of 2400", run_name[r],
			 ep_name[ep]);
		diff_eq_int(msg, t->rxrate, expect[ep].rxrate, r);

		/*
		 * The two stores, checked rather than assumed.  In a run
		 * where this endpoint is ours BOTH callbacks must be ours; in
		 * one where it is the blob's, NEITHER may be -- otherwise the
		 * restore did not put the blob's back and every later run is
		 * quietly running a mixture.
		 */
		snprintf(msg, sizeof(msg), "%s %s: the receive scrambler "
			 "callback was still ours when the run ended",
			 run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->rx_ours, run_ours[r][ep], r);
		snprintf(msg, sizeof(msg), "%s %s: the transmit scrambler "
			 "callback was still ours when the run ended",
			 run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->tx_ours, run_ours[r][ep], r);
	}

	/*
	 * THE TWO ENDPOINTS ARE NOT THE SAME MODEM TWICE.  They are built by
	 * the same constructor from the same configuration and differ only in
	 * `caller`, so a role that failed to take would leave two identical
	 * originators talking past each other and every claim above would
	 * still hold.
	 */
	snprintf(msg, sizeof(msg),
		 "%s: the two endpoints did not run the same call", run_name[r]);
	diff_eq_int(msg, rec[r][0][NBLOCK - 1].objhash
		    != rec[r][1][NBLOCK - 1].objhash, 1, r);

	/*
	 * AND THE RESTORE WAS EXACT.  Every run starts from the same graph;
	 * if it did not, the run-to-run comparison above is comparing two
	 * different modems and would fail for a reason that is the fixture's.
	 * Stated here so that when it does fail it says so in one line.
	 */
	snprintf(msg, sizeof(msg),
		 "%s: the graph this run started from", run_name[r]);
	diff_eq_int(msg, graph0[r], graph0[RUN_BLOB], r);
}

/* --- main ----------------------------------------------------------------- */

int
main(void)
{
	struct dp_operations *ops;
	int rc = 0;
	int r, ep;
	int a0, f0;

	dump = getenv("V34CONN_DUMP") != NULL;
	if (dump)
		setvbuf(stdout, NULL, _IONBF, 0);

	harness_alloc_reset();
	harness_param_reset();

	/* --- construction, once, and every number it produces ---------- */

	diff_begin("the blob's V.34 constructor, as a fixture");
	ops = vpcm_ops();
	diff_eq_int("dp_vpcm_init registered id 34", ops != 0, 1, 0);
	if (ops == 0 || ops->create == 0 || ops->name == 0)
		return diff_end();
	diff_eq_int("...under the name VPCM", strcmp(ops->name, "VPCM") == 0,
		    1, 0);

	/*
	 * DIAGNOSTICS ON FOR THE CONSTRUCTION TOO, so both sides take the
	 * same branches throughout.  A level that changes between the
	 * constructor and the call would make the object the runs start from
	 * a different object from the one the runs are driving.
	 */
	dsplib_debug_capture_on = 1;
	dsplibs_debug_level = 2u;
	ref_dsplibs_debug_level = 2u;
	dsplib_debug_capture_reset();
	for (ep = 0; ep < NEP; ep++) {
		char msg[96];

		snprintf(msg, sizeof(msg), "%s: the constructor returned an "
			 "object", ep_name[ep]);
		diff_eq_int(msg, build(ops, ep), 1, ep);
	}
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0u;
	ref_dsplibs_debug_level = 0u;
	if (root[0] == 0 || root[1] == 0)
		return diff_end();

	/*
	 * The allocation accounting, as exact literals -- 127 allocations per
	 * endpoint of which 2 are freed inside `create`, 279,600 bytes asked
	 * for, 125 regions left live (finding F800).  A constructor that
	 * silently took a different branch would move these before it moved
	 * anything else.
	 */
	diff_eq_int("allocations for two constructions", harness_alloc.allocs,
		    256, 0);
	diff_eq_int("...freed inside create", harness_alloc.frees, 4, 0);
	diff_eq_int("...live afterwards", harness_alloc.live, 252, 0);
	diff_eq_int("...bytes asked for", (long)harness_alloc.bytes, 559472, 0);
	diff_eq_int("...and no bad free", harness_alloc.bad_free, 0, 0);
	diff_eq_int("the two roots are distinct", root[0] != root[1], 1, 0);
	/*
	 * 254 and 559200 were the numbers before the runtime block moved to
	 * the heap; the extra two allocations and 272 bytes are the two
	 * `dp_runtime_create` blocks at 0x88 each, which is the whole of the
	 * difference and is checked as such rather than left implicit.
	 */
	diff_eq_int("...of which the two runtime blocks are 0x88 each",
		    (long)harness_alloc.bytes - 559200, 2 * 0x88, 0);

	/*
	 * WHERE EVERY CONFIGURATION PARAMETER LANDS IN THE CONSTRUCTED
	 * OBJECT.  This is the block that turns "the configuration is
	 * plausible" into "the configuration is derived": each claim reads a
	 * field of the object the BLOB's constructor wrote and compares it
	 * with what `vpcm_create`'s disassembly says should be there.
	 *
	 * The two rate claims are the interesting ones and they say the
	 * OPPOSITE of what the parameter names suggest.  MDMPRM_MIN_RATE and
	 * MDMPRM_MAX_RATE go to +0x30 and +0x34 and are never read again by
	 * anything V.34; the pair `V90Parameters::setToDefault` divides by
	 * 2400 to get a rate index is +0x38 and +0x3c, and `vpcm_create`
	 * writes those as the LITERALS 4800 and 33600 on both arms.  So the
	 * host's rate window does not reach the rate machinery at all, which
	 * is why sweeping it changes nothing (finding F824).
	 */
	for (ep = 0; ep < NEP; ep++) {
		char msg[128];
		struct _tagModemParameters *rt = runtime[ep];

		snprintf(msg, sizeof(msg), "%s: +0x24 is our dsp_info",
			 ep_name[ep]);
		diff_eq_int(msg, peek_root(ep, O_ROOT_DSPINFO)
			    == (int)(size_t)&dspinfo[ep], 1, ep);
		snprintf(msg, sizeof(msg), "%s: +0x28 is our runtime block",
			 ep_name[ep]);
		diff_eq_int(msg, peek_root(ep, O_ROOT_RUNTIME)
			    == (int)(size_t)rt, 1, ep);

		snprintf(msg, sizeof(msg),
			 "%s: MDMPRM_MIN_RATE reached +0x30 (%%ld)", ep_name[ep]);
		diff_eq_int(msg, (long)rt->vpcmRateLimitLow, CFG_MIN_RATE, ep);
		snprintf(msg, sizeof(msg),
			 "%s: MDMPRM_MAX_RATE reached +0x34 (%%ld)", ep_name[ep]);
		diff_eq_int(msg, (long)rt->vpcmRateLimitHigh, CFG_MAX_RATE, ep);
		snprintf(msg, sizeof(msg), "%s: ...and was not clamped, because "
			 "MODEM_MAX_RATE is the clamp (%%ld)", ep_name[ep]);
		diff_eq_int(msg, CFG_MAX_RATE, 0xdac0, ep);

		snprintf(msg, sizeof(msg), "%s: +0x38 is the literal 4800 "
			 "whatever the host said (%%ld)", ep_name[ep]);
		diff_eq_int(msg, (long)rt->minRate, 4800, ep);
		snprintf(msg, sizeof(msg), "%s: +0x3c is the literal 33600 "
			 "(%%ld)", ep_name[ep]);
		diff_eq_int(msg, (long)rt->maxRate, 33600, ep);

		snprintf(msg, sizeof(msg),
			 "%s: HW delay is MDMPRM_IODELAY + 4 (%%ld)", ep_name[ep]);
		diff_eq_int(msg, rt->hwDelay, CFG_IODELAY + 4, ep);
		snprintf(msg, sizeof(msg),
			 "%s: DMA delay is that less 48 (%%ld)", ep_name[ep]);
		diff_eq_int(msg, rt->dmaDelay, CFG_IODELAY + 4 - 48, ep);
		snprintf(msg, sizeof(msg), "%s: ...so the delay correction at "
			 "root +0xd254 stayed 0 (%%ld)", ep_name[ep]);
		diff_eq_int(msg, peek_root(ep, O_ROOT_D254), 0, ep);

		/*
		 * `paramFile` NULLED AT 0x3ad2.  This is the store that
		 * faulted on the harness's default answer and is the reason
		 * DPRUNTIME had to be an address; asserting it makes the
		 * derivation's central claim visible rather than implied.
		 */
		snprintf(msg, sizeof(msg),
			 "%s: the constructor NULLed paramFile", ep_name[ep]);
		diff_eq_int(msg, rt->paramFile == 0, 1, ep);

		/*
		 * The session type is V.34, so `vpcm_create` CLEARS runtime
		 * +2 bit 4 that `dp_runtime_create` had set -- and root
		 * +0xd250 is 0x210 exactly when that bit is clear.
		 */
		snprintf(msg, sizeof(msg),
			 "%s: session type V.34 cleared runtime +2 bit 4",
			 ep_name[ep]);
		diff_eq_int(msg, rt->qcFlags & 0x10, 0, ep);
		snprintf(msg, sizeof(msg),
			 "%s: ...so root +0xd250 is 0x210 (%%ld)", ep_name[ep]);
		diff_eq_int(msg, peek_root(ep, O_ROOT_D250), 0x210, ep);

		/*
		 * Untouched by the datapump: it is the host's to keep.  The
		 * LITERAL 4 and not CFG_CODECTYPE, because a claim written
		 * against the same macro the fixture feeds in cannot fail --
		 * changing the macro changes both sides.  Against the literal
		 * it fails if the fixture ever stops setting the parameter,
		 * which is the failure it exists to catch.
		 */
		snprintf(msg, sizeof(msg), "%s: codecType survived (%%ld)",
			 ep_name[ep]);
		diff_eq_int(msg, rt->codecType, 4, ep);
	}

	for (ep = 0; ep < NEP; ep++) {
		char msg[128];

		snprintf(msg, sizeof(msg), "%s: the role flag `caller` set",
			 ep_name[ep]);
		diff_eq_int(msg, peek16(ep, O_ROLE), ep_role[ep], ep);
		snprintf(msg, sizeof(msg),
			 "%s: the mode word the constructor left", ep_name[ep]);
		diff_eq_int(msg, peek32(ep, O_MODE), 0, ep);
		snprintf(msg, sizeof(msg),
			 "%s: the transmit queue the constructor left",
			 ep_name[ep]);
		diff_eq_int(msg, peek16(ep, O_TXCOUNT), 32, ep);
		snprintf(msg, sizeof(msg), "%s: ...against the block's limit",
			 ep_name[ep]);
		diff_eq_int(msg, peek16(ep, O_TXLIMIT), 16, ep);
		/*
		 * BOTH SCRAMBLER CALLBACKS POINT AT THE BLOB, before anything
		 * repoints them.  This is the measurement finding F802 made,
		 * asserted rather than quoted: if the constructor stopped
		 * installing them the substitution below would be replacing
		 * nothing and the fixture would prove nothing.
		 */
		snprintf(msg, sizeof(msg), "%s: the constructor's receive "
			 "scrambler is not ours", ep_name[ep]);
		diff_eq_int(msg,
			    is_ours_scrambler(scrambler_at(ep, RX_SCRAMBLE)),
			    0, ep);
		snprintf(msg, sizeof(msg), "%s: ...nor its transmit one",
			 ep_name[ep]);
		diff_eq_int(msg,
			    is_ours_scrambler(scrambler_at(ep, TX_SCRAMBLE)),
			    0, ep);
	}
	rc |= diff_end();

	/* --- the snapshot, and that it is live ------------------------- */

	diff_begin("the heap graph, snapshotted so four runs are congruent");
	graph_take();
	/*
	 * 252, not the 250 finding F900 recorded: the two `dp_runtime_create`
	 * blocks are now IN the graph, which is the point of allocating them
	 * rather than declaring them static.
	 */
	diff_eq_int("live regions", nreg, 252, 0);
	{
		unsigned h0 = graph_hash();
		unsigned poked;
		int last = nreg - 1;

		/*
		 * A HASH NOBODY HAS SEEN FIRE IS NOT A HASH -- gates.md rule
		 * 3.  One byte in the last region, and back.
		 */
		if (is_root(reg[last]))
			last--;
		((unsigned char *)reg[last])[0] ^= 0xff;
		poked = graph_hash();
		((unsigned char *)reg[last])[0] ^= 0xff;
		diff_eq_int("one byte anywhere in it changes the hash",
			    poked != h0, 1, 0);
		diff_eq_int("and putting it back restores the hash",
			    graph_hash(), h0, 0);
		graph_restore();
		diff_eq_int("a restore leaves the same hash", graph_hash(), h0,
			    0);
	}
	rc |= diff_end();

	/* --- the call, four ways --------------------------------------- */

	diff_begin("v34 call on two blob-constructed endpoints");
	a0 = harness_alloc.allocs;
	f0 = harness_alloc.frees;

	for (r = 0; r < NRUN; r++)
		run_call(r, NBLOCK);

	/*
	 * NOTHING WAS ALLOCATED OR FREED BY THE CALL, so the snapshot set is
	 * still the whole graph.  Had the handshake allocated, every run
	 * after the first would have been restoring a stale set and the
	 * comparison would be over the wrong memory.
	 */
	diff_eq_int("the call allocated nothing", harness_alloc.allocs, a0, 0);
	diff_eq_int("...and freed nothing", harness_alloc.frees, f0, 0);

	if (dump) {
		printf("\n  totals, per run per endpoint:\n");
		for (r = 0; r < NRUN; r++)
			for (ep = 0; ep < NEP; ep++)
				printf("    %-9s %-9s lines %5u  distinct %3d"
				       "  moved %4d  nonzero %5d  iterated %4d"
				       "  connected %d  mode %d  seed %06x  "
				       "final %06x  rates %d/%d\n",
				       run_name[r], ep_name[ep],
				       tot[r][ep].lines, tot[r][ep].distinct,
				       tot[r][ep].moved, tot[r][ep].nonzero,
				       tot[r][ep].iterated,
				       tot[r][ep].connected,
				       tot[r][ep].finalmode, tot[r][ep].seed,
				       tot[r][ep].final, tot[r][ep].txrate,
				       tot[r][ep].rxrate);
		printf("\n");
	}

	for (r = 0; r < NRUN; r++) {
		if (r != RUN_BLOB)
			compare_run(r, NBLOCK);
		check_totals(r);
	}

	if (bad_run >= 0)
		diagnose(bad_run, bad_ep, bad_blk);

	rc |= diff_end();

	/*
	 * --- teardown, and exactly how much of it ---------------------
	 *
	 * `dp_runtime_create` and `dp_runtime_delete` are a PAIR in the host
	 * (`modem.c:1136` and `:1197`), so the fixture pairs them: a test that
	 * modelled only the create half would be asserting a leak as correct.
	 *
	 * THE 250 V.PCM REGIONS ARE DELIBERATELY NOT TORN DOWN, and that is
	 * not an oversight.  `ops->delete` frees the whole graph, every
	 * `reg[]` entry then dangles, and the run above has already finished
	 * with it -- so calling it would test the destructor, which is a
	 * different test with a different fixture, while adding a window in
	 * which this one's snapshot machinery points at freed memory.  Finding
	 * F800 measured that `->delete` balances to `live=0, bad_free=0`; that
	 * is where the claim belongs.
	 */
	diff_begin("the host's own blocks are freed by the host's own free");
	{
		int f0 = harness_alloc.frees;

		for (ep = 0; ep < NEP; ep++)
			ref_dp_runtime_delete(runtime[ep]);
		diff_eq_int("two runtime blocks freed (%ld)",
			    harness_alloc.frees - f0, 2, 0);
		diff_eq_int("no bad free (%ld)", harness_alloc.bad_free, 0, 0);
		/*
		 * And the V.PCM graph is still outstanding, which is the
		 * statement above made checkable rather than left in a
		 * comment.
		 */
		diff_eq_int("the 250 V.PCM regions are still live (%ld)",
			    harness_alloc.live, 250, 0);
	}
	rc |= diff_end();

	return rc;
}
