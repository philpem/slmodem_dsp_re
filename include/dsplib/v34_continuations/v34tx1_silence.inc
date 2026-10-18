
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx = &o->receiver;
	short quiet[4];
	short txst;
	unsigned short n;

	quiet[0] = 0;
	quiet[1] = 0;
	quiet[2] = 0;
	quiet[3] = 0;
	o->short_aa7a = 0;
	txwritequeue(&o->txq, quiet);

	txst = o->txstate;

	if (txst == V34HS_SILENCERETRAIN) {
		/* 0x66b87 */
		short want;

		n = (unsigned short)((unsigned short)o->vect_idx + 1);
		o->vect_idx = (short)n;
		if (n != 0xb4)
			V34_TX_CASE_RETURN(V34TX1_LOOP);		/* 0x63da2 */

		/* 0x66ba1 */
		if (o->moh_active != 0)
			want = V34HS_MOH_TONE;
		else					/* 0x68a7c */
			want = o->role == 0x65 ? V34HS_RX_PHASE1_CALL
						: V34HS_TX_PHASE1_ANS;
		hs_set_microstate(o, want);

		/* 0x66be0 */
		hs_set_rxstate(o, V34HS_RX_DPSK);
		/* 0x66c10 */
		hs_set_txstate(o, V34HS_TONE_AB);

		/* 0x66c32 */
		o->short_358c = 0;
		o->vect_idx = 0;
		o->short_aa78 = 0;
		o->fsk.sr = -1;
		o->fsk.nbits = 0;
		if (dsplibs_debug_level > 1)		/* 0x66c72 */
			dsplibs_debug_printf(
				"V34RETRAIN, SILENCERETRAIN finished," " rx->rxflgs,=0x%x,rx->gain=0x%x,"
				"gainestimate=0x%x\n",
				rx->flags, rx->agc_gain, rx->agc_start_gain);
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x63941 */
	}

	if (txst != V34HS_SILENCEINFO)
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x6409a */

	/*
	 * 0x6410b.  THE COUNT IS STORED ON BOTH PATHS, and the transcript is
	 * what says so: the blob's `SILENCEINFO=>TX_DPSK` line prints `[1]10`,
	 * and `[1]` is `vect_idx` read at the moment of the print.  Zeroing
	 * before the state change -- which is how this was written until
	 * finding F573 -- prints `[1]0` instead.  Both orders leave the same
	 * bytes behind, so no byte comparison can separate them and none did
	 * for six batches; only turning the diagnostics on does.  74 stores
	 * its count before its own comparison in exactly this shape, and
	 * mutation 181 has asserted that since it landed.
	 */
	n = (unsigned short)((unsigned short)o->vect_idx + 1);
	o->vect_idx = (short)n;
	if (n != 0xa)
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x629cf */

	/* 0x6858f */
	hs_set_txstate(o, V34HS_TX_DPSK);
	o->vect_idx = 0;
	o->paa6c = o->msgrec;
	V34SetINFO0aBits(o, (short *)o->msgrec);

	{
		/*
		 * RE-READ, not the pointer just stored, because that is what
		 * the object does at 0x685f2 and 0x69fe1 -- the same reading
		 * V34hshak.c:1459 records for `v34handshakinit`'s copy of
		 * this sequence.  `V34SetINFO0aBits` does not write +0xaa6c,
		 * so nothing observable turns on it.
		 */
		struct v34_bitsource *r = (struct v34_bitsource *)o->paa6c;
		short lead = (o->role == 0x65 && o->v90_receiver != 0)
			     ? 0x1e : 0x11;

		r->crc = -1;
		r->pos = 0;
		r->idx = 0;
		r->repeats = 0;
		r->nbits = lead;
		/* 0x68616 */
		r->wordbits = 8;
		r->crc_on = 1;
		r->avail = 0x10;
		r->avail0 = 0x10;
		r->repeat = 0;
		r->acc = 0xff72;
		r->acc0 = 0xff72;
	}

	o->short_358c = 0;
	V34_TX_CASE_RETURN(V34TX1_LOOP);				/* 0x640a1 */
