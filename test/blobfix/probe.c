/*
 * probe.c -- drive the two table lookups `blobfix.py` repairs, over their
 * whole input domain, and say nothing about whether the answer is right.
 *
 * The answer is not the evidence here.  For D1 the patched and unpatched
 * objects return the SAME value on every input -- that is the whole reason
 * D1 is the safe rehearsal -- so a test that compares results proves nothing.
 * What has to be observed is WHICH MEMORY WAS READ, and this program is the
 * instrument three different observations are made with:
 *
 *   `addr`  prints where each table ended up, so the driver can see whether
 *           the reference was redirected and whether the tables are still
 *           adjacent (the adjacency IS the defect's camouflage).
 *
 *   `sqrt`  and `div` dump every result over the legal domain.  Run against
 *           a build whose replacement table carries a SENTINEL in its new
 *           last entry, the inputs whose output changes are exactly the
 *           inputs that read that entry -- so the dump identifies the
 *           out-of-bounds population without the program knowing the index
 *           expression.
 *
 * The same dumps also carry the other half of the argument: against a build
 * whose replacement table holds the GENERATED value, D1 must produce a dump
 * identical to the unpatched one, byte for byte, and D4 must differ on
 * exactly the inputs the sentinel identified.
 */

#include <stdio.h>
#include <string.h>

unsigned short FPM_sqrt(unsigned short x);
int FPM_div(unsigned short denom, unsigned short *recip, unsigned short *shift);
int FPM_div_32(unsigned int denom, unsigned short *recip, unsigned short *shift);

extern const unsigned short FPM_sqrt_table[];
extern const unsigned short FPM_div_table[];
extern const unsigned short FPM_xor_table[];

int
main(int argc, char **argv)
{
	unsigned i;

	if (argc != 2) {
		fprintf(stderr, "usage: probe addr|sqrt|div\n");
		return 2;
	}

	if (!strcmp(argv[1], "addr")) {
		printf("FPM_sqrt_table %p\n", (const void *)FPM_sqrt_table);
		printf("FPM_div_table  %p\n", (const void *)FPM_div_table);
		printf("FPM_xor_table  %p\n", (const void *)FPM_xor_table);
		return 0;
	}

	/* The Q15 contract: 0x0000..0x7fff.  Above that is D2, not D1. */
	if (!strcmp(argv[1], "sqrt")) {
		for (i = 0; i <= 0x7fff; i++)
			printf("%04x %04x\n", i,
			       FPM_sqrt((unsigned short)i));
		return 0;
	}

	/* Zero is the documented error return, so the sweep starts at one. */
	if (!strcmp(argv[1], "div")) {
		for (i = 1; i <= 0xffff; i++) {
			unsigned short recip = 0, shift = 0;
			int rc = FPM_div((unsigned short)i, &recip, &shift);

			printf("%04x %d %04x %04x\n", i, rc, recip, shift);
		}
		return 0;
	}

	/*
	 * `FPM_div_table`'s SECOND consumer, which nothing else in this tree
	 * drives -- `FPM_div_32` has no reconstruction, so no differential
	 * test reaches it, and the bound that says 129 entries are enough was
	 * derived from `FPM_div` alone.  It is not a formality: the sqrt
	 * pair's 32-bit sibling truncates its mantissa and can drive the
	 * index NEGATIVE, saved only by an unsigned clamp `FPM_div_32` does
	 * not have.
	 *
	 * Swept over the MANTISSA rather than over the input.  The index
	 * depends only on `x >> 16` once `x` is normalised to have bit 31
	 * set, so `x = m << 16` for m in [0x8000, 0xffff] enters the loop
	 * already normalised and covers every mantissa the function can ever
	 * form -- 32768 of them, which is exhaustive over the index domain
	 * rather than merely a large sample of inputs.
	 */
	if (!strcmp(argv[1], "div32")) {
		for (i = 0x8000; i <= 0xffff; i++) {
			unsigned short recip = 0, shift = 0;
			int rc = FPM_div_32(i << 16, &recip, &shift);

			printf("%04x %d %04x %04x\n", i, rc, recip, shift);
		}
		return 0;
	}

	fprintf(stderr, "probe: unknown mode %s\n", argv[1]);
	return 2;
}
