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
 * The two long receive paths are called out to `v8hsrx.c` rather than
 * inlined here, where the object carries them inline; that extraction is
 * not part of this unit's boundary recovery and is recorded as outstanding.
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
