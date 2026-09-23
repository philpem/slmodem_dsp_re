/*
 * t_v34hst3mid.c -- our reconstruction of table 3's middle group, against the
 * blob, one dispatch arm at a time.
 *
 * `v34handshak` is 61,541 bytes and nothing here reconstructs it.  What is
 * reconstructed is a set of the microstate arms at .rodata+0x3000 plus the
 * prologue that reaches them and the transmit dispatch and tail they leave
 * through (src/pump/v34/v34hshak_t3mid.c), and this drives each of them
 * through `test/harness/v34hsstep.c` with OUR entry on side A and the blob on
 * side B.  That makes it an ordinary tier-1 differential test: 44,096 bytes
 * of object, five blocks, seven filler regions and both transcripts, compared
 * after every step.
 *
 * THREE THINGS EVERY CASE HERE HAS TO DO, and the first two are what stop it
 * being an expensive way of comparing the blob with itself:
 *
 *   1  RUN THE SAME CASE WITH THE BLOB ON BOTH SIDES.  `v34hs_ours(0)`
 *      puts it back.  A green ours-versus-blob run means nothing if the same
 *      seed is green blob-versus-blob for a reason of the fixture's, and this
 *      is finding F290's whole point about what the harness proves.
 *
 *   2  REACH THE PATH IT CLAIMS TO.  Six of table 3's arms bump one counter
 *      at +0xaa78 and test it against one or two exact thresholds, and the
 *      fixture's own fill sends all six down the same branch -- finding F290
 *      measured them as one behaviour with one signature.  A sweep that never
 *      seeds the counter tests the increment and NONE of the thresholds, and
 *      swapping one arm's constants for another's could not fail.  So each
 *      threshold gets a trial that lands exactly on it and a trial that does
 *      not, and the counter is read back afterwards to say which happened.
 *
 *   3  NOT REACH A PATH THAT IS NOT WRITTEN.  Our entry records the first
 *      unwritten path it takes and returns; `check` fails if any trial hit
 *      one, so a case that quietly did nothing is a failure and not a pass.
 *
 * THE TXSTATE IS PART OF THE FIXTURE, not a don't-care (finding F288): most of
 * these arms end by jumping to the once-per-block transmit dispatch, so a
 * case driven with a txstate whose table-2 arm does nothing is a case tested
 * against silence.  SSEG is the choice here, as in `t_v34hsstep.c`, and it is
 * also the choice that exercises both of arm 48's diagnostics -- its forcing
 * path prints only when the txstate it is replacing is not already 5.
 *
 * ARM 63 IS THE CASE WHERE THAT IS NOT A CHOICE AT ALL: the txstate is what
 * SELECTS its body, so 60 TONE_AB and 5 SILENCE run two different pieces of
 * code and SSEG runs neither.  Driven cold at SSEG it is indistinguishable
 * from the twenty-four states that share the three-instruction arm, which is
 * exactly what the fixture's sweep reported, and the cure is the txstate and
 * not a companion field.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"

#include "dsplib/v34det.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"

/*
 * The txstate every case below is driven with, and the counter the six timer
 * arms share.  Prefixed, because four batches are writing `v34handshak` at
 * once and finding F325 is what one shared macro name cost.
 */
#define T3MT_TXSTATE	V34HS_SSEG	/* 18 */
#define T3MT_COUNTER	0xaa78		/* short */
#define T3MT_TOGGLE	0x358c		/* short */
#define T3MT_PROGRESS	0x0004		/* int   */
#define T3MT_MODE	0x2218		/* int, what the tail dispatches on */
#define T3MT_TICK	0x0234		/* int  */
#define T3MT_ELAPSED	0x0238		/* int  */
#define T3MT_DEADLINE	0x023c		/* int  */
#define T3MT_FLOOR	0x0230		/* int, rx_energy_floor            */
#define T3MT_ANSWER	0x359c		/* short, 0x65 originate 0x66 answer*/
#define T3MT_BAUD	0xaa96		/* short, the tail multiplies by 3  */
#define T3MT_VECTIDX	0x2aa2		/* short, the tail's 0x4a arm reads */
/* Arm 47's. */
#define T3MT_FSK_SR	0xaae2		/* short, obj->fsk.sr; low 4 bits   */
#define T3MT_FSK_NBITS	0xaae0		/* short, obj->fsk.nbits            */
/* The rest of `struct v34_fsk` at +0xaad0, which only arm 55 has to drive. */
#define T3MT_FSK_BITLO	0xaad4		/* short, shifted in for <= 0       */
#define T3MT_FSK_BITHI	0xaad6		/* short, and for > 0               */
#define T3MT_FSK_BITLEN	0xaad8		/* short, samples per bit           */
#define T3MT_FSK_RESYNC	0xaada		/* short, what `next` restarts at   */
#define T3MT_FSK_PHASE	0xaadc		/* short, the sample counter        */
#define T3MT_FSK_NEXT	0xaade		/* short, the next sampling instant */
#define T3MT_F3588	0x3588		/* short, the reset's second guard  */
#define T3MT_F358A	0x358a		/* short                            */
#define T3MT_FABC2	0xabc2		/* short, the eleventh cleared word */
#define T3MT_MSGREC0	0xa94c		/* the message record it fills      */
#define T3MT_V90RX	0x024c		/* int, tested for non-zero only    */
#define T3MT_K56RX	0x0250		/* int, which arm 47 does NOT read  */
/* Arm 63's. */
#define T3MT_PTC	0x0008		/* int, 0x30 picks 0x1e over 0x96   */
#define T3MT_BAUDRATE	0xaa84		/* short, `v34setuptxmit`'s baud    */
#define T3MT_CARRIER	0xaa94		/* short, and its carrier           */
#define T3MT_PREEMP	0xaa8a		/* short, and its pre-emphasis idx  */
#define T3MT_P3GATE	0xabff		/* SIGNED byte, must be > 0         */
#define T3MT_P3COUNT	0x0240		/* int, must be > 0x240             */
/* Arms 49, 50 and 51's. */
#define T3MT_FILTDELAY	0xaa7c		/* short, three of 49's and 50's
					   four thresholds are it plus a
					   constant                        */
#define T3MT_ISSHORT	0xabcc		/* short, and it picks a whole body
					   in 51 and a formula in 49       */
#define T3MT_PREVBD	0xac02		/* short, 49's bulk delay when
					   is_short is set                 */
#define T3MT_RTD	0xaa7e		/* short, what 49 computes and hands
					   to `ApplyBulkDelay`             */
#define T3MT_F35A0	0x35a0		/* short, 51 clears on both paths  */
#define T3MT_F35A2	0x35a2		/* short, 51's is_short path only  */
#define T3MT_INFOREC	0xa9ac		/* 51's message record, and the one
					   58's head is aimed at so that it is
					   NOT the one its resets fill       */
#define T3MT_SELFPTR	0xaa6c		/* 58's head reads through this     */
#define T3MT_RXBAUD	0xaa96		/* short, 51 copies the tx baud    */
#define T3MT_RXCARRIER	0xaaa8		/* short, and the tx carrier       */

/* Inside the receiver, which is the object's +0x264. */
#define T3MT_RX_FLAGS	(0x0264 + 0x122)	/* short */
#define T3MT_RX_LEVEL	(0x0264 + 0x134)	/* short, agc_level */
#define T3MT_RX_GAIN	(0x0264 + 0x136)	/* short, agc_gain  */
#define T3MT_RX_F264	(0x0264 + 0x264)	/* short, 49's last write */
#define T3MT_RX_F1D2	(0x0264 + 0x1d2)	/* short, rxtiminginit's  */
#define T3MT_RX_F1BA	(0x0264 + 0x1ba)	/* short, the carrier len */
#define T3MT_RX_F1AE	(0x0264 + 0x1ae)	/* short, a 2400-baud word*/

static int dump;

/* Which paths the trials actually reached, so the claims below can be made. */
static int saw_below, saw_toggle, saw_force, saw_trace, saw_fsk;

/*
 * One trial: seed the counter, drive the microstate, step, compare.
 *
 * `ours` picks side A.  Everything else is identical between the two runs,
 * which is what makes the pair a control rather than two unrelated tests.
 */
/*
 * What one trial seeds beyond the state words.  A struct rather than eight
 * arguments because the tail's branches want five of them set at once, and
 * because a trial that leaves a field alone has to be visibly different from
 * one that sets it to zero -- finding F230's rule: zero is the value that
 * makes an unwritten field look deliberate.
 */
struct seed {
	short	txstate;
	short	counter;
	int	set_mode;	int	mode;
	int	set_time;	int	elapsed, deadline;
	int	set_level;	short	level;	int	floor;
	int	set_tick;	int	tick;
	int	set_flags;	short	flags;
	int	set_answer;	short	answer;
	int	set_baud;	short	baud;
	int	set_vectidx;	short	vectidx;
	/*
	 * `fsk_inhibit` at +0x402 makes `fskdemodulate` return without doing
	 * anything, and the fixture's varied fill leaves it non-zero -- so
	 * every trial that does not clear it tests the CALL and not the
	 * demodulator.  Clearing it is what makes the second argument, the
	 * receiver's +0x10c, a claim a test can fail.
	 */
	int	set_fsk;
	/*
	 * Arm 47's two entry guards and the two fields its record depends on,
	 * and arm 63's three.  Each is a separate `set_` so that "left as the
	 * fixture filled it" and "set to zero" are different trials --
	 * finding F230 again, and arm 47's second guard is satisfied by ZERO,
	 * so a trial that cannot tell the two apart proves nothing.
	 */
	int	set_sr;		short	sr;
	int	set_f3588;	short	short_3588;
	int	set_pcmrx;	int	v90rx, k56rx;
	int	set_ptc;	int	ptc;
	int	set_p3;		int	p3gate;	int	p3count;
	/*
	 * The thirteen words arm 47's reset clears, filled with VARIED
	 * non-zero values.  Without this the reset's clears are half untested:
	 * `v34handshakinit` already leaves several of them zero, and a clear
	 * of a word that is already zero is a claim no comparison can fail.
	 */
	int	set_errrec;	short	errrec;
	/*
	 * The rate config arm 63 hands `V34SetupModulator` -- baud, carrier
	 * and pre-emphasis index, which `setfinalrate` writes as raw offsets.
	 * WITHOUT THIS THE SET-UP PATH TESTS THE ERROR ARMS: the fixture's
	 * pseudorandom halfwords are not a V.34 rate, `V34SetupModulator`
	 * prints "invalid baudrate" and returns, and "63 sets the transmitter
	 * up" degrades to "63 reached a diagnostic".  Finding F277's question,
	 * asked of the modulator rather than of a comparison.
	 */
	int	set_rate;	short	baudrate, carrier, preemp;
	/*
	 * Arms 49's, 50's and 51's.  `filt` is the field three of the four
	 * thresholds in 49 and 50 are measured from, so a suite that leaves
	 * it as the fixture filled it is a suite whose thresholds are one
	 * unknown constant each -- which is exactly the vacuity finding F288
	 * warns about, in the form it takes for these two arms.
	 *
	 * `isshort` is applied AFTER `errrec`, because `errrec` fills +0xabcc
	 * along with the twelve words either side of it and these two arms
	 * read it as a selector rather than as something to clear.
	 */
	int	set_filt;	short	filt;
	int	set_isshort;	short	isshort;
	int	set_prevbd;	short	prevbd;
	int	set_gain;	short	gain;
	/*
	 * Arm 59's.  `short_358a` is the OTHER half of the 32-bit word its last
	 * block compares against 0x20002, so a suite that sets +0x3588 and
	 * leaves +0x358a as the fixture filled it is a suite whose last block
	 * fires or does not on one pseudorandom halfword.
	 *
	 * `short_19e` gates whether the tone detector is consulted at all, and
	 * `det` seeds the detector itself: without it `tone_detect` returns
	 * whatever the fill happens to make it return, and "59 retrains when
	 * the detector asserts" is a claim about an unknown.  Both answers are
	 * driven -- finding F277's question, asked of a called function.
	 */
	int	set_f358a;	short	short_358a;
	int	set_f19e;	short	short_19e;
	int	set_det;	int	det_assert;
	/*
	 * `v34handshakinit`'s mode-1 body bumps one of two counters on
	 * `(rx->flags & 0x40) || m[0xac17]`, so 59's `flags |= 0x40` is
	 * invisible unless +0xac17 is zero and the flag was clear.  Both
	 * counters are seeded too: an increment of a field holding a
	 * pseudorandom value is a claim, but one nobody can read back.
	 */
	int	set_retrainq;	short	retrainq;
	/*
	 * Arm 58's head, which is the only guard in this file that reaches a
	 * message record THROUGH the self-pointer at +0xaa6c rather than at a
	 * fixed offset.  The fixture aims every pointer field at a dummy block
	 * before anything runs, so a trial that wants the guard's clear to
	 * land somewhere a comparison can read has to aim it into the object
	 * itself -- `v34hs_poke_self_ptr` and never `v34hs_poke_int`, because
	 * the two objects are at two addresses and one address written into
	 * both is exactly the asymmetry findings F319-322 are about.
	 *
	 * The record at +0xa94c is seeded with sentinels FIRST and the aimed
	 * one second, so `rec_at` may name either and a trial that aims
	 * elsewhere can assert that +0xa94c did not move -- which is what
	 * separates "read through the pointer" from "read at +0xa94c".
	 */
	int	set_rec;	unsigned rec_at;	short rec20, rec22;
	/*
	 * Arm 55's first guard, and it is the only guard in this file that is
	 * not a field: it is `fsk.nbits` BEFORE `fskdemodulate` against
	 * `fsk.nbits` after it (0x64a9e against 0x65b79).  So a trial has to
	 * decide whether the demodulator samples a bit, and that is
	 * `phase >= next` on the first of its three iterations.
	 *
	 * `bit_len` and `resync` at 0x400 with `next` at 0 and `phase` at
	 * 0x100 sample EXACTLY ONE bit: the sample sets `next` to `bit_len`,
	 * and the phase counter cannot reach it again in two more iterations
	 * whether or not the input changes sign.  One bit is what makes the
	 * shift register predictable -- `sr` becomes `(sr << 1) | bitval` --
	 * so the `& 7 == 7` test can be probed one bit at a time.
	 */
	int	set_bitclk;	short	nbits, phase, next, bit_len, resync;
	short	bitval;
};

#define T3MT_RETRAINQ	0xac17		/* SIGNED byte, the other input     */
#define T3MT_RETRAIN_A	0xac14		/* short, bumped when either is set */
#define T3MT_RETRAIN_B	0xac12		/* short, bumped when neither is    */

#define T3MT_FSKINHIBIT	0x0402

/* Arm 59's, and the detector it hands `tone_detect`. */
#define T3MT_RX_F19E	(0x0264 + 0x19e)	/* short, the retrain gate  */
#define T3MT_RX_CARRIER	(0x0264 + 0x1b4)	/* const short *, the table
						   V34SetupDemodulator picks */
#define T3MT_DET	0x3564			/* struct v34_detector      */

/*
 * MAKE `tone_detect` ANSWER, rather than hoping.  Absence mode with both
 * thresholds at 0x7fff counts every call and resets on none, so the answer is
 * `count > limit` and the limit is the only thing that decides it -- whatever
 * the filter did to the level, and whatever the fixture left in the taps.
 * Detector layout is `struct v34_detector` in include/dsplib/v34det.h.
 */
static void
det_seed(int assert_it)
{
	v34hs_poke_short(T3MT_DET + 0x04, 1);		/* polarity: absence */
	v34hs_poke_short(T3MT_DET + 0x06, 0);		/* armed, unread here*/
	v34hs_poke_short(T3MT_DET + 0x08, 0x64);	/* count             */
	v34hs_poke_short(T3MT_DET + 0x0a,
			 (short)(assert_it ? 0 : 0x7ff0));	/* limit     */
	v34hs_poke_short(T3MT_DET + 0x0c, V34_DET_STATE_RUNNING);
	v34hs_poke_short(T3MT_DET + 0x0e, 0x7fff);	/* thresh_hi         */
	v34hs_poke_short(T3MT_DET + 0x10, 0x7fff);	/* thresh_lo         */
	/*
	 * And a level nothing else writes, so that "did the filter loop run at
	 * all" is a reading rather than an assumption.  It is the only thing
	 * that can make the WINDOW arguments testable: with an empty window
	 * the two ends can be exchanged and no byte moves.
	 */
	v34hs_poke_short(T3MT_DET + 0x12, 0x1234);
}

static void
apply(const struct seed *s)
{
	if (s->set_mode)
		v34hs_poke_int(T3MT_MODE, s->mode);
	if (s->set_time) {
		v34hs_poke_int(T3MT_ELAPSED, s->elapsed);
		v34hs_poke_int(T3MT_DEADLINE, s->deadline);
	}
	if (s->set_level) {
		v34hs_poke_short(T3MT_RX_LEVEL, s->level);
		v34hs_poke_int(T3MT_FLOOR, s->floor);
	}
	if (s->set_tick)
		v34hs_poke_int(T3MT_TICK, s->tick);
	if (s->set_flags)
		v34hs_poke_short(T3MT_RX_FLAGS, s->flags);
	if (s->set_answer)
		v34hs_poke_short(T3MT_ANSWER, s->answer);
	if (s->set_baud)
		v34hs_poke_short(T3MT_BAUD, s->baud);
	if (s->set_vectidx)
		v34hs_poke_short(T3MT_VECTIDX, s->vectidx);
	if (s->set_sr)
		v34hs_poke_short(T3MT_FSK_SR, s->sr);
	if (s->set_f3588)
		v34hs_poke_short(T3MT_F3588, s->short_3588);
	if (s->set_pcmrx) {
		v34hs_poke_int(T3MT_V90RX, s->v90rx);
		v34hs_poke_int(T3MT_K56RX, s->k56rx);
	}
	if (s->set_ptc)
		v34hs_poke_int(T3MT_PTC, s->ptc);
	if (s->set_p3) {
		v34hs_poke_byte(T3MT_P3GATE, (unsigned char)s->p3gate);
		v34hs_poke_int(T3MT_P3COUNT, s->p3count);
	}
	if (s->set_rate) {
		v34hs_poke_short(T3MT_BAUDRATE, s->baudrate);
		v34hs_poke_short(T3MT_CARRIER, s->carrier);
		v34hs_poke_short(T3MT_PREEMP, s->preemp);
	}
	if (s->set_errrec) {
		int k;

		v34hs_poke_short(0xabca, (short)(s->errrec ^ 0x0011));
		v34hs_poke_short(0xabcc, (short)(s->errrec ^ 0x0022));
		for (k = 0; k <= 10; k++)
			v34hs_poke_short((unsigned)(0xabae + 2 * k),
					 (short)(s->errrec + 0x137 * k));
		v34hs_poke_short(T3MT_FSK_NBITS, (short)(s->errrec ^ 0x0033));
	}
	if (s->set_filt)
		v34hs_poke_short(T3MT_FILTDELAY, s->filt);
	if (s->set_prevbd)
		v34hs_poke_short(T3MT_PREVBD, s->prevbd);
	if (s->set_gain)
		v34hs_poke_short(T3MT_RX_GAIN, s->gain);
	if (s->set_isshort)
		v34hs_poke_short(T3MT_ISSHORT, s->isshort);
	if (s->set_f358a)
		v34hs_poke_short(T3MT_F358A, s->short_358a);
	if (s->set_f19e)
		v34hs_poke_short(T3MT_RX_F19E, s->short_19e);
	if (s->set_det)
		det_seed(s->det_assert);
	if (s->set_retrainq) {
		v34hs_poke_byte(T3MT_RETRAINQ, (unsigned char)s->retrainq);
		v34hs_poke_short(T3MT_RETRAIN_A, 0x0111);
		v34hs_poke_short(T3MT_RETRAIN_B, 0x0222);
	}
	if (s->set_rec) {
		v34hs_poke_short(T3MT_MSGREC0 + 0x20, 0x7a11);
		v34hs_poke_short(T3MT_MSGREC0 + 0x22, 0x7a22);
		v34hs_poke_self_ptr(T3MT_SELFPTR, s->rec_at);
		v34hs_poke_short(s->rec_at + 0x20, s->rec20);
		v34hs_poke_short(s->rec_at + 0x22, s->rec22);
	}
	/* AFTER `set_errrec`, which also writes `fsk.nbits`. */
	if (s->set_bitclk) {
		v34hs_poke_short(T3MT_FSK_NBITS, s->nbits);
		v34hs_poke_short(T3MT_FSK_BITLO, s->bitval);
		v34hs_poke_short(T3MT_FSK_BITHI, s->bitval);
		v34hs_poke_short(T3MT_FSK_BITLEN, s->bit_len);
		v34hs_poke_short(T3MT_FSK_RESYNC, s->resync);
		v34hs_poke_short(T3MT_FSK_PHASE, s->phase);
		v34hs_poke_short(T3MT_FSK_NEXT, s->next);
	}
	if (s->set_fsk) {
		int k;

		/*
		 * The bring-up leaves the receiver's sample buffer at +0x10c
		 * ZERO, and `fskdetect` reads exactly four shorts from it --
		 * so with the buffer as the fixture leaves it, an input
		 * pointer one short out reads zeroes either way and the whole
		 * chain lands on the same object.  Nine varied shorts across
		 * +0x108..+0x118 is what makes the pointer a claim: they
		 * differ from each other, so a window shifted by one short is
		 * a different window.  Finding F230's rule, in the form
		 * finding F277 gives it -- values can be varied and still all
		 * lie in the set where the operation is the identity.
		 */
		v34hs_poke_short(T3MT_FSKINHIBIT, 0);
		for (k = 3; k < 66; k++)
			v34hs_poke_int(0x0264 + 4 * k,
				       (int)(0x1234 + 0x2f1d * k));
	}
}

