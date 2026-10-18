
	unsigned idx = (unsigned)((int)tx - T3M_TBL2_FIRST);

	if (idx >= T3M_TBL2_COUNT) {
		t3m_tail(f, tx);
		V34_CONT_RETURN();
	}

	switch ((int)tx) {
	case 5:				/* 0x64480 SILENCE */
		/*
		 * Three ways to reach progress 1, all of them a microstate
		 * and rxstate pair agreeing with the answer/originate flag
		 * at +0x359c, and every other way is 0.
		 */
		{
			unsigned short mst = T3M_U16(f, V34HS_MICROSTATE_OFF);
			unsigned short rx = T3M_U16(f, V34HS_RXSTATE_OFF);

			if (mst == 0x3f && f->obj->role == 0x66)
				*f->progress = 1;
			else if (rx == 0x04 && mst == 0x2c
				 && f->obj->role == 0x65)
				*f->progress = 1;
			else if (rx == 0x23 && mst == 0x3f
				 && f->obj->role == 0x65)
				*f->progress = 1;
			else
				*f->progress = 0;
		}
		break;

	case 18:			/* 0x64518 SSEG   */
	case 19:			/*          SBARSEG */
		/*
		 * `sbb`/`add $3` is 3 when bit 3 of the receiver's flags is
		 * set and 2 when it is not.  The originate side is 2 whatever
		 * the flag says.
		 */
		if (f->obj->role == 0x65)
			*f->progress = 2;
		else
			*f->progress = (f->rx->flags >> 3) & 1 ? 3 : 2;
		break;

	case 20:			/* 0x64509 PPSEG    */
	case 21:			/*         TRNSEG4  */
	case 64:			/*         JTXMIT   */
	case 68:			/*         J1TXMIT  */
		*f->progress = 2;
		break;

	case 24:			/* 0x644c9 TX_DPSK  */
	case 51:			/*         TX_L1    */
	case 54:			/*         SILENCEINFO */
	case 60:			/*         TONE_AB  */
	case 74:			/*         SILENCERETRAIN */
		*f->progress = 0;
		break;

	case 66:			/* 0x644fa TRNSEG4A */
	case 67:			/*         XMITMP   */
	case 69:			/*         EXMIT    */
		*f->progress = 3;
		break;

	case 70:			/* 0x644d8 DATAXMIT */
		/*
		 * `cmp $0x1,%bp` then `sbb %eax,%eax` and `add $0x4,%eax`:
		 * the borrow is set only when the halfword is zero.
		 */
		*f->progress = T3M_U16(f, T3M_F0E4C) == 0 ? 3 : 4;
		break;

	default:			/* 0x62a40, straight to the tail */
		break;
	}

	t3m_tail(f, tx);
