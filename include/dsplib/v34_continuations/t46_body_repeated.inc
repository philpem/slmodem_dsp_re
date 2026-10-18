
	obj->short_3588 = 4;
	t46_reset_core(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Repeated info0 is detected, errorrecovery "
				     "is initialized in TX_PHASE1_ANS\n");

	/*
	 * 0x6adbd re-reads +0x3588 at 0x6adb6 before `cmp $0x2,%ax`, so
	 * the 4 this body has just stored is what the chain sees and the
	 * only way out is 0x6add0 (finding F418).
	 */
	t46_chain_full(obj);			/* 0x6adbd */
