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

#include "dsplib/v34filt.h"	/* struct v34_echo: the FSK delay line is one */
#include "dsplib/v34rx.h"	/* struct v34_queue: the object owns both      */

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

/*
 * The discriminator's delay line.
 *
 * NOT A DISTINCT OBJECT.  The pointer at V.34 object offset +0x80c4 that
 * fskdetect dereferences is the first echo canceller's `coeff_frac` field,
 * so this is a VIEW onto `struct v34_echo::coeff_frac` -- the delay line
 * occupies 49 of that array's 144 shorts.  The two are never live at the
 * same time: the FSK receiver carries phase 2's INFO messages and the echo
 * canceller's fractional coefficients only matter in data mode.
 *
 * See docs/findings.md, 100.  When V34RX.c defines the parent these must be
 * ONE region with two readings; declaring them as two members would
 * double-allocate and break the aliasing the original depends on.
 */
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
	unsigned char unmapped_0000[0x264];
	struct v34_queue rxq;				/* +0x264 */
	int rxq_ring_tail[V34_RXQ_RING - 1];		/* to +0x370 */
	unsigned char unmapped_0370[0x402 - 0x370];
	/*
	 * Non-zero makes fskdemodulate return without doing anything -- not
	 * even running the detector -- so it reads as "the FSK receiver is
	 * switched off".  Nothing here sets it.
	 */
	short fsk_inhibit;				/* +0x402 */
	unsigned char unmapped_0404[0x2074 - 0x404];
	/*
	 * A pointer V34InitializeImplementationSpecific aims at +0x146c of
	 * this same object.  What lives there is not yet known; the echo
	 * pre-filter is at +0x2078, immediately after, so this is plausibly
	 * its input buffer.
	 */
	void *p_2074;					/* +0x2074 */
	struct v34_echo_prefilter prefilter;		/* +0x2078 */
	unsigned char unmapped_20e0[0x20e0 - (0x2078 + 0x68)];
	unsigned char scratch_20e0[0x20];		/* +0x20e0 */
	unsigned char unmapped_2100[0x210c - 0x2100];
	unsigned char scratch_210c[0x100];		/* +0x210c */
	unsigned char unmapped_220c[0x221c - 0x220c];
	struct v34_queue txq;				/* +0x221c */
	int txq_ring_tail[V34_TXQ_RING - 1];		/* to +0x25c0 */
	short f25c0;					/* +0x25c0 */
	short f25c2;					/* +0x25c2 bit 9 gates the echo feed */
	unsigned char unmapped_25c4[0x25c6 - 0x25c4];
	short f25c6;					/* +0x25c6 */
	unsigned char unmapped_25c8[0x25cc - 0x25c8];
	int f25cc;					/* +0x25cc */
	short f25d0;					/* +0x25d0 symbol re */
	short f25d2;					/* +0x25d2 symbol im */
	short f25d4;					/* +0x25d4 tx scale  */
	unsigned char unmapped_25d6[0x2aa4 - 0x25d6];
	short f2aa4;					/* +0x2aa4 */
	unsigned char unmapped_2aa6[0x3550 - 0x2aa6];
	short f3550;					/* +0x3550 */
	short f3552;					/* +0x3552 */
	unsigned char unmapped_3554[0x35a8 - 0x3554];
	/*
	 * The bulk-delay ring feeding the second echo canceller.  Its wrap is
	 * BRANCHLESS -- idx &= -(len > idx), resetting to zero rather than
	 * subtracting -- unlike every other ring here.  Finding 116.
	 */
	int bulk_head;					/* +0x35a8 */
	int bulk_tail;					/* +0x35ac */
	short *bulk_ring;				/* +0x35b0 */
	int bulk_len;					/* +0x35b4 */
	unsigned char unmapped_35b8[0x80b8 - 0x35b8];
	/*
	 * The two echo cancellers and the arrays they point at, one
	 * contiguous block each (finding 98).  Declared here rather than in
	 * v34filt.h because this is the struct that owns the storage; the
	 * descriptors are `struct v34_echo` and the arrays are named
	 * separately because the object's own pointers are what tie them
	 * together, not adjacency.
	 *
	 * `echo0.coeff_frac` points at `echo0_frac`, and that array is ALSO
	 * DPSK.c's FSK delay line -- see finding 100 and the note on
	 * `struct v34_fskdelay` above.  One region, two readings; there is
	 * deliberately no separate member for the second.
	 */
	struct v34_echo echo0;				/* +0x80b8 */
	short echo0_frac[V34_ECHO_TAPS];		/* +0x80d8 */
	short echo0_dline[V34_ECHO_DLEN];		/* +0x81f8 */
	short echo0_hist[V34_ECHO_TAPS];		/* +0x8ee8 */
	unsigned char gap_9008[0x9018 - 0x9008];
	short echo0_coeff[V34_ECHO_TAPS];		/* +0x9018 */
	struct v34_echo echo1;				/* +0x9138 */
	short echo1_frac[V34_ECHO_TAPS];		/* +0x9158 */
	short echo1_dline[V34_ECHO_DLEN];		/* +0x9278 */
	short echo1_hist[V34_ECHO_TAPS];		/* +0x9f68 */
	unsigned char gap_a088[0xa098 - 0xa088];
	short echo1_coeff[V34_ECHO_TAPS];		/* +0xa098 */
	short hilbert[V34_HILBERT_TAPS];		/* +0xa1b8 */
	unsigned char unmapped_a238[0xaad0 - 0xa238];
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
