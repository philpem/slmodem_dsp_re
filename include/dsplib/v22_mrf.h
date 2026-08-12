/*
 * v22_mrf.h -- V.22/V.22bis Multi-Rate Filter: the receiver's resampling
 * downconverter.
 *
 * A 9:20 polyphase resampler -- nine phases of thirty taps, so a 270-tap
 * prototype -- driven by the datapump at 8000 samples/s, which puts its
 * output at 3600, six samples per 600-baud symbol.  `V22FP_create` does not
 * hand it `MRFv22_COFFS` directly: it multiplies the prototype by
 * `FPM_TONE_generate2`'s output and stores the product in a heap buffer,
 * so the coefficients the filter actually runs are the *modulated* set and
 * the filter resamples and downconverts in one pass.
 *
 * ---------------------------------------------------------------------------
 * WHY THIS IS NOT `struct fpm_mrf`, WHICH IT LOOKS LIKE
 *
 * The author kept `fpm_mrf.c` and `v22_mrf.c` as separate translation units
 * and the two objects are laid out differently.  The state has the same four
 * shorts in the same order -- `need`, `phase`, `widx`, `history_len` -- but
 * they sit at +0x08 rather than +0x10, because the configuration in front of
 * them is 8 bytes here and 16 there:
 *
 *      struct fpm_mrf_cfg      branches, decimate, coeff, taps, pad, aux
 *      struct v22_mrf_cfg      coeff, aux
 *
 * `V22_MRF_init` at 0x8d060 copies exactly two dwords (`mov (%esi),%ecx`,
 * `mov 0x4(%esi),%edx`), writes `need` to +0x08 and `history_len` to +0x0e,
 * and `V22_MRF_free` at 0x8d150 frees +0x10.  Every ratio `fpm_mrf` reads out
 * of its configuration is a literal here: 9 and 20 in the phase update, 30 in
 * the tap loop, 60 and 30 in the buffer management.  So the whole object is
 * 20 bytes against `fpm_mrf`'s 28, and sharing the type would mis-offset
 * every field.
 *
 * Three behavioural differences follow from the same split:
 *
 *   - `FPM_MRF_init` has three paths (fresh, reuse, replace-too-small) and a
 *     debug printf.  This one has two, no size check and no message: `fresh`
 *     allocates, anything else reuses the existing pointer unexamined.
 *   - The history is a 60-entry SLIDING buffer, not a 30-entry circular one.
 *     Samples are appended at `widx`; when the append would pass 60 the newer
 *     half is memcpy'd down over the older and `widx` drops by 30.  There is
 *     no wrap-around walk in the convolution.
 *   - `V22_MRF_init` REWRITES THE COEFFICIENT ARRAY IN PLACE.  See below.
 *
 * ---------------------------------------------------------------------------
 * INIT PERMUTES `cfg.coeff`, so the array must be writable and must not be
 * initialised twice from the same source
 *
 * The array arrives in natural impulse-response order, h[0 .. 269], and
 * leaves grouped by phase and reversed in time:
 *
 *      after[p * 30 + t]  =  before[9 * (29 - t) + p]      p 0..8, t 0..29
 *
 * which is what lets `V22_MRF_filter` run each output as a straight
 * contiguous dot product.  It is done through `cfg.coeff`, so `coeff` cannot
 * be `const` and `MRFv22_COFFS` -- which is in `.rodata` -- is never the
 * pointer passed in.  A second `V22_MRF_init` on the same array permutes it
 * again and silently produces a different filter; the original never does
 * that because `V22FP_create` regenerates the buffer immediately before
 * every call.
 */

#ifndef DSPLIB_V22_MRF_H
#define DSPLIB_V22_MRF_H

/*
 * Every one of these is a literal in the object, not a configured value.
 * They are named here so the source reads, not so it can be retargeted --
 * changing one does not change the filter the object implements.
 */
#define V22_MRF_PHASES		9	/* interpolation factor        */
#define V22_MRF_DECIMATE	20	/* phase step per output       */
#define V22_MRF_TAPS		30	/* taps per phase              */
#define V22_MRF_COEFFS		(V22_MRF_PHASES * V22_MRF_TAPS)	  /* 270 */
#define V22_MRF_HISTORY		(2 * V22_MRF_TAPS)		  /*  60 */

struct v22_mrf_cfg {
	/*
	 * Not `const`: init permutes the array through this pointer.
	 */
	short *coeff;		/* +0x00 V22_MRF_COEFFS entries          */
	/*
	 * +0x04 is copied wholesale by init and read by nothing in the
	 * object.  Its TYPE is therefore not established -- only that it is
	 * one dword and that it is always zero, since `V22_MRF_CFG` is
	 * all-zero .bss and `V22FP_create` patches only `coeff` in its stack
	 * copy.  Named `aux` for consistency with `struct fpm_mrf_cfg`,
	 * whose +0x0c is in the same position; that is a naming choice and
	 * not a derivation.
	 */
	void *aux;		/* +0x04                                 */
};

struct v22_mrf {
	struct v22_mrf_cfg cfg;	/* +0x00 copied wholesale by init        */
	short need;		/* +0x08 inputs owed before the next out */
	short phase;		/* +0x0a polyphase position, 0 .. 8      */
	short widx;		/* +0x0c next write slot in `history`    */
	short history_len;	/* +0x0e always V22_MRF_TAPS             */
	short *history;		/* +0x10 V22_MRF_HISTORY entries         */
};

/*
 * `fresh` non-zero allocates the history buffer; zero reuses whatever
 * `state->history` already points at, with no size check and no free.  Either
 * way the first `history_len` entries are zeroed and `cfg.coeff` is permuted.
 */
void V22_MRF_init(struct v22_mrf *state, const struct v22_mrf_cfg *cfg,
		  int fresh);
void V22_MRF_free(struct v22_mrf *state);

/*
 * Resample `count` input samples, returning the number of outputs written --
 * roughly count * 9 / 20.  `need`, `phase` and `widx` persist, so a stream
 * may be fed in arbitrary fragments, including ones too short to complete an
 * output.
 */
short V22_MRF_filter(struct v22_mrf *state, const short *in, short *out,
		     short count);

/*
 * The configuration template, and the one object in this pair that is in
 * `.bss` rather than `.rodata`: it is not `const`, so its all-zero initialiser
 * puts it there.  NOTHING IN THE OBJECT EVER WRITES IT.  Its only two
 * references are a pair of loads in `V22FP_create` at 0x87f3a and 0x87f3f
 * which copy it to the stack and overwrite `coeff` with the modulated
 * coefficient buffer before calling init, so both members are read as zero
 * on every call the object can make.
 */
extern struct v22_mrf_cfg V22_MRF_CFG;

/* The 270-tap prototype, in natural order -- see the permutation note. */
extern const short MRFv22_COFFS[V22_MRF_COEFFS];

#endif /* DSPLIB_V22_MRF_H */
