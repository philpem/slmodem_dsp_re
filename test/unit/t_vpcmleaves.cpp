/*
 * t_vpcmleaves.cpp -- differential test of the eight VPcmFloModem leaf
 * members claimed by the VPcmV34Main leaf pass:
 *
 *     setTerminateJaFlag(h)          0xd110   45 B
 *     setTerminateCpFlag(h)          0xd140   45 B
 *     setTerminateCpNotFlag(h)       0xd170   45 B
 *     setMinNofTransmitSequences(t)  0xd1a0   26 B
 *     setNofBitsPhase4(j)            0xd1c0   56 B
 *     resetBitPointer()              0xd200   51 B
 *     internalReset()                0xd4f0  165 B
 *     copyMpInfoForInterface()       0xd5a0  183 B
 *
 * These are the seven members `v90RunDemodulator` inlines -- the header
 * priced claiming them at "451 bytes and seven differential tests" and this
 * file is those tests -- plus `internalReset`.  None of them follows a
 * pointer: every load and store is `this`-relative, `copyMpInfoForInterface`
 * included (its source is the EMBEDDED `modem.mp`), so the fixture is two
 * seeded 32 KB slots and nothing else.  The destructor pair is t_vpcmctor's;
 * it needs a really-constructed object and lives with the constructor.
 *
 * THE OBJECTS ARE NEVER ZEROED and each member's stores are anti-vacuity
 * checked (findings F223, F224).  The three flag setters and setNofBitsPhase4
 * print at debug level 2, so the level is swept and the transcripts compared
 * -- the three flag bodies differ ONLY in the destination byte and the
 * string, and a pair with their strings swapped passes every object
 * comparison ever written (the t_v90p4dleaf argument).
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/VPcmFloModem.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void ref_set_ja(void *self, unsigned char v)
	asm("ref__ZN12VPcmFloModem18setTerminateJaFlagEh");
void ref_set_cp(void *self, unsigned char v)
	asm("ref__ZN12VPcmFloModem18setTerminateCpFlagEh");
void ref_set_cpnot(void *self, unsigned char v)
	asm("ref__ZN12VPcmFloModem21setTerminateCpNotFlagEh");
void ref_set_minseq(void *self, unsigned short n)
	asm("ref__ZN12VPcmFloModem26setMinNofTransmitSequencesEt");
void ref_set_nofbits(void *self, unsigned int c)
	asm("ref__ZN12VPcmFloModem16setNofBitsPhase4Ej");
void ref_resetbitptr(void *self)
	asm("ref__ZN12VPcmFloModem15resetBitPointerEv");
void ref_internalreset(void *self)
	asm("ref__ZN12VPcmFloModem13internalResetEv");
void ref_copympinfo(void *self)
	asm("ref__ZN12VPcmFloModem22copyMpInfoForInterfaceEv");
}

/* ------------------------------------------------------------- storage */

#define GUARD	64
#define SLOT	((unsigned)sizeof(VPcmFloModem) + GUARD)

static unsigned char vp[2][SLOT] __attribute__((aligned(8)));
static unsigned char before[SLOT];

#define M(s)	((VPcmFloModem *)(void *)vp[s])

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)((lfsr >> 3) | 1u);	/* never zero; F230 */
}

static void
seed(long trial)
{
	unsigned i;

	lfsr = 0x77e1u ^ (unsigned)trial * 0x9e37u;
	for (i = 0; i < SLOT; i++)
		vp[0][i] = nextb();
	memcpy(vp[1], vp[0], SLOT);
	memcpy(before, vp[0], SLOT);
}

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

