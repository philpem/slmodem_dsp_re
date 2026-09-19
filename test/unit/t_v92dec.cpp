/*
 * t_v92dec.cpp -- V90Phase3Demodulator::getV92Decision against the blob.
 *
 * 8,616 bytes, a thirty-four-way state machine, and eleven collaborators it
 * calls into.  What this file has to establish is that OUR arm and the BLOB's
 * arm leave the SAME state behind in all eleven of them and return the same
 * `short` -- for every state, and for the branch inside each state.
 *
 * THE FIXTURE IS BUILT, NOT SEEDED, AND THAT IS THE WHOLE DESIGN.
 * `t_v90p3dreset.cpp` can seed its objects with pseudorandom bytes because
 * `reset` OVERWRITES what it touches.  This method READS dozens of fields and
 * hands them to real code: `samplesInState` is compared against parameter-block
 * slots, `word_04` indexes six-entry rows, `generateSymbol` runs a state
 * machine over the embedded modulator, and the impairment detector does x87
 * arithmetic over 40 KB of tables.  A random 32-bit pattern is a signalling
 * NaN about one time in 250 and a random counter is an unbounded loop, so:
 *
 *   1. every block is seeded pairwise and varied, as finding F230 requires;
 *   2. then `reset` is called on BOTH sides, which legalises the modulator,
 *      the descrambler, the two Jd records and the impairment detector;
 *   3. then, and only then, the fields this method reads are written to
 *      values inside their domains -- AFTER the reset, so nothing clears them.
 *
 * The impairment detector is zeroed before the reset rather than seeded.  Its
 * float tables run to 40 KB and `porcessFirstStudy`, `porcessSecondStudy` and
 * `calculateLinearMeanAndVar` all read them; zeroing puts finite values
 * everywhere and the three tables this method actually indexes -- `linMapp`,
 * `linMappAlt` and `prevLinMapp` -- are filled with varied SHORTS afterwards,
 * which cannot be a NaN whatever the pattern.
 *
 * THE PARAMETER BLOCK IS PER SIDE AND IT IS NOT SEEDED EITHER.  This method
 * WRITES three slots of it -- +0x438, +0x31c and +0x4cc -- so the two sides
 * cannot share one, and its words are small integers so that every slot read
 * as a float is finite and every slot read as a length is short.  The slots
 * the arms compare `samplesInState` against are then set per trial, which is how the
 * timeout branches are reached at all.
 *
 * THE ANSam DETECTOR IS CONSTRUCTED PER SIDE by that side's own constructor,
 * with the eight arguments `V90Phase3Demodulator`'s constructor uses.  It owns
 * a `GenericIIR<float,double>` it allocates, so its `filter` and that filter's
 * four pointers are per-side addresses that can never compare equal; they are
 * neutralised and what they point at is compared separately.
 *
 * HOW THE BRANCHES ARE REACHED.  `samplesInState` is incremented on entry before
 * anything compares it, so a trial that wants the comparison `samplesInState == X`
 * to fire sets `samplesInState = X - 1`.  `cand[]` below is every X the thirty-four
 * arms test against -- the eleven parameter slots, the five literals, the two
 * float constants, the two `timeoutBase`-relative constants and the two modular
 * conditions -- and the sweep runs all of them against all thirty-four states.
 * That is what makes this a test of the arms rather than of the dispatch.
 *
 * WHAT IS DELIBERATELY NOT COMPARED, and why it is not a hole: states 6, 18
 * and everything above 33 do not write the return value at all -- see the
 * method's own comment and docs/deviations.md D-V92DEC-1 -- so on those three
 * the OBJECT STATE is compared and the return value is not.  `run_states`
 * asserts that those three were reached, so the exclusion is narrow and
 * visible rather than a silent gap.
 */

#include <string.h>

#include "harness.h"

extern "C" {
short ref_getV92Decision(void *self, float sample)
	asm("ref__ZN20V90Phase3Demodulator14getV92DecisionEf");

void ref_p3d_reset(void *self, int pcmType, unsigned char ucode, int state,
		   unsigned int word2c, void *jd, void *jdV92, void *dil,
		   short altRbs, short short414, float float418,
		   unsigned int word14)
	asm("ref__ZN20V90Phase3Demodulator5resetE7PcmTypeh22Phase3Demodulator"
	    "StatejP5V90JdP5V92JdP19tagV90DILdescriptorssfj");

/* The ANSam detector, built and destroyed by each side's own code. */
void ansam_ctor(void *self, unsigned int s1, unsigned int s2, float thresh,
		unsigned int flag, float ratio, unsigned int rate,
		unsigned int blockLen, unsigned int blockSize)
	asm("_ZN17ANSamToneDetectorC1Ejjfjfjjj");
void ansam_dtor(void *self) asm("_ZN17ANSamToneDetectorD1Ev");
void ref_ansam_ctor(void *self, unsigned int s1, unsigned int s2, float thresh,
		    unsigned int flag, float ratio, unsigned int rate,
		    unsigned int blockLen, unsigned int blockSize)
	asm("ref__ZN17ANSamToneDetectorC1Ejjfjfjjj");
void ref_ansam_dtor(void *self) asm("ref__ZN17ANSamToneDetectorD1Ev");

extern unsigned int ref_dsplibs_debug_level;
}

#include "dsplib/debug.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase3Demodulator.h"
#include "dsplib/ANSamToneDetector.h"

#define SLOT		(0x42c + 64)
#define PARM_SLOT	(0x558 + 64)

/* The descrambler and scrambler the constructor builds; see t_v90p3dreset. */
#define DSC_TAP1	0x12u
#define DSC_TAIL	0x17u
#define DSC_OUT		0x63u
#define DSC_WORDS	(1u + DSC_TAIL + DSC_OUT)
#define SCR_BUF		64u
#define SCR_OUT		40u
#define SCR_TAP1	45u
#define SCR_TAP2	63u
#define SCR_TAIL	23u
#define SDD_HIST	12u

/* The ANSam detector's eight, from V90Phase3Demodulator.cpp. */
#define AN_SAMPLES1	0x190u
#define AN_SAMPLES2	0x64u
#define AN_THRESHOLD	307200.0f
#define AN_RATIO	0.5f
#define AN_RATE		8000u
#define AN_BLOCK_LEN	0x32u
#define AN_BLOCK_SIZE	0x63u

