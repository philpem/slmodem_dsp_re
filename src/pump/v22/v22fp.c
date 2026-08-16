/*
 * v22fp.c -- V.22 / V.22bis: the datapump object's constructor and destructor.
 *
 *   V22FP_create   .text 0x087990  2,449 bytes
 *   V22FP_delete   .text 0x088330    332 bytes
 *
 * See include/dsplib/v22fp.h for the layout and where each field's name comes
 * from.  What is worth saying beside the code:
 *
 * THE SECOND ARGUMENT TO THE FOUR `*_free` CALLS IS DROPPED.  `V22FP_delete`
 * pushes a literal 1 as a second argument to `V22_PPS_free`, `V22_MRF_free`,
 * `V22_SRE_free` and `V22_FSE_free`, four times over with a fresh `mov $0x1`
 * each time, so the author declared those four with a second parameter.  None
 * of the four reads it -- each touches only `0x4(%esp)` or `0x10(%esp)`, which
 * is the first -- and this tree reconstructed all four from their own bodies
 * with one parameter apiece.  Adding a second would mean changing four
 * headers, one of which (v22_fse.h) another effort owns.  cdecl is
 * caller-cleaned and the callee never looks, so nothing observable turns on
 * it; recorded here so that a later reader finding the push is not left
 * thinking an argument went missing.
 *
 * CREATE DOES NOT CHECK ANY ALLOCATION.  Eleven `sysdep_malloc` calls, no
 * NULL test on any of them, and the very next instruction after the first
 * dereferences it.  `B103FP_create` in the same object does check.  Faithful,
 * and it is why `V22FP_create` cannot be handed a zeroed buffer either: the
 * `fp != NULL` path reads `fp->dsp` immediately.
 *
 * THE THROWAWAY OSCILLATOR.  Three of the object's coefficient sets are not
 * stored tables at all -- they are a stored PROTOTYPE multiplied sample by
 * sample by a carrier that create synthesises with an `fpm_tone` object built
 * for the purpose, retuned three times, and deleted before create returns.
 * That is why `FPM_TONE_generate2` appears in a constructor.
 */

#include "dsplib/debug.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22_iir.h"
#include "dsplib/v22_mrf.h"
#include "dsplib/v22_pps.h"
#include "dsplib/v22_sre.h"
#include "dsplib/v22dec.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22tab.h"
#include "dsplib/v22txtab.h"

/*
 * Release the whole tree: the four embedded blocks' own buffers, the four
 * heap sub-objects, the seven raw buffers, then the two blocks and the object.
 *
 * The order is the object's, and it is not the reverse of create's.
 */
void
V22FP_delete(struct v22fp *fp)
{
	V22_PPS_free(&fp->dsp->pps);
	V22_MRF_free(&fp->dsp->mrf);
	V22_SRE_free(&fp->dsp->sre);
	V22_FSE_free(&fp->dsp->fse);

	FPM_TONE_delete(fp->hdx->tone);
	FPM_MTD_delete(fp->hdx->mtd);
	FPM_MTD_delete(fp->hdx->mtd_s1);
	FPM_MTD_delete(fp->hdx->mtd2);
	sysdep_free(fp->hdx->iir);

	sysdep_free(fp->dsp->ra8);
	sysdep_free(fp->dsp->pps_coff_i);
	sysdep_free(fp->dsp->pps_coff_q);
	sysdep_free(fp->dsp->mrf_coeff);
	sysdep_free(fp->dsp->fse_coff_i);
	sysdep_free(fp->dsp->fse_coff_q);
	sysdep_free(fp->dsp->r1f4);

	sysdep_free(fp->hdx);
	sysdep_free(fp->dsp);
	sysdep_free(fp);
}

