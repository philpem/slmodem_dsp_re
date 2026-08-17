/*
 * t_v90cpb2i.cpp -- differential test of `V90CP::bitsToInfo`, the receive-side
 * driver, and with it of the two function-local statics `alpha` and `beta`.
 *
 * A RANDOM BIT STREAM PROVES NOTHING HERE, and that is the whole design.  The
 * state machine leaves state 0 only after seventeen consecutive ones, and
 * states 5 to 13 are reachable only through a message whose CRC checks, so a
 * sweep of varied bytes sits in states 0 and 1 for ever and every one of the
 * fourteen arms would go untested while the suite went green.  So the stimulus
 * is CLOSED WITH THE TRANSMITTER: a third object is filled in, `infoToBits`
 * lays the message out into its `bits`, and those `word_3bac` bytes are then
 * fed one at a time into a fresh pair.  `infoToBits` is differentially
 * identical to the blob's already (t_v90cpinfo), so using it to make the
 * stimulus asserts nothing about the member under test.
 *
 * WHAT EACH RUN HAS TO SEPARATE, because two objects agreeing proves nothing
 * about a body no input reaches:
 *
 *   message      THE STATES ACTUALLY ENTERED ARE COUNTED.  Every trial
 *                accumulates a mask of the blob's own +0xca4 after every bit
 *                and the run asserts the mask at the end, so "the long form
 *                walks 0,1,2,4,5,6,7,8,10,11,12,13" is measured and not
 *                assumed.  The decoded fields are then read back off the BLOB
 *                and compared against what the transmitter was given, which
 *                is what makes a stalled or short-circuited decode visible.
 *
 *   sixteen ones ARE NOT ENOUGH.  `case 0` leaves on `> 0x10`.  One trial
 *                feeds sixteen ones and a zero before the real preamble and
 *                requires the blob to still be in state 0 at that point;
 *                without it, `>=` and `>` are indistinguishable.
 *
 *   short form   Six states and not twelve, and `word_ca0` and `byte_13`
 *                carried in bits 0x20 and 0x21 -- the two `evaluateInfo`'s
 *                `case 3` reads.
 *
 *   bad CRC      One payload bit is flipped before the feed.  The blob must
 *                reach state 12, fail, reset to state 0 and never report; the
 *                diagnostic is compared as TEXT between the two sides, so the
 *                author's own spelling is what is being checked.
 *
 *   the answer   All four of 1, 2, 3 and 4 are produced, from the two bits
 *                that survive the message (+0x00 and +0x13), and the hold-off
 *                at +0x3bbc is then driven from both of its states: at -1 the
 *                answers all come through, at 0 the 2 and the 4 are suppressed
 *                and the 1 and the 3 are not.  Four counters, one per value.
 *
 *   Ed           A run of 2 * the group size of zeros with the cursor at 18.
 *                AND THE ZERO-GROUP CORNER, which is the trial that separates
 *                "read +0xcaa back out of the object" from "keep it in a
 *                local": with a group size of zero the test is true on a ONE
 *                bit, because the object stores 0 into +0xcaa and reloads it
 *                four instructions later.
 *
 *   the guard    Five of the ten store sites bound the cursor at 0x2edf and
 *                five do not (docs/deviations.md D500).  A guarded arm is
 *                driven at exactly 0x2edf, where it must store, and at
 *                0x2ee0, where it must not -- and 0x2ee0 is `crc[0]`, so
 *                "it did not store" is read off the blob's own object rather
 *                than restated.  An unguarded arm is driven at 0x2ee0 too,
 *                where it DOES write `crc[0]`, on both sides.
 *
 *   the holes    9 is a hole in the fourteen-entry table and so is anything
 *                above 13.  Those are driven and required to move nothing but
 *                the two run counters and the hold-off.
 *
 * WHAT IS BOUNDED, AND WHY IT IS NOT A WEAKENING.  The counts are kept to six
 * per list.  A count of 384 is legal on the wire and walks both sides off the
 * end of the bit vector together -- D390 -- which is the blob's behaviour and
 * not a difference, and a test that faulted could report nothing.  The cursor
 * sweeps stop at 0x2ee0, which is `crc[0]` and still inside the object, for
 * the same reason: an unguarded arm at 0x3000 writes past the object on both
 * sides and there would be nothing to see.
 *
 * A COUNT OF ZERO STALLS THE RECEIVER -- D501 -- so the generator keeps one
 * count per block non-zero, and one trial deliberately does not, to hold that
 * reading.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90CP.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

/* NOT void: %edi is zeroed at entry and moved to %eax at both `ret`s. */
int ref_cp_bitstoinfo(void *, unsigned char) asm("ref__ZN5V90CP10bitsToInfoEh");
}

/* ------------------------------------------------------------------ seeds */

static unsigned lfsr;

static unsigned
step(void)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	return lfsr;
}

static unsigned char
next_byte(int mode)
{
	step();
	switch (mode) {
	case 1:
		return 0x01;			/* every byte a live bit     */
	case 2:
		return 0xff;			/* every bit set             */
	default:
		return (unsigned char)(lfsr >> 3);
	}
}

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/* ----------------------------------------------------------- the storage */