/* The parameter-block slots the thirty-four arms reach, as word indexes. */
#define P_PROBING_MODE	(0x004 / 4)
#define P_SD_08		(0x284 / 4)
#define P_SD_0C		(0x288 / 4)
#define P_SD_10		(0x28c / 4)
#define P_SD_LIMIT	(0x290 / 4)
#define P_DFE_LENGTH	(0x1fc / 4)
#define P_TRN1D_DD	(0x2fc / 4)
#define P_300		(0x300 / 4)
#define P_308		(0x308 / 4)
#define P_30C		(0x30c / 4)
#define P_310		(0x310 / 4)
#define P_314		(0x314 / 4)
#define P_318		(0x318 / 4)
#define P_31C		(0x31c / 4)
#define P_320		(0x320 / 4)
#define P_344		(0x344 / 4)
#define P_438		(0x438 / 4)
#define P_440		(0x440 / 4)
#define P_TRN1_QC_DD	(0x4a0 / 4)
#define P_4A4		(0x4a4 / 4)
#define P_ANSPCM	(0x4cc / 4)

struct p3d_slot {
	union {
		unsigned char raw[SLOT];
		double align_;			/* alignment only; trivial */
	};
	V90Phase3Demodulator &o;

	p3d_slot() : o(*(V90Phase3Demodulator *)raw) {}
};

static struct p3d_slot slot[2];

static unsigned char adid_[2][sizeof(V90AutoDigitalImpDetector)]
	__attribute__((aligned(8)));
static unsigned char sdd_[2][sizeof(V90SdDetector)] __attribute__((aligned(8)));
static unsigned char an_[2][sizeof(ANSamToneDetector)]
	__attribute__((aligned(8)));
static float sdhist[2][SDD_HIST];
static unsigned char jdo_[2][sizeof(V90Jd)] __attribute__((aligned(8)));
static unsigned char jd92o_[2][sizeof(V92Jd)] __attribute__((aligned(8)));

#define adid	((V90AutoDigitalImpDetector *)adid_)
#define sdd	((V90SdDetector *)sdd_)
#define an	((ANSamToneDetector *)an_)
#define jdo	((V90Jd *)jdo_)
#define jd92o	((V92Jd *)jd92o_)

static tagV90DILdescriptor dilo[2];
static int dbuf[2][DSC_WORDS];
static unsigned char sbuf[2][SCR_BUF];
static unsigned char parm[2][PARM_SLOT] __attribute__((aligned(8)));

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/* VARIED BYTES, NEVER ZEROS (finding F230). */
static void
fill_pair(void *a, void *b, size_t n)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	size_t i;

	for (i = 0; i < n; i++)
		pa[i] = pb[i] = next_byte();
}

/*
 * Everything the trial can vary.  Held in one record so that the sweep below
 * reads as a table and the setup reads as one function.
 */
struct trial_args {
	int		state;		/* +0x28, the arm under test    */
	unsigned int	samplesInState;	/* the counter BEFORE the entry ++ */
	unsigned int	word_04;	/* the frame position, 0..5     */
	unsigned int	quickConnect;	/* selects the short-TRN1 arms  */
	unsigned int	timeoutBase;	/* the two relative timeouts    */
	int		pcmType;	/* 0 mu-law, 1 A-law            */
	unsigned char	ucode;
	short		ucodeLevel;
	int		a948;		/* the detector's alt-RBS gate  */
	int		s2800;		/* altRbsFlag[word_04]          */
	int		b280c;		/* byte_280c[word_04]           */
	int		usingSeg;	/* the modulator's +0x392       */
	unsigned int	segmentPos;	/* the modulator's +0x38c       */
	unsigned char	dilPcmCode;	/* the modulator's +0x394       */
	unsigned int	eventCode;	/* the modulator's +0x01c       */
	unsigned char	byte_3f9;
	unsigned int	word_3fc;
	short		short_400;
	unsigned int	word_404;
	unsigned int	word_408;
	unsigned int	trn1dDdLength;
	unsigned int	dilLength;
	int		withJd92;	/* a null jdV92 is a real arm   */
	int		forceUcode;	/* make abs(symbol)==ucodeLevel */
	/*
	 * THREE BRANCHES NOTHING RANDOM REACHES, put one step from firing.
	 *   forceJd      the V92Jd data unpacker is left in its last state
	 *                with 0x33 of 0x34 bits taken and bits[28] clear, so
	 *                the next bit -- whatever it is -- completes a match.
	 *   forceJdPhase the same for the phase unpacker, whose tag test is
	 *                `phaseBits[28] != 1` rather than `!= 0`.
	 *   forceDilEnd  the modulator is put in P3M_STATE_DIL_END with a zero
	 *                segment position, which is the ONE place
	 *                `generateSymbol` writes eventCode 6 -- the shared
	 *                Phase3-Terminated tail of the seven DIL arms.
	 * Each needs about seventy specific bits or a whole DIL to arrive at
	 * honestly; setting the collaborator's own state is the same device
	 * this tree uses to reach a dispatch case cold.
	 */
	int		forceJd;
	int		forceJdPhase;
	int		forceDilEnd;
	float		sample;
};

static void
place_descrambler(int side, unsigned int out)
{
	Descrambler<int, int> *d = &slot[side].o.descrambler;
	int *buf = dbuf[side];

	d->pLimit = buf;
	d->pInitOut = buf + DSC_OUT;
	d->pInitTap1 = buf + DSC_OUT + DSC_TAP1;
	d->pInitTap2 = buf + DSC_OUT + DSC_TAIL;
	d->pOut = buf + out;
	d->pTap1 = buf + out + DSC_TAP1;
	d->pTap2 = buf + out + DSC_TAIL;
	d->tailLength = DSC_TAIL;
}

static void
place_scrambler(int side, unsigned int out)
{
	Scrambler<unsigned char, int> *s = &slot[side].o.phase3Modulator.scrambler;
	unsigned char *buf = sbuf[side];

	s->pLimit = buf;
	s->pInitOut = buf + SCR_OUT;
	s->pInitTap1 = buf + SCR_TAP1;
	s->pInitTap2 = buf + SCR_TAP2;
	s->pOut = buf + out;
	s->pTap1 = buf + out + (SCR_TAP1 - SCR_OUT);
	s->pTap2 = buf + out + (SCR_TAP2 - SCR_OUT);
	s->tailLength = SCR_TAIL;
}

/*
 * SMALL POSITIVE WORDS, NOT SEEDED BYTES.  Every slot of this block is read
 * somewhere in the object as an `int`, a `float` or a length; a small integer
 * bit pattern is a tiny denormal as a float -- finite, and never a NaN -- and
 * is a short loop bound as a length.  The slots the arms compare against are
 * overwritten per trial by `setup`.
 */
static void
fill_params(int side, int trial)
{
	unsigned int i;

	memset(parm[side], 0, PARM_SLOT);
	for (i = 1; i < 0x558 / 4; i++)
		V90PW(parm[side])[i] = (int)((i * 7u + (unsigned)trial) % 61u)
		    + 1;
	/* The SD detector's four, as finite floats and a small limit. */
	V90PF(parm[side])[P_SD_08] = 0.25f;
	V90PF(parm[side])[P_SD_0C] = 0.75f;
	V90PF(parm[side])[P_SD_10] = 1.5f;
	V90PW(parm[side])[P_SD_LIMIT] = 6;
	/* +0x000 is a POINTER in the real block; two arms test its low bit. */
	*(void **)&parm[side][0] = &parm[side][0x554];
	parm[side][0x554] = (unsigned char)(trial & 1);
}

