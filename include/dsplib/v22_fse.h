/*
 * v22_fse.h -- V.22 / V.22bis: the fractionally spaced equaliser block.
 *
 * The V.22 datapump's own adaptive equaliser and carrier recovery.  It is a
 * SEPARATE block from `fpm_fse.h`'s -- same job, same shape, different struct
 * and different entry points -- and the two must not be confused: `fpm_fse`'s
 * configuration is 56 bytes copied wholesale into the state and its geometry
 * comes from that configuration, whereas this one copies EIGHT bytes and has
 * every buffer size wired into `V22_FSE_init` as a literal.
 *
 * Reconstructed from dsplibs.o:
 *   V22_FSE_init     .text 0x08cd00   372 bytes
 *   V22_FSE_free     .text 0x08ce80    90 bytes
 *   V22_FSE_getdiag  .text 0x08c590     3 bytes
 *   FSEv22_decision12 and FSEv22_decision24 -- see V22Dec.c
 *   V22_FSE_receive  .text 0x08c5a0 1885 bytes
 *
 * WHAT THE BLOCK DOES, from `V22_FSE_receive`, which is the only thing in the
 * object that gives most of these fields a meaning:
 *
 *   - input samples are appended to a LINEAR history of 98 entries, and when
 *     the next block would not fit, the top 49 entries are copied down over
 *     the bottom 49 and the write index drops by 49.  It is not circular;
 *     `FPM_FSE_receive`'s is;
 *   - every `need` samples -- 1 the first time, then 6 -- a 49-tap real FIR is
 *     run over the newest 49 history entries with each of the two coefficient
 *     sets, giving one I/Q pair;
 *   - that pair is turned into polar form by `FPM_atan` and one `FPM_phasor`,
 *     the recovered carrier phase is SUBTRACTED FROM THE ANGLE, and one more
 *     `FPM_phasor` turns it back into the I/Q pair the slicer sees.  The
 *     sibling block derotates by a complex multiply; this one goes through
 *     polar coordinates, so its magnitude survives the derotation exactly;
 *   - `state->decision` slices that point and reports the ideal angle and
 *     magnitude back, and its return value is the output symbol;
 *   - a second-order PLL runs on the angle error, with the gain pair chosen by
 *     a smoothed-error band, and held wide until 49 symbols have passed;
 *   - the ideal point is rotated BACK into the equaliser's own frame and
 *     subtracted from the FIR output; that error drives an LMS update of all
 *     49 taps of both coefficient sets.
 *
 * THE HISTORY IS NOT PRE-FILLED.  While fewer than 49 entries have been
 * written the FIR reads `hist[0..48]` -- a FIXED window, not a sliding one --
 * so during the first eight symbols the newest sample is somewhere in the
 * middle of the window and moves one place per sample.  That is the object's
 * arithmetic and it is reproduced.
 *
 * WHERE THE STATE LIVES.  `V22FP_GetDiagnostics` does
 * `V22_FSE_getdiag((char *)modem[0x54] + 0x164)`, so the block is embedded at
 * +0x164 of the object v22prc.h calls `V22_OBJ_FP`.  That is the only thing
 * the object says about its home, and `V22FP_create` -- which lays that object
 * out -- is not reconstructed.
 *
 * HOW MUCH OF THE STRUCT IS NAMED.  Only the fields some reconstructed
 * function actually uses, which is the rule `v22prc.h` and `b103fp.h` already
 * follow: a name implies a meaning, and the meaning of a field nothing
 * reconstructed reads is unknowable.  The rest keep `rNN` names carrying their
 * offset.  `struct fpm_fse` has a field at the analogous place for most of
 * them and the resemblance is noted where it is close enough to be worth
 * checking later, but it is NOT used to name anything -- see the note on
 * +0x10..+0x1c.
 */

#ifndef DSPLIB_V22_FSE_H
#define DSPLIB_V22_FSE_H

struct v22_fse;

