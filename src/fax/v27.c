/*
 * v27.c -- ITU-T V.27ter (fax): the receiver's primitives, and the
 * transmitter's status filler.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V27RX_delete         .text 0x099f10  193
 *   V27RX_eq_train       .text 0x09a110  255
 *   V27TX_delete         .text 0x09a7c0  107
 *   V27RX_decision       .text 0x09a210  284
 *   V27RX_modem          .text 0x0a2c60  127
 *   V27RX_status         .text 0x0a3320   11
 *   V27TX_status         .text 0x0a3ed0  118
 *   DemodDataV27         .text 0x0a5950  331
 *   DescrambleDataV27    .text 0x0a5aa0   28
 *   CarrierDetectV27     .text 0x0a5ac0   22
 *   DataCarrierDetectV27 .text 0x0a5ae0  579
 *   QualityDetectV27     .text 0x0a5d30  266
 *   EpochDetectV27       .text 0x0a5e40   22
 *   GetSNRV27            .text 0x0a5e60    6
 *   ScrambleDataV27      .text 0x0a5e70   28
 *   ModDataV27           .text 0x0a5ef0   89
 *
 * `tools/tumap.py` brackets these across `class1tx.c +94` and `class1.c`, so
 * it cannot say which translation units they are; they are kept in one file
 * because they are one layer -- everything V.27ter's datapump wrapper reaches
 * that is not the state machine itself -- and not because a translation unit
 * has been established.  `include/dsplib/v27fax.h` carries the offset
 * evidence.
 *
 * ---------------------------------------------------------------------------
 * THE AGC'S RETURN VALUE, WHICH IS NOT ONE
 *
 * `DemodDataV27` calls `FPM_AGC_agc`, passes it a FOURTH argument (the literal
 * 1) that it does not have, and then USES `%eax`.  `FPM_AGC_agc` is `void` --
 * `include/dsplib/fpm_agc.h` says so and the object's own frame reads confirm
 * it -- so the calling translation unit declared it as returning `int` while
 * the defining one returned nothing, and `%eax` holds whatever the definition
 * left there.
 *
 * WHAT IT LEAVES THERE IS `agc->signal`: the two instructions before its only
 * `ret` are `movzbl %dl,%eax` / `mov %eax,0x1c(%edi)`, the store to `signal`
 * itself.  So this file READS THE FIELD, which it can spell without a second
 * prototype disagreeing with `fpm_agc.h`, and `t_v27fax.c` MEASURES the
 * identity rather than believing it -- it declares `ref_FPM_AGC_agc` as
 * returning `int` and asserts the return equals `agc.signal` on every trial.
 * This is the fourth site with that shape; findings F8875 and F9116,
 * deviations D1035 and D1094.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE FOUR READING FUNCTIONS SHARE
 *
 * All of them start `rx = *(void **)(modem + 0x54)` and then read a field of
 * one of the four FPM modules embedded in that block.  Three flags account
 * for nearly every one of them:
 *
 *   fpm_agc::signal   more than half the last call's blocks were above the
 *                     gate.  `CarrierDetectV27`, `QualityDetectV27` and
 *                     `DataCarrierDetectV27` all AND it with
 *   fpm_sre::active   the symbol recovery's own squelch let the PLL run, and
 *   fpm_fse::mse      the equaliser's smoothed squared decision error, which
 *                     is what "Decoder error too big" is about.
 *
 * So "carrier" here means the level gate and the timing loop agree, and
 * "quality" means the equaliser is not struggling.  Neither is inferred from
 * a name: the fields are `fpm_agc.h`'s, `fpm_sre.h`'s and `fpm_fse.h`'s, and
 * the offsets land on them because the four modules tile the block with no
 * gap -- see the arithmetic in `v27fax.h`.
 *
 * ---------------------------------------------------------------------------
 * THE THREE `FPM_*_free` CALLS TAKE A SECOND ARGUMENT THEY DO NOT HAVE
 *
 * `V27RX_delete` stores the constant 1 at 0x4(%esp) before each of
 * `FPM_FSE_free`, `FPM_SRE_free` and `FPM_MRF_free`, and none of the three
 * reads it -- the same extra argument `v22data.c` and `bwchdem.c` record at
 * their `FPM_AGC_agc` sites.  cdecl makes it harmless and it is not
 * reproduced.  Finding F8870.
 */

