/*
 * v8seq.c -- building the CM and JM sequences.
 *
 * This is where V.8's negotiation becomes bits.  `initTxSequence` reads the
 * call menu at `v->cm` -- three flag bytes and two optional four-character
 * extensions -- and writes the message out as 10-bit V.21 characters into
 * `v->tx_seq`, then records how many bits that came to.
 *
 * One function serves both CM and JM: the caller points `v->cm` and
 * `v->tx_seq` at whichever pair it wants before calling, which is why
 * `v8handshakinit` calls it twice with different pointers.
 *
 * Every constant below is a 10-bit character as it goes on the wire, which is
 * why they look like 0x107 rather than an octet: the framing is already in
 * them.  `nbits` being the character count times ten is the confirmation.
 */

#include "dsplib/v8.h"

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

/*
 * One extension character.  The octet is reversed -- V.8 goes least
 * significant bit first -- shifted up one and given a low bit, which is the
 * framing the fixed constants above already carry.
 */
static short
ext_word(unsigned char c)
{
	return (short)((charFlip(c) << 1) | 1);
}

/*
 * Copy up to four characters of an extension field, stopping at the first
 * zero.  Returns how many were emitted; the caller clears the field's
 * present bit when that is none, which is how a field declared present but
 * left empty stops being declared.
 */
static int
emit_extension(struct v8_tx_sequence *seq, int *n, const unsigned char *ext)
{
	int k = 0;

	while (ext[k] != 0) {
		seq->word[*n] = ext_word(ext[k]);
		(*n)++;
		k++;
		if (k > V8_CM_EXT_MAX - 1)
			break;
	}
	return k;
}

void
initTxSequence(struct v8 *v)
{
	struct v8_tx_sequence *seq = v->tx_seq;
	struct v8_cm *cm = v->cm;
	int n = 2;
	int words;

	seq->word[0] = V8_SEQ_PREAMBLE_0;
	seq->word[1] = V8_SEQ_PREAMBLE_1;

	/* The first extension, if the menu says there is one. */
	if (cm->b2 & V8_CM_EXT1_PRESENT) {
		if (emit_extension(seq, &n, cm->ext1) == 0)
			cm->b2 &= (unsigned char)~V8_CM_EXT1_PRESENT;
	}

	/*
	 * The call function.  Note the order: the extension bit is retested
	 * here, so clearing it just above changes which branch is taken.
	 */
	if (cm->b2 & V8_CM_EXT1_PRESENT) {
		/* nothing: the extension stood in for the function character */
	} else if (cm->b1 & 0x40) {
		seq->word[n++] = V8_SEQ_FN_DEFAULT;
	} else if (cm->b2 & 0x01) {
		seq->word[n++] = V8_SEQ_FN_B0;
	} else if (cm->b1 & 0x80) {
		seq->word[n++] = V8_SEQ_FN_B1_80;
	} else if (cm->b2 & 0x02) {
		seq->word[n++] = V8_SEQ_FN_B2;
	} else {
		/* Nothing asked for, so ask for the default and remember it. */
		cm->b1 |= 0x40;
		seq->word[n++] = V8_SEQ_FN_DEFAULT;
	}

	/*
	 * Three characters carrying the menu proper.  Each has a base chosen
	 * by one bit and then further bits folded in, which is the modulation
	 * list and the capability flags packed into V.8's fields.
	 */
	seq->word[n] = (short)(((cm->b0 & 0x08) ? 0x149 : 0x141)
			       | ((cm->b0 & 0x20) ? 0x04 : 0)
			       | ((cm->b0 & 0x40) ? 0x02 : 0));

	seq->word[n + 1] = (short)(((cm->b0 & 0x80) ? 0x111 : 0x011)
				   | ((cm->b1 & 0x01) ? 0x80 : 0)
				   | ((cm->b1 & 0x02) ? 0x40 : 0)
				   | ((cm->b1 & 0x04) ? 0x04 : 0)
				   | ((cm->b1 & 0x08) ? 0x02 : 0));

	seq->word[n + 2] = (short)(((cm->b1 & 0x10) ? 0x51 : 0x11)
				   | ((cm->b1 & 0x20) ? 0x13 : 0));
	n += 3;

	/* The second extension, on the same terms as the first. */
	if (cm->b2 & V8_CM_EXT2_PRESENT) {
		if (emit_extension(seq, &n, cm->ext2) == 0)
			cm->b2 &= (unsigned char)~V8_CM_EXT2_PRESENT;
	}

	/*
	 * The tail.  The first character is skipped when either of two bits
	 * is set -- the original tests them as one 32-bit read across the
	 * flag bytes, which is the same as testing bit 3 of b0 and bit 3
	 * of b2.
	 */
	if ((cm->b0 & 0x08) == 0 && (cm->b2 & 0x08) == 0)
		seq->word[n++] = V8_SEQ_TAIL_A;

	seq->word[n] = V8_SEQ_TAIL_B;
	words = n + 1;

	if (cm->b0 & 0x08) {
		seq->word[n + 1] = V8_SEQ_TAIL_C;
		seq->word[n + 2] = V8_SEQ_TAIL_D;
		words = n + 3;
	}

	seq->crc = (short)0xffff;

	seq->bitpos = 0;
	seq->wordidx = 0;
	seq->repeats = 0;
	seq->nbits = (short)(words * V8_SEQ_BITS_PER_WORD);
	seq->wordbits = V8_SEQ_BITS_PER_WORD;
	seq->crc_enable = 0;
	seq->shifter = 0;
	seq->shifter0 = 0;
	seq->nleft = 0;
	seq->nleft0 = 0;
	seq->repeat = 1;
}

