/*
 * sdmv27.h -- V.27ter Scrambler/Descrambler Module.
 *
 * A DIFFERENT module from `fpm_sdm.h`'s, not a configuration of it: its own
 * object, its own config, its own three entry points, and a guard against
 * repeating patterns that the generic one has no room for.
 *
 * ---------------------------------------------------------------------------
 * The polynomial, and how it was read
 *
 * The taps are NOT configurable here.  They are literal shifts in the code:
 * the register is shifted up by `nbits` and the two feedback terms are that
 * value shifted right by 6 and by 7 (0x9a94b and 0x9a94e in the scrambler,
 * 0x9aba0 and 0x9ab9b in the descrambler).
 *
 * Number the scrambled bits D[n] in transmission order.  The register holds
 * them with bit 0 the MOST RECENT, so after `reg <<= nbits` its bit p is
 * D[n - p] for the bit n that is about to be produced; `reg >> 6` is
 * therefore D[n-6] and `reg >> 7` is D[n-7], giving
 *
 *     D[n] = data[n] ^ D[n-6] ^ D[n-7]
 *
 * -- the generating polynomial 1 + x^-6 + x^-7, which is what ITU-T V.27ter
 * specifies.  The standard is CORROBORATION; the derivation above is from the
 * object.  The descrambler is the exact feed-forward inverse, feeding the
 * RECEIVED bit into the register instead of the produced one.
 *
 * ---------------------------------------------------------------------------
 * The guard against repeating patterns
 *
 * A self-synchronising scrambler with a short polynomial can still emit a
 * periodic sequence, and the object carries a detector for it.  For each
 * scrambled bit D[n] it forms the three comparisons
 *
 *     D[n] == D[n-8]   D[n] == D[n-9]   D[n] == D[n-12]
 *
 * and counts how many consecutive bits have AT LEAST ONE of them true, i.e.
 * it resets the counter only when all three differ.  On the 33rd such bit the
 * counter is cleared and the NEXT bit is inverted -- one bit, not a run,
 * because the flag that carries the inversion is cleared by the same bit that
 * consumes it.
 *
 * In the object that is `& 0x1300` against three bits of the register (8, 9
 * and 12) compared for equality with 0x1300, and a threshold of 0x21.  Both
 * are named below.  This is a MULTI-BIT tap mask, not a flag, so it is named
 * as a set of shifts rather than given a flag name -- CLAUDE.md's rule.
 *
 * ---------------------------------------------------------------------------
 * Two paths, and why the bulk one is not an optimisation of the other
 *
 * Both the scrambler and the descrambler have a whole-word path and a
 * bit-at-a-time path, and they choose per WORD:
 *
 *     run < 33 - nbits   AND   pending + inverting == 0      -> whole word
 *
 * The whole-word path does all `nbits` bits with three shifts and two XORs
 * (the same trick `fpm_sdm.h` explains) and can then not reach the threshold
 * inside the word, so it needs no per-bit test for it; it still walks the
 * `nbits` guard bits to advance `run`.  The bit path is entered whenever an
 * inversion is pending or the threshold is within reach, and is the only one
 * that can invert.  Both are reproduced; neither is dead.
 *
 * ---------------------------------------------------------------------------
 * `inverting` is carried differently by the two directions, and that is real
 *
 * The scrambler assigns `inverting = pending` at the TOP of each bit, so a
 * call ignores whatever `inverting` it was handed and takes `pending`.  The
 * descrambler assigns it at the BOTTOM, so its first bit uses the
 * `inverting` it was handed.
 *
 * **THEY ARE NOT INTERCHANGEABLE, AND THAT WAS MEASURED RATHER THAN
 * ASSUMED.** Each spelling is self-consistent with the state its own
 * function leaves: after a threshold the scrambler exits with
 * (pending 1, inverting 0) and the descrambler with (1, 1), and each then
 * inverts exactly one bit on the next call.  Giving the scrambler the
 * descrambler's placement fails 1,994 of `t_sdmv27`'s first 7,179 checks --
 * the first call after a threshold stops inverting.  Finding F8901.
 */

#ifndef DSPLIB_SDMV27_H
#define DSPLIB_SDMV27_H

/* The two feedback taps, in bits: D[n] ^= D[n-6] ^ D[n-7]. */
#define SDMV27_TAP1		6
#define SDMV27_TAP2		7

