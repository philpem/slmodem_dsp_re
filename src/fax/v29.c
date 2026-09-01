/*
 * v29.c -- ITU-T V.29 (fax): the receiver's entry points, and the two
 *          transmitter accessors that sit in the same run of addresses.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V29RX_delete          .text 0x09b590  220
 *   V29TX_delete          .text 0x09be90  135
 *   V29RX_modem           .text 0x0a3f50  127
 *   RxHdxDataV29          .text 0x0a3fd0  226
 *   RxHdxErrorV29         .text 0x0a40c0   59
 *   V29RX_status          .text 0x0a45f0  190
 *   V29TX_modem           .text 0x0a46b0  182
 *   V29TX_status          .text 0x0a5030  100
 *   DemodDataV29          .text 0x0a5ff0  398
 *   DescrambleDataV29     .text 0x0a6180   30
 *   CarrierDetectV29      .text 0x0a61a0   22
 *   DataCarrierDetectV29  .text 0x0a61c0  579
 *   QualityDetectV29      .text 0x0a6410  266
 *   EpochDetectV29        .text 0x0a6520   22
 *   GetSNRV29             .text 0x0a6540   23
 *   ScrambleDataV29       .text 0x0a6560   28
 *   SeedScramblerV29      .text 0x0a6580   15
 *   SetEncoderV29         .text 0x0a6590   43
 *   ModDataV29            .text 0x0a65c0   89
 *
 * `include/dsplib/v29fax.h` carries the offset evidence; this file carries
 * the reasoning that is about the CODE.
 *
 * ---------------------------------------------------------------------------
 * THE ORDER OF DEFINITIONS IS THE OBJECT'S, AND IT IS A GUESS ABOUT ONE
 * TRANSLATION UNIT AND NOT A CLAIM ABOUT ELEVEN
 *
 * `tools/tumap.py` brackets these among 95 translation units it cannot
 * separate, so nothing establishes that they were one file.  A RUN of them IS
 * contiguous in the object -- 0x0a5ff0 through 0x0a6618 with no
 * foreign symbol between them, `ModDataV29` now closing that run -- and the
 * others are not: `V29RX_delete` and `V29TX_delete` sit 36 KB earlier, the
 * three half-duplex symbols 8 KB earlier, and the two status fillers and
 * `V29TX_modem` in between.  They are kept together here because they are one
 * layer, and written in ascending address order because emission order is a
 * register-allocation carrier (CLAUDE.md's lever, finding F7796) and the
 * object's own order is the only ordering with any evidence behind it.
 *
 * ---------------------------------------------------------------------------
 * THE AGC'S RETURN VALUE, WHICH IS NOT ONE
 *
 * `DemodDataV29` calls `FPM_AGC_agc` and then uses `%eax`.  `FPM_AGC_agc` is
 * `void` -- measured, not assumed: it takes three arguments (the object reads
 * 0x50, 0x54 and 0x58 of its frame and never 0x5c) and `include/dsplib/
 * fpm_agc.h` declares it that way.  So the calling translation unit declared
 * it as returning `int` while the defining one returned nothing, and what
 * `%eax` actually holds is whatever the definition left there.
 *
 * WHAT IT LEAVES THERE IS `agc->signal`, and that is a property of the object
 * rather than of C: `FPM_AGC_agc` has exactly one `ret`, every path funnels
 * through the same epilogue, and the two instructions before it are
 * `movzbl %dl,%eax` / `mov %eax,0x1c(%edi)` -- the store to `signal` itself.
 *
 * So this file reads the field.  It cannot spell what the object spells,
 * because `fpm_agc.h` is right and a second declaration disagreeing with it
 * would be the "one type, one home" failure in its function-prototype form;
 * and it does not need to, because the two are the same value on every path.
 * `t_v29fax.c` MEASURES that rather than believing it -- it declares
 * `ref_FPM_AGC_agc` as returning `int` and asserts the return equals
 * `agc.signal` over every trial, so if a future blob ever broke the identity
 * the test would say so.  Finding F8875, deviation D1035.
 *
 * ---------------------------------------------------------------------------
 * THE RELOADS ARE FORCED, SO THEY ARE WRITTEN AS RELOADS
 *
 * Every one of these functions re-reads `modem + 0x4c` or `modem + 0x50` after
 * each call rather than keeping it in a register.  That is not a style: a call
 * clobbers memory the compiler cannot see through, so a source that reads the
 * field once could not have produced it.  The `RX()` and `DET()` macros below
 * therefore expand at each use, and the object's reload pattern comes out of
 * the C rather than being imitated.
 */

#include "dsplib/v29fax.h"

#include <stddef.h>

#include "dsplib/debug.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_sdm.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sdm.h"
#include "dsplib/sgd.h"
#include "dsplib/smc.h"
#include "dsplib/sysdep.h"
#include "dsplib/v29data.h"

/* The instance is not modelled; see v29fax.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_INT(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_SHORT(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_USHORT(obj, off)	(*(unsigned short *)(void *)FIELD((obj), (off)))
#define FIELD_BYTE(obj, off)	(*(unsigned char *)FIELD((obj), (off)))

#define RX(modem)		FIELD_PTR((modem), V29_OBJ_RX)
#define DET(modem)		FIELD_PTR((modem), V29_OBJ_DET)

#define RX_AGC(rx)	((struct fpm_agc *)(void *)FIELD((rx), V29RX_AGC))
#define RX_SRE(rx)	((struct fpm_sre *)(void *)FIELD((rx), V29RX_SRE))
#define RX_FSE(rx)	((struct fpm_fse *)(void *)FIELD((rx), V29RX_FSE))
#define RX_MRF(rx)	((struct fpm_mrf *)(void *)FIELD((rx), V29RX_MRF))

/*
 * ---------------------------------------------------------------------------
 * V29RX_delete -- .text 0x09b590, 220 bytes.
 *
 * The object places a literal 1 in the second argument slot before
 * `FPM_FSE_free`, `FPM_SRE_free` and `FPM_MRF_free`.  All three take a single
 * argument -- none of them reads a frame slot past the first -- so it is dead
 * stack setup, presumably left from a version where they took a `fresh` flag
 * like their `_init` counterparts.  Not reproduced, because there is nothing
 * to reproduce; `B103FP_delete` records the identical pattern for the same
 * three-way reason.  Finding F8876.
 *
 * The final free is a sibling `jmp` and is unconditional.
 */
