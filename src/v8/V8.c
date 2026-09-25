/*
 * V8.c -- the handshake core.
 *
 * What is left once the pieces with their own translation unit are taken
 * out: the two state machines (`v8handshakinit` lays them out, `v8handshak`
 * runs them), the two tone generators the transmit side drives, and the
 * answerer's tone setup.  The object keeps these in one unit, which is why
 * `v8handshakinit` has `v8_ansaminit`'s seven stores inlined into it and
 * why `v8handshak` carries the receive paths inline rather than calling
 * them.
 *
 * The detector coefficient table is the unit's only local object: the
 * object's `.rodata+0x5670`, sixteen bytes, reached only from
 * `v8handshakinit`.
 */

#include "dsplib/debug.h"
#include "dsplib/v8.h"

/*
 * The detector's coefficient table, .rodata+0x5670.  Eight entries, not the
 * hundred-odd the gap to the next known table suggests: relocations begin at
 * .rodata+0x5680, so everything past index 8 is a pointer array belonging to
 * something else.  A verbatim copy of that region would hold link-time
 * addends where the running object holds addresses, which is exactly how the
 * first version failed.
 */
static const short detector_table[8] = {
	     0,      0,      0,      0,  -6608,  15416,  -5792,  15416
};

/* The two configured timeouts are in units of a quarter of a 9600 Hz second. */
static int
deadline(int units)
{
	if (units <= 0)
		return -1;
	return (units * 9600) >> 2;
}

/*
 * Arm the ANSam tone generator.
 *
 * These are the same seven stores `v8handshakinit` makes inline in its
 * answering shape; the compiler inlined this function there rather than
 * calling it.  Kept as a function because that is what the object says it
 * is, and because the constants belong in one place.
 */
void
v8_ansaminit(struct v8 *v)
{
	v->tone.carrier_phase = 0;
	v->tone.envelope_step = 0x1a;
	v->tone.carrier_step = 0xe00;
	v->tone.reversal_count = 0;
	v->tone.envelope_phase = 0;
	v->tone.amplitude = v8_mpyint(0x3e80, v->tx_gain);
	v->tone.reversal_enable = 1;
}

