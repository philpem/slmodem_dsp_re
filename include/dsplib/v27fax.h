/*
 * v27fax.h -- ITU-T V.27ter (fax): the receiver's primitives, and the
 * transmitter's status filler.
 *
 * Eleven functions sit directly on the FPM layer here.  Ten of them reach
 * into the V.27ter modem instance, pick a sub-object out of it, and either
 * hand that sub-object to the module that owns it or read one field back out;
 * one is the equaliser's slicer, and one is a constant.  The last two --
 * `DemodDataV27` -- is not reconstructed yet and is marked below.
 *
 *   GetSNRV27             a constant
 *   V27RX_status          "did the caller supply somewhere to write"
 *   EpochDetectV27        one flag out of the equaliser
 *   CarrierDetectV27      the AGC's gate AND the symbol recovery's
 *   V27TX_status          fill a status block from the transmitter's
 *   V27RX_modem           drive the half-duplex receive state handler
 *   V27RX_delete          release the receiver
 *   V27RX_decision        the equaliser's slicer: nearest DPSK phase
 *   QualityDetectV27      smooth the equaliser's MSE and grade it
 *   DataCarrierDetectV27  carrier up/down, and the V.21 escape
 *   DemodDataV27          AGC -> resample -> symbol recovery -> equalise  [-]
 *
 * `tools/service.py` puts all eleven on the FAX side.
 *
 * ---------------------------------------------------------------------------
 * THE INSTANCE IS NOT MODELLED, and this header follows the ruling
 * `include/dsplib/v17data.h` and `v22data.h` set out: `V27RX_create` -- 2,210
 * bytes that lay the instance out -- is not reconstructed, so naming its
 * fields now would mean guessing.  The parameter is `void *` and every offset
 * is a named constant below with the evidence beside it.
 *
 * WHAT IS DIFFERENT HERE, AND IT IS THE STRONGEST EVIDENCE IN THIS FILE: the
 * receiver block is not opaque all the way down.  Four of the library's own
 * modules are EMBEDDED in it at fixed offsets, and `V27RX_delete` names three
 * of them by handing their addresses to `FPM_MRF_free`, `FPM_SRE_free` and
 * `FPM_FSE_free` -- so those regions are `struct fpm_mrf`, `struct fpm_sre`
 * and `struct fpm_fse` because a callee types them, not because a layout
 * looked plausible.  The fourth, the AGC, is named the same way by
 * `DemodDataV27`'s `FPM_AGC_agc(rx + 0x68, ...)`.
 *
 * That closes the block arithmetically, which is why the four offsets can be
 * trusted rather than merely recorded:
 *
 *     0x4c + sizeof(struct fpm_mrf) == 0x4c + 0x1c == 0x68   the AGC
 *     0x68 + sizeof(struct fpm_agc) == 0x68 + 0x2c == 0x94   the SRE
 *     0x94 + sizeof(struct fpm_sre) == 0x94 + 0x90 == 0x124  the FSE
 *     0x124 + sizeof(struct fpm_fse) == 0x124 + 0x4e18 == 0x4f3c
 *
 * and 0x4f3c is where the two scratch pointers `V27RX_delete` frees begin,
 * rounded up to 0x4f40.  Four independent modules tile the block with no gap
 * and no overlap.  Everything the five reading functions touch between 0x4c
 * and 0x4f3c therefore lands on a NAMED FIELD of one of those four structures,
 * and this header states which:
 *
 *     rx + 0x84   fpm_agc::signal     (0x68 + 0x1c)
 *     rx + 0xd4   fpm_sre::active     (0x94 + 0x40)
 *     rx + 0xdc   fpm_sre::adapt      (0x94 + 0x48)
 *     rx + 0x164  fpm_fse::lms_force  (0x124 + 0x40)
 *     rx + 0x168  fpm_fse::pll_on     (0x124 + 0x44)
 *     rx + 0x16c  fpm_fse::tilt_on    (0x124 + 0x48)
 *     rx + 0x170  fpm_fse::lms_on     (0x124 + 0x4c)
 *     rx + 0x176  fpm_fse::mse        (0x124 + 0x52)
 *
 * The last one is corroborated by the author's own words: `DataCarrierDetectV27`
 * compares rx + 0x176 against 0x3fff and prints "V27 Decoder error too big...
 * no carrier" when it is larger, and `fpm_fse.h` already calls +0x52 the
 * smoothed squared decision error.  Evidence class 1 and class 2 agreeing
 * without either being derived from the other.
 *
 * ---------------------------------------------------------------------------
 * THE DECODER BLOCK IS CONFIRMED TWICE TOO
 *
 * `V27RX_decision` reads `state->cfg.owner` and works relative to that.
 * `V27RX_create` sets `cfg.owner` to rx + 0x14 (99b65: `mov 0x54(%ebp),%edi`
 * then `add $0x14,%edi`, stored at the stack cfg's +0x2c) and fills the same
 * fields at rx-relative addresses, so every offset below is confirmed by the
 * function that reads it AND by the one that writes it (addresses are into
 * dsplibs.o):
 *
 *   99c54  rx + 0x20 = (params->f8 == 1)          -> dec + 0x0c
 *   99c6c  rx + 0x28 = V27RX_DEC_PHS_MASK[rate]   -> dec + 0x14
 *   99c2b  rx + 0x2a = 0                          -> dec + 0x16
 *   99ca4  rx + 0x2c = V27RX_DEC_PMAP[rate]       -> dec + 0x18
 *   99c7d  rx + 0x32 = 0x3299                     -> dec + 0x1e
 *   99cbf  rx + 0x34 = V27RX_DEC_LAST_PHASE[rate] -> dec + 0x20
 *   99c97  rx + 0x3a = 0                          -> dec + 0x26
 *
 * `V27RX_DEC_PMAP` and `V27RX_DEC_LAST_PHASE` are each two pointers indexed by
 * the rate, and the tables they point at are 4 entries for 2400 and 8 for
 * 4800 -- which is exactly the 4-or-8 `V27RX_decision` selects on dec + 0x0c.
 */

