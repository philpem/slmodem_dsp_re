/*
 * t_encode.c -- differential test of the obfuscated diagnostic channel.
 *
 * `edprintf` has no return value and writes nothing a caller can see.  Its
 * two observable effects are the string it hands to `dsplibs_debug_printf`
 * and the shared key counter it leaves behind, and the second is visible at
 * ANY debug level because only the final print is gated.  So this test
 * checks both, and the counter check is the one that does not depend on the
 * diagnostic level being raised at all.
 *
 * READING THE COUNTER.  Neither side exports it -- `iEncodeOffset` is
 * file-local in the object and static here -- so it is read the way the
 * object allows: `cEncodeChar` returns `offsetarr[k] + '0' + c`, and TWO
 * consecutive probes identify `k` uniquely.  One does not: `offsetarr` holds
 * 7 at both index 3 and index 9.  All ten consecutive PAIRS are distinct,
 * which is what makes the two-probe read work, and `probe_key` asserts that
 * property rather than assuming it.
 *
 * THE STATE IS SHARED AND PERSISTENT, so the two sides are driven through
 * one scripted sequence of interleaved calls rather than reset between
 * cases.  Anything that reset it would hide exactly the coupling this file
 * is about.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/encode.h"

extern unsigned int ref_dsplibs_debug_level;

extern char ref_cEncodeChar(unsigned char c);
extern void ref_edprintf(const char *fmt, ...);

/*
 * The key, as the object holds it.  Duplicated here on purpose: the test
 * asserts the reconstruction's table against the blob's behaviour, so it
 * must not read the reconstruction's own copy.
 */
static const int key[ENCODE_KEY_LEN] = { 4, 6, 2, 7, 1, 9, 3, 5, 8, 7 };

/*
 * Two probes of `cEncodeChar(0)` name the counter position.  Costs two
 * steps of the key, which the caller has to account for.
 */
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

