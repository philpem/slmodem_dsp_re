/*
 * v8.h -- V.8 handshake.
 *
 * V.8 is the negotiation that happens once a call is answered and before any
 * datapump starts: the answering modem sends ANSam, the calling modem offers
 * a menu of modulations in a CM sequence, and the answerer picks one in JM.
 * Everything above 2400 bit/s in this library is chosen here.
 *
 * The public surface is unusually cooperative for this object file -- every
 * entry point below is a global symbol, so all of it can be driven by name
 * in a differential test, unlike the call-progress code where most of the
 * work lived in file statics.
 *
 * Reconstruction order is bottom-up, because `V8Create` reaches the signal
 * layer through `v8handshakinit` and the signal layer reaches the arithmetic
 * leaves in v8util.c.
 */

#ifndef DSPLIB_V8_H
#define DSPLIB_V8_H

/* One cycle of cosine, in 256 steps. */
#define V8_COSTAB_SIZE	256

/*
 * A bin of the handshake's DFT.  Sixteen bytes; `v8_dftenergy` reads the two
 * halves of the complex value and writes the magnitude squared beside them.
 */
struct v8_dft_bin {
	/*
	 * A phase accumulator per bin, stepped by its own increment: this is
	 * a sliding DFT, one oscillator per frequency of interest, not a
	 * transform over a block.
	 */
	short	phase;			/* +0x00 */
	short	step;			/* +0x02 */
	int	re;			/* +0x04 */
	int	im;			/* +0x08 */
	short	energy;			/* +0x0c */
	short	f0e;			/* +0x0e */
};

/*
 * The handshake state.  Only the CRC register is known so far -- the rest
 * arrives with `v8handshakinit`, which is what fills it.
 */
/*
 * What `v8_crc` is handed.  It is a `struct v8_tx_sequence` -- the CRC lives
 * at +0x1e of one -- kept as its own name only because v8_crc was
 * reconstructed before that was known.
 */
struct v8_handshake {
	unsigned char	pad[0x1e];	/* +0x00 */
	short		crc;		/* +0x1e */
};

/*
 * The handshake object.  3780 bytes, allocated by `V8Create`.  Only the two
 * regions below are mapped so far -- the initialisers reveal them, and the
 * rest is named as later functions claim it.  The padding is explicit rather
 * than implied so that every offset stays checkable against the object.
 */
#define V8_STATE_BYTES		0xec4

/*
 * The tone detector, at +0xad8.  Everything above the fixed fields is a set
 * of small accumulator arrays: two two-by-two, then two of three.
 */
struct v8_detector {
	/*
	 * A coefficient table in .rodata, not a number: v8handshakinit passes
	 * an address here.  Typed as an int until the caller was read, which
	 * a 32-bit differential test could never have caught -- both are four
	 * bytes and the value copies through either way.
	 */
	const short	*table;			/* +0x00 */
	/*
	 * Two mutually-exclusive rules choose what `counter` is counting.
	 * `lo_rule`, set only at construction (`v8_detectorinit`'s `a3`), asks
	 * for time spent BELOW `lo_thresh`; `armed`, set by this file's own
	 * warm-up (below), asks for time spent ABOVE `hi_thresh` instead --
	 * which is the ordinary "is a tone here" rule and the only one this
	 * object's single instance ever reaches (`a3` is always 0 at the one
	 * call site), but `t_v8sig.c` exercises `lo_rule` directly and calls
	 * it one of "the three rules the verdict can follow", so it is not
	 * dead in the reconstruction even though it is unused by the blob's
	 * own configuration.
	 */
	short	lo_rule;			/* +0x04 */
	short	armed;				/* +0x06 */
	/*
	 * The hysteresis run count, seeded NEGATIVE (the constructor's `a5`,
	 * negated) so the verdict cannot assert until it has run that many
	 * blocks past zero -- named "the counter" by t_v8sig.c's own comment.
	 */
	short	counter;			/* the negated argument  +0x08 */
	short	count_limit;			/* +0x0a */
	short	f0c;		/* 1                     +0x0c */
	/* Below: acc_a and acc_b are the two biquads' x and y histories. */
	short	lo_thresh;			/* +0x0e */
	short	hi_thresh;			/* +0x10 */
	/* The leaky-integrator envelope of the rectified signal -- "the
	 * integrator" in t_v8sig.c's own comment. */
	short	integrator;			/* +0x12 */
	short	acc_a[4];			/* +0x14 */
	short	acc_b[4];			/* +0x1c */
	short	acc_c[3];			/* +0x24 */
	short	acc_d[3];			/* +0x2a */
	/* Blocks the integrator has stayed above the warm-up floor, before
	 * `armed` gets set. */
	short	warmup;				/* +0x30 */
	unsigned char pad32[0x68 - 0x32];
};

/* What v8_detectorinit sets in the receiver's flag word. */
#define V8_RX_DETECTOR_ARMED	0x200

/*
 * The phase-reversal detector, at +0xb40.  ANSam is a 2100 Hz tone whose
 * phase inverts every 450 ms, and this is what watches for the inversions:
 * three accumulators, a window of 64 samples, and a countdown.
 */
struct v8_phase_rev {
	int	corr;		/* the correlation      +0x00 */
	int	energy;		/* the running energy   +0x04 */
	int	smoothed;	/* energy, smoothed     +0x08 */
	short	reversals;	/* how many seen        +0x0c */
	short	half;		/* half the window, 32  +0x0e */
	short	widx;		/* where the next sample goes  +0x10 */
	short	run;		/* samples since the last one  +0x12 */
	short	window[64];			/* +0x14 */
	unsigned char pad94[0xdc - 0x94];
	short	detected;	/* the verdict          +0xdc */
	unsigned char padde[0xe0 - 0xde];
};

/*
 * The V.21 modem's working parameters, at +0xc20.  `v8_V21_Init` sets them
 * from two independent choices: which channel is being sent (which picks the
 * carrier constants and the 61-tap filter) and whether this modem answered
 * the call (which picks the four filter designs and two more constants).
 */
