/*
 * t_v90p3ddec.cpp -- `V90Phase3Demodulator::getV90Decision(float)` against the
 * blob's own copy.
 *
 * 8,379 bytes and thirty-four dispatch cases, so the fixture matters more than
 * usual.  What is compared after every single call:
 *
 *     the returned decision                (except on the default path, below)
 *     the 0x42c demodulator object         including the embedded modulator
 *     the V90AutoDigitalImpDetector        43 KB of tables and accumulators
 *     the V90SdDetector and its history
 *     the ANSamToneDetector, its GenericIIR and all four of the filter's arrays
 *     the V90Jd and the DIL descriptor
 *     the descrambler and scrambler buffers
 *     the parameter block                  -- this method WRITES to it
 *     the debug transcript
 *
 * The parameter block is not shared between the sides the way t_v90adid.cpp
 * and t_v90p3dreset.cpp share theirs, because this method stores through it
 * three different ways: `params->PHASE4_MEAN_ERROR_...` from `unnamed_440`,
 * `unnamed_31c` from `unnamed_320`, and `ANSPCM_DEMODULATION_LENGTH` from a
 * literal 0x320.  A shared block would let a reconstruction that wrote the
 * wrong one of those pass.
 *
 * THE RETURN IS NOT ASSERTED ON THE DEFAULT PATH.  States 7, 8, 0x12 and
 * everything above 0x21 dispatch to a block that never writes the register the
 * value comes back in, so what the two sides return there is whatever each
 * call site happened to leave in `%edi`.  Asserting it would be asserting an
 * accident.  The OBJECT is still compared on those states, which is the part
 * that is defined.  D321.
 *
 * THE THRESHOLD PARAMETERS ARE SET SMALL AND DISTINCT.  Nine of the switch's
 * arms turn on `word_2c` reaching a field of the parameter block, and with the
 * block's real defaults (thousands of samples) a sweep long enough to reach
 * them is a sweep too long to run.  Setting them to 3, 5, 7 ... and sweeping
 * `word_2c` across 0..30 reaches every one of them, and the four hard-coded
 * timeouts -- 0x30, 0x180, 0x300, 0x7cf, 0x7f8, 0x9c40, and the two that are
 * `word_14` plus a float constant -- are reached by naming them directly.
 */

#include <string.h>

#include "harness.h"

extern "C" {
short ref_p3d_getV90Decision(void *self, float sample)
	asm("ref__ZN20V90Phase3Demodulator14getV90DecisionEf");

/*
 * The ten small members.  The return types below are the reconstruction's
 * reading of the object and are therefore part of what is under test: a
 * `getMaxUcode` that returned something other than a pointer would still link.
 */
int ref_p3d_getDecision(void *self, float sample)
	asm("ref__ZN20V90Phase3Demodulator11getDecisionEf");
int ref_p3d_twoLevelDemod(void *self, float sample, int *bit)
	asm("ref__ZN20V90Phase3Demodulator13twoLevelDemodEfRi");
void ref_p3d_exitDIL(void *self)
	asm("ref__ZN20V90Phase3Demodulator7exitDILEv");
int ref_p3d_JdNotDetector(void *self, int symbol)
	asm("ref__ZN20V90Phase3Demodulator13JdNotDetectorEi");
void ref_p3d_setDigitalImairmentsInfo(void *self)
	asm("ref__ZN20V90Phase3Demodulator24setDigitalImairmentsInfoEv");
void ref_p3d_enterWaitForANSpcmDrop(void *self)
	asm("ref__ZN20V90Phase3Demodulator22enterWaitForANSpcmDropEv");
void ref_p3d_incrementFramePosition(void *self)
	asm("ref__ZN20V90Phase3Demodulator22incrementFramePositionEv");
void ref_p3d_setAltRbsParams(void *self)
	asm("ref__ZN20V90Phase3Demodulator15setAltRbsParamsEv");
void ref_p3d_resetJdNotDetector(void *self)
	asm("ref__ZN20V90Phase3Demodulator18resetJdNotDetectorEv");
unsigned char *ref_p3d_getMaxUcode(void *self)
	asm("ref__ZN20V90Phase3Demodulator11getMaxUcodeEv");

extern unsigned int ref_dsplibs_debug_level;
}

#include "dsplib/debug.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/modem_params.h"
#include "dsplib/ANSamToneDetector.h"
#include "dsplib/V90Phase3Demodulator.h"

/*
 * Constructing over storage that already exists, with no <new> to include --
 * the build is -nostdinc++.  Same device as t_floatiirfree.cpp.
 */
inline void *operator new(size_t n, void *p) { (void)n; return p; }

/* The object is 0x42c; the slot is larger so an overrunning store shows up. */
#define SLOT		(0x42c + 64)

/*
 * THE TWO EMPTY SPECIAL MEMBERS ARE LOAD-BEARING -- see t_v90p3dreset.cpp,
 * which explains why a class with a non-trivial destructor cannot be a union
 * member without them.
 */
struct p3d_slot {
	union {
		unsigned char raw[SLOT];
		double align_;		/* alignment only; trivial */
	};
	V90Phase3Demodulator &o;

	p3d_slot() : o(*(V90Phase3Demodulator *)raw) {}
};

/* The descrambler and scrambler shapes the two constructors build. */
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

/* The eight arguments V90Phase3Demodulator's constructor builds one with. */
#define ANSAM_A1	0x190u
#define ANSAM_A2	0x64u
#define ANSAM_THRESH	307200.0f
#define ANSAM_FLAG	0u
#define ANSAM_RATIO	0.5f
#define ANSAM_RATE	8000u
#define ANSAM_BLOCK	50u
#define ANSAM_SIZE	99u

static struct p3d_slot slot[2];

static unsigned char adid_[2][sizeof(V90AutoDigitalImpDetector)]
	__attribute__((aligned(8)));
static unsigned char sdd_[2][sizeof(V90SdDetector)]
	__attribute__((aligned(8)));
static unsigned char jdo_[2][sizeof(V90Jd)] __attribute__((aligned(8)));
static unsigned char ansam_[2][sizeof(ANSamToneDetector)]
	__attribute__((aligned(8)));
static unsigned char parm_[2][sizeof(V90Parameters)]
	__attribute__((aligned(8)));

#define adid	((V90AutoDigitalImpDetector *)adid_)
#define sdd	((V90SdDetector *)sdd_)
#define jdo	((V90Jd *)jdo_)
#define ansam	((ANSamToneDetector *)ansam_)
#define parm	((V90Parameters *)parm_)

static float sdhist[2][SDD_HIST];
static tagV90DILdescriptor dilo[2];
static int dbuf[2][DSC_WORDS];
static unsigned char sbuf[2][SCR_BUF];
static struct _tagModemParameters mparm[2];

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/* VARIED BYTES, NEVER ZEROS (finding 230). */
static void
fill_pair(void *a, void *b, size_t n)
{
	unsigned char *pa = (unsigned char *)a;
	unsigned char *pb = (unsigned char *)b;
	size_t i;

	for (i = 0; i < n; i++)
		pa[i] = pb[i] = next_byte();
}

static void
place_descrambler(int side)
{
	Descrambler<int, int> *d = &slot[side].o.descrambler;
	int *buf = dbuf[side];

	d->pLimit = buf;
	d->pInitOut = buf + DSC_OUT;
	d->pInitTap1 = buf + DSC_OUT + DSC_TAP1;
	d->pInitTap2 = buf + DSC_OUT + DSC_TAIL;
	d->pOut = buf + DSC_OUT;
	d->pTap1 = buf + DSC_OUT + DSC_TAP1;
	d->pTap2 = buf + DSC_OUT + DSC_TAIL;
	d->tailLength = DSC_TAIL;
}

