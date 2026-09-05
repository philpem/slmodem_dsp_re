/*
 * t_v34call.c -- a whole V.34 call between TWO instances, run four ways.
 *
 * Every other test of `v34handshak` in this tree drives one function, or one
 * dispatch case, and compares.  None of them exercises the transitions
 * BETWEEN cases, the ordering, or what a sequence of blocks leaves behind --
 * and a reconstruction assembled case by case is most likely to be wrong
 * exactly there, while every unit still passes.  This is that oracle.
 *
 * WHAT IT IS.  Two V.34 objects, one originating and one answering, wired to
 * each other sample by sample and stepped block by block through
 * `datapumpv34`.  The whole call is then run FOUR times over the same memory:
 *
 *     ours-ours    both endpoints this tree's code
 *     blob-blob    both endpoints the blob's                  <- the oracle
 *     ours-blob    originate ours, answer the blob's
 *     blob-ours    originate the blob's, answer ours
 *
 * and every block of every run is compared against the same block of the
 * blob-blob run: the object byte for byte (as a hash, with the pointer skips
 * excluded), the three state words, the block's transcript and its line
 * count.  A divergence is reported with its BLOCK NUMBER and both endpoints'
 * state triples, because "the transcripts differ" after four hundred blocks
 * is not a finding and "block 37, the originator went 44 -> 62 where the blob
 * went 44 -> 41" is.
 *
 * THE MIXED RUNS ARE THE STRONGER SHAPE and they are not decoration.  A
 * homogeneous run can agree with another homogeneous run while both endpoints
 * are wrong in the same direction; a mixed run puts our code and the blob's
 * on the two ends of ONE call, so each side's output has to be acceptable to
 * the other's input as well as merely equal to it.
 *
 * WHY THIS FIXTURE.  Findings F319-322: what an object step depends on is the
 * geometry of the five blocks the object points at, and building a second
 * fixture is building that geometry a second time and being wrong about it
 * once.  `test/harness/v34hsstep.c` already has it -- two congruent arenas at
 * a stride this tree chooses -- so the two endpoints are arena A and arena B,
 * and the four runs reuse the same two arenas one after another.  That makes
 * the run-to-run comparison exact rather than approximate: it is literally
 * the same memory at the same addresses.  Finding F320 already established
 * that nothing is carried between calls, which is what makes four sequential
 * runs sound.
 *
 * THE THING THIS FIXTURE IS NOT USED FOR is `v34hs_compare`.  Side A and side
 * B are two ENDPOINTS here and not two implementations, so they hold
 * different objects by design and comparing them would fail by construction.
 * What is compared is run against run.
 *
 * HOW A CALL IS DRIVEN, read off the object rather than invented:
 *
 *   per sample   `obj->echo_residual` is the sample off the line and `obj->tx_sample` the
 *                sample onto it -- v34fsk.h names the pair, and
 *                `modem_serrint` reads the first, cancels the echo, pushes
 *                the residual onto the receive queue, and pops the next
 *                transmit sample into the second.  The wire is one sample of
 *                delay each way and nothing else: no attenuation, no noise.
 *   per block    `datapumpv34`, whose +0x2218 > 1 branch calls `v34handshak`
 *                until the transmit block is full AND the receive queue is
 *                drained.
 *
 * FOUR SAMPLES A BLOCK.  `v34modeminit` leaves the block's sample limit at
 * +0x2aa0 at six and `txinit` leaves the transmit queue holding 0x20, so four
 * is the largest count that cannot take the queue below the limit in one
 * block whichever limit is in force (the handshake also uses 0x10).  It is
 * also the granularity `rxreadqueue` drains at.
 *
 * AND +0x2218.  `v34handshakinit` clears it, so `datapumpv34` would take the
 * DATA branch; the object's own C++ half writes 2 there immediately after
 * calling `v34handshakinit` (src/pump/v34/v34pcmmain.cpp), so 2 is the
 * object's value for "handshaking" and not this test's invention.
 *
 * THIS IS THE FIRST THING IN THE TREE THAT DRIVES `datapumpv34`'s HANDSHAKE
 * LOOP.  Finding F452 recorded that the loop could not be entered because no
 * written arm advanced the transmit cursor or drained the receive queue.  The
 * table-1 arms are wired into `v34handshak` now, so it can be, and it is.
 *
 * VACUITY.  Two calls that both go nowhere produce two identical empty
 * transcripts and compare equal.  Every claim at the bottom of this file is
 * an EXACT recorded number measured from the blob-blob run, per endpoint, not
 * an inequality: distinct state triples visited, transcript lines, blocks in
 * which the state moved, non-zero transmit samples.  One dead endpoint cannot
 * hide behind a live one.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "harness.h"
#include "v34hsstep.h"

#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34rx.h"

extern void ref_datapumpv34(void *obj);
extern int ref_modem_serrint(void *obj);
extern void ref_v34handshakinit(void *obj, int mode);
extern void ref_V34InitializeImplementationSpecific(void *obj);

/* --- the object, by offset, so this file reads as the disassembly does ----- */

