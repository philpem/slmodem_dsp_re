/*
 * t_v34pcmapi.cpp -- the MANGLED half of the V.34 datapump's accessor
 * surface, plus the two `VPcmFloModem` members that pick the V.34 symbol
 * rates a session may settle on.
 *
 * WHY THIS FILE IS C++.  Five of the seven entry points take
 * `tagV34Object *` rather than `void *`, which is the whole of why the
 * object exports them mangled, and a `.c` cannot name a mangled symbol at
 * all -- so the blob side is reached through `asm("ref__Z...")` aliases, the
 * same idiom t_vpcmflomodem.cpp uses for its five.  `VPcmV34InitMOH` is not
 * mangled and is here anyway: it wants exactly the fixture the other five
 * want, and building a second copy of an 0x8000-byte session in a second
 * file would prove nothing the first one does not.
 *
 * SEVEN BLOCKS PER SIDE, NOT ONE.  These reach out of the V.34 object
 * through three pointers and two of those lead on further:
 *
 *     obj +0x3548 -> session   +0x175c -> demodulator +0x208 -> designer
 *                              +0x610c -> pcm receiver
 *                              +0x6bd0    an EMBEDDED V92EchoCanceller
 *                              +0x6f5c    an EMBEDDED GenericToneDetector
 *     obj +0xac18 -> k56flex modem
 *     obj +0xac3c -> configuration
 *
 * Each side gets its own copy of all of them, prefilled with the same varied
 * per-offset pattern rather than a constant (finding F230), and every block is
 * compared to its opposite number afterwards.  A test that compared only the
 * V.34 object would pass with `V90ConstellationDesigner::setMinMaxRates`,
 * `K56FlexFloModem::setMinMaxRates`, `V92EchoCanceller::setEchoDelay` and
 * `GenericToneDetector::reset` all deleted: not one of the four writes
 * anything the V.34 object can see.
 *
 * TWO COMPARISONS, NOT ONE, AND THAT IS DELIBERATE.  Five of the six object
 * entry points write a handful of fields and call nothing that installs a
 * pointer, so they are compared with NO holes but the three the fixture owns
 * -- which is the strongest comparison this tree can make.  `VPcmV34InitMOH`
 * runs `v34handshakinit` mode 4 and therefore rewrites a large part of the
 * object through `v34modeminit`, installing pointers INTO the object that
 * necessarily differ between two objects at two addresses; it gets its own
 * hole list, and every entry of that list is asserted REACHED at the end, so
 * a hole that stops being earned fails rather than quietly widening the test.
 *
 * THE STATE WORDS ARE SEEDED IN RANGE ON EVERY CASE THAT REACHES THE
 * HANDSHAKE.  `v34handshakinit` mode 4 runs transitions that index
 * `StateName` with whatever they find, and nothing bounds the index (D42), so
 * a fill pattern there is an out-of-bounds read as soon as the level is up.
 * All three are seeded to DIFFERENT in-range values, because equal words make
 * the three machines indistinguishable.
 *
 * AND THE TWO `setV34BaudFor*` ARE COMPARED WHOLE AND THEN AGAINST EACH
 * OTHER.  They are byte-identical bar the last of six stores, so two
 * separately-passing whole-buffer comparisons do not state the thing that
 * makes them two functions; the difference is asserted to be at +0x21c and
 * nowhere else.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34pcm_tables.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"
#include "dsplib/VPcmFloModem.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

void ref_SetUpstreamModulationInfo(void *obj)
	asm("ref__Z25SetUpstreamModulationInfoP12tagV34Object");
void ref_VPcmV34SetMinMaxBitRates(void *obj)
	asm("ref__Z24VPcmV34SetMinMaxBitRatesP12tagV34Object");
void ref_VPcmV34SetMinimumSigLevel(void *obj)
	asm("ref__Z25VPcmV34SetMinimumSigLevelP12tagV34Object");
void ref_VPcmV34SetDelays(void *obj)
	asm("ref__Z16VPcmV34SetDelaysP12tagV34Object");
void ref_VPcmV34SetTimeOut(void *obj, int secs)
	asm("ref__Z17VPcmV34SetTimeOutP12tagV34Objecti");
void ref_setV34BaudForV90(void *self)
	asm("ref__ZN12VPcmFloModem16setV34BaudForV90Ev");
void ref_setV34BaudForV34(void *self)
	asm("ref__ZN12VPcmFloModem16setV34BaudForV34Ev");

/* These two are `extern "C"` in the object as well as here. */
void ref_VPcmV34InitMOH(void *obj, int message, unsigned char late,
			unsigned char flag);
void ref_V34InitializeImplementationSpecific(void *obj);

/*
 * `VPcmV34InitMOH` has no declaration in any header: it is the one member of
 * the mangled batch whose name is not mangled, and `v34pcmmain.cpp` defines
 * it `extern "C"` without one.  Declared here rather than added to a header,
 * because a header declaration is a claim about the interface and this file
 * only needs to call it.
 */
void VPcmV34InitMOH(void *obj, int message, unsigned char late,
		    unsigned char flag);
}

/* --- object offsets ------------------------------------------------------- */

#define OB_STATUS	0x0000
#define OB_F0004	0x0004
#define OB_RATE_MIN	0x0220
#define OB_RATE_MAX	0x0224
#define OB_F0234	0x0234
#define OB_ENERGY_FLOOR	0x0230
#define OB_V90RX	0x024c
#define OB_K56RX	0x0250
#define OB_DMADELAY	0x025c
#define OB_SAMPLE_CNT	0x0238
#define OB_TIMEOUT	0x023c
#define OB_RX_F258	(0x0264 + 0x258)
#define OB_RX_F25A	(0x0264 + 0x25a)
#define OB_RX_F25C	(0x0264 + 0x25c)
#define OB_F2218	0x2218
#define OB_P3548	0x3548
#define OB_MICROSTATE	0x3592
#define OB_RXSTATE	0x3594
#define OB_TXSTATE	0x3596
#define OB_F359A	0x359a
#define OB_F359C	0x359c
#define OB_FILT_DELAY	0xaa7c
#define OB_MOH_LIMIT	0xabd8
#define OB_MOH_TIMER	0xabdc
#define OB_FABE0	0xabe0
#define OB_FABE2	0xabe2
#define OB_MOH_W4	0xabe4
#define OB_MOH_W6	0xabe6
#define OB_ANSAMLATE	0xabe9
#define OB_FABEC	0xabec
#define OB_MOH_MESSAGE	0xabf0
#define OB_MOH_RECVD	0xabf4
#define OB_MOH_FLAG	0xabf9
#define OB_FABFA	0xabfa
#define OB_PAC18	0xac18
#define OB_FAC1C	0xac1c
#define OB_PAC3C	0xac3c

/* --- session, configuration and K56flex offsets --------------------------- */

#define SS_DEMOD	0x175c
#define SS_PCM		0x610c
#define SS_SIDE		0x6114
#define SS_GATE		0x6120
#define SS_V92P2	0x612c		/* a V92Phase2Info, mode 4 walks it */
#define SS_TONE_FILTER	(SS_TONE + 0x00)	/* the detector's own IIR   */
#define SS_ECHO		0x6bd0		/* an embedded V92EchoCanceller  */
#define SS_TONE		0x6f5c		/* an embedded GenericToneDetector */

#define DM_DESIGNER	0x0208
#define PCM_SENS	0x04f8
#define K56_GATE	0x0008

#define CF_FLAGS3	0x03
#define CF_MIN_RATE	0x30
#define CF_MAX_RATE	0x34
#define CF_MIN_LEVEL	0x60
#define CF_FILT_DELAY	0x64
#define CF_EXT_DELAY	0x68

