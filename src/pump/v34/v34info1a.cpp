/*
 * v34info1a.cpp -- `V34SetINFO1aBits`, the outbound INFO1a/INFO1c encoder.
 *
 * WHY THIS IS A .cpp AND NOT PART OF v34info.c.  The object exports
 * `V34SetINFO1aBits` unmangled, so it was declared `extern "C"`; but one of
 * its calls is a relocation against
 *
 *     _ZN12VPcmFloModem13getUinfoValueEs
 *
 * which a C translation unit cannot name.  `src/pump/v34/v34k56.cpp` is the
 * same arrangement for the same reason and states it at length; the stem is
 * separate from `v34info` because the Makefile turns both `%.c` and `%.cpp`
 * into `$(BUILD)/%.o`, so `v34info.cpp` beside `v34info.c` would be two
 * sources racing for one object file.
 *
 * `docs/attribution.md` puts .text+0x8cb0 in the `V34.c` block, which is the
 * same coarse block every function in `v34info.c` and `v34pcmif.c` came out
 * of; the file name here is a language constraint and not a claim about which
 * source file the original lived in.
 *
 * ===========================================================================
 * THE SESSION POINTER AT +0x3548 IS A `VPcmFloModem *`
 * ===========================================================================
 *
 * `v34info.c` reaches four things through `obj->p3548` and deliberately does
 * not give it a type: none of its four functions does anything with the
 * pointer that would say what it points at.  THIS function does.  One
 * register holds it from the prologue to the end:
 *
 *     mov 0x3548(%esi),%ebp     <- obj->p3548
 *     mov 0x6120(%ebp),%eax     <- the "variant" v34info.c names SESSION_VARIANT
 *     mov 0x611c(%ebp),%esi     <- the int v34info.c names SESSION_UINFO6
 *     mov %ebp,(%esp) ; call _ZN12VPcmFloModem13getUinfoValueEs
 *
 * with no load in between, and `this` is the first STACK argument in this
 * object (finding F215).  So the same pointer is the `this` of a
 * `VPcmFloModem` member AND the base of both session offsets, and
 * `include/dsplib/VPcmFloModem.h` already calls +0x611c `pcmSessionType` and
 * +0x6120 `info0Layout` from a third translation unit.  They are the same two
 * fields, and this file uses the class rather than the offsets.
 *
 * WHAT +0x611c MEANS, RECONCILED.  Three writers and three readings that all
 * agree once "V.92 session" and "PCM upstream" are seen to be one thing:
 *
 *   VPcmFloModem::setPcmSessionType   stores (arg != 0) and prints
 *                                     "setting PCM session to V.9{0,2}"
 *   V34GiveINFO1aBits                 stores 1 exactly when the RECEIVED
 *                                     upstream baud index is 6
 *   this function                     prints "PCM Upstream is selected" for
 *                                     non-zero and "V.34 Upstream is
 *                                     selected" for zero -- and, on the
 *                                     non-zero branch, transmits the
 *                                     upstream baud index as the constant 6
 *
 * V.92 is the recommendation that adds a PCM upstream; baud index 6 is how
 * INFO1a asks for one.  Both strings are the object's own.
 *
 * ===========================================================================
 * THE MESSAGE
 * ===========================================================================
 *
 * Indices 0, 1, 2, 3, 4, 7, 8 and 9 of the caller's buffer are touched --
 * V34_INFO_MSG_SHORTS still bounds it.  The V.34/PCM upstream branches build
 * a whole message; the two K56Flex branches and the no-receiver branch only
 * OR fields into whatever is already there.
 *
 * The two multi-short fields are laid out MOST-SIGNIFICANT-BIT-FIRST ACROSS
 * ASCENDING INDICES, after a bit reversal:
 *
 *   f35a4, seven bits    rev7 bits 6..5 -> bits[0] 1..0
 *                        rev7 bits 4..0 -> bits[1] 7..3
 *   Uinfo, seven bits    rev7 bits 6..4 -> bits[1] 2..0
 *                        rev7 bits 3..0 -> bits[2] 7..4
 *   baud index, three    rev3 bits 2..1 -> bits[2] 1..0
 *                        rev3 bit  0    -> bits[3] 7
 *   a second three-bit field, always the constant 6:
 *                        rev3 bits 2..0 -> bits[3] 6..4
 *
 * THE UINFO AND BAUD PLACEMENTS ARE EXACTLY WHAT `V34GiveINFO1aBits` DECODES,
 * which is the strongest cross-check in the file and comes from two different
 * functions.  Its Uinfo is `bitreverse(((bits[1] & 7) << 4)
 * + ((bits[2] & 0xf0) >> 4), 7)`, and its upstream baud index is
 * `((bits[2] & 2) >> 1) + (bits[2] & 1) * 2 + ((bits[3] & 0x80) >> 5)` --
 * which is `bitreverse` of the three bits this writes, so the pair round-trip
 * exactly.  See the note on `recover_baud` below, where the SAME expression
 * turns up a third time inside this function.
 *
 * ===========================================================================
 * ONLY THREE OF THE FOUR TAIL COMBINATIONS ARE REACHABLE
 * ===========================================================================
 *
 * `is_short != 0` runs the short-phase-2 block, and that block sets
 * `pcmSessionType = 0` BEFORE `getUinfoValue` is called.  `getUinfoValue`
 * ends in `setPhaseIIinfo`, which ends in `setPcmSessionType(pcmSessionType)`
 * -- which normalises the field to 0 or 1 and cannot make a zero non-zero.
 * So the PCM-upstream branch is reachable only with `is_short == 0`, and the
 * combination "short phase 2 and PCM upstream" does not exist.  The test
 * drives all four and the fourth simply lands in the V.34 branch.
 *
 * ===========================================================================
 * NOT REPRODUCED, AND DELIBERATELY
 * ===========================================================================
 *
 * The PCM-upstream branch stores `bits[0]` SEVEN times -- once with 0 and
 * then after each of the six flag tests -- and the V.34 branch does not.
 * Only the last store is reproduced.  The intermediate values are
 * unobservable: the three fields being tested are at +0xabce..+0xabd3 of the
 * V.34 object and `bits` is `obj + 0xa9ac` at all three call sites in
 * `v34handshak`, so nothing the function reads can alias what it writes, and
 * a differential test cannot distinguish the two.  Recorded here rather than
 * left to be rediscovered, the way VPcmFloModem.cpp records its two.
 *
 * THE RETURN VALUE IS 0 ON EVERY PATH AND NO CALLER LOOKS AT IT.  All three
 * call sites in `v34handshak` step straight on to a load; nothing tests
 * `%eax`.  It is declared `int` because both epilogues clear `%eax`
 * explicitly, which GCC does not do for a `void` function -- that inference
 * is the whole of the evidence, and the test's check of the return can
 * therefore never fail.  It is there so that a reconstruction returning
 * something else would still be caught, and it is named as vacuous where it
 * is written.
 */

