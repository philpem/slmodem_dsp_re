/*
 * v21.c -- ITU-T V.21 (the fax control channel): the receive and transmit
 * primitives.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V21RX_delete      .text 0x099270   123
 *   V21TX_delete      .text 0x0995f0   104
 *   V21RX_modem       .text 0x0a1c40   127
 *   RxHdxErrorV21     .text 0x0a1cc0    59
 *   RxHdxIdleV21      .text 0x0a1d00    81
 *   RxNextStateV21    .text 0x0a1d60   290
 *   RxHdxStartV21     .text 0x0a1e90   522
 *   RxHdxWaitV21      .text 0x0a20a0   440
 *   RxHdxDataV21      .text 0x0a2260   418
 *   V21RX_status      .text 0x0a2460   132
 *   V21TX_modem       .text 0x0a24f0   182
 *   V21TX_status      .text 0x0a2c00    96
 *   DemodDataV21      .text 0x0a5740   217
 *   CarrierDetectV21  .text 0x0a5820    16
 *   GetSNRV21         .text 0x0a5830    77
 *   ModDataV21        .text 0x0a5880    87
 *   TxNoCarrierV21    .text 0x0a58e0   103
 *
 * THESE ARE SEVENTEEN LEAVES OF THREE DIFFERENT CLUSTERS, not one author file:
 * 0x099270 sits with the constructors and destructors, 0x0a1c40 with the
 * half-duplex machine, and 0x0a5740 onward with the per-modulation data
 * paths.  They are collected here because they are the V.21 work that is
 * startable, and `include/dsplib/v21fax.h` says what each one establishes.
 * The order below is the object's own address order.
 *
 * THE FIVE RECEIVE-PATH SYMBOLS ARE ONE INDIVISIBLE UNIT and had to be
 * written together.  `DemodDataV21` carries an `R_386_32` against
 * `RxHdxDataV21` on the `cmpl` at 0x0a57a3 -- a DATA reference, not a call --
 * and `RxHdxErrorV21`, `RxHdxIdleV21`, `RxHdxWaitV21` and `RxHdxDataV21` all
 * call `DemodDataV21`.  Under findings F8492/F8493 a reference from `src/` to
 * a symbol this tree has not written is an undefined reference that fails
 * every test binary, so no proper subset of the five links.
 *
 * The functions are laid out in the object's address order, which is the one
 * lever this tree has on register allocation across a translation unit
 * (finding F7796) -- it is not a claim that the author had them in one file.
 *
 * Everything here takes a `void *` handle, because neither `V21TX_create`
 * (0x0992f0) nor `V21RX_create` (0x098e70) is reconstructed and naming their
 * fields now would be guessing.  See the header for the ruling and for where
 * every offset used below comes from.
 */

#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v21fax.h"

/*
 * The TRANSMITTER handle is not modelled -- `V21TX_create` is not
 * reconstructed -- so the two transmit entry points below reach it through
 * offsets, exactly as `v17.c` and `v29.c` reach theirs.  The receive side
 * keeps its typed accessors from `v21fax.h`; the two halves are different
 * objects (see the header) and are spelled differently on purpose.
 */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define AT_I(p, off)		(*(int *)(void *)FIELD((p), (off)))

/*
 * The handle is re-read from the caller's argument before every free rather
 * than cached in a local, because the object re-reads it: five separate
 * `mov 0x50(%ebx),%eax` between 0x099278 and 0x0992c8.
 *
 * THE LITERAL 1 IN THE SECOND ARGUMENT SLOT IS NOT REPRODUCED.  The object
 * puts one there before `FPM_FSD_free` and `FPM_MRF_free`, both of which take
 * a single argument and never load a second -- so it is dead stack setup,
 * presumably from a version where they took a `fresh` flag like their init
 * counterparts.  `B103FP_delete` has exactly the same three call sites and
 * the same note; there is nothing to reproduce, because an argument the
 * callee never loads has no observable effect.
 *
 * There is no NULL guard on anything, and the last free releases the handle
 * unconditionally even when the caller supplied it.  Both reproduced; see
 * docs/deviations.md D1039.
 */