/*
 * The detector's three mapping tables, filled with varied SHORTS.  A short can
 * never be a NaN, so this is finding F230's varied fill without finding F230's
 * hazard.  Everything else in the detector was zeroed and then reset.
 */
static void
fill_tables(int side, int trial)
{
	unsigned int i;

	for (i = 0; i < V90ADID_PHASES * V90ADID_CODES; i++) {
		adid[side].linMapp[0][i] =
		    (short)((int)((i * 37u + (unsigned)trial) % 4001u) - 2000);
		adid[side].linMappAlt[0][i] =
		    (short)((int)((i * 53u + (unsigned)trial) % 4001u) - 2000);
	}
	for (i = 0; i < V90ADID_CODES; i++)
		adid[side].prevLinMapp[i] =
		    (short)((int)((i * 71u + (unsigned)trial) % 4001u) - 2000);
}

/*
 * Seed, wire, construct, reset, then write everything the arm reads.  The
 * order matters and is the file comment's argument.
 */
static void
setup(int trial, const struct trial_args *t)
{
	int side;
	float sdinit[5];

	lfsr_state = 0x51a7u + 0x9e37u * (unsigned)trial;

	fill_pair(slot[0].raw, slot[1].raw, SLOT);
	fill_pair(sdhist[0], sdhist[1], sizeof(sdhist[0]));
	fill_pair(&jdo[0], &jdo[1], sizeof(jdo[0]));
	fill_pair(&jd92o[0], &jd92o[1], sizeof(jd92o[0]));
	fill_pair(&dilo[0], &dilo[1], sizeof(dilo[0]));
	fill_pair(dbuf[0], dbuf[1], sizeof(dbuf[0]));
	fill_pair(sbuf[0], sbuf[1], sizeof(sbuf[0]));

	/* FINITE, because the SD correlator does x87 over these. */
	{
		unsigned int i;

		for (i = 0; i < SDD_HIST; i++)
			sdhist[0][i] = sdhist[1][i] =
			    -1000.0f + 250.0f * (float)(int)
			    ((i * 5u + (unsigned)trial) % 9u);
	}

	sdinit[0] = 0.0f;
	sdinit[1] = 0.0f;
	sdinit[2] = 0.25f;
	sdinit[3] = 0.75f;
	sdinit[4] = 1.5f;

	for (side = 0; side < 2; side++) {
		memset((void *)&adid[side], 0, sizeof(adid[0]));
		memset((void *)&sdd[side], 0, sizeof(sdd[0]));
		memcpy((void *)&sdd[side], sdinit, sizeof(sdinit));
		fill_params(side, trial);

		slot[side].o.autoDigitalImpDetector = &adid[side];
		slot[side].o.params = (V90Parameters *)parm[side];
		slot[side].o.sdDetector = &sdd[side];
		slot[side].o.ansamToneDetector = &an[side];
		sdd[side].history = sdhist[side];
		sdd[side].historyLength = SDD_HIST;
		adid[side].params = (V90Parameters *)parm[side];

		dilo[side].dilCount = (unsigned char)(trial % 5 + 1);
		dilo[side].seq1Length = (unsigned char)(trial % 7 + 1);
		dilo[side].seq2Length = (unsigned char)(trial % 11 + 1);

		place_descrambler(side, (unsigned)trial % (DSC_OUT + 1u));
		place_scrambler(side, (unsigned)trial % 41u);
	}

	ansam_ctor(&an[0], AN_SAMPLES1, AN_SAMPLES2, AN_THRESHOLD, 0, AN_RATIO,
		   AN_RATE, AN_BLOCK_LEN, AN_BLOCK_SIZE);
	ref_ansam_ctor(&an[1], AN_SAMPLES1, AN_SAMPLES2, AN_THRESHOLD, 0,
		       AN_RATIO, AN_RATE, AN_BLOCK_LEN, AN_BLOCK_SIZE);

	/*
	 * The reset that legalises the graph.  State 0 with a zero symbol
	 * count is the cheapest opening; every field the method under test
	 * reads is written again below, so the opening does not leak into the
	 * trial.
	 */
	slot[0].o.reset((PcmType)t->pcmType, t->ucode,
			P3D_STATE_WAIT_FOR_SD, 0, &jdo[0],
			t->withJd92 ? &jd92o[0] : (V92Jd *)0, &dilo[0],
			0, 0, 0.0f, t->timeoutBase);
	ref_p3d_reset(&slot[1].o, t->pcmType, t->ucode, 0, 0, &jdo[1],
		      t->withJd92 ? (void *)&jd92o[1] : (void *)0, &dilo[1],
		      0, 0, 0.0f, t->timeoutBase);

	for (side = 0; side < 2; side++) {
		V90Phase3Demodulator *d = &slot[side].o;

		fill_tables(side, trial);
		adid[side].altRbsInUse = (short)t->a948;
		adid[side].altRbsFlag[t->word_04 % V90ADID_PHASES] =
		    (short)t->s2800;
		adid[side].byte_280c[t->word_04 % V90ADID_PHASES] =
		    (unsigned char)t->b280c;

		d->state = (Phase3DemodulatorState)t->state;
		d->samplesInState = t->samplesInState;
		d->eventCode = 0xdead;
		d->framePosition = t->word_04;
		d->quickConnect = t->quickConnect;
		d->timeoutBase = t->timeoutBase;
		d->ucode = t->ucode;
		d->ucodeLevel = t->ucodeLevel;
		d->pcmType = (PcmType)t->pcmType;
		d->byte_3f9 = t->byte_3f9;
		d->word_3fc = t->word_3fc;
		d->short_400 = t->short_400;
		d->jdNotRunLength = t->word_404;
		d->word_408 = t->word_408;
		d->trn1dDdLength = t->trn1dDdLength;
		d->word_3f4 = 0x5a5a5a5au;
		d->dilLength = t->dilLength;
		d->verificationStatus = 0x1234u;
		/*
		 * +0x3cc used to be declared `unsigned int` and this line
		 * seeded it as one.  It is a `SerialDifferentialDecoder<int>`
		 * now -- both reconstructions concluded that independently,
		 * findings F2102 and F2110 -- so the seed names the member.  It
		 * is the SAME four bytes set to the same 1, so the fixture is
		 * unchanged in what it does: it puts a non-zero previous bit
		 * into the differential decoder so that an arm which fails to
		 * run it is visible.
		 */
		d->word_3cc.prev_ = 1;

		if (t->forceJd) {
			V92Jd *j = &jd92o[side];

			j->unpackWord = 9;
			j->unpack[1] = 0x33;
			j->bits[28] = 0;
		}
		if (t->forceJdPhase) {
			V92Jd *j = &jd92o[side];

			j->unpackPhaseWord = 8;
			j->unpack[1] = 0x33;
			j->phaseBits[28] = 1;
		}
		d->phase3Modulator.usingSegmentLevel = (short)t->usingSeg;
		d->phase3Modulator.segmentPos = t->segmentPos;
		d->phase3Modulator.dilPcmCode = t->dilPcmCode;
		d->phase3Modulator.eventCode = t->eventCode;
		if (t->forceDilEnd) {
			/*
			 * `segmentIndex` AND `segmentLength` ARE SET HERE, AND
			 * THAT IS THE WHOLE OF FINDING F2107.
			 *
			 * This trial runs with `dilCount` zero, so
			 * `resetDILGenerator` returns without touching the DIL
			 * half and `segmentIndex` is still a seeded RANDOM BYTE.
			 * `segmentLength[segmentIndex]` has a four-byte stride
			 * from +0x158, so an index above 183 addresses past the
			 * end of this fixture's slot -- and past the slot the two
			 * sides are two different allocations, so they read
			 * different bytes and the wrap test goes different ways.
			 * BOTH IMPLEMENTATIONS DO THE SAME OUT-OF-BOUNDS READ;
			 * what differed was the memory, not the code.  Setting
			 * the index inside the array is what makes the trial a
			 * test of `generateSymbol` rather than of the allocator.
			 *
			 * A length of 1 against a zero position then ends the
			 * segment on this very symbol, which is the only route to
			 * event code 6 and so the only way to reach the shared
			 * Phase3-Terminated tail of the seven DIL arms.
			 */
			d->phase3Modulator.state = (Phase3ModulatorState)10;
			d->phase3Modulator.segmentPos = 0;
			d->phase3Modulator.segmentIndex = 0;
			d->phase3Modulator.segmentLength[0] = 1;
		}
	}
}

