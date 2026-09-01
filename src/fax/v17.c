/*
 * v17.c -- ITU-T V.17 (fax): the receiver's primitives, and the transmit-side
 *          setters that sit beside them.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V17RX_delete      .text 0x097b40  251
 *   V17TX_delete      .text 0x098e00  107
 *   V17RX_modem       .text 0x09ff80  127
 *   RxHdxDataV17      .text 0x0a0000  226
 *   RxHdxErrorV17     .text 0x0a00f0   59
 *   V17RX_status      .text 0x0a0910  190
 *   ScrambleDataV17   .text 0x0a09d0   28
 *   SeedScramblerV17  .text 0x0a09f0   15
 *   SetEncoderV17     .text 0x0a0a00   90
 *   V17TX_modem       .text 0x0a0e40  182
 *   V17TX_status      .text 0x0a1bd0  106
 *   DemodDataV17      .text 0x0a50a0  415
 *   DescrambleDataV17 .text 0x0a5240   30
 *   CarrierDetectV17      .text 0x0a5260  121
 *   DataCarrierDetectV17  .text 0x0a52e0  625
 *   QualityDetectV17      .text 0x0a5560  266
 *   EpochDetectV17    .text 0x0a5670   22
 *   GetSNRV17         .text 0x0a5690   23
 *   StoreCoefV17      .text 0x0a56b0   81
 *   Restore_rateV17   .text 0x0a5710   37
 *
 * `include/dsplib/v17fax.h` carries the offset evidence and the naming.
 *
 * ---------------------------------------------------------------------------
 * THIS IS NOT ONE TRANSLATION UNIT, AND THE ADDRESSES SAY SO
 *
 * `tools/tumap.py` brackets 95 units together as `class1tx.c +94`, so it
 * cannot separate them -- but the symbols above span 0x09ff80 to 0x0a5735,
 * about 22 KB, with hundreds of unrelated functions between them.  GCC emits
 * one unit's functions contiguously, so at least three units are represented
 * here.  The definitions are ORDERED BY THE OBJECT'S OWN ADDRESSES anyway,
 * because emission order is a register-allocation carrier (CLAUDE.md, finding
 * F7796) and the object's order is the only one that is evidence.
 *
 * ---------------------------------------------------------------------------
 * THE SAME FIELD IS NOT ALWAYS THE SAME WIDTH, AND EACH SITE FOLLOWS THE OBJECT
 *
 * `CarrierDetectV17` loads receiver state + 0xd0 with a 32-bit `mov`;
 * `QualityDetectV17` loads it with `movswl`.  Neither instruction was free --
 * a `short` cannot produce the first and an `int` cannot produce the second --
 * so the two functions did not share a declaration and this file does not
 * make them share one.  The readings differ whenever the short at +0xd2 is
 * non-zero, and `t_v17fax.c` runs that case on purpose.  Finding F8853.
 *
 * The field itself is `struct fpm_agc::signal`, which the four FPM objects'
 * exact tiling of the state block identifies -- see `V17RXS_AGC_SIGNAL` in
 * v17fax.h and finding F8854.  It only ever holds 0 or 1, so on any state a
 * real receiver can reach the two readings agree; the width is followed
 * because the object was not free to choose it, not because it is reachable.
 *
 * ---------------------------------------------------------------------------
 * THE RELOADS ARE FORCED, SO THEY ARE WRITTEN AS RELOADS
 *
 * Every function below that calls anything re-reads `V17RX_OBJ_CTL` or
 * `V17RX_OBJ_STATE` after the call rather than keeping it in a register.  That
 * is not a style: a call clobbers memory the compiler cannot see through, so a
 * source that read the field once could not have produced it.  The `CTL()` and
 * `RXS()` macros therefore expand at each use, and the object's reload pattern
 * comes out of the C rather than being imitated.  `src/fax/v29.c` records the
 * same thing for the same reason.
 */

#include "dsplib/v17fax.h"

#include "dsplib/debug.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/sysdep.h"

