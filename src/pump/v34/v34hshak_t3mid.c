/*
 * v34hshak_t3mid.c -- `v34handshak`'s microstate arms 47/56, 48, 49, 50, 51,
 * 55, 58, 59 and 63, and the prologue, transmit dispatch and epilogue they
 * are reached through and leave through.
 *
 * THIS IS PART OF `v34hshak.c`, split off the way `V90PreFilter_loops.cpp` is
 * split off `V90PreFilter.cpp`.  `v34handshak` is 61,541 bytes and is being
 * reconstructed one dispatch arm at a time (docs/v34handshak.md), so it
 * cannot be one edit to one file; four batches are in flight at once and one
 * file each is what keeps them from colliding.  Everything file-local here is
 * spelled `T3M_*` or `t3m_*` for the same reason -- finding 325 is what two
 * batches sharing a macro name cost.
 *
 * WHAT IS AND IS NOT HERE.  The entry point is `v34handshak_t3mid`, not
 * `v34handshak`: this reconstructs the paths named above and NOTHING else, so
 * a function of the blob's name would be a claim about 61,541 bytes on the
 * strength of about 3,000.  `tools/coverage.py` credits `translated` by the
 * blob symbol's whole size, and it would credit all of it.  Every path not
 * written records itself through `v34handshak_t3mid_unwritten` and returns,
 * and `t_v34hst3mid.c` fails if any test reached one.  When the remaining
 * arms land, the assembled function takes the blob's name and this entry
 * disappears.
 *
 * THE THREE ADDRESSES EVERY COMMENT IS AGAINST are the blob's:
 *
 *      0x628f0   entry
 *      0x64a64   the rxstate == RX_DPSK arm, which reaches the microstate
 *                dispatch at 0x64ad2 through .rodata+0x3000
 *      0x62af1   the once-per-block transmit dispatch, .rodata+0x2ee8
 *      0x62a40   the tail every one of those arms falls into, and the only
 *                place this function returns from
 */

#include <stddef.h>

#include "dsplib/debug.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"

/*
 * ---------------------------------------------------------------------------
 * Offsets the header does not name.
 *
 * `struct v34_object` models about half of what these arms touch; the rest
 * lands in a `pad_*` or `unmapped_*` region, which `tools/whichfield.py`
 * reports as exactly that.  Reaching those by offset is this tree's existing
 * practice (src/pump/v34/v34pcmmain.cpp, src/pump/v34/v34k56.cpp) and is
 * preferred to widening the struct from one arm's reading of it.
 */
#define T3M_RECEIVER		0x0264	/* struct v34_receiver, and `V34agc`'s
					   argument; the compare at 0x629f1
					   reads its first halfword          */
#define T3M_TICK		0x0234	/* int, and NOT `unmapped_0234` as a
					   whole: 0x62aa9 increments this one
					   32-bit word and 0x62ab3 compares it
					   against 0x257f                    */
#define T3M_ELAPSED		0x0238	/* int, the running sample count       */
#define T3M_DEADLINE		0x023c	/* int, ELAPSED plus 431,488; the two
					   are compared UNSIGNED at 0x62a84  */
#define T3M_MODE		0x2218	/* int, the value 0x62a47 dispatches
					   the tail on                       */