struct v8_v21_params {
	/* The FSK carrier's own phase accumulator, stepped by `carrier_a` or
	 * `carrier_b` in v8_fskmodulate. */
	short	carrier_phase;			/* +0xc20 */
	short	carrier_a;	/* 0x62b or 0x3ef  +0xc22 */
	short	carrier_b;	/* 0x580 or 0x344  +0xc24 */
	/*
	 * The bit clock: `sample_count` is advanced by V8_QUEUE_BLOCK (4) per
	 * call and compared against `samples_per_bit` (0x20 -- 32 samples per
	 * V.21 bit, i.e. 300 bit/s at a 9600 Hz sample rate); when they meet,
	 * the next transmit bit is fetched and the count restarts.
	 */
	short	samples_per_bit;		/* 0x20            +0xc26 */
	short	sample_count;			/* +0xc28 */
	/* Q14 transmit amplitude scale, derived from `v8.tx_gain`. */
	short	tx_level;	/* scaled by v8_mpyint  +0xc2a */
	short	f0c;		/* 4 or 7          +0xc2c */
	short	f0e;		/* -100 or 0       +0xc2e */
	/*
	 * The bit value pushed for a run of decisions: `mark_bit` for
	 * `v21.mark_run`, `space_bit` for `v21.space_run` (v8_fskdemodulate).
	 */
	short	mark_bit;			/* +0xc30 */
	short	space_bit;	/* 1               +0xc32 */
	short	f14;		/* 0x18            +0xc34 */
	/* Index into `v21.inbuf` while it fills, one block at a time. */
	short	inbuf_pos;			/* +0xc36 */
	/* The receive character framing: `bits` is the raw shift register
	 * ("oldest first in the bottom of `bits`"), `bitcount` counts how
	 * many are in it -- both named from v8hsrx.c's own doc comment. */
	short	bitcount;			/* +0xc38 */
	short	bits;				/* +0xc3a */
	short	f1c;				/* +0xc3c */
	/* Consecutive zero bits since the last one bit, for character
	 * framing (V8_HS_ZERO_RUN ends a character). */
	short	zero_run;			/* +0xc3e */
	/*
	 * The "character of ones" idle detector: `ones_run` counts the
	 * current run of one-bits, `ones_run_len` freezes its length once a
	 * zero run starts.  When a zero run six long follows a one-run over
	 * V8_HS_MIN_ONES, `gap_count` is bumped and `gap_seen` (its previous
	 * value) is what v8_handshak_demod compares it against to notice.
	 */
	short	ones_run;			/* +0xc40 */
	short	ones_run_len;			/* +0xc42 */
	short	gap_count;			/* +0xc44 */
	short	gap_seen;			/* +0xc46 */
};

/*
 * A tone generator's working parameters.  Sixteen bytes, and the same shape
 * appears at +0xda4 with its own constants.
 */
struct v8_tone {
	/*
	 * ANSam: a 2100 Hz carrier (`carrier_phase`/`carrier_step`) whose
	 * amplitude (`amplitude`) is modulated by a slower oscillator
	 * (`envelope_phase`/`envelope_step`) and negated every
	 * `reversal_count` reaches
	 * V8_ANSAM_REVERSAL blocks -- the periodic phase reversal that is
	 * ITU-T V.8's whole reason for ANSam over a plain answer tone, and
	 * confirmed by V8_ANSAM_REVERSAL*4 samples at 9600 Hz landing on the
	 * standard's 450 ms.  `reversal_enable` gates the counter so a caller
	 * can have the tone without the reversals.
	 */
	short	envelope_phase;			/* +0x00 */
	short	carrier_phase;			/* +0x02 */
	short	envelope_step;	/* 0x1a          +0x04 */
	short	carrier_step;	/* 0xe00         +0x06 */
	short	amplitude;	/* scaled        +0x08 */
	short	reversal_count;			/* +0x0a */
	short	f0c;				/* +0x0c */
	short	reversal_enable;	/* 1             +0x0e */
};

/*
 * The transmit sequence: the CM or JM about to go on the wire, as 10-bit
 * V.21 characters -- start bit, eight data bits least significant first, stop
 * bit -- terminated by 0xffff, followed by the transmitter's control block.
 *
 * `nbits` is the character count times ten, which is what confirms the
 * entries are ten bits each rather than bytes with framing added later.
 */
#define V8_TX_SEQ_WORDS	15

struct v8_tx_sequence {
	short	word[V8_TX_SEQ_WORDS];		/* +0x00 */
	/*
	 * CRC-16-CCITT, and 0xffff is its initial value rather than a
	 * terminator: `v8_getbit` folds each bit it hands out into this as it
	 * goes, and appends the finished register to the message.  It was
	 * called a terminator for as long as only the builder had been read.
	 */
	short	crc;				/* +0x1e */
	short	crc_enable;			/* +0x20 */
	short	nbits;		/* words * 10   +0x22 */
	short	bitpos;		/* bits handed out   +0x24 */
	short	wordbits;	/* 10               +0x26 */
	short	wordidx;			/* +0x28 */
	short	repeat;		/* 1                +0x2a */
	short	repeats;			/* +0x2c */
	short	f2e;				/* +0x2e */
	int	shifter;	/* the bits being handed out  +0x30 */
	short	nleft;		/* how many are still in it   +0x34 */
	short	f36;				/* +0x36 */
	int	shifter0;	/* both restored on repeat    +0x38 */
	short	nleft0;				/* +0x3c */
	short	f3e;				/* +0x3e */
};

/*
 * The call menu itself -- the bits V.8 is actually negotiating over.  Three
 * flag bytes and two optional four-byte extensions, which is what the
 * standard calls the country code and vendor-specific fields.
 */
struct v8_cm {
	unsigned char	b0;			/* +0x00 */
	unsigned char	b1;			/* +0x01 */
	unsigned char	b2;			/* +0x02 */
	unsigned char	b3;			/* +0x03 */
	unsigned char	pad04[0x0c - 4];
	/*
	 * What the far end offered, copied straight out of the handshake by
	 * V8UpdateModemParameters.  Distinct from `menu` below, which is what
	 * the JM builder reads.
	 *
	 * V8Create's configuration trace prints this field of the LOCAL menu
	 * as "ansPcmLevel" and `menu` as "ucodeForQts" (finding F164) -- the
	 * same offsets carry different meanings depending on whose menu the
	 * struct holds, so neither name can claim the field outright.
	 */
	int		offered;		/* +0x0c  local: ansPcmLevel */
	/* The modulation list, read as one word when the JM is built. */
	int		menu;			/* +0x10  local: ucodeForQts */
	unsigned char	pad14[0x18 - 0x14];
	/*
	 * NOT country code and vendor fields, as an earlier draft guessed:
	 * the trace announces ext1 as raw CALL FUNCTION octets and ext2 as
	 * raw PROTOCOL octets ("raw CF specified", "raw Protocol specified"),
	 * and initTxSequence substitutes them for the function and protocol
	 * characters it would otherwise derive from the flag bits.
	 */
	unsigned char	ext1[4];		/* +0x18  raw call function */
	unsigned char	ext2[4];		/* +0x1c  raw protocol      */
	/*
	 * Up to eight call-function octets this end will accept.  When the
	 * received function is none of the four the flags name,
	 * rebuildJMSequence looks for it here before giving up.
	 */
	unsigned char	fn_list[8];		/* +0x20 */
	/* And eight more, checked the same way for the second field. */
	unsigned char	ext_list[8];		/* +0x28 */
};

