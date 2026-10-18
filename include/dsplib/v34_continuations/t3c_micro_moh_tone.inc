
	if (t3c_moh_step_detector(obj) == 0) {
		/*
		 * 0x6afc4.  The detector said nothing -- `test %ax,%ax` at
		 * 0x65806 -- and the counter step at 0x657ea/0x657eb stands,
		 * so the count below resumes from it on the next block.
		 */
		t3c_txblock(obj);			/* 0x6afc4 */
		V34_CONT_RETURN();
	}

	if (obj->short_aa78 < obj->short_abfc) {
		/*
		 * 0x6c701.  `cmp 0xabfc(%esi),%ax` at 0x6581d and a SIGNED
		 * `jl`: the count and the limit are both halfwords and both
		 * read off the object by adjacent instructions.
		 */
		t3c_txblock(obj);			/* 0x6c701 */
		V34_CONT_RETURN();
	}

	hs_set_txstate(obj, V34HS_TX_DPSK);

	/*
	 * 0x6d57c.  THE GUARD IS 32 BITS WIDE (`cmpl $0x1,0xabf0`) and the
	 * two sides of it choose a different NEXT MICROSTATE, not a different
	 * amount of work: MHfrr re-aims the tone detector and waits for the
	 * far end's carrier to DROP, anything else goes straight to DET_SYNC.
	 * Both then run the block below, which 0x6d57c's own exit at 0x6589f
	 * joins past the DET_SYNC store.
	 *
	 * AND THIS IS 0x6ebdf'S CALL WITH ONE CONSTANT CHANGED.  Microstate
	 * 44's Modem-on-Hold accept arm builds the same detector -- same
	 * coefficients, chosen the same way by `role`, same polarity, same
	 * warm-up, same thresholds, and the same +0x356a store and
	 * MOH_TONE_DROP after it.  The one difference is `limit`, 0xf0 here
	 * against 0x64 there, so this end waits about two and a half times as
	 * long before asserting.  Finding F749.
	 */
	if (obj->moh_message == 1) {
		detectorinit(&obj->detector,
			     obj->role == 0x65 ? c2400_ : c1200_,
			     1, 0xf0, 0x32, 0x800, 0x400);
		/*
		 * 0x6d5db sits BETWEEN the microstate load at 0x6d5d4 and the
		 * `cmp $0x50` at 0x6d5e2, so +0x356a takes its 1 whether or
		 * not the transition below turns out to be a move.
		 */
		obj->detector.armed = 1;		/* 0x6d5db */
		hs_set_microstate(obj, V34HS_MOH_TONE_DROP);
	} else {
		hs_set_microstate(obj, V34HS_DET_SYNC);
	}

	obj->fsk.phase = 0;
	obj->fsk.nbits = 0;
	obj->msgrec[1].crc = -1;
	obj->msgrec[1].nbits = 8;
	obj->short_aa78 = 0;
	obj->paa70 = &obj->msgrec[1];
	obj->paa6c = obj->msgrec;
	obj->short_358c = 0;
	obj->vect_idx = 0;

	t3c_txblock(obj);
