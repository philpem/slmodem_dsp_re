/*
 * v8proc.c -- one buffer through the handshake.
 *
 * `V8Process` is the per-sample loop: it drains the transmit ring into the
 * caller's buffer, filters the caller's samples into the symbol buffer, and
 * lets the state machine run whenever the transmit queue is nearly empty or
 * enough symbols have arrived.  Its return value is a status the caller can
 * act on.
 *
 * `v8_process` is the datapump operation on top of it, turning that status
 * into a DPSTAT_* code and, when the negotiation finishes, publishing the
 * result and asking the modem to change datapump.
 */

#include <stdint.h>

#include "dsplib/debug.h"
#include "dsplib/v8dp.h"
#include "dsplib/dp.h"
#include "dsplib/modem_params.h"

extern long modem_set_param(void *modem, unsigned param, int value);

/* The receive filter's one pole, in Q12. */
#define V8_RX_POLE	0xf85

int
V8Process(struct v8 *v, const short *in, short *out, int count)
{
	int status = 0;
	int changed = 0;
	int i;

	for (i = 0; i < count; i++) {
		int x;
		int c;

		/* One sample out of the transmit ring. */
		v->tx_avail = (short)(v->tx_avail - 1);
		*out++ = *v->tx_ring_base++;
		if (v->tx_ring_base >= v->tx_ring + V8_TX_RING_END)
			v->tx_ring_base = v->tx_ring;

		/*
		 * And one in, through a single pole, into the symbol buffer.
		 * The imaginary half is written as zero: what arrives is
		 * real, and the demodulator expects pairs.
		 */
		x = *in++;
		c = (short)(x + (unsigned short)v->pole_state);
		v->pole_state = (short)((c * V8_RX_POLE - (x << 12)) >> 12);
		v->tx_sym_b[0] = (short)c;
		v->tx_sym_b[1] = 0;
		v->tx_sym_b += 2;
		if (v->tx_sym_b >= v->tx_symbols + V8_TX_SYMBOLS)
			v->tx_sym_b = v->tx_symbols;
		v->sym_avail = (short)(v->sym_avail + 1);

		/* Run the machine when there is room to send or work to do. */
		if ((short)v->tx_avail <= 4 || (short)v->sym_avail > 4) {
			if ((short)v8handshak(v) == 2)
				changed = 1;
		}
	}

	/*
	 * The status.  Which state variable decides depends on the shape of
	 * handshake, and in the answering shape the receive state can
	 * overwrite what the transmit state chose.
	 */
	if (v->side == 0) {
		if (v->tx_state == 5 && v->rx_state == 0x19 && v->rx_substate == 0x19)
			status = V8_ORG_WAITING_FOR_ANSAM;
		else if ((unsigned short)v->rx_substate == 0x24)
			status = V8_ORG_ANSAM_DETECTED_WAITING_TE;
		else if ((unsigned short)v->tx_state == 0x17)
			status = v->tx_seq == &v->seq[1]
				 ? V8_ORG_SEND_CJ
				 : V8_ORG_SEND_CM + (v->rx_substate == V8_HS_TAKEN_RX);
		else if ((unsigned short)v->tx_state == 0x2b)
			status = V8_ORG_SEND_QC;
		else if ((unsigned short)v->rx_state == 0xb)
			status = V8_ORG_TIME_OUT_WAITING_FOR_ANSAM;
		else if ((unsigned short)v->rx_state == 0xc)
			status = V8_ORG_TIME_OUT_WAITING_FOR_JM;
	} else {
		if ((unsigned short)v->tx_state == 6)
			status = V8_ANS_SEND_ANSAM
				 + (v->rx_substate == V8_HS_TAKEN_TX);
		else if ((unsigned short)v->tx_state == 0x17)
			status = V8_ANS_SEND_JM;

		if ((unsigned short)v->rx_state == 4)
			status = V8_ANS_TIME_OUT_WAITING_FOR_CM;
		else if ((unsigned short)v->rx_state == 5)
			status = V8_ANS_TIME_OUT_WAITING_FOR_CJ;
	}

	if (changed)
		status = V8_OK;

	/*
	 * The conditional store is a change detector: this is the one place
	 * the whole negotiation is narrated, and `feb8` exists to hold the
	 * previous status so that it can be.
	 */
	if (v->prev_status != status) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: State changed from %s to %s\r\n",
			    v8StatusName[v->prev_status], v8StatusName[status]);
		v->prev_status = status;
	}
	return status;
}

