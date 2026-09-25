/*
 * V29t_stc.c -- split out of the merged v29.c / v29data.c so the definitions sit in
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
 * V29TX_control -- .text 0x0a4fb0, 126 bytes.  See v29fax.h for the five
 * effects and the request type's derivation.
 */
int
V29TX_control(void *fp, const struct v29tx_control_req *req)
{
	struct v29_tx_params *prm;
	short rate;

	if (req == 0)
		return 0;

	prm = ((struct v29_tx_root *)fp)->params;
	rate = prm->rate;

	((struct v29_tx_root *)fp)->tx->pps.cfg.scale = req->scale_mul;
	((struct v29_tx_root *)fp)->tx->pps.cfg.scale = V29TX_PPS_SCALE[rate] * req->scale_mul;

	((struct v29tx_cfg *)fp)->int_0008 = req->int_0004;

	if (req->ctl0 & V29TXCTL_CTL0_BIT2)
		(*(unsigned char *)(void *)&((struct v29_tx_root *)fp)->cfg.flags) |= V29TXS_10_BIT2;

	prm->int_0008 =
		(req->ctl1 & V29TXCTL_CTL1_BIT4) != 0;

	if (req->ctl1 & V29TXCTL_CTL1_BIT1)
		V29TX_create(fp, fp);

	return 1;
}

/*
 * ---------------------------------------------------------------------------
 * V29TX_status -- .text 0x0a5030, 100 bytes.
 *
 * THE STORE TO +0x14 HAPPENS TWICE AND THE FIRST ONE IS NOT DEAD.
 *
 * The object clears the low two bits of the report's +0x14, clears bit 0 of
 * its +0x15, then loads the source's +0x10 and stores `& 0x04` over the whole
 * of +0x14 again.  The second store covers the first completely, so a reader
 * expects the compiler to have deleted it -- and it could not, because the
 * load of `src + 0x10` sits BETWEEN them and the two pointers are unrelated
 * parameters.  If the caller ever passed `status = tx + 0x0c` the first store
 * would be visible in what the second one stores.
 *
 * So it is reproduced, spelled through `unsigned char` lvalues, which alias
 * everything and give the modern compiler the same reason to keep it.
 * `V17TX_status` writes byte for byte the same sequence, which is the second,
 * independent statement that this is the author's source and not an artefact.
 * Finding F8878.
 *
 * THE SECOND STORE IS AN ASSIGNMENT AND NOT A MERGE, so every bit the caller
 * had in +0x14 is lost -- including the two the statement above went to the
 * trouble of clearing.  Reproduced; deviation D1035.
 */
int
V29TX_status(void *tx, void *status)
{
	if (status == 0)
		return 0;

	((struct v29_status_prefix *)status)->protocol = ((struct v29_tx_root *)tx)->cfg.protocol;
	((struct v29_status_prefix *)status)->tx_bps = ((struct v29_tx_root *)tx)->cfg.bitrate;
	((struct v29_status_prefix *)status)->short_04 = 0;
	((struct v29_status_prefix *)status)->quality = 0;
	((struct v29_status_prefix *)status)->short_08 = 0;
	((struct v29_status_prefix *)status)->short_0a = 0;
	((struct v29_status_prefix *)status)->short_0c = 0;
	/*
	 * `tx + 0x02` IS READ TWICE.  The object loads it again at 0xa506a
	 * rather than reusing the copy it made at 0xa5044, which it could only
	 * be forced into by the stores in between -- `status` and `tx` are
	 * unrelated parameters and may overlap.  Two statements, therefore, and
	 * not one value used twice.
	 */
	((struct v29_status_prefix *)status)->short_10 = ((struct v29_tx_root *)tx)->cfg.bitrate;
	((struct v29_status_prefix *)status)->short_12 = 0;

	((struct v29_status_prefix *)status)->flags &= (unsigned char)~V29STAT_FLAGS_LOW2;
	((struct v29_status_prefix *)status)->flags2 &= (unsigned char)~V29STAT_FLAGS2_BIT0;
	((struct v29_status_prefix *)status)->flags =
		(unsigned char)((*(unsigned char *)(void *)&((struct v29_tx_root *)tx)->cfg.flags) & V29TXS_10_BIT2);

	return 1;
}
