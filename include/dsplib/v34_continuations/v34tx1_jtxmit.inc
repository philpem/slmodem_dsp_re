
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx = &o->receiver;
	unsigned shift;
	unsigned short idx;
	short bits, mode, q;

	/* 0x635cc, and the completion at 0x6533c falls back into 0x635f0 */
	(void)tx1_ja_common(o, "V34Hshak: on JTXMIT - time to freeze"
				   " echo...\r\n");		/* 0x682d6 */

	/* 0x635f0 */
	tx1_put(o, TX1_F25D8,
		(short)((unsigned short)tx1_get(o, TX1_F25D8) + 1));

	shift = (unsigned)(2 * (int)o->vect_idx) & 31u;
	bits = (short)((unsigned)(unsigned short)tx1_get(o, TX1_F25D6) >> shift);

	mode = (short)((o->tx_flags & V34_TXFLAG_CALLER) == 0);
	q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, mode, bits, 2);
	(void)tx1_dpsk4(o, q);
	txmit(o);

	idx = (unsigned short)(((unsigned short)o->vect_idx + 1) & 7);
	o->vect_idx = (short)idx;

	if (o->role == 0x66) {
		/* 0x66ca8 */
		if (!(rx->flags & V34_RX_FLAG_DATA)) {
			if (o->md_length == 0)
				V34_TX_CASE_RETURN(V34TX1_LOOP);	/* 0x629c8 */
			if (tx1_get(o, TX1_F25DA) == 0)
				V34_TX_CASE_RETURN(V34TX1_LOOP);	/* 0x63da2 */
		}
		/* 0x66cd1 */
		hs_set_txstate(o, V34HS_XMIT0);
		o->vect_idx = 0;			/* 0x62d32 */
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x62d70 */
	}

	/* 0x636f0 */
	if (idx != 0)
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x63941 */

	if (o->txstate == V34HS_J1TXMIT) {
		/* 0x65653 */
		o->tx_scr_sr = 0;
		hs_set_txstate(o, V34HS_TRNSEG4A);
		o->prev_quadrant = 0;
		o->seg_symcount = 0;
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x640a1 */
	}

	/* 0x6370d */
	if (tx1_get(o, TX1_F25DA) != 2)
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x6409a */
	if (tx1_get(o, TX1_F25D8) <= 0x80)
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x6431f */

	/* 0x6372e */
	tx1_put(o, TX1_F25D6, (short)0x899f);
	hs_set_txstate(o, V34HS_J1TXMIT);

	/* 0x637c8 */
	if (o->short_aa78 != 0) {
		if (dsplibs_debug_level > 1)		/* 0x6c725 */
			dsplibs_debug_printf(
				"V34Hshak: on J1TXMIT - forced freeze echo" " (count2 = %d)...\r\n",
				o->short_aa78);
		v34FreezeEcho(o);
		o->short_aa78 = 0;
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x64326 */
	}
	V34_TX_CASE_RETURN(V34TX1_LOOP);				/* 0x629c8 */
