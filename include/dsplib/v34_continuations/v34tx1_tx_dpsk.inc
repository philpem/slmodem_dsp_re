
	struct v34_object *o = (struct v34_object *)objp;
	short bit;

	/* 0x62b9d */
	if (o->moh_active != 0)
		o->vect_idx = (short)((unsigned short)o->vect_idx + 1);

	/* 0x62bb5, and 0x684bb where it does not inline */
	bit = getbit(tx1_bitsource(o));
	if (bit >= 0) {
		/* 0x64a13 */
		tx1_dpsk_tone(o, bit);
		txmit(o);				/* 0x64a42 */
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x640a1 */
	}

	/* 0x64e5c: the message is over.  The flag is RE-READ (0x64e63) */
	if (o->moh_active == 0) {
		/*
		 * 0x654dd.  The compare against TONE_AB cannot be false, for
		 * finding F342's reason at 65: the arm is reached only through
		 * table 1 at `txstate == 24` and nothing between the dispatch
		 * and here writes +0x3596 -- `getbit` does not.  It is
		 * written as the object writes it.
		 */
		hs_set_txstate(o, V34HS_TONE_AB);
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x63948 */
	}

	if (dsplibs_debug_level > 1)			/* 0x68704 */
		dsplibs_debug_printf(
			"End of current MOH msg: isterm=%d, count1(%d)," " pktcount(%d)...\r\n",
			*((signed char *)o + TX1_MOH_MSG_PENDING), o->vect_idx,
			tx1_bitsource(o)->repeats);

	if (o->moh_msg_pending == 0) {
		/*
		 * 0x67c4e.  The reader is re-armed BY HAND and read again --
		 * `getbit`'s own restart arm with the `repeat` test taken out,
		 * so a reader that declines to repeat is restarted anyway.
		 * The stores are the object's, in the object's order; the
		 * compiler sank the rest of them into the successors, which is
		 * why 0x6a855 and 0x6c9c8 store 0xffff into `crc` again.
		 */
		struct v34_bitsource *b = tx1_bitsource(o);

		b->repeats = (short)((unsigned short)b->repeats + 1);
		b->crc = (short)0xffff;
		b->pos = 0;
		b->idx = 0;
		b->acc = b->acc0;
		b->avail = b->avail0;

		tx1_dpsk_tone(o, getbit(b));
		txmit(o);				/* 0x67d3e */
		V34_TX_CASE_RETURN(tx1_moh_hold(o));			/* 0x64fec */
	}

	/*
	 * 0x64e91.  Both selectors are read as ints and compared UNSIGNED --
	 * `cmp $0x1 ; je ; jb` picks zero alone and `cmp $0x3 ; ja` sends a
	 * negative one to the same place a large one goes.
	 */
	if (o->moh_message == 1) {
		tx1_moh_on_hold(o);			/* 0x689f6 */
	} else if (o->moh_message == 0) {
		int got = o->moh_recvd;			/* 0x6901d */

		if (got == 4)
			tx1_moh_on_hold(o);
		else if ((unsigned)got > 4u) {
			/* 0x6a3c6 */
			if (got != 5)
				tx1_moh_reinit(o, MOH_ILLEGAL);	/* 0x69041 */
			else
				tx1_moh_send(o);	/* 0x6a3cf */
		} else if (got == 0)
			tx1_moh_on_hold(o);
		else
			tx1_moh_reinit(o, MOH_ILLEGAL);	/* 0x69041 */
	} else if ((unsigned)o->moh_message <= 3u) {
		tx1_moh_cleardown(o);			/* 0x64eaf */
	}

	/* 0x64fde */
	o->moh_msg_pending = 0;
	V34_TX_CASE_RETURN(tx1_moh_hold(o));				/* 0x64fec */
