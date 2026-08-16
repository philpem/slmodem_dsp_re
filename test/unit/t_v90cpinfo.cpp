/*
 * t_v90cpinfo.cpp -- differential test of the three V90CP members that carry
 * the message: `infoToBits`, `evaluateInfo` and `evaluateCRC`.
 *
 * THE SIX HEAP POINTERS ARE THE ONE THING THAT CANNOT AGREE, exactly as in
 * t_v90cp, so each side gets its OWN six buffers, the pointer words are
 * poisoned to a constant in a copy before the objects are compared, and the
 * buffer CONTENTS are compared separately.  Both matter: `evaluateInfo`'s
 * `case 11` writes nothing but the buffers, so a run that compared only the
 * objects would pass while decoding nothing at all.
 *
 * WHAT HAS TO BE BOUNDED, and why it is not a weakening.  Two members walk
 * `bits` forward by seventeen per entry with counts that come out of the
 * object, and `bits` is 12000 long: seed a count of 384 into all four lists
 * and the object's own loop runs off the end of the object, on BOTH sides.
 * That is the blob's behaviour and not a defect (docs/deviations.md D390),
 * but it is not a difference either, and a test that faulted could not report
 * anything.  So the counts are swept over 0..50 and the group size over a set
 * that includes 1, and the guard past the object then means what it says.
 *
 * THE ONE FIELD THAT IS NOT A SHIFT LOOP is +0x11.  `infoToBits` writes its
 * two bits from a four-arm switch, so a fifth value must leave bits[0x1b] and
 * bits[0x1c] HOLDING THE SEED.  Trials 5 and 7 of every group drive exactly
 * that, `saw_default` counts them, and the check reads the seed back rather
 * than restating our source -- which is what makes "replace the switch with a
 * two-bit shift loop" a mutation this test kills.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90CP.h"

extern "C" {
void ref_cp_evaluateinfo(void *) asm("ref__ZN5V90CP12evaluateInfoEv");
void ref_cp_infotobits(void *) asm("ref__ZN5V90CP10infoToBitsEv");

/* NOT void: the blob builds %eax with `sete`.  See V90CP.cpp. */
int ref_cp_evaluatecrc(void *) asm("ref__ZN5V90CP11evaluateCRCEv");
}

/* ------------------------------------------------------------------ seeds */

static unsigned lfsr;

static unsigned char
next_byte(int mode)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	switch (mode) {
	case 1:
		return 0x01;			/* every byte a live bit     */
	case 2:
		return 0xff;			/* every bit set             */
	default:
		return (unsigned char)(lfsr >> 3);
	}
}

/* ----------------------------------------------------------- the storage */

#define CP_SLOT		((unsigned)sizeof(V90CP) + 64u)
#define CP_POISON	0x5a5a5a5aUL
#define BITS_OFF	((unsigned)__builtin_offsetof(V90CP, bits))

static unsigned char cp_a[CP_SLOT] __attribute__((aligned(8)));
static unsigned char cp_b[CP_SLOT] __attribute__((aligned(8)));
static unsigned char cp_s[CP_SLOT];		/* the seed, for the guard  */
static unsigned char cmp_a[CP_SLOT];
static unsigned char cmp_b[CP_SLOT];

static int bufs_a[V90CP_BUFS][V90CP_BUFENTS];
static int bufs_b[V90CP_BUFS][V90CP_BUFENTS];

#define CPA	((V90CP *)cp_a)
#define CPB	((V90CP *)cp_b)

/*
 * The same varied bytes into both sides, into the objects and into the
 * buffers.  Never zeros -- finding 230 -- and the pointers are re-installed
 * afterwards because the seed has just walked over them.
 */
static void
seed_pair(int trial, int mode)
{
	unsigned i;
	int k;

	lfsr = 0x2f19u + 0x9e37u * (unsigned)trial + 0x51edu * (unsigned)mode;
	for (i = 0; i < CP_SLOT; i++) {
		unsigned char v = next_byte(mode);

		cp_a[i] = v;
		cp_b[i] = v;
		cp_s[i] = v;
	}
	for (k = 0; k < V90CP_BUFS; k++) {
		for (i = 0; i < V90CP_BUFENTS; i++) {
			int v = (int)(lfsr = (lfsr >> 1) ^
				      (-(int)(lfsr & 1u) & 0xb400u));

			bufs_a[k][i] = v;
			bufs_b[k][i] = v;
		}
		CPA->buf[k] = bufs_a[k];
		CPB->buf[k] = bufs_b[k];
	}
}

