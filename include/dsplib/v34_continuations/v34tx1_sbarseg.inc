
	struct v34_object *o = (struct v34_object *)objp;
	unsigned short n;

	tx1_put_point(o, vect4[2]);
	txmit(o);
	tx1_put_point(o, vect4[1]);
	txmit(o);

	n = (unsigned short)((unsigned short)o->seg_symcount + 1);
	if (n != 8) {
		o->seg_symcount = (short)n;
		V34_TX_CASE_RETURN(V34TX1_LOOP);		/* 0x629c8 */
	}

	/* 0x671f0 */
	o->seg_symcount = 0;
	if ((unsigned short)o->tx_flags & V34_TXFLAG_SEG4A) {
		hs_set_txstate(o, V34HS_TRNSEG4A);
	} else if (o->short_35a4 == 0) {
		/* 0x68375 */
		hs_set_txstate(o, V34HS_PPSEG);
	} else if ((unsigned short)o->tx_flags & V34_TXFLAG_PPSEG) {
		/* 0x69eca */
		o->seg_symcount = o->seg_len;
		hs_set_txstate(o, V34HS_PPSEG);
	} else {
		/* 0x6783e */
		int span;
		short q;

		o->seg_len = (short)((unsigned short)o->short_35a4 * 0x53);
		hs_set_txstate(o, V34HS_TXMD);

		/* 0x67885 */
		V34SetupModulator(v34_object_modulator(o),
				  4800, 2400, 0, 0, 0);

		/*
		 * `(0x5e8 - good_run) << 14` with the subtraction done in an int.
		 * Spelled through `unsigned` because the left shift of a
		 * negative int is undefined in C and the object's `shl` is
		 * not; the bits are the same either way.
		 */
		span = (int)((unsigned)(0x5e8 - o->dmadelay) << 14);
		q = (short)(span / 9600);
		o->short_aa78 = (short)((((int)q * 0x960) >> 14) + 0x96);
		if (dsplibs_debug_level > 1)		/* 0x67916 */
			dsplibs_debug_printf(
				"Moving to TX MD, would take %d symbols ,"
				" echo start delay is %d...\r\n",
				o->seg_len,
				o->short_aa78);
	}

	/* 0x67236 */
	o->vect_idx = 0;
	o->short_358e = 0;
	V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x63948 */
