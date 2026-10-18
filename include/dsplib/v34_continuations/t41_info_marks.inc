
	short rtd = obj->rtd;
	short aae0 = obj->fsk.nbits;
	int base = ((int)rtd) >> 4;
	short next;
	short tx;

	if ((int)aae0 <= base + 100) {
		/*
		 * 0x6ceba's `jle` goes straight to the junction at 0x6ab33
		 * with no stub, and 0x6cead has already spilled the delay to
		 * 0x42(%esp) -- the copy `t41_marks_late` reads at 0x6dfc2.
		 */
		t41_after_guards(obj, abe8);		/* 0x6ceba */
		V34_CONT_RETURN();
	}

	/*
	 * 0x6ceca.  The reset is BEFORE the sentinel step at 0x6cef0, so
	 * below the second threshold +0x35a0 never climbs: it is zero or
	 * one whatever the sentinel does (finding F391).
	 */
	if ((int)aae0 <= base + 400)
		obj->short_35a0 = 0;			/* 0x6ceca */

	next = 0;
	if (aae2 == 0 || aae2 == 0xffff)
		next = (short)((unsigned short)obj->short_35a0 + 1);

	tx = obj->txstate;
	/*
	 * 0x6da8e, out of line: `cmp $0x3c,%cx` at 0x6cf0f -- TONE_AB --
	 * jumps here, zeroes +0x35a0 and returns to 0x6cf27 past the store,
	 * so transmitting the tone discards the stepped value.
	 */
	if (tx == V34HS_TONE_AB)
		obj->short_35a0 = 0;			/* 0x6da8e */
	else
		obj->short_35a0 = next;

	/*
	 * 0x6cf39.  The base is rebuilt here from the delay spilled at
	 * 0x6cead -- `movswl 0x42(%esp)` then `sar $0x4` at 0x6cf27 -- and
	 * `jl 62af1` is the once-per-block transmit dispatch (finding F390).
	 */
	if ((int)aae0 < base + 400) {
		t3c_txblock(obj);			/* 0x6cf39 */
		V34_CONT_RETURN();
	}

	/*
	 * 0x6cf4e tests a +0x35a0 RE-READ at 0x6cf46, not the value the
	 * step above stored: the TONE_AB arm at 0x6da8e wrote zero there
	 * and rejoined below the store, so only memory has the answer.
	 */
	if (obj->short_35a0 <= 0x31) {
		t41_marks_late(obj, aae0);		/* 0x6cf4e */
		V34_CONT_RETURN();
	}

	if (aae2 == 0xffff) {
		/*
		 * 0x6d2f8.  The record at +0xa9ac is configured and +0xaa6c
		 * aimed at it -- an INTERIOR pointer, which the fixture
		 * compares by offset from each side's own base (finding F359).
		 */
		hs_set_txstate(obj, V34HS_TX_DPSK);

		t41_record_head(&obj->msgrec[2], 0x4d);
		obj->msgrec[2].wordbits = 8;
		obj->msgrec[2].crc_on = 1;
		obj->msgrec[2].avail = 0x10;
		obj->msgrec[2].avail0 = 0x10;
		obj->msgrec[2].repeat = 0;
		obj->paa6c = &obj->msgrec[2];
		obj->msgrec[2].acc = 0xff72;
		obj->msgrec[2].acc0 = 0xff72;
		obj->short_358c = 0;

		hs_set_microstate(obj, V34HS_INFODONE);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34PHASE2,detected INFOMARKS, "
					     "reinitializing Info1c\n");

		t3c_txblock(obj);
		V34_CONT_RETURN();
	}

	if (aae2 != 0) {
		/*
		 * 0x6cf61.  Finding F391's third answer: -1 took 0x6d2f8 at
		 * 0x6cf58 and zero falls through to 0x6cf67, so every other
		 * +0xaae2 lands in the late half.
		 */
		t41_marks_late(obj, aae0);		/* 0x6cf61 */
		V34_CONT_RETURN();
	}

	/* 0x6cf67, +0xaae2 exactly zero -- finding F391's second answer. */
	obj->short_3588 = (short)(obj->short_3588 | 2);
	hs_set_txstate(obj, V34HS_SILENCERETRAIN);
	hs_set_rxstate(obj, V34HS_WAIT);

	obj->short_aa78 = 0;
	obj->vect_idx = 0;
	obj->short_358c = 0;
	obj->fsk.sr = -1;
	obj->fsk.nbits = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("DET_SYNC not detected, but Tone is "
				     "detected during info1\n");

	t3c_txblock(obj);