/* The guard past the end of the object, on BOTH sides, against the seed. */
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
 * Compare the two objects with the six pointer words poisoned to the same
 * constant, and the six buffers on their own.
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

	diff_eq_int("the six buffers agree (%ld)",
		    memcmp(bufs_a, bufs_b, sizeof(bufs_a)) == 0, 1, tag);
	guard_intact(tag);
}

/* The group sizes +0x3ba8 is divided by.  1 makes the rounding a no-op. */
static const unsigned int groups[] = { 17u, 1u, 16u, 8u, 34u, 5u };
#define NGROUPS	((int)(sizeof(groups) / sizeof(groups[0])))

/*
 * Put the message fields into a state the object can survive: counts small
 * enough that seventeen bits an entry stays inside `bits`, and a group size
 * that is not zero.
 */
static void
set_info(int trial)
{
	int k;

	CPA->word_00 = CPB->word_00 = (trial % 5 == 0) ? (trial | 1) : 0;
	CPA->word_04 = CPB->word_04 = (trial & 1);
	CPA->word_08 = CPB->word_08 = (trial & 2) >> 1;
	CPA->word_0c = CPB->word_0c = (trial & 4) >> 2;

	CPA->byte_10 = CPB->byte_10 = (signed char)(trial * 7);
	/* 0..3 are the switch's arms; 4 and 7 are the fifth value. */
	CPA->byte_11 = CPB->byte_11 =
	    (unsigned char)((trial % 8) == 5 ? 4 :
			    (trial % 8) == 7 ? 7 : (trial % 4));
	CPA->byte_12 = CPB->byte_12 = (unsigned char)(trial & 1);
	CPA->byte_13 = CPB->byte_13 = (unsigned char)((trial >> 1) & 1);
	CPA->word_14 = CPB->word_14 = (int)(0x1234u * (unsigned)(trial + 1));

	for (k = 0; k < 12; k++)
		CPA->word_18[k] = CPB->word_18[k] =
		    (int)(0x33u * (unsigned)(trial + k) - 0x1000);

	for (k = 0; k < 4; k++)
		CPA->nof_58[k] = CPB->nof_58[k] =
		    (unsigned int)((trial * (k + 3)) % 51);
	for (k = 0; k < 4; k++) {
		int j;

		for (j = 0; j < V90CP_SHORTS; j++)
			CPA->short_58[k][j] = CPB->short_58[k][j] =
			    (short)(0x5bu * (unsigned)(trial + k * 7 + j));
	}

	for (k = 0; k < V90CP_BUFS; k++) {
		CPA->nof_buf[k] = CPB->nof_buf[k] =
		    (unsigned int)((trial * (k + 2)) % 51);
		CPA->word_c70[k] = CPB->word_c70[k] =
		    (int)(0x17u * (unsigned)(trial + k));
	}

	CPA->word_ca0 = CPB->word_ca0 = (unsigned int)(trial * 3);
	CPA->word_3ba8 = CPB->word_3ba8 = groups[trial % NGROUPS];
}

/* --------------------------------------------- infoToBits (2785 bytes) */