static void
teardown(void)
{
	ansam_dtor(&an[0]);
	ref_ansam_dtor(&an[1]);
}

/*
 * A copy of one side's slot with every per-side address replaced by something
 * both sides can agree about: an offset from that side's own buffer, or a
 * boolean saying it still points where `setup` put it.  Same idiom as
 * `snap` in t_v90p3dreset.cpp.
 */
static void
snap(unsigned char *dst, int side)
{
	V90Phase3Demodulator *s = (V90Phase3Demodulator *)dst;
	V90Phase3Demodulator *l = &slot[side].o;

	memcpy(dst, slot[side].raw, SLOT);

	s->autoDigitalImpDetector = (V90AutoDigitalImpDetector *)(long)
	    (l->autoDigitalImpDetector == &adid[side]);
	s->params = (V90Parameters *)(long)
	    (l->params == (V90Parameters *)parm[side]);
	s->sdDetector = (V90SdDetector *)(long)(l->sdDetector == &sdd[side]);
	s->ansamToneDetector = (ANSamToneDetector *)(long)
	    (l->ansamToneDetector == &an[side]);
	s->dil = (tagV90DILdescriptor *)(long)
	    (l->dil == NULL ? 2 : (l->dil == &dilo[side]));
	s->jd = (V90Jd *)(long)(l->jd == NULL ? 2 : (l->jd == &jdo[side]));
	s->jdV92 = (V92Jd *)(long)
	    (l->jdV92 == NULL ? 2 : (l->jdV92 == &jd92o[side]));

	s->descrambler.pLimit = (int *)(l->descrambler.pLimit - dbuf[side]);
	s->descrambler.pInitOut = (int *)(l->descrambler.pInitOut - dbuf[side]);
	s->descrambler.pInitTap1 =
	    (int *)(l->descrambler.pInitTap1 - dbuf[side]);
	s->descrambler.pInitTap2 =
	    (int *)(l->descrambler.pInitTap2 - dbuf[side]);
	s->descrambler.pOut = (int *)(l->descrambler.pOut - dbuf[side]);
	s->descrambler.pTap1 = (int *)(l->descrambler.pTap1 - dbuf[side]);
	s->descrambler.pTap2 = (int *)(l->descrambler.pTap2 - dbuf[side]);

	s->phase3Modulator.scrambler.pLimit = (unsigned char *)
	    (l->phase3Modulator.scrambler.pLimit - sbuf[side]);
	s->phase3Modulator.scrambler.pInitOut = (unsigned char *)
	    (l->phase3Modulator.scrambler.pInitOut - sbuf[side]);
	s->phase3Modulator.scrambler.pInitTap1 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pInitTap1 - sbuf[side]);
	s->phase3Modulator.scrambler.pInitTap2 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pInitTap2 - sbuf[side]);
	s->phase3Modulator.scrambler.pOut = (unsigned char *)
	    (l->phase3Modulator.scrambler.pOut - sbuf[side]);
	s->phase3Modulator.scrambler.pTap1 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pTap1 - sbuf[side]);
	s->phase3Modulator.scrambler.pTap2 = (unsigned char *)
	    (l->phase3Modulator.scrambler.pTap2 - sbuf[side]);
}

/*
 * The ANSam detector, its filter and the filter's two histories.  The four
 * pointers are per-side allocations and can never compare equal; the twelve
 * words behind them are the state that matters and they are compared.
 */