/*
 * The two members of the embedded `V92EchoCanceller` `setEchoDelay` touches,
 * as offsets from the canceller rather than as names: this file compares the
 * whole session, and these two are only used for the hand calculation.
 */
#define EC_LENGTH	(SS_ECHO + 0x2c)
#define EC_DELAY	(SS_ECHO + 0x38)

/* And the designer's two, for the same reason. */
#define CD_MAX_RATE	0x4c
#define CD_MIN_RATE	0x50

/*
 * The `GenericIIR<float, double>` the tone detector owns.  `reset()` calls
 * `filter->reset()` FIRST, so the pointer at the detector's +0x00 has to lead
 * somewhere legal or the fixture faults instead of reporting -- which is the
 * defect `run_setupreceiver` had and t_v34pcmif.c's note is about.  The two
 * history buffers are separate blocks from the filter for the reason that
 * file's rate chain gives: folding them in would make the indirection
 * invisible.
 */
#define IIR_INHIST	0x08
#define IIR_OUTHIST	0x0c
#define IIR_NDEN	0x10
#define IIR_NNUM	0x14
#define IIR_INLEN	0x18
#define IIR_OUTLEN	0x1c
#define IIR_INPOS	0x20
#define IIR_OUTPOS	0x24

#define IIR_HIST_LEN	8
#define IIR_NUM_TAPS	4
#define IIR_DEN_TAPS	3

/* The eight scalars `GenericToneDetector::reset` clears, by offset. */
#define GTD_ACC_0C	0x0c
#define GTD_ACC_10	0x10
#define GTD_ACC_14	0x14
#define GTD_ACC_18	0x18
#define GTD_SAMPLES	0x24
#define GTD_COUNT_2C	0x2c
#define GTD_COUNT_30	0x30
#define GTD_DETECTED	0x38

/* --- the blocks ----------------------------------------------------------- */

/*
 * Lengths are generous rather than measured: the point of comparing them is
 * to catch a store LANDING OUTSIDE the field the function is about, which a
 * block sized to the last known field could not do.  The session runs past
 * +0x6f5c because that is where the tone detector `VPcmV34InitMOH` resets
 * lives, and past +0x6bd0 for the echo canceller `VPcmV34SetDelays` reaches.
 */
#define SESS_LEN	0x8000
#define PCMRX_LEN	0x0600
#define DEMOD_LEN	0x0400
#define DESIGNER_LEN	0x0080
#define V92P2_LEN	0x0200
#define CFG_LEN		0x0100
#define K56_LEN		0x0040

struct side {
	unsigned int	align;
	unsigned char	sess[SESS_LEN];
	unsigned char	pcmrx[PCMRX_LEN];
	unsigned char	demod[DEMOD_LEN];
	unsigned char	designer[DESIGNER_LEN];
	unsigned char	v92p2[V92P2_LEN];
	unsigned char	iir[0x40];
	double		inhist[IIR_HIST_LEN];
	double		outhist[IIR_HIST_LEN];
	unsigned char	cfg[CFG_LEN];
	unsigned char	k56[K56_LEN];
};

static struct v34_object oa;
static unsigned char ob[sizeof(struct v34_object)];
static struct side sa, sb;

/*
 * The three the FIXTURE owns.  Nothing under test writes any of them, and
 * what justifies the holes is that the memory behind each is compared per
 * case.
 */
static const unsigned fixture_ptr[] = { OB_P3548, OB_PAC18, OB_PAC3C };
#define NFIXPTR (sizeof(fixture_ptr) / sizeof(fixture_ptr[0]))

/*
 * And the ones `VPcmV34InitMOH` reaches through `v34handshakinit` mode 4 and
 * `V34InitializeImplementationSpecific`.  Every one is a pointer INTO the
 * object, so the two sides necessarily hold different addresses; every one is
 * asserted reached at the end of the run.
 */
static const unsigned moh_ptr[] = {
	0x0268, 0x026c,			/* rxq read and write cursors    */
	0x0394,				/* receiver +0x130 rx_samples    */
	0x0418,				/* receiver +0x1b4 carrier       */
	0x0508,				/* receiver +0x2a4 f2a4          */
	/*
	 * NOT the timing filters' two coefficient pointers at +0x620 and
	 * +0x624: `V34TimingFiltersInit` installs those and only
	 * `v34handshakinit` MODE 0 calls it, so mode 4 leaves them at the
	 * fill on both sides and they compare.  They were in this list until
	 * the run said they had never been written, which is what the
	 * "pointer field was installed" assertion below is for.
	 */
	0x0a28, 0x0e48,			/* receive shell context         */
	0x1460,				/* modulator +0x10 sine          */
	0x2074,				/* modulator +0xc24 shaped       */
	0x20cc,				/* modulator +0xc7c ec_prem      */
	0x2100,				/* modulator +0xcb0 preemp       */
	0x2220, 0x2224,			/* txq cursors                   */
	0x2608, 0x2a28,			/* transmit shell context        */
	0x3564,				/* detector +0x00 coeff          */
	0xaa6c, 0xaa70,			/* the two message records       */
	0x80b8, 0x80bc, 0x80c0, 0x80c4, 0x80c8,	/* echo canceller 0      */
	0x9138, 0x913c, 0x9140, 0x9144, 0x9148	/* and 1                 */
};
#define NMOHPTR (sizeof(moh_ptr) / sizeof(moh_ptr[0]))

static int saw_moh_ptr[NMOHPTR];

/* The session's two outgoing pointers, and the demodulator's one. */
static const unsigned sess_ptr[] = {
	SS_DEMOD, SS_PCM, SS_V92P2, SS_TONE_FILTER
};
#define NSESSPTR (sizeof(sess_ptr) / sizeof(sess_ptr[0]))

static const unsigned demod_ptr[] = { DM_DESIGNER };
#define NDEMODPTR (sizeof(demod_ptr) / sizeof(demod_ptr[0]))

/* The filter's two history pointers, which are two blocks per side. */
static const unsigned iir_ptr[] = { IIR_INHIST, IIR_OUTHIST };
#define NIIRPTR (sizeof(iir_ptr) / sizeof(iir_ptr[0]))

static int
in_holes(unsigned off, const unsigned *holes, unsigned n)
{
	unsigned k;

	for (k = 0; k < n; k++)
		if (off >= holes[k] && off < holes[k] + 4)
			return 1;
	return 0;
}

/* --- poking and peeking --------------------------------------------------- */

static void
poke_ptr(unsigned off, void *pa, void *pb)
{
	memcpy((unsigned char *)&oa + off, &pa, sizeof(pa));
	memcpy(ob + off, &pb, sizeof(pb));
}