int
main(void)
{
	int rc = 0;
	int i, j;

	diff_begin("encode: the key's consecutive pairs are distinct");
	{
		/*
		 * The two-probe read below is only a read if no two positions
		 * share a pair.  Checked, not assumed -- single values do
		 * collide, at indices 3 and 9.
		 */
		int collisions = 0;

		for (i = 0; i < ENCODE_KEY_LEN; i++)
		for (j = 0; j < ENCODE_KEY_LEN; j++)
			if (i != j
			    && key[i] == key[j]
			    && key[(i + 1) % ENCODE_KEY_LEN]
			       == key[(j + 1) % ENCODE_KEY_LEN])
				collisions++;
		diff_eq_int("no two positions share a pair", collisions, 0, 0);

		/* And single values DO collide, so one probe is not enough. */
		{
			int singles = 0;

			for (i = 0; i < ENCODE_KEY_LEN; i++)
			for (j = i + 1; j < ENCODE_KEY_LEN; j++)
				if (key[i] == key[j])
					singles++;
			diff_eq_int("single values collide", singles, 1, 0);
		}
	}
	rc |= diff_end();

	diff_begin("encode: cEncodeChar over every byte, and the key wraps");
	{
		/*
		 * 256 bytes against a key of 10 is not a multiple, so the
		 * sweep walks every (byte, position) pair it can and crosses
		 * the 9 -> 0 wrap 25 times.
		 */
		int wraps = 0;
		int prev = -1;

		for (i = 0; i < 256; i++) {
			int a = (unsigned char)cEncodeChar((unsigned char)i);
			int b = (unsigned char)ref_cEncodeChar(
				(unsigned char)i);

			diff_eq_int("cEncodeChar(0x%02lx)", a, b, i);
			if (prev >= 0 && a < prev)
				wraps++;
			prev = a;
		}
		diff_eq_int("the sweep crossed the wrap", wraps > 0, 1, 0);
	}
	rc |= diff_end();

	diff_begin("encode: edprintf's transcript");
	{
		/*
		 * Every case takes three ints, because that is what the loop
		 * passes.  No `%s` here: it would make vsnprintf dereference
		 * one of them, which is a fault in the fixture and not a
		 * property of anything being tested.  `%s` is driven with a
		 * real string in the guard block below.
		 */
		static const char *const cases[] = {
			"", "H", "Hi", "hello world\n",
			"%d %d %d", "%u|%u", "0x%08x", "%c%c%c",
			"tab\there", "high\x80\xff byte"
		};
		unsigned c;

		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		for (c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
			dsplib_debug_capture_reset();
			edprintf(cases[c], 1, -2, 305419896);
			ref_edprintf(cases[c], 1, -2, 305419896);
			diff_eq_int("edprintf transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, c);
		}

		/*
		 * The worked example in encode.c's header, checked rather
		 * than asserted.  A fresh key is guaranteed: edprintf zeroes
		 * it before encoding.
		 */
		dsplib_debug_capture_reset();
		edprintf("Hi");
		ref_edprintf("Hi");
		diff_eq_int("the header's worked example",
			    strcmp(dsplib_debug_capture_text(0),
				   "$!$ 8>8@????\n") == 0, 1, 0);
		diff_eq_int("and the blob agrees",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, 0);

		diff_eq_int("transcript non-empty",
			    dsplib_debug_capture_text(1)[0] != 0, 1, 0);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	diff_begin("encode: the length guard, either side of it");
	{
		static char big[400];
		int n;

		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		/*
		 * 2*len + 8 <= 270 admits len <= 131.  Both sides of that
		 * boundary, plus the 0x100 cap vsnprintf imposes on `temp`
		 * -- which is what makes anything past 255 unreachable.
		 */
		for (n = 128; n <= 135; n++) {
			memset(big, 'A', sizeof(big));
			big[n] = '\0';

			dsplib_debug_capture_reset();
			edprintf("%s", big);
			ref_edprintf("%s", big);
			diff_eq_int("guard boundary",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, n);
		}

		/* 131 is the last accepted length; 132 is the first refused. */
		memset(big, 'A', sizeof(big));
		big[131] = '\0';
		dsplib_debug_capture_reset();
		edprintf("%s", big);
		diff_eq_int("131 encodes",
			    strncmp(dsplib_debug_capture_text(0), "$!$ ", 4)
			    == 0, 1, 131);
		memset(big, 'A', sizeof(big));
		big[132] = '\0';
		dsplib_debug_capture_reset();
		edprintf("%s", big);
		diff_eq_int("132 is refused",
			    strcmp(dsplib_debug_capture_text(0),
				   "too long print string\n") == 0, 1, 132);

		/* And well past the vsnprintf cap. */
		memset(big, 'B', sizeof(big) - 1);
		big[sizeof(big) - 1] = '\0';
		dsplib_debug_capture_reset();
		edprintf("%s", big);
		ref_edprintf("%s", big);
		diff_eq_int("far too long",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, 0);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	/*
	 * The half the two blocks above cannot check.  Both raise the level to
	 * 2 before capturing, so a print that lost its `if (DSPLIB_DEBUG_ON())`
	 * emits the same text on both sides and passes: with only those blocks,
	 * ungating either of encode.c's two sites SURVIVES.
	 *
	 * Level 1 rather than 0 is the point.  The gate is `> 1`, so 1 is the
	 * one value at which `> 1` and the `>= 1` a reader would write disagree;
	 * at 0 both are silent and both mutants live.  Both levels are swept
	 * anyway, since the pass costs two calls.
	 *
	 * BOTH SITES, which needs both lengths: the short string takes the
	 * normal path and the 300-character one takes the "too long" return,
	 * and each has a gate of its own.
	 */
	diff_begin("encode: below the threshold, nothing is said");
	{
		static char big[400];
		unsigned lvl;

		dsplib_debug_capture_on = 1;

		for (lvl = 0; lvl <= 1; lvl++) {
			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;
			dsplib_debug_capture_reset();

			edprintf("Hi");
			ref_edprintf("Hi");

			memset(big, 'A', sizeof(big));
			big[300] = '\0';
			edprintf("%s", big);
			ref_edprintf("%s", big);

			diff_eq_int("ours printed nothing",
				    dsplib_debug_capture_text(0)[0], 0,
				    (long)lvl);
			diff_eq_int("and neither did the reference",
				    dsplib_debug_capture_text(1)[0], 0,
				    (long)lvl);
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	diff_begin("encode: the key is shared, and moves with the level off");
	{
		/*
		 * THE POINT OF THE FILE.  With diagnostics off nothing is
		 * printed, but the key still moves: a successful edprintf
		 * resets it to zero and leaves it at (2 * len) mod 10, and
		 * the refused path leaves it exactly where it was.  Both are
		 * read back through cEncodeChar on each side and compared.
		 */
		static const char *const msgs[] = {
			"a", "ab", "abc", "abcde", "0123456789", ""
		};
		unsigned m;

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;

		for (m = 0; m < sizeof(msgs) / sizeof(msgs[0]); m++) {
			int a1, a2, b1, b2, ka, kb;

			edprintf("%s", msgs[m]);
			ref_edprintf("%s", msgs[m]);

			a1 = (unsigned char)cEncodeChar(0);
			a2 = (unsigned char)cEncodeChar(0);
			b1 = (unsigned char)ref_cEncodeChar(0);
			b2 = (unsigned char)ref_cEncodeChar(0);

			ka = key_pair_index(a1, a2);
			kb = key_pair_index(b1, b2);
			diff_eq_int("key position after edprintf", ka, kb,
				    (long)m);
			/*
			 * And it is where the encoding says it should be:
			 * the key is zeroed and then stepped twice per source
			 * byte.  `key_pair_index` reports the position the
			 * FIRST probe read, which is before either probe has
			 * moved it -- so the probes do not enter the sum.
			 */
			diff_eq_int("key position is 2*len mod 10", ka,
				    (int)((2 * strlen(msgs[m]))
					  % ENCODE_KEY_LEN), (long)m);
		}

		/* The refused path must NOT reset it. */
		{
			static char big[400];
			int a1, a2, b1, b2, before, after;

			memset(big, 'C', sizeof(big));
			big[200] = '\0';

			a1 = (unsigned char)cEncodeChar(0);
			a2 = (unsigned char)cEncodeChar(0);
			b1 = (unsigned char)ref_cEncodeChar(0);
			b2 = (unsigned char)ref_cEncodeChar(0);
			before = key_pair_index(a1, a2);
			diff_eq_int("both sides agree before", before,
				    key_pair_index(b1, b2), 0);

			edprintf("%s", big);
			ref_edprintf("%s", big);

			a1 = (unsigned char)cEncodeChar(0);
			a2 = (unsigned char)cEncodeChar(0);
			b1 = (unsigned char)ref_cEncodeChar(0);
			b2 = (unsigned char)ref_cEncodeChar(0);
			after = key_pair_index(a1, a2);
			diff_eq_int("both sides agree after", after,
				    key_pair_index(b1, b2), 0);
			diff_eq_int("the refused path did not reset the key",
				    after,
				    (before + 2) % ENCODE_KEY_LEN, 0);
		}

		/*
		 * Interleaved, at length: cEncodeChar and edprintf moving one
		 * counter between them, over enough calls that a reconstruction
		 * which reset in the wrong place would drift.
		 */
		for (i = 0; i < 200; i++) {
			int a, b;

			if (i % 7 == 0) {
				edprintf("i=%d", i);
				ref_edprintf("i=%d", i);
			}
			a = (unsigned char)cEncodeChar((unsigned char)(i * 3));
			b = (unsigned char)ref_cEncodeChar(
				(unsigned char)(i * 3));
			diff_eq_int("interleaved key", a, b, i);
		}
	}
	rc |= diff_end();

	diff_begin("encode: plain mode changes the text and nothing else");
	{
		/*
		 * `dsplib_encode_plain` is ours, not the object's (D40), so
		 * there is nothing to compare it against.  What CAN be
		 * compared is the claim it rests on: that turning it on
		 * changes only the string printed, and leaves the shared key
		 * exactly where the encoded path would have left it.
		 *
		 * So each message is run twice on our side -- once with the
		 * switch off, once on -- and the blob is run alongside the
		 * OFF pass.  The key is read after each, and all three must
		 * agree.
		 */
		static const char *const msgs[] = {
			"Hi", "V34: rate 33600", "", "%d", "abcdefghij"
		};
		unsigned m;

		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		for (m = 0; m < sizeof(msgs) / sizeof(msgs[0]); m++) {
			char encoded[512];
			char plain[512];
			int koff, kon, kref;
			int a1, a2;

			/* --- off: must still match the blob --- */
			dsplib_encode_plain = 0;
			dsplib_debug_capture_reset();
			edprintf(msgs[m], 42);
			ref_edprintf(msgs[m], 42);
			diff_eq_int("plain off still matches the blob",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, (long)m);
			snprintf(encoded, sizeof(encoded), "%s",
				 dsplib_debug_capture_text(0));

			a1 = (unsigned char)cEncodeChar(0);
			a2 = (unsigned char)cEncodeChar(0);
			koff = key_pair_index(a1, a2);
			a1 = (unsigned char)ref_cEncodeChar(0);
			a2 = (unsigned char)ref_cEncodeChar(0);
			kref = key_pair_index(a1, a2);

			/* --- on --- */
			dsplib_encode_plain = 1;
			dsplib_debug_capture_reset();
			edprintf(msgs[m], 42);
			snprintf(plain, sizeof(plain), "%s",
				 dsplib_debug_capture_text(0));
			a1 = (unsigned char)cEncodeChar(0);
			a2 = (unsigned char)cEncodeChar(0);
			kon = key_pair_index(a1, a2);
			dsplib_encode_plain = 0;

			/*
			 * The key must have moved identically -- that is what
			 * "changes only the text" means, and it is the part a
			 * caller could otherwise be perturbed by.
			 */
			diff_eq_int("the key moves the same either way",
				    kon, koff, (long)m);
			diff_eq_int("and the same as the blob's",
				    koff, kref, (long)m);

			/* The text is the message, framed by nothing. */
			{
				char want[512];

				snprintf(want, sizeof(want), "%s\n", msgs[m]);
				/* `%d` becomes "42" once formatted. */
				if (strcmp(msgs[m], "%d") == 0)
					snprintf(want, sizeof(want), "42\n");
				diff_eq_int("plain mode prints the message",
					    strcmp(plain, want) == 0, 1,
					    (long)m);
			}

			/* And the encoded form was a frame, so the two differ. */
			diff_eq_int("the two forms differ",
				    strcmp(plain, encoded) != 0, 1, (long)m);
			diff_eq_int("the encoded form is a frame",
				    strncmp(encoded, "$!$ ", 4) == 0, 1,
				    (long)m);
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_encode_plain = 0;
	}
	rc |= diff_end();

	return rc;
}