void
V21RX_delete(void *modem)
{
	FPM_MTD_delete(V21RX_DSP(modem)->mtd);
	FPM_FSD_free(&V21RX_DSP(modem)->fsd);
	FPM_MRF_free(&V21RX_DSP(modem)->mrf);

	sysdep_free(V21RX_DSP(modem)->mag);
	sysdep_free(V21RX_DSP(modem));
	sysdep_free(V21RX_HDX(modem));
	sysdep_free(modem);
}

/*
 * V21TX_delete -- .text 0x0995f0, 104 bytes.
 *
 * The transmitter's seven releases.  The DSP block is re-read from the handle
 * before every one rather than cached, because the object re-reads it: four
 * separate `mov 0x24(%ebx),%e?x` between 0x0995f8 and 0x099628, and two more
 * of `0x20(%ebx)` after them.
 *
 * `FPM_FSM_delete`, `FPM_MRF_free` and the `sysdep_free` of +0x2c type the
 * three members of `struct v21_tx_dsp` a second time -- `ModDataV21` typed
 * them by what each is handed TO, this function by what releases each -- so
 * the block's layout has two independent statements behind it.  See the
 * header and finding F9252.
 *
 * THE LITERAL 1 IN THE SECOND ARGUMENT SLOT IS NOT REPRODUCED (0x099603,
 * before `FPM_MRF_free`).  Finding F8876; `V21RX_delete` above carries the
 * same note for the same reason.
 *
 * No NULL guard anywhere, and the handle goes unconditionally; D1150.
 */
void
V21TX_delete(void *modem)
{
	FPM_FSM_delete(&V21TX_DSP(modem)->fsm);
	FPM_MRF_free(&V21TX_DSP(modem)->mrf);
	sysdep_free(V21TX_DSP(modem)->scratch);
	sysdep_free(V21TX_DSP(modem));

	FIFO_delete((struct fax_fifo *)
			FIELD_PTR(FIELD_PTR(modem, V21TX_OBJ_PARAMS),
				  V21TXP_FIFO));
	sysdep_free(FIELD_PTR(modem, V21TX_OBJ_PARAMS));

	sysdep_free(modem);
}

/*
 * One block of receive.
 *
 * `remaining` is the count the handler is still working through, and it is
 * held UNSIGNED while `before` is a `short`: the object sign-extends the
 * previous count into the subtraction (`movswl %cx,%ebx` at 0x0a1c70) and
 * zero-extends the new one (`movzwl %cx,%edx` at 0x0a1c91).  The second
 * extension feeds a 32-bit subtract, so it is FORCED and not the free kind
 * -- CLAUDE.md's rule for reading a codegen difference, applied the way round
 * it is meant to be.
 *
 * The loop is a do-while: `*count` of zero on entry still dispatches once.
 *
 * The return is the 32-bit word that starts at the status byte, taken with
 * `memcpy` rather than a cast for the reason `B103FP_modem` takes its
 * identically shaped one that way.
 */
int
V21RX_modem(void *modem, short *in, short *out, short *count)
{
	unsigned short remaining;
	short total = 0;
	int word;

	V21RX_FLAGS(modem) &= (unsigned char)~V21RX_FLAG_ERROR;

	remaining = (unsigned short)*count;
	do {
		short before = (short)remaining;
		short n;

		n = V21RX_HDX(modem)->handler(modem, in, out, count);
		remaining = (unsigned short)*count;

		out += n;
		in += before - remaining;
		total = (short)(total + n);
	} while (remaining != 0);

	*count = total;

	memcpy(&word, V21RX_STATUS_AT(modem), sizeof word);
	return word;
}

/*
 * The error state: raise the flag, run the block through the demodulator
 * anyway so the filters keep their history, and consume it.
 *
 * Nothing here advances the state, so once `RxHdxWaitV21` has installed this
 * handler the machine stays in it until something outside re-installs
 * another one.  The flag is a one-shot: `V21RX_modem` clears it at the top of
 * every block, so a caller that does not read the returned word each block
 * loses the event.
 */
short
RxHdxErrorV21(void *modem, short *in, short *out, short *count)
{
	V21RX_FLAGS(modem) |= V21RX_FLAG_ERROR;

	DemodDataV21(modem, in, out, (unsigned short)*count);
	*count = 0;

	return 0;
}

/*
 * The idle state: demodulate, report, and re-read the carrier.
 *
 * The carrier flag is cleared BEFORE `CarrierDetectV21` is called and set
 * again only if it answers, rather than being assigned from the answer --
 * `andb $0xdf` at 0x0a1d31 and `orb $0x20` at 0x0a1d45 with the call between
 * them.  The two spellings agree on the value and not on the instructions,
 * and this is the object's.
 */
