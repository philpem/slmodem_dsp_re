
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_ratecfg *cfg = &o->ratecfg;
	short q;
	int n, span, baud;

	/* 0x64339: the scrambler, the quadrant and one sample. */
	q = tx1_scramble2(o);
	o->cur_quadrant = q;
	tx1_put_point(o, vect4[q]);
	txmit(o);

	/* 0x643dc */
	o->seg_symcount = (short)((unsigned short)o->seg_symcount + 1);

	if (o->short_aa78 == o->seg_symcount) {
		/* 0x67613, and this one FALLS BACK INTO THE CHAIN */
		o->echo_calls = 0;
		o->tx_flags = (short)((unsigned short)o->tx_flags & ~V34_EC_FROZEN);
		o->echo_alpha = 0;
		o->far_echo_alpha = 0;
		o->echo_energy = 0;
		VPcmV34ReportStartOfEchoAdapt(o);
	}

	/* 0x643f8 */
	if (((unsigned short)o->tx_flags & 0x100u) == 0
	    && o->seg_symcount >= o->ratecfg.period)
		o->tx_flags = (short)((unsigned short)o->tx_flags | 0x100u);

	/* 0x64ae4 */
	n = o->seg_symcount;
	span = o->phase3_hd_length;

	if (n == (span >> 1)) {
		VPcmV34ReportMiddleOfEchoAdapt(o);	/* 0x67734 */
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x640a1 */
	}

	baud = cfg->baud;
	if (n == baud + (baud >> 1)) {
		/* 0x67278 */
		o->tx_flags = (short)(((unsigned short)o->tx_flags & ~V34_TXFLAG_PPSEG)
				   | 0x400u);
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x63da2 */
	}

	if (n != span)
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x629c8 */

	/* 0x64b24: the segment is over. */
	hs_set_txstate(o, V34HS_JTXMIT);

	if (o->rtd <= 2) {
		/* 0x6801e */
		v34FreezeEcho(o);
		o->short_aa78 = 0;
	} else {
		if (o->v90_receiver != 0 || o->k56flex_receiver != 0) {
			/* 0x68ad0 and 0x69de7, two copies of one store */
			o->short_aa78 = o->rtd;
		} else {
			/* 0x64b96 */
			o->short_aa78 = (short)(o->rtd >> 1);
		}

		/*
		 * 0x64ba9, and the nesting is what the object says: BOTH
		 * stores jump here (0x68ade and 0x69df5) and the `rtd <= 2`
		 * arm does not -- it reports the freeze instead.  An
		 * else-if chain cannot express a guard shared by two of its
		 * three arms.
		 */
		if (dsplibs_debug_level > 1)		/* 0x6917e */
			dsplibs_debug_printf(
				"V34Hshak: On J TX start, would freeze EC" " after bulk delay (%d samples,"
				" bulk=%d)\r\n",
				o->short_aa78, o->rtd);
	}

	/* 0x64bb2 */
	tx1_put(o, TX1_F25D6, (short)0x8990);
	o->prev_quadrant = o->cur_quadrant;
	hs_set_rxstate(o, V34HS_RECEIVE);
	setupreceiver(o);				/* 0x64c04 */

	/* 0x64d37: the +0xaa0c record, in the object's order */
	{
		unsigned char *r = (unsigned char *)objp + TX1_INFOREC;

		*(int *)(r + 0x24) = 0;
		*(int *)(r + 0x2c) = 0;
		*(short *)(r + 0x14) = -1;
		*(short *)(r + 0x1a) = 0;
		*(short *)(r + 0x1e) = 0;
		*(short *)(r + 0x22) = 0;
		*(short *)(r + 0x18) = 0;
		*(short *)(r + 0x1c) = 0;
		*(short *)(r + 0x16) = 0;
		*(short *)(r + 0x28) = 0;
		*(short *)(r + 0x2a) = 0;
		*(short *)(r + 0x20) = 0;
	}

	/* 0x64d8d */
	hs_set_microstate(o, V34HS_DET_SYNC);

	/* 0x64dba */
	o->vect_idx = 0;
	o->rate_change_pending = 1;

	if ((unsigned)o->v90_receiver > 1u) {
		/* 0x64de7 */
		hs_set_txstate(o, V34HS_JaTXMIT);
		hs_set_rxstate(o, V34HS_WAIT);
		indicateJaTransmission(o);
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x629c8 */
	}

	if ((unsigned)o->k56flex_receiver > 1u) {
		/* 0x67f91 */
		hs_set_txstate(o, V34HS_K56JaTXMIT);
		hs_set_rxstate(o, V34HS_WAIT);
		indicateJaTransmission(o);
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x63948 */
	}

	V34_TX_CASE_RETURN(V34TX1_LOOP);				/* 0x63da2 */
