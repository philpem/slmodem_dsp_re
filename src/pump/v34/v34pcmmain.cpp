/*
 * v34pcmmain.cpp -- the C++ half of VPcmV34Main.cpp.
 *
 * `src/pump/v34/v34pcmif.c` holds the `extern "C"` exports of the same
 * translation unit and explains why they are a `.c`: nothing about them is
 * C++, and splitting one TU by language would be a worse map than splitting
 * it by role.  THIS file is the exception that forces itself.  VPcmV34Main.cpp
 * starts at .text+0x9250 with
 *
 *     _Z14getMPrecvdBitsP12tagV34Object
 *
 * and a C translation unit cannot emit that name.  So the C++ half gets a
 * file, and it gets a DIFFERENT STEM from v34pcmif -- the Makefile turns both
 * `%.c` and `%.cpp` into `$(BUILD)/%.o`, so `v34pcmif.cpp` beside
 * `v34pcmif.c` would be two sources racing for one object file.
 *
 * `tagV34Object` is the object's own name for `struct v34_object`, and this
 * symbol is the only place it survives.  It is a distinct, incomplete type
 * here rather than a typedef, because a typedef would mangle as
 * `P11v34_object` and the point of the exercise is the exact string.
 *
 * The file is built with the same -fno-exceptions -fno-rtti -nostdinc++ as
 * FloatIIR.cpp; see the Makefile.  Nothing here is a class, has a virtual, or
 * allocates, which is what lets the test binaries link with $(CC).
 */

/*
 * THE UNWRITTEN-CALLEE MACROS, AND THEY MUST COME BEFORE THE INCLUDES.
 *
 * `VPcmV34Progress` at the bottom of this file calls five symbols nobody has
 * reconstructed -- four `VPcmFloModem` members and
 * `GenericToneDetector::process`.  Their declarations wear these macros, so
 * THIS translation unit makes a weak undefined reference to each: it resolves
 * to zero instead of leaving every test binary with an undefined symbol, and
 * each call site tests the pointer before using it.
 * `include/dsplib/vpcm.h` sets the arrangement out at length for the five
 * `VPcmV34*` entry points; this is the same one, one level further down.
 *
 * A translation unit that DEFINES any of them must not define these --
 * a definition compiled under the macro would itself be weak.
 *
 * `DSPLIB_V34HSHAK_UNWRITTEN` USED TO BE HERE AND IS DELIBERATELY GONE.  It
 * covered `v90RateReneg` and `v90RateRenegSilence`, which this file now
 * DEFINES; defining the macro would make both definitions weak.  The two call
 * sites in `VPcmV34Progress` lost their null tests with it, which is what the
 * object does -- it calls both unconditionally.
 */
#define DSPLIB_VPCMFLO_UNWRITTEN	__attribute__((weak))
#define DSPLIB_GTD_UNWRITTEN		__attribute__((weak))

#include <stdlib.h>

#include "dsplib/K56FlexFloModem.h"
#include "dsplib/V90ConstellationDesigner.h"
#include "dsplib/V92EchoCanceller.h"
#include "dsplib/VPcmFloModem.h"
#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34info.h"
#include "dsplib/v34pcm_tables.h"
#include "dsplib/v34pcmif.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"
#include "dsplib/v34shell.h"
#include "dsplib/VPcmFloModem.h"
/*
 * For `VPcmV34GetCurrentRxBitRate` and `VPcmV34GetCurrentTxBitRate` at the
 * bottom.  `DSPLIB_VPCM_UNWRITTEN` is deliberately NOT defined here, for the
 * reason v34pcmif.c's copy of this include gives: this file DEFINES two of
 * the five, and a definition compiled under the weak macro would stop being
 * one as soon as anything else defined the name.
 */
#include "dsplib/vpcm.h"

/*
 * The session object's MP block.  Six flag bytes and seven shorts, read as a
 * group and never written; the field names come from the diagnostics the
 * function prints out of them -- "V90mp - drn", "type", "trellisState",
 * "nonlinearEncoder", "constellationShaping" and "rateMask" -- so five of the
 * six bytes are named by the object and the sixth, at +5, is not.
 */
/*
 * The two receiver counters as `indicateJaTransmission` reaches them: the
 * object anchors a pointer at `obj + 4` and reads them at +0x248 and +0x24c
 * through it, so
 * these are `v34fsk.h`'s `v90_receiver` (+0x24c) and `k56flex_receiver`
 * (+0x250) less the anchor.  The `OB4_` prefix is what says the base is the
 * anchor and not the object; nothing else in the tree may use these.
 *
 * THEY LIVE HERE AND NOT BESIDE THEIR USER, which is 7799's rule and not a
 * preference: a macro placed next to its first user ends up BELOW it after
 * any lever-3 permutation, the identifier then survives unexpanded with
 * different text, and that is not always a compile error.  This file is a
 * live lever-3 candidate -- 3 of 16 in the blob's `nm -n` order -- so
 * somebody will permute it.
 */
#define OB4_ANCHOR		4
#define OB4_V90_RECEIVER	0x248		/* v34fsk.h's v90_receiver     */
#define OB4_K56_RECEIVER	0x24c		/* v34fsk.h's k56flex_receiver */

/*
 * And tie them to the header, because nothing else does: `make phase`'s
 * offsets tier reads `__builtin_offsetof` annotations and these are raw
 * numbers.  Without this pair, a future edit to `struct v34_object` moves the
 * fields and leaves these two reading whatever is now there -- which is the
 * failure mode CLAUDE.md's naming rules exist to prevent, and it would pass
 * every test that does not happen to drive both arms.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
typedef char ob4_v90_check[(OB4_ANCHOR + OB4_V90_RECEIVER ==
    (int)__builtin_offsetof(struct v34_object, v90_receiver)) ? 1 : -1];
typedef char ob4_k56_check[(OB4_ANCHOR + OB4_K56_RECEIVER ==
    (int)__builtin_offsetof(struct v34_object, k56flex_receiver)) ? 1 : -1];
#endif

#define SESS_MP			0x1744
#define SESS_PCM		0x610c
#define SESS_GATE		0x6120

/* Inside the block the session hands over. */
#define MP_TYPE			0x00
#define MP_DRN			0x01
#define MP_TRELLIS		0x02
#define MP_NONLINEAR		0x03
#define MP_SHAPING		0x04
#define MP_FLAG5		0x05
#define MP_RATEMASK		0x06

/* The PCM receiver's two words, and the configuration's one. */
#define PCM_SENS		0x04f8
#define PCM_RATE		0x04fc
#define CFG_RATE		0x003c

/* Where the answer goes. */
#define OB_INFO			0xaa0c		/* seven shorts follow  */
#define OB_CAPS			0xaa3c
#define OB_CAPS_PTR		0xaa6c
#define OB_DMA			0x2a68

/*
 * 33599, which is 14 rate steps of 2400 less one, and the largest rate the
 * four-bit field below can carry.  Above it the field is left alone.
 */
#define RATE_MAX		0x833f

/*
 * ---------------------------------------------------------------------------
 * And the rest of the session object, for `VPcmV34InitiateRetrain` below.
 *
 * `p3548` IS A `VPcmFloModem`, and this function is what settles it: it hands
 * that pointer straight to `VPcmFloModem::setPcmSessionType` as `this`.  Every
 * offset below then agrees with include/dsplib/VPcmFloModem.h --
 * `SESS_GATE` is its `info0Layout`, `SESS_PCM` its `modem.ptr_49b4` and
 * `SESS_DEMOD` its `modem.demodulator` -- which is three independent
 * confirmations of a map that was built without this function.
 *
 * Spelled as offsets all the same, the way v34pcmif.c spells the same chain:
 * `SESS_ECHO` and `SESS_RETRAIN_FLAG` fall inside that header's `pad_6130`
 * and naming them there would be a claim its owner has not checked.
 */
#define SESS_DEMOD		0x175c		/* V90Modem::demodulator     */
#define SESS_ECHO		0x6bd0		/* a V92EchoCanceller, `lea` */
#define SESS_TONE		0x6f5c		/* a GenericToneDetector     */
#define SESS_RETRAIN_FLAG	0x6fb4		/* an int, cleared           */

/*
 * +0x208 of the demodulator is a `V90ConstellationDesigner *`, and the
 * mangling of the call target is what says so.  Neither the pointer nor the
 * demodulator is modelled as a struct here, for the reason v34pcmif.c gives
 * for the neighbouring `+0x20c -> +0x8c` chain.
 */
#define DEMOD_DESIGNER		0x0208

/*
 * The K56flex object at `pac18`.  A byte at +8 gates the whole K56flex arm,
 * and it is READ THROUGH A CAST rather than declared in
 * include/dsplib/K56FlexFloModem.h: that header measures the class as having
 * no members it can bound, and adding one would change a `sizeof` its own
 * fixture relies on.  v34info.c reads `pac18 + 0xc` the same way.
 */
#define K56_ENABLED		0x08

/*
 * The configuration object at `pac3c`.  +0x3c, +0x44, +0x50 and +0x54 are
 * already described in v34fsk.h; these are the six this function adds.
 *
 * +0x50 is the same byte `chkForceBaudRate` reads bits 5..7 of as the maximum
 * V.34 baud rate index.  Bit 3 of it is set here and by nothing else read so
 * far, so the byte is a bag of unrelated fields rather than one number.
 */
#define CFG_FLAGS		0x00		/* bit 3 v90, bit 4 flex     */
#define CFG_MIN_RATE		0x30		/* bits per second, unsigned */
#define CFG_MAX_RATE		0x34
#define CFG_ISP			0x50		/* bit 3: a sensitive ISP    */
#define CFG_MIN_LEVEL		0x60		/* signed, biased by 0x30    */
#define CFG_FILT_DELAY		0x64		/* "params initial delay"    */
#define CFG_EXT_DELAY		0x68		/* "ext delay"               */

#define CFG_FLAG_V90		0x08
#define CFG_FLAG_FLEX		0x10
#define CFG_ISP_SENSITIVE	0x08

/*
 * +0x02, and the same shape as +0x50 above: a bag of bits and not a number.
 * `V34GiveINFO1dBits` tests it by its SIGN -- `cmpb $0x0; js` -- so bit 7 set
 * bars the V.92Lite retrain and everything else permits it, while
 * `VPcmV34Progress` tests bit 5 of the very same byte for something else.
 * Offset-named for that reason; neither reader names the byte as a whole.
 */
#define CFG_V92LITE		0x02

/*
 * The V.34 object's own fields, for the regions v34fsk.h leaves unmapped.
 * OFFSET-NAMED except for the two the object itself names:
 *
 *   OB_FILT_DELAY     "V34 filtdelay set to %d", printed by this function
 *                     out of the field it has just stored.
 *   OB_FORCE_LOW_BAUD `probeselect` opens with `if (*(short *)(m + 0x359a))
 *                     goto rate_2400`, jumping over the whole symbol-rate
 *                     ladder (src/pump/v34/v34hshak.c).  This function is the
 *                     only writer read so far and it sets it exactly when the
 *                     maximum bit-rate index came out as 1 -- 2400 bit/s,
 *                     which the lowest symbol rate is the only way to carry.
 *                     Two sites, one meaning; the name is descriptive and the
 *                     derivation is the pair.
 */
#define OB_RECEIVER		0x0264
#define OB_F0234		0x0234		/* in v34fsk.h's clock group */
#define OB_F0254		0x0254		/* short, short, then an int */
#define OB_F2218		0x2218		/* v34handshakinit clears it */
#define OB_FORCE_LOW_BAUD	0x359a		/* short; see above          */
#define OB_F35A4		0x35a4		/* signed short, scales F0254 */
#define OB_FILT_DELAY		0xaa7c		/* short; the object's name  */
#define OB_FAC00		0xac00		/* byte                      */
#define OB_FAC1C		0xac1c		/* 32 bytes, cleared below   */

/* The datapump codes, which are the modulation numbers themselves. */
#define DP_KEEP			0
#define DP_V34			34
#define DP_K56FLEX		56
#define DP_V90			90
#define DP_V92			92

/* Rates arrive in bits per second and every field here is an index. */
#define RATE_STEP		2400u
#define RATE_INDEX_MAX		14

