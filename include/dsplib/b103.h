/*
 * b103.h -- Bell 103 / V.21: datapump registration and glue.
 *
 * 300 bit/s FSK, full duplex, no equaliser.  The two standards differ only in
 * their tone frequencies, so one implementation serves both and registers
 * under both DP_IDs.
 */

#ifndef DSPLIB_B103_H
#define DSPLIB_B103_H

#include "dsplib/dp.h"
#include "dsplib/b103fp.h"
#include "dsplib/dp_wrapper.h"

/* Datapump identifiers, from slmodemd's enum DP_ID (modem_defs.h). */
#define DP_V21  21
#define DP_B103 103

/*
 * The datapump's own state.  840 bytes, of which the first 20 are the
 * `struct dp` the modem core sees -- so a `struct dp *` from this module can
 * be cast back to one of these, which is exactly what b103_delete does.
 */
#define B103_BITS_PER_BLOCK 6
#define B103_BIT_BUFFER     100

struct b103_dp {
	struct dp dp;		/* +0x00 .. +0x13                          */
	int caller;		/* +0x14 as passed to create               */
	int tx_bits_wanted;	/* +0x18 bits to fetch per block: 6 once
				 *       connected, 0 before.  Doubles as
				 *       the "carry data" flag             */
	int last_status;	/* +0x1c the previous B103FP_modem status,
				 *       kept only to log transitions      */
	struct b103fp *fp;	/* +0x20 the modulation                    */
	struct dp_wrapper *wrapper;	/* +0x24                           */
	/*
	 * The bit buffers.  Each is filled by the modem core as BYTES and
	 * then widened in place to ints, backwards, so one buffer serves
	 * both.  See b103_process.
	 */
	int tx_bits[B103_BIT_BUFFER];	/* +0x028 */
	int rx_bits[B103_BIT_BUFFER];	/* +0x1b8 */
};

/*
 * The datapump runs at 8 kHz in 160-sample fragments; dp_wrapper adapts
 * between that and whatever the host asks for.
 */
#define B103_DP_SRATE 8000
#define B103_DP_FRAG  160

/* 300 bit/s each way, reported to the core on connect. */
#define B103_LINE_RATE 300

/* From slmodemd's modem_param.h. */
#define MDMPRM_RX_RATE 1
#define MDMPRM_TX_RATE 2

/*
 * `b103_ops` is NOT declared here: it is file-local in b103.c, because the
 * blob's `dp_b103_init`/`dp_b103_exit` reach it through a `.data` section
 * relocation rather than by name.  A test wanting the table takes it from
 * what `dp_b103_init` registered -- `harness_reg_ours.ops[0]` -- which is the
 * path the modem core uses and is symmetric with the reference side.
 * Finding F8121.
 */

/*
 * The dp_operations entry points.  Declared here so a test can call them
 * directly rather than only through the ops table.
 */

/**
 * @brief Construct a Bell 103 / V.21 datapump.
 * @param modem     The host's modem object.
 * @param id        #DP_V21 or #DP_B103, selecting the tone frequencies.
 * @param caller    Nonzero if this side originated the call.
 * @param srate     Host sample rate.
 * @param max_frag  Host fragment size.
 * @param op        The `dp_operations` table this pump was registered
 *                  under (stored in the result's `dp.op`).
 * @return A new `struct dp *` (the `struct b103_dp` it heads), or NULL if
 *         allocation, dp_wrapper_create() or B103FP_create() failed.
 */
struct dp *b103_create(void *modem, int id, int caller, int srate,
		       int max_frag, struct dp_operations *op);

/**
 * @brief Tear down a Bell 103 / V.21 datapump.
 * @param dp  The datapump to free.
 * @return 0.
 */
int b103_delete(struct dp *dp);

/**
 * @brief Run one fragment of Bell 103 / V.21 through the datapump.
 *
 * Handed to dp_wrapper_create() as the datapump's `process` callback, so
 * it runs at #B103_DP_SRATE / #B103_DP_FRAG, not the host's rate -- the
 * wrapper is what the modem core actually calls.
 *
 * @param dp     The datapump, as `void *` (dp_wrapper's callback signature);
 *               really a `struct dp *`.
 * @param in     One fragment of B103-rate input samples.
 * @param out    One fragment of B103-rate output samples.
 * @param count  Unused; the fragment size is fixed at #B103_DP_FRAG.
 * @return A `DPSTAT_*` status code.
 */
int b103_process(void *dp, void *in, void *out, int count);

/**
 * @brief Register the Bell 103 / V.21 datapump's `dp_operations` table with
 * the modem core.
 * @return 0.
 */
int dp_b103_init(void);

/** @brief The other half of dp_b103_init(). */
void dp_b103_exit(void);

#endif /* DSPLIB_B103_H */