static const struct seed plain = { T3MT_TXSTATE, 0x0100, 0,0, 0,0,0,
				   0,0,0, 0,0, 0,0, 0,0, 0,0, 0,0, 0,
				   0,0, 0,0, 0,0,0, 0,0, 0,0,0, 0,0,
				   0,0,0,0, 0,0, 0,0, 0,0, 0,0,
				   0,0, 0,0, 0,0, 0,0,
				   0,0,0,0, 0,0,0,0,0,0, 0 };

/*
 * Did the BLOB print this?  Every claim below about a path having been taken
 * is made against side B, so what is asserted is the object's behaviour and
 * our arm only has to match it.
 */
static int
blob_said(const char *what)
{
	return strstr(v34hs_text(1), what) != NULL;
}

/*
 * DID THE TWO SIDES SELECT THE SAME LIBRARY TABLE?
 *
 * A pointer at one of the thirty-five skipped offsets that points OUTSIDE
 * both arenas is a library table, and side A holds ours while side B holds
 * the blob's -- two addresses of two copies, which no address comparison can
 * tell from two different tables (finding F324).  `v34hs_compare` therefore
 * cannot see an arm that picks `scale2800` where the object picks
 * `scale2400`, and six such mutations survived until this existed.
 *
 * What CAN be compared is what the two addresses point AT.  The five power
 * scales are pairwise distinct and so are the two carrier descriptors, so
 * equal contents means the same table -- and it is still a comparison against
 * the blob rather than against a constant this file chose.
 */
static void
same_table(const char *what, unsigned off, unsigned shorts, long tag)
{
	const short *a = *(const short **)((char *)v34hs_object(0) + off);
	const short *b = *(const short **)((char *)v34hs_object(1) + off);

	diff_eq_int(what, memcmp(a, b, shorts * sizeof(short)) == 0, 1, tag);
}

/*
 * Where the self-pointer at +0xaa6c points, as an offset into its OWN object.
 * `v34hs_compare` skips those four bytes because the two sides hold two
 * different addresses, so the only thing a test can compare is the offset --
 * which is also the only thing the object's own behaviour depends on.
 */
static long
selfptr_offset(int side)
{
	const char *p = *(const char *const *)((char *)v34hs_object(side)
					       + T3MT_SELFPTR);

	return p - (const char *)v34hs_object(side);
}

static void
trial_seeded(short mst, const struct seed *s, int ours, long tag)
{
	char what[96];

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(mst, V34HS_RX_DPSK, s->txstate);
	v34hs_poke_short(T3MT_COUNTER, s->counter);
	apply(s);

	v34hs_ours(ours);
	v34hs_step();

	snprintf(what, sizeof(what),
		 "microstate %d, tx %d, counter 0x%x, %s",
		 (int)mst, (int)s->txstate,
		 (unsigned)(unsigned short)s->counter,
		 ours ? "ours" : "blob");
	v34hs_compare(what, tag);

	if (dump) {
		const struct v34hs_obs *a = v34hs_observed(0);
		const struct v34hs_obs *b = v34hs_observed(1);

		printf("  %-40s  ours wrote %4u B sig %08x lines %u | "
		       "blob wrote %4u B sig %08x lines %u | "
		       "counter %04x -> %04x\n",
		       what, a->changed, a->hash, a->lines,
		       b->changed, b->hash, b->lines,
		       (unsigned)(unsigned short)s->counter,
		       (unsigned)(unsigned short)v34hs_peek_short(1,
								 T3MT_COUNTER));
	}
}

/*
 * A pair of trials -- ours and the blob -- plus the assertion that the step
 * did something at all.
 *
 * THE MOTION GUARD IS NOT OPTIONAL.  Every one of these arms writes at least
 * the counter, so a step that wrote nothing is a step that never reached the
 * arm; without this an entry that returned early would pass every byte
 * comparison in the file.  Finding F223 records the same shape from the other
 * direction: two objects agree when neither has moved.
 */
static void
both_seeded(short mst, const struct seed *s, long tag)
{
	trial_seeded(mst, s, 0, tag);
	trial_seeded(mst, s, 1, tag + 1);
	diff_eq_int("the step wrote something", v34hs_observed(0)->changed > 0,
		    1, tag);
}

/* The common case: nothing seeded but the counter, txstate held at SSEG. */
static void
both(short mst, short counter, long tag)
{
	struct seed s = plain;

	s.counter = counter;
	both_seeded(mst, &s, tag);
}

/*
 * --------------------------------------------------------------------------
 * 0x65d30 -- microstate 48 `TX_PHASE3_ANS`.
 *
 * The counter is incremented as an unsigned halfword and stored back before
 * either threshold is tested, and both thresholds are exact equalities.  Four
 * trials, chosen so that each of the three paths is taken and so that the two
 * thresholds cannot be exchanged for one another:
 *
 *      0x0100  neither: the ordinary path, txstate unchanged
 *      0x0077  becomes 0x78: inverts bit 0 of +0x358c
 *      0x00a1  becomes 0xa2: forces txstate to SILENCE and microstate to
 *              RX_PHASE2_ANS, clears the counter, and leaves through table
 *              2's txstate-5 arm rather than its SSEG arm
 *      0x00a2  becomes 0xa3: neither again, and it is the trial that says
 *              the 0xa2 test is an equality -- "at least 0xa2" would fire
 *
 * A fifth, 0x0079, is the same statement for the 0x78 threshold.
 */
static void
micro48(void)
{
	short before;

	both(V34HS_TX_PHASE3_ANS, 0x0100, 4800);
	saw_below = 1;

	/*
	 * The same, with `fsk_inhibit` cleared so `fskdemodulate` actually
	 * runs.  The fixture's fill leaves +0x402 non-zero, and every other
	 * trial in this file therefore exercises the CALL and not the
	 * demodulator -- which leaves the second argument, the receiver's
	 * +0x10c, unchecked.  Two trials rather than one, because the
	 * demodulator's own state advances and a single call is a weaker
	 * statement than a call whose input it disagrees about.
	 */
	{
		struct seed s = plain;

		s.set_fsk = 1;
		both_seeded(V34HS_TX_PHASE3_ANS, &s, 4802);
		s.counter = 0x0077;
		both_seeded(V34HS_TX_PHASE3_ANS, &s, 4804);
		saw_fsk = 1;
	}

	/*
	 * The 0x78 arm, and the proof it was the 0x78 arm: +0x358c comes back
	 * inverted in bit 0 and in nothing else.  Read off the BLOB's object,
	 * so it is a statement about the blob and not about our arm.
	 */
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_poke_short(T3MT_COUNTER, 0x0077);
	before = v34hs_peek_short(1, T3MT_TOGGLE);

	both(V34HS_TX_PHASE3_ANS, 0x0077, 4810);
	diff_eq_int("48 at 0x78 inverts bit 0 of +0x358c",
		    (unsigned short)(v34hs_peek_short(1, T3MT_TOGGLE)
				     ^ before),
		    1, 0x78);
	diff_eq_int("48 at 0x78 leaves the counter at 0x78",
		    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
		    0x78, 0x78);
	saw_toggle = 1;

	both(V34HS_TX_PHASE3_ANS, 0x0079, 4820);
	diff_eq_int("48 at 0x7a does not take the 0x78 arm",
		    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
		    0x7a, 0x7a);

	/*
	 * The 0xa2 arm.  Three separate consequences, each read off the blob:
	 * the counter is cleared, txstate becomes SILENCE and the microstate
	 * becomes RX_PHASE2_ANS.  Any one of them alone would be satisfied by
	 * a wrong arm that happened to write that field.
	 */
	both(V34HS_TX_PHASE3_ANS, 0x00a1, 4830);
	diff_eq_int("48 at 0xa2 clears the counter",
		    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER), 0, 0xa2);
	diff_eq_int("48 at 0xa2 forces txstate to SILENCE",
		    v34hs_peek_short(1, V34HS_TXSTATE_OFF), V34HS_SILENCE,
		    0xa2);
	diff_eq_int("48 at 0xa2 forces the microstate to RX_PHASE2_ANS",
		    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
		    V34HS_RX_PHASE2_ANS, 0xa2);
	/*
	 * And it announces both, which is the half of the evidence the byte
	 * comparison cannot give: two lines, one per machine, and the
	 * transcripts are compared in full by `v34hs_compare`.
	 */
	diff_eq_int("48 at 0xa2 prints both transitions",
		    v34hs_observed(1)->lines, 2, 0xa2);
	saw_force = 1;
	saw_trace = 1;

	both(V34HS_TX_PHASE3_ANS, 0x00a2, 4840);
	diff_eq_int("48 at 0xa3 does not take the 0xa2 arm",
		    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
		    0xa3, 0xa3);
}

/*
 * --------------------------------------------------------------------------
 * 0x66834 -- microstates 47 `TX_PHASE2_ANS` and 56 `TX_PHASE2_CALL`.
 *
 * A counter with ONE threshold and a reset in front of it.  The reset is what
 * makes this arm different from 48's shape: it is guarded by the FSK shift
 * register's low four bits and by +0x3588, it fills a twelve-field message
 * record, announces two transitions and prints a third line of its own -- and
 * then FALLS THROUGH into the counter, having cleared it, so a step that takes
 * it always leaves with the counter at 1 and cannot be recognised by the
 * counter alone.
 *
 * Four things every trial here has to pin down, and each has a trial on both
 * sides of it:
 *
 *   - `(sr & 0xf) == 0xf` is a MASK, not an equality: 0x000f, 0x5aaf and
 *     0xffff must all take it and 0x5aae must not.
 *   - the second guard is satisfied by ZERO, which is the value a field
 *     nothing has written also holds -- so it gets a non-zero trial.
 *   - the record's +0x18 is 0x1e only when this end originates AND a V.90
 *     receiver is running.  The K56flex one is set to a different non-zero
 *     value throughout, because `v34setuptxmit` reads BOTH and this arm reads
 *     only the first: a trial that set them together could not tell them
 *     apart.
 *   - the threshold is `<= 0x5f` on a SIGNED 16-bit value after an unsigned
 *     increment, so 0x7fff and 0xffff take the low arm and 0x5f does not.
 */
static int saw_reset47, saw_txl1, saw_collide;

static void
micro47_counter(short mst, short counter, short after, int moved, long tag)
{
	struct seed s = plain;
	char what[96];

	s.counter = counter;
	s.set_sr = 1;
	s.sr = 0x5aae;			/* low nibble 0xe: no reset */
	s.set_f3588 = 1;
	s.short_3588 = 0;			/* and the OTHER guard satisfied */
	both_seeded(mst, &s, tag);

	snprintf(what, sizeof(what), "47 at counter 0x%04x leaves 0x%04x",
		 (unsigned)(unsigned short)counter,
		 (unsigned)(unsigned short)after);
	diff_eq_int(what, (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
		    (unsigned short)after, tag);

	/*
	 * And WHICH arm it was, which the counter alone cannot say: the high
	 * arm is the only one that moves the microstate to TX_L1.
	 */
	snprintf(what, sizeof(what), "47 at counter 0x%04x %s TX_L1",
		 (unsigned)(unsigned short)counter, moved ? "reaches" : "does not reach");
	diff_eq_int(what, v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
		    moved ? V34HS_TX_L1 : mst, tag);
}

static void
micro47(void)
{
	struct seed s;
	long tag = 4600;
	short toggle;
	int i;

	/*
	 * The threshold, from both sides and at both wraps.  0x5e steps to
	 * 0x5f and stays; 0x5f steps to 0x60 and takes the high arm; 0x7fff
	 * steps to a NEGATIVE halfword and 0xffff steps to zero, and both of
	 * those take the low arm -- which is what says the compare is signed
	 * and sixteen bits wide rather than either one alone.
	 */
	micro47_counter(V34HS_TX_PHASE2_ANS, 0x005e, 0x005f, 0, 4600);
	micro47_counter(V34HS_TX_PHASE2_ANS, 0x005f, 0x0000, 1, 4610);
	saw_txl1 = 1;
	micro47_counter(V34HS_TX_PHASE2_ANS, 0x7fff, (short)0x8000, 0, 4620);
	micro47_counter(V34HS_TX_PHASE2_ANS, (short)0xffff, 0x0000, 0, 4630);
	micro47_counter(V34HS_TX_PHASE2_ANS, 0x0100, 0x0000, 1, 4640);

	/*
	 * The high arm inverts bit 0 of +0x358c and NOTHING else in it, and it
	 * prints exactly one line -- the microstate transition, whose `[2]` is
	 * the counter it has just cleared and therefore zero.  Read off the
	 * blob, so what is asserted is the blob's behaviour.
	 */
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	toggle = v34hs_peek_short(1, T3MT_TOGGLE);
	micro47_counter(V34HS_TX_PHASE2_ANS, 0x005f, 0x0000, 1, 4650);
	diff_eq_int("47's high arm inverts bit 0 of +0x358c",
		    (unsigned short)(v34hs_peek_short(1, T3MT_TOGGLE) ^ toggle),
		    1, 0x5f);
	diff_eq_int("47's high arm prints one transition",
		    v34hs_observed(1)->lines, 1, 0x5f);

	/*
	 * The reset.  Three trials for the record's +0x18 and two for each
	 * guard, and the seeded counter is chosen so that the fall-through
	 * cannot be confused with the high arm.
	 */
	for (i = 0; i < 6; i++) {
		/*
		 * 0x5aa7 is the trial that says the mask is FOUR bits: its low
		 * three are all ones and its fourth is not, so a guard reading
		 * `& 7` would take the reset here and the object does not.
		 */
		static const short sr[6] = { 0x5aaf, 0x000f, (short)0xffff,
					     0x5aae, 0x5aaf, 0x5aa7 };
		static const short g[6]  = { 0, 0, 0, 0, 0x0123, 0 };
		static const short ans[6] = { 0x65, 0x65, 0x66, 0x65, 0x65, 0x65 };
		static const int v90[6] = { 0x2f1d, 0, 0x2f1d, 0x2f1d, 0x2f1d,
					    0x2f1d };
		int reset = i < 3;
		char what[96];

		s = plain;
		s.counter = 0x0033;
		s.set_sr = 1;
		s.sr = sr[i];
		s.set_f3588 = 1;
		s.short_3588 = g[i];
		s.set_answer = 1;
		s.answer = ans[i];
		s.set_errrec = 1;
		s.errrec = 0x2f1d;
		s.set_pcmrx = 1;
		s.v90rx = v90[i];
		s.k56rx = 0x4b1f;	/* non-zero throughout: NOT read here */
		both_seeded(V34HS_TX_PHASE2_ANS, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what),
			 "47 with sr 0x%04x and +0x3588 0x%04x %s the reset",
			 (unsigned)(unsigned short)sr[i],
			 (unsigned)(unsigned short)g[i],
			 reset ? "takes" : "skips");
		diff_eq_int(what, v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    reset ? V34HS_DET_SYNC : V34HS_TX_PHASE2_ANS, tag);

		if (!reset) {
			diff_eq_int("47 without the reset prints nothing",
				    v34hs_observed(1)->lines, 0, tag);
			diff_eq_int("47 without the reset just bumps the counter",
				    (unsigned short)v34hs_peek_short(1,
								     T3MT_COUNTER),
				    0x0034, tag);
			continue;
		}

		/*
		 * Every consequence of the reset, separately.  Any one of them
		 * alone would be satisfied by a wrong arm that happened to
		 * write that field.
		 */
		diff_eq_int("47's reset moves the txstate to TX_DPSK",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    V34HS_TX_DPSK, tag);
		diff_eq_int("47's reset prints three lines",
			    v34hs_observed(1)->lines, 3, tag);
		diff_eq_int("47's reset sets +0x3588 to 4",
			    v34hs_peek_short(1, T3MT_F3588), 4, tag);
		diff_eq_int("47's reset sets +0x358a to 1",
			    v34hs_peek_short(1, T3MT_F358A), 1, tag);
		diff_eq_int("47's reset sets the shift register to -1",
			    v34hs_peek_short(1, T3MT_FSK_SR), -1, tag);
		diff_eq_int("47's reset clears the bit count",
			    v34hs_peek_short(1, T3MT_FSK_NBITS), 0, tag);
		diff_eq_int("47's reset clears +0xabc2",
			    v34hs_peek_short(1, T3MT_FABC2), 0, tag);
		diff_eq_int("47's reset clears +0xabca",
			    v34hs_peek_short(1, 0xabca), 0, tag);
		diff_eq_int("47's reset clears +0xabcc",
			    v34hs_peek_short(1, 0xabcc), 0, tag);
		{
			int k, any = 0;

			for (k = 0; k <= 9; k++)
				any |= (unsigned short)v34hs_peek_short(1,
					(unsigned)(0xabae + 2 * k));
			diff_eq_int("47's reset clears the ten shorts at "
				    "+0xabae", any, 0, tag);
		}
		diff_eq_int("47's reset falls through and leaves the counter 1",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    1, tag);

		/* The record, field by field, including the one that varies. */
		diff_eq_int("47's record +0x14",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x14), -1, tag);
		diff_eq_int("47's record +0x16",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x16), 1, tag);
		diff_eq_int("47's record +0x18 is 0x1e only when originating "
			    "with a V.90 receiver",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x18),
			    i == 0 ? 0x1e : 0x11, tag);
		diff_eq_int("47's record +0x1a",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x1a), 0, tag);
		diff_eq_int("47's record +0x1c",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x1c), 8, tag);
		diff_eq_int("47's record +0x1e",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x1e), 0, tag);
		diff_eq_int("47's record +0x20",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x20), 1, tag);
		diff_eq_int("47's record +0x22",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x22), 0, tag);
		diff_eq_int("47's record +0x24 low",
			    (unsigned short)v34hs_peek_short(1,
							     T3MT_MSGREC0 + 0x24),
			    0xf72, tag);
		diff_eq_int("47's record +0x26 high",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x26), 0, tag);
		diff_eq_int("47's record +0x28",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x28), 0xc, tag);
		diff_eq_int("47's record +0x2a",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x2a), 0xc, tag);
		diff_eq_int("47's record +0x2c low",
			    (unsigned short)v34hs_peek_short(1,
							     T3MT_MSGREC0 + 0x2c),
			    0xf72, tag);
		diff_eq_int("47's record +0x2e high",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x2e), 0, tag);
		saw_reset47 = 1;
	}
}

/*
 * --------------------------------------------------------------------------
 * 47 AND 56 ARE ONE ARM, and this is what says so rather than assuming it.
 *
 * .rodata+0x3000's entries 6 and 15 hold the same address, which is a fact
 * about the table and not about the code that runs; the check is that the
 * blob, driven with each of the two microstates and everything else identical,
 * leaves the identical object and the identical signature.
 *
 * +0x3592 IS EXCLUDED FROM THE BYTE COMPARISON and nothing else is.  It holds
 * the value the case was entered with on any path that does not overwrite it,
 * so including it would call the two cases different for the one reason that
 * cannot be evidence -- finding F290, which is also why `observe` hashes only
 * bytes that CHANGED and hashes the byte's NEW value: 47 -> 41 and 56 -> 41
 * contribute the same pair, so the signature is comparable as it stands.
 *
 * THE TRANSCRIPTS ARE NOT COMPARED, and that is not a weakening: the arm
 * announces the microstate it is LEAVING, so the two runs necessarily print
 * `TX_PHASE2_ANS` and `TX_PHASE2_CALL`.  What is compared is the line COUNT,
 * which must agree.
 */
