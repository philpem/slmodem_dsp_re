/*
 * v34recv.h -- ITU-T V.34: the receiver sub-object at V34object+0x264.
 *
 * ONE MAP, NOT FOUR.  This object was reconstructed piecemeal -- the detector
 * needed its flags word, `decision` its target and result, `agcadapt` its
 * gain state, `V34descrambler` its shift register -- and each got its own
 * partial struct.  Four maps of one object is exactly what finding F100 said
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
 *
 * NAMING PASS (F10123).  This header and `src/pump/v34/v34rx.c` carry the
 * heaviest concentration of bare `fNNN` fields in the tree, and most now
 * have well-evidenced meanings -- but a real rename changes the SYMBOL, and
 * this struct is shared with `v34hshak.c`, `v34hstx1.cpp`, `v34pcmif.c`,
 * `v34diag.cpp` and a dozen fixtures under `test/mutations` that match
 * against those files' source TEXT byte for byte.  A field renamed here
 * without also fixing every one of those breaks the build; fixing them
 * risks a silent, uncounted loss of mutation coverage exactly like findings
 * F2157/F3002.  So this pass renamed only the fields whose every reference
 * lives in `v34recv.h`, `v34rx.c`, `t_v34rx.c`, `t_v34demod.c` and
 * `test/mutations/v34rx.json` -- verified by grepping the whole tree for
 * each field, not assumed.  A field with a solid derivation but a reference
 * outside that set keeps its bare name here, with the derived name recorded
 * in its comment as `-- derived: NAME, withheld (F10123)` for whoever next
 * takes the handshake file itself.
 */

#ifndef DSPLIB_V34RECV_H
#define DSPLIB_V34RECV_H