#include "dsplib/v27fax.h"

#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/faxfifo.h"
#include "dsplib/sdmv27.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"

/* The instance is not modelled; see v27fax.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_S(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_US(obj, off)	(*(unsigned short *)(void *)FIELD((obj), (off)))
#define FIELD_I(obj, off)	(*(int *)(void *)FIELD((obj), (off)))

#define RX_AGC(rx)	((struct fpm_agc *)(void *)FIELD((rx), V27RX_AGC))
#define RX_MRF(rx)	((struct fpm_mrf *)(void *)FIELD((rx), V27RX_MRF))
#define RX_SRE(rx)	((struct fpm_sre *)(void *)FIELD((rx), V27RX_SRE))
#define RX_FSE(rx)	((struct fpm_fse *)(void *)FIELD((rx), V27RX_FSE))

/* ------------------------------------------------------------------ */

/*
 * Release everything the receiver owns, in the object's order.
 *
 * The instance pointer is re-read before every call rather than kept in a
 * local; that is what the object encodes and it is not observable, because
 * nothing on this path writes the instance.
 */
void
V27RX_delete(void *modem)
{
	void *rx;
	void *sh;

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	FPM_FSE_free(RX_FSE(rx));

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	FPM_SRE_free(RX_SRE(rx));

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	FPM_MRF_free(RX_MRF(rx));

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	sysdep_free(FIELD_PTR(rx, V27RX_BUF_B));

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	sysdep_free(FIELD_PTR(rx, V27RX_BUF_A));

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	sysdep_free(rx);

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(sh, V27SH_MTD));

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	sysdep_free(FIELD_PTR(sh, V27SH_BUF));

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(sh, V27SH_MTD_V21));

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	sysdep_free(sh);

	sysdep_free(modem);
}

/* ------------------------------------------------------------------ */

/*
 * Release the transmitter, in the object's order.
 *
 * The instance pointer is kept in a register across all seven calls -- it is
 * the only thing in this function that is not re-read -- while both BLOCK
 * pointers are loaded afresh before each use, exactly as `V27RX_delete` does.
 * Neither is observable, because nothing on this path writes the instance.
 *
 * `FPM_PPS_free` is given a second argument the object does not declare; see
 * `v27fax.h` and F8870.  Not reproduced.
 */
void
V27TX_delete(void *modem)
{
	void *tx;
	void *src;

	tx = FIELD_PTR(modem, V27_OBJ_TX);
	FPM_PPS_free((struct fpm_pps *)(void *)FIELD(tx, V27TX_PPS));

	/*
	 * The SYMBOL RING's buffer, not a scratch allocation of the
	 * transmitter's.  The object frees `*(tx + 0x10)`, and 0x10 is
	 * `V27TX_RING + offsetof(struct fpm_smc_ring, sym)`; see v27fax.h and
	 * F9121 for why there is no room for a separate field there.
	 */
	tx = FIELD_PTR(modem, V27_OBJ_TX);
	sysdep_free(((struct fpm_smc_ring *)(void *)
			FIELD(tx, V27TX_RING))->sym);

	tx = FIELD_PTR(modem, V27_OBJ_TX);
	sysdep_free(tx);

	src = FIELD_PTR(modem, V27_OBJ_TXDATA);
	FIFO_delete((struct fax_fifo *)FIELD_PTR(src, V27TXD_FIFO));

	src = FIELD_PTR(modem, V27_OBJ_TXDATA);
	SGD_delete((struct sgd *)FIELD_PTR(src, V27TXD_SGD));

	src = FIELD_PTR(modem, V27_OBJ_TXDATA);
	sysdep_free(src);

	sysdep_free(modem);
}

/* ------------------------------------------------------------------ */

