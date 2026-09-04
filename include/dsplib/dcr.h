/*
 * dcr.h -- the DC remover, reconstructed from dsplibs.o `dcr.c`.
 *
 *   dcr_create   .text 0x000060   89 bytes
 *   dcr_delete   .text 0x0000c0   17 bytes
 *   dcr_reset    .text 0x0000e0   25 bytes
 *   dcr_process  .text 0x000100  568 bytes
 *
 * WHAT DCR IS, from the object rather than from the name.  Three separate
 * pieces of the object say the same thing:
 *
 *   - The only diagnostic in the translation unit, at `.rodata.str1.4 + 0`,
 *     is the author's own sentence:
 *
 *         "DCR: initial DC Evaluation done, DC level %d, %sabled\n"
 *
 *     with `"en"` and `"dis"` at `.rodata.str1.1 + 0` and `+ 3`.  Both of
 *     those sections OPEN with this file's contributions -- cid's first
 *     string is at +7, after `"en\0dis\0"`.  That says dcr.c is the first
 *     translation unit to contribute a STRING and nothing more: it is the
 *     second contributor to `.text`, behind `prop_dp_init` at 0x00 and
 *     `prop_dp_exit` at 0x30.
 *   - No relocation anywhere in the object targets any `dcr_*` symbol, so
 *     nothing inside dsplibs.o calls it.  It is host-facing API, and the host
 *     is `slmodemd`: `modem.c:1135` creates one per modem, `modem.c:677` runs
 *     every received block through it as the FIRST thing `modem_process`
 *     does -- ahead of the datapump and ahead of the sample log -- under a
 *     comment reading "clean DC", and `modem.c:1201` deletes it.
 *   - `dcr_process` measures the arithmetic mean of the block's samples,
 *     accumulates that mean over a second of audio, and subtracts the result
 *     from every sample in place.  That is a DC blocker built as a counter
 *     and a divide rather than as a highpass filter.
 *
 * So: DCR removes the DC offset that a sound card's ADC leaves on the
 * received signal, before any datapump sees it.  It is measurement plus
 * subtraction, not filtering, and it is deliberately slow -- the estimate is
 * built over a full second and then re-estimated every two.
 *
 * THE FOUR PHASES, which are what `state` selects.  The sample counts are
 * fields rather than constants, and `dcr_create` fills them with values that
 * are round numbers at the 9600 Hz host rate (`docs/rate_assumptions.md` R-5):
 *
 *     0  SETTLE     5760 samples = 0.6 s.  Count only; the accumulator is
 *                   not touched, so whatever transient the card produces on
 *                   opening the device never reaches the estimate.
 *     1  EVALUATE   9600 samples = 1.0 s.  Accumulate, then `dc_level` is
 *                   the plain mean.  This is the "initial DC Evaluation" the
 *                   diagnostic announces, and the only place it is printed.
 *     2  TRACK      19200 samples = 2.0 s.  Accumulate, then blend the new
 *                   mean into `dc_level` at 0.1 (see `dcr.c`).  Repeats for
 *                   the life of the call.
 *     3  HOLD       `dc_level` is frozen; the block is only corrected.
 *
 * DCR_TRACK chooses between 2 and 3 when phase 1 finishes, and is the bit the
 * diagnostic's "%sabled" reports on.
 */

#ifndef DSPLIB_DCR_H
#define DSPLIB_DCR_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * `flags`, +0x00.  Three bits, all set by `dcr_create` (`orb $0x7,(%ebx)`),
 * and nothing in the object ever clears one -- the host is expected to poke
 * them through the `void *` it holds.  Named by bit value, never as
 * bitfields: every read in `dcr_process` is `test $imm` or `and $imm` against
 * a byte (CLAUDE.md, "Naming: fields, and flags").
 */

/*
 * Bit 0 -- the master gate.  Clear, and `dcr_process` returns 0 having
 * touched nothing, not even the counters.
 *
 * DELIBERATELY NOT CALLED `DCR_ENABLE`.  The object's own word "enabled" is
 * attached by the diagnostic to bit 2 below, not to this one, and spending it
 * here would put the author's vocabulary on the wrong bit.
 */
#define DCR_ACTIVE	(1 << 0)	/* 0x01 */

/*
 * Bit 1 -- subtract.  Set, and the block is corrected in place.  Clear, and
 * DCR still measures, still advances its phases and still returns the
 * over-threshold verdict, but leaves the samples alone.  So the two halves of
 * the module -- measure and correct -- are separately switchable.
 */
#define DCR_SUBTRACT	(1 << 1)	/* 0x02 */

/*
 * Bit 2 -- keep re-estimating after the initial evaluation.  Set, phase 1
 * hands over to TRACK; clear, it hands over to HOLD and `dc_level` never
 * changes again.  This is the bit the diagnostic prints:
 *
 *     "DCR: initial DC Evaluation done, DC level %d, %sabled\n"
 *
 * with `"en"` when it is set and `"dis"` when it is not.  "Tracking" is this
 * reconstruction's word for the behaviour the bit selects; "enabled" is the
 * author's word for the bit being set, and the sentence does not say what
 * noun it belongs to.
 */