short
RxHdxIdleV21(void *modem, short *in, short *out, short *count)
{
	DemodDataV21(modem, in, out, (unsigned short)*count);
	*count = 0;

	V21RX_FLAGS(modem) &= (unsigned char)~V21RX_FLAG_CARRIER;
	V21RX_STATUS(modem) = V21RX_STATUS_IDLE;

	if (CarrierDetectV21(modem))
		V21RX_FLAGS(modem) |= V21RX_FLAG_CARRIER;

	return 0;
}

/*
 * Advance the receive state machine one step.
 *
 * THIS IS `RxNextStateV21` (0x0a1d60, 290 bytes), AND IT IS NOW CLAIMED.
 * The object carries the block four times: once out of line under that name,
 * and three more times inlined into `RxHdxStartV21`, `RxHdxWaitV21` and
 * `RxHdxDataV21`.  All four copies are the same instructions in the same
 * order, which is what says the author wrote one function and the compiler
 * inlined it at `-O3`.  Finding F8898 left it `static v21rx_next_state`
 * because its third caller, `RxHdxStartV21`, was not reconstructed and a
 * `static` carrying a blob symbol's name would have been counted as written
 * by every tool that globs `build/repro`.  That caller is below, so the
 * symbol is global and named here; finding F9091.
 *
 * The four strings are the author's own words, out of .rodata.str1.1 at
 * 0x4b3a, 0x4b16, 0x4b28 and 0x4b03; `tools/relocscan.py` is what pairs them
 * with these sites, because the reference is an R_386_32 against the section
 * symbol with the offset as an inline addend (finding F604).
 *
 * The default arm is reached from state IDLE and state ERROR alike, and it
 * does not install a handler: it only resets the flags and reports
 * V21RX_STATUS_DEFAULT.
 */
void
RxNextStateV21(void *modem)
{
	struct v21_rx_hdx *hdx = V21RX_HDX(modem);

	switch (hdx->state) {
	case V21RX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RX_STATE_START\n");
		hdx->countdown = 1;
		hdx->handler = RxHdxWaitV21;
		hdx->state = V21RX_STATE_WAIT;
		break;

	case V21RX_STATE_WAIT:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RX_STATE_WAIT\n");
		hdx->countdown = 0;
		hdx->handler = RxHdxDataV21;
		hdx->state = V21RX_STATE_DATA;
		V21RX_FLAGS(modem) |= V21RX_FLAG_DATA;
		break;

	case V21RX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RX_STATE_DATA\n");
		hdx->handler = RxHdxIdleV21;
		hdx->state = V21RX_STATE_IDLE;
		hdx->countdown = 0;
		hdx->int_0000 = 0;
		V21RX_FLAGS1(modem) |= V21RX_FLAG1_IDLE;
		V21RX_FLAGS(modem) &= (unsigned char)~V21RX_FLAG_DATA;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RX_DEFAULT, %d\n", hdx->state);
		V21RX_FLAGS1(modem) &= (unsigned char)~V21RX_FLAG1_IDLE;
		V21RX_STATUS(modem) = V21RX_STATUS_DEFAULT;
		V21RX_FLAGS(modem) = (unsigned char)
			((V21RX_FLAGS(modem) | V21RX_FLAG_ERROR)
			 & ~(V21RX_FLAG_DATA | V21RX_FLAG_CARRIER));
		break;
	}
}

