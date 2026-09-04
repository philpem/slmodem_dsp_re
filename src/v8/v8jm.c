/*
 * v8jm.c -- reading the answer back.
 *
 * `evaluateRxJMSequence` decides whether the JM that arrived actually answers
 * the CM that went out: the call function has to be one the menu asked for,
 * and any extension the menu declared has to come back character for
 * character.
 */

#include "dsplib/debug.h"
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
 *
 * `*matched` is the "something has matched" flag, and it belongs to the
 * caller because the two callers give it different lifetimes: the first
 * keeps it in `febc` and clears it before every marker word, the second in
 * a local that is set up once and then carries across markers -- so once
 * anything has matched there, a later marker whose very first character is
 * wrong is abandoned instead of scanned through.
 */
static int
match_extension(struct v8 *v, const unsigned char *ext, int *at, short *keep,
		int *matched)
{
	struct v8_tx_sequence *seq = &v->seq[2];
	int k = 0;

	while (ext[k] != 0 && k <= V8_CM_EXT_MAX - 1) {
		unsigned short w = (unsigned short)seq->word[*at];

		if (w == (unsigned short)ext_expected(ext[k])) {
			*keep = (short)w;
			*matched = 1;
			k++;
			(*at)++;
			continue;
		}
		if (*matched)
			break;
		k++;
	}

	/* Ran to the end of the field, rather than stopping on a mismatch. */
	if ((ext[k] == 0 || k == V8_CM_EXT_MAX) && *matched)
		return 1;
	return 0;
}