void
getMPrecvdBits(struct tagV34Object *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	short *mp = (short *)(m + OB_INFO);
	const unsigned char *sess = (const unsigned char *)obj->p3548;
	int rate;

	if (obj->v90_receiver > 0) {
		const unsigned char *d = sess + SESS_MP;
		int v;

		/*
		 * The first short of the INFO record, assembled bit by bit:
		 * one flag in bit 0, a four-bit field at 6, a two-bit one at
		 * 11, and three more flags at 13, 14 and 15.  The masks are
		 * the object's -- `and $0xf` and `and $0x3` -- so a byte
		 * larger than its field is truncated rather than rejected.
		 */
		v = (d[MP_TYPE] != 0) ? 1 : 0;
		v |= (d[MP_DRN] & 0xf) << 6;
		v |= (d[MP_TRELLIS] & 3) << 11;
		if (d[MP_NONLINEAR] != 0)
			v |= 0x2000;
		if (d[MP_SHAPING] != 0)
			v |= 0x4000;
		if (d[MP_FLAG5] != 0)
			v |= 0x8000;

		/*
		 * Seven shorts straight across, except the first, which comes
		 * back with its top bit forced on.  `rate_mask`'s sign bit is
		 * "asymmetric rates are on the table" (v34fsk.h), so this says
		 * a V.90 MP always permits them.
		 */
		mp[1] = (short)(*(const unsigned short *)(d + MP_RATEMASK)
				| 0x8000);
		mp[2] = *(const short *)(d + 0x08);
		mp[3] = *(const short *)(d + 0x0a);
		mp[4] = *(const short *)(d + 0x0c);
		mp[5] = *(const short *)(d + 0x0e);
		mp[6] = *(const short *)(d + 0x10);
		mp[7] = *(const short *)(d + 0x12);

		/*
		 * And the four-bit field at 6 is COPIED DOWN over bits 2..5,
		 * which is where v34fsk.h says the two per-direction rate
		 * nibbles live: the V.90 upstream rate becomes the V.34
		 * one as well.  ORed, not assigned, so a bit already set
		 * there stays set.
		 */
		v |= (v >> 4) & 0x3c;
		obj->info_rates = (short)v;

		if (v & 1)
			txrxdmainit((short *)(m + OB_DMA), mp);

		/*
		 * Seven diagnostics, each with its OWN level test, because
		 * the call between them can change the level.  The names in
		 * them are the whole reason this function's fields have any.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90mp - drn = %d\r\n",
					     (signed char)d[MP_DRN]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90mp - type = %d\r\n",
					     (signed char)d[MP_TYPE]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90mp - trellisState = %d\r\n",
					     (signed char)d[MP_TRELLIS]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90mp - nonlinearEncoder = %d\r\n",
			    (signed char)d[MP_NONLINEAR]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90mp - constellationShaping = %d\r\n",
			    (signed char)d[MP_SHAPING]);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90mp - rateMask = 0x%X\r\n",
					     *(const short *)(d + MP_RATEMASK));
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "mp->data[0] = 0x%X , mp->data[1] = 0x%X\r\n",
			    (unsigned short)obj->info_rates,
			    (unsigned short)mp[1]);
	}

	/*
	 * AND THE SAME CALL AGAIN, on the same bit, unconditionally on the
	 * branch above.  With a V.90 receiver running and bit 0 set,
	 * `txrxdmainit` runs TWICE over the same six shorts; it is
	 * idempotent, so the repeat is wasted work rather than a defect.  The
	 * test at 0x9333 is a `testb` on the low byte, which is bit 0 of the
	 * short on this little-endian target.
	 */
	if (*(const unsigned char *)mp & 1)
		txrxdmainit((short *)(m + OB_DMA), mp);

	/*
	 * The capability word, built from scratch: 0x9dc3 first, then four
	 * bits of it replaced below.  The pointer at +0xaa6c is aimed at it,
	 * which is what says this word is a message the handshake will clock
	 * out through `getbit` -- that is the only reader of +0xaa6c.
	 */
	*(short *)(m + OB_CAPS) = (short)0x9dc3;
	/* Spelled as `v34handshakinit` spells the same two fields. */
	*(short **)(m + OB_CAPS_PTR) = (short *)(m + OB_CAPS);

	rate = *(const int *)((const unsigned char *)obj->pac3c + CFG_RATE);

	if (*(const int *)(sess + SESS_GATE) != 0 && obj->v90_receiver > 1
	    && *(const int *)(*(const unsigned char *const *)(sess + SESS_PCM)
			      + PCM_SENS) != 0) {
		int cap = *(const int *)(
		    *(const unsigned char *const *)(sess + SESS_PCM)
		    + PCM_RATE) * 0x960;

		if (rate > cap)
			rate = cap;
		edprintf("on get max upstream rate, on sensitive ISP, "
			 "returning %d\r\n", rate);
	} else {
		edprintf("on get max upstream rate, on regular ISP, "
			 "returning %d\r\n", rate);
	}

	if (rate <= RATE_MAX) {
		/*
		 * bits per second -> rate index, as `rate * 7 >> 14`: 2400 Hz
		 * of rate is 1.0254 of a step, so this is a divide by 2340
		 * rather than by 2400 and 33600 still lands on 14.  The four
		 * bits go in REVERSED, at positions 6..9, which is the same
		 * treatment v34fsk.h records for `info_caps`'s two nibbles.
		 */
		int rev = (short)bitreverse((unsigned short)((rate * 7) >> 14),
					    4);
		int mask = -0x41;			/* ~0x40 */
		int k;

		for (k = 0; k <= 3; k++) {
			unsigned short w = *(const unsigned short *)
			    (m + OB_CAPS);

			if ((rev >> k) & 1)
				w = (unsigned short)(w | (unsigned)~mask);
			else
				w = (unsigned short)(w & (unsigned)mask);
			*(short *)(m + OB_CAPS) = (short)w;
			mask = (short)(mask * 2 + 1);
		}
	}

	/*
	 * The rest of the record, cleared.  Nine shorts from +0xaa3e, and the
	 * one non-zero constant among them is +0xaa3e itself.  The object
	 * writes them in an order the compiler chose; nothing reads any of
	 * them in between, so this is the same state.
	 */
	*(short *)(m + 0xaa3e) = 0x7ffd;
	*(short *)(m + 0xaa40) = 0;
	*(short *)(m + 0xaa42) = 0;
	*(short *)(m + 0xaa44) = 0;
	*(short *)(m + 0xaa46) = 0;
	*(short *)(m + 0xaa48) = 0;
	*(short *)(m + 0xaa4a) = 0;
	*(short *)(m + 0xaa4c) = 0;
}

/*
 * ===========================================================================
 * v90Phase34 -- one symbol of the V.90 phase 3/4 transmit sequence
 * ===========================================================================
 *
 * WHY IT IS IN THIS FILE.  `v90Phase34` is unmangled, so it was declared
 * `extern "C"`; but two of its calls are relocations against
 *
 *     _ZN12VPcmFloModem12getV90CpBitsEPs
 *     _ZN12VPcmFloModem12getV90JaBitsEPs
 *
 * which a C translation unit cannot name.  So the translation unit is C++,
 * and WHICH C++ translation unit is settled by the call at .text+0x9fea:
 *
 *     e8 61 f2 ff ff        call 9250 <_Z14getMPrecvdBitsP12tagV34Object>
 *
 * -- a resolved PC-relative displacement with NO relocation beside it, which
 * in a non-PIC object happens only when the target is defined in the same
 * section of the same translation unit.  That is `getMPrecvdBits` above, and
 * the head of this file says why the file exists.  The debug string's
 * "VPcmV34Main:" prefix agrees, but the missing relocation is the argument.
 *
 * ONE ARGUMENT, and it is the V.34 object: `sub $0x2c,%esp` then
 * `mov 0x30(%esp),%ebp`, and nothing else is read from the incoming frame.
 * IT RETURNS 0 at both `ret`s -- `xor %eax,%eax` reaches each of them, and
 * its one caller discards the value, so "returns 0" is the whole of the
 * evidence for the return type, exactly as for `k56FlexPhase34`.
 *
 * ---------------------------------------------------------------------------
 * WHAT IT DOES.  One call emits one or two handshake symbols, and which
 * depends on where the V.90 phase 3/4 sequence has got to.  THREE things
 * select the arm, where `k56FlexPhase34` has two: bit 10 of the receiver's
 * flags word (`V34_RX_FLAG_DATA`), bit 4 of the same word, and the int at
 * +0x24c that `v34fsk.h` calls `v90_receiver` -- NOT `k56flex_receiver` at
 * +0x250, which is what the K56flex twin reads through the identical
 * `obj + 4` base.  One int apart, and it is the whole difference between the
 * two functions' state machines.
 *
 *     0x400 clear             transmit the next Ja dibit
 *     0x400 set, 0x10 clear   transmit the zero point, then arm the machine
 *     both set, +0x24c = 3    two symbols; count to 0x7f, then state 4
 *                  = 4        two symbols; count to 0x10, then state 5
 *                  = 5        one scrambled idle symbol
 *                  = 6        two symbols; count to 0x7f, then state 7
 *                  = 7        two symbols; count to 0x10, then state 8
 *                  = 8        the next CP symbol; on the last, state 9
 *                  = 9        a constant symbol
 *                  = 10       the next CP symbol; on the last, hand over
 *     both set, anything else do nothing
 *
 * BIT 4 OF THE FLAGS WORD IS WRITTEN HERE.  `v34recv.h` calls it
 * `V34_RX_FLAG_TRN_WATCH` after its READER in `v34rx.c`; the macro is used
 * below because it is the same bit of the same word, and for no stronger
 * reason.  Nothing here is a claim about TRN2.
 *
 * ---------------------------------------------------------------------------
 * THE IDLE SYMBOL IS NOT `txmitdibit` OR `txmitquadbit`, the same three ways
 * `src/pump/v34/v34k56.cpp` sets out for the K56flex twin -- with one
 * difference that matters, and it is in the FIRST of the three:
 *
 *   - `V34scrambler`'s mode argument is the LITERAL 0 here, where the
 *     K56flex twin passes the literal 1.  Both emitters pass
 *     `tx_scrambler_mode(o)`, which is bit 0 of `f25c2`; nothing in these
 *     1,358 bytes loads +0x25c2 at all.  Mode 0 is the CALLING station's
 *     polynomial (v34hshak.c), so a reconstruction that called an emitter
 *     here agrees with the blob for every object whose `f25c2` bit 0 is
 *     clear and disagrees for every one where it is set.
 *   - there is NO differential encoding: the scrambler's two bits go
 *     straight into `f25c8` and index the table.
 *   - `f25c6` is not written, so the quadrant the handshake carries does not
 *     advance across an idle symbol.
 *
 * AND THE CONSTELLATION DISCRIMINATOR IS READ THREE WAYS, NOT TWO.  Case 5
 * tests `f382` against 0x89b0 AND against 0x8990 and has a third arm for
 * everything else, which transmits the ZERO point.  The K56flex twin has two
 * arms and privileges neither value; here 0x8990 is privileged, and a value
 * that is neither reaches code no other arm does.  Cases 8, 9 and 10 test
 * for 0x89b0 alone, so for them 0x8990 is not privileged.
 *
 * ---------------------------------------------------------------------------
 * NEITHER `vect_idx` (+0x2aa2) NOR THE SHIFT REGISTER AT +0x25d6 APPEARS IN
 * THIS FUNCTION.  The K56flex twin's case 3 shifts that word out two bits at
 * a time and its Ja completion arm reloads it; the V.90 sequence carries its
 * bits in the `VPcmFloModem` instead and counts symbols in `f25c0`.  So none
 * of finding F282's shift-count masking applies here and no `& 31` is
 * written: there is no variable shift.
 */

/* The handshake's transmit state machine; see v34hshak.c. */
#define OB_TXSTATE		0x3596

/*
 * +0xabfe.  A byte `v34handshakinit` clears and this sets, on the one path
 * whose diagnostic names it: "tx buffer backward clear is enabled".  It is
 * inside `unmapped_abfb` in `struct v34_object`, so it is reached by offset.
 */
#define OB_BACKWARD_CLEAR	0xabfe

/*
 * +0xaa86, printed as `period` beside `tx->symcnt` by case 3's diagnostic.
 * That is `V34_RATECFG + 2`; see `struct v34_ratecfg`.
 */
#define OB_PERIOD		0xaa86

/*
 * The negotiated configuration at `pac3c`; see v34fsk.h.
 *
 * PREFIXED, because `CFG_FLAGS` is already taken in this translation unit and
 * means a DIFFERENT offset: VPcmV34InitiateRetrain's block above uses it for
 * `pac3c + 0x00`, the word whose bits 3 and 4 are the V.90 and K56flex
 * permissions.  This one is `pac3c + 0x50`, which that block calls `CFG_ISP`.
 * Two batches wrote into this file in parallel and picked the same name for
 * two things; finding F325 is what that cost.
 */
#define P34_CFG_FLAGS		0x50
#define P34_CFG_BACKWARD_CLEAR	0x04

/*
 * The constellation-size discriminator at +0x382, which
 * `VPcmV34SetV90RateReneg` sets to 0x89b0 or 0x8990.
 */
#define OB_CONSTEL_16		((short)0x89b0)
#define OB_CONSTEL_4		((short)0x8990)

extern "C" int
v90Phase34(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)objp;
	struct v34_receiver *rx = (struct v34_receiver *)(m + OB_RECEIVER);
	VPcmFloModem *vp = (VPcmFloModem *)o->p3548;
	unsigned short fl = rx->flags;
	short n;

	if (!(fl & V34_RX_FLAG_DATA)) {
		/*
		 * Ja.  The bit source writes the dibit into `f25c8` -- the
		 * same field the emitters use as their quadrant register --
		 * and returns non-zero on the symbol that ends the sequence.
		 * The dibit is transmitted either way, so the last one is
		 * sent and then acted on.
		 *
		 * `VPcmFloModem::getV90JaBits` is a REAL BODY, unlike the
		 * K56flex twin's three-byte stub, so everything below here is
		 * reachable and is tested.
		 */
		int done = (short)vp->getV90JaBits(&o->f25c8);

		txmitdibit(o, o->f25c8);
		if (done == 0)
			return 0;

		rx->flags = (unsigned short)(rx->flags | V34_RX_FLAG_DATA);
		if (!(((const unsigned char *)o->pac3c)[P34_CFG_FLAGS]
		      & P34_CFG_BACKWARD_CLEAR))
			return 0;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmV34Main: tx buffer backward "
					     "clear is enabled...\r\n");
		m[OB_BACKWARD_CLEAR] = 1;
		return 0;
	}

	if (!(fl & V34_RX_FLAG_TRN_WATCH)) {
		/*
		 * The sequence has bits but the machine has not been armed.
		 * Transmit the ZERO point -- two 16-bit stores in the object,
		 * where every other arm stores the whole packed complex at
		 * once -- and arm it only once the state has moved past 2.
		 */
		o->txpoint.c[0] = 0;
		o->txpoint.c[1] = 0;
		txmit(o);
		if (o->v90_receiver <= 2)
			return 0;
		rx->flags = (unsigned short)(rx->flags
					     | V34_RX_FLAG_TRN_WATCH);
		o->f25c0 = 0;
		return 0;
	}

	switch (o->v90_receiver) {
	/*
	 * Cases 3 and 6 are the same two symbols and the same 0x7f count, and
	 * differ only in the state they move to and in case 3's diagnostic.
	 * Cases 4 and 7 are the same pair again with a different point pair
	 * and a 0x10 count.  They are transcribed separately rather than
	 * folded together, because that is how the object lays them out and
	 * because folding would have to invent a conditional debug site.
	 */
	case 3:
		o->txpoint.word = vect4[0];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		o->txpoint.word = vect4[3];
		txmit(o);
		/* Re-read: `txmit` is between the two counts. */
		n = (short)((unsigned short)o->f25c0 + 1);
		if (n <= 0x7f) {
			o->f25c0 = n;
			return 0;
		}
		if (DSPLIB_DEBUG_ON()) {
			/*
			 * The object stores the count before printing it and
			 * zeroes it again below; the store is unobservable
			 * either way, and it is here because it is there.
			 */
			o->f25c0 = n;
			dsplibs_debug_printf("Entered v34m->v90Receiver == "
					     "V90RCV_P3_THIRD_S with "
					     "tx->symcnt = %d period = %d\n",
					     (int)n,
					     (int)*(const short *)
					     (m + OB_PERIOD));
		}
		o->v90_receiver = 4;
		o->f25c0 = 0;
		return 0;

	case 6:
		o->txpoint.word = vect4[0];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		o->txpoint.word = vect4[3];
		txmit(o);
		n = (short)((unsigned short)o->f25c0 + 1);
		if (n <= 0x7f) {
			o->f25c0 = n;
			return 0;
		}
		o->v90_receiver = 7;
		o->f25c0 = 0;
		return 0;

	case 4:
		o->txpoint.word = vect4[2];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		o->txpoint.word = vect4[1];
		txmit(o);
		n = (short)((unsigned short)o->f25c0 + 1);
		if (n != 0x10) {
			o->f25c0 = n;
			return 0;
		}
		o->v90_receiver = 5;
		/* Reset the transmitter for the idle symbols that follow. */
		o->f25c6 = 0;
		o->f25c0 = 0;
		o->f25cc = 0;
		return 0;

	case 7:
		o->txpoint.word = vect4[2];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		o->txpoint.word = vect4[1];
		txmit(o);
		n = (short)((unsigned short)o->f25c0 + 1);
		if (n != 0x10) {
			o->f25c0 = n;
			return 0;
		}
		o->v90_receiver = 8;
		o->f25c6 = 0;
		o->f25c0 = 0;
		o->f25cc = 0;
		return 0;

	case 5: {
		/* The idle symbol.  See the note at the top of this block. */
		short c = o->f382;
		int q;

		if (c == OB_CONSTEL_16) {
			int d;

			q = (short)V34scrambler((unsigned *)&o->f25cc,
						0, 3, 2);
			o->f25c8 = (short)q;
			d = (short)V34scrambler((unsigned *)&o->f25cc,
						0, 3, 2);
			q = o->f25c8;
			o->txpoint.word = vect16[d + q * 4];
		} else if (c == OB_CONSTEL_4) {
			q = (short)V34scrambler((unsigned *)&o->f25cc,
						0, 3, 2);
			o->f25c8 = (short)q;
			o->txpoint.word = vect4[q];
		} else {
			o->txpoint.c[0] = 0;
			o->txpoint.c[1] = 0;
		}

		txmit(o);
		/* Re-read: `txmit` is between the load and the store. */
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		return 0;
	}

	case 9:
		/*
		 * A constant symbol, and the one arm that uses the published
		 * emitters with a literal: all four bits set for the sixteen-
		 * point map, both bits set for the four-point one.  So it IS
		 * scrambled with `f25c2`'s polynomial and IS differentially
		 * encoded, unlike case 5.
		 */
		if (o->f382 == OB_CONSTEL_16)
			txmitquadbit(o, 15);
		else
			txmitdibit(o, 3);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		return 0;

	case 8: {
		/*
		 * CP, and the only arms whose bits are the far end's message
		 * rather than a fixed pattern -- so they go through the
		 * emitters like the rest of the handshake.  The bit source
		 * reports the end of the sequence, and the symbol carrying it
		 * is transmitted before the state moves.
		 */
		int done = (short)vp->getV90CpBits(&o->f25c8);

		if (o->f382 == OB_CONSTEL_16)
			txmitquadbit(o, o->f25c8);
		else
			txmitdibit(o, o->f25c8);
		if (done == 0)
			return 0;
		o->v90_receiver = 9;
		return 0;
	}

	case 10: {
		int done = (short)vp->getV90CpBits(&o->f25c8);

		if (o->f382 == OB_CONSTEL_16)
			txmitquadbit(o, o->f25c8);
		else
			txmitdibit(o, o->f25c8);
		if (done == 0)
			return 0;

		/*
		 * The end of phase 3/4: unpack what the far end sent, set the
		 * data rates up and hand the handshake to its transmit state.
		 *
		 * The state store is a compare-then-store in the object and
		 * is written as one here; it is indistinguishable from a
		 * plain store by any test, since the value written is the
		 * value compared against.
		 */
		getMPrecvdBits((struct tagV34Object *)objp);
		initdigital(o);
		if (*(short *)(m + OB_TXSTATE) != V34HS_EXMIT)
			*(short *)(m + OB_TXSTATE) = V34HS_EXMIT;
		o->v90_receiver = 2;
		return 0;
	}
	}

	return 0;
}

