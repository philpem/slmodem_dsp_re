/*
 * V32TXHDX.c -- ITU-T V.32 / V.32bis: the eight half-duplex TRANSMIT states.
 *
 * Reconstructed from dsplibs.o, in the object's own address order -- the whole
 * of the blob's `V32TXHDX.c` span except `V32TxHdxModem`, which is the driver
 * and lives in `v32hdx.c`:
 *
 *   TxHdxTone         .text 0x07fd40   113
 *   TxHdxCarrierState .text 0x07fdc0   194
 *   TxHdxScrSequence  .text 0x07fe90   209
 *   TxHdxTRN          .text 0x07ff70   237
 *   TxHdxData         .text 0x080060   194
 *   TxHdxNoCarrier    .text 0x080130   217
 *   TxHdxFinishFrame  .text 0x080210   152
 *   TxHdxNull         .text 0x0802b0   116
 *
 * `include/dsplib/v32hdx.h` is the CONTRACT these eight are written to and
 * `include/dsplib/v32hdxst.h` is the roster; read the first before reading
 * this file.  In one line: a state writes samples into `out`, reduces `*left`
 * by the symbols it consumed, returns the SAMPLE count, and transitions with
 * `V32NextState[hdx->mode](modem)`.
 *
 * ---------------------------------------------------------------------------
 * THE SEGMENT COUNTDOWN AT hdx + 0x78, WHICH IS THE SECOND BUDGET
 *
 * There are TWO budgets in play and confusing them is the easiest way to get
 * one of these functions wrong:
 *
 *   `*left`     the BLOCK's symbol budget.  Seeded by `V32TxHdxModem` from
 *               hdx + 0x9e, owned by the driver's loop, and the only thing
 *               that ends a call to `V32TxHdxModem`.
 *   hdx + 0x78  the HANDSHAKE STATE's countdown, an `int`.  It outlives the
 *               block and reaching zero or below is what TRANSITIONS.
 *
 * +0x78 is an `int` and not a short -- every access in these eight is a
 * 32-bit `mov`/`sub` (7fd75, 7fdf8, 80177 ...) with no truncation anywhere.
 * Its name here is usage inference, but over an EXHAUSTIVE reader set: the
 * five `V32*NextState` dispatchers are its only writers outside this file and
 * they set it to a per-state literal on entry to each handshake state (0x8,
 * 0x10, 0x18, 0x24, 0x28, 0x38, 0x40, 0x80, 0xb4, 0x100, 0x200, 0x400, 0x500,
 * 0x610, 0x960, 0xa60, 0x1a6c, 0x1b00, 0x1bbc, 0x2000, ...), every one of the
 * eight states below subtracts what it just consumed, and the `<= 0` test is
 * the sole trigger for `V32NextState`.  `RxHdxTone` (839ce) compares it
 * against 0xb4, which is exactly what `V32AnsNextState` (86462) and
 * `V32LocLoopNextState` (867ad) seed it with.
 *
 * WHAT EACH STATE CHARGES AGAINST IT IS NOT THE SAME QUANTITY, and that is
 * the distinction the two macros below carry:
 *
 *   the six symbol-wise states  charge `*left` -- the symbols they were
 *                               offered, NOT the `count` they clamped it to
 *   TxHdxTone, TxHdxNull        charge hdx + 0x84, once per call
 *
 * hdx + 0x84 is a `short` holding `V32_SYMBOL_LEN[rate]`, the symbols in one
 * block: `V32FP_recreate` (7f123) and `V32FP_control` (84606) both fill it
 * from that table by name, and `V32OrgNextState` (83529) copies hdx + 0x9e
 * into it -- the same value again.  It is nonetheless a SEPARATE field and
 * not an alias, because `V32RngRespNextState` (85594) writes ZERO there while
 * leaving +0x9e alone, which makes a tone or a silence state run to its
 * countdown rather than to a block count.  So the name is for what the two
 * block-wise states do with it and not for the table it usually comes from.
 *
 * ---------------------------------------------------------------------------
 * THE CLAMP IS NOT A MIN, AND A NEGATIVE COUNTDOWN IS NOT CLAMPED AWAY
 *
 * Six of the eight open with what looks like `count = min(*left, hdx->left)`.
 * It is not:
 *
 *      cmp    %eax,%ecx        (int)hdx->left  against  (int)*left
 *      mov    %eax,%ebx
 *      jg     .Lkeep
 *      movzwl %cx,%ebx         (unsigned short)hdx->left
 *
 * The comparison is SIGNED and the losing arm is TRUNCATED TO SIXTEEN BITS
 * UNSIGNED.  So a countdown that has already gone negative -- which it can,
 * since every state subtracts the symbols offered rather than the symbols
 * taken -- does not produce a zero or a small count: -1 produces 0xffff, and
 * the state then asks its callees for 65,535 symbols.  Writing this as a
 * `min` over `int`, or clamping the result at zero, is a different program.
 * `TxHdxNoCarrier` is the ONLY one that guards it, with a separate
 * `hdx->left > 0` test that GCC if-converts to `setg`/`neg`/`and` (8016b).
 *
 * ---------------------------------------------------------------------------
 * `*left` IS RE-READ AFTER EVERY CALL, AND THAT IS FORCED
 *
 * These states never cache `*left` in a local.  `TxHdxFinishFrame` is the
 * clearest case: it reads it at 8022e for the countdown subtraction AND as
 * `ScrambleDataV32`'s count, then reads it AGAIN at 80244 for
 * `TxNoCarrierV32`'s -- one CSE'd read where no call intervenes and a fresh
 * load where one does.  The six clamping states do the same at their tail,
 * loading `*left` afresh to subtract `count` from it.
 *
 * ---------------------------------------------------------------------------
 * THE RETURN VALUE IS READ AFTER THE TRANSITION, NOT BEFORE
 *
 * `TxHdxTone` and `TxHdxNull` return hdx + 0xa0 (`V32_SAMPLE_LEN[rate]`), and
 * both re-read `obj + 0x64` after the `V32NextState` call before doing so
 * (7fda2, 8030c).  A transition may replace the whole context, so the count
 * they report is the NEW context's block length and not the one they just
 * filled.  Whether that is a defect in the original is not this file's
 * question; it is what the object does.
 *
 * The other six return the sample count their modulator handed back, held in
 * a callee-saved register across the transition (`movswl %ax,%esi`).  The
 * local is a `short`: `ModDataV32` and `TxNoCarrierV32` are declared
 * `unsigned short` and the object still SIGN-extends, which per finding F7803
 * follows the declared type of the local rather than the callee's return.
 */