/* The two characters every sequence opens with. */
#define V8_SEQ_PREAMBLE_0	0x3ff
#define V8_SEQ_PREAMBLE_1	0x00f

/*
 * The call-function character, chosen by the first flags that match.  These
 * are the "what do you want to do" codes -- data, fax, and so on.
 */
#define V8_SEQ_FN_DEFAULT	0x107
#define V8_SEQ_FN_B0		0x103
#define V8_SEQ_FN_B1_80		0x10b
#define V8_SEQ_FN_B2		0x109

/* The characters that close a sequence. */
#define V8_SEQ_TAIL_A		0x0a9
#define V8_SEQ_TAIL_B		0x161
#define V8_SEQ_TAIL_C		0x1c9
#define V8_SEQ_TAIL_D		0x011

/* Ten bits per character on the wire. */
#define V8_SEQ_BITS_PER_WORD	10

/* At most four characters are taken from each extension field. */
#define V8_CM_EXT_MAX	4

/* Bits of v8_cm.b2 saying whether each extension is present. */
#define V8_CM_EXT1_PRESENT	0x04
#define V8_CM_EXT2_PRESENT	0x08

/* The 61-tap filter v8_V21_Init copies in, chosen by channel. */
#define V8_V21_TAPS	61

/* Stored size of each V.21 mark/space design; the correlator uses 40 taps. */
#define V8_V21_FILTER_TAPS	41

/* What v8_V21_Init sets in the receiver's flag word. */
#define V8_RX_V21_ARMED	0x800

/*
 * The V.21 modem V.8 signals over, at +0xdd8.  `V8_setFilters` swaps the four
 * coefficient pointers -- which is how one modem serves both channels -- and
 * `V8_V21_reset` clears the delay line between uses.
 *
 * It starts at +0xdd8 rather than at the tone-queue fields just before it
 * because those are shorts and these are pointers: putting them in one struct
 * makes the compiler realign, and the offsets stop matching the object.  The
 * compile-time assertions in v8util.c caught exactly that.
 */
#define V8_V21_DELAY	40
#define V8_V21_INBUF	12

/*
 * The receive front end's filter and its running history.  The history wraps
 * at 36 entries, which is what its index is tested against.
 */
#define V8_AGC_TAPS	40
#define V8_AGC_HIST	35

struct v8_v21 {
	/* The four filter designs, swapped as a set. */
	const short	*a;			/* +0x00  obj +0xdd8 */
	const short	*b;			/* +0x04 */
	const short	*c;			/* +0x08 */
	const short	*d;			/* +0x0c */

	int	pos;		/* where the next symbol falls  +0x10 */
	int	space_run;	/* consecutive space decisions  +0x14 */
	int	mark_run;	/* consecutive mark decisions   +0x18 */

	/* The twelve samples that arrived this block, oldest first. */
	short	inbuf[V8_V21_INBUF];		/* +0x1c  obj +0xdf4 */

	/* The forty before them, which the correlator runs back into. */
	short	delay[V8_V21_DELAY];		/* +0x34  obj +0xe0c */
};

/*
 * The receiver, at +0x1c.  The original works through a base pointer held in
 * a register rather than through the object, which is what marks it out as a
 * sub-object rather than a scattering of fields.
 */
struct v8_rx {
	unsigned char	pad00[0x0a];
	/* `v8_detectorinit` sets bit 9 here; the rest is not yet known. */
	unsigned short	flags;			/* +0x0a */
	unsigned char	pad0c[4];
	short		*buf;			/* +0x10  -> v8.rx_stage  */
	/*
	 * The band-filtered energy of the four samples just scaled, as one
	 * 32-bit int split across two shorts because v8_agcadapt reads only
	 * the upper half on its own -- V8agc's own comment: "They are the two
	 * halves of one int, which is why that field is a short of its own".
	 */
	short		energy_lo;		/* +0x14 */
	short		energy_hi;		/* +0x16 */
	unsigned char	pad18[2];
	/* The smoothed level estimate v8_agcadapt feeds back into itself. */
	short		level;			/* +0x1a */
	/* The AGC gain -- "gain" in t_v8sig.c's own diff labels. */
	short		gain;		/* 0x200     +0x1c */
	/* The carried adaptation accumulator between v8_agcadapt calls. */
	short		accum;			/* +0x1e */
	/* Q14 weight applied to `delta` when accumulating `accum`; 0x3333 at
	 * reset, 0x800 once a tone has been accepted. */
	short		adapt_rate;	/* 0x3333    +0x20 */
	short		hist[48];		/* +0x22 */
	/* Write cursor into `hist`, wrapping at V8_AGC_HIST. */
	short		hist_idx;		/* +0x82 */
	/* checkSignalStability's own pair: `refresh_timer` counts up to
	 * V8_STABLE_PERIOD before `gain_ref` is refreshed from `gain`, and
	 * `stable_timer` counts blocks since the gain last moved more than
	 * V8_STABLE_TOLERANCE away from it. */
	short		refresh_timer;		/* +0x84 */
	short		gain_ref;	/* 0x200     +0x86 */
	short		stable_timer;		/* +0x88 */
	/* The verdict itself -- "stable" in t_v8sig.c's own diff labels. */
	short		stable;			/* +0x8a */
	unsigned char	pad8c[0xac - 0x8c];
	/* Consecutive saturating blocks -- "clip count" in t_v8sig.c's own
	 * diff labels -- forcing the gain back down at V8_AGC_CLIP_LIMIT. */
	short		clip_count;		/* +0xac */
	unsigned char	padae[0xc2 - 0xae];
	short		fc2;		/* 0x50      +0xc2 */
	unsigned char	padc4[2];
	short		fc6;			/* +0xc6 */
	short		fc8;			/* +0xc8 */
	unsigned char	padca[0xd8 - 0xca];
	short		fd8;			/* +0xd8 */
	short		fda;			/* +0xda */
	unsigned char	paddc[0xdc - 0xdc];
};

