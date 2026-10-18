
	struct v34_receiver *rx = &obj->receiver;

	/*
	 * 0x65473, and it runs before either guard -- so even the two exits
	 * that do nothing else have run the gain control.
	 *
	 * THE ADDRESS IS ON THE LINE because rxstate 72's arm calls the same
	 * function with the same argument, and `\tV34agc(rx);` is a PREFIX of
	 * that line -- so the mutation anchored here matched twice the moment
	 * 0x650ee landed.  Finding F432's repair: information, not indentation.
	 */
	V34agc(rx);					/* 0x65473 */

	/* 0x65486, `cmpw $0x34`: 52 is TX_L2, not TX_PHASE3_ANS. */
	if (obj->microstate != V34HS_TX_L2) {
		/*
		 * The object tests the other way: 0x65486 `cmpw $0x34` and
		 * `je 6845e` jump TO the guard, so 0x65494 is the not-equal
		 * fallthrough.
		 */
		t3c_txblock(obj);			/* 0x65494 */
		V34_CONT_RETURN();
	}

	/* 0x6845e, sixteen bits and signed. */
	if (obj->vect_idx <= 0x5db) {
		t3c_txblock(obj);			/* 0x6846d */
		V34_CONT_RETURN();
	}

	if (t41_detect(obj) == 0) {
		/*
		 * 0x6af24, and the arithmetic is THIRTY-TWO bits wide.  See
		 * the head of this function.
		 */
		if ((int)obj->vect_idx <= (int)obj->rtd + 0x2418) {
			t3c_txblock(obj);		/* 0x6af47 */
			V34_CONT_RETURN();
		}

		/*
		 * 0x6b0f4.  This runs INSIDE the step, so a test driving it
		 * is the case finding F359 says `V34HS_REFINIT=1` cannot be
		 * used against: side A installs our library tables and side B
		 * the blob's, and no address comparison can settle two copies
		 * of one table.
		 */
		v34handshakinit(obj, 1);		/* 0x6b0f4 */

		/*
		 * The trace is exiled past the end of the arm: `ja 7170a` at
		 * 0x6b100, and it does not come back.  0x71716 is its own
		 * copy of the exit below, so that exit is in the object twice.
		 */
		if (DSPLIB_DEBUG_ON())			/* 0x7170a */
			dsplibs_debug_printf("V34RETRAIN, retrain is " "initiated in DET_AB\n");

		t3c_txblock(obj);
		V34_CONT_RETURN();
	}

	/*
	 * 0x69851.  Both arguments are sign-extended halfwords: the gain is
	 * the receiver's +0x136 and `count1` is the trace's `[1]`.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34RETRAIN, End of L2,rx->gain=0x%x," "count1=%d\n",
				     rx->agc_gain, obj->vect_idx);

	if (obj->role == 0x65) {
		/*
		 * 0x6b138.  The originating end builds INFO1c and hands the
		 * receive machine back to the FSK demodulator.
		 *
		 * IT IS INFO1c AND NOT INFO1a, and the length field is what
		 * says so: +0x18 of this record gets 0x4d, which is INFO1c's,
		 * and the 0x26 INFO1a length goes to a SECOND record at
		 * +0xa9dc below.  `V34SetINFO1aBits` assembles INFO1a or
		 * INFO1c or INFO1d (v34info.h), so the callee's name settles
		 * nothing; the object's own label for the trace at the bottom
		 * is "V34PROBE, txinfo1c".
		 */
		hs_set_txstate(obj, V34HS_TX_DPSK);

		probeselect(obj);			/* 0x6b1de */

		/*
		 * 0x6b1e3, and the store is BEFORE the call because the call
		 * takes the same interior pointer as its second argument.
		 *
		 * THE OBJECT WRITES THE TWELVE STORES BELOW THROUGH +0xaa6c
		 * AND THIS DOES NOT.  0x6b1f5 reloads the field into `%ebp`
		 * although `%ebx` still holds the identical value, which is
		 * what a source dereferencing the pointer rather than a local
		 * compiles to.  The two spellings cannot differ: the store
		 * two instructions earlier is what the reload reads, and
		 * `V34SetINFO1aBits` does not touch +0xaa6c.  So the
		 * differential tier cannot see the difference and these are
		 * microstate 41's own statements verbatim rather than a
		 * second reading of the same record.  Finding F726.
		 */
		obj->paa6c = &obj->msgrec[2];		/* 0x6b1e3 */
		V34SetINFO1aBits(obj,			/* 0x6b1f0 */
				 (short *)&obj->msgrec[2]);

		/*
		 * EACH STORE CARRIES ITS ADDRESS, and that is not decoration:
		 * these nine statements are character for character
		 * microstate 41's at 0x6d387, so without the addresses no
		 * mutation anchored on one of them could tell the two apart.
		 * Finding F432's hazard, answered with information rather than
		 * with indentation.
		 */
		/*
		 * 0x6b1fb IS FIVE STORES: +0x14 = 0xffff, +0x1a, +0x1e and
		 * +0x22 = 0, then +0x18 = 0x4d, all through the %ebp reloaded
		 * at 0x6b1f5.  The six that follow are in the object's order,
		 * +0x1c, +0x16, +0x28, +0x2a, +0x20 -- and 0x6f669..0x6f6b3
		 * writes the same twelve offsets in the same order with 0x26
		 * at +0x18 (finding F443).
		 */
		t41_record_head(&obj->msgrec[2], 0x4d);     /* 0x6b1fb */
		obj->msgrec[2].wordbits = 8;		    /* 0x6b219 */
		obj->msgrec[2].crc_on = 1;		    /* 0x6b21f */
		obj->msgrec[2].avail = 0x10;		    /* 0x6b225 */
		obj->msgrec[2].avail0 = 0x10;		    /* 0x6b22b */
		obj->msgrec[2].repeat = 0;		    /* 0x6b231 */
		/*
		 * THIRTY-TWO BITS, both of them: `movl $0xff72` and not
		 * `movw`, so the immediate is +65394 and the halfword above
		 * each is zeroed too.
		 */
		obj->msgrec[2].acc = 0xff72; /* 0x6b23e */
		obj->msgrec[2].acc0 = 0xff72; /* 0x6b245 */

		/*
		 * 0x6b24c and the 0x26 arm's 0x6f6ba are the same seven
		 * bytes, `mov %di,0x358c(%esi)`, and both clear +0x358c
		 * immediately before a state move.  The word that move
		 * compares was loaded at 0x6b237, in the middle of the record
		 * fill: `cmp $0x2b` -- RX_DPSK is 43 -- is at 0x6b253.
		 */
		obj->short_358c = 0;		/* 0x6b24c */

		hs_set_rxstate(obj, V34HS_RX_DPSK);
		hs_set_microstate(obj, V34HS_INFODONE);

		/* 0x6b379: the SECOND record, and INFO1a's length. */
		obj->paa70 = &obj->msgrec[3];
		obj->msgrec[3].nbits = 0x26;

		/*
		 * 0x6b3a2, and it reads the record through `%ebx` -- obj +
		 * 0xa9ac -- rather than through the pointer it has just
		 * aimed, which is the other half of the note above.
		 */
		if (DSPLIB_DEBUG_ON()) {
			const unsigned short *p =
			    (const unsigned short *)&obj->msgrec[2];

			dsplibs_debug_printf(
			    "%s 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x," "0x%x,0x%x\n",
			    "V34PROBE, txinfo1c",
			    (unsigned)p[0], (unsigned)p[1], (unsigned)p[2],
			    (unsigned)p[3], (unsigned)p[4], (unsigned)p[5],
			    (unsigned)p[6], (unsigned)p[7], (unsigned)p[8],
			    (unsigned)p[9]);
		}
	} else {
		/*
		 * 0x69888.  The answering end moves three state words and
		 * builds nothing.
		 */
		hs_set_txstate(obj, V34HS_TONE_AB);
		hs_set_microstate(obj, V34HS_TX_PHASE3_ANS);
		hs_set_rxstate(obj, V34HS_RX_DPSK);
	}

	/*
	 * 0x69924, the tail both halves reach.  The receiver's +0x136 is
	 * restored from its +0x264 -- an unmodelled halfword this arm only
	 * reads -- and the FSK demodulator's four scalars are re-armed for
	 * the next message, `next` at 6 rather than 0.
	 */
	/* 0x69933 */
	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);
	rx->agc_gain = (short)*(const unsigned short *)((const char *)rx
							+ T3M_RX_F264);

	/*
	 * +0xaae2 `sr` (finding F553), +0xaae0 `nbits`, +0xaadc `phase`,
	 * +0xaade `next`: the object stores them in THAT order, which is
	 * not the order they lie in, and all four values were already in
	 * registers -- 0x69928, 0x6992a, 0x6992c and `mov $0x6,%ebx` at
	 * 0x6992e -- before the flag word above was even read.
	 */
	obj->fsk.sr = 0;			/* 0x6995f */
	obj->fsk.nbits = 0;			/* 0x69966 */
	obj->fsk.phase = 0;			/* 0x6996d */
	obj->fsk.next = 6;			/* 0x69974 */
	/*
	 * 0x6997b and 0x69982 store registers the block above cleared -- `xor
	 * %eax,%eax` at 0x6994d, `xor %edi,%edi` at 0x6995d -- and both fields
	 * are ones the state-transition trace reports: 0x6ff94 pushes +0x2aa2
	 * and +0xaa78, finding F633's busiest counter, as two of its arguments.
	 * The whole run of six goes through the object pointer reloaded at
	 * 0x69956.
	 */
	obj->vect_idx = 0;			/* 0x6997b */
	obj->short_aa78 = 0;		/* 0x69982 */

	/*
	 * 0x69989 is an exit and not a call: `movzwl 0x3596(%ecx),%ecx` and
	 * `jmp 62af1`, where the txstate is sign-extended, has 5 subtracted
	 * and is bounds-checked against 0x45 before the dispatch.
	 */
	t3c_txblock(obj);				/* 0x69989 */
