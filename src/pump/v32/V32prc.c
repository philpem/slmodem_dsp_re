/*
 * V32prc.c -- ITU-T V.32 / V.32bis: the sequence generator and detector, and
 *             the tone-detector retune.
 *
 * Reconstructed from dsplibs.o.  The blob's STT_FILE order places this file
 * between V32org.c and V32rxhdx.c, and its .text run is
 * [SetToneDetect 0x083600, V32RxHdxModem 0x0838f0) -- eight functions.
 *
 *   SetToneDetect       .text 0x083600  110
 *   InitGenSequence     .text 0x083670   68
 *   GenSequence         .text 0x0836c0  118
 *   InitDetSequence     .text 0x083740   52
 *   DetSequence         .text 0x083780  275
 *   GetSequence         .text 0x0838a0   11
 *   LoadReg             .text 0x0838b0   26
 *   StoreReg            .text 0x0838d0   29
 *
 * SetToneDetect came from v32fpctl.c; the other seven from v32seq.c.  Every
 * body is the text it was reconstructed in, moved verbatim.  See F11395.
 */

#include "dsplib/v32fpctl.h"

#include "dsplib/v32data.h"
#include "dsplib/v32hdx.h"
#include "dsplib/v32seq.h"
#include "dsplib/v32struct.h"
#include "dsplib/fpm_tone.h"

#define HDX(m)		((m)->hdx)

/*
 * Retune the first tone detector.
 *
 * It rebuilds the object from a COPY OF ITS OWN CONFIGURATION with the
 * frequency replaced, which is why `struct fpm_tone_cfg` being exactly the
 * first 36 bytes of `struct fpm_tone` matters: the object copies nine dwords
 * off the front of the tone object onto the stack and hands that back.
 *
 * The tone pointer is loaded TWICE, once for the copy and once for the call.
 * That is the object's, and it is what a source that names the field at both
 * sites gives.
 */
void
SetToneDetect(struct v32_modem *modem, short hz)
{
	struct v32_hdx *hdx = HDX(modem);
	struct fpm_tone_cfg cfg;

	cfg = ((struct fpm_tone *)hdx->tone0)->cfg;
	cfg.freq = hz;
	FPM_TONE_create((struct fpm_tone *)hdx->tone0, &cfg);
}

void
InitGenSequence(struct v32_modem *modem, unsigned short pattern, unsigned short total,
		unsigned short width)
{
	struct v32_hdx *hdx = (struct v32_hdx *)
		modem->hdx;
	/* The object retains the promoted quotient until the two short stores.
	 * A short local adds an unsupported movzwl after the decrement. */
	int top = total / width - 1;	/* D401 */

	hdx->gen_width = width;
	hdx->gen_pattern = pattern;
	hdx->gen_index_mask = top;
	hdx->gen_index = top;
	hdx->gen_mask =
		(unsigned short)((1u << width) - 1u);		/* D402 */
}

/*
 * Emit `count` fields of the pattern.
 *
 * `width` and `pattern` are read once, before the loop; `mask` and
 * `index_mask` are read INSIDE it, every iteration.  That is not a choice
 * made here -- the first two are read before the first store to `*out` and
 * the last two after it, and a store through a `short *` may alias them, so
 * the compiler cannot hoist what follows it.  Written in the object's order
 * so the same thing happens.
 */
void
GenSequence(struct v32_modem *modem, short *out, unsigned short count)
{
	struct v32_hdx *hdx = (struct v32_hdx *)
		modem->hdx;
	int width = hdx->gen_width;
	int pattern = hdx->gen_pattern;
	unsigned short index = hdx->gen_index;
	unsigned short i;

	for (i = 0; i < count; i++) {
		*out++ = (short)((pattern >> (index * width)) &
				 hdx->gen_mask);	/* D402 */
		index = (unsigned short)((index - 1) &
					 hdx->gen_index_mask);
	}

	hdx->gen_index = index;
}

void
InitDetSequence(struct v32_modem *modem, int target, int mask, int out_mask,
		unsigned short width)
{
	struct v32_hdx *hdx = (struct v32_hdx *)
		modem->hdx;

	hdx->det_width = width;
	hdx->det_out_mask = out_mask;
	hdx->det_target = target;
	hdx->det_mask = mask;
	hdx->det_reg = 0;
	hdx->det_match = 0;
}

/*
 * Watch `count` words for the armed pattern.
 *
 * Two things about this are worth stating because neither is what a reader
 * expects.
 *
 * The inner loop DOES NOT STOP at the match.  It runs all `width` bits of the
 * word out, and a second match later in the same word overwrites the first --
 * so what is left at V32HDX_DET_MATCH is the LAST match in the word the
 * detector fired on, not the first.  The `found` flag is only tested after
 * the word is finished.
 *
 * The register is written back on both exits.  On the no-match path that is
 * the whole point: the detector keeps its position between calls, so a
 * pattern straddling two calls is still found.
 */
short
DetSequence(struct v32_modem *modem, const short *data, unsigned short count)
{
	struct v32_hdx *hdx = (struct v32_hdx *)
		modem->hdx;
	int nbits = hdx->det_width;
	int reg = hdx->det_reg;
	int mask = hdx->det_mask;
	int target = hdx->det_target;
	short nread = 0;
	unsigned short i;

	for (i = 0; i < count; i++) {
		unsigned int word = (unsigned short)*data++;
		int found = 0;
		short bit;

		nread++;
		for (bit = 0; bit < nbits; bit++) {
			reg = (reg << 1) |
			      (int)((word >> (nbits - 1 - bit)) & 1u);

			/*
			 * A BITWISE AND, and the object's own encoding: both
			 * tests become `sete`/`setne` bytes with no branch
			 * between them.  Both operands are pure, so `&&`
			 * cannot differ behaviourally -- `&` is written
			 * because it is what the instructions say, the same
			 * call src/pump/b103/b103fp.c makes for
			 * CarrierDetectB103.
			 */
			if (((reg & mask) == target) & (reg != -1)) {
				hdx->det_match = reg & hdx->det_out_mask;
				hdx->det_reg = reg;
				found = 1;
			}
		}

		if (found)
			return nread;
	}

	hdx->det_reg = reg;

	return -1;
}

int
GetSequence(struct v32_modem *modem)
{
	struct v32_hdx *hdx = (struct v32_hdx *)
		modem->hdx;

	return hdx->det_match;
}

short
LoadReg(struct v32_modem *modem, short reg)
{
	short value = 0;

	if (reg >= 0 && reg <= V32HDX_NREGS - 1) {
		struct v32_hdx *hdx = (struct v32_hdx *)
			modem->hdx;

		value = hdx->regs[reg];
	}

	return value;
}

void
StoreReg(struct v32_modem *modem, short value, short reg)
{
	if (reg >= 0 && reg <= V32HDX_NREGS - 1) {
		struct v32_hdx *hdx = (struct v32_hdx *)
			modem->hdx;

		hdx->regs[reg] = value;
	}
}