static void
compare_ansam(const char *what, long tag)
{
	/*
	 * The filter's four pointers are its first sixteen bytes and they are
	 * PRIVATE, so they are reached through a byte view rather than by
	 * name -- GenericIIR.h records the offsets and this test asserts them
	 * nowhere, because GenericIIR's own suite already does.
	 */
	static unsigned char a[sizeof(ANSamToneDetector)];
	static unsigned char b[sizeof(ANSamToneDetector)];
	static unsigned char fa[sizeof(GenericIIR<float, double>)];
	static unsigned char fb[sizeof(GenericIIR<float, double>)];
	unsigned char *ia = (unsigned char *)an[0].filter;
	unsigned char *ib = (unsigned char *)an[1].filter;
	unsigned int lenA, lenB;

	memcpy(a, (const void *)&an[0], sizeof(a));
	memcpy(b, (const void *)&an[1], sizeof(b));
	((ANSamToneDetector *)(void *)a)->filter = NULL;
	((ANSamToneDetector *)(void *)b)->filter = NULL;
	/*
	 * The six floats at +0x04..+0x1b are the base's threshold, ratio and
	 * four accumulators; the modern tier's rounding-level tolerance must
	 * reach those.  The counters, flags and the answer stay exact.
	 */
	{
		static const struct diff_float_span spans[] = { { 0x04, 6 } };

		diff_eq_obj_float_(__FILE__, __LINE__, what, "ANSamToneDetector",
				   a, b, sizeof(a), spans,
				   sizeof spans / sizeof spans[0], tag);
	}

	diff_eq_int("both filters exist (%ld)",
		    (ia != NULL) && (ib != NULL), 1, tag);
	if (ia == NULL || ib == NULL)
		return;

	memcpy(fa, ia, sizeof(fa));
	memcpy(fb, ib, sizeof(fb));
	memset(fa, 0, 16);			/* m_den .. m_outHist */
	memset(fb, 0, 16);
	/*
	 * +0x28 `m_i` and +0x2c `m_acc` are the filter's SCRATCH members --
	 * GenericIIR.h calls them "a loop counter, a member in the original"
	 * and "an accumulator, likewise".  They differ between our filter and
	 * the blob's after any `process`, and they differ for a reason that
	 * has nothing to do with this method: the blob leaves `m_i` at the
	 * loop bound and `m_acc` at whatever the allocator filled, and our
	 * reconstruction leaves both zero.  That is a GenericIIR question and
	 * it is recorded as finding F2103; excluding it here keeps this suite
	 * measuring getV92Decision.  Everything else about the filter,
	 * including both histories, IS compared.
	 */
	memset(fa + 0x28, 0, 12);
	memset(fb + 0x28, 0, 12);
	diff_eq_obj_(__FILE__, __LINE__, what, "GenericIIR<float,double>",
		     fa, fb, sizeof(fa), tag);

	memcpy(&lenA, ia + 0x18, sizeof(lenA));	/* m_inLen  */
	memcpy(&lenB, ib + 0x18, sizeof(lenB));
	if (lenA == lenB && lenA < 4096) {
		double *ha, *hb;

		memcpy(&ha, ia + 8, sizeof(ha));
		memcpy(&hb, ib + 8, sizeof(hb));
		diff_eq_obj_(__FILE__, __LINE__, what, "IIR input history",
			     ha, hb, lenA * sizeof(double), tag);
	}
	memcpy(&lenA, ia + 0x1c, sizeof(lenA));	/* m_outLen */
	memcpy(&lenB, ib + 0x1c, sizeof(lenB));
	if (lenA == lenB && lenA < 4096) {
		double *ha, *hb;

		memcpy(&ha, ia + 0xc, sizeof(ha));
		memcpy(&hb, ib + 0xc, sizeof(hb));
		diff_eq_obj_(__FILE__, __LINE__, what, "IIR output history",
			     ha, hb, lenA * sizeof(double), tag);
	}
}

