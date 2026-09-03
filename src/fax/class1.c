/*
 * class1.c -- Class 1 fax: the session object's own leaves, plus its
 * dispatcher and destructor.
 *
 * Reconstructed from dsplibs.o class1.c (F1410's method anchors this TU: its
 * four `t` statics start at 0x092c00 and its first global, `_put_silence`,
 * at 0x092b70):
 *
 *   _put_silence                  .text 0x092b70    28
 *   _send_silence_state_init      .text 0x092b90    42
 *   _recieve_silence_state_init   .text 0x092bc0    59
 *   _send_silence_state           .text 0x092c00    64
 *   _recieve_silence_state        .text 0x092c40   274
 *   _idle_state_init              .text 0x092dc0     3
 *   _idle_state                   .text 0x092dd0    72
 *   fax_class1_progress           .text 0x0936d0  1145
 *   fax_class1_status             .text 0x0093b50   150
 *   fax_class1_delete             .text 0x0093bf0   347
 *   fax_class1_info               .text 0x093690    56
 *   fax_class1_GetConstalation    .text 0x093d50     3
 *   _sym_size                     .text 0x093d60   103
 *   _set_modem_rate               .text 0x093dd0   168
 *
 * THIS IS NOT THE FAX PHASE, for the leaf pass's own symbols (F8320's
 * no-entry-point bucket, written on that bucket's own merit); `fax_class1_
 * progress` and `fax_class1_delete` are the first two symbols in this file
 * that ARE reached from elsewhere in the object (each other, and the
 * `_delete_data_*_modem` pair), landing once their own blockers cleared.
 * `fax_class1_status` landed the same way, once `FAXVMI_status` did
 * (finding F10058) -- the note that it was out of scope, calling an
 * unreconstructed callee with no link scaffold (F215), is history now, not
 * a live blocker. `fax_class1_status` sits at the FILE's end, after
 * `fax_class1_progress`, rather than between `fax_class1_progress` and
 * `fax_class1_delete` as the object has it: this file already has
 * `fax_class1_delete` before `fax_class1_progress`, out of address order,
 * and reordering either pair without a period-compiler re-measurement in
 * hand would risk the byte identity already banked. F10058.
 *
 * The spelling `_recieve_...` is the AUTHOR'S, from the symbol table; do
 * not fix it.
 *
 * `states_names` (.rodata 0x9360, 160 bytes) and `status_names` (.rodata
 * 0x9300, 88 bytes): twenty and eleven `{int, char *}` pairs, dumped with
 * their relocations resolved (never trimmed `objdump`, per this tree's own
 * rule about tables of pointers) -- see the tables below for the exact
 * bytes and the strings, which are the author's own and the strongest
 * evidence class this tree recognises.  `fax_class1_progress` is their
 * reader, searching both to log state transitions and status changes.
 *
 * `fax_class1_progress` WAS DECLINED TWICE (F9600's wave, F9802) on
 * `ctx->0x1254`'s meaning; findings F10051/F10052 settle it as a T.30 modem
 * RATE CODE, in (mostly) `_set_modem_rate`'s own code space, and the walk
 * through `ctx->0x1208 (vmi_b) -> link(+0x28) -> int_0014(+0x14)` reaches
 * whichever modulation's own RX object that code names -- `V17RX_OBJ_STATE`
 * (0x60), `V29_OBJ_RX` (0x50) or `V27_OBJ_RX` (0x54) -- each already typed by
 * v17fax.h/v27fax.h/v29fax.h, whose own `V17RXS_SHORT_4FB2`/
 * `V29RX_SHORT_4F62`/`V27RX_Q_FLAG` name the exact three offsets this
 * function tests.  See class1.h and finding F10057 for the full derivation.
 */

#include <stddef.h>

#include "dsplib/class1.h"
#include "dsplib/class1rx.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxfifo.h"
#include "dsplib/faxvmi.h"
#include "dsplib/fpm.h"
#include "dsplib/fpm_iir.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/sysdep.h"
#include "dsplib/v17fax.h"
#include "dsplib/v27fax.h"
#include "dsplib/v29fax.h"

