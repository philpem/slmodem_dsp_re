/* V32stc.c -- original V.32 status/control translation unit. */
#include "v32fpdisp-common.h"
static short SnrToRetrainTable[6] = {
	9,			/* 0   4800                                  */
	13,			/* 1   9600, no trellis                      */
	13,			/* 2   9600, trellis                         */
	11,			/* 3   7200                                  */
	20,			/* 4  12000                                  */
	24			/* 5  14400                                  */
};

static short RATEv32[6] = {
	4800,			/* 0                                         */
	9600,			/* 1   no trellis                            */
	9600,			/* 2   trellis                               */
	7200,			/* 3                                         */
	12000,			/* 4                                         */
	14400			/* 5                                         */
};

static short PROTOCOL[9] = {
	0, 1, 2, 9, 6, 6, 3, 7, 8
};

/* ------------------------------------------------------------------------ */

/*
 * V32FP_control -- .text 0x084530.  Always returns 1.
 */
int
V32FP_control(void *modem, struct v32fp_ctl *ctl)
{
	void *fp;
	void *hdx;
	int rate;

	if (FIELD_U8(modem, V32_OBJ_STATUS) == V32_MSG_RETRAIN_REQ) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Patch: set ctl_ptr->vxx_ctl."
					     "options.retrain = TRUE\n");
		ctl->ctl1 |= V32_CTL1_RETRAIN;
	}

	fp = FP(modem);
	FIELD_INT(fp, V32FP_R1C) = ctl->ctl0 & 1;
	FIELD_INT(fp, V32FP_R20) = (ctl->ctl0 >> 1) & 1;
	FIELD_INT(fp, V32FP_R24) = (ctl->ctl0 >> 2) & 1;
	FIELD_INT(fp, V32FP_R00) = ((ctl->ctl0 & 0x08) == 0);
	FIELD_INT(fp, V32FP_R0C) = ((ctl->ctl0 & 0x10) == 0);
	FIELD_INT(fp, V32FP_EQ_ADAPT) = ((ctl->ctl0 & 0x20) == 0);
	FIELD_INT(fp, V32FP_R14) = ctl->r18;
	FIELD_INT(fp, V32FP_R18) = ctl->r1c;
	FIELD_U8(modem, V32_OBJ_OPTIONS_HI) =
		(unsigned char)((FIELD_U8(modem, V32_OBJ_OPTIONS_HI)
				 & (unsigned char)~V32_OPTIONS_HI_CTL)
				| (unsigned char)((ctl->ctl0 >> 7) << 1));

	if (ctl->r14 != 0) {
		int ratio;

		if (FIELD_U8(modem, V32_OBJ_STATUS) != V32_MSG_RESIZE_DONE) {
			FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_FAULT;
			FIELD_U8(modem, V32_OBJ_STATUS) = V32_MSG_RESIZED;
		}
		/*
		 * BOTH TABLE READS ARE `movswl` HERE and `movzwl` twenty
		 * instructions later, on the same table and the same
		 * selector.  The difference is forced: this quotient is used
		 * as a 32-bit multiplier, so the extension survives, while
		 * the three below are stored 16-bit and it does not.  Finding
		 * F614's two columns in one function.
		 */
		ratio = V32_SAMPLE_LEN[PARAMS(modem)->symlen_sel]
			/ V32_SAMPLE_LEN[PARAMS(modem)->r16];
		hdx = HDX(modem);
		FIELD_U16(hdx, V32_HDX_SHORT_9C) =
			(unsigned short)(ratio
					 * FIELD_U16(hdx, V32_HDX_SHORT_9C));
		FIELD_U16(hdx, V32HDX_BLOCK_CHARGE) =
			(unsigned short)V32_SYMBOL_LEN[PARAMS(modem)->symlen_sel];
		FIELD_U16(hdx, V32HDX_SYMBOL_LEN) =
			(unsigned short)V32_SYMBOL_LEN[PARAMS(modem)->symlen_sel];
		FIELD_U16(hdx, V32HDX_SAMPLE_LEN) =
			(unsigned short)V32_SAMPLE_LEN[PARAMS(modem)->symlen_sel];
		FIELD_U8(modem, V32_OBJ_STATUS) = V32_MSG_OK;
	}

	if ((ctl->ctl1 & V32_CTL1_RETRAIN) != 0) {
		if (FIELD_U16(fp, V32FP_RATE_FALLBACK) != 0) {
			switch (FIELD_S16(fp, V32FP_SHORT_2E)) {
			case V32_RATE_14400:
				PARAMS(modem)->rx_rate = 12000;
				PARAMS(modem)->tx_rate = 12000;
				break;
			case V32_RATE_12000:
				PARAMS(modem)->rx_rate = 9600;
				PARAMS(modem)->tx_rate = 9600;
				break;
			case V32_RATE_9600:
			case V32_RATE_9600_NT:
				PARAMS(modem)->rx_rate = 7200;
				PARAMS(modem)->tx_rate = 7200;
				break;
			case V32_RATE_7200:
				PARAMS(modem)->rx_rate = 4800;
				PARAMS(modem)->tx_rate = 4800;
				break;
			default:
				break;
			}
		}
		/*
		 * TWO SEQUENTIAL `if`s AND NOT AN `else if`, and the object
		 * proves it: the protocol is RE-LOADED at 8477e after the
		 * originate arm has run, which an `else` could not need.
		 * `V32FP_recreate` copies its parameter block over the
		 * object's first 48 bytes, and the object here IS that
		 * parameter block, so the compiler cannot know the value
		 * survived the call.
		 */
		if (PARAMS(modem)->protocol == 0) {
			V32FP_recreate(modem, PARAMS(modem), 0);
			hdx = HDX(modem);
			FIELD_U16(hdx, V32HDX_STATE) = 1;
			FIELD_INT(hdx, V32HDX_LONG_7C) = 0x960;
			V32OrgNextState(modem);
		}
		if (PARAMS(modem)->protocol == 1) {
			V32FP_recreate(modem, PARAMS(modem), 0);
			hdx = HDX(modem);
			FIELD_U16(hdx, V32HDX_STATE) = 1;
			V32AnsNextState(modem);
		}
		FIELD_U8(modem, V32_OBJ_FLAGS) |= V32_FLAG_RETRAIN;
		FIELD_U8(modem, V32_OBJ_STATUS) = V32_MSG_RETRAINING;
		ctl->ctl1 &= (unsigned char)~V32_CTL1_RETRAIN;
		return 1;
	}

	if ((ctl->ctl1 & V32_CTL1_RENEG) != 0) {
		hdx = HDX(modem);
		FIELD_U16(hdx, V32HDX_STATE) = 0;
		FIELD_U8(modem, V32_OBJ_FLAGS) &= (unsigned char)~V32_FLAG_DATA;
		if (FIELD_S16(hdx, V32HDX_MODE) == V32_MODE_RING_RESP) {
			FIELD_U16(hdx, V32HDX_SHORT_44) = 0;
			V32RngRespNextState(modem);
			ctl->ctl1 &= (unsigned char)~V32_CTL1_RENEG;
			return 1;
		}
		FIELD_U16(hdx, V32HDX_MODE) = V32_MODE_RING_INIT;

		PARAMS(modem)->trellis = ctl->trellis;
		rate = ctl->bps;
		PARAMS(modem)->tx_rate = (short)rate;
		PARAMS(modem)->rx_rate = (short)rate;
		switch (rate) {
		case 14400:
			FIELD_U16(fp, V32FP_TX_RATE_IDX) = V32_RATE_14400;
			break;
		case 12000:
			FIELD_U16(fp, V32FP_TX_RATE_IDX) = V32_RATE_12000;
			break;
		case 9600:
			/*
			 * `2 - (trellis == 0)`, which is the same six
			 * instructions `V32FP_recreate` uses at 7f40b and
			 * 7f60f -- v32seq.h records both.
			 */
			FIELD_U16(fp, V32FP_TX_RATE_IDX) = (unsigned short)
				(2 - (PARAMS(modem)->trellis == 0));
			break;
		case 7200:
			FIELD_U16(fp, V32FP_TX_RATE_IDX) = V32_RATE_7200;
			break;
		default:
			FIELD_U16(fp, V32FP_TX_RATE_IDX) = V32_RATE_4800;
			break;
		}
		FIELD_U16(fp, V32FP_RX_RATE_IDX) =
			FIELD_U16(fp, V32FP_TX_RATE_IDX);
		V32RngInitNextState(modem);
		ctl->ctl1 &= (unsigned char)~V32_CTL1_RENEG;
		return 1;
	}

	return 1;
}