/*
 * The slicer, and the same contract as `fpm_fse_decision`: `angle` is in/out
 * -- the caller writes the measured angle and the slicer overwrites it with
 * the constellation point's ideal angle -- `mag` is out only, and the return
 * value is the decoded symbol, zero-extended from 16 bits.
 *
 * Both of the object's V.22 slicers take it: `V22_FSE_receive` reaches one
 * through `state->decision` with `call *0x60(%edx)`.
 */
typedef unsigned short (*v22_fse_decision)(struct v22_fse *state,
					   short *angle, short *mag);

/*
 * The configuration, EIGHT bytes, and both members are zero in the object's
 * only static instance.  `V22_FSE_init` copies exactly these two words into
 * the state and reads nothing else, so a caller builds one on the stack from
 * `FSEv22_CFG` and patches both pointers -- the same "copy the template, then
 * patch" shape `fpm_fse_cfg::owner` and `v22_pps_cfg::coeff_i` have.
 */
struct v22_fse_cfg {
	const short *icoff;	/* +0x00 initial I coefficients, 49 entries  */
	const short *qcoff;	/* +0x04 initial Q coefficients, 49 entries  */
};

/*
 * The geometry, every one of which is a LITERAL in `V22_FSE_init`'s
 * `sysdep_malloc` calls rather than anything derived from the configuration.
 * The element counts follow from the byte counts because every buffer is
 * `short *`: the coefficient loop indexes `icoeff` and `qcoeff` as shorts and
 * the slicers index `out_i` and `out_q` as shorts.
 */
#define V22_FSE_TAPS	49	/* 0x62 bytes: icoeff, qcoeff              */
#define V22_FSE_HIST	98	/* 0xc4 bytes: hist                        */
#define V22_FSE_AUX	20	/* 0x28 bytes: r44, r48                    */
#define V22_FSE_OUT	14	/* 0x1c bytes: out_i, out_q                */

/*
 * Input samples per symbol, and it is a LITERAL 6 in `V22_FSE_receive` and not
 * the six of `V22_CRR_CLK_STEPS` beside it, which the same function also
 * spells as a literal.  Two quantities that happen to be equal: this one is
 * the symbol interval, that one is the length of `CRRv22_CLK`.  600 baud at
 * 3600 Hz -- the V.22 receiver runs six samples to the symbol.
 */
#define V22_FSE_INTERP	6

/*
 * Symbols the PLL spends in its widest configuration.  `sym_count` counts up
 * to this and stops one past it, and while it is at or below, the integrator
 * is HELD AT ZERO -- so the loop is first-order for the first 49 symbols
 * whatever `pll_sel` would otherwise have selected.
 */
#define V22_FSE_TRAIN	48

/*
 * `v22_fse_mu` has two entries, from its 4 bytes in .rodata.  It is file-local
 * to src/pump/v22/v22_fse.c, so this is the bound and not a declaration.
 */
#define V22_FSE_MU	2

/*
 * 100 bytes.
 *
 * The size is the highest offset any reconstructed or read function touches
 * (+0x60, the slicer pointer) plus its width.  `V22_FSE_receive` reaches
 * nothing above +0x60 either, so 100 is a bound and not a guess -- but it is a
 * bound from use, so if a later reading finds a field above it the struct
 * grows rather than this having been wrong.
 */
