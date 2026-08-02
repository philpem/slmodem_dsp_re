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
	unsigned short  flags;           /* +0x122 see V34_RX_* below */
	short           f124;            /* +0x124 */
	short           best_index;      /* +0x126 decision: nearest point index */
	unsigned char pad_128[0x12a - 0x128];
	short           f12a;            /* +0x12a */
	short           f12c;            /* +0x12c rxinit clears 0x12c..0x12f as one int */
	unsigned short  agc_input;       /* +0x12e agcadapt: the new energy measurement */
	short *         rx_samples;      /* +0x130 -> the receive queue output at +0x10c */
	short           agc_level;       /* +0x134 */
	short           agc_gain;        /* +0x136 */
	short           agc_accum;       /* +0x138 D34: rxinit seeds this from a stale register */
	short           agc_step;        /* +0x13a */
	unsigned char pad_13c[0x1a0 - 0x13c];
	int             f1a0;            /* +0x1a0 */
	unsigned        scrambler_sr;    /* +0x1a4 */
	unsigned char pad_1a8[0x1b8 - 0x1a8];
	short           f1b8;            /* +0x1b8 */
	unsigned char pad_1ba[0x1bc - 0x1ba];
	short           f1bc;            /* +0x1bc */
	unsigned char pad_1be[0x1c0 - 0x1be];
	short           f1c0;            /* +0x1c0 */
	unsigned char pad_1c2[0x1c8 - 0x1c2];
	int             f1c8;            /* +0x1c8 */
	short           f1cc;            /* +0x1cc */
	short           f1ce;            /* +0x1ce */
	short           f1d0;            /* +0x1d0 */
	short           baud;            /* +0x1d2 starts at 2400 */
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
	short           f208;            /* +0x208 */
	short           f20a;            /* +0x20a */
	int             decision_point;  /* +0x20c decision: the winning point */
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
	unsigned char pad_232[0x244 - 0x232];
	short           f244;            /* +0x244 */
	short           f246;            /* +0x246 */
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
	unsigned char pad_262[0x798 - 0x262];
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
