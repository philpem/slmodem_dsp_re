/*
 * t_v90p3mod.cpp -- differential test of V90Phase3Modulator.
 *
 * The fixture is t_v90jd.cpp's, with the two differences this class forces:
 *
 * THE OBJECT IS SEEDED WITH VARIED BYTES, NEVER ZEROED.  Both sides get the
 * same pseudorandom fill before every call, reseeded each trial, so a clear
 * loop that stops one element short is visible and a field neither side
 * writes cannot pass by accident (findings 223, 224).
 *
 * BUT `pcmType` IS ALWAYS FORCED TO 0 OR 1.  `resetDILGenerator` indexes
 * `codeSegmentsBoundriesLookupTable` at `8 * pcmType`, and the table is
 * sixteen ints, so a random 32-bit `pcmType` would read wildly out of bounds
 * -- out of OUR table on our side and out of the REFERENCE table on theirs,
 * which are different objects at different addresses.  That is not a test of
 * anything.  Both values are exercised, and each is exercised against every
 * seed mode.
 *
 * THE OBJECT IS COMPARED WHOLE, AND SO IS A GUARD PAST ITS END.
 * `diff_eq_obj` covers `sizeof(V90Phase3Modulator)` = 920; the bytes from
 * there to the end of an over-large slot are compared separately, so a store
 * that overruns the object fails rather than passing in silence.  920 is the
 * largest displacement, +0x394, plus the width of the byte stored there,
 * rounded up for the four-byte members -- finding 229's rule.
 *
 * `resetDILGenerator` touches no pointer field, which is why the whole-object
 * comparison works here without the skip-and-compare-offsets form that
 * `reset` needs.
 *
 * The `ref_` aliases are reached through asm() labels rather than by spelling
 * the alias as an identifier, which sidesteps finding 225 entirely.  The
 * convention is plain cdecl with `this` as the first stack argument (finding
 * 215); both symbols are `T` in the blob, so no regparm is involved.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90Phase3Modulator.h"

extern "C" {
void ref_setSessionFlag(void *self, unsigned int flag)
	asm("ref__ZN18V90Phase3Modulator14setSessionFlagEj");
void ref_resetDILGenerator(void *self, const void *d)
	asm("ref__ZN18V90Phase3Modulator17resetDILGeneratorEPK19tagV90DILdescriptor");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT 1024

union mod_slot {
	V90Phase3Modulator o;
	unsigned char raw[SLOT];
};

static union mod_slot ours, theirs;
static tagV90DILdescriptor desc;

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * `mode` picks how varied the fill is.  The DIL expansion masks every code
 * with 0x7f before companding, so a fill whose high bit is constant and one
 * whose low seven bits are constant probe different halves of the path; the
 * all-0xa5 case is the harness's own malloc fill, which is what a field
 * neither side writes would look like if the object were not seeded at all.
 */
static void
seed(int trial, int mode)
{
	int i;

	lfsr_state = 0x1234u + 0x9e37u * (unsigned)trial + (unsigned)mode;

	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		switch (mode) {
		case 0:
			v = next_byte();
			break;
		case 1:
			v = 0xa5;
			break;
		case 2:
			v = (unsigned char)(next_byte() | 0x80);
			break;
		default:
			v = (unsigned char)(next_byte() & 0x7f);
			break;
		}
		ours.raw[i] = v;
		theirs.raw[i] = v;
	}
}

static void
seed_descriptor(int trial, int mode)
{
	unsigned char *p = (unsigned char *)&desc;
	unsigned int i;

	lfsr_state = 0x5eedu + 0x4f1bu * (unsigned)trial + (unsigned)mode;

	for (i = 0; i < sizeof(desc); i++) {
		switch (mode) {
		case 1:
			p[i] = 0;
			break;
		case 2:
			p[i] = 0xff;
			break;
		default:
			p[i] = next_byte();
			break;
		}
	}

	/*
	 * The two sequence lengths are kept inside their 128-byte
	 * destinations.  The object clamps neither, so a larger value would
	 * still compare equal on both sides -- but it would be testing the
	 * overrun rather than the copy, and `dilCount` below is the field that
	 * exercises a long loop.
	 */
	desc.seq1Length = (unsigned char)((trial * 13u + 1u) % 129u);
	desc.seq2Length = (unsigned char)((trial * 29u + 7u) % 129u);
	desc.dilCount = (unsigned char)(trial * 37u + 1u);
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + sizeof(V90Phase3Modulator),
		      theirs.raw + sizeof(V90Phase3Modulator),
		      SLOT - sizeof(V90Phase3Modulator)) == 0;
}

