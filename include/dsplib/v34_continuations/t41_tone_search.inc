
	struct v34_receiver *rx = &obj->receiver;
	short det;
	int i;

	rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_DET_PENDING);

	det = t41_detect(obj);
	if (det == 0 && obj->fsk.nbits <= 0x384) {
		/*
		 * 0x70f8f.  Both halves of the guard: `test %ax,%ax` at
		 * 0x70a1b and `cmpw $0x384,0xaae0` at 0x70a20.  The flag
		 * lowered at 0x709f8 stays lowered on this exit -- it is
		 * cleared before the detector runs, not after it answers.
		 */
		t3c_txblock(obj);			/* 0x70f8f */
		V34_CONT_RETURN();
	}

	obj->is_short = 0;
	obj->local_short = 0;
	obj->short_358a = 1;
	obj->short_3588 = (short)(obj->short_3588 | 1);

	for (i = 0; i <= 9; i++)
		obj->short_abae[i] = 0;

	obj->short_abc2 = 0;

	hs_set_txstate(obj, V34HS_TX_DPSK);
	hs_set_microstate(obj, V34HS_DET_SYNC);

	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	/*
	 * 0x70d35.  The longer warm-up is taken only when the far end's
	 * marker says 0x65 AND the V.90 receiver is present; the object reads
	 * that int through `obj + 4`, so it is +0x24c and not +0x248.
	 */
	if (obj->role == 0x65 && obj->v90_receiver != 0)
		/*
		 * ONE record with two heads.  0x70bd7 falls into the 0x11 head
		 * at 0x70bdd, and 0x70d41's `test`/`je` jumps BACK to it when
		 * the int loaded at 0x70d39 is zero, so 0x70d47 is reached
		 * only with both conditions -- and both heads end at 0x70c07.
		 */
		t41_record_head(obj->msgrec, 0x1e);		/* 0x70d47 */
	else
		t41_record_head(obj->msgrec, 0x11);		/* 0x70bdd */

	/*
	 * 0x70c07 is where the two heads MEET: it is the next instruction
	 * after the 0x11 arm's `movw $0x11,0x18` and it is the 0x1e arm's
	 * `jmp` target at 0x70d6c, so everything from here down is emitted
	 * once.
	 */
	obj->msgrec[0].wordbits = 8;				/* 0x70c07 */
	obj->msgrec[0].crc_on = 1;
	obj->msgrec[0].acc = 0xf72;
	obj->msgrec[0].acc0 = 0xf72;
	obj->msgrec[0].avail = 0xc;
	obj->msgrec[0].avail0 = 0xc;
	obj->msgrec[0].repeat = 1;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "errorrecovery is initialized in DET_SYNC\n");

	t3c_txblock(obj);
