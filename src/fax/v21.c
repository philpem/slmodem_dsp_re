/*
 * v21.c -- ITU-T V.21 (the fax control channel): the receive and transmit
 * primitives.
 *
 * Reconstructed from dsplibs.o:
 *
 *   V21RX_delete      .text 0x099270   123
 *   V21RX_modem       .text 0x0a1c40   127
 *   V21TX_status      .text 0x0a2c00    96
 *   CarrierDetectV21  .text 0x0a5820    16
 *   GetSNRV21         .text 0x0a5830    77
 *   ModDataV21        .text 0x0a5880    87
 *   TxNoCarrierV21    .text 0x0a58e0   103
 *
 * THESE ARE SEVEN LEAVES OF THREE DIFFERENT CLUSTERS, not one author file:
 * 0x099270 sits with the constructors and destructors, 0x0a1c40 with the
 * half-duplex machine, and 0x0a5820 onward with the per-modulation data
 * paths.  They are collected here because they are the V.21 work that is
 * startable, and `include/dsplib/v21fax.h` says what each one establishes.
 * The order below is the object's own address order.
 *
 * The functions are laid out in the object's address order, which is the one
 * lever this tree has on register allocation across a translation unit
 * (finding F7796) -- it is not a claim that the author had them in one file.
 *
 * Everything here takes a `void *` handle, because neither `V21TX_create`
 * (0x0992f0) nor `V21RX_create` (0x098e70) is reconstructed and naming their
 * fields now would be guessing.  See the header for the ruling and for where
 * every offset used below comes from.
 */

#include <string.h>

#include "dsplib/fpm_fsd.h"
#include "dsplib/fpm_fsm.h"
#include "dsplib/fpm_mrf.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/sysdep.h"
#include "dsplib/v21fax.h"

/*
 * The handle is re-read from the caller's argument before every free rather
 * than cached in a local, because the object re-reads it: five separate
 * `mov 0x50(%ebx),%eax` between 0x099278 and 0x0992c8.
 *
 * THE LITERAL 1 IN THE SECOND ARGUMENT SLOT IS NOT REPRODUCED.  The object
 * puts one there before `FPM_FSD_free` and `FPM_MRF_free`, both of which take
 * a single argument and never load a second -- so it is dead stack setup,
 * presumably from a version where they took a `fresh` flag like their init
 * counterparts.  `B103FP_delete` has exactly the same three call sites and
 * the same note; there is nothing to reproduce, because an argument the
 * callee never loads has no observable effect.
 *
 * There is no NULL guard on anything, and the last free releases the handle
 * unconditionally even when the caller supplied it.  Both reproduced; see
 * docs/deviations.md D1039.
 */
void
V21RX_delete(void *modem)
{
	FPM_MTD_delete(V21RX_DSP(modem)->mtd);
	FPM_FSD_free(&V21RX_DSP(modem)->fsd);
	FPM_MRF_free(&V21RX_DSP(modem)->mrf);

	sysdep_free(V21RX_DSP(modem)->mag);
	sysdep_free(V21RX_DSP(modem));
	sysdep_free(V21RX_HDX(modem));
	sysdep_free(modem);
}

/*
 * One block of receive.
 *
 * `remaining` is the count the handler is still working through, and it is
 * held UNSIGNED while `before` is a `short`: the object sign-extends the
 * previous count into the subtraction (`movswl %cx,%ebx` at 0x0a1c70) and
 * zero-extends the new one (`movzwl %cx,%edx` at 0x0a1c91).  The second
 * extension feeds a 32-bit subtract, so it is FORCED and not the free kind
 * -- CLAUDE.md's rule for reading a codegen difference, applied the way round
 * it is meant to be.
 *
 * The loop is a do-while: `*count` of zero on entry still dispatches once.
 *
 * The return is the 32-bit word that starts at the status byte, taken with
 * `memcpy` rather than a cast for the reason `B103FP_modem` takes its
 * identically shaped one that way.
 */
int
V21RX_modem(void *modem, short *in, short *out, short *count)
{
	unsigned short remaining;
	short total = 0;
	int word;

	V21RX_FLAGS(modem) &= (unsigned char)~V21RX_FLAG_ERROR;

	remaining = (unsigned short)*count;
	do {
		short before = (short)remaining;
		short n;

		n = V21RX_HDX(modem)->handler(modem, in, out, count);
		remaining = (unsigned short)*count;

		out += n;
		in += before - remaining;
		total = (short)(total + n);
	} while (remaining != 0);

	*count = total;

	memcpy(&word, V21RX_STATUS_AT(modem), sizeof word);
	return word;
}