/*
 * The state and status name tables `fax_class1_progress` searches to log a
 * transition -- see D1330 for why these are GLOBAL where the object has them
 * `r` (file-local).  Both are searched linearly by `id`, bounded by the
 * object's own `cmp $0x13,%eax`/`cmp $0xa,%eax` (index, not count-minus-one),
 * so a lookup that runs off either end is a real property of the object and
 * not modelled defensively here.
 *
 * THE SPELLINGS ARE THE AUTHOR'S, byte for byte, `RECIEVE` included -- see
 * class1.h's own note by the `CLASS1_*` macros, which these confirm rather
 * than derive: every string here is one of those macros with the `CLASS1_`
 * prefix stripped (`states_names`) or reproduced verbatim (`status_names`).
 * `struct class1_name` is declared in class1.h, not here, so a test can
 * reach it without a reader in `src/` yet.
 */
struct class1_name states_names[20] = {
	{ CLASS1_T30_SILENCE_BEFORE_PREAMBLE_STATE,
	  "T30_SILENCE_BEFORE_PREAMBLE_STATE" },
	{ CLASS1_T30_PREAMBLE_STATE,		"T30_PREAMBLE_STATE" },
	{ CLASS1_SEND_HDLC_BUFFER_STATE,	"SEND_HDLC_BUFFER_STATE" },
	{ CLASS1_SEND_HDLC_BETWEEN_BUFFER_STATE,
	  "SEND_HDLC_BETWEEN_BUFFER_STATE" },
	{ CLASS1_HDLC_RECEIVE_LOOK_CARRIER_STATE,
	  "HDLC_RECEIVE_LOOK_CARRIER_STATE" },
	{ CLASS1_HDLC_RECEIVE_STATE,		"HDLC_RECEIVE_STATE" },
	{ CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE,
	  "HDLC_RECEIVE_BETWEEN_BUFFERS_STATE" },
	{ CLASS1_HDLC_EMULATE_RECEIVE_STATE,	"HDLC_EMULATE_RECEIVE_STATE" },
	{ CLASS1_IDLE_STATE,			"IDLE_STATE" },
	{ CLASS1_TX_SCRAMBLED_ONES_STATE,	"TX_SCRAMBLED_ONES_STATE" },
	{ CLASS1_TX_DATA_STATE,			"TX_DATA_STATE" },
	{ CLASS1_TX_NULLS_STATE,		"TX_NULLS_STATE" },
	{ CLASS1_RX_LOOK_CARRIER,		"RX_LOOK_CARRIER" },
	{ CLASS1_RX_DATA_STATE,			"RX_DATA_STATE" },
	{ CLASS1_ANSWER_TONE_STATE,		"ANSWER_TONE_STATE" },
	{ CLASS1_SEND_SILENCE_STATE,		"SEND_SILENCE_STATE" },
	{ CLASS1_RECIEVE_SILENCE_STATE,		"RECIEVE_SILENCE_STATE" },
	{ CLASS1_CHDLCTX_OFF_STATE,		"CHDLCTX_OFF_STATE" },
	{ CLASS1_TX_SILENCE_BEFORE_SCRM_ONES,	"TX_SILENCE_BEFORE_SCRM_ONES" },
	{ CLASS1_MAX_STATES,			"MAX_STATES" },
};

struct class1_name status_names[11] = {
	{ FAX_CLASS1_NO_MESSAGE,		"FAX_CLASS1_NO_MESSAGE" },
	{ FAX_CLASS1_OK,			"FAX_CLASS1_OK" },
	{ FAX_CLASS1_ERROR,			"FAX_CLASS1_ERROR" },
	{ FAX_CLASS1_OK_NO_CARRIER,		"FAX_CLASS1_OK_NO_CARRIER" },
	{ FAX_CLASS1_ERROR_NO_CARRIER,		"FAX_CLASS1_ERROR_NO_CARRIER" },
	{ FAX_CLASS1_ERROR_ON_HOOK,		"FAX_CLASS1_ERROR_ON_HOOK" },
	{ FAX_CLASS1_CONNECT,			"FAX_CLASS1_CONNECT" },
	{ FAX_CLASS1_NO_CARRIER,		"FAX_CLASS1_NO_CARRIER" },
	{ FAX_CLASS1_NO_CARRIER_NO_MESSAGE,
	  "FAX_CLASS1_NO_CARRIER_NO_MESSAGE" },
	{ FAX_CLASS1_OTHER_CARRIER,		"FAX_CLASS1_OTHER_CARRIER" },
	{ FAX_CLASS1_ACCEPT_RATE,		"FAX_CLASS1_ACCEPT_RATE" },
};

