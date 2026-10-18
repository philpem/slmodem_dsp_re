
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx = &o->receiver;
	int wide = 0;
	int n = 0;

	o->cur_quadrant = 0;					/* 0x639ab */

	for (;;) {
		struct v34_bitsource *b;
		unsigned short flags;
		short bit, idx;

		wide = (unsigned short)o->receiver.short_382 == 0x89b0u;
		if (n > (wide ? 3 : 1))
			break;

		/* 0x639db, and 0x6484c where it does not inline */
		b = tx1_bitsource(o);
		bit = getbit(b);

		/*
		 * 0x63ab1.  `n` reaches the shift through `movzbl`, and the
		 * bit reaches it sign-extended -- so the reader's -1 fills
		 * `cur_quadrant` from bit `n` up rather than setting one bit.
		 */
		o->cur_quadrant = (short)((unsigned short)o->cur_quadrant
				   | (unsigned short)
				     ((unsigned)(int)bit
				      << ((unsigned)(unsigned char)n & 31u)));

		/* 0x63ad8 */
		o->vect_idx = (short)((unsigned short)o->vect_idx + 1);
		idx = o->vect_idx;
		flags = rx->flags;

		if (idx == ((flags & 0x20u) ? 0x55 : 0xbb)) {
			/* 0x6459b */
			b = tx1_bitsource(o);
			if (flags & 0x20u) {
				b->acc = (int)((unsigned)b->acc << 3);
				b->avail = (short)
					   ((unsigned short)b->avail + 3);
			} else {
				b->acc = (int)((unsigned)b->acc << 1);
				b->avail = (short)
					   ((unsigned short)b->avail + 1);
			}
		} else if (idx == ((flags & 0x20u) ? 0x58 : 0xbc)) {
			if (tx1_mp_sequence_end(o, rx)) {
				/* 0x6490e */
				wide = (unsigned short)
				       o->receiver.short_382 == 0x89b0u;
				break;
			}
		} else if ((unsigned short)o->vect_idx
			   == (unsigned short)o->short_3590) {
			/* 0x64727 */
			b = tx1_bitsource(o);
			b->acc = (int)((unsigned)b->acc << 1);
			b->avail = (short)((unsigned short)b->avail + 1);
			o->short_3590 =
				(short)((unsigned short)o->short_3590 + 0x11);
		}

		n = (short)(n + 1);			/* 0x63b38 */
	}

	if (wide)
		tx1_mp16(o, o->cur_quadrant);
	else
		tx1_mp4(o, o->cur_quadrant);

	txmit(o);				/* 0x62d5f and 0x64a42 */
	V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x62d70 and 0x640a1 */