/*
 * The guard's three comparison taps, 8, 9 and 12 bits back, as one mask over
 * the register.  Used as `(x & MASK) == MASK`: all three DIFFER from the bit
 * just produced.
 */
#define SDMV27_GUARD_TAPS	((1 << 8) | (1 << 9) | (1 << 12))

/* Consecutive bits without an all-three-differ before the next is inverted. */
#define SDMV27_GUARD_RUN	33

/* The register's seed.  Not zero, and not derived from anything else here. */
#define SDMV27_REG_SEED		60

/*
 * Two bytes, and `nbits` is the whole of it: SDMv27_CFG is `nm`-sized at 2 and
 * SDMv27_init reads nothing past +0.  Kept as a struct rather than a bare
 * short because init takes it by pointer and defaults it, which is how
 * `fpm_sdm_cfg` is handed over too.
 */
struct sdmv27_cfg {
	unsigned short nbits;	/* +0x00 bits carried by each data word      */
};

/*
 * 14 bytes.  Every field is read with `movzwl`, so every one is an
 * `unsigned short`; nothing beyond +0x0d is touched.
 */
struct sdmv27 {
	unsigned short nbits;	/* +0x00 copied from the config              */
	unsigned short mask;	/* +0x02 (1 << nbits) - 1, from a two-way
				 *       branch on nbits, not a shift        */
	unsigned short notmask;	/* +0x04 ~mask                               */
	unsigned short reg;	/* +0x06 shift register, bit 0 most recent.
				 *       SetScramblerV27 saves and restores
				 *       this across a re-init (0xa5ec6 and
				 *       0xa5edc), which is what identifies
				 *       it as the state worth keeping.      */
	unsigned short run;	/* +0x08 consecutive guard bits so far       */
	unsigned short pending;	/* +0x0a invert the NEXT bit                 */
	unsigned short inverting; /* +0x0c inverting the bit in hand         */
};

/* `.data`, two bytes, holding 3 -- V.27ter's 4800 bit/s tribit.  Passed by
 * SDMv27_init itself when the caller hands it a null config. */
extern struct sdmv27_cfg SDMv27_CFG;

/**
 * @brief Load a config and reset a V.27ter scrambler/descrambler state.
 *
 * There is no separate reset entry point: `SetScramblerV27` re-runs this
 * on a live object and puts `reg` back by hand afterwards.
 *
 * @param sdm  State to initialise.
 * @param cfg  Configuration (`nbits`), or NULL for ::SDMv27_CFG -- but a
 *             NULL @p cfg actually FAULTS: the object guards only the
 *             first of its two reads from @p cfg, and the second goes
 *             through the caller's null unguarded. Reproduced;
 *             unreachable in service (deviation D1043).
 */
void SDMv27_init(struct sdmv27 *sdm, const struct sdmv27_cfg *cfg);

/**
 * @brief Scramble @p count words in place with the V.27ter scrambler.
 *
 * Each word carries `nbits` bits in its low end. Runs the self-
 * synchronising scrambler plus the repeating-pattern guard (see the file
 * comment above) over all @p count words.
 *
 * @param sdm    Scrambler state.
 * @param data   Words to scramble in place, @p count of them.
 * @param count  Number of words in @p data. SIGNED here (unlike the
 *               generic fpm_sdm module) -- `ScrambleDataV27` widens it
 *               with `movswl`, so a negative count is not an early exit:
 *               the `while (count--)` loop counts down, wraps, and comes
 *               back to zero tens of thousands of words later.
 */
void SDMv27_scrambler(struct sdmv27 *sdm, unsigned short *data, short count);

/**
 * @brief Descramble @p count words in place with the V.27ter descrambler.
 *
 * The feed-forward inverse of SDMv27_scrambler(), with its own copy of
 * the repeating-pattern guard.
 *
 * @param sdm    Descrambler state.
 * @param data   Words to descramble in place, @p count of them.
 * @param count  Number of words in @p data; see SDMv27_scrambler() for
 *               the signed-count note (`DescrambleDataV27` widens with
 *               `movswl` too).
 */
void SDMv27_descrambler(struct sdmv27 *sdm, unsigned short *data, short count);

#endif /* DSPLIB_SDMV27_H */
