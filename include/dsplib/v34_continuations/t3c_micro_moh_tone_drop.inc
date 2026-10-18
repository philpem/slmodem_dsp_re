
	if (t3c_moh_step_detector(obj) != 0
	    && obj->moh_msg_pending == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34Handshake: Detected signal "
					     "drop on FRR request, time to " "move to phase1...\r\n");
		obj->moh_msg_pending = 1;
	}

	/* The round-trip delay sets how long the drop has to persist. */
	if ((int)obj->short_aa78 < (((int)obj->rtd) >> 2) + 0x12c0) {
		/*
		 * 0x6aae6.  Both halfwords are sign-extended before the
		 * arithmetic -- `movswl 0xaa7e`, `movswl 0xaa78`, `sar $0x2`,
		 * `add $0x12c0` -- so a negative delay shortens the wait
		 * rather than wrapping it.
		 */
		t3c_txblock(obj);			/* 0x6aae6 */
		V34_CONT_RETURN();
	}

	/*
	 * 0x6c8f8.  THE GUARD IS A BYTE (`cmpb $0x0,0xabf9`) and it is not
	 * symmetrical with the one microstate 79 takes four fields away: this
	 * one picks between giving up and retraining, and the fall-through --
	 * which is the retrain -- is the case the byte being zero selects.
	 *
	 * GIVING UP IS FOUR STORES AND NO CALL.  The transmit machine goes to
	 * MOH_CLEARDOWN and the receive machine to WAIT, +0xabe4 and +0xabe2
	 * are both raised, and the arm leaves through the once-per-block
	 * dispatch like every other path here -- no `v34handshakinit`, so
	 * findings F359 and F324 do not apply to it and the library tables stay
	 * where the bring-up put them.  Both state compares are LIVE, unlike
	 * 81's: nothing on the way in constrains either word.
	 */
	if (obj->moh_path_sel != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("MOH: Timeout waiting for MH " "sequence under MHfrr, "
					     "disconnecting...\r\n");

		hs_set_txstate(obj, V34HS_MOH_CLEARDOWN);
		hs_set_rxstate(obj, V34HS_WAIT);
		obj->short_abe4 = 1;		/* 0x6c983 */
		/*
		 * 0x6c983 and 0x6c98a are two halfword stores of the same
		 * constant, out of %ax and %di loaded at 0x6c979 and 0x6c97e
		 * -- so both fields are shorts, where the two guards that
		 * chose this path read bytes.
		 */
		obj->short_abe2 = 1;				/* 0x6c98a */

		/*
		 * 0x62af1 IS the dispatch entry and not one of the stubs that
		 * re-read +0x3596: 0x6c991 jumps straight in, because the
		 * transition at 0x6c8f8 already left MOH_CLEARDOWN in %cx,
		 * which the four stores between 0x6c93e and 0x6c991 do not
		 * disturb.
		 */
		t3c_txblock(obj);			/* 0x62af1 */
		V34_CONT_RETURN();
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("MOH: Timeout waiting for MH sequence "
				     "under MHfrr, initiating retrain\r\n");

	v34handshakinit(obj, 1);
	obj->short_abe6 = 1;

	t3c_txblock(obj);
