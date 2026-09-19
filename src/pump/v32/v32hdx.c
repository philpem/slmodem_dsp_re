/*
 * v32hdx.c -- ITU-T V.32 / V.32bis: the two half-duplex drivers.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V32TxHdxModem   .text 0x07fce0   96
 *   V32RxHdxModem   .text 0x0838f0   12
 *
 * The contract these two impose on the twenty V.32 `TxHdx*` / `RxHdx*` states
 * is written up in `include/dsplib/v32hdx.h` and is the reason this file
 * exists before any of those states do.
 *
 * ---------------------------------------------------------------------------
 * THE ACCUMULATOR IS A `short`, AND THAT IS FORCED
 *
 * The object accumulates the state's return with
 *
 *      lea (%esi,%eax,1),%edx      total + n, in 32 bits
 *      movswl %dx,%esi             truncated to 16 and sign-extended back
 *
 * every iteration, so the running total is sixteen bits wide throughout.
 *
 * AN `int` ACCUMULATOR IS INDISTINGUISHABLE AT TIER 1, and the reason is
 * arithmetic rather than a shortage of test inputs: `total` reaches exactly
 * one observable -- `*nsamples`, a `short *` -- and addition mod 2**16 is
 * associative, so truncating every term and truncating once at the end store
 * the same sixteen bits for EVERY sequence of state returns.  `out` advances
 * by `n` and not by `total`, so nothing else can see it either.  The measured
 * mutation is recorded `equivalent` on exactly that argument.  `short` is
 * written because the OBJECT encodes it and tier 3 reads it, which is the
 * only tier that can: finding F8231.
 *
 * The state's own return is sign-extended with `cwtl` before being added, so
 * a state returning a negative count walks `out` BACKWARDS -- `lea
 * (%ebx,%eax,2),%ebx` with a negative %eax.  No state does; that it is
 * expressible is D493.
 *
 * ---------------------------------------------------------------------------
 * THE TWO STATEMENTS ARE IN THE AUTHOR'S ORDER, AND THAT IS DECODED
 *
 * The object schedules
 *
 *      lea (%esi,%eax,1),%edx      total + n
 *      lea (%ebx,%eax,2),%ebx      out += n
 *      movswl %dx,%esi             the truncation
 *
 * with the pointer advance BETWEEN the sum and its truncation.  The two
 * statements are independent -- each reads `n` and neither reads what the
 * other writes -- so both orders are legal C and both compile.  The candidate
 * space therefore has exactly TWO members and it was ENUMERATED rather than
 * searched, per finding F7770: `out += n` first gives byte identity with the
 * object and `total` first does not (six bytes differ, the two `lea`s
 * swapped).  A complete enumeration with a unique preimage is F7782's TAKEN
 * side, so this is the author's order and not a fit to the compiler.
 *
 * ---------------------------------------------------------------------------
 * WHY THE CONTEXT IS RE-READ INSIDE THE LOOP
 *
 * `mov 0x64(%edi),%edx` sits at the loop's back-edge target (7fd06) and not
 * before it, and the first iteration jumps past it because the value is
 * already in %edx from the +0x9e read.  So the reload is real and is written
 * as one: a state may replace the whole context, and the next iteration must
 * see the replacement rather than a cached pointer.  Writing `hdx` once
 * outside the loop compiles to one fewer instruction and is a different
 * program.
 */

#include "dsplib/v32hdx.h"

void
V32TxHdxModem(struct v32_modem *modem, short *data, short *out, short *nsamples)
{
	unsigned short left;
	short total = 0;
	struct v32_hdx *hdx;

	hdx = modem->hdx;
	left = (unsigned short)hdx->symbol_len;

	do {
		short n;

		/* Re-read: a state may have swapped the context. */
		hdx = modem->hdx;
		n = hdx->tx_state(modem, data, out, &left);
		out += n;
		total = (short)(total + n);
	} while (left != 0);

	*nsamples = total;
}

void
V32RxHdxModem(struct v32_modem *modem, short *in, unsigned short *out,
	      unsigned short *count)
{
	struct v32_hdx *hdx;

	hdx = modem->hdx;
	hdx->rx_state(modem, in, out, count);
}
