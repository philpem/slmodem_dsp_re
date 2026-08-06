/*
 * t_v34info1d.c -- differential test of `V34GiveINFO1dBits`.
 *
 * NOT PART OF t_v34info.c, AND NOT LIFTED FROM t_v34info1a.cpp EITHER.  The
 * hand-over pointed at t_v34info1a.cpp because that file stands a real 32 KB
 * `VPcmFloModem` up beside the V.34 object; the reason it has to is that
 * `V34SetINFO1aBits` calls a C++ member.  `V34GiveINFO1dBits` calls none --
 * its only outward call is `VPcmV34InitiateRetrain`, which is `extern "C"` --
 * so this test is C, and the fixture it needs is `t_v34retrain.c`'s: the
 * SEVEN-BLOCK object graph that function walks, byte arrays throughout.
 * t_v34info.c's single 0x6140-byte session is not enough, because the retrain
 * reaches the demodulator, the constellation designer and the configuration
 * through it.
 *
 * (The function under test still ended up in `src/pump/v34/v34pcmmain.cpp`
 * rather than in `v34info.c`, because the 64-bit interop tier links every
 * `.c` under `src/` and would then need `VPcmV34InitiateRetrain` in a build
 * with no C++ in it.  That is about where the CALLEE lives, not about this
 * test's language: nothing here names a C++ type.)
 *
 *     obj +0x3548 -> session   +0x175c -> demodulator +0x208 -> designer
 *                              +0x610c -> pcm receiver
 *                              +0x612c -> V.92 phase 2 record
 *                              +0x6bd0    an EMBEDDED V92EchoCanceller
 *     obj +0xac18 -> k56flex modem
 *     obj +0xac3c -> configuration
 *
 * WHAT THIS FUNCTION MAKES HARD.  Its whole state footprint is one int in the
 * session, one byte in the V.34 object and whatever the retrain does.  So:
 *
 *   - THE RETURN VALUE IS COMPARED, and it has to be: a reconstruction that
 *     returned `session + 0x611c` read back -- which is what its sibling
 *     `V34GiveINFO1aBits` really does return -- agrees on every case except
 *     two.  Both are in the tables below.  The retraining path returns 1 with
 *     the flag back at 0 (`VPcmV34InitiateRetrain`'s `DP_V90` arm calls
 *     `setPcmSessionType(0)`), and a wanted-but-barred upstream returns 0
 *     with the flag left at 1.
 *
 *   - THE TRANSCRIPTS ARE COMPARED AT THREE LEVELS, 0, 1 and 2.  Every gate
 *     in the function is `> 1`; level 1 is what separates that from `>= 1`
 *     and level 2 from `> 2`.  Level 3 would add nothing -- there is no
 *     `DSPLIB_DEBUG_VERBOSE()` site here.
 *
 *   - AND THE TRANSCRIPT IS THE ONLY TIER THAT SEES FOUR WIDTHS.  The ten
 *     message shorts are printed `movzwl`, the capability byte `movzbl`,
 *     `remote_v92` `movswl`, and the message bit is printed as the masked
 *     value -- 32, not 1.  None of the four changes a single byte of state.
 *     So the tables drive bit 15 in every message index, a capability byte
 *     above 0x7f, a negative `remote_v92`, and both 0 and 0x20 in index 7.
 *
 * WHAT "DRIVE HIGH BYTES" MEANS HERE, AND WHAT IT DOES NOT.  Finding 312 warns
 * that `V34SetINFO1aBits` CLEARS index 7 with `and $0xdf` on a zero-extended
 * short, so its high byte is not inert.  This function only READS index 7, and
 * only through `testb $0x20` on the low byte -- so the high byte is inert for
 * the decision and live only for the print.  The `bits7` column drives it both
 * ways to establish that rather than inherit it: 0xffdf and 0xff20 differ from
 * 0x00df and 0x0020 in the transcript and in nothing else.
 *
 * EVERY BLOCK IS COMPARED WHOLE, with the pointer holes `t_v34retrain.c`
 * established, and `saw_ptr_skip` asserts at the end that each hole was a hole
 * in something -- which for most of the list means the retraining path really
 * was taken.  The counters below do the same job for the input columns.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34info.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"

extern unsigned int ref_dsplibs_debug_level;

extern int ref_V34GiveINFO1dBits(void *obj, const short *bits);
extern void ref_V34InitializeImplementationSpecific(void *obj);

/* The three handshake state words, seeded in range: see t_v34retrain.c. */
#define HSI_MICROSTATE	0x3592
#define HSI_RXSTATE	0x3594
#define HSI_TXSTATE	0x3596