#ifndef DSPLIB_V27FAX_H
#define DSPLIB_V27FAX_H

struct fpm_fse;

/* ------------------------------------------------------------------ */
/* The modem instance                                                  */

/*
 * The two sub-objects the instance owns, both pointers.  `V27RX_delete`
 * releases exactly these two and then the instance, which is the best single
 * witness to the layout there is without `V27RX_create`.
 */
#define V27_OBJ_SHARED		0x50	/* the half-duplex/detect block  */
#define V27_OBJ_RX		0x54	/* the receiver block            */

/*
 * The status word `V27RX_modem` returns, and the flags byte inside it.
 *
 * ONE 32-BIT WORD, NOT TWO FIELDS: `V27RX_create` zeroes all four bytes at
 * +0x1c with a single `movl $0x0,0x1c(%ebp)`, then ORs 0x50 into the byte at
 * +0x1d and stores 2 into the byte at +0x1c -- so +0x1d is byte 1 of the word
 * at +0x1c.  `V27RX_modem` clears one bit of that byte and returns the whole
 * word.
 */
#define V27_OBJ_STATUS		0x1c	/* int, returned by V27RX_modem  */
#define V27_OBJ_STATUS_FLAGS	0x1d	/* byte 1 of the word above      */

/*
 * The bit `V27RX_modem` clears on entry, `andb $0xfd,0x1d(%eax)`.
 *
 * NAMED NEUTRALLY AND DELIBERATELY.  What is established is the bit's
 * position and that entering the receive loop clears it; nothing traced sets
 * it, so what it announces is not known.  `V27RX_create`'s `orb $0x50` sets
 * bits 4 and 6 of the same byte and leaves this one clear.
 */
#define V27_STATUS_FLAG_02	0x02

/* ------------------------------------------------------------------ */
/* The receiver block, at *(void **)(obj + V27_OBJ_RX)                  */

