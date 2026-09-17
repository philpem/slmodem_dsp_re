/*
 * t_v90p4mgen.cpp -- differential test of the fifteen V90Phase4Modulator
 * members that print, call a peer, or both:
 *
 *     recivedCPtag()                        282 bytes
 *     recivedFirstSUVuPartTwoRrn()          266
 *     recivedPartTwoSilenceRrnSUV()         200
 *     recivedSUV()                          179
 *     recivedSUVtag()                       164
 *     exitMP() / recivedE2u()               142 each
 *     enterRepeatedCPd()                    140
 *     recivedPartOneSilenceRrnSUVtag()      111
 *     exitRi() / exitSilence()              104 each
 *     exitMPNot()                           102
 *     recivedFirstRrnE2u()                   98
 *     generateDataSymbolBeforeRRN() / ...FPE() 96 each
 *
 * `tools/closure.py --missing` says the set's closure is exactly itself.  The
 * peers it reaches are `V90CP::infoToBits`, `V90CP::getBitVector`,
 * `V90MP::getBitVector`, `V90BitsToSymbol::process`, `edprintf` and
 * `dsplibs_debug_printf`, all of them already written and already tested.
 *
 * ---------------------------------------------------------------------------
 * WHAT IS COMPARED
 *
 *   - the 12,204-byte modulator, whole, plus a 64-byte guard past it;
 *   - the `V90CP` each side owns, whole, buffers included;
 *   - the `V90BitsToSymbol` each side owns, and its symbol array;
 *   - the diagnostic transcript, as text, at levels 0 and 2;
 *   - the returned symbol, as a `short`, for the two data pumps.
 *
 * ---------------------------------------------------------------------------
 * WHICH PEERS ARE SHARED AND WHICH ARE SPLIT
 *
 * SPLIT, one per side, because they are written: the `V90CP` --
 * `enterRepeatedCPd` and the three SUV members run `infoToBits` through it --
 * and the `V90BitsToSymbol`, whose `process` moves `symbolsDone` and clears
 * `extraSymbolsPending`.  Their two pointer words in the modulator, +0x54 and
 * +0x44, and `cpBits` at +0x2f8c -- which points INTO the split CP -- are
 * blanked in a scratch copy before the object comparison, and the two objects
 * are then compared themselves.
 *
 * SHARED: the `V90MP`.  `V90MP::getBitVector` is `length = seqLength; return
 * bits;` and writes nothing, so one record keeps `mp` at +0x48 and `mpBits`
 * at +0x2f58 IN the comparison rather than blanked out of it -- and +0x2f58
 * is exactly where `exitMP`'s work lands.  Finding F1105 the useful way round.
 *
 * THE OBJECTS ARE NEVER ZEROED -- finding F230.
 *
 * ---------------------------------------------------------------------------
 * THE TWO DATA PUMPS' GRID EXCLUDES ONE CONFIGURATION, AND SAYS SO
 *
 * Both pumps pass an UNINITIALISED `unsigned int` to
 * `V90BitsToSymbol::process` by reference and read it back, and `process`
 * leaves it unwritten when `symbolsBlockSize` is zero -- so that one setting
 * makes the reconstruction read an indeterminate value and the trial would not
 * be a differential trial at all.  It is out of the grid, deliberately, and
 * deviation D661 is the entry.  The two settings that ARE swept both write it
 * and both write the symbol, so the return value is asserted on every trial:
 *
 *     symbolsBlockSize 1, symbolsDone 2   status 0, demand 0   -> no message
 *     symbolsBlockSize 2, symbolsDone 1   status 3, demand > 0 -> message
 *
 * `bitsPerFrame` is non-zero in the fixture or the second collapses into the
 * first with nothing to show for it.  Nothing bigger is swept: `process`
 * writes `min(blockSize, symbolsDone)` symbols into a ONE-SHORT stack slot in
 * the caller, so (2, 1) is the largest configuration that fits and a "(2, 2)
 * for coverage" would smash the caller's frame.
 *
 * ---------------------------------------------------------------------------
 * ANTI-VACUITY, per finding F3509
 *
 * Every counter names an OBSERVABLE difference and never a branch believed
 * taken.  `printed` counts trials whose transcript is non-empty, `moved`
 * counts trials in which the modulator changed at all, and `states` counts
 * how many distinct values of +0x04 the sweep left behind.  A run in which
 * every guard rejected and nothing was ever printed passes every `diff_eq`
 * and measures nothing; these stop it.
 *
 * ONE CLAIM IS BLOB AGAINST BLOB, because no identically-seeded comparison
 * can hold it: `recivedCPtag` acts when +0x0020 is NON-zero and `recivedE2u`
 * acts when it is ZERO.  Same seed, same field, and the two must move the
 * object on opposite settings.  That is what would catch one guard
 * copy-pasted into the other, which is the likeliest way to get this wrong
 * and the one thing the fifteen-way sweep cannot see.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/Scrambler.h"
#include "dsplib/V90BitsToSymbol.h"
#include "dsplib/V90CP.h"
#include "dsplib/V90MP.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Phase4Modulator.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void ref_p4m_exitRi(void *)	asm("ref__ZN18V90Phase4Modulator6exitRiEv");
void ref_p4m_exitSilence(void *) asm("ref__ZN18V90Phase4Modulator11exitSilenceEv");
void ref_p4m_exitMP(void *)	asm("ref__ZN18V90Phase4Modulator6exitMPEv");
void ref_p4m_exitMPNot(void *)	asm("ref__ZN18V90Phase4Modulator9exitMPNotEv");
void ref_p4m_enterRepeatedCPd(void *)
	asm("ref__ZN18V90Phase4Modulator16enterRepeatedCPdEv");
void ref_p4m_recivedSUV(void *)	asm("ref__ZN18V90Phase4Modulator10recivedSUVEv");
void ref_p4m_recivedPartTwoSilenceRrnSUV(void *)
	asm("ref__ZN18V90Phase4Modulator27recivedPartTwoSilenceRrnSUVEv");
void ref_p4m_recivedFirstSUVuPartTwoRrn(void *)
	asm("ref__ZN18V90Phase4Modulator26recivedFirstSUVuPartTwoRrnEv");
void ref_p4m_recivedCPtag(void *)
	asm("ref__ZN18V90Phase4Modulator12recivedCPtagEv");
void ref_p4m_recivedSUVtag(void *)
	asm("ref__ZN18V90Phase4Modulator13recivedSUVtagEv");
void ref_p4m_recivedE2u(void *)	asm("ref__ZN18V90Phase4Modulator10recivedE2uEv");
void ref_p4m_recivedFirstRrnE2u(void *)
	asm("ref__ZN18V90Phase4Modulator18recivedFirstRrnE2uEv");
void ref_p4m_recivedPartOneSilenceRrnSUVtag(void *)
	asm("ref__ZN18V90Phase4Modulator30recivedPartOneSilenceRrnSUVtagEv");
void ref_p4m_recivedPartTwoSilenceRrnSUVtag(void *)
	asm("ref__ZN18V90Phase4Modulator30recivedPartTwoSilenceRrnSUVtagEv");
short ref_p4m_generateDataSymbolBeforeFPE(void *)
	asm("ref__ZN18V90Phase4Modulator27generateDataSymbolBeforeFPEEv");
short ref_p4m_generateDataSymbolBeforeRRN(void *)
	asm("ref__ZN18V90Phase4Modulator27generateDataSymbolBeforeRRNEv");
}

/* ----------------------------------------------------------- the storage */

