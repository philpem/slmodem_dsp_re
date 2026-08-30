/*
 * v22ctl.c -- V.22 / V.22bis: the datapump object's four exported accessors.
 * See include/dsplib/v22ctl.h, which carries the evidence.
 *
 * In the object's own order: 0x088480, 0x08c3b0, 0x08e670, 0x08e680 -- three
 * different address blocks, so this file is a LAYER and not a translation
 * unit, exactly as src/pump/v22/v22data.c records for the same four blocks.
 */

#include "dsplib/v22ctl.h"

#include "dsplib/fpm_agc.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22fp.h"

int
V22FP_GetDiagnostics(struct v22fp *fp)
{
	return V22_FSE_getdiag(&fp->dsp->fse);
}

int
V22FP_control(struct v22fp *fp, const struct v22fp_ctl *ctl)
{
	/*
	 * The two scrambler switches, from the two low bits.  Both are read
	 * out of one byte load, which is why they are taken in this order --
	 * the object shifts the loaded byte for bit 1 and masks the original
	 * for bit 0.
	 */
	fp->dsp->r1c = (ctl->flags_0c >> 1) & 1;
	fp->dsp->r18 = ctl->flags_0c & 1;

	/*
	 * The half-duplex pair, twice over.  The flag is tested first and the
	 * two-bit field second, so a control byte carrying both leaves the
	 * field's values in place; that ordering is the object's and is
	 * observable, which is why it is not tidied into an if/else.
	 */
	if (ctl->flags_0d & V22_CTL_HDX_SIX) {
		fp->hdx->r0e = V22_HDX_R0E_SIX;
		fp->hdx->r0c = 1;
	}
	if ((ctl->flags_0d >> V22_CTL_HDX_SHIFT) == V22_CTL_HDX_FOUR) {
		fp->hdx->r0e = V22_HDX_R0E_FOUR;
		fp->hdx->r0c = 0;
	}

	fp->dsp->r20 = (ctl->flags_0c >> 2) & 1;
	/* Inverted; see V22_CTL_FREEZE_ADAPT. */
	fp->dsp->agc.f18 = (ctl->flags_0c & V22_CTL_FREEZE_ADAPT) == 0;

	/*
	 * Bit 7 into `params.flags` bit 9.
	 *
	 * THE OBJECT DOES THIS AS A BYTE-WIDE READ-MODIFY-WRITE at +0x11 --
	 * `movzbl 0x11(%esi)`, `and $0xfd`, `or`, `mov %bl,0x11(%esi)` -- which
	 * is one of the reasons v22fp.h says the original probably declared
	 * `flags` as bitfields.  Written here as a 32-bit read-modify-write on
	 * the `unsigned int` that header settles on.  The two are
	 * BEHAVIOURALLY IDENTICAL: both preserve every other bit of the word,
	 * and no test can separate them.  What differs is the codegen tier,
	 * and the alternative -- indexing byte 1 of an `unsigned int` -- would
	 * buy that back only by writing endianness into src/.
	 */
	fp->params.flags = (fp->params.flags & ~V22_PARAMS_FLAG_BIT9)
			   | ((unsigned int)((ctl->flags_0c >> 7) & 1) << 9);

	/* A literal on every path, not a status. */
	return 1;
}

int
ScramblerOn(struct v22fp *fp)
{
	return fp->dsp->r18;
}

int
DescramblerOn(struct v22fp *fp)
{
	return fp->dsp->r1c;
}
