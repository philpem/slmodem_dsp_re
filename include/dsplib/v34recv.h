/*
 * v34recv.h -- ITU-T V.34: the receiver sub-object at V34object+0x264.
 *
 * ONE MAP, NOT FOUR.  This object was reconstructed piecemeal -- the detector
 * needed its flags word, `decision` its target and result, `agcadapt` its
 * gain state, `V34descrambler` its shift register -- and each got its own
 * partial struct.  Four maps of one object is exactly what finding 100 said
 * not to do, and doing it hid the fact below.
 *
 * THE AGC FREEZE AND THE DETECTOR-PENDING FLAG ARE THE SAME BIT.  `agcadapt`
 * tests `byte 0x123 & 2`; `tone_detect` clears `short 0x122 & 0x200`.  Byte
 * 0x123 is the high half of the short at 0x122, and its bit 1 is the short's
 * bit 9 -- 0x200.  So the AGC is frozen exactly while a detector is armed
 * and has not yet seen signal, and starts adapting the moment one does.
 * Two functions reconstructed weeks apart, sharing a flag neither knew about.
 *
 * Offsets are from +0x264, which is what every caller passes.  Fields with
 * no name yet are `fNNN` after their offset; the pads are not a claim about
 * their contents.
 */

#ifndef DSPLIB_V34RECV_H
#define DSPLIB_V34RECV_H