#define CP_SLOT		((unsigned)sizeof(V90CP) + 64u)
#define CP_POISON	0x5a5a5a5aUL
#define SEQ_MAX		2048u

static unsigned char cp_a[CP_SLOT] __attribute__((aligned(8)));
static unsigned char cp_b[CP_SLOT] __attribute__((aligned(8)));
static unsigned char cp_s[CP_SLOT];		/* the seed, for the guard  */
static unsigned char cp_g[CP_SLOT] __attribute__((aligned(8)));
static unsigned char cmp_a[CP_SLOT];
static unsigned char cmp_b[CP_SLOT];

static int bufs_a[V90CP_BUFS][V90CP_BUFENTS];
static int bufs_b[V90CP_BUFS][V90CP_BUFENTS];
static int bufs_g[V90CP_BUFS][V90CP_BUFENTS];

#define CPA	((V90CP *)cp_a)
#define CPB	((V90CP *)cp_b)
#define GEN	((V90CP *)cp_g)

/* The sequence the transmitter laid out, and what went into it. */
static unsigned char seq[SEQ_MAX];
static unsigned int seqlen;

/* ------------------------------------------------------- the comparison */

static void
guard_intact(long tag)
{
	unsigned n = CP_SLOT - (unsigned)sizeof(V90CP);

	diff_eq_int("ours stored past the object (%ld)",
		    memcmp(cp_a + sizeof(V90CP), cp_s + sizeof(V90CP), n) == 0,
		    1, tag);
	diff_eq_int("the blob stored past the object (%ld)",
		    memcmp(cp_b + sizeof(V90CP), cp_s + sizeof(V90CP), n) == 0,
		    1, tag);
}

/*
 * The two objects with the six pointer words poisoned to one constant, and
 * the six buffers on their own -- `evaluateInfo`'s `case 11` writes nothing
 * but the buffers, so a run comparing only the objects would pass while
 * decoding nothing at all.
 */
static void
compare_pair(const char *what, long tag)
{
	unsigned long off = (unsigned long)__builtin_offsetof(V90CP, buf);
	int k;

	memcpy(cmp_a, cp_a, CP_SLOT);
	memcpy(cmp_b, cp_b, CP_SLOT);
	for (k = 0; k < V90CP_BUFS; k++) {
		unsigned long p = CP_POISON;

		memcpy(cmp_a + off + (unsigned)k * sizeof(int *), &p,
		       sizeof(int *) < sizeof(p) ? sizeof(int *) : sizeof(p));
		memcpy(cmp_b + off + (unsigned)k * sizeof(int *), &p,
		       sizeof(int *) < sizeof(p) ? sizeof(int *) : sizeof(p));
	}

	diff_eq_obj_(__FILE__, __LINE__, what, "V90CP", cmp_a, cmp_b,
		     sizeof(V90CP), tag);
	for (k = 0; k < V90CP_BUFS; k++)
		diff_eq_int("buffer %ld matches",
			    memcmp(bufs_a[k], bufs_b[k],
				   sizeof(bufs_a[0])) == 0, 1,
			    tag * 10 + k);
	guard_intact(tag);
}

/* --------------------------------------------------------- the stimulus */

/*
 * Fill the generator and lay the message out.  `zerocounts` asks for the
 * D501 corner -- every count zero, which stalls the receiver.
 */
static void
make_message(int trial, int shortform, unsigned int group, int flags,
	     int zerocounts)
{
	int k, j;
	unsigned i;

	lfsr = 0x13c7u + 0x9e37u * (unsigned)trial + 0x2f19u * (unsigned)flags
	       + 0x51edu * group;

	memset(cp_g, 0, CP_SLOT);
	for (k = 0; k < V90CP_BUFS; k++) {
		memset(bufs_g[k], 0, sizeof(bufs_g[0]));
		GEN->buf[k] = bufs_g[k];
	}

	GEN->word_3ba8 = group;
	GEN->word_00 = shortform;
	GEN->word_ca0 = step() & 1u;
	GEN->byte_13 = (unsigned char)((unsigned)flags >> 3 & 1u);
	GEN->byte_10 = (signed char)(step() & 0x1fu);
	GEN->byte_11 = (unsigned char)(step() & 3u);
	GEN->byte_12 = (unsigned char)(step() & 1u);
	GEN->word_14 = (int)(step() & 0xffffu);
	GEN->word_04 = (flags & 1) ? 1 : 0;
	GEN->word_08 = (flags & 2) ? 1 : 0;
	GEN->word_0c = (flags & 4) ? 1 : 0;

	for (k = 0; k < 12; k++)
		GEN->word_18[k] = (int)(step() & 0xffu);

	for (k = 0; k < 4; k++) {
		/*
		 * One non-zero per block, because a block of zero entries
		 * stalls the receiver -- D501 -- and the stall is tested on
		 * its own rather than by accident here.
		 */
		if (zerocounts)
			GEN->nof_58[k] = 0;
		else if (k == 0)
			GEN->nof_58[k] = 1u + step() % 5u;
		else
			GEN->nof_58[k] = step() % 6u;
		for (j = 0; j < (int)GEN->nof_58[k]; j++)
			GEN->short_58[k][j] = (short)(step() & 0xffffu);
	}

	for (k = 0; k < V90CP_BUFS; k++) {
		GEN->word_c70[k] = (int)(step() & 0xfu);
		if (zerocounts)
			GEN->nof_buf[k] = 0;
		else if (k == 0)
			GEN->nof_buf[k] = 1u + step() % 5u;
		else
			GEN->nof_buf[k] = step() % 6u;
		for (j = 0; j < (int)GEN->nof_buf[k]; j++)
			bufs_g[k][j] = (int)(step() & 0xffffu);
	}

	GEN->infoToBits();

	seqlen = GEN->word_3bac;
	if (seqlen > SEQ_MAX)
		seqlen = SEQ_MAX;		/* cannot happen; bounded above */
	for (i = 0; i < seqlen; i++)
		seq[i] = GEN->bits[i];
}