/*
 * Three caller-owned enables, each ANDed with the AGC's signal gate by
 * `DemodDataV27` and stored into one named flag of one sub-object.  The role
 * is not inferred from the name -- there is none -- it is read off the
 * destination:
 *
 *     rx + 0x04 & signal  ->  fpm_sre::adapt     phase update
 *     rx + 0x08 & signal  ->  fpm_fse::pll_on    carrier recovery
 *     rx + 0x10 & signal  ->  fpm_fse::lms_on    coefficient adaptation
 *
 * `V27RX_create` sets all three to 1 at 99ceb..99d06, together with a fourth
 * at +0x0c set to 0 which nothing in this file reads.
 */
#define V27RX_EN_SRE_ADAPT	0x04
#define V27RX_EN_FSE_PLL	0x08
#define V27RX_EN_FSE_LMS	0x10

/* The decoder block; see the header comment. */
#define V27RX_DEC		0x14

/* The four embedded FPM modules.  They tile 0x4c..0x4f3c exactly. */
#define V27RX_MRF		0x4c	/* struct fpm_mrf   */
#define V27RX_AGC		0x68	/* struct fpm_agc   */
#define V27RX_SRE		0x94	/* struct fpm_sre   */
#define V27RX_FSE		0x124	/* struct fpm_fse   */

/*
 * The two scratch buffers, allocated by `V27RX_create` and released by
 * `V27RX_delete`.  `DemodDataV27` uses A for the resampler's output and the
 * symbol recovery's input, and B for the symbol recovery's output and the
 * equaliser's input -- so B is the one the equaliser reads and A is the one
 * everything upstream shares.
 */
#define V27RX_BUF_A		0x4f40	/* short *  */
#define V27RX_BUF_B		0x4f44	/* short *  */

/*
 * `QualityDetectV27`'s own state, and it is the only reader or writer of all
 * four.  Their ROLES are measured -- what each one does is below -- and their
 * MEANINGS are usage inference, so the names say what happens rather than what
 * it is for.
 *
 * `q_acc` is a first-order smoother over `fpm_fse::mse` at 0.9/0.1 in Q15
 * with rounding; `q_count` counts the blocks that have gone into it and stops
 * the smoother at 0x31; at exactly 0x32 the accumulated value is compared
 * against `q_limit` and `q_flag` is set if it did NOT exceed it.
 */
#define V27RX_Q_ACC		0x4f48	/* short            */
#define V27RX_Q_COUNT		0x4f4a	/* unsigned short   */
#define V27RX_Q_LIMIT		0x4f4c	/* unsigned short   */
#define V27RX_Q_FLAG		0x4f4e	/* short            */

/*
 * The energy-drop detector's state, read and written only by
 * `DataCarrierDetectV27`, and named from the author's own words: the branch
 * that clears carrier prints "sudden energy drop > 8[dB], no carrier", and the
 * comparison it prints on is `FPM_rms(block) < (ref * 0x32fe) >> 15`.
 * 0x32fe / 32768 is 0.3984, and 20*log10(0.3984) is -8.0 dB -- so the constant
 * and the string agree to the digit the string quotes.
 *
 * `V27RX_create` sets `enable` and `ref` together at 99c1d.
 */
#define V27RX_RMS_ON		0x4f50	/* short: run the detector at all */
#define V27RX_RMS_REF		0x4f52	/* short: the level to fall from  */
#define V27RX_RMS_COUNT		0x4f54	/* unsigned short: blocks since
					 * `ref` was last republished     */

/* The -8 dB threshold, Q15.  See V27RX_RMS_REF. */
#define V27RX_RMS_DROP_Q15	0x32fe

/*
 * The two `fpm_fse::mse` thresholds, both compared with `>`.
 *
 * 0x3fff is the carrier test and the author names it twice over: above it
 * `DataCarrierDetectV27` prints "V27 Decoder error too big... no carrier".
 */
#define V27RX_MSE_NO_CARRIER	0x3fff

