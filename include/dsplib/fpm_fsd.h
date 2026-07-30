/*
 * fpm_fsd.h -- FSK demodulator.
 *
 * Bell 103 receives at 2400 Hz (FPM_MRF brings 8000 down to it), which is 8
 * samples per symbol at 300 baud.  The config carries two filters: an input
 * FIR and an IIR lowpass.
 *
 * NOTE the config is a mixed struct with POINTERS at +0x00 and +0x08 --
 * dumping it as int16 hides them behind plausible-looking scalars.
 */

#ifndef DSPLIB_FPM_FSD_H
#define DSPLIB_FPM_FSD_H

/* 28 bytes, copied wholesale by init. */
struct fpm_fsd_cfg {
	const short *fir;	/* +0x00 input FIR coefficients          */
	short fir_taps;		/* +0x04                                 */
	short f06;		/* +0x06                                 */
	const short *iir;	/* +0x08 IIR lowpass coefficients        */
	short iir_len;		/* +0x0c                                 */
	short f0e;		/* +0x0e                                 */
	short f10;		/* +0x10                                 */
	short f12;		/* +0x12 halved into the state at +0x22  */
	short f14;		/* +0x14                                 */
	short f16;		/* +0x16 length of the third buffer      */
	short f18;		/* +0x18                                 */
	short pad1a;
};

struct fpm_fsd {
	struct fpm_fsd_cfg cfg;	/* +0x00 .. +0x1a */
	short *buf1c;		/* +0x1c f16 entries        */
	short f20;		/* +0x20 */
	short f22;		/* +0x22 f12 / 2 */
	short *fir_hist;	/* +0x24 fir_taps entries   */
	short f28;		/* +0x28 */
	short pad2a;
	short *iir_hist;	/* +0x2c 2 * iir_len entries */
	short f30;		/* +0x30 */
	short f32;		/* +0x32 */
	short f34;		/* +0x34 */
	short pad36;
};

/* `fresh` non-zero allocates the three buffers; zero re-inits in place. */
void FPM_FSD_init(struct fpm_fsd *state, const struct fpm_fsd_cfg *cfg,
		  int fresh);
void FPM_FSD_free(struct fpm_fsd *state);

#endif /* DSPLIB_FPM_FSD_H */
