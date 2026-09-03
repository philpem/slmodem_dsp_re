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
/*
 * The scrambler and descrambler shift registers.
 *
 * BOTH ARE 128 BITS STEPPED SIXTEEN AT A TIME, held as words rather than as
 * a bit array, and both polynomials are applied to a whole 16-bit step at
 * once rather than bit by bit.  That is what makes `scrambleGPA` look
 * unlike `scrambleGPC`: GPC's shorter tap is 18 bits back, further than one
 * step, so one pass suffices; GPA's is 5 bits back, closer than one step, so
 * its feedback has to be folded in four times to cover sixteen.
 *
 * `unsigned` because two of the four shift right logically.
 */
struct v34_scrambler {
	unsigned w[4];		/* +0x00 */
	short nbits;		/* +0x10  0x20 out of preinitdigital */
};

struct v34_descrambler {
	unsigned w[3];		/* +0x00 */
	short count;		/* +0x0c  bits held, flushed past 31 */
};

struct v34_object {
	/*
	 * +0x0000.  The datapump's own status word: `receiver` puts 10 in it
	 * on loss of signal, alongside the string "Signal Energy below
	 * Threshold %d, initiate a disconnection".  What the other values
	 * mean belongs to VPcmV34Main.cpp, which is not reconstructed.
	 *
	 * ONE OF THEM IS NOW SOURCED: 1 and 2 mean a PCM receiver is running.
	 * `VPcmV34InitiateHangUp` and `VPcmV34InitiateRateRenegotiation` both
	 * test `(unsigned)(status - 1) <= 1` and, when it holds, hand the
	 * request to the V.90 side through `p3548` instead of to the V.34
	 * handshake -- and `VPcmV34GetCurrentTxBitRate` takes the
	 * `V90Demodulator` branch on exactly the same test.
	 */
	int status;					/* +0x0000 */
	/*
	 * +0x0004.  An int the shell polls: `VPcmV34InitiateHangUp`,
	 * `VPcmV34InitiateRateRenegotiation` and `VPcmV34SetV90RateReneg` all
	 * put 6 in it, `datapumpv34` puts 5 when the sample count at +0x238
	 * has run 287,488 past the mark at +0x248, and `VPcmV34Create` puts
	 * 10 and then 0.
	 * Nothing reconstructed reads it, so the values are all this says.
	 */
	int f0004;					/* +0x0004 */
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
	 * THE DATAPUMP'S TWO DATA BUFFERS, and the layout tiles exactly:
	 * 0x14 + 64*4 is precisely 0x114 where the receive count lives, and
	 * 0x118 + 64*4 precisely 0x218 where the transmit one does, so each
	 * array ends where its counter begins and nothing had to be guessed
	 * at to make the four bounds meet.
	 *
	 * descrambleGP* appends each sixteen recovered bits to `rx_data` and
	 * stops at 64 entries; scrambleGP* takes its next sixteen out of
	 * `tx_data` and, once `tx_rd` reaches `tx_n`, scrambles 0xffff
	 * instead -- the all-ones idle V.34 sends with nothing to carry.
	 * Both paths are gated on `data_enable` at +0x2214.
	 *
	 * The arrays are `int` and only the low short of each transmit entry
	 * is read -- `movzwl 0x114(%edx,%eax,4)`.
	 */
	int rx_data[64];				/* +0x0014 */
	int rx_n;					/* +0x0114 */
	int tx_data[64];				/* +0x0118 */
	int tx_n;					/* +0x0218 */
	int tx_rd;					/* +0x021c */
	/*
	 * +0x0220 to +0x022c.  THE RATE RENEGOTIATION'S FOUR WORDS, all
	 * indices rather than bit rates -- `VPcmV34GetCurrentRxBitRate`
	 * multiplies the same units by 2400 to answer in bits per second.
	 *
	 * Every one of the four names is read off a site, not inferred from
	 * the group:
	 *
	 *   min, max   `VPcmV34SetMinMaxBitRates` divides a pair of rates by
	 *              2400 into these two, caps both at 14, and then raises
	 *              +0x224 to +0x220 if it is below it.  Which way round
	 *              that fix-up goes is settled by
	 *              `VPcmV34InitiateRateRenegotiation`, whose step down
	 *              clamps at +0x220 and whose step up clamps at +0x224.
	 *
	 *   now        `v34handshak` at 0x63d03 stores the negotiated rate
	 *              index -- a copy of +0xaa98, the field the getter
	 *              multiplies -- into +0x228 and +0x22c together.
	 *
	 *   want       and at 0x63395 it reads +0x22c back, skips on `js`
	 *              and then on equality with +0x228, and otherwise
	 *              adopts it as +0xaa98.  So this is a REQUEST, and
	 *              NEGATIVE MEANS "no target": `VPcmV34InitiateRate-
	 *              Renegotiation` writes -1 for a renegotiation that
	 *              names no rate, and `VPcmV34InitiateHangUp` clears the
	 *              request and both bounds but leaves `rate_now` alone.
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
	 * +0x0234 to +0x024b holds the sample-clock timer -- +0x238 the
	 * running count and +0x248 the instant a span is measured from,
	 * with +0x23c and +0x244 written alongside them.  Left unnamed and
	 * unmapped: every writer reaches the group through `obj + 4` and
	 * spells the offsets out, which is what v34hshak.c's note explains,
	 * and only two of the four have a reader to name them from.
	 *
	 * `datapumpv34` reads +0x238 and +0x248 at their TRUE offsets and
	 * puts 5 in `f0004` when the difference passes 287,488; `v34handshak`
	 * copies +0x238 into +0x248 to restart the span.  Between them those
	 * are the second and third readings that settle finding F179's
	 * register-relative offsets as four low.
	 *
	 * TWO OF THE FOUR ARE NAMED NOW, by `VPcmV34SetTimeOut`, which is the
	 * only function in the object that writes the pair together and the
	 * only one that establishes what the second is for:
	 *
	 *     6e90  imul $0x2580,0x8(%esp),%eax     seconds * 9600
	 *     6ea1  mov  %ecx,0x234(%edx)           obj+0x238 <- 0
	 *     6ea7  mov  %eax,0x238(%edx)           obj+0x23c <- that
	 *
	 * so +0x238 is the running count `datapumpv34` reads (restarted here)
	 * and +0x23c is a DEADLINE in the same units.  9,600 is 8 kHz times
	 * 1.2 and not the sample rate; the argument is therefore not seconds
	 * of wall clock at the codec rate, and the scale is left as the
	 * constant it is rather than named a unit this cannot prove.
	 * +0x234, +0x240 and +0x244 are still unnamed and still unmapped.
	 */
	unsigned char unmapped_0234[0x238 - 0x234];
	int sample_count;				/* +0x0238 */
	int timeout_deadline;				/* +0x023c */
	unsigned char unmapped_0240[0x24c - 0x240];
	/*
	 * How far the V.90 receiver has got through phase 3, as a number the
	 * handshake's C++ side ratchets forward.  `V34XF_Indicate-
	 * Trn2dReceived`'s own debug string names it -- "IndicateTrn2d-
	 * Received called, v90Receiver = %d" -- and the Indicate entry points
	 * between them set 3 (Jd), 6 (DIL) and 10/14/18/20 (TRN2d).
	 *
	 * ALSO REACHED THROUGH `obj + 4`, so the note above is now true of
	 * three files rather than two: VPcmV34Main.cpp's code generation for
	 * this field and the next is `lea 0x4(obj); mov 0x248(that)`.  It is
	 * an addressing artifact in all three and is not evidence of a
	 * sub-object at +4.
	 *
	 * IT IS NOT ONLY RATCHETED.  `VPcmV34SetV90RateReneg` assigns 11 or
	 * 15 outright, so a renegotiation can move it down as well as up;
	 * see D48.
	 */
	int v90_receiver;				/* +0x024c */
	/*
	 * The same for K56Flex: `V34XF_IndicateK56FlexRateDetermined` sets 5
	 * and nothing reconstructed yet reads it.  `V34GiveProbeResults`
	 * treats the pair as one "is a PCM receiver running" test.
	 */
	int k56flex_receiver;				/* +0x0250 */
	unsigned char unmapped_0254[0x25c - 0x254];
	/*
	 * adaptecho's three scalars, immediately before the receiver.
	 * `dmadelay` is the base the echo filter's lag is measured from,
	 * f25e the transmit sample it just dequeued, f260 the running
	 * residual.
	 *
	 * +0x25c IS `V34dmadelay`, and it is the object's own spelling:
	 * `VPcmV34SetDelays` computes `0x610 - cfg[0x68]`, stores it here and
	 * reports it as "V34FEC, V34dmadelay set to %d, (ext delay=%d)" with
	 * the stored value first and the configured one second.  Was `f25c`.
	 * `VPcmV34Create` and `VPcmV34InitiateRetrain` already carried the
	 * same expression against the same offset.
	 */
	short dmadelay;					/* +0x25c */
	short f25e;					/* +0x25e */
	short f260;					/* +0x260 */
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
	 * +0x0382.  WRITTEN SIX TIMES AND READ NOWHERE in this object -- the
	 * only stores are VPcmV34Main.cpp's, four of them in the two Indicate
	 * entry points reconstructed here.  So whatever consumes it lives in
	 * the C++ half that is still to come, and the values are all this
	 * says about it: 0 when a Jd arrives with the silence-scrambler flag
	 * set, otherwise 0x89b0 or 0x8990 according to a constellation-size
	 * flag.  The two differ by 32, which is the only structure visible.
	 *
	 * As a `struct v34_receiver` offset this is +0x11e, immediately below
	 * that struct's `vectpp_idx`/`flags` pair; named here rather than
	 * there because every caller has the whole object in hand.
	 */
	short f382;					/* +0x0382 */
	unsigned char unmapped_0384[0x402 - 0x384];
	/*
	 * Non-zero makes fskdemodulate return without doing anything -- not
	 * even running the detector -- so it reads as "the FSK receiver is
	 * switched off".  Nothing here sets it.
	 */
	short fsk_inhibit;				/* +0x402 */
	unsigned char unmapped_0404[0x49c - 0x404];
	/*
	 * +0x049c and +0x04a0, named from the object's OWN EXPORTED ACCESSORS
	 * `getTimingOffset` (0x71b0) and `getTimingPhase` (0x71c0) -- eleven
	 * bytes each, `mov 0x49c(%eax),%eax` / `mov 0x4a0(%eax),%eax` on the
	 * argument, sitting in .text between `VPcmV34GetSNR` and
	 * `VPcmV34GetCleanedSamples` where every neighbour takes a
	 * `tagV34Object *`.  The symbol names are the author's words, the
	 * same class of evidence as a format string; what the fields MEAN
	 * beyond that is not claimed, because the two accessors are the only
	 * readers anywhere in the 1.2 MB (they are in the no-entry-point
	 * bucket -- exported API with no internal caller) and no writer has
	 * been traced to them.
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
	 * +0x2214.  Non-zero connects the scrambler callbacks to `tx_data`
	 * and `rx_data`; zero leaves the transmitter scrambling idle ones and
	 * the receiver throwing away what it recovers.  `preinitdigital`
	 * clears it and `modulatevector` sets it when the training-to-data
	 * symbol counter expires, which are the object's only two writers --
	 * so this is the data path being switched on at the end of training,
	 * not a facility something outside the core operates.
	 */
	short data_enable;				/* +0x2214 */
	unsigned char unmapped_2216[0x221c - 0x2216];
	struct v34_queue txq;				/* +0x221c */
	int txq_ring_tail[V34_TXQ_RING - 1];		/* to +0x25c0 */
	short f25c0;					/* +0x25c0 */
	/*
	 * +0x25c2.  Bit 9 gates the echo feed and bit 2 says both cancellers
	 * are frozen; BIT 0 picks the scrambler generator -- set for the
	 * calling station's, clear for the answering one.  txmitdibit and
	 * txmitquadbit are what pin the last of those.
	 */
	short f25c2;					/* +0x25c2 */
	unsigned char unmapped_25c4[0x25c6 - 0x25c4];
	/*
	 * The differentially-encoded quadrant, carried from one symbol to the
	 * next: f25c6 is the previous one and f25c8 the current.  In the
	 * dibit case the two end up equal; in the quadbit case f25c8 is set
	 * first and f25c6 only catches up at the end, because the second
	 * dibit indexes off the first one's quadrant.
	 */
	short f25c6;					/* +0x25c6 */
	short f25c8;					/* +0x25c8 */
	unsigned char unmapped_25ca[0x25cc - 0x25ca];
	/* The transmit scrambler's shift register. */
	int f25cc;					/* +0x25cc */
	/*
	 * +0x25d0.  The point being transmitted: two shorts, real then
	 * imaginary, and EVERY ARM THAT SENDS A CONSTELLATION POINT WRITES
	 * BOTH WITH ONE 32-BIT STORE --
	 *
	 *   txmitdibit     0x5e6c5   mov %edx,0x3b4(%edi)   edi = obj+0x221c
	 *   txmitquadbit   0x5e582   mov %ecx,0x3b4(%edi)   edi = obj+0x221c
	 *   modulatevector 0x59e94   mov %edx,0x25d0(%esi)  esi = obj
	 *
	 * which is why `vect4` and `vect16` hold ints at all.  The halves
	 * are ALSO written and read separately as shorts: the silent symbol
	 * zeroes each on its own, and `txmit` reassembles them as
	 * `(im << 16) | (unsigned short)re` before handing the result to
	 * `V34ModulatorProcess`.  That reader is the whole of the evidence
	 * for the name -- `txmit` is the only thing in the object that reads
	 * this pair, so it is the point going OUT and not one coming in.
	 * Was `f25d0` (`symbol re`) and `f25d2` (`symbol im`).
	 *
	 * The union spells the aliasing out rather than casting a pointer,
	 * which -O2 is entitled to reorder; `v34_receiver::energy` is the
	 * same reading and the same remedy.  `c` is an ARRAY rather than two
	 * named fields because `V34nlencoder` is handed `txpoint.c` and
	 * writes `out[0]` and `out[1]` through it -- two named shorts would
	 * make that second store its own out-of-bounds defect.
	 *
	 * A 4-byte STRUCT ASSIGNMENT would emit the same single `movl` and
	 * is an equally consistent reading of the original; nothing in the
	 * object separates the two, so this does not claim to have settled
	 * which the author wrote.  See finding F5303.
	 */
	union {
		int word;				/* +0x25d0 both at once */
		short c[2];				/* [0] real, [1] imag  */
	} txpoint;
	short f25d4;					/* +0x25d4 tx scale  */
	unsigned char unmapped_25d6[0x25dc - 0x25d6];
	/*
	 * +0x25dc.  The transmit power reduction in WHOLE dB, which
	 * `settxlevel` assembles from the far end's MP message and then
	 * applies to `f25d4` above -- so this is the request and that is the
	 * result.  Its own diagnostic names it: "power reduction requested by
	 * remote modem is %d dB".
	 *
	 * A SHORT, and `filds` at 0x7646 and 0x77d0 is the confirmation:
	 * something in the V.90 half loads it as a 16-bit integer straight
	 * onto the x87 stack, so it is neither an int nor a pair of bytes.
	 *
	 * It can be negative.  With a V.90 receiver running, a
	 * `GetVPcmMinimalTxPowerReduction` below zero is ADDED to the
	 * request rather than compared with it, and the result drives
	 * `settxlevel`'s other loop -- the one that raises the scale.
	 */
	short f25dc;					/* +0x25dc */
	unsigned char unmapped_25de[0x2a54 - 0x25de];
	/* The scrambler's shift register; see `struct v34_scrambler`. */
	struct v34_scrambler scrambler;			/* +0x2a54 */
	unsigned char unmapped_2a68[0x2a80 - 0x2a68];
	/*
	 * +0x2a80.  `modulatevector`'s output: eight complex points as
	 * sixteen shorts, and the cursor into them.  One call emits point
	 * `vect_idx` and bumps it; only when it reaches 8 does the mapping
	 * run and refill all eight.  `scaleVector`'s sixteen shorts are
	 * exactly this array.
	 *
	 * In transmit-shell coordinates these land at +0xea0 and +0xec2,
	 * inside `sub[]` and `cost[]` -- which is not a contradiction but the
	 * reason they are declared HERE.  Only the receive context decodes,
	 * so the transmit one has no sub-indices, costs, trellis or states,
	 * and the object reuses the space.  `preinitdigital` clearing those
	 * three arrays on the receive side and on neither other says the same
	 * thing from the other direction.  See finding F181.
	 *
	 * THE POINTS ARE ALSO ADDRESSED AS EIGHT INTS, and `modulatevector`
	 * is what pins both readings at once: it writes single shorts here
	 * (`mov %dx,0x2a80(%edi)` at 0x5a4fd, `mov %ax,0x2a80(%esi)` at
	 * 0x5a78e) and then loads one whole point with `mov
	 * 0x2a80(%esi,%eax,4),%edx` at 0x59e86, scaling the index by FOUR,
	 * to hand to the 32-bit store at +0x25d0.  Sixteen shorts and eight
	 * ints over the same 32 bytes, spelled as a union rather than
	 * reached with `*(int *)&vect[2 * n]` -- which is what -O2 is
	 * entitled to reorder.  `vect` keeps its name, so every short-wise
	 * user is unchanged; `vectp` is the same storage as points.
	 */
	union {
		short vect[16];				/* +0x2a80 */
		int vectp[8];				/* +0x2a80, one per point */
	};
	short f2aa0;					/* +0x2aa0 */
	short vect_idx;					/* +0x2aa2 */
	short f2aa4;					/* +0x2aa4 */
	short f2aa6;					/* +0x2aa6 */
	/*
	 * Two per-symbol history rings modem_serrint fills, indexed by f2aa4
	 * and f2aa6 and wrapping at 0x12b and 0x257 respectively.  The first
	 * holds each residual TWICE, as both halves of its entry -- so it is
	 * a complex buffer being written with a real value.
	 *
	 * ITS ELEMENT IS TWO SHORTS AND NOT AN INT, and modem_serrint is
	 * what settles it: the entry is filled by TWO 16-bit stores,
	 * `mov %si,0x2aa8(%edx)` at 0x5d0f7 and `mov %si,0x2aaa(%edx)` at
	 * 0x5d0fe, where a 32-bit field would have taken one `movl`.  It was
	 * declared `int[0x12c]` here and every writer reached the halves by
	 * casting `(short *)&hist_2aa8[k]`, which is the declaration being
	 * wrong rather than the access being clever.  Same 0x4b0 bytes.
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
	 * adaptecho's adaptation state.  f354c counts calls and gates the
	 * whole slow path; f3550 is the LMS step (updateAlpha's alpha, and
	 * the only short here); f3558 its decay; f355c a shift the step is
	 * scaled by, which the ladder at 0x90 moves between 2, 4 and 5; and
	 * f3560 the energy accumulated over the first 0x8f calls.
	 *
	 * AND THREE OF THEM ARE NAMED BY THEIR OTHER WRITER.
	 * `GetVPcmMinimalTxPowerReduction` sets f3554, f3558 and f355c
	 * together and then prints what it set: "setting echo: decay start =
	 * %d, decay fact = %d, beta = %d".  So f3554 is the call count decay
	 * starts at -- which is exactly what `adaptecho` compares it against
	 * -- f3558 is the decay factor and f355c is beta.
	 *
	 * The two writers do not agree on beta's range: `adaptecho`'s ladder
	 * moves it between 2, 4 and 5, and the PCM side sets 2, 4 or 6.  Both
	 * readings are the object's; nothing here reconciles them.
	 */
	int f354c;					/* +0x354c */
	short f3550;					/* +0x3550 */
	short f3552;					/* +0x3552 */
	int f3554;					/* +0x3554 */
	int f3558;					/* +0x3558 */
	int f355c;					/* +0x355c */
	int f3560;					/* +0x3560 */
	/*
	 * +0x3564 IS THE OBJECT'S OWN `struct v34_detector`, and the two
	 * ends meet exactly: `sizeof(struct v34_detector)` is 0x24 and
	 * 0x3564 + 0x24 is 0x3588, which is where the next field measured
	 * below begins.  v34hshak.c reaches it there as `T3C_DETECTOR` /
	 * `T3M_DETECTOR` and casts, and those arms are differentially
	 * tested.
	 *
	 * IT IS NOT EMBEDDED HERE, and that is a deliberate under-claim.
	 * Two things meeting is adjacency, not a bound -- finding F215's
	 * rule -- and the 0x24 is OUR declaration's size rather than
	 * anything the object states.  So the tiling is recorded as the
	 * measurement it is and the span stays a pad.  Finding F630.
	 */
	unsigned char unmapped_3564[0x3588 - 0x3564];
	/*
	 * +0x3588 and +0x358a.  TWO 16-BIT FIELDS on 68 accesses -- 42 at
	 * +0x3588 and 26 at +0x358a -- every one of them a halfword load, a
	 * halfword store or a `cmpw`.  Nothing reads either at a byte width
	 * and nothing reads an odd displacement inside them.
	 *
	 * AND TWO SITES READ THE PAIR 32 BITS WIDE.  0x65c24 and 0x66495
	 * are `cmpl $0x20002,0x3588(%reg)`: the two halves against 2 and 2
	 * in one instruction.  That is the ORIGINAL SOURCE's spelling and
	 * not its compiler's -- GCC 3.4.4, which is what `tools/toolchain`
	 * runs as the period compiler, compiles `s->a == 2 && s->b == 2` on
	 * two adjacent shorts into two separate `cmpw` with this object's own
	 * flags and never fuses them (measured; finding F631).  The blob's own
	 * `.comment` says 3.4.2; the container is a stand-in for it.
	 * v34hshak.c reads exactly those two sites through `T3M_I32` and
	 * every other site through `T3M_I16`, which is why both readings
	 * survive.
	 *
	 * So this span is finding F553's shape -- one region, two widths,
	 * both the object's -- and the OFFSET SPELLING STAYS at every use
	 * site.  The declaration here is the 16-bit reading because that is
	 * what 68 of the 70 accesses say.
	 *
	 * SIGNEDNESS IS NOT SETTLED for either.  No access to either offset
	 * sign-extends, and a `movzwl` whose upper half is discarded is a
	 * free choice for the compiler (finding F614), so `short` here
	 * matches the rest of this struct and is not a measurement.
	 *
	 * `fNNNN` and not a description: what they are FOR was not measured.
	 * v34hshak.c knows them as `T3M_F3588` and `T3M_F358A` and records
	 * what each use does.
	 */
	short f3588;					/* +0x3588 */
	short f358a;					/* +0x358a */
	/*
	 * +0x358c.  SIGNED short, and the sign is FORCED: 0x62d48 loads it
	 * `movswl`, masks bit 0 and indexes a table with scale 8, and
	 * 0x6343d and 0x677da shift the sign-extended result left by 6 and
	 * by 10 into a word being assembled.  A 32-bit result that is used
	 * is CLAUDE.md's own case for acting on the extension.  Thirty
	 * accesses, all sixteen bits wide.  `v34handshak`'s microstate 48
	 * inverts bit 0 of it -- v34hshak.c's `T3M_TOGGLE` and `T3C_F358C`.
	 */
	short f358c;					/* +0x358c */
	unsigned char unmapped_358e[0x3592 - 0x358e];
	/*
	 * THE THREE STATE WORDS.  `v34handshak` is not one state machine but
	 * three concurrent ones, and these are their state variables:
	 * finding F171 read each of `v34handshakinit`'s thirteen format
	 * strings against its arguments and settled which offset is which.
	 * v34hshak.h holds the eighty-seven state names they take, and
	 * finding F213 the three dispatch tables they drive.
	 *
	 * 728 ACCESSES BETWEEN THEM -- 201, 187 and 340 -- AND EVERY ONE IS
	 * SIXTEEN BITS WIDE.  There is no byte reader and no 32-bit reader
	 * at any of the three offsets, and none at an odd displacement
	 * inside them.  Nine functions besides `v34handshak` reach them.
	 *
	 * SIGNED, AND FORCED.  0x600e6 and 0x6012e load `txstate` and
	 * `microstate` with `movswl` and index `StateName` -- the table of
	 * eighty-seven string pointers at .data+0x6c00 -- with the
	 * sign-extended result, `mov 0x6c00(,%ebp,4),%ecx`.  A load whose
	 * 32-bit result indexes a table is precisely the case CLAUDE.md says
	 * to act on, and it says `short` and not `unsigned short`.
	 *
	 * THE MICROSTATE DISPATCH AGREES, AND IT IS WORTH READING BECAUSE IT
	 * LOOKS LIKE IT DOES NOT.  0x64abc loads `microstate` with `movzwl`
	 * -- which is what the frame carries as its 16-bit copy -- and the
	 * dispatch then RE-EXTENDS the halfword before using it:
	 *
	 *      64abc  movzwl 0x3592(%eax),%esi
	 *      64ac3  movswl %si,%eax
	 *      64ac6  sub    $0x29,%eax
	 *      64acc  ja     65329
	 *      64ad2  jmp    *0x3000(,%eax,4)          <== .rodata
	 *
	 * So the value that indexes the microstate table is sign-extended
	 * here too, by a separate instruction.  A `movzwl` at an offset is
	 * not by itself evidence of an unsigned field: what matters is the
	 * extension on the value that is USED 32 bits wide, and both of the
	 * two table indexes in this object are `movswl`.  The remaining
	 * zero-extending loads feed 16-bit compares and 16-bit stores, where
	 * the extension is the compiler's free choice (finding F614).
	 *
	 * THE USE SITES KEEP THE OFFSET SPELLING, deliberately, and this is
	 * not a half-done rename.  `hs_get`, `hs_put` and `hs_setstate` take
	 * the offset as a RUNTIME argument, because one function serving all
	 * three machines is the whole point of them -- the two context
	 * arguments the three format strings take are in an order that
	 * differs per string, and a second copy is a second place to get
	 * that order wrong.  There is no field for a field access to name.
	 * So v34hshak.h's `V34HS_MICROSTATE_OFF`, `_RXSTATE_OFF` and
	 * `_TXSTATE_OFF` remain what every one of the 59 callers passes, and
	 * v34hshak.c holds them against these three fields at compile time.
	 * Finding F632.
	 */
	short microstate;				/* +0x3592 */
	short rxstate;					/* +0x3594 */
	short txstate;					/* +0x3596 */
	unsigned char unmapped_3598[0x359c - 0x3598];
	/*
	 * 0x65 here selects setTimingStateParameters' second parameter
	 * table.  The two differ only in states 5, 6 and 7 -- the fast part
	 * of the acquisition ramp -- so this is a variant tuning rather than
	 * a different algorithm.
	 */
	short f359c;					/* +0x359c */
	unsigned char unmapped_359e[0x35a4 - 0x359e];
	/*
	 * A short `VPcmV34Create` clears and three functions read, always
	 * with `movswl`, so it is signed.  `V34SetINFO1aBits` sends the low
	 * seven bits of it, bit-reversed, as the leading field of an INFO1a;
	 * `VPcmV34InitiateRetrain` and `V34XF_IndicateK56FlexJdReceived` both
	 * compute `10000 + 336 * n` from it and store the result elsewhere.
	 *
	 * OFFSET-NAMED ON PURPOSE.  That arithmetic looks like a table index
	 * turning into a rate or a frequency, and it is deliberately not
	 * written down as one: neither of the two functions that does it is
	 * reconstructed, they write through different base registers, and
	 * nothing establishes that their destinations are the same field.
	 */
	short f35a4;					/* +0x35a4 */
	unsigned char unmapped_35a6[0x35a8 - 0x35a6];
	/*
	 * The bulk-delay ring feeding the second echo canceller.  Its wrap is
	 * BRANCHLESS -- idx &= -(len > idx), resetting to zero rather than
	 * subtracting -- unlike every other ring here.  Finding F116.
	 */
	int bulk_head;					/* +0x35a8 */
	int bulk_tail;					/* +0x35ac */
	short *bulk_ring;				/* +0x35b0 */
	int bulk_len;					/* +0x35b4 */
	unsigned char unmapped_35b8[0x80b8 - 0x35b8];
	/*
	 * The two echo cancellers and the arrays they point at, one
	 * contiguous block each (finding F98).  Declared here rather than in
	 * v34filt.h because this is the struct that owns the storage; the
	 * descriptors are `struct v34_echo` and the arrays are named
	 * separately because the object's own pointers are what tie them
	 * together, not adjacency.
	 *
	 * `echo0.coeff_frac` points at `echo0_frac`, and that array is ALSO
	 * DPSK.c's FSK delay line -- see finding F100 and the note on
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
	unsigned char unmapped_a238[0xa23c - 0xa238];
	/*
	 * Non-zero switches the FAR echo canceller on: modem_serrint filters
	 * and adapts echo1 only when this is set, and adaptecho never looks
	 * at it because it only ever drives the near one.
	 */
	short fa23c;					/* +0xa23c */
	/*
	 * adaptecho reads this, adds it to the residual, and clears it -- so
	 * it is a one-shot correction somebody upstream deposits.  Whoever
	 * writes it has not been reconstructed yet.
	 */
	short fa23e;					/* +0xa23e */
	/* A leaky estimate of the residual's energy, updated per symbol. */
	short fa240;					/* +0xa240 */
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
	 * Samples since the last measurement, advanced FOUR at a time and
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
	 * +0xa320.  The LINE PROBE's own DFT bank, twenty-five bins to match
	 * the twenty-five doubles above, and the second bank in this object
	 * after the retrain detector's three.
	 *
	 * The count is not inferred from the array above it: `v34handshak`
	 * loads `obj + 0xa320` and passes it to `dftupdate` with `nbins` in
	 * a register set to `$0x19` at 0x686ca, and to `dftenergy` at
	 * 0x67da3.  Twenty-five bins one unit apart is 150 Hz to 3750 Hz at
	 * the 9600 Hz rate V.34 runs at, which is the probe's tone spacing.
	 *
	 * `probeselect` reads `energy` and `shift` out of every one of them
	 * and `chkForceBaudRate` writes `shift` on six, so the two fields
	 * `dftenergy` produces are the whole interface -- nothing outside
	 * DFTC.c touches the accumulators.
	 */
	struct v34_dftbin probe_bins[V34_PROBE_BINS];	/* +0xa320 */
	/*
	 * +0xa76c.  The NOISE half of the nonlinear-distortion measurement,
	 * four bins at 900, 1200, 1800 and 2400 Hz.
	 *
	 * NAMED FROM ITS CALLER, which is rxstate 72's arm and nothing else:
	 * 0x6a5a1 and 0x6a7aa run `dftnlinitNoiseBins`' body on `obj +
	 * 0xa76c` while 0x6a54c and 0x6a760 run `dftnlinitSignalBins`' on
	 * `obj + 0xa320`, so the signal bank OVERLAYS `probe_bins[0..3]` and
	 * the noise bank is the four bins immediately after the probe's
	 * twenty-five.  0xa320 + 25 * 0x2c = 0xa76c exactly, and 0xa76c +
	 * 4 * 0x2c = 0xa81c exactly, so the region this replaces held four
	 * bins and nothing else -- it was `unmapped_a76c` until this arm gave
	 * it a reader.  v34hshak.h's note that "neither initialiser has a
	 * caller" was true when it was written; findings F738-745.
	 *
	 * The same arm reads both banks' `energy` across, at +0xa32c and
	 * +0xa778, which is what makes 0xa320 the numerator of the ratio and
	 * this the denominator.
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
	 * +0xa8a4.  The received INFO0 message, ONE BIT PER INT.
	 * `V34GiveINFO0dBits` unpacks it there MSB-first and every later
	 * reader indexes it as a bit vector; `V34XF_GetInfo0BitsPtr` hands
	 * out its address.  41 is what that unpacking writes -- 12 constant,
	 * then 8+8+8 from three message bytes and 5 from the fourth.
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
	 * are actually available, bit n-1 for rate n, and its SIGN BIT means
	 * asymmetric rates are on the table.  `info_caps` holds two more
	 * nibbles, at 6 and 10, which are bit-REVERSED before use.
	 * `caps_flags` bit 0 is the other half of the asymmetric permission.
	 */
	short info_rates;				/* +0xaa0c */
	short rate_mask;				/* +0xaa0e */
	unsigned char unmapped_aa10[0xaa3c - 0xaa10];
	short info_caps;				/* +0xaa3c */
	short caps_flags;				/* +0xaa3e */
	unsigned char unmapped_aa40[0xaa6c - 0xaa40];
	/*
	 * +0xaa6c and +0xaa70.  TWO POINTERS, and pointers rather than ints:
	 * fifty-eight accesses between them and every one is a 32-bit `mov`,
	 * the stores put an ADDRESS INSIDE THIS OBJECT in them, and the
	 * loads are dereferenced.  0x9354 in `getMPrecvdBits` stores
	 * `obj + 0xaa3c` into +0xaa6c; `v34handshakinit` aims +0xaa6c at
	 * `obj + 0xa94c` and +0xaa70 at `obj + 0xa97c`; `v34handshak` moves
	 * +0xaa6c on to `obj + 0xa9ac` and reads the record back through it.
	 * 0x62bbc loads +0xaa6c and immediately reads +0x18, +0x1a and +0x28
	 * off it -- `nbits`, `pos` and `avail` of `struct v34_bitsource`.
	 *
	 * `void *` AND NOT `struct v34_bitsource *`, which is the
	 * under-claim `p3548` at +0x3548 already sets the precedent for.
	 * The thirty-six sites that hand +0xaa6c to `getbit` are strong
	 * evidence for that type, but the region it is aimed at has two
	 * readings that this tree holds equally -- the five message records
	 * from +0xa94c on a 0x30 stride, and `info_caps`/`caps_flags` at
	 * +0xaa3c read straight out of the same words (see the note on
	 * `struct v34_bitsource` in v34hshak.h) -- and v34hshak.c's use
	 * sites cast to `unsigned short *`, `unsigned char *` and `short *`
	 * at different arms.  A pointer type here would pick a winner the
	 * object does not.  Finding F634.
	 */
	void *paa6c;					/* +0xaa6c */
	void *paa70;					/* +0xaa70 */
	/* Cleared by preinitdigital; nothing reconstructed reads it. */
	int faa74;					/* +0xaa74 */
	/*
	 * +0xaa78.  SIGNED short, and both halves of that are forced.
	 *
	 * 243 accesses, all sixteen bits wide -- the busiest field in the
	 * handshake after the three state words, and the one six of
	 * `v34handshak`'s microstate arms bump.  It is the `[2]` every state
	 * transition prints (v34hshak.c's `HS_TRACE_2`), which is what
	 * settles the sign: 0x60118 loads it `movswl` straight into a
	 * `dsplibs_debug_printf` argument slot, and the varargs promotion of
	 * an `unsigned short` would have been `movzwl`.  The `movzwl` loads
	 * are the increment-and-compare sites (0x65c4e: load, `inc`, `cmp
	 * $0x2a,%dx`, store back), where the upper half never survives.
	 *
	 * `faa78` and not `counter`: what it COUNTS differs per arm -- ticks
	 * in one, symbols in another -- and a name that says "counter" would
	 * read as measured when only the width and the sign are.
	 * v34hshak.c knows it as `T3M_COUNTER` / `T3C_COUNT`.  Finding F633.
	 */
	short faa78;					/* +0xaa78 */
	unsigned char unmapped_aa7a[0xaa7c - 0xaa7a];
	/*
	 * +0xaa7c.  THE NAME IS THE OBJECT'S OWN.  0x709d7 prints "On
	 * RX_PHASE1_ANS: is short=%d, bulkDelay=%d, filtDelay=%d" and this
	 * is the third thing it pushes -- 0x709af, `movswl 0xaa7c(%edi)`.
	 *
	 * SIGNED short: three of its twenty-one accesses are `movswl` and
	 * all three push a `%d` argument, which is the varargs promotion of
	 * a `short`.  The other eighteen are halfword loads, halfword stores
	 * and `cmpw`; nothing reads it wider or narrower.  Three of
	 * microstate 49's four thresholds and all of 50's are this plus a
	 * constant, and `VPcmV34SetDelays`, `VPcmV34InitiateRetrain` and
	 * `VPcmV34Create` are its other writers.  Finding F633.
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
	 * decoderv34 compares f124 against this and against half of it, and
	 * sets f218 accordingly -- so it is a frame length in symbols and the
	 * two tests are "half way" and "at the end".
	 */
	/*
	 * THE SYMBOL RATE IN BAUD, not an index and not a block count.
	 * `V34SetupDemodulator`'s caller compares it against 0xd65, 0xc80,
	 * 0xbb8 and 0xaf0 -- 3429, 3200, 3000 and 2800 -- so it holds the
	 * rate itself.  Written once, from T3M_TXBAUD.
	 *
	 * THAT MAKES EVERY THRESHOLD BUILT ON IT A TIME.  Multiplying by
	 * the baud converts seconds into symbols, which is what the
	 * recovery tests in `datapumpv34` are really expressing:
	 *
	 *     baud_rate >> 1   0.5 s of bad blocks -> FULL RETRAIN (~10 s)
	 *     2 * baud_rate    2 s                 -> renegotiate down
	 *     7 * baud_rate    7 s                 -> handshake timeout
	 *     8 * baud_rate    8 s of good blocks  -> renegotiate up
	 *
	 * Reading them as times is what shows the ladder is inverted: half
	 * a second of trouble buys a ten-second retrain, while the two-to-
	 * three-second renegotiation waits four times as long and never
	 * arrives.  Finding F1933.
	 */
	short baud_rate;				/* +0xaa96 */
	unsigned char unmapped_aa98[0xaad0 - 0xaa98];
	struct v34_fsk fsk;				/* +0xaad0 */
	short fsk_interp[V34_FSK_TAPS + 1];		/* +0xaae6 */
	short fsk_lpf[V34_FSK_LPF_TAPS];		/* +0xab00 */
	unsigned char unmapped_aba0[0xabae - 0xaba0];
	/*
	 * +0xabae.  TEN SHORTS, AND THE ARRAY IS MEASURED RATHER THAN
	 * INFERRED FROM ADJACENCY.  0x6cc5e is `mov %dx,0xabae(%ecx,%eax,2)`
	 * with `%eax` running 0..9 (`inc`, `cwtl`, `cmp $0x9,%ax`, `jle`) --
	 * an indexed halfword store over the span, which is what makes this
	 * an array and not ten fields.  Nothing reaches +0xabb0..+0xabc0 by
	 * a constant displacement anywhere in the object, which is the shape
	 * an array reached only by index has.
	 *
	 * +0xabc2 IS DECLARED SEPARATELY because the object writes it
	 * separately: 0x6cc72 stores it with its own instruction after the
	 * loop has ended.  `short fabae[11]` with the loop stopping one
	 * short would compile to the same code, so eleven is a reading the
	 * object does not force; ten plus one is what it shows.
	 *
	 * Both are sixteen bits wide at every access and neither is ever
	 * sign-extended, so the signedness is not settled -- `short` here is
	 * the struct's convention, not a measurement.  v34hshak.c knows them
	 * as `T3M_FABAE` and `T3M_FABC2`.  Finding F635.
	 */
	short fabae[10];				/* +0xabae */
	short fabc2;					/* +0xabc2 */
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
	short fabce;					/* +0xabce */
	short fabd0;					/* +0xabd0 */
	short fabd2;					/* +0xabd2 */
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
	 * +0xabe0.  Added to 0x50 to make the MHack message's first short,
	 * so it is what the acknowledgement CARRIES rather than a flag.
	 *
	 * AND THE OBJECT NAMES IT.  `VPcmV34InterpretMohMessageBits` takes
	 * the low nibble of an arriving MHack and puts it here, saying
	 * "MHack message detected ! , Time out period code = %d" -- so it is
	 * a hold-time code, and the field carries it in both directions: the
	 * one we received and the one we will send back.
	 */
	short fabe0;					/* +0xabe0 */
	/*
	 * +0xabe2.  Set to 3 by exactly one arriving message -- the 0x75
	 * MHnack that says the far end may NOT initiate MOH later -- and
	 * read by nothing this tree has reconstructed.  The 0x77 MHnack,
	 * which says it may, does not write it.
	 *
	 * AND SET TO 1 BY ONE PATH THAT IS NOT A MESSAGE AT ALL: microstate
	 * 80's disconnect at 0x6c8f8, beside +0xabe4, when the far end never
	 * sent its MH sequence under MHfrr.  Still read by nothing here.
	 */
	short fabe2;					/* +0xabe2 */
	unsigned char unmapped_abe4[0xabec - 0xabe4];
	/*
	 * +0xabec.  READ THIRTY-TWO BITS WIDE -- `cmpl $0x1,0xabec(%esi)` at
	 * 0x66dac -- which is what makes it an `int` and not two more
	 * halfwords of the region in front of it.
	 *
	 * AND THE OBJECT NAMES IT AGAINST `moh_message` FOUR BYTES ON.  81's
	 * wrap prints the pair together -- "V34F MOH: After 192 silence, org
	 * = %d , act = %d", this field as `org` and `moh_message` as `act`
	 * -- and then goes to MOH_FRR rather than MOH_ON_HOLD when EITHER of
	 * them is 1.  So it is the Modem-on-Hold message this end ORIGINALLY
	 * asked for against the one it is building now, in `moh_message`'s
	 * numbering where 1 is MHfrr.
	 *
	 * Nothing else this tree has reconstructed reads or writes it.
	 */
	int fabec;					/* +0xabec */
	/*
	 * +0xabf0.  Which Modem-on-Hold message to build, 0..5, and the six
	 * are named by the object's own strings: 0 MHreq, 1 MHfrr, 2 MHclrd,
	 * 3 MHcda, 4 MHack, 5 MHnack.  Anything above 5 builds nothing --
	 * the test is unsigned, so a negative value falls there too.
	 */
	int moh_message;				/* +0xabf0 */
	/*
	 * +0xabf4.  The message that ARRIVED, in the same six names but NOT
	 * the same numbering as `moh_message` four bytes back:
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
	unsigned char unmapped_abf8[0xabfa - 0xabf8];
	/*
	 * +0xabfa.  A byte that picks between three MHclrd codes -- 0x95,
	 * 0x96 and 0x9a for values 0, 1 and anything else -- and the same
	 * byte the arriving MHcld's low nibble is decoded into: 5 incoming,
	 * 6 outgoing, 0xa other, anything else "reserved (assume other)".
	 * So 0 is incoming, 1 outgoing and 2 other, and the sending side's
	 * three codes line up with the three the receiver recognises.
	 */
	unsigned char fabfa;				/* +0xabfa */
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
	short fac0c;					/* +0xac0c */
	/*
	 * The two rate-renegotiation counters, and the only fields here whose
	 * names come from a whole function rather than from a string:
	 * `VPcmV34IndicateLocalRRN` is nothing but the increment of the first
	 * and `VPcmV34IndicateRemoteRRN` nothing but the increment of the
	 * second.  `VPcmV34InitiateRateRenegotiation` bumps `rrn_local`
	 * inline, which is the same event counted at its source.
	 *
	 * Both wrap at 16 bits and `VPcmV34GetDiagnostics` reads `rrn_local`
	 * back SIGNED, so a session past 32,767 local renegotiations reports
	 * a negative count.  Not entered as a deviation: it takes a run no
	 * real call would reach.
	 */
	short rrn_local;				/* +0xac0e */
	short rrn_remote;				/* +0xac10 */
	/*
	 * +0xac12, +0xac14.  TWO SIGNED SHORTS, and the width and the sign
	 * are both forced: `VPcmV34GetDiagnostics` reads each with a single
	 * `movswl` into a 32-bit slot of `TAG_DiagnosticResults`, which is
	 * the promotion of a `short` and not of an `unsigned short`.  The
	 * other two accesses agree on the width -- `VPcmV34Create` clears
	 * both with `mov %reg16`, and `v34handshakinit` reads each back with
	 * `movzwl` and stores a halfword, where the upper half never
	 * survives (CLAUDE.md's free column, finding F614).
	 *
	 * OFFSET-NAMED.  No format string in the object prints either, no
	 * reconstructed function does arithmetic on them, and the one
	 * consumer copies them out unchanged -- +0xac12 into three
	 * diagnostics offsets at once and +0xac14 into a fourth.  They were
	 * `unmapped_ac12[4]`.
	 */
	short short_ac12;				/* +0xac12 */
	short short_ac14;				/* +0xac14 */
	/*
	 * +0xac16.  A BYTE, and past where this struct used to end: the
	 * declared length of 0xac10 was the largest offset anything
	 * reconstructed had touched, not a bound the object proves.
	 * initdigital writes here.
	 */
	unsigned char rates_latched;			/* +0xac16 */
	/*
	 * +0xac17.  A byte, and `VPcmV34SetIndicationOfRemoteRetrain` is the
	 * whole of what writes it -- twelve bytes that load the object and
	 * store 1 here.  Named from that function and from nothing else: no
	 * reader has been reconstructed, so what consumes the flag is not
	 * established, only that this one entry point raises it and never
	 * lowers it.  Was `unmapped_ac17`.
	 */
	unsigned char remote_retrain_ind;		/* +0xac17 */
	/*
	 * +0xac18.  A second pointer into the C++ side, distinct from
	 * `p3548`, and `V34GiveINFO1aBits` reads exactly one thing through
	 * it: an int at +0xc, printed as the LOCAL PCM type in "K56Flex
	 * enabled by remote, PCM type: local %d, remote %d (A=1, Mu=0)".
	 *
	 * It is also what says this struct's 0xac10 was never the object's
	 * size.  The bound is now 0xac40, and still a bound.
	 */
	void *pac18;					/* +0xac18 */
	unsigned char unmapped_ac1c[0xac3c - 0xac1c];
	/*
	 * +0xac3c.  A THIRD pointer into the C++ side, and the busiest of the
	 * three: forty-odd loads of it between .text+0x6200 and +0x7c00, all
	 * inside `VPcmV34Main.cpp`'s C exports.  Where `p3548` is the session
	 * and `pac18` is the PCM type, this one is the negotiated
	 * CONFIGURATION -- everything read through it is a limit somebody
	 * asked for:
	 *
	 *   +0x3c  int    the maximum upstream rate, in bits per second;
	 *                 `VPcmV34GetMaxUpstreamRateIndex` returns it
	 *   +0x44  short  a transmit power reduction in dB, clamped to
	 *                 [-10, +7] by `GetVPcmMinimalTxPowerReduction`
	 *   +0x50  byte   bits 5..7 are the maximum V.34 BAUD RATE INDEX,
	 *                 which is what `chkForceBaudRate` reads and its own
	 *                 trace names: "max V34 baud rate index = %d".
	 *                 BIT 2 IS A SECOND AND UNRELATED READER:
	 *                 `v90Phase34` tests it once the Ja sequence has
	 *                 finished, and its diagnostic names it -- "tx
	 *                 buffer backward clear is enabled".  It gates the
	 *                 byte at +0xabfe
	 *   +0x54  int    compared against 4 by GetVPcmMinimalTxPowerReduction
	 *
	 * Reconstructed as a `void *` with each field spelled out at its use,
	 * for the reason `p3548`'s note gives: a struct here would be a
	 * guess, and the object never states one.
	 */
	void *pac3c;					/* +0xac3c */
	/*
	 * +0xac40 to +0xac4c.  THE OBJECT IS 0xac4c BYTES AND THIS STRUCT
	 * USED TO STOP AT 0xac40, twelve bytes short of it: `vpcm.h`'s
	 * `VPCM_V34_BYTES` is 0xac4c and `VPcmV34Create` memsets exactly that
	 * much (0xaa7f-0xab15).  Nothing had reached past +0xac3c until
	 * `VPcmV34Progress`, whose `requestOutputSampleClear` arm writes all
	 * three words at .text+0xc98e, +0xc99f and +0xc9a7: a sample count, a
	 * flag set to 1 beside it, and a third word cleared to 0.
	 *
	 * THE READER HAS ARRIVED AND THEY ARE NAMED NOW.
	 * `VPcmV34RequestDPNotification` is the other end of exactly that arm:
	 * it hands the three words out through three `int *`, in this order,
	 * and then resets them to -1, 0 and 0 -- and it clears bit 0 of the
	 * configuration's +0x51, which is the same bit
	 * `requestOutputSampleClear` sets beside the three stores.  So the
	 * group is a one-deep request mailbox between the datapump and its
	 * caller, +0xac40 says whether one is outstanding and NEGATIVE means
	 * none (`test; js` and a reset to -1, against the writer's 1).
	 *
	 * `clr_count` is the writer's `n * 2`, in samples.  `clr_done` is
	 * written zero by both sides and read by neither, so the name says
	 * where it sits in the triple and not what it carries.
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
 * `rx_baud` IS `baud_rate` above -- one store, two readings, the same situation
 * as the echo array that is also the FSK delay line (finding F100).  It is
 * declared in both places on purpose; there is no third field.
 *
 * THE FOUR "CURRENT" GETTERS NAME FOUR OF THESE, and they name them in
 * transmit/receive pairs: `VPcmV34GetCurrentTxBaudRate` reads +0x00 and
 * `...RxBaudRate` +0x12; `...TxBitRate` reads +0x04; `...TxCarrier` reads
 * +0x10 and `...RxCarrier` +0x24.  `carrier` below is that fifth reading,
 * and it is the only member of the struct whose name comes from a getter
 * rather than from a use.  The receive carrier at +0x24 is inside `pad_24`
 * and is left there: nothing reconstructed touches it.
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
	 * +0x06.  THE TRANSMIT PRE-EMPHASIS FILTER INDEX, and the object
	 * names it: `V34XF_IndicateK56FlexJdReceived` prints this record's
	 * +0x00, +0x10 and +0x06 through
	 *
	 *   "VPcmV34Main: About to setup v34 txmit, baudrate = %d,
	 *    carrier = %d, preemp = %d"
	 *
	 * in that argument order (0xa6aa, 0xa69f, 0xa694 load +0x00, +0x10,
	 * +0x06 into the first, second and third slots).  Two of the three
	 * land on `baud` and `carrier`, which were named from elsewhere, so
	 * the string is checked against known answers on both sides of the
	 * one it settles.
	 *
	 * That agrees with every use already recorded: `setfinalrate` writes
	 * it, `v34setuptxmit` and `v34handshak` read it beside `baud` and
	 * `carrier` -- the three parameters of a V.34 transmitter setup --
	 * and `V34SetINFO1aBits` clears it when it hard-codes the other two
	 * for a short phase 2.  Was `f06`.
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
	 * +0x24.  The RECEIVE carrier, and the paragraph above used to say it
	 * was "inside `pad_24` and is left there: nothing reconstructed
	 * touches it".  `VPcmV34GetCurrentRxCarrier` touches it now -- it is
	 * the field that getter returns, `movswl 0xaaa8` -- and it is the
	 * receive half of the same tx/rx pairing every other member of this
	 * struct has.
	 */
	short rx_carrier;		/* +0x24                         */
	unsigned char pad_26[0x28 - 0x26];
	const short *rx_divtab;		/* +0x28                         */
};

#define V34_RATECFG	0xaa84

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
