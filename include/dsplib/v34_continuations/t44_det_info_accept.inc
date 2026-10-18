
	struct v34_receiver *rx = &obj->receiver;
	short len = rec->nbits;
	short count;
	int nbytes;
	int i;

	/*
	 * THE ORDER IS THE OBJECT'S: 0x4d, then 0x26, then 0x08, and the
	 * default is what falls out of all three.  The three constants are
	 * pinned from both sides -- a case at each length drives its body and
	 * a case one away drives the default.
	 */
	if (len == 0x4d)
		V34_CONT_RETURN(t44_accept_len4d(obj));		/* 0x6f438 */
	if (len == 0x26)
		V34_CONT_RETURN(t44_accept_len26(obj, (short *)rec));	/* 0x6ed17 */
	if (len == 0x08)
		V34_CONT_RETURN(t44_accept_len08(obj, (short *)rec, count2));	/* 0x6ea38 */

	if (obj->short_358a == 1) {
		struct v34_bitsource *blk = (struct v34_bitsource *)obj->paa6c;

		/* 0x6e571, and it is the OTHER record's +0x04. */
		if ((blk->word[2] & 0x80) == 0) {
			blk->repeats = 0;
			blk->word[2] = (short)(blk->word[2] | 0x80);
		}

		/*
		 * How many BYTES arrived: the bit count divided by eight
		 * truncating toward zero, plus one if any bits are left over.
		 * 0x6e59b's `lea 0x7(%edx)` before the `and` is how GCC spells
		 * a truncating divide of a value that may be negative, and
		 * 0x6e5b8 tests the low three bits of the SAME count.
		 */
		count = obj->short_aa78;
		nbytes = count / 8 + ((count & 7) != 0 ? 1 : 0);
		obj->short_abc2 = (short)nbytes;

		/* Into the array the restart clears -- finding F401. */
		for (i = 0; i < nbytes; i++)
			obj->short_abae[i] = rec->word[i];

		if ((rec->word[2] & 0x80) == 0) {
			/*
			 * 0x6e5fb.  This exit does NOT rejoin the bit clock:
			 * 0x6e740 jumps straight to the transmit dispatch.
			 */
			hs_set_rxstate(obj, V34HS_RX_DPSK);
			hs_set_microstate(obj, V34HS_DET_SYNC);
			((struct v34_bitsource *)obj->paa70)->crc = -1;
			/*
			 * 0x6e739 puts -1 in +0xaae2 and 0x6e740 is one
			 * `jmp 62af1`.  The `return 1` has no instructions
			 * of its own -- caller and callee leave through the
			 * same jump, which is why the caller's `return`
			 * carries this address too.
			 */
			obj->fsk.sr = -1;
			t3c_txblock(obj);		/* 0x6e740 */
			V34_CONT_RETURN(1);
		}
	}

	/* 0x6e745: F358A != 1 (0x6e561) and +0x04 bit 7 set (0x6e5f5). */
	obj->short_aa78 = 0;

	/*
	 * The buffer is obj+0xa97c LITERALLY and not the +0xaa70 record --
	 * 0x6e757 is `lea 0xa97c(%ebx)`.  `v34handshakinit` mode 0 hands
	 * `V34SetINFO0dBits` the same fixed address.
	 */
	V34GiveINFO0dBits(obj, (const short *)&obj->msgrec[1]);

	if (DSPLIB_DEBUG_ON()) {
		const short *r = (const short *)obj->paa70;

		dsplibs_debug_printf("%s 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,"
				     "0x%x,0x%x,0x%x\n", "V34INFO, rxinfo0",
				     (unsigned)(unsigned short)r[0],
				     (unsigned)(unsigned short)r[1],
				     (unsigned)(unsigned short)r[2],
				     (unsigned)(unsigned short)r[3],
				     (unsigned)(unsigned short)r[4],
				     (unsigned)(unsigned short)r[5],
				     (unsigned)(unsigned short)r[6],
				     (unsigned)(unsigned short)r[7],
				     (unsigned)(unsigned short)r[8],
				     (unsigned)(unsigned short)r[9]);
	}

	/*
	 * Where the handshake goes next, and `is_short` is `V34GiveINFO0dBits`'s
	 * own output rather than an input (v34info.c).
	 *
	 * The originating side skips BOTH the `sr` reset at 0x6e814 and the
	 * counter reload at 0x6e8ac; the answering side does the reset either
	 * way and the reload only on the short-phase-2 branch.
	 *
	 * The reset is one instruction sitting BETWEEN the compare and its
	 * branch -- 0x6e80c is `cmpw $0x0,0xabcc(%eax)`, 0x6e814 is the store
	 * to +0xaae2, and 0x6e81b is the `je`.  So "either way" is not a
	 * reading of the control flow, it is what the instruction order
	 * forces: the store has already happened whichever way the `je` goes.
	 */
	if (obj->role == 0x65) {
		hs_set_microstate(obj, V34HS_RX_PHASE1_CALL);
	} else {
		obj->fsk.sr = -1;			/* 0x6e814 */
		if (obj->is_short != 0) {
			hs_set_microstate(obj, V34HS_RX_PHASE1_ANS);
			/*
			 * 0x6e8ac is also where the inlined setter
			 * lands when the microstate ALREADY holds 0x31 --
			 * `je 6e8ac` at 0x6e82c -- so the reload of +0xaa78
			 * from +0xaa7c runs whether the state moved or not.
			 */
			obj->short_aa78 = obj->filtdelay;	/* 0x6e8ac */
		} else {
			hs_set_microstate(obj, V34HS_TX_PHASE1_ANS);
		}
	}

	/* 0x6e8c1: the detector this INFO0 was waiting on is disarmed. */
	rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_DET_PENDING);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34INFO,%s info0 received in DET_INFO\n",
				     obj->is_short != 0 ? " Short Phase 2" : "");

	V34_CONT_RETURN(0);					/* 0x6e90c */
