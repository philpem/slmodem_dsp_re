/*
 * v22conn.h -- V.22 / V.22bis: the two connect states of the protocol machine.
 *
 * Reconstructed from dsplibs.o:
 *
 *   connect_2400   .text 0x088cd0  1,505 bytes
 *   connect_1200   .text 0x08a460    818 bytes
 *
 * (`connect_2400` is at 0x88cd0, not the 0x8a7a0 an earlier note gave; its
 * size settles it -- 0x892b1 - 0x88cd0 = 0x5e1 = 1,505.)
 *
 * THEY ARE NOT `V22_PROTOCOL` ENTRIES.  That table is at `.rodata` + 0x8544
 * and its seven relocations resolve to `v22_data`, `v22_originate`,
 * `v22_answer`, `v22_local_loop`, `v22_org_rmloop2`, `v22_ans_rmloop2` and
 * `v22_retrain`.  These two are called BY three of those seven and by nothing
 * else -- six relocations in all, two each from `v22_local_loop`,
 * `v22_answer` and `v22_originate` -- so they are a shared subroutine of the
 * protocol machine rather than a state of it.  (The `.rodata` blocks that
 * follow the table at 0x8560, 0x8580, 0x85b8 and 0x85f4 are those functions'
 * own jump tables: their entries are addresses INSIDE a function, not
 * function symbols.)
 *
 * Both take the SAME seven arguments in the same order and both have the same
 * shape: set the status byte, dispatch on a sub-state held in the
 * tone/detector block, and then run one common carrier-loss tail.
 *
 * ---------------------------------------------------------------------------
 * WHAT NAMES THE SUB-STATES, AND WHAT DOES NOT
 *
 * `struct v22fp_hdx::r0c` is the sub-state.  connect_2400 prints four format
 * strings that name its four values in the author's own words --
 *
 *     "connect_2400, NODE_2400B\n"   printed while ENTERING the r0c == 9 arm
 *     "connect_2400, NODE_2400C\n"   printed while ENTERING the r0c == 10 arm
 *     "V22_MSG_CONNECT_2400 In NODE_2400C\n"   printed FROM the r0c == 10 arm
 *     "V22_MSG_CONNECT_2400 In NODE_2400D\n"   printed FROM the r0c == 11 arm
 *
 * -- and the two readings agree: 9 is NODE_2400B, 10 is NODE_2400C, 11 is
 * NODE_2400D, so 8 is NODE_2400A.  connect_1200's two values, 12 and 13, are
 * named by NOTHING: it references only two strings and neither mentions a
 * node.  They keep their values for names, and the obvious analogy
 * (NODE_1200A and NODE_1200B) is recorded here rather than used.
 *
 * THE RATE LADDER IS THE CORROBORATION.  NODE_2400A and B transmit
 * `V22_TXDATA_ONES_1200` and NODE_2400C and D transmit
 * `V22_TXDATA_ONES_2400`; the A -> B step calls `SetRxRate(fp, 1)` and the
 * B -> C step calls `SetTxRate(fp, 1)`.  That is V.22bis training at 1200 and
 * then changing up, receiver first, which is what the two patterns say
 * independently.
 *
 * ---------------------------------------------------------------------------
 * THE CARRIER-LOSS TAIL, AND THE TWO FIELDS IT NAMES
 *
 * Both functions end in the same block:
 *
 *     if (CarrierDetect(fp)) ... return;
 *     elapsed = ++hdx->carrier_loss_blocks * 20;
 *     if (params.carrier_loss_ms < elapsed) { status = NO_CARRIER; RxClampV22(...); }
 *     else debug("... (carrier_loss_time %d of %d ms)", elapsed, params.carrier_loss_ms);
 *
 * and that format string is the strongest evidence in either function.  It
 * prints the two operands of the comparison as milliseconds, so
 * `hdx->carrier_loss_blocks` is a COUNT OF CONSECUTIVE BLOCKS WITHOUT CARRIER and
 * `params.carrier_loss_ms` is the CARRIER-LOSS GRACE TIME IN MILLISECONDS -- 700 as
 * `v22_create` configures it, 35 blocks.  The 20 is `ReadGTimer`'s own block
 * length, so the two agree on what a block is.  Neither field is renamed
 * here; see the note at the bottom of this file.
 *
 * The tails are NOT identical.  connect_1200 returns the moment
 * `CarrierDetect` is true; connect_2400 first checks whether it is coming
 * back INSIDE the grace window (`hdx->carrier_loss_blocks != 0`) and, if so, initiates a
 * retrain -- "Carrier back during carrier_loss_time (Connect_2400). Retrain
 * initiated." -- which is its own fourth format string.
 *
 * ---------------------------------------------------------------------------
 * THE TIMER COMPARISONS ARE UNSIGNED, AND THAT IS ENCODED
 *
 * Every one of the six `ReadGTimer` tests is a `jbe`, not a `jle`, including
 * the two against `hdx->node_deadline`.  `ReadGTimer` returns `int` and the constants
 * are positive, so no reachable value can tell the two readings apart; the
 * unsigned form is what the object encodes and is what is written.
 *
 * ---------------------------------------------------------------------------
 * BOTH ARE DECLARED void AND THE OBJECT DOES NOT SETTLE THAT.  Each has three
 * exits: a bare `ret` with `CarrierDetect`'s result still in `%eax`, and two
 * tail jumps into `void` callees.  Same situation as `TxClockSync` in
 * v22prc.h and recorded for the same reason.
 */

