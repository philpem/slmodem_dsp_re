
	obj->short_aa7a = 0;
	obj->short_3588 = 4;
	obj->paa6c = obj->msgrec;
	t46_reset_core(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("info0 is detected, info0 is initialized "
				     "in TX_PHASE1_ANS\n");

	/*
	 * 0x6adc7 enters the chain one compare in, past the `== 2` test.
	 * Its `test %ax,%ax` cannot fire from here -- this body stored 4
	 * into +0x3588 -- which is finding F418's unreachable arm.
	 */
	t46_chain_tail(obj, obj->short_3588);	/* 0x6adc7 */
