
	struct v34_receiver *rx = &obj->receiver;
	short *rec = (short *)obj->paa70;

	hs_set_microstate(obj, V34HS_DET_INFO);

	*(short *)((char *)rec +
		    __builtin_offsetof(struct v34_bitsource, crc)) = -1;
	obj->fsk.nbits = 0;

	if (obj->short_358a == 0)
		rx->flags = (unsigned short)(rx->flags
					     & ~V34_RX_FLAG_DET_PENDING);

	obj->short_aa78 = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "DET_SYNC is received in RX_DPSK,rx->gain=0x%x\n",
		    (int)rx->agc_gain);

	t3c_txblock(obj);