/*
 * The start state, which is the one `V21RX_create` installs (0x098ee1).
 *
 * It demodulates the block like every other handler and then WALKS THE
 * DEMODULATED UNITS, which none of the other four does.  The walk keeps
 * `ones_run` as the length of the current run of non-zero units and bumps
 * `mark_seq` on each transition out of a run of exactly six; the state
 * advances once `mark_seq` has passed four and the carrier is up.
 *
 * THE TWO COUNTERS ARE UPDATED IN THE ORDER THE OBJECT UPDATES THEM, and the
 * ordering is observable: at `ones_run == 6` with a zero unit the object
 * increments `mark_seq` (0x0a1f9c) and then falls into the common store that
 * puts zero in `ones_run` (0x0a1efc), while at `ones_run == 6` with a
 * non-zero unit it jumps straight to the increment at 0x0a1ef7 and leaves
 * `mark_seq` alone.  The `? :` below is the same three-way outcome written
 * once; the object's two entries into the common store are the compiler's
 * tail-merge of it.
 *
 * BOTH LOOP VARIABLES ARE 16-BIT AND THAT IS FORCED.  `nbits` is decremented
 * with `lea -0x1(%esi),%eax` / `movzwl %ax,%esi` and `i` incremented with
 * `lea 0x1(%edi),%ebx` / `movzwl %bx,%edi` (0x0a1f00..0x0a1f0b), so a count of
 * 0x10000 would be a count of zero and the walk is skipped entirely; the
 * demodulator cannot return one, but the reproduction does not depend on
 * that.  The `> 4` test is `cmpw $0x4` with `jle`, so it is SIGNED over
 * sixteen bits -- the same shape `RxHdxWaitV21`'s countdown has and the same
 * cast is used for it.
 *
 * The return is a literal zero on every path (0x0a1f83), so the bits this
 * handler produced are reported to `V21RX_modem` as none.  That is the
 * object's; see docs/deviations.md D1090.
 */
short
RxHdxStartV21(void *modem, short *in, short *out, short *count)
{
	struct v21_rx_hdx *hdx;
	unsigned short nbits;
	unsigned short i;

	V21RX_STATUS(modem) = V21RX_STATUS_START;

	nbits = DemodDataV21(modem, in, out, (unsigned short)*count);
	*count = 0;

	hdx = V21RX_HDX(modem);

	i = 0;
	while (nbits != 0) {
		if (hdx->ones_run == V21RX_MARK_RUN && out[i] == 0)
			hdx->mark_seq = (unsigned short)(hdx->mark_seq + 1);

		hdx->ones_run = (unsigned short)
			(out[i] != 0 ? hdx->ones_run + 1 : 0);

		nbits = (unsigned short)(nbits - 1);
		i = (unsigned short)(i + 1);
	}

	V21RX_FLAGS(modem) &= (unsigned char)~V21RX_FLAG_CARRIER;

	if (!CarrierDetectV21(modem))
		return 0;

	hdx = V21RX_HDX(modem);
	if ((short)hdx->mark_seq <= V21RX_MARK_SEQ_THRESHOLD)
		return 0;

	V21RX_FLAGS(modem) |= V21RX_FLAG_CARRIER;
	RxNextStateV21(modem);

	return 0;
}

/*
 * The wait state: hold for `hdx->countdown` blocks with the carrier up, then
 * advance.
 *
 * Losing the carrier is fatal here and not merely reported: the error handler
 * is installed, the state goes to V21RX_STATE_ERROR and the return is zero
 * rather than the bit count, so the bits this block did produce are thrown
 * away.  That arm is the object's and is reproduced.
 *
 * The countdown is loaded `movzwl` and tested `jle` on the low sixteen bits
 * (0x0a20f5..0x0a2101), which is why the field is `unsigned short` and the
 * test narrows: the two readings agree over every value the field can hold,
 * and the load is what the object encodes.
 */
short
RxHdxWaitV21(void *modem, short *in, short *out, short *count)
{
	unsigned short nbits;

	nbits = DemodDataV21(modem, in, out, (unsigned short)*count);
	*count = 0;

	if (!CarrierDetectV21(modem)) {
		V21RX_HDX(modem)->handler = RxHdxErrorV21;
		V21RX_HDX(modem)->state = V21RX_STATE_ERROR;
		V21RX_STATUS(modem) = V21RX_STATUS_ERROR;
		V21RX_FLAGS(modem) = (unsigned char)
			((V21RX_FLAGS(modem) | V21RX_FLAG_ERROR)
			 & ~V21RX_FLAG_CARRIER);
		return 0;
	}

	V21RX_FLAGS(modem) |= V21RX_FLAG_CARRIER;
	V21RX_STATUS(modem) = V21RX_STATUS_WAIT;

	V21RX_HDX(modem)->countdown =
		(unsigned short)(V21RX_HDX(modem)->countdown - 1);
	if ((short)V21RX_HDX(modem)->countdown > 0)
		return 0;

	V21RX_STATUS(modem) = V21RX_STATUS_TIMEOUT;
	RxNextStateV21(modem);

	return (short)nbits;
}