#define O_F25E		0x025e	/* short: the sample this step puts on the line */
#define O_F260		0x0260	/* short: the sample this step takes off it     */
#define O_RXCOUNT	0x0264	/* short: the receive queue's count             */
#define O_MODE		0x2218	/* int:   above 1 is the handshake              */
#define O_TXCOUNT	0x221c	/* short: the transmit queue's count            */
#define O_TXLIMIT	0x2aa0	/* short: how many samples the block wants      */
#define O_MICROSTATE	0x3592
#define O_RXSTATE	0x3594
#define O_TXSTATE	0x3596
#define O_ROLE		0x359c	/* short: 0x65 originate, 0x66 answer           */
#define O_SESSION	0x3548	/* the session block VPcmV34Main.cpp owns       */
#define O_V90RX		0x024c	/* int:   a V.90 receiver is running            */
#define O_K56RX		0x0250	/* int:   ...or a K.56Flex one                  */
#define O_LOCAL_V92	0xabc6	/* short                                        */
#define O_LOCAL_SHORT	0xabca	/* short                                        */

/*
 * THE SESSION BLOCK, WHICH IS NOT A MODEM FIELD AND HAS TO BE MADE PLAUSIBLE.
 *
 * `v34hs_setup` fills the block at +0x3548 with pseudorandom bytes, which is
 * right for the differential tests it was built for: an unwritten field must
 * not look deliberate (finding F230).  Two of that block's own fields are
 * POINTERS the object dereferences -- `V34SetINFO0aBits` reads `caps + 0x11`
 * and `V34GiveINFO0aBits` reads through both -- and a pseudorandom pointer is
 * a fault rather than a value.  The first run of this test faulted inside
 * `ref_V34SetINFO0aBits` at block 13 for exactly that reason.
 *
 * So the driver aims both inside the same block, at regions it then ZEROES.
 * Zero is deliberate here rather than a default: it is a plain V.34 call with
 * no V.92 capability offered, which is also why `v90_receiver` and
 * `k56flex_receiver` are cleared below.  The two addresses are each side's
 * OWN session block, never one address written into both -- findings F319-322.
 * The offsets are v34info.c's, cited there.
 */
#define SESSION_UPSTREAM 0x1760	/* pointer */
#define SESSION_VARIANT	0x6120	/* int: non-zero selects INFO0a over INFO0d */
#define SESSION_CAPS	0x612c	/* pointer */
#define SESSION_LEN	0x6200	/* the fixture's block size                 */
#define SESS_CAPS_AT	0x2000	/* where in it the two aimed blocks sit     */
#define SESS_UP_AT	0x2100
#define SESS_AIMED_LEN	0x0080

#define ROLE_ORIGINATE	0x65
#define ROLE_ANSWER	0x66

#define MODE_HANDSHAKE	2	/* what v34pcmmain.cpp writes after mode 0      */

/* --- the shape of a run --------------------------------------------------- */

#define NSAMP		4	/* samples per endpoint per block  */
#define NBLOCK		1600	/* blocks in a call                */
#define NEP		2	/* endpoints                       */

#define TEXTMAX		8192	/* one endpoint's transcript for one block */
#define TEXTHEAD	160	/* how much of it is kept for reporting   */

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
static const short ep_role[NEP] = { ROLE_ORIGINATE, ROLE_ANSWER };

