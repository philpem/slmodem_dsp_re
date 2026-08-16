/*
 * v22_fse.c -- V.22 / V.22bis: the fractionally spaced equaliser block.
 *
 * Reconstructed from dsplibs.o:
 *   V22_FSE_getdiag  .text   0x08c590     3 bytes
 *   V22_FSE_init     .text   0x08cd00   372 bytes
 *   V22_FSE_free     .text   0x08ce80    90 bytes
 *   FSEv22_COFFS     .rodata 0x008640    98 bytes
 *   FSEv22_CFG       .rodata 0x0086a4     8 bytes
 *
 * `V22_FSE_receive` (0x08c5a0, 1,885 bytes) is in this translation unit in the
 * object and is not yet written; so is the file-local `v22_fse_mu`, which only
 * it reads.  See dsplib/v22_fse.h for what the block is.
 */

#include "dsplib/sysdep.h"
#include "dsplib/v22_fse.h"

/*
 * The prototype the datapump hands to `V22_FSE_init`.  Symmetric: entry i and
 * entry 48 - i are equal for every i, which is why the reversal init performs
 * is invisible to any test driven with this table.  Byte-exact from .rodata;
 * no derivation is attempted (docs/fastpass.md defers that to the retarget).
 */
const short FSEv22_COFFS[V22_FSE_TAPS] = {
	    9,     9,    -6,   -23,   -30,    -8,    38,    75,
	   45,   -76,  -223,  -255,   -64,   280,   477,   144,
	 -844, -2032, -2331,  -445,  4405, 11801, 19980, 26405,
	28841, 26405, 19980, 11801,  4405,  -445, -2331, -2032,
	 -844,   144,   477,   280,   -64,  -255,  -223,   -76,
	   45,    75,    38,    -8,   -30,   -23,    -6,     9,
	    9,
};

/*
 * Eight zero bytes in .rodata, with no relocation on either word, so both
 * pointers really are null and the caller patches them.  Kept as a named
 * object rather than folded away because `V22FP_create` loads it by name.
 */
const struct v22_fse_cfg FSEv22_CFG = { 0, 0 };

void
V22_FSE_init(struct v22_fse *state, const struct v22_fse_cfg *cfg, int fresh)
{
	short i;

	state->icoff = cfg->icoff;
	state->qcoff = cfg->qcoff;

	state->r08 = 0;
	state->r0a = 0;
	state->r0c = 0;
	state->r10 = 0;
	state->r14 = 1;
	state->r18 = 1;
	state->r1c = 1;
	state->r20 = 0;
	state->r22 = 0;
	state->r2c = 0;
	state->n_out = 0;
	state->r3c = 0;
	state->r3e = 0;
	state->r40 = 0;
	state->r42 = 0;
	state->r56 = 0;
	state->r58 = 1;

	/*
	 * Non-zero means the buffers do not exist yet.  Zero reuses them
	 * without inspecting them, which is why nothing here frees: see the
	 * note in the header about how this differs from `FPM_FSE_init`.
	 *
	 * Every size is a literal in the object -- 0x62, 0x62, 0xc4, 0x28,
	 * 0x28, 0x1c, 0x1c -- and none of them is derived from `cfg`.
	 */
	if (fresh) {
		state->icoeff = (short *)sysdep_malloc(2 * V22_FSE_TAPS);
		state->qcoeff = (short *)sysdep_malloc(2 * V22_FSE_TAPS);
		state->hist = (short *)sysdep_malloc(2 * V22_FSE_HIST);
		state->r44 = (short *)sysdep_malloc(2 * V22_FSE_AUX);
		state->r48 = (short *)sysdep_malloc(2 * V22_FSE_AUX);
		state->out_i = (short *)sysdep_malloc(2 * V22_FSE_OUT);
		state->out_q = (short *)sysdep_malloc(2 * V22_FSE_OUT);
	}

	/*
	 * The working coefficients are the configuration's arrays REVERSED and
	 * arithmetically shifted down two.  The reversal is what turns a
	 * causal prototype into the convolution order the filter walks; the
	 * shift is head-room for the LMS update, which adds to these in place.
	 *
	 * `hist` is zeroed here for its first 49 entries and again below for
	 * all 98.  The overlap is the object's, reproduced rather than tidied.
	 */
	for (i = 0; i <= V22_FSE_TAPS - 1; i++) {
		state->icoeff[V22_FSE_TAPS - 1 - i] = (short)(state->icoff[i] >> 2);
		state->qcoeff[V22_FSE_TAPS - 1 - i] = (short)(state->qcoff[i] >> 2);
		state->hist[i] = 0;
	}

	for (i = 0; i <= V22_FSE_HIST - 1; i++)
		state->hist[i] = 0;
}

void
V22_FSE_free(struct v22_fse *state)
{
	sysdep_free(state->out_q);
	sysdep_free(state->out_i);
	sysdep_free(state->r48);
	sysdep_free(state->r44);
	sysdep_free(state->hist);
	sysdep_free(state->qcoeff);
	sysdep_free(state->icoeff);
}

/*
 * Three bytes: `xor %eax,%eax; ret`.  It does not touch `state`; the parameter
 * is declared because `V22FP_GetDiagnostics` is seen to pass one.
 */
int
V22_FSE_getdiag(struct v22_fse *state)
{
	(void)state;
	return 0;
}