/* Sizes of the buffers the transmitter and receiver clear. */
#define V8_TX_SYMBOLS	128		/* +0x11c */
#define V8_TX_RING	460		/* +0x228 */
#define V8_TX_SHAPE	140		/* +0x77c */
#define V8_RX_SCRATCH	160		/* +0x894 */
#define V8_RX_HIST	48		/* +0x22 of the receiver */
#define V8_TX_RING_HALF	64		/* where the write cursor starts */

/*
 * The receiver's scratch buffer carries one value that is not zero: element
 * 40 is set to 0x1000 straight after the clear, so the initialiser writes
 * over what it has just written.  Reproduced in that order.
 */
#define V8_RX_SCRATCH_SEED_INDEX	40
#define V8_RX_SCRATCH_SEED		0x1000

struct v8 {
	unsigned char		pad000[4];
	/*
	 * Four fields `v8_txinit` sets, in write order 1/0/0/0 for
	 * `short_014`/`short_00c`/`short_018`/`int_004` -- and nothing else
	 * in this reconstruction ever reads back.  F10135 already checked
	 * and declined a real name here for exactly that reason; re-checked
	 * with a fresh whole-tree grep for this wave and still no reader.
	 * Promoted from bare `fNNNN` to their shaped names only.
	 */
	int			int_004;	/* +0x004 */
	unsigned char		pad008[4];
	short			short_00c;	/* +0x00c */
	unsigned char		pad00e[6];
	short			short_014;	/* 1        +0x014 */
	unsigned char		pad016[2];
	short			short_018;	/* +0x018 */
	unsigned char		pad01a[2];

	struct v8_rx		rx;		/* +0x01c */

	unsigned char		pad0f8[0x110 - 0xf8];
	/* Samples currently buffered in `tx_symbols`, awaiting the receiver:
	 * V8Process advances it per sample, v8_rxreadqueue drains it by
	 * V8_QUEUE_BLOCK, and v8handshak will not run the receiver below 6. */
	short			sym_avail;	/* +0x110 */
	unsigned char		pad112[2];
	short			*tx_sym_a;	/* +0x114 -> tx_symbols */
	short			*tx_sym_b;	/* +0x118 -> tx_symbols */
	short			tx_symbols[V8_TX_SYMBOLS];	/* +0x11c */
	/* Samples currently queued in `tx_ring`, awaiting output; goes
	 * negative (V8Process decrements it once a sample regardless), which
	 * is why every comparison against it is signed. */
	short			tx_avail;	/* 0x20     +0x21c */
	unsigned char		pad21e[2];
	short			*tx_ring_base;	/* +0x220 -> tx_ring[0]  */
	short			*tx_ring_half;	/* +0x224 -> tx_ring[64] */
	short			tx_ring[V8_TX_RING];		/* +0x228 */
	/*
	 * Two four-sample staging buffers.  `v8_txwritequeue` copies out of
	 * the first into the transmit ring; `v8_rxreadqueue` copies into the
	 * second out of the symbol buffer.  Four samples at a time is the
	 * handshake's block.
	 */
	short			tx_stage[4];	/* +0x5c0 */
	short			rx_stage[(0x77c - 0x5c8) / 2];	/* +0x5c8 */
	short			tx_shape[V8_TX_SHAPE];		/* +0x77c */
	short			rx_scratch[V8_RX_SCRATCH];	/* +0x894 */

	unsigned char		pad9d4[0x9d4 - 0x9d4];
	/*
	 * The two state variables, one per direction -- v8handshak.c's own
	 * header comment: "`tx_state` drives the transmitter and is
	 * dispatched inside a loop that runs until the transmit queue is
	 * full... `rx_state` drives the receiver and is dispatched once".
	 * `rx_substate` is "a sub-state below the receive state" (v8hsrx.c's
	 * own header comment) that only matters while `rx_state` is
	 * V8_RX_DEMOD, picking what the character stream is matched against.
	 */
	short			tx_state;	/* +0x9d4 */
	short			rx_state;	/* +0x9d6 */
	short			rx_substate;	/* +0x9d8 */
	unsigned char		pad9da[0xa3c - 0x9da];
	/* The bit currently going out, as v8_fskmodulate wants it. */
	short			tx_bit;		/* +0xa3c */

	/* The transmit ring's low-water mark: v8handshak's loop tops it up
	 * to this before running the receiver. */
	short			tx_fill_target;	/* 0x10     +0xa3e */
	/*
	 * Snapshots `rx.gain` when a QCA1-collected message completes
	 * (v8_hs_message_done); nothing reads it back anywhere in this
	 * reconstruction.  Promoted to its shaped name only -- F10135 already
	 * declined a real name here for the same reason, re-checked here with
	 * no new evidence.
	 */
	short			short_a40;	/* 0x200    +0xa40 */
	/* Q14 master transmit-level scale (unity, 0x4000, by default),
	 * multiplied into both the V.21 and ANSam amplitude derivations. */
	short			tx_gain;	/* +0xa42 */

	/*
	 * The configuration V8Create plants, which v8handshakinit reads back.
	 * `mode` picks between three whole shapes of handshake and is the
	 * first thing looked at; anything but 0 or 1 makes the function
	 * return having done only the common preamble.
	 *
	 * V8Create's configuration trace names four of these (finding F164):
	 * `side` ("Caller"/"Answer" -- 0 originates) and `op_mode` (the
	 * "Operation Mode") were `mode` and `fa48` before it named them, and
	 * the two timeouts are the signal-detect and message-detect timeouts,
	 * in SECONDS.  The trace does not mention `rate`, but v8dp.c's own
	 * caller sets `v8_cfg.rate` to `V8_DP_RATE` -- it is the datapump's
	 * sample rate, carried through and otherwise unread by this file.
	 */
	int			side;		/* +0xa44  0 caller, 1 answerer */
	int			op_mode;	/* +0xa48  the author's Operation
						 *         Mode              */
	int			timeout_a;	/* +0xa4c  signal detect, s  */
	int			timeout_b;	/* +0xa50  message detect, s */
	int			rate;		/* +0xa54 */