/*
 * The three state words as one number, so "how many distinct states did this
 * endpoint visit" is a comparison of scalars.  Packed rather than hashed
 * because the three are printed back out of it when a claim fails.
 */
#define TRIPLE(m, r, t)	(((unsigned)(unsigned short)(m) << 16) \
			 | ((unsigned)(unsigned char)(r) << 8) \
			 | (unsigned)(unsigned char)(t))

/* What one endpoint did in one block. */
struct blockrec {
	unsigned	objhash;	/* the object, pointer skips excluded */
	unsigned	arenahash;	/* ...and everything it points at     */
	unsigned	txthash;	/* this block's transcript            */
	unsigned	lines;		/* diagnostic lines it printed        */
	short		mst, rxst, txst;
	short		txcount, rxcount;
	int		nonzero;	/* non-zero transmit samples          */
	char		head[TEXTHEAD];
};

static struct blockrec rec[NRUN][NEP][NBLOCK];

/* Totals, per run per endpoint, for the anti-vacuity claims. */
struct total {
	unsigned	lines;
	int		distinct;	/* distinct (mst, rxst, txst) triples */
	int		moved;		/* blocks in which the triple changed */
	int		nonzero;	/* non-zero transmit samples          */
	unsigned	padhash0;	/* the filler before the call ran     */
	unsigned	padhash;	/* ...and after it                    */
	unsigned	seed;		/* the triple the bring-up left        */
	unsigned	final;		/* ...and the one the call ended on    */
	unsigned	triple[NBLOCK];	/* the triples, for the distinct count */
	int		ntriple;
};

static struct total tot[NRUN][NEP];

static int dump;

/* --- reaching the two objects --------------------------------------------- */

static short
peek16(int ep, unsigned off)
{
	short v;

	memcpy(&v, (char *)v34hs_object(ep) + off, sizeof(v));
	return v;
}

static void
poke16(int ep, unsigned off, short v)
{
	memcpy((char *)v34hs_object(ep) + off, &v, sizeof(v));
}

static void
poke32(int ep, unsigned off, int v)
{
	memcpy((char *)v34hs_object(ep) + off, &v, sizeof(v));
}

/*
 * FNV-1a over the object with the thirty-seven pointer skips excluded.
 *
 * The skips are not tidiness here: our bring-up installs OUR library tables
 * and the blob's installs the blob's, so those fields hold two addresses of
 * two copies across two runs and no run-to-run comparison can say anything
 * about them.  `v34hs_in_hole` is the fixture's own list rather than a second
 * copy of it.
 */
