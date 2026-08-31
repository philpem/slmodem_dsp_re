/*
 * v29.c -- ITU-T V.29 (fax): the receiver's entry points, and the two
 *          transmitter accessors that sit in the same run of addresses.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V29RX_delete          .text 0x09b590  220
 *   V29RX_modem           .text 0x0a3f50  127
 *   V29TX_status          .text 0x0a5030  100
 *   DemodDataV29          .text 0x0a5ff0  398
 *   CarrierDetectV29      .text 0x0a61a0   22
 *   DataCarrierDetectV29  .text 0x0a61c0  579
 *   QualityDetectV29      .text 0x0a6410  266
 *   EpochDetectV29        .text 0x0a6520   22
 *   GetSNRV29             .text 0x0a6540   23
 *   SeedScramblerV29      .text 0x0a6580   15
 *   SetEncoderV29         .text 0x0a6590   43
 *
 * `include/dsplib/v29fax.h` carries the offset evidence; this file carries
 * the reasoning that is about the CODE.
 *
 * ---------------------------------------------------------------------------
 * THE ORDER OF DEFINITIONS IS THE OBJECT'S, AND IT IS A GUESS ABOUT ONE
 * TRANSLATION UNIT AND NOT A CLAIM ABOUT ELEVEN
 *
 * `tools/tumap.py` brackets these among 95 translation units it cannot
 * separate, so nothing establishes that they were one file.  Eight of the
 * eleven ARE contiguous in the object -- 0x0a5ff0 through 0x0a65bb with no
 * foreign symbol between them -- and the other three are not: `V29RX_delete`
 * sits 36 KB earlier, `V29RX_modem` 8 KB earlier and `V29TX_status` 4 KB
 * earlier.  They are kept together here because they are one layer, and
 * written in ascending address order because emission order is a register-
 * allocation carrier (CLAUDE.md's lever, finding F7796) and the object's own
 * order is the only ordering with any evidence behind it.
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
#include "dsplib/fpm.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"
#include "dsplib/v29data.h"

/* The instance is not modelled; see v29fax.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_INT(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_SHORT(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
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

	FIELD_INT(modem, V29_OBJ_STATUS) &= ~V29_STATUS_0200;

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
