/*
 * sdmv27.c -- V.27ter Scrambler/Descrambler Module.
 *
 * Reconstructed from dsplibs.o:
 *   SDMv27_init         .text 0x09a830    89 bytes
 *   SDMv27_scrambler    .text 0x09a890   573 bytes
 *   SDMv27_descrambler  .text 0x09aad0   622 bytes
 *   SDMv27_CFG          .data 0x007d80     2 bytes
 *
 * The polynomial, the guard against repeating patterns and the two paths are
 * derived and documented in `include/dsplib/sdmv27.h`.  This file is the
 * mechanics, and the three things worth having in front of you while reading
 * it are all places where an obvious simplification would be wrong:
 *
 *   - `run += b; run *= b;` is TWO truncations to sixteen bits, not one.  The
 *     object narrows the sum before it multiplies (`movzwl %si,%edx` between
 *     the `lea` and the `imul`), so `run = (run + b) * b` -- which truncates
 *     once -- is a different function of a large `run`.
 *
 *   - `pending = ok * pending` is provably always zero, because `ok` is
 *     `~(alldiff | inverting) & 1` and `inverting` is `pending`.  It is here
 *     because the object multiplies; the only way `pending` becomes one is
 *     the threshold below it.
 *
 *   - the two directions carry `inverting` differently -- the scrambler
 *     assigns it at the head of the bit loop and the descrambler at the tail
 *     -- and the two are NOT interchangeable.  Each matches the exit state
 *     its own function leaves; swapping them stops the scrambler inverting
 *     after a threshold, which is 1,994 failing checks.  F8901.
 *
 * The bit paths re-read and re-write `*data` on every bit and advance the
 * pointer once, after the mask; the whole-word paths advance it with the
 * store.  That is the object's shape and it is why `data++` appears twice.
 */

#include "dsplib/sdmv27.h"

/*
 * Three bits per symbol -- V.27ter's 4800 bit/s tribit.  In `.data`, so not
 * const; SetScramblerV27 copies it and then overwrites `nbits` from
 * V27TX_SDM_NUM_BITS, and SDMv27_init falls back to it for a null config.
 */
struct sdmv27_cfg SDMv27_CFG = { 3 };

void
SDMv27_init(struct sdmv27 *sdm, const struct sdmv27_cfg *cfg)
{
	/*
	 * ONLY THE FIRST READ IS GUARDED, and that is the object's own defect
	 * rather than a transcription error: the null arm at 0x9a880 loads
	 * `nbits` from the table and jumps back to 0x9a83f with the caller's
	 * NULL still in `%ecx`, which 0x9a85a then dereferences with
	 * `cmpw $0x2,(%ecx)`.  So `SDMv27_init(s, NULL)` faults in the
	 * original.  Reproduced, and not reachable in service -- both callers
	 * (`V27RX_create`, `SetScramblerV27`) pass a stack local.  D1043.
	 */
	if (cfg != 0)
		sdm->nbits = cfg->nbits;
	else
		sdm->nbits = SDMv27_CFG.nbits;

	sdm->reg = SDMV27_REG_SEED;
	sdm->run = 0;
	sdm->pending = 0;
	sdm->inverting = 0;

	/*
	 * A two-way branch on `nbits`, not `(1 << nbits) - 1`: the object
	 * stores the four constants and never shifts.  Only 2 and 3 are
	 * reachable through V27TX_SDM_NUM_BITS, and anything else lands in
	 * the second arm.
	 */
	if (cfg->nbits == 2) {
		sdm->mask = 3;
		sdm->notmask = (unsigned short)~3;
	} else {
		sdm->mask = 7;
		sdm->notmask = (unsigned short)~7;
	}
}