/*
 * How many decisions must have been taken before the mse test is allowed to
 * drop carrier, compared against the decoder's own symbol counter.
 *
 * NEUTRAL ON PURPOSE: what is established is that `DataCarrierDetectV27`
 * gates on `dec->sym_count > 0x5db` and that `V27RX_decision` is the only
 * thing that advances that counter.  Whether 1500 symbols is a training
 * length, a timeout or something else is not read off anything.
 */
#define V27RX_DEC_SETTLED	0x5db

/* ------------------------------------------------------------------ */
/* The decoder block, at rx + V27RX_DEC (and at fse->cfg.owner)          */

/*
 * Four phases or eight.
 *
 * `V27RX_decision` computes `n = dec->eight_phase ? 8 : 4` -- `cmp $0x1,%ebp;
 * sbb %ebx,%ebx; and $0xfffffffc,%ebx; add $0x8,%ebx`, which is GCC's
 * branchless form of a two-constant conditional and sets the mask on
 * `%ebp == 0` alone.  `V27RX_create` fills it with `params->f8 == 1`, and the
 * same `params->f8` indexes `V27RX_DEC_PMAP` and `V27RX_DEC_LAST_PHASE`,
 * whose entry 1 points at eight-entry tables and whose entry 0 points at
 * four-entry ones.  So the field is the rate and the count follows it.
 */
#define V27DEC_EIGHT_PHASE	0x0c	/* int    */

/*
 * The phase index mask, `V27RX_DEC_PHS_MASK[rate]`.  3 or 7 for the two
 * table lengths above; the object never assumes that and neither does this.
 */
#define V27DEC_PHASE_MASK	0x14	/* unsigned short */

/* The previous symbol's phase index, the DPSK reference. */
#define V27DEC_LAST		0x16	/* short */

/* `V27RX_DEC_PMAP[rate]`: the bits each phase STEP carries. */
#define V27DEC_PMAP		0x18	/* const short * */

/* `V27RX_DEC_LAST_PHASE[rate]`: the phase angle of each index. */
#define V27DEC_ANGLES		0x20	/* const short * */

/*
 * Decisions taken, saturating.  `V27RX_decision` increments it and, when the
 * increment would reach 0x8000, stores 0x4000 instead -- so it never goes
 * negative and never stops moving.  `DataCarrierDetectV27` is its only reader.
 */
#define V27DEC_SYM_COUNT	0x26	/* unsigned short */

/*
 * The magnitude every decision reports, a literal in `V27RX_decision`.
 *
 * `V27RX_create` also writes 0x3299 into dec + 0x1e and nothing reconstructed
 * reads that field, so the two are independent occurrences of one number
 * rather than one field read twice -- which is what makes the value itself
 * evidence and not a coincidence of the disassembly.
 */
#define V27DEC_MAG		0x3299

/*
 * The full circle, in the units `fpm_fse` hands the slicer.
 *
 * `FPM_atan`'s output convention, restated by this function three times: the
 * phase difference is folded into [0, 0x8000] by adding 0x8000 when it is
 * negative and subtracting 0x8000 when it exceeds 0x8000.
 */
#define V27DEC_PHASE_FULL	0x8000

/* ------------------------------------------------------------------ */
/* The shared block, at *(void **)(obj + V27_OBJ_SHARED)                */

/*
 * The half-duplex receive machine and the tone detectors live together here.
 * `V27RX_delete` releases +0x00, +0x18 and +0x1c and then the block itself,
 * which fixes what it owns; `V27RX_modem` calls through +0x0c; and
 * `V27RX_create` stores `RxHdxStartV27` into +0x0c at 996f9, which is what
 * makes +0x0c the CURRENT STATE HANDLER rather than a vtable slot.
 */
#define V27SH_MTD		0x00	/* struct fpm_mtd *, deleted first */
#define V27SH_STATE		0x0c	/* v27_rx_state_fn                 */

/*
 * Two guards, both compared against zero as 16-bit values and neither written
 * by anything reconstructed.  Named for what they GATE, which is all that is
 * established: +0x10 skips `DemodDataV27`'s tone test, +0x14 selects which
 * half of `DataCarrierDetectV27` runs.
 */