/* ------------------------------------------------------------------------ */

/*
 * V32FP_status -- .text 0x084840.  Always returns 1.
 *
 * Two jobs in one function: fill the caller's block, and -- if the line has
 * got worse -- build a control request out of `V32_CTL` and issue it on the
 * spot.  It is the only writer of `V32_OBJ_SNR_DROPS`.
 *
 * ---------------------------------------------------------------------------
 * THE SNR IS A LOG OF A SMOOTHED DECISION ERROR, AND EVERY CONSTANT IS Q15
 *
 *     err  = (err * 0x7333 + 0x4000) >> 15
 *          + ((unsigned short)(fp->0x256 >> 2) * 0xccd + 0x4000) >> 15
 *
 * 0x7333 and 0xccd are 0.9 and 0.1 in Q15 and 0x4000 is the round-to-nearest
 * term, so this is a first-order smoother with a time constant of ten blocks.
 * The result is written back to fp + 0x50d6, which the object's own debug line
 * calls `Dec error`.
 *
 * What comes out is `log10` of a reference over that error:
 *
 *     k    = (rate is 12000 or 14400 ? 0x4000 : 0) + 0x1400 - err
 *     m    = (k * FPM_div_reciprocal(err)) >> 16
 *     snr  = ((short)((FPM_log10(m, ~shift) >> 3) * 10) + 256) >> 9
 *
 * -- and the `+ 256 >> 9` is a rounded divide by 512.  The SECOND log, which
 * fills `struct v32_status::r0a`, is the same expression over fp + 0x100 and
 * fp + 0x102 with three differences the object is explicit about: `>> 15`
 * rather than `>> 16`, `-shift` rather than `~shift`, and no `+ 256`.  None of
 * the three is a free choice and all three are reproduced.
 *
 * ---------------------------------------------------------------------------
 * TWO GUARDS THAT ARE NOT THERE
 *
 * `RATEv32` is indexed with fp + 0x2c and fp + 0x2e RAW, with no clamp -- the
 * clamp to 5 above applies only to `SnrToRetrainTable`.  That is D404's shape
 * at two more sites: a six-entry table subscripted from a sixteen-bit field.
 * Recorded rather than fixed.
 */
