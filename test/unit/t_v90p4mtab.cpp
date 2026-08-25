/*
 * t_v90p4mtab.cpp -- differential test of the thirteen V90Phase4Modulator
 * members that neither print nor call a peer's method:
 *
 *     setRfSymbols(V90MappingParams *)          1,005 bytes
 *     setRdRtSymbols(V90MappingParams *)          512
 *     generateRiNot() / generateRi()              100 / 88
 *     resetBeforRRN() / resetRRNSecondSection()    63 / 53
 *     generateRfNot() / generateRf()               42 / 39
 *     generateRdRtNot() / generateRdRt()           41 / 38
 *     recivedCP()                                  23
 *     setNextStateAfterTRN2d(Phase4ModulatorState) 12
 *     recivedPartOneSilenceRrnSUV()                12
 *
 * `tools/closure.py --missing` says this set's closure is exactly itself, so
 * nothing here reaches a member the branch has not written; the only external
 * calls are `alaw2linear` and `ulaw2linear`, which are C and shared.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS COMPARED
 *
 * Three things on every trial, and the third is the one that matters for the
 * six readers:
 *
 *   - the 12,204-byte modulator, whole, plus a 64-byte guard past it;
 *   - the `V90CP` each side owns, whole, because four of the thirteen write
 *     its +0x13 and nothing else here touches it;
 *   - the return value, as a `short`.
 *
 * THE RETURN IS COMPARED AS A `short` AND NOT AS AN `int`, DELIBERATELY.  The
 * six readers end in `neg %eax ; cwtl` or a bare `movswl`, so the value in
 * %ax is defined and the upper half of %eax is not something the i386 C++ ABI
 * makes a caller entitled to.  A 32-bit assertion would be comparing bits the
 * object does not promise.  What the upper half IS good for is the codegen
 * tier, and that is where the `int` intermediate in the four `*Not` readers
 * was settled -- see the source's comment; it is 613's case, invisible to any
 * differential test and visible to `compare.py`.
 *
 * ---------------------------------------------------------------------------
 * WHICH PEERS ARE SHARED AND WHICH ARE SPLIT
 *
 * SPLIT, one per side: the `V90CP`.  `recivedCP`, `recivedPartOneSilenceRrnSUV`
 * and `resetRRNSecondSection` write `cp->byte_13`, so a shared record would
 * let one side's store satisfy the other's and the comparison would pass on a
 * function that did nothing.  The modulator's own `cp` word at +0x54
 * therefore holds two different addresses and is blanked in a scratch copy
 * before the object comparison, exactly as t_v90p4ddec does for its four.
 *
 * SHARED: the `V90MappingParams` block both symbol setters read.  Nothing
 * under test writes through it, so one address keeps `mappingParams`,
 * `mappingParams2` and the argument IN the comparison rather than blanked out
 * of it -- finding F1105.
 *
 * THE OBJECTS ARE NEVER ZEROED -- finding F230.  Every slot gets varied
 * pseudorandom bytes before every trial, so a store that fails to happen is
 * visible and a store of zero into memory that was already zero cannot be
 * mistaken for one.  The fields each arm reads are planted on top of that,
 * identically on both sides.
 *
 * ---------------------------------------------------------------------------
 * ANTI-VACUITY, per finding F3509
 *
 * Every counter here names an OBSERVABLE difference -- a returned symbol, a
 * byte of the modulator, a byte of the CP record -- and never "a branch
 * believed to have been taken".  Specifically:
 *
 *   - `sym_varied` counts trials whose RETURNED symbol differs from the first
 *     trial's.  A reader wired to a constant scores zero and the run fails.
 *   - `tab_varied` counts trials whose WRITTEN table differs from the
 *     previous trial's, so a setter that ignored its argument or its law
 *     scores zero.
 *   - `law_split` is blob against blob: the same mapping block through
 *     `pcmType` 0 and 1 must produce DIFFERENT tables.  That is what says the
 *     companding branch is read from +0x38 and not folded to one law, and no
 *     comparison of two identically-seeded runs can hold it.
 *   - `cp_split` is also blob against blob: `recivedCP` must move a byte of
 *     the CP record that `setNextStateAfterTRN2d` does not.
 *   - `obj_moved` counts trials in which the modulator changed at all.
 *
 * The mutations in `test/mutations/v90p4mtab.json` are what adjudicate; the
 * counters only stop a green run that measured nothing.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/Scrambler.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90Phase4Modulator.h"

extern "C" {
short ref_p4m_generateRdRt(void *)
	asm("ref__ZN18V90Phase4Modulator12generateRdRtEv");
short ref_p4m_generateRdRtNot(void *)
	asm("ref__ZN18V90Phase4Modulator15generateRdRtNotEv");
short ref_p4m_generateRf(void *)
	asm("ref__ZN18V90Phase4Modulator10generateRfEv");
short ref_p4m_generateRfNot(void *)
	asm("ref__ZN18V90Phase4Modulator13generateRfNotEv");
short ref_p4m_generateRi(void *)
	asm("ref__ZN18V90Phase4Modulator10generateRiEv");
short ref_p4m_generateRiNot(void *)
	asm("ref__ZN18V90Phase4Modulator13generateRiNotEv");
void ref_p4m_setRdRtSymbols(void *, void *)
	asm("ref__ZN18V90Phase4Modulator14setRdRtSymbolsEP16V90MappingParams");
void ref_p4m_setRfSymbols(void *, void *)
	asm("ref__ZN18V90Phase4Modulator12setRfSymbolsEP16V90MappingParams");
void ref_p4m_setNextStateAfterTRN2d(void *, int)
	asm("ref__ZN18V90Phase4Modulator22setNextStateAfterTRN2dE20Phase4Modul"
	    "atorState");
void ref_p4m_resetBeforRRN(void *)
	asm("ref__ZN18V90Phase4Modulator13resetBeforRRNEv");
void ref_p4m_resetRRNSecondSection(void *)
	asm("ref__ZN18V90Phase4Modulator21resetRRNSecondSectionEv");
void ref_p4m_recivedCP(void *)
	asm("ref__ZN18V90Phase4Modulator9recivedCPEv");
void ref_p4m_recivedPartOneSilenceRrnSUV(void *)
	asm("ref__ZN18V90Phase4Modulator27recivedPartOneSilenceRrnSUVEv");
}

/* ----------------------------------------------------------- the storage */