/* The instances are not modelled; see v17fax.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))

#define AT_S(p, off)		(*(short *)(void *)FIELD((p), (off)))
#define AT_US(p, off)		(*(unsigned short *)(void *)FIELD((p), (off)))
#define AT_I(p, off)		(*(int *)(void *)FIELD((p), (off)))
#define AT_B(p, off)		(*(unsigned char *)FIELD((p), (off)))

#define CTL(modem)		FIELD_PTR((modem), V17RX_OBJ_CTL)
#define RXS(modem)		FIELD_PTR((modem), V17RX_OBJ_STATE)

/*
 * The four FPM objects the receive chain runs, reached the long way round
 * because the block they tile is not modelled.  See F8854 for the tiling and
 * v17fax.h for each offset's evidence.
 */
#define RXS_MRF(rxs)	((struct fpm_mrf *)(void *)FIELD((rxs), V17RXS_MRF))
#define RXS_AGC(rxs)	((struct fpm_agc *)(void *)FIELD((rxs), V17RXS_AGC))
#define RXS_SRE(rxs)	((struct fpm_sre *)(void *)FIELD((rxs), V17RXS_SRE))
#define RXS_FSE(rxs)	((struct fpm_fse *)(void *)FIELD((rxs), V17RXS_FSE))

/* --------------------------------------------------------------------- */

/*
 * V17RX_delete -- .text 0x097b40, 251 bytes.  See v17fax.h for the order and
 * for why the object's literal 1 in the second argument slot is not here.
 */
void
V17RX_delete(void *modem)
{
	SGD_delete((struct sgd *)FIELD_PTR(RXS(modem), V17RXS_SGD));
	sysdep_free(FIELD_PTR(RXS(modem), V17RXS_PTR_0030));

	FPM_FSE_free(RXS_FSE(RXS(modem)));
	FPM_SRE_free(RXS_SRE(RXS(modem)));
	FPM_MRF_free(RXS_MRF(RXS(modem)));

	sysdep_free(FIELD_PTR(RXS(modem), V17RXS_BUF_SRE));
	sysdep_free(FIELD_PTR(RXS(modem), V17RXS_BUF_MRF));
	sysdep_free(RXS(modem));

	FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(CTL(modem), V17RXC_MTD));
	FPM_TONE_delete((struct fpm_tone *)FIELD_PTR(CTL(modem), V17RXC_TONE));
	sysdep_free(FIELD_PTR(CTL(modem), V17RXC_SCRATCH));
	sysdep_free(FIELD_PTR(CTL(modem), V17RXC_BUF2));
	FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(CTL(modem), V17RXC_MTD2));
	sysdep_free(CTL(modem));

	sysdep_free(modem);
}

/* --------------------------------------------------------------------- */

/*
 * V17TX_delete -- .text 0x098e00, 107 bytes.  See v17fax.h; the object's
 * literal 1 before `FPM_PPS_free` is F8876 again and is not reproduced.
 */
void
V17TX_delete(void *modem)
{
	FPM_PPS_free((struct fpm_pps *)(void *)
			FIELD(FIELD_PTR(modem, V17TX_OBJ_FP), V17FP_PPS));
	sysdep_free(FIELD_PTR(FIELD_PTR(modem, V17TX_OBJ_FP), V17FP_PTR_0010));
	sysdep_free(FIELD_PTR(modem, V17TX_OBJ_FP));

	SGD_delete((struct sgd *)
			FIELD_PTR(FIELD_PTR(modem, V17TX_OBJ_PARAMS),
				  V17TXP_SGD));
	FIFO_delete((struct fax_fifo *)
			FIELD_PTR(FIELD_PTR(modem, V17TX_OBJ_PARAMS),
				  V17TXP_FIFO));
	sysdep_free(FIELD_PTR(modem, V17TX_OBJ_PARAMS));

	sysdep_free(modem);
}

/* --------------------------------------------------------------------- */