/* Object offsets this file seeds or reads. */
#define OB_STATUS	0x0000
#define OB_V90RX	0x024c
#define OB_K56RX	0x0250
#define OB_TIMER_BASE	0x0238
#define OB_TIMER_MARK	0x0248
#define OB_RXFLAGS	(0x0264 + 0x122)
#define OB_P3548	0x3548
#define OB_F359C	0x359c
#define OB_F35A4	0x35a4
#define OB_REMOTE_V92	0xabc8
#define OB_FAC00	0xac00
#define OB_PAC18	0xac18
#define OB_PAC3C	0xac3c

/* Session offsets. */
#define SS_DEMOD	0x175c
#define SS_PCM		0x610c
#define SS_SIDE		0x6114
#define SS_PCMTYPE	0x611c
#define SS_GATE		0x6120
#define SS_V92P2	0x612c

#define DM_DESIGNER	0x0208
#define PCM_SENS	0x04f8
#define K56_GATE	0x0008

/* The V.92 phase 2 record's local capability byte -- "local cap - %d". */
#define V92P2_CAPLOCAL	0x0011

/* Configuration offsets.  CF_V92LITE is the byte whose SIGN gates the retrain. */
#define CF_FLAGS	0x00
#define CF_V92LITE	0x02
#define CF_MIN_RATE	0x30
#define CF_MAX_RATE	0x34
#define CF_ISP		0x50
#define CF_MIN_LEVEL	0x60
#define CF_FILT_DELAY	0x64
#define CF_EXT_DELAY	0x68

/* Lengths are generous rather than measured, for t_v34retrain.c's reason. */
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
 * THE MESSAGE AND ITS SLACK ARE ONE OBJECT, not two statics: nothing requires
 * the linker to lay two statics out adjacently, and slack that did not follow
 * the buffer it guards would be decoration.  This function reads indices 0..9
 * and writes none of them; the guard is what would catch a store.
 */
#define MSG_SLACK	16
static struct { short o[V34_INFO_MSG_SHORTS]; unsigned char g[MSG_SLACK]; }
	ma, mb;

/*
 * Pointer-sized fields the two sides necessarily disagree about.  The list is
 * t_v34retrain.c's, because the retraining path runs exactly what that file
 * runs: `V34InitializeImplementationSpecific` and `v34handshakinit` mode 1.
 * The first three are the fixture's own and the memory behind each is compared
 * per case; the rest are reached only when the retrain is taken, which is what
 * makes `saw_ptr_skip` an anti-vacuity check on the case mix and not just on
 * the hole list.
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

/*
 * Everything a case chooses.  ONE FIELD PER INPUT, so no two can be driven
 * from one variable -- the fixture defect of findings 116b, 123 and 171.  The
 * first five are this function's own; the rest exist so that the retraining
 * tail is a real call rather than a fault, and are held at one value except
 * where a comment says otherwise.
 */