/*
 * ===========================================================================
 * v90RateReneg and v90RateRenegSilence -- the two rate-renegotiation
 * transmit sequences
 * ===========================================================================
 *
 * WHY THEY ARE IN THIS FILE, and it is `v90Phase34`'s argument twice over.
 * Both are unmangled `T`, so both were declared `extern "C"`; both relocate a
 * call against `_ZN12VPcmFloModem12getV90CpBitsEPs`, which a C translation
 * unit cannot name; and both reach `_Z14getMPrecvdBitsP12tagV34Object` at
 * .text+0x9250 through a resolved PC-relative displacement with NO relocation
 * beside it -- .text+0x98cf and .text+0x9aa0 -- which in a non-PIC object
 * happens only when the target is in the same section of the same translation
 * unit.  They are also this file's immediate neighbours in .text: 0x95d0,
 * 0x99b0, then `v90Phase34` at 0x9be0.
 *
 * ONE ARGUMENT each, the V.34 object, and each RETURNS 0 at all three of its
 * `ret`s -- see the header for why the return type is `int`.
 *
 * ---------------------------------------------------------------------------
 * WHAT THEY ARE.  `VPcmV34SetV90RateReneg` assigns `v90_receiver` 11 or 15
 * outright (v34fsk.h, D48), and those are the two entry points to the two
 * ladders below.  `VPcmV34Progress` then picks the transmitter by the same
 * number: above 14 the silence one, above 10 the other.  So the field is one
 * state variable with two disjoint renegotiation sequences on it, and each
 * function is the four or six states of its own:
 *
 *     v90RateReneg          11 -> 12 -> 13 -> 14 -> 2
 *     v90RateRenegSilence   15 -> 16 -> 17 -> 18 -> 19, and 20 -> 2
 *
 * Each ladder is `v90Phase34`'s phase 3/4 sequence with the Ja and CP arms
 * trimmed: the S/S-bar pair counted to 0x7f, then the point pair counted to
 * 0x10, then a constant symbol, then CP.  Both end exactly as `v90Phase34`'s
 * case 10 does -- `getMPrecvdBits`, `initdigital`, the handshake into
 * `V34HS_EXMIT`, and `v90_receiver` back to 2.
 *
 * THE SILENCE ONE HAS TWO STATES THE OTHER HAS NOT, and they are what the
 * name means.  State 18's CP-complete arm drops into state 19 rather than
 * finishing, having cleared the echo-canceller freeze and the SAS detector;
 * state 19 then transmits scrambled idle symbols indefinitely, and state 20
 * -- which nothing here reaches, so a caller sets it -- runs the closing CP
 * with the freeze put back.  So the silence ladder parks in 19 and is
 * restarted from outside, where the plain one runs straight through.
 *
 * ---------------------------------------------------------------------------
 * THE IDLE SYMBOL IN STATE 19 HAS TWO ARMS, NOT `v90Phase34`'s THREE.  Case 5
 * there privileges 0x8990 and has a third arm transmitting the zero point;
 * here `f382 != 0x89b0` IS the four-point arm, whatever it holds.  Everything
 * else about it is the same -- mode 0, no differential encoding, the tables
 * indexed unmasked -- with ONE addition: `f25c6` IS written, from `f25c8`,
 * which `v90Phase34`'s note explicitly records as not happening there.  So
 * the quadrant the handshake carries DOES advance across a silence idle
 * symbol and does not across a phase 3/4 one.
 *
 * ---------------------------------------------------------------------------
 * NEITHER FUNCTION IS A `switch` IN THE OBJECT.  Both dispatch through a
 * compare chain in ascending order -- `cmp $0xb; je; cmp $0xc; je; ...` at
 * .text+0x99e2 and six of them at .text+0x9602 -- where a `switch` over six
 * dense values compiles to a jump table under these flags.  Written as an
 * if-chain for that reason and not for taste; `v90Phase34` above is a
 * `switch` and its jump table is one of the ways it still differs from the
 * object.
 */

/*
 * +0xac3c + 3.  A byte of flags in the configuration object, and BIT 2 IS THE
 * ONLY ONE ANYTHING TOUCHES: `v90RateRenegSilence` clears it one line after
 * printing "disabling SAS detector on silence", and
 * `VPcmFloModem::runPcmModem` sets it again at .text+0xe86f on the way out of
 * the V.92 CPt.
 *
 * THE OFFSET IS OFFSET-NAMED AND THE BIT IS NOT, which is deliberate.  The
 * diagnostic is the author's own word for what the clear does, but the same
 * statement clears TWO bits -- this one and `V34_EC_FROZEN` in `f25c2` -- so
 * "SAS detector" names one of the two and the message does not say which.
 * It is this one by elimination: bit 2 of `f25c2` already has a name from an
 * independent reader (v34rx.h, and `v34FreezeEcho` is its writer), and
 * nothing about an echo canceller is a detector of anything.  That is
 * inference, which is CLAUDE.md's weakest rank, and it is why the byte itself
 * keeps its offset.
 */
#define CFG_FLAGS03		0x03
#define CFG_SAS_DETECT		0x04

extern "C" int
v90RateReneg(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)objp;
	VPcmFloModem *vp = (VPcmFloModem *)o->p3548;
	int r = o->v90_receiver;
	short n;

	if (r == 11) {
		/* `v90Phase34`'s case 6, counted to 0x7f. */
		o->txpoint.word = vect4[0];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		o->txpoint.word = vect4[3];
		txmit(o);
		/* Re-read: `txmit` is between the two counts. */
		n = (short)((unsigned short)o->f25c0 + 1);
		if (n <= 0x7f) {
			o->f25c0 = n;
			return 0;
		}
		o->v90_receiver = 12;
		o->f25c0 = 0;
		return 0;
	}

	if (r == 12) {
		/* `v90Phase34`'s case 7, counted to 0x10. */
		o->txpoint.word = vect4[2];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		o->txpoint.word = vect4[1];
		txmit(o);
		n = (short)((unsigned short)o->f25c0 + 1);
		if (n != 0x10) {
			o->f25c0 = n;
			return 0;
		}
		o->v90_receiver = 13;
		/* Reset the transmitter for the symbols that follow. */
		o->f25c6 = 0;
		o->f25c0 = 0;
		o->f25cc = 0;
		return 0;
	}

	if (r == 13) {
		/* `v90Phase34`'s case 9: the constant symbol, scrambled and
		 * differentially encoded through the published emitters. */
		if (o->f382 == OB_CONSTEL_16)
			txmitquadbit(o, 15);
		else
			txmitdibit(o, 3);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		return 0;
	}

	if (r == 14) {
		/* `v90Phase34`'s case 10, and the end of the ladder. */
		int done = (short)vp->getV90CpBits(&o->f25c8);

		if (o->f382 == OB_CONSTEL_16)
			txmitquadbit(o, o->f25c8);
		else
			txmitdibit(o, o->f25c8);
		if (done == 0)
			return 0;

		getMPrecvdBits((struct tagV34Object *)objp);
		initdigital(o);
		if (*(short *)(m + OB_TXSTATE) != V34HS_EXMIT)
			*(short *)(m + OB_TXSTATE) = V34HS_EXMIT;
		o->v90_receiver = 2;
		return 0;
	}

	return 0;
}

extern "C" int
v90RateRenegSilence(void *objp)
{
	struct v34_object *o = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)objp;
	VPcmFloModem *vp = (VPcmFloModem *)o->p3548;
	int r = o->v90_receiver;
	short n;

	if (r == 15) {
		o->txpoint.word = vect4[0];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		o->txpoint.word = vect4[3];
		txmit(o);
		n = (short)((unsigned short)o->f25c0 + 1);
		if (n <= 0x7f) {
			o->f25c0 = n;
			return 0;
		}
		o->v90_receiver = 16;
		o->f25c0 = 0;
		return 0;
	}

	if (r == 16) {
		o->txpoint.word = vect4[2];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		o->txpoint.word = vect4[1];
		txmit(o);
		n = (short)((unsigned short)o->f25c0 + 1);
		if (n != 0x10) {
			o->f25c0 = n;
			return 0;
		}
		o->v90_receiver = 17;
		o->f25c6 = 0;
		o->f25c0 = 0;
		o->f25cc = 0;
		return 0;
	}

	if (r == 17) {
		if (o->f382 == OB_CONSTEL_16)
			txmitquadbit(o, 15);
		else
			txmitdibit(o, 3);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		return 0;
	}

	if (r == 18) {
		/*
		 * CP, and on the symbol that ends it the machine goes to the
		 * idle state rather than to the data phase: the freeze comes
		 * off both echo cancellers and the SAS detector is disabled,
		 * and 19 then transmits scrambled idle symbols until something
		 * outside moves it on.
		 */
		int done = (short)vp->getV90CpBits(&o->f25c8);

		if (o->f382 == OB_CONSTEL_16)
			txmitquadbit(o, o->f25c8);
		else
			txmitdibit(o, o->f25c8);
		if (done == 0)
			return 0;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("move to SCR on silence rrn "
					     "(disabling SAS detector on "
					     "silence)\r\n");
		o->v90_receiver = 19;
		o->f25c2 = (short)((unsigned short)o->f25c2 & ~V34_EC_FROZEN);
		((unsigned char *)o->pac3c)[CFG_FLAGS03] &= ~CFG_SAS_DETECT;
		return 0;
	}

	if (r == 19) {
		/*
		 * The idle symbol.  Two arms, and see the note at the top for
		 * how it differs from `v90Phase34`'s three-armed case 5.
		 */
		int q;

		if (o->f382 == OB_CONSTEL_16) {
			int d;

			q = (short)V34scrambler((unsigned *)&o->f25cc,
						0, 3, 2);
			o->f25c8 = (short)q;
			d = (short)V34scrambler((unsigned *)&o->f25cc,
						0, 3, 2);
			q = o->f25c8;
			o->txpoint.word = vect16[d + q * 4];
		} else {
			q = (short)V34scrambler((unsigned *)&o->f25cc,
						0, 3, 2);
			o->f25c8 = (short)q;
			o->txpoint.word = vect4[q];
		}

		txmit(o);
		/* Re-read: `txmit` is between the load and the store. */
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		/* And the quadrant DOES advance here.  See the note above. */
		o->f25c6 = o->f25c8;
		return 0;
	}

	if (r == 20) {
		/*
		 * The closing CP.  Nothing in either function sets this state,
		 * so the caller does; the freeze goes back on before every
		 * symbol, not once at entry, because the state is re-entered
		 * per symbol.
		 */
		int done;

		o->f25c2 = (short)((unsigned short)o->f25c2 | V34_EC_FROZEN);
		done = (short)vp->getV90CpBits(&o->f25c8);

		if (o->f382 == OB_CONSTEL_16)
			txmitquadbit(o, o->f25c8);
		else
			txmitdibit(o, o->f25c8);
		if (done == 0)
			return 0;

		getMPrecvdBits((struct tagV34Object *)objp);
		initdigital(o);
		if (*(short *)(m + OB_TXSTATE) != V34HS_EXMIT)
			*(short *)(m + OB_TXSTATE) = V34HS_EXMIT;
		o->v90_receiver = 2;
		return 0;
	}

	return 0;
}

/*
 * ===========================================================================
 * VPcmV34InitiateRetrain -- start the V.34 handshake again, and choose what
 * comes back up.
 * ===========================================================================
 *
 * `extern "C"`: the object exports the name unmangled, so it was declared that
 * way; it is here rather than in v34pcmif.c because four of its calls are
 * relocations against mangled member names -- `V90ConstellationDesigner::
 * setMinMaxRates`, `K56FlexFloModem::setMinMaxRates`, `V92EchoCanceller::
 * setEchoDelay` and `VPcmFloModem::setPcmSessionType` -- which a C translation
 * unit cannot emit.  Same arrangement, same reason, as `k56FlexPhase34` in
 * v34k56.cpp.
 *
 * ---------------------------------------------------------------------------
 * THERE ARE TWO SWITCHES ON `requestedDp` AND THEY ARE NOT THE SAME SWITCH.
 *
 * The first VALIDATES and can only ever write 0 back into the local; the
 * second DISPATCHES.  They have different default arms and that is the whole
 * reason both exist:
 *
 *   - validation runs only when `v90_receiver == 0 && requestedDp != 0`, so
 *     an unknown code asked for while a V.90 receiver is already up is NOT
 *     demoted, is NOT complained about, and reaches the dispatch intact --
 *     where the default arm clears both receiver counters.  Asked for with no
 *     receiver running it becomes 0, which is the arm that KEEPS a receiver.
 *     So the same argument means opposite things according to a field the
 *     caller does not pass.
 *   - 34 is accepted by the validation and then falls into the dispatch's
 *     default: `fa23c` set, both counters cleared.  It shares the tail in the
 *     object and shares it here.
 *
 * ---------------------------------------------------------------------------
 * THE RATE BLOCK IS SKIPPED, NOT SHORT-CIRCUITED.
 *
 * Three arms lead into the configuration re-read, and only the third computes
 * `rate_min` and `rate_max`.  If a V.90 receiver is up AND the session's
 * `info0Layout` is set, the two configured rates go to the constellation
 * designer instead; failing that, if a K56flex receiver is up AND the K56flex
 * object's own gate byte is set, they go to `K56FlexFloModem::setMinMaxRates`,
 * which in this object is a bare `ret`.  Only when neither owns them do they
 * become the V.34 object's own two indices.
 *
 * `v90_receiver != 0` with `info0Layout == 0` therefore FALLS THROUGH to the
 * K56flex test rather than skipping it, which is the case a reading of one
 * input at a time gets wrong.
 *
 * The division is `unsigned / 2400` -- `mul $0x1b4e81b5; shr $8` on the high
 * word, and 0x1b4e81b5 is ceil(2^40 / 2400).  The clamp then caps at 14 and
 * raises the max to the min if it is below it, IN THAT ORDER, which is the
 * same sequence v34fsk.h records for `VPcmV34SetMinMaxBitRates`.
 *
 * ---------------------------------------------------------------------------
 * TWO STORES ARE PRINTED BACK AS SHORTS AND MUST BE STORED FIRST.
 *
 * "V34 filtdelay set to %d" and "V34dmadelay set to %d" both re-read the
 * 16-bit field the line above wrote -- the object does `cwtl` on the value it
 * has just stored -- so a configuration large enough to overflow a short
 * prints the truncated number, not the arithmetic one.  Written in that order
 * here for that reason and not for tidiness.
 *
 * "V34dmadelay" IS THE OBJECT'S NAME FOR +0x25c, which v34fsk.h reached from
 * the other end and describes as "the base the echo filter's lag is measured
 * from".  The two agree: the same configured delay sets this field and, plus
 * 0x68, the echo canceller's own.
 *
 * ---------------------------------------------------------------------------
 * AND EVERY PATH ENDS IN `v34handshakinit` MODE 1, which rewrites a large
 * part of the object afterwards.  Nothing above it may be assumed observable
 * for that reason; test/mutations/v34retrain.json is where each pre-handshake
 * store is shown to survive.
 */
