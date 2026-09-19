#ifndef DSPLIB_V32FPDISP_COMMON_H
#define DSPLIB_V32FPDISP_COMMON_H
/*
 * v32fpdisp.c -- ITU-T V.32 / V.32bis: the FP layer's dispatch surface.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V32FP_create    .text 0x07f710   169   build the object from a config
 *   V32FP_modem     .text 0x082630   356   one block, through V32_PROTOCOL
 *   v32_data        .text 0x0827a0   859   the data-mode protocol handler
 *   V32FP_control   .text 0x084530   776   apply a control request
 *   V32FP_status    .text 0x084840  1084   report, and post a request back
 *   V32_PROTOCOL    .data  0x007700    36   nine handlers, GLOBAL
 *   PROTOCOL        .data  0x007768    18   nine codes, file-local
 *
 * `v32fpctl.h` is the twenty-one small setters the HANDSHAKE calls; this file
 * is what the datapump GLUE calls, and `v32fpstat.h` declares it.
 *
 *
 * ---------------------------------------------------------------------------
 * THE CONTROL REQUEST IS CONSUMED, NOT READ
 *
 * `V32FP_control` CLEARS its own bit in the caller's block on each of the two
 * arms it can take -- `andb $0xfb` at 84669 and `andb $0xf7` at 84700 -- and
 * it SETS one, at 8472e and 84743.  So the second argument is in/out and the
 * caller's copy is where the state lives between calls; `v32fpstat.h` and
 * finding F8642 have the bit table.
 *
 * ---------------------------------------------------------------------------
 * THE `Patch:` LINE IS THE AUTHOR'S OWN WORKAROUND FOR A BUG IN `v32_data`
 *
 * The function opens by testing `V32_OBJ_STATUS` against 9 and, if it matches,
 * printing
 *
 *     Patch: set ctl_ptr->vxx_ctl.options.retrain = TRUE
 *
 * and setting bit 2 of the request's second byte -- the retrain bit, named by
 * that string and by nothing else.  Status 9 is what `v32_data` writes when
 * `RetrainDetectV32` fires, and `v32_data` builds its request in a STACK
 * LOCAL and defers the call to the next block through `Control_Flag`, so the
 * request it built is gone by the time this function sees it.  See
 * `docs/deviations.md` and finding F8645: this is the author patching the one
 * bit that mattered back in from the status code, rather than fixing the
 * lifetime.
 *
 * ---------------------------------------------------------------------------
 * THE RATE-FALLBACK TABLE IS A LADDER STEP, AND IT CORROBORATES v32seq.h
 *
 * The retrain arm, when `fp + 0x50d8` is set, switches on the receive rate
 * INDEX at `V32FP_SHORT_2E` through a six-entry jump table at `.rodata +
 * 0x7f40` and writes a new line rate into the object:
 *
 *     index 5 (14400) -> 12000        index 3 (7200)  -> 4800
 *     index 4 (12000) ->  9600        index 0 (4800)  -> nothing
 *     index 2 ( 9600) ->  7200
 *     index 1 ( 9600) ->  7200
 *
 * -- which is `v32seq.h`'s ladder stepped down exactly one rung at every
 * index, with the bottom rung having nowhere to go.  That is an independent
 * confirmation of the whole index -> rate correspondence from a table that
 * never mentions a rate index, index 0 included.  Finding F8647.
 */

#include "dsplib/debug.h"
#include "dsplib/v32.h"
#include "dsplib/v32struct.h"
#include "dsplib/v32data.h"
#include "dsplib/v32demod.h"
#include "dsplib/v32cfg.h"
#include "dsplib/v32fp.h"
#include "dsplib/v32fpctl.h"
#include "dsplib/v32fpstat.h"
#include "dsplib/v32hdx.h"
#include "dsplib/v32hdxst.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32state.h"
#include "dsplib/fpm.h"