static void
collide(const struct seed *s, const char *what, long tag)
{
	static unsigned char snap[2][sizeof(struct v34_object)];
	unsigned changed, hash, lines, i, diffs = 0, first = ~0u;
	char msg[128];

	both_seeded(V34HS_TX_PHASE2_ANS, s, tag);
	changed = v34hs_observed(1)->changed;
	hash = v34hs_observed(1)->hash;
	lines = v34hs_observed(1)->lines;
	memcpy(snap[0], v34hs_object(1), sizeof(snap[0]));

	both_seeded(V34HS_TX_PHASE2_CALL, s, tag + 2);
	memcpy(snap[1], v34hs_object(1), sizeof(snap[1]));

	snprintf(msg, sizeof(msg), "47 and 56 write the same bytes, %s", what);
	diff_eq_int(msg, (int)v34hs_observed(1)->changed, (int)changed, tag);
	snprintf(msg, sizeof(msg), "47 and 56 have one signature, %s", what);
	diff_eq_int(msg, (int)v34hs_observed(1)->hash, (int)hash, tag);
	snprintf(msg, sizeof(msg), "47 and 56 print the same count, %s", what);
	diff_eq_int(msg, (int)v34hs_observed(1)->lines, (int)lines, tag);

	for (i = 0; i < sizeof(snap[0]); i++) {
		if (i == V34HS_MICROSTATE_OFF || i == V34HS_MICROSTATE_OFF + 1)
			continue;
		if (snap[0][i] == snap[1][i])
			continue;
		diffs++;
		if (first == ~0u)
			first = i;
	}
	snprintf(msg, sizeof(msg),
		 "47 and 56 leave the identical object (+0x3592 aside), %s",
		 what);
	diff_eq_int(msg, (int)diffs, 0, (long)first);
	saw_collide = 1;
}

static void
micro47_56(void)
{
	struct seed s;

	/* The low arm, where the microstate is not written at all. */
	s = plain;
	s.counter = 0x005e;
	s.set_sr = 1;	s.sr = 0x5aae;
	s.set_f3588 = 1; s.short_3588 = 0;
	collide(&s, "below the threshold", 4700);

	/* The high arm, where it is overwritten with TX_L1 and announced. */
	s.counter = 0x005f;
	collide(&s, "at the threshold", 4710);

	/* And the reset, which writes seventy-odd bytes and prints three. */
	s.counter = 0x0033;
	s.sr = 0x5aaf;
	s.set_answer = 1;	s.answer = 0x65;
	s.set_pcmrx = 1;	s.v90rx = 0x2f1d;	s.k56rx = 0x4b1f;
	collide(&s, "through the reset", 4720);
}

/*
 * --------------------------------------------------------------------------
 * 0x6591e -- microstate 63 `INFODONE`.
 *
 * THE TXSTATE IS THE SELECTOR HERE and not a companion field: 60 TONE_AB and
 * 5 SILENCE run two different bodies and every other txstate leaves at once,
 * which is why this arm is indistinguishable from the twenty-four-state shared
 * arm when the fixture drives it cold at SSEG.
 */
static int saw_63_leave, saw_63_tone, saw_63_orig, saw_63_ans, saw_63_setup;

static void
micro63(void)
{
	struct seed s;
	long tag = 4750;
	int i;

	/*
	 * Every txstate that is not 60 or 5 leaves with the object's own
	 * txstate, prints nothing and moves nothing.  Three of them, because
	 * one would not say the test is `!= 5` rather than `< 5` or `> 5`.
	 */
	for (i = 0; i < 3; i++) {
		static const short tx[3] = { 4, V34HS_SSEG, 74 };

		s = plain;
		s.txstate = tx[i];
		both_seeded(V34HS_INFODONE, &s, tag);
		tag += 2;
		diff_eq_int("63 at an uninteresting txstate moves nothing",
			    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    V34HS_INFODONE, tag);
		diff_eq_int("63 at an uninteresting txstate prints nothing",
			    v34hs_observed(1)->lines, 0, tag);
		saw_63_leave = 1;
	}

	/*
	 * TONE_AB, below the threshold.  `<= 0xf` on the SIGNED halfword after
	 * an unsigned increment, so 0xe stays, 0xf crosses, and 0x7fff and
	 * 0xffff both stay because they step to a negative and to zero.
	 */
	for (i = 0; i < 3; i++) {
		static const short cnt[3] = { 0x000e, 0x7fff, (short)0xffff };
		static const short out[3] = { 0x000f, (short)0x8000, 0x0000 };

		s = plain;
		s.txstate = V34HS_TONE_AB;
		s.counter = cnt[i];
		both_seeded(V34HS_INFODONE, &s, tag);
		tag += 2;
		diff_eq_int("63 under the TONE_AB threshold stores the counter",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    (unsigned short)out[i], tag);
		diff_eq_int("63 under the TONE_AB threshold leaves the txstate",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    V34HS_TONE_AB, tag);
		diff_eq_int("63 under the TONE_AB threshold prints nothing",
			    v34hs_observed(1)->lines, 0, tag);
		saw_63_tone = 1;
	}

	/*
	 * The same arm ANSWERING, with the tail's five ways of overwriting
	 * progress seeded away.  This is the trial that says the value handed
	 * to the transmit dispatch here is TONE_AB and not the SILENCE the two
	 * endings above use: with the microstate still 63 and +0x359c at 0x66,
	 * table 2's SILENCE arm reports progress 1 by its first route and its
	 * TONE_AB arm reports 0, so the two are separable at last.
	 */
	s = plain;
	s.txstate = V34HS_TONE_AB;
	s.counter = 0x000e;
	s.set_answer = 1;	s.answer = 0x66;
	s.set_mode = 1;		s.mode = 0;
	s.set_time = 1;		s.elapsed = 100;  s.deadline = 200;
	s.set_level = 1;	s.level = 300;    s.floor = 100;
	both_seeded(V34HS_INFODONE, &s, tag);
	tag += 2;
	diff_eq_int("63 under the threshold leaves through TONE_AB and not "
		    "SILENCE", v34hs_observed(1)->progress, 0, tag);

	/*
	 * TONE_AB over the threshold, ANSWERING.  The txstate goes to SILENCE
	 * and the counter is REPLACED with one of two constants on `ptc` --
	 * both are driven, so the two cannot be exchanged.  The microstate is
	 * NOT moved, which is what separates this ending from the other one.
	 *
	 * It is also the first trial in this file to reach table 2's SILENCE
	 * arm by its FIRST route: the microstate is still 63 and +0x359c is
	 * 0x66, which is exactly what 0x64480 tests for progress 1.  The
	 * tail's five ways of overwriting progress are seeded away so that the
	 * claim is about the transmit arm and not about the tail.
	 */
	for (i = 0; i < 3; i++) {
		/*
		 * 0x10030 is the trial that says `ptc` is compared as a WHOLE
		 * INT: its low halfword is 0x30, so a halfword reading would
		 * take the 0x1e arm and the object takes the 0x96 one.
		 */
		static const int p[3] = { 0x31, 0x30, 0x10030 };
		static const int want[3] = { 0x96, 0x1e, 0x96 };

		s = plain;
		s.txstate = V34HS_TONE_AB;
		s.counter = 0x000f;
		s.set_answer = 1;	s.answer = 0x66;
		s.set_ptc = 1;		s.ptc = p[i];
		s.set_mode = 1;		s.mode = 0;
		s.set_time = 1;		s.elapsed = 100;  s.deadline = 200;
		s.set_level = 1;	s.level = 300;    s.floor = 100;
		both_seeded(V34HS_INFODONE, &s, tag);
		tag += 2;
		diff_eq_int("63 answering forces the txstate to SILENCE",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    V34HS_SILENCE, tag);
		diff_eq_int("63 answering leaves the microstate alone",
			    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    V34HS_INFODONE, tag);
		diff_eq_int("63 answering replaces the counter on ptc",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    want[i], tag);
		diff_eq_int("63 answering prints one transition",
			    v34hs_observed(1)->lines, 1, tag);
		diff_eq_int("63 answering reaches table 2's SILENCE arm by its "
			    "microstate route",
			    v34hs_observed(1)->progress, 1, tag);
		saw_63_ans = 1;
	}

	/*
	 * TONE_AB over the threshold, ORIGINATING.  A different ending: the
	 * counter KEEPS the incremented value, the microstate moves to
	 * DET_SYNC and +0xaae0 is cleared, and two transitions are printed.
	 * Progress is 0 here and 1 above, from the same table 2 arm, so the
	 * two endings are separated by five independent readings.
	 */
	s = plain;
	s.txstate = V34HS_TONE_AB;
	s.counter = 0x0044;
	s.set_answer = 1;	s.answer = 0x65;
	s.set_ptc = 1;		s.ptc = 0x30;
	s.set_errrec = 1;	s.errrec = 0x2f1d;	/* +0xaae0 non-zero */
	s.set_mode = 1;		s.mode = 0;
	s.set_time = 1;		s.elapsed = 100;  s.deadline = 200;
	s.set_level = 1;	s.level = 300;    s.floor = 100;
	both_seeded(V34HS_INFODONE, &s, tag);
	tag += 2;
	diff_eq_int("63 originating forces the txstate to SILENCE",
		    v34hs_peek_short(1, V34HS_TXSTATE_OFF), V34HS_SILENCE, tag);
	diff_eq_int("63 originating moves the microstate to DET_SYNC",
		    v34hs_peek_short(1, V34HS_MICROSTATE_OFF), V34HS_DET_SYNC,
		    tag);
	diff_eq_int("63 originating keeps the incremented counter",
		    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER), 0x0045,
		    tag);
	diff_eq_int("63 originating clears +0xaae0",
		    v34hs_peek_short(1, T3MT_FSK_NBITS), 0, tag);
	diff_eq_int("63 originating prints two transitions",
		    v34hs_observed(1)->lines, 2, tag);
	diff_eq_int("63 originating does not take the SILENCE arm's route",
		    v34hs_observed(1)->progress, 0, tag);
	saw_63_orig = 1;

	/*
	 * SILENCE, and its two extra guards.  Both are straddled at the exact
	 * boundary, and the first is a SIGNED byte -- so 0xff must fail it and
	 * 0x01 must pass, which no unsigned reading can produce.
	 */
	for (i = 0; i < 6; i++) {
		/*
		 * -1 in the int guard is the trial that says it is SIGNED: an
		 * unsigned reading makes it 0xffffffff and lets the set-up
		 * through, and the object does not.
		 */
		static const int gate[6] = { 0, -1, 1, 1, 1, 1 };
		static const int cnt[6]  = { 0x0400, 0x0400, 0x0240, 0x0241,
					     0x0400, -1 };
		static const int ok[6]   = { 0, 0, 0, 1, 1, 0 };
		/* Two real rates, so the two fields cannot be exchanged. */
		static const short bd[6] = { 2400, 2400, 2400, 2400, 3200,
					     2400 };
		static const short cr[6] = { 1800, 1800, 1800, 1800, 1920,
					     1800 };
		int go = ok[i];

		s = plain;
		s.txstate = V34HS_SILENCE;
		s.set_p3 = 1;
		s.p3gate = gate[i];
		s.p3count = cnt[i];
		s.set_rate = 1;
		s.baudrate = bd[i];
		s.carrier = cr[i];
		s.preemp = 0;
		both_seeded(V34HS_INFODONE, &s, tag);
		tag += 2;

		diff_eq_int(go ? "63 at SILENCE with both guards passed sets up"
			       : "63 at SILENCE with a guard failed does not",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    go ? V34HS_SSEG : V34HS_SILENCE, tag);
		if (!go) {
			diff_eq_int("63 at SILENCE, guard failed, prints nothing",
				    v34hs_observed(1)->lines, 0, tag);
			diff_eq_int("63 at SILENCE, guard failed, moves no rxstate",
				    v34hs_peek_short(1, V34HS_RXSTATE_OFF),
				    V34HS_RX_DPSK, tag);
			continue;
		}

		/*
		 * `v34setuptxmit`'s six steps, seen from outside: the rxstate
		 * is WAIT, +0x25c2 has bit 9 set, and the transcript carries
		 * the arm's own announcement and both transitions.
		 *
		 * THE LINE COUNT IS NOT ASSERTED and the transcript is read
		 * instead, deliberately: the path also runs `settxlevel`,
		 * `V34SetupModulator` and `txinit`, each of which prints
		 * diagnostics of its own whose number depends on the rate
		 * config the fixture's fill happens to leave -- twelve lines
		 * with this seed.  Three named phrases is the claim that
		 * survives a different fill; the line-for-line comparison
		 * `v34hs_compare` makes against the blob is what checks the
		 * other nine.
		 */
		diff_eq_int("63's setup moves the rxstate to WAIT",
			    v34hs_peek_short(1, V34HS_RXSTATE_OFF), V34HS_WAIT,
			    tag);
		diff_eq_int("63's setup announces itself",
			    strstr(v34hs_text(1),
				   "Setting up transmitter for phase3") != NULL,
			    1, tag);
		diff_eq_int("63's setup announces the rxstate transition",
			    strstr(v34hs_text(1),
				   "rxstate RX_DPSK=>WAIT") != NULL, 1, tag);
		diff_eq_int("63's setup announces the txstate transition",
			    strstr(v34hs_text(1),
				   "txstate SILENCE=>SSEG") != NULL, 1, tag);
		/*
		 * AND IT IS A REAL RATE, which is the difference between
		 * exercising `V34SetupModulator` and exercising its two error
		 * arms.  The rate config the arm hands it -- baud at +0xaa84,
		 * carrier at +0xaa94, pre-emphasis at +0xaa8a -- is seeded
		 * above precisely so that this holds; with the fixture's own
		 * pseudorandom halfwords it would not, and "63 sets the
		 * transmitter up" would mean "63 reached a diagnostic".
		 */
		diff_eq_int("63's setup runs a rate rather than "
			    "V34SetupModulator's error arms",
			    strstr(v34hs_text(1), "invalid") == NULL, 1, tag);
		{
			char want[64];

			/*
			 * And it is THE SEEDED rate, in the right two fields:
			 * the modulator prints baud and carrier in that order,
			 * so a reconstruction that exchanged +0xaa84 and
			 * +0xaa94 would print 1800 and 2400.
			 */
			snprintf(want, sizeof(want),
				 "baudrate %d, carrier %d, preemp 0",
				 (int)bd[i], (int)cr[i]);
			diff_eq_int("63's setup hands the modulator +0xaa84 as "
				    "the baud and +0xaa94 as the carrier",
				    strstr(v34hs_text(1), want) != NULL, 1,
				    tag);
		}
		diff_eq_int("63's setup sets bit 9 of +0x25c2",
			    (v34hs_peek_short(1, 0x25c2) >> 9) & 1, 1, tag);
		diff_eq_int("63's setup clears +0x25c0",
			    v34hs_peek_short(1, 0x25c0), 0, tag);
		saw_63_setup = 1;
	}
}

/*
 * --------------------------------------------------------------------------
 * 0x662b0 -- microstate 59 `RX_PHASE2_CALL`.
 *
 * FIVE BLOCKS IN SEQUENCE AND THEY ARE NOT EXCLUSIVE, which is what makes this
 * the one arm of the batch with its own behaviour cold: the first guard is the
 * shift register and not a companion field, and the fixture's fill satisfies
 * it.  Each family below holds the other four blocks OFF so that one is being
 * measured at a time, and what holds them off is stated in each:
 *
 *   1  the reset               `+0xaa7c` high, so block 4 cannot fire, and
 *                              `+0x358a` away from 2, so block 5 cannot
 *   2  the 0x2a forcing        the same, and a counter far below 0x125f
 *   3  the retrain gate        `+0xaa7c` high; the shift register is 0x155,
 *                              which is neither 8 nor 0x18 nor `& 7 == 7`
 *   4  the bulk delay          shift register 8 or 0x18 -- `& 7` is 0, so the
 *                              reset cannot fire -- and a counter below 0x125f
 *   5  the second reset        shift register 0x372, whose low three bits are
 *                              2, and a counter below every other threshold
 *
 * `+0x3588` IS NOT A GUARD HERE, unlike 49's and 50's copies of the reset, and
 * one case says so rather than leaving it to be assumed.
 */
static int saw_59_reset, saw_59_noreset, saw_59_silence, saw_59_already5;
static int saw_59_deep, saw_59_shallow, saw_59_clamp;
static int saw_59_gate, saw_59_retrain, saw_59_noretrain, saw_59_arm2;