static void
poke_int(unsigned off, int v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

static void
poke_short(unsigned off, short v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

static void
poke_byte(unsigned off, unsigned char v)
{
	*((unsigned char *)&oa + off) = v;
	ob[off] = v;
}

static int
get_int_a(unsigned off)
{
	int v;

	memcpy(&v, (const unsigned char *)&oa + off, sizeof(v));
	return v;
}

static short
get_short_a(unsigned off)
{
	short v;

	memcpy(&v, (const unsigned char *)&oa + off, sizeof(v));
	return v;
}

static unsigned char
get_byte_a(unsigned off)
{
	return *((const unsigned char *)&oa + off);
}

static int
get_int_sess(unsigned off)
{
	int v;

	memcpy(&v, sa.sess + off, sizeof(v));
	return v;
}

static int
get_int_iir(unsigned off)
{
	int v;

	memcpy(&v, sa.iir + off, sizeof(v));
	return v;
}

/* Fill a side's blocks with a varied, per-offset pattern.  Never zeros. */
static void
fill_side(struct side *s)
{
	unsigned char *p = (unsigned char *)s;
	unsigned i;

	for (i = 0; i < sizeof(*s); i++)
		p[i] = (unsigned char)(0x31u + i * 7u + (i >> 5));
}

static void
setup(void)
{
	memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
	memset(ob, HARNESS_MALLOC_FILL, sizeof(ob));
	fill_side(&sa);
	fill_side(&sb);
}

/* --- comparing ------------------------------------------------------------ */

static void
compare_block(const char *what, const unsigned char *pa,
	      const unsigned char *pb, unsigned len, const unsigned *holes,
	      unsigned nholes, long tag)
{
	unsigned i;
	int bad = 0;

	for (i = 0; i < len; i++) {
		if (pa[i] == pb[i] || in_holes(i, holes, nholes))
			continue;
		bad++;
		if (bad <= 6) {
			char msg[160];

			snprintf(msg, sizeof(msg),
				 "%s: byte at +0x%x (case %ld)", what, i, tag);
			diff_eq_int(msg, pa[i], pb[i], (long)i);
		}
	}
	diff_eq_int(what, bad, 0, tag);
}

/* Every block but the V.34 object itself. */
static void
compare_sides(const char *what, long tag)
{
	char msg[128];

	snprintf(msg, sizeof(msg), "%s: session", what);
	compare_block(msg, sa.sess, sb.sess, SESS_LEN, sess_ptr, NSESSPTR, tag);
	snprintf(msg, sizeof(msg), "%s: pcm receiver", what);
	compare_block(msg, sa.pcmrx, sb.pcmrx, PCMRX_LEN, 0, 0, tag);
	snprintf(msg, sizeof(msg), "%s: demodulator", what);
	compare_block(msg, sa.demod, sb.demod, DEMOD_LEN, demod_ptr,
		      NDEMODPTR, tag);
	snprintf(msg, sizeof(msg), "%s: constellation designer", what);
	compare_block(msg, sa.designer, sb.designer, DESIGNER_LEN, 0, 0, tag);
	snprintf(msg, sizeof(msg), "%s: v92 phase 2", what);
	compare_block(msg, sa.v92p2, sb.v92p2, V92P2_LEN, 0, 0, tag);
	snprintf(msg, sizeof(msg), "%s: tone detector filter", what);
	compare_block(msg, sa.iir, sb.iir, sizeof(sa.iir), iir_ptr, NIIRPTR,
		      tag);
	snprintf(msg, sizeof(msg), "%s: filter input history", what);
	compare_block(msg, (const unsigned char *)sa.inhist,
		      (const unsigned char *)sb.inhist, sizeof(sa.inhist),
		      0, 0, tag);
	snprintf(msg, sizeof(msg), "%s: filter output history", what);
	compare_block(msg, (const unsigned char *)sa.outhist,
		      (const unsigned char *)sb.outhist, sizeof(sa.outhist),
		      0, 0, tag);
	snprintf(msg, sizeof(msg), "%s: configuration", what);
	compare_block(msg, sa.cfg, sb.cfg, CFG_LEN, 0, 0, tag);
	snprintf(msg, sizeof(msg), "%s: k56flex", what);
	compare_block(msg, sa.k56, sb.k56, K56_LEN, 0, 0, tag);
}

/*
 * THE STRONG COMPARISON: the whole object with only the three fixture
 * pointers held out.  Five of the six entry points install nothing, so any
 * other difference is a defect and not an address.
 */
static void
compare_plain(const char *what, long tag)
{
	compare_block(what, (const unsigned char *)&oa, ob, sizeof(oa),
		      fixture_ptr, NFIXPTR, tag);
	compare_sides(what, tag);
}

/* And the wide one, for the case that runs the handshake. */
static void
compare_moh(const char *what, long tag)
{
	unsigned all[NFIXPTR + NMOHPTR];
	unsigned k;

	for (k = 0; k < NFIXPTR; k++)
		all[k] = fixture_ptr[k];
	for (k = 0; k < NMOHPTR; k++) {
		all[NFIXPTR + k] = moh_ptr[k];
		if (memcmp((const unsigned char *)&oa + moh_ptr[k],
			   ob + moh_ptr[k], 4) != 0)
			saw_moh_ptr[k] = 1;
	}

	compare_block(what, (const unsigned char *)&oa, ob, sizeof(oa),
		      all, NFIXPTR + NMOHPTR, tag);
	compare_sides(what, tag);
}

/* --- the case description ------------------------------------------------- */

/*
 * ONE FIELD PER INPUT.  Two inputs driven from one variable cannot be told
 * apart -- findings F116b, F123 and F171 -- and `VPcmV34SetMinMaxBitRates` has
 * four gating inputs whose conjunction is the whole point of it, so all four
 * are separate members here even where a case only varies one.
 */
struct api_case {
	int		v90rx;		/* obj +0x24c                       */
	int		k56rx;		/* obj +0x250                       */
	int		gate;		/* session +0x6120                  */
	unsigned char	k56gate;	/* k56flex +0x08                    */
	unsigned int	min_rate;	/* cfg +0x30, bits per second       */
	unsigned int	max_rate;	/* cfg +0x34                        */
	int		min_level;	/* cfg +0x60, biased by 0x30        */
	int		filt_delay;	/* cfg +0x64                        */
	int		ext_delay;	/* cfg +0x68                        */
	unsigned char	cfg_flags3;	/* cfg +0x03, bit 2 is the retrain  */
	int		status;		/* obj +0x00                        */
	short		f359c;		/* obj +0x359c, originate/answer    */
	int		mside;		/* session +0x6114                  */
	int		sens;		/* pcm receiver +0x4f8              */
	int		rate_min;	/* obj +0x220, seeded not zero      */
	int		rate_max;	/* obj +0x224                       */
	short		f359a;		/* obj +0x359a, seeded not zero     */
};

/*
 * The default.  `min_level` of -0x30 is index 0; the rates are 4800 and
 * 33600, which are indices 2 and 14; `mside` is 2 so that
 * `V90Modem::setSessionFlag` -- which the handshake reaches -- stops at the
 * modem rather than walking into a modulator this fixture does not model.
 */
static const struct api_case api_base = {
	0,			/* v90rx                                    */
	0,			/* k56rx                                    */
	0,			/* gate                                     */
	0,			/* k56gate                                  */
	4800u, 33600u,		/* min_rate, max_rate                       */
	-0x30,			/* min_level -> index 0                     */
	64, 200,		/* filt_delay, ext_delay                    */
	0xff,			/* cfg_flags3: every bit set                */
	3,			/* status                                   */
	0x65,			/* f359c                                    */
	2,			/* mside                                    */
	0,			/* sens                                     */
	0x1111, 0x2222,		/* rate_min, rate_max: distinct, non-zero   */
	0x3333			/* f359a                                    */
};

static void
seed_side(struct side *s, const struct api_case *c)
{
	void *p;

	p = s->demod;
	memcpy(s->sess + SS_DEMOD, &p, sizeof(p));
	p = s->pcmrx;
	memcpy(s->sess + SS_PCM, &p, sizeof(p));
	p = s->v92p2;
	memcpy(s->sess + SS_V92P2, &p, sizeof(p));
	p = s->iir;
	memcpy(s->sess + SS_TONE_FILTER, &p, sizeof(p));
	p = s->inhist;
	memcpy(s->iir + IIR_INHIST, &p, sizeof(p));
	p = s->outhist;
	memcpy(s->iir + IIR_OUTHIST, &p, sizeof(p));
	{
		unsigned int v;

		v = IIR_DEN_TAPS;
		memcpy(s->iir + IIR_NDEN, &v, sizeof(v));
		v = IIR_NUM_TAPS;
		memcpy(s->iir + IIR_NNUM, &v, sizeof(v));
		v = IIR_HIST_LEN;
		memcpy(s->iir + IIR_INLEN, &v, sizeof(v));
		memcpy(s->iir + IIR_OUTLEN, &v, sizeof(v));
	}
	p = s->designer;
	memcpy(s->demod + DM_DESIGNER, &p, sizeof(p));

	memcpy(s->sess + SS_GATE, &c->gate, sizeof(c->gate));
	memcpy(s->sess + SS_SIDE, &c->mside, sizeof(c->mside));
	memcpy(s->pcmrx + PCM_SENS, &c->sens, sizeof(c->sens));

	s->k56[K56_GATE] = c->k56gate;

	s->cfg[CF_FLAGS3] = c->cfg_flags3;
	memcpy(s->cfg + CF_MIN_RATE, &c->min_rate, sizeof(c->min_rate));
	memcpy(s->cfg + CF_MAX_RATE, &c->max_rate, sizeof(c->max_rate));
	memcpy(s->cfg + CF_MIN_LEVEL, &c->min_level, sizeof(c->min_level));
	memcpy(s->cfg + CF_FILT_DELAY, &c->filt_delay, sizeof(c->filt_delay));
	memcpy(s->cfg + CF_EXT_DELAY, &c->ext_delay, sizeof(c->ext_delay));
}

/*
 * Seed everything.  `handshake` asks for the extra preparation the one entry
 * point that runs `v34handshakinit` needs: `V34InitializeImplementationSpecific`
 * aims both echo cancellers' five pointers each, and without it the first
 * dereference inside `txinit` faults.  That is what a real caller does, not
 * something this fixture invents.
 */
static void
seed(const struct api_case *c, int handshake)
{
	setup();

	if (handshake) {
		V34InitializeImplementationSpecific(&oa);
		ref_V34InitializeImplementationSpecific(ob);
	}

	poke_ptr(OB_P3548, sa.sess, sb.sess);
	poke_ptr(OB_PAC18, sa.k56, sb.k56);
	poke_ptr(OB_PAC3C, sa.cfg, sb.cfg);

	poke_int(OB_STATUS, c->status);
	poke_int(OB_V90RX, c->v90rx);
	poke_int(OB_K56RX, c->k56rx);
	poke_int(OB_RATE_MIN, c->rate_min);
	poke_int(OB_RATE_MAX, c->rate_max);
	poke_short(OB_F359A, c->f359a);
	poke_short(OB_F359C, c->f359c);

	poke_short(OB_MICROSTATE, V34HS_PHASE1);
	poke_short(OB_RXSTATE, V34HS_PHASE2);
	poke_short(OB_TXSTATE, V34HS_TONE_AB);

	seed_side(&sa, c);
	seed_side(&sb, c);
}

/* --- SetUpstreamModulationInfo -------------------------------------------- */

/*
 * ONE INSTRUCTION AND A `ret`.  The claim is that it does nothing, and the
 * check for that is not "the two sides agree" -- an empty function and a
 * function that scribbles identically on both sides agree too -- but that
 * the object is byte for byte what it was before the call.
 */
static unsigned char before_obj[sizeof(struct v34_object)];
static struct side before_side;

static void
run_upstream(const struct api_case *c, long tag)
{
	seed(c, 0);
	memcpy(before_obj, &oa, sizeof(before_obj));
	memcpy(&before_side, &sa, sizeof(before_side));

	SetUpstreamModulationInfo((struct tagV34Object *)&oa);
	ref_SetUpstreamModulationInfo(ob);

	compare_plain("SetUpstreamModulationInfo", tag);
	diff_eq_int("SetUpstreamModulationInfo changed nothing at all",
		    memcmp(before_obj, &oa, sizeof(before_obj)) == 0, 1, tag);
	diff_eq_int("...nor anything it points at",
		    memcmp(&before_side, &sa, sizeof(before_side)) == 0, 1,
		    tag);
}

/* --- SetTimeOut ------------------------------------------------------------ */

static void
run_timeout(const struct api_case *c, int secs, long tag)
{
	seed(c, 0);
	poke_int(OB_SAMPLE_CNT, 0x5a5a5a5a);
	poke_int(OB_TIMEOUT, 0x3c3c3c3c);

	VPcmV34SetTimeOut((struct tagV34Object *)&oa, secs);
	ref_VPcmV34SetTimeOut(ob, secs);

	compare_plain("SetTimeOut", tag);
	diff_eq_int("SetTimeOut restarted the sample clock",
		    get_int_a(OB_SAMPLE_CNT), 0, tag);
	/*
	 * THE MULTIPLY IS 32-BIT AND WRAPS.  Spelled unsigned here for the
	 * same reason t_v34pcmif.c's `want_after` is: `secs * 9600` on a
	 * signed int overflows, and an optimiser is entitled to assume it
	 * cannot -- so an expectation written the signed way is a different
	 * computation from the `imul` the object emits.
	 */
	diff_eq_int("SetTimeOut set the deadline", get_int_a(OB_TIMEOUT),
		    (int)((unsigned int)secs * 0x2580u), tag);
}

/* --- SetMinMaxBitRates ----------------------------------------------------- */

#define RATE_STEP	2400u

static int saw_mm_v90, saw_mm_k56, saw_mm_v34, saw_mm_capped, saw_mm_raised;
static int saw_mm_force;

static void
run_minmax(const struct api_case *c, long tag)
{
	int v90_arm = (c->v90rx != 0 && c->gate != 0);
	int k56_arm = (!v90_arm && c->k56rx != 0 && c->k56gate != 0);

	seed(c, 0);

	VPcmV34SetMinMaxBitRates((struct tagV34Object *)&oa);
	ref_VPcmV34SetMinMaxBitRates(ob);

	compare_plain("SetMinMaxBitRates", tag);

	if (v90_arm) {
		/*
		 * The designer takes both rates RAW -- in bits per second and
		 * undivided -- which is what makes this arm distinguishable
		 * from the V.34 one by the numbers alone.
		 */
		unsigned int lo, hi;

		memcpy(&lo, sa.designer + CD_MIN_RATE, sizeof(lo));
		memcpy(&hi, sa.designer + CD_MAX_RATE, sizeof(hi));
		diff_eq_int("the V.90 arm handed over the raw minimum",
			    (int)lo, (int)c->min_rate, tag);
		diff_eq_int("...and the raw maximum", (int)hi,
			    (int)c->max_rate, tag);
		diff_eq_int("...and left the V.34 rate group alone",
			    get_int_a(OB_RATE_MIN) == c->rate_min
			    && get_int_a(OB_RATE_MAX) == c->rate_max, 1, tag);
		saw_mm_v90 = 1;
	} else if (k56_arm) {
		/*
		 * `K56FlexFloModem::setMinMaxRates` is an empty body, so the
		 * whole of what this arm does is NOT take the V.34 one.
		 */
		diff_eq_int("the K56flex arm left the V.34 rate group alone",
			    get_int_a(OB_RATE_MIN) == c->rate_min
			    && get_int_a(OB_RATE_MAX) == c->rate_max, 1, tag);
		saw_mm_k56 = 1;
	} else {
		/*
		 * THE V.34 ARM, WORKED OUT IN THE OBJECT'S ORDER.  The cap on
		 * the minimum happens BEFORE the comparison, so 40000/33600
		 * ends at 14/14 and not at 16/14; the cap on the maximum
		 * happens after, and RETURNS -- so only a maximum of exactly
		 * 1 ever reaches the force-low-baud store.
		 */
		int lo = (int)(c->min_rate / RATE_STEP);
		int hi = (int)(c->max_rate / RATE_STEP);
		short want_force = c->f359a;

		if (lo > 14)
			lo = 14;
		if (hi < lo)
			hi = lo;
		if (hi > 14)
			hi = 14;
		else if (hi == 1)
			want_force = 1;

		diff_eq_int("the V.34 arm divided the minimum",
			    get_int_a(OB_RATE_MIN), lo, tag);
		diff_eq_int("...and the maximum", get_int_a(OB_RATE_MAX), hi,
			    tag);
		diff_eq_int("...and the force-low-baud short",
			    (int)get_short_a(OB_F359A), (int)want_force, tag);
		if ((int)(c->min_rate / RATE_STEP) > 14
		    || (int)(c->max_rate / RATE_STEP) > 14)
			saw_mm_capped = 1;
		if ((int)(c->max_rate / RATE_STEP) < lo)
			saw_mm_raised = 1;
		if (want_force == 1)
			saw_mm_force = 1;
		saw_mm_v34 = 1;
	}

	compare_sides("SetMinMaxBitRates", tag);
}

/* --- SetMinimumSigLevel ---------------------------------------------------- */

static int saw_sig_table[V34_DISCONNECT_THRESH_ENTRIES];
static int saw_sig_default;

static void
run_siglevel(const struct api_case *c, long tag)
{
	unsigned idx = (unsigned)c->min_level + 0x30u;

	seed(c, 0);
	poke_int(OB_F0234, 0x5a5a5a5a);
	poke_int(OB_ENERGY_FLOOR, 0x3c3c3c3c);

	VPcmV34SetMinimumSigLevel((struct tagV34Object *)&oa);
	ref_VPcmV34SetMinimumSigLevel(ob);

	compare_plain("SetMinimumSigLevel", tag);

	/*
	 * THE INDEX IS COMPARED UNSIGNED, so everything outside -48..-41 --
	 * including every positive level and every level below -48 -- takes
	 * entry 3.  A reconstruction using a signed comparison agrees over
	 * the eight values in range and differs everywhere else.
	 */
	if (idx > 7u) {
		idx = 3u;
		saw_sig_default = 1;
	} else {
		saw_sig_table[idx] = 1;
	}

	diff_eq_int("SetMinimumSigLevel indexed the threshold table",
		    get_int_a(OB_ENERGY_FLOOR), V34DisconnectThreshTable[idx],
		    tag);
	diff_eq_int("...and cleared +0x234", get_int_a(OB_F0234), 0, tag);
}

/* --- SetDelays ------------------------------------------------------------- */

static int saw_delay_negative, saw_delay_positive;

static void
run_delays(const struct api_case *c, long tag)
{
	int want_filt, want_dma;
	unsigned int ec_len_before, ec_delay_before;

	seed(c, 0);
	memcpy(&ec_len_before, sa.sess + EC_LENGTH, sizeof(ec_len_before));
	memcpy(&ec_delay_before, sa.sess + EC_DELAY, sizeof(ec_delay_before));

	VPcmV34SetDelays((struct tagV34Object *)&oa);
	ref_VPcmV34SetDelays(ob);

	compare_plain("SetDelays", tag);

	/*
	 * THE SHIFT IS ARITHMETIC AND BOTH STORES TRUNCATE TO A SHORT, which
	 * is where a reconstruction keeping 32 bits differs: a delay past
	 * 32767*4 reports negative and the field holds the same negative
	 * value.  Written with the truncation explicit so the expectation is
	 * the object's computation and not a wider one that happens to agree
	 * over small inputs.
	 */
	want_filt = (int)(short)((((int)((unsigned int)c->filt_delay + 2u))
				  >> 2) + 0x22);
	want_dma = (int)(short)(0x610u - (unsigned int)c->ext_delay);

	diff_eq_int("SetDelays computed filtdelay",
		    (int)get_short_a(OB_FILT_DELAY), want_filt, tag);
	diff_eq_int("SetDelays computed dmadelay",
		    (int)get_short_a(OB_DMADELAY), want_dma, tag);

	/*
	 * AND THE ECHO CANCELLER, which lives inside the session and which
	 * the object comparison alone cannot attribute: `setEchoDelay` adds
	 * the DIFFERENCE to `echoLength` and then stores the delay, so a
	 * version that assigned instead of accumulating agrees whenever the
	 * old delay was zero.
	 */
	{
		unsigned int want_delay =
			(unsigned int)c->ext_delay + 0x68u;
		unsigned int got_len, got_delay;

		memcpy(&got_len, sa.sess + EC_LENGTH, sizeof(got_len));
		memcpy(&got_delay, sa.sess + EC_DELAY, sizeof(got_delay));
		diff_eq_int("SetDelays set the echo delay", (int)got_delay,
			    (int)want_delay, tag);
		diff_eq_int("...and moved the echo length by the difference",
			    (int)got_len,
			    (int)(ec_len_before + want_delay - ec_delay_before),
			    tag);
	}

	if (want_filt < 0 || want_dma < 0)
		saw_delay_negative = 1;
	else
		saw_delay_positive = 1;
}

/* --- InitMOH --------------------------------------------------------------- */

static int saw_moh_bypass, saw_moh_plain, saw_moh_notch[3];

static void
run_initmoh(const struct api_case *c, int message, unsigned char late,
	    unsigned char flag, long tag)
{
	seed(c, 1);

	/*
	 * The MOH block's own fields, seeded away from what the function
	 * stores so that "wrote it" and "left it" are different pictures.
	 */
	poke_int(OB_MOH_LIMIT, 0x11111111);
	poke_int(OB_MOH_TIMER, 0x22222222);
	poke_short(OB_FABE0, (short)0x3333);
	poke_short(OB_FABE2, (short)0x4444);
	poke_short(OB_MOH_W4, (short)0x5555);
	poke_short(OB_MOH_W6, (short)0x6666);
	poke_byte(OB_ANSAMLATE, 0x77);
	poke_byte(OB_MOH_FLAG, 0x88);
	poke_byte(OB_FABFA, 0x99);
	poke_int(OB_FABEC, 0x0aaaaaaa);
	poke_int(OB_MOH_MESSAGE, 0x0bbbbbbb);
	poke_int(OB_MOH_RECVD, 0x0ccccccc);
	poke_int(OB_F0004, 0x0ddddddd);

	VPcmV34InitMOH(&oa, message, late, flag);
	ref_VPcmV34InitMOH(ob, message, late, flag);

	compare_moh("InitMOH", tag);

	/*
	 * The stores the handshake cannot have overwritten, against the value
	 * rather than only against the blob.  `message == 1` is a BYPASS and
	 * not an error path: the code is stored at +0xabec whatever it is,
	 * and only the second copy is zeroed.
	 */
	diff_eq_int("InitMOH stored the message code as given",
		    get_int_a(OB_FABEC), message, tag);
	diff_eq_int("InitMOH stored the message code as it will be sent",
		    get_int_a(OB_MOH_MESSAGE), message == 1 ? 0 : message, tag);
	if (message == 1)
		saw_moh_bypass = 1;
	else
		saw_moh_plain = 1;

	/*
	 * THE BYPASS ZEROES THE FLAG BYTE TOO.  `message == 1` reaches the
	 * common store with the argument register already cleared, so the
	 * caller's `flag` never lands -- which is invisible on every case
	 * where it was 0 anyway, and is why the two are crossed.
	 */
	diff_eq_int("InitMOH stored the flag byte", (int)get_byte_a(OB_MOH_FLAG),
		    message == 1 ? 0 : (int)flag, tag);
	diff_eq_int("InitMOH stored the late byte",
		    (int)get_byte_a(OB_ANSAMLATE), (int)late, tag);
	/*
	 * AND ITS `setne` BESIDE IT.  That is what says the byte is a value
	 * with an "is it set" reading next to it rather than a flag, and
	 * 0xff is the case that separates the two.
	 */
	diff_eq_int("InitMOH stored whether the late byte was set",
		    (int)get_byte_a(OB_FABFA), late != 0 ? 1 : 0, tag);

	diff_eq_int("InitMOH marked MOH received", get_int_a(OB_MOH_RECVD), 5,
		    tag);
	diff_eq_int("InitMOH cleared the MOH limit", get_int_a(OB_MOH_LIMIT), 0,
		    tag);
	diff_eq_int("InitMOH set the datapump state", get_int_a(OB_F0004), 7,
		    tag);
	diff_eq_int("InitMOH cleared the status", get_int_a(OB_STATUS), 0, tag);
	diff_eq_int("InitMOH set +0x2218", get_int_a(OB_F2218), 2, tag);
	diff_eq_int("InitMOH cleared the three receiver scalars",
		    get_short_a(OB_RX_F258) == 0 && get_short_a(OB_RX_F25A) == 0
		    && get_short_a(OB_RX_F25C) == 0, 1, tag);

	/* The retrain-request bit is cleared FIRST, before any of it. */
	diff_eq_int("InitMOH cleared the retrain request",
		    (int)sa.cfg[CF_FLAGS3],
		    (int)(unsigned char)(c->cfg_flags3 & ~4u), tag);

	/*
	 * THE NOTCH BLOCK TAKES A DIFFERENT ARM FOR EACH ROLE, and the third
	 * arm -- neither 0x65 nor 0x66 -- leaves three shorts alone rather
	 * than storing anything.  A reconstruction with an `else` where the
	 * object has a second `cmpw` agrees on both known roles.
	 */
	{
		short w0c = get_short_a(OB_FAC1C + 0x0c);
		short w0e = get_short_a(OB_FAC1C + 0x0e);
		short w10 = get_short_a(OB_FAC1C + 0x10);

		if (c->f359c == 0x65) {
			diff_eq_int("InitMOH notch, originate",
				    w0c == 0 && w0e == 0
				    && w10 == (short)0x39c3, 1, tag);
			saw_moh_notch[0] = 1;
		} else if (c->f359c == 0x66) {
			diff_eq_int("InitMOH notch, answer",
				    w0c == (short)0x5a82
				    && w0e == (short)0x55fc
				    && w10 == (short)0x39c3, 1, tag);
			saw_moh_notch[1] = 1;
		} else {
			saw_moh_notch[2] = 1;
		}
		diff_eq_int("InitMOH cleared the notch head",
			    get_short_a(OB_FAC1C + 0x00) == 0
			    && get_short_a(OB_FAC1C + 0x02) == 0
			    && get_short_a(OB_FAC1C + 0x04) == 0
			    && get_short_a(OB_FAC1C + 0x06) == 0
			    && get_int_a(OB_FAC1C + 0x08) == 0
			    && get_int_a(OB_FAC1C + 0x14) == 0
			    && get_int_a(OB_FAC1C + 0x18) == 0
			    && get_int_a(OB_FAC1C + 0x1c) == 0, 1, tag);
	}

	/*
	 * AND THE TONE DETECTOR REALLY WAS RESET.  Comparing the two sides
	 * cannot see the call being dropped from BOTH -- the block would then
	 * hold the same fill pattern on each -- so the eight scalars
	 * `GenericToneDetector::reset` clears are checked against zero, and
	 * the filter it resets first is checked through the two positions it
	 * recomputes and the history it zeroes.  Every one of them started at
	 * the fill pattern.
	 */
	{
		unsigned k;
		int zeroed = 1;

		diff_eq_int("InitMOH reset the detector's accumulators",
			    get_int_sess(SS_TONE + GTD_ACC_0C) == 0
			    && get_int_sess(SS_TONE + GTD_ACC_10) == 0
			    && get_int_sess(SS_TONE + GTD_ACC_14) == 0
			    && get_int_sess(SS_TONE + GTD_ACC_18) == 0, 1, tag);
		diff_eq_int("...and its four counters",
			    get_int_sess(SS_TONE + GTD_SAMPLES) == 0
			    && get_int_sess(SS_TONE + GTD_COUNT_2C) == 0
			    && get_int_sess(SS_TONE + GTD_COUNT_30) == 0
			    && get_int_sess(SS_TONE + GTD_DETECTED) == 0, 1,
			    tag);
		for (k = 0; k < IIR_HIST_LEN; k++)
			if (sa.inhist[k] != 0.0 || sa.outhist[k] != 0.0)
				zeroed = 0;
		diff_eq_int("...and the filter's history", zeroed, 1, tag);
		diff_eq_int("...and the filter's two write positions",
			    get_int_iir(IIR_INPOS) == IIR_HIST_LEN - IIR_NUM_TAPS
			    && get_int_iir(IIR_OUTPOS)
			       == IIR_HIST_LEN - IIR_DEN_TAPS, 1, tag);
	}
}

/* --- the two VPcmFloModem members ------------------------------------------ */

/*
 * WHOLE-BUFFER, AND THEN AGAINST EACH OTHER.  Six stores each into an object
 * this tree does not have a settled size for, so the slot is `sizeof` plus
 * slack and every byte of it is compared -- a store landing outside the six
 * is otherwise inside memory nobody looks at.
 */
#define FLO_SLACK	64
#define FLO_SLOT	(sizeof(VPcmFloModem) + FLO_SLACK)
#define FLO_ALLOW	0x217

static unsigned char flo_a[FLO_SLOT], flo_b[FLO_SLOT], flo_c[FLO_SLOT];

static void
fill_flo(unsigned char *p)
{
	unsigned i;

	for (i = 0; i < FLO_SLOT; i++)
		p[i] = (unsigned char)(0x31u + i * 7u + (i >> 5));
}

int
main(void)
{
	int rc = 0;
	unsigned i, j, k, l, n;
	long tag;

	/* --- SetUpstreamModulationInfo ----------------------------------- */

	diff_begin("v34 pcm api: SetUpstreamModulationInfo does nothing");
	{
		static const short role_in[] = { 0x64, 0x65, 0x66 };
		static const int st_in[] = { 0, 1, 2, 3, -1 };

		tag = 1000;
		for (i = 0; i < sizeof(role_in) / sizeof(role_in[0]); i++)
		for (j = 0; j < sizeof(st_in) / sizeof(st_in[0]); j++) {
			struct api_case c = api_base;

			c.f359c = role_in[i];
			c.status = st_in[j];
			run_upstream(&c, tag++);
		}
	}
	rc |= diff_end();

	/* --- SetTimeOut --------------------------------------------------- */

	/*
	 * THE MULTIPLY OVERFLOWS AND THE SWEEP GOES PAST IT.  9,600 times
	 * 223,697 is over 2^31, so the two rows either side of that are where
	 * a reconstruction using a wider type differs from the `imul` -- and
	 * both extremes of the signed range are in for the same reason.
	 */
	diff_begin("v34 pcm api: SetTimeOut, past the 32-bit wrap");
	{
		static const int secs_in[] = {
			0, 1, -1, 2, 10, 60, 1000, 223695, 223696, 223697,
			223698, 447392, 0x7fffffff, (-0x7fffffff - 1),
			-223697
		};

		tag = 2000;
		for (i = 0; i < sizeof(secs_in) / sizeof(secs_in[0]); i++) {
			struct api_case c = api_base;

			run_timeout(&c, secs_in[i], tag++);
		}
	}
	rc |= diff_end();

	/* --- SetMinMaxBitRates -------------------------------------------- */

	/*
	 * THE FOUR GATING INPUTS ARE CROSSED, because the object does not
	 * combine them with a single operator: the V.90 test SKIPS the
	 * K56flex one rather than being or-ed with it, and the case that
	 * tells an `&&` from an `||` is "V.90 receiver up, session gate
	 * clear, K56flex receiver up" -- which falls THROUGH to the second
	 * test.  A K56flex receiver whose own gate byte is clear falls
	 * through again, to the V.34 arm, which is the third thing a
	 * two-armed reading gets wrong.
	 *
	 * The rate pairs hit every fix-up in the V.34 arm and both orders of
	 * the comparison between them, plus the top of the UNSIGNED range,
	 * which is where a signed division gives a different answer.
	 */
	diff_begin("v34 pcm api: SetMinMaxBitRates, three arms and every "
		   "fix-up");
	{
		static const int v90_in[] = { 0, 1, -1 };
		static const int gate_in[] = { 0, 1, -1 };
		static const int k56_in[] = { 0, 1, -1 };
		static const unsigned char k56gate_in[] = { 0, 1, 0xff };
		static const unsigned int rate_in[][2] = {
			{ 0u, 0u },
			{ 2400u, 2400u },
			{ 2400u, 33600u },
			{ 40000u, 33600u },
			{ 33600u, 2400u },
			{ 36000u, 40000u },
			{ 0u, 2400u },
			{ 2399u, 4799u },
			{ 33600u, 33600u },
			{ 36000u, 36000u },
			{ 0xffffffffu, 0xffffffffu },
			{ 0x80000000u, 0x80000000u },
			{ 2400u, 0u }
		};

		tag = 3000;
		for (i = 0; i < sizeof(v90_in) / sizeof(v90_in[0]); i++)
		for (j = 0; j < sizeof(gate_in) / sizeof(gate_in[0]); j++)
		for (k = 0; k < sizeof(k56_in) / sizeof(k56_in[0]); k++)
		for (l = 0; l < sizeof(k56gate_in) / sizeof(k56gate_in[0]); l++)
		for (n = 0; n < sizeof(rate_in) / sizeof(rate_in[0]); n++) {
			struct api_case c = api_base;

			c.v90rx = v90_in[i];
			c.gate = gate_in[j];
			c.k56rx = k56_in[k];
			c.k56gate = k56gate_in[l];
			c.min_rate = rate_in[n][0];
			c.max_rate = rate_in[n][1];
			run_minmax(&c, tag++);
		}

		diff_eq_int("the V.90 arm was reached", saw_mm_v90, 1, 3900);
		diff_eq_int("the K56flex arm was reached", saw_mm_k56, 1, 3901);
		diff_eq_int("and the V.34 arm", saw_mm_v34, 1, 3902);
		diff_eq_int("some V.34 case was capped at 14", saw_mm_capped, 1,
			    3903);
		diff_eq_int("some V.34 case raised the maximum", saw_mm_raised,
			    1, 3904);
		diff_eq_int("and some case forced the low baud", saw_mm_force,
			    1, 3905);
	}
	rc |= diff_end();

	/* --- SetMinimumSigLevel ------------------------------------------- */

	/*
	 * EVERY ENTRY OF THE TABLE AND BOTH SIDES OF ITS RANGE.  The index is
	 * `level + 48` compared UNSIGNED against 7, so -48..-41 select entries
	 * 0..7 and everything else -- including 0 and every positive level --
	 * takes entry 3.
	 */
	diff_begin("v34 pcm api: SetMinimumSigLevel, every table entry and "
		   "the unsigned default");
	{
		static const int lvl_in[] = {
			-50, -49, -48, -47, -46, -45, -44, -43, -42, -41, -40,
			-39, 0, 1, 7, 1000, 0x7fffffff, (-0x7fffffff - 1)
		};

		tag = 4000;
		for (i = 0; i < sizeof(lvl_in) / sizeof(lvl_in[0]); i++) {
			struct api_case c = api_base;

			c.min_level = lvl_in[i];
			run_siglevel(&c, tag++);
		}

		for (i = 0; i < V34_DISCONNECT_THRESH_ENTRIES; i++)
			diff_eq_int("every table entry was selected",
				    saw_sig_table[i], 1, 4900 + (long)i);
		diff_eq_int("and the default was taken", saw_sig_default, 1,
			    4910);
	}
	rc |= diff_end();

	/* --- SetDelays ---------------------------------------------------- */

	/*
	 * BOTH DELAYS SWEPT INDEPENDENTLY, and both past the point where the
	 * short truncates.  The shift is ARITHMETIC, so a negative configured
	 * delay is not the same as a large positive one, and `0x610 - ext` is
	 * a subtraction that goes negative for any ext past 1552.
	 */
	diff_begin("v34 pcm api: SetDelays, both truncations and the echo "
		   "canceller");
	{
		static const int filt_in[] = {
			0, 1, 2, -1, -2, -3, -4, 64, 200, 131068, 131069,
			131070, -131072, 0x7ffffffc, (-0x7fffffff - 1)
		};
		static const int ext_in[] = {
			0, 1, 200, 1551, 1552, 1553, -1, -1000, 0x7fffffff,
			(-0x7fffffff - 1)
		};

		tag = 5000;
		for (i = 0; i < sizeof(filt_in) / sizeof(filt_in[0]); i++)
		for (j = 0; j < sizeof(ext_in) / sizeof(ext_in[0]); j++) {
			struct api_case c = api_base;

			c.filt_delay = filt_in[i];
			c.ext_delay = ext_in[j];
			run_delays(&c, tag++);
		}

		diff_eq_int("some case truncated to a negative delay",
			    saw_delay_negative, 1, 5900);
		diff_eq_int("and some case did not", saw_delay_positive, 1,
			    5901);
	}
	rc |= diff_end();

	/* --- InitMOH ------------------------------------------------------ */

	/*
	 * THE THREE ARGUMENTS AND THE ROLE, CROSSED.  `message` decides the
	 * bypass, `late` reaches TWO fields -- itself and a `setne` of itself
	 * -- so 0xff is what separates a value from a flag, `flag` is stored
	 * whole, and the role picks one of three arms in the notch block.
	 */
	diff_begin("v34 pcm api: InitMOH, every message against every role");
	{
		static const int msg_in[] = { 0, 1, 2, -1 };
		static const unsigned char late_in[] = { 0, 1, 0xff };
		static const unsigned char flag_in[] = { 0, 0xff };
		static const short role_in[] = { 0x64, 0x65, 0x66 };

		tag = 6000;
		for (i = 0; i < sizeof(msg_in) / sizeof(msg_in[0]); i++)
		for (j = 0; j < sizeof(late_in) / sizeof(late_in[0]); j++)
		for (k = 0; k < sizeof(flag_in) / sizeof(flag_in[0]); k++)
		for (l = 0; l < sizeof(role_in) / sizeof(role_in[0]); l++) {
			struct api_case c = api_base;

			c.f359c = role_in[l];
			run_initmoh(&c, msg_in[i], late_in[j], flag_in[k],
				    tag++);
		}

		diff_eq_int("the bypass was taken", saw_moh_bypass, 1, 6900);
		diff_eq_int("and the ordinary path", saw_moh_plain, 1, 6901);
		diff_eq_int("the originate notch arm was taken",
			    saw_moh_notch[0], 1, 6902);
		diff_eq_int("the answer notch arm was taken", saw_moh_notch[1],
			    1, 6903);
		diff_eq_int("and the third arm, which stores nothing",
			    saw_moh_notch[2], 1, 6904);

		for (i = 0; i < NMOHPTR; i++)
			diff_eq_int("InitMOH: pointer field was installed",
				    saw_moh_ptr[i], 1, (long)moh_ptr[i]);
	}
	rc |= diff_end();

	/* --- the same six, with the diagnostics live ---------------------- */

	/*
	 * THREE OF THESE PRINT AND ONE OF THE THREE PRINTS NOTHING ELSE.
	 * `VPcmV34SetMinimumSigLevel`'s line is the only trace of the value it
	 * was given, `VPcmV34SetDelays` prints each stored short back
	 * SIGN-EXTENDED beside the configured int, and `VPcmV34InitMOH`'s
	 * bypass line is the only difference between `message == 1` and any
	 * other code at the level of the transcript.  Finding F134.
	 */
	diff_begin("v34 pcm api: the accessor surface, transcripts too");
	{
		static const int msg_in[] = { 0, 1, 2 };
		static const int lvl_in[] = { -48, -44, -41, 0, 100 };
		static const int filt_in[] = { 0, 64, -4, 131070 };
		static const int ext_in[] = { 0, 200, 1553, -1000 };
		static const unsigned int rate_in[][2] = {
			{ 2400u, 2400u }, { 4800u, 33600u }, { 40000u, 33600u }
		};

		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		tag = 7000;
		for (i = 0; i < sizeof(lvl_in) / sizeof(lvl_in[0]); i++) {
			struct api_case c = api_base;

			c.min_level = lvl_in[i];
			dsplib_debug_capture_reset();
			run_siglevel(&c, tag);
			diff_eq_int("SetMinimumSigLevel transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("and it said something",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    tag);
			tag++;
		}

		for (i = 0; i < sizeof(filt_in) / sizeof(filt_in[0]); i++)
		for (j = 0; j < sizeof(ext_in) / sizeof(ext_in[0]); j++) {
			struct api_case c = api_base;

			c.filt_delay = filt_in[i];
			c.ext_delay = ext_in[j];
			dsplib_debug_capture_reset();
			run_delays(&c, tag);
			diff_eq_int("SetDelays transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("and it said something",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    tag);
			tag++;
		}

		/*
		 * The V.90 arm's two lines come from the constellation
		 * designer, which is the only thing that arm does that the
		 * object comparison can see nothing of.
		 */
		for (i = 0; i < sizeof(rate_in) / sizeof(rate_in[0]); i++) {
			struct api_case c = api_base;

			c.v90rx = 1;
			c.gate = 1;
			c.min_rate = rate_in[i][0];
			c.max_rate = rate_in[i][1];
			dsplib_debug_capture_reset();
			run_minmax(&c, tag);
			diff_eq_int("SetMinMaxBitRates transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("and it said something",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    tag);
			tag++;
		}

		for (i = 0; i < sizeof(msg_in) / sizeof(msg_in[0]); i++) {
			struct api_case c = api_base;

			dsplib_debug_capture_reset();
			run_initmoh(&c, msg_in[i], 1, 0, tag);
			diff_eq_int("InitMOH transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
			diff_eq_int("and it said something",
				    dsplib_debug_capture_text(1)[0] != 0, 1,
				    tag);
			tag++;
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	/*
	 * AND THE GATE, which the section above is structurally blind to: it
	 * raises the level first, so a call site that lost its
	 * `if (DSPLIB_DEBUG_ON())` prints the same thing and passes.  Both
	 * levels below the threshold, because every gate in the object is
	 * `> 1` and 1 is the only value that separates it from `>= 1`.
	 */
	diff_begin("v34 pcm api: below the threshold, nothing is said");
	{
		unsigned lvl;

		dsplib_debug_capture_on = 1;

		for (lvl = 0; lvl <= 1; lvl++) {
			struct api_case c = api_base;

			dsplibs_debug_level = lvl;
			ref_dsplibs_debug_level = lvl;
			dsplib_debug_capture_reset();

			run_upstream(&c, 8000 + (long)lvl * 10);
			run_timeout(&c, 10, 8001 + (long)lvl * 10);
			run_siglevel(&c, 8002 + (long)lvl * 10);
			run_delays(&c, 8003 + (long)lvl * 10);
			run_minmax(&c, 8004 + (long)lvl * 10);
			c.v90rx = 1;
			c.gate = 1;
			run_minmax(&c, 8005 + (long)lvl * 10);
			c = api_base;
			run_initmoh(&c, 1, 1, 0, 8006 + (long)lvl * 10);

			diff_eq_int("ours printed nothing",
				    dsplib_debug_capture_text(0)[0], 0,
				    8000 + (long)lvl);
			diff_eq_int("and neither did the reference",
				    dsplib_debug_capture_text(1)[0], 0,
				    8000 + (long)lvl);
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	/* --- setV34BaudForV90 and setV34BaudForV34 ------------------------ */

	/*
	 * SIX STORES EACH, AND THE WHOLE SLOT IS COMPARED.  The two functions
	 * are byte-identical bar the last store, so a whole-buffer comparison
	 * on each is what proves that the difference is at +0x21c and nowhere
	 * else -- and that claim is made explicitly at the end rather than
	 * left implicit in two separately-passing comparisons.
	 */
	diff_begin("v34 pcm api: setV34BaudForV90 and setV34BaudForV34");
	{
		unsigned d;
		int diffs = 0, at21c = 0;

		fill_flo(flo_a);
		fill_flo(flo_b);
		((VPcmFloModem *)(void *)flo_a)->setV34BaudForV90();
		ref_setV34BaudForV90(flo_b);
		compare_block("setV34BaudForV90", flo_a, flo_b, FLO_SLOT, 0, 0,
			      9000);

		fill_flo(flo_c);
		fill_flo(flo_b);
		((VPcmFloModem *)(void *)flo_c)->setV34BaudForV34();
		ref_setV34BaudForV34(flo_b);
		compare_block("setV34BaudForV34", flo_c, flo_b, FLO_SLOT, 0, 0,
			      9001);

		/*
		 * The six values, worked out by hand: entry 1 is zero on both
		 * and entry 5 is the only one they disagree about.
		 */
		diff_eq_int("setV34BaudForV90 barred entry 1",
			    (int)flo_a[FLO_ALLOW + 1], 0, 9002);
		diff_eq_int("setV34BaudForV90 barred entry 5",
			    (int)flo_a[FLO_ALLOW + 5], 0, 9003);
		diff_eq_int("setV34BaudForV34 barred entry 1",
			    (int)flo_c[FLO_ALLOW + 1], 0, 9004);
		diff_eq_int("setV34BaudForV34 allowed entry 5",
			    (int)flo_c[FLO_ALLOW + 5], 1, 9005);
		for (d = 0; d < 6; d++) {
			if (d == 1 || d == 5)
				continue;
			diff_eq_int("and the other four are allowed by both",
				    (int)flo_a[FLO_ALLOW + d] == 1
				    && (int)flo_c[FLO_ALLOW + d] == 1, 1,
				    9010 + (long)d);
		}

		/*
		 * AND THE PAIR DIFFERS IN EXACTLY ONE BYTE.  Two whole-buffer
		 * comparisons that both pass do not say that; this does, and
		 * it is the whole of what makes them two functions.
		 */
		for (d = 0; d < FLO_SLOT; d++)
			if (flo_a[d] != flo_c[d]) {
				diffs++;
				if (d == FLO_ALLOW + 5)
					at21c = 1;
			}
		diff_eq_int("the two differ in exactly one byte", diffs, 1,
			    9020);
		diff_eq_int("and that byte is +0x21c", at21c, 1, 9021);
	}
	rc |= diff_end();

	return rc;
}