static void
place_scrambler(int side)
{
	Scrambler<unsigned char, int> *s =
	    &slot[side].o.phase3Modulator.scrambler;
	unsigned char *buf = sbuf[side];

	s->pLimit = buf;
	s->pInitOut = buf + SCR_OUT;
	s->pInitTap1 = buf + SCR_TAP1;
	s->pInitTap2 = buf + SCR_TAP2;
	s->pOut = buf + SCR_OUT;
	s->pTap1 = buf + SCR_TAP1;
	s->pTap2 = buf + SCR_TAP2;
	s->tailLength = SCR_TAIL;
}

/*
 * The parameter block: seeded like everything else, then every field this
 * method reads is SET, so that a `word_2c` sweep of thirty samples reaches
 * all nine of the thresholds rather than none of them.  The two float fields
 * are set for the reason v90demfix.h gives -- a seeded 32-bit pattern is a
 * signalling NaN about one time in 250.
 */
static void
set_params(int side, int trial)
{
	V90Parameters *p = &parm[side];

	p->modemParams = &mparm[side];
	mparm[side].sessionFlags = (unsigned char)(trial & 1);

	p->PROBING_MODE = (trial >> 1) & 1;
	p->DFE_LENGTH = 2;
	p->TRN1D_DD_LENGTH = 21;
	p->unnamed_300 = 3;
	p->unnamed_308 = 5;
	p->unnamed_30c = 7;
	p->unnamed_310 = (trial >> 2) & 1 ? 9 : 0;
	p->unnamed_314 = 11;
	p->unnamed_318 = 13;
	p->unnamed_31c = 15;
	p->unnamed_320 = 17;
	p->unnamed_344 = 19;
	p->TRN1_QC_DD_LENGTH = 23;
	p->unnamed_4a4 = 25;
	p->ANSPCM_DEMODULATION_LENGTH = 27;

	p->PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH = 1.5f;
	p->QC_PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH = 1.75f;
	p->unnamed_440 = 10.0f;
}

/*
 * The three linear-mapping tables, given varied contents after `reset` has
 * cleared them: every arm of this method that produces a decision at all
 * reads one of the three, and a table of zeros makes all three agree.
 */
static void
fill_tables(int side, int trial)
{
	short *lm = &adid[side].linMapp[0][0];
	short *la = &adid[side].linMappAlt[0][0];
	unsigned int i;

	for (i = 0; i < V90ADID_PHASES * V90ADID_CODES; i++) {
		lm[i] = (short)(i * 7u + (unsigned)trial);
		la[i] = (short)(0 - (int)(i * 3u) + trial);
	}
	for (i = 0; i < V90ADID_CODES; i++)
		adid[side].prevLinMapp[i] = (short)(i * 11u - (unsigned)trial);
}

static void
build(int trial, PcmType law, unsigned char ucode, unsigned int word14)
{
	int side;

	lfsr_state = 0xa37u + 0x9e37u * (unsigned)trial;

	fill_pair(slot[0].raw, slot[1].raw, SLOT);
	fill_pair(&adid[0], &adid[1], sizeof(adid[0]));
	fill_pair(&sdd[0], &sdd[1], sizeof(sdd[0]));
	fill_pair(sdhist[0], sdhist[1], sizeof(sdhist[0]));
	fill_pair(&jdo[0], &jdo[1], sizeof(jdo[0]));
	fill_pair(&dilo[0], &dilo[1], sizeof(dilo[0]));
	fill_pair(dbuf[0], dbuf[1], sizeof(dbuf[0]));
	fill_pair(sbuf[0], sbuf[1], sizeof(sbuf[0]));
	fill_pair(&parm[0], &parm[1], sizeof(parm[0]));
	fill_pair(&mparm[0], &mparm[1], sizeof(mparm[0]));

	for (side = 0; side < 2; side++) {
		set_params(side, trial);

		slot[side].o.autoDigitalImpDetector = &adid[side];
		slot[side].o.sdDetector = &sdd[side];
		slot[side].o.params = &parm[side];
		adid[side].params = &parm[side];
		sdd[side].history = sdhist[side];
		sdd[side].historyLength = SDD_HIST;
		sdd[side].limit = 8;
		sdd[side].count = (unsigned int)(trial % 12);

		dilo[side].dilCount = (unsigned char)(trial % 5u + 1u);
		dilo[side].seq1Length = (unsigned char)(trial * 13u % 65u);
		dilo[side].seq2Length = (unsigned char)(trial * 29u % 65u);

		place_descrambler(side);
		place_scrambler(side);

		/*
		 * Each side gets its own detector, built by our constructor.
		 *
		 * THE FILTER'S SCRATCH PAIR IS CLEARED AFTERWARDS, and finding
		 * that out cost a run: `GenericIIR`'s `m_i` and `m_acc` at
		 * +0x28..+0x34 are loop temporaries the constructor does not
		 * write (t_gtonedet.cpp says the same), the allocator hands
		 * back a recycled block, and the block one side gets was last
		 * written by OUR `process` while the other's was last written
		 * by the BLOB's.  The stale difference then surfaced on the
		 * one state that does not drive the detector at all -- eight
		 * failures out of 1.59 million, every one of them the fixture
		 * and not the method.
		 */
		memset(ansam_[side], 0, sizeof(ansam_[side]));
		new ((void *)ansam_[side]) ANSamToneDetector(
		    ANSAM_A1, ANSAM_A2, ANSAM_THRESH, ANSAM_FLAG, ANSAM_RATIO,
		    ANSAM_RATE, ANSAM_BLOCK, ANSAM_SIZE);
		memset((unsigned char *)ansam[side].filter + 0x28, 0,
		       sizeof(GenericIIR<float, double>) - 0x28);
		slot[side].o.ansamToneDetector = &ansam[side];

		/*
		 * ON ODD TRIALS THE DETECTOR IS MADE SENSITIVE.  With the
		 * constructor's own configuration -- 400 samples of history in
		 * blocks of 50, a threshold of 307200 -- `process` never once
		 * answers yes over a sweep this length, and six of the eight
		 * states that call it only branch when it does.  Shrinking the
		 * block and dropping the two thresholds is not a claim about
		 * the detector; it is what makes those six arms reachable at
		 * all, and t_gtonedet.cpp owns the question of what the
		 * configuration should be.
		 */
		if (trial & 1) {
			ansam[side].blockLen = 1;
			ansam[side].blocks1 = 1;
			ansam[side].blocks2 = 3;
			ansam[side].threshold = 0.0f;
			ansam[side].ratio = 0.0f;
		}

		/*
		 * Our own `reset`, on both sides.  It is verified against the
		 * blob by t_v90p3dreset.cpp, and using it on both sides means
		 * this test is measuring `getV90Decision` and nothing else.
		 */
		slot[side].o.reset(law, ucode,
				   (Phase3DemodulatorState)0, 0,
				   &jdo[side], NULL, &dilo[side],
				   (short)(trial & 1), (short)trial, 0.25f,
				   word14);

		fill_tables(side, trial);
	}
}

static void
teardown(void)
{
	int side;

	for (side = 0; side < 2; side++)
		ansam[side].~ANSamToneDetector();
}

/*
 * Everything the method can reach, per side, with the pointers each side owns
 * replaced by something the two sides can agree about.  Two static arrays at
 * two addresses never compare equal and never will.
 */
