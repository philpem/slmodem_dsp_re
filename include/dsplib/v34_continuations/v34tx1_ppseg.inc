
	struct v34_object *o = (struct v34_object *)objp;
	int i = o->vect_idx;			/* movswl 0x2aa2 */
	int point;

	memcpy(&point, &vectpp[2 * i], sizeof(point));
	o->vect_idx = (short)(i + 1);
	tx1_put_point(o, point);
	txmit(o);

	if ((unsigned short)o->vect_idx == (unsigned short)V34_VECTPP_POINTS) {
		/* 0x66d57 */
		unsigned short n = (unsigned short)
			((unsigned short)o->short_358e + 1);

		if (n != 6) {
			o->short_358e = (short)n;
			o->vect_idx = 0;
		} else {
			/* 0x680ac */
			struct v34_ratecfg *cfg = &o->ratecfg;
			short v = (short)((unsigned short)o->rtd + 0x90);
			short baud = cfg->baud;
			int span, q, acc;

			o->vect_idx = v;
			switch ((unsigned short)baud) {
			case 2400:
				o->ratecfg.period = (short)(v >> 2);
				break;
			case 2800:
				o->ratecfg.period =
					(short)(((int)v * 0x12ab) >> 14);
				break;
			case 3000:
				o->ratecfg.period =
					(short)(((int)v * 0x1400) >> 14);
				break;
			case 3200:
				o->ratecfg.period =
					(short)(((int)v * 0x1555) >> 14);
				break;
			case 3429:
				o->ratecfg.period =
					(short)(((int)v * 0x16dc) >> 14);
				break;
			default:
				break;
			}

			/* 0x680fd */
			o->short_358e = 0;
			o->ratecfg.period =
				(short)((unsigned short)o->ratecfg.period
					+ 0x120);
			hs_set_txstate(o, V34HS_TRNSEG4);

			/* 0x68154 */
			span = (0x5e8 - o->dmadelay) * (int)baud;
			q = span / 9600;
			o->short_aa78 = (short)q;	/* 0x6818d */

			acc = q + (unsigned short)o->filtdelay
				+ (unsigned short)o->seg_symcount + 1;
			if (o->role == 0x65)			/* 0x6926d */
				acc += (unsigned short)o->rtd;
			o->short_aa78 = (short)acc;

			/* 0x681c6 */
			o->ratecfg.period =
				(short)((unsigned short)o->ratecfg.period
					+ (unsigned short)o->short_aa78);
			if (dsplibs_debug_level > 1)	/* 0x681ef */
				dsplibs_debug_printf(
					"V34Hshak: echo start wait time would" " be: NEC %d symbols, FEC %d"
					" symbols...\r\n",
					o->short_aa78,
					o->ratecfg.period);
			V34_TX_CASE_RETURN(V34TX1_LOOP);		/* 0x63da2 */
		}
	}

	/* 0x6430c */
	o->seg_symcount = (short)((unsigned short)o->seg_symcount + 1);
	V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x6431f */
