/*
 * v22conn.c -- V.22 / V.22bis: the two connect states of the protocol machine.
 *
 * Reconstructed from dsplibs.o:
 *
 *   connect_2400   .text 0x088cd0  1,505 bytes
 *   connect_1200   .text 0x08a460    818 bytes
 *
 * `include/dsplib/v22conn.h` carries the derivation -- the four format
 * strings that name connect_2400's sub-states, the one that names the
 * carrier-loss pair, and the argument list.  This file is the code.
 *
 * They are written in the object's address order, connect_2400 first.  That
 * is a guess about the translation unit and not a claim: the two are 6 KB
 * apart and `tools/tumap.py` puts thirteen V.22 units in one bracket, so
 * nothing here says they share a file.
 *
 * ---------------------------------------------------------------------------
 * THE INSTANCE POINTER IS READ AGAIN AFTER EVERY CALL
 *
 * Same observation src/pump/v22/v22data.c records for its four wrappers, and
 * it holds here too: connect_2400 reloads `0x50(%ebx)` after
 * `SetAdaptEqV22` rather than keeping the block pointer in a callee-saved
 * register across the call, and connect_1200 reloads it between
 * `DescrambleDataV22` and the `r10` test and again after `ReadGTimer`.  A
 * local holding it would have been spilled and reloaded from the stack.  So
 * `fp->hdx` is spelled out at each use rather than cached, which is what the
 * compiler was forced to encode.
 *
 * ---------------------------------------------------------------------------
 * THE ONE FIELD THAT HAS NO NAME TO USE
 *
 * connect_2400's retrain path stores a 16-bit 1 at `hdx` + 0x38.
 * `struct v22fp_hdx` models +0x36..+0x3b as `unsigned char r36[6]` --
 * unmodelled space, six bytes rather than a shape, because nothing had read
 * any of it.  This is the first reader, and it wants `short r38` there; the
 * store is spelled through the byte array so that this file changes no
 * header.
 */

#include "dsplib/v22conn.h"

#include "dsplib/debug.h"
#include "dsplib/v22data.h"
#include "dsplib/v22det.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22prc.h"
#include "dsplib/v22rate.h"

/* `hdx` + 0x38, a 16-bit field inside v22fp.h's unmodelled `r36[6]`. */
#define HDX_R38(h)	(*(short *)(void *)&(h)->r36[2])

/*
 * The carrier-loss tail both functions end in, up to the point where they
 * differ.  It is written out twice rather than shared, because the object
 * writes it out twice: connect_1200's copy returns as soon as the carrier is
 * back and connect_2400's does not, and each copy tail-jumps into its own
 * `RxClampV22`.
 */

/* ------------------------------------------------------------------------ */

void
connect_2400(struct v22fp *fp, unsigned short *txsym, short *txout,
	     short *rxin, unsigned short *rxsym,
	     unsigned short *txcount, unsigned short *rxcount)
{
	unsigned short nsym;
	unsigned int now;
	short elapsed;

	fp->status = V22_STATUS_01;

	switch (fp->hdx->r0c) {
	case V22_NODE_2400A:
		/* Training at 1200: send ones, receive, clamp, wait. */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > V22_NODE_2400A_MS) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "connect_2400, NODE_2400B\n");
			fp->hdx->r0c = V22_NODE_2400B;
			SetRxRate(fp, V22_RATE_2400);
		}
		break;

	case V22_NODE_2400B:
		/* The receiver is at 2400 now; the transmitter follows. */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > V22_NODE_2400B_MS) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "connect_2400, NODE_2400C\n");
			fp->hdx->r0c = V22_NODE_2400C;
			SetTxRate(fp, V22_RATE_2400);
			SetAdaptEqV22(fp, 3);
		}
		break;

	case V22_NODE_2400C:
		/*
		 * Both directions at 2400.  `r10` latches the first
		 * "receiver trained" verdict and is not re-tested once set.
		 */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_2400);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		if (fp->hdx->r10 == 0)
			fp->hdx->r10 = RxTrained2400((const short *)rxsym,
						     rxcount);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > V22_NODE_2400C_MS) {
			if (fp->hdx->r10 == 1) {
				fp->flags |= V22FP_FLAGS_CONNECT;
				fp->status = V22_MSG_CONNECT_2400;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V22_MSG_CONNECT_2400 In "
					    "NODE_2400C\n");
			} else {
				fp->hdx->gtimer = 0;
				fp->hdx->r0c = V22_NODE_2400D;
			}
		}
		break;

	case V22_NODE_2400D:
		/*
		 * The last node tests the verdict fresh on every block rather
		 * than latching it, and its deadline is `hdx->r04` -- the
		 * caller's own limit, 60000 ms as `v22_create` configures it.
		 */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_2400);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		if (RxTrained2400((const short *)rxsym, rxcount)) {
			fp->flags |= V22FP_FLAGS_CONNECT;
			fp->status = V22_MSG_CONNECT_2400;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V22_MSG_CONNECT_2400 In NODE_2400D\n");
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > (unsigned int)fp->hdx->r04) {
			fp->flags |= V22FP_FLAGS_TIMEOUT;
			fp->status = V22_MSG_ERROR7;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V22_MSG_ERROR7\n");
		}
		break;

	default:
		break;
	}

	/*
	 * The carrier-loss tail.  Every arm above falls into it, including
	 * the ones that have just declared the call connected.
	 */
	if (CarrierDetect(fp)) {
		/*
		 * Carrier back, and `r3c` non-zero means it went away inside
		 * the grace window rather than never having been lost.  That
		 * is a retrain, not a connect.
		 */
		if (fp->hdx->r3c != 0) {
			fp->hdx->r0e = V22_HDX_R0E_RETRAIN;
			fp->hdx->r0c = V22_NODE_RETRAIN;
			SetAdaptEqV22(fp, 1);
			HDX_R38(fp->hdx) = V22_HDX_R38_RETRAIN;
			fp->status = V22_STATUS_0B;
			fp->hdx->r3c = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "Carrier back during carrier_loss_time "
				    "(Connect_2400). Retrain initiated.");
		}
		return;
	}

	fp->hdx->r3c++;
	elapsed = (short)(fp->hdx->r3c * V22_BLOCK_MS);
	if (fp->params.r18 < elapsed) {
		fp->status = V22_MSG_NO_CARRIER;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_MSG_NO_CARRIER3\n");
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V22_MSG_NO_CARRIER won't be reported "
				     "(carrier_loss_time %d of %d ms)",
				     (int)elapsed, (int)fp->params.r18);
}

