
	if ((int)aae0 < (((int)obj->rtd) >> 4) + 0x1ea) {
		/*
		 * 0x6dfd5 is the `jl` itself, straight into the dispatch entry
		 * with no stub, and the delay it compares against is the spill
		 * at 0x42(%esp) -- read there at 0x6dfc2, not off the object.
		 */
		t3c_txblock(obj);			/* 0x6dfd5 */
		V34_CONT_RETURN();
	}

	if (obj->role != 0x65) {
		if (obj->retrain_state != 1) {
			/*
			 * 0x6dffb, another `jne` straight to the dispatch, on
			 * the `cmpw $0x1,0xa24a` at 0x6dff3 -- sixteen bits
			 * wide.
			 */
			t3c_txblock(obj);		/* 0x6dffb */
			V34_CONT_RETURN();
		}
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("DET_SYNC not detected, during "
					     "search for info1c initiating a " "retrain\n");
		/*
		 * 0x6e029.  The second argument is built at 0x6e01d as a
		 * literal 1, and 0x6e02e reloads +0x3596 after the call, so
		 * the dispatch runs on whatever `v34handshakinit` left there.
		 */
		v34handshakinit(obj, 1);		/* 0x6e029 */
		t3c_txblock(obj);
		V34_CONT_RETURN();
	}

	/* 0x6e03a, and +0x3588 gets bit 1 before any transition. */
	obj->short_3588 = (short)(obj->short_3588 | 2);
	hs_set_microstate(obj, V34HS_RX_PHASE1_CALL);
	hs_set_rxstate(obj, V34HS_RX_DPSK);
	hs_set_txstate(obj, V34HS_TONE_AB);

	obj->short_358c = 0;
	obj->vect_idx = 0;
	obj->short_aa78 = 0;
	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DET_SYNC not detected, during search " "for info1a\n");

	t3c_txblock(obj);
