/*
 * v34pcmcreate.cpp -- `VPcmV34Create`, the V.PCM construction path's anchor.
 *
 * IT IS PART OF `VPcmV34Main.cpp` LIKE ITS NEIGHBOURS, and that translation
 * unit is now spread over three files here.  `src/pump/v34/v34pcmif.c` holds
 * the exports that are C in everything but their file's extension;
 * `src/pump/v34/v34pcmmain.cpp` holds the half that names a mangled symbol and
 * therefore cannot be C at all.  This is the third, and it exists for a reason
 * that is about the TIER rather than about the object, which is unusual enough
 * to state plainly:
 *
 *     `VPcmV34Create` and `VPcmV34InitiateRetrain` SHARE FIVE VERBATIM BLOCKS
 *     -- the three-armed rate block, the disconnect threshold, the filter and
 *     DMA delays, the echo delay, and the 32 bytes at +0xac1c.  The object has
 *     them twice because the compiler emitted them twice, and a faithful
 *     reconstruction has them twice too.
 *
 *     `mutate.py` matches a mutation's `find` as a substring OF THE WHOLE
 *     SOURCE FILE and reports an anchor matching twice as UNUSABLE -- and
 *     UNUSABLE DOES NOT FAIL A RUN (finding 347).  Putting the two functions in
 *     one file made 44 of `v34retrain`'s anchors match twice, which would have
 *     silently disabled two thirds of that suite while it went on printing
 *     `0 NOT caught`.  `tools/anchorcheck.py` is what caught it, and it scopes
 *     every check to the suite's own source file -- so a separate translation
 *     unit is not a workaround for the tool, it is the unit the tool measures.
 *
 * Factoring the five blocks into shared helpers was the other candidate and is
 * worse: it would move every one of those 44 anchors by a tab, breaking them
 * outright rather than de-duplicating them, and four of the collisions are
 * single statements (`obj->status = 0;`) that no factoring can separate.
 *
 * WHY IT IS C++ AND NOT C.  Seven C++ MEMBER functions --
 * `VPcmFloModem::externalReset`, `K56FlexFloModem::externalReset` and
 * `::setMinMaxRates`, `V90ConstellationDesigner::setMinMaxRates`,
 * `V90Demodulator::enterChannelVerification`, `V92EchoCanceller::setEchoDelay`
 * and `GenericIIR<float,double>::reset`.  A member takes a `this` and has no
 * unmangled form to name, so a C translation unit cannot reach one whatever
 * the link line says: CLAUDE.md's trap, and the third of finding 711's
 * conditions.  The stem differs from `v34pcmif`'s and `v34pcmmain`'s because
 * the Makefile turns `%.c` and `%.cpp` into the same `$(BUILD)/%.o`.
 *
 * `tools/tuattrib.py` has nothing to say about this symbol.  The placement
 * rests on the constraint above plus the interleaving `v34pcmif.c` already
 * records, not on an attribution.
 *
 * Built with the same -fno-exceptions -fno-rtti -nostdinc++ as its two
 * neighbours; nothing here is a class, has a virtual, or allocates.
 *
 * Constants are repeated from `v34pcmmain.cpp` rather than shared, which is
 * the choice that file already made for the same reason: splitting one
 * translation unit across files leaves no private header to put them in.
 */

#include "dsplib/GenericIIR.h"
#include "dsplib/K56FlexFloModem.h"
#include "dsplib/V90ConstellationDesigner.h"
#include "dsplib/V90Demodulator.h"
#include "dsplib/V92EchoCanceller.h"
#include "dsplib/VPcmFloModem.h"
#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/sysdep.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34pcm_tables.h"
#include "dsplib/v34recv.h"
#include "dsplib/vpcm.h"

/*
 * ---------------------------------------------------------------------------
 * The maps, all of them repeated from `v34pcmmain.cpp` except the block this
 * function adds.  See that file for the derivations of the shared ones.
 */
#define SESS_DEMOD		0x175c		/* V90Modem::demodulator     */
#define SESS_GATE		0x6120		/* setPhaseIIinfo's layout   */
#define SESS_ECHO		0x6bd0		/* a V92EchoCanceller, `lea` */
#define DEMOD_DESIGNER		0x0208

