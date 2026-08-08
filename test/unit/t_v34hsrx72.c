/*
 * t_v34hsrx72.c -- `v34handshak`'s rxstate 72 `RX_L1`, 0x650c6.
 *
 * The last of the rxstate chain's six exits, and the last arm of the whole
 * function that was not written.  Driven case by case against the blob
 * through test/harness/v34hsstep.c with `v34hs_ours(1)` putting this tree's
 * `v34handshak` on side A, so every case is an ordinary tier-1 differential
 * comparison of the whole 44,096-byte object, the five blocks it points at,
 * the padding around them and both transcripts.
 *
 * WHAT THE ARM IS.  3,811 bytes over twenty-four ranges.  `+0xaa78` counts
 * blocks; while it is at or below 0x440 the arm runs `rxtiming` and then a
 * six-rung LADDER on that counter, each rung correlating the same burst
 * against one or both DFT banks.  Above 0x440 the probe is over: the
 * answering end finishes and hands over to phase 2's DPSK, the originating
 * end takes one more block and then watches the FSK demodulator for tone A.
 *
 * THE COUNTER IS POKEABLE IN SPITE OF FINDING 429, and the reason is worth
 * stating because it is the opposite of 66's case: the arm READS it, stores
 * `read + 1`, and only then branches on the value it stored.  So a poke of n
 * drives every branch below as n + 1, and it is the arm's master selector --
 * every case here sets it.
 *
 * AND THE FSK-GATED SUBTREE IS REACHABLE FROM A SEED, which the analysis
 * this file was written from said it was not.  `fsk.nbits` and `fsk.sr` are
 * both written by `fskdemodulate` on the same step, from a burst `V34agc`
 * has just rewritten -- but `obj->fsk_inhibit` at +0x402 makes
 * `fskdemodulate` return without doing anything at all, not even running the
 * detector (v34fsk.h, dpsk.c:210).  Nothing on this path writes it, so
 * setting it leaves the two fields exactly as poked and the gate at 0x65145
 * becomes an ordinary two-way choice.  That is what lets 0x6881e, 0x6afd7,
 * 0x6b120, 0x6c9f1, 0x7086b and 0x69723 -- 530 bytes -- be tested at all,
 * and it is why no part of this arm is guarded.  Finding 741.
 *
 * WHAT NO TRIAL CAN SEPARATE, and why that is not a gap.  0x6881e compares
 * the counter against `0xf10 + rtd/4`, `0xf14 + rtd/4` and `0xf2c + rtd/4`
 * in THIRTY-TWO-BIT registers.  `rtd` is a short, so `rtd >> 2` is bounded
 * by +-8192 and the three sums span -4304..12048 -- inside a short at every
 * value the field can hold, so a 16-bit spelling agrees with the object
 * everywhere.  Unlike rxstate 53's `rtd + 0x2418` (finding 724) there is no
 * value that overflows.  `suite_fsk` therefore tests what IS forced and IS
 * separable: the SIGNEDNESS of the `rtd` load, at rtd = -4000, where a
 * `movzwl` reading moves the threshold by 15,384 and the modem stops
 * toggling +0x358c altogether.  The counter's own signedness is separable at
 * the ENTRY compare instead -- see `suite_entry`.
 *
 * THREE THINGS THAT CANNOT BE POKED and are steered another way.  `V34agc`
 * rewrites the receiver's +0x10c burst, its +0x130 and its +0x136 on every
 * path above the ladder, so the gain the 0x40 rung reads is seeded through
 * the LADDER path -- which calls `rxtiming` and not `V34agc` -- and the gain
 * the two end paths store is seeded through the receiver's +0x264, which
 * they copy it from.  And `rxtiming` writes +0x130, so the sample count
 * every `dftupdate` here is given is the object's and not the fixture's;
 * what the fixture controls is the burst at +0x10c, which is how the two
 * "the denominator came out zero" guards are reached.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"
#include "dsplib/v34hshak.h"

/* The five inputs of table 2's tail, which can erase what an arm decided. */
#define RX72_LVL_LIMIT	0x0230
#define RX72_LVL_COUNT	0x0234
#define RX72_TIMER_LO	0x0238
#define RX72_TIMER_HI	0x023c
#define RX72_MODE	0x2218

#define RX72_RECEIVER	0x0264
#define RX72_VECTIDX	0x2aa2	/* short, the trace's [1], zeroed by 0x67f74 */
#define RX72_INHIBIT	0x0402	/* short, switches `fskdemodulate` off      */
#define RX72_F358C	0x358c	/* short, toggled by the tone-A wait        */
#define RX72_F359C	0x359c	/* short, 0x65 is the originating end       */
#define RX72_PROBE_BINS	0xa320	/* 25 bins; the nl SIGNAL bank overlays 0..3 */
#define RX72_NOISE_BINS	0xa76c	/* and the nl NOISE bank is the four after   */
#define RX72_FSKGATE	0xa8a0	/* int, opened by the answering end's exit  */
#define RX72_BLK_A9DC	0xa9dc	/* the record whose length 0x67f54 sets     */
#define RX72_NL_NOISE	0xaab4	/* int, the 0x180 rung's totals and ratio   */
#define RX72_NL_SIGNAL	0xaab8
#define RX72_NL_RATIO	0xaac4
#define RX72_PB_NOISE	0xaabc	/* int, the 0x300 rung's                    */
#define RX72_PB_SIGNAL	0xaac0
#define RX72_PB_RATIO	0xaac8
#define RX72_PTR_AA70	0xaa70	/* pointer, re-aimed at +0xa9dc            */
#define RX72_FAACC	0xaacc	/* int, copied from the receiver's +0x238  */
#define RX72_COUNT	0xaa78	/* short, the master selector and [2]      */
#define RX72_RTD	0xaa7e	/* short, the three 0x6881e thresholds     */
#define RX72_FSK_DELAY	0xaad0
#define RX72_FSK_PHASE	0xaadc
#define RX72_FSK_NEXT	0xaade
#define RX72_FSK_NBITS	0xaae0
#define RX72_FSK_SR	0xaae2
#define RX72_FSK_PREV	0xaae4
#define RX72_FSK_INTERP	0xaae6