#include "dsplib/debug.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34info.h"
#include "dsplib/v34rx.h"		/* bitreverse */
#include "dsplib/VPcmFloModem.h"

/*
 * The PCM law, in the block `obj->pac18` points at.  `V34GiveINFO1aBits`
 * prints this same int as "PCM type: local %d ... (A=1, Mu=0)", which is
 * where the reading comes from; it is read as 32 bits here on both branches.
 */
#define PCM_LAW			0x0c

/*
 * The three-bit field this transmits after the upstream baud index.  It is
 * the literal 6 in the object on every path -- twice in the PCM-upstream
 * branch, where the baud index is 6 as well, and once in the V.34 branch,
 * where it is not.  Nothing here establishes what it selects.
 */
#define INFO1A_FIELD_B		6

/* The upstream baud index that asks for a PCM upstream. */
#define INFO1A_BAUD_PCM		6

/*
 * The upstream baud index a short phase 2 transmits.  A CONSTANT, not a
 * recovered value: the object loads 4 into the register before testing
 * `is_short` and only replaces it on the zero side.
 */
#define INFO1A_BAUD_SHORT	4

/*
 * The rate configuration a short phase 2 hard-codes.  3200 baud and an 1829
 * Hz carrier is a legal V.34 pairing, and the two offsets are named by the
 * object's own getters: `VPcmV34GetCurrentTxBaudRate` reads +0xaa84 and
 * `VPcmV34GetCurrentTxCarrier` reads +0xaa94.  `preemp` is named the same
 * way now -- `V34XF_IndicateK56FlexJdReceived` prints all three of them
 * together as "baudrate = %d, carrier = %d, preemp = %d" -- and this is the
 * function that clears it, so a short phase 2 asks for no pre-emphasis.
 */