#ifdef __cplusplus
extern "C" {
#endif

struct v34_receiver {
	unsigned char pad_000[0x120 - 0x0];
	short           f120;            /* +0x120 */
	unsigned short  flags;           /* +0x122 */
	short           f124;            /* +0x124 */
	short           best_index;      /* +0x126 */
	short           f128;            /* +0x128 rxtiming: output count */
	short           f12a;            /* +0x12a V34demodulate: samples held */
	/*
	 * +0x12c.  ONE LOCATION, TWO WIDTHS.  V34demodulate and V34agc
	 * accumulate a 32-bit sum of squared gained samples here; agcadapt
	 * reads `movzwl 0x12e` -- the high half of that same int -- as its
	 * measurement.  So the AGC's input is the energy sum divided by
	 * 65536, and the two were only ever separate fields because they
	 * were reconstructed by different functions weeks apart.
	 *
	 * The union spells the aliasing out rather than casting a pointer,
	 * which -O2 is entitled to reorder.  It assumes a little-endian
	 * layout, as the whole port does.
	 */
	union {
		int             sum;        /* +0x12c the accumulator      */
		struct {
			short           lo;         /* +0x12c              */
			unsigned short  agc_input;  /* +0x12e  == sum >> 16*/
		} h;
	} energy;
	short *         rx_samples;      /* +0x130 */
	short           agc_level;       /* +0x134 */
	short           agc_gain;        /* +0x136 */
	short           agc_accum;       /* +0x138 */
	short           agc_step;        /* +0x13a */
	short           rms_buf[36];     /* +0x13c V34demodulate: 36 samples for the RMS */
	unsigned char pad_184[0x19c - 0x184];
	short           f19c;            /* +0x19c its index */
	unsigned char pad_19e[0x1a0 - 0x19e];
	int             f1a0;            /* +0x1a0 */
	unsigned        scrambler_sr;    /* +0x1a4 */
	unsigned char pad_1a8[0x1ac - 0x1a8];
	short           f1ac;            /* +0x1ac rxtiming: fractional phase */
	short           f1ae;            /* +0x1ae   its increment */
	short           f1b0;            /* +0x1b0   its wrap */
	unsigned char pad_1b2[0x1b4 - 0x1b2];
	const short *   carrier;         /* +0x1b4 V34demodulate: sin then cos */
	short           f1b8;            /* +0x1b8   phase increment */
	short           f1ba;            /* +0x1ba   half-length */
	short           f1bc;            /* +0x1bc   phase */
	unsigned char pad_1be[0x1c0 - 0x1be];
	short           f1c0;            /* +0x1c0 */
	unsigned char pad_1c2[0x1c8 - 0x1c2];
	int             f1c8;            /* +0x1c8 */
	short           f1cc;            /* +0x1cc */
	short           f1ce;            /* +0x1ce */
	short           f1d0;            /* +0x1d0 */
	short           baud;            /* +0x1d2 */
	short           f1d4;            /* +0x1d4 */
	unsigned char pad_1d6[0x1d8 - 0x1d6];
	int             f1d8;            /* +0x1d8 */
	unsigned char pad_1dc[0x1e0 - 0x1dc];
	int             f1e0;            /* +0x1e0 */
	int             f1e4;            /* +0x1e4 */
	int             f1e8;            /* +0x1e8 */
	unsigned char pad_1ec[0x1f0 - 0x1ec];
	short           f1f0;            /* +0x1f0 */
	short           f1f2;            /* +0x1f2 */
	short           f1f4;            /* +0x1f4 */
	unsigned char pad_1f6[0x1f8 - 0x1f6];
	int             f1f8;            /* +0x1f8 */
	unsigned char pad_1fc[0x200 - 0x1fc];
	short           f200;            /* +0x200 */
	short           f202;            /* +0x202 */
	short           f204;            /* +0x204 */
	short           f206;            /* +0x206 */
	short           f208;            /* +0x208 rxtiming IIR state, I(-1) */
	short           f20a;            /* +0x20a   Q(-1) */
	/*
	 * +0x20c.  The same trick again, and just as load-bearing: `decision`
	 * writes the winning constellation point here as one 32-bit word (real
	 * in the low half, imaginary in the high), while rxtiming uses those
	 * four bytes as the two second-order history taps of its timing IIR.
	 */
	union {
		int             point;   /* +0x20c decision(): packed (re,im) */
		struct {
			short   i;       /* +0x20c rxtiming: I(-2)           */
			short   q;       /* +0x20e   Q(-2)                   */
		} iir2;
	} dp;
	short           target_re;       /* +0x210 */
	short           target_im;       /* +0x212 */
	unsigned char pad_214[0x218 - 0x214];
	short           f218;            /* +0x218 */
	short           f21a;            /* +0x21a */
	short           f21c;            /* +0x21c */
	unsigned char pad_21e[0x220 - 0x21e];
	int             f220;            /* +0x220 */
	short           f224;            /* +0x224 */
	unsigned char pad_226[0x228 - 0x226];
	int             f228;            /* +0x228 */
	unsigned char pad_22c[0x22e - 0x22c];
	short           f22e;            /* +0x22e */
	short           f230;            /* +0x230 */
	unsigned char pad_232[0x240 - 0x232];
	short           f240;            /* +0x240 demodulated I */
	short           f242;            /* +0x242 demodulated Q */
	short           f244;            /* +0x244 previous I */
	short           f246;            /* +0x246 previous Q */
	int             f248;            /* +0x248 */
	int             f24c;            /* +0x24c */
	unsigned char pad_250[0x252 - 0x250];
	short           f252;            /* +0x252 */
	short           f254;            /* +0x254 */
	short           f256;            /* +0x256 */
	short           f258;            /* +0x258 */
	short           f25a;            /* +0x25a */
	short           f25c;            /* +0x25c */
	short           f25e;            /* +0x25e */
	short           f260;            /* +0x260 */
	unsigned char pad_262[0x27a - 0x262];
	short           timing_out[64];  /* +0x27a rxtiming writes its metric here */
	unsigned char pad_2fa[0x798 - 0x2fa];
	short           f798;            /* +0x798 */
};

/*
 * `flags` at +0x122.
 *
 * Bit 9 is set by the handshake when it arms a tone detector, cleared by
 * tone_detect when the level first crosses its floor -- and read by agcadapt
 * as "do not adapt yet".  One bit, three readers.
 */
#define V34_RX_FLAG_DET_PENDING	0x0200
#define V34_RX_FLAG_AGC_FREEZE	V34_RX_FLAG_DET_PENDING

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34RECV_H */
