/*
 * v34pcmif.c -- the V.90/K56Flex side's hooks into the V.34 machinery.
 *
 * A cluster of very small functions at .text+0xa6e0 onwards, separate from
 * V34RX.c and from the handshake: the PCM modem's view of what V.34 is
 * doing.  Reconstructed one at a time as the V.34 code that calls them
 * arrives, rather than as a module, because that is the order the call
 * graph gives (tools/callgraph.py).
 *
 * They also fix the object's real extent.  `VPcmV34LogTimingOffset` writes
 * at +0xac0c, past where v34fsk.h had the struct ending -- so the V.34
 * object is at least 0xac0e bytes and the tail of it belongs to this
 * interface rather than to the datapump.
 */

#include "dsplib/v34fsk.h"
#include "dsplib/v34pcmif.h"

void
VPcmV34LogTimingOffset(void *objp, short offset)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->fac0c = offset;
}
