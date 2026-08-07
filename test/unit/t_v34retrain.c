/*
 * t_v34retrain.c -- differential test of `VPcmV34InitiateRetrain`.
 *
 * SEVEN OBJECTS PER SIDE, NOT ONE.  The function reaches out of the V.34
 * object through three pointers and two of those lead on further:
 *
 *     obj +0x3548 -> session   +0x175c -> demodulator +0x208 -> designer
 *                              +0x610c -> pcm receiver
 *                              +0x612c -> V.92 phase 2 record
 *                              +0x6bd0    an EMBEDDED V92EchoCanceller
 *     obj +0xac18 -> k56flex modem
 *     obj +0xac3c -> configuration
 *
 * Each side gets its own copy of all of them, prefilled with the same varied
 * pattern, and every block is compared to its opposite number afterwards.  A
 * test that compared only the V.34 object would pass with all four calls out
 * of the function deleted: three of them write nothing the V.34 object can
 * see, and the fourth -- `setPcmSessionType` -- writes only into the session.
 *
 * AND THE HANDSHAKE IS THE REASON THE MUTATION SUITE IS NOT OPTIONAL.  Every
 * path through this function ends in `v34handshakinit(obj, 1)`, which runs
 * `v34modeminit` and rewrites a large part of the object with the SAME code
 * on both sides.  Any store this function makes before that which the
 * handshake later overwrites is invisible here whatever the sweep looks like
 * -- finding 253's trap with a bigger blast radius.  test/mutations/
 * v34retrain.json breaks each pre-handshake store in turn and finding 318
 * records which ones the differential tier can actually see.
 *
 * THE INPUTS ARE CROSSED WHERE THE OBJECT CROSSES THEM.  Which of three arms
 * re-reads the configured rates is decided by four inputs that are not one
 * expression: `v90_receiver` AND the session's `+0x6120`, failing that
 * `k56flex_receiver` AND the K56flex object's `+0x8`.  The case that tells an
 * `&&` from an `||` is "V.90 receiver up, gate clear, K56flex up" -- it falls
 * THROUGH to the second test -- so all four are swept together rather than
 * one at a time.  `requestedDp` is crossed with them because the validation
 * switch is itself gated on `v90_receiver`.
 *
 * THE DEBUG TRANSCRIPTS ARE COMPARED, and here that is not a formality:
 * three of this function's eight call sites -- "Initiating retrain",
 * "minLevel given is" and "V34 filtdelay set to" -- write NOTHING but the
 * message, so deleting any of them is invisible to a state comparison
 * (finding 134).  Every case therefore runs twice, once with both levels at 0
 * and once with both at 2, and the two transcripts are diffed.
 *
 * AND TWO OF THE STORES ARE PRINTED BACK AS SHORTS.  "V34 filtdelay set to
 * %d" and "V34dmadelay set to %d" re-read the 16-bit field the line above
 * wrote, so a reconstruction that printed the int it computed would agree on
 * every small input.  The delay sweeps therefore contain values that overflow
 * a short in both directions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34pcm_tables.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"

extern unsigned int ref_dsplibs_debug_level;

extern void ref_VPcmV34InitiateRetrain(void *obj, unsigned char requestedDp);
extern void ref_V34InitializeImplementationSpecific(void *obj);

/*
 * The three state words, seeded in range on every case.  `v34handshakinit`
 * mode 1 runs two transitions and each indexes `StateName` with the value it
 * finds; nothing bounds the index (D42), so a fill pattern there is an
 * out-of-bounds read the moment the debug level goes up.
 */
#define HSI_MICROSTATE	0x3592
#define HSI_RXSTATE	0x3594
#define HSI_TXSTATE	0x3596

/* Object offsets this file seeds or reads.  See src/pump/v34/v34pcmmain.cpp. */
#define OB_STATUS	0x0000
#define OB_F0004	0x0004
#define OB_RATE_MIN	0x0220
#define OB_RATE_MAX	0x0224
#define OB_ENERGY_FLOOR	0x0230
#define OB_V90RX	0x024c
#define OB_K56RX	0x0250
#define OB_F0254	0x0254
#define OB_F25C		0x025c
#define OB_TIMER_BASE	0x0238
#define OB_TIMER_MARK	0x0248
#define OB_RXFLAGS	(0x0264 + 0x122)
#define OB_F2218	0x2218
#define OB_P3548	0x3548
#define OB_F359A	0x359a
#define OB_F359C	0x359c
#define OB_F35A4	0x35a4
#define OB_FA23C	0xa23c
#define OB_FILT_DELAY	0xaa7c
#define OB_LOCAL_SHORT	0xabca
#define OB_IS_SHORT	0xabcc
#define OB_FAC00	0xac00
#define OB_AC17		0xac17
#define OB_PAC18	0xac18
#define OB_FAC1C	0xac1c
#define OB_PAC3C	0xac3c