#define DCR_TRACK	(1 << 2)	/* 0x04 */

/*
 * `state`, +0x04.  See "THE FOUR PHASES" above.
 *
 * THE FIELD IS UNSIGNED, and that is forced rather than chosen.  The object's
 * switch tree is
 *
 *     167:  cmp  $0x1,%ecx
 *     16a:  je   28e            <- phase 1
 *     170:  jae  220            <- phase 2 or the default
 *     176:                      <- phase 0, with NO further test
 *
 * `jae` is the unsigned branch, so its fall-through is "below 1 unsigned",
 * which is exactly zero and needs no confirming.  Declared `int`, GCC 3.4.2
 * emits `jle` and then a `test %ecx,%ecx; jne` to exclude negatives, because
 * with a signed expression "below 1" also covers every negative value.  That
 * extra pair is measurable and was there before this field was retyped.
 *
 * What it does NOT settle is `unsigned int` against an `enum`: C gives an
 * enumeration with only non-negative enumerators an unsigned compatible type,
 * so an `enum dcr_state` would emit the same tree.  These are macros because
 * that is the weaker claim of the two.
 */
#define DCR_STATE_SETTLE	0
#define DCR_STATE_EVALUATE	1
#define DCR_STATE_TRACK		2
#define DCR_STATE_HOLD		3

/*
 * Exactly 32 bytes -- `dcr_create` passes `$0x20` to both `sysdep_malloc` and
 * `sysdep_memset`, so the size is the object's claim and not an inference,
 * and `dcr.c` asserts it.  Three bytes of hole at +0x01 and no trailing pad;
 * nothing here is a pointer, so unlike `struct cid_mtd` the layout is the
 * same at 64 bits and `make check64` covers it.
 */
struct dcr {
	unsigned char flags;		/* +0x00 DCR_ACTIVE etc. above       */
	unsigned char pad01[3];		/* +0x01                             */
	unsigned int state;		/* +0x04 DCR_STATE_* above.  UNSIGNED,
					 *       and forced -- see below     */
	short dc_level;			/* +0x08 the offset being subtracted;
					 *       the "DC level %d" printed   */
	short threshold;		/* +0x0a |dc_level| at or above this
					 *       makes dcr_process return 1  */
	int sum;			/* +0x0c samples accumulated since the
					 *       phase began                 */
	int count;			/* +0x10 how many, i.e. sum's divisor */
	int settle_samples;		/* +0x14 5760: ends phase 0          */
	int evaluate_samples;		/* +0x18 9600: ends phase 1          */
	int track_samples;		/* +0x1c 19200: ends each phase-2 pass*/
};

/**
 * @brief Allocate and initialise a DC remover.
 *
 * Zeroed, then `flags = 7`, `state = 0`, and the four sample-count
 * constants (see "THE FOUR PHASES" above). Reads no argument --
 * `slmodemd` declares it `void *dcr_create()`, which in C is
 * "unspecified", and the object's prologue touches no incoming slot.
 *
 * @return A new `struct dcr *`, or NULL if the allocation fails.
 */
struct dcr *dcr_create(void);

/**
 * @brief Free a DC remover. `if (dcr) sysdep_free(dcr);` -- a tail call
 * in the object.
 * @param dcr  The DC remover to free.
 */
void dcr_delete(struct dcr *dcr);

/**
 * @brief Drop the current DC estimate, without changing phase.
 *
 * Clears `dc_level`, `sum` and `count`, and NOTHING ELSE -- in particular
 * not `state`, so this does not send the estimator back through SETTLE
 * and EVALUATE; it drops the estimate and carries on in whatever phase it
 * was in. Exported by the object and declared by no caller anywhere, in
 * dsplibs.o or in slmodemd.
 *
 * @param dcr  The DC remover to reset.
 */
void dcr_reset(struct dcr *dcr);

/**
 * @brief Remove DC offset from one block of samples, in place.
 *
 * Evaluated on every call including the ones that change nothing.
 * Nothing in `slmodemd` reads the return value: `modem.c:79` declares
 * the function `void`, so the value is computed and discarded on the
 * only host there is.
 *
 * @param dcr  The DC remover, updated in place.
 * @param buf  Samples to correct in place. `short *` is forced, not
 *             chosen: the two accumulate loops sign-extend each element
 *             into a 32-bit sum. `slmodemd` declares the parameter
 *             `void *` and passes `in` from `modem_process`.
 * @param len  Number of samples (not bytes).
 * @return 1 when `|dc_level| >= threshold`, 0 otherwise.
 */
int dcr_process(struct dcr *dcr, short *buf, int len);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_DCR_H */
