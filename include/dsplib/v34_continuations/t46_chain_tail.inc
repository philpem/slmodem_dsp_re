
	/*
	 * 0x65dcd is the second limit on one counter: 0xc7 at the head
	 * (0x65dad) and 0x4b0 here (0x65ddb), both `jle`, and this one
	 * re-reads +0xaa78 at 0x65dd4 through a freshly loaded object.
	 */
	if (sub == 0) {
		t46_past_the_counter(obj);		/* 0x65dcd */
		V34_CONT_RETURN();
	}
	/*
	 * 0x6add0.  With +0x3588 at 4 or 2 on every entry (finding F418),
	 * this is where the two bodies that tail-call the chain really
	 * end up, and it is two instructions and `jmp 62af1` -- `mov
	 * 0xc0(%esp),%ebp` then `movzwl 0x3596(%ebp),%ecx`.
	 */
	t3c_txblock(obj);				/* 0x6add0 */
