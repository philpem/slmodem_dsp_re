/*
 * v8hsrx.c -- the handshake's two long receive paths.
 *
 * Split out of v8handshak.c because between them they are most of that
 * function's four kilobytes and neither shares anything with the transmit
 * side except the object.
 *
 * `v8_handshak_agc` waits for the line to settle and then for a tone.
 *
 * `v8_handshak_demod` turns the demodulator's bits into characters and
 * matches them.  What it matches against is `f9d8`, a sub-state below the
 * receive state, and each value gets its own function below.
 */

#include "dsplib/debug.h"
#include "dsplib/v8.h"

/* How long each wait runs before it gives up. */
#define V8_AGC_BLOCKS		0x960

/* Consecutive zero bits that end a character. */
#define V8_HS_ZERO_RUN		6

/* Bits a character must be at least this long to count. */
#define V8_HS_MIN_ONES		9

int
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
					    "V8: Time Out Waiting For "
					    "ANSam...\r\n");
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
		v->fdb4 = 0;
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
 * on which side this is and on `fa48`; only the two answering arms rebuild
 * the JM and reset the transmitter with it.
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
		v->fdb4 = 0;
		v->word_count = 0;
		v->tx_bit = 1;
		v->tx_seq->shifter = 0;
		v->tx_seq->nleft = 0;
	}

	r->flags |= V8_RX_DETECTOR_ARMED;
	v->fa40 = r->gain;
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
	 * `fdbc == 15` looks at `crc`, and the matching path raises `fdbc`
	 * with no cap at all.  Reproduced, but read through a view of the
	 * whole sequence object so that it stays a defined access here.  It
	 * only runs away on a stream that never sends the marker again and
	 * whose characters go on matching the transmit fields past the array;
	 * a well-formed message resets `fdbc` to 1 every fifteen words.
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

int
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