void
V29RX_delete(void *modem)
{
	FPM_FSE_free(RX_FSE(RX(modem)));
	FPM_SRE_free(RX_SRE(RX(modem)));
	FPM_MRF_free(RX_MRF(RX(modem)));

	sysdep_free(FIELD_PTR(RX(modem), V29RX_BUF_SRE));
	sysdep_free(FIELD_PTR(RX(modem), V29RX_BUF_MRF));
	sysdep_free(RX(modem));

	FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(DET(modem), V29DET_MTD));
	FPM_TONE_delete((struct fpm_tone *)FIELD_PTR(DET(modem), V29DET_TONE));
	sysdep_free(FIELD_PTR(DET(modem), V29DET_BUF));
	sysdep_free(FIELD_PTR(DET(modem), V29DET_V21_BUF));
	FPM_MTD_delete((struct fpm_mtd *)FIELD_PTR(DET(modem), V29DET_V21_MTD));
	sysdep_free(DET(modem));

	sysdep_free(modem);
}

/*
 * ---------------------------------------------------------------------------
 * V29TX_delete -- .text 0x09be90, 135 bytes.
 *
 * The transmitter's nine releases.  The private block is re-read from the
 * handle before every one -- five separate `mov 0x24(%ebx),%e?x` between
 * 0x09bea1 and 0x09bed9, then three of `0x20(%ebx)` -- so `V29TX(modem)`
 * expands at each use here as `RX()` and `DET()` do above.
 *
 * The object places a literal 1 in the second argument slot before
 * `FPM_PPS_free` (0x09be91), which takes a single argument and reads no frame
 * slot past the first.  Not reproduced; finding F8876, exactly as for
 * `V29RX_delete`.
 *
 * The final free is a sibling `jmp` and is unconditional, and nothing on the
 * way is guarded; D1150.
 */
void
V29TX_delete(void *modem)
{
	FPM_PPS_free(&V29TX(modem)->pps);

	sysdep_free(V29TX(modem)->ring.sym);
	sysdep_free(V29TX(modem)->ring.q);
	sysdep_free(V29TX(modem)->ring.i);
	sysdep_free(V29TX(modem));

	SGD_delete((struct sgd *)
			FIELD_PTR(FIELD_PTR(modem, V29TX_OBJ_PARAMS),
				  V29TXP_SGD));
	FIFO_delete((struct fax_fifo *)
			FIELD_PTR(FIELD_PTR(modem, V29TX_OBJ_PARAMS),
				  V29TXP_FIFO));
	sysdep_free(FIELD_PTR(modem, V29TX_OBJ_PARAMS));

	sysdep_free(modem);
}

/*
 * ---------------------------------------------------------------------------
 * V29RX_modem -- .text 0x0a3f50, 127 bytes.
 *
 * THE TWO EXTENSIONS OF `*count` ARE BOTH FORCED, AND THEY DISAGREE, WHICH IS
 * WHAT DECIDES THE TYPES.
 *
 *     movswl %cx,%ebx     the value taken BEFORE the call, used as a 32-bit
 *                         subtrahend and as a scaled index -- SIGNED
 *     movzwl %cx,%edx     the value read back AFTER it, used the same way
 *                         -- UNSIGNED
 *
 * One memory location, one call between the two reads, two different
 * extensions of the 32-bit result.  A single declared type cannot produce
 * that, and the spelling that does is the obvious one: `count` points at an
 * `unsigned short` and the saved copy is a `short` local.  Everything else
 * about the loop -- the 16-bit `test %cx,%cx`, the 16-bit store of the total
 * -- is consistent with both and settles nothing.  Finding F8877.
 */
int
V29RX_modem(void *modem, short *in, short *out, unsigned short *count)
{
	short produced = 0;

	FIELD_INT(modem, V29_OBJ_STATUS) &= ~V29_STATUS_ERROR;

	/*
	 * A do-while: the object has no test above the loop head, only
	 * eleven bytes of alignment padding.  A zero count on entry still
	 * dispatches the slot once.
	 */
	do {
		short avail = (short)*count;
		short n;

		n = (*(v29_demod_fn *)(void *)
			FIELD(DET(modem), V29DET_DEMOD))(modem, in, out, count);

		in += avail - *count;
		out += n;
		produced = (short)(produced + n);
	} while (*count != 0);

	*count = (unsigned short)produced;

	return FIELD_INT(modem, V29_OBJ_STATUS);
}

