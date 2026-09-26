/*
 * V21t_int.c -- split out of the merged v21.c so the definitions sit in the
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
 * Modulate, then resample.  The handle is re-read after the modulator
 * returns; see the header.
 */
unsigned short
ModDataV21(void *modem, const unsigned short *bits, short *out,
	   unsigned short nbits)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	unsigned short nsamples;

	nsamples = (unsigned short)FPM_FSM_modulate(&tx->dsp->fsm,
						    bits,
						    tx->dsp->scratch,
						    nbits);

	return (unsigned short)FPM_MRF_filter(&tx->dsp->mrf,
					      tx->dsp->scratch, out,
					      (short)nsamples);
}

/*
 * The same with the output scale forced to zero across the modulator, so the
 * modulator's phase and the converter's history advance exactly as they would
 * have and the carrier comes back in phase.  The converter runs with whatever
 * scale it was given, which by then is silence anyway.
 */
unsigned short
TxNoCarrierV21(void *modem, const unsigned short *bits, short *out,
	       unsigned short nbits)
{
	struct v21_tx *tx = (struct v21_tx *)modem;
	short saved_scale = tx->dsp->fsm.cfg.scale;
	unsigned short nsamples;

	tx->dsp->fsm.cfg.scale = 0;
	nsamples = (unsigned short)FPM_FSM_modulate(&tx->dsp->fsm,
						    bits,
						    tx->dsp->scratch,
						    nbits);
	tx->dsp->fsm.cfg.scale = saved_scale;

	return (unsigned short)FPM_MRF_filter(&tx->dsp->mrf,
					      tx->dsp->scratch, out,
					      (short)nsamples);
}