	struct v8_cm		*cm;		/* +0xa58 */
	short			v21_taps[V8_V21_TAPS];	/* +0xa5c */
	unsigned char		pada_d6[2];
	struct v8_detector	detector;	/* +0xad8 */
	struct v8_phase_rev	phase_rev;	/* +0xb40 */
	struct v8_v21_params	v21_params;	/* +0xc20 */

	/* Which sequence is being sent, and two more of the five. */
	struct v8_tx_sequence	*tx_seq;	/* +0xc48 */
	struct v8_tx_sequence	*seq_alt;	/* +0xc4c */
	struct v8_tx_sequence	*seq_spare;	/* +0xc50 */

	/* Five sequence buffers in a row, exactly 0x40 bytes each -- confirmed
	 * by the object, not assumed (finding F68). */
	struct v8_tx_sequence	seq[5];		/* +0xc54 */

	/*
	 * One sliding-DFT bin, used by v8_handshak_agc to watch the settled
	 * line's energy (finding F10136). `energy` (at +0xda0) is what
	 * v8_handshak_agc compares against 0x18f to help decide whether the
	 * far end asked for something worth turning round for.
	 */
	struct v8_dft_bin	dft;		/* +0xd94 */

	/*
	 * A tone generator, laid out like the V.21 parameters but with its own
	 * constants -- this is the one the answering side uses.
	 */
	struct v8_tone		tone;		/* +0xda4 */

	/*
	 * Cleared in several receive-path resets (v8hsrx.c, v8hs.c); nothing
	 * reads it back anywhere in this reconstruction.  Promoted to its
	 * shaped name only -- F10135 already declined a real name here for
	 * the same reason, re-checked here with no new evidence.
	 */
	short			short_db4;	/* +0xdb4 */
	/*
	 * A scratch counter reused by whichever receive sub-state is active:
	 * elapsed blocks while waiting for the AGC or the line to settle, the
	 * count of repeated words while a CM/JM/QCA1 message is being
	 * collected, and the CJ detector's "twice" count.  One field, several
	 * unrelated lifetimes -- named for its shape, not a single meaning.
	 */
	short			block_count;	/* +0xdb6 */
	/* Consecutive zero bits, for the CJ detector.  +0xdb8 */
	short			cj_zero_run;
	/* The receive front end's single-pole filter state (V8_RX_POLE). */
	short			pole_state;	/* +0xdba */
	/* Which word of a received message comes next.  +0xdbc */
	short			word_count;
	/*
	 * Permission to send CM: true by default when `op_mode` is the plain
	 * V.8 mode, or set explicitly by a V8CTRL_START_CM request.  Gates the
	 * calling side's turn-round once ANSam is detected.
	 */
	short			cm_ready;	/* +0xdbe */
	/* Bits of CM sent so far, counted up to V8_HS_CM_BITS. */
	short			cm_bit_count;	/* +0xdc0 */
	unsigned char		paddc2[2];

	/*
	 * What the QCA1 exchange decided.  `quick_connect` is the flag the JM
	 * builder reads to say a QCA1 message was accepted at all -- it is
	 * also what makes `V8GetMessage` hand back `seq_spare` rather than
	 * `tx_seq` -- and the other two are fields lifted out of that
	 * message.  The original's own debug output names them:
	 * "LAPM Indication" for the one bit and "ANSpcm level index" for the
	 * two; its "Finished with Quick Connect" line names the first.
	 */
	int			quick_connect;	/* +0xdc4 */
	int			lapm_indication;	/* +0xdc8 */
	int			anspcm_level;	/* +0xdcc */
	/* QCA1a already accepted once, so the ANSam wait that follows knows
	 * not to expect a fresh CM/JM exchange behind it. */
	short			qca1a_done;	/* +0xdd0 */

	short			toneq_pending;	/* +0xdd2 */
	short			toneq_period;	/* +0xdd4 */
	unsigned char		paddd6[2];
	struct v8_v21		v21;		/* +0xdd8 */

	/* The two timeouts, in samples, and a counter. */
	int			deadline_a;	/* +0xe5c */
	int			deadline_b;	/* +0xe60 */
	/* Elapsed blocks against `deadline_a`/`deadline_b`, in the same
	 * quarter-sample-rate units `deadline()` converts seconds into. */
	int			elapsed;	/* +0xe64 */
	/* The receive front end's own filter line, oldest last. */
	short			agc_line[V8_AGC_TAPS];	/* +0xe68 */
	/* The previous status V8Process returned -- "this is the one place
	 * the whole negotiation is narrated, and `prev_status` exists to hold
	 * the previous status so that it can be" (v8proc.c's own comment). */
	int			prev_status;	/* +0xeb8 */
	/* The call-function and second-extension match verdicts, set by
	 * evaluateRxJMSequence/rebuildJMSequence -- named for the identical
	 * local variables both functions use for the same concept. */
	short			fn_matched;	/* +0xebc */
	short			ext2_matched;	/* +0xebe */
	/* The word that satisfied each match, remembered for
	 * V8UpdateModemParameters/rebuildJMSequence to read back. */
	short			fn_word;	/* +0xec0 */
	short			ext2_word;	/* +0xec2 */
	unsigned char		padec4[V8_STATE_BYTES - 0xec4];
};

/**
 * @brief Point the V.21 modem at a set of filter designs.
 *
 * Called from v8_V21_Init(), and again whenever the handshake changes
 * direction.
 *
 * @param v  The handshake object.
 * @param a  Filter design A.
 * @param b  Filter design B.
 * @param c  Filter design C.
 * @param d  Filter design D.
 */
void V8_setFilters(struct v8 *v, const short *a, const short *b,
		   const short *c, const short *d);

/** @brief Clear the V.21 delay line and its three accumulators. */
void V8_V21_reset(struct v8 *v);

/** @brief Arm the tone queue. */
void v8_TONEq_init(struct v8 *v);

/** @brief Arm the ANSam phase-reversal detector. */
void v8_phase_rev_init(struct v8_phase_rev *pr);

#define V8_PHASE_REV_MIN	0x1af	/* the spacing that counts */
#define V8_PHASE_REV_SPAN	0x26

/**
 * @brief Look for ANSam's phase reversals.
 *
 * The window holds the last 64 samples. Each new sample is correlated
 * against the one half a window back: while the phase is steady that
 * product stays positive, and when the carrier inverts it goes sharply
 * negative. The comparison is against a smoothed energy rather than a
 * fixed threshold, so it works at any level the AGC leaves.
 *
 * Two reversals the right distance apart set `detected`.
 *
 * @param pr     The phase-reversal detector.
 * @param in     Input samples.
 * @param count  Sample count.
 */