void
evaluateRxJMSequence(struct v8 *v)
{
	struct v8_tx_sequence *seq = &v->seq[2];
	struct v8_cm *cm = v->cm;
	int matched;
	int i;

	v->fn_matched = 0;

	/* The call function, and the first extension if one was declared. */
	for (i = 0; i < (short)seq->wordidx; i++) {
		unsigned short w = (unsigned short)seq->word[i];

		if ((w & V8_JM_FN_MASK) != V8_JM_FN_MARK)
			continue;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: on CALLER: remote call function is: %X\r\n", w);

		if (cm->b2 & V8_CM_EXT1_PRESENT) {
			matched = 0;
			if (match_extension(v, cm->ext1, &i, &v->fn_word,
					    &matched)) {
				v->fn_matched = 1;
				break;
			}
			v->fn_word = 0;
			v->fn_matched = 0;
		} else {
			/*
			 * No extension: the function word itself has to be
			 * one the menu asked for.  Each of the four announces
			 * itself; V.80 shares the data announcement, which is
			 * why the object has four tests and three strings.
			 */
			if (w == 0x107 && (cm->b1 & 0x40)) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V8: call function DATA "
					    "indication...\r\n");
				v->fn_matched = 1;
			} else if (w == 0x103 && (cm->b2 & 0x01)) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V8: call function FAX TX from "
					    "caller indication...\r\n");
				v->fn_matched = 1;
			} else if (w == 0x10b && (cm->b1 & 0x80)) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V8: call function FAX RX to "
					    "caller indication...\r\n");
				v->fn_matched = 1;
			} else if (w == 0x109 && (cm->b2 & 0x02)) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V8: call function DATA "
					    "indication...\r\n");
				v->fn_matched = 1;
			}

			if (v->fn_matched != 0)
				v->fn_word = (short)w;
		}
	}

	/*
	 * The second extension, against its own marker.  `febe` is only ever
	 * set here, never cleared: whatever the caller left in it stands if
	 * nothing matches.
	 */
	matched = 0;
	for (i = 0; i < (short)seq->wordidx; i++) {
		unsigned short w = (unsigned short)seq->word[i];

		if ((w & V8_JM_FN_MASK) != V8_JM_EXT2_MARK)
			continue;

		if ((cm->b2 & V8_CM_EXT2_PRESENT) == 0) {
			v->ext2_word = (short)w;
			v->ext2_matched = 1;
			continue;
		}
		if (match_extension(v, cm->ext2, &i, &v->ext2_word, &matched)) {
			v->ext2_matched = 1;
			break;
		}
		v->ext2_word = 0;
	}

	/*
	 * The verdict, and the only place the whole function is summarised.
	 * It reports the FIRST field only: `febe` does not appear, so a JM
	 * whose protocol matched but whose call function did not still reads
	 * as a failure here.  The parenthesis is the author's own gloss on
	 * what a failure costs -- see V8UpdateModemParameters, which does
	 * exactly that.
	 *
	 * This is why the loop above breaks rather than returning: the
	 * announcement has to be reached on the matched path too.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V8: %s Call Function Match%s!\n",
				     v->fn_matched != 0 ? "Got" : "Didn't get",
				     v->fn_matched != 0 ? ""
				      : " (not indicating modulation "
					"capabilities)!!");
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
	struct v8_tx_sequence *seq = v->side != 0 ? &v->seq[0] : &v->seq[2];
	int v90_mod = 0, digital_connection = 0, pcm_indication = 0;
	int i;

	out->b2 = (unsigned char)((out->b2 & 0xef)
				  | (((unsigned char)v->quick_connect & 1) << 4));
	out->offered = v->anspcm_level;
	out->b2 = (unsigned char)((out->b2 & 0xbf)
				  | ((v->lapm_indication != 0 && (out->b2 & 0x40)) << 6));

	if (v->quick_connect != 0) {
		/*
		 * `fdc4` set means the quick-connect sequence was used, and
		 * this is where the author says so -- which is how the field
		 * gets its meaning.  Announced before the menu is stamped.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8Report: Finished with Quick Connect\n");
		out->b0 |= 9;
		return 0;
	}

	if ((short)seq->wordidx <= 0)
		return -1;

	out->b2 &= 0xf8;
	out->ext1[0] = 0;
	out->b1 &= 0x3f;

	if (v->fn_matched != 0) {
		short fn = v->fn_word;

		if (fn == 0x107) {
			out->b1 |= 0x40;
		} else if (fn == 0x109) {
			out->b2 |= 2;
		} else if (fn == 0x10b) {
			out->b1 |= 0x80;
		} else if (fn == 0x103) {
			out->b2 |= 1;
		} else if (fn == 0) {
			/*
			 * `febc` said a function matched but nothing was
			 * remembered.  The complaint is all that happens: the
			 * walk below still runs, so the modulation list is
			 * still intersected.
			 */
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V8Report: Didn't get "
						     "matching call "
						     "function\r\n");
		} else {
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
				v90_mod = (int)(f >> 2);

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
				digital_connection = (int)((w >> 1) & 1);
			} else if (top == 0x1c) {
				pcm_indication = (int)((w >> 2) & 3);
			}
		}

		/*
		 * What the three gathered words mean.  The names are the
		 * author's, from the line below: modulation, digital
		 * connection, PCM indication -- V.90's three preconditions.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8Report: remote V90: mod - %d, digital "
			    "connection - %d, pcmIndication - %d\n",
			    v90_mod, digital_connection, pcm_indication);

		/*
		 * The three flags together decide one bit of the menu: the
		 * offer only stands if all of them agree.
		 */
		out->b0 = (unsigned char)((out->b0 & 0xf7)
			  | (((out->b0 & 8) && v90_mod != 0
			      && digital_connection != 0
			      && pcm_indication == 1) << 3));
	} else if (DSPLIB_DEBUG_ON()) {
		dsplibs_debug_printf("V8Report: since no call function match, "
				     "not indicating any modulation "
				     "capability...\r\n");
	}

	/* The second extension, if one came back that is not the filler. */
	if (v->ext2_word != 0 && v->ext2_word != 0xa9) {
		out->b2 |= 8;
		out->ext2[0] = charFlip((unsigned char)(v->ext2_word >> 1));
	}

	out->b0 |= 1;

	/*
	 * The menu as it now stands, bit for bit -- the same ten bits, in the
	 * same order, that V8Create prints for the local configuration.  The
	 * two lines side by side are what the negotiation asked for and what
	 * it settled on.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V8Report: v90:%d, v34:%d, v34hd:%d, V32:%d, V22:%d, "
		    "V17:%d, V29:%d, V27:%d, V23:%d, V21:%d\n",
		    (out->b0 >> 3) & 1, (out->b0 >> 5) & 1,
		    (out->b0 >> 6) & 1, out->b0 >> 7,
		    out->b1 & 1, (out->b1 >> 1) & 1, (out->b1 >> 2) & 1,
		    (out->b1 >> 3) & 1, (out->b1 >> 4) & 1,
		    (out->b1 >> 5) & 1);

	return 0;
}

/* Up to eight entries in each acceptance list. */
#define V8_FN_LIST_MAX	8