#define T3M_TXCURSOR		0x221c	/* short, samples emitted this block   */
#define T3M_COUNTER		0xaa78	/* short, the counter six of these arms
					   bump; it is the `[2]` every trace
					   prints (v34hshak.c's HS_TRACE_2)  */
#define T3M_FSKGATE		0xa8a0	/* int, non-zero diverts at 0x64a87    */
#define T3M_TOGGLE		0x358c	/* short, arm 48 inverts bit 0 of it   */

/*
 * The five fields arms 47 and 63 reach that nothing in this tree has named.
 * `fNNNN` rather than a description, deliberately: what each is FOR is not
 * something this batch measured, and a guessed name in the record is worse
 * than an offset (docs/findings.md's rule about decompiler-shaped names).
 * What IS measured is in the comment beside each use.
 */
#define T3M_F3588		0x3588	/* short, arm 47's second entry guard;
					   `v34handshakinit` also writes it  */
#define T3M_F358A		0x358a	/* short, set to 1 beside it           */
#define T3M_FABAE		0xabae	/* ten shorts arm 47 clears            */
#define T3M_FABC2		0xabc2	/* the eleventh, cleared separately    */
#define T3M_FABFF		0xabff	/* SIGNED byte, arm 63's third guard   */
#define T3M_F0240		0x0240	/* int, arm 63's fourth guard          */

/*
 * The first of the five message records `v34hshak.c` names at +0xa94c ..
 * +0xaa3c -- twelve fields on a 0x30 stride, and `v34handshakinit`'s mode-2
 * body blanks the +0xaa0c one field for field.  Arm 47 fills this one.
 */
#define T3M_MSGREC0		0xa94c

/* Within the receiver. */
#define T3M_RX_FSKIN		0x010c	/* `fskdemodulate`'s second argument   */

/* The microstate table: 40 entries, index = microstate - 41. */
#define T3M_TBL3_FIRST		41
#define T3M_TBL3_COUNT		40

/* The once-per-block transmit table: 70 entries, index = txstate - 5. */
#define T3M_TBL2_FIRST		5
#define T3M_TBL2_COUNT		70

/*
 * ---------------------------------------------------------------------------
 * The paths not written.
 *
 * A reconstruction of nine arms out of forty has to do something definite
 * with the other thirty-one, and "nothing" is the one answer a differential
 * test cannot tell from a wrong answer -- the object would simply come back
 * unmodified and the comparison would report whatever the blob wrote.  So
 * every unwritten path names itself here and the test asserts nothing reached
 * one.  It is deliberately not an `abort()`: src/ has none, and a test that
 * dies has no offset in it.
 *
 * A CODE AND NOT A STRING, and that is the strings firewall's doing rather
 * than a preference: `tools/debugaudit.py --invented` holds every literal in
 * src/ against the object's .rodata and .data, so a diagnostic phrase this
 * tree made up cannot live here at all (findings 180 and 201).  The names are
 * in the test, which is where an invented string belongs.
 */
static int t3m_unwritten;

int
v34handshak_t3mid_unwritten(void)
{
	return t3m_unwritten;
}

void
v34handshak_t3mid_unwritten_reset(void)
{
	t3m_unwritten = T3M_WRITTEN;
}

static void
t3m_notwritten(int what)
{
	if (t3m_unwritten == T3M_WRITTEN)
		t3m_unwritten = what;
}

/*
 * ---------------------------------------------------------------------------
 * The four things the prologue computes once and every arm and the tail use.
 *
 * 0x628f0 puts them in four stack slots and they are live for the whole
 * function: 0x74(%esp) is the receiver, 0x78(%esp) is `&obj->f0004`, and
 * 0x4c(%esp) is `obj + 0x221c`.  `%esi` holds the microstate from 0x64abc.
 *
 * `0x78(%esp)` matters more than it looks.  Every "progress code" store in
 * the tail is `movl $n,(%esi)` through it, and every timer field the tail
 * reads is an offset from it -- so `0x234(%esi)` is the object's +0x238 and
 * not its +0x234.  Finding 179 quoted the register-relative offsets and was
 * wrong by four for exactly this reason; v34hshak.c's `v34handshakinit`
 * comment records it.
 */
struct t3m_frame {
	struct v34_object	*obj;
	unsigned char		*m;	/* the object as bytes             */
	struct v34_receiver	*rx;	/* obj + 0x264                     */
	int			*progress;	/* &obj->f0004             */
	short			mst;	/* the microstate the dispatch read*/
};

#define T3M_I32(f, off)		(*(int *)((f)->m + (off)))
#define T3M_U32(f, off)		(*(unsigned *)((f)->m + (off)))
#define T3M_I16(f, off)		(*(short *)((f)->m + (off)))
#define T3M_U16(f, off)		(*(unsigned short *)((f)->m + (off)))

/*
 * ---------------------------------------------------------------------------
 * 0x62a40 -- the tail, and the only `ret`.
 *
 * Eighty-eight instructions with no exit but its own four returns, which is
 * what makes a per-arm reconstruction possible at all: every microstate arm
 * and every transmit arm ends here, so writing it once covers all of them.
 *
 * `tx` IS A PARAMETER AND NOT `obj->txstate`.  The value in `%cx` arrives
 * from whichever arm jumped here and two comparisons read it -- 0x62a70 and
 * 0x62ac5 -- while `obj + 0x3596` may hold something else entirely.  Arm 48's
 * threshold path is the case that separates them: it stores 5 into the object
 * and passes 5, but 0x62b45 below RE-READS the object into `%cx`, so the two
 * readings differ on one path out of four and modelling either one as the
 * other passes at txstate 18 and fails at txstate 5.
 */
static void
t3m_tail(struct t3m_frame *f, short tx)
{
	int mode = T3M_I32(f, T3M_MODE);
	int esi;

	if (mode == 1) {
		/*
		 * 0x62b45.  `faa96` is the receive baud rate; the receiver's
		 * +0x1d2 gets three times it, by `lea (%ebp,%ebp,2)` on the
		 * sign-extended halfword and stored back as one.
		 */
		f->rx->f1d2 = (short)(3 * (int)f->obj->faa96);
		tx = (short)T3M_U16(f, V34HS_TXSTATE_OFF);
		*f->progress = 4;
	}

	/* 0x62a56 and 0x62a68: both windows are UNSIGNED, so mode 1 and any
	   negative mode fall through both. */
	if ((unsigned)(mode - 4) <= 1u)
		*f->progress = 6;

	if ((unsigned)(mode - 2) <= 1u && tx == 0x4a) {
		/*
		 * 0x64884.  `setg`/`dec`/`and $7` is 0 when `vect_idx` is
		 * above 60 and 7 when it is not -- the arithmetic is the
		 * compiler's way of writing a two-valued select and the 7 is
		 * not a mask of anything.
		 */
		*f->progress = f->obj->vect_idx > 0x3c ? 0 : 7;
	}

	/* 0x62a7a.  Unsigned, so an elapsed count past the deadline's wrap
	   is late rather than early. */
	if (T3M_U32(f, T3M_ELAPSED) > T3M_U32(f, T3M_DEADLINE))
		*f->progress = 8;

	/* 0x62a92.  Signed: the level is a sign-extended halfword. */
	esi = f->rx->agc_level;
	if (esi >= f->obj->rx_energy_floor)
		T3M_I32(f, T3M_TICK) = 0;
	else
		T3M_I32(f, T3M_TICK)++;

	if (T3M_I32(f, T3M_TICK) > 0x257f)
		*f->progress = 9;

	/* 0x62ac5, a three-way compare on the SIGN-EXTENDED halfword. */
	if (tx == 0x53)
		*f->progress = 0x0f;
	else if (tx > 0x53) {
		if (tx == 0x54)
			*f->progress = 0x10;
	} else if (tx == 0x52)
		*f->progress = 0x0d;
}

/*
 * ---------------------------------------------------------------------------
 * .rodata+0x2ee8 -- the once-per-block transmit dispatch at 0x62af1.
 *
 * Seven targets over seventy entries, and the three written here are the
 * three the nine microstate arms can reach: an arm either passes the object's
 * own txstate through or forces 5.
 *
 * THIS IS NOT A RECONSTRUCTION OF TABLE 2, which is another batch's (#56).
 * What is here is the three arms the microstate arms hand control to, written
 * because an arm that ends in the transmit dispatch cannot be compared
 * without them.  Whoever assembles the whole function should expect these
 * three to exist twice and should keep the other batch's.
 */
static void
t3m_txblock(struct t3m_frame *f, short tx)
{
	unsigned idx = (unsigned)((int)tx - T3M_TBL2_FIRST);

	if (idx >= T3M_TBL2_COUNT) {
		t3m_tail(f, tx);
		return;
	}

	switch ((int)tx) {
	case 5:				/* 0x64480 SILENCE */
		/*
		 * Three ways to reach progress 1, all of them a microstate
		 * and rxstate pair agreeing with the answer/originate flag
		 * at +0x359c, and every other way is 0.
		 */
		{
			unsigned short mst = T3M_U16(f, V34HS_MICROSTATE_OFF);
			unsigned short rx = T3M_U16(f, V34HS_RXSTATE_OFF);

			if (mst == 0x3f && f->obj->f359c == 0x66)
				*f->progress = 1;
			else if (rx == 0x04 && mst == 0x2c
				 && f->obj->f359c == 0x65)
				*f->progress = 1;
			else if (rx == 0x23 && mst == 0x3f
				 && f->obj->f359c == 0x65)
				*f->progress = 1;
			else
				*f->progress = 0;
		}
		break;

	case 18:			/* 0x64518 SSEG   */
	case 19:			/*          SBARSEG */
		/*
		 * `sbb`/`add $3` is 3 when bit 3 of the receiver's flags is
		 * set and 2 when it is not.  The originate side is 2 whatever
		 * the flag says.
		 */
		if (f->obj->f359c == 0x65)
			*f->progress = 2;
		else
			*f->progress = (f->rx->flags >> 3) & 1 ? 3 : 2;
		break;

	case 24:			/* 0x644c9 TX_DPSK  */
	case 51:			/*         TX_L1    */
	case 54:			/*         SILENCEINFO */
	case 60:			/*         TONE_AB  */
	case 74:			/*         SILENCERETRAIN */
		*f->progress = 0;
		break;

	default:
		t3m_notwritten(T3M_UNWRITTEN_TBL2_ARM);
		return;
	}

	t3m_tail(f, tx);
}

/*
 * ---------------------------------------------------------------------------
 * The arms.
 *
 * Each is `static void t3m_micro<n>(struct t3m_frame *)` and each ends by
 * calling `t3m_txblock` with the value the object's `jmp 62af1` leaves in
 * `%cx`.  Nothing else may end an arm: the tail is the only `ret`.
 */

/*
 * 0x65d30 -- microstate 48 `TX_PHASE3_ANS`.
 *
 * A counter with two thresholds and nothing else, which is the shape finding
 * 288 gives to six of table 3's arms.  The counter at +0xaa78 is incremented
 * as an UNSIGNED halfword (`movzwl`/`inc`/`cmp %ax`), stored back before
 * either threshold is tested, and the two thresholds are exact equalities --
 * not "at least", so a counter that steps past 0x78 without landing on it
 * never takes that arm again until it wraps.
 *
 *      0x78    invert bit 0 of +0x358c, and leave with the object's txstate
 *      0xa2    force txstate to SILENCE and microstate to RX_PHASE2_ANS
 *              (0x32), clear the
 *              counter, and leave with 5 -- both forcings announce
 *              themselves, and both are `hs_setstate`'s compare-print-store
 *      else    leave with the object's txstate
 */
static void
t3m_micro48(struct t3m_frame *f)
{
	unsigned short n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);

	T3M_U16(f, T3M_COUNTER) = n;

	if (n == 0x78) {
		/* 0x6c6e4.  An XOR of bit 0, not a store of a constant. */
		T3M_U16(f, T3M_TOGGLE) ^= 1;
		t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
		return;
	}

	if (n == 0xa2) {
		/* 0x6c670. */
		hs_setstate(f->obj, V34HS_TXSTATE_OFF, V34HS_SILENCE);
		hs_setstate(f->obj, V34HS_MICROSTATE_OFF, V34HS_RX_PHASE2_ANS);
		T3M_U16(f, T3M_COUNTER) = 0;
		t3m_txblock(f, V34HS_SILENCE);
		return;
	}

	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * 0x66834 -- microstates 47 `TX_PHASE2_ANS` AND 56 `TX_PHASE2_CALL`.
 *
 * ONE ARM FOR TWO STATES.  .rodata+0x3000's entries 6 and 15 hold the same
 * address, so the answer side and the originate side of phase 2 run the same
 * code and the arm never looks at which of the two it was entered with.  The
 * test asserts that rather than assuming it.
 *
 * The body is the counter shape of finding 288 with a reset in front of it:
 *
 *      the FSK shift register's low four bits are all ones AND +0x3588 is
 *      still zero  ->  re-arm the error recovery, then fall into the counter
 *      counter+1 <= 0x5f    store it and leave with the object's txstate
 *      otherwise            clear the counter, invert bit 0 of +0x358c, move
 *                           the microstate to TX_L1 and leave
 *
 * BOTH COMPARES ARE SIGNED 16-BIT (`cmp $0x5f,%ax` after a `movzwl`/`inc`),
 * so a counter of 0x7fff steps to -32768 and takes the low arm, and one of
 * 0xffff steps to 0 and takes it as well.
 *
 * THE RESET FALLS THROUGH rather than returning: it clears the counter as its
 * last act and jumps back to 0x6684e, so a step that takes it always leaves
 * with the counter at 1.  That is why the reset cannot be tested by the
 * counter alone.
 */
static void
t3m_micro47(struct t3m_frame *f)
{
	unsigned short n;

	/*
	 * 0x6683b and 0x6cc1b.  `obj->fsk.sr` is the demodulator's shift
	 * register and `nbits` its bit count, and the two are reset together
	 * below -- which is what "repeated info0" means here.  The second
	 * test is what stops it running on every block: +0x3588 becomes 4.
	 */
	if ((f->obj->fsk.sr & 0xf) == 0xf && T3M_I16(f, T3M_F3588) == 0) {
		unsigned char *r = f->m + T3M_MSGREC0;
		int k;

		T3M_I16(f, T3M_F3588) = 4;
		f->obj->is_short = 0;
		f->obj->local_short = 0;
		T3M_I16(f, T3M_F358A) = 1;

		/* 0x6cc55, ten shorts, and the eleventh is not in the loop. */
		for (k = 0; k <= 9; k++)
			T3M_I16(f, T3M_FABAE + 2 * k) = 0;
		T3M_I16(f, T3M_FABC2) = 0;

		/*
		 * Both transitions, and they are announced BEFORE the counter
		 * is cleared -- the counter is `[2]` in each of the two format
		 * strings, so clearing it first would print two different
		 * lines and change no byte of the object.
		 */
		hs_setstate(f->obj, V34HS_TXSTATE_OFF, V34HS_TX_DPSK);
		hs_setstate(f->obj, V34HS_MICROSTATE_OFF, V34HS_DET_SYNC);

		f->obj->fsk.sr = -1;
		f->obj->fsk.nbits = 0;

		/*
		 * 0x6cdc4 and 0x7094b.  The record is filled with one constant
		 * per field, and only +0x18 depends on anything: 0x1e when
		 * this end originates AND a V.90 receiver is running, 0x11
		 * otherwise.  The originate test alone is not enough -- with
		 * no V.90 receiver the object jumps back into the 0x11 block.
		 */
		*(short *)(r + 0x14) = -1;
		*(short *)(r + 0x16) = 1;
		*(short *)(r + 0x18) =
			(f->obj->f359c == 0x65 && f->obj->v90_receiver != 0)
			? 0x1e : 0x11;
		*(short *)(r + 0x1a) = 0;
		*(short *)(r + 0x1c) = 8;
		*(short *)(r + 0x1e) = 0;
		*(short *)(r + 0x20) = 1;
		*(short *)(r + 0x22) = 0;
		*(int *)(r + 0x24) = 0xf72;
		*(short *)(r + 0x28) = 0xc;
		*(short *)(r + 0x2a) = 0xc;
		*(int *)(r + 0x2c) = 0xf72;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Repeated info0 is detected, "
					     "errorrecovery is initialized in "
					     "TX_PHASE2_xxx\n");

		T3M_U16(f, T3M_COUNTER) = 0;
	}

	/* 0x6684e. */
	n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);

	if ((short)n <= 0x5f) {
		/* 0x6aaf9. */
		T3M_U16(f, T3M_COUNTER) = n;
		t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
		return;
	}

	/*
	 * 0x66867.  The counter is cleared FIRST, so the transition below
	 * prints `[2]` as 0 and not as the value that crossed the threshold;
	 * the object hands the printf a literal zero, which is how it shows.
	 */
	T3M_U16(f, T3M_COUNTER) = 0;
	T3M_U16(f, T3M_TOGGLE) ^= 1;
	hs_setstate(f->obj, V34HS_MICROSTATE_OFF, V34HS_TX_L1);
	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * 0x6591e -- microstate 63 `INFODONE`.
 *
 * THE TXSTATE IS THE SELECTOR, not a companion: three bodies chosen by the
 * object's own +0x3596, and every other txstate is the three-instruction
 * shared arm -- read the txstate, jump to the transmit dispatch.  That is why
 * this arm is indistinguishable from twenty-four others when it is entered
 * cold at SSEG.
 *
 *      TONE_AB   a counter with one threshold, and two different endings on
 *                the answer/originate flag
 *      SILENCE   three more guards and then `v34setuptxmit`
 *      anything  leave with it
 */
