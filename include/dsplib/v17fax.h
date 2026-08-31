/*
 * v17fax.h -- ITU-T V.17 (fax): the receiver's primitives, and three
 *             transmit-side setters that sit beside them.
 *
 * `include/dsplib/v17data.h` already carries the V.17 TRANSMITTER's two data
 * leaves and the offsets inside the transmitter instance.  This header is the
 * other side: the functions the fax state machine calls to drive and
 * interrogate the V.17 RECEIVER, plus `SeedScramblerV17`, `SetEncoderV17` and
 * `V17TX_status`, which are transmit-side and are here because they are the
 * remainder of the same batch rather than because they belong with the
 * receiver.
 *
 * `tools/service.py` puts every one of them on the FAX side.
 *
 * ---------------------------------------------------------------------------
 * THE TWO INSTANCES ARE NOT MODELLED, AND THERE ARE TWO OF THEM
 *
 * This follows `v17data.h`'s ruling verbatim: `V17RX_create` (3,201 bytes) and
 * `V17TX_create` (1,043) are not reconstructed, so naming their fields would
 * be guessing.  Every parameter is `void *` and every offset is a named
 * constant below with the evidence beside it.
 *
 * That there are TWO instance types, and not one, is MEASURED rather than
 * assumed, and the measurement is a contradiction that only two types resolve:
 *
 *   - `SeedScramblerV17` and `SetEncoderV17` dereference `obj + 0x28` as a
 *     POINTER and write through it -- the transmitter's private block, which
 *     `v17data.h` establishes twice over as `V17TX_OBJ_FP`;
 *   - `V17RX_modem` clears bit 1 of the BYTE at `obj + 0x29` and then RETURNS
 *     the int at `obj + 0x28`.
 *
 * Byte 0x29 is inside the 4 bytes at 0x28.  Clearing a bit of a live pointer
 * and then returning it is not something one object can survive, so the two
 * functions are not looking at the same struct.  The split is corroborated by
 * every other offset: the transmit pair touch 0x24 and 0x28 only, and the ten
 * receive-side functions touch 0x18, 0x1c, 0x20, 0x28, 0x29, 0x5c and 0x60
 * and never dereference 0x28.  Finding F8850.
 *
 * ---------------------------------------------------------------------------
 * THE AUTHOR'S OWN WORDS
 *
 * Five format strings, which is the strongest evidence class this tree
 * recognises, and they name what the fields below are for:
 *
 *   .rodata.str1.4 0x12bfc  "ERROR: SRE buffer violation!(%d)"
 *   .rodata.str1.4 0x12c20  "V17 Decoder error too big... no carrier\n"
 *   .rodata.str1.4 0x12c4c  "sudden energy drop > 8[dB], no carrier"
 *   .rodata.str1.4 0x12c74  "V17 Dec error too big... unreliable data\n"
 *   .rodata.str1.1 0x04dfb  "V17: V21 Carrier detected\n"
 *
 * `0x12c20` is referenced from BOTH `CarrierDetectV17` and
 * `DataCarrierDetectV17`, and in both it is reached only when the short at
 * receiver state + 0x1c2 exceeds 0x3fff -- so that field is the decoder error
 * the message names, and `V17RXS_DEC_ERROR` is the author's word and not a
 * guess.  `GetSNRV17` returning `13 - error` from the same field is the third
 * independent use.
 */

#ifndef DSPLIB_V17FAX_H
#define DSPLIB_V17FAX_H

#include "dsplib/v17data.h"	/* V17TX_OBJ_FP, V17FP_SMC, V17FP_ENCODER_SEL */

struct fpm_sdm;

/* ------------------------------------------------------------------------ */
/* The TRANSMIT instance -- offsets shared with v17data.h                    */

/*
 * The scrambler, in the transmitter's private block.
 *
 * `v17data.h` records that `V17TX_create` builds an `SDM` at fp + 0x1c and
 * that neither of its two functions touches it.  `SeedScramblerV17` is what
 * touches it: it stores its second argument at fp + 0x2c, which is
 * `struct fpm_sdm` + 0x10 -- `reg`, the shift register, the ONLY 32-bit field
 * in that struct and the only one a "seed" could mean.  `fpm_sdm.h` pins the
 * layout independently of anything here, so the pairing of the function's
 * name to the field is not inference from the offset alone.  Finding F8851.
 */