int
V17RX_modem(void *modem, short *in, short *out, unsigned short *count)
{
	short total;
	short before;
	unsigned short left;

	*FIELD(modem, V17RX_OBJ_RESULT_B1) &=
		(unsigned char)~V17RX_FLAG_ERROR;

	total = 0;
	do {
		void *ctl;
		short got;

		/*
		 * `before` is signed and `left` is not, and both are what the
		 * object encodes: ONE 16-bit load feeds a `movswl` at the top
		 * of the loop and a `movzwl` after the call, because the
		 * compiler shared the read across the back edge.
		 */
		before = (short)*count;

		ctl = FIELD_PTR(modem, V17RX_OBJ_CTL);
		got = (*(v17rx_process_fn *)(void *)FIELD(ctl, V17RXC_PROCESS))
				(modem, in, out, count);

		left = *count;
		in += before - left;
		out += got;
		total = (short)(total + got);
	} while (left != 0);

	*count = (unsigned short)total;

	return AT_I(modem, V17RX_OBJ_RESULT);
}

/* --------------------------------------------------------------------- */

/*
 * RxHdxDataV17 -- .text 0x0a0000, 226 bytes.
 *
 * The DATA state of the receive machine.  See v17fax.h for the flag-bit
 * enumeration and for `V17RXC_INT_0008`.
 *
 * THE `out` CASTS ARE THE RECONSTRUCTION'S AND THE OBJECT CANNOT SEE THEM.
 * The state handlers share one signature (`v17rx_process_fn`) whose third
 * argument this tree already spells `short *`, while `DemodDataV17` and
 * `DescrambleDataV17` declare theirs `unsigned short *`.  Both pointers are
 * passed through untouched, so nothing in the object distinguishes the two
 * spellings and the casts cost no instruction.  Deviation D1141.
 *
 * THE RESULT IS A `?:` AND THE OBJECT SPELLS IT BRANCHLESSLY.  It emits
 * `cmp $0x2,%ax` / `setne %al` / `movzbl %al,%esi` / `neg %esi` /
 * `and %edi,%esi`, which is `n & -(q != 2)` -- the standard shape GCC folds a
 * two-armed conditional into when one arm is a constant zero and the guard is
 * already a flag.  The `?:` is what is written here, because it is the source
 * that expression is the compilation of and because writing the mask by hand
 * would be fitting the object rather than reading it.  `n` is `unsigned short`
 * and the object zero-extends it (`movzwl %ax,%edi`), which is the local's
 * declared type showing through (finding F7803); the final `movswl %si` is the
 * function's own `short` return.
 *
 * THE `andb $0x7f` SITS BETWEEN THE `setne` AND THE `and` in the object.  That
 * is the scheduler moving a store with no dependence on either, not a
 * statement order to reproduce: the clear of `V17RX_FLAG_LOW_SNR` belongs with
 * the `GetSNRV17` test it precedes.
 */