/*
 * ---------------------------------------------------------------------------
 * RxHdxDataV29 -- .text 0x0a3fd0, 226 bytes.
 *
 * The DATA state: demodulate and descramble while the carrier is up, and
 * grade the result.  It is `RxHdxDataV17` and `RxHdxDataV27` instruction for
 * instruction with three offsets changed, and it is NOT `RxHdxDataV21`'s
 * shape -- there is no state advance on either arm.
 *
 * THREE RETURNS FROM THE CALLEES ARE TESTED SIXTEEN BITS WIDE and that is
 * FORCED, not free: `test %ax,%ax` at 0x0a400e on `DataCarrierDetectV29`'s
 * result and `cmp $0x2,%ax` at 0x0a4077 on `QualityDetectV29`'s.  Both are
 * declared `int` in `v29fax.h` and both are read here through a narrowing
 * cast, which is what the object encodes.  The two readings AGREE over every
 * value either function can produce -- the carrier verdict is 0 or 1 and the
 * quality verdict is 0, 1 or 2 -- so the cast changes the instructions and
 * cannot change the answer.  Finding F9257.
 *
 * THE CARRIER BIT IS RAISED UNCONDITIONALLY ON ENTRY and lowered again on the
 * arm where the carrier has gone, which is not the same as assigning it: a
 * caller reading the word between two handlers in one block sees the raised
 * bit.  Same order as `RxHdxDataV21` (0x0a3ff3 before the call, 0x0a401d
 * after it).
 *
 * `V29DET_INT_0008` is the second gate and nothing reconstructed sets it, so
 * the test plants it rather than reaching it.
 *
 * THE UNITS REPORTED ARE ZERO WHEN THE QUALITY VERDICT IS `V29Q_NO_CARRIER`,
 * after `out` has already been written and every filter advanced.  The object
 * computes it `setne`/`movzbl`/`neg`/`and` (0x0a407b..0x0a4087), so there are
 * exactly two outcomes and no third; D1149.
 */
short
RxHdxDataV29(void *modem, short *in, short *out, unsigned short *count)
{
	unsigned short n;
	short units;

	FIELD_INT(modem, V29_OBJ_STATUS) |= V29_STATUS_CARRIER;
	FIELD_BYTE(modem, V29_OBJ_STATUS_B0) = V29RX_STATUS_DATA;

	if ((short)DataCarrierDetectV29(modem, in, *count) == 0
	    || FIELD_INT(DET(modem), V29DET_INT_0008) != 0) {
		FIELD_INT(modem, V29_OBJ_STATUS) &= ~V29_STATUS_CARRIER;
		*count = 0;
		return 0;
	}

	n = DemodDataV29(modem, in, (unsigned short *)(void *)out, *count);
	DescrambleDataV29(modem, (unsigned short *)(void *)out, n);
	*count = 0;

	units = (short)((short)QualityDetectV29(modem) != V29Q_NO_CARRIER
			? (short)n : 0);

	FIELD_INT(modem, V29_OBJ_STATUS) &= ~V29_STATUS_LOW_SNR;
	if (GetSNRV29(modem) <= V29RX_SNR_THRESHOLD)
		FIELD_INT(modem, V29_OBJ_STATUS) |= V29_STATUS_LOW_SNR;

	return units;
}

/*
 * ---------------------------------------------------------------------------
 * RxHdxErrorV29 -- .text 0x0a40c0, 59 bytes.
 *
 * The ERROR state: raise the flag, run the block through the demodulator
 * anyway so the filters keep their history, and consume it.
 *
 * Nothing here advances the state, so once something has installed this
 * handler the machine stays in it until something outside installs another.
 * The flag is a one-shot: `V29RX_modem` clears it at the top of every block,
 * so a caller that does not read the returned word each block loses the
 * event.  `RxHdxErrorV21` is the same function for V.21.
 */
