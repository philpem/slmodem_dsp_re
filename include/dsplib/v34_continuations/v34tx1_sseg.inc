
	struct v34_object *o = (struct v34_object *)objp;
	unsigned short n;

	tx1_put_point(o, vect4[0]);
	txmit(o);
	tx1_put_point(o, vect4[3]);
	txmit(o);

	n = (unsigned short)((unsigned short)o->seg_symcount + 1);
	o->seg_symcount = (short)n;
	if (n == 0x40) {
		/* 0x66d11 */
		hs_set_txstate(o, V34HS_SBARSEG);
		o->seg_symcount = 0;
	}
	V34_TX_CASE_RETURN(V34TX1_LOOP);
