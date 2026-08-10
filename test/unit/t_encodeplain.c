/*
 * t_encodeplain.c -- the refused path of `edprintf`, in both print modes.
 *
 * WHAT THIS COVERS THAT `t_encode.c` DOES NOT.  `t_encode` drives plain mode
 * (D40's added switch) only over messages the encoder ACCEPTS, and drives the
 * refused path -- `2 * strlen(temp) + 8 > 270` -- only with plain mode off.
 * The two never meet, and gcov says so: in the instrumented tree the
 * conditional at encode.c's too-long print site
 *
 *     dsplibs_debug_printf("%s\n", dsplib_encode_plain ? temp : cEncodedTemp);
 *
 * runs nine times and takes its `temp` arm zero times.  D40's claim that
 * plain mode "shows messages the encoded channel drops" therefore rested on
 * reading the source.  This file drives it.
 *
 * WHAT IS COMPARABLE HERE, AND WHAT IS NOT.  `dsplib_encode_plain` is ours;
 * the blob has no such switch and always prints "too long print string".  So
 * the TEXT in plain mode is a property of our side alone and is asserted as
 * one.  What is differential -- and is the whole point -- is the shared key
 * counter `iEncodeOffset`: whatever mode we are in, it must end where the
 * blob's ends, because that is the only thing a later `cEncodeChar` caller
 * can see.  A plain mode that short-circuited the function, or that reset the
 * counter on the way out, would be invisible in the transcript and would
 * change what every subsequent caller reads.
 *
 * READING THE COUNTER.  As in `t_encode`: neither side exports it, so it is
 * read with TWO consecutive `cEncodeChar(0)` probes, which name a position
 * uniquely where one probe does not (`offsetarr` holds 7 at both index 3 and
 * index 9).  Each read costs two steps of the key and the arithmetic below
 * accounts for them.
 *
 * WHY EVERY START POSITION.  The refused path must leave the counter exactly
 * where it found it, so after a two-probe read at position `start` the
 * counter must read `(start + 2) % 10`.  At `start == 8` that is 0 -- which
 * is also what a spurious reset would produce.  A test pinned to one start
 * has a one-in-ten chance of being blind to the defect it exists to catch,
 * so all ten are swept and the sweep is asserted to visit ten distinct
 * expected positions.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/encode.h"

extern unsigned int ref_dsplibs_debug_level;

extern char ref_cEncodeChar(unsigned char c);
extern void ref_edprintf(const char *fmt, ...);

/* What the blob prints for anything too long, in either of our modes. */
#define REFUSED "too long print string\n"

/*
 * The key as the object holds it.  Duplicated on purpose, exactly as
 * `t_encode` duplicates it: the probes must be decoded against a table this
 * test owns, not against the reconstruction's own copy.
 */
static const int key[ENCODE_KEY_LEN] = { 4, 6, 2, 7, 1, 9, 3, 5, 8, 7 };

static int
key_pair_index(int a, int b)
{
	int k;

	for (k = 0; k < ENCODE_KEY_LEN; k++)
		if (key[k] + '0' == a
		    && key[(k + 1) % ENCODE_KEY_LEN] + '0' == b)
			return k;
	return -1;
}

/*
 * A read is two probes and costs two steps.  `-1` means the pair matched no
 * position, which every caller checks: two failed reads compare equal to each
 * other and would otherwise pass silently.
 */
static int
probe_ours(void)
{
	int a = (unsigned char)cEncodeChar(0);
	int b = (unsigned char)cEncodeChar(0);

	return key_pair_index(a, b);
}

static int
probe_ref(void)
{
	int a = (unsigned char)ref_cEncodeChar(0);
	int b = (unsigned char)ref_cEncodeChar(0);

	return key_pair_index(a, b);
}

/*
 * Put a side's counter at a chosen position.  An accepted `edprintf("")`
 * zeroes it and steps it zero times, and each `cEncodeChar` steps it once, so
 * `k` calls land on `k`.  Both sides are driven by the same two lines, and
 * the alignment is not assumed: the "before" read in every case asserts it.
 */
static void
align_ours(int k)
{
	int i;

	edprintf("");
	for (i = 0; i < k; i++)
		(void)cEncodeChar(0);
}

