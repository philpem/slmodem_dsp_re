/*
 * fdspkrnl.h -- the full-duplex speakerphone kernel: a two-direction LMS
 * echo canceller with energy validation, and the TONE_* helpers that share
 * its translation unit (the blob span labelled `Fdspkrnl.c`).
 *
 * Layouts below are read from the four functions reconstructed so far plus
 * FDSP_Kernel_InitObj (still the blob's; it zeroes every region named here
 * and sets the defaults quoted in the comments).  Nothing reconstructed yet
 * allocates these objects, so sizes are lower bounds: each struct ends at
 * the last field any read function touches.
 */

#ifndef DSPLIB_FDSPKRNL_H
#define DSPLIB_FDSPKRNL_H

#ifdef __cplusplus
extern "C" {
#endif

/* One block of samples per FDSP_Kernel_Loop call, and the delay-line unit. */
#define FDSP_BLOCK	160

/* Delay line length in floats: 8 blocks. */
#define FDSP_DLY	0x500

/*
 * One direction of the canceller.  The other direction's error output is
 * written into `cross` -- FDSP_Kernel_Loop passes chan A's `cross` to the
 * EchoCanceler run of chan B and vice versa; nothing reconstructed reads it
 * back yet.
 */
struct fdsp_channel {
	float	dly[FDSP_DLY];		/* +0x0000 reference history; newest
					 *         block reversed at [0]     */
	float	cross[FDSP_BLOCK];	/* +0x1400 other direction's error   */
	float	*coef;			/* +0x1680 taps (240 floats, InitObj)*/
	int	verdict;		/* +0x1684 EchoCanceler's flag       */
	short	offset;			/* +0x1688 window offset into dly    */
	short	short_168a;		/* +0x168a not yet seen touched      */
	float	mu;			/* +0x168c LMS step (0.032f / 0.0f
					 *         from InitObj)             */
	short	short_1690;		/* +0x1690 zeroed by InitObj         */
	short	short_1692;		/* +0x1692 not yet seen touched      */
	int	energy[4];		/* +0x1694 recent block RMS values   */
	unsigned int energy_idx;	/* +0x16a4 ring index into energy[]  */
};

struct fdsp_kernel {
	int	int_00;			/* +0x00 InitObj sets 2              */
	int	saturation;		/* +0x04 blocks left before the
					 *       delayed re-init; the object's
					 *       own debug line calls the
					 *       condition "saturation"      */
	int	ntaps_a;		/* +0x08 chan_a filter length (80)   */
	int	ntaps_b;		/* +0x0c chan_b filter length (40)   */
	void	*ptr_10;		/* +0x10 short buffers InitObj clears;
					 *       unused by the loop itself   */
	struct fdsp_channel *chan_a;	/* +0x14 */
	struct fdsp_channel *chan_b;	/* +0x18 */
};

/*
 * The MTK oscillator's state, as TONE_generate builds it on the stack: only
 * `phase` and `step` go in, `out_08` comes back as the sample.  MTK_phasor
 * (still the blob's) writes +0x04 and +0x08 and reads +0x00 and +0x0c.
 */
struct mtk_phasor {
	float	phase;			/* +0x00 radians, in/out       */
	float	out_04;			/* +0x04 written by MTK_phasor */
	float	out_08;			/* +0x08 the generated sample  */
	float	step;			/* +0x0c phase increment       */
};

/*
 * The tone object the three TONE_* functions share.  Three independent
 * clusters -- generator, FIR, biquad -- with unmodelled space between; the
 * creator is not reconstructed, so the gaps stay pads.
 */
struct fdsp_tone {
	unsigned char pad_00[4];	/* +0x000 */
	float	amp;			/* +0x004 output scale               */
	float	duration;		/* +0x008 ms; <= 0 means endless     */
	unsigned char pad_0c[0x14];	/* +0x00c */
	short	fir_len;		/* +0x020 */
	unsigned char pad_22[0xe];	/* +0x022 */
	float	phase;			/* +0x030 oscillator phase, radians  */
	float	step;			/* +0x034 oscillator increment       */
	float	elapsed;		/* +0x038 ms since (re)start         */
	float	*fir_coef;		/* +0x03c */
	float	*fir_dly;		/* +0x040 ring of fir_len floats     */
	short	fir_idx;		/* +0x044 */
	unsigned char pad_46[0x176];	/* +0x046 */
	float	*iir_coef;		/* +0x1bc three floats c0 c1 c2      */
	float	iir_z1;			/* +0x1c0 */
	float	iir_z2;			/* +0x1c4 */
};

/*
 * Set while a locally generated beep is playing; bValidateEnergyValue
 * returns 0 outright when it is up.  LOCAL (`b`) in the blob; the second
 * referent there is the unwritten function at 0xaea27, same span.
 */
extern int bInternalBeepInProgress;

/* Still the blob's. */
void FDSP_Kernel_InitObj(struct fdsp_kernel *k);
void MTK_phasor(struct mtk_phasor *p);

/*
 * One 160-sample block through both directions: shift both delay lines up a
 * block, validate each side's reference energy, cancel each direction, then
 * load the fresh reference blocks in reversed.  `in_a`/`out_a` face chan_a
 * (whose reference is `in_b`) and vice versa.  Always returns 1.
 */
int FDSP_Kernel_Loop(struct fdsp_kernel *k, float *in_a, float *out_b,
		     float *in_b, float *out_a);

/*
 * One direction, one block: NLMS-ish cancel of `in` against
 * hist[pos .. pos+ntaps-1], pos starting at offset+159 and sliding down one
 * per sample.  The error lands in both `out` and `out2`; taps adapt by
 * e*mu*hist when `update` is set, mu is nonzero and the near sample is
 * below half the window peak; *verdict reports whether more than 80 of the
 * 160 samples sat below that half-peak.
 *
 * LOCAL in the blob (regparm(2) there); external and ordinary convention
 * here, as with GetGain.
 */
void EchoCanceler(float *hist, int offset, float *coef, unsigned int ntaps,
		  float *in, float *out, float *out2, int *verdict,
		  float mu, int update);

/*
 * Block-energy gate.  Records the block's scaled RMS in hist[] (ring of
 * `histlen`), and while the running average exceeds 2200 counts down
 * k->saturation, re-initialising the kernel through FDSP_Kernel_InitObj
 * when it reaches zero.  Returns 1 only when the average is quiet and no
 * countdown is pending.  LOCAL in the blob (regparm(2) there).
 */
int bValidateEnergyValue(float *buf, unsigned int n, int *hist,
			 unsigned int *idxp, unsigned int histlen,
			 struct fdsp_kernel *k);

/* Amplitude-scaled oscillator with a millisecond timer; see the source. */
void TONE_generate(struct fdsp_tone *t, float *buf, short n);

/* In-place FIR over the tone object's ring delay line. */
void TONE_filter(struct fdsp_tone *t, float *buf, short n);

/* In-place biquad (direct form II transposed-ish; see the source). */
void TONE_kill(struct fdsp_tone *t, float *buf, short n);

/* buf[0..n-1] = v. */
void zFLTUTL_FloatMemSet(float v, float *buf, unsigned int n);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_FDSPKRNL_H */