extern "C" void
VPcmV34InitiateRetrain(void *objp, unsigned char requestedDp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	struct v34_receiver *rx = (struct v34_receiver *)(m + OB_RECEIVER);
	unsigned char *sess = (unsigned char *)obj->p3548;
	unsigned char *cfg;
	unsigned char dp = requestedDp;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: Initiating retrain, "
				     "requested DP is %d\r\n", (int)dp);

	if (obj->v90_receiver != 0 && *(const int *)(sess + SESS_GATE) != 0) {
		V90ConstellationDesigner *cd;

		cfg = (unsigned char *)obj->pac3c;
		cd = *(V90ConstellationDesigner **)
		     (*(unsigned char **)(sess + SESS_DEMOD) + DEMOD_DESIGNER);
		cd->setMinMaxRates(*(const unsigned int *)(cfg + CFG_MIN_RATE),
				   *(const unsigned int *)(cfg + CFG_MAX_RATE));
	} else if (obj->k56flex_receiver != 0
		   && ((const unsigned char *)obj->pac18)[K56_ENABLED] != 0) {
		cfg = (unsigned char *)obj->pac3c;
		((K56FlexFloModem *)obj->pac18)->setMinMaxRates(
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

	/* Re-read on all three arms; the object reloads it after each. */
	cfg = (unsigned char *)obj->pac3c;

	/*
	 * The disconnect threshold, indexed EXACTLY as
	 * `VPcmV34SetMinimumSigLevel` indexes the same table (finding F270):
	 * bias by 0x30, reject the result unsigned so a negative level is out
	 * of range too, and fall back on ENTRY 3 rather than on either end.
	 * A `min`/`max` clamp would give entry 0 or entry 7 and be wrong at
	 * both.
	 *
	 * It lands in `rx_energy_floor`, which v34fsk.h independently
	 * describes as the floor `receiver` compares its 36-sample RMS against
	 * before declaring the line dead -- so "disconnect threshold" and that
	 * sentence are the same statement reached from two directions.
	 */
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

	/*
	 * `(delay + 2) >> 2` and NOT `(delay + 2) / 4`: the object shifts
	 * arithmetically, so a negative configured delay rounds towards minus
	 * infinity rather than towards zero.  The add is done unsigned for the
	 * same reason v34pcmif.c's rate step is -- wrapping is defined there
	 * and undefined on a signed int.
	 */
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

		obj->dmadelay = (short)(0x610u - (unsigned)ext);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34FEC, V34dmadelay set to %d, "
					     "(ext delay=%d)\n",
					     (int)obj->dmadelay, ext);
	}

	((V92EchoCanceller *)(sess + SESS_ECHO))->setEchoDelay(
		(unsigned)*(const int *)(cfg + CFG_EXT_DELAY) + 0x68u);

	/*
	 * The sensitive-ISP notice.  Through `edprintf` and NOT through a
	 * level gate, unlike the three above -- the same inconsistency
	 * v34pcmif.c records for `VPcmV34SetTxScale`, and the original's.
	 */
	if (*(const int *)(*(const unsigned char *const *)(sess + SESS_PCM)
			   + PCM_SENS) != 0) {
		edprintf("VPcmV34Main: Notifying Sensitive ISP "
			 "detected...\r\n");
		cfg = (unsigned char *)obj->pac3c;
		cfg[CFG_ISP] = (unsigned char)(cfg[CFG_ISP]
					       | CFG_ISP_SENSITIVE);
	}

	obj->is_short = 0;
	obj->local_short = 0;
	*(int *)(sess + SESS_RETRAIN_FLAG) = 0;

	/*
	 * SWITCH ONE: is the caller allowed what it asked for?  Skipped
	 * entirely unless no V.90 receiver is running and something was asked
	 * for -- see the note at the top of the function for why that matters.
	 */
	if (obj->v90_receiver == 0 && dp != DP_KEEP) {
		cfg = (unsigned char *)obj->pac3c;

		switch (dp) {
		case DP_V34:
			break;
		case DP_K56FLEX:
			if ((cfg[CFG_FLAGS] & CFG_FLAG_FLEX) != 0)
				break;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"VPcmV34InitiateRetrain: requested mod"
					" K56Flex, but params.flex is OFF, "
					"keeping same modulation!!!\r\n");
			dp = DP_KEEP;
			break;
		case DP_V90:
		case DP_V92:
			if ((cfg[CFG_FLAGS] & CFG_FLAG_V90) != 0)
				break;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"VPcmV34InitiateRetrain: requested mod"
					" V.%d, but params.v90 is OFF, keeping"
					" same modulation!!!\r\n", (int)dp);
			dp = DP_KEEP;
			break;
		default:
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"VPcmV34InitiateRetrain: unknown "
					"requested mod (%d), keeping same "
					"modulation!!!\r\n", (int)dp);
			dp = DP_KEEP;
			break;
		}
	}

	/* SWITCH TWO: act on it. */
	switch (dp) {
	case DP_KEEP:
		/*
		 * `> 0` and not `!= 0`: the object tests with `jle`, so a
		 * negative counter is left where it is rather than being
		 * pulled up to 1.  `VPcmV34SetV90RateReneg` can assign the
		 * field outright (D48), so negative is reachable in principle.
		 */
		if (obj->v90_receiver > 0)
			obj->v90_receiver = 1;
		break;

	case DP_K56FLEX:
		obj->k56flex_receiver = 1;
		obj->v90_receiver = 0;
		break;

	case DP_V90:
		((VPcmFloModem *)sess)->setPcmSessionType(0);
		obj->v90_receiver = 1;
		/*
		 * The one place `status` is read here, and only on this arm.
		 * v34pcmif.c reads 1 and 2 together as "a PCM receiver has the
		 * line"; this singles 2 out.
		 */
		if (obj->status == 2)
			m[OB_FAC00] = 1;
		break;

	case DP_V92:
		((VPcmFloModem *)sess)->setPcmSessionType(1);
		obj->v90_receiver = 1;
		break;

	case DP_V34:
		obj->fa23c = 1;
		/* FALLTHROUGH -- the object shares the default arm's tail. */
	default:
		obj->v90_receiver = 0;
		obj->k56flex_receiver = 0;
		break;
	}

	v34handshakinit(obj, 1);

	obj->f0004 = 7;
	obj->status = 0;
	*(int *)(m + OB_F2218) = 2;

	rx->f258 = 0;
	rx->f25a = 0;
	rx->f25c = 0;

	/*
	 * +0x254, and 336 is `x * 21 * 16` written as two `lea`s and a shift.
	 * The source short is read SIGNED, so a negative one gives a result
	 * below 10000.
	 */
	*(short *)(m + OB_F0254) =
		(short)(336 * (int)*(const short *)(m + OB_F35A4) + 10000);
	*(short *)(m + OB_F0254 + 2) = 0;
	*(int *)(m + OB_F0254 + 4) = 0;

	/*
	 * The 32 bytes at +0xac1c, cleared -- except for a three-short group
	 * at +0xac28 that only the originate/answer flag at +0x359c decides,
	 * and +0xac2e, the ONE short in the block left alone on every path.
	 * +0xac26 is not a second one: it is the upper half of the `movl` at
	 * +0xac24 and is written with it, which the mutation that narrows
	 * that store to a short measures rather than assumes.
	 *
	 * 0x65 and 0x66 are the two values `v34modeminit`, `preinitdigital`
	 * and `v34handshakinit` all test +0x359c against, so this fork is the
	 * same one and not a new enumeration.  0x39c3 goes in on BOTH arms;
	 * the pair 0x5a82/0x55fc that distinguishes them is 32768/sqrt(2) and
	 * a neighbour of it, which is what a quadrature pair looks like -- but
	 * nothing here reads the block back, so that is an observation and not
	 * a name.
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
}

/*
 * ---------------------------------------------------------------------------
 * V34GiveINFO1dBits -- take apart a received INFO1d, and undo a PCM upstream
 * the configuration does not allow.
 *
 * WHY IT IS HERE AND NOT IN v34info.c BESIDE ITS THREE SIBLINGS.  It names no
 * mangled symbol, so C would have compiled it -- but it calls
 * `VPcmV34InitiateRetrain`, which is `extern "C"` and lives in THIS file
 * because four of ITS calls are C++ members.  `$(SRC)` is every `.c` under
 * `src/`, and the interop tier links exactly that list into a 64-bit binary
 * with no C++ in it; a `V34GiveINFO1dBits` in `v34info.c` therefore links in
 * the 32-bit tests and fails `make phase` at `t_spandsp_v23` with an
 * undefined reference.  The first version of this function did exactly that.
 * Putting it beside its callee also matches the object, where all three of
 * `V34GiveINFO1dBits`, `VPcmV34InitiateRetrain` and `v90Phase34` are inside
 * one `VPcmV34Main.cpp` and this one prints "VPcmV34Main:" like the rest.
 * The declaration stays in `v34info.h` with its three siblings, which is the
 * arrangement `V34SetINFO1aBits` already has.
 *
 * ONE DECISION AND ONE CONSEQUENCE.  The decision is a three-way conjunction
 * -- the local V.92 capability byte, the remote's V.92 flag and one bit of the
 * arriving message -- written into `VPcmFloModem::pcmSessionType`, which is
 * the same "PCM upstream is in play" flag `V34GiveINFO1aBits` sets from the
 * received upstream baud index.  The consequence is that if the answer is yes
 * and the configuration bars it, the modem RETRAINS rather than refusing:
 * `VPcmV34InitiateRetrain(obj, DP_V90)`, whose own arm then puts the flag back
 * to 0 through `setPcmSessionType`.  The object's string is the whole story --
 * "we got PCM upstream under V.92Lite (after Info1d), retraining to V.34
 * upstream...".
 *
 * SO THE RETURN VALUE IS NOT THE FLAG, and the difference is not cosmetic.
 * `V34GiveINFO1aBits` really does return `pcmSessionType` read back; this one
 * keeps a separate register at 0 and raises it to 1 only where it retrains.
 * Two cases separate them: the retraining path returns 1 with the flag back at
 * 0, and a wanted-but-barred upstream returns 0 with the flag left at 1.
 *
 * THE FIRST STATEMENT IS UNCONDITIONAL, exactly as in `V34GiveINFO1aBits`:
 * the flag is cleared before `v90_receiver` is even looked at, so a call with
 * no V.90 receiver still clears it.
 *
 * WIDTHS.  Each of the four printed quantities is loaded at a width its value
 * alone would not show -- the ten message shorts `movzwl`, the capability byte
 * `movzbl`, `remote_v92` `movswl`, and the message bit printed as the MASKED
 * value, 32 and not 1.  None of the four changes a byte of state, so the
 * transcript comparison is the only tier that can see any of them; finding F335
 * is what test/unit/t_v34info1d.c does about that.
 */
extern "C" int
V34GiveINFO1dBits(void *objp, const short *bits)
{
	struct v34_object *obj = (struct v34_object *)objp;
	VPcmFloModem *sess = (VPcmFloModem *)obj->p3548;
	const unsigned char *cfg = (const unsigned char *)obj->pac3c;
	int ispcm;

	sess->pcmSessionType = 0;

	if (obj->v90_receiver == 0)
		return 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: giveINFO1dBits\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"VPcmV34Main: rxinfo1d = 0x%x,0x%x,0x%x,0x%x,0x%x,"
			"0x%x,0x%x,0x%x,0x%x,0x%x\n",
			(unsigned short)bits[0], (unsigned short)bits[1],
			(unsigned short)bits[2], (unsigned short)bits[3],
			(unsigned short)bits[4], (unsigned short)bits[5],
			(unsigned short)bits[6], (unsigned short)bits[7],
			(unsigned short)bits[8], (unsigned short)bits[9]);

	/*
	 * All three are tested, and at three different widths: the capability
	 * byte with `cmpb $0`, `remote_v92` with `cmpw $0`, and the message
	 * bit with `testb $0x20` on the LOW byte of index 7 -- so index 7's
	 * high byte cannot reach this decision.  Measured, not assumed.
	 */
	ispcm = 0;
	if (sess->v92modem.phase2Info->v92CapabilitiesLocal != 0
	    && obj->remote_v92 != 0
	    && ((unsigned short)bits[7] & 0x20) != 0)
		ispcm = 1;

	sess->pcmSessionType = ispcm;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"VPcmV34Main: upstream selection: local cap - %d, "
			"remote cap - %d, requested in info1 - %d, "
			"isPCM - %d\r\n",
			sess->v92modem.phase2Info->v92CapabilitiesLocal,
			obj->remote_v92,
			(unsigned short)bits[7] & 0x20,
			sess->pcmSessionType);

	/*
	 * Read back out of the session rather than reused: the object loads
	 * +0x611c a third time here, after the store and after the print.
	 */
	if (sess->pcmSessionType == 0)
		return 0;

	if (*(const signed char *)(cfg + CFG_V92LITE) < 0)
		return 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"VPcmV34Main: we got PCM upstream under V.92Lite "
			"(after Info1d), retraining to V.34 upstream...\r\n");

	VPcmV34InitiateRetrain(obj, DP_V90);
	*((unsigned char *)obj + OB_FAC00) = 1;

	return 1;
}

/*
 * ---------------------------------------------------------------------------
 * indicateJaTransmission -- whichever PCM modem is past phase 2 enters phase 3.
 *
 * Fifty-seven bytes, two loads, two compares and TWO TAIL JUMPS.  It stores
 * nothing, prints nothing and returns nothing: every path either falls into
 * `ret` with %eax never set or `jmp`s into a callee whose own return type is
 * `void`, so `void` is what the object supports and no more.
 *
 * THE TWO POINTERS ARE LOADED BEFORE EITHER TEST, into %edx and %ecx, and
 * only one of them is ever used.  That is register allocation and not a
 * claim: both loads are unconditional at the top because the tail jump needs
 * its argument already in hand, and neither pointer is dereferenced here.
 *
 * THE `+ 4` IS IN THE SOURCE AND NOT AN ADDRESSING ARTIFACT, and this entry
 * used to say the opposite.  `add $0x4,%eax` followed by
 * `cmpl $0x1,0x248(%eax)` and `cmpl $0x1,0x24c(%eax)` is `obj + 0x24c` and
 * `obj + 0x250` -- `v90_receiver` and `k56flex_receiver`, so the ARITHMETIC
 * was read right.  What was wrong was calling the `add` free: written as
 * plain member reads it does not appear at all, and its absence is the one
 * instruction lever 2 reports (padding-stripped, ours 12 to the blob's 13).
 *
 * TWO CONDITIONS ARE NEEDED TOGETHER AND THE CROSS PRODUCT SEPARATES THEM.
 * The anchor must be created AFTER both session pointers are in hand -- all
 * six spellings declared ahead of them fold the `+4` into the two
 * displacements and emit the pristine bytes exactly, 12 cells, none at zero
 * -- and BOTH tests must go through it: a control with the anchor after the
 * loads but only the first test through it folds back too.  With both, seven
 * of ten cells reach zero, so what is decoded is that FACT and not this
 * spelling.  Naming the two offsets was measured separately and costs
 * nothing, 3 cells, 2 at zero.
 *
 * BOTH TESTS ARE SIGNED AND BOTH ARE `> 1`, not `!= 0` and not `>= 1`:
 * `cmpl $0x1,...; jg`.  So a receiver that has been noticed but has not got
 * past `1` does NOT enter phase 3 -- which is the state
 * `VPcmV34InitiateRetrain` and `V34GiveINFO1aBits` leave behind when they
 * write 1 rather than 2 -- and a negative value is below 1 rather than above
 * it.  `v34pcmif.c`'s `VPcmV34GetMaxUpstreamRateIndex` and this file's
 * `v90Phase34` read the same field with the same `> 1`.
 *
 * THE K56FLEX ARM'S INTERIOR IS UNOBSERVABLE, and this is said here rather
 * than left for a reader to discover: `K56FlexFloModem::enterPhase3FullDuplex`
 * is one byte of code in the object -- a bare `ret` at 0x101d0 -- so nothing
 * downstream of the second test can be seen by any test that drives this
 * function.  The second condition, the object it is given, and `else if`
 * against two independent `if`s are one equivalence class for as long as that
 * stays true.  Its POSITION is not in that class and is tested: putting the
 * K56flex test first changes what happens when both receivers are above 1.
 * Finding F381 is the record and test/mutations/v34ja.json carries both.
 *
 * WHY IT IS HERE.  It is inside VPcmV34Main.cpp's run in the object -- between
 * `V34XF_IndicateTrn2dReceived` at 0xa390 and `chkForceBaudRate` at 0xa450 --
 * and it calls two C++ members, so finding F333's rule puts it in this file
 * rather than in `v34pcmif.c`: a `.c` may not call anything defined in a
 * `.cpp`, because the six interop binaries link every `.c` under `src/` with
 * no C++ object among them.  Its two callers are both inside `v34handshak`,
 * which is why the declaration sits in `v34hshak.h` beside `v90Phase34`.
 */
extern "C" void
indicateJaTransmission(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	VPcmFloModem *sess = (VPcmFloModem *)obj->p3548;
	K56FlexFloModem *k56 = (K56FlexFloModem *)obj->pac18;
	const unsigned char *m = (const unsigned char *)objp + OB4_ANCHOR;

	if (*(const int *)(m + OB4_V90_RECEIVER) > 1)
		sess->enterPhase3();
	else if (*(const int *)(m + OB4_K56_RECEIVER) > 1)
		k56->enterPhase3FullDuplex();
}

/*
 * ---------------------------------------------------------------------------
 * THE LAST TWO OF `vpcm_run`'s FIVE CALLEES THAT ARE NOT `VPcmV34Progress`.
 *
 * `include/dsplib/vpcm.h` declares all five WEAK; `src/pump/v34/v34pcmif.c`
 * defines two of them and explains the mechanism at length -- the
 * declarations are taken with `DSPLIB_VPCM_UNWRITTEN` empty, which is what
 * makes a definition STRONG, and defining one under the weak macro would link
 * identically today and stop being a definition the moment a real one
 * appeared.  These two follow it exactly.
 *
 * THEY ARE HERE AND NOT IN THAT `.c` FOR ONE REASON: `VPcmV34GetCurrentRxBitRate`
 * CALLS A C++ MEMBER.
 *
 *     6f89  e8 fc ff ff ff   call <R_386_PC32 _ZNK14V90Demodulator10getBitRateEv>
 *
 * A C translation unit cannot name that symbol -- CLAUDE.md's trap section,
 * "the link line is necessary and not sufficient".  Its twin comes with it
 * because the two share every offset they read and splitting them would put
 * one table of session offsets in two files.
 *
 * ---------------------------------------------------------------------------
 * WHAT THE TWO ACTUALLY ANSWER, and they are NOT each other's mirror: 90
 * bytes against 213, with three arms on one side and five on the other.
 *
 * Both open on the same pair of tests, and the pair is CROSSED rather than
 * nested -- `f359c` picks which question is asked of `status`, and the two
 * questions are different:
 *
 *     6f48  cmpw   $0x66,0x359c(%edx)          Rx
 *     6f62  je     6f78                          == 0x66 -> status in 1..2
 *     6f64  cmpl   $0x3,(%edx)                   != 0x66 -> status == 3
 *
 *     6fad  cmpw   $0x66,0x359c(%edx)          Tx
 *     6fb5  je     6fd1                          == 0x66 -> status 2, then 3
 *     6fb7  mov    (%edx),%eax; dec; cmp $1; ja  != 0x66 -> status in 1..2
 *
 * `f359c` is the field v34fsk.h describes as selecting
 * `setTimingStateParameters`' second parameter table and `VPcmV34InitiateRetrain`
 * reads as `role`; 0x66 is the value its other readers pair with V.90 being
 * available.  `(unsigned)(status - 1) <= 1` is the SAME test the three
 * Initiate entry points fork on, which is where v34pcmif.c's comment says the
 * meaning of status 1 and 2 comes from -- a PCM receiver has the line.
 *
 * AND EVERY PATH THAT IS NOT A PCM ONE ENDS IN THE SAME PLACE: the rate
 * configuration at +0xaa84, times 2400.  `VPcmV34GetCurrentTxBitRate` reads
 * its `txbits` at +0x04 and `...RxBitRate` its `rxbits` at +0x14, both with
 * `movswl` -- so both are SIGNED and a negative index gives a negative rate
 * rather than a huge one.  v34fsk.h's note on `struct v34_ratecfg` lists four
 * "current" getters and names no reader for `rxbits`; this is that reader.
 */