#define V17FP_SDM		0x1c

/*
 * The short `SetEncoderV17` writes for encoder modes 0 and 2 and NOT for
 * mode 1.
 *
 * NEUTRAL NAME AND DELIBERATELY SO.  It is `V17FP_SMC` + 6, inside the SMCv17
 * coder state, which nothing reconstructed models.  What is established is
 * only that the differential and trellis encoders are given this value and
 * the absolute encoder is not; what it means is not.
 */
#define V17FP_SMC_SHORT_06	0x3a

/*
 * The three values `SetEncoderV17` will accept, which is what BOUNDS the
 * selector `v17data.h` describes.  That header records that nothing it read
 * writes `V17FP_ENCODER_SEL` and that nothing bounds the index; this function
 * is the writer, and it writes 0, 1 or 2 and ignores every other argument.
 * The order matches `V17TX_create`'s table exactly -- dif at fp + 0x80, abs at
 * fp + 0x84, tcm at fp + 0x88 -- so the argument selects the encoder by the
 * same numbering.  Finding F8852.
 */
#define V17_ENCODER_DIF		0
#define V17_ENCODER_ABS		1
#define V17_ENCODER_TCM		2

/* ------------------------------------------------------------------------ */
/* The RECEIVE instance                                                     */

/*
 * Two `short *` the receiver does not own, and the coefficient store they are
 * the destination of.
 *
 * `StoreCoefV17` copies 49 entries from each of the receiver state's two
 * coefficient pointers into the arrays at +0x18 and +0x1c.  The pairing of
 * the two is the author's: the function's own name is the only word for what
 * is being stored.  WHICH of the two arrays is which rail is NOT established
 * -- nothing traced distinguishes them -- so they are numbered, not named.
 */
#define V17RX_OBJ_COEFSAVE0	0x18
#define V17RX_OBJ_COEFSAVE1	0x1c

/*
 * A `short *` that holds a saved copy of the receiver's rate.
 *
 * ESTABLISHED BY A MATCHED PAIR, which is why this one carries a real name
 * where its two neighbours do not: `StoreCoefV17` writes
 * `*p = (short)rx[V17RXS_RATE]` and `Restore_rateV17` writes
 * `rx[V17RXS_RATE] = *p`.  The direction, the width and the field at the far
 * end all agree, and "rate" is the author's word from `Restore_rateV17`.
 */
#define V17RX_OBJ_RATESAVE	0x20

/*
 * The int `V17RX_modem` returns, and the flag byte inside it.
 *
 * NEUTRAL, and see the two-instances note above: 0x29 is byte 1 of the 4
 * bytes at 0x28, so the object both modifies and returns the same word.  What
 * `V17RX_modem` does is clear one bit of it on entry and hand the rest back;
 * nothing traced writes it, so what it reports is unknown.  The bit is named
 * by its VALUE, per CLAUDE.md, and the value is the one the object encodes:
 * `andb $0xfd,0x29(%eax)`.
 */
#define V17RX_OBJ_RESULT	0x28
#define V17RX_OBJ_RESULT_B1	0x29
#define V17RX_RESULT_B1_BIT1	0x02

/*
 * The receiver's two sub-blocks.  Both are POINTERS the instance holds.
 *
 *   +0x5c  the control block: two detector objects, a scratch buffer, a
 *          dispatch slot, and a second detector chain of its own
 *   +0x60  the demodulator state: every FPM object the receive chain runs,
 *          and about 20 KB of it -- `DemodDataV17` reaches +0x4fa8
 */
#define V17RX_OBJ_CTL		0x5c
#define V17RX_OBJ_STATE		0x60

/* ------------------------------------------------------------------------ */
/* Inside the control block at V17RX_OBJ_CTL                                */

/*
 * `struct fpm_mtd *` and `struct fpm_tone *`, TYPED BY THEIR CALLEES:
 * `DemodDataV17` passes +0x00 to `FPM_MTD_detect` and +0x04 to
 * `FPM_TONE_kill`, which is CLAUDE.md's rank-2 evidence and not usage
 * inference.
 */
#define V17RXC_MTD		0x00
#define V17RXC_TONE		0x04

/*
 * An int `CarrierDetectV17` and `DataCarrierDetectV17` both require to be
 * non-zero before they will look at the decoder error at all.  Neutral: what
 * it indicates is not established, only that it gates the carrier verdict.
 */
