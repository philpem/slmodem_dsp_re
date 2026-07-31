/*
 * t_v8jm.c -- differential test of the JM validator.
 *
 * The received sequence is built by the encoder from a menu, so the words
 * look like a real JM rather than random shorts, and then the menu is varied
 * independently of it -- which is the case that matters, since the whole
 * point of the function is deciding whether the two agree.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v8.h"

extern void ref_evaluateRxJMSequence(struct v8 *v);
extern void ref_initTxSequence(struct v8 *v);

static struct v8 obj_a, obj_b;
static struct v8_cm cm_a, cm_b;

int
main(void)
{
	int rc = 0;
	unsigned b0, b1, b2;
	int ext, k;
	long matched = 0, rejected = 0, second = 0;

	diff_begin("evaluateRxJMSequence");

	for (b1 = 0; b1 < 256; b1 += 3) {
		for (b2 = 0; b2 < 32; b2++) {
			for (ext = 0; ext < 4; ext++) {
				b0 = (b1 * 7 + b2) & 0xff;

				memset(&obj_a, 0, sizeof(obj_a));
				memset(&obj_b, 0, sizeof(obj_b));
				memset(&cm_a, 0, sizeof(cm_a));
				cm_a.b0 = (unsigned char)b0;
				cm_a.b1 = (unsigned char)b1;
				cm_a.b2 = (unsigned char)b2;
				if (ext & 1) {
					cm_a.ext1[0] = 'G';
					cm_a.ext1[1] = 'B';
				}
				if (ext & 2) {
					cm_a.ext2[0] = 'Z';
					cm_a.ext2[1] = '1';
					cm_a.ext2[2] = '9';
				}
				memcpy(&cm_b, &cm_a, sizeof(cm_a));

				/*
				 * Build a plausible JM into the buffer the
				 * validator reads, from a menu that may or
				 * may not be the one it is checked against.
				 */
				obj_a.cm = &cm_a;
				obj_b.cm = &cm_b;
				obj_a.tx_seq = &obj_a.seq[2];
				obj_b.tx_seq = &obj_b.seq[2];
				ref_initTxSequence(&obj_a);
				initTxSequence(&obj_b);
				obj_a.seq[2].wordidx =
					(short)(obj_a.seq[2].nbits / 10);
				obj_b.seq[2].wordidx = obj_a.seq[2].wordidx;

				/* Now perturb one side's menu, not the words. */
				if ((b2 & 3) == 3) {
					cm_a.ext1[0] = 'X';
					cm_b.ext1[0] = 'X';
				}

				ref_evaluateRxJMSequence(&obj_a);
				evaluateRxJMSequence(&obj_b);

				diff_eq_int("febc (%ld)", obj_b.febc,
					    obj_a.febc, (long)b1);
				diff_eq_int("febe (%ld)", obj_b.febe,
					    obj_a.febe, (long)b1);
				diff_eq_int("fec0 (%ld)", obj_b.fec0,
					    obj_a.fec0, (long)b1);
				diff_eq_int("fec2 (%ld)", obj_b.fec2,
					    obj_a.fec2, (long)b1);
				diff_eq_int("menu untouched (%ld)",
					    memcmp(&cm_a, &cm_b,
						   sizeof(cm_a)) == 0, 1,
					    (long)b1);
				for (k = 0; k < V8_TX_SEQ_WORDS; k++)
					diff_eq_int("word %ld untouched",
						    obj_b.seq[2].word[k],
						    obj_a.seq[2].word[k], k);

				if (obj_a.febc)
					matched++;
				else
					rejected++;
				if (obj_a.febe)
					second++;
			}
		}
	}

	/*
	 * Anti-vacuity: the validator must have both accepted and rejected,
	 * or it agreed with itself about one answer.
	 */
	diff_eq_int("JMs were accepted (%ld)", matched > 0, 1, matched);
	diff_eq_int("JMs were rejected (%ld)", rejected > 0, 1, rejected);
	diff_eq_int("the second field matched (%ld)", second > 0, 1, second);

	rc |= diff_end();
	return rc;
}
