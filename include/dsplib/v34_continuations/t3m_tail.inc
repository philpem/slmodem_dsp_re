
	int mode = f->obj->hs_mode;
	int esi;

	if (mode == 1) {
		/*
		 * The block at 0x62b45: `ratecfg.rx_baud` is the receive baud rate
		 * and the receiver's +0x1d2 gets three times it, by
		 * `lea (%ebp,%ebp,2)` on the sign-extended halfword, stored
		 * back as one.  THE RELOAD IS THE NEXT INSTRUCTION, 0x62b5f
		 * `movzwl 0x3596(%ebx),%ecx`, and 0x62b66 writes the 4.
		 */
		f->rx->report_interval = (short)(3 * (int)f->obj->ratecfg.rx_baud);
		tx = (short)T3M_U16(f, V34HS_TXSTATE_OFF);
		*f->progress = 4;
	}

	/* 0x62a56 and 0x62a68: both windows are UNSIGNED, so mode 1 and any
	   negative mode fall through both. */
	if ((unsigned)(mode - 4) <= 1u)
		*f->progress = 6;

	if ((unsigned)(mode - 2) <= 1u && tx == 0x4a) {
		/*
		 * 0x64884.  `setg`/`dec`/`and $7` is 0 when `vect_idx` is
		 * above 60 and 7 when it is not -- the arithmetic is the
		 * compiler's way of writing a two-valued select and the 7 is
		 * not a mask of anything.
		 */
		*f->progress = f->obj->vect_idx > 0x3c ? 0 : 7;
	}

	/* 0x62a7a.  Unsigned, so an elapsed count past the deadline's wrap
	   is late rather than early. */
	if (T3M_U32(f, T3M_ELAPSED) > T3M_U32(f, T3M_DEADLINE))
		*f->progress = 8;

	/* 0x62a92.  Signed: the level is a sign-extended halfword. */
	esi = f->rx->agc_level;
	if (esi >= f->obj->rx_energy_floor)
		T3M_I32(f, T3M_TICK) = 0;
	else
		T3M_I32(f, T3M_TICK)++;

	if (T3M_I32(f, T3M_TICK) > 0x257f)
		*f->progress = 9;

	/* 0x62ac5, a three-way compare on the SIGN-EXTENDED halfword. */
	if (tx == 0x53)
		*f->progress = 0x0f;
	else if (tx > 0x53) {
		if (tx == 0x54)
			*f->progress = 0x10;
	} else if (tx == 0x52)
		*f->progress = 0x0d;