void v8_phase_rev_detect(struct v8_phase_rev *pr, const short *in,
			 short count);

/**
 * @brief Reverse the eight bits of a byte.
 *
 * V.8 transmits its octets least significant bit first, so every byte of a
 * CM or JM sequence passes through here on its way out. The original does
 * it as two table lookups on the nibbles, with the halves swapped --
 * reversing each nibble and exchanging them is the same as reversing all
 * eight bits.
 *
 * @param b  The byte to reverse.
 * @return The bit-reversed byte.
 */
unsigned char charFlip(unsigned char b);

/** @brief Multiply two Q15 values, returning the Q15 product. */
short v8_mpyint(short a, short b);
/** @brief Absolute value. */
short v8_absfn(short x);
/** @brief Read the cosine table at a given phase. */
short v8_cosread(unsigned char phase);
/** @brief Fold one bit into a handshake sequence's running CRC. */
void v8_crc(struct v8_handshake *hs, int bit);
/** @brief Copy `n` filter coefficients from `src` to `dst`. */
void v8_copycoeff(short *dst, const short *src, short n);
/** @brief Compute a DFT bin's magnitude squared from its real/imaginary sums. */
void v8_dftenergy(struct v8_dft_bin *bin, short n, short shift);

/**
 * @brief Arm the tone detector.
 *
 * @param v      The handshake object; its receiver gets a flag set.
 * @param d      The detector to configure.
 * @param table  Filter coefficient table.
 * @param a3     Non-zero to count time below `lo_thresh` instead of above
 *               `hi_thresh` (see struct v8_detector's `lo_rule`).
 * @param a4     Configuration value, stored as given.
 * @param a5     Hysteresis run count; stored negated.
 * @param a6     Configuration value, stored as given.
 * @param a7     Configuration value, stored as given.
 */
void v8_detectorinit(struct v8 *v, struct v8_detector *d, const short *table,
		     short a3, short a4, short a5, short a6, short a7);

/**
 * @brief Bring up the V.21 modem V.8 signals over.
 *
 * @param v         The handshake object.
 * @param channel   Which of the two V.21 channels this modem transmits on.
 * @param answerer  Non-zero if this modem answered the call. Independent
 *                  of `channel` -- the two choices pick different things.
 */
void v8_V21_Init(struct v8 *v, short channel, short answerer);

/**
 * @brief Build the CM or JM sequence about to be transmitted.
 *
 * Reads the call menu at `v->cm` and writes into the buffer at
 * `v->tx_seq`. Both are set by the caller, which is how one function
 * serves both messages.
 *
 * @param v  The handshake object.
 */
void initTxSequence(struct v8 *v);

/**
 * @brief Lay out the handshake from the configuration V8Create() planted.
 *
 * Reads `v->side` and builds one of three shapes.
 *
 * @param v  The handshake object.
 */
void v8handshakinit(struct v8 *v);

/*
 * What V8Create is handed: six words the object keeps and reads back from
 * v8handshakinit onwards.
 */
struct v8_cfg {
	int		side;			/* +0x00 -> v8.side      */
	int		op_mode;		/* +0x04 -> v8.op_mode  */
	int		timeout_a;		/* +0x08 */
	int		timeout_b;		/* +0x0c */
	/* The datapump's sample rate; v8dp.c sets it to V8_DP_RATE and
	 * V8Create just carries it into v8.rate, unread by this file. */
	int		rate;			/* +0x10 */
	struct v8_cm	*cm;			/* +0x14 */
};

/**
 * @brief Build a handshake.
 *
 * Allocates 3780 bytes and does not zero them: only the fields the
 * initialisers below set, and whatever v8handshakinit() writes, are
 * defined afterwards.
 *
 * @param cfg  Configuration to plant into the new object.
 * @return The new handshake object.
 */
struct v8 *V8Create(const struct v8_cfg *cfg);

/** @brief Free a handshake object. Tolerates NULL. */
void V8Delete(struct v8 *v);

#define V8_GET_EMPTY	(-1)

/**
 * @brief Read back the message that was received, as octets.
 *
 * This is the inverse of what initTxSequence() builds: each 10-bit
 * character has its framing shifted off and its bits put back in order, so
 * a CM or JM captured off the line becomes the bytes the standard
 * describes. It is the decode half of V.8 and the one thing needed to
 * watch a negotiation.
 *
 * @param v      The handshake object.
 * @param out    Output buffer for the decoded octets.
 * @param count  In/out: the caller's buffer capacity going in, the number
 *               of octets written coming out.
 * @return 0 normally, the full message length when it did not fit `count`
 *         (so the caller can tell truncation from a short message), or
 *         #V8_GET_EMPTY when there is nothing to read.
 */
int V8GetMessage(struct v8 *v, unsigned char *out, int *count);

/*
 * The names are the author's: V8SetMessage announces each through the
 * exported v8SequenceName table, and the fourth selector is the V.92
 * quick-connect QC1A sequence -- an earlier draft guessed "CI".
 */
#define V8_SET_CM	0
#define V8_SET_JM	1
#define V8_SET_CJ	2
#define V8_SET_QC1A	3

/** .rodata+0x53ac, a GLOBAL symbol in the object, so exported here too. */
extern const char *const v8SequenceName[4];

#define V8_SET_TRUNCATED	15
#define V8_SET_REJECTED		(-1)

/**
 * @brief Put a message of your own into one of the five sequence buffers.
 *
 * The counterpart to V8GetMessage() and the same framing: each octet is
 * reversed, shifted up one and given a low bit.
 *
 * @param v       The handshake object.
 * @param which   Which buffer (#V8_SET_CM, #V8_SET_JM, #V8_SET_CJ or
 *                #V8_SET_QC1A); the mapping is not the order the buffers
 *                sit in memory -- CM and JM are swapped.
 * @param octets  The message.
 * @param n       Its length in octets.
 * @return 0 normally, #V8_SET_TRUNCATED when the message was longer than
 *         a buffer holds and only the first fifteen octets went in, or
 *         #V8_SET_REJECTED for an unknown selector or an empty message.
 */
int V8SetMessage(struct v8 *v, int which, const unsigned char *octets, int n);

