
	short n;

	if (obj->short_aa78 <= T46_CNT_LOW) {
		/*
		 * 0x6add0, the tail the whole chain funnels into: reload
		 * the object, put +0x3596 in %ecx, jump to 0x62af1.  The
		 * guard above it is `cmpw $0xc7,0xaa78(%ebx)` at 0x6c3fa.
		 */
		t3c_txblock(obj);			/* 0x6add0 */
		V34_CONT_RETURN();
	}
	if ((obj->fsk.sr & 0xfff) != 0xf72) {
		/*
		 * 0x6add0 again, and this guard is TWELVE bits wide:
		 * `and $0xfff` then `cmp $0xf72` at 0x6c410, where the
		 * head at 0x65dbb masks ten and compares 0x372.  0xf72 &
		 * 0x3ff is 0x372, so this test is the stricter of the two.
		 */
		t3c_txblock(obj);			/* 0x6add0 */
		V34_CONT_RETURN();
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("TX_PHASE_ANS: count3 = %d \n",
				     (int)obj->short_aa7a);

	n = (short)(obj->short_aa7a + 1);
	/*
	 * 0x70c7f stores the stepped count and re-enters the chain at
	 * 0x6adc7 with +0x3588 re-read at 0x70c8d -- still the 2 that
	 * got here, so its zero arm cannot fire (finding F418).
	 */
	if (n <= T46_COUNT3_LIM) {			/* 0x70c7f */
		obj->short_aa7a = n;
		t46_chain_tail(obj, obj->short_3588);
		V34_CONT_RETURN();
	}

	/*
	 * 0x6c459.  The give-up zeroes +0xaa7a at 0x6c462 rather than
	 * leaving the thirteenth step there -- 0x6c447 re-read the count
	 * and `inc %eax`, and only 0x70c7f's arm stores it back.
	 */
	t46_body_detected(obj);				/* 0x6c459 */
