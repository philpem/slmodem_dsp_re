
	struct v34_object *o = (struct v34_object *)objp;

	(void)tx1_ja_common(o, "V34Hshak: on K56JaTXMIT - time to freeze"
				   " echo...\r\n");		/* 0x68282 */
	(void)k56FlexPhase34(o);
	V34_TX_CASE_RETURN(V34TX1_LOOP);
