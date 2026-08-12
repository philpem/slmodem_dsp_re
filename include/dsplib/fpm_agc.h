/*
 * fpm_agc.h -- Fixed Point Modem: automatic gain control.
 *
 * Not a per-sample AGC: the input is chopped into fixed-length measurement
 * blocks, one RMS is taken per block, and the whole block is scaled by a
 * single gain.  Bell 103 uses a block of 36 samples, which is exactly the
 * length FPM_rms's 1/36 scaling was sized for.
 *
 * Each block takes one of two paths:
 *
 *   - too quiet -> the block is *zeroed*, not attenuated.  It is a noise gate
 *     as well as a gain control, and the gate has two levels: `acquire_level`
 *     applies before any gain has ever been computed, `squelch_level` once
 *     one has.  For Bell 103 that is 10 and 80, so almost anything starts the
 *     loop and a much higher floor keeps it running.
 *
 *   - loud enough -> the level estimate is smoothed, the gain recomputed from
 *     it (unless frozen), and the block scaled with saturation.
 *
 * The smoother is first order:
 *
 *     level += (alpha[0] * level + beta[0] * measured) >> 15
 *
 * with alpha and beta supplied by *pointer* from the config, not as scalars.
 * See the note on the config struct below -- an int16 dump of the config
 * shows them as ordinary-looking coefficients and hides that they are
 * addresses.
 *
 * NOTE the settled output level is `ref_level / 2`, not `ref_level`.  The
 * applied gain works out as `ref_level / (2 * level)` because the exponent
 * correction is `shift - 1` rather than `shift`; see src/dsp/fpm_agc.c.  For
 * Bell 103's ref_level of 16384 the loop settles at an output RMS of 8192,
 * a quarter of full scale.
 */

#ifndef DSPLIB_FPM_AGC_H
#define DSPLIB_FPM_AGC_H

/*
 * 24 bytes, copied wholesale by init.
 *
 * MIXED STRUCT: +0x0c and +0x10 are POINTERS (R_386_32 into .data).
 */
struct fpm_agc_cfg {
	short ref_level;	/* +0x00 gain reference; output settles at /2 */
	short acquire_level;	/* +0x02 gate floor before the first gain     */
	short squelch_level;	/* +0x04 gate floor once a gain exists        */
	short f06;		/* +0x06 not read by any fpm_agc function     */
	short f08;		/* +0x08 not read by any fpm_agc function     */
	unsigned short block_len;	/* +0x0a measurement block, in samples */
	const short *alpha;	/* +0x0c smoother feedback  coefficient, Q15  */
	const short *beta;	/* +0x10 smoother feedforward coefficient, Q15 */
	short f14;		/* +0x14 not read by any fpm_agc function     */
	short f16;		/* +0x16 not read by any fpm_agc function; NOT padding --
				 *       V.32's two configs both carry 6553 (0.2 in Q15)
				 *       here.  Bell 103's and V.23's carry zero, which is
				 *       why it read as padding.  Finding 1621.       */
};

struct fpm_agc {
	struct fpm_agc_cfg cfg;	/* +0x00 .. +0x17                             */
	int f18;		/* +0x18 set to 1 by init on reset; agc() never
				 *       reads it -- a caller must.           */
	int signal;		/* +0x1c output: more than half the blocks in
				 *       the last call were above the gate    */
	short level;		/* +0x20 smoothed level estimate              */
	short pad22;
	short mult;		/* +0x24 gain mantissa, Q15                   */
	short shift;		/* +0x26 gain exponent, applied as a left shift */
	int freeze;		/* +0x28 non-zero: hold mult/shift            */
};

/*
 * `reset` non-zero clears the gain as well as the level, so the next block
 * faces `acquire_level` rather than `squelch_level`.  Either way init clears
 * the level estimate and releases the freeze.
 */
void FPM_AGC_init(struct fpm_agc *agc, const struct fpm_agc_cfg *cfg,
		  int reset);

/* Hold the current gain: blocks are still measured and still gated. */
void FPM_AGC_Freeze(struct fpm_agc *agc);

/*
 * Resume adapting.  Note this clears the level estimate but NOT the gain, so
 * the squelch stays at `squelch_level` -- Release is not a reset.
 */
void FPM_AGC_Release(struct fpm_agc *agc);

/*
 * Gain-control `count` samples in place.
 *
 * Counts shorter than half a block are left completely untouched; see the
 * partitioning note in src/dsp/fpm_agc.c.
 */
void FPM_AGC_agc(struct fpm_agc *agc, short *samples, unsigned short count);

#endif /* DSPLIB_FPM_AGC_H */
