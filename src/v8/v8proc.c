/*
 * v8proc.c -- one buffer through the handshake.
 *
 * `V8Process` is the per-sample loop: it drains the transmit ring into the
 * caller's buffer, filters the caller's samples into the symbol buffer, and
 * lets the state machine run whenever the transmit queue is nearly empty or
 * enough symbols have arrived.  Its return value is a status the caller can
 * act on.
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