#define V27SH_SKIP_TONE		0x10	/* unsigned short */
#define V27SH_V21_WATCH		0x14	/* unsigned short */

/*
 * The second tone detector and the buffer `DataCarrierDetectV27` copies into
 * before gain-controlling it.  The detector is the one whose success prints
 * "V27: V21 Carrier detected", so it is the V.21 (fax control channel) one;
 * `V27SH_MTD` above is the other and `DemodDataV27` runs it on the raw input.
 */
#define V27SH_MTD_V21		0x18	/* struct fpm_mtd * */
#define V27SH_BUF		0x1c	/* short *          */

/*
 * How many samples the V.21 detector has seen without a hit, and the latch
 * that starts it.  0x4ff samples at 8 kHz is 160 ms.
 */
#define V27SH_V21_SAMPLES	0x20	/* unsigned short */
#define V27SH_V21_ARMED		0x22	/* short          */
#define V27SH_V21_TIMEOUT	0x4ff

/* The gain control the V.21 detector runs behind, `struct fpm_agc`. */
#define V27SH_AGC		0x24

/*
 * The state handler `V27RX_modem` drives.
 *
 * `count` is IN/OUT: the handler is given the samples still to consume and
 * leaves behind how many are still to consume after it, and its RETURN is how
 * many output samples it produced.  `V27RX_modem` uses the difference for the
 * input pointer and the return for the output pointer, which is what types
 * both.
 */
typedef short (*v27_rx_state_fn)(void *modem, short *in, short *out,
				 unsigned short *count);

/* ------------------------------------------------------------------ */
/* The functions                                                       */

/*
 * The signal-to-noise ratio, in whatever units the caller's family uses.
 *
 * `mov $0xa,%eax; ret` -- six bytes, no argument touched.  THE ARITY IS NOT
 * SETTLED BY THE OBJECT and this is the least claim compatible with the
 * family: `GetSNRV17` and `GetSNRV29` are the same accessor written out,
 * `mov 0x4(%esp),%ecx` then a subtraction from 13 and 14 respectively, and
 * both narrow the result with `cwtl`.  So the family takes the modem and
 * returns `short`; V.27ter's answer happens not to depend on either.
 */
short GetSNRV27(void *modem);

/*
 * Fill a receive status block -- or rather, report whether there was one.
 *
 * `xor %eax,%eax; cmpl $0x0,0x8(%esp); setne %al` and nothing else: the
 * SECOND argument decides the answer and the first is not read at all.  The
 * first is still declared, because `v27rx_status` loads `dp + 0x14` into it
 * before tail-jumping here exactly as `v27tx_status` does for
 * `V27TX_status`, which does read it.
 */
int V27RX_status(void *rx, void *status);

/*
 * Has the equaliser been told to adapt regardless of its own gate?
 *
 * Reads `fpm_fse::lms_force` and returns it as 0 or 1.  The NAME is the
 * author's and the FIELD is `fpm_fse.h`'s; what connects "epoch" to
 * `lms_force` is not established, so nothing here claims it does.
 */
int EpochDetectV27(void *modem);

/*
 * Is there a carrier: `fpm_agc::signal & fpm_sre::active`.
 *
 * Both are 32-bit and the AND is 32-bit, so the result is not reduced to 0/1
 * -- unlike `EpochDetectV27` immediately above it in the object, which is the
 * same shape and does reduce.  That difference is the object's.
 */
int CarrierDetectV27(void *modem);

/* ------------------------------------------------------------------ */
/* The status block `V27TX_status` fills                                */

