/*
 * t_trn2dknown.cpp -- differential test of
 * `V90Phase4Demodulator::trn2dKnownDemod(short)` (0x25de0, 170 bytes),
 * claimed by the VPcmV34Main leaf pass.
 *
 * Callerless exported API, driven through the `ref_` alias (F7000).
 *
 * WHAT THE FIXTURE STANDS UP, and why it is small: the function runs the
 * EMBEDDED modulator's `generateSymbol()` one step, and that call fans out
 * by the modulator's `state`.  The fixture pins `sessionFlag` to 0 and
 * `state` to `P4M_STATE_RI` (0), whose arm is `generateRi()` -- a pure
 * in-object sequence reader with NO transition -- so nothing under either
 * side follows a pointer except the impairment detector, which is a SHARED
 * seeded block (input only: `linMapp` and `pcmType` are read, nothing is
 * written).  The rest of the demodulator is seed, compared whole.
 *
 * WHAT THE GRID DRIVES:
 *   - both signs of `codeLevel`, zero, and -32768 -- the last is the double
 *     absolute value through a short intermediate, the one input where
 *     `(short)abs` wraps and the compander is handed 32768;
 *   - `countInState` across more than one whole period of the `% 6`;
 *   - `pcmType` 0 and 1 for the two companding laws, AND 0x10000 /
 *     0x10001 -- the law test is a SIXTEEN-BIT compare (`cmpw`) of a
 *     four-byte field, and a value with only high bits set is what tells
 *     `cmpw` from a `cmpl` spelling.  Both sides read the same block, so
 *     this is a differential input like any other;
 *   - the short argument, which is NEVER READ -- varied anyway, so a
 *     reconstruction that started reading it would fail.
 *
 * The modulator's own step (`symbolCount++`, `word_000c = 0`) lands in the
 * object comparison, which is how the generateSymbol excursion is checked
 * without being separately instrumented.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90AutoDigitalImpDetector.h"
#include "dsplib/V90Phase4Demodulator.h"

extern "C" {
int ref_trn2dknown(void *self, short arg)
	asm("ref__ZN20V90Phase4Demodulator15trn2dKnownDemodEs");
}

#define GUARD	64
#define P4D_SLOT	((unsigned)sizeof(V90Phase4Demodulator) + GUARD)
#define AID_SLOT	((unsigned)sizeof(V90AutoDigitalImpDetector) + GUARD)

static unsigned char p4d[2][P4D_SLOT] __attribute__((aligned(8)));
static unsigned char aid[AID_SLOT] __attribute__((aligned(8)));
static unsigned char before[P4D_SLOT];
static unsigned char aid_before[AID_SLOT];

#define D(s)	((V90Phase4Demodulator *)(void *)p4d[s])
#define AID	((V90AutoDigitalImpDetector *)(void *)aid)

static unsigned lfsr;

static unsigned char
nextb(void)
{
	lfsr = (lfsr >> 1) ^ (unsigned)(-(int)(lfsr & 1u) & 0xb400u);
	return (unsigned char)((lfsr >> 3) | 1u);
}

static void
seed(long trial, short codeLevel, unsigned int count, int pcm)
{
	unsigned i;

	lfsr = 0x2d31u ^ (unsigned)trial * 0x9e37u;
	for (i = 0; i < AID_SLOT; i++)
		aid[i] = nextb();
	for (i = 0; i < P4D_SLOT; i++)
		p4d[0][i] = nextb();
	memcpy(p4d[1], p4d[0], P4D_SLOT);

	for (i = 0; i < 2; i++) {
		V90Phase4Demodulator *d = D((int)i);

		d->countInState = count;
		d->autoDigitalImpDetector = AID;
		d->phase4Modulator.sessionFlag = 0;
		d->phase4Modulator.state = P4M_STATE_RI;
		d->phase4Modulator.codeLevel = codeLevel;
		d->phase4Modulator.symbolCount =
		    0x100u + (unsigned)trial * 5u;
	}
	*(int *)(void *)&AID->pcmType = pcm;
	memcpy(before, p4d[0], P4D_SLOT);
	memcpy(aid_before, aid, AID_SLOT);
}

int
main(void)
{
	static const short levels[] = { 100, -100, 0, -32768, 32767, 1 };
	static const int laws[] = { 0, 1, 0x10000, 0x10001 };
	long tag = 0;
	int li, wi, ci;
	int rc;
	long lastr = -1;
	int distinct = 0;

	diff_begin("V90Phase4Demodulator::trn2dKnownDemod");

	for (li = 0; li < (int)(sizeof levels / sizeof levels[0]); li++)
	    for (wi = 0; wi < (int)(sizeof laws / sizeof laws[0]); wi++)
		for (ci = 1; ci <= 13; ci += 3) {
			int r0, r1;
			short arg = (short)(tag * 331);

			seed(tag, levels[li], (unsigned)ci, laws[wi]);

			r0 = D(0)->trn2dKnownDemod(arg);
			r1 = ref_trn2dknown(p4d[1], arg);

			diff_eq_int("the demodulated level (%ld)", r0, r1,
				    tag);
			diff_eq_obj("after the call", V90Phase4Demodulator,
				    D(0), D(1), tag);
			diff_eq_int("the guard held (%ld)",
				    memcmp(p4d[0]
					   + sizeof(V90Phase4Demodulator),
					   before
					   + sizeof(V90Phase4Demodulator),
					   GUARD) == 0, 1, tag);
			diff_eq_int("the detector block was only read (%ld)",
				    memcmp(aid, aid_before, AID_SLOT) == 0, 1,
				    tag);
			/* seed() planted 0x100 + trial*5; the RI arm adds 1 */
			diff_eq_int("the modulator stepped (%ld)",
				    (long)D(0)->phase4Modulator.symbolCount,
				    (long)(0x101u + (unsigned)tag * 5u), tag);

			if (lastr >= 0 && r0 != lastr)
				distinct = 1;
			lastr = r0;
			tag++;
		}

	diff_eq_int("the answers vary with the input", distinct, 1, 0);

	rc = diff_end();
	return rc;
}