static void
align_ref(int k)
{
	int i;

	ref_edprintf("");
	for (i = 0; i < k; i++)
		(void)ref_cEncodeChar(0);
}

/*
 * One message, as `edprintf` will see it.  Every case takes a string then an
 * int, so one call shape drives them all; a format that consumes only the
 * first simply ignores the second.
 */
struct tcase {
	const char *what;
	const char *fmt;
	const char *sarg;
	int iarg;
};

struct meas {
	int before_ours, before_ref;
	int after_ours, after_ref;
};

/* Counts every measured drive, so a loop that ran zero times cannot pass. */
static int drives;

/*
 * Align both sides, read the counter, make the call, read it again.
 *
 * The capture is reset AFTER alignment, so the transcript holds only the call
 * under test; the probes print nothing, so they cannot pollute it either.
 * `plain` is set for the measured call alone -- alignment always runs in the
 * blob's own mode.
 */
static void
drive(const struct tcase *c, int start, int plain, struct meas *m)
{
	dsplib_encode_plain = 0;
	align_ours(start);
	align_ref(start);

	dsplib_debug_capture_reset();

	m->before_ours = probe_ours();
	m->before_ref = probe_ref();

	dsplib_encode_plain = plain;
	edprintf(c->fmt, c->sarg, c->iarg);
	ref_edprintf(c->fmt, c->sarg, c->iarg);
	dsplib_encode_plain = 0;

	m->after_ours = probe_ours();
	m->after_ref = probe_ref();
	drives++;
}

/* What `sysdep_vsnprintf(temp, ENCODE_FMT_MAX, ...)` will have produced. */
static void
expect(char *out, unsigned size, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(out, size, fmt, ap);
	va_end(ap);
}

/* Every read is checked for -1 in one place, with the case in the input. */
static void
check_read(const struct meas *m, long tag)
{
	diff_eq_int("before-read named a position, ours", m->before_ours >= 0,
		    1, tag);
	diff_eq_int("before-read named a position, blob", m->before_ref >= 0,
		    1, tag);
	diff_eq_int("after-read named a position, ours", m->after_ours >= 0,
		    1, tag);
	diff_eq_int("after-read named a position, blob", m->after_ref >= 0,
		    1, tag);
}

static char sA132[133];
static char sC200[201];
static char sD255[256];
static char sB400[401];
static char sE140[141];

static const struct tcase cases[] = {
	/* 131 is the last accepted length, so 132 is the first refusal. */
	{ "132 chars, the first refused length",	"%s",		sA132, 0 },
	{ "200 chars",				"%s",		sC200, 0 },
	{ "255 chars, all `temp` can hold",	"%s",		sD255, 0 },
	{ "400 chars, truncated by vsnprintf",	"%s",		sB400, 0 },
	/* Formatting happens BEFORE the guard, so the number counts. */
	{ "140 chars and a number",		"%s%d",		sE140, 42 },
	{ "200 chars in a frame",		"[%s] %d",	sC200, -7 },
};

#define NCASES ((int)(sizeof(cases) / sizeof(cases[0])))

static void
fillbuf(char *b, int n, char c)
{
	memset(b, c, (size_t)n);
	b[n] = '\0';
}