static int
run_cp_infotobits(void)
{
	int trial;
	int saw_short = 0, saw_long = 0, saw_default = 0, varied = 0;
	int saw_gate_on = 0, saw_gate_off = 0, saw_pad = 0, saw_exact = 0;
	unsigned char first[CP_SLOT];

	diff_begin("V90CP::infoToBits");
	for (trial = 0; trial < 80; trial++) {
		unsigned char before[CP_SLOT];
		long tag = 5000 + trial;
		unsigned int len, want, n, g;
		int i;

		seed_pair(trial + 100, trial % 3);
		set_info(trial);
		memcpy(before, cp_b, CP_SLOT);

		CPA->infoToBits();
		ref_cp_infotobits(cp_b);

		compare_pair("after infoToBits", tag);
		diff_eq_int("infoToBits changed the object (%ld)",
			    memcmp(before, cp_b, sizeof(V90CP)) != 0, 1, tag);

		if (trial == 0)
			memcpy(first, cp_b, CP_SLOT);
		else if (memcmp(first, cp_b, sizeof(V90CP)) != 0)
			varied = 1;

		/* The frame grid, read off the BLOB's own vector. */
		for (i = 0; i <= 0x10; i++)
			if (CPB->bits[i] != 1)
				break;
		diff_eq_int("the preamble is seventeen ones (%ld)", i, 0x11,
			    tag);
		diff_eq_int("bit 17 is the framing zero (%ld)",
			    (long)CPB->bits[0x11], 0, tag);
		diff_eq_int("bit 18 is +0x00 (%ld)", (long)CPB->bits[0x12],
			    (long)(unsigned char)CPB->word_00, tag);

		if (CPB->word_00 != 0) {
			saw_short = 1;
			diff_eq_int("the short form put +0xca0 at 0x20 (%ld)",
				    (long)CPB->bits[0x20],
				    (long)(unsigned char)CPB->word_ca0, tag);
			diff_eq_int("the short form is 0x22 bits long (%ld)",
				    (long)CPB->word_3bb0, 0x22 + 0x11, tag);
		} else {
			saw_long = 1;
			diff_eq_int("bit 19 is +0x04 (%ld)",
				    (long)CPB->bits[0x13],
				    (long)(unsigned char)CPB->word_04, tag);
			diff_eq_int("bit 20 is +0x08 (%ld)",
				    (long)CPB->bits[0x14],
				    (long)(unsigned char)CPB->word_08, tag);
			diff_eq_int("bit 21 is +0x0c (%ld)",
				    (long)CPB->bits[0x15],
				    (long)(unsigned char)CPB->word_0c, tag);
			diff_eq_int("+0xca8 was cleared (%ld)",
				    (long)CPB->byte_ca8, 0, tag);

			/* Five bits of +0x10, least significant first. */
			{
				int v = 0;

				for (i = 4; i >= 0; i--)
					v = (v << 1) | CPB->bits[0x16 + i];
				diff_eq_int("bits 0x16..0x1a are +0x10 (%ld)",
					    v,
					    (long)(CPB->byte_10 & 0x1f), tag);
			}

			/* The switch, including the arm that stores nothing. */
			if (CPB->byte_11 <= 3) {
				diff_eq_int("bit 0x1b is +0x11 bit 0 (%ld)",
					    (long)CPB->bits[0x1b],
					    (long)(CPB->byte_11 & 1), tag);
				diff_eq_int("bit 0x1c is +0x11 bit 1 (%ld)",
					    (long)CPB->bits[0x1c],
					    (long)((CPB->byte_11 >> 1) & 1),
					    tag);
			} else {
				saw_default = 1;
				diff_eq_int("a fifth +0x11 left 0x1b alone "
					    "(%ld)", (long)CPB->bits[0x1b],
					    (long)cp_s[BITS_OFF + 0x1b], tag);
				diff_eq_int("a fifth +0x11 left 0x1c alone "
					    "(%ld)", (long)CPB->bits[0x1c],
					    (long)cp_s[BITS_OFF + 0x1c], tag);
			}

			diff_eq_int("bit 0x21 is +0x13 (%ld)",
				    (long)CPB->bits[0x21],
				    (long)CPB->byte_13, tag);
			diff_eq_int("bit 0x22 is a framing zero (%ld)",
				    (long)CPB->bits[0x22], 0, tag);

			if (CPB->word_04 || CPB->word_08 || CPB->word_0c)
				saw_gate_on = 1;
			else
				saw_gate_off = 1;
		}

		/* Every seventeenth bit inside the message is a framing zero. */
		len = CPB->word_3bb0;
		for (i = 0x11; (unsigned int)i < len; i += 17)
			if (CPB->bits[i] != 0)
				break;
		diff_eq_int("every framing bit is zero (%ld)",
			    (unsigned int)i >= len, 1, tag);

		/* The length, computed from the blob's own +0x3bb0. */
		n = CPB->word_3bb0 + 1;
		g = CPB->word_3ba8;
		want = (n / g) * g == n ? n : (n / g + 1) * g;
		diff_eq_int("+0x3bac is +0x3bb0+1 rounded up (%ld)",
			    (long)CPB->word_3bac, (long)want, tag);
		if (want == n)
			saw_exact = 1;
		else
			saw_pad = 1;

		/* The pad really is written, not just counted. */
		if (want > CPB->word_3bb0 + 1) {
			int all0 = 1;

			for (i = (int)CPB->word_3bb0 + 1; (unsigned int)i < want;
			     i++)
				if (CPB->bits[i] != 0)
					all0 = 0;
			diff_eq_int("the pad is zeroed (%ld)", all0, 1, tag);
		}
	}

	diff_eq_int("the object varied between trials", varied, 1, 0);
	diff_eq_int("the short form was built", saw_short, 1, 0);
	diff_eq_int("the long form was built", saw_long, 1, 0);
	diff_eq_int("a fifth value of +0x11 was tried", saw_default, 1, 0);
	diff_eq_int("all three block gates were off", saw_gate_off, 1, 0);
	diff_eq_int("at least one block gate was on", saw_gate_on, 1, 0);
	diff_eq_int("a length that needed no padding", saw_exact, 1, 0);
	diff_eq_int("a length that needed padding", saw_pad, 1, 0);

	return diff_end();
}