static void
micro59(void)
{
	static const char reset_msg[] = "Repeated info0 is detected, "
					"errorrecovery is initialized in "
					"RX_PHASE2_CALL";
	static const char arm2_msg[] = "V34 In Retrain. errorrecovery for info0 "
				       "is initialized in RX_PHASE2_CALL";
	static const char retrain_msg[] = "V34RETRAIN, retrain is initiated in "
					  "RX_PHASE2_CALL";
	/*
	 * The first block.  The guard is the shift register's low THREE bits,
	 * where 47's copy of the same reset wants four -- so 0x157 separates
	 * this arm from that one, and 0x15f is the value both accept.
	 */
	static const struct {
		short	counter, sr, short_3588;
		int	reset;
		const char *why;
	} rr[] = {
	  { 0x0100, 0x0155, 2, 0, "0x155 has bit 1 clear" },
	  { 0x0100, 0x0157, 2, 1, "0x157 has all three, and 47 would decline" },
	  { 0x0100, 0x015f, 2, 1, "0x15f has 47's four as well" },
	  { 0x0100, 0x0153, 2, 0, "0x153 has bit 2 clear" },
	  { 0x0100, 0x0156, 2, 0, "0x156 has bit 0 clear" },
	  { 0x0100, 0x0007, 2, 1, "and 0x0007 on its own" },
	  { 0x0100, 0x0372, 2, 0, "49's 0x372 does not reset 59" },
	  { 0x0100, 0x0157, 4, 1, "+0x3588 is not a guard here, unlike 49's" },
	  { 0x0100, 0x0157, 0, 1, "nor when it is zero, which is 49's value" }
	};
	/* The second: an exact equality on the counter AFTER the increment. */
	static const struct {
		short	counter, txstate;
		int	forced, lines;
		const char *why;
	} sc[] = {
	  { 0x0029, T3MT_TXSTATE,  1, 1, "n == 0x2a forces SILENCE" },
	  { 0x0029, V34HS_SILENCE, 1, 0, "and says nothing when it is 5" },
	  { 0x0028, T3MT_TXSTATE,  0, 0, "n == 0x29 does not" },
	  { 0x002a, T3MT_TXSTATE,  0, 0, "nor does n == 0x2b" }
	};
	/* The third. */
	static const struct {
		short	counter, short_19e;
		int	assertit, retrain, entered;
		const char *why;
	} tg[] = {
	  { 0x125e, 0x33, 1, 0, 0, "n == 0x125f is not above 0x125f" },
	  { 0x125f, 0x33, 1, 1, 1, "n == 0x1260 is, and the detector asserts" },
	  { 0x125f, 0,    1, 0, 1, "+0x19e zero never consults the detector" },
	  { 0x125f, 0x33, 0, 0, 1, "and a detector that says no falls through" },
	  /* The compare is SIGNED, so a counter that steps to 0xffff is -1. */
	  { (short)0xfffe, 0x33, 1, 0, 0, "0xffff steps to -1, which is not "
					  "above 0x125f" }
	};
	/* The fourth. */
	static const struct {
		short	counter, filt, sr, gain, want;
		int	deep, rtd;
		const char *why;
	} dc[] = {
	  { 0x006b, 0x0010, 0x0008, 0x2346, 0x2346, 0, 0,
	    "n == 0x6c is not above filtdelay+0x5c" },
	  { 0x006c, 0x0010, 0x0008, 0x2346, 0x11a3, 1, 1, "n == 0x6d is" },
	  { 0x006c, 0x0040, 0x0008, 0x2346, 0x2346, 0, 0,
	    "and the same n at filtdelay 0x40 is not" },
	  { 0x0087, 0x0010, 0x0008, 0x2346, 0x11a3, 1, 84,
	    "(0x88 - 0x10 - 0x63) * 4" },
	  { 0x0073, 0x0010, 0x0018, 0x2346, 0x11a3, 1, 4,
	    "0x18 is the other shift register value the guard takes" },
	  { 0x0072, 0x0010, 0x0008, 0x2346, 0x11a3, 1, 1,
	    "a computed zero is clamped to one" },
	  { 0x0087, 0x0010, 0x0408, 0x2346, 0x2346, 0, 0,
	    "0x408 is not 8: the test is full width, not masked" },
	  { 0x0087, 0x0010, 0x0009, 0x2346, 0x2346, 0, 0, "and 9 is not 8" },
	  { 0x0087, 0x0010, 0x0372, 0x2346, 0x2346, 0, 0,
	    "nor is 49's 0x372" },
	  { 0x004c, (short)0xfff0, 0x0008, 0x2346, 0x11a3, 1, 1,
	    "filtdelay is signed in the compare" },
	  { 0x0087, 0x0010, 0x0008, (short)0xdcba, (short)0xee5d, 1, 84,
	    "and the halving is arithmetic" }
	};
	/* The fifth: ONE 32-bit compare of two halfwords, then the mask. */
	static const struct {
		short	counter, sr, short_3588, short_358a;
		int	armed;
		const char *why;
	} ar[] = {
	  { 0x0100, 0x0372, 2, 2, 1, "+0x3588 2, +0x358a 2 and sr 0x372" },
	  { 0x0100, 0x0372, 2, 3, 0, "+0x358a must be 2" },
	  { 0x0100, 0x0372, 3, 2, 0, "and +0x3588 must be 2" },
	  { 0x0100, 0x0371, 2, 2, 0, "0x371 is not 0x372" },
	  { 0x0100, 0x0772, 2, 2, 1, "but 0x772 is, masked with 0x3ff" },
	  { 0x0100, (short)0xfb72, 2, 2, 1, "and so is 0xfb72" },
	  /* Bit 9 is inside the mask and inside the constant, so a narrower
	     mask would take this one and the object does not. */
	  { 0x0100, 0x0172, 2, 2, 0, "0x172 lacks bit 9" }
	};
	long tag = 6400;
	int i;

	for (i = 0; i < (int)(sizeof(rr) / sizeof(rr[0])); i++) {
		struct seed s = plain;
		char what[160];

		s.counter = rr[i].counter;
		s.set_sr = 1;		s.sr = rr[i].sr;
		s.set_f3588 = 1;	s.short_3588 = rr[i].short_3588;
		s.set_f358a = 1;	s.short_358a = 5;	/* block 5 off */
		s.set_filt = 1;		s.filt = 0x2000; /* block 4 off */
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		both_seeded(V34HS_RX_PHASE2_CALL, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "59's reset: %s", rr[i].why);
		diff_eq_int(what, blob_said(reset_msg), rr[i].reset, tag);

		/*
		 * FOUR LINES OR NONE.  The reset prints its own phrase, then
		 * the shift register, then announces two transitions -- and
		 * the two strings come BEFORE the transitions, which is the
		 * opposite of 47's, 49's and 50's copies.
		 */
		diff_eq_int("59's reset prints four lines or none",
			    v34hs_observed(1)->lines, rr[i].reset ? 4 : 0, tag);
		{
			char line[40];

			snprintf(line, sizeof(line), "V21RXBUF=0x%x",
				 (unsigned)(unsigned short)rr[i].sr);
			diff_eq_int("59's reset prints the shift register that "
				    "satisfied its guard, before clearing it",
				    blob_said(line), rr[i].reset, tag);
		}
		diff_eq_int("59's reset moves the microstate to DET_SYNC",
			    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    rr[i].reset ? V34HS_DET_SYNC : V34HS_RX_PHASE2_CALL,
			    tag);
		diff_eq_int("59's reset moves the txstate to TX_DPSK",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    rr[i].reset ? V34HS_TX_DPSK : T3MT_TXSTATE, tag);
		/*
		 * AND IT SETS BIT 0 OF +0x3588 RATHER THAN STORING 4.  Three
		 * seeds -- 2, 4 and 0 -- so that "or with 1" and "store 4"
		 * and "store 1" are three different answers here.
		 */
		diff_eq_int("59's reset ORs bit 0 into +0x3588",
			    v34hs_peek_short(1, T3MT_F3588),
			    rr[i].reset ? (short)(rr[i].short_3588 | 1)
					: rr[i].short_3588, tag);
		/*
		 * AND IT CLEARS THE COUNTER TO ZERO, not to one.  55's copy of
		 * this same block jumps back INTO the increment and leaves
		 * with 1; this one jumps past it.
		 */
		diff_eq_int("59's reset leaves the counter at zero",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    rr[i].reset
			    ? 0 : (unsigned short)(rr[i].counter + 1), tag);

		if (rr[i].reset)
			saw_59_reset = 1;
		else
			saw_59_noreset = 1;
	}

	/*
	 * AND THE COUNTER IS RE-READ AFTER THE RESET, NOT REMEMBERED.  Two
	 * trials whose reset fires and whose seeded counter would take a later
	 * branch if the incremented value were carried forward: the object
	 * reads +0xaa78 again and gets the zero the reset just put there, so
	 * neither branch is taken.  Nothing in the family above can say this --
	 * every counter in it lands on no threshold either way.
	 */
	for (i = 0; i < 2; i++) {
		struct seed s = plain;

		s.counter = i ? 0x1300 : 0x0029;
		s.set_sr = 1;		s.sr = 0x0157;	/* the reset fires */
		s.set_f3588 = 1;	s.short_3588 = 2;
		s.set_f358a = 1;	s.short_358a = 5;
		s.set_filt = 1;		s.filt = 0x2000;
		s.set_f19e = 1;		s.short_19e = 0x33;
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		both_seeded(V34HS_RX_PHASE2_CALL, &s, tag);
		tag += 2;

		diff_eq_int("59 re-reads the counter the reset cleared",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    0, tag);
		diff_eq_int("so the reset's txstate survives the 0x2a test",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    V34HS_TX_DPSK, tag);
		diff_eq_int("and the retrain block is not entered",
			    v34hs_peek_short(1, T3MT_RX_F19E), 0x33, tag);
	}

	for (i = 0; i < (int)(sizeof(sc) / sizeof(sc[0])); i++) {
		struct seed s = plain;
		char what[160];

		s.txstate = sc[i].txstate;
		s.counter = sc[i].counter;
		s.set_sr = 1;		s.sr = 0x0155;
		s.set_f3588 = 1;	s.short_3588 = 4;
		s.set_f358a = 1;	s.short_358a = 5;
		s.set_filt = 1;		s.filt = 0x2000;
		both_seeded(V34HS_RX_PHASE2_CALL, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "59 at 0x2a: %s", sc[i].why);
		diff_eq_int(what, v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    sc[i].forced ? V34HS_SILENCE : sc[i].txstate, tag);
		diff_eq_int("59's forcing is hs_setstate's compare-print-store",
			    v34hs_observed(1)->lines, sc[i].lines, tag);
		if (sc[i].lines)
			/* 0x2a is 42, and it is the counter the line prints. */
			diff_eq_int("59 announces the forcing with the counter "
				    "that reached 0x2a", blob_said("[2]42)"), 1,
				    tag);

		if (sc[i].forced && sc[i].lines)
			saw_59_silence = 1;
		else if (sc[i].forced)
			saw_59_already5 = 1;
	}

	for (i = 0; i < (int)(sizeof(tg) / sizeof(tg[0])); i++) {
		struct seed s = plain;
		char what[160];

		s.counter = tg[i].counter;
		s.set_sr = 1;		s.sr = 0x0155;
		s.set_f3588 = 1;	s.short_3588 = 4;
		s.set_f358a = 1;	s.short_358a = 5;
		s.set_filt = 1;		s.filt = 0x2000;
		s.set_f19e = 1;		s.short_19e = tg[i].short_19e;
		s.set_det = 1;		s.det_assert = tg[i].assertit;
		/*
		 * NO 0x40 IN THE FLAGS AND NOTHING IN +0xac17, which is what
		 * makes 59's `flags |= 0x40` readable: it is the bit
		 * `v34handshakinit`'s mode-1 body dispatches on, and with
		 * either input already set both answers are the same.
		 * 0x115 and not 0x155: 0x40 IS bit 6 of 0x155.
		 */
		s.set_flags = 1;	s.flags = 0x0115;
		s.set_retrainq = 1;	s.retrainq = 0;
		both_seeded(V34HS_RX_PHASE2_CALL, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "59's retrain gate: %s",
			 tg[i].why);
		diff_eq_int(what, blob_said(retrain_msg), tg[i].retrain, tag);

		if (tg[i].entered && tg[i].short_19e != 0)
			/*
			 * DID THE FILTER LOOP RUN?  `tone_detect`'s window is
			 * the receiver's +0x10c to its sample pointer, and
			 * nothing but that loop writes the detector's level.
			 * If it never runs, the two ends of the window are
			 * exchangeable and this suite cannot say which is
			 * which.
			 */
			diff_eq_int("59's tone_detect window is not empty",
				    v34hs_peek_short(1, T3MT_DET + 0x12)
				    != 0x1234, 1, tag);

		if (tg[i].retrain) {
			/*
			 * `v34handshakinit(obj, 1)` and not mode 0: the
			 * retrain entry is the only mode that leaves 2 in
			 * +0x3588, and mode 0 leaves 0.
			 */
			diff_eq_int("59's retrain runs v34handshakinit's mode 1",
				    v34hs_peek_short(1, T3MT_F3588), 2, tag);
			/*
			 * AND 0x40 AND NOT SOME OTHER BIT: mode 1 bumps
			 * +0xac14 when the flag is set and +0xac12 when it is
			 * not, so which counter moved says which bit 59 wrote.
			 */
			diff_eq_int("59's retrain sets the bit mode 1 reads",
				    v34hs_peek_short(1, T3MT_RETRAIN_A),
				    0x0112, tag);
			diff_eq_int("and mode 1's other counter stands still",
				    v34hs_peek_short(1, T3MT_RETRAIN_B),
				    0x0222, tag);
			saw_59_retrain = 1;
			continue;
		}

		/*
		 * +0x19e IS SET TO 1 WHETHER THE DETECTOR WAS CONSULTED OR
		 * NOT, and only inside the block -- so the counter threshold
		 * and the gate are two separate readings.
		 */
		diff_eq_int("59 sets the receiver's +0x19e once it is past "
			    "0x125f", v34hs_peek_short(1, T3MT_RX_F19E),
			    tg[i].entered ? 1 : tg[i].short_19e, tag);
		if (tg[i].entered && tg[i].short_19e == 0)
			saw_59_gate = 1;
		else if (tg[i].entered)
			saw_59_noretrain = 1;
	}

	for (i = 0; i < (int)(sizeof(dc) / sizeof(dc[0])); i++) {
		struct seed s = plain;
		char what[160];

		s.counter = dc[i].counter;
		s.set_filt = 1;		s.filt = dc[i].filt;
		s.set_sr = 1;		s.sr = dc[i].sr;
		s.set_f3588 = 1;	s.short_3588 = 4;
		s.set_f358a = 1;	s.short_358a = 5;
		/* 0x200 and 0x800 must go and 0x155 must stay. */
		s.set_flags = 1;	s.flags = 0x0b55;
		s.set_gain = 1;		s.gain = dc[i].gain;
		both_seeded(V34HS_RX_PHASE2_CALL, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "59 past filtdelay+0x5c: %s",
			 dc[i].why);
		diff_eq_int(what, blob_said("V34SetupDemodulator: baudrate "
					    "2400, carrier 1800"),
			    dc[i].deep, tag);

		if (!dc[i].deep) {
			diff_eq_int("59 below the threshold prints nothing",
				    v34hs_observed(1)->lines, 0, tag);
			diff_eq_int("59 below the threshold leaves the gain",
				    v34hs_peek_short(1, T3MT_RX_GAIN),
				    dc[i].want, tag);
			diff_eq_int("59 below the threshold leaves the rxstate",
				    v34hs_peek_short(1, V34HS_RXSTATE_OFF),
				    V34HS_RX_DPSK, tag);
			saw_59_shallow = 1;
			continue;
		}

		diff_eq_int("59 computes the bulk delay",
			    v34hs_peek_short(1, T3MT_RTD), dc[i].rtd, tag);
		diff_eq_int("59 replaces the counter with 0x14",
			    v34hs_peek_short(1, T3MT_COUNTER), 0x14, tag);
		diff_eq_int("59 moves the rxstate to RX_L1",
			    v34hs_peek_short(1, V34HS_RXSTATE_OFF),
			    V34HS_RX_L1, tag);
		diff_eq_int("59 leaves the microstate alone",
			    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    V34HS_RX_PHASE2_CALL, tag);
		diff_eq_int("59 clears 0x200 and 0x800 and nothing else",
			    (unsigned short)v34hs_peek_short(1, T3MT_RX_FLAGS),
			    0x0155, tag);
		diff_eq_int("59 halves the gain, arithmetically",
			    v34hs_peek_short(1, T3MT_RX_GAIN), dc[i].want, tag);
		diff_eq_int("59 sets the 2400-baud resampler word",
			    (unsigned short)v34hs_peek_short(1, T3MT_RX_F1AE),
			    0x3e80, tag);
		diff_eq_int("59 sets the 1800-Hz carrier length",
			    v34hs_peek_short(1, T3MT_RX_F1BA), 0x10, tag);
		/*
		 * AND THE CARRIER TABLE IT SELECTED, by what it points at:
		 * side A holds ours and side B the blob's, so the addresses
		 * differ for ever and only the contents are comparable
		 * (finding F324).  `half_len` shorts twice over is the whole table.
		 */
		same_table("59 selects the 1800-Hz carrier table",
			   T3MT_RX_CARRIER, 2 * 0x10, tag);
		diff_eq_int("59 then runs rxtiminginit",
			    v34hs_peek_short(1, T3MT_RX_F1D2), 2400, tag);
		diff_eq_int("59 closes with the bulk-delay line",
			    blob_said("at end of bulkdelay estimation"), 1, tag);

		saw_59_deep = 1;
		if (dc[i].rtd == 1)
			saw_59_clamp = 1;
	}

	for (i = 0; i < (int)(sizeof(ar) / sizeof(ar[0])); i++) {
		struct seed s = plain;
		char what[160];

		s.counter = ar[i].counter;
		s.set_sr = 1;		s.sr = ar[i].sr;
		s.set_f3588 = 1;	s.short_3588 = ar[i].short_3588;
		s.set_f358a = 1;	s.short_358a = ar[i].short_358a;
		s.set_filt = 1;		s.filt = 0x2000;
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		both_seeded(V34HS_RX_PHASE2_CALL, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "59's second reset: %s",
			 ar[i].why);
		diff_eq_int(what, blob_said(arm2_msg), ar[i].armed, tag);

		diff_eq_int("59's second reset moves the microstate to DET_SYNC",
			    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    ar[i].armed ? V34HS_DET_SYNC : V34HS_RX_PHASE2_CALL,
			    tag);
		diff_eq_int("59's second reset moves the txstate to TX_DPSK",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    ar[i].armed ? V34HS_TX_DPSK : T3MT_TXSTATE, tag);
		/*
		 * +0x3588 IS READ BACK AFTER `v34handshakinit(obj, 0)` AND NOT
		 * BEFORE.  Mode 0 runs `v34modeminit`, which zeroes the word,
		 * so the correct answer is 1 -- and a reading taken before the
		 * call would leave the seeded 2 ORed with 1, which is 3.
		 */
		diff_eq_int("59's second reset ORs bit 0 into what "
			    "v34handshakinit left, not into what it found",
			    v34hs_peek_short(1, T3MT_F3588),
			    ar[i].armed ? 1 : ar[i].short_3588, tag);
		diff_eq_int("59's second reset leaves 1 in +0x358a",
			    v34hs_peek_short(1, T3MT_F358A),
			    ar[i].armed ? 1 : ar[i].short_358a, tag);
		/*
		 * THE COUNTER COMES BACK ZERO AND THAT IS NOT THE ARM'S DOING.
		 * `v34handshakinit` clears +0xaa78 itself, so unlike the first
		 * reset -- whose own clear IS visible, because nothing else on
		 * that path touches the counter -- this reading cannot tell an
		 * arm that clears it from one that does not.  It is recorded as
		 * a measurement of the pair and not as a claim about the arm;
		 * the matching mutation is marked equivalent for this reason.
		 */
		diff_eq_int("59's second reset comes back with a counter "
			    "v34handshakinit zeroed",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    ar[i].armed
			    ? 0 : (unsigned short)(ar[i].counter + 1), tag);
		if (ar[i].armed) {
			const char *o = (const char *)v34hs_object(1);

			diff_eq_int("59's second reset aims +0xaa6c at the "
				    "first message record",
				    *(const char *const *)(o + 0xaa6c)
				    == o + T3MT_MSGREC0, 1, tag);
			saw_59_arm2 = 1;
		}
	}
}

/*
 * --------------------------------------------------------------------------
 * 0x62a40 -- the tail, driven through microstate 48's ordinary path.
 *
 * Every arm of table 3 and every arm of table 2 falls into these eighty-eight
 * instructions, and they are the only place the function returns from -- so
 * a batch that reconstructs nine arms and leaves the tail's branches
 * unexercised has nine arms resting on an untested epilogue.  Six of its
 * branches are reachable from here and each gets a trial on both sides of its
 * condition:
 *
 *      +0x2218 == 1            the receiver's +0x1d2 gets three times the
 *                              baud rate, progress 4, and the txstate is
 *                              RE-READ from the object
 *      +0x2218 in {4, 5}       progress 6
 *      +0x2218 in {2, 3}       and txstate 0x4a: progress from `vect_idx`
 *      +0x238 > +0x23c         progress 8, UNSIGNED
 *      agc_level vs +0x230     one arm zeroes +0x234, the other increments it
 *      +0x234 > 0x257f         progress 9
 *      txstate 0x52/0x53/0x54  progress 0x0d / 0x0f / 0x10
 *
 * 0x52, 0x53 and 0x54 are past table 2's last entry, so the transmit dispatch
 * hands straight to the tail -- which is how a table-3 arm can reach those
 * three at all.
 */
/*
 * --------------------------------------------------------------------------
 * 0x66a0d -- microstate 49 `RX_PHASE1_ANS`, and 0x664b8 -- 50 `RX_PHASE2_ANS`.
 *
 * THREE OF THE FOUR THRESHOLDS ARE NOT CONSTANTS.  Both arms bump the counter
 * and then measure it against `+0xaa7c` plus 0x4c and plus 0x50, and only the
 * FIRST threshold -- 0x28 in 49 and 0x32 in 50 -- is a literal.  A suite that
 * leaves +0xaa7c as the fixture filled it therefore tests one arbitrary number
 * per arm and cannot fail if the two arms' constants are exchanged, which is
 * finding F288's group-D vacuity in the form these two take.  So every trial
 * below sets it, and the two families are driven at two different values so
 * that "threshold" and "constant" are separable.
 *
 * THE TWO INTERESTING PATHS CANNOT BOTH BE REACHED IN ONE STEP, and that is
 * structural rather than a property of any seed: 0x4c is BELOW 0x50, so the
 * window the error-recovery reset needs (`n < filt + 0x4c`) lies entirely
 * below the threshold everything else needs (`n > filt + 0x50`).  Two
 * families of trials, at two values of +0xaa7c, is the consequence.
 *
 * `filtDelay` is the object's own name for +0xaa7c: it is the third thing
 * 49's diagnostic at 0x709d7 prints.
 */
static int saw_49_reset, saw_49_noreset, saw_49_leave, saw_49_bulk;
static int saw_49_short, saw_49_long, saw_49_clamp, saw_49_nofsk;
static int saw_50_reset, saw_50_noreset, saw_50_leave, saw_50_setup;
static int saw_50_nofsk;

/* The two arms' reset families differ only in the guard and the string. */
struct resetcase {
	short	counter;	/* what the counter is seeded with     */
	short	filt;		/* and +0xaa7c                         */
	short	sr;		/* and the FSK shift register          */
	short	short_3588;
	int	reset;		/* is the reset expected to fire?      */
	const char *why;
};

static void
reset_family(short mst, const struct resetcase *c, int n, const char *msg,
	     long tag, int *saw_yes, int *saw_no)
{
	int i;

	for (i = 0; i < n; i++) {
		struct seed s = plain;
		char what[128];

		s.counter = c[i].counter;
		s.set_filt = 1;		s.filt = c[i].filt;
		s.set_sr = 1;		s.sr = c[i].sr;
		s.set_f3588 = 1;	s.short_3588 = c[i].short_3588;
		/*
		 * The thirteen words the reset clears, filled with varied
		 * non-zero values: `v34handshakinit` already leaves several of
		 * them zero, and a clear of a word that is already zero is a
		 * claim no comparison can fail (finding F230).
		 */
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		s.set_fsk = 0;		/* the demodulator must not move sr */
		both_seeded(mst, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "%s: %s", msg, c[i].why);
		diff_eq_int(what, blob_said(msg), c[i].reset, tag);

		/*
		 * The reset is not one line: it announces two transitions
		 * before it prints, and both are to states the trial is not
		 * already in, so three lines or none.
		 */
		diff_eq_int("the reset prints three lines or none",
			    v34hs_observed(1)->lines, c[i].reset ? 3 : 0, tag);
		diff_eq_int("the reset moves the microstate to DET_SYNC",
			    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    c[i].reset ? V34HS_DET_SYNC : mst, tag);
		diff_eq_int("the reset moves the txstate to TX_DPSK",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    c[i].reset ? V34HS_TX_DPSK : T3MT_TXSTATE, tag);
		diff_eq_int("the reset arms +0x3588",
			    v34hs_peek_short(1, T3MT_F3588),
			    c[i].reset ? 4 : c[i].short_3588, tag);
		/*
		 * AND THE COUNTER SURVIVES IT.  47's copy of this same block
		 * clears the counter as its last act and these two do not, so
		 * a shared helper that cleared it would make 47's clear
		 * unfalsifiable.  This is the reading that says so.
		 */
		diff_eq_int("the reset does NOT clear the counter",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    (unsigned short)(c[i].counter + 1), tag);

		if (c[i].reset)
			*saw_yes = 1;
		else
			*saw_no = 1;
	}
}

