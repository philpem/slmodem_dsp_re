
	struct v34_object *o = (struct v34_object *)objp;
	struct v34_receiver *rx = &o->receiver;
	int idx;

	modulatevector(o);

	if (!(o->tx_flags & PROG_TXBIT_DATA))
		V34_TX_CASE_RETURN(V34TX1_LOOP);

	rx->equerr_accum = 0;
	rx->err_symcount = 0;
	o->int_0248 = o->sample_count;

	idx = o->ratecfg.rxbits;
	o->rate_now = idx;
	o->rate_want = idx;

	/*
	 * 0x63d11.  The data-mode transition reports both negotiated rates ONCE,
	 * and only when the machine was not already in data mode: +0x2218 is read
	 * at 0x63cf4 and a non-zero value jumps over the whole block (0x63d0f).
	 * The guard is therefore the mode test AHEAD of the debug gate, which is
	 * why the `test` precedes the `cmpl $0x1` and not the other way round.
	 *
	 * Both arguments are the rate INDEX times 2400 (0x960, the V.34 symbol
	 * rate at the host rate), the transmit index first: 0x63d1a's `imul` is
	 * on 0xaa88 and its sibling on 0xaa98, and 0xaa88 lands in the first
	 * vararg slot.  Finding F11444's residual, closed here.
	 */
	if (o->hs_mode == 0 && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"V34DATA, getting into data mode from Handshake, "
			"Tx bit rate - %d, Rx bit Rate - %d\n",
			o->ratecfg.txbits * 2400, idx * 2400);

	o->hs_mode = 1;
	V34_TX_CASE_RETURN(V34TX1_LOOP);