static void
compare(const char *what, long tag)
{
	diff_eq_obj_(__FILE__, __LINE__, what, "VPcmFloModem",
		     vp[0], vp[1], (unsigned)sizeof(VPcmFloModem), tag);
	diff_eq_int("the guard held (%ld)",
		    memcmp(vp[0] + sizeof(VPcmFloModem),
			   before + sizeof(VPcmFloModem), GUARD) == 0
		    && memcmp(vp[1] + sizeof(VPcmFloModem),
			      before + sizeof(VPcmFloModem), GUARD) == 0,
		    1, tag);
	diff_eq_int("the transcripts agree (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
}

#define NTRIAL	16

/* ----------------------------------------------------- the flag setters */

typedef void (VPcmFloModem::*ourflag)(unsigned char);
typedef void (*refflag)(void *, unsigned char);

struct flagcase {
	const char	*name;
	refflag		theirs;
	unsigned	off;		/* the byte it stores          */
};

static const struct flagcase flags[] = {
	{ "VPcmFloModem::setTerminateJaFlag", ref_set_ja, 0x7dceu },
	{ "VPcmFloModem::setTerminateCpFlag", ref_set_cp, 0x7dcfu },
	{ "VPcmFloModem::setTerminateCpNotFlag", ref_set_cpnot, 0x7dd0u },
};

static void
our_flag(int which, unsigned char v)
{
	if (which == 0)
		M(0)->setTerminateJaFlag(v);
	else if (which == 1)
		M(0)->setTerminateCpFlag(v);
	else
		M(0)->setTerminateCpNotFlag(v);
}

static int
run_flags(void)
{
	int which, trial;
	long tag = 100;
	int distinct = 0;
	char kept[3][64];

	diff_begin("the three termination-flag setters");

	for (which = 0; which < 3; which++)
		for (trial = 0; trial < NTRIAL; trial++) {
			unsigned char v = (unsigned char)(trial * 53 + which);
			unsigned lvl = (trial & 1) ? 2u : 0u;

			seed(tag);
			set_level(lvl);
			dsplib_debug_capture_reset();
			dsplib_debug_capture_on = 1;
			our_flag(which, v);
			flags[which].theirs(vp[1], v);
			dsplib_debug_capture_on = 0;
			set_level(0);

			compare(flags[which].name, tag);
			diff_eq_int("the byte landed (%ld)",
				    vp[0][flags[which].off], v, tag);
			diff_eq_int("its two neighbours kept their seed (%ld)",
				    vp[0][flags[which].off == 0x7dceu
					  ? 0x7dcfu : 0x7dceu]
				    == before[flags[which].off == 0x7dceu
					      ? 0x7dcfu : 0x7dceu], 1, tag);

			/*
			 * The blob's transcript for each setter, kept so the
			 * three can be shown DISTINCT -- the swapped-string
			 * mutation is invisible to everything above.
			 */
			if (lvl == 2 && trial == 1) {
				strncpy(kept[which],
					dsplib_debug_capture_text(1), 63);
				kept[which][63] = 0;
			}
			tag++;
		}

	if (strcmp(kept[0], kept[1]) != 0 && strcmp(kept[0], kept[2]) != 0
	    && strcmp(kept[1], kept[2]) != 0)
		distinct = 1;
	diff_eq_int("the three messages are three messages", distinct, 1, 0);

	return diff_end();
}

/* ------------------------------------------------------- the other five */

static int
run_small(void)
{
	int trial;
	long tag = 500;
	int zeroArm = 0, bigArm = 0;

	diff_begin("setMinNofTransmitSequences / setNofBitsPhase4 / "
		   "resetBitPointer");

	for (trial = 0; trial < NTRIAL; trial++) {
		unsigned short n = (unsigned short)(trial * 4099 + 1);
		unsigned int c = (trial & 3);	/* 0 takes the 2-bit arm */
		unsigned lvl = (trial & 1) ? 2u : 0u;

		seed(tag);
		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;

		M(0)->setMinNofTransmitSequences(n);
		ref_set_minseq(vp[1], n);
		M(0)->setNofBitsPhase4(c);
		ref_set_nofbits(vp[1], c);
		M(0)->resetBitPointer();
		ref_resetbitptr(vp[1]);

		dsplib_debug_capture_on = 0;
		set_level(0);

		compare("the three small members", tag);
		diff_eq_int("the minimum landed (%ld)",
			    M(0)->minNofTransmitSequences, n, tag);
		diff_eq_int("the bit width landed (%ld)",
			    M(0)->nofBitsPerSymbol, c == 0 ? 2 : 4, tag);
		diff_eq_int("resetBitPointer cleared the pointer (%ld)",
			    M(0)->bitPointer, 0, tag);
		if (c == 0)
			zeroArm++;
		else
			bigArm++;
		tag++;
	}

	diff_eq_int("the 2-bit arm was reached", zeroArm > 0, 1, 0);
	diff_eq_int("the 4-bit arm was reached", bigArm > 0, 1, 0);

	return diff_end();
}

static int
run_internalreset(void)
{
	int trial;
	long tag = 700;

	diff_begin("VPcmFloModem::internalReset");

	for (trial = 0; trial < NTRIAL; trial++) {
		seed(tag);
		M(0)->internalReset();
		ref_internalreset(vp[1]);

		compare("internalReset", tag);
		diff_eq_int("the baud list is the V.90 one (%ld)",
			    M(0)->v34BaudAllow[0] == 1
			    && M(0)->v34BaudAllow[1] == 0
			    && M(0)->v34BaudAllow[5] == 0, 1, tag);
		diff_eq_int("the sequence pair is 0 and 1 (%ld)",
			    M(0)->nofTransmitSequences == 0
			    && M(0)->minNofTransmitSequences == 1, 1, tag);
		tag++;
	}

	return diff_end();
}

static int
run_copympinfo(void)
{
	int trial, distinct = 0;
	unsigned char first[20];
	long tag = 900;

	diff_begin("VPcmFloModem::copyMpInfoForInterface");

	for (trial = 0; trial < NTRIAL; trial++) {
		seed(tag);

		/*
		 * The rate mask is forced NEGATIVE on half the trials: the
		 * doubling is `(short)(x * 2)` and only its low sixteen bits
		 * survive, which is finding F7585's whole point -- a fixture
		 * that seeded it positive could not tell signed from
		 * unsigned.
		 */
		M(0)->modem.mp.rateMask = M(1)->modem.mp.rateMask =
		    (short)((trial & 1) ? -(0x1234 + trial * 7)
					: (0x2345 + trial * 7));

		M(0)->copyMpInfoForInterface();
		ref_copympinfo(vp[1]);

		compare("copyMpInfoForInterface", tag);
		diff_eq_int("the copy really happened (%ld)",
			    M(0)->mpType == (char)M(0)->modem.mp.Type, 1, tag);

		if (trial == 0)
			memcpy(first, &M(0)->mpType, 20);
		else if (memcmp(first, &M(0)->mpType, 20) != 0)
			distinct = 1;
		tag++;
	}

	diff_eq_int("the block is not the same on every trial", distinct, 1,
		    0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_flags();
	rc |= run_small();
	rc |= run_internalreset();
	rc |= run_copympinfo();

	return rc;
}