/*
 * `f359c`'s PCM value.  `VPcmV34InitiateRetrain` already switches on 0x65 and
 * 0x66 of the same field a few hundred lines above and spells them inline;
 * this is 0x66 given a name because two functions here now compare against
 * it and a bare 0x66 in four places is four chances to transcribe 0x65.
 */
#define PCM_ROLE		0x66

/*
 * The transmit chain the two PCM arms walk, which is TWO DIFFERENT OBJECTS
 * read at the same two shapes.  Neither is modelled anywhere in this tree, so
 * these are offsets and not fields:
 *
 *     6fc7  83 7a 2c 03   cmpl $0x3,0x2c(%edx)    the gate, both arms
 *     6ff1  8b 4a 40      mov  0x40(%edx),%ecx    V.90: the frame record
 *     7057  8b 42 4c      mov  0x4c(%edx),%eax    V.92: the frame record
 *     6ff8  69 48 04 ..   imul $0x1f40,0x4(%eax)  and its +0x04, both arms
 *
 * The last is one indirection deeper than it looks: +0x40 and +0x4c hold a
 * pointer to a pointer, and the count is at +0x04 of what the SECOND one
 * addresses.
 */
#define PCMTX_STATE		0x2c
#define PCMTX_READY		3
#define PCMTX_V90_FRAME		0x40
#define PCMTX_V92_FRAME		0x4c
#define PCMTX_FRAME_BITS	0x04

/*
 * 0x7530.  The K56flex transmit rate is a CONSTANT in this object -- there is
 * no chain, no gate and nothing to read -- and finding F1090 is why naming the
 * modulation is the whole of what a K56flex session does in this build.
 */
#define K56FLEX_TX_BITRATE	30000

/*
 * The receive rate.
 *
 * THREE ARMS, and the middle one is the only place in this file that
 * dereferences `pac18` as an `int *`: `V34GiveINFO1aBits` reads +0xc of the
 * same pointer as the local PCM type, so what is at +0 is not otherwise
 * described and is spelled as an offset.
 */
extern "C" int
VPcmV34GetCurrentRxBitRate(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	const struct v34_ratecfg *cfg =
	    (const struct v34_ratecfg *)((unsigned char *)obj + V34_RATECFG);

	if (obj->f359c == PCM_ROLE) {
		if ((unsigned)(obj->status - 1) <= 1) {
			VPcmFloModem *sess = (VPcmFloModem *)obj->p3548;

			return (int)sess->modem.demodulator->getBitRate();
		}
	} else if (obj->status == 3) {
		return *(const int *)obj->pac18;
	}

	return cfg->rxbits * (int)RATE_STEP;
}

/*
 * The transmit rate, which is where the PCM UPSTREAM rates come from.
 *
 * FIVE ARMS.  Two of them are the same eleven instructions over two different
 * chains and two different constants, and the object SHARES THEIR TAIL --
 * 0x7073 jumps back into 0x700d, so the rounding sequence exists once.  That
 * is a code-generation fact and not a source one; what the source has to have
 * is one expression written twice, which is what is below.
 *
 *   f359c != 0x66, status 1 or 2   the V.90 modulator at `modem.modulator`,
 *                                  its +0x40, one indirection, +0x04 of that,
 *                                  times 8000/6
 *   f359c == 0x66, status 2        the object at the session's +0x6124, its
 *                                  +0x4c, one indirection, +0x04 of that,
 *                                  times 8000/12
 *   f359c == 0x66, status 3        a flat 0x7530 -- 30000
 *   everything else                `txbits` * 2400
 *
 * BOTH CHAINS ARE GATED ON `+0x2c == 3` AND RETURN ZERO OTHERWISE, and the
 * zero is a RETURN and not a fall-through to the 2400 arm:
 *
 *     6fc5  31 c0              xor    %eax,%eax
 *     6fc7  83 7a 2c 03        cmpl   $0x3,0x2c(%edx)
 *     6fcb  74 24              je     6ff1
 *     6fcd  83 c4 14  c3       add;ret                     <- 0 goes out
 *
 * Neither object is modelled anywhere in this tree, so both are reached as
 * bytes; +0x2c is a state the two share and 3 is the only value either is
 * tested against.
 *
 * 8000/6 AND 8000/12 ARE THE TWO PCM FRAME GRANULARITIES, and they are the
 * whole reason the two arms differ -- see `V90Demodulator::getBitRate`, which
 * computes the same expression with the same two `float` constants for the
 * other direction.  Both are `* (1.0f/N)` and not `/ N.0f`, because the
 * object multiplies (`fmuls`) by the nearest `float` to the reciprocal and a
 * division would have been `fdivs`.
 *
 * THE V.92 ARM'S `tx` USED TO BE `*(const unsigned char *const *)sess->
 * pad_6124`, a pointer read out of a four-byte pad.  It is the SAME field
 * the V.90 arm spells `sess->modem.modulator`, one class along: +0x6124 is
 * where the `VPcmFloModem`'s `V92Modem` starts and +0x000 of a `V92Modem` is
 * its `V92Modulator *`.  The construction path named the member; the offset
 * and the read are unchanged, and nothing here was re-measured.
 *
 * The conversion in and out is UNSIGNED at both ends, by the same `fildll`
 * off a zeroed high word and `fistpll` with the low half taken that settled
 * `getBitRate`'s return type.  Written as `unsigned` here for that reason and
 * not because a rate cannot be negative.
 */
extern "C" int
VPcmV34GetCurrentTxBitRate(void *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	VPcmFloModem *sess = (VPcmFloModem *)obj->p3548;
	const struct v34_ratecfg *cfg =
	    (const struct v34_ratecfg *)((unsigned char *)obj + V34_RATECFG);
	const unsigned char *tx;
	unsigned int n;

	/*
	 * ONE `txbits` SITE AND NOT TWO.  The object reaches its
	 * `movswl 0xaa88; imul $0x960` at 0x6fe0 from BOTH the role-0x66
	 * switch falling through and the `ja` at 0x6fbd, so the arms that do
	 * not answer fall out of the `if` rather than each returning the
	 * configuration themselves.  Written that way here: a second copy is
	 * a second thing to keep in step, and the mutation that made it read
	 * `txbits` unsigned found only one of them.
	 */
	if (obj->f359c != PCM_ROLE) {
		if ((unsigned)(obj->status - 1) <= 1) {
			tx = (const unsigned char *)sess->modem.modulator;
			if (*(const int *)(tx + PCMTX_STATE) != PCMTX_READY)
				return 0;

			n = *(const unsigned int *)
			    (**(const unsigned char *const *const *)
			       (tx + PCMTX_V90_FRAME) + PCMTX_FRAME_BITS);

			return (int)(unsigned)(n * 8000u * (1.0f / 6.0f)
					       + 0.5f);
		}
	} else if (obj->status == 2) {
		tx = (const unsigned char *)sess->v92modem.modulator;
		if (*(const int *)(tx + PCMTX_STATE) != PCMTX_READY)
			return 0;

		n = *(const unsigned int *)
		    (**(const unsigned char *const *const *)
		       (tx + PCMTX_V92_FRAME) + PCMTX_FRAME_BITS);

		return (int)(unsigned)(n * 8000u * (1.0f / 12.0f) + 0.5f);
	} else if (obj->status == 3) {
		return K56FLEX_TX_BITRATE;
	}

	return cfg->txbits * (int)RATE_STEP;
}

/*
 * ---------------------------------------------------------------------------
 * `VPcmV34Progress` -- 0xb3c0, 7,278 bytes.  One block of samples through
 * whichever of V.34, V.90, V.92 and K56flex has the line.
 *
 * IT IS THE WHOLE V.PCM RUN PATH.  `vpcm_run` (src/pump/v90/vpcm.c) converts
 * the host's shorts to floats, fetches the transmit bits, calls this once per
 * block and converts back; everything a connecting call does between those
 * two conversions is here.  Finding F1454 measured what that means: traced
 * under callgrind, a real 33,600 V.34 connect executes exactly ONE symbol
 * this tree had not written, and it was this one.
 *
 * THE SHAPE IS THREE DISPATCHES, NOT ONE.
 *
 *   1. `obj->status`, 0 to 10, the jump table at .rodata+0x2d4 -- which modem
 *      owns the line and what phase it is in.  Anything above 10 is a bug and
 *      says so ("Illegal Modem State").
 *   2. the modem's own answer -- `runPcmModem`, `k56FlexRunDemodulator` and
 *      `v90RunDemodulator` each return a small code that the tables at
 *      +0x300, +0x324 and +0x3bc turn into a new `f0004` or a retrain.
 *   3. `obj->f0004` itself, the table at +0x340, reached only from the V.34
 *      arm -- five of its seventeen entries do anything, and one of those is
 *      a fourth table (+0x384) that turns a modem-on-hold code into a timeout
 *      in seconds.
 *
 * `f0004` IS THE RETURN VALUE AND IS ALSO A LOCAL.  The object reads it into
 * %esi at the top of each arm, switches on that copy, and returns THAT --
 * `obj->f0004` is written by several arms after the copy is taken and the
 * function still returns the older value.  `ret` below is that copy, assigned
 * exactly where the object assigns it and nowhere else.  Two arms re-read it
 * only when the debug level is up, which changes what the function returns;
 * see D296.
 *
 * WHAT IS NOT WRITTEN, and how it is kept from breaking the link.  Seven
 * symbols this function calls are unreconstructed -- four `VPcmFloModem`
 * members, `GenericToneDetector::process`, and the two rate-renegotiation
 * transmitters -- 7,922 bytes belonging to the V.90 and V.92 arms.  Each is
 * declared WEAK by the three macros defined at the top of this file, so the
 * reference resolves to zero rather than leaving 78 test binaries with an
 * undefined symbol, and each call site tests the pointer first and records
 * `v34pcm_notwritten` when it is null.  That is `vpcm.h`'s arrangement for
 * the five `VPcmV34*` entry points, one level further down; the boundary
 * moved, it did not go away.  `t_vpcmguard.c` is the binary that watches it
 * stop.
 *
 * NONE OF THE SEVEN IS ON A V.34 CALL.  Finding F1454's trace is what says so
 * and it is the reason this function could be written at all; a V.90 or V.92
 * connect enters arms no test in this tree can yet drive.
 */

/*
 * ---------------------------------------------------------------------------
 * FIVE ALIASES THAT EXIST ONLY TO BE COMPARED WITH ZERO, and finding F1460 is
 * why they have to.
 *
 * The five C++ members below are declared WEAK, `nm` shows all five as `w`,
 * and the free functions beside them (`v90RateReneg`, `v90RateRenegSilence`)
 * are weak in exactly the same way.  Test a free one for null and GCC leaves
 * the comparison alone.  Test a MEMBER -- `&VPcmFloModem::runPcmModem == 0`
 * -- and GCC 13 folds it to false and says so:
 *
 *     warning: the address 'VPcmFloModem::runPcmModem' will never be NULL
 *
 * which is finding F985's trap in a form the `weak` attribute does not fix:
 * the guard would compile away and the call would go to address zero.  A
 * pointer-to-member is not an address as far as that optimisation is
 * concerned, and no spelling of the member reference avoids it.
 *
 * So the TEST is made through an ordinary function pointer wearing the
 * member's mangled name -- `this` is a member's first stack argument on this
 * ABI, so the declaration is the same function seen the other way round --
 * and the CALL is still written as a call.  Nothing here is an alias for
 * anything but the null test.
 */
extern int alias_runPcmModem(VPcmFloModem *, float *, float *, unsigned int,
			     int *, int *, int *, int *)
	__asm__("_ZN12VPcmFloModem11runPcmModemEPfS0_jPiS1_S1_S1_")
	__attribute__((weak));
extern int alias_v90RunDemodulator(VPcmFloModem *, float *, unsigned int,
				   int *, int *)
	__asm__("_ZN12VPcmFloModem17v90RunDemodulatorEPfjPiS1_")
	__attribute__((weak));
extern int alias_qcLineVerification(VPcmFloModem *, float *, float *,
				    unsigned int, int *, int *, int *, int *)
	__asm__("_ZN12VPcmFloModem18qcLineVerificationEPfS0_jPiS1_S1_S1_")
	__attribute__((weak));
extern void alias_vPcmResetPhase3Modem(VPcmFloModem *)
	__asm__("_ZN12VPcmFloModem20vPcmResetPhase3ModemEv")
	__attribute__((weak));
extern int alias_toneDetectorProcess(GenericToneDetector *, float *,
				      unsigned int)
	__asm__("_ZN19GenericToneDetector7processEPfj")
	__attribute__((weak));

/* `mov 0x...(%esi)` sites in regions v34fsk.h models as `unmapped_*`. */
#define PROG_S16(o, off)	(*(short *)((unsigned char *)(o) + (off)))
#define PROG_U16(o, off)	(*(unsigned short *)((unsigned char *)(o) + (off)))
#define PROG_S32(o, off)	(*(int *)((unsigned char *)(o) + (off)))
#define PROG_U8(o, off)		(*(unsigned char *)((unsigned char *)(o) + (off)))

/*
 * +0x0238  Samples this session has processed, masked to 31 bits on every
 * block.  `datapumpv34` reads it at its true offset and v34fsk.h's note on
 * `unmapped_0234` records the two other readers.
 */
#define O_SAMPLES	0x238
/*
 * +0x0240 and +0x0244.  The first is named by the object -- "On
 * PHASE2_COMPLETE: added Silence = %d, p2DelayCntr = %d" -- and the second by
 * "phase3halfDuplexLength = %d symbols (baud %d)".
 */
#define O_P2DELAY	0x240
#define O_HDLENGTH	0xa244
/*
 * +0x0254 to +0x0258, adaptecho's DC estimator: a countdown, the estimate the
 * loop subtracts from every sample, and the accumulator averaged into it
 * every 128 samples.  "Estimated DC = %d  (acc = %d)" names the last two.
 */
#define O_DCCOUNT	0x254
#define O_DCEST		0x256
#define O_DCACC		0x258
/* +0x0262.  Zero means the object is not running and the function returns. */
#define O_RUNNING	0x262
/* +0xa248.  Set with the half-duplex length and cleared by three arms. */
#define O_HDSET		0xa248
/* +0x0e4c.  A short set to 1 by each of the three "modem is up" arms. */
#define O_MODEMUP	0xe4c
/* +0xabc4.  Non-zero once the session has settled which PCM modem it is. */
#define O_PCMCHOSEN	0xabc4
/*
 * +0xabd8 and +0xabdc.  "Modem On Hold approved by phase2 (ISP timeout is %d
 * seconds)" names the first; the second counts samples against it.  -1 is
 * "no limit" and is tested for as such.
 */
#define O_MOHLIMIT	0xabd8
#define O_MOHCOUNT	0xabdc
/*
 * +0xabe4, +0xabe6 and +0xabf9, the three words of the modem-on-hold request
 * `VPcmV34InitMOH` clears or stores and nothing here reads.  Offsets, not
 * names: no string and no reader says what any of them carries.
 */
#define OB_MOH_W4	0xabe4
#define OB_MOH_W6	0xabe6
#define OB_MOH_FLAG	0xabf9
/* +0xabe9.  Gates the late ANSam case on an outgoing call. */
#define O_ANSAMLATE	0xabe9
/* +0xabfe and +0xabff.  The output-clear request, and the phase-2 substate. */
#define O_CLEARREQ	0xabfe
#define O_P2STATE	0xabff
/*
 * +0xac1c, the retrain detector's eleven words.  Four filter states, three
 * coefficients, a signal counter and two energies with a block counter --
 * "retrainDetector() notchDetectSigCnt = %d energyInp>>NOTCH_IN_OUT_RATIO_
 * SHIFT = %d energyOut = %d" names the last three and the counter.
 */
#define O_NOTCH_S0	0xac1c
#define O_NOTCH_S1	0xac1e
#define O_NOTCH_S2	0xac20
#define O_NOTCH_S3	0xac22
#define O_NOTCH_CNT	0xac24
#define O_NOTCH_K0	0xac28
#define O_NOTCH_K1	0xac2a
#define O_NOTCH_K2	0xac2c
#define O_NOTCH_EIN	0xac30
#define O_NOTCH_EOUT	0xac34
#define O_NOTCH_BLK	0xac38
/*
 * +0xac40 to +0xac48, the three words `requestOutputSampleClear` writes and
 * nothing here reads.  v34fsk.h's `unmapped_ac40` is the twelve bytes this
 * arm is the only reason to model at all.
 */
#define O_CLR_FLAG	0xac40
#define O_CLR_COUNT	0xac44
#define O_CLR_DONE	0xac48

/* +0x25c2, `testb $0x10` -- transmit through the datapump rather than the
 * handshake.  v34fsk.h names it `f25c2`. */
#define PROG_TXBIT_DATA		0x10

/* The two `f359c` roles, `cmpw $0x65` and `$0x66` at 0xb50c and 0xb522. */
#define PROG_ROLE_ORIGINATE	0x65
#define PROG_ROLE_ANSWER	0x66