void
SDMv27_scrambler(struct sdmv27 *sdm, unsigned short *data, short count)
{
	const int nbits = sdm->nbits;
	const int mask = sdm->mask;
	const int notmask = sdm->notmask;
	const unsigned short bulk_limit =
		(unsigned short)(SDMV27_GUARD_RUN - nbits);
	unsigned short reg = sdm->reg;
	unsigned short run = sdm->run;
	unsigned short pending = sdm->pending;
	unsigned short inverting = sdm->inverting;
	short j;

	while (count--) {
		if (run < bulk_limit && pending + inverting == 0) {
			/*
			 * Whole word.  All `nbits` output bits at once from
			 * the register as it stood, then `nbits` guard bits
			 * walked to advance the run.  The entry test bounds
			 * the run at 32 after the walk, so the threshold
			 * cannot be crossed in here and is not tested for.
			 */
			unsigned int shifted =
				((unsigned int)reg << nbits) & notmask;
			unsigned int out;
			unsigned short guard;

			out = (*data ^ (shifted >> SDMV27_TAP1)
			       ^ (shifted >> SDMV27_TAP2)) & mask;
			*data++ = (unsigned short)out;
			reg = (unsigned short)(shifted | out);

			guard = (unsigned short)
				~(((reg >> 8) ^ out) & ((reg >> 9) ^ out)
				  & ((reg >> 12) ^ out));

			/* Oldest bit of the word first, which is the high
			 * one: bit 0 is the most recent. */
			for (j = (short)(nbits - 1); j >= 0; j--) {
				unsigned int b = (guard >> j) & 1;

				run += (unsigned short)b;
				run *= (unsigned short)b;
			}
		} else {
			for (j = (short)nbits; j > 0; j--) {
				unsigned int in;
				unsigned int out;
				unsigned int alldiff;
				unsigned int ok;

				inverting = pending;

				reg = (unsigned short)(reg << 1);
				in = *data;
				out = (((unsigned int)reg >> SDMV27_TAP1)
				       ^ ((unsigned int)reg >> SDMV27_TAP2)
				       ^ inverting
				       ^ (in >> (nbits - 1))) & 1;
				*data = (unsigned short)((in << 1) | out);
				reg = (unsigned short)(reg | out);

				/*
				 * `out * 0xffff` broadcasts the bit across
				 * the word, so one AND and one compare test
				 * all three guard taps at once.
				 */
				alldiff = ((((out * 0xffffu) ^ reg)
					    & SDMV27_GUARD_TAPS)
					   == SDMV27_GUARD_TAPS);
				ok = ~(alldiff | inverting) & 1;

				run += (unsigned short)ok;
				run *= (unsigned short)ok;
				pending = (unsigned short)(ok * inverting);

				if (run == SDMV27_GUARD_RUN) {
					run = 0;
					pending = 1;
				}
			}

			*data &= (unsigned short)mask;
			data++;
		}
	}

	sdm->reg = reg;
	sdm->run = run;
	sdm->pending = pending;
	sdm->inverting = inverting;
}

void
SDMv27_descrambler(struct sdmv27 *sdm, unsigned short *data, short count)
{
	const int nbits = sdm->nbits;
	const int mask = sdm->mask;
	const int notmask = sdm->notmask;
	const unsigned short bulk_limit =
		(unsigned short)(SDMV27_GUARD_RUN - nbits);
	unsigned short reg = sdm->reg;
	unsigned short run = sdm->run;
	unsigned short pending = sdm->pending;
	unsigned short inverting = sdm->inverting;
	short j;

	while (count--) {
		if (run < bulk_limit && pending + inverting == 0) {
			unsigned int taps;
			unsigned int in;

			/*
			 * Feed-forward: the taps come off the shifted
			 * register BEFORE `notmask` clears the incoming
			 * bits' slots, and it is the RECEIVED word that goes
			 * in -- masked here, unlike the generic module.
			 */
			reg = (unsigned short)(reg << nbits);
			taps = ((unsigned int)reg >> SDMV27_TAP1)
				^ ((unsigned int)reg >> SDMV27_TAP2);
			reg = (unsigned short)(reg & notmask);
			in = *data & mask;
			reg = (unsigned short)(reg | in);
			*data++ = (unsigned short)((taps ^ in) & mask);

			{
				unsigned short guard = (unsigned short)
					~(((reg >> 8) ^ in)
					  & ((reg >> 9) ^ in)
					  & ((reg >> 12) ^ in));

				for (j = (short)(nbits - 1); j >= 0; j--) {
					unsigned int b = (guard >> j) & 1;

					run += (unsigned short)b;
					run *= (unsigned short)b;
				}
			}
		} else {
			for (j = (short)nbits; j > 0; j--) {
				unsigned int taps;
				unsigned int w;
				unsigned int in;
				unsigned int out;
				unsigned int alldiff;
				unsigned int ok;

				reg = (unsigned short)(reg << 1);
				taps = ((unsigned int)reg >> SDMV27_TAP1)
					^ ((unsigned int)reg >> SDMV27_TAP2);

				w = (unsigned int)*data << 1;
				in = ((unsigned short)w >> nbits) & 1;
				reg = (unsigned short)(reg | in);

				out = ((taps ^ inverting) & 1) ^ in;
				*data = (unsigned short)(w | out);

				alldiff = ((((in * 0xffffu) ^ reg)
					    & SDMV27_GUARD_TAPS)
					   == SDMV27_GUARD_TAPS);
				ok = ~(alldiff | inverting) & 1;

				run += (unsigned short)ok;
				run *= (unsigned short)ok;
				pending = (unsigned short)(ok * pending);

				if (run == SDMV27_GUARD_RUN) {
					run = 0;
					pending = 1;
				}

				/*
				 * At the BOTTOM here, and at the top in the
				 * scrambler.  Not tidied into agreement:
				 * each placement matches the exit state its
				 * own direction leaves, and exchanging them
				 * is a behavioural change, not a rewrite.
				 * F8901.
				 */
				inverting = pending;
			}

			*data &= (unsigned short)mask;
			data++;
		}
	}

	sdm->reg = reg;
	sdm->run = run;
	sdm->pending = pending;
	sdm->inverting = inverting;
}