static unsigned
objhash(int ep)
{
	const unsigned char *p = (const unsigned char *)v34hs_object(ep);
	unsigned h = 2166136261u;
	unsigned i;

	for (i = 0; i < sizeof(struct v34_object); i++) {
		if (v34hs_in_hole(i))
			continue;
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

/*
 * One endpoint's configuration: the role, the session block's two pointers
 * and the flags that keep this a plain V.34 call.  Everything here is set
 * before the bring-up that matters, because `v34modeminit` branches on the
 * role and `v34handshakinit`'s mode 0 calls it.
 */
static void
configure(int ep)
{
	char *o = (char *)v34hs_object(ep);
	char *sess;
	void *p;
	int zero = 0;

	memcpy(&sess, o + O_SESSION, sizeof(sess));

	memset(sess + SESS_CAPS_AT, 0, SESS_AIMED_LEN);
	memset(sess + SESS_UP_AT, 0, SESS_AIMED_LEN);
	p = sess + SESS_CAPS_AT;
	memcpy(sess + SESSION_CAPS, &p, sizeof(p));
	p = sess + SESS_UP_AT;
	memcpy(sess + SESSION_UPSTREAM, &p, sizeof(p));
	memcpy(sess + SESSION_VARIANT, &zero, sizeof(zero));

	poke16(ep, O_ROLE, ep_role[ep]);
	poke32(ep, O_V90RX, 0);
	poke32(ep, O_K56RX, 0);
	poke16(ep, O_LOCAL_V92, 0);
	poke16(ep, O_LOCAL_SHORT, 0);
}

/* --- the four implementations, chosen per endpoint per run ---------------- */

static int ep_ours[NEP];		/* set once per run */

static void
ep_bringup(int ep, int mode)
{
	void *o = v34hs_object(ep);

	if (ep_ours[ep]) {
		V34InitializeImplementationSpecific(o);
		v34handshakinit(o, mode);
	} else {
		ref_V34InitializeImplementationSpecific(o);
		ref_v34handshakinit(o, mode);
	}
}

static void
ep_serrint(int ep)
{
	if (ep_ours[ep])
		modem_serrint(v34hs_object(ep));
	else
		ref_modem_serrint(v34hs_object(ep));
}

static void
ep_pump(int ep)
{
	if (ep_ours[ep])
		datapumpv34(v34hs_object(ep));
	else
		ref_datapumpv34(v34hs_object(ep));
}

/* Our code logs to capture channel 0 and the blob's to channel 1. */
static int
ep_slot(int ep)
{
	return ep_ours[ep] ? 0 : 1;
}

/* --- the alarm ------------------------------------------------------------ */

/*
 * `datapumpv34`'s handshake loop is `while (txq.count < tx_fill_target || rxq.count >
 * 5) v34handshak(obj)`, and `v34handshak`'s own default arm inside the
 * per-sample loop is the loop bottom (finding F287, D59), so neither is
 * guaranteed to terminate.  Outside `v34hs_step` this file has to arm its own
 * alarm or a divergence becomes a `make phase` that hangs and reports
 * nothing.
 */
static volatile int cur_run, cur_block, cur_ep;
static volatile int cur_state[NEP][3];

static void
call_alarm(int sig)
{
	char msg[256];
	int n;

	(void)sig;
	n = snprintf(msg, sizeof(msg),
		     "\nt_v34call: %s block %d endpoint %s did not return.\n"
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

/*
 * Append whatever the last capture window holds to this block's transcript.
 * Overflow is a hard failure rather than a truncation: a truncated transcript
 * that still hashes consistently across runs would compare equal for a reason
 * that has nothing to do with the object.
 */
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
	/*
	 * One block records at most one new triple, so `NBLOCK` entries is
	 * exactly enough and running out means the loop bound moved.  The cap
	 * used to be silent, with `distinct++` OUTSIDE it -- which would have
	 * counted insert failures as new states, and read as a check while
	 * measuring the array's length.
	 */
	if (t->ntriple >= NBLOCK) {
		printf("FIXTURE: more than %d distinct state triples\n", NBLOCK);
		exit(1);
	}
	t->triple[t->ntriple++] = triple;
	t->distinct++;
}

/*
 * Run one whole call and record every block.  `nblock` is a parameter so the
 * diagnosis path below can replay a run up to the block that diverged.
 */
static void
run_call(int r, int nblock)
{
	static char text[NEP][TEXTMAX];
	void (*prev)(int);
	short line[NEP];
	int ep, blk, i;

	cur_run = r;
	for (ep = 0; ep < NEP; ep++) {
		ep_ours[ep] = run_ours[r][ep];
		memset(&tot[r][ep], 0, sizeof(tot[r][ep]));
	}

	/*
	 * The bring-up inside `v34hs_setup` has to match the run, or the two
	 * runs differ in what they leave in the fields the second bring-up
	 * below does not touch.  `v34hs_refinit` moves side A to the blob and
	 * `v34hs_oursinit` moves side B to ours; between them they express all
	 * four combinations.
	 */
	v34hs_refinit(!ep_ours[0]);
	v34hs_oursinit(ep_ours[1]);
	v34hs_setup(0);
	v34hs_debug(1);

	/*
	 * AND THEN THE ROLE, WHICH THE FIXTURE'S BRING-UP CANNOT KNOW.
	 * `v34modeminit` branches on +0x359c and `v34handshakinit`'s mode 0
	 * calls it, so the flag has to be in place before the bring-up that
	 * matters.  Running mode 0 a second time is what the object itself
	 * does on every retrain, so this is not a fixture-only path.
	 */
	for (ep = 0; ep < NEP; ep++) {
		configure(ep);
		ep_bringup(ep, 0);
		poke32(ep, O_MODE, MODE_HANDSHAKE);
		line[ep] = 0;
		cur_state[ep][0] = peek16(ep, O_MICROSTATE);
		cur_state[ep][1] = peek16(ep, O_RXSTATE);
		cur_state[ep][2] = peek16(ep, O_TXSTATE);
		tot[r][ep].seed = TRIPLE(cur_state[ep][0], cur_state[ep][1],
					 cur_state[ep][2]);
		tot[r][ep].padhash0 = v34hs_padding_hash(ep);
	}

	prev = signal(SIGALRM, call_alarm);

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
		 * THE WIRE.  One sample of delay each way, both directions at
		 * once: each endpoint is handed what the other put on the line
		 * during the previous sample instant.  The capture is reset
		 * around every single call because in a homogeneous run both
		 * endpoints write the same channel, and a transcript nobody
		 * can attribute to an endpoint cannot name which one diverged.
		 */
		for (i = 0; i < NSAMP; i++) {
			short out[NEP];

			for (ep = 0; ep < NEP; ep++) {
				cur_ep = ep;
				dsplib_debug_capture_reset();
				poke16(ep, O_F260, line[ep]);
				alarm(20);
				ep_serrint(ep);
				alarm(0);
				out[ep] = peek16(ep, O_F25E);
				if (out[ep] != 0)
					nonzero[ep]++;
				grab(text[ep], &len[ep], ep);
			}
			line[0] = out[1];
			line[1] = out[0];
		}

		for (ep = 0; ep < NEP; ep++) {
			struct blockrec *b = &rec[r][ep][blk];
			struct total *t = &tot[r][ep];
			unsigned triple;

			cur_ep = ep;
			dsplib_debug_capture_reset();
			alarm(20);
			ep_pump(ep);
			alarm(0);
			grab(text[ep], &len[ep], ep);

			b->mst = peek16(ep, O_MICROSTATE);
			b->rxst = peek16(ep, O_RXSTATE);
			b->txst = peek16(ep, O_TXSTATE);
			b->txcount = peek16(ep, O_TXCOUNT);
			b->rxcount = peek16(ep, O_RXCOUNT);
			b->objhash = objhash(ep);
			b->arenahash = v34hs_arena_hash(ep);
			b->txthash = strhash(text[ep]);
			b->nonzero = nonzero[ep];
			snprintf(b->head, sizeof(b->head), "%s", text[ep]);

			triple = TRIPLE(b->mst, b->rxst, b->txst);
			t->final = triple;
			note_triple(t, triple);
			if (b->mst != cur_state[ep][0]
			    || b->rxst != cur_state[ep][1]
			    || b->txst != cur_state[ep][2])
				t->moved++;
			cur_state[ep][0] = b->mst;
			cur_state[ep][1] = b->rxst;
			cur_state[ep][2] = b->txst;
			t->nonzero += nonzero[ep];

			/*
			 * The line count is counted from the text rather than
			 * from `dsplib_debug_capture_lines`, which the reset
			 * above clears on every call.
			 */
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
				printf("  %-9s blk %3d %-9s mst %3d rx %3d "
				       "tx %3d  txq %4d rxq %4d  obj %08x  "
				       "lines %u  nz %d\n",
				       run_name[r], blk, ep_name[ep],
				       b->mst, b->rxst, b->txst,
				       b->txcount, b->rxcount, b->objhash,
				       b->lines, b->nonzero);
		}
	}

	for (ep = 0; ep < NEP; ep++)
		tot[r][ep].padhash = v34hs_padding_hash(ep);

	signal(SIGALRM, prev);
	v34hs_debug(0);
	v34hs_refinit(0);
	v34hs_oursinit(0);
}

/* --- comparing a run against the oracle ----------------------------------- */

static void
show(int r, int ep, int blk)
{
	const struct blockrec *b = &rec[r][ep][blk];

	printf("      %-9s %-9s block %3d: mst %d rxstate %d txstate %d  "
	       "txq %d rxq %d  obj %08x arena %08x  %u lines\n",
	       run_name[r], ep_name[ep], blk, b->mst, b->rxst, b->txst,
	       b->txcount, b->rxcount, b->objhash, b->arenahash, b->lines);
	if (b->head[0] != '\0')
		printf("        first of its transcript: %.80s\n", b->head);
}

/*
 * Compare one run against the blob-blob oracle, block by block, and stop at
 * the first block that differs.  Everything after a divergence is its
 * consequence, and a wall of differing blocks names nothing.
 */
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
			    && a->arenahash == b->arenahash
			    && a->txthash == b->txthash
			    && a->lines == b->lines
			    && a->mst == b->mst && a->rxst == b->rxst
			    && a->txst == b->txst
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
				 "%s vs %s: %s block %d object", run_name[r],
				 run_name[RUN_BLOB], ep_name[ep], blk);
			diff_eq_int(msg, b->objhash, a->objhash, blk);
			snprintf(msg, sizeof(msg),
				 "%s vs %s: %s block %d arena outside the "
				 "object", run_name[r], run_name[RUN_BLOB],
				 ep_name[ep], blk);
			diff_eq_int(msg, b->arenahash, a->arenahash, blk);
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

	/*
	 * THE COUNT, AND NOT A BARE `1 == 1`.  This used to assert that the
	 * comparison had not failed, which is a check that cannot fail and is
	 * indistinguishable from a comparison that ran over no blocks at all
	 * -- gates.md rule 1.  What it says now is how many of the
	 * `nblock * NEP` block-endpoint pairs agreed, so a comparison that
	 * stopped early, or one whose loop bound was wrong, fails on the
	 * number rather than passing silently.
	 */
	snprintf(msg, sizeof(msg),
		 "%s: block-endpoint pairs that agree with %s", run_name[r],
		 run_name[RUN_BLOB]);
	diff_eq_int(msg, agreed, nblock * NEP, r);
	return bad;
}