static void
micro49(void)
{
	static const char msg[] = "Repeated info0 is detected, errorrecovery "
				  "is initialized in RX_PHASE1_ANS";
	/*
	 * The reset family, at +0xaa7c = 0x40 unless the case is there to say
	 * that the window MOVES with +0xaa7c.  With filtdelay 0x40 the window
	 * is 0x28 < n < 0x8c and the second threshold is 0x90, so every one of
	 * these leaves by the first exit whatever the reset did.
	 */
	static const struct resetcase rc[] = {
	  { 0x0027, 0x40, 0x0372, 0, 0, "n == 0x28 is not above 0x28" },
	  { 0x0028, 0x40, 0x0372, 0, 1, "n == 0x29 is" },
	  { 0x008a, 0x40, 0x0372, 0, 1, "n == 0x8b is below filtdelay+0x4c" },
	  { 0x008b, 0x40, 0x0372, 0, 0, "n == 0x8c is not" },
	  { 0x0028, 0x00, 0x0372, 0, 1, "the window moves with filtdelay" },
	  { 0x004b, 0x00, 0x0372, 0, 0, "n == 0x4c is filtdelay+0x4c at 0" },
	  { 0x004b, 0x40, 0x0372, 0, 1, "and the same n resets at 0x40" },
	  { 0x0028, 0x40, 0x0371, 0, 0, "0x371 is not 0x372" },
	  { 0x0028, 0x40, 0x0373, 0, 0, "nor is 0x373" },
	  { 0x0028, 0x40, 0x0772, 0, 1, "but 0x772 is, masked with 0x3ff" },
	  { 0x0028, 0x40, (short)0xfb72, 0, 1, "and so is 0xfb72" },
	  { 0x0028, 0x40, 0x0372, 1, 0, "+0x3588 must be zero" },
	  { 0x0028, 0x40, 0x0372, -1, 0, "and -1 is not zero either" },
	  /* The increment is unsigned and the first compare is signed. */
	  { 0x7fff, 0x40, 0x0372, 0, 0, "0x7fff steps to -32768" },
	  { (short)0xffff, 0x40, 0x0372, 0, 0, "and 0xffff steps to 0" }
	};
	/*
	 * The deep family, at +0xaa7c = 0x10 so the second threshold is 0x60.
	 * Every one of these is above the reset window, so nothing here can be
	 * confused with the family above.
	 */
	static const struct {
		short	counter, filt, sr, isshort, prevbd;
		int	deep;		/* past the second threshold?      */
		int	rtd;		/* and what it left in +0xaa7e     */
		const char *why;
	} dc[] = {
	  { 0x005f, 0x10, 0x0155, 0, 0,	   0, 0,    "n == 0x60 is not above "
						    "filtdelay+0x50" },
	  { 0x0060, 0x10, 0x0155, 0, 0,	   1, 1,    "n == 0x61 is" },
	  { 0x0060, 0x40, 0x0155, 0, 0,	   0, 0,    "and the same n at "
						    "filtdelay 0x40 is not" },
	  { 0x0087, 0x10, 0x0155, 0, 0,	   1, 84,   "(0x88-0x10)*4-0x18c" },
	  { 0x0072, 0x10, 0x0155, 0, 0,	   1, 1,    "a computed zero is "
						    "clamped to one" },
	  { 0x0073, 0x10, 0x0155, 0, 0,	   1, 4,    "and a computed four is "
						    "not" },
	  { 0x0087, 0x10, 0x0155, 1, 0x37,  1, 0x37, "is_short takes "
						    "prev_bulk_delay" },
	  { 0x0087, 0x10, 0x0155, 1, 0,	   1, 1,    "which is clamped at "
						    "zero" },
	  { 0x0087, 0x10, 0x0155, 1, -5,   1, 1,    "and below it" },
	  /* Signed: filtdelay + 0x50 is -0x10 + 0x50 = 0x40, not 65600. */
	  { 0x0050, (short)0xfff0, 0x0155, 0, 0, 1, 1,
						    "filtdelay is signed" }
	};
	long tag = 5200;
	int i;

	reset_family(V34HS_RX_PHASE1_ANS, rc,
		     (int)(sizeof(rc) / sizeof(rc[0])), msg, tag,
		     &saw_49_reset, &saw_49_noreset);
	tag += 400;

	for (i = 0; i < (int)(sizeof(dc) / sizeof(dc[0])); i++) {
		struct seed s = plain;
		char what[160];

		s.counter = dc[i].counter;
		s.set_filt = 1;		s.filt = dc[i].filt;
		s.set_sr = 1;		s.sr = dc[i].sr;
		s.set_f3588 = 1;	s.short_3588 = 0;
		s.set_isshort = 1;	s.isshort = dc[i].isshort;
		s.set_prevbd = 1;	s.prevbd = dc[i].prevbd;
		s.set_flags = 1;	s.flags = 0x0155;
		s.set_gain = 1;		s.gain = 0x2345;
		both_seeded(V34HS_RX_PHASE1_ANS, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "49 past filtdelay+0x50: %s",
			 dc[i].why);
		diff_eq_int(what, blob_said("On RX_PHASE1_ANS: is short="),
			    dc[i].deep, tag);

		/*
		 * +0x200 IS SET BEFORE THE LAST GUARD, so a step that reaches
		 * the bulk-delay block and one that turns back at the shift
		 * register both leave it set -- and a step that never got past
		 * the second threshold does not.
		 */
		diff_eq_int("49 sets +0x200 in the receiver's flags only past "
			    "the second threshold",
			    (v34hs_peek_short(1, T3MT_RX_FLAGS) & 0x200) != 0,
			    dc[i].deep, tag);

		if (!dc[i].deep) {
			diff_eq_int("49 below the threshold prints nothing",
				    v34hs_observed(1)->lines, 0, tag);
			diff_eq_int("49 below the threshold leaves the "
				    "microstate", v34hs_peek_short(1,
				    V34HS_MICROSTATE_OFF),
				    V34HS_RX_PHASE1_ANS, tag);
			saw_49_leave = 1;
			continue;
		}

		diff_eq_int("49 computes the bulk delay",
			    v34hs_peek_short(1, T3MT_RTD), dc[i].rtd, tag);
		diff_eq_int("49 moves the microstate to TX_PHASE2_ANS",
			    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    V34HS_TX_PHASE2_ANS, tag);
		/*
		 * AND THE COUNTER TAKES filtdelay BEFORE THAT TRANSITION IS
		 * ANNOUNCED.  The counter is `[2]` in the line, so the order of
		 * the two writes is visible in the transcript and nowhere in
		 * the object -- both orders leave the same bytes behind.
		 */
		diff_eq_int("49 replaces the counter with filtdelay",
			    v34hs_peek_short(1, T3MT_COUNTER), dc[i].filt, tag);
		{
			char line[80];

			snprintf(line, sizeof(line), "[2]%d)", (int)dc[i].filt);
			diff_eq_int("49 announces the transition AFTER the "
				    "counter took filtdelay", blob_said(line),
				    1, tag);
		}
		diff_eq_int("49 copies agc_gain to the receiver's +0x264",
			    v34hs_peek_short(1, T3MT_RX_F264), 0x2345, tag);
		diff_eq_int("49 prints prev_bulk_delay's line only when "
			    "is_short is set",
			    blob_said("Setting Bulk delay according to prev "
				      "session"), dc[i].isshort != 0, tag);

		saw_49_bulk = 1;
		if (dc[i].isshort)
			saw_49_short = 1;
		else
			saw_49_long = 1;
		if (dc[i].rtd == 1)
			saw_49_clamp = 1;
	}

	/*
	 * BIT 0 OF THE SHIFT REGISTER, past the second threshold.  It is the
	 * only guard between +0x200 going in and the bulk delay being applied,
	 * so this pair is what says the flag write is not part of the block
	 * below it.
	 */
	for (i = 0; i < 2; i++) {
		struct seed s = plain;

		s.counter = 0x0087;
		s.set_filt = 1;		s.filt = 0x10;
		s.set_sr = 1;		s.sr = i ? 0x0155 : 0x0154;
		s.set_f3588 = 1;	s.short_3588 = 0;
		s.set_isshort = 1;	s.isshort = 0;
		s.set_flags = 1;	s.flags = 0x0155;
		both_seeded(V34HS_RX_PHASE1_ANS, &s, tag);
		tag += 2;
		diff_eq_int("49 needs bit 0 of the shift register",
			    blob_said("On RX_PHASE1_ANS: is short="), i, tag);
		diff_eq_int("49 sets +0x200 either way",
			    (v34hs_peek_short(1, T3MT_RX_FLAGS) & 0x200) != 0,
			    1, tag);
		if (!i) {
			diff_eq_int("49 with bit 0 clear prints nothing",
				    v34hs_observed(1)->lines, 0, tag);
			saw_49_nofsk = 1;
		}
	}
}

static void
micro50(void)
{
	static const char msg[] = "Repeated info0 is detected, errorrecovery "
				  "is initialized in RX_PHASE2_ANS";
	/*
	 * 50's first threshold is 0x32 and its shift-register test is
	 * `(sr & 0x3ff) > 0x200` where 49's is `== 0x372`, so the two arms
	 * disagree on 0x201 and on 0x373 -- both are driven.
	 */
	static const struct resetcase rc[] = {
	  { 0x0031, 0x40, 0x0201, 0, 0, "n == 0x32 is not above 0x32" },
	  { 0x0032, 0x40, 0x0201, 0, 1, "n == 0x33 is" },
	  { 0x0028, 0x40, 0x0201, 0, 0, "and 49's 0x28 is not 50's" },
	  { 0x008a, 0x40, 0x0201, 0, 1, "n == 0x8b is below filtdelay+0x4c" },
	  { 0x008b, 0x40, 0x0201, 0, 0, "n == 0x8c is not" },
	  { 0x0032, 0x00, 0x0201, 0, 1, "the window moves with filtdelay" },
	  { 0x004b, 0x00, 0x0201, 0, 0, "n == 0x4c is filtdelay+0x4c at 0" },
	  { 0x004b, 0x40, 0x0201, 0, 1, "and the same n resets at 0x40" },
	  { 0x0032, 0x40, 0x0200, 0, 0, "0x200 is not above 0x200" },
	  { 0x0032, 0x40, 0x0600, 0, 0, "nor is 0x600, masked with 0x3ff" },
	  { 0x0032, 0x40, 0x0601, 0, 1, "but 0x601 is" },
	  { 0x0032, 0x40, 0x0373, 0, 1, "and so is 49's 0x373" },
	  { 0x0032, 0x40, 0x0201, 1, 0, "+0x3588 must be zero" },
	  { 0x7fff, 0x40, 0x0201, 0, 0, "0x7fff steps to -32768" },
	  { (short)0xffff, 0x40, 0x0201, 0, 0, "and 0xffff steps to 0" }
	};
	/*
	 * `gain` is here so that ONE trial can carry a negative one: 50 halves
	 * the gain with no band test in front of it, so the arithmetic shift
	 * is a claim only a negative input can fail.
	 */
	static const struct {
		short	counter, filt, sr, gain, want;
		int	deep;
		const char *why;
	} dc[] = {
	  { 0x005f, 0x10, 0x0155, 0x2346, 0x2346, 0,
	    "n == 0x60 is not above filtdelay+0x50" },
	  { 0x0060, 0x10, 0x0155, 0x2346, 0x11a3, 1, "n == 0x61 is" },
	  { 0x0060, 0x40, 0x0155, 0x2346, 0x2346, 0,
	    "and the same n at 0x40 is not" },
	  { 0x0060, 0x10, 0x0154, 0x2346, 0x2346, 0,
	    "bit 0 of the shift register is needed" },
	  { 0x0050, (short)0xfff0, 0x0155, 0x2346, 0x11a3, 1,
	    "filtdelay is signed" },
	  { 0x0060, 0x10, 0x0155, (short)0xdcba, (short)0xee5d, 1,
	    "and the halving is arithmetic" }
	};
	long tag = 5600;
	int i;

	reset_family(V34HS_RX_PHASE2_ANS, rc,
		     (int)(sizeof(rc) / sizeof(rc[0])), msg, tag,
		     &saw_50_reset, &saw_50_noreset);
	tag += 400;

	for (i = 0; i < (int)(sizeof(dc) / sizeof(dc[0])); i++) {
		struct seed s = plain;
		char what[160];

		s.counter = dc[i].counter;
		s.set_filt = 1;		s.filt = dc[i].filt;
		s.set_sr = 1;		s.sr = dc[i].sr;
		s.set_f3588 = 1;	s.short_3588 = 0;
		/*
		 * 0x0b55 has BOTH the bits 50 clears and one it must not:
		 * 0x200 and 0x800 go and 0x155 stays, so a mask of ~0x200 or
		 * of ~0xfff would both be caught.
		 */
		s.set_flags = 1;	s.flags = 0x0b55;
		s.set_gain = 1;		s.gain = dc[i].gain;
		both_seeded(V34HS_RX_PHASE2_ANS, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "50 past filtdelay+0x50: %s",
			 dc[i].why);
		diff_eq_int(what, blob_said("V34SetupDemodulator: baudrate "
					    "2400, carrier 1800"),
			    dc[i].deep, tag);

		if (!dc[i].deep) {
			diff_eq_int("50 below the threshold prints nothing",
				    v34hs_observed(1)->lines, 0, tag);
			diff_eq_int("50 below the threshold leaves the "
				    "receiver's flags alone",
				    (unsigned short)v34hs_peek_short(1,
					    T3MT_RX_FLAGS), 0x0b55, tag);
			diff_eq_int("50 below the threshold leaves the gain "
				    "alone", v34hs_peek_short(1, T3MT_RX_GAIN),
				    dc[i].want, tag);
			if (dc[i].sr == 0x0154)
				saw_50_nofsk = 1;
			else
				saw_50_leave = 1;
			continue;
		}

		diff_eq_int("50 replaces the counter with 0x14",
			    v34hs_peek_short(1, T3MT_COUNTER), 0x14, tag);
		diff_eq_int("50 moves the rxstate to RX_L1",
			    v34hs_peek_short(1, V34HS_RXSTATE_OFF),
			    V34HS_RX_L1, tag);
		diff_eq_int("50 leaves the microstate alone",
			    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    V34HS_RX_PHASE2_ANS, tag);
		diff_eq_int("50 clears 0x200 and 0x800 and nothing else",
			    (unsigned short)v34hs_peek_short(1, T3MT_RX_FLAGS),
			    0x0155, tag);
		diff_eq_int("50 halves the gain, arithmetically",
			    v34hs_peek_short(1, T3MT_RX_GAIN), dc[i].want, tag);
		/*
		 * THE DEMODULATOR IS SET UP FOR 2400/1800 AND THE TWO NUMBERS
		 * ARE LITERALS AT THE CALL SITE, not a read of any rate field
		 * -- so a mutation of either is caught by the diagnostic above
		 * and by these three fields, which are the ones that separate
		 * 2400 from the other five rates and 1800 from the other seven
		 * carriers.
		 */
		diff_eq_int("50 sets the 2400-baud resampler word",
			    (unsigned short)v34hs_peek_short(1, T3MT_RX_F1AE),
			    0x3e80, tag);
		diff_eq_int("50 sets the 1800-Hz carrier length",
			    v34hs_peek_short(1, T3MT_RX_F1BA), 0x10, tag);
		/* And rxtiminginit's own signature field. */
		diff_eq_int("50 then runs rxtiminginit",
			    v34hs_peek_short(1, T3MT_RX_F1D2), 2400, tag);
		saw_50_setup = 1;
	}
}

/*
 * --------------------------------------------------------------------------
 * 0x65c47 -- microstate 51 `TX_L1`.
 *
 * ONE THRESHOLD AND IT IS AN EQUALITY, so 0x29 and 0x2b both leave and only
 * 0x2a runs the body.  `is_short` then chooses between two whole bodies which
 * write the SAME TWO STATE WORDS and announce them in OPPOSITE ORDERS -- the
 * only reading that separates the orders is the transcript, since either order
 * leaves the object byte for byte the same.
 */
static int saw_51_leave, saw_51_detect, saw_51_info, saw_51_2743, saw_51_dflt;
static int saw_51_gain[3];