/*
 * The equaliser's TRAINING slicer, and the handover to the running one.
 *
 * The decision it takes is a binary one: the training symbol alternates
 * between two constellation points half a revolution apart, so the only
 * question is whether the measured angle has crossed to the other side.  The
 * phase difference is folded into [-V27DEC_HALF_TURN, +V27DEC_HALF_TURN] --
 * NOT into [0, V27DEC_PHASE_FULL] as `V27RX_decision` folds it -- and the
 * reference index is advanced by half the constellation when what is left
 * exceeds a quarter of a revolution.
 *
 * THE FOLD'S TWO TESTS ARE ASYMMETRIC AND THAT IS THE OBJECT'S: the high side
 * is `> 0x4000` and the low side is `< -0x4000`, so exactly +0x4000 folds and
 * exactly -0x4000 does not.  Both comparisons are 16-bit and signed
 * (`cmp $0x4000,%dx` / `jle`, `cmp $0xc000,%dx` / `jge`), which is why `diff`
 * is a `short` and not an `int`; a 32-bit fold would not wrap the same way.
 *
 * THE EMPTY LOOP IS THE OBJECT'S TOO, AND WHAT THE AUTHOR PUT IN IT CANNOT BE
 * RECOVERED.  At 0x9a1ce the object loads `state->cfg.taps`, runs a counted
 * loop with a `short` induction variable (`inc %eax` then `cwtl`) and NO body
 * at all, and falls through.  Whatever was written there produced no
 * instructions, so there is no preimage to derive: the loop is reproduced for
 * its control flow and it is not observable.  Finding F9117.
 *
 * `mu_sel` IS CLEARED ON EVERY CALL and set to 1 only on the handover, so the
 * equaliser trains on `cfg.mu[0]` and runs on `cfg.mu[1]`.
 *
 * THE COUNTER IS RE-READ FROM MEMORY for the limit test rather than reused
 * from the increment (`mov %dx,0x1c(%ecx)` ... `cmp %di,0x1c(%ecx)`), and that
 * is forced: `state->mu_sel` is a `short` and the counter is an
 * `unsigned short`, so the store between them may alias and the compiler has
 * to reload.  Written as a re-read for that reason.
 */
unsigned short
V27RX_eq_train(struct fpm_fse *state, short *angle, short *mag)
{
	void *dec = state->cfg.owner;
	const short *tbl;
	unsigned short count;
	short step;
	short diff;
	short err;
	short a;
	short limit;
	short i;

	/* Saturating, and it restarts at half scale -- V27RX_decision's. */
	count = (unsigned short)(FIELD_US(dec, V27DEC_SYM_COUNT) + 1);
	if (count == V27DEC_PHASE_FULL)
		FIELD_US(dec, V27DEC_SYM_COUNT) = V27DEC_PHASE_FULL / 2;
	else
		FIELD_US(dec, V27DEC_SYM_COUNT) = count;

	/* Half the constellation: 4 of 8, or 2 of 4. */
	step = FIELD_I(dec, V27DEC_EIGHT_PHASE) ? 4 : 2;

	diff = (short)(*angle - FIELD_S(dec, V27DEC_ANGLE_PREV));
	if (diff > V27DEC_HALF_TURN)
		diff = (short)(diff + V27DEC_PHASE_FULL);
	if (diff < -V27DEC_HALF_TURN)
		diff = (short)(diff - V27DEC_PHASE_FULL);

	*mag = V27DEC_MAG;

	err = diff < 0 ? (short)-diff : diff;
	if (err > V27DEC_QUARTER_TURN)
		FIELD_S(dec, V27DEC_LAST) =
			(short)((FIELD_S(dec, V27DEC_LAST) + step)
				& FIELD_US(dec, V27DEC_PHASE_MASK));

	tbl = (const short *)FIELD_PTR(dec, V27DEC_ANGLES);
	a = tbl[FIELD_S(dec, V27DEC_LAST)];
	limit = FIELD_I(dec, V27DEC_TRAIN_SHORT) ? V27DEC_TRAIN_SYMS_SHORT
						 : V27DEC_TRAIN_SYMS_LONG;
	*angle = a;
	FIELD_S(dec, V27DEC_ANGLE_PREV) = a;
	FIELD_US(dec, V27DEC_TRAIN_COUNT) =
		(unsigned short)(FIELD_US(dec, V27DEC_TRAIN_COUNT) + 1);

	state->mu_sel = 0;

	if (FIELD_S(dec, V27DEC_TRAIN_COUNT) >= limit) {
		for (i = 0; i < state->cfg.taps; i = (short)(i + 1)) {
			/* No body in the object.  See the note above. */
		}
		state->lms_force = 0;
		state->mu_sel = 1;
		state->cfg.decision = V27RX_decision;
	}

	return 0xffff;
}

