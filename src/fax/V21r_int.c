/*
 * V21r_int.c -- split out of the merged v21.c so the definitions sit in the
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
 * One block through the receive chain.
 *
 * The DSP block is re-read from the handle at every use rather than cached,
 * because the object re-reads it: four separate `mov 0x50(%edi),%e?x` at
 * 0x0a5764, 0x0a5772, 0x0a57a0/0x0a57c8 and 0x0a57f7, with `%edi` holding the
 * handle throughout.  A local pointer would have lived in a callee-saved
 * register across the calls instead.
 *
 * `FPM_AGC_agc` IS GIVEN A FOURTH ARGUMENT BY THE OBJECT and its return value
 * is used, and it has neither.  The extra argument is dead stack setup and is
 * not reproduced -- an argument the callee never loads has no observable
 * effect, which is `V21RX_delete`'s note above and `v22data.c`'s at its own
 * call site.  The value in %eax on return is `agc.signal`, the same quantity
 * the function's last store put in the state, so the field is read here
 * instead; `src/pump/v23/bwchdem.c` records that reading and is tested on it.
 *
 * The squelch loop indexes with an `unsigned short` and compares `jb`
 * (0x0a57be..0x0a57c4), so a count with the top bit set walks forward rather
 * than not at all.
 */
unsigned short
DemodDataV21(void *modem, short *in, short *bits, unsigned short count)
{
	struct v21_rx *rx = (struct v21_rx *)modem;
	short nsamples;

	FPM_AGC_agc(&rx->dsp->agc, in, count);

	rx->dsp->int_0004 = rx->dsp->agc.signal;
	rx->dsp->int_0008 = 1;

	if (FPM_MTD_detect(rx->dsp->mtd, in, (short)count)
	    != FPM_MTD_ABSENT) {
		rx->dsp->int_0008 = 0;

		if (rx->hdx->handler != RxHdxDataV21) {
			unsigned short i;

			for (i = 0; i < count; i++)
				in[i] = 0;
		}
	}

	nsamples = FPM_MRF_filter(&rx->dsp->mrf, in,
				  rx->dsp->mag, (short)count);

	return (unsigned short)
		FPM_FSD_demodulate(&rx->dsp->fsd,
				   rx->dsp->mag,
				   (unsigned short *)(void *)bits,
				   (unsigned short)nsamples);
}

int
CarrierDetectV21(void *modem)
{
	struct v21_rx_dsp *dsp = ((struct v21_rx *)modem)->dsp;

	return dsp->int_0004 & dsp->int_0008;
}

/*
 * Rectify the demodulator's trace into the block's own buffer.
 *
 * The three fields are lifted into locals before the loop because the object
 * lifts them: 0x74, 0x70 and 0x90 are all loaded at 0x0a583c..0x0a5843,
 * ahead of the first test.  Read through the struct each time they would not
 * be, since the store into `mag` may alias them.
 *
 * The absolute value is written out rather than called: C's `abs()` is
 * undefined at INT_MIN and this one is reached with -32768, which the object
 * turns into -32768 by truncating 32768 back to a short.  That corner is
 * driven by the differential test rather than reasoned about.
 *
 * THE SECOND LOOP HAS NO BODY IN THE OBJECT, and this is not an omission
 * here.  0x0a5868..0x0a5875 counts from zero to the same bound with nothing
 * between the increment and the test, and the return is a literal zero.  The
 * natural reading is an accumulation whose result became dead before the
 * compiler saw it, but the object does not say that and nothing here claims
 * it.  Reproduced because it is there; it has no observable effect.  D1038.
 */
int
GetSNRV21(void *modem)
{
	struct v21_rx_dsp *dsp = ((struct v21_rx *)modem)->dsp;
	short n = dsp->fsd.last_count;
	const short *trace = dsp->fsd.trace;
	short *mag = dsp->mag;
	short i;

	for (i = 0; i < n; i++) {
		int v = trace[i];

		mag[i] = (short)(v < 0 ? -v : v);
	}

	for (i = 0; i < n; i++)
		;

	return 0;
}