#include "dsplib/v32hdxst.h"

#include "dsplib/v32data.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32seq.h"
#include "dsplib/fpm_tone.h"

/* The instance is not modelled; see v32hdx.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(void *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_INT(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_S16(obj, off)	(*(short *)(void *)FIELD((obj), (off)))

#define HDX(m)			FIELD_PTR((m), V32_OBJ_HDX)

/*
 * The handshake state's countdown, in symbols, and the charge a whole-block
 * state makes against it.  Both are derived at the top of this file; guarded
 * because the twelve `RxHdx*` states reach the same two fields and may spell
 * them in their own file.
 */
#ifndef V32HDX_STATE_LEFT
#define V32HDX_STATE_LEFT	0x78	/* int, <= 0 transitions              */
#endif
#ifndef V32HDX_BLOCK_CHARGE
#define V32HDX_BLOCK_CHARGE	0x84	/* short, = V32_SYMBOL_LEN[rate]      */
#endif

/*
 * The TRN alphabet.  `TxHdxTRN` folds each scrambled word onto one of two
 * constellation points by its low two bits, 0 and 1 giving 0 and 2 and 3
 * giving 3.  The table is a LOCAL initialised by four `movw` stores at
 * 7ff7b..7ffa2 and not a `.rodata` object, so it is spelled that way below;
 * this comment is here because the four literals are otherwise unexplained.
 */

/* ------------------------------------------------------------------------ */

/*
 * Fill the block with the tone at hdx + 0x2c and spend the whole of `*left`.
 *
 * `data` is unused: the tone generator writes `out` directly and no data path
 * runs.  Nothing at 0x24(%esp) is touched.
 */
