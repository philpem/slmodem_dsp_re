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

extern struct dp_operations b103_ops;

/*
 * The dp_operations entry points.  Declared here so a test can call them
 * directly rather than only through the ops table.
 */
struct dp *b103_create(void *modem, int id, int caller, int srate,
		       int max_frag, struct dp_operations *op);
int b103_delete(struct dp *dp);
int b103_process(void *dp, void *in, void *out, int count);

int dp_b103_init(void);
void dp_b103_exit(void);

#endif /* DSPLIB_B103_H */