#define RX72_RX_SAMPS	(RX72_RECEIVER + 0x010c)	/* object +0x370 */
#define RX72_RX_FLAGS	(RX72_RECEIVER + 0x0122)	/* object +0x386 */
#define RX72_RX_F124	(RX72_RECEIVER + 0x0124)	/* object +0x388 */
#define RX72_RX_F128	(RX72_RECEIVER + 0x0128)	/* object +0x38c */
#define RX72_RX_GAIN	(RX72_RECEIVER + 0x0136)	/* object +0x39a */
#define RX72_RX_F1AC	(RX72_RECEIVER + 0x01ac)	/* object +0x410 */
#define RX72_RX_F1AE	(RX72_RECEIVER + 0x01ae)	/* object +0x412 */
#define RX72_RX_F1B0	(RX72_RECEIVER + 0x01b0)	/* object +0x414 */
#define RX72_RX_F1BE	(RX72_RECEIVER + 0x01be)	/* object +0x422 */
#define RX72_RX_F238	(RX72_RECEIVER + 0x0238)	/* object +0x49c */
#define RX72_RX_F262	(RX72_RECEIVER + 0x0262)	/* object +0x4c6 */
#define RX72_RX_F264	(RX72_RECEIVER + 0x0264)	/* object +0x4c8 */

/* One DFT bin, and the two fields the averaging loops read across. */
#define RX72_BIN		0x2c
#define RX72_BIN_ACC_RE		0x04
#define RX72_BIN_ACC_IM		0x08
#define RX72_BIN_ENERGY		0x0c

/*
 * Where +0xaa70 is aimed BEFORE the step.
 *
 * The answering end's exit re-aims it at obj + 0xa9dc, so seeding it
 * somewhere else is what makes the re-aiming observable -- and it must be
 * `v34hs_poke_self_ptr` and not `v34hs_poke_int`, because the two objects
 * are at two addresses and one address written into both is the asymmetry
 * findings 319-322 are about.  The harness excludes +0xaa70 from the byte
 * comparison and checks it as an OFFSET FROM ITS OWN BASE instead, which is
 * what makes this visible at all.
 */
#define RX72_REC_AA70	0x8100

/*
 * The microstate and txstate every case is driven with.
 *
 * 52 TX_L2 is none of 41, 51 or 62, so all three of the arm's microstate
 * transitions are a CHANGE and `hs_setstate`'s guard fires on none of them
 * by accident; `suite_guard` drives 62 and 51 deliberately to see it fire.
 * 81 MOH_SILENCE is above table 2's window and selects the once-per-block
 * dispatch's own default at 0x62a40, which is written; the arm's own
 * transition to 60 TONE_AB selects table 2's 0x644c9, which
 * `t_v34hstbl2.c` proves.  Nothing here can reach a table-2 arm the partial
 * `v34handshak` would halt on.
 */
#define RX72_MST	V34HS_TX_L2
#define RX72_TXSTATE	V34HS_MOH_SILENCE

static int dump;
static int default_fill;

/*
 * Open a case: the route, the three state words, table 2's five inputs, and
 * every field the arm writes seeded to something it does NOT write.
 *
 * That last part is finding 345's failure mode turned on this arm.  The two
 * end paths store 4 into the receiver's +0x128 and 0x3e80 into three more of
 * its halfwords; `dpskDetectInfo1Init` stores 0 into six FSK scalars and 6
 * into two; the ladder's 0x40 rung ORs 0x200 into flags.  A field already
 * holding what a store writes makes the store invisible and its mutation
 * equivalent, so all of them are seeded rather than reasoned about.
 */
static void
begin(short count)
{
	unsigned i;

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(RX72_MST, V34HS_RX_L1, RX72_TXSTATE);

	v34hs_poke_int(RX72_MODE, 0);
	v34hs_poke_int(RX72_TIMER_LO, 1000);
	v34hs_poke_int(RX72_TIMER_HI, 2000);
	v34hs_poke_int(RX72_LVL_LIMIT, 0x7fffffff);
	v34hs_poke_int(RX72_LVL_COUNT, 0);

	/* The master selector, and the arm's other three scalar inputs. */
	v34hs_poke_short(RX72_COUNT, count);
	v34hs_poke_short(RX72_RTD, 0);
	v34hs_poke_short(RX72_F359C, 0x65);
	v34hs_poke_short(RX72_INHIBIT, 0);

	/*
	 * The receiver's flags with BOTH bits the arm sets clear, so `|=
	 * 0x800` and `|= 0x200` are changes rather than no-ops -- AND ONE
	 * OTHER BIT SET, which is what separates the OR from a plain store.
	 * 0x4000 is picked because v34recv.h gives it no meaning: 0x200 is
	 * `AGC_FREEZE` and would change what `V34agc` does on the two paths
	 * that run it.
	 */
	v34hs_poke_short(RX72_RX_FLAGS, 0x4000);
	v34hs_poke_short(RX72_RX_F124, 0x1234);
	/*
	 * THREE, and the value is not free: `rxtiming` loops `for (i = 0;
	 * i < rx->f128; i++)` and writes `rx->timing_out[i]`, which is seven
	 * shorts -- so a large seed here runs thousands of iterations and
	 * scribbles over half the object, on BOTH sides, which compares and
	 * measures nothing.  Three is inside the array and is not the 4 the
	 * two end paths store, so the store stays observable.
	 */
	v34hs_poke_short(RX72_RX_F128, 3);
	v34hs_poke_short(RX72_RX_F1AC, 0x2222);
	v34hs_poke_short(RX72_RX_F1AE, 0x3333);
	v34hs_poke_short(RX72_RX_F1B0, 0x4444);
	v34hs_poke_short(RX72_RX_F1BE, 0x5555);
	v34hs_poke_short(RX72_RX_F262, 0x6666);
	v34hs_poke_short(RX72_RX_GAIN, 0x0100);
	v34hs_poke_short(RX72_RX_F264, 0x2a2a);
	v34hs_poke_int(RX72_RX_F238, 0x5a5a1234);

	/* The six ints the two `t72_measure` sites write. */
	v34hs_poke_int(RX72_NL_NOISE, -1);
	v34hs_poke_int(RX72_NL_SIGNAL, -1);
	v34hs_poke_int(RX72_NL_RATIO, -1);
	v34hs_poke_int(RX72_PB_NOISE, -1);
	v34hs_poke_int(RX72_PB_SIGNAL, -1);
	v34hs_poke_int(RX72_PB_RATIO, -1);
	v34hs_poke_int(RX72_FAACC, -1);

	/* The answering end's five, and the counter's trace companion. */
	v34hs_poke_short(RX72_VECTIDX, 0x77);
	/*
	 * WITH A NON-ZERO UPPER HALF.  The store at 0x67f68 is `mov %ebx`
	 * and not `mov %bx`, so a sixteen-bit spelling leaves the top of the
	 * word alone -- invisible against a seed of zero, and the whole
	 * claim against this one.  The route clears +0xa8a0 because rxstate
	 * 43 diverts on it at 0x64a87; nothing on rxstate 72's path reads it.
	 */
	v34hs_poke_int(RX72_FSKGATE, 0x12340000);
	v34hs_poke_short(RX72_BLK_A9DC + 0x18, 0x1234);
	v34hs_poke_short(RX72_F358C, 0x66);
	v34hs_poke_self_ptr(RX72_PTR_AA70, RX72_REC_AA70);

	/*
	 * The eleven FSK scalars `dpskDetectInfo1Init` writes and the hundred
	 * shorts it clears, none of them holding what it stores.
	 *
	 * AND EVERY ONE OF THEM PLAUSIBLE, which the first version was not.
	 * A sweep of `0x7100 + offset` over the struct is different from what
	 * the initialiser stores -- which is all finding 345 asks for -- but
	 * it puts 28,928 in `delay`, and `delay` is the discriminator's lag
	 * IN TAPS into a 49-short line.  The one case here that lets
	 * `fskdemodulate` actually run then read tens of thousands of shorts
	 * past the object on BOTH sides, which compares perfectly until the
	 * two sides' padding is made to differ: `V34HS_OBJSKEW=32` and
	 * `V34HS_PADVARY=0` failed at +0xaae4 and +0xab80..+0xab85 while the
	 * blob-on-both-sides control passed, and the fixture was right both
	 * times.  Finding 746.
	 */
	v34hs_poke_short(RX72_FSK_DELAY + 0x00, 0x20);	/* delay,  not 0x30 */
	v34hs_poke_short(RX72_FSK_DELAY + 0x02, 1);	/* offset, not 0    */
	v34hs_poke_short(RX72_FSK_DELAY + 0x04, 5);	/* bit_lo, not 1    */
	v34hs_poke_short(RX72_FSK_DELAY + 0x06, 7);	/* bit_hi, not 0    */
	v34hs_poke_short(RX72_FSK_DELAY + 0x08, 0x11);	/* bit_len, not 0xc */
	v34hs_poke_short(RX72_FSK_DELAY + 0x0a, 9);	/* resync_next, 6   */
	v34hs_poke_short(RX72_FSK_DELAY + 0x0c, 3);	/* phase,  not 0    */
	v34hs_poke_short(RX72_FSK_DELAY + 0x0e, 4);	/* next,   not 6    */
	v34hs_poke_short(RX72_FSK_DELAY + 0x10, 0x11);	/* nbits,  not 0    */
	v34hs_poke_short(RX72_FSK_DELAY + 0x12, 0x1234);/* sr,     not -1   */
	v34hs_poke_short(RX72_FSK_DELAY + 0x14, 0x55);	/* prev,   not 0    */

	for (i = 0; i < 100; i++)
		v34hs_poke_short(RX72_FSK_INTERP + 2 * i, (short)(0x2000 + i));
}