short
RxHdxErrorV29(void *modem, short *in, short *out, unsigned short *count)
{
	FIELD_INT(modem, V29_OBJ_STATUS) |= V29_STATUS_ERROR;

	DemodDataV29(modem, in, (unsigned short *)(void *)out, *count);
	*count = 0;

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * V29RX_status -- .text 0x0a45f0, 190 bytes.
 *
 * The receiver's half of the status report.  It is NOT a mirror of
 * `V29TX_status`: it reads the receive handle, puts the bit rate in +0x04 and
 * +0x12 rather than +0x02 and +0x10, writes +0x0e rather than +0x0c, and
 * rewrites the flags byte four times rather than twice.
 *
 * THE BIT RATE IS LOADED TWICE, from 0x0a4615 and 0x0a4646, with stores to
 * the report in between.  Two statements, not one value used twice -- the
 * same reasoning `V29TX_status` records for its own double load, and forced
 * for the same reason: `status` and `modem` are unrelated parameters.
 *
 * THE FOUR STORES TO THE FLAGS BYTE ARE ALIASING, NOT REDUNDANCY.  The object
 * flushes +0x14 at 0x0a4657, 0x0a466f, 0x0a4685 and 0x0a46a2, and each flush
 * sits immediately before a load through `modem`.  A compiler that could
 * prove the two blocks disjoint would have emitted one store; this one could
 * not, so the intermediate values are visible to a caller that overlaps them.
 * Spelled through `unsigned char` lvalues, which alias everything and give
 * the modern compiler the same reason to keep them.  `t_v29fax.c` drives the
 * overlapping case rather than assuming it away.
 *
 * AND THE VALUE IT READS OUT OF +0x14 IS DEAD.  All EIGHT bits are determined
 * before the function returns -- 0, 2 and 7 cleared, 4 and 6 set, and 1, 3
 * and 5 assigned from fields -- so the final byte does not depend on what the
 * caller had there.  The object reads it anyway, at 0x0a464e, and the read is
 * not removable for the same aliasing reason the stores are not: it is the
 * source of the three intermediate values that reach memory.  So this reads
 * as a merge and behaves as an assignment, which is the opposite of
 * `V29TX_status`, which reads as an assignment after two pointless clears
 * (D1035).  Deviation D1097; finding F9130.
 *
 * BIT 15 OF THE STATUS WORD IS TESTED AS A BYTE.  The object writes `testb
 * $0x80,0x19(%esi)`, which is GCC's narrowing of `& 0x8000` on the `int` at
 * +0x18 -- exactly as `V29RX_modem`'s `andb $0xfd,0x19` is its narrowing of
 * `&= ~0x200`.  The word is spelled as the `int` it is at both sites.
 */
int
V29RX_status(void *modem, void *status)
{
	if (status == 0)
		return 0;

	FIELD_SHORT(status, V29STAT_PROTOCOL) =
		(short)FIELD_USHORT(modem, V29_OBJ_PROTOCOL);
	FIELD_SHORT(status, V29STAT_TX_BPS) = 0;
	FIELD_SHORT(status, V29STAT_ZERO_LO) =
		(short)FIELD_USHORT(modem, V29_OBJ_BITRATE);
	FIELD_SHORT(status, 0x06) = (short)
		((FIELD_INT(modem, V29_OBJ_STATUS) & V29_STATUS_LOW_SNR) == 0);
	FIELD_SHORT(status, 0x08) = GetSNRV29(modem);
	FIELD_SHORT(status, 0x0a) = 0;
	FIELD_SHORT(status, V29STAT_SHORT_0E) = 0;
	FIELD_SHORT(status, V29STAT_SHORT_10) = 0;
	FIELD_SHORT(status, V29STAT_SHORT_12) =
		(short)FIELD_USHORT(modem, V29_OBJ_BITRATE);

	FIELD_BYTE(status, V29STAT_FLAGS) &= (unsigned char)~V29STAT_BIT0;
	FIELD_BYTE(status, V29STAT_FLAGS) = (unsigned char)
		((FIELD_BYTE(status, V29STAT_FLAGS) & ~V29STAT_BIT1)
		 | ((FIELD_BYTE(RX(modem), V29RX_FLAGS_0018)
		     & V29RX_0018_BIT0) << 1));
	FIELD_BYTE(status, V29STAT_FLAGS) &= (unsigned char)~V29STAT_BIT2;
	FIELD_BYTE(status, V29STAT_FLAGS) = (unsigned char)
		((FIELD_BYTE(status, V29STAT_FLAGS) & ~V29STAT_BIT3)
		 | ((FIELD_INT(RX(modem), V29RX_INT_0000) == 0) << 3));
	FIELD_BYTE(status, V29STAT_FLAGS) |= V29STAT_BIT4;
	FIELD_BYTE(status, V29STAT_FLAGS2) &=
		(unsigned char)~V29STAT_FLAGS2_BIT0;
	FIELD_BYTE(status, V29STAT_FLAGS) = (unsigned char)
		((FIELD_BYTE(status, V29STAT_FLAGS) & ~V29STAT_BIT5)
		 | ((FIELD_INT(RX(modem), V29RX_INT_0020) == 0) << 5));
	FIELD_BYTE(status, V29STAT_FLAGS) |= V29STAT_BIT6;
	FIELD_BYTE(status, V29STAT_FLAGS) &= (unsigned char)~V29STAT_BIT7;

	return 1;
}

/*
 * ---------------------------------------------------------------------------
 * V29TX_modem -- .text 0x0a46b0, 182 bytes.  See v29fax.h for the two arms,
 * the budget and why `in` does not advance while `out` does.
 *
 * IT IS `V17TX_modem` AND `V21TX_modem` FOR A THIRD MODULATION, and the
 * object says so: all three are 182 bytes and the instruction sequences differ
 * in four immediates and nothing else -- the gate at params + 0x08, the
 * dispatch slot at + 0x10, the budget 0x30 and the status byte 7.  V.21's are
 * + 0x04, + 0x08, 6 and 4; V.17's are + 0x08, + 0x14, 0x30 and 9.  Finding
 * F9253.
 *
 * The parameter block is read ONCE before the gate and re-read at the top of
 * every loop iteration; the object hoists the first iteration's read out of
 * the loop (0x0a46bf and 0x0a475e reach the head at 0x0a46ea, and the back
 * edge at 0x0a46e7 reloads), which is loop rotation of exactly this source.
 */
int
V29TX_modem(void *modem, unsigned short *in, short *out, unsigned short *count)
{
	void *prm;
	unsigned short taken;
	short budget;
	short total;

	prm = FIELD_PTR(modem, V29TX_OBJ_PARAMS);

	FIELD_BYTE(modem, V29TX_OBJ_RESULT_B1) &=
		(unsigned char)~V29TX_RESULT_B1_BIT1;

	if (FIELD_INT(prm, V29TXP_INT_0008) == 0)
		taken = (unsigned short)FIFO_write(
				(struct fax_fifo *)
					FIELD_PTR(prm, V29TXP_FIFO),
				in, *count);
	else
		taken = *count;

	budget = V29TX_MODEM_BUDGET;
	total = 0;
	do {
		short got;

		prm = FIELD_PTR(modem, V29TX_OBJ_PARAMS);
		got = (*(v29tx_process_fn *)(void *)
				FIELD(prm, V29TXP_PROCESS))
					(modem, in, out, &budget);

		out += got;
		total = (short)(total + got);
	} while (budget > 0);

	if (*count != taken) {
		FIELD_BYTE(modem, V29TX_OBJ_RESULT_B1) |=
			V29TX_RESULT_B1_BIT1;
		/*
		 * A BYTE store into the low byte of the int this function
		 * returns -- `movb $0x7,0x1c(%edi)` at 0x0a4724 -- which is
		 * why it cannot be written through `FIELD_INT`.
		 */
		FIELD_BYTE(modem, V29TX_OBJ_RESULT) = V29TX_RESULT_BYTE_07;
	}

	*count = (unsigned short)total;

	return FIELD_INT(modem, V29TX_OBJ_RESULT);
}

/*
 * ---------------------------------------------------------------------------
 * V29TX_status -- .text 0x0a5030, 100 bytes.
 *
 * THE STORE TO +0x14 HAPPENS TWICE AND THE FIRST ONE IS NOT DEAD.
 *
 * The object clears the low two bits of the report's +0x14, clears bit 0 of
 * its +0x15, then loads the source's +0x10 and stores `& 0x04` over the whole
 * of +0x14 again.  The second store covers the first completely, so a reader
 * expects the compiler to have deleted it -- and it could not, because the
 * load of `src + 0x10` sits BETWEEN them and the two pointers are unrelated
 * parameters.  If the caller ever passed `status = tx + 0x0c` the first store
 * would be visible in what the second one stores.
 *
 * So it is reproduced, spelled through `unsigned char` lvalues, which alias
 * everything and give the modern compiler the same reason to keep it.
 * `V17TX_status` writes byte for byte the same sequence, which is the second,
 * independent statement that this is the author's source and not an artefact.
 * Finding F8878.
 *
 * THE SECOND STORE IS AN ASSIGNMENT AND NOT A MERGE, so every bit the caller
 * had in +0x14 is lost -- including the two the statement above went to the
 * trouble of clearing.  Reproduced; deviation D1035.
 */
int
V29TX_status(void *tx, void *status)
{
	if (status == 0)
		return 0;

	FIELD_SHORT(status, V29STAT_PROTOCOL) = FIELD_SHORT(tx, V29TXS_PROTOCOL);
	FIELD_SHORT(status, V29STAT_TX_BPS) = FIELD_SHORT(tx, V29TXS_BITRATE);
	FIELD_SHORT(status, 0x04) = 0;
	FIELD_SHORT(status, 0x06) = 0;
	FIELD_SHORT(status, 0x08) = 0;
	FIELD_SHORT(status, 0x0a) = 0;
	FIELD_SHORT(status, 0x0c) = 0;
	/*
	 * `tx + 0x02` IS READ TWICE.  The object loads it again at 0xa506a
	 * rather than reusing the copy it made at 0xa5044, which it could only
	 * be forced into by the stores in between -- `status` and `tx` are
	 * unrelated parameters and may overlap.  Two statements, therefore, and
	 * not one value used twice.
	 */
	FIELD_SHORT(status, V29STAT_SHORT_10) = FIELD_SHORT(tx, V29TXS_BITRATE);
	FIELD_SHORT(status, V29STAT_SHORT_12) = 0;

	FIELD_BYTE(status, V29STAT_FLAGS) &= (unsigned char)~V29STAT_FLAGS_LOW2;
	FIELD_BYTE(status, V29STAT_FLAGS2) &= (unsigned char)~V29STAT_FLAGS2_BIT0;
	FIELD_BYTE(status, V29STAT_FLAGS) =
		(unsigned char)(FIELD_BYTE(tx, V29TXS_FLAGS_10) & V29TXS_10_BIT2);

	return 1;
}

/*
 * ---------------------------------------------------------------------------
 * DemodDataV29 -- .text 0x0a5ff0, 398 bytes.
 *
 * One block through the receive chain: gain control, an optional tone pre-pass,
 * resample, symbol recovery, equalise and slice.
 *
 * THE PRE-PASS ABANDONS THE WHOLE CALL.  While the detection block's +0x14 is
 * zero the demodulator halves the input into a scratch buffer, notches a tone
 * out of it and asks the tone detector whether it fired -- and if it did, it
 * returns zero without touching the resampler, the recoverer or the equaliser.
 * The AGC has already run by then and its effect on the caller's buffer stands.
 *
 * THE TWO LOOP COUNTERS IN THIS FILE ARE DIFFERENT TYPES AND THAT IS FORCED.
 * Here the halving loop compares `cmp %di,%dx` with `jb` -- 16 bits, unsigned
 * -- so the index is an `unsigned short`.  In `DataCarrierDetectV29` the copy
 * loop compares `cmp %esi,%edx` with `jl` after a `movswl` -- 32 bits, signed
 * -- so that index is a `short` widened to int.  Two loops over the same
 * `count`, written by the same author, spelled differently; neither reading can
 * be carried to the other.  Finding F8879.
 *
 * THE CARRIER BIT COMES FROM THE FIELD, NOT FROM `%eax`.  See the note at the
 * top of this file, finding F8875 and deviation D1036.
 */
unsigned short
DemodDataV29(void *modem, short *in, unsigned short *out, unsigned short count)
{
	int signal;
	unsigned short n;
	void *rx;

	FPM_AGC_agc(RX_AGC(RX(modem)), in, count);
	/* Not the object's `%eax`; the same value.  D1036. */
	signal = RX_AGC(RX(modem))->signal;

	if (FIELD_SHORT(DET(modem), V29DET_GATE_14) == 0) {
		short *buf = (short *)FIELD_PTR(DET(modem), V29DET_BUF);
		unsigned short i;

		for (i = 0; i < count; i++)
			buf[i] = (short)(in[i] >> 1);

		FPM_TONE_kill((struct fpm_tone *)
				FIELD_PTR(DET(modem), V29DET_TONE),
			      (short *)FIELD_PTR(DET(modem), V29DET_BUF),
			      (short)count);

		if (FPM_MTD_detect((struct fpm_mtd *)
					FIELD_PTR(DET(modem), V29DET_MTD),
				   (const short *)
					FIELD_PTR(DET(modem), V29DET_BUF),
				   (short)count) != 0)
			return 0;
	}

	n = (unsigned short)FPM_MRF_filter(
			RX_MRF(RX(modem)),
			in,
			(short *)FIELD_PTR(RX(modem), V29RX_BUF_MRF),
			(short)count);

	rx = RX(modem);
	RX_SRE(rx)->adapt = signal & FIELD_INT(rx, V29RX_INT_0004);

	n = FPM_SRE_recover(RX_SRE(RX(modem)),
			    (const short *)FIELD_PTR(RX(modem), V29RX_BUF_MRF),
			    (short *)FIELD_PTR(RX(modem), V29RX_BUF_SRE),
			    (short)n);

	if (n > V29RX_SRE_MAX && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("ERROR: SRE buffer violation(%d)", n);

	rx = RX(modem);
	RX_FSE(rx)->tilt_on = 0;
	RX_FSE(rx)->pll_on = signal & FIELD_INT(rx, V29RX_INT_0008);
	RX_FSE(rx)->lms_on = signal & FIELD_INT(rx, V29RX_INT_0020);

	return FPM_FSE_receive(RX_FSE(RX(modem)),
			       (const short *)
				FIELD_PTR(RX(modem), V29RX_BUF_SRE),
			       out, n);
}

/*
 * ---------------------------------------------------------------------------
 * DescrambleDataV29 -- .text 0x0a6180, 30 bytes.
 *
 * Two instructions and a tail jump: pick the `fpm_sdm` out of the receive
 * block and hand the caller's buffer and count straight to `SDM_descrambler`.
 *
 * NO INTERMEDIATE LOCAL, and that is measured rather than a style choice --
 * `ScrambleDataV22` and `DescrambleDataV22` enumerated seven spellings and
 * exactly one reproduces them, the one with no local (finding F8120).  The
 * local costs the register: with it GCC puts the sub-object pointer in %edx
 * and pays the six-byte `add $imm32,%edx`, and the object has the five-byte
 * `add $imm32,%eax` here at 0x0a6190.  The same evidence, in the same form,
 * at a fourth site.
 *
 * IT JUMPS TO `SDM_descrambler`, NOT TO `FPM_SDM_descrambler`.  The two are
 * byte for byte the same code at two addresses (`include/dsplib/sdm.h`), and
 * which one a call site names is settled by the relocation and not by which
 * would work -- 0x0a6199 carries `R_386_PC32 SDM_descrambler`.
 */
void
DescrambleDataV29(void *modem, unsigned short *data, unsigned short count)
{
	SDM_descrambler((struct fpm_sdm *)(void *)
			FIELD(FIELD_PTR(modem, V29_OBJ_RX), V29RX_SDM),
			data, count);
}

/*
 * ---------------------------------------------------------------------------
 * CarrierDetectV29 -- .text 0x0a61a0, 22 bytes.
 *
 * The AGC's own signal bit, gated.  Both loads are 32 bits wide, which is what
 * makes `rx + 0x80` an `int` and therefore `struct fpm_agc`'s `signal` rather
 * than a `short` of its own: a 16-bit field could not be read with a 32-bit
 * `mov` at all.
 */
int
CarrierDetectV29(void *modem)
{
	void *rx = RX(modem);

	return RX_SRE(rx)->active & RX_AGC(rx)->signal;
}

/*
 * ---------------------------------------------------------------------------
 * DataCarrierDetectV29 -- .text 0x0a61c0, 579 bytes.
 *
 * Three questions, and the answer to any one of them can be the answer.
 *
 *   1. the carrier bit, masked down to its low bit once the receiver has been
 *      running long enough (rx + 0x46 past 999) and zeroed outright if the
 *      decoder's error is over 0x3fff;
 *   2. the V.21 scan, which runs only once the detection block's +0x2a has
 *      latched, and which reports NO data carrier as soon as it has seen
 *      0x4ff samples of V.21 -- that is the branch the author's own
 *      "V29: V21 Carrier detected" message sits in;
 *   3. the energy-drop check, which is gated on rx + 0x4f64 and reports no
 *      carrier when this block's RMS falls below 0.39984 of the reference.
 *
 * THE IDIOM `x & (err <= limit)` OCCURS TWICE AND IS WHAT THE BRANCHES SAY.
 * The object does not compute a boolean and AND it; it branches on the
 * comparison and either zeroes the value or masks it with 1.  That is exactly
 * how GCC compiles `x &= (a <= b)` when it cannot prove `x` is already 0 or 1,
 * and the two sites do it the same way round.  Written as the object's
 * branches would be a different function with the same behaviour; written as
 * the idiom it is one statement in each place.
 */
int
DataCarrierDetectV29(void *modem, short *in, unsigned short count)
{
	void *rx = RX(modem);
	void *det = DET(modem);
	int detected = (short)(RX_SRE(rx)->active & RX_AGC(rx)->signal);
	short rms;

	if (FIELD_SHORT(det, V29DET_GATE_1C) == 0) {
		if (FIELD_SHORT(rx, V29RX_SHORT_0046) > 999)
			detected &= RX_FSE(rx)->mse
					<= V29RX_DEC_ERROR_MAX;

		if (RX_FSE(rx)->mse > V29RX_DEC_ERROR_MAX
		    && DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"V29 Decoder error too big... no carrier\n");
	} else {
		if ((detected & (RX_FSE(rx)->mse
				 <= V29RX_DEC_ERROR_MAX)) == 0)
			FIELD_SHORT(det, V29DET_V21_ENABLE) = 1;

		detected = 1;

		if (FIELD_SHORT(det, V29DET_V21_ENABLE) != 0) {
			short *buf = (short *)FIELD_PTR(det, V29DET_V21_BUF);
			short i;

			for (i = 0; i < (int)count; i++)
				buf[i] = in[i];

			FPM_AGC_agc((struct fpm_agc *)(void *)
					FIELD(det, V29DET_V21_AGC),
				    (short *)FIELD_PTR(det, V29DET_V21_BUF),
				    count);

			det = DET(modem);
			if (FPM_MTD_detect((struct fpm_mtd *)
						FIELD_PTR(det, V29DET_V21_MTD),
					   (const short *)
						FIELD_PTR(det, V29DET_V21_BUF),
					   (short)count) != 0)
				FIELD_SHORT(DET(modem), V29DET_V21_SAMPLES) = 0;
			else
				FIELD_SHORT(det, V29DET_V21_SAMPLES) =
					(short)(FIELD_SHORT(det,
						V29DET_V21_SAMPLES) + count);

			if (FIELD_SHORT(det, V29DET_V21_SAMPLES)
			    > V29DET_V21_THRESHOLD) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"V29: V21 Carrier detected\n");
				detected = 0;
			}

			rx = RX(modem);
		}
	}

	if (FIELD_SHORT(rx, V29RX_SHORT_4F64) == 0)
		return detected;

	rms = FPM_rms(in, count);
	rx = RX(modem);

	if (rms < ((FIELD_SHORT(rx, V29RX_RMS_REF) * V29RX_RMS_DROP_Q15)
		   >> V29RX_RMS_SHIFT)) {
		detected = 0;
		if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(
				"sudden energy drop > 8[dB], no carrier");
			rx = RX(modem);
		}
	}

	if ((short)(FIELD_SHORT(rx, V29RX_RMS_N) + 1) == 2) {
		FIELD_SHORT(rx, V29RX_RMS_REF) = rms;
		FIELD_SHORT(rx, V29RX_RMS_N) = 0;
	} else {
		FIELD_SHORT(rx, V29RX_RMS_N) =
			(short)(FIELD_SHORT(rx, V29RX_RMS_N) + 1);
	}

	return detected;
}