/*
 * Which of the five buffers holds what was received.  Three cases, and the
 * middle one is the reason the object keeps a spare pointer at all: once
 * `fdc4` is set the handshake has moved on and the message lives wherever
 * that pointer says, rather than at a fixed place.
 */
static const struct v8_tx_sequence *
rx_sequence(const struct v8 *v)
{
	if (v->fdc4 != 0)
		return v->seq_spare;
	if (v->mode != 0)
		return &v->seq[0];
	return &v->seq[2];
}

int
V8GetMessage(struct v8 *v, unsigned char *out, int *count)
{
	const struct v8_tx_sequence *seq = rx_sequence(v);
	int n = seq->wordidx;
	int rc = 0;
	int i;

	if (n <= 0)
		return V8_GET_EMPTY;

	/*
	 * Too long for the caller's buffer: fill what fits and hand back the
	 * length it would have needed, which is how truncation is told apart
	 * from a message that was simply this short.
	 */
	if (n > *count) {
		rc = n;
		n = *count;
	}

	for (i = 0; i < n; i++)
		out[i] = charFlip((unsigned char)(seq->word[i] >> 1));

	*count = n;
	return rc;
}

/*
 * Which buffer a selector names.  The order is not the order they sit in
 * memory: selector 1 is the third buffer and selector 2 the second.  Kept as
 * the original has it -- guessing that the swap is a mistake and "fixing" it
 * would put messages in the wrong place.
 */
static struct v8_tx_sequence *
selected_sequence(struct v8 *v, int which)
{
	switch (which) {
	case V8_SET_CM:	return &v->seq[0];
	case V8_SET_JM:	return &v->seq[2];
	case V8_SET_CJ:	return &v->seq[1];
	case V8_SET_CI:	return &v->seq[3];
	default:	return 0;
	}
}

int
V8SetMessage(struct v8 *v, int which, const unsigned char *octets, int n)
{
	struct v8_tx_sequence *seq = selected_sequence(v, which);
	int rc = 0;
	int i;

	if (seq == 0)
		return V8_SET_REJECTED;
	if (n == 0)
		return V8_SET_REJECTED;

	if (n > V8_TX_SEQ_WORDS) {
		n = V8_TX_SEQ_WORDS;
		rc = V8_SET_TRUNCATED;
	}

	for (i = 0; i < n; i++)
		seq->word[i] = ext_word(octets[i]);

	seq->crc = (short)0xffff;
	seq->bitpos = 0;
	seq->wordidx = 0;
	seq->repeats = 0;
	seq->nbits = (short)(n * V8_SEQ_BITS_PER_WORD);
	seq->wordbits = V8_SEQ_BITS_PER_WORD;
	seq->crc_enable = 0;
	seq->shifter = 0;
	seq->shifter0 = 0;
	seq->nleft = 0;
	seq->nleft0 = 0;
	seq->repeat = 1;

	return rc;
}

/*
 * Hand out the next bit of a sequence, least significant first.
 *
 * The sequence is 10-bit characters; this is what turns them into a bit
 * stream. A shift register holds whatever has been loaded and not yet handed
 * out, `nleft` says how much of it is still owed, and each load folds the new
 * bits into the CRC.
 *
 * Three things happen at the end of a message, in this order: the finished
 * CRC is appended as sixteen more bits, then four more bits of ones, and only
 * then does the sequence either repeat from the top or report that it is
 * done.  The two overruns are recognised by how far past the length the bit
 * position has gone, which is why `remaining` is allowed to go negative
 * rather than being clamped.
 */
int
v8_getbit(struct v8_tx_sequence *s)
{
	int remaining;
	int loaded = 0;
	int pos;

	if (s->nleft != 0)
		goto emit;

	pos = (unsigned short)s->bitpos;
	remaining = (short)((unsigned short)s->nbits - (unsigned short)pos);

	if (remaining <= 0) {
		if (remaining == 0 && s->crc_enable != 0) {
			/* The CRC itself, sixteen bits of it. */
			s->shifter = (unsigned short)s->crc;
			s->nleft = 16;
			s->bitpos = (short)(pos + 16);
			goto emit;
		}
		if (remaining == -16) {
			/* Four ones behind the CRC. */
			s->shifter = 0xf;
			s->nleft = 4;
			s->bitpos = (short)(pos + 4);
			goto emit;
		}
		if (s->repeat == 0)
			return V8_GETBIT_END;

		s->crc = (short)0xffff;
		s->bitpos = 0;
		s->wordidx = 0;
		s->repeats = (short)(s->repeats + 1);
		s->shifter = s->shifter0;
		s->nleft = s->nleft0;
		return (short)v8_getbit(s);
	}

	/*
	 * Load a whole character, or whatever is left of one when the message
	 * ends part way through.
	 */
	loaded = (unsigned short)s->wordbits;
	if (loaded > remaining)
		loaded = remaining;

	s->nleft = (short)loaded;
	s->shifter = (s->shifter << loaded)
		     | (unsigned short)s->word[(unsigned short)s->wordidx];
	if ((unsigned short)s->wordbits <= (unsigned)remaining)
		s->wordidx = (short)(s->wordidx + 1);
	s->bitpos = (short)(pos + loaded);

	if (s->crc_enable != 0 && loaded != 0) {
		int c = loaded;

		do {
			unsigned crc = (unsigned short)s->crc;
			int msb = (short)crc < 0 ? 1 : 0;

			c--;
			if ((s->shifter >> c) & 1)
				msb ^= 1;
			crc += crc;
			if (msb)
				crc ^= 0x1021;
			s->crc = (short)crc;
		} while (c != 0);
	}

emit:
	s->nleft = (short)(s->nleft - 1);
	return (s->shifter >> (short)s->nleft) & 1;
}