/* ------------------------------------------------------------------------ */

void
connect_1200(struct v22fp *fp, unsigned short *txsym, short *txout,
	     short *rxin, unsigned short *rxsym,
	     unsigned short *txcount, unsigned short *rxcount)
{
	unsigned short nsym;
	unsigned int now;
	short elapsed;

	fp->status = V22_STATUS_01;

	switch (fp->hdx->r0c) {
	case V22_NODE_1200_12:
		/*
		 * The latching node: `r10` takes the first verdict and the
		 * deadline then reads it once.  Same shape as NODE_2400C, and
		 * the only difference is the rate and the constants.
		 */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		if (fp->hdx->r10 == 0)
			fp->hdx->r10 = RxTrained1200((const short *)rxsym,
						     rxcount);

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > V22_NODE_1200_12_MS) {
			if (fp->hdx->r10 == 1) {
				SetAdaptEqV22(fp, 3);
				fp->flags |= V22FP_FLAGS_CONNECT;
				fp->status = V22_STATUS_04;
			} else {
				fp->hdx->gtimer = 0;
				fp->hdx->r0c = V22_NODE_1200_13;
			}
		}
		break;

	case V22_NODE_1200_13:
		/*
		 * The last node.  NOTE THE DOUBLE CLAMP: the connect arm
		 * calls `RxClampV22` and then falls into the unconditional
		 * call below, so a block that completes training clamps
		 * twice.  The second one is what the object executes and it
		 * writes the same twelve values over the first, so nothing
		 * observable turns on it -- but it is two calls in the object
		 * and it is two here.  NODE_2400D, which is otherwise the
		 * same node, has only the unconditional one.
		 */
		MakeTxData((short *)txsym, (const short *)txcount,
			   V22_TXDATA_ONES_1200);
		ScrambleDataV22(fp, txsym, *txcount);
		*txcount = ModDataV22(fp, txsym, txout, *txcount);

		nsym = DemodDataV22(fp, rxin, rxsym, *rxcount);
		*rxcount = nsym;
		DescrambleDataV22(fp, rxsym, nsym);

		if (RxTrained1200((const short *)rxsym, rxcount)) {
			RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
			fp->flags |= V22FP_FLAGS_CONNECT;
			fp->status = V22_STATUS_04;
		}

		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);

		now = (unsigned int)ReadGTimer(fp);
		if (now > (unsigned int)fp->hdx->r04) {
			fp->flags |= V22FP_FLAGS_TIMEOUT;
			fp->status = V22_STATUS_18;
		}
		break;

	default:
		break;
	}

	/*
	 * The same tail, minus the retrain: connect_1200 returns the instant
	 * the carrier is back and does not look at `r3c` at all.
	 */
	if (CarrierDetect(fp))
		return;

	fp->hdx->r3c++;
	elapsed = (short)(fp->hdx->r3c * V22_BLOCK_MS);
	if (fp->params.r18 < elapsed) {
		fp->status = V22_MSG_NO_CARRIER;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V22_MSG_NO_CARRIER4\n");
		RxClampV22(fp, rxin, (short *)rxsym, (short *)rxcount);
		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V22_MSG_NO_CARRIER won't be reported "
				     "(carrier_loss_time %d of %d ms)",
				     (int)elapsed, (int)fp->params.r18);
}