void
v8handshakinit(struct v8 *v)
{
	struct v8_cm *cm;
	int mode;

	/* The preamble, common to every shape. */
	v->tx_fill_target = 0x10;
	v->dft.phase = 0;
	v->dft.step = 0x1a;
	v->dft.im = 0;
	v->dft.energy = 0;
	v->dft.re = 0;
	v->short_a40 = 0x200;

	v8_rxinit(v);
	v8_txinit(v);

	v->quick_connect = 0;
	v->qca1a_done = 0;
	v->ext2_word = 0;
	mode = v->side;
	v->lapm_indication = 0;
	v->anspcm_level = 0;
	v->fn_matched = 0;
	v->ext2_matched = 0;
	v->fn_word = 0;

	if (mode == 0) {
		v->tx_state = 5;
		v->rx_substate = 0x19;
		v->rx_state = 0x19;
		/*
		 * Assigned, not or-ed: this drops whatever v8_rxinit left in
		 * the flag word, and the two detectors below then set their
		 * own bits on top.
		 */
		v->rx.flags = 0x8000;

		v->deadline_a = deadline(v->timeout_a);
		v->deadline_b = deadline(v->timeout_b);
		v->elapsed = 0;

		v8_detectorinit(v, &v->detector, detector_table, 0, 100, 50,
				1500, 0);
		v8_phase_rev_init(&v->phase_rev);

		v->tx_seq = &v->seq[0];
		v->seq_alt = &v->seq[2];
		initTxSequence(v);

		/* A three-character sequence of all ones, built by hand. */
		v->seq[1].word[0] = 1;
		v->seq[1].word[1] = 1;
		v->seq[1].word[2] = 1;
		v->seq[1].crc = -1;
		v->seq[1].nbits = 30;
		v->seq[1].wordbits = 10;
		v->seq[1].repeat = 1;
		v->seq[1].crc_enable = 0;
		v->seq[1].bitpos = 0;
		v->seq[1].wordidx = 0;
		v->seq[1].repeats = 0;
		v->seq[1].shifter = 0;
		v->seq[1].nleft = 0;
		v->seq[1].shifter0 = 0;
		v->seq[1].nleft0 = 0;

		v->cm_ready = (short)(v->op_mode == 0);
		cm = v->cm;

		if (cm->b2 & 0x10) {
			int bits;
			int menu = cm->menu;

			v->tx_seq = &v->seq[3];
			v->seq_spare = &v->seq[4];

			v->seq[3].word[0] = 0x3ff;
			v->seq[3].word[1] = 0x155;

			bits = ((menu * 2) & 0x04) | ((menu * 2) & 0x08)
			       | ((menu * 4) & 0x20);
			bits |= (cm->b2 & 0x40) ? 0x41 : 0x01;
			if (menu & 0x01)
				bits |= 0x02;

			v->seq[3].word[2] = (short)bits;
			v->seq[3].word[5] = (short)bits;
			v->seq[3].word[3] = 0x3ff;
			v->seq[3].word[4] = 0x155;

			v->cm_bit_count = 0;
			v->toneq_pending = 0;
			v->toneq_period = 0x688;

			v->seq[3].crc = -1;
			v->seq[3].bitpos = 0;
			v->seq[3].wordidx = 0;
			v->seq[3].repeats = 0;
			v->seq[3].nbits = 60;
			v->seq[3].wordbits = 10;
			v->seq[3].crc_enable = 0;
			v->seq[3].shifter = 0;
			v->seq[3].shifter0 = 0;
			v->seq[3].nleft = 0;
			v->seq[3].nleft0 = 0;
			v->seq[3].repeat = 1;
		}

		v->block_count = 0;
		v->short_db4 = 0;
		return;
	}

	if (mode != 1)
		return;

	v->rx_state = 0x20;
	v->tx_state = 6;
	v->deadline_a = deadline(v->timeout_a);
	v->deadline_b = deadline(v->timeout_b);

	v->tone.carrier_phase = 0;
	v->tone.envelope_step = 0x1a;
	v->tone.carrier_step = 0xe00;
	v->tone.reversal_count = 0;
	v->elapsed = 0;
	v->tone.envelope_phase = 0;
	v->tone.amplitude = v8_mpyint(0x3e80, v->tx_gain);
	v->tone.reversal_enable = 1;

	v8_V21_Init(v, 1, 0);

	v->tx_seq = &v->seq[2];
	v->seq_alt = &v->seq[0];
	/* Again assigned, so the bit v8_V21_Init just set is dropped. */
	v->rx.flags = 0x8004;
	initTxSequence(v);

	v->cj_zero_run = 0;
	v->block_count = 0;
	v->short_db4 = 0;
}

/*
 * Four samples of the queued tone.  A 14-bit phase accumulator stepped by
 * the period, read out of the cosine table with the usual rounding -- the
 * same idiom as the dialler's DTMF, at a different width.
 */
void
v8_TONEq_generate(struct v8 *v, short *out)
{
	int i;

	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		unsigned phase = (unsigned)(unsigned short)v->toneq_pending
				 + (unsigned short)v->toneq_period;

		phase &= 0x3fff;
		v->toneq_pending = (short)phase;
		out[i] = v8_cosread((unsigned char)((phase + 0x20) >> 6));
	}
}

/*
 * The answer tone that opens every answerer call.  It is ANSam: a 2100 Hz
 * carrier amplitude-modulated at 15 Hz, with a phase reversal every
 * V8_ANSAM_REVERSAL blocks -- the periodic phase reversal that is what
 * tells a listening modem this is ANSam and not a bare answer tone.
 *
 * The reversal counter only runs while the enable at +0x0e is set, so a
 * caller can have the tone without the reversals.
 */