/* Seed both receivers alike, and put them in the state `resetDetector` does. */
static void
seed_receivers(int trial, int mode, unsigned int group, int holdoff)
{
	unsigned i;
	int k, j;

	lfsr = 0x2f19u + 0x9e37u * (unsigned)trial + 0x51edu * (unsigned)mode;
	for (i = 0; i < CP_SLOT; i++) {
		unsigned char v = next_byte(mode);

		cp_a[i] = v;
		cp_b[i] = v;
		cp_s[i] = v;
	}
	for (k = 0; k < V90CP_BUFS; k++) {
		for (j = 0; j < V90CP_BUFENTS; j++) {
			int v = (int)step();

			bufs_a[k][j] = v;
			bufs_b[k][j] = v;
		}
		CPA->buf[k] = bufs_a[k];
		CPB->buf[k] = bufs_b[k];
	}

	CPA->word_cac = CPB->word_cac = 18;
	CPA->word_cb0 = CPB->word_cb0 = 0;
	CPA->word_ca4 = CPB->word_ca4 = 0;
	CPA->byte_ca9 = CPB->byte_ca9 = 0;
	CPA->byte_caa = CPB->byte_caa = 0;
	CPA->word_3ba8 = CPB->word_3ba8 = group;
	CPA->word_3bbc = CPB->word_3bbc = holdoff;
}

/* ------------------------------------------------- driving the sequence */

static unsigned int statemask;
static int reports;
static int last_rc;
static int state_at_16_ones;

/*
 * Feed one byte to both and compare the answer.  Every state change compares
 * the whole object, which is where a decode that went wrong shows up before
 * the sequence has run out.
 */
static void
feed_one(unsigned char v, long tag)
{
	unsigned int before = CPB->word_ca4;
	int ra = CPA->bitsToInfo(v);
	int rb = ref_cp_bitstoinfo(cp_b, v);

	diff_eq_int("the answer matches (%ld)", ra, rb, tag);
	if (rb != 0) {
		reports++;
		last_rc = rb;
	}
	statemask |= 1u << (CPB->word_ca4 & 31u);
	if (CPB->word_ca4 != before)
		compare_pair("at a state change", tag);
}

static void
run_sequence(long tagbase, int prefix16)
{
	unsigned int i;

	statemask = 1u << (CPB->word_ca4 & 31u);
	reports = 0;
	last_rc = 0;
	state_at_16_ones = -1;

	if (prefix16) {
		/*
		 * Sixteen ones and a zero.  `case 0` needs SEVENTEEN, so the
		 * blob must still be in state 0 when the sixteenth arrives.
		 */
		for (i = 0; i < 16; i++)
			feed_one(1, tagbase * 10000 + 9000 + (long)i);
		state_at_16_ones = (int)CPB->word_ca4;
		feed_one(0, tagbase * 10000 + 9100);
	}

	for (i = 0; i < seqlen; i++)
		feed_one(seq[i], tagbase * 10000 + (long)i);

	compare_pair("at the end of the sequence", tagbase);
}

/* What the two surviving bits say the answer should be.  Read off the BLOB. */
static int
expected_rc(void)
{
	if (CPB->byte_13 != 0)
		return (CPB->word_00 != 0) ? 4 : 2;
	return (CPB->word_00 != 0) ? 3 : 1;
}

/* ------------------------------------------------ the long form, in full */

