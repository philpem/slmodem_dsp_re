/*
 * t_v8jm.c -- differential test of the JM validator.
 *
 * The received sequence is built by the encoder from a menu, so the words
 * look like a real JM rather than random shorts, and then the menu is varied
 * independently of it -- which is the case that matters, since the whole
 * point of the function is deciding whether the two agree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v8.h"

extern unsigned int ref_dsplibs_debug_level;

extern void ref_evaluateRxJMSequence(struct v8 *v);
extern void ref_initTxSequence(struct v8 *v);
extern int ref_V8UpdateModemParameters(struct v8 *v, struct v8_cm *out);
extern void ref_rebuildJMSequence(struct v8 *v);

static struct v8 obj_a, obj_b;
static struct v8_cm cm_a, cm_b;

/*
 * Hand-built received sequences.
 *
 * The block above builds what arrives with the encoder, which is realistic
 * but blind in three ways: the encoder never emits a word carrying the
 * second-extension marker other than the filler, never emits one of the
 * capability words with `pcmIndication` set to 1, and the acceptance lists it
 * is given hold characters no encoded word can ever match.  So the list
 * searches, the V.90 path and the extension matcher were all inert.
 *
 * A word on the wire is (charFlip(c) << 1) | 1, so an acceptance list is
 * searched for the plain character: 0xc1 matches the word 0x107 and 0x2a
 * matches the filler 0x0a9.  An extension character travels the other way --
 * '*' (0x2a) is sent as 0x0a9 and 'J' (0x4a) as 0x0a5 -- and only characters
 * that land in 0x0a1..0x0af carry the marker at all.
 */
#define W_PRE0	0x3ff
#define W_PRE1	0x00f

struct rebuild_case {
	const char	*name;
	unsigned char	b0, b1, b2;
	unsigned char	ext1_0, ext2_0, ext2_1;
	unsigned char	fn_list_0, ext_list_0;
	short		febc, febe;
	short		rx[12];
};