short
RxHdxDataV17(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short r;

	AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_CARRIER;
	AT_B(modem, V17RX_OBJ_RESULT) = V17RX_STATUS_DATA;

	if (DataCarrierDetectV17(modem, in, *count) == 0
	    || AT_I(CTL(modem), V17RXC_INT_0008) != 0) {
		AT_B(modem, V17RX_OBJ_RESULT_B1) &=
			(unsigned char)~V17RX_FLAG_CARRIER;
		*count = 0;
		return 0;
	}

	n = DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV17(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	r = (short)(QualityDetectV17(modem) != V17_QUALITY_UNRELIABLE ? n : 0);

	AT_B(modem, V17RX_OBJ_RESULT_B1) &=
		(unsigned char)~V17RX_FLAG_LOW_SNR;
	if (GetSNRV17(modem) <= V17RX_SNR_THRESHOLD)
		AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_LOW_SNR;

	return r;
}

/* --------------------------------------------------------------------- */

/*
 * RxHdxErrorV17 -- .text 0x0a00f0, 59 bytes.
 *
 * The ERROR state: raise the flag, demodulate anyway so the filters keep their
 * history, and consume the block.  `DemodDataV17`'s return is DISCARDED, which
 * is the one thing separating this from a data handler that happened to fail.
 */
short
RxHdxErrorV17(void *modem, short *in, short *out, unsigned short *count)
{
	AT_B(modem, V17RX_OBJ_RESULT_B1) |= V17RX_FLAG_ERROR;

	DemodDataV17(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	return 0;
}

/* --------------------------------------------------------------------- */

/*
 * V17RX_status -- .text 0x0a0910, 190 bytes.
 *
 * THE FLAGS BYTE IS FOUR STORES AND THE VALUE IT SETTLES ON IS DETERMINISTIC.
 * Reading the object's chain from the incoming byte `b`, with `x` for the
 * three state bits:
 *
 *     store 1  b & 0xfe
 *     store 2  ((b & 0xfc) | x1) & 0xfb          =  (b & 0xf8) | x1
 *     store 3  (((b & 0xf8) & 0xf3) | x3) | 0x10 = ((b & 0xf0) | x1 | x3 | 0x10)
 *     store 4  ((... & 0xdf) | x5 | 0x40) & 0x7f
 *
 * -- and the last line leaves `(b & 0x50) | x1 | x3 | x5 | 0x10 | 0x40`, in
 * which bits 4 and 6 of `b` are re-set by the two constants anyway.  So the
 * result is `0x50 | x1 | x3 | x5` and NOTHING of the caller's byte survives.
 * The three intermediate stores are the object's and are kept: each is
 * separated from the next by a load of `V17RX_OBJ_STATE`, which is reached
 * through a character type and may alias the status block, so a source that
 * assigned once could not have produced them.  They are observable only to a
 * caller that overlaps its two arguments, which is what deviation D1092
 * records.
 */
int
V17RX_status(void *modem, struct v17_status *status)
{
	unsigned char *rx;

	if (status == 0)
		return 0;

	/*
	 * `modem` stays a byte pointer for the same reason `V17TX_status`'s
	 * `params` does: it is an unmodelled block, and it is what keeps the
	 * stores above from being merged.
	 */
	rx = (unsigned char *)modem;

	status->protocol = (short)AT_US(rx, V17RX_OBJ_PROTOCOL);
	status->tx_bps = 0;
	status->rx_bps = (short)AT_US(rx, V17RX_OBJ_RX_BPS);
	status->short_06 = (short)
		((rx[V17RX_OBJ_RESULT_B1] & V17RX_FLAG_LOW_SNR) == 0);
	status->snr = GetSNRV17(modem);
	status->short_0a = 0;
	status->short_0e = 0;
	status->short_10 = 0;
	status->short_12 = (short)AT_US(rx, V17RX_OBJ_RX_BPS);

	status->flags &= (unsigned char)~V17_STATUS_FLAG_01;
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_02)
		 | ((AT_B(RXS(modem), V17RXS_BYTE_001C) & V17RXS_001C_BIT0)
		    << 1));
	status->flags &= (unsigned char)~V17_STATUS_FLAG_04;
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_08)
		 | ((AT_I(RXS(modem), V17RXS_INT_0000) == 0) << 3));
	status->flags |= V17_STATUS_FLAG_10;
	status->flags1 &= (unsigned char)~V17_STATUS_FLAGS1_CLEAR;
	status->flags = (unsigned char)
		((status->flags & ~V17_STATUS_FLAG_20)
		 | ((AT_I(RXS(modem), V17RXS_INT_0010) == 0) << 5));
	status->flags |= V17_STATUS_FLAG_40;
	status->flags &= (unsigned char)~V17_STATUS_FLAG_80;

	return 1;
}

/* --------------------------------------------------------------------- */

void
ScrambleDataV17(void *modem, unsigned short *data, unsigned short count)
{
	void *fp;

	fp = FIELD_PTR(modem, V17TX_OBJ_FP);
	SDM_scrambler((struct fpm_sdm *)(void *)FIELD(fp, V17FP_SDM), data,
		      count);
}

/* --------------------------------------------------------------------- */