#define NTRIAL 32

static int
run_setsessionflag(void)
{
	int trial, moved = 0;

	diff_begin("V90Phase3Modulator::setSessionFlag");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned int flag = 0x51a70000u + (unsigned)trial;
		unsigned char before[SLOT];

		seed(trial, trial % 4);
		memcpy(before, ours.raw, SLOT);

		ours.o.setSessionFlag(flag);
		ref_setSessionFlag(&theirs.o, flag);

		diff_eq_obj("after setSessionFlag", V90Phase3Modulator,
			    &ours.o, &theirs.o, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("setSessionFlag stored the flag (trial %ld)",
			    ours.o.sessionFlag, (long)flag, trial);
		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
	}

	diff_eq_int("setSessionFlag changed the object", moved, 1, 0);

	return diff_end();
}

static int
run_resetdilgenerator(void)
{
	unsigned char first[SLOT];
	int trial, moved = 0, distinct = 0;
	int saw_index7 = 0, saw_index8 = 0, saw_negative = 0;

	diff_begin("V90Phase3Modulator::resetDILGenerator");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned char before[SLOT];
		PcmType law = (trial & 1) ? PCM_TYPE_A_LAW : PCM_TYPE_MU_LAW;

		seed(trial, trial % 4);
		seed_descriptor(trial, trial % 3);

		/* See the file comment: this one field cannot be random. */
		ours.o.pcmType = theirs.o.pcmType = law;
		memcpy(before, ours.raw, SLOT);

		ours.o.resetDILGenerator(&desc);
		ref_resetDILGenerator(&theirs.o, &desc);

		diff_eq_obj("after resetDILGenerator", V90Phase3Modulator,
			    &ours.o, &theirs.o, trial);
		diff_eq_int("no store past the object (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("resetDILGenerator copied dilCount (trial %ld)",
			    ours.o.dilCount, desc.dilCount, trial);

		if (memcmp(before, ours.raw, SLOT) != 0)
			moved = 1;
		if (trial == 0)
			memcpy(first, ours.raw, SLOT);
		else if (memcmp(first, ours.raw, SLOT) != 0)
			distinct = 1;
	}

	/*
	 * Every PCM code, both laws, one DIL entry.  The random descriptors
	 * above leave two things untested, and mutation showed it: the
	 * segment search never reached its last row entry, and it never saw a
	 * level the object reads back as more than 0x7fff.  Sweeping the code
	 * that produces `dilLevel[0]` reaches both -- the level is a signed
	 * short and the search zero-extends it, so half the sweep is above
	 * every boundary and lands on the index one past the end of the row.
	 */
	for (trial = 0; trial < 512; trial++) {
		PcmType law = (trial & 0x100) ? PCM_TYPE_A_LAW
					      : PCM_TYPE_MU_LAW;

		seed(trial, trial % 4);
		seed_descriptor(trial, 0);
		ours.o.pcmType = theirs.o.pcmType = law;
		desc.dilCount = 1;
		desc.dilCode[0] = (unsigned char)(trial & 0xff);

		ours.o.resetDILGenerator(&desc);
		ref_resetDILGenerator(&theirs.o, &desc);

		diff_eq_obj("after resetDILGenerator, code sweep",
			    V90Phase3Modulator, &ours.o, &theirs.o, trial);
		diff_eq_int("no store past the object, sweep (trial %ld)",
			    guard_equal(), 1, trial);
		if (ours.o.segmentIndex == 8)
			saw_index8 = 1;
		if (ours.o.segmentIndex == 7)
			saw_index7 = 1;
		if (ours.o.dilLevel[0] < 0)
			saw_negative = 1;
	}

	/*
	 * `dilCount` = 0, with `dilLevel[0]` set directly.  The sweep above
	 * cannot make the segment search see a large or negative level,
	 * because every code the expansion produces has its top bit set and
	 * this library's companding puts that half above zero -- so
	 * `dilLevel[0]` after a non-empty expansion is always in 0..0x7fff and
	 * always inside the first seven boundaries.
	 *
	 * With no DIL entries the field is not written, and the search reads
	 * back whatever was already there.  That is the only path on which the
	 * zero-extension is observable, and it is the path that reaches the
	 * index one past the end of the row.  Both sides get the same value,
	 * so this is still a comparison and not a fixture of our own making.
	 */
	for (trial = 0; trial < 2 * 34; trial++) {
		static const unsigned short probe[34] = {
			0, 1, 123, 124, 125, 255, 256, 257, 379, 380, 381,
			511, 512, 513, 891, 892, 893, 1915, 1916, 1917,
			3963, 3964, 3965, 8059, 8060, 8061, 16251, 16252,
			16253, 32635, 32636, 32637, 32768, 65535
		};
		PcmType law = (trial >= 34) ? PCM_TYPE_A_LAW
					    : PCM_TYPE_MU_LAW;
		short level = (short)probe[trial % 34];

		seed(trial, trial % 4);
		seed_descriptor(trial, 0);
		ours.o.pcmType = theirs.o.pcmType = law;
		desc.dilCount = 0;
		ours.o.dilLevel[0] = theirs.o.dilLevel[0] = level;

		ours.o.resetDILGenerator(&desc);
		ref_resetDILGenerator(&theirs.o, &desc);

		diff_eq_obj("after resetDILGenerator, level probe",
			    V90Phase3Modulator, &ours.o, &theirs.o, trial);
		diff_eq_int("no store past the object, probe (trial %ld)",
			    guard_equal(), 1, trial);
		if (ours.o.segmentIndex == 8)
			saw_index8 = 1;
		if (ours.o.segmentIndex == 7)
			saw_index7 = 1;
		if (ours.o.dilLevel[0] < 0)
			saw_negative = 1;
	}

	/* The null descriptor path, which is the whole of the error handling. */
	for (trial = 0; trial < 4; trial++) {
		seed(trial + NTRIAL, trial % 4);
		ours.o.pcmType = theirs.o.pcmType =
		    (trial & 1) ? PCM_TYPE_A_LAW : PCM_TYPE_MU_LAW;
		ours.o.dilCount = theirs.o.dilCount =
		    (unsigned char)(trial | 0x40);

		ours.o.resetDILGenerator(NULL);
		ref_resetDILGenerator(&theirs.o, NULL);

		diff_eq_obj("after resetDILGenerator(NULL)",
			    V90Phase3Modulator, &ours.o, &theirs.o, trial);
		diff_eq_int("no store past the object, NULL (trial %ld)",
			    guard_equal(), 1, trial);
		diff_eq_int("resetDILGenerator(NULL) cleared dilCount "
			    "(trial %ld)", ours.o.dilCount, 0, trial);
	}

	diff_eq_int("resetDILGenerator changed the object", moved, 1, 0);
	diff_eq_int("resetDILGenerator is not the same on every trial",
		    distinct, 1, 0);

	/*
	 * Anti-vacuity for the segment search specifically: the sweep
	 * must actually have reached the last row entry, the index one
	 * past the end, and a level the object reads back above 0x7fff.
	 * Without these three the two mutations "row bound 7 -> 6" and
	 * "read the level signed" both survive.
	 */
	diff_eq_int("the sweep reached segment index 7", saw_index7, 1, 0);
	diff_eq_int("the sweep reached segment index 8", saw_index8, 1, 0);
	diff_eq_int("the sweep produced a negative level", saw_negative,
		    1, 0);

	return diff_end();
}

/*
 * The static table.  It is a defined data symbol in the blob and therefore
 * renamed, so both copies exist and can be compared element by element -- the
 * one thing in this file that is a comparison of data rather than behaviour,
 * and the reason the table above is an extraction rather than a generator.
 */
extern "C" int ref_codeSegmentsBoundries[2][8]
	asm("ref__ZN18V90Phase3Modulator32codeSegmentsBoundriesLookupTableE");

static int
run_table(void)
{
	int law, seg;

	diff_begin("V90Phase3Modulator::codeSegmentsBoundriesLookupTable");

	for (law = 0; law < 2; law++)
		for (seg = 0; seg < 8; seg++)
			diff_eq_int("table[%ld]",
			    V90Phase3Modulator::
				codeSegmentsBoundriesLookupTable[law][seg],
			    ref_codeSegmentsBoundries[law][seg],
			    law * 8 + seg);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_table();
	rc |= run_setsessionflag();
	rc |= run_resetdilgenerator();

	return rc;
}
