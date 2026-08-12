/*
 * v32cfg.h -- ITU-T V.32/V.32bis: the DSP blocks' configurations.
 *
 * Declarations only.  Every object here is an instance of a struct defined
 * elsewhere -- `struct fpm_agc_cfg` in `fpm_agc.h`, `struct fpm_mrf_cfg` in
 * `fpm_mrf.h` -- so this header defines no type of its own.
 *
 * The V.32 datapump runs at 7200 Hz internally, three samples per symbol at
 * 2400 baud, and `MRFv32_CFG` is the 9:10 resampler that gets it there from
 * the 8000 Hz datapump interface.
 */

#ifndef DSPLIB_V32CFG_H
#define DSPLIB_V32CFG_H

#ifdef __cplusplus
extern "C" {
#endif

struct fpm_agc_cfg;
struct fpm_mrf_cfg;

/* Q15 smoother pairs: [0] acquisition, [1] tracking.  Both configs use [0]. */
extern short AGC_DEF_ALPHA[2];
extern short AGC_DEF_BETA[2];

extern struct fpm_agc_cfg AGCv32_CFG;		/* 36-sample blocks, ref 9061  */
extern struct fpm_agc_cfg AGCv32Prc_CFG;	/* 40-sample blocks, ref 10000 */

extern const short MRFv32_COFFS[360];		/* 9 branches x 40 taps        */
extern const struct fpm_mrf_cfg MRFv32_CFG;	/* 8000 -> 7200 Hz             */

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V32CFG_H */
