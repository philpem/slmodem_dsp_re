
	struct v34_object *o = (struct v34_object *)objp;
	short mode = (short)((o->tx_flags & V34_TXFLAG_CALLER) == 0);
	short q;

	if ((unsigned short)o->receiver.short_382 == 0x89b0u) {
		/* 0x67031 */
		short k;
		int point;

		q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, mode, 0xf, 2);
		o->cur_quadrant = (short)((q + (unsigned short)o->prev_quadrant) & 3);

		q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, mode, 3, 2);
		k = o->cur_quadrant;			/* re-read, 0x67131 */
		o->prev_quadrant = k;
		point = vect16[q + 4 * k];
		tx1_put_point(o, point);
		txmit(o);
		o->vect_idx = (short)((unsigned short)o->vect_idx + 4);
	} else {
		/* 0x6386b */
		q = (short)V34scrambler((unsigned *)&o->tx_scr_sr, mode, 3, 2);
		(void)tx1_dpsk4(o, q);
		txmit(o);
		o->vect_idx = (short)((unsigned short)o->vect_idx + 2);
	}

	/* 0x6392c */
	if ((unsigned short)o->vect_idx == 0x14u) {
		/* 0x66e01 */
		short txst = o->txstate;

		o->vect_idx = 8;
		hs_set_txstate(o, V34HS_DATAXMIT);
		if (dsplibs_debug_level > 1)		/* 0x66e48 */
			dsplibs_debug_printf("V34MP- E transmit completed\n");
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x6409a, 0x6431f */
	}
	V34_TX_CASE_RETURN(V34TX1_LOOP);				/* 0x63941 */