static void
t3m_micro63(struct t3m_frame *f)
{
	short tx = (short)T3M_U16(f, V34HS_TXSTATE_OFF);
	unsigned short n;

	if (tx == V34HS_TONE_AB) {			/* 0x6d160 */
		n = (unsigned short)(T3M_U16(f, T3M_COUNTER) + 1);

		if ((short)n <= 0x0f) {
			/* 0x6d230, and %ecx still holds TONE_AB. */
			T3M_U16(f, T3M_COUNTER) = n;
			t3m_txblock(f, tx);
			return;
		}

		/*
		 * The incremented counter is stored before either ending, and
		 * that is not bookkeeping: it is `[2]` in the txstate
		 * transition printed next, and the originate ending keeps it.
		 */
		T3M_U16(f, T3M_COUNTER) = n;

		if (f->obj->f359c == 0x65) {		/* 0x6de96 */
			hs_setstate(f->obj, V34HS_TXSTATE_OFF, V34HS_SILENCE);
			hs_setstate(f->obj, V34HS_MICROSTATE_OFF,
				    V34HS_DET_SYNC);
			f->obj->fsk.nbits = 0;
			t3m_txblock(f,
				    (short)T3M_U16(f, V34HS_TXSTATE_OFF));
			return;
		}

		/*
		 * 0x6d187.  The answer side moves the txstate and REPLACES the
		 * counter, with one of two constants on `ptc`; it does not
		 * touch the microstate, so 63 is entered again next block.
		 */
		hs_setstate(f->obj, V34HS_TXSTATE_OFF, V34HS_SILENCE);
		T3M_U16(f, T3M_COUNTER) =
			f->obj->ptc == 0x30 ? 0x1e : 0x96;
		t3m_txblock(f, V34HS_SILENCE);
		return;
	}

	if (tx != V34HS_SILENCE) {			/* 0x6593a */
		t3m_txblock(f, tx);
		return;
	}

	/* 0x65947, a SIGNED byte, and 0x65958, a SIGNED int against 0x240. */
	if ((signed char)f->m[T3M_FABFF] <= 0
	    || T3M_I32(f, T3M_F0240) <= 0x240) {
		t3m_txblock(f, tx);
		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34Hshak: Setting up transmitter for "
				     "phase3...\r\n");

	/*
	 * 0x6597d..0x65b6d is `v34setuptxmit` inlined -- the same six steps in
	 * the same order, down to the two transitions and the tail call to
	 * `txinit`.  Calling it keeps one copy of the sequence that
	 * `t_v34hshak.c` already sweeps over every rate and carrier.
	 */
	v34setuptxmit(f->obj);
	t3m_txblock(f, (short)T3M_U16(f, V34HS_TXSTATE_OFF));
}