/*
 * NAME THE BYTES, by replaying both runs to the block that diverged.
 *
 * A hash says two blocks differ and nothing else, and "the transcripts
 * differ" after four hundred blocks is not a finding.  Both runs are
 * deterministic and both use the same memory, so the cheapest way to turn a
 * hash into an offset is to run each of them again as far as the block that
 * failed and compare what is left -- which is why `run_call` takes a block
 * count.  It costs one extra pair of runs and only on a failure.
 *
 * This RE-RUNS AND SO OVERWRITES `rec`, which is why it is called after every
 * comparison rather than inside one.
 */
static void
diagnose(int r, int ep, int blk)
{
	static unsigned char ref_obj[sizeof(struct v34_object)];
	const unsigned char *now;
	unsigned i, nbad = 0;

	printf("\n  replaying %s and %s to block %d to name the bytes:\n",
	       run_name[RUN_BLOB], run_name[r], blk);

	run_call(RUN_BLOB, blk + 1);
	memcpy(ref_obj, v34hs_object(ep), sizeof(ref_obj));

	run_call(r, blk + 1);
	now = (const unsigned char *)v34hs_object(ep);

	for (i = 0; i < sizeof(ref_obj); i++) {
		if (v34hs_in_hole(i) || now[i] == ref_obj[i])
			continue;
		if (nbad++ < 16)
			printf("    object +0x%05x: %s %02x, %s %02x\n", i,
			       run_name[r], now[i], run_name[RUN_BLOB],
			       ref_obj[i]);
	}
	printf("    %u object bytes differ at block %d, %s\n", nbad, blk,
	       ep_name[ep]);
	if (nbad == 0)
		printf("    ...so the difference is OUTSIDE the object: one of "
		       "the five blocks or the padding\n");
}

