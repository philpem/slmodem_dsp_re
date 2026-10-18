
	struct v34_object *o = (struct v34_object *)objp;

	(void)tx1_ja_common(o, "V34Hshak: on JaTXMIT - time to freeze"
				   " echo...\r\n");		/* 0x6821d */
	v90Phase34(o);
	V34_TX_CASE_RETURN(V34TX1_LOOP);