static void
micro51(void)
{
	long tag = 6000;
	int i;

	/* The equality.  Three counters, one of which is it. */
	for (i = 0; i < 3; i++) {
		static const short cnt[3] = { 0x0028, 0x0029, 0x002a };
		struct seed s = plain;

		s.counter = cnt[i];
		s.set_isshort = 1;	s.isshort = 0;
		s.set_answer = 1;	s.answer = 0x66;
		s.set_gain = 1;		s.gain = 0x0800;
		s.set_pcmrx = 1;	s.v90rx = 0;	s.k56rx = 0;
		both_seeded(V34HS_TX_L1, &s, tag);
		tag += 2;
		diff_eq_int("51 runs its body only when the counter reaches "
			    "0x2a exactly",
			    v34hs_peek_short(1, V34HS_RXSTATE_OFF)
			    == V34HS_DET_AB, cnt[i] == 0x0029, tag);
		if (cnt[i] != 0x0029) {
			diff_eq_int("51 off the threshold prints nothing",
				    v34hs_observed(1)->lines, 0, tag);
			diff_eq_int("51 off the threshold leaves the txstate",
				    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
				    T3MT_TXSTATE, tag);
			saw_51_leave = 1;
		}
	}

	/*
	 * `is_short == 0`: the detector body.  The coefficient table is the
	 * only one of `detectorinit`'s seven arguments that depends on
	 * anything, so both roles are driven; the table is a library pointer
	 * and the comparison sees it by which block +0x3564 selects.
	 */
	for (i = 0; i < 2; i++) {
		struct seed s = plain;

		s.counter = 0x0029;
		s.set_isshort = 1;	s.isshort = 0;
		s.set_answer = 1;	s.answer = i ? 0x65 : 0x66;
		s.set_gain = 1;		s.gain = 0x0800;
		s.set_flags = 1;	s.flags = 0x0155;
		s.set_vectidx = 1;	s.vectidx = 0x1234;
		both_seeded(V34HS_TX_L1, &s, tag);
		tag += 2;
		diff_eq_int("51 moves the txstate to TX_L1",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    V34HS_TX_L1, tag);
		diff_eq_int("51 leaves the microstate at TX_L1, which is where "
			    "it already was", v34hs_peek_short(1,
			    V34HS_MICROSTATE_OFF), V34HS_TX_L1, tag);
		diff_eq_int("51 moves the rxstate to DET_AB",
			    v34hs_peek_short(1, V34HS_RXSTATE_OFF),
			    V34HS_DET_AB, tag);
		diff_eq_int("51 clears vect_idx", v34hs_peek_short(1,
			    T3MT_VECTIDX), 0, tag);
		diff_eq_int("51 sets +0x200 in the receiver's flags",
			    (unsigned short)v34hs_peek_short(1, T3MT_RX_FLAGS),
			    0x0355, tag);
		diff_eq_int("51 clears +0x35a0",
			    v34hs_peek_short(1, T3MT_F35A0), 0, tag);
		diff_eq_int("51 keeps the counter it arrived at",
			    v34hs_peek_short(1, T3MT_COUNTER), 0x2a, tag);
		/*
		 * TWO TRANSITIONS IN THIS ORDER, and the txstate line is
		 * printed while the microstate is still TX_L1 -- which is
		 * exactly what the other body's order would not produce.
		 */
		diff_eq_int("51 announces the txstate move first",
			    blob_said("txstate SSEG=>TX_L1(rx RX_DPSK, "
				      "mst TX_L1"), 1, tag);
		diff_eq_int("51 then the rxstate move",
			    blob_said("rxstate RX_DPSK=>DET_AB(tx TX_L1, "
				      "mst TX_L1"), 1, tag);
		diff_eq_int("51 ends by naming DET_AB",
			    blob_said("V34RETRAIN, starting DET_AB"), 1, tag);
		diff_eq_int("51 prints three lines here",
			    v34hs_observed(1)->lines, 3, tag);
		/*
		 * AND THE COEFFICIENT TABLE, by its contents.  `c1200_` and
		 * `c2400_` differ, and the role flag is the only thing that
		 * chooses between them, so this is what makes the two runs of
		 * this loop two different claims rather than one repeated.
		 */
		same_table("51 hands detectorinit the table its role selects",
			   0x3564, 8, tag);
		saw_51_detect = 1;
	}

	/*
	 * `is_short != 0`: the info1a body, and the rate switch inside it.
	 *
	 * EVERY ONE OF THE SIX RATES NEEDS ITS OWN TRIAL.  The switch writes
	 * one pointer and nothing else per arm, its default writes nothing,
	 * and the fixture's pseudorandom halfword is not a V.34 baud -- so a
	 * suite that does not set +0xaa84 tests the default and reports it as
	 * "51 sets the transmitter up".  That is the previous agent's trap,
	 * one level down.
	 */
	for (i = 0; i < 9; i++) {
		static const short baud[9] = { 0x0960, 0x0ab7, 0x0ab7, 0x0af0,
					       0x0bb8, 0x0c80, 0x0d65, 0x0123,
					       0x0960 };
		static const short carr[9] = { 0x0700, 0x0725, 0x0701, 0x0702,
					       0x0703, 0x0704, 0x0705, 0x0706,
					       0x0725 };
		/* What the arm leaves in +0xaa84 and +0xaa94. */
		static const short wbaud[9] = { 0x0960, 0x0af0, 0x0af0, 0x0af0,
						0x0bb8, 0x0c80, 0x0d65, 0x0123,
						0x0960 };
		static const short wcarr[9] = { 0x0700, 0x074b, 0x0690, 0x0702,
						0x0703, 0x0704, 0x0705, 0x0706,
						0x0725 };
		struct seed s = plain;

		s.counter = 0x0029;
		s.set_isshort = 1;	s.isshort = 0x0101;
		s.set_answer = 1;	s.answer = 0x66;
		s.set_gain = 1;		s.gain = 0x0800;
		s.set_flags = 1;	s.flags = 0x0155;
		s.set_vectidx = 1;	s.vectidx = 0x1234;
		s.set_pcmrx = 1;	s.v90rx = 0;	s.k56rx = 0;
		s.set_rate = 1;
		s.baudrate = baud[i];
		s.carrier = carr[i];
		s.preemp = 3;
		both_seeded(V34HS_TX_L1, &s, tag);
		tag += 2;

		diff_eq_int("51 with is_short set moves the microstate to "
			    "INFODONE", v34hs_peek_short(1,
			    V34HS_MICROSTATE_OFF), V34HS_INFODONE, tag);
		diff_eq_int("51 with is_short set moves the txstate to TX_DPSK",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    V34HS_TX_DPSK, tag);
		/*
		 * AND THE MICROSTATE LINE COMES FIRST HERE.  The txstate line
		 * names the microstate it is printed beside, so the two orders
		 * produce two different lines out of the same two moves.
		 */
		diff_eq_int("51 announces the microstate move first",
			    blob_said("microstate TX_L1=>INFODONE(tx SSEG"),
			    1, tag);
		diff_eq_int("51 then the txstate move, with the microstate "
			    "already moved",
			    blob_said("txstate SSEG=>TX_DPSK(rx RX_DPSK, "
				      "mst INFODONE"), 1, tag);
		/*
		 * BOTH LINES CARRY THE PRE-CLEAR vect_idx AND COUNTER, which
		 * is the only reading that says the three clears happen after
		 * the two transitions rather than before them.
		 */
		diff_eq_int("51's transitions print vect_idx and the counter "
			    "before either is cleared",
			    blob_said("[1]4660, [2]42)"), 1, tag);
		diff_eq_int("51 clears vect_idx",
			    v34hs_peek_short(1, T3MT_VECTIDX), 0, tag);
		diff_eq_int("51 clears the counter",
			    v34hs_peek_short(1, T3MT_COUNTER), 0, tag);
		diff_eq_int("51 clears +0x35a2",
			    v34hs_peek_short(1, T3MT_F35A2), 0, tag);
		diff_eq_int("51 clears +0x358c",
			    v34hs_peek_short(1, T3MT_TOGGLE), 0, tag);
		diff_eq_int("51 leaves the receiver's flags alone on this path",
			    (unsigned short)v34hs_peek_short(1, T3MT_RX_FLAGS),
			    0x0155, tag);
		diff_eq_int("51 leaves the rxstate alone on this path",
			    v34hs_peek_short(1, V34HS_RXSTATE_OFF),
			    V34HS_RX_DPSK, tag);

		diff_eq_int("51 replaces the 2743 rate and no other",
			    v34hs_peek_short(1, T3MT_BAUDRATE), wbaud[i], tag);
		diff_eq_int("51 picks the 2743 carrier on the old one",
			    v34hs_peek_short(1, T3MT_CARRIER), wcarr[i], tag);
		diff_eq_int("51 announces the 2743 substitution",
			    blob_said("Illegal prev session baud (2743)"),
			    baud[i] == 0x0ab7, tag);

		/* The four copies, taken AFTER the switch may have replaced. */
		diff_eq_int("51 copies the baud rate to the receive side",
			    v34hs_peek_short(1, T3MT_RXBAUD), wbaud[i], tag);
		diff_eq_int("51 copies the carrier too",
			    v34hs_peek_short(1, T3MT_RXCARRIER), wcarr[i], tag);

		/* And the record, which is reached through +0xaa6c. */
		diff_eq_int("51 fills the message record's +0x18",
			    v34hs_peek_short(1, T3MT_INFOREC + 0x18), 0x26,
			    tag);
		diff_eq_int("51 fills the message record's +0x28",
			    v34hs_peek_short(1, T3MT_INFOREC + 0x28), 0x10,
			    tag);
		diff_eq_int("51 fills the message record's +0x20 with zero and "
			    "not with one", v34hs_peek_short(1,
			    T3MT_INFOREC + 0x20), 0, tag);
		diff_eq_int("51 ends by naming info1a",
			    blob_said("V34RETRAIN, transmitting info1a"), 1,
			    tag);
		diff_eq_int("51 prints the record it just filled",
			    blob_said("V34PROBE, txinfo1a (QC)"), 1, tag);
		/*
		 * THE POWER SCALE, by its contents and not by its address.
		 * All five scales differ from one another, so this is what
		 * separates the switch's six arms; without it every one of
		 * them writes a pointer the comparison skips and exchanging
		 * any two cannot fail.  The default arm is excluded because
		 * it writes nothing and both sides keep the fixture's dummy.
		 */
		if (baud[i] != 0x0123) {
			same_table("51's rate switch picks the baud's own "
				   "transmit power scale", 0xaa90, 28, tag);
			same_table("51 copies that scale to the receive side",
				   0xaaac, 28, tag);
		}

		if (baud[i] == 0x0ab7)
			saw_51_2743 = 1;
		else if (baud[i] == 0x0123)
			saw_51_dflt = 1;
		saw_51_info = 1;
	}

	/*
	 * The gain, which both bodies share.  Two thresholds, two shifts and a
	 * band that is left alone, all signed.
	 */
	for (i = 0; i < 5; i++) {
		static const short g[5] = { 0x2001, 0x2000, 0x1001, 0x1000,
					    (short)0xf000 };
		static const short w[5] = { 0x0800, 0x1000, 0x0800, 0x1000,
					    (short)0xf000 };
		struct seed s = plain;

		s.counter = 0x0029;
		s.set_isshort = 1;	s.isshort = 0;
		s.set_answer = 1;	s.answer = 0x66;
		s.set_gain = 1;		s.gain = g[i];
		both_seeded(V34HS_TX_L1, &s, tag);
		tag += 2;
		diff_eq_int("51 scales the gain by its band",
			    v34hs_peek_short(1, T3MT_RX_GAIN), w[i], tag);
		saw_51_gain[i < 1 ? 0 : (i < 3 ? 1 : 2)] = 1;
	}
}

