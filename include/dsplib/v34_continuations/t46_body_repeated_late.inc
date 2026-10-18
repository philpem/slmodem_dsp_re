
	obj->short_3588 = 4;
	t46_reset_core(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Repeated info0 is detected (after 1200), "
				     "errorrecovery is initialized in " "TX_PHASE1_ANS\n");

	t3c_txblock(obj);
