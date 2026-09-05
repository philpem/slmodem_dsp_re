/*
 * faxcfg.c -- Class 1 fax: the four receive-side configuration tables.
 *
 *   FAXVMI_CFG   .rodata 0x009490   24
 *   V17RX_CFG    .data   0x0079a0   40
 *   V27RX_CFG    .data   0x007b94   28
 *   V29RX_CFG    .data   0x007df0   24
 *
 * Read `faxcfg.h` for the types and for what is named and what is not.  The
 * bytes here were taken with `tools/tabdump.py` and every dword was checked
 * for a relocation inside its own range first, because a config table is
 * exactly where a pointer read as a small integer does its damage.  There are
 * none in these four.
 *
 * THE TRANSLATION UNIT IS NOT SETTLED, AND THIS FILE IS NOT A CLAIM ABOUT IT.
 * `V17RX_CFG` sits in `.data` between `V17RX_CTL` (0x7988) and `V17TX_CFG`
 * (0x7a40), which is the V.17 module's own run of data symbols, so the
 * author's `V17RX_CFG` was almost certainly in the file that holds
 * `V17RX_create` rather than in one shared table file.  It is here because
 * `class1rx.c` cannot link without it and the V.17, V.27 and V.29 modules
 * belong to other strands of this wave; moving each table into its module is
 * free (a data symbol's bytes do not depend on its translation unit) and
 * should happen when those modules land.  Recorded as D1080.
 */

#include "dsplib/faxcfg.h"

/*
 * The VMI's own defaults.  Every constructor overwrites +0x00, +0x04, +0x08,
 * +0x0a, +0x0c, +0x0e, +0x10 and +0x14, which is all of it except +0x02 --
 * so the only field of this table that survives into a live `struct faxvmi`
 * is the one nothing has ever been seen to read.  The other seven values are
 * still the object's and are still what a constructor that forgot a field
 * would leave behind.
 */
const struct faxvmi_cfg FAXVMI_CFG = {
	0,		/* +0x00  mode                                       */
	0,		/* +0x02 */
	0,		/* +0x04  reverse                                    */
	128,		/* +0x08  fifo_size                                  */
	50,		/* +0x0a  max_frame                                  */
	128,		/* +0x0c  frame_size                                 */
	0,		/* +0x0e  slot                                       */
	0,		/* +0x10  modem_cfg                                  */
	0		/* +0x14                                             */
};

/* V.17: 14400 bit/s, the fastest of the three. */
struct v17rx_cfg V17RX_CFG = {
	1,		/* +0x00 */
	14400,		/* +0x04  bit_rate                                   */
	0,		/* +0x06 */
	60000,		/* +0x08 */
	0,		/* +0x0c */
	0,		/* +0x10 */
	0,		/* +0x14 */
	0,		/* +0x18  coefsave0                                  */
	0,		/* +0x1c  coefsave1                                  */
	0,		/* +0x20  ratesave                                   */
	0		/* +0x24 */
};

/* V.27ter: 4800 bit/s.  `v27rx_create` tests this field against 2400. */
struct v27rx_cfg V27RX_CFG = {
	1,		/* +0x00 */
	4800,		/* +0x04  bit_rate                                   */
	0,		/* +0x06 */
	60000,		/* +0x08 */
	0,		/* +0x0c */
	0,		/* +0x10 */
	0,		/* +0x14 */
	0		/* +0x18 */
};

/* V.29: 9600 bit/s.  `v29rx_create` tests this field against 7200. */
struct v29rx_cfg V29RX_CFG = {
	1,		/* +0x00 */
	9600,		/* +0x04  bit_rate                                   */
	0,		/* +0x06 */
	60000,		/* +0x08 */
	0,		/* +0x0c */
	0,		/* +0x10 */
	0		/* +0x14 */
};

/*
 * V21_CHAN2_MTD_COEFF   .data 0x007a60   20
 *
 * The V.21 channel-2 tone detector's biquad bank, and it is here because it is
 * the one table ALL THREE fax receiver constructors share: `V17RX_create`,
 * `V27RX_create` and `V29RX_create` each build an `fpm_mtd_cfg` around it, so
 * a per-modulation file would have had to pick one of the three arbitrarily.
 * A V.21 file would be the better home once one exists; `src/fax/v21.c`
 * belongs to another strand of this wave.  D1100, the same reasoning as the
 * paragraph at the top of this file.
 *
 * TEN SHORTS IS THE CONSUMER'S COUNT, NOT A DIVISION OF THE BYTE SIZE.  Each
 * of the three constructors writes `tones = 2` into the config beside it, and
 * `FPM_MTD_detect` runs a five-coefficient biquad section per tone, so 20
 * bytes is 2 x 5 shorts.  `V29_MTD_COEFF` and `DEF_COEFS` are both 20 bytes
 * with the same `tones = 2`, which is three independent instances of the
 * shape.
 *
 * The two sections share their first two coefficients (-13271, 16384) and
 * differ in the last three, which is one bandpass pair at two centre
 * frequencies -- V.21's channel-2 mark and space, 1650 and 1850 Hz.  That
 * reading is usage inference and is not needed by anything here.
 */
short V21_CHAN2_MTD_COEFF[10] = {
	-13271,  16384,   8005,  -8895,  16384,
	-13271,  16384,   3466,  -3851,  16384
};
