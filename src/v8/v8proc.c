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
		v->f21c = (short)(v->f21c - 1);
		*out++ = *v->tx_ring_base++;
		if (v->tx_ring_base >= v->tx_ring + V8_TX_RING_END)
			v->tx_ring_base = v->tx_ring;

		/*
		 * And one in, through a single pole, into the symbol buffer.
		 * The imaginary half is written as zero: what arrives is
		 * real, and the demodulator expects pairs.
		 */
		x = *in++;
		c = (short)(x + (unsigned short)v->fdba);
		v->fdba = (short)((c * V8_RX_POLE - (x << 12)) >> 12);
		v->tx_sym_b[0] = (short)c;
		v->tx_sym_b[1] = 0;
		v->tx_sym_b += 2;
		if (v->tx_sym_b >= v->tx_symbols + V8_TX_SYMBOLS)
			v->tx_sym_b = v->tx_symbols;
		v->f110 = (short)(v->f110 + 1);

		/* Run the machine when there is room to send or work to do. */
		if ((short)v->f21c <= 4 || (short)v->f110 > 4) {
			if ((short)v8handshak(v) == 2)
				changed = 1;
		}
	}

	/*
	 * The status.  Which state variable decides depends on the shape of
	 * handshake, and in the answering shape the receive state can
	 * overwrite what the transmit state chose.
	 */
	if (v->mode == 0) {
		if (v->f9d4 == 5 && v->f9d6 == 0x19 && v->f9d8 == 0x19)
			status = 6;
		else if ((unsigned short)v->f9d8 == 0x24)
			status = 7;
		else if ((unsigned short)v->f9d4 == 0x17)
			status = v->tx_seq == &v->seq[1] ? 0xa
				 : 8 + (v->f9d8 == 0x32);
		else if ((unsigned short)v->f9d4 == 0x2b)
			status = 0xe;
		else if ((unsigned short)v->f9d6 == 0xb)
			status = 0xb;
		else if ((unsigned short)v->f9d6 == 0xc)
			status = 0xc;
	} else {
		if ((unsigned short)v->f9d4 == 6)
			status = 1 + (v->f9d8 == 0x33);
		else if ((unsigned short)v->f9d4 == 0x17)
			status = 3;

		if ((unsigned short)v->f9d6 == 4)
			status = 4;
		else if ((unsigned short)v->f9d6 == 5)
			status = 5;
	}

	if (changed)
		status = 0xd;
	if (v->feb8 != status)
		v->feb8 = status;
	return status;
}

/* Which statuses mean what to the datapump layer. */
#define V8_ST_FAILED_LO		4
#define V8_ST_NEGOTIATED	13
#define V8_ST_PCM		16

int
v8_process(struct dp *dp, void *in, void *out, int count)
{
	struct v8_dp *st = ((struct v8_dp *)dp)->self;
	int rc = V8Process(st->v8, in, out, count);
	int ret = 0;
	int arg = -1;

	switch (rc) {
	case 4: case 5: case 11: case 12: case 17:
		ret = DPSTAT_ERROR;
		break;

	case V8_ST_NEGOTIATED:
		/* Publish what was agreed, then ask for the change. */
		V8UpdateModemParameters(st->v8, st->cm);
		if (st->cm->b2 & 0x10) {
			st->dspinfo->f08 = (st->cm->b2 >> 6) & 1;
			st->dspinfo->f0c = st->cm->menu;
			arg = st->want;
		}
		break;

	case V8_ST_PCM:
		/*
		 * The far end offered PCM.  Only take it if this call asked
		 * for V.90 or V.92, and only once.
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
	}

	if (st->f2c != rc)
		st->f2c = rc;

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