/* --- the anti-vacuity claims ---------------------------------------------- */

/*
 * MEASURED FROM THE BLOB, and exact rather than a floor.
 *
 * Two calls that both go nowhere agree perfectly, so every number here is a
 * literal read off the blob-blob run and asserted for all four runs and BOTH
 * endpoints separately -- a dead endpoint cannot hide behind a live one.
 * `V34CALL_DUMP=1` prints them.
 */
struct claim {
	unsigned	lines;
	int		distinct;
	int		moved;
	int		nonzero;
	unsigned	seed;
	unsigned	final;
};

static const struct claim expect[NEP] = {
	/*
	 * originate: microstate 41 DET_SYNC -> 44 DET_INFO -> 58 -> 55 -> 59
	 * -> 41 -> 44 and round again, with the transmit machine going 54
	 * SILENCEINFO -> 24 TX_DPSK -> 60 TONE_AB.  Fifteen state moves over
	 * seven distinct triples.
	 */
	{ 124, 7, 15, 4741, TRIPLE(41, 43, 54), TRIPLE(41, 43, 24) },
	/* answer: the same shape one microstate short of it. */
	{ 112, 6, 13, 6320, TRIPLE(41, 43, 54), TRIPLE(46, 43, 60) }
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
		 * AND THE PADDING, once per run rather than once per block.
		 * Finding F322 measured that a step writes no filler byte at
		 * all; this is that claim over a whole call, and it is the one
		 * check here that compares a run against the oracle rather
		 * than against a literal, because the fill is the fixture's
		 * and not the modem's.
		 */
		/*
		 * THE PADDING, BEFORE AGAINST AFTER.  Finding F322 measured
		 * that a single step writes no filler byte; this is the same
		 * claim over a whole call, and it is stated as before-versus-
		 * after rather than against the oracle run so that it is a
		 * real check for the oracle run too.  A write one element off
		 * the end of any of the five blocks lands here.
		 */
		snprintf(msg, sizeof(msg), "%s %s: the arena's seven filler "
			 "regions are untouched by the call", run_name[r],
			 ep_name[ep]);
		diff_eq_int(msg, t->padhash, t->padhash0, r);

		/*
		 * AND THE TWO ENDS OF THE TRAJECTORY, spelled out.  The seed
		 * is what `v34handshakinit`'s mode 0 leaves -- microstate 41
		 * DET_SYNC, rxstate 43 RX_DPSK, txstate 54 SILENCEINFO -- and
		 * the final triple is where the call had got to.  Asserting
		 * they DIFFER is the narrowest possible statement that the
		 * call went somewhere; asserting each exactly is what makes a
		 * shortened or re-routed call fail rather than merely look
		 * different.
		 */
		snprintf(msg, sizeof(msg), "%s %s: the triple the bring-up "
			 "left", run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->seed, expect[ep].seed, r);
		snprintf(msg, sizeof(msg), "%s %s: the triple the call ended "
			 "on", run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->final, expect[ep].final, r);
		snprintf(msg, sizeof(msg), "%s %s: the call left the state it "
			 "started in", run_name[r], ep_name[ep]);
		diff_eq_int(msg, t->final != t->seed, 1, r);
	}

	/*
	 * AND THE TWO ENDPOINTS ARE NOT THE SAME MODEM TWICE.  Both are
	 * brought up from the same fill in two congruent arenas and the only
	 * thing that separates them is +0x359c, so a role flag that failed to
	 * take would leave two identical originators talking past each other
	 * -- a call in shape and not in substance, and every claim above would
	 * still hold.  The two objects have to differ at the end.
	 */
	snprintf(msg, sizeof(msg),
		 "%s: the two endpoints did not run the same call", run_name[r]);
	diff_eq_int(msg, rec[r][0][NBLOCK - 1].objhash
		    != rec[r][1][NBLOCK - 1].objhash, 1, r);
}

/* --- main ----------------------------------------------------------------- */

int
main(void)
{
	int rc = 0;
	int r, ep;

	dump = getenv("V34CALL_DUMP") != NULL;
	if (dump)
		setvbuf(stdout, NULL, _IONBF, 0);

	diff_begin("v34 call: two instances, one originating and one answering");

	for (r = 0; r < NRUN; r++)
		run_call(r, NBLOCK);

	if (dump) {
		printf("\n  totals, per run per endpoint:\n");
		for (r = 0; r < NRUN; r++)
			for (ep = 0; ep < NEP; ep++)
				printf("    %-9s %-9s lines %5u  distinct %3d"
				       "  moved %4d  nonzero %5d\n",
				       run_name[r], ep_name[ep],
				       tot[r][ep].lines, tot[r][ep].distinct,
				       tot[r][ep].moved, tot[r][ep].nonzero);
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
	return rc;
}
