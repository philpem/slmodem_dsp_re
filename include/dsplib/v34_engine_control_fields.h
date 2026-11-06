/* Identical prefix fields for the named owner and GNU flat compatibility view.
 * Offsets in comments are complete v34_object offsets, not prefix offsets.
 * This file intentionally has no include guard. */

	/*
	 * An int the shell polls: `VPcmV34InitiateHangUp`,
	 * `VPcmV34InitiateRateRenegotiation` and `VPcmV34SetV90RateReneg` all
	 * put 6 in it, `datapumpv34` puts 5 when the sample count at +0x238
	 * has run 287,488 past the mark at +0x248, and `VPcmV34Create` puts
	 * 10 and then 0. Nothing reconstructed reads it, so the values above
	 * are all this says.
	 */
	int progress;					/* +0x0004 */
	/*
	 * +0x0008 and +0x0010, both named by initdigital's own debug string:
	 * "for tx data rate - %d, PTC - %d, setting nofTxBits to %d".  `ptc`
	 * is read and never written here; `nof_tx_bits` is
	 * `((txbits * ptc) >> 6) + 6`, or zero when the rate is zero.
	 */
	int ptc;					/* +0x0008 */
	unsigned char unmapped_000c[0x010 - 0x00c];
	int nof_tx_bits;				/* +0x0010 */
	/*
	 * The datapump's two data buffers. The layout tiles exactly --
	 * 0x14 + 64*4 is precisely 0x114 where the receive count lives, and
	 * 0x118 + 64*4 precisely 0x218 where the transmit one does, so each
	 * array ends where its counter begins.
	 *
	 * descrambleGP* appends each sixteen recovered bits to `rx_data` and
	 * stops at 64 entries; scrambleGP* takes its next sixteen out of
	 * `tx_data` and, once `tx_rd` reaches `tx_n`, scrambles 0xffff
	 * instead -- the all-ones idle V.34 sends with nothing to carry.
	 * Both paths are gated on `data_enable` at +0x2214. The arrays are
	 * `int`, and only the low short of each transmit entry is read.
	 */
	int rx_data[64];				/* +0x0014 */
	int rx_n;					/* +0x0114 */
	int tx_data[64];				/* +0x0118 */
	int tx_n;					/* +0x0218 */
	int tx_rd;					/* +0x021c */
	/*
	 * The rate renegotiation's four words, all indices rather than bit
	 * rates -- `VPcmV34GetCurrentRxBitRate` multiplies the same units by
	 * 2400 to answer in bits per second. Each name is read off a site,
	 * not inferred from the group:
	 *
	 *   min, max   `VPcmV34SetMinMaxBitRates` divides a pair of rates by
	 *              2400 into these two, caps both at 14, and then raises
	 *              `rate_max` to `rate_min` if it is below it. Which way
	 *              round that fix-up goes is settled by
	 *              `VPcmV34InitiateRateRenegotiation`, whose step down
	 *              clamps at `rate_min` and whose step up at `rate_max`.
	 *
	 *   now        `v34handshak` stores the negotiated rate index -- a
	 *              copy of +0xaa98, the field the getter multiplies --
	 *              into `rate_now` and `rate_want` together.
	 *
	 *   want       and elsewhere reads `rate_want` back, skips on a
	 *              negative value and then on equality with `rate_now`,
	 *              and otherwise adopts it as the field at +0xaa98. So
	 *              this is a request, and negative means "no target":
	 *              `VPcmV34InitiateRateRenegotiation` writes -1 for a
	 *              renegotiation that names no rate, and
	 *              `VPcmV34InitiateHangUp` clears the request and both
	 *              bounds but leaves `rate_now` alone.
	 */
	int rate_min;					/* +0x0220 */
	int rate_max;					/* +0x0224 */
	int rate_now;					/* +0x0228 */
	int rate_want;					/* +0x022c */
	/*
	 * The signal-energy floor `receiver` compares its 36-sample RMS
	 * against before declaring the line dead.  An int, and the only field
	 * either V.34 core reads through `obj + 4` rather than `obj`.
	 */
	int rx_energy_floor;				/* +0x0230 */
	/*
	 * +0x0234 to +0x024b is the sample-clock group: six ints, every one
	 * of them reached through `obj + 4` and spelled by displacement in
	 * the object (finding F179), and all six are modelled now (issue
	 * #260 wave 9).  The three the wave-9 recovery added:
	 *
	 * `low_energy_count` (+0x0234).  The tail of `v34handshak` runs the
	 * disconnect ladder on it: when `agc_level` is at or above
	 * `rx_energy_floor` it is zeroed, otherwise incremented, and 9600
	 * (the compare is against 0x257f) consecutive low samples -- one
	 * second at the 9600 Hz host rate -- put 9 in `progress`.  The
	 * C++ side zeroes it at each state transition that restarts
	 * listening (`VPcmV34InitiateRetrain` and three siblings).  Named
	 * by usage inference from that ladder; nothing in the object
	 * prints it.
	 *
	 * `p2_delay_cntr` (+0x0240).  Named by the object's own diagnostic:
	 * "On PHASE2_COMPLETE: added Silence = %d, p2DelayCntr = %d" prints
	 * the value VPcmV34Progress accumulates into it, and `v34handshak`'s
	 * microstate 63 tests the same field against the same 0x240 the
	 * C++ ladder waits past before phase 3 may start.
	 *
	 * `int_0244` (+0x0244) and `int_0248` (+0x0248) are offset-named on
	 * purpose.  `int_0244` is a copy of `sample_count` taken by
	 * `v34handshakinit`'s opening guard, and `VPcmV34Progress` measures
	 * `sample_count - int_0244` to mask CAS detection for one block
	 * after training ("Masking CAS detection after %d in train") -- a
	 * train-start mark, but nothing says so beyond those two uses.
	 * `int_0248` is read two ways that this tree cannot reconcile into
	 * one meaning: `v34handshakinit` treats it as a positive delta and
	 * pulls `sample_count` back by it (resetting it to -960,000 = one
	 * hundred seconds when the result would leave the window), while
	 * `datapumpv34` and the transmitter's arm 70 treat it as the mark
	 * the running count is measured from (arm 70 copies
	 * `sample_count` into it; `datapumpv34` puts 5 in `progress` when
	 * `sample_count - int_0248` passes 287,488 and reports the rung
	 * results at 144,000 and 1,152,000).  `VPcmV34SetV90RateReneg`
	 * writes the pair with the same constants `v34handshakinit` uses,
	 * which is what says they are one group.
	 *
	 * `datapumpv34` reads sample_count and int_0248 and puts 5 in
	 * `progress` when their difference passes 287,488; `v34handshak`
	 * copies `sample_count` into `timeout_deadline` to restart the
	 * span. `VPcmV34SetTimeOut` is the only function that writes the
	 * pair together and the only one that establishes what the second
	 * is for: it stores `seconds * 9600` into `timeout_deadline` after
	 * zeroing `sample_count`, so `timeout_deadline` is a deadline in
	 * the same units `sample_count` runs in. 9,600 is 8 kHz times 1.2
	 * and not the sample rate, so the argument is not seconds of wall
	 * clock at the codec rate, and the scale is left as the constant
	 * it is rather than named a unit this cannot prove.
	 */
	int low_energy_count;				/* +0x0234 */
	int sample_count;				/* +0x0238 */
	int timeout_deadline;				/* +0x023c */
	int p2_delay_cntr;				/* +0x0240 */
	int int_0244;					/* +0x0244 */
	int int_0248;					/* +0x0248 */
	/*
	 * How far the V.90 receiver has got through phase 3, as a number the
	 * handshake's C++ side ratchets forward. `V34XF_IndicateTrn2dReceived`'s
	 * own debug string names it -- "IndicateTrn2dReceived called,
	 * v90Receiver = %d" -- and the Indicate entry points between them
	 * set 3 (Jd), 6 (DIL) and 10/14/18/20 (TRN2d). Not only ratcheted:
	 * `VPcmV34SetV90RateReneg` assigns 11 or 15 outright, so a
	 * renegotiation can move it down as well as up (deviation D48).
	 */
	int v90_receiver;				/* +0x024c */
	/*
	 * The same for K56Flex: `V34XF_IndicateK56FlexRateDetermined` sets 5
	 * and nothing reconstructed yet reads it. `V34GiveProbeResults`
	 * treats the pair as one "is a PCM receiver running" test.
	 */
	int k56flex_receiver;				/* +0x0250 */
	/*
	 * +0x254 to +0x25c, adaptecho's DC estimator (issue #260 wave 11).
	 * `VPcmV34Create` and `VPcmV34InitiateRetrain` load the first with
	 * `(short)(336 * short_35a4 + 10000)` -- a sample budget scaled by the
	 * negotiated rate -- `VPcmV34Progress` counts it down per received
	 * sample while accumulating the residual's energy into the third, and
	 * every 128th sample (`& 0x7f`) folds `acc >> 8` into the second, which
	 * is then subtracted from every sample.  The object's own diagnostic
	 * names the last two: "Estimated DC = %d  (acc = %d)" (blob 0xcb868).
	 * `dc_count` is read zero-extending only (`PROG_U16`, `movzwl`), so it
	 * is unsigned while the other two are signed.
	 */
	unsigned short dc_count;				/* +0x0254 */
	short dc_est;					/* +0x0256 */
	int dc_acc;					/* +0x0258 */
	/*
	 * adaptecho's three scalars, immediately before the receiver.
	 * `dmadelay` is the base the echo filter's lag is measured from,
	 * `tx_sample` the transmit sample it just dequeued, and
	 * `echo_residual` the running residual.
	 *
	 * `dmadelay` is `V34dmadelay`, the object's own spelling:
	 * `VPcmV34SetDelays` computes `0x610 - cfg[0x68]`, stores it here and
	 * reports it as "V34FEC, V34dmadelay set to %d, (ext delay=%d)" with
	 * the stored value first and the configured one second.
	 */
	short dmadelay;					/* +0x25c */
	short tx_sample;					/* +0x25e */
	short echo_residual;					/* +0x260 */
	/*
	 * +0x262.  `VPcmV34NotifyDP` sets it to 1 under "VPcmV34
	 * Notification: Valid in samples..." and to 0 under "...Invalid in
	 * samples...", and `datapumpv34` returns early when it is zero.  So
	 * the incoming sample stream is usable exactly while this is set.
	 * Two bytes, written as a short by both arms.
	 */
	short samples_valid;				/* +0x262 */