struct v22_fse {
	/*
	 * The configuration, copied in by init.  These stay pointing at the
	 * caller's tables; the working copies below are what adapt.
	 */
	const short *icoff;	/* +0x00 <- cfg.icoff                       */
	const short *qcoff;	/* +0x04 <- cfg.qcoff                       */
	/*
	 * The LMS step size, as an index into the file-local `v22_fse_mu`.
	 * `V22_FSE_receive` reads it (`movswl 0x8(%esi),%ebp` at 0x8ca51) and
	 * indexes that two-entry table with it; nothing in the block writes it,
	 * so the datapump chooses the step.  Init leaves it 0, which is the
	 * larger of the two.
	 */
	short mu_sel;		/* +0x08 init 0                             */
	/*
	 * The PLL gain pair, as an index into `CRRv22_PLL_K1`/`CRRv22_PLL_K2`.
	 * Written by `V22_FSE_receive` only: 0 and 1 while `sym_count` is still
	 * counting, then 1 above the wide error threshold and 2 below the
	 * narrow one.  Set 0 has a zero K2, so the loop really is first-order
	 * during training rather than merely slow.
	 */
	short pll_sel;		/* +0x0a init 0                             */
	/*
	 * The PLL's frequency integrator, in the same phase units as `phase`.
	 * `freq += (K2[pll_sel] * err) >> 15` per symbol, and it is FORCED TO
	 * ZERO on every symbol up to `V22_FSE_TRAIN`.  A short, not the int
	 * `struct fpm_fse` uses -- `movw $0x0,0xc(%ebx)`.
	 */
	short freq;		/* +0x0c init 0                             */
	/*
	 * NOT written by init, and the test proves it: the byte pattern the
	 * harness's allocator leaves survives the call.  `V22_FSE_receive` does
	 * not touch it either.
	 */
	short r0e;		/* +0x0e                                    */
	/*
	 * Four ints, initialised 0, 1, 1, 1.  `struct fpm_fse` has
	 * `lms_force`, `pll_on`, `tilt_on`, `lms_on` at the analogous place
	 * with exactly those four values.  `V22_FSE_receive` reads the SECOND
	 * and the FOURTH, and each is a plain `test`-and-skip over one block of
	 * the loop, so those two are named from what they gate and not from the
	 * resemblance.  It reads NEITHER +0x10 nor +0x18, so those two keep
	 * their offsets for a name: the resemblance is not evidence, and the
	 * function that would settle them has not been found.
	 */
	int r10;		/* +0x10 init 0                             */
	int pll_on;		/* +0x14 init 1; 0 skips carrier recovery
				 *       entirely -- `mov 0x14(%ecx),%edx;
				 *       test %edx,%edx; je` at 0x8c8ec        */
	int r18;		/* +0x18 init 1                             */
	int lms_on;		/* +0x1c init 1; 0 skips the coefficient
				 *       update -- `mov 0x1c(%esi),%edi;
				 *       test %edi,%edi; je` at 0x8ca46        */
	/*
	 * The smoothed phase error, and the smoothed squared decision error.
	 * Both are the same first-order IIR, `x = (x * 0x799a >> 15) + (new *
	 * 0x666 >> 15)` -- 0.95 and 0.05 in Q15, each term shifted SEPARATELY,
	 * which is not the same as shifting the sum.  `err_avg` is what selects
	 * the PLL gain band; `mse` gates the LMS update, which runs only while
	 * it is strictly positive.
	 */
	short err_avg;		/* +0x20 init 0                             */
	short mse;		/* +0x22 init 0                             */
	/*
	 * One entry per symbol the last `V22_FSE_receive` call produced.  Both
	 * slicers read `out_i[n_out]` and `out_q[n_out]`, and the pairing is
	 * forced: the value from +0x24 is differenced against `DECv22_IMAP*`
	 * and the value from +0x28 against `DECv22_QMAP*`.
	 */
	short *out_i;		/* +0x24 V22_FSE_OUT entries                */
	short *out_q;		/* +0x28 V22_FSE_OUT entries                */
	/*
	 * The two counters, both rewritten by every `V22_FSE_receive` call:
	 * `n_in` is its `count` argument, stored on entry and never read back,
	 * and `n_out` is zeroed on entry, incremented once per symbol and
	 * RETURNED.  So `n_out` is per call, not cumulative, and it is also the
	 * slicers' index into `out_i`/`out_q` because the increment happens
	 * after the slicer returns.
	 *
	 * NOTHING BOUNDS `n_out` AGAINST `V22_FSE_OUT`.  Fifteen symbols in one
	 * call walks off both buffers; see the note on the receive prototype.
	 */
	short n_in;		/* +0x2c init 0                             */
	short n_out;		/* +0x2e the slicers' index into out_i/out_q*/
	/*
	 * The working coefficients, and what `V22_FSE_init` builds them from.
	 * They are the configuration's arrays REVERSED and divided by four --
	 * `icoeff[48 - i] = icoff[i] >> 2` -- which is why a symmetric
	 * prototype like `FSEv22_COFFS` cannot tell the reversal from its
	 * absence.
	 */
	short *icoeff;		/* +0x30 V22_FSE_TAPS entries               */
	short *qcoeff;		/* +0x34 V22_FSE_TAPS entries               */
	short *hist;		/* +0x38 V22_FSE_HIST entries, zeroed       */
	/*
	 * How many entries of `hist` are filled: the next input sample goes to
	 * `hist[hist_n]`, and the FIR reads the 49 entries BELOW it.  Not an
	 * index of the newest sample and not a circular cursor -- when
	 * `hist_n + need` would pass 98 the whole history is shifted down 49
	 * places and this drops by 49, so it walks 0..98 and never wraps.
	 */
	short hist_n;		/* +0x3c init 0                             */
	/*
	 * The recovered carrier phase and the sample clock's contribution to
	 * it.  `phase` is the PLL's own accumulator, wrapped into +-0x4000 by
	 * two conditional subtractions of 0x8000; `clk_phase` steps by TWO per
	 * input sample and is reduced modulo `V22_CRR_CLK_STEPS`, indexing
	 * `CRRv22_CLK`.  The carrier the derotation uses is the sum.
	 *
	 * Because the step is even and init leaves `clk_phase` zero, only the
	 * even entries of `CRRv22_CLK` are reachable unless the datapump seeds
	 * this field -- and only entry 2 is reachable at all if every call is a
	 * whole number of symbols.
	 */
	short phase;		/* +0x3e init 0                             */
	short clk_phase;	/* +0x40 init 0                             */
	short r42;		/* +0x42 init 0                             */
	/*
	 * Allocated by init and released by free, and read by NOTHING that has
	 * been read out of the object so far -- not by the slicers and not by
	 * `V22_FSE_receive`.  So they belong to a caller, and there is no
	 * basis for a name.
	 */
	short *r44;		/* +0x44 V22_FSE_AUX entries                */
	short *r48;		/* +0x48 V22_FSE_AUX entries                */
	/*
	 * NOT written by init.  Ten bytes rather than a shape, because nothing
	 * reconstructed reads any of it; `tools/whichfield.py` reporting an
	 * offset in here is the answer that this part is not modelled.
	 */
	unsigned char r4c[10];	/* +0x4c .. +0x55                           */
	/*
	 * Symbols since init, counted only while the PLL is enabled and held at
	 * `V22_FSE_TRAIN + 1` for ever after.  Its only use is to hold the
	 * integrator at zero and the gain band wide during training.
	 */
	short sym_count;	/* +0x56 init 0                             */
	/*
	 * Input samples owed before the next symbol.  Init leaves it 1, so the
	 * first symbol comes out after ONE sample; `V22_FSE_receive` sets it to
	 * `V22_FSE_INTERP` after that, and a call that runs out of input part
	 * way through an interval leaves the remainder here.
	 */
	short need;		/* +0x58 init 1                             */
	short r5a;		/* +0x5a not written by init                */
	/*
	 * The previous symbol's quadrant, kept OUTSIDE this block -- both
	 * slicers load the pointer, read a `short` through it, and store the
	 * new quadrant back through it.  Nothing here initialises it, so the
	 * datapump supplies it.
	 */
	short *prev_quad;	/* +0x5c                                    */
	v22_fse_decision decision;	/* +0x60 called by V22_FSE_receive  */
};