/*
 * The data state: demodulate while the carrier is up, and grade the result.
 *
 * The carrier flag is raised UNCONDITIONALLY on entry and lowered again on
 * the arm where `CarrierDetectV21` says it has gone, which is not the same
 * as assigning it -- that is the object's order (0x0a2277 before the call,
 * 0x0a22eb after it) and it is what a caller reading the flag from a handler
 * that ran earlier in the same block would see.
 *
 * `hdx->int_0000` is the second gate and nothing reconstructed sets it, so it
 * is exercised in the test by planting it rather than by reaching it.
 *
 * `GetSNRV21` is compared 16 bits wide (`cmpw $0x5,%ax`), so the narrowing
 * below is the object's and not a convenience.  As reconstructed that
 * function returns a literal zero, so the flag is raised on every block the
 * data state demodulates; the comparison is reproduced because it is there.
 */
short
RxHdxDataV21(void *modem, short *in, short *out, short *count)
{
	unsigned short nbits;

	V21RX_FLAGS(modem) |= V21RX_FLAG_CARRIER;
	V21RX_STATUS(modem) = V21RX_STATUS_DATA;

	if (CarrierDetectV21(modem) && V21RX_HDX(modem)->int_0000 == 0) {
		nbits = DemodDataV21(modem, in, out, (unsigned short)*count);
		*count = 0;

		V21RX_FLAGS(modem) &= (unsigned char)~V21RX_FLAG_LOW_SNR;
		if ((short)GetSNRV21(modem) <= V21RX_SNR_THRESHOLD)
			V21RX_FLAGS(modem) |= V21RX_FLAG_LOW_SNR;

		return (short)nbits;
	}

	V21RX_FLAGS(modem) &= (unsigned char)~V21RX_FLAG_CARRIER;
	RxNextStateV21(modem);

	return 0;
}

/*
 * Fill the caller's status block from the RECEIVER.
 *
 * NOT a mirror of `V21TX_status`.  The rate goes in `rx_bps` and not
 * `tx_bps`, `snr` carries what `GetSNRV21` answered rather than a literal
 * zero, `quality` is the COMPLEMENT of V21RX_FLAG_LOW_SNR (`testb $0x80`
 * followed by `sete`, 0x0a2487), +0x0e is zeroed where the transmit side
 * zeroes +0x0c, and the flags byte is stored as a literal 0 rather than
 * merged from the handle.
 *
 * `short_12` IS THE ONLY ARITHMETIC IN THE FUNCTION and it is a `cltd`/`idiv`
 * over two SIGNED shorts loaded `movswl` (0x0a24b3 and 0x0a24b7), then
 * `imul $0x12c`.  Both operands live in the fsd: +0x76 of the DSP block is
 * `fsd.f22` and +0x66 is `fsd.cfg.bit_samples`, and `FPM_FSD_init` sets the
 * first to half the second -- so for an even `bit_samples` the quotient is 1
 * and the field comes out at 300, which is `rx_bps` again by a different
 * route.  Finding F9132.
 *
 * THE DIVIDE HAS NO GUARD.  A receiver whose fsd was never configured has
 * `bit_samples` zero and this faults; see docs/deviations.md D1098.  It is
 * reproduced, and the test asserts the precondition on BOTH sides rather
 * than driving it.
 */
int
V21RX_status(void *modem, struct v21_status *st)
{
	short f22, bit_samples;

	if (st == NULL)
		return 0;

	st->protocol = (short)*(unsigned short *)(void *)
		((char *)modem + V21RX_OBJ_PROTOCOL);
	st->tx_bps = 0;
	st->rx_bps = V21_STATUS_BPS;
	st->quality = (short)
		((V21RX_FLAGS(modem) & V21RX_FLAG_LOW_SNR) == 0);
	st->snr = (short)GetSNRV21(modem);
	st->short_0a = 0;
	st->short_0e = 0;
	st->short_10 = 0;
	st->flags1 &= (unsigned char)~V21_STATUS1_BIT0;
	st->flags = 0;

	f22 = V21RX_DSP(modem)->fsd.f22;
	bit_samples = V21RX_DSP(modem)->fsd.cfg.bit_samples;
	st->short_12 = (short)((2 - 2 * (int)f22 / (int)bit_samples)
			       * V21_STATUS_BPS);

	return 1;
}

