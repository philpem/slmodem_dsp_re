/*
 * fdspkrnl.h -- the full-duplex speakerphone kernel: a two-direction LMS
 * echo canceller with energy validation, and the TONE_* helpers that share
 * its translation unit (the blob span labelled `Fdspkrnl.c`).
 *
 * Layouts below are read from the functions of that span reconstructed so
 * far, FDSP_Kernel_InitObj included -- it zeroes every region named here and
 * sets the defaults quoted in the comments.  Nothing reconstructed yet
 * ALLOCATES these objects, so the sizes are lower bounds: each struct ends
 * at the last field any read function touches.
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
	float	mu;			/* +0x168c LMS step: FDSP_CHAN_A_MU on
					 *         chan_a and 0.0f on chan_b,
					 *         from InitObj              */
	short	short_1690;		/* +0x1690 zeroed by InitObj         */
	short	short_1692;		/* +0x1692 not yet seen touched      */
	int	energy[4];		/* +0x1694 recent block RMS values   */
	unsigned int energy_idx;	/* +0x16a4 ring index into energy[]  */
};

/*
 * What `fdsp_kernel.buffers` points at.  Only FDSP_Kernel_InitObj touches it
 * and it only ever writes zeros, so the SHAPE below is what InitObj's four
 * loops and three stores describe and the ROLES are unknown: two 2000-entry
 * short arrays back to back, then a gap, then three ints.  The trailing size
 * is a lower bound, as everywhere else in this header.
 */
struct fdsp_buffers {
	short	short_0000[2000];	/* +0x0000                           */
	short	short_0fa0[2000];	/* +0x0fa0 immediately after the
					 *         first, cleared in the same
					 *         loop                      */
	unsigned char pad_1f40[0x7d0];	/* +0x1f40                           */
	int	int_2710;		/* +0x2710                           */
	int	int_2714;		/* +0x2714                           */
	int	int_2718;		/* +0x2718                           */
};