void
SeedScramblerV17(void *modem, unsigned int seed)
{
	void *fp;

	fp = FIELD_PTR(modem, V17TX_OBJ_FP);
	((struct fpm_sdm *)(void *)FIELD(fp, V17FP_SDM))->reg = seed;
}

/* --------------------------------------------------------------------- */

void
SetEncoderV17(void *modem, short which, short arg)
{
	void *fp;

	switch (which) {
	case V17_ENCODER_DIF:
		fp = FIELD_PTR(modem, V17TX_OBJ_FP);
		AT_S(fp, V17FP_ENCODER_SEL) = V17_ENCODER_DIF;
		AT_S(fp, V17FP_SMC_SHORT_06) = arg;
		break;
	case V17_ENCODER_ABS:
		/* No second value on this arm; see v17fax.h. */
		fp = FIELD_PTR(modem, V17TX_OBJ_FP);
		AT_S(fp, V17FP_ENCODER_SEL) = V17_ENCODER_ABS;
		break;
	case V17_ENCODER_TCM:
		fp = FIELD_PTR(modem, V17TX_OBJ_FP);
		AT_S(fp, V17FP_ENCODER_SEL) = V17_ENCODER_TCM;
		AT_S(fp, V17FP_SMC_SHORT_06) = arg;
		break;
	default:
		break;
	}
}

/* --------------------------------------------------------------------- */

/*
 * V17TX_modem -- .text 0x0a0e40, 182 bytes.  See v17fax.h for the two arms,
 * the budget and why `in` does not advance while `out` does.
 */
int
V17TX_modem(void *modem, unsigned short *in, short *out, unsigned short *count)
{
	void *prm;
	unsigned short taken;
	short budget;
	short total;

	prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);

	*FIELD(modem, V17TX_OBJ_RESULT_B1) &=
		(unsigned char)~V17TX_RESULT_B1_BIT1;

	if (AT_I(prm, V17TXP_INT_0008) == 0)
		taken = (unsigned short)FIFO_write(
				(struct fax_fifo *)
					FIELD_PTR(prm, V17TXP_FIFO),
				in, *count);
	else
		taken = *count;

	budget = V17TX_MODEM_BUDGET;
	total = 0;
	do {
		short got;

		prm = FIELD_PTR(modem, V17TX_OBJ_PARAMS);
		got = (*(v17tx_process_fn *)(void *)
				FIELD(prm, V17TXP_PROCESS))
					(modem, in, out, &budget);

		out += got;
		total = (short)(total + got);
	} while (budget > 0);

	if (*count != taken) {
		*FIELD(modem, V17TX_OBJ_RESULT_B1) |= V17TX_RESULT_B1_BIT1;
		/*
		 * A BYTE store into the low byte of the int this function
		 * returns, which is what the object encodes
		 * (`movb $0x9,0x20(%edi)`) and is why it cannot be written
		 * through `AT_I`.
		 */
		*FIELD(modem, V17TX_OBJ_RESULT) = V17TX_RESULT_BYTE_09;
	}

	*count = (unsigned short)total;

	return AT_I(modem, V17TX_OBJ_RESULT);
}

/* --------------------------------------------------------------------- */

int
V17TX_status(void *params, struct v17_status *status)
{
	unsigned char *p;

	if (status == 0)
		return 0;

	/*
	 * `params` stays a byte pointer: it is an unidentified block (see
	 * v17fax.h), and it is also what keeps the dead store below alive,
	 * since a character type may alias anything.
	 */
	p = (unsigned char *)params;

	status->protocol = (short)AT_US(p, 0x00);
	status->tx_bps = (short)AT_US(p, 0x02);
	status->rx_bps = 0;
	status->short_06 = 0;
	status->snr = 0;
	status->short_0a = 0;
	status->short_0c = 0;
	status->short_10 = (short)AT_US(p, 0x02);
	status->short_12 = 0;

	/*
	 * The first of these two writes to `flags` is dead and is the
	 * object's; see v17fax.h and D1032.  It stays because the load of
	 * `p[0x10]` sits between them and may alias.
	 */
	status->flags &= (unsigned char)~V17_STATUS_FLAGS_CLEAR;
	status->flags1 &= (unsigned char)~V17_STATUS_FLAGS1_CLEAR;
	status->flags = (unsigned char)(p[0x10] & V17_STATUS_FLAG_04);

	status->int_18 = AT_I(p, 0x18);

	return 1;
}

