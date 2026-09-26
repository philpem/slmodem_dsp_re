/*
 * V21t_prc.c -- split out of the merged v21.c so the definitions sit in the
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
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct v21_tx_hdx *hdx;
	unsigned short taken;
	short budget;
	short total;

	hdx = tx->hdx;

	tx->result.byte.flags1 &=
		(unsigned char)~V21TX_RESULT_B1_BIT1;

	if (hdx->int_0004 == 0)
		taken = (unsigned short)FIFO_write(
				hdx->fifo,
				in, *count);
	else
		taken = *count;

	budget = V21TX_MODEM_BUDGET;
	total = 0;
	do {
		short got;

		hdx = tx->hdx;
		got = hdx->handler(modem, in, out, &budget);

		out += got;
		total = (short)(total + got);
	} while (budget > 0);

	if (*count != taken) {
		tx->result.byte.flags1 |= V21TX_RESULT_B1_BIT1;
		/*
		 * A BYTE store into the low byte of the int this function
		 * returns -- `movb $0x4,0x1c(%edi)` at 0x0a2564 -- which is
		 * why it cannot be written through `AT_I`.
		 */
		tx->result.byte.status = V21TX_RESULT_BYTE_04;
	}

	*count = (unsigned short)total;

	return tx->result.word;
}

/*
 * Advance the transmit state machine one step.
 *
 * THE OBJECT CARRIES THIS BLOCK FOUR TIMES, byte for byte: once out of line
 * as `TxNextStateV21` (0x0a25b0, 280 bytes) and three more times inlined,
 * once each into `TxHdxStartV21`, `TxHdxIdleV21` and `TxHdxDataV21` --
 * `RxNextStateV21`'s situation on the transmit side (F8898/F9091), except
 * every caller lands in this same commit, so there is no intermediate
 * `static` step to record.
 *
 * The four strings are the author's own words, out of .rodata.str1.1 at
 * 0x4b60, 0x4b72, 0x4b84 and 0x4b4d; `tools/relocscan.py` pairs them with
 * these sites (finding F604).  They name the CURRENT state at each arm --
 * "V21TX_STATE_DATA" is printed when state == V21TX_STATE_DATA, which then
 * installs `TxHdxIdleV21` and advances to V21TX_STATE_IDLE -- exactly as
 * `RxNextStateV21`'s strings name the state being LEFT, not the one entered.
 *
 * The default arm is reached for any state outside 0..2; it installs no
 * handler, only reports V21TX_STATUS_DEFAULT and clears
 * `V21TX_RESULT_B2_BIT0` while setting `V21TX_RESULT_B1_BIT1` and clearing
 * `V21TX_RESULT_B1_BIT0`.
 */
void
TxNextStateV21(void *modem)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct v21_tx_hdx *hdx = tx->hdx;
	short state = hdx->state;

	switch (state) {
	case V21TX_STATE_DATA:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21TX_STATE_DATA\n");
		hdx->handler = TxHdxIdleV21;
		hdx->state = V21TX_STATE_IDLE;
		tx->result.byte.flags2 |= V21TX_RESULT_B2_BIT0;
		tx->result.byte.flags1 &=
			(unsigned char)~V21TX_RESULT_B1_BIT0;
		break;

	case V21TX_STATE_IDLE:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21TX_STATE_IDLE\n");
		hdx->short_000e = 0;
		hdx->handler = TxHdxStartV21;
		hdx->state = V21TX_STATE_START;
		tx->result.byte.flags2 &=
			(unsigned char)~V21TX_RESULT_B2_BIT0;
		tx->result.byte.flags1 |= V21TX_RESULT_B1_BIT0;
		break;

	case V21TX_STATE_START:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21TX_STATE_START\n");
		hdx->short_000e = 0;
		hdx->handler = TxHdxDataV21;
		hdx->state = V21TX_STATE_DATA;
		tx->result.byte.flags2 &=
			(unsigned char)~V21TX_RESULT_B2_BIT0;
		tx->result.byte.flags1 |= V21TX_RESULT_B1_BIT0;
		break;

	default:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V21TX_DEFAULT, %d\n", state);
		tx->result.byte.flags2 &=
			(unsigned char)~V21TX_RESULT_B2_BIT0;
		tx->result.byte.status = V21TX_STATUS_DEFAULT;
		tx->result.byte.flags1 = (unsigned char)
			((tx->result.byte.flags1
			  | V21TX_RESULT_B1_BIT1)
			 & ~V21TX_RESULT_B1_BIT0);
		break;
	}
}

