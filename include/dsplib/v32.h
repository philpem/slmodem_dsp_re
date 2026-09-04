/*
 * v32.h -- ITU-T V.32 / V.32bis: datapump registration and glue.
 *
 * The thin layer between the modem core and the V.32 modulation.  It is
 * `v23.h`'s file with V.32's numbers: same `struct dp` header, the same two
 * hundred-element bit buffers at the same offsets, the same 840-byte total,
 * the same `dp_wrapper` in front of an 8 kHz fragment, and the same trick of
 * widening a byte buffer into an int buffer in place.  Reading the two side
 * by side is the fastest way to see what belongs to V.32 and what belongs to
 * the datapump architecture.
 *
 * TWO IDS, NOT ONE.  `dp_v32_init` registers the SAME operations table under
 * `DP_V32` and `DP_V32BIS`, exactly as `b103.c` registers one table under
 * `DP_V21` and `DP_B103`.  The numbers are slmodemd's own -- `enum DP_ID` in
 * `ref/slmodemd/modem_defs.h` gives `DP_V32 = 32` and `DP_V32BIS = 132` --
 * so this is the vendored header's word and not an inference.
 *
 * ---------------------------------------------------------------------------
 * THE FRAGMENT IS 40 SAMPLES, WHICH IS TWELVE SYMBOLS
 *
 * `dp_wrapper_create` is called with a fragment of 40 and a datapump rate of
 * 8000, so one call to `v32_process` is 5 ms.  V.32 is 2400 baud, so 40
 * samples at 8000 Hz is exactly twelve symbols -- which is `V32_SYMBOL_LEN[0]`
 * and is the value `v32_process` puts in `symbols_per_block` when the line
 * comes up.
 *
 * ---------------------------------------------------------------------------
 * `bits_per_symbol` IS DERIVED FROM THE RATE BY A DIVISION BY 2400
 *
 * `v32_process` recomputes it on every connect as
 *
 *     bits_per_symbol = line_rate / 2400
 *
 * emitted as a multiply by 0x1b4e81b5 and a 40-bit shift.  `v32_create` seeds
 * it with 6 and `line_rate` with 14400, which is the same relation held in
 * advance.  It is the second argument to `modem_get_bits` and
 * `modem_put_bits`, so those two carry a WIDTH here and not the channel
 * number V.23 passes -- V.23's is 1 because V.23 is one bit per element.
 */

#ifndef DSPLIB_V32_H
#define DSPLIB_V32_H

#include "dsplib/dp.h"
#include "dsplib/dp_wrapper.h"

/* slmodemd's enum DP_ID (ref/slmodemd/modem_defs.h). */
#define DP_V32		32
#define DP_V32BIS	132

/*
 * The bit buffers are a hundred elements each, and the number is the object's
 * twice over: `v32_process` clamps the receive count to 100 with a FATAL
 * message above it, and the `.bss` scratch pair `V32FP_modem` uses --
 * `tx_in_internal` and `rx_out_internal`, 0xc8 bytes each -- is a hundred
 * shorts as well.
 */
#define V32_BIT_BUFFER	100

struct v32_dp {
	struct dp dp;			/* +0x000 .. +0x013                */
	/*
	 * Bits per symbol, and the second argument to `modem_get_bits` and
	 * `modem_put_bits`.  Seeded 6 by create and recomputed as
	 * `line_rate / 2400` on every connect.
	 */
	int bits_per_symbol;		/* +0x014                          */
	/*
	 * +0x018, bit/s, seeded 14400.  UNSIGNED, and that is forced: the
	 * division by 2400 that derives `bits_per_symbol` is emitted as
	 * `mov $0x1b4e81b5,%eax; mull; shr $0x8,%edx` -- an UNSIGNED magic
	 * multiply.  A signed `int` would have given `imul`, a `sar` and a
	 * correction for the sign, which is what every signed division by a
	 * constant in this object looks like.
	 */
	unsigned int line_rate;		/* +0x018                          */
	/*
	 * Symbols to exchange per block.  ZERO UNTIL THE LINE COMES UP, which
	 * is what keeps the core's data off a line that is still training --
	 * the same job V.23's `connected` does, done with the count itself.
	 * `v32_process` sets it to 12 on connect, which is one 40-sample
	 * fragment at 2400 baud.
	 */
	int symbols_per_block;		/* +0x01c                          */
	void *fp;			/* +0x020 the V32FP datapump object */
	struct dp_wrapper *wrapper;	/* +0x024                          */
	/*
	 * Filled by the core as BYTES and widened in place to ints, and
	 * narrowed back the same way on the receive side.  See `v32_process`,
	 * and `v23.h` for the same two buffers at the same two offsets.
	 */
	int tx_bits[V32_BIT_BUFFER];	/* +0x028                          */
	int rx_bits[V32_BIT_BUFFER];	/* +0x1b8                          */
};

/* The datapump runs at 8 kHz in 40-sample fragments. */
#define V32_DP_SRATE	8000
#define V32_DP_FRAG	40

/* V.32's symbol rate, and the divisor that turns a line rate into bits. */
#define V32_BAUD	2400

/*
 * `v32_ops` is NOT declared here -- file-local in `v32.c`, on the same
 * evidence as `b103.h`'s and `v23.h`'s note.  Take it from
 * `harness_reg_ours.ops[0]` after `dp_v32_init()`.  Finding F8121.
 */

/*
 * The dp_operations entry points below are declared here so a test can call
 * them directly rather than only through the ops table.
 */

/**
 * @brief Create a V.32/V.32bis datapump instance.
 * @param modem     Opaque modem core handle.
 * @param id        Datapump id (DP_V32 or DP_V32BIS).
 * @param caller    Non-zero if this end originated the call.
 * @param srate     Sample rate (V32_DP_SRATE).
 * @param max_frag  Maximum fragment size (V32_DP_FRAG).
 * @param op        The dp_operations table to fill in.
 * @return The new `struct dp *` (a `struct v32_dp *` in disguise).
 */
struct dp *v32_create(void *modem, int id, int caller, int srate, int max_frag,
		      struct dp_operations *op);

/**
 * @brief Tear a V.32 datapump instance down.
 * @param dp  The datapump instance.
 * @return The object's own deletion status.
 */
int v32_delete(struct dp *dp);

/**
 * @brief Run one block of V.32 modulation and demodulation.
 * @param dp_arg  The datapump instance (`struct v32_dp *`).
 * @param in      Input samples from the line.
 * @param out     Output for the modulated transmit samples.
 * @param count   Block size in samples.
 * @return The object's own processing status.
 */
int v32_process(void *dp_arg, void *in, void *out, int count);

/**
 * @brief Register V.32 and V.32bis (DP_V32, DP_V32BIS) under one operations table with the datapump core.
 * @return The object's own registration status.
 */
int dp_v32_init(void);

/** @brief Deregister V.32 and V.32bis from the datapump core. */
void dp_v32_exit(void);

#endif /* DSPLIB_V32_H */
