
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx = &o->receiver;

	o->txpoint.c[0] = 0;
	o->txpoint.c[1] = 0;
	txmit(o);

	if (rx->flags & V34_RX_FLAG_LATE_TRN) {
		o->tx_flags = (short)((unsigned short)o->tx_flags | V34_TXFLAG_SEG4A);
		hs_set_txstate(o, V34HS_SSEG);
		o->prev_quadrant = 0;
		o->seg_symcount = 0;
		o->tx_scr_sr = 0;
	}
	V34_TX_CASE_RETURN(V34TX1_LOOP);