#define P4M_SLOT	((unsigned)sizeof(V90Phase4Modulator) + 64u)
#define CP_SLOT		((unsigned)sizeof(V90CP) + 64u)

static unsigned char p4m_s[2][P4M_SLOT] __attribute__((aligned(8)));
static unsigned char p4m_seed[P4M_SLOT];
static unsigned char p4m_cmp[2][P4M_SLOT];

static unsigned char cp_s[2][CP_SLOT] __attribute__((aligned(8)));

/* Shared: nothing under test writes through it. */
static unsigned char mapp_s[sizeof(V90MappingParams) + 64]
	__attribute__((aligned(8)));

#define P4M(s)		(*(V90Phase4Modulator *)p4m_s[s])
#define CPR(s)		(*(V90CP *)cp_s[s])
#define MAPP		((V90MappingParams *)mapp_s)

/* Varied, never zero, never the same twice: findings F223, F224, F230. */
static unsigned
fill(unsigned char *p, unsigned n, unsigned lfsr)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
		p[i] = (unsigned char)((lfsr >> 3) | 1u);
	}
	return lfsr;
}

/*
 * The one pointer that legitimately differs between the sides.  Everything
 * else in the modulator either is identical or is what we are measuring.
 */
static const unsigned p4m_skip[] = { 0x54u, ~0u };

static void
scrub(unsigned char *dst, const unsigned char *src, unsigned n,
      const unsigned *skip)
{
	int i;

	memcpy(dst, src, n);
	for (i = 0; skip[i] != ~0u; i++)
		memset(dst + skip[i], 0, 4);
}

/*
 * Plant one modulator and one CP record per side.  Nothing here runs a
 * constructor: `V90Phase4Modulator`'s is the ctor batch's business and
 * running it would allocate a converter and hide the seed the comparison
 * rests on.
 */