/* ------------------------------------------- evaluateInfo (1986 bytes) */

static int
run_cp_evaluateinfo(void)
{
	int trial;
	int varied = 0, saw_arm[14];
	int saw_live = 0, saw_dead = 0;
	unsigned char first[CP_SLOT];
	int s;

	for (s = 0; s < 14; s++)
		saw_arm[s] = 0;

	diff_begin("V90CP::evaluateInfo");
	for (trial = 0; trial < 84; trial++) {
		unsigned char before[CP_SLOT];
		long tag = 6000 + trial;
		unsigned int state = (unsigned int)(trial % 14);
		unsigned int i;

		seed_pair(trial + 200, trial % 3);
		set_info(trial);

		/*
		 * THE CURSOR IS SWEPT AND NOT SEEDED.  It is a bit index into
		 * a 12000-byte array and the arms walk it forward; a random
		 * word would leave the object on the first read.
		 */
		CPA->word_ca4 = CPB->word_ca4 = state;
		CPA->word_cb4 = CPB->word_cb4 =
		    (unsigned int)(0x32 + (trial % 24));

		/* Only 0 and 1 make sense as a decoded bit, but the arms mask
		 * with 1 themselves -- so seed the vector wide and let the
		 * blob be the oracle for what the masking does. */
		for (i = 0; i < 0x40; i++)
			CPA->bits[i] = CPB->bits[i] = cp_s[BITS_OFF + i];

		memcpy(before, cp_b, CP_SLOT);

		CPA->evaluateInfo();
		ref_cp_evaluateinfo(cp_b);

		compare_pair("after evaluateInfo", tag);

		saw_arm[state] = 1;
		if (memcmp(before, cp_b, sizeof(V90CP)) != 0 ||
		    memcmp(bufs_b, bufs_a, sizeof(bufs_a)) != 0)
			saw_live = 1;

		if (state == 0 || state == 4 || state == 9 || state == 12 ||
		    state == 13) {
			/*
			 * The holes in the jump table and the values outside
			 * it: nothing at all may move, buffers included.
			 */
			saw_dead = 1;
			diff_eq_int("state %ld stored nothing",
				    memcmp(before, cp_b, CP_SLOT) == 0, 1,
				    (long)state);
		}

		if (state == 5) {
			/* The header arm, read back off the blob's vector. */
			diff_eq_int("+0x12 took bit 0x1d (%ld)",
				    (long)CPB->byte_12,
				    (long)CPB->bits[0x1d], tag);
			diff_eq_int("+0x13 took bit 0x21 (%ld)",
				    (long)CPB->byte_13,
				    (long)CPB->bits[0x21], tag);
			diff_eq_int("+0x11 took bits 0x1b and 0x1c (%ld)",
				    (long)CPB->byte_11,
				    (long)((CPB->bits[0x1b] & 1) |
					   ((CPB->bits[0x1c] & 1) << 1)), tag);
			diff_eq_int("the cursor ended at 0x32 (%ld)",
				    (long)CPB->word_cb4, 0x32, tag);
		}
		if (state == 6)
			diff_eq_int("the cursor ended at 0x98 (%ld)",
				    (long)CPB->word_cb4, 0x98, tag);
		if (state == 3) {
			diff_eq_int("+0xca0 took bit 0x20 (%ld)",
				    (long)CPB->word_ca0,
				    (long)CPB->bits[0x20], tag);
			diff_eq_int("+0x13 took bit 0x21 (%ld)",
				    (long)CPB->byte_13,
				    (long)CPB->bits[0x21], tag);
		}

		if (trial == 0)
			memcpy(first, cp_b, CP_SLOT);
		else if (memcmp(first, cp_b, sizeof(V90CP)) != 0)
			varied = 1;
	}

	for (s = 3; s <= 11; s++)
		diff_eq_int("arm %ld was reached", saw_arm[s], 1, (long)s);
	diff_eq_int("a value below the table was tried", saw_arm[0], 1, 0);
	diff_eq_int("a value above the table was tried", saw_arm[13], 1, 0);
	diff_eq_int("the object varied between trials", varied, 1, 0);
	diff_eq_int("some arm decoded something", saw_live, 1, 0);
	diff_eq_int("some state decoded nothing", saw_dead, 1, 0);

	return diff_end();
}

