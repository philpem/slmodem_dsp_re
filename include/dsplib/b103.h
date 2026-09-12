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
 *
 * `b103_create`, `b103_delete` and `b103_process` are file-local in the
 * object too -- `nm` shows a lower-case `t` -- so they are static in b103.c
 * and have no declarations here.  A test reaches `create` and `destroy`
 * through the table `dp_b103_init` registers and `b103_process` back out of
 * the wrapper the datapump built, exactly as `v22.h` records for V.22.
 */

/**
 * @brief Register the Bell 103 / V.21 datapump's `dp_operations` table with
 * the modem core.
 * @return 0.
 */
int dp_b103_init(void);

/** @brief The other half of dp_b103_init(). */
void dp_b103_exit(void);

#endif /* DSPLIB_B103_H */