/*
 * THE TWO RATE-INDEXED TABLES ARE FILE-LOCAL IN THE OBJECT and this file is
 * their only consumer -- `V32FP_status` reads both `RATEv32` and
 * `SnrToRetrainTable` -- so they are `static` here, which is what the
 * reference's LOCAL `d` records say.  They moved out of `v32fptab.c`, which
 * used to hold them with external linkage so a test could name them; the test
 * tier's globalized copies (tools/testvisible.py) are what replaces that now.
 * Both are `.data` and not `const`, even though nothing writes either.
 */
/* Remaining byte aliases are for fields not yet named by the owner model. */

#define HDX(m)			((m)->hdx)
#define FP(m)			((m)->fp)

/*
 * The parameter block IS the object's first 48 bytes -- v32fp.h derives that
 * from the three `rep movsl` sites -- so this accessor is not a
 * reinterpretation.
 */
#define PARAMS(m)		(&(m)->params)

/*
 * The six `int` switches `struct v32fp_ctl::ctl0` drives, at fp + 0x00 ..
 * fp + 0x24.  v32fpctl.h already names four of the ten; these are the other
 * three this file writes, plus the two `V32FP_status` reports through.  What
 * they SWITCH is not established by anything reconstructed.
 */
#define V32FP_R1C		0x1c
#define V32FP_R20		0x20
#define V32FP_R24		0x24

/*
 * The negotiated rate indices.  v32seq.h's own note pins the pair:
 * "0x7f40b stores to fp + 0x28, 0x7f60f to fp + 0x2a", the two arms of
 * `V32FP_recreate`'s 9600 split.  They are the rate the handshake is moving
 * TO; `V32FP_SHORT_2C` and `V32FP_SHORT_2E` are the rate in force.
 */
#define V32FP_TX_RATE_IDX	0x28
#define V32FP_RX_RATE_IDX	0x2a

/*
 * fp + 0x50d8.  Non-zero selects the rate-fallback ladder on the retrain arm
 * and is cleared by nothing this file writes; `V32FP_status` sets it to 1
 * when eight consecutive SNR drops have been seen, so it reads as "the last
 * retrain was asked for because the line got worse".  Usage inference, and
 * the weakest thing named here.
 */
#define V32FP_RATE_FALLBACK	0x50d8

/*
 * hdx + 0x7c.  Set to 0x960 -- 2400 -- on the originate retrain arm and
 * nowhere else that is written.  Modelled, unnamed.
 */
#define V32HDX_LONG_7C		0x7c
/* hdx + 0x44, zeroed on the ring-response arm.  Modelled, unnamed. */
#define V32HDX_SHORT_44		0x44
/* hdx + 0x84, the transmit block charge; V32TXHDX.c names it. */
#define V32HDX_BLOCK_CHARGE	0x84

/*
 * `V32_OBJ_STATUS` values this file tests or writes.  Only the first is named
 * by the object -- `V32_MSG_NO_CARRIER` is a `.rodata` string `v32_data`
 * prints beside the code 12 -- and the retrain pair below are named for the
 * arm they select, which is usage inference stated as such.
 */
#define V32_MSG_RETRAIN_REQ	0x09	/* v32_data posts it; this file acts   */
#define V32_MSG_RETRAINING	0x0b	/* written on the way into a retrain   */
#define V32_MSG_RESIZED		0x0e	/* the length-change arm               */
#define V32_MSG_RESIZE_DONE	0x0f	/* already reported; do not re-post    */
#define V32_MSG_OK		0x01

/*
 * The datapump-block fields `V32FP_status` reads.  One is named by the author
 * and the rest are not.
 *
 * fp + 0x50d6 is the smoothed DECISION ERROR: the debug line beside it is
 * `Dec error = %d (*64)`, which is a format string and so the strongest
 * evidence class there is.  Everything the function calls an SNR is derived
 * from it by `FPM_div` and `FPM_log10`.
 */
#define V32FP_DEC_ERROR		0x50d6
#define V32FP_SHORT_256		0x256	/* the IIR's input, taken >> 2       */
#define V32FP_SHORT_100		0x100
#define V32FP_SHORT_102		0x102
#define V32FP_SHORT_6C		0x6c
#define V32FP_SHORT_182		0x182
#define V32FP_SHORT_1F8		0x1f8
#define V32FP_INT_188		0x188
#define V32FP_INT_1F4		0x1f4