/*
 * The two DFT banks, and what the measurement will make of them.
 *
 * `dftupdate` ADDS to the accumulators, so seeding them decides the energies
 * `dftenergy` then produces -- which is the only handle on the sums, since
 * the burst the correlation reads is `rxtiming`'s and the energies
 * themselves are overwritten inside the step.
 *
 * AND THE SCALE IS NOT FREE.  `dftenergy` computes `(acc << shift) >> 16`,
 * squares that and takes the top half of the square, so an accumulator below
 * about 2^22 at shift 2 -- or 2^18 at shift 6 -- reduces to an energy of
 * ZERO.  The first version of this file seeded 4000 * bin, every energy came
 * out zero, both totals came out zero and the ratio took the zero guard: the
 * two shifts, the `<< 8`, the two banks and the rounding term were all
 * untestable and TWELVE mutations survived, with the comparison green.  The
 * constants below are the smallest round ones that put every bin's energy in
 * the hundreds, and they are why the noise bank's are an eighth of the probe
 * bank's -- the two are reduced at different shifts.
 *
 * AND `live` HAS THREE VALUES, NOT TWO, BECAUSE THE ROUNDING TERM NEEDS
 * BOTH SIDES OF ITSELF.  `+ noise/2` before the divide changes the quotient
 * only when the remainder is at least half the denominator, and `+ noise`
 * -- the other way to get it wrong -- changes it always.  So one seeding
 * cannot fail both mutations: at BANKS_UP the totals are 407,552 over 3,296
 * with a remainder past the half, which catches deleting the term; at
 * BANKS_DOWN they are 178,176 over the same 3,296 with a remainder of 192,
 * which catches widening it.  Measured rather than reasoned -- a sweep of
 * the last probe bin's accumulator over twenty-four values rounded up on
 * eleven of them -- and both were survivors before it was done.
 */
#define BANKS_SILENT	0
#define BANKS_UP	3	/* the probe bank's `i + n`: remainder high */
#define BANKS_DOWN	1	/* and low                                  */

static void
banks(int live)
{
	unsigned i;

	for (i = 0; i < 25; i++) {
		v34hs_poke_int(RX72_PROBE_BINS + i * RX72_BIN + RX72_BIN_ACC_RE,
			       live ? (int)(i + live) * 0x1000000 : 0);
		v34hs_poke_int(RX72_PROBE_BINS + i * RX72_BIN + RX72_BIN_ACC_IM,
			       live ? (int)(i + 2) * 0x800000 : 0);
		v34hs_poke_short(RX72_PROBE_BINS + i * RX72_BIN
				 + RX72_BIN_ENERGY, (short)(0x1000 + i));
	}
	for (i = 0; i < 4; i++) {
		v34hs_poke_int(RX72_NOISE_BINS + i * RX72_BIN + RX72_BIN_ACC_RE,
			       live ? (int)(i + 1) * 0x200000 : 0);
		v34hs_poke_int(RX72_NOISE_BINS + i * RX72_BIN + RX72_BIN_ACC_IM,
			       live ? (int)(i + 3) * 0x100000 : 0);
		v34hs_poke_short(RX72_NOISE_BINS + i * RX72_BIN
				 + RX72_BIN_ENERGY, (short)(0x2000 + i));
	}
}

