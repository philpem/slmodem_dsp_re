
	struct v34_object *o = (struct v34_object *)objp;
	/* Arm 51 is the one arm here that reads a state word belonging to
	 * another machine: it selects between two copies of its own loop on
	 * `microstate == TX_L1`, having been dispatched on
	 * `txstate == TX_L1`.  The two 51s are the same number in two
	 * machines and `StateName` is one table for all three
	 * (docs/v34handshak.md). */
	int doubled = o->microstate == V34HS_TX_L1;
	short buf[4];
	int i;

	for (i = 0; i < 4; i++) {
		unsigned idx = (unsigned short)o->vect_idx;
		int v = probe[idx & 0x3f];

		if (doubled)
			v = (short)(2 * v);
		buf[i] = (short)((v * o->tx_scale) >> 14);
		o->vect_idx = (short)(idx + 1);
	}

	txwritequeue(&o->txq, buf);

	if (o->microstate != V34HS_TX_L1)
		V34_TX_CASE_RETURN(V34TX1_LOOP);
	if ((unsigned short)o->vect_idx != 0x600u)
		V34_TX_CASE_RETURN(V34TX1_LOOP);

	hs_set_microstate(o, V34HS_TX_L2);
	/* 0x62d32 */
	o->vect_idx = 0;
	V34_TX_CASE_RETURN(V34TX1_LOOP);
