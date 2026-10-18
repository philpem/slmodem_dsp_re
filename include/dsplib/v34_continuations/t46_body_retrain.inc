
	obj->paa6c = obj->msgrec;
	obj->paa70 = &obj->msgrec[1];

	V34SetINFO0aBits(obj, (short *)obj->msgrec);
	if (obj->role == 0x66)
		V34SetINFO0dBits(obj, (short *)obj->paa70);

	obj->short_3588 = (short)(obj->short_3588 | 4);
	t46_reset_core(obj);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Repeated info0 is initialized on " "retrain\n");

	obj->retrain_req = 0;
	t3c_txblock(obj);