/*
 * ---------------------------------------------------------------------------
 * The layout, held to the compiler.
 *
 * 32-BIT ONLY, and necessarily so: the reserved regions above are byte counts
 * measured from an object whose pointers are four bytes, so under any other
 * ABI the named fields land elsewhere and these are simply false rather than
 * violated.  Same argument as src/pump/b103/b103fp.c.
 *
 * The last block is the interesting one: it holds `v22prc.h`'s nine offset
 * constants -- written before this object was modelled, from nine other
 * functions' loads and stores -- against the struct.  Nine independent
 * addresses landing on nine plausible fields is the check that no amount of
 * differential testing of `create` alone could provide.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V22FP_ASSERT_OFF(tag, type, field, off) \
	typedef char v22fp_off_##tag[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V22FP_ASSERT_OFF(p_bps, struct v22fp_params, bps, 0x02);
V22FP_ASSERT_OFF(p_bps2, struct v22fp_params, bps2, 0x04);
V22FP_ASSERT_OFF(p_r08, struct v22fp_params, r08, 0x08);
V22FP_ASSERT_OFF(p_flags, struct v22fp_params, flags, 0x10);
V22FP_ASSERT_OFF(p_r14, struct v22fp_params, r14, 0x14);
V22FP_ASSERT_OFF(p_thresh, struct v22fp_params, disconnect_thresh, 0x16);
V22FP_ASSERT_OFF(p_r18, struct v22fp_params, r18, 0x18);

V22FP_ASSERT_OFF(h_r04, struct v22fp_hdx, r04, 0x04);
V22FP_ASSERT_OFF(h_r0e, struct v22fp_hdx, r0e, 0x0e);
V22FP_ASSERT_OFF(h_tone, struct v22fp_hdx, tone, 0x14);
V22FP_ASSERT_OFF(h_mtd, struct v22fp_hdx, mtd, 0x18);
V22FP_ASSERT_OFF(h_mtd_s1, struct v22fp_hdx, mtd_s1, 0x1c);
V22FP_ASSERT_OFF(h_mtd2, struct v22fp_hdx, mtd2, 0x20);
V22FP_ASSERT_OFF(h_iir, struct v22fp_hdx, iir, 0x24);
V22FP_ASSERT_OFF(h_r30, struct v22fp_hdx, r30, 0x30);
V22FP_ASSERT_OFF(h_r3c, struct v22fp_hdx, r3c, 0x3c);

V22FP_ASSERT_OFF(d_r20, struct v22fp_dsp, r20, 0x20);
V22FP_ASSERT_OFF(d_r28, struct v22fp_dsp, r28, 0x28);
V22FP_ASSERT_OFF(d_r2e, struct v22fp_dsp, r2e, 0x2e);
V22FP_ASSERT_OFF(d_sdm, struct v22fp_dsp, sdm, 0x30);
V22FP_ASSERT_OFF(d_smc, struct v22fp_dsp, smc, 0x48);
V22FP_ASSERT_OFF(d_pps, struct v22fp_dsp, pps, 0x78);
V22FP_ASSERT_OFF(d_ra8, struct v22fp_dsp, ra8, 0xa8);
V22FP_ASSERT_OFF(d_rb0, struct v22fp_dsp, rb0, 0xb0);
V22FP_ASSERT_OFF(d_ppsi, struct v22fp_dsp, pps_coff_i, 0xb4);
V22FP_ASSERT_OFF(d_ppsq, struct v22fp_dsp, pps_coff_q, 0xb8);
V22FP_ASSERT_OFF(d_mrf, struct v22fp_dsp, mrf, 0xbc);
V22FP_ASSERT_OFF(d_agc, struct v22fp_dsp, agc, 0xd0);
V22FP_ASSERT_OFF(d_agc2, struct v22fp_dsp, agc2, 0xfc);
V22FP_ASSERT_OFF(d_sre, struct v22fp_dsp, sre, 0x128);
V22FP_ASSERT_OFF(d_fse, struct v22fp_dsp, fse, 0x164);
V22FP_ASSERT_OFF(d_quad, struct v22fp_dsp, prev_quad, 0x1c8);
V22FP_ASSERT_OFF(d_sdm2, struct v22fp_dsp, sdm2, 0x1cc);
V22FP_ASSERT_OFF(d_mrfc, struct v22fp_dsp, mrf_coeff, 0x1e4);
V22FP_ASSERT_OFF(d_fsei, struct v22fp_dsp, fse_coff_i, 0x1e8);
V22FP_ASSERT_OFF(d_fseq, struct v22fp_dsp, fse_coff_q, 0x1ec);
V22FP_ASSERT_OFF(d_r1f4, struct v22fp_dsp, r1f4, 0x1f4);

V22FP_ASSERT_OFF(o_status, struct v22fp, status, 0x1c);
V22FP_ASSERT_OFF(o_flags, struct v22fp, flags, 0x1d);
V22FP_ASSERT_OFF(o_out_i, struct v22fp, out_i, 0x20);
V22FP_ASSERT_OFF(o_out_q, struct v22fp, out_q, 0x24);
V22FP_ASSERT_OFF(o_n_out, struct v22fp, n_out, 0x28);
V22FP_ASSERT_OFF(o_icoeff, struct v22fp, icoeff, 0x2c);
V22FP_ASSERT_OFF(o_qcoeff, struct v22fp, qcoeff, 0x30);
V22FP_ASSERT_OFF(o_r34, struct v22fp, r34, 0x34);
V22FP_ASSERT_OFF(o_r4c, struct v22fp, r4c, 0x4c);

typedef char v22fp_size[(sizeof(struct v22fp) == 0x5c) ? 1 : -1];
typedef char v22fp_hdx_size[(sizeof(struct v22fp_hdx) == 0x40) ? 1 : -1];
typedef char v22fp_dsp_size[(sizeof(struct v22fp_dsp) == 0x1f8) ? 1 : -1];
typedef char v22fp_params_size[(sizeof(struct v22fp_params) == 28) ? 1 : -1];
typedef char v22fp_cfg_size[(sizeof(struct v22fp_cfg) == 28) ? 1 : -1];

/*
 * v22prc.h's nine constants, resolved.  Each was derived from a load or a
 * store in a DIFFERENT function, none of which knew where the object's
 * sub-blocks began; that all nine land on a field is the corroboration.
 *
 * Three of them resolve INSIDE `struct v22_fse` and one inside `struct
 * v22_sre`, so the constants stay: retiring them would mean naming fields in
 * headers this task must not edit.  The mapping is recorded here instead.
 *
 *   V22_OBJ_GTIMER  0x50  -> struct v22fp::hdx, and hdx->gtimer beyond it
 *   V22_OBJ_FP      0x54  -> struct v22fp::dsp
 *   V22FP_EQ_ADAPT  0x10  -> dsp->eq_adapt      (create leaves 1)
 *   V22FP_TX_CLOCK  0x78  -> dsp->pps.cfg.step  (see the note below)
 *   V22FP_SIGNAL    0xec  -> dsp->agc.signal
 *   V22FP_BAUD     0x12a  -> dsp->sre.pll_acc   (see the note below)
 *   V22FP_CARRIER  0x130  -> dsp->sre.active
 *   V22FP_EQ_MODE  0x16c  -> dsp->fse.r08       (init 0)
 *   V22FP_EQ_EXTRA 0x180  -> dsp->fse.r1c       (init 1)
 *   V22FP_QUALITY  0x186  -> dsp->fse.r22       (init 0)
 *
 * TWO OF THE TEN ARE AN OPEN QUESTION AND ARE LEFT AS ONE.  `TxClockSync`
 * stores a short at the object's +0x54 +0x78, which is `v22_pps_cfg::step` --
 * a field `V22_PPS_init` copies in from the configuration and `V22_PPS_filter`
 * reads on every output.  `TxClockSync` therefore overwrites a configuration
 * word after init, which is either a deliberate rate correction or a soft
 * spot in one of the two readings.  `V22FP_BAUD` at +0x12a is
 * `v22_sre::pll_acc` and has the same shape.  Nothing here decides it, and
 * v22prc.h's names are NOT propagated inward on the strength of an offset
 * agreeing.
 */