#define K56_ENABLED		0x08

#define CFG_V92LITE		0x02
#define CFG_MIN_RATE		0x30		/* bits per second, unsigned */
#define CFG_MAX_RATE		0x34
#define CFG_MIN_LEVEL		0x60		/* signed, biased by 0x30    */
#define CFG_FILT_DELAY		0x64		/* "params initial delay"    */
#define CFG_EXT_DELAY		0x68		/* "ext delay"               */

#define OB_RECEIVER		0x0264
#define OB_F0234		0x0234
#define OB_F0254		0x0254
#define OB_FORCE_LOW_BAUD	0x359a
#define OB_F35A4		0x35a4
#define OB_FILT_DELAY		0xaa7c
#define OB_FAC1C		0xac1c

#define RATE_STEP		2400u
#define RATE_INDEX_MAX		14

/*
 * ---------------------------------------------------------------------------
 * And what `VPcmV34Create` adds to the three maps above.  Same rule as the
 * rest of the file: offset-named unless the object names the field itself.
 *
 * CFG_ANSPCM_LEVEL and CFG_UQTS ARE THE OBJECT'S OWN NAMES -- "VPcmV34Create:
 * ANSpcm level index is %d" and "VPcmV34Create: Uqts index is %d" are printed
 * out of exactly these two words.  Both are loaded as `int` (a plain 32-bit
 * `mov`, not a `movswl`) and both are TRUNCATED TO SHORT at the one place
 * they are used for anything, which is `enterChannelVerification`.
 *
 * CFG_QUICKCONNECT is bit 4 of the byte v34pcmmain already calls CFG_V92LITE
 * for its sign bit and `VPcmV34Progress` reads bit 5 of: a third unrelated
 * reader of one byte, which is why that byte still has no name as a whole.
 */
#define CFG_ANSPCM_LEVEL	0x0c
#define CFG_UQTS		0x10
#define CFG_TXMD		0x14		/* scaled by 2.4 into +0xabfc */
#define CFG_F54			0x54		/* == 14 turns the filter on  */
#define CFG_ENTRANCE		0x70		/* -1 HW, 1 on, anything off  */

#define CFG_QUICKCONNECT	0x10		/* bit 4 of CFG_V92LITE       */

/*
 * The session object again.  SESS_PCMTYPE is the same `int` VPcmFloModem.h
 * describes at +0x611c and `setPcmSessionType` writes; SESS_PCMV92 is the
 * V.92 Phase 2 record POINTER at +0x612c, which is not SESS_PCM at +0x610c.
 * SESS_ENTRANCE is the byte the three-way fork at the bottom of the function
 * sets and the last diagnostic reads back -- "entrance filter applied".
 */
#define SESS_PCMTYPE		0x611c
#define SESS_PCMV92		0x612c
#define SESS_IIR		0x7f28		/* a GenericIIR<float,double> */
#define SESS_ENTRANCE		0x7f5c

/* Inside the V.92 Phase 2 record. */
#define PCMV92_QUICK		0x10
#define PCMV92_FLAG11		0x11
#define PCMV92_COPIED		0x14		/* four bytes, +0x14..+0x17   */

/*
 * The K56flex object's A-law/mu-law selector at +0xc.  THE OBJECT NAMES IT:
 * the one ungated `edprintf` in `VPcmV34Create` is "VPcmV34Main: it is
 * K56Flex session type, (AorMu = %d)" and it prints this word back after
 * storing it.  v34info.c reads `pac18 + 0xc` as an offset for the same reason
 * K56_ENABLED is one -- K56FlexFloModem.h bounds the class at no members.
 */
#define K56_AORMU		0x0c

/* The V.34 object's own, for the regions v34fsk.h leaves unmapped. */
#define OB_F000C		0x000c
#define OB_F0238		0x0238
#define OB_F0248		0x0248		/* = 0xfffe8900               */
#define OB_F0262		0x0262
#define OB_BULK_RING		0x35b8		/* what `bulk_ring` points at */
#define OB_FA248		0xa248
#define OB_FAA80		0xaa80
#define OB_FABC4		0xabc4
#define OB_FABD4		0xabd4		/* three ints, then two shorts */
#define OB_FABD8		0xabd8
#define OB_FABDC		0xabdc
#define OB_FABE4		0xabe4
#define OB_FABE6		0xabe6
#define OB_FABFC		0xabfc
#define OB_FAC12		0xac12
#define OB_FAC14		0xac14
#define OB_FAC17		0xac17