/*
 * Fill the caller's status block.
 *
 * The last statement ASSIGNS the flags byte rather than merging into it, so
 * the two bits cleared four statements earlier are cleared for nothing and
 * every other bit the caller had is lost.  That is the object's; see D1037.
 */
int
V21TX_status(void *modem, struct v21_status *st)
{
	if (st == NULL)
		return 0;

	st->protocol = (short)V21TX_PROTOCOL(modem);
	st->tx_bps = V21_STATUS_BPS;
	st->rx_bps = 0;
	st->quality = 0;
	st->snr = 0;
	st->short_0a = 0;
	st->short_0c = 0;
	st->flags &= (unsigned char)~(V21_STATUS_BIT0 | V21_STATUS_BIT1);
	st->short_10 = 0;
	st->short_12 = 0;
	st->flags1 &= (unsigned char)~V21_STATUS1_BIT0;
	st->flags = (unsigned char)(V21TX_FLAGS(modem) & V21_STATUS_BIT2);

	return 1;
}

/* Carrier present: the receive block's two words at once. */
int
CarrierDetectV21(void *modem)
{
	struct v21_rx_dsp *dsp = V21RX_DSP(modem);

	return dsp->int_0008 & dsp->int_0004;
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
	struct v21_rx_dsp *dsp = V21RX_DSP(modem);
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

/*
 * Modulate, then resample.  The handle is re-read after the modulator
 * returns; see the header.
 */
unsigned short
ModDataV21(void *modem, const unsigned short *bits, short *out,
	   unsigned short nbits)
{
	unsigned short nsamples;

	nsamples = (unsigned short)FPM_FSM_modulate(&V21TX_DSP(modem)->fsm,
						    bits,
						    V21TX_DSP(modem)->scratch,
						    nbits);

	return (unsigned short)FPM_MRF_filter(&V21TX_DSP(modem)->mrf,
					      V21TX_DSP(modem)->scratch, out,
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
	short saved_scale = V21TX_DSP(modem)->fsm.cfg.scale;
	unsigned short nsamples;

	V21TX_DSP(modem)->fsm.cfg.scale = 0;
	nsamples = (unsigned short)FPM_FSM_modulate(&V21TX_DSP(modem)->fsm,
						    bits,
						    V21TX_DSP(modem)->scratch,
						    nbits);
	V21TX_DSP(modem)->fsm.cfg.scale = saved_scale;

	return (unsigned short)FPM_MRF_filter(&V21TX_DSP(modem)->mrf,
					      V21TX_DSP(modem)->scratch, out,
					      (short)nsamples);
}

/*
 * The layout above is a claim about a 32-bit object and is asserted as one.
 * The guard is the tree's usual `__SIZEOF_POINTER__` one; `tools/assertlive.py`
 * is what keeps it from quietly reading `#if 0` under a compiler that does not
 * predefine it.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V21_ASSERT_OFF(type, field, off) \
	typedef char v21_off_##field[ \
		((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V21_ASSERT_OFF(struct v21_tx_dsp, fsm, 0x00);
V21_ASSERT_OFF(struct v21_tx_dsp, mrf, 0x10);
V21_ASSERT_OFF(struct v21_tx_dsp, scratch, 0x2c);

V21_ASSERT_OFF(struct v21_rx_dsp, int_0004, 0x04);
V21_ASSERT_OFF(struct v21_rx_dsp, int_0008, 0x08);
V21_ASSERT_OFF(struct v21_rx_dsp, mrf, 0x38);
V21_ASSERT_OFF(struct v21_rx_dsp, fsd, 0x54);
V21_ASSERT_OFF(struct v21_rx_dsp, mtd, 0x8c);
V21_ASSERT_OFF(struct v21_rx_dsp, mag, 0x90);

V21_ASSERT_OFF(struct v21_rx_hdx, handler, 0x04);

V21_ASSERT_OFF(struct v21_status, tx_bps, 0x02);
V21_ASSERT_OFF(struct v21_status, rx_bps, 0x04);
V21_ASSERT_OFF(struct v21_status, quality, 0x06);
V21_ASSERT_OFF(struct v21_status, snr, 0x08);
V21_ASSERT_OFF(struct v21_status, short_0a, 0x0a);
V21_ASSERT_OFF(struct v21_status, short_0c, 0x0c);
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

#endif