/* The `+0x6c0c` block of VPcmFloModem, read as floats by the V.PCM arm. */
#define SESS_OUTBLOCK		0x6c0c
/* `V92Phase2Info::shortPhase2Local`, cleared through the session. */
#define SESS_V92_P2INFO		0x612c

/* `pac3c`'s flag bytes, `orb`/`andb`/`testb` sites. */
#define CFG_FLAGS3		3
#define CFG_FLAG3_RETRAIN	4
#define CFG_FLAG3_PHASE2	2
#define CFG_FLAGS2		2
#define CFG_FLAG2_SAMELINE	0x20
#define CFG_FLAGS51		0x51
#define CFG_FLAG51_CLEAR	1
#define CFG_SILENCE		0x6c

/* The retrain detector's three thresholds and its block length. */
#define NOTCH_BLOCK		0x40
#define NOTCH_EIN_MIN		0x249f0
#define NOTCH_EOUT_MAX		0x22550f
#define NOTCH_SIGCNT_MAX	5

/* `hist_2f58` wraps here; `cmp $0x257,%dx` and it is a 16-bit compare. */
#define PROG_HIST_LAST		0x257

/*
 * ---------------------------------------------------------------------------
 * The unwritten-path record.  `vpcm.c`'s `vpcm_notwritten` verbatim in shape,
 * and for the reason `v34hshak.c`'s `t3m_notwritten` gives: an arm that
 * returns quietly is indistinguishable from an arm that correctly did
 * nothing, so the default is to STOP, and a test opts out of the stop BY NAME
 * before it reads the code.
 */
static int v34pcm_unwritten_code;
static int v34pcm_unwritten_soft;

extern "C" int
v34pcm_unwritten(void)
{
	return v34pcm_unwritten_code;
}

extern "C" void
v34pcm_unwritten_reset(void)
{
	v34pcm_unwritten_code = V34PCM_WRITTEN;
	v34pcm_unwritten_soft = 1;
}

static void
v34pcm_notwritten(int what)
{
	if (v34pcm_unwritten_code == V34PCM_WRITTEN)
		v34pcm_unwritten_code = what;
	if (!v34pcm_unwritten_soft)
		abort();
}

extern "C" int
VPcmV34Progress(void *objp, float *in, float *out, int nin, int *rxbits,
		int *nrx, int *txbits, int *nbits)
{
	struct v34_object *obj = (struct v34_object *)objp;
	VPcmFloModem *sess = (VPcmFloModem *)obj->p3548;
	K56FlexFloModem *k56 = (K56FlexFloModem *)obj->pac18;
	const int n = nin & ~3;
	int st;
	int ret;
	int prev;
	int r;
	int i;
	int t;

	/*
	 * 0xb41f.  A 16-bit test, and the only exit that does not go through
	 * the tail: the object is not running, so nothing is consumed and the
	 * last progress code is repeated.
	 */
	if (PROG_S16(obj, O_RUNNING) == 0)
		return obj->f0004;

	PROG_S32(obj, O_SAMPLES) = (PROG_S32(obj, O_SAMPLES) + n) & 0x7fffffff;

	/*
	 * 0xb46a-0xb52a.  The transmit bits are packed into whole 16-bit
	 * words for the three cases that have a V.34 modulator on the line:
	 * idle, the answerer in V.90/V.92, and the originator in K56flex.
	 */
	st = obj->status;
	if (st == 0
	    || (st == 1 && obj->f359c == PROG_ROLE_ANSWER)
	    || (st == 3 && obj->f359c == PROG_ROLE_ORIGINATE)) {
		int nwords = *nbits >> 4;

		if (nwords > 0) {
			int src = 0;
			int done = 0;
			int idx = obj->tx_n;

			do {
				unsigned int w = 0;
				int b;

				for (b = 0; b <= 15; b++)
					w += (unsigned int)
					     (txbits[src++] & 1) << b;
				obj->tx_data[idx] = (int)w;
				obj->tx_n = ++idx;
				done++;
			} while ((*nbits >> 4) > done);
		}
	}

	/*
	 * 0xb4d8.  One byte in the session turns the entrance filter on; it
	 * runs in place over the caller's buffer, and the state is re-read
	 * afterwards because the filter is allowed to change it.
	 */
	if (sess->byte_7f5c != 0) {
		sess->entFilt.process(in, in, (unsigned int)n);
		st = obj->status;
	}

	if ((unsigned int)st > 10) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmV34Main: Illegal Modem "
					     "State !!!!\r\n");
		goto reload;
	}

	switch (st) {

	/* --- 0: V.34, and the only arm that reaches the +0x340 table ----- */
	case 0:
		ret = obj->f0004;
		prev = ret;
		if (n != 0) {
			int left = n;

			do {
				obj->f260 = (short)*in++;
				modem_serrint(obj);
				*out++ = (float)obj->f25e;
				if (obj->txq.count < obj->f2aa0
				    || obj->rxq.count > 5) {
					if (obj->f25c2 & PROG_TXBIT_DATA)
						datapumpv34(obj);
					else
						v34handshak(obj);
				}
			} while (--left != 0);
			ret = obj->f0004;
		}
		/*
		 * 0xc100.  A code that CHANGED to 4 or 5 during the block --
		 * the two connect codes -- arms the retrain bit; the code is
		 * then re-read because `pac3c` is shared with the handshake.
		 */
		if (ret != prev) {
			if ((unsigned int)(ret - 4) <= 1) {
				PROG_U8(obj->pac3c, CFG_FLAGS3)
				    |= CFG_FLAG3_RETRAIN;
				ret = obj->f0004;
			}
		}
		/*
		 * 0xc127.  Still idle, and the session has been running for
		 * between 4,800 and 4,800 + n samples: the window is one
		 * block wide, so it fires exactly once.  "Masking CAS
		 * detection after %d in train".
		 */
		if (ret == 0) {
			int since = PROG_S32(obj, O_SAMPLES)
				    - PROG_S32(obj, 0x244);

			if ((unsigned int)since > 0x12bfu
			    && (unsigned int)since < (unsigned int)(n + 0x12c0)) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmV34Main: Masking CAS "
					    "detection after %d in train..."
					    "\r\n", since);
				PROG_U8(obj->pac3c, CFG_FLAGS3)
				    &= (unsigned char)~CFG_FLAG3_RETRAIN;
				ret = obj->f0004;
			}
		}

		/* 0xc172, the transmit queue compaction. */
		{
			int rd = obj->tx_rd;
			int wr = obj->tx_n;
			int k = 0;

			while ((unsigned int)wr > (unsigned int)rd)
				obj->tx_data[k++] = obj->tx_data[rd++];
			obj->tx_rd = 0;
			obj->tx_n = k;
		}

		if (ret <= 3) {
			*nbits = 0;
		} else {
			int avail = obj->nof_tx_bits;

			if (avail != 0) {
				avail -= obj->tx_n;
				if (avail <= 0)
					avail = 0;
				else
					avail <<= 4;
			}
			*nbits = avail;
		}

		/* 0xc1e7, the receive queue unpacked one bit per int. */
		{
			int *p = rxbits;
			unsigned int k = 0;

			while ((unsigned int)obj->rx_n > k) {
				unsigned int w = (unsigned int)obj->rx_data[k];
				int b;

				for (b = 15; b >= 0; b--) {
					*p++ = (int)(w & 1);
					w >>= 1;
				}
				k++;
			}
			*nrx = obj->rx_n << 4;
			obj->rx_n = 0;
		}

		if ((unsigned int)ret > 0x10)
			goto done;

		switch (ret) {

		/* Phase 2 completed.  0xc360. */
		case 1:
			if (prev == 0) {
				int baud = PROG_S16(obj, V34_RATECFG);
				int ofs = PROG_S16(obj, V34_RATECFG + 2);
				int len;

				PROG_U8(obj, O_P2STATE) = 0;
				PROG_S32(obj, O_P2DELAY) = 0;
				if ((PROG_U8(obj->pac3c, CFG_FLAGS3)
				     & CFG_FLAG3_PHASE2) != 0
				    && PROG_S16(obj, O_PCMCHOSEN) != 0
				    && PROG_S16(obj, O_HDSET) == 0) {
					/*
					 * 0xc3ae.  Five symbol periods per
					 * baud unit plus the configured
					 * offset, less 588 -- and this is the
					 * only path that latches +0xa248.
					 */
					PROG_S16(obj, O_HDSET) = 1;
					len = baud * 5 + ofs - 0x24c;
				} else {
					/*
					 * 0xcff9, the other length: fifteen
					 * eighths of the baud unit.
					 */
					len = (((baud << 4) - baud) >> 3)
					      + ofs - 0x24c;
				}
				PROG_S32(obj, O_HDLENGTH) = len;
				edprintf("VPcmV34Main: phase3halfDuplexLength"
					 " = %d symbols (baud %d)\r\n",
					 len, baud);
				if (obj->v90_receiver > 1) {
					if (alias_vPcmResetPhase3Modem == 0)
						v34pcm_notwritten(
						    V34PCM_UNWRITTEN_RESETP3);
					else
						sess->vPcmResetPhase3Modem();
				} else if (obj->k56flex_receiver > 1) {
					k56->k56FlexEnterPhase3();
				}
			}
			t = PROG_S32(obj, O_P2DELAY) + n;
			PROG_S32(obj, O_P2DELAY) = t;
			r = PROG_U8(obj, O_P2STATE);
			if (r == 0) {
				if (t <= 0x5f) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "VPcmV34Main: Wait (before"
						    " P2 COMPLETE)...\r\n");
					return 0;
				}
				PROG_U8(obj, O_P2STATE) = 1;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmV34Main: Indicating First P2 "
					    "COMPLETE... (after %d)\r\n",
					    PROG_S32(obj, O_P2DELAY));
				return 1;
			}
			if (r == 1) {
				/* 0xcfa3, the configured extra silence. */
				int sil = PROG_S32(obj->pac3c, CFG_SILENCE);

				t += sil;
				PROG_S32(obj, O_P2DELAY) = t;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "On PHASE2_COMPLETE: added Silence"
					    " = %d, p2DelayCntr = %d\r\n",
					    PROG_S32(obj->pac3c, CFG_SILENCE),
					    t);
			}
			if (PROG_S32(obj, O_P2DELAY) <= 0x240) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmV34Main: Wait (after P2 "
					    "COMPLETE)...\r\n");
				PROG_U8(obj, O_P2STATE)++;
				return 1;
			}
			if (prev != 1)
				goto reload;
			/*
			 * 0xcf1d.  The same move as case 2 below and NOT the
			 * same code: this copy has no V.90 branch at all --
			 * a session whose `pcmSessionType` is 0 falls
			 * straight through to the K56flex test.  Written
			 * twice because the object has it twice.
			 */
			if (obj->v90_receiver > 1
			    && sess->pcmSessionType != 0) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmV34Main: Moving to Phase3 "
					    "Modem V92..\r\n");
				obj->status = 2;
			}
			if (obj->k56flex_receiver > 1) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmV34Main: Moving to Phase3 "
					    "Modem K56Flex..\r\n");
				obj->status = 3;
				PROG_S16(obj, O_DCCOUNT) = 0;
			}
			goto reload;

		/* Move to phase 3.  0xc2d2. */
		case 2:
			if (obj->v90_receiver > 1) {
				if (sess->pcmSessionType != 0) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "VPcmV34Main: Moving to "
						    "Phase3 Modem V92..\r\n");
					obj->status = 2;
				} else {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "VPcmV34Main: Moving to "
						    "Phase3 Modem V90..\r\n");
					obj->status = 1;
				}
			}
			if (obj->k56flex_receiver > 1) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmV34Main: Moving to Phase3 "
					    "Modem K56Flex..\r\n");
				obj->status = 3;
				PROG_S16(obj, O_DCCOUNT) = 0;
			}
			goto reload;

		/* Modem on hold approved.  0xc481 and the +0x384 table. */
		case 13:
		{
			int secs;

			switch ((unsigned short)obj->fabe0) {
			case 1:		secs = 0xa;	break;
			case 2:		secs = 0x14;	break;
			case 3:		secs = 0x1e;	break;
			case 4:		secs = 0x28;	break;
			case 5:		secs = 0x3c;	break;
			case 6:		secs = 0x78;	break;
			case 7:		secs = 0xb4;	break;
			case 8:		secs = 0xf0;	break;
			case 9:		secs = 0x168;	break;
			case 10:	secs = 0x1e0;	break;
			case 11:	secs = 0x2d0;	break;
			case 12:	secs = 0x3c0;	break;
			case 13:	secs = -1;	break;
			default:	secs = 0;	break;
			}
			PROG_S32(obj, O_MOHLIMIT) = secs;
			PROG_S32(obj, O_MOHCOUNT) = 0;
			if (DSPLIB_DEBUG_ON()) {
				dsplibs_debug_printf(
				    "VPcmV34Main: Modem On Hold approved by "
				    "phase2 (ISP timeout is %d seconds) !!\r\n",
				    secs);
				ret = obj->f0004;
				secs = PROG_S32(obj, O_MOHLIMIT);
			}
			if (secs > 0)
				PROG_S32(obj, O_MOHLIMIT) = secs * 0x2580;
			obj->status = 7;
			goto done;
		}

		/* V.90 handshake gave up.  0xc272. */
		case 15:
		{
			int lim;

			obj->status = 8;
			lim = ((unsigned short)obj->fabe2 < 1u ? 0xbb80 : 0)
			      + 0x2580;
			obj->faa74 = 0;
			PROG_S32(obj, O_MOHCOUNT) = lim;
			obj->f0004 = 0;
			/*
			 * 0xc2a8, `xor %esi,%esi`, and it is easy to miss:
			 * this arm returns 0 and not the 13 it switched on.
			 * The debug path reaches the same 0 by re-reading
			 * `f0004` at 0xb53d, which is the only reason the two
			 * paths agree.
			 */
			ret = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "VPcmV34Main: Reconnect request indicated "
				    "from phase2 (waiting min time = %d before"
				    " reconenct request)...\r\n", lim);
			goto done;
		}

		/* 0xc252. */
		case 16:
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "VPcmV34Main: End Transmission indicated "
				    "by V.34 / Handshake...\r\n");
			obj->status = 10;
			goto done;

		default:
			goto done;
		}

	/* --- 1: V.90 -------------------------------------------------- */
	case 1:
		r = obj->v90_receiver;
		if (r <= 1) {
			ret = obj->f0004;
			goto wrongstate;
		}
		ret = obj->f0004;
		if (ret <= 1)
			goto wrongstate;
		obj->f2aa0 = (short)n;
		while (obj->txq.count < obj->f2aa0) {
			r = obj->v90_receiver;
			if (r > 14) {
				v90RateRenegSilence(obj);
			} else if (r > 10) {
				v90RateReneg(obj);
			} else if (obj->f25c2 & PROG_TXBIT_DATA) {
				modulatevector(obj);
			} else {
				v34handshak(obj);
			}
		}
		/* 0xc642, the echo canceller and the DC estimator. */
		if (n != 0) {
			int left = n;

			do {
				int s;
				int idx;

				obj->f260 = (short)*in;
				adaptecho(obj);
				s = obj->f260;
				if (PROG_U16(obj, O_DCCOUNT) != 0) {
					int acc = PROG_S32(obj, O_DCACC) + s;

					PROG_U16(obj, O_DCCOUNT)--;
					if ((PROG_U16(obj, O_DCCOUNT) & 0x7f)
					    == 0) {
						int e = (PROG_S16(obj, O_DCEST)
							 >> 1) + (acc >> 8);

						PROG_S16(obj, O_DCEST) =
						    (short)e;
						if (DSPLIB_DEBUG_ON()) {
							PROG_S32(obj, O_DCACC)
							    = acc;
							dsplibs_debug_printf(
							    "Estimated DC = %d"
							    "  (acc = %d)\n",
							    (int)(short)e, acc);
						}
						PROG_S32(obj, O_DCACC) = 0;
					} else {
						PROG_S32(obj, O_DCACC) = acc;
					}
					s = obj->f260;
				}
				s = (short)(s - PROG_S16(obj, O_DCEST));
				idx = obj->f2aa6;
				obj->hist_2f58[idx] = (short)s;
				if ((unsigned short)(idx + 1) <= PROG_HIST_LAST)
					obj->f2aa6 = (short)(idx + 1);
				else
					obj->f2aa6 = 0;
				*in++ = (float)(short)s;
				*out++ = (float)obj->f25e;
			} while (--left != 0);
		}
		in -= n;
		if (alias_v90RunDemodulator == 0) {
			v34pcm_notwritten(V34PCM_UNWRITTEN_V90RUN);
			r = 0;
		} else {
			r = sess->v90RunDemodulator(in, (unsigned int)n,
						    rxbits, nrx);
		}
		switch ((unsigned int)r) {
		case 1:
			obj->f0004 = 3;
			break;
		case 2:
			PROG_S16(obj, O_MODEMUP) = 1;
			if (obj->rates_latched == 0)
				obj->rx_bps = (int)sess->modem.demodulator
						  ->getBitRate();
			if (PROG_S16(obj, O_PCMCHOSEN) == 0) {
				int both = 0;

				if (obj->local_v92 != 0
				    && obj->remote_v92 != 0)
					both = 1;
				PROG_S16(obj, O_PCMCHOSEN) = (short)both;
			}
			PROG_S16(obj, O_HDSET) = 0;
			break;
		case 3:
			PROG_S16(obj, O_MODEMUP) = 1;
			obj->f0004 = 5;
			PROG_S16(obj, O_HDSET) = 0;
			break;
		case 4:
			obj->f0004 = 0xb;
			obj->status = 5;
			PROG_S32(obj, O_SAMPLES) = 0;
			break;
		case 5:
			VPcmV34InitiateRetrain(obj, VPCM_DP_V90);
			break;
		case 7:
			VPcmV34InitiateRetrain(obj, VPCM_DP_V34);
			break;
		case 8:
			obj->status = 10;
			obj->f0004 = 0x10;
			break;
		default:
			obj->f0004 = 2;
			break;
		}
		/* 0xc91b, requestOutputSampleClear. */
		if (PROG_U8(obj, O_CLEARREQ) != 0
		    && (unsigned int)n > 0x30u) {
			int want = n * 2;

			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "VPcmV34Main: requestOutputSampleClear "
				    "(asking %d samples clear) !!!\r\n", want);
			PROG_U8(obj, O_CLEARREQ) = 0;
			out -= n;
			sysdep_memset(out, 0, (size_t)n * sizeof(float));
			PROG_S32(obj, O_CLR_COUNT) = want;
			PROG_S32(obj, O_CLR_FLAG) = 1;
			PROG_S32(obj, O_CLR_DONE) = 0;
			PROG_U8(obj->pac3c, CFG_FLAGS51) |= CFG_FLAG51_CLEAR;
			V34EchoHistoryBackwardClean(obj,
						    (unsigned)(want + n));
		}
		goto compact;

	/* --- 2: V.92 -------------------------------------------------- */
	case 2:
		if (alias_runPcmModem == 0) {
			v34pcm_notwritten(V34PCM_UNWRITTEN_RUNPCM);
			r = 0;
		} else {
			r = sess->runPcmModem(in, out, (unsigned int)n, rxbits,
					      nrx, txbits, nbits);
		}
		switch ((unsigned int)r) {
		case 1:
			obj->f0004 = 3;
			break;
		case 2:
			if (obj->rates_latched == 0) {
				const unsigned char *tx;
				unsigned int bits;

				obj->rx_bps = (int)sess->modem.demodulator
						  ->getBitRate();
				tx = (const unsigned char *)
				     sess->v92modem.modulator;
				bits = 0;
				if (*(const int *)(tx + PCMTX_STATE)
				    == PCMTX_READY)
					bits = *(const unsigned int *)
					       (**(const unsigned char *const *
						  const *)
						  (tx + PCMTX_V92_FRAME)
						+ PCMTX_FRAME_BITS);
				obj->tx_bps = (*(const int *)(tx + PCMTX_STATE)
					       == PCMTX_READY)
					      ? (int)(unsigned)
						(bits * 8000u * (1.0f / 12.0f)
						 + 0.5f)
					      : 0;
				obj->rates_latched = 1;
			}
			PROG_S16(obj, O_PCMCHOSEN) = 1;
			obj->f0004 = 4;
			PROG_S16(obj, O_HDSET) = 0;
			break;
		case 3:
			PROG_S16(obj, O_PCMCHOSEN) = 1;
			obj->f0004 = 5;
			PROG_S16(obj, O_HDSET) = 0;
			break;
		case 4:
			obj->f0004 = 0xb;
			obj->status = 5;
			PROG_S32(obj, O_SAMPLES) = 0;
			break;
		case 5:
			VPcmV34InitiateRetrain(obj, VPCM_DP_V92);
			break;
		case 6:
			VPcmV34InitiateRetrain(obj, VPCM_DP_V90);
			break;
		case 7:
			VPcmV34InitiateRetrain(obj, VPCM_DP_V34);
			break;
		case 8:
			obj->status = 10;
			obj->f0004 = 0x10;
			break;
		default:
			obj->f0004 = 2;
			break;
		}
		/* 0xc564: the modem's own output block goes into the echo
		 * history, not the caller's input. */
		for (i = 0; i < n; i++) {
			int idx = obj->f2aa6;

			obj->hist_2f58[idx] = (short)
			    ((const float *)((unsigned char *)sess
					     + SESS_OUTBLOCK))[i];
			if ((unsigned short)(idx + 1) <= PROG_HIST_LAST)
				obj->f2aa6 = (short)(idx + 1);
			else
				obj->f2aa6 = 0;
		}
		goto reload;

	/* --- 3: K56flex ----------------------------------------------- */
	case 3:
		obj->f2aa0 = (short)n;
		while (obj->txq.count < obj->f2aa0) {
			if (obj->f25c2 & PROG_TXBIT_DATA)
				modulatevector(obj);
			else
				v34handshak(obj);
		}
		/* 0xbf38, the same loop as the V.90 arm's. */
		if (n != 0) {
			int left = n;

			do {
				int s;
				int idx;

				obj->f260 = (short)*in;
				adaptecho(obj);
				s = obj->f260;
				if (PROG_U16(obj, O_DCCOUNT) != 0) {
					int acc = PROG_S32(obj, O_DCACC) + s;

					PROG_U16(obj, O_DCCOUNT)--;
					if ((PROG_U16(obj, O_DCCOUNT) & 0x7f)
					    == 0) {
						int e = (PROG_S16(obj, O_DCEST)
							 >> 1) + (acc >> 8);

						PROG_S16(obj, O_DCEST) =
						    (short)e;
						if (DSPLIB_DEBUG_ON()) {
							PROG_S32(obj, O_DCACC)
							    = acc;
							dsplibs_debug_printf(
							    "Estimated DC = %d"
							    "  (acc = %d)\n",
							    (int)(short)e, acc);
						}
						PROG_S32(obj, O_DCACC) = 0;
					} else {
						PROG_S32(obj, O_DCACC) = acc;
					}
					s = obj->f260;
				}
				s = (short)(s - PROG_S16(obj, O_DCEST));
				idx = obj->f2aa6;
				obj->hist_2f58[idx] = (short)s;
				if ((unsigned short)(idx + 1) <= PROG_HIST_LAST)
					obj->f2aa6 = (short)(idx + 1);
				else
					obj->f2aa6 = 0;
				*in++ = (float)(short)s;
				*out++ = (float)obj->f25e;
			} while (--left != 0);
		}
		in -= n;
		r = k56->k56FlexRunDemodulator(in, (unsigned int)n, rxbits,
					       nrx);
		switch ((unsigned int)r) {
		case 1:
			obj->f0004 = 3;
			break;
		case 3:
			obj->f0004 = 5;
			/* FALLTHROUGH -- 0xc8c1 falls into 0xc8cc. */
		case 2:
			PROG_S16(obj, O_MODEMUP) = 1;
			if (obj->rates_latched == 0) {
				obj->rx_bps = *(const int *)k56;
				obj->rates_latched = 1;
			}
			PROG_U8(obj->pac3c, CFG_FLAGS3) |= CFG_FLAG3_RETRAIN;
			break;
		case 4:
			VPcmV34InitiateRetrain(obj, 0x38);
			break;
		case 5:
			VPcmV34InitiateRetrain(obj, VPCM_DP_V34);
			break;
		case 6:
			obj->status = 10;
			obj->f0004 = 0x10;
			break;
		default:
			obj->f0004 = 2;
			break;
		}
		goto compact;

	/* --- 4: line verification ------------------------------------- */
	case 4:
		if (alias_qcLineVerification == 0) {
			v34pcm_notwritten(V34PCM_UNWRITTEN_QCLINE);
			r = 0;
		} else {
			r = sess->qcLineVerification(in, out, (unsigned int)n,
						     rxbits, nrx, txbits,
						     nbits);
		}
		if (r == 0) {
			obj->f0004 = 10;
		} else {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "VPcmV34Main: Line verification period "
				    "completed !!!\r\n");
			if ((PROG_U8(obj->pac3c, CFG_FLAGS2)
			     & CFG_FLAG2_SAMELINE) == 0) {
				obj->is_short = 0;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmV34Main: Moving to full "
					    "phase2 upon DP Manager setting..."
					    "\r\n");
			} else if (obj->local_short
				   == (int)sess->verificationStatus) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmV34Main: Moving to short "
					    "phase2 due to same line "
					    "verification...\r\n");
				obj->is_short = 1;
			} else {
				obj->is_short = 0;
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmV34Main: Moving to full "
					    "phase2 due to false same line "
					    "verification...\r\n");
			}
			if (obj->is_short == 0) {
				PROG_U8(sess->v92modem.phase2Info, 0x10) = 0;
				obj->local_short = 0;
				sess->modem.ptr_49b4->init();
			}
			obj->status = 0;
			obj->f0004 = 0;
		}
		goto hist_from_in;

	/* --- 5: waiting for the user --------------------------------- */
	case 5:
		for (i = 0; i < n; i++)
			out[i] = 0.0f;
		if ((unsigned int)PROG_S32(obj, O_SAMPLES) > 0x464ffu) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "VPcmV34Main: waiting for user response "
				    "for 30sec, initiating retrain !\r\n");
			VPcmV34InitiateRetrain(obj, 0);
		}
		*nrx = 0;
		obj->f0004 = 0xc;
		*nbits = 0;
		goto hist_from_in;

	/* --- 6: the reconnect delay, which falls into 7 --------------- */
	case 6:
	{
		int held = PROG_S32(obj, O_MOHCOUNT);
		int want = obj->faa74;

		if (held < want) {
			obj->f0004 = 0xd;
		} else {
			if ((unsigned int)(held - n) < (unsigned int)want
			    && DSPLIB_DEBUG_ON()) {
				dsplibs_debug_printf(
				    "VPcmV34Main: ANSam not detected on "
				    "out-going, assuming 3-way call "
				    "supported !\r\n");
				st = obj->status;
			}
			obj->f0004 = 0xe;
		}
		goto on_hold;
	}

	/* --- 7: modem on hold ----------------------------------------- */
	case 7:
	on_hold:
		for (i = 0; i < n; i++)
			out[i] = 0.0f;
		*nrx = 0;
		*nbits = 0;
		if (st == 7)
			obj->f0004 = 0xd;
		{
			int held = PROG_S32(obj, O_MOHCOUNT);
			int lim = PROG_S32(obj, O_MOHLIMIT);

			PROG_S32(obj, O_MOHCOUNT) = held + n;
			if (lim != -1 && (held + n) >= lim) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmV34Main: Modem On Hold "
					    "Timeout expired , ending session"
					    " !!!\r\n");
				obj->f0004 = 0x10;
				obj->status = 0xa;
			}
		}
		if (alias_toneDetectorProcess == 0) {
			v34pcm_notwritten(V34PCM_UNWRITTEN_TONEPROC);
			r = 0;
		} else {
			r = sess->ansam.process(in, (unsigned int)n);
		}
		if (r != 0) {
			if (obj->status == 7) {
				int held = PROG_S32(obj, O_MOHCOUNT);

				if (PROG_U8(obj, O_ANSAMLATE) != 0
				    && held <= 0xbb7f) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "VPcmV34Main: ANSam "
						    "detected on out going "
						    "call (later case, after "
						    "%d smp) ! assuming no "
						    "3-way call...\r\n", held);
					obj->fabe2 = 2;
				} else if (DSPLIB_DEBUG_ON()) {
					dsplibs_debug_printf(
					    "VPcmV34Main: ANSam detected on "
					    "hold ! requesting Reconnect..."
					    "\r\n");
				}
			} else {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "VPcmV34Main: ANSam detected on "
					    "out going call ! assuming no "
					    "3-way call...\r\n");
				obj->fabe2 = 2;
			}
			obj->f0004 = 0xf;
			obj->status = 9;
		}
		goto hist_from_in;

	/* --- 8: waiting out the minimum reconnect delay --------------- */
	case 8:
		for (i = 0; i < n; i++)
			out[i] = 0.0f;
		*nrx = 0;
		*nbits = 0;
		t = obj->faa74 + n;
		obj->faa74 = t;
		if (t < PROG_S32(obj, O_MOHCOUNT)) {
			obj->f0004 = 0;
			ret = 0;
			goto done;
		}
		obj->status = 9;
		if (DSPLIB_DEBUG_ON()) {
			obj->f0004 = 0;
			dsplibs_debug_printf("VPcmV34Main: Delay ended, "
					     "indicating reconnect request..."
					     "\r\n");
		}
		ret = 0xf;
		obj->f0004 = 0xf;
		goto done;

	/* --- 9: reconnect requested ----------------------------------- */
	case 9:
		for (i = 0; i < n; i++)
			out[i] = 0.0f;
		*nrx = 0;
		*nbits = 0;
		ret = 0xf;
		obj->f0004 = 0xf;
		goto done;

	/* --- 10: the session is over ---------------------------------- */
	case 10:
		for (i = 0; i < n; i++)
			out[i] = 0.0f;
		*nrx = 0;
		*nbits = 0;
		ret = 0x10;
		obj->f0004 = 0x10;
		goto done;
	}

