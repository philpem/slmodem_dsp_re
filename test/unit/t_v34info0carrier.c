/*
 * Component regression for the phase-2 false INFO0 repeat. The existing
 * handshake arena supplies the object and pointer geometry. A quiet signal
 * is passed through the real FSK demodulator until it produces four fresh
 * ones. We then drive the shared 47/56 dispatch through real V34agc.
 *
 * These are seeded component boundaries, not an end-to-end call history.
 * Live Olinet captures establish production reachability separately.
 * The normal arm must reject quiet input; the differential arm must retain
 * the original defect. Both must accept the same prefix at carrier level.
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "v34hsstep.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"

extern void ref_dpskDetectInfo1Init(void *);
extern void ref_fskdemodulate(void *, const short *, void *);

static void
trial(short state, const short *signal, int carrier, int armed, long tag)
{
	static const short silence[4] = { 0, 0, 0, 0 };
	int side, i;

	v34hs_refinit(1);
	v34hs_setup(0);
	v34hs_ours(1);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(state, V34HS_RX_DPSK, V34HS_SSEG);
	v34hs_poke_short(0xaa78, 85);

	for (side = 0; side < 2; side++) {
		struct v34_object *o = v34hs_object(side);
		struct v34_receiver *rx = &o->receiver;
		struct v34_fskdelay *d = (void *)o->echo0.coeff_frac;
		int *ring = (int *)((char *)&rx->rxq + 0x0c);

		o->role = state == V34HS_TX_PHASE2_ANS
			? V34_ROLE_ANSWER : V34_ROLE_CALL;
		if (side) ref_dpskDetectInfo1Init(o);
		else dpskDetectInfo1Init(o);
		memset(d->line, 0, sizeof(d->line));
		rx->retrain_gate = 0;
		/* Exclude the initialiser's all-ones fill from the denominator. */
		o->fsk.sr = 0;
		for (i = 0; i < 20; i++) {
			if (side) ref_fskdemodulate(o, silence, &o->fsk);
			else fskdemodulate(o, silence, &o->fsk);
		}
		diff_eq_int("quiet FSK produces at least four fresh bits (%ld)",
			    o->fsk.nbits >= 4, 1, tag + side);
		diff_eq_int("quiet FSK produces four ones (%ld)",
			    o->fsk.sr & 15, 15, tag + side);
		/* Unity gain, frozen; raw queue samples become the AGC burst. */
		rx->agc_gain = 1024;
		rx->flags = V34_RX_FLAG_AGC_FREEZE;
		rx->rms_idx = 0;
		rx->rxq.rd = ring;
		rx->rxq.wr = ring + 8;
		rx->rxq.count = 8;
		for (i = 0; i < 8; i++) ring[i] = signal[i % 4];
		o->short_3588 = armed;
	}
	v34hs_step();
	for (side = 0; side < 2; side++) {
		int reset = !armed;
		struct v34_object *o = v34hs_object(side);
#ifdef INFO0_REPAIRED
		if (!side && !carrier) reset = 0;
#else
		(void)carrier;
#endif
		diff_eq_int("repeat recovery state (%ld)", o->short_3588,
			    reset ? 4 : armed, tag + side);
		diff_eq_int("repeat counter (%ld)",
			    v34hs_peek_short(side, 0xaa78), reset ? 1 : 86,
			    tag + side);
	}
#ifndef INFO0_REPAIRED
	v34hs_compare("INFO0 carrier reproduction", tag);
#endif
}

int
main(void)
{
	static const short samples[][4] = {
		{ 0, 0, 0, 0 }, { 1, 0, -2, -3 },
		{ 63, -63, 63, -63 }, { 64, -64, 64, -64 },
		{ 4000, 0, -4000, 0 },
		{ SHRT_MIN, SHRT_MAX, SHRT_MIN, SHRT_MAX }
	};
	static const short states[] = { V34HS_TX_PHASE2_ANS, V34HS_TX_PHASE2_CALL };
	unsigned s, n;
	int armed;

	diff_begin("V34 phase-2 INFO0 carrier: quiet, threshold, signal, full scale");
	for (s = 0; s < 2; s++)
		for (n = 0; n < sizeof(samples) / sizeof(samples[0]); n++)
			for (armed = 0; armed < 2; armed++)
				trial(states[s], samples[n], n >= 3, armed,
				      100 * s + 10 * n + 2 * armed);
	return diff_end();
}