/* Inside `struct v34_receiver`, which v34fsk.h leaves as padding at +0x238. */
#define RX_F238			0x0238

/*
 * ---------------------------------------------------------------------------
 * VPcmV34Create -- wipe the V.34 object, decide which of five session types it
 * is going to be, and hand it to the handshake.
 *
 * WHY IT IS IN THIS FILE AND NOT IN v34pcmif.c BESIDE THE OTHER `VPcmV34*`
 * EXPORTS.  It calls FIVE C++ MEMBER functions -- `VPcmFloModem::externalReset`,
 * `K56FlexFloModem::externalReset` and `::setMinMaxRates`,
 * `V90ConstellationDesigner::setMinMaxRates`,
 * `V90Demodulator::enterChannelVerification`, `V92EchoCanceller::setEchoDelay`
 * and `GenericIIR<float,double>::reset` -- and a member has a `this` and no
 * unmangled form to name, so a C translation unit cannot reach one whatever
 * the link line says (CLAUDE.md's trap, and finding 711's three conditions).
 * `VPcmV34InitiateRetrain` above is here for the weaker version of the same
 * reason and this is the strong one.  `tools/tuattrib.py` has nothing to say
 * about this symbol; the placement rests on that constraint plus the
 * interleaving v34pcmif.c already records, not on an attribution.
 *
 * FIVE ARGUMENTS (finding 1119, correcting 1117's prologue read).  `0x50(%esp)`
 * is read at five sites and `vpcm_create` pushes five slots; the fifth is the
 * session type and it is the function's primary dispatch.
 *
 *     obj          the V.34 object, at root +0x2c
 *     side         0 or 1; becomes +0x359c's 0x65 / 0x66, but see below
 *     ptc          straight into +0x8, and read by nothing here
 *     runtime      the negotiated configuration, kept at +0xac3c
 *     sessionType  0..4, and `vpcm_create` can only pass 0, 1 or 2
 *
 * IT ALWAYS RETURNS 0.  Both `ret`s are reached by `xor %eax,%eax`, so
 * `vpcm_create`'s `test %eax,%eax / jne` failure path is dead.
 *
 * THE SIDE-TO-0x359C POLARITY IS NOT ONE RULE.  Session types 1 and 2 invert
 * the flag before the common store, so they map side 0 to 0x66 where types 0,
 * 3 and 4 map it to 0x65.  A single rule would be wrong for three of the five.
 *
 * THE TWO POINTERS THE MEMSET WOULD DESTROY are read out first and put back:
 * +0x3548 (the `VPcmFloModem`) and +0xac18 (the `K56FlexFloModem`).  +0xac3c
 * is not one of them -- it is the argument, and is stored fresh.
 *
 * THE SECOND MEMSET IS REDUNDANT AND IS THE OBJECT'S.  0x264 + 0x79c is
 * 0xa00, entirely inside the 0xac4c the first one already cleared.  It is
 * transcribed because it is there; `sizeof(struct v34_receiver)` IS 0x79c,
 * which is what says the second one is the receiver rather than a run of
 * bytes that happens to start there.
 *
 * ARMS 3 AND 4 ARE UNREACHABLE FROM WITHIN THIS OBJECT -- deviation D149, and
 * finding 1119 reaches it a third way: `objdump -r` finds exactly one
 * relocation against this symbol, and `vpcm_create` computes its session type
 * as `(x == 0x5c) ? 2 : (x == 0x5a) ? 1 : 0`.  They are written out because a
 * reconstruction has to agree on them, and `t_vpcmcreate.c` sweeps them.
 *
 * A NEGATIVE SESSION TYPE TAKES THE SAME ARM AS 0: the dispatch's second test
 * is a signed `jle`, so `switch` with 0 in the default arm is the wrong shape
 * and 0 has to share the default's body.
 *
 * +0x2218 IS LEFT AT 0.  `v34handshakinit` clears it and nothing here puts it
 * back; `VPcmV34InitiateRetrain` is what writes 2.  Finding 806 names that as
 * a cost for a downstream handshake fixture, not a defect here.
 */