/*
 * NOT the modem instance: this is the caller's own block, and it is ALREADY
 * MODELLED TWICE in this tree -- `struct v22_status` in
 * `include/dsplib/v22status.h` and `struct v32_status` in
 * `include/dsplib/v32fpstat.h`, both from datapumps that fill the same block
 * with the same fields at the same offsets.  Those are evidence class 2 for
 * the four names below and this header takes them.
 *
 * IT DOES NOT DEFINE A THIRD STRUCTURE, deliberately.  V.27ter writes +0x0c
 * and +0x18, both of which are outside what `struct v22_status` models, so a
 * `struct v27_status` would assert an extent nothing here can bound -- the
 * block is the caller's and no reconstructed function allocates it.  Named
 * constants say exactly what is known and no more.  A parallel V.21 pass has
 * reached the same block from the other side; whoever merges the two should
 * reconcile the spellings rather than let a fourth accumulate.  See finding
 * F8872.
 *
 * THE RATE FIELD IS RANK 1 AS WELL AS RANK 2.  `V21TX_status` does not copy
 * +0x02 from anywhere -- it stores the literal `movw $0x12c,0x2(%edx)` at
 * 0xa2c11, and 0x12c is 300, which is V.21's bit rate to the digit.  V.17,
 * V.27ter and V.29 fill the same slot from their own handle's +0x02 instead,
 * which is what a rate-selectable modem does with a field a fixed-rate one
 * can write as a constant.  `v22status.h` calls it `tx_bps` independently.
 *
 * AND V.27ter'S ZEROES ARE INFORMATIVE.  It zeroes `rx_bps` and `quality`
 * while filling `tx_bps`, which is what a HALF-DUPLEX TRANSMITTER's status
 * would carry: there is no receive rate to report and no equaliser to grade.
 * V.22's full-duplex `V22_status` fills all four.
 *
 * +0x10 IS BOUNDED AND NOT NAMED.  V.17, V.27ter and V.29 give it the same
 * value they gave +0x02 -- the object reads the source's +0x02 a SECOND time
 * rather than reusing the first read -- and V.21 gives it zero where it gave
 * +0x02 its 300.  So it is not simply a copy of the first, since the one
 * modem that knows its rate statically writes two different numbers into
 * them.  `v22status.h` leaves it unnamed too.
 */
#define V27STAT_PROTOCOL	0x00	/* copied from the handle's +0x00  */
#define V27STAT_TX_BPS		0x02	/* bit/s; V21TX_status writes 300  */
#define V27STAT_RX_BPS		0x04	/* zeroed: no receive side here    */
#define V27STAT_QUALITY		0x06	/* zeroed: no equaliser here       */
#define V27STAT_ZERO_08		0x08
#define V27STAT_ZERO_0A		0x0a
#define V27STAT_ZERO_0C		0x0c	/* outside struct v22_status       */
#define V27STAT_WORD_10		0x10	/* see the note above              */
#define V27STAT_ZERO_12		0x12
#define V27STAT_FLAGS		0x14	/* byte; the asymmetry lives here  */
#define V27STAT_FLAGS2		0x15	/* byte                            */
#define V27STAT_WORD_18		0x18	/* int; outside struct v22_status  */

/*
 * The two bits of `V27STAT_FLAGS` this function decides, and the one bit of
 * `V27STAT_FLAGS2` it clears.
 *
 * NAMED BY BIT VALUE AND NOT BY MEANING, because V.22's assignment of these
 * bits is V.22's: `v22status.h` reads bit 0 as its scrambler enable off five
 * datapump words this modem does not have.  What IS established here is that
 * bit 0 comes out set for V.27ter and clear for the other three, that bit 1
 * is cleared by all four, and that bit 2 is copied from the transmitter
 * handle's own +0x10.
 */
#define V27STAT_FLAGS_BIT0	0x01
#define V27STAT_FLAGS_BIT1	0x02
#define V27STAT_FLAGS_FROM_TX	0x04	/* the bit taken from tx + 0x10    */
#define V27STAT_FLAGS2_BIT0	0x01

/*
 * The byte of the transmitter's handle that supplies V27STAT_FLAGS_FROM_TX.
 */
#define V27TX_HANDLE_FLAGS	0x10

