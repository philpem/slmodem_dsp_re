
	unsigned short aae2 = (unsigned short)obj->fsk.sr;
	unsigned char abe8;
	short short_358a;

	obj->short_aa7a = 0;

	if ((unsigned char)aae2 == 0x72) {
		/*
		 * 0x6c862 opens with `cmp $0x2c,%si` -- the setters'
		 * folded guard on DET_INFO, and one the dispatch that read
		 * 41 can never satisfy (finding F392).
		 */
		t41_to_det_info(obj);			/* 0x6c862 */
		V34_CONT_RETURN();
	}

	abe8 = obj->moh_active;
	short_358a = obj->short_358a;

	if (abe8 == 0 && short_358a == 0) {
		if (obj->fsk.nbits > 0x64) {
			/*
			 * 0x709e1 clears bit 9 of the receiver's
			 * +0x122 unconditionally (`and $0xfffffdff`
			 * at 0x709f2), where the DET_INFO body only
			 * does it when +0x358a is clear (finding F390).
			 */
			t41_tone_search(obj);		/* 0x709e1 */
			V34_CONT_RETURN();
		}
		/*
		 * 0x669fa.  A hundred blocks have not passed, so nothing
		 * happens this call beyond the entry's +0xaa7a clear; the
		 * tail loads +0x3596 into %ecx and jumps to 0x62af1.
		 */
		t3c_txblock(obj);			/* 0x669fa */
		V34_CONT_RETURN();
	}

	/* 0x6ab21, which the two doors above arrive at with the same two. */
	/*
	 * 0x6ce8b and 0x6da9c, the junction's two bodies.  The first reads
	 * the delay at +0xaa7e and spills it to 0x42(%esp) at 0x6cead, so
	 * every threshold below sees the value the first one saw; the
	 * second loads the receiver and aims the detector at obj+0x3564
	 * (0x6dab1) before anything else.
	 */
	if (short_358a == 2) {
		t41_info_marks(obj, aae2, abe8);	/* 0x6ce8b */
		V34_CONT_RETURN();
	}
	if (short_358a == 1) {
		t41_tone_ab(obj);			/* 0x6da9c */
		V34_CONT_RETURN();
	}
	t41_after_guards(obj, abe8);