int
V32FP_status(void *modem, struct v32_status *st)
{
	struct v32fp_ctl ctl;
	void *fp;
	void *hdx;
	unsigned short recip;
	unsigned short shift;
	int rate_idx;
	int snr;
	int r0a;
	int err;
	int k;
	int m;
	int t;

	fp = FP(modem);
	rate_idx = FIELD_S16(fp, V32FP_SHORT_2E);
	if ((unsigned int)rate_idx > 5)
		rate_idx = 5;

	err = ((FIELD_S16(fp, V32FP_DEC_ERROR) * 0x7333 + 0x4000) >> 15)
		+ (((int)(unsigned short)(FIELD_S16(fp, V32FP_SHORT_256) >> 2)
		    * 0xccd + 0x4000) >> 15);
	snr = V32_SNR_SETTLED_VALUE;
	r0a = 0;
	FIELD_S16(fp, V32FP_DEC_ERROR) = (short)err;

	if ((short)err != 0) {
		k = (short)((((unsigned short)
			      (FIELD_U16(fp, V32FP_SHORT_2E) - 4) < 2)
			     ? 0x4000 : 0) + 0x1400 - err);
		FPM_div(FIELD_U16(fp, V32FP_DEC_ERROR), &recip, &shift);
		m = (k * (int)recip) >> 16;
		snr = V32_SNR_FLOOR;
		if ((short)m > 0) {
			t = (int)FPM_log10((unsigned short)m,
					   (short)~(short)shift) >> 3;
			snr = ((int)(short)(t * 10) + 0x100) >> 9;
		}
		fp = FP(modem);
		if (FIELD_S16(fp, V32FP_SHORT_102) != 0
		    && FIELD_S16(fp, V32FP_SHORT_100) != 0) {
			FPM_div(FIELD_U16(fp, V32FP_SHORT_102), &recip, &shift);
			fp = FP(modem);
			m = (FIELD_S16(fp, V32FP_SHORT_100) * (int)recip) >> 15;
			t = (int)FPM_log10((unsigned short)m,
					   (short)-(short)shift) >> 3;
			r0a = (short)(t * 10) >> 9;
			fp = FP(modem);
		}
	}

	if ((short)FIELD_U16(modem, V32_OBJ_SHORT_2C)
	    <= V32_SNR_SETTLE_BLOCKS) {
		FIELD_U16(modem, V32_OBJ_SHORT_2C) = (unsigned short)
			(FIELD_U16(modem, V32_OBJ_SHORT_2C) + 1);
		snr = V32_SNR_SETTLED_VALUE;
	}

	hdx = HDX(modem);
	st->protocol = PROTOCOL[FIELD_S16(hdx, V32HDX_MODE)];
	st->tx_rate = RATEv32[FIELD_S16(fp, V32FP_SHORT_2C)];
	st->rx_rate = RATEv32[FIELD_S16(fp, V32FP_SHORT_2E)];
	st->r0a = (short)r0a;
	st->snr = (short)snr;
	st->r06 = (short)(1 - (FIELD_S16(fp, V32FP_SHORT_256) >> 1));
	st->r0e = 0;
	st->r0c = (short)FIELD_U16(fp, V32FP_SHORT_1F8);
	st->r10 = (short)FIELD_U16(fp, V32FP_SHORT_6C);
	st->r12 = (short)FIELD_U16(fp, V32FP_SHORT_182);

	/*
	 * Eight read-modify-writes of the caller's byte, in the object's own
	 * order.  BIT 6 IS FORCED TO ZERO and bit 7 comes from the object's
	 * fault flag; the other six are the datapump switches, three of them
	 * INVERTED, which is exactly the set `V32FP_control` writes from
	 * `struct v32fp_ctl::ctl0`.
	 */
	st->flags = (unsigned char)((st->flags & 0xfe)
				    | (FIELD_U8(fp, V32FP_R1C) & 1));
	fp = FP(modem);
	st->flags = (unsigned char)((st->flags & 0xfd)
				    | ((FIELD_U8(fp, V32FP_R20) & 1) << 1));
	fp = FP(modem);
	st->flags = (unsigned char)((st->flags & 0xfb)
				    | ((FIELD_U8(fp, V32FP_R24) & 1) << 2));
	fp = FP(modem);
	st->flags = (unsigned char)((st->flags & 0xf7)
				    | ((FIELD_INT(fp, V32FP_R00) == 0) << 3));
	fp = FP(modem);
	st->flags = (unsigned char)((st->flags & 0xef)
				    | ((FIELD_INT(fp, V32FP_R0C) == 0) << 4));
	fp = FP(modem);
	st->flags = (unsigned char)((st->flags & 0xdf)
				    | ((FIELD_INT(fp, V32FP_EQ_ADAPT) == 0) << 5));
	st->flags = (unsigned char)(st->flags & 0xbf);
	st->flags = (unsigned char)((st->flags & 0x7f)
				    | (((FIELD_U8(modem, V32_OBJ_OPTIONS_HI)
					 >> 1) & 1) << 7));
	st->r20 = 0;
	st->r24 = 0;
	st->flags1 = (unsigned char)((st->flags1 & 0xfe)
				     | ((FIELD_U8(modem, V32_OBJ_OPTIONS_HI)
					 >> 2) & 1));

	fp = FP(modem);
	st->r18 = FIELD_INT(fp, V32FP_INT_188);
	st->r1c = (FIELD_INT(fp, V32FP_INT_1F4) != 1);

	if (SnrToRetrainTable[rate_idx] <= (short)snr) {
		FIELD_U16(modem, V32_OBJ_SNR_DROPS) = 0;
		return 1;
	}

	ctl = V32_CTL;
	if (DSPLIB_DEBUG_ON()) {
		dsplibs_debug_printf("V32STC - SNR drop observed, "
				     "SNR = %d < threshold = %d\n",
				     snr, SnrToRetrainTable[rate_idx]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Dec error = %d (*64)\n",
					     FIELD_S16(FP(modem),
						       V32FP_DEC_ERROR));
	}

	if ((short)FIELD_U16(modem, V32_OBJ_SNR_DROPS) > V32_SNR_DROPS_LIMIT) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("%d coseq SNR drops detected "
					     "local retrain is initiated\n",
					     V32_SNR_DROPS_LIMIT);
		ctl.ctl1 |= V32_CTL1_RETRAIN;
		FIELD_U16(modem, V32_OBJ_SNR_DROPS) = 0;
		FIELD_U16(modem, V32_OBJ_SHORT_2C) = 0;
		FIELD_U16(FP(modem), V32FP_RATE_FALLBACK) = 1;
	} else {
		FIELD_U16(modem, V32_OBJ_SNR_DROPS) = (unsigned short)
			(FIELD_U16(modem, V32_OBJ_SNR_DROPS) + 1);
	}

	ctl.ctl0 = (unsigned char)
		(((((ctl.ctl0 & 0xfc) | (st->flags & 1)
		    | (st->flags & 2)) & 0xfb) | (st->flags & 4)) | 0x80);
	V32FP_control(modem, &ctl);
	return 1;
}