static void
setup(int trial, int law, int code)
{
	unsigned lf = 0x51a3u + 0x9e37u * (unsigned)trial + 0x4d1bu *
		      (unsigned)law + 0x2f11u * (unsigned)code;
	int s, k, j;

	fill(mapp_s, (unsigned)sizeof mapp_s, lf ^ 0x6c31u);

	/*
	 * THE FIRST BYTE OF EACH CONSTELLATION IS WHAT THE TWO SETTERS READ,
	 * and it is planted rather than left to the seed so that the six rows
	 * are distinguishable from one another: a setter that read row 0 six
	 * times would otherwise pass whenever the seed happened to repeat.
	 * The rest of each row stays pseudorandom, which is what says nothing
	 * here reads past byte 0.
	 */
	for (k = 0; k < V90_CONSTELLATIONS; k++)
		MAPP->constellation[k][0] =
			(unsigned char)(0x11u + 0x1du * (unsigned)k +
					0x07u * (unsigned)code);

	fill(p4m_s[0], P4M_SLOT, lf);
	memcpy(p4m_s[1], p4m_s[0], P4M_SLOT);
	memcpy(p4m_seed, p4m_s[0], P4M_SLOT);

	for (s = 0; s < 2; s++) {
		V90Phase4Modulator *m = &P4M(s);

		fill(cp_s[s], CP_SLOT, lf ^ 0x3bu);
		m->cp = &CPR(s);
		m->mappingParams = MAPP;
		m->mappingParams2 = MAPP;
		m->pcmType = law ? PCM_TYPE_A_LAW : PCM_TYPE_MU_LAW;
		m->codeLevel = (short)(0x1000 - 0x137 * code);

		/*
		 * The two tables, seeded so that every element is distinct and
		 * none is zero: an index bug that reached the wrong slot has
		 * to change the answer.
		 */
		for (j = 0; j < V90P4M_RDRT_SYMBOLS; j++)
			m->rdRtSymbols[j] = (short)(0x2000 - 0x101 * j);
		for (j = 0; j < V90P4M_RF_SYMBOLS; j++)
			m->rfSymbols[j] = (short)(-0x1800 + 0x91 * j);
	}
}

static void
compare_all(const char *what, long tag)
{
	unsigned n = (unsigned)sizeof(V90Phase4Modulator);
	int s;

	scrub(p4m_cmp[0], p4m_s[0], P4M_SLOT, p4m_skip);
	scrub(p4m_cmp[1], p4m_s[1], P4M_SLOT, p4m_skip);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase4Modulator",
		     p4m_cmp[0], p4m_cmp[1], n, tag);
	diff_eq_int("nothing stored past the modulator (%ld)",
		    memcmp(p4m_cmp[0] + n, p4m_cmp[1] + n, P4M_SLOT - n) == 0,
		    1, tag);
	for (s = 0; s < 2; s++)
		diff_eq_int("the guard past the modulator held (%ld)",
			    memcmp(p4m_s[s] + n, p4m_seed + n,
				   P4M_SLOT - n) == 0, 1, tag);

	diff_eq_int("the CP record (%ld)",
		    memcmp(cp_s[0], cp_s[1], CP_SLOT) == 0, 1, tag);
}

/* ------------------------------------------------------- the six readers */

typedef short (*reader)(void *);

struct arm {
	const char *name;
	reader ours;
	reader theirs;
};

/*
 * `our_*` are taken through the class rather than through an `asm` label:
 * these are the only members in this file with a return value, and calling
 * them by name is what makes the declared return type -- `short` -- the one
 * the compiler applies to both sides.
 */
static short our_rdrt(void *p)	 { return ((V90Phase4Modulator *)p)->generateRdRt(); }
static short our_rdrtn(void *p)	 { return ((V90Phase4Modulator *)p)->generateRdRtNot(); }
static short our_rf(void *p)	 { return ((V90Phase4Modulator *)p)->generateRf(); }
static short our_rfn(void *p)	 { return ((V90Phase4Modulator *)p)->generateRfNot(); }
static short our_ri(void *p)	 { return ((V90Phase4Modulator *)p)->generateRi(); }
static short our_rin(void *p)	 { return ((V90Phase4Modulator *)p)->generateRiNot(); }

static const struct arm readers[] = {
	{ "generateRdRt",	our_rdrt,  ref_p4m_generateRdRt },
	{ "generateRdRtNot",	our_rdrtn, ref_p4m_generateRdRtNot },
	{ "generateRf",		our_rf,	   ref_p4m_generateRf },
	{ "generateRfNot",	our_rfn,   ref_p4m_generateRfNot },
	{ "generateRi",		our_ri,	   ref_p4m_generateRi },
	{ "generateRiNot",	our_rin,   ref_p4m_generateRiNot }
};

#define NREADERS	((int)(sizeof readers / sizeof readers[0]))