/*
 * ---------------------------------------------------------------------------
 * QualityDetectV29 -- .text 0x0a6410, 266 bytes.
 *
 * Folds this block's decoder error into a running average and reports the
 * carrier bit, or 2 when the carrier bit is clear.
 *
 * The counter runs 0, 1, 2 ... and does three different things:
 *
 *   0            seed the average with this block's error outright
 *   1 .. 0x31    fold it in at one part in ten
 *   0x32         test the average against the limit ONCE, then stop
 *   0x33 and up  nothing at all -- neither the average nor the counter moves
 *
 * so the average is a fifty-block measurement taken once per carrier and then
 * frozen, not a filter that runs for ever.
 */
int
QualityDetectV29(void *modem)
{
	void *rx = RX(modem);
	int err = RX_FSE(rx)->mse;
	int verdict = (short)(RX_SRE(rx)->active & RX_AGC(rx)->signal);
	short n;

	if (verdict == 0) {
		if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(
				"V29 Dec error too big... unreliable data\n");
			rx = RX(modem);
		}
		verdict = V29Q_NO_CARRIER;
	}

	n = FIELD_SHORT(rx, V29RX_DEC_ERROR_N);
	if (n == 0) {
		FIELD_SHORT(rx, V29RX_DEC_ERROR_AVG) = (short)err;
		FIELD_SHORT(rx, V29RX_DEC_ERROR_N) = 1;
		return verdict;
	}

	if (n > V29Q_AVG_BLOCKS) {
		if (n != V29Q_VERDICT_BLOCK)
			return verdict;
		if (!(FIELD_SHORT(rx, V29RX_DEC_ERROR_AVG)
		      > FIELD_SHORT(rx, V29RX_DEC_ERROR_LIMIT)))
			FIELD_SHORT(rx, V29RX_SHORT_4F62) = 1;
	} else {
		FIELD_SHORT(rx, V29RX_DEC_ERROR_AVG) = (short)
			(((FIELD_SHORT(rx, V29RX_DEC_ERROR_AVG) * V29Q_AVG_KEEP
			   + V29Q_AVG_ROUND) >> V29Q_AVG_SHIFT)
			 + ((err * V29Q_AVG_NEW + V29Q_AVG_ROUND)
			    >> V29Q_AVG_SHIFT));
	}

	FIELD_SHORT(rx, V29RX_DEC_ERROR_N) =
		(short)(FIELD_SHORT(rx, V29RX_DEC_ERROR_N) + 1);

	return verdict;
}