static int
run_cp_b2i_message(void)
{
	int trial;
	int full_walk = 0, sixteen_seen = 0, decoded = 0, high_bytes = 0;

	diff_begin("V90CP::bitsToInfo -- the long form");
	set_level(0);

	for (trial = 0; trial < 24; trial++) {
		long tag = 100 + trial;
		int flags = trial & 7;			/* +0x04, +0x08, +0x0c */
		int b13 = (trial >> 3) & 1;
		int mode = trial % 3;
		unsigned int i;
		int k;

		make_message(trial, 0, 6u, flags | (b13 << 3), 0);
		seed_receivers(trial + 40, mode, 6u, -1);

		/*
		 * Every fourth trial sends the PAYLOAD's ones as 0xff.  The
		 * bit is stored WHOLE -- `mov %bl,0xcb8(...)` -- so the three
		 * fields copied out of `bits` unmasked come back 0xff while
		 * every field built by a shift loop is unchanged, and the CRC
		 * does not notice because 0xff is odd and it takes `& 1`.
		 *
		 * NOT the CRC's own sixteen bits, and that bound is the
		 * object's: `evaluateCRC` compares the register against them
		 * by absolute DIFFERENCE, not by parity, so a received 0xff
		 * against a computed 1 is a mismatch.  The payload ends at
		 * the framing bit before them, which is word_3bb0 - 0x11.
		 */
		if ((trial & 3) == 3) {
			unsigned int end = GEN->word_3bb0 - 0x11u;

			for (i = 0; i < seqlen && i < end; i++)
				if (seq[i] != 0)
					seq[i] = 0xff;
			high_bytes = 1;
		}

		run_sequence(tag, (trial & 1) ? 1 : 0);

		if (trial & 1) {
			diff_eq_int("sixteen ones left it in state 0 (%ld)",
				    state_at_16_ones, 0, tag);
			sixteen_seen = 1;
		}

		/* THE STATES THE BLOB ACTUALLY ENTERED, not the ones we hoped. */
		diff_eq_int("state 0 was entered (%ld)",
			    (statemask >> 0) & 1u, 1, tag);
		diff_eq_int("state 1 was entered (%ld)",
			    (statemask >> 1) & 1u, 1, tag);
		diff_eq_int("state 2 was entered (%ld)",
			    (statemask >> 2) & 1u, 1, tag);
		diff_eq_int("state 4 was entered (%ld)",
			    (statemask >> 4) & 1u, 1, tag);
		diff_eq_int("state 5 was entered (%ld)",
			    (statemask >> 5) & 1u, 1, tag);
		diff_eq_int("state 12 was entered (%ld)",
			    (statemask >> 12) & 1u, 1, tag);
		diff_eq_int("state 13 was entered (%ld)",
			    (statemask >> 13) & 1u, 1, tag);
		diff_eq_int("state 3 was NOT entered (%ld)",
			    (statemask >> 3) & 1u, 0, tag);
		diff_eq_int("state 9 was NOT entered (%ld)",
			    (statemask >> 9) & 1u, 0, tag);

		diff_eq_int("state 6 iff +0x04 (%ld)", (statemask >> 6) & 1u,
			    (flags & 1) ? 1 : 0, tag);
		diff_eq_int("state 7 iff +0x08 (%ld)", (statemask >> 7) & 1u,
			    (flags & 2) ? 1 : 0, tag);
		diff_eq_int("state 8 iff +0x08 (%ld)", (statemask >> 8) & 1u,
			    (flags & 2) ? 1 : 0, tag);
		diff_eq_int("state 10 iff +0x0c (%ld)", (statemask >> 10) & 1u,
			    (flags & 4) ? 1 : 0, tag);
		diff_eq_int("state 11 iff +0x0c (%ld)", (statemask >> 11) & 1u,
			    (flags & 4) ? 1 : 0, tag);
		if (flags == 7)
			full_walk = 1;

		/*
		 * The group size is six and the transmitter pads to a whole
		 * number of six, so the report lands on the LAST bit of the
		 * sequence and there is exactly one of them.
		 */
		diff_eq_int("exactly one answer (%ld)", reports, 1, tag);
		diff_eq_int("and it is what the two bits say (%ld)", last_rc,
			    expected_rc(), tag);
		diff_eq_int("the hold-off started (%ld)",
			    CPB->word_3bbc >= 0, 1, tag);

		/*
		 * DID IT ACTUALLY DECODE?  Read the blob's own fields back
		 * and compare them against what the transmitter was given.
		 * Without this a receiver that walked the states and stored
		 * nothing would pass everything above.
		 */
		diff_eq_int("+0x10 came back (%ld)", CPB->byte_10 & 0x1f,
			    GEN->byte_10 & 0x1f, tag);
		diff_eq_int("+0x11 came back (%ld)", CPB->byte_11,
			    GEN->byte_11, tag);
		diff_eq_int("+0x12 came back (%ld)", CPB->byte_12 != 0,
			    GEN->byte_12 != 0, tag);
		diff_eq_int("+0x13 came back (%ld)", CPB->byte_13 != 0,
			    GEN->byte_13 != 0, tag);
		diff_eq_int("+0x14 came back (%ld)", CPB->word_14,
			    GEN->word_14, tag);
		diff_eq_int("+0x00 is the type bit (%ld)", CPB->word_00 != 0,
			    0, tag);
		decoded = 1;

		if (flags & 1)
			for (k = 0; k < 12; k++)
				diff_eq_int("+0x18[%ld] came back",
					    CPB->word_18[k],
					    GEN->word_18[k], tag * 100 + k);
		if (flags & 2)
			for (k = 0; k < 4; k++) {
				unsigned int j;

				diff_eq_int("nof_58[%ld] came back",
					    (long)CPB->nof_58[k],
					    (long)GEN->nof_58[k],
					    tag * 100 + k);
				for (j = 0; j < GEN->nof_58[k]; j++)
					diff_eq_int("short_58 came back (%ld)",
						    CPB->short_58[k][j],
						    GEN->short_58[k][j],
						    tag * 100 + k);
			}
		if (flags & 4)
			for (k = 0; k < V90CP_BUFS; k++) {
				unsigned int j;

				diff_eq_int("word_c70[%ld] came back",
					    CPB->word_c70[k],
					    GEN->word_c70[k], tag * 100 + k);
				diff_eq_int("nof_buf[%ld] came back",
					    (long)CPB->nof_buf[k],
					    (long)GEN->nof_buf[k],
					    tag * 100 + k);
				for (j = 0; j < GEN->nof_buf[k]; j++)
					diff_eq_int("buf came back (%ld)",
						    bufs_b[k][j],
						    bufs_g[k][j],
						    tag * 100 + k);
			}
	}

	diff_eq_int("a message with all three blocks was walked", full_walk, 1,
		    0);
	diff_eq_int("the sixteen-ones corner was driven", sixteen_seen, 1, 0);
	diff_eq_int("the decode was checked against the transmitter", decoded,
		    1, 0);
	diff_eq_int("a sequence of 0xff ones was driven", high_bytes, 1, 0);
	return diff_end();
}

