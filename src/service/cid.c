/*
 * cid.c -- Caller ID: the service object's small setters and the TLV walkers.
 *
 * Reconstructed from dsplibs.o cid.c (finding F1410 for the TU map):
 *
 *   cid_threshold         .text 0x08fdd0    40
 *   cid_value             .text 0x08fe00    15
 *   _look_for             .text 0x0903c0    72
 *   _look_for_other_than  .text 0x090410    88
 *
 * All four are exported API with no internal referrer (finding F8320's
 * bucket), written here on their own merit.  The rest of the TU --
 * cid_reset, cid_freq_sampl, cid_create, cid_delete, cid_progress,
 * cid_get_strings -- waits for the CID service pass: cid_reset alone was in
 * this batch's scope but calls the unreconstructed `reset_cid` (Rxcid.c),
 * and this tree links no scaffold, so it is left out rather than written
 * half-connected.  See docs/findings.md F8492.
 *
 * See include/dsplib/cid_modem.h for the object and the mode encoding.
 */

#include "dsplib/cid_modem.h"
#include "dsplib/cid.h"
#include "dsplib/dtmf_rx.h"

/*
 * Both writes are gated on the mode, with the same two tests cid_create and
 * cid_reset use: `!= 0` reaches the DTMF side, `!= 1` the FSK side, so mode
 * 5 sets both.  The object stores the int argument as a short on both sides.
 */
void
cid_threshold(struct cid_modem *ctx, int thr)
{
	if (ctx->mode != 0)
		ctx->dtmf->sens = (short)thr;
	if (ctx->mode != 1)
		ctx->fsk->f028 = (short)thr;
}

void
cid_value(struct cid_modem *ctx, int v)
{
	ctx->f264 = v;
}

/*
 * Walk the TLV entries: buf[2] is the first tag, buf[pos + 1] each entry's
 * length, and the next entry starts at pos + length + 2.  The scan bound is
 * buf[1], the message length, read once as a SIGNED char; positions live in
 * shorts.  Both are the object's widths -- a length byte >= 0x80 steps the
 * position BACKWARDS here, exactly as it does there.
 */
int
_look_for(const char *buf, char tag)
{
	short len = buf[1];
	short pos = 2;

	while (pos < len) {
		if (buf[pos] == tag)
			return pos;
		pos = (short)(pos + buf[pos + 1] + 2);
	}
	return -1;
}

/*
 * The same walk from `start`, returning the first entry whose tag is none
 * of 1, 2 or 7 and whose position is not `except`.  The object tests 1 and
 * 7 together (two setne into one test) and 2 apart; behaviourally one
 * exclusion set.
 */
int
_look_for_other_than(const char *buf, short start, int except)
{
	short len = buf[1];
	short pos = start;

	while (pos < len) {
		char tag = buf[pos];

		if (tag != 1 && tag != 7 && tag != 2 && pos != except)
			return pos;
		pos = (short)(pos + buf[pos + 1] + 2);
	}
	return -1;
}