#define V8_GETBIT_END	(-1)

/**
 * @brief Hand out the next bit of a sequence, least significant first.
 *
 * Folds each bit into the sequence's CRC on the way out.
 *
 * @param s  The sequence.
 * @return 0 or 1, or #V8_GETBIT_END when the sequence is finished and not
 *         set to repeat.
 */
int v8_getbit(struct v8_tx_sequence *s);

/** @brief Arm the transmitter. Always returns 0. */
int v8_txinit(struct v8 *v);
/** @brief Arm the receiver. Always returns 0. */
int v8_rxinit(struct v8 *v);

/** @brief Arm the ANSam tone generator (the same fields v8handshakinit() sets inline). */
void v8_ansaminit(struct v8 *v);

/** @brief Generate four samples of the queued tone, from the phase accumulator. */
void v8_TONEq_generate(struct v8 *v, short *out);

/** @brief Move four samples from the receive ring into its staging buffer. */
int v8_rxreadqueue(struct v8 *v);
/** @brief Move four samples from the transmit staging buffer into its ring. */
int v8_txwritequeue(struct v8 *v);

/** @brief Run one sample through the 61-tap transmit shaping filter. */
short v8_fsktxfilter(struct v8 *v, short sample);

/**
 * @brief Advance a sliding DFT.
 *
 * Each bin has its own phase accumulator and step, and takes `nsamples`
 * samples into its running real and imaginary sums.
 *
 * @param bins      The DFT bins to update.
 * @param nbins     How many bins.
 * @param samples   Input samples.
 * @param nsamples  How many samples.
 */
void v8_dftupdate(struct v8_dft_bin *bins, short nbins, const short *samples,
		  short nsamples);

/**
 * @brief Generate four samples of FSK.
 *
 * The result goes through the shaping filter and straight into the
 * transmit ring.
 *
 * @param v      The handshake object.
 * @param which  Picks the mark or the space carrier.
 */
int v8_fskmodulate(struct v8 *v, short which);

/** @brief One step of the receive AGC. */
int v8_agcadapt(struct v8 *v);

/**
 * @brief The receive front end.
 *
 * Takes a block out of the symbol buffer, filters it with the design that
 * matches which end of the call this is, applies the current gain with
 * saturation, and adapts that gain from what it measured.
 *
 * @param v  The handshake object.
 */
int V8agc(struct v8 *v);

#define V8_STABLE_PERIOD	0x3bf	/* counter limit, stepped by four */
#define V8_STABLE_TOLERANCE	0x333	/* Q14: about five percent     */

/**
 * @brief Watch the AGC's gain and decide when it has settled.
 *
 * Every call advances two counters by four. The first sets how often the
 * reference is refreshed; between refreshes the gain is compared against
 * it, and a relative change of more than about five percent resets the
 * second counter. When the second counter survives long enough the line
 * is called stable, which is what the handshake waits for before
 * believing anything it hears.
 *
 * @param v  The handshake object.
 */
void checkSignalStability(struct v8 *v);

#define V8_JM_FN_MASK	0xfff1	/* what identifies a call-function word */
#define V8_JM_FN_MARK	0x101
#define V8_JM_EXT2_MARK	0x0a1

/**
 * @brief Check that a received JM answers the CM that was sent.
 *
 * Two passes over the received sequence. The first looks for the call
 * function and, when the menu declared one, matches the extension
 * characters that follow it; the second does the same for the second
 * extension against its own marker. Each pass records what it matched and
 * sets a flag.
 *
 * A mismatch inside an extension slides the expected character along
 * rather than the received word, so a JM that repeats a character still
 * matches -- which is what makes this tolerant of the framing jitter a
 * real line gives.
 *
 * @param v  The handshake object.
 */
void evaluateRxJMSequence(struct v8 *v);

/**
 * @brief Turn what was received into a call menu.
 *
 * This is the other half of the monitoring story: V8GetMessage() hands
 * back the octets, and this reads the fields out of them -- which call
 * function was asked for, which modulations were offered, and what the
 * two extension bytes carried.
 *
 * @param v    The handshake object.
 * @param out  Output: the decoded call menu.
 * @return 0 when it filled the menu, -1 when there was nothing to read.
 */
int V8UpdateModemParameters(struct v8 *v, struct v8_cm *out);

/**
 * @brief Build the JM that answers a received CM.
 *
 * Unlike initTxSequence(), which builds a message from the local menu
 * alone, this one is driven by what arrived: the call function is echoed
 * when it is one this end accepts, the extensions are echoed character for
 * character, and the three menu words are ANDed with the ones received --
 * so what goes back is the intersection of what was offered and what is
 * wanted.
 *
 * @param v  The handshake object.
 */
void rebuildJMSequence(struct v8 *v);

/**
 * @brief One pass of the handshake: transmit until the queue is full,
 * then run the receiver once.
 *
 * @param v  The handshake object.
 * @return 0 normally, 1 when a deadline expired, 2 when the handshake
 *         finished.
 */
int v8handshak(struct v8 *v);

/*
 * What V8Process returns, and what the object's own table calls each value.
 *
 * `v8StatusName` is a global at .rodata+0x53c0, nineteen pointers, indexed
 * directly by the status -- so these are the author's names, not ours.  Read
 * together with the handshake's diagnostics they settle several fields at
 * once: `rx_state` of 4 is the state whose timeout prints "Time Out Waiting For
 * CM" and whose status is named ..._WAITING_FOR_CM, and the same holds for
 * CJ, ANSam and JM.  ORG is the calling side (mode 0), ANS the answering one.
 *
 * The last three are never produced by V8Process; they exist in the table and
 * in the datapump's switch, and the enum ends with the author's own sentinel.
 */
enum v8_status {
	V8_INIT				= 0,
	V8_ANS_SEND_ANSAM		= 1,
	V8_ANS_CM_DETECTED		= 2,
	V8_ANS_SEND_JM			= 3,
	V8_ANS_TIME_OUT_WAITING_FOR_CM	= 4,
	V8_ANS_TIME_OUT_WAITING_FOR_CJ	= 5,
	V8_ORG_WAITING_FOR_ANSAM	= 6,
	V8_ORG_ANSAM_DETECTED_WAITING_TE = 7,
	V8_ORG_SEND_CM			= 8,
	V8_ORG_JM_DETECTED		= 9,
	V8_ORG_SEND_CJ			= 10,
	V8_ORG_TIME_OUT_WAITING_FOR_ANSAM = 11,
	V8_ORG_TIME_OUT_WAITING_FOR_JM	= 12,
	V8_OK				= 13,
	V8_ORG_SEND_QC			= 14,
	V8_ORG_WAITING_FOR_QCA1d	= 15,
	V8_ORG_BAD_QCA1d_MESSAGE	= 16,
	V8_ORG_TIME_OUT_WAITING_FOR_QCA1d = 17,
	V8_LAST_ENUM			= 18
};

