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

#include "dsplib/v34det.h"	/* struct v34_dftbin: the retrain detector's   */
#include "dsplib/v34filt.h"	/* struct v34_echo: the FSK delay line is one  */
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
 * Four array bounds in the V.34 object that belong to the handshake's
 * interface rather than to the FSK receiver, declared here because this is
 * where `struct v34_object` is.  All are the object's own loop bounds, not
 * inferred sizes -- see the members.
 */
#define V34_PROBE_RESULTS	25	/* doubles at +0xa258 */
#define V34_PROBE_BINS		25	/* DFT bins at +0xa320 */
#define V34_NL_BINS		4	/* DFT bins at +0xa76c */
#define V34_INFO0_BITS		41	/* one bit per int at +0xa8a4 */
#define V34_RETRAIN_BINS	3	/* DFT bins at +0xa81c */

/*
 * The calling/answering role, `struct v34_object::role` at +0x359c.  Two
 * other files already carry the same two values under file-local names --
 * `v34diag.cpp`'s `VDIAG_ROLE_CALL`/`VDIAG_ROLE_ANSWER` and
 * `v34pcmmain.cpp`'s `PROG_ROLE_ORIGINATE` -- kept where they are rather
 * than consolidated; these are a third spelling for this header's own use.
 */
#define V34_ROLE_CALL		0x65
#define V34_ROLE_ANSWER		0x66

/*
 * `struct v34_object::tx_flags` at +0x25c2.  See the field's own comment for
 * what each bit does and for the two (8, 10) deliberately left unnamed.
 *
 * Three of the eight named bits already had names, from readers this tree
 * wrote before this field did -- bit 2 is `V34_EC_FROZEN`, bit 4 is
 * `v34pcmmain.cpp`'s own `PROG_TXBIT_DATA`, and bit 9 is `V34_EC_FEED`, all
 * in `v34rx.h` or beside their one reader.  Kept where they are rather than
 * duplicated or moved -- "one type, one home" is about the struct, not
 * about which header a flag constant sits beside its user in.
 */
#define V34_TXFLAG_CALLER	(1u << 0)
#define V34_TXFLAG_SEG4A	(1u << 13)
#define V34_TXFLAG_NLENCODE	(1u << 14)
#define V34_TXFLAG_PPSEG	(1u << 15)

/*
 * `struct v34_object::caps_flags` at +0xaa3e.  The field's own comment
 * (beside its declaration below) already names bit 0: "the other half of
 * the asymmetric permission" -- paired with `rate_mask`'s sign bit, both
 * required before the two directions may run at different rates.
 */
#define V34_CAPS_ASYMMETRIC	(1u << 0)

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

/**
 * @brief The FSK demodulator's state and configuration, 0x16 bytes, kept at
 * V.34 object offset 0xaad0.
 *
 * The first two fields are read by fskdetect(), and the rest only by
 * fskdemodulate(); they are one struct because one pointer is passed to
 * both -- fskdemodulate() hands its own third argument straight through.
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

/**
 * @brief A view onto the FSK discriminator's delay line.
 *
 * Not a distinct object: the pointer fskdetect() dereferences at V.34
 * object offset +0x80c4 is the first echo canceller's `coeff_frac` field,
 * so this struct is a view onto `struct v34_echo::coeff_frac` (finding
 * F100) -- the delay line occupies 49 of that array's 144 shorts. The two
 * uses are never live at once: the FSK receiver carries phase 2's INFO
 * messages, while the echo canceller's fractional coefficients only matter
 * in data mode.
 *
 * When `V34RX.c` defines the parent object, these must stay one region
 * with two readings; declaring them as two members would double-allocate
 * and break the aliasing the original depends on.
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
 * detector.c lives at +0x3564. Only the fields DPSK.c uses are named. The
 * pads are not a claim about what is in them, and the struct's size is not
 * a claim about the object's size; it is a lower bound.
 *
 * Every named offset is checked by a static assertion in DPSK.c, so the
 * padding cannot drift silently once the surrounding translation units
 * arrive and start filling it in.
 *
 * v34det.h declares `struct v34_rx`, the other partial map of this same
 * object, seen from +0x264 -- so its `flags` at +0x122 is this struct's
 * +0x386. Extend one of the two when the next V.34 file needs a field, and
 * say which; do not start a third.
 */

/**
 * @brief The V.34 scrambler's shift register.
 *
 * Both the scrambler and descrambler registers are 128 bits stepped
 * sixteen at a time, held as words rather than a bit array, with both
 * polynomials applied to a whole 16-bit step at once rather than bit by
 * bit. That is what makes scrambleGPA() look unlike scrambleGPC(): GPC's
 * shorter tap is 18 bits back, further than one step, so one pass
 * suffices; GPA's is 5 bits back, closer than one step, so its feedback
 * has to be folded in four times to cover sixteen.
 */
struct v34_scrambler {
	unsigned w[4];		/* +0x00 */
	short nbits;		/* +0x10  0x20 out of preinitdigital */
};

/** @brief The V.34 descrambler's shift register (see struct v34_scrambler). */
struct v34_descrambler {
	unsigned w[3];		/* +0x00 */
	short count;		/* +0x0c  bits held, flushed past 31 */
};