/*
 * The 49-tap prototype, symmetric about its centre.  `V22FP_create` is the
 * only thing in the object that names it, and it hands it to init through
 * `struct v22_fse_cfg`.
 */
extern const short FSEv22_COFFS[V22_FSE_TAPS];

/* The template: both pointers zero, for a caller to copy and patch. */
extern const struct v22_fse_cfg FSEv22_CFG;

/**
 * @brief Initialise (or re-arm) the V.22 equaliser.
 *
 * @p fresh non-zero allocates the seven buffers; zero reuses whatever the
 * struct already holds. That is the OPPOSITE sense to `FPM_FSE_init`, where
 * zero means "re-init" and frees the old buffers before allocating again --
 * this one never frees and never reallocates when told not to. Both are
 * spelled `fresh` here because the flag is in the same argument position
 * and the same word describes what it selects; the difference is in what
 * happens on the other branch, which is why it is written down.
 *
 * Everything below the allocation happens on both paths: the coefficients
 * are rebuilt from the configuration and the history is zeroed whether or
 * not the buffers are new.
 *
 * @param state  The equaliser state to initialise.
 * @param cfg    The initial coefficient tables.
 * @param fresh  Non-zero to allocate the seven buffers; zero to reuse existing ones.
 */
void V22_FSE_init(struct v22_fse *state, const struct v22_fse_cfg *cfg,
		  int fresh);

