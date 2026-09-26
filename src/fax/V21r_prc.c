/*
 * V21r_prc.c -- split out of the merged v21.c so the definitions sit in the
 * translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
 */
#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v21fax.h"

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
	struct v21_rx *rx = (struct v21_rx *)modem;
	unsigned short remaining;
	short total = 0;
	int word;

	rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_ERROR;

	remaining = (unsigned short)*count;
	do {
		short before = (short)remaining;
		short n;

		n = rx->hdx->handler(modem, in, out, count);
		remaining = (unsigned short)*count;

		out += n;
		in += before - remaining;
		total = (short)(total + n);
	} while (remaining != 0);

	*count = total;

	memcpy(&word, &rx->status, sizeof word);
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
	struct v21_rx *rx = (struct v21_rx *)modem;
	rx->status.byte.flags |= V21RX_FLAG_ERROR;

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
	struct v21_rx *rx = (struct v21_rx *)modem;
	DemodDataV21(modem, in, out, (unsigned short)*count);
	*count = 0;

	rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_CARRIER;
	rx->status.byte.status = V21RX_STATUS_IDLE;

	if (CarrierDetectV21(modem))
		rx->status.byte.flags |= V21RX_FLAG_CARRIER;

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
	struct v21_rx *rx = (struct v21_rx *)modem;
	struct v21_rx_hdx *hdx = rx->hdx;

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
		rx->status.byte.flags |= V21RX_FLAG_DATA;
		break;

	case V21RX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RX_STATE_DATA\n");
		hdx->handler = RxHdxIdleV21;
		hdx->state = V21RX_STATE_IDLE;
		hdx->countdown = 0;
		hdx->int_0000 = 0;
		rx->status.byte.flags1 |= V21RX_FLAG1_IDLE;
		rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_DATA;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21RX_DEFAULT, %d\n", hdx->state);
		rx->status.byte.flags1 &= (unsigned char)~V21RX_FLAG1_IDLE;
		rx->status.byte.status = V21RX_STATUS_DEFAULT;
		rx->status.byte.flags = (unsigned char)
			((rx->status.byte.flags | V21RX_FLAG_ERROR)
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
	struct v21_rx *rx = (struct v21_rx *)modem;
	struct v21_rx_hdx *hdx;
	unsigned short nbits;
	unsigned short i;

	rx->status.byte.status = V21RX_STATUS_START;

	nbits = DemodDataV21(modem, in, out, (unsigned short)*count);
	*count = 0;

	hdx = rx->hdx;

	i = 0;
	while (nbits != 0) {
		if (hdx->ones_run == V21RX_MARK_RUN && out[i] == 0)
			hdx->mark_seq = (unsigned short)(hdx->mark_seq + 1);

		hdx->ones_run = (unsigned short)
			(out[i] != 0 ? hdx->ones_run + 1 : 0);

		nbits = (unsigned short)(nbits - 1);
		i = (unsigned short)(i + 1);
	}

	rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_CARRIER;

	if (!CarrierDetectV21(modem))
		return 0;

	hdx = rx->hdx;
	if ((short)hdx->mark_seq <= V21RX_MARK_SEQ_THRESHOLD)
		return 0;

	rx->status.byte.flags |= V21RX_FLAG_CARRIER;
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
	struct v21_rx *rx = (struct v21_rx *)modem;
	unsigned short nbits;

	nbits = DemodDataV21(modem, in, out, (unsigned short)*count);
	*count = 0;

	if (!CarrierDetectV21(modem)) {
		rx->hdx->handler = RxHdxErrorV21;
		rx->hdx->state = V21RX_STATE_ERROR;
		rx->status.byte.status = V21RX_STATUS_ERROR;
		rx->status.byte.flags = (unsigned char)
			((rx->status.byte.flags | V21RX_FLAG_ERROR)
			 & ~V21RX_FLAG_CARRIER);
		return 0;
	}

	rx->status.byte.flags |= V21RX_FLAG_CARRIER;
	rx->status.byte.status = V21RX_STATUS_WAIT;

	rx->hdx->countdown = (unsigned short)(rx->hdx->countdown - 1);
	if ((short)rx->hdx->countdown > 0)
		return 0;

	rx->status.byte.status = V21RX_STATUS_TIMEOUT;
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
	struct v21_rx *rx = (struct v21_rx *)modem;
	unsigned short nbits;

	rx->status.byte.flags |= V21RX_FLAG_CARRIER;
	rx->status.byte.status = V21RX_STATUS_DATA;

	if (CarrierDetectV21(modem) && rx->hdx->int_0000 == 0) {
		nbits = DemodDataV21(modem, in, out, (unsigned short)*count);
		*count = 0;

		rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_LOW_SNR;
		if ((short)GetSNRV21(modem) <= V21RX_SNR_THRESHOLD)
			rx->status.byte.flags |= V21RX_FLAG_LOW_SNR;

		return (short)nbits;
	}

	rx->status.byte.flags &= (unsigned char)~V21RX_FLAG_CARRIER;
	RxNextStateV21(modem);

	return 0;
}
