/*
 * V29r_stc.c -- split out of the merged v29.c / v29data.c so the definitions sit in
 * the translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
 */
#include <stddef.h>
#include <string.h>

#include "dsplib/v29fax.h"
#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v29cfg.h"
#include "dsplib/v29data.h"

/*
 * ---------------------------------------------------------------------------
 * V29RX_control -- .text 0x0a4580, 110 bytes.  See v29fax.h for the four
 * effects, the request type's derivation and the two corrections it makes to
 * V29DET_INT_0008 and to V29RX_INT_0000/V29RX_INT_0020's writer sets.
 */
int
V29RX_control(void *modem, const struct v29rx_control_req *req)
{
	if (req == 0)
		return 0;

	((struct v29_rx *)modem)->cfg.int_0008 = req->int_0004;
	((struct v29_rx *)modem)->det->int_0008 =
		(req->ctl1 & V29RXCTL_CTL1_BIT4) != 0;

	if (req->ctl1 & V29RXCTL_CTL1_BIT1)
		V29RX_create(modem, modem);

	if (req->ctl0 & V29RXCTL_CTL0_BIT3)
		((struct v29_rx *)modem)->rx->int_0000 = 0;
	if (req->ctl0 & V29RXCTL_CTL0_BIT5)
		((struct v29_rx *)modem)->rx->int_0020 = 0;

	return 1;
}

/*
 * ---------------------------------------------------------------------------
 * V29RX_status -- .text 0x0a45f0, 190 bytes.
 *
 * The receiver's half of the status report.  It is NOT a mirror of
 * `V29TX_status`: it reads the receive handle, puts the bit rate in +0x04 and
 * +0x12 rather than +0x02 and +0x10, writes +0x0e rather than +0x0c, and
 * rewrites the flags byte four times rather than twice.
 *
 * THE BIT RATE IS LOADED TWICE, from 0x0a4615 and 0x0a4646, with stores to
 * the report in between.  Two statements, not one value used twice -- the
 * same reasoning `V29TX_status` records for its own double load, and forced
 * for the same reason: `status` and `modem` are unrelated parameters.
 *
 * THE FOUR STORES TO THE FLAGS BYTE ARE ALIASING, NOT REDUNDANCY.  The object
 * flushes +0x14 at 0x0a4657, 0x0a466f, 0x0a4685 and 0x0a46a2, and each flush
 * sits immediately before a load through `modem`.  A compiler that could
 * prove the two blocks disjoint would have emitted one store; this one could
 * not, so the intermediate values are visible to a caller that overlaps them.
 * Spelled through `unsigned char` lvalues, which alias everything and give
 * the modern compiler the same reason to keep them.  `t_v29fax.c` drives the
 * overlapping case rather than assuming it away.
 *
 * AND THE VALUE IT READS OUT OF +0x14 IS DEAD.  All EIGHT bits are determined
 * before the function returns -- 0, 2 and 7 cleared, 4 and 6 set, and 1, 3
 * and 5 assigned from fields -- so the final byte does not depend on what the
 * caller had there.  The object reads it anyway, at 0x0a464e, and the read is
 * not removable for the same aliasing reason the stores are not: it is the
 * source of the three intermediate values that reach memory.  So this reads
 * as a merge and behaves as an assignment, which is the opposite of
 * `V29TX_status`, which reads as an assignment after two pointless clears
 * (D1035).  Deviation D1097; finding F9130.
 *
 * BIT 15 OF THE STATUS WORD IS TESTED AS A BYTE.  The object writes `testb
 * $0x80,0x19(%esi)`, which is GCC's narrowing of `& 0x8000` on the `int` at
 * +0x18 -- exactly as `V29RX_modem`'s `andb $0xfd,0x19` is its narrowing of
 * `&= ~0x200`.  The word is spelled as the `int` it is at both sites.
 */
int
V29RX_status(void *modem, void *status)
{
	if (status == 0)
		return 0;

	((struct v29_status_prefix *)status)->protocol =
		(short)((unsigned short)((struct v29_rx *)modem)->cfg.protocol);
	((struct v29_status_prefix *)status)->tx_bps = 0;
	((struct v29_status_prefix *)status)->short_04 =
		(short)((unsigned short)((struct v29_rx *)modem)->cfg.bit_rate);
	((struct v29_status_prefix *)status)->quality = (short)
		((((struct v29_rx *)modem)->result.word & V29_STATUS_LOW_SNR) == 0);
	((struct v29_status_prefix *)status)->short_08 = GetSNRV29(modem);
	((struct v29_status_prefix *)status)->short_0a = 0;
	((struct v29_status_prefix *)status)->short_0e = 0;
	((struct v29_status_prefix *)status)->short_10 = 0;
	((struct v29_status_prefix *)status)->short_12 =
		(short)((unsigned short)((struct v29_rx *)modem)->cfg.bit_rate);

	((struct v29_status_prefix *)status)->flags &= (unsigned char)~V29STAT_BIT0;
	((struct v29_status_prefix *)status)->flags = (unsigned char)
		((((struct v29_status_prefix *)status)->flags & ~V29STAT_BIT1)
		 | ((((struct v29_rx *)modem)->rx->flags_0018
		     & V29RX_0018_BIT0) << 1));
	((struct v29_status_prefix *)status)->flags &= (unsigned char)~V29STAT_BIT2;
	((struct v29_status_prefix *)status)->flags = (unsigned char)
		((((struct v29_status_prefix *)status)->flags & ~V29STAT_BIT3)
		 | ((((struct v29_rx *)modem)->rx->int_0000 == 0) << 3));
	((struct v29_status_prefix *)status)->flags |= V29STAT_BIT4;
	((struct v29_status_prefix *)status)->flags2 &=
		(unsigned char)~V29STAT_FLAGS2_BIT0;
	((struct v29_status_prefix *)status)->flags = (unsigned char)
		((((struct v29_status_prefix *)status)->flags & ~V29STAT_BIT5)
		 | ((((struct v29_rx *)modem)->rx->int_0020 == 0) << 5));
	((struct v29_status_prefix *)status)->flags |= V29STAT_BIT6;
	((struct v29_status_prefix *)status)->flags &= (unsigned char)~V29STAT_BIT7;

	return 1;
}
