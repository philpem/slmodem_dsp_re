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
	/*
	 * +0x00  `status`, AND THE NAME IS A TYPED CALLEE'S.  `voicedp.c`'s
	 * `voice_tx` passes `v->dp` -- `voice.h`'s own comment on that field
	 * says its TRUE type is `struct fdsp_kernel *` -- straight into
	 * `FDSP_DP_Run`'s FIRST argument, which `beepgen.h`/`Beepgen.c`
	 * (finding F8786) already name `int *status` off a sibling
	 * signature; `FDSP_DP_Run` does nothing with it but
	 * `*status = 2;`, which is this field's own InitObj/Create value.
	 * CLAUDE.md's evidence tier 2.
	 */
	int	status;			/* +0x00 InitObj sets 2              */
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
 * `phase` and `step` go in, `sine` comes back as the sample.  MTK_phasor
 * (still the blob's) writes +0x04 and +0x08 and reads +0x00 and +0x0c.
 *
 * +0x04 and +0x08 ARE THE OBJECT'S OWN COSINE AND SINE, and the evidence is
 * a typed callee: `src/service/mtk.c`'s `MTK_phasor` builds +0x04 from
 * `MTK_cos_table`/`MTK_cos_sign` and +0x08 from `MTK_sin_table`/
 * `MTK_sin_sign` (see mtk.h), and both table names are the object's own
 * (`mtk_tables.c`, finding F8772) rather than an invention here.
 * `Fdspkrnl.c`'s own use of +0x04 corroborates it: `TONE_create` builds a
 * resonator's denominator coefficients out of it with `-2.0f * osc.cosine`
 * and the standard `1 - 2*r*cos(w) z^-1 + ...` shape, and the 60 Hz notch
 * a section later does the same with `hum.cosine`.
 */
struct mtk_phasor {
	float	phase;			/* +0x00 radians, in/out       */
	float	cosine;			/* +0x04 MTK_cos_table, written
					 *       by MTK_phasor         */
	float	sine;			/* +0x08 MTK_sin_table, the
					 *       generated sample      */
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
	/*
	 * +0x046 was `pad_46[2]` -- REMOVED (finding F10151).  It is exactly
	 * the 2-byte compiler alignment gap a `short` at +0x044 leaves ahead
	 * of the 4-byte-aligned `float det_coef` below; `TONE_ASSERT_OFF
	 * (det_coef, 0x048)` in the .c proves the layout, and `dis.py` over
	 * every TONE_* function finds no access to offset 0x046 anywhere.
	 *
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
	/*
	 * +0x066 was `pad_66[2]` -- REMOVED (finding F10151), the same
	 * short-to-int alignment gap as +0x046 above; `TONE_ASSERT_OFF
	 * (int_0068, 0x068)` already proved the target offset and `dis.py`
	 * finds no access to 0x066.
	 */
	int	int_0068;		/* +0x068 */
	int	int_006c;		/* +0x06c */
	int	int_0070[80];		/* +0x070 */
	short	short_01b0;		/* +0x1b0 */
	/*
	 * +0x1b2 was `pad_1b2[2]` -- REMOVED (finding F10151), the same
	 * short-to-pointer alignment gap; `TONE_ASSERT_OFF(ptr_01b4, 0x1b4)`
	 * already proved the target offset and `dis.py` finds no access to
	 * 0x1b2.  `ptr_01b4`/`ptr_01b8` are set by two separate
	 * `sysdep_malloc` calls in `TONE_create`, never by a bulk store that
	 * could have touched the gap.
	 */
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

/**
 * @brief Create the kernel, or re-initialise one the caller already has,
 * and publish it in #pGlobalFDSPObj.
 *
 * The two delays are the object's own names, from the debug line it
 * prints on entry: the RX one becomes chan_a's window offset and the TX
 * one chan_b's.
 *
 * @param k                Caller-owned kernel, or NULL to allocate one.
 * @param sRxSamplesDelay  RX delay, in samples. A negative value leaves
 *                         `status` at 0 instead of 2.
 * @param sTxSamplesDelay  TX delay, in samples.
 * @return The kernel, or NULL if any of the six allocations failed.
 */
struct fdsp_kernel *FDSP_DP_Create(struct fdsp_kernel *k,
				   short sRxSamplesDelay,
				   short sTxSamplesDelay);

/**
 * @brief Free a kernel, its buffer block and both channels with their taps.
 *
 * Clears #pGlobalFDSPObj. A NULL kernel is a no-op; a kernel with a NULL
 * CHANNEL is not, and faults -- see the note at the definition.
 *
 * @param k  The kernel to free.
 */
void FDSP_DP_Delete(struct fdsp_kernel *k);

/**
 * @brief Raise or drop the internal-beep-in-progress flag.
 *
 * Prints a debug line either way. The flag is the object's own
 * (#bInternalBeepInProgress); this is only its setter, and the other
 * writer -- the unreconstructed function at 0xaea27 -- writes it directly.
 *
 * @param on  Nonzero to set the flag, zero to clear it.
 */
void FDSP_Kernel_SetInternalBeepInProgress(int on);

/**
 * @brief Zero and default-configure a kernel.
 *
 * Zeroes both buffer arrays, both channels' delay lines, taps, energy
 * rings and ring indices; then sets the fixed defaults -- 80 taps on
 * chan_a and 40 on chan_b, the LMS step on chan_a and 0.0 on chan_b (the
 * step is one ULP below 0.032f -- finding F8750), `status` 2 and the
 * saturation countdown cleared.
 *
 * @param k  The kernel. The channel POINTERS and the tap POINTERS must
 *           already be set: this dereferences all four and allocates
 *           nothing.
 */
void FDSP_Kernel_InitObj(struct fdsp_kernel *k);

/**
 * @brief One step of the quarter-wave table oscillator.
 *
 * `src/service/mtk.c`, finding F8780.
 *
 * @param p  In: `phase` and `step`. Out: `cosine` and `sine` for the
 *           current phase; `phase` is advanced by `step` and wrapped at pi.
 */
void MTK_phasor(struct mtk_phasor *p);

/*
 * The 48 bytes TONE_create copies over the head of a `struct fdsp_tone`.
 *
 * It is a separate type because the object's own configs are 48 bytes of
 * `.data` and not 0x1c8 -- `rep movsl` with `$0xc` in `%ecx` at 0xaf6c5 --
 * so the author cannot have declared them as whole tone objects.  The
 * layout is `struct fdsp_tone`'s first twelve words and is checked against
 * it by offset assertion in `src/service/Fdspkrnl.c`.
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
	/*
	 * +0x022 was `pad_22[2]` -- REMOVED (finding F10151): the same
	 * short-to-int alignment gap as `fdsp_tone::pad_46`/`pad_66` above,
	 * proved by `TONE_ASSERT_OFF_CFG(int_0024, 0x024)` in the .c.  This
	 * region is never accessed field-by-field -- `TONE_CFG` is static
	 * `.data` and `TONE_create` moves the whole 0x30-byte struct over
	 * `fdsp_tone`'s head with one `rep movsl`, never a per-field access
	 * -- so there is no code to check for a stray read of just these two
	 * bytes; the static initialiser itself holds `00 00` there, which is
	 * consistent with (not proof of) pure padding.  Left NOTEWORTHY: the
	 * three fields past it (`int_0024`/`int_0028`/`int_002c`) land inside
	 * the SAME 14 bytes `fdsp_tone::pad_22[0xe]` (above) still treats as
	 * one undifferentiated unmodelled span -- that asymmetry is a
	 * field-naming question for those three fields' own types, not a
	 * reason to keep this alignment gap explicit; the pad-removal
	 * arithmetic depends only on `int_0024` needing 4-byte alignment,
	 * true whatever its final type turns out to be.
	 */
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

/**
 * @brief Build a tone object from a configuration.
 *
 * The four heap blocks -- two of `fir_len` floats, one of 20 and one of
 * 8 -- are allocated ONLY when this call did the allocating AND
 * `fir_len` is positive, so a caller supplying its own object must
 * supply those too.
 *
 * @param t    NULL allocates 0x1c8 bytes.
 * @param cfg  NULL means #TONE_CFG.
 * @return @p t, or the allocation.
 */
struct fdsp_tone *TONE_create(struct fdsp_tone *t,
			      const struct fdsp_tone_cfg *cfg);

/**
 * @brief One 160-sample block through both echo-cancellation directions.
 *
 * Shifts both delay lines up a block, validates each side's reference
 * energy, cancels each direction, then loads the fresh reference blocks
 * in reversed.
 *
 * @param k      The kernel.
 * @param in_a   chan_a's near input.
 * @param out_b  chan_b's cancelled output.
 * @param in_b   chan_b's near input (chan_a's reference).
 * @param out_a  chan_a's cancelled output.
 * @return 1, always.
 */
int FDSP_Kernel_Loop(struct fdsp_kernel *k, float *in_a, float *out_b,
		     float *in_b, float *out_a);

/**
 * @brief One direction, one block of NLMS-ish echo cancellation.
 *
 * Cancels @p in against `hist[pos .. pos+ntaps-1]`, @p pos starting at
 * `offset+159` and sliding down one per sample. LOCAL in the blob
 * (`regparm(2)` there); external and ordinary convention here, as with
 * GetGain().
 *
 * @param hist     Reference delay line.
 * @param offset   Window offset into @p hist.
 * @param coef     Adaptive filter taps, @p ntaps entries.
 * @param ntaps    Number of taps.
 * @param in       Near-end input for this block.
 * @param out      Error output.
 * @param out2     Error output, duplicated (the other direction's `cross`).
 * @param verdict  Set to whether more than 80 of the 160 samples sat
 *                 below half the window peak.
 * @param mu       LMS step size; taps adapt only when nonzero.
 * @param update   Nonzero enables tap adaptation (also gated on @p mu
 *                 and the near sample being below half the window peak).
 */
void EchoCanceler(float *hist, int offset, float *coef, unsigned int ntaps,
		  float *in, float *out, float *out2, int *verdict,
		  float mu, int update);

/**
 * @brief Block-energy gate, and the trigger for a delayed kernel re-init.
 *
 * Records the block's scaled RMS in @p hist (a ring of @p histlen), and
 * while the running average exceeds 2200 counts down `k->saturation`,
 * re-initialising the kernel through FDSP_Kernel_InitObj() when it
 * reaches zero. LOCAL in the blob (`regparm(2)` there).
 *
 * @param buf     Block to measure.
 * @param n       Number of samples in @p buf.
 * @param hist    Ring buffer of recent scaled RMS values, @p histlen entries.
 * @param idxp    Ring write index into @p hist, updated in place.
 * @param histlen Length of @p hist.
 * @param k       The kernel, whose `saturation` counter this may drive
 *                to zero and re-init.
 * @return 1 only when the average is quiet and no countdown is pending,
 *         0 otherwise.
 */
int bValidateEnergyValue(float *buf, unsigned int n, int *hist,
			 unsigned int *idxp, unsigned int histlen,
			 struct fdsp_kernel *k);

/**
 * @brief Free a tone object.
 *
 * The four heap blocks hanging off it are freed only when `fir_len` is
 * positive -- an object that never got a filter never got any of them
 * either.
 *
 * @param t  The tone object to free.
 */
void TONE_delete(struct fdsp_tone *t);

/**
 * @brief Generate an amplitude-scaled tone with a millisecond timer.
 * @param t    The tone object, advanced in place.
 * @param buf  Output samples.
 * @param n    Number of samples to generate.
 */
void TONE_generate(struct fdsp_tone *t, float *buf, short n);

/**
 * @brief Run samples through the tone object's FIR and detector biquad,
 * and judge whether the tone is present.
 *
 * @p buf is read and not written; the FIR state (`fir_dly`, `fir_idx`)
 * and the biquad state advance exactly as TONE_filter()'s would.
 *
 * @param t    The tone object, updated in place.
 * @param buf  Input samples.
 * @param n    Number of samples.
 * @return 2 if the smoothed total power (`float_0060`) is below
 *         `float_0014` -- there is nothing to judge; 1 if
 *         `float_0060 * float_000c >= float_005c` -- the residual is
 *         small against the total, which is what the object treats as
 *         the tone; 0 if there is signal and it fails that test.
 */
int TONE_detect(struct fdsp_tone *t, float *buf, short n);

/**
 * @brief In-place FIR over the tone object's ring delay line.
 * @param t    The tone object, updated in place.
 * @param buf  Samples to filter in place.
 * @param n    Number of samples.
 */
void TONE_filter(struct fdsp_tone *t, float *buf, short n);

/**
 * @brief In-place biquad (direct form II transposed-ish; see the source).
 * @param t    The tone object, updated in place.
 * @param buf  Samples to filter in place.
 * @param n    Number of samples.
 */
void TONE_kill(struct fdsp_tone *t, float *buf, short n);

/**
 * @brief Fill a float buffer with a constant.
 * @param v    The value to fill with.
 * @param buf  Buffer to fill.
 * @param n    Number of entries.
 */
void zFLTUTL_FloatMemSet(float v, float *buf, unsigned int n);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_FDSPKRNL_H */