#define V17RXC_INT_0010		0x10

/*
 * A dispatch slot: `V17RX_modem` calls `*(fn *)(ctl + 0x14)` with its own
 * four arguments unchanged.  `call *0x14(%eax)` carries no relocation, so
 * this is a table entry planted at construction and NOT a symbol reference --
 * nothing in this batch can say which function lands here.
 */
#define V17RXC_PROCESS		0x14

/*
 * A short `DemodDataV17` tests: non-zero skips the whole tone-kill and
 * tone-detect front end and goes straight to the resampler.  Neutral.
 */
#define V17RXC_SHORT_0018	0x18

/* The scratch buffer `DemodDataV17` copies its input into.  `short *`. */
#define V17RXC_SCRATCH		0x1c

/*
 * The SECOND detector chain, which is `DataCarrierDetectV17`'s alone: a
 * `struct fpm_mtd *` at +0x24, its own `short *` buffer at +0x28, a sample
 * counter at +0x2c, a latch at +0x2e and a `struct fpm_agc` at +0x30.
 *
 * `V17RXC_MTD2` and `V17RXC_BUF2` are typed by their callees exactly as the
 * first pair are.  `V17RXC_AGC` is `FPM_AGC_agc`'s first argument, so it is
 * `struct fpm_agc` by the same rank-2 rule.
 *
 * The counter is the one the "V17: V21 Carrier detected" message hangs off:
 * it accumulates the sample count while the tone detector stays silent, is
 * cleared the moment the detector fires, and crossing 0x4ff (1,279 samples,
 * 160 ms at 8 kHz) is what prints the message and drops the carrier.  So
 * "silence" is what it counts, and that IS established by the branch the
 * string sits on.
 */
#define V17RXC_MTD2		0x24
#define V17RXC_BUF2		0x28
#define V17RXC_SILENCE		0x2c
#define V17RXC_SHORT_002E	0x2e
#define V17RXC_AGC		0x30

/* The threshold the silence counter is compared against, from the object. */
#define V17RXC_SILENCE_MAX	0x4ff

/* ------------------------------------------------------------------------ */
/* Inside the demodulator state at V17RX_OBJ_STATE                          */

/*
 * Three ints ANDed with the AGC's signal flag and stored elsewhere in the
 * same block, and the three destinations.  All six are neutral: what is
 * established is the plumbing, not the meaning.
 *
 * `+0x1b4`, `+0x1b8` and `+0x1bc` sit immediately above `V17RXS_EPOCH`, and
 * `+0x128` immediately above `V17RXS_0120`, which is consistent with two
 * small int arrays -- but nothing read here proves either is one, so they are
 * spelled as separate fields.
 */
#define V17RXS_INT_0004		0x04
#define V17RXS_INT_0008		0x08
#define V17RXS_INT_0010		0x10
#define V17RXS_INT_0128		0x128
#define V17RXS_INT_01B4		0x1b4
#define V17RXS_INT_01B8		0x1b8
#define V17RXS_INT_01BC		0x1bc

/*
 * The four FPM objects the receive chain runs, every one typed by the
 * function it is handed to: `DemodDataV17` computes each of these four
 * addresses and passes it to exactly one module, which is CLAUDE.md's rank-2
 * evidence.
 *
 * AND THE FOUR TILE THE BLOCK EXACTLY, WHICH IS A SECOND, INDEPENDENT
 * CONFIRMATION OF ALL FOUR OFFSETS AT ONCE.  Every boundary is the previous
 * object's own `sizeof`, taken from the four headers that model them and not
 * from anything read here:
 *
 *     0x098 + sizeof(struct fpm_mrf) 0x001c = 0x0b4   V17RXS_AGC
 *     0x0b4 + sizeof(struct fpm_agc) 0x002c = 0x0e0   V17RXS_SRE
 *     0x0e0 + sizeof(struct fpm_sre) 0x0090 = 0x170   V17RXS_FSE
 *     0x170 + sizeof(struct fpm_fse) 0x4e18 = 0x4f88  (below the buffers)
 *
 * Four `add $imm` in one function agreeing with four independently derived
 * struct sizes, with no gap anywhere, is not a coincidence that survives one
 * offset being wrong.  It also explains the block's size: the equaliser alone
 * is 19.5 KB of it.  Finding F8854.
 */