/*
 * Is `c` in one of the menu's acceptance lists?  The list ends at the first
 * zero or after eight entries, whichever comes first.
 */
static int
in_list(const unsigned char *list, unsigned char c)
{
	int i;

	if (list[0] == 0)
		return 0;
	for (i = 0; i <= V8_FN_LIST_MAX - 1 && list[i] != 0; i++)
		if (list[i] == c)
			return 1;
	return 0;
}

/*
 * Echo an extension field into the JM, one character per word.  Used when
 * nothing was matched against the received message and the local field is
 * simply sent as it stands.
 */
static void
emit_extension_words(struct v8_tx_sequence *jm, int *n,
		     const unsigned char *ext)
{
	int k = 0;

	while (ext[k] != 0 && k <= V8_CM_EXT_MAX - 1) {
		jm->word[*n] = ext_expected(ext[k]);
		(*n)++;
		k++;
	}
}

void
rebuildJMSequence(struct v8 *v)
{
	struct v8_tx_sequence *jm = v->tx_seq;
	struct v8_tx_sequence *rx = &v->seq[0];
	struct v8_cm *cm = v->cm;
	int n = 2;
	int base;
	int fn_matched = 0;
	int ext2_matched = 0;
	int v90_mod = 0, digital_connection = 0, pcm_indication = 0;
	int all_flags;
	int words;
	int i;

	jm->word[0] = V8_SEQ_PREAMBLE_0;
	jm->word[1] = V8_SEQ_PREAMBLE_1;

	/* Find the call function in what arrived, and try to accept it. */
	for (i = 0; i < (short)rx->wordidx; i++) {
		unsigned short w = (unsigned short)rx->word[i];
		int accept = 0;

		if ((w & V8_JM_FN_MASK) != V8_JM_FN_MARK)
			continue;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: on ANSWER: remote call function is: %X\r\n", w);

		if (cm->b2 & V8_CM_EXT1_PRESENT) {
			/* An extension stands in for the function. */
			int k = 0;

			while (cm->ext1[k] != 0 && k <= V8_CM_EXT_MAX - 1) {
				unsigned short got =
					(unsigned short)rx->word[i];

				if (got == (unsigned short)
					   ext_expected(cm->ext1[k])) {
					jm->word[n++] = (short)got;
					v->fn_word = (short)got;
					fn_matched = 1;
					i++;
					k++;
					continue;
				}
				if (fn_matched)
					break;
				k++;
			}
			/*
			 * A full extension match settles it here -- the words
			 * and the remembered character went into the JM as
			 * they matched, so all that is left is to say so.  An
			 * incomplete one falls through to the acceptance list
			 * BELOW, not to the four flag tests: those belong to
			 * the no-extension case only.
			 */
			if ((cm->ext1[k] == 0 || k == V8_CM_EXT_MAX)
			    && fn_matched) {
				v->fn_matched = 1;
				break;
			}
			w = (unsigned short)rx->word[i];
		} else if (w == 0x107 && (cm->b1 & 0x40)) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V8: call function DATA "
						     "indication...\r\n");
			accept = 1;
		} else if (w == 0x103 && (cm->b2 & 0x01)) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V8: call function FAX TX from caller "
				    "indication...\r\n");
			accept = 1;
		} else if (w == 0x10b && (cm->b1 & 0x80)) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V8: call function FAX RX to caller "
				    "indication...\r\n");
			accept = 1;
		} else if (w == 0x109 && (cm->b2 & 0x02)) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V8: call function DATA "
						     "indication...\r\n");
			accept = 1;
		}

		/* Or one the menu lists explicitly? */
		if (!accept
		    && in_list(cm->fn_list,
			       charFlip((unsigned char)(w >> 1)))) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V8: Got Call Function Match (in call "
				    "function range) !!!\r\n");
			accept = 1;
		}

		if (accept)
			fn_matched = 1;

		if (v->fn_matched == 0 && fn_matched) {
			jm->word[n++] = (short)w;
			v->fn_word = (short)w;
			v->fn_matched = 1;
		}
		break;
	}

	if (v->fn_matched == 0) {
		/*
		 * Nothing accepted: send our own field instead, or the call
		 * function the menu asks for.
		 */
		if (cm->b2 & V8_CM_EXT1_PRESENT) {
			emit_extension_words(jm, &n, cm->ext1);
		} else if (cm->b1 & 0x40) {
			jm->word[n++] = 0x107;
		} else if (cm->b2 & 0x01) {
			jm->word[n++] = 0x103;
		} else if (cm->b1 & 0x80) {
			jm->word[n++] = 0x10b;
		} else if (cm->b2 & 0x02) {
			jm->word[n++] = 0x109;
		} else {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V8: BUG - no Call Function selected in "
				    "bit fields - use data as default !!!\r\n");
			cm->b1 |= 0x40;
			jm->word[n++] = 0x107;
		}
	}

	base = n;

	if (v->fn_matched != 0) {
		/*
		 * Intersect: walk what arrived and AND its menu words into
		 * the three already in the buffer, gathering the three flags
		 * on the way.
		 */
		for (i = 0; i < (short)rx->wordidx; i++) {
			unsigned short w = (unsigned short)rx->word[i];

			if ((w & V8_JM_FN_MASK) == 0x141) {
				int at = base;

				jm->word[at] = (short)(jm->word[at] & w);
				at++;
				v90_mod = (rx->word[i] >> 3) & 1;
				i++;
				w = (unsigned short)rx->word[i];
				while ((w & 0x39) == 0x11 && at < base + 3) {
					jm->word[at] = (short)(jm->word[at]
							       & w);
					at++;
					i++;
					w = (unsigned short)rx->word[i];
				}
				n = base + 3;
			}
			if ((w & V8_JM_FN_MASK) == 0x161)
				digital_connection = (w >> 1) & 1;
			else if ((w & V8_JM_FN_MASK) == 0x1c1)
				pcm_indication = (w >> 2) & 3;
		}

		/*
		 * The three gathered words, under the author's names for them
		 * -- the same three V8UpdateModemParameters reports -- with
		 * the local V.90 bit beside them, so the trace shows both
		 * halves of the decision immediately below.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: on ANSWER: remote V90: mod - %d, digital "
			    "connection - %d, pcmIndication - %d , local - "
			    "%d\n", v90_mod, digital_connection,
			    pcm_indication, (cm->b0 >> 3) & 1);
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: NO Call Function Match !!! zeroing all "
			    "modulation capabilities...\r\n");
		jm->word[n] = 0x141;
		jm->word[n + 1] = 0x011;
		jm->word[n + 2] = 0x011;
		n += 3;
	}

	/*
	 * V.90 is on offer only if the remote said all three and we asked for
	 * it.  The object recomputes this expression at each of its four uses
	 * rather than keeping it in a register -- `cm` is memory and the
	 * stores to `jm->word[]` in between might alias it, as far as the
	 * compiler knows -- so the value is the same at every use here.
	 */
	all_flags = (cm->b0 & 8) && v90_mod != 0 && digital_connection != 0
		    && pcm_indication == 1;

	if (all_flags) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V8: on ANSWER: rebuilding JM "
					     "with V90 capabilities...\r\n");
		jm->word[base] |= 8;
	}

	/*
	 * The second extension, and the two shapes it takes.
	 *
	 * With V.90 on offer there is only a local field to confirm: the scan
	 * walks every marker word looking for it, echoes what matches into
	 * the JM, and leaves the remembered character alone.  Nothing else --
	 * no acceptance list, no filler, and nothing at all when there is no
	 * local field.
	 *
	 * Without it the scan stops at the first marker word and settles the
	 * question there, one way or the other, and what it settles on is
	 * remembered in `fec2`.
	 */
	if (all_flags) {
		if (cm->b2 & V8_CM_EXT2_PRESENT) {
			for (i = 0; i < (short)rx->wordidx; i++) {
				unsigned short w = (unsigned short)rx->word[i];
				int k = 0;

				if ((w & V8_JM_FN_MASK) != V8_JM_EXT2_MARK)
					continue;

				while (cm->ext2[k] != 0
				       && k <= V8_CM_EXT_MAX - 1) {
					unsigned short got = (unsigned short)
							     rx->word[i];

					if (got == (unsigned short)
						   ext_expected(cm->ext2[k])) {
						jm->word[n++] = (short)got;
						ext2_matched = 1;
						i++;
					} else if (ext2_matched) {
						break;
					}
					k++;
				}
				if ((cm->ext2[k] == 0 || k == V8_CM_EXT_MAX)
				    && ext2_matched) {
					v->ext2_matched = 1;
					break;
				}
			}
		}
	} else {
		for (i = 0; i < (short)rx->wordidx; i++) {
			unsigned short w = (unsigned short)rx->word[i];
			int k = 0;

			if ((w & V8_JM_FN_MASK) != V8_JM_EXT2_MARK)
				continue;

			if (cm->b2 & V8_CM_EXT2_PRESENT) {
				while (cm->ext2[k] != 0
				       && k <= V8_CM_EXT_MAX - 1) {
					unsigned short got = (unsigned short)
							     rx->word[i];

					if (got == (unsigned short)
						   ext_expected(cm->ext2[k])) {
						jm->word[n++] = (short)got;
						v->ext2_word = (short)got;
						ext2_matched = 1;
						i++;
					} else if (ext2_matched) {
						break;
					}
					k++;
				}
				if ((cm->ext2[k] == 0 || k == V8_CM_EXT_MAX)
				    && ext2_matched) {
					v->ext2_matched = 1;
					break;
				}
				v->ext2_word = 0;
				/*
				 * Not a `continue`: a local field that failed
				 * to match still gets the acceptance list
				 * tried against the same word below.
				 */
			} else if (cm->ext_list[0] == 0) {
				/*
				 * No field and no list, so only the filler
				 * will do -- and the object tests the list
				 * BEFORE reaching for charFlip, which is what
				 * puts this branch here rather than after
				 * in_list has had its say.
				 */
				if (w == V8_SEQ_TAIL_A) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "V8: Got Default Protocol "
						    "Match (LAPM) !!!\r\n");
					ext2_matched = 1;
				}
			}

			if (!ext2_matched
			    && in_list(cm->ext_list,
				       charFlip((unsigned char)(w >> 1)))) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "V8: Got Protocol Match (in "
					    "protocol range) !!!\r\n");
				ext2_matched = 1;
			}

			if (v->ext2_matched == 0 && ext2_matched) {
				jm->word[n++] = (short)w;
				v->ext2_word = (short)w;
				v->ext2_matched = 1;
			}
			break;
		}

		if (v->ext2_matched != 0) {
			/* Already settled by the scan above. */
		} else if (cm->b2 & V8_CM_EXT2_PRESENT) {
			/*
			 * Nothing matched, but we have a field of our own:
			 * send it, and remember the last word of it.
			 */
			int k = 0;

			while (cm->ext2[k] != 0 && k <= V8_CM_EXT_MAX - 1) {
				jm->word[n] = ext_expected(cm->ext2[k]);
				v->ext2_word = jm->word[n];
				n++;
				k++;
			}
		} else {
			/* Nothing to send either: the filler. */
			jm->word[n++] = V8_SEQ_TAIL_A;
			v->ext2_word = V8_SEQ_TAIL_A;
		}
	}

	/* The tail. */
	jm->word[n] = V8_SEQ_TAIL_B;
	words = n + 1;
	if (all_flags) {
		jm->word[n + 1] = V8_SEQ_TAIL_C;
		jm->word[n + 2] = V8_SEQ_TAIL_D;
		words = n + 3;
	}

	/*
	 * "octets" again, as in initTxSequence, and again counting ten-bit
	 * characters.  Announced after the tail is counted and before any of
	 * the sequence state is written, so the number is the one that will
	 * be transmitted.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V8: Final JM message length is %d octets\r\n", words);

	jm->crc = (short)0xffff;
	jm->bitpos = 0;
	jm->wordidx = 0;
	jm->repeats = 0;
	jm->nbits = (short)(words * V8_SEQ_BITS_PER_WORD);
	jm->wordbits = V8_SEQ_BITS_PER_WORD;
	jm->crc_enable = 0;
	jm->shifter = 0;
	jm->shifter0 = 0;
	jm->nleft = 0;
	jm->nleft0 = 0;
	jm->repeat = 1;
}
