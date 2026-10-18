
	struct v34_object *o = (struct v34_object *)objp;

	o->cur_quadrant = tx1_scramble2(o);
	tx1_put_point(o, vect4[0]);
	txmit(o);
	V34_TX_CASE_RETURN(V34TX1_LOOP);
