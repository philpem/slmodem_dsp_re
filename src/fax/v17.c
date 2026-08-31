/*
 * v17.c -- ITU-T V.17 (fax): the receiver's primitives, and the transmit-side
 *          setters that sit beside them.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V17RX_modem       .text 0x09ff80  127
 *   SeedScramblerV17  .text 0x0a09f0   15
 *   SetEncoderV17     .text 0x0a0a00   90
 *   V17TX_status      .text 0x0a1bd0  106
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
 */

#include "dsplib/v17fax.h"

#include "dsplib/debug.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"

/* The instances are not modelled; see v17fax.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))

#define AT_S(p, off)		(*(short *)(void *)FIELD((p), (off)))
#define AT_US(p, off)		(*(unsigned short *)(void *)FIELD((p), (off)))
#define AT_I(p, off)		(*(int *)(void *)FIELD((p), (off)))

/* --------------------------------------------------------------------- */

int
V17RX_modem(void *modem, short *in, short *out, unsigned short *count)
{
	short total;
	short before;
	unsigned short left;

	*FIELD(modem, V17RX_OBJ_RESULT_B1) &=
		(unsigned char)~V17RX_RESULT_B1_BIT1;

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
	status->short_08 = 0;
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
