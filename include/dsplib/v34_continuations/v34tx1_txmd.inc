
	struct v34_object *o = (struct v34_object *)objp;
	short q = tx1_scramble2(o);

	o->cur_quadrant = q;
	tx1_put_point(o, vect4[q]);
	txmit(o);

	o->vect_idx = (short)(o->vect_idx + 1);
	if ((unsigned short)o->vect_idx
	    == (unsigned short)o->short_aa78) {
		/* 0x66fe9 */
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
				"On MD - enabling echo adaptation...\r\n");

		o->echo_calls = 0;				/* 0x6700e */
		o->tx_flags =				/* 0x67017 */
			(short)((unsigned short)o->tx_flags & ~V34_EC_FROZEN);
		o->echo_alpha = 0;				/* 0x6701e */
		o->far_echo_alpha = 0;				/* 0x67025 */
	}

	if ((unsigned short)o->vect_idx
	    == (unsigned short)o->seg_len) {
		struct v34_ratecfg *cfg = &o->ratecfg;
		int pcm = (o->v90_receiver != 0 || o->k56flex_receiver != 0);

		if (dsplibs_debug_level > 1)		/* 0x6800d */
			dsplibs_debug_printf(
				"TX: Done with MD, moving to S/Sbar" " again...\r\n");

		V34SetupModulator(v34_object_modulator(o),
				  cfg->baud, cfg->carrier, cfg->preemp, pcm, 0);

		hs_set_txstate(o, V34HS_SSEG);
		o->seg_symcount = 0;
		o->tx_scr_sr = 0;
		o->tx_flags = (short)((unsigned short)o->tx_flags
				     | (V34_TXFLAG_PPSEG | V34_EC_FROZEN));
	}

	/* 0x6430c */
	o->seg_symcount = (short)(o->seg_symcount + 1);
	V34_TX_CASE_RETURN(V34TX1_LOOP);
