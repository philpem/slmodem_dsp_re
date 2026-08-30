/*
 * data.c -- Caller ID: the raw-message hex dumper.
 *
 * Reconstructed from dsplibs.o Data.c (finding F1410 pins the TU: the three
 * `data_*` functions from 0x090470 are a complete file of their own):
 *
 *   data_raw   .text 0x090470   117
 *
 * `data_unformatted_output` and `data_formatted_output` complete the TU and
 * are the CID service pass's; nothing here calls them.
 *
 * The dump covers buf[1] + 2 bytes -- the two-byte TLV header plus the
 * declared length -- capped at 0xf5 = 245 BYTES, i.e. 490 output digits, so
 * the cap protects a 512-byte output buffer with a terminator to spare.
 * Digits above 9 are lowercase ('a' is 0x57 + 10).
 */

#include "dsplib/cid_modem.h"

void
data_raw(const char *buf, char *out)
{
	int n = buf[1] + 2;
	int i;

	if (n > 0xf5)
		n = 0xf5;

	for (i = 0; i < n; i++) {
		unsigned char hi = (unsigned char)buf[i] >> 4;
		unsigned char lo = (unsigned char)buf[i] & 0x0f;

		out[2 * i] = (char)(hi > 9 ? hi + 0x57 : hi + 0x30);
		out[2 * i + 1] = (char)(lo > 9 ? lo + 0x57 : lo + 0x30);
	}
	out[2 * i] = '\0';
}