/*
 * ---------------------------------------------------------------------------
 * EpochDetectV29 -- .text 0x0a6520, 22 bytes.
 */
int
EpochDetectV29(void *modem)
{
	return RX_FSE(RX(modem))->lms_force != 0;
}

/*
 * ---------------------------------------------------------------------------
 * GetSNRV29 -- .text 0x0a6540, 23 bytes.
 *
 * The constant is 14; `GetSNRV17`'s is 13.  The object loads the error
 * `movzwl` where `QualityDetectV29` loads the same field `movswl`, and that
 * disagreement is FREE rather than forced: the difference is truncated to 16
 * bits by a `cwtl` before it leaves, so the upper half never reaches anything.
 * Finding F614's case, and the field's type is settled by the site where it is
 * NOT free.
 */
short
GetSNRV29(void *modem)
{
	return (short)(14 - RX_FSE(RX(modem))->mse);
}

/*
 * ---------------------------------------------------------------------------
 * ScrambleDataV29 -- .text 0x0a6560, 28 bytes.
 *
 * The transmit half of the pair, reaching the TRANSMIT block (`V29_OBJ_TX`,
 * +0x24) where its sibling reaches the receive one.  Same shape, same "no
 * intermediate local" ruling, and `R_386_PC32 SDM_scrambler` at 0x0a6577.
 *
 * Two bytes shorter than the receive one for one reason and it is not a
 * difference in the source: `add $0x1c,%eax` has an eight-bit displacement
 * and `add $0x4f3c,%eax` does not.
 */
