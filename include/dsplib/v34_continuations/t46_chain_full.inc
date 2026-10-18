
	short sub = obj->short_3588;

	/*
	 * 0x6c3f3 reloads the object and gates on the same 199 the head
	 * uses at 0x65dad -- `cmpw $0xc7,0xaa78(%ebx)` at 0x6c3fa -- on
	 * a path along which the head never read the counter at all.
	 */
	if (sub == 2) {
		t46_info0_counting(obj);		/* 0x6c3f3 */
		V34_CONT_RETURN();
	}
	t46_chain_tail(obj, sub);