hist_from_in:
	for (i = 0; i < n; i++) {
		int idx = obj->f2aa6;

		obj->hist_2f58[idx] = (short)in[i];
		if ((unsigned short)(idx + 1) <= PROG_HIST_LAST)
			obj->f2aa6 = (short)(idx + 1);
		else
			obj->f2aa6 = 0;
	}
	goto reload;

wrongstate:
	if (DSPLIB_DEBUG_ON()) {
		dsplibs_debug_printf("VPcmV34Main: BUG BUG, Wrong state & "
				     "statuses... (v90receiver = %d, cond = "
				     "%d ; DPstatus = %d, cond = %d)\r\n",
				     r, 2, ret, 2);
		goto reload;
	}
	goto done;

compact:
	{
		int rd = obj->tx_rd;
		int wr = obj->tx_n;
		int k = 0;

		while ((unsigned int)wr > (unsigned int)rd)
			obj->tx_data[k++] = obj->tx_data[rd++];
		obj->tx_rd = 0;
		obj->tx_n = k;
	}
	ret = obj->f0004;
	if (ret <= 3) {
		*nbits = 0;
	} else {
		int avail = obj->nof_tx_bits;

		if (avail != 0) {
			avail -= obj->tx_n;
			if (avail <= 0)
				avail = 0;
			else
				avail <<= 4;
		}
		*nbits = avail;
	}
	goto done;

reload:
	ret = obj->f0004;

done:
	/*
	 * 0xb53f.  Codes 3 to 6 are the four "a modem is up" ones, and they
	 * are the only ones that run the retrain detector -- a second-order
	 * notch whose input and output energies are compared over 64-sample
	 * blocks.  Four times the output energy below the input, with the
	 * input above 150,000, counts once; six counts in a row is a retrain.
	 */
	if ((unsigned int)(ret - 3) <= 3) {
		const short *hist = obj->hist_2f58;
		int limit = obj->f2aa6;
		int k0 = PROG_S16(obj, O_NOTCH_K0);
		int k1 = PROG_S16(obj, O_NOTCH_K1);
		int k2 = PROG_S16(obj, O_NOTCH_K2);
		int retrain = 0;

		for (i = 0; i < limit; i++) {
			int t1 = (PROG_S16(obj, O_NOTCH_S1) * k2 + 0x2000)
				 >> 14;
			int x = *hist++;
			int s0 = PROG_S16(obj, O_NOTCH_S0);
			int t2;
			int s2;
			int t3;
			int y;
			int ein;
			int eout;
			int blk;

			PROG_S16(obj, O_NOTCH_S1) = (short)s0;
			t2 = (s0 * k1 + 0x2000) >> 14;
			s2 = PROG_S16(obj, O_NOTCH_S2);
			PROG_S16(obj, O_NOTCH_S2) = (short)x;
			t3 = (s2 * k0 + 0x2000) >> 14;
			y = (short)(x + (short)t2 - (short)t1 - (short)t3
				    + PROG_U16(obj, O_NOTCH_S3));
			PROG_S16(obj, O_NOTCH_S3) = (short)s2;
			PROG_S16(obj, O_NOTCH_S0) = (short)y;

			ein = PROG_S32(obj, O_NOTCH_EIN) + ((x * x + 0x20) >> 6);
			eout = PROG_S32(obj, O_NOTCH_EOUT)
			       + ((y * y + 0x20) >> 6);
			blk = PROG_S32(obj, O_NOTCH_BLK) + 1;
			if (blk == NOTCH_BLOCK) {
				int ratio = ein >> 2;

				if (ratio > eout && ein > NOTCH_EIN_MIN
				    && eout <= NOTCH_EOUT_MAX) {
					int cnt;

					PROG_S32(obj, O_NOTCH_EIN) = ein;
					cnt = PROG_S32(obj, O_NOTCH_CNT);
					PROG_S32(obj, O_NOTCH_EOUT) = eout;
					PROG_S32(obj, O_NOTCH_BLK) =
					    NOTCH_BLOCK;
					PROG_S32(obj, O_NOTCH_CNT) = ++cnt;
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "********** "
						    "retrainDetector() "
						    "notchDetectSigCnt = %d "
						    "energyInp>>NOTCH_IN_OUT_"
						    "RATIO_SHIFT = %d "
						    "energyOut = %d\r\n",
						    cnt, ratio, eout);
				} else {
					PROG_S32(obj, O_NOTCH_CNT) = 0;
				}
				PROG_S32(obj, O_NOTCH_EIN) = 0;
				PROG_S32(obj, O_NOTCH_EOUT) = 0;
				PROG_S32(obj, O_NOTCH_BLK) = 0;
			} else {
				PROG_S32(obj, O_NOTCH_BLK) = blk;
				PROG_S32(obj, O_NOTCH_EOUT) = eout;
				PROG_S32(obj, O_NOTCH_EIN) = ein;
			}
			if (PROG_S32(obj, O_NOTCH_CNT) > NOTCH_SIGCNT_MAX) {
				retrain = 1;
				break;
			}
		}

		if (retrain != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "VPcmV34Main: Retrain Detected by Tone "
				    "detector !\r\n");
			VPcmV34InitiateRetrain(obj, 0);
		}
		return obj->f0004;
	}

	return ret;
}

