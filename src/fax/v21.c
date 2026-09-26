/*
 * v21.c -- ITU-T V.21 (the fax control channel): the receive and transmit
 * primitives.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V21RX_create      .text 0x098e70  1011
 *   V21RX_delete      .text 0x099270   123
 *   V21TX_create      .text 0x0992f0   757
 *   V21TX_delete      .text 0x0995f0   104
 *   V21RX_modem       .text 0x0a1c40   127
 *   RxHdxErrorV21     .text 0x0a1cc0    59
 *   RxHdxIdleV21      .text 0x0a1d00    81
 *   RxNextStateV21    .text 0x0a1d60   290
 *   RxHdxStartV21     .text 0x0a1e90   522
 *   RxHdxWaitV21      .text 0x0a20a0   440
 *   RxHdxDataV21      .text 0x0a2260   418
 *   V21RX_control     .text 0x0a2410    75
 *   V21RX_status      .text 0x0a2460   132
 *   V21TX_modem       .text 0x0a24f0   182
 *   TxNextStateV21    .text 0x0a25b0   280
 *   TxHdxStartV21     .text 0x0a26d0   380
 *   TxHdxIdleV21      .text 0x0a2850   372
 *   TxHdxDataV21      .text 0x0a29d0   451
 *   V21TX_control     .text 0x0a2ba0    94
 *   V21TX_status      .text 0x0a2c00    96
 *   DemodDataV21      .text 0x0a5740   217
 *   CarrierDetectV21  .text 0x0a5820    16
 *   GetSNRV21         .text 0x0a5830    77
 *   ModDataV21        .text 0x0a5880    87
 *   TxNoCarrierV21    .text 0x0a58e0   103
 *
 * THESE ARE TWENTY-FIVE SYMBOLS OF THREE DIFFERENT CLUSTERS, not one author
 * file: 0x098e70..0x0995f0 sit with the constructors and destructors,
 * 0x0a1c40..0x0a2c00 with the half-duplex machines (receive and, since
 * F9500, transmit), and 0x0a5740 onward with the per-modulation data paths.
 * They are collected here because they are the V.21 work that is startable,
 * and `include/dsplib/v21fax.h` says what each one establishes.  The order
 * below is the object's own address order.
 *
 * THE TRANSMIT HALF-DUPLEX MACHINE IS A SECOND INDIVISIBLE UNIT, F9500's,
 * on the same F8492/F8493 ground the receive one already stood on: each of
 * `TxHdxStartV21`, `TxHdxIdleV21` and `TxHdxDataV21` installs at least one of
 * the other two as a STORED FUNCTION POINTER (a data reference, no `call`),
 * and `TxNextStateV21` -- the canonical, out-of-line copy of the switch all
 * three inline -- installs all three itself.  No proper subset links.
 *
 * THE FIVE RECEIVE-PATH SYMBOLS ARE ONE INDIVISIBLE UNIT and had to be
 * written together.  `DemodDataV21` carries an `R_386_32` against
 * `RxHdxDataV21` on the `cmpl` at 0x0a57a3 -- a DATA reference, not a call --
 * and `RxHdxErrorV21`, `RxHdxIdleV21`, `RxHdxWaitV21` and `RxHdxDataV21` all
 * call `DemodDataV21`.  Under findings F8492/F8493 a reference from `src/` to
 * a symbol this tree has not written is an undefined reference that fails
 * every test binary, so no proper subset of the five links.
 *
 * The functions are laid out in the object's address order, which is the one
 * lever this tree has on register allocation across a translation unit
 * (finding F7796) -- it is not a claim that the author had them in one file.
 *
 * Everything here takes a `void *` handle.  That was originally because
 * NEITHER constructor was reconstructed, so naming their fields would have
 * been guessing; both `V21RX_create` and `V21TX_create` are written now, and
 * each handle is STILL a `void *` because each constructor decided its own
 * layout without settling what most of it MEANS.  `V21RX_create` fixes the
 * receive handle's size at 0x54 and the width of every field, and `v21fax.h`
 * records both -- but +0x1c through +0x4b are written by that one function
 * and read by nothing else in the object, so they keep `type_NNNN` names and
 * a set of offset constants rather than becoming a struct whose members would
 * each be a claim.  `V21TX_create` does the same for the transmit handle at
 * 0x28 bytes: +0x00..+0x1b (`struct v21tx_cfg`, v21cfg.h) is likewise
 * write-only.  See the header for the ruling and for where every offset used
 * below comes from.
 */

#include <string.h>

#include "dsplib/debug.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxfifo.h"
#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v21fax.h"

/*
 * The TRANSMITTER handle is not modelled -- `V21TX_create` is not
 * reconstructed -- so the two transmit entry points below reach it through
 * offsets, exactly as `v17.c` and `v29.c` reach theirs.  The receive side
 * keeps its typed accessors from `v21fax.h`; the two halves are different
 * objects (see the header) and are spelled differently on purpose.
 */





















/* Carrier present: the receive block's two words at once. */




/*
 * The layout above is a claim about a 32-bit object and is asserted as one.
 * The guard is the tree's usual `__SIZEOF_POINTER__` one; `tools/assertlive.py`
 * is what keeps it from quietly reading `#if 0` under a compiler that does not
 * predefine it.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

/*
 * THE TYPEDEF NAME CARRIES `__LINE__`, AND THAT IS NOT DECORATION.  Naming it
 * after the FIELD alone collides the moment two structures here share a field
 * name, and two of them do: `mrf` is in both `v21_tx_dsp` and `v21_rx_dsp`.
 * GCC 14 accepts an identical typedef redefinition (C11 permits it) and said
 * nothing; GCC 3.4.2 rejects it outright, so `make period` -- the tier that
 * decides -- would not compile this file at all.  A discriminator that cannot
 * repeat is what keeps the next added field from bringing it back.
 */
