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
	short           vectpp_cursor;            /* +0x120 receiver: the vectpp cursor */
	unsigned short  flags;           /* +0x122 */
	/*
	 * +0x124.  `rxsymcnt` -- receiver's own debug string names it, in
	 * "S-S1 is detected,rxsymcnt= %d,pllcnt= %d,gain= 0x%x".  It counts
	 * received symbols, and receiver reads it as an acquisition clock:
	 * thresholds at 0x11, 0x40, 0x68, 0x132, 0x143, 0x153, 0x212, 0x332
	 * and 0x7530 each move the receiver to a different behaviour.
	 */
	short           rx_symbol_count;            /* +0x124 */
	short           best_index;      /* +0x126 */
	short           timing_output_count;            /* +0x128 rxtiming: output count */
	short           agc_decimation_phase;            /* +0x12a V34demodulate: AGC pair phase */
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
	short           rms_window_index;            /* +0x19c its index */
	/*
	 * +0x19e.  Cleared alongside rms_window_index by `dpskinit`, which is the only
	 * thing in the object that touches it -- so it is the RMS window's
	 * second scalar and nothing yet reads it back.
	 */
	short           f19e;
	int             f1a0;            /* +0x1a0 */
	unsigned        scrambler_sr;    /* +0x1a4 */
	unsigned char pad_1a8[0x1aa - 0x1a8];
	short           previous_constellation_index;            /* +0x1aa decoderv34: the previous
					  * constellation index, for the
					  * differential decode */
	short           timing_phase;            /* +0x1ac rxtiming: fractional phase */
	short           timing_phase_increment;            /* +0x1ae   its increment */
	short           timing_phase_wrap;            /* +0x1b0   its wrap */
	unsigned char pad_1b2[0x1b4 - 0x1b2];
	const short *   carrier;         /* +0x1b4 V34demodulate: sin then cos */
	short           carrier_phase_increment;            /* +0x1b8   phase increment */
	short           carrier_quadrature_offset;            /* +0x1ba   sine/cosine table offset */
	short           carrier_phase;            /* +0x1bc   phase */
	short           f1be;            /* +0x1be V34SetupDemodulator: the
					 * unmodified symbol period, kept
					 * beside timing_phase_increment which the timing loop
					 * then slews */
	/*
	 * +0x1c0.  `pllcnt`, from the same debug string as rx_symbol_count.  receiver
	 * declines to do anything at all until it exceeds 1.
	 */
	short           timing_state;            /* +0x1c0 */
	unsigned char pad_1c2[0x1c8 - 0x1c2];
	int             f1c8;            /* +0x1c8 */
	short           f1cc;            /* +0x1cc */
	short           f1ce;            /* +0x1ce */
	short           f1d0;            /* +0x1d0 */
	/*
	 * +0x1d2.  Named `baud` when rxtiminginit was the only thing seen
	 * writing it (2400, the slowest V.34 rate).  It is not: 
	 * setTimingStateParameters loads it from the frame length over 8, and
	 * TimingV34 uses it as the interval between timing-offset reports.
	 * So it is a symbol count, and 2400 was a plausible-looking
	 * coincidence.
	 */
	short           timing_report_interval_symbols;            /* +0x1d2 */
	short           f1d4;            /* +0x1d4 */
	unsigned char pad_1d6[0x1d8 - 0x1d6];
	int             f1d8;            /* +0x1d8 */
	unsigned char pad_1dc[0x1e0 - 0x1dc];
	int             f1e0;            /* +0x1e0 */
	int             f1e4;            /* +0x1e4 */
	int             f1e8;            /* +0x1e8 */
	/*
	 * The two timing_out[] indices TimingV34 differences.  setInitialPhase
	 * picks them, and the pair is (1,2) or (2,1) depending on where the
	 * metric changed sign -- so the order encodes the slope's direction.
	 */
	short           f1ec;            /* +0x1ec */
	short           f1ee;            /* +0x1ee */
	short           f1f0;            /* +0x1f0 */
	short           f1f2;            /* +0x1f2 */
	short           f1f4;            /* +0x1f4 */
	unsigned char pad_1f6[0x1f8 - 0x1f6];
	int             carrier_loop_integrator;            /* +0x1f8 the carrier loop's
					  * integrator */
	/*
	 * +0x1fc.  The phase error: the imaginary part of
	 * decision* x target, shifted up two.  receiver computes it three
	 * ways depending on which decoder ran, and the NCO at the end of the
	 * same call is its only consumer.
	 */
	int             carrier_phase_error;            /* +0x1fc */
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
	/*
	 * +0x210.  TWO QUANTITIES, ONE PAIR OF FIELDS, WITHIN ONE CALL.
	 * receiver first writes the DEROTATED RECEIVED point here, and then,
	 * after the decoder has run, overwrites it with the DECISION rotated
	 * back up by the same carrier.  Everything between the two reads the
	 * first; the error term at the end reads the second.
	 */
	short           target_re;       /* +0x210 */
	short           target_im;       /* +0x212 */
	/* The decision error, target - decision, and the predictor's input. */
	short           predictor_work_re;            /* +0x214 */
	short           predictor_work_im;            /* +0x216 */
	short           equalizer_error_gain;            /* +0x218 scales equaliser error */
	short           equalizer_error_1024;            /* +0x21a `equerr` -- see equalizer_error_accum */
	short           error_window_symbols;            /* +0x21c the 1024-symbol counter
					  * that publishes them */
	unsigned char pad_21e[0x220 - 0x21e];
	/*
	 * Two error energies accumulated over 1024 symbols and republished as
	 * shorts when the counter wraps.  receiver's own names, from
	 * "V34EQU, equerr = %d, preerr = %d": equalizer_error_accum -> equalizer_error_1024 is the EQUALISER
	 * error and predictor_error_accum -> predictor_error_1024 the PREDICTOR error, so the pair says which
	 * of the two stages is failing to converge.
	 */
	int             equalizer_error_accum;            /* +0x220 */
	short           predictor_error_1024;            /* +0x224 `preerr` */
	unsigned char pad_226[0x228 - 0x226];
	int             predictor_error_accum;            /* +0x228 */
	unsigned char pad_22c[0x22e - 0x22c];
	short           f22e;            /* +0x22e */
	short           timing_state_dwell;            /* +0x230 */
	/*
	 * setTimingStateParameters installs these three together, one triple
	 * per timing state.  timing_proportional_gain_q11 and timing_integral_gain_q15 are the loop's two gains -- see
	 * TimingV34, which scales them by the phase error in Q15 and Q11 --
	 * and timing_state_dwell_limit is a dwell count, or -1 for "no limit".
	 */
	short           timing_state_dwell_limit;            /* +0x232 */
	short           timing_proportional_gain_q11;            /* +0x234 */
	short           timing_integral_gain_q15;            /* +0x236 */
	unsigned char pad_238[0x240 - 0x238];
	short           demod_i;            /* +0x240 demodulated I */
	short           demod_q;            /* +0x242 demodulated Q */
	short           demod_prev_i;            /* +0x244 previous I */
	short           demod_prev_q;            /* +0x246 previous Q */
	int             target_signal_power_1024;            /* +0x248 */
	int             target_signal_power_accum;            /* +0x24c */
	unsigned char pad_250[0x252 - 0x250];
	short           retrain_error_threshold;            /* +0x252 */
	short           reneg_down_error_threshold;            /* +0x254 */
	short           reneg_up_error_threshold;            /* +0x256 */
	short           retrain_bad_block_run;            /* +0x258 */
	short           reneg_down_bad_block_run;            /* +0x25a */
	short           reneg_up_good_block_run;            /* +0x25c */
	short           rate_change_reason;            /* +0x25e */
	short           rate_change_rate_index;            /* +0x260 */
	/*
	 * +0x262.  The AGC's STARTING GAIN, copied into `agc_gain` by both
	 * `dpskinit` and `setupreceiver` -- the two functions that bring a
	 * receiver up -- each of which then installs its own `agc_step`.
	 * Whoever writes it has not been reconstructed; `rxinit` does not,
	 * and puts a literal 0x200 in `agc_gain` instead.
	 */
	short           agc_reset_gain;            /* +0x262 */
	unsigned char pad_264[0x266 - 0x264];
	short           demap_subframe_index;            /* +0x266 demapFrame's sub-frame
					  * counter, stepped by decoderv34 */
	/*
	 * +0x268.  The equaliser output one and two symbols ago, which
	 * receiver's retrain detector differences against the current one.
	 * Kept as two pairs so the copy `retrain_prev2_i/retrain_prev2_q = retrain_prev_i/retrain_prev_q` can be one
	 * 32-bit move, which is how the object does it.
	 */
	short           retrain_prev_i;            /* +0x268 */
	short           retrain_prev_q;            /* +0x26a */
	short           retrain_prev2_i;            /* +0x26c */
	short           retrain_prev2_q;            /* +0x26e */
	unsigned char pad_270[0x27a - 0x270];
	/*
	 * +0x27a.  SEVEN, not twenty-one.  The earlier figure came from the
	 * pointer at +0x2a4 and assumed everything between belonged to this
	 * array; `receiver` shows it does not -- the predictor's coefficients
	 * start at +0x288, which is entry seven.
	 *
	 * Two unrelated constraints land on the same number.  Finding 123
	 * measured fourteen shorts of headroom in the receive burst before
	 * it eats rxtiming's own loop bound, and seven outputs at up to two
	 * pulls each is exactly fourteen.  So timing_output_count <= 7 is both what fits
	 * here and what the burst survives, and the fixtures assert both.
	 */
	short           timing_out[7];   /* +0x27a rxtiming's metric */
	/*
	 * The three-tap complex predictor, +0x288 to +0x2a3.
	 *
	 * `b` is the real half of its coefficients and `a` the imaginary, in
	 * the sense that the prediction is the complex product
	 * (b + ja) . (hist_i + j hist_q).  receiver runs the same engine
	 * twice per symbol -- once on the equaliser output and once on the
	 * decision error -- over ONE shared set of coefficients and ONE
	 * shared history, and only the second run adapts them.
	 *
	 * The histories are four long and only three are read: the shift
	 * writes hist[3] and nothing ever looks at it again except the LMS
	 * update, which reads taps 3..1 because it runs after the shift.
	 */
	short           pred_b[3];       /* +0x288 */
	short           pred_a[3];       /* +0x28e */
	short           pred_i[4];       /* +0x294 */
	short           pred_q[4];       /* +0x29c */
	const short *   fir60_coeffs;            /* +0x2a4 modem_serrint: the 60-tap
					  * receive filter's coefficients */
	unsigned char pad_2a8[0x798 - 0x2a8];
	/*
	 * +0x798.  `rtncount`, from receiver's two retrain strings.  A
	 * saturating up/down counter over how far the equaliser output moves
	 * between symbols: it counts UP while the signal is standing still,
	 * and passing 0x8c that way raises V34_RX_FLAG_RETRAIN.  It counts
	 * DOWN on the middling case, and being between -0x84 and -0x78 when
	 * the signal moves again raises V34_RX_FLAG_RENEG instead.
	 */
	short           retrain_reneg_motion_count;            /* +0x798 */
};