#ifndef DSPLIB_V22CONN_H
#define DSPLIB_V22CONN_H

struct v22fp;

/*
 * ---------------------------------------------------------------------------
 * The sub-state, `struct v22fp_hdx::r0c`.
 */

/* connect_2400's four.  Named from the four format strings above. */
#define V22_NODE_2400A		8
#define V22_NODE_2400B		9
#define V22_NODE_2400C		10
#define V22_NODE_2400D		11

/*
 * connect_1200's two.  NOT named: nothing in the object says what either is.
 * Named for their value, which is all that is established.
 */
#define V22_NODE_1200_12	12
#define V22_NODE_1200_13	13

/*
 * The sub-state connect_2400 hands the machine when the carrier comes back
 * inside the grace window.  Neither this file nor anything reconstructed says
 * what state 1 is; it is the retrain entry by position in that one branch.
 */
#define V22_NODE_RETRAIN	1

/*
 * ---------------------------------------------------------------------------
 * `struct v22fp::status` (+0x1c).
 *
 * It is a message code -- `V22STAT: --> %d` is how the object reports one --
 * and four of the seven values these two functions write sit immediately
 * beside a `dsplibs_debug_printf` that names a `V22_MSG_*`.  Those four carry
 * the author's own word; the other three are named for their value, because
 * co-occurrence at ONE site is not the same evidence.
 */
#define V22_STATUS_01		1	/* written on entry, unconditionally  */
#define V22_MSG_CONNECT_2400	3	/* printed at NODE_2400C and D        */
#define V22_STATUS_04		4	/* connect_1200's trained exit.  The
					 * analogue of the line above would
					 * make it CONNECT_1200; connect_1200
					 * prints nothing there, so no.        */
#define V22_STATUS_0B		11	/* connect_2400's carrier-back retrain */
#define V22_MSG_NO_CARRIER	16	/* printed as NO_CARRIER3 and _4      */
#define V22_MSG_ERROR7		23	/* connect_2400's training timeout    */
#define V22_STATUS_18		24	/* connect_1200's; prints nothing     */

/*
 * ---------------------------------------------------------------------------
 * Bits OR-ed into `struct v22fp::flags` (+0x1d).
 *
 * WHAT ANY OF THESE BITS MEANS IS NOT ESTABLISHED.  Nothing reconstructed
 * reads that byte and no format string names it.  They are named by BIT
 * VALUE, and the two masks the object writes are spelled from those bits so
 * that the source and the object's immediates stay 1:1.  What IS observed is
 * only where each mask is written, and that is all the comments claim.
 */
#define V22FP_FLAG_1D_BIT0	0x01
#define V22FP_FLAG_1D_BIT1	0x02
#define V22FP_FLAG_1D_BIT3	0x08
#define V22FP_FLAG_1D_BIT5	0x20

/* 0x29 -- written at every exit that also writes a CONNECT status. */
#define V22FP_FLAGS_CONNECT	(V22FP_FLAG_1D_BIT0 | V22FP_FLAG_1D_BIT3 \
				 | V22FP_FLAG_1D_BIT5)
/* 0x02 -- written at every exit that also writes a training-timeout status. */
#define V22FP_FLAGS_TIMEOUT	V22FP_FLAG_1D_BIT1

/*
 * ---------------------------------------------------------------------------
 * The node timeouts, in milliseconds on `ReadGTimer`'s clock.
 *
 * Each is a bare immediate in its own `cmp`.  The unit is `ReadGTimer`'s, and
 * v22prc.h establishes that; the names are the node each test belongs to.
 * The clock advances 20 ms per call, so the effective thresholds are the next
 * multiple of 20 above each -- 460, 620, 800 and 760.
 */
#define V22_NODE_2400A_MS	449u
#define V22_NODE_2400B_MS	600u
#define V22_NODE_2400C_MS	790u
#define V22_NODE_1200_12_MS	755u