extern const char *const v8StatusName[V8_LAST_ENUM + 1];

/**
 * @brief Run one buffer of samples through the handshake.
 *
 * @param v      The handshake object.
 * @param in     Input samples.
 * @param out    Output samples.
 * @param count  Sample count.
 * @return A #v8_status value.
 */
int V8Process(struct v8 *v, const short *in, short *out, int count);

/**
 * @brief The tone detector's notch filter section, as a standalone
 * function.
 *
 * The object defines this and calls it from nowhere: v8_tone_detect()
 * has it inlined.
 */
short notch_filter(const short *in, struct v8_detector *d);
/**
 * @brief The tone detector's biquad filter section, as a standalone
 * function.
 *
 * The object defines this and calls it from nowhere: v8_tone_detect()
 * has it inlined.
 */
short biquad_filter(short in, struct v8_detector *d, const short *coeff);

/** @brief Wait for the line to settle and then for a tone. One of the handshake's two long receive paths; see v8hsrx.c. */
int v8_handshak_agc(struct v8 *v);
/** @brief Turn the demodulator's bits into characters and match them against `rx_substate`. The other long receive path; see v8hsrx.c. */
int v8_handshak_demod(struct v8 *v);

/*
 * Sub-states of the demodulate path, as `rx_substate` holds them.  They pick what
 * the character stream is being matched against; the receive state stays at
 * 0x28 throughout.  Values below 0x28 belong to the other receive states and
 * are not listed here.
 */
#define V8_HS_DRAIN	0x23	/* let the outgoing sequence finish   */
#define V8_HS_COLLECT	0x28	/* the fifteen-word message, twice    */
#define V8_HS_HUNT	0x29	/* look for either preamble           */
#define V8_HS_CJ	0x2a	/* look for CJ                        */
#define V8_HS_QCA1	0x2c	/* the six-word QCA1 message          */
/*
 * And two that mean "stop matching": the message has been taken and the
 * character stream is ignored from here.  Which one is set says which side
 * took it, and `V8Process` reports them as different statuses.
 */
#define V8_HS_TAKEN_RX	0x32	/* side != 1, op_mode == 1 */
#define V8_HS_TAKEN_TX	0x33	/* side == 1, op_mode == 1 */

#define V8_ANSAM_REVERSAL	0x438
#define V8_ANSAM_DEPTH		0xccd	/* Q14: 0.20 */
#define V8_ANSAM_UNITY		0x4000	/* Q14: 1.0  */

/**
 * @brief Generate four samples of ANSam.
 *
 * A carrier amplitude-modulated by a second, slower oscillator, with the
 * amplitude negated every #V8_ANSAM_REVERSAL blocks -- the periodic phase
 * reversal that distinguishes ANSam from a plain answer tone.
 *
 * @param v    The handshake object.
 * @param out  Output: four samples.
 */
void v8_ansamgenerate(struct v8 *v, short *out);

/*
 * The names are the author's, from the table V8Control prints through
 * (.rodata+0x5380, eleven entries).  Each says which message the request
 * starts, and each matches what the arm does: START_JM puts the answerer into
 * hunt-for-CJ and the transmitter into sending, which is what follows a JM.
 * The eight beyond them are placeholders in the original too -- CTRL3 to
 * CTRL10, named after their own indices -- and no arm accepts them.
 */
#define V8CTRL_START_CM		0
#define V8CTRL_START_CJ		1
#define V8CTRL_START_JM		2
#define V8CTRL_LAST		10

extern const char *const v8ControlName[V8CTRL_LAST + 1];

/**
 * @brief Nudge the handshake from outside.
 *
 * Three requests are implemented, each valid only from one state; the
 * other eight (#V8CTRL_LAST's worth) are placeholders in the original too
 * and no arm accepts them.
 *
 * @param v     The handshake object.
 * @param what  One of #V8CTRL_START_CM, #V8CTRL_START_CJ, #V8CTRL_START_JM,
 *              or an unimplemented request.
 * @return 0 when the request was accepted, -1 when it was not (including
 *         for an unknown request).
 */
int V8Control(struct v8 *v, int what);

/**
 * @brief Run the tone detector over the samples between `in` and
 * `v->rx.buf`, filtering in place.
 *
 * A fixed input biquad, then two more from the table the detector was
 * built with, then a rectifier and a leaky integrator. The verdict comes
 * from counting how long the integrator stays the right side of two
 * thresholds, which is the hysteresis that stops a passing noise burst
 * counting as a tone.
 *
 * @param v   The handshake object.
 * @param d   The detector.
 * @param in  Input samples, up to `v->rx.buf`.
 * @return 1 while the tone is considered present.
 */
int v8_tone_detect(struct v8 *v, struct v8_detector *d, short *in);

/**
 * @brief Demodulate V.21.
 *
 * Four correlations per symbol -- the mark pair and the space pair, each a
 * real and an imaginary arm -- and the larger energy wins. If neither
 * reaches the threshold the line is called silent and both run counters
 * are dropped, which is what stops noise between characters producing
 * bits.
 *
 * Bits come out of run lengths rather than one per symbol: four
 * consecutive decisions the same way is one bit, and the remainder is
 * carried. That is how a 300 baud signal sampled at 1200 symbols a second
 * is decoded.
 *
 * @param v  The handshake object.
 */
void v8_fskdemodulate(struct v8 *v);

/* How many samples each queue operation moves. */
#define V8_QUEUE_BLOCK	4

/*
 * Where the transmit ring ends, in samples, and the top of the shaping
 * filter's delay line.  Both come out of the object rather than the array
 * sizes: the ring's wrap point is +0x5c0 and the filter's line starts at
 * +0x90c, which is the sixtieth sample of the receive scratch.
 */
#define V8_TX_RING_END	((0x5c0 - 0x228) / 2)
#define V8_FSK_TAP_TOP	((0x90c - 0x894) / 2)

#endif /* DSPLIB_V8_H */