/*
 * The adaptive equaliser sits at +0x3cc, which is `struct v34_equalizer` --
 * declared in v34filt.h, which this header must not include, since the
 * dependency runs the other way.  Reach it with a cast at the one place that
 * needs it.
 */
#define V34_RX_EQ_OFFSET	0x3cc

/*
 * `flags` at +0x122.
 *
 * Bit 9 is set by the handshake when it arms a tone detector, cleared by
 * tone_detect when the level first crosses its floor -- and read by agcadapt
 * as "do not adapt yet".  One bit, three readers.
 */
#define V34_RX_FLAG_DET_PENDING	0x0200
/* Bit 11: modem_serrint runs the 60-tap FIR instead of the Hilbert pair. */
#define V34_RX_FLAG_FIR		0x0800
#define V34_RX_FLAG_AGC_FREEZE	V34_RX_FLAG_DET_PENDING

/*
 * The rest, all of them read by `receiver` and all of them set elsewhere --
 * mostly in v34handshak, which is not reconstructed.  Bit 2 is the
 * scrambler's polynomial select, already spelt V34_SCR_ANSWERER in v34rx.h;
 * both names are kept because the two readers mean different things by it.
 */
#define V34_RX_FLAG_RENEG	0x0020	/* RRN request seen: see retrain_reneg_motion_count     */
#define V34_RX_FLAG_RETRAIN	0x0040	/* retrain request seen           */
#define V34_RX_FLAG_LATE_TRN	0x0008	/* shifts rx_symbol_count's decoder threshold
					 * on by 0x120 symbols            */
#define V34_RX_FLAG_TRN_WATCH	0x0010	/* run the shifted-TRN2 check     */
#define V34_RX_FLAG_TRAINED	0x0100	/* set at rx_symbol_count > 0x68; suppresses
					 * the carrier loop's error term  */
#define V34_RX_FLAG_DATA	0x0400	/* the decoder, not the handshake
					 * slicer; and the loss-of-signal
					 * gate                           */
#define V34_RX_FLAG_PREDICT	0x1000	/* adapt the predictor on the
					 * decision error                 */
#define V34_RX_FLAG_PRECODE	0x2000	/* run the predictor on the
					 * equaliser output               */

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34RECV_H */
