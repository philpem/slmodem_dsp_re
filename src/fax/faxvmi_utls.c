/*
 * faxvmi_utls.c -- Class 1 fax: the VMI's FCS and bit-reversal utilities.
 *
 * Split out of the merged faxvmi.c; the FILE is faxvmi_utls.c, between
 * faxvmi_tbls.c and faxvmififo.c; its two functions fill [0x96780, 0x96850)
 * contiguously.  Bodies moved verbatim (finding F11399).
 */
#include <stddef.h>

#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/nulldp.h"
#include "dsplib/sysdep.h"
#include "dsplib/t30frame.h"

/*
 * The framing layer's three pure buffer walks.
 *
 * All three count DOWN from `count` and stop at zero, so a negative count
 * runs 65536 + count times before the 16-bit counter reaches zero rather
 * than not running at all.  That is the object's and is reproduced; D1052.
 */

/*
 * Two nibble steps per element, high nibble first.  Written as the object
 * computes it: `t` is the polynomial multiplicand already shifted into bits
 * 15..12, so `t >> 11` is the x^5 term, `t >> 12` is the x^0 term, and `t`
 * itself is the x^12 one.  See faxvmi.h for the derivation of 0x1021.
 */
#define FCS16_NIBBLE(fcs, shifted)					\
	do {								\
		unsigned int t_ = ((shifted) ^ (fcs)) & 0xf000u;	\
		(fcs) = (unsigned short)					\
			(((((fcs) ^ (t_ >> 11)) << 4) ^ t_) | (t_ >> 12)); \
	} while (0)

int
faxvmi_gen_fcs16(unsigned short *buf, short count)
{
	unsigned short fcs = 0xffff;
	short i;

	for (i = count; i != 0; i--) {
		unsigned int octet = *buf++;

		FCS16_NIBBLE(fcs, octet << 8);
		FCS16_NIBBLE(fcs, octet << 12);
	}
	return (unsigned short)~fcs;
}

void
faxvmi_byte_reverse(unsigned short *buf, short count)
{
	short i;

	for (i = count; i != 0; i--) {
		unsigned short in = *buf;
		unsigned short out = 0;
		short bit;

		for (bit = 7; bit >= 0; bit--) {
			out = (unsigned short)((out << 1) | (in & 1));
			in >>= 1;
		}
		*buf++ = out;
	}
}