struct gi_case {
	int		v90rx;		/* obj +0x24c                      */
	unsigned char	caplocal;	/* v92 phase 2 +0x11               */
	short		remote_v92;	/* obj +0xabc8                     */
	unsigned short	bits7;		/* message index 7                 */
	unsigned char	cfg2;		/* cfg +0x02, tested by its SIGN   */
	int		pcmtype;	/* session +0x611c ON ENTRY        */
	unsigned short	msgbase;	/* message fill, index 0           */
	unsigned short	msgstep;	/* message fill, per index         */
	int		hi_index;	/* -1, or the index given bit 15   */

	int		status;		/* obj +0x00; 2 makes the retrain  */
					/* write +0xac00 itself            */
	int		k56rx;		/* obj +0x250                      */
	int		gate;		/* session +0x6120                 */
	unsigned char	k56gate;	/* k56flex +0x08                   */
	unsigned int	min_rate;	/* cfg +0x30, bits per second      */
	unsigned int	max_rate;	/* cfg +0x34                       */
	int		min_level;	/* cfg +0x60, biased by 0x30       */
	int		filt_delay;	/* cfg +0x64                       */
	int		ext_delay;	/* cfg +0x68                       */
	unsigned char	cfg_flags;	/* cfg +0x00, bits 3 and 4         */
	unsigned char	cfg_isp;	/* cfg +0x50                       */
	int		sens;		/* pcm receiver +0x4f8             */
	short		f359c;		/* obj +0x359c, originate/answer   */
	short		f35a4;		/* obj +0x35a4                     */
	int		mside;		/* session +0x6114, V90Modem::side */
};

/*
 * The default.  It is `t_v34retrain.c`'s `rt_base` for the retrain's own
 * inputs -- `mside` 2 so that `V90Modem::setSessionFlag` stops at the modem
 * rather than walking into a modulator this fixture does not model -- and the
 * "PCM upstream wanted and permitted" corner for this function's own, so that
 * a case which changes nothing else takes the longest path.
 */
static const struct gi_case gi_base = {
	1,			/* v90rx                                    */
	1,			/* caplocal                                 */
	1,			/* remote_v92                               */
	0x0020,			/* bits7: the bit set, high byte clear      */
	0x00,			/* cfg2: bit 7 clear, so the retrain runs   */
	0,			/* pcmtype on entry                         */
	0x1357, 0x0249,		/* msgbase, msgstep                         */
	-1,			/* hi_index: no forced bit 15               */

	0,			/* status: NOT 2, so +0xac00 has one writer */
	0,			/* k56rx                                    */
	0,			/* gate                                     */
	0,			/* k56gate                                  */
	4800u, 33600u,		/* min_rate, max_rate                       */
	-0x30,			/* min_level -> index 0                     */
	64, 200,		/* filt_delay, ext_delay                    */
	0x18,			/* cfg_flags: both v90 and flex allowed     */
	0x41,			/* cfg_isp: bit 3 clear, other bits set     */
	0,			/* sens                                     */
	0x65,			/* f359c                                    */
	3,			/* f35a4                                    */
	2			/* mside                                    */
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
	snprintf(msg, sizeof(msg), "%s: the message", what);
	compare_block(msg, (const unsigned char *)&ma,
		      (const unsigned char *)&mb, sizeof(ma), 0, 0, tag);
}

/* Seed one side's out-of-object blocks. */
static void
seed_side(struct side *s, const struct gi_case *c)
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
	memcpy(s->sess + SS_PCMTYPE, &c->pcmtype, sizeof(c->pcmtype));
	memcpy(s->pcmrx + PCM_SENS, &c->sens, sizeof(c->sens));

	s->v92p2[V92P2_CAPLOCAL] = c->caplocal;

	s->k56[K56_GATE] = c->k56gate;

	s->cfg[CF_FLAGS] = c->cfg_flags;
	s->cfg[CF_V92LITE] = c->cfg2;
	s->cfg[CF_ISP] = c->cfg_isp;
	memcpy(s->cfg + CF_MIN_RATE, &c->min_rate, sizeof(c->min_rate));
	memcpy(s->cfg + CF_MAX_RATE, &c->max_rate, sizeof(c->max_rate));
	memcpy(s->cfg + CF_MIN_LEVEL, &c->min_level, sizeof(c->min_level));
	memcpy(s->cfg + CF_FILT_DELAY, &c->filt_delay, sizeof(c->filt_delay));
	memcpy(s->cfg + CF_EXT_DELAY, &c->ext_delay, sizeof(c->ext_delay));
}

