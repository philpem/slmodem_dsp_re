/*
 * v8sig.c -- the handshake control entry.
 *
 * What remains here is V8Control, the out-of-band request handler.  The
 * staging copies and receive AGC are `V8global.c`'s; the tone and
 * phase-reversal detector is `V8Detector.c`'s; and the V.21 FSK modulator,
 * demodulator and shaping filter are `V8Fsk.c`'s and `V8Dpsk.c`'s, which is
 * where the object keeps them.
 */

#include "dsplib/debug.h"
#include "dsplib/v8.h"

/*
 * Nudge the handshake from outside.
 *
 * Each request is accepted only from the one state it makes sense in, and
 * refused otherwise -- there is no queueing and no error beyond the return
 * value, so a caller that asks at the wrong moment simply gets -1.
 */
int
V8Control(struct v8 *v, int what)
{
	int rc;

	switch (what) {
	case V8CTRL_START_CM:
		if (v->side != 0 || v->rx_state != 0x19 || v->cm_ready != 0) {
			rc = -1;
		} else {
			v->cm_ready = 1;
			rc = 0;
		}
		break;

	case V8CTRL_START_CJ:
		if (v->rx_substate != V8_HS_TAKEN_RX) {
			rc = -1;
		} else {
			v->rx_substate = V8_HS_DRAIN;
			rc = 0;
		}
		break;

	case V8CTRL_START_JM:
		if (v->rx_substate != V8_HS_TAKEN_TX) {
			rc = -1;
		} else {
			v->rx_substate = V8_HS_CJ;
			v->elapsed = 0;
			v->tx_state = 0x17;
			rc = 0;
		}
		break;

	default:
		/*
		 * The only exit that does not name the request: there is no
		 * name to give it, so it reports the number instead -- and it
		 * is the one message here that ends \r\n.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: V8Control called with currently not "
			    "supported control type (type=%d)\r\n", what);
		return -1;
	}

	/*
	 * Announced whether it was accepted or refused -- the object has two
	 * copies of this, one per return value, sharing the one call.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V8: V8Control called - control type is %s\n",
		    v8ControlName[what]);
	return rc;
}