/* --------------------------------------------------------------------- */

/*
 * DemodDataV17 -- .text 0x0a50a0, 415 bytes.  See v17fax.h for the shape, for
 * why the pre-pass copy does NOT halve where `DemodDataV29`'s does, and for
 * where `signal` comes from.
 *
 * THE STORE ORDER OF THE THREE EQUALISER ENABLES IS THE OBJECT'S.  `tilt_on`
 * is written first (0x0a51dd), then `pll_on` (0x0a51f0), then `lms_on`
 * (0x0a51fa) -- the same three fields in the same order as `DemodDataV29`.
 */
unsigned short
DemodDataV17(void *modem, short *in, unsigned short *bits, unsigned short count)
{
	int signal;
	unsigned short n;
	unsigned char *rxs;

	FPM_AGC_agc(RXS_AGC(RXS(modem)), in, count);
	/* Not the object's `%eax`; the same value.  D1091. */
	signal = RXS_AGC(RXS(modem))->signal;

	if (AT_S(CTL(modem), V17RXC_SHORT_0018) == 0) {
		short *buf = (short *)FIELD_PTR(CTL(modem), V17RXC_SCRATCH);
		unsigned short i;

		/* No `>> 1` here.  F9103. */
		for (i = 0; i < count; i++)
			buf[i] = in[i];

		FPM_TONE_kill((struct fpm_tone *)
				FIELD_PTR(CTL(modem), V17RXC_TONE),
			      (short *)FIELD_PTR(CTL(modem), V17RXC_SCRATCH),
			      (short)count);

		if (FPM_MTD_detect((struct fpm_mtd *)
					FIELD_PTR(CTL(modem), V17RXC_MTD),
				   (const short *)
					FIELD_PTR(CTL(modem), V17RXC_SCRATCH),
				   (short)count) != 0)
			return 0;
	}

	n = (unsigned short)FPM_MRF_filter(
			RXS_MRF(RXS(modem)),
			in,
			(short *)FIELD_PTR(RXS(modem), V17RXS_BUF_MRF),
			(short)count);

	rxs = RXS(modem);
	RXS_SRE(rxs)->adapt = signal & AT_I(rxs, V17RXS_INT_0004);

	n = FPM_SRE_recover(RXS_SRE(RXS(modem)),
			    (const short *)
				FIELD_PTR(RXS(modem), V17RXS_BUF_MRF),
			    (short *)FIELD_PTR(RXS(modem), V17RXS_BUF_SRE),
			    (short)n);

	if (n > V17RXS_SRE_MAX && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("ERROR: SRE buffer violation!(%d)", n);

	rxs = RXS(modem);
	RXS_FSE(rxs)->tilt_on = 0;
	RXS_FSE(rxs)->pll_on = signal & AT_I(rxs, V17RXS_INT_0008);
	RXS_FSE(rxs)->lms_on = signal & AT_I(rxs, V17RXS_INT_0010);

	return FPM_FSE_receive(RXS_FSE(RXS(modem)),
			       (const short *)
				FIELD_PTR(RXS(modem), V17RXS_BUF_SRE),
			       bits, n);
}

/* --------------------------------------------------------------------- */

void
DescrambleDataV17(void *modem, unsigned short *data, unsigned short count)
{
	SDM_descrambler((struct fpm_sdm *)(void *)
				FIELD(RXS(modem), V17RXS_SDM),
			data, count);
}

/* --------------------------------------------------------------------- */

int
CarrierDetectV17(void *modem)
{
	unsigned char *rx;
	unsigned char *ctl;
	int r;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	ctl = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_CTL);

	/* +0xd0 is read 32-bit HERE and 16-bit in QualityDetectV17. */
	r = AT_I(rx, V17RXS_AGC_SIGNAL) & AT_I(rx, V17RXS_INT_0120);

	if (AT_I(ctl, V17RXC_INT_0010) != 0
	    && AT_I(rx, V17RXS_EPOCH) != 0
	    && AT_S(rx, V17RXS_SHORT_0094) > V17RXS_0094_MIN) {
		if (AT_S(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX)
			r = 0;
		else
			r &= 1;
		/*
		 * The object tests the same field twice, with the two arms
		 * merged in between; it is two `if`s in the source and not
		 * one, or the compare would have been shared.
		 */
		if (AT_S(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V17 Decoder error too big..."
					" no carrier\n");
		}
	}

	return r;
}

/* --------------------------------------------------------------------- */

short
DataCarrierDetectV17(void *modem, const short *in, unsigned short count)
{
	unsigned char *rx;
	unsigned char *ctl;
	short r;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	ctl = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_CTL);

	/* +0xd0 is read 16-bit HERE and 32-bit in CarrierDetectV17. */
	r = (short)(AT_S(rx, V17RXS_AGC_SIGNAL) & AT_I(rx, V17RXS_INT_0120));

	if (AT_S(ctl, V17RXC_SHORT_0020) == 0) {
		/*
		 * The same three gates and the same two arms as
		 * `CarrierDetectV17`, including its doubled test of the
		 * decoder error and its format string.
		 */
		if (AT_I(ctl, V17RXC_INT_0010) != 0
		    && AT_I(rx, V17RXS_EPOCH) != 0
		    && AT_S(rx, V17RXS_SHORT_0094) > V17RXS_0094_MIN) {
			if (AT_S(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX)
				r = 0;
			else
				r &= 1;
			if (AT_S(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V17 Decoder error too big..."
						" no carrier\n");
			}
		}
	} else {
		if (AT_S(rx, V17RXS_DEC_ERROR) > V17RXS_DEC_ERROR_MAX
		    || (r & 1) == 0)
			AT_S(ctl, V17RXC_SHORT_002E) = 1;

		r = 1;
		if (AT_S(ctl, V17RXC_SHORT_002E) != 0) {
			short *buf;
			short i;

			buf = (short *)FIELD_PTR(ctl, V17RXC_BUF2);
			for (i = 0; i < (int)count; i++)
				buf[i] = (short)(unsigned short)in[i];

			/*
			 * The object passes FOUR arguments here, the fourth a
			 * constant 1 the callee never reads; see D1031.
			 */
			FPM_AGC_agc((struct fpm_agc *)(void *)
					FIELD(ctl, V17RXC_AGC),
				    (short *)FIELD_PTR(ctl, V17RXC_BUF2),
				    count);

			if (FPM_MTD_detect((struct fpm_mtd *)
						FIELD_PTR(ctl, V17RXC_MTD2),
					   (const short *)
						FIELD_PTR(ctl, V17RXC_BUF2),
					   (short)count) != 0)
				AT_S(ctl, V17RXC_OFFBAND) = 0;
			else
				AT_S(ctl, V17RXC_OFFBAND) = (short)
					(AT_US(ctl, V17RXC_OFFBAND) + count);

			if (AT_S(ctl, V17RXC_OFFBAND) > V17RXC_OFFBAND_MAX) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V17: V21 Carrier detected\n");
				r = 0;
			}
		}
	}

	if (AT_S(rx, V17RXS_SHORT_4FB4) != 0) {
		short rms;
		unsigned short phase;

		rms = FPM_rms(in, count);

		/*
		 * No rounding term on this one, unlike QualityDetectV17's
		 * smoothing: the object is `imul $0x32fe ; sar $0xf` and
		 * nothing else.
		 */
		if ((int)rms < ((int)AT_S(rx, V17RXS_RMS_REF)
				* V17RXS_RMS_DROP_Q15) >> 15) {
			r = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"sudden energy drop > 8[dB],"
					" no carrier");
		}

		phase = (unsigned short)(AT_US(rx, V17RXS_RMS_PHASE) + 1);
		if ((short)phase == V17RXS_RMS_PERIOD) {
			AT_S(rx, V17RXS_RMS_REF) = rms;
			AT_S(rx, V17RXS_RMS_PHASE) = 0;
		} else {
			AT_S(rx, V17RXS_RMS_PHASE) = (short)phase;
		}
	}

	return r;
}