#define P4M_SLOT	((unsigned)sizeof(V90Phase4Modulator) + 64u)
#define CP_SLOT		((unsigned)sizeof(V90CP) + 64u)
#define MP_SLOT		((unsigned)sizeof(V90MP) + 64u)
#define BTS_SLOT	((unsigned)sizeof(V90BitsToSymbol) + 64u)
#define NSYM		8

static unsigned char p4m_s[2][P4M_SLOT] __attribute__((aligned(8)));
static unsigned char p4m_seed[P4M_SLOT];
static unsigned char p4m_cmp[2][P4M_SLOT];

static unsigned char cp_s[2][CP_SLOT] __attribute__((aligned(8)));
static int cpbuf_s[2][V90CP_BUFS][V90CP_BUFENTS];

static unsigned char bts_s[2][BTS_SLOT] __attribute__((aligned(8)));
static unsigned char bts_cmp[2][BTS_SLOT];
static short btssym_s[2][NSYM];

/* Shared: `V90MP::getBitVector` writes nothing. */
static unsigned char mp_s[MP_SLOT] __attribute__((aligned(8)));
static unsigned char mapp_s[sizeof(V90MappingParams) + 64]
	__attribute__((aligned(8)));

#define P4M(s)		(*(V90Phase4Modulator *)p4m_s[s])
#define CPR(s)		(*(V90CP *)cp_s[s])
#define BTS(s)		(*(V90BitsToSymbol *)bts_s[s])
#define MPR		((V90MP *)mp_s)
#define MAPP		((V90MappingParams *)mapp_s)

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

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/*
 * `cp` and `bitsToSymbol` are one object per side and `cpBits` points into
 * the first of them, so all three hold two different addresses for ever.
 * `mp` and `mpBits` are NOT here: the MP record is shared.
 */