/*
 * obj + 0x20 counts CONSECUTIVE SNR drops and the ninth starts a retrain --
 * `%d coseq SNR drops detected local retrain is initiated`, printed with the
 * literal 8, is the author's own description of it.
 */
#define V32_OBJ_SNR_DROPS	0x20
/*
 * obj + 0x2c.  While it is at or below 199 the reported SNR is forced to 40
 * and the counter advances, so it is a settling period of two hundred blocks
 * -- one second at `v32.c`'s 5 ms a block.  Modelled, unnamed: nothing says
 * what it settles.
 */
#define V32_OBJ_SHORT_2C	0x2c
#define V32_SNR_SETTLE_BLOCKS	0xc7
#define V32_SNR_SETTLED_VALUE	40
#define V32_SNR_FLOOR		(-60)
#define V32_SNR_DROPS_LIMIT	8

/*
 * obj + 0x11 -- the SECOND BYTE OF `params.options`, and not `V32_OBJ_FLAGS`
 * at 0x31, which is 0x20 above it and is easy to read it as.  Three sites
 * reach it as a byte: `V32FP_create` patches bit 2 (options bit 10) from the
 * caller's configuration, `V32FP_control` writes bit 1 (options bit 9) from
 * `ctl0`'s bit 7, and `V32FP_status` reports both back -- bit 1 as
 * `struct v32_status::flags` bit 7 and bit 2 as `flags1` bit 0.  So the two
 * are a round trip through the control block, which is what says they are the
 * same two bits and not four.
 */
#define V32_OBJ_OPTIONS_HI	0x11
#define V32_OPTIONS_HI_CTL	0x02	/* options bit  9 */
#define V32_OPTIONS_HI_CFG	0x04	/* options bit 10 */

/* obj + 0x31 bit 5, set on the way into a retrain. */
#define V32_FLAG_RETRAIN	0x20
/*
 * obj + 0x31 bit 0.  `V32FP_modem` reads it as "hand the machine to the
 * data-mode handler" -- it parks the state machine at `V32_STATE_DONT_CARE`
 * and points `V32HDX_MODE` at `V32_PROTOCOL`'s data slot -- and `v32_data`
 * clears it when a retrain is wanted.  Usage inference from those two sites.
 */
#define V32_FLAG_DATA		0x01

/* `V32HDX_MODE`'s data-mode slot; V32_MODE_* 0..5 are v32hdxst.h's. */
#define V32_PROTO_DATA		6

/*
 * hdx + 0xae, blocks since the retrain flag was last seen set.  `v32_data`
 * multiplies it by five and the author's own format string calls the product
 * `carrier_loss_time %d ... ms`, so the field is a BLOCK COUNT and five
 * milliseconds is one block.  Finding F8648.
 */
#define V32HDX_LOSS_BLOCKS	0xae
#define V32_BLOCK_MS		5

/*
 * `struct v32_status::flags`, as `v32_data` USES it.  Every one of these four
 * is named for the call it gates and nothing stronger -- the bits themselves
 * come back out of `V32FP_status` from six `int` switches in the datapump
 * block that nothing reconstructed names.  Usage inference, said plainly.
 */
#define V32_STFLAG_SCRAMBLE	0x01	/* ScrambleDataV32 on the tx buffer  */
#define V32_STFLAG_DESCRAMBLE	0x02	/* DescrambleDataV32 on the rx buffer */
#define V32_STFLAG_TXCLOCK	0x04	/* TxClockSyncV32                    */
#define V32_STFLAG_RETRAIN_DET	0x80	/* poll RetrainDetectV32             */

/*
 * Two more `V32_OBJ_STATUS` codes, from `v32_data`.  12 is the author's:
 * the string it prints beside the code is literally `V32_MSG_NO_CARRIER`.
 * 28 is named for the arm that posts it and is inference.
 */
#define V32_MSG_NO_CARRIER	0x0c
#define V32_MSG_RENEG		0x1c

#endif /* DSPLIB_V32FPDISP_COMMON_H */
