
	struct v34_receiver *rx = &obj->receiver;

	/*
	 * 0x650c6, and THE STORE PRECEDES THE BRANCH: `movzwl`, `inc`,
	 * `cmp $0x440,%di`, `mov %di,0xaa78`, `jle`.  So everything below
	 * sees the incremented value, which is what makes the counter
	 * pokeable in spite of finding F429 -- a poke of n drives the whole
	 * arm as n + 1.  The compare is signed and sixteen bits.
	 */
	obj->short_aa78 = (short)(obj->short_aa78 + 1);

	if (obj->short_aa78 <= 0x440) {
		t72_ladder(obj, rx);
		/*
		 * 0x65597.  The ladder's exit, one of the arm's seventeen
		 * `jmp 62af1`s: `mov 0xc0(%esp),%edi`, `movzwl
		 * 0x3596(%edi),%ecx`, jump.  The txstate is reloaded into
		 * %ecx first because the tail dispatches on whatever the
		 * jumping arm left there.
		 */
		t3c_txblock(obj);			/* 0x65597 */
		V34_CONT_RETURN();
	}

	/*
	 * 0x650ee.  The `jle 6551a` at 0x650e1 keeps this off the ladder
	 * path entirely, so the AGC runs only once the counter is past
	 * 0x440 -- which on the originating end is every block of the
	 * tone-A wait below, not one.  Its argument is `0x74(%esp)` --
	 * the receiver at obj + 0x264 -- pushed with no displacement.
	 */
	V34agc(rx);					/* 0x650ee */

	if (obj->role != 0x65) {
		/*
		 * 0x67d70 -- the ANSWERING end.  It finishes the probe, arms
		 * the FSK receiver for INFO1, aims the second record pointer
		 * at obj + 0xa9dc with INFO1c's length in it, opens the gate
		 * at +0xa8a0 that diverts 0x64a87, and restarts the counter.
		 */
		t72_probe_done(obj, rx);

		/*
		 * 0x67e1e.  `movzwl 0x3594(%ecx),%edx; cmp $0x2b,%dx; je
		 * 67e4b` -- and the `je` can never take, because the arm is
		 * entered with the rxstate at 72 and nothing between 0x650c6
		 * and here writes +0x3594.  The store and its trace always
		 * run.
		 */
		hs_set_rxstate(obj, V34HS_RX_DPSK);	 /* 0x67e1e */
		/*
		 * 0x67e4b.  `cmp $0x3c,%dx`, and it is NOT one of the three
		 * the head of this section calls unreachable: those three
		 * compare +0x3594 and this one compares +0x3596.  An end
		 * already in TONE_AB stores nothing and prints nothing.
		 */
		hs_set_txstate(obj, V34HS_TONE_AB);	 /* 0x67e4b */
		/*
		 * 0x67e7f.  `cmp $0x29,%dx` on +0x3592: microstate 41, whose
		 * arm at 0x669a4 is the table-3 case at the bottom of this
		 * file.  This store is what sends the next block there.
		 */
		hs_set_microstate(obj, V34HS_DET_SYNC); /* 0x67e7f */

		/*
		 * 0x67eb3.  Inlined, and it runs to 0x67f4d: `mov
		 * %bx,0xaae6(%esi,%eax,2)` at 0x67ebe is the head of the
		 * hundred-halfword clear, and the two stores at 0x67f46 and
		 * 0x67f4d -- +0xaae2 to all ones, +0xaae0 to zero -- are the
		 * last two of `fsk_state_init`'s eleven, not statements of
		 * this arm.
		 */
		dpskDetectInfo1Init(obj);			 /* 0x67eb3 */

		/*
		 * 0x67f54.  `movw $0x4d,0x18(%ebp)`, and %ebp already holds
		 * obj + 0xa9dc: the length field is filled in BEFORE 0x67f6e
		 * publishes the pointer to the record.  Microstate 41 puts
		 * INFO1a's 0x26 in the same field of the same record at
		 * 0x6b379 before aiming the same pointer at it -- which
		 * sequence the end expects is the whole difference.
		 */
		obj->msgrec[3].nbits = 0x4d;		 /* 0x67f54 */
		obj->vect_idx = 0;				 /* 0x67f5a */
		/*
		 * 0x67f68.  `mov %ebx,0xa8a0(%esi)`, thirty-two bits.  This
		 * is the gate 0x64a87 tests, so from the next block on the
		 * RX_DPSK path polls the retrain detector as well -- and
		 * falls through to the FSK demodulator either way, which is
		 * finding F721.
		 */
		obj->retrain_poll = 1;				 /* 0x67f68 */
		obj->paa70 = &obj->msgrec[3];			 /* 0x67f6e */
		/*
		 * 0x67f74.  `mov %di,0xaa78(%esi)`, a halfword.  +0xaa78 is
		 * not this arm's private counter -- six arms bump it and it
		 * is the `[2]` every state trace prints -- so leaving it at
		 * 0x441 on the way out would be visible everywhere.
		 */
		obj->short_aa78 = 0;					 /* 0x67f74 */

		/*
		 * 0x67f7b.  The exit, and the %ecx it dispatches on was
		 * loaded from +0x3596 at 0x67f61 -- scheduled into the middle
		 * of the store run, three stores before the jump.
		 */
		t3c_txblock(obj);				 /* 0x67f7b */
		V34_CONT_RETURN();
	}

	if (obj->short_aa78 == 0x441) {
		/*
		 * 0x69611 -- the ORIGINATING end, on the one block after the
		 * probe.  Same finish, but it stays in RX_L1 and only clears
		 * the FSK shift register, because what it is waiting for is
		 * tone A and that is the test below.
		 */
		t72_probe_done(obj, rx);

		/*
		 * 0x696d0.  `cmp $0x3c,%cx; je 69702`: when the txstate is
		 * already 60 the store at 0x696f6 and the trace are both
		 * skipped -- and both paths leave 0x3c in %ecx, which is what
		 * the exit at 0x6971e dispatches on.
		 */
		hs_set_txstate(obj, V34HS_TONE_AB);	/* 0x696d0 */

		/*
		 * 0x69710.  `mov $0xffffffff,%ebp` and then a halfword store,
		 * so +0xaae2 gets 0xffff.  It is the shift register's reset
		 * value and this arm writes it at three sites -- 0x67f46,
		 * 0x68851 and here.
		 */
		obj->fsk.sr = -1;				/* 0x69710 */
		obj->fsk.nbits = 0;				/* 0x69717 */

		/* 0x6971e, and %ecx still holds TONE_AB from 0x696d0. */
		t3c_txblock(obj);				/* 0x6971e */
		V34_CONT_RETURN();
	}

	/*
	 * 0x65140, and the two guards below read what it writes:
	 * `cmpw $0x14,0xaae0` at 0x65145 and `testw $0xfff,0xaae2` at
	 * 0x65153, both sixteen bits, both on fields this call has
	 * just filled in.  No fixture can choose them by poking.
	 */
	fskdemodulate(obj,					/* 0x65140 */
		      rx->burst,
		      &obj->fsk);

	if (obj->fsk.nbits <= 0x14 || (obj->fsk.sr & 0xfff) != 0) {
		/*
		 * 0x6881e.  Not enough bits, or the twelve are not all ZERO:
		 * no tone A this block, so all that is left is the timeout
		 * ladder.  The polarity is the object's and is worth stating,
		 * because this comment used to have it backwards: 0x65153 is
		 * `testw $0xfff,0xaae2(%esi)` with `jne 6881e`, so it is a
		 * NON-zero masked register that fails.  The register is reset
		 * to all ones at all three sites above, so twelve ZEROS
		 * shifting in is what tone A looks like here.  See the head of this section for why the 32-bit
		 * arithmetic here cannot be separated from a 16-bit spelling
		 * by any trial, and what is tested instead.
		 */
		int limit = obj->rtd >> 2;
		int count = obj->short_aa78;

		if (count > 0xf10 + limit) {
			/*
			 * 0x68851, AND THE STORE PRECEDES THE BRANCH: it
			 * sits between `cmp %ebx,%ecx` at 0x6884f and `je
			 * 6afd7` at 0x68858, so every block past 0xf10 +
			 * rtd/4 clears the shift register, not only the one
			 * that matches 0xf14.
			 */
			obj->fsk.sr = -1;		/* 0x68851 */

			if (count == 0xf14 + limit) {
				/*
				 * 0x6afe8 sits between the debug
				 * test at 0x6afe1 and its `ja
				 * 6b120` at 0x6afef -- the toggle
				 * happens whether or not the
				 * message prints.
				 */
				obj->short_358c ^= 1;	/* 0x6afe8 */
				/*
				 * 0x6b120 is out of line and does
				 * not come back: it prints,
				 * reloads +0x3596 into %ecx and
				 * jumps to 0x62af1 itself.
				 */
				if (DSPLIB_DEBUG_ON())	/* 0x6b120 */
					dsplibs_debug_printf(
					    "V34RETRAIN, waiting for tone A " "at the end of RX_L2\n");
			} else if (count == 0xf2c + limit) {
				/* 0x6c9f1: give up and go round again. */
				/*
				 * 0x6c9fa zeroes the counter
				 * before either transition below,
				 * and 0x6ca01's `cmp $0x2b` is one
				 * of the three that can never
				 * match.
				 */
				obj->short_aa78 = 0;		/* 0x6c9fa */
				hs_set_rxstate(obj, V34HS_RX_DPSK);
				hs_set_microstate(obj, V34HS_TX_L1);
			}
		}

		t3c_txblock(obj);
		V34_CONT_RETURN();
	}

	/*
	 * 0x65162.  Tone A: hand the receive machine to phase 2's DPSK and
	 * the microstate to the caller's phase-3 wait.
	 */
	/*
	 * 0x65162.  `cmp $0x2b,%dx` on +0x3594, and the `je 651e9`
	 * can never take -- the arm is entered with the rxstate at 72
	 * and nothing on the way here writes it -- so the store and
	 * its trace always run.
	 */
	hs_set_rxstate(obj, V34HS_RX_DPSK);		/* 0x65162 */
	/*
	 * 0x651e9.  `cmp $0x3e,%dx` on +0x3592 -- 62 -- and it is
	 * also 0x6516d's `je` target, so the microstate transition is
	 * where the rxstate transition's "already there" exit lands.
	 */
	hs_set_microstate(obj, V34HS_RX_PHASE3_CALL);	/* 0x651e9 */

	/*
	 * 0x65289.  The exit, and its %ecx was loaded from +0x3596 at
	 * 0x6527b -- after the trace call at 0x6526a and one
	 * instruction before the microstate store at 0x65282.
	 */
	t3c_txblock(obj);					/* 0x65289 */