static void
seed_msg(const struct gi_case *c)
{
	unsigned i;

	memset(&ma, 0x5a, sizeof(ma));
	for (i = 0; i < V34_INFO_MSG_SHORTS; i++)
		ma.o[i] = (short)(unsigned short)(c->msgbase
						  + (unsigned short)(i
								     * c->msgstep));
	if (c->hi_index >= 0 && c->hi_index < V34_INFO_MSG_SHORTS)
		ma.o[c->hi_index] = (short)(unsigned short)
			(ma.o[c->hi_index] | 0x8000);
	ma.o[7] = (short)c->bits7;

	memcpy(&mb, &ma, sizeof(mb));
}

/* Return values, one per side, kept for the comparison the tables need. */
static int ret_a, ret_b;

/* Anti-vacuity counters: every one is asserted non-zero at the end. */
static int saw_retrain;		/* the retraining path was taken           */
static int saw_barred;		/* wanted, but cfg bit 7 said no           */
static int saw_no_v90;		/* the early return, with the flag cleared */
static int saw_cap_hi;		/* a capability byte above 0x7f            */
static int saw_remote_neg;	/* a negative remote_v92                   */
static int saw_bits7_hi;	/* index 7 with a non-zero high byte       */
static int saw_msg_hi;		/* a printed short with bit 15 set         */
static int saw_flag_left;	/* the flag ended at 1 and the return at 0 */
static int transcript_seen;

static void
drive(const struct gi_case *c)
{
	setup();

	/*
	 * `v34modeminit` cleans both echo cancellers through five pointers
	 * each, and aiming them is `V34InitializeImplementationSpecific`'s job
	 * rather than the handshake's -- without it the retraining path's
	 * first dereference faults.  t_v34retrain.c does the same.
	 */
	V34InitializeImplementationSpecific(&oa);
	ref_V34InitializeImplementationSpecific(ob);

	poke_ptr(OB_P3548, sa.sess, sb.sess);
	poke_ptr(OB_PAC18, sa.k56, sb.k56);
	poke_ptr(OB_PAC3C, sa.cfg, sb.cfg);

	poke_int(OB_STATUS, c->status);
	poke_int(OB_V90RX, c->v90rx);
	poke_int(OB_K56RX, c->k56rx);
	poke_int(OB_TIMER_BASE, 0);
	poke_int(OB_TIMER_MARK, 0);
	poke_short(OB_F359C, c->f359c);
	poke_short(OB_F35A4, c->f35a4);
	poke_short(OB_REMOTE_V92, c->remote_v92);
	poke_short(OB_RXFLAGS, 0);
	poke_byte(OB_FAC00, 0x5c);	/* neither 0 nor 1 */

	poke_short(HSI_MICROSTATE, V34HS_PHASE1);
	poke_short(HSI_RXSTATE, V34HS_PHASE2);
	poke_short(HSI_TXSTATE, V34HS_TONE_AB);

	seed_side(&sa, c);
	seed_side(&sb, c);
	seed_msg(c);

	ret_a = V34GiveINFO1dBits(&oa, ma.o);
	ret_b = ref_V34GiveINFO1dBits(ob, mb.o);
}