#define V17RXS_MRF		0x98	/* struct fpm_mrf  -> FPM_MRF_filter  */
#define V17RXS_AGC		0xb4	/* struct fpm_agc  -> FPM_AGC_agc     */
#define V17RXS_SRE		0xe0	/* struct fpm_sre  -> FPM_SRE_recover */
#define V17RXS_FSE		0x170	/* struct fpm_fse  -> FPM_FSE_receive */

/*
 * The two halves of the carrier verdict, ANDed together by all three of
 * `CarrierDetectV17`, `QualityDetectV17` and `DataCarrierDetectV17`.
 *
 * THE FIRST HALF IS THE AGC's OWN `signal` FLAG, and the tiling above is what
 * says so: `V17RXS_AGC` is 0xb4, `offsetof(struct fpm_agc, signal)` is 0x1c,
 * and 0xb4 + 0x1c is 0xd0.  `fpm_agc.h` describes that field as "more than
 * half the blocks in the last call were above the gate", which is exactly
 * what a carrier verdict wants, and `FPM_AGC_agc` is the only writer.  So
 * this is not an unnamed offset that happens to be read three times; it is a
 * field of a modelled struct, reached the long way round because the
 * enclosing block is not modelled.  Finding F8854.
 *
 * THE WIDTH IS NOT THE SAME AT EVERY SITE, and that is the object's and not
 * ours.  `CarrierDetectV17` reads it with a 32-bit `mov`, which is what the
 * `int` field calls for; `QualityDetectV17` and `DataCarrierDetectV17` read
 * it with `movswl`.  Both instructions were forced -- an `int` cannot produce
 * `movswl` and a `short` cannot produce a 32-bit load -- so the three
 * functions did not share a declaration, and `src/fax/v17.c` spells each site
 * the way the object does rather than picking one.
 *
 * IT IS OBSERVABLE IN `CarrierDetectV17` ALONE.  The other two narrow the AND
 * back to a `short`, and `(short)(x & m)` depends on nothing above bit 15.
 * And on any state a real receiver can reach it is observable NOWHERE, since
 * `signal` only ever holds 0 or 1 -- so `t_v17fax.c` separates it with a
 * synthetic +0xd2, and says so.  Finding F8853.
 */
#define V17RXS_AGC_SIGNAL	(V17RXS_AGC + 0x1c)
#define V17RXS_INT_0120		0x120

/*
 * A short compared against 999, and required to EXCEED it, before either
 * carrier detector will look at the decoder error.  Neutral.
 */
#define V17RXS_SHORT_0094	0x94
#define V17RXS_0094_MIN		0x3e7

/*
 * The epoch flag, named from `EpochDetectV17`, which is the author's own
 * function name and does nothing except report this int as 0 or 1.  Both
 * carrier detectors require it non-zero on the same path.
 */
#define V17RXS_EPOCH		0x1b0

/* The rate, from the Store/Restore pair above.  An `int` in the state. */
#define V17RXS_RATE		0x1ac

/*
 * The decoder error metric.  Named from the two format strings above, which
 * are the only two sites that report it and are both gated on it exceeding
 * 0x3fff.  `GetSNRV17` reads the same field.
 *
 * IT IS READ UNSIGNED IN ONE PLACE.  `GetSNRV17` loads it `movzwl` where
 * every other site loads it as a signed short or compares it 16-bit.  The
 * extension is FREE there -- the difference is 65536 and the result is
 * truncated back to a short -- so it follows the declared type of the local
 * and is reproduced rather than reasoned about.
 */
#define V17RXS_DEC_ERROR	0x1c2
#define V17RXS_DEC_ERROR_MAX	0x3fff

/*
 * `Restore_rateV17`'s other pair: it writes the short at +0x1f8 with the
 * unsigned short at +0x18c plus five.  Both neutral -- nothing else read in
 * this batch touches either.
 */
#define V17RXS_USHORT_018C	0x18c
#define V17RXS_SHORT_01F8	0x1f8

/*
 * The two coefficient arrays `StoreCoefV17` reads, and how many entries it
 * takes from each.  The count is the object's loop bound, `cmp $0x30` with
 * `jbe`, so 49 and not 48.
 */
#define V17RXS_COEF0		0x1d0
#define V17RXS_COEF1		0x1d4
#define V17_COEF_N		49