struct fdsp_kernel {
	int	int_00;			/* +0x00 InitObj sets 2              */
	int	saturation;		/* +0x04 blocks left before the
					 *       delayed re-init; the object's
					 *       own debug line calls the
					 *       condition "saturation"      */
	int	ntaps_a;		/* +0x08 chan_a filter length (80)   */
	int	ntaps_b;		/* +0x0c chan_b filter length (40)   */
	struct fdsp_buffers *buffers;	/* +0x10 cleared by InitObj; unused
					 *       by the loop itself          */
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
 * The tone object the TONE_* functions share.  Four clusters -- generator,
 * FIR, detector, output biquad -- with unmodelled space between them; the
 * creator (TONE_create, 0xaf690) is not reconstructed, so the gaps stay
 * pads and no field here is known to be the whole story.
 */
struct fdsp_tone {
	float	freq;			/* +0x000 Hz.  TONE_create turns it
					 *        into `step` by multiplying
					 *        by 2*pi/8000, and the two
					 *        configs hold 2100 and 980  */
	float	amp;			/* +0x004 output scale               */
	float	duration;		/* +0x008 ms; <= 0 means endless     */
	float	float_000c;		/* +0x00c TONE_detect scales the
					 *        smoothed total power by it
					 *        before the in-band test    */
	unsigned char pad_10[4];	/* +0x010 */
	float	float_0014;		/* +0x014 TONE_detect's floor on the
					 *        smoothed total power: under
					 *        it the verdict is 2        */
	float	pole_radius;		/* +0x018 INFERENCE, and structural:
					 *        TONE_create builds
					 *        det_coef[1] = 2*r*cos(w) and
					 *        det_coef[2] = -r*r out of
					 *        it, which is a resonator's
					 *        pole radius and nothing
					 *        else.  Both configs: 0.9375 */
	const float *fir_proto;		/* +0x01c fir_len taps, modulated by
					 *        the tone into fir_coef      */
	short	fir_len;		/* +0x020 */
	unsigned char pad_22[0xe];	/* +0x022 */
	float	phase;			/* +0x030 oscillator phase, radians  */
	float	step;			/* +0x034 oscillator increment       */
	float	elapsed;		/* +0x038 ms since (re)start         */
	float	*fir_coef;		/* +0x03c */
	float	*fir_dly;		/* +0x040 ring of fir_len floats     */
	short	fir_idx;		/* +0x044 */
	unsigned char pad_46[2];	/* +0x046 */
	/*
	 * TONE_detect's own two-pole section.  Its three coefficients sit
	 * INLINE here rather than behind a pointer, and its arithmetic is
	 * TONE_kill's instruction for instruction -- same c[0..2] roles, same
	 * z1 <- z2, z2 <- w update -- which is what fixes these five fields.
	 */
	float	det_coef[3];		/* +0x048 c0 c1 c2                   */
	float	det_z1;			/* +0x054 */
	float	det_z2;			/* +0x058 */
	float	float_005c;		/* +0x05c smoothed (fir^2 - biquad^2),
					 *        clamped at zero on the way
					 *        out of TONE_detect         */
	float	float_0060;		/* +0x060 smoothed fir^2             */
	/*
	 * Everything from here to +0x1b0 is cleared by TONE_create and read
	 * by nothing reconstructed, so the shapes are its stores and the
	 * TYPES are not established: a `movl $0x0` says four bytes and says
	 * nothing about what they mean.  The 80-entry block is one loop
	 * there, counted in a `short` to 79.
	 */
	short	short_0064;		/* +0x064 */
	unsigned char pad_66[2];	/* +0x066 */
	int	int_0068;		/* +0x068 */
	int	int_006c;		/* +0x06c */
	int	int_0070[80];		/* +0x070 */
	short	short_01b0;		/* +0x1b0 */
	unsigned char pad_1b2[2];	/* +0x1b2 */
	float	*ptr_01b4;		/* +0x1b4 freed by TONE_delete       */
	float	*ptr_01b8;		/* +0x1b8 freed by TONE_delete       */
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

/*
 * The kernel FDSP_DP_Create last handed out, and a counter written by that
 * function and read by nothing in the whole object.  Both are `b` in the
 * blob and external here, the same trade `bInternalBeepInProgress` makes.
 */
extern struct fdsp_kernel *pGlobalFDSPObj;
extern unsigned int uCorrelationReportsNo;

/*
 * Create the kernel, or re-initialise one the caller already has, and
 * publish it in `pGlobalFDSPObj`.
 *
 * The two delays are the object's own names, from the debug line it prints
 * on entry: the RX one becomes chan_a's window offset and the TX one
 * chan_b's.  A negative RX delay leaves `int_00` at 0 instead of 2.
 * Returns the kernel, or NULL if any of the six allocations failed.
 */
struct fdsp_kernel *FDSP_DP_Create(struct fdsp_kernel *k,
				   short sRxSamplesDelay,
				   short sTxSamplesDelay);

/*
 * Free the kernel, its buffer block and both channels with their taps, and
 * clear `pGlobalFDSPObj`.  A NULL kernel is a no-op; a kernel with a NULL
 * CHANNEL is not, and faults -- see the note at the definition.
 */
void FDSP_DP_Delete(struct fdsp_kernel *k);

/*
 * Raise or drop the beep flag, with a debug line either way.  The flag is
 * the object's own; this is only its setter, and the other writer -- the
 * unreconstructed function at 0xaea27 -- writes it directly.
 */
void FDSP_Kernel_SetInternalBeepInProgress(int on);

/*
 * Zero everything: both buffer arrays, both channels' delay lines, taps,
 * energy rings and ring indices; then the fixed defaults -- 80 taps on
 * chan_a and 40 on chan_b, the LMS step on chan_a and 0.0 on chan_b (the
 * step is one ULP below 0.032f -- finding F8750), `int_00`
 * 2 and the saturation countdown cleared.
 *
 * The channel POINTERS and the tap POINTERS have to be set before this is
 * called: it dereferences all four and allocates nothing.
 */
void FDSP_Kernel_InitObj(struct fdsp_kernel *k);

/*
 * One step of the quarter-wave table oscillator: `phase` and `step` go in,
 * `out_04` comes back as the cosine and `out_08` as the sine, and `phase`
 * is advanced and wrapped at pi.  `src/service/mtk.c`, finding F8780.
 */
void MTK_phasor(struct mtk_phasor *p);

/*
 * The 48 bytes TONE_create copies over the head of a `struct fdsp_tone`.
 *
 * It is a separate type because the object's own configs are 48 bytes of
 * `.data` and not 0x1c8 -- `rep movsl` with `$0xc` in `%ecx` at 0xaf6c5 --
 * so the author cannot have declared them as whole tone objects.  The
 * layout is `struct fdsp_tone`'s first twelve words and is checked against
 * it by offset assertion in `src/service/fdspkrnl.c`.
 */
struct fdsp_tone_cfg {
	float	freq;			/* +0x000 */
	float	amp;			/* +0x004 */
	float	duration;		/* +0x008 */
	float	float_000c;		/* +0x00c */
	float	float_0010;		/* +0x010 both configs hold 0.01    */
	float	float_0014;		/* +0x014 */
	float	pole_radius;		/* +0x018 */
	const float *fir_proto;		/* +0x01c */
	short	fir_len;		/* +0x020 */
	unsigned char pad_22[2];	/* +0x022 */
	int	int_0024;		/* +0x024 */
	int	int_0028;		/* +0x028 */
	int	int_002c;		/* +0x02c */
};

/*
 * The 2100 Hz tone's configuration -- `D` and not `d` in the blob, so it is
 * the object's own exported default and the only one TONE_create reaches.
 * `TONE_create(t, 0)` uses it.
 */
extern struct fdsp_tone_cfg TONE_CFG;

/*
 * Build a tone object from a configuration.
 *
 * `t` NULL allocates 0x1c8 bytes; `cfg` NULL means TONE_CFG.  The four
 * heap blocks -- two of `fir_len` floats, one of 20 and one of 8 -- are
 * allocated ONLY when this call did the allocating AND `fir_len` is
 * positive, so a caller supplying its own object must supply those too.
 * Returns `t`, or the allocation.
 */
struct fdsp_tone *TONE_create(struct fdsp_tone *t,
			      const struct fdsp_tone_cfg *cfg);

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

/*
 * Free the tone object.  The four heap blocks hanging off it are freed only
 * when `fir_len` is positive -- an object that never got a filter never got
 * any of them either.
 */
void TONE_delete(struct fdsp_tone *t);

/* Amplitude-scaled oscillator with a millisecond timer; see the source. */
void TONE_generate(struct fdsp_tone *t, float *buf, short n);

/*
 * Run n samples through the tone object's FIR and its detector biquad, and
 * report:
 *
 *	2   the smoothed total power (float_0060) is below float_0014 --
 *	    there is nothing to judge
 *	1   float_0060 * float_000c >= float_005c: the residual is small
 *	    against the total, which is what the object treats as the tone
 *	0   there is signal and it fails that test
 *
 * `buf` is read and not written; the FIR state (fir_dly, fir_idx) and the
 * biquad state advance exactly as TONE_filter's would.
 */
int TONE_detect(struct fdsp_tone *t, float *buf, short n);

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