static void
compare_all(const char *what, long tag)
{
	static unsigned char a[SLOT], b[SLOT];
	static unsigned char pa[PARM_SLOT], pb[PARM_SLOT];

	snap(a, 0);
	snap(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase3Demodulator slot",
		     a, b, SLOT, tag);

	/* The block's first word is each side's own pointer; the rest is not. */
	memcpy(pa, parm[0], PARM_SLOT);
	memcpy(pb, parm[1], PARM_SLOT);
	memset(pa, 0, sizeof(void *));
	memset(pb, 0, sizeof(void *));
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Parameters block",
		     pa, pb, PARM_SLOT, tag);

	{
		/* Its `params` is each side's own block; nothing else is. */
		static unsigned char aa_[sizeof(V90AutoDigitalImpDetector)]
			__attribute__((aligned(8)));
		static unsigned char ab_[sizeof(V90AutoDigitalImpDetector)]
			__attribute__((aligned(8)));

		memcpy(aa_, &adid[0], sizeof(aa_));
		memcpy(ab_, &adid[1], sizeof(ab_));
		((V90AutoDigitalImpDetector *)aa_)->params = NULL;
		((V90AutoDigitalImpDetector *)ab_)->params = NULL;
		diff_eq_obj_(__FILE__, __LINE__, what,
			     "V90AutoDigitalImpDetector", aa_, ab_,
			     sizeof(aa_), tag);
	}

	diff_eq_obj_(__FILE__, __LINE__, what, "V90SdDetector",
		     &sdd[0], &sdd[1], (size_t)__builtin_offsetof(
			 V90SdDetector, history), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90SdDetector history",
		     sdhist[0], sdhist[1], sizeof(sdhist[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Jd",
		     &jdo[0], &jdo[1], sizeof(jdo[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V92Jd",
		     &jd92o[0], &jd92o[1], sizeof(jd92o[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "tagV90DILdescriptor",
		     &dilo[0], &dilo[1], sizeof(dilo[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "descrambler buffer",
		     dbuf[0], dbuf[1], sizeof(dbuf[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "scrambler buffer",
		     sbuf[0], sbuf[1], sizeof(sbuf[0]), tag);
	compare_ansam(what, tag);
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
}

/*
 * States 6, 18 and anything above 33 leave the return value unwritten; see the
 * file comment.
 */
static int
returns_a_value(int st)
{
	return st != 6 && st != 18 && st >= 0 && st <= 33;
}

/*
 * WHAT EACH ARM LEAVES BEHIND THAT NOTHING ELSE DOES.  A sweep that agrees
 * with the blob proves nothing unless the interesting branches were entered,
 * and most of them are only visible through the state and event code the blob
 * writes.  These tallies are read off the BLOB's side after every call, and
 * `run_states` requires every one of them to be non-zero -- findings F247 and
 * F262 are why a coverage claim in this tree has to be satisfiable and checked.
 */
struct tally {
	int firstStudy;		/* case 10  -> state 0x0b            */
	int qcFirstStudy;	/* case 13  -> state 0x0e            */
	int secondStudy;	/* 11/12/15 -> state 0x10, eventCode 17 */
	int thirdStage;		/* case 11  -> state 0x0c, eventCode 16 */
	int qcSecond;		/* case 14  -> state 0x0f, eventCode 16 */
	int relaxation;		/* case 16  -> eventCode 0x12          */
	int relaxCount;		/* case 16  -> eventCode 0x0f          */
	int dilEnded;		/* case 17  -> eventCode 0x13          */
	int jdNot;		/* cases 5 and 9 -> eventCode 8 or 5   */
	int v92Jd;		/* case 7   -> eventCode 6             */
	int v92JdPhase;		/* case 8   -> eventCode 7             */
	int studyUref;		/* case 4   -> state 5               */
	int trn1dDd;		/* cases 2 and 3 -> state 4          */
	int nullJd92;		/* case 3   -> state 0x19            */
	int dilTimeout;		/* 10..16   -> state 0x18            */
	int terminated;		/* the shared tail -> eventCode 0x14   */
	int sdSeen;		/* case 0   -> state 1               */
	int sdNotSeen;		/* case 1   -> state 2               */
	int qtsSeen;		/* case 26  -> state 0x1b            */
	int qtsNotSeen;		/* case 27  -> state 0x1c            */
	int ansamEnter;		/* case 28  -> state 0x1d            */
	int ansamDrop;		/* case 30  -> state 0x1f            */
	int notSameLine;	/* cases 26/27 -> state 0x20 or 0x21 */
	int midEvent;		/* 11/14    -> eventCode 0x0c or 0x0a  */
	int ucodeForced;	/* the symbol was made to match      */
};

static struct tally tal;

static void
count_branches(void)
{
	const V90Phase3Demodulator *d = &slot[1].o;
	int st = (int)d->state;
	unsigned int ev = d->eventCode;

	if (st == 0x0b)
		tal.firstStudy++;
	if (st == 0x0e)
		tal.qcFirstStudy++;
	if (st == 0x10 && ev == 0x11)
		tal.secondStudy++;
	if (st == 0x0c && ev == 0x10)
		tal.thirdStage++;
	if (st == 0x0f && ev == 0x10)
		tal.qcSecond++;
	if (ev == 0x12)
		tal.relaxation++;
	if (ev == 0x0f)
		tal.relaxCount++;
	if (ev == 0x13)
		tal.dilEnded++;
	if (ev == 8 || ev == 5)
		tal.jdNot++;
	if (ev == 6)
		tal.v92Jd++;
	if (ev == 7)
		tal.v92JdPhase++;
	if (st == 5)
		tal.studyUref++;
	if (st == 4)
		tal.trn1dDd++;
	if (st == 0x19)
		tal.nullJd92++;
	if (st == 0x18)
		tal.dilTimeout++;
	if (ev == 0x14)
		tal.terminated++;
	if (st == 1)
		tal.sdSeen++;
	if (st == 2)
		tal.sdNotSeen++;
	if (st == 0x1b)
		tal.qtsSeen++;
	if (st == 0x1c)
		tal.qtsNotSeen++;
	if (st == 0x1d)
		tal.ansamEnter++;
	if (st == 0x1f)
		tal.ansamDrop++;
	if (st == 0x20 || st == 0x21)
		tal.notSameLine++;
	if (ev == 0x0c || ev == 0x0a)
		tal.midEvent++;
}

/*
 * `abs(symbol) == ucodeLevel` splits cases 11 and 14 in half and NOTHING
 * OBSERVABLE SAYS WHICH HALF RAN, so breadth would have been a guess.  It is
 * forced instead: the modulator and its scrambler buffer are saved, our side's
 * `generateSymbol` is run once to learn the symbol the real call is about to
 * produce, both are restored, and `ucodeLevel` is set to its magnitude on both
 * sides.  `generateSymbol` touches the modulator and that buffer and nothing
 * else, so the restore is exact -- and it is our own `generateSymbol`, which
 * `t_v90p3mod` has already shown to agree with the blob's, so the value it
 * predicts is the value both sides will see.
 */
static void
force_ucode_match(void)
{
	static unsigned char modsave[0x398];
	static unsigned char scrsave[SCR_BUF];
	int sym, mag, side;

	memcpy(modsave, (const void *)&slot[0].o.phase3Modulator,
	       sizeof(modsave));
	memcpy(scrsave, sbuf[0], sizeof(scrsave));

	sym = slot[0].o.phase3Modulator.generateSymbol();

	memcpy((void *)&slot[0].o.phase3Modulator, modsave, sizeof(modsave));
	memcpy(sbuf[0], scrsave, sizeof(scrsave));

	mag = (short)sym;
	if (mag < 0)
		mag = -mag;
	for (side = 0; side < 2; side++)
		slot[side].o.ucodeLevel = (short)mag;
	tal.ucodeForced++;
}

/*
 * One trial: `calls` consecutive samples through both sides, comparing after
 * each.  A single call exercises one arm's FIRST sample only, and every arm
 * that advances `word_04` or a counter behaves differently on the second.
 */
static void
run_trial(int trial, const struct trial_args *t, int calls, long tag)
{
	int i;

	setup(trial, t);

	for (i = 0; i < calls; i++) {
		float s = t->sample + 137.0f * (float)i;
		short mine, theirs;

		if (t->forceUcode)
			force_ucode_match();

		mine = slot[0].o.getV92Decision(s);
		theirs = ref_getV92Decision(&slot[1].o, s);

		if (returns_a_value(t->state))
			diff_eq_int("decision (%ld)", (long)mine,
				    (long)theirs, tag * 8 + i);
		compare_all("after getV92Decision", tag * 8 + i);
		count_branches();
	}

	teardown();
}

/*
 * Every value the thirty-four arms compare `samplesInState` against, expressed as the
 * value to PUT there -- one less than the target, because the method's first
 * act is `samplesInState++`.  `cand_of` needs the trial's own `timeoutBase` for the two
 * relative timeouts and the parameter block for the eleven slot ones.
 */
#define NCAND	26

/*
 * `fill_params` is deterministic in (slot, trial), so the value a slot WILL
 * hold can be computed before it is written.  This used to read the block
 * itself, which was read one trial too early -- `setup` had not run yet -- so
 * candidates 19..25 were systematically one below the PREVIOUS trial's value
 * and the seven parameter-slot timeouts fired only by coincidence with the
 * literal ones.  The tallies in `run_trial` are what proves the repair.
 */
static int
pslot(int idx, int trial)
{
	return (int)(((unsigned)idx * 7u + (unsigned)trial) % 61u) + 1;
}

static unsigned int
cand_of(int k, const struct trial_args *t, int trial)
{
	switch (k) {
	case 0:  return 0u;
	case 1:  return 1u;
	case 2:  return 5u;
	case 3:  return 0x30u - 1u;
	case 4:  return 0x180u - 1u;
	case 5:  return 0x300u - 1u;
	case 6:  return 0x7f8u - 1u;
	case 7:  return 0x7cfu - 1u;
	case 8:  return 0x7d0u;
	case 9:  return 0x9c40u - 1u;
	case 10: return 36000u - 1u;
	case 11: return 24804u - 1u;
	case 12: return t->timeoutBase + 12000u - 1u;
	case 13: return t->timeoutBase + 38760u - 1u;
	case 14: return t->trn1dDdLength - 1u;
	case 15: return t->dilLength - 1u;
	case 16: return 11u;			/* %6 == 0 on entry     */
	case 17: return 12u;			/* %6 != 0 on entry     */
	case 18: return 11u + 72u;		/* %72 == 12            */
	case 19: return (unsigned int)pslot(P_300, trial) - 1u;
	case 20: return (unsigned int)pslot(P_308, trial) - 1u;
	case 21: return (unsigned int)pslot(P_30C, trial) - 1u;
	case 22: return (unsigned int)pslot(P_310, trial) - 1u;
	case 23: return (unsigned int)pslot(P_314, trial) - 1u;
	case 24: return (unsigned int)pslot(P_318, trial) - 1u;
	default: return (unsigned int)pslot(P_ANSPCM, trial) - 1u;
	}
}

static const float sample_v[] = {
	0.0f, 1.0f, -1.0f, 4095.0f, -4095.0f, 8031.0f, -32000.0f, 0.5f
};
#define NSAMPLE ((int)(sizeof(sample_v) / sizeof(sample_v[0])))

static int
run_states(void)
{
	int st, k, v, pass, trial = 0;
	int sawNoValue = 0, sawValue = 0, sawAlt = 0, sawPlain = 0;
	int sawSeg = 0, sawNoSeg = 0, sawS2800 = 0, sawNoS2800 = 0;
	int sawMu = 0, sawA = 0, sawShortTrn = 0, sawLongTrn = 0;
	int sawNullJd92 = 0, sawJd92 = 0, sawUcodeHit = 0;

	diff_begin("V90Phase3Demodulator::getV92Decision -- all 34 arms");

	set_level(0);

	/*
	 * TWO PASSES OVER THE SAME SWEEP, and the second one exists to reach
	 * the shared Phase3-Terminated tail.  It is a separate pass rather
	 * than a bit multiplexed into `k`, because forcing the modulator to
	 * DIL_END rewrites its `eventCode` and several arms are selected BY
	 * that field -- folding it into the candidate index costs whichever
	 * transitions those candidates were there to drive, and it cost two
	 * of them when it was tried.  Pass 0 is the sweep exactly as it was.
	 */
	for (pass = 0; pass < 2; pass++)
	for (st = 0; st <= 35; st++) {
		for (k = 0; k < NCAND; k++) {
			struct trial_args t;
			long tag;

			v = (st * NCAND + k);
			memset(&t, 0, sizeof(t));
			t.state = st;
			t.word_04 = (unsigned int)(v % 6);
			t.timeoutBase = (unsigned int)((v % 5) * 100u);
			t.quickConnect = (v / 2) & 1;
			t.pcmType = (v / 3) & 1;
			t.ucode = (unsigned char)(v * 17u + 3u);
			t.a948 = (v / 5) & 1;
			t.s2800 = (v / 7) & 1;
			t.b280c = (v / 11) & 1;
			t.usingSeg = (v / 13) & 1;
			t.segmentPos = (unsigned int)((v / 17) & 1);
			t.dilPcmCode = (unsigned char)
			    (((v / 19) & 1) ? t.ucode : (unsigned char)(v + 1));
			t.eventCode = (unsigned int)(((v / 23) & 1) ? 6 : 2);
			t.byte_3f9 = (unsigned char)((v / 29) & 1);
			t.word_3fc = 0;
			t.short_400 = (short)((v / 31) & 1);
			t.word_404 = (unsigned int)(0x20u + (v % 3) * 6u);
			t.word_408 = (unsigned int)(v % 3);
			t.trn1dDdLength = (unsigned int)(v % 97 + 1);
			t.dilLength = (unsigned int)(v % 89 + 1);
			t.withJd92 = (st == 3) ? ((v / 4) & 1) : 1;
			t.forceUcode = (v / 37) & 1;
			t.forceJd = (st == 7 && (k & 1));
			t.forceJdPhase = (st == 8 && (k & 1));
			t.forceDilEnd = pass;
			if (t.forceJd || t.forceJdPhase)
				t.samplesInState = 11u;	/* 12 on entry, %6 == 0 */
			t.sample = sample_v[v % NSAMPLE];
			t.ucodeLevel = (short)((v & 1) ? 4095 : 1234);
			/*
			 * `word_3fc` is compared against +0x31c one step
			 * after it is incremented, so it starts one short.
			 */
			t.word_3fc = (unsigned int)
			    pslot(P_31C, trial) - (unsigned int)((v & 1));
			t.samplesInState = cand_of(k, &t, trial);

			tag = (long)pass * 100000 + (long)st * 1000 + k;
			run_trial(trial++, &t, 3, tag);

			if (returns_a_value(st))
				sawValue = 1;
			else
				sawNoValue = 1;
			if (t.a948)
				sawAlt = 1;
			else
				sawPlain = 1;
			if (t.usingSeg)
				sawSeg = 1;
			else
				sawNoSeg = 1;
			if (t.s2800)
				sawS2800 = 1;
			else
				sawNoS2800 = 1;
			if (t.pcmType)
				sawA = 1;
			else
				sawMu = 1;
			if (t.quickConnect)
				sawShortTrn = 1;
			else
				sawLongTrn = 1;
			if (t.withJd92)
				sawJd92 = 1;
			else
				sawNullJd92 = 1;
			if (t.ucodeLevel == 4095)
				sawUcodeHit = 1;
		}
	}

	diff_eq_int("an arm that returns a value was reached", sawValue, 1, 0);
	diff_eq_int("an arm that does not was reached", sawNoValue, 1, 0);
	diff_eq_int("the alt-RBS gate was set", sawAlt, 1, 0);
	diff_eq_int("...and clear", sawPlain, 1, 0);
	diff_eq_int("usingSegmentLevel was set", sawSeg, 1, 0);
	diff_eq_int("...and clear", sawNoSeg, 1, 0);
	diff_eq_int("altRbsFlag was set", sawS2800, 1, 0);
	diff_eq_int("...and clear", sawNoS2800, 1, 0);
	diff_eq_int("the mu-law arm was taken", sawMu, 1, 0);
	diff_eq_int("the A-law arm was taken", sawA, 1, 0);
	diff_eq_int("the short-TRN1 arm was taken", sawShortTrn, 1, 0);
	diff_eq_int("the long-TRN1 arm was taken", sawLongTrn, 1, 0);
	diff_eq_int("a null V92Jd was passed", sawNullJd92, 1, 0);
	diff_eq_int("a non-null V92Jd was passed", sawJd92, 1, 0);
	diff_eq_int("a symbol matching ucodeLevel was possible", sawUcodeHit,
		    1, 0);

	/*
	 * The branch tallies.  Every one of these is a transition only ONE arm
	 * makes, read off the blob's side, so a zero here means that arm's
	 * interesting path was never entered and the agreement above was
	 * agreement about nothing.
	 */
	diff_eq_int("case 10 reached porcessFirstStudy", tal.firstStudy > 0,
		    1, 0);
	diff_eq_int("case 13 reached porcessFirstStudy", tal.qcFirstStudy > 0,
		    1, 0);
	diff_eq_int("a second-study transition fired", tal.secondStudy > 0,
		    1, 0);
	diff_eq_int("case 11's third-stage transition fired",
		    tal.thirdStage > 0, 1, 0);
	diff_eq_int("case 14's QC-second transition fired", tal.qcSecond > 0,
		    1, 0);
	diff_eq_int("case 16's +0x318 event fired", tal.relaxation > 0, 1, 0);
	diff_eq_int("case 16's +0x31c counter event fired", tal.relaxCount > 0,
		    1, 0);
	diff_eq_int("case 17 saw the DIL end", tal.dilEnded > 0, 1, 0);
	diff_eq_int("a JdNot detection fired", tal.jdNot > 0, 1, 0);
	diff_eq_int("case 7 detected a V92Jd", tal.v92Jd > 0, 1, 0);
	diff_eq_int("case 8 detected a V92JdPhase", tal.v92JdPhase > 0, 1, 0);
	diff_eq_int("case 4 entered study reference Ucode", tal.studyUref > 0,
		    1, 0);
	diff_eq_int("an arm entered TRN1d DD", tal.trn1dDd > 0, 1, 0);
	diff_eq_int("case 3 took the null V92Jd arm", tal.nullJd92 > 0, 1, 0);
	diff_eq_int("a DIL timeout fired", tal.dilTimeout > 0, 1, 0);
	/*
	 * THE SHARED Phase3-Terminated TAIL, WHICH USED TO BE A DECLARED GAP.
	 * The seven DIL arms end with `if (phase3Modulator.eventCode == 6)`,
	 * and `generateSymbol` writes 6 in exactly one place: the DIL_END case
	 * when `segmentPos` is zero after `dilSymbol`.  This assertion read
	 * `== 0` and the flag that reaches the tail was off, because forcing
	 * it made the two sides disagree and that was recorded as a divergence
	 * in `generateSymbol` (finding F2107).  It was not one: the fixture left
	 * `segmentIndex` a random byte and `segmentLength[segmentIndex]` read
	 * off the end of the slot, so the two sides were reading different
	 * memory rather than running different code -- 2152, and `setup` now
	 * puts the index inside the array.  The tail is reached and compared.
	 */
	diff_eq_int("the terminated tail is reached (%ld)",
		    tal.terminated > 0, 1, 0);
	diff_eq_int("case 0 detected Sd", tal.sdSeen > 0, 1, 0);
	diff_eq_int("case 1 detected SdNot", tal.sdNotSeen > 0, 1, 0);
	diff_eq_int("case 26 detected QTS", tal.qtsSeen > 0, 1, 0);
	diff_eq_int("case 27 detected QTSNot", tal.qtsNotSeen > 0, 1, 0);
	diff_eq_int("case 28 entered ANSpcm demod", tal.ansamEnter > 0, 1, 0);
	diff_eq_int("case 30 saw the ANSpcm energy drop", tal.ansamDrop > 0,
		    1, 0);
	diff_eq_int("a not-same-line decision was reached",
		    tal.notSameLine > 0, 1, 0);
	diff_eq_int("cases 11/14 reached their mid-arm event codes",
		    tal.midEvent > 0, 1, 0);
	diff_eq_int("the symbol was forced to match ucodeLevel",
		    tal.ucodeForced > 0, 1, 0);

	return diff_end();
}

/*
 * The same sweep with the diagnostics on.  Two arms differ from their
 * neighbours ONLY by what they print -- state 30 sets the same event code as
 * state 33 and prints nothing where 33 prints -- so at level 0 they are
 * indistinguishable, and this says so rather than pretending otherwise.
 */
static int
run_transcripts(void)
{
	int st, lvl, printed = 0, trial = 20000;

	diff_begin("getV92Decision -- the diagnostics");

	dsplib_debug_capture_on = 1;

	for (lvl = 2; lvl <= 3; lvl++) {
		set_level((unsigned int)lvl);
		for (st = 0; st <= 33; st++) {
			struct trial_args t;
			int k;

			for (k = 0; k < NCAND; k++) {
				long tag = (long)lvl * 100000 + st * 100 + k;

				memset(&t, 0, sizeof(t));
				t.state = st;
				t.word_04 = (unsigned int)((st + k) % 6);
				t.timeoutBase = 100u;
				t.quickConnect = (k & 1);
				t.pcmType = (k / 2) & 1;
				t.ucode = (unsigned char)(k * 13u + 7u);
				t.ucodeLevel = 4095;
				t.a948 = (k / 4) & 1;
				t.s2800 = (k / 8) & 1;
				t.b280c = (k / 16) & 1;
				t.usingSeg = (k / 3) & 1;
				t.segmentPos = 0;
				t.dilPcmCode = (unsigned char)(k + 1);
				t.eventCode = 6;
				t.byte_3f9 = 1;
				t.short_400 = (short)((k / 5) & 1);
				t.word_404 = 12u;
				t.word_408 = 2u;
				t.trn1dDdLength = 50u;
				t.dilLength = 40u;
				t.withJd92 = (st == 3) ? (k & 1) : 1;
				t.sample = sample_v[k % NSAMPLE];
				t.word_3fc =
				    (unsigned int)pslot(P_31C, trial) - 1u;
				t.samplesInState = cand_of(k, &t, trial);
				/*
				 * THE THREE GATED DIAGNOSTICS LIVE BEHIND
				 * BRANCHES NOTHING RANDOM REACHES, so the
				 * level-2 sweep forces them the same way the
				 * level-0 one does.  Case 7 prints
				 * "waitForJd framePosition" and
				 * "adjustUinfoToPhaseOffset" only once the
				 * V92Jd unpacker has declared a match; case 9
				 * prints "changing state to
				 * DILDemodQCfirstStudy" only when the JdNot
				 * arm fires with +0x410 set, which needs a
				 * zero bit, a frame counter congruent to 12
				 * modulo 72 and a run of at least twelve
				 * zeros behind it at once.
				 */
				t.forceJd = (st == 7);
				t.forceJdPhase = (st == 8);
				if (st == 7 || st == 8)
					t.samplesInState = 11u;
				if (st == 9) {
					t.samplesInState = 11u + 72u;
					t.quickConnect = 1;
					t.word_404 = 0x40u;
				}
				/*
				 * Case 3 prints "V92 setting params for short
				 * TRN1" only when the frame counter is exactly
				 * 0x7f8, the V92Jd record is present AND
				 * +0x410 is set; the free sweep never lines
				 * all three up because the candidate that
				 * gives 0x7f8 is an even k and both of the
				 * others are driven from k's low bit.
				 */
				if (st == 3) {
					t.samplesInState = 0x7f8u - 1u;
					t.withJd92 = (k & 1) == 0;
					t.quickConnect = 1;
				}

				dsplib_debug_capture_reset();
				run_trial(trial++, &t, 2, tag);
				printed +=
				    (int)dsplib_debug_capture_lines(0);
				diff_eq_int("transcript line count (%ld)",
				    (long)dsplib_debug_capture_lines(0),
				    (long)dsplib_debug_capture_lines(1), tag);
				diff_eq_int("transcript text (%ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			}
		}
	}

	dsplib_debug_capture_on = 0;
	set_level(0);

	diff_eq_int("the diagnostics were emitted", printed > 0, 1, 0);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	bad |= run_states();
	bad |= run_transcripts();

	return bad;
}