/*
 * `DemodDataV17`'s two intermediate buffers, chained resampler -> recoverer
 * -> equaliser.  `+0x4fa4` is the resampler's output and the recoverer's
 * input; `+0x4fa8` is the recoverer's output and the equaliser's input.  Both
 * are typed by the callees at each end.
 */
#define V17RXS_BUF_MRF		0x4fa4
#define V17RXS_BUF_SRE		0x4fa8

/*
 * The energy-drop watchdog `DataCarrierDetectV17` runs, gated on the short at
 * +0x4fb4.
 *
 * The reference level at +0x4fb6 is refreshed every SECOND block, and the
 * block RMS is required to stay at or above 0x32fe/32768 of it -- 0.3984,
 * which is -8.00 dB, and the message says "8[dB]".  So the constant and the
 * author's own text agree, and the naming here rests on that agreement rather
 * than on the arithmetic alone.
 */
#define V17RXS_SHORT_4FB4	0x4fb4
#define V17RXS_RMS_REF		0x4fb6
#define V17RXS_RMS_PHASE	0x4fb8
#define V17RXS_RMS_DROP_Q15	0x32fe
#define V17RXS_RMS_PERIOD	2

/*
 * `QualityDetectV17`'s block of four shorts.
 *
 * +0x4fac is a smoothed decoder error: on the first block it is seeded with
 * the raw value, and on each block after it is
 * `0.1 * error + 0.9 * average` in Q15 with round-to-nearest, which are the
 * two constants 0xccd and 0x7333.  +0x4fae counts blocks.  On block 0x32 the
 * average is compared against +0x4fb0 and +0x4fb2 is latched to 1 if the
 * average has NOT stayed above it.
 *
 * THE THRESHOLD AND THE LATCH ARE NEUTRAL because nothing traced reads them
 * back; only the direction of the test is established.
 */
#define V17RXS_QAVG		0x4fac
#define V17RXS_QCOUNT		0x4fae
#define V17RXS_SHORT_4FB0	0x4fb0
#define V17RXS_SHORT_4FB2	0x4fb2
#define V17RXS_QWEIGHT_NEW	0xccd	/* 3277/32768  = 0.1000            */
#define V17RXS_QWEIGHT_OLD	0x7333	/* 29491/32768 = 0.8999            */
#define V17RXS_QROUND		0x4000	/* 0.5 before the >> 15            */
#define V17RXS_QCOUNT_SETTLE	0x31	/* the last block that only smooths */
#define V17RXS_QCOUNT_JUDGE	0x32	/* the one block that judges        */

/*
 * `QualityDetectV17`'s own return values, from the object: it returns the
 * carrier verdict unchanged, except that a ZERO verdict is reported as 2 and
 * annotated with the "unreliable data" message.
 */
#define V17_QUALITY_UNRELIABLE	2

/* ------------------------------------------------------------------------ */
/* The dispatch slot's type                                                 */

/*
 * `V17RX_modem`'s inner call, spelled from the object's own argument set:
 * four slots written, the instance first and the caller's three arguments
 * unchanged after it.  The result is sign-extended with `cwtl` before it is
 * added to the running total, so it is `short` and that is forced.
 */
typedef short (*v17rx_process_fn)(void *modem, short *in, short *out,
				  unsigned short *count);

/* ------------------------------------------------------------------------ */
/* The functions                                                            */

/*
 * Drive the receiver until it stops consuming, and report what the dispatch
 * slot left in the instance.
 *
 * `count` is IN/OUT and changes meaning across the call: on entry it is the
 * number of input samples, on return the total number of outputs produced.
 * The loop is the object's own -- it re-reads `*count` after each inner call,
 * advances `in` by what was consumed and `out` by what was returned, and
 * stops when `*count` reaches zero.
 *
 * THE RUNNING TOTAL IS A `short`, AND THAT IS FORCED: the object sign-extends
 * it with `cwtl` on every iteration, so a total above 32767 wraps.  It is
 * reproduced, not corrected.
 *
 * The one thing the loop does before any of that is clear
 * `V17RX_RESULT_B1_BIT1` in the word it will later return.
 */
int V17RX_modem(void *modem, short *in, short *out, unsigned short *count);