/*
 * ---------------------------------------------------------------------------
 * THE MANGLED HALF OF THE PUBLIC ACCESSOR SURFACE.
 *
 * Six entry points that take `tagV34Object *` rather than `void *`, which is
 * the whole of why they are mangled and therefore why they are here and not
 * in `v34pcmif.c` beside the rest of the batch.  `VPcmV34InitMOH` is the odd
 * one out: its name is NOT mangled, but it calls
 * `GenericToneDetector::reset`, and a C translation unit cannot name that.
 *
 * FOUR OF THE SIX ARE ALREADY IN THIS FILE, INLINED.
 * `VPcmV34InitiateRetrain` carries the minimum-signal-level block, both delay
 * blocks and the notch reset verbatim; the standalone functions below are the
 * out-of-line copies of the same source.  They are transcribed against the
 * disassembly rather than factored out of the inlined copies, because a
 * shared static helper would have no blob symbol and its bytes would count
 * against neither side.
 */

/*
 * Two instructions, and the second is the `ret`.
 *
 * `_Z25SetUpstreamModulationInfoP12tagV34Object` is one byte long.  Its
 * mangling is what fixes the parameter -- there is exactly one, and it is the
 * object -- and nothing else about it is observable.  It survives as an empty
 * body because the object has the symbol, at 0x6200, immediately before
 * `VPcmV34SetMinMaxBitRates`.
 */
void
SetUpstreamModulationInfo(struct tagV34Object *objp)
{
	(void)objp;
}

/*
 * Push the configured rate bounds down to whichever modem is running.
 *
 * THREE ARMS, AND THE FIRST TWO DO NOT TOUCH THE V.34 FIELDS AT ALL -- they
 * hand the configuration's two raw rates, in bits per second and undivided,
 * to a C++ object and return:
 *
 *   V.90     `v90_receiver` non-zero AND the session's `info0Layout` set:
 *            `V90ConstellationDesigner::setMinMaxRates`, reached as
 *            `p3548 -> +0x175c -> +0x208`
 *   K56flex  `k56flex_receiver` non-zero AND `pac18[8]` non-zero:
 *            `K56FlexFloModem::setMinMaxRates`, with `pac18` as `this`
 *   V.34     everything else, including a K56flex receiver whose `pac18[8]`
 *            is clear -- that arm FALLS THROUGH rather than returning
 *
 * THE V.34 ARM IS THE ONE v34fsk.h DESCRIBES.  Both rates are divided by
 * 2400 UNSIGNED -- `mul $0x1b4e81b5; shr $8`, which is `ceil(2^40/2400)` and
 * so is the unsigned magic and not the signed one -- capped at 14, and then
 * the maximum is raised to the minimum if it sits below it.  The order
 * matters: the cap on `rate_min` happens BEFORE the comparison, so a
 * configuration asking for 40000/33600 ends at 14/14 and not at 16/14.
 *
 * AND ONE RATE IS SPECIAL.  A `rate_max` of exactly 1 -- 2400 bps, the
 * slowest V.34 rate -- also sets the force-low-baud short at +0x359a, which
 * is `dec %edx; jne` and therefore that value alone rather than a threshold.
 * A `rate_max` above 14 returns without reaching it.
 */
void
VPcmV34SetMinMaxBitRates(struct tagV34Object *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	unsigned char *sess = (unsigned char *)obj->p3548;
	const unsigned char *cfg;

	if (obj->v90_receiver != 0
	    && *(const int *)(sess + SESS_GATE) != 0) {
		V90Demodulator *demod =
		    *(V90Demodulator *const *)(sess + SESS_DEMOD);

		cfg = (const unsigned char *)obj->pac3c;
		demod->constellationDesigner->setMinMaxRates(
			(unsigned)*(const int *)(cfg + CFG_MIN_RATE),
			(unsigned)*(const int *)(cfg + CFG_MAX_RATE));
		return;
	}

	if (obj->k56flex_receiver != 0
	    && *((const unsigned char *)obj->pac18 + 8) != 0) {
		cfg = (const unsigned char *)obj->pac3c;
		((K56FlexFloModem *)obj->pac18)->setMinMaxRates(
			*(const int *)(cfg + CFG_MIN_RATE),
			*(const int *)(cfg + CFG_MAX_RATE));
		return;
	}

	cfg = (const unsigned char *)obj->pac3c;
	obj->rate_min = (int)(*(const unsigned int *)(cfg + CFG_MIN_RATE)
			      / (unsigned)RATE_STEP);
	obj->rate_max = (int)(*(const unsigned int *)(cfg + CFG_MAX_RATE)
			      / (unsigned)RATE_STEP);

	if (obj->rate_min > 14)
		obj->rate_min = 14;

	if (obj->rate_max < obj->rate_min)
		obj->rate_max = obj->rate_min;

	if (obj->rate_max > 14) {
		obj->rate_max = 14;
		return;
	}

	if (obj->rate_max == 1)
		*(short *)(m + OB_FORCE_LOW_BAUD) = 1;
}

/*
 * The signal-energy floor, out of `V34DisconnectThreshTable`.
 *
 * The same eight-entry table and the same biased index
 * `VPcmV34InitiateRetrain` uses inline, and the same clearing of +0x234
 * behind it.  The index is `level + 48` compared UNSIGNED against 7, so a
 * configured level outside -48..-41 takes entry 3; the table runs 71 to 160
 * in 1 dB steps.
 */
void
VPcmV34SetMinimumSigLevel(struct tagV34Object *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	const unsigned char *cfg = (const unsigned char *)obj->pac3c;
	int level = *(const int *)(cfg + CFG_MIN_LEVEL);
	unsigned idx = (unsigned)level + 0x30u;
	int thresh;

	if (idx > 7u)
		idx = 3u;

	thresh = V34DisconnectThreshTable[idx];
	obj->rx_energy_floor = thresh;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: minLevel given is %d , "
				     "minSigLevel set to %d\n", level, thresh);

	*(int *)(m + OB_F0234) = 0;
}

/*
 * The two filter delays, and the echo canceller's.
 *
 * `filtdelay` is `((cfg[0x64] + 2) >> 2) + 0x22` and `dmadelay` is
 * `0x610 - cfg[0x68]`, both truncated to a short, and the echo canceller is
 * given `cfg[0x68] + 0x68`.  IT IS THESE TWO STRINGS THAT NAME BOTH FIELDS --
 * "V34 filtdelay set to %d (params initial delay = %d)" and "V34FEC,
 * V34dmadelay set to %d, (ext delay=%d)", each with the STORED value first
 * and the configured one second.
 *
 * THE CONFIGURATION POINTER IS RE-LOADED AFTER EACH DIAGNOSTIC -- 0x6475 and
 * 0x64b5, both `mov 0xac3c(%esi),%ecx` after a call.  Reproduced by reading
 * `pac3c` again rather than holding it, exactly as
 * `GetVPcmMinimalTxPowerReduction` does with `p3548`; nothing here can change
 * it, so the two spellings agree and this is written the object's way.
 *
 * Both reports print the stored short SIGN-EXTENDED (`cwtl`), so a delay
 * configured past 32767/4 reports negative and the field holds the same
 * negative value.
 */
void
VPcmV34SetDelays(struct tagV34Object *objp)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	unsigned char *sess = (unsigned char *)obj->p3548;

	{
		const unsigned char *cfg = (const unsigned char *)obj->pac3c;
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
		const unsigned char *cfg = (const unsigned char *)obj->pac3c;
		int ext = *(const int *)(cfg + CFG_EXT_DELAY);

		obj->dmadelay = (short)(0x610u - (unsigned)ext);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34FEC, V34dmadelay set to %d, "
					     "(ext delay=%d)\n",
					     (int)obj->dmadelay, ext);
	}

	((V92EchoCanceller *)(sess + SESS_ECHO))->setEchoDelay(
		(unsigned)*(const int *)
		    ((const unsigned char *)obj->pac3c + CFG_EXT_DELAY)
		+ 0x68u);
}

/*
 * Restart the sample clock and set a deadline.
 *
 * Thirty bytes: `sample_count` to zero and `secs * 9600` into the word beside
 * it.  This is the function that names both -- see v34fsk.h -- and 9,600 is
 * left as the constant it is: it is 8 kHz times 1.2 and not the codec rate,
 * so calling the argument "seconds" would be a claim this cannot make.
 */
void
VPcmV34SetTimeOut(struct tagV34Object *objp, int secs)
{
	struct v34_object *obj = (struct v34_object *)objp;

	obj->sample_count = 0;
	/*
	 * THE MULTIPLY IS DONE UNSIGNED, and it is the same defect
	 * `VPcmV34GetSNR` had: `secs * 0x2580` on a signed int is undefined
	 * the moment `secs` passes 2^31/9600, about 223,696, and an optimiser
	 * is entitled to assume it never does.  The object is a plain
	 * `imul $0x2580,0x8(%esp),%eax` at 0x6e90, which WRAPS, so the
	 * defined-behaviour spelling is the unsigned one and it is the same
	 * single instruction.
	 *
	 * Nothing bounds the argument: it arrives from outside the object and
	 * no caller is reconstructed, so "no real caller passes 223,696" is
	 * not something this can rely on.
	 */
	obj->timeout_deadline = (int)((unsigned)secs * 0x2580u);
}

/*
 * Start modem-on-hold.
 *
 * FOUR ARGUMENTS AND THREE OF THEM ARE STORED RATHER THAN ACTED ON: the
 * message code, and two bytes that go to +0xabe9/+0xabfa and +0xabf9.  The
 * first is `movzbl` and reaches two fields -- itself at +0xabe9 and a
 * `setne` of itself at +0xabfa -- which is what says it is a value with a
 * "is it set" reading beside it rather than a flag.
 *
 * `mode == 1` IS A BYPASS AND IT IS NOT AN ERROR PATH.  The message code is
 * stored at +0xabec whatever it is; the SECOND copy at +0xabf0 is zeroed
 * instead of stored when it is 1, under "Special bypass - sending MOHreq
 * instead of MOHFRR...".  So the two fields are the code as given and the
 * code as it will be sent.
 *
 * AND THE BYPASS TAKES THE FLAG BYTE DOWN WITH IT.  0x6e3b is `xor %ecx,%ecx`
 * -- on the register holding the fourth argument -- immediately before the
 * `jmp 6d27` back into the common tail, whose `mov %cl,0xabf9(%ebx)` is the
 * only store to that field.  Both bypass paths reach it, the quiet one from
 * 0x6e39 and the one that printed from 0x6e8a, so `message == 1` stores ZERO
 * at +0xabf9 whatever the caller passed.  Written here as the store repeated
 * in both arms, which is what GCC's tail merge turns into that `xor`; the
 * first reconstruction stored `flag` unconditionally and agreed with the
 * object on every case where `flag` was already 0, which is most of them.
 * Found by `test/unit/t_v34pcmapi.cpp` crossing `message` with `flag`.
 *
 * AFTER THAT IT IS `VPcmV34InitiateRetrain`'S TAIL, mode 4 rather than 1:
 * the same `v34handshakinit`, the same `f0004 = 7`, the same three receiver
 * scalars, the same +0x2218, the same `status = 0` and the same notch reset
 * at +0xac1c with the same 0x5a82/0x55fc/0x39c3 by role.  What is here and
 * not there is `GenericToneDetector::reset` on the session's detector at
 * +0x6f5c, and what is there and not here is the DC-estimator seed.
 *
 * The retrain-request bit in the configuration is cleared FIRST, before any
 * of it -- `andb $0xfb,0x3(%eax)` at 0x6d0e, which is `CFG_FLAG3_RETRAIN`.
 */
extern "C" void
VPcmV34InitMOH(void *objp, int message, unsigned char late,
	       unsigned char flag)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned char *m = (unsigned char *)obj;
	unsigned char *sess = (unsigned char *)obj->p3548;
	struct v34_receiver *rx = (struct v34_receiver *)(m + OB_RECEIVER);

	PROG_U8(obj->pac3c, CFG_FLAGS3) &= (unsigned char)~CFG_FLAG3_RETRAIN;

	obj->fabec = message;

	if (message == 1) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "VPcmV34Main: Special bypass - sending MOHreq "
			    "instead of MOHFRR...\r\n");
		obj->moh_message = 0;
		PROG_U8(obj, OB_MOH_FLAG) = 0;
	} else {
		obj->moh_message = message;
		PROG_U8(obj, OB_MOH_FLAG) = flag;
	}

	obj->moh_recvd = 5;
	obj->fabfa = (unsigned char)(late != 0);
	PROG_U8(obj, O_ANSAMLATE) = late;

	obj->fabe2 = 0;
	PROG_S16(obj, OB_MOH_W6) = 0;
	PROG_S16(obj, OB_MOH_W4) = 0;
	obj->fabe0 = 0;
	obj->moh_limit = 0;

	v34handshakinit(obj, 4);

	obj->f0004 = 7;

	rx->f258 = 0;
	rx->f25a = 0;
	rx->f25c = 0;

	PROG_S32(obj, OB_F2218) = 2;
	obj->status = 0;

	((GenericToneDetector *)(sess + SESS_TONE))->reset();

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
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  Same argument as v34pcmif.c's block: the fields this file
 * reaches by offset sit in regions that are otherwise padding, so a field that
 * drifted would compile silently.  Only the fields v34fsk.h already NAMES are
 * asserted -- the rest are spelled as offsets on purpose and have nothing to
 * be checked against.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34PCMMAIN_ASSERT(name, field, off) \
	typedef char v34pcmmain_off_##name[ \
		((int)__builtin_offsetof(struct v34_object, field) == (off)) \
		? 1 : -1]

V34PCMMAIN_ASSERT(status,  status,           0x0000);
V34PCMMAIN_ASSERT(f0004,   f0004,            0x0004);
V34PCMMAIN_ASSERT(rmin,    rate_min,         0x0220);
V34PCMMAIN_ASSERT(rmax,    rate_max,         0x0224);
V34PCMMAIN_ASSERT(floor,   rx_energy_floor,  0x0230);
V34PCMMAIN_ASSERT(v90rx,   v90_receiver,     0x024c);
V34PCMMAIN_ASSERT(k56rx,   k56flex_receiver, 0x0250);
V34PCMMAIN_ASSERT(dmadly,  dmadelay,             0x025c);
V34PCMMAIN_ASSERT(p3548,   p3548,            0x3548);
V34PCMMAIN_ASSERT(f359c,   f359c,            0x359c);
V34PCMMAIN_ASSERT(fa23c,   fa23c,            0xa23c);
V34PCMMAIN_ASSERT(lshort,  local_short,      0xabca);
V34PCMMAIN_ASSERT(isshort, is_short,         0xabcc);
V34PCMMAIN_ASSERT(pac18,   pac18,            0xac18);
V34PCMMAIN_ASSERT(pac3c,   pac3c,            0xac3c);

/* And the receiver trio, reached as `obj + 0x264 + 0x258`. */
#define V34PCMMAIN_RXASSERT(name, field, off) \
	typedef char v34pcmmain_rxoff_##name[ \
		((int)(__builtin_offsetof(struct v34_object, rxq) \
		       + __builtin_offsetof(struct v34_receiver, field)) \
		 == (off)) ? 1 : -1]

V34PCMMAIN_RXASSERT(f258, f258, 0x4bc);
V34PCMMAIN_RXASSERT(f25a, f25a, 0x4be);
V34PCMMAIN_RXASSERT(f25c, f25c, 0x4c0);

/*
 * The two classes this file constructs a `this` for by adding a constant, and
 * the one it reads a member pointer out of.  A size change in either would
 * move nothing here -- these are the object's offsets, not ours -- but the
 * embedded V92EchoCanceller must still fit inside what VPcmFloModem declares.
 */
typedef char v34pcmmain_echo_fits[
	(SESS_ECHO + (int)sizeof(V92EchoCanceller)
	 <= (int)sizeof(VPcmFloModem)) ? 1 : -1];

#endif /* 32-bit */