static void
snap(unsigned char *dst, int side)
{
	V90Phase3Demodulator *s = (V90Phase3Demodulator *)dst;
	V90Phase3Demodulator *l = &slot[side].o;

	memcpy(dst, slot[side].raw, SLOT);

	s->autoDigitalImpDetector = (V90AutoDigitalImpDetector *)(long)
	    (l->autoDigitalImpDetector == &adid[side]);
	s->sdDetector = (V90SdDetector *)(long)(l->sdDetector == &sdd[side]);
	s->params = (V90Parameters *)(long)(l->params == &parm[side]);
	s->ansamToneDetector = (ANSamToneDetector *)(long)
	    (l->ansamToneDetector == &ansam[side]);
	s->dil = (tagV90DILdescriptor *)(long)
	    (l->dil == NULL ? 2 : (l->dil == &dilo[side]));
	s->jd = (V90Jd *)(long)(l->jd == NULL ? 2 : (l->jd == &jdo[side]));
	s->jdV92 = (V92Jd *)(long)(l->jdV92 == NULL);

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
 * The tone detector's own storage: 0x3c of object, a 52-byte filter it owns,
 * and the filter's four arrays.  The lengths come out of the filter rather
 * than being asserted here, because `t_gtonedet.cpp` is what owns the question
 * of what the constructor should have built.
 */
/*
 * `GenericIIR`'s members are private and this file is not its friend, so the
 * four pointers and four lengths are read through a view of the same layout.
 * `GenericIIR.h` carries the offsets; only the ones needed to size an array
 * are named here, so `m_acc` -- a `double` at +0x2c, which no C++ struct can
 * reproduce without packing -- is left out rather than misdeclared.
 */
struct iir_view {
	double		*m_den;		/* +0x00 */
	double		*m_num;		/* +0x04 */
	double		*m_inHist;	/* +0x08 */
	double		*m_outHist;	/* +0x0c */
	unsigned int	m_nden;		/* +0x10 */
	unsigned int	m_nnum;		/* +0x14 */
	unsigned int	m_inLen;	/* +0x18 */
	unsigned int	m_outLen;	/* +0x1c */
	unsigned int	m_inPos;	/* +0x20 */
	unsigned int	m_outPos;	/* +0x24 */
	unsigned int	m_i;		/* +0x28 */
};

static void
compare_ansam(const char *what, long tag)
{
	static unsigned char ca[sizeof(ANSamToneDetector)];
	static unsigned char cb[sizeof(ANSamToneDetector)];
	static unsigned char fa[sizeof(GenericIIR<float, double>)];
	static unsigned char fb[sizeof(GenericIIR<float, double>)];
	void *pa = ansam[0].filter;
	void *pb = ansam[1].filter;
	struct iir_view *va = (struct iir_view *)pa;
	struct iir_view *vb = (struct iir_view *)pb;

	memcpy(ca, &ansam[0], sizeof(ca));
	memcpy(cb, &ansam[1], sizeof(cb));
	((GenericToneDetector *)ca)->filter = 0;
	((GenericToneDetector *)cb)->filter = 0;
	diff_eq_obj_(__FILE__, __LINE__, what, "ANSamToneDetector",
		     ca, cb, sizeof(ca), tag);

	memcpy(fa, pa, sizeof(fa));
	memcpy(fb, pb, sizeof(fb));
	memset(fa, 0, 4 * sizeof(double *));
	memset(fb, 0, 4 * sizeof(double *));
	/*
	 * `m_i` AT +0x28 IS EXCLUDED, AND THE EXCLUSION IS AN ASSERTION.
	 * Finding 1250: the blob's `GenericIIR::reset` keeps its loop counter
	 * in the member and leaves it holding `m_outLen`, where our
	 * reconstruction uses a local and leaves the member alone.  That is a
	 * divergence in `_ZN10GenericIIRIfdE5resetEv`, not in the method under
	 * test -- t_gtonedet.cpp's `compare_filters_` excludes the same word
	 * for the same reason -- and it reaches this test only because state
	 * 0x1c calls `ANSamToneDetector::reset()`.  Every other byte of the
	 * filter, `m_acc` included, is still compared, and the check below
	 * fails the moment `m_i` takes a value the finding does not predict.
	 */
	diff_eq_int("the filter's m_i is ours, or the blob's m_outLen (%ld)",
		    va->m_i == vb->m_i || vb->m_i == vb->m_outLen, 1, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "ANSam GenericIIR up to m_i",
		     fa, fb, 0x28, tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "ANSam GenericIIR past m_i",
		     fa + 0x2c, fb + 0x2c, sizeof(fa) - 0x2c, tag);

	diff_eq_obj_(__FILE__, __LINE__, what, "ANSam filter input history",
		     va->m_inHist, vb->m_inHist,
		     va->m_inLen * sizeof(double), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "ANSam filter output history",
		     va->m_outHist, vb->m_outHist,
		     va->m_outLen * sizeof(double), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "ANSam filter denominator",
		     va->m_den, vb->m_den, va->m_nden * sizeof(double), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "ANSam filter numerator",
		     va->m_num, vb->m_num, va->m_nnum * sizeof(double), tag);
}

static void
compare_all(const char *what, long tag)
{
	static unsigned char a[SLOT], b[SLOT];
	static unsigned char pa[sizeof(V90Parameters)];
	static unsigned char pb[sizeof(V90Parameters)];

	snap(a, 0);
	snap(b, 1);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Phase3Demodulator slot",
		     a, b, SLOT, tag);
	/*
	 * The detector's only pointer is its own parameter block at +0x2814,
	 * and each side has its own.  Dropped for the comparison and put back,
	 * which is t_gtonedet.cpp's device for the filter pointer.
	 */
	{
		V90Parameters *ka = adid[0].params;
		V90Parameters *kb = adid[1].params;

		diff_eq_int("the detector still points at its own block (%ld)",
			    (ka == &parm[0]) && (kb == &parm[1]), 1, tag);
		adid[0].params = 0;
		adid[1].params = 0;
		diff_eq_obj_(__FILE__, __LINE__, what,
			     "V90AutoDigitalImpDetector",
			     &adid[0], &adid[1], sizeof(adid[0]), tag);
		adid[0].params = ka;
		adid[1].params = kb;
	}
	diff_eq_obj_(__FILE__, __LINE__, what, "V90SdDetector",
		     &sdd[0], &sdd[1], (size_t)__builtin_offsetof(
			 V90SdDetector, history), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90SdDetector history",
		     sdhist[0], sdhist[1], sizeof(sdhist[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Jd",
		     &jdo[0], &jdo[1], sizeof(jdo[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "tagV90DILdescriptor",
		     &dilo[0], &dilo[1], sizeof(dilo[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "descrambler buffer",
		     dbuf[0], dbuf[1], sizeof(dbuf[0]), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "scrambler buffer",
		     sbuf[0], sbuf[1], sizeof(sbuf[0]), tag);

	/* The parameter block, with each side's own two pointers removed. */
	memcpy(pa, &parm[0], sizeof(pa));
	memcpy(pb, &parm[1], sizeof(pb));
	((V90Parameters *)pa)->modemParams = 0;
	((V90Parameters *)pb)->modemParams = 0;
	diff_eq_obj_(__FILE__, __LINE__, what, "V90Parameters",
		     pa, pb, sizeof(pa), tag);
	diff_eq_obj_(__FILE__, __LINE__, what, "_tagModemParameters",
		     &mparm[0], &mparm[1], sizeof(mparm[0]), tag);

	compare_ansam(what, tag);

	diff_eq_int("transcript line count (%ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), tag);
	diff_eq_int("transcript text (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
}

/*
 * Whether the state dispatches to the default block, where the returned value
 * has no reaching definition.
 */
static int
is_default_state(int st)
{
	return st == 7 || st == 8 || st == 0x12 || st > 0x21;
}

/*
 * The dirtying pass: after `reset` every flag this method branches on is zero,
 * and a machine whose flags are all zero exercises one side of nine `if`s and
 * neither side of the rest.
 */
/*
 * The run-length state 9 counts JdNot symbols with.  Chosen so that the value
 * the method TESTS -- this plus one, on a symbol that came back zero -- lands
 * on 12 exactly, which is the only value that separates `> 0xb` from `> 0xc`.
 */
static const unsigned int w404_v[8] = { 0, 5, 10, 11, 12, 13, 15, 20 };

static void
dirty(int side, int trial, int st, unsigned int word2c)
{
	V90Phase3Demodulator *o = &slot[side].o;
	unsigned int i;

	o->state = (Phase3DemodulatorState)st;
	o->word_2c = word2c;
	o->word_04 = (unsigned int)(trial % 6);
	o->byte_3f9 = (unsigned char)(trial & 1);
	o->short_400 = (short)((trial >> 1) & 1);
	o->word_410 = (unsigned int)((trial >> 2) & 1);
	o->word_3fc = 14;
	/*
	 * 0, 3, 6, 9, 12, 15, 18, 21 over the eight trials, which straddles
	 * the `word_404 > 0xb` test in state 9 and lands exactly on 12 so that
	 * a bound of 0xc rather than 0xb is a different answer.
	 */
	o->word_404 = w404_v[trial & 7];
	o->word_408 = (unsigned int)(trial % 3);
	/*
	 * NON-ZERO, so that a state which forgets to clear the event code is a
	 * different object afterwards.  The default block's only statement is
	 * that clear.
	 */
	o->word_30 = (unsigned int)(0x5a + trial);
	/*
	 * 0, 5, 10, 2, 7, 12, 4, 9 -- reaches both the 10 the recovery flag
	 * arms on and the 0 it disarms on, and `reset` has just zeroed this.
	 */
	sdd[side].count = (unsigned int)((trial * 5) % 13);
	o->byte_424 = (unsigned char)((trial >> 3) & 1);
	o->word_420 = 5;
	/*
	 * SMALL, so that state 0x11's "Probing DIL ended" arm is inside the
	 * `word_2c` sweep at all.  `reset` computes this from the DIL
	 * descriptor and gets a number in the thousands, which no sweep this
	 * length reaches -- the same reason the parameter thresholds are set
	 * rather than left at their defaults.
	 */
	o->dilLength = 7;
	o->word_3f4 = 0;
	o->verificationStatus = (unsigned int)(trial & 1);

	adid[side].short_a948 = (short)((trial >> 1) & 1);
	for (i = 0; i < V90ADID_PHASES; i++) {
		adid[side].short_2800[i] = (short)((trial + (int)i) & 1);
		adid[side].byte_280c[i] =
		    (unsigned char)((trial + (int)i + 1) & 1);
	}

	o->phase3Modulator.usingSegmentLevel = (short)((trial >> 2) & 1);
	o->phase3Modulator.segmentPos = (unsigned int)(trial & 1);
	o->phase3Modulator.dilPcmCode = (unsigned char)((trial * 17) & 0x7f);
	o->phase3Modulator.eventCode = (unsigned int)(trial % 7);
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
}

/*
 * `word_2c` is incremented before the dispatch, so a value of N-1 here is what
 * makes the method see N.  Everything below is one less than a threshold the
 * disassembly compares against, plus a short run of small values so that the
 * "no threshold reached" path is covered too.
 */
static const unsigned int w2c[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 20, 22,
	26, 0x2fu, 0x17fu, 0x2ffu, 0x7ceu, 0x7cfu, 0x7f7u, 0x9c3fu,
	11999u, 35999u, 38759u, 71u, 83u
};
#define NW2C ((int)(sizeof(w2c) / sizeof(w2c[0])))

static const float samples[] = {
	0.0f, 1.0f, -1.0f, 4000.0f, -4000.0f, 300.5f, -17.25f, 32000.0f
};
#define NSAMP ((int)(sizeof(samples) / sizeof(samples[0])))

#define NTRIAL	8

static int
run_states(void)
{
	int trial, st, w, k;
	int law;

	diff_begin("V90Phase3Demodulator::getV90Decision -- every state");

	for (trial = 0; trial < NTRIAL; trial++) {
		law = trial & 1;
		set_level((trial & 2) ? 2u : 0u);

		for (st = 0; st <= 0x25; st++) {
			for (w = 0; w < NW2C; w++) {
				long tag = ((long)trial << 24)
				    | ((long)st << 16) | (long)w;

				build(trial, (PcmType)law,
				      (unsigned char)(0x40 + trial),
				      (trial & 4) ? 0u : 12u);
				dsplib_debug_capture_reset();
				dirty(0, trial, st, w2c[w]);
				dirty(1, trial, st, w2c[w]);

				for (k = 0; k < NSAMP; k++) {
					float x = samples[(w + k) % NSAMP];
					short g, r;

					g = slot[0].o.getV90Decision(x);
					r = ref_p3d_getV90Decision(&slot[1].o,
								   x);
					if (!is_default_state(st))
						diff_eq_int(
						    "decision (%ld)",
						    (long)g, (long)r,
						    tag * 16 + k);
					compare_all("after getV90Decision",
						    tag * 16 + k);
				}
				teardown();
			}
		}
	}

	set_level(0);
	return diff_end();
}

/*
 * The two arms that are only reachable with a null Jd pointer: state 3's
 * "ERROR: Null JdDetector" and the state-2 store that has no such guard.
 */
static int
run_null_jd(void)
{
	int trial, st, k;

	diff_begin("V90Phase3Demodulator::getV90Decision -- null Jd");

	for (trial = 0; trial < 4; trial++) {
		set_level((trial & 1) ? 2u : 0u);

		for (st = 3; st <= 3; st++) {
			long tag = (long)trial << 8 | (long)st;

			build(trial, (PcmType)(trial & 1),
			      (unsigned char)(0x40 + trial), 0);
			dsplib_debug_capture_reset();
			dirty(0, trial, st, 0x7f7u);
			dirty(1, trial, st, 0x7f7u);
			slot[0].o.jd = NULL;
			slot[1].o.jd = NULL;

			for (k = 0; k < 3; k++) {
				short g = slot[0].o.getV90Decision(1000.0f);
				short r = ref_p3d_getV90Decision(&slot[1].o,
								 1000.0f);

				diff_eq_int("decision (%ld)", (long)g, (long)r,
					    tag * 16 + k);
				compare_all("after getV90Decision, null Jd",
					    tag * 16 + k);
			}
			teardown();
		}
	}

	set_level(0);
	return diff_end();
}

/*
 * A long free run from the opening state, which is the only way the machine
 * walks its own transitions rather than being placed in each state by hand.
 */
static int
run_free(void)
{
	int trial, k;

	diff_begin("V90Phase3Demodulator::getV90Decision -- free run");

	for (trial = 0; trial < 4; trial++) {
		set_level((trial & 1) ? 2u : 0u);
		build(trial, (PcmType)(trial & 1),
		      (unsigned char)(0x40 + trial), 0);
		dsplib_debug_capture_reset();
		dirty(0, trial, 0, 0);
		dirty(1, trial, 0, 0);

		for (k = 0; k < 4000; k++) {
			float x = samples[k % NSAMP]
			    * (float)((k % 7) - 3) * 0.37f;
			short g = slot[0].o.getV90Decision(x);
			short r = ref_p3d_getV90Decision(&slot[1].o, x);

			diff_eq_int("decision (%ld)", (long)g, (long)r,
				    (long)trial * 100000 + k);
			compare_all("after getV90Decision, free run",
				    (long)trial * 100000 + k);
		}
		teardown();
	}

	set_level(0);
	return diff_end();
}

/*
 * State 9's JdNot arm, driven on purpose.  Reaching it needs three things at
 * once -- a descrambled symbol of zero, `word_404` past its bound, and
 * `word_2c` at 12 modulo 72 -- and the first of those is whatever the
 * descrambler happens to produce, so the sweep above reaches it only by luck.
 * This walks the grid instead, and it is what tests the two states the arm
 * chooses between and the bound it compares against.
 */
static int
run_jdnot(void)
{
	static const unsigned int w2c_v[] = { 11u, 83u, 155u, 227u };
	static const unsigned int cnt_v[] = { 10u, 11u, 12u, 13u, 40u };
	int trial, w, c, k;

	diff_begin("V90Phase3Demodulator::getV90Decision -- state 9 JdNot");

	for (trial = 0; trial < 8; trial++) {
		set_level((trial & 4) ? 2u : 0u);

		for (w = 0; w < (int)(sizeof(w2c_v) / sizeof(w2c_v[0])); w++)
			for (c = 0;
			     c < (int)(sizeof(cnt_v) / sizeof(cnt_v[0])); c++) {
				long tag = ((long)trial << 16)
				    | ((long)w << 8) | (long)c;

				build(trial, (PcmType)(trial & 1),
				      (unsigned char)(0x40 + trial), 0);
				dsplib_debug_capture_reset();
				dirty(0, trial, 9, w2c_v[w]);
				dirty(1, trial, 9, w2c_v[w]);
				slot[0].o.word_404 = cnt_v[c];
				slot[1].o.word_404 = cnt_v[c];

				for (k = 0; k < NSAMP; k++) {
					float x = samples[(w + k) % NSAMP];
					short g, r;

					g = slot[0].o.getV90Decision(x);
					r = ref_p3d_getV90Decision(&slot[1].o,
								   x);
					diff_eq_int("decision (%ld)", (long)g,
						    (long)r, tag * 16 + k);
					compare_all("after getV90Decision, "
						    "JdNot grid", tag * 16 + k);
				}
				teardown();
			}
	}

	set_level(0);
	return diff_end();
}

/*
 * ===========================================================================
 * THE TEN SMALL MEMBERS.  They share this file's rig because they share its
 * object: `build` gives each side its own detector, Sd detector, tone
 * detector, Jd, DIL descriptor and parameter block, all seeded from varied
 * bytes and never zeroed (finding 230), and `compare_all` compares every one
 * of them plus the transcript after every call.
 *
 * WHAT THE OBJECT COMPARISON CANNOT SEE IS THE RETURN, and four of the ten
 * have one.  Each is compared explicitly below; `getMaxUcode`'s is a pointer
 * into each side's OWN detector, so it is compared as an offset from that
 * side's base -- two static arrays at two addresses never compare equal.
 * ===========================================================================
 */

/* The states `exitDIL` acts on, and a spread of the ones it must ignore. */
static const int exitdil_states[] = {
	0, 1, 5, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 26, 30, 40
};
#define N_EXITDIL ((int)(sizeof(exitdil_states) / sizeof(exitdil_states[0])))

/*
 * `resetJdNotDetector`, `getMaxUcode`, `setAltRbsParams` and
 * `incrementFramePosition` -- the four that are one statement each.
 */
static int
run_p3d_leaves(void)
{
	int trial, st, w;
	int sawwrap = 0, sawstep = 0, sawcleared = 0, sawcopied = 0;

	diff_begin("V90Phase3Demodulator: the four one-statement members");

	for (trial = 0; trial < NTRIAL; trial++) {
		set_level((trial & 2) ? 2u : 0u);

		for (st = 0; st < 6; st++)
			for (w = 0; w < 4; w++) {
				long tag = ((long)trial << 16)
				    | ((long)st << 8) | (long)w;
				unsigned char *ga, *gb;
				int side;

				build(trial, (PcmType)(trial & 1),
				      (unsigned char)(0x40 + trial), 0);
				dsplib_debug_capture_reset();
				dirty(0, trial, 4, w2c[w]);
				dirty(1, trial, 4, w2c[w]);

				/* The frame position under test, both sides. */
				for (side = 0; side < 2; side++) {
					slot[side].o.word_04 =
					    (unsigned int)st;
					slot[side].o.word_404 = 7u + (unsigned)w;
					parm[side].unnamed_440 =
					    3.5f + (float)trial;
					parm[side].
					  PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH
					    = -1.0f - (float)trial;
					adid[side].maxUcode[trial % 6] =
					    (unsigned char)(0x11 + trial);
				}

				slot[0].o.resetJdNotDetector();
				ref_p3d_resetJdNotDetector(&slot[1].o);
				compare_all("after resetJdNotDetector", tag);
				if (slot[1].o.word_404 == 0)
					sawcleared++;

				slot[0].o.setAltRbsParams();
				ref_p3d_setAltRbsParams(&slot[1].o);
				compare_all("after setAltRbsParams", tag);
				if (parm[1].
				      PHASE4_MEAN_ERROR_BEF_TO_AFT_UPDATE_RATIO_THRESH
				    == parm[1].unnamed_440)
					sawcopied++;

				ga = slot[0].o.getMaxUcode();
				gb = ref_p3d_getMaxUcode(&slot[1].o);
				diff_eq_int("getMaxUcode, as an offset into the"
					    " detector (%ld)",
					    (long)(ga - (unsigned char *)&adid[0]),
					    (long)(gb - (unsigned char *)&adid[1]),
					    tag);
				diff_eq_int("and it is the maxUcode array"
					    " (%ld)",
					    (long)(gb - (unsigned char *)&adid[1]),
					    (long)__builtin_offsetof(
						V90AutoDigitalImpDetector,
						maxUcode), tag);
				compare_all("after getMaxUcode", tag);

				slot[0].o.incrementFramePosition();
				ref_p3d_incrementFramePosition(&slot[1].o);
				compare_all("after incrementFramePosition", tag);
				if (st == 5) {
					if (slot[1].o.word_04 == 0)
						sawwrap++;
				} else if (slot[1].o.word_04 ==
					   (unsigned int)st + 1u) {
					sawstep++;
				}

				teardown();
			}
	}

	set_level(0);

	/*
	 * Anti-vacuity, and each counts trials whose OBJECT differed from
	 * another trial's rather than a branch believed taken (finding 3509).
	 */
	diff_eq_int("the frame position wrapped, and stepped (%ld)",
		    sawwrap > 0 && sawstep > 0, 1, 0);
	diff_eq_int("the JdNot counter was cleared (%ld)", sawcleared > 0, 1,
		    0);
	diff_eq_int("the alt-RBS parameter was copied (%ld)", sawcopied > 0, 1,
		    0);

	return diff_end();
}

/*
 * `enterWaitForANSpcmDrop` -- and the state it is already in, which is the
 * only input that separates the guard from an unconditional store.  Called
 * TWICE per trial: the second call must do nothing whatever the first did,
 * and it is the transcript that says so.
 */
static int
run_p3d_enterdrop(void)
{
	int trial, i;
	int sawentered = 0, sawalready = 0;

	diff_begin("V90Phase3Demodulator::enterWaitForANSpcmDrop");

	for (trial = 0; trial < NTRIAL; trial++) {
		set_level((trial & 1) ? 2u : 0u);

		for (i = 0; i < N_EXITDIL; i++) {
			long tag = ((long)trial << 16) | (long)i;
			unsigned lines;

			build(trial, (PcmType)(trial & 1),
			      (unsigned char)(0x40 + trial), 0);
			dsplib_debug_capture_reset();
			dirty(0, trial, exitdil_states[i], 13u);
			dirty(1, trial, exitdil_states[i], 13u);

			slot[0].o.enterWaitForANSpcmDrop();
			ref_p3d_enterWaitForANSpcmDrop(&slot[1].o);
			compare_all("after enterWaitForANSpcmDrop", tag);

			lines = dsplib_debug_capture_lines(1);
			if (exitdil_states[i] == 30) {
				if (slot[1].o.word_2c == 13u)
					sawalready++;
			} else if (slot[1].o.word_2c == 0
				   && (int)slot[1].o.state == 30) {
				sawentered++;
			}
			(void)lines;

			/* Again: idempotent, and silent the second time. */
			dsplib_debug_capture_reset();
			slot[0].o.enterWaitForANSpcmDrop();
			ref_p3d_enterWaitForANSpcmDrop(&slot[1].o);
			compare_all("after enterWaitForANSpcmDrop repeated",
				    tag);
			diff_eq_int("the repeat said nothing (%ld)",
				    (long)dsplib_debug_capture_lines(1), 0L,
				    tag);

			teardown();
		}
	}

	set_level(0);

	diff_eq_int("the state was entered, and was already held (%ld)",
		    sawentered > 0 && sawalready > 0, 1, 0);

	return diff_end();
}

/*
 * `exitDIL` -- seven states act and the rest do not, and the modulator's
 * event code decides whether the terminated transition follows.
 */
static int
run_p3d_exitdil(void)
{
	int trial, i, e;
	int sawacted = 0, sawignored = 0, sawterm = 0, sawnoterm = 0;

	diff_begin("V90Phase3Demodulator::exitDIL");

	for (trial = 0; trial < NTRIAL / 2; trial++) {
		set_level((trial & 1) ? 2u : 0u);

		for (i = 0; i < N_EXITDIL; i++)
			for (e = 0; e < 8; e++) {
				long tag = ((long)trial << 16)
				    | ((long)i << 8) | (long)e;
				int st = exitdil_states[i];
				int side;

				build(trial, (PcmType)(trial & 1),
				      (unsigned char)(0x40 + trial), 0);
				dsplib_debug_capture_reset();
				dirty(0, trial, st, 29u);
				dirty(1, trial, st, 29u);

				for (side = 0; side < 2; side++) {
					V90Phase3Modulator *m =
					    &slot[side].o.phase3Modulator;

					m->eventCode = (unsigned int)e;
					/*
					 * THE MODULATOR HAS TO BE IN ITS OWN
					 * DIL STATE OR ITS `exitDIL` RETURNS
					 * AT ITS FIRST LINE, and then neither
					 * "does not tell the modulator" nor
					 * the ordering of the two statements
					 * is observable -- both mutations read
					 * NOT CAUGHT, which is how this was
					 * found.  `reset` leaves it in
					 * TRN1D, so it is placed here: half
					 * the grid in DIL with a non-zero
					 * symbol count, which is what makes
					 * the modulator act, and the segment
					 * position decides which of its two
					 * arms -- only one of which sets the
					 * event code this method then reads.
					 */
					if ((e & 1) != 0) {
						m->state = P3M_STATE_DIL;
						m->symbolCount =
						    1u + (unsigned int)e;
						m->segmentPos =
						    (unsigned int)(e & 2);
					}
				}

				slot[0].o.exitDIL();
				ref_p3d_exitDIL(&slot[1].o);
				compare_all("after exitDIL", tag);

				if (st >= 10 && st <= 16) {
					sawacted++;
					if ((int)slot[1].o.state == 0x13)
						sawterm++;
					else
						sawnoterm++;
				} else if ((int)slot[1].o.state == st
					   && slot[1].o.word_2c == 29u) {
					sawignored++;
				}

				teardown();
			}
	}

	set_level(0);

	diff_eq_int("a DIL state acted, and a non-DIL state did not (%ld)",
		    sawacted > 0 && sawignored > 0, 1, 0);
	diff_eq_int("the terminated transition fired, and did not (%ld)",
		    sawterm > 0 && sawnoterm > 0, 1, 0);

	return diff_end();
}

/*
 * `JdNotDetector` -- the run length, and the one frame in seventy-two.
 *
 * `w404_v` above straddles the `> 0xb` bound and lands on 12 exactly, which
 * is what separates that bound from `> 0xc`; the `word_2c` values here
 * straddle the modulus, which is what separates 72 from anything else.
 */
static int
run_p3d_jdnotdetector(void)
{
	/*
	 * 48 AND 120 ARE THE ONES THAT SEPARATE 72 FROM 36.  Every other value
	 * here is 12 modulo 72, which is 12 modulo 36 as well, so a modulus of
	 * 36 answers the same on all of them -- the mutation read NOT CAUGHT
	 * until these two were added.
	 */
	static const unsigned int w2c_v[] = {
		0u, 11u, 12u, 13u, 48u, 71u, 72u, 83u, 84u, 120u, 143u, 155u,
		156u, 227u
	};
	static const int sym_v[] = { 0, 1, -1, 7 };
	int trial, w, c, k;
	int sawyes = 0, sawno = 0, sawinc = 0, sawclr = 0;

	diff_begin("V90Phase3Demodulator::JdNotDetector");

	for (trial = 0; trial < NTRIAL / 2; trial++) {
		set_level((trial & 1) ? 2u : 0u);

		for (w = 0; w < (int)(sizeof(w2c_v) / sizeof(w2c_v[0])); w++)
		    for (c = 0; c < 8; c++)
			for (k = 0;
			     k < (int)(sizeof(sym_v) / sizeof(sym_v[0])); k++) {
				long tag = ((long)trial << 24) | ((long)w << 16)
				    | ((long)c << 8) | (long)k;
				int side, ga, gb;
				unsigned int before;

				build(trial, (PcmType)(trial & 1),
				      (unsigned char)(0x40 + trial), 0);
				dsplib_debug_capture_reset();
				dirty(0, trial, 9, w2c_v[w]);
				dirty(1, trial, 9, w2c_v[w]);

				for (side = 0; side < 2; side++) {
					slot[side].o.word_404 = w404_v[c];
					slot[side].o.word_2c = w2c_v[w];
				}
				before = w404_v[c];

				ga = slot[0].o.JdNotDetector(sym_v[k]);
				gb = ref_p3d_JdNotDetector(&slot[1].o,
							   sym_v[k]);

				diff_eq_int("JdNotDetector (%ld)", (long)ga,
					    (long)gb, tag);
				compare_all("after JdNotDetector", tag);

				if (gb != 0)
					sawyes++;
				else
					sawno++;
				if (sym_v[k] == 0
				    && slot[1].o.word_404 == before + 1u)
					sawinc++;
				if (sym_v[k] != 0 && slot[1].o.word_404 == 0)
					sawclr++;

				teardown();
			}
	}

	set_level(0);

	diff_eq_int("it answered yes, and no (%ld)", sawyes > 0 && sawno > 0,
		    1, 0);
	diff_eq_int("the counter stepped, and was cleared (%ld)",
		    sawinc > 0 && sawclr > 0, 1, 0);

	return diff_end();
}

/*
 * `setDigitalImairmentsInfo` -- three calls on the detector, and the ORDER
 * matters: `applyPadGainToLinMapp` divides by the gain `findPadGain` stores,
 * and `determineMaxUcode`'s byte is what `findPadGain` starts from.  Every one
 * of those is inside the 43 KB detector that `compare_all` compares whole.
 *
 * The byte at +0x3f8 is swept, because it is this method's only input and
 * nothing else in the object reaches it.
 *
 * EVERY TRIAL IS PROBED FIRST, BECAUSE `findPadGain` DOES NOT ALWAYS RETURN.
 * D290: its opening search counts a BYTE down against `(int)byte_a954 - 8`
 * with a signed `jg`, so a `byte_a954` of 3..7 makes the bound negative and
 * the loop never ends.  That byte is whatever `determineMaxUcode` -- the call
 * immediately before -- happens to leave, so this method can be handed a
 * non-terminating input by its own first line, and this fixture found four
 * such inputs by hanging on them.
 *
 * So each trial runs OUR `determineMaxUcode` alone on a saved copy of the
 * detector, reads the byte, puts the copy back, and skips the trial if the
 * value is one of the five that do not terminate.  A hang is not a
 * differential result, and driving the object into a loop it cannot leave is
 * measuring D290 rather than this method (D561's ruling).  The skipped count
 * is reported, so the denominator is visible rather than implied.
 */
static int
run_p3d_setdigimp(void)
{
	static const unsigned char maxcode_v[] = { 0x40u, 0x7fu };
	static unsigned char probe_[sizeof(V90AutoDigitalImpDetector)];
	int trial, m;
	int sawmoved = 0, ran = 0, skipped = 0;

	diff_begin("V90Phase3Demodulator::setDigitalImairmentsInfo");

	/*
	 * LEVEL 0 THROUGHOUT, AND THAT IS A LIMIT WORTH STATING.  This method
	 * prints nothing of its own -- its 54 bytes are three calls and a tail
	 * jump -- so its whole transcript is `determineMaxUcode`'s and
	 * `findPadGain`'s, and those are t_v90adid's subject and its 20,407
	 * checks.  Driven from here at level 2 the two sides' REPORTS differ
	 * (69 lines against 43 on one trial, 195 against 106 on another) while
	 * the 43 KB detector they leave behind is identical byte for byte;
	 * that is a difference in those two functions' diagnostic paths at
	 * inputs t_v90adid does not reach, it is recorded in finding 4980 for
	 * whoever owns them, and it is not this method's to fix or to excuse.
	 * What IS this method's -- which three, in which order, with which
	 * argument -- is entirely in the object, which is compared whole.
	 */
	for (trial = 0; trial < 2; trial++) {
		set_level(0);

		for (m = 0;
		     m < (int)(sizeof(maxcode_v) / sizeof(maxcode_v[0])); m++) {
			long tag = ((long)trial << 16) | (long)m;
			unsigned char keep[V90ADID_PHASES];
			int side;
			unsigned int i;

			build(trial, (PcmType)(trial & 1),
			      (unsigned char)(0x40 + trial), 0);
			dsplib_debug_capture_reset();
			dirty(0, trial, 4, 5u);
			dirty(1, trial, 4, 5u);
			fill_tables(0, trial);
			fill_tables(1, trial);

			for (side = 0; side < 2; side++) {
				unsigned int ph, ci;

				slot[side].o.byte_3f8 = maxcode_v[m];
				adid[side].unSuspectedPhase =
				    (short)(trial % V90ADID_PHASES);
				adid[side].padGain = 1.0f;
				/*
				 * FINITE FLOATS, PLANTED, and this is not
				 * decoration.  `build` seeds the whole 43 KB
				 * detector from varied bytes and `reset` does
				 * not clear these three tables, so about one
				 * seeded word in 250 is a signalling NaN --
				 * the reason `set_params` above plants its two
				 * float parameters as well.  `determineMaxUcode`
				 * and `findPadGain` both reach a
				 * `v == 0.0f` test that the object compiles as
				 * ONE ordered `fcom` and GCC 13 cannot (finding
				 * 2304), so a NaN here makes the two builds
				 * keep different entries and print different
				 * reports.  Measured: 131 lines against 105
				 * before this was planted, objects identical,
				 * transcripts not.  That divergence belongs to
				 * V90AutoDigitalImpDetector and is already
				 * declared against t_v90adid; reaching it from
				 * here would be this fixture's NaN and not a
				 * property of the method under test.
				 */
				for (ph = 0; ph < V90ADID_PHASES; ph++)
					for (ci = 0; ci < V90ADID_CODES; ci++) {
						adid[side].float_9d48[ph][ci] =
						    1.0f + (float)((ci * 3u
							+ ph) % 17u);
						adid[side].float_1000[ph][ci] =
						    0.5f + (float)(ci % 11u);
						adid[side].float_9118[ph][ci] =
						    2.0f + (float)(ci % 13u);
					}
				adid[side].float_a970 = 5.0f;
				adid[side].float_a974 = 5.0f;
				adid[side].float_a97c = 1.0f;
				adid[side].float_a980 = 1.0f;
				adid[side].trn1Sigma = 1.0f;
				/*
				 * 0x30-ish, as t_v90adid's own findPadGain
				 * fixture uses.  The floor `determineMaxUcode`
				 * starts from decides the `byte_a954` it
				 * leaves, and a floor near zero leaves a byte
				 * near zero -- which is D290's non-terminating
				 * band and also a scan long enough to dominate
				 * this binary's run time.
				 */
				adid[side].short_a97a =
				    (short)(0x30 + (trial & 7));
				for (i = 0; i < V90ADID_PHASES; i++)
					adid[side].maxUcode[i] = 0u;
			}
			for (i = 0; i < V90ADID_PHASES; i++)
				keep[i] = adid[1].maxUcode[i];

			/* D290's five non-terminating inputs, probed for. */
			memcpy(probe_, &adid[0], sizeof(probe_));
			adid[0].determineMaxUcode((short)maxcode_v[m]);
			{
				unsigned char b = adid[0].byte_a954;

				memcpy(&adid[0], probe_, sizeof(probe_));
				if (b >= 3 && b <= 7) {
					skipped++;
					teardown();
					continue;
				}
			}
			ran++;

			slot[0].o.setDigitalImairmentsInfo();
			ref_p3d_setDigitalImairmentsInfo(&slot[1].o);

			compare_all("after setDigitalImairmentsInfo", tag);

			if (memcmp(keep, adid[1].maxUcode, sizeof(keep)) != 0)
				sawmoved++;

			teardown();
		}
	}

	set_level(0);

	diff_eq_int("the detector's maxUcode array moved (%ld)", sawmoved > 0,
		    1, 0);
	diff_eq_int("trials that ran (%ld)", ran > 0, 1, (long)ran);
	diff_eq_int("and the D290 skip is reported, not implied (%ld)",
		    ran + skipped,
		    2 * (int)(sizeof(maxcode_v) / sizeof(maxcode_v[0])), 0);

	return diff_end();
}

/*
 * `twoLevelDemod` -- one bit out through the reference argument, one level
 * back.
 *
 * THE -32768 TRIAL IS THE POINT OF THIS SUITE.  The object negates the level
 * and TRUNCATES the result to sixteen bits; written `-level` on an `int` the
 * answer for a mapping entry of -32768 is +32768 where the object's is
 * -32768, and the two agree over every other value a `short` table can hold.
 * So the extreme entry is planted, on both sides, for the whole grid.
 */
static int
run_p3d_twolevel(void)
{
	static const short level_v[] = {
		0, 1, -1, 1234, -1234, 32767, -32768, -32767
	};
	int trial, l, k, a;
	int sawpos = 0, sawneg = 0, sawalt = 0, sawmain = 0, sawextreme = 0;

	diff_begin("V90Phase3Demodulator::twoLevelDemod");

	for (trial = 0; trial < NTRIAL / 2; trial++) {
		set_level((trial & 1) ? 2u : 0u);

		for (l = 0; l < (int)(sizeof(level_v) / sizeof(level_v[0])); l++)
		    for (a = 0; a < 2; a++)
			for (k = 0; k < NSAMP; k++) {
				long tag = ((long)trial << 24) | ((long)l << 16)
				    | ((long)a << 8) | (long)k;
				unsigned char uc =
				    (unsigned char)(0x40 + trial);
				float x = samples[k];
				int side, ga, gb, ba, bb;

				build(trial, (PcmType)(trial & 1), uc, 0);
				dsplib_debug_capture_reset();
				dirty(0, trial, 4, 5u);
				dirty(1, trial, 4, 5u);
				fill_tables(0, trial);
				fill_tables(1, trial);

				for (side = 0; side < 2; side++) {
					unsigned int ph =
					    (unsigned int)(trial % 6);

					slot[side].o.word_04 = ph;
					/*
					 * The two tables differ at the entry
					 * the method reads, so which of them
					 * it read is visible in the answer.
					 */
					adid[side].linMapp[ph][uc] =
					    level_v[l];
					adid[side].linMappAlt[ph][uc] =
					    (short)(level_v[l] ^ 0x2a5);
					adid[side].short_a948 = (short)a;
					/*
					 * `isAltRbs` answers yes only when
					 * `short_2800[phase]` is set AND the
					 * sample is further from the main
					 * mapping entry than `short_a9a6`.
					 * A threshold of 0 on the arm that
					 * arms the flag is what makes the
					 * alternate table reachable at all --
					 * with a large one the counter below
					 * reads zero, which is how this was
					 * found.
					 */
					adid[side].short_2800[ph] = (short)a;
					adid[side].short_a9a6 =
					    (short)(a ? 0 : 0x4000);
				}

				ba = 0x5a5a5a;
				bb = 0x5a5a5a;
				ga = slot[0].o.twoLevelDemod(x, ba);
				gb = ref_p3d_twoLevelDemod(&slot[1].o, x, &bb);

				diff_eq_int("twoLevelDemod, the level (%ld)",
					    (long)ga, (long)gb, tag);
				diff_eq_int("twoLevelDemod, the bit (%ld)",
					    (long)ba, (long)bb, tag);
				compare_all("after twoLevelDemod", tag);

				if (x > 0.0f)
					sawpos++;
				else
					sawneg++;
				if (gb == (int)(short)(level_v[l] ^ 0x2a5)
				    || gb == -(int)(short)(level_v[l] ^ 0x2a5))
					sawalt++;
				if (gb == (int)level_v[l]
				    || gb == -(int)level_v[l])
					sawmain++;
				if (level_v[l] == -32768 && !(x > 0.0f))
					sawextreme++;

				teardown();
			}
	}

	set_level(0);

	diff_eq_int("both signs of the sample were driven (%ld)",
		    sawpos > 0 && sawneg > 0, 1, 0);
	diff_eq_int("both mapping tables were reached (%ld)",
		    sawalt > 0 && sawmain > 0, 1, 0);
	diff_eq_int("the -32768 entry was negated (%ld)", sawextreme > 0, 1, 0);

	return diff_end();
}

/*
 * `getDecision` -- the session flag picks the callee, and the callee's `short`
 * is widened.  Both callees are this file's other subject, so what is new here
 * is only the dispatch and the widening: a level that is negative as a `short`
 * must come back negative as an `int`.
 */
static int
run_p3d_getdecision(void)
{
	/*
	 * THE V.92 ARM NEEDS A Jd BLOCK.  `build` passes a null `V92Jd`,
	 * because every suite above it drives `getV90Decision` only, and
	 * `getV92Decision` walks the block its Jd states are about -- a null
	 * one faults.  Storage is provided here rather than in `build`, so
	 * that the four suites above keep exercising exactly what they did.
	 * `snap` maps this pointer to "is it null", which is false on both
	 * sides once it is set, so the blocks themselves are compared here.
	 */
	static unsigned char jd92_[2][sizeof(V92Jd)] __attribute__((aligned(8)));
	static const unsigned int flag_v[] = { 0u, 1u, 2u, 0xffffffffu };
	/*
	 * SIX STATES, NAMED, NOT A SWEEP.  What is new in `getDecision` is the
	 * dispatch and the widening; the state machines behind it are what
	 * `run_states` above sweeps, at 80,000 trials, and repeating that here
	 * would double this binary's run time for no claim it does not already
	 * make.  These six are one default-block state, one that returns a
	 * negative level, and four that move the object.
	 */
	static const int st_v[] = { 0, 4, 7, 9, 0x11, 0x1e };
	int trial, f, si, k, st;
	int sawv90 = 0, sawv92 = 0, sawneg = 0;

	diff_begin("V90Phase3Demodulator::getDecision");

	for (trial = 0; trial < NTRIAL / 2; trial++) {
		set_level((trial & 2) ? 2u : 0u);

		for (f = 0; f < (int)(sizeof(flag_v) / sizeof(flag_v[0])); f++)
		    for (si = 0; si < (int)(sizeof(st_v) / sizeof(st_v[0])); si++)
			for (k = 0; k < NSAMP; k += 2) {
				st = st_v[si];
				long tag = ((long)trial << 24) | ((long)f << 16)
				    | ((long)st << 8) | (long)k;
				float x = samples[k];
				int side, ga, gb;

				build(trial, (PcmType)(trial & 1),
				      (unsigned char)(0x40 + trial), 0);
				dsplib_debug_capture_reset();
				dirty(0, trial, st, w2c[k % NW2C]);
				dirty(1, trial, st, w2c[k % NW2C]);
				fill_tables(0, trial);
				fill_tables(1, trial);

				for (side = 0; side < 2; side++) {
					unsigned int b;

					slot[side].o.sessionFlag = flag_v[f];
					for (b = 0; b < sizeof(jd92_[0]); b++)
						jd92_[side][b] =
						    (unsigned char)(b * 7u + 1u
						        + (unsigned)trial);
					slot[side].o.jdV92 = (V92Jd *)jd92_[side];
				}

				ga = slot[0].o.getDecision(x);
				gb = ref_p3d_getDecision(&slot[1].o, x);
				diff_eq_obj_(__FILE__, __LINE__,
					     "after getDecision", "V92Jd",
					     jd92_[0], jd92_[1],
					     sizeof(jd92_[0]), tag);

				if (!is_default_state(st))
					diff_eq_int("getDecision (%ld)",
						    (long)ga, (long)gb, tag);
				compare_all("after getDecision", tag);

				if (!is_default_state(st)) {
					if (flag_v[f] == 0u)
						sawv90++;
					else
						sawv92++;
					if (gb < 0)
						sawneg++;
				}

				teardown();
			}
	}

	set_level(0);

	diff_eq_int("both callees were dispatched to (%ld)",
		    sawv90 > 0 && sawv92 > 0, 1, 0);
	diff_eq_int("a negative decision was widened (%ld)", sawneg > 0, 1, 0);

	return diff_end();
}

int
main(void)
{
	int bad = 0;

	dsplib_debug_capture_on = 1;

	bad |= run_states();
	bad |= run_jdnot();
	bad |= run_null_jd();
	bad |= run_free();
	bad |= run_p3d_leaves();
	bad |= run_p3d_enterdrop();
	bad |= run_p3d_exitdil();
	bad |= run_p3d_jdnotdetector();
	bad |= run_p3d_twolevel();
	bad |= run_p3d_getdecision();
	bad |= run_p3d_setdigimp();

	return bad;
}