/* ------------------------------------------------------------------ */

/*
 * The slicer.
 *
 * THE INITIAL `best` IS THE SAME EXPRESSION AS THE LOOP'S, applied to a
 * virtual phase of half a revolution -- which is the largest distance any
 * legal phase can be at.  The object writes it out with the constant folded:
 * `lea 0x8000(%ebx),%eax` where the arithmetic says subtract, and
 * `mov $0xffff8000,%eax; sub %ebx,%eax` where it says 0x8000 - diff.  Both
 * are correct because the result is immediately narrowed to `short` and
 * 0x8000 is its own negation modulo 2^16, so the compiler was free to choose
 * either sign of the constant.  It is written here the way the arithmetic
 * reads.
 *
 * THE DISTANCE IS A `short` AND THAT IS FORCED: the object narrows every one
 * of them with `cwtl` before the comparison, so a table entry far enough from
 * `diff` wraps and compares as its own opposite.  A `short` local is the only
 * spelling that reproduces it, and `t_v27fax` reaches the wrap deliberately.
 *
 * The two extensions on the table load -- `movzwl` for the subtraction and
 * `movswl` for the comparison, from the same 16 bits -- are finding F614's
 * free case: the subtraction's result is narrowed, so only its low half is
 * ever read.  The COMPARISON is the one that is forced, and it is signed.
 */
unsigned short
V27RX_decision(struct fpm_fse *state, short *angle, short *mag)
{
	void *dec = state->cfg.owner;
	const short *tbl = (const short *)FIELD_PTR(dec, V27DEC_ANGLES);
	const short *pmap = (const short *)FIELD_PTR(dec, V27DEC_PMAP);
	short n = FIELD_I(dec, V27DEC_EIGHT_PHASE) ? 8 : 4;
	unsigned short count;
	short best, bi, k;
	int diff;

	diff = *angle - tbl[FIELD_S(dec, V27DEC_LAST)];

	/* Saturating, and it restarts at half scale rather than at zero. */
	count = (unsigned short)(FIELD_US(dec, V27DEC_SYM_COUNT) + 1);
	if (count == V27DEC_PHASE_FULL)
		FIELD_US(dec, V27DEC_SYM_COUNT) = V27DEC_PHASE_FULL / 2;
	else
		FIELD_US(dec, V27DEC_SYM_COUNT) = count;

	/* One revolution, taken as [0, V27DEC_PHASE_FULL]. */
	if (diff < 0)
		diff += V27DEC_PHASE_FULL;
	if (diff > V27DEC_PHASE_FULL)
		diff -= V27DEC_PHASE_FULL;

	best = diff >= V27DEC_PHASE_FULL
	     ? (short)(diff - V27DEC_PHASE_FULL)
	     : (short)(V27DEC_PHASE_FULL - diff);
	bi = 0;

	for (k = 0; k < n; k = (short)(k + 1)) {
		short d;

		d = diff >= tbl[k] ? (short)(diff - tbl[k])
				   : (short)(tbl[k] - diff);
		if (d < best) {
			bi = k;
			best = d;
		}
	}

	FIELD_S(dec, V27DEC_LAST) = (short)((FIELD_S(dec, V27DEC_LAST) + bi)
					    & FIELD_US(dec, V27DEC_PHASE_MASK));
	*mag = V27DEC_MAG;
	*angle = tbl[FIELD_S(dec, V27DEC_LAST)];

	return (unsigned short)pmap[bi];
}