void
v8_ansamgenerate(struct v8 *v, short *out)
{
	struct v8_tone *t = &v->tone;
	int i;

	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		unsigned envelope;
		unsigned carrier;
		short depth;
		short level;

		envelope = ((unsigned)(unsigned short)t->envelope_phase
			    + (unsigned short)t->envelope_step) & 0x3fff;
		t->envelope_phase = (short)envelope;

		carrier = ((unsigned)(unsigned short)t->carrier_phase
			   + (unsigned short)t->carrier_step) & 0x3fff;
		t->carrier_phase = (short)carrier;

		depth = v8_mpyint(V8_ANSAM_DEPTH,
				  v8_cosread((unsigned char)((envelope + 0x20)
							     >> 6)));
		level = v8_mpyint((short)(depth + V8_ANSAM_UNITY), t->amplitude);

		out[i] = v8_fsktxfilter(v,
			v8_mpyint(v8_cosread((unsigned char)((t->carrier_phase + 0x20)
							     >> 6)), level));
	}

	if (t->reversal_enable == 0)
		return;

	if ((unsigned short)(t->reversal_count + 1) == V8_ANSAM_REVERSAL) {
		t->reversal_count = 0;
		t->amplitude = (short)-t->amplitude;
	} else {
		t->reversal_count = (short)(t->reversal_count + 1);
	}
}

/* Transmit states, as `tx_state` holds them. */
#define V8_TX_SILENCE	5
#define V8_TX_ANSAM	6
#define V8_TX_FSK_TIMED	23
#define V8_TX_FSK	43
#define V8_TX_TONE	45

/* Receive states, as `rx_state` holds them. */
#define V8_RX_AGC	0x19
#define V8_RX_SETTLE	0x20
#define V8_RX_DRAIN	0x23
#define V8_RX_DEMOD	0x28
#define V8_RX_DONE	0x63

/* How long each receive state waits before giving up on it. */
#define V8_RX_AGC_BLOCKS	0x960
#define V8_RX_SETTLE_BLOCKS	0x258

/* Bits per character on the wire, and how many make a full CM. */
#define V8_HS_CM_BITS		0x3c

/*
 * Send the next four samples of whatever this state transmits.  Returns 0 to
 * carry on, or a value to return from the handshake.
 */
static int
transmit(struct v8 *v, int *done)
{
	struct v8_v21_params *p = &v->v21_params;
	int i;

	*done = 0;

	switch ((short)v->tx_state) {
	case V8_TX_SILENCE:
		for (i = 0; i < V8_QUEUE_BLOCK; i++)
			v->tx_stage[i] = 0;
		v8_txwritequeue(v);
		return 0;

	case V8_TX_TONE:
		v8_TONEq_generate(v, v->tx_stage);
		v8_txwritequeue(v);
		return 0;

	case V8_TX_ANSAM:
		if (v->deadline_a != -1 && v->elapsed >= v->deadline_a) {
			/*
			 * The equal case counts this block, the past-it case
			 * does not -- and the announcement sits inside it, so
			 * the timeout is reported exactly once however many
			 * blocks arrive afterwards.  That is what the odd
			 * count-once idiom is FOR; without the call site it
			 * reads as a pointless conditional increment.
			 */
			if (v->elapsed == v->deadline_a) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V8: Time Out Waiting For " "CM...\r\n");
				v->elapsed++;
			}
			v->rx_state = 4;
			*done = 1;
			return 1;
		}
		v8_ansamgenerate(v, v->tx_stage);
		v8_txwritequeue(v);
		v->elapsed++;
		return 0;

	case V8_TX_FSK_TIMED:
		if (v->deadline_b != -1 && v->elapsed >= v->deadline_b) {
			v->rx_state = v->side == 1 ? 5 : 0xc;
			/*
			 * Announced once, as above.  Which message was being
			 * waited for follows the side: the answerer is waiting
			 * for the caller's CJ, the caller for the answerer's
			 * JM.
			 */
			if (v->elapsed == v->deadline_b) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V8: Timeout waiting for %s " "message...\r\n",
					    v->side == 1 ? "CJ" : "JM");
				v->elapsed++;
			}
			*done = 1;
			return 1;
		}
		v8_fskmodulate(v, v->tx_bit);
		p->sample_count = (short)(p->sample_count + 4);
		if (p->samples_per_bit == p->sample_count) {
			v->tx_bit = (short)v8_getbit(v->tx_seq);
			p->sample_count = 0;
		}
		v->elapsed++;
		return 0;

	case V8_TX_FSK:
		v8_fskmodulate(v, v->tx_bit);
		p->sample_count = (short)(p->sample_count + 4);
		if (p->samples_per_bit == p->sample_count) {
			v->tx_bit = (short)v8_getbit(v->tx_seq);
			v->cm_bit_count = (short)(v->cm_bit_count + 1);
			if (v->cm_bit_count == V8_HS_CM_BITS) {
				/*
				 * A whole CM has gone out; switch to the
				 * timed state and to the other buffer.
				 */
				v->tx_state = V8_TX_FSK_TIMED;
				v->tx_seq = &v->seq[0];
			}
			p->sample_count = 0;
		}
		return 0;

	default:
		/* No such state: wait for the queue to drain. */
		return 0;
	}
}

