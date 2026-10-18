
	if (obj->short_aa78 <= T46_CNT_HIGH) {
		/*
		 * 0x6c120, another copy of the tail: the object, +0x3596
		 * into %ecx, and `jmp 62af1` at 0x6c12e.  The counter was
		 * re-read at 0x65dd4 for the compare above.
		 */
		t3c_txblock(obj);			/* 0x6c120 */
		V34_CONT_RETURN();
	}
	/*
	 * 0x65de6 is `cmpw $0x0,0xaae2(%edi)` -- sixteen bits wide, not
	 * the byte spelling microstate 62 keeps (findings F417 and F553) --
	 * and 0x6bd7a is one more copy of the transmit tail.
	 */
	if (obj->fsk.sr == 0) {
		t3c_txblock(obj);			/* 0x6bd7a */
		V34_CONT_RETURN();
	}

	/*
	 * 0x65df4 opens with the 1 for +0x358a in %eax and a 4 staged in
	 * %ebp: this body STORES +0x3588, where 0x6f90c ORs into it
	 * (finding F410).
	 */
	t46_body_repeated_late(obj);			/* 0x65df4 */