/* Session offsets. */
#define SS_DEMOD	0x175c
#define SS_PCM		0x610c
#define SS_SESSFLAG	0x6110
#define SS_SIDE		0x6114
#define SS_PCMTYPE	0x611c
#define SS_GATE		0x6120
#define SS_V92P2	0x612c
#define SS_ECHO		0x6bd0
#define SS_RETRAIN	0x6fb4

#define DM_DESIGNER	0x0208
#define PCM_SENS	0x04f8
#define K56_GATE	0x0008
#define V92P2_CAPLOCAL	0x0011		/* V92Phase2Info's local caps byte */

#define CF_FLAGS	0x00
#define CF_MIN_RATE	0x30
#define CF_MAX_RATE	0x34
#define CF_ISP		0x50
#define CF_MIN_LEVEL	0x60
#define CF_FILT_DELAY	0x64
#define CF_EXT_DELAY	0x68

/*
 * The blocks behind the pointers.  Lengths are generous rather than measured:
 * the point of comparing them is to catch a store LANDING OUTSIDE the fields
 * the function is about, which a block sized to the last known field could
 * not do.  Every length is a multiple of four and the leading `align` member
 * gives the whole struct four-byte alignment, so every block starts aligned.
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
	unsigned char	cfg[CFG_LEN];
	unsigned char	k56[K56_LEN];
};

static struct v34_object oa;
static unsigned char ob[sizeof(struct v34_object)];
static struct side sa, sb;

/*
 * Pointer-sized fields the two sides necessarily disagree about, excluded
 * from the byte comparison and checked by what they select instead.
 *
 * The first three are the FIXTURE's: nothing under test writes them, and what
 * justifies the holes is that the memory behind each is compared per case.
 * The rest are installed by `V34InitializeImplementationSpecific` and by
 * `v34handshakinit` mode 1 through `v34modeminit`, and were established by
 * running this file with an empty list and classifying what differed.
 * `saw_ptr_skip` asserts at the end that every one was really reached, so an
 * entry that stopped being written turns into a failure rather than into a
 * hole nobody notices.
 */
static const unsigned ptr_skip[] = {
	OB_P3548,			/* the session object            */
	OB_PAC18,			/* the K56flex modem             */
	OB_PAC3C,			/* the configuration             */
	0x0268, 0x026c,			/* rxq read and write cursors    */
	0x0394,				/* receiver +0x130 rx_samples    */
	0x0418,				/* receiver +0x1b4 carrier       */
	0x0508,				/* receiver +0x2a4 f2a4          */
	0x0a28, 0x0e48,			/* receive shell context         */
	0x1460,				/* modulator +0x10 sine          */
	0x2074,				/* modulator +0xc24 shaped       */
	0x20cc,				/* modulator +0xc7c ec_prem      */
	0x2100,				/* modulator +0xcb0 preemp       */
	0x2220, 0x2224,			/* txq cursors                   */
	0x2608, 0x2a28,			/* transmit shell context        */
	0x3564,				/* detector +0x00 coeff          */
	0x80b8, 0x80bc, 0x80c0, 0x80c4, 0x80c8,	/* echo canceller 0      */
	0x9138, 0x913c, 0x9140, 0x9144, 0x9148	/* and 1                 */
};
#define NPTR (sizeof(ptr_skip) / sizeof(ptr_skip[0]))
#define NFIXTURE_PTR 3

static int saw_ptr_skip[NPTR];

/* The session's three outgoing pointers, and the demodulator's one. */
static const unsigned sess_ptr_skip[] = { SS_DEMOD, SS_PCM, SS_V92P2 };
#define NSESSPTR (sizeof(sess_ptr_skip) / sizeof(sess_ptr_skip[0]))

static const unsigned demod_ptr_skip[] = { DM_DESIGNER };
#define NDEMODPTR (sizeof(demod_ptr_skip) / sizeof(demod_ptr_skip[0]))

static int
in_holes(unsigned off, const unsigned *holes, unsigned n)
{
	unsigned k;

	for (k = 0; k < n; k++)
		if (off >= holes[k] && off < holes[k] + 4)
			return 1;
	return 0;
}