/*
 * The two long receive paths, and the helpers the second one drives.
 * All of them are `static` here because the object defines no such
 * symbols: `-O3` inlined the whole chain into `v8handshak`, and a
 * translation unit is an inlining boundary, so they can only be in
 * V8.c.  Moved verbatim out of the old `v8hsrx.c`, which is gone.
 */

/* How long each wait runs before it gives up. */
#define V8_AGC_BLOCKS		0x960

/* Consecutive zero bits that end a character. */
#define V8_HS_ZERO_RUN		6

/* Bits a character must be at least this long to count. */
#define V8_HS_MIN_ONES		9

static int
v8_handshak_agc(struct v8 *v)
{
	struct v8_rx *r = &v->rx;
	short scratch[V8_QUEUE_BLOCK];
	int i;

	V8agc(v);

	if (v->rx_substate == 0x19) {
		/*
		 * Listening for the answer tone.  The deadline is checked
		 * first, and -1 means there is not one.
		 */
		if (v->deadline_a != -1 && v->elapsed >= v->deadline_a) {
			/* Announced once, on the block that reaches it. */
			if (v->elapsed == v->deadline_a) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V8: Time Out Waiting For " "ANSam...\r\n");
				v->elapsed++;
			}
			v->rx_state = 0xb;
			return 1;
		}
		v->elapsed++;
		if (v8_tone_detect(v, &v->detector, v->rx_stage) != 0) {
			v->rx_substate = 0x24;
			v->block_count = 0;
			v->elapsed = 0;
			r->flags &= (unsigned short)~V8_RX_DETECTOR_ARMED;
		}
		return 0;
	}

	/*
	 * Still settling.  The rectified block feeds the DFT once the gain
	 * has stopped moving; the phase-reversal detector sees the block
	 * either way.
	 */
	for (i = 0; i < V8_QUEUE_BLOCK; i++)
		scratch[i] = v8_absfn(v->rx_stage[i]);

	checkSignalStability(v);
	if (r->stable != 0)
		v8_dftupdate(&v->dft, 1, scratch, V8_QUEUE_BLOCK);

	v8_phase_rev_detect(&v->phase_rev, v->rx_stage, V8_QUEUE_BLOCK);

	v->block_count = (short)(v->block_count + 1);
	if ((short)v->block_count <= V8_AGC_BLOCKS)
		return 0;

	r->flags |= V8_RX_DETECTOR_ARMED;
	if (r->stable != 0)
		v8_dftenergy(&v->dft, 1, 1);

	/*
	 * Time is up.  What happens next depends on whether the far end
	 * asked for something this end can offer.
	 */
	if ((unsigned short)v->dft.energy > 0x18f || v->phase_rev.detected != 0) {
		if (((v->cm->b2 >> 4) & 1 & (short)v->qca1a_done) != 0) {
			v->tx_state = 0x2d;
			return 0;
		}
		if (v->cm_ready == 0)
			return 0;

		/*
		 * Turn round: answer on the other channel.  The one message
		 * in the handshake with no "V8: " on the front and a bare \n
		 * -- and it names what the wait was for, which is how the
		 * tone this branch has just accepted is identified as ANSam.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V8 ANSAM Detected (CM ready)\n");

		v8_V21_Init(v, 0, 1);
		r->adapt_rate = 0x800;
		v->tx_bit = 1;
		v->block_count = 0;
		v->short_db4 = 0;
		v->rx_state = 0x28;
		v->rx_substate = V8_HS_HUNT;
		v->tx_state = (v->cm->b2 & 0x10) ? 0x2b : 0x17;
		return 0;
	}

	if (((v->cm->b2 >> 4) & 1 & (short)v->qca1a_done) == 0)
		return 1;
	if (v->tx_state == 0x2d) {
		v->tx_state = 5;
		v->rx_state = 0x63;
		return 2;
	}
	v->tx_state = 5;
	return 1;
}

/*
 * The rest of this file is the matching half, one function per sub-state.
 *
 * Bits arrive ten to a character: a start bit, eight data bits and a stop
 * bit, oldest first in the bottom of `f1a`.  `f18` counts them.  Two of the
 * sub-states look at the raw shift register instead and so never wait for a
 * whole character; the other four take `f1a & 0x3ff` once `f18` reaches ten.
 */

/* Ten bits to a character, and the top bit of one. */
#define V8_HS_CHAR_BITS		10
#define V8_HS_CHAR_TOP		0x200
#define V8_HS_CHAR_MASK		0x3ff

/*
 * The two preambles the hunt matches, twelve bits wide because each is the
 * tail of one character and the head of the next.  `0xc0f` introduces the
 * fifteen-word message, `0xd55` the six-word QCA1 one.
 */
#define V8_HS_PREAMBLE_MSG	0xc0f
#define V8_HS_PREAMBLE_QCA1	0xd55

/* The word each message starts with, which is also its frame marker. */
#define V8_HS_MARK_MSG		0x0f
#define V8_HS_MARK_QCA1		0x155

/* Words in each, not counting the marker. */
#define V8_HS_MSG_WORDS		14
#define V8_HS_QCA1_WORDS	5

/* Every bit set: what a word is filled with before anything is received. */
#define V8_HS_WORD_ANY		0x3ff

/* A CJ octet is nine zero bits and a stop bit, and it must arrive twice. */
#define V8_HS_CJ_ZEROS		9
#define V8_HS_CJ_COUNT		2

/* How long the drain waits before giving up on the sequence emptying. */
#define V8_HS_DRAIN_BLOCKS	0x104

/*
 * Waiting for the transmit sequence to run out, then swapping to the next
 * buffer and starting a long count.  When that count expires the handshake
 * is over.
 */
static int
v8_hs_drain(struct v8 *v)
{
	if (v->block_count != 0) {
		v->block_count = (short)(v->block_count + 1);
		if (v->block_count != V8_HS_DRAIN_BLOCKS)
			return 0;
		v->tx_state = 5;
		v->rx_state = 0x63;
		return 2;
	}
	if (v->tx_seq->nleft != 0)
		return 0;
	v->tx_seq = &v->seq[1];
	v->block_count = 1;
	return 0;
}

/*
 * Hunting for a preamble in the raw bit stream.  Either match arms the
 * matching buffer -- marker in the first word, "anything" in the rest, and
 * -1 in `wordidx`, which is where the length of the previous repetition is
 * kept and cannot be a valid one -- and restarts the character framing.
 */
static int
v8_hs_hunt(struct v8 *v)
{
	struct v8_v21_params *p = &v->v21_params;
	struct v8_tx_sequence *s;
	int bits = (unsigned short)p->bits & 0xfff;
	int n;
	int i;

	if (bits == V8_HS_PREAMBLE_MSG) {
		s = v->seq_alt;
		s->word[0] = V8_HS_MARK_MSG;
		n = V8_HS_MSG_WORDS;
		v->rx_substate = V8_HS_COLLECT;
	} else if (bits != V8_HS_PREAMBLE_QCA1) {
		return 0;
	} else if (!(v->cm->b2 & 0x10)) {
		/* The far end never offered PCM, so there is no QCA1. */
		return 0;
	} else {
		s = v->seq_spare;
		s->word[0] = V8_HS_MARK_QCA1;
		n = V8_HS_QCA1_WORDS;
		v->rx_substate = V8_HS_QCA1;
	}

	for (i = 1; i <= n; i++)
		s->word[i] = V8_HS_WORD_ANY;
	v->block_count = 1;
	v->word_count = 1;
	s->wordidx = -1;
	p->bitcount = 0;
	p->bits = 0;
	return 0;
}

/*
 * The far end's message has arrived twice the same.  What that means depends
 * on which side this is and on `op_mode`; only the two answering arms
 * rebuild the JM and reset the transmitter with it.
 */
static int
v8_hs_message_done(struct v8 *v)
{
	struct v8_rx *r = &v->rx;
	int rebuild = 1;

	if (v->side != 1) {
		evaluateRxJMSequence(v);
		v->rx_substate = v->op_mode == 1 ? V8_HS_TAKEN_RX : V8_HS_DRAIN;
		rebuild = 0;
	} else if (v->op_mode == 1) {
		v->rx_substate = V8_HS_TAKEN_TX;
	} else {
		v->rx_substate = V8_HS_CJ;
		v->tx_state = 0x17;
		v->elapsed = 0;
	}

	if (rebuild) {
		rebuildJMSequence(v);
		v->short_db4 = 0;
		v->word_count = 0;
		v->tx_bit = 1;
		v->tx_seq->shifter = 0;
		v->tx_seq->nleft = 0;
	}

	r->flags |= V8_RX_DETECTOR_ARMED;
	v->short_a40 = r->gain;
	v->block_count = 0;
	return 0;
}

/*
 * Collecting the fifteen-word message.  Each character either matches what
 * the buffer already holds -- which counts towards accepting it -- or
 * replaces it and resets the count.  The marker starts a repetition, and a
 * repetition that ran as far as the last one did is the message.
 */
static int
v8_hs_collect(struct v8 *v, int ch)
{
	struct v8_tx_sequence *s = v->seq_alt;
	/*
	 * The bound check below comes *after* this read in the original, so
	 * `word_count == 15` looks at `crc`, and the matching path raises
	 * `word_count` with no cap at all.  Reproduced, but read through a
	 * view of the whole sequence object so that it stays a defined access
	 * here.  It only runs away on a stream that never sends the marker
	 * again and whose characters go on matching the transmit fields past
	 * the array; a well-formed message resets `word_count` to 1 every
	 * fifteen words.
	 */
	const short *w = (const short *)s;
	int idx;

	if (ch == V8_HS_MARK_MSG) {
		if (s->wordidx == v->block_count)
			return v8_hs_message_done(v);
		s->word[0] = V8_HS_MARK_MSG;
		s->wordidx = v->word_count;
		v->block_count = 1;
		v->word_count = 1;
		return 0;
	}

	idx = (short)v->word_count;
	if ((unsigned short)w[idx] == (unsigned)ch) {
		v->word_count = (short)(idx + 1);
		v->block_count = (short)(v->block_count + 1);
		return 0;
	}

	if ((short)v->word_count <= V8_HS_MSG_WORDS) {
		s->word[idx] = (short)ch;
		v->word_count = (short)(v->word_count + 1);
	}
	v->block_count = 0;
	return 0;
}

/*
 * Collecting the six-word QCA1 message, and checking it when the sixth
 * arrives.  Word 1 and word 4 carry the same field twice over, words 2 and 3
 * are fixed, and word 5 has to have its top six bits set; word 1 then says
 * which of the two shapes this is.  The original's own names for them are
 * QCA1a and QCA1d, from its debug output.
 */
static int
v8_hs_qca1(struct v8 *v, int ch)
{
	struct v8_tx_sequence *s = v->seq_spare;
	int idx = (short)v->word_count;
	int w1;
	int w4;
	int is_d;
	int ok;

	v->word_count = (short)(idx + 1);
	s->word[idx] = (short)ch;
	if (v->word_count != V8_HS_QCA1_WORDS + 1)
		return 0;

	w1 = (unsigned short)s->word[1];
	w4 = (unsigned short)s->word[4];
	is_d = (w1 & 0x3b9) == 0x181;

	/*
	 * Words 2 and 3 are constants on the wire -- all ones, then the
	 * marker again -- not leftovers of the fill, which the two words
	 * before them would also still be if nothing had arrived.
	 */
	ok = (is_d || (w1 & 0x391) == 0x81)
	     && (unsigned short)s->word[2] == 0x3ff
	     && (unsigned short)s->word[3] == 0x155
	     && w4 == w1
	     && ((unsigned short)s->word[5] & 0x3f0) == 0x3f0;

	if (!ok) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: reseting QCA1 detector...\r\n");
		v->rx_substate = V8_HS_HUNT;
		return 0;
	}

	v->quick_connect = 1;
	v->lapm_indication = (w1 >> 6) & 1;

	if (!is_d) {
		/*
		 * QCA1a.  Back to waiting for the answer tone, with the
		 * detector told that one has already been through.
		 *
		 * Every field is reported from BOTH copies -- word 1 and its
		 * repeat in word 4 -- and since the acceptance test above
		 * requires the two to be equal, the pairs always agree.  That
		 * is the point: the line shows the redundancy survived.
		 *
		 * The bit numbers are the author's, over the received stream
		 * with word k occupying bits 10k+10 to 10k+19, most
		 * significant first: word 1 is bits 20-29 and word 4 is bits
		 * 50-59, which is why every number here differs by thirty.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8:  QCA1a: Got Good QCA1a !!!!\r\n");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8:  QCA1a: U_QTS: bits24,26-28 = %d%d%d%d, "
			    "bits54,56-58 = %d%d%d%d\r\n",
			    (w1 >> 5) & 1, (w1 >> 3) & 1, (w1 >> 2) & 1,
			    (w1 >> 1) & 1, (w4 >> 5) & 1, (w4 >> 3) & 1,
			    (w4 >> 2) & 1, (w4 >> 1) & 1);

		v->tx_state = 5;
		v->rx_substate = 0x19;
		v->rx_state = 0x19;
		v->qca1a_done = 1;

		/*
		 * Labelled QCA1d, but it is in the QCA1a arm -- the QCA1d arm
		 * below returns before it could ever be reached.  Reproduced
		 * as it stands; the prefix is the author's slip, not ours.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8:  QCA1d: LAPM Indication: bit23 = %d, "
			    "bit53 = %d\r\n", (w1 >> 6) & 1, (w4 >> 6) & 1);
		return 0;
	}

	/* QCA1d, and that is the whole negotiation. */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V8:  QCA1d: Got Good QCA1d !!!!\r\n");
	v->anspcm_level = (w1 >> 1) & 3;
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V8:  QCA1d: ANSpcm level index: bits27-28 = %d, "
		    "bits57-58 = %d\r\n", (w1 >> 1) & 3, (w4 >> 1) & 3);
	v->tx_state = 5;
	v->rx_state = 0x63;
	return 2;
}

