/*
 * v34fsk.h -- ITU-T V.34: the FSK discriminator (DPSK.c).
 *
 * V.34's handshake has to read the V.21-rate FSK that carries the INFO
 * messages, and this is how.  Three stages, in one call:
 *
 *     4 input samples
 *       -> a 3-phase polyphase interpolator, 12 taps per phase   -> 12
 *       -> delay-and-multiply against a 49-entry line            -> 12
 *       -> an 80-tap low-pass, decimating 4:1                    ->  3
 *
 * `fskdemodulate` then slices those 3 into bits with a zero-crossing
 * resynchronised clock.  The file is named DPSK.c and contains neither
 * differential nor phase-shift keying; the STT_FILE entry is what puts these
 * two functions here, via the `fsklpfcoeff600` static they own.
 *
 * See the standing caveat in v34det.h: there is no tier-2 peer for any V.34
 * code *yet*, so "confirmed" here means "identical to the original" and
 * nothing more -- until the hardware peer described there is in place.
 */

#ifndef DSPLIB_V34FSK_H
#define DSPLIB_V34FSK_H

#ifdef __cplusplus
extern "C" {
#endif

#define V34_FSK_PHASES		3	/* interpolation factor            */
#define V34_FSK_TAPS		12	/* taps per interpolator phase     */
#define V34_FSK_BLOCK		4	/* input samples consumed per call */
#define V34_FSK_INTERP		(V34_FSK_BLOCK * V34_FSK_PHASES)
#define V34_FSK_DELAY_LINE	49	/* discriminator delay line        */
#define V34_FSK_LPF_TAPS	80

/*
 * The three interpolator phases, and the post-detection low-pass.
 *
 * All four are the original's own symbols.  intcoef1 and intcoef3 are exact
 * time-reverses of each other and intcoef2 is symmetric, which is the phase
 * decomposition of one 36-tap linear-phase prototype -- see
 * docs/coefficients.md.  They are global in the object, so they are declared
 * rather than made static.
 */
extern const short intcoef1[V34_FSK_TAPS];
extern const short intcoef2[V34_FSK_TAPS];
extern const short intcoef3[V34_FSK_TAPS];

/*
 * The demodulator's state and configuration in one object, 0x16 bytes, which
 * the handshake keeps at V.34 object offset 0xaad0.
 *
 * The first two fields are read by `fskdetect` and the rest only by
 * `fskdemodulate`, but they are one struct because one pointer is passed to
 * both -- fskdemodulate hands its own third argument straight through.
 */
struct v34_fsk {
	short delay;		/* +0x00  discriminator lag, in taps      */
	short offset;		/* +0x02  subtracted from every output    */
	short bit_lo;		/* +0x04  bit shifted in for a <= 0 sample */
	short bit_hi;		/* +0x06  and for a > 0 one               */
	short bit_len;		/* +0x08  samples per bit                 */
	short resync_next;	/* +0x0a  what `next` restarts at         */
	short phase;		/* +0x0c  sample counter                  */
	short next;		/* +0x0e  the next sampling instant       */
	short nbits;		/* +0x10  bits collected so far           */
	short sr;		/* +0x12  the shift register              */
	short prev;		/* +0x14  last sample, for the sign test  */
};

/* The object that holds the discriminator's delay line, via a pointer. */
struct v34_fskdelay {
	unsigned char unmapped_00[0x14];
	short line[V34_FSK_DELAY_LINE];		/* +0x14 .. +0x74 */
};

/*
 * The V.34 object, mapped only where this module reaches into it.
 *
 * This is the top-level `tagV34Object` -- the name comes from the object's
 * one surviving C++ mangling, `_Z14getMPrecvdBitsP12tagV34Object` -- and it
 * is enormous: `datapumpv34` alone touches +0xaa98, and the detector in
 * detector.c lives at +0x3564.  Only the fields DPSK.c uses are named.  The
 * pads are not a claim about what is in them, and the struct's size is not a
 * claim about the object's size; it is a lower bound.
 *
 * Every named offset is checked by a static assertion in dpsk.c, so the
 * padding cannot drift silently once the surrounding translation units
 * arrive and start filling it in.
 *
 * THE OTHER PARTIAL MAP.  v34det.h declares `struct v34_rx`, which is this
 * same object seen from +0x264 -- so its `flags` at +0x122 is this struct's
 * +0x386.  Extend one of the two when the next V.34 file needs a field, and
 * say which; do not start a third.
 */
struct v34_object {
	unsigned char unmapped_0000[0x402];
	/*
	 * Non-zero makes fskdemodulate return without doing anything -- not
	 * even running the detector -- so it reads as "the FSK receiver is
	 * switched off".  Nothing here sets it.
	 */
	short fsk_inhibit;				/* +0x402 */
	unsigned char unmapped_0404[0x80c4 - 0x404];
	struct v34_fskdelay *fsk_delay;			/* +0x80c4 */
	unsigned char unmapped_80c8[0xaad0 - 0x80c8];
	struct v34_fsk fsk;				/* +0xaad0 */
	short fsk_interp[V34_FSK_TAPS + 1];		/* +0xaae6 */
	short fsk_lpf[V34_FSK_LPF_TAPS];		/* +0xab00 */
};

/*
 * Interpolate, discriminate and filter one block.
 *
 * Reads exactly V34_FSK_BLOCK samples from `in` and writes exactly
 * V34_FSK_PHASES to `out`.  Neither length is a parameter: both are built
 * into the loop bounds, so a caller cannot ask for more or less.
 *
 * `cfg` supplies only `delay` and `offset`.
 */
void fskdetect(struct v34_object *obj, const short *in, short *out,
	       const struct v34_fsk *cfg);

/*
 * Run fskdetect over one block and slice the result into bits.
 *
 * Does nothing at all if `obj->fsk_inhibit` is set.  `st` is both the
 * configuration handed to fskdetect and the bit-clock state; the caller reads
 * the recovered bits back out of `st->sr` and `st->nbits`.
 */
void fskdemodulate(struct v34_object *obj, const short *in,
		   struct v34_fsk *st);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34FSK_H */
