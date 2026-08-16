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
 *   FSEv22_decision12 and FSEv22_decision24 -- see v22dec.c
 *   V22_FSE_receive  .text 0x08c5a0  NOT YET RECONSTRUCTED
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
	short r08;		/* +0x08 init 0                             */
	short r0a;		/* +0x0a init 0                             */
	short r0c;		/* +0x0c init 0                             */
	/*
	 * NOT written by init, and the test proves it: the byte pattern the
	 * harness's allocator leaves survives the call.
	 */
	short r0e;		/* +0x0e                                    */
	/*
	 * Four ints, initialised 0, 1, 1, 1.  `struct fpm_fse` has
	 * `lms_force`, `pll_on`, `tilt_on`, `lms_on` at the analogous place
	 * with exactly those four values, which is suggestive and is NOT
	 * evidence: nothing reconstructed here reads any of them, and
	 * `V22_FSE_receive` is where the meaning would have to come from.
	 * Left unnamed deliberately.
	 */
	int r10;		/* +0x10 init 0                             */
	int r14;		/* +0x14 init 1                             */
	int r18;		/* +0x18 init 1                             */
	int r1c;		/* +0x1c init 1                             */
	short r20;		/* +0x20 init 0                             */
	short r22;		/* +0x22 init 0                             */
	/*
	 * One entry per symbol the last `V22_FSE_receive` call produced.  Both
	 * slicers read `out_i[n_out]` and `out_q[n_out]`, and the pairing is
	 * forced: the value from +0x24 is differenced against `DECv22_IMAP*`
	 * and the value from +0x28 against `DECv22_QMAP*`.
	 */
	short *out_i;		/* +0x24 V22_FSE_OUT entries                */
	short *out_q;		/* +0x28 V22_FSE_OUT entries                */
	short r2c;		/* +0x2c init 0                             */
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
	short r3c;		/* +0x3c init 0                             */
	short r3e;		/* +0x3e init 0                             */
	short r40;		/* +0x40 init 0                             */
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
	short r56;		/* +0x56 init 0                             */
	short r58;		/* +0x58 init 1                             */
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

/*
 * `fresh` NON-ZERO allocates the seven buffers; zero reuses whatever the
 * struct already holds.  THAT IS THE OPPOSITE SENSE TO `FPM_FSE_init`, where
 * zero means "re-init" and frees the old buffers before allocating again --
 * this one never frees and never reallocates when told not to.  Both are
 * spelled `fresh` here because the flag is in the same argument position and
 * the same word describes what it selects; the difference is in what happens
 * on the other branch, which is why it is written down.
 *
 * Everything below the allocation happens on both paths: the coefficients are
 * rebuilt from the configuration and the history is zeroed whether or not the
 * buffers are new.
 */
void V22_FSE_init(struct v22_fse *state, const struct v22_fse_cfg *cfg,
		  int fresh);

/* Releases all seven, in the reverse of the order init allocated them. */
void V22_FSE_free(struct v22_fse *state);

/*
 * Returns zero, always, and reads nothing.
 *
 * The object's is three bytes -- `xor %eax,%eax; ret` -- against 0xe5 for the
 * general-purpose `FSE_getdiag`, so it is a STUB and not a smaller version of
 * that function.  ITS ARITY IS NOT SETTLED BY THE OBJECT: `V22FP_GetDiagnostics`
 * tail-jumps to it after rewriting only the first argument, so any further
 * arguments its caller passed are still in place, and a function that reads
 * none of them cannot say how many there were.  One parameter is declared
 * because one is what the object is seen to pass; under cdecl a caller passing
 * more is harmless.
 */
int V22_FSE_getdiag(struct v22_fse *state);

#endif /* DSPLIB_V22_FSE_H */