/*
 * Make the correlation a no-op, which is the only way to zero a bank.
 *
 * `rxtiming` sets `rx_samples` to the start of the burst and then advances
 * it once per output, so `rx->f128 == 0` leaves the two equal and the sample
 * count every `dftupdate` here is given is ZERO.  With the accumulators
 * seeded to zero as well, `dftenergy` produces nothing and the denominator
 * guard is reached.  Zeroing the burst itself does NOT work: the receiver's
 * `flags`, `f124` and `f128` are only 0x16 bytes past +0x10c, so a poke long
 * enough to cover the samples destroys the loop bound it depends on.
 */
static void
silent(void)
{
	banks(BANKS_SILENT);
	v34hs_poke_short(RX72_RX_F128, 0);
}

#define NOCHECK	((unsigned)-1)

static unsigned last_hash;
static unsigned last_changed;
static unsigned last_lines;

/*
 * Step, compare, and say what the step DID.
 *
 * The comparison is the differential check; the assertions after it are the
 * anti-vacuity half.  A case that left one guard earlier than the name says
 * still compares -- both sides left -- so without them a seed that stopped
 * reaching a body would leave this file green and testing nothing.
 */
static void
step(const char *what, long tag, unsigned changed, unsigned lines,
     int mst, int rxst, int txst)
{
	const struct v34hs_obs *o;

	v34hs_ours(1);
	v34hs_step();
	v34hs_ours(0);
	v34hs_compare(what, tag);

	o = v34hs_observed(0);
	last_hash = o->hash;
	last_changed = o->changed;
	last_lines = o->lines;
	if (dump)
		printf("  %-46s changed %5u  lines %2u  mst %2d rx %2d tx %2d "
		       "hash %08x\n", what, o->changed, o->lines,
		       o->mst, o->rxst, o->txst, o->hash);

	/*
	 * `changed` and `lines` are properties of the FILL as well as of the
	 * arm, so both are asserted at the default fixture only; the
	 * transcript itself is compared line for line at every seed by
	 * `v34hs_compare` above and what is gated here is only the count.
	 *
	 * No conversion in these: `diff_eq_int` appends the input itself.
	 */
	if (default_fill && changed != NOCHECK)
		diff_eq_int("object bytes the step wrote", o->changed, changed,
			    tag);
	if (default_fill && lines != NOCHECK)
		diff_eq_int("diagnostic lines printed", o->lines, lines, tag);
	if (mst != (int)NOCHECK)
		diff_eq_int("microstate afterwards", o->mst, mst, tag);
	if (rxst != (int)NOCHECK)
		diff_eq_int("rxstate afterwards", o->rxst, rxst, tag);
	if (txst != (int)NOCHECK)
		diff_eq_int("txstate afterwards", o->txst, txst, tag);
}

/* The same case with the BLOB on both sides -- the fixture's own control. */
static void
control(const char *what, long tag)
{
	v34hs_step();
	v34hs_compare(what, tag);
}

/*
 * The distinct behaviours this file claims, so that a change collapsing two
 * of them is a failure rather than a silence (finding 290).
 */
/*
 * THE CAP FAILS RATHER THAN TRUNCATES.  `t_v34hsrx53.c`'s version silently
 * drops anything past `NSIG`, and this file found out the hard way: with
 * twenty-six calls and a cap of twenty it recorded twenty, compared twenty
 * and passed the pairwise sweep having never looked at six behaviours.  That
 * is gates.md rule 1 -- a count of what was examined, and a failure on the
 * difference -- so overflowing it is a hard error here.
 */
#define NSIG	40
static unsigned sig[NSIG];
static const char *signame[NSIG];
static int nsig;

static void
record(const char *name)
{
	if (nsig >= NSIG) {
		fprintf(stderr, "t_v34hsrx72: more than %d behaviours "
			"recorded, at \"%s\"\n", NSIG, name);
		exit(2);
	}
	sig[nsig] = last_hash;
	signame[nsig] = name;
	nsig++;
}

/* --- the entry, and the counter that drives everything ------------------ */

/*
 * 0x650c6.  `movzwl`, `inc`, `cmp $0x440,%di`, `mov %di,0xaa78`, `jle`.
 *
 * THE STORE PRECEDES THE BRANCH, so a poke of n is a branch on n + 1, and
 * every other suite here depends on that.  The two cases at 0x43f and 0x440
 * are the boundary: one goes to the ladder and the other to `V34agc`, and
 * they are as far apart as two cases in this arm get.
 *
 * AND THE COMPARE IS SIGNED, which is separable HERE and nowhere else.  A
 * counter of -20 becomes -19, which a signed reading sends to the ladder --
 * where it matches no rung and the block is dropped -- and an unsigned one
 * reads as 65,517 and sends past `V34agc` into the end paths.  The same
 * question at 0x6881e cannot be asked, because nothing negative reaches it.
 */