/*
 * Hunting for CJ: nine zero bits followed by a one, twice.  The zero count
 * lives in the object rather than on the stack because a run can straddle
 * two characters.
 */
static int
v8_hs_cj(struct v8 *v, int ch)
{
	int mask = V8_HS_CHAR_TOP;
	int i;

	for (i = 0; i < V8_HS_CHAR_BITS; i++) {
		if ((ch & mask) != 0) {
			if (v->cj_zero_run == V8_HS_CJ_ZEROS) {
				v->block_count = (short)(v->block_count + 1);
				if (v->block_count == V8_HS_CJ_COUNT) {
					v->tx_state = 5;
					v->rx_state = 0x63;
					return 2;
				}
			}
			v->cj_zero_run = 0;
		} else {
			v->cj_zero_run = (short)(v->cj_zero_run + 1);
		}
		mask >>= 1;
	}
	return 0;
}

static int
v8_handshak_demod(struct v8 *v)
{
	struct v8_v21_params *p = &v->v21_params;
	int before;
	int got;
	int k;
	int sub;
	int ch;

	V8agc(v);
	before = (short)p->bitcount;
	v8_fskdemodulate(v);
	got = (short)p->bitcount - before;

	/*
	 * Walk the bits that just arrived, most recent last.  A one extends
	 * the current character; six zeros in a row end it, and a character
	 * of at least nine ones counts as received.
	 */
	for (k = 0; k < got; k++) {
		int shift = got - k - 1;

		if (((unsigned short)p->bits >> shift) & 1) {
			p->zero_run = 0;
			p->ones_run = (short)(p->ones_run + 1);
			p->ones_run_len = p->ones_run;
			continue;
		}
		p->zero_run = (short)(p->zero_run + 1);
		if (p->zero_run == V8_HS_ZERO_RUN
		    && (short)p->ones_run_len > V8_HS_MIN_ONES) {
			p->gap_seen = p->gap_count;
			p->gap_count = (short)(p->gap_count + 1);
		}
		p->ones_run = 0;
	}

	sub = (unsigned short)v->rx_substate;

	/*
	 * A character of ones went by since the last look, and it was not the
	 * first: the far end is between messages, so drop what is half
	 * assembled and start the next character six bits in.  Not done while
	 * hunting for CJ, which is all zeros and would never survive it.
	 */
	if ((unsigned short)p->gap_seen != (unsigned short)p->gap_count
	    && (short)(p->gap_count - 1) > 0 && sub != V8_HS_CJ) {
		p->bitcount = V8_HS_ZERO_RUN;
		p->bits = 0;
		p->gap_seen = (short)(p->gap_seen + 1);
	}

	/* Two sub-states read the raw stream and so run every block. */
	if (sub == V8_HS_HUNT)
		return v8_hs_hunt(v);
	if (sub == V8_HS_DRAIN)
		return v8_hs_drain(v);

	/* The rest wait for a whole character. */
	if ((short)p->bitcount != V8_HS_CHAR_BITS)
		return 0;
	ch = (short)p->bits & V8_HS_CHAR_MASK;
	p->bitcount = 0;

	if (sub == V8_HS_COLLECT)
		return v8_hs_collect(v, ch);
	if (sub == V8_HS_QCA1)
		return v8_hs_qca1(v, ch);
	if ((unsigned short)(sub - V8_HS_TAKEN_RX) <= 1)
		return 0;
	return v8_hs_cj(v, ch);
}

