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
	const short *coeff;	/* +0x00 biquad sections, one per tone   */
	short tones;		/* +0x04 number of sections              */
	short ratio;		/* +0x06 detection threshold, Q15        */
	short min_level;	/* +0x08 below this, report "no signal"  */
	short f0a;		/* +0x0a                                 */
};

struct fpm_mtd {
	struct fpm_mtd_cfg cfg;	/* +0x00 .. +0x0a                        */
	short *acc;		/* +0x0c two words per tone section      */
	short dc_state[2];	/* +0x10 the DC filter's biquad state    */
	short out_of_band;	/* +0x14 wideband minus tone energy      */
	short wideband;		/* +0x16 total energy                    */
};

/*
 * NULL `state` allocates 24 bytes and the accumulator array; supplying your
 * own state means supplying your own array.  NULL `cfg` uses FPM_MTD_CFG.
 */
struct fpm_mtd *FPM_MTD_create(struct fpm_mtd *state,
			       const struct fpm_mtd_cfg *cfg);
void FPM_MTD_delete(struct fpm_mtd *state);

/*
 * Detection verdicts.  Verified by sweeping a live detector: with Bell 103's
 * two-section bank, 800..2200 Hz returns 1 and 2300 Hz and above returns 0.
 *
 * NOTE this is the OPPOSITE polarity to FPM_TONE_detect, where zero means the
 * tone is present.  The difference is real, not a mistake in either: this
 * module's coefficients are per-tone bandpasses, so `out_of_band` is genuinely
 * the leftover; FPM_TONE's are a notch, so its equivalent quantity is the
 * tone's own share.  See finding 33.
 */
#define FPM_MTD_ABSENT   0	/* signal present, but not in band       */
#define FPM_MTD_PRESENT  1	/* tone detected                         */
#define FPM_MTD_NOSIGNAL 2	/* level below cfg.min_level             */

/*
 * Run `count` samples through the detector and report the verdict.  Energy
 * estimates persist in the state, so the result reflects a running average
 * rather than this block alone.
 */
short FPM_MTD_detect(struct fpm_mtd *state, const short *samples, short count);

extern const struct fpm_mtd_cfg FPM_MTD_CFG_data;

#endif /* DSPLIB_FPM_MTD_H */
