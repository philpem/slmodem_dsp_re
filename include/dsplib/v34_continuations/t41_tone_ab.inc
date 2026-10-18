
	struct v34_receiver *rx = &obj->receiver;
	const unsigned char *rec;
	short *out;
	int i;

	/*
	 * 0x6dac7's `test %ax,%ax` sends a silent detector to 0x6de83, which
	 * re-reads +0xabe8 with a `movzbl` before jumping to the junction --
	 * the re-read the note above says is why `abe8` is a parameter.
	 */
	if (t41_detect(obj) == 0) {
		t41_after_guards(obj, obj->moh_active); /* 0x6de83 */
		V34_CONT_RETURN();
	}

	rec = (const unsigned char *)obj->paa6c;
	if ((rec[4] & 0x80) == 0) {
		/*
		 * 0x6dae1's `testb $0x80,0x4(%eax)` on the record +0xaa6c
		 * points at, and 0x6de70 re-reads +0xabe8 exactly as 0x6de83
		 * does -- two stubs three instructions apart, one junction.
		 */
		t41_after_guards(obj, obj->moh_active); /* 0x6de70 */
		V34_CONT_RETURN();
	}

	obj->short_358a = 0;

	/*
	 * 0x6dafa is `js`, so a negative count copies nothing at all, and
	 * the loop itself is a do/while over 0..+0xabc2 INCLUSIVE -- the
	 * test at 0x6db1b compares the already-incremented index.
	 */
	if (obj->short_abc2 >= 0) {
		out = (short *)obj->paa70;
		for (i = 0; (short)i <= obj->short_abc2; i++)
			out[i] = obj->short_abae[i];
	}

	obj->short_aa78 = 0;

	if (obj->role == 0x65) {
		hs_set_microstate(obj, V34HS_RX_PHASE1_CALL);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Tone AB detected ending " "errorrecovery. Switch to "
					     "RX_PHASE1_CALL.\n");
	} else {
		obj->fsk.sr = -1;
		hs_set_microstate(obj, V34HS_TX_PHASE1_ANS);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Tone AB detected ending " "errorrecovery. Switch to "
					     "TX_PHASE1_ANS.\n");
	}

	/* 0x6dbf3, which both of the two above fall into. */
	rx->flags = (unsigned short)(rx->flags & ~V34_RX_FLAG_DET_PENDING);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V34INFO, info0 received in DET_SYNC\n");

	if (DSPLIB_DEBUG_ON()) {
		const unsigned short *p =
		    (const unsigned short *)obj->paa70;

		dsplibs_debug_printf(
		    "%s 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
		    "V34INFO, rxinfo0",
		    (unsigned)p[0], (unsigned)p[1], (unsigned)p[2],
		    (unsigned)p[3], (unsigned)p[4], (unsigned)p[5],
		    (unsigned)p[6], (unsigned)p[7], (unsigned)p[8],
		    (unsigned)p[9]);
	}

	t3c_txblock(obj);