/*
 * The START state, installed by `V21TX_create` and by IDLE's own transition.
 *
 * Reads up to `*budget` elements out of the FIFO into `in`, modulates
 * whatever it got, decrements `*budget` by the amount actually taken, and
 * ALWAYS advances the state machine and reports V21TX_STATUS_START --
 * unconditionally, on every path, which is why the status write sits after
 * `TxNextStateV21` rather than inside any one of its arms: it overwrites
 * whatever that call itself wrote, including its own default-arm status.
 */
short
TxHdxStartV21(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct v21_tx_hdx *hdx = tx->hdx;
	unsigned short taken;
	short nsamples;

	taken = (unsigned short)
		FIFO_read(hdx->fifo,
			  in, (unsigned short)*budget);
	*budget = (short)((unsigned short)*budget - taken);

	nsamples = (short)ModDataV21(modem, in, out, taken);

	TxNextStateV21(modem);
	tx->result.byte.status = V21TX_STATUS_START;

	return nsamples;
}

/*
 * The IDLE state, installed only by `TxNextStateV21`'s own V21TX_STATE_DATA
 * arm.
 *
 * With the FIFO empty, asks `TxNoCarrierV21` for the WHOLE current budget in
 * one call -- not a per-block amount -- forces `*budget` to zero and reports
 * V21TX_STATUS_IDLE.  With the FIFO non-empty, does not modulate at all:
 * calls `TxNextStateV21` and returns 0, leaving `*budget` untouched for the
 * newly-installed handler to consume on the SAME caller block (`V21TX_modem`'s
 * `do`/`while` runs the dispatch slot again while `budget > 0`).
 */
short
TxHdxIdleV21(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct fax_fifo *fifo = tx->hdx->fifo;

	if (fifo->count == 0) {
		unsigned short b = (unsigned short)*budget;
		short nsamples = (short)TxNoCarrierV21(modem, in, out, b);

		*budget = (short)((unsigned short)*budget - b);
		tx->result.byte.status = V21TX_STATUS_IDLE;
		return nsamples;
	}

	TxNextStateV21(modem);
	return 0;
}

/*
 * The DATA state, installed only by `TxNextStateV21`'s own V21TX_STATE_START
 * arm.
 *
 * Reads up to `*budget` elements.  THREE ARMS:
 *
 *   - satisfied (the FIFO supplied the whole request): modulate `taken`,
 *     zero `*budget` (it equals `taken` by construction), report
 *     V21TX_STATUS_DATA.
 *   - underrun, `V21TXP_INT_0004 == 0`: modulate the FULL REQUESTED BUDGET
 *     regardless of what the FIFO actually supplied, force `*budget` to
 *     zero, and report V21TX_STATUS_DATA -- after MOMENTARILY reporting
 *     V21TX_STATUS_UNDERRUN, which the object writes and then immediately
 *     overwrites at the same call (D1240; reproduced because it is there and
 *     has no observable effect on any interface this file exposes).
 *   - underrun, `V21TXP_INT_0004 != 0`: modulate only `taken`, LEAVE the
 *     remainder in `*budget` (do not force it to zero), call
 *     `TxNextStateV21`, and report whatever that installed.
 */
short
TxHdxDataV21(void *modem, unsigned short *in, short *out, short *budget)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	struct v21_tx_hdx *hdx = tx->hdx;
	unsigned short req = (unsigned short)*budget;
	unsigned short taken;
	unsigned short nbits;
	short nsamples;

	taken = (unsigned short)
		FIFO_read(hdx->fifo,
			  in, req);

	if (req <= taken) {
		nbits = taken;
		nsamples = (short)ModDataV21(modem, in, out, nbits);
		*budget = (short)((unsigned short)*budget - nbits);
		tx->result.byte.status = V21TX_STATUS_DATA;
		return nsamples;
	}

	if (hdx->int_0004 != 0) {
		nbits = taken;
		nsamples = (short)ModDataV21(modem, in, out, nbits);
		*budget = (short)((unsigned short)*budget - taken);
		TxNextStateV21(modem);
		return nsamples;
	}

	tx->result.byte.flags1 |= V21TX_RESULT_B1_BIT1;
	nbits = req;
	tx->result.byte.status = V21TX_STATUS_UNDERRUN;
	nsamples = (short)ModDataV21(modem, in, out, nbits);
	*budget = (short)((unsigned short)*budget - nbits);
	tx->result.byte.status = V21TX_STATUS_DATA;

	return nsamples;
}
