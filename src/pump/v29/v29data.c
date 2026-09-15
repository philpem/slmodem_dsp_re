/*
 * v29data.c -- ITU-T V.29 (fax): the transmitter's no-carrier leaf.
 *
 * Reconstructed from dsplibs.o:
 *
 *   TxNoCarrierV29   .text 0x0a6620  146
 *
 * ---------------------------------------------------------------------------
 * WHY THE SILENCE IS TWO ZEROED RAILS AND NOT A ZERO INDEX
 *
 * `V29TX_create` builds the shaper's configuration with `mapped` clear, so
 * `FPM_PPS_filter` reads `src->i[ridx]` and `src->q[ridx]` and never looks at
 * `sym`.  Zeroing the two rails is therefore how this modulation says "no
 * symbol"; writing a zero into `sym` would be read by nothing.  V.17's
 * `mapped` is set and its no-carrier path writes `sym` alone.  Neither
 * reading is derived from the other -- see finding F3641.
 *
 * Both rail pointers are loaded once, above the loop, and that is FREE: the
 * loop stores `short`s, which cannot alias a `short *` object, so a compiler
 * may hoist them whether the source read them once or per iteration.  Written
 * once, which is the plainer source.
 */

#include "dsplib/v29data.h"

#include <stddef.h>

#include "dsplib/fpm_pps.h"
#include "dsplib/fpm_smc.h"



/*
 * The block's own allocation is the only thing that fixes its length, so
 * assert it rather than trusting the layout to add up by eye.  These are what
 * would have caught a wrong `pad_` run before it silently under-allocated
 * anything -- `V90Parameters`' 0x504-against-0x558 is the case that argument
 * comes from.
 *
 * GUARDED ON THE POINTER SIZE, because `struct fpm_smc_ring` is three pointers
 * and is 0x14 bytes only where a pointer is four.  The 64-bit portability
 * build makes it 0x20 and the block 0xb0, and an unguarded assertion fails
 * there -- which is how this one first announced itself.  81 files in `src/`
 * carry the same guard for the same reason.  Read the note in
 * `src/pump/v90/V90Mapper.cpp` before trusting it too far: `__SIZEOF_POINTER__`
 * is a GCC 4.6+ predefine, so under the PERIOD compiler this whole block reads
 * `#if 0` and asserts nothing at all.  What actually protects these offsets is
 * the differential test.
 */
#if __SIZEOF_POINTER__ == 4
typedef char v29tx_size[(sizeof(struct v29tx) == 0x9c) ? 1 : -1];
typedef char v29tx_ring_at[(offsetof(struct v29tx, ring) == V29FP_SMC_RING) ? 1 : -1];
typedef char v29tx_smc_at[(offsetof(struct v29tx, smc) == V29FP_SMC) ? 1 : -1];
typedef char v29tx_pps_at[(offsetof(struct v29tx, pps) == V29FP_PPS) ? 1 : -1];
#endif
unsigned short
TxNoCarrierV29(void *modem, const unsigned short *data, short *out,
	       unsigned short count)
{
	struct fpm_smc_ring *ring;
	unsigned short i, produced;
	short widx, len;
	struct v29tx *fp;

	(void)data;			/* never read; see v29data.h */

	fp = V29TX(modem);
	ring = &fp->ring;
	widx = ring->widx;
	len = ring->len;

	for (i = 0; i < count; i++) {
		short next;

		ring->i[widx] = 0;
		ring->q[widx] = 0;
		next = (short)(widx + 1);
		widx = (short)(next < len ? next : 0);
	}

	produced = FPM_PPS_filter(&fp->pps, ring, out, count);

	/* Back through a FRESH read of the instance pointer, after the shaper. */
	fp = V29TX(modem);
	fp->ring.widx = widx;

	return produced;
}

/*
 * GenEQTrnSequenceV29  .text 0x0a4770  99 bytes
 *
 * The V.29 equaliser-training generator: a 7-bit LFSR whose feedback is
 * bit0 XOR bit1, planted at bit 7 BEFORE the shift so it lands at bit 6
 * after -- the object computes `((sr << 6) & 0x80) ^ ((sr & 1) << 7)`, ORs
 * it in, shifts right once and masks to 7 bits.  A 1 bit answers
 * constellation index 0xb, a 0 bit answers 0.
 *
 * The register is a short in a block this tree has not modelled (see
 * V29TX_OBJ_SCRAM in v29data.h); it is read once, stepped n times and
 * written back once, exactly as the object does.
 */
void
GenEQTrnSequenceV29(void *modem, unsigned short *out, unsigned short n)
{
	void *blk = *(void **)(void *)((unsigned char *)modem
				       + V29TX_OBJ_SCRAM);
	short *srp = (short *)(void *)((unsigned char *)blk + V29SCRAM_SR);
	int sr = *srp;
	unsigned short i;

	for (i = n; i != 0; i--) {
		int lsb = sr & 1;
		int fb = ((sr << 6) & 0x80) ^ (lsb << 7);

		sr = (int)(((unsigned)(sr | fb) >> 1) & 0x7f);
		*out++ = (unsigned short)(lsb ? 0xb : 0);
	}
	*srp = (short)sr;
}