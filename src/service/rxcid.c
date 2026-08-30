/*
 * rxcid.c -- Caller ID: the async framer over the FSK bit stream.
 *
 * Reconstructed from dsplibs.o Rxcid.c (finding F1410 pins the TU as
 * reset_cid, create_cid, pack_next_bit, cid_modem):
 *
 *   pack_next_bit   .text 0x091b80   301
 *
 * The other three are the CID service pass's; pack_next_bit alone is
 * exported API with no internal referrer (finding F8320) and is written on
 * its own merit.
 *
 * WHAT THE MACHINE IS.  CID_FSD_demodulate emits one short per bit;
 * cid_modem feeds them here one at a time, and bytes accumulate in
 * cid->data.  Four states:
 *
 *   0  hunting carrier.  A mark (bit == 1 EXACTLY -- any other value
 *      counts as space, including values above 1) steps mark_bal up, a
 *      space steps it down; sixteen net marks arm state 3 and clear the
 *      accumulator.
 *   3  channel seizure.  A run of f028 + 1 consecutive spaces enters the
 *      byte collector -- but the object BACKDATES pack_pos to f028 rather
 *      than clearing it, so the first byte's bits land at positions
 *      f028..7 and the low positions keep whatever state 0 left in the
 *      accumulator (state 0 cleared it, so zeros).  Kept exactly; see the
 *      deviations note D954.
 *   1  between bytes: wait for a start bit (space), then clear and collect.
 *   2  collect: shift the bit in LSB-first at pack_pos; at position 8 store
 *      the byte and return to state 1.  There is NO stop-bit check and NO
 *      bound on pack_len -- both the object's own shape.
 *
 * The widths matter and are kept: pack_acc accumulates as a short while the
 * bit is shifted as an int, so a bit value other than 0/1 smears into high
 * positions exactly as it does in the object.
 */

#include "dsplib/cid.h"

void
pack_next_bit(short bit, struct cid *cid)
{
	unsigned short limit = 0;

	if (cid->f028)
		limit = (unsigned short)cid->f028;

	if (cid->pack_state == 0) {
		if (bit == 1)
			cid->mark_bal = (short)(cid->mark_bal + 1);
		else
			cid->mark_bal = (short)(cid->mark_bal - 1);
		if (cid->mark_bal > 15) {
			cid->pack_state = 3;
			cid->pack_acc = 0;
			cid->pack_pos = 0;
		}
	} else if (cid->pack_state == 3) {
		if (bit != 0)
			cid->pack_pos = 0;
		else
			cid->pack_pos = (short)(cid->pack_pos + 1);
		if (cid->pack_pos == (int)limit + 1) {
			cid->pack_pos = (short)limit;
			cid->pack_state = 2;
		}
	} else if (cid->pack_state == 1) {
		if (bit == 0) {
			cid->pack_state = 2;
			cid->pack_acc = 0;
			cid->pack_pos = 0;
		}
	} else {
		short pos = cid->pack_pos;
		unsigned short acc = (unsigned short)cid->pack_acc;

		acc = (unsigned short)(acc | ((int)bit << pos));
		cid->pack_pos = (short)(pos + 1);
		cid->pack_acc = (short)acc;
		if ((short)(pos + 1) == 8) {
			short idx = cid->pack_len;

			cid->pack_state = 1;
			cid->pack_len = (short)(idx + 1);
			cid->data[idx] = (unsigned char)acc;
		}
	}
}