extern "C" int
VPcmV34Create(void *objp, int side, int ptc, void *runtime, int sessionType)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	unsigned char *cfg = (unsigned char *)runtime;
	unsigned char *sess = (unsigned char *)obj->p3548;
	unsigned char *k56 = (unsigned char *)obj->pac18;
	struct v34_receiver *rx = (struct v34_receiver *)(m + OB_RECEIVER);
	unsigned char *v92;
	/*
	 * All three are read BEFORE the memset, out of `runtime` rather than
	 * out of the object, so the memset cannot reach them -- but the object
	 * still keeps them in stack slots across it, and the two `int`s are
	 * re-read as SHORTS at the one place they are used.
	 */
	int quick = (cfg[CFG_V92LITE] & CFG_QUICKCONNECT) != 0;
	int ansLevel = *(const int *)(cfg + CFG_ANSPCM_LEVEL);
	int uqts = *(const int *)(cfg + CFG_UQTS);

	/*
	 * FIVE SEPARATE GATES AND NOT ONE.  Each re-loads `dsplibs_debug_level`
	 * because the call between them could have changed it, which is what
	 * the object does; a single `if` round all five would be one test.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Create: quick connect indication"
				     " from phase1 = %d\r\n", quick);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Create: Uqts index is %d\r\n",
				     uqts);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Create: ANSpcm level index is"
				     " %d\r\n", ansLevel);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Create, initial Session Type ="
				     " %d\n", sessionType);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("RX at %lX\n", (unsigned long)rx);

	sysdep_memset(obj, 0, VPCM_V34_BYTES);
	sysdep_memset(rx, 0, sizeof(struct v34_receiver));

	obj->pac18 = k56;
	*(short *)(m + OB_FAA80) = 1;
	obj->pac3c = runtime;
	obj->p3548 = sess;

	V34InitializeImplementationSpecific(obj);

	obj->fa23c = 1;
	obj->bulk_tail = 0;
	obj->bulk_head = 0;
	obj->bulk_ring = (short *)(m + OB_BULK_RING);
	obj->bulk_len = 0x2580;

	*(int *)(m + OB_FABD8) = 0;
	*(int *)(m + OB_FABDC) = 0;
	obj->fabe0 = 0;
	obj->fabe2 = 0;
	*(short *)(m + OB_FABE4) = 0;
	*(short *)(m + OB_FABE6) = 0;
	obj->f35a4 = 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("On Create: Setting desired TX MD"
				     " (%d mSec)!\n", (int)obj->f35a4);

	/*
	 * `x * 2.4`, and the object spells it as an unsigned divide of `x * 24`
	 * by ten -- `mul $0xcccccccd` with `shr $3` on the high half, which is
	 * GCC's divide-by-ten and not a fixed-point scale.  Verified
	 * numerically rather than read off.
	 */
	*(short *)(m + OB_FABFC) =
		(short)((*(const unsigned int *)(cfg + CFG_TXMD) * 24u) / 10u);

	/*
	 * The V.92 record is re-loaded between the two byte stores, which is
	 * what the object does and is not an optimisation this file may fold:
	 * nothing says the two loads have to give the same pointer.
	 */
	v92 = *(unsigned char **)(sess + SESS_PCMV92);
	v92[PCMV92_QUICK] = 0;
	v92 = *(unsigned char **)(sess + SESS_PCMV92);
	v92[PCMV92_FLAG11] = 0;
	*(int *)(sess + SESS_PCMTYPE) = 0;

	*(short *)(m + OB_FABC4) = 0;
	obj->local_v92 = 0;
	obj->remote_v92 = 0;
	obj->local_short = 0;
	obj->is_short = 0;
	obj->fabce = 0;
	obj->fabd0 = 0;
	obj->fabd2 = 0;
	*(short *)(m + OB_FABD4) = 0;

	obj->status = 0;
	obj->f0004 = 0;
	obj->k56flex_receiver = 0;

	/*
	 * THE DISPATCH.  Not a `switch` with 0 in `default`: the object tests
	 * `== 2`, then a SIGNED `jle`, then `== 3` and `== 4`, so 0, every
	 * negative value and everything above 4 share one body.
	 */
	if (sessionType == 2) {
		*(int *)(k56 + K56_AORMU) = 0;
		obj->v90_receiver = 1;
		((VPcmFloModem *)sess)->externalReset();

		obj->local_v92 = 1;
		obj->local_short = (short)quick;

		/*
		 * The four bytes `V34GiveINFO1aBits` copied INTO the session
		 * object at +0x14..+0x16, read back out of it -- and a fourth
		 * at +0x17 that no writer reconstructed here accounts for.
		 */
		v92 = *(unsigned char **)(sess + SESS_PCMV92);
		*(short *)(m + OB_FABD4) = v92[PCMV92_COPIED + 3];
		obj->fabd2 = v92[PCMV92_COPIED + 2];
		obj->fabd0 = v92[PCMV92_COPIED + 1];
		obj->fabce = v92[PCMV92_COPIED + 0];
		v92[PCMV92_FLAG11] = 1;

		v92 = *(unsigned char **)(sess + SESS_PCMV92);
		v92[PCMV92_QUICK] = (unsigned char)quick;

		/*
		 * Channel verification only when the far end asked for a quick
		 * connect AND we are the side that was passed 0.  The two
		 * `int`s captured at the top go in as SHORTS here, which is
		 * the only place either is used for anything.
		 */
		if (quick != 0 && side == 0) {
			obj->status = 4;
			(*(V90Demodulator **)(sess + SESS_DEMOD))
				->enterChannelVerification((short)uqts,
							   (short)ansLevel);
			obj->f0004 = 10;
		}

		if (side == 0) {
			side = 1;
		} else {
			*(int *)(sess + SESS_PCMTYPE) = 1;
			side = 0;
		}
	} else if (sessionType == 1) {
		*(int *)(k56 + K56_AORMU) = 0;
		obj->v90_receiver = 1;
		side = (side == 0);
		((VPcmFloModem *)sess)->externalReset();
	} else if (sessionType == 3 || sessionType == 4) {
		*(int *)(k56 + K56_AORMU) = (sessionType == 4);
		obj->v90_receiver = 0;

		/*
		 * THE ONE UNGATED PRINT IN THE FUNCTION.  Through `edprintf`
		 * and behind no level test at all, where the other thirteen
		 * are `dsplibs_debug_printf` behind `DSPLIB_DEBUG_ON()`.  The
		 * same inconsistency v34pcmif.c records for
		 * `VPcmV34SetTxScale`, and the original's.
		 */
		edprintf("VPcmV34Main: it is K56Flex session type,"
			 " (AorMu = %d)\r\n", *(const int *)(k56 + K56_AORMU));

		obj->k56flex_receiver = 1;
		((K56FlexFloModem *)k56)->externalReset();
	} else {
		*(int *)(k56 + K56_AORMU) = 0;
		obj->v90_receiver = 0;
	}

	obj->f359c = (short)(side != 0 ? 0x66 : 0x65);

	obj->ptc = ptc;
	obj->rx_n = 0;
	obj->tx_n = 0;
	obj->tx_rd = 0;
	obj->rate_min = 0;
	obj->rate_max = RATE_INDEX_MAX;
	obj->rate_now = 0;
	obj->rate_want = -1;
	obj->rx_energy_floor = 0;
	*(int *)(m + OB_F0234) = 0;
	*(int *)(m + OB_F0238) = 0;
	*(int *)(m + OB_F0248) = (int)0xfffe8900;
	*(short *)(m + OB_FORCE_LOW_BAUD) = 0;
	rx->f262 = 0x600;
	*(int *)((unsigned char *)rx + RX_F238) = 0;
	*(int *)(m + OB_F000C) = 0;
	obj->nof_tx_bits = 0;

	v34handshakinit(obj, 0);

	/*
	 * +0x35a4 IS RE-READ HERE, AFTER THE HANDSHAKE INITIALISER, so this is
	 * not the zero stored above: the object emits a `movswl` immediately
	 * after the call and carries whatever `v34handshakinit` left.  336 is
	 * `x * 21 * 16`, two `lea`s and a shift, and the source short is
	 * signed, so a negative one gives a result below 10000.
	 */
	*(short *)(m + OB_F0254) =
		(short)(336 * (int)*(const short *)(m + OB_F35A4) + 10000);
	obj->f2aa6 = 0;
	*(short *)(m + OB_F0254 + 2) = 0;
	*(int *)(m + OB_F0254 + 4) = 0;

	/*
	 * The 32 bytes at +0xac1c, byte for byte the same block
	 * `VPcmV34InitiateRetrain` writes; its comment is the derivation and
	 * is not repeated.  +0xac2e is again the one short left alone.
	 */
	{
		unsigned char *st = m + OB_FAC1C;
		short role = obj->f359c;

		*(short *)(st + 0x00) = 0;
		*(short *)(st + 0x02) = 0;
		*(short *)(st + 0x04) = 0;
		*(short *)(st + 0x06) = 0;
		*(int *)(st + 0x08) = 0;
		*(int *)(st + 0x14) = 0;
		*(int *)(st + 0x18) = 0;
		*(int *)(st + 0x1c) = 0;

		if (role == 0x65) {
			*(short *)(st + 0x0c) = 0;
			*(short *)(st + 0x0e) = 0;
			*(short *)(st + 0x10) = (short)0x39c3;
		} else if (role == 0x66) {
			*(short *)(st + 0x0c) = (short)0x5a82;
			*(short *)(st + 0x0e) = (short)0x55fc;
			*(short *)(st + 0x10) = (short)0x39c3;
		}
	}

	obj->rates_latched = 0;
	obj->fac0c = 0;
	m[OB_FAC17] = 0;
	*(short *)(m + OB_F0262) = 1;
	obj->tx_bps = 0;
	obj->rx_bps = 0;
	obj->rrn_local = 0;
	obj->rrn_remote = 0;
	*(short *)(m + OB_FAC12) = 0;
	*(short *)(m + OB_FAC14) = 0;
	obj->f3554 = 0x7d0;
	*(short *)(m + OB_FA248) = 0;
	obj->f3558 = 0x7fdf;
	obj->f355c = 2;

	/*
	 * THE RATE BLOCK, and it is `VPcmV34InitiateRetrain`'s to the
	 * instruction -- same three arms in the same order, same unsigned
	 * divide by 2400, same clamp ladder with `rate_max == 1` falling out
	 * of the `else` rather than being tested separately.  Two functions,
	 * one block; see that one for why each step is the shape it is.
	 */
	if (obj->v90_receiver != 0 && *(const int *)(sess + SESS_GATE) != 0) {
		V90ConstellationDesigner *cd;

		cfg = (unsigned char *)obj->pac3c;
		cd = *(V90ConstellationDesigner **)
		     (*(unsigned char **)(sess + SESS_DEMOD) + DEMOD_DESIGNER);
		cd->setMinMaxRates(*(const unsigned int *)(cfg + CFG_MIN_RATE),
				   *(const unsigned int *)(cfg + CFG_MAX_RATE));
	} else if (obj->k56flex_receiver != 0 && k56[K56_ENABLED] != 0) {
		cfg = (unsigned char *)obj->pac3c;
		((K56FlexFloModem *)k56)->setMinMaxRates(
			*(const int *)(cfg + CFG_MIN_RATE),
			*(const int *)(cfg + CFG_MAX_RATE));
	} else {
		cfg = (unsigned char *)obj->pac3c;

		obj->rate_min = (int)(*(const unsigned int *)
				      (cfg + CFG_MIN_RATE) / RATE_STEP);
		obj->rate_max = (int)(*(const unsigned int *)
				      (cfg + CFG_MAX_RATE) / RATE_STEP);

		if (obj->rate_min > RATE_INDEX_MAX)
			obj->rate_min = RATE_INDEX_MAX;
		if (obj->rate_max < obj->rate_min)
			obj->rate_max = obj->rate_min;
		if (obj->rate_max > RATE_INDEX_MAX)
			obj->rate_max = RATE_INDEX_MAX;
		else if (obj->rate_max == 1)
			*(short *)(m + OB_FORCE_LOW_BAUD) = 1;
	}

	cfg = (unsigned char *)obj->pac3c;

	/* The disconnect threshold, indexed as `VPcmV34InitiateRetrain`. */
	{
		int level = *(const int *)(cfg + CFG_MIN_LEVEL);
		unsigned idx = (unsigned)level + 0x30u;
		int thresh;

		if (idx > 7u)
			idx = 3u;
		thresh = V34DisconnectThreshTable[idx];
		obj->rx_energy_floor = thresh;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmV34Main: minLevel given is "
					     "%d , minSigLevel set to %d\n",
					     level, thresh);
	}

	*(int *)(m + OB_F0234) = 0;

	{
		int delay = *(const int *)(cfg + CFG_FILT_DELAY);
		int biased = (int)((unsigned)delay + 2u);

		*(short *)(m + OB_FILT_DELAY) = (short)((biased >> 2) + 0x22);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34 filtdelay set to %d "
					     "(params initial delay = %d)\n",
					     (int)*(short *)(m + OB_FILT_DELAY),
					     delay);
	}

	{
		int ext = *(const int *)(cfg + CFG_EXT_DELAY);

		obj->f25c = (short)(0x610u - (unsigned)ext);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34FEC, V34dmadelay set to %d, "
					     "(ext delay=%d)\n",
					     (int)obj->f25c, ext);
	}

	((V92EchoCanceller *)(sess + SESS_ECHO))->setEchoDelay(
		(unsigned)*(const int *)(cfg + CFG_EXT_DELAY) + 0x68u);

	/*
	 * THE ENTRANCE FILTER, three ways, and only the first two are named by
	 * their own diagnostics.  -1 means "ask the hardware", which here is
	 * one `int` of the configuration compared against 14; 1 forces it on;
	 * ANYTHING ELSE forces it off, which is why this is not a `switch` --
	 * the object decrements and tests, so 0 and 7 take the same arm.
	 */
	cfg = (unsigned char *)obj->pac3c;
	{
		int stream = *(const int *)(cfg + CFG_ENTRANCE);

		if (stream == -1) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("VPcmFlo: From Stream - "
						     "Entrance Filter according"
						     " to HW...\r\n");
			cfg = (unsigned char *)obj->pac3c;
			sess[SESS_ENTRANCE] = (unsigned char)
				(*(const int *)(cfg + CFG_F54) == 14);
		} else if (stream == 1) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("VPcmFlo: From Stream - "
						     "Entrance Filter forced "
						     "enabled...\r\n");
			sess[SESS_ENTRANCE] = 1;
		} else {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("VPcmFlo: From Stream - "
						     "Entrance Filter forced "
						     "disabled...\r\n");
			sess[SESS_ENTRANCE] = 0;
		}
	}

	((GenericIIR<float, double> *)(sess + SESS_IIR))->reset();

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmFlo: YES! entrance filter applied ="
				     " %d\r\n", (int)sess[SESS_ENTRANCE]);

	return 0;
}


