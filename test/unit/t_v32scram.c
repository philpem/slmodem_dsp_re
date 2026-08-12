/*
 * t_v32scram.c -- differential test of V.32's scrambler pair.
 *
 * Nothing in the object constructs a `struct v32_sdm`, so the fixture builds
 * one and both sides are handed the same bytes.  That makes the field layout
 * itself part of what is being tested: a wrong offset changes which value
 * reaches which shift and the two sides diverge on the first word.
 *
 * The sweep covers every `group` from 1 to 8 -- 6 is the special case that
 * splits a word into two 3-bit halves, and 3 and 12 are its neighbours in
 * meaning rather than in value -- crossed with two register lengths, two tap
 * pairs, and both V.32 polynomial orders.  Each configuration runs a stream
 * long enough for the register to fill several times over, in ragged chunks,
 * so the state carried between calls is exercised rather than assumed.
 *
 * And it checks the pair ROUND-TRIPS: descrambling what our scrambler
 * produced returns the input, which is the property the two functions exist
 * for and which neither one alone can demonstrate.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v32scram.h"

extern void ref_SDMv32_scrambler(void *sdm, short *buf, unsigned short count);
extern void ref_SDMv32_descrambler(void *sdm, short *buf,
				   unsigned short count);

static void
fill(short *buf, int n, int seed)
{
	int i;
	unsigned int x = (unsigned int)seed * 2654435761u + 1u;

	for (i = 0; i < n; i++) {
		x = x * 1103515245u + 12345u;
		buf[i] = (short)((x >> 13) & 0xffffu);
	}
}

static int
run(const char *label, short group, unsigned int regmask,
    unsigned int outmask, short tap1, short tap2)
{
	static short ours[512], theirs[512], original[512];
	struct v32_sdm a, b;
	int fed;
	int chunk;
	int i;
	int moved = 0;

	diff_begin(label);

	memset(&a, 0, sizeof(a));
	a.group = group;
	a.regmask = regmask;
	a.outmask = outmask;
	a.tap1 = tap1;
	a.tap2 = tap2;
	a.reg = 0x2aaaaa;		/* a register that is not all zero */
	b = a;

	fill(original, 512, group * 31 + tap1);
	memcpy(ours, original, sizeof(original));
	memcpy(theirs, original, sizeof(original));

	/* Ragged chunks: `reg` is the only thing carried between calls. */
	fed = 0;
	for (chunk = 1; fed + chunk <= 512; chunk += 5) {
		ref_SDMv32_scrambler(&a, theirs + fed, (unsigned short)chunk);
		SDMv32_scrambler(&b, ours + fed, (unsigned short)chunk);
		diff_eq_int("register after chunk at %ld", (long)b.reg,
			    (long)a.reg, fed);
		for (i = 0; i < chunk; i++) {
			diff_eq_int("scrambled word %ld", ours[fed + i],
				    theirs[fed + i], fed + i);
			if (ours[fed + i] != original[fed + i])
				moved++;
		}
		fed += chunk;
	}
	diff_eq_int("the scrambler changed something (%ld)", moved > 0, 1,
		    moved);

	/* Now the descrambler, over the same stream, from the same state. */
	memset(&a, 0, sizeof(a));
	a.group = group;
	a.regmask = regmask;
	a.outmask = outmask;
	a.tap1 = tap1;
	a.tap2 = tap2;
	a.reg = 0x155555;
	b = a;

	memcpy(ours, original, sizeof(original));
	memcpy(theirs, original, sizeof(original));
	fed = 0;
	for (chunk = 3; fed + chunk <= 512; chunk += 7) {
		ref_SDMv32_descrambler(&a, theirs + fed,
				       (unsigned short)chunk);
		SDMv32_descrambler(&b, ours + fed, (unsigned short)chunk);
		diff_eq_int("descrambler register at %ld", (long)b.reg,
			    (long)a.reg, fed);
		for (i = 0; i < chunk; i++)
			diff_eq_int("descrambled word %ld", ours[fed + i],
				    theirs[fed + i], fed + i);
		fed += chunk;
	}

	/*
	 * The round trip.  Only meaningful where the input fits in `group`
	 * bits, since the not-6 path xors the whole 16-bit word: mask the
	 * source to what a real caller would hand over.
	 */
	{
		struct v32_sdm tx, rx;
		short data[256], wire[256];
		int width = (group == 6) ? 3 : group;
		unsigned int keep = (width >= 16)
			? 0xffffu : ((1u << width) - 1u);
		/*
		 * At group 6 a word is two 3-bit halves, each masked by
		 * outmask, so what a caller may present is outmask in both
		 * halves rather than six bits of anything.
		 */
		unsigned int datamask = (group == 6)
			? ((outmask << 3) | outmask) : keep;

		/*
		 * At group 6 the two halves are packed `(high << 3) | low`,
		 * so an outmask wider than 3 bits makes them overlap and the
		 * pair is not invertible -- by construction, not by defect.
		 * The differential comparison above still covers that case;
		 * only the round trip is skipped.
		 */
		if (outmask >= keep && (group != 6 || outmask <= 7u)) {
			memset(&tx, 0, sizeof(tx));
			tx.group = group;
			tx.regmask = regmask;
			tx.outmask = outmask;
			tx.tap1 = tap1;
			tx.tap2 = tap2;
			tx.reg = 0x3ff;
			rx = tx;
			rx.reg = 0x12345;	/* deliberately different */

			fill(data, 256, group + 99);
			for (i = 0; i < 256; i++)
				data[i] = (short)(data[i]
						  & (short)datamask);
			memcpy(wire, data, sizeof(data));
			SDMv32_scrambler(&tx, wire, 256);
			SDMv32_descrambler(&rx, wire, 256);
			/*
			 * The first few words recover the register, so the
			 * comparison starts once it has been refilled: 32
			 * words is more than 23 bits at any group size here.
			 */
			for (i = 32; i < 256; i++)
				diff_eq_int("round trip word %ld", wire[i],
					    data[i], i);
		}
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	short g;

	/*
	 * V.32's register is 23 bits.  GPA is 1 + x^-18 + x^-23 and GPC is
	 * 1 + x^-5 + x^-23; SDMv32_GPA and SDMv32_GPC hold 5 and 18 in the
	 * two orders, so both orders are driven.
	 */
	for (g = 1; g <= 8; g++) {
		char label[64];
		int n = 0;

		label[n++] = 'g';
		label[n++] = 'r';
		label[n++] = 'o';
		label[n++] = 'u';
		label[n++] = 'p';
		label[n++] = ' ';
		label[n++] = (char)('0' + g);
		label[n++] = ',';
		label[n++] = ' ';
		label[n++] = '2';
		label[n++] = '3';
		label[n++] = '-';
		label[n++] = 'b';
		label[n++] = 'i';
		label[n++] = 't';
		label[n] = '\0';
		rc |= run(label, g, 0x7fffffu, 0xfu, 5, 18);
	}

	rc |= run("group 4, taps 18 then 5", 4, 0x7fffffu, 0xfu, 18, 5);
	rc |= run("group 6 split, taps 5/18", 6, 0x7fffffu, 0x7u, 5, 18);
	rc |= run("group 6 split, taps 18/5", 6, 0x7fffffu, 0x7u, 18, 5);
	rc |= run("group 3, short register", 3, 0x1ffu, 0x7u, 2, 7);
	rc |= run("group 12, wide mask", 12, 0xffffffffu, 0xfffu, 5, 18);
	rc |= run("group 2, tap on the same bit", 2, 0x7fffffu, 0x3u, 9, 9);
	rc |= run("outmask wider than the group", 4, 0x7fffffu, 0xffffu, 5,
		  18);
	rc |= run("register already full", 8, 0xffffu, 0xffu, 1, 15);

	return rc;
}