struct v34_object {
	/*
	 * The datapump's own status word: `receiver` puts 10 in it on loss
	 * of signal, alongside the string "Signal Energy below Threshold
	 * %d, initiate a disconnection". What the other values mean belongs
	 * to VPcmV34Main.cpp, which is not reconstructed, except that 1 and
	 * 2 mean a PCM receiver is running: `VPcmV34InitiateHangUp` and
	 * `VPcmV34InitiateRateRenegotiation` both test
	 * `(unsigned)(status - 1) <= 1` and, when it holds, hand the request
	 * to the V.90 side through `p3548` instead of to the V.34 handshake
	 * -- and `VPcmV34GetCurrentTxBitRate` takes the `V90Demodulator`
	 * branch on exactly the same test.
	 */
	int status;					/* +0x0000 */
	/*
	 * An int the shell polls: `VPcmV34InitiateHangUp`,
	 * `VPcmV34InitiateRateRenegotiation` and `VPcmV34SetV90RateReneg` all
	 * put 6 in it, `datapumpv34` puts 5 when the sample count at +0x238
	 * has run 287,488 past the mark at +0x248, and `VPcmV34Create` puts
	 * 10 and then 0. Nothing reconstructed reads it, so the values above
	 * are all this says.
	 */
	int progress;					/* +0x0004 */
	/*
	 * +0x0008 and +0x0010, both named by initdigital's own debug string:
	 * "for tx data rate - %d, PTC - %d, setting nofTxBits to %d".  `ptc`
	 * is read and never written here; `nof_tx_bits` is
	 * `((txbits * ptc) >> 6) + 6`, or zero when the rate is zero.
	 */
	int ptc;					/* +0x0008 */
	unsigned char unmapped_000c[0x010 - 0x00c];
	int nof_tx_bits;				/* +0x0010 */
	/*
	 * The datapump's two data buffers. The layout tiles exactly --
	 * 0x14 + 64*4 is precisely 0x114 where the receive count lives, and
	 * 0x118 + 64*4 precisely 0x218 where the transmit one does, so each
	 * array ends where its counter begins.
	 *
	 * descrambleGP* appends each sixteen recovered bits to `rx_data` and
	 * stops at 64 entries; scrambleGP* takes its next sixteen out of
	 * `tx_data` and, once `tx_rd` reaches `tx_n`, scrambles 0xffff
	 * instead -- the all-ones idle V.34 sends with nothing to carry.
	 * Both paths are gated on `data_enable` at +0x2214. The arrays are
	 * `int`, and only the low short of each transmit entry is read.
	 */
	int rx_data[64];				/* +0x0014 */
	int rx_n;					/* +0x0114 */
	int tx_data[64];				/* +0x0118 */
	int tx_n;					/* +0x0218 */
	int tx_rd;					/* +0x021c */
	/*
	 * The rate renegotiation's four words, all indices rather than bit
	 * rates -- `VPcmV34GetCurrentRxBitRate` multiplies the same units by
	 * 2400 to answer in bits per second. Each name is read off a site,
	 * not inferred from the group:
	 *
	 *   min, max   `VPcmV34SetMinMaxBitRates` divides a pair of rates by
	 *              2400 into these two, caps both at 14, and then raises
	 *              `rate_max` to `rate_min` if it is below it. Which way
	 *              round that fix-up goes is settled by
	 *              `VPcmV34InitiateRateRenegotiation`, whose step down
	 *              clamps at `rate_min` and whose step up at `rate_max`.
	 *
	 *   now        `v34handshak` stores the negotiated rate index -- a
	 *              copy of +0xaa98, the field the getter multiplies --
	 *              into `rate_now` and `rate_want` together.
	 *
	 *   want       and elsewhere reads `rate_want` back, skips on a
	 *              negative value and then on equality with `rate_now`,
	 *              and otherwise adopts it as the field at +0xaa98. So
	 *              this is a request, and negative means "no target":
	 *              `VPcmV34InitiateRateRenegotiation` writes -1 for a
	 *              renegotiation that names no rate, and
	 *              `VPcmV34InitiateHangUp` clears the request and both
	 *              bounds but leaves `rate_now` alone.
	 */
	int rate_min;					/* +0x0220 */
	int rate_max;					/* +0x0224 */
	int rate_now;					/* +0x0228 */
	int rate_want;					/* +0x022c */
	/*
	 * The signal-energy floor `receiver` compares its 36-sample RMS
	 * against before declaring the line dead.  An int, and the only field
	 * either V.34 core reads through `obj + 4` rather than `obj`.
	 */
	int rx_energy_floor;				/* +0x0230 */
	/*
	 * +0x0234 to +0x024b holds the sample-clock timer -- `sample_count`
	 * the running count and `timeout_deadline` the instant a span is
	 * measured from, with +0x234 and +0x244 written alongside them but
	 * left unnamed: every writer reaches the group through `obj + 4` and
	 * spells the offsets out (finding F179), and only these two of the
	 * four have a reader to name them from.
	 *
	 * `datapumpv34` reads both and puts 5 in `progress` when their
	 * difference passes 287,488; `v34handshak` copies `sample_count`
	 * into `timeout_deadline` to restart the span. `VPcmV34SetTimeOut`
	 * is the only function that writes the pair together and the only
	 * one that establishes what the second is for: it stores
	 * `seconds * 9600` into `timeout_deadline` after zeroing
	 * `sample_count`, so `timeout_deadline` is a deadline in the same
	 * units `sample_count` runs in. 9,600 is 8 kHz times 1.2 and not the
	 * sample rate, so the argument is not seconds of wall clock at the
	 * codec rate, and the scale is left as the constant it is rather
	 * than named a unit this cannot prove.
	 */
	unsigned char unmapped_0234[0x238 - 0x234];
	int sample_count;				/* +0x0238 */
	int timeout_deadline;				/* +0x023c */
	unsigned char unmapped_0240[0x24c - 0x240];
	/*
	 * How far the V.90 receiver has got through phase 3, as a number the
	 * handshake's C++ side ratchets forward. `V34XF_IndicateTrn2dReceived`'s
	 * own debug string names it -- "IndicateTrn2dReceived called,
	 * v90Receiver = %d" -- and the Indicate entry points between them
	 * set 3 (Jd), 6 (DIL) and 10/14/18/20 (TRN2d). Not only ratcheted:
	 * `VPcmV34SetV90RateReneg` assigns 11 or 15 outright, so a
	 * renegotiation can move it down as well as up (deviation D48).
	 */
	int v90_receiver;				/* +0x024c */
	/*
	 * The same for K56Flex: `V34XF_IndicateK56FlexRateDetermined` sets 5
	 * and nothing reconstructed yet reads it. `V34GiveProbeResults`
	 * treats the pair as one "is a PCM receiver running" test.
	 */
	int k56flex_receiver;				/* +0x0250 */
	unsigned char unmapped_0254[0x25c - 0x254];
	/*
	 * adaptecho's three scalars, immediately before the receiver.
	 * `dmadelay` is the base the echo filter's lag is measured from,
	 * `tx_sample` the transmit sample it just dequeued, and
	 * `echo_residual` the running residual.
	 *
	 * `dmadelay` is `V34dmadelay`, the object's own spelling:
	 * `VPcmV34SetDelays` computes `0x610 - cfg[0x68]`, stores it here and
	 * reports it as "V34FEC, V34dmadelay set to %d, (ext delay=%d)" with
	 * the stored value first and the configured one second.
	 */
	short dmadelay;					/* +0x25c */
	short tx_sample;					/* +0x25e */
	short echo_residual;					/* +0x260 */
	/*
	 * +0x262.  `VPcmV34NotifyDP` sets it to 1 under "VPcmV34
	 * Notification: Valid in samples..." and to 0 under "...Invalid in
	 * samples...", and `datapumpv34` returns early when it is zero.  So
	 * the incoming sample stream is usable exactly while this is set.
	 * Two bytes, written as a short by both arms.
	 */
	short samples_valid;				/* +0x262 */
	struct v34_queue rxq;				/* +0x264 */
	int rxq_ring_tail[V34_RXQ_RING - 1];		/* to +0x370 */
	unsigned char unmapped_0370[0x382 - 0x370];
	/*
	 * +0x0382. Written six times and read nowhere in this object -- the
	 * only stores are VPcmV34Main.cpp's, four of them in the two Indicate
	 * entry points reconstructed here. So whatever consumes it lives in
	 * the C++ half that is still to come, and the values are all this
	 * says about it: 0 when a Jd arrives with the silence-scrambler flag
	 * set, otherwise 0x89b0 or 0x8990 according to a constellation-size
	 * flag. The two differ by 32, which is the only structure visible.
	 *
	 * As a `struct v34_receiver` offset this is +0x11e, immediately below
	 * that struct's `vectpp_idx`/`flags` pair; named here rather than
	 * there because every caller has the whole object in hand.
	 */
	short short_382;					/* +0x0382 */
	unsigned char unmapped_0384[0x402 - 0x384];
	/*
	 * Non-zero makes fskdemodulate return without doing anything -- not
	 * even running the detector -- so it reads as "the FSK receiver is
	 * switched off".  Nothing here sets it.
	 */
	short fsk_inhibit;				/* +0x402 */
	unsigned char unmapped_0404[0x49c - 0x404];
	/*
	 * +0x049c and +0x04a0, named from the object's own exported
	 * accessors `getTimingOffset` and `getTimingPhase` -- the same class
	 * of evidence as a format string. What the fields mean beyond that
	 * is not claimed: the two accessors are the only readers anywhere in
	 * the object (exported API with no internal caller) and no writer
	 * has been traced to them.
	 */
	int timing_offset;				/* +0x049c */
	int timing_phase;				/* +0x04a0 */
	unsigned char unmapped_04a4[0xe74 - 0x4a4];
	/* The descrambler's shift register; see `struct v34_descrambler`. */
	struct v34_descrambler descrambler;		/* +0x0e74 */
	unsigned char unmapped_0e84[0x2074 - 0xe84];
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
	unsigned char unmapped_220c[0x2214 - 0x220c];
	/*
	 * Non-zero connects the scrambler callbacks to `tx_data` and
	 * `rx_data`; zero leaves the transmitter scrambling idle ones and
	 * the receiver throwing away what it recovers. `preinitdigital`
	 * clears it and `modulatevector` sets it when the training-to-data
	 * symbol counter expires -- the object's only two writers -- so this
	 * is the data path being switched on at the end of training, not a
	 * facility something outside the core operates.
	 */
	short data_enable;				/* +0x2214 */
	unsigned char unmapped_2216[0x2218 - 0x2216];
	/*
	 * `V34hshak.c`'s own `DP_MODE`/`T3C_MODE`: the recovery supervisor in
	 * `datapumpv34` reads it as "handshake above 1", and `v90p34`'s
	 * table-2 tail is the same int's other reader.
	 */
	int hs_mode;					/* +0x2218 */
	struct v34_queue txq;				/* +0x221c */
	int txq_ring_tail[V34_TXQ_RING - 1];		/* to +0x25c0 */
	short seg_symcount;					/* +0x25c0 */
	/*
	 * The transmit-side flag word `v34hstx1.cpp` and `v34shell.c` both
	 * test directly, bit by bit, rather than through a name for the
	 * whole field -- eight bits with an established meaning and two with
	 * none yet:
	 *
	 *   bit 0   V34_TXFLAG_CALLER       set for the calling station's
	 *           scrambler generator, clear for the answering one --
	 *           `txmitdibit`/`txmitquadbit` pin this one.
	 *   bit 2   V34_EC_FROZEN (v34rx.h) both echo cancellers frozen; the
	 *           echo-adapt start/stop reports clear and set it.
	 *   bit 4   PROG_TXBIT_DATA (v34pcmmain.cpp) set by `modulatevector`
	 *           once the training-to-data symbol count (`train_symcount`
	 *           against `span`) expires; gates the shell mapper's
	 *           non-idle input.
	 *   bit 8   set once the segment counter passes a threshold during echo
	 *           adaptation (`v34hstx1.cpp`); no established name.
	 *   bit 9   V34_EC_FEED (v34rx.h) gates the echo feed.
	 *   bit 10  set on one baud-ratio branch of the echo-adapt ladder,
	 *           alongside clearing bit 15; no established name.
	 *   bit 13  V34_TXFLAG_SEG4A        raised entering TRNSEG4A/SSEG.
	 *   bit 14  V34_TXFLAG_NLENCODE     `initdigital`'s own comment names
	 *           it: "Bit 13 of the same word is modulatevector's
	 *           non-linear encoder" (bit 13 of the INFO word, stored here
	 *           at bit 14); selects `V34nlencoder` over a plain point copy.
	 *   bit 15  V34_TXFLAG_PPSEG        raised for PPSEG, read as the
	 *           field's sign.
	 *
	 * Bits 8 and 10 are usage inference only -- a single write site each,
	 * no format string, no reader that types them -- and are left as bare
	 * hex rather than guessed into a name.
	 */
	short tx_flags;					/* +0x25c2 */
	unsigned char unmapped_25c4[0x25c6 - 0x25c4];
	/*
	 * The differentially-encoded quadrant, carried from one symbol to the
	 * next: prev_quadrant is the previous one and cur_quadrant the current.  In the
	 * dibit case the two end up equal; in the quadbit case cur_quadrant is set
	 * first and prev_quadrant only catches up at the end, because the second
	 * dibit indexes off the first one's quadrant.
	 */
	short prev_quadrant;					/* +0x25c6 */
	short cur_quadrant;					/* +0x25c8 */
	unsigned char unmapped_25ca[0x25cc - 0x25ca];
	/* The transmit scrambler's shift register. */
	int tx_scr_sr;					/* +0x25cc */
	/*
	 * The point being transmitted: two shorts, real then imaginary.
	 * Every arm that sends a constellation point writes both halves with
	 * one 32-bit store (txmitdibit(), txmitquadbit(), modulatevector()),
	 * which is why `vect4` and `vect16` hold ints at all -- but the
	 * halves are also written and read separately as shorts: the silent
	 * symbol zeroes each on its own, and `txmit` reassembles them as
	 * `(im << 16) | (unsigned short)re` before handing the result to
	 * `V34ModulatorProcess`. That reader is the only thing in the object
	 * that reads this pair, which is what settles it as the point going
	 * out rather than one coming in.
	 *
	 * The union spells the aliasing out rather than casting a pointer,
	 * which -O2 is entitled to reorder (the same remedy as
	 * `v34_receiver::energy`). `c` is an array rather than two named
	 * fields because `V34nlencoder` is handed `txpoint.c` and writes
	 * `out[0]` and `out[1]` through it -- two named shorts would make
	 * that second store its own out-of-bounds defect.
	 *
	 * A 4-byte struct assignment would emit the same single `movl` and
	 * is an equally consistent reading of the original; nothing in the
	 * object separates the two, so this does not claim to have settled
	 * which the author wrote (finding F5303).
	 */
	union {
		int word;				/* +0x25d0 both at once */
		short c[2];				/* [0] real, [1] imag  */
	} txpoint;
	short tx_scale;					/* +0x25d4 tx scale  */
	unsigned char unmapped_25d6[0x25dc - 0x25d6];
	/*
	 * The transmit power reduction in whole dB, which `settxlevel`
	 * assembles from the far end's MP message and then applies to
	 * `tx_scale` above -- so this is the request and that is the result.
	 * Its own diagnostic names it: "power reduction requested by remote
	 * modem is %d dB". It can be negative: with a V.90 receiver running,
	 * a `GetVPcmMinimalTxPowerReduction` below zero is added to the
	 * request rather than compared with it, and the result drives
	 * `settxlevel`'s other loop -- the one that raises the scale.
	 */
	short tx_pwr_reduction;					/* +0x25dc */
	unsigned char unmapped_25de[0x2a54 - 0x25de];
	/* The scrambler's shift register; see `struct v34_scrambler`. */
	struct v34_scrambler scrambler;			/* +0x2a54 */
	unsigned char unmapped_2a68[0x2a80 - 0x2a68];
	/*
	 * `modulatevector`'s output: eight complex points as sixteen shorts,
	 * and the cursor into them. One call emits point `vect_idx` and
	 * bumps it; only when it reaches 8 does the mapping run and refill
	 * all eight. `scaleVector`'s sixteen shorts are exactly this array.
	 *
	 * In transmit-shell coordinates these land inside `sub[]` and
	 * `cost[]` -- not a contradiction: only the receive context decodes,
	 * so the transmit one has no sub-indices, costs, trellis or states,
	 * and the object reuses the space (finding F181).
	 *
	 * The points are also addressed as eight ints: `modulatevector`
	 * writes single shorts into this array and separately loads one
	 * whole point at a time (index scaled by four) to hand to the
	 * 32-bit store at `txpoint` above. Sixteen shorts and eight ints
	 * over the same 32 bytes are spelled as a union rather than reached
	 * with `*(int *)&vect[2 * n]`, which is what -O2 is entitled to
	 * reorder. `vect` keeps its name, so every short-wise user is
	 * unchanged; `vectp` is the same storage addressed as points.
	 */
	union {
		short vect[16];				/* +0x2a80 */
		int vectp[8];				/* +0x2a80, one per point */
	};
	/*
	 * +0x2aa0. `VPcmV34Progress`'s per-call requested sample count
	 * (`nin & ~3`) stashed here at entry (`v34pcmmain.cpp`'s V.90/K56flex
	 * arms, `obj->tx_fill_target = (short)n`), then read back nowhere but
	 * in `while (obj->txq.count < obj->tx_fill_target)` -- the TX-queue
	 * top-up loop three functions share (`VPcmV34Progress` itself,
	 * `datapumpv34` and the handshake's `v34tx1_*` dispatch in
	 * `v34hstx1.cpp`, per finding F714's own read of the five `movzwl
	 * 0x2aa0(reg)` sites). Usage inference: it is never printed and no
	 * typed callee takes it, but every one of its five readers and both
	 * of its writers agree on this one role.
	 */
	short tx_fill_target;					/* +0x2aa0 */
	short vect_idx;					/* +0x2aa2 */
	short hist1_idx;					/* +0x2aa4 */
	short hist2_idx;					/* +0x2aa6 */
	/*
	 * Two per-symbol history rings modem_serrint fills, indexed by
	 * `hist1_idx` and `hist2_idx` and wrapping at 0x12b and 0x257
	 * respectively. The first holds each residual twice, as both halves
	 * of its entry -- so it is a complex buffer being written with a
	 * real value. Its element is two shorts and not one int: the entry
	 * is filled by two separate 16-bit stores where a 32-bit field would
	 * have taken one `movl`.
	 */
	short hist_2aa8[0x12c][2];			/* +0x2aa8 */
	short hist_2f58[0x258];				/* +0x2f58 */
	unsigned char unmapped_3408[0x3548 - 0x3408];
	/*
	 * +0x3548.  The session object VPcmV34Main.cpp hangs everything else
	 * off: twenty-odd functions in that translation unit load it.  What
	 * it points at is not reconstructed, so it is a `void *` here and
	 * each field is spelled out at its use rather than given a struct
	 * that would be a guess.
	 *
	 * An earlier note here said +0x6120 was the only field any of them
	 * read.  That was true of the functions reconstructed at the time and
	 * is not true of the object: +0x610c, +0x611c, +0x612c, +0x6bd0,
	 * +0x1744 and +0x1760 are all read as well, and the two Initiate
	 * entry points reach a chain -- a flag byte at +0x173e, and a pointer
	 * at +0x175c that `VPcmV34GetCurrentRxBitRate` hands to
	 * `V90Demodulator::getBitRate`, so the session object holds the
	 * demodulator rather than being it.
	 */
	void *p3548;					/* +0x3548 */
	/*
	 * adaptecho's adaptation state. `echo_calls` counts calls and gates
	 * the whole slow path; `echo_alpha` is the near-echo LMS step
	 * (updateAlpha's alpha -- an EARLIER note here called it "the only
	 * short here", which stopped being true the moment `far_echo_alpha`
	 * below is read as its far-echo counterpart rather than left as
	 * `short_3552`); `echo_decay_fact` its decay; `echo_beta` a shift the
	 * step is scaled by, moved between 2, 4 and 5 by adaptecho's own
	 * ladder; and `echo_energy` the energy accumulated over the first
	 * 0x8f calls.
	 *
	 * `echo_decay_start`, `echo_decay_fact` and `echo_beta` are also named
	 * by their other writer: `GetVPcmMinimalTxPowerReduction` sets all
	 * three together and prints what it set ("setting echo: decay start
	 * = %d, decay fact = %d, beta = %d"). So `echo_decay_start` is the
	 * call count decay starts at -- exactly what `adaptecho` compares it
	 * against. The two writers do not agree on beta's range (adaptecho's
	 * ladder moves it between 2, 4 and 5; the PCM side sets 2, 4 or 6),
	 * and both readings are the object's -- nothing here reconciles them.
	 */
	int echo_calls;					/* +0x354c */
	short echo_alpha;					/* +0x3550 */
	/*
	 * +0x3552. `far_echo_alpha` and not `short_3552`: `V34RX.c`'s own
	 * paired `updateAlpha` calls tag each field with the object's own
	 * debug string (`updateAlpha`'s `tag` argument, printed verbatim as
	 * "updateAlpha%s: updated %d => %d") -- `echo_alpha` gets `"NE"`
	 * (near echo) at both its call sites, this field gets `"FE"` (far
	 * echo) at its one call site, gated the same way `echo1`/
	 * `far_echo_enable` gate every other far-echo-only quantity in this
	 * struct. CLAUDE.md's evidence tier 1: the object's own words, via a
	 * format string it is passed to and prints from.
	 */
	short far_echo_alpha;					/* +0x3552 */
	int echo_decay_start;					/* +0x3554 */
	int echo_decay_fact;					/* +0x3558 */
	int echo_beta;					/* +0x355c */
	int echo_energy;					/* +0x3560 */
	/*
	 * +0x3564 is the object's own `struct v34_detector`, and the two
	 * ends meet exactly: `sizeof(struct v34_detector)` is 0x24 and
	 * 0x3564 + 0x24 is 0x3588, where the next field measured below
	 * begins. V34hshak.c reaches it there as `T3C_DETECTOR`/`T3M_DETECTOR`
	 * and casts, and those arms are differentially tested.
	 *
	 * It is not embedded here, deliberately: two things meeting is
	 * adjacency, not a bound (finding F215), and the 0x24 is our
	 * declaration's size rather than anything the object states. So the
	 * tiling is recorded as the measurement it is and the span stays a
	 * pad (finding F630).
	 */
	unsigned char unmapped_3564[0x3588 - 0x3564];
	/*
	 * +0x3588 and +0x358a. Two 16-bit fields on 68 accesses -- 42 at
	 * +0x3588 and 26 at +0x358a -- every one of them a halfword load, a
	 * halfword store or a `cmpw`. Nothing reads either at a byte width
	 * and nothing reads an odd displacement inside them.
	 *
	 * Two sites read the pair 32 bits wide, `cmpl $0x20002` against the
	 * pair -- the two halves against 2 and 2 in one instruction. That is
	 * the original source's spelling and not its compiler's: the period
	 * compiler compiles `s->a == 2 && s->b == 2` on two adjacent shorts
	 * into two separate `cmpw` and never fuses them (measured, finding
	 * F631). V34hshak.c reads exactly those two sites through `T3M_I32`
	 * and every other site through `T3M_I16`, which is why both
	 * readings survive -- this span is finding F553's shape (one
	 * region, two widths, both the object's), and the offset spelling
	 * stays at every use site. The declaration here is the 16-bit
	 * reading because that is what 68 of the 70 accesses say.
	 *
	 * Signedness is not settled for either: no access sign-extends, and
	 * a `movzwl` whose upper half is discarded is a free choice for the
	 * compiler (finding F614), so `short` here matches the rest of this
	 * struct and is not a measurement.
	 *
	 * `fNNNN` and not a description: what they are for was not measured.
	 * V34hshak.c knows them as `T3M_F3588` and `T3M_F358A` and records
	 * what each use does.
	 */
	short short_3588;					/* +0x3588 */
	short short_358a;					/* +0x358a */
	/*
	 * +0x358c. Signed short, and the sign is forced: one site loads it
	 * `movswl`, masks bit 0 and indexes a table with scale 8, and two
	 * others shift the sign-extended result left by 6 and by 10 into a
	 * word being assembled -- a 32-bit result that is used, CLAUDE.md's
	 * own case for acting on the extension. Thirty accesses, all
	 * sixteen bits wide. `v34handshak`'s microstate 48 inverts bit 0 of
	 * it -- V34hshak.c's `T3M_TOGGLE` and `T3C_F358C`.
	 */
	short short_358c;					/* +0x358c */
	unsigned char unmapped_358e[0x3592 - 0x358e];
	/*
	 * The three state words. `v34handshak` is not one state machine but
	 * three concurrent ones, and these are their state variables --
	 * `v34handshakinit`'s format strings settle which offset is which
	 * (finding F171); v34hshak.h holds the eighty-seven state names they
	 * take and the three dispatch tables they drive (finding F213).
	 * 728 accesses between them, every one sixteen bits wide, and nine
	 * functions besides `v34handshak` reach them.
	 *
	 * Signed, and forced: `txstate` and `microstate` are both loaded
	 * `movswl` to index `StateName` (the table of eighty-seven state
	 * name strings) with the sign-extended result -- a load whose
	 * 32-bit result indexes a table, CLAUDE.md's own case for acting on
	 * the extension. The microstate dispatch agrees despite one site
	 * loading it `movzwl`: that load is immediately re-extended with a
	 * separate `movswl` before it indexes the microstate jump table, so
	 * both of the object's table indexes are in fact sign-extended
	 * values. The remaining zero-extending loads feed 16-bit compares
	 * and stores, where the extension is the compiler's free choice
	 * (finding F614).
	 *
	 * The use sites keep the offset spelling deliberately, and this is
	 * not a half-done rename: `hs_get`, `hs_put` and `hs_setstate` take
	 * the offset as a runtime argument, because one function serving all
	 * three machines is the whole point of them -- the two context
	 * arguments the three format strings take are in an order that
	 * differs per string, and a second copy per machine is a second
	 * place to get that order wrong. So v34hshak.h's
	 * `V34HS_MICROSTATE_OFF`, `_RXSTATE_OFF` and `_TXSTATE_OFF` remain
	 * what every caller passes, checked against these three fields at
	 * compile time (finding F632).
	 */
	short microstate;				/* +0x3592 */
	short rxstate;					/* +0x3594 */
	short txstate;					/* +0x3596 */
	unsigned char unmapped_3598[0x359c - 0x3598];
	/*
	 * The calling/answering role. `V34hshak.c` reads it into a variable
	 * it names `originate` (`originate = (obj->role == 0x65)`), and
	 * forty-odd call sites elsewhere compare it against 0x65/0x66
	 * directly to choose everything from the scrambler polynomial to
	 * which of a pair of debug constants to print -- including a timing
	 * variant: 0x65 selects `setTimingStateParameters`' second parameter
	 * table, differing from the other only in the fast part of the
	 * acquisition ramp (states 5-7), so this is a per-role tuning rather
	 * than a second, unrelated flag. `v34diag.cpp` carries the same two
	 * values under its own names, `VDIAG_ROLE_CALL`/`VDIAG_ROLE_ANSWER`
	 * -- reused here as `V34_ROLE_CALL`/`V34_ROLE_ANSWER` rather than a
	 * third pair of names for the same two constants.
	 */
	short role;					/* +0x359c */
	unsigned char unmapped_359e[0x35a4 - 0x359e];
	/*
	 * A short `VPcmV34Create` clears and three functions read, always
	 * with `movswl`, so it is signed. `V34SetINFO1aBits` sends the low
	 * seven bits of it, bit-reversed, as the leading field of an INFO1a;
	 * `VPcmV34InitiateRetrain` and `V34XF_IndicateK56FlexJdReceived` both
	 * compute `10000 + 336 * n` from it and store the result elsewhere.
	 *
	 * Offset-named on purpose: that arithmetic looks like a table index
	 * turning into a rate or a frequency, and it is deliberately not
	 * written down as one, since neither of the two functions that does
	 * it is reconstructed, they write through different base registers,
	 * and nothing establishes that their destinations are the same field.
	 */
	short short_35a4;					/* +0x35a4 */
	/*
	 * `v34hstx1.cpp`'s own `TX1_SEGLEN`: txstate 86 compares `vect_idx`
	 * against it to decide when the modulator is reconfigured, and sets
	 * it from `short_35a4 * 0x53` first. Nothing else in the tree reads it.
	 */
	short seg_len;					/* +0x35a6 */
	/*
	 * The bulk-delay ring feeding the second echo canceller. Its wrap is
	 * branchless -- `idx &= -(len > idx)`, resetting to zero rather than
	 * subtracting -- unlike every other ring here (finding F116).
	 */
	int bulk_head;					/* +0x35a8 */
	int bulk_tail;					/* +0x35ac */
	short *bulk_ring;				/* +0x35b0 */
	int bulk_len;					/* +0x35b4 */
	unsigned char unmapped_35b8[0x80b8 - 0x35b8];
	/*
	 * The two echo cancellers and the arrays they point at, one
	 * contiguous block each (finding F98). Declared here rather than in
	 * v34filt.h because this is the struct that owns the storage; the
	 * descriptors are `struct v34_echo` and the arrays are named
	 * separately because the object's own pointers are what tie them
	 * together, not adjacency.
	 *
	 * `echo0.coeff_frac` points at `echo0_frac`, and that array is also
	 * DPSK.c's FSK delay line -- see finding F100 and the note on
	 * `struct v34_fskdelay` above. One region, two readings; there is
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
	unsigned char unmapped_a238[0xa23c - 0xa238];
	/*
	 * Non-zero switches the FAR echo canceller on: modem_serrint filters
	 * and adapts echo1 only when this is set, and adaptecho never looks
	 * at it because it only ever drives the near one.
	 */
	short far_echo_enable;					/* +0xa23c */
	/*
	 * adaptecho reads this, adds it to the residual, and clears it -- so
	 * it is a one-shot correction somebody upstream deposits.  Whoever
	 * writes it has not been reconstructed yet.
	 */
	short echo_correction;					/* +0xa23e */
	/* A leaky estimate of the residual's energy, updated per symbol. */
	short echo_resid_energy;					/* +0xa240 */
	unsigned char unmapped_a242[0xa24a - 0xa242];
	/*
	 * The retrain-request detector's five scalars, all five written by
	 * `dftRetrainDetInit` and all five read by `detectRetrainReq`, which
	 * are the only two functions in the object that touch any of them.
	 *
	 * It is a two-state machine over the bank at `retrain_bins`: state 1
	 * waits for every bin to fall below its `thresh_lo` for
	 * `retrain_quiet_runs` consecutive measurements, state 2 then waits
	 * for the middle bin to rise above its `thresh_hi` for
	 * `retrain_tone_runs` of them, and reaching that is the "retrain
	 * requested" answer.  `retrain_runs` is the run length in whichever
	 * state is current, so the two limits share one counter.
	 */
	short retrain_state;				/* +0xa24a */
	/*
	 * Samples since the last measurement, advanced four at a time and
	 * compared for equality with 128 -- not `>=`.  The caller therefore
	 * has to arrive in multiples of four or the detector never fires,
	 * which is a fact about the call site rather than a defect: the
	 * receive queue is drained four samples at a time everywhere.
	 */
	int retrain_phase;				/* +0xa24c */
	short retrain_runs;				/* +0xa250 */
	short retrain_quiet_runs;			/* +0xa252 */
	short retrain_tone_runs;			/* +0xa254 */
	unsigned char unmapped_a256[0xa258 - 0xa256];
	/*
	 * +0xa258.  Twenty-five doubles -- the only floating point anywhere
	 * in this struct -- that `V34GiveProbeResults` copies in from a
	 * 44-byte-stride record the C++ side owns.  The count is the object's
	 * own: the loop's bound is `cmp $0x18,%ax`, so 0..24 inclusive.
	 * `V34XF_GetProbeResultsPtr` hands the array out unchanged.
	 */
	double probe_results[V34_PROBE_RESULTS];	/* +0xa258 */
	/*
	 * The line probe's own DFT bank, twenty-five bins to match the
	 * twenty-five doubles above, and the second bank in this object
	 * after the retrain detector's three.
	 *
	 * The count is not inferred from the array above it: `v34handshak`
	 * passes it to `dftupdate` and `dftenergy` with `nbins` set to 25.
	 * Twenty-five bins one unit apart is 150 Hz to 3750 Hz at the
	 * 9600 Hz rate V.34 runs at, which is the probe's tone spacing.
	 *
	 * `probeselect` reads `energy` and `shift` out of every one of them
	 * and `chkForceBaudRate` writes `shift` on six, so the two fields
	 * `dftenergy` produces are the whole interface -- nothing outside
	 * DFTC.c touches the accumulators.
	 */
	struct v34_dftbin probe_bins[V34_PROBE_BINS];	/* +0xa320 */
	/*
	 * The noise half of the nonlinear-distortion measurement, four bins
	 * at 900, 1200, 1800 and 2400 Hz. Named from its caller, rxstate
	 * 72's arm and nothing else: that arm runs `dftnlinitSignalBins` on
	 * `probe_bins` and `dftnlinitNoiseBins` on this field, so the signal
	 * bank overlays `probe_bins[0..3]` and this noise bank is the four
	 * bins immediately after the probe's twenty-five -- the offsets tile
	 * exactly (findings F738-745). The same arm reads both banks'
	 * `energy` across, which is what makes `probe_bins`' the numerator
	 * of the ratio and this the denominator.
	 */
	struct v34_dftbin nl_noise_bins[V34_NL_BINS];	/* +0xa76c */
	/*
	 * +0xa81c.  The retrain detector's three DFT bins, at 900, 1200 and
	 * 1500 Hz -- `dftRetrainDetInit` gives them phase steps of 0x600,
	 * 0x800 and 0xa00, and one bin unit is 150 Hz at the 9600 Hz rate
	 * V.34 runs at (docs/rate_assumptions.md R-1).
	 */
	struct v34_dftbin retrain_bins[V34_RETRAIN_BINS];/* +0xa81c */
	unsigned char unmapped_a8a0[0xa8a4 - 0xa8a0];
	/*
	 * The received INFO0 message, one bit per int. `V34GiveINFO0dBits`
	 * unpacks it there MSB-first and every later reader indexes it as a
	 * bit vector; `V34XF_GetInfo0BitsPtr` hands out its address. 41 is
	 * what that unpacking writes -- 12 constant, then 8+8+8 from three
	 * message bytes and 5 from the fourth.
	 */
	int info0_bits[V34_INFO0_BITS];			/* +0xa8a4 */
	unsigned char unmapped_a948[0xaa0c - 0xa948];
	/*
	 * The negotiated INFO bits, which initdigital unpacks into the rate
	 * config at +0xaa84.
	 *
	 * `info_rates` carries two four-bit rate fields -- bits 2..5 and
	 * 6..9, one per direction, and which is "ours" depends on the role --
	 * plus the trellis depth at 11..12, a flag at 14, and the non-linear
	 * encoder select at 13.  `rate_mask` is a bitmap of the rates that
	 * are actually available, bit n-1 for rate n, and its sign bit means
	 * asymmetric rates are on the table.  `info_caps` holds two more
	 * nibbles, at 6 and 10, which are bit-reversed before use.
	 * `caps_flags` bit 0 is the other half of the asymmetric permission.
	 */
	short info_rates;				/* +0xaa0c */
	short rate_mask;				/* +0xaa0e */
	unsigned char unmapped_aa10[0xaa3c - 0xaa10];
	short info_caps;				/* +0xaa3c */
	short caps_flags;				/* +0xaa3e */
	unsigned char unmapped_aa40[0xaa6c - 0xaa40];
	/*
	 * Two pointers, and pointers rather than ints: fifty-eight accesses
	 * between them, every one a 32-bit `mov`, with stores putting an
	 * address inside this object in them and loads dereferencing them.
	 * `getMPrecvdBits` stores `obj + 0xaa3c` into `paa6c`;
	 * `v34handshakinit` aims `paa6c` at `obj + 0xa94c` and `paa70` at
	 * `obj + 0xa97c`; `v34handshak` moves `paa6c` on to `obj + 0xa9ac`
	 * and reads the record back through it -- one site loads `paa6c`
	 * and immediately reads `nbits`, `pos` and `avail` of `struct
	 * v34_bitsource` off it.
	 *
	 * `void *` and not `struct v34_bitsource *`, the same under-claim
	 * `p3548` sets the precedent for: the thirty-six sites that hand
	 * `paa6c` to `getbit` are strong evidence for that type, but the
	 * region it is aimed at has two readings this tree holds equally --
	 * the five message records on a 0x30 stride, and
	 * `info_caps`/`caps_flags` read straight out of the same words (see
	 * the note on `struct v34_bitsource` in v34hshak.h) -- and
	 * V34hshak.c's use sites cast to three different pointer types at
	 * different arms. A pointer type here would pick a winner the
	 * object does not (finding F634).
	 */
	void *paa6c;					/* +0xaa6c */
	void *paa70;					/* +0xaa70 */
	/* Cleared by preinitdigital; nothing reconstructed reads it. */
	int train_symcount;					/* +0xaa74 */
	/*
	 * +0xaa78. Signed short, and both halves of that are forced.
	 *
	 * 243 accesses, all sixteen bits wide -- the busiest field in the
	 * handshake after the three state words, and the one six of
	 * `v34handshak`'s microstate arms bump. It is the `[2]` every state
	 * transition prints (V34hshak.c's `HS_TRACE_2`), which is what
	 * settles the sign: it is loaded `movswl` straight into a debug
	 * printf argument slot, and the varargs promotion of an `unsigned
	 * short` would have been `movzwl` instead. The `movzwl` loads found
	 * elsewhere are the increment-and-compare sites, where the upper
	 * half never survives.
	 *
	 * `short_aa78` and not `counter`: what it counts differs per arm --
	 * ticks in one, symbols in another -- and a name that says "counter"
	 * would read as measured when only the width and the sign are.
	 * V34hshak.c knows it as `T3M_COUNTER`/`T3C_COUNT` (finding F633).
	 */
	short short_aa78;					/* +0xaa78 */
	/*
	 * +0xaa7a. `V34hshak.c`'s own `T41_FAA7A`: cleared unconditionally on
	 * entry to microstate 41's DET_SYNC arm. No other reader or writer in
	 * this tree, so what it signals downstream is not established --
	 * only that this one arm resets it.
	 */
	short short_aa7a;				/* +0xaa7a */
	/*
	 * The name is the object's own: one diagnostic prints "On
	 * RX_PHASE1_ANS: is short=%d, bulkDelay=%d, filtDelay=%d" and this
	 * is the third thing it pushes, loaded `movswl` -- signed short, and
	 * the other eighteen of its twenty-one accesses (halfword loads,
	 * stores and compares) agree on the width. Three of microstate 49's
	 * four thresholds and all of 50's are this plus a constant, and
	 * `VPcmV34SetDelays`, `VPcmV34InitiateRetrain` and `VPcmV34Create`
	 * are its other writers (finding F633).
	 */
	short filtdelay;				/* +0xaa7c */
	/*
	 * +0xaa7e.  Round-trip delay, in samples, which `v34handshak` both
	 * measures and consumes.  `V34XF_GetRTD` is the C++ side's window
	 * onto it and adds 480 -- 60 ms at 8 kHz -- before handing it over.
	 */
	short rtd;					/* +0xaa7e */
	unsigned char unmapped_aa80[0xaa96 - 0xaa80];
	/*
	 * The symbol rate in baud, not an index and not a block count:
	 * `V34SetupDemodulator`'s caller compares it against the four V.34
	 * data-mode rates directly, so it holds the rate itself. Written
	 * once, from `T3M_TXBAUD`.
	 *
	 * That makes every threshold built on it a time -- multiplying by
	 * the baud converts seconds into symbols, which is what the
	 * recovery tests in `datapumpv34` are really expressing:
	 *
	 *     baud_rate >> 1   0.5 s of bad blocks -> full retrain (~10 s)
	 *     2 * baud_rate    2 s                 -> renegotiate down
	 *     7 * baud_rate    7 s                 -> handshake timeout
	 *     8 * baud_rate    8 s of good blocks  -> renegotiate up
	 *
	 * Reading them as times is what shows the ladder is inverted: half
	 * a second of trouble buys a ten-second retrain, while the two-to-
	 * three-second renegotiation waits four times as long and never
	 * arrives (finding F1933).
	 */
	short baud_rate;				/* +0xaa96 */
	unsigned char unmapped_aa98[0xaad0 - 0xaa98];
	struct v34_fsk fsk;				/* +0xaad0 */
	short fsk_interp[V34_FSK_TAPS + 1];		/* +0xaae6 */
	short fsk_lpf[V34_FSK_LPF_TAPS];		/* +0xab00 */
	unsigned char unmapped_aba0[0xabae - 0xaba0];
	/*
	 * Ten shorts, and the array is measured rather than inferred from
	 * adjacency: an indexed halfword store runs over the span with the
	 * index counted 0..9, which is what makes this an array and not ten
	 * fields, and nothing reaches inside the span by a constant
	 * displacement anywhere in the object -- the shape an array reached
	 * only by index has.
	 *
	 * `short_abc2` is declared separately because the object writes it
	 * separately, with its own instruction after the loop has ended.
	 * `short short_abae[11]` with the loop stopping one short would
	 * compile to the same code, so eleven is a reading the object does
	 * not force; ten plus one is what it shows.
	 *
	 * Both are sixteen bits wide at every access and neither is ever
	 * sign-extended, so the signedness is not settled -- `short` here is
	 * the struct's convention, not a measurement. V34hshak.c knows them
	 * as `T3M_FABAE` and `T3M_FABC2` (finding F635).
	 */
	short short_abae[10];				/* +0xabae */
	short short_abc2;					/* +0xabc2 */
	unsigned char unmapped_abc4[0xabc6 - 0xabc4];
	/*
	 * The V.92 short-phase-2 negotiation, four shorts, and the object
	 * names all four itself -- `V34GiveINFO0dBits` prints
	 * "localV92- %d, remoteV92- %d, localShort- %d, remoteShort- %d,
	 * isShort- %d" from exactly these plus one INFO0 bit.
	 *
	 * The two `local_*` are inputs nothing reconstructed yet writes;
	 * `remote_v92` and `is_short` are `V34GiveINFO0dBits`'s outputs.
	 * There is no `remote_short` field: that value is read straight out
	 * of the unpacked INFO0 and never stored.
	 */
	short local_v92;				/* +0xabc6 */
	short remote_v92;				/* +0xabc8 */
	short local_short;				/* +0xabca */
	short is_short;					/* +0xabcc */
	/*
	 * Three two-bit fields `V34GiveINFO1aBits` takes out of the first
	 * INFO1a short when the upstream baud index is 6, and immediately
	 * copies on into the session object at +0x14, +0x15 and +0x16.  Each
	 * pair is assembled with the higher-numbered bit as the LOW one --
	 * bit7 + 2*bit6, bit5 + 2*bit4, bit3 + 2*bit2 -- and nothing here
	 * names what the pairs mean.
	 */
	short short_abce;					/* +0xabce */
	short short_abd0;					/* +0xabd0 */
	short short_abd2;					/* +0xabd2 */
	unsigned char unmapped_abd4[0xabd8 - 0xabd4];
	/*
	 * +0xabd8 and +0xabdc, the modem-on-hold timer and its limit.
	 * `VPcmV34Progress` names the first from "Modem On Hold approved by
	 * phase2 (ISP timeout is %d seconds)" and counts samples in the
	 * second against it, with -1 meaning no limit; and
	 * `VPcmV34NotifyDP`'s three-way-call arm prints the second as
	 * "mohTimer = %d" on its way to setting a deadline 48,000 samples
	 * further on.  That second string is what turns the counter from
	 * "the one beside the limit" into a named field.
	 */
	int moh_limit;					/* +0xabd8 */
	int moh_timer;					/* +0xabdc */
	/*
	 * The hold-time code carried in an MHack message's first short (added
	 * to 0x50), in both directions: the one received and the one this
	 * end will send back. `VPcmV34InterpretMohMessageBits` names it --
	 * "MHack message detected ! , Time out period code = %d".
	 */
	short moh_holdtime_code;					/* +0xabe0 */
	/*
	 * +0xabe2. Set to 3 by exactly one arriving message -- the MHnack that
	 * says the far end may not initiate MOH later (the MHnack that says it
	 * may does not write it) -- and also to 1 by microstate 80's disconnect
	 * path, when the far end never sent its MH sequence under MHfrr. Read
	 * by nothing this tree has reconstructed.
	 */
	short short_abe2;					/* +0xabe2 */
	/*
	 * +0xabe4 and +0xabe6, two of the four halfwords of what was
	 * `unmapped_abe4` (0xabe4..0xabec, 8 bytes). `v34hstx1.cpp`'s own
	 * `TX1_FABE4`/`TX1_FABE6`: the hold tail's clear-down raises
	 * `short_abe4` on the way out, and the two `v34handshakinit` paths that
	 * start a fresh cycle raise `short_abe6`. Neither is read anywhere in
	 * this tree, so what either signals downstream is not established.
	 */
	short short_abe4;				/* +0xabe4 */
	short short_abe6;				/* +0xabe6 */
	/*
	 * The Modem-on-Hold active flag: while set, 24 `TX_DPSK`'s clock ticks
	 * `vect_idx` once per call. `v34hstx1.cpp`'s `TX1_FABE8` and
	 * `V34hshak.c`'s microstate 41 (`T41_FABE8`) are the same offset under
	 * two names. +0xabea is still unmapped.
	 */
	short moh_active;				/* +0xabe8 */
	unsigned char unmapped_abea[0xabec - 0xabea];
	/*
	 * The Modem-on-Hold message this end originally asked for, against
	 * the one it is building now in `moh_message` (four bytes on) -- read
	 * 32 bits wide, which is what makes this an `int` rather than two more
	 * halfwords. One diagnostic prints the pair together -- "V34F MOH:
	 * After 192 silence, org = %d , act = %d", this field as `org` and
	 * `moh_message` as `act` -- and goes to MOH_FRR rather than
	 * MOH_ON_HOLD when either of them is 1, in `moh_message`'s
	 * numbering where 1 is MHfrr.
	 *
	 * Nothing else this tree has reconstructed reads or writes it.
	 */
	int moh_org;					/* +0xabec */
	/*
	 * +0xabf0.  Which Modem-on-Hold message to build, 0..5, and the six
	 * are named by the object's own strings: 0 MHreq, 1 MHfrr, 2 MHclrd,
	 * 3 MHcda, 4 MHack, 5 MHnack.  Anything above 5 builds nothing --
	 * the test is unsigned, so a negative value falls there too.
	 */
	int moh_message;				/* +0xabf0 */
	/*
	 * The message that arrived, in the same six names but not the same
	 * numbering as `moh_message` above:
	 *
	 *     0  MHreq   1  MHfrr   2  MHcld   3  MHcda   4  MHack
	 *     5  MHnack, and also 5 for a message that is none of them
	 *
	 * against the sending side's 0 MHreq, 1 MHfrr, 2 MHclrd, 3 MHcda,
	 * 4 MHack, 5 MHnack.  They agree, which is worth saying explicitly
	 * because nothing forces them to and the two fields are written by
	 * different translation units.  An unrecognised message is forced to
	 * MHnack and says so -- "forcing message type to MH NACK".
	 *
	 * Written by `VPcmV34InterpretMohMessageBits` and read by nothing
	 * reconstructed so far.
	 */
	int moh_recvd;					/* +0xabf4 */
	/*
	 * +0xabf8 and +0xabf9, the two bytes of what was `unmapped_abf8`.
	 * `v34hstx1.cpp`'s own `TX1_FABF8`/`TX1_FABF9`, read with `cmpb`: while
	 * `moh_msg_pending` is set, 24 `TX_DPSK`'s hold tail runs the message
	 * dispatch and clears it, and once it is clear the arm re-arms the
	 * reader instead.  `moh_path_sel` is read twice more -- which message
	 * is built, and which of the tail's three exits is taken.  Neither has
	 * another reader here.
	 */
	unsigned char moh_msg_pending;			/* +0xabf8 */
	unsigned char moh_path_sel;			/* +0xabf9 */
	/*
	 * +0xabfa.  A byte that picks between three MHclrd codes -- 0x95,
	 * 0x96 and 0x9a for values 0, 1 and anything else -- and the same
	 * byte the arriving MHcld's low nibble is decoded into: 5 incoming,
	 * 6 outgoing, 0xa other, anything else "reserved (assume other)".
	 * So 0 is incoming, 1 outgoing and 2 other, and the sending side's
	 * three codes line up with the three the receiver recognises.
	 */
	unsigned char moh_clrd_sel;				/* +0xabfa */
	/*
	 * +0xabfe, inside the run below.  `v34handshakinit` clears it and
	 * `v90Phase34` sets it to 1 on the one path whose own diagnostic
	 * names it: "tx buffer backward clear is enabled".  Reached by offset
	 * because nothing else in the run is mapped.
	 */
	unsigned char unmapped_abfb[0xac02 - 0xabfb];
	/*
	 * +0xac02.  `V34SetINFO0aBits` puts 20 here when it asks for a short
	 * phase 2, and says what it is doing: "Setting prev bulk delay = %d".
	 * Distinct from the bulk-delay ring at +0x35a8, which is the second
	 * echo canceller's.
	 */
	short prev_bulk_delay;				/* +0xac02 */
	/*
	 * The negotiated rates in bits per second -- 2400 times the counts in
	 * the rate config -- published once and latched, so a second
	 * negotiation does not overwrite them.
	 */
	int tx_bps;					/* +0xac04 */
	int rx_bps;					/* +0xac08 */
	/* Where the V.90 side is told the recovered timing offset. */
	short v90_timing_offset;					/* +0xac0c */
	/*
	 * The two rate-renegotiation counters, and the only fields here whose
	 * names come from a whole function rather than from a string:
	 * `VPcmV34IndicateLocalRRN` is nothing but the increment of the first
	 * and `VPcmV34IndicateRemoteRRN` nothing but the increment of the
	 * second.  `VPcmV34InitiateRateRenegotiation` bumps `rrn_local`
	 * inline, which is the same event counted at its source.
	 *
	 * Both wrap at 16 bits and `VPcmV34GetDiagnostics` reads `rrn_local`
	 * back signed, so a session past 32,767 local renegotiations reports
	 * a negative count.  Not entered as a deviation: it takes a run no
	 * real call would reach.
	 */
	short rrn_local;				/* +0xac0e */
	short rrn_remote;				/* +0xac10 */
	/*
	 * Two signed shorts, and the width and the sign are both forced:
	 * `VPcmV34GetDiagnostics` reads each with a single `movswl` into a
	 * 32-bit slot of `TAG_DiagnosticResults`, the promotion of a `short`
	 * and not of an `unsigned short`. The other two accesses agree on
	 * the width -- `VPcmV34Create` clears both, and `v34handshakinit`
	 * reads each back and stores a halfword, where the upper half never
	 * survives (CLAUDE.md's free column, finding F614).
	 *
	 * Offset-named: no format string in the object prints either, no
	 * reconstructed function does arithmetic on them, and the one
	 * consumer copies them out unchanged -- `short_ac12` into three
	 * diagnostics offsets at once and `short_ac14` into a fourth.
	 */
	short short_ac12;				/* +0xac12 */
	short short_ac14;				/* +0xac14 */
	/*
	 * A byte past where this struct used to end: the declared length of
	 * 0xac10 was the largest offset anything reconstructed had touched,
	 * not a bound the object proves.  initdigital writes here.
	 */
	unsigned char rates_latched;			/* +0xac16 */
	/*
	 * A byte, and `VPcmV34SetIndicationOfRemoteRetrain` is the whole of
	 * what writes it, storing 1.  Named from that function and from
	 * nothing else: no reader has been reconstructed, so what consumes
	 * the flag is not established, only that this one entry point
	 * raises it and never lowers it.
	 */
	unsigned char remote_retrain_ind;		/* +0xac17 */
	/*
	 * A second pointer into the C++ side, distinct from `p3548`, and
	 * `V34GiveINFO1aBits` reads exactly one thing through it: an int
	 * printed as the local PCM type in "K56Flex enabled by remote, PCM
	 * type: local %d, remote %d (A=1, Mu=0)".
	 */
	void *pac18;					/* +0xac18 */
	unsigned char unmapped_ac1c[0xac3c - 0xac1c];
	/*
	 * A third pointer into the C++ side, and the busiest of the three:
	 * forty-odd loads of it, all inside `VPcmV34Main.cpp`'s C exports.
	 * Where `p3548` is the session and `pac18` is the PCM type, this one
	 * is the negotiated configuration -- everything read through it is a
	 * limit somebody asked for:
	 *
	 *   +0x3c  int    the maximum upstream rate, in bits per second;
	 *                 `VPcmV34GetMaxUpstreamRateIndex` returns it
	 *   +0x44  short  a transmit power reduction in dB, clamped to
	 *                 [-10, +7] by `GetVPcmMinimalTxPowerReduction`
	 *   +0x50  byte   bits 5..7 are the maximum V.34 BAUD RATE INDEX,
	 *                 which is what `chkForceBaudRate` reads and its own
	 *                 trace names: "max V34 baud rate index = %d". Bit 2
	 *                 is a second and unrelated reader: `v90Phase34`
	 *                 tests it once the Ja sequence has finished, and
	 *                 its diagnostic names it -- "tx buffer backward
	 *                 clear is enabled". It gates the byte at +0xabfe
	 *   +0x54  int    compared against 4 by GetVPcmMinimalTxPowerReduction
	 *
	 * Reconstructed as a `void *` with each field spelled out at its use,
	 * for the reason `p3548`'s note gives: a struct here would be a
	 * guess, and the object never states one.
	 */
	void *pac3c;					/* +0xac3c */
	/*
	 * The object is 0xac4c bytes; `VPcmV34Progress`'s
	 * `requestOutputSampleClear` arm writes all three words here (a
	 * sample count, a flag set to 1 beside it, and a third word cleared
	 * to 0), and `VPcmV34RequestDPNotification` is the other end: it
	 * hands the three words out through three `int *`, in this order,
	 * then resets them to -1, 0 and 0. So the group is a one-deep
	 * request mailbox between the datapump and its caller: `clr_flag`
	 * says whether one is outstanding and negative means none, `clr_count`
	 * is the writer's `n * 2` in samples, and `clr_done` is written zero
	 * by both sides and read by neither, so the name says where it sits
	 * in the triple and not what it carries.
	 */
	int clr_flag;					/* +0xac40 */
	int clr_count;					/* +0xac44 */
	int clr_done;					/* +0xac48 */
};