/* ------------------------------------------------------ the short form */

static int
run_cp_b2i_short(void)
{
	int trial, seen = 0;

	diff_begin("V90CP::bitsToInfo -- the short form");
	set_level(0);

	for (trial = 0; trial < 8; trial++) {
		long tag = 300 + trial;
		int b13 = trial & 1;

		make_message(trial + 50, 1, 6u, (b13 << 3), 0);
		seed_receivers(trial + 60, trial % 3, 6u, -1);
		run_sequence(tag, 0);

		diff_eq_int("state 3 was entered (%ld)",
			    (statemask >> 3) & 1u, 1, tag);
		diff_eq_int("state 4 was NOT entered (%ld)",
			    (statemask >> 4) & 1u, 0, tag);
		diff_eq_int("state 5 was NOT entered (%ld)",
			    (statemask >> 5) & 1u, 0, tag);
		diff_eq_int("state 12 was entered (%ld)",
			    (statemask >> 12) & 1u, 1, tag);
		diff_eq_int("state 13 was entered (%ld)",
			    (statemask >> 13) & 1u, 1, tag);

		diff_eq_int("exactly one answer (%ld)", reports, 1, tag);
		diff_eq_int("and it is what the two bits say (%ld)", last_rc,
			    expected_rc(), tag);

		/* The two bits `evaluateInfo`'s `case 3` reads back. */
		diff_eq_int("+0x00 is the type bit (%ld)", CPB->word_00 != 0,
			    1, tag);
		diff_eq_int("+0xca0 came back (%ld)", CPB->word_ca0 != 0,
			    GEN->word_ca0 != 0, tag);
		diff_eq_int("+0x13 came back (%ld)", CPB->byte_13 != 0,
			    GEN->byte_13 != 0, tag);
		seen = 1;
	}

	diff_eq_int("the short form was driven", seen, 1, 0);
	return diff_end();
}

/* ------------------------------------------------ all four answers, and
 *                                                   the hold-off          */

static int
run_cp_b2i_answers(void)
{
	int trial;
	int saw[6];
	int suppressed = 0, survived = 0, wrapped = 0;
	int i;

	for (i = 0; i < 6; i++)
		saw[i] = 0;

	diff_begin("V90CP::bitsToInfo -- the answer and the hold-off");
	set_level(0);

	for (trial = 0; trial < 16; trial++) {
		long tag = 500 + trial;
		int shortform = (trial >> 1) & 1;
		int b13 = trial & 1;
		int holdoff = (trial & 2) ? 0 : -1;
		int want;

		make_message(trial + 70, shortform, 6u, 7 | (b13 << 3), 0);
		seed_receivers(trial + 80, trial % 3, 6u, holdoff);
		run_sequence(tag, 0);

		want = expected_rc();
		if (want >= 0 && want < 6)
			saw[want] = 1;

		if (holdoff < 0) {
			diff_eq_int("idle: the answer comes through (%ld)",
				    last_rc, want, tag);
			/* 1 and 2 start it; 3 and 4 do not. */
			diff_eq_int("1 and 2 start the hold-off (%ld)",
				    CPB->word_3bbc >= 0,
				    (want == 1 || want == 2) ? 1 : 0, tag);
		} else if (want == 4 || want == 2) {
			diff_eq_int("running: 2 and 4 are suppressed (%ld)",
				    last_rc, 0, tag);
			suppressed = 1;
		} else {
			diff_eq_int("running: 1 and 3 are not (%ld)", last_rc,
				    want, tag);
			survived = 1;
		}
	}

	for (i = 1; i <= 4; i++)
		diff_eq_int("answer %ld was produced", saw[i], 1, i);
	diff_eq_int("an answer was suppressed", suppressed, 1, 0);
	diff_eq_int("an answer survived the hold-off", survived, 1, 0);

	/*
	 * And the wrap.  State 15 is a hole, so nothing but the run counters
	 * and the counter itself moves; 0x320 calls from 0 must land on -1.
	 */
	{
		long tag = 600;
		int n;

		seed_receivers(99, 0, 6u, 0);
		CPA->word_ca4 = CPB->word_ca4 = 15;
		for (n = 0; n < 0x320; n++) {
			int ra = CPA->bitsToInfo((unsigned char)(n & 1));
			int rb = ref_cp_bitstoinfo(cp_b,
						   (unsigned char)(n & 1));

			diff_eq_int("the answer matches (%ld)", ra, rb,
				    tag * 10000 + n);
			diff_eq_int("and it is nothing (%ld)", rb, 0,
				    tag * 10000 + n);
		}
		compare_pair("after 0x320 calls", tag);
		diff_eq_int("the hold-off wrapped to -1 (%ld)",
			    (long)CPB->word_3bbc, -1, tag);
		wrapped = 1;
	}
	diff_eq_int("the hold-off was driven to its wrap", wrapped, 1, 0);

	return diff_end();
}

