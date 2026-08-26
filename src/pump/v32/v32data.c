/*
 * v32data.c -- ITU-T V.32 / V.32bis: the transmitter's data-path leaves.
 *
 * Reconstructed from dsplibs.o:
 *
 *   ModDataV32       .text 0x081b80  121
 *   TxNoCarrierV32   .text 0x082550  152
 *
 * ---------------------------------------------------------------------------
 * ONE OBJECT, TWO STRUCT TAGS, AND THE OBJECT SAYS SO IN ONE INSTRUCTION PAIR
 *
 * `ModDataV32` computes `fp + 0xb0` twice: once as the encoder's second
 * argument, where `v32smc.h` types it `struct v32_symout *`, and once as
 * `FPM_PPS_filter`'s, where `fpm_pps.h` types it `struct fpm_smc_ring *`.
 * The two declarations describe the same bytes -- `pad00[8]` against `i` and
 * `q`, `pad0e` against `ridx`, `buf`/`widx`/`limit` against
 * `sym`/`widx`/`len` -- and neither header could have known, because each was
 * written from a different side of the ring.
 *
 * This file does NOT unify them.  That is a type-punned site, `docs/plan.md`
 * phase 6 collects those into one batch, and merging two struct tags is
 * exactly the change §3 says must not be made from inside a batch that has
 * other work in flight.  D431 records it; the cast below is deliberate and is
 * the whole of the deviation.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE NO-CARRIER PATH DOES TO THE CODER
 *
 * It steps `quad` backwards by one per symbol -- `add $3` then `and $3` -- and
 * never reads it.  `quad` is the name `v32smc.h` gives fp + 0x4e from the
 * three encoders that DO read it, so the field is not being named here; what
 * is new is that a function outside the coder moves it.  Why is not
 * established and is not guessed at.
 *
 * The loop is written up-counting where the object counts down from
 * `count - 1`.  Both run exactly `count` times for every `unsigned short`
 * count.
 *
 * **THAT IS NOT A FORM THE COMPILER IS FREE TO CHOOSE, and this comment said
 * it was.**  The object's loop foot is
 *
 *     lea -0x1(%ebx),%eax ; movzwl %ax,%ebx ; inc %ax ; jne
 *
 * -- a SIXTEEN-BIT countdown, truncated on every pass, which is the counter's
 * declared type and not something strength reduction makes of a 32-bit
 * induction variable.  `SDMv32_scrambler` next door was closed sixteen bytes
 * on exactly that motif over a 22-cell domain (F8242).
 *
 * Writing it HERE makes the function worse -- 156 bytes to 173 against the
 * object's 152 -- because it does not stand alone: the object's same loop
 * computes its ring wrap branchlessly (`setl`/`neg`/`and`, F8249) where we
 * branch, and the two interact through the loop's register pressure.  All four
 * cells of {short, int wrap locals} x {up, countdown} were compiled and the
 * countdown arm is worse at both.  So the direction is DECODED and UNBOUGHT,
 * which is a different state from free, and the next pass here should move
 * both together or neither.
 */

#include "dsplib/v32data.h"

#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"

/* The instance is not modelled; see v32data.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))

unsigned short
ModDataV32(void *modem, short *data, short *out, unsigned short count)
{
	const v32_encoder_fn *tbl;
	void *fp;
	short sel;

	fp = FIELD_PTR(modem, V32_OBJ_FP);
	tbl = (const v32_encoder_fn *)(void *)FIELD(fp, V32FP_ENCODERS);
	sel = *(short *)(void *)FIELD(fp, V32FP_ENCODER_SEL);
	tbl[sel]((struct v32_smc *)(void *)FIELD(fp, V32FP_SMC),
		 (struct v32_symout *)(void *)FIELD(fp, V32FP_SYMOUT),
		 data, count);

	fp = FIELD_PTR(modem, V32_OBJ_FP);
	return FPM_PPS_filter((struct fpm_pps *)(void *)FIELD(fp, V32FP_PPS),
			      /* D431: the same bytes as the v32_symout above */
			      (struct fpm_smc_ring *)(void *)
					FIELD(fp, V32FP_SYMOUT),
			      out, count);
}

unsigned short
TxNoCarrierV32(void *modem, const short *data, short *out,
	       unsigned short count)
{
	struct v32_symout *ring;
	struct v32_smc *smc;
	unsigned short i;
	short widx, limit, quad;
	void *fp;

	(void)data;			/* never read; see v32data.h */

	fp = FIELD_PTR(modem, V32_OBJ_FP);
	ring = (struct v32_symout *)(void *)FIELD(fp, V32FP_SYMOUT);
	smc = (struct v32_smc *)(void *)FIELD(fp, V32FP_SMC);
	quad = smc->quad;
	widx = ring->widx;
	limit = ring->limit;

	for (i = 0; i < count; i++) {
		short next;

		ring->buf[widx] = V32_SYMBOL_NOCARRIER;
		next = (short)(widx + 1);
		quad = (short)((quad + 3) & 3);
		widx = next < limit ? next : 0;
	}

	/* BOTH write-backs precede the shaper here, unlike V.17 and V.29. */
	smc->quad = quad;
	ring->widx = widx;

	return FPM_PPS_filter((struct fpm_pps *)(void *)FIELD(fp, V32FP_PPS),
			      /* D431 again */
			      (struct fpm_smc_ring *)(void *)
					FIELD(fp, V32FP_SYMOUT),
			      out, count);
}