void
ScrambleDataV29(void *modem, unsigned short *data, unsigned short count)
{
	SDM_scrambler((struct fpm_sdm *)(void *)
		      FIELD(FIELD_PTR(modem, V29_OBJ_TX), V29TX_SDM),
		      data, count);
}

/*
 * ---------------------------------------------------------------------------
 * SeedScramblerV29 -- .text 0x0a6580, 15 bytes.
 */
void
SeedScramblerV29(void *modem, int seed)
{
	FIELD_INT(V29TX(modem), V29TXFP_SCRAMBLER_SEED) = seed;
}

/*
 * ---------------------------------------------------------------------------
 * SetEncoderV29 -- .text 0x0a6590, 43 bytes.
 *
 * `which` is loaded `movswl` and the 32-bit result is compared and
 * decremented, so `short` is forced.  Only 0 and 1 write anything; the object
 * tests for each in turn and returns.
 *
 * WHAT IT WRITES IS `fpm_smc_cfg`'s `direct`, and that is not inference: the
 * transmitter's block puts `struct fpm_smc` at +0x34 (`v29data.h`, confirmed
 * by `V29TX_create` calling `SMC_init` there) and `direct` is that structure's
 * +0x04, so +0x38 is the one field and there is nothing to choose between.
 * `fpm_smc.h` describes it as "take the quadrant straight out of the data word
 * instead of accumulating pmap increments" -- differential encoding off.
 * Finding F8880.
 */
