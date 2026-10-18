
	short tx = obj->txstate;

	/*
	 * 0x6b5b0, reached by `cmp $0x18,%cx; je` at 0x65d7b.  The two
	 * fields are two compares with their own `je 65d8f` (0x6b5bb and
	 * 0x6b5c6), so either one clear leaves the counter alone.
	 */
	if (tx == V34HS_TX_DPSK) {			/* 0x6b5b0 */
		short *r = (short *)obj->paa6c;

		if (r[0x20 / 2] != 0 && r[0x22 / 2] != 0) {
			obj->short_aa78 = 0;
			r[0x20 / 2] = 0;
		}
	/*
	 * 0x6b4ba, reached by `cmp $0x3c,%cx` at 0x65d85 on the transmit
	 * state the head read once.  It re-reads +0xaa78 through its own
	 * object load, steps it, and only then compares: `inc %eax` then
	 * `cmp $0x18f,%ax`.
	 */
	} else if (tx == V34HS_TONE_AB) {		/* 0x6b4ba */
		short n = (short)(obj->short_aa78 + 1);

		if (n <= T46_TONE_LIMIT) {
			/*
			 * 0x6f8f9 stores the step and jumps
			 * back to 0x65d8f -- the head's
			 * +0x3588 test, not the transmit tail.
			 */
			obj->short_aa78 = n;	/* 0x6f8f9 */
		} else if (obj->fsk.sr != 0) {
			/*
			 * 0x6fb6e is its twin for the
			 * past-399 path with +0xaae2 set:
			 * same store, same `jmp 65d8f`.
			 */
			obj->short_aa78 = n;	/* 0x6fb6e */
		} else if (obj->retrain_req != 0) {
			obj->short_aa78 = n;
			/*
			 * 0x6f90c stores the step through
			 * the %edi 0x6b4ba loaded, then
			 * aims +0xaa6c at obj+0xa94c and
			 * +0xaa70 at obj+0xa97c.
			 */
			t46_body_retrain(obj);		/* 0x6f90c */
			V34_CONT_RETURN();
		/*
		 * 0x6b4ee is the one path of the four that does NOT keep
		 * the step: `xor %eax,%eax` and `mov %ax,0xaa78(%esi)`
		 * restart the counter, and +0x358c is `xor $0x1` at
		 * 0x6b50e, stored back sixteen bits wide (finding F413).
		 */
		} else {				/* 0x6b4ee */
			obj->short_aa78 = 0;
			obj->short_358c = (short)(obj->short_358c ^ 1);
			hs_set_microstate(obj, V34HS_RX_PHASE1_ANS);
			obj->short_358a = 2;
			t3c_txblock(obj);
			V34_CONT_RETURN();
		}
	}

	/* 0x65d8f, where both head guards rejoin, 0x6f8f9 included. */
	/* 0x6adbd re-reads +0x3588 at 0x6adb6; 0x65d96 is this test's. */
	if (obj->short_3588 != 0) {
		t46_chain_full(obj);			/* 0x6adbd */
		V34_CONT_RETURN();
	}

	/* 0x65da6, on the %esi 0x65d8f loaded, and a ten-bit mask. */
	if (obj->short_aa78 > T46_CNT_LOW
	    && (obj->fsk.sr & 0x3ff) == 0x372) {
		/*
		 * 0x6abc1 opens by clearing +0xabca and storing 4 into
		 * +0x3588 at 0x6abd8 -- a store, like the other two, and
		 * not 0x6f90c's OR (finding F410).
		 */
		t46_body_repeated(obj);			/* 0x6abc1 */
		V34_CONT_RETURN();
	}

	/*
	 * 0x65dcd, and the head is the only thing that reaches it: the
	 * chain's own `test %ax,%ax` at 0x6adc7 never sees zero
	 * (finding F418).
	 */
	t46_past_the_counter(obj);			/* 0x65dcd */
