/*
 * class1.c -- Class 1 fax: the session leaves nothing in the object calls.
 *
 * Reconstructed from dsplibs.o class1.c (F1410's method anchors this TU: its
 * four `t` statics start at 0x092c00 and its first global, `_put_silence`,
 * at 0x092b70):
 *
 *   _send_silence_state           .text 0x092c00    64
 *   _recieve_silence_state        .text 0x092c40   274
 *   _idle_state_init              .text 0x092dc0     3
 *   _idle_state                   .text 0x092dd0    72
 *   _send_silence_state_init      .text 0x092b90    42
 *   _recieve_silence_state_init   .text 0x092bc0    59
 *   fax_class1_info               .text 0x093690    56
 *   fax_class1_GetConstalation    .text 0x093d50     3
 *   _sym_size                     .text 0x093d60   103
 *   _set_modem_rate               .text 0x093dd0   168
 *
 * THIS IS NOT THE FAX PHASE.  All six are exported API with no internal
 * referrer (finding F8320's no-entry-point bucket) and are written on that
 * bucket's own merit; the file is placed and named so the eventual fax
 * phase inherits the blob's own module structure.  `fax_class1_status` was
 * in this batch's scope too and is left out: it calls the unreconstructed
 * `FAXVMI_status`, and this tree links no scaffold (F215).  See
 * docs/findings.md F8492.
 *
 * The spelling `_recieve_...` is the AUTHOR'S, from the symbol table; do
 * not fix it.
 */

#include "dsplib/class1.h"
#include "dsplib/debug.h"
#include "dsplib/fpm.h"

/*
 * Both silence inits: install the state number and arm the countdown with
 * half the sample count, floored at ONE -- the floor keeps a zero-length
 * silence from parking the machine forever.  The halving is the object's
 * signed divide (shr $0x1f / add / sar), so -1 samples arms 1 via the
 * floor... after rounding toward zero, not minus one.
 *
 * The receive side also clears two fields of its own and installs 0x10
 * rather than 0x0f; otherwise the twins are identical.
 */
int
_send_silence_state_init(struct fax_class1 *ctx, int samples)
{
	int half = samples / 2;

	ctx->state = CLASS1_SEND_SILENCE_STATE;
	if (half == 0)
		half = 1;
	ctx->countdown = half;
	return half;
}

int
_recieve_silence_state_init(struct fax_class1 *ctx, int samples)
{
	int half;

	ctx->state = CLASS1_RECIEVE_SILENCE_STATE;
	ctx->silence_blocks = 0;
	ctx->energy = 0;
	half = samples / 2;
	if (half == 0)
		half = 1;
	ctx->countdown = half;
	return half;
}

int
fax_class1_info(struct fax_class1 *ctx, int sel, int *out)
{
	if (sel == 0) {
		*out = ctx->f12d8;
		return 0;
	}
	if (sel == 1) {
		void *p = ctx->f1288;

		*out = p ? *(unsigned short *)((char *)p + 0xc) : 0;
	}
	return 0;
}

int
fax_class1_GetConstalation(void *ctx)
{
	(void)ctx;
	return 0;
}

/*
 * Bits per symbol.  2400 is 2 (V.27ter's 1200 baud), 4800 and 7200 are 3,
 * 9600 is 4, 12000 is 5, 14400 is 6; anything else answers 0.  The object
 * is one dense switch (a balanced compare tree); written as one.
 */
int
_sym_size(int rate)
{
	switch (rate) {
	case 0x960:
		return 2;
	case 0x12c0:
		return 3;
	case 0x1c20:
		return 3;
	case 0x2580:
		return 4;
	case 0x2ee0:
		return 5;
	case 0x3840:
		return 6;
	default:
		return 0;
	}
}

/*
 * T.30 rate code to (modulation, bit rate).  Each V.17 rate has two codes
 * -- long and short training -- so those four tests are RANGES of two, and
 * they are four SEPARATE ifs in the object (no else), kept that way here.
 * The V.29 / V.27ter codes are single equalities in an else-if chain.  A
 * code matching nothing writes nothing.
 */
void
_set_modem_rate(int code, int *mod, int *rate)
{
	if ((unsigned)(code - 0x91) <= 1) {	/* V.17 14400 */
		*rate = 0x3840;
		*mod = 2;
	}
	if ((unsigned)(code - 0x79) <= 1) {	/* V.17 12000 */
		*rate = 0x2ee0;
		*mod = 2;
	}
	if ((unsigned)(code - 0x61) <= 1) {	/* V.17 9600 */
		*rate = 0x2580;
		*mod = 2;
	}
	if ((unsigned)(code - 0x49) <= 1) {	/* V.17 7200 */
		*rate = 0x1c20;
		*mod = 2;
	}
	if (code == 0x60) {			/* V.29 9600 */
		*rate = 0x2580;
		*mod = 1;
	} else if (code == 0x48) {		/* V.29 7200 */
		*rate = 0x1c20;
		*mod = 1;
	} else if (code == 0x30) {		/* V.27ter 4800 */
		*rate = 0x12c0;
		*mod = 0;
	} else if (code == 0x18) {		/* V.27ter 2400 */
		*rate = 0x960;
		*mod = 0;
	}
}