static void
suite_entry(void)
{
	long tag = 1000;

	begin(0x43f);
	step("entry: 0x440 is the last ladder block", tag,
	     76, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("entry 0x440");
	diff_eq_int("the counter was stored before the branch",
		    v34hs_peek_short(0, RX72_COUNT), 0x440, tag);

	begin(0x440);
	step("entry: 0x441 leaves the ladder", tag + 1,
	     335, 2, RX72_MST, V34HS_RX_L1, V34HS_TONE_AB);
	/*
	 * NOT RECORDED, and deliberately: this case and `suite_ends`' second
	 * are the SAME case -- 0x441 is exactly what the originating end's
	 * exit is selected by -- so recording both would claim a distinction
	 * that does not exist and finding 290's check would fail on it.
	 */

	/*
	 * The signedness trial.  -19 matches no rung, so the object leaves
	 * having written nothing but the counter; an unsigned reading would
	 * run `V34agc` and one of the two end paths, which move four state
	 * words and a hundred bytes.
	 */
	begin(-20);
	step("entry: a negative counter stays on the ladder", tag + 2,
	     26, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("entry negative");
	diff_eq_int("and it is still negative afterwards",
		    v34hs_peek_short(0, RX72_COUNT), -19, tag + 2);
}

/* --- the ladder --------------------------------------------------------- */

/*
 * 0x6551a.  Six rungs and a default, chosen by the counter.
 *
 * The two WINDOWS are the interesting pair: 0x1c1..0x2ff and 0x41..0x17f run
 * the same two correlations and differ only in whether the receiver's own
 * +0x124 is bumped, so each is driven at both ends and one outside, and the
 * bump is what tells them apart.
 */
static void
suite_ladder(void)
{
	long tag = 1100;

	begin(0x400);
	banks(BANKS_SILENT);
	step("ladder: >0x300 correlates all 25 bins", tag,
	     76, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("ladder 0x401");

	begin(0x200);
	banks(BANKS_SILENT);
	step("ladder: 0x1c1..0x2ff correlates 4 and 4", tag + 1,
	     42, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("ladder 0x201");

	begin(0x100);
	banks(BANKS_SILENT);
	step("ladder: 0x41..0x17f also bumps rx+0x124", tag + 2,
	     43, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("ladder 0x101");
	diff_eq_int("the receiver's own counter moved",
		    v34hs_peek_short(0, RX72_RX_F124), 0x1235, tag + 2);

	begin(0x10);
	banks(BANKS_SILENT);
	step("ladder: below 0x40 does nothing at all", tag + 3,
	     26, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("ladder 0x11");
	diff_eq_int("and rx+0x124 did NOT move",
		    v34hs_peek_short(0, RX72_RX_F124), 0x1234, tag + 3);

	/*
	 * The two rungs that are a single value: 0x301 is the bottom of the
	 * top rung and 0x2ff the top of the upper window, and neither can be
	 * separated from its neighbour by a case in the middle of a run.
	 */
	begin(0x300);
	banks(BANKS_SILENT);
	step("ladder: 0x301 is the bottom of the top rung", tag + 8,
	     76, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	/* Not recorded: it is the SAME rung as 0x401, only its first value. */

	begin(0x2fe);
	banks(BANKS_SILENT);
	step("ladder: 0x2ff is the top of the upper window", tag + 9,
	     42, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("0x2ff does not bump rx+0x124",
		    v34hs_peek_short(0, RX72_RX_F124), 0x1234, tag + 9);

	/* The four window edges, each against the rung on the other side. */
	begin(0x1c0);
	banks(BANKS_SILENT);
	step("ladder: 0x1c1 is inside the upper window", tag + 4,
	     42, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("0x1c1 does not bump rx+0x124",
		    v34hs_peek_short(0, RX72_RX_F124), 0x1234, tag + 4);

	begin(0x1bf);
	banks(BANKS_SILENT);
	step("ladder: 0x1c0 is below it and above 0x17f", tag + 5,
	     26, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("0x1c0 does not bump rx+0x124 either",
		    v34hs_peek_short(0, RX72_RX_F124), 0x1234, tag + 5);
	record("ladder 0x1c0, no rung");

	begin(0x17e);
	banks(BANKS_SILENT);
	step("ladder: 0x17f is the top of the lower window", tag + 6,
	     43, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("0x17f bumps rx+0x124",
		    v34hs_peek_short(0, RX72_RX_F124), 0x1235, tag + 6);

	begin(0x40);
	banks(BANKS_SILENT);
	step("ladder: 0x41 is the bottom of it", tag + 7,
	     43, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("0x41 bumps rx+0x124",
		    v34hs_peek_short(0, RX72_RX_F124), 0x1235, tag + 7);
}

/* --- the two measurements ----------------------------------------------- */

/*
 * 0x69c7a and 0x6a668: the same construct at two sets of offsets.
 *
 * Both correlate, reduce the noise bank at shift 6 and the probe bank at
 * shift 2, total four bins of each with the SIGNAL total scaled by 256, and
 * divide with a rounding term.  0x300 writes +0xaabc/+0xaac0/+0xaac8 and
 * re-arms with `dftfreqinit`; 0x180 writes +0xaab4/+0xaab8/+0xaac4 and
 * re-arms with the two `dftnlinit*` bodies.  A mutation that crossed the two
 * sets, or the two shifts, or dropped the `<< 8`, moves a number this
 * compares.
 */
static void
suite_measure(void)
{
	long tag = 1200;

	begin(0x2ff);
	banks(BANKS_UP);
	step("measure: 0x300, probe SNR and dftfreqinit", tag,
	     872, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("measure 0x300");

	begin(0x17f);
	banks(BANKS_DOWN);
	step("measure: 0x180, nl ratio and the two nl inits", tag + 1,
	     184, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("measure 0x180");

	/*
	 * THE DENOMINATOR GOES TO ZERO.  0x6aa0b and 0x6c9aa are the only
	 * two blocks in the arm that a seeded accumulator cannot reach --
	 * they need the noise bank to reduce to nothing, which needs both a
	 * cleared accumulator AND a silent burst, because `dftupdate` runs
	 * first and adds whatever `rxtiming` left at +0x10c.
	 */
	begin(0x2ff);
	silent();
	step("measure: 0x300 with the noise bank silent", tag + 2,
	     775, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("measure 0x300 zero");
	diff_eq_int("the ratio was forced to zero",
		    (int)v34hs_peek_short(0, RX72_PB_RATIO)
		    | (int)v34hs_peek_short(0, RX72_PB_RATIO + 2), 0, tag + 2);

	begin(0x17f);
	silent();
	step("measure: 0x180 with the noise bank silent", tag + 3,
	     139, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("measure 0x180 zero");
	diff_eq_int("the nl ratio was forced to zero",
		    (int)v34hs_peek_short(0, RX72_NL_RATIO)
		    | (int)v34hs_peek_short(0, RX72_NL_RATIO + 2), 0, tag + 3);
}

/* --- the 0x40 rung, and the gain ---------------------------------------- */

/*
 * 0x6a4f4.  The gain `V34agc` settled on becomes the starting gain for what
 * follows, doubled -- unless it is above 0x34ff, when 0x6aec1 substitutes
 * 0x6000 and says so.  0x6aec1 REJOINS at 0x6a52c, so the "beginning of
 * RX_L1" line prints on both paths and the transcripts differ by exactly the
 * error line.
 *
 * The comparison is SIGNED sixteen bits, so 0x8000 is not "above 0x34ff":
 * a negative gain takes the doubling path and stores 0.  That is the third
 * case here, and it is what a `movzwl` reading of the gain would get wrong.
 */
static void
suite_gain(void)
{
	long tag = 1300;

	begin(0x3f);
	banks(BANKS_SILENT);
	v34hs_poke_short(RX72_RX_GAIN, 0x1234);
	step("gain: 0x40 doubles a gain in range", tag,
	     61, 1, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("gain in range");
	diff_eq_int("the doubled gain was stored",
		    v34hs_peek_short(0, RX72_RX_F262), 0x2468, tag);

	begin(0x3f);
	banks(BANKS_SILENT);
	v34hs_poke_short(RX72_RX_GAIN, 0x3500);
	step("gain: one above the limit substitutes 0x6000", tag + 1,
	     61, 2, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("gain too high");
	diff_eq_int("the substitute was stored",
		    v34hs_peek_short(0, RX72_RX_F262), 0x6000, tag + 1);

	begin(0x3f);
	banks(BANKS_SILENT);
	v34hs_poke_short(RX72_RX_GAIN, 0x34ff);
	step("gain: 0x34ff itself is still in range", tag + 2,
	     61, 1, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("0x34ff doubled rather than substituted",
		    v34hs_peek_short(0, RX72_RX_F262), 0x69fe, tag + 2);

	begin(0x3f);
	banks(BANKS_SILENT);
	v34hs_poke_short(RX72_RX_GAIN, (short)0x8000);
	step("gain: a negative gain is BELOW the limit", tag + 3,
	     61, 1, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("gain negative");
	diff_eq_int("so it was doubled, not substituted",
		    v34hs_peek_short(0, RX72_RX_F262), 0, tag + 3);
}

/* --- the two ways out of the probe -------------------------------------- */

/*
 * 0x67d70 and 0x69611.  Both finish the probe the same way -- the receiver's
 * +0x238 kept at +0xaacc, the whole bank reduced, `V34SetupDemodulator` at
 * 2400 baud with no carrier, one flag bit and the gain restored from the
 * receiver's +0x264 -- and then diverge completely.
 */
static void
suite_ends(void)
{
	long tag = 1400;

	begin(0x500);
	v34hs_poke_short(RX72_F359C, 0);
	step("end: the answering end arms INFO1", tag,
	     551, 4, V34HS_DET_SYNC, V34HS_RX_DPSK, V34HS_TONE_AB);
	record("end answering");
	diff_eq_int("the counter restarted",
		    v34hs_peek_short(0, RX72_COUNT), 0, tag);
	diff_eq_int("the record length is INFO1c's",
		    v34hs_peek_short(0, RX72_BLK_A9DC + 0x18), 0x4d, tag);
	diff_eq_int("the trace companion was cleared",
		    v34hs_peek_short(0, RX72_VECTIDX), 0, tag);
	diff_eq_int("the FSK gate at +0xa8a0 was opened",
		    v34hs_peek_short(0, RX72_FSKGATE), 1, tag);
	diff_eq_int("the receiver's +0x238 was kept",
		    v34hs_peek_short(0, RX72_FAACC), 0x1234, tag);
	diff_eq_int("and its upper half too",
		    v34hs_peek_short(0, RX72_FAACC + 2), 0x5a5a, tag);
	diff_eq_int("the gain came from the receiver's +0x264",
		    v34hs_peek_short(0, RX72_RX_GAIN), 0x2a2a, tag);
	diff_eq_int("bit 11 of the receiver's flags is set",
		    v34hs_peek_short(0, RX72_RX_FLAGS) & 0x800, 0x800, tag);
	diff_eq_int("2400 baud went into the receiver's +0x1ac",
		    v34hs_peek_short(0, RX72_RX_F1AC), 0x1f40, tag);

	begin(0x440);
	step("end: the originating end waits one more block", tag + 1,
	     335, 2, RX72_MST, V34HS_RX_L1, V34HS_TONE_AB);
	record("end originating");
	diff_eq_int("the counter did NOT restart",
		    v34hs_peek_short(0, RX72_COUNT), 0x441, tag + 1);
	diff_eq_int("the shift register was cleared to all ones",
		    v34hs_peek_short(0, RX72_FSK_SR), -1, tag + 1);
	diff_eq_int("and the bit count to zero",
		    v34hs_peek_short(0, RX72_FSK_NBITS), 0, tag + 1);
	diff_eq_int("the record length was NOT touched",
		    v34hs_peek_short(0, RX72_BLK_A9DC + 0x18), 0x1234,
		    tag + 1);
	diff_eq_int("the gain came from the receiver's +0x264 here too",
		    v34hs_peek_short(0, RX72_RX_GAIN), 0x2a2a, tag + 1);
}

/* --- the FSK gate, and the timeout ladder behind it --------------------- */

/*
 * 0x65145 onwards, reached with `fsk_inhibit` set so that `fskdemodulate`
 * returns without touching either field -- see the head of this file.
 *
 * The gate is `nbits <= 0x14 || (sr & 0xfff)`, and both halves are driven:
 * a clean 0xf000 with 21 bits is tone A, and either 20 bits or one bit set
 * in the low twelve is not.
 */
static void
suite_fsk(void)
{
	long tag = 1500;

	begin(0x600);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x15);
	v34hs_poke_short(RX72_FSK_SR, (short)0xf000);
	step("fsk: tone A hands over to DPSK", tag,
	     14, 2, V34HS_RX_PHASE3_CALL, V34HS_RX_DPSK,
	     RX72_TXSTATE);
	record("fsk tone A");

	begin(0x600);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	v34hs_poke_short(RX72_FSK_SR, (short)0xf000);
	step("fsk: twenty bits is one too few", tag + 1,
	     12, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("fsk too few bits");

	begin(0x600);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x15);
	v34hs_poke_short(RX72_FSK_SR, (short)0xf001);
	step("fsk: one bit set in the low twelve is not tone A", tag + 2,
	     12, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("and the counter was below the first threshold",
		    v34hs_peek_short(0, RX72_FSK_SR), (short)0xf001, tag + 2);

	/*
	 * 0x6881e.  Above 0xf10 the shift register is cleared every block;
	 * at exactly 0xf14 the arm toggles +0x358c and says what it is
	 * waiting for; at exactly 0xf2c it gives up and restarts.
	 */
	begin(0xf12);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	step("fsk: past 0xf10 the shift register is cleared", tag + 3,
	     14, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("fsk past 0xf10");
	diff_eq_int("cleared to all ones",
		    v34hs_peek_short(0, RX72_FSK_SR), -1, tag + 3);
	diff_eq_int("and +0x358c was NOT toggled",
		    v34hs_peek_short(0, RX72_F358C), 0x66, tag + 3);

	begin(0xf0f);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	step("fsk: 0xf10 itself is not past it", tag + 4,
	     12, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("fsk at 0xf10");
	diff_eq_int("the shift register was left alone",
		    v34hs_peek_short(0, RX72_FSK_SR), 0x1234, tag + 4);

	begin(0xf13);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	step("fsk: 0xf14 toggles +0x358c and says so", tag + 5,
	     15, 1, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("fsk at 0xf14");
	diff_eq_int("the toggle is an XOR of bit 0 and not a store",
		    v34hs_peek_short(0, RX72_F358C), 0x67, tag + 5);

	begin(0xf2b);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	step("fsk: 0xf2c gives up and goes round again", tag + 6,
	     17, 2, V34HS_TX_L1, V34HS_RX_DPSK, RX72_TXSTATE);
	record("fsk at 0xf2c");
	diff_eq_int("the counter restarted",
		    v34hs_peek_short(0, RX72_COUNT), 0, tag + 6);

	/*
	 * 0xf11 is the FIRST value past the first threshold, which is what
	 * separates `> 0xf10` from `> 0xf11`; a run of values cannot.
	 */
	begin(0xf10);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	step("fsk: 0xf11 is the first value past the threshold", tag + 11,
	     14, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("so the shift register was cleared",
		    v34hs_peek_short(0, RX72_FSK_SR), -1, tag + 11);

	/*
	 * +0x358c ALREADY ODD.  An XOR of bit 0 clears it and an OR leaves
	 * it, and against a seed with the bit clear the two are the same
	 * store -- which is the whole of this case.
	 */
	begin(0xf13);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	v34hs_poke_short(RX72_F358C, 0x67);
	step("fsk: 0xf14 toggles +0x358c the other way too", tag + 12,
	     15, 1, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("fsk toggle down");
	diff_eq_int("the bit went off, so it is an XOR and not an OR",
		    v34hs_peek_short(0, RX72_F358C), 0x66, tag + 12);

	/*
	 * The give-up threshold with a round-trip delay, which is what says
	 * 0xf2c is a base and not the whole constant.
	 */
	begin(0x112b);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	v34hs_poke_short(RX72_RTD, 0x800);
	step("fsk: 0xf2c + 0x200 gives up too", tag + 13,
	     17, 2, V34HS_TX_L1, V34HS_RX_DPSK, RX72_TXSTATE);
	/* Not recorded: the same give-up, at a threshold the rtd moved. */
	diff_eq_int("the counter restarted there as well",
		    v34hs_peek_short(0, RX72_COUNT), 0, tag + 13);

	/*
	 * THE MASK IS TWELVE BITS AND NOT EIGHT.  0xf100 has a bit set in
	 * the low twelve and none in the low eight, so an eight-bit mask
	 * calls it tone A and the object does not.
	 */
	begin(0x600);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x15);
	v34hs_poke_short(RX72_FSK_SR, (short)0xf100);
	step("fsk: bit 8 alone is still not tone A", tag + 14,
	     12, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	/* Not recorded: the same "not tone A" behaviour as twenty bits. */

	/*
	 * AND ONE CASE THAT LETS `fskdemodulate` ACTUALLY RUN, which is the
	 * only thing that can see what buffer it was handed.  Everything
	 * else here inhibits it, so the pointer is dead in every other case.
	 */
	begin(0x600);
	step("fsk: the demodulator runs on the receiver's +0x10c", tag + 15,
	     NOCHECK, NOCHECK, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("fsk demodulator runs");

	/*
	 * THE SIGNEDNESS TRIAL, and it is the only one the three thresholds
	 * admit.  At rtd = -4000 the arithmetic shift gives -1000 and 0xf14
	 * becomes 0xb2c; a `movzwl` reading gives +15,384 and the same
	 * counter is nearly 15,000 below even the first threshold, so the
	 * modem sits there instead of toggling.
	 */
	begin(0xb2b);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	v34hs_poke_short(RX72_RTD, -4000);
	step("fsk: a NEGATIVE rtd lowers all three thresholds", tag + 7,
	     15, 1, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	record("fsk negative rtd");
	diff_eq_int("0xb2c was 0xf14 - 1000, so +0x358c toggled",
		    v34hs_peek_short(0, RX72_F358C), 0x67, tag + 7);

	/* And a positive one, which moves them the other way. */
	begin(0x1113);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	v34hs_poke_short(RX72_RTD, 0x800);
	step("fsk: a positive rtd raises them", tag + 8,
	     15, 1, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("0x1114 was 0xf14 + 0x200, so +0x358c toggled",
		    v34hs_peek_short(0, RX72_F358C), 0x67, tag + 8);

	/* The SHIFT itself: rtd 3 gives 0, rtd 4 gives 1. */
	begin(0xf13);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	v34hs_poke_short(RX72_RTD, 3);
	step("fsk: rtd 3 shifts to nothing", tag + 9,
	     15, 1, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("so 0xf14 still toggles",
		    v34hs_peek_short(0, RX72_F358C), 0x67, tag + 9);

	begin(0xf13);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	v34hs_poke_short(RX72_RTD, 4);
	step("fsk: rtd 4 shifts to one, and 0xf14 misses", tag + 10,
	     14, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("so it does not toggle",
		    v34hs_peek_short(0, RX72_F358C), 0x66, tag + 10);
}

/* --- `hs_setstate`'s guard --------------------------------------------- */

/*
 * Eight sites, five of which can fire.  Driving the microstate the arm is
 * about to write is what makes the guard's `now == next` return observable:
 * the transition line disappears from the transcript and the byte count
 * drops by nothing at all, since the store was a no-op either way.
 */
static void
suite_guard(void)
{
	long tag = 1600;

	begin(0x600);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x15);
	v34hs_poke_short(RX72_FSK_SR, (short)0xf000);
	v34hs_state(V34HS_RX_PHASE3_CALL, V34HS_RX_L1, RX72_TXSTATE);
	step("guard: the microstate is already 62", tag,
	     13, 1, V34HS_RX_PHASE3_CALL, V34HS_RX_DPSK,
	     RX72_TXSTATE);
	record("guard microstate 62");

	begin(0xf2b);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	v34hs_state(V34HS_TX_L1, V34HS_RX_L1, RX72_TXSTATE);
	step("guard: the microstate is already 51", tag + 1,
	     16, 1, V34HS_TX_L1, V34HS_RX_DPSK, RX72_TXSTATE);
	record("guard microstate 51");

	begin(0x440);
	v34hs_state(RX72_MST, V34HS_RX_L1, V34HS_TONE_AB);
	step("guard: the txstate is already 60", tag + 2,
	     334, 1, RX72_MST, V34HS_RX_L1, V34HS_TONE_AB);
	record("guard txstate 60");
}

/* --- the diagnostics off ------------------------------------------------ */

/*
 * The same five behaviours with `dsplibs_debug_level` at zero.
 *
 * Every one of them must write exactly what its loud twin wrote: the arm has
 * thirteen debug sites and not one of them has a side effect, which is a
 * claim and not an assumption -- `hs_setstate` reads three state words and
 * two counters inside its own `if`, and a mistake there would move a byte.
 */
static void
suite_quiet(void)
{
	long tag = 1700;
	unsigned h_answering, h_originating, h_tone, h_gain, h_measure;

	begin(0x500);
	v34hs_poke_short(RX72_F359C, 0);
	step("loud: the answering end", tag,
	     551, 4, V34HS_DET_SYNC, V34HS_RX_DPSK, V34HS_TONE_AB);
	h_answering = last_hash;

	begin(0x440);
	step("loud: the originating end", tag + 1,
	     335, 2, RX72_MST, V34HS_RX_L1, V34HS_TONE_AB);
	h_originating = last_hash;

	begin(0x600);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x15);
	v34hs_poke_short(RX72_FSK_SR, (short)0xf000);
	step("loud: tone A", tag + 2,
	     14, 2, V34HS_RX_PHASE3_CALL, V34HS_RX_DPSK,
	     RX72_TXSTATE);
	h_tone = last_hash;

	begin(0x3f);
	banks(BANKS_SILENT);
	v34hs_poke_short(RX72_RX_GAIN, 0x3500);
	step("loud: the gain error", tag + 3,
	     61, 2, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	h_gain = last_hash;

	begin(0x2ff);
	banks(BANKS_UP);
	step("loud: the probe measurement", tag + 4,
	     872, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	h_measure = last_hash;

	v34hs_debug(0);

	begin(0x500);
	v34hs_poke_short(RX72_F359C, 0);
	step("quiet: the answering end", tag + 5,
	     551, 0, V34HS_DET_SYNC, V34HS_RX_DPSK, V34HS_TONE_AB);
	diff_eq_int("quiet answering writes what the loud one did",
		    last_hash, h_answering, tag + 5);

	begin(0x440);
	step("quiet: the originating end", tag + 6,
	     335, 0, RX72_MST, V34HS_RX_L1, V34HS_TONE_AB);
	diff_eq_int("quiet originating writes what the loud one did",
		    last_hash, h_originating, tag + 6);

	begin(0x600);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x15);
	v34hs_poke_short(RX72_FSK_SR, (short)0xf000);
	step("quiet: tone A", tag + 7,
	     14, 0, V34HS_RX_PHASE3_CALL, V34HS_RX_DPSK, RX72_TXSTATE);
	diff_eq_int("quiet tone A writes what the loud one did",
		    last_hash, h_tone, tag + 7);

	begin(0x3f);
	banks(BANKS_SILENT);
	v34hs_poke_short(RX72_RX_GAIN, 0x3500);
	step("quiet: the gain error", tag + 8,
	     61, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("quiet gain error writes what the loud one did",
		    last_hash, h_gain, tag + 8);

	begin(0x2ff);
	banks(BANKS_UP);
	step("quiet: the probe measurement", tag + 9,
	     872, 0, RX72_MST, V34HS_RX_L1, RX72_TXSTATE);
	diff_eq_int("quiet measurement writes what the loud one did",
		    last_hash, h_measure, tag + 9);

	v34hs_debug(1);
}

/* --- the fixture's own control ----------------------------------------- */

/*
 * The same cases with the BLOB ON BOTH SIDES.
 *
 * A green ours-versus-blob run says nothing unless the same seed is green
 * blob-versus-blob, because then the disagreement could be the fixture's --
 * docs/v34handshak.md's rule for a per-case test.  It matters more here than
 * usual: finding 736 records a fixture fault confined to the receiver's
 * +0x1ae and +0x1cc..+0x1e3 at seeds 7 and 20, and +0x1ae is one of the five
 * halfwords `V34SetupDemodulator` writes on both of the end paths.
 */
static void
suite_control(void)
{
	long tag = 1800;

	begin(0x500);
	v34hs_poke_short(RX72_F359C, 0);
	control("control: the answering end, blob on both sides", tag);

	begin(0x440);
	control("control: the originating end, blob on both sides", tag + 1);

	begin(0x600);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x15);
	v34hs_poke_short(RX72_FSK_SR, (short)0xf000);
	control("control: tone A, blob on both sides", tag + 2);

	begin(0x2ff);
	banks(BANKS_UP);
	control("control: the probe measurement, blob on both sides",
		tag + 3);

	begin(0x3f);
	banks(BANKS_SILENT);
	v34hs_poke_short(RX72_RX_GAIN, 0x3500);
	control("control: the gain error, blob on both sides", tag + 4);

	begin(0xb2b);
	v34hs_poke_short(RX72_INHIBIT, 1);
	v34hs_poke_short(RX72_FSK_NBITS, 0x14);
	v34hs_poke_short(RX72_RTD, -4000);
	control("control: the negative rtd, blob on both sides", tag + 5);

	begin(0x600);
	control("control: the demodulator running, blob on both sides",
		tag + 6);
}

int
main(void)
{
	int i, j;

	dump = getenv("V34HS_DUMP") != NULL;
	default_fill = getenv("V34HS_SEED") == NULL;
	diff_begin("v34handshak: rxstate 72 RX_L1, 0x650c6");

	/*
	 * The diagnostics on.  Thirteen of the arm's sites print -- eight of
	 * them `hs_setstate`'s own and two `V34SetupDemodulator`'s -- and the
	 * transcripts are compared line for line, which is this file's check
	 * on `StateName` and on the three literals the arm owns.  `StateName`
	 * is indexed unbounded (D42), so every state word driven here stays
	 * inside 0..86.
	 */
	v34hs_debug(1);

	suite_entry();
	suite_ladder();
	suite_measure();
	suite_gain();
	suite_ends();
	suite_fsk();
	suite_guard();
	suite_quiet();
	suite_control();

	/*
	 * The behaviours claimed distinct, pairwise.  A change collapsing two
	 * of them is a failure rather than a silence (finding 290), and the
	 * count is pinned because a `record()` deleted in an edit would
	 * shrink the check in silence.
	 */
	for (i = 0; i < nsig; i++)
		for (j = i + 1; j < nsig; j++) {
			char msg[160];

			snprintf(msg, sizeof(msg), "%s differs from %s",
				 signame[i], signame[j]);
			diff_eq_int(msg, sig[i] != sig[j], 1,
				    9000 + i * 100 + j);
		}

	diff_eq_int("behaviours recorded", nsig, 28, 0);

	v34hs_holes_check();
	return diff_end();
}