#define V21_CAT2(a, b)	a##b
#define V21_CAT(a, b)	V21_CAT2(a, b)
#define V21_ASSERT_OFF(type, field, off) \
	typedef char V21_CAT(v21_off_line_, __LINE__)[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V21_ASSERT_OFF(struct v21_tx_dsp, fsm, 0x00);
V21_ASSERT_OFF(struct v21_tx_dsp, mrf, 0x10);
V21_ASSERT_OFF(struct v21_tx_dsp, scratch, 0x2c);
V21_ASSERT_OFF(struct v21_tx_hdx, fifo, 0x00);
V21_ASSERT_OFF(struct v21_tx_hdx, int_0004, 0x04);
V21_ASSERT_OFF(struct v21_tx_hdx, handler, 0x08);
V21_ASSERT_OFF(struct v21_tx_hdx, state, 0x0c);
V21_ASSERT_OFF(struct v21_tx_hdx, short_000e, 0x0e);
V21_ASSERT_OFF(struct v21_tx, result, 0x1c);
V21_ASSERT_OFF(struct v21_tx, hdx, 0x20);
V21_ASSERT_OFF(struct v21_tx, dsp, 0x24);

V21_ASSERT_OFF(struct v21_rx_dsp, int_0004, 0x04);
V21_ASSERT_OFF(struct v21_rx_dsp, int_0008, 0x08);
V21_ASSERT_OFF(struct v21_rx_dsp, agc, 0x0c);
V21_ASSERT_OFF(struct v21_rx_dsp, mrf, 0x38);
V21_ASSERT_OFF(struct v21_rx_dsp, fsd, 0x54);
V21_ASSERT_OFF(struct v21_rx_dsp, mtd, 0x8c);
V21_ASSERT_OFF(struct v21_rx_dsp, mag, 0x90);

V21_ASSERT_OFF(struct v21_rx_hdx, handler, 0x04);
V21_ASSERT_OFF(struct v21_rx_hdx, state, 0x08);
V21_ASSERT_OFF(struct v21_rx_hdx, countdown, 0x0a);
V21_ASSERT_OFF(struct v21_rx_hdx, ones_run, 0x0c);
V21_ASSERT_OFF(struct v21_rx_hdx, mark_seq, 0x0e);
V21_ASSERT_OFF(struct v21_rx, status, 0x18);
V21_ASSERT_OFF(struct v21_rx, ptr_001c, 0x1c);
V21_ASSERT_OFF(struct v21_rx, ptr_0024, 0x24);
V21_ASSERT_OFF(struct v21_rx, hdx, 0x4c);
V21_ASSERT_OFF(struct v21_rx, dsp, 0x50);

V21_ASSERT_OFF(struct v21_status, tx_bps, 0x02);
V21_ASSERT_OFF(struct v21_status, rx_bps, 0x04);
V21_ASSERT_OFF(struct v21_status, quality, 0x06);
V21_ASSERT_OFF(struct v21_status, snr, 0x08);
V21_ASSERT_OFF(struct v21_status, short_0a, 0x0a);
V21_ASSERT_OFF(struct v21_status, short_0c, 0x0c);
V21_ASSERT_OFF(struct v21_status, short_0e, 0x0e);
V21_ASSERT_OFF(struct v21_status, short_10, 0x10);
V21_ASSERT_OFF(struct v21_status, short_12, 0x12);
V21_ASSERT_OFF(struct v21_status, flags, 0x14);
V21_ASSERT_OFF(struct v21_status, flags1, 0x15);
V21_ASSERT_OFF(struct v21_status, int_18, 0x18);

/*
 * The two DSP blocks are gapless: every offset above abuts the next, which is
 * what makes the layout a reading of the object rather than a set of
 * independent guesses.  Asserting the sizes is what would catch a sub-struct
 * changing under us.
 */
typedef char v21_tx_dsp_size[(sizeof(struct v21_tx_dsp) == 0x30) ? 1 : -1];
typedef char v21_rx_dsp_size[(sizeof(struct v21_rx_dsp) == 0x94) ? 1 : -1];
typedef char v21_tx_hdx_size[(sizeof(struct v21_tx_hdx) == 0x10) ? 1 : -1];
typedef char v21_tx_result_size[(sizeof(union v21_tx_result) == 4) ? 1 : -1];
typedef char v21_rx_status_word_size[
	(sizeof(union v21_rx_status_word) == 4) ? 1 : -1];
typedef char v21_tx_size[(sizeof(struct v21_tx) == 0x28) ? 1 : -1];
typedef char v21_rx_size[(sizeof(struct v21_rx) == 0x54) ? 1 : -1];

/*
 * The transmit config table is what `V21TX_create` copies onto the handle's
 * head, whole; its size is the literal 28 `V21TX_create` itself carries
 * (0x099328..0x09934d's six-plus-one dword copy), and confirming it here
 * catches a struct-shape slip the same way `t_faxcfg.c`'s `offcheck.py` pass
 * caught one for `v29rx_cfg` (finding F9059).
 */
typedef char v21tx_cfg_size[(sizeof(struct v21tx_cfg) == 0x1c) ? 1 : -1];

#endif