int
main(void)
{
	int rc = 0;
	int start, ci;
	char exp[ENCODE_FMT_MAX];
	char want[ENCODE_FMT_MAX + 2];
	struct meas m;

	fillbuf(sA132, 132, 'A');
	fillbuf(sC200, 200, 'C');
	fillbuf(sD255, 255, 'D');
	fillbuf(sB400, 400, 'B');
	fillbuf(sE140, 140, 'E');

	/*
	 * The cases have to be refused or nothing below tests anything: an
	 * accepted message takes the other path entirely and every check
	 * against the encoded transcript still passes.  The guard is
	 * recomputed here from the SAME formatted string the assertions
	 * compare against, not from the raw argument.
	 */
	diff_begin("encodeplain: every case is refused, and by how much");
	{
		diff_eq_int("the case list is the expected length", NCASES, 6,
			    0);
		for (ci = 0; ci < NCASES; ci++) {
			unsigned len;

			expect(exp, ENCODE_FMT_MAX, cases[ci].fmt,
			       cases[ci].sarg, cases[ci].iarg);
			len = (unsigned)strlen(exp);
			diff_eq_int("2*len+8 exceeds the encoded buffer",
				    2 * len + 8 > ENCODE_OUT_MAX, 1,
				    (long)ci);
			/*
			 * And within what `temp` can hold, which is what makes
			 * D40's "there is always something to print" true.
			 */
			diff_eq_int("the message fits temp", len
				    < ENCODE_FMT_MAX, 1, (long)ci);
			diff_eq_int("the message is not empty", len > 0, 1,
				    (long)ci);
		}
	}
	rc |= diff_end();

	dsplibs_debug_level = 2;
	ref_dsplibs_debug_level = 2;
	dsplib_debug_capture_on = 1;

	/*
	 * THE CONTROL FOR THE INSTRUMENT.  Everything below asserts that the
	 * counter did NOT move, and an instrument stuck on one answer would
	 * assert that happily.  So first drive an ACCEPTED message from every
	 * start and watch the reading change: a successful `edprintf` zeroes
	 * the counter and steps it twice per character, so "abc" must leave it
	 * at 6 from every start, which differs from `(start + 2) % 10` at nine
	 * starts out of ten.
	 */
	diff_begin("encodeplain: control -- an accepted message resets the key");
	{
		static const struct tcase ok = { "abc", "%s", "abc", 0 };
		int agreed = 0;

		for (start = 0; start < ENCODE_KEY_LEN; start++) {
			drive(&ok, start, 0, &m);
			check_read(&m, (long)start);
			diff_eq_int("aligned, ours", m.before_ours, start,
				    (long)start);
			diff_eq_int("aligned, blob", m.before_ref, start,
				    (long)start);
			diff_eq_int("accepted: ours lands at 2*3",
				    m.after_ours, 6, (long)start);
			diff_eq_int("accepted: blob lands at 2*3",
				    m.after_ref, 6, (long)start);
			if (m.after_ours == (start + 2) % ENCODE_KEY_LEN)
				agreed++;
			diff_eq_int("accepted: the transcript is a frame",
				    strncmp(dsplib_debug_capture_text(0),
					    "$!$ ", 4) == 0, 1, (long)start);
			diff_eq_int("accepted: and the blob's matches",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, (long)start);
		}
		/*
		 * Exactly one start (4) puts an accepted message where a
		 * refused one would land.  If this were 10 the two predictions
		 * would be indistinguishable and the sweep below would prove
		 * nothing.
		 */
		diff_eq_int("the two predictions differ at 9 of 10 starts",
			    agreed, 1, 0);
	}
	rc |= diff_end();

	/*
	 * PLAIN OFF: the refused path, against the blob, from every start.
	 * This is the differential half and it must hold before the plain-mode
	 * half means anything.
	 */
	diff_begin("encodeplain: refused with plain OFF matches the blob");
	{
		int seen[ENCODE_KEY_LEN];
		int distinct = 0;

		memset(seen, 0, sizeof(seen));

		for (start = 0; start < ENCODE_KEY_LEN; start++)
		for (ci = 0; ci < NCASES; ci++) {
			long tag = start * 10L + ci;

			drive(&cases[ci], start, 0, &m);
			check_read(&m, tag);
			diff_eq_int("aligned, ours", m.before_ours, start,
				    tag);
			diff_eq_int("aligned, blob", m.before_ref, start, tag);

			/* The counter ends where the two probes left it. */
			diff_eq_int("ours and the blob end together",
				    m.after_ours, m.after_ref, tag);
			diff_eq_int("the refused path did not move the key",
				    m.after_ours,
				    (start + 2) % ENCODE_KEY_LEN, tag);

			diff_eq_int("ours printed exactly one line",
				    (int)dsplib_debug_capture_lines(0), 1,
				    tag);
			diff_eq_int("the blob printed exactly one line",
				    (int)dsplib_debug_capture_lines(1), 1,
				    tag);
			diff_eq_int("the transcripts agree",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("and both are the refusal",
				    strcmp(dsplib_debug_capture_text(1),
					   REFUSED) == 0, 1, tag);

			if (!seen[m.after_ours]) {
				seen[m.after_ours] = 1;
				distinct++;
			}
		}

		/*
		 * The sweep really did visit ten positions.  Without this the
		 * whole loop could be sitting on one start and reading as a
		 * sweep.
		 */
		diff_eq_int("the sweep visited every key position", distinct,
			    ENCODE_KEY_LEN, 0);
	}
	rc |= diff_end();

	/*
	 * PLAIN ON: the arm gcov reports as never taken.
	 *
	 * The blob is driven alongside, so the counter comparison is made
	 * within one run rather than against a remembered number, and so the
	 * two transcripts can be shown to differ in exactly the documented
	 * way: side 0 is ours and carries the message, side 1 is the blob's
	 * and carries the refusal.
	 */
	diff_begin("encodeplain: refused with plain ON -- text differs, key does not");
	{
		int differed = 0;

		for (start = 0; start < ENCODE_KEY_LEN; start++)
		for (ci = 0; ci < NCASES; ci++) {
			long tag = start * 10L + ci;

			drive(&cases[ci], start, 1, &m);
			check_read(&m, tag);
			diff_eq_int("aligned, ours", m.before_ours, start,
				    tag);
			diff_eq_int("aligned, blob", m.before_ref, start, tag);

			/*
			 * THE CLAIM UNDER TEST.  Plain mode changes the string
			 * and nothing else, so the shared counter must be
			 * exactly where the blob's is -- including on the path
			 * that leaves it alone entirely.
			 */
			diff_eq_int("plain ON still ends where the blob does",
				    m.after_ours, m.after_ref, tag);
			diff_eq_int("plain ON did not move the key either",
				    m.after_ours,
				    (start + 2) % ENCODE_KEY_LEN, tag);

			diff_eq_int("ours printed exactly one line",
				    (int)dsplib_debug_capture_lines(0), 1,
				    tag);
			diff_eq_int("the blob printed exactly one line",
				    (int)dsplib_debug_capture_lines(1), 1,
				    tag);

			/*
			 * Ours is the message the channel dropped.  `temp` is
			 * what vsnprintf produced, so the 400-character case
			 * is expected TRUNCATED to 255 -- plain mode prints
			 * `temp`, not the argument.
			 */
			expect(exp, ENCODE_FMT_MAX, cases[ci].fmt,
			       cases[ci].sarg, cases[ci].iarg);
			snprintf(want, sizeof(want), "%s\n", exp);
			diff_eq_int("plain mode printed the message",
				    strcmp(dsplib_debug_capture_text(0), want)
				    == 0, 1, tag);
			diff_eq_int("the message survived non-empty",
				    dsplib_debug_capture_text(0)[0] != 0, 1,
				    tag);

			/* The blob still drops it, and says so. */
			diff_eq_int("the blob printed the refusal",
				    strcmp(dsplib_debug_capture_text(1),
					   REFUSED) == 0, 1, tag);
			diff_eq_int("so the two transcripts differ",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) != 0,
				    1, tag);
			if (strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) != 0)
				differed++;
		}
		diff_eq_int("every case differed", differed,
			    ENCODE_KEY_LEN * NCASES, 0);
	}
	rc |= diff_end();

	/*
	 * The 255-character case is the one that shows what "prints the
	 * message" means at the edge: `temp` is 256 bytes, so a longer
	 * argument arrives truncated and plain mode prints the truncation.
	 * Asserted separately because it is a refinement of D40's wording, not
	 * a restatement of it.
	 */
	diff_begin("encodeplain: plain mode prints temp, so 400 chars become 255");
	{
		static const struct tcase big = { "400 chars", "%s", sB400, 0 };

		drive(&big, 0, 1, &m);
		check_read(&m, 0);
		diff_eq_int("still did not move the key", m.after_ours,
			    m.after_ref, 0);
		diff_eq_int("the printed message is 255 chars plus a newline",
			    (int)strlen(dsplib_debug_capture_text(0)), 256, 0);
		diff_eq_int("and it is all 'B'",
			    (int)strspn(dsplib_debug_capture_text(0), "B"),
			    255, 0);
	}
	rc |= diff_end();

	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;
	dsplib_encode_plain = 0;

	/*
	 * Nothing above can have passed by not running: 10 control drives,
	 * two sweeps of 10 x 6, and the truncation case.
	 */
	diff_begin("encodeplain: the loops ran");
	{
		diff_eq_int("measured drives", drives,
			    ENCODE_KEY_LEN + 2 * ENCODE_KEY_LEN * NCASES + 1,
			    0);
	}
	rc |= diff_end();

	return rc;
}
