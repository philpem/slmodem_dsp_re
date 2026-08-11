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
 * of finding 282's shift-count masking applies here and no `& 31` is
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
 * two things; finding 325 is what that cost.
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
		o->f25d0 = 0;
		o->f25d2 = 0;
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
		*(int *)&o->f25d0 = vect4[0];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		*(int *)&o->f25d0 = vect4[3];
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
		*(int *)&o->f25d0 = vect4[0];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		*(int *)&o->f25d0 = vect4[3];
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
		*(int *)&o->f25d0 = vect4[2];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		*(int *)&o->f25d0 = vect4[1];
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
		*(int *)&o->f25d0 = vect4[2];
		txmit(o);
		o->f25c0 = (short)((unsigned short)o->f25c0 + 1);
		*(int *)&o->f25d0 = vect4[1];
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
			*(int *)&o->f25d0 = vect16[d + q * 4];
		} else if (c == OB_CONSTEL_4) {
			q = (short)V34scrambler((unsigned *)&o->f25cc,
						0, 3, 2);
			o->f25c8 = (short)q;
			*(int *)&o->f25d0 = vect4[q];
		} else {
			o->f25d0 = 0;
			o->f25d2 = 0;
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
	 * `VPcmV34SetMinimumSigLevel` indexes the same table (finding 270):
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

		obj->f25c = (short)(0x610u - (unsigned)ext);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V34FEC, V34dmadelay set to %d, "
					     "(ext delay=%d)\n",
					     (int)obj->f25c, ext);
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
 * transcript comparison is the only tier that can see any of them; finding 335
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
	if (sess->v92Phase2Info->v92CapabilitiesLocal != 0
	    && obj->remote_v92 != 0
	    && ((unsigned short)bits[7] & 0x20) != 0)
		ispcm = 1;

	sess->pcmSessionType = ispcm;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"VPcmV34Main: upstream selection: local cap - %d, "
			"remote cap - %d, requested in info1 - %d, "
			"isPCM - %d\r\n",
			sess->v92Phase2Info->v92CapabilitiesLocal,
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
 * THE `+ 4` IS AN ADDRESSING ARTIFACT, exactly as v34fsk.h says of these two
 * fields and as `v90Phase34` in this file already shows: `add $0x4,%eax`
 * followed by `cmpl $0x1,0x248(%eax)` and `cmpl $0x1,0x24c(%eax)` is
 * `obj + 0x24c` and `obj + 0x250` -- `v90_receiver` and `k56flex_receiver`.
 * There is no sub-object at +4 and this is the third file to spell it out.
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
 * Finding 381 is the record and test/mutations/v34ja.json carries both.
 *
 * WHY IT IS HERE.  It is inside VPcmV34Main.cpp's run in the object -- between
 * `V34XF_IndicateTrn2dReceived` at 0xa390 and `chkForceBaudRate` at 0xa450 --
 * and it calls two C++ members, so finding 333's rule puts it in this file
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

	if (obj->v90_receiver > 1)
		sess->enterPhase3();
	else if (obj->k56flex_receiver > 1)
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
 * no chain, no gate and nothing to read -- and finding 1090 is why naming the
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
		tx = *(const unsigned char *const *)sess->pad_6124;
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
V34PCMMAIN_ASSERT(f25c,    f25c,             0x025c);
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