short
TxHdxTone(void *modem, short *data, short *out, unsigned short *left)
{
	void *hdx = HDX(modem);

	FPM_TONE_generate((struct fpm_tone *)FIELD_PTR(hdx, V32_HDX_TONE0),
			  out, FIELD_S16(hdx, V32HDX_SAMPLE_LEN));
	*left = 0;

	hdx = HDX(modem);
	FIELD_INT(hdx, V32HDX_STATE_LEFT) -=
		FIELD_S16(hdx, V32HDX_BLOCK_CHARGE);
	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) <= 0) {
		V32NextState[FIELD_S16(hdx, V32HDX_MODE)](modem);
		hdx = HDX(modem);
	}

	return FIELD_S16(hdx, V32HDX_SAMPLE_LEN);
}

/*
 * Emit the generator's sequence, unscrambled, through the modulator.
 *
 * This is the carrier-bearing handshake segment: `GenSequence` fills `data`
 * from the pattern `InitGenSequence` armed and `ModDataV32` shapes it.
 */
short
TxHdxCarrierState(void *modem, short *data, short *out, unsigned short *left)
{
	void *hdx = HDX(modem);
	unsigned short count;
	short n;

	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) > *left)
		count = *left;
	else
		count = (unsigned short)FIELD_INT(hdx, V32HDX_STATE_LEFT);

	FIELD_INT(hdx, V32HDX_STATE_LEFT) -= *left;

	GenSequence(modem, data, count);
	n = (short)ModDataV32(modem, data, out, count);

	hdx = HDX(modem);
	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) <= 0)
		V32NextState[FIELD_S16(hdx, V32HDX_MODE)](modem);

	*left = (unsigned short)(*left - count);
	return n;
}

/* The same, with the scrambler in the path between generator and modulator. */
short
TxHdxScrSequence(void *modem, short *data, short *out, unsigned short *left)
{
	void *hdx = HDX(modem);
	unsigned short count;
	short n;

	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) > *left)
		count = *left;
	else
		count = (unsigned short)FIELD_INT(hdx, V32HDX_STATE_LEFT);

	FIELD_INT(hdx, V32HDX_STATE_LEFT) -= *left;

	GenSequence(modem, data, count);
	ScrambleDataV32(modem, data, count);
	n = (short)ModDataV32(modem, data, out, count);

	hdx = HDX(modem);
	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) <= 0)
		V32NextState[FIELD_S16(hdx, V32HDX_MODE)](modem);

	*left = (unsigned short)(*left - count);
	return n;
}

/*
 * The equaliser training segment: generate, scramble, then FOLD each word
 * onto the two-point TRN alphabet before modulating it.
 *
 * The fold's loop counter is a `short` -- `lea 0x1(%edx),%edi` then
 * `movswl %di,%edx` at 7ffe4/7fff3, a truncation the object pays for on every
 * iteration -- while the bound is `count` widened to 32 bits, so the compare
 * is signed.  An `int` counter is a different program only above 32,767
 * symbols in one block, which no rate reaches; it is written as the object
 * encodes it.
 */
short
TxHdxTRN(void *modem, short *data, short *out, unsigned short *left)
{
	short trn[4];
	void *hdx;
	unsigned short count;
	short i;
	short n;

	trn[0] = 0;
	trn[1] = 0;
	trn[2] = 3;
	trn[3] = 3;

	hdx = HDX(modem);

	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) > *left)
		count = *left;
	else
		count = (unsigned short)FIELD_INT(hdx, V32HDX_STATE_LEFT);

	FIELD_INT(hdx, V32HDX_STATE_LEFT) -= *left;

	GenSequence(modem, data, count);
	ScrambleDataV32(modem, data, count);

	for (i = 0; i < count; i++)
		data[i] = trn[data[i] & 3];

	n = (short)ModDataV32(modem, data, out, count);

	hdx = HDX(modem);
	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) <= 0)
		V32NextState[FIELD_S16(hdx, V32HDX_MODE)](modem);

	*left = (unsigned short)(*left - count);
	return n;
}

/*
 * The connected data segment: whatever the caller left in `data`, scrambled
 * and modulated.  No generator.
 */