/*
 * THE COUNT IS SWEPT PAST BOTH PERIODS AND PAST ZERO.  `(symbolCount - 1)` is
 * computed unsigned, so a count of 0 indexes 0xffffffff % 6 == 3 and
 * 0xffffffff % 12 == 3 -- both inside their tables, so no input in this grid
 * reaches an out-of-bounds index in OUR code and none had to be excluded.
 */
static const unsigned counts[] = {
	0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u,
	24u, 25u, 0xfffffffbu, 0xffffffffu
};

#define NCOUNTS		((int)(sizeof counts / sizeof counts[0]))

static int
run_readers(void)
{
	long trial = 810000L;
	int a, c, law;

	diff_begin("V90Phase4Modulator symbol readers");

	for (a = 0; a < NREADERS; a++) {
		short first = 0;
		int sym_varied = 0;
		int seen = 0;

		for (law = 0; law < 2; law++)
			for (c = 0; c < NCOUNTS; c++) {
				short rc[2];
				int s;

				setup((int)trial, law, c);
				for (s = 0; s < 2; s++)
					P4M(s).symbolCount = counts[c];

				rc[0] = readers[a].ours(p4m_s[0]);
				rc[1] = readers[a].theirs(p4m_s[1]);

				compare_all(readers[a].name, trial);
				diff_eq_int("the symbol (%ld)", (long)rc[0],
					    (long)rc[1], trial);

				if (!seen) {
					first = rc[0];
					seen = 1;
				} else if (rc[0] != first) {
					sym_varied++;
				}
				trial++;
			}

		diff_eq_int("the reader returned more than one value",
			    sym_varied > 0, 1, (long)a);
	}

	return diff_end();
}

/* ------------------------------------------------------- the two setters */

static int
run_setters(void)
{
	long trial = 820000L;
	int law, code;
	int rd_varied = 0, rf_varied = 0;
	short rd_prev[V90P4M_RDRT_SYMBOLS];
	short rf_prev[V90P4M_RF_SYMBOLS];
	int seen = 0;

	diff_begin("V90Phase4Modulator::setRdRtSymbols / setRfSymbols");

	memset(rd_prev, 0, sizeof rd_prev);
	memset(rf_prev, 0, sizeof rf_prev);

	for (law = 0; law < 2; law++)
		for (code = 0; code < 6; code++) {
			setup((int)trial, law, code);
			P4M(0).setRdRtSymbols(MAPP);
			ref_p4m_setRdRtSymbols(p4m_s[1], mapp_s);
			compare_all("setRdRtSymbols", trial);

			if (seen && memcmp(rd_prev, P4M(0).rdRtSymbols,
					   sizeof rd_prev) != 0)
				rd_varied++;
			memcpy(rd_prev, P4M(0).rdRtSymbols, sizeof rd_prev);
			trial++;

			setup((int)trial, law, code);
			P4M(0).setRfSymbols(MAPP);
			ref_p4m_setRfSymbols(p4m_s[1], mapp_s);
			compare_all("setRfSymbols", trial);

			if (seen && memcmp(rf_prev, P4M(0).rfSymbols,
					   sizeof rf_prev) != 0)
				rf_varied++;
			memcpy(rf_prev, P4M(0).rfSymbols, sizeof rf_prev);
			seen = 1;
			trial++;
		}

	diff_eq_int("setRdRtSymbols wrote more than one table (%ld)",
		    rd_varied > 0, 1, trial);
	diff_eq_int("setRfSymbols wrote more than one table (%ld)",
		    rf_varied > 0, 1, trial);

	/*
	 * BLOB AGAINST BLOB: +0x38 selects the companding law.  Same mapping
	 * block, same seed, the two settings of `pcmType`, and the two tables
	 * must DIFFER.  A `setRdRtSymbols` that always took one law would
	 * pass every comparison above and fail this.
	 */
	{
		short after_ulaw[V90P4M_RDRT_SYMBOLS];
		short after_ulaw_rf[V90P4M_RF_SYMBOLS];

		setup((int)trial, 0, 2);
		ref_p4m_setRdRtSymbols(p4m_s[1], mapp_s);
		ref_p4m_setRfSymbols(p4m_s[1], mapp_s);
		memcpy(after_ulaw, P4M(1).rdRtSymbols, sizeof after_ulaw);
		memcpy(after_ulaw_rf, P4M(1).rfSymbols, sizeof after_ulaw_rf);

		setup((int)trial, 0, 2);
		P4M(1).pcmType = PCM_TYPE_A_LAW;
		ref_p4m_setRdRtSymbols(p4m_s[1], mapp_s);
		ref_p4m_setRfSymbols(p4m_s[1], mapp_s);

		diff_eq_int("pcmType selects the law, Rd/Rt (%ld)",
			    memcmp(after_ulaw, P4M(1).rdRtSymbols,
				   sizeof after_ulaw) != 0, 1, trial);
		diff_eq_int("pcmType selects the law, Rf (%ld)",
			    memcmp(after_ulaw_rf, P4M(1).rfSymbols,
				   sizeof after_ulaw_rf) != 0, 1, trial);
		trial++;
	}

	return diff_end();
}

