/*
 * v23.h -- ITU-T V.23: datapump registration and glue.
 *
 * 1200 bps one way, 75 bps the other.  One DP_ID, unlike Bell 103, because
 * there is nothing else that shares the implementation.
 */

#ifndef DSPLIB_V23_H
#define DSPLIB_V23_H

#include "dsplib/dp.h"
#include "dsplib/dp_wrapper.h"
#include "dsplib/v23fp.h"

/* Datapump identifier, from slmodemd's enum DP_ID (modem_defs.h). */
#define DP_V23 23

/*
 * The datapump's own state.  840 bytes, laid out exactly as Bell 103's is --
 * same header, same two bit buffers at the same offsets, same total size.
 * Two of the fields differ only in what they mean, and the comments say so.
 */
#define V23_BIT_BUFFER 100

struct v23_dp {
	struct dp dp;			/* +0x000 .. +0x013                */
	int caller;			/* +0x014 as passed to create      */
	/*
	 * How many bits to ask the core for next block.  Not a constant the
	 * way Bell 103's is: it is whatever V23ModemMain reported the
	 * transmitter CONSUMED last block, so the request tracks the bit rate
	 * without this file having to know what it is.  Zero until connect,
	 * which is also what stops the core's data reaching a line that is
	 * still training.
	 */
	int tx_bits_wanted;		/* +0x018                          */
	int connected;			/* +0x01c carrier has been up once */
	struct v23modem *modem;		/* +0x020 the modulation           */
	struct dp_wrapper *wrapper;	/* +0x024                          */
	/*
	 * Filled by the core as BYTES and widened in place to ints, and
	 * narrowed back the same way on the receive side.  See v23_process.
	 */
	int tx_bits[V23_BIT_BUFFER];	/* +0x028 */
	int rx_bits[V23_BIT_BUFFER];	/* +0x1b8 */
};

/*
 * The datapump runs at 8 kHz in 160-sample fragments; dp_wrapper adapts
 * between that and whatever the host asks for.
 */
#define V23_DP_SRATE 8000
#define V23_DP_FRAG  160

/* The two line rates, and the carrier-loss timeout the modem is built with. */
#define V23_RATE_FORWARD  1200
#define V23_RATE_BACKWARD 75
#define V23_SILENCE_MS    700

/*
 * `v23_ops` is NOT declared here -- file-local in v23.c, on the same
 * evidence as b103.h's note.  Take it from `harness_reg_ours.ops[0]` after
 * `dp_v23_init()`.  Finding F8121.
 */

/*
 * The dp_operations entry points below are declared here so a test can call
 * them directly rather than only through the ops table.
 */

/**
 * @brief Create a V.23 datapump instance.
 * @param modem     Opaque modem core handle.
 * @param id        Datapump id (DP_V23).
 * @param caller    Non-zero if this end originated the call.
 * @param srate     Sample rate (V23_DP_SRATE).
 * @param max_frag  Maximum fragment size (V23_DP_FRAG).
 * @param op        The dp_operations table to fill in.
 * @return The new `struct dp *` (a `struct v23_dp *` in disguise).
 */
struct dp *v23_create(void *modem, int id, int caller, int srate, int max_frag,
		      struct dp_operations *op);

/**
 * @brief Tear a V.23 datapump instance down.
 * @param dp  The datapump instance.
 * @return The object's own deletion status.
 */
int v23_delete(struct dp *dp);

/**
 * @brief Run one block of V.23 modulation and demodulation.
 * @param dp_arg  The datapump instance (`struct v23_dp *`).
 * @param in      Input samples from the line.
 * @param out     Output for the modulated transmit samples.
 * @param count   Block size in samples.
 * @return The object's own processing status.
 */
int v23_process(void *dp_arg, void *in, void *out, int count);

/**
 * @brief Register V.23 (DP_V23) with the datapump core.
 * @return The object's own registration status.
 */
int dp_v23_init(void);

/** @brief Deregister V.23 (DP_V23) from the datapump core. */
void dp_v23_exit(void);

#endif /* DSPLIB_V23_H */