#define SHORT_PHASE2_BAUD	0xc80
#define SHORT_PHASE2_CARRIER	0x725

/*
 * The seven-bit field this shares with `V34GiveINFO1aBits`'s Uinfo, put
 * where that function reads it from.  Split so the two branches that build
 * it cannot drift apart; they really are one expression in the object as
 * well, reached by two different paths through the same three instructions.
 */
static void
put_rev7_high(short *bits, int r)
{
	bits[0] = (short)((((short)r >> 5) & 3) | (unsigned short)bits[0]);
	bits[1] = (short)((((short)r << 3) & 0xf8) | (unsigned short)bits[1]);
}

/*
 * The upstream baud index, read back out of the message buffer the caller
 * handed in.
 *
 * THIS IS `V34GiveINFO1aBits`'S DECODER, SPELLED DIFFERENTLY.  The object
 * gathers `((bits[2] & 3) * 2) | ((bits[3] & 0x80) >> 7)` and reverses three
 * bits of it; expand the reversal and it is
 *
 *     ((bits[2] & 2) >> 1) + (bits[2] & 1) * 2 + ((bits[3] & 0x80) >> 5)
 *
 * character for character what the receiving side computes.  Two independent
 * functions, one field.  The shift on `bits[3]` is ARITHMETIC in the object
 * (`and $0x80` then `sar $7` on a zero-extended short), which cannot matter
 * for a value masked to one bit and is kept so it stays true if the width
 * moves -- the same reason v34info.c keeps its.
 */
static int
recover_baud(const short *bits)
{
	int v;

	v = (int)(((unsigned short)bits[2] & 3) * 2)
	    | (int)(((unsigned short)bits[3] & 0x80) >> 7);

	return (short)bitreverse((unsigned short)v, 3);
}

