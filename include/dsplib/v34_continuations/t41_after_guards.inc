
	/*
	 * 0x6ab33's `test %cl,%cl` is a BYTE test of the +0xabe8 its callers
	 * loaded, and 0x669fa is also where the arm's first door leaves -- one
	 * stub for two paths.
	 */
	if (abe8 == 0) {
		t3c_txblock(obj);			/* 0x669fa */
		V34_CONT_RETURN();
	}
	/*
	 * 0x6ab42's `cmpl $0x1,0xabf0` is THIRTY-TWO bits wide, so a low
	 * halfword of 1 with anything at all in the high half does not reach
	 * 0x6dca7.
	 */
	if (obj->moh_message == 1) {
		t41_frr_nack(obj);			/* 0x6dca7 */
		V34_CONT_RETURN();
	}
	/* 0x6ab4f, the fall-through: +0xabe8 set, +0xabf0 not 1. */
	t3c_txblock(obj);				/* 0x6ab4f */