#ifdef __cplusplus
extern "C" {
#endif

struct v34_receiver {
	unsigned char pad_000[0x120 - 0x0];
	short           vectpp_idx;      /* +0x120 receiver: the cursor into
					  `vectpp[]` during phase 3 (was f120) */
	unsigned short  flags;           /* +0x122 */
	/*
	 * +0x124.  `rxsymcnt` -- receiver's own debug string names it, in
	 * "S-S1 is detected,rxsymcnt= %d,pllcnt= %d,gain= 0x%x".  It counts
	 * received symbols, and receiver reads it as an acquisition clock:
	 * thresholds at 0x11, 0x40, 0x68, 0x132, 0x143, 0x153, 0x212, 0x332
	 * and 0x7530 each move the receiver to a different behaviour.
	 *
	 * -- derived: rxsymcnt, withheld (F10123): eleven references in
	 * `v34hshak.c` and two mutation fixtures (`v34hsrx4.json`,
	 * `v34hsrx72.json`) match this field's text verbatim.
	 */
	short           rx_blocks;            /* +0x124 */
	short           best_index;      /* +0x126 */
	/*
	 * +0x128.  `rxtiming`'s and `receiver`'s own output count -- always 4,
	 * set uniformly by `V34SetupDemodulator` whatever the baud rate.
	 * Named `out_count` and threaded through `v34hshak.c` (including its
	 * own `V34HS_OFF` offset assertion) by the handshake-cluster naming
	 * pass; kept here rather than this file's own `pulls_per_call`
	 * candidate since that work is complete and verified (F9480/F10123).
	 */
	short           out_count;            /* +0x128 */
	short           agc_pair_count;  /* +0x12a V34demodulate: how many of
					  the four samples-per-AGC-tick have
					  landed (was f12a) */
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
	short           rms_idx;         /* +0x19c the cursor into `rms_buf[]`,
					  wrapping at 36 (was f19c).  Renamed
					  despite four references in
					  `v34hshak.c`, none of them in a
					  mutation fixture (F10123). */
	/*
	 * +0x19e.  NOT "the RMS window's second scalar and nothing reads it
	 * back" -- that was the handshake-cluster pass's own first reading
	 * (applied here as `short_19e`) and it is wrong, corrected on merge:
	 * `dpskinit`/its re-arm clear it alongside rms_idx, but `v34hshak.c`'s
	 * own RX_PHASE2_CALL step (already reconstructed on that branch) reads
	 * it as a one-shot latch: the first time the phase-2 symbol counter
	 * passes 0x125f it is still zero, so the retrain tone-detector is
	 * SKIPPED and the field is set to 1; every call after that it runs the
	 * detector for real.  So it is a "have we already passed this gate
	 * once" flag, not an RMS scalar -- renamed `retrain_gate` and threaded
	 * through every one of `short_19e`'s own sites in `v34hshak.c`
	 * (F10123/F9480).
	 */
	short           retrain_gate;    /* +0x19e */
	int             trn_ref_sr;      /* +0x1a0 the TRN reference
					  generator's own scrambler register,
					  distinct from `scrambler_sr` below
					  (was f1a0) */
	unsigned        scrambler_sr;    /* +0x1a4 */
	unsigned char pad_1a8[0x1aa - 0x1a8];
	short           prev_quadrant;   /* +0x1aa decoderv34: the previous
					  * constellation index, for the
					  * differential decode (was f1aa) */
	/*
	 * +0x1ac..0x1b0.  The shared interpolator's phase, step and wrap,
	 * read by both `rxtiming` and `receiver`.  Named `phase_frac`/
	 * `phase_inc`/`phase_wrap` and threaded through `v34hshak.c`'s
	 * handshake steps and four mutation fixtures (`v34hsmst44.json`,
	 * `v34hst3mid.json`, plus the handshake source itself) by the
	 * handshake-cluster naming pass (F10123/F9480).
	 */
	short           phase_frac;      /* +0x1ac rxtiming: fractional phase */
	short           phase_inc;       /* +0x1ae   its increment */
	short           phase_wrap;      /* +0x1b0   its wrap */
	unsigned char pad_1b2[0x1b4 - 0x1b2];
	const short *   carrier;         /* +0x1b4 V34demodulate: sin then cos */
	short           mix_carrier_step;    /* +0x1b8 the down-mix carrier's
					      phase increment (was f1b8) */
	/*
	 * +0x1ba.  Half the down-mix carrier table's length -- a quarter
	 * cycle, since `carrier[i]` and `carrier[i + half_len]` are cosine and
	 * sine of the same running phase (see `carrier` above).  Named
	 * `half_len` and threaded through `v34hshak.c` (twenty-one references)
	 * and `v34hstx1.cpp` by the handshake-cluster naming pass
	 * (F10123/F9480).
	 */
	short           half_len;        /* +0x1ba */
	short           mix_carrier_phase;   /* +0x1bc the down-mix carrier's
					      running phase (was f1bc) */
	/*
	 * +0x1be.  `V34SetupDemodulator`'s unmodified symbol period, kept
	 * beside `phase_inc` which the timing loop then slews.  Named
	 * `symbol_period` and threaded through `v34hshak.c` by the
	 * handshake-cluster naming pass (F10123/F9480).
	 */
	short           symbol_period;   /* +0x1be
					 * unmodified symbol period, kept
					 * beside phase_inc which the timing loop
					 * then slews */
	/*
	 * +0x1c0.  `pllcnt`, from the same debug string as `rx_blocks`, AND
	 * the timing-recovery state machine's own state (`TimingV34`,
	 * `setTimingStateParameters`): -1 done, 1 start, 2..8 the ramp.  One
	 * field serving both roles, the same overlay pattern as the AGC
	 * freeze bit above -- kept as `pllcnt` (rank-1, its own debug string)
	 * rather than the timing-state role's weaker inferred name.
	 */
	short           pllcnt;            /* +0x1c0 */
	unsigned char pad_1c2[0x1c8 - 0x1c2];
	/*
	 * +0x1c8.  `TimingV34`'s ramp selector: 1 takes the full nine-state
	 * ramp from state 2, anything else jumps straight to state 6 and
	 * skips the slow part.  So 1 means "take the slow ramp", not "go
	 * fast" -- named for that reading rather than for its more common
	 * value.
	 */
	int             slow_ramp;       /* +0x1c8 (was f1c8) */
	short           ppm_acc;         /* +0x1cc TimingV34: the whole-part
					  correction accumulated over the
					  current ppm-reporting window (was
					  f1cc) */
	short           ppm_count;       /* +0x1ce   symbols seen in that
					  window so far (was f1ce) */
	/*
	 * +0x1d0.  `setTimingStateParameters` reports this onward via
	 * `VPcmV34LogTimingOffset` as `timing_offset * 10`; `TimingV34`
	 * computes it every `report_interval` symbols by converting the
	 * accumulated timing slip to parts per million.  Named
	 * `timing_offset` and threaded through `v34hshak.c`/`v34diag.cpp` by
	 * the handshake-cluster naming pass (F10123/F9480).
	 */
	short           timing_offset;   /* +0x1d0 */
	/*
	 * +0x1d2.  Named `baud` when rxtiminginit was the only thing seen
	 * writing it (2400, the slowest V.34 rate).  It is not:
	 * setTimingStateParameters loads it from the frame length over 8, and
	 * TimingV34 uses it as the interval between timing-offset reports.
	 * So it is a symbol count, and 2400 was a plausible-looking
	 * coincidence.
	 *
	 * -- derived: ppm_period, withheld (F10123): two references in
	 * `v34hshak.c` and three mutation fixtures.
	 */
	short           report_interval;            /* +0x1d2 */
	short           f1d4;            /* +0x1d4 */
	unsigned char pad_1d6[0x1d8 - 0x1d6];
	/*
	 * +0x1d8.  `TimingV34`'s fractional carry: the whole part of each
	 * symbol's correction goes to the interpolator step, the remainder
	 * stays here for the next symbol.
	 *
	 * -- derived: timing_frac, withheld (F10123): one reference in
	 * `v34diag.cpp` and a mutation fixture (`v34vdiag.json`) that mutates
	 * it seven times.
	 */
	int             f1d8;            /* +0x1d8 */
	unsigned char pad_1dc[0x1e0 - 0x1dc];
	int             timing_integrator; /* +0x1e0 TimingV34: the phase
					    detector's own integrator, fed
					    error * timing_i_gain in Q15
					    (was f1e0) */
	int             f1e4;            /* +0x1e4 */
	int             f1e8;            /* +0x1e8 */
	/*
	 * The two timing_out[] indices TimingV34 differences.  setInitialPhase
	 * picks them, and the pair is (1,2) or (2,1) depending on where the
	 * metric changed sign -- so the order encodes the slope's direction.
	 */
	short           timing_idx_a;    /* +0x1ec (was f1ec) */
	short           timing_idx_b;    /* +0x1ee (was f1ee) */
	short           f1f0;            /* +0x1f0 */
	short           cloop_cos;       /* +0x1f2 the carrier-recovery loop's
					  rotator, real part (was f1f2) */
	short           cloop_sin;       /* +0x1f4   imaginary part (was f1f4) */
	unsigned char pad_1f6[0x1f8 - 0x1f6];
	int             cloop_integrator; /* +0x1f8 the carrier loop's
					   integrator (was f1f8) */
	/*
	 * +0x1fc.  The phase error: the imaginary part of
	 * decision* x target, shifted up two.  receiver computes it three
	 * ways depending on which decoder ran, and the NCO at the end of the
	 * same call is its only consumer.
	 */
	int             cloop_phase_err; /* +0x1fc (was f1fc) */
	short           cloop_p_shift;   /* +0x200 shift on the direct
					  (proportional) term (was f200) */
	short           cloop_i_shift;   /* +0x202 shift on the integrated
					  term (was f202) */
	short           cloop_phase_lo;  /* +0x204 NCO phase accumulator, low
					  word (was f204) */
	short           cloop_phase_hi;  /* +0x206   wrapped high word, masked
					  to 0x1fff (was f206) */
	short           f208;            /* +0x208 rxtiming IIR state, I(-1) --
					  * AND, in `receiver`'s own separate
					  * resample loop, this call's raw
					  * equaliser output.  Two functions,
					  * two meanings, never live at once
					  * (`rxtiming` and `receiver` are
					  * called from different handshake
					  * states) but genuinely different
					  * data either way -- left bare rather
					  * than pick one name and mislead
					  * about the other (F10123). */
	short           f20a;            /* +0x20a   Q(-1), same overlay */
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
	/* The decision error, target - decision, and the predictor's input
	 * and output (rx_predict mutates both in place). */
	short           pred_err_re;     /* +0x214 (was f214) */
	short           pred_err_im;     /* +0x216 (was f216) */
	/*
	 * +0x218.  `receiver`'s own comment already names it: the
	 * equaliser's step, Q15, selected by `decoderv34` (0x2000 or
	 * 0x4000) from how far into the frame the symbol count is.  Named
	 * `equ_step` and threaded through `v34hshak.c`/`v34fsk.h` by the
	 * handshake-cluster naming pass (F10123/F9480).
	 */
	short           equ_step;        /* +0x218 */
	/*
	 * +0x21a.  `equerr` -- receiver's own debug string, "V34EQU, equerr
	 * = %d, preerr = %d".  Republished from `equerr_accum` every 1024
	 * symbols.  `v34hstx1.cpp` inlines the SNR computation over this
	 * field eleven times; both naming passes agreed on `equerr`.
	 */
	short           equerr;          /* +0x21a */
	/*
	 * +0x21c.  The 1024-symbol counter that publishes equerr/preerr and
	 * resets their accumulators.  Named `err_symcount` and threaded
	 * through `v34hstx1.cpp` by the handshake-cluster naming pass
	 * (F10123/F9480).
	 */
	short           err_symcount;    /* +0x21c the 1024-symbol counter
					  * that publishes them */
	unsigned char pad_21e[0x220 - 0x21e];
	/*
	 * Two error energies accumulated over 1024 symbols and republished as
	 * shorts when the counter wraps.  receiver's own names, from
	 * "V34EQU, equerr = %d, preerr = %d": `equerr_accum` -> equerr is the
	 * EQUALISER error and `preerr_acc` -> preerr the PREDICTOR error, so
	 * the pair says which of the two stages is failing to converge.
	 * `equerr_accum`/`preerr` named and threaded through `v34hstx1.cpp`
	 * (same blast radius as `err_symcount` above) by the handshake-cluster
	 * naming pass (F10123/F9480).
	 */
	int             equerr_accum;    /* +0x220 */
	short           preerr;          /* +0x224 */
	unsigned char pad_226[0x228 - 0x226];
	int             preerr_acc;      /* +0x228 the predictor-error
					  accumulator paired with preerr
					  (was f228) */
	unsigned char pad_22c[0x22e - 0x22c];
	short           f22e;            /* +0x22e */
	short           dwell_count;     /* +0x230 TimingV34: symbols spent in
					  the current timing state so far
					  (was f230) */
	/*
	 * setTimingStateParameters installs these three together, one triple
	 * per timing state.  timing_p_gain and timing_i_gain are the loop's
	 * two gains -- see TimingV34, which scales them by the phase error in
	 * Q11 and Q15 respectively -- and dwell_limit is how many symbols to
	 * spend in this state before advancing, or -1 for "no limit".
	 */
	short           dwell_limit;     /* +0x232 (was f232) */
	short           timing_p_gain;   /* +0x234 (was f234) */
	short           timing_i_gain;   /* +0x236 (was f236) */
	unsigned char pad_238[0x240 - 0x238];
	short           demod_i;         /* +0x240 this half-baud's demodulated
					  I (was f240) */
	short           demod_q;         /* +0x242   and Q (was f242) */
	short           demod_i_prev;    /* +0x244 the previous half-baud's I,
					  the interpolator's other endpoint
					  (was f244) */
	short           demod_q_prev;    /* +0x246   and Q (was f246) */
	int             f248;            /* +0x248 */
	int             sig_energy_acc;  /* +0x24c the received-point energy
					  accumulator paired with f248 (was
					  f24c) */
	unsigned char pad_250[0x252 - 0x250];
	short           bad_thresh;            /* +0x252 */
	short           bad_long_thresh;            /* +0x254 */
	short           good_thresh;            /* +0x256 */
	short           bad_run;            /* +0x258 */
	short           bad_long_run;            /* +0x25a */
	short           good_run;            /* +0x25c */
	short           short_25e;            /* +0x25e */
	short           baud_copy;            /* +0x260 */
	/*
	 * +0x262.  The AGC's STARTING GAIN, copied into `agc_gain` by both
	 * `dpskinit` and `setupreceiver` -- the two functions that bring a
	 * receiver up -- each of which then installs its own `agc_step`.
	 * Whoever writes it has not been reconstructed; `rxinit` does not,
	 * and puts a literal 0x200 in `agc_gain` instead.
	 *
	 * -- derived: agc_gain_init, withheld (F10123): nineteen references in
	 * `v34hshak.c`, two more each in `v34hstx1.cpp`/`v34pcmcreate.cpp`,
	 * and four mutation fixtures.
	 */
	short           agc_start_gain;            /* +0x262 */
	unsigned char pad_264[0x266 - 0x264];
	/*
	 * +0x266.  demapFrame's sub-frame counter, stepped by decoderv34.
	 * Named `subframe_idx` and threaded through `v34hshak.c` (and its
	 * mutation fixture) by the handshake-cluster naming pass
	 * (F10123/F9480).
	 */
	short           subframe_idx;    /* +0x266 demapFrame's sub-frame
					  * counter, stepped by decoderv34 */
	/*
	 * +0x268.  The equaliser output one and two symbols ago, which
	 * receiver's retrain detector differences against the current one.
	 * Kept as two pairs so the copy `eq_out_i2/q2 = eq_out_i1/q1` can be
	 * one 32-bit move, which is how the object does it.
	 */
	short           eq_out_i1;       /* +0x268 (was f268) */
	short           eq_out_q1;       /* +0x26a (was f26a) */
	short           eq_out_i2;       /* +0x26c (was f26c) */
	short           eq_out_q2;       /* +0x26e (was f26e) */
	unsigned char pad_270[0x27a - 0x270];
	/*
	 * +0x27a.  SEVEN, not twenty-one.  The earlier figure came from the
	 * pointer at +0x2a4 and assumed everything between belonged to this
	 * array; `receiver` shows it does not -- the predictor's coefficients
	 * start at +0x288, which is entry seven.
	 *
	 * Two unrelated constraints land on the same number.  Finding F123
	 * measured fourteen shorts of headroom in the receive burst before
	 * it eats rxtiming's own loop bound, and seven outputs at up to two
	 * pulls each is exactly fourteen.  So out_count <= 7 is both what fits
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
	/*
	 * +0x2a4.  modem_serrint's 60-tap receive filter's coefficients.
	 * Named `fir_coeff` and threaded through `v34hshak.c` (including its
	 * own offset assertion) by the handshake-cluster naming pass
	 * (F10123/F9480).
	 */
	const short *   fir_coeff;       /* +0x2a4 modem_serrint: the 60-tap
					  * receive filter's coefficients */
	unsigned char pad_2a8[0x798 - 0x2a8];
	/*
	 * +0x798.  A saturating up/down counter over how far the equaliser
	 * output moves between symbols, from receiver's two retrain strings.
	 * It counts UP while the signal is standing still, and passing 0x8c
	 * that way raises V34_RX_FLAG_RETRAIN.  It counts DOWN on the
	 * middling case, and being between -0x84 and -0x78 when the signal
	 * moves again raises V34_RX_FLAG_RENEG instead.
	 */
	short           rtncount;        /* +0x798 (was f798) */
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
#define V34_RX_FLAG_RENEG	0x0020	/* RRN request seen: see rtncount */
#define V34_RX_FLAG_RETRAIN	0x0040	/* retrain request seen           */
#define V34_RX_FLAG_LATE_TRN	0x0008	/* shifts rx_blocks's decoder threshold
					 * on by 0x120 symbols            */
#define V34_RX_FLAG_TRN_WATCH	0x0010	/* run the shifted-TRN2 check     */
#define V34_RX_FLAG_TRAINED	0x0100	/* set at rx_blocks > 0x68; suppresses
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