/* ------------------------------------------------------------------ */

/*
 * Drive the half-duplex receive state handler until it stops asking for more.
 *
 * A DO-WHILE, not a while: the handler is entered once even when the caller
 * offers no samples at all, which is how a state that only has to emit gets
 * to run.  The accumulated output count is a `short` -- `add %edx,%eax` then
 * `cwtl` on every iteration -- and it is written back over the caller's input
 * count.
 */
int
V27RX_modem(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short total = 0;

	*FIELD(modem, V27_OBJ_STATUS_FLAGS) &=
			(unsigned char)~(unsigned char)V27_STATUS_FLAG_02;

	n = *count;
	do {
		short before = (short)n;
		short got;

		got = (*(v27_rx_state_fn *)(void *)
			FIELD(FIELD_PTR(modem, V27_OBJ_SHARED), V27SH_STATE))
				(modem, in, out, count);
		n = *count;
		out += got;
		/*
		 * The samples the handler took.  `before` is signed and `n` is
		 * not, which the object states by extending the same sixteen
		 * bits two different ways -- `movswl %cx` at the top of the
		 * loop and `movzwl %cx` here.  Both are used at full width by
		 * the subtraction, so neither is free.
		 */
		in += before - n;
		total = (short)(total + got);
	} while (n != 0);

	*count = (unsigned short)total;

	return FIELD_I(modem, V27_OBJ_STATUS);
}

/* ------------------------------------------------------------------ */

/*
 * Report whether the caller supplied a receive status block.
 *
 * The block is not filled: eleven bytes, and the first argument is not read.
 */
int
V27RX_status(void *rx, void *status)
{
	(void)rx;
	return status != 0;
}

/*
 * Fill the caller's status block from the transmitter's.
 *
 * THE TWO WRITES TO +0x14 ARE BOTH THE OBJECT'S, and the first is not dead.
 * The second reads the SOURCE's +0x10, and nothing tells the compiler the two
 * blocks do not overlap -- so the store has to happen first, and it is
 * observable exactly when they do.  See D1033.
 */
int
V27TX_status(const void *tx, void *status)
{
	unsigned char flags;

	if (status == 0)
		return 0;

	FIELD_US(status, V27STAT_PROTOCOL) = FIELD_US(tx, V27STAT_PROTOCOL);
	FIELD_US(status, V27STAT_TX_BPS) = FIELD_US(tx, V27STAT_TX_BPS);
	FIELD_US(status, V27STAT_RX_BPS) = 0;
	FIELD_US(status, V27STAT_QUALITY) = 0;
	FIELD_US(status, V27STAT_ZERO_08) = 0;
	FIELD_US(status, V27STAT_ZERO_0A) = 0;
	FIELD_US(status, V27STAT_ZERO_0C) = 0;
	/*
	 * The SOURCE IS READ AGAIN, not reused: `movzwl 0x2(%ebx),%eax` at
	 * a3f0b after the store at a3efb.  Observable only if the two blocks
	 * overlap, and what the compiler was forced to encode.
	 */
	FIELD_US(status, V27STAT_WORD_10) = FIELD_US(tx, V27STAT_TX_BPS);
	FIELD_US(status, V27STAT_ZERO_12) = 0;

	/*
	 * V.17, V.21 and V.29 spell this `flags &= ~(BIT0 | BIT1)`.  V.27ter
	 * SETS bit 0 instead of clearing it, which changes what the last line
	 * of the function produces.  Finding F8866, deviation D1033.
	 */
	flags = (unsigned char)(*FIELD(status, V27STAT_FLAGS)
				| V27STAT_FLAGS_BIT0);
	*FIELD(status, V27STAT_FLAGS) =
			(unsigned char)(flags & (unsigned char)~V27STAT_FLAGS_BIT1);
	*FIELD(status, V27STAT_FLAGS2) &= (unsigned char)~V27STAT_FLAGS2_BIT0;
	*FIELD(status, V27STAT_FLAGS) =
			(unsigned char)((flags & V27STAT_FLAGS_BIT0)
					| (*FIELD(tx, V27TX_HANDLE_FLAGS)
					   & V27STAT_FLAGS_FROM_TX));

	FIELD_I(status, V27STAT_WORD_18) = FIELD_I(tx, V27STAT_WORD_18);

	return 1;
}