/*
 * Copy the transmitter's status into the caller's block, reporting 1, or 0 if
 * there is no block.
 *
 * THE ODD ONE OF FOUR, AND THE ASYMMETRY IS REAL.  `V17TX_status`,
 * `V29TX_status` and this function are otherwise the same code; where the
 * other two clear bits 0 and 1 of the destination's byte at +0x14
 * (`and $0xfc,%al`), this one SETS bit 0 and then clears bit 1
 * (`or $0x1,%al` ... `and $0xfd,%dl`).  Finding F8866 records it and the
 * deviation register carries it as D1033, because the store it makes is
 * dead unless the two blocks overlap.
 */
int V27TX_status(const void *tx, void *status);

/*
 * Run the receive state machine until it has consumed `*count` samples,
 * then report the status word.
 *
 * `count` is IN/OUT in a second sense: on the way in it is the number of
 * input samples, on the way out it is the number of OUTPUT samples produced,
 * accumulated as a `short`.  The loop is a do-while -- the handler is called
 * once even for a count of zero -- and `in` advances by what the handler
 * consumed while `out` advances by what it returned.
 */
int V27RX_modem(void *modem, short *in, short *out, unsigned short *count);

/*
 * Release the receiver.
 *
 * Eleven calls in one fixed order: the equaliser, the symbol recovery and the
 * resampler give their buffers back through their own `_free`; the two scratch
 * buffers and the receive block go to `sysdep_free`; then the shared block's
 * two tone detectors, its own scratch buffer and itself; then the instance.
 *
 * THE THREE `FPM_*_free` CALLS ARE PASSED A SECOND ARGUMENT, the constant 1,
 * and none of the three functions has one -- the same extra argument
 * `v22data.c` and `bwchdem.c` record at their `FPM_AGC_agc` sites.  It is
 * ignored, cdecl makes it harmless, and it is not reproduced here.
 */
void V27RX_delete(void *modem);

/*
 * The equaliser's slicer: `fpm_fse_decision` for V.27ter.
 *
 * Both arguments are in/out.  It is handed the measured angle, subtracts the
 * previous symbol's phase, folds the difference into one revolution, finds the
 * nearest of the four or eight legal phase steps, advances the DPSK reference
 * by that step, and writes the constellation point back: the angle becomes the
 * new reference phase and the magnitude becomes the constant V27DEC_MAG.  The
 * return is the bit pattern the step carries.
 */
unsigned short V27RX_decision(struct fpm_fse *state, short *angle, short *mag);

/*
 * Grade the data: 1 while carrier holds, 2 when it does not.
 *
 * Also drives the mse smoother described at V27RX_Q_ACC, which runs whether
 * or not carrier is up, and whose verdict is left in V27RX_Q_FLAG for someone
 * else to read.
 *
 * RETURNS `short`: the carrier term is `(short)(signal & active)` and the
 * function returns that when it is non-zero, so the value is not reduced to a
 * code.  2 is a code and is returned only for the zero case.
 */
short QualityDetectV27(void *modem);

/*
 * Is the far end still there?
 *
 * Three tests, and which of them run depends on V27SH_V21_WATCH and
 * V27RX_RMS_ON: the equaliser's mse against V27RX_MSE_NO_CARRIER, a V.21 tone
 * detector run over a gain-controlled copy of the block, and the energy-drop
 * detector at V27RX_RMS_REF.  Any of them may clear the answer; none of them
 * sets it.
 *
 * RETURNS `short`, and NOT a code: the carrier term is
 * `(short)(signal & active)` and three of the six paths out return it
 * unchanged, so the value is whatever those two 32-bit flags AND to in their
 * low half.  Only the paths that DENY carrier produce a constant.
 */
short DataCarrierDetectV27(void *modem, short *samples, unsigned short count);

/*
 * `DemodDataV27` IS NOT DECLARED HERE, because it is not written yet.  It
 * drives all four embedded modules end to end, so a differential test for it
 * needs the receiver's whole FPM chain configured rather than merely planted;
 * the offsets and constants it needs are all above, which is why they are
 * stated here rather than deferred with the code.
 */

#endif /* DSPLIB_V27FAX_H */