/* -------------------------------------------- evaluateCRC (668 bytes) */

static int
run_cp_evaluatecrc(void)
{
	int trial;
	int saw_good = 0, saw_bad = 0, saw_wrap = 0;

	diff_begin("V90CP::evaluateCRC");
	for (trial = 0; trial < 48; trial++) {
		long tag = 7000 + trial;
		int ours, theirs;
		int mode = trial % 3;	/* 0 intact, 1 a flipped bit, 2 the wrap */

		seed_pair(trial + 300, trial % 3);
		set_info(trial);
		CPA->word_3ba8 = CPB->word_3ba8 = 17;

		/*
		 * Build a real sequence first -- with the blob, so that what
		 * `evaluateCRC` is handed is the blob's own output and not
		 * ours.  Then copy it across verbatim.
		 */
		ref_cp_infotobits(cp_b);
		memcpy(cp_a, cp_b, CP_SLOT);
		{
			int k;

			for (k = 0; k < V90CP_BUFS; k++) {
				CPA->buf[k] = bufs_a[k];
				CPB->buf[k] = bufs_b[k];
			}
		}

		if (mode == 1) {
			unsigned int at = 0x12 + (unsigned int)trial %
					  (CPB->word_3bb0 - 0x22);

			if (at % 17 == 0)
				at++;
			CPA->bits[at] = CPB->bits[at] =
			    (unsigned char)(CPB->bits[at] ^ 1);
		} else if (mode == 2) {
			/*
			 * THE ACCUMULATOR IS ONE BYTE AND CAN WRAP.  Two of
			 * the sixteen received CRC bits are moved 128 away
			 * from what this end computed, so the absolute
			 * differences are 128 and 128 -- 256, which is zero in
			 * a byte and not in anything wider.  Nothing else in
			 * this file reaches that: every other trial's sixteen
			 * differences are 0 or 1.  The blob is the oracle for
			 * what a byte does, and it answers "matches".
			 */
			unsigned int at = CPB->word_3bb0 - 0x10;

			CPA->bits[at] = CPB->bits[at] =
			    (unsigned char)(CPB->bits[at] + 128);
			CPA->bits[at + 1] = CPB->bits[at + 1] =
			    (unsigned char)(CPB->bits[at + 1] + 128);
		}

		ours = CPA->evaluateCRC();
		theirs = ref_cp_evaluatecrc(cp_b);

		diff_eq_int("evaluateCRC returned (%ld)", ours, theirs, tag);
		compare_pair("after evaluateCRC", tag);

		if (theirs)
			saw_good = 1;
		else
			saw_bad = 1;

		if (mode == 2) {
			unsigned int at = CPB->word_3bb0 - 0x10;

			/*
			 * Both halves of the claim, read off the BLOB: the two
			 * bits really are 128 or more, so they cannot equal a
			 * register that only ever holds 0 or 1 -- and the blob
			 * accepted the sequence anyway.  That is the byte
			 * wrapping, and it is the only thing that makes a
			 * widened accumulator a catchable mistake.
			 */
			diff_eq_int("the two bits are out of range (%ld)",
				    CPB->bits[at] >= 128 &&
				    CPB->bits[at + 1] >= 128, 1, tag);
			diff_eq_int("and the blob accepted anyway (%ld)",
				    theirs, 1, tag);
			saw_wrap = 1;
		}
	}

	diff_eq_int("an intact sequence was accepted", saw_good, 1, 0);
	diff_eq_int("a corrupted sequence was rejected", saw_bad, 1, 0);
	diff_eq_int("the byte accumulator was made to wrap", saw_wrap, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_cp_infotobits();
	rc |= run_cp_evaluateinfo();
	rc |= run_cp_evaluatecrc();

	return rc;
}