/* ------------------------------------------------------------- bad CRC */

static int
run_cp_b2i_badcrc(void)
{
	int trial, seen = 0, above = 0, below = 0;
	unsigned lvl;

	diff_begin("V90CP::bitsToInfo -- the bad CRC");

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level(lvl);
		for (trial = 0; trial < 6; trial++) {
			long tag = (long)lvl * 1000 + 700 + trial;
			unsigned int flip;

			make_message(trial + 90, 0, 6u, 7, 0);

			/*
			 * Flip one INFORMATION bit inside +0x14, which is
			 * bits 0x23..0x32.  Never a framing bit, which the
			 * CRC does not cover, and never one of the three
			 * block flags at 0x13..0x15 -- those change which
			 * states the receiver walks, so it would never reach
			 * the CRC at all and the run would be measuring
			 * something else.
			 */
			flip = 0x23u + (unsigned)trial;
			seq[flip] = (unsigned char)(seq[flip] ? 0 : 1);

			seed_receivers(trial + 95, trial % 3, 6u, -1);

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			run_sequence(tag, 0);
			dsplib_debug_capture_on = 0;

			diff_eq_int("transcript matches (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("line counts match (%ld)",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1), tag);

			diff_eq_int("state 12 was reached (%ld)",
				    (statemask >> 12) & 1u, 1, tag);
			diff_eq_int("state 13 was NOT (%ld)",
				    (statemask >> 13) & 1u, 0, tag);
			diff_eq_int("nothing was reported (%ld)", reports, 0,
				    tag);
			diff_eq_int("and it reset to state 0 (%ld)",
				    (long)CPB->word_ca4, 0, tag);
			diff_eq_int("the cursor went home (%ld)",
				    (long)CPB->word_cac, 18, tag);
			seen = 1;

			if (lvl > 1) {
				diff_eq_int("the blob announced it (%ld)",
					    (int)dsplib_debug_capture_lines(1)
					    >= 1, 1, tag);
				above = 1;
			} else {
				diff_eq_int("below the gate, silence (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    0, tag);
				below = 1;
			}
		}
	}

	set_level(0);
	diff_eq_int("a bad CRC was driven", seen, 1, 0);
	diff_eq_int("the gate was tried open", above, 1, 0);
	diff_eq_int("the gate was tried shut", below, 1, 0);
	return diff_end();
}

/* ------------------------------------------------------------------ Ed */

static int
run_cp_b2i_ed(void)
{
	static const unsigned int groups[] = { 1u, 2u, 3u, 6u, 17u, 127u };
	int trial, seen = 0, zero_group = 0;

	diff_begin("V90CP::bitsToInfo -- the run counters");
	set_level(0);

	for (trial = 0; trial < 12; trial++) {
		long tag = 800 + trial;
		unsigned int g = groups[(unsigned)trial %
					(sizeof(groups) / sizeof(groups[0]))];
		unsigned int n;
		int fired = -1;

		seed_receivers(trial + 110, trial % 3, g, -1);

		for (n = 0; n < 2u * g + 4u; n++) {
			int ra = CPA->bitsToInfo(0);
			int rb = ref_cp_bitstoinfo(cp_b, 0);

			diff_eq_int("the answer matches (%ld)", ra, rb,
				    tag * 10000 + (long)n);
			if (rb == 5 && fired < 0)
				fired = (int)n;
		}
		compare_pair("after a run of zeros", tag);

		/*
		 * The (2g)th zero is the one: +0xcaa counts from 0, so it
		 * reaches 2g on call number 2g, which is index 2g-1.
		 */
		diff_eq_int("Ed fired on the 2*group'th zero (%ld)", fired,
			    (int)(2u * g) - 1, tag);
		seen = 1;
	}

	/*
	 * THE ZERO-GROUP CORNER.  With a group size of zero the object
	 * clears +0xcaa on a ONE bit and reloads it, so `+0xcaa == 2 * 0` is
	 * true and the answer is 5 -- which a body that kept the counter in a
	 * local would not produce.  A ZERO bit makes it 1 and gives nothing.
	 */
	{
		long tag = 900;
		int ra, rb;

		seed_receivers(120, 0, 0u, -1);
		ra = CPA->bitsToInfo(1);
		rb = ref_cp_bitstoinfo(cp_b, 1);
		diff_eq_int("the answer matches (%ld)", ra, rb, tag);
		diff_eq_int("a one bit with a zero group is Ed (%ld)", rb, 5,
			    tag);
		compare_pair("after the one bit", tag);

		seed_receivers(120, 0, 0u, -1);
		ra = CPA->bitsToInfo(0);
		rb = ref_cp_bitstoinfo(cp_b, 0);
		diff_eq_int("the answer matches (%ld)", ra, rb, tag + 1);
		diff_eq_int("a zero bit with a zero group is not (%ld)", rb, 0,
			    tag + 1);
		compare_pair("after the zero bit", tag + 1);
		zero_group = 1;
	}

	/* And the cursor has to be home: 18 and nothing else. */
	{
		long tag = 950;
		int ra, rb;

		seed_receivers(121, 0, 0u, -1);
		CPA->word_cac = CPB->word_cac = 19;
		ra = CPA->bitsToInfo(1);
		rb = ref_cp_bitstoinfo(cp_b, 1);
		diff_eq_int("the answer matches (%ld)", ra, rb, tag);
		diff_eq_int("a cursor of 19 gives nothing (%ld)", rb, 0, tag);
		compare_pair("with the cursor off home", tag);
	}

	diff_eq_int("the run of zeros was driven", seen, 1, 0);
	diff_eq_int("the zero-group corner was driven", zero_group, 1, 0);
	return diff_end();
}

/* -------------------------------------------------- the bit-vector guard */

static int
run_cp_b2i_guard(void)
{
	/* The five arms that bound the cursor, and the five that do not. */
	static const unsigned int guarded[] = { 3u, 8u, 10u, 11u, 12u };
	static const unsigned int open_arms[] = { 2u, 4u, 5u, 6u, 7u };
	int i, stored = 0, refused = 0, unguarded = 0, above = 0, below = 0;
	unsigned lvl;

	diff_begin("V90CP::bitsToInfo -- the bit-vector bound");

	for (lvl = 0; lvl <= 3; lvl++) {
		set_level(lvl);
		for (i = 0; i < 5; i++) {
			long tag = (long)lvl * 1000 + 1100 + i;
			unsigned int st = guarded[i];

			/* At the last index that fits: it must store. */
			seed_receivers(130 + i, i % 3, 6u, -1);
			CPA->word_ca4 = CPB->word_ca4 = st;
			CPA->word_cac = CPB->word_cac = V90CP_BITS - 1;
			CPA->word_cb0 = CPB->word_cb0 = 1;
			CPA->nof_58[0] = CPB->nof_58[0] = 0;

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			{
				int ra = CPA->bitsToInfo(1);
				int rb = ref_cp_bitstoinfo(cp_b, 1);

				diff_eq_int("the answer matches (%ld)", ra, rb,
					    tag);
			}
			dsplib_debug_capture_on = 0;

			compare_pair("at the last index", tag);
			diff_eq_int("the last byte was stored (%ld)",
				    (long)CPB->bits[V90CP_BITS - 1], 1, tag);
			diff_eq_int("the cursor moved on (%ld)",
				    (long)CPB->word_cac, V90CP_BITS, tag);
			diff_eq_int("and it was silent (%ld)",
				    (int)dsplib_debug_capture_lines(1), 0,
				    tag);
			stored = 1;

			/* One past it: it must refuse, and say so. */
			seed_receivers(140 + i, i % 3, 6u, -1);
			CPA->word_ca4 = CPB->word_ca4 = st;
			CPA->word_cac = CPB->word_cac = V90CP_BITS;
			CPA->word_cb0 = CPB->word_cb0 = 1;
			CPA->crc[0] = CPB->crc[0] = 0x37;
			cp_s[0x3b98] = 0x37;

			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();
			{
				int ra = CPA->bitsToInfo(1);
				int rb = ref_cp_bitstoinfo(cp_b, 1);

				diff_eq_int("the answer matches (%ld)", ra, rb,
					    tag + 500);
			}
			dsplib_debug_capture_on = 0;

			compare_pair("one past the last index", tag + 500);
			diff_eq_int("transcript matches (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag + 500);
			/*
			 * bits[V90CP_BITS] IS crc[0], so "it did not store" is
			 * read straight off the blob's own object.
			 */
			diff_eq_int("crc[0] was not touched (%ld)",
				    (long)CPB->crc[0], 0x37, tag + 500);
			diff_eq_int("the cursor did not move (%ld)",
				    (long)CPB->word_cac, V90CP_BITS,
				    tag + 500);
			refused = 1;

			if (lvl > 1) {
				diff_eq_int("it said so (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    1, tag + 500);
				above = 1;
			} else {
				diff_eq_int("below the gate, silence (%ld)",
					    (int)dsplib_debug_capture_lines(1),
					    0, tag + 500);
				below = 1;
			}
		}
	}

	/*
	 * AND THE FIVE THAT ARE NOT GUARDED.  At the same cursor these write
	 * crc[0] -- inside the object, so both sides can be compared and the
	 * difference from the guarded arms is what D500 records.
	 */
	set_level(0);
	for (i = 0; i < 5; i++) {
		long tag = 1300 + i;
		unsigned int st = open_arms[i];

		seed_receivers(150 + i, i % 3, 6u, -1);
		CPA->word_ca4 = CPB->word_ca4 = st;
		CPA->word_cac = CPB->word_cac = V90CP_BITS;
		CPA->word_cb0 = CPB->word_cb0 = 0;
		CPA->crc[0] = CPB->crc[0] = 0x37;
		cp_s[0x3b98] = 0x37;

		{
			int ra = CPA->bitsToInfo(1);
			int rb = ref_cp_bitstoinfo(cp_b, 1);

			diff_eq_int("the answer matches (%ld)", ra, rb, tag);
		}

		compare_pair("an unguarded arm past the end", tag);
		diff_eq_int("it wrote crc[0] (%ld)", (long)CPB->crc[0], 1,
			    tag);
		diff_eq_int("and moved on (%ld)", (long)CPB->word_cac,
			    V90CP_BITS + 1, tag);
		unguarded = 1;
	}

	diff_eq_int("a guarded arm stored at the last index", stored, 1, 0);
	diff_eq_int("a guarded arm refused past it", refused, 1, 0);
	diff_eq_int("an unguarded arm wrote past it", unguarded, 1, 0);
	diff_eq_int("the gate was tried open", above, 1, 0);
	diff_eq_int("the gate was tried shut", below, 1, 0);
	return diff_end();
}

/* ------------------------------------------------------------ the holes */

static int
run_cp_b2i_holes(void)
{
	static const unsigned int holes[] = {
		9u, 14u, 15u, 100u, 0x7fffffffu, 0xffffffffu
	};
	int trial, seen = 0;

	diff_begin("V90CP::bitsToInfo -- the holes in the table");
	set_level(0);

	for (trial = 0; trial < 24; trial++) {
		long tag = 1500 + trial;
		unsigned int st = holes[(unsigned)trial %
					(sizeof(holes) / sizeof(holes[0]))];
		unsigned char v = (unsigned char)((trial & 4) ? 0 : 1);
		unsigned char before[CP_SLOT];
		int ra, rb;

		seed_receivers(trial + 160, trial % 3, 6u, -1);
		CPA->word_ca4 = CPB->word_ca4 = st;
		CPA->word_cac = CPB->word_cac = 0x40 + (unsigned)trial;
		memcpy(before, cp_b, CP_SLOT);

		ra = CPA->bitsToInfo(v);
		rb = ref_cp_bitstoinfo(cp_b, v);

		diff_eq_int("the answer matches (%ld)", ra, rb, tag);
		diff_eq_int("and it is nothing (%ld)", rb, 0, tag);
		compare_pair("after a hole", tag);

		/* Nothing but the two run counters and the hold-off. */
		diff_eq_int("the state did not move (%ld)",
			    (long)(CPB->word_ca4 == st), 1, tag);
		diff_eq_int("the cursor did not move (%ld)",
			    (long)CPB->word_cac, 0x40 + trial, tag);
		diff_eq_int("the count did not move (%ld)",
			    memcmp(before + 0xcb0, cp_b + 0xcb0, 4) == 0, 1,
			    tag);
		diff_eq_int("the bit vector did not move (%ld)",
			    memcmp(before + 0xcb8, cp_b + 0xcb8,
				   V90CP_BITS) == 0, 1, tag);
		seen = 1;
	}

	diff_eq_int("the holes were driven", seen, 1, 0);
	return diff_end();
}

/* ------------------------------- a group size that is not six, and D501 */

static int
run_cp_b2i_corners(void)
{
	static const unsigned int groups[] = { 1u, 2u, 3u, 4u, 5u, 6u, 12u,
					       17u };
	int trial, early = 0, none = 0, stalled = 0;

	diff_begin("V90CP::bitsToInfo -- the group size, and a zero count");
	set_level(0);

	/*
	 * THE RECEIVER HARDCODES SIX.  The transmitter pads to a whole number
	 * of +0x3ba8, so with a group size that is not a multiple of six the
	 * report can land early or not at all, and both sides do the same
	 * thing.  Counted, not asserted per trial.
	 */
	for (trial = 0; trial < 16; trial++) {
		long tag = 1700 + trial;
		unsigned int g = groups[(unsigned)trial %
					(sizeof(groups) / sizeof(groups[0]))];

		make_message(trial + 170, trial & 1, g, 7, 0);
		seed_receivers(trial + 175, trial % 3, g, -1);
		run_sequence(tag, 0);

		if (reports == 0)
			none = 1;
		else if (last_rc != 0)
			early = 1;
	}
	diff_eq_int("a group size that reports was driven", early, 1, 0);
	diff_eq_int("a group size that does not was driven", none, 1, 0);

	/*
	 * D501: a counted block whose count is zero never satisfies
	 * `word_cb0 == alpha`, because the count is incremented before the
	 * test, so the receiver sits in state 8 and consumes the rest.
	 */
	for (trial = 0; trial < 4; trial++) {
		long tag = 1800 + trial;

		make_message(trial + 180, 0, 6u, 7, 1);
		seed_receivers(trial + 185, trial % 3, 6u, -1);
		run_sequence(tag, 0);

		diff_eq_int("state 8 was entered (%ld)",
			    (statemask >> 8) & 1u, 1, tag);
		diff_eq_int("and it never left (%ld)", (long)CPB->word_ca4, 8,
			    tag);
		diff_eq_int("nothing was reported (%ld)", reports, 0, tag);
		stalled = 1;
	}
	diff_eq_int("the zero-count stall was driven", stalled, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_cp_b2i_message();
	rc |= run_cp_b2i_short();
	rc |= run_cp_b2i_answers();
	rc |= run_cp_b2i_badcrc();
	rc |= run_cp_b2i_ed();
	rc |= run_cp_b2i_guard();
	rc |= run_cp_b2i_holes();
	rc |= run_cp_b2i_corners();

	return rc;
}