/*
 * `.bss`/COMMON, 0x4c bytes, all-NULL until `fax_class1_create` (unwritten)
 * installs the nineteen handler addresses.  Declared here, not `static`,
 * for the same reason `_send_silence_state` and its siblings are global
 * (D1053's shape): a test needs to reach it directly, since the writer is
 * still hundreds of symbols away.
 */
class1_state_fn class1_state_functions[19];

/*
 * Zero `count` elements of `buf`.  Trivial in the object -- one loop, no
 * session access -- but the return value is worth a note: the object leaves
 * whatever the loop counter reached in %eax rather than reloading `count`,
 * so it is `count` on a normal exit and 0 when `count` was not positive
 * (the loop never ran).  Written as the loop counter's own value for exactly
 * that reason.
 */
int
_put_silence(short *buf, int count)
{
	int i;

	for (i = 0; i < count; i++)
		buf[i] = 0;
	return i;
}

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

/*
 * `.text` 0x0093bf0, 347 bytes.  Tear the session down, checking (and
 * freeing) eight fields in the object's own order -- `f120c`, `vmi_c`,
 * `f1210`, `vmi_a`, the current data modem, `f1288`, `f1258` -- and finally
 * the session object itself.  Every reload of a just-freed field between
 * calls is the object's own conservative re-read across an opaque
 * `sysdep_free`/`FAXVMI_delete`/etc. call, not cached here.
 *
 * `f120c` and `f1210` share one shape: a pointer that owns exactly one
 * sub-allocation, at +0x10 of what it points to, freed first.  Neither
 * object's type is established beyond that.
 *
 * THE CURRENT DATA MODEM IS TORN DOWN ONLY WHEN BOTH `modem_vmi` AND
 * `vmi_b` ARE NON-NULL -- a single guard on the pair, not two separate
 * ones -- and `f1244` is what selects which of `_delete_data_rx_modem` /
 * `_delete_data_tx_modem` applies (1 for the RX shape).  This is the first
 * function to explain HOW the caller knows which direction `modem_vmi`
 * currently holds.
 *
 * Returns 1 on every path -- `mov $0x1,%eax` before both `ret`s -- and
 * nothing reconstructed reads it back.
 */
int
fax_class1_delete(struct fax_class1 *ctx)
{
	if (ctx->f120c != NULL) {
		void *p = ctx->f120c;

		sysdep_free(*(void **)((char *)p + 0x10));
		sysdep_free(ctx->f120c);
	}
	if (ctx->vmi_c != NULL)
		FAXVMI_delete(ctx->vmi_c);
	if (ctx->f1210 != NULL) {
		void *p = ctx->f1210;

		sysdep_free(*(void **)((char *)p + 0x10));
		sysdep_free(ctx->f1210);
	}
	if (ctx->vmi_a != NULL)
		FAXVMI_delete(ctx->vmi_a);

	if (ctx->modem_vmi != NULL && ctx->vmi_b != NULL) {
		if (ctx->f1244 == 1)
			_delete_data_rx_modem(ctx);
		else
			_delete_data_tx_modem(ctx);
	}

	if (ctx->f1288 != NULL)
		FIFO_delete(ctx->f1288);
	if (ctx->f1258 != NULL)
		FPM_TONE_delete(ctx->f1258);

	sysdep_free(ctx);
	return 1;
}

/*
 * The two linear searches `fax_class1_progress` runs against its own
 * tables.  Both keep scanning to the LAST matching entry rather than
 * stopping at the first (0x093820/0x093842/0x0939f6, none of them a `jne`
 * out of the loop on a hit) -- inert for `states_names`/`status_names`,
 * whose ids are unique, but written to match rather than to assume a
 * `break`.  Both default to "" (the object's own `.rodata.str1.1` empty
 * string at 0x41a4) when nothing matches.
 */
static const char *
class1_state_name(int id)
{
	const char *name = "";
	int i;

	for (i = 0; i <= 19; i++)
		if (states_names[i].id == id)
			name = states_names[i].name;
	return name;
}

static const char *
class1_status_name(int id)
{
	const char *name = "";
	int i;

	for (i = 0; i <= 10; i++)
		if (status_names[i].id == id)
			name = status_names[i].name;
	return name;
}

