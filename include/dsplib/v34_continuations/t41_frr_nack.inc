
	/*
	 * 0x6dca7's `cmpw $0x31,0x35a0` and `jle 6dd19`: the counter has to be
	 * past 0x31 and not at it, and the compare is signed.
	 */
	if (obj->short_35a0 <= 0x31) {
		t3c_txblock(obj);			/* 0x6dd19 */
		V34_CONT_RETURN();
	}
	if (obj->fsk.sr != 0) {
		/*
		 * 0x6dd06.  `cmpw $0x0,0xaae2` at 0x6dcb1 -- sixteen bits,
		 * where the arm's entry test on the same field is eight.
		 */
		t3c_txblock(obj);			/* 0x6dd06 */
		V34_CONT_RETURN();
	}
	if (obj->moh_msg_pending != 0) {
		/*
		 * 0x6dcf3.  `cmpb $0x0,0xabf8` at 0x6dcbb: the drop has
		 * already been reported, and reporting it is all the
		 * fall-through has left to do.
		 */
		t3c_txblock(obj);			/* 0x6dcf3 */
		V34_CONT_RETURN();
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34Handshake: Detected signal drop on "
				     "FRR request (after NACK), time to move " "to phase1...\r\n");

	obj->moh_msg_pending = 1;
	t3c_txblock(obj);