/* ------------------------------------------------------------------ */

/*
 * One block through the receive chain.
 *
 * THE TONE TEST HAS NO COPY AND NO NOTCH, WHERE V.17 AND V.29 HAVE BOTH.
 * Both of those halve the caller's block into a scratch buffer, run
 * `FPM_TONE_kill` over the copy and hand the copy to the detector; V.27ter
 * hands `FPM_MTD_detect` the CALLER's buffer, which by then is the buffer
 * `FPM_AGC_agc` has rewritten in place.  There is no copy loop in the object's
 * 331 bytes -- no loop at all -- and no `FPM_TONE_kill` relocation.  Checked
 * against the bytes rather than inferred from the size, because it is the one
 * structural difference between the three demodulators.  Finding F9115.
 *
 * A DETECTION ABANDONS THE WHOLE CALL: it returns 0 having run the gain
 * control and nothing else, so the caller's samples are left gain-controlled
 * and the resampler, the recoverer and the equaliser do not advance.
 *
 * THE TWO WIDENINGS OF `count` ARE DIFFERENT AND BOTH ARE FORCED.  The gain
 * control takes an `unsigned short` and gets `movzwl`; the tone detector takes
 * a `short` and gets `movswl` (0x0a599f).  The resampler's is `movzwl` again
 * only because the register already held that value and the callee's parameter
 * is 16 bits wide -- F614's free case, not a third reading.
 *
 * THE THREE EQUALISER FLAGS ARE STORED IN THE OBJECT'S ORDER, which is
 * `tilt_on`, `lms_on`, `pll_on` (0x0a5a24, 0x0a5a2f, 0x0a5a37).  That is NOT
 * V.17's or V.29's order, and the difference is kept because a store order is
 * evidence about one function and does not carry to its siblings.
 */