/*
 * ------------------------------------------------------------------
 * Three of the nineteen state handlers.
 *
 *   _send_silence_state       .text 0x092c00    64
 *   _recieve_silence_state    .text 0x092c40   274
 *   _idle_state               .text 0x092dd0    72
 *   _idle_state_init          .text 0x092dc0     3
 *
 * THE FIRST THREE ARE FILE-LOCAL IN THE OBJECT and are global here.  Nothing
 * calls them by name: `fax_class1_create` stores their addresses into
 * `class1_state_functions`, which shows up as an `R_386_32` against the
 * SECTION symbol with the address as an inline addend
 * (`.text+0x092ef4 -> .text:0x092dd0`) and so appears in no call graph and
 * under no name.  `getbit` and `ApplyBulkDelay` in `src/pump/v34/v34hshak.c`
 * are the precedent for writing a file-local as a global so that its
 * `ref_` alias can be driven directly (F221, F227); the storage class is a
 * knowing divergence and is recorded as D1053.
 *
 * The handler contract is nine arguments -- see class1.h, which says which
 * of them are established and which are only established as WIDTHS.
 */

int
_idle_state_init(struct fax_class1 *ctx)
{
	(void)ctx;
	return 0;
}

/*
 * The object emits two loops here, the second with 160 as an immediate, which
 * is what jump threading makes of one loop over a variable the compiler has
 * just pinned to 160 on that path.  Written as the one loop.
 */
int
_idle_state(struct fax_class1 *ctx, const short *rx, short *tx,
	    int word3, int word4, int *rx_count, int *tx_count,
	    int word7, int *word8)
{
	int n = *rx_count;
	int i;

	(void)ctx;
	(void)rx;
	(void)word3;
	(void)word4;
	(void)word7;
	(void)word8;

	if (n <= 0)
		n = CLASS1_BLOCK_SAMPLES;
	for (i = 0; i < n; i++)
		tx[i] = 0;
	*tx_count = n;
	return 0;
}

int
_send_silence_state(struct fax_class1 *ctx, const short *rx, short *tx,
		    int word3, int word4, int *rx_count, int *tx_count,
		    int word7, int *word8)
{
	int i;

	(void)rx;
	(void)word3;
	(void)word4;
	(void)rx_count;
	(void)word7;
	(void)word8;

	ctx->countdown--;
	for (i = 0; i < CLASS1_BLOCK_SAMPLES; i++)
		tx[i] = 0;
	*tx_count = CLASS1_BLOCK_SAMPLES;
	if (ctx->countdown == 0)
		ctx->status = FAX_CLASS1_OK_NO_CARRIER;
	return 0;
}

/*
 * `*word8` is read into a register before the transmit loop in the object.
 * That is the compiler's doing rather than the author's -- `tx` is `short *`
 * and `word8` is `int *`, so strict aliasing lets the load move -- and the
 * read is written where a human puts it, at the test.
 *
 * The threshold compare is SIGNED and sixteen-bit (`cmp $0x64,%ax; jle`), so
 * it is on the `short` the RMS was stored into; the silence-block compare
 * against `countdown` is UNSIGNED (`jae`), which is why `silence_blocks` is
 * an `unsigned int` in class1.h and the cast below is written out.
 */
int
_recieve_silence_state(struct fax_class1 *ctx, const short *rx, short *tx,
		       int word3, int word4, int *rx_count, int *tx_count,
		       int word7, int *word8)
{
	int i;

	(void)word3;
	(void)word4;
	(void)word7;

	for (i = 0; i < CLASS1_BLOCK_SAMPLES; i++)
		tx[i] = 0;
	*tx_count = CLASS1_BLOCK_SAMPLES;
	ctx->energy = FPM_rms(rx, (unsigned short)*rx_count);

	if (*word8 != 0) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("Abort waiting for silence!");
		ctx->status = FAX_CLASS1_OK_NO_CARRIER;
		ctx->state = CLASS1_IDLE_STATE;
		return 0;
	}
	if (ctx->energy > CLASS1_SILENCE_THRESHOLD) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("Energy %d > silence treshold\n",
					     ctx->energy);
		ctx->silence_blocks = 0;
		ctx->status = FAX_CLASS1_NO_MESSAGE;
		*word8 = 5;
		return 0;
	}
	if (dsplibs_debug_level > 1)
		dsplibs_debug_printf("Energy %d < silence treshold...\n",
				     ctx->energy);
	ctx->silence_blocks++;
	if (ctx->silence_blocks >= (unsigned int)ctx->countdown) {
		ctx->status = FAX_CLASS1_OK_NO_CARRIER;
		ctx->state = CLASS1_IDLE_STATE;
	}
	return 0;
}