/*
 * The session dispatcher.  `.text` 0x0936d0, 1,145 bytes.  See class1.h for
 * the nine-step derivation; this comment covers what class1.h does not.
 *
 * THE SAMPLE-SCAN LOOP (step 1) IS REPRODUCED FOR ITS OBSERED EFFECT, NOT
 * ITS PURPOSE -- nothing traced reads `f127c` back outside this function,
 * so what it is FOR is not established. `n / 8` and `n / 16` are plain C
 * division, which is deliberate: the object's own `test`/`js`/`lea 0x7`
 * (or `0xf`)/`sar` sequences are exactly what GCC 3.4.2 emits for signed
 * division by a power of two, not a hand-written shift -- so writing the
 * shifts out would be reproducing the COMPILER's idiom as if it were the
 * AUTHOR's.
 *
 * THE IIR TICK'S ARGUMENTS (step 2) are typed by `FPM_iir_filt_II`'s own
 * signature (`fpm_iir.h`): `rx` is `samples` (filtered in place, which is
 * WHY this function's own `rx` parameter is `short *` and not `const`),
 * `f12dc` is `coeff`, `iir_state` is `state` (sized `4 * sections` =
 * 8 shorts for the literal `sections = 2` here), and the `count` argument
 * is `(short)n` -- `n`'s LOW 16 BITS specifically (`cwtl` sign-extends
 * `%ax`), not `n` itself.
 *
 * THE DISPATCH'S LOCAL COPY (step 3): `local_rx_count` starts as `n`
 * (loaded once, at entry, from `*rx_count`) and is handed to the state
 * handler BY ADDRESS -- class1.h's `class1_state_fn` contract already
 * documents that a handler may read (never write, in the three handlers
 * reconstructed so far) through it.
 *
 * THE RATE-CODE SWITCH (step 7) TESTS FOUR SEPARATE EQUALITIES for V.17,
 * not the two-code RANGES `_set_modem_rate` uses for the same rates --
 * `_set_modem_rate`'s `(unsigned)(code - 0x91) <= 1` covers BOTH the long-
 * and short-training codes (0x91/0x92 and so on); this function's own
 * `cmp $0x91,%ebx` / `cmp $0x79,%ebx` / ... tests each LONG-training code
 * alone. A short-training code reaches neither `if` and falls through this
 * whole step leaving `status` at FAX_CLASS1_NO_MESSAGE.
 */
int
fax_class1_progress(struct fax_class1 *ctx, short *rx, short *tx,
		    int word3, int word4, int *rx_count, int *tx_count,
		    int *word7, int *word8)
{
	int n = *rx_count;
	int local_rx_count = n;
	int code;

	(void)word3;
	(void)word4;

	if (ctx->f127c != 0) {
		int lim8 = n / 8;
		int lim16 = n / 16;
		int i;

		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("At %2d.%02d[sec] Input Buffer: ",
					     ctx->clock_sec, ctx->clock_frac);

		for (i = 0; i < lim8; i++) {
			if (rx[i] != 0)
				ctx->f127c++;
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("%d,", rx[i]);
		}

		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("\n");

		if (ctx->f127c > lim16)
			ctx->f127c = 0;
	}

	ctx->clock_frac += 2;
	if (ctx->clock_frac > 99) {
		ctx->clock_sec++;
		ctx->clock_frac = 0;
	}
	*word7 = 0;
	ctx->status = FAX_CLASS1_NO_MESSAGE;
	if (ctx->f12f0 != 0)
		FPM_iir_filt_II(rx, ctx->f12dc, ctx->iir_state, 2,
				(short)n);

	/*
	 * `class1_state_fn`'s own position-8 slot is `int`, the LEAST claim
	 * (nothing written so far reads it), so this function's genuinely
	 * pointer-typed `word7` is forwarded through it the same
	 * `(void *)(long)`-shaped idiom `faxadapt.h` already names for a
	 * pointer riding through a plain `int` slot -- not a claim that the
	 * callee treats the value as anything but bits.
	 */
	(*class1_state_functions[ctx->state])(ctx, rx, tx, word3, word4,
					      &local_rx_count, tx_count,
					      (int)(long)word7, word8);

	if (ctx->state != ctx->prev_state) {
		const char *new_name = class1_state_name(ctx->state);
		const char *old_name = class1_state_name(ctx->prev_state);

		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec], FAX_CLASS1_STATE: %s ===> %s\n",
			    ctx->clock_sec, ctx->clock_frac, old_name,
			    new_name);
	}
	ctx->prev_state = ctx->state;

	if (*tx_count != CLASS1_BLOCK_SAMPLES) {
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("ERROR: TxSmpCnt != 160 !!!\n");
	}

	if (ctx->delayed_status_countdown != 0) {
		if (--ctx->delayed_status_countdown == 0) {
			ctx->status = ctx->delayed_status;
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "STATUS = DELAYED_STATUS\n");
		}
	}

	code = ctx->modem_rate_code;
	if (ctx->status == FAX_CLASS1_NO_MESSAGE) {
		if (code == 0x91 || code == 0x79 || code == 0x61 ||
		    code == 0x49) {
			if (ctx->f1244 == 1) {
				struct faxvmi_link *link = ctx->vmi_b->link;
				void *modem = (void *)(long)link->int_0014;
				void *obj = *(void **)((char *)modem +
							V17RX_OBJ_STATE);
				short *flag = (short *)((char *)obj +
							V17RXS_SHORT_4FB2);

				if (*flag != 0) {
					*flag = 0;
					ctx->status = FAX_CLASS1_ACCEPT_RATE;
				}
			}
		} else if (code == 0x60 || code == 0x48) {
			if (ctx->f1244 == 1) {
				struct faxvmi_link *link = ctx->vmi_b->link;
				void *modem = (void *)(long)link->int_0014;
				void *obj = *(void **)((char *)modem +
							V29_OBJ_RX);
				short *flag = (short *)((char *)obj +
							V29RX_SHORT_4F62);

				if (*flag != 0) {
					*flag = 0;
					ctx->status = FAX_CLASS1_ACCEPT_RATE;
				}
			}
		} else if (code == 0x30 || code == 0x18) {
			if (ctx->f1244 == 1) {
				struct faxvmi_link *link = ctx->vmi_b->link;
				void *modem = (void *)(long)link->int_0014;
				void *obj = *(void **)((char *)modem +
							V27_OBJ_RX);
				short *flag = (short *)((char *)obj +
							V27RX_Q_FLAG);

				if (*flag != 0) {
					*flag = 0;
					ctx->status = FAX_CLASS1_ACCEPT_RATE;
				}
			}
		}
	}

	if (ctx->status != FAX_CLASS1_NO_MESSAGE) {
		const char *name = class1_status_name(ctx->status);

		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "At %2d.%02d[sec] New STATUS message: %s \n",
			    ctx->clock_sec, ctx->clock_frac, name);
	}

	return ctx->status;
}

