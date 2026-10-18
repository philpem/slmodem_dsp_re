
	struct v34_object *o = (struct v34_object *)objp;
	short quiet[4];

	quiet[0] = 0;
	quiet[1] = 0;
	quiet[2] = 0;
	quiet[3] = 0;
	txwritequeue(&o->txq, quiet);

	o->vect_idx = (short)(o->vect_idx + 1);
	if ((unsigned short)o->vect_idx != 0xc0u)
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x63da2 */

	/* 0x66d85 */
	if (o->txstate != V34HS_MOH_SILENCE)
		V34_TX_CASE_RETURN(V34TX1_LOOP);			/* 0x63941 */

	if (dsplibs_debug_level > 1)			/* 0x66d98 */
		dsplibs_debug_printf(
			"V34F MOH: After 192 silence, org = %d ," " act = %d\r\n",
			o->moh_org, o->moh_message);

	/* 0x66dac */
	hs_set_txstate(o,
		    (o->moh_org == 1 || o->moh_message == 1)
		    ? V34HS_MOH_FRR : V34HS_MOH_ON_HOLD);

	o->vect_idx = 0;				/* 0x66dec */
	V34_TX_CASE_RETURN(V34TX1_LOOP);				/* 0x63948 */