/*
 * One datapump block on the shared clock, and the multiplier the carrier-loss
 * counter is scaled by before it is compared with `params.carrier_loss_ms`.  The same 20
 * `ReadGTimer` adds, and the format string is what says both are milliseconds.
 */
#define V22_BLOCK_MS		20

/*
 * The value connect_2400 stores at `hdx` + 0x38 on the retrain path.  See the
 * note in the source: v22fp.h models +0x36..+0x3b as unmodelled bytes.
 */
#define V22_HDX_R38_RETRAIN	1

/*
 * `hdx->protocol` on the same path.  Unestablished; `V22FP_create` writes 1, 2 or
 * 3 there and nothing reconstructed reads it.
 */
#define V22_HDX_R0E_RETRAIN	6

/*
 * ---------------------------------------------------------------------------
 * The handlers.
 *
 * SEVEN ARGUMENTS, and the count and the order are read off the stack slots
 * both functions touch -- 0x30 through 0x48 above `%esp` after a
 * `sub $0x2c`, with nothing above 0x48 read.  The types come from the callees
 * each argument reaches, which between them pin every one:
 *
 *   fp        `struct v22fp *`   +0x1c, +0x1d, +0x18, +0x50, +0x54
 *   txsym     MakeTxData's `out`, then ScrambleDataV22's and ModDataV22's
 *             `data` -- so `short *` at one call site and `unsigned short *`
 *             at two.  Declared unsigned and cast for MakeTxData.
 *   txout     ModDataV22's `out`
 *   rxin      DemodDataV22's `in`, and RxClampV22's ignored second argument
 *   rxsym     DemodDataV22's `sym`, DescrambleDataV22's `data`, RxClampV22's
 *             `out` and RxTrained*'s `symbols`.  Same split; same choice.
 *   txcount   MakeTxData's `count`, and read with `movzwl` for the two data
 *             calls.  ModDataV22's sample count is stored back over it.
 *   rxcount   the INPUT SAMPLE COUNT on the way in, and the symbol count on
 *             the way out -- DemodDataV22's return is stored over it, and
 *             `RxClampV22` then sets it to 12.  Also RxTrained*'s `count`.
 *
 * So both counts are in/out and both symbol buffers are scratch the caller
 * owns.  A caller loads `*txcount` with the symbols to send and `*rxcount`
 * with the samples received, and reads back the samples to send and the
 * symbols received.
 */
/**
 * @brief Shared V.22bis training subroutine: connect at 1200 bit/s (NODE_1200_12/13).
 *
 * Called from `v22_local_loop`, `v22_answer` and `v22_originate` (not a
 * `V22_PROTOCOL` table entry itself). Dispatches on `hdx->r0c`'s two live
 * values, then runs the shared carrier-loss tail: returns the moment
 * `CarrierDetect` reports no carrier.
 *
 * @param fp       The V.22 datapump instance.
 * @param txsym    Transmit symbols to scramble and modulate.
 * @param txout    Output for the modulated transmit samples.
 * @param rxin     Received samples to demodulate.
 * @param rxsym    Output for the demodulated receive symbols.
 * @param txcount  In/out: transmit symbol/sample count.
 * @param rxcount  In/out: receive sample/symbol count.
 */
void connect_1200(struct v22fp *fp, unsigned short *txsym, short *txout,
		  short *rxin, unsigned short *rxsym,
		  unsigned short *txcount, unsigned short *rxcount);

/**
 * @brief Shared V.22bis training subroutine: connect at 2400 bit/s (NODE_2400A..D).
 *
 * Same calling contract as connect_1200(), for the four 2400 bit/s training
 * sub-states (NODE_2400A -> B -> C -> D, changing the receive then the
 * transmit rate as it goes). Its carrier-loss tail differs from
 * connect_1200()'s: if carrier comes back inside the grace window
 * (`hdx->carrier_loss_blocks != 0`), it initiates a retrain instead of
 * just returning.
 *
 * @param fp       The V.22 datapump instance.
 * @param txsym    Transmit symbols to scramble and modulate.
 * @param txout    Output for the modulated transmit samples.
 * @param rxin     Received samples to demodulate.
 * @param rxsym    Output for the demodulated receive symbols.
 * @param txcount  In/out: transmit symbol/sample count.
 * @param rxcount  In/out: receive sample/symbol count.
 */
void connect_2400(struct v22fp *fp, unsigned short *txsym, short *txout,
		  short *rxin, unsigned short *rxsym,
		  unsigned short *txcount, unsigned short *rxcount);

#endif /* DSPLIB_V22CONN_H */