static void
poke_ptr(unsigned off, void *pa, void *pb)
{
	memcpy((unsigned char *)&oa + off, &pa, sizeof(pa));
	memcpy(ob + off, &pb, sizeof(pb));
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

static void
poke_int(unsigned off, int v)
{
	memcpy((unsigned char *)&oa + off, &v, sizeof(v));
	memcpy(ob + off, &v, sizeof(v));
}

static int
get_int_a(unsigned off)
{
	int v;

	memcpy(&v, (unsigned char *)&oa + off, sizeof(v));
	return v;
}

static short
get_short_a(unsigned off)
{
	short v;

	memcpy(&v, (unsigned char *)&oa + off, sizeof(v));
	return v;
}

/*
 * Everything a case chooses.  ONE FIELD PER INPUT, so that no two can be
 * driven from one variable: that is the fixture defect of findings 116b, 123
 * and 171, and this function has four gating inputs whose conjunction is the
 * whole point.
 */
struct rt_case {
	unsigned char	dp;		/* the argument                    */
	int		v90rx;		/* obj +0x24c                      */
	int		k56rx;		/* obj +0x250                      */
	int		gate;		/* session +0x6120                 */
	unsigned char	k56gate;	/* k56flex +0x08                   */
	unsigned int	min_rate;	/* cfg +0x30, bits per second      */
	unsigned int	max_rate;	/* cfg +0x34                       */
	int		min_level;	/* cfg +0x60, biased by 0x30       */
	int		filt_delay;	/* cfg +0x64                       */
	int		ext_delay;	/* cfg +0x68                       */
	unsigned char	cfg_flags;	/* cfg +0x00, bits 3 and 4         */
	unsigned char	cfg_isp;	/* cfg +0x50, bit 3 is set here    */
	int		sens;		/* pcm receiver +0x4f8             */
	int		status;		/* obj +0x00                       */
	short		f359c;		/* obj +0x359c, originate/answer   */
	short		f35a4;		/* obj +0x35a4                     */
	int		mside;		/* session +0x6114, V90Modem::side */
	int		timer_base;	/* obj +0x238, for the handshake   */
	int		timer_mark;	/* obj +0x248                      */
	unsigned short	rxflags;	/* receiver +0x122                 */
	unsigned char	ac17;		/* obj +0xac17                     */
};

/*
 * The default.  `min_level` of -0x30 is index 0; the rates are 4800 and
 * 33600, which are indices 2 and 14; `mside` is 2 so that
 * `V90Modem::setSessionFlag` stops at the modem rather than walking into a
 * modulator this fixture does not model -- the chain below it is wave 2's and
 * is tested there.  Two cases put it back to 1 to prove that choice is a
 * limit of this fixture and not of the reconstruction.
 */
static const struct rt_case rt_base = {
	0,			/* dp                                       */
	0, 0,			/* v90rx, k56rx                             */
	0,			/* gate                                     */
	0,			/* k56gate                                  */
	4800u, 33600u,		/* min_rate, max_rate                       */
	-0x30,			/* min_level -> index 0                     */
	64, 200,		/* filt_delay, ext_delay                    */
	0x18,			/* cfg_flags: both v90 and flex allowed     */
	0x41,			/* cfg_isp: bit 3 clear, other bits set     */
	0,			/* sens                                     */
	0,			/* status                                   */
	0x65,			/* f359c                                    */
	3,			/* f35a4                                    */
	2,			/* mside                                    */
	0, 0,			/* timer_base, timer_mark                   */
	0x0000,			/* rxflags                                  */
	0			/* ac17                                     */
};

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

/*
 * Compare one pair of blocks.  Only mismatches are reported, plus one summary
 * check per block so an all-equal run still counts as a check.
 */
static void
compare_block(const char *what, const unsigned char *pa,
	      const unsigned char *pb, unsigned len,
	      const unsigned *holes, unsigned nholes, long tag)
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

static void
compare_all(const char *what, long tag)
{
	unsigned k;
	char msg[96];

	for (k = 0; k < NPTR; k++)
		if (memcmp((unsigned char *)&oa + ptr_skip[k],
			   ob + ptr_skip[k], 4) != 0)
			saw_ptr_skip[k] = 1;

	compare_block(what, (const unsigned char *)&oa, ob, sizeof(oa),
		      ptr_skip, NPTR, tag);

	snprintf(msg, sizeof(msg), "%s: session", what);
	compare_block(msg, sa.sess, sb.sess, SESS_LEN,
		      sess_ptr_skip, NSESSPTR, tag);
	snprintf(msg, sizeof(msg), "%s: pcm receiver", what);
	compare_block(msg, sa.pcmrx, sb.pcmrx, PCMRX_LEN, 0, 0, tag);
	snprintf(msg, sizeof(msg), "%s: demodulator", what);
	compare_block(msg, sa.demod, sb.demod, DEMOD_LEN,
		      demod_ptr_skip, NDEMODPTR, tag);
	snprintf(msg, sizeof(msg), "%s: constellation designer", what);
	compare_block(msg, sa.designer, sb.designer, DESIGNER_LEN, 0, 0, tag);
	snprintf(msg, sizeof(msg), "%s: v92 phase 2", what);
	compare_block(msg, sa.v92p2, sb.v92p2, V92P2_LEN, 0, 0, tag);
	snprintf(msg, sizeof(msg), "%s: configuration", what);
	compare_block(msg, sa.cfg, sb.cfg, CFG_LEN, 0, 0, tag);
	snprintf(msg, sizeof(msg), "%s: k56flex", what);
	compare_block(msg, sa.k56, sb.k56, K56_LEN, 0, 0, tag);
}

/* Seed one side's out-of-object blocks. */
static void
seed_side(struct side *s, const struct rt_case *c)
{
	void *p;

	p = s->demod;
	memcpy(s->sess + SS_DEMOD, &p, sizeof(p));
	p = s->pcmrx;
	memcpy(s->sess + SS_PCM, &p, sizeof(p));
	p = s->v92p2;
	memcpy(s->sess + SS_V92P2, &p, sizeof(p));
	p = s->designer;
	memcpy(s->demod + DM_DESIGNER, &p, sizeof(p));

	memcpy(s->sess + SS_GATE, &c->gate, sizeof(c->gate));
	memcpy(s->sess + SS_SIDE, &c->mside, sizeof(c->mside));
	memcpy(s->pcmrx + PCM_SENS, &c->sens, sizeof(c->sens));

	s->k56[K56_GATE] = c->k56gate;

	s->cfg[CF_FLAGS] = c->cfg_flags;
	s->cfg[CF_ISP] = c->cfg_isp;
	memcpy(s->cfg + CF_MIN_RATE, &c->min_rate, sizeof(c->min_rate));
	memcpy(s->cfg + CF_MAX_RATE, &c->max_rate, sizeof(c->max_rate));
	memcpy(s->cfg + CF_MIN_LEVEL, &c->min_level, sizeof(c->min_level));
	memcpy(s->cfg + CF_FILT_DELAY, &c->filt_delay, sizeof(c->filt_delay));
	memcpy(s->cfg + CF_EXT_DELAY, &c->ext_delay, sizeof(c->ext_delay));
}

/* One case, at the debug level the caller has already set. */
static void
drive(const struct rt_case *c)
{
	setup();

	/*
	 * `v34modeminit` cleans both echo cancellers through five pointers
	 * each, and aiming them is this function's job rather than the
	 * handshake's -- without it the first dereference faults.  t_v34hshak.c
	 * does the same for every mode that reaches `txinit`.
	 */
	V34InitializeImplementationSpecific(&oa);
	ref_V34InitializeImplementationSpecific(ob);

	poke_ptr(OB_P3548, sa.sess, sb.sess);
	poke_ptr(OB_PAC18, sa.k56, sb.k56);
	poke_ptr(OB_PAC3C, sa.cfg, sb.cfg);

	poke_int(OB_STATUS, c->status);
	poke_int(OB_V90RX, c->v90rx);
	poke_int(OB_K56RX, c->k56rx);
	poke_int(OB_TIMER_BASE, c->timer_base);
	poke_int(OB_TIMER_MARK, c->timer_mark);
	poke_short(OB_F359C, c->f359c);
	poke_short(OB_F35A4, c->f35a4);
	poke_short(OB_RXFLAGS, (short)c->rxflags);
	poke_byte(OB_AC17, c->ac17);

	poke_short(HSI_MICROSTATE, V34HS_PHASE1);
	poke_short(HSI_RXSTATE, V34HS_PHASE2);
	poke_short(HSI_TXSTATE, V34HS_TONE_AB);

	seed_side(&sa, c);
	seed_side(&sb, c);

	VPcmV34InitiateRetrain(&oa, c->dp);
	ref_VPcmV34InitiateRetrain(ob, c->dp);
}

/*
 * One case, twice: silent and talking.  The second run is the only thing that
 * can see a dropped diagnostic, and three of this function's eight print
 * sites write nothing else at all.
 */
static int transcript_seen;

static void
run_case(const struct rt_case *c, long tag)
{
	drive(c);
	compare_all("InitiateRetrain", tag);

	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();
	dsplibs_debug_level = 2;
	ref_dsplibs_debug_level = 2;

	drive(c);

	dsplibs_debug_level = 0;
	ref_dsplibs_debug_level = 0;
	dsplib_debug_capture_on = 0;

	compare_all("InitiateRetrain, talking", tag);
	diff_eq_int("InitiateRetrain transcript",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, tag);
	if (dsplib_debug_capture_lines(1) > 0)
		transcript_seen = 1;
}

int
main(void)
{
	int rc = 0;
	unsigned i, j, k, l, n;
	long tag;

	/*
	 * ------------------------------------------------------------------
	 * The four gating inputs, crossed with every datapump code.
	 *
	 * 4 x 3 x 3 x 2 x 14 = 1,008 cases, each run twice.  The values are
	 * chosen to separate the tests the object actually makes: `v90rx`
	 * carries 0, a negative and two positives because the dispatch's
	 * `dp == 0` arm tests `> 0` while the arm selection tests `!= 0`, and
	 * the K56flex gate byte is swept because the K56flex arm needs BOTH
	 * its inputs.
	 */
	diff_begin("VPcmV34InitiateRetrain: four gates crossed with every "
		   "datapump code");
	{
		static const int v90_in[] = { 0, -1, 1, 2 };
		static const int gate_in[] = { 0, 1, -1 };
		static const int k56_in[] = { 0, 1, -1 };
		static const unsigned char k56gate_in[] = { 0, 1 };
		static const unsigned char dp_in[] = {
			0, 1, 33, 34, 35, 55, 56, 57, 89, 90, 91, 92, 93, 255
		};

		tag = 1000;
		for (i = 0; i < sizeof(v90_in) / sizeof(v90_in[0]); i++)
		for (j = 0; j < sizeof(gate_in) / sizeof(gate_in[0]); j++)
		for (k = 0; k < sizeof(k56_in) / sizeof(k56_in[0]); k++)
		for (l = 0; l < sizeof(k56gate_in) / sizeof(k56gate_in[0]); l++)
		for (n = 0; n < sizeof(dp_in) / sizeof(dp_in[0]); n++) {
			struct rt_case c = rt_base;

			c.v90rx = v90_in[i];
			c.gate = gate_in[j];
			c.k56rx = k56_in[k];
			c.k56gate = k56gate_in[l];
			c.dp = dp_in[n];
			run_case(&c, tag++);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The rate clamp.  Both bounds are divided UNSIGNED by 2400, capped at
	 * 14, and then the max is raised to the min if it is below it -- in
	 * that order, so a pair that is both above 14 and inverted takes two
	 * fix-ups.  A max of exactly 1 additionally sets +0x359a.
	 *
	 * `min_rate` and `max_rate` are swept together because the fix-up is a
	 * comparison between them; the pairs below cover both orders, both
	 * sides of the cap on each, exact multiples of 2400 and values between
	 * them, and the top of the unsigned range -- which is where a signed
	 * division would give a different answer.
	 */
	diff_begin("VPcmV34InitiateRetrain: the rate clamp");
	{
		static const unsigned pair[][2] = {
			{ 0u, 0u },
			{ 0u, 2400u },
			{ 2400u, 2400u },		/* max index 1     */
			{ 0u, 2400u },
			{ 2400u, 0u },			/* inverted at 1   */
			{ 2399u, 4799u },
			{ 4800u, 2400u },		/* inverted        */
			{ 33600u, 33600u },		/* both exactly 14 */
			{ 36000u, 33600u },		/* min over the cap*/
			{ 33600u, 36000u },		/* max over the cap*/
			{ 100000u, 100000u },
			{ 0u, 100000u },
			{ 100000u, 0u },
			{ 0x80000000u, 0x80000000u },	/* signed would be -*/
			{ 0xffffffffu, 0u },
			{ 0u, 0xffffffffu },
			{ 0xffffffffu, 0xffffffffu },
			{ 2400u, 4800u },
			{ 4801u, 4800u }
		};

		tag = 2000;
		for (i = 0; i < sizeof(pair) / sizeof(pair[0]); i++) {
			struct rt_case c = rt_base;

			c.min_rate = pair[i][0];
			c.max_rate = pair[i][1];
			run_case(&c, tag++);

			/*
			 * And again on an arm that must NOT compute them, so
			 * "the clamp ran" and "the designer was called" are
			 * different results.
			 */
			c.v90rx = 1;
			c.gate = 1;
			run_case(&c, tag++);

			/* And on the K56flex arm, which also skips it. */
			c.v90rx = 0;
			c.gate = 0;
			c.k56rx = 1;
			c.k56gate = 1;
			run_case(&c, tag++);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The disconnect threshold.  The level is biased by 0x30 and rejected
	 * UNSIGNED, so both a negative index and one above 7 fall back on
	 * ENTRY 3 -- not on entry 0 and not on entry 7, which is what a
	 * min/max clamp would give and what makes this sweep worth having.
	 * Every one of the eight in-range levels is driven, plus both
	 * neighbours of each end and both extremes of the int.
	 */
	diff_begin("VPcmV34InitiateRetrain: the disconnect threshold index");
	{
		static const int level_in[] = {
			-0x31, -0x30, -0x2f, -0x2e, -0x2d, -0x2c, -0x2b,
			-0x2a, -0x29, -0x28, -1, 0, 1, 7, 8,
			(-0x7fffffff - 1), 0x7fffffff
		};

		tag = 3000;
		for (i = 0; i < sizeof(level_in) / sizeof(level_in[0]); i++) {
			struct rt_case c = rt_base;

			c.min_level = level_in[i];
			run_case(&c, tag++);
		}

		/*
		 * ANTI-VACUITY, and it is the check finding 247 says to make:
		 * that this fixture can tell the eight entries apart at all.
		 * Two in-range levels whose table entries differ must leave
		 * different floors behind, or every case above proved nothing.
		 */
		{
			struct rt_case c = rt_base;
			int f0, f3, f7;

			c.min_level = -0x30;
			drive(&c);
			f0 = get_int_a(OB_ENERGY_FLOOR);
			c.min_level = -0x2d;
			drive(&c);
			f3 = get_int_a(OB_ENERGY_FLOOR);
			c.min_level = -0x29;
			drive(&c);
			f7 = get_int_a(OB_ENERGY_FLOOR);

			diff_eq_int("entry 0 is the table's entry 0",
				    f0, V34DisconnectThreshTable[0], 0);
			diff_eq_int("entry 3 is the table's entry 3",
				    f3, V34DisconnectThreshTable[3], 3);
			diff_eq_int("entry 7 is the table's entry 7",
				    f7, V34DisconnectThreshTable[7], 7);
			diff_eq_int("and the three are not all the same",
				    (f0 != f3) && (f3 != f7), 1, 0);

			/* Out of range on both sides is entry 3, not an end. */
			c.min_level = -0x31;
			drive(&c);
			diff_eq_int("below the range falls back on entry 3",
				    get_int_a(OB_ENERGY_FLOOR), f3, -0x31);
			c.min_level = 0;
			drive(&c);
			diff_eq_int("above the range falls back on entry 3",
				    get_int_a(OB_ENERGY_FLOOR), f3, 0);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The two delays, swept across the 16-bit truncation.
	 *
	 * `filt_delay` becomes `((d + 2) >> 2) + 0x22` stored as a short, so
	 * it wraps above about 131,000 and below about -131,000; the shift is
	 * ARITHMETIC, so -1, -2, -3 and -5 do not all give the same answer as
	 * a divide would.  `ext_delay` becomes `0x610 - d` as a short and is
	 * also what the echo canceller's delay is built from, so the same
	 * sweep drives both.
	 */
	diff_begin("VPcmV34InitiateRetrain: the two delays and their "
		   "truncation");
	{
		static const int delay_in[] = {
			0, 1, 2, 3, 4, 5, -1, -2, -3, -4, -5, -6,
			64, 200, 0x610, 0x611, 1552, 32767, 32768,
			131044, 131048, 131052, -131052, -131048,
			1000000, -1000000, 0x7ffffffe, 0x7fffffff,
			(-0x7fffffff - 1)
		};

		tag = 4000;
		for (i = 0; i < sizeof(delay_in) / sizeof(delay_in[0]); i++) {
			struct rt_case c = rt_base;

			c.filt_delay = delay_in[i];
			run_case(&c, tag++);

			c = rt_base;
			c.ext_delay = delay_in[i];
			run_case(&c, tag++);
		}

		/*
		 * ANTI-VACUITY: the truncation has to be REACHED, or the
		 * "print the field, not the arithmetic" reading above is
		 * untested.  A filt delay of 131,052 gives 0x8000 in a short
		 * and 32,802 in an int.
		 */
		{
			struct rt_case c = rt_base;
			int whole;

			c.filt_delay = 131052;
			whole = (((c.filt_delay + 2) >> 2) + 0x22);
			drive(&c);
			diff_eq_int("the filt delay arithmetic left a short",
				    whole > 32767, 1, c.filt_delay);
			diff_eq_int("and the field kept the low 16 bits",
				    (int)get_short_a(OB_FILT_DELAY),
				    (int)(short)whole, c.filt_delay);
			diff_eq_int("which is not the untruncated number",
				    (int)get_short_a(OB_FILT_DELAY) != whole,
				    1, c.filt_delay);

			c = rt_base;
			c.ext_delay = -100000;
			whole = 0x610 - c.ext_delay;
			drive(&c);
			diff_eq_int("the dma delay arithmetic left a short",
				    whole > 32767, 1, c.ext_delay);
			diff_eq_int("and that field kept the low 16 bits",
				    (int)get_short_a(OB_F25C),
				    (int)(short)whole, c.ext_delay);
			diff_eq_int("which is not the untruncated number",
				    (int)get_short_a(OB_F25C) != whole, 1,
				    c.ext_delay);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The remaining single-input forks: the sensitive-ISP notice, the
	 * configuration's two permission bits, `status` on the V.90 arm, the
	 * originate/answer flag that picks the +0xac28 group, and the short
	 * that scales +0x254.
	 */
	diff_begin("VPcmV34InitiateRetrain: the sensitive-ISP notice");
	{
		static const int sens_in[] = { 0, 1, -1, 0x7fffffff };
		static const unsigned char isp_in[] = {
			0x00, 0x08, 0xf7, 0xff, 0xe0
		};

		tag = 5000;
		for (i = 0; i < sizeof(sens_in) / sizeof(sens_in[0]); i++)
		for (j = 0; j < sizeof(isp_in) / sizeof(isp_in[0]); j++) {
			struct rt_case c = rt_base;

			c.sens = sens_in[i];
			c.cfg_isp = isp_in[j];
			run_case(&c, tag++);
		}
	}
	rc |= diff_end();

	diff_begin("VPcmV34InitiateRetrain: what the configuration permits");
	{
		static const unsigned char flags_in[] = {
			0x00, 0x08, 0x10, 0x18, 0xe7, 0xff
		};
		static const unsigned char dp_in[] = { 0, 34, 56, 90, 92, 7 };
		static const int v90_in[] = { 0, 1 };

		tag = 6000;
		for (i = 0; i < sizeof(flags_in) / sizeof(flags_in[0]); i++)
		for (j = 0; j < sizeof(dp_in) / sizeof(dp_in[0]); j++)
		for (k = 0; k < sizeof(v90_in) / sizeof(v90_in[0]); k++) {
			struct rt_case c = rt_base;

			c.cfg_flags = flags_in[i];
			c.dp = dp_in[j];
			c.v90rx = v90_in[k];
			run_case(&c, tag++);
		}
	}
	rc |= diff_end();

	diff_begin("VPcmV34InitiateRetrain: status, the session type and the "
		   "modem side");
	{
		static const int status_in[] = { 0, 1, 2, 3, -1 };
		static const unsigned char dp_in[] = { 0, 34, 56, 90, 92 };

		tag = 7000;
		for (i = 0; i < sizeof(status_in) / sizeof(status_in[0]); i++)
		for (j = 0; j < sizeof(dp_in) / sizeof(dp_in[0]); j++) {
			struct rt_case c = rt_base;

			c.status = status_in[i];
			c.dp = dp_in[j];
			run_case(&c, tag++);
		}

		/*
		 * ANTI-VACUITY, and this is the check the fixture's one held
		 * constant makes necessary.
		 *
		 * `V90Modem::side` is 2 on every case above, so
		 * `V90Modem::setSessionFlag` stops at the modem instead of
		 * walking into a modulator or demodulator this fixture does
		 * not build -- that chain is wave 2's and
		 * test/unit/t_v90sessionflag.c is where it is tested.  WHAT
		 * MUST STILL BE VISIBLE is that `setPcmSessionType` ran at
		 * all and ran with the right argument, and all three of its
		 * effects are inside blocks this file compares: the session
		 * type at +0x611c, the modem's flag at +0x6110 and the byte
		 * it writes through +0x612c.  If V.90 and V.92 left the same
		 * session behind, the two arms would be indistinguishable and
		 * every case above would have proved nothing about them.
		 */
		{
			struct rt_case c = rt_base;
			unsigned char s90[16], s92[16], snone[16];
			unsigned char p90, p92;

			c.dp = 90;
			drive(&c);
			memcpy(s90, sa.sess + SS_SESSFLAG, sizeof(s90));
			p90 = sa.v92p2[V92P2_CAPLOCAL];
			c.dp = 92;
			drive(&c);
			memcpy(s92, sa.sess + SS_SESSFLAG, sizeof(s92));
			p92 = sa.v92p2[V92P2_CAPLOCAL];
			c.dp = 34;
			drive(&c);
			memcpy(snone, sa.sess + SS_SESSFLAG, sizeof(snone));

			diff_eq_int("V.90 and V.92 leave different sessions",
				    memcmp(s90, s92, sizeof(s90)) != 0, 1, 0);
			diff_eq_int("and V.34 leaves a third thing behind",
				    memcmp(snone, s90, sizeof(s90)) != 0
				    && memcmp(snone, s92, sizeof(s92)) != 0,
				    1, 0);
			diff_eq_int("V.90 set the session type to 0",
				    (int)s90[SS_PCMTYPE - SS_SESSFLAG], 0, 90);
			diff_eq_int("V.92 set the session type to 1",
				    (int)s92[SS_PCMTYPE - SS_SESSFLAG], 1, 92);
			diff_eq_int("and the V.92 record saw the argument",
				    (int)p92 - (int)p90, 1, 0);
		}
	}
	rc |= diff_end();

	diff_begin("VPcmV34InitiateRetrain: the +0xac1c block");
	{
		static const short role_in[] = {
			0, 0x64, 0x65, 0x66, 0x67, -1, 0x7fff
		};
		static const short scale_in[] = {
			0, 1, -1, 2, 100, -100, 0x7fff, (short)0x8000
		};

		tag = 8000;
		for (i = 0; i < sizeof(role_in) / sizeof(role_in[0]); i++)
		for (j = 0; j < sizeof(scale_in) / sizeof(scale_in[0]); j++) {
			struct rt_case c = rt_base;

			c.f359c = role_in[i];
			c.f35a4 = scale_in[j];
			run_case(&c, tag++);
		}

		/*
		 * ANTI-VACUITY: the three arms of the +0xac28 fork must leave
		 * DIFFERENT memory behind, or the sweep above compared the
		 * fill pattern with itself.
		 */
		{
			struct rt_case c = rt_base;
			unsigned char n65[8], n66[8], nother[8];

			c.f359c = 0x65;
			drive(&c);
			memcpy(n65, (unsigned char *)&oa + OB_FAC1C + 0x0c, 8);
			c.f359c = 0x66;
			drive(&c);
			memcpy(n66, (unsigned char *)&oa + OB_FAC1C + 0x0c, 8);
			c.f359c = 0x64;
			drive(&c);
			memcpy(nother, (unsigned char *)&oa + OB_FAC1C + 0x0c,
			       8);

			diff_eq_int("0x65 and 0x66 differ at +0xac28",
				    memcmp(n65, n66, 8) != 0, 1, 0);
			diff_eq_int("0x65 and neither differ at +0xac28",
				    memcmp(n65, nother, 8) != 0, 1, 0);
			diff_eq_int("0x66 and neither differ at +0xac28",
				    memcmp(n66, nother, 8) != 0, 1, 0);
		}

		/* And that the scale really reached +0x254. */
		{
			struct rt_case c = rt_base;
			short s0, s1;

			c.f35a4 = 0;
			drive(&c);
			s0 = get_short_a(OB_F0254);
			c.f35a4 = 1;
			drive(&c);
			s1 = get_short_a(OB_F0254);
			diff_eq_int("+0x254 is 10000 at scale 0", (int)s0,
				    10000, 0);
			diff_eq_int("+0x254 steps by 336",
				    (int)(short)(s1 - s0), 336, 1);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The handshake's own two inputs, so that its mode-1 fork is exercised
	 * from here as well: which retrain counter it bumps depends on bit 6
	 * of the receiver's flags OR the byte at +0xac17, and the timer guard
	 * is an unsigned difference.
	 */
	diff_begin("VPcmV34InitiateRetrain: what it hands the handshake");
	{
		static const unsigned short rxflag_in[] = { 0x0000, 0x0040 };
		static const unsigned char ac17_in[] = { 0, 1 };
		static const int base_in[] = { 0, 50000, 0x176ff, 0x17700 };

		tag = 9000;
		for (i = 0; i < sizeof(rxflag_in) / sizeof(rxflag_in[0]); i++)
		for (j = 0; j < sizeof(ac17_in) / sizeof(ac17_in[0]); j++)
		for (k = 0; k < sizeof(base_in) / sizeof(base_in[0]); k++) {
			struct rt_case c = rt_base;

			c.rxflags = rxflag_in[i];
			c.ac17 = ac17_in[j];
			c.timer_base = base_in[k];
			c.timer_mark = 1;
			run_case(&c, tag++);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * And the checks that the holes above were holes in something.
	 */
	diff_begin("VPcmV34InitiateRetrain: the fixture reached what it "
		   "skipped");
	{
		for (i = 0; i < NPTR; i++)
			diff_eq_int("skipped pointer +0x%lx was never written",
				    saw_ptr_skip[i], 1, (long)ptr_skip[i]);
		diff_eq_int("the transcript was non-empty at least once",
			    transcript_seen, 1, 0);
	}
	rc |= diff_end();

	return rc;
}
