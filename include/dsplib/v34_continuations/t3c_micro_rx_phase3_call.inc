
	struct v34_receiver *rx = &obj->receiver;

	if ((t3c_getb(obj, T3C_FAAE2) & 1) == 0) {
		/*
		 * 0x6abae.  Bit 0 clear -- `testb $0x1,0xaae2` at 0x65c81 --
		 * and the counter at +0xaa78 keeps whatever it had: the copy
		 * from +0xaa7c is on the other side of that branch, at 0x65c8e
		 * and 0x65c99.
		 */
		t3c_txblock(obj);			/* 0x6abae */
		V34_CONT_RETURN();
	}

	/* Sixteen bits, and the trace below reads it back as [2]. */
	obj->short_aa78 = obj->filtdelay;

	hs_set_microstate(obj, V34HS_TX_PHASE2_CALL);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34PHASE2, RX_PHASE3_CALL ,filtdelay = "
				     "%d, rxflgs= 0x%x,rx->gain=0x%x\n",
				     (int)obj->filtdelay,
				     (unsigned)rx->flags,
				     (unsigned)(int)rx->agc_gain);

	rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DET_PENDING);
	t3c_txblock(obj);