static void
tail_paths(void)
{
	struct seed s;
	long tag = 4900;
	int i;

	/* +0x2218: 0 and 6 take nothing, 1, 2, 3, 4 and 5 each take an arm. */
	for (i = 0; i <= 6; i++) {
		s = plain;
		s.set_mode = 1;
		s.mode = i;
		s.set_baud = 1;
		s.baud = (short)(0x1234 + i);
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/*
	 * The 0x4a arm needs mode 2 or 3 AND txstate 0x4a, and 0x4a is 74
	 * SILENCERETRAIN, which table 2 sends to 0x644c9.  Both sides of the
	 * `vect_idx > 0x3c` test, because the two answers are 0 and 7 and a
	 * trial that only ever saw one of them could not tell them apart.
	 */
	for (i = 0; i < 2; i++) {
		s = plain;
		s.txstate = 0x4a;
		s.set_mode = 1;
		s.mode = 2;
		s.set_vectidx = 1;
		s.vectidx = (short)(i ? 0x3d : 0x3c);
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/* +0x238 against +0x23c, unsigned and straddled in both directions,
	   with the EQUAL case, which is what says the test is strict. */
	for (i = 0; i < 4; i++) {
		static const int el[4] = { 100, 200, -1, 150 };
		static const int dl[4] = { 200, 100, 5, 150 };

		s = plain;
		s.set_time = 1;
		s.elapsed = el[i];
		s.deadline = dl[i];
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/* agc_level against +0x230: equal, above and below. */
	for (i = 0; i < 3; i++) {
		static const short lv[3] = { 300, 300, -300 };
		static const int fl[3] = { 300, 500, 100 };

		s = plain;
		s.set_level = 1;
		s.level = lv[i];
		s.floor = fl[i];
		s.set_tick = 1;
		s.tick = 0x1000;
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/* +0x234 across 0x257f, with the level below the floor so the counter
	   is incremented rather than zeroed. */
	for (i = 0; i < 3; i++) {
		static const int tk[3] = { 0x257e, 0x257f, 0x2580 };

		s = plain;
		s.set_level = 1;
		s.level = -1000;
		s.floor = 1000;
		s.set_tick = 1;
		s.tick = tk[i];
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/* The three txstates past table 2's window, and one that is not. */
	for (i = 0; i < 4; i++) {
		static const short tx[4] = { 0x52, 0x53, 0x54, 0x55 };

		s = plain;
		s.txstate = tx[i];
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}
}

/*
 * --------------------------------------------------------------------------
 * 0x65b72 -- microstate 55 `TX_PHASE1_CALL`.
 *
 * THE FIRST GUARD IS NOT A FIELD, and that is the whole difficulty of this
 * arm: it asks whether `fskdemodulate` moved `fsk.nbits`, comparing the field
 * against a copy taken before the call.  So a trial has to make the
 * demodulator sample a bit, or not, on purpose -- which nothing else in this
 * file has needed and which is why `set_bitclk` exists.
 *
 * Three families, and each says what holds the other two off:
 *
 *   A  the reset          the demodulator RUNS (`fsk_inhibit` cleared) and the
 *                         bit clock is armed or not; `+0x358a` away from 2 so
 *                         block C cannot fire, and the counter far below 0x5f
 *   B  the 0x5f threshold the demodulator is INHIBITED, so `nbits` cannot move
 *                         and the reset cannot fire whatever the shift
 *                         register holds; `+0x3588` at 4
 *   C  the 32-bit word    the demodulator is INHIBITED for the same reason,
 *                         and the counter is 0x10 so block B stays on its
 *                         quiet branch
 *
 * A AND C CANNOT BOTH RUN IN ONE STEP: A's reset leaves `+0x358a` at 1 and the
 * shift register at -1, and C wants `+0x358a` at 2 and `(sr & 0x3ff)` at
 * 0x372.  One trial says so on the value rather than by not being written.
 */
static int saw_55_reset, saw_55_nonbits, saw_55_nosr, saw_55_below;
static int saw_55_over, saw_55_arm, saw_55_noarm, saw_55_exclusive;

static void
micro55(void)
{
	static const char reset_msg[] = "Repeated info0 is detected, "
					"errorrecovery is initialized in "
					"TX_PHASE1_CALL";
	static const char arm_msg[] = "V34 In Retrain. errorrecovery for info0 "
				      "is initialized in TX_PHASE1_CALL";
	/*
	 * The reset.  Exactly one bit is sampled when the clock is armed, so
	 * the shift register after the demodulator is `(sr << 1) | bitval` and
	 * the `& 7 == 7` test can be probed a bit at a time -- 0x03 with a 1
	 * shifted in is 0x07 and each of the three neighbours is one bit out.
	 */
	static const struct {
		short	sr, bitval, phase, next;
		int	moved, reset;
		const char *why;
	} rr[] = {
	  { 0x0003, 1, 0x0100, 0, 1, 1, "0x3 << 1 | 1 is 7" },
	  { 0x0003, 0, 0x0100, 0, 1, 0, "0x3 << 1 | 0 is 6" },
	  { 0x0001, 1, 0x0100, 0, 1, 0, "0x1 << 1 | 1 is 3" },
	  { 0x0002, 1, 0x0100, 0, 1, 0, "0x2 << 1 | 1 is 5" },
	  { 0x0007, 1, 0x0100, 0, 1, 1, "and 0xf has 47's four as well" },
	  { 0x01fb, 1, 0x0100, 0, 1, 1, "the mask is three bits and not the "
					"whole register" },
	  { 0x01b9, 1, 0x0100, 0, 1, 0, "49's 0x372 does not reset 55" },
	  /*
	   * AND THE GUARD IS THE MOVE AND NOT THE REGISTER.  The demodulator
	   * runs on this row too -- the same code, the same input -- but the
	   * bit clock is not due, so `nbits` does not change and the reset
	   * declines with a shift register that would otherwise take it.
	   */
	  { 0x0007, 1, 0, 0x7f00, 0, 0, "nbits did not move" },
	  { 0x0003, 1, 0, 0x7f00, 0, 0, "nor here" }
	};
	/* The 0x5f threshold, with the demodulator inhibited throughout. */
	static const struct {
		short	counter;
		int	over;
		const char *why;
	} th[] = {
	  { 0x005e, 0, "n == 0x5f is not above 0x5f" },
	  { 0x005f, 1, "n == 0x60 is" },
	  { 0x0100, 1, "and so is 0x101" },
	  { 0x7fff, 0, "0x8000 is -32768 and the compare is signed" },
	  { (short)0xfffe, 0, "0xffff is -1" },
	  { (short)0xffff, 0, "and 0x0000 is not above it either" }
	};
	/* The 32-bit word at +0x3588, and the mask on the shift register. */
	static const struct {
		short	short_3588, short_358a, sr;
		int	armed;
		const char *why;
	} ar[] = {
	  { 2, 2, 0x0372, 1, "+0x3588 2, +0x358a 2 and sr 0x372" },
	  { 2, 3, 0x0372, 0, "+0x358a must be 2" },
	  { 3, 2, 0x0372, 0, "and +0x3588 must be 2" },
	  { 2, 2, 0x0371, 0, "0x371 is not 0x372" },
	  { 2, 2, 0x0772, 1, "but 0x772 is, masked with 0x3ff" },
	  { 2, 2, (short)0xfb72, 1, "and so is 0xfb72" },
	  { 2, 2, 0x0172, 0, "0x172 lacks bit 9, which is inside both the "
			     "mask and the constant" }
	};
	long tag = 7600;
	short tog0;
	int i;

	/*
	 * The fill is deterministic, so +0x358c holds the same halfword at
	 * every trial below and the inversion can be read against it.  Taken
	 * from a bring-up of its own rather than from a step, because after a
	 * step both sides have already inverted it.
	 */
	v34hs_setup(0);
	tog0 = v34hs_peek_short(1, T3MT_TOGGLE);

	for (i = 0; i < (int)(sizeof(rr) / sizeof(rr[0])); i++) {
		struct seed s = plain;
		char what[192];

		s.counter = 0x0010;		/* far below 0x5f */
		s.set_f3588 = 1;	s.short_3588 = 4;	/* block C off */
		s.set_f358a = 1;	s.short_358a = 5;
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		s.set_fsk = 1;			/* the demodulator RUNS */
		s.set_bitclk = 1;
		s.nbits = 0x0040;	s.bitval = rr[i].bitval;
		s.phase = rr[i].phase;	s.next = rr[i].next;
		s.bit_len = 0x0400;	s.resync = 0x0400;
		s.set_sr = 1;		s.sr = rr[i].sr;
		both_seeded(V34HS_TX_PHASE1_CALL, &s, tag);
		tag += 2;

		/*
		 * WHAT THE DEMODULATOR DID, read off the blob.  Without this
		 * the whole family rests on a model of `fskdemodulate` rather
		 * than on a measurement, and a bit clock that never fired
		 * would look like a guard that never took.
		 */
		diff_eq_int("55's bit clock sampled exactly one bit, or none",
			    (int)v34hs_peek_short(1, T3MT_FSK_NBITS),
			    rr[i].reset ? 0	/* the reset clears it */
					: 0x0040 + rr[i].moved, tag);

		snprintf(what, sizeof(what), "55's reset: %s", rr[i].why);
		diff_eq_int(what, blob_said(reset_msg), rr[i].reset, tag);
		/*
		 * FOUR LINES OR NONE, and they are 59's four in 59's order:
		 * the phrase, the shift register, and then the two
		 * transitions -- which is the opposite of 47's, 49's and 50's
		 * copies of the same body.
		 *
		 * THREE MORE ARE NOT THIS ARM'S.  Clearing `fsk_inhibit` also
		 * means seeding the receive window, and `V34agc` then prints
		 * `V34AGC, overflow = ...` three times before the microstate
		 * is even read -- on every row of this family, reset or not.
		 * Counted rather than filtered out, because a family that
		 * subtracted an unknown would not notice the day it changed.
		 */
		diff_eq_int("55's reset prints four lines past the three "
			    "`V34agc` prints the demodulator's own seed causes",
			    v34hs_observed(1)->lines, rr[i].reset ? 7 : 3, tag);
		if (rr[i].reset) {
			char line[40];

			snprintf(line, sizeof(line), "V21RXBUF=0x%x",
				 (unsigned)(unsigned short)
				 ((rr[i].sr << 1) | rr[i].bitval));
			diff_eq_int("55's reset prints the register the "
				    "demodulator left, before clearing it",
				    blob_said(line), 1, tag);
		}
		diff_eq_int("55's reset moves the microstate to DET_SYNC",
			    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    rr[i].reset ? V34HS_DET_SYNC
					: V34HS_TX_PHASE1_CALL, tag);
		diff_eq_int("55's reset ORs bit 0 into +0x3588, not 4",
			    v34hs_peek_short(1, T3MT_F3588),
			    rr[i].reset ? 5 : 4, tag);
		/*
		 * AND IT LEAVES THE COUNTER AT ONE, not at zero.  This copy
		 * clears the counter and jumps back INTO the increment, where
		 * 59's clears it and jumps past -- so the two arms' resets are
		 * separated by exactly this halfword.
		 */
		diff_eq_int("55's reset leaves the counter at one",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    rr[i].reset ? 1 : 0x0011, tag);

		if (rr[i].reset)
			saw_55_reset = 1;
		else if (rr[i].moved)
			saw_55_nosr = 1;
		else
			saw_55_nonbits = 1;
	}

	for (i = 0; i < (int)(sizeof(th) / sizeof(th[0])); i++) {
		struct seed s = plain;
		char what[192];
		unsigned short n = (unsigned short)(th[i].counter + 1);

		s.counter = th[i].counter;
		s.set_sr = 1;		s.sr = 0x0157;	/* & 7 == 7, and the
							   demodulator is off,
							   so it cannot matter */
		s.set_f3588 = 1;	s.short_3588 = 4;
		s.set_f358a = 1;	s.short_358a = 5;
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		both_seeded(V34HS_TX_PHASE1_CALL, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "55 at 0x5f: %s", th[i].why);
		diff_eq_int(what, v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    th[i].over ? V34HS_RX_PHASE2_CALL
				       : V34HS_TX_PHASE1_CALL, tag);
		diff_eq_int("55 clears the counter past 0x5f and stores it "
			    "below",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    th[i].over ? 0 : n, tag);
		/*
		 * BIT 0 OF +0x358c IS INVERTED AND NOT SET.  The seed comes
		 * off the fixture's fill rather than from a poke, so the
		 * inversion is read against whatever was there.
		 */
		diff_eq_int("55 inverts bit 0 of +0x358c past 0x5f",
			    v34hs_peek_short(1, T3MT_TOGGLE),
			    th[i].over ? (short)(tog0 ^ 1) : tog0, tag);
		/*
		 * TWO LINES OR NONE: the transition and the shift register,
		 * and the register line is INSIDE the branch -- a step that
		 * stayed below 0x5f prints nothing at all.
		 */
		diff_eq_int("55 prints two lines past 0x5f and none below",
			    v34hs_observed(1)->lines, th[i].over ? 2 : 0, tag);
		if (th[i].over) {
			char line[40];

			snprintf(line, sizeof(line), "V21RXBUF=0x%x", 0x157);
			diff_eq_int("and the second is the shift register",
				    blob_said(line), 1, tag);
			saw_55_over = 1;
		} else {
			saw_55_below = 1;
		}
	}

	for (i = 0; i < (int)(sizeof(ar) / sizeof(ar[0])); i++) {
		struct seed s = plain;
		char what[192];

		s.counter = 0x0010;
		s.set_sr = 1;		s.sr = ar[i].sr;
		s.set_f3588 = 1;	s.short_3588 = ar[i].short_3588;
		s.set_f358a = 1;	s.short_358a = ar[i].short_358a;
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		s.set_rec = 1;		s.rec_at = T3MT_INFOREC;
		s.rec20 = 0x1234;	s.rec22 = 0x5678;
		both_seeded(V34HS_TX_PHASE1_CALL, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "55's second reset: %s",
			 ar[i].why);
		diff_eq_int(what, blob_said(arm_msg), ar[i].armed, tag);
		diff_eq_int("55's second reset moves the microstate to "
			    "DET_SYNC",
			    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    ar[i].armed ? V34HS_DET_SYNC
					: V34HS_TX_PHASE1_CALL, tag);
		diff_eq_int("55's second reset moves the txstate to TX_DPSK",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    ar[i].armed ? V34HS_TX_DPSK : T3MT_TXSTATE, tag);
		diff_eq_int("55's second reset sets bit 0 of +0x3588 after "
			    "the init has rewritten it",
			    v34hs_peek_short(1, T3MT_F3588) & 1,
			    ar[i].armed ? 1 : (ar[i].short_3588 & 1), tag);
		/*
		 * `v34handshakinit` CLEARS THE COUNTER ITSELF (finding F375),
		 * so a step whose second reset ran comes back with 0 where an
		 * ordinary one comes back with the incremented value.
		 */
		diff_eq_int("55's second reset leaves the counter at zero",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    ar[i].armed ? 0 : 0x0011, tag);
		diff_eq_int("55 re-aims +0xaa6c at the first record",
			    (int)selfptr_offset(1),
			    ar[i].armed ? T3MT_MSGREC0 : T3MT_INFOREC, tag);

		if (ar[i].armed)
			saw_55_arm = 1;
		else
			saw_55_noarm = 1;
	}

	/*
	 * THE TWO RESETS CANNOT BOTH RUN, and this trial says so on the value.
	 * The word at +0x3588 is seeded 0x20002, which is exactly what block C
	 * wants -- and the reset above fires first, leaves 3 and 1 there, and
	 * overwrites the shift register with -1, so block C declines twice
	 * over.  A reading in which the reset stored 4 rather than setting bit
	 * 0 would leave 4 here.
	 */
	{
		struct seed s = plain;

		s.counter = 0x0010;
		s.set_f3588 = 1;	s.short_3588 = 2;
		s.set_f358a = 1;	s.short_358a = 2;
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		s.set_fsk = 1;
		s.set_bitclk = 1;
		s.nbits = 0x0040;	s.bitval = 1;
		s.phase = 0x0100;	s.next = 0;
		s.bit_len = 0x0400;	s.resync = 0x0400;
		s.set_sr = 1;		s.sr = 0x0003;
		both_seeded(V34HS_TX_PHASE1_CALL, &s, tag);
		tag += 2;

		diff_eq_int("55's reset ran", blob_said(reset_msg), 1, tag);
		diff_eq_int("and left +0x3588 at 3, not at 4",
			    v34hs_peek_short(1, T3MT_F3588), 3, tag);
		diff_eq_int("and +0x358a at 1, which is not the 2 the second "
			    "reset wants",
			    v34hs_peek_short(1, T3MT_F358A), 1, tag);
		diff_eq_int("so the second reset declined",
			    blob_said(arm_msg), 0, tag);
		diff_eq_int("and the counter is one and not zero",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    1, tag);
		saw_55_exclusive = 1;
	}
}

/*
 * --------------------------------------------------------------------------
 * 0x66003 -- microstate 58 `RX_PHASE1_CALL`.
 *
 * FOUR BLOCKS AND THREE THRESHOLDS ON ONE COUNTER -- 0x5f, 0x3bf and 0x4b0 --
 * which a single step can cross all of.  Each family below holds the others
 * off and says what holds them off:
 *
 *   head  the txstate, and it is TX_DPSK and not the SSEG every other case in
 *         this file is driven with, so it is skipped unless a trial asks for
 *         it.  `+0xaa6c` is aimed at +0xa9ac, which is NOT the record the two
 *         resets fill, so "reached through the pointer" is a claim that can
 *         fail.
 *   0x5f  `+0x3588` at 4, which has bit 1 clear and is not zero, so neither
 *         reset can fire whatever the counter does
 *   0x3bf `+0x3588` at 2, 3 or 6 and `+0x358a` at 2, and a shift register of
 *         0x372 -- which is not 1, so the deep path above cannot fire
 *   0x4b0 `+0x3588` at 0, which is what block 3 needs and what stops block 2
 *
 * THE TWO RESETS CANNOT BOTH RUN, and one family says so rather than leaving
 * it to be inferred: block 2 leaves bit 0 of +0x3588 set and block 3 needs the
 * whole halfword zero.
 *
 * AND THE COUNTER IS READ THREE TIMES.  The third read is the one that can be
 * seen: `v34handshakinit` zeroes +0xaa78 itself (finding F375), so a step whose
 * block 2 takes the branch that calls it comes back with 0 and cannot reach
 * block 3, while the branch that does not call it comes back with the value it
 * went in with.
 */
static int saw_58_head, saw_58_nohead, saw_58_headtx;
static int saw_58_deep, saw_58_shallow, saw_58_flag;
static int saw_58_armed, saw_58_noarm, saw_58_init, saw_58_noinit;
static int saw_58_reset, saw_58_noreset, saw_58_exclusive;

static void
micro58(void)
{
	static const char deep_msg[] = "V34RETRAIN, RX_PHASE1_CALL received,";
	static const char arm_msg[] = "errorrecovery for info0 is initialized "
				      "in RX_PHASE1_CALL";
	/*
	 * The head.  Two non-zero halfwords and a clear of the first, so every
	 * one of the three ways to write it wrong is a different answer here:
	 * dropping either condition takes a row it should not, and exchanging
	 * +0x20 for +0x22 clears the wrong field.
	 */
	static const struct {
		short	txstate, rec20, rec22;
		int	cleared;
		const char *why;
	} hd[] = {
	  { V34HS_TX_DPSK, 0x1234, 0x5678, 1, "both non-zero" },
	  { V34HS_TX_DPSK, 0,      0x5678, 0, "+0x20 zero" },
	  { V34HS_TX_DPSK, 0x1234, 0,      0, "+0x22 zero" },
	  { V34HS_TX_DPSK, 0,      0,      0, "both zero" },
	  { T3MT_TXSTATE,  0x1234, 0x5678, 0, "the txstate is SSEG and not 24" },
	  { V34HS_SILENCE, 0x1234, 0x5678, 0, "nor is SILENCE" },
	  { V34HS_TX_L1,   0x1234, 0x5678, 0, "nor TX_L1, which is 51 and not "
					      "0x18" }
	};
	/*
	 * The first threshold and the deep path.  `sr` is a FULL-WIDTH
	 * equality against 1, so 0x101 and 0x201 -- which every masked reading
	 * of the same register would accept -- must decline.
	 */
	static const struct {
		short	counter, sr, filt, gain;
		int	deep, flagged;
		const char *why;
	} dp[] = {
	  { 0x005e, 0x0001, 0x0123, 0x2346, 0, 0, "n == 0x5f is not above" },
	  { 0x005f, 0x0001, 0x0123, 0x2346, 1, 1, "n == 0x60 is" },
	  { 0x005f, 0x0000, 0x0123, 0x2346, 0, 1, "the flag is set anyway" },
	  { 0x005f, 0x0101, 0x0123, 0x2346, 0, 1, "0x101 is not 1" },
	  { 0x005f, 0x0201, 0x0123, 0x2346, 0, 1, "nor is 0x201" },
	  { 0x005f, 0x0007, 0x0123, 0x2346, 0, 1, "nor 59's 0x7" },
	  { 0x005f, 0x0372, 0x0123, 0x2346, 0, 1, "nor 49's 0x372" },
	  { 0x005f, 0x000f, 0x0123, 0x2346, 0, 1, "nor 47's 0xf" },
	  /* The compare is SIGNED on the incremented halfword. */
	  { (short)0xfffe, 0x0001, 0x0123, 0x2346, 0, 0, "0xffff is -1" },
	  { 0x7fff, 0x0001, 0x0123, 0x2346, 0, 0, "and 0x8000 is -32768" },
	  /* Both arguments of the diagnostic are SIGN-extended. */
	  { 0x005f, 0x0001, (short)0xfff0, (short)0xdcba, 1, 1,
	    "a negative filtdelay and a negative gain" }
	};
	/* Block 2, and the branch inside it that calls `v34handshakinit`. */
	static const struct {
		short	counter, short_3588, short_358a, sr;
		int	armed, init;
		const char *why;
	} ar[] = {
	  { 0x03bf, 2, 2, 0x0372, 1, 1, "+0x3588 exactly 2 calls the init" },
	  { 0x03bf, 6, 2, 0x0372, 1, 0, "6 has bit 1 and does not" },
	  { 0x03bf, 3, 2, 0x0372, 1, 0, "nor does 3" },
	  { 0x03bf, 4, 2, 0x0372, 0, 0, "4 has bit 1 clear" },
	  { 0x03bf, 0, 2, 0x0372, 0, 0, "and so has 0" },
	  { 0x03bf, 2, 3, 0x0372, 0, 0, "+0x358a must be exactly 2" },
	  { 0x03bf, 2, 0, 0x0372, 0, 0, "and zero is not 2 either" },
	  { 0x03bf, 2, 2, 0x0371, 0, 0, "0x371 is not 0x372" },
	  { 0x03bf, 2, 2, 0x0772, 1, 1, "but 0x772 is, masked with 0x3ff" },
	  { 0x03bf, 2, 2, (short)0xfb72, 1, 1, "and so is 0xfb72" },
	  { 0x03bf, 2, 2, 0x0172, 0, 0, "0x172 lacks bit 9, which is inside "
					"both the mask and the constant" },
	  { 0x03be, 2, 2, 0x0372, 0, 0, "n == 0x3bf is not above 0x3bf" }
	};
	/* Block 3, which is `t3m_errrec_reset` and stores the constant 4. */
	static const struct {
		short	counter, short_3588, sr;
		int	reset;
		const char *why;
	} rs[] = {
	  { 0x04b0, 0, 0x0372, 1, "n == 0x4b1 is above 0x4b0" },
	  { 0x04af, 0, 0x0372, 0, "n == 0x4b0 is not" },
	  { 0x0500, 4, 0x0372, 0, "+0x3588 must be zero, and 4 is not" },
	  { 0x0500, 2, 0x0372, 0, "nor is 2 -- and 2 is what block 2 wants" },
	  { 0x0500, 0, 0x0000, 0, "the shift register must not be zero" },
	  { 0x0500, 0, 0x0002, 1, "and 2 is enough: it is a test against "
				  "zero and not a mask" },
	  { 0x0500, 0, (short)0x8000, 1, "so is 0x8000" }
	};
	long tag = 7000;
	int i;

	for (i = 0; i < (int)(sizeof(hd) / sizeof(hd[0])); i++) {
		struct seed s = plain;
		char what[192];

		s.txstate = hd[i].txstate;
		s.counter = 0x0100;		/* below every threshold */
		s.set_sr = 1;		s.sr = 0x0155;
		s.set_f3588 = 1;	s.short_3588 = 4;	/* both resets off */
		s.set_f358a = 1;	s.short_358a = 5;
		s.set_filt = 1;		s.filt = 0x2000;
		s.set_rec = 1;		s.rec_at = T3MT_INFOREC;
		s.rec20 = hd[i].rec20;	s.rec22 = hd[i].rec22;
		both_seeded(V34HS_RX_PHASE1_CALL, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "58's head: %s", hd[i].why);
		diff_eq_int(what,
			    v34hs_peek_short(1, T3MT_INFOREC + 0x20),
			    hd[i].cleared ? 0 : hd[i].rec20, tag);
		diff_eq_int("58's head leaves +0x22 alone",
			    v34hs_peek_short(1, T3MT_INFOREC + 0x22),
			    hd[i].rec22, tag);
		/*
		 * AND IT GOES THROUGH THE POINTER.  The record at +0xa94c is
		 * the one both resets fill by address; neither reset runs
		 * here, so it must still hold its sentinels.
		 */
		diff_eq_int("58's head does not touch the record at +0xa94c",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x20), 0x7a11,
			    tag);
		diff_eq_int("nor its +0x22",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x22), 0x7a22,
			    tag);
		diff_eq_int("and the head does not move the pointer",
			    (int)selfptr_offset(1), T3MT_INFOREC, tag);

		if (hd[i].cleared)
			saw_58_head = 1;
		else if (hd[i].txstate == V34HS_TX_DPSK)
			saw_58_nohead = 1;
		else
			saw_58_headtx = 1;
	}

	for (i = 0; i < (int)(sizeof(dp) / sizeof(dp[0])); i++) {
		struct seed s = plain;
		char what[192];
		unsigned short n = (unsigned short)(dp[i].counter + 1);

		s.counter = dp[i].counter;
		s.set_sr = 1;		s.sr = dp[i].sr;
		s.set_f3588 = 1;	s.short_3588 = 4;	/* both resets off */
		s.set_f358a = 1;	s.short_358a = 5;
		s.set_filt = 1;		s.filt = dp[i].filt;
		s.set_gain = 1;		s.gain = dp[i].gain;
		s.set_flags = 1;	s.flags = 0x0115;  /* 0x200 clear */
		both_seeded(V34HS_RX_PHASE1_CALL, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "58's deep path: %s", dp[i].why);
		diff_eq_int(what, v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    dp[i].deep ? V34HS_TX_PHASE1_CALL
				       : V34HS_RX_PHASE1_CALL, tag);
		/*
		 * 0x200 IS SET BEFORE THE SHIFT-REGISTER TEST, so every trial
		 * past 0x5f has it whether or not the body below ran.  Without
		 * this the flag would look like part of the deep path.
		 */
		diff_eq_int("58 sets 0x200 in the receiver's flags on every "
			    "step past 0x5f",
			    (v34hs_peek_short(1, T3MT_RX_FLAGS) & 0x200) != 0,
			    dp[i].flagged, tag);
		diff_eq_int("58's deep path replaces the counter with filtdelay",
			    v34hs_peek_short(1, T3MT_COUNTER),
			    dp[i].deep ? dp[i].filt : (short)n, tag);
		diff_eq_int("58's deep path sets +0x358a to 2",
			    v34hs_peek_short(1, T3MT_F358A),
			    dp[i].deep ? 2 : 5, tag);
		diff_eq_int("58's deep path copies the gain to the receiver's "
			    "+0x264",
			    v34hs_peek_short(1, T3MT_RX_F264),
			    dp[i].deep ? dp[i].gain
				       : v34hs_peek_short(0, T3MT_RX_F264),
			    tag);
		diff_eq_int("58's deep path prints one line and nothing else "
			    "does", blob_said(deep_msg), dp[i].deep, tag);

		if (dp[i].deep) {
			char line[192];

			/*
			 * THE LINE IS PRINTED BEFORE THE COUNTER IS REPLACED,
			 * so `count2` is the value that crossed 0x5f and not
			 * filtdelay -- and both of the other two are
			 * sign-extended, which a negative seed is what says.
			 */
			snprintf(line, sizeof(line),
				 "count2=%d,rx->gain=0x%x,filtdelay=%d",
				 (int)(short)n, (unsigned)(int)dp[i].gain,
				 (int)dp[i].filt);
			diff_eq_int("58's line is printed before the counter "
				    "moves, and sign-extends all three",
				    blob_said(line), 1, tag);
			saw_58_deep = 1;
		} else {
			saw_58_shallow = 1;
		}
		if (dp[i].flagged)
			saw_58_flag = 1;
	}

	for (i = 0; i < (int)(sizeof(ar) / sizeof(ar[0])); i++) {
		struct seed s = plain;
		char what[192];
		unsigned short n = (unsigned short)(ar[i].counter + 1);

		s.counter = ar[i].counter;
		s.set_sr = 1;		s.sr = ar[i].sr;
		s.set_f3588 = 1;	s.short_3588 = ar[i].short_3588;
		s.set_f358a = 1;	s.short_358a = ar[i].short_358a;
		s.set_filt = 1;		s.filt = 0x2000;
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		s.set_rec = 1;		s.rec_at = T3MT_INFOREC;
		s.rec20 = 0x1234;	s.rec22 = 0x5678;
		both_seeded(V34HS_RX_PHASE1_CALL, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "58's second reset: %s",
			 ar[i].why);
		diff_eq_int(what, blob_said(arm_msg), ar[i].armed, tag);
		diff_eq_int("58's second reset moves the microstate to "
			    "DET_SYNC",
			    v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    ar[i].armed ? V34HS_DET_SYNC
					: V34HS_RX_PHASE1_CALL, tag);
		diff_eq_int("58's second reset moves the txstate to TX_DPSK",
			    v34hs_peek_short(1, V34HS_TXSTATE_OFF),
			    ar[i].armed ? V34HS_TX_DPSK : T3MT_TXSTATE, tag);
		/*
		 * IT SETS BIT 0 RATHER THAN STORING 4, and the two seeds that
		 * arm without the call -- 3 and 6 -- are what say so exactly:
		 * a store of 4 would give 4 and a store of 1 would give 1.
		 * The seed of 2 cannot: `v34handshakinit` rewrites the field.
		 */
		if (ar[i].armed && !ar[i].init)
			diff_eq_int("58's second reset ORs bit 0 into +0x3588",
				    v34hs_peek_short(1, T3MT_F3588),
				    (short)(ar[i].short_3588 | 1), tag);
		else if (ar[i].armed)
			diff_eq_int("and bit 0 is set after the init too",
				    v34hs_peek_short(1, T3MT_F3588) & 1, 1,
				    tag);
		else
			diff_eq_int("and a declined reset leaves +0x3588",
				    v34hs_peek_short(1, T3MT_F3588),
				    ar[i].short_3588, tag);
		/*
		 * THE COUNTER IS READ A THIRD TIME AFTER THE RESET.  On the
		 * branch that calls `v34handshakinit` the counter comes back
		 * zero, because that function clears it (finding F375), and
		 * every threshold below is then out of reach; on the branch
		 * that does not, it still holds the value it went in with.
		 */
		diff_eq_int("58 re-reads the counter after its second reset",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    ar[i].init ? 0 : n, tag);
		/*
		 * AND THE SELF-POINTER MOVES ONLY ON THAT BRANCH.  The trials
		 * aim it at +0xa9ac, so "re-aimed at +0xa94c" is a change and
		 * not the value it already had.
		 */
		diff_eq_int("58 re-aims +0xaa6c only when it calls the init",
			    (int)selfptr_offset(1),
			    ar[i].init ? T3MT_MSGREC0 : T3MT_INFOREC, tag);

		if (ar[i].armed) {
			saw_58_armed = 1;
			if (ar[i].init)
				saw_58_init = 1;
			else
				saw_58_noinit = 1;
		} else {
			saw_58_noarm = 1;
		}
	}

	for (i = 0; i < (int)(sizeof(rs) / sizeof(rs[0])); i++) {
		struct seed s = plain;
		char what[192];
		unsigned short n = (unsigned short)(rs[i].counter + 1);

		s.counter = rs[i].counter;
		s.set_sr = 1;		s.sr = rs[i].sr;
		s.set_f3588 = 1;	s.short_3588 = rs[i].short_3588;
		s.set_f358a = 1;	s.short_358a = 5;
		s.set_filt = 1;		s.filt = 0x2000;
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		both_seeded(V34HS_RX_PHASE1_CALL, &s, tag);
		tag += 2;

		snprintf(what, sizeof(what), "58's first reset: %s", rs[i].why);
		diff_eq_int(what, v34hs_peek_short(1, V34HS_MICROSTATE_OFF),
			    rs[i].reset ? V34HS_DET_SYNC
					: V34HS_RX_PHASE1_CALL, tag);
		/*
		 * AND IT STORES THE CONSTANT 4, which is the other half of
		 * finding F375's split: +0x3588 is seeded ZERO on every row
		 * that fires, so `|= 1` would leave 1 and this leaves 4.
		 */
		diff_eq_int("58's first reset stores 4 into +0x3588, not bit 0",
			    v34hs_peek_short(1, T3MT_F3588),
			    rs[i].reset ? 4 : rs[i].short_3588, tag);
		diff_eq_int("58's first reset does not clear the counter",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    n, tag);
		diff_eq_int("58's first reset fills the record at +0xa94c",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x24),
			    rs[i].reset ? 0x0f72
					: v34hs_peek_short(0,
							   T3MT_MSGREC0 + 0x24),
			    tag);
		/*
		 * TWO LINES OR NONE: this reset prints no phrase of its own,
		 * so what the transcript holds is exactly the two transitions
		 * `t3m_errrec_core` announces.
		 */
		diff_eq_int("58's first reset prints two lines or none",
			    v34hs_observed(1)->lines, rs[i].reset ? 2 : 0, tag);

		if (rs[i].reset)
			saw_58_reset = 1;
		else
			saw_58_noreset = 1;
	}

	/*
	 * THE TWO RESETS CANNOT BOTH RUN IN ONE STEP, and this is the trial
	 * that says it rather than the absence of a trial that shows it.  The
	 * counter is past 0x4b0 and +0x3588 is 6, so block 2 fires without
	 * calling the init and leaves 7 -- and block 3 wants zero, so it
	 * declines.  A reading in which block 2 stored 4 would leave 4 here
	 * and block 3 would still decline, so the assertion is on the value.
	 */
	{
		struct seed s = plain;

		s.counter = 0x0500;
		s.set_sr = 1;		s.sr = 0x0372;
		s.set_f3588 = 1;	s.short_3588 = 6;
		s.set_f358a = 1;	s.short_358a = 2;
		s.set_filt = 1;		s.filt = 0x2000;
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		both_seeded(V34HS_RX_PHASE1_CALL, &s, tag);
		tag += 2;

		diff_eq_int("58's second reset runs and leaves bit 0 set",
			    v34hs_peek_short(1, T3MT_F3588), 7, tag);
		diff_eq_int("so 58's first reset declines, though the counter "
			    "is past 0x4b0",
			    (unsigned short)v34hs_peek_short(1, T3MT_COUNTER),
			    0x0501, tag);
		diff_eq_int("and it printed its own phrase and nothing else's",
			    blob_said(arm_msg), 1, tag);
		saw_58_exclusive = 1;
	}

	/*
	 * AND THE 0x1e RECORD.  `t3m_errrec_core`'s only conditional field
	 * needs the originate flag AND a V.90 receiver, and both answers are
	 * driven here through 58's first reset -- the other arms reach the
	 * same body, but only through their own guards, so a trial here is
	 * what says 58 reaches the conditional at all.
	 */
	for (i = 0; i < 3; i++) {
		static const short ans[3] = { 0x65, 0x65, 0x66 };
		static const int v90[3] = { 0x1234, 0, 0x1234 };
		static const short want[3] = { 0x1e, 0x11, 0x11 };
		struct seed s = plain;

		s.counter = 0x0500;
		s.set_sr = 1;		s.sr = 0x0372;
		s.set_f3588 = 1;	s.short_3588 = 0;
		s.set_f358a = 1;	s.short_358a = 5;
		s.set_filt = 1;		s.filt = 0x2000;
		s.set_errrec = 1;	s.errrec = 0x2f1d;
		s.set_answer = 1;	s.answer = ans[i];
		s.set_pcmrx = 1;	s.v90rx = v90[i];
		s.k56rx = 0x5678;
		both_seeded(V34HS_RX_PHASE1_CALL, &s, tag);
		tag += 2;

		diff_eq_int("58's reset picks 0x1e only when this end "
			    "originates and a V.90 receiver is running",
			    v34hs_peek_short(1, T3MT_MSGREC0 + 0x18), want[i],
			    tag);
	}
}

/*
 * --------------------------------------------------------------------------
 * .rodata+0x2ee8 -- the three transmit arms a table-3 arm can hand control to.
 *
 * These are not a reconstruction of table 2, which is #56's; they are here
 * because an arm that ends in `jmp 62af1` cannot be compared without them.
 * What this drives is what microstate 48 can reach: SSEG and SBARSEG on the
 * ordinary path, SILENCE on the 0xa2 path, the 24/51/54/60/74 arm, and an
 * index past the end.
 */
static void
txblock_paths(void)
{
	struct seed s;
	long tag = 4970;
	int i;

	/* 0x64518: originate answers 2 whatever the flag says, and the answer
	   side answers 3 or 2 on bit 3 of the receiver's flags. */
	for (i = 0; i < 4; i++) {
		s = plain;
		s.txstate = (short)(i & 1 ? 19 : 18);
		s.set_answer = 1;
		s.answer = (short)(i < 2 ? 0x65 : 0x66);
		s.set_flags = 1;
		s.flags = (short)(i & 1 ? 0x3fff : 0x37f7);
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/* 0x644c9, five txstates into one arm. */
	for (i = 0; i < 5; i++) {
		static const short tx[5] = { 24, 51, 54, 60, 74 };

		s = plain;
		s.txstate = tx[i];
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}

	/*
	 * 0x64480, reached the only way a table-3 arm can reach it: microstate
	 * 48 at the 0xa2 threshold forces txstate to SILENCE.  Its three
	 * routes to progress 1 all want a microstate or rxstate this route
	 * cannot present -- by the time the arm gets there the microstate is
	 * RX_PHASE2_ANS and the rxstate is RX_DPSK -- so what is exercised
	 * here is the fall-through, and the finding says so.
	 */
	for (i = 0; i < 2; i++) {
		s = plain;
		s.counter = 0x00a1;
		s.set_answer = 1;
		s.answer = (short)(i ? 0x66 : 0x65);
		both_seeded(V34HS_TX_PHASE3_ANS, &s, tag);
		tag += 2;
	}
}

/*
 * --------------------------------------------------------------------------
 * The four guards, and the two table bounds, checked by the path they select.
 *
 * WHAT THIS USED TO CHECK AND WHY IT NOW CHECKS MORE.  When these arms lived
 * in a file of their own, most of what a guard selects was unwritten, so the
 * only observable was WHICH unwritten path ran -- comparing objects was
 * pointless when our entry did nothing and the blob did the whole step.  The
 * unify closed that: table 3's forty arms are all written, and so are both
 * ends of the range test and the receiver-idle route.  So five of these
 * trials are held by a full object comparison
 * instead, and a range constant off by one moves the step to a different arm
 * rather than to a different code.  Finding F551.
 *
 * Table 2's last arm landed the same way (591) and took the sixth trial with
 * it: all five table windows are now written and held by the object
 * comparison rather than by a recorded code.
 *
 * Without this, `T3M_TBL3_COUNT`, `T3M_TBL3_FIRST`, `T3M_TBL2_COUNT`, the
 * cursor compare, the receiver-count compare and the +0xa8a0 gate are all
 * free: every trial above drives values that satisfy them either way.
 */
static void
guard(short mst, const struct seed *s, short rxstate, long tag)
{
	char what[96];

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(mst, rxstate, s->txstate);
	v34hs_poke_short(T3MT_COUNTER, s->counter);
	apply(s);

	v34hs_ours(1);
	v34hs_step();
	snprintf(what, sizeof(what), "guard: microstate %d, rxstate %d",
		 (int)mst, (int)rxstate);
	v34hs_compare(what, tag);
	v34hs_ours(0);
}

static void
guards(void)
{
	struct seed s;
	long tag = 5100;

	/*
	 * Table 3's window is 41..80, and one past each end is the default
	 * arm at 0x65329 -- the transmit dispatch -- rather than an entry of
	 * the table.  All four are written now, so what separates them is the
	 * object comparison `guard` makes: 40 and 81 take the block route and
	 * 41 and 80 take an arm of their own, and a window off by one at
	 * either end swaps one for the other.
	 */
	s = plain;
	guard(40, &s, V34HS_RX_DPSK, tag++);
	guard(81, &s, V34HS_RX_DPSK, tag++);
	guard(41, &s, V34HS_RX_DPSK, tag++);
	guard(80, &s, V34HS_RX_DPSK, tag++);

	/*
	 * Table 2's window is 5..74, and all seven of its targets are written
	 * now (591), so what separates the edge from the outside is the object
	 * comparison rather than an unwritten code -- 551's rule, that the
	 * CLAIM a trial made has to survive the path landing even though the
	 * constant it asserted cannot.
	 *
	 * 74 is the table's last entry and takes 0x644c9; 75 is one past it
	 * and reaches the tail with no arm at all; 20 is interior and takes
	 * 0x64509.  A window one short at the top swaps 74's arm for the bare
	 * tail, which is what makes `T3M_TBL2_COUNT` fail rather than free.
	 * ONE PAST THE TOP IS NOT OBSERVABLE and deliberately not asserted
	 * here: 75 inside the window would reach the switch's default and fall
	 * into the same tail it reaches from outside, with the same txstate.
	 */
	s = plain;
	s.txstate = 75;
	guard(V34HS_TX_PHASE3_ANS, &s, V34HS_RX_DPSK, tag++);
	s.txstate = 74;			/* 0x644c9, the table's last entry */
	guard(V34HS_TX_PHASE3_ANS, &s, V34HS_RX_DPSK, tag++);
	s.txstate = 20;			/* 0x64509, interior */
	guard(V34HS_TX_PHASE3_ANS, &s, V34HS_RX_DPSK, tag++);

	/*
	 * The rxstate chain: only RX_DPSK reaches table 3.
	 *
	 * NEITHER OF THESE TWO RECORDS ANYTHING ANY MORE, and both stay.  35
	 * WAIT was the first to change: its arm is 0x6752c, four instructions,
	 * and writing it turned this trial from a recorded code into a real
	 * object comparison.  4 RECEIVE is the second, for the same reason --
	 * 0x653e4 is `t_v34hsrx4.c`'s arm now.  Both stay rather than being
	 * deleted, because "this rxstate does not reach table 3" is the claim
	 * the lines were making and that claim is still true; what changed is
	 * only where each goes instead.  Findings F717, F549 and F731.
	 */
	s = plain;
	guard(V34HS_TX_PHASE3_ANS, &s, V34HS_RX_RECEIVE, tag++);
	guard(V34HS_TX_PHASE3_ANS, &s, V34HS_RX_WAIT, tag++);

	/*
	 * The receiver's first halfword.  `v34hs_route` leaves it at 6; at 5
	 * the once-per-block dispatch runs instead -- 0x629f1 is `jle 62ae3`
	 * and 0x62ae3 reads the transmit state for 0x62af1, so this is the
	 * BLOCK route and not an idle return (finding F549).  `<= 5` and `< 5`
	 * are the two readings and this is the trial that separates them: at
	 * 5 the step must do what the block dispatch does, and the object
	 * comparison is what says it did.
	 */
	s = plain;
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, T3MT_TXSTATE);
	v34hs_poke_short(0x0264, 5);
	v34hs_ours(1);
	v34hs_step();
	v34hs_compare("a receiver count of 5 takes the block route", tag++);
	v34hs_ours(0);

	/*
	 * The cursor against the limit.  Route RXCHAIN leaves both at zero,
	 * where `<` and `>` agree, so a trial with the cursor ABOVE the limit
	 * is what says the compare is the right way round -- it must still
	 * reach table 3.
	 */
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, T3MT_TXSTATE);
	v34hs_poke_short(0x221c, 5);
	v34hs_poke_short(0x2aa0, 0);
	v34hs_ours(1);
	v34hs_step();
	v34hs_compare("cursor above the limit still reaches table 3", tag++);
	v34hs_ours(0);

	/*
	 * And below it, which is table 1's per-sample loop.
	 *
	 * THIS TRIAL USED TO ASSERT A RECORDED CODE AND NOT STEP, on the
	 * grounds that the loop was #56's and its default arm does not
	 * terminate.  The loop and its nineteen arms are written now, so the
	 * claim inverts: this reaches the loop, runs it to its exit at
	 * 0x629ed and goes on into the rest of the function, and the step
	 * makes that a differential comparison rather than a claim about a
	 * guard.  It terminates because `T3MT_TXSTATE` is 18 SSEG, one of the
	 * nineteen, and an arm that transmits advances the cursor the loop
	 * test reads.  Finding F712.
	 */
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, T3MT_TXSTATE);
	v34hs_poke_short(0x221c, 0);
	v34hs_poke_short(0x2aa0, 5);
	v34hs_ours(1);
	v34hs_step();
	v34hs_compare("a cursor below the limit runs table 1's loop", tag++);
	v34hs_ours(0);

	/*
	 * The +0xa8a0 gate, which `v34hs_route` clears.
	 *
	 * THIS TRIAL USED TO ASSERT A RECORDED CODE AND NOT COMPARE.
	 * The arm at 0x6754b is written now, and it FALLS THROUGH -- all six
	 * of its exits are `jmp 0x64a8f`, so a non-zero gate polls the retrain
	 * detector and then runs exactly the code the cleared gate runs.  So
	 * what this checks is no longer which guard fired but that the whole
	 * step still agrees with the blob's, and `t_v34hsfsk.c` drives the arm
	 * itself.  Finding F721.
	 */
	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_TX_PHASE3_ANS, V34HS_RX_DPSK, T3MT_TXSTATE);
	v34hs_poke_int(0xa8a0, 1);
	v34hs_ours(1);
	v34hs_step();
	v34hs_compare("guard: a non-zero +0xa8a0 polls the retrain detector",
		      tag++);
	v34hs_ours(0);
}