void
SetEncoderV29(void *modem, short which)
{
	if (which == 0)
		V29TX(modem)->smc.cfg.direct = 0;
	else if (which == 1)
		V29TX(modem)->smc.cfg.direct = 1;
}

/*
 * ---------------------------------------------------------------------------
 * ModDataV29 -- .text 0x0a65c0, 89 bytes.
 *
 * One block through the transmit chain: encode the caller's data words into
 * the ring, then shape the ring into samples.
 *
 * `count` GOES TO BOTH CALLS UNCHANGED and is not the same unit in each --
 * `SMC_encoder` takes data words and `FPM_PPS_filter` takes symbols.  The
 * object holds it in `%ebx` across both and stores it into `0xc(%esp)` twice
 * (0x0a65d6 and 0x0a65ef), so there is no conversion to reproduce.
 * `ModDataV27` is the same function over V.27ter's block.
 *
 * The block is re-read from the handle BETWEEN the two calls -- `mov
 * 0x24(%esi),%eax` at 0x0a65da and again at 0x0a65fb -- and the ring is taken
 * from it twice, once per call.  That is the reload pattern of every function
 * in this file and it is what the two locals below spell.
 *
 * THE THREE OFFSETS ARE `v29data.h`'s AND THEY CORROBORATE `V29TX_delete`.
 * `SMC_encoder` types +0x34 and `FPM_PPS_filter` types +0x64, and the delete
 * above releases +0x64 through `FPM_PPS_free` and the ring's three rails --
 * two readings of one block from two functions, neither derived from the
 * other.  Finding F9255.
 */
unsigned short
ModDataV29(void *modem, const unsigned short *bits, short *samples,
	   unsigned short count)
{
	struct v29tx *tx;

	tx = V29TX(modem);
	SMC_encoder(&tx->smc, &tx->ring, bits, count);

	tx = V29TX(modem);
	return FPM_PPS_filter(&tx->pps, &tx->ring, samples, count);
}

/*
 * The offsets this file states independently, checked against the one struct
 * it borrows.  `__SIZEOF_POINTER__` is a GCC 4.6+ predefine, so under the
 * period compiler this reads `#if 0` and the assertions vanish -- see
 * docs/method/compilers.md; that is the tree's established idiom and not an
 * oversight here.
 */
#if __SIZEOF_POINTER__ == 4
typedef char v29fax_agc_signal[
	(V29RX_AGC + (int)offsetof(struct fpm_agc, signal)
	 == V29RX_AGC_SIGNAL) ? 1 : -1];
typedef char v29fax_sre_active[
	(V29RX_SRE + (int)offsetof(struct fpm_sre, active)
	 == V29RX_SRE_ACTIVE) ? 1 : -1];
typedef char v29fax_sre_adapt[
	(V29RX_SRE + (int)offsetof(struct fpm_sre, adapt)
	 == V29RX_SRE_ADAPT) ? 1 : -1];
typedef char v29fax_fse_lms_force[
	(V29RX_FSE + (int)offsetof(struct fpm_fse, lms_force)
	 == V29RX_FSE_LMS_FORCE) ? 1 : -1];
typedef char v29fax_fse_pll_on[
	(V29RX_FSE + (int)offsetof(struct fpm_fse, pll_on)
	 == V29RX_FSE_PLL_ON) ? 1 : -1];
typedef char v29fax_fse_tilt_on[
	(V29RX_FSE + (int)offsetof(struct fpm_fse, tilt_on)
	 == V29RX_FSE_TILT_ON) ? 1 : -1];
typedef char v29fax_fse_lms_on[
	(V29RX_FSE + (int)offsetof(struct fpm_fse, lms_on)
	 == V29RX_FSE_LMS_ON) ? 1 : -1];
typedef char v29fax_fse_mse[
	(V29RX_FSE + (int)offsetof(struct fpm_fse, mse)
	 == V29RX_FSE_MSE) ? 1 : -1];
/*
 * And that the sub-objects do not overlap: the SRE ends where 0x120 begins
 * and the FSE ends below the two buffers.  Without this the eight readings
 * above would be arithmetic rather than layout.
 */
typedef char v29fax_sre_fits[
	(V29RX_SRE + (int)sizeof(struct fpm_sre) <= V29RX_FSE) ? 1 : -1];
typedef char v29fax_fse_fits[
	(V29RX_FSE + (int)sizeof(struct fpm_fse) <= V29RX_BUF_MRF) ? 1 : -1];
typedef char v29fax_agc_fits[
	(V29RX_AGC + (int)sizeof(struct fpm_agc) <= V29RX_SRE) ? 1 : -1];
typedef char v29fax_smc_direct[
	(V29FP_SMC + (int)offsetof(struct fpm_smc, cfg)
	 + (int)offsetof(struct fpm_smc_cfg, direct) == 0x38) ? 1 : -1];
typedef char v29fax_tx_fp[(V29_OBJ_TX == V29TX_OBJ_FP) ? 1 : -1];
#endif