/*
 * The rate configuration initdigital fills, at +0xaa84 in the object.
 *
 * Two halves, transmit then receive, and the transmit one is what feeds
 * initV34 for the context at +0x25e0 while the receive one feeds +0xa00.
 * `bits` counts units of 2400 bps, so the bit rate is 2400 times it.
 *
 * `rx_baud` is `baud_rate` above -- one store, two readings, the same
 * situation as the echo array that is also the FSK delay line (finding
 * F100). It is declared in both places on purpose; there is no third field.
 *
 * The four "current" getters name four of these, in transmit/receive
 * pairs: `VPcmV34GetCurrentTxBaudRate`/`...RxBaudRate` read `baud`/`rx_baud`,
 * `...TxBitRate` reads `txbits`, and `...TxCarrier`/`...RxCarrier` read
 * `carrier`/`rx_carrier`. `carrier` is the only member of the struct whose
 * name comes from a getter rather than from a use.
 */
struct v34_ratecfg {
	short baud;			/* +0x00 transmit symbol rate    */
	/*
	 * +0x02.  `v90Phase34`'s case 3 prints it as `period`, beside
	 * `tx->symcnt`, and that diagnostic is the only thing in the tree
	 * that names it.  Note it is NOT `tx->period`: the string carries no
	 * prefix on it and the load is from this record rather than from the
	 * transmitter.  Nothing reconstructed writes it.
	 */
	short period;			/* +0x02                         */
	short txbits;			/* +0x04 in units of 2400 bps    */
	/*
	 * The transmit pre-emphasis filter index, and the object names it:
	 * one diagnostic prints this record's `baud`, `carrier` and this
	 * field together as "VPcmV34Main: About to setup v34 txmit, baudrate
	 * = %d, carrier = %d, preemp = %d", in that order -- two of the
	 * three land on fields already named from elsewhere, so the string
	 * is checked against known answers on both sides of the one it
	 * settles. That agrees with every use already recorded:
	 * `setfinalrate` writes it, `v34setuptxmit` and `v34handshak` read
	 * it beside `baud` and `carrier` -- the three parameters of a V.34
	 * transmitter setup -- and `V34SetINFO1aBits` clears it when it
	 * hard-codes the other two for a short phase 2.
	 */
	short preemp;			/* +0x06                         */
	short depth;			/* +0x08 trellis, initV34's arg  */
	short use_max;			/* +0x0a picks MMaxTable         */
	const short *divtab;		/* +0x0c the divisor table       */
	short carrier;			/* +0x10 transmit carrier, Hz    */
	short rx_baud;			/* +0x12 receive symbol rate     */
	short rxbits;			/* +0x14                         */
	unsigned char pad_16[0x22 - 0x16];
	short rx_use_max;		/* +0x22                         */
	/*
	 * The receive carrier: the field `VPcmV34GetCurrentRxCarrier`
	 * returns, and the receive half of the same tx/rx pairing every
	 * other member of this struct has.
	 */
	short rx_carrier;		/* +0x24                         */
	const short *rx_divtab;		/* +0x28
					 * pad_26[2] removed here -- pure
					 * alignment gap ahead of this pointer;
					 * `rx_carrier` ends on a 2-mod-4
					 * offset.  Confirmed by the assertion
					 * below and a whole-object disassembly
					 * search: nothing reads or writes
					 * absolute offset 0xaaaa/0xaaab
					 * (finding F10148). */
};

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char v34ratecfg_off_rxdivtab[
	((int)__builtin_offsetof(struct v34_ratecfg, rx_divtab) == 0x28)
		? 1 : -1];