short
TxHdxData(void *modem, short *data, short *out, unsigned short *left)
{
	void *hdx = HDX(modem);
	unsigned short count;
	short n;

	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) > *left)
		count = *left;
	else
		count = (unsigned short)FIELD_INT(hdx, V32HDX_STATE_LEFT);

	FIELD_INT(hdx, V32HDX_STATE_LEFT) -= *left;

	ScrambleDataV32(modem, data, count);
	n = (short)ModDataV32(modem, data, out, count);

	hdx = HDX(modem);
	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) <= 0)
		V32NextState[FIELD_S16(hdx, V32HDX_MODE)](modem);

	*left = (unsigned short)(*left - count);
	return n;
}

/*
 * Drop the carrier: run the scrambler over `data` as usual, then shape the
 * no-carrier constellation point instead of the data.
 *
 * THE SECOND GUARD IS THIS FUNCTION'S ALONE.  `count` is forced to zero once
 * the countdown has expired, so an over-run segment stops scrambling rather
 * than asking for the 0xffff the truncation would otherwise produce.  The
 * scrambler still RUNS -- with a count of zero -- because the object calls it
 * unconditionally.
 */
short
TxHdxNoCarrier(void *modem, short *data, short *out, unsigned short *left)
{
	void *hdx = HDX(modem);
	unsigned short count;
	short n;

	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) > *left)
		count = *left;
	else
		count = (unsigned short)FIELD_INT(hdx, V32HDX_STATE_LEFT);

	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) <= 0)
		count = 0;

	FIELD_INT(hdx, V32HDX_STATE_LEFT) -= *left;

	ScrambleDataV32(modem, data, count);
	n = (short)TxNoCarrierV32(modem, data, out, count);

	hdx = HDX(modem);
	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) <= 0)
		V32NextState[FIELD_S16(hdx, V32HDX_MODE)](modem);

	*left = (unsigned short)(*left - count);
	return n;
}

/*
 * Finish the block with no carrier, then hand the rest of it back.
 *
 * NO CLAMP AND NO COUNTDOWN GUARD: `*left` itself is the count, charged in
 * full against the countdown whether or not any of it was left, and `*left`
 * is ZEROED at the end rather than reduced -- so this state always ends the
 * driver's loop.  It is the only one of the eight that does.
 */
short
TxHdxFinishFrame(void *modem, short *data, short *out, unsigned short *left)
{
	void *hdx = HDX(modem);
	short n;

	FIELD_INT(hdx, V32HDX_STATE_LEFT) -= *left;

	ScrambleDataV32(modem, data, *left);
	n = (short)TxNoCarrierV32(modem, data, out, *left);

	hdx = HDX(modem);
	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) <= 0)
		V32NextState[FIELD_S16(hdx, V32HDX_MODE)](modem);

	*left = 0;
	return n;
}

/*
 * Silence: write hdx + 0xa0 zero samples and spend the whole block.
 *
 * The loop is `while (i-- != 0)` and the object encodes exactly that shape --
 * `dec %eax; cmp $0xffffffff,%eax; jne` at 802d0, entered at its TEST.  The
 * counter is an `int`: 32 bits throughout with no truncation, though the field
 * it comes from is a `short` (`movswl` at 802bf).
 *
 * IT IS NOT A `for (i = 0; i < n; i++)`, AND THE DIFFERENCE IS UNBOUNDED.
 * The test is `--i != -1`, so a block length of zero writes nothing but a
 * NEGATIVE one runs 2**32 - |n| iterations, filling memory until it faults.
 * The object has no guard and neither does this; a test must not drive
 * hdx + 0xa0 negative here, which is a property of the original and not a
 * limitation of the harness.  (A deviation number is not claimed for this;
 * the pass that lands it should register one.)
 *
 * `data` is unused here too.
 */
short
TxHdxNull(void *modem, short *data, short *out, unsigned short *left)
{
	void *hdx = HDX(modem);
	int i = FIELD_S16(hdx, V32HDX_SAMPLE_LEN);

	while (i-- != 0)
		*out++ = 0;

	FIELD_INT(hdx, V32HDX_STATE_LEFT) -=
		FIELD_S16(hdx, V32HDX_BLOCK_CHARGE);
	if (FIELD_INT(hdx, V32HDX_STATE_LEFT) <= 0) {
		V32NextState[FIELD_S16(hdx, V32HDX_MODE)](modem);
		hdx = HDX(modem);
	}

	*left = 0;
	return FIELD_S16(hdx, V32HDX_SAMPLE_LEN);
}
