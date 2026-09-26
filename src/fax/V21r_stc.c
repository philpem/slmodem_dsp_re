/*
 * V21r_stc.c -- split out of the merged v21.c so the definitions sit in the
 * translation unit the object's FILE order gives them.  Bodies moved
 * verbatim; no source text changed.  See finding F11390.
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
 * V21RX_control -- .text 0x0a2410, 75 bytes.
 *
 * `arg == NULL` returns 0 and touches nothing.  Otherwise: `cfg->int_0008`
 * (the receive handle's own head, `struct v21rx_cfg`) is set unconditionally
 * from `arg->int_0004`; `V21RX_HDX(modem)->int_0000` -- the SAME field
 * `RxHdxDataV21` gates demodulation on, above, and whose header comment used
 * to read "nothing written sets it" -- is set to 1 when
 * `V21RXCTL_SET_HDX_INT0000` is set in `arg->flags` and to 0 otherwise;
 * and `V21RXCTL_REINIT` (also in `flags`) calls `V21RX_create(modem,
 * modem)`, the same self-referential reinit `v17fax.h` documents for
 * `V17RX_control` (finding F9470/F9900: the receive handle's head, byte for
 * byte, IS a `struct v21rx_cfg`, so passing it as its own `params` re-copies
 * its current configuration onto itself and reruns construction).
 *
 * NOTHING IN THE OBJECT CALLS THIS DIRECTLY: like `V17RX_control`, its only
 * referrer is the lowercase adapter `v21rx_control` (faxadapt.c), which
 * forwards `arg` unchanged from its own caller -- so `struct v21rx_ctl`
 * reads what the two loads force and no further, `v22ctl.h`'s discipline.
 */
int
V21RX_control(void *modem, const struct v21rx_ctl *arg)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	struct v21rx_cfg *cfg = &rx->cfg;

	if (arg == NULL)
		return 0;

	cfg->int_0008 = arg->int_0004;

	rx->hdx->int_0000 =
		(arg->flags & V21RXCTL_SET_HDX_INT0000) != 0;

	if (arg->flags & V21RXCTL_REINIT)
		V21RX_create(modem, (const struct v21rx_cfg *)modem);

	return 1;
}

/*
 * Fill the caller's status block from the RECEIVER.
 *
 * NOT a mirror of `V21TX_status`.  The rate goes in `rx_bps` and not
 * `tx_bps`, `snr` carries what `GetSNRV21` answered rather than a literal
 * zero, `quality` is the COMPLEMENT of V21RX_FLAG_LOW_SNR (`testb $0x80`
 * followed by `sete`, 0x0a2487), +0x0e is zeroed where the transmit side
 * zeroes +0x0c, and the flags byte is stored as a literal 0 rather than
 * merged from the handle.
 *
 * `short_12` IS THE ONLY ARITHMETIC IN THE FUNCTION and it is a `cltd`/`idiv`
 * over two SIGNED shorts loaded `movswl` (0x0a24b3 and 0x0a24b7), then
 * `imul $0x12c`.  Both operands live in the fsd: +0x76 of the DSP block is
 * `fsd.f22` and +0x66 is `fsd.cfg.bit_samples`, and `FPM_FSD_init` sets the
 * first to half the second -- so for an even `bit_samples` the quotient is 1
 * and the field comes out at 300, which is `rx_bps` again by a different
 * route.  Finding F9132.
 *
 * THE DIVIDE HAS NO GUARD.  A receiver whose fsd was never configured has
 * `bit_samples` zero and this faults; see docs/deviations.md D1098.  It is
 * reproduced, and the test asserts the precondition on BOTH sides rather
 * than driving it.
 */
int
V21RX_status(void *modem, struct v21_status *st)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	short f22, bit_samples;

	if (st == NULL)
		return 0;

	st->protocol = (short)(unsigned short)rx->cfg.chan2;
	st->tx_bps = 0;
	st->rx_bps = V21_STATUS_BPS;
	st->quality = (short)
		((rx->status.byte.flags & V21RX_FLAG_LOW_SNR) == 0);
	st->snr = (short)GetSNRV21(modem);
	st->short_0a = 0;
	st->short_0e = 0;
	st->short_10 = 0;
	st->flags1 &= (unsigned char)~V21_STATUS1_BIT0;
	st->flags = 0;

	f22 = rx->dsp->fsd.f22;
	bit_samples = rx->dsp->fsd.cfg.bit_samples;
	st->short_12 = (short)((2 - 2 * (int)f22 / (int)bit_samples)
			       * V21_STATUS_BPS);

	return 1;
}
