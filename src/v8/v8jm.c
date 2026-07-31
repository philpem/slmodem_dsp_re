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
