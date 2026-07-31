/*
 * v8jm.c -- reading the answer back.
 *
 * `evaluateRxJMSequence` decides whether the JM that arrived actually answers
 * the CM that went out: the call function has to be one the menu asked for,
 * and any extension the menu declared has to come back character for
 * character.
 */

#include "dsplib/v8.h"

/* One extension character as it appears on the wire. */
static short
ext_expected(unsigned char c)
{
	return (short)((charFlip(c) << 1) | 1);
}

/*
 * Match an extension field against the words from `*at` onwards.
 *
 * Returns non-zero if anything matched.  On a mismatch the expected
 * character advances but the received word does not, so a repeated word
 * still lines up; once something has matched, the first mismatch ends it.
 */
static int
match_extension(struct v8 *v, const unsigned char *ext, int *at, short *keep)
{
	struct v8_tx_sequence *seq = &v->seq[2];
	int matched = 0;
	int k = 0;

	while (ext[k] != 0 && k <= V8_CM_EXT_MAX - 1) {
		unsigned short w = (unsigned short)seq->word[*at];

		if (w == (unsigned short)ext_expected(ext[k])) {
			*keep = (short)w;
			matched = 1;
			k++;
			(*at)++;
			continue;
		}
		if (matched)
			break;
		k++;
	}

	/* Ran to the end of the field, rather than stopping on a mismatch. */
	if ((ext[k] == 0 || k == V8_CM_EXT_MAX) && matched)
		return 1;
	return 0;
}

void
evaluateRxJMSequence(struct v8 *v)
{
	struct v8_tx_sequence *seq = &v->seq[2];
	struct v8_cm *cm = v->cm;
	int i;

	v->febc = 0;

	/* The call function, and the first extension if one was declared. */
	for (i = 0; i < (short)seq->wordidx; i++) {
		unsigned short w = (unsigned short)seq->word[i];

		if ((w & V8_JM_FN_MASK) != V8_JM_FN_MARK)
			continue;

		if (cm->b2 & V8_CM_EXT1_PRESENT) {
			if (match_extension(v, cm->ext1, &i, &v->fec0)) {
				v->febc = 1;
				break;
			}
			v->fec0 = 0;
			v->febc = 0;
		} else {
			/*
			 * No extension: the function word itself has to be
			 * one the menu asked for.
			 */
			if (w == 0x107 && (cm->b1 & 0x40))
				v->febc = 1;
			else if (w == 0x103 && (cm->b2 & 0x01))
				v->febc = 1;
			else if (w == 0x10b && (cm->b1 & 0x80))
				v->febc = 1;
			else if (w == 0x109 && (cm->b2 & 0x02))
				v->febc = 1;

			if (v->febc != 0)
				v->fec0 = (short)w;
		}
	}

	/* The second extension, against its own marker. */
	v->febe = 0;
	for (i = 0; i < (short)seq->wordidx; i++) {
		unsigned short w = (unsigned short)seq->word[i];

		if ((w & V8_JM_FN_MASK) != V8_JM_EXT2_MARK)
			continue;

		if ((cm->b2 & V8_CM_EXT2_PRESENT) == 0) {
			v->fec2 = (short)w;
			v->febe = 1;
			continue;
		}
		if (match_extension(v, cm->ext2, &i, &v->fec2)) {
			v->febe = 1;
			return;
		}
		v->fec2 = 0;
	}
}

/*
 * Turn a received sequence into a call menu.
 *
 * The words carry their meaning in their top nibble: 0x14 opens the
 * modulation list, 0x16 and 0x1c carry two more flags, and a word whose bit 4
 * is set says another word of the same group follows.  Everything the menu
 * does NOT offer is cleared, so the result is the intersection of what was
 * asked for and what came back.
 */
int
V8UpdateModemParameters(struct v8 *v, struct v8_cm *out)
{
	struct v8_tx_sequence *seq = v->mode != 0 ? &v->seq[0] : &v->seq[2];
	int flag_a = 0, flag_b = 0, flag_c = 0;
	int i;

	out->b2 = (unsigned char)((out->b2 & 0xef)
				  | (((unsigned char)v->fdc4 & 1) << 4));
	out->offered = v->fdcc;
	out->b2 = (unsigned char)((out->b2 & 0xbf)
				  | ((v->fdc8 != 0 && (out->b2 & 0x40)) << 6));

	if (v->fdc4 != 0) {
		out->b0 |= 9;
		return 0;
	}

	if ((short)seq->wordidx <= 0)
		return -1;

	out->b2 &= 0xf8;
	out->ext1[0] = 0;
	out->b1 &= 0x3f;

	if (v->febc != 0) {
		short fn = v->fec0;

		if (fn == 0x107) {
			out->b1 |= 0x40;
		} else if (fn == 0x109) {
			out->b2 |= 2;
		} else if (fn == 0x10b) {
			out->b1 |= 0x80;
		} else if (fn == 0x103) {
			out->b2 |= 1;
		} else if (fn != 0) {
			/* Something else: keep it as an extension octet. */
			out->b2 |= 4;
			out->ext1[0] = charFlip((unsigned char)(fn >> 1));
		}

		/* Walk the sequence for the modulation list and its flags. */
		for (i = 0; i < (short)(unsigned short)seq->wordidx; i++) {
			unsigned w = (unsigned short)seq->word[i];
			unsigned top = w >> 4;

			if (top == 0x14) {
				unsigned f = (w & 0xf) >> 1;

				if ((f & 2) == 0)
					out->b0 &= 0xdf;
				if ((f & 1) == 0)
					out->b0 &= 0xbf;
				flag_a = (int)(f >> 2);

				if (((unsigned short)seq->word[i + 1] & 0x10)
				    == 0)
					continue;
				i++;
				f = (unsigned short)seq->word[i] >> 1;
				if ((f & 0x01) == 0)
					out->b1 &= 0xf7;
				if ((f & 0x02) == 0)
					out->b1 &= 0xfb;
				if ((f & 0x20) == 0)
					out->b1 &= 0xfd;
				if ((f & 0x40) == 0)
					out->b1 &= 0xfe;
				if ((f & 0x80) == 0)
					out->b0 &= 0x7f;

				if (((unsigned short)seq->word[i + 1] & 0x10)
				    == 0)
					continue;
				i++;
				f = (unsigned short)seq->word[i];
				if ((f & 0x40) == 0)
					out->b1 &= 0xef;
				if ((f & 3) == 0)
					out->b1 &= 0xdf;
			} else if (top == 0x16) {
				flag_b = (int)((w >> 1) & 1);
			} else if (top == 0x1c) {
				flag_c = (int)((w >> 2) & 3);
			}
		}

		/*
		 * The three flags together decide one bit of the menu: the
		 * offer only stands if all of them agree.
		 */
		out->b0 = (unsigned char)((out->b0 & 0xf7)
			  | (((out->b0 & 8) && flag_a != 0 && flag_b != 0
			      && flag_c == 1) << 3));
	}

	/* The second extension, if one came back that is not the filler. */
	if (v->fec2 != 0 && v->fec2 != 0xa9) {
		out->b2 |= 8;
		out->ext2[0] = charFlip((unsigned char)(v->fec2 >> 1));
	}

	out->b0 |= 1;
	return 0;
}