/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  Same argument as v34pcmmain.cpp's block: the fields this
 * file reaches by offset sit in regions that are otherwise padding, so a field
 * that drifted would compile silently.  This function writes forty-odd NAMED
 * fields as well, and every one of them is asserted here -- a rename that
 * moved one would still compile and would be caught only by the differential
 * sweep, which is the wrong place to find out.  The offsets are the
 * disassembly's, not v34fsk.h's, so this is a check and not a restatement.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34PCMCREATE_ASSERT(name, field, off) \
	typedef char v34pcmcreate_off_##name[ \
		(__builtin_offsetof(struct v34_object, field) == (off)) \
		? 1 : -1]

V34PCMCREATE_ASSERT(status,   status,           0x0000);
V34PCMCREATE_ASSERT(f0004,    f0004,            0x0004);
V34PCMCREATE_ASSERT(ptc,      ptc,              0x0008);
V34PCMCREATE_ASSERT(nofbits,  nof_tx_bits,      0x0010);
V34PCMCREATE_ASSERT(rxn,      rx_n,             0x0114);
V34PCMCREATE_ASSERT(txn,      tx_n,             0x0218);
V34PCMCREATE_ASSERT(txrd,     tx_rd,            0x021c);
V34PCMCREATE_ASSERT(rmin,     rate_min,         0x0220);
V34PCMCREATE_ASSERT(rmax,     rate_max,         0x0224);
V34PCMCREATE_ASSERT(rnow,     rate_now,         0x0228);
V34PCMCREATE_ASSERT(rwant,    rate_want,        0x022c);
V34PCMCREATE_ASSERT(floor,    rx_energy_floor,  0x0230);
V34PCMCREATE_ASSERT(v90rx,    v90_receiver,     0x024c);
V34PCMCREATE_ASSERT(k56rx,    k56flex_receiver, 0x0250);
V34PCMCREATE_ASSERT(f25c,     f25c,             0x025c);
V34PCMCREATE_ASSERT(f2aa6,    f2aa6,            0x2aa6);
V34PCMCREATE_ASSERT(p3548,    p3548,            0x3548);
V34PCMCREATE_ASSERT(f3554,    f3554,            0x3554);
V34PCMCREATE_ASSERT(f3558,    f3558,            0x3558);
V34PCMCREATE_ASSERT(f355c,    f355c,            0x355c);
V34PCMCREATE_ASSERT(f359c,    f359c,            0x359c);
V34PCMCREATE_ASSERT(f35a4,    f35a4,            0x35a4);
V34PCMCREATE_ASSERT(bhead,    bulk_head,        0x35a8);
V34PCMCREATE_ASSERT(btail,    bulk_tail,        0x35ac);
V34PCMCREATE_ASSERT(bring,    bulk_ring,        0x35b0);
V34PCMCREATE_ASSERT(blen,     bulk_len,         0x35b4);
V34PCMCREATE_ASSERT(fa23c,    fa23c,            0xa23c);
V34PCMCREATE_ASSERT(filtdly,  filtdelay,        0xaa7c);
V34PCMCREATE_ASSERT(lv92,     local_v92,        0xabc6);
V34PCMCREATE_ASSERT(rv92,     remote_v92,       0xabc8);
V34PCMCREATE_ASSERT(lshort,   local_short,      0xabca);
V34PCMCREATE_ASSERT(isshort,  is_short,         0xabcc);
V34PCMCREATE_ASSERT(fabce,    fabce,            0xabce);
V34PCMCREATE_ASSERT(fabd0,    fabd0,            0xabd0);
V34PCMCREATE_ASSERT(fabd2,    fabd2,            0xabd2);
V34PCMCREATE_ASSERT(fabe0,    fabe0,            0xabe0);
V34PCMCREATE_ASSERT(fabe2,    fabe2,            0xabe2);
V34PCMCREATE_ASSERT(txbps,    tx_bps,           0xac04);
V34PCMCREATE_ASSERT(rxbps,    rx_bps,           0xac08);
V34PCMCREATE_ASSERT(fac0c,    fac0c,            0xac0c);
V34PCMCREATE_ASSERT(rrnl,     rrn_local,        0xac0e);
V34PCMCREATE_ASSERT(rrnr,     rrn_remote,       0xac10);
V34PCMCREATE_ASSERT(latched,  rates_latched,    0xac16);
V34PCMCREATE_ASSERT(pac18,    pac18,            0xac18);
V34PCMCREATE_ASSERT(pac3c,    pac3c,            0xac3c);

/*
 * The receiver, reached as `obj + OB_RECEIVER`, and the one field of it this
 * function names.  `sizeof` is asserted too: it IS the second memset's length,
 * and if it stopped being 0x79c the memset would silently clear a different
 * span (D195).
 */
typedef char v34pcmcreate_rxsize[
	((int)sizeof(struct v34_receiver) == 0x79c) ? 1 : -1];
typedef char v34pcmcreate_rxf262[
	((int)__builtin_offsetof(struct v34_receiver, f262) == 0x262)
	? 1 : -1];

/* And the V.34 object's own extent, which is the first memset's length. */
typedef char v34pcmcreate_objlen[
	((int)VPCM_V34_BYTES == 0xac4c) ? 1 : -1];

#endif /* 32-bit */