typedef char v22fp_prc_gtimer[
	((int)__builtin_offsetof(struct v22fp, hdx) == V22_OBJ_GTIMER)
		? 1 : -1];
typedef char v22fp_prc_fp[
	((int)__builtin_offsetof(struct v22fp, dsp) == V22_OBJ_FP) ? 1 : -1];
typedef char v22fp_prc_adapt[
	((int)__builtin_offsetof(struct v22fp_dsp, eq_adapt)
		== V22FP_EQ_ADAPT) ? 1 : -1];
typedef char v22fp_prc_txclock[
	((int)__builtin_offsetof(struct v22fp_dsp, pps) == V22FP_TX_CLOCK)
		? 1 : -1];
typedef char v22fp_prc_signal[
	((int)__builtin_offsetof(struct v22fp_dsp, agc)
	 + (int)__builtin_offsetof(struct fpm_agc, signal)
		== V22FP_SIGNAL) ? 1 : -1];
typedef char v22fp_prc_baud[
	((int)__builtin_offsetof(struct v22fp_dsp, sre)
	 + (int)__builtin_offsetof(struct v22_sre, pll_acc)
		== V22FP_BAUD) ? 1 : -1];
typedef char v22fp_prc_carrier[
	((int)__builtin_offsetof(struct v22fp_dsp, sre)
	 + (int)__builtin_offsetof(struct v22_sre, active)
		== V22FP_CARRIER) ? 1 : -1];
typedef char v22fp_prc_eqmode[
	((int)__builtin_offsetof(struct v22fp_dsp, fse)
	 + (int)__builtin_offsetof(struct v22_fse, r08)
		== V22FP_EQ_MODE) ? 1 : -1];
typedef char v22fp_prc_eqextra[
	((int)__builtin_offsetof(struct v22fp_dsp, fse)
	 + (int)__builtin_offsetof(struct v22_fse, r1c)
		== V22FP_EQ_EXTRA) ? 1 : -1];
typedef char v22fp_prc_quality[
	((int)__builtin_offsetof(struct v22fp_dsp, fse)
	 + (int)__builtin_offsetof(struct v22_fse, r22)
		== V22FP_QUALITY) ? 1 : -1];

#endif /* 32-bit */