/* --------------------------------------------------------------------- */

short
QualityDetectV17(void *modem)
{
	unsigned char *rx;
	short r;
	short err;
	short n;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);

	/* +0xd0 is read 16-bit HERE and 32-bit in CarrierDetectV17. */
	r = (short)(AT_S(rx, V17RXS_AGC_SIGNAL) & AT_I(rx, V17RXS_INT_0120));

	/*
	 * Read BEFORE the diagnostic, because the object reads it before the
	 * call and no compiler may hoist a load across one.
	 */
	err = AT_S(rx, V17RXS_DEC_ERROR);

	if (r == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"V17 Dec error too big..."
				" unreliable data\n");
		r = V17_QUALITY_UNRELIABLE;
	}

	n = (short)AT_US(rx, V17RXS_QCOUNT);
	if (n == 0) {
		AT_S(rx, V17RXS_QAVG) = err;
		AT_S(rx, V17RXS_QCOUNT) = 1;
		return r;
	}

	if (n > V17RXS_QCOUNT_SETTLE) {
		if (n != V17RXS_QCOUNT_JUDGE)
			return r;
		if (AT_S(rx, V17RXS_QAVG)
		    <= (short)AT_US(rx, V17RXS_SHORT_4FB0))
			AT_S(rx, V17RXS_SHORT_4FB2) = 1;
	} else {
		AT_S(rx, V17RXS_QAVG) = (short)
			(((err * V17RXS_QWEIGHT_NEW + V17RXS_QROUND) >> 15)
			 + ((AT_S(rx, V17RXS_QAVG) * V17RXS_QWEIGHT_OLD
			     + V17RXS_QROUND) >> 15));
	}

	AT_S(rx, V17RXS_QCOUNT) = (short)(n + 1);
	return r;
}