static const struct rebuild_case rebuild_cases[] = {
	{ "call function 0x107 by flag", 0x00, 0x40, 0x00, 0, 0, 0, 0, 0, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	{ "call function 0x109 by flag", 0x00, 0x00, 0x02, 0, 0, 0, 0, 0, 0, 0,
	  { W_PRE0, W_PRE1, 0x109, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	{ "call function 0x103 by flag", 0x00, 0x00, 0x01, 0, 0, 0, 0, 0, 0, 0,
	  { W_PRE0, W_PRE1, 0x103, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	{ "call function 0x10b by flag", 0x00, 0x80, 0x00, 0, 0, 0, 0, 0, 0, 0,
	  { W_PRE0, W_PRE1, 0x10b, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	{ "call function by list", 0x00, 0x00, 0x00, 0, 0, 0, 0xc1, 0, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	{ "no call function anywhere", 0x00, 0x00, 0x00, 0, 0, 0, 0, 0, 0, 0,
	  { W_PRE0, W_PRE1, 0x105, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	{ "default protocol (LAPM)", 0x00, 0x40, 0x00, 0, 0, 0, 0, 0, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	{ "protocol by list", 0x00, 0x40, 0x00, 0, 0, 0, 0, 0x2a, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	{ "list present but no match", 0x00, 0x40, 0x00, 0, 0, 0, 0, 0x11, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	{ "extension fails, list catches", 0x00, 0x40, 0x08, 0, '*', 0, 0, 0x4a,
	  0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x141, 0x011, 0x011, 0x0a5, 0x161, 0 } },
	{ "extension matches in full", 0x00, 0x40, 0x08, 0, '*', 'J', 0, 0, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x141, 0x011, 0x011, 0x0a9, 0x0a5, 0x161,
	    0 } },
	{ "second already settled", 0x00, 0x40, 0x08, 0, '*', 0, 0, 0, 0, 1,
	  { W_PRE0, W_PRE1, 0x107, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	/*
	 * The three flags all agree only if no later word overwrites one, so
	 * the capability words come last: a trailing 0x161 would put
	 * `digital connection` back to zero and the V.90 path would never be
	 * taken.  That is what made the first attempt at these cases pass.
	 */
	{ "V.90 offered, no extension", 0x08, 0x40, 0x00, 0, 0, 0, 0, 0, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x149, 0x011, 0x011, 0x1c5, 0x0a9,
	    0x163, 0 } },
	{ "V.90 offered, extension matches", 0x08, 0x40, 0x08, 0, '*', 0, 0, 0,
	  0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x149, 0x011, 0x011, 0x1c5, 0x0a9,
	    0x163, 0 } },
	{ "V.90 offered, extension fails", 0x08, 0x40, 0x08, 0, 'J', 0, 0, 0x2a,
	  0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x149, 0x011, 0x011, 0x1c5, 0x0a9,
	    0x163, 0 } },
	{ "V.90 offered, list would match", 0x08, 0x40, 0x00, 0, 0, 0, 0, 0x2a,
	  0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x149, 0x011, 0x011, 0x1c5, 0x0a9,
	    0x163, 0 } },
	{ "V.90 offered, second already settled", 0x08, 0x40, 0x08, 0, '*', 0, 0,
	  0, 0, 1,
	  { W_PRE0, W_PRE1, 0x107, 0x149, 0x011, 0x011, 0x1c5, 0x0a9,
	    0x163, 0 } },
	/*
	 * The first extension stands in for the call function, so its
	 * characters have to land in the call-function range: 0x41 is sent as
	 * 0x105 and 0xc1 as 0x107.  The encoder-built block above gave ext1
	 * 'G', which is sent as 0x1c5 -- a capability word, never a function
	 * one -- so this matcher had never run either.
	 */
	{ "extension is the call function", 0x00, 0x00, 0x04, 0x41, 0, 0, 0, 0,
	  0, 0,
	  { W_PRE0, W_PRE1, 0x105, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	{ "extension wrong, list catches", 0x00, 0x00, 0x04, 0x41, 0, 0, 0xc1,
	  0, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	{ "extension wrong, nothing catches", 0x00, 0x00, 0x04, 0x41, 0, 0, 0,
	  0, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x141, 0x011, 0x011, 0x0a9, 0x161, 0 } },
	/*
	 * The three V.90 conditions have to take different values somewhere,
	 * or the line that reports all three reads the same whichever order
	 * they are printed in.  0x141 withholds the modulation, 0x161 the
	 * digital connection, and 0x1c9 gives a pcmIndication of 2 rather
	 * than the 1 the offer requires -- so each of these also leaves V.90
	 * refused while the local bit is still set.
	 */
	{ "V.90 asked for, modulation withheld", 0x08, 0x40, 0x00, 0, 0, 0, 0,
	  0, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x141, 0x011, 0x011, 0x1c5, 0x0a9,
	    0x163, 0 } },
	{ "V.90 asked for, connection withheld", 0x08, 0x40, 0x00, 0, 0, 0, 0,
	  0, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x149, 0x011, 0x011, 0x1c5, 0x0a9,
	    0x161, 0 } },
	{ "V.90 asked for, wrong pcm indication", 0x08, 0x40, 0x00, 0, 0, 0, 0,
	  0, 0, 0,
	  { W_PRE0, W_PRE1, 0x107, 0x149, 0x011, 0x011, 0x1c9, 0x0a9,
	    0x163, 0 } }
};

/* Lay one case into a pair of objects, one per side. */
static void
rebuild_setup(const struct rebuild_case *c)
{
	int k;

	memset(&obj_a, 0, sizeof(obj_a));
	memset(&obj_b, 0, sizeof(obj_b));
	memset(&cm_a, 0, sizeof(cm_a));

	cm_a.b0 = c->b0;
	cm_a.b1 = c->b1;
	cm_a.b2 = c->b2;
	cm_a.ext1[0] = c->ext1_0;
	cm_a.ext2[0] = c->ext2_0;
	cm_a.ext2[1] = c->ext2_1;
	cm_a.fn_list[0] = c->fn_list_0;
	cm_a.ext_list[0] = c->ext_list_0;
	memcpy(&cm_b, &cm_a, sizeof(cm_a));

	obj_a.cm = &cm_a;
	obj_b.cm = &cm_b;
	obj_a.tx_seq = &obj_a.seq[2];
	obj_b.tx_seq = &obj_b.seq[2];
	obj_a.febc = obj_b.febc = c->febc;
	obj_a.febe = obj_b.febe = c->febe;

	for (k = 0; k < 12 && c->rx[k] != 0; k++) {
		obj_a.seq[0].word[k] = c->rx[k];
		obj_b.seq[0].word[k] = c->rx[k];
	}
	obj_a.seq[0].wordidx = obj_b.seq[0].wordidx = (short)k;
}

static int
t_rebuild_cases(void)
{
	unsigned n = sizeof(rebuild_cases) / sizeof(rebuild_cases[0]);
	unsigned i, lvl;
	long lines = 0;

	diff_begin("rebuildJMSequence: hand-built sequences");

	for (i = 0; i < n; i++) {
		const struct rebuild_case *c = &rebuild_cases[i];

		printf("  case %u: %s\n", i, c->name);

		rebuild_setup(c);
		ref_rebuildJMSequence(&obj_a);
		rebuildJMSequence(&obj_b);

		diff_eq_int("JM words (case %ld)",
			    memcmp(obj_a.seq[2].word, obj_b.seq[2].word,
				   sizeof(obj_a.seq[2].word)) == 0, 1, (long)i);
		diff_eq_int("nbits (case %ld)", obj_b.seq[2].nbits,
			    obj_a.seq[2].nbits, (long)i);
		diff_eq_int("febc (case %ld)", obj_b.febc, obj_a.febc, (long)i);
		diff_eq_int("febe (case %ld)", obj_b.febe, obj_a.febe, (long)i);
		diff_eq_int("fec0 (case %ld)", obj_b.fec0, obj_a.fec0, (long)i);
		diff_eq_int("fec2 (case %ld)", obj_b.fec2, obj_a.fec2, (long)i);
		diff_eq_int("menu (case %ld)",
			    memcmp(&cm_a, &cm_b, sizeof(cm_a)) == 0, 1, (long)i);
	}

	/*
	 * The same cases with both sides' diagnostics raised.  Levels 1 to 3:
	 * every gate here is `> 1`, so level 1 must be silent and 2 and 3 must
	 * be identical -- which is what would catch a site placed behind the
	 * wrong threshold (finding 150).
	 */
	for (lvl = 1; lvl <= 3; lvl++) {
		dsplib_debug_capture_reset();
		dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
		dsplib_debug_capture_on = 1;

		for (i = 0; i < n; i++) {
			rebuild_setup(&rebuild_cases[i]);
			ref_rebuildJMSequence(&obj_a);
			rebuildJMSequence(&obj_b);
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		diff_eq_int("transcript matches (level %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)lvl);
		if (getenv("DBGDIFF")
		    && strcmp(dsplib_debug_capture_text(0),
			      dsplib_debug_capture_text(1)) != 0) {
			const char *o = dsplib_debug_capture_text(0);
			const char *r = dsplib_debug_capture_text(1);
			int j = 0;

			while (o[j] && o[j] == r[j])
				j++;
			while (j > 0 && o[j - 1] != '\n')
				j--;
			printf("=== level %u: divergence at %d\n", lvl, j);
			printf("--- ours: %.400s\n", o + j);
			printf("--- ref : %.400s\n", r + j);
		}
		diff_eq_int("line counts match (level %ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), (long)lvl);
		if (lvl == 1)
			diff_eq_int("silent below the threshold",
				    (int)dsplib_debug_capture_lines(1), 0, 0);
		else
			lines += dsplib_debug_capture_lines(1);
	}

	diff_eq_int("diagnostics were captured (%ld lines)", lines > 40, 1,
		    lines);

	return diff_end();
}

/*
 * The same sequences read as a received JM.  evaluateRxJMSequence looks in
 * seq[2] rather than seq[0] and only decides -- it builds nothing -- so the
 * cases carry over unchanged, and between them they reach all four call
 * functions, both extension fields and both verdicts.
 */
static void
evaluate_setup(const struct rebuild_case *c)
{
	int k;

	rebuild_setup(c);
	for (k = 0; k < V8_TX_SEQ_WORDS; k++) {
		obj_a.seq[2].word[k] = obj_a.seq[0].word[k];
		obj_b.seq[2].word[k] = obj_b.seq[0].word[k];
	}
	obj_a.seq[2].wordidx = obj_a.seq[0].wordidx;
	obj_b.seq[2].wordidx = obj_b.seq[0].wordidx;
}

static int
t_evaluate_cases(void)
{
	unsigned n = sizeof(rebuild_cases) / sizeof(rebuild_cases[0]);
	unsigned i, lvl;
	long lines = 0, got = 0, missed = 0;

	diff_begin("evaluateRxJMSequence: hand-built sequences");

	for (i = 0; i < n; i++) {
		evaluate_setup(&rebuild_cases[i]);
		ref_evaluateRxJMSequence(&obj_a);
		evaluateRxJMSequence(&obj_b);

		diff_eq_int("febc (case %ld)", obj_b.febc, obj_a.febc, (long)i);
		diff_eq_int("febe (case %ld)", obj_b.febe, obj_a.febe, (long)i);
		diff_eq_int("fec0 (case %ld)", obj_b.fec0, obj_a.fec0, (long)i);
		diff_eq_int("fec2 (case %ld)", obj_b.fec2, obj_a.fec2, (long)i);
		diff_eq_int("words untouched (case %ld)",
			    memcmp(obj_a.seq[2].word, obj_b.seq[2].word,
				   sizeof(obj_a.seq[2].word)) == 0, 1, (long)i);
		if (obj_a.febc)
			got++;
		else
			missed++;
	}

	/* Both verdicts have to occur or the closing line is only half seen. */
	diff_eq_int("some matched (%ld)", got > 0, 1, got);
	diff_eq_int("some did not (%ld)", missed > 0, 1, missed);

	for (lvl = 1; lvl <= 3; lvl++) {
		dsplib_debug_capture_reset();
		dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
		dsplib_debug_capture_on = 1;

		for (i = 0; i < n; i++) {
			evaluate_setup(&rebuild_cases[i]);
			ref_evaluateRxJMSequence(&obj_a);
			evaluateRxJMSequence(&obj_b);
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		diff_eq_int("transcript matches (level %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)lvl);
		if (getenv("DBGDIFF")
		    && strcmp(dsplib_debug_capture_text(0),
			      dsplib_debug_capture_text(1)) != 0) {
			const char *o = dsplib_debug_capture_text(0);
			const char *r = dsplib_debug_capture_text(1);
			int j = 0;

			while (o[j] && o[j] == r[j])
				j++;
			while (j > 0 && o[j - 1] != '\n')
				j--;
			printf("=== level %u: divergence at %d\n", lvl, j);
			printf("--- ours: %.400s\n", o + j);
			printf("--- ref : %.400s\n", r + j);
		}
		diff_eq_int("line counts match (level %ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), (long)lvl);
		if (lvl == 1)
			diff_eq_int("silent below the threshold",
				    (int)dsplib_debug_capture_lines(1), 0, 0);
		else
			lines += dsplib_debug_capture_lines(1);
	}

	diff_eq_int("diagnostics were captured (%ld lines)", lines > 40, 1,
		    lines);

	return diff_end();
}

/*
 * V8UpdateModemParameters' five messages.  Four of them are one per exit --
 * quick connect, no call function match, a match with nothing remembered, and
 * the closing menu -- so the variants are chosen by which exit they take
 * rather than by any property of the sequence.
 */
static int
t_update_trace(void)
{
	static const struct {
		short	fdc4, febc, fec0, fec2;
		short	words[8];
	} vars[] = {
		{ 1, 0, 0, 0, { W_PRE0, W_PRE1, 0x107, 0x149, 0x011, 0 } },
		{ 0, 0, 0, 0, { W_PRE0, W_PRE1, 0x107, 0x149, 0x011, 0 } },
		{ 0, 1, 0x107, 0, { W_PRE0, W_PRE1, 0x107, 0x149, 0x011, 0 } },
		/* The three conditions, each taking a different value. */
		{ 0, 1, 0x109, 0xa9, { W_PRE0, W_PRE1, 0x141, 0x011, 0x163,
				       0x1c5, 0 } },
		{ 0, 1, 0x10b, 0x155, { W_PRE0, W_PRE1, 0x149, 0x011, 0x161,
					0x1c9, 0 } },
		{ 0, 1, 0x103, 0, { W_PRE0, W_PRE1, 0x149, 0x011, 0 } },
		{ 0, 1, 0x141, 0, { W_PRE0, W_PRE1, 0x149, 0x011, 0 } },
		/* Matched, but nothing was remembered -- the complaint. */
		{ 0, 1, 0, 0, { W_PRE0, W_PRE1, 0x149, 0x011, 0x163, 0x1c5,
				0 } }
	};
	static struct v8_cm out_a, out_b;
	unsigned n = sizeof(vars) / sizeof(vars[0]);
	unsigned i, lvl;
	long lines = 0;
	int k;

	diff_begin("V8UpdateModemParameters: the report");

	for (lvl = 1; lvl <= 3; lvl++) {
		dsplib_debug_capture_reset();
		dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
		dsplib_debug_capture_on = 1;

		for (i = 0; i < n; i++) {
			memset(&obj_a, 0, sizeof(obj_a));
			memset(&obj_b, 0, sizeof(obj_b));
			memset(&cm_a, 0, sizeof(cm_a));
			cm_a.b0 = 0xaa;
			cm_a.b1 = 0x55;
			memcpy(&cm_b, &cm_a, sizeof(cm_a));
			obj_a.cm = &cm_a;
			obj_b.cm = &cm_b;

			for (k = 0; k < 8 && vars[i].words[k] != 0; k++) {
				obj_a.seq[2].word[k] = vars[i].words[k];
				obj_b.seq[2].word[k] = vars[i].words[k];
			}
			obj_a.seq[2].wordidx = obj_b.seq[2].wordidx = (short)k;

			obj_a.fdc4 = obj_b.fdc4 = vars[i].fdc4;
			obj_a.febc = obj_b.febc = vars[i].febc;
			obj_a.fec0 = obj_b.fec0 = vars[i].fec0;
			obj_a.fec2 = obj_b.fec2 = vars[i].fec2;
			obj_a.fdcc = obj_b.fdcc = 7;

			memset(&out_a, 0x11, sizeof(out_a));
			memcpy(&out_b, &out_a, sizeof(out_a));

			diff_eq_int("returns (case %ld)",
				    V8UpdateModemParameters(&obj_b, &out_b),
				    ref_V8UpdateModemParameters(&obj_a, &out_a),
				    (long)(lvl * 16 + i));
			diff_eq_int("menu out (case %ld)",
				    memcmp(&out_a, &out_b, sizeof(out_a)) == 0,
				    1, (long)(lvl * 16 + i));
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		diff_eq_int("transcript matches (level %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)lvl);
		if (getenv("DBGDIFF")
		    && strcmp(dsplib_debug_capture_text(0),
			      dsplib_debug_capture_text(1)) != 0) {
			const char *o = dsplib_debug_capture_text(0);
			const char *r = dsplib_debug_capture_text(1);
			int j = 0;

			while (o[j] && o[j] == r[j])
				j++;
			while (j > 0 && o[j - 1] != '\n')
				j--;
			printf("=== level %u: divergence at %d\n", lvl, j);
			printf("--- ours: %.400s\n", o + j);
			printf("--- ref : %.400s\n", r + j);
		}
		diff_eq_int("line counts match (level %ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), (long)lvl);
		if (lvl == 1)
			diff_eq_int("silent below the threshold",
				    (int)dsplib_debug_capture_lines(1), 0, 0);
		else
			lines += dsplib_debug_capture_lines(1);
	}

	diff_eq_int("diagnostics were captured (%ld lines)", lines > 20, 1,
		    lines);

	return diff_end();
}

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

	diff_begin("V8UpdateModemParameters");
	{
		static struct v8_cm out_a, out_b;
		long filled = 0, empty = 0;

		for (b1 = 0; b1 < 256; b1 += 5) {
			for (b2 = 0; b2 < 32; b2++) {
				for (ext = 0; ext < 4; ext++) {
					memset(&obj_a, 0, sizeof(obj_a));
					memset(&obj_b, 0, sizeof(obj_b));
					memset(&cm_a, 0, sizeof(cm_a));
					cm_a.b0 = (unsigned char)(b1 ^ b2);
					cm_a.b1 = (unsigned char)b1;
					cm_a.b2 = (unsigned char)(b2 | 0x04);
					cm_a.menu = (int)(b1 * 65537u);
					cm_a.ext1[0] = 'G';
					cm_a.ext2[0] = 'B';
					memcpy(&cm_b, &cm_a, sizeof(cm_a));

					obj_a.cm = &cm_a;
					obj_b.cm = &cm_b;
					obj_a.tx_seq = &obj_a.seq[2];
					obj_b.tx_seq = &obj_b.seq[2];
					ref_initTxSequence(&obj_a);
					initTxSequence(&obj_b);
					obj_a.seq[2].wordidx = (short)
						(ext == 0 ? 0
						 : obj_a.seq[2].nbits / 10);
					obj_b.seq[2].wordidx =
						obj_a.seq[2].wordidx;
					memcpy(obj_b.seq[2].word,
					       obj_a.seq[2].word,
					       sizeof(obj_a.seq[2].word));

					obj_a.mode = obj_b.mode = ext & 1;
					obj_a.fdc4 = obj_b.fdc4 =
						(ext == 3 ? 1 : 0);
					obj_a.fdc8 = obj_b.fdc8 = b2 & 1;
					obj_a.fdcc = obj_b.fdcc = (int)b1;
					obj_a.febc = obj_b.febc =
						(short)(b2 & 2 ? 1 : 0);
					obj_a.fec0 = obj_b.fec0 =
						(short)(b1 & 1 ? 0x107 : 0x103);
					obj_a.fec2 = obj_b.fec2 =
						(short)(b2 & 4 ? 0xa9 : 0x155);

					memset(&out_a, 0x11, sizeof(out_a));
					memcpy(&out_b, &out_a, sizeof(out_a));

					k = V8UpdateModemParameters(&obj_b,
								    &out_b);
					diff_eq_int("returns (%ld)", k,
						    ref_V8UpdateModemParameters(
							    &obj_a, &out_a),
						    (long)b1);
					diff_eq_int("menu out (%ld)",
						    memcmp(&out_a, &out_b,
							   sizeof(out_a)) == 0,
						    1, (long)b1);
					if (k == 0)
						filled++;
					else
						empty++;
				}
			}
		}
		diff_eq_int("menus were filled (%ld)", filled > 0, 1, filled);
		diff_eq_int("and refused (%ld)", empty > 0, 1, empty);
	}
	rc |= diff_end();

	diff_begin("rebuildJMSequence");
	{
		long built = 0;

		for (b1 = 0; b1 < 256; b1 += 7) {
			for (b2 = 0; b2 < 32; b2++) {
				for (ext = 0; ext < 4; ext++) {
					memset(&obj_a, 0, sizeof(obj_a));
					memset(&obj_b, 0, sizeof(obj_b));
					memset(&cm_a, 0, sizeof(cm_a));
					cm_a.b0 = (unsigned char)(b1 ^ 0x5a);
					cm_a.b1 = (unsigned char)b1;
					cm_a.b2 = (unsigned char)b2;
					if (ext & 1) {
						cm_a.ext1[0] = 'G';
						cm_a.ext1[1] = 'B';
					}
					if (ext & 2) {
						cm_a.ext2[0] = 'Z';
						cm_a.ext2[1] = '9';
					}
					cm_a.fn_list[0] = 0x83;
					cm_a.ext_list[0] = 0x54;
					memcpy(&cm_b, &cm_a, sizeof(cm_a));

					/* A received CM to answer. */
					obj_a.cm = &cm_a;
					obj_b.cm = &cm_b;
					obj_a.tx_seq = &obj_a.seq[0];
					obj_b.tx_seq = &obj_b.seq[0];
					ref_initTxSequence(&obj_a);
					initTxSequence(&obj_b);
					obj_a.seq[0].wordidx = (short)
						(obj_a.seq[0].nbits / 10);
					obj_b.seq[0].wordidx =
						obj_a.seq[0].wordidx;

					/* Build the JM into another buffer. */
					obj_a.tx_seq = &obj_a.seq[2];
					obj_b.tx_seq = &obj_b.seq[2];
					obj_a.febc = obj_b.febc =
						(short)(b2 & 1);
					obj_a.febe = obj_b.febe =
						(short)((b2 >> 1) & 1);

					ref_rebuildJMSequence(&obj_a);
					rebuildJMSequence(&obj_b);

					diff_eq_int("JM (%ld)",
						    memcmp(&obj_a.seq[2],
							   &obj_b.seq[2],
							   sizeof(obj_a.seq[2]))
						    == 0, 1, (long)b1);
					diff_eq_int("menu after (%ld)",
						    memcmp(&cm_a, &cm_b,
							   sizeof(cm_a)) == 0,
						    1, (long)b1);
					diff_eq_int("fec0 (%ld)", obj_b.fec0,
						    obj_a.fec0, (long)b1);
					diff_eq_int("fec2 (%ld)", obj_b.fec2,
						    obj_a.fec2, (long)b1);
					diff_eq_int("febc (%ld)", obj_b.febc,
						    obj_a.febc, (long)b1);
					if (obj_a.seq[2].nbits != 0)
						built++;
				}
			}
		}
		diff_eq_int("JMs were built (%ld)", built > 0, 1, built);
	}
	rc |= diff_end();

	rc |= t_rebuild_cases();
	rc |= t_evaluate_cases();
	rc |= t_update_trace();
	return rc;
}