/* -------------------------------------------------- the five that store */

static int
run_stores(void)
{
	long trial = 830000L;
	int obj_moved = 0;
	int k;

	diff_begin("V90Phase4Modulator resets and tags");

	for (k = 0; k < 8; k++) {
		unsigned char before[P4M_SLOT];

		setup((int)trial, k & 1, k);
		memcpy(before, p4m_s[0], P4M_SLOT);
		P4M(0).resetBeforRRN();
		ref_p4m_resetBeforRRN(p4m_s[1]);
		compare_all("resetBeforRRN", trial);
		if (memcmp(before, p4m_s[0], P4M_SLOT) != 0)
			obj_moved++;
		trial++;

		setup((int)trial, k & 1, k);
		memcpy(before, p4m_s[0], P4M_SLOT);
		P4M(0).resetRRNSecondSection();
		ref_p4m_resetRRNSecondSection(p4m_s[1]);
		compare_all("resetRRNSecondSection", trial);
		if (memcmp(before, p4m_s[0], P4M_SLOT) != 0)
			obj_moved++;
		trial++;

		setup((int)trial, k & 1, k);
		memcpy(before, p4m_s[0], P4M_SLOT);
		P4M(0).recivedCP();
		ref_p4m_recivedCP(p4m_s[1]);
		compare_all("recivedCP", trial);
		if (memcmp(before, p4m_s[0], P4M_SLOT) != 0)
			obj_moved++;
		trial++;

		setup((int)trial, k & 1, k);
		P4M(0).recivedPartOneSilenceRrnSUV();
		ref_p4m_recivedPartOneSilenceRrnSUV(p4m_s[1]);
		compare_all("recivedPartOneSilenceRrnSUV", trial);
		trial++;

		/*
		 * Every state the class ever stores, plus one it never does,
		 * so the argument reaches +0x10 unmodified whatever its value.
		 */
		setup((int)trial, k & 1, k);
		P4M(0).setNextStateAfterTRN2d(
			(Phase4ModulatorState)(k * 5 - 3));
		ref_p4m_setNextStateAfterTRN2d(p4m_s[1], k * 5 - 3);
		compare_all("setNextStateAfterTRN2d", trial);
		diff_eq_int("the state landed at +0x10 (%ld)",
			    (long)P4M(0).nextStateAfterTRN2d,
			    (long)(k * 5 - 3), trial);
		trial++;
	}

	diff_eq_int("the resets moved the modulator (%ld)", obj_moved > 0, 1,
		    trial);

	/*
	 * BLOB AGAINST BLOB: `recivedCP` writes the CP record and
	 * `setNextStateAfterTRN2d` does not.  Without this a `recivedCP` that
	 * only set the modulator's own latch would pass every comparison
	 * above, because both sides would be equally wrong.
	 */
	{
		unsigned char cp_before[CP_SLOT];

		setup((int)trial, 0, 3);
		memcpy(cp_before, cp_s[1], CP_SLOT);
		ref_p4m_setNextStateAfterTRN2d(p4m_s[1], 4);
		diff_eq_int("setNextStateAfterTRN2d leaves the CP alone (%ld)",
			    memcmp(cp_before, cp_s[1], CP_SLOT) == 0, 1,
			    trial);
		ref_p4m_recivedCP(p4m_s[1]);
		diff_eq_int("recivedCP writes the CP record (%ld)",
			    memcmp(cp_before, cp_s[1], CP_SLOT) != 0, 1,
			    trial);
		trial++;
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_readers();
	rc |= run_setters();
	rc |= run_stores();

	return rc;
}