typedef char v34ratecfg_size[
	(sizeof(struct v34_ratecfg) == 0x2c) ? 1 : -1];
#endif

#define V34_RATECFG	0xaa84

/**
 * @brief Interpolate, discriminate and filter one FSK block.
 *
 * Reads exactly #V34_FSK_BLOCK samples from @p in and writes exactly
 * #V34_FSK_PHASES to @p out. Neither length is a parameter: both are built
 * into the loop bounds, so a caller cannot ask for more or less.
 *
 * @param obj  The V.34 modem object.
 * @param in   Input samples, #V34_FSK_BLOCK of them.
 * @param out  Output samples, #V34_FSK_PHASES of them.
 * @param cfg  The FSK configuration; only `delay` and `offset` are used.
 */
void fskdetect(struct v34_object *obj, const short *in, short *out,
	       const struct v34_fsk *cfg);

/**
 * @brief Run fskdetect() over one block and slice the result into bits.
 *
 * Does nothing at all if `obj->fsk_inhibit` is set.
 *
 * @param obj  The V.34 modem object.
 * @param in   Input samples, #V34_FSK_BLOCK of them.
 * @param st   Both the configuration handed to fskdetect() and the
 *             bit-clock state; the caller reads the recovered bits back
 *             out of `st->sr` and `st->nbits`.
 */
void fskdemodulate(struct v34_object *obj, const short *in,
		   struct v34_fsk *st);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34FSK_H */