static void
note_case(const struct gi_case *c)
{
	int flag;
	unsigned i;

	memcpy(&flag, sa.sess + SS_PCMTYPE, sizeof(flag));

	if (ret_a == 1)
		saw_retrain = 1;
	if (ret_a == 0 && c->v90rx != 0 && flag == 1)
		saw_barred = 1;
	if (c->v90rx == 0 && flag == 0)
		saw_no_v90 = 1;
	if (ret_a == 0 && flag != 0)
		saw_flag_left = 1;

	/*
	 * The four width columns are only claims about the TRANSCRIPT, and
	 * nothing is printed without a V.90 receiver -- so they are counted
	 * only on cases that reach the prints.  Counting them everywhere would
	 * be finding 247's defect: an assertion that holds for a reason other
	 * than the one it names.
	 */
	if (c->v90rx == 0)
		return;

	if (c->caplocal >= 0x80)
		saw_cap_hi = 1;
	if (c->remote_v92 < 0)
		saw_remote_neg = 1;
	if ((c->bits7 & 0xff00u) != 0)
		saw_bits7_hi = 1;
	for (i = 0; i < 10; i++)
		if (((unsigned short)ma.o[i] & 0x8000u) != 0)
			saw_msg_hi = 1;
}

/*
 * One case at three debug levels.  The state comparison is made at every one
 * of them, because the print sites re-read the session flag and a
 * reconstruction that printed a stale copy would still have to store the same
 * thing; the transcripts are what the levels are really for.
 */
static void
run_case(const struct gi_case *c, long tag)
{
	static const unsigned int level[] = { 0, 1, 2 };
	unsigned l;

	for (l = 0; l < sizeof(level) / sizeof(level[0]); l++) {
		char what[64];

		snprintf(what, sizeof(what), "GiveINFO1dBits at level %u",
			 level[l]);

		dsplib_debug_capture_on = 1;
		dsplib_debug_capture_reset();
		dsplibs_debug_level = level[l];
		ref_dsplibs_debug_level = level[l];

		drive(c);

		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;

		compare_all(what, tag);
		diff_eq_int("GiveINFO1dBits return value", ret_a, ret_b, tag);
		diff_eq_int("GiveINFO1dBits transcript",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);

		if (level[l] == 0)
			diff_eq_int("GiveINFO1dBits is silent at level 0",
				    (int)dsplib_debug_capture_lines(0), 0, tag);
		if (level[l] == 1)
			diff_eq_int("GiveINFO1dBits is silent at level 1",
				    (int)dsplib_debug_capture_lines(0), 0, tag);
		if (level[l] == 2 && dsplib_debug_capture_lines(0) > 0)
			transcript_seen = 1;

		if (level[l] == 0)
			note_case(c);
	}
}