/*
 * ---------------------------------------------------------------------------
 * 0x64ad2 -- the microstate dispatch, .rodata+0x3000.
 */
static void
t3m_table3(struct t3m_frame *f)
{
	unsigned idx = (unsigned)((int)f->mst - T3M_TBL3_FIRST);

	if (idx >= T3M_TBL3_COUNT) {
		/* 0x65329, the default arm: another batch's. */
		t3m_notwritten(T3M_UNWRITTEN_TBL3_DEFAULT);
		return;
	}

	switch ((int)f->mst) {
	case V34HS_TX_PHASE2_ANS:	/* 47, and 56 is the same address */
	case V34HS_TX_PHASE2_CALL:	/* 56 */
		t3m_micro47(f);
		break;
	case V34HS_TX_PHASE3_ANS:	/* 48 */
		t3m_micro48(f);
		break;
	case V34HS_INFODONE:		/* 63 */
		t3m_micro63(f);
		break;
	default:
		t3m_notwritten(T3M_UNWRITTEN_TBL3_ARM);
		break;
	}
}

/*
 * ---------------------------------------------------------------------------
 * 0x628f0 -- the entry, and the four guards that choose a dispatch.
 */
void
v34handshak_t3mid(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)objp;
	struct t3m_frame frame;

	frame.obj = obj;
	frame.m = m;
	frame.rx = (struct v34_receiver *)(m + T3M_RECEIVER);
	frame.progress = &obj->f0004;
	frame.mst = 0;

	/*
	 * 0x62933.  The cursor against the limit, both signed halfwords.
	 * Below it is the per-sample transmit loop, which is #56 and not
	 * here -- and which does not terminate for most txstates (D59), so
	 * this returns rather than looping.
	 */
	if (T3M_I16(&frame, T3M_TXCURSOR) < obj->f2aa0) {
		t3m_notwritten(T3M_UNWRITTEN_TBL1);
		return;
	}

	/* 0x629f1.  The receiver's first halfword, signed. */
	if (*(short *)frame.rx <= 5) {
		t3m_notwritten(T3M_UNWRITTEN_RXIDLE);
		return;
	}

	/* 0x62a02, the rxstate compare chain.  It has no table. */
	if (hs_get(obj, V34HS_RXSTATE_OFF) != V34HS_RX_DPSK) {
		t3m_notwritten(T3M_UNWRITTEN_RXSTATE);
		return;
	}

	/*
	 * 0x64a64.  `V34agc` and `fskdemodulate` first, and only then is the
	 * microstate read -- which is what lets a test write +0x3592 and step
	 * once: neither of the two writes any of the three state words,
	 * swept over the whole of .text (finding 285).
	 */
	V34agc(frame.rx);

	if (T3M_I32(&frame, T3M_FSKGATE) != 0) {
		t3m_notwritten(T3M_UNWRITTEN_FSKGATE);
		return;
	}

	fskdemodulate(obj, (const short *)((unsigned char *)frame.rx
					   + T3M_RX_FSKIN), &obj->fsk);

	frame.mst = hs_get(obj, V34HS_MICROSTATE_OFF);
	t3m_table3(&frame);
}