/*
 * Run the handshake for one block.
 *
 * Two state variables, one per direction:
 *
 *   `tx_state` drives the transmitter and is dispatched inside a loop that
 *   runs until the transmit queue is full, so one call does as much work as
 *   the queue has room for.
 *
 *   `rx_state` drives the receiver and is dispatched once, after that loop
 *   ends and only if at least six symbols have arrived.
 *
 * The return value is 0 normally, 1 when a deadline expired, and 2 when the
 * handshake finished -- which is what `V8Process` reads as "something
 * changed".
 *
 * The two long receive paths are `static` functions in this file, as the
 * object has them: `-O3` inlines the whole chain into this body, which is
 * why the object defines no symbols for them and why this function is
 * four kilobytes rather than one.
 */
int
v8handshak(struct v8 *v)
{
	struct v8_rx *r = &v->rx;
	int done = 0;
	int rc;

	/*
	 * Transmit until the queue is full.  The comparison is signed, and
	 * `tx_avail` does go negative -- `V8Process` decrements it once a
	 * sample whatever the queue is doing -- so this keeps transmitting
	 * where an unsigned one would stop.
	 */
	while ((short)v->tx_avail < (short)v->tx_fill_target) {
		int st = (short)v->tx_state - 5;

		if ((unsigned)st > 0x28)
			continue;
		rc = transmit(v, &done);
		if (done)
			return rc;
	}

	/* Then the receiver, once, and only with something to look at. */
	if ((short)v->sym_avail <= 5)
		return 0;

	switch ((short)v->rx_state) {
	case V8_RX_DRAIN:
		v8_rxreadqueue(v);
		return 0;

	case V8_RX_DONE:
		return 2;

	case V8_RX_SETTLE:
		V8agc(v);
		v->block_count = (short)(v->block_count + 1);
		if ((short)v->block_count <= V8_RX_SETTLE_BLOCKS)
			return 0;
		v->rx_state = V8_RX_DEMOD;
		v->rx_substate = V8_HS_HUNT;
		r->adapt_rate = 0x800;
		v->block_count = 0;
		return 0;

	case V8_RX_AGC:
		return v8_handshak_agc(v);

	case V8_RX_DEMOD:
		return v8_handshak_demod(v);

	default:
		return 0;
	}
}