static const unsigned p4m_skip[] = { 0x44u, 0x54u, 0x2f8cu, ~0u };

/* The converter's three pointer words, likewise. */
static const unsigned bts_skip[] = { 0x00u, 0x04u, 0x08u, ~0u };

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
 * Plant one modulator, one CP record and one converter per side, and one
 * shared MP record.  `mode` selects the converter's two swept configurations
 * and moves the seed; `latch` is the three flag words the guards read.
 */
static void
setup(int trial, int mode, int latch, unsigned int count)
{
	unsigned lf = 0x2c19u + 0x9e37u * (unsigned)trial + 0x51edu *
		      (unsigned)mode + 0x1b7du * (unsigned)latch;
	int s, k;

	fill(mapp_s, (unsigned)sizeof mapp_s, lf ^ 0x6c31u);
	fill(mp_s, MP_SLOT, lf ^ 0x4411u);

	/*
	 * The MP record's two fields this batch reads: the length
	 * `getBitVector` reports and the group size `exitMP` divides by.  A
	 * pseudorandom divisor is legal but a pseudorandom ZERO is a trap, and
	 * the byte at +0x118 has to stay small enough that `6 * length` does
	 * not wrap.
	 */
	MPR->seqLength = 0x24;
	MPR->groupSize = 3u;

	fill(p4m_s[0], P4M_SLOT, lf);
	memcpy(p4m_s[1], p4m_s[0], P4M_SLOT);
	memcpy(p4m_seed, p4m_s[0], P4M_SLOT);

	for (s = 0; s < 2; s++) {
		V90Phase4Modulator *m = &P4M(s);
		V90CP *c = &CPR(s);
		V90BitsToSymbol *b = &BTS(s);

		fill(cp_s[s], CP_SLOT, lf ^ 0x3bu);
		fill(bts_s[s], BTS_SLOT, lf ^ 0x77u);
		fill((unsigned char *)cpbuf_s[s], (unsigned)sizeof cpbuf_s[s],
		     lf ^ 0x2du);
		fill((unsigned char *)btssym_s[s], (unsigned)sizeof btssym_s[s],
		     lf ^ 0x5eu);

		m->cp = c;
		m->mp = MPR;
		m->bitsToSymbol = b;
		m->mappingParams = MAPP;
		m->mappingParams2 = MAPP;

		/*
		 * THE DIVISORS.  Five members take `symbolCount %
		 * cpSequenceSymbols` and two take it modulo
		 * `mpSequenceSymbols`; a zero divisor is a SIGFPE and not a
		 * failing check, so both are planted and both are small enough
		 * that `count` can be on and off a boundary.
		 */
		m->cpSequenceSymbols = 4u;
		m->mpSequenceSymbols = 3u;
		m->symbolCount = count;
		m->word_0020 = (latch & 1) ? 1u : 0u;
		m->word_2f9c = (latch & 2) ? 1u : 0u;
		m->word_2fa0 = (latch & 4) ? 1u : 0u;

		/*
		 * The CP record, planted the way t_v90cpinfo plants it:
		 * `infoToBits` walks four counted lists and six buffers, and a
		 * pseudorandom count there is a loop that writes off the end
		 * of the bit vector.
		 */
		c->word_00 = 0;
		c->word_04 = (trial & 1);
		c->word_08 = (trial & 2) >> 1;
		c->word_0c = (trial & 4) >> 2;
		c->byte_10 = (signed char)(trial * 7);
		c->byte_11 = (unsigned char)(trial % 4);
		c->byte_12 = (unsigned char)(trial & 1);
		c->word_14 = (int)(0x1234u * (unsigned)(trial + 1));
		for (k = 0; k < 12; k++)
			c->word_18[k] =
			    (int)(0x33u * (unsigned)(trial + k) - 0x1000);
		for (k = 0; k < 4; k++) {
			int j;

			c->nof_58[k] = (unsigned int)((trial * (k + 3)) % 13);
			for (j = 0; j < V90CP_SHORTS; j++)
				c->short_58[k][j] = (short)
				    (0x5bu * (unsigned)(trial + k * 7 + j));
		}
		for (k = 0; k < V90CP_BUFS; k++) {
			c->buf[k] = cpbuf_s[s][k];
			c->nof_buf[k] = (unsigned int)((trial * (k + 2)) % 13);
			c->word_c70[k] = (int)(0x17u * (unsigned)(trial + k));
		}
		c->word_ca0 = (unsigned int)(trial * 3);
		c->groupSize = 17u;
		c->seqLength = 0x30u;

		/*
		 * The converter.  Two configurations and no third: see the
		 * header comment for why `symbolsBlockSize == 0` is not one of
		 * them and why nothing larger than (2, 1) can be.
		 */
		b->symbols = btssym_s[s];
		b->nofSymbols = NSYM;
		b->bitsPerFrame = 8u;
		b->extraSymbols = 5u;
		b->extraSymbolsPending = (unsigned char)((mode >> 1) & 1);
		if (mode & 1) {
			b->symbolsBlockSize = 2u;
			b->symbolsDone = 1u;
		} else {
			b->symbolsBlockSize = 1u;
			b->symbolsDone = 2u;
		}
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

	/* The CP record, and its six buffers, which live outside it. */
	diff_eq_int("the CP record (%ld)",
		    memcmp((unsigned char *)&CPR(0) +
			   __builtin_offsetof(V90CP, word_00),
			   (unsigned char *)&CPR(1) +
			   __builtin_offsetof(V90CP, word_00),
			   __builtin_offsetof(V90CP, buf) -
			   __builtin_offsetof(V90CP, word_00)) == 0, 1, tag);
	diff_eq_int("the CP record past its buffer pointers (%ld)",
		    memcmp((unsigned char *)&CPR(0) +
			   __builtin_offsetof(V90CP, word_ca0),
			   (unsigned char *)&CPR(1) +
			   __builtin_offsetof(V90CP, word_ca0),
			   CP_SLOT - __builtin_offsetof(V90CP, word_ca0)) == 0,
		    1, tag);
	diff_eq_int("the CP record's buffers (%ld)",
		    memcmp(cpbuf_s[0], cpbuf_s[1], sizeof cpbuf_s[0]) == 0, 1,
		    tag);

	/* The converter, its three pointer words blanked, and its symbols. */
	scrub(bts_cmp[0], bts_s[0], BTS_SLOT, bts_skip);
	scrub(bts_cmp[1], bts_s[1], BTS_SLOT, bts_skip);
	diff_eq_obj_(__FILE__, __LINE__, "the bits-to-symbol converter",
		     "V90BitsToSymbol", bts_cmp[0], bts_cmp[1], BTS_SLOT, tag);
	diff_eq_int("the converter's symbols (%ld)",
		    memcmp(btssym_s[0], btssym_s[1], sizeof btssym_s[0]) == 0,
		    1, tag);

	diff_eq_int("the transcripts agreed (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
}

/* ------------------------------------------------------------- the sweep */

typedef void (*edge)(void *);

struct arm {
	const char *name;
	edge ours;
	edge theirs;
};

static void our_exitRi(void *p)	{ ((V90Phase4Modulator *)p)->exitRi(); }
static void our_exitSilence(void *p) { ((V90Phase4Modulator *)p)->exitSilence(); }
static void our_exitMP(void *p)	{ ((V90Phase4Modulator *)p)->exitMP(); }
static void our_exitMPNot(void *p) { ((V90Phase4Modulator *)p)->exitMPNot(); }
static void our_enterRepeatedCPd(void *p)
	{ ((V90Phase4Modulator *)p)->enterRepeatedCPd(); }
static void our_recivedSUV(void *p) { ((V90Phase4Modulator *)p)->recivedSUV(); }
static void our_recivedPartTwoSilenceRrnSUV(void *p)
	{ ((V90Phase4Modulator *)p)->recivedPartTwoSilenceRrnSUV(); }
static void our_recivedFirstSUVuPartTwoRrn(void *p)
	{ ((V90Phase4Modulator *)p)->recivedFirstSUVuPartTwoRrn(); }
static void our_recivedCPtag(void *p)
	{ ((V90Phase4Modulator *)p)->recivedCPtag(); }
static void our_recivedSUVtag(void *p)
	{ ((V90Phase4Modulator *)p)->recivedSUVtag(); }
static void our_recivedE2u(void *p) { ((V90Phase4Modulator *)p)->recivedE2u(); }
static void our_recivedFirstRrnE2u(void *p)
	{ ((V90Phase4Modulator *)p)->recivedFirstRrnE2u(); }
static void our_recivedPartOneSilenceRrnSUVtag(void *p)
	{ ((V90Phase4Modulator *)p)->recivedPartOneSilenceRrnSUVtag(); }
static void our_recivedPartTwoSilenceRrnSUVtag(void *p)
	{ ((V90Phase4Modulator *)p)->recivedPartTwoSilenceRrnSUVtag(); }

static const struct arm edges[] = {
	{ "exitRi",		our_exitRi,	  ref_p4m_exitRi },
	{ "exitSilence",	our_exitSilence,  ref_p4m_exitSilence },
	{ "exitMP",		our_exitMP,	  ref_p4m_exitMP },
	{ "exitMPNot",		our_exitMPNot,	  ref_p4m_exitMPNot },
	{ "enterRepeatedCPd",	our_enterRepeatedCPd,
					   ref_p4m_enterRepeatedCPd },
	{ "recivedSUV",		our_recivedSUV,	  ref_p4m_recivedSUV },
	{ "recivedPartTwoSilenceRrnSUV", our_recivedPartTwoSilenceRrnSUV,
					   ref_p4m_recivedPartTwoSilenceRrnSUV },
	{ "recivedFirstSUVuPartTwoRrn", our_recivedFirstSUVuPartTwoRrn,
					   ref_p4m_recivedFirstSUVuPartTwoRrn },
	{ "recivedCPtag",	our_recivedCPtag, ref_p4m_recivedCPtag },
	{ "recivedSUVtag",	our_recivedSUVtag, ref_p4m_recivedSUVtag },
	{ "recivedE2u",		our_recivedE2u,	  ref_p4m_recivedE2u },
	{ "recivedFirstRrnE2u",	our_recivedFirstRrnE2u,
					   ref_p4m_recivedFirstRrnE2u },
	{ "recivedPartOneSilenceRrnSUVtag",
	  our_recivedPartOneSilenceRrnSUVtag,
					   ref_p4m_recivedPartOneSilenceRrnSUVtag },
	/*
	 * The five-byte sibling call the VPcmV34Main leaf pass claimed: its
	 * whole body is `recivedSUVtag()`, so the grid that drives that edge
	 * drives this one, and what THIS row adds is the claim that the
	 * symbol goes where its name says rather than to a different callee.
	 */
	{ "recivedPartTwoSilenceRrnSUVtag",
	  our_recivedPartTwoSilenceRrnSUVtag,
					   ref_p4m_recivedPartTwoSilenceRrnSUVtag }
};

#define NEDGES		((int)(sizeof edges / sizeof edges[0]))

/*
 * On a boundary and off one, for both divisors: 4 divides 0, 8 and 12, and 3
 * divides 0, 6 and 12.  Zero is in the list because four members test the
 * counter against zero BEFORE the modulus.
 */
static const unsigned int counts[] = { 0u, 1u, 6u, 8u, 12u };

#define NCOUNTS		((int)(sizeof counts / sizeof counts[0]))

/* The whole enumeration, plus 0x1f, which the class never mentions. */
#define NSTATES		0x20

static int
run_edges(void)
{
	long trial = 840000L;
	int a, st, latch, c, lvl;
	int printed = 0, moved = 0;
	unsigned char seen_state[NSTATES];
	int states = 0;

	diff_begin("V90Phase4Modulator state-machine edges");
	memset(seen_state, 0, sizeof seen_state);

	for (a = 0; a < NEDGES; a++)
		for (st = 0; st < NSTATES; st++)
		    for (latch = 0; latch < 8; latch++)
			for (c = 0; c < NCOUNTS; c++) {
				lvl = ((st + latch) & 1) ? 2 : 0;

				setup((int)trial, (int)(trial & 3), latch,
				      counts[c]);
				P4M(0).state = (Phase4ModulatorState)st;
				P4M(1).state = (Phase4ModulatorState)st;

				set_level((unsigned)lvl);
				dsplib_debug_capture_reset();
				dsplib_debug_capture_on = 1;
				edges[a].ours(p4m_s[0]);
				edges[a].theirs(p4m_s[1]);
				dsplib_debug_capture_on = 0;
				set_level(0);

				compare_all(edges[a].name, trial);

				if (dsplib_debug_capture_text(0)[0] != '\0')
					printed++;
				if (memcmp(p4m_s[0], p4m_seed,
					   sizeof(V90Phase4Modulator)) != 0)
					moved++;
				if ((unsigned)P4M(0).state < NSTATES &&
				    !seen_state[P4M(0).state]) {
					seen_state[P4M(0).state] = 1;
					states++;
				}
				trial++;
			}

	diff_eq_int("something was printed (%ld)", printed > 0, 1, trial);
	diff_eq_int("the modulator moved (%ld)", moved > 0, 1, trial);
	diff_eq_int("more than one state was reached (%ld)", states > 1, 1,
		    trial);

	return diff_end();
}

/* --------------------------------------------------- the two data pumps */

static int
run_pumps(void)
{
	long trial = 850000L;
	int mode, st, which;
	int printed = 0, varied = 0;
	short first = 0;
	int seen = 0;

	diff_begin("V90Phase4Modulator::generateDataSymbolBefore{FPE,RRN}");

	for (which = 0; which < 2; which++)
		for (mode = 0; mode < 4; mode++)
			for (st = 0; st < 6; st++) {
				short rc[2];

				setup((int)trial, mode, st & 7,
				      counts[st % NCOUNTS]);
				P4M(0).state = (Phase4ModulatorState)st;
				P4M(1).state = (Phase4ModulatorState)st;

				set_level((st & 1) ? 2u : 0u);
				dsplib_debug_capture_reset();
				dsplib_debug_capture_on = 1;
				if (which) {
					rc[0] = P4M(0)
					    .generateDataSymbolBeforeRRN();
					rc[1] =
					    ref_p4m_generateDataSymbolBeforeRRN(
						p4m_s[1]);
				} else {
					rc[0] = P4M(0)
					    .generateDataSymbolBeforeFPE();
					rc[1] =
					    ref_p4m_generateDataSymbolBeforeFPE(
						p4m_s[1]);
				}
				dsplib_debug_capture_on = 0;
				set_level(0);

				compare_all(which ? "beforeRRN" : "beforeFPE",
					    trial);
				diff_eq_int("the symbol (%ld)", (long)rc[0],
					    (long)rc[1], trial);

				if (dsplib_debug_capture_text(0)[0] != '\0')
					printed++;
				if (!seen) {
					first = rc[0];
					seen = 1;
				} else if (rc[0] != first) {
					varied++;
				}
				trial++;
			}

	diff_eq_int("a pump announced its state (%ld)", printed > 0, 1, trial);
	diff_eq_int("the pumps returned more than one symbol (%ld)",
		    varied > 0, 1, trial);

	return diff_end();
}

/* ------------------------------------------------------- blob vs blob */

/*
 * +0x0020 SELECTS, AND IN OPPOSITE SENSES.  `recivedCPtag` acts only when it
 * is non-zero; `recivedE2u` acts only when it is zero.  Two runs of the blob
 * with the same seed and the two settings must move the object on opposite
 * ones, which is what says the two guards are not the same guard.
 */
static int
run_latch(void)
{
	long trial = 860000L;
	int changed[2][2];
	int f, v;

	diff_begin("V90Phase4Modulator: +0x0020 selects, in two senses");

	for (f = 0; f < 2; f++)
		for (v = 0; v < 2; v++) {
			setup((int)trial, 0, v ? 1 : 0, 8u);
			P4M(1).state = P4M_STATE_CPD;
			P4M(1).word_2f9c = 1u;

			set_level(0);
			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			if (f)
				ref_p4m_recivedE2u(p4m_s[1]);
			else
				ref_p4m_recivedCPtag(p4m_s[1]);
			dsplib_debug_capture_on = 0;

			/*
			 * THE STATE AND NOT THE WHOLE OBJECT.  Both members
			 * clear +0x18 and +0x1c BEFORE the guard, so a
			 * byte-for-byte comparison moves on the rejected
			 * setting too and would hold nothing.  What the guard
			 * governs is +0x04.
			 */
			changed[f][v] = P4M(1).state != P4M_STATE_CPD;
		}

	diff_eq_int("recivedCPtag acts when +0x20 is set (%ld)",
		    changed[0][1], 1, trial);
	diff_eq_int("recivedE2u acts when +0x20 is clear (%ld)",
		    changed[1][0], 1, trial);
	diff_eq_int("the two guards are not the same guard (%ld)",
		    changed[0][1] != changed[0][0] &&
		    changed[1][0] != changed[1][1], 1, trial);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_edges();
	rc |= run_pumps();
	rc |= run_latch();

	return rc;
}