extern "C" int
V34SetINFO1aBits(void *objp, short *bits)
{
	struct v34_object *obj = (struct v34_object *)objp;
	VPcmFloModem *sess = (VPcmFloModem *)obj->p3548;
	const unsigned char *pcm = (const unsigned char *)obj->pac18;
	int r;
	int uinfo;

	/*
	 * NEITHER PCM RECEIVER RUNNING.  The only thing that happens is the
	 * seven-bit field; the object reaches it by falling out of the first
	 * test and then re-reading `k56flex_receiver`, which nothing between
	 * has touched.
	 */
	if (obj->v90_receiver == 0 && obj->k56flex_receiver == 0)
		put_rev7_high(bits, bitreverse((unsigned short)obj->f35a4, 7));

	/*
	 * K56FLEX, AND THE ROLE FLAG SPLITS IT.  `f359c == 0x65` is the
	 * originating end everywhere else in the tree, and the object's two
	 * strings agree: INFO1c for "Caller-Analog", INFO1a for
	 * "Answer-Digital".
	 *
	 * The caller's half repeats the seven-bit field the branch above
	 * writes, byte for byte.  Written through the same helper because it
	 * is the same three instructions; what is NOT shared is the law test,
	 * which is `== 1` here and `!= 0` on the answer side.  That asymmetry
	 * is in the object -- `cmpl $0x1` against `test` -- and finding F130 is
	 * the standing warning about factoring two blocks that look alike.
	 */
	if (obj->k56flex_receiver != 0) {
		/*
		 * READ HERE AND NOT AT ENTRY.  The object loads the POINTER
		 * in the prologue and spills it, but dereferences +0xc at
		 * exactly two sites, one in each of the two arms below.  On
		 * every path with no K56Flex receiver it never touches that
		 * memory, and a reconstruction that read it unconditionally
		 * would fault on a caller that had not set the pointer up.
		 * The differential test cannot see the difference, because it
		 * always installs a valid block; `V34GiveINFO1aBits` in
		 * v34info.c reads the same field from inside its own K56Flex
		 * branch for the same reason.
		 */
		int law = *(const int *)(pcm + PCM_LAW);

		if (obj->f359c == 0x65) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V34SetINFO1aBits: Setting INFO1c for "
					"K56Flex (Caller-Analog)...\r\n");

			put_rev7_high(bits,
				      bitreverse((unsigned short)obj->f35a4,
						 7));

			bits[8] = (short)(((unsigned short)bits[8] & ~0xeu)
					  | (law == 1 ? 0x13u : 0x11u));
			bits[9] = (short)((unsigned short)bits[9] & 0x3ff);
		} else {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"V34SetINFO1aBits: Setting INFO1a for "
					"K56Flex (Answer-Digital)...\r\n");

			bits[3] = (short)(((unsigned short)bits[3] & ~6u)
					  | (law != 0 ? 9u : 0xbu));
		}
	}

	/* Everything below needs a V.90 receiver. */
	if (obj->v90_receiver == 0)
		return 0;

	/*
	 * INFO1d.  One bit, and the two arms are NOT each other's inverse:
	 * setting it is a 16-bit OR and clearing it is `and $0xdf` on the
	 * zero-extended short, which also clears the whole high byte.  A
	 * `bits[7] &= ~0x20` would be a different function.
	 */
	if (sess->info0Layout == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmV34Main, setINFO1dBits\n");

		if (sess->pcmSessionType != 0)
			bits[7] = (short)((unsigned short)bits[7] | 0x20);
		else
			bits[7] = (short)(bits[7] & 0xdf);

		obj->v90_receiver = 2;
		return 0;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main, setINFO1aBits\n");

	/*
	 * SHORT PHASE 2 SKIPS THE RATE NEGOTIATION, so the rate configuration
	 * is written here instead of arriving from `setfinalrate`, and the
	 * upstream goes back to V.34.  `f25dc` is the transmit power
	 * reduction v34hshak.c computes; this clears it.
	 */
	if (obj->is_short != 0) {
		struct v34_ratecfg *cfg =
		    (struct v34_ratecfg *)((unsigned char *)obj + V34_RATECFG);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"VPcmV34Main: Short phase2 (Hard Coded) !!!\r\n");

		sess->pcmSessionType = 0;
		cfg->baud = SHORT_PHASE2_BAUD;
		cfg->carrier = SHORT_PHASE2_CARRIER;
		cfg->preemp = 0;
		obj->f25dc = 0;
	}

	/*
	 * NO UINFO, NO V.90.  `getUinfoValue` returns the modem's Uinfo or
	 * zero; zero takes the receiver back to 0 -- not to 2 -- and the
	 * message is left exactly as the branches above made it.
	 */
	uinfo = (short)sess->getUinfoValue(obj->is_short);
	if (uinfo == 0) {
		obj->v90_receiver = 0;
		return 0;
	}

	if (sess->pcmSessionType != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmV34Main: PCM Upstream is "
					     "selected (info1)...\r\n");

		/*
		 * SIX FLAGS, THREE FIELDS, READ AS BYTES.  The object tests
		 * `fabce`, `fabd0` and `fabd2` with `testb`, so only the low
		 * byte of each short can reach the message; the pairs go out
		 * high bit first, which is the same reversal
		 * `V34GiveINFO1aBits` undoes when it fills them in.
		 */
		r = 0;
		if (obj->fabce & 1)
			r = 0x80;
		if (obj->fabce & 2)
			r |= 0x40;
		if (obj->fabd0 & 1)
			r |= 0x20;
		if (obj->fabd0 & 2)
			r |= 0x10;
		if (obj->fabd2 & 1)
			r |= 8;
		if (obj->fabd2 & 2)
			r |= 4;
		bits[0] = (short)r;

		bits[1] = 0;
		r = bitreverse((unsigned short)uinfo, 7);
		bits[1] = (short)((((short)r >> 4) & 7)
				  | (unsigned short)bits[1]);
		bits[2] = (short)(((short)r << 4) & 0xf0);

		r = bitreverse(INFO1A_BAUD_PCM, 3);
		bits[2] = (short)((((short)r >> 1) & 3)
				  | (unsigned short)bits[2]);
		bits[3] = (short)(((short)r << 7) & 0x80);

		r = bitreverse(INFO1A_FIELD_B, 3);
		bits[4] = (short)0xfc;
		bits[3] = (short)((((short)r << 4) & 0x70)
				  | (unsigned short)bits[3] | 0xf);

		obj->v90_receiver = 2;
		return 0;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: V.34 Upstream is selected "
				     "(info1)...\r\n");

	/*
	 * The baud index is settled BEFORE anything is written, which is what
	 * makes reading it back out of the buffer meaningful: the two shorts
	 * it comes out of are overwritten three instructions later.
	 */
	{
		int baud = obj->is_short != 0 ? INFO1A_BAUD_SHORT
					      : recover_baud(bits);

		r = bitreverse((unsigned short)obj->f35a4, 7);
		bits[1] = (short)(((short)r << 3) & 0xf8);
		bits[0] = (short)(((short)r >> 5) & 3);

		r = bitreverse((unsigned short)uinfo, 7);
		bits[1] = (short)((((short)r >> 4) & 7)
				  | (unsigned short)bits[1]);
		bits[2] = (short)(((short)r << 4) & 0xf0);

		r = bitreverse((unsigned short)baud, 3);
		bits[3] = (short)(((short)r << 7) & 0x80);
		bits[2] = (short)((((short)r >> 1) & 3)
				  | (unsigned short)bits[2]);

		r = bitreverse(INFO1A_FIELD_B, 3);
		bits[4] = 0;
		bits[3] = (short)((((short)r << 4) & 0x70)
				  | (unsigned short)bits[3]);
	}

	/*
	 * `is_short` is re-read from the object here, after `getUinfoValue`
	 * has run.  Nothing in that call can reach it -- it is +0xabcc of the
	 * V.34 object and the modem only writes its own -- so the reload is
	 * kept as the object has it rather than reusing the value above.
	 */
	if (obj->is_short != 0)
		bits[4] = 1;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: It is ===== V90 receiver "
				     "==== from Now on\n");

	obj->v90_receiver = 2;
	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  The two fields this batch carved out of a pad, and the
 * two session offsets it depends on being where VPcmFloModem.h puts them.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34INFO1A_ASSERT(name, type, field, off) \
	typedef char v34info1a_off_##name[ \
	    ((int)__builtin_offsetof(type, field) == (off)) ? 1 : -1]

V34INFO1A_ASSERT(f35a4,   struct v34_object,  f35a4,		0x35a4);
V34INFO1A_ASSERT(f25dc,   struct v34_object,  f25dc,		0x25dc);
V34INFO1A_ASSERT(baud,    struct v34_ratecfg, baud,		0x00);
V34INFO1A_ASSERT(preemp,  struct v34_ratecfg, preemp,		0x06);
V34INFO1A_ASSERT(carrier, struct v34_ratecfg, carrier,		0x10);
V34INFO1A_ASSERT(sesstype, VPcmFloModem,      pcmSessionType,	0x611c);
V34INFO1A_ASSERT(layout,  VPcmFloModem,       info0Layout,	0x6120);

#endif
