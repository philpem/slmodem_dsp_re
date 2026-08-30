/*
 * voice.c -- the voice modem: one status-mapping leaf.
 *
 * Reconstructed from dsplibs.o voice.c#3 (the cluster around voice_modem,
 * 0x0abe20..0x0ac95f):
 *
 *   _handle_status   .text 0x0ac7d0   44
 *
 * Finding F8320's no-entry-point bucket.  See include/dsplib/voice.h for
 * why its two scope-mates stayed out and what the mapping is; the codes
 * themselves are named by nothing in the object, so they stay literal
 * here rather than wearing invented names.
 */

#include "dsplib/voice.h"

int
_handle_status(int status, int code)
{
	if (code == 1)
		return 10;
	if (code == 2)
		return 11;
	if (code == 4)
		return 12;
	return status;
}