/*
 * V21TX_modem -- .text 0x0a24f0, 182 bytes.  See v21fax.h for the two arms,
 * the budget and why `in` does not advance while `out` does.
 *
 * IT IS `V17TX_modem` FOR A DIFFERENT MODULATION, and the object says so:
 * both are 182 bytes and the instruction sequences differ in four immediates
 * and nothing else -- the gate at params + 0x04 against V.17's + 0x08, the
 * dispatch slot at + 0x08 against + 0x14, the budget 6 against 0x30, and the
 * status byte 4 against 9.  `V29TX_modem` is the third copy.  Finding F9253.
 *
 * The parameter block is read ONCE before the gate and re-read at the top of
 * every loop iteration; the object hoists the first iteration's read out of
 * the loop (0x0a24ff and 0x0a259e reach the head at 0x0a252a, and the back
 * edge at 0x0a2527 reloads), which is loop rotation of exactly this source.
 */
int
V21TX_modem(void *modem, unsigned short *in, short *out, unsigned short *count)
{
	void *prm;
	unsigned short taken;
	short budget;
	short total;

	prm = FIELD_PTR(modem, V21TX_OBJ_PARAMS);

	*FIELD(modem, V21TX_OBJ_RESULT_B1) &=
		(unsigned char)~V21TX_RESULT_B1_BIT1;

	if (AT_I(prm, V21TXP_INT_0004) == 0)
		taken = (unsigned short)FIFO_write(
				(struct fax_fifo *)
					FIELD_PTR(prm, V21TXP_FIFO),
				in, *count);
	else
		taken = *count;

	budget = V21TX_MODEM_BUDGET;
	total = 0;
	do {
		short got;

		prm = FIELD_PTR(modem, V21TX_OBJ_PARAMS);
		got = (*(v21tx_process_fn *)(void *)
				FIELD(prm, V21TXP_PROCESS))
					(modem, in, out, &budget);

		out += got;
		total = (short)(total + got);
	} while (budget > 0);

	if (*count != taken) {
		*FIELD(modem, V21TX_OBJ_RESULT_B1) |= V21TX_RESULT_B1_BIT1;
		/*
		 * A BYTE store into the low byte of the int this function
		 * returns -- `movb $0x4,0x1c(%edi)` at 0x0a2564 -- which is
		 * why it cannot be written through `AT_I`.
		 */
		*FIELD(modem, V21TX_OBJ_RESULT) = V21TX_RESULT_BYTE_04;
	}

	*count = (unsigned short)total;

	return AT_I(modem, V21TX_OBJ_RESULT);
}

/*
 * Fill the caller's status block.
 *
 * The last statement ASSIGNS the flags byte rather than merging into it, so
 * the two bits cleared four statements earlier are cleared for nothing and
 * every other bit the caller had is lost.  That is the object's; see D1037.
 */
int
V21TX_status(void *modem, struct v21_status *st)
{
	if (st == NULL)
		return 0;

	st->protocol = (short)V21TX_PROTOCOL(modem);
	st->tx_bps = V21_STATUS_BPS;
	st->rx_bps = 0;
	st->quality = 0;
	st->snr = 0;
	st->short_0a = 0;
	st->short_0c = 0;
	st->flags &= (unsigned char)~(V21_STATUS_BIT0 | V21_STATUS_BIT1);
	st->short_10 = 0;
	st->short_12 = 0;
	st->flags1 &= (unsigned char)~V21_STATUS1_BIT0;
	st->flags = (unsigned char)(V21TX_FLAGS(modem) & V21_STATUS_BIT2);

	return 1;
}

/*
 * One block through the receive chain.
 *
 * The DSP block is re-read from the handle at every use rather than cached,
 * because the object re-reads it: four separate `mov 0x50(%edi),%e?x` at
 * 0x0a5764, 0x0a5772, 0x0a57a0/0x0a57c8 and 0x0a57f7, with `%edi` holding the
 * handle throughout.  A local pointer would have lived in a callee-saved
 * register across the calls instead.
 *
 * `FPM_AGC_agc` IS GIVEN A FOURTH ARGUMENT BY THE OBJECT and its return value
 * is used, and it has neither.  The extra argument is dead stack setup and is
 * not reproduced -- an argument the callee never loads has no observable
 * effect, which is `V21RX_delete`'s note above and `v22data.c`'s at its own
 * call site.  The value in %eax on return is `agc.signal`, the same quantity
 * the function's last store put in the state, so the field is read here
 * instead; `src/pump/v23/bwchdem.c` records that reading and is tested on it.
 *
 * The squelch loop indexes with an `unsigned short` and compares `jb`
 * (0x0a57be..0x0a57c4), so a count with the top bit set walks forward rather
 * than not at all.
 */
