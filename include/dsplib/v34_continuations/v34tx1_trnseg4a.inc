
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx = &o->receiver;
	struct v34_ratecfg *cfg = &o->ratecfg;
	int n, lim, baud, period;
	short *rec;

	if ((unsigned short)o->receiver.short_382 == 0x89b0u) {
		/* 0x66e59 */
		short k = tx1_scramble2(o);
		short q;

		o->cur_quadrant = k;
		q = tx1_scramble2(o);
		tx1_put_point(o, vect16[q + 4 * k]);
	} else {
		/* 0x62e3b */
		short q = tx1_scramble2(o);

		o->cur_quadrant = q;
		tx1_put_point(o, vect4[q]);
	}

	/* 0x62ecd */
	txmit(o);

	/* 0x62edc */
	o->seg_symcount = (short)((unsigned short)o->seg_symcount + 1);
	n = o->seg_symcount;
	baud = cfg->baud;
	period = cfg->period;
	lim = baud + (baud >> 1) + period;

	if (n != lim) {
		if ((unsigned)o->hs_mode <= 3u)
			V34_TX_CASE_RETURN(V34TX1_LOOP);		/* 0x63941 */
		if (n < lim) {
			/* 0x6559c */
			if (n < baud + period)
				V34_TX_CASE_RETURN(V34TX1_LOOP);	/* 0x6409a */
			if ((int)rx->equerr > (int)o->receiver.short_4b4 + 10)
				V34_TX_CASE_RETURN(V34TX1_LOOP);	/* 0x6431f */
		}
	}

	/* 0x62f22 */
	rec = (short *)((char *)objp + TX1_FAA3C);
	o->prev_quadrant = (short)(unsigned short)o->cur_quadrant;
	o->paa6c = rec;

	if (rx->flags & V34_RX_FLAG_RENEG) {
		/* 0x62f5f */
		rec[2] = 0;
		rec[0] = 0;
		o->receiver.short_4b4 = rx->equerr;
	} else {
		tx1_ts_snapshot(o, rx, rec);
	}

	V34_TX_CASE_RETURN(tx1_ts_rates(o, rx, cfg, rec));
