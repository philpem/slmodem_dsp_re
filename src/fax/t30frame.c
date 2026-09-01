/*
 * t30frame.c -- Class 1 fax: naming a T.30 frame from its first three octets.
 *
 * Reconstructed from dsplibs.o, in the object's own emission order:
 *
 *   GetT30FrameNameByID     .text 0x096e00      76
 *   GetT30FrameIDFromBuffer .text 0x096e50      84
 *   FrameNames              .rodata 0x9660     288  (36 pairs)
 *
 * plus the 64-byte `.bss` scratch at 0x880 that the object calls `Buffer.0`,
 * which is GCC's spelling for a static named `Buffer` inside a function --
 * so it is written as one.
 *
 * The only caller in the object is `faxvmi_hdlc_frame`'s debug line, which
 * hands the two functions the first three octets of the frame it is about to
 * transmit and prints "FCL1: FRAME TRANSMITTED (%s)".
 */

#include <stddef.h>

#include "dsplib/class1tx.h"
#include "dsplib/sysdep.h"
#include "dsplib/t30frame.h"

/*
 * THE NAMES AND THEIR SPELLINGS ARE THE AUTHOR'S, byte for byte, including
 * the ones that carry no expansion (`RTN`, `PIN`, `NSS`, `CRP`, `DTC`) and
 * the underscores in `PRI_EOM`.  The identifiers are read from the table's
 * own first words; the order is the object's and is not sorted.
 */
static const struct {
	int id;
	char *name;
} FrameNames[36] = {
	{ 0xfe, "TCF - Train" },
	{ 0x00, "NONE - Data" },
	{ 0x01, "DIS - Digital Identification Signal" },
	{ 0x02, "CSI - Called Subscriber Identification" },
	{ 0x04, "NSF - Non-Standard Facilities" },
	{ 0x21, "CFR - Confirmation To Receive" },
	{ 0x22, "FTT - Failure To Train" },
	{ 0x23, "CTR - Response for Continue To Correct" },
	{ 0x31, "MCF - Message Confirmation" },
	{ 0x32, "RTN" },
	{ 0x33, "RTP" },
	{ 0x34, "PIN" },
	{ 0x35, "PIP" },
	{ 0x37, "RNR - Receive Not Ready" },
	{ 0x38, "ERR - Response for End of Retransmission" },
	{ 0x3d, "PPR - Partial Page Request" },
	{ 0x41, "DCS - Digital Command Signal" },
	{ 0x42, "TSI - Transmitting Subscriber Identification" },
	{ 0x44, "NSS" },
	{ 0x48, "CTC - Continue To Correct" },
	{ 0x58, "CRP" },
	{ 0x5f, "DCN - Disconnect" },
	{ 0x71, "EOM - End Of Message" },
	{ 0x72, "MPS - MultiPage Signal" },
	{ 0x73, "EOR - End Of Retransmission" },
	{ 0x74, "EOP - End Of Procedure" },
	{ 0x76, "RR - Receive Ready" },
	{ 0x79, "PRI_EOM - Procedure Interrupt-End Of Message" },
	{ 0x7a, "PRI_MPS - Procedure Interrupt-MultiPage Signal" },
	{ 0x7c, "PRI_EOP - Procedure Interrupt-End Of Procedure" },
	{ 0x7d, "PPS - Partial Page Signal" },
	{ 0x81, "DTC" },
	{ 0x82, "CIG" },
	{ 0x83, "PWD" },
	{ 0x84, "NSC" },
	{ 0xff, "INVALID" },
};

/*
 * A linear search of all 36, then the fallback.  The bound is the object's
 * own `cmp $0x23; ja`, so the last entry examined is index 35 and the table
 * is walked whole; there is no early exit on a sentinel.
 */
char *
GetT30FrameNameByID(int id)
{
	static char Buffer[64];
	unsigned int i;

	for (i = 0; i <= 0x23; i++)
		if (FrameNames[i].id == id)
			return FrameNames[i].name;

	sysdep_sprintf(Buffer, "Unknown frame (ID=0x%02x)", id);
	return Buffer;
}

/*
 * See t30frame.h for what each arm means.  Two details are the object's and
 * are easy to lose:
 *
 *   - the 0x8000 marker is held in a register that is ZEROED at entry and
 *     only ever set on the "other control octet" arm, so the reversed arm ORs
 *     nothing in (0x96e51 against 0x96e85);
 *   - the `> 0x84` test and the mask are SHARED by both arms -- the reversed
 *     arm jumps back into the middle of the other one at 0x96e8a -- so a
 *     reversed FCF is masked exactly as an unreversed one is.
 */
int
GetT30FrameIDFromBuffer(unsigned char address, unsigned char control,
			unsigned char fcf)
{
	unsigned int id;
	int marker = 0;

	if (address != 0xff)
		return 0xff;

	if (control == 0x03 || control == 0x13) {
		id = aReversedCharsArray[fcf];
	} else {
		id = fcf;
		marker = 0x8000;
	}
	if (id > 0x84)
		id &= 0x7f;
	return (int)id | marker;
}