unsigned short
DemodDataV21(void *modem, short *in, short *bits, unsigned short count)
{
	short nsamples;

	FPM_AGC_agc(&V21RX_DSP(modem)->agc, in, count);

	V21RX_DSP(modem)->int_0004 = V21RX_DSP(modem)->agc.signal;
	V21RX_DSP(modem)->int_0008 = 1;

	if (FPM_MTD_detect(V21RX_DSP(modem)->mtd, in, (short)count)
	    != FPM_MTD_ABSENT) {
		V21RX_DSP(modem)->int_0008 = 0;

		if (V21RX_HDX(modem)->handler != RxHdxDataV21) {
			unsigned short i;

			for (i = 0; i < count; i++)
				in[i] = 0;
		}
	}

	nsamples = FPM_MRF_filter(&V21RX_DSP(modem)->mrf, in,
				  V21RX_DSP(modem)->mag, (short)count);

	return (unsigned short)
		FPM_FSD_demodulate(&V21RX_DSP(modem)->fsd,
				   V21RX_DSP(modem)->mag,
				   (unsigned short *)(void *)bits,
				   (unsigned short)nsamples);
}

/* Carrier present: the receive block's two words at once. */
int
CarrierDetectV21(void *modem)
{
	struct v21_rx_dsp *dsp = V21RX_DSP(modem);

	return dsp->int_0008 & dsp->int_0004;
}

/*
 * Rectify the demodulator's trace into the block's own buffer.
 *
 * The three fields are lifted into locals before the loop because the object
 * lifts them: 0x74, 0x70 and 0x90 are all loaded at 0x0a583c..0x0a5843,
 * ahead of the first test.  Read through the struct each time they would not
 * be, since the store into `mag` may alias them.
 *
 * The absolute value is written out rather than called: C's `abs()` is
 * undefined at INT_MIN and this one is reached with -32768, which the object
 * turns into -32768 by truncating 32768 back to a short.  That corner is
 * driven by the differential test rather than reasoned about.
 *
 * THE SECOND LOOP HAS NO BODY IN THE OBJECT, and this is not an omission
 * here.  0x0a5868..0x0a5875 counts from zero to the same bound with nothing
 * between the increment and the test, and the return is a literal zero.  The
 * natural reading is an accumulation whose result became dead before the
 * compiler saw it, but the object does not say that and nothing here claims
 * it.  Reproduced because it is there; it has no observable effect.  D1038.
 */
int
GetSNRV21(void *modem)
{
	struct v21_rx_dsp *dsp = V21RX_DSP(modem);
	short n = dsp->fsd.last_count;
	const short *trace = dsp->fsd.trace;
	short *mag = dsp->mag;
	short i;

	for (i = 0; i < n; i++) {
		int v = trace[i];

		mag[i] = (short)(v < 0 ? -v : v);
	}

	for (i = 0; i < n; i++)
		;

	return 0;
}

/*
 * Modulate, then resample.  The handle is re-read after the modulator
 * returns; see the header.
 */
unsigned short
ModDataV21(void *modem, const unsigned short *bits, short *out,
	   unsigned short nbits)
{
	unsigned short nsamples;

	nsamples = (unsigned short)FPM_FSM_modulate(&V21TX_DSP(modem)->fsm,
						    bits,
						    V21TX_DSP(modem)->scratch,
						    nbits);

	return (unsigned short)FPM_MRF_filter(&V21TX_DSP(modem)->mrf,
					      V21TX_DSP(modem)->scratch, out,
					      (short)nsamples);
}

/*
 * The same with the output scale forced to zero across the modulator, so the
 * modulator's phase and the converter's history advance exactly as they would
 * have and the carrier comes back in phase.  The converter runs with whatever
 * scale it was given, which by then is silence anyway.
 */