unsigned short
DemodDataV27(void *modem, short *in, unsigned short *bits, unsigned short count)
{
	int signal;
	unsigned short n;
	unsigned short m;
	void *rx;
	void *sh;

	FPM_AGC_agc(RX_AGC(FIELD_PTR(modem, V27_OBJ_RX)), in, count);
	/* Not the object's `%eax`; the same value.  D1094. */
	signal = RX_AGC(FIELD_PTR(modem, V27_OBJ_RX))->signal;

	sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	if (FIELD_US(sh, V27SH_SKIP_TONE) == 0) {
		if (FPM_MTD_detect((struct fpm_mtd *)FIELD_PTR(sh, V27SH_MTD),
				   in, (short)count) != 0)
			return 0;
	}

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	n = (unsigned short)FPM_MRF_filter(RX_MRF(rx), in,
					   (short *)FIELD_PTR(rx, V27RX_BUF_A),
					   (short)count);

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	RX_SRE(rx)->adapt = signal & FIELD_I(rx, V27RX_EN_SRE_ADAPT);
	m = FPM_SRE_recover(RX_SRE(rx),
			    (const short *)FIELD_PTR(rx, V27RX_BUF_A),
			    (short *)FIELD_PTR(rx, V27RX_BUF_B),
			    (short)n);

	if (m > V27RX_SRE_MAX && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("ERROR: SRE buffer violation(%d)", m);

	rx = FIELD_PTR(modem, V27_OBJ_RX);
	RX_FSE(rx)->tilt_on = 0;
	RX_FSE(rx)->lms_on = signal & FIELD_I(rx, V27RX_EN_FSE_LMS);
	RX_FSE(rx)->pll_on = signal & FIELD_I(rx, V27RX_EN_FSE_PLL);

	return FPM_FSE_receive(RX_FSE(rx),
			       (const short *)FIELD_PTR(rx, V27RX_BUF_B),
			       bits, m);
}

/* ------------------------------------------------------------------ */

/*
 * The receiver's descrambler, and its sibling `ScrambleDataV27` at the foot of
 * this file.  One indirection, one addition and a tail call each; the only
 * work either does is sign-extending `count`.
 */
void
DescrambleDataV27(void *modem, unsigned short *data, short count)
{
	SDMv27_descrambler((struct sdmv27 *)(void *)
				FIELD(FIELD_PTR(modem, V27_OBJ_RX), V27RX_SDM),
			   data, count);
}

/* ------------------------------------------------------------------ */

/*
 * Carrier, and the two things that can take it away.
 *
 * The V.21 arm exists because a fax receiver that has lost the image carrier
 * must notice the sending end going back to the control channel.  It is armed
 * -- and stays armed -- the moment the equaliser's error goes bad or carrier
 * drops, and from then on every block is copied out, gain-controlled and run
 * past the V.21 tone detector.  0x4ff samples without a hit is what the
 * detector needs to be believed.
 */
short
DataCarrierDetectV27(void *modem, short *samples, unsigned short count)
{
	void *rx = FIELD_PTR(modem, V27_OBJ_RX);
	void *sh = FIELD_PTR(modem, V27_OBJ_SHARED);
	void *dec = FIELD(rx, V27RX_DEC);
	short cd;

	cd = (short)(RX_AGC(rx)->signal & RX_SRE(rx)->active);

	if (FIELD_US(sh, V27SH_V21_WATCH) == 0) {
		if (FIELD_S(dec, V27DEC_SYM_COUNT) > V27RX_DEC_SETTLED) {
			if (RX_FSE(rx)->mse > V27RX_MSE_NO_CARRIER)
				cd = 0;
			else
				cd &= 1;
		}
		if (RX_FSE(rx)->mse > V27RX_MSE_NO_CARRIER && DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27 Decoder error too big..."
					     " no carrier\n");
	} else {
		short i;

		if (RX_FSE(rx)->mse > V27RX_MSE_NO_CARRIER || (cd & 1) == 0)
			FIELD_S(sh, V27SH_V21_ARMED) = 1;

		cd = 1;
		if (FIELD_S(sh, V27SH_V21_ARMED) != 0) {
			short *buf = (short *)FIELD_PTR(sh, V27SH_BUF);

			for (i = 0; i < (int)count; i = (short)(i + 1))
				buf[i] = samples[i];

			FPM_AGC_agc((struct fpm_agc *)(void *)
					FIELD(sh, V27SH_AGC), buf, count);

			if (FPM_MTD_detect((struct fpm_mtd *)
						FIELD_PTR(sh, V27SH_MTD_V21),
					   buf, (short)count) != 0)
				FIELD_US(sh, V27SH_V21_SAMPLES) = 0;
			else
				FIELD_US(sh, V27SH_V21_SAMPLES) =
					(unsigned short)
					(FIELD_US(sh, V27SH_V21_SAMPLES)
					 + count);

			if (FIELD_S(sh, V27SH_V21_SAMPLES)
			    > V27SH_V21_TIMEOUT) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V27: V21 Carrier detected\n");
				cd = 0;
			}
		}
	}

	if (FIELD_S(rx, V27RX_RMS_ON) != 0) {
		short level = FPM_rms(samples, count);
		unsigned short n;

		if (level < (short)((FIELD_S(rx, V27RX_RMS_REF)
				     * V27RX_RMS_DROP_Q15) >> 15)) {
			cd = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("sudden energy drop >"
						     " 8[dB], no carrier");
		}

		n = (unsigned short)(FIELD_US(rx, V27RX_RMS_COUNT) + 1);
		if (n == 2) {
			FIELD_S(rx, V27RX_RMS_REF) = level;
			FIELD_US(rx, V27RX_RMS_COUNT) = 0;
		} else {
			FIELD_US(rx, V27RX_RMS_COUNT) = n;
		}
	}

	return cd;
}

/* ------------------------------------------------------------------ */