/*
 * Load the scrambler's shift register.  See `V17FP_SDM`: this is
 * `struct fpm_sdm::reg` in the transmitter's private block, and nothing else
 * in the function.
 */
void SeedScramblerV17(void *modem, unsigned int seed);

/*
 * Select one of the transmitter's three encoders, and for two of them set the
 * short at `V17FP_SMC_SHORT_06`.
 *
 * MODE 1 DOES NOT WRITE THE SECOND VALUE, and that asymmetry is the object's:
 * the arm for `V17_ENCODER_ABS` stores the selector and returns.  Any `which`
 * outside 0..2 does nothing at all -- there is no default arm, just a `ret`.
 *
 * Both arguments are `short` and both are FORCED.  `which` is loaded
 * `movswl` and its 32-bit result drives a signed comparison chain, which is
 * CLAUDE.md's forced case exactly; `arg` is loaded `movswl` and only `%cx` is
 * stored, so its extension is dead and follows the local's declared type.
 */
void SetEncoderV17(void *modem, short which, short arg);

/*
 * Fill a status block, or report that there was nothing to fill.
 *
 * Returns 1 when `status` is non-NULL and 0 when it is NULL, and the NULL
 * case does nothing else -- so this is the object's own guard, not ours.
 *
 * NEITHER BLOCK IS IDENTIFIED.  `params` is read at +0x00, +0x02, +0x10 and
 * +0x18 and `status` is written at ten offsets, and nothing reconstructed
 * names either type.  It is worth recording that +0x10 of `params` is the
 * same offset `V17TX_create` takes a rate index from (`v17data.h`,
 * `V17TX_OBJ_PARAMS`), and that this function reads only the LOW BYTE of it
 * and only bit 2; whether the two are the same block is NOT established and
 * the coincidence is recorded rather than acted on.
 *
 * THE DEAD STORE IS THE OBJECT'S.  `status + 0x14` is cleared of its low two
 * bits and then overwritten outright a few instructions later.  It survives
 * in the object because the load of `params + 0x10` sits between the two and
 * may alias, and it survives here for the same reason -- both blocks are
 * `unsigned char *`.  Removing it would be tidier and would stop being the
 * object.
 */
int V17TX_status(void *params, void *status);

/*
 * Report whether a carrier is present.
 *
 * The verdict starts as the AND of the state's two carrier words and is
 * masked to bit 0, or forced to zero, only when the receiver is far enough
 * along to have a decoder error worth believing: the control block's
 * `V17RXC_INT_0010` set, the epoch found, and `V17RXS_SHORT_0094` past 999.
 *
 * RETURNS `int`, AND NOTHING NARROWS IT.  The verdict lives in a register the
 * object never truncates, so unlike `QualityDetectV17` there is no `short`
 * here to reproduce.
 */
int CarrierDetectV17(void *modem);

/*
 * Report the carrier verdict, and maintain the smoothed decoder error behind
 * it.
 *
 * The verdict is the same AND as `CarrierDetectV17`'s, except that +0xd0 is
 * read as a `short` here (see `V17RXS_00D0`) and the result is narrowed to a
 * `short`, which the object forces with `movswl %ax`.  A zero verdict is
 * reported as `V17_QUALITY_UNRELIABLE` and annotated.
 *
 * The maintenance runs on EVERY call and is what makes this function
 * stateful: block 0 seeds the average, blocks 1..0x31 smooth it, block 0x32
 * judges it once, and every block after 0x32 leaves it alone.
 */
short QualityDetectV17(void *modem);

/* Non-zero once the receiver has found its epoch.  0 or 1, never anything else. */
int EpochDetectV17(void *modem);

/*
 * `13 - error`, narrowed to a short.  See `V17RXS_DEC_ERROR` for why the load
 * is unsigned and why that does not change the answer.
 */
short GetSNRV17(void *modem);

/*
 * Copy the receiver's two coefficient arrays and its rate out into the
 * instance's three save pointers.  `V17_COEF_N` entries from each array.
 */
void StoreCoefV17(void *modem);

/*
 * Put the saved rate back, and recompute the short at `V17RXS_SHORT_01F8`
 * from `V17RXS_USHORT_018C`.  It is NOT the inverse of `StoreCoefV17` -- it
 * restores the rate and not the coefficients, which is what its name says.
 */
void Restore_rateV17(void *modem);

#endif /* DSPLIB_V17FAX_H */
