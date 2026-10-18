
	struct v34_receiver *rx;
	unsigned short flags;

	/*
	 * 0x653ee.  The arm's first act, ahead of the retrain test at 0x65401:
	 * the block is demodulated even on the call that hands the handshake
	 * back to `v34handshakinit`.
	 */
	receiver(obj);					/* 0x653ee */

	rx = &obj->receiver;
	/*
	 * 0x653f7.  `movzwl 0x122(%edx),%esi` -- read once, and %esi carries
	 * it into the whole arm as the working copy -- 0x67999 reads it as
	 * `flags` -- while the retrain test below works on a second copy, in
	 * %ebx.
	 */
	flags = rx->flags;				/* 0x653f7 */

	if ((flags & V34_RX_FLAG_RETRAIN) != 0
	    || ((int)rx->rx_blocks > 7 * (int)obj->ratecfg.rx_baud
		&& (flags & 0x80) == 0)) {
		/*
		 * 0x6544e.  The second argument is a literal 1, pushed at
		 * 0x65447, and nothing follows the call but the trace and the
		 * shared tail -- the retrain path writes no state of its own.
		 */
		v34handshakinit(obj, 1);		/* 0x6544e */
		/*
		 * 0x690e1 is a cold block: the format string and no arguments
		 * at all, then `movzwl 0x3596`/`jmp 62af1` rather than a
		 * return to the gate at 0x65453.
		 */
		if (DSPLIB_DEBUG_ON())			/* 0x690e1 */
			dsplibs_debug_printf("V34RETRAIN, going into retrain " "in Handshake\n");
	} else {
		/*
		 * 0x67999 is the merge of both not-retraining branches -- the
		 * `jle` at 0x65429 and the `jne` at 0x65435 -- and it is
		 * entered with the flags in %si and the baud counter in %di,
		 * where the entry left them.
		 */
		t4_receive_body(obj, flags);		/* 0x67999 */
	}

	t3c_txblock(obj);