/*
 * Grade the data, and keep the running error average the grade will be read
 * off later.
 *
 * The average runs for exactly 0x32 blocks and then stops; the block after it
 * -- and only that one -- compares the result against the limit and latches
 * the verdict.  The counter keeps incrementing past that, so the latch fires
 * once.
 */
short
QualityDetectV27(void *modem)
{
	void *rx = FIELD_PTR(modem, V27_OBJ_RX);
	short mse = RX_FSE(rx)->mse;
	short verdict;
	unsigned short n;

	verdict = (short)(RX_AGC(rx)->signal & RX_SRE(rx)->active);
	if (verdict == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V27 Dec error too big..."
					     " unreliable data\n");
		verdict = 2;
	}

	n = FIELD_US(rx, V27RX_Q_COUNT);
	if (n == 0) {
		FIELD_S(rx, V27RX_Q_ACC) = mse;
		FIELD_US(rx, V27RX_Q_COUNT) = 1;
	} else if ((short)n <= 0x31) {
		FIELD_S(rx, V27RX_Q_ACC) = (short)
			(((FIELD_S(rx, V27RX_Q_ACC) * 0x7333 + 0x4000) >> 15)
			 + ((mse * 0xccd + 0x4000) >> 15));
		FIELD_US(rx, V27RX_Q_COUNT) = (unsigned short)(n + 1);
	} else if ((short)n == 0x32) {
		if (FIELD_S(rx, V27RX_Q_ACC) <= FIELD_S(rx, V27RX_Q_LIMIT))
			FIELD_S(rx, V27RX_Q_FLAG) = 1;
		FIELD_US(rx, V27RX_Q_COUNT) = (unsigned short)(n + 1);
	}

	return verdict;
}

/* ------------------------------------------------------------------ */

int
EpochDetectV27(void *modem)
{
	void *rx = FIELD_PTR(modem, V27_OBJ_RX);

	return RX_FSE(rx)->lms_force != 0;
}

int
CarrierDetectV27(void *modem)
{
	void *rx = FIELD_PTR(modem, V27_OBJ_RX);

	return RX_SRE(rx)->active & RX_AGC(rx)->signal;
}

short
GetSNRV27(void *modem)
{
	(void)modem;			/* never read; see v27fax.h */
	return 10;
}

/* ------------------------------------------------------------------ */

/*
 * One block through the transmit chain.
 *
 * `count` GOES TO BOTH CALLS UNCHANGED, and it is not the same unit in each:
 * `SMC_encoder` takes data words and `FPM_PPS_filter` takes symbols.  The
 * object holds the caller's count in `%ebx` across both and stores it into
 * `0xc(%esp)` twice, so there is no conversion to reproduce.
 *
 * The ring is loaded from `tx + 0x08` twice, once per call, and `tx` itself is
 * re-read from the instance in between -- the reload pattern of every function
 * in this file.
 */
unsigned short
ModDataV27(void *modem, const unsigned short *bits, short *samples,
	   unsigned short count)
{
	void *tx;

	tx = FIELD_PTR(modem, V27_OBJ_TX);
	SMC_encoder((struct fpm_smc *)(void *)FIELD(tx, V27TX_SMC),
		    (struct fpm_smc_ring *)(void *)FIELD(tx, V27TX_RING),
		    bits, count);

	tx = FIELD_PTR(modem, V27_OBJ_TX);
	return FPM_PPS_filter((struct fpm_pps *)(void *)FIELD(tx, V27TX_PPS),
			      (struct fpm_smc_ring *)(void *)
					FIELD(tx, V27TX_RING),
			      samples, count);
}

/* ------------------------------------------------------------------ */

/*
 * The transmitter's scrambler.  `DescrambleDataV27` above is the same shape
 * over a different block: the scrambler lives in the TRANSMITTER's, at
 * tx + V27TX_SDM, and the descrambler in the receiver's.
 */
void
ScrambleDataV27(void *modem, unsigned short *data, short count)
{
	SDMv27_scrambler((struct sdmv27 *)(void *)
				FIELD(FIELD_PTR(modem, V27_OBJ_TX), V27TX_SDM),
			 data, count);
}