/* --------------------------------------------------------------------- */

int
EpochDetectV17(void *modem)
{
	unsigned char *rx;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	return AT_I(rx, V17RXS_EPOCH) != 0;
}

/* --------------------------------------------------------------------- */

short
GetSNRV17(void *modem)
{
	unsigned char *rx;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	return (short)(13 - AT_US(rx, V17RXS_DEC_ERROR));
}

/* --------------------------------------------------------------------- */

void
StoreCoefV17(void *modem)
{
	unsigned char *rx;
	short *d0;
	short *d1;
	const unsigned short *s0;
	const unsigned short *s1;
	unsigned short i;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	d0 = (short *)FIELD_PTR(modem, V17RX_OBJ_COEFSAVE0);
	d1 = (short *)FIELD_PTR(modem, V17RX_OBJ_COEFSAVE1);
	s0 = (const unsigned short *)FIELD_PTR(rx, V17RXS_COEF0);
	s1 = (const unsigned short *)FIELD_PTR(rx, V17RXS_COEF1);

	for (i = 0; i < V17_COEF_N; i++) {
		d0[i] = (short)s0[i];
		d1[i] = (short)s1[i];
	}

	*(short *)FIELD_PTR(modem, V17RX_OBJ_RATESAVE) =
		(short)AT_I(rx, V17RXS_RATE);
}

/* --------------------------------------------------------------------- */

void
Restore_rateV17(void *modem)
{
	unsigned char *rx;
	const short *saved;

	rx = (unsigned char *)FIELD_PTR(modem, V17RX_OBJ_STATE);
	saved = (const short *)FIELD_PTR(modem, V17RX_OBJ_RATESAVE);

	AT_I(rx, V17RXS_RATE) = *saved;
	AT_S(rx, V17RXS_SHORT_01F8) =
		(short)(AT_US(rx, V17RXS_USHORT_018C) + 5);
}
