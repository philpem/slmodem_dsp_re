/*
 * fpm_mtd.h -- multi-tone detector.
 *
 * Holds a bank of resonators, one per tone, and reports which is ringing.
 * Bell 103 builds one with MTDb103_COEF; the DTMF path uses the same module
 * with the MTD*_COEF_8000/_9600 tables.
 *
 * The config is another mixed struct -- +0x00 is a POINTER to the coefficient
 * bank, not a scalar.
 */

#ifndef DSPLIB_FPM_MTD_H
#define DSPLIB_FPM_MTD_H

/* 12 bytes, copied wholesale by create. */
struct fpm_mtd_cfg {
	const short *coeff;	/* +0x00 one pair per tone */
	short tones;		/* +0x04 */
	short f06;		/* +0x06 */
	short f08;		/* +0x08 */
	short f0a;		/* +0x0a */
};

struct fpm_mtd {
	struct fpm_mtd_cfg cfg;	/* +0x00 .. +0x0a */
	short *acc;		/* +0x0c two accumulators per tone */
	short f10;		/* +0x10 */
	short f12;		/* +0x12 */
	short f14;		/* +0x14 */
	short f16;		/* +0x16 */
};

/*
 * NULL `state` allocates 24 bytes and the accumulator array; supplying your
 * own state means supplying your own array.  NULL `cfg` uses FPM_MTD_CFG.
 */
struct fpm_mtd *FPM_MTD_create(struct fpm_mtd *state,
			       const struct fpm_mtd_cfg *cfg);
void FPM_MTD_delete(struct fpm_mtd *state);

extern const struct fpm_mtd_cfg FPM_MTD_CFG_data;

#endif /* DSPLIB_FPM_MTD_H */