int
v8_process(struct dp *dp, void *in, void *out, int count)
{
	struct v8_dp *st = ((struct v8_dp *)dp)->self;
	int rc = V8Process(st->v8, in, out, count);
	int ret = 0;
	int arg = -1;

	switch (rc) {
	/* Every status whose name ends TIME_OUT_WAITING_FOR_something. */
	case V8_ANS_TIME_OUT_WAITING_FOR_CM:
	case V8_ANS_TIME_OUT_WAITING_FOR_CJ:
	case V8_ORG_TIME_OUT_WAITING_FOR_ANSAM:
	case V8_ORG_TIME_OUT_WAITING_FOR_JM:
	case V8_ORG_TIME_OUT_WAITING_FOR_QCA1d:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("v8: process: timeout.\n");
		ret = DPSTAT_ERROR;
		break;

	case V8_OK:
		/* Publish what was agreed, then ask for the change. */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("v8: process: OK.\n");
		/*
		 * ...but only while the idle timer is not already running.
		 * Once a change has been asked for, `f20` is counting down to
		 * it and a second V8_OK must not start over.
		 */
		if (st->f20 != 0)
			break;
		V8UpdateModemParameters(st->v8, st->cm);

		/*
		 * Which datapump comes next.  Quick connect keeps whatever the
		 * call asked for; otherwise it is whichever modulation
		 * survived the negotiation, most capable first -- the same
		 * three bits of `b0` that V8Create prints as V90, V34 and V32
		 * (finding F164), and the datapump ids are the standard
		 * numbers.  Nothing left means nothing to change to.
		 */
		if (st->cm->b2 & 0x10) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("v8: process: QC.\n");
			arg = st->want;
		} else if (st->cm->b0 & 0x08) {
			arg = DP_V90;
		} else if (st->cm->b0 & 0x20) {
			arg = DP_V34;
		} else if (st->cm->b0 & 0x80) {
			arg = DP_V32;
		} else {
			ret = DPSTAT_ERROR;
			break;
		}

		/* Common to all four: what was agreed goes to the modem. */
		st->dspinfo->f08 = (st->cm->b2 >> 6) & 1;
		st->dspinfo->f0c = st->cm->menu;
		break;

	case V8_ORG_BAD_QCA1d_MESSAGE:
		/*
		 * The far end offered PCM.  Only take it if this call asked
		 * for V.90 or V.92, and only once.  Nothing reaches it: the
		 * object's OWN V8Process has no arm that produces 15, 16 or
		 * 17 either -- its status chain at 0x74680 is the one above,
		 * arm for arm -- so the three QCA1d statuses exist in the
		 * table and in this switch and nowhere else.  Reproduced.
		 */
		if (st->want != 92 && st->want != 90) {
			ret = DPSTAT_ERROR;
		} else if (st->f20 == 0) {
			st->dspinfo->f08 &= 1;
			arg = 92;
		}
		break;

	default:
		break;
	}

	if (arg >= 0) {
		modem_set_param(dp->modem, 9, arg);
		ret = DPSTAT_CHANGEDP;
		st->f20 = (int)modem_get_param(dp->modem, 5) + 0x2a0;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "v8: Link established. Idle timer %d.\n",
			    st->f20);
	}

	/* The same change detector as V8Process's, one layer up. */
	if (st->f2c != rc) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("v8: status (%d) %s\n", rc,
					     v8StatusName[rc]);
		st->f2c = rc;
	}

	if (st->f20 > 0) {
		st->f20 -= count;
		if (st->f20 <= 0) {
			/* The window closed: give up and change anyway. */
			st->f20 = -1;
			st->f1c = 0;
			modem_set_param(dp->modem, 9, 0);
			ret = DPSTAT_CHANGEDP;
		}
	}
	return ret;
}