int
main(void)
{
	dump = getenv("V34HS_DUMP") != NULL;
	diff_begin("v34handshak table 3, the middle group");

	/*
	 * The diagnostics are on throughout.  They are the cheapest
	 * discriminator the object offers, and two of these arms are
	 * distinguished from their neighbours by nothing else; `StateName` is
	 * indexed unbounded while they are on (D42), so every state word
	 * driven here stays inside 0..86.
	 */
	v34hs_debug(1);

	micro47();
	micro47_56();
	micro48();
	micro49();
	micro50();
	micro51();
	micro55();
	micro58();
	micro59();
	micro63();
	tail_paths();
	txblock_paths();
	guards();

	/*
	 * WHAT THE TRIALS REACHED.  Each of these is a branch of an arm, and
	 * a suite that never took one is a suite in which that branch's
	 * constants are free.  Findings F247 and F262 are two checks in this
	 * tree that could not be satisfied by any input and failed loudly;
	 * these can be, and the trials above are chosen so that they are.
	 */
	diff_eq_int("a below-threshold trial ran", saw_below, 1, 0);
	diff_eq_int("the 0x78 arm was taken", saw_toggle, 1, 0);
	diff_eq_int("the 0xa2 arm was taken", saw_force, 1, 0);
	diff_eq_int("a trial printed a transition", saw_trace, 1, 0);
	diff_eq_int("a trial ran the demodulator", saw_fsk, 1, 0);
	diff_eq_int("47's reset was taken", saw_reset47, 1, 0);
	diff_eq_int("47's TX_L1 arm was taken", saw_txl1, 1, 0);
	diff_eq_int("47 and 56 were compared", saw_collide, 1, 0);
	diff_eq_int("63's leave-at-once arm was taken", saw_63_leave, 1, 0);
	diff_eq_int("63's TONE_AB counter arm was taken", saw_63_tone, 1, 0);
	diff_eq_int("63's answering ending was taken", saw_63_ans, 1, 0);
	diff_eq_int("63's originating ending was taken", saw_63_orig, 1, 0);
	diff_eq_int("63's v34setuptxmit path was taken", saw_63_setup, 1, 0);
	diff_eq_int("49's reset was taken", saw_49_reset, 1, 0);
	diff_eq_int("49's reset was declined", saw_49_noreset, 1, 0);
	diff_eq_int("49 left below its second threshold", saw_49_leave, 1, 0);
	diff_eq_int("49's bulk-delay path was taken", saw_49_bulk, 1, 0);
	diff_eq_int("49's is_short formula was taken", saw_49_short, 1, 0);
	diff_eq_int("49's computed formula was taken", saw_49_long, 1, 0);
	diff_eq_int("49's clamp to one fired", saw_49_clamp, 1, 0);
	diff_eq_int("49's shift-register guard turned a step back",
		    saw_49_nofsk, 1, 0);
	diff_eq_int("50's reset was taken", saw_50_reset, 1, 0);
	diff_eq_int("50's reset was declined", saw_50_noreset, 1, 0);
	diff_eq_int("50 left below its second threshold", saw_50_leave, 1, 0);
	diff_eq_int("50's demodulator path was taken", saw_50_setup, 1, 0);
	diff_eq_int("50's shift-register guard turned a step back",
		    saw_50_nofsk, 1, 0);
	diff_eq_int("51 left off its threshold", saw_51_leave, 1, 0);
	diff_eq_int("51's detector body was taken", saw_51_detect, 1, 0);
	diff_eq_int("51's info1a body was taken", saw_51_info, 1, 0);
	diff_eq_int("51's 2743 substitution was taken", saw_51_2743, 1, 0);
	diff_eq_int("51's rate default was taken", saw_51_dflt, 1, 0);
	diff_eq_int("51 scaled a gain by four", saw_51_gain[0], 1, 0);
	diff_eq_int("51 scaled a gain by two", saw_51_gain[1], 1, 0);
	diff_eq_int("51 left a gain alone", saw_51_gain[2], 1, 0);
	diff_eq_int("59's reset was taken", saw_59_reset, 1, 0);
	diff_eq_int("59's reset was declined", saw_59_noreset, 1, 0);
	diff_eq_int("59 forced the txstate to SILENCE", saw_59_silence, 1, 0);
	diff_eq_int("59 found the txstate already SILENCE", saw_59_already5,
		    1, 0);
	diff_eq_int("59's retrain gate turned a step back", saw_59_gate, 1, 0);
	diff_eq_int("59's detector said no", saw_59_noretrain, 1, 0);
	diff_eq_int("59's retrain was taken", saw_59_retrain, 1, 0);
	diff_eq_int("59's bulk-delay path was taken", saw_59_deep, 1, 0);
	diff_eq_int("59 left below filtdelay+0x5c", saw_59_shallow, 1, 0);
	diff_eq_int("59's clamp to one fired", saw_59_clamp, 1, 0);
	diff_eq_int("59's second reset was taken", saw_59_arm2, 1, 0);
	diff_eq_int("55's reset was taken", saw_55_reset, 1, 0);
	diff_eq_int("55's reset was declined on the shift register",
		    saw_55_nosr, 1, 0);
	diff_eq_int("55's reset was declined because nbits did not move",
		    saw_55_nonbits, 1, 0);
	diff_eq_int("55 stepped below 0x5f", saw_55_below, 1, 0);
	diff_eq_int("55 stepped past 0x5f", saw_55_over, 1, 0);
	diff_eq_int("55's second reset was taken", saw_55_arm, 1, 0);
	diff_eq_int("55's second reset was declined", saw_55_noarm, 1, 0);
	diff_eq_int("55's two resets were shown exclusive", saw_55_exclusive,
		    1, 0);
	diff_eq_int("58's head cleared a record field", saw_58_head, 1, 0);
	diff_eq_int("58's head declined on a zero field", saw_58_nohead, 1, 0);
	diff_eq_int("58's head was skipped on another txstate", saw_58_headtx,
		    1, 0);
	diff_eq_int("58's deep path was taken", saw_58_deep, 1, 0);
	diff_eq_int("58's deep path was declined", saw_58_shallow, 1, 0);
	diff_eq_int("58 set 0x200 past its first threshold", saw_58_flag, 1, 0);
	diff_eq_int("58's second reset was taken", saw_58_armed, 1, 0);
	diff_eq_int("58's second reset was declined", saw_58_noarm, 1, 0);
	diff_eq_int("58's second reset called v34handshakinit", saw_58_init,
		    1, 0);
	diff_eq_int("58's second reset declined to call it", saw_58_noinit,
		    1, 0);
	diff_eq_int("58's first reset was taken", saw_58_reset, 1, 0);
	diff_eq_int("58's first reset was declined", saw_58_noreset, 1, 0);
	diff_eq_int("58's two resets were shown exclusive", saw_58_exclusive,
		    1, 0);

	/*
	 * And every pointer field `v34hs_compare` skips was exercised, so the
	 * thirty-five offsets this test trusts cannot go stale unnoticed.
	 */
	v34hs_holes_check();

	v34hs_ours(0);
	return diff_end();
}