int
main(void)
{
	int rc = 0;
	unsigned i, j, k, l, n;
	long tag;

	/*
	 * ------------------------------------------------------------------
	 * The three-way conjunction, crossed with the receiver test and the
	 * configuration byte.
	 *
	 * FULL CROSS PRODUCT AND NOT ONE AT A TIME, because the whole point of
	 * the group is that it is an `&&`: a reconstruction using `||` agrees
	 * with the object on every case in which at most one input is false.
	 *
	 * The values are chosen to separate the widths as well as the logic.
	 * `remote_v92` carries 0x100 -- non-zero as a short with a zero low
	 * byte -- so a byte-wide test fails there, and -1 so the `movswl` in
	 * the print is exercised.  `caplocal` carries 0x80 and 0xff so the
	 * `movzbl` is.  `bits7` carries the bit set and clear with the high
	 * byte both zero and non-zero, which is what shows the mask never
	 * reaches it.  `cfg2` varies bit 5 independently of bit 7, because
	 * `VPcmV34Progress` tests bit 5 of the same byte and only bit 7 may
	 * matter here.  `v90rx` carries 0x10000 so a 16-bit read of it fails.
	 */
	diff_begin("V34GiveINFO1dBits: the three-way conjunction, the "
		   "receiver test and the configuration byte");
	{
		static const int v90_in[] = { 0, 1, -1, 0x10000 };
		static const unsigned char cap_in[] = { 0, 1, 0x80, 0xff };
		static const short rem_in[] = { 0, 1, -1, 0x100 };
		static const unsigned short b7_in[] = {
			0x0000, 0x0020, 0x00df, 0xff00, 0xff20, 0xffdf
		};
		static const unsigned char cfg2_in[] = {
			0x00, 0x20, 0x7f, 0x80, 0xa0, 0xff
		};

		tag = 1000;
		for (i = 0; i < sizeof(v90_in) / sizeof(v90_in[0]); i++)
		for (j = 0; j < sizeof(cap_in) / sizeof(cap_in[0]); j++)
		for (k = 0; k < sizeof(rem_in) / sizeof(rem_in[0]); k++)
		for (l = 0; l < sizeof(b7_in) / sizeof(b7_in[0]); l++)
		for (n = 0; n < sizeof(cfg2_in) / sizeof(cfg2_in[0]); n++) {
			struct gi_case c = gi_base;

			c.v90rx = v90_in[i];
			c.caplocal = cap_in[j];
			c.remote_v92 = rem_in[k];
			c.bits7 = b7_in[l];
			c.cfg2 = cfg2_in[n];
			run_case(&c, tag++);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The session flag on entry.
	 *
	 * The first statement of the function clears +0x611c BEFORE the
	 * receiver is tested, so the no-receiver path is not a no-op.  A
	 * reconstruction that moved the clear inside the test agrees with the
	 * object on every case whose entry value was already 0 -- so the entry
	 * value is swept, with the receiver both up and down.
	 */
	diff_begin("V34GiveINFO1dBits: the flag is cleared before the "
		   "receiver is tested");
	{
		static const int entry_in[] = { 0, 1, -1, 0x7fffffff, 2 };
		static const int v90_in[] = { 0, 1 };
		static const unsigned char cap_in[] = { 0, 3 };

		tag = 2000;
		for (i = 0; i < sizeof(entry_in) / sizeof(entry_in[0]); i++)
		for (j = 0; j < sizeof(v90_in) / sizeof(v90_in[0]); j++)
		for (k = 0; k < sizeof(cap_in) / sizeof(cap_in[0]); k++) {
			struct gi_case c = gi_base;

			c.pcmtype = entry_in[i];
			c.v90rx = v90_in[j];
			c.caplocal = cap_in[k];
			/* Barred, so the flag survives to be looked at. */
			c.cfg2 = 0x80;
			run_case(&c, tag++);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The ten printed message shorts.
	 *
	 * Bit 15 walked through every index the print reads, and through three
	 * it does not, so "index 10 is not printed" is a measured claim rather
	 * than an assumption.  Nothing here changes a byte of state: the whole
	 * sweep lives or dies on the transcript comparison at level 2.
	 */
	diff_begin("V34GiveINFO1dBits: the ten message shorts, and their "
		   "width");
	{
		static const unsigned short base_in[] = { 0x0001, 0x8001 };

		tag = 3000;
		for (i = 0; i < V34_INFO_MSG_SHORTS + 1; i++)
		for (j = 0; j < sizeof(base_in) / sizeof(base_in[0]); j++) {
			struct gi_case c = gi_base;

			c.hi_index = (i == V34_INFO_MSG_SHORTS) ? -1 : (int)i;
			c.msgbase = base_in[j];
			c.msgstep = 0x1111;
			/* Barred: the print must happen, the retrain must not,
			   so a state change cannot mask a transcript one. */
			c.cfg2 = 0x80;
			run_case(&c, tag++);

			/* And once more with the retrain taken. */
			c.cfg2 = 0x00;
			run_case(&c, tag++);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * +0xac00, and the one input that gives it a second writer.
	 *
	 * `VPcmV34InitiateRetrain`'s `DP_V90` arm writes 1 there itself when
	 * `status` is 2.  Everywhere else this function is the only writer, so
	 * the byte separates "the store happened" from "the retrain happened"
	 * only when `status` is not 2 -- both are swept, and the barred case
	 * is swept beside them because it must leave the byte alone.
	 */
	diff_begin("V34GiveINFO1dBits: +0xac00 and its second writer");
	{
		static const int status_in[] = { 0, 1, 2, 3 };
		static const unsigned char cfg2_in[] = { 0x00, 0x80 };
		static const unsigned char cap_in[] = { 0, 1 };

		tag = 4000;
		for (i = 0; i < sizeof(status_in) / sizeof(status_in[0]); i++)
		for (j = 0; j < sizeof(cfg2_in) / sizeof(cfg2_in[0]); j++)
		for (k = 0; k < sizeof(cap_in) / sizeof(cap_in[0]); k++) {
			struct gi_case c = gi_base;

			c.status = status_in[i];
			c.cfg2 = cfg2_in[j];
			c.caplocal = cap_in[k];
			run_case(&c, tag++);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * The retrain really is `VPcmV34InitiateRetrain(obj, 90)`.
	 *
	 * 90's arm is the only one that calls `setPcmSessionType(0)` and the
	 * only one gated on `status`; 92's sets the flag to 1 instead and 34's
	 * clears `v90_receiver`.  Sweeping the retrain's OWN gating inputs on
	 * the taken path is what separates the datapump code this function
	 * passes from its neighbours -- a mutation to 92 or to 34 changes the
	 * session and the object, not just the transcript.
	 */
	diff_begin("V34GiveINFO1dBits: what the retrain is asked for");
	{
		static const int k56_in[] = { 0, 1, -1 };
		static const int gate_in[] = { 0, 1 };
		static const unsigned char k56gate_in[] = { 0, 1 };
		static const short f359c_in[] = { 0x65, 0x66 };

		tag = 5000;
		for (i = 0; i < sizeof(k56_in) / sizeof(k56_in[0]); i++)
		for (j = 0; j < sizeof(gate_in) / sizeof(gate_in[0]); j++)
		for (k = 0; k < sizeof(k56gate_in) / sizeof(k56gate_in[0]); k++)
		for (l = 0; l < sizeof(f359c_in) / sizeof(f359c_in[0]); l++) {
			struct gi_case c = gi_base;

			c.k56rx = k56_in[i];
			c.gate = gate_in[j];
			c.k56gate = k56gate_in[k];
			c.f359c = f359c_in[l];
			run_case(&c, tag++);
		}
	}
	rc |= diff_end();

	/*
	 * ------------------------------------------------------------------
	 * And the checks that every column above was a column in something.
	 */
	diff_begin("V34GiveINFO1dBits: the fixture reached what it claims");
	{
		for (i = 0; i < NPTR; i++)
			diff_eq_int("skipped pointer +0x%lx was never written",
				    saw_ptr_skip[i], 1, (long)ptr_skip[i]);

		diff_eq_int("the retraining path was never taken",
			    saw_retrain, 1, 0);
		diff_eq_int("a wanted upstream was never barred",
			    saw_barred, 1, 0);
		diff_eq_int("the no-receiver return never happened",
			    saw_no_v90, 1, 0);
		diff_eq_int("the flag never survived a zero return",
			    saw_flag_left, 1, 0);
		diff_eq_int("the capability byte was never above 0x7f",
			    saw_cap_hi, 1, 0);
		diff_eq_int("remote_v92 was never negative",
			    saw_remote_neg, 1, 0);
		diff_eq_int("index 7 never carried a high byte",
			    saw_bits7_hi, 1, 0);
		diff_eq_int("no printed short ever had bit 15 set",
			    saw_msg_hi, 1, 0);
		diff_eq_int("the transcript was non-empty at least once",
			    transcript_seen, 1, 0);
	}
	rc |= diff_end();

	return rc;
}