/**
 * @brief Release the V.22 equaliser's seven buffers, in the reverse of the order V22_FSE_init() allocated them.
 * @param state  The equaliser state to tear down.
 */
void V22_FSE_free(struct v22_fse *state);

/**
 * @brief The V.22 equaliser's diagnostic hook. Always returns zero and reads nothing.
 *
 * The object's is three bytes -- `xor %eax,%eax; ret` -- against 0xe5 for
 * the general-purpose `FSE_getdiag`, so it is a stub and not a smaller
 * version of that function. Its arity is not settled by the object:
 * `V22FP_GetDiagnostics` tail-jumps to it after rewriting only the first
 * argument, so any further arguments its caller passed are still in
 * place, and a function that reads none of them cannot say how many there
 * were. One parameter is declared because one is what the object is seen
 * to pass; under cdecl a caller passing more is harmless.
 *
 * @param state  The equaliser state (unused).
 * @return Always 0.
 */
int V22_FSE_getdiag(struct v22_fse *state);

/**
 * @brief Run the V.22 equaliser over one block of input samples.
 *
 * One block of input samples in, one symbol per `V22_FSE_INTERP` of them
 * out. The return value is `state->n_out`, the number of symbols this call
 * produced, and the same count applies to @p out, to `state->out_i` and to
 * `state->out_q`.
 *
 * @p count IS SIGNED and a negative one is not the same as zero: zero
 * returns without entering the loop, and a negative count enters it, takes
 * the short-block path and leaves without stashing anything -- so neither
 * produces a symbol, but only the negative one can decrement `need`. It
 * cannot: the stash is guarded by `count > 0`. Reproduced from the
 * object's `test`/`cmpw $0x0` pair rather than tidied into one test.
 *
 * The caller must not ask for more than `V22_FSE_OUT` symbols. `out_i` and
 * `out_q` are 14 entries, the function writes one of each per symbol, and
 * there is no bound anywhere in it -- 84 samples in one call (79 on the
 * first) overruns both heap buffers. The datapump's own block size is not
 * reconstructed, so this is stated rather than enforced: adding a check
 * here would be a fix, and this file is a reconstruction.
 *
 * @param state  The equaliser state.
 * @param in     Input samples.
 * @param out    Output for the decoded symbols.
 * @param count  How many input samples; signed, see above.
 * @return The number of symbols produced (`state->n_out`).
 */
unsigned short V22_FSE_receive(struct v22_fse *state, const short *in,
			       unsigned short *out, short count);

#endif /* DSPLIB_V22_FSE_H */