unsigned short
TxNoCarrierV21(void *modem, const unsigned short *bits, short *out,
	       unsigned short nbits)
{
	short saved_scale = V21TX_DSP(modem)->fsm.cfg.scale;
	unsigned short nsamples;

	V21TX_DSP(modem)->fsm.cfg.scale = 0;
	nsamples = (unsigned short)FPM_FSM_modulate(&V21TX_DSP(modem)->fsm,
						    bits,
						    V21TX_DSP(modem)->scratch,
						    nbits);
	V21TX_DSP(modem)->fsm.cfg.scale = saved_scale;

	return (unsigned short)FPM_MRF_filter(&V21TX_DSP(modem)->mrf,
					      V21TX_DSP(modem)->scratch, out,
					      (short)nsamples);
}

/*
 * The layout above is a claim about a 32-bit object and is asserted as one.
 * The guard is the tree's usual `__SIZEOF_POINTER__` one; `tools/assertlive.py`
 * is what keeps it from quietly reading `#if 0` under a compiler that does not
 * predefine it.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

/*
 * THE TYPEDEF NAME CARRIES `__LINE__`, AND THAT IS NOT DECORATION.  Naming it
 * after the FIELD alone collides the moment two structures here share a field
 * name, and two of them do: `mrf` is in both `v21_tx_dsp` and `v21_rx_dsp`.
 * GCC 14 accepts an identical typedef redefinition (C11 permits it) and said
 * nothing; GCC 3.4.2 rejects it outright, so `make period` -- the tier that
 * decides -- would not compile this file at all.  A discriminator that cannot
 * repeat is what keeps the next added field from bringing it back.
 */
#define V21_CAT2(a, b)	a##b
#define V21_CAT(a, b)	V21_CAT2(a, b)
#define V21_ASSERT_OFF(type, field, off) \
	typedef char V21_CAT(v21_off_line_, __LINE__)[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V21_ASSERT_OFF(struct v21_tx_dsp, fsm, 0x00);
V21_ASSERT_OFF(struct v21_tx_dsp, mrf, 0x10);
V21_ASSERT_OFF(struct v21_tx_dsp, scratch, 0x2c);

V21_ASSERT_OFF(struct v21_rx_dsp, int_0004, 0x04);
V21_ASSERT_OFF(struct v21_rx_dsp, int_0008, 0x08);
V21_ASSERT_OFF(struct v21_rx_dsp, agc, 0x0c);
V21_ASSERT_OFF(struct v21_rx_dsp, mrf, 0x38);
V21_ASSERT_OFF(struct v21_rx_dsp, fsd, 0x54);
V21_ASSERT_OFF(struct v21_rx_dsp, mtd, 0x8c);
V21_ASSERT_OFF(struct v21_rx_dsp, mag, 0x90);

V21_ASSERT_OFF(struct v21_rx_hdx, handler, 0x04);
V21_ASSERT_OFF(struct v21_rx_hdx, state, 0x08);
V21_ASSERT_OFF(struct v21_rx_hdx, countdown, 0x0a);
V21_ASSERT_OFF(struct v21_rx_hdx, ones_run, 0x0c);
V21_ASSERT_OFF(struct v21_rx_hdx, mark_seq, 0x0e);

V21_ASSERT_OFF(struct v21_status, tx_bps, 0x02);
V21_ASSERT_OFF(struct v21_status, rx_bps, 0x04);
V21_ASSERT_OFF(struct v21_status, quality, 0x06);
V21_ASSERT_OFF(struct v21_status, snr, 0x08);
V21_ASSERT_OFF(struct v21_status, short_0a, 0x0a);
V21_ASSERT_OFF(struct v21_status, short_0c, 0x0c);
V21_ASSERT_OFF(struct v21_status, short_0e, 0x0e);
V21_ASSERT_OFF(struct v21_status, short_10, 0x10);
V21_ASSERT_OFF(struct v21_status, short_12, 0x12);
V21_ASSERT_OFF(struct v21_status, flags, 0x14);
V21_ASSERT_OFF(struct v21_status, flags1, 0x15);
V21_ASSERT_OFF(struct v21_status, int_18, 0x18);

/*
 * The two DSP blocks are gapless: every offset above abuts the next, which is
 * what makes the layout a reading of the object rather than a set of
 * independent guesses.  Asserting the sizes is what would catch a sub-struct
 * changing under us.
 */
typedef char v21_tx_dsp_size[(sizeof(struct v21_tx_dsp) == 0x30) ? 1 : -1];
typedef char v21_rx_dsp_size[(sizeof(struct v21_rx_dsp) == 0x94) ? 1 : -1];

#endif
