
	struct v34_object *o = (struct v34_object *)objp;
	int sel = o->short_358c & 1;

	tx1_put_point(o, vect4[2 * sel]);
	txmit(o);
	V34_TX_CASE_RETURN(V34TX1_LOOP);