/*
 * The object's own zeroed `struct faxvmi_status` template -- `.rodata`
 * 0x0945c, 28 bytes, all-zero (`tabdump.py --sym FAXVMI_STS --type u32
 * --count 7`).  Declared in `faxvmi.h` alongside `FAXVMI_CTL`; defined
 * here because `fax_class1_status`, below, is its first writer.  See F10058.
 */
const struct faxvmi_status FAXVMI_STS = { 0 };

/*
 * `.text` 0x0093b50, 150 bytes.  Report the active VMI handle's status,
 * for whichever of two directions `ctx->state` says is live.
 *
 * A local `struct faxvmi_status` starts as `FAXVMI_STS` (all zero) and has
 * its `modem_status` overwritten with `modem_status` -- the SECOND
 * argument's raw value, forwarded through unmodified, exactly the "IN:
 * forwarded to vxx_status[slot] when non-NULL" contract `faxvmi.h`
 * documents for that field.  The object copies only the template's other
 * six dwords into the local before that overwrite (0x93b55..0x93b8b): the
 * seventh would be dead on arrival, and GCC 3.4.2 at -O3 proves that and
 * drops the copy, which is why this is written as a plain struct
 * assignment rather than six hand-picked field copies.
 *
 * `ctx->state` selects the handle exactly as class1.h's own note on
 * `vmi_a`/`vmi_b` already recorded, from THIS function's evidence: states
 * 4..6 use `vmi_a`, states 12..13 use `vmi_b` (tested as
 * `(unsigned)(state - 12) <= 1`, the same idiom `_set_modem_rate` uses for
 * its own two-code ranges).  Anything else touches nothing and returns 0;
 * either matching range calls `FAXVMI_status` once and returns 1 -- the
 * object's own `mov $0x1,%edx` on both paths, never the call's own return
 * value.
 */
int
fax_class1_status(struct fax_class1 *ctx, void *modem_status)
{
	struct faxvmi_status st = FAXVMI_STS;

	st.modem_status = modem_status;

	if (ctx->state >= 4 && ctx->state <= 6) {
		FAXVMI_status(ctx->vmi_a, &st);
		return 1;
	}
	if ((unsigned)(ctx->state - 12) <= 1) {
		FAXVMI_status(ctx->vmi_b, &st);
		return 1;
	}
	return 0;
}
